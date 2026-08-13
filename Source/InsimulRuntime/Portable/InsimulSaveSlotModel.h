// Copyright 2024 Insimul. All Rights Reserved.
//
// FInsimulSaveSlotModel — the host-testable, portable save/load slot view-model
// for the default-UI save/load screen (US-XU4, InsimulSaveGame / the save-load
// tab). The Unreal mirror of the engine-neutral slot contract
// (packages/core/src/ui/save-slot-model.ts; the Godot leg is
// addons/insimul/ui/save_slot_model.gd).
//
// A slot is loaded by the codec (the portable save system — canonical JSON +
// SHA-256 integrity, see InsimulSaveSystem) into an OUTCOME: empty / ok (+ a
// summary) / one of the envelope validation failures (invalid_format /
// missing_save_file / integrity_mismatch). This model renders each into a row
// (status / title / message / can_load / can_save). The corrupted-envelope
// MESSAGING is the cross-engine contract — an integrity mismatch on a tampered
// save surfaces the SAME message on every default-UI leg.
//
// std-only (no Unreal Engine, no CoreMinimal.h) so the whole contract runs under
// ctest `ui_save_slots` (190 US-3). Shared cases:
// packages/core/conformance/ui/save-slot-cases.json.

#pragma once

#include <string>
#include <vector>

namespace insimul {

/** The summary a healthy save exposes for its slot title/message (all optional). */
struct FSaveSummary {
	std::string PlayerName;
	bool bHasLevel = false;
	long long Level = 0;
	std::string LocationName;
	std::string SavedAt;
};

/** A codec-reported slot outcome. Outcome is one of:
 *  empty | ok | invalid_format | missing_save_file | integrity_mismatch. */
struct FSlotResult {
	int Index = 0;
	std::string Outcome = "empty";
	bool bHasSummary = false;
	FSaveSummary Summary;
};

/** The rendered slot row. */
struct FSlotView {
	int Index = 0;
	std::string Status;   // "empty" | "ok" | "corrupted"
	std::string Title;
	std::string Message;
	bool bCanLoad = false;
	bool bCanSave = false;
};

class FInsimulSaveSlotModel {
public:
	FInsimulSaveSlotModel() = default;
	explicit FInsimulSaveSlotModel(const std::vector<FSlotResult>& Results) { SetSlots(Results); }

	void SetSlots(const std::vector<FSlotResult>& Results);

	/** The rendered rows, in slot-index order. */
	std::vector<FSlotView> Slots() const;

	/** The rendered row for one slot index (bCanSave false / empty title if absent). */
	FSlotView Slot(int Index) const;

	/** True when any slot is loadable (main-menu Continue gate). */
	bool HasAnyLoadable() const;

	/**
	 * The slot the main menu's Continue resumes: the most recently saved LOADABLE
	 * one, or -1 when there is none. A corrupted envelope is never it, however
	 * recent it looks — loadability is this model's answer and not the caller's,
	 * which is the whole reason Continue lives here rather than in a menu widget
	 * keeping its own "has save" boolean.
	 *
	 * "Most recent" is the codec's `savedAt` compared as the ISO-8601 STRINGS it
	 * emits — never a local clock, which would make two engines disagree about the
	 * same save directory. An unstamped save loses to a stamped one, and a tie
	 * falls to the lower slot index, so the answer is total and deterministic.
	 *
	 * The shared corpus pins the ROWS, not this ordering (core's own main menu is
	 * playthrough-based), so `ui_save_slots` derives its expectations from the
	 * corpus instead: the answer is always a `can_load` row, it is never a
	 * corrupted one, and it is the `savedAt`-maximal loadable row.
	 */
	int ContinueSlot() const;

	/**
	 * Why Continue is unavailable ("" when it is available). A corrupted slot
	 * explains itself in MessageForOutcome's cross-engine words; a player with no
	 * saves at all is simply told there are none. Disabled AND unexplained is the
	 * state the main menu refuses to be in.
	 */
	std::string ContinueBlockedReason() const;

	/** Human, cross-engine message for a non-ok outcome (empty for empty/ok). */
	static std::string MessageForOutcome(const std::string& Outcome);

private:
	std::vector<FSlotResult> Results;

	static FSlotView View(const FSlotResult& Result);
	static std::string SummaryTitle(int Index, const FSlotResult& Result);
};

} // namespace insimul
