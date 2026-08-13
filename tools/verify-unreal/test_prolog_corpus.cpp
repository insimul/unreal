// test_prolog_corpus.cpp — the vendored PROLOG corpus, EXECUTED (US-2 of tasklist 146).
//
// WHY THIS EXISTS. Before this file, conformance/prolog/ was checked for
// PROVENANCE only — vendor-conformance.mjs proved the files were present, hashed
// what the manifest recorded and were byte-identical to core — and executed by
// NOTHING this repo can run. That is exactly the failure mode the corpus guard
// itself was written for (see vendor-conformance.mjs's header: the corpus had
// rotted to 41 of core's 76 cases and nothing noticed), one level up: a vendored
// corpus nothing runs is a checked-in file.
//
// Tasklist 146 US-2 makes that the difference between the story passing and
// failing, because the band-120 modules arrive with their PREDICATE vocabulary as
// ten new packs — mechanic-combat, -stamina, -perception, -traversal, -skill,
// -equipment, -gameplay-state, plus scaffold / agent-ai / geo-map. 255 cases
// total, 125 of them in the mechanic-* packs.
//
// WHAT IT PROVES. Every case under conformance/prolog/*.json is consulted and
// queried through insimul::InsimulKB — the plugin's OWN RAII wrapper over the
// libinsimul C ABI (Source/InsimulRuntime/Private/Prolog/InsimulKB.h), the same
// class UInsimulPrologSubsystem wraps in UStructs — and its solution set is
// compared to the golden `expected` as an unordered MULTISET, which is
// conformance/README.md's order-insensitivity rule.
//
// So this is not a second implementation reading the corpus: it is this repo's
// shipping Prolog path, pointed at core's vectors. That is the same shape as
// US-1's rule — execution host-side, decisions core-side.
//
// WHAT IT DOES NOT PROVE. Not one line of UE-coupled code runs here.
// UInsimulPrologSubsystem's UStruct marshalling (FInsimulPrologBinding and the
// Blueprint surface) needs a real engine; VERIFICATION.md is its pass. This leg
// proves libinsimul + InsimulKB + the corpus, and says so rather than implying
// more.
//
// It also says NOTHING about the band-120 DECISION layers — CombatResolver,
// Market, DetectionTracker and the rest. Those live in core behind bridge rows
// that do not exist (ctest `mechanic_bridge` pins the five-method list that
// proves it). tools/verify-mechanics/check-mechanic-corpora.mjs is the gate that
// measures that half, and RUNTIME_CORE_ADOPTION.md section 13 classifies it.
//
// ONE PROCESS, ONE KB PER CASE — AND THAT IS A MEASUREMENT.
// Unity's probe (tasklist 145 US-2, its check-prolog.mjs) could not do this: the
// libinsimul it measured (git f1548a4) ABORTED on the second insimul_kb_destroy
// in a process, and leaked KBs stopped loading library modules after ~64, so it
// had to spawn a fresh process per case at ~40ms each. Re-measured here against
// the shipping libinsimul (git e019244, trealla v2.106.1): 200 create/consult/
// query/destroy cycles in one process, clean. Both defects are FIXED, so this
// leg runs all 255 cases in-process — and because it does, it is itself the
// standing regression test for them: if either comes back, this binary crashes
// or its consults start failing, and ctest says so.
//
//   ctest --test-dir tools/verify-unreal/build -R prolog_corpus --output-on-failure
//
// Registered ONLY when a libinsimul is found (the same discovery the bridge legs
// use), so a clone with no insimul-native beside it still configures. That
// absence is a configure-time WARNING, and -DINSIMUL_REQUIRE_BINARIES=ON turns
// it into a hard failure — this harness's --require-binaries.

#include "InsimulJson.h"
#include "InsimulKB.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using insimul::FJsonValue;
using insimul::FJsonValuePtr;
using insimul::InsimulKB;
using insimul::PrologBinding;
using insimul::PrologValue;
using insimul::PrologValueType;

// The corpus as vendored at adoption time: 21 files / 255 cases, of which the
// seven mechanic-* packs are the band-120 predicate half. Growing the corpus
// must not break the gate; shrinking it must. conformance/VENDORED.json's
// caseFloor guards the same number from the provenance side — this guards it
// from the execution side, where it decides whether a case actually ran.
static const int MIN_CASES = 255;

// Files that must exist and must contribute cases. Named individually because
// "255 cases" could be reached with the band-120 packs missing entirely.
static const char* REQUIRED_FILES[] = {
	"mechanic-combat.json",
	"mechanic-equipment.json",
	"mechanic-gameplay-state.json",
	"mechanic-perception.json",
	"mechanic-routine.json",
	"mechanic-skill.json",
	"mechanic-stamina.json",
	"mechanic-traversal.json",
	"agent-ai.json",
	"geo-map.json",
	"scaffold.json",
};

namespace {

int Failures = 0;
int CasesRun = 0;
int Amended = 0;

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

// ── the amendment table (mirrored from conformance/README.md) ────────────────
//
// The corpus was authored against tau-prolog and is deliberately left UNAMENDED
// ON DISK: it is the byte-for-byte copy every runtime vendors, so editing a case
// to please one engine would erase the evidence downstream. Exactly one case
// needs a rewrite to run, and EVERY harness applies the same one, in memory,
// with a printed [AMEND] line — never a skip.
//
//   assert-retract.json::asserta-prepends uses log/1 as a user dynamic
//   predicate. ISO reserves `log` only as an evaluable functor, so tau-prolog
//   accepted it; Trealla additionally registers the arithmetic/list functors as
//   STATIC BUILTIN predicates, so asserta(log(0)) raises
//   permission_error(modify, static_procedure, log/1). The amendment renames the
//   predicate to `entry`.
//
// A rewrite rule that silently decides whether a case passes is exactly the kind
// of thing that rots, so this table is not merely applied: every row must MATCH
// something (a row that matches nothing is stale and fails), and every amended
// case is ALSO run unamended and must FAIL that way (a case that has started
// passing unamended means the amendment is obsolete and fails too).
struct FAmendment {
	const char* Area;
	const char* CaseName;
	const char* From;
	const char* To;
	int Matches; // filled in during the run
};

FAmendment Amendments[] = {
	{"assert-retract", "asserta-prepends", "log(", "entry(", 0},
	{"assert-retract", "asserta-prepends", "log/", "entry/", 0},
};

/** `Text` with every amendment declared for this case applied. */
std::string Amend(const std::string& Area, const std::string& CaseName, const std::string& Text, bool& bApplied) {
	std::string Cur = Text;
	for (FAmendment& A : Amendments) {
		if (Area != A.Area || CaseName != A.CaseName) {
			continue;
		}
		const std::string From(A.From);
		const std::string To(A.To);
		std::string Out;
		std::size_t Pos = 0;
		for (;;) {
			const std::size_t Hit = Cur.find(From, Pos);
			if (Hit == std::string::npos) {
				Out.append(Cur, Pos, std::string::npos);
				break;
			}
			Out.append(Cur, Pos, Hit - Pos);
			Out += To;
			Pos = Hit + From.size();
			A.Matches++;
			bApplied = true;
		}
		Cur = Out;
	}
	return Cur;
}

bool HasAmendment(const std::string& Area, const std::string& CaseName) {
	for (const FAmendment& A : Amendments) {
		if (Area == A.Area && CaseName == A.CaseName) {
			return true;
		}
	}
	return false;
}

// ── canonical rendering + multiset comparison ───────────────────────────────
//
// The same rules as core's TS runner and Unity's check-prolog.mjs: keys sorted,
// integral numbers normalized so an engine answering 1.0 matches an expected 1,
// and the solution SET compared as an unordered multiset.

std::string CanonNumber(double N) {
	if (N == static_cast<double>(static_cast<long long>(N))) {
		char Buf[32];
		std::snprintf(Buf, sizeof(Buf), "%lld", static_cast<long long>(N));
		return std::string("n:") + Buf;
	}
	char Buf[64];
	std::snprintf(Buf, sizeof(Buf), "%.17g", N);
	return std::string("n:") + Buf;
}

std::string CanonJson(const FJsonValue& V);

/** One expected value from the corpus, canonically. */
std::string CanonExpected(const FJsonValue& V) {
	switch (V.Type) {
		case insimul::EJsonType::Null:
			return "null";
		case insimul::EJsonType::Bool:
			return V.BoolValue ? "b:true" : "b:false";
		case insimul::EJsonType::String:
			return "s:" + V.StringValue;
		case insimul::EJsonType::Number:
			return CanonNumber(V.NumberValue);
		default:
			// Compound terms / lists, rendered as the ABI's JSON shape. No case
			// in the corpus needs this today (conformance/README.md: `expected`
			// is scalar bindings), but a future one must not silently compare
			// equal to everything.
			return "j:" + CanonJson(V);
	}
}

std::string JsonEscape(const std::string& S) {
	std::string Out;
	for (const char C : S) {
		if (C == '"' || C == '\\') {
			Out += '\\';
			Out += C;
		} else if (C == '\n') {
			Out += "\\n";
		} else {
			Out += C;
		}
	}
	return Out;
}

std::string CanonJson(const FJsonValue& V) {
	switch (V.Type) {
		case insimul::EJsonType::Null:
			return "null";
		case insimul::EJsonType::Bool:
			return V.BoolValue ? "true" : "false";
		case insimul::EJsonType::String:
			return "\"" + JsonEscape(V.StringValue) + "\"";
		case insimul::EJsonType::Number:
			return CanonNumber(V.NumberValue).substr(2);
		case insimul::EJsonType::Array: {
			std::string Out = "[";
			for (std::size_t i = 0; i < V.ArrayItems.size(); ++i) {
				if (i > 0) {
					Out += ",";
				}
				Out += CanonJson(*V.ArrayItems[i]);
			}
			return Out + "]";
		}
		case insimul::EJsonType::Object: {
			std::vector<std::pair<std::string, const FJsonValue*>> Sorted;
			for (const auto& KV : V.ObjectItems) {
				Sorted.emplace_back(KV.first, KV.second.get());
			}
			std::sort(Sorted.begin(), Sorted.end(),
				[](const auto& A, const auto& B) { return A.first < B.first; });
			std::string Out = "{";
			for (std::size_t i = 0; i < Sorted.size(); ++i) {
				if (i > 0) {
					Out += ",";
				}
				Out += "\"" + JsonEscape(Sorted[i].first) + "\":" + CanonJson(*Sorted[i].second);
			}
			return Out + "}";
		}
	}
	return "null";
}

/** One value the engine bound, in the same canonical vocabulary. */
std::string CanonBound(const PrologValue& V) {
	switch (V.Type) {
		case PrologValueType::Null:
			return "null";
		case PrologValueType::Atom:
			return "s:" + V.Text;
		case PrologValueType::Int: {
			char Buf[32];
			std::snprintf(Buf, sizeof(Buf), "%lld", V.Int);
			return std::string("n:") + Buf;
		}
		case PrologValueType::Float:
			return CanonNumber(V.Float);
		case PrologValueType::List:
		case PrologValueType::Compound:
			return "j:" + V.ToDisplayString();
	}
	return "null";
}

std::string CanonExpectedBinding(const FJsonValue& Obj) {
	std::vector<std::pair<std::string, std::string>> Pairs;
	if (Obj.IsObject()) {
		for (const auto& KV : Obj.ObjectItems) {
			Pairs.emplace_back(KV.first, CanonExpected(*KV.second));
		}
	}
	std::sort(Pairs.begin(), Pairs.end());
	std::string Out = "{";
	for (std::size_t i = 0; i < Pairs.size(); ++i) {
		if (i > 0) {
			Out += ",";
		}
		Out += Pairs[i].first + "=" + Pairs[i].second;
	}
	return Out + "}";
}

std::string CanonBoundBinding(const PrologBinding& B) {
	std::vector<std::pair<std::string, std::string>> Pairs;
	for (const auto& KV : B.Vars) {
		Pairs.emplace_back(KV.first, CanonBound(KV.second));
	}
	std::sort(Pairs.begin(), Pairs.end());
	std::string Out = "{";
	for (std::size_t i = 0; i < Pairs.size(); ++i) {
		if (i > 0) {
			Out += ",";
		}
		Out += Pairs[i].first + "=" + Pairs[i].second;
	}
	return Out + "}";
}

std::string Describe(std::vector<std::string> Set) {
	std::sort(Set.begin(), Set.end());
	std::string Out = "[";
	for (std::size_t i = 0; i < Set.size(); ++i) {
		if (i > 0) {
			Out += ", ";
		}
		Out += Set[i];
	}
	return Out + "]";
}

bool SameSolutionSet(std::vector<std::string> A, std::vector<std::string> B) {
	std::sort(A.begin(), A.end());
	std::sort(B.begin(), B.end());
	return A == B;
}

// ── one case ────────────────────────────────────────────────────────────────

struct FCase {
	std::string File;
	std::string Area;
	std::string Name;
	std::string Program; // kb clauses joined by newlines
	std::string Query;
	std::vector<std::string> Expected;
};

enum class ERunResult {
	Match,
	Mismatch,
	EngineError,
};

/**
 * A fresh KB, consulted, queried, and destroyed when it leaves scope — the
 * lifetime the measurement above says is safe again. Returns the engine's
 * solution set (canonical) in OutActual, and the reason in OutWhy on failure.
 */
ERunResult RunCase(const FCase& C, std::vector<std::string>& OutActual, std::string& OutWhy) {
	InsimulKB KB;
	if (!KB.IsValid()) {
		OutWhy = "insimul_kb_create returned NULL";
		return ERunResult::EngineError;
	}
	if (!KB.Consult(C.Program)) {
		OutWhy = "consult failed: " + KB.LastError();
		return ERunResult::EngineError;
	}
	std::vector<PrologBinding> Solutions;
	if (!KB.QueryAll(C.Query, Solutions)) {
		OutWhy = "query failed: " + KB.LastError();
		return ERunResult::EngineError;
	}
	OutActual.clear();
	for (const PrologBinding& B : Solutions) {
		OutActual.push_back(CanonBoundBinding(B));
	}
	if (!SameSolutionSet(OutActual, C.Expected)) {
		OutWhy = "expected " + Describe(C.Expected) + "\n      actual   " + Describe(OutActual);
		return ERunResult::Mismatch;
	}
	return ERunResult::Match;
}

} // namespace

int main(int argc, char** argv) {
	std::string CorpusDir;
#ifdef INSIMUL_PROLOG_DIR
	CorpusDir = INSIMUL_PROLOG_DIR;
#endif
	for (int i = 1; i < argc; ++i) {
		if (std::strcmp(argv[i], "--corpus") == 0 && i + 1 < argc) {
			CorpusDir = argv[++i];
		}
	}
	if (CorpusDir.empty()) {
		std::printf("prolog_corpus: no corpus directory (INSIMUL_PROLOG_DIR unset, no --corpus)\n");
		return 1;
	}

	std::printf("Prolog conformance corpus, executed through insimul::InsimulKB\n");
	std::printf("  library: %s\n", InsimulKB::Version().c_str());
	std::printf("  corpus:  %s\n", CorpusDir.c_str());

	// ── load ────────────────────────────────────────────────────────────────
	const std::vector<std::string> Files = ListJsonFiles(CorpusDir);
	if (Files.empty()) {
		std::printf("  x no *.json under %s\n", CorpusDir.c_str());
		return 1;
	}
	std::vector<FCase> Cases;
	std::map<std::string, int> PerFile;
	for (const std::string& File : Files) {
		bool bOk = false;
		const std::string Text = ReadFile(CorpusDir + "/" + File, bOk);
		if (!bOk) {
			Fail(File, "cannot read");
			continue;
		}
		const insimul::FJsonParseResult Parsed = insimul::ParseJson(Text);
		if (!Parsed.bOk || !Parsed.Root || !Parsed.Root->IsObject()) {
			Fail(File, "not a JSON object: " + Parsed.Error);
			continue;
		}
		const std::string Area = Parsed.Root->GetString("area");
		const FJsonValue* CaseList = Parsed.Root->Find("cases");
		if (!CaseList || !CaseList->IsArray()) {
			Fail(File, "no `cases` array");
			continue;
		}
		for (const FJsonValuePtr& Item : CaseList->ArrayItems) {
			FCase C;
			C.File = File;
			C.Area = Area;
			C.Name = Item->GetString("name");
			C.Query = Item->GetString("query");
			const FJsonValue* KbLines = Item->Find("kb");
			if (KbLines && KbLines->IsArray()) {
				for (const FJsonValuePtr& Line : KbLines->ArrayItems) {
					C.Program += Line->AsString();
					C.Program += "\n";
				}
			}
			const FJsonValue* ExpectedList = Item->Find("expected");
			if (ExpectedList && ExpectedList->IsArray()) {
				for (const FJsonValuePtr& E : ExpectedList->ArrayItems) {
					C.Expected.push_back(CanonExpectedBinding(*E));
				}
			}
			PerFile[File]++;
			Cases.push_back(std::move(C));
		}
	}

	// ── the corpus itself ───────────────────────────────────────────────────
	for (const char* Required : REQUIRED_FILES) {
		if (PerFile.find(Required) == PerFile.end() || PerFile[Required] == 0) {
			Fail(std::string("corpus/") + Required, "missing or contributes no cases");
		}
	}
	if (static_cast<int>(Cases.size()) < MIN_CASES) {
		Fail("corpus", "holds " + std::to_string(Cases.size()) + " case(s), the floor is "
			+ std::to_string(MIN_CASES) + " — a shrinking corpus is a failure, not a smaller pass");
	}

	// ── run ─────────────────────────────────────────────────────────────────
	std::map<std::string, int> PerArea;
	for (const FCase& Original : Cases) {
		FCase C = Original;
		bool bApplied = false;
		C.Program = Amend(C.Area, C.Name, C.Program, bApplied);
		C.Query = Amend(C.Area, C.Name, C.Query, bApplied);
		const std::string Label = C.File + "::" + C.Name;

		if (bApplied) {
			Amended++;
			std::printf("  [AMEND] %s — applied the conformance/README.md rewrite in memory\n", Label.c_str());

			// The amendment must still be NEEDED. If the unamended case now runs
			// clean, the rewrite is obsolete and must be retired rather than left
			// to quietly decide the verdict.
			std::vector<std::string> Ignored;
			std::string Why;
			if (RunCase(Original, Ignored, Why) == ERunResult::Match) {
				Fail(Label, "the amendment is STALE — this case now passes UNAMENDED. "
					"Retire the row here and in conformance/README.md.");
			}
		} else if (HasAmendment(C.Area, C.Name)) {
			Fail(Label, "an amendment is declared for this case but matched nothing — stale table");
		}

		std::vector<std::string> Actual;
		std::string Why;
		const ERunResult Result = RunCase(C, Actual, Why);
		CasesRun++;
		PerArea[C.Area.empty() ? C.File : C.Area]++;
		if (Result != ERunResult::Match) {
			Fail(Label, Why);
		}
	}

	// Every declared amendment row must have matched something.
	for (const FAmendment& A : Amendments) {
		if (A.Matches == 0) {
			Fail(std::string("amendment ") + A.Area + "::" + A.CaseName,
				std::string("the rewrite '") + A.From + "' -> '" + A.To
					+ "' matched nothing — the table has drifted from the corpus");
		}
	}

	if (CasesRun != static_cast<int>(Cases.size())) {
		Fail("corpus", "loaded " + std::to_string(Cases.size()) + " case(s) but executed "
			+ std::to_string(CasesRun) + " — a gate that skips is not a gate");
	}

	// ── the controls ────────────────────────────────────────────────────────
	//
	// "255 of 255 matched" is only meaningful if a mismatch could have been seen.
	// A live control (a goal with a known answer) and a negative control (the
	// same goal against a deliberately wrong expectation) prove the driver and
	// the comparator both work in THIS process, right now.
	{
		FCase Live;
		Live.File = "control";
		Live.Name = "live-control";
		Live.Program = "colour(red).\ncolour(green).\n";
		Live.Query = "colour(X)";
		Live.Expected = {"{X=s:red}", "{X=s:green}"};
		std::vector<std::string> Actual;
		std::string Why;
		if (RunCase(Live, Actual, Why) != ERunResult::Match) {
			Fail("control::live", "the driver cannot run a trivial goal: " + Why);
		}

		FCase Negative = Live;
		Negative.Name = "negative-control";
		Negative.Expected = {"{X=s:red}", "{X=s:blue}"};
		if (RunCase(Negative, Actual, Why) != ERunResult::Mismatch) {
			Fail("control::negative", "a WRONG expectation compared equal — the comparator is decorative");
		}

		FCase Broken = Live;
		Broken.Name = "syntax-control";
		Broken.Program = "colour(red).\ncolour(\n";
		if (RunCase(Broken, Actual, Why) != ERunResult::EngineError) {
			Fail("control::syntax", "a malformed program consulted clean — consult errors are not being seen");
		}
	}

	// ── report ──────────────────────────────────────────────────────────────
	std::printf("  areas:\n");
	for (const auto& KV : PerArea) {
		std::printf("    %-22s %d case(s)\n", KV.first.c_str(), KV.second);
	}
	std::printf("  %d case(s) executed, %d amendment rewrite(s) applied, %d failure(s)\n",
		CasesRun, Amended, Failures);
	if (Failures == 0) {
		std::printf("  ok: every vendored Prolog case matches core's golden solution set\n");
	}
	return Failures == 0 ? 0 : 1;
}
