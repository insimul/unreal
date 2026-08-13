// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulQuickBar — the action shortcuts (US-2 of tasklist 190), panel key
// `quickbar`. Every world has actions, so the bar itself is ungated; what is ON it
// is the host's, which is why nothing here compiles an action list in.
//
// THE SAME ENTRIES THE WHEEL HOLDS. The bar takes FInsimulRadialEntry, the struct
// UInsimulRadialMenu already uses, on purpose: the quick bar and the radial menu are
// two INPUT surfaces over one set of shortcuts, and a creator who changes what one
// carries must change both. A second entry struct would be the drift itself.
//
// A DISABLED ENTRY IS SHOWN AND GREYED, never hidden — the player is told what
// exists, then why not. Firing one does nothing but broadcast: whether an actor may
// perform an action is `can_perform/2` and `forbids/4`, the simulation's answer, and
// a bar that ran the action itself would be a second rule.
//
// Thin, syntax-gated UMG boundary.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InsimulRadialMenu.h"
#include "InsimulQuickBar.generated.h"

class UPanelWidget;
class UTextBlock;

/** Fired when a slot is triggered (a hotkey, a click, a gamepad face button). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInsimulQuickBarTriggered, int32, SlotIndex,
	const FString&, ActionId);

UCLASS()
class INSIMULRUNTIME_API UInsimulQuickBar : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The stable panel key this widget serves (see the shipped panel catalog). */
	static const FName PanelKey;

	/** Replace the bar's entries and repaint. Entries past SlotCount are kept but
	 *  not drawn, so a host may hand over one list and let the bar page it later. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	void SetEntries(const TArray<FInsimulRadialEntry>& InEntries);

	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	const TArray<FInsimulRadialEntry>& Entries() const { return BarEntries; }

	/** The entries actually drawn — the first SlotCount of them. */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	TArray<FInsimulRadialEntry> VisibleEntries() const;

	/** The action id in a slot, or empty for an out-of-range or empty slot. */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	FString ActionAt(int32 SlotIndex) const;

	/**
	 * Fire a slot. Returns false for an out-of-range or disabled slot — a REQUEST
	 * was broadcast, never an action performed (see the header note).
	 */
	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	bool Trigger(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	void Refresh();

	UPROPERTY(BlueprintAssignable, Category = "Insimul|UI")
	FOnInsimulQuickBarTriggered OnTriggered;

	/** How many slots this bar draws. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|UI")
	int32 SlotCount = 8;

	/** The slot widget class a creator supplies; unset builds a plain text row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|UI")
	TSubclassOf<UUserWidget> SlotWidgetClass;

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> SlotContainer;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyBarText;

private:
	UPROPERTY(EditAnywhere, Category = "Insimul|UI")
	TArray<FInsimulRadialEntry> BarEntries;
};
