// Copyright 2024 Insimul. All Rights Reserved.
//
// test_skill_tree.cpp — host gate for the skill panel (US-2 of tasklist 190,
// panel key `skill_tree`).
//
// THE CLAIM. A skill panel is a VALUE core returns, not a callback core invokes
// (core's module contract §3), so what the four engine legs share is the
// view-model in `conformance/skills/trees.json` and what differs is how each of
// them paints it. This gate drives all six cases of that corpus through
// FInsimulSkillTreeModel and diffs the canonical projection byte for byte —
// every node's row, price, state and "why not", the rows a host lays out from,
// the edges it draws lines along, the tree header's level / cap / bank / pool,
// plus the two read-outs beside the view (`funded`, which is
// `skill_tree(TreeId, SkillId)` read backwards, and `depths`).
//
// The corpus's own teeth are the cases that would catch a port that guessed: a
// brand-new character whose only available node is the one the world priced at
// nothing (an empty pool is not "nothing is possible"), a capped skill whose
// price is zeroed rather than invented, a world that tunes every number so not
// one of them is core's, the two rungs only a KB can answer arriving from
// outside, and a world with no trees at all — the shape of every world whose
// genre bundle did not select this module.
//
// Negative controls are mandatory in this repo (CLAUDE.md): each block below
// carries trials that prove it can fail — funding the pool moves a `points`
// refusal, dropping the KB's answers moves `requires` and `forbidden`, an
// authored parent edge moves a node's row, and a cyclic tree terminates.
//
// The corpus dir comes from the compile definition the ctest target sets; argv[1]
// overrides it.

#include "../Portable/InsimulCanonicalJson.h"
#include "../Portable/InsimulJson.h"
#include "../Portable/InsimulSkillTreeModel.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace insimul;

namespace {

int g_pass = 0;
int g_fail = 0;

void Report(const std::string& Name, bool bOk, const std::string& Detail = "") {
	std::printf("  %s  %-62s%s%s\n", bOk ? "PASS" : "FAIL", Name.c_str(),
			Detail.empty() ? "" : "  ", Detail.c_str());
	if (bOk) {
		g_pass++;
	} else {
		g_fail++;
	}
}

std::string ReadFile(const std::string& Path) {
	std::ifstream In(Path, std::ios::binary);
	if (!In) {
		return std::string();
	}
	std::ostringstream Ss;
	Ss << In.rdbuf();
	return Ss.str();
}

FJsonValuePtr LoadJson(const std::string& Path) {
	const std::string Text = ReadFile(Path);
	if (Text.empty()) {
		return nullptr;
	}
	FJsonParseResult Parsed = ParseJson(Text);
	return Parsed.bOk ? Parsed.Root : nullptr;
}

/** `{"trees":[…],"skills":[…],"tuning":{…},"actor":{…}}` parsed, or a failure. */
bool ReadCaseInput(const FJsonValue& Case, FSkillViewInput& Out, std::string& OutError) {
	const FJsonValue* Input = Case.Find("input");
	if (!Input) {
		OutError = "case has no input";
		return false;
	}
	return FSkillViewInput::FromJson(*Input, Out, OutError);
}

std::string CanonicalOf(const FJsonValue* Value) {
	return Value ? CanonicalJsonStringify(*Value) : std::string();
}

/** The `depths` read-out as the corpus writes it: node id -> depth, one object. */
std::string CanonicalDepths(const std::vector<std::pair<std::string, long long>>& Depths) {
	FJsonValue Object;
	Object.Type = EJsonType::Object;
	for (const auto& Pair : Depths) {
		auto Number = std::make_shared<FJsonValue>();
		Number->Type = EJsonType::Number;
		Number->NumberValue = static_cast<double>(Pair.second);
		Number->RawNumber = std::to_string(Pair.second);
		Object.ObjectItems.emplace_back(Pair.first, Number);
	}
	return CanonicalJsonStringify(Object);
}

std::string CanonicalIds(const std::vector<std::string>& Ids) {
	FJsonValue Array;
	Array.Type = EJsonType::Array;
	for (const std::string& Id : Ids) {
		auto Node = std::make_shared<FJsonValue>();
		Node->Type = EJsonType::String;
		Node->StringValue = Id;
		Array.ArrayItems.push_back(Node);
	}
	return CanonicalJsonStringify(Array);
}

/** The refusal on one node of one tree, or "" when it is available. */
std::string RefusalOf(const std::vector<FSkillTreeView>& View, const std::string& TreeId,
	const std::string& NodeId) {
	for (const FSkillTreeView& Tree : View) {
		if (Tree.Id != TreeId) {
			continue;
		}
		if (const FSkillNodeView* Node = Tree.Find(NodeId)) {
			return Node->bAvailable ? std::string() : Node->Refusal;
		}
	}
	return "<no such node>";
}

// ── The corpus ──────────────────────────────────────────────────────────────

void RunCorpus(const std::string& SkillsDir, std::vector<FSkillViewInput>& OutInputs) {
	const FJsonValuePtr Doc = LoadJson(SkillsDir + "/trees.json");
	if (!Doc) {
		Report("trees: corpus is readable", false, SkillsDir + "/trees.json");
		return;
	}
	const FJsonValue* Cases = Doc->Find("cases");
	if (!Cases || !Cases->IsArray() || Cases->ArrayItems.empty()) {
		Report("trees: corpus carries cases", false);
		return;
	}
	Report("trees: corpus is readable", true,
		std::to_string(Cases->ArrayItems.size()) + " case(s)");

	// What the corpus is supposed to exercise, measured rather than assumed.
	bool bSawOwned = false;
	bool bSawPoints = false;
	bool bSawRequires = false;
	bool bSawForbidden = false;
	bool bSawCapped = false;
	bool bSawEmptyWorld = false;

	for (const FJsonValuePtr& CaseValue : Cases->ArrayItems) {
		if (!CaseValue || !CaseValue->IsObject()) {
			Report("trees: a case is an object", false);
			continue;
		}
		const std::string Name = CaseValue->GetString("name");

		FSkillViewInput Input;
		std::string Error;
		if (!ReadCaseInput(*CaseValue, Input, Error)) {
			Report("trees: " + Name, false, Error);
			continue;
		}

		const FJsonValue* Expected = CaseValue->Find("expected");
		if (!Expected) {
			Report("trees: " + Name, false, "case has no expected view");
			continue;
		}

		const std::vector<FSkillTreeView> View = FInsimulSkillTreeModel::BuildView(Input);
		const std::string Got = FInsimulSkillTreeModel::ProjectionCanonical(View);
		const std::string Want = CanonicalOf(Expected->Find("view"));
		Report("trees: " + Name, Got == Want, Got == Want ? "" : "projection differs");

		// `funded` — skill_tree(TreeId, SkillId) read backwards, for the skill the
		// first authored tree hangs off (the corpus's own probe).
		const std::string Skill = Input.Trees.empty() ? std::string() : Input.Trees.front().Skill;
		const std::string GotFunded =
			CanonicalIds(FInsimulSkillTreeModel::TreesFundedBy(Input.Trees, Skill));
		const std::string WantFunded = CanonicalOf(Expected->Find("funded"));
		Report("funded: " + Name, GotFunded == WantFunded,
			GotFunded == WantFunded ? "" : GotFunded + " != " + WantFunded);

		const std::string GotDepths =
			CanonicalDepths(FInsimulSkillTreeModel::Depths(Input.Trees));
		const std::string WantDepths = CanonicalOf(Expected->Find("depths"));
		Report("depths: " + Name, GotDepths == WantDepths,
			GotDepths == WantDepths ? "" : "depth map differs");

		for (const FSkillTreeView& Tree : View) {
			bSawCapped = bSawCapped || Tree.bCapped;
			for (const FSkillNodeView& Node : Tree.Nodes) {
				bSawOwned = bSawOwned || Node.Refusal == "owned";
				bSawPoints = bSawPoints || Node.Refusal == "points";
				bSawRequires = bSawRequires || Node.Refusal == "requires";
				bSawForbidden = bSawForbidden || Node.Refusal == "forbidden";
			}
		}
		bSawEmptyWorld = bSawEmptyWorld || View.empty();
		OutInputs.push_back(Input);
	}

	Report("trees: the corpus exercises the whole refusal ladder",
		bSawOwned && bSawPoints && bSawRequires && bSawForbidden);
	Report("trees: the corpus exercises a capped skill and a world with no trees",
		bSawCapped && bSawEmptyWorld);
}

// ── Negative controls ───────────────────────────────────────────────────────
//
// A check that cannot fail is a decoration. Each trial mutates one thing and
// asserts the view moves with it.

void RunNegativeControls(const std::vector<FSkillViewInput>& Inputs) {
	// The richest case: the one whose actor carries the KB's two answers.
	const FSkillViewInput* Rich = nullptr;
	for (const FSkillViewInput& Input : Inputs) {
		if (!Input.Actor.Unmet.empty() && !Input.Actor.Forbidden.empty()) {
			Rich = &Input;
		}
	}
	if (!Rich) {
		Report("control: the corpus carries a case with both KB answers", false);
		return;
	}
	Report("control: the corpus carries a case with both KB answers", true);

	const std::string ForbiddenNode = Rich->Actor.Forbidden.front();
	const std::string UnmetNode = Rich->Actor.Unmet.front().first;

	// The prohibition is what refuses the node: drop it and the same node is available.
	{
		FSkillViewInput Mutated = *Rich;
		const std::string TreeId = Mutated.Trees.front().Id;
		Report("control: a prohibited node reads `forbidden`",
			RefusalOf(FInsimulSkillTreeModel::BuildView(Mutated), TreeId, ForbiddenNode) ==
				"forbidden");
		Mutated.Actor.Forbidden.clear();
		Report("control: dropping the KB's prohibition makes the same node available",
			RefusalOf(FInsimulSkillTreeModel::BuildView(Mutated), TreeId, ForbiddenNode).empty());
	}

	// The unmet goal is what refuses the other one, and it outranks the prohibition.
	{
		FSkillViewInput Mutated = *Rich;
		std::string TreeOfUnmet;
		for (const FSkillTree& Tree : Mutated.Trees) {
			if (Tree.Find(UnmetNode)) {
				TreeOfUnmet = Tree.Id;
			}
		}
		Report("control: a node with an unmet goal reads `requires`",
			RefusalOf(FInsimulSkillTreeModel::BuildView(Mutated), TreeOfUnmet, UnmetNode) ==
				"requires");

		// Both rungs at once: `requires` wins, because a node whose prerequisites are
		// not met is refused whatever the norms say.
		Mutated.Actor.Forbidden.push_back(UnmetNode);
		Report("control: an unmet goal outranks a prohibition on the same node",
			RefusalOf(FInsimulSkillTreeModel::BuildView(Mutated), TreeOfUnmet, UnmetNode) ==
				"requires");

		Mutated.Actor.Forbidden.clear();
		Mutated.Actor.Unmet.clear();
		Report("control: dropping the KB's unmet goal makes the same node available",
			RefusalOf(FInsimulSkillTreeModel::BuildView(Mutated), TreeOfUnmet, UnmetNode).empty());
	}

	// The pool is a measurement, not a constant: empty it and the ladder moves to
	// `points`; a node already taken says `owned` whatever the pool holds.
	{
		FSkillViewInput Mutated = *Rich;
		for (auto& Pair : Mutated.Actor.Points) {
			Pair.second = 0;
		}
		Mutated.Actor.Forbidden.clear();
		Mutated.Actor.Unmet.clear();
		const std::vector<FSkillTreeView> View = FInsimulSkillTreeModel::BuildView(Mutated);
		bool bAllRefused = true;
		bool bTakenSaysOwned = true;
		for (const FSkillTreeView& Tree : View) {
			for (const FSkillNodeView& Node : Tree.Nodes) {
				if (Node.bTaken) {
					bTakenSaysOwned = bTakenSaysOwned && Node.Refusal == "owned";
					continue;
				}
				if (Node.Cost > 0 && Node.Refusal != "points") {
					bAllRefused = false;
				}
				// The node the world priced at nothing is available out of an empty
				// pool — the panel's hardest state.
				if (Node.Cost == 0 && !Node.bAvailable) {
					bAllRefused = false;
				}
			}
		}
		Report("control: an empty pool refuses every priced node and none of the free ones",
			bAllRefused);
		Report("control: a taken node says `owned` whatever the pool holds", bTakenSaysOwned);
	}

	// A row comes from the authored EDGES, never from an authored tier: add a parent
	// and the node moves down a row, and the tree's rows move with it.
	{
		FSkillViewInput Mutated = *Rich;
		FSkillTree& Tree = Mutated.Trees.front();
		if (Tree.Nodes.size() < 2) {
			Report("control: the corpus's first tree has two nodes to re-parent", false);
		} else {
			// The last node of the corpus's first tree is a root (`apprentice_mark`).
			FSkillNode& Leaf = Tree.Nodes.back();
			const std::string LeafId = Leaf.Id;
			const std::vector<FSkillTreeView> Before = FInsimulSkillTreeModel::BuildView(Mutated);
			const FSkillNodeView* BeforeNode = Before.front().Find(LeafId);
			const std::size_t BeforeRows = Before.front().Rows.size();
			const std::size_t BeforeEdges = Before.front().Edges.size();

			Leaf.Parents.push_back(Tree.Nodes.front().Id);
			const std::vector<FSkillTreeView> After = FInsimulSkillTreeModel::BuildView(Mutated);
			const FSkillNodeView* AfterNode = After.front().Find(LeafId);
			Report("control: an authored parent edge moves the node's row",
				BeforeNode && AfterNode && BeforeNode->Depth == 0 && AfterNode->Depth == 1 &&
					After.front().Edges.size() == BeforeEdges + 1 &&
					After.front().Rows.size() == BeforeRows);
			Report("control: the desugared parent arrives as one more requirement",
				AfterNode && BeforeNode &&
					AfterNode->Requires.size() == BeforeNode->Requires.size() + 1 &&
					AfterNode->bConditional);
		}
	}

	// An authored tree is content, and content can be wrong: a cycle must terminate.
	{
		FSkillViewInput Cyclic;
		FSkillTree Tree;
		Tree.Id = "loop";
		Tree.Skill = "nothing";
		FSkillNode A;
		A.Id = "a";
		A.Tree = "loop";
		A.Parents.push_back("b");
		FSkillNode B;
		B.Id = "b";
		B.Tree = "loop";
		B.Parents.push_back("a");
		Tree.Nodes.push_back(A);
		Tree.Nodes.push_back(B);
		Cyclic.Trees.push_back(Tree);
		const std::vector<FSkillTreeView> View = FInsimulSkillTreeModel::BuildView(Cyclic);
		Report("control: a cyclic authored tree terminates rather than hanging",
			View.size() == 1 && View.front().Nodes.size() == 2);
	}

	// A half-authored tree is still inspectable: the label falls back to the id.
	{
		FSkillViewInput Bare;
		FSkillTree Tree;
		Tree.Id = "unnamed_tree";
		Tree.Skill = "unnamed_skill";
		FSkillNode Node;
		Node.Id = "unnamed_node";
		Node.Tree = "unnamed_tree";
		Tree.Nodes.push_back(Node);
		Bare.Trees.push_back(Tree);
		const std::vector<FSkillTreeView> View = FInsimulSkillTreeModel::BuildView(Bare);
		Report("control: an unnamed tree and node render as their own ids",
			View.size() == 1 && View.front().Label == "unnamed_tree" &&
				View.front().Nodes.front().Label == "unnamed_node");
	}

	// A level past the end of the curve repeats the last entry rather than becoming
	// free — free is the failure that ships as an infinite levelling exploit.
	{
		FSkillTuning Tuning;
		Tuning.LevelXp = {0, 0, 40, 120, 300};
		Tuning.DefaultMaxLevel = 99;
		FSkillDefinition Skill;
		Skill.Id = "masonry";
		Report("control: a level past the end of the curve repeats its last price",
			FInsimulSkillTreeModel::XpForLevel(&Skill, 9, Tuning) == 300 &&
				FInsimulSkillTreeModel::XpForLevel(&Skill, 2, Tuning) == 40);
		Report("control: a skill with no curve of its own prices from the world's",
			FInsimulSkillTreeModel::XpForLevel(nullptr, 3, Tuning) == 120);
	}

	// A document that is not a skill view is refused, never quietly drawn empty.
	{
		FSkillViewInput Ignored;
		std::string Error;
		const FJsonParseResult NoTrees = ParseJson("{\"skills\":[]}");
		Report("control: a document with no trees array is refused",
			NoTrees.bOk && !FSkillViewInput::FromJson(*NoTrees.Root, Ignored, Error) &&
				!Error.empty());
		const FJsonParseResult NoId = ParseJson("{\"trees\":[{\"skill\":\"masonry\"}]}");
		Report("control: a tree with no id is refused",
			NoId.bOk && !FSkillViewInput::FromJson(*NoId.Root, Ignored, Error));
		const FJsonParseResult NodeNoId =
			ParseJson("{\"trees\":[{\"id\":\"t\",\"nodes\":[{\"cost\":1}]}]}");
		Report("control: a node with no id is refused",
			NodeNoId.bOk && !FSkillViewInput::FromJson(*NodeNoId.Root, Ignored, Error));
		const FJsonParseResult Empty = ParseJson("{\"trees\":[]}");
		Report("control: a world with no trees at all parses to an empty panel",
			Empty.bOk && FSkillViewInput::FromJson(*Empty.Root, Ignored, Error) &&
				FInsimulSkillTreeModel::BuildView(Ignored).empty());
	}
}

} // namespace

int main(int argc, char** argv) {
#ifdef INSIMUL_SKILLS_DIR
	const char* DefaultCorpus = INSIMUL_SKILLS_DIR;
#else
	const char* DefaultCorpus = "../../../conformance/skills";
#endif
	const std::string Dir = argc > 1 ? argv[1] : DefaultCorpus;

	std::printf("== Insimul skill-tree panel host tests (tasklist 190 US-2) ==\n");
	std::printf("   corpus dir: %s\n", Dir.c_str());

	std::vector<FSkillViewInput> Inputs;
	RunCorpus(Dir, Inputs);
	RunNegativeControls(Inputs);

	std::printf("\n  %d passed, %d failed\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
