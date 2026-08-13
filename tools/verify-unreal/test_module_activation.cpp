// test_module_activation.cpp — modules activated from the genre bundle, executed
// (US-3 of tasklist 146, RUNTIME_CORE_ADOPTION.md §14).
//
// WHAT IT COVERS. The portable half of the activation: the resolver
// (InsimulModuleActivation), the pack consult (InsimulModulePacks), the host
// restriction (FInsimulMechanicHosts::RestrictTo) and the scenario reader/runner
// (InsimulMechanicScenario) — driven over the REAL shipped data
// (templates/project/Content/Data/insimul/), under plain clang++ with no Unreal and
// no native library. Every outcome each of them can produce is reached here,
// including the ones a healthy build never takes: a missing pack, a refused pack, a
// silent host, a raised goal.
//
// WHAT IT DOES NOT COVER. Whether a consulted pack actually put its predicates in a
// KB — no KB exists in this leg. That is ctest `activation_witness`, which drives
// the same three files over the real libinsimul. And the Unreal glue
// (templates/source/mechanics/InsimulMechanicSampleScene.*) needs an engine and a
// level; VERIFICATION.md US-M2 is its pass.
//
// THE DATA IS THE POINT. This test reads the files an exported game reads, so a
// re-vendored table with a genre whose module rows and declared packs disagree fails
// here rather than at someone's boot.

#include "InsimulMechanicHosts.h"
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

// ── the shipped data, read once ─────────────────────────────────────────────

struct FShippedData {
	insimul::FInsimulActivationTable Table;
	insimul::FInsimulPredicatePackManifest Manifest;
	bool bOk = false;
};

FShippedData LoadShippedData() {
	FShippedData Data;
	std::string TableJson;
	std::string ManifestJson;
	if (!ReadFile(DataPath("modules/genre-activation.json"), TableJson)) {
		Check(false, "the shipped activation table exists at Content/Data/insimul/modules/genre-activation.json");
		return Data;
	}
	if (!ReadFile(DataPath("packs/PACKS.json"), ManifestJson)) {
		Check(false, "the shipped pack manifest exists at Content/Data/insimul/packs/PACKS.json");
		return Data;
	}
	std::string Error;
	if (!insimul::FInsimulActivationTable::Parse(TableJson, Data.Table, Error)) {
		Check(false, "the shipped activation table parses: " + Error);
		return Data;
	}
	if (!insimul::FInsimulPredicatePackManifest::Parse(ManifestJson, Data.Manifest, Error)) {
		Check(false, "the shipped pack manifest parses: " + Error);
		return Data;
	}
	Data.bOk = true;
	return Data;
}

// ── 1. the resolver, over the real table ────────────────────────────────────

void TestTheResolution(const FShippedData& Data) {
	const std::vector<std::string>& Universe = Data.Manifest.ConsultOrder();
	Check(!Universe.empty(), "the pack manifest carries a consult order");
	Check(Data.Table.Genres().size() >= 2, "the activation table knows more than one genre");

	// Core's three answers, kept apart. None of them is named here — the table is
	// asked which genres it knows and the first with modules is used.
	std::string WithModules;
	std::string WithoutModules;
	for (const std::string& Genre : Data.Table.Genres()) {
		const insimul::FInsimulActiveModuleSet Set =
			Data.Table.Resolve(Genre, Universe, insimul::EInsimulGenreSource::WorldIr);
		Check(Set.bKnown, "genre '" + Genre + "' resolves as KNOWN");
		Check(Set.Warnings.empty(),
			"genre '" + Genre + "': the table's own pack list agrees with the one its module rows resolve to" +
				(Set.Warnings.empty() ? "" : " — " + Set.Warnings[0]));

		// The always-active packs reach every genre, module or no module.
		for (const std::string& Shared : Data.Table.AlwaysActivePacks()) {
			Check(Set.IsPackActive(Shared),
				"genre '" + Genre + "' consults the shared pack '" + Shared + "'");
		}
		// Consult ORDER is the universe's, never the table's listing order.
		std::size_t At = 0;
		bool bOrdered = true;
		for (const std::string& Area : Set.PredicatePacks) {
			bool bFound = false;
			while (At < Universe.size()) {
				if (Universe[At++] == Area) {
					bFound = true;
					break;
				}
			}
			bOrdered = bOrdered && bFound;
		}
		Check(bOrdered, "genre '" + Genre + "': the active packs are in the manifest's consult order");

		// Every host interface the set names comes from an active module, and every
		// active module's interfaces are in the set.
		for (const insimul::FInsimulActiveModule& Module : Set.Modules) {
			for (const std::string& Host : Module.HostInterfaces) {
				Check(Set.ActivatesHost(Host),
					"genre '" + Genre + "': module '" + Module.Id + "' names host '" + Host + "' and the set carries it");
			}
			if (!Module.PredicatePack.empty()) {
				Check(Set.IsPackActive(Module.PredicatePack),
					"genre '" + Genre + "': active module '" + Module.Id + "' has its own pack consulted");
			}
		}

		if (WithModules.empty() && !Set.Modules.empty()) {
			WithModules = Genre;
		}
		if (WithoutModules.empty() && Set.Modules.empty()) {
			WithoutModules = Genre;
		}
	}

	Check(!WithModules.empty(), "at least one genre in the table selects modules");
	if (WithModules.empty()) {
		return;
	}

	// A genre that selects NO module is a real answer and still gets the vocabulary.
	if (!WithoutModules.empty()) {
		const insimul::FInsimulActiveModuleSet Set =
			Data.Table.Resolve(WithoutModules, Universe, insimul::EInsimulGenreSource::WorldIr);
		Check(Set.PredicatePacks.size() == Data.Table.AlwaysActivePacks().size(),
			"a genre that selects no module consults exactly the shared packs");
		Check(Set.HostInterfaces.empty(), "a genre that selects no module names no host interface");
	}

	// An UNKNOWN genre: shared vocabulary and nothing else. Not the same answer as
	// "no genre declared", which is the next check.
	const insimul::FInsimulActiveModuleSet Unknown =
		Data.Table.Resolve("a-genre-core-has-never-heard-of", Universe, insimul::EInsimulGenreSource::Declared);
	Check(!Unknown.bKnown, "an unknown genre resolves as NOT known");
	Check(Unknown.Modules.empty(), "an unknown genre activates no module");
	Check(Unknown.PredicatePacks.size() == Data.Table.AlwaysActivePacks().size(),
		"an unknown genre consults the shared vocabulary and nothing else");
	Check(Unknown.Source == insimul::EInsimulGenreSource::Declared,
		"an unknown genre keeps the SOURCE it was given — who said so survives the lookup");

	// NO genre declared: every pack. The right default for a commandlet, and a
	// different answer from 'unknown'.
	const insimul::FInsimulActiveModuleSet Undeclared =
		Data.Table.Resolve("", Universe, insimul::EInsimulGenreSource::WorldIr);
	Check(Undeclared.Source == insimul::EInsimulGenreSource::Undeclared,
		"a blank genre reports Undeclared however it was asked for — a report cannot claim the IR said so");
	Check(Undeclared.PredicatePacks.size() == Universe.size(),
		"no genre declared consults EVERY pack this build carries");
	Check(!Undeclared.Modules.empty(), "no genre declared activates every module the table knows");
	Check(Undeclared.PredicatePacks.size() > Unknown.PredicatePacks.size(),
		"'no genre declared' and 'unknown genre' are DIFFERENT answers (core module contract §7.3)");
}

// ── 2. the World IR read, and the hole in it ────────────────────────────────

void TestTheGenreRead(const FShippedData& Data) {
	const std::vector<std::string>& Universe = Data.Manifest.ConsultOrder();
	const std::string Genre = Data.Table.Genres().front();

	const std::string Ir = "{\"meta\":{\"genreConfig\":{\"id\":\"" + Genre + "\",\"name\":\"x\"}},\"world\":{}}";
	Check(insimul::FInsimulActivationTable::GenreOfWorldIr(Ir) == Genre,
		"the genre is read from the World IR's meta.genreConfig.id");
	const insimul::FInsimulActiveModuleSet FromIr = Data.Table.ResolveForWorldIr(Ir, Universe);
	Check(FromIr.Source == insimul::EInsimulGenreSource::WorldIr && FromIr.bKnown,
		"resolving from a World IR reports the IR as the source");

	// A document that shortens genreConfig to the bare id is read, not rejected.
	const std::string Short = "{\"meta\":{\"genreConfig\":\"" + Genre + "\"}}";
	Check(insimul::FInsimulActivationTable::GenreOfWorldIr(Short) == Genre,
		"a genreConfig shortened to the bare id string is read");

	// THE FINDING (§14.2). A SaveFile's worldSnapshot carries no meta at all, so a
	// resumed game cannot resolve its own module set. Pinned here so that core
	// persisting the resolved set fails this check and closes the finding.
	const std::string Snapshot =
		"{\"worldSnapshot\":{\"world\":{},\"countries\":[],\"settlements\":[],\"characters\":[],"
		"\"lots\":[],\"quests\":[],\"rules\":[],\"actions\":[],\"grammars\":[]}}";
	Check(insimul::FInsimulActivationTable::GenreOfWorldIr(Snapshot).empty(),
		"§14.2 still holds: a SaveFile worldSnapshot declares no genre, so a resumed game must be TOLD one");
	const insimul::FInsimulActiveModuleSet FromSave = Data.Table.ResolveForWorldIr(Snapshot, Universe);
	Check(FromSave.Source == insimul::EInsimulGenreSource::Undeclared,
		"a resumed save resolves as Undeclared rather than silently picking a genre");

	// The documented workaround: the game states it, and the report says who did.
	const insimul::FInsimulActiveModuleSet Declared =
		Data.Table.Resolve(Genre, Universe, insimul::EInsimulGenreSource::Declared);
	Check(Declared.Source == insimul::EInsimulGenreSource::Declared && Declared.bKnown,
		"a game-declared genre resolves and is reported as declared by the game, not as read from the IR");
	Check(Declared.Describe().find("declared by the game") != std::string::npos,
		"the boot line states where the genre came from");

	// Garbage in is not a genre.
	Check(insimul::FInsimulActivationTable::GenreOfWorldIr("not json at all").empty(),
		"an unparseable IR yields no genre rather than a fabricated one");
	Check(insimul::FInsimulActivationTable::GenreOfWorldIr("{\"meta\":{}}").empty(),
		"an IR with no genreConfig yields no genre");
}

// ── 3. the pack consult ─────────────────────────────────────────────────────

/** Records what it was asked to consult; can be told to refuse. */
class FRecordingConsult {
public:
	std::vector<std::string> Consulted;
	std::string RefuseContaining;

	bool operator()(const std::string& Text, std::string& OutError) {
		Consulted.push_back(Text);
		if (!RefuseContaining.empty() && Text.find(RefuseContaining) != std::string::npos) {
			OutError = "syntax error near line 1";
			return false;
		}
		return true;
	}
};

void TestThePackConsult(const FShippedData& Data) {
	const std::vector<std::string>& Universe = Data.Manifest.ConsultOrder();
	const std::string Genre = Data.Table.Genres().front();

	// A genre with modules, over the REAL pack files.
	std::string Rich;
	for (const std::string& G : Data.Table.Genres()) {
		if (!Data.Table.Resolve(G, Universe, insimul::EInsimulGenreSource::WorldIr).Modules.empty()) {
			Rich = G;
			break;
		}
	}
	if (Rich.empty()) {
		Rich = Genre;
	}
	const insimul::FInsimulActiveModuleSet Set =
		Data.Table.Resolve(Rich, Universe, insimul::EInsimulGenreSource::WorldIr);

	insimul::FInsimulDirectoryPackSource Source(DataPath("packs"), &Data.Manifest);
	FRecordingConsult Consult;
	insimul::FInsimulPackConsultReport Report = insimul::ConsultActivePacks(
		&Set, Data.Manifest, &Source,
		[&Consult](const std::string& Text, std::string& OutError) { return Consult(Text, OutError); });

	Check(Report.IsOk(), "every ACTIVE pack of genre '" + Rich + "' was found and consulted");
	Check(Report.Consulted().size() == Set.PredicatePacks.size(),
		"exactly the active packs were consulted, no more");
	Check(Report.Results.size() == Universe.size(),
		"every pack the build carries has a REPORTED fate — an unmentioned pack is the silent case this replaces");
	Check(!Report.Skipped().empty(),
		"the packs this genre did not activate are named as skipped rather than omitted");
	for (const std::string& Area : Report.Skipped()) {
		Check(!Set.IsPackActive(Area), "a skipped pack '" + Area + "' is genuinely inactive for this genre");
	}
	Check(Consult.Consulted.size() == Set.PredicatePacks.size(),
		"the KB was handed one program per active pack and nothing else");
	Check(Report.Describe().find("NOT consulted") != std::string::npos,
		"the consult report says out loud what it did not consult");

	// A NULL set activates nothing. That is the "no activation resolved" path and it
	// must not fall back to consulting everything.
	FRecordingConsult NoneConsult;
	insimul::FInsimulPackConsultReport NoneReport = insimul::ConsultActivePacks(
		nullptr, Data.Manifest, &Source,
		[&NoneConsult](const std::string& Text, std::string& OutError) { return NoneConsult(Text, OutError); });
	Check(NoneConsult.Consulted.empty(), "an unresolved activation consults NOTHING rather than everything");
	Check(NoneReport.Skipped().size() == Universe.size(), "and every pack is reported as not activated");

	// A pack the source cannot supply is MISSING — reported, and the report is not ok.
	insimul::FInsimulMemoryPackSource Empty({}, "an empty source");
	FRecordingConsult Unused;
	insimul::FInsimulPackConsultReport MissingReport = insimul::ConsultActivePacks(
		&Set, Data.Manifest, &Empty,
		[&Unused](const std::string& Text, std::string& OutError) { return Unused(Text, OutError); });
	Check(!MissingReport.IsOk() && !MissingReport.Missing().empty(),
		"an active pack with no text is MISSING, and the report is not ok");
	Check(MissingReport.Missing().size() == Set.PredicatePacks.size(),
		"every active pack is reported missing when the source has none of them");

	// A pack the engine REFUSES is FAILED, with the engine's own message.
	std::vector<std::pair<std::string, std::string>> Texts;
	for (const std::string& Area : Set.PredicatePacks) {
		Texts.emplace_back(Area, "a_clause(" + Area + ").\n");
	}
	insimul::FInsimulMemoryPackSource Memory(Texts, "in memory");
	FRecordingConsult Refusing;
	Refusing.RefuseContaining = "a_clause(" + Set.PredicatePacks.front() + ")";
	insimul::FInsimulPackConsultReport FailedReport = insimul::ConsultActivePacks(
		&Set, Data.Manifest, &Memory,
		[&Refusing](const std::string& Text, std::string& OutError) { return Refusing(Text, OutError); });
	Check(!FailedReport.IsOk() && FailedReport.Failed().size() == 1,
		"a pack the engine refuses is FAILED, once");
	Check(FailedReport.Describe().find("syntax error") != std::string::npos,
		"the engine's own message survives into the report");

	// A source with no consult at all is a failure, not a silent success.
	insimul::FInsimulPackConsultReport NoConsult =
		insimul::ConsultActivePacks(&Set, Data.Manifest, &Memory, nullptr);
	Check(!NoConsult.IsOk(), "no consult callback is a reported failure, not a no-op that reads as consulted");
}

// ── 4. the host restriction ─────────────────────────────────────────────────

class FStubPerception : public insimul::IPerceptionProbe {
public:
	bool Sense(const insimul::FPerceptionQuery&, insimul::FPerceptionReading&) override { return false; }
};
class FStubTraversal : public insimul::ITraversalProbe {
public:
	bool Query(const insimul::FTraversalQuery&, insimul::FTraversalReading&) override { return false; }
};
class FStubTrajectory : public insimul::ITrajectoryProbe {
public:
	bool Query(const insimul::FTrajectoryQuery&, insimul::FTrajectoryReading&) override { return false; }
};
class FStubLocomotion : public insimul::ILocomotionHost {
public:
	bool Travel(const insimul::FLocomotionOrder&, insimul::FArrivalReport&) override { return false; }
};
class FStubSkillSink : public insimul::ISkillModifierSink {
public:
	void ApplyModifiers(const std::string&, const insimul::FSkillModifiers&) override {}
};
class FStubStatSink : public insimul::ICombatStatSink {
public:
	bool GetBaseStats(const std::string&, insimul::FCombatStats&) const override { return false; }
	void ApplyStats(const std::string&, const insimul::FCombatStats&) override {}
};
class FStubCombat : public insimul::ICombatSystem {
public:
	void RegisterEntity(const insimul::FCombatEntityData&) override {}
	void UnregisterEntity(const std::string&) override {}
	bool ExecuteAttack(const std::string&, const std::string&, insimul::FDamageResult&) override { return false; }
	void ApplyDamage(const std::string&, double) override {}
	bool IsCombatEnabled() const override { return true; }
	double GetHealth(const std::string&) const override { return 0.0; }
	void Heal(const std::string&, double) override {}
	void Dispose() override {}
};
class FStubSurvival : public insimul::ISurvivalSystem {
public:
	void Update(double) override {}
	void RestoreNeed(const std::string&, double) override {}
	bool ConsumeStamina(double) override { return true; }
	void RecoverStamina(double) override {}
	void SetTemperature(double) override {}
	void AddModifier(const insimul::FNeedModifier&) override {}
	void RemoveModifier(const std::string&) override {}
	bool GetNeed(const std::string&, insimul::FNeedState&) const override { return false; }
	void GetAllNeeds(std::vector<insimul::FNeedState>&) const override {}
	double GetNeedPercent(const std::string&) const override { return 0.0; }
	bool IsAnyCritical() const override { return false; }
	bool IsAnyWarning() const override { return false; }
	void SetEnabled(bool) override {}
	bool IsEnabled() const override { return true; }
	void SetOnNeedChanged(void (*)(const insimul::FNeedState&)) override {}
	void SetOnSurvivalEvent(void (*)(const insimul::FSurvivalEvent&)) override {}
	void SetOnDamageFromNeed(void (*)(const std::string&, double)) override {}
	void Dispose() override {}
};

/** Wire every slot. */
void WireAll(insimul::FInsimulMechanicHosts& Hosts,
	FStubCombat& Combat, FStubSurvival& Survival, FStubStatSink& Stats, FStubTrajectory& Trajectory,
	FStubPerception& Perception, FStubTraversal& Traversal, FStubLocomotion& Locomotion, FStubSkillSink& Skills) {
	Hosts.Combat = &Combat;
	Hosts.Survival = &Survival;
	Hosts.SurvivalActorId = "player";
	Hosts.Adapter.CombatStats = &Stats;
	Hosts.Adapter.Trajectory = &Trajectory;
	Hosts.Adapter.Perception = &Perception;
	Hosts.Adapter.Traversal = &Traversal;
	Hosts.Adapter.Locomotion = &Locomotion;
	Hosts.Adapter.SkillModifiers = &Skills;
}

void TestTheHostRestriction(const FShippedData& Data) {
	FStubCombat Combat;
	FStubSurvival Survival;
	FStubStatSink Stats;
	FStubTrajectory Trajectory;
	FStubPerception Perception;
	FStubTraversal Traversal;
	FStubLocomotion Locomotion;
	FStubSkillSink Skills;

	const std::vector<std::string>& Universe = Data.Manifest.ConsultOrder();
	const std::vector<std::string>& Slots = insimul::FInsimulMechanicHosts::Slots();

	for (const std::string& Genre : Data.Table.Genres()) {
		const insimul::FInsimulActiveModuleSet Set =
			Data.Table.Resolve(Genre, Universe, insimul::EInsimulGenreSource::WorldIr);

		insimul::FInsimulMechanicHosts Hosts;
		WireAll(Hosts, Combat, Survival, Stats, Trajectory, Perception, Traversal, Locomotion, Skills);
		for (const std::string& Slot : Slots) {
			Check(Hosts.Has(Slot), "a fully wired container holds '" + Slot + "' before any restriction");
		}

		const std::vector<std::string> Dropped = Hosts.RestrictTo(Set.HostInterfaces);
		for (const std::string& Slot : Slots) {
			const bool bShouldHold = Set.ActivatesHost(Slot);
			Check(Hosts.Has(Slot) == bShouldHold,
				"genre '" + Genre + "': host '" + Slot + "' is registered exactly when an active module names it");
			if (!bShouldHold) {
				Check(Contains(Dropped, Slot),
					"genre '" + Genre + "': dropping '" + Slot + "' is REPORTED, not silent");
			}
		}
		Check(Dropped.size() + Set.HostInterfaces.size() >= Slots.size(),
			"genre '" + Genre + "': every slot is either kept or named as dropped");
		for (const std::string& Name : Dropped) {
			Check(Name.find("INTERNAL:") == std::string::npos, "RestrictTo's self-check: " + Name);
		}

		// Restricting twice drops nothing more — the operation is idempotent, so a
		// second activation pass cannot report phantom drops.
		Check(Hosts.RestrictTo(Set.HostInterfaces).empty(),
			"genre '" + Genre + "': a second restriction drops nothing");
	}

	// An EMPTY active set drops everything — the correct reading of a genre that
	// selects no module, and the fallback most likely to be quietly skipped.
	insimul::FInsimulMechanicHosts Hosts;
	WireAll(Hosts, Combat, Survival, Stats, Trajectory, Perception, Traversal, Locomotion, Skills);
	const std::vector<std::string> All = Hosts.RestrictTo({});
	Check(All.size() == Slots.size(), "an empty active set unregisters every host");
	for (const std::string& Slot : Slots) {
		Check(!Hosts.Has(Slot), "'" + Slot + "' is unregistered under an empty active set");
	}
	// And unregistering never destroys: the objects are still there to re-wire.
	WireAll(Hosts, Combat, Survival, Stats, Trajectory, Perception, Traversal, Locomotion, Skills);
	Check(Hosts.Has(Slots.front()), "a dropped host is UNREGISTERED, not destroyed — re-wiring works");
}

// ── 5. the scenario reader and runner ───────────────────────────────────────

/** A KB that answers from a table, so every runner outcome is reachable. */
class FScriptedKb : public insimul::IInsimulScenarioKb {
public:
	std::vector<std::string> Asserted;
	std::vector<std::string> Retracted;
	std::vector<std::string> Asked;
	std::string RefuseAssertContaining;
	std::string RaiseGoalContaining;
	/** Whether the Nth ask holds, in step order. The same GOAL legitimately answers
	 *  differently at two points in a scenario — the lit-torch step re-asks the
	 *  perception query after the world changed — so a scripted KB must answer by
	 *  position, not by goal text. */
	std::vector<bool> HoldsInOrder;

	bool Assert(const std::string& Clause, std::string& OutError) override {
		if (!RefuseAssertContaining.empty() && Clause.find(RefuseAssertContaining) != std::string::npos) {
			OutError = "type_error(callable, " + Clause + ")";
			return false;
		}
		Asserted.push_back(Clause);
		return true;
	}

	bool Retract(const std::string& Clause, std::string& OutError) override {
		(void)OutError;
		Retracted.push_back(Clause);
		return true;
	}

	bool Ask(const std::string& Goal, bool& bOutHolds, std::string& OutError) override {
		Asked.push_back(Goal);
		if (!RaiseGoalContaining.empty() && Goal.find(RaiseGoalContaining) != std::string::npos) {
			OutError = "existence_error(procedure, " + Goal + ")";
			return false;
		}
		const std::size_t At = Asked.size() - 1;
		bOutHolds = At < HoldsInOrder.size() ? HoldsInOrder[At] : false;
		return true;
	}
};

/** A host supplier that can answer, decline or fall silent. */
class FScriptedSupplier : public insimul::IInsimulScenarioHostSupplier {
public:
	bool bSilent = false;
	bool bMeasuredNotHolding = false;
	std::vector<std::string> Seen;

	bool Supply(const insimul::FInsimulScenarioHostFact& Declared, std::string& OutClause) override {
		Seen.push_back(Declared.Probe);
		if (bSilent) {
			return false;
		}
		OutClause = bMeasuredNotHolding ? std::string() : Declared.Fact;
		return true;
	}
};

void TestTheScenario(const FShippedData& Data) {
	std::string Json;
	if (!ReadFile(DataPath("scenarios/dark-courtyard.json"), Json)) {
		Check(false, "the shipped scenario exists at Content/Data/insimul/scenarios/dark-courtyard.json");
		return;
	}
	insimul::FInsimulMechanicScenario Scenario;
	std::string Error;
	if (!insimul::FInsimulMechanicScenario::Parse(Json, Scenario, Error)) {
		Check(false, "the shipped scenario parses: " + Error);
		return;
	}

	Check(Scenario.Mechanics().size() >= 2,
		"the scenario names at least two mechanics — US-3's third criterion, as a property of the file");
	Check(!Scenario.Genre.empty(), "the scenario states the genre whose bundle decides its packs");

	// The scenario's genre must be one the table knows, or its packs are a fiction.
	const insimul::FInsimulActiveModuleSet Set = Data.Table.Resolve(
		Scenario.Genre, Data.Manifest.ConsultOrder(), insimul::EInsimulGenreSource::WorldIr);
	Check(Set.bKnown, "the scenario's genre is one the activation table knows");
	for (const insimul::FInsimulScenarioStep& Step : Scenario.Steps) {
		if (Step.Mechanic.empty()) {
			continue;
		}
		Check(Set.IsModuleActive(Step.Mechanic) != Step.bInactive,
			"step '" + Step.Name + "': its module is active exactly when the step does not claim to be about an inactive one");
	}
	// A step marked inactive is what proves the §7.3 cost from inside the scene.
	bool bHasInactiveStep = false;
	for (const insimul::FInsimulScenarioStep& Step : Scenario.Steps) {
		bHasInactiveStep = bHasInactiveStep || Step.bInactive;
	}
	Check(bHasInactiveStep, "the scenario asks about a module its genre does NOT activate");

	// Every host fact declares which interface measured it, and how.
	for (const insimul::FInsimulScenarioStep& Step : Scenario.Steps) {
		for (const insimul::FInsimulScenarioHostFact& Fact : Step.HostFacts) {
			Check(!Fact.From.empty() && !Fact.Probe.empty() && !Fact.Note.empty(),
				"host fact '" + Fact.Fact + "' names the interface, the probe and how this engine measures it");
			Check(Contains(insimul::FInsimulMechanicHosts::Slots(), Fact.From),
				"host fact '" + Fact.Fact + "' names a host interface this plugin actually implements");
			Check(Set.ActivatesHost(Fact.From),
				"host fact '" + Fact.Fact + "' comes from an interface this genre's modules activate");
		}
	}

	// The runner, over a KB scripted to agree with the file.
	FScriptedKb Kb;
	for (const insimul::FInsimulScenarioStep& Step : Scenario.Steps) {
		Kb.HoldsInOrder.push_back(Step.ExpectsSuccess());
	}
	insimul::FInsimulScenarioReport Report = insimul::RunScenario(Scenario, Kb);
	Check(Report.IsOk(), "the runner matches every step against a KB that answers what the file expects");
	Check(Report.MechanicsExercised().size() >= 2,
		"and it counts at least two mechanics as exercised: " + insimul::JoinNames(Report.MechanicsExercised()));
	Check(Report.Steps.size() == Scenario.Steps.size(), "every step produced a result");
	Check(Report.Steps.front().bUsedRecordedHostFacts,
		"with no live host, the report SAYS the recorded readings were replayed");
	for (const insimul::FInsimulScenarioStepResult& Result : Report.Steps) {
		Check(Result.Step.bInactive || Contains(Report.MechanicsExercised(), Result.Step.Mechanic),
			"step '" + Result.Step.Name + "' counts towards its mechanic unless it is the inactive-module step");
	}

	// A live supplier is what a scene passes; the probe names reach it.
	FScriptedKb LiveKb;
	for (const insimul::FInsimulScenarioStep& Step : Scenario.Steps) {
		LiveKb.HoldsInOrder.push_back(Step.ExpectsSuccess());
	}
	FScriptedSupplier Supplier;
	insimul::FInsimulScenarioReport LiveReport = insimul::RunScenario(Scenario, LiveKb, &Supplier);
	Check(LiveReport.IsOk(), "the same scenario runs against a live host supplier");
	Check(!Supplier.Seen.empty(), "the live supplier was asked for every declared host fact");
	Check(!LiveReport.Steps.front().bUsedRecordedHostFacts,
		"a live run is NOT reported as a replay");

	// THE THREE HOST ANSWERS, kept apart.
	FScriptedKb SilentKb;
	FScriptedSupplier Silent;
	Silent.bSilent = true;
	insimul::FInsimulScenarioReport SilentReport = insimul::RunScenario(Scenario, SilentKb, &Silent);
	bool bSawSilent = false;
	for (const insimul::FInsimulScenarioStepResult& Result : SilentReport.Steps) {
		bSawSilent = bSawSilent || Result.Outcome == insimul::EInsimulScenarioOutcome::HostSilent;
	}
	Check(bSawSilent, "a host with NO reading makes the step HostSilent — the goal is never asked against a stale value");

	FScriptedKb NotHoldingKb;
	FScriptedSupplier NotHolding;
	NotHolding.bMeasuredNotHolding = true;
	insimul::FInsimulScenarioReport NotHoldingReport = insimul::RunScenario(Scenario, NotHoldingKb, &NotHolding);
	bool bAskedAnyway = false;
	bool bRecordedNotHolding = false;
	for (const insimul::FInsimulScenarioStepResult& Result : NotHoldingReport.Steps) {
		bAskedAnyway = bAskedAnyway || Result.Outcome != insimul::EInsimulScenarioOutcome::HostSilent;
		bRecordedNotHolding = bRecordedNotHolding || !Result.HostFactsNotHolding.empty();
	}
	Check(bAskedAnyway && bRecordedNotHolding,
		"a host that MEASURED the fact does not hold asserts nothing and the goal is still asked");

	// A raised goal is a third outcome, never a refusal.
	FScriptedKb RaisingKb;
	RaisingKb.RaiseGoalContaining = Scenario.Steps.front().Goal;
	insimul::FInsimulScenarioReport RaisedReport = insimul::RunScenario(Scenario, RaisingKb);
	Check(RaisedReport.Steps.front().Outcome == insimul::EInsimulScenarioOutcome::Raised,
		"a goal that RAISES is Raised, not 'did not hold' (§14.3 — a scene must not show a refusal core never decided)");
	Check(!RaisedReport.IsOk(), "and a raised step fails the run");

	// A refused world fact stops the run before a single step.
	FScriptedKb RefusingKb;
	RefusingKb.RefuseAssertContaining = Scenario.World.front();
	insimul::FInsimulScenarioReport RefusedReport = insimul::RunScenario(Scenario, RefusingKb);
	Check(!RefusedReport.SetupErrors.empty() && RefusedReport.Steps.empty(),
		"a world fact the KB refuses stops the scenario before any step runs");

	// A KB that answers the wrong way is caught.
	FScriptedKb WrongKb;
	insimul::FInsimulScenarioReport WrongReport = insimul::RunScenario(Scenario, WrongKb);
	bool bMismatched = false;
	for (const insimul::FInsimulScenarioStepResult& Result : WrongReport.Steps) {
		bMismatched = bMismatched || Result.Outcome == insimul::EInsimulScenarioOutcome::Mismatched;
	}
	Check(bMismatched && !WrongReport.IsOk(),
		"a KB that answers the other way is Mismatched — the comparator does not accept everything");

	// Malformed scenarios are rejected rather than half-read.
	insimul::FInsimulMechanicScenario Bad;
	std::string BadError;
	Check(!insimul::FInsimulMechanicScenario::Parse("{\"steps\":[]}", Bad, BadError),
		"a scenario with no steps is rejected");
	Check(!insimul::FInsimulMechanicScenario::Parse(
			  "{\"steps\":[{\"goal\":\"x\",\"expect\":\"maybe\"}]}", Bad, BadError),
		"a step expecting neither 'succeeds' nor 'fails' is rejected rather than read as 'fails'");
	Check(!insimul::FInsimulMechanicScenario::Parse("{\"steps\":[{\"expect\":\"fails\"}]}", Bad, BadError),
		"a step that asks no goal is rejected");
}

// ── 6. the pack manifest itself ─────────────────────────────────────────────

void TestTheManifest(const FShippedData& Data) {
	Check(Data.Manifest.CoreCommit() != "unknown",
		"the pack manifest records the core commit its texts came from");
	insimul::FInsimulDirectoryPackSource Source(DataPath("packs"), &Data.Manifest);
	for (const std::string& Area : Data.Manifest.ConsultOrder()) {
		std::string Text;
		Check(Source.Read(Area, Text) && !Text.empty(),
			"pack '" + Area + "' has text on disk where the manifest says (" + Source.LastError() + ")");
		Check(!Data.Manifest.FileOf(Area).empty(), "pack '" + Area + "' has a declared file name");
	}
	// The manifest's always-active packs are packs it carries.
	for (const std::string& Area : Data.Manifest.AlwaysActivePacks()) {
		Check(!Data.Manifest.FileOf(Area).empty(),
			"always-active pack '" + Area + "' is one this build carries");
	}
	// A pack the build does not carry cannot be read, and says so.
	std::string Missing;
	insimul::FInsimulDirectoryPackSource Bare(DataPath("packs"), nullptr);
	Check(!Bare.Read("a-pack-that-does-not-exist", Missing) && !Bare.LastError().empty(),
		"a pack that is not there reads as a reported miss, not as empty text");

	// A corrupt manifest fails loudly.
	insimul::FInsimulPredicatePackManifest Bad;
	std::string Error;
	Check(!insimul::FInsimulPredicatePackManifest::Parse("{}", Bad, Error),
		"a manifest with no consultOrder is rejected");
	Check(!insimul::FInsimulPredicatePackManifest::Parse("nonsense", Bad, Error),
		"a manifest that is not JSON is rejected");
	insimul::FInsimulActivationTable BadTable;
	Check(!insimul::FInsimulActivationTable::Parse("{\"genres\":{}}", BadTable, Error),
		"an activation table that knows no genres is rejected — a game must not boot activating nothing");
	Check(!insimul::FInsimulActivationTable::Parse("{}", BadTable, Error),
		"an activation table with no genres object is rejected");
}

} // namespace

int main() {
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	std::printf("module activation gate (portable, no native library)\n");
	std::printf("  data: %s\n", INSIMUL_ACTIVATION_DATA_DIR);

	const FShippedData Data = LoadShippedData();
	if (Data.bOk) {
		TestTheResolution(Data);
		TestTheGenreRead(Data);
		TestThePackConsult(Data);
		TestTheHostRestriction(Data);
		TestTheScenario(Data);
		TestTheManifest(Data);
	}

	if (Failures > 0) {
		std::printf("FAILED: %d of %d check(s)\n", Failures, Checks);
		return 1;
	}
	std::printf("OK: %d check(s)\n", Checks);
	return 0;
}
