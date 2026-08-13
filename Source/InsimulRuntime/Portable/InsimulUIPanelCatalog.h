// Copyright 2024 Insimul. All Rights Reserved.
//
// Which default-UI panels this world actually has, resolved through the MODULE
// REGISTRY rather than through a list in this file (US-1 of tasklist 190,
// PLATFORM_SPLIT_AND_ENGINE_PLUGINS.md §4.5 + core's module contract §7).
//
// WHAT THIS ADDS TO FInsimulUIRegistryModel. The registry answers "which widget
// serves panel key K", with a creator override layer on top of the shipped default.
// It cannot answer the question a module-gated UI actually asks, which is whether
// panel key K belongs to this world AT ALL: a world whose genre bundle did not
// select the module that owns a panel must not show that panel, because core's
// module contract §7.3 says an unselected module contributes no consulted rule pack
// and no registered system — its vocabulary is ABSENT from the KB, so a panel over
// it would render an empty box backed by predicates that have no solutions. This
// file is the join: catalog (which module owns which panel) x resolved module set
// (which modules this world turns on) x registry (which widget serves it).
//
// AND THE OWNERSHIP IS DATA. Search this file for the name of any mechanic and you
// will not find one — the same acceptance criterion the activation resolver carries
// (see InsimulModuleActivation.h). The panel -> module ownership lives in the data an
// exported game ships, `Content/Data/insimul/ui/panels.json`, so adding a panel, or
// moving one under a different module, is a re-vendor and not an engine code change.
// `tools/verify-mechanics/check-activation.mjs` fails if a module id or a pack area
// ever appears quoted in this source, and ctest `ui_registry` fails if the catalog
// names a module the activation table does not know.
//
// THE FINDING BEHIND THE DATA FILE. Core emits the genre -> module table
// (`conformance/modules/genre-activation.json`), and a module row carries its pack,
// its IR section, its decision layers and its host interfaces — but NO UI surface.
// Core has no field that says which panels a module brings, so the ownership table
// cannot be vendored from core the way the activation table is; it is this port's
// own data, held in the shipped data dir next to core's so the three native legs
// (Unreal / Unity / Godot) and the Babylon reference can be diffed against ONE file
// per engine rather than against each engine's source. See docs/ui.md § Module
// gating. The day core emits a `uiPanels` field per module, this file's Parse() reads
// it and the local table is deleted.
//
// THE THREE ANSWERS, MIRRORED. Gating follows the activation resolver's asymmetry
// exactly (InsimulModuleActivation.h), because a UI that hid panels in a state where
// the pack consult activates everything would be a second, disagreeing answer:
//
//   * a resolved set from a KNOWN genre    -> its modules' panels, and no others;
//   * a resolved set from an UNKNOWN genre -> no module-owned panel at all, the same
//                                             refusal the pack consult makes;
//   * NOTHING RESOLVED / genre UNDECLARED  -> every panel, because that is the state
//                                             an editor session, a commandlet and a
//                                             test are in, and it is reported rather
//                                             than silently assumed (Describe()).
//
// An override never ungates a panel: a creator swapping the widget for a panel says
// nothing about whether this world's modules brought that panel, and the registry
// (widget) and the module set (existence) are separate questions on purpose.
//
// std-only (no Unreal Engine, no CoreMinimal.h) so tools/verify-unreal drives every
// outcome headless. The UE seam is UInsimulUIPanelSurface (Public/InsimulUIPanelSurface.h),
// a thin, syntax-gated subsystem that mirrors these answers to Blueprint.

#pragma once

#include "InsimulModuleActivation.h"
#include "InsimulUIRegistryModel.h"

#include <string>
#include <vector>

namespace insimul {

/** One row of the shipped panel catalog. */
struct FInsimulPanelEntry {
	/** Stable panel key — the same key the registry and the shared corpus use. */
	std::string Key;
	/** The engine widget reference the shipped default binds (a WBP class path). */
	std::string Widget;
	/** The module id that owns this panel, or empty for a panel every world has. */
	std::string Module;
	/** Why this panel is owned by that module (creator-facing, never behavior). */
	std::string Notes;
};

/** What a panel resolution amounted to. */
enum class EInsimulPanelOutcome {
	/** Available, served by the shipped default widget. */
	Shipped,
	/** Available, served by a creator override. */
	Overridden,
	/** The key exists, and this world does not activate the module that owns it. */
	Gated,
	/** No such panel key in the catalog or the registry. */
	Unknown,
};

/** One panel's fate, with the reason. */
struct FInsimulPanelResolution {
	std::string Key;
	/** The widget to create, or empty for Gated / Unknown. */
	std::string Widget;
	/** The owning module id, or empty for an ungated panel. */
	std::string Module;
	EInsimulPanelOutcome Outcome = EInsimulPanelOutcome::Unknown;
	/** Creator-facing reason — never empty for Gated / Unknown. */
	std::string Detail;

	/** True when the panel may be shown at all. */
	bool IsAvailable() const {
		return Outcome == EInsimulPanelOutcome::Shipped || Outcome == EInsimulPanelOutcome::Overridden;
	}
};

/**
 * The shipped panel catalog (`Content/Data/insimul/ui/panels.json`), parsed.
 */
class FInsimulUIPanelCatalog {
public:
	/**
	 * Parse the catalog. Returns false with OutError set on a document that is not
	 * one — a build whose UI data is corrupt must say so, not quietly show nothing.
	 * (A return code rather than an exception: UE builds disable them.)
	 */
	static bool Parse(const std::string& Json, FInsimulUIPanelCatalog& OutCatalog, std::string& OutError);

	/**
	 * The catalog every build falls back to when the data dir is absent — the
	 * registry's own default panel map with no module ownership, i.e. every panel
	 * available. A plugin dropped into a project that ships no Insimul data dir still
	 * resolves panels; it just cannot gate them, and Describe() says so.
	 */
	static FInsimulUIPanelCatalog FallbackCatalog();

	const std::vector<FInsimulPanelEntry>& Entries() const { return Rows; }

	/** The row for `Key`, or null. */
	const FInsimulPanelEntry* Find(const std::string& Key) const;

	/** Every catalog key, in file order. */
	std::vector<std::string> Keys() const;

	/** Every distinct module id the catalog names, in file order. */
	std::vector<std::string> Modules() const;

	/** The catalog as a registry default map (key -> widget), in file order. */
	std::vector<std::pair<std::string, std::string>> DefaultRefs() const;

private:
	std::vector<FInsimulPanelEntry> Rows;
};

/**
 * Panel resolution through the module registry: the catalog joined to a resolved
 * module set, with the registry's creator-override layer underneath.
 *
 * Constructed UNGATED — every panel is available until a module set is applied —
 * because "nothing has resolved yet" and "this world activates nothing" are
 * different states and must not render the same.
 */
class FInsimulUIPanelResolver {
public:
	explicit FInsimulUIPanelResolver(FInsimulUIPanelCatalog InCatalog);

	/**
	 * Apply a resolved module set. A set whose genre is UNDECLARED leaves the
	 * resolver ungated, which is the same answer its pack consult gives (every pack).
	 */
	void SetActiveModules(const FInsimulActiveModuleSet& Set);

	/** Apply module ids directly — the Blueprint / host-test path. Gating is ON. */
	void SetActiveModuleIds(std::vector<std::string> Ids);

	/** Return to "nothing resolved": every panel available. */
	void SetUngated();

	/** True when a module set has been applied and gating is in force. */
	bool IsGated() const { return bGated; }

	/** Register / override the widget serving `Key` (an override always wins). */
	void Override(const std::string& Key, const std::string& Widget);

	/** Resolve one panel key. Records a diagnostic for Gated / Unknown. */
	FInsimulPanelResolution Resolve(const std::string& Key);

	/** Non-mutating peek — no diagnostic recorded. */
	FInsimulPanelResolution Peek(const std::string& Key) const;

	/** Catalog keys this world may show, in file order. */
	std::vector<std::string> AvailableKeys() const;

	/** Catalog keys withheld because their module is not active, in file order. */
	std::vector<std::string> GatedKeys() const;

	/** One line a boot log can print and a bug report can quote. */
	std::string Describe() const;

	const FInsimulUIPanelCatalog& Catalog() const { return PanelCatalog; }

	/** The registry underneath — the widget layer, with its own diagnostics. */
	FInsimulUIRegistryModel& Registry() { return RegistryModel; }
	const FInsimulUIRegistryModel& Registry() const { return RegistryModel; }

	/** Gating diagnostics (kind "inactive_module") plus the registry's own. */
	std::vector<FUIRegistryDiagnostic> Diagnostics() const;
	bool HasDiagnostics() const;
	void ClearDiagnostics();

private:
	bool IsModuleActive(const std::string& ModuleId) const;

	FInsimulUIPanelCatalog PanelCatalog;
	FInsimulUIRegistryModel RegistryModel;
	std::vector<std::string> ActiveModules;
	std::string ActiveGenre;
	bool bGated = false;
	std::vector<FUIRegistryDiagnostic> GateDiagnostics;
};

} // namespace insimul
