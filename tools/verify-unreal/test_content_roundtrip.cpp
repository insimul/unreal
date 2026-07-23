// Copyright 2024 Insimul. All Rights Reserved.
//
// Round-trip parity host tests for FInsimulContentLibrary (US-IM2).
//
// The per-engine leg of the author-once/use-anywhere proof: importing the
// shared conformance library must carry the SAME semantics the corpus pins (and
// that the other engines materialize). We prove that by re-serializing the
// MATERIALIZED entities — not the raw input — into the shared canonical form and
// pinning the result three ways:
//
//   1. It byte-matches the committed golden projection (library-golden.json).
//   2. Its SHA-256 matches the cross-engine vector (golden-vectors.json) that
//      every runtime independently reproduces.
//   3. Re-importing the projection re-materializes the identical entity set and
//      projection (a full import -> project -> import -> project fixpoint), so
//      the projection is itself a lossless, portable content library.

#include "InsimulCanonicalJson.h"
#include "InsimulContentLibrary.h"
#include "InsimulJson.h"
#include "InsimulSha256.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#ifndef INSIMUL_CONTENT_DIR
#define INSIMUL_CONTENT_DIR "."
#endif

namespace {

int g_Failures = 0;
int g_Checks = 0;

void Check(bool Condition, const char* Message) {
	++g_Checks;
	if (!Condition) {
		++g_Failures;
		std::fprintf(stderr, "  [FAIL] %s\n", Message);
	}
}

void CheckStr(const std::string& Actual, const std::string& Expected, const char* Message) {
	++g_Checks;
	if (Actual != Expected) {
		++g_Failures;
		std::fprintf(stderr, "  [FAIL] %s\n    expected: %s\n    got:      %s\n", Message,
			Expected.c_str(), Actual.c_str());
	}
}

std::string ReadFixture(const std::string& Name) {
	const std::string Path = std::string(INSIMUL_CONTENT_DIR) + "/" + Name;
	std::ifstream In(Path, std::ios::binary);
	if (!In) {
		std::fprintf(stderr, "  [FAIL] could not open fixture: %s\n", Path.c_str());
		++g_Failures;
		return std::string();
	}
	std::ostringstream Buffer;
	Buffer << In.rdbuf();
	return Buffer.str();
}

/** Canonicalize a JSON document (whitespace-insensitive golden comparison). */
std::string CanonOf(const std::string& Json) {
	insimul::FJsonParseResult Parsed = insimul::ParseJson(Json);
	if (!Parsed.bOk || !Parsed.Root) {
		return std::string();
	}
	return insimul::CanonicalJsonStringify(*Parsed.Root);
}

/** Read a committed cross-engine round-trip vector from golden-vectors.json. */
std::string VectorFor(const std::string& SourceName) {
	const std::string Json = ReadFixture("golden-vectors.json");
	insimul::FJsonParseResult Parsed = insimul::ParseJson(Json);
	if (!Parsed.bOk || !Parsed.Root) {
		return std::string();
	}
	const insimul::FJsonValue* Vectors = Parsed.Root->Find("vectors");
	const insimul::FJsonValue* Table = Vectors ? Vectors : Parsed.Root.get();
	return Table->GetString(SourceName);
}

} // namespace

int main() {
	using insimul::FInsimulContentLibrary;

	std::printf("== FInsimulContentLibrary round-trip host tests ==\n");

	FInsimulContentLibrary Lib;
	std::string Error;
	const bool bImported = Lib.ImportFromJson(ReadFixture("library-basic.json"), Error);
	Check(bImported, "library-basic imports");
	if (!bImported) {
		std::fprintf(stderr, "  import error: %s\n", Error.c_str());
	}

	const std::string Projection = Lib.CanonicalProjection();
	Check(!Projection.empty(), "projection is non-empty");

	// ── AC: the imported content matches the shared golden (same semantics) ──
	{
		const std::string Golden = ReadFixture("library-golden.json");
		Check(!Golden.empty(), "committed golden projection exists");
		CheckStr(Projection, CanonOf(Golden),
			"projection byte-matches committed golden (library-golden.json)");
	}

	// ── AC: SHA-256 matches the cross-engine round-trip vector ──────────────
	{
		const std::string Expected = VectorFor("library-basic.json");
		Check(!Expected.empty(), "golden vector exists for library-basic.json");
		CheckStr(Lib.ProjectionIntegrity(), Expected,
			"projection integrity matches cross-engine vector");
		// The vector really is the SHA-256 of the projection bytes.
		CheckStr(insimul::Sha256Hex(Projection), Expected, "vector is sha256(projection)");
	}

	// ── Canonical projection is idempotent (stable under re-canonicalization) ──
	CheckStr(CanonOf(Projection), Projection, "projection is canonical/idempotent");

	// ── AC: full round-trip fixpoint — the projection is a lossless library ──
	// Import the projection AS a content library; it must re-materialize the
	// same entity set and project to identical bytes. This is what proves the
	// import preserved semantics rather than merely echoing the input.
	{
		FInsimulContentLibrary Reimported;
		std::string ReError;
		const bool bReimported = Reimported.ImportFromJson(Projection, ReError);
		Check(bReimported, "projection re-imports as a content library");
		if (!bReimported) {
			std::fprintf(stderr, "  re-import error: %s\n", ReError.c_str());
		}

		Check(Reimported.TotalEntityCount() == Lib.TotalEntityCount(),
			"re-import materializes the same entity count");
		Check(Reimported.ItemCount() == Lib.ItemCount(), "item count parity");
		Check(Reimported.QuestCount() == Lib.QuestCount(), "quest count parity");
		Check(Reimported.CharacterCount() == Lib.CharacterCount(), "character count parity");
		Check(Reimported.TownCount() == Lib.TownCount(), "town count parity");
		Check(Reimported.NarrativeCount() == Lib.NarrativeCount(), "narrative count parity");

		CheckStr(Reimported.CanonicalProjection(), Projection,
			"import -> project -> import -> project is a fixpoint");
	}

	std::printf("== %d checks, %d failure(s) ==\n", g_Checks, g_Failures);
	return g_Failures == 0 ? 0 : 1;
}
