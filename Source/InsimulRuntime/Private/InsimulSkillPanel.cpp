// Copyright 2024 Insimul. All Rights Reserved.
//
// UInsimulSkillPanel — thin UUserWidget over FInsimulSkillTreeModel. Marshals
// FString<->std::string and renders; every skill-tree SEMANTIC (the row a node sits
// on, its price, the refusal ladder, the tuned curve) is the portable model's and is
// host-tested against the shared trees corpus by ctest `ui_skill_tree`.

#include "InsimulSkillPanel.h"

#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

#include "../Portable/InsimulJson.h"
#include "../Portable/InsimulSkillTreeModel.h"

const FName UInsimulSkillPanel::PanelKey = FName(TEXT("skill_tree"));

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

	TArray<FString> ToFStrings(const std::vector<std::string>& Values)
	{
		TArray<FString> Out;
		Out.Reserve(static_cast<int32>(Values.size()));
		for (const std::string& Value : Values)
		{
			Out.Add(ToFString(Value));
		}
		return Out;
	}

	FInsimulSkillNodeRow ToRow(const insimul::FSkillNodeView& Node)
	{
		FInsimulSkillNodeRow Row;
		Row.Id = ToFString(Node.Id);
		Row.Tree = ToFString(Node.Tree);
		Row.Label = ToFString(Node.Label);
		Row.Description = Node.bHasDescription ? ToFString(Node.Description) : FString();
		Row.Depth = static_cast<int32>(Node.Depth);
		Row.Cost = static_cast<int32>(Node.Cost);
		Row.Parents = ToFStrings(Node.Parents);
		Row.Requires = ToFStrings(Node.Requires);
		Row.Unmet = ToFStrings(Node.Unmet);
		Row.bTaken = Node.bTaken;
		Row.bAvailable = Node.bAvailable;
		Row.bConditional = Node.bConditional;
		Row.Refusal = ToFString(Node.Refusal);
		Row.Unlocks = ToFStrings(Node.Unlocks);
		Row.Permits = ToFStrings(Node.Permits);
		for (const insimul::FSkillModifierView& Modifier : Node.Modifies)
		{
			FInsimulSkillModifier Out;
			Out.Param = ToFString(Modifier.Param);
			Out.Amount = static_cast<float>(Modifier.Amount);
			Row.Modifies.Add(Out);
		}
		return Row;
	}

	FInsimulSkillTreeRow ToRow(const insimul::FSkillTreeView& Tree)
	{
		FInsimulSkillTreeRow Row;
		Row.Id = ToFString(Tree.Id);
		Row.Skill = ToFString(Tree.Skill);
		Row.Label = ToFString(Tree.Label);
		Row.Level = static_cast<int32>(Tree.Level);
		Row.MaxLevel = static_cast<int32>(Tree.MaxLevel);
		Row.bCapped = Tree.bCapped;
		Row.Banked = static_cast<int32>(Tree.Banked);
		Row.NextLevel = static_cast<int32>(Tree.NextLevel);
		Row.NextLevelPrice = static_cast<int32>(Tree.NextLevelPrice);
		Row.bAffordable = Tree.bAffordable;
		Row.Points = static_cast<int32>(Tree.Points);
		Row.Spent = static_cast<int32>(Tree.Spent);
		for (const insimul::FSkillNodeView& Node : Tree.Nodes)
		{
			Row.Nodes.Add(ToRow(Node));
		}
		for (const std::vector<std::string>& Line : Tree.Rows)
		{
			FInsimulSkillRow Laid;
			Laid.NodeIds = ToFStrings(Line);
			Row.Rows.Add(Laid);
		}
		for (const insimul::FSkillEdgeView& Edge : Tree.Edges)
		{
			FInsimulSkillEdge Out;
			Out.From = ToFString(Edge.From);
			Out.To = ToFString(Edge.To);
			Row.Edges.Add(Out);
		}
		Row.Taken = static_cast<int32>(Tree.Taken);
		Row.Total = static_cast<int32>(Tree.Total);
		return Row;
	}
}

UInsimulSkillPanel::UInsimulSkillPanel()
{
	Input = MakeUnique<insimul::FSkillViewInput>();
}

void UInsimulSkillPanel::NativeDestruct()
{
	Input.Reset();
	Super::NativeDestruct();
}

bool UInsimulSkillPanel::LoadFromJson(const FString& Json, FString& OutError)
{
	OutError.Reset();
	const insimul::FJsonParseResult Parsed = insimul::ParseJson(ToStd(Json));
	if (!Parsed.bOk || !Parsed.Root)
	{
		OutError = ToFString(Parsed.Error);
		UE_LOG(LogTemp, Warning, TEXT("Insimul skill panel: skills document is not JSON (%s)"),
			*OutError);
		return false;
	}

	if (!Input.IsValid())
	{
		Input = MakeUnique<insimul::FSkillViewInput>();
	}
	std::string Error;
	if (!insimul::FSkillViewInput::FromJson(*Parsed.Root, *Input, Error))
	{
		OutError = ToFString(Error);
		// A corrupt skills section is REPORTED: an empty panel and a world with no
		// trees must not render the same.
		UE_LOG(LogTemp, Warning, TEXT("Insimul skill panel: %s"), *OutError);
		return false;
	}
	Refresh();
	return true;
}

void UInsimulSkillPanel::Refresh()
{
	TreeRows.Reset();
	if (Input.IsValid())
	{
		for (const insimul::FSkillTreeView& Tree : insimul::FInsimulSkillTreeModel::BuildView(*Input))
		{
			TreeRows.Add(ToRow(Tree));
		}
	}
	Repaint();
	OnSkillTreeChanged.Broadcast();
}

FInsimulSkillTreeRow UInsimulSkillPanel::TreeById(const FString& TreeId) const
{
	for (const FInsimulSkillTreeRow& Row : TreeRows)
	{
		if (Row.Id == TreeId)
		{
			return Row;
		}
	}
	return FInsimulSkillTreeRow();
}

TArray<FString> UInsimulSkillPanel::TreesFundedBy(const FString& Skill) const
{
	if (!Input.IsValid())
	{
		return TArray<FString>();
	}
	return ToFStrings(insimul::FInsimulSkillTreeModel::TreesFundedBy(Input->Trees, ToStd(Skill)));
}

FString UInsimulSkillPanel::Describe() const
{
	int32 Nodes = 0;
	int32 Taken = 0;
	for (const FInsimulSkillTreeRow& Row : TreeRows)
	{
		Nodes += Row.Total;
		Taken += Row.Taken;
	}
	return FString::Printf(TEXT("skill panel: %d tree(s), %d node(s), %d taken"),
		TreeRows.Num(), Nodes, Taken);
}

void UInsimulSkillPanel::Repaint()
{
	if (EmptyPanelText)
	{
		// A world whose bundle did not select the skills module never resolves this
		// panel at all; an EMPTY panel means the world has no authored trees, which
		// is a different thing and says so.
		EmptyPanelText->SetText(TreeRows.Num() == 0
			? FText::FromString(TEXT("This world authors no skill trees."))
			: FText::GetEmpty());
	}

	if (!TreeListBox)
	{
		return;
	}
	TreeListBox->ClearChildren();
	for (const FInsimulSkillTreeRow& Tree : TreeRows)
	{
		for (const FInsimulSkillRow& Row : Tree.Rows)
		{
			for (const FString& NodeId : Row.NodeIds)
			{
				if (NodeWidgetClass)
				{
					if (UUserWidget* Node = CreateWidget<UUserWidget>(this, NodeWidgetClass))
					{
						TreeListBox->AddChildToVerticalBox(Node);
						continue;
					}
				}
				const FInsimulSkillNodeRow* Found = Tree.Nodes.FindByPredicate(
					[&NodeId](const FInsimulSkillNodeRow& Candidate) { return Candidate.Id == NodeId; });
				UTextBlock* Text = NewObject<UTextBlock>(this);
				Text->SetText(FText::FromString(Found ? Found->Label : NodeId));
				// A refused node is SHOWN and greyed, never hidden: the player is told
				// what exists, then why not.
				Text->SetIsEnabled(Found ? Found->bAvailable : false);
				TreeListBox->AddChildToVerticalBox(Text);
			}
		}
	}
}
