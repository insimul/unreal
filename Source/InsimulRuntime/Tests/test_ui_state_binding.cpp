// Copyright 2024 Insimul. All Rights Reserved.
//
// test_ui_state_binding.cpp — host gate for the three claims US-2 of tasklist 190
// makes about the inventory / container / equipment / shop panels:
//
//   1. SHOP + REPUTATION. A price is a function of the simulation, not a shop table:
//      every case of the shared pricing corpus (conformance/items/pricing.json) is
//      driven through FInsimulTradePricing and the canonical projection is diffed
//      byte for byte against the corpus's expected price — including the corpus's
//      own teeth, the same sword in the same shop at two prices because the KB knows
//      one buyer stands well with the faction the shop answers to.
//
//   2. EQUIPMENT. Every case of the shared equipping corpus
//      (conformance/items/equipping.json) is driven through FInsimulEquipmentModel:
//      the worn set, the carried weight (a worn breastplate still weighs thirty),
//      the armour, the modifiers, the facts in the shared vocabulary, and the whole
//      refusal ladder with its reported capacity and unmet requirements.
//
//   3. THE STATE-LOCATION INVARIANT. The panels are backed EXCLUSIVELY by
//      save.currentState: a real SaveFile fixture is loaded through
//      FInsimulSaveSystem, the trade / journal panels are hydrated from its
//      currentState, ops are run, the result is written back, and the test asserts
//      the change is in the save's canonical bytes, that a re-hydrate reproduces it
//      exactly, that everything OUTSIDE currentState is byte-identical, and that the
//      item + gold census is conserved.
//
// Negative controls are mandatory in this repo (CLAUDE.md): each block carries
// trials that prove it can fail — a mutated slot capacity flips a refusal, a removed
// standing fact moves a price, and an op that is never flushed leaves the save alone.
//
// Corpus dirs come from the compile definitions the ctest target sets; argv[1] and
// argv[2] override them (items dir, saves dir).

#include "../Portable/InsimulEquipmentModel.h"
#include "../Portable/InsimulJson.h"
#include "../Portable/InsimulQuestJournalModel.h"
#include "../Portable/InsimulCanonicalJson.h"
#include "../Portable/InsimulSaveSystem.h"
#include "../Portable/InsimulTradeModel.h"
#include "../Portable/InsimulTradePricing.h"
#include "../Portable/InsimulUIStateBinding.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace insimul;

namespace {

int g_pass = 0;
int g_fail = 0;

void Report(const std::string& Name, bool bOk, const std::string& Detail = "") {
	std::printf("  %s  %-62s%s%s\n", bOk ? "PASS" : "FAIL", Name.c_str(),
			Detail.empty() ? "" : "  ", Detail.c_str());
	if (bOk) {
		g_pass++;
	} else {
		g_fail++;
	}
}

std::string ReadFile(const std::string& Path) {
	std::ifstream In(Path, std::ios::binary);
	if (!In) {
		return std::string();
	}
	std::ostringstream Ss;
	Ss << In.rdbuf();
	return Ss.str();
}

FJsonValuePtr LoadJson(const std::string& Dir, const std::string& File) {
	const std::string Text = ReadFile(Dir + "/" + File);
	if (Text.empty()) {
		return nullptr;
	}
	FJsonParseResult Parsed = ParseJson(Text);
	return Parsed.bOk ? Parsed.Root : nullptr;
}

const FJsonValue* Member(const FJsonValue& Obj, const std::string& Key) {
	return Obj.Find(Key);
}

// ── Corpus readers ──────────────────────────────────────────────────────────

FPriceItem ReadPriceItem(const FJsonValue& Case) {
	const FJsonValue* Item = Member(Case, "item");
	const std::string FallbackId = Case.GetString("itemId");
	if (Item && Item->IsObject()) {
		return FPriceItem::FromJson(Item, FallbackId);
	}
	return FPriceItem::FromJson(nullptr, FallbackId);
}

FItemRequirement ReadRequirement(const FJsonValue& Row) {
	FItemRequirement Out;
	Out.Skill = Row.GetString("skill");
	Out.Level = Row.GetInt("level", 0);
	return Out;
}

FCatalogueItem ReadCatalogueRow(const FJsonValue& Row) {
	FCatalogueItem Out;
	Out.Id = Row.GetString("id");
	const std::string Slot = Row.GetString("equipSlot");
	if (!Slot.empty()) {
		Out.bHasEquipSlot = true;
		Out.EquipSlot = Slot;
	}
	Out.Weight = Row.GetInt("weight", 0);
	Out.Armor = Row.GetInt("armor", 0);
	if (const FJsonValue* Requires = Row.Find("requires")) {
		if (Requires->IsArray()) {
			for (const FJsonValuePtr& Req : Requires->ArrayItems) {
				if (Req && Req->IsObject()) {
					Out.Requires.push_back(ReadRequirement(*Req));
				}
			}
		}
	}
	if (const FJsonValue* Effects = Row.Find("effects")) {
		if (Effects->IsObject()) {
			for (const auto& Pair : Effects->ObjectItems) {
				if (Pair.second) {
					Out.Effects.emplace_back(Pair.first, Pair.second->AsInt(0));
				}
			}
		}
	}
	return Out;
}

/** Build the ledger + query an equipping case describes. */
void ReadEquipCase(const FJsonValue& Input, FItemLedger& Ledger, FEquipQuery& Query) {
	Ledger = FItemLedger();

	if (const FJsonValue* Catalogue = Input.Find("catalogue")) {
		if (Catalogue->IsArray()) {
			for (const FJsonValuePtr& Row : Catalogue->ArrayItems) {
				if (Row && Row->IsObject()) {
					Ledger.Catalogue.push_back(ReadCatalogueRow(*Row));
				}
			}
		}
	}
	if (const FJsonValue* Slots = Input.Find("slots")) {
		if (Slots->IsArray()) {
			for (const FJsonValuePtr& Row : Slots->ArrayItems) {
				if (!Row || !Row->IsObject()) {
					continue;
				}
				FEquipSlotRow Slot;
				Slot.Id = Row->GetString("id");
				Slot.Name = Row->GetString("name");
				if (const FJsonValue* Capacity = Row->Find("capacity")) {
					if (Capacity->IsNumber()) {
						Slot.bHasCapacity = true;
						Slot.Capacity = Capacity->AsInt(0);
					}
				}
				Slot.Order = Row->GetInt("order", 0);
				Ledger.Slots.push_back(Slot);
			}
		}
	}
	if (const FJsonValue* Stacks = Input.Find("stacks")) {
		if (Stacks->IsArray()) {
			for (const FJsonValuePtr& Row : Stacks->ArrayItems) {
				if (!Row || !Row->IsObject()) {
					continue;
				}
				FItemStack Stack;
				Stack.Item = Row->GetString("item");
				Stack.Quantity = Row->GetInt("quantity", 0);
				if (const FJsonValue* Place = Row->Find("place")) {
					Stack.Place.Kind = Place->GetString("kind");
					Stack.Place.Holder = Place->GetString("holder");
					Stack.Place.Slot = Place->GetString("slot");
				}
				Ledger.Stacks.push_back(Stack);
			}
		}
	}
	if (const FJsonValue* Levels = Input.Find("levels")) {
		if (Levels->IsObject()) {
			for (const auto& Pair : Levels->ObjectItems) {
				if (Pair.second) {
					Ledger.Levels.emplace_back(Pair.first, Pair.second->AsInt(0));
				}
			}
		}
	}
	if (const FJsonValue* Tuning = Input.Find("tuning")) {
		Ledger.Tuning = FEquipTuning::FromJson(*Tuning);
	}

	Query = FEquipQuery();
	Query.Actor = Input.GetString("actor");
	const FJsonValue* Item = Input.Find("item");
	Query.ItemId = (Item && Item->IsObject()) ? Item->GetString("id") : Input.GetString("itemId");
	Query.bHeld = Input.GetBool("held", false);
	Query.bEquipped = Input.GetBool("equipped", false);
	Query.Occupied = Input.GetInt("occupied", 0);
}

// ── 1. Shop + reputation: the pricing corpus ────────────────────────────────

void RunPricingCorpus(const std::string& ItemsDir) {
	std::printf("\n-- shop + reputation: conformance/items/pricing.json --\n");

	FJsonValuePtr Root = LoadJson(ItemsDir, "pricing.json");
	if (!Root || !Root->IsObject()) {
		Report("pricing: corpus is readable", false, ItemsDir);
		return;
	}
	const FJsonValue* Cases = Root->Find("cases");
	if (!Cases || !Cases->IsArray() || Cases->ArrayItems.empty()) {
		Report("pricing: corpus carries cases", false);
		return;
	}

	int Standing = 0;
	for (const FJsonValuePtr& CasePtr : Cases->ArrayItems) {
		if (!CasePtr || !CasePtr->IsObject()) {
			continue;
		}
		const FJsonValue& Case = *CasePtr;
		const std::string Name = Case.GetString("name");
		const FJsonValue* Input = Case.Find("input");
		const FJsonValue* Expected = Case.Find("expected");
		if (!Input || !Expected) {
			Report("pricing: " + Name, false, "case is malformed");
			continue;
		}
		const FJsonValue* ExpectedPrice = Expected->Find("price");
		if (!ExpectedPrice) {
			Report("pricing: " + Name, false, "case has no expected price");
			continue;
		}

		const FPriceItem Item = ReadPriceItem(*Input);
		const FPriceMarket Market = FPriceMarket::FromJson(Input->Find("market"));
		const FJsonValue* TuningNode = Input->Find("tuning");
		const FPriceTuning Tuning = TuningNode ? FPriceTuning::FromJson(*TuningNode) : FPriceTuning();
		if (Market.bHasStanding) {
			++Standing;
		}

		const std::string Got = FInsimulTradePricing::QuoteCanonical(Item, Market, Tuning,
			Input->GetString("actor"), Input->GetString("direction"),
			Input->GetInt("quantity", 1));
		const std::string Want = CanonicalJsonStringify(*ExpectedPrice);
		Report("pricing: " + Name, Got == Want, Got == Want ? "" : ("got " + Got));
	}

	Report("pricing: the corpus exercises reputation at all", Standing >= 2,
		"standing cases: " + std::to_string(Standing));

	// ── negative controls ───────────────────────────────────────────────────
	// The corpus's own teeth, run backwards: take the case that differs from the
	// baseline ONLY by a reputation fact and delete the fact. A port that reads a
	// price off item_value/2 cannot tell these apart, so this must move the number.
	{
		FPriceItem Sword;
		Sword.bDeclared = true;
		Sword.Id = "steel_sword";
		Sword.Value = 100;
		Sword.SellValue = 50;

		FPriceMarket Market;
		Market.bPresent = true;
		Market.VendorId = "hilde";
		Market.Business = "ironmongers";
		Market.bHasMarkupPercent = true;
		Market.MarkupPercent = 20;
		Market.Owner = "tomas";
		Market.Faction = "town_watch";

		FPriceMarket Friendly = Market;
		Friendly.bHasStanding = true;
		Friendly.Standing = 100;

		const FPriceTuning Tuning;
		const FPriceQuote Stranger = FInsimulTradePricing::Quote(Sword, Market, Tuning, "wren", "buy", 1);
		const FPriceQuote Friend = FInsimulTradePricing::Quote(Sword, Friendly, Tuning, "brannoc", "buy", 1);
		Report("control: deleting the reputation fact moves the price",
			Stranger.Unit != Friend.Unit,
			std::to_string(Stranger.Unit) + " vs " + std::to_string(Friend.Unit));
		Report("control: standing flips sign with the direction",
			FInsimulTradePricing::Quote(Sword, Friendly, Tuning, "brannoc", "sell", 1).Unit
				> FInsimulTradePricing::Quote(Sword, Market, Tuning, "wren", "sell", 1).Unit);

		FPriceTuning Floored = Tuning;
		Floored.MinimumPrice = 500;
		Report("control: the world's floor is applied last and binds",
			FInsimulTradePricing::Quote(Sword, Market, Floored, "wren", "buy", 1).Unit == 500);

		Report("control: quantity multiplies the unit and nothing else",
			FInsimulTradePricing::Quote(Sword, Market, Tuning, "wren", "buy", 7).Total
				== Stranger.Unit * 7);
	}
	Report("control: rounding is half away from zero",
		FInsimulTradePricing::RoundHalfAwayFromZero(4.5) == 5
			&& FInsimulTradePricing::RoundHalfAwayFromZero(-4.5) == -5
			&& FInsimulTradePricing::RoundHalfAwayFromZero(3.2) == 3);
}

// ── 2. Equipment: the equipping corpus ──────────────────────────────────────

void RunEquippingCorpus(const std::string& ItemsDir) {
	std::printf("\n-- equipment: conformance/items/equipping.json --\n");

	FJsonValuePtr Root = LoadJson(ItemsDir, "equipping.json");
	if (!Root || !Root->IsObject()) {
		Report("equip: corpus is readable", false, ItemsDir);
		return;
	}
	const FJsonValue* Cases = Root->Find("cases");
	if (!Cases || !Cases->IsArray() || Cases->ArrayItems.empty()) {
		Report("equip: corpus carries cases", false);
		return;
	}

	const FJsonValue* RingCase = nullptr;
	for (const FJsonValuePtr& CasePtr : Cases->ArrayItems) {
		if (!CasePtr || !CasePtr->IsObject()) {
			continue;
		}
		const FJsonValue& Case = *CasePtr;
		const std::string Name = Case.GetString("name");
		const FJsonValue* Input = Case.Find("input");
		const FJsonValue* Expected = Case.Find("expected");
		if (!Input || !Expected) {
			Report("equip: " + Name, false, "case is malformed");
			continue;
		}

		FItemLedger Ledger;
		FEquipQuery Query;
		ReadEquipCase(*Input, Ledger, Query);

		FInsimulEquipmentModel Model(&Ledger);
		const std::string Got = Model.ProjectionCanonical(Query);
		const std::string Want = CanonicalJsonStringify(*Expected);
		Report("equip: " + Name, Got == Want, Got == Want ? "" : ("got " + Got));

		if (Expected->Find("resolution")
			&& Expected->Find("resolution")->GetString("refusal") == "slot_full") {
			RingCase = Input;
		}
	}

	// ── negative controls ───────────────────────────────────────────────────
	// The corpus states the counterfactual in prose: "the same case under a world
	// that authored capacity: 3 is available". A port with a compiled slot table
	// cannot produce it, so the capacity is raised here and the refusal must lift.
	if (RingCase) {
		FItemLedger Ledger;
		FEquipQuery Query;
		ReadEquipCase(*RingCase, Ledger, Query);
		Report("control: the world's capacity, not core's, refuses the third ring",
			FInsimulEquipmentModel(&Ledger).Resolve(Query).Refusal == "slot_full");

		for (FEquipSlotRow& Slot : Ledger.Slots) {
			if (Slot.Id == "ring") {
				Slot.Capacity = 3;
			}
		}
		const FEquipResolution Raised = FInsimulEquipmentModel(&Ledger).Resolve(Query);
		Report("control: a world that authored capacity 3 lets the third ring on",
			Raised.bAvailable && Raised.Refusal.empty() && Raised.Capacity == 3);
	} else {
		Report("control: the corpus carries a slot_full case to invert", false);
	}

	{
		// A worn thing is still carried: retract the has_item half and the actor
		// gets lighter by wearing armour, which is the bug the corpus exists to catch.
		FItemLedger Ledger;
		Ledger.Tuning.CarryCapacity = 40;
		FCatalogueItem Plate;
		Plate.Id = "steel_cuirass";
		Plate.bHasEquipSlot = true;
		Plate.EquipSlot = "chest";
		Plate.Weight = 30;
		Plate.Armor = 6;
		Ledger.Catalogue.push_back(Plate);
		Ledger.Slots.push_back(FEquipSlotRow{"chest", "Chest", true, 1, 0});
		FItemStack Worn;
		Worn.Item = "steel_cuirass";
		Worn.Quantity = 1;
		Worn.Place.Kind = "equipped";
		Worn.Place.Holder = "wren";
		Worn.Place.Slot = "chest";
		Ledger.Stacks.push_back(Worn);

		const FEquipmentLoadout Load = FInsimulEquipmentModel(&Ledger).Loadout("wren");
		Report("control: a worn breastplate still weighs thirty", Load.Weight == 30);
		Report("control: a worn stack says both facts", Load.Facts.size() == 2
			&& Load.Facts[0] == "has_equipped(wren, chest, steel_cuirass)."
			&& Load.Facts[1] == "has_item(wren, steel_cuirass, 1).");
		Report("control: a detached equipment model answers empty, never crashes",
			FInsimulEquipmentModel().Loadout("wren").Worn.empty()
				&& FInsimulEquipmentModel().Resolve(FEquipQuery()).Refusal == "unknown");
	}
}

// ── 3. The state-location invariant, against a real SaveFile ────────────────

void RunStateBinding(const std::string& SavesDir) {
	std::printf("\n-- state location: the panels are backed only by save.currentState --\n");

	const std::string SaveText = ReadFile(SavesDir + "/v2-typical.json");
	if (SaveText.empty()) {
		Report("binding: the save fixture is readable", false, SavesDir);
		return;
	}

	FInsimulSaveSystem Save;
	std::string Error;
	if (!Save.Load(SaveText, Error)) {
		Report("binding: the save fixture loads", false, Error);
		return;
	}
	Report("binding: the save fixture loads and migrates", true);

	FJsonValue* Root = Save.MutableSaveFile();
	if (!Root) {
		Report("binding: the loaded save exposes its tree", false);
		return;
	}

	// A merchant and a container the fixture does not carry: the panels have to be
	// able to start from a save that has none, which is the normal case.
	{
		FTradeState Seed;
		if (!FInsimulUIStateBinding::HydrateTrade(*Root, Seed, Error)) {
			Report("binding: currentState hydrates a trade state", false, Error);
			return;
		}
		Report("binding: currentState hydrates a trade state", true);
		Report("binding: the player's gold came from the save", Seed.PlayerGold == 25,
			std::to_string(Seed.PlayerGold));
		Report("binding: the player's inventory came from the save",
			Seed.PlayerInventory.size() == 1 && Seed.PlayerInventory[0].ItemId == "item-bread"
				&& Seed.PlayerInventory[0].Quantity == 2);

		FTradeContainer Chest;
		Chest.Items.push_back(FTradeItem{"item-bread", 3, 5, true});
		Seed.Containers["chest1"] = Chest;
		FTradeMerchant Shop;
		Shop.GoldReserve = 100;
		Shop.Items.push_back(FTradeItem{"item-lamp", 2, 10, true});
		Seed.Merchants["shop1"] = Shop;
		if (!FInsimulUIStateBinding::ApplyTrade(Seed, *Root, Error)) {
			Report("binding: a seeded shop writes back into currentState", false, Error);
			return;
		}
		Report("binding: a seeded shop writes back into currentState", true);
	}

	const std::string OutsideBefore = FInsimulUIStateBinding::CanonicalOutsideCurrentState(*Root);

	FTradeState State;
	if (!FInsimulUIStateBinding::HydrateTrade(*Root, State, Error)) {
		Report("binding: the seeded state re-hydrates", false, Error);
		return;
	}
	const long long ItemsBefore = FInsimulUIStateBinding::ItemCensus(State);
	const long long GoldBefore = FInsimulUIStateBinding::GoldCensus(State);

	// The panels' own ops, run through the host-tested model over the save's slice.
	FInsimulTradeModel Trade(&State);
	const FTradeResult Took = Trade.TakeFromContainer("chest1", "item-bread", 2);
	const FTradeResult Bought = Trade.Buy("shop1", "item-lamp", 1);
	Report("binding: a container take succeeds against the save's slice",
		Took.bOk && Took.Moved == 2, Took.Reason);
	Report("binding: a merchant buy succeeds against the save's slice",
		Bought.bOk && Bought.Moved == 1, Bought.Reason);

	Report("binding: the item census is conserved across the ops",
		FInsimulUIStateBinding::ItemCensus(State) == ItemsBefore,
		std::to_string(FInsimulUIStateBinding::ItemCensus(State)) + " vs "
			+ std::to_string(ItemsBefore));
	Report("binding: the gold census is conserved across the ops",
		FInsimulUIStateBinding::GoldCensus(State) == GoldBefore);

	// The gate that can fail: nothing is in the save until it is written back.
	{
		FTradeState NotYet;
		FInsimulUIStateBinding::HydrateTrade(*Root, NotYet, Error);
		Report("control: an op that is never flushed leaves the save alone",
			NotYet.PlayerGold != State.PlayerGold
				|| NotYet.Containers["chest1"].Items.size()
					!= State.Containers["chest1"].Items.size());
	}

	if (!FInsimulUIStateBinding::ApplyTrade(State, *Root, Error)) {
		Report("binding: the ops write back into currentState", false, Error);
		return;
	}
	Report("binding: the ops write back into currentState", true);

	// 1. The change is in the bytes a save WRITE would emit.
	const std::string Canonical = Save.SerializeCanonical();
	Report("binding: the change is in the save's canonical bytes",
		Canonical.find("\"gold\":" + std::to_string(State.PlayerGold)) != std::string::npos,
		"gold " + std::to_string(State.PlayerGold));

	// 2. A re-hydrate reproduces it exactly — the model kept nothing back.
	{
		FTradeState Reloaded;
		FInsimulUIStateBinding::HydrateTrade(*Root, Reloaded, Error);
		bool bSame = Reloaded.PlayerGold == State.PlayerGold
			&& Reloaded.PlayerInventory.size() == State.PlayerInventory.size()
			&& FInsimulUIStateBinding::ItemCensus(Reloaded)
				== FInsimulUIStateBinding::ItemCensus(State)
			&& FInsimulUIStateBinding::GoldCensus(Reloaded)
				== FInsimulUIStateBinding::GoldCensus(State);
		Report("binding: a re-hydrate reproduces the mutated state exactly", bSame);
	}

	// 3. Nothing leaked sideways: everything outside currentState is untouched.
	Report("binding: everything outside currentState is byte-identical",
		FInsimulUIStateBinding::CanonicalOutsideCurrentState(*Root) == OutsideBefore);

	// 4. The integrity hash tracks the save, so a panel op is a save change.
	Report("binding: the integrity hash follows the panel op",
		Save.ComputeIntegrity() == CanonicalJsonIntegrity(*Root));

	// ── the journal, hydrated through the real quest system ─────────────────
	{
		std::vector<FQuestEntry> Entries;
		if (!FInsimulUIStateBinding::HydrateQuests(*Root, Entries, Error)) {
			Report("journal: currentState hydrates the quest set", false, Error);
			return;
		}
		Report("journal: currentState hydrates the quest set", Entries.size() == 1,
			std::to_string(Entries.size()) + " entries");
		if (Entries.empty()) {
			return;
		}
		Report("journal: the row's status is the playthrough's, not the snapshot's",
			Entries[0].Id == "quest-welcome" && Entries[0].Status == "active");
		Report("journal: the row carries the authored title",
			!Entries[0].Title.empty(), Entries[0].Title);

		FInsimulQuestJournalModel Journal;
		Journal.SetQuests(Entries);
		Report("journal: the model counts what the save carries",
			Journal.Counts().All == 1 && Journal.Counts().Active == 1);
		Report("journal: completing an active quest moves it",
			Journal.Complete("quest-welcome") && Journal.Counts().Completed == 1);

		std::vector<FQuestEntry> After = Journal.Filtered();
		Journal.SetFilter("all");
		After = Journal.Filtered();
		if (!FInsimulUIStateBinding::ApplyQuests(After, *Root, Error)) {
			Report("journal: the completion writes back into currentState", false, Error);
			return;
		}
		Report("journal: the completion writes back into currentState", true);

		std::vector<FQuestEntry> Reloaded;
		FInsimulUIStateBinding::HydrateQuests(*Root, Reloaded, Error);
		Report("journal: the save now reports the quest completed",
			Reloaded.size() == 1 && Reloaded[0].Status == "completed",
			Reloaded.empty() ? "" : Reloaded[0].Status);
		Report("journal: the completion did not leak outside currentState",
			FInsimulUIStateBinding::CanonicalOutsideCurrentState(*Root) == OutsideBefore);

		// A declined offer is a DELETION: the progress row leaves the save.
		FInsimulUIStateBinding::ApplyQuests({}, *Root, Error);
		std::vector<FQuestEntry> Empty;
		FInsimulUIStateBinding::HydrateQuests(*Root, Empty, Error);
		const std::string Bytes = Save.SerializeCanonical();
		Report("journal: a declined offer's progress row leaves the save",
			Bytes.find("\"progress\":{}") != std::string::npos);
	}

	// ── the shop panel prices out of the same one store ─────────────────────
	{
		// A merchant row that names a business, an owner and the faction it answers
		// to, and a reputation row for that faction: everything the price is a
		// function of already lives in currentState.
		FJsonValue* CurrentStateNode = nullptr;
		for (auto& Pair : Root->ObjectItems) {
			if (Pair.first == "currentState") {
				CurrentStateNode = Pair.second.get();
			}
		}
		if (!CurrentStateNode) {
			Report("market: the save exposes currentState", false);
			return;
		}

		FJsonParseResult Seeded = ParseJson(
			"{\"shop1\":{\"business\":\"ironmongers\",\"owner\":\"tomas\","
			"\"faction\":\"town_watch\",\"markupPercent\":20,\"goldReserve\":300,"
			"\"items\":[{\"itemId\":\"steel_sword\",\"quantity\":4,\"value\":100}]}}");
		FJsonParseResult Standing = ParseJson(
			"{\"town_watch\":{\"standing\":100}}");
		if (!Seeded.bOk || !Standing.bOk) {
			Report("market: the seed documents parse", false);
			return;
		}
		for (auto& Pair : CurrentStateNode->ObjectItems) {
			if (Pair.first == "npcs" && Pair.second) {
				for (auto& Inner : Pair.second->ObjectItems) {
					if (Inner.first == "merchantStates") {
						Inner.second = Seeded.Root;
					}
				}
			}
			if (Pair.first == "reputation" && Pair.second) {
				Pair.second->ObjectItems.emplace_back("factions", Standing.Root);
			}
		}

		FPriceMarket Market;
		if (!FInsimulUIStateBinding::HydrateMarket(*Root, "shop1", "steel_sword", Market, Error)) {
			Report("market: currentState hydrates the market", false, Error);
			return;
		}
		Report("market: the vendor's markup came from the save",
			Market.bPresent && Market.bHasMarkupPercent && Market.MarkupPercent == 20);
		Report("market: the player's standing came from currentState.reputation",
			Market.bHasStanding && Market.Standing == 100);

		FPriceItem Sword;
		Sword.bDeclared = true;
		Sword.Id = "steel_sword";
		Sword.Value = 100;
		Sword.SellValue = 50;
		const FPriceTuning Tuning;
		const FPriceQuote Friendly =
			FInsimulTradePricing::Quote(Sword, Market, Tuning, "brannoc", "buy", 1);
		Report("market: the shop panel's price is the corpus's reputation price",
			Friendly.Unit == 95, std::to_string(Friendly.Unit));

		// The control: the SAME sword in the SAME shop, priced for a stranger the
		// KB knows nothing about. A panel reading a price off item value cannot
		// tell these two players apart.
		FPriceMarket Stranger = Market;
		Stranger.bHasStanding = false;
		Report("control: a stranger pays the shop's own margin instead",
			FInsimulTradePricing::Quote(Sword, Stranger, Tuning, "wren", "buy", 1).Unit == 120);

		FPriceMarket Absent;
		std::string MarketError;
		Report("control: an unknown merchant is a fallback price, not an error",
			FInsimulUIStateBinding::HydrateMarket(*Root, "no-such-shop", "steel_sword",
				Absent, MarketError)
				&& !Absent.bPresent
				&& FInsimulTradePricing::Quote(Sword, Absent, Tuning, "wren", "buy", 1).bFallback);
	}

	// ── the equipment panel reads the same one store ────────────────────────
	{
		FItemLedger Ledger;
		if (!FInsimulUIStateBinding::HydrateLedger(*Root, "player", Ledger, Error)) {
			Report("loadout: currentState hydrates the item ledger", false, Error);
			return;
		}
		Report("loadout: currentState hydrates the item ledger",
			!Ledger.Stacks.empty(), std::to_string(Ledger.Stacks.size()) + " stacks");
		const FEquipmentLoadout Load = FInsimulEquipmentModel(&Ledger).Loadout("player");
		Report("loadout: a save with no equipment slice wears nothing",
			Load.Worn.empty() && Load.Armor == 0);
		Report("loadout: the inventory stacks are said in the shared vocabulary",
			!Load.Facts.empty() && Load.Facts[0].rfind("container_contains(", 0) == 0);
	}

	// ── controls on the binding itself ──────────────────────────────────────
	{
		FJsonValue Bare;
		Bare.Type = EJsonType::Object;
		FTradeState Dummy;
		std::vector<FQuestEntry> DummyQuests;
		FItemLedger DummyLedger;
		std::string Err1, Err2, Err3, Err4;
		const bool bRefused = !FInsimulUIStateBinding::HydrateTrade(Bare, Dummy, Err1)
			&& !FInsimulUIStateBinding::HydrateQuests(Bare, DummyQuests, Err2)
			&& !FInsimulUIStateBinding::HydrateLedger(Bare, "player", DummyLedger, Err3)
			&& !FInsimulUIStateBinding::ApplyTrade(Dummy, Bare, Err4);
		Report("control: a document with no currentState is refused, not defaulted",
			bRefused && !Err1.empty() && !Err4.empty());
	}
}

} // namespace

int main(int argc, char** argv) {
	std::string ItemsDir =
#ifdef INSIMUL_ITEMS_DIR
		INSIMUL_ITEMS_DIR;
#else
		"../../conformance/items";
#endif
	std::string SavesDir =
#ifdef INSIMUL_FIXTURE_DIR
		INSIMUL_FIXTURE_DIR;
#else
		"../../conformance/saves";
#endif
	if (argc > 1) {
		ItemsDir = argv[1];
	}
	if (argc > 2) {
		SavesDir = argv[2];
	}

	std::printf("default-UI panels backed by save.currentState (tasklist 190 US-2)\n");
	std::printf("items corpus: %s\nsaves corpus: %s\n", ItemsDir.c_str(), SavesDir.c_str());

	RunPricingCorpus(ItemsDir);
	RunEquippingCorpus(ItemsDir);
	RunStateBinding(SavesDir);

	std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
