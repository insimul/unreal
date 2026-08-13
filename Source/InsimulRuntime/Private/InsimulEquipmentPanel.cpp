// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulEquipmentPanel — thin UUserWidget over FInsimulEquipmentModel. Marshals
// FString<->std::string and renders; every equipment SEMANTIC (the slot table, the
// refusal ladder, the carried weight) is the portable core's and is host-tested
// against the shared equipping corpus.

#include "InsimulEquipmentPanel.h"

#include "InsimulRuntimeSubsystem.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

#include "../Portable/InsimulBootstrap.h"
#include "../Portable/InsimulEquipmentModel.h"
#include "../Portable/InsimulSaveSystem.h"
#include "../Portable/InsimulUIStateBinding.h"

const FName UInsimulEquipmentPanel::PanelKey = FName(TEXT("equipment"));

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

insimul::FInsimulEquipmentModel& UInsimulEquipmentPanel::EnsureModel()
{
	if (!Ledger.IsValid())
	{
		Ledger = MakeUnique<insimul::FItemLedger>();
	}
	if (!Model.IsValid())
	{
		Model = MakeUnique<insimul::FInsimulEquipmentModel>(Ledger.Get());
	}
	return *Model;
}

bool UInsimulEquipmentPanel::BindToRuntime(UInsimulRuntimeSubsystem* Runtime, const FString& Actor)
{
	EnsureModel();
	ActorId = Actor;

	const insimul::FJsonValue* SaveFile =
		(Runtime && Runtime->Context()) ? Runtime->Context()->Save().SaveFile() : nullptr;
	if (!SaveFile)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Insimul equipment panel: no booted runtime; the loadout would not be "
				 "this playthrough's."));
		Refresh();
		return false;
	}

	std::string Error;
	const bool bOk = insimul::FInsimulUIStateBinding::HydrateLedger(
		*SaveFile, ToStd(Actor), *Ledger, Error);
	if (!bOk)
	{
		UE_LOG(LogTemp, Error, TEXT("Insimul equipment panel: %s"), *ToFString(Error));
	}
	Refresh();
	return bOk;
}

TArray<FString> UInsimulEquipmentPanel::WornItems() const
{
	TArray<FString> Out;
	if (!Model.IsValid())
	{
		return Out;
	}
	for (const std::string& Item : Model->Loadout(ToStd(ActorId)).Worn)
	{
		Out.Add(ToFString(Item));
	}
	return Out;
}

int32 UInsimulEquipmentPanel::Armor() const
{
	return Model.IsValid() ? static_cast<int32>(Model->Loadout(ToStd(ActorId)).Armor) : 0;
}

int32 UInsimulEquipmentPanel::CarriedWeight() const
{
	return Model.IsValid() ? static_cast<int32>(Model->Loadout(ToStd(ActorId)).Weight) : 0;
}

bool UInsimulEquipmentPanel::IsEncumbered() const
{
	return Model.IsValid() && Model->Loadout(ToStd(ActorId)).bEncumbered;
}

FInsimulEquipResolution UInsimulEquipmentPanel::CanEquip(const FString& ItemId) const
{
	FInsimulEquipResolution Out;
	Out.ItemId = ItemId;
	if (!Model.IsValid())
	{
		Out.Refusal = TEXT("unknown");
		return Out;
	}

	const insimul::FEquipQuery Query = Model->QueryFor(ToStd(ActorId), ToStd(ItemId));
	const insimul::FEquipResolution Resolved = Model->Resolve(Query);
	Out.Slot = ToFString(Resolved.Slot);
	Out.bAvailable = Resolved.bAvailable;
	Out.Capacity = static_cast<int32>(Resolved.Capacity);
	Out.Occupied = static_cast<int32>(Resolved.Occupied);
	Out.Refusal = ToFString(Resolved.Refusal);
	for (const insimul::FItemRequirement& Req : Resolved.Unmet)
	{
		FInsimulEquipRequirement Row;
		Row.Skill = ToFString(Req.Skill);
		Row.Level = static_cast<int32>(Req.Level);
		Out.Unmet.Add(Row);
	}
	return Out;
}

void UInsimulEquipmentPanel::Refresh()
{
	const TArray<FString> Worn = WornItems();

	if (ArmorText)
	{
		ArmorText->SetText(FText::AsNumber(Armor()));
	}
	if (WeightText)
	{
		WeightText->SetText(FText::AsNumber(CarriedWeight()));
	}
	if (EncumberedText)
	{
		EncumberedText->SetVisibility(IsEncumbered() ? ESlateVisibility::Visible
													 : ESlateVisibility::Collapsed);
	}

	if (SlotListBox)
	{
		SlotListBox->ClearChildren();
		for (const FString& Item : Worn)
		{
			if (SlotRowClass)
			{
				if (UUserWidget* Row = CreateWidget<UUserWidget>(this, SlotRowClass))
				{
					SlotListBox->AddChildToVerticalBox(Row);
					continue;
				}
			}
			UTextBlock* Row = NewObject<UTextBlock>(this);
			Row->SetText(FText::FromString(Item));
			SlotListBox->AddChildToVerticalBox(Row);
		}
	}

	OnLoadoutChanged.Broadcast();
}

void UInsimulEquipmentPanel::BeginDestroy()
{
	Model.Reset();
	Ledger.Reset();
	Super::BeginDestroy();
}
