// Copyright 2024 Insimul. All Rights Reserved.
//
// FInsimulLocomotionHost — ILocomotionHost over AAIController::MoveTo (US-1 of
// tasklist 146, RUNTIME_CORE_ADOPTION.md §12.2 finding 3 and §12.4).
//
// ONE ORDER IN, ONE ARRIVAL OUT. Locomotion is the most per-frame thing in a game — a
// path follower, a character-movement component, root motion, a crowd agent — and none
// of it crosses the boundary. Core has already decided the movement is afforded,
// permitted and PAID FOR by the time an order arrives here; this class dispatches it
// and reports one location atom back.
//
// ARRIVAL IS NOT A RETURN VALUE WHEN A BODY TAKES SECONDS TO MOVE. Core's TypeScript
// signature may return a Promise, so a JS host awaits a walk. C++ here cannot: the
// portable layer is deliberately synchronous (§2.3 item 5) and the C ABI cannot await
// a host at all. So this host answers with what it knows AT THE DECISION MOMENT:
//
//   - no body, no destination, or a path the navigation system cannot solve
//     → `bArrived = false` immediately, with the reason;
//   - anything else → the agent is dispatched and `bArrived = true`, so world state
//     moves at the decision moment and the body catches up.
//
// The alternative — reporting `arrived: false` for every movement that takes time —
// would make LocomotionDirector count a successful walk as a failure and re-plan
// against it. That is the finding the Unity probe recorded, and it lands identically
// here; §12.2 is where it is written up rather than worked around.
//
// URGENCY IS NOT A SPEED, AND THIS IS WHERE IT BECOMES ONE. Core states one of four
// atoms; four engines are each right about what a hurried walk looks like. The mapping
// below is THIS engine's answer and is a tunable of this class, not a constant of the
// contract.

#pragma once

#include "CoreMinimal.h"
#include "InsimulActorRegistry.h"
#include "InsimulMechanicContracts.h"

class UWorld;

/**
 * Carries out movements core afforded. Constructed by UInsimulMechanicHostBinder with
 * the same registry the probes use, so `forge_gate` means one place in this level.
 */
class FInsimulLocomotionHost : public insimul::ILocomotionHost
{
public:
    FInsimulLocomotionHost(UWorld* InWorld, const FInsimulActorRegistry* InRegistry)
        : World(InWorld), Registry(InRegistry) {}

    virtual bool Travel(const insimul::FLocomotionOrder& Order, insimul::FArrivalReport& OutReport) override;

    /** Movement speed in cm/s for each of core's four urgency atoms. This engine's
     *  answer to "how pressing", and the whole of what urgency becomes here. */
    double SpeedFor(const std::string& Urgency) const;

    double IdleSpeed = 100.0;
    double OrdinarySpeed = 200.0;
    double HurriedSpeed = 350.0;
    double UrgentSpeed = 600.0;

    /** How close counts as there, in centimetres — handed to MoveTo, never to core. */
    double AcceptanceRadiusCm = 100.0;

private:
    TWeakObjectPtr<UWorld> World;
    const FInsimulActorRegistry* Registry = nullptr;
};
