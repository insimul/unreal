// Copyright 2024 Insimul. All Rights Reserved.
//
// FInsimulActorRegistry — atom → body, and location atom → place.
//
// Core names `nessa` and `forge_gate`. A raycast needs an FVector and a MoveTo needs
// an AActor*, and nothing in this template mapped one to the other before US-1 of
// tasklist 146: the probes, the combat host and the locomotion host all need the same
// lookup, so it exists once rather than three times.
//
// IT DECIDES NOTHING. A registry that cannot find an actor makes the probe unable to
// answer, which the portable fallback (insimul::FInsimulMechanicHosts) turns into
// core's documented degradation — clear, passable, no reading. It never guesses a
// position and never substitutes a different body.
//
// Plain C++ over UE types, deliberately: only UInsimulMechanicHostBinder is reflected,
// so a creator has one Blueprint surface and the mechanic hosts stay ordinary classes
// (RUNTIME_CORE_ADOPTION.md §12.4).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

/**
 * Where core's atoms live in this level. Weak pointers throughout: a despawned NPC
 * must read as "no body", not as a dangling actor.
 */
class FInsimulActorRegistry
{
public:
    /** Bind an entity atom (`nessa`, `player`) to the actor that is its body. */
    void RegisterActor(const FString& Atom, AActor* Actor);

    /** Forget an entity atom. Safe for an atom that was never registered. */
    void UnregisterActor(const FString& Atom);

    /** Bind a location atom (`forge_gate`, `market_square`) to a place in the level. */
    void RegisterLocation(const FString& Atom, const FVector& Where);

    /** The actor for an entity atom, or nullptr — despawned, or never registered. */
    AActor* FindActor(const FString& Atom) const;

    /** The world position of an entity atom. False when it has no live body. */
    bool FindActorLocation(const FString& Atom, FVector& OutLocation) const;

    /**
     * The world position of a LOCATION atom. Falls back to an entity of the same name
     * (a settlement that is an actor in this level), then fails. False is an answer:
     * a traversal probe that cannot place either end reports no reading rather than
     * measuring against the origin.
     */
    bool FindPlace(const FString& Atom, FVector& OutLocation) const;

    /** Every entity atom with a live body, for a boot-time diagnostic. */
    void LiveActorAtoms(TArray<FString>& OutAtoms) const;

    /** Drop every binding. Called when a level is torn down. */
    void Reset();

private:
    TMap<FString, TWeakObjectPtr<AActor>> Actors;
    TMap<FString, FVector> Places;
};
