// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulContainerPanel — thin UUserWidget over UInsimulTradePanel. Every take
// goes through the portable model and is committed to the save immediately, so the
// window is never the only place a moved stack exists.

#include "InsimulContainerPanel.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

const FName UInsimulContainerPanel::PanelKey = FName(TEXT("container"));

void UInsimulContainerPanel::Open(UInsimulTradePanel* InTrade, const FString& InContainerId)
{
	TradeModel = InTrade;
	ContainerId = InContainerId;
	Refresh();
}

void UInsimulContainerPanel::Close()
{
	ContainerId.Reset();
	Refresh();
}

TArray<FInsimulTradeItem> UInsimulContainerPanel::Items() const
{
	if (!TradeModel || ContainerId.IsEmpty())
	{
		return TArray<FInsimulTradeItem>();
	}
	return TradeModel->ContainerItems(ContainerId);
}

FInsimulTradeResult UInsimulContainerPanel::Take(const FString& ItemId, int32 Qty)
{
	FInsimulTradeResult Result;
	if (!TradeModel || ContainerId.IsEmpty())
	{
		Result.Reason = TEXT("no_container");
		return Result;
	}
	Result = TradeModel->TakeFromContainer(ContainerId, ItemId, Qty);
	if (Result.bOk)
	{
		TradeModel->CommitToSave();
	}
	Refresh();
	return Result;
}

FInsimulTradeResult UInsimulContainerPanel::TakeAll()
{
	FInsimulTradeResult Result;
	if (!TradeModel || ContainerId.IsEmpty())
	{
		Result.Reason = TEXT("no_container");
		return Result;
	}
	Result = TradeModel->TakeAllFromContainer(ContainerId);
	if (Result.bOk)
	{
		TradeModel->CommitToSave();
	}
	Refresh();
	return Result;
}

void UInsimulContainerPanel::Refresh()
{
	const TArray<FInsimulTradeItem> Stacks = Items();

	if (TitleText)
	{
		TitleText->SetText(FText::FromString(ContainerId));
	}
	if (TakeAllButton)
	{
		TakeAllButton->SetIsEnabled(Stacks.Num() > 0);
	}

	if (ContainerListBox)
	{
		ContainerListBox->ClearChildren();
		for (const FInsimulTradeItem& Stack : Stacks)
		{
			if (ItemRowClass)
			{
				if (UUserWidget* Row = CreateWidget<UUserWidget>(this, ItemRowClass))
				{
					ContainerListBox->AddChildToVerticalBox(Row);
					continue;
				}
			}
			UTextBlock* Row = NewObject<UTextBlock>(this);
			Row->SetText(FText::FromString(
				FString::Printf(TEXT("%s x%d"), *Stack.ItemId, Stack.Quantity)));
			ContainerListBox->AddChildToVerticalBox(Row);
		}
	}

	OnContainerChanged.Broadcast();
}
