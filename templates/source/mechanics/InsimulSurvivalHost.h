// Copyright 2024 Insimul. All Rights Reserved.
//
// FInsimulSurvivalHost — ISurvivalSystem over the template's own USurvivalSystem
// (US-1 of tasklist 146, RUNTIME_CORE_ADOPTION.md §12.4).
//
// CORE PRICES; THIS SPENDS. `StaminaPool` decides what an action costs (`action/4`'s
// EnergyCost), whether the actor can pay for it, and which band that leaves them in
// (`winded/1`, `exhausted/1` over the world's authored thresholds), then hands the
// amount here. `ConsumeStamina` subtracts it and reports whether the meter could
// cover it. It does not re-price anything — a host that charges its own number makes
// the same save mean two things.
//
// THE NEEDS CLOCK STAYS THE HOST'S, WHICH IS WHY Update IS A NO-OP HERE.
// `USurvivalSystem::Update(DeltaTime)` is a subsystem the game already ticks from its
// own loop. Calling it again through this interface would decay hunger twice a frame,
// which is worse than not ticking it at all. Core's own contract says
// `update(deltaTime)` is "the host ticking its own system, not core requiring a
// per-frame call", so the interface method is deliberately inert and this is the
// documented consequence.
//
// THE ONE GAP CORE'S INTERFACE HAS AND THIS ENGINE DOES NOT: `setOnNeedChanged` fires
// on the TRANSITIONS USurvivalSystem announces (critical / warning / restored /
// satisfied), not on every per-tick decay. Nothing in core requires per-tick
// notification — `docs/module-contract.md` §3 forbids core needing a per-frame call —
// so the narrower cadence is a legitimate reading, and it is written here rather than
// left for a reader to discover.
//
// UNLIKE THE UNITY PROBE, `AddModifier` LANDS. Unity had to forward a modifier id as an
// authored PRESET id and warn when the world had no such preset;
// `USurvivalSystem::AddModifier(Id, NeedId, RateMultiplier, Duration)` takes the whole
// modifier, so a modifier core invents at runtime applies here. That is an engine
// difference worth knowing rather than a contract one.

#pragma once

#include "CoreMinimal.h"
#include "InsimulMechanicContracts.h"

class USurvivalSystem;

/**
 * Carries out stamina spends core already priced, and answers core's questions about
 * the host's needs. Constructed by UInsimulMechanicHostBinder over the game's own
 * USurvivalSystem; a null system makes every method inert and IsEnabled() false.
 */
class FInsimulSurvivalHost : public insimul::ISurvivalSystem
{
public:
    explicit FInsimulSurvivalHost(USurvivalSystem* InSystem) : System(InSystem) {}

    /** No-op by design — see the header. The game's own loop ticks USurvivalSystem. */
    virtual void Update(double DeltaTime) override;
    virtual void RestoreNeed(const std::string& NeedType, double Amount) override;
    /** Spend an amount core priced. False = the meter could not cover it. */
    virtual bool ConsumeStamina(double Amount) override;
    virtual void RecoverStamina(double Amount) override;
    virtual void SetTemperature(double Value) override;
    virtual void AddModifier(const insimul::FNeedModifier& Modifier) override;
    virtual void RemoveModifier(const std::string& ModifierId) override;
    virtual bool GetNeed(const std::string& NeedType, insimul::FNeedState& OutState) const override;
    virtual void GetAllNeeds(std::vector<insimul::FNeedState>& OutStates) const override;
    virtual double GetNeedPercent(const std::string& NeedType) const override;
    virtual bool IsAnyCritical() const override;
    virtual bool IsAnyWarning() const override;
    virtual void SetEnabled(bool bEnabled) override;
    virtual bool IsEnabled() const override;
    virtual void SetOnNeedChanged(void (*Callback)(const insimul::FNeedState&)) override;
    virtual void SetOnSurvivalEvent(void (*Callback)(const insimul::FSurvivalEvent&)) override;
    virtual void SetOnDamageFromNeed(void (*Callback)(const std::string&, double)) override;
    virtual void Dispose() override;

    /** Called by the binder when USurvivalSystem broadcasts. Fans the one Unreal
     *  delegate out to the three callbacks core's interface registers. */
    void OnHostSurvivalEvent(const FString& EventType, const FString& NeedId, double Value, const FString& Message);

    /** Whether Update() was ever called through the interface — read by the binder's
     *  boot log, so a game that wired core to drive the clock is TOLD it is inert
     *  rather than quietly losing its ticks. */
    bool WasUpdateCalled() const { return bUpdateWasCalled; }

private:
    USurvivalSystem* System = nullptr;
    void (*OnNeedChanged)(const insimul::FNeedState&) = nullptr;
    void (*OnSurvivalEvent)(const insimul::FSurvivalEvent&) = nullptr;
    void (*OnDamageFromNeed)(const std::string&, double) = nullptr;
    bool bUpdateWasCalled = false;
};
