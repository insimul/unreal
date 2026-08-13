// Copyright 2024 Insimul. All Rights Reserved.
//
// The playable scene's SCRIPT, and the one runner that executes it (US-3 of
// tasklist 146, RUNTIME_CORE_ADOPTION.md §14.4).
//
// WHY A SCENE HAS A FILE. US-3's third criterion is "a playable scene exercises at
// least two adopted mechanics end to end". A scene alone cannot be checked by
// anything in this repository — there is no Unreal in the harness — so the scene
// reads its script out of `Content/Data/insimul/scenarios/<id>.json` and the gate
// (ctest `activation_witness`) reads the SAME file and runs the SAME steps through
// the SAME libinsimul. What the human confirms in the editor is that the actor moves;
// what the gate confirms is that every decision the scene acts on is the decision
// core actually makes. Neither claim is asked to cover the other.
//
// HOST READINGS ARE MARKED AS SUCH. A step's `hostFacts` are the clauses a PROBE
// supplies — a line trace, a navigation query — and each records which host interface
// produced it and how. In a game the scene passes a live supplier and the geometry
// answers; in the gate no supplier is passed and the recorded value is used, which
// the report states (bUsedRecordedHostFacts) rather than letting a replay read as a
// measurement. Three answers are kept apart: a clause (assert it), NO reading at all
// (HostSilent — the step never runs), and a reading that does NOT hold (assert
// nothing and ask anyway — that is the case where the scene's geometry legitimately
// changes core's answer).
//
// RAISED IS NOT "NO". A goal that raises is a third outcome, never a refusal — a
// scene that showed a player a refusal core never decided would be exactly the bug
// §14.3 is about.
//
// std-only. The KB is behind IInsimulScenarioKb, so the runner drives
// insimul::InsimulKB in the gate, UInsimulPrologSubsystem in a game, and a recording
// stub in a host test.

#pragma once

#include <string>
#include <vector>

namespace insimul {

/** One clause a HOST measured, with the interface that measured it. */
struct FInsimulScenarioHostFact {
	/** The clause as recorded, without a trailing '.'. */
	std::string Fact;
	/** The host interface that supplies it (`IPerceptionProbe`, …). */
	std::string From;
	/** Which probe call on that interface, for a live supplier to dispatch on. */
	std::string Probe;
	/** The probe's arguments, in file order. */
	std::vector<std::pair<std::string, std::string>> Args;
	/** How the engine measures it, in prose — the note a creator reads. */
	std::string Note;

	/** The named argument, or empty. */
	std::string Arg(const std::string& Key) const;
};

/** One step: set the world up, ask core one question, act on the answer. */
struct FInsimulScenarioStep {
	std::string Name;
	/** The module this step exercises — used to count mechanics, not to dispatch. */
	std::string Mechanic;
	/** True when the step is ABOUT a module this genre does not activate. Such a step
	 *  proves the absence and does not count towards "two mechanics end to end". */
	bool bInactive = false;
	std::vector<std::string> Retract;
	std::vector<FInsimulScenarioHostFact> HostFacts;
	std::vector<std::string> Facts;
	std::string Goal;
	/** "succeeds" or "fails". */
	std::string Expect;
	/** What the scene DOES with the answer — the end of "end to end". */
	std::string Then;

	bool ExpectsSuccess() const { return Expect == "succeeds"; }
};

/** A scenario document. */
struct FInsimulMechanicScenario {
	std::string Id;
	/** The genre whose bundle decides which packs this scenario may use. */
	std::string Genre;
	std::string Description;
	/** Authored world facts, asserted once before the first step. */
	std::vector<std::string> World;
	std::vector<FInsimulScenarioStep> Steps;

	/** The distinct mechanics the ACTIVE steps name, in first-named order. */
	std::vector<std::string> Mechanics() const;

	/** Parse a scenario document. False with OutError set when it is not one. */
	static bool Parse(const std::string& Json, FInsimulMechanicScenario& OutScenario, std::string& OutError);
};

/** The KB a scenario runs against — assert, retract, ask. */
class IInsimulScenarioKb {
public:
	virtual ~IInsimulScenarioKb() = default;

	/** Assert a clause. False + OutError on refusal. */
	virtual bool Assert(const std::string& Clause, std::string& OutError) = 0;

	/** Retract the first matching clause. False + OutError on refusal; a clause that
	 *  matched nothing is NOT an error. */
	virtual bool Retract(const std::string& Clause, std::string& OutError) = 0;

	/** Ask a goal. bOutHolds is whether it has at least one solution; false +
	 *  OutError when the engine RAISED, which is a third answer and never "no". */
	virtual bool Ask(const std::string& Goal, bool& bOutHolds, std::string& OutError) = 0;
};

/**
 * Supplies the live value of a host-measured fact. Three answers, three meanings:
 *  - bHasReading = true, Clause non-empty → assert that clause;
 *  - bHasReading = false                  → the host has NO reading; HostSilent;
 *  - bHasReading = true, Clause empty     → measured, and it does NOT hold.
 */
class IInsimulScenarioHostSupplier {
public:
	virtual ~IInsimulScenarioHostSupplier() = default;
	virtual bool Supply(const FInsimulScenarioHostFact& Declared, std::string& OutClause) = 0;
};

/** How one step turned out. */
enum class EInsimulScenarioOutcome {
	/** Core answered, and it answered what the step expects. */
	Matched,
	/** Core answered the other way. */
	Mismatched,
	/** The goal RAISED — not an answer at all (see §14.3). */
	Raised,
	/** A fact could not be asserted or retracted; the step never ran. */
	SetupFailed,
	/** A host fact was expected from a live host and it supplied no reading at all —
	 *  which is not the same as measuring that it does not hold. */
	HostSilent,
};

/** One executed step. */
struct FInsimulScenarioStepResult {
	FInsimulScenarioStep Step;
	EInsimulScenarioOutcome Outcome = EInsimulScenarioOutcome::SetupFailed;
	/** Whether the goal held (meaningless unless Matched/Mismatched). */
	bool bHolds = false;
	std::string Detail;
	/** The host facts actually used, in the form they were asserted. */
	std::vector<std::string> HostFactsUsed;
	/** Host facts a live host measured as NOT holding — asserted nowhere, and the
	 *  reason core's answer may differ from the recorded one. */
	std::vector<std::string> HostFactsNotHolding;
	/** True when the recorded host facts were used because no live host was supplied
	 *  — a gate leg, not a scene. */
	bool bUsedRecordedHostFacts = false;
};

/** The whole run. */
struct FInsimulScenarioReport {
	FInsimulMechanicScenario Scenario;
	std::vector<FInsimulScenarioStepResult> Steps;
	/** World facts the KB refused; no step runs after one. */
	std::vector<std::string> SetupErrors;

	bool IsOk() const;

	/** The mechanics whose steps actually ran and matched — what "exercised end to
	 *  end" counts. */
	std::vector<std::string> MechanicsExercised() const;

	std::string Describe() const;
};

/**
 * Run a scenario against a KB.
 *
 * @param HostSupplier Null to use the RECORDED host facts (the gate's leg, reported
 *        as such); a live supplier in a scene, where the geometry answers.
 */
FInsimulScenarioReport RunScenario(
	const FInsimulMechanicScenario& Scenario,
	IInsimulScenarioKb& Kb,
	IInsimulScenarioHostSupplier* HostSupplier = nullptr);

} // namespace insimul
