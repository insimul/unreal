// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulRadialMenu — thin UUserWidget. The only arithmetic is angle -> wedge, and
// it is written to be total: no division by an empty wheel, and an out-of-range
// angle wraps rather than clamping.

#include "InsimulRadialMenu.h"

#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

const FName UInsimulRadialMenu::PanelKey = FName(TEXT("radial_menu"));

void UInsimulRadialMenu::SetEntries(const TArray<FInsimulRadialEntry>& InEntries)
{
	WheelEntries = InEntries;
	if (!WheelEntries.IsValidIndex(Selected))
	{
		Selected = -1;
	}
	Refresh();
}

int32 UInsimulRadialMenu::IndexForAngle(float Degrees) const
{
	const int32 Count = WheelEntries.Num();
	if (Count <= 0)
	{
		return -1;
	}

	// Wrap into [0, 360): a stick reading 361 degrees means the same as one
	// reading 1, and clamping would make the last wedge twice as wide.
	float Wrapped = FMath::Fmod(Degrees, 360.0f);
	if (Wrapped < 0.0f)
	{
		Wrapped += 360.0f;
	}

	// Entry zero is centred on the top, so the wedge boundaries sit half a step
	// either side of it.
	const float Step = 360.0f / static_cast<float>(Count);
	const int32 Index = static_cast<int32>(FMath::Fmod(Wrapped + Step * 0.5f, 360.0f) / Step);
	return FMath::Clamp(Index, 0, Count - 1);
}

void UInsimulRadialMenu::HighlightAngle(float Degrees)
{
	const int32 Index = IndexForAngle(Degrees);
	if (Index == Selected)
	{
		return;
	}
	Selected = Index;
	Refresh();
	OnSelectionChanged.Broadcast(Selected);
}

bool UInsimulRadialMenu::Commit()
{
	if (!WheelEntries.IsValidIndex(Selected) || !WheelEntries[Selected].bEnabled)
	{
		return false;
	}
	OnCommitted.Broadcast(WheelEntries[Selected].ActionId);
	return true;
}

void UInsimulRadialMenu::Refresh()
{
	if (SelectionLabelText)
	{
		SelectionLabelText->SetText(WheelEntries.IsValidIndex(Selected)
			? FText::FromString(WheelEntries[Selected].Label)
			: FText::GetEmpty());
	}

	if (!WedgeContainer)
	{
		return;
	}
	WedgeContainer->ClearChildren();
	for (const FInsimulRadialEntry& Entry : WheelEntries)
	{
		if (WedgeClass)
		{
			if (UUserWidget* Wedge = CreateWidget<UUserWidget>(this, WedgeClass))
			{
				WedgeContainer->AddChildToVerticalBox(Wedge);
				continue;
			}
		}
		UTextBlock* Wedge = NewObject<UTextBlock>(this);
		Wedge->SetText(FText::FromString(Entry.Label));
		Wedge->SetIsEnabled(Entry.bEnabled);
		WedgeContainer->AddChildToVerticalBox(Wedge);
	}
}
