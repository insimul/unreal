// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulLoadingScreen — thin UUserWidget over FInsimulLoadingViewModel. The phase
// table, the monotonic progress rule and the deterministic tip all live in (and are
// host-tested by) the portable view-model; this file only marshals
// FString<->std::string, paints the bound widgets and is structurally syntax-gated.

#include "InsimulLoadingScreen.h"

#include "InsimulSettings.h"
#include "InsimulUITheme.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

#include "../Portable/InsimulLoadingViewModel.h"

const FName UInsimulLoadingScreen::PanelKey = FName(TEXT("loading_screen"));

namespace
{
	FString ToFString(const std::string& S)
	{
		return FString(UTF8_TO_TCHAR(S.c_str()));
	}

	std::string ToStd(const FString& S)
	{
		return std::string(TCHAR_TO_UTF8(*S));
	}
}

insimul::FInsimulLoadingViewModel& UInsimulLoadingScreen::EnsureModel()
{
	if (!Model)
	{
		Model = MakeUnique<insimul::FInsimulLoadingViewModel>();
	}
	return *Model;
}

void UInsimulLoadingScreen::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureModel();
	ApplyTheme();
	Repaint();
}

void UInsimulLoadingScreen::BeginDestroy()
{
	Model.Reset();
	Super::BeginDestroy();
}

void UInsimulLoadingScreen::AdvancePhase(FName PhaseKey)
{
	insimul::FInsimulLoadingViewModel& VM = EnsureModel();
	VM.Advance(ToStd(PhaseKey.ToString()));
	Repaint();

	OnLoadingPhase.Broadcast(GetCurrentPhase(), GetPhaseLabel(), GetProgress());

	if (VM.IsComplete() && !bCompleteBroadcast)
	{
		bCompleteBroadcast = true;
		OnLoadingComplete.Broadcast();
	}
}

void UInsimulLoadingScreen::ResetPhases()
{
	EnsureModel().Reset();
	bCompleteBroadcast = false;
	Repaint();
}

float UInsimulLoadingScreen::GetProgress() const
{
	return Model ? static_cast<float>(Model->Progress()) : 0.0f;
}

FString UInsimulLoadingScreen::GetPhaseLabel() const
{
	return Model ? ToFString(Model->Label()) : FString();
}

FString UInsimulLoadingScreen::GetTip() const
{
	return Model ? ToFString(Model->Tip()) : FString();
}

FName UInsimulLoadingScreen::GetCurrentPhase() const
{
	if (!Model)
	{
		return NAME_None;
	}
	const std::string Key = Model->CurrentPhase();
	return Key.empty() ? NAME_None : FName(*ToFString(Key));
}

bool UInsimulLoadingScreen::IsLoadingComplete() const
{
	return Model ? Model->IsComplete() : false;
}

void UInsimulLoadingScreen::Repaint()
{
	const float Progress = GetProgress();

	if (LoadingProgressBar)
	{
		LoadingProgressBar->SetPercent(Progress);
	}
	if (PhaseLabelText)
	{
		PhaseLabelText->SetText(FText::FromString(GetPhaseLabel()));
	}
	if (PercentText)
	{
		PercentText->SetText(FText::FromString(
			FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Progress * 100.0f))));
	}
	if (TipText)
	{
		TipText->SetText(FText::FromString(GetTip()));
	}
}

void UInsimulLoadingScreen::ApplyTheme()
{
	const UInsimulSettings* Settings = GetDefault<UInsimulSettings>();
	if (!Settings)
	{
		return;
	}
	const UInsimulUITheme* Theme = Cast<UInsimulUITheme>(Settings->UITheme.TryLoad());
	if (!Theme)
	{
		// The generated DA_InsimulUITheme is absent (a project that has not run the
		// content generator). The widget still works; it just wears the WBP's own
		// colors instead of the shared tokens.
		return;
	}

	if (LoadingProgressBar)
	{
		LoadingProgressBar->SetFillColorAndOpacity(Theme->Accent);
	}
	if (PhaseLabelText)
	{
		PhaseLabelText->SetColorAndOpacity(FSlateColor(Theme->TextPrimary));
	}
	if (PercentText)
	{
		PercentText->SetColorAndOpacity(FSlateColor(Theme->TextSecondary));
	}
	if (TipText)
	{
		TipText->SetColorAndOpacity(FSlateColor(Theme->TextSecondary));
	}
}
