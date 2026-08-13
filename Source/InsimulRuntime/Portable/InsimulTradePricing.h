// Copyright 2024 Insimul. All Rights Reserved.
//
// FInsimulTradePricing — what a thing costs HERE, from whom, and why (US-2 of
// tasklist 190; the shop + REPUTATION half of the default-UI trade suite).
//
// WHY THE SHOP PANEL NEEDS THIS AT ALL. FInsimulTradeModel (InsimulTradeModel.h)
// moves stacks and gold: it is the ledger, and it prices a stack at the `value` the
// stack carries. That is enough for a container and for a merchant in a world with
// no economy, and it is NOT enough for a shop panel, because a shop panel has to be
// able to answer the player's question — why am I being charged 95 for a sword the
// next town sells at 120. A price in Insimul is a function of the simulation that
// already exists rather than a row in a shop table: the vendor's business markup,
// the stock on the vendor's own shelf against what the item normally sits at, the
// player's STANDING with the faction the shop answers to, and whether the player
// owns the place. Every term is reported with its percentage and the coin it moved,
// so the panel can show the reason next to the number.
//
// THE SEMANTICS ARE THE CORPUS'S, NOT THIS FILE'S. Every rule below is pinned by
// `conformance/items/pricing.json` (band 124, US-2), which every engine leg runs:
//
//   * base      — buy: the catalogue's `value`; sell: `sellValue` * sellMarginPercent,
//                 because a vendor pays the world's margin and charges no markup on
//                 top (charging one would be the same term twice). A catalogue row
//                 that does not exist has base 0 and no terms are read.
//   * markup    — the business's `vendor_markup/2`, buy side only. Subject: the
//                 business it came from.
//   * scarcity  — the vendor's own shelf against the item's normal stock level,
//                 scaled by scarcityPercent. ABSENT (not zero) at or above normal:
//                 a host can then tell "this world has no scarcity" from "this shelf
//                 is full". Keeps its sign in both directions — an empty shelf raises
//                 what the shop charges AND what it offers.
//   * standing  — reputation with the faction the shop answers to, scaled by
//                 standing/standingScale. FLIPS SIGN with the direction: a friend
//                 charged less for a sword is offered more for one.
//   * proprietor— the buyer owns the business. Authored as its own percentage rather
//                 than as "skip the markup", so a guild may charge its owner nothing
//                 and a family firm something.
//
// Terms are summed against the AUTHORED base rather than compounded, so term order
// cannot make two engines disagree; every percentage is per unit; the world's
// `minimumPrice` floor is applied LAST (after the terms, so the adjustments still
// report what they were worth); and the quantity multiplies the unit at the very end,
// so a stack of forty arrows cannot round differently from forty single arrows.
// Rounding is half-away-from-zero at each reported number.
//
// std-only (no Unreal Engine, no CoreMinimal.h): the whole contract runs headless
// under ctest `ui_state_binding`, which diffs the canonical projection of every
// corpus case byte for byte. The UMG seam is UInsimulMerchantPanel.

#pragma once

#include "InsimulJson.h"

#include <string>
#include <vector>

namespace insimul {

/** One reported term of a price: what it was, how much, and whose it is. */
struct FPriceAdjustment {
	/** "markup" | "scarcity" | "standing" | "proprietor". */
	std::string Factor;
	/** Signed percentage applied to the AUTHORED base, per unit. */
	long long Percent = 0;
	/** Signed coin this term moved, per unit. */
	long long Amount = 0;
	/** The business / vendor / faction / owner the term came from. */
	std::string Subject;
};

/** A quoted price, with every term that made it. */
struct FPriceQuote {
	std::string Item;
	/** "buy" (the player pays) | "sell" (the vendor pays). */
	std::string Direction;
	long long Base = 0;
	std::vector<FPriceAdjustment> Adjustments;
	/** Per-unit price after the terms and the world's floor. */
	long long Unit = 0;
	long long Quantity = 1;
	/** Unit * Quantity — the last multiplication. */
	long long Total = 0;
	/** True when there is no economy at all and the price IS the item's value. */
	bool bFallback = false;

	/** The present-only projection the corpus pins (mirrors expected.price). */
	FJsonValuePtr ToProjection() const;
};

/** The world's authored economy dials (the resolved ItemTuning subset). */
struct FPriceTuning {
	long long MarkupPercent = 10;
	long long SellMarginPercent = 80;
	long long ScarcityPercent = 50;
	long long StandingPercent = 25;
	long long StandingScale = 100;
	long long ProprietorPercent = 30;
	long long MinimumPrice = 1;

	/** Read the dials out of a corpus/world `tuning` object (absent -> defaults). */
	static FPriceTuning FromJson(const FJsonValue& Tuning);
};

/**
 * The market a price is asked in. `bPresent` false is a real path, not a
 * degradation: a puzzle world, a language-learning world and a bare harness price
 * an item with no vendor, no business, no faction and no shelf.
 */
struct FPriceMarket {
	bool bPresent = false;

	std::string VendorId;
	std::string Business;
	bool bHasMarkupPercent = false;
	long long MarkupPercent = 0;

	/** Who owns the business (the proprietor term fires when it is the actor). */
	std::string Owner;
	/** The faction the shop answers to (the standing term's subject). */
	std::string Faction;

	bool bHasStanding = false;
	long long Standing = 0;

	/** What is on the vendor's own shelf, and what this item normally sits at. */
	bool bHasStock = false;
	long long Stock = 0;
	bool bHasStockNormal = false;
	long long StockNormal = 0;

	/** Read a corpus/world `market` object (null -> an absent market). */
	static FPriceMarket FromJson(const FJsonValue* Market);
};

/** The catalogue row a price is asked about. */
struct FPriceItem {
	/** False when the catalogue never declared this id — base 0, no terms read. */
	bool bDeclared = false;
	std::string Id;
	long long Value = 0;
	long long SellValue = 0;

	/** Read a corpus/world `item` row (null -> undeclared, keeping `FallbackId`). */
	static FPriceItem FromJson(const FJsonValue* Item, const std::string& FallbackId);
};

class FInsimulTradePricing {
public:
	/**
	 * Quote `Quantity` of `Item` for `Actor` in `Market`. `Direction` is "buy" (the
	 * player pays) or "sell" (the vendor pays); any other value is treated as "buy".
	 */
	static FPriceQuote Quote(const FPriceItem& Item, const FPriceMarket& Market,
		const FPriceTuning& Tuning, const std::string& Actor,
		const std::string& Direction, long long Quantity);

	/** Canonical JSON of the quote projection — byte-comparable with the corpus. */
	static std::string QuoteCanonical(const FPriceItem& Item, const FPriceMarket& Market,
		const FPriceTuning& Tuning, const std::string& Actor,
		const std::string& Direction, long long Quantity);

	/** Round half away from zero — the corpus's rounding at every reported number. */
	static long long RoundHalfAwayFromZero(double Value);
};

} // namespace insimul
