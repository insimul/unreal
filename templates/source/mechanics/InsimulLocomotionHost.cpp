// Copyright 2024 Insimul. All Rights Reserved.

#include "InsimulLocomotionHost.h"

#include "AIController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"

namespace
{
    FString ToFString(const std::string& Text)
    {
        return FString(UTF8_TO_TCHAR(Text.c_str()));
    }
}

double FInsimulLocomotionHost::SpeedFor(const std::string& Urgency) const
{
    if (Urgency == "idle") return IdleSpeed;
    if (Urgency == "hurried") return HurriedSpeed;
    if (Urgency == "urgent") return UrgentSpeed;
    // `ordinary`, and anything outside core's closed vocabulary. An unknown atom walks
    // at the default rather than standing still: a body that stops because a future
    // core added a fifth rung is worse than a body that walks.
    return OrdinarySpeed;
}

bool FInsimulLocomotionHost::Travel(const insimul::FLocomotionOrder& Order, insimul::FArrivalReport& OutReport)
{
    if (!World.IsValid() || Registry == nullptr)
    {
        // The host itself is not usable. False, so the portable fallback answers
        // ARRIVED — the headless behaviour, which is what a world with no bodies wants.
        return false;
    }

    AActor* Body = Registry->FindActor(ToFString(Order.Actor));
    if (Body == nullptr)
    {
        OutReport.bArrived = false;
        OutReport.Reason = "no body is registered for this actor in the current level";
        return true;
    }

    FVector Destination;
    if (!Registry->FindPlace(ToFString(Order.To), Destination))
    {
        OutReport.bArrived = false;
        OutReport.Reason = "the destination location atom is not placed in the current level";
        return true;
    }

    APawn* Pawn = Cast<APawn>(Body);
    AAIController* Controller = Pawn != nullptr ? Cast<AAIController>(Pawn->GetController()) : nullptr;
    if (Controller == nullptr)
    {
        // No AI controller: this is a body somebody else drives (the player, a
        // cinematic). Core is told it did not arrive rather than being told a lie, and
        // the reason names what is missing.
        OutReport.bArrived = false;
        OutReport.Reason = "the actor has no AAIController to carry out a movement";
        return true;
    }

    if (ACharacter* Character = Cast<ACharacter>(Pawn))
    {
        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
        {
            // Urgency becomes a speed HERE, and only here.
            Movement->MaxWalkSpeed = static_cast<float>(SpeedFor(Order.Urgency));
        }
    }

    const EPathFollowingRequestResult::Type Result =
        Controller->MoveToLocation(Destination, static_cast<float>(AcceptanceRadiusCm), /*bStopOnOverlap=*/true);

    if (Result == EPathFollowingRequestResult::Failed)
    {
        OutReport.bArrived = false;
        OutReport.Reason = "the navigation system could not solve a path to the destination";
        return true;
    }

    // Dispatched, or already there. World state moves at the decision moment and the
    // body catches up — the finding in §12.2, applied.
    OutReport.bArrived = true;
    OutReport.Location = Order.To;
    return true;
}
