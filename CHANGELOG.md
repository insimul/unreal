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
