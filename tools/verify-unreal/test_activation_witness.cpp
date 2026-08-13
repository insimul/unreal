// test_activation_witness.cpp — an inactive module contributes NOTHING, witnessed in
// a real KB (US-3 of tasklist 146, RUNTIME_CORE_ADOPTION.md §14).
//
// WHY A KB IS THE ONLY WITNESS. US-3's second criterion is that an inactive module
// contributes nothing — "no consulted rule pack and no registered system" (core's
// module contract §7.3). The registration half is portable and ctest
// `module_activation` proves it. The PACK half cannot be proved by reading code or
// counting files: "was this pack consulted" is a question about a knowledge base, and
// the only honest answer comes from asking one. So this leg consults each genre's
// active packs into a real libinsimul, through the plugin's own insimul::InsimulKB
// and its own ConsultActivePacks — not a second reader — and then asks, for EVERY
// pack the build carries, whether that pack's own signature predicate is there.
// Present exactly when active, for every genre. `current_predicate(<a stamina
// predicate>)` has no solutions in an rpg world.
//
// THE SIGNATURE PREDICATES ARE DERIVED, NOT LISTED. A hand-written probe per pack
// would be the hardcoded list this story exists to delete, and would rot the day core
// renames a predicate. DerivePackProbes() reads the vendored pack texts and picks,
// per pack, a predicate no other pack defines or even declares — uniqueness is what
// makes `current_predicate/1` a statement about ONE pack having been consulted.
//
// THE SCENE, EXECUTED. The sample scenario
// (Content/Data/insimul/scenarios/dark-courtyard.json) is the script the playable
// scene reads. Here it runs through the SAME reader, the SAME runner and the SAME
// library, against the packs its genre activates — so "the scene exercises two
// mechanics end to end" is a checked statement rather than a screenshot. What the
// gate cannot do is move an actor; VERIFICATION.md US-M2 is that pass.
//
// Registered ONLY when a libinsimul is found (the same discovery the bridge legs
// use). Its absence is a configure-time WARNING, and -DINSIMUL_REQUIRE_BINARIES=ON
// turns it into a hard failure — this harness's --require-binaries.

#include "InsimulKB.h"
#include "InsimulMechanicScenario.h"
#include "InsimulModuleActivation.h"
#include "InsimulModulePacks.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef INSIMUL_ACTIVATION_DATA_DIR
#error "INSIMUL_ACTIVATION_DATA_DIR must point at the shipped Content/Data/insimul directory"
#endif

namespace {

int Failures = 0;
int Checks = 0;

void Check(bool bCondition, const std::string& What) {
	Checks++;
	if (!bCondition) {
		Failures++;
		std::printf("  x %s\n", What.c_str());
	}
}

std::string DataPath(const std::string& Relative) {
	return std::string(INSIMUL_ACTIVATION_DATA_DIR) + "/" + Relative;
}

bool ReadFile(const std::string& Path, std::string& OutText) {
	std::ifstream In(Path.c_str(), std::ios::binary);
	if (!In) {
		return false;
	}
	std::ostringstream Buffer;
	Buffer << In.rdbuf();
	OutText = Buffer.str();
	return true;
}

bool Contains(const std::vector<std::string>& Names, const std::string& Name) {
	for (const std::string& N : Names) {
		if (N == Name) {
			return true;
		}
	}
	return false;
}

std::string Trim(const std::string& S) {
	std::size_t Begin = 0;
	while (Begin < S.size() && (S[Begin] == ' ' || S[Begin] == '\t' || S[Begin] == '\r')) {
		Begin++;
	}
	std::size_t End = S.size();
	while (End > Begin && (S[End - 1] == ' ' || S[End - 1] == '\t' || S[End - 1] == '\r')) {
		End--;
	}
	return S.substr(Begin, End - Begin);
}

// ── deriving one signature predicate per pack ───────────────────────────────

/** Every `name/arity` a pack DEFINES (a clause head) and every one it merely DECLARES
 *  (`:- dynamic`). The distinction matters below: current_predicate/1 succeeds for a
 *  dynamic declaration too, so a probe must be a predicate no other pack declares
 *  either. */
struct FPackPredicates {
	std::vector<std::string> Defined;
	std::vector<std::string> Declared;
};

bool IsLowerAlpha(char C) { return C >= 'a' && C <= 'z'; }
bool IsAtomChar(char C) {
	return (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9') || C == '_';
}

/** Count top-level arguments of a term whose '(' sits at Open - 1. 0 when the head
 *  does not close on this line — not a probe candidate. */
int ArityOf(const std::string& Line, std::size_t Open) {
	int Depth = 1;
	int Args = 1;
	char Quote = 0;
	for (std::size_t At = Open; At < Line.size(); ++At) {
		const char C = Line[At];
		if (Quote != 0) {
			if (C == Quote && (At == 0 || Line[At - 1] != '\\')) {
				Quote = 0;
			}
			continue;
		}
		if (C == '\'' || C == '"') {
			Quote = C;
			continue;
		}
		if (C == '(' || C == '[') {
			Depth++;
		} else if (C == ')' || C == ']') {
			if (--Depth == 0) {
				return Args;
			}
		} else if (C == ',' && Depth == 1) {
			Args++;
		}
	}
	return 0;
}

FPackPredicates ScanPack(const std::string& Text) {
	FPackPredicates Out;
	std::istringstream In(Text);
	std::string Raw;
	while (std::getline(In, Raw)) {
		const std::string Line = Trim(Raw);
		if (Line.empty() || Line[0] == '%') {
			continue;
		}
		if (Line.compare(0, 2, ":-") == 0) {
			// `:- dynamic(name/arity)` — a declaration, not a definition.
			const std::size_t At = Line.find("dynamic");
			if (At == std::string::npos) {
				continue;
			}
			std::size_t Open = Line.find('(', At);
			if (Open == std::string::npos) {
				continue;
			}
			std::size_t Cursor = Open + 1;
			while (Cursor < Line.size() && Line[Cursor] == ' ') {
				Cursor++;
			}
			const std::size_t NameStart = Cursor;
			while (Cursor < Line.size() && IsAtomChar(Line[Cursor])) {
				Cursor++;
			}
			const std::string Name = Line.substr(NameStart, Cursor - NameStart);
			while (Cursor < Line.size() && (Line[Cursor] == ' ')) {
				Cursor++;
			}
			if (Name.empty() || Cursor >= Line.size() || Line[Cursor] != '/') {
				continue;
			}
			Cursor++;
			while (Cursor < Line.size() && Line[Cursor] == ' ') {
				Cursor++;
			}
			const std::size_t ArityStart = Cursor;
			while (Cursor < Line.size() && Line[Cursor] >= '0' && Line[Cursor] <= '9') {
				Cursor++;
			}
			if (Cursor == ArityStart) {
				continue;
			}
			const std::string Signature = Name + "/" + Line.substr(ArityStart, Cursor - ArityStart);
			if (!Contains(Out.Declared, Signature)) {
				Out.Declared.push_back(Signature);
			}
			continue;
		}
		// A clause head: `name(` at column 0 of a trimmed line.
		if (!IsLowerAlpha(Line[0])) {
			continue;
		}
		std::size_t Cursor = 0;
		while (Cursor < Line.size() && IsAtomChar(Line[Cursor])) {
			Cursor++;
		}
		if (Cursor >= Line.size() || Line[Cursor] != '(') {
			continue;
		}
		const std::string Name = Line.substr(0, Cursor);
		const int Arity = ArityOf(Line, Cursor + 1);
		if (Arity <= 0) {
			continue;
		}
		const std::string Signature = Name + "/" + std::to_string(Arity);
		if (!Contains(Out.Defined, Signature)) {
			Out.Defined.push_back(Signature);
		}
	}
	return Out;
}

/**
 * One predicate per pack that NO other pack defines or declares. That uniqueness is
 * what makes `current_predicate(P)` a statement about one pack having been consulted;
 * a shared predicate would answer for whichever pack happened to be in.
 *
 * A DEFINED predicate (a clause head) is preferred, and a DECLARED one (`:- dynamic`)
 * is accepted when a pack has no unique head — the shared-vocabulary pack is
 * declarations and nothing else on purpose, and it is still consulted or not
 * consulted like any other pack.
 */
std::vector<std::pair<std::string, std::string>> DerivePackProbes(
	const std::vector<std::pair<std::string, std::string>>& Packs) {
	std::vector<std::pair<std::string, FPackPredicates>> Scanned;
	for (const auto& Pack : Packs) {
		Scanned.emplace_back(Pack.first, ScanPack(Pack.second));
	}

	std::vector<std::pair<std::string, std::string>> Probes;
	for (const auto& Own : Scanned) {
		auto IsUnique = [&Scanned, &Own](const std::string& Signature) {
			for (const auto& Other : Scanned) {
				if (Other.first == Own.first) {
					continue;
				}
				if (Contains(Other.second.Defined, Signature) || Contains(Other.second.Declared, Signature)) {
					return false;
				}
			}
			return true;
		};

		std::string Pick;
		for (const std::string& Signature : Own.second.Defined) {
			if (IsUnique(Signature)) {
				Pick = Signature;
				break;
			}
		}
		if (Pick.empty()) {
			for (const std::string& Signature : Own.second.Declared) {
				if (IsUnique(Signature)) {
					Pick = Signature;
					break;
				}
			}
		}
		Check(!Pick.empty(),
			"pack '" + Own.first + "' has a predicate no other pack declares — it can be witnessed");
		if (!Pick.empty()) {
			Probes.emplace_back(Own.first, Pick);
		}
	}
	return Probes;
}

// ── asking the KB ───────────────────────────────────────────────────────────

enum class EAnswer {
	Holds,
	DoesNotHold,
	Raised,
};

/** Ask a ground question. RAISED is a third answer and is never folded into "no" —
 *  §14.3 is entirely about that distinction. */
EAnswer AskGoal(insimul::InsimulKB& KB, const std::string& Goal, std::string& OutError) {
	OutError.clear();
	insimul::PrologQuery Query = KB.StartQuery(Goal);
	if (!Query.IsValid()) {
		OutError = KB.LastError();
		return EAnswer::Raised;
	}
	insimul::PrologBinding Binding;
	long long Solutions = 0;
	while (Query.Next(Binding)) {
		Solutions++;
	}
	// A goal that raises MID-enumeration leaves an error behind; the count alone
	// would read as a plain failure.
	const std::string Error = KB.LastError();
	if (Solutions == 0 && !Error.empty()) {
		OutError = Error;
		return EAnswer::Raised;
	}
	return Solutions > 0 ? EAnswer::Holds : EAnswer::DoesNotHold;
}

/** The scenario runner's KB, over the plugin's own wrapper. */
class FLiveKb : public insimul::IInsimulScenarioKb {
public:
	explicit FLiveKb(insimul::InsimulKB& InKB) : KB(InKB) {}

	bool Assert(const std::string& Clause, std::string& OutError) override {
		if (KB.Assert(Clause)) {
			return true;
		}
		OutError = KB.LastError();
		return false;
	}

	bool Retract(const std::string& Clause, std::string& OutError) override {
		// A clause that matched nothing is NOT an error (the runner's contract).
		if (KB.Retract(Clause) == insimul::InsimulKB::RetractResult::Error) {
			OutError = KB.LastError();
			return false;
		}
		return true;
	}

	bool Ask(const std::string& Goal, bool& bOutHolds, std::string& OutError) override {
		const EAnswer Answer = AskGoal(KB, Goal, OutError);
		if (Answer == EAnswer::Raised) {
			return false;
		}
		bOutHolds = Answer == EAnswer::Holds;
		return true;
	}

private:
	insimul::InsimulKB& KB;
};

/** Consult one genre's active packs into a fresh KB. */
bool ConsultFor(
	insimul::InsimulKB& KB,
	const insimul::FInsimulActiveModuleSet& Set,
	const insimul::FInsimulPredicatePackManifest& Manifest,
	insimul::IInsimulPredicatePackSource& Source,
	insimul::FInsimulPackConsultReport& OutReport) {
	OutReport = insimul::ConsultActivePacks(
		&Set, Manifest, &Source,
		[&KB](const std::string& Text, std::string& OutError) {
			if (KB.Consult(Text)) {
				return true;
			}
			OutError = KB.LastError();
			return false;
		});
	return OutReport.IsOk();
}

} // namespace

int main() {
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	std::printf("activation witness — an inactive module contributes nothing, asked of a real KB\n");
	std::printf("  library: %s\n", insimul::InsimulKB::Version().c_str());
	std::printf("  data:    %s\n", INSIMUL_ACTIVATION_DATA_DIR);

	std::string TableJson;
	std::string ManifestJson;
	std::string ScenarioJson;
	if (!ReadFile(DataPath("modules/genre-activation.json"), TableJson) ||
		!ReadFile(DataPath("packs/PACKS.json"), ManifestJson) ||
		!ReadFile(DataPath("scenarios/dark-courtyard.json"), ScenarioJson)) {
		std::printf("FAILED: the shipped activation data is not where a build reads it\n");
		return 1;
	}

	insimul::FInsimulActivationTable Table;
	insimul::FInsimulPredicatePackManifest Manifest;
	insimul::FInsimulMechanicScenario Scenario;
	std::string Error;
	if (!insimul::FInsimulActivationTable::Parse(TableJson, Table, Error) ||
		!insimul::FInsimulPredicatePackManifest::Parse(ManifestJson, Manifest, Error) ||
		!insimul::FInsimulMechanicScenario::Parse(ScenarioJson, Scenario, Error)) {
		std::printf("FAILED: %s\n", Error.c_str());
		return 1;
	}

	insimul::FInsimulDirectoryPackSource Source(DataPath("packs"), &Manifest);
	const std::vector<std::string>& Universe = Manifest.ConsultOrder();

	// The probes, derived from the pack texts themselves.
	std::vector<std::pair<std::string, std::string>> Texts;
	for (const std::string& Area : Universe) {
		std::string Text;
		if (!Source.Read(Area, Text)) {
			std::printf("FAILED: pack '%s': %s\n", Area.c_str(), Source.LastError().c_str());
			return 1;
		}
		Texts.emplace_back(Area, Text);
	}
	const std::vector<std::pair<std::string, std::string>> Probes = DerivePackProbes(Texts);
	if (Probes.size() != Universe.size()) {
		std::printf("FAILED: %zu probe(s) for %zu pack(s)\n", Probes.size(), Universe.size());
		return 1;
	}
	for (const auto& Probe : Probes) {
		std::printf("  probe %-14s %s\n", Probe.first.c_str(), Probe.second.c_str());
	}

	// ── THE WITNESS: present exactly when active, for every genre ─────────────
	int Witnessed = 0;
	for (const std::string& Genre : Table.Genres()) {
		const insimul::FInsimulActiveModuleSet Set =
			Table.Resolve(Genre, Universe, insimul::EInsimulGenreSource::WorldIr);

		insimul::InsimulKB KB;
		if (!KB.IsValid()) {
			Check(false, "genre '" + Genre + "': insimul_kb_create returned NULL");
			continue;
		}
		insimul::FInsimulPackConsultReport Report;
		if (!ConsultFor(KB, Set, Manifest, Source, Report)) {
			Check(false, "genre '" + Genre + "': " + Report.Describe());
			continue;
		}
		Check(Report.Consulted().size() == Set.PredicatePacks.size(),
			"genre '" + Genre + "': exactly its active packs went into the KB");

		for (const auto& Probe : Probes) {
			const bool bShouldBeThere = Set.IsPackActive(Probe.first);
			std::string AskError;
			const EAnswer Answer = AskGoal(KB, "current_predicate(" + Probe.second + ")", AskError);
			if (Answer == EAnswer::Raised) {
				Check(false, "genre '" + Genre + "': witnessing '" + Probe.first + "' raised — " + AskError);
				continue;
			}
			const bool bIsThere = Answer == EAnswer::Holds;
			if (bShouldBeThere) {
				Check(bIsThere,
					"genre '" + Genre + "': pack '" + Probe.first + "' is ACTIVE and " + Probe.second +
						" is in the KB");
			} else {
				Check(!bIsThere,
					"genre '" + Genre + "': pack '" + Probe.first + "' is INACTIVE and " + Probe.second +
						" is NOT in the KB — an inactive module contributes nothing (core module contract §7.3)");
			}
		}
		Witnessed++;
	}
	std::printf("  witnessed %d genre(s) x %zu pack(s)\n", Witnessed, Universe.size());
	Check(Witnessed == static_cast<int>(Table.Genres().size()), "every genre in the table was witnessed");

	// ── THE SCENE, EXECUTED ───────────────────────────────────────────────────
	{
		const insimul::FInsimulActiveModuleSet Set =
			Table.Resolve(Scenario.Genre, Universe, insimul::EInsimulGenreSource::WorldIr);
		insimul::InsimulKB KB;
		insimul::FInsimulPackConsultReport Report;
		if (!KB.IsValid() || !ConsultFor(KB, Set, Manifest, Source, Report)) {
			Check(false, "the scenario's genre consults: " + Report.Describe());
		} else {
			FLiveKb Live(KB);
			const insimul::FInsimulScenarioReport Run = insimul::RunScenario(Scenario, Live);
			std::printf("  %s\n", Run.Describe().c_str());
			Check(Run.IsOk(), "every step of '" + Scenario.Id + "' answers what the scene acts on");
			Check(Run.MechanicsExercised().size() >= 2,
				"the scene exercises at least two adopted mechanics end to end [" +
					insimul::JoinNames(Run.MechanicsExercised()) + "]");
			Check(Run.Steps.size() == Scenario.Steps.size(), "every step ran");
		}
	}

	// ── THE PINNED CROSS-MODULE MEASUREMENT (§14.3) ──────────────────────────
	// An authored requirement may name ANOTHER module's vocabulary — core's own
	// traversal pack documents `traversal_requires(camp, ford, has_item(...))`. Under
	// a genre that activates traversal and NOT the module that owns the goal, that
	// goal's predicate is not in the KB at all. This measures what the engine then
	// does, and PINS the answer: a scene must never show a player a refusal core
	// never decided, so the runner has to keep Raised apart from "did not hold". If
	// core (or trealla) changes this, the check fails and the finding gets closed
	// rather than forgotten.
	{
		std::string CrossGenre;
		for (const std::string& Genre : Table.Genres()) {
			const insimul::FInsimulActiveModuleSet Set =
				Table.Resolve(Genre, Universe, insimul::EInsimulGenreSource::WorldIr);
			if (Set.IsPackActive("traversal") && !Set.IsPackActive("skill")) {
				CrossGenre = Genre;
				break;
			}
		}
		if (CrossGenre.empty()) {
			std::printf("  .  no genre activates traversal without skill any more — §14.3 cannot be measured here\n");
		} else {
			const insimul::FInsimulActiveModuleSet Set =
				Table.Resolve(CrossGenre, Universe, insimul::EInsimulGenreSource::WorldIr);
			insimul::InsimulKB KB;
			insimul::FInsimulPackConsultReport Report;
			if (!KB.IsValid() || !ConsultFor(KB, Set, Manifest, Source, Report)) {
				Check(false, "genre '" + CrossGenre + "' consults: " + Report.Describe());
			} else {
				KB.Assert("traversal_link(courtyard, rooftop, climb)");
				KB.Assert("traversal_requires(courtyard, rooftop, has_skill(Actor, climbing, 2))");
				KB.Assert("movement_mode(player, climb)");
				std::string AskError;
				const EAnswer Answer = AskGoal(KB, "can_traverse(player, courtyard, rooftop)", AskError);
				std::printf("  §14.3 under genre '%s': %s%s%s\n", CrossGenre.c_str(),
					Answer == EAnswer::Raised ? "RAISED " : (Answer == EAnswer::Holds ? "held" : "did not hold"),
					Answer == EAnswer::Raised ? "— " : "", AskError.c_str());
				Check(Answer == EAnswer::Raised,
					"§14.3 still holds: under genre '" + CrossGenre + "' an authored requirement naming an "
					"INACTIVE module's vocabulary RAISES rather than failing. If it no longer does, close the "
					"finding in RUNTIME_CORE_ADOPTION.md §14.3 and delete this check.");
			}
		}
	}

	if (Failures > 0) {
		std::printf("FAILED: %d of %d check(s)\n", Failures, Checks);
		return 1;
	}
	std::printf("OK: %d check(s)\n", Checks);
	return 0;
}
