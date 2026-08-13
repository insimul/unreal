# Insimul default-UI (Unreal) — registry, theme tokens, loading view-model

This is the Unreal leg of the shared default-runtime UI (plan §4.5). It is a
mirror of the same contract the Babylon reference and the Unity/Godot plugins
implement, so the **behavior** and the **design tokens** are pinned by an
engine-neutral corpus under `packages/core/conformance/ui/` — every engine runs
the same cases. Here the runnable proof is the host test
`Source/InsimulRuntime/Tests/test_ui_registry.cpp` (run via
`npm run engines:unreal:ui`).

## The two layers: UE-free core + syntax-gated UE seam

Following the repo convention (see `CLAUDE.md`), all decision logic lives in
UE-free portable cores that host-test on a bare clang toolchain; the UMG /
UObject boundary is a thin layer on top, verified structurally only (a human
builds it in a real editor — `autoMerge` is off).

| Concern | UE-free core (host-tested) | UE seam (syntax-gated) |
| --- | --- | --- |
| Panel registry | `Portable/InsimulUIRegistryModel.{h,cpp}` | `UInsimulUIRegistry` (`Public/InsimulUIRegistry.h`) |
| Module gate | `Portable/InsimulUIPanelCatalog.{h,cpp}` | `UInsimulUIPanelSurface` (`Public/InsimulUIPanelSurface.h`) |
| Loading screen | `Portable/InsimulLoadingViewModel.{h,cpp}` | `UInsimulLoadingScreen` (`Public/InsimulLoadingScreen.h`) |
| Notifications | `Portable/InsimulNotifications.{h,cpp}` | `UInsimulNotificationsWidget` (`Public/InsimulNotificationsWidget.h`) |
| Theme tokens | `Portable/InsimulUIThemeTokens.{h,cpp}` | `UInsimulUITheme` (`Public/InsimulUITheme.h`) |

## Panel registry — `InsimulUIRegistryModel` / `UInsimulUIRegistry`

Maps a stable panel **key** to a widget reference, with a creator **override**
layer and **missing-panel diagnostics**.

- **Default map** — `FInsimulUIRegistryModel::DefaultPanelMap()` ships one entry
  per default-UI panel (`loading_screen`, `notifications`, `hud`, `main_menu`,
  `game_menu`, `quest_journal`, `quest_tracker`, `quest_offer`, `inventory`,
  `container`, `merchant`, `dialogue`, `pause_menu`, `save_load`), each mapped to
  a `/Game/UI/WBP_*` asset path. The key list is pinned by
  `conformance/ui/registry-cases.json → panel_keys`.
- **Creator override** — a per-key override always wins over the shipped default.
  `Register(key, ref)` / `ApplyOverrides(map)` add the override layer; at the UE
  seam `UInsimulUIRegistry.Panels` is the editable list (a later entry for a key
  overrides an earlier one), pointed at from `UInsimulSettings.UIRegistry`.
- **Diagnostics** — `SceneRef(key)` records a `missing_panel` diagnostic for an
  unknown key (surfaced via `Diagnostics()` / `HasDiagnostics()`); `PeekRef(key)`
  is a non-mutating peek. `UInsimulUIRegistry::ResolvePanelClass` logs a warning
  so a creator sees exactly which panel is unbound.

### Generated default asset

`templates/scripts/GenerateInsimulContent.py` is the source of the *default*
registry. After creating each `WBP_*` it registers the WBP's generated class
under the spec's `panel_key` into `DA_InsimulUIRegistry`. See that file's
**OUTPUT CONTRACT** docstring. Panels whose widget the generator does not yet
create (US-XU2..XU4 finish them) are absent from the generated asset but present
in `DefaultPanelMap()` as the intended target path.

## Loading view-model — `InsimulLoadingViewModel`

Driven by the runtime boot loop (`world source → save slot → KB → systems init`;
see `Portable/InsimulBootstrap.h`). `Advance(key)` moves through the ordered
weighted phases yielding a **monotonic** progress fraction, a phase label, and a
deterministic per-phase tip. Phases/weights/tips mirror
`conformance/ui/loading-phases.json`; progress at a phase = cumulative weight
through that phase ÷ total weight. Re-entering the current phase (or an earlier
one) never lowers the bar. `IsComplete()` is true at the terminal phase.

## Theme tokens — `InsimulUIThemeTokens` / `UInsimulUITheme`

`Portable/InsimulUIThemeTokens.{h,cpp}` mirrors `conformance/ui/theme-tokens.json`
(the single source of truth); the host test asserts the two agree byte-for-byte.
`UInsimulUITheme` (`DA_InsimulUITheme`, created by the generator) exposes the
tokens as `FLinearColor` / `int` for the widgets to read. A value that diverges
from a token in the JSON is a parity bug — keep them in lockstep.

### Token → Slate mapping

| Token (theme-tokens.json)     | Value      | Unreal binding |
| ----------------------------- | ---------- | -------------- |
| `colors.background`           | `#12141c`  | loading-screen backdrop fill |
| `colors.surface`              | `#1b1e2a`  | panel / border-brush background; toast bg |
| `colors.surface_alt`          | `#242838`  | disabled button, progress-bar background |
| `colors.overlay`              | `#0a0b10cc`| modal scrims (dialogue / menus) |
| `colors.border`               | `#333a52`  | border brush color |
| `colors.text_primary`         | `#eef1f8`  | primary label / button `ColorAndOpacity` |
| `colors.text_secondary`       | `#9aa3bd`  | secondary text; loading tip |
| `colors.text_disabled`        | `#5a6076`  | disabled text |
| `colors.accent`               | `#5b8cff`  | button normal; progress-bar fill |
| `colors.accent_hover`         | `#7aa2ff`  | button hovered |
| `colors.accent_pressed`       | `#3f6fe0`  | button pressed |
| `colors.success`              | `#4ecb8d`  | success toast/border |
| `colors.warning`              | `#e6b34d`  | warning toast/border |
| `colors.danger`               | `#e05a6a`  | danger toast/border, save integrity failure |
| `colors.quest`                | `#c9a24b`  | quest markers/highlights |
| `spacing.{xs,sm,md,lg,xl}`    | 4/8/12/16/24 | slot padding, container separation |
| `radius.{sm,md,lg}`           | 4/8/12     | rounded-box corner radii |
| `font_size.{caption,body,title,display}` | 12/16/22/32 | text-block font sizes |

## Module gating — every panel resolves through the module registry

A panel is not only "which widget serves this key"; it is also "does this world have
this panel at all". Core's module contract §7.3 states the cost of a genre bundle not
selecting a module: **no consulted rule pack and no registered system** — the
module's vocabulary is absent from the KB entirely. A panel over those predicates
would render an empty box, so the UI has to answer the same question the KB does.

- **The ownership is data.** `Content/Data/insimul/ui/panels.json` (shipped from
  `templates/project/…`) carries one row per panel: the key, the widget, and the
  module that owns it. An empty `module` is a panel every world has. Moving a panel
  under a different module — or adding one — is an edit to that file, not to engine
  code: `Portable/InsimulUIPanelCatalog.{h,cpp}` names no mechanic, and
  `tools/verify-mechanics/check-activation.mjs` fails if one ever appears there.
- **The three answers mirror the pack consult exactly.** A KNOWN genre shows its
  modules' panels; an UNKNOWN genre gets no module-owned panel (the same refusal the
  consult makes); an UNDECLARED genre is ungated, because that is the state an editor
  session or a commandlet is in — and it activates every pack, so it must withhold no
  panel either. `UInsimulUIPanelSurface::DescribeSurface()` says which happened.
- **An override never ungates a panel.** Swapping the widget for a key says nothing
  about which modules the bundle selected; the registry (widget) and the catalog
  (existence) are separate questions.
- **Nothing is a silent no-op.** A withheld panel resolves to nothing WITH an
  `inactive_module` diagnostic; an unknown key with a `missing_panel` one.

Who applies the set: the exported game's `UInsimulModuleActivator`, once it has
resolved a genre, hands it to `UInsimulUIPanelSurface::ApplyModuleSet()`.

**The finding behind the data file.** Core emits the genre → module table, and a
module row carries its pack, its IR section, its decision layers and its host
interfaces — but no UI surface. There is no field saying which panels a module
brings, so this ownership table cannot be vendored from core the way the activation
table is; it is this port's own data. The day core emits one, `Parse()` reads it and
the local table goes away. Until then ctest `ui_registry` pins every module id in the
catalog to one the activation table names, so a typo cannot hide a panel in every
world with no error anywhere.

## The pattern-proof pair — loading screen + notifications

The two smallest panels in the suite, built end to end, so the shape every other
panel follows is demonstrated rather than described: a stable panel key resolved
through `UInsimulUIPanelSurface`, a WBP the export generator creates, a UE-free core
host-tested against the shared corpus, and design tokens read from the theme asset.

- **`UInsimulLoadingScreen`** (panel key `loading_screen`, `WBP_LoadingScreen`) is
  driven by PHASES, never by a number: the boot loop calls `AdvancePhase(key)` and
  the view-model turns the ordered weighted table into a monotonic fraction, a label
  and a deterministic tip. Every bound widget is `BindWidgetOptional`. (The narrative
  `WBP_IntroSequence` is a cutscene and no longer carries this key — a game's boot
  progress and its intro are two panels a creator replaces independently.)
- **`UInsimulNotificationsWidget`** (panel key `notifications`, `WBP_Notifications`)
  is the toast stack every system pushes to. It repaints when the queue reports the
  VISIBLE set changed, not per frame, and maps a kind to a theme TOKEN name rather
  than a color, so a re-skin moves every toast with the rest of the UI.

## Tests

`npm run check:host` from `tools/` runs ctest **`ui_registry`**: `test_ui_registry.cpp`
against the shared corpus and the shipped data — the registry cases (default /
override precedence / missing diagnostics + the real WBP default map), the loading
cases (weighted progress, monotonicity, labels, completion, tips), the theme-token
table, the panel catalog (covers every corpus key, agrees with the built-in fallback
map, names only modules the activation table knows) and the module gate (per-genre
withholding, override-cannot-ungate, the diagnostics), plus six negative controls.

Before tasklist 190 US-1 this section named `tools/verify-unreal/run-ui-tests.sh` and
`npm run engines:unreal:ui`. Neither exists in this repository: the four UI host
tests under `Source/InsimulRuntime/Tests/` were compiled and run by nothing, so "the
registry tests pass on the shared cases" was a claim with no gate behind it. The
registry/loading/theme/gate leg is a ctest target now. US-2 wired the rest of its
half: `test_quest_journal.cpp` is ctest `ui_quest_journal`, `test_trade.cpp` is
`ui_trade`, `test_ui_state_binding.cpp` is `ui_state_binding` and
`test_skill_tree.cpp` is `ui_skill_tree`. `test_dialogue_ui.cpp` is still orphaned
and is US-3's to wire.

## Quest panels + notifications (US-XU2)

`Portable/InsimulQuestJournalModel.{h,cpp}` is the engine-neutral view-model the
three quest panels share — the JOURNAL (`QuestJournalWidget`: tab filtering +
counts), the TRACKER HUD (`InsimulQuestTrackerWidget`: a bounded set of tracked
active quests, `max_tracked`), and the OFFER dialog (`InsimulQuestOfferPanel`:
accept / decline; radiant `quest_offered` arrivals land here via `Upsert`).
Lifecycle transitions mirror the real `FInsimulQuestSystem` signals: accept
(available→active), complete (active→completed, auto-untracked), upsert (a radiant
arrival appears as available). It mirrors `packages/core/src/ui/quest-journal-model.ts`
and the Godot `quest_journal_model.gd` case-for-case against the shared corpus
`conformance/ui/quest-journal-cases.json`.

**Event-driven (no polling):** every state-changing op emits a `FQuestJournalEvent`
(`Reset`/`QuestAdded`/`QuestUpdated`/`QuestAccepted`/`QuestDeclined`/`QuestCompleted`/
`QuestTracked`/`QuestUntracked`/`FilterChanged`) to registered listeners
synchronously. Read-only ops (`Filtered`/`Counts`/`TrackedIds`) and rejected ops
(accepting a non-available quest, tracking past `max_tracked`) emit **nothing**. The
UE seam `UInsimulQuestJournal` (`Public/InsimulQuestJournal.h`) wraps the portable
model (pimpl) and forwards these events through the `OnQuestJournalChanged` dynamic
multicast delegate, so the UMG widgets rebuild only when the model changes — they
never poll each frame.

`Portable/InsimulNotifications.{h,cpp}` is the transient-toast queue (push / tick
lifetime expiry / dismiss / `kind→color` mapping to the shared theme tokens),
mirroring `notifications.ts` / `insimul_notifications.gd`. It ties into the quest
panels as the "+ notifications" seam: a listener on the quest model `Push`es a toast
on accept / complete / radiant-arrival, and the toast Control ages them on its own
timer — no polling either direction.

### Tests

`npm run engines:unreal:quest-ui` (`tools/verify-unreal/run-quest-ui-tests.sh`)
grep-guards the cores as UE-free, then compiles + runs `test_quest_journal.cpp` (29
assertions): the full quest-journal corpus matrix, the event-driven surface
(mutations push events; read-only/rejected ops are silent), the notifications core,
and the quest-events-drive-toasts integration. Wired into `npm run engines:check`
under the Unreal block.

## Trade panels — inventory / container / merchant (US-XU3)

`Portable/InsimulTradeModel.{h,cpp}` is the engine-neutral view-model the three
trade panels share — the INVENTORY (`InsimulInventoryUI`: `PlayerGold` /
`PlayerItems`), the CONTAINER transfer panel (`TakeFromContainer` / `TakeAll`), and
the MERCHANT/shop panel (`InsimulShopPanel`: `Buy` / `Sell` against `MerchantItems`
/ `MerchantGold`). It mirrors `packages/core/src/ui/trade-model.ts` and the Godot
`trade_model.gd` operation-for-operation against the shared corpus
`conformance/ui/trade-cases.json`.

**Backed exclusively by save.currentState (the state-location invariant):** the
model holds a POINTER to the caller's `FTradeState` (the live currentState slice:
`player.gold`/`player.inventory`, `containers.containers[id].items`,
`npcs.merchantStates[id].{goldReserve,items}`) and keeps NO private item store.
Every read returns the live arrays and every mutation touches only the attached
state, so a snapshot at any moment is the whole truth. Conservation is a hard
invariant of every op: items MOVE between stacks (never created/destroyed) and a
merchant trade conserves gold (`player.gold + merchant.goldReserve` constant across
buy/sell). Failure reasons are machine-readable (`no_container` / `not_present` /
`out_of_stock` / `insufficient_gold` / `insufficient_items` /
`merchant_cannot_afford` / `bad_qty` / `no_merchant`).

The UE seam `UInsimulTradePanel` (`Public/InsimulTradePanel.h`) wraps the portable
model (pimpl over `FTradeState` + `FInsimulTradeModel`), marshalling
`FString`/`FInsimulTradeItem`/`FInsimulTradeResult` at the Blueprint boundary. The
save shell hydrates its `FTradeState` from `save.currentState`, so buy/sell/transfer
land in the one place the save serializes.

### Tests

`npm run engines:unreal:trade` (`tools/verify-unreal/run-trade-ui-tests.sh`)
grep-guards the core as UE-free, then compiles + runs `test_trade.cpp` (15
assertions): the full trade corpus matrix (take / take_all / buy / sell, with final
quantities + gold + failure reasons) and the state-location invariant (reads return
the live currentState arrays, two models never share, every op conserves the item /
gold census, a detached model fails safely). Wired into `npm run engines:check`
under the Unreal block.

## The play panels — one store, one view-model each (tasklist 190 US-2)

Every panel below is backed EXCLUSIVELY by `save.currentState` (platform/CLAUDE.md):
`Portable/InsimulUIStateBinding.{h,cpp}` is the one place a panel's slice is read out
of a real `SaveFile` and written back into it, and ctest `ui_state_binding` asserts
after every op that the change is in the save's canonical bytes, that a re-hydrate
reproduces it, that everything OUTSIDE `currentState` is byte-identical, and that the
item + gold census is conserved.

| panel key | widget | owning module | view-model |
| --- | --- | --- | --- |
| `inventory` / `container` / `merchant` | `UInsimulInventoryPanel` / `UInsimulContainerPanel` / `UInsimulMerchantPanel` | equipment | `InsimulTradeModel` + `InsimulTradePricing` |
| `equipment` | `UInsimulEquipmentPanel` | equipment | `InsimulEquipmentModel` |
| `skill_tree` | `UInsimulSkillPanel` | skill | `InsimulSkillTreeModel` |
| `minimap` / `world_map` | `UInsimulMinimapPanel` / `UInsimulWorldMapPanel` | map | — (host-supplied pins) |
| `quickbar` / `radial_menu` | `UInsimulQuickBar` / `UInsimulRadialMenu` | — | — (host-supplied entries) |
| `notice_board` | `UInsimulNoticeBoard` | — | `InsimulQuestJournalModel` |
| `documents` | `UInsimulDocumentPanel` | — | — (authored content) |
| `hud` | `UInsimulHUDPanel` | — | the panel surface itself |

### Skill tree — `InsimulSkillTreeModel` → `UInsimulSkillPanel`

A skill panel is **a value core returns, not a callback it invokes** (module contract
§3 forbids a UI hook on the C ABI), so what the four engine legs share is the
view-model and what differs is how each of them paints it.
`Portable/InsimulSkillTreeModel.{h,cpp}` is core's `skills/skill-view.ts` in C++: for
each authored tree it derives the header (level, cap, banked XP, the next level's
price off the world's own curve, the pool and what the taken nodes have spent) and,
per node, the row it sits on (from the authored PARENT EDGES, never a `tier` field),
its cost, its requirements with parents desugared into one gate, whether it is taken
or available, and the refusal when it is not — in core's `SKILL_UNLOCK_REFUSALS`
order `unknown → owned → points → requires → forbidden`.

Two of those rungs are **handed in, never computed**: `unmet` (authored goals the
rules layer did not satisfy) and `forbidden` (what `permissible/3` refused) are the
KB's answers, and a pure function that guessed one would be inventing it. A caller
with no KB passes neither and gets nodes that read `conditional` — "core has nothing
against this, and something core cannot evaluate might".

Nothing in the file knows a node: labels fall back to ids, the effect kind set is
OPEN (`sings(the_masons_round)` rides through untouched), and every derived list is
sorted by id while authored order is preserved.

### HUD, maps, quick bar, documents

- **`UInsimulHUDPanel`** holds no list of what a HUD contains. Its slots are catalog
  KEYS, and it asks `UInsimulUIPanelSurface` which of them this world may show; a
  withheld slot lands in `WithheldSubPanels()` and in `Describe()`, never silently
  missing. This is the one place the gate becomes visible to a player.
- **`UInsimulMinimapPanel` / `UInsimulWorldMapPanel`** project host-supplied pins —
  the corner map around the player within a range, the fullscreen map over a world
  rect. Both projections are total (a zero range / degenerate rect centres rather
  than dividing). Fast travel is a REQUEST broadcast to the host: whether a world
  lets an actor cross it is the simulation's answer, not a panel's.
- **No discovery or read/unread state is invented.** The save envelope declares no
  map or document slice (see `conformance/saves`), so those flags are the host's and
  the panels never write one back — the same restraint the equipment model shows
  about equipping. Inventing a `currentState.map` schema in one engine port is
  exactly how four legs stop agreeing about what a save contains.
- **`UInsimulQuickBar`** takes the SAME `FInsimulRadialEntry` the wheel does, because
  the two are input surfaces over one set of shortcuts; a second entry struct would
  be the drift itself. A disabled entry is shown and greyed, never hidden.
- **`UInsimulDocumentPanel`** paginates by a character budget over the source text
  rather than by rendered layout, so a page break is the same on four engines and two
  resolutions.

### Tests

- ctest **`ui_skill_tree`** (`test_skill_tree.cpp`) drives all six cases of
  `conformance/skills/trees.json` and diffs the canonical projection byte for byte,
  plus the `funded` (`skill_tree/2` read backwards) and `depths` read-outs — 39
  checks including 18 negative controls: funding the pool moves a `points` refusal,
  dropping the KB's answers moves `requires` and `forbidden`, an unmet goal outranks
  a prohibition, an authored parent edge moves a node's row, a cyclic tree
  terminates, and a level past the end of the curve repeats its last price instead of
  becoming free.
- ctest **`ui_state_binding`** covers the shop + reputation corpus, the equipping
  corpus and the state-location invariant.
- `npm run check:panels` closes the gap between the catalog and the widgets an
  exported game actually builds: every catalog row names a WBP that
  `GenerateInsimulContent.py` generates AND binds to that key, no spec claims a key
  the catalog lacks, and every spec's parent is a `UUserWidget` this repository
  ships. Rows nothing serves yet print as PENDING with the story that owes them (as
  of US-2: `main_menu`, `pause_menu`, `save_load` — US-3's — and `quest_journal`,
  whose export-module widget declares no bound children). Five negative controls.

### A note on the export module's prototypes

`templates/source/ui/` still carries the pre-registry prototypes
(`UInsimulSkillTreePanel` with its hardcoded five tiers, `UInsimulMinimap`,
`UInsimulWorldMap`, `UInsimulDocumentReader`, `UInsimulActionQuickBar`,
`UInsimulHUD`). They are no longer what the panel catalog resolves — the WBP specs
now name the plugin's panels — and the plugin's classes are named apart from them
(`…Panel` suffixes) because a UObject class name is global and two `UInsimulMinimap`s
in one project do not link. Deleting the prototypes is a separate change from
landing their replacements.


## Dialogue panel + pause/main menu + save/load (US-XU4)

The last three default-UI cores mirror the shared corpora case-for-case (Babylon /
Unity / Godot / Unreal); all decision logic is UE-free and host-tested, with a thin
UObject seam on top.

### Dialogue / chat (`InsimulChatModel` → `UInsimulChatPanel`)

`Portable/InsimulChatModel.{h,cpp}` is the streaming conversation view-model. A
player line opens a turn + an empty in-flight NPC bubble; response CHUNKS accumulate
into that bubble (`StreamingText()` is the live-render source); the stream may
TRIGGER actions the panel asserts into the KB (`FactToAssert`); `CompleteTurn()`
(optionally with an authoritative full text that overrides the chunks) or
`FailTurn(error)` closes it. A second `BeginUserTurn` while streaming is rejected. A
failed turn renders an `[Error: …]` bubble and is DROPPED from history. `LastNpcText()`
(the last settled, non-error NPC line) feeds TTS + the `InsimulFaceSync` lip-sync
hook. `History()` projects the settled transcript into the
`save.conversations` (`ConversationSummary.recentTurns`) shape. Shared cases:
`chat-cases.json`. The UE seam `UInsimulChatPanel` marshals the transcript /
actions and broadcasts `OnChatChanged` after every mutation (the no-poll UMG seam).

### Pause / ESC menu (`InsimulPauseMenuModel` → `UInsimulPauseMenu`)

`Portable/InsimulPauseMenuModel.{h,cpp}` gates the unified in-game menu's tabs by the
feature modules the active genre bundle enabled (from the IR feature-modules
registry). Core tabs (`resume`/`journal`/`inventory`/`map`/`settings`/`save`) are
ungated; learning/progression tabs (`character`/`vocabulary`/`skills`/`analytics`/
`assessment`) gate on a module, and a custom tab may AND-gate on several. `OpenMenu`
falls back to the first visible tab (opening to a hidden tab is coerced),
`SetActive` rejects hidden tabs, and `Toggle` flips open/closed. Shared cases:
`pause-menu-cases.json`. The `UInsimulPauseMenu` seam builds its tab bar from
`VisibleTabs()`; the UUserWidget owns the actual pause + input capture.

### Save / load slots (`InsimulSaveSlotModel` → `UInsimulSaveSlotPanel`)

`Portable/InsimulSaveSlotModel.{h,cpp}` renders each codec-reported slot OUTCOME
(empty / ok+summary / `invalid_format` / `missing_save_file` / `integrity_mismatch`,
from the portable save system's canonical-JSON + SHA-256 chain) into a row
(status / title / message / can_load / can_save). A healthy slot's title is
`Name · Lv N · Location`; a corrupted slot cannot load but can be overwritten, and
its MESSAGE is the cross-engine contract — e.g. an integrity mismatch on a tampered
save always reads "Save file integrity check failed — file may be corrupted or
tampered." `HasAnyLoadable()` gates the main-menu Continue button. Shared cases:
`save-slot-cases.json`. The `UInsimulSaveSlotPanel` seam feeds the rows to the
save/load screen.

### Tests

`npm run engines:unreal:dialogue-ui`
(`tools/verify-unreal/run-dialogue-ui-tests.sh`) grep-guards all three cores as
UE-free, then compiles + runs `test_dialogue_ui.cpp` (24 assertions): the full
`chat-cases.json` streaming/action/history matrix, the `pause-menu-cases.json`
tab-gating + open/active reducer, the `save-slot-cases.json` row rendering incl. the
corrupted-envelope messaging, plus a handful of edge-behaviour unit assertions.
Wired into `npm run engines:check` under the Unreal block.
