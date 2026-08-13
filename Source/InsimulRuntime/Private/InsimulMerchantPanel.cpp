// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulMerchantPanel — thin UUserWidget over UInsimulTradePanel. The panel does
// no pricing: it asks QuotePrice() and renders the terms it is handed.

#include "InsimulMerchantPanel.h"

#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

const FName UInsimulMerchantPanel::PanelKey = FName(TEXT("merchant"));

void UInsimulMerchantPanel::Open(UInsimulTradePanel* InTrade, const FString& InMerchantId)
{
	TradeModel = InTrade;
	MerchantId = InMerchantId;
	Refresh();
}

void UInsimulMerchantPanel::Close()
{
	MerchantId.Reset();
	Refresh();
}

TArray<FInsimulTradeItem> UInsimulMerchantPanel::MerchantStock() const
{
	if (!TradeModel || MerchantId.IsEmpty())
	{
		return TArray<FInsimulTradeItem>();
	}
	return TradeModel->MerchantItems(MerchantId);
}

TArray<FInsimulTradeItem> UInsimulMerchantPanel::PlayerStock() const
{
	return TradeModel ? TradeModel->PlayerItems() : TArray<FInsimulTradeItem>();
}

FInsimulPriceQuote UInsimulMerchantPanel::Price(const FString& ItemId, int32 Qty, bool bSell) const
{
	if (!TradeModel || MerchantId.IsEmpty())
	{
		return FInsimulPriceQuote();
	}
	return TradeModel->QuotePrice(MerchantId, ItemId, Qty, bSell);
}

FString UInsimulMerchantPanel::PriceReason(const FInsimulPriceQuote& Quote) const
{
	if (Quote.Terms.Num() == 0)
	{
		return FString::FromInt(Quote.Total);
	}

	FString Reasons;
	for (const FInsimulPriceTerm& Term : Quote.Terms)
	{
		if (!Reasons.IsEmpty())
		{
			Reasons += TEXT(", ");
		}
		Reasons += FString::Printf(TEXT("%s %+d%% %s"), *Term.Factor, Term.Percent, *Term.Subject);
	}
	return FString::Printf(TEXT("%d (%s)"), Quote.Total, *Reasons);
}

FInsimulTradeResult UInsimulMerchantPanel::Buy(const FString& ItemId, int32 Qty)
{
	FInsimulTradeResult Result;
	if (!TradeModel || MerchantId.IsEmpty())
	{
		Result.Reason = TEXT("no_merchant");
		return Result;
	}
	Result = TradeModel->Buy(MerchantId, ItemId, Qty);
	if (Result.bOk)
	{
		TradeModel->CommitToSave();
	}
	Refresh();
	return Result;
}

FInsimulTradeResult UInsimulMerchantPanel::Sell(const FString& ItemId, int32 Qty)
{
	FInsimulTradeResult Result;
	if (!TradeModel || MerchantId.IsEmpty())
	{
		Result.Reason = TEXT("no_merchant");
		return Result;
	}
	Result = TradeModel->Sell(MerchantId, ItemId, Qty);
	if (Result.bOk)
	{
		TradeModel->CommitToSave();
	}
	Refresh();
	return Result;
}

void UInsimulMerchantPanel::FillList(UVerticalBox* Box, const TArray<FInsimulTradeItem>& Stacks,
	bool bSell)
{
	if (!Box)
	{
		return;
	}
	Box->ClearChildren();
	for (const FInsimulTradeItem& Stack : Stacks)
	{
		if (ItemRowClass)
		{
			if (UUserWidget* Row = CreateWidget<UUserWidget>(this, ItemRowClass))
			{
				Box->AddChildToVerticalBox(Row);
				continue;
			}
		}
		UTextBlock* Row = NewObject<UTextBlock>(this);
		Row->SetText(FText::FromString(FString::Printf(TEXT("%s x%d — %s"),
			*Stack.ItemId, Stack.Quantity, *PriceReason(Price(Stack.ItemId, 1, bSell)))));
		Box->AddChildToVerticalBox(Row);
	}
}

void UInsimulMerchantPanel::Refresh()
{
	if (MerchantNameText)
	{
		MerchantNameText->SetText(FText::FromString(MerchantId));
	}
	if (PlayerGoldText)
	{
		PlayerGoldText->SetText(FText::AsNumber(TradeModel ? TradeModel->PlayerGold() : 0));
	}
	if (MerchantGoldText)
	{
		MerchantGoldText->SetText(FText::AsNumber(
			(TradeModel && !MerchantId.IsEmpty()) ? TradeModel->MerchantGold(MerchantId) : 0));
	}

	FillList(MerchantListBox, MerchantStock(), /*bSell=*/false);
	FillList(PlayerListBox, PlayerStock(), /*bSell=*/true);

	OnMerchantChanged.Broadcast();
}
