// Copyright 2024 Insimul. All Rights Reserved.
//
// See InsimulModuleActivator.h. Nothing here names a mechanic: the table does, and
// tools/verify-mechanics/check-activation.mjs fails if one ever appears in this file.

#include "InsimulModuleActivator.h"

#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "InsimulPrologSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogInsimulActivation, Log, All);

namespace
{
    FString ToFString(const std::string& Text)
    {
        return FString(UTF8_TO_TCHAR(Text.c_str()));
    }

    std::string ToStdString(const FString& Text)
    {
        return std::string(TCHAR_TO_UTF8(*Text));
    }

    TArray<FString> ToStringArray(const std::vector<std::string>& Names)
    {
        TArray<FString> Out;
        Out.Reserve(static_cast<int32>(Names.size()));
        for (const std::string& Name : Names)
        {
            Out.Add(ToFString(Name));
        }
        return Out;
    }

    /** Read a UTF-8 file into a std::string, or false. */
    bool LoadUtf8(const FString& Path, std::string& OutText)
    {
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *Path))
        {
            return false;
        }
        OutText = ToStdString(Text);
        return true;
    }
}

FString UInsimulModuleActivator::DataRoot()
{
    return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data"), TEXT("insimul"));
}

void UInsimulModuleActivator::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (!LoadTables())
    {
        return;
    }
    // The IR is the authority; a game that states a genre itself only wins when the
    // document declares none, so a mis-set inspector field cannot override an export.
    ActivateFromWorldIr();
}

void UInsimulModuleActivator::Deinitialize()
{
    ActiveSet = insimul::FInsimulActiveModuleSet();
    ConsultReport = insimul::FInsimulPackConsultReport();
    bResolved = false;

    Super::Deinitialize();
}

bool UInsimulModuleActivator::LoadTables()
{
    if (bTablesLoaded)
    {
        return true;
    }

    const FString Root = DataRoot();
    std::string TableJson;
    std::string ManifestJson;
    if (!LoadUtf8(FPaths::Combine(Root, TEXT("modules"), TEXT("genre-activation.json")), TableJson))
    {
        UE_LOG(LogInsimulActivation, Error,
            TEXT("No activation table at %s/modules/genre-activation.json — NO module is activated and no rule pack ")
            TEXT("is consulted. Re-export this game, or vendor the table (tools/vendor-conformance.mjs)."),
            *Root);
        return false;
    }
    if (!LoadUtf8(FPaths::Combine(Root, TEXT("packs"), TEXT("PACKS.json")), ManifestJson))
    {
        UE_LOG(LogInsimulActivation, Error,
            TEXT("No pack manifest at %s/packs/PACKS.json — the active modules' vocabulary cannot be consulted. ")
            TEXT("Re-vendor with tools/vendor-packs/vendor-packs.mjs --core <core> --write."),
            *Root);
        return false;
    }

    std::string Error;
    if (!insimul::FInsimulActivationTable::Parse(TableJson, Table, Error))
    {
        UE_LOG(LogInsimulActivation, Error, TEXT("The activation table is unreadable: %s"), *ToFString(Error));
        return false;
    }
    if (!insimul::FInsimulPredicatePackManifest::Parse(ManifestJson, Manifest, Error))
    {
        UE_LOG(LogInsimulActivation, Error, TEXT("The pack manifest is unreadable: %s"), *ToFString(Error));
        return false;
    }

    bTablesLoaded = true;
    UE_LOG(LogInsimulActivation, Log, TEXT("Activation data loaded: %d genre(s), %d rule pack(s) (core %s)"),
        static_cast<int32>(Table.Genres().size()), static_cast<int32>(Manifest.ConsultOrder().size()),
        *ToFString(Manifest.CoreCommit()));
    return true;
}

bool UInsimulModuleActivator::ActivateFromWorldIr()
{
    if (!LoadTables())
    {
        return false;
    }

    std::string IrJson;
    const FString IrPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data"), TEXT("WorldIR.json"));
    const bool bHaveIr = LoadUtf8(IrPath, IrJson);
    const std::string IrGenre = bHaveIr ? insimul::FInsimulActivationTable::GenreOfWorldIr(IrJson) : std::string();

    if (!IrGenre.empty())
    {
        ActiveSet = Table.Resolve(IrGenre, Manifest.ConsultOrder(), insimul::EInsimulGenreSource::WorldIr);
    }
    else if (!DeclaredGenre.IsEmpty())
    {
        // §14.2: a resumed save's worldSnapshot carries no genre, so the game states
        // one. Reported as Declared, never as though the IR had said so.
        ActiveSet = Table.Resolve(
            ToStdString(DeclaredGenre), Manifest.ConsultOrder(), insimul::EInsimulGenreSource::Declared);
    }
    else
    {
        // Nothing declared one: every pack. Right for an editor session, wrong for a
        // shipped game — so it is a WARNING rather than a silent default.
        ActiveSet = Table.Resolve(std::string(), Manifest.ConsultOrder(), insimul::EInsimulGenreSource::Undeclared);
        UE_LOG(LogInsimulActivation, Warning,
            TEXT("No genre declared (%s%s) — EVERY rule pack is consulted and every module is treated as active. ")
            TEXT("That is core's editor default; a shipped game should carry meta.genreConfig.id in its World IR, ")
            TEXT("or set DeclaredGenre."),
            bHaveIr ? TEXT("the World IR carries no meta.genreConfig.id") : TEXT("no World IR at "),
            bHaveIr ? TEXT("") : *IrPath);
    }

    if (!ActiveSet.bKnown && ActiveSet.Source != insimul::EInsimulGenreSource::Undeclared)
    {
        UE_LOG(LogInsimulActivation, Warning,
            TEXT("Genre '%s' is not one core knows — the shared vocabulary is consulted and NO mechanic module is ")
            TEXT("activated. Check the genre id against %s/modules/genre-activation.json."),
            *ToFString(ActiveSet.Genre), *DataRoot());
    }
    for (const std::string& Warning : ActiveSet.Warnings)
    {
        UE_LOG(LogInsimulActivation, Warning, TEXT("%s"), *ToFString(Warning));
    }

    bResolved = true;
    return ConsultActive();
}

bool UInsimulModuleActivator::ActivateForGenre(const FString& GenreId)
{
    if (!LoadTables())
    {
        return false;
    }
    ActiveSet = Table.Resolve(
        ToStdString(GenreId), Manifest.ConsultOrder(),
        GenreId.IsEmpty() ? insimul::EInsimulGenreSource::Undeclared : insimul::EInsimulGenreSource::Declared);
    bResolved = true;
    return ConsultActive();
}

bool UInsimulModuleActivator::ConsultActive()
{
    UInsimulPrologSubsystem* Prolog = ResolveProlog();
    if (Prolog == nullptr || !Prolog->IsPrologReady())
    {
        UE_LOG(LogInsimulActivation, Error,
            TEXT("The Prolog KB is not ready — %s resolved, and NOTHING was consulted."),
            *DescribeActivation());
        return false;
    }

    insimul::FInsimulDirectoryPackSource Source(
        ToStdString(FPaths::Combine(DataRoot(), TEXT("packs"))), &Manifest);

    ConsultReport = insimul::ConsultActivePacks(
        &ActiveSet, Manifest, &Source,
        [Prolog](const std::string& Text, std::string& OutError)
        {
            if (Prolog->ConsultWorldData(ToFString(Text)))
            {
                return true;
            }
            OutError = ToStdString(Prolog->GetLastError());
            return false;
        });

    UE_LOG(LogInsimulActivation, Log, TEXT("%s"), *DescribeActivation());
    UE_LOG(LogInsimulActivation, Log, TEXT("%s"), *ToFString(ConsultReport.Describe()));

    if (!ConsultReport.IsOk())
    {
        // An ACTIVE pack that is missing or refused means a mechanic the world
        // selected has no vocabulary. That is never a warning.
        UE_LOG(LogInsimulActivation, Error,
            TEXT("%d active rule pack(s) did not reach the KB — the modules that own them are selected and cannot ")
            TEXT("answer. See the report above."),
            static_cast<int32>(ConsultReport.Missing().size() + ConsultReport.Failed().size()));
        return false;
    }
    return true;
}

UInsimulPrologSubsystem* UInsimulModuleActivator::ResolveProlog() const
{
    UGameInstance* GameInstance = GetGameInstance();
    return GameInstance != nullptr ? GameInstance->GetSubsystem<UInsimulPrologSubsystem>() : nullptr;
}

FString UInsimulModuleActivator::GetActiveGenre() const
{
    return ToFString(ActiveSet.Genre);
}

EInsimulGenreOrigin UInsimulModuleActivator::GetGenreOrigin() const
{
    switch (ActiveSet.Source)
    {
    case insimul::EInsimulGenreSource::WorldIr:  return EInsimulGenreOrigin::WorldIr;
    case insimul::EInsimulGenreSource::Declared: return EInsimulGenreOrigin::Declared;
    case insimul::EInsimulGenreSource::Undeclared:
    default:                                     return EInsimulGenreOrigin::Undeclared;
    }
}

TArray<FString> UInsimulModuleActivator::GetActiveModules() const
{
    TArray<FString> Out;
    Out.Reserve(static_cast<int32>(ActiveSet.Modules.size()));
    for (const insimul::FInsimulActiveModule& Module : ActiveSet.Modules)
    {
        Out.Add(ToFString(Module.Id));
    }
    return Out;
}

TArray<FString> UInsimulModuleActivator::GetConsultedPacks() const
{
    return ToStringArray(ConsultReport.Consulted());
}

TArray<FString> UInsimulModuleActivator::GetSkippedPacks() const
{
    return ToStringArray(ConsultReport.Skipped());
}

bool UInsimulModuleActivator::IsModuleActive(const FString& ModuleId) const
{
    return ActiveSet.IsModuleActive(ToStdString(ModuleId));
}

FString UInsimulModuleActivator::DescribeActivation() const
{
    if (!bResolved)
    {
        return TEXT("no module set has been resolved — nothing is active and nothing was consulted");
    }
    return ToFString(ActiveSet.Describe());
}
