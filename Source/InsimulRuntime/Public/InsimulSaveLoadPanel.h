// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulSaveLoadPanel — the save/load slot screen (US-3 of tasklist 190), panel
// key `save_load`. The tab the ESC menu hosts, and the screen the main menu opens.
//
// IT RENDERS OUTCOMES, IT DOES NOT JUDGE THEM. Each slot arrives as a codec-reported
// OUTCOME from the portable save system — empty / ok (+ a summary) / one of the
// envelope failures (invalid_format / missing_save_file / integrity_mismatch, which
// is what a tampered or truncated save produces against its SHA-256). Turning that
// into a row — the status, the title, the message, whether it may be loaded and
// whether it may be overwritten — is FInsimulSaveSlotModel's, and the MESSAGE a
// corrupted envelope shows is a cross-engine contract: the same words on Babylon,
// Unity, Godot and here (ctest `ui_save_slots`, conformance/ui/save-slot-cases.json).
//
// A CORRUPTED SLOT IS SHOWN, NOT HIDDEN. Dropping the row would tell a player their
// save "vanished"; the panel shows it, says what failed, refuses Load and still
// offers Save, because overwriting a broken slot is exactly the recovery a player
// needs. That behaviour is the model's and is pinned by the corpus.
//
// Thin, syntax-gated UMG boundary over a core host-tested UE-free.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InsimulSaveSlotPanel.h"
#include "InsimulSaveLoadPanel.generated.h"

class UInsimulSaveSlotPanel;
class UPanelWidget;
class UTextBlock;

/** Fired when the player asks to load / overwrite a slot. The screen never loads
 *  or writes anything itself — the save subsystem does, and it owns the codec. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInsimulSlotChosen, int32, SlotIndex);

/** Fired after the rows are rebuilt (new outcomes, a new selection). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInsimulSlotsChanged);

UCLASS()
class INSIMULRUNTIME_API UInsimulSaveLoadPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The stable panel key this widget serves (see the shipped panel catalog). */
	static const FName PanelKey;

	/** Feed the codec-reported outcomes (from the portable save system) and repaint. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Save")
	void SetSlots(const TArray<FInsimulSaveSlotResult>& Results);

	/** The rendered rows, in slot-index order. */
	UFUNCTION(BlueprintPure, Category = "Insimul|Save")
	TArray<FInsimulSaveSlotView> Rows() const;

	/** The rendered row for one slot index. */
	UFUNCTION(BlueprintPure, Category = "Insimul|Save")
	FInsimulSaveSlotView Row(int32 SlotIndex) const;

	/** True when any slot is loadable (the Continue / Load gate). */
	UFUNCTION(BlueprintPure, Category = "Insimul|Save")
	bool HasAnyLoadable() const;

	/** Select a row. -1 clears the selection. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Save")
	void SelectSlot(int32 SlotIndex);

	UFUNCTION(BlueprintPure, Category = "Insimul|Save")
	int32 SelectedSlot() const { return Selected; }

	/** Whether the selected row may be loaded / overwritten (the model's answer —
	 *  a corrupted slot refuses the first and allows the second). */
	UFUNCTION(BlueprintPure, Category = "Insimul|Save")
	bool CanLoadSelected() const;

	UFUNCTION(BlueprintPure, Category = "Insimul|Save")
	bool CanSaveSelected() const;

	/** Ask to load / overwrite the selected slot. Refused (false) when the row says
	 *  it cannot be — a corrupted envelope never loads by accident. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Save")
	bool RequestLoad();

	UFUNCTION(BlueprintCallable, Category = "Insimul|Save")
	bool RequestSave();

	/** One line for a boot log or a bug report: how many slots, how many corrupted. */
	UFUNCTION(BlueprintPure, Category = "Insimul|Save")
	FString Describe() const;

	UPROPERTY(BlueprintAssignable, Category = "Insimul|Save")
	FOnInsimulSlotChosen OnLoadRequested;

	UPROPERTY(BlueprintAssignable, Category = "Insimul|Save")
	FOnInsimulSlotChosen OnSaveRequested;

	UPROPERTY(BlueprintAssignable, Category = "Insimul|Save")
	FOnInsimulSlotsChanged OnSlotsChanged;

	/** The row widget class a creator supplies; unset paints a plain text line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Save")
	TSubclassOf<UUserWidget> SlotRowWidgetClass;

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> SlotListBox;

	/** The selected row's message — where the corrupted-envelope text lands. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyListText;

private:
	/** The host-tested rendering core (ctest `ui_save_slots`). */
	UPROPERTY(Transient)
	TObjectPtr<UInsimulSaveSlotPanel> Model;

	int32 Selected = INDEX_NONE;

	UInsimulSaveSlotPanel& EnsureModel();
	void Repaint();
};
