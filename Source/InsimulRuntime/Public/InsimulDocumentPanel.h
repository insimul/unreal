// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulDocumentPanel — letters, books and notices (US-2 of tasklist 190), panel
// key `documents`. Authored content rather than a mechanic, so every genre bundle
// carries it and the panel is ungated.
//
// WHAT A DOCUMENT IS. The narratives half of an imported content library
// (Portable/InsimulContentLibrary.h — id, title, body, language), plus whatever else
// a host hands over in the same shape. The reader holds the text it was given and
// paginates it; it fetches nothing and owns no library, because the content library
// is already loaded once for the whole runtime and a second copy inside a panel is a
// second thing to keep in step.
//
// PAGINATION IS BY CHARACTER BUDGET, NOT BY LAYOUT. A page break that depended on
// the rendered font would differ between the four engine legs and between two
// players' resolutions; a budget over the source text does not. A creator whose
// widget lays text out itself sets PageLength to 0 and gets one page.
//
// NO READ / UNREAD STATE IS WRITTEN. Whether the player has read a document is a
// per-playthrough fact and the save envelope declares no field for it (see
// conformance/saves), so the panel reports what it was told and never invents a
// `currentState.documents` schema — the same restraint the equipment panel shows
// about equipping (Portable/InsimulUIStateBinding.h).
//
// Thin, syntax-gated UMG boundary.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InsimulDocumentPanel.generated.h"

class UTextBlock;

/** One readable thing: a letter, a book, a notice, a journal page. */
USTRUCT(BlueprintType)
struct FInsimulDocument
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Documents")
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Documents")
	FString Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Documents")
	FString Body;

	/** The language it is written in — a world may hand the player a letter they
	 *  cannot yet read, and that is the content's decision, not the panel's. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Documents")
	FString Language;

	/** The host's own vocabulary (letter, book, notice…), never a compiled enum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Documents")
	FString Kind;
};

/** Fired when the open document or its page changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInsimulDocumentChanged, const FString&, DocumentId,
	int32, PageIndex);

UCLASS()
class INSIMULRUNTIME_API UInsimulDocumentPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The stable panel key this widget serves (see the shipped panel catalog). */
	static const FName PanelKey;

	/** Replace the shelf. Closes whatever was open if it is no longer on it. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Documents")
	void SetDocuments(const TArray<FInsimulDocument>& InDocuments);

	UFUNCTION(BlueprintPure, Category = "Insimul|Documents")
	const TArray<FInsimulDocument>& Documents() const { return Shelf; }

	/** Open one by id, at page zero. An unknown id opens nothing and returns false. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Documents")
	bool Open(const FString& DocumentId);

	UFUNCTION(BlueprintCallable, Category = "Insimul|Documents")
	void Close();

	UFUNCTION(BlueprintPure, Category = "Insimul|Documents")
	FString OpenDocument() const { return OpenId; }

	/** How many pages the open document has. Zero when nothing is open. */
	UFUNCTION(BlueprintPure, Category = "Insimul|Documents")
	int32 PageCount() const;

	UFUNCTION(BlueprintPure, Category = "Insimul|Documents")
	int32 CurrentPage() const { return Page; }

	/** The text of the current page, or empty when nothing is open. */
	UFUNCTION(BlueprintPure, Category = "Insimul|Documents")
	FString PageText() const;

	/** Turn a page. Returns false at the ends — a reader never wraps a book. */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Documents")
	bool NextPage();

	UFUNCTION(BlueprintCallable, Category = "Insimul|Documents")
	bool PreviousPage();

	UFUNCTION(BlueprintCallable, Category = "Insimul|Documents")
	void Refresh();

	UPROPERTY(BlueprintAssignable, Category = "Insimul|Documents")
	FOnInsimulDocumentChanged OnDocumentChanged;

	/** Characters per page; 0 means one page however long the text is. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Documents")
	int32 PageLength = 900;

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BodyText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PageCounterText;

private:
	const FInsimulDocument* FindOpen() const;

	UPROPERTY(EditAnywhere, Category = "Insimul|Documents")
	TArray<FInsimulDocument> Shelf;

	UPROPERTY(Transient)
	FString OpenId;

	UPROPERTY(Transient)
	int32 Page = 0;
};
