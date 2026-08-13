// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulSkillPanel — the skill panel (US-2 of tasklist 190), panel key
// `skill_tree`, owned by the world's skill module.
//
// The UE seam over the portable view-model (Portable/InsimulSkillTreeModel.h). It
// holds NO tree of its own: the rows, the edges, the prices and the "why not" on
// every node are the value that model returns, and this class copies it into
// reflected structs so UMG and Blueprint can draw it. The semantics — depth from
// the authored edges, the refusal ladder, the label fallback, the tuned numbers —
// are proven UE-free by ctest `ui_skill_tree` against `conformance/skills/trees.json`,
// the same six cases the Babylon reference and the Unity/Godot ports run.
//
// READ-ONLY, like the equipment panel: TAKING a node is the skills module's
// decision layer, not the UI's. The panel says what may be taken and what it would
// cost; the unlock itself goes through the module, and the pool it spends from is
// the save's. Nothing here writes playthrough state.
//
// GATED BY DATA. This panel belongs to the module the shipped catalog names
// (Content/Data/insimul/ui/panels.json), so a world whose genre bundle did not
// select it never resolves this widget at all — the resolver refuses the key and
// says why. Nothing in this file decides that; UInsimulUIPanelSurface does.
//
// Thin, syntax-gated UMG boundary.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InsimulSkillPanel.generated.h"

class UTextBlock;
class UVerticalBox;

// The engine-agnostic model this seam wraps (pimpl — never in the reflected
// layout). Host-tested by ctest `ui_skill_tree`.
namespace insimul { struct FSkillViewInput; }

/** One `modifies(Param, Amount)` total, as the world's own atom names it. */
USTRUCT(BlueprintType)
struct FInsimulSkillModifier
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	FString Param;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	float Amount = 0.0f;
};

/** One node of one tree, with everything a panel needs to draw it. */
USTRUCT(BlueprintType)
struct FInsimulSkillNodeRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	FString Tree;

	/** The authored name, or the node's own id — a half-authored tree is still
	 *  inspectable rather than a row of blank boxes. */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	FString Label;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	FString Description;

	/** Which row it sits on — derived from the authored edges, never authored. */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	int32 Depth = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	int32 Cost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	TArray<FString> Parents;

	/** Every goal it asks, parents desugared — ONE gate. */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	TArray<FString> Requires;

	/** The subset the KB did not satisfy. Empty when nobody asked a KB. */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	TArray<FString> Unmet;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	bool bTaken = false;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	bool bAvailable = false;

	/** It asks something core cannot settle alone. May be true AND available. */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	bool bConditional = false;

	/** "owned" | "points" | "requires" | "forbidden". Empty when available. */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	FString Refusal;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	TArray<FString> Unlocks;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	TArray<FString> Permits;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	TArray<FInsimulSkillModifier> Modifies;
};

/** One laid-out row of node ids (UMG cannot nest an array in an array). */
USTRUCT(BlueprintType)
struct FInsimulSkillRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	TArray<FString> NodeIds;
};

/** One authored edge, for a host that draws lines between boxes. */
USTRUCT(BlueprintType)
struct FInsimulSkillEdge
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	FString From;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	FString To;
};

/** One tree, as the panel renders it. */
USTRUCT(BlueprintType)
struct FInsimulSkillTreeRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	FString Id;

	/** The skill whose levels FUND this tree. */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	FString Skill;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	FString Label;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	int32 Level = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	int32 MaxLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	bool bCapped = false;

	/** What the actor has banked toward the next level. */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	int32 Banked = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	int32 NextLevel = 0;

	/** What that level prices, or 0 at the cap — never an invented number. */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	int32 NextLevelPrice = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	bool bAffordable = false;

	/** Unspent points in this tree's pool. */
	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	int32 Points = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	int32 Spent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	TArray<FInsimulSkillNodeRow> Nodes;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	TArray<FInsimulSkillRow> Rows;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	TArray<FInsimulSkillEdge> Edges;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	int32 Taken = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Insimul|Skill")
	int32 Total = 0;
};

/** Fired after the panel rebuilds from a new view. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInsimulSkillTreeChanged);

UCLASS()
class INSIMULRUNTIME_API UInsimulSkillPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	UInsimulSkillPanel();

	/** The stable panel key this widget serves (see the shipped panel catalog). */
	static const FName PanelKey;

	/**
	 * Load the world's authored trees + this playthrough's progression from one
	 * document (the shape `WorldIR.skills` carries) and rebuild. Returns false with
	 * OutError on a document that is not one — a corrupt skills section says so
	 * rather than drawing an empty panel that looks like a world with no trees.
	 */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Skill")
	bool LoadFromJson(const FString& Json, FString& OutError);

	/** The panel: every authored tree, in AUTHORING order. */
	UFUNCTION(BlueprintPure, Category = "Insimul|Skill")
	const TArray<FInsimulSkillTreeRow>& Trees() const { return TreeRows; }

	/** One tree by id, or an empty row. */
	UFUNCTION(BlueprintPure, Category = "Insimul|Skill")
	FInsimulSkillTreeRow TreeById(const FString& TreeId) const;

	/** Which trees a level in this skill funds. */
	UFUNCTION(BlueprintPure, Category = "Insimul|Skill")
	TArray<FString> TreesFundedBy(const FString& Skill) const;

	/** Rebuild the rows from the loaded input (call after the save's pools move). */
	UFUNCTION(BlueprintCallable, Category = "Insimul|Skill")
	void Refresh();

	/** One line for a boot log or a bug report. */
	UFUNCTION(BlueprintPure, Category = "Insimul|Skill")
	FString Describe() const;

	UPROPERTY(BlueprintAssignable, Category = "Insimul|Skill")
	FOnInsimulSkillTreeChanged OnSkillTreeChanged;

	/** The node widget class a creator supplies; unset builds a plain text row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insimul|Skill")
	TSubclassOf<UUserWidget> NodeWidgetClass;

protected:
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> TreeListBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyPanelText;

private:
	/** The authored trees + the actor, as the portable model takes them. */
	TUniquePtr<insimul::FSkillViewInput> Input;

	UPROPERTY(Transient)
	TArray<FInsimulSkillTreeRow> TreeRows;

	void Repaint();
};
