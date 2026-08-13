// Copyright 2024 Insimul. All Rights Reserved.

#include "InsimulUIPanelCatalog.h"

#include "InsimulJson.h"

#include <algorithm>
#include <utility>

namespace insimul {

namespace {

/** ", "-joined, or "none" — the one rendering the reports share. */
std::string JoinKeys(const std::vector<std::string>& Names) {
	if (Names.empty()) {
		return "none";
	}
	std::string Out;
	for (std::size_t i = 0; i < Names.size(); ++i) {
		if (i > 0) {
			Out += ", ";
		}
		Out += Names[i];
	}
	return Out;
}

} // namespace

// ── FInsimulUIPanelCatalog ───────────────────────────────────────────────────

bool FInsimulUIPanelCatalog::Parse(
	const std::string& Json, FInsimulUIPanelCatalog& OutCatalog, std::string& OutError) {
	OutCatalog = FInsimulUIPanelCatalog();
	OutError.clear();

	FJsonParseResult Parsed = ParseJson(Json);
	if (!Parsed.bOk || !Parsed.Root) {
		OutError = Parsed.Error.empty() ? std::string("not JSON") : Parsed.Error;
		return false;
	}
	const FJsonValue* Panels = Parsed.Root->Find("panels");
	if (!Panels || !Panels->IsArray()) {
		OutError = "no 'panels' array — this is not a panel catalog";
		return false;
	}

	for (std::size_t i = 0; i < Panels->Size(); ++i) {
		const FJsonValue* Row = Panels->ArrayItems[i].get();
		if (!Row || !Row->IsObject()) {
			OutError = "a 'panels' entry is not an object";
			return false;
		}
		FInsimulPanelEntry Entry;
		Entry.Key = Row->GetString("key");
		Entry.Widget = Row->GetString("widget");
		Entry.Module = Row->GetString("module");
		Entry.Notes = Row->GetString("notes");
		if (Entry.Key.empty()) {
			OutError = "a 'panels' entry has no key";
			return false;
		}
		if (OutCatalog.Find(Entry.Key) != nullptr) {
			OutError = "duplicate panel key '" + Entry.Key + "'";
			return false;
		}
		OutCatalog.Rows.push_back(std::move(Entry));
	}

	if (OutCatalog.Rows.empty()) {
		OutError = "the catalog names no panel at all";
		return false;
	}
	return true;
}

FInsimulUIPanelCatalog FInsimulUIPanelCatalog::FallbackCatalog() {
	FInsimulUIPanelCatalog Out;
	for (const auto& Pair : FInsimulUIRegistryModel::DefaultPanelMap()) {
		FInsimulPanelEntry Entry;
		Entry.Key = Pair.first;
		Entry.Widget = Pair.second;
		Out.Rows.push_back(std::move(Entry));
	}
	return Out;
}

const FInsimulPanelEntry* FInsimulUIPanelCatalog::Find(const std::string& Key) const {
	for (const FInsimulPanelEntry& Row : Rows) {
		if (Row.Key == Key) {
			return &Row;
		}
	}
	return nullptr;
}

std::vector<std::string> FInsimulUIPanelCatalog::Keys() const {
	std::vector<std::string> Out;
	Out.reserve(Rows.size());
	for (const FInsimulPanelEntry& Row : Rows) {
		Out.push_back(Row.Key);
	}
	return Out;
}

std::vector<std::string> FInsimulUIPanelCatalog::Modules() const {
	std::vector<std::string> Out;
	for (const FInsimulPanelEntry& Row : Rows) {
		if (Row.Module.empty()) {
			continue;
		}
		if (std::find(Out.begin(), Out.end(), Row.Module) == Out.end()) {
			Out.push_back(Row.Module);
		}
	}
	return Out;
}

std::vector<std::pair<std::string, std::string>> FInsimulUIPanelCatalog::DefaultRefs() const {
	std::vector<std::pair<std::string, std::string>> Out;
	Out.reserve(Rows.size());
	for (const FInsimulPanelEntry& Row : Rows) {
		Out.emplace_back(Row.Key, Row.Widget);
	}
	return Out;
}

// ── FInsimulUIPanelResolver ──────────────────────────────────────────────────

FInsimulUIPanelResolver::FInsimulUIPanelResolver(FInsimulUIPanelCatalog InCatalog)
	: PanelCatalog(std::move(InCatalog)), RegistryModel(PanelCatalog.DefaultRefs()) {}

void FInsimulUIPanelResolver::SetActiveModules(const FInsimulActiveModuleSet& Set) {
	// An undeclared genre activates every pack (InsimulModuleActivation.h), so it
	// withholds no panel either. The two answers are the same answer on purpose.
	if (Set.Source == EInsimulGenreSource::Undeclared) {
		SetUngated();
		ActiveGenre = Set.Genre;
		return;
	}
	ActiveModules.clear();
	for (const FInsimulActiveModule& Module : Set.Modules) {
		ActiveModules.push_back(Module.Id);
	}
	ActiveGenre = Set.Genre;
	bGated = true;
}

void FInsimulUIPanelResolver::SetActiveModuleIds(std::vector<std::string> Ids) {
	ActiveModules = std::move(Ids);
	bGated = true;
}

void FInsimulUIPanelResolver::SetUngated() {
	ActiveModules.clear();
	ActiveGenre.clear();
	bGated = false;
}

bool FInsimulUIPanelResolver::IsModuleActive(const std::string& ModuleId) const {
	return std::find(ActiveModules.begin(), ActiveModules.end(), ModuleId) != ActiveModules.end();
}

void FInsimulUIPanelResolver::Override(const std::string& Key, const std::string& Widget) {
	RegistryModel.Register(Key, Widget);
}

FInsimulPanelResolution FInsimulUIPanelResolver::Peek(const std::string& Key) const {
	FInsimulPanelResolution Out;
	Out.Key = Key;

	const FInsimulPanelEntry* Entry = PanelCatalog.Find(Key);
	if (Entry) {
		Out.Module = Entry->Module;
	}

	// Gating is decided BEFORE the widget layer: an override swaps the widget, it
	// does not enrol this world in a module it did not select.
	if (bGated && Entry && !Entry->Module.empty() && !IsModuleActive(Entry->Module)) {
		Out.Outcome = EInsimulPanelOutcome::Gated;
		Out.Detail = "panel '" + Key + "' belongs to a module this world does not activate ("
			+ Entry->Module + ")";
		return Out;
	}

	const std::string Ref = RegistryModel.PeekRef(Key);
	if (Ref.empty()) {
		Out.Outcome = EInsimulPanelOutcome::Unknown;
		Out.Detail = "no panel registered for key '" + Key + "'";
		return Out;
	}

	Out.Widget = Ref;
	Out.Outcome = RegistryModel.IsOverridden(Key) ? EInsimulPanelOutcome::Overridden
		: EInsimulPanelOutcome::Shipped;
	return Out;
}

FInsimulPanelResolution FInsimulUIPanelResolver::Resolve(const std::string& Key) {
	FInsimulPanelResolution Out = Peek(Key);
	if (Out.Outcome == EInsimulPanelOutcome::Gated) {
		GateDiagnostics.push_back({"inactive_module", Key, Out.Detail});
	} else if (Out.Outcome == EInsimulPanelOutcome::Unknown) {
		// Let the registry record its own missing-panel diagnostic, so a creator
		// reads ONE diagnostic list whichever layer refused.
		RegistryModel.SceneRef(Key);
	}
	return Out;
}

std::vector<std::string> FInsimulUIPanelResolver::AvailableKeys() const {
	std::vector<std::string> Out;
	for (const FInsimulPanelEntry& Row : PanelCatalog.Entries()) {
		if (Peek(Row.Key).IsAvailable()) {
			Out.push_back(Row.Key);
		}
	}
	return Out;
}

std::vector<std::string> FInsimulUIPanelResolver::GatedKeys() const {
	std::vector<std::string> Out;
	for (const FInsimulPanelEntry& Row : PanelCatalog.Entries()) {
		if (Peek(Row.Key).Outcome == EInsimulPanelOutcome::Gated) {
			Out.push_back(Row.Key);
		}
	}
	return Out;
}

std::string FInsimulUIPanelResolver::Describe() const {
	if (!bGated) {
		return "UI panels: UNGATED (no module set applied) — all "
			+ std::to_string(PanelCatalog.Entries().size()) + " panel(s) available";
	}
	const std::vector<std::string> Gated = GatedKeys();
	std::string Out = "UI panels: genre '" + (ActiveGenre.empty() ? std::string("(none)") : ActiveGenre)
		+ "' shows " + std::to_string(AvailableKeys().size()) + " of "
		+ std::to_string(PanelCatalog.Entries().size()) + " panel(s)";
	Out += "; withheld: " + JoinKeys(Gated);
	return Out;
}

std::vector<FUIRegistryDiagnostic> FInsimulUIPanelResolver::Diagnostics() const {
	std::vector<FUIRegistryDiagnostic> Out = GateDiagnostics;
	const std::vector<FUIRegistryDiagnostic>& FromRegistry = RegistryModel.Diagnostics();
	Out.insert(Out.end(), FromRegistry.begin(), FromRegistry.end());
	return Out;
}

bool FInsimulUIPanelResolver::HasDiagnostics() const {
	return !GateDiagnostics.empty() || RegistryModel.HasDiagnostics();
}

void FInsimulUIPanelResolver::ClearDiagnostics() {
	GateDiagnostics.clear();
	RegistryModel.ClearDiagnostics();
}

} // namespace insimul
