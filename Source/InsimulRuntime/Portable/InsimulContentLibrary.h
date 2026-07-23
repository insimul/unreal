// Copyright 2024 Insimul. All Rights Reserved.
//
// FInsimulContentLibrary — the host-testable content-library importer (US-IM1).
//
// A content library is a portable, world-independent bundle of authored content
// (items / quests / characters / towns / narratives) produced by the content
// library API (the v1 client's importWorld sibling) or read from a file. This
// loader parses such a library, validates it against the vendored core schema
// (the schemaVersion gate + required-field checks), and materializes the native
// DTOs the UStruct boundary layer maps into Blueprint/UMG entities.
//
// This is the Unreal leg of the author-once/use-anywhere content-portability
// track: every native engine imports the SAME shared conformance fixture
// (conformance/content/library-basic.json) into the same entity set, and the
// invalid-cases corpus pins that malformed libraries are rejected identically.
//
// Portable (std only) — no Unreal Engine. Reuses the world-snapshot DTOs
// (FCharacterDTO / FSettlementDTO / FQuestDTO) so imported content shares the
// exact same shapes the world source already feeds the runtime, plus two
// content-library-only shapes (items, narratives) defined below.

#pragma once

#include "../Generated/InsimulWorldDTO.h"
#include "InsimulJson.h"

#include <string>
#include <vector>

namespace insimul {

/** ContentLibrary.items[] — an authored inventory/world item template. */
struct FContentItemDTO {
	std::string Id;
	std::string Name;
	std::string ItemType;
	std::string Description;
	long long Value = 0;
};

/** ContentLibrary.narratives[] — an authored narrative/story beat. */
struct FContentNarrativeDTO {
	std::string Id;
	std::string Title;
	std::string Body;
	std::string Language;
};

/**
 * A parsed content library. Quests/characters/towns reuse the world-snapshot
 * DTOs so imported content is indistinguishable from world-sourced content at
 * the boundary; items and narratives are content-library-only.
 */
struct FContentLibraryDTO {
	int SchemaVersion = 0;
	std::string Id;
	std::string Name;
	std::string Description;
	std::string TargetLanguage;

	std::vector<FContentItemDTO> Items;
	std::vector<FQuestDTO> Quests;
	std::vector<FCharacterDTO> Characters;
	std::vector<FSettlementDTO> Towns;
	std::vector<FContentNarrativeDTO> Narratives;
};

class FInsimulContentLibrary {
public:
	/**
	 * Oldest content-library schema version this importer understands. Older
	 * libraries must be migrated up first.
	 */
	static constexpr int MinSupportedSchemaVersion = 1;

	/**
	 * Newest content-library schema version this importer understands. A library
	 * stamped beyond this was authored by a newer build and is rejected. MUST
	 * track the CONTENT_LIBRARY_VERSION in the core content-library schema.
	 */
	static constexpr int MaxSupportedSchemaVersion = 1;

	/**
	 * Import a content library from its JSON document. On success returns true
	 * and populates the DTOs. On any schema-validation failure returns false
	 * with a clear, human-facing OutError (unsupported version, missing required
	 * field, malformed entity) and leaves the importer not loaded.
	 */
	bool ImportFromJson(const std::string& Json, std::string& OutError);

	bool IsLoaded() const { return bLoaded; }

	const FContentLibraryDTO& Library() const { return Data; }
	int SchemaVersion() const { return Data.SchemaVersion; }

	// ── Materialized entity counts ──────────────────────────────────────
	std::size_t ItemCount() const { return Data.Items.size(); }
	std::size_t QuestCount() const { return Data.Quests.size(); }
	std::size_t CharacterCount() const { return Data.Characters.size(); }
	std::size_t TownCount() const { return Data.Towns.size(); }
	std::size_t NarrativeCount() const { return Data.Narratives.size(); }

	/** Total native entities materialized from the library. */
	std::size_t TotalEntityCount() const {
		return ItemCount() + QuestCount() + CharacterCount() + TownCount() + NarrativeCount();
	}

	const FContentItemDTO* FindItem(const std::string& Id) const;
	const FQuestDTO* FindQuest(const std::string& Id) const;
	const FCharacterDTO* FindCharacter(const std::string& Id) const;
	const FSettlementDTO* FindTown(const std::string& Id) const;
	const FContentNarrativeDTO* FindNarrative(const std::string& Id) const;

	/**
	 * Canonical semantic projection of the MATERIALIZED entities (US-IM2). This
	 * re-serializes the imported DTOs — not the raw input document — into the
	 * shared canonical JSON form (key-sorted, minified, JS-faithful numbers and
	 * string escaping), so it is byte-comparable across the TS/Unity/Unreal
	 * runtimes. Two libraries with the same author-once semantics project to the
	 * same bytes regardless of source key order or whitespace; this is the
	 * per-engine leg of the author-once/use-anywhere round-trip proof. Empty
	 * when the importer is not loaded.
	 */
	std::string CanonicalProjection() const;

	/** SHA-256 hex of CanonicalProjection() — the corpus round-trip vector. */
	std::string ProjectionIntegrity() const;

private:
	bool bLoaded = false;
	FContentLibraryDTO Data;
};

} // namespace insimul
