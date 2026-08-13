#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "../data/GameTypes.h"
#include "SurvivalSystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSurvivalEvent, const FInsimulSurvivalEvent&, Event);

/**
 * Survival needs (hunger, thirst, temperature, stamina, sleep)
 * Ported from Insimul's Babylon.js SurvivalSystem to Unreal subsystem.
 */
UCLASS()
class INSIMULEXPORT_API USurvivalSystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Load data from WorldIR JSON */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Survival")
    void LoadFromIR(const FString& JsonString);

    /** Call each frame with delta seconds to apply decay and damage */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Survival")
    void Update(float DeltaTime);

    /** Get current value for a need (0–MaxValue) */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Survival")
    float GetNeedValue(const FString& NeedId) const;

    /** Get current value as percentage (0–1) */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Survival")
    float GetNeedPercent(const FString& NeedId) const;

    /** Restore a need by the given amount (e.g. eating restores hunger) */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Survival")
    void RestoreNeed(const FString& NeedId, float Amount);

    /** Consume stamina for an action; returns false if insufficient */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Survival")
    bool ConsumeStamina(float Amount);

    /** Recover stamina by the given amount */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Survival")
    void RecoverStamina(float Amount);

    /** Set temperature directly (environment-driven) */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Survival")
    void SetTemperature(float Value);

    /** Add a modifier that affects a need's decay rate */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Survival")
    void AddModifier(const FString& ModifierId, const FString& NeedId, float RateMultiplier, float Duration);

    /** Remove a modifier by ID */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Survival")
    void RemoveModifier(const FString& ModifierId);

    /** Check if any need is at critical level */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Survival")
    bool IsAnyCritical() const;

    /** Check if any need is at warning level */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Survival")
    bool IsAnyWarning() const;

    // ── The queries core's ISurvivalSystem asks that this system could not answer ──
    // Added by US-1 of tasklist 146 (RUNTIME_CORE_ADOPTION.md §12.4). `getNeed`,
    // `getAllNeeds`, `setEnabled` and `isEnabled` are members of the interface core's
    // `stamina` module declares, and every one of them needed a reader this class did
    // not expose. They are ordinary accessors over state it already held — nothing
    // here decides anything, and FInsimulSurvivalHost is their only caller today.

    /** Whether this world authored that need at all. A world with no hunger must read
     *  as ABSENT rather than as hunger at zero. */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Survival")
    bool HasNeed(const FString& NeedId) const;

    /** Every authored need id, sorted. */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Survival")
    void GetNeedIds(TArray<FString>& OutIds) const;

    /** The need's authored ceiling (0 when it has none). */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Survival")
    float GetNeedMax(const FString& NeedId) const;

    /** The need's authored decay rate per second (0 when it has none). */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Survival")
    float GetNeedDecayRate(const FString& NeedId) const;

    /** Whether ONE need is at its critical threshold — the per-need form of
     *  IsAnyCritical, using the same both-extremes rule Update() applies. */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Survival")
    bool IsNeedCritical(const FString& NeedId) const;

    /** Whether ONE need is at its warning threshold and not critical. */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Survival")
    bool IsNeedWarning(const FString& NeedId) const;

    /** Stop or resume the needs clock. A disabled system decays nothing and fires
     *  nothing; values and modifiers are kept, so re-enabling resumes rather than
     *  restarts. */
    UFUNCTION(BlueprintCallable, Category = "Insimul|Survival")
    void SetEnabled(bool bInEnabled);

    UFUNCTION(BlueprintCallable, Category = "Insimul|Survival")
    bool IsEnabled() const;

    /** Broadcast when a survival event occurs */
    UPROPERTY(BlueprintAssignable, Category = "Insimul|Survival")
    FOnSurvivalEvent OnSurvivalEvent;

    UPROPERTY(BlueprintReadOnly, Category = "Insimul|Survival")
    int32 NeedCount = 0;

    /** Whether the needs clock runs. False makes Update() a no-op. */
    UPROPERTY(BlueprintReadOnly, Category = "Insimul|Survival")
    bool bEnabled = true;

    UPROPERTY(BlueprintReadOnly, Category = "Insimul|Survival")
    FInsimulSurvivalDamageConfig DamageConfig;

    UPROPERTY(BlueprintReadOnly, Category = "Insimul|Survival")
    FInsimulTemperatureConfig TemperatureConfig;

    UPROPERTY(BlueprintReadOnly, Category = "Insimul|Survival")
    FInsimulStaminaConfig StaminaConfig;

    UPROPERTY(BlueprintReadOnly, Category = "Insimul|Survival")
    TArray<FInsimulSurvivalModifierPreset> ModifierPresets;

private:
    /** Runtime state per need, keyed by need ID string */
    struct FNeedRuntime
    {
        FInsimulNeedConfig Config;
        float Current;
        bool bWasCritical = false;
        bool bWasWarning = false;
    };

    TMap<FString, FNeedRuntime> Needs;

    struct FActiveModifier
    {
        FString Id;
        FString NeedId;
        float RateMultiplier;
        float RemainingTime; // <= 0 means permanent
    };

    TArray<FActiveModifier> ActiveModifiers;

    /** Get effective decay multiplier for a need from all active modifiers */
    float GetDecayMultiplier(const FString& NeedId) const;

    /** Fire a survival event and broadcast delegate */
    void FireEvent(EInsimulSurvivalEventType EventType, const FString& NeedId, float Value, const FString& Message);

    /** Map need ID string to enum */
    static EInsimulNeedType StringToNeedType(const FString& Id);
};
