// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulMinimapPanel — the HUD corner map (US-2 of tasklist 190), panel key `minimap`,
// owned by the world's map module. UInsimulWorldMapPanel next door is the fullscreen
// half; both read the SAME marker set, because two maps that disagreed about where
// a village is would be two answers to one question.
//
// WHAT A MARKER IS, AND WHERE IT COMES FROM. A marker is a place the HOST already
// knows about — a settlement, a lot, a quest objective, the player. This widget
// holds no world model and does no discovery: the host hands it markers and it
// projects them into map space. That is the whole seam, and it is deliberate — the
// world's geography lives in the world snapshot and the simulation, and a UI that
// re-derived it would be a second, drifting copy.
//
// NO DISCOVERY STATE IS INVENTED. Whether a marker has been FOUND is a
// per-playthrough fact, and the save envelope this port reads declares no field for
// it (see conformance/saves — currentState has player / quests / npcs / containers /
// reputation and no map slice). So `bDiscovered` is a flag the host sets from
// whatever it tracks, and this panel never writes one back: inventing a
// `currentState.map` schema in an engine port would be this leg disagreeing with the
// other three about what a save contains, which is exactly what the state-location
// invariant exists to stop (Portable/InsimulUIStateBinding.h says the same about
// equipment).
//
// THE PROJECTION IS TOTAL. A zero or negative range projects everything to the
// centre rather than dividing by zero, and a marker outside the range is REPORTED as
// out of range rather than drawn at the rim pretending to be near.
//
// Thin, syntax-gated UMG boundary.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InsimulMinimapPanel.generated.h"

class UTextBlock;
class UPanelWidget;

/** One place on the map. `Kind` is the host's own vocabulary (settlement, quest…). */
USTRUCT(BlueprintType)
struct FInsimulMapPin
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Map")
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Map")
	FString Label;

	/** The host's own marker vocabulary — never an enum this plugin compiles in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Map")
	FString Kind;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Map")
	FVector WorldLocation = FVector::ZeroVector;

	/** Whether the player has found it. The HOST tracks this; the panel never
	 *  writes it back (see the header note on the save schema). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Map")
	bool bDiscovered = true;

	/** Whether the host offers it as a fast-travel destination. Permission is the
	 *  simulation's answer, not this panel's. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Map")
	bool bFastTravel = false;
};

/** Fired after the map repaints (new markers, a moved player). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInsimulMinimapChanged);

UCLASS()
class INSIMULRUNTIME_API UInsimulMinimapPanel : public UUserWidget
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

	/** Where the map is centred, and which way the player faces. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Map")
	void SetPlayerPose(const FVector& Location, float YawDegrees);

	/**
	 * A world location in map space: the unit square [-1, 1] with the player at the
	 * origin, X right and Y up. Beyond `Range` the point keeps its direction and its
	 * magnitude passes 1, so a caller can clamp it to the rim knowingly rather than
	 * be lied to. A range of zero projects to the centre rather than dividing.
	 */
	UFUNCTION(BlueprintPure, Category = "Insimul|Map")
	FVector2D MapPointFor(const FVector& WorldLocation) const;

	/** The markers within `Range` of the player, discovered ones only. */
	UFUNCTION(BlueprintPure, Category = "Insimul|Map")
	TArray<FInsimulMapPin> MarkersInRange() const;

	UFUNCTION(BlueprintCallable, Category = "Insimul|Map")
	void Refresh();

	UPROPERTY(BlueprintAssignable, Category = "Insimul|Map")
	FOnInsimulMinimapChanged OnMinimapChanged;

	/** How far the corner map sees, in world units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Map")
	float Range = 4000.0f;

	/** Whether the map turns under the player or holds north up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Map")
	bool bRotateWithPlayer = true;

	/** The marker widget class a creator supplies; unset builds a plain text row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Map")
	TSubclassOf<UUserWidget> MarkerWidgetClass;

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> MarkerContainer;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlaceNameText;

private:
	UPROPERTY(Transient)
	TArray<FInsimulMapPin> MapMarkers;

	UPROPERTY(Transient)
	FVector PlayerLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	float PlayerYaw = 0.0f;
};
