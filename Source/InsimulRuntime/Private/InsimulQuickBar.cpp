// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulQuickBar — thin UUserWidget over the same entries the radial wheel holds.
// Triggering broadcasts; it never performs.

#include "InsimulQuickBar.h"

#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"

const FName UInsimulQuickBar::PanelKey = FName(TEXT("quickbar"));

void UInsimulQuickBar::SetEntries(const TArray<FInsimulRadialEntry>& InEntries)
{
	BarEntries = InEntries;
	Refresh();
}

TArray<FInsimulRadialEntry> UInsimulQuickBar::VisibleEntries() const
{
	TArray<FInsimulRadialEntry> Out;
	const int32 Count = FMath::Min(BarEntries.Num(), FMath::Max(SlotCount, 0));
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Out.Add(BarEntries[Index]);
	}
	return Out;
}

FString UInsimulQuickBar::ActionAt(int32 SlotIndex) const
{
	if (SlotIndex < 0 || SlotIndex >= FMath::Min(BarEntries.Num(), FMath::Max(SlotCount, 0)))
	{
		return FString();
	}
	return BarEntries[SlotIndex].ActionId;
}

bool UInsimulQuickBar::Trigger(int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= FMath::Min(BarEntries.Num(), FMath::Max(SlotCount, 0)))
	{
		return false;
	}
	const FInsimulRadialEntry& Entry = BarEntries[SlotIndex];
	if (!Entry.bEnabled)
	{
		return false;
	}
	OnTriggered.Broadcast(SlotIndex, Entry.ActionId);
	return true;
}

void UInsimulQuickBar::Refresh()
{
	const TArray<FInsimulRadialEntry> Visible = VisibleEntries();

	if (EmptyBarText)
	{
		EmptyBarText->SetText(Visible.Num() == 0
			? FText::FromString(TEXT("No shortcuts assigned."))
			: FText::GetEmpty());
	}

	if (!SlotContainer)
	{
		return;
	}
	SlotContainer->ClearChildren();
	for (const FInsimulRadialEntry& Entry : Visible)
	{
		if (SlotWidgetClass)
		{
			if (UUserWidget* Slot = CreateWidget<UUserWidget>(this, SlotWidgetClass))
			{
				SlotContainer->AddChild(Slot);
				continue;
			}
		}
		UTextBlock* Slot = NewObject<UTextBlock>(this);
		Slot->SetText(FText::FromString(Entry.Label.IsEmpty() ? Entry.ActionId : Entry.Label));
		// Greyed, not gone.
		Slot->SetIsEnabled(Entry.bEnabled);
		SlotContainer->AddChild(Slot);
	}
}
