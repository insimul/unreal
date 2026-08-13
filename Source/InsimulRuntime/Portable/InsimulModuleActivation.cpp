// Copyright 2024 Insimul. All Rights Reserved.
//
// See InsimulModuleActivation.h. Nothing in this file names a mechanic — the table
// it reads does, and `tools/verify-mechanics/check-activation.mjs` fails if one ever
// appears here.

#include "InsimulModuleActivation.h"

#include "InsimulJson.h"

namespace insimul {

namespace {

bool Blank(const std::string& S)
{
	for (char C : S)
	{
		if (C != ' ' && C != '\t' && C != '\n' && C != '\r')
		{
			return false;
		}
	}
	return true;
}

bool ContainsName(const std::vector<std::string>& Names, const std::string& Name)
{
	for (const std::string& N : Names)
	{
		if (N == Name)
		{
			return true;
		}
	}
	return false;
}

std::vector<std::string> ReadStrings(const FJsonValue& Owner, const std::string& Key)
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

FInsimulActiveModule ReadModule(const FJsonValue& M)
{
	FInsimulActiveModule Module;
	Module.Id = M.GetString("id");
	Module.Name = M.GetString("name");
	Module.PredicatePack = M.GetString("predicatePack");
	Module.IrSection = M.GetString("irSection");
	Module.DecisionLayers = ReadStrings(M, "decisionLayer");
	Module.HostInterfaces = ReadStrings(M, "hostInterface");
	Module.bConforms = M.GetBool("conforms");
	return Module;
}

std::vector<std::string> HostsOf(const std::vector<FInsimulActiveModule>& Modules)
{
	std::vector<std::string> Hosts;
	for (const FInsimulActiveModule& M : Modules)
	{
		for (const std::string& H : M.HostInterfaces)
		{
			if (!ContainsName(Hosts, H))
			{
				Hosts.push_back(H);
			}
		}
	}
	return Hosts;
}

} // namespace

std::vector<std::string> FilterInUniverseOrder(
	const std::vector<std::string>& Universe, const std::vector<std::string>& Wanted)
{
	std::vector<std::string> Kept;
	for (const std::string& Area : Universe)
	{
		if (ContainsName(Wanted, Area))
		{
			Kept.push_back(Area);
		}
	}
	return Kept;
}

std::string JoinNames(const std::vector<std::string>& Names)
{
	if (Names.empty())
	{
		return "none";
	}
	std::string Out;
	for (const std::string& N : Names)
	{
		if (!Out.empty())
		{
			Out += ", ";
		}
		Out += N;
	}
	return Out;
}

// ── FInsimulActiveModuleSet ──────────────────────────────────────────────────

bool FInsimulActiveModuleSet::IsModuleActive(const std::string& ModuleId) const
{
	for (const FInsimulActiveModule& M : Modules)
	{
		if (M.Id == ModuleId)
		{
			return true;
		}
	}
	return false;
}

bool FInsimulActiveModuleSet::IsPackActive(const std::string& Area) const
{
	return ContainsName(PredicatePacks, Area);
}

bool FInsimulActiveModuleSet::ActivatesHost(const std::string& HostInterface) const
{
	return ContainsName(HostInterfaces, HostInterface);
}

std::string FInsimulActiveModuleSet::Describe() const
{
	std::vector<std::string> Ids;
	Ids.reserve(Modules.size());
	for (const FInsimulActiveModule& M : Modules)
	{
		Ids.push_back(M.Id);
	}

	std::string Out = "genre '" + Genre + "' (";
	Out += Source == EInsimulGenreSource::Undeclared ? "no genre declared"
		: Source == EInsimulGenreSource::WorldIr     ? "from the World IR"
													 : "declared by the game";
	if (!bKnown && Source != EInsimulGenreSource::Undeclared)
	{
		Out += ", NOT a genre core knows";
	}
	Out += "): ";
	Out += std::to_string(Modules.size()) + " module(s) [" + JoinNames(Ids) + "], ";
	Out += std::to_string(PredicatePacks.size()) + " pack(s) [" + JoinNames(PredicatePacks) + "], ";
	Out += std::to_string(HostInterfaces.size()) + " host interface(s) [" + JoinNames(HostInterfaces) + "]";
	return Out;
}

// ── FInsimulActivationTable ──────────────────────────────────────────────────

bool FInsimulActivationTable::Parse(
	const std::string& Json, FInsimulActivationTable& OutTable, std::string& OutError)
{
	OutTable = FInsimulActivationTable();
	OutError.clear();

	const FJsonParseResult Parsed = ParseJson(Json);
	if (!Parsed.bOk || !Parsed.Root)
	{
		OutError = "the activation table is not JSON: " + Parsed.Error;
		return false;
	}
	if (!Parsed.Root->IsObject())
	{
		OutError = "the activation table is not a JSON object";
		return false;
	}

	OutTable.AlwaysActive = ReadStrings(*Parsed.Root, "alwaysActivePacks");

	const FJsonValue* Genres = Parsed.Root->Find("genres");
	if (Genres == nullptr || !Genres->IsObject())
	{
		OutError = "the activation table declares no 'genres' object";
		return false;
	}

	for (const auto& Entry : Genres->ObjectItems)
	{
		FGenreEntry Genre;
		if (Entry.second && Entry.second->IsObject())
		{
			const FJsonValue* Modules = Entry.second->Find("modules");
			if (Modules != nullptr && Modules->IsArray())
			{
				for (const FJsonValuePtr& M : Modules->ArrayItems)
				{
					if (M && M->IsObject())
					{
						Genre.Modules.push_back(ReadModule(*M));
					}
				}
			}
			Genre.DeclaredPacks = ReadStrings(*Entry.second, "predicatePacks");
		}
		OutTable.GenreOrder.push_back(Entry.first);
		OutTable.ByGenre.emplace_back(Entry.first, Genre);
	}

	if (OutTable.GenreOrder.empty())
	{
		OutError = "the activation table knows no genres";
		return false;
	}
	return true;
}

std::string FInsimulActivationTable::GenreOfWorldIr(const std::string& IrJson)
{
	const FJsonParseResult Parsed = ParseJson(IrJson);
	if (!Parsed.bOk || !Parsed.Root || !Parsed.Root->IsObject())
	{
		return std::string();
	}
	const FJsonValue* Meta = Parsed.Root->Find("meta");
	if (Meta == nullptr || !Meta->IsObject())
	{
		return std::string();
	}
	const FJsonValue* Config = Meta->Find("genreConfig");
	if (Config == nullptr)
	{
		return std::string();
	}
	// Core's type is an object with an id; a document that shortens it to the bare
	// string is read rather than rejected — the id is the whole payload.
	if (Config->IsString())
	{
		return Blank(Config->StringValue) ? std::string() : Config->StringValue;
	}
	if (!Config->IsObject())
	{
		return std::string();
	}
	const std::string Id = Config->GetString("id");
	return Blank(Id) ? std::string() : Id;
}

const FInsimulActivationTable::FGenreEntry* FInsimulActivationTable::FindGenre(const std::string& GenreId) const
{
	for (const auto& Entry : ByGenre)
	{
		if (Entry.first == GenreId)
		{
			return &Entry.second;
		}
	}
	return nullptr;
}

FInsimulActiveModuleSet FInsimulActivationTable::Resolve(
	const std::string& GenreId,
	const std::vector<std::string>& PackUniverse,
	EInsimulGenreSource Source) const
{
	FInsimulActiveModuleSet Set;

	// No genre at all: every pack, every module the table knows. Core's
	// GamePrologEngineConfig does exactly this, and for the same reason.
	if (Blank(GenreId))
	{
		std::vector<FInsimulActiveModule> All;
		for (const auto& Entry : ByGenre)
		{
			for (const FInsimulActiveModule& M : Entry.second.Modules)
			{
				bool bSeen = false;
				for (const FInsimulActiveModule& Have : All)
				{
					if (Have.Id == M.Id)
					{
						bSeen = true;
						break;
					}
				}
				if (!bSeen)
				{
					All.push_back(M);
				}
			}
		}
		Set.Genre.clear();
		Set.bKnown = false;
		Set.Source = EInsimulGenreSource::Undeclared;
		Set.Modules = All;
		Set.PredicatePacks = PackUniverse;
		Set.HostInterfaces = HostsOf(All);
		return Set;
	}

	const FGenreEntry* Entry = FindGenre(GenreId);
	if (Entry == nullptr)
	{
		// An unrecognised genre gets the shared vocabulary and nothing else.
		Set.Genre = GenreId;
		Set.bKnown = false;
		Set.Source = Source;
		Set.PredicatePacks = FilterInUniverseOrder(PackUniverse, AlwaysActive);
		return Set;
	}

	std::vector<std::string> Owned = AlwaysActive;
	for (const FInsimulActiveModule& M : Entry->Modules)
	{
		if (!M.PredicatePack.empty() && !ContainsName(Owned, M.PredicatePack))
		{
			Owned.push_back(M.PredicatePack);
		}
	}
	const std::vector<std::string> Packs = FilterInUniverseOrder(PackUniverse, Owned);

	// The table also STATES the pack list. Recomputing it and comparing is how a
	// table emitted from a core whose pack order moved stops being silently trusted;
	// neither side is preferred, the disagreement is reported and the recomputed
	// order (this build's real packs) is used.
	for (const std::string& Area : Entry->DeclaredPacks)
	{
		if (!ContainsName(Packs, Area))
		{
			Set.Warnings.push_back(
				"the activation table lists pack '" + Area + "' for genre '" + GenreId +
				"', and this build carries no such pack");
		}
	}
	for (const std::string& Area : Packs)
	{
		if (!Entry->DeclaredPacks.empty() && !ContainsName(Entry->DeclaredPacks, Area))
		{
			Set.Warnings.push_back(
				"pack '" + Area + "' resolves as active for genre '" + GenreId +
				"' and the activation table does not list it");
		}
	}

	Set.Genre = GenreId;
	Set.bKnown = true;
	Set.Source = Source;
	Set.Modules = Entry->Modules;
	Set.PredicatePacks = Packs;
	Set.HostInterfaces = HostsOf(Entry->Modules);
	return Set;
}

FInsimulActiveModuleSet FInsimulActivationTable::ResolveForWorldIr(
	const std::string& IrJson, const std::vector<std::string>& PackUniverse) const
{
	const std::string Genre = GenreOfWorldIr(IrJson);
	return Resolve(
		Genre, PackUniverse,
		Genre.empty() ? EInsimulGenreSource::Undeclared : EInsimulGenreSource::WorldIr);
}

} // namespace insimul
