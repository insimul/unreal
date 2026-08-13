// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulNoticeBoard — thin UUserWidget over UInsimulQuestJournal. The board reads
// the journal's available tab and repaints on the journal's event, so a radiant
// arrival appears without anything polling.

#include "InsimulNoticeBoard.h"

#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

const FName UInsimulNoticeBoard::PanelKey = FName(TEXT("notice_board"));

void UInsimulNoticeBoard::Bind(UInsimulQuestJournal* InJournal)
{
	if (Journal)
	{
		Journal->OnQuestJournalChanged.RemoveDynamic(this, &UInsimulNoticeBoard::HandleJournalChanged);
	}
	Journal = InJournal;
	if (Journal)
	{
		Journal->OnQuestJournalChanged.AddDynamic(this, &UInsimulNoticeBoard::HandleJournalChanged);
	}
	Refresh();
}

TArray<FInsimulQuestEntry> UInsimulNoticeBoard::Offers() const
{
	TArray<FInsimulQuestEntry> Out;
	if (!Journal)
	{
		return Out;
	}
	// The journal owns the filter; the board asks for one tab and puts it back, so
	// two panels open at once cannot leave each other on the wrong tab.
	for (const FInsimulQuestEntry& Entry : Journal->Filtered())
	{
		if (Entry.Status == TEXT("available"))
		{
			Out.Add(Entry);
		}
	}
	return Out;
}

bool UInsimulNoticeBoard::Accept(const FString& QuestId)
{
	return Journal && Journal->Accept(QuestId);
}

bool UInsimulNoticeBoard::Decline(const FString& QuestId)
{
	return Journal && Journal->Decline(QuestId);
}

void UInsimulNoticeBoard::HandleJournalChanged(EInsimulQuestJournalEvent EventKind,
	const FString& QuestId)
{
	// Every kind repaints: an accept empties a row, a radiant arrival adds one, and
	// a filter change moves what Filtered() reports.
	(void)EventKind;
	(void)QuestId;
	Refresh();
}

void UInsimulNoticeBoard::Refresh()
{
	const TArray<FInsimulQuestEntry> Posted = Offers();

	if (EmptyBoardText)
	{
		EmptyBoardText->SetVisibility(Posted.Num() == 0 ? ESlateVisibility::Visible
														: ESlateVisibility::Collapsed);
	}

	if (NoticeListBox)
	{
		NoticeListBox->ClearChildren();
		for (const FInsimulQuestEntry& Entry : Posted)
		{
			if (NoticeRowClass)
			{
				if (UUserWidget* Row = CreateWidget<UUserWidget>(this, NoticeRowClass))
				{
					NoticeListBox->AddChildToVerticalBox(Row);
					continue;
				}
			}
			UTextBlock* Row = NewObject<UTextBlock>(this);
			Row->SetText(FText::FromString(Entry.Title.IsEmpty() ? Entry.Id : Entry.Title));
			NoticeListBox->AddChildToVerticalBox(Row);
		}
	}

	OnNoticeBoardChanged.Broadcast();
}
