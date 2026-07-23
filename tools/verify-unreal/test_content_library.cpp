// Copyright 2024 Insimul. All Rights Reserved.
//
// Host tests for FInsimulContentLibrary (US-IM1). Imports the shared content
// library conformance fixture (conformance/content/library-basic.json) and
// asserts the expected native entity set materializes, then walks the
// invalid-cases corpus and asserts every malformed library is rejected with a
// clear, schema-validated error.
//
// Cross-engine parity: the entity counts and rejection messages below are the
// Unreal leg of the author-once/use-anywhere content-portability proof — every
// engine's importer consumes the SAME fixtures and must agree.

#include "InsimulContentLibrary.h"
#include "InsimulCanonicalJson.h"
#include "InsimulJson.h"

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

template <typename A, typename B>
void CheckEq(const A& Actual, const B& Expected, const char* Message) {
	++g_Checks;
	if (!(Actual == static_cast<A>(Expected))) {
		++g_Failures;
		std::ostringstream Oss;
		Oss << "  [FAIL] " << Message << " (expected " << Expected << ", got " << Actual << ")";
		std::fprintf(stderr, "%s\n", Oss.str().c_str());
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

} // namespace

int main() {
	using insimul::FInsimulContentLibrary;

	std::printf("== FInsimulContentLibrary host tests ==\n");

	// ── AC: the shared conformance fixture imports to the expected entities ──
	{
		const std::string Json = ReadFixture("library-basic.json");
		FInsimulContentLibrary Lib;
		std::string Error;
		const bool bImported = Lib.ImportFromJson(Json, Error);
		Check(bImported, "library-basic imports");
		if (!bImported) {
			std::fprintf(stderr, "  import error: %s\n", Error.c_str());
		}

		CheckEq(Lib.SchemaVersion(), 1, "schema version == 1");
		CheckEq(Lib.Library().Id, std::string("conformance-basic"), "library id");
		CheckEq(Lib.Library().TargetLanguage, std::string("es"), "target language");

		CheckEq(Lib.ItemCount(), std::size_t(3), "item count");
		CheckEq(Lib.QuestCount(), std::size_t(2), "quest count");
		CheckEq(Lib.CharacterCount(), std::size_t(2), "character count");
		CheckEq(Lib.TownCount(), std::size_t(2), "town count");
		CheckEq(Lib.NarrativeCount(), std::size_t(2), "narrative count");
		CheckEq(Lib.TotalEntityCount(), std::size_t(11), "total entity count");

		// Spot-check that fields materialize across the entity kinds.
		const insimul::FContentItemDTO* Sword = Lib.FindItem("item-sword");
		Check(Sword != nullptr, "item-sword found");
		if (Sword) {
			CheckEq(Sword->Name, std::string("Iron Sword"), "item-sword name");
			CheckEq(Sword->ItemType, std::string("weapon"), "item-sword type");
			CheckEq(Sword->Value, 25, "item-sword value");
		}

		const insimul::FQuestDTO* Quest = Lib.FindQuest("quest-lost-map");
		Check(Quest != nullptr, "quest-lost-map found");
		if (Quest) {
			CheckEq(Quest->GiverNpcId, std::string("char-mira"), "quest giver");
			Check(!Quest->Content.empty(), "quest carries Prolog content");
		}

		const insimul::FCharacterDTO* Char = Lib.FindCharacter("char-tomas");
		Check(Char != nullptr, "char-tomas found");
		if (Char) {
			CheckEq(Char->Occupation, std::string("merchant"), "character occupation");
			Check(Char->bIsAlive, "character is alive");
		}

		const insimul::FSettlementDTO* Town = Lib.FindTown("town-puerto");
		Check(Town != nullptr, "town-puerto found");
		if (Town) {
			CheckEq(Town->Population, 1200, "town population");
		}

		const insimul::FContentNarrativeDTO* Narr = Lib.FindNarrative("narr-intro");
		Check(Narr != nullptr, "narr-intro found");
		if (Narr) {
			CheckEq(Narr->Title, std::string("Arrival"), "narrative title");
			Check(!Narr->Body.empty(), "narrative carries body");
		}
	}

	// ── AC: invalid libraries are rejected with a clear, schema-validated error ──
	{
		const std::string Json = ReadFixture("invalid-cases.json");
		insimul::FJsonParseResult Parsed = insimul::ParseJson(Json);
		Check(Parsed.bOk && Parsed.Root && Parsed.Root->IsObject(), "invalid-cases corpus parses");

		const insimul::FJsonValue* Cases =
			(Parsed.bOk && Parsed.Root) ? Parsed.Root->Find("cases") : nullptr;
		Check(Cases != nullptr && Cases->IsArray(), "invalid-cases has cases[]");

		if (Cases && Cases->IsArray()) {
			for (const insimul::FJsonValuePtr& Case : Cases->ArrayItems) {
				if (!Case || !Case->IsObject()) {
					continue;
				}
				const std::string Name = Case->GetString("name", "<unnamed>");
				const std::string Expected = Case->GetString("expectedErrorContains");
				const insimul::FJsonValue* LibNode = Case->Find("library");
				if (!LibNode) {
					std::fprintf(stderr, "  [FAIL] case %s missing library\n", Name.c_str());
					++g_Failures;
					continue;
				}

				const std::string LibJson = insimul::CanonicalJsonStringify(*LibNode);
				FInsimulContentLibrary Lib;
				std::string Error;
				const bool bImported = Lib.ImportFromJson(LibJson, Error);

				std::ostringstream RejMsg;
				RejMsg << "case " << Name << " is rejected";
				Check(!bImported, RejMsg.str().c_str());
				Check(!Lib.IsLoaded(), (std::string("case ") + Name + " leaves importer not loaded").c_str());

				const bool bMatches = !Expected.empty() && Error.find(Expected) != std::string::npos;
				if (!bMatches) {
					std::ostringstream Oss;
					Oss << "  [FAIL] case " << Name << " error should contain \"" << Expected
						<< "\" (got \"" << Error << "\")";
					std::fprintf(stderr, "%s\n", Oss.str().c_str());
					++g_Failures;
				}
				++g_Checks;
			}
		}
	}

	std::printf("== %d checks, %d failure(s) ==\n", g_Checks, g_Failures);
	return g_Failures == 0 ? 0 : 1;
}
