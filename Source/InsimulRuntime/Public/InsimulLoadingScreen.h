// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulLoadingScreen — the UMG half of the pattern-proof pair (US-1 of tasklist
// 190): the loading screen DRIVEN BY THE STARTUP PHASES, over the portable
// view-model (Portable/InsimulLoadingViewModel.h).
//
// WHY THIS IS THE PATTERN PROOF. It is the smallest panel in the suite that still
// exercises every seam the other thirteen use: a stable panel key resolved through
// UInsimulUIPanelSurface, a shipped WBP the export generator creates
// (WBP_LoadingScreen), a UE-free view-model host-tested against the SAME corpus the
// Babylon/Unity/Godot legs run (conformance/ui/loading-phases.json), and design
// tokens read from the shared theme asset rather than hard-coded here. A panel that
// cannot be built this way is a panel whose logic is in the wrong place.
//
// PHASES, NOT PERCENTAGES. The screen never sets a number; the boot loop calls
// AdvancePhase() with a phase key (`Portable/InsimulBootstrap.h` — world source ->
// save slot -> KB -> systems init) and the view-model turns the ordered weighted
// phase table into a MONOTONIC fraction, a label and a deterministic per-phase tip.
// Re-entering a phase, or arriving at one out of order, never moves the bar
// backwards — that rule is the view-model's and is proven headless, not re-decided
// here.
//
// Every bound widget is OPTIONAL: a creator's own WBP may keep the progress bar and
// drop the tip, or bind none of them and drive its own visuals off the delegates.
// This class is the thin, syntax-gated UMG boundary; ctest `ui_registry` proves the
// semantics.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InsimulLoadingScreen.generated.h"

class UInsimulUITheme;
class UProgressBar;
class UTextBlock;

// The engine-agnostic view-model this seam wraps (pimpl — never in the reflected
// layout). Host-tested by ctest `ui_registry`.
namespace insimul { class FInsimulLoadingViewModel; }

/** Fired on every phase change: the new key, its label and the progress fraction. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnInsimulLoadingPhase, FName, PhaseKey, const FString&, Label, float, Progress);

/** Fired once the terminal phase is reached (the game may swap the screen out). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInsimulLoadingComplete);

/**
 * The default loading screen. Panel key `loading_screen`.
 */
UCLASS()
class INSIMULRUNTIME_API UInsimulLoadingScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The stable panel key this widget serves (see the shipped panel catalog). */
	static const FName PanelKey;

	/**
	 * Advance to a named boot phase. Progress is clamped monotonic and an unknown
	 * key is a no-op — the view-model's rules, not this widget's.
	 */
	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	void AdvancePhase(FName PhaseKey);

	/** Back to the pre-boot state (progress 0, no phase) — a second boot reuses it. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	void ResetPhases();

	/** Cumulative progress in [0, 1]. */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	float GetProgress() const;

	/** The current phase's human label ("" before the first advance). */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	FString GetPhaseLabel() const;

	/** The deterministic tip for the current phase. */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	FString GetTip() const;

	/** The current phase key (NAME_None before the first advance). */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	FName GetCurrentPhase() const;

	/** True once the terminal phase has been reached. */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	bool IsLoadingComplete() const;

	UPROPERTY(BlueprintAssignable, Category = "Insimul|UI")
	FOnInsimulLoadingPhase OnLoadingPhase;

	UPROPERTY(BlueprintAssignable, Category = "Insimul|UI")
	FOnInsimulLoadingComplete OnLoadingComplete;

	virtual void NativeConstruct() override;
	virtual void BeginDestroy() override;

protected:
	/** Filled from the view-model's fraction. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> LoadingProgressBar;

	/** The current phase's label. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PhaseLabelText;

	/** "42%" — the fraction as text, for creators who want a number too. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PercentText;

	/** The per-phase tip. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TipText;

private:
	/** Push the view-model's current state into the bound widgets. */
	void Repaint();

	/** Apply the shared design tokens (colors / font sizes) to the bound widgets. */
	void ApplyTheme();

	insimul::FInsimulLoadingViewModel& EnsureModel();

	/** The host-tested portable view-model. */
	TUniquePtr<insimul::FInsimulLoadingViewModel> Model;

	/** True once OnLoadingComplete has fired, so it fires exactly once per boot. */
	bool bCompleteBroadcast = false;
};
