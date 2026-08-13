# Changelog — Insimul Unreal Plugin

All notable changes to the Unreal Engine plugin are documented here. This package
is independently versioned; its version is the `unreal` entry in the repo-root
`VERSIONS.json` (the single source of truth, enforced by
`npm run engines:manifests`) and must match both `Insimul.uplugin`'s `VersionName`
and the `VERSION` file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **The play panels, backed only by `save.currentState` (US-2 of 190).** Quest
  journal / tracker / offer, inventory / containers / equipment, shop + reputation,
  the skill tree, the HUD and its map / quick-bar / wheel sub-panels, the notice
  board and the document reader. Every one resolves through the module registry, and
  every one that shows playthrough data reads it out of the save file and nowhere
  else.
  - **One store.** `Portable/InsimulUIStateBinding.{h,cpp}` is the single place a
    panel's slice is hydrated out of a real `SaveFile` and applied back into
    `currentState` — trade (`player.gold` / `player.inventory` /
    `containers.containers` / `npcs.merchantStates`), quests (`quests.progress` /
    `quests.dynamicQuests`, with the TITLE coming from the quest system's Prolog
    content rather than the snapshot's `name`), the market a shop prices in (the
    merchant's markup joined to the player's standing in `currentState.reputation`)
    and the item ledger the equipment panel reads. There is deliberately no
    equipment WRITE path: wearing a thing is the items module's decision layer, and
    inventing a `currentState.player.equipped` schema in one engine port is how four
    legs stop agreeing about what a save contains.
  - **The skill panel is a VALUE core returns, not a callback it invokes** (module
    contract §3). `Portable/InsimulSkillTreeModel.{h,cpp}` is core's
    `skills/skill-view.ts` in C++: rows derived from the authored parent EDGES rather
    than a `tier` field, prices off the world's own curve, labels falling back to
    ids, an OPEN effect-kind set, and the refusal ladder in core's own order
    (`unknown → owned → points → requires → forbidden`). The two rungs only a KB can
    answer — an unmet authored goal and a prohibition — are handed in, never guessed.
  - **The HUD holds no list of what a HUD contains.** `UInsimulHUDPanel`'s slots are
    catalog keys it asks `UInsimulUIPanelSurface` about, so a world without the map
    module has no minimap and the HUD SAYS SO (`WithheldSubPanels()` / `Describe()`)
    instead of quietly missing one. The maps project host-supplied pins totally (a
    zero range / degenerate rect centres rather than dividing) and fast travel is a
    request broadcast to the host, never a teleport a panel performs. No discovery or
    read/unread state is invented: the save envelope declares no map or document
    slice, so those flags are the host's and no panel writes one back.
  - **Gates.** ctest **`ui_skill_tree`** drives all six cases of
    `conformance/skills/trees.json` — the view-model the Babylon reference and the
    Unity/Godot ports run — and diffs the canonical projection byte for byte plus the
    `funded` and `depths` read-outs: 39 checks, 18 of them negative controls. ctest
    **`ui_state_binding`** covers the pricing + equipping corpora and the
    state-location invariant. 22 ctest legs with binaries (was 18).
  - **`npm run check:panels`** is new, and it closes a gap `ui_registry` structurally
    cannot see: a catalog row can name a WBP that nothing generates, so the key
    resolves "available" and then renders an asset path that does not exist. The gate
    pins every catalog row to a `GenerateInsimulContent.py` spec bound to that key,
    refuses a spec that claims a key the catalog lacks or names a parent class this
    repo does not ship, and prints the rows nothing serves yet as PENDING with the
    story that owes them (`main_menu`, `pause_menu`, `save_load` — US-3's — and
    `quest_journal`). Five negative controls.
  - **The export generator now builds the whole US-2 set.** `WBP_SkillTree`,
    `WBP_WorldMap`, `WBP_HUD`, `WBP_Inventory`, `WBP_Container`, `WBP_ShopPanel`,
    `WBP_EquipmentPanel`, `WBP_RadialMenu` and `WBP_NoticeBoard` are generated and
    registered under their panel keys, and `WBP_Minimap` / `WBP_ActionQuickBar` /
    `WBP_DocumentReader` were repointed from the export module's pre-registry
    prototypes to the plugin's panels (and given the panel keys they never carried).
    The prototypes in `templates/source/ui/` are untouched and unreferenced by the
    catalog; the plugin's classes carry `…Panel` suffixes because a UObject class
    name is global.

- **Every default-UI panel resolves through the module registry (US-1 of 190).**
  A panel is now two questions, answered in two layers: which widget serves a key
  (the registry, with the creator override on top) and whether this world has the
  panel at all (the module gate). Core's module contract §7.3 says an unselected
  module contributes no consulted pack and no registered system — its vocabulary is
  absent from the KB — so a panel over it would render an empty box. The UI answers
  the same question the KB does.
  - `Source/InsimulRuntime/Portable/InsimulUIPanelCatalog.{h,cpp}` joins the shipped
    panel catalog to a resolved module set. The three answers mirror the pack consult
    exactly: a KNOWN genre shows its modules' panels, an UNKNOWN genre gets none of
    them, and an UNDECLARED genre is ungated because it activates every pack. An
    override never ungates a panel, and both refusals are diagnosed
    (`inactive_module` / `missing_panel`) rather than resolving to nothing quietly.
  - **The ownership is data.** `Content/Data/insimul/ui/panels.json` carries panel
    key → widget → owning module, so moving a panel under a different module is an
    edit to that file and **no engine code change**; the catalog source names no
    mechanic and is in `check:activation`'s no-hardcoded-list scan (11 sources now).
    Core emits no UI surface per module, so this one table is this port's own —
    written up in `docs/ui.md` § Module gating, with ctest pinning every module id in
    it to one core's activation table names.
  - **The UE seam** is `UInsimulUIPanelSurface`, a game-instance subsystem the
    exported game's `UInsimulModuleActivator` hands its resolved set to.
  - **The pattern-proof pair, end to end.** `UInsimulLoadingScreen`
    (`WBP_LoadingScreen`, driven by boot PHASES rather than a number, monotonic by
    the view-model's rule) and `UInsimulNotificationsWidget` (`WBP_Notifications`,
    repainting when the visible set changes rather than per frame, colouring toasts
    by theme TOKEN). `WBP_IntroSequence` keeps its narrative role and gives up the
    `loading_screen` panel key.
  - **A gate where there was none.** The four UI host tests under
    `Source/InsimulRuntime/Tests/` named a runner (`run-ui-tests.sh`) that does not
    exist in this repository, so nothing compiled them and "the registry tests pass
    on the shared cases" had nothing behind it. `test_ui_registry.cpp` is ctest
    **`ui_registry`** now — 43 checks over the shared corpus AND the shipped data,
    with six negative controls. 18 ctest legs with binaries, 13 without. The other
    three UI tests are still orphaned and named as such in `docs/ui.md`.

- **Modules activated from the genre bundle, not from a hardcoded list (US-3 of 146).**
  An exported game now reads which mechanic modules this world turns on out of the
  table core emits, consults exactly the rule packs they own, and unregisters every
  host no active module names. Adding a module to a genre bundle in core is a
  re-vendor here and **no engine code change** — `check:activation` fails if a module
  id or a pack area ever appears in an activation source.
  - `Source/InsimulRuntime/Portable/InsimulModuleActivation.{h,cpp}` resolves a genre
    against the vendored table, keeping core's three answers apart exactly as
    module-contract §7.3 states them: a KNOWN genre gets its modules; an UNKNOWN one
    gets the shared vocabulary and nothing else; **no genre declared** gets every pack
    (right for a commandlet, a warning in a game). `EInsimulGenreSource` reports which
    of the three happened and who said so.
  - `Portable/InsimulModulePacks.{h,cpp}` consults the active packs **in core's
    consult order** (a hard constraint — a `:- dynamic` after a clause is a
    `permission_error`) and names every pack it refused to consult.
    `FInsimulMechanicHosts::RestrictTo` is the other half of §7.3's cost: a wired host
    whose interface no active module names is unregistered, and the drop is logged.
  - **The rule packs are vendored as data the game ships.** No C ABI row returns a
    pack (`core.methods` still answers with five names), so
    `tools/vendor-packs/vendor-packs.mjs` mirrors core's eleven `PREDICATE_PACKS`
    byte-for-byte into `templates/project/Content/Data/insimul/packs/` behind a
    manifest carrying core's commit, a sha256 and the consult order. It **executes**
    core rather than parsing it, resolving a runner (`vite-node`, else `esbuild` +
    node) instead of requiring one. RUNTIME_CORE_ADOPTION.md §14.1 asks core for the
    `prolog.packs` row that would delete all of it.
  - **A playable scene, whose script is a file.** `AInsimulMechanicSampleScene` builds
    a guard, a dark courtyard and a wall: the probes measure, core's own packs decide
    (`detects/2`, `can_traverse/3`), the scene executes the answer. Its steps live in
    `Content/Data/insimul/scenarios/dark-courtyard.json`, so ctest
    `activation_witness` runs the same five steps through the same library — **2
    mechanics end to end**, as a gate rather than a screenshot.
  - **New gates.** `check:packs` and `check:activation` (six negative controls) are
    toolchain-free; ctest **`module_activation`** (444 checks) drives the resolver,
    the consult, the host restriction and the scenario runner over the data a build
    reads, with no library; ctest **`activation_witness`** is the only honest witness
    for "an inactive module contributes nothing" — for all 8 genres × 11 packs, each
    pack's own signature predicate is in a real KB **exactly when** its module is
    active, and `current_predicate(can_afford_stamina/2)` has no solutions in an `rpg`
    world. 17 ctest legs with binaries, 12 without.
  - **Three findings, all core-side (§14).** The activation table names packs the C
    ABI cannot hand over (§14.1); a `SaveFile`'s `worldSnapshot` carries no genre, so
    a resumed game cannot resolve its own module set (§14.2); and an authored
    requirement naming an INACTIVE module's vocabulary **raises** rather than failing —
    measured here independently under `adventure` as
    `existence_error(procedure, has_skill/3)`, and pinned so that fixing it core-side
    fails this repo and closes the finding (§14.3).
- **The band-120 conformance corpora, vendored AND executed (US-2 of 146).**
  `conformance/` grew from 34 mirrored files to **64** at core `76782e5` — the seven
  band-120 decision areas (`combat/`, `items/`, `routines/`, `skills/`, `stealth/`,
  `traversal/`), the module-activation table, and ten new Prolog packs. 529 cases in
  ten areas.
  - **`prolog_corpus` — the corpus is EXECUTED now.** `conformance/prolog/` used to be
    checked for provenance only: present, hashed, byte-identical to core, and run by
    nothing this repo could invoke. `tools/verify-unreal/test_prolog_corpus.cpp` drives
    all **255 cases** — including the **125 band-120 `mechanic-*` cases** — through
    `insimul::InsimulKB`, the plugin's own `libinsimul` wrapper (the same class
    `UInsimulPrologSubsystem` wraps), and diffs core's golden solution sets as an
    unordered multiset. 255/255. It carries a case floor, a required-pack list, live /
    negative / syntax controls, and it checks the one documented corpus amendment
    **both ways** — a rewrite that matches nothing fails as stale, and a case that
    starts passing unamended fails as obsolete.
  - **`check:mechanic-corpora` — the decision half, measured rather than pretended.**
    The other 212 cases cannot be executed in any language: reaching a decision layer
    means a row in `native/corebridge/js/entry.js` and there are none. The new gate
    pins every area, file and case count; requires every `executedBy` to name a file
    that exists and a ctest target CMake registers; requires every unexecuted area to
    state its blocker; mirrors the bridge method list from `test_mechanic_hosts.cpp`
    so an ARRIVING mechanic row fails as loudly as a vanishing one; checks the
    vendored genre-activation table against `MODULE_HOSTS.json`; and proves all of it
    can fail with **eleven negative controls**.
- **The band-120 mechanic host half — eight interfaces, implemented (US-1 of 146).**
  Core's seven mechanic modules in band 120–125 name eight distinct host interfaces
  (`ICombatSystem`, `ISurvivalSystem`, `ICombatStatSink`, `ITrajectoryProbe`,
  `IPerceptionProbe`, `ITraversalProbe`, `ILocomotionHost`, `ISkillModifierSink`).
  All eight now have an Unreal implementation and **none is stubbed**.
  - `Source/InsimulRuntime/Portable/InsimulMechanicContracts.h` is the std-only
    mirror of the interfaces and their payloads; `InsimulMechanicHosts.{h,cpp}` holds
    the fallback core documents for every empty slot (absent or unanswering probe
    reads as clear / no reading / passable / arrived) implemented **once**, plus
    `ConsequenceOf()` — what leaving each hook empty costs, as data a report prints
    rather than a comment.
  - `templates/source/mechanics/` is the engine half: an actor/location registry,
    a combat roster (`ICombatSystem` + `ICombatStatSink`), three geometry probes over
    `LineTraceSingleByChannel` and `UNavigationSystemV1`, a locomotion host over
    `AAIController::MoveToLocation`, a skill-modifier sink, a survival host over the
    template's own `USurvivalSystem`, and `UInsimulMechanicHostBinder` — the one
    reflected surface a creator wires.
  - **Measured, not assumed: no mechanic is reachable in any build that ships
    today.** `insimul::FInsimulMechanicSurface` asks the library `core.methods` and
    reports per module; the shipped `libinsimulcore` answers with five methods and no
    mechanic row, so every module is `BridgeHasNoRow` and the host half is implemented
    and **inert**. The boot log says exactly that rather than letting a combat host
    imply combat is wired. `RUNTIME_CORE_ADOPTION.md` §12 is the write-up, including
    the three findings that must be answered before the rows can be written at all.
- **`check:mechanics` — the mirror's drift guard (US-1 of 146).**
  `tools/verify-mechanics/check-mechanics.mjs`, now part of `npm run check`, pins
  every band-120 module and every interface member against
  `MODULE_HOSTS.json` (core's commit plus the sha256 of the three files it was derived
  from), and requires each interface to be implemented or listed in `stubbed` with a
  stated consequence. `--core <packages/core>` re-derives it all from core's
  TypeScript; four negative controls prove each check can fail. A port of Unity's
  script of the same name — one mechanism across the engine repos.
- **`ctest mechanic_hosts` / `mechanic_bridge` (US-1 of 146).** The portable half is
  EXECUTED: 106 checks over the mirror, the fallbacks and all four surface states with
  no engine and no native library, and 120 with the real `libinsimulcore` — the
  bridge leg pins the whole method list, so a mechanic row **arriving** fails as
  loudly as one disappearing.
- **`npm run check:host:binaries` — this harness's `--require-binaries`.**
  `-DINSIMUL_REQUIRE_BINARIES=ON` turns the configure-time "no libinsimulcore found"
  warning into a hard failure. `check:full` is now `check` + that. `check:host` stays
  permissive so a standalone clone still runs the toolchain-free gates.

### Changed
- `USurvivalSystem` gained `HasNeed`, `GetNeedIds`, `GetNeedMax`, `GetNeedDecayRate`,
  `IsNeedCritical`, `IsNeedWarning`, `SetEnabled`/`IsEnabled` and a `bEnabled` gate on
  `Update` — `getNeed`, `getAllNeeds`, `setEnabled` and `isEnabled` are members of the
  interface core's `stamina` module declares and none of them had a reader here.
  A disabled clock keeps its values, so re-enabling resumes rather than restarts.
- `UInsimulRadiantSourceShell` exposes a non-reflected `GetCoreCaller()`, so another
  adopted slice shares the **one** libinsimulcore runtime instead of starting a second
  QuickJS. `Source/InsimulRuntime/Portable/` is now on the module's public include
  path: the mechanic contracts are a boundary the exported game implements, and that
  cannot be a private header.

- **Quest-parity diff — `quest_parity` / `quest_parity_core` (US-3 of 99).** Two
  new `tools/verify-unreal` ctest targets that run the shared quest corpus
  (`conformance/quests/{hydration,radiant}-cases.json`, 4 + 3 cases) through
  **three legs** — the committed golden, this plugin's hand-ported
  `FInsimulQuestSystem`, and `@insimul/core` through `libinsimulcore` — reduced
  to a canonical string by the same C++ serializer so a surviving difference is
  semantic rather than a formatting artifact. Every case is classified
  AGREE / SHAPE / FIX / REGRESSION / UNGOLDENED; the last two fail the build.
  **Result: 7 AGREE, 0 of everything else** — the hand-port and core agree
  completely on the surface the corpus covers. The classifier is self-tested over
  five synthetic triples (5/5 verdicts reachable) so that result is a finding
  rather than the only thing the gate can say. `quest_parity` runs the corpus and
  hand-port legs plus the self-test with no native library, so a standalone clone
  still gates something.
  - `quest.hydrate` / `quest.radiantTick` are **comparison surfaces, not adopted
    ones**: nothing in `Source/` calls them, and `FInsimulQuestSystem` remains
    what ships. Agreement is the evidence a future retirement would need, not the
    retirement.
- **Vendored conformance corpus re-vendored, and now guarded (US-3 of 99).** The
  corpus described itself as a byte-for-byte mirror of
  `packages/core/conformance/` and was not one — measured at **41 of core's 76**
  Prolog cases, missing the entire KINP pack, with a pre-KINP `gameplay.json`,
  and with `content/*` claiming to mirror a core directory that does not exist.
  Re-vendored to **34 files / 76 Prolog cases**, byte-identical to the source and
  to the set the Godot adapter carries (same files, same 34 hashes).
  - `tools/vendor-conformance.mjs` (`npm run vendor:conformance`,
    `npm run check:corpus`) — a deliberate port of Godot's script of the same
    name rather than a second mechanism. `--check` verifies every mirrored file
    against the sha256 in the new `conformance/VENDORED.json`, counts the Prolog
    cases, and rejects any file that is neither mirrored nor *declared local*; it
    needs no core checkout, so it runs as the **`corpus_manifest`** ctest target.
    `--core` does the real byte-for-byte diff against a core checkout. The drift
    happened because nothing ever ran that diff.
  - Newly mirrored: `prolog/{identity,equivalence,worlds}.json` (the KINP pack,
    34 cases), `predicate-schema-hash.json`, `content-library/*.json`.
  - `conformance/content/*` is now **declared local** and its README says so.
    Core's shared content-library golden (`content-library/*.json`) is a
    different, current shape and now sits beside it; reconciling
    `FInsimulContentLibrary` onto it is content-portability work, not runtime-core
    adoption, and was not attempted.
- **`RUNTIME_CORE_ADOPTION.md` §10 — the US-3 parity report.** What the adopted
  slice proves (11/11 radiant vectors, unreduced), what the corpus looked like
  before and after, both implementation diffs with their classifications, the
  retain/remove decision with reasons, and the honest gaps. Nothing was removed:
  `ERadiantSource::None` and `Portable/InsimulQuestSystem.cpp` are both retained
  explicitly — neither is superseded, and `None` is now load-bearing evidence
  that the adoption is a strict capability gain.
- **Radiant quest *generation* — the first adopted slice of `@insimul/core`
  (US-2 of 99).** This plugin now *calls* core's generator instead of shipping
  none: `UInsimulRadiantSourceShell::GenerateQuests()` turns radiant templates
  plus current world facts into new quests, deterministically, from a seed. It is
  a capability this engine did not have — not a replacement for anything. (Not to
  be confused with the radiant *tick*, `FInsimulQuestSystem::RadiantTick`, which
  *offers* already-authored radiant quests and is unchanged.)
  - `Source/ThirdParty/InsimulCoreLibrary/` — a new `External` module publishing
    `libinsimulcore` (the C ABI over `@insimul/core`), shaped exactly like the
    existing `InsimulLibrary` module over libinsimul. `include/insimulcore.h` is
    a byte-for-byte copy of the shipping header; `VERSION` records the QuickJS
    pin and the **core commit compiled into the binary**. Desktop only — on a
    platform with no build it defines `INSIMUL_WITH_CORE=0` and the plugin
    compiles and runs without the bridge.
  - `Private/Core/InsimulCoreBridge.{h,cpp}` — a UE-free RAII handle over that
    ABI, mirroring `insimul::InsimulKB` over libinsimul. The **only** file in the
    plugin that includes `insimulcore.h`; it marshals bytes and nothing else.
  - `Portable/InsimulCoreCaller.h` (`ICoreCaller`, the JSON-in/JSON-out transport
    seam, reusable by every later slice) and `Portable/InsimulRadiantSource.{h,cpp}`
    (`FRadiantSource`) — the **single translation site** where engine types
    become core's, `std`-only so it is host-testable under plain `clang++`.
  - `Public/InsimulRadiantSourceShell.h` — the thin, game-thread-affine
    `UCLASS`/Blueprint surface (`FInsimulGeneratedQuest`), pimpl'd so neither the
    C ABI nor the portable headers leak downstream.
  - Which implementation answers is **selectable**: `EInsimulRadiantSource::Core`
    (through the bridge) or `None` (this plugin's pre-adoption behaviour, and the
    fallback wherever `libinsimulcore` is absent).
- **Radiant conformance gate — three new `tools/verify-unreal` ctest targets.**
  All drive the shared corpus `conformance/radiant/*.json` (5 files / 11 cases —
  the same unreduced vectors `packages/core` and the Godot adapter run) through
  the adapter, asserting the executed-case count is non-zero and ≥ 11, that all
  five areas are present, that case names are unique, and that the bundle still
  exposes `radiant.generate`.
  - `radiant_bridge` — the **full stack**: core's real TypeScript in QuickJS over
    the natively linked libinsimul. **11/11 pass.** The first target in this
    harness to link a native library at all; built when an `insimul-native`
    checkout is discoverable (`-DINSIMUL_NATIVE_DIR=…`), and its absence is a
    loud configure-time warning rather than a silent skip.
  - `radiant_source` — the translation site over a recording stub, so it runs in
    a standalone clone with no native library. Asserts the request document
    byte-for-byte against an independently built expectation and answers in a
    deliberately perturbed wire shape.
  - `radiant_source_none` — the pre-adoption leg, classified rather than failed:
    **4 AGREE / 7 GAIN / 0 REGRESSION**, matching the Godot adapter exactly.
- **`RUNTIME_CORE_ADOPTION.md` — the shared-runtime-core adoption plan (US-1 of
  99).** A design document, no code: it reads `@insimul/core`'s runtime contract
  and restates it against this plugin's own types and lifecycle
  (`FInsimulRuntimeContext::Boot()`, `UInsimulPrologSubsystem`, the portable
  `std`-only cores), maps all five host hooks to what exists here / what must be
  written / what has **no** counterpart, recommends adopt-or-keep for every
  system this repo implements that core also implements (with the behavioural
  differences called out), confirms the C-ABI language boundary already decided
  by tasklist 100 and costs the Unreal-specific parts of binding
  `libinsimulcore`, and chooses **radiant quest generation** as the first slice.
  Also records what measurement contradicted: the vendored Prolog corpus is 41 of
  core's 76 cases with a stale `gameplay.json`, `conformance/radiant/`'s 11
  vectors have no reader, `VERIFICATION.md`'s `npm run engines:*` gates no longer
  exist (13 test files / 4,738 lines have no build wiring here), and the
  libinsimul ABI polarity trap that bit the Godot plugin does **not** exist in
  this repo — the header is byte-identical to the shipping one and every call
  site tests the correct polarity.
- `VERSION` file alongside `Insimul.uplugin`, kept in sync with the manifest's
  `VersionName` and `VERSIONS.json` by `npm run engines:manifests`.
- FAB/Marketplace release dry-run (`scripts/release/build-plugin-zip.mjs`): stages
  the plugin (`.uplugin` + `Source/`, no `templates/` or build intermediates) and
  builds + validates a `dist/Insimul-<version>.zip`.
- **`InsimulEditor` module (`Type: Editor`) + Asset Binding Layer (US-XG1).** New
  `Source/InsimulEditor` module, now declared in `Insimul.uplugin`. It ships the
  `UInsimulBindingTable` UDataAsset (archetype-key → soft asset reference + transform
  fixups + sockets; resolution order Project → Pack → Placeholder) as a thin adapter
  over a UE-free resolver core (`Portable/InsimulBindingResolver.{h,cpp}`) with
  wildcard/descendant matching, most-specific-within-source + first-matching-source
  selection, sorted/canonical portable-pack import/export, and an unbound report. The
  core host-tests on the bare clang toolchain against the same cross-engine fixtures
  (`resolver-matrix.json` + Unity's `unity-fixture-pack.json`) as the Unity/Godot legs
  (`npm run engines:unreal:binding`; wired into `npm run engines:check`).

- **Terrain + settlement generation pipeline (US-XG2).** New UE-free placement
  core (`Source/InsimulEditor/Portable/InsimulScenePlacement.{h,cpp}`): given a
  parsed World IR + the binding resolver it computes the deterministic, canonically
  ordered PLACEMENT MANIFEST (terrain chunks from the IR heightmap, roads at road-
  point centroids, buildings on grid-snapped + zone-scaled footprints, interiors,
  props, and a nav bake root) — bilinear heightmap sampling, 1-unit footprint grid,
  zone-role scaling, `0.001` coordinate quantization, all host-tested. The archetype
  keys + numbers mirror the Unity leg exactly, so the host gate
  (`npm run engines:unreal:scene`) asserts every node reproduces Unity's committed
  golden manifest (byte-copied fixtures) — the cross-engine determinism contract.
  The UE-coupled materializer (`InsimulSceneGenerator` entry point + pipeline stages,
  `InsimulEntityIdComponent` identity stamp + `Insimul.Generated` tag) is syntax-
  gated; `Landscape`/`LandscapeEditor`/`Foliage`/`NavigationSystem`/`PCG` deps
  declared in `InsimulEditor.Build.cs`. Conventions in `docs/scene-generation.md`.
  Wired into `npm run engines:check`.
- **PCG vegetation + placeholder pack (US-XG3).** New UE-free placeholder pack
  recipe (`Source/InsimulEditor/Portable/InsimulPlaceholderPack.{h,cpp}`): an
  ordinally sorted list of primitive+taxonomy-color specs (five base-node wildcards
  `building.*`/`npc.*`/`item.*`/`prop.*`/`terrain.*` + nicer sub-node defaults)
  projected into a Priority-0 Placeholder-tier `FBindingSource`. Host coverage gate
  (`npm run engines:unreal:placeholder`) proves every archetype key in the shared
  `golden-world-archetypes.json` (byte-identical to Unity's) resolves with zero
  unbound. The UE-coupled generator (`InsimulPlaceholderPackGenerator`) walks the
  same specs to materialize primitive meshes + a pre-wired `UInsimulBindingTable`.
  PCG vegetation scatter: portable graph descriptor
  (`data/pcg/insimul-vegetation-graph.json`) + the syntax-gated
  `InsimulPcgVegetation` stage that feeds graph parameters from the IR biome/density
  slice. Licensing note `data/placeholders/LICENSE.md` (all original / CC0).
  Conventions in `docs/scene-generation.md`; wired into `npm run engines:check`.
- **Conservative re-import diff + Binding Editor (US-XG4).** Two new UE-free cores.
  The re-import policy (`Source/InsimulEditor/Portable/InsimulReimportDiff.{h,cpp}`)
  matches existing actors to the fresh placement manifest by `InsimulEntityId` and
  classifies each into added / updated / unchanged / skipped(hand edit) /
  deprecated: only `generated=true` actors are ever refreshed, hand edits are
  preserved verbatim, dropped generated actors are reparented under `Deprecated/`
  (never deleted), and the dry-run report serializes byte-identical to the
  cross-engine golden (`Tests/fixtures/reimport/golden-diff-report.json`,
  byte-identical to Unity's + Godot's). The Binding Editor view-model
  (`Portable/InsimulBindingEditorModel.{h,cpp}`) turns the world's archetype keys
  into a taxonomy tree annotated with bound / placeholder / unbound status,
  partitions bound/unbound keys, and ranks name/tag asset suggestions — the same
  cases the Unity leg proves. UE-coupled seams (syntax-gated only): the
  `UInsimulReimport` driver (`DryRun`/`Apply`, one Undo transaction, live-tree
  mutator) and the `UInsimulBindingEditorWidget` Editor Utility Widget (bind /
  bind-descendants / pack import-export / suggestions over the Asset Registry).
  Host gates `npm run engines:unreal:reimport` (19/19) +
  `engines:unreal:binding-editor` (17/17), both wired into
  `npm run engines:check`. Policy + editor loop documented in `docs/reimport.md`;
  human checklist in `VERIFICATION.md` (US-XG4). `InsimulEditor.Build.cs` gains
  `AssetRegistry` + `Blutility`/`UMG`/`UMGEditor` deps for the utility widget.
- **`VENDORED.json` guards a floor, not just hashes (US-2 of 146).** The manifest gained
  `cases` (exact per-area counts, re-derived on `--check`) and `caseFloor` (the largest
  count ever vendored, which never drops on its own). Hashes cannot see a corpus
  *shrink* — a trimmed `cases` array re-hashes clean — so a re-vendor that would lower a
  floor now fails and names the area; `--allow-corpus-shrink` is the explicit act of
  accepting one. `--core` re-vendoring also prunes mirrors core has dropped. Mechanism,
  flag names and manifest keys ported unchanged from Unity (tasklist 145 US-2).
- **Corpora core ships and this repo deliberately does not mirror are now declared.**
  `editor/`, `generation/`, `ai/`, `map/` and `grounding/` each carry a stated reason and
  are printed with a file count on every `--core` run, so a corpus for an unadopted
  surface stops reading as drift.
- `npm run check` gained `check:mechanic-corpora`; `check:host:binaries` gained the
  `prolog_corpus` leg (15 ctest legs, up from 14).

### Fixed
- **Re-measured and documented: the `libinsimul` KB-lifetime defects are gone.** Unity's
  probe spawns a fresh process per Prolog case because the library it measured (git
  `f1548a4`) aborted on the second `insimul_kb_destroy` in a process and broke
  library-module loading after ~64 leaked KBs. Against the shipping library (git
  `e019244`, trealla v2.106.1): 200 create/consult/query/destroy cycles in one process,
  clean. `prolog_corpus` therefore runs all 255 cases in-process — and is now the
  standing regression test for both defects. `RUNTIME_CORE_ADOPTION.md` §13.2.

### Notes
- `EngineVersion` is intentionally left unset: this is a source plugin that builds
  against the installed engine. It is pinned per target at release time (FAB /
  Marketplace submission).

## [1.0.0]

### Added
- Initial plugin: `InsimulRuntime` module — streaming NPC conversations, TTS/STT,
  quest management, and crowd character integration.
