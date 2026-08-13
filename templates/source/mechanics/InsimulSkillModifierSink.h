// Copyright 2024 Insimul. All Rights Reserved.
//
// FInsimulSkillModifierSink — ISkillModifierSink, the narrowest interface in the band
// (US-1 of tasklist 146, RUNTIME_CORE_ADOPTION.md §12.4).
//
// TOLD, NEVER ASKED. Core computes the totals — integer sums over the nodes an actor
// has taken, canonically ordered — and hands the WHOLE current set, not a delta.
// Re-applying the same set twice is a no-op, which is what makes a save restore and a
// replayed unlock safe. Nothing here reads a total back.
//
// A PARAMETER THIS HOST DOES NOT RECOGNISE IS IGNORED, exactly as core's own
// `withSkillModifiers` ignores one that names no field: the parameter set is open and
// most of what arrives belongs to somebody else. Ignored is not silent, though — every
// unapplied parameter is kept in `Unapplied()` and the binder announces it once, so a
// creator who authored `reach` and saw nothing happen can find out why.
//
// WHAT LANDS, AND WHAT DOES NOT, IN THIS TEMPLATE:
//
//   move_speed      APPLIED. UCharacterMovementComponent::MaxWalkSpeed, as a multiple
//                   of the actor's own authored base — an absolute cm/s here would be
//                   core deciding a speed it cannot see.
//   carry_capacity  NOT APPLIED, and announced. InventorySystem.cpp limits an inventory
//                   by `MaxSlots` and sums no weight, though FInsimulItem carries one.
//                   Applying it would mean inventing a limit no rule in this template
//                   reads, which is worse than not applying it. The Unity probe found
//                   the identical gap; it is an engine-side to-do, not a contract one.
//   anything else   NOT APPLIED, and announced.

#pragma once

#include "CoreMinimal.h"
#include "InsimulActorRegistry.h"
#include "InsimulMechanicContracts.h"

/**
 * Applies the skill effects whose subject this engine owns. Constructed by
 * UInsimulMechanicHostBinder with the registry the probes use.
 */
class FInsimulSkillModifierSink : public insimul::ISkillModifierSink
{
public:
    explicit FInsimulSkillModifierSink(const FInsimulActorRegistry* InRegistry) : Registry(InRegistry) {}

    virtual void ApplyModifiers(const std::string& ActorId, const insimul::FSkillModifiers& Modifiers) override;

    /** Every parameter that arrived and reached nothing, deduplicated. Read by the
     *  binder's boot log — an ignored effect a creator cannot see is the silent no-op
     *  US-1 forbids. */
    const TArray<FString>& Unapplied() const { return UnappliedParameters; }

    /** The last set applied for an actor, so a re-registration can re-apply it. */
    const TMap<FString, TMap<FString, double>>& Applied() const { return LastApplied; }

private:
    /** The base MaxWalkSpeed an actor had before any modifier — captured once, so
     *  applying a set twice cannot compound. */
    TMap<FString, double> BaseWalkSpeeds;
    TMap<FString, TMap<FString, double>> LastApplied;
    TArray<FString> UnappliedParameters;
    const FInsimulActorRegistry* Registry = nullptr;
};
