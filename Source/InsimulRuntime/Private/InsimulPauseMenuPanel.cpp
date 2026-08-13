// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulPauseMenuPanel — thin UUserWidget. It decides nothing: which tabs a
// bundle unlocks is UInsimulPauseMenu's portable core (ctest `ui_pause_menu`) and
// which panels this world may show is UInsimulUIPanelSurface's portable resolver
// (ctest `ui_registry`). This file joins the two answers and paints them.

#include "InsimulPauseMenuPanel.h"

#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "InsimulPauseMenu.h"
#include "InsimulUIPanelSurface.h"

const FName UInsimulPauseMenuPanel::PanelKey = FName(TEXT("pause_menu"));

UInsimulPauseMenuPanel::UInsimulPauseMenuPanel()
{
	// The default shell's tab -> panel wiring. Every value is a CATALOG key, so
	// whether a tab survives is decided by the module that owns its panel, in
	// Content/Data/insimul/ui/panels.json, not here. Tabs with no entry (resume,
	// settings, and the IR-gated learning tabs) are painted by the shell.
	TabPanelKeys.Add(FName(TEXT("journal")), FName(TEXT("quest_journal")));
	TabPanelKeys.Add(FName(TEXT("inventory")), FName(TEXT("inventory")));
	TabPanelKeys.Add(FName(TEXT("map")), FName(TEXT("world_map")));
	TabPanelKeys.Add(FName(TEXT("skills")), FName(TEXT("skill_tree")));
	TabPanelKeys.Add(FName(TEXT("save")), FName(TEXT("save_load")));
}

void UInsimulPauseMenuPanel::NativeDestruct()
{
	Hosted.Reset();
	Super::NativeDestruct();
}

void UInsimulPauseMenuPanel::Configure(const TArray<FString>& EnabledModules,
	UInsimulUIPanelSurface* InSurface)
{
	if (!Menu)
	{
		Menu = NewObject<UInsimulPauseMenu>(this);
	}
	Menu->Configure(EnabledModules);

	Surface = InSurface;
	if (!Surface)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Insimul menu: no panel surface bound; every panel-backed tab is withheld. This "
				 "is a wiring bug, not a world without modules."));
	}
	Refresh();
}

void UInsimulPauseMenuPanel::Refresh()
{
	VisibleTabs.Reset();
	Withheld.Reset();
	Hosted.Reset();

	if (TabBar)
	{
		TabBar->ClearChildren();
	}
	if (TabContentBox)
	{
		TabContentBox->ClearChildren();
	}

	if (!Menu)
	{
		Menu = NewObject<UInsimulPauseMenu>(this);
	}

	// Gate 1: the module bundle, through the portable model.
	const TArray<FInsimulPauseTab> Unlocked = Menu->VisibleTabs();
	for (const FInsimulPauseTab& Tab : Unlocked)
	{
		const FName Key(*Tab.Key);
		const FName* Panel = TabPanelKeys.Find(Key);

		FInsimulMenuTab Row;
		Row.Key = Key;
		Row.Label = Tab.Label;
		Row.PanelKey = Panel ? *Panel : NAME_None;

		// Gate 2: the module that owns the panel the tab hosts.
		if (Panel && !Panel->IsNone())
		{
			// CreatePanelWidget already logs the reason for a refusal, so a dropped
			// tab is never a silent no-op.
			UUserWidget* Content = Surface ? Surface->CreatePanelWidget(*Panel) : nullptr;
			if (!Content)
			{
				Withheld.Add(Key);
				continue;
			}
			Hosted.Add(Key, Content);
		}
		VisibleTabs.Add(Row);
	}

	// An active tab that no longer exists falls back to the first visible one.
	if (VisibleTabs.Num() > 0 && !Menu->IsTabVisible(ActiveTab().ToString()))
	{
		Menu->SetActive(VisibleTabs[0].Key.ToString());
	}
	MountActive();

	if (DiagnosticText)
	{
		DiagnosticText->SetText(
			Withheld.Num() > 0 ? FText::FromString(Describe()) : FText::GetEmpty());
	}
	OnMenuChanged.Broadcast();
}

void UInsimulPauseMenuPanel::MountActive()
{
	if (!TabContentBox)
	{
		return;
	}
	TabContentBox->ClearChildren();
	if (UUserWidget* Content = TabContent(ActiveTab()))
	{
		TabContentBox->AddChild(Content);
	}
}

UUserWidget* UInsimulPauseMenuPanel::TabContent(FName TabKey) const
{
	const TObjectPtr<UUserWidget>* Found = Hosted.Find(TabKey);
	return Found ? Found->Get() : nullptr;
}

void UInsimulPauseMenuPanel::Open(FName TabKey)
{
	if (!Menu)
	{
		return;
	}
	Menu->Open(TabKey.IsNone() ? FString() : TabKey.ToString());
	MountActive();
	OnMenuChanged.Broadcast();
}

void UInsimulPauseMenuPanel::Close()
{
	if (Menu)
	{
		Menu->Close();
		OnMenuChanged.Broadcast();
	}
}

void UInsimulPauseMenuPanel::Toggle()
{
	if (!Menu)
	{
		return;
	}
	Menu->Toggle();
	MountActive();
	OnMenuChanged.Broadcast();
}

bool UInsimulPauseMenuPanel::IsOpen() const
{
	return Menu ? Menu->IsOpen() : false;
}

bool UInsimulPauseMenuPanel::SetActiveTab(FName TabKey)
{
	if (!Menu)
	{
		return false;
	}
	// A tab the bundle unlocked but whose panel this world withholds is NOT a tab:
	// the shell refuses it here rather than opening an empty box.
	if (Withheld.Contains(TabKey))
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("Insimul menu: tab '%s' is not shown — the panel it hosts is withheld."),
			*TabKey.ToString());
		return false;
	}
	if (!Menu->SetActive(TabKey.ToString()))
	{
		return false;
	}
	MountActive();
	OnMenuChanged.Broadcast();
	return true;
}

FName UInsimulPauseMenuPanel::ActiveTab() const
{
	return Menu ? FName(*Menu->ActiveTab()) : NAME_None;
}

FString UInsimulPauseMenuPanel::Describe() const
{
	if (Withheld.Num() == 0)
	{
		return FString::Printf(TEXT("ESC menu: %d tab(s), none withheld"), VisibleTabs.Num());
	}
	FString Missing;
	for (const FName& Key : Withheld)
	{
		if (!Missing.IsEmpty())
		{
			Missing += TEXT(", ");
		}
		Missing += Key.ToString();
	}
	// Only the PANEL gate is reported here: a tab the module bundle never unlocked
	// was never a tab of this world's menu, and the bundle itself is reported by
	// the activator that applied it.
	return FString::Printf(
		TEXT("ESC menu: %d tab(s) shown; %d dropped because the panel they host is withheld: %s "
			 "(%s)"),
		VisibleTabs.Num(), Withheld.Num(), *Missing,
		Surface ? *Surface->DescribeSurface() : TEXT("no panel surface bound"));
}
