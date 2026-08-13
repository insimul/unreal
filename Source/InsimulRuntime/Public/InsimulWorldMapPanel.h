// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulWorldMapPanel — the fullscreen map (US-2 of tasklist 190), panel key
// `world_map`, owned by the world's map module.
//
// The same marker set the corner map reads (FInsimulMapPin, InsimulMinimapPanel.h),
// projected into a fixed world RECT instead of a range around the player: a
// fullscreen map is the whole world at once, so its projection is bounds-relative
// and its contents do not move when the player does.
//
// FAST TRAVEL IS A REQUEST, NOT A DECISION. Selecting a destination broadcasts
// OnFastTravelRequested and nothing else happens here. Whether a world lets an actor
// cross it — a curfew, a siege, a wounded leg, a faction that will not have them —
// is the simulation's answer through the KB, and a panel that teleported the player
// itself would be a second, disagreeing rule. The host listens, asks, and moves them
// or says why not.
//
// UNDISCOVERED MARKERS ARE ABSENT, NOT GREYED. A marker the host has not flagged
// discovered is not drawn at all: greying it would still tell the player that
// something is there, which is a spoiler the map has no business giving. (A world
// that wants "rumoured" places authors them as discovered markers of its own kind.)
//
// Thin, syntax-gated UMG boundary.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InsimulMinimapPanel.h"
#include "InsimulWorldMapPanel.generated.h"

class UPanelWidget;
class UTextBlock;

/** Fired when the highlighted marker changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInsimulWorldMapSelection, const FString&, MarkerId);

/** Fired when the player asks to travel. The HOST decides whether they may. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInsimulFastTravelRequested, const FString&, MarkerId);

UCLASS()
class INSIMULRUNTIME_API UInsimulWorldMapPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The stable panel key this widget serves (see the shipped panel catalog). */
	static const FName PanelKey;

	/** Replace the marker set and repaint. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Map")
	void SetMarkers(const TArray<FInsimulMapPin>& InMarkers);

	UFUNCTION(BlueprintPure, Category = "Insimul|Map")
	const TArray<FInsimulMapPin>& Markers() const { return MapMarkers; }

	/** The world rect the map covers, in world units (min and max corners). */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Map")
	void SetWorldBounds(const FVector2D& InMin, const FVector2D& InMax);

	/** Where the player is, for the "you are here" marker. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Map")
	void SetPlayerLocation(const FVector& Location);

	/**
	 * A world location in map space: the unit square [0, 1], X right and Y up, over
	 * the world bounds. A degenerate bounds rect projects to the centre rather than
	 * dividing by zero.
	 */
	UFUNCTION(BlueprintPure, Category = "Insimul|Map")
	FVector2D MapPointFor(const FVector& WorldLocation) const;

	/** The markers this map draws — discovered only, in the order they were given. */
	UFUNCTION(BlueprintPure, Category = "Insimul|Map")
	TArray<FInsimulMapPin> VisibleMarkers() const;

	/** The subset the host offers as fast-travel destinations. */
	UFUNCTION(BlueprintPure, Category = "Insimul|Map")
	TArray<FInsimulMapPin> FastTravelMarkers() const;

	/** Highlight a marker. An unknown id selects nothing and returns false. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Map")
	bool Select(const FString& MarkerId);

	UFUNCTION(BlueprintPure, Category = "Insimul|Map")
	FString SelectedMarker() const { return Selected; }

	/**
	 * Ask to travel to the selected marker. Returns false when nothing is selected
	 * or the host does not offer that marker for travel; true means the REQUEST was
	 * broadcast, never that the journey happened.
	 */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Map")
	bool RequestFastTravel();

	UFUNCTION(BlueprintCallable, Category = "Insimul|Map")
	void Refresh();

	UPROPERTY(BlueprintAssignable, Category = "Insimul|Map")
	FOnInsimulWorldMapSelection OnSelectionChanged;

	UPROPERTY(BlueprintAssignable, Category = "Insimul|Map")
	FOnInsimulFastTravelRequested OnFastTravelRequested;

	/** The marker widget class a creator supplies; unset builds a plain text row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Map")
	TSubclassOf<UUserWidget> MarkerWidgetClass;

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> MarkerContainer;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SelectionLabelText;

private:
	UPROPERTY(Transient)
	TArray<FInsimulMapPin> MapMarkers;

	UPROPERTY(EditAnywhere, Category = "Insimul|Map")
	FVector2D WorldMin = FVector2D(-100000.0f, -100000.0f);

	UPROPERTY(EditAnywhere, Category = "Insimul|Map")
	FVector2D WorldMax = FVector2D(100000.0f, 100000.0f);

	UPROPERTY(Transient)
	FVector PlayerLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FString Selected;
};
