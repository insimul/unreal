// Copyright 2024 Insimul. All Rights Reserved.
//
// Implementation of UInsimulRadiantSourceShell — the thin UE marshalling layer over
// insimul::FRadiantSource. Nothing here builds a request, encodes JSON or
// interprets a result; it converts FString <-> std::string and enforces
// game-thread affinity, delegating everything else to the portable adapter
// (Portable/InsimulRadiantSource.h), which is the single translation site.

#include "InsimulRadiantSourceShell.h"

#include "InsimulCoreBridge.h"
#include "../../Portable/InsimulRadiantSource.h"

DEFINE_LOG_CATEGORY_STATIC(LogInsimulRadiant, Log, All);

namespace
{
	// std::string <-> FString marshalling. Core emits/consumes UTF-8.
	FString ToFString(const std::string& S)
	{
		return FString(UTF8_TO_TCHAR(S.c_str()));
	}

	std::string ToStdString(const FString& S)
	{
		return std::string(TCHAR_TO_UTF8(*S));
	}

	std::vector<std::string> ToStdLines(const TArray<FString>& Lines)
	{
		std::vector<std::string> Out;
		Out.reserve(static_cast<std::size_t>(Lines.Num()));
		for (const FString& Line : Lines)
		{
			Out.push_back(ToStdString(Line));
		}
		return Out;
	}

	TArray<FString> ToFStrings(const std::vector<std::string>& Values)
	{
		TArray<FString> Out;
		Out.Reserve(static_cast<int32>(Values.size()));
		for (const std::string& Value : Values)
		{
			Out.Add(ToFString(Value));
		}
		return Out;
	}

	FInsimulGeneratedQuest MakeQuest(const insimul::FGeneratedRadiantQuest& Quest)
	{
		FInsimulGeneratedQuest Out;
		Out.QuestId = ToFString(Quest.QuestId);
		Out.TemplateId = ToFString(Quest.TemplateId);
		Out.QuestContent = ToFString(Quest.QuestContent);
		Out.ContentClauses = ToFStrings(Quest.ContentClauses);
		Out.FactsToAssert = ToFStrings(Quest.FactsToAssert);
		Out.FactsToRetract = ToFStrings(Quest.FactsToRetract);
		return Out;
	}
} // namespace

UInsimulRadiantSourceShell::UInsimulRadiantSourceShell()
{
	// Starting the core runtime is the expensive call; do it once, here, and
	// keep it for this object's lifetime — as with a libinsimul KB.
	Bridge = MakeUnique<insimul::FInsimulCoreBridge>();
	Adapter = MakeUnique<insimul::FRadiantSource>(Bridge.Get(), insimul::ERadiantSource::Core);

	if (!Bridge->IsAvailable())
	{
		// Not an error: a platform with no libinsimulcore build falls back to
		// the pre-adoption path (RUNTIME_CORE_ADOPTION.md §4.7.2). Log it once
		// so the fallback is visible rather than silent.
		UE_LOG(LogInsimulRadiant, Log, TEXT("radiant generation unavailable, falling back: %s"),
			*ToFString(Bridge->LastError()));
	}
}

void UInsimulRadiantSourceShell::BeginDestroy()
{
	// Deterministic release, before UObject teardown. Adapter borrows Bridge, so
	// it must go first.
	Adapter.Reset();
	Bridge.Reset();
	Super::BeginDestroy();
}

void UInsimulRadiantSourceShell::SetSource(EInsimulRadiantSource InSource)
{
	checkf(IsInGameThread(), TEXT("UInsimulRadiantSourceShell must be used on the game thread"));
	Adapter->SetSource(InSource == EInsimulRadiantSource::None
		? insimul::ERadiantSource::None
		: insimul::ERadiantSource::Core);
}

EInsimulRadiantSource UInsimulRadiantSourceShell::GetSource() const
{
	return Adapter->Source() == insimul::ERadiantSource::None
		? EInsimulRadiantSource::None
		: EInsimulRadiantSource::Core;
}

bool UInsimulRadiantSourceShell::IsCoreAvailable() const
{
	return Adapter->IsCoreAvailable();
}

bool UInsimulRadiantSourceShell::GenerateQuests(const TArray<FString>& KbLines, const TArray<FString>& TemplateLines,
	const FString& Seed, int64 Now, int32 MaxQuests, TArray<FInsimulGeneratedQuest>& OutQuests)
{
	checkf(IsInGameThread(), TEXT("UInsimulRadiantSourceShell must be used on the game thread"));
	OutQuests.Reset();

	insimul::FRadiantOptions Options;
	Options.Seed = insimul::FRadiantSeed::FromText(ToStdString(Seed));
	Options.Now = static_cast<long long>(Now);
	Options.MaxQuests = static_cast<int>(MaxQuests);

	std::vector<insimul::FGeneratedRadiantQuest> Quests;
	const bool bOk = Adapter->Generate(ToStdLines(KbLines), ToStdLines(TemplateLines), Options, Quests);
	if (!bOk)
	{
		UE_LOG(LogInsimulRadiant, Warning, TEXT("radiant.generate failed: %s"),
			*ToFString(Adapter->LastError()));
		return false;
	}

	OutQuests.Reserve(static_cast<int32>(Quests.size()));
	for (const insimul::FGeneratedRadiantQuest& Quest : Quests)
	{
		OutQuests.Add(MakeQuest(Quest));
	}
	return true;
}

FString UInsimulRadiantSourceShell::GetBaseTemplates()
{
	checkf(IsInGameThread(), TEXT("UInsimulRadiantSourceShell must be used on the game thread"));
	return ToFString(Adapter->BaseTemplates());
}

TArray<FString> UInsimulRadiantSourceShell::GetBaseTemplateIds()
{
	checkf(IsInGameThread(), TEXT("UInsimulRadiantSourceShell must be used on the game thread"));
	return ToFStrings(Adapter->BaseTemplateIds());
}

TArray<FString> UInsimulRadiantSourceShell::GetCoreMethods()
{
	checkf(IsInGameThread(), TEXT("UInsimulRadiantSourceShell must be used on the game thread"));
	return ToFStrings(Adapter->CoreMethods());
}

FString UInsimulRadiantSourceShell::GetCoreVersion() const
{
	return ToFString(Bridge->Version());
}

FString UInsimulRadiantSourceShell::GetLastError() const
{
	return ToFString(Adapter->LastError());
}
