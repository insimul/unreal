// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulInventoryPanel — the inventory panel (US-2 of tasklist 190), panel key
// `inventory`.
//
// It owns NO items. Every stack it renders is read through UInsimulTradePanel from
// save.currentState.player.inventory, and the gold beside them is
// currentState.player.gold: a panel that cached the list would be a second store of
// the one thing this platform says lives in exactly one place. That is why there is
// no AddItem() here — items arrive because the ledger moved, and the panel repaints.
//
// The row widget class is a creator's to replace; with none set the panel builds a
// plain text row per stack, which is enough for the default game.
//
// Thin, syntax-gated UMG boundary: the transfer semantics are the portable trade
// model's (host-tested by ctest `ui_trade`) and the save binding's (ctest
// `ui_state_binding`).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InsimulTradePanel.h"
#include "InsimulInventoryPanel.generated.h"

class UInsimulRuntimeSubsystem;
class UTextBlock;
class UVerticalBox;

/** Fired after a repaint so a designer's WBP can rebuild its own rows. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInsimulInventoryChanged);

UCLASS()
class INSIMULRUNTIME_API UInsimulInventoryPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The stable panel key this widget serves (see the shipped panel catalog). */
	static const FName PanelKey;

	/** Bind to the booted runtime's save — the one store this panel reads. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	bool BindToRuntime(UInsimulRuntimeSubsystem* Runtime);

	/** The trade view-model this panel and the container / merchant panels share. */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	UInsimulTradePanel* Trade() const { return TradeModel; }

	/** Re-read the ledger and rebuild the rows. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	void Refresh();

	/** The player's live stacks, straight off currentState. */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	TArray<FInsimulTradeItem> Items() const;

	/** The player's gold, straight off currentState. */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	int32 Gold() const;

	UPROPERTY(BlueprintAssignable, Category = "Insimul|UI")
	FOnInsimulInventoryChanged OnInventoryChanged;

	/** The row widget class a creator supplies; unset builds a plain text row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|UI")
	TSubclassOf<UUserWidget> ItemRowClass;

	virtual void NativeConstruct() override;

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> ItemListBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GoldText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyText;

private:
	UPROPERTY()
	TObjectPtr<UInsimulTradePanel> TradeModel;

	UInsimulTradePanel& EnsureTrade();
};
