// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulNotificationsWidget — thin UUserWidget over FInsimulNotifications. The
// queue's ordering, expiry and dismissal all live in (and are host-tested by) the
// portable core; this file only marshals FString<->std::string, builds the rows and
// is structurally syntax-gated.

#include "InsimulNotificationsWidget.h"

#include "InsimulSettings.h"
#include "InsimulUITheme.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#include "../Portable/InsimulNotifications.h"

const FName UInsimulNotificationsWidget::PanelKey = FName(TEXT("notifications"));

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

	insimul::ENotificationKind ToPortableKind(EInsimulNotificationKind Kind)
	{
		switch (Kind)
		{
		case EInsimulNotificationKind::Success:
			return insimul::ENotificationKind::Success;
		case EInsimulNotificationKind::Warning:
			return insimul::ENotificationKind::Warning;
		case EInsimulNotificationKind::Danger:
			return insimul::ENotificationKind::Danger;
		default:
			return insimul::ENotificationKind::Info;
		}
	}

	EInsimulNotificationKind FromPortableKind(insimul::ENotificationKind Kind)
	{
		switch (Kind)
		{
		case insimul::ENotificationKind::Success:
			return EInsimulNotificationKind::Success;
		case insimul::ENotificationKind::Warning:
			return EInsimulNotificationKind::Warning;
		case insimul::ENotificationKind::Danger:
			return EInsimulNotificationKind::Danger;
		default:
			return EInsimulNotificationKind::Info;
		}
	}
}

insimul::FInsimulNotifications& UInsimulNotificationsWidget::EnsureQueue()
{
	if (!Queue)
	{
		Queue = MakeUnique<insimul::FInsimulNotifications>();
	}
	return *Queue;
}

void UInsimulNotificationsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureQueue();
	Repaint();
}

void UInsimulNotificationsWidget::BeginDestroy()
{
	Queue.Reset();
	Super::BeginDestroy();
}

void UInsimulNotificationsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Repaint only when the VISIBLE set changed — the queue answers that, so the
	// stack costs nothing on the frames where nothing expired.
	if (EnsureQueue().Tick(static_cast<double>(InDeltaTime)))
	{
		Repaint();
		OnToastsChanged.Broadcast();
	}
}

int32 UInsimulNotificationsWidget::Push(const FString& Text, EInsimulNotificationKind Kind,
	float LifetimeSeconds)
{
	const int32 Id = static_cast<int32>(EnsureQueue().Push(
		ToStd(Text), ToPortableKind(Kind), static_cast<double>(LifetimeSeconds)));
	Repaint();
	OnToastsChanged.Broadcast();
	return Id;
}

bool UInsimulNotificationsWidget::Dismiss(int32 Id)
{
	if (!EnsureQueue().Dismiss(static_cast<int>(Id)))
	{
		return false;
	}
	Repaint();
	OnToastsChanged.Broadcast();
	return true;
}

void UInsimulNotificationsWidget::ClearToasts()
{
	EnsureQueue().Clear();
	Repaint();
	OnToastsChanged.Broadcast();
}

TArray<FInsimulToast> UInsimulNotificationsWidget::VisibleToasts() const
{
	TArray<FInsimulToast> Out;
	if (!Queue)
	{
		return Out;
	}
	for (const insimul::FNotificationItem& Item : Queue->Visible())
	{
		FInsimulToast Toast;
		Toast.Id = static_cast<int32>(Item.Id);
		Toast.Text = ToFString(Item.Text);
		Toast.Kind = FromPortableKind(Item.Kind);
		Toast.Remaining = static_cast<float>(Item.Remaining);
		Toast.ColorToken = ToFString(Item.Color);
		Out.Add(Toast);
	}
	return Out;
}

FLinearColor UInsimulNotificationsWidget::ColorForToken(const FString& Token) const
{
	const UInsimulSettings* Settings = GetDefault<UInsimulSettings>();
	const UInsimulUITheme* Theme =
		Settings ? Cast<UInsimulUITheme>(Settings->UITheme.TryLoad()) : nullptr;
	if (!Theme)
	{
		return FLinearColor::White;
	}
	if (Token == TEXT("success"))
	{
		return Theme->Success;
	}
	if (Token == TEXT("warning"))
	{
		return Theme->Warning;
	}
	if (Token == TEXT("danger"))
	{
		return Theme->Danger;
	}
	return Theme->Accent;
}

void UInsimulNotificationsWidget::Repaint()
{
	if (!ToastContainer)
	{
		// A creator's WBP that binds nothing drives its own rows off
		// OnToastsChanged / VisibleToasts — not an error, just nothing to paint.
		return;
	}

	ToastContainer->ClearChildren();
	for (const FInsimulToast& Toast : VisibleToasts())
	{
		if (ToastEntryClass)
		{
			if (UUserWidget* Row = CreateWidget<UUserWidget>(this, ToastEntryClass))
			{
				ToastContainer->AddChildToVerticalBox(Row);
				continue;
			}
		}

		UTextBlock* Row = NewObject<UTextBlock>(this);
		Row->SetText(FText::FromString(Toast.Text));
		Row->SetColorAndOpacity(FSlateColor(ColorForToken(Toast.ColorToken)));
		ToastContainer->AddChildToVerticalBox(Row);
	}
}
