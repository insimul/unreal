// Copyright 2024 Insimul. All Rights Reserved.
//
// See InsimulModulePacks.h. Like the resolver beside it, this file names no
// mechanic: the areas come from the manifest and the active set, never from here.

#include "InsimulModulePacks.h"

#include "InsimulJson.h"

#include <fstream>
#include <sstream>

namespace insimul {

namespace {

const std::vector<std::string>& EmptyNames()
{
	static const std::vector<std::string> Empty;
	return Empty;
}

std::vector<std::string> ReadStringArray(const FJsonValue& Owner, const std::string& Key)
{
	std::vector<std::string> Out;
	const FJsonValue* Arr = Owner.Find(Key);
	if (Arr == nullptr || !Arr->IsArray())
	{
		return Out;
	}
	for (const FJsonValuePtr& Item : Arr->ArrayItems)
	{
		if (Item && Item->IsString())
		{
			Out.push_back(Item->StringValue);
		}
	}
	return Out;
}

std::string JoinPath(const std::string& Dir, const std::string& File)
{
	if (Dir.empty())
	{
		return File;
	}
	const char Last = Dir[Dir.size() - 1];
	return (Last == '/' || Last == '\\') ? Dir + File : Dir + "/" + File;
}

} // namespace

// ── FInsimulPredicatePackManifest ────────────────────────────────────────────

bool FInsimulPredicatePackManifest::Parse(
	const std::string& Json, FInsimulPredicatePackManifest& OutManifest, std::string& OutError)
{
	OutManifest = FInsimulPredicatePackManifest();
	OutError.clear();

	const FJsonParseResult Parsed = ParseJson(Json);
	if (!Parsed.bOk || !Parsed.Root)
	{
		OutError = "PACKS.json is not JSON: " + Parsed.Error;
		return false;
	}
	if (!Parsed.Root->IsObject())
	{
		OutError = "PACKS.json is not a JSON object";
		return false;
	}

	OutManifest.Commit = Parsed.Root->GetString("coreCommit", "unknown");
	OutManifest.Order = ReadStringArray(*Parsed.Root, "consultOrder");
	OutManifest.AlwaysActive = ReadStringArray(*Parsed.Root, "alwaysActivePacks");

	const FJsonValue* Packs = Parsed.Root->Find("packs");
	if (Packs != nullptr && Packs->IsObject())
	{
		for (const auto& Entry : Packs->ObjectItems)
		{
			if (!Entry.second || !Entry.second->IsObject())
			{
				continue;
			}
			FEntry Pack;
			Pack.Area = Entry.first;
			Pack.File = Entry.second->GetString("file");
			Pack.RuntimePredicates = ReadStringArray(*Entry.second, "runtimePredicates");
			OutManifest.Entries.push_back(Pack);
		}
	}

	if (OutManifest.Order.empty())
	{
		OutError = "PACKS.json declares no consultOrder";
		return false;
	}
	return true;
}

const FInsimulPredicatePackManifest::FEntry* FInsimulPredicatePackManifest::FindEntry(const std::string& Area) const
{
	for (const FEntry& Entry : Entries)
	{
		if (Entry.Area == Area)
		{
			return &Entry;
		}
	}
	return nullptr;
}

std::string FInsimulPredicatePackManifest::FileOf(const std::string& Area) const
{
	const FEntry* Entry = FindEntry(Area);
	return Entry != nullptr ? Entry->File : std::string();
}

const std::vector<std::string>& FInsimulPredicatePackManifest::RuntimePredicatesOf(const std::string& Area) const
{
	const FEntry* Entry = FindEntry(Area);
	return Entry != nullptr ? Entry->RuntimePredicates : EmptyNames();
}

// ── the sources ──────────────────────────────────────────────────────────────

bool FInsimulDirectoryPackSource::Read(const std::string& Area, std::string& OutText)
{
	Error.clear();
	OutText.clear();

	std::string File = Manifest != nullptr ? Manifest->FileOf(Area) : std::string();
	if (File.empty())
	{
		File = Area + ".pl";
	}
	const std::string Path = JoinPath(Dir, File);

	std::ifstream In(Path.c_str(), std::ios::binary);
	if (!In)
	{
		Error = Path + " does not exist or cannot be read";
		return false;
	}
	std::ostringstream Buffer;
	Buffer << In.rdbuf();
	OutText = Buffer.str();
	if (OutText.empty())
	{
		Error = Path + " is empty";
		return false;
	}
	return true;
}

bool FInsimulMemoryPackSource::Read(const std::string& Area, std::string& OutText)
{
	Error.clear();
	OutText.clear();
	for (const auto& Pair : Texts)
	{
		if (Pair.first == Area)
		{
			OutText = Pair.second;
			return true;
		}
	}
	Error = "no pack '" + Area + "' was supplied";
	return false;
}

// ── the report ───────────────────────────────────────────────────────────────

namespace {

std::vector<std::string> NamesWith(const std::vector<FInsimulPackResult>& Results, EInsimulPackOutcome Outcome)
{
	std::vector<std::string> Out;
	for (const FInsimulPackResult& R : Results)
	{
		if (R.Outcome == Outcome)
		{
			Out.push_back(R.Area);
		}
	}
	return Out;
}

} // namespace

std::vector<std::string> FInsimulPackConsultReport::Consulted() const
{
	return NamesWith(Results, EInsimulPackOutcome::Consulted);
}

std::vector<std::string> FInsimulPackConsultReport::Skipped() const
{
	return NamesWith(Results, EInsimulPackOutcome::Inactive);
}

std::vector<std::string> FInsimulPackConsultReport::Missing() const
{
	return NamesWith(Results, EInsimulPackOutcome::Missing);
}

std::vector<std::string> FInsimulPackConsultReport::Failed() const
{
	return NamesWith(Results, EInsimulPackOutcome::Failed);
}

bool FInsimulPackConsultReport::IsOk() const
{
	for (const FInsimulPackResult& R : Results)
	{
		if (R.Outcome == EInsimulPackOutcome::Missing || R.Outcome == EInsimulPackOutcome::Failed)
		{
			return false;
		}
	}
	return true;
}

std::string FInsimulPackConsultReport::Describe() const
{
	std::string Out = "consulted [" + JoinNames(Consulted()) + "]; ";
	Out += "not activated, so NOT consulted [" + JoinNames(Skipped()) + "]";
	for (const FInsimulPackResult& R : Results)
	{
		if (R.Outcome == EInsimulPackOutcome::Missing || R.Outcome == EInsimulPackOutcome::Failed)
		{
			Out += "\n  ";
			Out += R.Outcome == EInsimulPackOutcome::Missing ? "MISSING " : "FAILED  ";
			Out += R.Area + ": " + R.Detail;
		}
	}
	return Out;
}

// ── the activation ───────────────────────────────────────────────────────────

FInsimulPackConsultReport ConsultActivePacks(
	const FInsimulActiveModuleSet* Set,
	const FInsimulPredicatePackManifest& Manifest,
	IInsimulPredicatePackSource* Source,
	const std::function<bool(const std::string& Text, std::string& OutError)>& Consult)
{
	FInsimulPackConsultReport Report;

	for (const std::string& Area : Manifest.ConsultOrder())
	{
		const bool bActive = Set != nullptr && Set->IsPackActive(Area);
		if (!bActive)
		{
			FInsimulPackResult Result;
			Result.Area = Area;
			Result.Outcome = EInsimulPackOutcome::Inactive;
			Result.Detail = "no active module owns this pack";
			Report.Results.push_back(Result);
			continue;
		}

		std::string Text;
		if (Source == nullptr || !Source->Read(Area, Text) || Text.empty())
		{
			const std::string Why = Source == nullptr
				? "no pack source was supplied"
				: (Source->LastError().empty() ? "the source returned no text" : Source->LastError());
			FInsimulPackResult Result;
			Result.Area = Area;
			Result.Outcome = EInsimulPackOutcome::Missing;
			Result.Detail = Why + " — the module is active and its vocabulary will not exist in this KB";
			Report.Results.push_back(Result);
			continue;
		}

		FInsimulPackResult Result;
		Result.Area = Area;
		Result.Bytes = Text.size();
		std::string Error;
		if (!Consult || !Consult(Text, Error))
		{
			Result.Outcome = EInsimulPackOutcome::Failed;
			Result.Detail = !Consult ? "no consult was supplied" : (Error.empty() ? "the engine refused the program" : Error);
		}
		else
		{
			Result.Outcome = EInsimulPackOutcome::Consulted;
		}
		Report.Results.push_back(Result);
	}

	return Report;
}

} // namespace insimul
