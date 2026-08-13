// Copyright 2024 Insimul. All Rights Reserved.
//
// test_dialogue_ui.cpp — host gate for the default-UI dialogue / pause-menu /
// save-load view-models (tasklist 190 US-3; the cores landed under US-XU4). Builds
// under a plain clang toolchain (no Unreal Engine, no UBT) and proves the Unreal
// cores against the SAME engine-neutral corpora every other default-UI mirror
// runs, so the four legs (Babylon, Unity, Godot, Unreal) can never diverge:
//
//   - chat-cases.json      -> FInsimulChatModel: the streaming turn lifecycle
//     (greeting/begin/chunk/action/complete/fail), the transcript, the streaming
//     flag, triggered actions, completed-turn count, LastNpcText (TTS/lip-sync
//     source), and the save.conversations history projection.
//   - pause-menu-cases.json -> FInsimulPauseMenuModel: module-bundle tab-gating
//     (AND-gating) + the open/active-tab reducer.
//   - save-slot-cases.json  -> FInsimulSaveSlotModel: the codec-outcome -> row
//     rendering incl. the corrupted-envelope MESSAGING cross-engine contract.
//   - conformance/saves/v2-typical.json -> FInsimulUIStateBinding: the history a
//     panel projects, flushed into the REAL save envelope's `conversations` and
//     read back, with everything else byte-identical.
//
// Four ctest legs run this one binary, selected by `--only <area>`:
//   ui_chat, ui_pause_menu, ui_save_slots, ui_chat_history.
// Running it with no --only runs all four. This file NAMES ITS RUNNER CORRECTLY:
// the shell script the original header pointed at belongs to the parent platform
// checkout and does not exist here, so for two bands nothing compiled these cases
// (CLAUDE.md § the verification story is the deliverable).
//
// NEGATIVE CONTROLS. Every corpus comparison below is a function a control can
// call with a MUTATED model, and each control asserts the comparison goes red. A
// check that cannot fail is a decoration.
//
// The UE seams (UInsimulChatPanel / UInsimulPauseMenu / UInsimulSaveSlotPanel, and
// the UMG panels UInsimulMainMenuPanel / UInsimulPauseMenuPanel /
// UInsimulSaveLoadPanel) sit ON TOP of these pure cores, syntax-gated only.
//
// Corpus dirs come from the compile definitions with a `--ui` / `--saves`
// override; a bare argv[1] is still read as the UI dir.

#include "../Portable/InsimulChatModel.h"
#include "../Portable/InsimulJson.h"
#include "../Portable/InsimulPauseMenuModel.h"
#include "../Portable/InsimulSaveSlotModel.h"
#include "../Portable/InsimulSaveSystem.h"
#include "../Portable/InsimulUIStateBinding.h"

#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace insimul;

namespace {

int g_pass = 0;
int g_fail = 0;

void Report(const std::string& Name, bool bOk, const std::string& Detail = "") {
	std::printf("  %s  %-58s%s%s\n", bOk ? "PASS" : "FAIL", Name.c_str(),
			Detail.empty() ? "" : "  ", Detail.c_str());
	if (bOk) {
		g_pass++;
	} else {
		g_fail++;
	}
}

std::string ReadFile(const std::string& Path) {
	std::ifstream In(Path, std::ios::binary);
	if (!In) {
		return std::string();
	}
	std::ostringstream Ss;
	Ss << In.rdbuf();
	return Ss.str();
}

FJsonValuePtr LoadJson(const std::string& Dir, const std::string& File) {
	const std::string Text = ReadFile(Dir + "/" + File);
	if (Text.empty()) {
		std::printf("  FAIL  could not read corpus file: %s/%s\n", Dir.c_str(), File.c_str());
		g_fail++;
		return nullptr;
	}
	FJsonParseResult Parsed = ParseJson(Text);
	if (!Parsed.bOk) {
		std::printf("  FAIL  parse error in %s: %s\n", File.c_str(), Parsed.Error.c_str());
		g_fail++;
		return nullptr;
	}
	return Parsed.Root;
}

std::vector<std::string> StringArray(const FJsonValue* Arr) {
	std::vector<std::string> Out;
	if (Arr && Arr->IsArray()) {
		for (const auto& V : Arr->ArrayItems) {
			Out.push_back(V ? V->AsString() : std::string());
		}
	}
	return Out;
}

std::string Join(const std::vector<std::string>& V) {
	std::string Out = "[";
	for (std::size_t I = 0; I < V.size(); ++I) {
		if (I) {
			Out += ",";
		}
		Out += V[I];
	}
	return Out + "]";
}

/** A real deep copy. FJsonValue holds its children by shared_ptr, so `Copy = *Node`
 *  shares them: a control that mutated one would be mutating the fixture. */
FJsonValuePtr DeepCopy(const FJsonValue& In) {
	auto Out = std::make_shared<FJsonValue>();
	Out->Type = In.Type;
	Out->BoolValue = In.BoolValue;
	Out->RawNumber = In.RawNumber;
	Out->NumberValue = In.NumberValue;
	Out->StringValue = In.StringValue;
	for (const FJsonValuePtr& Item : In.ArrayItems) {
		Out->ArrayItems.push_back(Item ? DeepCopy(*Item) : nullptr);
	}
	for (const auto& Pair : In.ObjectItems) {
		Out->ObjectItems.emplace_back(Pair.first, Pair.second ? DeepCopy(*Pair.second) : nullptr);
	}
	return Out;
}

const FJsonValue* CasesOf(const FJsonValuePtr& Root, const std::string& File) {
	if (!Root) {
		return nullptr;
	}
	const FJsonValue* Cases = Root->Find("cases");
	if (!Cases || !Cases->IsArray()) {
		Report(File + " has a cases array", false);
		return nullptr;
	}
	return Cases;
}

// ── Chat corpus ───────────────────────────────────────────────────────────────

/** Replay a case's ordered event stream into a model. bCheckOk asserts each
 *  event's `expected_ok`; SkipOp drops the FIRST event of that op, which is how a
 *  control mutates the stream without touching the parsed corpus. Returns the
 *  number of events dropped in OutSkipped. */
bool ReplayChat(const FJsonValue& Case, FInsimulChatModel& Model, bool bCheckOk,
		std::string& Detail, const std::string& SkipOp = std::string(), int* OutSkipped = nullptr) {
	const FJsonValue* Events = Case.Find("events");
	if (!Events || !Events->IsArray()) {
		return true;
	}
	bool bSkipped = false;
	for (const auto& Ev : Events->ArrayItems) {
		const std::string Op = Ev->GetString("op");
		if (!SkipOp.empty() && !bSkipped && Op == SkipOp) {
			bSkipped = true;
			if (OutSkipped) {
				(*OutSkipped)++;
			}
			continue;
		}
		bool bResultKnown = false;
		bool bResult = false;
		if (Op == "greeting") {
			Model.Greeting(Ev->GetString("text"));
		} else if (Op == "begin") {
			bResult = Model.BeginUserTurn(Ev->GetString("text"));
			bResultKnown = true;
		} else if (Op == "chunk") {
			Model.AppendChunk(Ev->GetString("text"));
		} else if (Op == "action") {
			FChatAction A;
			A.Name = Ev->GetString("name");
			A.Args = StringArray(Ev->Find("args"));
			A.FactToAssert = Ev->GetString("fact");
			Model.TriggerAction(A);
		} else if (Op == "complete") {
			if (Ev->Find("full_text")) {
				bResult = Model.CompleteTurn(Ev->GetString("full_text"));
			} else {
				bResult = Model.CompleteTurn();
			}
			bResultKnown = true;
		} else if (Op == "fail") {
			bResult = Model.FailTurn(Ev->GetString("error"));
			bResultKnown = true;
		}
		if (bCheckOk && bResultKnown && Ev->Find("expected_ok")) {
			const bool WantOk = Ev->GetBool("expected_ok");
			if (bResult != WantOk) {
				Detail = Op + " ok=" + (bResult ? "true" : "false") + " want " +
						(WantOk ? "true" : "false");
				return false;
			}
		}
	}
	return true;
}

/** Diff a settled model against a case's expectations. THE comparison — the
 *  corpus run and every negative control call this same function. */
bool CompareChat(const FJsonValue& Case, const FInsimulChatModel& Model, std::string& Detail) {
	// Transcript (role / text / error flag).
	{
		const FJsonValue* WantMsgs = Case.Find("expected_messages");
		const std::vector<FChatMessage>& Got = Model.MessageList();
		const std::size_t WantN = WantMsgs && WantMsgs->IsArray() ? WantMsgs->Size() : 0;
		if (Got.size() != WantN) {
			Detail = "messages n=" + std::to_string(Got.size()) + " want " + std::to_string(WantN);
			return false;
		}
		for (std::size_t m = 0; m < WantN; ++m) {
			const FJsonValue* W = WantMsgs->ArrayItems[m].get();
			const bool WantErr = W->GetBool("error");
			if (Got[m].Role != W->GetString("role") || Got[m].Text != W->GetString("text") ||
					Got[m].bError != WantErr) {
				Detail = "message[" + std::to_string(m) + "] '" + Got[m].Role + ":" + Got[m].Text +
						"' want '" + W->GetString("role") + ":" + W->GetString("text") + "'";
				return false;
			}
		}
	}

	// Streaming flag.
	if (Case.Find("expected_streaming")) {
		const bool WantStream = Case.GetBool("expected_streaming");
		if (Model.IsStreaming() != WantStream) {
			Detail = "streaming mismatch";
			return false;
		}
	}

	// Triggered actions (name / args / factToAssert) — what the panel asserts to the KB.
	{
		const FJsonValue* WantActs = Case.Find("expected_actions");
		const std::vector<FChatAction>& Got = Model.ActionList();
		const std::size_t WantN = WantActs && WantActs->IsArray() ? WantActs->Size() : 0;
		if (Got.size() != WantN) {
			Detail = "actions n=" + std::to_string(Got.size()) + " want " + std::to_string(WantN);
			return false;
		}
		for (std::size_t a = 0; a < WantN; ++a) {
			const FJsonValue* W = WantActs->ArrayItems[a].get();
			const std::vector<std::string> WantArgs = StringArray(W->Find("args"));
			if (Got[a].Name != W->GetString("name") || Got[a].Args != WantArgs ||
					Got[a].FactToAssert != W->GetString("factToAssert")) {
				Detail = "action[" + std::to_string(a) + "] '" + Got[a].Name + Join(Got[a].Args) +
						"->" + Got[a].FactToAssert + "'";
				return false;
			}
		}
	}

	// Completed-turn count.
	if (Case.Find("expected_turn_count")) {
		const long long WantCount = Case.GetInt("expected_turn_count");
		if (Model.CompletedTurnCount() != WantCount) {
			Detail = "turn_count " + std::to_string(Model.CompletedTurnCount()) + " want " +
					std::to_string(WantCount);
			return false;
		}
	}

	// Last settled NPC line (TTS / lip-sync source).
	if (Case.Find("expected_last_npc_text")) {
		const std::string WantLast = Case.GetString("expected_last_npc_text");
		if (Model.LastNpcText() != WantLast) {
			Detail = "last_npc_text '" + Model.LastNpcText() + "' want '" + WantLast + "'";
			return false;
		}
	}

	// save.conversations history projection (role / content).
	{
		const FJsonValue* WantHist = Case.Find("expected_history_turns");
		const FChatHistory Hist = Model.History();
		const std::size_t WantN = WantHist && WantHist->IsArray() ? WantHist->Size() : 0;
		if (Hist.RecentTurns.size() != WantN) {
			Detail = "history n=" + std::to_string(Hist.RecentTurns.size()) + " want " +
					std::to_string(WantN);
			return false;
		}
		for (std::size_t h = 0; h < WantN; ++h) {
			const FJsonValue* W = WantHist->ArrayItems[h].get();
			if (Hist.RecentTurns[h].Role != W->GetString("role") ||
					Hist.RecentTurns[h].Content != W->GetString("content")) {
				Detail = "history[" + std::to_string(h) + "] '" + Hist.RecentTurns[h].Role + ":" +
						Hist.RecentTurns[h].Content + "'";
				return false;
			}
		}
		// totalTurnCount tracks the completed-turn count.
		if (Hist.TotalTurnCount != Model.CompletedTurnCount()) {
			Detail = "history.totalTurnCount mismatch";
			return false;
		}
	}
	return true;
}

FInsimulChatModel ModelFor(const FJsonValue& Case) {
	const FJsonValue* Character = Case.Find("character");
	return FInsimulChatModel(Character ? Character->GetString("id") : std::string(),
			Character ? Character->GetString("name") : std::string());
}

void RunChatCases(const std::string& Dir) {
	std::printf("\n-- the streaming turn lifecycle (chat-cases.json) --\n");
	FJsonValuePtr Root = LoadJson(Dir, "chat-cases.json");
	const FJsonValue* Cases = CasesOf(Root, "chat-cases.json");
	if (!Cases) {
		return;
	}

	bool bSawAction = false;
	bool bSawError = false;
	for (std::size_t i = 0; i < Cases->Size(); ++i) {
		const FJsonValue& C = *Cases->ArrayItems[i];
		FInsimulChatModel Model = ModelFor(C);
		std::string Detail;
		bool bOk = ReplayChat(C, Model, true, Detail) && CompareChat(C, Model, Detail);
		Report("chat: " + C.GetString("name"), bOk, Detail);

		bSawAction = bSawAction || !Model.ActionList().empty();
		for (const FChatMessage& M : Model.MessageList()) {
			bSawError = bSawError || M.bError;
		}
	}
	Report("chat: the corpus exercises a KB action trigger and a stream failure",
			bSawAction && bSawError);

	// ── Negative controls ─────────────────────────────────────────────────────
	// A greeting appends an NPC bubble the corpus did not expect: every case's
	// comparison must go red. If one does not, that case is comparing nothing.
	int Fired = 0;
	for (std::size_t i = 0; i < Cases->Size(); ++i) {
		const FJsonValue& C = *Cases->ArrayItems[i];
		FInsimulChatModel Model = ModelFor(C);
		std::string Detail;
		ReplayChat(C, Model, false, Detail);
		Model.Greeting("an extra line the corpus never authored");
		if (!CompareChat(C, Model, Detail)) {
			Fired++;
		}
	}
	Report("control: an unexpected NPC line reddens every case",
			Fired == static_cast<int>(Cases->Size()),
			std::to_string(Fired) + "/" + std::to_string(Cases->Size()));

	// The chunk stream is what the bubble renders: drop one and the case it came
	// from no longer matches — UNLESS the case ends by overriding the bubble
	// (`full_text`) or by discarding it (`fail`), where the accumulated chunks are
	// deliberately unobservable. Those two are counted and named rather than
	// quietly folded in, because a control that silently excused a case would be
	// the decoration this block exists to prevent.
	{
		int Observable = 0;
		int Red = 0;
		int Excused = 0;
		for (std::size_t i = 0; i < Cases->Size(); ++i) {
			const FJsonValue& C = *Cases->ArrayItems[i];
			bool bHasChunk = false;
			bool bOverridden = false;
			if (const FJsonValue* Events = C.Find("events")) {
				for (const auto& Ev : Events->ArrayItems) {
					bHasChunk = bHasChunk || Ev->GetString("op") == "chunk";
					bOverridden = bOverridden || Ev->Find("full_text") != nullptr ||
							Ev->GetString("op") == "fail";
				}
			}
			if (!bHasChunk) {
				continue;
			}
			if (bOverridden) {
				Excused++;
				continue;
			}
			Observable++;
			FInsimulChatModel Model = ModelFor(C);
			std::string Detail;
			ReplayChat(C, Model, false, Detail, "chunk");
			if (!CompareChat(C, Model, Detail)) {
				Red++;
			}
		}
		Report("control: dropping a streamed chunk reddens every case that renders one",
				Observable > 0 && Red == Observable,
				std::to_string(Red) + "/" + std::to_string(Observable) + " (" +
						std::to_string(Excused) + " excused: full_text / fail overrides the bubble)");
	}

	// ── Edge behaviours the corpus does not carry ─────────────────────────────
	{
		FInsimulChatModel M("npc", "Aldric");
		M.BeginUserTurn("Hi");
		M.AppendChunk("Hel");
		M.AppendChunk("lo");
		Report("chat: StreamingText tracks the in-flight bubble",
				M.IsStreaming() && M.StreamingText() == "Hello");
	}
	{
		FInsimulChatModel M("npc");
		const bool bRejected = !M.BeginUserTurn("   ");
		Report("chat: a blank player line is rejected", bRejected && M.MessageList().empty());
	}
}

// ── Pause-menu corpus ────────────────────────────────────────────────────────

std::vector<FPauseTab> TabsFromJson(const FJsonValue* Arr) {
	std::vector<FPauseTab> Out;
	if (Arr && Arr->IsArray()) {
		for (const auto& T : Arr->ArrayItems) {
			FPauseTab Tab;
			Tab.Key = T->GetString("key");
			Tab.Label = T->GetString("label");
			Tab.Requires = StringArray(T->Find("requires"));
			Out.push_back(Tab);
		}
	}
	return Out;
}

/** Diff a configured menu against a case: the visible set, then the reducer steps. */
bool ComparePause(const FJsonValue& Case, FInsimulPauseMenuModel& Model, std::string& Detail) {
	const std::vector<std::string> WantKeys = StringArray(Case.Find("expected_visible_keys"));
	const std::vector<std::string> GotKeys = Model.VisibleKeys();
	if (GotKeys != WantKeys) {
		Detail = "visible " + Join(GotKeys) + " want " + Join(WantKeys);
		return false;
	}

	const FJsonValue* Steps = Case.Find("steps");
	if (!Steps || !Steps->IsArray()) {
		return true;
	}
	for (const auto& S : Steps->ArrayItems) {
		const std::string Op = S->GetString("op");
		if (Op == "open") {
			Model.OpenMenu(S->Find("tab") ? S->GetString("tab") : std::string());
		} else if (Op == "close") {
			Model.CloseMenu();
		} else if (Op == "toggle") {
			Model.Toggle();
		} else if (Op == "set_active") {
			const bool R = Model.SetActive(S->GetString("key"));
			if (S->Find("expected_ok") && R != S->GetBool("expected_ok")) {
				Detail = "set_active " + S->GetString("key") + " ok mismatch";
				return false;
			}
		} else if (Op == "expect_active") {
			if (Model.ActiveTab() != S->GetString("key")) {
				Detail = "active '" + Model.ActiveTab() + "' want '" + S->GetString("key") + "'";
				return false;
			}
		} else if (Op == "expect_open") {
			if (Model.IsOpen() != S->GetBool("value")) {
				Detail = "open mismatch";
				return false;
			}
		}
	}
	return true;
}

void RunPauseMenuCases(const std::string& Dir) {
	std::printf("\n-- module-bundle tab gating + the reducer (pause-menu-cases.json) --\n");
	FJsonValuePtr Root = LoadJson(Dir, "pause-menu-cases.json");
	const FJsonValue* Cases = CasesOf(Root, "pause-menu-cases.json");
	if (!Cases) {
		return;
	}

	for (std::size_t i = 0; i < Cases->Size(); ++i) {
		const FJsonValue& C = *Cases->ArrayItems[i];
		FInsimulPauseMenuModel Model(StringArray(C.Find("enabled_modules")), TabsFromJson(C.Find("tabs")));
		std::string Detail;
		Report("pause: " + C.GetString("name"), ComparePause(C, Model, Detail), Detail);
	}

	// ── Negative controls ─────────────────────────────────────────────────────
	// An extra ungated tab is visible in every bundle, so every case's expected
	// visible set must stop matching.
	{
		int Fired = 0;
		for (std::size_t i = 0; i < Cases->Size(); ++i) {
			const FJsonValue& C = *Cases->ArrayItems[i];
			std::vector<FPauseTab> Tabs = TabsFromJson(C.Find("tabs"));
			FPauseTab Extra;
			Extra.Key = "a_tab_nobody_authored";
			Extra.Label = "Extra";
			Tabs.push_back(Extra);
			FInsimulPauseMenuModel Model(StringArray(C.Find("enabled_modules")), Tabs);
			std::string Detail;
			if (!ComparePause(C, Model, Detail)) {
				Fired++;
			}
		}
		Report("control: an unauthored tab reddens every case",
				Fired == static_cast<int>(Cases->Size()),
				std::to_string(Fired) + "/" + std::to_string(Cases->Size()));
	}

	// THE gate of this area: the module bundle is what withholds a tab. Enable
	// every module a case's tabs ask for and the cases that hid one must change.
	{
		int Gated = 0;
		int Red = 0;
		for (std::size_t i = 0; i < Cases->Size(); ++i) {
			const FJsonValue& C = *Cases->ArrayItems[i];
			const std::vector<FPauseTab> Tabs = TabsFromJson(C.Find("tabs"));
			std::vector<std::string> All = StringArray(C.Find("enabled_modules"));
			bool bHidTab = false;
			for (const FPauseTab& Tab : Tabs) {
				for (const std::string& Need : Tab.Requires) {
					bool bHave = false;
					for (const std::string& Have : All) {
						bHave = bHave || Have == Need;
					}
					if (!bHave) {
						All.push_back(Need);
						bHidTab = true;
					}
				}
			}
			if (!bHidTab) {
				continue;
			}
			Gated++;
			FInsimulPauseMenuModel Model(All, Tabs);
			std::string Detail;
			if (!ComparePause(C, Model, Detail)) {
				Red++;
			}
		}
		Report("control: enabling the missing modules reveals every withheld tab",
				Gated > 0 && Red == Gated, std::to_string(Red) + "/" + std::to_string(Gated));
	}

	// The shared default tab set (what the shipped ESC menu shows with no bundle).
	{
		FInsimulPauseMenuModel M;
		const std::vector<std::string> Keys = M.VisibleKeys();
		Report("pause: default model with no modules shows the 6 core tabs",
				Keys.size() == 6 && Keys.front() == "resume" && Keys.back() == "save");
	}
}

// ── Save-slot corpus ─────────────────────────────────────────────────────────

FSlotResult SlotFromJson(const FJsonValue* S) {
	FSlotResult R;
	R.Index = static_cast<int>(S->GetInt("index"));
	R.Outcome = S->GetString("outcome");
	if (const FJsonValue* Sum = S->Find("summary")) {
		R.bHasSummary = true;
		R.Summary.PlayerName = Sum->GetString("playerName");
		if (Sum->Find("level")) {
			R.Summary.bHasLevel = true;
			R.Summary.Level = Sum->GetInt("level");
		}
		R.Summary.LocationName = Sum->GetString("locationName");
		R.Summary.SavedAt = Sum->GetString("savedAt");
	}
	return R;
}

std::vector<FSlotResult> SlotsFromCase(const FJsonValue& Case) {
	std::vector<FSlotResult> Slots;
	if (const FJsonValue* SlotsJson = Case.Find("slots")) {
		if (SlotsJson->IsArray()) {
			for (const auto& S : SlotsJson->ArrayItems) {
				Slots.push_back(SlotFromJson(S.get()));
			}
		}
	}
	return Slots;
}

/** Diff rendered rows (status / title / message / can_load / can_save) + the
 *  Continue gate against a case. */
bool CompareSlots(const FJsonValue& Case, const FInsimulSaveSlotModel& Model, std::string& Detail) {
	const FJsonValue* Exp = Case.Find("expected");
	const std::vector<FSlotView> Got = Model.Slots();
	const std::size_t WantN = Exp && Exp->IsArray() ? Exp->Size() : 0;
	if (Got.size() != WantN) {
		Detail = "rows n=" + std::to_string(Got.size()) + " want " + std::to_string(WantN);
		return false;
	}
	for (std::size_t r = 0; r < WantN; ++r) {
		const FJsonValue* W = Exp->ArrayItems[r].get();
		const FSlotView& V = Got[r];
		if (V.Index != static_cast<int>(W->GetInt("index")) || V.Status != W->GetString("status") ||
				V.Title != W->GetString("title") || V.Message != W->GetString("message") ||
				V.bCanLoad != W->GetBool("can_load") || V.bCanSave != W->GetBool("can_save")) {
			Detail = "row[" + std::to_string(r) + "] status=" + V.Status + " title='" + V.Title +
					"' msg='" + V.Message + "' want status=" + W->GetString("status") + " title='" +
					W->GetString("title") + "'";
			return false;
		}
	}
	if (Case.Find("expected_has_loadable")) {
		if (Model.HasAnyLoadable() != Case.GetBool("expected_has_loadable")) {
			Detail = "has_loadable mismatch";
			return false;
		}
	}
	return true;
}

void RunSaveSlotCases(const std::string& Dir) {
	std::printf("\n-- slot rendering + corrupted-envelope messaging (save-slot-cases.json) --\n");
	FJsonValuePtr Root = LoadJson(Dir, "save-slot-cases.json");
	const FJsonValue* Cases = CasesOf(Root, "save-slot-cases.json");
	if (!Cases) {
		return;
	}

	int Corrupted = 0;
	for (std::size_t i = 0; i < Cases->Size(); ++i) {
		const FJsonValue& C = *Cases->ArrayItems[i];
		const std::vector<FSlotResult> Slots = SlotsFromCase(C);
		FInsimulSaveSlotModel Model(Slots);
		std::string Detail;
		Report("save: " + C.GetString("name"), CompareSlots(C, Model, Detail), Detail);
		for (const FSlotResult& Slot : Slots) {
			if (Slot.Outcome != "empty" && Slot.Outcome != "ok") {
				Corrupted++;
			}
		}
	}
	// AC2 names the corrupted envelope explicitly: it is a case, not an anecdote.
	Report("save: the corpus carries corrupted envelopes (integrity / format / payload)",
			Corrupted >= 3, std::to_string(Corrupted) + " corrupt slot(s)");

	// ── Negative controls ─────────────────────────────────────────────────────
	{
		int Fired = 0;
		for (std::size_t i = 0; i < Cases->Size(); ++i) {
			const FJsonValue& C = *Cases->ArrayItems[i];
			std::vector<FSlotResult> Slots = SlotsFromCase(C);
			FSlotResult Extra;
			Extra.Index = 99;
			Extra.Outcome = "ok";
			Slots.push_back(Extra);
			FInsimulSaveSlotModel Model(Slots);
			std::string Detail;
			if (!CompareSlots(C, Model, Detail)) {
				Fired++;
			}
		}
		Report("control: an unexpected slot reddens every case",
				Fired == static_cast<int>(Cases->Size()),
				std::to_string(Fired) + "/" + std::to_string(Cases->Size()));
	}

	// The OUTCOME is what makes a row corrupted — call the same slot healthy and
	// the message, the status and the load gate all have to move.
	{
		int Healed = 0;
		int Red = 0;
		for (std::size_t i = 0; i < Cases->Size(); ++i) {
			const FJsonValue& C = *Cases->ArrayItems[i];
			std::vector<FSlotResult> Slots = SlotsFromCase(C);
			bool bAny = false;
			for (FSlotResult& Slot : Slots) {
				if (Slot.Outcome != "empty" && Slot.Outcome != "ok") {
					Slot.Outcome = "ok";
					bAny = true;
				}
			}
			if (!bAny) {
				continue;
			}
			Healed++;
			FInsimulSaveSlotModel Model(Slots);
			std::string Detail;
			if (!CompareSlots(C, Model, Detail)) {
				Red++;
			}
		}
		Report("control: calling a corrupted envelope healthy reddens every such case",
				Healed > 0 && Red == Healed, std::to_string(Red) + "/" + std::to_string(Healed));
	}

	// The cross-engine messaging contract, verbatim.
	{
		Report("save: integrity_mismatch messaging is the cross-engine contract",
				FInsimulSaveSlotModel::MessageForOutcome("integrity_mismatch") ==
						"Save file integrity check failed — file may be corrupted or tampered.");
		Report("save: a healthy or empty outcome carries no failure message",
				FInsimulSaveSlotModel::MessageForOutcome("ok").empty() &&
						FInsimulSaveSlotModel::MessageForOutcome("empty").empty());
	}

	// ── The main-menu Continue gate ───────────────────────────────────────────
	//
	// Which slot Continue resumes is a DECISION, so it lives in this core rather
	// than in the menu widget (which has no host gate). The shared corpus pins the
	// rows and `expected_has_loadable`, not the ordering — core's own main menu is
	// playthrough-based — so the expectations below are DERIVED from each case:
	// the answer must be a can_load row, never a corrupted one, and it must be the
	// savedAt-maximal loadable row. Deriving beats asserting a constant, because a
	// derivation still fails when the model changes its mind.
	{
		int Checked = 0;
		int Wrong = 0;
		for (std::size_t i = 0; i < Cases->Size(); ++i) {
			const FJsonValue& C = *Cases->ArrayItems[i];
			const std::vector<FSlotResult> Slots = SlotsFromCase(C);
			const FInsimulSaveSlotModel Model(Slots);
			const int Answer = Model.ContinueSlot();

			// What the corpus itself says the answer must be, computed from the
			// EXPECTED rows rather than from the model under test.
			int WantIndex = -1;
			std::string WantSavedAt;
			const FJsonValue* Exp = C.Find("expected");
			for (std::size_t r = 0; Exp && Exp->IsArray() && r < Exp->Size(); ++r) {
				const FJsonValue* W = Exp->ArrayItems[r].get();
				if (!W->GetBool("can_load")) {
					continue;
				}
				const int Index = static_cast<int>(W->GetInt("index"));
				std::string SavedAt;
				for (const FSlotResult& Slot : Slots) {
					if (Slot.Index == Index && Slot.bHasSummary) {
						SavedAt = Slot.Summary.SavedAt;
					}
				}
				if (WantIndex < 0 || SavedAt > WantSavedAt ||
						(SavedAt == WantSavedAt && Index < WantIndex)) {
					WantIndex = Index;
					WantSavedAt = SavedAt;
				}
			}
			Checked++;
			if (Answer != WantIndex) {
				Wrong++;
				continue;
			}
			// The reason and the answer are one state: exactly one of them speaks.
			if ((Answer < 0) != !Model.ContinueBlockedReason().empty()) {
				Wrong++;
				continue;
			}
			// Never a corrupted slot, and never one the rows call unloadable.
			if (Answer >= 0 && (Model.Slot(Answer).Status == "corrupted" ||
									   !Model.Slot(Answer).bCanLoad)) {
				Wrong++;
			}
		}
		Report("save: Continue resumes the newest LOADABLE slot in every case",
				Checked > 0 && Wrong == 0,
				std::to_string(Checked - Wrong) + "/" + std::to_string(Checked));
	}

	// A directory holding nothing but a tampered save is the case AC2 names: the
	// entry is refused AND the refusal says why, in the cross-engine words.
	{
		std::vector<FSlotResult> Broken;
		FSlotResult Tampered;
		Tampered.Index = 0;
		Tampered.Outcome = "integrity_mismatch";
		Tampered.bHasSummary = true;
		Tampered.Summary.SavedAt = "2999-01-01T00:00:00.000Z"; // as recent as it gets
		Broken.push_back(Tampered);
		const FInsimulSaveSlotModel Model(Broken);
		Report("save: a corrupted-only directory refuses Continue however recent the save",
				Model.ContinueSlot() < 0 && !Model.HasAnyLoadable());
		Report("save: and the refusal is the integrity message, not silence",
				Model.ContinueBlockedReason() ==
						"Save file integrity check failed — file may be corrupted or tampered.");

		const FInsimulSaveSlotModel Fresh;
		Report("save: a fresh install is told there is nothing to continue",
				Fresh.ContinueSlot() < 0 &&
						Fresh.ContinueBlockedReason() == "No saved game to continue.");
	}

	// The ordering rule, stated: the newest stamp wins, an unstamped save loses to
	// a stamped one, ties fall to the lower index — and none of it depends on the
	// order the caller listed its slots in.
	{
		FSlotResult Old;
		Old.Index = 0;
		Old.Outcome = "ok";
		Old.bHasSummary = true;
		Old.Summary.SavedAt = "2026-01-01T00:00:00.000Z";
		FSlotResult New;
		New.Index = 1;
		New.Outcome = "ok";
		New.bHasSummary = true;
		New.Summary.SavedAt = "2026-08-13T00:00:00.000Z";
		FSlotResult Unstamped;
		Unstamped.Index = 2;
		Unstamped.Outcome = "ok";

		Report("save: the newest stamp wins",
				FInsimulSaveSlotModel(std::vector<FSlotResult>{Old, New}).ContinueSlot() == 1);
		Report("save: listing order does not change the answer",
				FInsimulSaveSlotModel(std::vector<FSlotResult>{New, Old}).ContinueSlot() == 1);
		Report("save: an unstamped save loses to a stamped one",
				FInsimulSaveSlotModel(std::vector<FSlotResult>{Unstamped, Old}).ContinueSlot() == 0);
		FSlotResult Tie = New;
		Tie.Index = 5;
		Report("save: an equal stamp falls to the lower slot index",
				FInsimulSaveSlotModel(std::vector<FSlotResult>{Tie, New}).ContinueSlot() == 1);

		// Negative controls: the answer MOVES when the thing it measures moves.
		FSlotResult Newest = New;
		Newest.Index = 3;
		Newest.Summary.SavedAt = "2026-12-31T00:00:00.000Z";
		Report("control: a newer loadable save moves the answer",
				FInsimulSaveSlotModel(std::vector<FSlotResult>{Old, New, Newest}).ContinueSlot() == 3);
		FSlotResult Broken = New;
		Broken.Outcome = "integrity_mismatch";
		Report("control: corrupting the winner hands Continue to the runner-up",
				FInsimulSaveSlotModel(std::vector<FSlotResult>{Old, Broken}).ContinueSlot() == 0);
	}
}

// ── The history a panel projects, in the REAL save envelope ──────────────────
//
// The chat corpus proves the PROJECTION; this proves where it lands. A dialogue
// panel persists to save.conversations and nowhere else, so after a flush the save
// must carry the turns, a re-hydrate must reproduce them, and everything outside
// `conversations` — currentState above all — must be byte-identical.

bool SameHistory(const FChatHistory& A, const FChatHistory& B) {
	if (A.TotalTurnCount != B.TotalTurnCount || A.RecentTurns.size() != B.RecentTurns.size()) {
		return false;
	}
	for (std::size_t i = 0; i < A.RecentTurns.size(); ++i) {
		if (A.RecentTurns[i].Role != B.RecentTurns[i].Role ||
				A.RecentTurns[i].Content != B.RecentTurns[i].Content ||
				A.RecentTurns[i].Timestamp != B.RecentTurns[i].Timestamp) {
			return false;
		}
	}
	return true;
}

void RunHistoryPersistence(const std::string& SavesDir) {
	std::printf("\n-- the dialogue history lands in save.conversations (v2-typical.json) --\n");

	const std::string SaveText = ReadFile(SavesDir + "/v2-typical.json");
	if (SaveText.empty()) {
		Report("history: the save fixture is readable", false, SavesDir);
		return;
	}
	FInsimulSaveSystem Save;
	std::string Error;
	if (!Save.Load(SaveText, Error)) {
		Report("history: the save fixture loads", false, Error);
		return;
	}
	FJsonValue* Root = Save.MutableSaveFile();
	if (!Root) {
		Report("history: the loaded save exposes its tree", false);
		return;
	}
	Report("history: the save fixture loads and migrates", true);

	// The fixture has already spoken to this character — the panel must OPEN on
	// what the save holds, not on an empty transcript.
	const std::string CharId = "npc-shopkeeper";
	FChatHistory Existing;
	if (!FInsimulUIStateBinding::HydrateConversation(*Root, CharId, Existing, Error)) {
		Report("history: the save's turns hydrate", false, Error);
		return;
	}
	Report("history: the save's turns hydrate",
			Existing.RecentTurns.size() == 2 && Existing.TotalTurnCount == 2 &&
					Existing.RecentTurns[0].Role == "npc" && Existing.RecentTurns[1].Role == "player",
			std::to_string(Existing.RecentTurns.size()) + " turn(s)");

	// A stranger the save has never met is an EMPTY history, not a failure.
	{
		FChatHistory Fresh;
		const bool bOk = FInsimulUIStateBinding::HydrateConversation(*Root, "npc-nobody", Fresh, Error);
		Report("history: an unmet character hydrates empty rather than failing",
				bOk && Fresh.RecentTurns.empty() && Fresh.TotalTurnCount == 0, Error);
	}

	// One turn through the real panel core, stamped by the caller (the host owns
	// the clock — the model mints no timestamp of its own).
	FInsimulChatModel Panel(CharId, "Marie");
	for (const FChatHistoryTurn& Turn : Existing.RecentTurns) {
		// Replay what the save holds so a flush appends rather than truncates.
		if (Turn.Role == "npc") {
			Panel.Greeting(Turn.Content);
		}
	}
	Panel.BeginUserTurn("Do you have any bread?");
	Panel.AppendChunk("Of course — ");
	Panel.AppendChunk("two loaves, fresh.");
	FChatAction Sale;
	Sale.Name = "offer_item";
	Sale.Args.push_back("item-bread");
	Sale.FactToAssert = "offered(npc-shopkeeper,item-bread)";
	Panel.TriggerAction(Sale);
	Panel.CompleteTurn();
	Report("history: the panel's action reached the list the KB is fed from",
			Panel.ActionList().size() == 1 &&
					Panel.ActionList()[0].FactToAssert == "offered(npc-shopkeeper,item-bread)");
	Report("history: the settled NPC line is the TTS / lip-sync source",
			Panel.LastNpcText() == "Of course — two loaves, fresh.");

	const FChatHistory Flushed = Panel.History("2026-02-01T12:30:00.000Z");
	const std::string OutsideBefore = FInsimulUIStateBinding::CanonicalOutsideConversations(*Root);

	// The gate that can fail: nothing is in the save until it is written back.
	{
		FChatHistory NotYet;
		FInsimulUIStateBinding::HydrateConversation(*Root, CharId, NotYet, Error);
		Report("control: a conversation that is never flushed leaves the save alone",
				!SameHistory(NotYet, Flushed));
	}

	if (!FInsimulUIStateBinding::ApplyConversation(CharId, "Marie", Flushed, *Root, Error)) {
		Report("history: the panel's history writes back into save.conversations", false, Error);
		return;
	}
	Report("history: the panel's history writes back into save.conversations", true);

	// 1. It is in the bytes a save WRITE would emit.
	{
		const std::string Canonical = Save.SerializeCanonical();
		Report("history: the new turn is in the save's canonical bytes",
				Canonical.find("two loaves, fresh.") != std::string::npos &&
						Canonical.find("2026-02-01T12:30:00.000Z") != std::string::npos);
	}

	// 2. A re-hydrate reproduces it exactly — the panel kept nothing back.
	{
		FChatHistory Reloaded;
		FInsimulUIStateBinding::HydrateConversation(*Root, CharId, Reloaded, Error);
		Report("history: a re-hydrate reproduces the flushed turns exactly",
				SameHistory(Reloaded, Flushed),
				std::to_string(Reloaded.RecentTurns.size()) + " vs " +
						std::to_string(Flushed.RecentTurns.size()));

		// Control: the comparison above is sensitive to a single character.
		FChatHistory Tampered = Reloaded;
		if (!Tampered.RecentTurns.empty()) {
			Tampered.RecentTurns.front().Content += "!";
		}
		Report("control: one changed character reddens the re-hydrate comparison",
				!SameHistory(Tampered, Flushed));
	}

	// 3. Everything outside `conversations` is byte-identical: no playthrough data
	//    leaked sideways, and currentState above all was not touched.
	{
		const std::string OutsideAfter = FInsimulUIStateBinding::CanonicalOutsideConversations(*Root);
		Report("history: everything outside conversations is byte-identical",
				OutsideAfter == OutsideBefore);
		// Control: that instrument is not a constant — move a field it covers.
		FJsonValuePtr Moved = DeepCopy(*Root);
		FTradeState State;
		if (FInsimulUIStateBinding::HydrateTrade(*Moved, State, Error)) {
			State.PlayerGold += 1;
			FInsimulUIStateBinding::ApplyTrade(State, *Moved, Error);
			Report("control: the instrument moves when currentState does",
					FInsimulUIStateBinding::CanonicalOutsideConversations(*Moved) != OutsideAfter);
		} else {
			Report("control: the instrument moves when currentState does", false, Error);
		}
	}

	// 4. The row's OTHER fields are the world's, not this seam's.
	{
		const FJsonValue* Rows = Root->Find("conversations");
		const FJsonValue* Row = nullptr;
		if (Rows && Rows->IsArray()) {
			for (const auto& Candidate : Rows->ArrayItems) {
				if (Candidate->GetString("npcCharacterId") == CharId) {
					Row = Candidate.get();
				}
			}
		}
		Report("history: the character's row is updated in place, not duplicated",
				Rows && Rows->IsArray() && Rows->Size() == 1 && Row != nullptr);
		Report("history: the fields this seam does not own survive the flush",
				Row && Row->GetString("lastLocationId") == "lot-shop" &&
						Row->Find("topics") != nullptr && Row->Find("wordsUsed") != nullptr);
		Report("history: the display name it was given is written",
				Row && Row->GetString("npcCharacterName") == "Marie");
	}

	// 5. A character the save has never met appends a row and disturbs no other.
	{
		FInsimulChatModel Stranger("npc-guard", "Ilse");
		Stranger.BeginUserTurn("Who goes there?");
		Stranger.CompleteTurn("State your business.");
		const std::string Before = FInsimulUIStateBinding::CanonicalOutsideConversations(*Root);
		if (!FInsimulUIStateBinding::ApplyConversation("npc-guard", "Ilse",
					Stranger.History("2026-02-01T12:45:00.000Z"), *Root, Error)) {
			Report("history: a first meeting appends its own row", false, Error);
		} else {
			const FJsonValue* Rows = Root->Find("conversations");
			FChatHistory Marie;
			FInsimulUIStateBinding::HydrateConversation(*Root, CharId, Marie, Error);
			Report("history: a first meeting appends its own row",
					Rows && Rows->Size() == 2 && SameHistory(Marie, Flushed));
			Report("history: appending a row still touches nothing outside conversations",
					FInsimulUIStateBinding::CanonicalOutsideConversations(*Root) == Before);
		}
	}

	// ── Refusals: a document that is not a save says so ───────────────────────
	{
		FJsonValue Bare;
		Bare.Type = EJsonType::Object;
		std::string Err;
		FChatHistory Empty;
		const bool bNoRows = FInsimulUIStateBinding::HydrateConversation(Bare, CharId, Empty, Err);
		Report("history: a save with no conversations hydrates empty rather than failing",
				bNoRows && Empty.RecentTurns.empty() && Err.empty());

		// A `conversations` that is not an array is refused, never clobbered.
		FJsonValuePtr Wrong = DeepCopy(*Root);
		for (auto& Pair : Wrong->ObjectItems) {
			if (Pair.first == "conversations") {
				Pair.second = std::make_shared<FJsonValue>();
				Pair.second->Type = EJsonType::String;
				Pair.second->StringValue = "not an array";
			}
		}
		std::string ErrRead;
		std::string ErrWrite;
		const bool bRefusedRead =
				!FInsimulUIStateBinding::HydrateConversation(*Wrong, CharId, Empty, ErrRead);
		const bool bRefusedWrite =
				!FInsimulUIStateBinding::ApplyConversation(CharId, "Marie", Flushed, *Wrong, ErrWrite);
		Report("control: a conversations field that is not an array is refused, not overwritten",
				bRefusedRead && bRefusedWrite && !ErrRead.empty() && !ErrWrite.empty() &&
						Wrong->Find("conversations")->AsString() == "not an array");

		// A history is written FOR a character; an anonymous flush is a bug.
		std::string ErrId;
		Report("control: a flush with no character id is refused",
				!FInsimulUIStateBinding::ApplyConversation("", "Marie", Flushed, *Root, ErrId) &&
						!ErrId.empty());
	}
}

} // namespace

int main(int argc, char** argv) {
	std::string UiDir =
#ifdef INSIMUL_UI_DIR
			INSIMUL_UI_DIR;
#else
			"../../conformance/ui";
#endif
	std::string SavesDir =
#ifdef INSIMUL_FIXTURE_DIR
			INSIMUL_FIXTURE_DIR;
#else
			"../../conformance/saves";
#endif
	std::string Only;

	for (int i = 1; i < argc; ++i) {
		const std::string Arg = argv[i];
		if (Arg == "--only" && i + 1 < argc) {
			Only = argv[++i];
		} else if (Arg == "--ui" && i + 1 < argc) {
			UiDir = argv[++i];
		} else if (Arg == "--saves" && i + 1 < argc) {
			SavesDir = argv[++i];
		} else if (Arg.rfind("--", 0) != 0) {
			UiDir = Arg;
		}
	}

	std::printf("default-UI dialogue / pause-menu / save-load view-models (190 US-3)\n");
	std::printf("ui corpus: %s\nsaves corpus: %s\nleg: %s\n", UiDir.c_str(), SavesDir.c_str(),
			Only.empty() ? "all" : Only.c_str());

	const bool bAll = Only.empty();
	if (bAll || Only == "chat") {
		RunChatCases(UiDir);
	}
	if (bAll || Only == "pause") {
		RunPauseMenuCases(UiDir);
	}
	if (bAll || Only == "save") {
		RunSaveSlotCases(UiDir);
	}
	if (bAll || Only == "history") {
		RunHistoryPersistence(SavesDir);
	}
	if (!bAll && Only != "chat" && Only != "pause" && Only != "save" && Only != "history") {
		std::printf("  FAIL  unknown --only area '%s' (chat|pause|save|history)\n", Only.c_str());
		return 1;
	}

	std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
