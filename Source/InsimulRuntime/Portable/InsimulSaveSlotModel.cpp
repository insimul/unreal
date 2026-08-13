// Copyright 2024 Insimul. All Rights Reserved.
//
// FInsimulSaveSlotModel implementation — see InsimulSaveSlotModel.h. std-only;
// host-tested by ctest `ui_save_slots` against save-slot-cases.json.

#include "InsimulSaveSlotModel.h"

#include <algorithm>

namespace insimul {

std::string FInsimulSaveSlotModel::MessageForOutcome(const std::string& Outcome) {
	// Keep in lockstep with the TS SLOT_MESSAGES map / the Godot MESSAGES const.
	if (Outcome == "invalid_format") {
		return "Unrecognized save format — this slot cannot be loaded.";
	}
	if (Outcome == "missing_save_file") {
		return "Save data is missing or unreadable.";
	}
	if (Outcome == "integrity_mismatch") {
		return "Save file integrity check failed — file may be corrupted or tampered.";
	}
	return std::string();
}

void FInsimulSaveSlotModel::SetSlots(const std::vector<FSlotResult>& InResults) {
	Results = InResults;
}

std::string FInsimulSaveSlotModel::SummaryTitle(int Index, const FSlotResult& Result) {
	const std::string Fallback = "Slot " + std::to_string(Index + 1);
	if (!Result.bHasSummary) {
		return Fallback;
	}
	const FSaveSummary& S = Result.Summary;
	std::vector<std::string> Bits;
	if (!S.PlayerName.empty()) {
		Bits.push_back(S.PlayerName);
	}
	if (S.bHasLevel) {
		Bits.push_back("Lv " + std::to_string(S.Level));
	}
	if (!S.LocationName.empty()) {
		Bits.push_back(S.LocationName);
	}
	if (Bits.empty()) {
		return Fallback;
	}
	std::string Out = Bits.front();
	for (std::size_t I = 1; I < Bits.size(); ++I) {
		Out += " \xC2\xB7 " + Bits[I]; // " · " (U+00B7 middle dot)
	}
	return Out;
}

FSlotView FInsimulSaveSlotModel::View(const FSlotResult& Result) {
	FSlotView V;
	V.Index = Result.Index;
	if (Result.Outcome == "empty") {
		V.Status = "empty";
		V.Title = "Empty Slot";
		V.Message = std::string();
		V.bCanLoad = false;
		V.bCanSave = true;
		return V;
	}
	if (Result.Outcome == "ok") {
		V.Status = "ok";
		V.Title = SummaryTitle(Result.Index, Result);
		V.Message = (Result.bHasSummary && !Result.Summary.SavedAt.empty())
				? ("Saved " + Result.Summary.SavedAt)
				: std::string();
		V.bCanLoad = true;
		V.bCanSave = true;
		return V;
	}
	// Any validation failure -> corrupted. Cannot load, but can overwrite.
	V.Status = "corrupted";
	V.Title = "Corrupted Save";
	V.Message = MessageForOutcome(Result.Outcome);
	V.bCanLoad = false;
	V.bCanSave = true;
	return V;
}

std::vector<FSlotView> FInsimulSaveSlotModel::Slots() const {
	std::vector<FSlotResult> Sorted = Results;
	std::stable_sort(Sorted.begin(), Sorted.end(),
			[](const FSlotResult& A, const FSlotResult& B) { return A.Index < B.Index; });
	std::vector<FSlotView> Out;
	Out.reserve(Sorted.size());
	for (const FSlotResult& R : Sorted) {
		Out.push_back(View(R));
	}
	return Out;
}

FSlotView FInsimulSaveSlotModel::Slot(int Index) const {
	for (const FSlotResult& R : Results) {
		if (R.Index == Index) {
			return View(R);
		}
	}
	return FSlotView();
}

int FInsimulSaveSlotModel::ContinueSlot() const {
	int Best = -1;
	std::string BestSavedAt;
	for (const FSlotResult& R : Results) {
		// Loadability is View()'s answer, so the corrupted-envelope rule is stated
		// once: a tampered save cannot be the slot Continue resumes.
		if (!View(R).bCanLoad) {
			continue;
		}
		const std::string SavedAt = R.bHasSummary ? R.Summary.SavedAt : std::string();
		// A later stamp wins; an equal stamp falls to the lower slot index. Stated
		// as a comparison rather than left to the order the caller happened to hand
		// its slots in, so two engines reading one directory answer alike.
		if (Best < 0 || SavedAt > BestSavedAt || (SavedAt == BestSavedAt && R.Index < Best)) {
			Best = R.Index;
			BestSavedAt = SavedAt;
		}
	}
	return Best;
}

std::string FInsimulSaveSlotModel::ContinueBlockedReason() const {
	if (ContinueSlot() >= 0) {
		return std::string();
	}
	// Slots() is index-ordered, so the slot that speaks for a directory holding
	// several broken saves is the first one, not the first one the caller listed.
	for (const FSlotView& Row : Slots()) {
		if (Row.Status == "corrupted" && !Row.Message.empty()) {
			return Row.Message;
		}
	}
	return "No saved game to continue.";
}

bool FInsimulSaveSlotModel::HasAnyLoadable() const {
	for (const FSlotResult& R : Results) {
		if (R.Outcome == "ok") {
			return true;
		}
	}
	return false;
}

} // namespace insimul
