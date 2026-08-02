// test_quest_parity.cpp — the two-implementation diff (tasklist 99, US-3).
//
// WHY THIS EXISTS. US-3's third criterion asks that, where this engine's existing
// implementation and core's disagree, each difference be classified as a fix, a
// tolerable shape change, or a regression. For the ADOPTED slice (radiant quest
// GENERATION) that diff is nearly vacuous: this engine generated nothing before
// US-2, so `test_radiant.cpp --source none` can only ever report AGREE or GAIN
// (RUNTIME_CORE_ADOPTION.md §6.2). It is run and asserted, but it is the weak
// half of the answer.
//
// This is the useful half. `Portable/InsimulQuestSystem.cpp` is a hand-port of
// core's quest hydration and radiant TICK — a real second implementation of two
// things core also implements, shipping in this plugin today — and
// `conformance/quests/{hydration,radiant}-cases.json` pins both. So the same
// vectors can drive both implementations and the difference, if any, is real.
//
// NOT AN ADOPTION. `quest.hydrate` and `quest.radiantTick` are comparison
// surfaces, not adopted ones — core's own bundle entry point says so in as many
// words (native/corebridge/js/entry.js). Nothing in Source/ calls them and
// nothing should: FInsimulQuestSystem is what ships. That is why this gate builds
// its own request documents instead of routing through an adapter — there is no
// adapter to route through, and adding one would adopt surface US-2 did not.
//
// ── THREE LEGS, ONE SERIALIZER ──────────────────────────────────────────────
//
//   CORPUS  the committed `expected` block — the referee.
//   PORT    insimul::FInsimulQuestSystem, the hand-port that ships.
//   CORE    @insimul/core through libinsimulcore (QuickJS over the natively
//           linked libinsimul). Present only in the `quest_parity_core` target;
//           `quest_parity` runs the CORPUS/PORT legs and the classifier
//           self-test with no native library, so a standalone clone still gates
//           something.
//
// All three are reduced to a canonical string by the SAME C++ code
// (CanonicalJsonStringify for hydration, CanonicalFactList for the tick), so a
// difference that survives is semantic and never a formatting artifact. The one
// dimension the corpus declares insignificant — radiant fact EMIT ORDER, which
// conformance/quests/radiant-cases.json compares as a multiset — is captured
// separately so it can be classified SHAPE rather than counted as a difference.
//
// ── THE VERDICTS ────────────────────────────────────────────────────────────
//
//   AGREE       PORT and CORE produce the same thing, and it is what the corpus
//               says. Nothing to do.
//   SHAPE       They differ only in a dimension the corpus declares
//               insignificant (fact emit order). Tolerable; both still match.
//   FIX         They differ semantically and CORE matches the corpus — adopting
//               core would correct a bug in the hand-port. Reported, not fatal.
//   REGRESSION  They differ semantically and the PORT matches the corpus —
//               adopting core would break a committed golden. FATAL.
//   UNGOLDENED  They differ semantically and NEITHER matches the corpus. The
//               corpus is not describing either implementation, so no verdict
//               about adoption can be drawn from it. FATAL.
//
// THE CLASSIFIER IS SELF-TESTED. Before touching the corpus the gate runs its
// classifier over five synthetic triples and asserts all five verdicts are
// reachable. Otherwise "N AGREE, 0 everything else" would be indistinguishable
// from a classifier that can only say AGREE.
//
// THE COUNT IS ASSERTED, NOT REPORTED. Same discipline as test_radiant.cpp: the
// gate fails if either corpus file is missing or empty, if fewer than MIN_CASES
// run, if either area contributes nothing, or if a case name repeats.

#include "InsimulCanonicalJson.h"
#include "InsimulJson.h"
#include "InsimulQuestSystem.h"

#ifdef INSIMUL_HAVE_CORE_BRIDGE
#include "InsimulCoreBridge.h"
#endif
#include "InsimulCoreCaller.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace insimul;

// The corpus as vendored at adoption time: 4 hydration cases + 3 tick cases.
// Growing the corpus must not break the gate; shrinking it must.
static const int MIN_HYDRATION_CASES = 4;
static const int MIN_RADIANT_CASES = 3;

namespace {

int Failures = 0;
int CasesRun = 0;
int HydrationCases = 0;
int RadiantCases = 0;

// ── verdicts ────────────────────────────────────────────────────────────────

enum class EVerdict { Agree, Shape, Fix, Regression, Ungoldened };

const char* VerdictName(EVerdict V) {
	switch (V) {
	case EVerdict::Agree: return "AGREE";
	case EVerdict::Shape: return "SHAPE";
	case EVerdict::Fix: return "FIX";
	case EVerdict::Regression: return "REGRESSION";
	case EVerdict::Ungoldened: return "UNGOLDENED";
	}
	return "?";
}

bool IsFatal(EVerdict V) {
	return V == EVerdict::Regression || V == EVerdict::Ungoldened;
}

int Counts[5] = {0, 0, 0, 0, 0};

void Tally(EVerdict V) { Counts[static_cast<int>(V)]++; }

/**
 * One leg's output in two forms: `Canonical` is the comparison the corpus
 * declares significant (order-normalised for facts), `Raw` additionally carries
 * the dimension it declares insignificant (emit order). Splitting them is the
 * whole reason SHAPE can be told apart from AGREE.
 */
struct FLegOutput {
	std::string Canonical;
	std::string Raw;
};

/**
 * The classifier. Pure — takes three reduced outputs and returns a verdict — so
 * the self-test below can exercise every branch without a corpus or a bridge.
 */
EVerdict Classify(const FLegOutput& Port, const FLegOutput& Core, const std::string& Corpus) {
	if (Port.Canonical == Core.Canonical) {
		if (Port.Canonical != Corpus) {
			// They agree with each other and disagree with the golden. Adoption
			// is not the question here — the corpus is describing neither.
			return EVerdict::Ungoldened;
		}
		return Port.Raw == Core.Raw ? EVerdict::Agree : EVerdict::Shape;
	}
	const bool bCoreMatches = Core.Canonical == Corpus;
	const bool bPortMatches = Port.Canonical == Corpus;
	if (bCoreMatches && !bPortMatches) return EVerdict::Fix;
	if (bPortMatches && !bCoreMatches) return EVerdict::Regression;
	return EVerdict::Ungoldened;
}

void Report(const std::string& Area, const std::string& Name, EVerdict V, const FLegOutput& Port,
	const FLegOutput& Core, const std::string& Corpus) {
	const char* Mark = V == EVerdict::Agree ? "=" : (IsFatal(V) ? "x" : "+");
	std::printf("  %s %s/%s  %s\n", Mark, Area.c_str(), Name.c_str(), VerdictName(V));
	if (V != EVerdict::Agree) {
		std::printf("      port:   %s\n", Port.Canonical.c_str());
		std::printf("      core:   %s\n", Core.Canonical.c_str());
		std::printf("      corpus: %s\n", Corpus.c_str());
		if (V == EVerdict::Shape) {
			std::printf("      (emit order only: port %s | core %s)\n", Port.Raw.c_str(), Core.Raw.c_str());
		}
	}
	Tally(V);
	if (IsFatal(V)) {
		Failures++;
	}
}

// ── the classifier self-test ────────────────────────────────────────────────

/**
 * Five synthetic triples, one per verdict. If a refactor collapses a branch,
 * this fails before the corpus is ever read — so the corpus result below is a
 * finding rather than the classifier's only possible output.
 */
void SelfTestClassifier() {
	std::printf("== classifier self-test ==\n");
	struct FCase {
		const char* Name;
		FLegOutput Port;
		FLegOutput Core;
		const char* Corpus;
		EVerdict Want;
	};
	const FCase Cases[] = {
		{"agree", {"A", "A"}, {"A", "A"}, "A", EVerdict::Agree},
		{"shape-emit-order", {"A", "x,y"}, {"A", "y,x"}, "A", EVerdict::Shape},
		{"fix", {"B", "B"}, {"A", "A"}, "A", EVerdict::Fix},
		{"regression", {"A", "A"}, {"B", "B"}, "A", EVerdict::Regression},
		{"ungoldened-both-off", {"B", "B"}, {"C", "C"}, "A", EVerdict::Ungoldened},
	};
	std::set<EVerdict> Seen;
	for (const FCase& C : Cases) {
		const EVerdict Got = Classify(C.Port, C.Core, C.Corpus);
		Seen.insert(Got);
		if (Got != C.Want) {
			std::printf("  x %s: classifier said %s, expected %s\n", C.Name, VerdictName(Got), VerdictName(C.Want));
			Failures++;
		}
	}
	// A sixth shape: agreeing with each other but not the golden must NOT read
	// as AGREE just because the two legs match.
	if (Classify({"Z", "Z"}, {"Z", "Z"}, "A") != EVerdict::Ungoldened) {
		std::printf("  x both-legs-agree-but-golden-differs must not classify AGREE\n");
		Failures++;
	}
	if (Seen.size() != 5) {
		std::printf("  x only %zu of 5 verdicts reachable\n", Seen.size());
		Failures++;
	} else {
		std::printf("  . 5/5 verdicts reachable\n");
	}
}

// ── corpus plumbing ─────────────────────────────────────────────────────────

std::string ReadFile(const std::string& Dir, const std::string& Name, bool& bOk) {
	const std::string Path = Dir + "/" + Name;
	std::ifstream In(Path, std::ios::binary);
	if (!In) {
		std::fprintf(stderr, "error: could not open %s\n", Path.c_str());
		bOk = false;
		return std::string();
	}
	std::ostringstream Buffer;
	Buffer << In.rdbuf();
	bOk = true;
	return Buffer.str();
}

/** Emit-order-preserving rendering of a fact list (the insignificant dimension). */
std::string RawFactList(const std::vector<FPrologFact>& Facts) {
	std::string Out;
	for (std::size_t I = 0; I < Facts.size(); I++) {
		if (I) {
			Out += ',';
		}
		Out += FInsimulQuestSystem::CanonicalFactString(Facts[I]);
	}
	return Out;
}

/** `{predicate, args:[...]}` -> FPrologFact. Shared by the corpus and core legs. */
FPrologFact FactFromJson(const FJsonValue& Node) {
	FPrologFact F;
	F.Predicate = Node.GetString("predicate");
	const FJsonValue* Args = Node.Find("args");
	if (Args && Args->IsArray()) {
		for (const FJsonValuePtr& A : Args->ArrayItems) {
			if (A->IsNumber()) {
				F.Args.push_back(FPrologArg::MakeNumber(A->AsNumber()));
			} else {
				F.Args.push_back(FPrologArg::MakeAtom(A->AsString()));
			}
		}
	}
	return F;
}

std::vector<FPrologFact> FactsFromArray(const FJsonValue* Array) {
	std::vector<FPrologFact> Out;
	if (Array && Array->IsArray()) {
		for (const FJsonValuePtr& F : Array->ArrayItems) {
			Out.push_back(FactFromJson(*F));
		}
	}
	return Out;
}

// ── the CORE leg ────────────────────────────────────────────────────────────

/**
 * Call a comparison method on core and return the parsed result, or nullptr with
 * `OutError` set. A failure here is a gate failure, never a silent skip: a leg
 * that quietly stops answering would turn every case into a false AGREE.
 */
FJsonValuePtr CallCore(ICoreCaller& Caller, const std::string& Method, const std::string& ArgsJson,
	std::string& OutError) {
	std::string Response;
	if (!Caller.Call(Method, ArgsJson, Response)) {
		OutError = Caller.LastError();
		return nullptr;
	}
	FJsonParseResult Parsed = ParseJson(Response);
	if (!Parsed.bOk || !Parsed.Root) {
		OutError = Method + " returned unparseable JSON: " + Parsed.Error;
		return nullptr;
	}
	return Parsed.Root;
}

// ── hydration ───────────────────────────────────────────────────────────────

void RunHydration(const std::string& QuestsDir, ICoreCaller* Core) {
	std::printf("== hydration parity (conformance/quests/hydration-cases.json) ==\n");
	bool bOk = false;
	const std::string Text = ReadFile(QuestsDir, "hydration-cases.json", bOk);
	if (!bOk) {
		Failures++;
		return;
	}
	FJsonParseResult Parsed = ParseJson(Text);
	if (!Parsed.bOk || !Parsed.Root) {
		std::fprintf(stderr, "error: hydration-cases.json is not valid JSON: %s\n", Parsed.Error.c_str());
		Failures++;
		return;
	}
	const FJsonValue* Cases = Parsed.Root->Find("cases");
	if (!Cases || !Cases->IsArray() || Cases->ArrayItems.empty()) {
		std::fprintf(stderr, "error: hydration-cases.json has no cases - the gate would execute NOTHING\n");
		Failures++;
		return;
	}

	std::set<std::string> Names;
	for (const FJsonValuePtr& C : Cases->ArrayItems) {
		const std::string Name = C->GetString("name");
		if (!Names.insert(Name).second) {
			std::fprintf(stderr, "error: duplicate hydration case name '%s'\n", Name.c_str());
			Failures++;
		}
		const FJsonValue* Input = C->Find("input");
		const FJsonValue* Expected = C->Find("expected");
		if (!Input || !Expected) {
			std::fprintf(stderr, "error: hydration case '%s' is malformed\n", Name.c_str());
			Failures++;
			continue;
		}
		HydrationCases++;
		CasesRun++;

		const std::string Content = Input->GetString("content");
		const std::string Status = Input->GetString("status");
		const std::string Corpus = CanonicalJsonStringify(*Expected);

		FLegOutput Port;
		Port.Canonical = FInsimulQuestSystem::HydrateCanonical(Content, Status);
		Port.Raw = Port.Canonical; // hydration has no insignificant dimension

		if (!Core) {
			// Two-leg mode: no core to diff against, so this degenerates to the
			// pre-existing golden check. Say so rather than reporting a verdict.
			if (Port.Canonical != Corpus) {
				std::printf("  x hydration/%s  PORT does not match the corpus\n", Name.c_str());
				std::printf("      port:   %s\n      corpus: %s\n", Port.Canonical.c_str(), Corpus.c_str());
				Failures++;
			} else {
				std::printf("  . hydration/%s  port matches corpus (no core leg)\n", Name.c_str());
			}
			continue;
		}

		std::string Args = "{\"content\":" + CanonicalJsonString(Content);
		if (!Status.empty()) {
			Args += ",\"status\":" + CanonicalJsonString(Status);
		}
		Args += "}";

		std::string Error;
		const FJsonValuePtr Result = CallCore(*Core, "quest.hydrate", Args, Error);
		if (!Result) {
			std::printf("  x hydration/%s  core leg failed: %s\n", Name.c_str(), Error.c_str());
			Failures++;
			continue;
		}
		const FJsonValue* Quest = Result->Find("quest");
		if (!Quest) {
			std::printf("  x hydration/%s  quest.hydrate returned no `quest`\n", Name.c_str());
			Failures++;
			continue;
		}
		FLegOutput CoreOut;
		CoreOut.Canonical = CanonicalJsonStringify(*Quest);
		CoreOut.Raw = CoreOut.Canonical;

		Report("hydration", Name, Classify(Port, CoreOut, Corpus), Port, CoreOut, Corpus);
	}
}

// ── radiant tick ────────────────────────────────────────────────────────────

void RunRadiantTick(const std::string& QuestsDir, ICoreCaller* Core) {
	std::printf("== radiant-tick parity (conformance/quests/radiant-cases.json) ==\n");
	bool bOk = false;
	const std::string Text = ReadFile(QuestsDir, "radiant-cases.json", bOk);
	if (!bOk) {
		Failures++;
		return;
	}
	FJsonParseResult Parsed = ParseJson(Text);
	if (!Parsed.bOk || !Parsed.Root) {
		std::fprintf(stderr, "error: radiant-cases.json is not valid JSON: %s\n", Parsed.Error.c_str());
		Failures++;
		return;
	}
	const FJsonValue* Cases = Parsed.Root->Find("cases");
	if (!Cases || !Cases->IsArray() || Cases->ArrayItems.empty()) {
		std::fprintf(stderr, "error: radiant-cases.json has no cases - the gate would execute NOTHING\n");
		Failures++;
		return;
	}

	std::set<std::string> Names;
	for (const FJsonValuePtr& C : Cases->ArrayItems) {
		const std::string Name = C->GetString("name");
		if (!Names.insert(Name).second) {
			std::fprintf(stderr, "error: duplicate radiant case name '%s'\n", Name.c_str());
			Failures++;
		}
		RadiantCases++;
		CasesRun++;

		const int MaxOffering = static_cast<int>(C->GetInt("maxOffering"));
		const int Ticks = static_cast<int>(C->GetInt("ticks"));

		std::vector<FRadiantQuest> Quests;
		const FJsonValue* QuestsNode = C->Find("quests");
		if (QuestsNode && QuestsNode->IsArray()) {
			for (const FJsonValuePtr& Q : QuestsNode->ArrayItems) {
				FRadiantQuest RQ;
				RQ.Id = Q->GetString("id");
				RQ.Status = Q->GetString("status");
				const FJsonValue* Tags = Q->Find("tags");
				if (Tags && Tags->IsArray()) {
					for (const FJsonValuePtr& T : Tags->ArrayItems) {
						RQ.Tags.push_back(T->AsString());
					}
				}
				Quests.push_back(RQ);
			}
		}

		const std::vector<FPrologFact> CorpusFacts = FactsFromArray(C->Find("expected"));
		const std::string Corpus = FInsimulQuestSystem::CanonicalFactList(CorpusFacts);

		const std::vector<FPrologFact> PortFacts = FInsimulQuestSystem::RadiantTick(Quests, MaxOffering, Ticks);
		FLegOutput Port;
		Port.Canonical = FInsimulQuestSystem::CanonicalFactList(PortFacts);
		Port.Raw = RawFactList(PortFacts);

		if (!Core) {
			if (Port.Canonical != Corpus) {
				std::printf("  x radiant/%s  PORT does not match the corpus\n", Name.c_str());
				std::printf("      port:   %s\n      corpus: %s\n", Port.Canonical.c_str(), Corpus.c_str());
				Failures++;
			} else {
				std::printf("  . radiant/%s  port matches corpus (no core leg)\n", Name.c_str());
			}
			continue;
		}

		// The request document mirrors the corpus case one-for-one: core's
		// radiantTick takes the same {quests, maxOffering, ticks} the port does.
		std::string Args = "{\"quests\":[";
		for (std::size_t I = 0; I < Quests.size(); I++) {
			if (I) {
				Args += ',';
			}
			Args += "{\"id\":" + CanonicalJsonString(Quests[I].Id) + ",\"tags\":[";
			for (std::size_t J = 0; J < Quests[I].Tags.size(); J++) {
				if (J) {
					Args += ',';
				}
				Args += CanonicalJsonString(Quests[I].Tags[J]);
			}
			Args += "],\"status\":" + CanonicalJsonString(Quests[I].Status) + "}";
		}
		Args += "],\"maxOffering\":" + std::to_string(MaxOffering) + ",\"ticks\":" + std::to_string(Ticks) + "}";

		std::string Error;
		const FJsonValuePtr Result = CallCore(*Core, "quest.radiantTick", Args, Error);
		if (!Result) {
			std::printf("  x radiant/%s  core leg failed: %s\n", Name.c_str(), Error.c_str());
			Failures++;
			continue;
		}
		const FJsonValue* FactsNode = Result->Find("facts");
		if (!FactsNode || !FactsNode->IsArray()) {
			std::printf("  x radiant/%s  quest.radiantTick returned no `facts` array\n", Name.c_str());
			Failures++;
			continue;
		}
		const std::vector<FPrologFact> CoreFacts = FactsFromArray(FactsNode);
		FLegOutput CoreOut;
		CoreOut.Canonical = FInsimulQuestSystem::CanonicalFactList(CoreFacts);
		CoreOut.Raw = RawFactList(CoreFacts);

		Report("radiant", Name, Classify(Port, CoreOut, Corpus), Port, CoreOut, Corpus);
	}
}

} // namespace

int main(int argc, char** argv) {
	std::string QuestsDir;
	bool bWantCore = false;
	for (int I = 1; I < argc; I++) {
		if (std::strcmp(argv[I], "--core") == 0) {
			bWantCore = true;
		} else if (QuestsDir.empty()) {
			QuestsDir = argv[I];
		}
	}
#ifdef INSIMUL_QUESTS_DIR
	if (QuestsDir.empty()) {
		QuestsDir = INSIMUL_QUESTS_DIR;
	}
#endif
	if (QuestsDir.empty()) {
		std::fprintf(stderr, "usage: test_quest_parity [--core] <conformance/quests dir>\n");
		return 2;
	}
#ifndef INSIMUL_HAVE_CORE_BRIDGE
	if (bWantCore) {
		std::fprintf(stderr,
			"error: this binary was built WITHOUT libinsimulcore, so --core cannot run.\n"
			"       Configure with an insimul-native checkout visible (see\n"
			"       tools/verify-unreal/CMakeLists.txt) and run the `quest_parity_core` test.\n");
		return 1;
	}
#endif

	SelfTestClassifier();

	ICoreCaller* Core = nullptr;
#ifdef INSIMUL_HAVE_CORE_BRIDGE
	FInsimulCoreBridge Bridge;
	if (bWantCore) {
		if (!Bridge.IsAvailable()) {
			std::fprintf(stderr, "error: the core bridge could not start: %s\n", Bridge.LastError().c_str());
			return 1;
		}
		Core = &Bridge;
		std::printf("libinsimulcore %s\n", Bridge.Version().c_str());

		// The comparison surface, asserted rather than assumed: a bundle that
		// dropped either method must fail here, not degrade to a two-leg run.
		std::string Error;
		const FJsonValuePtr Methods = CallCore(Bridge, "core.methods", "{}", Error);
		if (!Methods) {
			std::fprintf(stderr, "error: core.methods failed: %s\n", Error.c_str());
			return 1;
		}
		const FJsonValue* List = Methods->Find("methods");
		for (const char* Required : {"quest.hydrate", "quest.radiantTick"}) {
			bool bFound = false;
			if (List && List->IsArray()) {
				for (const FJsonValuePtr& M : List->ArrayItems) {
					bFound = bFound || M->AsString() == Required;
				}
			}
			if (!bFound) {
				std::fprintf(stderr, "error: the bundle does not expose %s\n", Required);
				return 1;
			}
		}
	}
#endif

	std::printf("Quest parity (%s) - legs: corpus, port%s\n", QuestsDir.c_str(), Core ? ", core" : " (NO core leg)");

	RunHydration(QuestsDir, Core);
	RunRadiantTick(QuestsDir, Core);

	// ── the gate cannot pass without having executed something ──────────────
	std::printf("\n%d case(s) executed: %d hydration + %d radiant\n", CasesRun, HydrationCases, RadiantCases);
	if (CasesRun == 0) {
		std::fprintf(stderr, "error: the gate executed ZERO cases\n");
		return 1;
	}
	if (HydrationCases < MIN_HYDRATION_CASES) {
		std::fprintf(stderr, "error: only %d hydration case(s), expected at least %d - the corpus shrank\n",
			HydrationCases, MIN_HYDRATION_CASES);
		Failures++;
	}
	if (RadiantCases < MIN_RADIANT_CASES) {
		std::fprintf(stderr, "error: only %d radiant case(s), expected at least %d - the corpus shrank\n",
			RadiantCases, MIN_RADIANT_CASES);
		Failures++;
	}

	if (Core) {
		const int Classified = Counts[0] + Counts[1] + Counts[2] + Counts[3] + Counts[4];
		std::printf("classification: %d AGREE, %d SHAPE, %d FIX, %d REGRESSION, %d UNGOLDENED\n",
			Counts[0], Counts[1], Counts[2], Counts[3], Counts[4]);
		if (Classified != CasesRun) {
			std::fprintf(stderr, "error: %d case(s) executed but only %d classified\n", CasesRun, Classified);
			Failures++;
		}
	}

	if (Failures > 0) {
		std::printf("FAILED: %d\n", Failures);
		return 1;
	}
	std::printf("PASSED: %d case(s)\n", CasesRun);
	return 0;
}
