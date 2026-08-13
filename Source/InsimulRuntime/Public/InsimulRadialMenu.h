// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulRadialMenu — the gamepad wheel (US-2 of tasklist 190), panel key
// `radial_menu`.
//
// The same entries the quickbar holds, arranged around a circle: this is an input
// surface, not a second action list, so a creator who changes what the quickbar
// carries changes both. Entry zero sits at the TOP and the wheel runs clockwise,
// which is the convention every engine leg follows so a player's muscle memory
// survives a port.
//
// The one piece of logic here — angle to index — is deliberately small and total: an
// empty wheel selects nothing (index -1) rather than dividing by zero, and an angle
// outside [0, 360) is wrapped rather than clamped, because a stick reading 361
// degrees means the same thing as one reading 1.
//
// Thin, syntax-gated UMG boundary.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InsimulRadialMenu.generated.h"

class UTextBlock;
class UVerticalBox;

/** One wheel entry: the action id the host will run, and what the player reads. */
USTRUCT(BlueprintType)
struct FInsimulRadialEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|UI")
	FString ActionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|UI")
	FString Label;

	/** A greyed entry is still shown — the player is told what exists, then why not. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|UI")
	bool bEnabled = true;
};

/** Fired when the highlighted entry changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInsimulRadialSelectionChanged, int32, Index);

/** Fired when an entry is committed (the stick released, the button pressed). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInsimulRadialCommitted, const FString&, ActionId);

UCLASS()
class INSIMULRUNTIME_API UInsimulRadialMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The stable panel key this widget serves (see the shipped panel catalog). */
	static const FName PanelKey;

	/** Replace the wheel's entries and repaint. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	void SetEntries(const TArray<FInsimulRadialEntry>& InEntries);

	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	const TArray<FInsimulRadialEntry>& Entries() const { return WheelEntries; }

	/**
	 * The entry a stick angle points at — 0 degrees is the top, clockwise. Returns
	 * -1 for an empty wheel.
	 */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	int32 IndexForAngle(float Degrees) const;

	/** Highlight the entry at `Degrees`. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	void HighlightAngle(float Degrees);

	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	int32 SelectedIndex() const { return Selected; }

	/** Fire the highlighted entry. A disabled or absent entry commits nothing. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	bool Commit();

	UPROPERTY(BlueprintAssignable, Category = "Insimul|UI")
	FOnInsimulRadialSelectionChanged OnSelectionChanged;

	UPROPERTY(BlueprintAssignable, Category = "Insimul|UI")
	FOnInsimulRadialCommitted OnCommitted;

	/** The wedge widget class a creator supplies; unset builds a plain text row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|UI")
	TSubclassOf<UUserWidget> WedgeClass;

	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	void Refresh();

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> WedgeContainer;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SelectionLabelText;

private:
	UPROPERTY(EditAnywhere, Category = "Insimul|UI")
	TArray<FInsimulRadialEntry> WheelEntries;

	int32 Selected = -1;
};
