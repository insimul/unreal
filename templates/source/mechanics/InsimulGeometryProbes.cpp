// Copyright 2024 Insimul. All Rights Reserved.

#include "InsimulGeometryProbes.h"

#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"

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

    /** Eye height for a line-of-sight / line-of-fire trace. A trace between two actor
     *  origins is a trace between two pairs of feet, which the floor blocks. */
    constexpr double EyeHeightCm = 80.0;

    FVector EyeOf(const FVector& Origin)
    {
        return Origin + FVector(0.0, 0.0, EyeHeightCm);
    }
}

// ── FInsimulTrajectoryProbe ─────────────────────────────────────────────────

bool FInsimulTrajectoryProbe::Query(const insimul::FTrajectoryQuery& InQuery, insimul::FTrajectoryReading& OutReading)
{
    if (!Context.IsUsable())
    {
        return false;
    }

    AActor* Attacker = Context.Registry->FindActor(ToFString(InQuery.Attacker));
    AActor* Target = Context.Registry->FindActor(ToFString(InQuery.Target));
    if (Attacker == nullptr || Target == nullptr)
    {
        // No body for one of them. Report no reading rather than "blocked": a
        // despawned target is not cover, and core's fallback (clear) plus its own
        // reach check is the right answer.
        return false;
    }

    const FVector From = EyeOf(Attacker->GetActorLocation());
    const FVector To = EyeOf(Target->GetActorLocation());

    OutReading.bHasSeparation = true;
    OutReading.Separation = FVector::Dist(From, To) * Context.UnitsPerCentimetre;

    FCollisionQueryParams Params(SCENE_QUERY_STAT(InsimulTrajectory), /*bTraceComplex=*/false);
    Params.AddIgnoredActor(Attacker);
    Params.AddIgnoredActor(Target);

    FHitResult Hit;
    const bool bBlocked = Context.World->LineTraceSingleByChannel(Hit, From, To, Context.SightChannel, Params);
    OutReading.bClear = !bBlocked;
    if (bBlocked && Hit.GetActor() != nullptr)
    {
        // A reason string for display. Core never reads it as a decision.
        OutReading.BlockedBy = ToStdString(Hit.GetActor()->GetName());
    }
    return true;
}

// ── FInsimulPerceptionProbe ─────────────────────────────────────────────────

double FInsimulPerceptionProbe::LightAt(const FVector& Where) const
{
    // The approximation: an ambient floor, plus the world's dominant directional light
    // if nothing stands between it and this point. Unreal exposes no cheap per-point
    // lightmap read at gameplay time, so this is what an engine measurement of
    // `light_level/2` honestly is here. What darkness is WORTH stays authored.
    double Light = Context.AmbientLight;
    if (!Context.World.IsValid())
    {
        return Light;
    }

    for (TActorIterator<ADirectionalLight> It(Context.World.Get()); It; ++It)
    {
        const ADirectionalLight* Sun = *It;
        if (Sun == nullptr || Sun->IsHidden())
        {
            continue;
        }
        const FVector Towards = -Sun->GetActorForwardVector();
        const FVector SkyPoint = Where + Towards * 100000.0;

        FCollisionQueryParams Params(SCENE_QUERY_STAT(InsimulLight), /*bTraceComplex=*/false);
        FHitResult Hit;
        const bool bOccluded = Context.World->LineTraceSingleByChannel(Hit, Where, SkyPoint, Context.SightChannel, Params);
        if (!bOccluded)
        {
            Light = 100.0;
        }
        break;
    }
    return FMath::Clamp(Light, 0.0, 100.0);
}

bool FInsimulPerceptionProbe::Sense(const insimul::FPerceptionQuery& InQuery, insimul::FPerceptionReading& OutReading)
{
    if (!Context.IsUsable())
    {
        return false;
    }

    AActor* Observer = Context.Registry->FindActor(ToFString(InQuery.Observer));
    AActor* Target = Context.Registry->FindActor(ToFString(InQuery.Target));
    if (Observer == nullptr || Target == nullptr)
    {
        return false;
    }

    const FVector Eye = EyeOf(Observer->GetActorLocation());
    const FVector TargetPoint = EyeOf(Target->GetActorLocation());
    const double DistanceCm = FVector::Dist(Eye, TargetPoint);

    FCollisionQueryParams Params(SCENE_QUERY_STAT(InsimulPerception), /*bTraceComplex=*/false);
    Params.AddIgnoredActor(Observer);
    Params.AddIgnoredActor(Target);

    FHitResult Hit;
    const bool bOccluded = Context.World->LineTraceSingleByChannel(Hit, Eye, TargetPoint, Context.SightChannel, Params);

    // visibility folds line_of_sight/2 and distance attenuation into [0,1], which is
    // exactly what core asks for. Linear falloff: a curve here would be this engine
    // deciding how much distance hides, which is the world's to author.
    const double Attenuation = SightRangeCm > 0.0
        ? FMath::Clamp(1.0 - (DistanceCm / SightRangeCm), 0.0, 1.0)
        : 0.0;
    OutReading.Visibility = bOccluded ? 0.0 : Attenuation;

    OutReading.bHasCover = true;
    OutReading.Cover = bOccluded ? 1.0 : 0.0;

    OutReading.bHasAudibility = true;
    OutReading.Audibility = HearingRangeCm > 0.0
        ? FMath::Clamp(1.0 - (DistanceCm / HearingRangeCm), 0.0, 1.0)
        : 0.0;

    OutReading.bHasLight = true;
    OutReading.Light = LightAt(Target->GetActorLocation());

    // Stance is what the TARGET's body is doing, in core's own three atoms. This
    // template has no crouch or prone state to read, so it reports `standing` — the
    // one it can actually see — rather than guessing from velocity. A game with a
    // crouch sets it from its own movement component.
    OutReading.Stance = "standing";

    OutReading.bHasNoise = true;
    OutReading.Noise = FMath::Clamp(Target->GetVelocity().Size() * 0.1, 0.0, 100.0);
    return true;
}

// ── FInsimulTraversalProbe ──────────────────────────────────────────────────

bool FInsimulTraversalProbe::Query(const insimul::FTraversalQuery& InQuery, insimul::FTraversalReading& OutReading)
{
    if (!Context.IsUsable())
    {
        return false;
    }

    FVector From;
    FVector To;
    if (!Context.Registry->FindPlace(ToFString(InQuery.From), From) ||
        !Context.Registry->FindPlace(ToFString(InQuery.To), To))
    {
        // Neither end is placed in this level, so this engine has nothing to measure.
        // No reading; core's fallback treats the geometric link as passable, which is
        // the right answer for a link whose geometry does not exist here.
        return false;
    }

    OutReading.bHasDistance = true;
    OutReading.Distance = FVector::Dist(From, To) * Context.UnitsPerCentimetre;

    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Context.World.Get());
    if (Nav == nullptr)
    {
        // No navigation data. A raycast against world geometry is the fallback
        // measurement, not a refusal: `walk` over open ground is still answerable.
        FCollisionQueryParams Params(SCENE_QUERY_STAT(InsimulTraversal), /*bTraceComplex=*/false);
        FHitResult Hit;
        const bool bBlocked = Context.World->LineTraceSingleByChannel(Hit, From, To, Context.SightChannel, Params);
        OutReading.bPassable = !bBlocked;
        if (bBlocked && Hit.GetActor() != nullptr)
        {
            OutReading.BlockedBy = ToStdString(Hit.GetActor()->GetName());
        }
        return true;
    }

    // NavigationRaycast reports the first point on the navmesh where a straight walk
    // from `From` to `To` leaves navigable space. Reaching the destination is the
    // affordance core asked about; the ROUTE is this engine's business and does not
    // cross the boundary.
    FVector HitLocation = To;
    const bool bHitNavBoundary = Nav->NavigationRaycast(Context.World.Get(), From, To, HitLocation);
    OutReading.bPassable = !bHitNavBoundary;
    if (bHitNavBoundary)
    {
        OutReading.BlockedBy = "navmesh";
    }
    return true;
}
