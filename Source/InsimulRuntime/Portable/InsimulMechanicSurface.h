// Copyright 2024 Insimul. All Rights Reserved.
//
// Which band-120 mechanic modules this build can actually reach, asked of the BINARY
// rather than inferred (US-1 of tasklist 146, RUNTIME_CORE_ADOPTION.md §12).
//
// THE ONE HONEST QUESTION. Adopting a module is a row in
// `native/corebridge/js/entry.js` plus a host implementation — not a port (§4). So the
// host half in InsimulMechanicHosts.h is worth exactly as much as the rows behind it,
// and the only way to know whether the rows are there is to ask the library that
// shipped: `core.methods`. Not a version stamp, not a sibling checkout — the handle
// this game is holding.
//
// WHAT IT FOUND, AT THE TIME OF WRITING. The libinsimulcore this repo builds against
// (0.1.0, core 247adfa2) answers with FIVE methods — core.methods, quest.hydrate,
// quest.radiantTick, radiant.baseTemplates, radiant.generate — and not one mechanic
// row. Every band-120 module is therefore `BridgeHasNoRow` in every build that exists
// today, and this surface says so per module, with the rows it is missing. §12 is the
// write-up, including the findings that have to be answered before the rows can be
// written at all. The measurement itself is a gate:
// `tools/verify-mechanics/check-bridge-rows.mjs --require-binaries`.
//
// THE METHOD NAMES BELOW ARE A PROPOSAL, AND SAY SO. `RequiredMethods` is this
// package's proposed row set per module (§12.3), derived from each decision layer's
// own entry point — `CombatResolver.attack`, `DetectionTracker.observe`,
// `TraversalPlanner.traverse`, `StaminaPool.spend`, `SkillProgression.unlock`,
// `EquipmentManager.equip`, `RoutineDirector.tick`. It is NOT vendored from core and
// nothing in core promises it. Its only job is to make "this build cannot reach the
// combat module" a checkable statement instead of a feeling; when the real rows land
// with different names, this table changes and the gate that pins it fails until it
// does. It is the SAME table Unity's InsimulMechanicSurface.cs carries, deliberately:
// one proposal across the engines, not three.
//
// std-only and the bridge is behind insimul::ICoreCaller, so every branch — including
// the no-library one — is exercisable headless by tools/verify-unreal.

#pragma once

#include "InsimulCoreCaller.h"
#include "InsimulMechanicHosts.h"

#include <string>
#include <vector>

namespace insimul {

/** Why a module is or is not reachable. One reason per state, and none of them
 *  falls back to pretending. */
enum class EMechanicState {
	/** The bridge is there, the rows are there, and a host is wired. */
	Ready,
	/** libinsimulcore is not loadable at all, or cannot list its methods. */
	NoNativeBridge,
	/** The bridge loads and carries none of this module's rows. */
	BridgeHasNoRow,
	/** The rows are there and this game wired no host — legitimate, and every absent
	 *  hook's documented cost is in the message. */
	NoHost,
};

/** One band-120 module, as far as this plugin needs to know it: the parts core's
 *  manifest names, and the bridge rows a build needs to reach it. */
struct FMechanicModule {
	/** Core's `InsimulModuleId`. */
	std::string Id;
	/** The tasklist that landed the module in core. */
	std::string Tasklist;
	/** Core's `decisionLayer` — what decides, on core's side. */
	std::vector<std::string> DecisionLayers;
	/** Core's `hostInterface` — what executes, on this side. */
	std::vector<std::string> HostInterfaces;
	/** This plugin's PROPOSED bridge rows (§12.3). Not core's promise. */
	std::vector<std::string> RequiredMethods;
};

/**
 * The band-120 modules, mirrored from core's `INSIMUL_MODULES` — seven modules, eight
 * distinct host interfaces. `agentAi` and `map` are core's other two and are out of
 * this tasklist's band; `map`'s part 4 is `ILocomotionHost`, so adopting it later
 * costs no new interface here.
 *
 * Pinned by `tools/verify-mechanics/MODULE_HOSTS.json`, which carries core's commit
 * and the sha256 of the files this was read from.
 */
const std::vector<FMechanicModule>& MechanicModules();

/** The module with that id, or nullptr. */
const FMechanicModule* FindMechanicModule(const std::string& Id);

/** Every host interface the band names, deduplicated, in first-named order. */
const std::vector<std::string>& MechanicHostInterfaces();

/** What one module's adoption amounts to in this build. */
struct FMechanicModuleReport {
	std::string ModuleId;
	EMechanicState State = EMechanicState::NoNativeBridge;
	/** One sentence, with the remedy. Never "unavailable". */
	std::string Message;
	/** The proposed rows this build does not carry. */
	std::vector<std::string> MissingMethods;
	/** The module's host interfaces this game wired nothing for, each with the cost
	 *  core's contract states for leaving it empty. */
	std::vector<std::string> MissingHosts;

	bool IsReady() const { return State == EMechanicState::Ready; }
};

/**
 * Probes one build for the band-120 modules: asks the library what it can do, asks the
 * game what it wired, and reports per module. It executes no mechanic — it is the
 * thing you read before believing that any of them are wired.
 */
class FInsimulMechanicSurface {
public:
	/** The introspection row every build carries. */
	static const char* const MethodMethods;

	/** `Caller` is borrowed and may be null — a build with no libinsimulcore is a
	 *  supported answer, not an error. */
	explicit FInsimulMechanicSurface(ICoreCaller* InCaller) : Caller(InCaller) {}

	/** Ask the library and the host, and report on every band-120 module. */
	std::vector<FMechanicModuleReport> Probe(const FInsimulMechanicHosts& Hosts);

	/** The bridge's whole method list, after Probe(). */
	const std::vector<std::string>& Methods() const { return MethodList; }

	/** Why the bridge could not be asked, or "". */
	const std::string& BridgeError() const { return BridgeErrorText; }

	/** True once the library answered `core.methods`. */
	bool BridgeAnswered() const { return bBridgeAnswered; }

	/** Parse a `core.methods` result document into its method names. Exposed because a
	 *  gate reads the same answer this class does, and one reader is the point. */
	static std::vector<std::string> ParseMethodList(const std::string& Json);

private:
	void AskBridge();
	FMechanicModuleReport ReportFor(const FMechanicModule& Module, const FInsimulMechanicHosts& Hosts) const;

	ICoreCaller* Caller = nullptr;
	std::vector<std::string> MethodList;
	std::string BridgeErrorText;
	bool bBridgeAnswered = false;
};

} // namespace insimul
