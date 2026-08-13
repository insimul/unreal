// Copyright 2024 Insimul. All Rights Reserved.

#include "InsimulActorRegistry.h"

void FInsimulActorRegistry::RegisterActor(const FString& Atom, AActor* Actor)
{
    if (Atom.IsEmpty() || Actor == nullptr)
    {
        return;
    }
    Actors.Add(Atom, TWeakObjectPtr<AActor>(Actor));
}

void FInsimulActorRegistry::UnregisterActor(const FString& Atom)
{
    Actors.Remove(Atom);
}

void FInsimulActorRegistry::RegisterLocation(const FString& Atom, const FVector& Where)
{
    if (Atom.IsEmpty())
    {
        return;
    }
    Places.Add(Atom, Where);
}

AActor* FInsimulActorRegistry::FindActor(const FString& Atom) const
{
    const TWeakObjectPtr<AActor>* Found = Actors.Find(Atom);
    return Found != nullptr ? Found->Get() : nullptr;
}

bool FInsimulActorRegistry::FindActorLocation(const FString& Atom, FVector& OutLocation) const
{
    if (AActor* Actor = FindActor(Atom))
    {
        OutLocation = Actor->GetActorLocation();
        return true;
    }
    return false;
}

bool FInsimulActorRegistry::FindPlace(const FString& Atom, FVector& OutLocation) const
{
    if (const FVector* Found = Places.Find(Atom))
    {
        OutLocation = *Found;
        return true;
    }
    return FindActorLocation(Atom, OutLocation);
}

void FInsimulActorRegistry::LiveActorAtoms(TArray<FString>& OutAtoms) const
{
    for (const TPair<FString, TWeakObjectPtr<AActor>>& Pair : Actors)
    {
        if (Pair.Value.IsValid())
        {
            OutAtoms.Add(Pair.Key);
        }
    }
    OutAtoms.Sort();
}

void FInsimulActorRegistry::Reset()
{
    Actors.Reset();
    Places.Reset();
}
