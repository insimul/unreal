// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulNoticeBoard — the offers board (US-2 of tasklist 190), panel key
// `notice_board`.
//
// The board is the PULL half of quest delivery: a radiant arrival that nobody was
// standing next to still has to be findable, so it lands in the journal as an
// AVAILABLE quest and the board is where the player reads it. That is the whole
// difference between this and the offer dialog — same quests, same accept/decline,
// one pushed at you by an NPC and one posted on a wall.
//
// It holds no quest list of its own: the rows are the journal model's `available`
// tab, and accepting one is the journal's Accept() (available -> active), which is
// the single lifecycle every engine leg runs. Quests are not a mechanic module, so
// the board is ungated — every genre bundle has it.
//
// Thin, syntax-gated UMG boundary over UInsimulQuestJournal.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InsimulQuestJournal.h"
#include "InsimulNoticeBoard.generated.h"

class UTextBlock;
class UVerticalBox;

/** Fired after a repaint (an arrival, an accept, a decline). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInsimulNoticeBoardChanged);

UCLASS()
class INSIMULRUNTIME_API UInsimulNoticeBoard : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The stable panel key this widget serves (see the shipped panel catalog). */
	static const FName PanelKey;

	/** Point the board at the journal every quest panel shares. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Quest")
	void Bind(UInsimulQuestJournal* InJournal);

	/** The posted offers — the journal's available quests, in stable order. */
	UFUNCTION(BlueprintPure, Category = "Insimul|Quest")
	TArray<FInsimulQuestEntry> Offers() const;

	/** Take the job (available -> active). */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Quest")
	bool Accept(const FString& QuestId);

	/** Tear the notice down (the offer is removed, not merely hidden). */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Quest")
	bool Decline(const FString& QuestId);

	UPROPERTY(BlueprintAssignable, Category = "Insimul|Quest")
	FOnInsimulNoticeBoardChanged OnNoticeBoardChanged;

	/** The notice widget class a creator supplies; unset builds a plain text row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Quest")
	TSubclassOf<UUserWidget> NoticeRowClass;

	UFUNCTION(BlueprintCallable, Category = "Insimul|Quest")
	void Refresh();

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> NoticeListBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyBoardText;

private:
	UPROPERTY()
	TObjectPtr<UInsimulQuestJournal> Journal;

	/** Repaint on the journal's own event — push, never a per-frame poll. */
	UFUNCTION()
	void HandleJournalChanged(EInsimulQuestJournalEvent EventKind, const FString& QuestId);
};
