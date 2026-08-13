// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulMainMenuPanel — thin UUserWidget. The one thing it must not get wrong —
// whether the player may continue — is the portable slot model's answer
// (ctest `ui_save_slots`), not a boolean this file keeps.

#include "InsimulMainMenuPanel.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

const FName UInsimulMainMenuPanel::PanelKey = FName(TEXT("main_menu"));

UInsimulSaveSlotPanel& UInsimulMainMenuPanel::EnsureSlots()
{
	if (!Slots)
	{
		Slots = NewObject<UInsimulSaveSlotPanel>(this);
	}
	return *Slots;
}

void UInsimulMainMenuPanel::NativeConstruct()
{
	Super::NativeConstruct();

	if (NewGameButton)
	{
		NewGameButton->OnClicked.AddDynamic(this, &UInsimulMainMenuPanel::HandleNewGameClicked);
	}
	if (ContinueButton)
	{
		ContinueButton->OnClicked.AddDynamic(this, &UInsimulMainMenuPanel::HandleContinueClicked);
	}
	if (LoadGameButton)
	{
		LoadGameButton->OnClicked.AddDynamic(this, &UInsimulMainMenuPanel::HandleLoadGameClicked);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked.AddDynamic(this, &UInsimulMainMenuPanel::HandleSettingsClicked);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked.AddDynamic(this, &UInsimulMainMenuPanel::HandleQuitClicked);
	}
	Repaint();
}

void UInsimulMainMenuPanel::SetTitle(const FString& InTitle)
{
	GameTitle = InTitle;
	Repaint();
}

void UInsimulMainMenuPanel::SetSlots(const TArray<FInsimulSaveSlotResult>& Results)
{
	EnsureSlots().SetSlots(Results);
	Repaint();
}

bool UInsimulMainMenuPanel::CanContinue() const
{
	// HasAnyLoadable() and ContinueSlot() are one answer, not two: asking the slot
	// question is what keeps the button and the slot it would resume in agreement.
	return ContinueSlot() != INDEX_NONE;
}

int32 UInsimulMainMenuPanel::ContinueSlot() const
{
	// Which slot, and whether there is one at all, are the portable model's answers
	// (ctest `ui_save_slots`). This widget marshals them and nothing more.
	return Slots ? Slots->ContinueSlot() : INDEX_NONE;
}

FString UInsimulMainMenuPanel::ContinueBlockedReason() const
{
	return Slots ? Slots->ContinueBlockedReason() : FString(TEXT("No saved game to continue."));
}

void UInsimulMainMenuPanel::RequestNewGame()
{
	OnNewGameRequested.Broadcast();
}

bool UInsimulMainMenuPanel::RequestContinue()
{
	const int32 Slot = ContinueSlot();
	if (Slot == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("Insimul main menu: Continue is unavailable (%s)"),
			*ContinueBlockedReason());
		return false;
	}
	OnContinueRequested.Broadcast(Slot);
	return true;
}

void UInsimulMainMenuPanel::RequestLoadGame()
{
	OnLoadGameRequested.Broadcast();
}

void UInsimulMainMenuPanel::RequestSettings()
{
	OnSettingsRequested.Broadcast();
}

void UInsimulMainMenuPanel::RequestQuit()
{
	OnQuitRequested.Broadcast();
}

void UInsimulMainMenuPanel::HandleNewGameClicked()
{
	RequestNewGame();
}

void UInsimulMainMenuPanel::HandleContinueClicked()
{
	RequestContinue();
}

void UInsimulMainMenuPanel::HandleLoadGameClicked()
{
	RequestLoadGame();
}

void UInsimulMainMenuPanel::HandleSettingsClicked()
{
	RequestSettings();
}

void UInsimulMainMenuPanel::HandleQuitClicked()
{
	RequestQuit();
}

void UInsimulMainMenuPanel::Repaint()
{
	if (TitleText)
	{
		TitleText->SetText(FText::FromString(GameTitle));
	}
	if (ContinueButton)
	{
		ContinueButton->SetIsEnabled(CanContinue());
	}
	if (ContinueHintText)
	{
		// Disabled AND unexplained is the state this panel refuses to be in.
		ContinueHintText->SetText(FText::FromString(ContinueBlockedReason()));
	}
}

FString UInsimulMainMenuPanel::Describe() const
{
	return FString::Printf(TEXT("main menu '%s': %d slot(s), continue %s%s"), *GameTitle,
		Slots ? Slots->Slots().Num() : 0, CanContinue() ? TEXT("available") : TEXT("unavailable"),
		CanContinue() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *ContinueBlockedReason()));
}
