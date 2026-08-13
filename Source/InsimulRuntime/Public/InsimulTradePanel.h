// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulTradePanel — the UE seam over the portable trade view-model
// (Portable/InsimulTradeModel.h, US-XU3). A UObject the three trade panels bind
// to: the INVENTORY (UInsimulInventoryPanel) reads PlayerGold / PlayerItems, the
// CONTAINER panel (UInsimulContainerPanel) drives TakeFromContainer / TakeAll, and
// the MERCHANT panel (UInsimulMerchantPanel) drives Buy / Sell against
// MerchantItems / MerchantGold at the price QuotePrice() reports.
//
// Backed EXCLUSIVELY by save.currentState: the seam attaches the portable model to
// the FTradeState the save shell hydrates from currentState (player.gold /
// player.inventory, containers.containers, npcs.merchantStates), so every read and
// mutation lands in the one place the save serializes — the state-location
// invariant, proven UE-free by FInsimulTradeModel. BindToRuntime() / CommitToSave()
// are where that stops being a convention: they hydrate from and write back into the
// booted runtime's real SaveFile (Portable/InsimulUIStateBinding.h), and ctest
// `ui_state_binding` proves the change lands in the save's canonical bytes and
// nowhere else. This class is the thin, syntax-gated Blueprint / UObject boundary;
// all buy/sell/transfer SEMANTICS (conservation of item + gold census, failure
// reasons) and every price term live in the portable cores and are host-tested by
// ctest `ui_trade` and ctest `ui_state_binding`.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "InsimulTradePanel.generated.h"

// The engine-agnostic model + state this seam wraps (pimpl — never in the
// reflected layout). Its op matrix is host-tested by ctest `ui_trade`, its price
// terms by ctest `ui_state_binding`.
namespace insimul { class FInsimulTradeModel; struct FTradeState; }

class UInsimulRuntimeSubsystem;

/** One item stack as the trade panels render it. */
USTRUCT(BlueprintType)
struct FInsimulTradeItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Trade")
	FString ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Trade")
	int32 Quantity = 0;

	/** Unit price used by buy/sell (0 when the stack carries no value). */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Trade")
	int32 Value = 0;
};

/** Outcome of a trade op: ok + a machine-readable failure Reason + Moved count. */
USTRUCT(BlueprintType)
struct FInsimulTradeResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Trade")
	bool bOk = false;

	/** Empty on success; otherwise e.g. "insufficient_gold" / "out_of_stock". */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Trade")
	FString Reason;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Trade")
	int32 Moved = 0;
};

/** One reported term of a price — what it was, how much, and whose it is. */
USTRUCT(BlueprintType)
struct FInsimulPriceTerm
{
	GENERATED_BODY()

	/** "markup" | "scarcity" | "standing" | "proprietor". */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Trade")
	FString Factor;

	/** Signed percentage against the authored base, per unit. */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Trade")
	int32 Percent = 0;

	/** Signed coin this term moved, per unit. */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Trade")
	int32 Amount = 0;

	/** The business / vendor / faction / owner it came from. */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Trade")
	FString Subject;
};

/**
 * A quoted price with every term that made it — what the shop panel shows next to
 * the number, so a player charged double is told the reason.
 */
USTRUCT(BlueprintType)
struct FInsimulPriceQuote
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Trade")
	int32 Base = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Trade")
	TArray<FInsimulPriceTerm> Terms;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Trade")
	int32 Unit = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Trade")
	int32 Quantity = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Trade")
	int32 Total = 0;

	/** True when the world has no economy and the price IS the item's value. */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Trade")
	bool bFallback = false;
};

/**
 * The default-runtime trade view-model. The inventory / container / merchant
 * UUserWidgets bind Buy/Sell/Take and read the accessors below; all state lives in
 * (and only in) the attached save.currentState slice.
 */
UCLASS(BlueprintType)
class INSIMULRUNTIME_API UInsimulTradePanel : public UObject
{
	GENERATED_BODY()

public:
	UInsimulTradePanel();

	/** Player gold held in currentState. */
	UFUNCTION(BlueprintPure, Category = "Insimul|Trade")
	int32 PlayerGold() const;

	/** The player's live inventory stacks. */
	UFUNCTION(BlueprintPure, Category = "Insimul|Trade")
	TArray<FInsimulTradeItem> PlayerItems() const;

	/** A container's live stacks (empty if the container is absent). */
	UFUNCTION(BlueprintPure, Category = "Insimul|Trade")
	TArray<FInsimulTradeItem> ContainerItems(const FString& ContainerId) const;

	/** A merchant's live stock (empty if the merchant is absent). */
	UFUNCTION(BlueprintPure, Category = "Insimul|Trade")
	TArray<FInsimulTradeItem> MerchantItems(const FString& MerchantId) const;

	/** A merchant's gold reserve. */
	UFUNCTION(BlueprintPure, Category = "Insimul|Trade")
	int32 MerchantGold(const FString& MerchantId) const;

	/** Take `Qty` (<=0 = whole stack) of an item from a container into inventory. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Trade")
	FInsimulTradeResult TakeFromContainer(const FString& ContainerId, const FString& ItemId, int32 Qty = 0);

	/** Take every stack from a container into inventory. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Trade")
	FInsimulTradeResult TakeAllFromContainer(const FString& ContainerId);

	/** Buy `Qty` of an item from a merchant (item merchant->player, gold player->merchant). */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Trade")
	FInsimulTradeResult Buy(const FString& MerchantId, const FString& ItemId, int32 Qty);

	/** Sell `Qty` of an item to a merchant (item player->merchant, gold merchant->player). */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Trade")
	FInsimulTradeResult Sell(const FString& MerchantId, const FString& ItemId, int32 Qty);

	// ── The save is the store (Portable/InsimulUIStateBinding.h) ─────────────

	/**
	 * Hydrate this panel's slice out of the booted runtime's SaveFile and remember
	 * the runtime, so CommitToSave() can write it back. Returns false before the
	 * runtime is ready or when the save has no currentState — a panel over a save
	 * that is not one must say so rather than show a plausible empty inventory.
	 */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Trade")
	bool BindToRuntime(UInsimulRuntimeSubsystem* Runtime);

	/** Write the mutated slice back into the bound runtime's save, in place. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Trade")
	bool CommitToSave();

	/**
	 * What `MerchantId` charges for (or pays for) `ItemId` HERE, with every term:
	 * the business's markup, the shelf's scarcity, the player's STANDING with the
	 * faction the shop answers to, and the proprietor's own discount. The market is
	 * read from the bound save, so the price is a function of the simulation rather
	 * than of a shop table.
	 */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Trade")
	FInsimulPriceQuote QuotePrice(const FString& MerchantId, const FString& ItemId,
		int32 Qty = 1, bool bSell = false) const;

	virtual void BeginDestroy() override;

private:
	/** The runtime whose SaveFile is this panel's one store (weak — it outlives us). */
	TWeakObjectPtr<UInsimulRuntimeSubsystem> BoundRuntime;

	/** The save's currentState slice this model reads/writes (single source of truth). */
	TUniquePtr<insimul::FTradeState> State;

	/** The host-tested portable model, attached to State. */
	TUniquePtr<insimul::FInsimulTradeModel> Model;

	/** Ensure State + Model exist (attached) so a Blueprint call is always safe. */
	insimul::FInsimulTradeModel& EnsureModel();
};
