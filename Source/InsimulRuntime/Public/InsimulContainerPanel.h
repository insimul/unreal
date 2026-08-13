// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulContainerPanel — the loot / container panel (US-2 of tasklist 190), panel
// key `container`.
//
// A container's contents are a place in the ledger, not a widget's array: the rows
// are read from save.currentState.containers.containers[id].items and a take MOVES
// the stack, so the item count across the world is the same before and after (the
// conservation invariant ctest `ui_trade` asserts). The panel commits to the save
// after every transfer, which is why closing the loot window cannot lose an item.
//
// Thin, syntax-gated UMG boundary over UInsimulTradePanel.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InsimulTradePanel.h"
#include "InsimulContainerPanel.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;

/** Fired after a repaint (a take, an open, a close). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInsimulContainerChanged);

UCLASS()
class INSIMULRUNTIME_API UInsimulContainerPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The stable panel key this widget serves (see the shipped panel catalog). */
	static const FName PanelKey;

	/** Point the panel at a container and repaint. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	void Open(UInsimulTradePanel* InTrade, const FString& ContainerId);

	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	void Close();

	/** The container's live stacks. */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	TArray<FInsimulTradeItem> Items() const;

	/** Move `Qty` (<=0 = the whole stack) into the player's inventory, then commit. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	FInsimulTradeResult Take(const FString& ItemId, int32 Qty = 0);

	/** Move every stack into the player's inventory, then commit. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	FInsimulTradeResult TakeAll();

	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	FString CurrentContainerId() const { return ContainerId; }

	UPROPERTY(BlueprintAssignable, Category = "Insimul|UI")
	FOnInsimulContainerChanged OnContainerChanged;

	/** The row widget class a creator supplies; unset builds a plain text row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|UI")
	TSubclassOf<UUserWidget> ItemRowClass;

	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	void Refresh();

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> ContainerListBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> TakeAllButton;

private:
	UPROPERTY()
	TObjectPtr<UInsimulTradePanel> TradeModel;

	FString ContainerId;
};
