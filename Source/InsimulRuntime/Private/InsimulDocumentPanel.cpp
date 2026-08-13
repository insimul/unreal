// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulDocumentPanel — thin UUserWidget. The only arithmetic is a character
// budget over the source text, which is the one pagination that reads the same on
// four engines and two resolutions.

#include "InsimulDocumentPanel.h"

#include "Components/TextBlock.h"

const FName UInsimulDocumentPanel::PanelKey = FName(TEXT("documents"));

const FInsimulDocument* UInsimulDocumentPanel::FindOpen() const
{
	if (OpenId.IsEmpty())
	{
		return nullptr;
	}
	return Shelf.FindByPredicate(
		[this](const FInsimulDocument& Document) { return Document.Id == OpenId; });
}

void UInsimulDocumentPanel::SetDocuments(const TArray<FInsimulDocument>& InDocuments)
{
	Shelf = InDocuments;
	if (!FindOpen())
	{
		OpenId.Reset();
		Page = 0;
	}
	Refresh();
}

bool UInsimulDocumentPanel::Open(const FString& DocumentId)
{
	const FInsimulDocument* Found = Shelf.FindByPredicate(
		[&DocumentId](const FInsimulDocument& Document) { return Document.Id == DocumentId; });
	if (!Found)
	{
		UE_LOG(LogTemp, Warning, TEXT("Insimul document reader: no document '%s' on this shelf"),
			*DocumentId);
		return false;
	}
	OpenId = DocumentId;
	Page = 0;
	Refresh();
	OnDocumentChanged.Broadcast(OpenId, Page);
	return true;
}

void UInsimulDocumentPanel::Close()
{
	if (OpenId.IsEmpty())
	{
		return;
	}
	OpenId.Reset();
	Page = 0;
	Refresh();
	OnDocumentChanged.Broadcast(FString(), 0);
}

int32 UInsimulDocumentPanel::PageCount() const
{
	const FInsimulDocument* Document = FindOpen();
	if (!Document)
	{
		return 0;
	}
	if (PageLength <= 0 || Document->Body.Len() <= PageLength)
	{
		return 1;
	}
	return FMath::DivideAndRoundUp(Document->Body.Len(), PageLength);
}

FString UInsimulDocumentPanel::PageText() const
{
	const FInsimulDocument* Document = FindOpen();
	if (!Document)
	{
		return FString();
	}
	if (PageLength <= 0)
	{
		return Document->Body;
	}
	const int32 Start = Page * PageLength;
	if (Start >= Document->Body.Len())
	{
		return FString();
	}
	return Document->Body.Mid(Start, PageLength);
}

bool UInsimulDocumentPanel::NextPage()
{
	if (Page + 1 >= PageCount())
	{
		return false;
	}
	++Page;
	Refresh();
	OnDocumentChanged.Broadcast(OpenId, Page);
	return true;
}

bool UInsimulDocumentPanel::PreviousPage()
{
	if (Page <= 0)
	{
		return false;
	}
	--Page;
	Refresh();
	OnDocumentChanged.Broadcast(OpenId, Page);
	return true;
}

void UInsimulDocumentPanel::Refresh()
{
	const FInsimulDocument* Document = FindOpen();

	if (TitleText)
	{
		TitleText->SetText(Document ? FText::FromString(Document->Title) : FText::GetEmpty());
	}
	if (BodyText)
	{
		BodyText->SetText(FText::FromString(PageText()));
	}
	if (PageCounterText)
	{
		PageCounterText->SetText(Document
			? FText::FromString(FString::Printf(TEXT("%d / %d"), Page + 1, PageCount()))
			: FText::GetEmpty());
	}
}
