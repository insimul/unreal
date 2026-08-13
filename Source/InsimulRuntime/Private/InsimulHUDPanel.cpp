// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulHUDPanel — thin UUserWidget. It decides nothing: every slot is a catalog key
// the panel surface answers for, and the answers (available / withheld / unknown)
// are the portable resolver's, host-tested by ctest `ui_registry`.

#include "InsimulHUDPanel.h"

#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "InsimulUIPanelSurface.h"

const FName UInsimulHUDPanel::PanelKey = FName(TEXT("hud"));

UInsimulHUDPanel::UInsimulHUDPanel()
{
	// The default HUD's slots, in draw order. These are catalog KEYS — which of them
	// a world actually has is decided by the module that owns each one, in
	// Content/Data/insimul/ui/panels.json, not here.
	SlotKeys = {
		FName(TEXT("minimap")),
		FName(TEXT("quest_tracker")),
		FName(TEXT("quickbar")),
		FName(TEXT("radial_menu")),
		FName(TEXT("notifications")),
	};
}

void UInsimulHUDPanel::Bind(UInsimulUIPanelSurface* InSurface)
{
	Surface = InSurface;
	if (!Surface)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Insimul HUD: no panel surface bound; no sub-panel can be resolved. This is a "
				 "wiring bug, not a world without modules."));
	}
	Refresh();
}

void UInsimulHUDPanel::SetSubPanelKeys(const TArray<FName>& InKeys)
{
	SlotKeys = InKeys;
	Refresh();
}

void UInsimulHUDPanel::Refresh()
{
	Resolved.Reset();
	Withheld.Reset();

	if (SlotContainer)
	{
		SlotContainer->ClearChildren();
	}

	for (const FName& Key : SlotKeys)
	{
		if (!Surface)
		{
			Withheld.Add(Key);
			continue;
		}
		// ResolvePanelClass already logs the reason for a refusal, so a missing slot
		// is never a silent no-op.
		UUserWidget* Widget = Surface->CreatePanelWidget(Key);
		if (!Widget)
		{
			Withheld.Add(Key);
			continue;
		}
		Resolved.Add(Key, Widget);
		if (SlotContainer)
		{
			SlotContainer->AddChild(Widget);
		}
	}

	if (DiagnosticText)
	{
		DiagnosticText->SetText(Withheld.Num() > 0 ? FText::FromString(Describe()) : FText::GetEmpty());
	}
	OnHUDChanged.Broadcast();
}

TArray<FName> UInsimulHUDPanel::VisibleSubPanels() const
{
	TArray<FName> Out;
	for (const FName& Key : SlotKeys)
	{
		if (Resolved.Contains(Key))
		{
			Out.Add(Key);
		}
	}
	return Out;
}

TArray<FName> UInsimulHUDPanel::WithheldSubPanels() const
{
	return Withheld;
}

UUserWidget* UInsimulHUDPanel::SubPanel(FName InPanelKey) const
{
	const TObjectPtr<UUserWidget>* Found = Resolved.Find(InPanelKey);
	return Found ? Found->Get() : nullptr;
}

FString UInsimulHUDPanel::Describe() const
{
	FString Missing;
	for (const FName& Key : Withheld)
	{
		if (!Missing.IsEmpty())
		{
			Missing += TEXT(", ");
		}
		Missing += Key.ToString();
	}
	if (Missing.IsEmpty())
	{
		return FString::Printf(TEXT("HUD: %d slot(s), none withheld"), SlotKeys.Num());
	}
	return FString::Printf(TEXT("HUD: %d of %d slot(s) shown; withheld: %s (%s)"),
		SlotKeys.Num() - Withheld.Num(), SlotKeys.Num(), *Missing,
		Surface ? *Surface->DescribeSurface() : TEXT("no panel surface bound"));
}
