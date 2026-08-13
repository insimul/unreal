// Copyright 2024 Insimul. All Rights Reserved.

#include "InsimulMechanicHostBinder.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "InsimulMechanicSurface.h"
#include "InsimulModuleActivator.h"
#include "InsimulRadiantSourceShell.h"
#include "../systems/SurvivalSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogInsimulMechanicSurface, Log, All);

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
}

void UInsimulMechanicHostBinder::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // The active module set has to exist before the hosts are restricted to it, and
    // subsystem initialisation order is otherwise unspecified.
    Collection.InitializeDependency<UInsimulModuleActivator>();

    UWorld* World = GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr;
    USurvivalSystem* Survival = GetGameInstance() != nullptr
        ? GetGameInstance()->GetSubsystem<USurvivalSystem>()
        : nullptr;

    FInsimulProbeContext Context;
    Context.World = World;
    Context.Registry = &Registry;

    // A world with no authored survival needs has no combat either in this template's
    // export; NeedCount is the closest thing to "the world authored this" that is
    // available at Initialize, so combat is enabled unless the game says otherwise.
    CombatHost = MakeUnique<FInsimulCombatHost>(/*bInCombatEnabled=*/true);
    TrajectoryProbe = MakeUnique<FInsimulTrajectoryProbe>(Context);
    PerceptionProbe = MakeUnique<FInsimulPerceptionProbe>(Context);
    TraversalProbe = MakeUnique<FInsimulTraversalProbe>(Context);
    LocomotionHost = MakeUnique<FInsimulLocomotionHost>(World, &Registry);
    SkillSink = MakeUnique<FInsimulSkillModifierSink>(&Registry);
    SurvivalHost = MakeUnique<FInsimulSurvivalHost>(Survival);

    // Two containers, because core has two (`EngineHostAdapter` vs the systems the
    // engine owns and wires into a decision layer's config).
    MechanicHosts.Combat = CombatHost.Get();
    MechanicHosts.Survival = SurvivalHost.Get();
    MechanicHosts.Adapter.CombatStats = CombatHost.Get();
    MechanicHosts.Adapter.Trajectory = TrajectoryProbe.Get();
    MechanicHosts.Adapter.Perception = PerceptionProbe.Get();
    MechanicHosts.Adapter.Traversal = TraversalProbe.Get();
    MechanicHosts.Adapter.Locomotion = LocomotionHost.Get();
    MechanicHosts.Adapter.SkillModifiers = SkillSink.Get();

    if (Survival != nullptr)
    {
        Survival->OnSurvivalEvent.AddDynamic(this, &UInsimulMechanicHostBinder::HandleSurvivalEvent);
    }

    // One libinsimulcore runtime, borrowed from the plugin's own owner of it.
    CoreShell = NewObject<UInsimulRadiantSourceShell>(this);

    // Order matters: wire everything, then take away what this world does not
    // activate. Restricting first would leave a later wire-up re-registering a host
    // no active module names.
    RestrictHostsToActiveModules();

    LogMechanicSurface();
}

void UInsimulMechanicHostBinder::Deinitialize()
{
    if (GetGameInstance() != nullptr)
    {
        if (USurvivalSystem* Survival = GetGameInstance()->GetSubsystem<USurvivalSystem>())
        {
            Survival->OnSurvivalEvent.RemoveDynamic(this, &UInsimulMechanicHostBinder::HandleSurvivalEvent);
        }
    }

    // Unregister before the implementations die: the container borrows, it does not own.
    MechanicHosts = insimul::FInsimulMechanicHosts();
    if (CombatHost.IsValid())
    {
        CombatHost->Dispose();
    }
    if (SurvivalHost.IsValid())
    {
        SurvivalHost->Dispose();
    }
    SurvivalHost.Reset();
    SkillSink.Reset();
    LocomotionHost.Reset();
    TraversalProbe.Reset();
    PerceptionProbe.Reset();
    TrajectoryProbe.Reset();
    CombatHost.Reset();
    Registry.Reset();
    CoreShell = nullptr;

    Super::Deinitialize();
}

void UInsimulMechanicHostBinder::RegisterActor(const FString& Atom, AActor* Actor)
{
    Registry.RegisterActor(Atom, Actor);
}

void UInsimulMechanicHostBinder::UnregisterActor(const FString& Atom)
{
    Registry.UnregisterActor(Atom);
}

void UInsimulMechanicHostBinder::RegisterLocation(const FString& Atom, const FVector& Where)
{
    Registry.RegisterLocation(Atom, Where);
}

void UInsimulMechanicHostBinder::RegisterCombatEntity(const FString& EntityId, const FString& DisplayName,
    float Health, float MaxHealth, float AttackPower, float Defense, float DodgeChance)
{
    if (!CombatHost.IsValid() || EntityId.IsEmpty())
    {
        return;
    }

    insimul::FCombatEntityData Data;
    Data.Id = ToStdString(EntityId);
    Data.Name = ToStdString(DisplayName);
    Data.Health = Health;
    Data.MaxHealth = MaxHealth;
    CombatHost->RegisterEntity(Data);

    insimul::FCombatStats Base;
    Base.AttackPower = AttackPower;
    Base.Defense = Defense;
    Base.DodgeChance = DodgeChance;
    CombatHost->SetBaseStats(EntityId, Base);
}

float UInsimulMechanicHostBinder::GetEntityHealth(const FString& EntityId) const
{
    if (!CombatHost.IsValid())
    {
        return 0.f;
    }
    return static_cast<float>(CombatHost->GetHealth(ToStdString(EntityId)));
}

int32 UInsimulMechanicHostBinder::RestrictHostsToActiveModules()
{
    UInsimulModuleActivator* Activator = GetGameInstance() != nullptr
        ? GetGameInstance()->GetSubsystem<UInsimulModuleActivator>()
        : nullptr;

    if (Activator == nullptr || !Activator->IsResolved())
    {
        // NOT a silent "register everything": a build whose activation never resolved
        // is a build whose activation data is missing or unreadable, and the activator
        // has already logged which. Restricting against a set nobody resolved would
        // unregister every host on a bug in the data.
        UE_LOG(LogInsimulMechanicSurface, Error,
            TEXT("[Insimul] No active module set was resolved, so NO host was unregistered. Every wired host stays ")
            TEXT("registered and may be called for a module this world never selected — see the ")
            TEXT("LogInsimulActivation errors above."));
        return -1;
    }

    const std::vector<std::string> Dropped = MechanicHosts.RestrictTo(Activator->ActiveHostInterfaces());
    UE_LOG(LogInsimulMechanicSurface, Log,
        TEXT("[Insimul] %s"), *ToFString(Activator->ActiveModuleSet().Describe()));

    for (const std::string& Name : Dropped)
    {
        // Warning, not Log: a creator whose host is never called deserves the reason
        // in their own log rather than in this comment.
        UE_LOG(LogInsimulMechanicSurface, Warning,
            TEXT("[Insimul]   %s is implemented and UNREGISTERED — no module this world activates names it, so ")
            TEXT("core would never call it (module contract §7.3)."),
            *ToFString(Name));
    }
    return static_cast<int32>(Dropped.size());
}

int32 UInsimulMechanicHostBinder::LogMechanicSurface()
{
    insimul::ICoreCaller* Caller = CoreShell != nullptr ? CoreShell->GetCoreCaller() : nullptr;
    insimul::FInsimulMechanicSurface Surface(Caller);
    const std::vector<insimul::FMechanicModuleReport> Reports = Surface.Probe(MechanicHosts);

    UE_LOG(LogInsimulMechanicSurface, Log,
        TEXT("[Insimul] band-120 mechanic surface: bridge %s, %d method(s) offered"),
        Surface.BridgeAnswered() ? TEXT("answered") : TEXT("did NOT answer"),
        static_cast<int32>(Surface.Methods().size()));

    int32 Ready = 0;
    for (const insimul::FMechanicModuleReport& Report : Reports)
    {
        if (Report.IsReady())
        {
            ++Ready;
            UE_LOG(LogInsimulMechanicSurface, Log, TEXT("[Insimul]   %s"), *ToFString(Report.Message));
        }
        else
        {
            // Warning, not Log: an inert mechanic that reads as wired is the thing
            // this whole surface exists to prevent.
            UE_LOG(LogInsimulMechanicSurface, Warning, TEXT("[Insimul]   %s"), *ToFString(Report.Message));
        }
        for (const std::string& Missing : Report.MissingHosts)
        {
            UE_LOG(LogInsimulMechanicSurface, Warning, TEXT("[Insimul]     no host: %s"), *ToFString(Missing));
        }
    }

    if (SkillSink.IsValid() && SkillSink->Unapplied().Num() > 0)
    {
        UE_LOG(LogInsimulMechanicSurface, Warning,
            TEXT("[Insimul]   skill modifiers that reached nothing: %s"),
            *FString::Join(SkillSink->Unapplied(), TEXT(", ")));
    }
    return Ready;
}

void UInsimulMechanicHostBinder::HandleSurvivalEvent(const FInsimulSurvivalEvent& Event)
{
    if (!SurvivalHost.IsValid())
    {
        return;
    }
    SurvivalHost->OnHostSurvivalEvent(
        SurvivalEventAtom(Event.EventType),
        NeedTypeAtom(Event.NeedType),
        Event.Value,
        Event.Message);
}

FString UInsimulMechanicHostBinder::SurvivalEventAtom(EInsimulSurvivalEventType EventType)
{
    // Core's `SurvivalEvent.type` union, spelled core's way. A second spelling of one
    // vocabulary is the drift the module contract exists to stop.
    switch (EventType)
    {
    case EInsimulSurvivalEventType::NeedCritical:   return TEXT("need_critical");
    case EInsimulSurvivalEventType::NeedWarning:    return TEXT("need_warning");
    case EInsimulSurvivalEventType::NeedRestored:   return TEXT("need_restored");
    case EInsimulSurvivalEventType::DamageFromNeed: return TEXT("damage_from_need");
    case EInsimulSurvivalEventType::NeedSatisfied:  return TEXT("need_satisfied");
    default:                                        return TEXT("need_warning");
    }
}

FString UInsimulMechanicHostBinder::NeedTypeAtom(EInsimulNeedType NeedType)
{
    switch (NeedType)
    {
    case EInsimulNeedType::Hunger:      return TEXT("hunger");
    case EInsimulNeedType::Thirst:      return TEXT("thirst");
    case EInsimulNeedType::Temperature: return TEXT("temperature");
    case EInsimulNeedType::Stamina:     return TEXT("stamina");
    case EInsimulNeedType::Sleep:       return TEXT("sleep");
    default:                            return TEXT("hunger");
    }
}
