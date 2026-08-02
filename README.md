# Insimul for Unreal Engine

> An Unreal Engine plugin that gives your NPCs real, streaming AI conversations —
> with voice, quests, and a live logic engine — online or fully offline.

Drop this plugin into an Unreal Engine project and a placed character can hold an
open-ended, streaming conversation with the player: the reply arrives token by token,
optionally spoken aloud with lip-sync, and the character stays in character because
it carries its own personality, relationships, and knowledge. It's for game and
world builders who want believable, talkable NPCs without wiring an LLM, a
speech pipeline, and a dialogue UI together by hand.

The plugin is the **Unreal Engine integration of Insimul** — a larger system for
building fictional worlds and turning them into games — but you do not need to know
anything about that system to use it here. Everything below stands on its own.

## What it gives you

- **Streaming NPC conversations.** Attach one component and an actor can talk. The
  reply streams in live; NPCs can also strike up conversations with *each other* when
  they get close.
- **Voice, in and out.** Text-to-speech playback with viseme-driven lip-sync, and
  optional speech-to-text so the player can talk back — each independently switchable.
- **Runs online *or* offline.** Point it at a hosted **Insimul server** for the
  richest behavior, or run entirely on the player's machine against a **local LLM**
  (Ollama / llama.cpp) with an exported world file and no network at all.
- **A real logic engine.** A built-in **Prolog** knowledge base you can query and
  update at runtime — world rules and facts as logic clauses, not a maze of `if`s.
- **Quests and a full game UI.** Ready-made quest tracking plus a default HUD, menus,
  inventory, save/load, and dialogue UI you can restyle or replace.
- **Editor tools.** In-editor panels to browse worlds on a backend and chat-test any
  character, plus a pipeline that turns a generated world into a native, hand-editable
  Unreal scene (Landscape, buildings, PCG vegetation, baked NavMesh).

## The problem it solves

An AI NPC is not one feature — it's an LLM call, a streaming transport, text-to-speech,
lip-sync, a place to keep the character's knowledge, a dialogue widget, and a way to
switch all of that between a cloud service and a local model. Build it yourself and
most of the effort is plumbing, done again for every project. This plugin is that
plumbing, already assembled and switchable from a settings panel, so your work is the
game.

## How it works

A few ideas run through the whole plugin.

- **Providers are pluggable and per-concern.** *Where* the LLM runs (`ChatProvider`),
  *where* speech is synthesized (`TTSProvider`), and *where* the player's voice is
  transcribed (`STTProvider`) are three independent settings — each `Server`, `Local`,
  or (for speech) `None`. Switching from a hosted backend to a local model is a config
  change, not a code change.
- **A world drives the characters.** Characters and their *dialogue contexts* (a
  pre-built prompt carrying personality, relationships, and known facts) come from a
  **world** — fetched live from the server, or read from a JSON file you exported for
  offline play. See [`docs/world-data.md`](docs/world-data.md).
- **Two modules.** `InsimulRuntime` is the gameplay layer you ship in a game;
  `InsimulEditor` is a set of editor-only authoring tools. You can use the runtime
  without ever touching the editor tools.
- **Portable cores, thin engine seams.** Under the hood, the decision logic
  (conversation state machines, quest rules, scene-placement math, the save format)
  lives in plain, engine-free C++ that is unit-tested on its own; the Unreal-specific
  code is a thin layer on top. That's why behavior matches the other Insimul engine
  integrations exactly — but as a *user* you just see components and Blueprint nodes.

## Getting started

**Requirements:** Unreal Engine (this is a source plugin — it builds against your
installed engine version) and a C++-enabled project.

1. Copy the `Insimul/` folder into your project's `Plugins/` directory.
2. Enable it in your `.uproject`:

   ```json
   { "Name": "Insimul", "Enabled": true }
   ```

3. Add `"InsimulRuntime"` to your module's `PublicDependencyModuleNames` in
   `*.Build.cs`.
4. Point the plugin at a world in `Config/DefaultGame.ini`:

   ```ini
   [/Script/InsimulRuntime.InsimulSettings]
   ServerURL=http://localhost:8080
   DefaultWorldID=your-world-id
   bPreferWebSocket=true
   ```

5. Create a Blueprint child of `InsimulDialogueWidget` for your dialogue UI.
6. Create a Blueprint child of `InsimulAICharacter` and set its `DialogueWidgetClass`.
7. Place an `InsimulSpawner` in your level.

Press Play, walk up to a spawned NPC, and interact — the dialogue widget opens and the
conversation streams. All settings are also editable under **Project Settings ▸
Plugins ▸ Insimul**.

### A minimal conversation in C++

You rarely need to touch the conversation component directly, but this is the whole
loop — start a conversation, receive streamed utterances:

```cpp
UInsimulConversationComponent* Convo =
    NPC->FindComponentByClass<UInsimulConversationComponent>();

Convo->OnUtteranceReceived.AddDynamic(this, &AMyActor::HandleUtterance);
Convo->StartConversation();               // uses the resolved world + character IDs
Convo->SendPlayerMessage(TEXT("Hello — what do you sell?"));
```

Want it to run with no server? Export the world to a JSON file, set
`ChatProvider=Local`, and point it at a local Ollama or llama.cpp endpoint — the same
component drives it. The [runtime guide](docs/runtime.md) walks through both.

## Learn by example

The best way to see the plugin end to end is the **runtime guide**
([`docs/runtime.md`](docs/runtime.md)) — it follows a conversation from a player line
to a streamed, spoken reply, then documents every provider, mode, setting, and class:
server vs. local, the settings tables, world-ID resolution, the component/actor/widget
catalogue, and the backend endpoints.

## Repository layout

| Path | Contents |
| --- | --- |
| `Source/InsimulRuntime/` | The **gameplay runtime** — conversation components, AI-character actors, dialogue widgets, quest system, the Prolog subsystem, the default UI, and the online/offline networking clients. |
| `Source/InsimulEditor/` | **Editor-only tools** — the backend-connected panels (World Browser, Generation Console, Conversation Tester) and the scene-generation / re-import pipeline. |
| `Source/ThirdParty/` | Vendored native dependencies — `libinsimul` (the Prolog engine) and `nlohmann/json`. |
| `templates/` | A game-template project skeleton the Insimul export pipeline copies into generated games — see [`docs/templates-and-release.md`](docs/templates-and-release.md). |
| `conformance/` | Language-neutral test fixtures shared across all Insimul engine integrations, so behavior stays identical between them. |
| `data/` | Data-driven assets (the PCG vegetation graph descriptor, placeholder-pack recipe). |
| `tools/` | The host-side test harness that verifies the engine-free cores without a full Unreal build. |
| `scripts/` | Release packaging (`build-plugin-zip.mjs`). |
| `docs/` | The guides below. |

## Going deeper

Focused guides in [`docs/`](docs/), by topic:

- **[`docs/runtime.md`](docs/runtime.md)** — the full runtime reference: providers &
  execution modes, every setting, world-ID resolution, the class catalogue, and
  server endpoints.
- **[`docs/prolog.md`](docs/prolog.md)** — the in-game Prolog knowledge base: the
  Blueprint/C++ API, reading solutions, save/restore, and thread rules.
- **[`docs/world-data.md`](docs/world-data.md)** — the world-export JSON format and,
  for contributors, how the C++ types map to their schemas.
- **[`docs/ui.md`](docs/ui.md)** — the default game UI: panel registry, theme tokens,
  and the quest / trade / dialogue / menu / save-load panels.
- **[`docs/editor-connect.md`](docs/editor-connect.md)** — the editor panels (World
  Browser, Generation Console, Conversation Tester), the shared backend session, and
  where the auth token is stored (and why it never lands in version control).
- **[`docs/scene-generation.md`](docs/scene-generation.md)** — turning a world into a
  native Unreal scene: terrain, roads, buildings, PCG vegetation, placeholder assets.
- **[`docs/reimport.md`](docs/reimport.md)** — re-importing an updated world without
  clobbering hand edits, plus the Binding Editor.
- **[`docs/templates-and-release.md`](docs/templates-and-release.md)** — the game-
  template tree and how the plugin is packaged for distribution.

And the reference documents at the repository root:

- **[`CHANGELOG.md`](CHANGELOG.md)** — release history (this package is versioned
  independently).
- **[`MIGRATION.md`](MIGRATION.md)** — behavioural changes as the portable core
  replaced the earlier template prototypes (e.g. the world-source loader, the
  `DataLoader` deprecation path).
- **[`VERIFICATION.md`](VERIFICATION.md)** — the human in-editor verification
  checklists for the parts that need a real Unreal editor to test (the Prolog
  subsystem, the full gameplay loop, the editor panels, scene generation).
- **[`RUNTIME_CORE_ADOPTION.md`](RUNTIME_CORE_ADOPTION.md)** — the adoption plan
  for the shared runtime core (`@insimul/core`): what this plugin already
  implements that core also implements, what core expects from a host and how
  much of it exists here, how C++ reaches TypeScript, and which slice is adopted
  first.

## License

Apache 2.0 — see [`LICENSE`](LICENSE).
