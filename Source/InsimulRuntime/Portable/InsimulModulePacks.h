// Copyright 2024 Insimul. All Rights Reserved.
//
// Consulting exactly the rule packs the active modules own, and nothing else
// (US-3 of tasklist 146, RUNTIME_CORE_ADOPTION.md §14).
//
// WHAT "AN INACTIVE MODULE CONTRIBUTES NOTHING" MEANS HERE. Core's module contract
// §7.3 states the cost of not being selected: **no consulted rule pack and no
// registered system**, so an unselected mechanic is not merely unpopulated, it is
// ABSENT — `current_predicate(can_afford_stamina/2)` has no solutions in a world
// whose bundle did not select that module. This file is that sentence on the engine
// side: given a resolved FInsimulActiveModuleSet it consults the active packs, in
// core's consult order, and refuses to consult anything else. The refusal is
// reported, not implied: the report names every pack this build carries and this
// world did not activate.
//
// WHERE THE PACK TEXT COMES FROM, AND WHY THAT IS A FINDING. Core's §7.2 says a
// plugin "reads the genre out of the World IR, looks it up in that file, and knows
// which rule packs to consult" — and stops there. The TEXT of a pack is a TypeScript
// constant inside core's bundle and no C ABI row returns one (`core.methods` answers
// with five names, none of them a pack — ctest `mechanic_bridge`). So this plugin
// VENDORS the pack texts as data the exported game ships
// (`Content/Data/insimul/packs/`, hash-pinned to core by
// `tools/vendor-packs/vendor-packs.mjs`), and the source is behind an interface so
// that the day a `prolog.packs` row exists, a build can take its packs from the
// binary instead by swapping one implementation. RUNTIME_CORE_ADOPTION.md §14.1.
//
// NOTHING HERE PARSES OR EVALUATES PROLOG. The text goes from a source to a consult
// callback; the engine that reads it is libinsimul (insimul::InsimulKB in this
// plugin, UInsimulPrologSubsystem::ConsultWorldData in a game). A pack that fails to
// consult is reported with the engine's own message and never silently dropped.
//
// std-only, and the consult is a std::function so the seam is drivable with no KB at
// all — tools/verify-unreal exercises every outcome headless, and ctest
// `activation_witness` runs the same code over the real library.

#pragma once

#include "InsimulModuleActivation.h"

#include <functional>
#include <string>
#include <vector>

namespace insimul {

/** Reads one rule pack's Prolog text by area key. */
class IInsimulPredicatePackSource {
public:
	virtual ~IInsimulPredicatePackSource() = default;

	/**
	 * The pack's text into OutText. False when this source does not have it — a
	 * missing pack is a reported state, and this returns rather than throwing
	 * because UE builds disable exceptions (§12.5).
	 */
	virtual bool Read(const std::string& Area, std::string& OutText) = 0;

	/** Why the most recent Read() returned false. */
	virtual std::string LastError() const = 0;

	/** Where this source reads from, for the report ("Content/Data/insimul/packs"). */
	virtual std::string Describe() const = 0;
};

/**
 * The vendored pack manifest (`Content/Data/insimul/packs/PACKS.json`): core's
 * commit, the consult ORDER, and one entry per pack.
 */
class FInsimulPredicatePackManifest {
public:
	/** Parse the manifest. False with OutError set on a document that is not one. */
	static bool Parse(const std::string& Json, FInsimulPredicatePackManifest& OutManifest, std::string& OutError);

	/** The core commit the packs were vendored from. */
	const std::string& CoreCommit() const { return Commit; }

	/** Every pack this build carries, IN CORE'S CONSULT ORDER. This is the pack
	 *  universe an FInsimulActivationTable filters. */
	const std::vector<std::string>& ConsultOrder() const { return Order; }

	const std::vector<std::string>& AlwaysActivePacks() const { return AlwaysActive; }

	/** The file name a pack's text lives in, or empty. */
	std::string FileOf(const std::string& Area) const;

	/** The `name/arity` of the runtime predicates a pack declares — what a restored
	 *  save may legitimately carry for it. */
	const std::vector<std::string>& RuntimePredicatesOf(const std::string& Area) const;

private:
	struct FEntry {
		std::string Area;
		std::string File;
		std::vector<std::string> RuntimePredicates;
	};

	std::string Commit = "unknown";
	std::vector<std::string> Order;
	std::vector<std::string> AlwaysActive;
	std::vector<FEntry> Entries;

	const FEntry* FindEntry(const std::string& Area) const;
};

/**
 * Reads packs out of a directory of `<area>.pl` files — the exported game's
 * `Content/Data/insimul/packs`.
 *
 * It uses `std::ifstream` rather than `FFileHelper` on purpose: this is the ONE pack
 * reader, and the game, the ctest harness and any future editor tool all have to be
 * able to hold it. A UE-only reader would mean the gate exercising a second
 * implementation of the thing under test.
 */
class FInsimulDirectoryPackSource : public IInsimulPredicatePackSource {
public:
	/**
	 * @param InDir       Directory holding the pack files and PACKS.json.
	 * @param InManifest  Borrowed, may be null, for the area → file mapping. A null
	 *                    manifest falls back to "<area>.pl", which it records.
	 */
	FInsimulDirectoryPackSource(std::string InDir, const FInsimulPredicatePackManifest* InManifest = nullptr)
		: Dir(std::move(InDir)), Manifest(InManifest) {}

	bool Read(const std::string& Area, std::string& OutText) override;
	std::string LastError() const override { return Error; }
	std::string Describe() const override { return Dir; }

private:
	std::string Dir;
	const FInsimulPredicatePackManifest* Manifest = nullptr;
	std::string Error;
};

/** Packs handed over in memory — the seam a host test, an editor tool or a future
 *  `prolog.packs` bridge row plugs into. */
class FInsimulMemoryPackSource : public IInsimulPredicatePackSource {
public:
	explicit FInsimulMemoryPackSource(
		std::vector<std::pair<std::string, std::string>> InTexts, std::string InDescription = "in memory")
		: Texts(std::move(InTexts)), Description(std::move(InDescription)) {}

	bool Read(const std::string& Area, std::string& OutText) override;
	std::string LastError() const override { return Error; }
	std::string Describe() const override { return Description; }

private:
	std::vector<std::pair<std::string, std::string>> Texts;
	std::string Description;
	std::string Error;
};

/** What one pack's activation amounted to. */
enum class EInsimulPackOutcome {
	/** Consulted into the KB. */
	Consulted,
	/** This world does not activate the module that owns it — the pack was NOT
	 *  consulted, which is the whole point. */
	Inactive,
	/** Active, and the source has no text for it. The mechanic's vocabulary is absent
	 *  and every rule that reads it will fail or raise. */
	Missing,
	/** Active, present, and the Prolog engine refused it. */
	Failed,
};

/** One pack's fate, with the reason. */
struct FInsimulPackResult {
	std::string Area;
	EInsimulPackOutcome Outcome = EInsimulPackOutcome::Inactive;
	std::string Detail;
	std::size_t Bytes = 0;
};

/** Everything the activation did to the KB, in consult order. */
struct FInsimulPackConsultReport {
	std::vector<FInsimulPackResult> Results;

	std::vector<std::string> Consulted() const;
	std::vector<std::string> Skipped() const;
	std::vector<std::string> Missing() const;
	std::vector<std::string> Failed() const;

	/** True when every ACTIVE pack was consulted. A world whose packs are missing
	 *  boots with a mechanic that cannot answer — never a silent state. */
	bool IsOk() const;

	std::string Describe() const;
};

/**
 * Consults the active modules' rule packs into a KB — the engine-side half of core's
 * `GamePrologEngine.initialize()`, which consults ONLY the active set's packs.
 *
 * @param Set       The resolved module set (which packs are active). Null-safe: a
 *                  null set activates nothing, which is reported per pack.
 * @param Manifest  The pack universe and its order.
 * @param Source    Where pack text comes from; may be null (reported, not crashed).
 * @param Consult   The KB's consult — `InsimulKB::Consult` or
 *                  `UInsimulPrologSubsystem::ConsultWorldData` in a game. Returns
 *                  false with a message on a program the engine refused.
 */
FInsimulPackConsultReport ConsultActivePacks(
	const FInsimulActiveModuleSet* Set,
	const FInsimulPredicatePackManifest& Manifest,
	IInsimulPredicatePackSource* Source,
	const std::function<bool(const std::string& Text, std::string& OutError)>& Consult);

} // namespace insimul
