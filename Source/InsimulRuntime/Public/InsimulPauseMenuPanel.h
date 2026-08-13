// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulPauseMenuPanel — the unified ESC menu (US-3 of tasklist 190), panel key
// `pause_menu`. The shell the player opens with Escape: a tab bar and one hosted
// panel at a time.
//
// TWO GATES, BOTH DATA, NEITHER OF THEM THIS WIDGET'S.
//   1. The TAB gate is the module bundle the world's genre enabled, applied by the
//      portable FInsimulPauseMenuModel (UInsimulPauseMenu) against the shared
//      DEFAULT_TABS — the same matrix (conformance/ui/pause-menu-cases.json) the
//      Babylon reference and the Unity/Godot ports run, so the four legs cannot
//      disagree about which tabs a bundle unlocks. ctest `ui_pause_menu`.
//   2. The PANEL gate is the mechanic module that OWNS the panel a tab hosts
//      (Content/Data/insimul/ui/panels.json), answered by UInsimulUIPanelSurface.
//      ctest `ui_registry`.
// They are different vocabularies on purpose: the first is the IR's feature-module
// registry (proficiency, assessment…), the second is core's mechanic modules
// (equipment, skill, map). A tab shows when BOTH say yes, and a tab whose panel is
// withheld is dropped rather than left to open an empty box.
//
// WITHHOLDING IS REPORTED, never silent: WithheldTabs() names every tab dropped
// because the panel it hosts is withheld, and Describe() says so in one line
// together with the surface's own account of the module set — so a creator looking
// at a menu with no Skills tab can find out why without a debugger.
//
// THE SHELL DECIDES NOTHING ELSE. Pausing the game, capturing input and animating
// the transition are the owning UUserWidget's / player controller's business. This
// class holds the tab list, the active tab and the hosted widgets.
//
// Thin, syntax-gated UMG boundary over cores host-tested UE-free.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InsimulPauseMenuPanel.generated.h"

class UInsimulPauseMenu;
class UInsimulUIPanelSurface;
class UPanelWidget;
class UTextBlock;

/** One tab as the shell draws it: the menu key, its label, and the catalog panel
 *  key it hosts (None for a tab the shell itself paints, e.g. Settings). */
USTRUCT(BlueprintType)
struct FInsimulMenuTab
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Menu")
	FName Key;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Menu")
	FString Label;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Menu")
	FName PanelKey;
};

/** Fired after the tab set, the active tab or the open state changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInsimulMenuChanged);

UCLASS()
class INSIMULRUNTIME_API UInsimulPauseMenuPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	UInsimulPauseMenuPanel();

	/** The stable panel key this widget serves (see the shipped panel catalog). */
	static const FName PanelKey;

	/**
	 * Wire the shell: the feature modules this world's genre bundle enabled (the
	 * tab gate) and the panel surface (the panel gate). A null surface leaves every
	 * panel-backed tab withheld and SAYS so — it is a wiring bug, not a world
	 * without modules, and a shell that hid the difference would make it invisible.
	 */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Menu")
	void Configure(const TArray<FString>& EnabledModules, UInsimulUIPanelSurface* InSurface);

	/** Rebuild from both gates' current answers (call after a module set lands). */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Menu")
	void Refresh();

	/** The tabs this world shows, in declaration order. */
	UFUNCTION(BlueprintPure, Category = "Insimul|Menu")
	const TArray<FInsimulMenuTab>& Tabs() const { return VisibleTabs; }

	/** Tab keys the bundle unlocked but whose hosted panel this world withholds,
	 *  in declaration order. (A tab the bundle never unlocked was never a tab.) */
	UFUNCTION(BlueprintPure, Category = "Insimul|Menu")
	const TArray<FName>& WithheldTabs() const { return Withheld; }

	/** The panel widget hosted by a tab, or nullptr (shell-painted / withheld). */
	UFUNCTION(BlueprintPure, Category = "Insimul|Menu")
	UUserWidget* TabContent(FName TabKey) const;

	// ── The open / active-tab reducer (the portable model's, not this class's) ──

	UFUNCTION(BlueprintCallable, Category = "Insimul|Menu")
	void Open(FName TabKey);

	UFUNCTION(BlueprintCallable, Category = "Insimul|Menu")
	void Close();

	/** What the Escape key calls. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Menu")
	void Toggle();

	UFUNCTION(BlueprintPure, Category = "Insimul|Menu")
	bool IsOpen() const;

	/** Switch tabs. Rejected (false) for a tab this world does not show. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Menu")
	bool SetActiveTab(FName TabKey);

	UFUNCTION(BlueprintPure, Category = "Insimul|Menu")
	FName ActiveTab() const;

	/** One line for a boot log or a bug report — names what was withheld and why. */
	UFUNCTION(BlueprintPure, Category = "Insimul|Menu")
	FString Describe() const;

	UPROPERTY(BlueprintAssignable, Category = "Insimul|Menu")
	FOnInsimulMenuChanged OnMenuChanged;

	/** Which catalog panel each menu tab hosts. DATA, so a creator can point the
	 *  Map tab at their own panel key without touching engine code. Tabs absent
	 *  from this map are painted by the shell itself. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Menu")
	TMap<FName, FName> TabPanelKeys;

protected:
	virtual void NativeDestruct() override;

	/** Where the tab buttons go (the shell builds them from Tabs()). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> TabBar;

	/** Where the active tab's panel is mounted. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> TabContentBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DiagnosticText;

private:
	/** The host-tested tab-gating + reducer core (ctest `ui_pause_menu`). */
	UPROPERTY(Transient)
	TObjectPtr<UInsimulPauseMenu> Menu;

	UPROPERTY(Transient)
	TObjectPtr<UInsimulUIPanelSurface> Surface;

	UPROPERTY(Transient)
	TArray<FInsimulMenuTab> VisibleTabs;

	UPROPERTY(Transient)
	TArray<FName> Withheld;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UUserWidget>> Hosted;

	void MountActive();
};
