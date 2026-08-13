// Copyright 2024 Insimul. All Rights Reserved.

#include "InsimulEquipmentModel.h"

#include "InsimulCanonicalJson.h"

#include <algorithm>
#include <map>

namespace insimul {
namespace {

// ── JSON node factories (mutable building; mirrors InsimulSaveSystem.cpp) ────

FJsonValuePtr MakeObject() {
	auto Node = std::make_shared<FJsonValue>();
	Node->Type = EJsonType::Object;
	return Node;
}

FJsonValuePtr MakeArray() {
	auto Node = std::make_shared<FJsonValue>();
	Node->Type = EJsonType::Array;
	return Node;
}

FJsonValuePtr MakeString(const std::string& S) {
	auto Node = std::make_shared<FJsonValue>();
	Node->Type = EJsonType::String;
	Node->StringValue = S;
	return Node;
}

FJsonValuePtr MakeBool(bool B) {
	auto Node = std::make_shared<FJsonValue>();
	Node->Type = EJsonType::Bool;
	Node->BoolValue = B;
	return Node;
}

FJsonValuePtr MakeInt(long long N) {
	auto Node = std::make_shared<FJsonValue>();
	Node->Type = EJsonType::Number;
	Node->NumberValue = static_cast<double>(N);
	Node->RawNumber = std::to_string(N);
	return Node;
}

void ObjSet(FJsonValue& Obj, const std::string& Key, FJsonValuePtr Value) {
	for (auto& Pair : Obj.ObjectItems) {
		if (Pair.first == Key) {
			Pair.second = std::move(Value);
			return;
		}
	}
	Obj.ObjectItems.emplace_back(Key, std::move(Value));
}

FJsonValuePtr RequirementArray(const std::vector<FItemRequirement>& Reqs) {
	auto Out = MakeArray();
	for (const FItemRequirement& Req : Reqs) {
		auto Row = MakeObject();
		ObjSet(*Row, "skill", MakeString(Req.Skill));
		ObjSet(*Row, "level", MakeInt(Req.Level));
		Out->ArrayItems.push_back(Row);
	}
	return Out;
}

/** `predicate(a, b, c).` — the shared 110 vocabulary's surface form. */
std::string Fact(const std::string& Predicate, const std::vector<std::string>& Args) {
	std::string Out = Predicate + "(";
	for (std::size_t I = 0; I < Args.size(); ++I) {
		if (I > 0) {
			Out += ", ";
		}
		Out += Args[I];
	}
	Out += ").";
	return Out;
}

constexpr const char* KindInventory = "inventory";
constexpr const char* KindEquipped = "equipped";
constexpr const char* KindContainer = "container";
constexpr const char* KindWorld = "world";

} // namespace

// ── Ledger lookups ──────────────────────────────────────────────────────────

const FCatalogueItem* FItemLedger::FindItem(const std::string& Id) const {
	for (const FCatalogueItem& Row : Catalogue) {
		if (Row.Id == Id) {
			return &Row;
		}
	}
	return nullptr;
}

const FEquipSlotRow* FItemLedger::FindSlot(const std::string& Id) const {
	for (const FEquipSlotRow& Row : Slots) {
		if (Row.Id == Id) {
			return &Row;
		}
	}
	return nullptr;
}

long long FItemLedger::LevelOf(const std::string& Skill) const {
	for (const auto& Pair : Levels) {
		if (Pair.first == Skill) {
			return Pair.second;
		}
	}
	return 0;
}

FEquipTuning FEquipTuning::FromJson(const FJsonValue& Tuning) {
	FEquipTuning Out;
	if (!Tuning.IsObject()) {
		return Out;
	}
	Out.CarryCapacity = Tuning.GetInt("carryCapacity", Out.CarryCapacity);
	Out.DefaultSlotCapacity = Tuning.GetInt("defaultSlotCapacity", Out.DefaultSlotCapacity);
	const std::string Action = Tuning.GetString("equipAction");
	if (!Action.empty()) {
		Out.EquipAction = Action;
	}
	return Out;
}

// ── Loadout ─────────────────────────────────────────────────────────────────

FEquipmentLoadout FInsimulEquipmentModel::Loadout(const std::string& Actor) const {
	FEquipmentLoadout Out;
	if (!Ledger) {
		return Out;
	}

	// Facts are emitted in CANONICAL STACK ORDER — kind, then holder, then slot,
	// then item — so two engines reading the same ledger emit the same list and an
	// equipped stack's two facts stay adjacent (they are one stack, said twice).
	std::vector<const FItemStack*> Ordered;
	Ordered.reserve(Ledger->Stacks.size());
	for (const FItemStack& Stack : Ledger->Stacks) {
		Ordered.push_back(&Stack);
	}
	std::stable_sort(Ordered.begin(), Ordered.end(),
		[](const FItemStack* A, const FItemStack* B) {
			if (A->Place.Kind != B->Place.Kind) return A->Place.Kind < B->Place.Kind;
			if (A->Place.Holder != B->Place.Holder) return A->Place.Holder < B->Place.Holder;
			if (A->Place.Slot != B->Place.Slot) return A->Place.Slot < B->Place.Slot;
			return A->Item < B->Item;
		});

	for (const FItemStack* Stack : Ordered) {
		const std::string Qty = std::to_string(Stack->Quantity);
		if (Stack->Place.Kind == KindEquipped) {
			// Both facts, always: `carried_weight/2` sums has_item/3, so a worn
			// breastplate that stopped being held would stop weighing thirty.
			Out.Facts.push_back(Fact("has_equipped", {Stack->Place.Holder, Stack->Place.Slot, Stack->Item}));
			Out.Facts.push_back(Fact("has_item", {Stack->Place.Holder, Stack->Item, Qty}));
		} else if (Stack->Place.Kind == KindInventory) {
			Out.Facts.push_back(Fact("has_item", {Stack->Place.Holder, Stack->Item, Qty}));
		} else if (Stack->Place.Kind == KindContainer) {
			Out.Facts.push_back(Fact("container_contains", {Stack->Place.Holder, Stack->Item, Qty}));
		} else if (Stack->Place.Kind == KindWorld) {
			Out.Facts.push_back(Fact("item_at", {Stack->Item, Stack->Place.Holder, Qty}));
		}
	}

	// Carried weight: everything the actor holds, worn included.
	std::vector<const FItemStack*> WornStacks;
	for (const FItemStack& Stack : Ledger->Stacks) {
		if (Stack.Place.Holder != Actor) {
			continue;
		}
		const bool bWorn = Stack.Place.Kind == KindEquipped;
		if (!bWorn && Stack.Place.Kind != KindInventory) {
			continue;
		}
		if (const FCatalogueItem* Row = Ledger->FindItem(Stack.Item)) {
			Out.Weight += Row->Weight * Stack.Quantity;
		}
		if (bWorn) {
			WornStacks.push_back(&Stack);
		}
	}
	Out.bEncumbered = Out.Weight > Ledger->Tuning.CarryCapacity;

	// The panel's row order is the world's own slot order, then the item id, so two
	// rings in one slot do not swap places between frames.
	std::stable_sort(WornStacks.begin(), WornStacks.end(),
		[this](const FItemStack* A, const FItemStack* B) {
			const FEquipSlotRow* SlotA = Ledger->FindSlot(A->Place.Slot);
			const FEquipSlotRow* SlotB = Ledger->FindSlot(B->Place.Slot);
			const long long OrderA = SlotA ? SlotA->Order : 0;
			const long long OrderB = SlotB ? SlotB->Order : 0;
			if (OrderA != OrderB) return OrderA < OrderB;
			if (A->Place.Slot != B->Place.Slot) return A->Place.Slot < B->Place.Slot;
			return A->Item < B->Item;
		});

	std::map<std::string, long long> Effects;
	for (const FItemStack* Stack : WornStacks) {
		Out.Worn.push_back(Stack->Item);
		const FCatalogueItem* Row = Ledger->FindItem(Stack->Item);
		if (!Row) {
			continue;
		}
		Out.Armor += Row->Armor;
		for (const auto& Effect : Row->Effects) {
			Effects[Effect.first] += Effect.second;
		}
	}
	for (const auto& Pair : Effects) {
		Out.Modifiers.emplace_back(Pair.first, Pair.second);
	}
	return Out;
}

// ── Resolution ──────────────────────────────────────────────────────────────

FEquipQuery FInsimulEquipmentModel::QueryFor(const std::string& Actor, const std::string& ItemId) const {
	FEquipQuery Out;
	Out.Actor = Actor;
	Out.ItemId = ItemId;
	if (!Ledger) {
		return Out;
	}

	std::string TargetSlot;
	if (const FCatalogueItem* Row = Ledger->FindItem(ItemId)) {
		if (Row->bHasEquipSlot) {
			TargetSlot = Row->EquipSlot;
		}
	}

	for (const FItemStack& Stack : Ledger->Stacks) {
		if (Stack.Place.Holder != Actor) {
			continue;
		}
		const bool bWorn = Stack.Place.Kind == KindEquipped;
		if (Stack.Item == ItemId && (bWorn || Stack.Place.Kind == KindInventory)) {
			Out.bHeld = true;
			if (bWorn) {
				Out.bEquipped = true;
			}
		}
		if (bWorn && !TargetSlot.empty() && Stack.Place.Slot == TargetSlot) {
			Out.Occupied += 1;
		}
	}
	return Out;
}

FEquipResolution FInsimulEquipmentModel::Resolve(const FEquipQuery& Query) const {
	FEquipResolution Out;
	Out.Actor = Query.Actor;
	Out.Item = Query.ItemId;
	Out.Occupied = Query.Occupied;
	Out.Action = Ledger ? Ledger->Tuning.EquipAction : FEquipTuning().EquipAction;

	const FCatalogueItem* Row = Ledger ? Ledger->FindItem(Query.ItemId) : nullptr;

	// The slot and its capacity are REPORTED whether or not the attempt succeeds, so
	// a panel can say "1 of 2 rings" next to a refusal as easily as next to an offer.
	const FEquipSlotRow* SlotRow = nullptr;
	if (Row && Row->bHasEquipSlot) {
		Out.Slot = Row->EquipSlot;
		SlotRow = Ledger->FindSlot(Row->EquipSlot);
		if (SlotRow) {
			Out.Capacity = SlotRow->bHasCapacity ? SlotRow->Capacity
												 : Ledger->Tuning.DefaultSlotCapacity;
		}
	}

	if (Row) {
		for (const FItemRequirement& Req : Row->Requires) {
			if (Ledger->LevelOf(Req.Skill) < Req.Level) {
				Out.Unmet.push_back(Req);
			}
		}
	}

	// The ladder, in the corpus's order. `forbidden` is absent on purpose: that is
	// `forbids/4`'s answer and a pure function may not ask a KB.
	if (!Row) {
		Out.Refusal = "unknown";
	} else if (!Query.bHeld) {
		Out.Refusal = "not_held";
	} else if (!Row->bHasEquipSlot) {
		Out.Refusal = "no_slot";
	} else if (!SlotRow) {
		Out.Refusal = "unknown_slot";
	} else if (Query.bEquipped) {
		Out.Refusal = "already_equipped";
	} else if (Query.Occupied >= Out.Capacity) {
		Out.Refusal = "slot_full";
	} else if (!Out.Unmet.empty()) {
		Out.Refusal = "requires";
	} else {
		Out.bAvailable = true;
	}
	return Out;
}

// ── Projection ──────────────────────────────────────────────────────────────

FJsonValuePtr FInsimulEquipmentModel::ToProjection(const FEquipQuery& Query) const {
	const FEquipmentLoadout Load = Loadout(Query.Actor);
	const FEquipResolution Res = Resolve(Query);

	auto Root = MakeObject();
	ObjSet(*Root, "armor", MakeInt(Load.Armor));
	ObjSet(*Root, "encumbered", MakeBool(Load.bEncumbered));

	auto Facts = MakeArray();
	for (const std::string& F : Load.Facts) {
		Facts->ArrayItems.push_back(MakeString(F));
	}
	ObjSet(*Root, "facts", Facts);

	auto Modifiers = MakeObject();
	for (const auto& Pair : Load.Modifiers) {
		ObjSet(*Modifiers, Pair.first, MakeInt(Pair.second));
	}
	ObjSet(*Root, "modifiers", Modifiers);

	auto Resolution = MakeObject();
	ObjSet(*Resolution, "action", MakeString(Res.Action));
	ObjSet(*Resolution, "actor", MakeString(Res.Actor));
	ObjSet(*Resolution, "available", MakeBool(Res.bAvailable));
	ObjSet(*Resolution, "capacity", MakeInt(Res.Capacity));
	ObjSet(*Resolution, "item", MakeString(Res.Item));
	ObjSet(*Resolution, "occupied", MakeInt(Res.Occupied));
	if (!Res.Refusal.empty()) {
		ObjSet(*Resolution, "refusal", MakeString(Res.Refusal));
	}
	ObjSet(*Resolution, "slot", MakeString(Res.Slot));
	ObjSet(*Resolution, "unmet", RequirementArray(Res.Unmet));
	ObjSet(*Root, "resolution", Resolution);

	ObjSet(*Root, "unmet", RequirementArray(Res.Unmet));
	ObjSet(*Root, "weight", MakeInt(Load.Weight));

	auto Worn = MakeArray();
	for (const std::string& W : Load.Worn) {
		Worn->ArrayItems.push_back(MakeString(W));
	}
	ObjSet(*Root, "worn", Worn);
	return Root;
}

std::string FInsimulEquipmentModel::ProjectionCanonical(const FEquipQuery& Query) const {
	const FJsonValuePtr Projection = ToProjection(Query);
	return CanonicalJsonStringify(*Projection);
}

} // namespace insimul
