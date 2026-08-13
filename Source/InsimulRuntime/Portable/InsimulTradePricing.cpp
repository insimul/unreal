// Copyright 2024 Insimul. All Rights Reserved.

#include "InsimulTradePricing.h"

#include "InsimulCanonicalJson.h"

#include <cmath>

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

/** The percentage a term contributes, applied to the AUTHORED base, per unit. */
long long AmountFor(long long Base, long long Percent) {
	return FInsimulTradePricing::RoundHalfAwayFromZero(
		static_cast<double>(Base) * static_cast<double>(Percent) / 100.0);
}

} // namespace

// ── Rounding ────────────────────────────────────────────────────────────────

long long FInsimulTradePricing::RoundHalfAwayFromZero(double Value) {
	// std::round is exactly half-away-from-zero, and is the rounding every number
	// the corpus reports was produced with. Spelled out rather than called bare so
	// a port that reaches for a banker's-rounding helper sees the requirement.
	return static_cast<long long>(std::round(Value));
}

// ── Input readers ───────────────────────────────────────────────────────────

FPriceTuning FPriceTuning::FromJson(const FJsonValue& Tuning) {
	FPriceTuning Out;
	if (!Tuning.IsObject()) {
		return Out;
	}
	Out.MarkupPercent = Tuning.GetInt("markupPercent", Out.MarkupPercent);
	Out.SellMarginPercent = Tuning.GetInt("sellMarginPercent", Out.SellMarginPercent);
	Out.ScarcityPercent = Tuning.GetInt("scarcityPercent", Out.ScarcityPercent);
	Out.StandingPercent = Tuning.GetInt("standingPercent", Out.StandingPercent);
	Out.StandingScale = Tuning.GetInt("standingScale", Out.StandingScale);
	Out.ProprietorPercent = Tuning.GetInt("proprietorPercent", Out.ProprietorPercent);
	Out.MinimumPrice = Tuning.GetInt("minimumPrice", Out.MinimumPrice);
	return Out;
}

FPriceMarket FPriceMarket::FromJson(const FJsonValue* Market) {
	FPriceMarket Out;
	if (!Market || !Market->IsObject()) {
		return Out;
	}
	Out.bPresent = true;
	Out.Owner = Market->GetString("owner");
	Out.Faction = Market->GetString("faction");
	if (const FJsonValue* Standing = Market->Find("standing")) {
		if (Standing->IsNumber()) {
			Out.bHasStanding = true;
			Out.Standing = Standing->AsInt(0);
		}
	}
	if (const FJsonValue* Stock = Market->Find("stock")) {
		if (Stock->IsNumber()) {
			Out.bHasStock = true;
			Out.Stock = Stock->AsInt(0);
		}
	}
	if (const FJsonValue* Normal = Market->Find("stockNormal")) {
		if (Normal->IsNumber()) {
			Out.bHasStockNormal = true;
			Out.StockNormal = Normal->AsInt(0);
		}
	}
	if (const FJsonValue* Vendor = Market->Find("vendor")) {
		if (Vendor->IsObject()) {
			Out.VendorId = Vendor->GetString("id");
			Out.Business = Vendor->GetString("business");
			if (const FJsonValue* Markup = Vendor->Find("markupPercent")) {
				if (Markup->IsNumber()) {
					Out.bHasMarkupPercent = true;
					Out.MarkupPercent = Markup->AsInt(0);
				}
			}
		}
	}
	return Out;
}

FPriceItem FPriceItem::FromJson(const FJsonValue* Item, const std::string& FallbackId) {
	FPriceItem Out;
	Out.Id = FallbackId;
	if (!Item || !Item->IsObject()) {
		return Out;
	}
	Out.bDeclared = true;
	const std::string Id = Item->GetString("id");
	if (!Id.empty()) {
		Out.Id = Id;
	}
	Out.Value = Item->GetInt("value", 0);
	Out.SellValue = Item->GetInt("sellValue", 0);
	return Out;
}

// ── Projection ──────────────────────────────────────────────────────────────

FJsonValuePtr FPriceQuote::ToProjection() const {
	auto Root = MakeObject();
	ObjSet(*Root, "item", MakeString(Item));
	ObjSet(*Root, "direction", MakeString(Direction));
	ObjSet(*Root, "base", MakeInt(Base));

	auto Terms = MakeArray();
	for (const FPriceAdjustment& Adj : Adjustments) {
		auto Row = MakeObject();
		ObjSet(*Row, "factor", MakeString(Adj.Factor));
		ObjSet(*Row, "percent", MakeInt(Adj.Percent));
		ObjSet(*Row, "amount", MakeInt(Adj.Amount));
		ObjSet(*Row, "subject", MakeString(Adj.Subject));
		Terms->ArrayItems.push_back(Row);
	}
	ObjSet(*Root, "adjustments", Terms);

	ObjSet(*Root, "unit", MakeInt(Unit));
	ObjSet(*Root, "quantity", MakeInt(Quantity));
	ObjSet(*Root, "total", MakeInt(Total));
	ObjSet(*Root, "fallback", MakeBool(bFallback));
	return Root;
}

// ── The quote ───────────────────────────────────────────────────────────────

FPriceQuote FInsimulTradePricing::Quote(const FPriceItem& Item, const FPriceMarket& Market,
	const FPriceTuning& Tuning, const std::string& Actor,
	const std::string& Direction, long long Quantity) {
	const bool bSell = (Direction == "sell");

	FPriceQuote Out;
	Out.Item = Item.Id;
	Out.Direction = bSell ? "sell" : "buy";
	Out.Quantity = Quantity;

	// The base is authored: what the catalogue says the thing is worth, and on the
	// sell side the world's own margin on what it will pay back for one.
	if (Item.bDeclared) {
		Out.Base = bSell
			? RoundHalfAwayFromZero(static_cast<double>(Item.SellValue)
				* static_cast<double>(Tuning.SellMarginPercent) / 100.0)
			: Item.Value;
	}

	// No vendor, no business, no faction, no shelf: the price IS the item's value,
	// and `fallback` says so rather than leaving a host to infer it from an empty
	// adjustment list (a stocked shop with a 0% markup has one of those too).
	if (!Market.bPresent) {
		Out.bFallback = true;
	} else if (Item.bDeclared) {
		// markup — the business's margin. Buy side only: a vendor who paid the
		// world's sell margin and then charged a markup would charge it twice.
		if (!bSell) {
			const long long Percent = Market.bHasMarkupPercent ? Market.MarkupPercent
															   : Tuning.MarkupPercent;
			FPriceAdjustment Adj;
			Adj.Factor = "markup";
			Adj.Percent = Percent;
			Adj.Amount = AmountFor(Out.Base, Percent);
			// The term names where it came from: the business if the vendor trades
			// for one, else the vendor themselves.
			Adj.Subject = Market.Business.empty() ? Market.VendorId : Market.Business;
			Out.Adjustments.push_back(Adj);
		}

		// scarcity — the shelf against the item's normal level. Above normal the
		// term is ABSENT rather than zero; a vendor who overbought is not obliged
		// to say so. Same sign in both directions: an empty shelf raises what the
		// shop charges AND what it offers.
		if (Market.bHasStock && Market.bHasStockNormal && Market.StockNormal > 0
			&& Market.Stock < Market.StockNormal) {
			const double Shortfall = static_cast<double>(Market.StockNormal - Market.Stock)
				/ static_cast<double>(Market.StockNormal);
			const long long Percent = RoundHalfAwayFromZero(
				static_cast<double>(Tuning.ScarcityPercent) * Shortfall);
			FPriceAdjustment Adj;
			Adj.Factor = "scarcity";
			Adj.Percent = Percent;
			Adj.Amount = AmountFor(Out.Base, Percent);
			Adj.Subject = Market.VendorId;
			Out.Adjustments.push_back(Adj);
		}

		// standing — REPUTATION with the faction the shop answers to. One authored
		// percentage, not two: it flips sign with the direction, so a friend charged
		// less for a sword is offered more for one.
		if (Market.bHasStanding && Tuning.StandingScale != 0) {
			const double Scaled = static_cast<double>(Tuning.StandingPercent)
				* static_cast<double>(Market.Standing) / static_cast<double>(Tuning.StandingScale);
			long long Percent = RoundHalfAwayFromZero(Scaled);
			if (!bSell) {
				Percent = -Percent;
			}
			FPriceAdjustment Adj;
			Adj.Factor = "standing";
			Adj.Percent = Percent;
			Adj.Amount = AmountFor(Out.Base, Percent);
			Adj.Subject = Market.Faction;
			Out.Adjustments.push_back(Adj);
		}

		// proprietor — the buyer owns the place. The anti-markup, so buy side only
		// and authored as its own percentage: a guild may want the owner to pay
		// nothing and a family firm may want them to pay something.
		if (!bSell && !Actor.empty() && Actor == Market.Owner) {
			const long long Percent = -Tuning.ProprietorPercent;
			FPriceAdjustment Adj;
			Adj.Factor = "proprietor";
			Adj.Percent = Percent;
			Adj.Amount = AmountFor(Out.Base, Percent);
			Adj.Subject = Market.Owner;
			Out.Adjustments.push_back(Adj);
		}
	}

	long long Unit = Out.Base;
	for (const FPriceAdjustment& Adj : Out.Adjustments) {
		Unit += Adj.Amount;
	}
	// The world's floor, applied LAST — after the terms, so the adjustments still
	// report what they were worth.
	if (Unit < Tuning.MinimumPrice) {
		Unit = Tuning.MinimumPrice;
	}
	Out.Unit = Unit;
	Out.Total = Unit * Quantity;
	return Out;
}

std::string FInsimulTradePricing::QuoteCanonical(const FPriceItem& Item, const FPriceMarket& Market,
	const FPriceTuning& Tuning, const std::string& Actor,
	const std::string& Direction, long long Quantity) {
	const FPriceQuote Result = Quote(Item, Market, Tuning, Actor, Direction, Quantity);
	const FJsonValuePtr Projection = Result.ToProjection();
	return CanonicalJsonStringify(*Projection);
}

} // namespace insimul
