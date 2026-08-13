// Copyright 2024 Insimul. All Rights Reserved.

#include "InsimulSurvivalHost.h"

#include "../systems/SurvivalSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogInsimulSurvival, Log, All);

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

void FInsimulSurvivalHost::Update(double DeltaTime)
{
    (void)DeltaTime;
    // Inert by design. USurvivalSystem is a UGameInstanceSubsystem the game's own loop
    // already ticks; ticking it again here would decay every need twice. Recorded so
    // the binder's boot log can say so out loud.
    if (!bUpdateWasCalled)
    {
        bUpdateWasCalled = true;
        UE_LOG(LogInsimulSurvival, Warning,
            TEXT("[Insimul] ISurvivalSystem::Update was called through the host interface and is a no-op: ")
            TEXT("this game's own loop ticks USurvivalSystem (RUNTIME_CORE_ADOPTION.md §12.4)."));
    }
}

void FInsimulSurvivalHost::RestoreNeed(const std::string& NeedType, double Amount)
{
    if (System == nullptr) return;
    System->RestoreNeed(ToFString(NeedType), static_cast<float>(Amount));
}

bool FInsimulSurvivalHost::ConsumeStamina(double Amount)
{
    if (System == nullptr)
    {
        // No host meter. False would tell core the spend failed and make it refuse an
        // action the world permits; core's own energy/3 meter is the authority when
        // this engine has none, so the spend is reported as having gone through.
        return true;
    }
    // The amount is core's. Subtract it; do not re-price it.
    return System->ConsumeStamina(static_cast<float>(Amount));
}

void FInsimulSurvivalHost::RecoverStamina(double Amount)
{
    if (System == nullptr) return;
    System->RecoverStamina(static_cast<float>(Amount));
}

void FInsimulSurvivalHost::SetTemperature(double Value)
{
    if (System == nullptr) return;
    System->SetTemperature(static_cast<float>(Value));
}

void FInsimulSurvivalHost::AddModifier(const insimul::FNeedModifier& Modifier)
{
    if (System == nullptr) return;
    System->AddModifier(
        ToFString(Modifier.Id),
        ToFString(Modifier.NeedType),
        static_cast<float>(Modifier.RateMultiplier),
        static_cast<float>(Modifier.Duration));
}

void FInsimulSurvivalHost::RemoveModifier(const std::string& ModifierId)
{
    if (System == nullptr) return;
    System->RemoveModifier(ToFString(ModifierId));
}

bool FInsimulSurvivalHost::GetNeed(const std::string& NeedType, insimul::FNeedState& OutState) const
{
    if (System == nullptr) return false;
    const FString NeedId = ToFString(NeedType);
    if (!System->HasNeed(NeedId))
    {
        // "or undefined" — a world that authored no hunger has no hunger, which is not
        // the same as hunger at zero.
        return false;
    }
    OutState.Id = NeedType;
    OutState.Current = System->GetNeedValue(NeedId);
    OutState.Max = System->GetNeedMax(NeedId);
    OutState.DecayRate = System->GetNeedDecayRate(NeedId);
    OutState.bIsCritical = System->IsNeedCritical(NeedId);
    OutState.bIsWarning = System->IsNeedWarning(NeedId);
    // Modifiers stay empty: USurvivalSystem holds them privately as a decay multiplier
    // and exposes no per-need list. Nothing in core reads NeedState.modifiers today,
    // and inventing entries would be worse than an empty list.
    OutState.Modifiers.clear();
    return true;
}

void FInsimulSurvivalHost::GetAllNeeds(std::vector<insimul::FNeedState>& OutStates) const
{
    OutStates.clear();
    if (System == nullptr) return;
    TArray<FString> NeedIds;
    System->GetNeedIds(NeedIds);
    for (const FString& NeedId : NeedIds)
    {
        insimul::FNeedState State;
        if (GetNeed(ToStdString(NeedId), State))
        {
            OutStates.push_back(State);
        }
    }
}

double FInsimulSurvivalHost::GetNeedPercent(const std::string& NeedType) const
{
    if (System == nullptr) return 0.0;
    return System->GetNeedPercent(ToFString(NeedType));
}

bool FInsimulSurvivalHost::IsAnyCritical() const
{
    return System != nullptr && System->IsAnyCritical();
}

bool FInsimulSurvivalHost::IsAnyWarning() const
{
    return System != nullptr && System->IsAnyWarning();
}

void FInsimulSurvivalHost::SetEnabled(bool bEnabled)
{
    if (System == nullptr) return;
    System->SetEnabled(bEnabled);
}

bool FInsimulSurvivalHost::IsEnabled() const
{
    return System != nullptr && System->IsEnabled();
}

void FInsimulSurvivalHost::SetOnNeedChanged(void (*Callback)(const insimul::FNeedState&))
{
    OnNeedChanged = Callback;
}

void FInsimulSurvivalHost::SetOnSurvivalEvent(void (*Callback)(const insimul::FSurvivalEvent&))
{
    OnSurvivalEvent = Callback;
}

void FInsimulSurvivalHost::SetOnDamageFromNeed(void (*Callback)(const std::string&, double))
{
    OnDamageFromNeed = Callback;
}

void FInsimulSurvivalHost::Dispose()
{
    OnNeedChanged = nullptr;
    OnSurvivalEvent = nullptr;
    OnDamageFromNeed = nullptr;
    System = nullptr;
}

void FInsimulSurvivalHost::OnHostSurvivalEvent(
    const FString& EventType, const FString& NeedId, double Value, const FString& Message)
{
    if (OnSurvivalEvent != nullptr)
    {
        insimul::FSurvivalEvent Event;
        Event.Type = ToStdString(EventType);
        Event.NeedType = ToStdString(NeedId);
        Event.Value = Value;
        Event.Message = ToStdString(Message);
        OnSurvivalEvent(Event);
    }

    if (OnDamageFromNeed != nullptr && EventType == TEXT("damage_from_need"))
    {
        OnDamageFromNeed(ToStdString(NeedId), Value);
    }

    if (OnNeedChanged != nullptr)
    {
        // The narrower cadence the header names: a transition, not a per-tick decay.
        insimul::FNeedState State;
        if (GetNeed(ToStdString(NeedId), State))
        {
            OnNeedChanged(State);
        }
    }
}
