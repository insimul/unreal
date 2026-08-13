// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulMechanicHostBinder — the one reflected surface of the band-120 host half
// (US-1 of tasklist 146, RUNTIME_CORE_ADOPTION.md §12).
//
// WHAT IT IS. A game instance subsystem that constructs the eight host
// implementations, hands them to insimul::FInsimulMechanicHosts, and — at boot — asks
// the shipped libinsimulcore what it can actually do and logs the answer per module.
// Everything else in templates/source/mechanics/ is plain C++ deliberately: one
// Blueprint surface for a creator, and hosts that a plain test can drive.
//
// THE BOOT LOG IS THE POINT. A creator who sees a combat host component in their scene
// will assume combat is wired. It is not, in any build that exists today: the bridge
// carries no mechanic rows, so `CombatResolver` cannot be reached from C++ at all and
// the host half is implemented and INERT. `LogMechanicSurface()` says exactly that,
// per module, with the rows that are missing and where they would be added. A silent
// component is the failure this whole story exists to prevent.
//
// WIRING, IN ORDER:
//   1. RegisterActor / RegisterLocation — bind core's atoms to bodies and places.
//   2. RegisterCombatEntity — put an entity on the roster core will name.
//   3. everything else is automatic; the hosts read the registry live.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "../data/GameTypes.h"
#include "InsimulActorRegistry.h"
#include "InsimulCombatHost.h"
#include "InsimulGeometryProbes.h"
#include "InsimulLocomotionHost.h"
#include "InsimulMechanicHosts.h"
#include "InsimulSkillModifierSink.h"
#include "InsimulSurvivalHost.h"
#include "InsimulMechanicHostBinder.generated.h"

class UInsimulRadiantSourceShell;
class USurvivalSystem;

/**
 * Builds and owns this game's host half of the band-120 mechanic boundary.
 */
UCLASS()
class INSIMULEXPORT_API UInsimulMechanicHostBinder : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Bind an entity atom core will name (`nessa`, `player`) to its body. */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Mechanics")
    void RegisterActor(const FString& Atom, AActor* Actor);

    /** Forget an entity atom — a despawn, a level change. */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Mechanics")
    void UnregisterActor(const FString& Atom);

    /** Bind a location atom (`forge_gate`) to a place in this level. */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Mechanics")
    void RegisterLocation(const FString& Atom, const FVector& Where);

    /** Put an entity on the combat roster, with the health and base stats the world
     *  authored. Core's own CombatEntityData carries no stats, so they are separate
     *  arguments here rather than invented from the health. */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Mechanics")
    void RegisterCombatEntity(const FString& EntityId, const FString& DisplayName,
        float Health, float MaxHealth, float AttackPower, float Defense, float DodgeChance);

    /** Current health on the roster — what ApplyDamage last left. */
    UFUNCTION(BlueprintPure, Category = "Insimul|Mechanics")
    float GetEntityHealth(const FString& EntityId) const;

    /**
     * Ask the shipped libinsimulcore what it can do and log one line per band-120
     * module. Runs once at Initialize; call it again after swapping a world.
     * Returns the number of modules that are actually reachable — 0 in every build
     * that exists today, and the log says why.
     */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Mechanics")
    int32 LogMechanicSurface();

    /** The host half, for a system that needs to ask a probe directly. Its fallbacks
     *  are core's documented ones, implemented once. */
    const insimul::FInsimulMechanicHosts& Hosts() const { return MechanicHosts; }

private:
    /** Forwards USurvivalSystem's one Unreal delegate into the three callbacks core's
     *  ISurvivalSystem registers. */
    UFUNCTION()
    void HandleSurvivalEvent(const FInsimulSurvivalEvent& Event);

    /** Core's own SurvivalEvent.type spelling for one of this template's enum values. */
    static FString SurvivalEventAtom(EInsimulSurvivalEventType EventType);

    /** Core's NeedType atom for one of this template's enum values. */
    static FString NeedTypeAtom(EInsimulNeedType NeedType);

    /** Holds the plugin's one libinsimulcore runtime; borrowed, never a second one. */
    UPROPERTY()
    TObjectPtr<UInsimulRadiantSourceShell> CoreShell;

    FInsimulActorRegistry Registry;
    TUniquePtr<FInsimulCombatHost> CombatHost;
    TUniquePtr<FInsimulTrajectoryProbe> TrajectoryProbe;
    TUniquePtr<FInsimulPerceptionProbe> PerceptionProbe;
    TUniquePtr<FInsimulTraversalProbe> TraversalProbe;
    TUniquePtr<FInsimulLocomotionHost> LocomotionHost;
    TUniquePtr<FInsimulSkillModifierSink> SkillSink;
    TUniquePtr<FInsimulSurvivalHost> SurvivalHost;

    insimul::FInsimulMechanicHosts MechanicHosts;
};
