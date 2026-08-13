// Copyright 2024 Insimul. All Rights Reserved.
//
// See InsimulMechanicSampleScene.h. Nothing here names a mechanic — the scenario file
// does — and tools/verify-mechanics/check-activation.mjs fails if one ever appears.

#include "InsimulMechanicSampleScene.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "InsimulMechanicHostBinder.h"
#include "InsimulModuleActivator.h"
#include "InsimulPrologSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogInsimulSampleScene, Log, All);

namespace
{
    FString ToFString(const std::string& Text)
    {
        return FString(UTF8_TO_TCHAR(Text.c_str()));
    }

    std::string ToStdString(const FString& Text)
    {
        return std::string(TCHAR_TO_UTF8(*Text));
    }

    /**
     * The scenario runner's KB, over the plugin's own Prolog subsystem. Nothing here
     * evaluates Prolog: it marshals strings and keeps a RAISED goal apart from one
     * that simply did not hold.
     */
    class FSceneKb : public insimul::IInsimulScenarioKb
    {
    public:
        explicit FSceneKb(UInsimulPrologSubsystem& InProlog) : Prolog(InProlog) {}

        virtual bool Assert(const std::string& Clause, std::string& OutError) override
        {
            if (Prolog.AssertFact(ToFString(Clause)))
            {
                return true;
            }
            OutError = ToStdString(Prolog.GetLastError());
            return false;
        }

        virtual bool Retract(const std::string& Clause, std::string& OutError) override
        {
            // A clause that matched nothing is NOT an error (the runner's contract),
            // and RetractFact reports exactly that as false — so a retract that found
            // nothing is only a failure when the KB also has an error to show for it.
            if (Prolog.RetractFact(ToFString(Clause)))
            {
                return true;
            }
            const FString Error = Prolog.GetLastError();
            if (Error.IsEmpty())
            {
                return true;
            }
            OutError = ToStdString(Error);
            return false;
        }

        virtual bool Ask(const std::string& Goal, bool& bOutHolds, std::string& OutError) override
        {
            TArray<FInsimulPrologBinding> Solutions;
            if (!Prolog.QueryAll(ToFString(Goal), Solutions))
            {
                // QueryAll answers false only when the goal failed to START, which is
                // the RAISED case — a goal with zero solutions returns true and an
                // empty array. Conflating the two is what would let this scene show a
                // player a refusal core never decided (§14.3).
                OutError = ToStdString(Prolog.GetLastError());
                return false;
            }
            bOutHolds = Solutions.Num() > 0;
            return true;
        }

    private:
        UInsimulPrologSubsystem& Prolog;
    };

    /**
     * The scenario's host readings, taken from THIS level's geometry.
     *
     * Three answers, and they are three different things (see the runner's contract):
     * a clause the probe measured to hold; nothing at all, when the probe could not
     * answer (the step is then HostSilent rather than run against a stale value); and
     * the empty string, when the probe measured and the fact does NOT hold — which is
     * exactly the case where the scene's own geometry changes core's answer.
     */
    class FProbeSupplier : public insimul::IInsimulScenarioHostSupplier
    {
    public:
        explicit FProbeSupplier(const insimul::FInsimulMechanicHosts& InHosts) : Hosts(InHosts) {}

        virtual bool Supply(const insimul::FInsimulScenarioHostFact& Declared, std::string& OutClause) override
        {
            if (Declared.Probe == "lineOfSight")
            {
                insimul::FPerceptionQuery Query;
                Query.Observer = Declared.Arg("observer");
                Query.Target = Declared.Arg("target");
                insimul::FPerceptionReading Reading;
                if (!Hosts.Sense(Query, Reading))
                {
                    return false; // no reading — not "nothing was visible"
                }
                OutClause = Reading.Visibility > 0.0 ? Declared.Fact : std::string();
                return true;
            }

            if (Declared.Probe == "near")
            {
                // Separation is what the engine can measure; whether that is NEAR is
                // the world's authored range, which the scenario carries and core
                // reads. This host compares, it does not decide what a range is worth.
                insimul::FTrajectoryQuery Query;
                Query.Attacker = Declared.Arg("observer");
                Query.Target = Declared.Arg("target");
                const std::string Range = Declared.Arg("range");
                if (Range.empty())
                {
                    return false;
                }
                Query.bHasRange = true;
                Query.Range = FCString::Atod(*ToFString(Range));
                const insimul::FTrajectoryReading Reading = Hosts.Ask(Query);
                if (!Reading.bHasSeparation)
                {
                    return false;
                }
                OutClause = Reading.Separation <= Query.Range ? Declared.Fact : std::string();
                return true;
            }

            if (Declared.Probe == "movementMode")
            {
                insimul::FTraversalQuery Query;
                Query.Actor = Declared.Arg("actor");
                Query.From = Declared.Arg("from");
                Query.To = Declared.Arg("to");
                Query.Mode = Declared.Arg("mode");
                insimul::FTraversalReading Reading;
                if (!Hosts.Adapter.Traversal || !Hosts.Adapter.Traversal->Query(Query, Reading))
                {
                    return false;
                }
                OutClause = Reading.bPassable ? Declared.Fact : std::string();
                return true;
            }

            // A probe this scene does not know how to take is SILENT, never replayed
            // as though it had been measured.
            UE_LOG(LogInsimulSampleScene, Warning,
                TEXT("[Insimul] no probe in this scene takes the reading '%s' that %s declares"),
                *ToFString(Declared.Probe), *ToFString(Declared.From));
            return false;
        }

    private:
        const insimul::FInsimulMechanicHosts& Hosts;
    };
}

AInsimulMechanicSampleScene::AInsimulMechanicSampleScene()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AInsimulMechanicSampleScene::BeginPlay()
{
    Super::BeginPlay();

    if (bRunOnBeginPlay)
    {
        RunScene();
    }
}

UInsimulMechanicHostBinder* AInsimulMechanicSampleScene::ResolveBinder() const
{
    UGameInstance* GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
    return GameInstance != nullptr ? GameInstance->GetSubsystem<UInsimulMechanicHostBinder>() : nullptr;
}

UInsimulModuleActivator* AInsimulMechanicSampleScene::ResolveActivator() const
{
    UGameInstance* GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
    return GameInstance != nullptr ? GameInstance->GetSubsystem<UInsimulModuleActivator>() : nullptr;
}

void AInsimulMechanicSampleScene::BindAtoms(UInsimulMechanicHostBinder& Binder)
{
    // The atoms are the scenario's; the bodies are this level's. A body the level did
    // not supply falls back to this actor, so the scene still runs in an empty map —
    // the decisions are the point, not the art.
    AActor* Guard = GuardActor != nullptr ? GuardActor.Get() : this;
    AActor* Player = PlayerActor != nullptr ? PlayerActor.Get() : this;
    Binder.RegisterActor(TEXT("guard"), Guard);
    Binder.RegisterActor(TEXT("player"), Player);

    const FVector Lower = LowerPlace != nullptr ? LowerPlace->GetActorLocation() : GetActorLocation();
    const FVector Upper = UpperPlace != nullptr ? UpperPlace->GetActorLocation() : GetActorLocation() + FVector(0, 0, 400);
    Binder.RegisterLocation(TEXT("courtyard"), Lower);
    Binder.RegisterLocation(TEXT("rooftop"), Upper);
}

FString AInsimulMechanicSampleScene::Act(const insimul::FInsimulScenarioStepResult& Result)
{
    switch (Result.Outcome)
    {
    case insimul::EInsimulScenarioOutcome::Matched:
        // THE END OF END-TO-END. In a shipped game this is where the alarm sounds, the
        // climb montage plays or the refusal barks. Here it is logged, because a
        // template cannot know what the creator's guard looks like — and because a
        // logged reaction is one a gate and a human can both read.
        return ToFString(Result.Step.Then);

    case insimul::EInsimulScenarioOutcome::Mismatched:
        return FString::Printf(
            TEXT("core answered '%s' and the scene expected it to %s — the scene did NOT act"),
            Result.bHolds ? TEXT("yes") : TEXT("no"), *ToFString(Result.Step.Expect));

    case insimul::EInsimulScenarioOutcome::Raised:
        // Never acted on. A raised goal is not a refusal, and showing the player one
        // would be a decision core never made (§14.3).
        return FString::Printf(TEXT("the goal RAISED (%s) — nothing was decided, so nothing happened"),
            *ToFString(Result.Detail));

    case insimul::EInsimulScenarioOutcome::HostSilent:
        return FString::Printf(TEXT("no reading: %s"), *ToFString(Result.Detail));

    case insimul::EInsimulScenarioOutcome::SetupFailed:
    default:
        return FString::Printf(TEXT("the world could not be set up: %s"), *ToFString(Result.Detail));
    }
}

bool AInsimulMechanicSampleScene::RunScene()
{
    Steps.Reset();

    UInsimulMechanicHostBinder* Binder = ResolveBinder();
    UInsimulModuleActivator* Activator = ResolveActivator();
    UGameInstance* GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
    UInsimulPrologSubsystem* Prolog = GameInstance != nullptr
        ? GameInstance->GetSubsystem<UInsimulPrologSubsystem>()
        : nullptr;

    if (Binder == nullptr || Activator == nullptr || Prolog == nullptr || !Prolog->IsPrologReady())
    {
        UE_LOG(LogInsimulSampleScene, Error,
            TEXT("[Insimul] the sample scene needs the host binder, the module activator and a ready Prolog KB; ")
            TEXT("one of them is missing, so nothing was asked of core."));
        return false;
    }
    if (!Activator->IsResolved())
    {
        UE_LOG(LogInsimulSampleScene, Error,
            TEXT("[Insimul] no active module set was resolved, so the rule packs this scene needs were never ")
            TEXT("consulted — see the LogInsimulActivation errors above."));
        return false;
    }

    const FString Path = FPaths::Combine(
        UInsimulModuleActivator::DataRoot(), TEXT("scenarios"), ScenarioId + TEXT(".json"));
    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *Path))
    {
        UE_LOG(LogInsimulSampleScene, Error, TEXT("[Insimul] no scenario at %s"), *Path);
        return false;
    }

    insimul::FInsimulMechanicScenario Scenario;
    std::string Error;
    if (!insimul::FInsimulMechanicScenario::Parse(ToStdString(Json), Scenario, Error))
    {
        UE_LOG(LogInsimulSampleScene, Error, TEXT("[Insimul] %s is not a scenario: %s"), *Path, *ToFString(Error));
        return false;
    }

    // A scenario written for another genre would ask for vocabulary this world never
    // consulted. That is a legible error, not a pile of existence_errors.
    if (!Scenario.Genre.empty() && Scenario.Genre != ToStdString(Activator->GetActiveGenre()))
    {
        UE_LOG(LogInsimulSampleScene, Warning,
            TEXT("[Insimul] scenario '%s' is written for genre '%s' and this world activated '%s' — its steps may ")
            TEXT("name vocabulary that was never consulted."),
            *ScenarioId, *ToFString(Scenario.Genre), *Activator->GetActiveGenre());
    }

    BindAtoms(*Binder);

    FSceneKb Kb(*Prolog);
    FProbeSupplier Supplier(Binder->Hosts());
    const insimul::FInsimulScenarioReport Report =
        insimul::RunScenario(Scenario, Kb, bUseLiveProbes ? &Supplier : nullptr);

    UE_LOG(LogInsimulSampleScene, Log, TEXT("[Insimul] %s (%s)"),
        *ToFString(Report.Describe()),
        bUseLiveProbes ? TEXT("live probe readings") : TEXT("REPLAYED readings from the scenario file"));

    for (const insimul::FInsimulScenarioStepResult& Result : Report.Steps)
    {
        FInsimulSampleSceneStep Step;
        Step.Name = ToFString(Result.Step.Name);
        Step.Mechanic = ToFString(Result.Step.Mechanic);
        Step.Goal = ToFString(Result.Step.Goal);
        Step.bMatched = Result.Outcome == insimul::EInsimulScenarioOutcome::Matched;
        Step.bHolds = Result.bHolds;
        Step.Outcome = Act(Result);
        Steps.Add(Step);

        for (const std::string& Used : Result.HostFactsUsed)
        {
            UE_LOG(LogInsimulSampleScene, Verbose, TEXT("[Insimul]   host reading: %s"), *ToFString(Used));
        }
        for (const std::string& NotHolding : Result.HostFactsNotHolding)
        {
            UE_LOG(LogInsimulSampleScene, Verbose,
                TEXT("[Insimul]   host measured that '%s' does NOT hold — core is answering without it"),
                *ToFString(NotHolding));
        }
        if (Step.bMatched)
        {
            UE_LOG(LogInsimulSampleScene, Log, TEXT("[Insimul]   [%s] %s → %s"),
                *Step.Mechanic, *Step.Name, *Step.Outcome);
        }
        else
        {
            UE_LOG(LogInsimulSampleScene, Warning, TEXT("[Insimul]   [%s] %s: %s"),
                *Step.Mechanic, *Step.Name, *Step.Outcome);
        }
    }

    for (const std::string& SetupError : Report.SetupErrors)
    {
        UE_LOG(LogInsimulSampleScene, Error, TEXT("[Insimul]   %s"), *ToFString(SetupError));
    }

    const std::vector<std::string> Exercised = Report.MechanicsExercised();
    UE_LOG(LogInsimulSampleScene, Log,
        TEXT("[Insimul] %d mechanic(s) exercised end to end: %s"),
        static_cast<int32>(Exercised.size()), *ToFString(insimul::JoinNames(Exercised)));

    return Report.IsOk();
}
