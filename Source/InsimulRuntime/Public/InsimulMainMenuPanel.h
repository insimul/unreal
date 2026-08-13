// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulMainMenuPanel — the front screen (US-3 of tasklist 190), panel key
// `main_menu`. Title, New Game, Continue, Load, Settings, Quit.
//
// CONTINUE IS A MEASUREMENT, NOT A FLAG. Whether the player may continue, and
// WHICH slot they would resume, are one question — "the most recently saved slot
// that loads" — and it is answered by FInsimulSaveSlotModel::ContinueSlot()
// (UInsimulSaveSlotPanel), the same core the save/load screen renders and the same
// core ctest `ui_save_slots` gates. A menu that kept its own "has save" boolean
// would be a second opinion about the save directory, and it would offer Continue
// on a save whose integrity check fails. The corrupted-envelope case is exactly
// the one this must get right: a tampered slot is NOT continuable however recent
// it looks, and the reason is shown rather than swallowed.
//
// THE MENU ASKS, THE GAME ANSWERS. Every entry is a delegate. Loading a level,
// writing a save and quitting are the game mode's business; a panel that called
// UGameplayStatics itself could not be swapped out through the registry, which is
// the thing tasklist 190 exists to make possible.
//
// Thin, syntax-gated UMG boundary over a core host-tested UE-free.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InsimulSaveSlotPanel.h"
#include "InsimulMainMenuPanel.generated.h"

class UInsimulSaveSlotPanel;
class UButton;
class UTextBlock;

/** Fired when the player picks an entry. `Continue` carries the slot it resolved. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInsimulMainMenuEntry);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInsimulContinueRequested, int32, SlotIndex);

UCLASS()
class INSIMULRUNTIME_API UInsimulMainMenuPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The stable panel key this widget serves (see the shipped panel catalog). */
	static const FName PanelKey;

	/** The game's title, as the world declared it. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|MainMenu")
	void SetTitle(const FString& InTitle);

	UFUNCTION(BlueprintPure, Category = "Insimul|MainMenu")
	FString Title() const { return GameTitle; }

	/**
	 * Feed the codec-reported slot outcomes (from the portable save system). This is
	 * what decides Continue: no slots, or only broken ones, and the entry is
	 * disabled with the model's own message under it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Insimul|MainMenu")
	void SetSlots(const TArray<FInsimulSaveSlotResult>& Results);

	/** True when at least one slot loads. */
	UFUNCTION(BlueprintPure, Category = "Insimul|MainMenu")
	bool CanContinue() const;

	/** The slot Continue would resume: the most recently saved loadable one, or
	 *  INDEX_NONE. The ordering rule — the codec's ISO-8601 savedAt, never a local
	 *  clock — is the portable model's and is gated by ctest `ui_save_slots`. */
	UFUNCTION(BlueprintPure, Category = "Insimul|MainMenu")
	int32 ContinueSlot() const;

	/** Why Continue is unavailable ("" when it is available) — the slot model's
	 *  cross-engine message when the only save present is corrupted. */
	UFUNCTION(BlueprintPure, Category = "Insimul|MainMenu")
	FString ContinueBlockedReason() const;

	// ── Entries ───────────────────────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "Insimul|MainMenu")
	void RequestNewGame();

	/** Resume the most recent loadable slot. Refused (false) when there is none. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|MainMenu")
	bool RequestContinue();

	UFUNCTION(BlueprintCallable, Category = "Insimul|MainMenu")
	void RequestLoadGame();

	UFUNCTION(BlueprintCallable, Category = "Insimul|MainMenu")
	void RequestSettings();

	UFUNCTION(BlueprintCallable, Category = "Insimul|MainMenu")
	void RequestQuit();

	/** One line for a boot log or a bug report. */
	UFUNCTION(BlueprintPure, Category = "Insimul|MainMenu")
	FString Describe() const;

	UPROPERTY(BlueprintAssignable, Category = "Insimul|MainMenu")
	FOnInsimulMainMenuEntry OnNewGameRequested;

	UPROPERTY(BlueprintAssignable, Category = "Insimul|MainMenu")
	FOnInsimulContinueRequested OnContinueRequested;

	UPROPERTY(BlueprintAssignable, Category = "Insimul|MainMenu")
	FOnInsimulMainMenuEntry OnLoadGameRequested;

	UPROPERTY(BlueprintAssignable, Category = "Insimul|MainMenu")
	FOnInsimulMainMenuEntry OnSettingsRequested;

	UPROPERTY(BlueprintAssignable, Category = "Insimul|MainMenu")
	FOnInsimulMainMenuEntry OnQuitRequested;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> NewGameButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ContinueButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> LoadGameButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SettingsButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> QuitButton;

	/** Where a blocked Continue explains itself. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ContinueHintText;

	UFUNCTION()
	void HandleNewGameClicked();

	UFUNCTION()
	void HandleContinueClicked();

	UFUNCTION()
	void HandleLoadGameClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleQuitClicked();

private:
	/** The host-tested slot core (ctest `ui_save_slots`) — the Continue gate. */
	UPROPERTY(Transient)
	TObjectPtr<UInsimulSaveSlotPanel> Slots;

	UPROPERTY(Transient)
	FString GameTitle;

	UInsimulSaveSlotPanel& EnsureSlots();
	void Repaint();
};
