// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulMinimapPanel — thin UUserWidget. The only arithmetic is world -> map space, and
// it is written to be total: no division by a zero range, and a marker beyond the
// range keeps its direction instead of being clamped into a lie.

#include "InsimulMinimapPanel.h"

#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"

const FName UInsimulMinimapPanel::PanelKey = FName(TEXT("minimap"));

void UInsimulMinimapPanel::SetMarkers(const TArray<FInsimulMapPin>& InMarkers)
{
	MapMarkers = InMarkers;
	Refresh();
}

void UInsimulMinimapPanel::SetPlayerPose(const FVector& Location, float YawDegrees)
{
	PlayerLocation = Location;
	PlayerYaw = YawDegrees;
	Refresh();
}

FVector2D UInsimulMinimapPanel::MapPointFor(const FVector& WorldLocation) const
{
	if (Range <= 0.0f)
	{
		return FVector2D::ZeroVector;
	}

	const FVector Offset = WorldLocation - PlayerLocation;
	float X = static_cast<float>(Offset.X);
	float Y = static_cast<float>(Offset.Y);

	if (bRotateWithPlayer)
	{
		// Turn the world under the player so "up" is where they are looking.
		const float Radians = FMath::DegreesToRadians(-PlayerYaw);
		const float Sin = FMath::Sin(Radians);
		const float Cos = FMath::Cos(Radians);
		const float RotatedX = X * Cos - Y * Sin;
		const float RotatedY = X * Sin + Y * Cos;
		X = RotatedX;
		Y = RotatedY;
	}

	// UE's X is forward and Y is right; a map's X is right and Y is up.
	return FVector2D(Y / Range, X / Range);
}

TArray<FInsimulMapPin> UInsimulMinimapPanel::MarkersInRange() const
{
	TArray<FInsimulMapPin> Out;
	for (const FInsimulMapPin& Marker : MapMarkers)
	{
		if (!Marker.bDiscovered)
		{
			continue;
		}
		if (MapPointFor(Marker.WorldLocation).Size() <= 1.0)
		{
			Out.Add(Marker);
		}
	}
	return Out;
}

void UInsimulMinimapPanel::Refresh()
{
	const TArray<FInsimulMapPin> Visible = MarkersInRange();

	if (PlaceNameText)
	{
		// The nearest discovered marker is what a corner map names.
		const FInsimulMapPin* Nearest = nullptr;
		double Best = TNumericLimits<double>::Max();
		for (const FInsimulMapPin& Marker : Visible)
		{
			const double Distance = FVector::Dist(Marker.WorldLocation, PlayerLocation);
			if (Distance < Best)
			{
				Best = Distance;
				Nearest = &Marker;
			}
		}
		PlaceNameText->SetText(Nearest ? FText::FromString(Nearest->Label) : FText::GetEmpty());
	}

	if (MarkerContainer)
	{
		MarkerContainer->ClearChildren();
		for (const FInsimulMapPin& Marker : Visible)
		{
			if (MarkerWidgetClass)
			{
				if (UUserWidget* Widget = CreateWidget<UUserWidget>(this, MarkerWidgetClass))
				{
					MarkerContainer->AddChild(Widget);
					continue;
				}
			}
			UTextBlock* Text = NewObject<UTextBlock>(this);
			Text->SetText(FText::FromString(Marker.Label.IsEmpty() ? Marker.Id : Marker.Label));
			MarkerContainer->AddChild(Text);
		}
	}

	OnMinimapChanged.Broadcast();
}
