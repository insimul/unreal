// Copyright 2024 Insimul. All Rights Reserved.
//
// FInsimulUIStateBinding — the join that makes "backed EXCLUSIVELY by
// save.currentState" a fact rather than a comment (US-2 of tasklist 190).
//
// THE INVARIANT. Playthrough data lives in the save file and nowhere else
// (platform/CLAUDE.md). The quest and trade view-models already keep no private
// store — FInsimulTradeModel holds a POINTER to the caller's state and
// FInsimulQuestJournalModel is a projection — but until this file the state they
// were pointed at was whatever a caller happened to build, so a panel that quietly
// grew a second store would have failed no gate. This is the one place the panels'
// slices are read out of a real SaveFile and written back into it:
//
//   player.gold / player.inventory        -> the inventory panel
//   containers.containers[id].items       -> the container panel
//   npcs.merchantStates[id]               -> the merchant panel
//   quests.progress / quests.dynamicQuests-> the journal / tracker / offer panels
//
// and the one slice that is NOT under currentState, because the save schema has
// never put it there (US-3 of tasklist 190):
//
//   conversations[npcCharacterId]         -> the dialogue panel's history
//
// AND THE TITLES COME FROM THE QUEST SYSTEM. A journal row is not the save's
// `name` field: the quest's Prolog `content` is the single source of truth
// (InsimulQuestSystem.h), so hydration produces the title, the difficulty and the
// authored status, and currentState.quests.progress overrides the status with what
// this playthrough did. A port that read the snapshot's `name` would show a title
// the KB does not agree with the moment content and snapshot drift.
//
// WHAT IS DELIBERATELY NOT HERE. There is no equipment WRITE path. Wearing a thing
// is the items module's decision layer (band 124), and inventing a
// `currentState.player.equipped` schema in an engine port would be this leg
// disagreeing with the other three about what a save contains. HydrateLedger reads
// the items slice when a world's save carries one and yields an empty loadout when
// it does not — a read that tolerates absence, never a write that invents a field.
//
// WHAT THE GATE PROVES (ctest `ui_state_binding`). After any panel op:
//   * the change is in the save's canonical bytes (so it survives a serialize),
//   * a re-hydrate reproduces the mutated state exactly (no data left in the model),
//   * everything OUTSIDE currentState is byte-identical (nothing leaked sideways),
//   * the item + gold census is conserved (stacks moved, none created).
//
// std-only (no Unreal Engine, no CoreMinimal.h).

#pragma once

#include "InsimulChatModel.h"
#include "InsimulEquipmentModel.h"
#include "InsimulJson.h"
#include "InsimulQuestJournalModel.h"
#include "InsimulTradeModel.h"
#include "InsimulTradePricing.h"

#include <string>
#include <vector>

namespace insimul {

class FInsimulUIStateBinding {
public:
	// ── Trade (inventory / container / merchant) ─────────────────────────────

	/**
	 * Read the trade slice out of a loaded SaveFile. Returns false with OutError
	 * when the document has no currentState — a build whose save is not one must
	 * say so rather than show an empty, plausible-looking inventory.
	 */
	static bool HydrateTrade(const FJsonValue& SaveRoot, FTradeState& Out, std::string& OutError);

	/**
	 * Write the trade slice back into currentState IN PLACE, touching nothing else.
	 * Unrelated fields of a merchant / container row are preserved.
	 */
	static bool ApplyTrade(const FTradeState& In, FJsonValue& SaveRoot, std::string& OutError);

	// ── Quests (journal / tracker / offer) ───────────────────────────────────

	/**
	 * Project the playthrough's quests: the world snapshot's authored rows hydrated
	 * through the quest system, with currentState.quests.progress deciding status,
	 * then the radiant arrivals in currentState.quests.dynamicQuests (bIsRadiant).
	 */
	static bool HydrateQuests(const FJsonValue& SaveRoot, std::vector<FQuestEntry>& Out,
		std::string& OutError);

	/**
	 * Write journal state back: a status per quest into quests.progress (other
	 * fields of a progress row are preserved), the radiant set into dynamicQuests,
	 * and a declined offer's row REMOVED — declining is a deletion, not a status.
	 */
	static bool ApplyQuests(const std::vector<FQuestEntry>& In, FJsonValue& SaveRoot,
		std::string& OutError);

	/**
	 * Build the MARKET a shop panel prices in, out of the same one store: the
	 * merchant's own row (its business, its owner, the faction it answers to, its
	 * markup and what is on its shelf) joined to the player's STANDING with that
	 * faction, read from currentState.reputation. A merchant row that names no
	 * faction simply has no standing term — the price is then the same for
	 * everyone, which is the correct answer for a world that tracks no reputation.
	 *
	 * `ItemId` selects the shelf count the scarcity term reads; pass empty for a
	 * market with no particular item in view.
	 */
	static bool HydrateMarket(const FJsonValue& SaveRoot, const std::string& MerchantId,
		const std::string& ItemId, FPriceMarket& Out, std::string& OutError);

	// ── Equipment (read-only) ────────────────────────────────────────────────

	/**
	 * Build the item ledger the equipment panel reads: inventory stacks from
	 * player.inventory, container stacks from containers.containers, the worn set
	 * and the world's slot table / catalogue from the items slice when the save
	 * carries one. An absent items slice is an empty loadout, not an error.
	 */
	static bool HydrateLedger(const FJsonValue& SaveRoot, const std::string& Actor,
		FItemLedger& Out, std::string& OutError);

	// ── Conversations (the dialogue panel's history) ─────────────────────────
	//
	// The ONE exception to "the panels read currentState": a conversation is not
	// playthrough STATE, it is the transcript, and the save schema has carried it
	// as its own top-level `conversations` array (ConversationSummary rows) since
	// v1. So the dialogue panel persists there and nowhere else — a port that
	// invented `currentState.dialogue` would be this leg disagreeing with the
	// other three about what a save contains, which is the failure the state
	// invariant exists to stop.

	/**
	 * Read the persisted turns for one character out of save.conversations. A save
	 * that has never spoken to this character yields an EMPTY history rather than
	 * an error: absence is the normal first-meeting case. A `conversations` that is
	 * present and is not an array IS an error — that document is not a save.
	 */
	static bool HydrateConversation(const FJsonValue& SaveRoot, const std::string& CharacterId,
		FChatHistory& Out, std::string& OutError);

	/**
	 * Write a panel's history back into save.conversations IN PLACE. The row for
	 * CharacterId takes `recentTurns` + `totalTurnCount` (and `npcCharacterName`
	 * when one is given); every OTHER field of that row — the compressed history,
	 * the last location, the topics a language world tracks — is preserved, as is
	 * every other row. A character the save has no row for gets one appended.
	 * currentState is never touched.
	 *
	 * A turn's `timestamp` is written only when the caller stamped one: the clock
	 * belongs to the host, and a port that minted its own would produce saves that
	 * differ from the other three legs' byte for byte.
	 */
	static bool ApplyConversation(const std::string& CharacterId, const std::string& CharacterName,
		const FChatHistory& History, FJsonValue& SaveRoot, std::string& OutError);

	// ── The invariant's instrument ───────────────────────────────────────────

	/**
	 * Canonical JSON of the save with currentState removed — everything a panel op
	 * must leave byte-identical. The test diffs this before and after every op.
	 */
	static std::string CanonicalOutsideCurrentState(const FJsonValue& SaveRoot);

	/**
	 * Canonical JSON of the save with `conversations` removed — everything a
	 * dialogue write must leave byte-identical, currentState included. The gate
	 * diffs this before and after a history flush.
	 */
	static std::string CanonicalOutsideConversations(const FJsonValue& SaveRoot);

	/** Total item count across player, every container and every merchant. */
	static long long ItemCensus(const FTradeState& State);

	/** Player gold plus every merchant's reserve. */
	static long long GoldCensus(const FTradeState& State);
};

} // namespace insimul
