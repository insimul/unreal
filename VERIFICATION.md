# Insimul Unreal plugin — human verification checklist

The native-Prolog stack has two verification tiers:

1. **Automated, in this harness** — the plain-C++ `insimul::InsimulKB` core is
   conformance-tested on the host clang toolchain against a real `libinsimul`:
   ctest **`prolog_corpus`** drives all **255** vendored `conformance/prolog` cases
   (including the 125 band-120 `mechanic-*` cases) through `InsimulKB` and diffs
   core's golden solution sets. It is one of the four legs that need the native
   library, so run `npm run check:host:binaries` from `tools/` — plain
   `check:host` drops them with a configure-time warning. Every UE `.h/.cpp` also
   passes the structural syntax gate (`npm run engines:check`).

   Until tasklist 146 US-2 this paragraph promised something no gate did:
   `conformance/prolog/` was hash-checked and executed by nothing (see
   `RUNTIME_CORE_ADOPTION.md` §6.5 and §13.2). It is true now.
2. **Human, in a real UE editor** — the UE-coupled layer (subsystems, actors,
   widgets) cannot be built in this harness (no Unreal SDK / UBT), so it is
   syntax-gated only and verified by a person following the checklists below.
   `autoMerge` is **off** for this branch precisely so a human runs these first.

Perform the relevant checklist in an editor with `libinsimul` staged into
`Source/ThirdParty/InsimulLibrary/lib/<Platform>/` (see that module's
`lib/README.md`).

---

## US-XP3 — `UInsimulPrologSubsystem` in-editor smoke

**Goal:** confirm the real Prolog engine is wired through the GameInstance
subsystem and callable from Blueprint + C++.

### Setup

- [ ] `libinsimul` static/shared lib for your platform is present under
      `Source/ThirdParty/InsimulLibrary/lib/<Mac|Linux|Win64>/`.
- [ ] The project compiles with the `InsimulRuntime` module enabled (Development
      Editor target).
- [ ] Play-In-Editor (PIE) starts without a fatal log from `LogInsimulProlog`.

### Lifecycle

- [ ] On PIE start, the log shows `InsimulPrologSubsystem initialized (insimul
      <version> …)` — confirms the KB was created and `GetPrologVersion()`
      returns a real version string.
- [ ] `IsPrologReady()` returns `true` during play.
- [ ] On PIE stop, `Deinitialize` runs with no crash / assert (KB released
      cleanly on the game thread).

### Blueprint surface (in a Level Blueprint or test widget)

Get the subsystem via **Get Game Instance Subsystem → InsimulPrologSubsystem**.

- [ ] `ConsultWorldData("parent(tom, bob). parent(bob, ann).")` returns `true`.
- [ ] `ConsultWorldData("parent(tom, .")` (malformed) returns `false` and
      `GetLastError()` is non-empty.
- [ ] `AssertFact("quest(find_sword, active)")` returns `true`.
- [ ] `QueryFirst("parent(tom, X)", …)` returns `true`; `GetBoundValue(Binding,
      "X", …)` yields a value whose `DisplayString` is `bob`.
- [ ] `QueryAll("parent(P, C)", …)` returns `true` with **2** solutions
      (`tom/bob`, `bob/ann`) — order-independent.
- [ ] `QueryFirst("parent(nobody, X)", …)` returns `false` and `GetLastError()`
      is **empty** (a no-solution, not an error).
- [ ] `RetractFact("quest(find_sword, active)")` returns `true`; a second
      `RetractFact` of the same clause returns `false`.

### Snapshot / restore round-trip

- [ ] After asserting a few facts, `SnapshotToString()` returns non-empty Prolog
      text.
- [ ] Clearing/reloading (or restoring into a fresh session) via
      `RestoreFromString(image)` returns `true`, and the previously-asserted facts
      re-query successfully.
- [ ] (Contract note) A KB whose clauses rely on a **custom operator** declared
      with `:- op/3` will NOT restore into a fresh KB — snapshots serialize
      clauses only. Round-trip plain clauses.

### Thread affinity

- [ ] Calling any subsystem method from a background task (e.g. inside an
      `AsyncTask(ENamedThreads::AnyThread, …)`) logs an error from
      `LogInsimulProlog` and returns `false`/`""` **without** corrupting the KB —
      subsequent game-thread calls still work.

---

## US-XP4 — `UPrologEngine` adapter (game-template smoke)

**Goal:** confirm the exported game template's `UPrologEngine` — now a thin
adapter over `UInsimulPrologSubsystem` (the substring stub is retired) — drives
the **real** engine end-to-end. See `templates/MIGRATION.md` for the behavior
deltas this checklist exercises.

### Setup

- [ ] Same prerequisites as US-XP3 (libinsimul staged, `InsimulRuntime` enabled).
- [ ] The exported game module (`InsimulExport`) compiles with both
      `UPrologEngine` and `UInsimulPrologSubsystem` present.
- [ ] PIE starts with no fatal `LogTemp` / `LogInsimulProlog` error.

### Lifecycle & wiring

- [ ] On PIE start, `UPrologEngine::LoadFromIR(worldJson)` logs
      `PrologEngine loaded via real engine (…)` and **does not** log
      `UInsimulPrologSubsystem unavailable`.
- [ ] `LoadItemReasoningRules()` and `LoadHelperPredicates()` log success (rules
      consulted, no `GetLastError` warning).

### Real unification (the migration's whole point)

- [ ] Assert `person(alice)` and a rule via `LoadFromIR` world data, then a query
      method that depends on a **rule** (e.g. an `is_a/2` chain, or
      `IsQuestComplete` whose `quest_complete/2` is rule-defined) returns `true`
      where the old substring stub returned `false`.
- [ ] `EvaluateCondition("2 >= 1")` returns `true` (arithmetic — impossible under
      the stub).
- [ ] `EvaluateCondition("nonexistent_pred(x)")` returns `false`.

### Fact management & queries

- [ ] After `InitializeInventory` with an item of quantity 3, `Query("has_item(player, X, N)")`
      returns one solution binding `N=3`.
- [ ] `WhoShouldTalkTo(npc)` / `GetPreferredTopics(npc)` return the expected
      targets when the world KB defines `should_talk_to/2` / `prefers_topic/2`.
- [ ] An `EventBus` item-collect event updates `has_item/3` (collect twice →
      quantity accumulates; drop below 1 → `has(player, item)` no longer holds).

### Save round-trip via Snapshot/Restore

- [ ] Assert some gameplay facts, call `SnapshotToString()` → non-empty image;
      store it as `GameSaveState.prologFacts`.
- [ ] In a fresh session, `LoadFromIR(...)` then `RestoreFromString(image)`
      returns `true` and the previously-asserted gameplay facts re-query
      successfully.
- [ ] Alternatively, `GetPlayerFacts()` → save array → `RestorePlayerFacts(array)`
      after `LoadFromIR` re-establishes the same facts (item quantities included).

### Graceful degradation

- [ ] With `libinsimul` absent (subsystem unavailable), `LoadFromIR` logs the
      "unavailable" error and adapter mutations no-op / queries return
      empty-or-default **without** crashing PIE.
# Insimul Unreal Runtime — Human Verification Checklist (US-XC4)

The portable runtime core (`FInsimul*`) is proven cross-runtime by the host
harness and the TS drift guards; those run without an engine. This document is
the **human pass**: the things only a real Unreal Editor + build can confirm —
the syntax gates on the UE-coupled seam and the full gameplay loop end-to-end.

Everything in §1–§2 has an automated proxy that is already green (see
[MIGRATION.md](./MIGRATION.md) and the harness commands below); the human is
confirming an already-proven sequence, not debugging it.

## 0. Automated pre-checks (run before the human pass)

These must all be green first — they gate the portable semantics the loop below
exercises.

- [ ] Host harness: `cmake -S tools/verify-unreal -B build && cmake --build build && ctest --test-dir build --output-on-failure`
      → 12/12: `world_source`, `save_system`, `quest_system`, `bootstrap`,
      `content_library`, `content_roundtrip`; the radiant-adoption gate
      `radiant_source`, `radiant_source_none`, **`radiant_bridge`**; the
      quest-parity diff `quest_parity`, **`quest_parity_core`**; and the vendored
      corpus drift guard `corpus_manifest`.
      **`radiant_bridge` and `quest_parity_core` need a native library** — they
      run the shared vectors through the real `@insimul/core` bundle, and are
      built only when an `insimul-native` checkout with `build/libinsimulcore.a`
      + `build/libinsimul.a` is discoverable (a sibling `native/`, or
      `-DINSIMUL_NATIVE_DIR=<dir>`). Without one, configure prints a warning and
      you get **10/12** — the full stack was NOT exercised, so do not record this
      checkbox as green.
      `corpus_manifest` additionally needs `node` on PATH; if it is missing,
      configure says so and the target is not registered.
- [ ] Vendored corpus is byte-identical to its source (the check `corpus_manifest`
      cannot do on its own): `node tools/vendor-conformance.mjs --check --core <packages/core>`
      → "byte-identical". Without `--core` the guard only verifies the corpus
      against its own manifest, which cannot detect that *core itself* moved.
- [ ] Structural syntax gate: `node tools/verify-unreal/check.mjs` → all
      git-tracked `.h`/`.cpp` under this repo structurally sound. It reads
      `git ls-files`, so a new file must be **staged** before it is scanned.
- [ ] Root type-check: `npm run check` → exit 0.
- [ ] Root tests: `npm test` → all green (includes the save-integrity + quest
      drift guards that pin the C++ output to the TS authority).
- [ ] Save portability cross-check: `npx vite-node packages/unreal/tools/cross-check/verify-save-integrity.ts` → exit 0.

## 1. Build / syntax gates (Unreal Editor required)

The UE-coupled files are syntax-gated (`#if WITH_ENGINE`, UHT `GENERATED_BODY`)
and are NOT compiled by the host harness. Confirm they build under UBT:

- [ ] Generate project files and build the `InsimulExport` editor target
      (`templates/project/`) — see `templates/project/INTEGRATION_GUIDE.md`.
- [ ] The build compiles the new engine seam cleanly:
  - [ ] `UInsimulRuntimeSubsystem` (`Public/InsimulRuntimeSubsystem.h`, `Private/InsimulRuntimeSubsystem.cpp`) — the startup orchestrator.
  - [ ] `FInsimulSaveSystemShell`, `FInsimulQuestSystemShell`, `InsimulWorldBoundary` (the US-XC1..XC3 seams the subsystem drives).
  - [ ] The re-pointed consumers: `AInsimulLevelScriptActor`, `AInsimulSpawner`, `UInsimulCrowdIntegration`.
- [ ] No UHT errors on the `USTRUCT`/`UCLASS`/`UFUNCTION` reflection surface.

## 2. Full gameplay loop (Play-In-Editor)

Drive the whole loop the way the automated `bootstrap` host test does, but in the
live engine. Use the golden world (`Content/Data/` export, or a bundled
`worldSnapshot`).

### New game on the golden world

- [ ] Start Play with **no existing save slot**. `UInsimulRuntimeSubsystem::Boot`
      logs `Insimul runtime booted (new game): N characters, M quests`.
- [ ] NPCs spawn from the **world source**, not a hardcoded/server list:
      `AInsimulSpawner` logs `populated N characters from the world source`
      (and `AInsimulLevelScriptActor` uses the world-source ids when present).
- [ ] The spawned character ids/names match the golden world's characters.

### Radiant quest

- [ ] A radiant-tagged quest is offered deterministically (the same offering the
      `radiant-cases.json` corpus + host test pin). Re-running Play from the same
      state offers the **same** quest(s) in the same order (RNG-free).

### Objective

- [ ] Complete an objective's trigger in-world (e.g. talk to the target NPC).
      The quest system shell broadcasts `OnObjectiveCompleted`, then
      `OnQuestCompleted` once all objectives are satisfied — and a
      `quest_complete(<id>)` fact is asserted into the KB.

### Save

- [ ] Save to a slot (`UInsimulRuntimeSubsystem::SaveToSlot`). A canonical,
      integrity-stamped envelope file is written under the project SaveGames dir.
- [ ] (Optional) Copy that slot file to another device/runtime and confirm it
      still verifies — the cross-runtime portability property (§5.2 B2).

### Reload

- [ ] Stop and re-enter Play. `Boot` logs `resumed save`; `DidResumeSave()` is
      true.
- [ ] Quest + radiant progress is intact: the completed quest stays completed,
      the offered radiant quest is still offered (KB facts round-tripped).
- [ ] The `worldSnapshot` hash is unchanged across the save/reload boundary (a
      `currentState`-only mutation must never perturb the world hash) — the host
      `bootstrap` test asserts this; confirm no world-drift warnings in the log.

## 3. Deliberate deltas vs the Babylon/Unity behaviour reference

**Target: zero.** The portable core ports the semantics authority
(`packages/core`, TypeScript; Unity is the reference implementation) byte-for-byte
where it matters, pinned by the shared corpora + drift guards. Known,
**intentional** deltas at the UE seam (none change observable save/quest/world
semantics):

| Delta | Where | Why it is not a semantic difference |
| ----- | ----- | ----------------------------------- |
| Slot timestamps (`createdAt`/`lastSavedAt`/envelope `exportedAt`) use `FDateTime::UtcNow()` at write time. | `FInsimulSaveSystemShell` | Timestamps are identity metadata, not part of the integrity-hashed contract dimension the corpora pin; the golden envelope uses a fixed timestamp so the byte-pin holds. |
| No native Prolog resolution engine yet (KB is a ground-fact store with Assert/Has). | `FInsimulKB` | Sufficient for query-driven completion; `unreal-native-prolog` plugs in behind the same Assert/Has shape without changing the fact contract. |
| Corrupt/incompatible save slot falls back to a new game instead of aborting boot. | `FInsimulRuntimeContext::Boot` | Resilience choice, not a semantic difference; a valid save always resumes. Matches the "never brick startup" intent. |

Any delta discovered during the human pass that is **not** listed here is a bug —
file it against this story rather than accepting it.

---

## US-XG4 — Scene generation ▸ re-import ▸ Binding Editor (full editor loop)

**Goal:** confirm the interactive Editor-module pipeline (US-XG1..4) works
end-to-end in a real UE editor — import the golden world, bind a custom mesh,
regenerate, and verify placement + the conservative re-import diff. The placement
math + resolver + placeholder coverage + re-import policy + binding-editor logic
all host-test green (`npm run engines:check`); this is the human pass over the
UE-coupled seam (the `UInsimul*` widgets / generators / drivers that UBT compiles
but the harness only syntax-gates).

### 0. Automated pre-checks (run before the human pass)

- [ ] `npm run engines:check` → green. Includes the five unreal host gates:
      binding resolver (21/21), scene placement (13/13), placeholder coverage
      (12/12), **re-import diff (19/19)**, **binding-editor view-model (17/17)**,
      plus the C++ structural syntax gate over every `.h/.cpp`.
- [ ] The InsimulEditor module builds under UBT (Development Editor target) with
      no UHT errors on the `USTRUCT`/`UCLASS`/`UFUNCTION` reflection surface —
      `UInsimulSceneGenerator`, `UInsimulReimport`, `UInsimulBindingEditorWidget`,
      `UInsimulEntityIdComponent`, `UInsimulBindingTable`,
      `UInsimulPlaceholderPackGenerator`, `UInsimulPcgVegetation`.

### 1. Import the golden world + placeholder coverage

- [ ] Generate the placeholder pack (**Insimul ▸ Generate Placeholder Pack**) →
      a `UInsimulBindingTable` (SourceKind = Placeholder) + primitive meshes are
      written under `Content/Insimul/Placeholders/`.
- [ ] Run **Insimul ▸ Generate Scene From World IR** on the golden world export.
      The scene populates: terrain, roads, buildings (+ interiors), props, a
      NavMesh bounds volume. Every generated actor carries a
      `UInsimulEntityIdComponent` (+ the `Insimul.Generated` tag).
- [ ] With ONLY the placeholder pack bound, **every** archetype the golden IR uses
      resolves (nothing renders as an unbound/empty actor) — the coverage rule.

### 2. Binding Editor: bind a custom mesh

Open the Binding Editor utility widget (a Blueprint child of
`UInsimulBindingEditorWidget`), set `WorldArchetypes` to the golden world's keys.

- [ ] `BuildRows()` shows the taxonomy-grouped archetype list; used-archetype
      leaves show **Placeholder** status (bound only via the placeholder tier).
- [ ] `SuggestBindings("building.commercial.bakery")` ranks project assets whose
      name/path/tags contain the most dot-segments first.
- [ ] **Bind** a custom StaticMesh/Blueprint to a building archetype in the
      project table. That row now shows **Bound** (project tier wins over
      placeholder); `BoundKeys()` includes it, `UnboundKeys()` does not.
- [ ] **Bind-descendants** on a parent key (e.g. `building`) — every `building.*`
      archetype with no more-specific entry now resolves to it.
- [ ] **Export Pack** → portable `insimul-binding-pack` JSON; re-**Import Pack**
      of that JSON reproduces the same table (keys + fixups preserved).

### 3. Regenerate + verify placement

- [ ] Re-run **Generate Scene From World IR**. The building you bound now spawns
      your custom asset at the SAME lot footprint / transform the placeholder used
      — placement is a pure function of the IR, independent of the bound asset.
- [ ] The spawned transform matches the host golden manifest coordinates (the
      cross-engine determinism the `run-scene-tests.sh` gate pins).

### 4. Conservative re-import diff (the payoff)

- [ ] Hand-edit the scene: move a generated building, and add a hand-placed prop
      (no `UInsimulEntityIdComponent`). Mark one generated actor `bGenerated=false`
      (an adopted override).
- [ ] Run **Insimul ▸ Re-import World IR (Diff)** (dry run) on a *changed* IR
      export. The summary logs canonical counts: added / updated / unchanged /
      skipped / deprecated. Confirm the JSON matches
      `UInsimulReimport::DryRun(...).ReportJson`.
- [ ] **Apply** the re-import (one Undo group):
  - [ ] Moved generated actors are snapped back to the fresh IR transform
        (**Updated**).
  - [ ] The hand-placed prop is untouched, and the `bGenerated=false` override is
        **Skipped** (not updated, not deprecated) — hand edits survive.
  - [ ] A generated actor the new IR dropped is reparented under the `Deprecated/`
        folder, **not deleted**.
  - [ ] Brand-new IR ids are materialized + stamped (**Added**).
- [ ] Undo restores the pre-apply state in one step.

### Deltas vs Unity/Godot

**Target: zero on the numeric + policy contract.** The scene placement numbers,
the placeholder coverage set, the re-import classification, and the binding-editor
decisions are all pinned byte-for-byte against the shared cross-engine fixtures
(the Unity manifests/goldens are copied verbatim). Only the engine-specific
asset-ref strings (`placeholder:building` vs a real `/Game/...` path) differ, by
design. Any classification / placement / coverage difference discovered in the
human pass is a bug — file it against US-XG4.

---

## US-XE2 — World Browser: connect ▸ browse ▸ import (Unreal editor + backend required)

The World Browser tab's parsing, list/detail/selection reducer, compatibility
badge (imported snapshot version vs the world's current snapshot), open-in-web
link, and Import/Sync orchestration all live in the Unreal-Engine-free
`insimul::FWorldBrowserModel`, host-tested headless over a routing transport + a
fake registry/pipeline (`test_world_browser.cpp`, `npm run
engines:unreal:world-browser`). This is the **human pass** for the two UE-coupled
seams only a real editor + backend can exercise: the `FInsimulEditorHttpTransport`
HTTP path (US-XE1) and the `FInsimulSceneImportPipeline` bridge into the
US-XG2/US-XG4 scene generation + re-import diff, with the imported-version record
persisted per-user in `FInsimulImportedWorldRegistry` (GEditorPerProjectIni —
never a committed asset). A running backend (`UInsimulSettings::ServerURL`) with at
least one world on the account is required.

### 0. Automated pre-checks (run before the human pass)

- [ ] `npm run engines:unreal:world-browser` — the view-model host gate is green
      (parsing, list load incl. 401 re-auth, detail merge, selection reducer,
      compatibility badge, open-in-web, import wiring, report summary).
- [ ] `npm run engines:check` — the structural syntax gate covers the new UE-coupled
      seams (`FWorldBrowserModel` bridge, imported-world registry, scene-import
      pipeline).

### 1. Connect + browse

- [ ] In **Project Settings ▸ Insimul**, set the server URL and authenticate
      (world API key or user login — US-XE1). Open **Insimul ▸ World Browser** and
      click **Refresh worlds**. The account's worlds list, each showing name, genre
      bundle, `snapshot vN`, and NPC / Settlement / Quest counts.
- [ ] Every never-imported world shows the **Not imported** badge. Select one and
      confirm its detail counts match the world on the web.
- [ ] Click **Open in web** — the browser opens `…/worlds/<id>` for that world.

### 2. Import / Sync through the scene pipeline

- [ ] With a level open, click **Preview Sync (dry run)**. A report line appears
      (`+A / ~U / -D (… unchanged, … hand-edited)`) and the scene is **NOT**
      mutated (no generated actors added/moved).
- [ ] Click **Sync IR now…**, confirm the dialog. The scene's generated actors
      update per the US-XG4 re-import policy (generated nodes added/updated,
      hand-edited nodes untouched, dropped nodes reparented under the **Deprecated**
      group — never deleted), all in one **Undo** group. The badge flips to **Up to
      date (vN)**.
- [ ] With **no level open**, the Import action is disabled and the tab shows
      *"Open a level to import a world into the scene."* (the pipeline's unavailable
      reason).

### 3. Stale-version detection + re-auth

- [ ] Regenerate/advance the world on the backend so its snapshot version bumps,
      **Refresh worlds** again, and confirm the badge now reads **Update available
      (imported vN → vM)** — the stale-version detection driven off the per-user
      imported-version record.
- [ ] Let the token expire (or revoke it) and Refresh: the tab shows the **Session
      expired — re-authenticate** warning (the `NeedsReauth` state), and
      re-authenticating in Project Settings restores the list on the next Refresh.

### Deltas vs Unity/Godot

**Target: zero on the parsing + reducer + badge + import-orchestration contract.**
`FWorldBrowserModel` mirrors `InsimulWorldBrowserModel.cs` and
`world-browser.ts` case-for-case (same `WorldBrowserTests` set). Only the
engine-specific scene mutations (UE actors + `UInsimulEntityIdComponent` vs Unity
GameObjects) differ, by design. Any parsing / selection / badge / count difference
found in the human pass is a bug — file it against US-XE2.

---

## US-XE3 — Generation Console: invoke ▸ track progress ▸ sync (Unreal editor + backend required)

The Generation Console's start-body build, status/event/diff parsing, the job
lifecycle state machine (Idle → Starting → Queued → Running →
Completed/Failed/Canceled), and the leak-free poll teardown all live in the
Unreal-Engine-free `insimul::FGenerationConsoleModel` + `insimul::FJobPoller`,
host-tested headless over a routing transport + a scripted stream + a fake clock
(`test_generation_console.cpp`, `npm run engines:unreal:generation`). This is the
**human pass** for the two UE-coupled seams only a real editor + backend can
exercise: the `FInsimulEditorHttpTransport` HTTP start (US-XE1) and the
`FInsimulHttpJobStream` FTimerManager+FHttpModule poll stream. A running backend
(`UInsimulSettings::ServerURL`, authenticated) with at least one world is required.

### 0. Automated pre-checks (run before the human pass)

- [ ] `npm run engines:unreal:generation` — the view-model host gate is green
      (36 cases: body+parsing, lifecycle over a scripted stream, start guards incl.
      401 re-auth, cancel/dispose/reset, and the FJobPoller no-leaked-ticker
      teardown + maxPolls cap + end-to-end poll→stream→model).
- [ ] `npm run engines:check` — the structural syntax gate covers the new UE-coupled
      seam (`FInsimulTimerScheduler`, `FInsimulHttpJobStream(Factory)`).

### 1. Invoke a generator + live progress

- [ ] Authenticate (US-XE1) and select a world in the World Browser, then open
      **Insimul ▸ Generation Console**. Click **Regenerate settlements** (or
      **Generate characters** / **Generate quests**).
- [ ] The status advances **Queued → Running** with a live progress bar + phase
      label as the poll returns fresh `getGenerationJob` frames (roughly one update
      per second). No editor hitch/freeze while polling (the poll is non-blocking).
- [ ] On completion the status reads **Completed**, the progress bar is full, and a
      results line shows the entity diff (`+A added / ~U updated / -R removed`).

### 2. Sync-now

- [ ] With a level open, the completed job shows **Sync IR now…**. Click it, confirm
      the dialog: the scene's generated actors update through the same US-XG4
      re-import path (World Browser `ApplyImport`), and the World Browser badge for
      that world flips to **Up to date (vN)**.
- [ ] Re-open the Console and confirm a **New job** / reset returns it to Idle.

### 3. Failure, cancel, and domain-reload safety

- [ ] Trigger a failing generation (e.g. an invalid world / server-side error). The
      status reads **Failed** with the server reason; no Sync is offered.
- [ ] Start a job, then **Cancel** mid-run — the status reads **Canceled** and the
      poll loop stops immediately (no further progress updates). The server job may
      still finish; the editor simply stops tracking it.
- [ ] Start a job, then force a **domain reload** (recompile / Live Coding, or enter
      PIE). Confirm no orphaned polling continues after the tab is torn down — no
      `getGenerationJob` requests fire once the Console tab is closed/reloaded, and
      no `Ensure`/access-violation from a timer touching a freed widget.
- [ ] Let the token expire and start a job: the Console surfaces the **Session
      expired — re-authenticate** state (the `NeedsReauth` flag armed by the 401 on
      `startGenerationJob`).

### Deltas vs Unity/Godot

**Target: zero on the parsing + lifecycle + teardown contract.**
`FGenerationConsoleModel` / `FJobPoller` mirror `InsimulGenerationConsoleModel.cs` /
the core `generation-console.ts` + `job-poller.ts` case-for-case (same
`GenerationConsoleTests` set). Only the engine-specific timer (`FTimerManager` vs
`EditorApplication.update` vs `SceneTreeTimer`) and HTTP client differ, by design.
Any status / progress / diff / teardown difference found in the human pass is a bug
— file it against US-XE3.

---

## US-XE4 — Conversation Tester: talk to an NPC in the editor (Unreal editor + backend required)

The whole turn lifecycle (send → stream reply chunks → complete/error), the
character-list + SSE parsing, and the multi-turn transcript over one session id live
in the Unreal-Engine-free `insimul::FConversationTesterModel`, host-tested headless
over a routing transport + a scripted conversation stream
(`test_conversation_tester.cpp`, `npm run engines:unreal:conversation`). This is the
**human pass** for the UE-coupled seam only a real editor + backend can exercise: the
`FInsimulHttpConversationStream` SSE POST driven off the editor tick, and the
tab's pump / dispose wiring. A running backend (`UInsimulSettings::ServerURL`,
authenticated) with the conversation service and at least one world with characters is
required. **Text streaming works in edit mode; audio playback + lip sync do not (Play
mode only) — see the README ▸ Conversation Tester window mode constraint.**

### 0. Automated pre-checks (run before the human pass)

- [ ] `npm run engines:unreal:conversation` — the view-model host gate is green
      (30 cases: parsing, character load incl. 401 re-auth, send guards, the turn
      lifecycle over a scripted stream, multi-turn session sharing, character-switch
      reset, dispose-on-teardown).
- [ ] `npm run engines:check` — the structural syntax gate covers the new UE-coupled
      seam (`FInsimulHttpConversationClient` / `FInsimulHttpConversationStream`).

### 1. Load characters + two-turn conversation

- [ ] With the session authenticated (Project Settings ▸ Insimul), open **Insimul ▸
      Conversation Tester**. Select a world (copy the id from the World Browser) and
      click **Load characters** — the **Character** picker fills with the world's NPCs.
- [ ] Pick a character, type a message, and click **Send**: the **Transcript** shows a
      **You** line immediately, then an **NPC** line as the reply streams in, without
      freezing the editor (the request runs off the editor tick, not blocking).
- [ ] Send a **second** turn to the same character and confirm the reply is coherent
      in context (the two turns share one conversation session) and **both** exchanges
      remain in the transcript (four lines total).

### 2. Audio, switching, and re-auth

- [ ] If the world's characters have TTS enabled, confirm a **TTS audio: N chunk(s)
      returned (not played in edit mode)** line appears — audio is not played here by
      design (drive Play mode with `InsimulConversationComponent` to hear it).
- [ ] Switch to a **different** character in the picker: the transcript clears (a fresh
      conversation), and the next turn starts a **new** session id.
- [ ] Let the token expire / revoke it and **Load characters** again: the load fails and
      the panel shows the **Session expired — re-authenticate** state (the `NeedsReauth`
      flag armed by the 401 on `getWorldDetail`).

### 3. Domain-reload safety

- [ ] Send a turn, then force a **recompile** (Live Coding) or **enter Play mode** while
      it is streaming. Confirm the editor does not throw, **no** `/api/conversation/stream`
      request keeps running after the reload/close, and re-opening the window starts
      clean (the tab's `Dispose()` → cancel-request + drop-late-callback path).

### Deltas vs Unity/Godot

**Target: zero on the parsing + lifecycle + teardown contract.**
`FConversationTesterModel` mirrors `InsimulConversationTesterModel.cs` / the core
`conversation-tester.ts` case-for-case (same `ConversationTesterTests` set). Only the
engine-specific streaming HTTP client (`FHttpModule` vs `UnityWebRequest` vs
`HTTPRequest`) and the edit-mode audio-playback wall (shared across all three) differ,
by design. Any load / send-guard / transcript / teardown difference found in the human
pass is a bug — file it against US-XE4.

## US-XU1 — Default-UI registry + theme + widget-generation preservation (Unreal editor required)

**Automated cores are green before the human pass; the UMG/asset seam is
editor-verified (autoMerge off).**

### 0. Automated pre-checks

- [ ] `npm run engines:unreal:ui` — the default-UI host gate is green (20 cases:
      registry default/override/missing + the real WBP default map, the loading
      view-model weighted-progress/monotonicity/label/completion/tips against
      `conformance/ui/loading-phases.json`, and the theme-token table vs
      `conformance/ui/theme-tokens.json`).
- [ ] `npm run engines:check` — the structural syntax gate covers the new UE seams
      (`UInsimulUIRegistry`, `UInsimulUITheme`) and the grep-guard confirms the
      cores stay UE-free.

### 1. Generation preserves widgets + registry + fonts

- [ ] Run the export pipeline (or `Scripts/GenerateInsimulContent.py` manually) on a
      project with the InsimulExport module built. Confirm `/Game/UI/WBP_*` are
      created as before (dialogue, game menu, quest tracker, quest offer, intro, …)
      with their bound child widgets named to match the C++ `BindWidget` properties.
- [ ] Confirm `/Game/UI/DA_InsimulUIRegistry` exists and its **Panels** array binds
      `dialogue`, `game_menu`, `loading_screen`, `quest_offer`, `quest_tracker` to the
      matching `WBP_*` generated classes. Confirm `/Game/UI/DA_InsimulUITheme` exists
      with the shared token defaults.
- [ ] With a `Content/Fonts/` folder present, confirm fonts still import and the
      created text widgets use them (CJK/Arabic/Devanagari renders glyphs, not tofu) —
      the i18n font path is unchanged by this story.

### 2. Registry override + loading screen

- [ ] Point **Project Settings ▸ Insimul ▸ UI ▸ UIRegistry** at a custom
      `UInsimulUIRegistry` (or add a `Panels` entry overriding, e.g., `dialogue`).
      Confirm `ResolvePanelClass("dialogue")` returns the override class, and an
      unknown key logs the `No panel registered for key` warning.
- [ ] Drive a boot from the main menu and confirm the loading screen advances through
      the phases (Starting up → Loading world → … → Ready) with a monotonically
      rising bar and a per-phase tip, reaching 100% at **Ready**.

### Deltas vs Unity/Godot

**Target: zero on the registry + loading + token contract.** The three cores mirror
the Godot leg (`insimul_ui_registry.gd` / `loading_screen_model.gd` /
`insimul_ui_tokens.gd`) and the shared corpus case-for-case; only the concrete
default map (WBP asset paths vs Godot `.tscn`) and the native theme representation
(Slate vs Godot `Theme`) differ by design. Any resolution / progress / token
divergence found in the human pass is a bug — file it against US-XU1.

## US-XU2 — Quest journal / tracker / offer + notifications (Unreal editor required)

**Automated cores are green before the human pass; the UMG seam is
editor-verified (autoMerge off).**

### 0. Automated pre-checks

- [ ] `npm run engines:unreal:quest-ui` — the quest-UI host gate is green (29
      assertions): the full `conformance/ui/quest-journal-cases.json` matrix (tab
      filtering + counts, accept/decline offers, bounded tracker HUD, radiant
      arrival via upsert), the **event-driven** surface (every mutation pushes a
      `FQuestJournalEvent`; read-only + rejected ops emit nothing — proving no
      polling), the toast notifications core (push / tick-expiry / dismiss /
      kind→color), and the quest-events-drive-toasts integration.
- [ ] `npm run engines:check` — the structural syntax gate covers the new UE seam
      (`UInsimulQuestJournal` + its `OnQuestJournalChanged` dynamic multicast
      delegate) and the grep-guard confirms `InsimulQuestJournalModel` /
      `InsimulNotifications` stay UE-free.

### 1. Journal filtering + counts

- [ ] Open the quest journal (WBP_QuestJournal). Confirm the tab row shows the
      per-status counts and switching tabs (All / Active / Completed / Available)
      partitions the list to exactly the matching quests, in stable order.

### 2. Tracker HUD (bounded, event-driven)

- [ ] Track an active quest — it appears on the HUD tracker immediately (the widget
      rebuilds off the `QuestTracked` event, not a tick). Track past the configured
      max: further tracks are rejected with no HUD change. Untrack frees a slot.
- [ ] Complete a tracked active quest: it auto-untracks (drops off the HUD) and moves
      to the Completed tab in one event round-trip.

### 3. Offer dialog + radiant arrival + toasts

- [ ] Trigger a quest offer (InsimulQuestOfferPanel). Accept → the quest moves to
      Active and a success toast fires; Decline → the offer is removed entirely.
- [ ] Let a radiant quest arrive (quest_offered): it appears under the Available tab
      and raises an info toast, with no per-frame polling on the panels. Toasts age
      out on their own timer.

### Deltas vs Unity/Godot

**Target: zero on the journal/tracker/offer + notifications contract.** The cores
mirror the Godot leg (`quest_journal_model.gd` / `insimul_notifications.gd`) and the
shared `quest-journal-cases.json` case-for-case; only the UMG widget layer (dynamic
multicast delegate vs Godot signals) differs by design. Any lifecycle / filtering /
tracking / notification divergence found in the human pass is a bug — file it
against US-XU2.

## US-XU3 — Inventory / container / merchant trade

Host gate: `npm run engines:unreal:trade` (15 assertions — the shared
`trade-cases.json` matrix + the state-location invariant). Human pass in a real
editor build for the UMG layer (`UInsimulTradePanel` + `InsimulInventoryUI` /
`InsimulShopPanel`):

### 1. Inventory + container transfer

- [ ] Open the inventory (WBP_Inventory) — the stacks and gold shown are exactly
      `save.currentState.player` (no private copy). Open a container (WBP_Container),
      Take a partial stack: the item moves into inventory, the container stack shrinks,
      and the total item count is unchanged (conserved, not duplicated). Take-all
      empties the container into inventory.
- [ ] A take request larger than stock is clamped to what's available; taking from a
      missing/absent container is a safe no-op (reason `no_container`).

### 2. Merchant buy / sell

- [ ] Open the merchant (WBP_ShopPanel). Buy an affordable item: it moves merchant→
      player, `player.gold` drops by the cost and `merchant.goldReserve` rises by the
      same (gold conserved). Sell an item back: it moves player→merchant, gold flows
      the other way, still conserved.
- [ ] Buy with insufficient gold → rejected (`insufficient_gold`), no state change.
      Buy past stock → `out_of_stock`. Sell an item the player lacks →
      `insufficient_items`. Sell when the merchant can't afford it →
      `merchant_cannot_afford`. Every rejected op leaves currentState untouched.

### 3. Save round-trip (state-location invariant)

- [ ] After a buy/sell/transfer, save then reload: the post-trade quantities and gold
      persist exactly, because the panels wrote only to `save.currentState` — there is
      no side store to lose.

### Deltas vs Unity/Godot

**Target: zero on the trade contract.** The core mirrors the Godot `trade_model.gd`
and the shared `trade-cases.json` case-for-case; only the UMG widget layer differs by
design. Any buy/sell/transfer/conservation divergence found in the human pass is a
bug — file it against US-XU3.

## US-XU4 — Dialogue panel + pause/main menu + save/load

Host gate: `npm run engines:unreal:dialogue-ui` (24 assertions — the shared
`chat-cases.json` streaming/action/history matrix, `pause-menu-cases.json`
tab-gating + reducer, and `save-slot-cases.json` row rendering incl. the
corrupted-envelope messaging). Human pass in a real editor build for the UMG layer
(`UInsimulChatPanel` + `InsimulChatPanel`, `UInsimulPauseMenu` +
`InsimulGameMenuWidget`, `UInsimulSaveSlotPanel` + `InsimulSaveGame`):

### 1. Dialogue / streaming conversation (full loop)

- [ ] Start a conversation (WBP_Dialogue). The NPC greeting shows first. Send a line:
      a player bubble appears immediately, then the NPC reply streams in chunk-by-chunk
      into one bubble (typing indicator while `IsStreaming()`), and settles on
      completion. TTS plays and the `InsimulFaceSync` lip-sync drives off the settled
      `LastNpcText()`.
- [ ] A reply that triggers an action (e.g. "take the sword") applies its
      `FactToAssert` to the live KB — verify the world state changed (item now owned).
- [ ] Force a stream error (kill the model connection): an `[Error: …]` bubble renders
      and that turn does NOT persist. Send another line — it works; the errored turn is
      absent from the saved history.
- [ ] Send two turns, then save + reload: `save.conversations` holds exactly the
      settled player/npc pairs (no in-flight or errored bubbles), `totalTurnCount`
      matches the completed count.

### 2. Pause / ESC menu tab-gating

- [ ] Press ESC in a language-learning-bundle game: the menu shows every gated tab
      (vocabulary / skills / analytics / assessment / character alongside the core
      tabs). In an RPG bundle (no assessment module) the Assessment tab is absent; with
      no modules only the six core tabs (resume/journal/inventory/map/settings/save)
      show. Tabs match the enabled feature-modules from the IR.
- [ ] Opening the menu selects the first visible tab; clicking a visible tab switches;
      a hidden tab is never selectable. ESC toggles the menu open/closed and un-pauses.

### 3. Main menu + save/load slots

- [ ] Main menu Continue is enabled only when at least one slot is loadable
      (`HasAnyLoadable()`). Open Save/Load: healthy slots show `Name · Lv N · Location`
      + "Saved <when>"; empty slots show "Empty Slot".
- [ ] Corrupt a save on disk (flip a byte): its slot renders as "Corrupted Save" with
      the integrity message and cannot be loaded, but CAN be overwritten by a new save.
      A missing/unrecognized payload shows its respective message. The messaging is
      identical to the Unity/Godot legs.

### Deltas vs Unity/Godot

**Target: zero on the dialogue / menu / save contract.** The three cores mirror the
Godot `chat_model.gd` / `pause_menu_model.gd` / `save_slot_model.gd` and the shared
corpora case-for-case; only the UMG widget layer differs by design. Any streaming /
gating / slot-rendering divergence found in the human pass is a bug — file it against
US-XU4.

---

## US-2 (190) — The play panels, backed only by `save.currentState` (Unreal editor required)

**Automated cores are green before the human pass; the UMG seams are editor-verified
(autoMerge off).**

### 0. Automated pre-checks

- [ ] `cd tools && npm run check` — **`check-panels: OK`**: every catalog row is
      generated by a `WIDGET_SPECS` entry bound to that key, and the four rows
      nothing serves yet print as PENDING with the story that owes them.
- [ ] `npm run check:host` — ctest **`ui_state_binding`** (the pricing + equipping
      corpora and the state-location invariant) and **`ui_skill_tree`** (all six
      cases of `conformance/skills/trees.json`, 39 checks) are green.

### 1. The panels resolve, and the generator built them

- [ ] After `GenerateInsimulContent.py`, `/Game/UI/` holds `WBP_Inventory`,
      `WBP_Container`, `WBP_ShopPanel`, `WBP_EquipmentPanel`, `WBP_SkillTree`,
      `WBP_Minimap`, `WBP_WorldMap`, `WBP_ActionQuickBar`, `WBP_RadialMenu`,
      `WBP_NoticeBoard`, `WBP_DocumentReader` and `WBP_HUD`, each parented to the
      **plugin** class named in its spec (not the `templates/source/ui/` prototype).
- [ ] `DA_InsimulUIRegistry` binds each of those to its panel key.

### 2. Playthrough data lives in the save file and nowhere else

- [ ] Open the inventory, take from a container, buy from a merchant. Save, quit to
      desktop, relaunch and load: gold, stacks, container contents and the merchant's
      reserve are exactly what they were.
- [ ] Inspect the written save: the changes are inside `currentState`, and nothing
      outside it moved. The integrity hash follows the change (a tampered file is
      reported by the save/load panel, not silently loaded).
- [ ] Two panels open at once (inventory + container) show one set of numbers — take
      an item in one and the other reflects it without a reload.

### 3. Shop + reputation, and equipment

- [ ] The same item in the same shop costs a character in good standing with the
      shop's faction less than one in bad standing. Removing the standing fact moves
      the price back.
- [ ] The equipment panel's slots are the WORLD's (its content declares them, and a
      world that authored two ring slots has two). A refused equip names its reason
      and, for an unmet requirement, says which skill at which level.
- [ ] Wearing armour does not make the character lighter: the carried weight is
      unchanged by equipping.

### 4. The skill panel

- [ ] Open it on a brand-new character: every priced node is greyed with `points`,
      and a node the world priced at nothing is available out of an empty pool.
- [ ] Spend a point. The node moves to taken, `spent` rises by its cost, and the row
      below it opens.
- [ ] A node whose authored requirement the KB does not satisfy reads its reason
      (the unmet goal), and a node a norm forbids reads that — the panel greys it out
      for the SAME reason an unlock would refuse it with.
- [ ] At the skill's cap the header shows no invented price for a level nobody can
      take, and the tree still draws.
- [ ] A world with no authored trees shows an empty panel with a sentence, not a
      crash — and a world whose bundle omits the skill module never opens the panel
      at all (the HUD/menu entry is withheld and says so).

### 5. HUD, maps, quick bar, documents

- [ ] Play a world whose genre selects the map module: the HUD shows the minimap and
      the world map opens with its pins. The corner map rotates (or holds north) per
      the setting, and a pin beyond the range is not drawn at the rim.
- [ ] Play a world whose genre does NOT select it: the HUD has no map slot,
      `Describe()` names `minimap` / `world_map` as withheld with the reason, and
      nothing logs an error about a missing widget.
- [ ] Selecting a fast-travel destination broadcasts the request — the host decides
      whether the journey happens; the panel never teleports the player itself.
- [ ] The quick bar and the radial wheel show the SAME shortcuts; a disabled one is
      greyed on both and firing it does nothing.
- [ ] The notice board lists the available quests the journal holds, and accepting
      one there moves it to active exactly as accepting the offer dialog does.
- [ ] A long document paginates, the counter reads `n / N`, and the ends do not wrap.

### Deltas vs Unity/Godot

**Target: zero on the view-model contracts.** The skill view (rows, edges, prices,
refusal order), the trade + quest matrices, the pricing and equipping corpora and the
state-location invariant are shared; only the widget refs and the layout differ by
design. A native leg that draws a different row, prices a level differently, or
writes playthrough data outside `currentState` is a bug against this story.


## US-M1 — band-120 mechanic hosts (Unreal editor required)

Tasklist 146 US-1. The eight host interfaces core's band-120 mechanic modules
declare now have Unreal implementations in `templates/source/mechanics/`. **Nothing
here can be executed by a gate in this repo** — no UBT, no engine headers — so the
whole of the engine half is a human pass. `RUNTIME_CORE_ADOPTION.md` §12 is the
write-up; read §12.1 first, because the expected outcome of the boot log below is
"every mechanic is inert", and that is a pass, not a failure.

### 0e. Automated pre-checks (run before the human pass)

```sh
cd tools
npm run check                 # includes the mechanic host manifest + 4 negative controls
npm run check:host:binaries   # --require-binaries: adds mechanic_bridge (the measurement)
```

- [ ] `check:mechanics` prints 7 modules, 8 host interfaces, 33 members, and names an
      implementing file for **every** interface (no `STUBBED:` line).
- [ ] `ctest mechanic_hosts` passes (106 checks) and `mechanic_bridge` passes (120),
      the latter printing the libinsimulcore version stamp it measured.

### 1. Boot log — the honest report

- [ ] Open an exported game with `UInsimulMechanicHostBinder` present and play. The
      log carries one `[Insimul]` line per band-120 module. In any build shipping today
      each is a **Warning** reading `<module>: this build's core bridge carries no
      <module> rows … The host half is implemented and inert.` That is the correct
      output — see §12.1. A `Log`-level "reachable and wired" line means a mechanic row
      has landed in `native/corebridge/js/entry.js`, and §12 needs revisiting.
- [ ] No module line is missing, and none reads "unavailable" with no remedy.

### 2. The registry and the probes

- [ ] `RegisterActor("nessa", <NPC>)` then `RegisterLocation("forge_gate", …)`. Call a
      trajectory query through `Hosts()`: with clear line of sight the reading is
      `clear`, with a wall between the two it is not, and `blockedBy` names the actor
      the trace hit.
- [ ] Stand the NPC in shadow and in sunlight: the perception reading's `light` moves
      between the ambient floor and 100. It is an approximation (§12.4) — check it
      *moves*, not that it matches a lightmap.
- [ ] Unregister the NPC's atom, then query again: the probe reports **no reading**,
      and the caller sees core's documented fallback rather than a blocked line.

### 3. Locomotion

- [ ] Order a movement to a registered location atom for a pawn with an
      `AAIController`: the pawn walks there and the report is `arrived: true` at the
      moment of the call, not at the moment of arrival (§12.2 finding 3).
- [ ] Order one for a pawn with no controller, and one to an unregistered atom: both
      report `arrived: false` with a reason naming what was missing. Neither crashes.
- [ ] Order the same movement at `urgency: "urgent"` and `"idle"`: `MaxWalkSpeed`
      differs. The atom never leaves core as a speed.

### 4. Combat and survival

- [ ] `RegisterCombatEntity` a target, then apply a damage number through the host:
      health drops by **exactly** that number. No crit, no block, no dodge is applied
      here — if the number changes, an adapter is rolling its own damage (§12.4).
- [ ] Call `ExecuteAttack`: it refuses and warns once. This is deliberate.
- [ ] In a survival world, spend stamina through the host: the HUD meter moves once,
      not twice, and the needs clock is not double-ticked. `SetEnabled(false)` stops
      decay and keeps values; re-enabling resumes rather than restarts.

### 5. Skill modifiers

- [ ] Apply `{ move_speed: 20 }` for a registered character: `MaxWalkSpeed` rises.
      Apply the **same** set again: it does not rise a second time (core requires
      idempotence).
- [ ] Apply `{ carry_capacity: 10 }`: nothing changes and the log warns once that the
      parameter reached nothing. That is the documented gap, not a bug (§12.4).

### Deltas vs Unity/Godot

**Expected: the four in §12.5**, and no others. `false` in place of a caught
exception, `AddModifier` landing without a preset, plain C++ implementations behind
one reflected binder, and the portable half being executed by a gate here. Anything
else — a different fallback, a re-priced action, a re-rolled damage number — is a
bug against US-M1.

---

## US-M2 — modules activated from the genre bundle (Unreal editor required)

Tasklist 146 US-3. This world's active mechanic modules are now read out of the
genre bundle core emitted, the packs they own are consulted into the KB, the hosts
no active module names are unregistered, and a sample scene lets two adopted
mechanics decide something. `RUNTIME_CORE_ADOPTION.md` §14 is the write-up; read
§14.4 first, because the scene exercises the **predicate** half of each module and
the decision layers are still unreachable — that is a pass, not a failure.

### 0. Automated pre-checks (run before the human pass)

```sh
cd tools
npm run check                 # adds check:packs + check:activation (6 controls)
npm run check:host:binaries   # adds module_activation + activation_witness
```

- [ ] `check:activation` prints 8 genres, 8 activation sources with no mechanic
      named in any of them, and 1 scenario.
- [ ] `ctest module_activation` passes (444 checks) — the resolver's three answers,
      the consult's four outcomes, the host restriction and the runner's five
      outcomes, over the data an exported game reads.
- [ ] `ctest activation_witness` passes (112 checks) and prints
      `witnessed 8 genre(s) x 11 pack(s)`, the scenario's five steps, and the
      §14.3 line — an authored requirement naming an inactive module's vocabulary
      still **RAISES**. If that line no longer says RAISED, core changed and §14.3
      is stale.

### 1. Boot log — which modules this world turns on, and who said so

- [ ] Play an exported game. `LogInsimulActivation` carries one line naming the
      genre, **where it came from** (`from the World IR` for an exported world),
      the active modules, the consulted packs and the active host interfaces.
- [ ] The next line names the packs that were **not** consulted. It is not empty
      for any real genre, and every name in it belongs to a module this world's
      bundle did not select.
- [ ] `LogInsimulMechanicSurface` then warns once per host it **unregistered**:
      "implemented and UNREGISTERED — no module this world activates names it".
      Cross-check one against the activation line: it must not appear there.

### 2. The three answers, deliberately provoked

- [ ] Set `UInsimulModuleActivator::DeclaredGenre` to a genre core does not know
      and re-run `ActivateForGenre`: a **warning** naming the genre, the shared
      vocabulary consulted, and **no** mechanic module active. Not a crash, and not
      a silent fallback to every module.
- [ ] Rename `Content/Data/WorldIR.json` and play: a **warning** that no genre was
      declared, and every pack consulted. That is core's editor default and is
      wrong for a shipped game — which is exactly what the warning says.
- [ ] Rename `Content/Data/insimul/packs/` and play: an **error** per active pack
      and a final error that the modules owning them cannot answer. Nothing
      pretends to have booted.

### 3. The scene (the two-mechanics claim)

- [ ] Drop `AInsimulMechanicSampleScene` into a level, point `GuardActor` and
      `PlayerActor` at two pawns with something solid between them, and play. The
      log prints five steps; the two `perception` and two `traversal` steps read
      `→ <what the scene did>`, and the fifth reports that an inactive module's
      vocabulary is absent.
- [ ] Move the player into clear line of sight of the guard and re-run
      (`RunScene`): the first step's answer **changes**, because the probe's
      reading changed. That is the whole claim — the engine measured, core decided.
- [ ] Set `bUseLiveProbes = false` and re-run: the log says the readings were
      **REPLAYED**, and the answers match ctest `activation_witness` exactly. A
      replay must never be reported as a measurement.
- [ ] Delete the scenario file and play: one error naming the path, and no steps.

### Deltas vs Unity/Godot

**Expected: the three in §14.5**, and no others — the KB witness is a ctest rather
than a compiled-at-gate-time C driver, one KB per genre in one process, and a
resolved rather than required TypeScript runner for re-vendoring. The activation
semantics themselves (the three answers, the consult order, what an inactive module
costs) are core's and must be identical across the three engines. Anything else is
a bug against US-M2.

## US-1 (190) — Module-gated panel surface + the pattern-proof pair (Unreal editor required)

**Automated cores are green before the human pass; the UMG/subsystem seam is
editor-verified (autoMerge off).**

### 0. Automated pre-checks

- [ ] `cd tools && npm run check:host` — ctest **`ui_registry`** is green (43
      checks: the shared registry / loading / theme corpora, the shipped panel
      catalog, the module gate, and six negative controls).
- [ ] `npm run check:activation` — `no module id or pack area appears in 11
      activation source(s)`: the panel catalog and its seam are in the scan now, so
      a hardcoded mechanic name in the UI fails the gate.

### 1. The gate, in a running game

- [ ] Export a game whose World IR declares a genre that owns the panels the
      catalog gates, play, and confirm the boot log prints
      `[UI] UI panels: genre '<id>' shows N of M panel(s); withheld: none`.
- [ ] Export (or `DeclaredGenre`-set) a game whose genre selects none of those
      modules. The same line names the **withheld** panels, and asking the
      subsystem for one returns null with a warning that names the key and the
      module — not a silent no-op, and not an empty panel.
- [ ] Set a genre id core has never heard of. Every module-owned panel is withheld
      (the same refusal the pack consult makes), and the activation warning about
      an unknown genre is present.
- [ ] Remove `Content/Data/WorldIR.json` so nothing declares a genre. The UI line
      reads **UNGATED** and every panel resolves — matching the pack consult, which
      activates everything in that state.
- [ ] Corrupt `Content/Data/insimul/ui/panels.json`. An **error** naming the file
      and stating that no panel can be gated; the game still runs on the built-in
      map. (Delete it instead: the same fallback, at Log rather than Error.)

### 2. Override still wins, and still cannot ungate

- [ ] Add a `Panels` entry to `DA_InsimulUIRegistry` overriding an available key
      with your own WBP. `UInsimulUIPanelSurface::ResolvePanelClass` returns YOUR
      class.
- [ ] Do the same for a key that is currently **withheld**. It stays withheld —
      swapping a widget says nothing about which modules the bundle selected.

### 3. The pattern-proof pair

- [ ] Confirm the generator created `/Game/UI/WBP_LoadingScreen` (parent
      `InsimulLoadingScreen`) and `/Game/UI/WBP_Notifications` (parent
      `InsimulNotificationsWidget`), and that `DA_InsimulUIRegistry` binds
      `loading_screen` and `notifications` to them. `WBP_IntroSequence` no longer
      carries the `loading_screen` key — it is the narrative cutscene.
- [ ] Boot with the loading screen up: the bar rises **monotonically** through the
      phase labels, the percent text matches the bar, the tip changes per phase,
      and it reaches 100% at the terminal phase (`OnLoadingComplete` fires once).
- [ ] Re-entering a phase, or arriving at one out of order, never moves the bar
      backwards.
- [ ] Push toasts of each kind. They stack oldest-first, take their colour from
      `DA_InsimulUITheme` (accent / success / warning / danger), age out after
      their lifetime, and dismissing one early removes exactly that one. With no
      toasts, the widget does no per-frame work beyond the queue's tick.

### Deltas vs Unity/Godot

**Target: zero on the registry, gate and loading contracts.** The catalog rows and
the three gating answers are shared; only the concrete widget refs (WBP paths vs
`.tscn` / prefabs) differ by design. The panel → module ownership table itself is
engine-side data until core emits a UI surface per module — if the Unity or Godot
leg gates a panel differently from `ui/panels.json`, that is a bug against this
story, not a local choice.
