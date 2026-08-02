// Copyright 2024 Insimul. All Rights Reserved.
//
// Radiant quest GENERATION — the first slice of `@insimul/core` this plugin
// adopts (tasklist 99, RUNTIME_CORE_ADOPTION.md §5).
//
// Radiant quests are the ones a game makes up on the fly from templates and the
// current world state ("cull the wolves", "deliver bread to cara") rather than
// authoring by hand. Core owns that algorithm — 678 lines of Prolog-driven,
// seeded, deterministic slot filling — and this plugin now CALLS it instead of
// re-implementing it. Nothing here reimplements any of it.
//
// NOT TO BE CONFUSED WITH THE RADIANT *TICK* (RUNTIME_CORE_ADOPTION.md §3.2).
// FInsimulQuestSystem::RadiantTick (Portable/InsimulQuestSystem.h) *offers*
// already-authored radiant quests over successive ticks and is pinned by
// conformance/quests/radiant-cases.json. This file *creates* quests and is
// pinned by conformance/radiant/*.json. Different capability, similar name.
//
// THIS FILE IS THE ONLY PLACE ENGINE TYPES BECOME CORE'S TYPES (US-2's fourth
// criterion). Everything below ICoreCaller is JSON bytes; everything above
// FRadiantSource is Unreal (UInsimulRadiantSourceShell, the thin UCLASS in
// Public/InsimulRadiantSourceShell.h — the `Shell` suffix is this module's
// convention for the UE-coupled wrapper over a portable core, as in
// FInsimulQuestSystemShell over FInsimulQuestSystem). Putting the conversion
// HERE — in Portable/, std-only, no CoreMinimal.h — is what makes it
// host-testable under plain clang++ by tools/verify-unreal, in the pattern
// every other semantic core in this module already follows:
//
//     UInsimulRadiantSourceShell  Public/ + Private/  FString / USTRUCT / TArray
//     insimul::FRadiantSource     <- you are here     std::string / FJsonValue
//     insimul::ICoreCaller        Portable/           JSON in, JSON out
//     FInsimulCoreBridge          Private/Core/       C ABI: insimul_core_call
//     libinsimulcore                                  QuickJS + the core bundle
//     libinsimul                                      Trealla, natively linked
//
// PERFORMANCE. Generate() crosses a JSON boundary and runs a Prolog program. It
// is a DECISION call — run it when the director should offer new work (entering
// a settlement, a day boundary), NEVER from Tick. See insimulcore.h's "ONE HARD
// RULE".
//
// PARITY. The generated quests are pinned by the shared cross-runtime corpus
// conformance/radiant/*.json — the same 11 vectors packages/core runs, and the
// same ones the Godot adapter runs. Gate: tools/verify-unreal, targets
// `radiant_source` (translation site, always runs) and `radiant_bridge` (the
// full stack, needs an insimul-native checkout).

#pragma once

#include "InsimulCoreCaller.h" // insimul::ICoreCaller — the transport seam
#include "InsimulJson.h"

#include <cstdint>
#include <string>
#include <vector>

namespace insimul {

// ── Selectable implementation (US-2: the old one stays reachable) ───────────

/**
 * Which implementation answers Generate().
 *
 * There is no *second* implementation of radiant generation in this repo to
 * keep alive — this engine has never generated a radiant quest (§6.2). `None`
 * is therefore literally the pre-adoption behaviour: no quests, ever. Keeping
 * it selectable for one story is what lets US-3 run both legs over the same 11
 * vectors and classify every difference, the same discipline tasklist 91 used
 * when it kept tau-prolog alive for one story before deleting it.
 *
 * It is also a genuine runtime fallback: a platform with no libinsimulcore
 * build (§4.7.2) lands here rather than erroring.
 */
enum class ERadiantSource : std::uint8_t {
	/** Generate through `@insimul/core` across the bridge. */
	Core,
	/** Pre-adoption behaviour: generate nothing. */
	None,
};

// ── Options ─────────────────────────────────────────────────────────────────

/**
 * The generation seed. Core accepts a string OR a number and hashes a string to
 * a uint32, so the type is carried across the boundary intact rather than
 * normalised — the corpus uses strings ("contract", "cap", "join7") and a game
 * may reasonably use a world seed integer.
 */
struct FRadiantSeed {
	bool bNumeric = false;
	double Number = 0.0;
	/** Original number lexeme, when this seed came from parsed JSON. */
	std::string RawNumber;
	/** Seed text, when !bNumeric. */
	std::string Text;

	static FRadiantSeed FromText(const std::string& InText) {
		FRadiantSeed Seed;
		Seed.Text = InText;
		return Seed;
	}

	static FRadiantSeed FromNumber(double InNumber, const std::string& InRaw = std::string()) {
		FRadiantSeed Seed;
		Seed.bNumeric = true;
		Seed.Number = InNumber;
		Seed.RawNumber = InRaw;
		return Seed;
	}
};

struct FRadiantOptions {
	FRadiantSeed Seed;
	/** Current in-game time in seconds; drives cooldowns and quest ids. */
	long long Now = 0;
	/** Cap for this tick. <= 0 means unbounded (the key is then omitted). */
	int MaxQuests = 0;
};

// ── Output ──────────────────────────────────────────────────────────────────

/**
 * One generated quest, with core's own field names so a caller can be read
 * against the runtime contract.
 *
 * ORDER OF OPERATIONS: FactsToRetract must be retracted BEFORE FactsToAssert is
 * asserted — they are the stale cooldown/generation facts the fresh ones
 * replace.
 */
struct FGeneratedRadiantQuest {
	std::string QuestId;
	std::string TemplateId;
	/** Canonical quest Prolog to consult, exactly as core emitted it. */
	std::string QuestContent;
	/**
	 * QuestContent split into clauses: trimmed, blank lines dropped, EMIT ORDER
	 * PRESERVED. A conforming engine may emit clauses within a quest in any
	 * order, so a comparison sorts these; that sort is the caller's, not ours.
	 */
	std::vector<std::string> ContentClauses;
	std::vector<std::string> FactsToAssert;
	std::vector<std::string> FactsToRetract;
};

// ── The adapter ─────────────────────────────────────────────────────────────

class FRadiantSource {
public:
	/**
	 * `InCaller` is borrowed, not owned — the caller outlives this object (in
	 * the plugin it is the bridge owned by UInsimulRadiantSource). A null caller
	 * behaves exactly like ERadiantSource::None.
	 */
	explicit FRadiantSource(ICoreCaller* InCaller = nullptr, ERadiantSource InSource = ERadiantSource::Core)
		: Caller(InCaller), SourceMode(InSource) {}

	void SetSource(ERadiantSource InSource) { SourceMode = InSource; }
	ERadiantSource Source() const { return SourceMode; }

	/**
	 * True when generation will actually reach core: the source is Core, a
	 * caller is attached, and its runtime started. False means Generate()
	 * behaves as None; LastError() explains why once something has been tried.
	 */
	bool IsCoreAvailable() const;

	/**
	 * Generate radiant quests for one tick.
	 *
	 * `Kb`        world facts + rules as Prolog source lines (the current KB).
	 * `Templates` the radiant template pack as Prolog source lines.
	 *
	 * Returns false only when the call FAILED (LastError() set). Zero quests is
	 * a normal, successful outcome — an unsatisfiable precondition, an active
	 * cooldown or an exclusion all produce it, and three corpus cases assert it.
	 * Under ERadiantSource::None this always succeeds with zero quests.
	 */
	bool Generate(const std::vector<std::string>& Kb,
		const std::vector<std::string>& Templates,
		const FRadiantOptions& Options,
		std::vector<FGeneratedRadiantQuest>& OutQuests);

	/**
	 * Core's shipped base template pack as Prolog source, so a game does not
	 * have to vendor a copy of it to generate anything. Empty when core is
	 * unavailable.
	 */
	std::string BaseTemplates();

	/** The template ids in the base pack, in declaration order. */
	std::vector<std::string> BaseTemplateIds();

	/**
	 * The adopted core surface, sorted — a build sanity check for a game (or a
	 * gate) that wants to assert the bridge is the one it expects.
	 */
	std::vector<std::string> CoreMethods();

	/** The bridge's version stamp: "<abi> (quickjs <pin>, core <commit>)". */
	std::string CoreVersion();

	/** Reason the last call failed, or "" if it succeeded. */
	const std::string& LastError() const { return LastErrorText; }

	// ── the wire format, exposed so a gate can assert it ────────────────────

	/**
	 * The exact `radiant.generate` argument document Generate() sends.
	 *
	 * Core takes ONE Prolog program: world facts first, then the template pack,
	 * joined with '\n'. That concatenation and its ORDER are part of the
	 * contract the corpus pins — packages/core's own runner
	 * (src/conformance/__tests__/radiant-corpus.test.ts) joins the two arrays
	 * exactly this way. Getting it wrong produces plausible-looking quests that
	 * silently disagree with every other runtime, which is why the host gate
	 * asserts this document byte-for-byte rather than only its effects.
	 */
	static std::string BuildGenerateArgs(const std::vector<std::string>& Kb,
		const std::vector<std::string>& Templates,
		const FRadiantOptions& Options);

	/** Decode core's `{"quests":[...]}` reply. False (with Error set) on garbage. */
	static bool DecodeQuests(const std::string& Json,
		std::vector<FGeneratedRadiantQuest>& OutQuests,
		std::string& OutError);

	/** Split emitted quest content into clauses: trimmed, blanks dropped. */
	static std::vector<std::string> SplitContentClauses(const std::string& Content);

private:
	/** Call `Method` and parse the reply as a JSON object. Null on failure. */
	FJsonValuePtr CallObject(const std::string& Method, const std::string& ArgsJson);

	ICoreCaller* Caller = nullptr;
	ERadiantSource SourceMode = ERadiantSource::Core;
	std::string LastErrorText;
};

} // namespace insimul
