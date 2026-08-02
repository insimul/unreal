# Adopting `@insimul/core` in the Unreal plugin — the adoption plan (US-1 of 99)

**Status: design document. No code changes accompany it.** It reads
`packages/core/docs/runtime-contract.md` (US-4 of `93-runtime-logic-to-core`) and
turns it into a concrete plan for *this* repo — its `InsimulRuntime` module, its
portable `std`-only cores, its `UInsimulPrologSubsystem`, its `ThirdParty`
libinsimul link, its C++ game template, and its vendored conformance corpus.

This is the **second** native adapter designed against that contract. The first
— `godot/RUNTIME_CORE_ADOPTION.md` (tasklist 100) — answered the question the
whole program hung on, and this document does **not** re-derive it. §4 confirms
that answer from Unreal's side and costs the Unreal-specific parts of it.

**Verdict up front:** adopt, through `libinsimulcore` (already built, already
promoted to `native/` by tasklist 104), and only for the decision layer. The
first slice is **radiant quest *generation*** (§5) — a capability this engine
does not have, whose 11 corpus vectors are already vendored here and read by
**nothing**. §7 lists what we should *not* adopt, and §6 lists what this repo
believes about itself that turned out to be false.

---

## 0. Where this engine actually stands

Measured in this worktree on 2026-08-02, because a plan that mis-sizes its
starting position mis-sizes everything after it. (Godot's §6.1 found its own
tasklist description off by an order of magnitude; ours is *right*, see §6.1.)

| surface | files | lines | what it is |
| --- | --- | --- | --- |
| `Source/InsimulRuntime/**` | 102 | **17,113** | the runtime module — the number the tasklist description cites, and it is correct |
| ├ `Portable/` | 34 | 5,198 | the UE-free, host-testable semantic cores (`std` only, no `CoreMinimal.h`) |
| ├ `Private/` | 30 | 6,327 | UE implementations: conversation, REST/WS, spawner, quest manager, crowd, **`Private/Prolog/`** |
| ├ `Public/` | 32 | 3,473 | the reflected UE surface (`USTRUCT`/`UCLASS`/Blueprint) |
| ├ `Tests/` | 4 | 1,577 | UI view-model host tests — **no build wiring in this repo** (§6.4) |
| └ `Generated/` | 2 | 538 | `InsimulWorldDTO.h` / `InsimulGenerated.h`, emitted from the World IR |
| `Source/InsimulEditor/**` | 61 | 10,733 | the editor module (binding table, scene placement, reimport, Connect panels) |
| `Source/ThirdParty/**` | — | — | `InsimulLibrary` (libinsimul ABI + `Build.cs`) and vendored `nlohmann/json.hpp` |
| `templates/source/**` | 211 | **39,096** | the C++ **game template** the export pipeline copies into a generated game |
| └ `templates/source/systems/` | 92 | 16,328 | 46 gameplay systems — the surface §3 compares against core |
| `conformance/**` | 27 JSON | — | the vendored cross-engine corpus. It has drifted (§6.3) |

Five of those matter to this plan:

- **`Source/InsimulRuntime/Portable/` is already a runtime core** — 5,198 lines of
  dependency-free C++17 that hand-ports core's semantics (`InsimulQuestSystem`
  650, `InsimulJson` 391, `InsimulSaveSystem` 380, `InsimulContentLibrary` 361,
  `InsimulWorldSource` 230, `InsimulCanonicalJson` 151, `InsimulBootstrap` 148,
  plus six corpus-pinned UI view models) and is pinned to the TypeScript
  authority by the shared corpus. This is the same **fourth answer to the
  language boundary** Godot found itself running, and it is further along here.
  §4.4 says why it cannot be the strategy.
- **libinsimul is already linked, correctly.** `Private/Prolog/InsimulKB.{h,cpp}`
  (712 lines) wraps the C ABI; `UInsimulPrologSubsystem` (447 lines across
  `Public/` + `Private/`) is the game-thread-affine `UGameInstanceSubsystem` over
  it, holding **one long-lived KB per `GameInstance`**, created in `Initialize`
  and released in `Deinitialize`. This repo has consumed a shared native core
  across the language boundary before, and it worked.
- **The vendored ABI header is byte-identical to the shipping one.** `diff
  Source/ThirdParty/InsimulLibrary/include/insimul.h
  native/include/insimul.h` → no difference, and every call site tests `== 0`.
  Godot's §6.6 warned 98 and 99 to check this. Checked; clean (§6.6 below).
- **`conformance/` is vendored, not referenced** — this repo is standalone, so it
  holds its own copy of core's corpus. That copy has drifted, exactly as
  predicted (§6.3).
- **The game template is C++, not script.** Unlike Godot's GDScript template,
  every template system is a `UGameInstanceSubsystem`/`UActorComponent` compiled
  into the exported game. That raises the cost of *replacing* a template system
  and lowers the cost of *calling a C ABI from* one.

---

## 1. The contract, in this engine's terms

The contract has three halves. Restated against the classes in this repo rather
than abstractly.

### 1.1 `system-contracts.ts` — nine interfaces the engine implements and owns

Core declares them; each engine ports its own. The contract even names our
filenames (`ICombatSystem` → `CombatSystem.h`). These are **not** things we
adopt — they stay ours. In this repo they resolve to two different layers, and
conflating them is the most common way to mis-read §3:

| layer | where | what it is |
| --- | --- | --- |
| plugin runtime | `Source/InsimulRuntime/Portable/InsimulQuestSystem.{h,cpp}` | the corpus-pinned semantic core the plugin ships |
| game template | `templates/source/systems/QuestSystem.{h,cpp}` (1,389) | the gameplay-facing subsystem an exported game gets, **deprecated in favour of the plugin core** by `MIGRATION.md` §US-XC3 |

§3 compares both against core's modules, separately, because they have different
owners and different replacement costs.

### 1.2 `host-contracts.ts` — five hooks core calls back into us

`EngineHostAdapter` = `{ debug?, lifecycle?, speech?, resources?, combatStats? }`.
Every field is optional and degrades to a documented fallback, so an adapter can
come up in stages. The contract already guesses our implementations (`UE_LOG` /
a `UInsimulDebugSubsystem`, `AActor::EndPlay` / `FCoreDelegates::OnPreExit`, a
`USoundWave` from platform TTS, a `UAttributeSet` / GAS attribute write). §2
checks those guesses against what is actually here — **three are right, one is
half-right, one is wrong.**

### 1.3 `data-source.ts` — `IDataSource`, the one required interface

182 lines declaring **79 `Promise`-returning methods** covering world / character
/ quest / settlement loading, playthroughs, inventories, containers and game-state
save/load. In this repo that surface is spread across five classes and does not
look like `IDataSource` at all:

| `IDataSource` area | this engine today |
| --- | --- |
| `loadWorld` / `loadCharacters` / `loadSettlements` / `loadQuests` | `FInsimulWorldSource` (`Portable/InsimulWorldSource.{h,cpp}`) — reads a `SaveFile.worldSnapshot` through the **generated** DTOs (`Generated/InsimulWorldDTO.h`), schema-version gated |
| content packs (items / characters / towns / quests / narratives) | `FInsimulContentLibrary` (`Portable/InsimulContentLibrary.{h,cpp}`, 361) — a *different*, engine-neutral library format |
| `saveGameState` / `loadGameState` | `FInsimulSaveSystem` (`Portable/InsimulSaveSystem.{h,cpp}`) — canonical JSON + SHA-256 integrity envelope, v1→v3 migration, KB round-trip |
| `loadPrologContent` | `UInsimulPrologSubsystem::Consult()` → libinsimul. Not a fetch |
| everything network-shaped (playthroughs, dynamic quests, merchant inventories, NPC guidance) | `UInsimulRestClient` (415) / `UInsimulWSClient` (281) — the authoring-server path, used by the conversation SDK and the editor Connect panels, **not** by an exported offline game |

**The structural mismatch that matters:** `IDataSource` is `async` end to end
because it was derived from a browser client talking to an authoring server. An
exported Unreal game has no server; its world is a file already on disk, and
`FInsimulSaveSystem`/`FInsimulWorldSource` are **synchronous by design** so they
can be host-tested under plain `clang++`. Adopting `IDataSource` verbatim would
mean wrapping synchronous file reads in promises so a JS runtime can await them,
and then pumping that runtime's job queue from a `FTSTicker` delegate. That is
real work, and it is the largest single cost item in §4.

**This is a contract revision, not an adapter contortion** (§5.5 invites it).
Godot reached the identical conclusion independently, which is the strongest
evidence available that the interface — not the two adapters — is what is wrong.
Recorded in §8 as amendment 1.

### 1.4 The lifecycle, in Unreal terms

Core assumes a host that constructs a game object, drives it, and tears it down.
This repo's equivalent already exists, is corpus-pinned, and is where any core
adoption must enter:

```
UGameInstance
  └─ UInsimulPrologSubsystem   Initialize() → one insimul_kb   Deinitialize() → release
  └─ UInsimulRuntimeSubsystem  the plugin's session object
       └─ insimul::FInsimulRuntimeContext::Boot()      (Portable/InsimulBootstrap.h)
            world source → save slot → KB → systems init
            Rehydrate()  — world off worldSnapshot, KB off currentState.prologFacts,
                           hydrate every quest's Prolog content
            Commit()     — snapshot the live KB back into currentState.prologFacts
```

`FInsimulRuntimeContext::Boot()` is this engine's `InsimulRuntimeCore::boot()`.
**Any core adoption must enter through it**, not alongside it, or the
`worldSnapshot` integrity-hash stability that `test_bootstrap.cpp` asserts (43
checks) stops meaning anything.

Two Unreal-specific lifecycle facts a core adapter must respect, both already
enforced in this repo and both worth stating because core has never met them:

- **Game-thread affinity is checked, not assumed.** Every mutating/querying call
  on `UInsimulPrologSubsystem` asserts `IsInGameThread()`. A bridge handle must
  inherit the same rule — QuickJS is single-threaded and libinsimul's KB is
  single-thread-owned, so this is the *right* rule, not a limitation.
- **`Deinitialize` is deterministic.** Unlike a GC'd host, we know exactly when
  the KB dies. Contract §5.5 warns of "shapes that assumed a single-threaded,
  garbage-collected host"; the single-threaded assumption survives here, the
  garbage-collected one does not, and every handle we take must have an explicit
  release site in `Deinitialize`.

---

## 2. Host-capability map — have / must write / no counterpart

The third column is the interesting one and is listed explicitly, as the story
asks.

### 2.1 Already have it — the adapter is a wrapper

| core hook | what this engine already has | gap |
| --- | --- | --- |
| `IDebugSink` | `UInsimulDebugComponent` (114 lines) plus `UE_LOG` categories throughout and `insimul::FInsimulNotifications` (`Portable/InsimulNotifications`, 63) | none of substance. `DebugSinkEvent`'s seven fields map to a `USTRUCT`; `isEnabled()` maps to a log-verbosity check. **~60 lines.** The contract's guess ("`UE_LOG` / a `UInsimulDebugSubsystem`") was right. |
| `IHostLifecycle` | `UGameInstanceSubsystem::Deinitialize` (used by 6 subsystems here), `AActor::EndPlay`, `FCoreDelegates::OnPreExit` | none. **~20 lines.** The contract's guess was right. Note the contract requires the handler be *synchronous* — which is exactly what `Deinitialize` gives us. |
| persistence (`IDataSource`'s save half) | `FInsimulSaveSystem` + `FInsimulCanonicalJson` + `FInsimulSha256`: canonical JSON, SHA-256 integrity envelope, v1→v3 migration, KB round-trip — **already byte-pinned to the TS authority** (`tools/cross-check/cpp-produced.envelope.json`, 51 host checks) | none. This is the strongest thing in the repo. Do not replace it (§7). |
| world / content loading (`IDataSource`'s load half) | `FInsimulWorldSource` + `FInsimulContentLibrary`, both DTO-typed and version-gated, 101 host checks between them | shape only — sync vs `Promise`. See §1.3. |
| Prolog | `UInsimulPrologSubsystem` → `insimul::InsimulKB` → libinsimul/Trealla, the *same engine source* core's browser runtime now runs (contract §5, blocker 1 RESOLVED) | none. |

### 2.2 Must be written, but the pieces exist

| core hook | what exists | what must be written |
| --- | --- | --- |
| `ISpeechSynthesizer` | **more than any other engine has.** `UInsimulConversationComponent` (510) streams server TTS audio + visemes over `UInsimulWSClient`; `UInsimulFaceSync` (190) drives lip-sync; `UInsimulOfflineProvider` (342) runs a local LLM path; `InsimulSettings` already models `TTSProvider = Server \| Local \| None` | an adapter that returns `SynthesizedSpeech` rather than *playing* it. The existing path is a **player**, not a byte producer — the same shape problem Godot hit with `tts_speak`, but here the server path really does return bytes, so the `audio`/`mimeType` reading is satisfiable rather than aspirational. Est. **~120 lines**, and `null` (text-only) remains a documented, normal outcome. |
| `IResourceStore` | `templates/source/systems/ResourceSystem` (208) — a *world-node harvesting* system (spawn, gather with a tool, deplete, respawn) with no `hasResources`/`consumeResources`; `InventorySystem` (269) holds the counts; `ResourceGatheringSystem` (152) is the interaction half | the two-method affordance query over the player's inventory. Est. **~70 lines of glue**, plus mapping core's `ResourceType` union onto this engine's resource-type `FName`s. |

### 2.3 **No counterpart — the honest list**

These are the ones with nothing behind them in this repo.

1. **`ICombatStatSink` — the contract's guess is wrong for this engine.** It
   suggests "an `UAttributeSet` / GAS attribute write". **This plugin does not
   use the Gameplay Ability System at all** (no `GameplayAbilities` dependency in
   either `Build.cs`), and `templates/source/systems/CombatSystem` is **48
   lines** of template-substituted constants (`BaseDamage`, `CriticalChance`,
   `BlockReduction`, `DodgeChance` as `{{TOKEN}}` placeholders) plus
   `CalculateDamage`. There is no entity registry to look an id up in, so
   `getBaseStats(entityId)` has nothing to read and `applyStats` has nothing to
   write. `CombatModeManager` (204) manages *modes*, not stats.
   `EquipmentManager` — the only core-side consumer — is one of the **seven
   un-inverted modules** anyway, so this hook has no caller today.
   **Implication: stub it, and do not let the stub imply combat is wired.**
   Recorded in §8 as amendment 5, because a contract that names GAS invites the
   next reader to assume a GAS-shaped adapter exists.
2. **The whole language-acquisition stack has no host.** Core's §1.2 is 13
   modules / 3,057 lines and the contract calls it *"the product, not a
   subsystem"*. This engine has a conversation transport (`ConversationComponent`,
   `RestClient`, `WSClient`, `ChatModel`) and nothing behind it: no vocabulary
   state, no CEFR tracking, no pronunciation scoring, no learning drills.
   `LanguageProgressTracker` (1,313) and `AssessmentEngine` (1,042) are **both
   un-inverted**, so this is not adoptable yet regardless — but when it is, it
   needs a host that does not exist.
3. **`GameTruthSync` (926) has a name-alike, not a counterpart.**
   `templates/source/systems/TruthSyncSystem` (107 + header) is a **world-truth
   bulletin board**: `FWorldTruth { TruthId, Title, Content, bActive,
   ExpiresAtGameHour }` with set/expire/gate semantics. It does not reconcile
   anything with the Prolog KB. The real KB↔world mirror in this repo is
   one-way and append-only: `FInsimulQuestSystem` asserts `quest_complete/1` into
   `currentState.prologFacts` and `FInsimulRuntimeContext::Commit()` snapshots it
   back. The contract flags this module specifically — *"without it an engine has
   a KB and a world that drift."* **The name collision is a trap for the next
   reader and is called out here so nobody reports it as "already have it".**
4. **Social simulation (§1.4, 7 modules / 2,207) has no counterpart.** Volition,
   relationships, romance, residence/business behaviour, cultural events. The
   nearest things — `BusinessInteractionSystem` (195), `ReputationManager` (97),
   `NPCScheduleSystem` (104), `AmbientConversationSystem` (196) — are activity
   *drivers*, not a volition model. `AmbientLifeBehaviorSystem` is un-inverted,
   so `NpcPersonalityTraits` has no consumer either.
5. **No host for `async`.** Core is `Promise`-based throughout (`IDataSource`,
   `ISpeechSynthesizer`, `generateRadiantQuests` is itself `async`). Unreal has
   `FTSTicker`, `AsyncTask` and latent Blueprint nodes, but nothing in this repo
   pumps a JS job queue, and the portable cores are deliberately synchronous so
   they stay host-testable. **This is not a missing *capability* — it is a
   missing *mechanism*, and it is §4's problem, not §2's.** It is listed here
   because it is the reason the first slice is chosen the way it is. (It is also
   already solved for the first slice: see §4.6.)

---

## 3. Systems this engine implements that core also implements

The contract's §4.1 framing holds here: our ports are **execution surfaces**;
core is the **decision layer** behind them. So most rows are *neither* "adopt
core's" *nor* "keep ours" — they are "keep ours, put core's behind it", which is
the shape `UInsimulQuestSystemShell` (UE) → `insimul::FInsimulQuestSystem`
(portable) already has.

| area | this engine | core | recommendation |
| --- | --- | --- | --- |
| **Quests — hydration + radiant *tick* + query-driven completion** | `Portable/InsimulQuestSystem` (650 + 200 header), corpus-pinned to `quest-hydrator.ts` and `radiantTick` by 33 host checks | `QuestCompletionEngine` (1,897) + 15 more (6,312 total) | **Reconcile, later.** Ours is a hand-port of a *slice* of core's, already proven equal on the vectors it covers. Replacing it is a like-for-like swap with no capability gain and high churn. **But it is the ideal diff instrument** — see §5.3. |
| **Quests — the template tracker** | `templates/source/systems/QuestSystem` (1,389), already **deprecated** by `MIGRATION.md` §US-XC3 in favour of the plugin core | `QuestCompletionEngine`'s Prolog-backed objective evaluation | **Adopt core's, eventually — and this is a BEHAVIOURAL change.** Ours decides completion by imperative bookkeeping; core's decides it by proving a goal. The set of things that complete a quest is not the same set. Not in this tasklist (§3.1). |
| **Prolog (plugin)** | `UInsimulPrologSubsystem` + `insimul::InsimulKB` → libinsimul — real unification, real error strings, game-thread-affine | core's `src/prolog/` toolchain over the same libinsimul/Trealla source, wasm-compiled | **Already adopted.** Blocker 1 is resolved; both sides run the same engine source. Nothing to do. |
| **Prolog (template)** | `templates/source/systems/PrologEngine` (1,648) — since US-XP4 a thin game-facing **adapter** over `UInsimulPrologSubsystem`, not a second engine | `GamePrologEngine` (2,267) — **un-inverted**, still in `packages/babylon` | **Keep ours.** Core's is not available at any price. Ours is already the right shape (adapter over the shared engine). |
| **Save / persistence** | `FInsimulSaveSystem` + `FInsimulCanonicalJson` + `FInsimulSha256`, **byte-matched against a TS-produced golden envelope**, v1→v3 migration, 51 host checks | the save-file format + migrations (contract layer), `SaveConflictResolver` (377) | **Keep ours.** It is byte-identical to the authority already — adopting core's would replace a proof with a bridge crossing. `SaveConflictResolver` has no counterpart and is a future *addition*, not a replacement. |
| **Content library / import** | `FInsimulContentLibrary` (361) + round-trip parity, 73 host checks | the World IR + content contract layer | **Keep ours.** Already pinned; and see §6.3 — core's shared golden is `content-library/*.json`, a *different* shape from our `conformance/content/library-*.json`. Reconciling them is content-portability work, not runtime-core adoption. |
| **Crafting** | `templates/source/systems/CraftingSystem` — **122 lines** | `RecipeCraftingSystem` (665) + farming (534) + herbalism (534) + mining (384) + fishing (340) = **2,457** | **Adopt core's.** Near-pure capability gain. Blocked only on §4 and on `IResourceStore` (§2.2). (Core's `CraftingSystem`, 521, is un-inverted — but `RecipeCraftingSystem` is not, and it is the one that matters.) |
| **Inventory / containers** | `InventorySystem` (269) + `ContainerSpawnSystem` (80) + `Portable/InsimulTradeModel` (227, corpus-pinned by `ui/trade-cases.json`) | `ContainerManager` (151) | **Keep ours.** Ours is larger and already pinned. No gain. |
| **Resources** | `ResourceSystem` (208) + `ResourceGatheringSystem` (152) — world nodes, gathering, respawn | the four gathering systems (1,792) | **Reconcile.** Ours is the *spatial/interaction* half; core's is the *yield/table* half. They compose rather than compete: ours becomes `IResourceStore`'s backing (§2.2). |
| **Dialogue** | `DialogueSystem` (213) + `Portable/InsimulChatModel` (143, pinned by `ui/chat-cases.json`) + the conversation SDK (`ConversationComponent` 510, WS/REST 696) | §1.2's 13 modules (3,057) | **Adopt core's, eventually.** Ours is a dialogue-tree cursor plus a *very* capable transport; core's is utterance-driven action detection and live CEFR difficulty. Different thing, not a bigger version of the same thing. Blocked on §4 *and* on the un-inverted `LanguageProgressTracker`/`AssessmentEngine`. |
| **Actions** | `ActionSystem` (504) | `actions/ActionManager` (421) + the four drills | **Reconcile.** Ours executes; core's registers and dispatches. Low priority; ours is the larger of the two. |
| **Rules** | `RuleEnforcer` (389) — real Prolog queries through the shared engine | **no core counterpart** (contract §4.1: rules stay engine-side; core supplies the rule *data*) | **Keep ours.** By design. |
| **Combat / survival** | `CombatSystem` (48) + `CombatModeManager` (204) + `SurvivalSystem` (306) | **no core counterpart** | **Keep ours.** By design. See §2.3.1 for why this makes `ICombatStatSink` unhostable today. |
| **Event bus** | `templates/source/systems/EventBus` (280) | `GameEventBus` (285) | **Reconcile.** Core's `GameEventType` is what `GameQuestManager` wires itself to for automatic triggers, so adopting quest orchestration later means adopting this. Do not touch until something needs it. |
| **Onboarding** | `templates/source/systems/OnboardingManager` (148) | `OnboardingManager` (54) | **Keep ours.** Ours is larger; core's is a state bag. |
| **UI view models** | six portable models (`ChatModel`, `QuestJournalModel`, `TradeModel`, `SaveSlotModel`, `PauseMenuModel`, `UIRegistryModel`, `LoadingViewModel`, `UIThemeTokens`), all pinned by `conformance/ui/*.json` | not in core (rendering stays per-engine, contract §3) | **Keep ours.** By design. |
| **Radiant quest *generation*** | **nothing** | `generateRadiantQuests` (`src/radiant/`, 678) | **Adopt core's. This is the first slice** (§5). |

### 3.1 The behavioural-difference callout

Three rows above are behaviour changes if adopted. All three are deliberately
excluded from this tasklist, because silently switching implementations changes
shipped games:

- **Template quest completion.** Imperative `track_*` bookkeeping → Prolog proof.
  Changes *what completes*.
- **Dialogue.** Tree cursor → utterance-driven detection. Changes *what a player
  can say to make something happen*.
- **Crafting.** A 122-line recipe check → a 2,457-line system with gathering,
  growth stages and yield tables. Changes *what can be made and from what*. This
  one is nearly pure gain, but "nearly" is doing work: any recipe the template
  currently allows that core's tables disallow is a regression in a shipped game.

Everything else is either additive (radiant, social sim, language, save-conflict
resolution) or a no-op (Prolog, save, content library).

### 3.2 The distinction this tasklist will otherwise get wrong

**Radiant *tick* ≠ radiant *generation*.** They are different capabilities with
similarly named corpora, and this engine has exactly one of them:

| | corpus | cases | what it does | here? |
| --- | --- | --- | --- | --- |
| radiant **tick** | `conformance/quests/radiant-cases.json` | 3 | distributes *already-existing* quests tagged `radiant` into offering slots, deterministically | **yes** — `FInsimulQuestSystem::RadiantTick`, host-tested |
| radiant **generation** | `conformance/radiant/*.json` | **11** (5 files) | *creates* quests from Prolog templates + world preconditions + a seeded RNG | **no** — zero implementation, zero readers |

The tasklist description's claim ("has the 5-case corpus staged and no
implementation") is right about generation and would be wrong if applied to the
tick. §5 adopts generation only.

---

## 4. Decision 1 — the language boundary

**Already answered, and this repo does not get a second vote.**
`docs/UNIFICATION_ROADMAP.md` Decision 1 and `godot/RUNTIME_CORE_ADOPTION.md` §4
settled it: adapters bind a **C ABI, not a language** — `libinsimulcore`, an
opaque handle with JSON in / JSON out and an explicit error string, shaped
exactly like `libinsimul`'s ABI. TypeScript in an embedded QuickJS runs behind it
today; Rust may run behind the same ABI later, invisibly. The corollary is
addressed to this tasklist by name: **do not invent a second mechanism, and do
not hand-port core to C++.**

Tasklist 104 has landed, so the artifact is where it should be:
`native/corebridge/` — `include/insimulcore.h` (5 functions), `src/insimulcore.c`
(QuickJS host + promise pump + the libinsimul Prolog seam), `js/entry.js` (the
adopted method table), `vendor/quickjs/`, `vendor/core/` (the generated bundle +
provenance), built by the same CMake as libinsimul into `build/libinsimulcore.a`
and `build/libinsimulcore.dylib`.

**This section's job is therefore not to re-decide but to (a) confirm the
decision from Unreal's side and (b) cost the Unreal-specific parts, which nobody
has paid yet.** §4.1–§4.4 are kept as a short record of the options so this
document stands alone; §4.5–§4.7 are the new material.

### 4.1 Option A — embed a JS engine behind a C ABI *(chosen, and already built)*

Zero lines of core reimplemented; TypeScript stays the single semantics
authority. Unreal consumes it exactly as it consumes libinsimul today.

### 4.2 Option B — port core to Rust behind the same C ABI

The right destination, the wrong first move: 18,959 lines ported and kept
correct, while TypeScript cannot be retired because it *is* the browser runtime.
Slots in behind the unchanged ABI whenever it is funded.

### 4.3 Option C — a service boundary

Rejected for a shipping game. This repo already has it where it belongs —
editor-time only (`UInsimulRestClient` / the Connect panels).

### 4.4 Option D — hand-port to portable C++ and pin with the corpus *(what we do today)*

`Source/InsimulRuntime/Portable/` is 5,198 lines of exactly this, and it works:
the save envelope byte-matches a TS-produced golden, quest hydration and the
radiant tick pass the shared corpus, and all of it is testable under plain
`clang++` with no Unreal toolchain (228 host checks, §6.4). It deserves credit
and it deserves to be rejected as a *strategy*, for the same reason Godot
rejected it: extrapolated to core's full surface across three engines it is tens
of thousands of lines of hand-maintained re-implementation plus per-engine drift
guards — the duplication the program exists to delete.

**Keep it as a tactic** for the small, hot, already-done surfaces (canonical
JSON, SHA-256, the save codec, quest hydration, the UI view models), where the
port exists and is proven. **Never extend it to new core surface.**

### 4.5 What binding `libinsimulcore` costs *in Unreal specifically*

The `ThirdParty` pattern this repo already uses makes most of this cheap.

| item | cost | notes |
| --- | --- | --- |
| `ThirdParty` module wiring | **~40 lines** | Extend `Source/ThirdParty/InsimulLibrary/InsimulLibrary.Build.cs`, or add a sibling `InsimulCoreLibrary` module. Same three-platform `if (Target.Platform)` shape already written for libinsimul: `PublicAdditionalLibraries` + `RuntimeDependencies` on Mac/Linux, `PublicDelayLoadDLLs` + `RuntimeDependencies` on Win64. `native/docs/consuming.md` already specifies the `dist/<platform>/` layout, and it already ships `libinsimulcore.dylib` + `insimulcore.h` beside `libinsimul`'s. |
| binary staging | **~15 lines** | `scripts/release/build-plugin-zip.mjs` already copies `dist/<platform>/` artifacts into `lib/`; add two filenames. |
| the C++ wrapper | **~250 lines** | `Private/Core/InsimulCoreBridge.{h,cpp}` — an RAII handle mirroring `insimul::InsimulKB` exactly: `insimul_core_create/destroy`, one `Call(method, json) -> optional<string>` with `insimul_core_last_error()` on failure, `IsInGameThread()` assertions. This is the *only* file that touches the C ABI. |
| the translation site | **~150 lines** | One `UCLASS`/portable pair that converts engine types → JSON and back. §5.4. |
| host gate | **~200 lines** | A new `tools/verify-unreal` ctest target that links `libinsimulcore` and drives the corpus. §5.5. |
| **total** | **≈ 650 lines of new code**, no core reimplemented | plus the risks in §4.7 |

Compare: hand-porting `radiant-engine.ts` + `base-templates.ts` (option D) is
678 lines of TypeScript semantics re-expressed in C++, permanently, and buys one
capability. The bridge is a comparable number of lines and buys **all 60
modules**, once, for three engines.

### 4.6 The async pump is already solved for this slice

Godot's §4.1 named the async pump "the single hardest piece". `insimulcore.c`
resolved it for the adopted surface: **`insimul_core_call` is synchronous to the
host** — it drives QuickJS's job queue with `JS_ExecutePendingJob` until the
returned promise settles, which works because every `await` on the adopted
surface resolves inside the JS runtime and never calls back out to the host. The
header records that the moment a method needs a *host* callback, the design
becomes a pump driven from the host's frame loop (`FTSTicker` here) and **the ABI
does not change**.

For the radiant slice this means: no ticker, no latent node, no async task, no
change to `FInsimulRuntimeContext`. A blocking call on the game thread, exactly
like `UInsimulPrologSubsystem::Query()` today.

### 4.7 The three risks that are Unreal's alone

Recorded now so US-2 budgets them rather than discovering them.

1. **Two Trealla instances, or one, depending on link mode.**
   `libinsimulcore.dylib` **static-links libinsimul** so it loads standalone
   without an rpath dance (`native/corebridge/CMakeLists.txt` documents this).
   Unreal already links `libinsimul.dylib` separately for
   `UInsimulPrologSubsystem`. So an editor/Development build that stages both
   shared libraries has **two independent Trealla instances** in one process;
   a monolithic Shipping build linking both static archives has **one**. Both are
   correct — no `insimul_kb` handle ever crosses `insimulcore.h`, so the two ABIs
   share no state — but the consequence must be designed for, not assumed:
   **the bridge's KB and `UInsimulPrologSubsystem`'s KB are not the same KB, in
   any configuration.** What makes the radiant slice safe is that
   `radiant.generate` takes Prolog **program text**, not a handle (§5.4), so
   nothing has to be shared.
2. **The platform matrix stops at desktop.** `native/docs/consuming.md` ships
   `macos-arm64`, `macos-x64`, `linux-x64`, `windows-x64`. Unreal targets
   consoles, iOS and Android; none of those have a `libinsimulcore` build, and
   QuickJS on a console platform is a porting question nobody has asked. **The
   plugin must therefore degrade cleanly when the bridge is absent** — which is
   the same requirement as "the pre-adoption path stays reachable" (US-2), so it
   costs nothing extra if designed in from the start. It is a hard gate on
   shipping a console title with core adopted, and it should be said out loud
   now.
3. **Unreal's build system will not run `esbuild`.** The core bundle
   (`vendor/core/insimul_core_bundle.c`, generated by
   `tools/vendor-core-bundle.mjs` from a `packages/core` checkout) is a
   **vendored build artifact** with a recorded source commit and a `--check`
   drift guard. That is already true in `native/` and this repo inherits it for
   free — but it means the plugin's provenance story now includes "which commit
   of core is compiled into the staged binary", and `VERSION`/`THIRD_PARTY.md`
   must say so. Same discipline as the Trealla pin already in
   `Source/ThirdParty/InsimulLibrary/VERSION`.

### 4.8 The decision, restated for this repo

> **Unreal binds `libinsimulcore` through a `ThirdParty` module, exactly as it
> binds `libinsimul` today.** No second bridge, no hand-port of core to C++, no
> service boundary at runtime. The hard rule stands: **nothing on a per-frame
> path crosses the boundary** — core is the decision layer, called when a quest
> is offered or a recipe is crafted; rendering, input, animation and physics stay
> engine-side, which is what contract §3 already mandates.

**Cost of being wrong: low.** If QuickJS proves unshippable on some target, the
ABI is unchanged and option B (or a per-surface option D fallback) slots in
behind it. That reversibility is the main reason to specify the ABI rather than
the language — and it is now also this repo's console-platform answer.

---

## 5. The first slice — radiant quest generation

### 5.1 What it is

`generateRadiantQuests` — `packages/core/src/radiant/radiant-engine.ts` (547) +
`base-templates.ts` (131), **678 lines**, pinned by `conformance/radiant/*.json`:
**5 files, 11 cases** (`empty` 2, `single-slot` 3, `multi-slot` 1, `maxquests` 2,
`exclusion-cooldown` 3). Already exposed across the ABI as `radiant.generate` and
`radiant.baseTemplates`.

### 5.2 Why this one

- **The vectors are already here and byte-identical.** `conformance/radiant/`
  matches `packages/core/conformance/radiant/` exactly (`diff -rq`: no
  difference) — unlike `conformance/prolog/`, which has rotted (§6.3).
- **Nothing in this repo reads them.** `grep` for `conformance/radiant`,
  `exclusion-cooldown`, `maxquests`, `single-slot`, `multi-slot` across the whole
  repo outside `conformance/` returns **zero** hits. The gate goes from 0
  executed cases to 11, and it **cannot be a gate that silently executes
  nothing** — the thing US-3 explicitly guards against.
- **It needs no host hooks at all.** Every `EngineHostAdapter` field is optional
  and this path uses none of them. The slice tests **the language boundary and
  nothing else**. That is the point: isolate the unknown.
- **It needs no async pump** (§4.6) and **no shared KB** (§4.7.1). Its input is
  Prolog program *text* — which is precisely what
  `FInsimulRuntimeContext::Commit()` and `insimul_kb_snapshot()` already produce
  from `currentState.prologFacts`.
- **Zero regression risk is structural, not asserted.** This engine generates no
  radiant quests today. Nothing shipped can change behaviour.
- **It is genuine capability gain**, not a like-for-like swap — what contract
  §4.1 says adoption should be.
- **Godot has already proven it end to end** (11/11 through the real bundle), so
  US-2 is porting a working recipe rather than discovering one.

Explicitly *not* chosen, and why: **quest hydration and the radiant tick** (a
like-for-like swap of an area already proven equal — churn with no gain);
**crafting** (the biggest gain, but it needs `IResourceStore`, which needs the
inventory glue of §2.2 — two unknowns at once); **the language stack** (blocked
on the un-inverted modules); **save** (ours is already byte-identical to the
authority, §7).

### 5.3 How US-2 keeps both implementations reachable

US-2 requires the existing implementation to stay reachable so US-3 can diff. For
this slice **there is no existing implementation** — the honest statement, not a
shortcut. So:

- The radiant source becomes selectable — `Core` (through `libinsimulcore`) or
  `None` (today's shipped behaviour: no radiant quests, and the fallback on any
  platform without a bridge build, §4.7.2). US-3 runs both legs over the same 11
  vectors; every difference classifies as **new capability**, and a regression is
  not constructible — which the gate must **assert** (fail if the pre-adoption
  leg ever emits a quest, and fail if the gain count is zero) rather than argue.
- **Because that diff is weak, US-3 gets a second, real one at near-zero cost:**
  run the *same bridge* over `conformance/quests/hydration-cases.json` (4) and
  `radiant-cases.json` (3), which `Portable/InsimulQuestSystem.cpp` (650 lines,
  option D) **already implements**, and which the bridge already exposes as
  `quest.hydrate` / `quest.radiantTick` for exactly this purpose. That is a
  genuine two-implementation comparison over identical vectors — hand-ported C++
  vs core-through-the-bridge — and it is the evidence a future retirement of
  option D would need. It adds **no adopted surface**:
  `Portable/InsimulQuestSystem.cpp` is not replaced, only compared.

### 5.4 The adapter boundary

Per US-2's rule that no Unreal type crosses into core and translation is not
scattered:

```
templates / Blueprint / UMG
        │  FString / USTRUCT / TArray
        ▼
UInsimulRadiantSource        Public/InsimulRadiantSource.h + Private/…       ← UE-facing, thin
        │  std::string / insimul::FJsonValue
        ▼
insimul::FRadiantSource      Portable/InsimulRadiantSource.{h,cpp}           ← the ONLY translation site
        │  canonical JSON  { kb: string[], options: { seed, now, maxQuests? } }
        ▼
insimul::FInsimulCoreBridge  Private/Core/InsimulCoreBridge.{h,cpp}          ← RAII handle, mirrors InsimulKB
        │  C ABI: insimul_core_call(h, "radiant.generate", json)
        ▼
libinsimulcore  (native/corebridge/) = QuickJS + the vendored core bundle
        │  JS → C: __insimul_prolog_{create,consult,query,destroy}
        ▼
libinsimul  (Trealla, natively linked)
```

Two properties this layering buys, both required by US-2:

- **The translation site is portable and therefore host-testable.** Putting
  `FRadiantSource` in `Portable/` (not `Private/`) means the whole conversion is
  covered by a plain-`clang++` ctest target, in the pattern every other semantic
  core in this repo already follows. The `UCLASS` above it is a thin, UE-coupled
  shell — the same `Shell` split `UInsimulQuestSystemShell` already uses.
- **One-way by construction.** Core never learns an Unreal type; this repo gains
  no edge into `packages/core` beyond two vendored artifacts (the corpus here,
  the bundle inside `libinsimulcore`).

### 5.5 Gate

A new `tools/verify-unreal` ctest target — the established pattern: plain
`clang++`, no Unreal Build Tool, no engine headers — that links
`libinsimulcore`, loads `conformance/radiant/*.json`, drives each case through
the bridge, and **asserts the executed-case count is 11 and non-zero before
comparing anything**, plus that all five areas are present, that no two case
names collide, and that `core.methods` still exposes `radiant.generate`.

This is the first target in this harness that links a native library at all
(§6.5), so US-2 must also decide how the harness finds it — the same
vendored-first-then-sibling-checkout fallback `CMakeLists.txt` already uses for
the corpus directories is the obvious precedent, and Godot's
`run_radiant_tests.sh` probing `<native>/build/libinsimul.*` is the working one.

### 5.6 Abort condition

If US-2 cannot stand up the bridge and pass the 11 cases inside this tasklist,
the correct outcome is to **report the blocker and stop**. It is *not* to
hand-port `radiant-engine.ts` into `Source/InsimulRuntime/Portable/` — that is
option D again, and shipping it would answer this tasklist's central question
with the wrong answer while appearing to succeed.

---

## 6. What turned out to be false (or true, where it mattered)

Recorded because the remaining adapter tasklists will otherwise inherit them.

### 6.1 The tasklist's own figures — correct, for once

"102 files / 17.1k LOC" is **right**: `Source/InsimulRuntime/**` is exactly 102
`.h`/`.cpp` files and 17,113 lines. Godot's §6.1 found its equivalent figure off
by 23×, so this is worth stating rather than assuming.

What the figure **omits**, and what a sizing exercise needs: the `InsimulEditor`
module (61 files / 10,733) and the **C++ game template** (211 files / 39,096,
of which 92 files / 16,328 are the gameplay systems §3 compares). Total C++ under
this repo's management is **≈ 78,000 lines**, not 17k. Unreal is by a wide margin
the largest of the three engine repos, which is the correct reason for it to have
gone *second*, not first.

The contract's own figures also moved, exactly as Godot recorded: the tasklist
description cites 59 modules / 17,946 lines and a `.d.ts` `GameQuestManager`; the
merged contract says **60 modules / 18,959 lines** with `GameQuestManager` (1,013)
as real code in core behind `IQuestSeedSource`. Contract §3 still carries the
stale claim (§8 amendment 2).

### 6.2 "Unreal has no radiant quest generation" — true, and narrower than it sounds

Confirmed by grep: zero readers of `conformance/radiant/`. But this engine **does**
implement the radiant *tick* and pins it against `conformance/quests/radiant-cases.json`.
See §3.2 — the two are different capabilities and the corpus names are close
enough to mislead.

### 6.3 The vendored corpus is a 54% subset of the Prolog pack, and one file is stale

`conformance/VENDORED.md` is three lines long and calls this directory a *"mirror
of `packages/core/conformance/` (source of truth)"*. For `prolog/` it is not:

| | source | vendored here |
| --- | --- | --- |
| files | 10 | **7** |
| cases | **76** | **41** |
| missing | — | `identity.json` (11), `equivalence.json` (11), `worlds.json` (12) — the entire KINP pack |
| drifted | — | `gameplay.json`: **7 cases against the source's 8**, on pre-KINP terms |

Also absent: `predicate-schema-hash.json` and core's `content-library/` fixtures.
Also present-but-not-a-mirror: `conformance/content/` (`golden-vectors.json`,
`invalid-cases.json`, `library-basic.json`, `library-golden.json`) — core has
`content-library/{minimal,riverside-starter}.json` instead, a *different and
current* shape. These are **local fixtures mis-described as a mirror**, exactly
as Godot found. `conformance/README.md` also differs from core's.
`saves/`, `quests/`, `radiant/` and `ui/` are byte-identical and fine.

Two things make this worse here than in Godot:

1. **Nothing in this repo reads `conformance/prolog/` at all.** Godot at least
   had a marshalling gate over its 41 cases. Here the grep returns zero readers,
   so the 41 stale cases are not even being decoded — they are inert files that a
   reader would reasonably assume represent parity. The corpus this engine
   actually runs is `saves/` (51 checks), `quests/` (33), `content/` (73) and
   `ui/` (via `Source/InsimulRuntime/Tests/`, which has no build wiring, §6.4).
2. **`VENDORED.md` records no hashes and no source commit**, so nothing could
   ever have detected the drift.

**US-3 must re-vendor** (its criterion 4), and the fix is already designed:
Godot's `tools/vendor-conformance.mjs` — sha256 per mirrored file in a
`VENDORED.json`, an explicit *declared-local* list for `content/`, a `--check`
mode needing no core checkout, and a `--core` mode that does the real byte diff.
Port that, do not re-invent it. **Treat this criterion as known-failing today**
rather than something to discover.

### 6.4 The host gates that exist, the ones that are claimed, and the gap

Good news first: **all six ctest targets in `tools/verify-unreal` build and pass
in a standalone checkout** — 228 checks, 0 failures. `CMakeLists.txt` already
resolves each corpus directory vendored-first with a monorepo-sibling fallback,
which is the exact bug Godot's §6.4 had to fix in three gates. This repo got it
right.

The gap is elsewhere. `VERIFICATION.md` references **`npm run engines:check` and
nine `npm run engines:unreal:*` scripts** roughly thirty times as the merge gate.
**None of those scripts exist anywhere in the project checkout** — the root
`package.json` has no `engines:*` entries at all; they were lost when the
monorepo split into submodules. Concretely, that leaves **13 test files / 4,738
lines with no build wiring in this repo**:

| suite | files | lines | what it covers | runnable here? |
| --- | --- | --- | --- | --- |
| `tools/verify-unreal/` (ctest) | 6 | — | world source, save, quest, bootstrap, content library, round-trip — **228 checks** | **yes** |
| `tools/verify-unreal/check.mjs` | — | — | structural syntax over **382** `.h`/`.cpp` | **yes** |
| `Source/InsimulRuntime/Tests/` | 4 | 1,577 | the UI view models against `conformance/ui/*.json` | **no** |
| `Source/InsimulEditor/Tests/` | 9 | 3,161 | binding resolver, scene placement, reimport, Connect view models | **no** |

`tools/verify-unreal/README.md` and `tools/README.md`, cited by the harness's own
header comment, do not exist either; nor does the `tools/verify-unreal/host-test`
directory that `Source/ThirdParty/InsimulLibrary/lib/README.md` says links
libinsimul.

**Consequence for this tasklist:** US-2's "existing build and test gates stay
green" means *the six ctest targets and the structural gate*, and must say so
rather than implying the UI and editor suites were checked. Adding CMake targets
for those 13 files is cheap (they are the same plain-`clang++` shape) and would
be the single highest-value non-adoption work available here — but it is **not**
in this tasklist's scope, and inventing scope is how a design story stops being
one. Recorded as a finding, not a task.

### 6.5 Nothing in this repo's automated gates links libinsimul

`tools/verify-unreal/CMakeLists.txt` compiles the eight `Portable/` sources and
nothing else. `Private/Prolog/InsimulKB.cpp` (539 lines — the wrapper that
actually calls the C ABI) is covered by the **structural syntax gate only**.

The `FInsimulKB` that `test_quest_system.cpp` exercises is **not** that wrapper:
it is a 15-line in-memory ground-fact store declared in
`Portable/InsimulQuestSystem.h`, deliberately so the quest core stays
host-testable. Its own comment says the native engine "plugs in behind the same
Assert/Has query shape". That is a defensible split — identical to Godot's §6.5 —
but it means **this repo has never executed a Prolog query in an automated gate**,
and §5.5's target would be the first thing here to link a native library at all.

Where real query parity does live: `native/` (`ctest -R conformance`, 76/76,
native vs wasm32 per `native/conformance/WASM_PARITY.md`).

### 6.6 The libinsimul ABI polarity trap — checked, and this repo is clean

Godot's §6.6 found its hand-authored copy of `insimul.h` disagreed with the
shipping library on return-code polarity, and its wrapper returned `false` on
success for four calls. It ends: *"The lesson generalises to 98 and 99 — Unity
and Unreal carry their own copies of this ABI, written from the same source.
Check their polarity before trusting a green syntax gate."*

Checked. **This repo is clean on both counts:**

- `diff Source/ThirdParty/InsimulLibrary/include/insimul.h native/include/insimul.h`
  → byte-identical to the shipping header.
- Every call site in `InsimulKB.cpp` tests the correct polarity: `consult`,
  `assert` and `restore` compare `== 0` for success, and `retract` distinguishes
  `0` (removed) from `1` (nothing matched) via a three-valued `RetractResult`.

Recorded rather than silently dropped, because "we checked and it was fine" is a
result, and because tasklist 98 (Unity) still has to run the same check.

### 6.7 The libinsimul KB-lifecycle crash does not bite this engine today

Godot's §6.7 found libinsimul SIGTRAPs when a KB is created after the live count
reaches zero, which a radiant tick building throwaway KBs would hit on its second
tick. `libinsimulcore` works around it by holding one `keepalive` KB open for the
lifetime of the handle (`native/corebridge/src/insimulcore.c`, still present).

This engine's shape makes it immune *by accident*: `UInsimulPrologSubsystem`
holds **one KB for the whole `GameInstance` lifetime**, so the live count never
reaches zero mid-session. That immunity is worth knowing about and worth not
relying on — any future code that creates and releases a KB per operation
reintroduces it. The fix belongs in `native/`, which is outside this worktree.

---

## 7. What we should *not* adopt

Stated because US-1's last criterion makes "don't adopt" a valid outcome, and it
is the right outcome for most of this surface.

- **The save system.** `FInsimulSaveSystem` + `FInsimulCanonicalJson` +
  `FInsimulSha256` produce an envelope that **byte-matches a TypeScript-produced
  golden** (`tools/cross-check/cpp-produced.envelope.json`). Replacing a proof
  with a bridge crossing is a strict downgrade.
- **Prolog.** Already the same engine source on both sides. Blocker 1 is closed.
- **Rules, combat, survival.** No core counterpart, by design (contract §4.1).
- **Inventory, content library, and the corpus-pinned UI view models.** Ours are
  larger and already pinned by `conformance/ui/*.json` and
  `conformance/content/*.json`.
- **Quest hydration and the radiant tick.** Proven equal on the vectors that
  exist; swapping them is churn. US-3 will *compare* them (§5.3) precisely so a
  future tasklist can make that decision on evidence.
- **Anything needing the seven un-inverted modules** — `GamePrologEngine` (2,267),
  `LanguageProgressTracker` (1,313), `AssessmentEngine` (1,042), `CraftingSystem`
  (521), `AmbientLifeBehaviorSystem` (460), `RadiantQuestDirector` (186),
  `EquipmentManager` (111). **5,900 lines that are not in core yet.** An adapter
  that "adopts" these adopts nothing. Note this is what makes `ICombatStatSink`
  (§2.3.1) academic as well as unhostable.
- **Template quest completion, dialogue and crafting.** Real capability gains,
  real behavioural changes (§3.1). Each needs its own tasklist with its own
  before/after evidence, not a slice of this one.

---

## 8. Proposed amendments to the runtime contract

Contract §5.5 invites revision. Godot proposed four; this engine **confirms
amendments 1–4 from a second, independent implementation** — which is the
strongest signal available that the interface rather than the adapter is what
needs changing — and adds two.

1. **Split `IDataSource`.** *(Confirms Godot's 1.)* 79 `async` methods conflate
   *loading an exported world* (~20, sync-satisfiable, what every native adapter
   needs) with *authoring-server session management* (~59: playthroughs, dynamic
   quest creation, merchant inventories, NPC guidance — what only the platform
   client calls). Requiring a native adapter to implement or stub 59 methods it
   will never call is the largest avoidable cost in this plan, and it is the
   largest in Godot's too. Proposal: `IWorldSource` (the load-only subset)
   extended by `IDataSource`.
2. **Contract §3 is stale on `GameQuestManager`.** *(Confirms Godot's 2.)* §3
   still says the class is a `.d.ts` injected by the platform at export time;
   §1.1 and §2.2 say it is real code in core with `IQuestSeedSource` inverted. §3
   is what an adapter author reads to decide what they must implement themselves,
   so the stale half is the more harmful one.
3. **Say which interfaces are sync-safe.** *(Confirms Godot's 3.)* A native
   adapter needs to know which hooks may be synchronous before it designs its
   pump. `IHostLifecycle` already says "must be synchronous"; nothing else does.
4. **Declare the host builtins core imports.** *(Confirms Godot's 4.)*
   `src/save-envelope.ts` imports Node's `crypto` at module scope, which is what
   forced `js/host-crypto.js` into the bridge.
5. **`ICombatStatSink`'s Unreal guess should not name GAS.** The contract says
   "Unreal: an `UAttributeSet` / GAS attribute write". This plugin does not
   depend on `GameplayAbilities` and its template combat system is 48 lines of
   constants with no entity registry (§2.3.1). Naming GAS makes the hook read as
   "wire it to the thing Unreal already has", when the honest instruction is
   "this hook has no host and no caller; stub it". Proposal: replace the
   per-engine guess with the caller status — *`EquipmentManager` is un-inverted,
   so this interface has no consumer in any engine today.*
6. **State the deployment surface of the bridge, not just its ABI.** The contract
   describes what core provides and what a host provides, and is silent on where
   the code physically runs. Every native adapter now hits the same three
   questions and answers them privately (§4.7): which platforms have a build,
   what happens on a platform that does not, and which commit of core is baked
   into a shipped binary. One short section — *"an adapter must degrade to its
   pre-adoption path when `libinsimulcore` is unavailable, and must record the
   core commit its binary embeds"* — turns three private answers into one shared
   requirement, and it is the only place a console/mobile target gets considered
   at all.

---

## 9. What the next stories do

| story | work |
| --- | --- |
| **US-2** | (a) `ThirdParty` wiring for `libinsimulcore` + staging in `build-plugin-zip.mjs`; (b) `insimul::FInsimulCoreBridge` — the RAII C-ABI handle, game-thread-affine, the only file that touches `insimulcore.h`; (c) `insimul::FRadiantSource` in `Portable/` as the single translation site, selectable `Core` \| `None`, with `UInsimulRadiantSource` as the thin UE shell; (d) the corpus runner wired as a new ctest target. All 11 vectors passing is the bar. |
| **US-3** | (a) the 11-vector gate with the count asserted non-zero and floors on areas/names/method table; (b) re-vendor the corpus with a `tools/vendor-conformance.mjs` port — sha256 manifest, declared-local list, `--check` and `--core` modes — taking `prolog/` from 41 to 76 and adding the KINP pack; (c) the §5.3 second diff over the 7 quest vectors, hand-ported C++ vs core; (d) the retain/remove decision, with reasons. |

The scope US-1 deliberately leaves out, so a later reader does not mistake
silence for oversight: adopting crafting or the language stack (§3.1, §7),
wiring the 13 orphaned test files into a build (§6.4), and fixing the
libinsimul KB-lifecycle crash (§6.7, belongs in `native/`).

---

## Appendix A — `docs/UNIFICATION_ROADMAP.md` Decision 1

That file lives in the **project checkout**, outside this submodule, so this
tasklist cannot edit it. Decision 1 already carries the answer, recorded by
tasklist 100 — **this repo confirms it and adds nothing that changes it.** Two
edits are owed at merge time.

**1. Decision 1's "Not yet true" paragraph is now stale.** It reads:

> **Not yet true:** 100 built the bridge in `godot/gdextension/corebridge/`, not
> `native/`, because `native` is a sibling submodule outside its worktree.
> **104 promotes it** before 98/99 can bind to it; the header is the part that
> must not fork when it moves.

104 has landed. Replace with:

> **Done:** 104 promoted the bridge to `native/corebridge/` — `insimulcore.h`
> moved byte-for-byte, both artifacts (`libinsimulcore.a`, `libinsimulcore.dylib`)
> build with libinsimul under the same CMake, and `scripts/package.sh` ships them
> in `dist/<platform>/`. 99 confirms the decision from Unreal's side and binds
> that artifact through the same `ThirdParty` module pattern it already uses for
> libinsimul — see `unreal/RUNTIME_CORE_ADOPTION.md` §4 for the Unreal-specific
> costs (≈650 lines) and §4.7 for the three risks that are Unreal's alone: link
> mode determines whether there are one or two Trealla instances in the process;
> the platform matrix stops at desktop, so the plugin must degrade to its
> pre-adoption path where no bridge build exists; and the core bundle is a
> vendored build artifact whose source commit must be recorded in the shipped
> `VERSION`.

**2. The status table's row for 99.** It reads `parked`; 99 is unparked and its
design story is complete.

Two further corrections, from §6.1: Unreal's runtime module is **102 files /
17,113 lines** (the roadmap's figure is right, unlike Godot's), but the repo
totals **≈78,000 lines** of C++ once the editor module and the C++ game template
are counted — Unreal is the largest of the three engine repos by a wide margin.

---

## Appendix B — how to reproduce the measurements

```bash
# LOC (§0, §6.1)
find Source/InsimulRuntime -name '*.h' -o -name '*.cpp' | wc -l      # 102
find Source/InsimulRuntime \( -name '*.h' -o -name '*.cpp' \) -exec cat {} + | wc -l   # 17113
find templates/source \( -name '*.h' -o -name '*.cpp' \) -exec cat {} + | wc -l        # 39096

# the radiant corpus has no readers (§5.2, §6.2)
grep -rn 'conformance/radiant\|exclusion-cooldown\|maxquests\|single-slot\|multi-slot' . \
  --exclude-dir=.git --exclude-dir=conformance --exclude-dir=node_modules   # only .chief/state

# the prolog corpus has no readers either (§6.3)
grep -rn 'conformance/prolog' . --exclude-dir=.git --exclude-dir=conformance --exclude-dir=node_modules

# corpus drift (§6.3) — CORE is the packages/core checkout
diff -rq conformance/ "$CORE/conformance/"
node -e "const fs=require('fs');let t=0;for(const f of fs.readdirSync('conformance/prolog'))
  t+=JSON.parse(fs.readFileSync('conformance/prolog/'+f,'utf8')).cases.length;console.log(t)"   # 41 (source: 76)

# the ABI header and its polarity (§6.6)
diff Source/ThirdParty/InsimulLibrary/include/insimul.h "$NATIVE/include/insimul.h"   # identical
grep -n 'insimul_kb_consult\|insimul_kb_assert\|insimul_kb_retract\|insimul_kb_restore' \
  Source/InsimulRuntime/Private/Prolog/InsimulKB.cpp

# the gates that exist (§6.4). All green in this worktree.
cmake -S tools/verify-unreal -B build && cmake --build build
ctest --test-dir build --output-on-failure    # 6/6: 28+51+33+43+57+16 = 228 checks
node tools/verify-unreal/check.mjs            # 382 files structurally sound

# the gates VERIFICATION.md claims (§6.4) — none of these scripts exist
grep -n '"engines' /path/to/project/package.json    # no matches

# the bridge this plan binds (§4)
ls "$NATIVE/corebridge"                             # include js src tools vendor CMakeLists.txt
grep -n "'radiant\.\|'quest\." "$NATIVE/corebridge/js/entry.js"
```
