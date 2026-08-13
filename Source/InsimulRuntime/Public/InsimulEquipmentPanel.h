// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulEquipmentPanel — what the player is wearing (US-2 of tasklist 190), panel
// key `equipment`, owned by the equipment module.
//
// READ-ONLY ON PURPOSE. This panel shows the worn set, the armour it comes to, the
// carried weight and whether a thing MAY go on — and it performs no equip. Wearing
// a thing is the items module's decision layer, and a UI that wrote a loadout would
// be inventing a save schema the other three engine legs do not have. The panel
// therefore cannot break the state-location invariant: it never writes.
//
// THE SLOTS ARE THE WORLD'S. There is no compiled slot enum behind this: `back` is a
// slot a world's content authored and `ring` holds two because that world said two,
// so the rows here are whatever the ledger's slot table declares.
//
// A refusal is REPORTED rather than counted — the panel can say "you need Heavy
// Armour 2" and "1 of 2 rings" because FInsimulEquipmentModel hands it the unmet
// requirement and the slot's capacity. Those semantics are host-tested against the
// shared equipping corpus by ctest `ui_state_binding`.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InsimulEquipmentPanel.generated.h"

class UInsimulRuntimeSubsystem;
class UTextBlock;
class UVerticalBox;

// The engine-agnostic model + ledger this seam wraps (pimpl — never in the
// reflected layout).
namespace insimul { class FInsimulEquipmentModel; struct FItemLedger; }

/** A skill level an item wants before it may be worn. */
USTRUCT(BlueprintType)
struct FInsimulEquipRequirement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Equipment")
	FString Skill;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Equipment")
	int32 Level = 0;
};

/** Whether a thing may go on, and why not when it may not. */
USTRUCT(BlueprintType)
struct FInsimulEquipResolution
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Equipment")
	FString ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Equipment")
	FString Slot;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Equipment")
	bool bAvailable = false;

	/** Reported whether or not it refuses, so a panel can show "1 of 2". */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Equipment")
	int32 Capacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Equipment")
	int32 Occupied = 0;

	/** Empty when available; else unknown / not_held / no_slot / unknown_slot /
	 *  already_equipped / slot_full / requires. */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Equipment")
	FString Refusal;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Equipment")
	TArray<FInsimulEquipRequirement> Unmet;
};

/** Fired after a repaint. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInsimulLoadoutChanged);

UCLASS()
class INSIMULRUNTIME_API UInsimulEquipmentPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The stable panel key this widget serves (see the shipped panel catalog). */
	static const FName PanelKey;

	/** Read the ledger out of the booted runtime's save and repaint. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Equipment")
	bool BindToRuntime(UInsimulRuntimeSubsystem* Runtime, const FString& Actor);

	/** The worn item ids, in the world's own slot order. */
	UFUNCTION(BlueprintPure, Category = "Insimul|Equipment")
	TArray<FString> WornItems() const;

	UFUNCTION(BlueprintPure, Category = "Insimul|Equipment")
	int32 Armor() const;

	/** Everything the actor carries, worn included — a worn plate still weighs. */
	UFUNCTION(BlueprintPure, Category = "Insimul|Equipment")
	int32 CarriedWeight() const;

	UFUNCTION(BlueprintPure, Category = "Insimul|Equipment")
	bool IsEncumbered() const;

	/** Whether `ItemId` may go on this actor, and why not when it may not. */
	UFUNCTION(BlueprintPure, Category = "Insimul|Equipment")
	FInsimulEquipResolution CanEquip(const FString& ItemId) const;

	UPROPERTY(BlueprintAssignable, Category = "Insimul|Equipment")
	FOnInsimulLoadoutChanged OnLoadoutChanged;

	/** The row widget class a creator supplies; unset builds a plain text row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Equipment")
	TSubclassOf<UUserWidget> SlotRowClass;

	UFUNCTION(BlueprintCallable, Category = "Insimul|Equipment")
	void Refresh();

	virtual void BeginDestroy() override;

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> SlotListBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ArmorText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WeightText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EncumberedText;

private:
	/** The one store's projection — rebuilt by BindToRuntime, never edited here. */
	TUniquePtr<insimul::FItemLedger> Ledger;
	TUniquePtr<insimul::FInsimulEquipmentModel> Model;

	FString ActorId;

	insimul::FInsimulEquipmentModel& EnsureModel();
};
