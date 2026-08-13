// Copyright 2024 Insimul. All Rights Reserved.
//
// Which mechanic modules this world turns on, read out of DATA rather than out of a
// list in this file (US-3 of tasklist 146, RUNTIME_CORE_ADOPTION.md §14).
//
// THE POINT OF THIS FILE IS WHAT IS NOT IN IT. Search it for the name of any
// mechanic — the combat module, the stamina module, whichever one you are looking
// for — and you will not find one, and that is the acceptance criterion rather than
// a stylistic preference: core's module contract §7.2 is titled "It is data, so a
// plugin does not hardcode a list", and the reason is that a hardcoded list is the
// thing that drifts. Adding a module to a genre bundle in core re-emits
// `conformance/modules/genre-activation.json`; re-vendoring that file is the whole
// of the engine-side change. `tools/verify-mechanics/check-activation.mjs` fails if
// a module id or a pack area ever appears in this source.
//
// THREE ANSWERS, AND THEY ARE DIFFERENT ANSWERS (core §7.3's "one deliberate
// asymmetry", mirrored exactly):
//
//   * a KNOWN genre        → its modules, its packs, its host interfaces;
//   * an UNKNOWN genre     → the always-active packs and nothing else, because a
//                            genre core has never heard of must not silently
//                            inherit every mechanic in the build;
//   * NO GENRE DECLARED    → every pack. That is not a fallback to "unknown": it is
//                            the right default for a commandlet, a test or an
//                            editor session that is not a game, and a game that
//                            lands here says so out loud (Source == Undeclared).
//
// WHERE THE GENRE COMES FROM, AND THE HOLE IN IT. `ir.meta.genreConfig.id` is what
// core's `activeModulesForWorld()` reads and GenreOfWorldIr() is that read. A
// SaveFile's `worldSnapshot` carries NO meta and no genre (measured: its nine
// sections are world/countries/settlements/characters/lots/quests/rules/actions/
// grammars), so a resumed save cannot answer this question at all — the second boot
// path lands on Source == Undeclared unless the game states the genre itself. That
// is a finding written up in RUNTIME_CORE_ADOPTION.md §14.2, not something this file
// quietly patches over.
//
// std-only (FJsonValue is this module's own reader), so tools/verify-unreal drives
// every branch headless.

#pragma once

#include <string>
#include <vector>

namespace insimul {

/** One module, exactly as the vendored activation table describes it. Every field is
 *  core's; this plugin adds nothing. */
struct FInsimulActiveModule {
	/** Core's `InsimulModuleId`. */
	std::string Id;
	/** Core's display name. */
	std::string Name;
	/** The rule pack area this module owns (part 1), or empty. */
	std::string PredicatePack;
	/** The dotted WorldIR path its authored data rides in (part 2). */
	std::string IrSection;
	/** Core-side modules that DECIDE (part 3). */
	std::vector<std::string> DecisionLayers;
	/** Interfaces the host implements and core calls (part 4). */
	std::vector<std::string> HostInterfaces;
	/** Core's own claim that the module has all six parts. */
	bool bConforms = false;
};

/** Where the genre that produced an FInsimulActiveModuleSet came from. Printed in the
 *  boot report: "which modules are on" is only as trustworthy as the answer to "who
 *  said so". */
enum class EInsimulGenreSource {
	/** No genre was found or supplied — every pack is active (core's tool/editor
	 *  default). A shipped game in this state is a bug in its export. */
	Undeclared,
	/** Read from the World IR's `meta.genreConfig.id`. */
	WorldIr,
	/** Stated by the game (a settings field, a commandlet argument) — the documented
	 *  workaround while a save carries no genre (§14.2). */
	Declared,
};

/**
 * The resolved set: core's `ActiveModuleSet`, plus where the genre came from and
 * which packs this build can actually consult.
 */
struct FInsimulActiveModuleSet {
	std::string Genre;
	/** True when the table knows this genre. */
	bool bKnown = false;
	EInsimulGenreSource Source = EInsimulGenreSource::Undeclared;
	std::vector<FInsimulActiveModule> Modules;
	/**
	 * Pack areas to consult, IN CONSULT ORDER. The order is core's (PACKS.json
	 * `consultOrder`) and is a hard constraint, not a preference: a `:- dynamic`
	 * arriving after a clause for the same predicate is a permission_error on a
	 * strict ISO engine.
	 */
	std::vector<std::string> PredicatePacks;
	/** Host interfaces the active modules name — the whole set a host registers, and
	 *  nothing outside it may be registered. */
	std::vector<std::string> HostInterfaces;
	/** Non-fatal disagreements found while resolving (for instance the table's own
	 *  pack list not matching the recomputed one). Reported, never silently preferred
	 *  one way or the other. */
	std::vector<std::string> Warnings;

	bool IsModuleActive(const std::string& ModuleId) const;
	bool IsPackActive(const std::string& Area) const;

	/** Whether a host interface belongs to any active module. A host for an interface
	 *  outside this set MUST NOT be registered — an inactive module contributes no
	 *  consulted pack and no registered system (core §7.3). */
	bool ActivatesHost(const std::string& HostInterface) const;

	/** One line a boot log can print and a bug report can quote. */
	std::string Describe() const;
};

/**
 * The vendored genre → module table (`conformance/modules/genre-activation.json`,
 * shipped to a game as `Content/Data/insimul/modules/genre-activation.json`), parsed.
 * Core emits it with `npm run module-activation`; this plugin vendors it through
 * `tools/vendor-conformance.mjs` and never edits it.
 */
class FInsimulActivationTable {
public:
	/**
	 * Parse the vendored table. Returns false with OutError set on a document that is
	 * not one — a game booting on a corrupt activation table must fail loudly, not
	 * silently activate nothing. (A return code rather than an exception: UE builds
	 * disable them, §12.5.)
	 */
	static bool Parse(const std::string& Json, FInsimulActivationTable& OutTable, std::string& OutError);

	/**
	 * The genre a World IR export declares (`meta.genreConfig.id`), or empty when the
	 * document carries none — which is every SaveFile worldSnapshot (§14.2).
	 */
	static std::string GenreOfWorldIr(const std::string& IrJson);

	/** Packs every game consults whatever it selected — core derives them from the
	 *  packs that belong to no module, so withholding one would not deactivate a
	 *  mechanic, it would break the ones that are active. */
	const std::vector<std::string>& AlwaysActivePacks() const { return AlwaysActive; }

	/** Every genre the table knows, in file order. */
	const std::vector<std::string>& Genres() const { return GenreOrder; }

	/**
	 * Resolve a genre against this table.
	 *
	 * @param GenreId       The genre, or empty for "none declared".
	 * @param PackUniverse  Every pack area this build carries, in CONSULT order —
	 *                      PACKS.json's `consultOrder`. The active packs are this list
	 *                      filtered, so the order is core's and cannot be re-invented
	 *                      here.
	 * @param Source        Where the genre came from, for the report.
	 */
	FInsimulActiveModuleSet Resolve(
		const std::string& GenreId,
		const std::vector<std::string>& PackUniverse,
		EInsimulGenreSource Source) const;

	/**
	 * Resolve straight from a World IR export — core's `activeModulesForWorld()`. A
	 * document with no genre resolves as undeclared, which is a different answer from
	 * "unknown genre".
	 */
	FInsimulActiveModuleSet ResolveForWorldIr(
		const std::string& IrJson, const std::vector<std::string>& PackUniverse) const;

private:
	struct FGenreEntry {
		std::vector<FInsimulActiveModule> Modules;
		/** The pack list the table STATES, kept apart from the recomputed one. */
		std::vector<std::string> DeclaredPacks;
	};

	std::vector<std::string> GenreOrder;
	std::vector<std::pair<std::string, FGenreEntry>> ByGenre;
	std::vector<std::string> AlwaysActive;

	const FGenreEntry* FindGenre(const std::string& GenreId) const;
};

/** `Universe` restricted to `Wanted`, IN UNIVERSE ORDER. Exposed because the pack
 *  consult and the tests both need core's ordering rule and must not each invent it. */
std::vector<std::string> FilterInUniverseOrder(
	const std::vector<std::string>& Universe, const std::vector<std::string>& Wanted);

/** ", "-joined, or "none" for an empty list — the one rendering the reports share. */
std::string JoinNames(const std::vector<std::string>& Names);

} // namespace insimul
