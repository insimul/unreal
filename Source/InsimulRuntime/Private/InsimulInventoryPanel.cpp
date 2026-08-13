// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulInventoryPanel — thin UUserWidget over UInsimulTradePanel. No item store
// of its own: Items() and Gold() are reads of the attached currentState slice.

#include "InsimulInventoryPanel.h"

#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

const FName UInsimulInventoryPanel::PanelKey = FName(TEXT("inventory"));

UInsimulTradePanel& UInsimulInventoryPanel::EnsureTrade()
{
	if (!TradeModel)
	{
		TradeModel = NewObject<UInsimulTradePanel>(this);
	}
	return *TradeModel;
}

bool UInsimulInventoryPanel::BindToRuntime(UInsimulRuntimeSubsystem* Runtime)
{
	const bool bBound = EnsureTrade().BindToRuntime(Runtime);
	Refresh();
	return bBound;
}

void UInsimulInventoryPanel::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureTrade();
	Refresh();
}

TArray<FInsimulTradeItem> UInsimulInventoryPanel::Items() const
{
	return TradeModel ? TradeModel->PlayerItems() : TArray<FInsimulTradeItem>();
}

int32 UInsimulInventoryPanel::Gold() const
{
	return TradeModel ? TradeModel->PlayerGold() : 0;
}

void UInsimulInventoryPanel::Refresh()
{
	const TArray<FInsimulTradeItem> Stacks = Items();

	if (GoldText)
	{
		GoldText->SetText(FText::AsNumber(Gold()));
	}
	if (EmptyText)
	{
		EmptyText->SetVisibility(Stacks.Num() == 0 ? ESlateVisibility::Visible
													: ESlateVisibility::Collapsed);
	}

	if (ItemListBox)
	{
		ItemListBox->ClearChildren();
		for (const FInsimulTradeItem& Stack : Stacks)
		{
			if (ItemRowClass)
			{
				if (UUserWidget* Row = CreateWidget<UUserWidget>(this, ItemRowClass))
				{
					ItemListBox->AddChildToVerticalBox(Row);
					continue;
				}
			}
			UTextBlock* Row = NewObject<UTextBlock>(this);
			Row->SetText(FText::FromString(
				FString::Printf(TEXT("%s x%d"), *Stack.ItemId, Stack.Quantity)));
			ItemListBox->AddChildToVerticalBox(Row);
		}
	}

	OnInventoryChanged.Broadcast();
}
