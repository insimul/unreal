// Copyright 2024 Insimul. All Rights Reserved.
//
// Implementation of the radiant-generation adapter. std-only; the only thing it
// knows about the outside world is ICoreCaller (JSON in, JSON out).

#include "InsimulRadiantSource.h"

#include "InsimulCanonicalJson.h"

#include <sstream>

namespace insimul {

namespace {

std::string Trim(const std::string& S) {
	const std::size_t Begin = S.find_first_not_of(" \t\r\n");
	if (Begin == std::string::npos) {
		return std::string();
	}
	const std::size_t End = S.find_last_not_of(" \t\r\n");
	return S.substr(Begin, End - Begin + 1);
}

/** Read a JSON string array into a vector, preserving order. */
std::vector<std::string> ReadStringArray(const FJsonValue* Array) {
	std::vector<std::string> Out;
	if (Array && Array->IsArray()) {
		for (const FJsonValuePtr& Item : Array->ArrayItems) {
			if (Item) {
				Out.push_back(Item->AsString());
			}
		}
	}
	return Out;
}

} // namespace

std::vector<std::string> FRadiantSource::SplitContentClauses(const std::string& Content) {
	std::vector<std::string> Clauses;
	std::istringstream In(Content);
	std::string Line;
	while (std::getline(In, Line)) {
		const std::string Clause = Trim(Line);
		if (!Clause.empty()) {
			Clauses.push_back(Clause);
		}
	}
	return Clauses;
}

std::string FRadiantSource::BuildGenerateArgs(const std::vector<std::string>& Kb,
	const std::vector<std::string>& Templates,
	const FRadiantOptions& Options) {
	// One program: world facts first, then the template pack. See the header —
	// this order is contract, not convenience.
	std::string Program;
	bool bFirst = true;
	for (const std::string& Line : Kb) {
		if (!bFirst) {
			Program += '\n';
		}
		Program += Line;
		bFirst = false;
	}
	for (const std::string& Line : Templates) {
		if (!bFirst) {
			Program += '\n';
		}
		Program += Line;
		bFirst = false;
	}

	std::string Args = "{\"kb\":";
	Args += CanonicalJsonString(Program);
	Args += ",\"options\":{\"seed\":";
	if (Options.Seed.bNumeric) {
		Args += CanonicalNumber(Options.Seed.Number, Options.Seed.RawNumber);
	} else {
		Args += CanonicalJsonString(Options.Seed.Text);
	}
	Args += ",\"now\":";
	Args += std::to_string(Options.Now);
	if (Options.MaxQuests > 0) {
		// Omitted rather than sent as 0: core reads a present `maxQuests` as a
		// cap, and a cap of 0 would generate nothing.
		Args += ",\"maxQuests\":";
		Args += std::to_string(Options.MaxQuests);
	}
	Args += "}}";
	return Args;
}

bool FRadiantSource::DecodeQuests(const std::string& Json,
	std::vector<FGeneratedRadiantQuest>& OutQuests,
	std::string& OutError) {
	const FJsonParseResult Parsed = ParseJson(Json);
	if (!Parsed.bOk || !Parsed.Root) {
		OutError = "core returned malformed JSON: " + Parsed.Error;
		return false;
	}
	const FJsonValue* Quests = Parsed.Root->Find("quests");
	if (!Quests || !Quests->IsArray()) {
		OutError = "core result has no `quests` array";
		return false;
	}
	for (const FJsonValuePtr& Item : Quests->ArrayItems) {
		if (!Item) {
			continue;
		}
		FGeneratedRadiantQuest Quest;
		Quest.QuestId = Item->GetString("questId");
		Quest.TemplateId = Item->GetString("templateId");
		Quest.QuestContent = Item->GetString("questContent");
		Quest.ContentClauses = SplitContentClauses(Quest.QuestContent);
		Quest.FactsToAssert = ReadStringArray(Item->Find("factsToAssert"));
		Quest.FactsToRetract = ReadStringArray(Item->Find("factsToRetract"));
		OutQuests.push_back(std::move(Quest));
	}
	return true;
}

bool FRadiantSource::IsCoreAvailable() const {
	return SourceMode == ERadiantSource::Core && Caller != nullptr && Caller->IsAvailable();
}

FJsonValuePtr FRadiantSource::CallObject(const std::string& Method, const std::string& ArgsJson) {
	if (SourceMode != ERadiantSource::Core) {
		LastErrorText = "radiant source is set to `none` (pre-adoption behaviour)";
		return nullptr;
	}
	if (Caller == nullptr) {
		LastErrorText = "no core caller attached — this build has no libinsimulcore";
		return nullptr;
	}
	if (!Caller->IsAvailable()) {
		LastErrorText = Caller->LastError();
		if (LastErrorText.empty()) {
			LastErrorText = "core bridge unavailable";
		}
		return nullptr;
	}

	std::string Response;
	if (!Caller->Call(Method, ArgsJson, Response)) {
		LastErrorText = Caller->LastError();
		if (LastErrorText.empty()) {
			LastErrorText = Method + " failed";
		}
		return nullptr;
	}

	const FJsonParseResult Parsed = ParseJson(Response);
	if (!Parsed.bOk || !Parsed.Root || !Parsed.Root->IsObject()) {
		LastErrorText = "core returned a non-object result for " + Method;
		return nullptr;
	}
	return Parsed.Root;
}

bool FRadiantSource::Generate(const std::vector<std::string>& Kb,
	const std::vector<std::string>& Templates,
	const FRadiantOptions& Options,
	std::vector<FGeneratedRadiantQuest>& OutQuests) {
	LastErrorText.clear();
	OutQuests.clear();

	if (SourceMode == ERadiantSource::None) {
		// Pre-adoption behaviour: this engine generated no radiant quests at
		// all. Success with nothing produced, not an error.
		return true;
	}

	const std::string Args = BuildGenerateArgs(Kb, Templates, Options);
	if (Caller == nullptr || !Caller->IsAvailable()) {
		// Degrade to the pre-adoption path rather than failing: a platform with
		// no libinsimulcore build must still run the game (§4.7.2). LastError()
		// records why nothing was generated.
		LastErrorText = (Caller == nullptr)
			? std::string("no core caller attached — this build has no libinsimulcore")
			: Caller->LastError();
		if (LastErrorText.empty()) {
			LastErrorText = "core bridge unavailable";
		}
		return true;
	}

	std::string Response;
	if (!Caller->Call("radiant.generate", Args, Response)) {
		LastErrorText = Caller->LastError();
		if (LastErrorText.empty()) {
			LastErrorText = "radiant.generate failed";
		}
		return false;
	}

	std::string Error;
	if (!DecodeQuests(Response, OutQuests, Error)) {
		OutQuests.clear();
		LastErrorText = Error;
		return false;
	}
	return true;
}

std::string FRadiantSource::BaseTemplates() {
	LastErrorText.clear();
	const FJsonValuePtr Root = CallObject("radiant.baseTemplates", "{}");
	if (!Root) {
		return std::string();
	}
	return Root->GetString("templates");
}

std::vector<std::string> FRadiantSource::BaseTemplateIds() {
	LastErrorText.clear();
	const FJsonValuePtr Root = CallObject("radiant.baseTemplates", "{}");
	if (!Root) {
		return std::vector<std::string>();
	}
	return ReadStringArray(Root->Find("templateIds"));
}

std::vector<std::string> FRadiantSource::CoreMethods() {
	LastErrorText.clear();
	const FJsonValuePtr Root = CallObject("core.methods", "{}");
	if (!Root) {
		return std::vector<std::string>();
	}
	return ReadStringArray(Root->Find("methods"));
}

std::string FRadiantSource::CoreVersion() {
	LastErrorText.clear();
	if (Caller == nullptr) {
		LastErrorText = "no core caller attached — this build has no libinsimulcore";
		return std::string();
	}
	return Caller->Version();
}

} // namespace insimul
