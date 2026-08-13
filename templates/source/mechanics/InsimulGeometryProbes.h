// Copyright 2024 Insimul. All Rights Reserved.
//
// The three probes an engine must supply — trajectory, perception, traversal (US-1 of
// tasklist 146, RUNTIME_CORE_ADOPTION.md §12.4).
//
// EACH ONE IS A MEASUREMENT, NEVER A DECISION. A probe reports what this engine's
// geometry found: is the line clear, how far apart are they, how much of the target is
// behind something, how bright is it there, can this actor cross that link. What any of
// it is WORTH — how much darkness hides, whether the shot is permitted, what the climb
// costs — is authored in the world and decided in core. A host that answered `clear` to
// everything still cannot change one number of the outcome, and that is the property
// that makes these three safe to write in engine terms at all.
//
// FALSE IS AN ANSWER. Core says a probe "must not throw" and states the reading a
// thrown error is read as. Unreal builds with exceptions disabled, so the mirror in
// Portable/InsimulMechanicContracts.h makes that a return value instead: false means
// "this engine could not answer", and insimul::FInsimulMechanicHosts turns it into
// core's documented fallback — clear, no reading, passable — in ONE place rather than
// three. These classes therefore never invent a reading to avoid returning false.
//
// WHAT IS APPROXIMATE, AND SAID SO. `light_level/2` is the one reading this engine
// cannot take exactly: Unreal has no cheap runtime per-point lightmap read at gameplay
// time, so LightAt() is an ambient floor plus a trace to the world's dominant
// directional light. It is a measurement either way, and it is documented here rather
// than silently rounded, because a stealth game whose darkness is wrong is wrong in a
// way no corpus catches.

#pragma once

#include "CoreMinimal.h"
#include "InsimulActorRegistry.h"
#include "InsimulMechanicContracts.h"

class UWorld;

/**
 * Shared geometry for the three probes: one world, one registry, one set of trace
 * settings. Held by UInsimulMechanicHostBinder and handed to each probe.
 */
struct FInsimulProbeContext
{
    TWeakObjectPtr<UWorld> World;
    const FInsimulActorRegistry* Registry = nullptr;

    /** Trace channel used for line-of-sight and line-of-fire. */
    ECollisionChannel SightChannel = ECC_Visibility;

    /** Unreal centimetres per one unit of the world's authored distance unit. Core
     *  prices reach in the unit `item_range/2` is authored in; this is the only place
     *  the two meet, and getting it wrong makes every range wrong by a constant. */
    double UnitsPerCentimetre = 0.01;

    /** Ambient light floor, 0–100, for a world with no directional light. */
    double AmbientLight = 20.0;

    bool IsUsable() const { return World.IsValid() && Registry != nullptr; }
};

/** ITrajectoryProbe — is the line to that target clear, and how far away is it. */
class FInsimulTrajectoryProbe : public insimul::ITrajectoryProbe
{
public:
    explicit FInsimulTrajectoryProbe(const FInsimulProbeContext& InContext) : Context(InContext) {}

    virtual bool Query(const insimul::FTrajectoryQuery& InQuery, insimul::FTrajectoryReading& OutReading) override;

private:
    FInsimulProbeContext Context;
};

/** IPerceptionProbe — what can this observer's senses reach of that target, right now. */
class FInsimulPerceptionProbe : public insimul::IPerceptionProbe
{
public:
    explicit FInsimulPerceptionProbe(const FInsimulProbeContext& InContext) : Context(InContext) {}

    virtual bool Sense(const insimul::FPerceptionQuery& InQuery, insimul::FPerceptionReading& OutReading) override;

    /** Sight range in centimetres beyond which visibility attenuates to zero. Authored
     *  perception tuning lives in core; this is the engine's own sensing horizon. */
    double SightRangeCm = 4000.0;

    /** Hearing range in centimetres, for the audibility reading. */
    double HearingRangeCm = 2500.0;

private:
    /** The approximation this file's header paragraph names. 0–100. */
    double LightAt(const FVector& Where) const;

    FInsimulProbeContext Context;
};

/** ITraversalProbe — could this actor get across that link, from where they stand. */
class FInsimulTraversalProbe : public insimul::ITraversalProbe
{
public:
    explicit FInsimulTraversalProbe(const FInsimulProbeContext& InContext) : Context(InContext) {}

    virtual bool Query(const insimul::FTraversalQuery& InQuery, insimul::FTraversalReading& OutReading) override;

private:
    FInsimulProbeContext Context;
};
