// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulHUDPanel — the HUD frame (US-2 of tasklist 190), panel key `hud`.
//
// THE FRAME IS UNCONDITIONAL; ITS SUB-PANELS ARE NOT. Every world has a HUD, and
// almost nothing on it is guaranteed: the corner map belongs to the map module, the
// quest tracker to quests, the quick bar to the shared action vocabulary. So this
// widget holds no list of what a HUD contains — it holds the KEYS its slots are
// for, and asks UInsimulUIPanelSurface which of them this world may show. A HUD
// that constructed its own children would have decided for itself whether a
// mechanic is on, which is the thing the module contract forbids and the thing a
// creator most needs to override.
//
// WITHHOLDING IS REPORTED. A sub-panel this world does not have is recorded in
// WithheldSubPanels() and named by Describe(), never silently missing: a creator
// looking at a HUD with no minimap must be able to find out in one line that the
// genre bundle did not select the module that owns it.
//
// Thin, syntax-gated UMG boundary over the resolver ctest `ui_registry` proves.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InsimulHUDPanel.generated.h"

class UInsimulUIPanelSurface;
class UPanelWidget;
class UTextBlock;

/** Fired after the HUD rebuilds its slots (an apply, a re-bind, a Refresh). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInsimulHUDChanged);

UCLASS()
class INSIMULRUNTIME_API UInsimulHUDPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	UInsimulHUDPanel();

	/** The stable panel key this widget serves (see the shipped panel catalog). */
	static const FName PanelKey;

	/**
	 * Point the HUD at the game instance's panel surface and build the slots this
	 * world may show. Passing nullptr leaves the HUD empty and says so — it does NOT
	 * fall back to showing everything, because "no surface" is a wiring bug and a
	 * HUD that hid it would make it invisible.
	 */
	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	void Bind(UInsimulUIPanelSurface* InSurface);

	/** Rebuild from the surface's current answers (call after a module set lands). */
	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	void Refresh();

	/** The sub-panel keys this HUD asks for, in draw order. Creator-editable. */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	const TArray<FName>& SubPanelKeys() const { return SlotKeys; }

	/** Replace the slot list (a creator's HUD may carry more or fewer). */
	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	void SetSubPanelKeys(const TArray<FName>& InKeys);

	/** The sub-panels this world may show, in slot order. */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	TArray<FName> VisibleSubPanels() const;

	/** The sub-panels withheld because their module is not active, in slot order. */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	TArray<FName> WithheldSubPanels() const;

	/** The widget serving a slot, or nullptr when it was withheld / unknown. */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	UUserWidget* SubPanel(FName PanelKey) const;

	/** One line for a boot log or a bug report — names what was withheld and why. */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	FString Describe() const;

	UPROPERTY(BlueprintAssignable, Category = "Insimul|UI")
	FOnInsimulHUDChanged OnHUDChanged;

protected:
	/** Where the resolved sub-panels are added. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> SlotContainer;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DiagnosticText;

	/** The slots a default HUD carries, in draw order. Every one is a CATALOG key,
	 *  so which of them this world has is data rather than code. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|UI")
	TArray<FName> SlotKeys;

private:
	UPROPERTY(Transient)
	TObjectPtr<UInsimulUIPanelSurface> Surface;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UUserWidget>> Resolved;

	UPROPERTY(Transient)
	TArray<FName> Withheld;
};
