// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulRadiantSourceShell — the UE-facing layer over radiant quest GENERATION,
// the first slice of `@insimul/core` this plugin adopts (tasklist 99,
// RUNTIME_CORE_ADOPTION.md §5).
//
// THIN BY DESIGN, exactly like UInsimulPrologSubsystem over insimul::InsimulKB:
// this object owns one insimul::FInsimulCoreBridge (the RAII handle over
// libinsimulcore) and one insimul::FRadiantSource (the portable, host-tested
// translation site) and does nothing but marshal between UE reflected types
// (FString / USTRUCT / TArray) and those cores. ALL argument building, JSON
// encoding and result decoding lives in Portable/InsimulRadiantSource.cpp —
// this class converts strings and enforces game-thread affinity, nothing more.
// Both cores are forward-declared (pimpl via TUniquePtr) so this Public header
// never drags the C ABI or the std-based portable headers downstream.
//
// NOT THE RADIANT TICK. UInsimulQuestSystemShell::RadiantTick *offers*
// already-authored radiant quests; this class *creates* quests from templates
// and world state. Different capability, similar name — see §3.2.
//
// THREAD AFFINITY: QuickJS is single-threaded and the bundle's Prolog seam
// calls straight into libinsimul, whose KB is single-thread-owned. Every call
// here asserts IsInGameThread(). Do not call these off the game thread.
//
// COST: Generate() crosses a JSON boundary and runs a Prolog program. It is a
// DECISION call — run it when the director should offer new work (entering a
// settlement, a day boundary), NEVER from Tick.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "InsimulRadiantSourceShell.generated.h"

// Forward declarations of the plain-C++ cores. Kept out of this header so the C
// ABI and the std-based portable types stay implementation details.
namespace insimul { class FRadiantSource; class FInsimulCoreBridge; }

/** Which implementation answers GenerateQuests(). Mirrors insimul::ERadiantSource. */
UENUM(BlueprintType)
enum class EInsimulRadiantSource : uint8
{
	/** Generate through `@insimul/core` across the native bridge. */
	Core = 0 UMETA(DisplayName = "Core (libinsimulcore)"),
	/** Pre-adoption behaviour: generate nothing. Also the no-bridge fallback. */
	None = 1 UMETA(DisplayName = "None (pre-adoption)")
};

/**
 * One generated radiant quest, with core's own field names so a caller can be
 * read against the runtime contract.
 *
 * ORDER OF OPERATIONS: retract FactsToRetract BEFORE asserting FactsToAssert —
 * they are the stale cooldown/generation facts the fresh ones replace.
 */
USTRUCT(BlueprintType)
struct INSIMULRUNTIME_API FInsimulGeneratedQuest
{
	GENERATED_BODY()

	/** Generated quest id, e.g. "radiant_rt_bounty_1000". */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Radiant")
	FString QuestId;

	/** Id of the radiant template this quest was generated from. */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Radiant")
	FString TemplateId;

	/** Canonical quest Prolog to consult, exactly as core emitted it. */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Radiant")
	FString QuestContent;

	/** QuestContent split into clauses (trimmed, blanks dropped, emit order). */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Radiant")
	TArray<FString> ContentClauses;

	/** Facts to assert into the KB after this quest is accepted. */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Radiant")
	TArray<FString> FactsToAssert;

	/** Stale facts to retract FIRST (superseded cooldowns). */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Radiant")
	TArray<FString> FactsToRetract;
};

/**
 * Radiant quest generation. Create one per game instance and keep it — starting
 * the core runtime costs a few milliseconds.
 */
UCLASS(BlueprintType)
class INSIMULRUNTIME_API UInsimulRadiantSourceShell : public UObject
{
	GENERATED_BODY()

public:
	UInsimulRadiantSourceShell();
	virtual void BeginDestroy() override;

	/**
	 * Choose the implementation. Defaults to Core. Setting None reproduces this
	 * plugin's pre-adoption behaviour (no generation at all), which is also what
	 * a build without libinsimulcore falls back to.
	 */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Radiant")
	void SetSource(EInsimulRadiantSource InSource);

	UFUNCTION(BlueprintPure, Category = "Insimul|Radiant")
	EInsimulRadiantSource GetSource() const;

	/**
	 * True when generation will actually reach core. False means GenerateQuests
	 * behaves as None — check GetLastError() for why (no bridge on this
	 * platform, or the bundle failed to evaluate).
	 */
	UFUNCTION(BlueprintPure, Category = "Insimul|Radiant")
	bool IsCoreAvailable() const;

	/**
	 * Generate radiant quests for one tick.
	 *
	 * @param KbLines        world facts + rules as Prolog source lines.
	 * @param TemplateLines  the radiant template pack as Prolog source lines.
	 * @param Seed           hashed to a uint32. The SAME seed, KB and Now always
	 *                       produce byte-identical quests, on every engine.
	 * @param Now            current in-game time in seconds (cooldowns, ids).
	 * @param MaxQuests      cap for this tick; <= 0 means unbounded.
	 *
	 * Returns false only when the call FAILED. An empty OutQuests with a true
	 * return is a normal outcome — an unsatisfiable precondition, an active
	 * cooldown or an exclusion all produce it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Radiant", meta = (AutoCreateRefTerm = "KbLines,TemplateLines"))
	bool GenerateQuests(const TArray<FString>& KbLines, const TArray<FString>& TemplateLines,
		const FString& Seed, int64 Now, int32 MaxQuests, TArray<FInsimulGeneratedQuest>& OutQuests);

	/**
	 * Core's shipped base template pack as Prolog source, so a game does not have
	 * to vendor a copy of it. Empty when core is unavailable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Radiant")
	FString GetBaseTemplates();

	/** The template ids in the base pack, in declaration order. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Radiant")
	TArray<FString> GetBaseTemplateIds();

	/** The adopted core surface, sorted — assert the bridge is the expected one. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Radiant")
	TArray<FString> GetCoreMethods();

	/** "<abi> (quickjs <pin>, core <commit>)" — quote this in a bug report. */
	UFUNCTION(BlueprintPure, Category = "Insimul|Radiant")
	FString GetCoreVersion() const;

	/** Reason the last call failed or produced nothing, or "" if it succeeded. */
	UFUNCTION(BlueprintPure, Category = "Insimul|Radiant")
	FString GetLastError() const;

private:
	/** RAII handle over libinsimulcore. Released in BeginDestroy. */
	TUniquePtr<insimul::FInsimulCoreBridge> Bridge;

	/** The portable translation site. Borrows Bridge; destroyed before it. */
	TUniquePtr<insimul::FRadiantSource> Adapter;
};
