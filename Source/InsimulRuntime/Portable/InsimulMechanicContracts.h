// Copyright 2024 Insimul. All Rights Reserved.
//
// The band-120 host interfaces, in this engine's language (US-1 of tasklist 146,
// RUNTIME_CORE_ADOPTION.md §12).
//
// Core landed nine mechanic modules; the seven in band 120–125 name EIGHT distinct
// host interfaces, and this header is their Unreal mirror. Core's own words for what
// each one is:
//
//   ICombatSystem      src/game-engine/system-contracts.ts — carries out an attack
//                      core already resolved. "An adapter must not roll its own damage."
//   ISurvivalSystem    src/game-engine/system-contracts.ts — carries out a spend core
//                      already priced. "An adapter must not price actions itself."
//   ICombatStatSink    src/game-engine/host-contracts.ts — told equipment-adjusted totals.
//   ITrajectoryProbe   is the line to that target clear, and how far away is it.
//   IPerceptionProbe   what can this observer's senses reach of that target, right now.
//   ITraversalProbe    could this actor actually get across that, from where they stand.
//   ILocomotionHost    carry out a movement core has afforded, permitted and charged for.
//   ISkillModifierSink apply the modifier set whose subject the engine owns.
//
// DECISIONS ARE CORE'S; EXECUTION IS OURS. Nothing here computes a damage number, a
// stamina cost, a suspicion rung, a traversal cost or a skill total. A probe REPORTS
// a measurement its engine took; a sink is TOLD an absolute core computed. An
// implementation of any of these that decides something has forked the decision layer
// — `docs/module-contract.md` §2.4, and the reason `check-mechanics.mjs` pins the
// member lists rather than trusting them.
//
// A MIRROR NOTHING CHECKS ROTS. `tools/verify-mechanics/check-mechanics.mjs` pins
// every interface below, member by member, against core's TypeScript — carrying core's
// commit and the sha256 of the three files it was derived from. This repo has shipped
// a hand-written mirror that drifted before (conformance/ was 41 of core's 76 Prolog
// cases for as long as nothing diffed it, §6.3), which is why the guard lands in the
// same commit as the mirror.
//
// std-only, no CoreMinimal.h: it lives in Portable/ so the fallbacks and the surface
// above it compile and run under plain clang++ in tools/verify-unreal, in the pattern
// every semantic core in this module already follows.
//
// UNREAL SPECIFIC — "must not throw" is a TYPE here, not a rule. Core's contracts say
// a probe "must not throw; a thrown error reads as clear/passable/no reading", and the
// Unity mirror honours that with a try/catch per call. Unreal builds with C++
// exceptions DISABLED (`bEnableExceptions` is false by default and this plugin does
// not set it), so a catch would be dead code in the shipping configuration and a lie
// in the harness. Every probe therefore returns `bool` — false is "the host could not
// answer" — and the fallback for it lives once, in FInsimulMechanicHosts. A host says
// "I have no answer" by returning false, never by throwing.

#pragma once

#include <string>
#include <vector>

namespace insimul {

// ── Closed vocabularies core states, mirrored as atoms ──────────────────────
//
// Strings rather than enums, deliberately: every one of these crosses the C ABI as
// JSON, and an enum here would be a second spelling of an atom core already owns
// (`docs/mechanic-predicates.md` §10.1). The arrays are what a host validates against.

/** MOVEMENT_URGENCIES — how pressing a movement is. Never a speed. */
extern const std::vector<std::string> MovementUrgencies;

/** MOVEMENT_STANCES — how the body is carried. Also PerceptionReading.stance's three. */
extern const std::vector<std::string> MovementStances;

/** NeedType — core's five needs (`visual-types.ts`). */
extern const std::vector<std::string> NeedTypes;

/** True when `Atom` is one of `Vocabulary`. */
bool IsInVocabulary(const std::vector<std::string>& Vocabulary, const std::string& Atom);

// ── Payloads ────────────────────────────────────────────────────────────────

/** CombatStats — the three numbers equipment modifies. */
struct FCombatStats {
	double AttackPower = 0.0;
	double Defense = 0.0;
	double DodgeChance = 0.0;
};

/** CombatEntityData — what registering an entity for combat states. */
struct FCombatEntityData {
	std::string Id;
	std::string Name;
	double Health = 0.0;
	double MaxHealth = 0.0;
	/** Optional in core; `bHasDamage` is false when the entity declared none. */
	bool bHasDamage = false;
	double Damage = 0.0;
};

/** DamageResult — what `ExecuteAttack` answers for a host that drives its own attacks. */
struct FDamageResult {
	std::string TargetId;
	double Damage = 0.0;
	bool bIsCritical = false;
	bool bIsBlocked = false;
	bool bIsDodged = false;
	double NewHealth = 0.0;
};

/** NeedModifier — a timed or permanent change to one need's decay rate. */
struct FNeedModifier {
	std::string Id;
	std::string NeedType;
	double RateMultiplier = 1.0;
	double Duration = 0.0;
	double StartTime = 0.0;
	std::string Source;
};

/** NeedState — one need, as the host reports it. */
struct FNeedState {
	std::string Id;
	double Current = 0.0;
	double Max = 0.0;
	double DecayRate = 0.0;
	bool bIsCritical = false;
	bool bIsWarning = false;
	std::vector<FNeedModifier> Modifiers;
};

/** SurvivalEvent — what the host's needs clock announced. */
struct FSurvivalEvent {
	std::string Type;
	std::string NeedType;
	double Value = 0.0;
	std::string Message;
};

/** TrajectoryQuery — one question about the line between two entities. */
struct FTrajectoryQuery {
	std::string Attacker;
	std::string Target;
	/** The action atom being attempted — the same atom `action/4` declares. */
	std::string Action;
	/** The action's authored reach, when it has one. */
	bool bHasRange = false;
	double Range = 0.0;
};

/** TrajectoryReading — two scalars and an atom. Nothing else. */
struct FTrajectoryReading {
	bool bClear = true;
	/** Separation in the world's authored distance unit; omitted = core uses its own. */
	bool bHasSeparation = false;
	double Separation = 0.0;
	/** For display. Core reads it as a reason string, never as a decision. */
	std::string BlockedBy;
};

/** PerceptionQuery — one (observer, target) question, per DETECTION tick, never per frame. */
struct FPerceptionQuery {
	std::string Observer;
	std::string Target;
	long long Tick = 0;
};

/**
 * PerceptionReading — MEASUREMENTS the engine took. What any of them is WORTH is
 * authored in `WorldIR.perception` and decided in core, so a host that reported
 * perfect visibility still cannot change one number of the ladder.
 */
struct FPerceptionReading {
	/** line_of_sight/2 and distance attenuation folded into [0,1]. Required. */
	double Visibility = 0.0;
	bool bHasCover = false;
	double Cover = 0.0;
	bool bHasAudibility = false;
	double Audibility = 0.0;
	/** light_level/2 where the TARGET is, 0–100. */
	bool bHasLight = false;
	double Light = 0.0;
	/** One of MovementStances — the carriage core asks for, reported back. */
	std::string Stance;
	/** noise_level/2 — how loudly the target is presenting, 0–100. */
	bool bHasNoise = false;
	double Noise = 0.0;
};

/** TraversalQuery — asked only for links a world marked `geometric`. */
struct FTraversalQuery {
	std::string Actor;
	/** A location atom, never a coordinate. */
	std::string From;
	std::string To;
	/** walk, climb, swim, jump, … */
	std::string Mode;
	std::string Link;
};

/** TraversalReading — a boolean, an advisory scalar and an atom. */
struct FTraversalReading {
	bool bPassable = true;
	/** Advisory: core prices a link from traversal_cost/3, not from what we measured. */
	bool bHasDistance = false;
	double Distance = 0.0;
	std::string BlockedBy;
};

/**
 * LocomotionOrder — one movement, as the host receives it. No path, no waypoints, no
 * speed, no animation name, no duration: every one of those is the engine's answer to
 * HOW, and core has no surface for any of them.
 */
struct FLocomotionOrder {
	std::string Actor;
	std::string From;
	std::string To;
	std::string Mode;
	std::string Link;
	/** What core already charged the shared meter. Never a control input. */
	double Cost = 0.0;
	std::string Action;
	/** One of MovementUrgencies. Required — a half-intent would be guessed four ways. */
	std::string Urgency;
	/** One of MovementStances. Required, for the same reason. */
	std::string Stance;
	/** The vehicle carrying the movement, when one is. Identity and nothing more. */
	std::string Vehicle;
};

/**
 * ArrivalReport — what became of one movement. `Location` is the ONLY thing core
 * learns about where the actor ended up, and it is a location atom.
 *
 * `bArrived == false` is an ANSWER, not an error: core counts it, reports it and
 * eventually re-plans against it, and does NOT refund the meter for it.
 */
struct FArrivalReport {
	bool bArrived = true;
	/** Where they ended up when they did not. Empty = where they started. */
	std::string Location;
	std::string Reason;
};

/**
 * SkillModifiers — every parameter an actor's taken nodes modify, summed and keyed by
 * the AUTHORED atom (`move_speed`, `carry_capacity`). A flat list of pairs rather than
 * a map because this is what leaves core: JSON, a C ABI payload and three native
 * adapters can all carry it. A parameter the host does not recognise is IGNORED — the
 * set is open and most of what arrives belongs to somebody else.
 */
struct FSkillModifier {
	std::string Parameter;
	double Amount = 0.0;
};

using FSkillModifiers = std::vector<FSkillModifier>;

// ── The eight interfaces ────────────────────────────────────────────────────

/**
 * ICombatSystem — the `combat` module's host interface. Core decides
 * (`CombatResolver`: legality through can_attack/2, the damage pipeline, the
 * health/death transition) and calls ApplyDamage with the number it computed.
 */
class ICombatSystem {
public:
	virtual ~ICombatSystem() = default;
	virtual void RegisterEntity(const FCombatEntityData& Entity) = 0;
	virtual void UnregisterEntity(const std::string& EntityId) = 0;
	/** For a host that drives its own attacks. Core NEVER calls this. */
	virtual bool ExecuteAttack(const std::string& AttackerId, const std::string& TargetId, FDamageResult& OutResult) = 0;
	/** Apply damage core already resolved. Do not recompute it. */
	virtual void ApplyDamage(const std::string& TargetId, double Damage) = 0;
	virtual bool IsCombatEnabled() const = 0;
	virtual double GetHealth(const std::string& EntityId) const = 0;
	virtual void Heal(const std::string& EntityId, double Amount) = 0;
	virtual void Dispose() = 0;
};

/**
 * ISurvivalSystem — the `stamina` module's host interface. Core prices the action
 * (`StaminaPool` over `action/4`'s EnergyCost and the world's authored bands) and
 * hands the amount here. The needs CLOCK stays entirely the host's: Update(DeltaTime)
 * is the host ticking its own system, not core requiring a per-frame call.
 */
class ISurvivalSystem {
public:
	virtual ~ISurvivalSystem() = default;
	virtual void Update(double DeltaTime) = 0;
	virtual void RestoreNeed(const std::string& NeedType, double Amount) = 0;
	/** Consume stamina core already priced; false = insufficient. Do not re-price it. */
	virtual bool ConsumeStamina(double Amount) = 0;
	virtual void RecoverStamina(double Amount) = 0;
	virtual void SetTemperature(double Value) = 0;
	virtual void AddModifier(const FNeedModifier& Modifier) = 0;
	virtual void RemoveModifier(const std::string& ModifierId) = 0;
	virtual bool GetNeed(const std::string& NeedType, FNeedState& OutState) const = 0;
	virtual void GetAllNeeds(std::vector<FNeedState>& OutStates) const = 0;
	virtual double GetNeedPercent(const std::string& NeedType) const = 0;
	virtual bool IsAnyCritical() const = 0;
	virtual bool IsAnyWarning() const = 0;
	virtual void SetEnabled(bool bEnabled) = 0;
	virtual bool IsEnabled() const = 0;
	virtual void SetOnNeedChanged(void (*Callback)(const FNeedState&)) = 0;
	virtual void SetOnSurvivalEvent(void (*Callback)(const FSurvivalEvent&)) = 0;
	virtual void SetOnDamageFromNeed(void (*Callback)(const std::string&, double)) = 0;
	virtual void Dispose() = 0;
};

/**
 * ICombatStatSink — `EquipmentManager` reads an entity's base stats once, then writes
 * recomputed totals back whenever equipment changes. Told, never asked to decide.
 */
class ICombatStatSink {
public:
	virtual ~ICombatStatSink() = default;
	/** Unmodified stats for an entity; false when it is not in combat. */
	virtual bool GetBaseStats(const std::string& EntityId, FCombatStats& OutStats) const = 0;
	/** Apply equipment-adjusted totals. A no-op for an unknown entity. */
	virtual void ApplyStats(const std::string& EntityId, const FCombatStats& Stats) = 0;
};

/**
 * ITrajectoryProbe — the ONLY thing an engine must supply for ranged combat. A query,
 * not an executor: a host that answered `clear` to everything still cannot change one
 * number of the outcome. Unreal: LineTraceSingleByChannel.
 */
class ITrajectoryProbe {
public:
	virtual ~ITrajectoryProbe() = default;
	/** False = no answer; FInsimulMechanicHosts reads that as CLEAR, which is core's fallback. */
	virtual bool Query(const FTrajectoryQuery& InQuery, FTrajectoryReading& OutReading) = 0;
};

/**
 * IPerceptionProbe — the only thing an engine must supply for stealth. Core asks; the
 * engine answers in its own geometry; core alone decides what follows.
 * Unreal: LineTraceSingleByChannel + UAIPerceptionComponent.
 */
class IPerceptionProbe {
public:
	virtual ~IPerceptionProbe() = default;
	/** False = sensed nothing, which is NOT "saw nothing of interest". */
	virtual bool Sense(const FPerceptionQuery& InQuery, FPerceptionReading& OutReading) = 0;
};

/**
 * ITraversalProbe — could this actor get across that, from where they are now. Asked
 * once per attempt, on the decision path, never on a frame path.
 * Unreal: UNavigationSystemV1::NavigationRaycast / FindPathToLocationSynchronously.
 */
class ITraversalProbe {
public:
	virtual ~ITraversalProbe() = default;
	/** False = no answer; the fallback reads that as PASSABLE, which is core's. */
	virtual bool Query(const FTraversalQuery& InQuery, FTraversalReading& OutReading) = 0;
};

/**
 * ILocomotionHost — the `traversal` module's second interface and the `routine`
 * module's only one. Core has already decided the movement is afforded, permitted and
 * paid for; this carries it out. Unreal: AAIController::MoveTo.
 */
class ILocomotionHost {
public:
	virtual ~ILocomotionHost() = default;
	/** False = the host could not take the order at all; the fallback reads that as ARRIVED. */
	virtual bool Travel(const FLocomotionOrder& Order, FArrivalReport& OutReport) = 0;
};

/**
 * ISkillModifierSink — the narrowest interface in the band. Told the whole current
 * set, absolute rather than incremental, and re-applying the same totals twice must be
 * a no-op. Unreal: a UCharacterMovementComponent field (this plugin has no GAS — §2.3).
 */
class ISkillModifierSink {
public:
	virtual ~ISkillModifierSink() = default;
	/** Apply this actor's whole current set. Idempotent, and never read back. */
	virtual void ApplyModifiers(const std::string& ActorId, const FSkillModifiers& Modifiers) = 0;
};

} // namespace insimul
