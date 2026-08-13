// Copyright 2024 Insimul. All Rights Reserved.
//
// The host half a game registers, and the documented fallback for every slot it
// leaves empty (US-1 of tasklist 146, RUNTIME_CORE_ADOPTION.md §12).
//
// TWO CONTAINERS, BECAUSE CORE HAS TWO. Core's `EngineHostAdapter`
// (`game-engine/host-contracts.ts`) carries the hooks core CALLS; `ICombatSystem` and
// `ISurvivalSystem` are systems the ENGINE owns and ports
// (`game-engine/system-contracts.ts`) and are wired straight into their decision
// layer's config instead — `CombatResolverConfig.combat`, `StaminaPoolConfig.survival`
// plus `survivalActorId`, which exists because the host's meter takes no actor
// argument. Collapsing the two into one bag here would misreport how core is actually
// configured, so this file keeps the same shape core does.
//
// NO SILENT NO-OP. Every slot is optional, every absence has a consequence core
// documents, and that consequence is DATA here rather than a comment: an empty slot
// answers ConsequenceOf() with the sentence core's contract gives it, and
// FInsimulMechanicSurface prints it. A host that supplies nothing is a legitimate
// configuration (core's own NULL_HOST_ADAPTER); a host that supplies nothing and says
// nothing is what this file exists to prevent.
//
// THE FALLBACKS ARE CORE'S, IMPLEMENTED ONCE. An absent probe, or one that answers
// false, reads as clear / passable / no reading; an absent locomotion host reads as
// arrived. Those are the exact degradations `host-contracts.ts` states per interface,
// and they belong on the host side of the boundary — the alternative is four engines
// each guessing, which is the drift the contract exists to stop. They decide NOTHING
// about a mechanic: a fallback cannot change a cost, a rung or a damage number,
// because none of those is computed here.
//
// NON-OWNING. Every pointer below is a borrowed observer. In this engine the hosts are
// UObjects (a UGameInstanceSubsystem, an actor component) whose lifetime is the game
// instance's, and a portable container must not out-live or delete one. Clearing a
// slot unregisters; it never destroys.
//
// std-only, no CoreMinimal.h — tools/verify-unreal drives this seam headless.

#pragma once

#include "InsimulMechanicContracts.h"

#include <string>
#include <vector>

namespace insimul {

/**
 * Core's `EngineHostAdapter`, restricted to the hooks the band-120 modules name.
 *
 * The six slots not here — `debug`, `lifecycle`, `speech`, `resources`, `agentActions`,
 * `inference` — belong to modules outside this band and to RUNTIME_CORE_ADOPTION.md
 * §2.1–§2.2's capability map. Leaving them out is a scope statement, not an omission.
 */
struct FInsimulHostAdapter {
	/** equipment — `EquipmentManager` writes recomputed totals here. */
	ICombatStatSink* CombatStats = nullptr;

	/** combat — is the line to that target clear. */
	ITrajectoryProbe* Trajectory = nullptr;

	/** perception — what an observer's senses can reach. */
	IPerceptionProbe* Perception = nullptr;

	/** traversal — can this actor cross that geometric link. */
	ITraversalProbe* Traversal = nullptr;

	/** traversal + routine + map — carry out a movement core afforded. */
	ILocomotionHost* Locomotion = nullptr;

	/** skill — apply the modifiers whose subject the engine owns. */
	ISkillModifierSink* SkillModifiers = nullptr;

	/** Whether a hook is wired, by core's interface name. */
	bool Has(const std::string& HostInterface) const;

	/**
	 * What leaving a hook empty COSTS, in core's own words — the sentence each
	 * interface's contract gives for "a host that supplies none". This is the
	 * acceptance criterion "no silent no-op" as a value a report can print.
	 */
	static std::string ConsequenceOf(const std::string& HostInterface);
};

/**
 * Everything the host half of the band-120 boundary consists of: the hook adapter core
 * calls, plus the two ported systems wired directly to their decision layers. A game
 * constructs one of these and hands it to FInsimulMechanicSurface.
 */
class FInsimulMechanicHosts {
public:
	FInsimulHostAdapter Adapter;

	/** combat — wired into `CombatResolverConfig.combat`, not into the adapter. */
	ICombatSystem* Combat = nullptr;

	/** stamina — wired into `StaminaPoolConfig.survival`. */
	ISurvivalSystem* Survival = nullptr;

	/**
	 * Whose spends reach Survival (`StaminaPoolConfig.survivalActorId`). The host's
	 * meter takes no actor argument, so core forwards one actor's spends and no
	 * others. Empty with a survival system wired means none do.
	 */
	std::string SurvivalActorId;

	/** Whether a host interface is wired, by core's name — either container. */
	bool Has(const std::string& HostInterface) const;

	/**
	 * Every interface name this container can hold, in core's order — the eight the
	 * band names. A list, not a switch, so RestrictTo() cannot fall behind Has().
	 */
	static const std::vector<std::string>& Slots();

	/**
	 * UNREGISTER every host whose interface no active module names — the second half
	 * of "an inactive module contributes nothing" (core's module contract §7.3: no
	 * consulted rule pack AND no registered system). A game that wires a combat host
	 * under a genre with no combat module keeps its component and loses the
	 * registration, and the returned list names every drop so a creator sees it
	 * rather than wondering why their host is never called.
	 *
	 * @param ActiveInterfaces The active set's host interfaces. An EMPTY list drops
	 *        everything, which is the correct reading of a genre that selects no
	 *        module; the "no activation resolved at all" case is the caller's to
	 *        report, and is why this takes the list rather than reaching for one.
	 * @return The interfaces that were wired and are now not, in Slots() order.
	 */
	std::vector<std::string> RestrictTo(const std::vector<std::string>& ActiveInterfaces);

	// ── the fallbacks, exactly as core's contracts state them ─────────────────

	/** Ask the trajectory probe. Absent or unanswered reads as CLEAR. */
	FTrajectoryReading Ask(const FTrajectoryQuery& Query) const;

	/**
	 * Ask the perception probe. Absent or unanswered reads as NO READING — core's
	 * "sensed nothing", which is not the same as "saw nothing of interest", so the
	 * answer is a bool rather than a zeroed reading.
	 */
	bool Sense(const FPerceptionQuery& Query, FPerceptionReading& OutReading) const;

	/** Ask the traversal probe. Absent or unanswered reads as PASSABLE. */
	FTraversalReading Ask(const FTraversalQuery& Query) const;

	/**
	 * Order a movement. Absent or unanswered reads as ARRIVED. A host that genuinely
	 * could not get there returns `bArrived = false` itself — that is an answer, and
	 * core does not refund the meter for it.
	 */
	FArrivalReport Travel(const FLocomotionOrder& Order) const;

	/** Hand an actor's whole current skill-modifier set to the sink, if any. */
	void ApplySkillModifiers(const std::string& ActorId, const FSkillModifiers& Modifiers) const;

	/** Hand an entity's equipment-adjusted totals to the sink, if any. */
	void ApplyCombatStats(const std::string& EntityId, const FCombatStats& Stats) const;

	/** Apply damage core already resolved. Absent = decided and tracked, not executed. */
	void ApplyDamage(const std::string& TargetId, double Damage) const;

	/**
	 * Forward one stamina spend core already priced, and only for the actor whose
	 * meter this host owns. Returns false when the host refused it — which core treats
	 * as the spend having failed, not as a licence to re-price it.
	 */
	bool ForwardStaminaSpend(const std::string& ActorId, double Amount) const;
};

} // namespace insimul
