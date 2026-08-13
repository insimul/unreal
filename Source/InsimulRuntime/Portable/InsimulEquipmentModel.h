// Copyright 2024 Insimul. All Rights Reserved.
//
// FInsimulEquipmentModel — the equipment panel's view-model (US-2 of tasklist 190):
// what the actor is WEARING, what it comes to, and whether a thing may go on.
//
// ONE LEDGER, NOT A SECOND ONE. The panel holds a POINTER to the caller's
// FItemLedger — the stacks, the world's own slot table, the catalogue rows they name
// and the actor's skill levels — and keeps no private store, exactly as
// FInsimulTradeModel holds a pointer to the currentState slice
// (InsimulTradeModel.h). Inventory and equipment are the SAME stacks in different
// places: a worn breastplate emits `has_equipped/3` AND `has_item/3`, because
// `carried_weight/2` sums the latter and thirty units of steel do not stop being
// carried when you put them on. A model that retracted the item on equip would make
// a player lighter by wearing armour.
//
// THE WORLD DECLARES THE SLOTS. There is no compiled slot enum here: `back` is a slot
// a world's content authored, and `ring` holds two because that world said two. The
// slot table rides in the ledger, so adding a slot is content, not an engine change.
//
// REFUSALS ARE REPORTED, NOT COUNTED. A refusal names itself in the fixed order
// unknown -> not_held -> no_slot -> unknown_slot -> already_equipped -> slot_full ->
// requires, and an unmet requirement is reported in full so a panel can say "you need
// Heavy Armour 2" instead of greying a button out. `forbidden` is deliberately NOT
// among them: permissibility is the KB's answer (`forbids/4`) and this is a pure
// function, so a verdict here would be a guess.
//
// The semantics are `conformance/items/equipping.json`'s (band 124, US-1), which
// every engine leg runs; ctest `ui_state_binding` diffs the canonical projection of
// all twelve cases byte for byte.
//
// READ-ONLY BY DESIGN. Performing an equip is the items module's decision layer, not
// the UI's: this model answers what is worn and whether one MAY equip. That is also
// why the panel cannot violate the state-location invariant — it never writes.
//
// std-only (no Unreal Engine, no CoreMinimal.h). The UMG seam is
// UInsimulEquipmentPanel (Public/InsimulEquipmentPanel.h), syntax-gated only.

#pragma once

#include "InsimulJson.h"

#include <string>
#include <utility>
#include <vector>

namespace insimul {

/** A slot row the WORLD declared: its id, its display name and how many it holds. */
struct FEquipSlotRow {
	std::string Id;
	std::string Name;
	bool bHasCapacity = false;
	long long Capacity = 0;
	long long Order = 0;
};

/** Where a stack is. `Kind` is "inventory" | "equipped" | "container" | "world". */
struct FItemPlace {
	std::string Kind;
	/** The actor / container / place holding it. */
	std::string Holder;
	/** Only meaningful for "equipped". */
	std::string Slot;
};

/** One stack of one item in one place. */
struct FItemStack {
	std::string Item;
	FItemPlace Place;
	long long Quantity = 0;
};

/** A skill level an item wants before it may be worn. */
struct FItemRequirement {
	std::string Skill;
	long long Level = 0;
};

/** A catalogue row — what the content says the thing IS. */
struct FCatalogueItem {
	std::string Id;
	bool bHasEquipSlot = false;
	std::string EquipSlot;
	long long Weight = 0;
	long long Armor = 0;
	std::vector<FItemRequirement> Requires;
	/** Effects the item confers while worn, in authored order. */
	std::vector<std::pair<std::string, long long>> Effects;
};

/** The world's authored equipment dials (the resolved ItemTuning subset). */
struct FEquipTuning {
	long long CarryCapacity = 40;
	long long DefaultSlotCapacity = 1;
	/** The world's own action atom for an equip (never hard-coded here). */
	std::string EquipAction = "equip";

	static FEquipTuning FromJson(const FJsonValue& Tuning);
};

/**
 * The ledger the panel reads: every stack the world holds, the slot table, the
 * catalogue rows and the actor's levels. This is the single store — the model
 * copies none of it.
 */
struct FItemLedger {
	std::vector<FItemStack> Stacks;
	std::vector<FEquipSlotRow> Slots;
	std::vector<FCatalogueItem> Catalogue;
	/** skill -> level, for the actor the panel is showing. */
	std::vector<std::pair<std::string, long long>> Levels;
	FEquipTuning Tuning;

	const FCatalogueItem* FindItem(const std::string& Id) const;
	const FEquipSlotRow* FindSlot(const std::string& Id) const;
	long long LevelOf(const std::string& Skill) const;
};

/** What an equip attempt amounts to, with the reason when it is refused. */
struct FEquipResolution {
	/** The world's own action atom (tuning.equipAction). */
	std::string Action;
	std::string Actor;
	std::string Item;
	/** The slot the item names, "" when it names none or the catalogue lacks it. */
	std::string Slot;
	bool bAvailable = false;
	long long Capacity = 0;
	long long Occupied = 0;
	/** Empty when available; else one of the fixed refusal atoms. */
	std::string Refusal;
	std::vector<FItemRequirement> Unmet;
};

/** What the actor is wearing and what it comes to. */
struct FEquipmentLoadout {
	/** Worn item ids in slot order, then item order — the panel's row order. */
	std::vector<std::string> Worn;
	long long Armor = 0;
	/** Everything the actor carries, worn included. */
	long long Weight = 0;
	bool bEncumbered = false;
	/** Summed effects of the worn set (sorted by name in the projection). */
	std::vector<std::pair<std::string, long long>> Modifiers;
	/** Every stack said in the shared vocabulary, in canonical stack order. */
	std::vector<std::string> Facts;
};

/** What the equip query already knows about the actor's relationship to the item. */
struct FEquipQuery {
	std::string Actor;
	std::string ItemId;
	/** The actor has the item at all (`has_item/3`). */
	bool bHeld = false;
	/** The actor is already wearing THIS item (`has_equipped/3`). */
	bool bEquipped = false;
	/** How many of the target slot's places are already taken. */
	long long Occupied = 0;
};

class FInsimulEquipmentModel {
public:
	FInsimulEquipmentModel() = default;
	explicit FInsimulEquipmentModel(const FItemLedger* InLedger) : Ledger(InLedger) {}

	/** Bind the ledger. The model reads it in place and never copies it. */
	void Attach(const FItemLedger* InLedger) { Ledger = InLedger; }

	/** True once a ledger is attached (a detached model answers empties, never crashes). */
	bool IsAttached() const { return Ledger != nullptr; }

	/** What `Actor` is wearing, weighs, and is warded by. */
	FEquipmentLoadout Loadout(const std::string& Actor) const;

	/** Whether `Query.ItemId` may go on `Query.Actor`, and why not when it may not. */
	FEquipResolution Resolve(const FEquipQuery& Query) const;

	/** Derive `bHeld` / `bEquipped` / `Occupied` for an item straight off the ledger. */
	FEquipQuery QueryFor(const std::string& Actor, const std::string& ItemId) const;

	/** The projection the corpus pins: loadout + resolution in one object. */
	FJsonValuePtr ToProjection(const FEquipQuery& Query) const;

	/** Canonical JSON of that projection — byte-comparable with the corpus. */
	std::string ProjectionCanonical(const FEquipQuery& Query) const;

private:
	const FItemLedger* Ledger = nullptr;
};

} // namespace insimul
