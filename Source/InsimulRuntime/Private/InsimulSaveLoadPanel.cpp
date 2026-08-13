// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulSaveLoadPanel — thin UUserWidget over UInsimulSaveSlotPanel. Every
// rendering decision (status, title, the corrupted-envelope message, the load and
// overwrite gates) belongs to the portable model and is pinned against the shared
// corpus by ctest `ui_save_slots`.

#include "InsimulSaveLoadPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"

const FName UInsimulSaveLoadPanel::PanelKey = FName(TEXT("save_load"));

UInsimulSaveSlotPanel& UInsimulSaveLoadPanel::EnsureModel()
{
	if (!Model)
	{
		Model = NewObject<UInsimulSaveSlotPanel>(this);
	}
	return *Model;
}

void UInsimulSaveLoadPanel::SetSlots(const TArray<FInsimulSaveSlotResult>& Results)
{
	EnsureModel().SetSlots(Results);
	// A selection that no longer names a row is dropped rather than left dangling.
	if (Selected != INDEX_NONE)
	{
		bool bStillThere = false;
		for (const FInsimulSaveSlotResult& Result : Results)
		{
			bStillThere = bStillThere || Result.Index == Selected;
		}
		if (!bStillThere)
		{
			Selected = INDEX_NONE;
		}
	}
	Repaint();
}

TArray<FInsimulSaveSlotView> UInsimulSaveLoadPanel::Rows() const
{
	return Model ? Model->Slots() : TArray<FInsimulSaveSlotView>();
}

FInsimulSaveSlotView UInsimulSaveLoadPanel::Row(int32 SlotIndex) const
{
	return Model ? Model->Slot(SlotIndex) : FInsimulSaveSlotView();
}

bool UInsimulSaveLoadPanel::HasAnyLoadable() const
{
	return Model ? Model->HasAnyLoadable() : false;
}

void UInsimulSaveLoadPanel::SelectSlot(int32 SlotIndex)
{
	Selected = SlotIndex;
	Repaint();
}

bool UInsimulSaveLoadPanel::CanLoadSelected() const
{
	return Selected != INDEX_NONE && Row(Selected).bCanLoad;
}

bool UInsimulSaveLoadPanel::CanSaveSelected() const
{
	return Selected != INDEX_NONE && Row(Selected).bCanSave;
}

bool UInsimulSaveLoadPanel::RequestLoad()
{
	if (!CanLoadSelected())
	{
		// The refusal is the model's — a corrupted envelope says why in its row.
		UE_LOG(LogTemp, Warning, TEXT("Insimul save/load: slot %d cannot be loaded (%s)"), Selected,
			*Row(Selected).Message);
		return false;
	}
	OnLoadRequested.Broadcast(Selected);
	return true;
}

bool UInsimulSaveLoadPanel::RequestSave()
{
	if (!CanSaveSelected())
	{
		return false;
	}
	OnSaveRequested.Broadcast(Selected);
	return true;
}

void UInsimulSaveLoadPanel::Repaint()
{
	const TArray<FInsimulSaveSlotView> Views = Rows();

	if (SlotListBox)
	{
		SlotListBox->ClearChildren();
		for (const FInsimulSaveSlotView& View : Views)
		{
			if (SlotRowWidgetClass)
			{
				if (UUserWidget* RowWidget = CreateWidget<UUserWidget>(this, SlotRowWidgetClass))
				{
					SlotListBox->AddChild(RowWidget);
					continue;
				}
			}
			// The fallback line: a creator with no row widget still sees the slot,
			// its title and — when it is broken — why it is broken.
			if (UWidgetTree* Tree = WidgetTree)
			{
				UTextBlock* Line = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
				const FString Text = View.Message.IsEmpty()
					? FString::Printf(TEXT("%d. %s"), View.Index, *View.Title)
					: FString::Printf(TEXT("%d. %s — %s"), View.Index, *View.Title, *View.Message);
				Line->SetText(FText::FromString(Text));
				SlotListBox->AddChild(Line);
			}
		}
	}

	if (EmptyListText)
	{
		EmptyListText->SetText(Views.Num() == 0
				? FText::FromString(TEXT("No save slots."))
				: FText::GetEmpty());
	}
	if (StatusText)
	{
		StatusText->SetText(Selected == INDEX_NONE ? FText::GetEmpty()
													: FText::FromString(Row(Selected).Message));
	}
	OnSlotsChanged.Broadcast();
}

FString UInsimulSaveLoadPanel::Describe() const
{
	int32 Corrupted = 0;
	const TArray<FInsimulSaveSlotView> Views = Rows();
	for (const FInsimulSaveSlotView& View : Views)
	{
		if (View.Status == TEXT("corrupted"))
		{
			Corrupted++;
		}
	}
	return FString::Printf(TEXT("save/load: %d slot(s), %d corrupted, %s"), Views.Num(), Corrupted,
		HasAnyLoadable() ? TEXT("at least one loadable") : TEXT("none loadable"));
}
