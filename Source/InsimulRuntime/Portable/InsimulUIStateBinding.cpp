// Copyright 2024 Insimul. All Rights Reserved.

#include "InsimulUIStateBinding.h"

#include "InsimulCanonicalJson.h"
#include "InsimulQuestSystem.h"

#include <algorithm>
#include <memory>

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

FJsonValuePtr MakeInt(long long N) {
	auto Node = std::make_shared<FJsonValue>();
	Node->Type = EJsonType::Number;
	Node->NumberValue = static_cast<double>(N);
	Node->RawNumber = std::to_string(N);
	return Node;
}

FJsonValue* ObjFind(FJsonValue& Obj, const std::string& Key) {
	for (auto& Pair : Obj.ObjectItems) {
		if (Pair.first == Key) {
			return Pair.second.get();
		}
	}
	return nullptr;
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

void ObjRemove(FJsonValue& Obj, const std::string& Key) {
	for (auto It = Obj.ObjectItems.begin(); It != Obj.ObjectItems.end(); ++It) {
		if (It->first == Key) {
			Obj.ObjectItems.erase(It);
			return;
		}
	}
}

/** Find `Key`, creating an empty object there when it is absent or not one. */
FJsonValue& ObjEnsureObject(FJsonValue& Obj, const std::string& Key) {
	FJsonValue* Found = ObjFind(Obj, Key);
	if (!Found || !Found->IsObject()) {
		ObjSet(Obj, Key, MakeObject());
		Found = ObjFind(Obj, Key);
	}
	return *Found;
}

const FJsonValue* CurrentState(const FJsonValue& SaveRoot) {
	const FJsonValue* State = SaveRoot.Find("currentState");
	return (State && State->IsObject()) ? State : nullptr;
}

// ── Item stacks ─────────────────────────────────────────────────────────────

FTradeItem ReadItem(const FJsonValue& Row) {
	FTradeItem Out;
	Out.ItemId = Row.GetString("itemId");
	Out.Quantity = Row.GetInt("quantity", 0);
	if (const FJsonValue* Value = Row.Find("value")) {
		if (Value->IsNumber()) {
			Out.bHasValue = true;
			Out.Value = Value->AsInt(0);
		}
	}
	return Out;
}

void ReadItems(const FJsonValue* Array, std::vector<FTradeItem>& Out) {
	Out.clear();
	if (!Array || !Array->IsArray()) {
		return;
	}
	for (const FJsonValuePtr& Row : Array->ArrayItems) {
		if (Row && Row->IsObject()) {
			Out.push_back(ReadItem(*Row));
		}
	}
}

FJsonValuePtr WriteItems(const std::vector<FTradeItem>& Items) {
	auto Out = MakeArray();
	for (const FTradeItem& Item : Items) {
		auto Row = MakeObject();
		ObjSet(*Row, "itemId", MakeString(Item.ItemId));
		ObjSet(*Row, "quantity", MakeInt(Item.Quantity));
		if (Item.bHasValue) {
			ObjSet(*Row, "value", MakeInt(Item.Value));
		}
		Out->ArrayItems.push_back(Row);
	}
	return Out;
}

/** Map keys in ascending order — an unordered_map must not decide save bytes. */
template <typename MapType>
std::vector<std::string> SortedKeys(const MapType& Map) {
	std::vector<std::string> Keys;
	Keys.reserve(Map.size());
	for (const auto& Pair : Map) {
		Keys.push_back(Pair.first);
	}
	std::sort(Keys.begin(), Keys.end());
	return Keys;
}

} // namespace

// ── Trade ───────────────────────────────────────────────────────────────────

bool FInsimulUIStateBinding::HydrateTrade(const FJsonValue& SaveRoot, FTradeState& Out,
	std::string& OutError) {
	const FJsonValue* State = CurrentState(SaveRoot);
	if (!State) {
		OutError = "SaveFile has no currentState object — nothing to back the panels with";
		return false;
	}

	Out = FTradeState();

	if (const FJsonValue* Player = State->Find("player")) {
		if (Player->IsObject()) {
			Out.PlayerGold = Player->GetInt("gold", 0);
			ReadItems(Player->Find("inventory"), Out.PlayerInventory);
		}
	}

	if (const FJsonValue* Containers = State->Find("containers")) {
		if (const FJsonValue* Rows = Containers->Find("containers")) {
			if (Rows->IsObject()) {
				for (const auto& Pair : Rows->ObjectItems) {
					if (!Pair.second || !Pair.second->IsObject()) {
						continue;
					}
					FTradeContainer Container;
					ReadItems(Pair.second->Find("items"), Container.Items);
					Out.Containers[Pair.first] = Container;
				}
			}
		}
	}

	if (const FJsonValue* Npcs = State->Find("npcs")) {
		if (const FJsonValue* Rows = Npcs->Find("merchantStates")) {
			if (Rows->IsObject()) {
				for (const auto& Pair : Rows->ObjectItems) {
					if (!Pair.second || !Pair.second->IsObject()) {
						continue;
					}
					FTradeMerchant Merchant;
					Merchant.GoldReserve = Pair.second->GetInt("goldReserve", 0);
					ReadItems(Pair.second->Find("items"), Merchant.Items);
					Out.Merchants[Pair.first] = Merchant;
				}
			}
		}
	}
	return true;
}

bool FInsimulUIStateBinding::ApplyTrade(const FTradeState& In, FJsonValue& SaveRoot,
	std::string& OutError) {
	FJsonValue* State = ObjFind(SaveRoot, "currentState");
	if (!State || !State->IsObject()) {
		OutError = "SaveFile has no currentState object — refusing to invent one";
		return false;
	}

	FJsonValue& Player = ObjEnsureObject(*State, "player");
	ObjSet(Player, "gold", MakeInt(In.PlayerGold));
	ObjSet(Player, "inventory", WriteItems(In.PlayerInventory));

	// The container / merchant ROWS are updated in place, so a row's other fields
	// (a lock state, a restock clock) survive a trade this panel knows nothing about.
	FJsonValue& Containers = ObjEnsureObject(ObjEnsureObject(*State, "containers"), "containers");
	for (const std::string& Key : SortedKeys(In.Containers)) {
		FJsonValue& Row = ObjEnsureObject(Containers, Key);
		ObjSet(Row, "items", WriteItems(In.Containers.at(Key).Items));
	}

	FJsonValue& Merchants = ObjEnsureObject(ObjEnsureObject(*State, "npcs"), "merchantStates");
	for (const std::string& Key : SortedKeys(In.Merchants)) {
		const FTradeMerchant& Merchant = In.Merchants.at(Key);
		FJsonValue& Row = ObjEnsureObject(Merchants, Key);
		ObjSet(Row, "goldReserve", MakeInt(Merchant.GoldReserve));
		ObjSet(Row, "items", WriteItems(Merchant.Items));
	}
	return true;
}

// ── Quests ──────────────────────────────────────────────────────────────────

bool FInsimulUIStateBinding::HydrateQuests(const FJsonValue& SaveRoot,
	std::vector<FQuestEntry>& Out, std::string& OutError) {
	const FJsonValue* State = CurrentState(SaveRoot);
	if (!State) {
		OutError = "SaveFile has no currentState object — nothing to back the journal with";
		return false;
	}
	Out.clear();

	const FJsonValue* Progress = nullptr;
	const FJsonValue* Dynamic = nullptr;
	if (const FJsonValue* Quests = State->Find("quests")) {
		Progress = Quests->Find("progress");
		Dynamic = Quests->Find("dynamicQuests");
	}

	// One reader for both sources: an authored row and a radiant arrival are the
	// same kind of thing, and only `bIsRadiant` tells the offer panel them apart.
	auto ReadQuest = [&](const FJsonValue& Row, bool bRadiant) {
		FQuestEntry Entry;
		Entry.Id = Row.GetString("id");
		if (Entry.Id.empty()) {
			return;
		}
		Entry.bIsRadiant = bRadiant;

		// This playthrough's status wins over the authored one; that is the whole
		// difference between the snapshot (what the world shipped) and currentState
		// (what this player did).
		std::string Status = Row.GetString("status");
		if (Progress && Progress->IsObject()) {
			if (const FJsonValue* Row2 = Progress->Find(Entry.Id)) {
				const std::string Live = Row2->GetString("status");
				if (!Live.empty()) {
					Status = Live;
				}
			}
		}

		// The Prolog content is the single source of truth for the structured
		// fields (InsimulQuestSystem.h); the snapshot's own name/description are the
		// fallback for a row that authored no content.
		const std::string Content = Row.GetString("content");
		if (!Content.empty()) {
			const FHydratedQuest Hydrated = FInsimulQuestSystem::HydrateFromContent(Content, Status);
			if (Hydrated.bHasTitle) {
				Entry.Title = Hydrated.Title;
			}
			if (Hydrated.bHasDifficulty) {
				Entry.Difficulty = Hydrated.Difficulty;
			}
			if (Hydrated.bHasStatus) {
				Status = Hydrated.Status;
			}
		}
		if (Entry.Title.empty()) {
			Entry.Title = Row.GetString("name");
		}
		if (Entry.Difficulty.empty()) {
			Entry.Difficulty = Row.GetString("difficulty");
		}
		Entry.Description = Row.GetString("description");
		Entry.Status = Status.empty() ? "available" : Status;
		Out.push_back(Entry);
	};

	if (const FJsonValue* Snapshot = SaveRoot.Find("worldSnapshot")) {
		if (const FJsonValue* Quests = Snapshot->Find("quests")) {
			if (Quests->IsArray()) {
				for (const FJsonValuePtr& Row : Quests->ArrayItems) {
					if (Row && Row->IsObject()) {
						ReadQuest(*Row, false);
					}
				}
			}
		}
	}

	if (Dynamic && Dynamic->IsArray()) {
		for (const FJsonValuePtr& Row : Dynamic->ArrayItems) {
			if (Row && Row->IsObject()) {
				ReadQuest(*Row, true);
			}
		}
	}
	return true;
}

bool FInsimulUIStateBinding::ApplyQuests(const std::vector<FQuestEntry>& In, FJsonValue& SaveRoot,
	std::string& OutError) {
	FJsonValue* State = ObjFind(SaveRoot, "currentState");
	if (!State || !State->IsObject()) {
		OutError = "SaveFile has no currentState object — refusing to invent one";
		return false;
	}

	FJsonValue& Quests = ObjEnsureObject(*State, "quests");
	FJsonValue& Progress = ObjEnsureObject(Quests, "progress");

	// A progress ROW is updated in place: currentStageIndex, stageData and
	// completedAt belong to the quest system, not to the journal panel.
	for (const FQuestEntry& Entry : In) {
		FJsonValue& Row = ObjEnsureObject(Progress, Entry.Id);
		ObjSet(Row, "status", MakeString(Entry.Status));
	}

	// A declined offer is a DELETION — the row leaves the save, which is why the
	// journal cannot express "declined" as a status.
	std::vector<std::string> Stale;
	for (const auto& Pair : Progress.ObjectItems) {
		const bool bLive = std::any_of(In.begin(), In.end(),
			[&](const FQuestEntry& Entry) { return Entry.Id == Pair.first; });
		if (!bLive) {
			Stale.push_back(Pair.first);
		}
	}
	for (const std::string& Key : Stale) {
		ObjRemove(Progress, Key);
	}

	auto Radiant = MakeArray();
	for (const FQuestEntry& Entry : In) {
		if (!Entry.bIsRadiant) {
			continue;
		}
		auto Row = MakeObject();
		ObjSet(*Row, "id", MakeString(Entry.Id));
		ObjSet(*Row, "name", MakeString(Entry.Title));
		ObjSet(*Row, "description", MakeString(Entry.Description));
		ObjSet(*Row, "difficulty", MakeString(Entry.Difficulty));
		ObjSet(*Row, "status", MakeString(Entry.Status));
		Radiant->ArrayItems.push_back(Row);
	}
	ObjSet(Quests, "dynamicQuests", Radiant);
	return true;
}

// ── The market a shop panel prices in ───────────────────────────────────────

bool FInsimulUIStateBinding::HydrateMarket(const FJsonValue& SaveRoot, const std::string& MerchantId,
	const std::string& ItemId, FPriceMarket& Out, std::string& OutError) {
	const FJsonValue* State = CurrentState(SaveRoot);
	if (!State) {
		OutError = "SaveFile has no currentState object — nothing to price against";
		return false;
	}

	Out = FPriceMarket();

	const FJsonValue* Merchant = nullptr;
	if (const FJsonValue* Npcs = State->Find("npcs")) {
		if (const FJsonValue* Rows = Npcs->Find("merchantStates")) {
			if (Rows->IsObject()) {
				Merchant = Rows->Find(MerchantId);
			}
		}
	}
	if (!Merchant || !Merchant->IsObject()) {
		// Not an error: a world with no economy prices an item at its own value,
		// and `fallback` on the quote says so.
		return true;
	}

	Out.bPresent = true;
	Out.VendorId = MerchantId;
	Out.Business = Merchant->GetString("business");
	Out.Owner = Merchant->GetString("owner");
	Out.Faction = Merchant->GetString("faction");
	if (const FJsonValue* Markup = Merchant->Find("markupPercent")) {
		if (Markup->IsNumber()) {
			Out.bHasMarkupPercent = true;
			Out.MarkupPercent = Markup->AsInt(0);
		}
	}

	// The shelf IS the ledger's container place: the scarcity term reads what is
	// actually stocked rather than a shop table, so a purchase that empties it
	// moves the next price by construction.
	if (!ItemId.empty()) {
		if (const FJsonValue* Items = Merchant->Find("items")) {
			if (Items->IsArray()) {
				for (const FJsonValuePtr& Row : Items->ArrayItems) {
					if (!Row || !Row->IsObject() || Row->GetString("itemId") != ItemId) {
						continue;
					}
					Out.bHasStock = true;
					Out.Stock = Row->GetInt("quantity", 0);
					if (const FJsonValue* Normal = Row->Find("stockNormal")) {
						if (Normal->IsNumber()) {
							Out.bHasStockNormal = true;
							Out.StockNormal = Normal->AsInt(0);
						}
					}
					break;
				}
			}
		}
	}

	// Standing with the faction the shop answers to. Factions first, then the
	// settlement ledger a language-learning save keeps — one lookup, two shapes,
	// because reputation is one idea however a world happens to file it.
	if (!Out.Faction.empty()) {
		if (const FJsonValue* Reputation = State->Find("reputation")) {
			const char* Sections[] = {"factions", "settlements"};
			for (const char* Section : Sections) {
				const FJsonValue* Rows = Reputation->Find(Section);
				if (!Rows || !Rows->IsObject()) {
					continue;
				}
				const FJsonValue* Row = Rows->Find(Out.Faction);
				if (!Row || !Row->IsObject()) {
					continue;
				}
				if (const FJsonValue* Standing = Row->Find("standing")) {
					if (Standing->IsNumber()) {
						Out.bHasStanding = true;
						Out.Standing = Standing->AsInt(0);
						break;
					}
				}
			}
		}
	}
	return true;
}

// ── Equipment (read-only) ───────────────────────────────────────────────────

bool FInsimulUIStateBinding::HydrateLedger(const FJsonValue& SaveRoot, const std::string& Actor,
	FItemLedger& Out, std::string& OutError) {
	const FJsonValue* State = CurrentState(SaveRoot);
	if (!State) {
		OutError = "SaveFile has no currentState object — nothing to back the loadout with";
		return false;
	}

	Out = FItemLedger();

	if (const FJsonValue* Player = State->Find("player")) {
		if (const FJsonValue* Inventory = Player->Find("inventory")) {
			if (Inventory->IsArray()) {
				for (const FJsonValuePtr& Row : Inventory->ArrayItems) {
					if (!Row || !Row->IsObject()) {
						continue;
					}
					FItemStack Stack;
					Stack.Item = Row->GetString("itemId");
					Stack.Quantity = Row->GetInt("quantity", 0);
					Stack.Place.Kind = "inventory";
					Stack.Place.Holder = Actor;
					Out.Stacks.push_back(Stack);
				}
			}
		}
		// The worn set, when the world's items slice carries one. Absent is an empty
		// loadout, never an error: a v3 core save has no equipment schema and a panel
		// that invented one would be this leg disagreeing with the other three.
		if (const FJsonValue* Equipped = Player->Find("equipped")) {
			if (Equipped->IsArray()) {
				for (const FJsonValuePtr& Row : Equipped->ArrayItems) {
					if (!Row || !Row->IsObject()) {
						continue;
					}
					FItemStack Stack;
					Stack.Item = Row->GetString("itemId");
					Stack.Quantity = Row->GetInt("quantity", 1);
					Stack.Place.Kind = "equipped";
					Stack.Place.Holder = Actor;
					Stack.Place.Slot = Row->GetString("slot");
					Out.Stacks.push_back(Stack);
				}
			}
		}
	}

	if (const FJsonValue* Containers = State->Find("containers")) {
		if (const FJsonValue* Rows = Containers->Find("containers")) {
			if (Rows->IsObject()) {
				for (const auto& Pair : Rows->ObjectItems) {
					if (!Pair.second || !Pair.second->IsObject()) {
						continue;
					}
					const FJsonValue* Items = Pair.second->Find("items");
					if (!Items || !Items->IsArray()) {
						continue;
					}
					for (const FJsonValuePtr& Row : Items->ArrayItems) {
						if (!Row || !Row->IsObject()) {
							continue;
						}
						FItemStack Stack;
						Stack.Item = Row->GetString("itemId");
						Stack.Quantity = Row->GetInt("quantity", 0);
						Stack.Place.Kind = "container";
						Stack.Place.Holder = Pair.first;
						Out.Stacks.push_back(Stack);
					}
				}
			}
		}
	}

	// The world's own slot table + catalogue, from the items IR section when the
	// snapshot carries one. There is no compiled slot enum to fall back to.
	if (const FJsonValue* Snapshot = SaveRoot.Find("worldSnapshot")) {
		if (const FJsonValue* Items = Snapshot->Find("items")) {
			if (const FJsonValue* Slots = Items->Find("slots")) {
				if (Slots->IsArray()) {
					long long Order = 0;
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
						Slot.Order = Row->Find("order") ? Row->GetInt("order", Order) : Order;
						Out.Slots.push_back(Slot);
						++Order;
					}
				}
			}
			if (const FJsonValue* Catalogue = Items->Find("catalogue")) {
				if (Catalogue->IsArray()) {
					for (const FJsonValuePtr& Row : Catalogue->ArrayItems) {
						if (!Row || !Row->IsObject()) {
							continue;
						}
						FCatalogueItem Item;
						Item.Id = Row->GetString("id");
						const std::string Slot = Row->GetString("equipSlot");
						if (!Slot.empty()) {
							Item.bHasEquipSlot = true;
							Item.EquipSlot = Slot;
						}
						Item.Weight = Row->GetInt("weight", 0);
						Item.Armor = Row->GetInt("armor", 0);
						Out.Catalogue.push_back(Item);
					}
				}
			}
			if (const FJsonValue* Tuning = Items->Find("tuning")) {
				Out.Tuning = FEquipTuning::FromJson(*Tuning);
			}
		}
	}
	return true;
}

// ── The invariant's instrument ──────────────────────────────────────────────

// ── Conversations (the dialogue panel's history) ────────────────────────────

namespace {

/** The conversations array, or nullptr when the save carries none. Sets OutError
 *  only when the member is present and is NOT an array. */
const FJsonValue* FindConversations(const FJsonValue& SaveRoot, std::string& OutError) {
	const FJsonValue* Rows = SaveRoot.Find("conversations");
	if (!Rows) {
		return nullptr;
	}
	if (!Rows->IsArray()) {
		OutError = "save.conversations is present and is not an array";
		return nullptr;
	}
	return Rows;
}

} // namespace

bool FInsimulUIStateBinding::HydrateConversation(const FJsonValue& SaveRoot,
	const std::string& CharacterId, FChatHistory& Out, std::string& OutError) {
	Out = FChatHistory();
	OutError.clear();

	if (!SaveRoot.IsObject()) {
		OutError = "the document is not a save file (no object root)";
		return false;
	}
	if (CharacterId.empty()) {
		OutError = "a conversation is read for a character; no id was given";
		return false;
	}

	const FJsonValue* Rows = FindConversations(SaveRoot, OutError);
	if (!OutError.empty()) {
		return false;
	}
	if (!Rows) {
		// A save that has spoken to nobody. Not an error — the first meeting.
		return true;
	}

	for (const FJsonValuePtr& Row : Rows->ArrayItems) {
		if (!Row || !Row->IsObject() || Row->GetString("npcCharacterId") != CharacterId) {
			continue;
		}
		const FJsonValue* Turns = Row->Find("recentTurns");
		if (Turns && Turns->IsArray()) {
			for (const FJsonValuePtr& Turn : Turns->ArrayItems) {
				if (!Turn || !Turn->IsObject()) {
					continue;
				}
				FChatHistoryTurn Out1;
				Out1.Role = Turn->GetString("role");
				Out1.Content = Turn->GetString("content");
				Out1.Timestamp = Turn->GetString("timestamp");
				Out.RecentTurns.push_back(Out1);
			}
		}
		Out.TotalTurnCount = Row->GetInt("totalTurnCount", 0);
		return true;
	}
	// No row for this character: an empty history, which is what a panel opening on
	// a stranger must show.
	return true;
}

bool FInsimulUIStateBinding::ApplyConversation(const std::string& CharacterId,
	const std::string& CharacterName, const FChatHistory& History, FJsonValue& SaveRoot,
	std::string& OutError) {
	OutError.clear();

	if (!SaveRoot.IsObject()) {
		OutError = "the document is not a save file (no object root)";
		return false;
	}
	if (CharacterId.empty()) {
		OutError = "a conversation is written for a character; no id was given";
		return false;
	}
	{
		std::string TypeError;
		FindConversations(SaveRoot, TypeError);
		if (!TypeError.empty()) {
			// Refuse rather than clobber: whatever is under that key, it is not the
			// history array this seam owns.
			OutError = TypeError;
			return false;
		}
	}

	FJsonValue* Rows = ObjFind(SaveRoot, "conversations");
	if (!Rows) {
		ObjSet(SaveRoot, "conversations", MakeArray());
		Rows = ObjFind(SaveRoot, "conversations");
	}

	auto Turns = MakeArray();
	for (const FChatHistoryTurn& Turn : History.RecentTurns) {
		auto Node = MakeObject();
		ObjSet(*Node, "role", MakeString(Turn.Role));
		ObjSet(*Node, "content", MakeString(Turn.Content));
		// The clock is the host's. An unstamped turn writes no timestamp rather
		// than a minted one (see the header).
		if (!Turn.Timestamp.empty()) {
			ObjSet(*Node, "timestamp", MakeString(Turn.Timestamp));
		}
		Turns->ArrayItems.push_back(std::move(Node));
	}

	// The existing row, if this playthrough has met the character before. Every
	// field this seam does not own stays exactly as it was.
	FJsonValue* Row = nullptr;
	for (const FJsonValuePtr& Candidate : Rows->ArrayItems) {
		if (Candidate && Candidate->IsObject()
			&& Candidate->GetString("npcCharacterId") == CharacterId) {
			Row = Candidate.get();
			break;
		}
	}
	if (!Row) {
		auto Fresh = MakeObject();
		ObjSet(*Fresh, "npcCharacterId", MakeString(CharacterId));
		Rows->ArrayItems.push_back(Fresh);
		Row = Rows->ArrayItems.back().get();
	}

	if (!CharacterName.empty()) {
		ObjSet(*Row, "npcCharacterName", MakeString(CharacterName));
	}
	ObjSet(*Row, "recentTurns", std::move(Turns));
	ObjSet(*Row, "totalTurnCount", MakeInt(History.TotalTurnCount));
	return true;
}

std::string FInsimulUIStateBinding::CanonicalOutsideConversations(const FJsonValue& SaveRoot) {
	FJsonValue Copy = SaveRoot;
	ObjRemove(Copy, "conversations");
	return CanonicalJsonStringify(Copy);
}

std::string FInsimulUIStateBinding::CanonicalOutsideCurrentState(const FJsonValue& SaveRoot) {
	FJsonValue Copy = SaveRoot;
	ObjRemove(Copy, "currentState");
	return CanonicalJsonStringify(Copy);
}

long long FInsimulUIStateBinding::ItemCensus(const FTradeState& State) {
	long long Total = 0;
	for (const FTradeItem& Item : State.PlayerInventory) {
		Total += Item.Quantity;
	}
	for (const auto& Pair : State.Containers) {
		for (const FTradeItem& Item : Pair.second.Items) {
			Total += Item.Quantity;
		}
	}
	for (const auto& Pair : State.Merchants) {
		for (const FTradeItem& Item : Pair.second.Items) {
			Total += Item.Quantity;
		}
	}
	return Total;
}

long long FInsimulUIStateBinding::GoldCensus(const FTradeState& State) {
	long long Total = State.PlayerGold;
	for (const auto& Pair : State.Merchants) {
		Total += Pair.second.GoldReserve;
	}
	return Total;
}

} // namespace insimul
