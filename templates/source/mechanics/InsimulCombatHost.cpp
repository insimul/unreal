// Copyright 2024 Insimul. All Rights Reserved.

#include "InsimulCombatHost.h"

DEFINE_LOG_CATEGORY_STATIC(LogInsimulMechanics, Log, All);

namespace
{
    FString ToFString(const std::string& Text)
    {
        return FString(UTF8_TO_TCHAR(Text.c_str()));
    }
}

void FInsimulCombatHost::RegisterEntity(const insimul::FCombatEntityData& Entity)
{
    if (Entity.Id.empty())
    {
        return;
    }
    const FString Key = ToFString(Entity.Id);
    FInsimulCombatEntity& Record = Roster.FindOrAdd(Key);
    Record.Id = Key;
    Record.Name = ToFString(Entity.Name);
    Record.Health = Entity.Health;
    Record.MaxHealth = Entity.MaxHealth;
    if (Entity.bHasDamage && !Record.bHasStats)
    {
        // Core's CombatEntityData carries one optional number and calls it `damage`.
        // It is the entity's own attack power; nothing else in the record is a stat.
        Record.Base.AttackPower = Entity.Damage;
        Record.Current = Record.Base;
        Record.bHasStats = true;
    }
}

void FInsimulCombatHost::UnregisterEntity(const std::string& EntityId)
{
    Roster.Remove(ToFString(EntityId));
}

bool FInsimulCombatHost::ExecuteAttack(const std::string& AttackerId, const std::string& TargetId, insimul::FDamageResult& OutResult)
{
    (void)OutResult;
    if (!bWarnedAboutExecuteAttack)
    {
        bWarnedAboutExecuteAttack = true;
        UE_LOG(LogInsimulMechanics, Warning,
            TEXT("[Insimul] ExecuteAttack(%s -> %s) refused. Core never calls it, and rolling damage here ")
            TEXT("would be a second damage pipeline (RUNTIME_CORE_ADOPTION.md §12.4). Ask core to resolve ")
            TEXT("the attack and call ApplyDamage with the number it returns."),
            *ToFString(AttackerId), *ToFString(TargetId));
    }
    return false;
}

void FInsimulCombatHost::ApplyDamage(const std::string& TargetId, double Damage)
{
    if (!bCombatEnabled)
    {
        return;
    }
    FInsimulCombatEntity* Record = Find(TargetId);
    if (Record == nullptr)
    {
        return;
    }
    // Subtract, and nothing else. No crit, no block, no dodge: core already applied
    // every one of those before it handed us this number.
    Record->Health = FMath::Max(0.0, Record->Health - Damage);
}

bool FInsimulCombatHost::IsCombatEnabled() const
{
    return bCombatEnabled;
}

double FInsimulCombatHost::GetHealth(const std::string& EntityId) const
{
    const FInsimulCombatEntity* Record = Find(EntityId);
    return Record != nullptr ? Record->Health : 0.0;
}

void FInsimulCombatHost::Heal(const std::string& EntityId, double Amount)
{
    FInsimulCombatEntity* Record = Find(EntityId);
    if (Record == nullptr || Amount <= 0.0)
    {
        return;
    }
    Record->Health = FMath::Min(Record->MaxHealth, Record->Health + Amount);
}

void FInsimulCombatHost::Dispose()
{
    Roster.Reset();
}

bool FInsimulCombatHost::GetBaseStats(const std::string& EntityId, insimul::FCombatStats& OutStats) const
{
    const FInsimulCombatEntity* Record = Find(EntityId);
    if (Record == nullptr || !Record->bHasStats)
    {
        // "or undefined if it is not in combat" — core's own words. An entity with no
        // authored stats reads as absent rather than as three zeroes, so
        // EquipmentManager totals a loadout it can actually add to.
        return false;
    }
    OutStats = Record->Base;
    return true;
}

void FInsimulCombatHost::ApplyStats(const std::string& EntityId, const insimul::FCombatStats& Stats)
{
    FInsimulCombatEntity* Record = Find(EntityId);
    if (Record == nullptr)
    {
        // "A no-op for an unknown entity" — core's own words.
        return;
    }
    // Absolute totals, not a delta: re-applying the same set twice is a no-op.
    Record->Current = Stats;
    Record->bHasStats = true;
}

void FInsimulCombatHost::SetBaseStats(const FString& EntityId, const insimul::FCombatStats& Base)
{
    if (EntityId.IsEmpty())
    {
        return;
    }
    FInsimulCombatEntity& Record = Roster.FindOrAdd(EntityId);
    Record.Id = EntityId;
    Record.Base = Base;
    Record.Current = Base;
    Record.bHasStats = true;
}

FInsimulCombatEntity* FInsimulCombatHost::Find(const std::string& EntityId)
{
    return Roster.Find(ToFString(EntityId));
}

const FInsimulCombatEntity* FInsimulCombatHost::Find(const std::string& EntityId) const
{
    return Roster.Find(ToFString(EntityId));
}
