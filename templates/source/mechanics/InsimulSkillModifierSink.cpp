// Copyright 2024 Insimul. All Rights Reserved.

#include "InsimulSkillModifierSink.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogInsimulSkills, Log, All);

namespace
{
    FString ToFString(const std::string& Text)
    {
        return FString(UTF8_TO_TCHAR(Text.c_str()));
    }
}

void FInsimulSkillModifierSink::ApplyModifiers(const std::string& ActorId, const insimul::FSkillModifiers& Modifiers)
{
    if (ActorId.empty() || Registry == nullptr)
    {
        return;
    }
    const FString Actor = ToFString(ActorId);

    TMap<FString, double> Set;
    for (const insimul::FSkillModifier& Modifier : Modifiers)
    {
        Set.Add(ToFString(Modifier.Parameter), Modifier.Amount);
    }
    LastApplied.Add(Actor, Set);

    ACharacter* Character = Cast<ACharacter>(Registry->FindActor(Actor));
    UCharacterMovementComponent* Movement = Character != nullptr ? Character->GetCharacterMovement() : nullptr;

    for (const TPair<FString, double>& Pair : Set)
    {
        if (Pair.Key == TEXT("move_speed"))
        {
            if (Movement == nullptr)
            {
                if (!UnappliedParameters.Contains(Pair.Key))
                {
                    UnappliedParameters.Add(Pair.Key);
                }
                continue;
            }
            // Capture the authored base once. Without this, applying the same absolute
            // set twice would compound, and core requires idempotence.
            const double* Known = BaseWalkSpeeds.Find(Actor);
            const double Base = Known != nullptr ? *Known : static_cast<double>(Movement->MaxWalkSpeed);
            if (Known == nullptr)
            {
                BaseWalkSpeeds.Add(Actor, Base);
            }
            // A multiple of the actor's own base: core's `modifies(move_speed, N)` is a
            // magnitude, and cm/s is a number only this engine can be right about.
            Movement->MaxWalkSpeed = static_cast<float>(FMath::Max(0.0, Base * (1.0 + Pair.Value * 0.01)));
            continue;
        }

        // carry_capacity and everything else: recorded, announced, not applied. See the
        // header — inventing a limit no rule reads is worse than not applying it.
        if (!UnappliedParameters.Contains(Pair.Key))
        {
            UnappliedParameters.Add(Pair.Key);
            UE_LOG(LogInsimulSkills, Warning,
                TEXT("[Insimul] skill modifier '%s' reached nothing in this game: no system here owns that ")
                TEXT("quantity. Core still holds the total (RUNTIME_CORE_ADOPTION.md §12.4)."),
                *Pair.Key);
        }
    }

    // An actor whose whole set no longer names move_speed goes back to its base.
    if (Movement != nullptr && !Set.Contains(TEXT("move_speed")))
    {
        if (const double* Base = BaseWalkSpeeds.Find(Actor))
        {
            Movement->MaxWalkSpeed = static_cast<float>(*Base);
        }
    }
}
