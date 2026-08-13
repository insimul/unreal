// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulWorldMapPanel — thin UUserWidget. The projection is bounds-relative and total
// (a degenerate rect centres rather than divides), and fast travel is a broadcast
// request: nothing here moves an actor.

#include "InsimulWorldMapPanel.h"

#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"

const FName UInsimulWorldMapPanel::PanelKey = FName(TEXT("world_map"));

void UInsimulWorldMapPanel::SetMarkers(const TArray<FInsimulMapPin>& InMarkers)
{
	MapMarkers = InMarkers;
	if (!Selected.IsEmpty())
	{
		const bool bStillThere = MapMarkers.ContainsByPredicate(
			[this](const FInsimulMapPin& Marker) { return Marker.Id == Selected; });
		if (!bStillThere)
		{
			Selected.Reset();
		}
	}
	Refresh();
}

void UInsimulWorldMapPanel::SetWorldBounds(const FVector2D& InMin, const FVector2D& InMax)
{
	WorldMin = InMin;
	WorldMax = InMax;
	Refresh();
}

void UInsimulWorldMapPanel::SetPlayerLocation(const FVector& Location)
{
	PlayerLocation = Location;
	Refresh();
}

FVector2D UInsimulWorldMapPanel::MapPointFor(const FVector& WorldLocation) const
{
	const double Width = WorldMax.X - WorldMin.X;
	const double Height = WorldMax.Y - WorldMin.Y;
	if (Width == 0.0 || Height == 0.0)
	{
		return FVector2D(0.5, 0.5);
	}
	// UE's X is north and Y is east; a map's X is right (east) and Y is up (north).
	const double X = (WorldLocation.Y - WorldMin.Y) / Height;
	const double Y = (WorldLocation.X - WorldMin.X) / Width;
	return FVector2D(X, Y);
}

TArray<FInsimulMapPin> UInsimulWorldMapPanel::VisibleMarkers() const
{
	TArray<FInsimulMapPin> Out;
	for (const FInsimulMapPin& Marker : MapMarkers)
	{
		if (Marker.bDiscovered)
		{
			Out.Add(Marker);
		}
	}
	return Out;
}

TArray<FInsimulMapPin> UInsimulWorldMapPanel::FastTravelMarkers() const
{
	TArray<FInsimulMapPin> Out;
	for (const FInsimulMapPin& Marker : MapMarkers)
	{
		if (Marker.bDiscovered && Marker.bFastTravel)
		{
			Out.Add(Marker);
		}
	}
	return Out;
}

bool UInsimulWorldMapPanel::Select(const FString& MarkerId)
{
	const FInsimulMapPin* Found = MapMarkers.FindByPredicate(
		[&MarkerId](const FInsimulMapPin& Marker)
		{ return Marker.Id == MarkerId && Marker.bDiscovered; });
	if (!Found)
	{
		return false;
	}
	if (Selected == MarkerId)
	{
		return true;
	}
	Selected = MarkerId;
	Refresh();
	OnSelectionChanged.Broadcast(Selected);
	return true;
}

bool UInsimulWorldMapPanel::RequestFastTravel()
{
	if (Selected.IsEmpty())
	{
		return false;
	}
	const FInsimulMapPin* Found = MapMarkers.FindByPredicate(
		[this](const FInsimulMapPin& Marker)
		{ return Marker.Id == Selected && Marker.bDiscovered && Marker.bFastTravel; });
	if (!Found)
	{
		return false;
	}
	// A REQUEST. Whether the world lets them cross it is the simulation's answer.
	OnFastTravelRequested.Broadcast(Selected);
	return true;
}

void UInsimulWorldMapPanel::Refresh()
{
	if (SelectionLabelText)
	{
		const FInsimulMapPin* Found = MapMarkers.FindByPredicate(
			[this](const FInsimulMapPin& Marker) { return Marker.Id == Selected; });
		SelectionLabelText->SetText(Found ? FText::FromString(Found->Label) : FText::GetEmpty());
	}

	if (!MarkerContainer)
	{
		return;
	}
	MarkerContainer->ClearChildren();
	for (const FInsimulMapPin& Marker : VisibleMarkers())
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
