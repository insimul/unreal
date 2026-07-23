// Copyright 2024 Insimul. All Rights Reserved.

#include "InsimulContentLibrary.h"

namespace insimul {

namespace {

/** Require a non-empty string member; on failure set OutError and return false. */
bool RequireId(const FJsonValue& Obj, const std::string& Where, std::string& OutError) {
	const FJsonValue* Node = Obj.Find("id");
	if (!Node || !Node->IsString() || Node->AsString().empty()) {
		OutError = Where + " is missing required \"id\"";
		return false;
	}
	return true;
}

} // namespace

const FContentItemDTO* FInsimulContentLibrary::FindItem(const std::string& Id) const {
	for (const FContentItemDTO& I : Data.Items) {
		if (I.Id == Id) {
			return &I;
		}
	}
	return nullptr;
}

const FQuestDTO* FInsimulContentLibrary::FindQuest(const std::string& Id) const {
	for (const FQuestDTO& Q : Data.Quests) {
		if (Q.Id == Id) {
			return &Q;
		}
	}
	return nullptr;
}

const FCharacterDTO* FInsimulContentLibrary::FindCharacter(const std::string& Id) const {
	for (const FCharacterDTO& C : Data.Characters) {
		if (C.Id == Id) {
			return &C;
		}
	}
	return nullptr;
}

const FSettlementDTO* FInsimulContentLibrary::FindTown(const std::string& Id) const {
	for (const FSettlementDTO& T : Data.Towns) {
		if (T.Id == Id) {
			return &T;
		}
	}
	return nullptr;
}

const FContentNarrativeDTO* FInsimulContentLibrary::FindNarrative(const std::string& Id) const {
	for (const FContentNarrativeDTO& N : Data.Narratives) {
		if (N.Id == Id) {
			return &N;
		}
	}
	return nullptr;
}

bool FInsimulContentLibrary::ImportFromJson(const std::string& Json, std::string& OutError) {
	bLoaded = false;
	Data = FContentLibraryDTO();

	FJsonParseResult Parsed = ParseJson(Json);
	if (!Parsed.bOk || !Parsed.Root || !Parsed.Root->IsObject()) {
		OutError = Parsed.bOk ? "content library root is not a JSON object" : Parsed.Error;
		return false;
	}

	const FJsonValue& Root = *Parsed.Root;

	// ── Schema-version gate ─────────────────────────────────────────────
	const FJsonValue* VersionNode = Root.Find("schemaVersion");
	if (!VersionNode || !VersionNode->IsNumber()) {
		OutError = "content library is missing required \"schemaVersion\"";
		return false;
	}
	Data.SchemaVersion = static_cast<int>(VersionNode->AsInt(0));
	if (Data.SchemaVersion < MinSupportedSchemaVersion ||
		Data.SchemaVersion > MaxSupportedSchemaVersion) {
		OutError = "content library schemaVersion " + std::to_string(Data.SchemaVersion) +
			" is unsupported (supported " + std::to_string(MinSupportedSchemaVersion) +
			".." + std::to_string(MaxSupportedSchemaVersion) + ")";
		return false;
	}

	// ── Required top-level identity ─────────────────────────────────────
	if (!RequireId(Root, "content library", OutError)) {
		return false;
	}
	const FJsonValue* NameNode = Root.Find("name");
	if (!NameNode || !NameNode->IsString() || NameNode->AsString().empty()) {
		OutError = "content library is missing required \"name\"";
		return false;
	}
	Data.Id = Root.GetString("id");
	Data.Name = Root.GetString("name");
	Data.Description = Root.GetString("description");
	Data.TargetLanguage = Root.GetString("targetLanguage");

	// ── items[] ─────────────────────────────────────────────────────────
	if (const FJsonValue* Items = Root.Find("items")) {
		for (std::size_t Idx = 0; Idx < Items->ArrayItems.size(); ++Idx) {
			const FJsonValuePtr& Item = Items->ArrayItems[Idx];
			if (!Item || !Item->IsObject()) {
				OutError = "content library items[" + std::to_string(Idx) + "] is not an object";
				return false;
			}
			if (!RequireId(*Item, "content library items[" + std::to_string(Idx) + "]", OutError)) {
				return false;
			}
			FContentItemDTO I;
			I.Id = Item->GetString("id");
			I.Name = Item->GetString("name");
			I.ItemType = Item->GetString("itemType");
			I.Description = Item->GetString("description");
			I.Value = Item->GetInt("value", 0);
			Data.Items.push_back(std::move(I));
		}
	}

	// ── quests[] ────────────────────────────────────────────────────────
	if (const FJsonValue* Quests = Root.Find("quests")) {
		for (std::size_t Idx = 0; Idx < Quests->ArrayItems.size(); ++Idx) {
			const FJsonValuePtr& Item = Quests->ArrayItems[Idx];
			if (!Item || !Item->IsObject()) {
				OutError = "content library quests[" + std::to_string(Idx) + "] is not an object";
				return false;
			}
			if (!RequireId(*Item, "content library quests[" + std::to_string(Idx) + "]", OutError)) {
				return false;
			}
			FQuestDTO Q;
			Q.Id = Item->GetString("id");
			Q.Name = Item->GetString("name");
			Q.Description = Item->GetString("description");
			Q.GiverNpcId = Item->GetString("giverNpcId");
			Q.Status = Item->GetString("status");
			Q.Content = Item->GetString("content");
			Q.QuestType = Item->GetString("questType");
			Q.Difficulty = Item->GetString("difficulty");
			Q.TargetLanguage = Item->GetString("targetLanguage");
			Data.Quests.push_back(std::move(Q));
		}
	}

	// ── characters[] ────────────────────────────────────────────────────
	if (const FJsonValue* Characters = Root.Find("characters")) {
		for (std::size_t Idx = 0; Idx < Characters->ArrayItems.size(); ++Idx) {
			const FJsonValuePtr& Item = Characters->ArrayItems[Idx];
			if (!Item || !Item->IsObject()) {
				OutError = "content library characters[" + std::to_string(Idx) + "] is not an object";
				return false;
			}
			if (!RequireId(*Item, "content library characters[" + std::to_string(Idx) + "]", OutError)) {
				return false;
			}
			FCharacterDTO C;
			C.Id = Item->GetString("id");
			C.FirstName = Item->GetString("firstName");
			C.LastName = Item->GetString("lastName");
			C.Gender = Item->GetString("gender");
			C.Occupation = Item->GetString("occupation");
			C.CurrentLocation = Item->GetString("currentLocation");
			C.bIsAlive = Item->GetBool("isAlive", true);
			Data.Characters.push_back(std::move(C));
		}
	}

	// ── towns[] (world-snapshot settlements) ────────────────────────────
	if (const FJsonValue* Towns = Root.Find("towns")) {
		for (std::size_t Idx = 0; Idx < Towns->ArrayItems.size(); ++Idx) {
			const FJsonValuePtr& Item = Towns->ArrayItems[Idx];
			if (!Item || !Item->IsObject()) {
				OutError = "content library towns[" + std::to_string(Idx) + "] is not an object";
				return false;
			}
			if (!RequireId(*Item, "content library towns[" + std::to_string(Idx) + "]", OutError)) {
				return false;
			}
			FSettlementDTO T;
			T.Id = Item->GetString("id");
			T.Name = Item->GetString("name");
			T.SettlementType = Item->GetString("settlementType");
			T.Population = Item->GetInt("population", 0);
			T.CountryId = Item->GetString("countryId");
			Data.Towns.push_back(std::move(T));
		}
	}

	// ── narratives[] ────────────────────────────────────────────────────
	if (const FJsonValue* Narratives = Root.Find("narratives")) {
		for (std::size_t Idx = 0; Idx < Narratives->ArrayItems.size(); ++Idx) {
			const FJsonValuePtr& Item = Narratives->ArrayItems[Idx];
			if (!Item || !Item->IsObject()) {
				OutError = "content library narratives[" + std::to_string(Idx) + "] is not an object";
				return false;
			}
			if (!RequireId(*Item, "content library narratives[" + std::to_string(Idx) + "]", OutError)) {
				return false;
			}
			FContentNarrativeDTO N;
			N.Id = Item->GetString("id");
			N.Title = Item->GetString("title");
			N.Body = Item->GetString("body");
			N.Language = Item->GetString("language");
			Data.Narratives.push_back(std::move(N));
		}
	}

	bLoaded = true;
	return true;
}

} // namespace insimul
