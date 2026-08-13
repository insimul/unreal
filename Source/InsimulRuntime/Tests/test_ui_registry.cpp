// Copyright 2024 Insimul. All Rights Reserved.
//
// test_ui_registry.cpp — host gate for the default-UI registry + loading
// view-model + theme tokens (US-XU1). Builds under a plain clang toolchain (no
// Unreal Engine, no UBT; ctest `ui_registry`) and proves the
// Unreal cores against the SAME engine-neutral corpus every other default-UI
// mirror runs (packages/core/conformance/ui/*.json), so the four legs (Babylon,
// Unity, Godot, Unreal) can never diverge:
//
//   - registry resolution: default-resolves / override-wins / override-of-one-key
//     -leaves-others-default / unknown-is-missing-and-diagnosed / override-with-no
//     -default (registry-cases.json), plus the real Unreal default WBP map;
//   - loading view-model: weighted cumulative progress, monotonicity, labels,
//     completion, per-phase tips (loading-phases.json);
//   - theme tokens: the Unreal token table matches theme-tokens.json byte-for-byte.
//
// The UE seams (UInsimulUIRegistry / UInsimulUITheme UDataAssets, the loading
// UUserWidget) sit ON TOP of these pure cores and are syntax-gated only.
//
// Tasklist 190 US-1 added the fourth leg — the MODULE GATE — and, more basically,
// a gate that runs this file at all: until then it named a runner
// (run-ui-tests.sh) that does not exist in this repository, so nothing compiled it.
// It is ctest `ui_registry` now (tools/verify-unreal/CMakeLists.txt).
//
//   - panel catalog + module gate: the catalog an exported game ships
//     (Content/Data/insimul/ui/panels.json) covers every corpus panel key, agrees
//     with the built-in fallback map, names only modules core's activation table
//     knows, and withholds exactly the panels whose module a genre did not select —
//     with a creator override proven unable to ungate one.
//
// The conformance dir is argv[1] (the runner passes REPO/packages/core/
// conformance/ui); it falls back to INSIMUL_UI_DIR, then to a path relative to this
// source file. The shipped data dir is argv[2], falling back to
// INSIMUL_ACTIVATION_DATA_DIR.

#include "../Portable/InsimulUIRegistryModel.h"
#include "../Portable/InsimulLoadingViewModel.h"
#include "../Portable/InsimulUIThemeTokens.h"
#include "../Portable/InsimulUIPanelCatalog.h"
#include "../Portable/InsimulModuleActivation.h"
#include "../Portable/InsimulJson.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace insimul;

namespace {

int g_pass = 0;
int g_fail = 0;

void Report(const std::string& Name, bool bOk, const std::string& Detail = "") {
	std::printf("  %s  %-58s%s%s\n", bOk ? "PASS" : "FAIL", Name.c_str(),
			Detail.empty() ? "" : "  ", Detail.c_str());
	if (bOk) {
		g_pass++;
	} else {
		g_fail++;
	}
}

std::string ReadFile(const std::string& Path) {
	std::ifstream In(Path, std::ios::binary);
	if (!In) {
		return std::string();
	}
	std::ostringstream Ss;
	Ss << In.rdbuf();
	return Ss.str();
}

FJsonValuePtr LoadJson(const std::string& Dir, const std::string& File) {
	const std::string Text = ReadFile(Dir + "/" + File);
	if (Text.empty()) {
		std::printf("  FAIL  could not read corpus file: %s/%s\n", Dir.c_str(), File.c_str());
		g_fail++;
		return nullptr;
	}
	FJsonParseResult Parsed = ParseJson(Text);
	if (!Parsed.bOk) {
		std::printf("  FAIL  parse error in %s: %s\n", File.c_str(), Parsed.Error.c_str());
		g_fail++;
		return nullptr;
	}
	return Parsed.Root;
}

// Read an object's { key: value } string members into an ordered vector so the
// insertion order (and thus override precedence) is preserved.
std::vector<std::pair<std::string, std::string>> ObjectToPairs(const FJsonValue* Obj) {
	std::vector<std::pair<std::string, std::string>> Out;
	if (Obj && Obj->IsObject()) {
		for (const auto& Member : Obj->ObjectItems) {
			Out.emplace_back(Member.first, Member.second ? Member.second->AsString() : std::string());
		}
	}
	return Out;
}

bool NearlyEqual(double A, double B) { return std::fabs(A - B) < 1e-9; }

// ── Registry corpus ──────────────────────────────────────────────────────────
void RunRegistryCases(const std::string& Dir) {
	FJsonValuePtr Root = LoadJson(Dir, "registry-cases.json");
	if (!Root) {
		return;
	}
	const FJsonValue* Cases = Root->Find("cases");
	if (!Cases || !Cases->IsArray()) {
		Report("registry-cases.json has a cases array", false);
		return;
	}
	for (std::size_t i = 0; i < Cases->Size(); ++i) {
		const FJsonValue* C = Cases->ArrayItems[i].get();
		const std::string Name = C->GetString("name");

		FInsimulUIRegistryModel Reg(ObjectToPairs(C->Find("defaults")));
		Reg.ApplyOverrides(ObjectToPairs(C->Find("overrides")));

		const std::string Key = C->GetString("resolve");
		const std::string ExpectedScene = C->GetString("expected_scene");
		const bool ExpectedMissing = C->GetBool("expected_missing");
		const bool ExpectedOverridden = C->GetBool("expected_overridden");

		const std::string GotScene = Reg.SceneRef(Key);
		const bool GotMissing = Reg.HasDiagnostics();
		const bool GotOverridden = Reg.IsOverridden(Key);

		const bool bOk = GotScene == ExpectedScene && GotMissing == ExpectedMissing &&
			GotOverridden == ExpectedOverridden;
		Report("registry: " + Name, bOk,
			bOk ? "" : ("scene='" + GotScene + "' missing=" + (GotMissing ? "1" : "0") +
				" overridden=" + (GotOverridden ? "1" : "0")));
	}
}

// ── The real Unreal default panel map ────────────────────────────────────────
void RunDefaultMapChecks(const std::string& Dir) {
	FJsonValuePtr Root = LoadJson(Dir, "registry-cases.json");
	if (!Root) {
		return;
	}
	FInsimulUIRegistryModel Reg(FInsimulUIRegistryModel::DefaultPanelMap());

	// Every corpus panel_key must resolve to a non-empty default WBP ref.
	const FJsonValue* Keys = Root->Find("panel_keys");
	bool bAllPresent = Keys && Keys->IsArray() && Keys->Size() > 0;
	std::string MissingKey;
	if (Keys && Keys->IsArray()) {
		for (std::size_t i = 0; i < Keys->Size(); ++i) {
			const std::string K = Keys->ArrayItems[i]->AsString();
			if (Reg.PeekRef(K).empty()) {
				bAllPresent = false;
				MissingKey = K;
				break;
			}
		}
	}
	Report("registry: Unreal default map covers every corpus panel_key", bAllPresent,
		MissingKey.empty() ? "" : ("missing default for '" + MissingKey + "'"));
	Report("registry: default map records no diagnostics for known keys",
		!Reg.HasDiagnostics());

	// A creator override still wins over the shipped default.
	Reg.Register("quest_journal", "/Game/Custom/WBP_MyJournal.WBP_MyJournal_C");
	const bool bOverride = Reg.IsOverridden("quest_journal") &&
		Reg.SceneRef("quest_journal") == "/Game/Custom/WBP_MyJournal.WBP_MyJournal_C";
	Report("registry: creator override wins over the WBP default", bOverride);
}

// ── Loading view-model corpus ────────────────────────────────────────────────
void RunLoadingCases(const std::string& Dir) {
	FJsonValuePtr Root = LoadJson(Dir, "loading-phases.json");
	if (!Root) {
		return;
	}

	// Build the phase table / tips from the corpus so the model runs the exact
	// shared definitions (and verify the Unreal defaults equal them below).
	std::vector<FLoadingPhase> CorpusPhases;
	const FJsonValue* Phases = Root->Find("phases");
	if (Phases && Phases->IsArray()) {
		for (std::size_t i = 0; i < Phases->Size(); ++i) {
			const FJsonValue* P = Phases->ArrayItems[i].get();
			CorpusPhases.push_back({P->GetString("key"), P->GetString("label"),
				static_cast<int>(P->GetInt("weight", 1))});
		}
	}
	std::vector<std::string> CorpusTips;
	const FJsonValue* Tips = Root->Find("tips");
	if (Tips && Tips->IsArray()) {
		for (std::size_t i = 0; i < Tips->Size(); ++i) {
			CorpusTips.push_back(Tips->ArrayItems[i]->AsString());
		}
	}

	// The Unreal default phase table must equal the corpus (parity contract).
	{
		std::vector<FLoadingPhase> Def = FInsimulLoadingViewModel::DefaultPhases();
		bool bMatch = Def.size() == CorpusPhases.size();
		for (std::size_t i = 0; bMatch && i < Def.size(); ++i) {
			bMatch = Def[i].Key == CorpusPhases[i].Key && Def[i].Label == CorpusPhases[i].Label &&
				Def[i].Weight == CorpusPhases[i].Weight;
		}
		Report("loading: Unreal DefaultPhases() matches loading-phases.json", bMatch);
		Report("loading: Unreal DefaultTips() matches loading-phases.json",
			FInsimulLoadingViewModel::DefaultTips() == CorpusTips);
	}

	const FJsonValue* Cases = Root->Find("cases");
	if (!Cases || !Cases->IsArray()) {
		Report("loading-phases.json has a cases array", false);
		return;
	}
	for (std::size_t i = 0; i < Cases->Size(); ++i) {
		const FJsonValue* C = Cases->ArrayItems[i].get();
		const std::string Name = C->GetString("name");
		FInsimulLoadingViewModel VM(CorpusPhases, CorpusTips);

		const FJsonValue* Steps = C->Find("steps");
		bool bOk = Steps && Steps->IsArray();
		std::string Detail;
		if (Steps && Steps->IsArray()) {
			for (std::size_t s = 0; s < Steps->Size() && bOk; ++s) {
				const FJsonValue* Step = Steps->ArrayItems[s].get();
				const std::string Adv = Step->GetString("advance");
				VM.Advance(Adv);
				const double ExpProg = Step->Find("expected_progress")
					? Step->Find("expected_progress")->AsNumber() : 0.0;
				const std::string ExpLabel = Step->GetString("expected_label");
				const bool ExpComplete = Step->GetBool("expected_complete");
				if (!NearlyEqual(VM.Progress(), ExpProg) || VM.Label() != ExpLabel ||
					VM.IsComplete() != ExpComplete) {
					bOk = false;
					char Buf[128];
					std::snprintf(Buf, sizeof(Buf), "step '%s' progress=%.4f complete=%d",
						Adv.c_str(), VM.Progress(), VM.IsComplete() ? 1 : 0);
					Detail = Buf;
				}
			}
		}
		Report("loading: " + Name, bOk, Detail);
	}

	// Deterministic per-phase tip + reset behavior (not in the corpus steps).
	{
		FInsimulLoadingViewModel VM(CorpusPhases, CorpusTips);
		VM.Advance("init");
		const bool bTip0 = VM.Tip() == CorpusTips[0 % CorpusTips.size()];
		VM.Advance("kb"); // index 3 -> tips[3 % size]
		const bool bTip3 = VM.Tip() == CorpusTips[3 % CorpusTips.size()];
		VM.Reset();
		const bool bReset = NearlyEqual(VM.Progress(), 0.0) && VM.CurrentPhase().empty() &&
			!VM.IsComplete();
		Report("loading: deterministic per-phase tip", bTip0 && bTip3);
		Report("loading: reset returns to pre-boot state", bReset);
	}
}

// ── Theme tokens corpus ──────────────────────────────────────────────────────
bool PairsMatchInts(const std::vector<std::pair<std::string, int>>& Got, const FJsonValue* Obj) {
	if (!Obj || !Obj->IsObject() || Obj->ObjectItems.size() != Got.size()) {
		return false;
	}
	for (const auto& Pair : Got) {
		const FJsonValue* V = Obj->Find(Pair.first);
		if (!V || static_cast<int>(V->AsInt()) != Pair.second) {
			return false;
		}
	}
	return true;
}

void RunThemeCases(const std::string& Dir) {
	FJsonValuePtr Root = LoadJson(Dir, "theme-tokens.json");
	if (!Root) {
		return;
	}

	// Colors.
	const FJsonValue* Colors = Root->Find("colors");
	auto GotColors = FInsimulUIThemeTokens::Colors();
	bool bColors = Colors && Colors->IsObject() && Colors->ObjectItems.size() == GotColors.size();
	std::string BadColor;
	for (const auto& Pair : GotColors) {
		const FJsonValue* V = Colors ? Colors->Find(Pair.first) : nullptr;
		if (!V || V->AsString() != Pair.second) {
			bColors = false;
			BadColor = Pair.first;
			break;
		}
	}
	Report("theme: colors match theme-tokens.json", bColors,
		BadColor.empty() ? "" : ("diverged on '" + BadColor + "'"));

	Report("theme: spacing matches theme-tokens.json",
		PairsMatchInts(FInsimulUIThemeTokens::Spacing(), Root->Find("spacing")));
	Report("theme: radius matches theme-tokens.json",
		PairsMatchInts(FInsimulUIThemeTokens::Radius(), Root->Find("radius")));
	Report("theme: font_size matches theme-tokens.json",
		PairsMatchInts(FInsimulUIThemeTokens::FontSize(), Root->Find("font_size")));

	Report("theme: Color() accessor resolves a known token",
		FInsimulUIThemeTokens::Color("accent") == "#5b8cff" &&
		FInsimulUIThemeTokens::Color("nope").empty());
}


// ── Panel catalog + the module gate (tasklist 190 US-1) ──────────────────────
//
// The catalog and the activation table are read from the SHIPPED data dir — the
// same two files an exported game loads — so a re-pointed panel or a re-vendored
// table this gate would not notice cannot exist.

bool LoadCatalog(const std::string& DataDir, FInsimulUIPanelCatalog& Out) {
	const std::string Text = ReadFile(DataDir + "/ui/panels.json");
	if (Text.empty()) {
		Report("catalog: Content/Data/insimul/ui/panels.json is readable", false, DataDir);
		return false;
	}
	std::string Error;
	if (!FInsimulUIPanelCatalog::Parse(Text, Out, Error)) {
		Report("catalog: panels.json parses", false, Error);
		return false;
	}
	return true;
}

bool LoadTable(const std::string& DataDir, FInsimulActivationTable& Out) {
	const std::string Text = ReadFile(DataDir + "/modules/genre-activation.json");
	if (Text.empty()) {
		Report("catalog: the activation table is readable", false, DataDir);
		return false;
	}
	std::string Error;
	if (!FInsimulActivationTable::Parse(Text, Out, Error)) {
		Report("catalog: the activation table parses", false, Error);
		return false;
	}
	return true;
}

/** Every module id the table names, across every genre it knows. */
std::vector<std::string> TableModuleIds(const FInsimulActivationTable& Table) {
	std::vector<std::string> Out;
	for (const std::string& Genre : Table.Genres()) {
		const FInsimulActiveModuleSet Set =
			Table.Resolve(Genre, std::vector<std::string>(), EInsimulGenreSource::WorldIr);
		for (const FInsimulActiveModule& Module : Set.Modules) {
			if (std::find(Out.begin(), Out.end(), Module.Id) == Out.end()) {
				Out.push_back(Module.Id);
			}
		}
	}
	return Out;
}

/**
 * The check a mutated catalog must fail: every module a panel row names is one the
 * activation table knows. A typo here would gate a panel out of EVERY world with no
 * error anywhere, which is the quietest failure this feature can have.
 */
std::string UnknownModuleIn(const FInsimulUIPanelCatalog& Catalog, const FInsimulActivationTable& Table) {
	const std::vector<std::string> Known = TableModuleIds(Table);
	for (const std::string& Module : Catalog.Modules()) {
		if (std::find(Known.begin(), Known.end(), Module) == Known.end()) {
			return Module;
		}
	}
	return std::string();
}

/** The genre activating the MOST of the catalog's modules, and one activating none. */
void PickGenres(const FInsimulUIPanelCatalog& Catalog, const FInsimulActivationTable& Table,
		std::string& OutOwner, std::string& OutEmpty) {
	std::size_t Best = 0;
	for (const std::string& Genre : Table.Genres()) {
		const FInsimulActiveModuleSet Set =
			Table.Resolve(Genre, std::vector<std::string>(), EInsimulGenreSource::WorldIr);
		std::size_t Hits = 0;
		for (const std::string& Module : Catalog.Modules()) {
			if (Set.IsModuleActive(Module)) {
				++Hits;
			}
		}
		if (Hits > Best) {
			Best = Hits;
			OutOwner = Genre;
		}
		if (Hits == 0 && OutEmpty.empty()) {
			OutEmpty = Genre;
		}
	}
}

void RunPanelCatalogCases(const std::string& CorpusDir, const std::string& DataDir) {
	FInsimulUIPanelCatalog Catalog;
	if (!LoadCatalog(DataDir, Catalog)) {
		return;
	}
	Report("catalog: the shipped panels.json parses", true);

	// 1. Every panel key the shared corpus names is in the shipped catalog.
	FJsonValuePtr Root = LoadJson(CorpusDir, "registry-cases.json");
	if (Root) {
		const FJsonValue* Keys = Root->Find("panel_keys");
		bool bAll = Keys && Keys->IsArray() && Keys->Size() > 0;
		std::string Missing;
		if (Keys && Keys->IsArray()) {
			for (std::size_t i = 0; i < Keys->Size(); ++i) {
				const std::string K = Keys->ArrayItems[i]->AsString();
				if (Catalog.Find(K) == nullptr) {
					bAll = false;
					Missing = K;
					break;
				}
			}
		}
		Report("catalog: covers every corpus panel_key", bAll,
			Missing.empty() ? "" : ("no catalog row for '" + Missing + "'"));
	}

	// 2. The shipped catalog and the built-in fallback map are the same map. Two
	//    sources for one fact is how they drift; this is the check that they cannot.
	{
		const std::vector<std::pair<std::string, std::string>> Fallback =
			FInsimulUIRegistryModel::DefaultPanelMap();
		bool bSame = Fallback.size() == Catalog.Entries().size();
		std::string Detail;
		for (const auto& Pair : Fallback) {
			const FInsimulPanelEntry* Row = Catalog.Find(Pair.first);
			if (!Row) {
				bSame = false;
				Detail = "the fallback map has '" + Pair.first + "' and the catalog does not";
				break;
			}
			if (Row->Widget != Pair.second) {
				bSame = false;
				Detail = "'" + Pair.first + "' points at two different widgets";
				break;
			}
		}
		Report("catalog: agrees with the built-in fallback panel map", bSame, Detail);
	}

	// 3. Every row binds a widget.
	{
		std::string Empty;
		for (const FInsimulPanelEntry& Row : Catalog.Entries()) {
			if (Row.Widget.empty()) {
				Empty = Row.Key;
				break;
			}
		}
		Report("catalog: every row binds a widget", Empty.empty(),
			Empty.empty() ? "" : ("'" + Empty + "' binds nothing"));
	}

	// 4. Every module a row names is one core's activation table knows.
	FInsimulActivationTable Table;
	if (LoadTable(DataDir, Table)) {
		const std::string Unknown = UnknownModuleIn(Catalog, Table);
		Report("catalog: every owning module is one the activation table names", Unknown.empty(),
			Unknown.empty() ? "" : ("no module '" + Unknown + "' in the table"));
		// A catalog that owns nothing could not gate anything, so the gate checks
		// below would pass vacuously.
		Report("catalog: the catalog gates at least one panel", !Catalog.Modules().empty());
	}
}

void RunModuleGateCases(const std::string& DataDir) {
	FInsimulUIPanelCatalog Catalog;
	FInsimulActivationTable Table;
	if (!LoadCatalog(DataDir, Catalog) || !LoadTable(DataDir, Table)) {
		return;
	}

	std::string OwnerGenre;
	std::string EmptyGenre;
	PickGenres(Catalog, Table, OwnerGenre, EmptyGenre);
	if (OwnerGenre.empty() || EmptyGenre.empty()) {
		Report("gate: the table has both an owning and a module-less genre", false,
			"owner='" + OwnerGenre + "' empty='" + EmptyGenre + "'");
		return;
	}

	const std::vector<std::string> AllKeys = Catalog.Keys();

	// A genre that activates the owning modules shows every panel.
	{
		FInsimulUIPanelResolver Resolver(Catalog);
		Resolver.SetActiveModules(
			Table.Resolve(OwnerGenre, std::vector<std::string>(), EInsimulGenreSource::WorldIr));
		Report("gate: a genre that activates the owning modules withholds nothing",
			Resolver.IsGated() && Resolver.GatedKeys().empty() &&
				Resolver.AvailableKeys().size() == AllKeys.size(),
			OwnerGenre);
	}

	// A KNOWN genre that selected none of them withholds exactly those panels.
	std::string GatedKey;
	{
		FInsimulUIPanelResolver Resolver(Catalog);
		Resolver.SetActiveModules(
			Table.Resolve(EmptyGenre, std::vector<std::string>(), EInsimulGenreSource::WorldIr));

		bool bExact = true;
		std::string Detail;
		for (const FInsimulPanelEntry& Row : Catalog.Entries()) {
			const FInsimulPanelResolution R = Resolver.Peek(Row.Key);
			const bool bShouldGate = !Row.Module.empty();
			if (bShouldGate && R.Outcome != EInsimulPanelOutcome::Gated) {
				bExact = false;
				Detail = "'" + Row.Key + "' should be withheld and is not";
				break;
			}
			if (!bShouldGate && !R.IsAvailable()) {
				bExact = false;
				Detail = "'" + Row.Key + "' should be available and is not";
				break;
			}
			if (bShouldGate && GatedKey.empty()) {
				GatedKey = Row.Key;
			}
		}
		Report("gate: a genre with none of the owning modules withholds exactly those panels",
			bExact, Detail);
		Report("gate: available + withheld partition the catalog",
			Resolver.AvailableKeys().size() + Resolver.GatedKeys().size() == AllKeys.size());
		Report("gate: the report names what was withheld",
			Resolver.Describe().find("withheld") != std::string::npos);

		// The refusal is DIAGNOSED, never a silent empty panel.
		const FInsimulPanelResolution R = Resolver.Resolve(GatedKey);
		bool bDiagnosed = false;
		for (const FUIRegistryDiagnostic& D : Resolver.Diagnostics()) {
			if (D.Kind == "inactive_module" && D.Key == GatedKey && !D.Message.empty()) {
				bDiagnosed = true;
			}
		}
		Report("gate: a withheld panel resolves to nothing WITH a diagnostic",
			R.Outcome == EInsimulPanelOutcome::Gated && R.Widget.empty() && !R.Detail.empty() &&
				bDiagnosed);
	}

	// An UNKNOWN genre is not a free pass: it withholds every module-owned panel,
	// the same refusal the pack consult makes.
	{
		FInsimulUIPanelResolver Resolver(Catalog);
		const FInsimulActiveModuleSet Set = Table.Resolve(
			"a-genre-core-has-never-heard-of", std::vector<std::string>(), EInsimulGenreSource::WorldIr);
		Resolver.SetActiveModules(Set);
		std::size_t Owned = 0;
		for (const FInsimulPanelEntry& Row : Catalog.Entries()) {
			if (!Row.Module.empty()) {
				++Owned;
			}
		}
		Report("gate: an unknown genre inherits no module-owned panel",
			!Set.bKnown && Owned > 0 && Resolver.GatedKeys().size() == Owned &&
				Resolver.Peek(GatedKey).Outcome == EInsimulPanelOutcome::Gated);
	}

	// An UNDECLARED genre activates every pack, so it withholds no panel either —
	// the two answers must not disagree.
	{
		FInsimulUIPanelResolver Resolver(Catalog);
		Resolver.SetActiveModules(
			Table.Resolve(std::string(), std::vector<std::string>(), EInsimulGenreSource::Undeclared));
		Report("gate: an undeclared genre is UNGATED, as its pack consult is",
			!Resolver.IsGated() && Resolver.GatedKeys().empty() &&
				Resolver.AvailableKeys().size() == AllKeys.size());
	}

	// Nothing resolved yet is the same state, and says so.
	{
		FInsimulUIPanelResolver Resolver(Catalog);
		Report("gate: a resolver with no module set applied shows every panel",
			!Resolver.IsGated() && Resolver.AvailableKeys().size() == AllKeys.size() &&
				Resolver.Describe().find("UNGATED") != std::string::npos);
	}

	// The override layer: it wins for a panel this world has …
	{
		FInsimulUIPanelResolver Resolver(Catalog);
		Resolver.SetActiveModules(
			Table.Resolve(OwnerGenre, std::vector<std::string>(), EInsimulGenreSource::WorldIr));
		const std::string Key = AllKeys.front();
		Resolver.Override(Key, "/Game/Custom/WBP_Creator.WBP_Creator_C");
		const FInsimulPanelResolution R = Resolver.Resolve(Key);
		Report("gate: an overridden panel key resolves to the override",
			R.Outcome == EInsimulPanelOutcome::Overridden &&
				R.Widget == "/Game/Custom/WBP_Creator.WBP_Creator_C");
	}

	// … and it does NOT ungate one this world does not have. Swapping a widget says
	// nothing about which modules the genre bundle selected.
	{
		FInsimulUIPanelResolver Resolver(Catalog);
		Resolver.SetActiveModules(
			Table.Resolve(EmptyGenre, std::vector<std::string>(), EInsimulGenreSource::WorldIr));
		Resolver.Override(GatedKey, "/Game/Custom/WBP_Creator.WBP_Creator_C");
		const FInsimulPanelResolution R = Resolver.Resolve(GatedKey);
		Report("gate: an override cannot ungate a withheld panel",
			R.Outcome == EInsimulPanelOutcome::Gated && R.Widget.empty());
	}

	// An unknown key is still an unknown key, through the gate.
	{
		FInsimulUIPanelResolver Resolver(Catalog);
		const FInsimulPanelResolution R = Resolver.Resolve("no_such_panel");
		bool bDiagnosed = false;
		for (const FUIRegistryDiagnostic& D : Resolver.Diagnostics()) {
			if (D.Kind == "missing_panel" && D.Key == "no_such_panel") {
				bDiagnosed = true;
			}
		}
		Report("gate: an unknown panel key is unknown AND diagnosed",
			R.Outcome == EInsimulPanelOutcome::Unknown && R.Widget.empty() && !R.Detail.empty() &&
				bDiagnosed);
	}
}

// ── Negative controls ────────────────────────────────────────────────────────
//
// A check that cannot fail is a decoration. Each trial below mutates one thing and
// asserts the corresponding check above goes red.
void RunNegativeControls(const std::string& DataDir) {
	FInsimulUIPanelCatalog Ignored;
	std::string Error;

	Report("control: a document with no panels array is refused",
		!FInsimulUIPanelCatalog::Parse("{\"version\":1}", Ignored, Error) && !Error.empty());
	Report("control: a duplicate panel key is refused",
		!FInsimulUIPanelCatalog::Parse(
			"{\"panels\":[{\"key\":\"hud\",\"widget\":\"a\"},{\"key\":\"hud\",\"widget\":\"b\"}]}",
			Ignored, Error));
	Report("control: a row with no key is refused",
		!FInsimulUIPanelCatalog::Parse("{\"panels\":[{\"widget\":\"a\"}]}", Ignored, Error));
	Report("control: text that is not JSON is refused",
		!FInsimulUIPanelCatalog::Parse("not json at all", Ignored, Error));

	FInsimulActivationTable Table;
	if (!LoadTable(DataDir, Table)) {
		return;
	}

	// A catalog whose panel names a module the table does not know must be caught.
	FInsimulUIPanelCatalog Mutated;
	const bool bParsed = FInsimulUIPanelCatalog::Parse(
		"{\"panels\":[{\"key\":\"hud\",\"widget\":\"w\",\"module\":\"a-module-core-never-emitted\"}]}",
		Mutated, Error);
	Report("control: a catalog naming an unknown module fails the table check",
		bParsed && !UnknownModuleIn(Mutated, Table).empty());

	// And the gate itself is not vacuous: the SAME panel resolves once its module is
	// active, so "withheld" is a measurement rather than a constant.
	FInsimulUIPanelCatalog Owned;
	const std::vector<std::string> Known = TableModuleIds(Table);
	if (bParsed && !Known.empty() &&
		FInsimulUIPanelCatalog::Parse(
			"{\"panels\":[{\"key\":\"hud\",\"widget\":\"w\",\"module\":\"" + Known.front() + "\"}]}",
			Owned, Error)) {
		FInsimulUIPanelResolver On(Owned);
		On.SetActiveModuleIds({Known.front()});
		FInsimulUIPanelResolver Off(Owned);
		Off.SetActiveModuleIds({});
		Report("control: the same panel resolves with its module on and is withheld with it off",
			On.Peek("hud").IsAvailable() && Off.Peek("hud").Outcome == EInsimulPanelOutcome::Gated);
	}
}

} // namespace

int main(int argc, char** argv) {
#ifdef INSIMUL_UI_DIR
	const char* DefaultCorpus = INSIMUL_UI_DIR;
#else
	const char* DefaultCorpus = "../../../../packages/core/conformance/ui";
#endif
#ifdef INSIMUL_ACTIVATION_DATA_DIR
	const char* DefaultData = INSIMUL_ACTIVATION_DATA_DIR;
#else
	const char* DefaultData = "../../../templates/project/Content/Data/insimul";
#endif
	std::string Dir = argc > 1 ? argv[1] : DefaultCorpus;
	std::string DataDir = argc > 2 ? argv[2] : DefaultData;

	std::printf("== Insimul default-UI host tests (US-XU1 + tasklist 190 US-1) ==\n");
	std::printf("   corpus dir: %s\n", Dir.c_str());
	std::printf("   data dir:   %s\n", DataDir.c_str());

	RunRegistryCases(Dir);
	RunDefaultMapChecks(Dir);
	RunLoadingCases(Dir);
	RunThemeCases(Dir);
	RunPanelCatalogCases(Dir, DataDir);
	RunModuleGateCases(DataDir);
	RunNegativeControls(DataDir);

	std::printf("\n  %d passed, %d failed\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
