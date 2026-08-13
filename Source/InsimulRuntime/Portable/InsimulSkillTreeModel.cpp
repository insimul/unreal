// Copyright 2024 Insimul. All Rights Reserved.
//
// The skill panel's view-model. See InsimulSkillTreeModel.h for the design and for
// what this file deliberately does NOT decide.

#include "InsimulSkillTreeModel.h"

#include "InsimulCanonicalJson.h"

#include <algorithm>
#include <cmath>

namespace insimul {

namespace {

// ── JSON node factories (mirrors InsimulEquipmentModel.cpp) ─────────────────

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

FJsonValuePtr MakeNumber(double N, const std::string& Raw) {
	auto Node = std::make_shared<FJsonValue>();
	Node->Type = EJsonType::Number;
	Node->NumberValue = N;
	Node->RawNumber = Raw.empty() ? CanonicalNumber(N, std::string()) : Raw;
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

FJsonValuePtr StringArray(const std::vector<std::string>& Values) {
	auto Out = MakeArray();
	for (const std::string& Value : Values) {
		Out->ArrayItems.push_back(MakeString(Value));
	}
	return Out;
}

// ── Refusal names (core's SKILL_UNLOCK_REFUSALS, in its own order) ──────────

constexpr const char* RefusalOwned = "owned";
constexpr const char* RefusalPoints = "points";
constexpr const char* RefusalRequires = "requires";
constexpr const char* RefusalForbidden = "forbidden";

constexpr const char* EffectUnlocks = "unlocks";
constexpr const char* EffectModifies = "modifies";
constexpr const char* EffectPermits = "permits";

FSkillEffectArg ReadArg(const FJsonValue& Value) {
	FSkillEffectArg Out;
	if (Value.IsNumber()) {
		Out.bIsNumber = true;
		Out.Number = Value.NumberValue;
		Out.Text = Value.RawNumber;
		return Out;
	}
	Out.Text = Value.AsString();
	return Out;
}

std::vector<long long> ReadCurve(const FJsonValue* Array) {
	std::vector<long long> Out;
	if (!Array || !Array->IsArray()) {
		return Out;
	}
	for (const FJsonValuePtr& Entry : Array->ArrayItems) {
		if (Entry) {
			Out.push_back(Entry->AsInt(0));
		}
	}
	return Out;
}

std::vector<std::string> ReadStrings(const FJsonValue* Array) {
	std::vector<std::string> Out;
	if (!Array || !Array->IsArray()) {
		return Out;
	}
	for (const FJsonValuePtr& Entry : Array->ArrayItems) {
		if (Entry) {
			Out.push_back(Entry->AsString());
		}
	}
	return Out;
}

std::vector<std::pair<std::string, long long>> ReadNumberMap(const FJsonValue* Obj) {
	std::vector<std::pair<std::string, long long>> Out;
	if (!Obj || !Obj->IsObject()) {
		return Out;
	}
	for (const auto& Pair : Obj->ObjectItems) {
		Out.emplace_back(Pair.first, Pair.second ? Pair.second->AsInt(0) : 0);
	}
	return Out;
}

/** Insert into a sorted, deduplicated id list. */
void AddId(std::vector<std::string>& Ids, const std::string& Id) {
	if (std::find(Ids.begin(), Ids.end(), Id) == Ids.end()) {
		Ids.push_back(Id);
	}
}

/** The first argument of every `Kind(Arg)` effect, deduplicated and ordered. */
std::vector<std::string> AtomArguments(const std::vector<FSkillEffect>& Effects,
	const std::string& Kind) {
	std::vector<std::string> Out;
	for (const FSkillEffect& Effect : Effects) {
		if (Effect.Kind != Kind || Effect.Args.empty()) {
			continue;
		}
		AddId(Out, Effect.Args.front().AsAtom());
	}
	std::sort(Out.begin(), Out.end());
	return Out;
}

/**
 * Every `modifies(Param, Amount)` summed per parameter, keyed by the AUTHORED atom.
 * Additive and summed — two nodes that each modify `damage` contribute both numbers.
 * A non-numeric amount is IGNORED rather than coerced: `modifies(damage, big)` is
 * authored content that is wrong, and reading it as a NaN would poison every number
 * downstream of it.
 */
std::vector<FSkillModifierView> ModifierTotals(const std::vector<FSkillEffect>& Effects) {
	std::vector<FSkillModifierView> Out;
	for (const FSkillEffect& Effect : Effects) {
		if (Effect.Kind != EffectModifies || Effect.Args.empty()) {
			continue;
		}
		if (Effect.Args.size() < 2 || !Effect.Args[1].bIsNumber ||
			!std::isfinite(Effect.Args[1].Number)) {
			continue;
		}
		const std::string Param = Effect.Args.front().AsAtom();
		bool bFound = false;
		for (FSkillModifierView& Row : Out) {
			if (Row.Param == Param) {
				Row.Amount += Effect.Args[1].Number;
				bFound = true;
				break;
			}
		}
		if (!bFound) {
			FSkillModifierView Row;
			Row.Param = Param;
			Row.Amount = Effect.Args[1].Number;
			Out.push_back(Row);
		}
	}
	std::sort(Out.begin(), Out.end(),
		[](const FSkillModifierView& A, const FSkillModifierView& B) { return A.Param < B.Param; });
	return Out;
}

long long NodeDepthSeen(const FSkillTree& Tree, const std::string& NodeId,
	std::vector<std::string>& Seen) {
	if (std::find(Seen.begin(), Seen.end(), NodeId) != Seen.end()) {
		return 0;
	}
	const FSkillNode* Node = Tree.Find(NodeId);
	if (!Node || Node->Parents.empty()) {
		return 0;
	}
	Seen.push_back(NodeId);
	long long Deepest = 0;
	for (const std::string& Parent : Node->Parents) {
		const long long Candidate = 1 + NodeDepthSeen(Tree, Parent, Seen);
		if (Candidate > Deepest) {
			Deepest = Candidate;
		}
	}
	Seen.pop_back();
	return Deepest;
}

} // namespace

// ── Small accessors ─────────────────────────────────────────────────────────

std::string FSkillEffectArg::AsAtom() const {
	return bIsNumber ? CanonicalNumber(Number, Text) : Text;
}

const FSkillNode* FSkillTree::Find(const std::string& NodeId) const {
	for (const FSkillNode& Node : Nodes) {
		if (Node.Id == NodeId) {
			return &Node;
		}
	}
	return nullptr;
}

const FSkillNodeView* FSkillTreeView::Find(const std::string& NodeId) const {
	for (const FSkillNodeView& Node : Nodes) {
		if (Node.Id == NodeId) {
			return &Node;
		}
	}
	return nullptr;
}

long long FSkillViewActor::LevelOf(const std::string& Skill) const {
	for (const auto& Pair : Levels) {
		if (Pair.first == Skill) {
			return Pair.second;
		}
	}
	return 0;
}

long long FSkillViewActor::XpOf(const std::string& Skill) const {
	for (const auto& Pair : Xp) {
		if (Pair.first == Skill) {
			return Pair.second;
		}
	}
	return 0;
}

long long FSkillViewActor::PointsOf(const std::string& TreeId) const {
	for (const auto& Pair : Points) {
		if (Pair.first == TreeId) {
			return Pair.second;
		}
	}
	return 0;
}

bool FSkillViewActor::IsUnlocked(const std::string& NodeId) const {
	return std::find(Unlocked.begin(), Unlocked.end(), NodeId) != Unlocked.end();
}

bool FSkillViewActor::IsForbidden(const std::string& NodeId) const {
	return std::find(Forbidden.begin(), Forbidden.end(), NodeId) != Forbidden.end();
}

std::vector<std::string> FSkillViewActor::UnmetFor(const std::string& NodeId) const {
	for (const auto& Pair : Unmet) {
		if (Pair.first == NodeId) {
			return Pair.second;
		}
	}
	return std::vector<std::string>();
}

const FSkillDefinition* FSkillViewInput::FindSkill(const std::string& Id) const {
	for (const FSkillDefinition& Skill : Skills) {
		if (Skill.Id == Id) {
			return &Skill;
		}
	}
	return nullptr;
}

// ── Reading the authored half ───────────────────────────────────────────────

FSkillTuning FSkillTuning::FromJson(const FJsonValue& Tuning) {
	FSkillTuning Out;
	if (!Tuning.IsObject()) {
		return Out;
	}
	Out.PointsPerLevel = Tuning.GetInt("pointsPerLevel", Out.PointsPerLevel);
	Out.DefaultNodeCost = Tuning.GetInt("defaultNodeCost", Out.DefaultNodeCost);
	Out.DefaultMaxLevel = Tuning.GetInt("defaultMaxLevel", Out.DefaultMaxLevel);
	Out.LevelXp = ReadCurve(Tuning.Find("levelXp"));
	const std::string Advance = Tuning.GetString("advanceAction");
	if (!Advance.empty()) {
		Out.AdvanceAction = Advance;
	}
	const std::string Unlock = Tuning.GetString("unlockAction");
	if (!Unlock.empty()) {
		Out.UnlockAction = Unlock;
	}
	return Out;
}

bool FSkillViewInput::FromJson(const FJsonValue& Doc, FSkillViewInput& Out, std::string& OutError) {
	OutError.clear();
	if (!Doc.IsObject()) {
		OutError = "skill view input is not an object";
		return false;
	}
	const FJsonValue* Trees = Doc.Find("trees");
	if (!Trees || !Trees->IsArray()) {
		OutError = "skill view input has no trees array";
		return false;
	}

	Out = FSkillViewInput();
	for (const FJsonValuePtr& TreeNode : Trees->ArrayItems) {
		if (!TreeNode || !TreeNode->IsObject()) {
			OutError = "a tree row is not an object";
			return false;
		}
		FSkillTree Tree;
		Tree.Id = TreeNode->GetString("id");
		if (Tree.Id.empty()) {
			OutError = "a tree row has no id";
			return false;
		}
		Tree.Skill = TreeNode->GetString("skill");
		if (const FJsonValue* Name = TreeNode->Find("name")) {
			if (Name->IsString()) {
				Tree.bHasName = true;
				Tree.Name = Name->StringValue;
			}
		}
		if (const FJsonValue* Nodes = TreeNode->Find("nodes")) {
			for (const FJsonValuePtr& NodeValue : Nodes->ArrayItems) {
				if (!NodeValue || !NodeValue->IsObject()) {
					continue;
				}
				FSkillNode Node;
				Node.Id = NodeValue->GetString("id");
				if (Node.Id.empty()) {
					OutError = "a node of tree '" + Tree.Id + "' has no id";
					return false;
				}
				Node.Tree = NodeValue->GetString("tree");
				if (Node.Tree.empty()) {
					Node.Tree = Tree.Id;
				}
				if (const FJsonValue* Name = NodeValue->Find("name")) {
					if (Name->IsString()) {
						Node.bHasName = true;
						Node.Name = Name->StringValue;
					}
				}
				if (const FJsonValue* Description = NodeValue->Find("description")) {
					if (Description->IsString()) {
						Node.bHasDescription = true;
						Node.Description = Description->StringValue;
					}
				}
				if (const FJsonValue* Cost = NodeValue->Find("cost")) {
					if (Cost->IsNumber()) {
						Node.bHasCost = true;
						Node.Cost = Cost->AsInt(0);
					}
				}
				Node.Parents = ReadStrings(NodeValue->Find("parents"));
				Node.Requires = ReadStrings(NodeValue->Find("requires"));
				if (const FJsonValue* Effects = NodeValue->Find("effects")) {
					for (const FJsonValuePtr& EffectValue : Effects->ArrayItems) {
						if (!EffectValue || !EffectValue->IsObject()) {
							continue;
						}
						FSkillEffect Effect;
						Effect.Kind = EffectValue->GetString("kind");
						if (const FJsonValue* Args = EffectValue->Find("args")) {
							for (const FJsonValuePtr& Arg : Args->ArrayItems) {
								if (Arg) {
									Effect.Args.push_back(ReadArg(*Arg));
								}
							}
						}
						Node.Effects.push_back(Effect);
					}
				}
				Tree.Nodes.push_back(Node);
			}
		}
		Out.Trees.push_back(Tree);
	}

	if (const FJsonValue* Skills = Doc.Find("skills")) {
		for (const FJsonValuePtr& SkillValue : Skills->ArrayItems) {
			if (!SkillValue || !SkillValue->IsObject()) {
				continue;
			}
			FSkillDefinition Skill;
			Skill.Id = SkillValue->GetString("id");
			Skill.Category = SkillValue->GetString("category");
			if (const FJsonValue* MaxLevel = SkillValue->Find("maxLevel")) {
				if (MaxLevel->IsNumber()) {
					Skill.bHasMaxLevel = true;
					Skill.MaxLevel = MaxLevel->AsInt(0);
				}
			}
			Skill.LevelXp = ReadCurve(SkillValue->Find("levelXp"));
			if (const FJsonValue* Requires = SkillValue->Find("requires")) {
				for (const FJsonValuePtr& Row : Requires->ArrayItems) {
					if (!Row || !Row->IsObject()) {
						continue;
					}
					FSkillPrereq Prereq;
					Prereq.Skill = Row->GetString("skill");
					Prereq.Level = Row->GetInt("level", 0);
					Skill.Requires.push_back(Prereq);
				}
			}
			Out.Skills.push_back(Skill);
		}
	}

	if (const FJsonValue* Tuning = Doc.Find("tuning")) {
		Out.Tuning = FSkillTuning::FromJson(*Tuning);
	}

	if (const FJsonValue* Actor = Doc.Find("actor")) {
		if (Actor->IsObject()) {
			Out.Actor.Id = Actor->GetString("id");
			Out.Actor.Levels = ReadNumberMap(Actor->Find("levels"));
			Out.Actor.Xp = ReadNumberMap(Actor->Find("xp"));
			Out.Actor.Points = ReadNumberMap(Actor->Find("points"));
			Out.Actor.Unlocked = ReadStrings(Actor->Find("unlocked"));
			Out.Actor.Forbidden = ReadStrings(Actor->Find("forbidden"));
			if (const FJsonValue* Unmet = Actor->Find("unmet")) {
				if (Unmet->IsObject()) {
					for (const auto& Pair : Unmet->ObjectItems) {
						Out.Actor.Unmet.emplace_back(Pair.first, ReadStrings(Pair.second.get()));
					}
				}
			}
		}
	}
	return true;
}

// ── The derivation ──────────────────────────────────────────────────────────

long long FInsimulSkillTreeModel::NodeCost(const FSkillNode& Node, const FSkillTuning& Tuning) {
	const long long Cost = Node.bHasCost ? Node.Cost : Tuning.DefaultNodeCost;
	return Cost > 0 ? Cost : 0;
}

long long FInsimulSkillTreeModel::MaxLevelOf(const FSkillDefinition* Skill,
	const FSkillTuning& Tuning) {
	return (Skill && Skill->bHasMaxLevel) ? Skill->MaxLevel : Tuning.DefaultMaxLevel;
}

long long FInsimulSkillTreeModel::XpForLevel(const FSkillDefinition* Skill, long long Level,
	const FSkillTuning& Tuning) {
	const std::vector<long long>& Curve =
		(Skill && !Skill->LevelXp.empty()) ? Skill->LevelXp : Tuning.LevelXp;
	if (Curve.empty() || Level <= 0) {
		return 0;
	}
	const std::size_t Index = static_cast<std::size_t>(Level);
	const long long Price = Index < Curve.size() ? Curve[Index] : Curve.back();
	return Price > 0 ? Price : 0;
}

std::vector<std::string> FInsimulSkillTreeModel::NodeRequirements(const FSkillNode& Node) {
	std::vector<std::string> Goals;
	for (const std::string& Parent : Node.Parents) {
		AddId(Goals, "skill_unlocked(Actor, " + Parent + ")");
	}
	for (const std::string& Goal : Node.Requires) {
		AddId(Goals, Goal);
	}
	std::sort(Goals.begin(), Goals.end());
	return Goals;
}

long long FInsimulSkillTreeModel::NodeDepth(const FSkillTree& Tree, const std::string& NodeId) {
	std::vector<std::string> Seen;
	return NodeDepthSeen(Tree, NodeId, Seen);
}

std::vector<std::pair<std::string, long long>> FInsimulSkillTreeModel::Depths(
	const std::vector<FSkillTree>& Trees) {
	std::vector<std::pair<std::string, long long>> Out;
	for (const FSkillTree& Tree : Trees) {
		for (const FSkillNode& Node : Tree.Nodes) {
			Out.emplace_back(Node.Id, NodeDepth(Tree, Node.Id));
		}
	}
	return Out;
}

std::vector<std::string> FInsimulSkillTreeModel::TreesFundedBy(
	const std::vector<FSkillTree>& Trees, const std::string& Skill) {
	std::vector<std::string> Out;
	for (const FSkillTree& Tree : Trees) {
		if (Tree.Skill == Skill) {
			Out.push_back(Tree.Id);
		}
	}
	std::sort(Out.begin(), Out.end());
	return Out;
}

std::vector<FSkillTreeView> FInsimulSkillTreeModel::BuildView(const FSkillViewInput& In) {
	std::vector<FSkillTreeView> Out;
	Out.reserve(In.Trees.size());

	for (const FSkillTree& Tree : In.Trees) {
		const FSkillDefinition* Definition = In.FindSkill(Tree.Skill);

		FSkillTreeView View;
		View.Id = Tree.Id;
		View.Skill = Tree.Skill;
		View.Label = Tree.bHasName ? Tree.Name : Tree.Id;
		View.Level = In.Actor.LevelOf(Tree.Skill);
		View.MaxLevel = MaxLevelOf(Definition, In.Tuning);
		View.bCapped = View.Level >= View.MaxLevel;
		View.Banked = In.Actor.XpOf(Tree.Skill);
		View.NextLevel = View.bCapped ? View.Level : View.Level + 1;
		View.NextLevelPrice = View.bCapped ? 0 : XpForLevel(Definition, View.NextLevel, In.Tuning);
		View.bAffordable = !View.bCapped && View.Banked >= View.NextLevelPrice;
		View.Points = In.Actor.PointsOf(Tree.Id);

		for (const FSkillNode& Node : Tree.Nodes) {
			FSkillNodeView NodeView;
			NodeView.Id = Node.Id;
			NodeView.Tree = Node.Tree.empty() ? Tree.Id : Node.Tree;
			NodeView.Label = Node.bHasName ? Node.Name : Node.Id;
			NodeView.bHasDescription = Node.bHasDescription;
			NodeView.Description = Node.Description;
			NodeView.Depth = NodeDepth(Tree, Node.Id);
			NodeView.Cost = NodeCost(Node, In.Tuning);
			NodeView.Parents = Node.Parents;
			NodeView.Requires = NodeRequirements(Node);
			NodeView.Unmet = In.Actor.UnmetFor(Node.Id);
			NodeView.bTaken = In.Actor.IsUnlocked(Node.Id);
			NodeView.bConditional = !NodeView.Requires.empty();
			NodeView.Effects = Node.Effects;
			NodeView.Unlocks = AtomArguments(Node.Effects, EffectUnlocks);
			NodeView.Permits = AtomArguments(Node.Effects, EffectPermits);
			NodeView.Modifies = ModifierTotals(Node.Effects);

			// The refusal ladder, in core's SKILL_UNLOCK_REFUSALS order. The first two
			// rungs a pure function settles alone; the last two are the KB's answers,
			// layered on rather than guessed (see the header).
			if (NodeView.bTaken) {
				NodeView.bAvailable = false;
				NodeView.Refusal = RefusalOwned;
			} else if (View.Points < NodeView.Cost) {
				NodeView.bAvailable = false;
				NodeView.Refusal = RefusalPoints;
			} else if (!NodeView.Unmet.empty()) {
				NodeView.bAvailable = false;
				NodeView.Refusal = RefusalRequires;
			} else if (In.Actor.IsForbidden(Node.Id)) {
				NodeView.bAvailable = false;
				NodeView.Refusal = RefusalForbidden;
			} else {
				NodeView.bAvailable = true;
			}

			View.Nodes.push_back(NodeView);
		}

		// Rows: node ids by depth, row 0 first. Derived from the authored edges,
		// which is the whole reason NodeDepth exists rather than a `tier` field.
		std::vector<long long> Depths;
		for (const FSkillNodeView& Node : View.Nodes) {
			if (std::find(Depths.begin(), Depths.end(), Node.Depth) == Depths.end()) {
				Depths.push_back(Node.Depth);
			}
		}
		std::sort(Depths.begin(), Depths.end());
		for (long long Depth : Depths) {
			std::vector<std::string> Row;
			for (const FSkillNodeView& Node : View.Nodes) {
				if (Node.Depth == Depth) {
					Row.push_back(Node.Id);
				}
			}
			std::sort(Row.begin(), Row.end());
			View.Rows.push_back(Row);
		}

		for (const FSkillNodeView& Node : View.Nodes) {
			for (const std::string& Parent : Node.Parents) {
				FSkillEdgeView Edge;
				Edge.From = Parent;
				Edge.To = Node.Id;
				View.Edges.push_back(Edge);
			}
		}
		std::sort(View.Edges.begin(), View.Edges.end(),
			[](const FSkillEdgeView& A, const FSkillEdgeView& B) {
				return A.From != B.From ? A.From < B.From : A.To < B.To;
			});

		for (const FSkillNodeView& Node : View.Nodes) {
			if (Node.bTaken) {
				View.Spent += Node.Cost;
				++View.Taken;
			}
		}
		View.Total = static_cast<long long>(View.Nodes.size());
		Out.push_back(View);
	}
	return Out;
}

// ── The projection the corpus pins ──────────────────────────────────────────

FJsonValuePtr FInsimulSkillTreeModel::ToProjection(const std::vector<FSkillTreeView>& View) {
	auto Root = MakeArray();
	for (const FSkillTreeView& Tree : View) {
		auto TreeNode = MakeObject();
		ObjSet(*TreeNode, "id", MakeString(Tree.Id));
		ObjSet(*TreeNode, "skill", MakeString(Tree.Skill));
		ObjSet(*TreeNode, "label", MakeString(Tree.Label));
		ObjSet(*TreeNode, "level", MakeInt(Tree.Level));
		ObjSet(*TreeNode, "maxLevel", MakeInt(Tree.MaxLevel));
		ObjSet(*TreeNode, "capped", MakeBool(Tree.bCapped));
		ObjSet(*TreeNode, "banked", MakeInt(Tree.Banked));
		ObjSet(*TreeNode, "nextLevel", MakeInt(Tree.NextLevel));
		ObjSet(*TreeNode, "nextLevelPrice", MakeInt(Tree.NextLevelPrice));
		ObjSet(*TreeNode, "affordable", MakeBool(Tree.bAffordable));
		ObjSet(*TreeNode, "points", MakeInt(Tree.Points));
		ObjSet(*TreeNode, "spent", MakeInt(Tree.Spent));

		auto Nodes = MakeArray();
		for (const FSkillNodeView& Node : Tree.Nodes) {
			auto NodeValue = MakeObject();
			ObjSet(*NodeValue, "id", MakeString(Node.Id));
			ObjSet(*NodeValue, "tree", MakeString(Node.Tree));
			ObjSet(*NodeValue, "label", MakeString(Node.Label));
			if (Node.bHasDescription) {
				ObjSet(*NodeValue, "description", MakeString(Node.Description));
			}
			ObjSet(*NodeValue, "depth", MakeInt(Node.Depth));
			ObjSet(*NodeValue, "cost", MakeInt(Node.Cost));
			ObjSet(*NodeValue, "parents", StringArray(Node.Parents));
			ObjSet(*NodeValue, "requires", StringArray(Node.Requires));
			ObjSet(*NodeValue, "unmet", StringArray(Node.Unmet));
			ObjSet(*NodeValue, "taken", MakeBool(Node.bTaken));
			ObjSet(*NodeValue, "available", MakeBool(Node.bAvailable));
			ObjSet(*NodeValue, "conditional", MakeBool(Node.bConditional));

			auto Effects = MakeArray();
			for (const FSkillEffect& Effect : Node.Effects) {
				auto EffectValue = MakeObject();
				ObjSet(*EffectValue, "kind", MakeString(Effect.Kind));
				auto Args = MakeArray();
				for (const FSkillEffectArg& Arg : Effect.Args) {
					Args->ArrayItems.push_back(Arg.bIsNumber ? MakeNumber(Arg.Number, Arg.Text)
															 : MakeString(Arg.Text));
				}
				ObjSet(*EffectValue, "args", Args);
				Effects->ArrayItems.push_back(EffectValue);
			}
			ObjSet(*NodeValue, "effects", Effects);
			ObjSet(*NodeValue, "unlocks", StringArray(Node.Unlocks));
			ObjSet(*NodeValue, "permits", StringArray(Node.Permits));

			auto Modifies = MakeArray();
			for (const FSkillModifierView& Modifier : Node.Modifies) {
				auto Row = MakeObject();
				ObjSet(*Row, "param", MakeString(Modifier.Param));
				ObjSet(*Row, "amount", MakeNumber(Modifier.Amount, std::string()));
				Modifies->ArrayItems.push_back(Row);
			}
			ObjSet(*NodeValue, "modifies", Modifies);
			if (!Node.Refusal.empty()) {
				ObjSet(*NodeValue, "refusal", MakeString(Node.Refusal));
			}
			Nodes->ArrayItems.push_back(NodeValue);
		}
		ObjSet(*TreeNode, "nodes", Nodes);

		auto Rows = MakeArray();
		for (const std::vector<std::string>& Row : Tree.Rows) {
			Rows->ArrayItems.push_back(StringArray(Row));
		}
		ObjSet(*TreeNode, "rows", Rows);

		auto Edges = MakeArray();
		for (const FSkillEdgeView& Edge : Tree.Edges) {
			auto EdgeValue = MakeObject();
			ObjSet(*EdgeValue, "from", MakeString(Edge.From));
			ObjSet(*EdgeValue, "to", MakeString(Edge.To));
			Edges->ArrayItems.push_back(EdgeValue);
		}
		ObjSet(*TreeNode, "edges", Edges);

		ObjSet(*TreeNode, "taken", MakeInt(Tree.Taken));
		ObjSet(*TreeNode, "total", MakeInt(Tree.Total));
		Root->ArrayItems.push_back(TreeNode);
	}
	return Root;
}

std::string FInsimulSkillTreeModel::ProjectionCanonical(const std::vector<FSkillTreeView>& View) {
	const FJsonValuePtr Projection = ToProjection(View);
	return CanonicalJsonStringify(*Projection);
}

} // namespace insimul
