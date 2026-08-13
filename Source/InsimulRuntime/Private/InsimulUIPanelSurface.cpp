// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulUIPanelSurface — thin UObject wrapper over FInsimulUIPanelResolver. Every
// decision (catalog parse, module gate, override precedence, the diagnostics) lives
// in (and is host-tested by) the portable core; this file only marshals
// FString<->std::string, loads the shipped catalog off disk, turns a widget path or
// a registry binding into a UClass, and is structurally syntax-gated (check.mjs).

#include "InsimulUIPanelSurface.h"

#include "InsimulSettings.h"
#include "InsimulUIRegistry.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "../Portable/InsimulUIPanelCatalog.h"

DEFINE_LOG_CATEGORY_STATIC(LogInsimulUI, Log, All);

namespace
{
	FString ToFString(const std::string& S)
	{
		return FString(UTF8_TO_TCHAR(S.c_str()));
	}

	std::string ToStd(const FString& S)
	{
		return std::string(TCHAR_TO_UTF8(*S));
	}

	EInsimulPanelAvailability ToAvailability(insimul::EInsimulPanelOutcome Outcome)
	{
		switch (Outcome)
		{
		case insimul::EInsimulPanelOutcome::Shipped:
			return EInsimulPanelAvailability::Shipped;
		case insimul::EInsimulPanelOutcome::Overridden:
			return EInsimulPanelAvailability::Overridden;
		case insimul::EInsimulPanelOutcome::Gated:
			return EInsimulPanelAvailability::Gated;
		default:
			return EInsimulPanelAvailability::Unknown;
		}
	}
}

FString UInsimulUIPanelSurface::CatalogPath()
{
	return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data"), TEXT("insimul"), TEXT("ui"),
		TEXT("panels.json"));
}

void UInsimulUIPanelSurface::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadCatalog();
}

void UInsimulUIPanelSurface::Deinitialize()
{
	Resolver.Reset();
	Registry = nullptr;
	bRegistryResolved = false;
	Super::Deinitialize();
}

void UInsimulUIPanelSurface::LoadCatalog()
{
	insimul::FInsimulUIPanelCatalog Catalog;

	FString Text;
	const FString Path = CatalogPath();
	if (FFileHelper::LoadFileToString(Text, *Path))
	{
		std::string Error;
		if (!insimul::FInsimulUIPanelCatalog::Parse(ToStd(Text), Catalog, Error))
		{
			// Not a silent fallback: a corrupt catalog means every module-owned panel
			// would quietly become ungated, which is exactly the bug this gate exists
			// to prevent.
			UE_LOG(LogInsimulUI, Error,
				TEXT("[UI] The panel catalog at %s is unreadable (%s) — falling back to the "
					 "built-in panel map, so NO panel can be module-gated."),
				*Path, *ToFString(Error));
			Catalog = insimul::FInsimulUIPanelCatalog::FallbackCatalog();
		}
	}
	else
	{
		// A plugin dropped into a project that ships no Insimul data dir. Legal, and
		// stated out loud: the panels resolve, the gate cannot.
		UE_LOG(LogInsimulUI, Log,
			TEXT("[UI] No panel catalog at %s — using the built-in panel map (panels resolve, "
				 "module gating is off)."),
			*Path);
		Catalog = insimul::FInsimulUIPanelCatalog::FallbackCatalog();
	}

	Resolver = MakeUnique<insimul::FInsimulUIPanelResolver>(MoveTemp(Catalog));
}

insimul::FInsimulUIPanelResolver& UInsimulUIPanelSurface::EnsureResolver() const
{
	if (!Resolver)
	{
		Resolver = MakeUnique<insimul::FInsimulUIPanelResolver>(
			insimul::FInsimulUIPanelCatalog::FallbackCatalog());
	}
	return *Resolver;
}

void UInsimulUIPanelSurface::ApplyActiveModules(const TArray<FString>& ModuleIds, const FString& GenreId)
{
	std::vector<std::string> Ids;
	Ids.reserve(static_cast<std::size_t>(ModuleIds.Num()));
	for (const FString& Id : ModuleIds)
	{
		Ids.push_back(ToStd(Id));
	}
	EnsureResolver().SetActiveModuleIds(MoveTemp(Ids));

	UE_LOG(LogInsimulUI, Log, TEXT("[UI] genre '%s': %s"), *GenreId,
		*ToFString(EnsureResolver().Describe()));
}

void UInsimulUIPanelSurface::ApplyModuleSet(const insimul::FInsimulActiveModuleSet& Set)
{
	EnsureResolver().SetActiveModules(Set);
	UE_LOG(LogInsimulUI, Log, TEXT("[UI] %s"), *ToFString(EnsureResolver().Describe()));
}

void UInsimulUIPanelSurface::ClearActiveModules()
{
	EnsureResolver().SetUngated();
}

bool UInsimulUIPanelSurface::IsGated() const
{
	return EnsureResolver().IsGated();
}

bool UInsimulUIPanelSurface::IsPanelAvailable(FName PanelKey) const
{
	return EnsureResolver().Peek(ToStd(PanelKey.ToString())).IsAvailable();
}

EInsimulPanelAvailability UInsimulUIPanelSurface::PanelAvailability(FName PanelKey) const
{
	return ToAvailability(EnsureResolver().Peek(ToStd(PanelKey.ToString())).Outcome);
}

UInsimulUIRegistry* UInsimulUIPanelSurface::GetRegistry() const
{
	if (!bRegistryResolved)
	{
		bRegistryResolved = true;
		if (const UInsimulSettings* Settings = GetDefault<UInsimulSettings>())
		{
			Registry = Cast<UInsimulUIRegistry>(Settings->UIRegistry.TryLoad());
		}
	}
	return Registry;
}

TSubclassOf<UUserWidget> UInsimulUIPanelSurface::ResolvePanelClass(FName PanelKey)
{
	const insimul::FInsimulPanelResolution Resolution =
		EnsureResolver().Resolve(ToStd(PanelKey.ToString()));

	if (!Resolution.IsAvailable())
	{
		// Gated and Unknown are DIFFERENT failures and read differently in the log:
		// one is this world's module set doing its job, the other is a missing panel.
		UE_LOG(LogInsimulUI, Warning, TEXT("[UI] %s"), *ToFString(Resolution.Detail));
		return nullptr;
	}

	// The registry asset is the creator's override surface, so it wins when it binds
	// the key; the catalog's widget path is the shipped default underneath it.
	if (UInsimulUIRegistry* Reg = GetRegistry())
	{
		if (Reg->HasPanel(PanelKey))
		{
			if (TSubclassOf<UUserWidget> Bound = Reg->ResolvePanelClass(PanelKey))
			{
				return Bound;
			}
		}
	}

	const FString WidgetPath = ToFString(Resolution.Widget);
	if (UClass* Loaded = LoadClass<UUserWidget>(nullptr, *WidgetPath))
	{
		return Loaded;
	}

	UE_LOG(LogInsimulUI, Warning,
		TEXT("[UI] Panel '%s' resolves to '%s', which is not a loadable widget class."),
		*PanelKey.ToString(), *WidgetPath);
	return nullptr;
}

UUserWidget* UInsimulUIPanelSurface::CreatePanelWidget(FName PanelKey)
{
	TSubclassOf<UUserWidget> Class = ResolvePanelClass(PanelKey);
	if (!Class)
	{
		return nullptr;
	}
	UGameInstance* Instance = GetGameInstance();
	if (!Instance)
	{
		return nullptr;
	}
	return CreateWidget<UUserWidget>(Instance, Class);
}

TArray<FName> UInsimulUIPanelSurface::AvailablePanels() const
{
	TArray<FName> Out;
	for (const std::string& Key : EnsureResolver().AvailableKeys())
	{
		Out.Add(FName(*ToFString(Key)));
	}
	return Out;
}

TArray<FName> UInsimulUIPanelSurface::WithheldPanels() const
{
	TArray<FName> Out;
	for (const std::string& Key : EnsureResolver().GatedKeys())
	{
		Out.Add(FName(*ToFString(Key)));
	}
	return Out;
}

FString UInsimulUIPanelSurface::DescribeSurface() const
{
	return ToFString(EnsureResolver().Describe());
}
