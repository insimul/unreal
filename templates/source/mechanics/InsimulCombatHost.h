// Copyright 2024 Insimul. All Rights Reserved.
//
// FInsimulCombatHost — ICombatSystem and ICombatStatSink, over one entity roster
// (US-1 of tasklist 146, RUNTIME_CORE_ADOPTION.md §12.4).
//
// TWO INTERFACES, ONE CLASS, BECAUSE THEY ARE ONE THING. `ICombatSystem.applyDamage`
// writes health on an entity and `ICombatStatSink.applyStats` writes attackPower /
// defense / dodgeChance on the same entity. RUNTIME_CORE_ADOPTION.md §2.3 item 1
// recorded that this engine had no entity registry to look an id up in, so
// `getBaseStats(entityId)` had nothing to read; that is what this class fixes, and it
// fixes both halves at once or neither.
//
// NO SECOND DAMAGE PIPELINE. `ApplyDamage` takes a number `CombatResolver` resolved
// deterministically from (attacker, defender, action, tuning, separation, seed, tick)
// and SUBTRACTS it. It does not roll, crit, block or dodge — "an adapter must not roll
// its own damage" (`system-contracts.ts`), and an engine that recomputes it makes the
// same save mean two things.
//
// `ExecuteAttack` DELIBERATELY REFUSES. Core never calls it; it exists for a host that
// drives its own attacks without asking core. Implementing it here would be exactly
// the second damage pipeline the paragraph above forbids, and §2.3 item 1's "do not
// let the stub imply combat is wired" applies to a rolled CalculateDamage more than to
// anything else. It returns false and warns once. UCombatSystem's own
// `CalculateDamage` is left where it is: it is the template's pre-adoption behaviour
// and this class does not call it.
//
// UCombatSystem (templates/source/systems/CombatSystem.h) stays the holder of the
// world's authored combat CONFIGURATION — style, base damage, crit chance — which is
// what `LoadFromIR` reads. This class is the roster and the execution surface.

#pragma once

#include "CoreMinimal.h"
#include "InsimulMechanicContracts.h"

/**
 * The combat roster this engine did not have. One record per entity core may name, and
 * both host interfaces write to it.
 */
struct FInsimulCombatEntity
{
    FString Id;
    FString Name;
    double Health = 0.0;
    double MaxHealth = 0.0;
    /** Base stats, as the host knows them before equipment. */
    insimul::FCombatStats Base;
    /** Equipment-adjusted totals, once ICombatStatSink has been told any. */
    insimul::FCombatStats Current;
    bool bHasStats = false;
};

/**
 * Carries out combat decisions core already made, and holds the stats equipment
 * modifies. Constructed by UInsimulMechanicHostBinder; not a UObject, so the whole
 * class is drivable from a plain test.
 */
class FInsimulCombatHost : public insimul::ICombatSystem, public insimul::ICombatStatSink
{
public:
    /** Whether this world authored combat at all (`ir.combat`). A false makes
     *  IsCombatEnabled false and every ApplyDamage a no-op, which is the honest answer
     *  for a world with no combat rather than a silent zero. */
    explicit FInsimulCombatHost(bool bInCombatEnabled) : bCombatEnabled(bInCombatEnabled) {}

    // ── ICombatSystem ────────────────────────────────────────────────────────
    virtual void RegisterEntity(const insimul::FCombatEntityData& Entity) override;
    virtual void UnregisterEntity(const std::string& EntityId) override;
    virtual bool ExecuteAttack(const std::string& AttackerId, const std::string& TargetId, insimul::FDamageResult& OutResult) override;
    virtual void ApplyDamage(const std::string& TargetId, double Damage) override;
    virtual bool IsCombatEnabled() const override;
    virtual double GetHealth(const std::string& EntityId) const override;
    virtual void Heal(const std::string& EntityId, double Amount) override;
    virtual void Dispose() override;

    // ── ICombatStatSink ──────────────────────────────────────────────────────
    virtual bool GetBaseStats(const std::string& EntityId, insimul::FCombatStats& OutStats) const override;
    virtual void ApplyStats(const std::string& EntityId, const insimul::FCombatStats& Stats) override;

    /** Seed an entity's UNMODIFIED stats — what the world authored, before equipment.
     *  Separate from RegisterEntity because core's CombatEntityData carries no stats. */
    void SetBaseStats(const FString& EntityId, const insimul::FCombatStats& Base);

    /** The roster, for a diagnostic and for the binder's boot log. */
    const TMap<FString, FInsimulCombatEntity>& Entities() const { return Roster; }

private:
    FInsimulCombatEntity* Find(const std::string& EntityId);
    const FInsimulCombatEntity* Find(const std::string& EntityId) const;

    TMap<FString, FInsimulCombatEntity> Roster;
    bool bCombatEnabled = false;
    bool bWarnedAboutExecuteAttack = false;
};
