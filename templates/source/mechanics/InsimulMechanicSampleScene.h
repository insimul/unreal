// Copyright 2024 Insimul. All Rights Reserved.
//
// AInsimulMechanicSampleScene — the playable scene US-3 of tasklist 146 ships: a
// guard, a dark courtyard and a wall (RUNTIME_CORE_ADOPTION.md §14.4).
//
// WHAT IT DEMONSTRATES, END TO END. Two adopted mechanics, in the only shape an
// adoption can honestly take today:
//
//   the ENGINE measures   — FInsimulPerceptionProbe line-traces from the guard's eye
//                           to the player and reports the distance; the traversal
//                           probe reports the movement mode at the wall;
//   CORE decides          — its own rule packs, consulted into libinsimul because
//                           this world's genre bundle selected the modules that own
//                           them, answer `detects/2` and `can_traverse/3`;
//   the ENGINE executes   — the guard goes on alert or stays on patrol, the climb
//                           happens or the refusal plays.
//
// Not one rule is re-expressed in C++. The decision layers (`DetectionTracker`,
// `TraversalPlanner`) are still unreachable — the bridge carries no mechanic rows,
// and UInsimulMechanicHostBinder's boot log says so per module — so what this scene
// exercises is the PREDICATE half of each module. §14.4 states that boundary rather
// than blurring it.
//
// THE SCRIPT IS A FILE, WHICH IS WHY THE CLAIM IS CHECKABLE. The steps come from
// `Content/Data/insimul/scenarios/dark-courtyard.json`, read by
// insimul::FInsimulMechanicScenario and executed by insimul::RunScenario. ctest
// `activation_witness` reads the SAME file with the SAME reader and runs it through
// the SAME libinsimul, so "the scene exercises two mechanics end to end" is a gate
// rather than a screenshot. What no gate here can do is move an actor: VERIFICATION.md
// US-M2 is the human pass, and it is the pass that covers this file.
//
// DROP IT IN A LEVEL. Place the actor, point GuardActor / PlayerActor at two pawns
// (or leave them empty and it registers its own transform for both, which still
// exercises every decision), press Play, and read the log. `bRunOnBeginPlay` off
// turns it into a Blueprint-callable demo.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InsimulMechanicScenario.h"
#include "InsimulMechanicSampleScene.generated.h"

class UInsimulMechanicHostBinder;
class UInsimulModuleActivator;

/** What one executed step did, flattened for Blueprint and the on-screen readout. */
USTRUCT(BlueprintType)
struct FInsimulSampleSceneStep
{
    GENERATED_BODY()

    /** The step's own name, from the scenario file. */
    UPROPERTY(BlueprintReadOnly, Category = "Insimul|Mechanics")
    FString Name;

    /** The module it exercises. */
    UPROPERTY(BlueprintReadOnly, Category = "Insimul|Mechanics")
    FString Mechanic;

    /** The goal core was asked. */
    UPROPERTY(BlueprintReadOnly, Category = "Insimul|Mechanics")
    FString Goal;

    /** True when core answered what the scene expected and the scene acted. */
    UPROPERTY(BlueprintReadOnly, Category = "Insimul|Mechanics")
    bool bMatched = false;

    /** Whether the goal HELD — what the scene actually acted on. */
    UPROPERTY(BlueprintReadOnly, Category = "Insimul|Mechanics")
    bool bHolds = false;

    /** What the scene did about it, or why it could not. */
    UPROPERTY(BlueprintReadOnly, Category = "Insimul|Mechanics")
    FString Outcome;
};

/**
 * A self-contained demonstration of two adopted mechanics deciding a scene.
 */
UCLASS()
class INSIMULEXPORT_API AInsimulMechanicSampleScene : public AActor
{
    GENERATED_BODY()

public:
    AInsimulMechanicSampleScene();

    virtual void BeginPlay() override;

    /** Scenario id under Content/Data/insimul/scenarios (without the .json). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Mechanics")
    FString ScenarioId = TEXT("dark-courtyard");

    /** Run at BeginPlay. Off makes this a Blueprint-callable demo. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Mechanics")
    bool bRunOnBeginPlay = true;

    /** The body behind the observer atom. Empty registers this actor for it. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Mechanics")
    TObjectPtr<AActor> GuardActor;

    /** The body behind the subject atom. Empty registers this actor for it. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Mechanics")
    TObjectPtr<AActor> PlayerActor;

    /**
     * Where the two location atoms this scenario names sit in the level. Empty falls
     * back to this actor's transform, which still exercises every decision.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Mechanics")
    TObjectPtr<AActor> LowerPlace;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Mechanics")
    TObjectPtr<AActor> UpperPlace;

    /**
     * Use the live probes for the scenario's host readings. Off replays the readings
     * recorded in the file — useful for reproducing the gate's answer in the editor,
     * and reported as a replay rather than passed off as a measurement.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Mechanics")
    bool bUseLiveProbes = true;

    /**
     * Read the scenario, set the world up, ask core, and act on every answer.
     * @return true when every step answered what the scene acts on.
     */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Mechanics")
    bool RunScene();

    /** The last run, step by step. */
    UFUNCTION(BlueprintPure, Category = "Insimul|Mechanics")
    const TArray<FInsimulSampleSceneStep>& LastRun() const { return Steps; }

private:
    /** Register the scenario's atoms against the level's bodies and places. */
    void BindAtoms(UInsimulMechanicHostBinder& Binder);

    /** The scene's reaction to one answered step — this is the "end" of end to end. */
    FString Act(const insimul::FInsimulScenarioStepResult& Result);

    UInsimulMechanicHostBinder* ResolveBinder() const;
    UInsimulModuleActivator* ResolveActivator() const;

    UPROPERTY()
    TArray<FInsimulSampleSceneStep> Steps;
};
