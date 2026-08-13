// Copyright 2024 Insimul. All Rights Reserved.
//
// FInsimulSkillTreeModel — the skill panel's view-model (US-2 of tasklist 190),
// panel key `skill_tree`.
//
// A VALUE, NOT A CALLBACK. core's module contract §3 forbids a UI hook on the C
// ABI, so what a skill panel needs from the skills module is not an interface this
// plugin implements but a value it is handed: rows of nodes, edges between them, a
// label and a price on each, the state that greys the unaffordable ones out and the
// "why not" beside the refused ones. Every one of those is derivable from the
// world's authored trees plus the actor's per-playthrough state, and this file is
// that derivation — the C++ twin of core's `skills/skill-view.ts`, so the Babylon
// reference and the three native ports draw ONE tree from one save instead of four
// panels each deciding for themselves what a row is.
//
// NOTHING HERE KNOWS A NODE. There is no node list, no tier table, no icon map and
// no layout constant: a row is DERIVED from the authored parent edges (NodeDepth),
// a label falls back to the node's own id so a half-authored tree is still
// inspectable, and an effect is reported as the term the world wrote. The effect
// kind set is OPEN — `sings(the_masons_round)` is not an error and is not dropped;
// it rides through as authored for a rule to read, and every function here ignores
// it. A switch over a closed set of kinds would be exactly the fork the skills
// module exists to avoid.
//
// THE TWO ANSWERS THIS FILE MAY NOT GIVE. `Unmet` (which authored goals the rules
// layer did not satisfy) and `Forbidden` (what `permissible/3` refused) are HANDED
// IN on the actor, never computed here, for the reason every pure function in this
// area refuses to call a KB: an authored `skill_node_requires/2` goal can name a
// reputation, a faction, a genealogy or a quest, and inventing a verdict for one
// would be inventing the answer. A caller with no KB passes neither and gets a view
// whose gated nodes read `conditional` — "core has nothing against this, and
// something core cannot evaluate might".
//
// REFUSALS IN ONE ORDER. unknown -> owned -> points -> requires -> forbidden, which
// is core's SKILL_UNLOCK_REFUSALS: what a pure function settles alone first, then
// the authored goals the KB answers for, then permissibility. A panel that greyed a
// node out for a different reason than the one an unlock would refuse it with is a
// UI that lies.
//
// DETERMINISTIC. Trees and nodes stay in AUTHORING order, because that order is a
// creator's decision and a panel that re-sorted it alphabetically would have thrown
// the design away; everything DERIVED — rows, edges, requirements, effect summaries
// — is sorted by id, because two engines granting the same panel two orders is a
// divergence nobody authored.
//
// The semantics are `conformance/skills/trees.json`'s (band 123, US-3), which every
// engine leg runs; ctest `ui_skill_tree` diffs the canonical projection of all six
// cases byte for byte, plus the `funded` and `depths` read-outs.
//
// READ-ONLY BY DESIGN, like the equipment model next door: taking a node is the
// skills module's decision layer, not the UI's. That is also why this panel cannot
// violate the state-location invariant — it never writes. The per-playthrough half
// of its input (levels, banked XP, pools, taken nodes) is read out of
// save.currentState by the caller; see InsimulUIStateBinding.h.
//
// std-only (no Unreal Engine, no CoreMinimal.h). The UMG seam is
// UInsimulSkillPanel (Public/InsimulSkillPanel.h), syntax-gated only.

#pragma once

#include "InsimulJson.h"

#include <string>
#include <utility>
#include <vector>

namespace insimul {

/** One argument of an authored effect term, as the world wrote it. */
struct FSkillEffectArg {
	bool bIsNumber = false;
	/** The string value, or the number's original lexeme. */
	std::string Text;
	double Number = 0.0;

	/** What `String(arg)` reads as — the atom a host prints. */
	std::string AsAtom() const;
};

/** `skill_node_effect/2` — an authored term. The kind set is OPEN. */
struct FSkillEffect {
	std::string Kind;
	std::vector<FSkillEffectArg> Args;
};

/** One authored node of one tree. */
struct FSkillNode {
	std::string Id;
	/** The tree it hangs off — filled in from the owning tree when absent. */
	std::string Tree;
	bool bHasName = false;
	std::string Name;
	bool bHasDescription = false;
	std::string Description;
	/** Authored cost; absent means the world's default node cost. */
	bool bHasCost = false;
	long long Cost = 0;
	/** Authored parent edges, in authoring order. */
	std::vector<std::string> Parents;
	/** Authored `skill_node_requires/2` goals, in authoring order. */
	std::vector<std::string> Requires;
	std::vector<FSkillEffect> Effects;
};

/** One authored tree: the skill whose levels FUND it, and its nodes. */
struct FSkillTree {
	std::string Id;
	std::string Skill;
	bool bHasName = false;
	std::string Name;
	std::vector<FSkillNode> Nodes;

	const FSkillNode* Find(const std::string& NodeId) const;
};

/** A `skill_requires/3` row: another skill at a level. */
struct FSkillPrereq {
	std::string Skill;
	long long Level = 0;
};

/** One authored skill — the cap and the curve the tree's header prices from. */
struct FSkillDefinition {
	std::string Id;
	std::string Category;
	bool bHasMaxLevel = false;
	long long MaxLevel = 0;
	/** `skill_level_xp/3` — a PRICE per level, never a running total. */
	std::vector<long long> LevelXp;
	std::vector<FSkillPrereq> Requires;
};

/** The world's resolved skill dials. Every number a panel prints comes from here. */
struct FSkillTuning {
	long long PointsPerLevel = 1;
	long long DefaultNodeCost = 1;
	long long DefaultMaxLevel = 10;
	std::vector<long long> LevelXp;
	/** The world's own action atoms — never hard-coded by a panel. */
	std::string AdvanceAction = "train_skill";
	std::string UnlockAction = "unlock_skill_node";

	static FSkillTuning FromJson(const FJsonValue& Tuning);
};

/**
 * One actor's progression as a save carries it, plus the two answers only the KB
 * can give (see the header note — they are handed in, never computed).
 */
struct FSkillViewActor {
	std::string Id;
	/** `has_skill(Actor, SkillId, Level)`. */
	std::vector<std::pair<std::string, long long>> Levels;
	/** `skill_xp/3` — banked, unspent. */
	std::vector<std::pair<std::string, long long>> Xp;
	/** `skill_points/3` — per TREE. */
	std::vector<std::pair<std::string, long long>> Points;
	/** `skill_unlocked/2`. */
	std::vector<std::string> Unlocked;
	/** Per node id, the authored goals the KB did NOT satisfy. */
	std::vector<std::pair<std::string, std::vector<std::string>>> Unmet;
	/** Node ids `permissible/3` refused — a norm, a law, a guild's own rules. */
	std::vector<std::string> Forbidden;

	long long LevelOf(const std::string& Skill) const;
	long long XpOf(const std::string& Skill) const;
	long long PointsOf(const std::string& TreeId) const;
	bool IsUnlocked(const std::string& NodeId) const;
	bool IsForbidden(const std::string& NodeId) const;
	/** The unmet goals for a node, or an empty list when the KB said nothing. */
	std::vector<std::string> UnmetFor(const std::string& NodeId) const;
};

/** Everything the view is built from. */
struct FSkillViewInput {
	std::vector<FSkillTree> Trees;
	std::vector<FSkillDefinition> Skills;
	FSkillTuning Tuning;
	FSkillViewActor Actor;

	/**
	 * Read the authored half + the actor out of one document (the shape
	 * `WorldIR.skills` carries and the shared corpus pins). Returns false with
	 * OutError on a document that is not one — a build whose skill data is corrupt
	 * must say so, not quietly draw an empty panel.
	 */
	static bool FromJson(const FJsonValue& Doc, FSkillViewInput& Out, std::string& OutError);

	const FSkillDefinition* FindSkill(const std::string& Id) const;
};

/** One `modifies(Param, Amount)` total, ready to print. */
struct FSkillModifierView {
	/** The AUTHORED atom, never a translated field name. */
	std::string Param;
	double Amount = 0.0;
};

/** One node, with everything a panel needs to draw it and nothing else. */
struct FSkillNodeView {
	std::string Id;
	std::string Tree;
	/** The authored name, or the node's own id. */
	std::string Label;
	bool bHasDescription = false;
	std::string Description;
	/** Which row it sits on — derived from the authored edges, never authored. */
	long long Depth = 0;
	long long Cost = 0;
	std::vector<std::string> Parents;
	/** Every goal it asks, parents desugared. */
	std::vector<std::string> Requires;
	/** The subset the KB did not satisfy. Empty when unknown. */
	std::vector<std::string> Unmet;
	bool bTaken = false;
	bool bAvailable = false;
	bool bConditional = false;
	/** Why not, in the refusal order. Empty when available. */
	std::string Refusal;
	/** What taking it does, as the authored terms the KB carries. */
	std::vector<FSkillEffect> Effects;
	std::vector<std::string> Unlocks;
	std::vector<std::string> Permits;
	std::vector<FSkillModifierView> Modifies;
};

/** One authored edge, for a host that draws lines between boxes. */
struct FSkillEdgeView {
	std::string From;
	std::string To;
};

/** One tree, as a panel renders it. */
struct FSkillTreeView {
	std::string Id;
	std::string Skill;
	std::string Label;
	long long Level = 0;
	long long MaxLevel = 0;
	bool bCapped = false;
	long long Banked = 0;
	long long NextLevel = 0;
	long long NextLevelPrice = 0;
	bool bAffordable = false;
	long long Points = 0;
	long long Spent = 0;
	/** Every node, in AUTHORING order. */
	std::vector<FSkillNodeView> Nodes;
	/** Node ids by depth, row 0 first — a host lays rows out without deriving them. */
	std::vector<std::vector<std::string>> Rows;
	/** Parent -> child, canonically ordered. */
	std::vector<FSkillEdgeView> Edges;
	long long Taken = 0;
	long long Total = 0;

	const FSkillNodeView* Find(const std::string& NodeId) const;
};

/**
 * The derivation. Pure, total and canonically ordered: it performs no IO, calls no
 * KB and reads no clock, so a host may ask for it whenever the player opens the
 * screen and a test may pin it.
 */
class FInsimulSkillTreeModel {
public:
	/** The whole panel: every authored tree, with every node's state for one actor. */
	static std::vector<FSkillTreeView> BuildView(const FSkillViewInput& In);

	/** Which trees a level in this skill funds — `skill_tree(TreeId, SkillId)` read
	 *  backwards. Canonically ordered. */
	static std::vector<std::string> TreesFundedBy(const std::vector<FSkillTree>& Trees,
		const std::string& Skill);

	/** How deep a node sits — 0 for a root, otherwise one past its deepest parent.
	 *  Cycle-safe, because an authored tree is content and content can be wrong. */
	static long long NodeDepth(const FSkillTree& Tree, const std::string& NodeId);

	/** Every node's depth across every tree, for a host that lays out one canvas. */
	static std::vector<std::pair<std::string, long long>> Depths(
		const std::vector<FSkillTree>& Trees);

	/** The authored goals plus one `skill_unlocked(Actor, Parent)` per parent,
	 *  deduplicated and canonically ordered — ONE gate to evaluate. */
	static std::vector<std::string> NodeRequirements(const FSkillNode& Node);

	/** What the node costs its tree's pool. */
	static long long NodeCost(const FSkillNode& Node, const FSkillTuning& Tuning);

	/** The cap this skill is held to. */
	static long long MaxLevelOf(const FSkillDefinition* Skill, const FSkillTuning& Tuning);

	/** What reaching `Level` prices — a level past the end of the curve repeats the
	 *  last entry rather than becoming free. */
	static long long XpForLevel(const FSkillDefinition* Skill, long long Level,
		const FSkillTuning& Tuning);

	/** The projection the corpus pins (the `view` array). */
	static FJsonValuePtr ToProjection(const std::vector<FSkillTreeView>& View);

	/** Canonical JSON of that projection — byte-comparable with the corpus. */
	static std::string ProjectionCanonical(const std::vector<FSkillTreeView>& View);
};

} // namespace insimul
