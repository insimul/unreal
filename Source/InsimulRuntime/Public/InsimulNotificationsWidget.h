// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulNotificationsWidget — the other half of the pattern-proof pair (US-1 of
// tasklist 190): the toast stack every system pushes to, over the portable queue
// (Portable/InsimulNotifications.h).
//
// PUSH, NEVER POLL. A system that has something to say calls Push() once; the queue
// ages its entries in Tick() and reports whether the VISIBLE set changed, so this
// widget repaints on change rather than every frame. The quest journal, the trade
// panel and the save shell all reach the player through this one surface — a second
// toast implementation inside a panel would be a second lifetime rule to keep in
// step, which is why the panels are given this widget rather than a text block.
//
// KIND -> TOKEN, NOT KIND -> COLOR. A notification carries a kind (info / success /
// warning / danger) and the queue maps it to the NAME of a shared theme token; this
// widget looks that name up in the theme asset. A re-skin therefore moves every
// toast with the rest of the UI, and a creator who swaps the theme does not have to
// find hard-coded colors in a widget.
//
// Bound widgets are optional and the entry row class is a creator's to replace: with
// no ToastEntryClass set the widget builds a plain text row per notification, which
// is enough for the default game and is where a designer starts.
//
// This class is the thin, syntax-gated UMG boundary; the queue's semantics (ordering,
// expiry, early dismissal) are host-tested UE-free.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InsimulNotificationsWidget.generated.h"

class UInsimulUITheme;
class UVerticalBox;

// The engine-agnostic queue this seam wraps (pimpl — never in the reflected layout).
namespace insimul { class FInsimulNotifications; }

/** Toast severity (mirrors insimul::ENotificationKind). */
UENUM(BlueprintType)
enum class EInsimulNotificationKind : uint8
{
	Info,
	Success,
	Warning,
	Danger,
};

/** One visible toast, as UMG sees it. */
USTRUCT(BlueprintType)
struct FInsimulToast
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|UI")
	int32 Id = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|UI")
	FString Text;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|UI")
	EInsimulNotificationKind Kind = EInsimulNotificationKind::Info;

	/** Seconds left before it ages out. */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|UI")
	float Remaining = 0.0f;

	/** The shared theme token this kind resolves to (e.g. the accent token). */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|UI")
	FString ColorToken;
};

/** Fired when the visible set changes — a designer's WBP can rebuild its own rows. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInsimulToastsChanged);

/**
 * The default toast stack. Panel key `notifications`.
 */
UCLASS()
class INSIMULRUNTIME_API UInsimulNotificationsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The stable panel key this widget serves (see the shipped panel catalog). */
	static const FName PanelKey;

	/** Enqueue a toast. Returns its id, so a caller can dismiss it early. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	int32 Push(const FString& Text, EInsimulNotificationKind Kind = EInsimulNotificationKind::Info,
		float LifetimeSeconds = 4.0f);

	/** Dismiss a toast before it ages out. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	bool Dismiss(int32 Id);

	/** Drop every toast (a scene change, a load). */
	UFUNCTION(BlueprintCallable, Category = "Insimul|UI")
	void ClearToasts();

	/** The visible toasts, oldest first. */
	UFUNCTION(BlueprintPure, Category = "Insimul|UI")
	TArray<FInsimulToast> VisibleToasts() const;

	UPROPERTY(BlueprintAssignable, Category = "Insimul|UI")
	FOnInsimulToastsChanged OnToastsChanged;

	/** The row widget class a creator supplies; unset builds a plain text row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|UI")
	TSubclassOf<UUserWidget> ToastEntryClass;

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void BeginDestroy() override;

protected:
	/** The stack the rows are added to. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> ToastContainer;

private:
	/** Rebuild the rows from the queue's visible set. */
	void Repaint();

	/** The theme token color for a kind, or white when no theme asset resolves. */
	FLinearColor ColorForToken(const FString& Token) const;

	insimul::FInsimulNotifications& EnsureQueue();

	/** The host-tested portable queue. */
	TUniquePtr<insimul::FInsimulNotifications> Queue;
};
