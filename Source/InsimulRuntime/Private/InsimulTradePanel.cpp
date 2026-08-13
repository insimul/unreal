// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulTradePanel — thin UObject wrapper over FInsimulTradeModel. All trade
// logic lives in (and is host-tested by) the portable core; this file only marshals
// FString<->std::string and FTradeItem<->std::vector, and is structurally
// syntax-gated (check.mjs). State is the attached FTradeState the save shell
// hydrates from currentState — the model keeps no store of its own.

#include "InsimulTradePanel.h"

#include "InsimulRuntimeSubsystem.h"

#include "../Portable/InsimulBootstrap.h"
#include "../Portable/InsimulSaveSystem.h"
#include "../Portable/InsimulTradeModel.h"
#include "../Portable/InsimulTradePricing.h"
#include "../Portable/InsimulUIStateBinding.h"

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

	FInsimulTradeItem FromPortable(const insimul::FTradeItem& I)
	{
		FInsimulTradeItem Out;
		Out.ItemId = ToFString(I.ItemId);
		Out.Quantity = static_cast<int32>(I.Quantity);
		Out.Value = static_cast<int32>(I.Value);
		return Out;
	}

	TArray<FInsimulTradeItem> ToArray(const std::vector<insimul::FTradeItem>& Items)
	{
		TArray<FInsimulTradeItem> Out;
		Out.Reserve(static_cast<int32>(Items.size()));
		for (const insimul::FTradeItem& I : Items)
		{
			Out.Add(FromPortable(I));
		}
		return Out;
	}

	FInsimulTradeResult FromResult(const insimul::FTradeResult& R)
	{
		FInsimulTradeResult Out;
		Out.bOk = R.bOk;
		Out.Reason = ToFString(R.Reason);
		Out.Moved = static_cast<int32>(R.Moved);
		return Out;
	}
}

UInsimulTradePanel::UInsimulTradePanel()
{
}

insimul::FInsimulTradeModel& UInsimulTradePanel::EnsureModel()
{
	if (!State.IsValid())
	{
		// BindToRuntime() fills this from save.currentState (player.gold/inventory,
		// containers.containers, npcs.merchantStates); until it does, the slice is
		// empty so a Blueprint call before hydration is always safe.
		State = MakeUnique<insimul::FTradeState>();
	}
	if (!Model.IsValid())
	{
		Model = MakeUnique<insimul::FInsimulTradeModel>(State.Get());
	}
	return *Model;
}

int32 UInsimulTradePanel::PlayerGold() const
{
	return Model.IsValid() ? static_cast<int32>(Model->PlayerGold()) : 0;
}

TArray<FInsimulTradeItem> UInsimulTradePanel::PlayerItems() const
{
	return Model.IsValid() ? ToArray(Model->PlayerItems()) : TArray<FInsimulTradeItem>();
}

TArray<FInsimulTradeItem> UInsimulTradePanel::ContainerItems(const FString& ContainerId) const
{
	return Model.IsValid() ? ToArray(Model->ContainerItems(ToStd(ContainerId))) : TArray<FInsimulTradeItem>();
}

TArray<FInsimulTradeItem> UInsimulTradePanel::MerchantItems(const FString& MerchantId) const
{
	return Model.IsValid() ? ToArray(Model->MerchantItems(ToStd(MerchantId))) : TArray<FInsimulTradeItem>();
}

int32 UInsimulTradePanel::MerchantGold(const FString& MerchantId) const
{
	return Model.IsValid() ? static_cast<int32>(Model->MerchantGold(ToStd(MerchantId))) : 0;
}

FInsimulTradeResult UInsimulTradePanel::TakeFromContainer(const FString& ContainerId, const FString& ItemId, int32 Qty)
{
	return FromResult(EnsureModel().TakeFromContainer(ToStd(ContainerId), ToStd(ItemId), Qty));
}

FInsimulTradeResult UInsimulTradePanel::TakeAllFromContainer(const FString& ContainerId)
{
	return FromResult(EnsureModel().TakeAllFromContainer(ToStd(ContainerId)));
}

FInsimulTradeResult UInsimulTradePanel::Buy(const FString& MerchantId, const FString& ItemId, int32 Qty)
{
	return FromResult(EnsureModel().Buy(ToStd(MerchantId), ToStd(ItemId), Qty));
}

FInsimulTradeResult UInsimulTradePanel::Sell(const FString& MerchantId, const FString& ItemId, int32 Qty)
{
	return FromResult(EnsureModel().Sell(ToStd(MerchantId), ToStd(ItemId), Qty));
}

bool UInsimulTradePanel::BindToRuntime(UInsimulRuntimeSubsystem* Runtime)
{
	BoundRuntime = nullptr;
	if (!Runtime || !Runtime->Context())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Insimul trade panel: no booted runtime to bind to; the panel would "
				 "show an inventory that is not this playthrough's."));
		return false;
	}

	const insimul::FJsonValue* SaveFile = Runtime->Context()->Save().SaveFile();
	if (!SaveFile)
	{
		UE_LOG(LogTemp, Warning, TEXT("Insimul trade panel: the runtime has no loaded save."));
		return false;
	}

	// EnsureModel first: it is what allocates the slice the binding fills, and a
	// Blueprint may call BindToRuntime before any other method has touched us.
	EnsureModel();

	std::string Error;
	if (!insimul::FInsimulUIStateBinding::HydrateTrade(*SaveFile, *State, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Insimul trade panel: %s"), *ToFString(Error));
		return false;
	}
	BoundRuntime = Runtime;
	return true;
}

bool UInsimulTradePanel::CommitToSave()
{
	UInsimulRuntimeSubsystem* Runtime = BoundRuntime.Get();
	if (!Runtime || !Runtime->Context() || !State.IsValid())
	{
		return false;
	}
	insimul::FJsonValue* SaveFile = Runtime->Context()->Save().MutableSaveFile();
	if (!SaveFile)
	{
		return false;
	}
	std::string Error;
	if (!insimul::FInsimulUIStateBinding::ApplyTrade(*State, *SaveFile, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Insimul trade panel: %s"), *ToFString(Error));
		return false;
	}
	return true;
}

FInsimulPriceQuote UInsimulTradePanel::QuotePrice(const FString& MerchantId, const FString& ItemId,
	int32 Qty, bool bSell) const
{
	FInsimulPriceQuote Out;
	Out.Quantity = Qty;

	const UInsimulRuntimeSubsystem* Runtime = BoundRuntime.Get();
	const insimul::FJsonValue* SaveFile =
		(Runtime && Runtime->Context()) ? Runtime->Context()->Save().SaveFile() : nullptr;

	insimul::FPriceMarket Market;
	if (SaveFile)
	{
		std::string Error;
		insimul::FInsimulUIStateBinding::HydrateMarket(*SaveFile, ToStd(MerchantId),
			ToStd(ItemId), Market, Error);
	}

	// The catalogue row this panel can see is the stack itself: a merchant's shelf
	// carries the authored value, which is the base the terms are summed against.
	insimul::FPriceItem Item;
	Item.Id = ToStd(ItemId);
	if (State.IsValid())
	{
		const auto Found = State->Merchants.find(ToStd(MerchantId));
		const std::vector<insimul::FTradeItem>* Shelf =
			(Found != State->Merchants.end()) ? &Found->second.Items : nullptr;
		const std::vector<insimul::FTradeItem>* Bag = bSell ? &State->PlayerInventory : Shelf;
		if (Bag)
		{
			for (const insimul::FTradeItem& Stack : *Bag)
			{
				if (Stack.ItemId != Item.Id)
				{
					continue;
				}
				Item.bDeclared = true;
				Item.Value = Stack.Value;
				Item.SellValue = Stack.Value;
				break;
			}
		}
	}

	const insimul::FPriceQuote Quote = insimul::FInsimulTradePricing::Quote(
		Item, Market, insimul::FPriceTuning(), std::string(),
		bSell ? "sell" : "buy", Qty);

	Out.Base = static_cast<int32>(Quote.Base);
	Out.Unit = static_cast<int32>(Quote.Unit);
	Out.Quantity = static_cast<int32>(Quote.Quantity);
	Out.Total = static_cast<int32>(Quote.Total);
	Out.bFallback = Quote.bFallback;
	for (const insimul::FPriceAdjustment& Adj : Quote.Adjustments)
	{
		FInsimulPriceTerm Term;
		Term.Factor = ToFString(Adj.Factor);
		Term.Percent = static_cast<int32>(Adj.Percent);
		Term.Amount = static_cast<int32>(Adj.Amount);
		Term.Subject = ToFString(Adj.Subject);
		Out.Terms.Add(Term);
	}
	return Out;
}

void UInsimulTradePanel::BeginDestroy()
{
	Model.Reset();
	State.Reset();
	Super::BeginDestroy();
}
