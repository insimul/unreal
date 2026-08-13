// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulMerchantPanel — the shop panel WITH ITS REASONS (US-2 of tasklist 190),
// panel key `merchant`.
//
// The point of this panel is not the two lists; it is the number between them. A
// price here is a function of the simulation the world already runs — the business's
// markup, what is left on this vendor's shelf, the player's STANDING with the
// faction the shop answers to, and whether the player owns the place — and every
// term is shown, because a player charged more than the next town charges is owed
// the reason. `PriceReason()` is that line, built from the quote's own terms rather
// than from a string the panel invents.
//
// Stock, gold and reputation are all read from save.currentState through
// UInsimulTradePanel; a completed trade is committed to the save at once. The price
// semantics are the portable FInsimulTradePricing's, pinned case for case against
// the shared items corpus by ctest `ui_state_binding`.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InsimulTradePanel.h"
#include "InsimulMerchantPanel.generated.h"

class UTextBlock;
class UVerticalBox;

/** Fired after a repaint (an open, a buy, a sell). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInsimulMerchantChanged);

UCLASS()
class INSIMULRUNTIME_API UInsimulMerchantPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The stable panel key this widget serves (see the shipped panel catalog). */
	static const FName PanelKey;

	/** Point the panel at a merchant and repaint. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	void Open(UInsimulTradePanel* InTrade, const FString& MerchantId);

	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	void Close();

	/** The merchant's live shelf. */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	TArray<FInsimulTradeItem> MerchantStock() const;

	/** The player's live stacks (the sell side). */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	TArray<FInsimulTradeItem> PlayerStock() const;

	/** What this item costs HERE, with every term that made the number. */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	FInsimulPriceQuote Price(const FString& ItemId, int32 Qty = 1, bool bSell = false) const;

	/**
	 * The player-facing reason line for a quote — "120 (markup +20% ironmongers,
	 * standing -25% town_watch)". Empty terms yield just the number, which is the
	 * correct answer for a world that simulates no economy.
	 */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	FString PriceReason(const FInsimulPriceQuote& Quote) const;

	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	FInsimulTradeResult Buy(const FString& ItemId, int32 Qty = 1);

	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	FInsimulTradeResult Sell(const FString& ItemId, int32 Qty = 1);

	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	FString CurrentMerchantId() const { return MerchantId; }

	UPROPERTY(BlueprintAssignable, Category = "Insimul|UI")
	FOnInsimulMerchantChanged OnMerchantChanged;

	/** The row widget class a creator supplies; unset builds a plain text row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|UI")
	TSubclassOf<UUserWidget> ItemRowClass;

	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	void Refresh();

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> MerchantListBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> PlayerListBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MerchantNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayerGoldText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MerchantGoldText;

private:
	UPROPERTY()
	TObjectPtr<UInsimulTradePanel> TradeModel;

	FString MerchantId;

	/** One list, built the same way on both sides, priced for its own direction. */
	void FillList(UVerticalBox* Box, const TArray<FInsimulTradeItem>& Stacks, bool bSell);
};
