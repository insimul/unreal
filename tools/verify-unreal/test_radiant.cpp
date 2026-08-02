// test_radiant.cpp — the radiant-adoption gate (tasklist 99, US-2/US-3).
//
// Drives every case in conformance/radiant/*.json through the ADAPTER —
// insimul::FRadiantSource (Source/InsimulRuntime/Portable/InsimulRadiantSource.h)
// — and compares against the corpus's `expected`. These are the same 11 vectors
// packages/core's own runner (src/conformance/__tests__/radiant-corpus.test.ts)
// executes, unreduced, and the same ones the Godot adapter runs.
//
// IT DRIVES THE ADAPTER, NOT THE ABI. The gate deliberately does NOT hand-build
// `radiant.generate` arguments: doing so would create a second translation site
// that could drift from the shipping one, which is exactly what US-2's fourth
// criterion forbids. Everything below FRadiantSource is swapped instead, through
// insimul::ICoreCaller.
//
// This file mirrors core's reference runner's comparison rules exactly:
//
//   program        = kb ++ templates, joined with '\n'
//   quest order    = as emitted (deterministic sorted-TplId order)
//   content        = split on '\n', trimmed, blanks dropped, SORTED
//   factsToAssert  = SORTED
//   factsToRetract = SORTED
//
// A conforming engine may emit clauses within a quest in any order; it may NOT
// reorder the quests themselves.
//
// ── THE THREE LEGS ──────────────────────────────────────────────────────────
//
//   --source core   the adopted implementation, through the REAL libinsimulcore
//                   (QuickJS + the vendored @insimul/core bundle) over the
//                   natively linked libinsimul. Must match the corpus exactly;
//                   any difference is a failure. Built only when an
//                   insimul-native checkout was found at configure time —
//                   ctest target `radiant_bridge`.
//
//   --source stub   the adapter over a RECORDING stub caller, so the
//                   translation site is gated with no native library present
//                   (ctest target `radiant_source`, always built). It is not a
//                   tautology: the stub asserts the request document
//                   byte-for-byte against an INDEPENDENTLY built expectation —
//                   its own JSON escaper, its own key order — and answers with a
//                   deliberately PERTURBED wire shape (clauses reversed, padded
//                   and blank-line-separated; fact arrays reversed) so a decoder
//                   that forgets to trim, drop blanks or normalise fails here.
//                   What it cannot check is core's semantics; that is `core`.
//
//   --source none   this plugin's PRE-ADOPTION behaviour — it generated no
//                   radiant quests at all (RUNTIME_CORE_ADOPTION.md §6.2), so
//                   every case yields zero quests. US-2 requires the old
//                   implementation to stay reachable; here there is no old
//                   implementation, and `none` is the honest statement of that.
//                   Running it over the same vectors is how US-3 shows the
//                   adoption is a capability GAIN with no reachable regression.
//                   It mirrors insimul::ERadiantSource::None, which is also the
//                   runtime fallback on a platform with no libinsimulcore.
//
// US-3's CLASSIFICATION (`--source none`). The `none` leg is expected NOT to
// match the corpus — that is the point — so instead of counting failures it
// classifies every case and asserts the classification:
//   AGREE       the corpus expects zero quests, and `none` produces zero
//   GAIN        the corpus expects quests, `none` produces none — new capability
//   REGRESSION  `none` produced a quest core does not, or a different one
// A regression is not constructible from an implementation that emits nothing,
// which is exactly why it is worth asserting rather than asserting by argument:
// the assertion is what will still be true after somebody edits the adapter.
//
// THE COUNT IS ASSERTED, NOT REPORTED. A corpus runner that silently executes
// nothing must fail — this codebase has shipped gates that could not fail. So
// the gate requires the corpus files to be found, the case count to be non-zero
// and at least MIN_CASES, all five area files to be present, and case names to
// be unique. Then it compares.

#include "InsimulJson.h"
#include "InsimulRadiantSource.h"

#ifdef INSIMUL_HAVE_CORE_BRIDGE
#include "InsimulCoreBridge.h"
#endif

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using insimul::FJsonValue;
using insimul::FJsonValuePtr;
using insimul::FRadiantSource;

// The corpus as vendored at adoption time: 5 files / 11 cases. Growing the
// corpus should not break the gate; shrinking it must.
static const int MIN_CASES = 11;
static const char* REQUIRED_AREAS[] = {
	"radiant-single-slot",
	"radiant-multi-slot",
	"radiant-exclusion-cooldown",
	"radiant-empty",
	"radiant-maxquests",
};

namespace {

int Failures = 0;
int CasesRun = 0;
int Agree = 0, Gain = 0, Regression = 0;

void Fail(const std::string& CaseName, const std::string& What) {
	Failures++;
	std::printf("  x %s\n      %s\n", CaseName.c_str(), What.c_str());
}

std::string ReadFile(const std::string& Path, bool& bOk) {
	std::ifstream In(Path, std::ios::binary);
	if (!In) {
		bOk = false;
		return std::string();
	}
	std::ostringstream SS;
	SS << In.rdbuf();
	bOk = true;
	return SS.str();
}

std::vector<std::string> ListJsonFiles(const std::string& Dir) {
	std::vector<std::string> Files;
	DIR* D = opendir(Dir.c_str());
	if (!D) {
		return Files;
	}
	while (struct dirent* E = readdir(D)) {
		const std::string Name(E->d_name);
		if (Name.size() > 5 && Name.compare(Name.size() - 5, 5, ".json") == 0) {
			Files.push_back(Name);
		}
	}
	closedir(D);
	std::sort(Files.begin(), Files.end());
	return Files;
}

std::vector<std::string> ReadStringArray(const FJsonValue* Array) {
	std::vector<std::string> Out;
	if (Array && Array->IsArray()) {
		for (const FJsonValuePtr& Item : Array->ArrayItems) {
			Out.push_back(Item->AsString());
		}
	}
	return Out;
}

std::string Join(const std::vector<std::string>& Xs, const char* Sep) {
	std::string Out;
	for (std::size_t I = 0; I < Xs.size(); I++) {
		if (I) {
			Out += Sep;
		}
		Out += Xs[I];
	}
	return Out;
}

// ── the corpus case, and the comparable shape of a quest ────────────────────

/** One quest reduced to the corpus's comparable shape (everything sorted). */
struct FQuest {
	std::string QuestId;
	std::string TemplateId;
	std::vector<std::string> Content;
	std::vector<std::string> AssertFacts;
	std::vector<std::string> RetractFacts;

	bool operator==(const FQuest& O) const {
		return QuestId == O.QuestId && TemplateId == O.TemplateId && Content == O.Content &&
			AssertFacts == O.AssertFacts && RetractFacts == O.RetractFacts;
	}

	std::string Describe() const {
		std::string S = QuestId + " [" + TemplateId + "]";
		S += "\n        content:  " + Join(Content, " | ");
		S += "\n        assert:   " + Join(AssertFacts, " | ");
		S += "\n        retract:  " + Join(RetractFacts, " | ");
		return S;
	}
};

std::vector<std::string> Sorted(std::vector<std::string> Xs) {
	std::sort(Xs.begin(), Xs.end());
	return Xs;
}

std::vector<FQuest> ExpectedQuests(const FJsonValue& Case) {
	std::vector<FQuest> Out;
	const FJsonValue* Expected = Case.Find("expected");
	const FJsonValue* Quests = Expected ? Expected->Find("quests") : nullptr;
	if (!Quests || !Quests->IsArray()) {
		return Out;
	}
	for (const FJsonValuePtr& Q : Quests->ArrayItems) {
		FQuest Quest;
		Quest.QuestId = Q->GetString("questId");
		Quest.TemplateId = Q->GetString("templateId");
		Quest.Content = Sorted(ReadStringArray(Q->Find("content")));
		Quest.AssertFacts = Sorted(ReadStringArray(Q->Find("factsToAssert")));
		Quest.RetractFacts = Sorted(ReadStringArray(Q->Find("factsToRetract")));
		Out.push_back(Quest);
	}
	return Out;
}

/** What the adapter produced, reduced to the same comparable shape. */
std::vector<FQuest> ActualQuests(const std::vector<insimul::FGeneratedRadiantQuest>& Generated) {
	std::vector<FQuest> Out;
	for (const insimul::FGeneratedRadiantQuest& G : Generated) {
		FQuest Quest;
		Quest.QuestId = G.QuestId;
		Quest.TemplateId = G.TemplateId;
		Quest.Content = Sorted(G.ContentClauses);
		Quest.AssertFacts = Sorted(G.FactsToAssert);
		Quest.RetractFacts = Sorted(G.FactsToRetract);
		Out.push_back(Quest);
	}
	return Out;
}

insimul::FRadiantOptions CaseOptions(const FJsonValue& Case) {
	insimul::FRadiantOptions Options;
	const FJsonValue* Seed = Case.Find("seed");
	if (Seed && Seed->IsNumber()) {
		Options.Seed = insimul::FRadiantSeed::FromNumber(Seed->NumberValue, Seed->RawNumber);
	} else {
		Options.Seed = insimul::FRadiantSeed::FromText(Seed ? Seed->AsString() : std::string());
	}
	Options.Now = Case.GetInt("now", 0);
	Options.MaxQuests = static_cast<int>(Case.GetInt("maxQuests", 0));
	return Options;
}

// ── the recording stub caller (`--source stub`) ─────────────────────────────

/**
 * An INDEPENDENT JSON string escaper, written here rather than reused from
 * Portable/InsimulCanonicalJson.h on purpose: if the gate and the adapter shared
 * an escaper, a bug in it would be invisible to both.
 */
std::string EscapeJsonString(const std::string& S) {
	std::string Out = "\"";
	for (unsigned char C : S) {
		switch (C) {
		case '"': Out += "\\\""; break;
		case '\\': Out += "\\\\"; break;
		case '\b': Out += "\\b"; break;
		case '\f': Out += "\\f"; break;
		case '\n': Out += "\\n"; break;
		case '\r': Out += "\\r"; break;
		case '\t': Out += "\\t"; break;
		default:
			if (C < 0x20) {
				char Buf[7];
				std::snprintf(Buf, sizeof(Buf), "\\u%04x", C);
				Out += Buf;
			} else {
				Out += static_cast<char>(C);
			}
		}
	}
	Out += '"';
	return Out;
}

/**
 * The `radiant.generate` request document this case MUST produce, built from the
 * corpus independently of FRadiantSource::BuildGenerateArgs. Core takes one
 * Prolog program: world facts first, then templates, joined with '\n'.
 */
std::string ExpectedArgs(const FJsonValue& Case) {
	std::vector<std::string> Program = ReadStringArray(Case.Find("kb"));
	const std::vector<std::string> Templates = ReadStringArray(Case.Find("templates"));
	Program.insert(Program.end(), Templates.begin(), Templates.end());

	std::string Args = "{\"kb\":" + EscapeJsonString(Join(Program, "\n"));
	Args += ",\"options\":{\"seed\":";
	const FJsonValue* Seed = Case.Find("seed");
	if (Seed && Seed->IsNumber()) {
		Args += Seed->RawNumber;
	} else {
		Args += EscapeJsonString(Seed ? Seed->AsString() : std::string());
	}
	Args += ",\"now\":" + std::to_string(Case.GetInt("now", 0));
	const long long MaxQuests = Case.GetInt("maxQuests", 0);
	if (MaxQuests > 0) {
		Args += ",\"maxQuests\":" + std::to_string(MaxQuests);
	}
	Args += "}}";
	return Args;
}

/**
 * Answers `radiant.generate` from the case's `expected` block, in a deliberately
 * PERTURBED wire shape — clause order reversed, each clause indented, a blank
 * line between clauses, fact arrays reversed. Core does not emit this, and that
 * is the point: it is a shape a conforming engine is ALLOWED to emit, so the
 * adapter's decode has to normalise it. A decoder that skips the trim, keeps the
 * blank lines, or assumes emit order is sorted fails here.
 */
class FStubCaller : public insimul::ICoreCaller {
public:
	std::string PendingRequest;   // what Call() must receive, byte for byte
	std::string PendingResponse;  // what Call() answers with
	std::string RequestMismatch;  // non-empty when the document did not match
	int GenerateCalls = 0;

	bool IsAvailable() const override { return true; }

	bool Call(const std::string& Method, const std::string& ArgsJson, std::string& OutJson) override {
		if (Method == "core.methods") {
			OutJson = "{\"methods\":[\"core.methods\",\"quest.hydrate\",\"quest.radiantTick\","
					  "\"radiant.baseTemplates\",\"radiant.generate\"]}";
			return true;
		}
		if (Method != "radiant.generate") {
			LastErrorText = "insimulcore: unknown method \"" + Method + "\"";
			return false;
		}
		GenerateCalls++;
		if (ArgsJson != PendingRequest) {
			RequestMismatch = "request document differs\n      actual:   " + ArgsJson +
				"\n      expected: " + PendingRequest;
		}
		OutJson = PendingResponse;
		return true;
	}

	std::string LastError() const override { return LastErrorText; }
	std::string Version() const override { return "stub (no libinsimulcore)"; }

private:
	std::string LastErrorText;
};

/** Build the perturbed `{"quests":[...]}` wire document from `expected`. */
std::string StubResponse(const FJsonValue& Case) {
	const FJsonValue* Expected = Case.Find("expected");
	const FJsonValue* Quests = Expected ? Expected->Find("quests") : nullptr;
	std::string Out = "{\"quests\":[";
	if (Quests && Quests->IsArray()) {
		for (std::size_t I = 0; I < Quests->ArrayItems.size(); I++) {
			const FJsonValuePtr& Q = Quests->ArrayItems[I];
			if (I) {
				Out += ',';
			}
			std::vector<std::string> Content = ReadStringArray(Q->Find("content"));
			std::reverse(Content.begin(), Content.end());
			std::string Wire = "\n";
			for (const std::string& Clause : Content) {
				Wire += "  " + Clause + "\n\n";
			}
			std::vector<std::string> Asserts = ReadStringArray(Q->Find("factsToAssert"));
			std::reverse(Asserts.begin(), Asserts.end());
			std::vector<std::string> Retracts = ReadStringArray(Q->Find("factsToRetract"));
			std::reverse(Retracts.begin(), Retracts.end());

			Out += "{\"questId\":" + EscapeJsonString(Q->GetString("questId"));
			Out += ",\"templateId\":" + EscapeJsonString(Q->GetString("templateId"));
			Out += ",\"questContent\":" + EscapeJsonString(Wire);
			Out += ",\"factsToAssert\":[";
			for (std::size_t J = 0; J < Asserts.size(); J++) {
				if (J) { Out += ','; }
				Out += EscapeJsonString(Asserts[J]);
			}
			Out += "],\"factsToRetract\":[";
			for (std::size_t J = 0; J < Retracts.size(); J++) {
				if (J) { Out += ','; }
				Out += EscapeJsonString(Retracts[J]);
			}
			Out += "]}";
		}
	}
	Out += "]}";
	return Out;
}

// ── comparison / classification ─────────────────────────────────────────────

void Compare(const std::string& Name, const std::vector<FQuest>& Actual, const std::vector<FQuest>& Expected) {
	if (Actual.size() != Expected.size()) {
		std::string Msg = "expected " + std::to_string(Expected.size()) + " quest(s), got " +
			std::to_string(Actual.size());
		for (const FQuest& Q : Actual) {
			Msg += "\n      actual:   " + Q.Describe();
		}
		for (const FQuest& Q : Expected) {
			Msg += "\n      expected: " + Q.Describe();
		}
		Fail(Name, Msg);
		return;
	}
	for (std::size_t I = 0; I < Actual.size(); I++) {
		if (!(Actual[I] == Expected[I])) {
			Fail(Name, "quest " + std::to_string(I) + " differs\n      actual:   " + Actual[I].Describe() +
				"\n      expected: " + Expected[I].Describe());
			return;
		}
	}
	std::printf("  . %s (%zu quest(s))\n", Name.c_str(), Actual.size());
}

void ClassifyNone(const std::string& Name, const std::vector<FQuest>& Actual, const std::vector<FQuest>& Expected) {
	if (!Actual.empty()) {
		Regression++;
		Failures++;
		std::printf("  x %s  REGRESSION - the pre-adoption leg produced %zu quest(s)\n", Name.c_str(),
			Actual.size());
		return;
	}
	if (Expected.empty()) {
		Agree++;
		std::printf("  = %s  AGREE (corpus expects no quests)\n", Name.c_str());
		return;
	}
	Gain++;
	std::printf("  + %s  GAIN - core produces %zu quest(s) where this engine produced none\n", Name.c_str(),
		Expected.size());
}

/**
 * A non-corpus check of the one wire-format path the corpus cannot reach: every
 * one of its 11 cases uses a STRING seed, but core accepts a number too, and the
 * adapter must send it as a JSON number rather than a quoted string. Cheap to
 * assert, and silently wrong otherwise.
 */
void CheckNumericSeed() {
	insimul::FRadiantOptions Options;
	Options.Seed = insimul::FRadiantSeed::FromNumber(42.0, "42");
	Options.Now = 7;
	Options.MaxQuests = 3;
	const std::string Args = FRadiantSource::BuildGenerateArgs({"a."}, {"b."}, Options);
	const std::string Want = "{\"kb\":\"a.\\nb.\",\"options\":{\"seed\":42,\"now\":7,\"maxQuests\":3}}";
	if (Args != Want) {
		Fail("numeric-seed-wire-format", "actual:   " + Args + "\n      expected: " + Want);
	} else {
		std::printf("  . numeric-seed-wire-format\n");
	}

	// maxQuests <= 0 means unbounded and must be OMITTED, not sent as 0 (core
	// reads a present maxQuests as a cap, so a 0 would generate nothing).
	Options.MaxQuests = 0;
	const std::string Unbounded = FRadiantSource::BuildGenerateArgs({"a."}, {"b."}, Options);
	if (Unbounded.find("maxQuests") != std::string::npos) {
		Fail("unbounded-omits-maxquests", "maxQuests must be omitted when <= 0: " + Unbounded);
	} else {
		std::printf("  . unbounded-omits-maxquests\n");
	}
}

} // namespace

int main(int argc, char** argv) {
	std::string CorpusDir;
	std::string SourceArg = INSIMUL_RADIANT_DEFAULT_SOURCE;
	for (int I = 1; I < argc; I++) {
		if (std::strcmp(argv[I], "--source") == 0 && I + 1 < argc) {
			SourceArg = argv[++I];
		} else if (CorpusDir.empty()) {
			CorpusDir = argv[I];
		}
	}
#ifdef INSIMUL_RADIANT_DIR
	if (CorpusDir.empty()) {
		CorpusDir = INSIMUL_RADIANT_DIR;
	}
#endif
	if (CorpusDir.empty()) {
		std::fprintf(stderr, "usage: test_radiant [--source core|stub|none] <conformance/radiant dir>\n");
		return 2;
	}
	if (SourceArg != "core" && SourceArg != "stub" && SourceArg != "none") {
		std::fprintf(stderr, "error: --source must be 'core', 'stub' or 'none', got '%s'\n", SourceArg.c_str());
		return 2;
	}
#ifndef INSIMUL_HAVE_CORE_BRIDGE
	if (SourceArg == "core") {
		std::fprintf(stderr,
			"error: this binary was built WITHOUT libinsimulcore, so --source core cannot run.\n"
			"       Configure with an insimul-native checkout visible (see\n"
			"       tools/verify-unreal/CMakeLists.txt) and run the `radiant_bridge` test.\n");
		return 1;
	}
#endif

	const std::vector<std::string> Files = ListJsonFiles(CorpusDir);
	if (Files.empty()) {
		std::fprintf(stderr, "error: no corpus files under %s - the gate executed NOTHING\n", CorpusDir.c_str());
		return 1;
	}

	// ── pick the caller for this leg ────────────────────────────────────────
	FStubCaller Stub;
	insimul::ICoreCaller* Caller = &Stub;
#ifdef INSIMUL_HAVE_CORE_BRIDGE
	insimul::FInsimulCoreBridge Bridge;
	if (SourceArg == "core") {
		if (!Bridge.IsAvailable()) {
			std::fprintf(stderr, "error: the core bridge could not start: %s\n", Bridge.LastError().c_str());
			return 1;
		}
		Caller = &Bridge;
		std::printf("libinsimulcore %s\n", Bridge.Version().c_str());
	}
#endif

	FRadiantSource Source(Caller,
		SourceArg == "none" ? insimul::ERadiantSource::None : insimul::ERadiantSource::Core);

	// The adopted surface, asserted rather than assumed: a bundle that lost
	// radiant.generate must fail here, not silently return zero quests.
	if (SourceArg != "none") {
		const std::vector<std::string> Methods = Source.CoreMethods();
		if (Methods.empty()) {
			std::fprintf(stderr, "error: core.methods failed: %s\n", Source.LastError().c_str());
			return 1;
		}
		std::printf("adopted surface: %s\n", Join(Methods, ", ").c_str());
		if (std::find(Methods.begin(), Methods.end(), std::string("radiant.generate")) == Methods.end()) {
			std::fprintf(stderr, "error: the bundle does not expose radiant.generate\n");
			return 1;
		}
	}

	std::printf("Radiant conformance corpus (%s) - source=%s\n", CorpusDir.c_str(), SourceArg.c_str());

	std::set<std::string> Areas;
	std::set<std::string> Names;
	for (const std::string& File : Files) {
		bool bOk = false;
		const std::string Text = ReadFile(CorpusDir + "/" + File, bOk);
		if (!bOk) {
			std::fprintf(stderr, "error: could not read %s\n", File.c_str());
			Failures++;
			continue;
		}
		const insimul::FJsonParseResult Parsed = insimul::ParseJson(Text);
		if (!Parsed.bOk || !Parsed.Root) {
			std::fprintf(stderr, "error: %s is not valid JSON: %s\n", File.c_str(), Parsed.Error.c_str());
			Failures++;
			continue;
		}
		Areas.insert(Parsed.Root->GetString("area"));
		const FJsonValue* Cases = Parsed.Root->Find("cases");
		if (!Cases || !Cases->IsArray()) {
			std::fprintf(stderr, "error: %s has no `cases` array\n", File.c_str());
			Failures++;
			continue;
		}
		std::printf("%s (%s)\n", File.c_str(), Parsed.Root->GetString("area").c_str());
		for (const FJsonValuePtr& Case : Cases->ArrayItems) {
			const std::string Name = Case->GetString("name");
			if (!Names.insert(Name).second) {
				std::fprintf(stderr, "error: duplicate case name '%s'\n", Name.c_str());
				Failures++;
			}
			CasesRun++;

			if (SourceArg == "stub") {
				Stub.PendingRequest = ExpectedArgs(*Case);
				Stub.PendingResponse = StubResponse(*Case);
				Stub.RequestMismatch.clear();
			}

			std::vector<insimul::FGeneratedRadiantQuest> Generated;
			if (!Source.Generate(ReadStringArray(Case->Find("kb")), ReadStringArray(Case->Find("templates")),
					CaseOptions(*Case), Generated)) {
				Fail(Name, "radiant.generate failed: " + Source.LastError());
				continue;
			}
			if (SourceArg == "stub" && !Stub.RequestMismatch.empty()) {
				Fail(Name, Stub.RequestMismatch);
				continue;
			}

			const std::vector<FQuest> Actual = ActualQuests(Generated);
			const std::vector<FQuest> Expected = ExpectedQuests(*Case);
			if (SourceArg == "none") {
				ClassifyNone(Name, Actual, Expected);
			} else {
				Compare(Name, Actual, Expected);
			}
		}
	}

	// ── the gate cannot pass without having executed something ──────────────
	std::printf("\n%d case(s) executed from %zu corpus file(s)\n", CasesRun, Files.size());
	if (CasesRun == 0) {
		std::fprintf(stderr, "error: the gate executed ZERO cases\n");
		return 1;
	}
	if (CasesRun < MIN_CASES) {
		std::fprintf(stderr, "error: only %d case(s) executed, expected at least %d - the corpus shrank\n",
			CasesRun, MIN_CASES);
		Failures++;
	}
	for (const char* Required : REQUIRED_AREAS) {
		if (Areas.find(Required) == Areas.end()) {
			std::fprintf(stderr, "error: missing corpus area '%s'\n", Required);
			Failures++;
		}
	}

	if (SourceArg == "stub") {
		// The stub leg is worthless if the adapter never called out. Assert it
		// did, once per case.
		if (Stub.GenerateCalls != CasesRun) {
			std::fprintf(stderr, "error: %d case(s) executed but the adapter issued %d radiant.generate call(s)\n",
				CasesRun, Stub.GenerateCalls);
			Failures++;
		}
		CheckNumericSeed();
	}

	if (SourceArg == "none") {
		const int Classified = Agree + Gain + Regression;
		std::printf("classification: %d AGREE, %d GAIN, %d REGRESSION\n", Agree, Gain, Regression);
		if (Classified != CasesRun) {
			std::fprintf(stderr, "error: %d case(s) executed but only %d classified\n", CasesRun, Classified);
			Failures++;
		}
		if (Gain == 0) {
			std::fprintf(stderr,
				"error: ZERO cases classified as GAIN - either the corpus expects nothing anywhere "
				"or this leg is not comparing what it claims to\n");
			Failures++;
		}
	}

	if (Failures > 0) {
		std::printf("FAILED: %d\n", Failures);
		return 1;
	}
	std::printf("PASSED: %d case(s), all %zu areas\n", CasesRun, Areas.size());
	return 0;
}
