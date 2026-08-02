# Runtime guide — talking NPCs, quests, and configuration

This is the reference for the **in-game** side of the plugin: how an NPC holds a
conversation, where the LLM and speech run, and every setting that controls it. If
you just want the shortest path to a talking NPC, the README's *Getting started*
covers that; come here when you need the full picture.

The runtime lives in the **`InsimulRuntime`** module. You add its components to your
own actors (or use the ready-made ones), point the plugin at a source of characters
and dialogue, and the component drives the whole turn — sending the player's line,
streaming the reply back token by token, and (optionally) playing synthesized speech
with lip-sync.

## Where the work runs: three providers, chosen independently

The plugin never hard-codes *where* inference happens. You pick a provider for each
of the three concerns — **Chat** (the LLM), **TTS** (text→speech), and **STT**
(speech→text) — separately, exactly as the JavaScript SDK (`@insimul/typescript`)
does.

| Setting | Options | Default | What it selects |
| --- | --- | --- | --- |
| `ChatProvider` | `Server`, `Local` | `Server` | Where LLM inference runs |
| `TTSProvider` | `Server`, `Local`, `None` | `Server` | Where TTS audio is synthesized |
| `STTProvider` | `Server`, `Local`, `None` | `None` | Where player voice is transcribed |

That gives you two headline ways to run:

- **Server mode** (`ChatProvider = Server`) — talk to a running **Insimul server**.
  Conversations stream over a WebSocket (with a REST fallback); the server handles
  LLM inference, TTS, STT, and viseme (lip-sync) generation. Best fidelity, needs a
  backend.
- **Offline / local mode** (`ChatProvider = Local`) — no server at runtime. The
  plugin reads a world you exported to JSON and calls a **local LLM** (Ollama or
  llama.cpp). Ship a game that runs entirely on the player's machine.

### Server mode

```ini
[/Script/InsimulRuntime.InsimulSettings]
ChatProvider=Server
TTSProvider=Server
ServerURL=http://localhost:8080
DefaultWorldID=your-world-id
bPreferWebSocket=true
```

Characters are fetched from the server at `GET /api/worlds/{worldId}/characters`.
Conversations stream via `/ws/conversation` (text + audio + visemes), falling back
to REST if the socket is unavailable.

### Offline / local mode

Uses **exported world data** plus a local LLM server. No server connection is needed
once the world is exported.

1. Export the world data while a server is running (see [`world-data.md`](world-data.md)
   for the JSON shape):
   ```bash
   curl http://localhost:8080/api/conversation/export/YOUR_WORLD_ID \
     > Content/InsimulData/world_export.json
   ```
2. Install a local LLM and pull a model:
   ```bash
   brew install ollama
   ollama pull mistral
   ```
3. Configure:
   ```ini
   [/Script/InsimulRuntime.InsimulSettings]
   ChatProvider=Local
   TTSProvider=Local
   LocalLLMServerURL=http://localhost:11434/api/generate
   LocalLLMModel=mistral
   WorldDataPath=InsimulData/world_export.json
   ```

The offline provider builds prompts from the exported `dialogueContexts` — pre-built
system prompts carrying each character's personality, relationships, and knowledge —
and calls the local LLM. For offline **TTS**, install the **Runtime Text To Speech**
plugin from Fab (Piper/Kokoro ONNX voices).

The local endpoint format is inferred from the URL:

| URL contains | Format |
| --- | --- |
| `/api/generate` | Ollama |
| `/api/chat` | Ollama chat |
| anything else | llama.cpp |

## Configuration reference

All settings live in **Project Settings ▸ Plugins ▸ Insimul**, stored in
`Config/DefaultGame.ini` under `[/Script/InsimulRuntime.InsimulSettings]`.

### Server settings (`ChatProvider = Server`)

| Setting | Description | Default |
| --- | --- | --- |
| `ServerURL` | Insimul server base URL | `http://localhost:8080` |
| `DefaultWorldID` | World ID for character loading and conversations | `default-world` |
| `APIKey` | Optional authentication key | *(empty)* |
| `bPreferWebSocket` | Use WebSocket streaming (recommended) | `true` |

### Local LLM settings (`ChatProvider = Local`)

| Setting | Description | Default |
| --- | --- | --- |
| `LocalLLMServerURL` | Local LLM endpoint (Ollama or llama.cpp) | `http://localhost:11434/api/generate` |
| `LocalLLMModel` | Model name (for Ollama) | `mistral` |
| `WorldDataPath` | Path to exported JSON (relative to `Content/`) | `InsimulData/world_export.json` |
| `MaxTokens` | Max response tokens | `256` |
| `Temperature` | LLM creativity (0.0–2.0) | `0.7` |

### Local TTS settings (`TTSProvider = Local`)

| Setting | Description | Default |
| --- | --- | --- |
| `LocalVoiceModel` | Piper/Kokoro voice model name (Runtime TTS plugin) | `en_US-amy-medium` |
| `LocalSpeakerIndex` | Speaker index in multi-speaker models | `0` |

### Common

| Setting | Description | Default |
| --- | --- | --- |
| `LanguageCode` | Default BCP47 language code | `en` |

### Per-component overrides

Anything on `InsimulConversationComponent.Config` overrides the global default for
that one NPC:

| Property | Description | Default |
| --- | --- | --- |
| `APIBaseUrl` | Server URL override | `UInsimulSettings::ServerURL` |
| `WorldID` | World ID override | `UInsimulSettings::DefaultWorldID` |
| `CharacterID` | This NPC's Insimul character ID | *(empty — set by spawner)* |
| `PlayerCharacterID` | Player's character ID | `"player"` |
| `ConversationCheckInterval` | NPC↔NPC proximity check interval (seconds) | `5.0` |
| `ConversationRadius` | NPC↔NPC conversation trigger distance (units) | `300.0` |

### Which world loads: world-ID resolution

The world ID decides which characters load and which world context frames the
conversation. It resolves most-specific-first:

1. **Per-component** — `InsimulConversationComponent.Config.WorldID`, if set.
2. **Per-spawner** — `InsimulSpawner.WorldID`, copied into each spawned NPC.
3. **Global default** — `UInsimulSettings::DefaultWorldID` from `DefaultGame.ini`.

So set `DefaultWorldID` for a project-wide default, `WorldID` on a spawner for
per-level control, or `Config.WorldID` on one component for a single NPC.

## Key classes

The runtime module in one table, grouped by role.

### Components

| Class | Description |
| --- | --- |
| `UInsimulConversationComponent` | Core conversation driver — attach to any actor. Routes through server (WebSocket/REST) or local LLM based on settings; also handles NPC↔NPC proximity conversations. Delegates: `OnConversationStarted`, `OnUtteranceReceived`, `OnConversationEnded`, `OnAudioChunkReceived`. |
| `UInsimulCharacterMappingComponent` | Maps an Unreal actor to an Insimul character ID. Auto-assigned by the crowd integration subsystem. |
| `UInsimulAudioCaptureComponent` | Captures player microphone audio for STT. |
| `UInsimulDebugComponent` | Debug visualization for Insimul characters. |

### Actors

| Class | Description |
| --- | --- |
| `AInsimulAICharacter` | NPC with an `InsimulConversationComponent`, a 200-unit `InteractionSphere` (`USphereComponent`), and a `SpeechAudioComponent` (`UAudioComponent`). Creates the dialogue widget on player interaction and plays TTS via `USoundWaveProcedural`. |
| `AInsimulSpawner` | Spawns NPCs at configured or server-fetched locations. Properties: `WorldID`, `bAutoSpawnAI`, `bFetchCharactersFromServer`, `AICharacterClass`, `CharacterSpawnData`. |
| `AInsimulLevelScriptActor` | Level-script spawn hook for simple setups. |

### Widgets

| Class | Description |
| --- | --- |
| `UInsimulDialogueWidget` | Abstract base for the player dialogue UI. Blueprint events: `BP_AddUtterance(Speaker, Text)`, `BP_ClearHistory()`. Functions: `SubmitPlayerMessage(Message)`, `CloseDialogue()`. |
| `UInsimulQuestWidget` | Quest-list display widget with auto-refresh. |

The plugin also ships a full **default game UI** (HUD, menus, inventory, save/load,
quest journal, dialogue) built on engine-neutral view-models — see
[`ui.md`](ui.md).

### Subsystems

| Class | Description |
| --- | --- |
| `UInsimulCharacterMappingSubsystem` | World subsystem. Manages the character-ID pool; auto-loads from server (online) or JSON (offline) at startup. `LoadInsimulCharacters(ServerURL)` / `LoadInsimulCharactersFromFile(FilePath)`. |
| `UInsimulCrowdIntegration` | GameInstance subsystem. Auto-adds `InsimulCharacterMappingComponent` to spawned crowd actors. `EnableAutomaticMapping(bool)`. |
| `UInsimulQuestManager` | GameInstance subsystem. Quest tracking and UI management. |
| `UInsimulPrologSubsystem` | GameInstance subsystem. A **real Prolog** knowledge base (backed by `libinsimul`). Consult / assert / retract / query / snapshot over the game world's logic — see [`prolog.md`](prolog.md). |

### Networking

| Class | Description |
| --- | --- |
| `FInsimulWSClient` | WebSocket client for `/ws/conversation` — streaming text, audio, visemes. |
| `FInsimulRestClient` | REST HTTP fallback for conversation lifecycle, TTS, STT, character CRUD. |
| `FInsimulOfflineProvider` | Local LLM client (Ollama/llama.cpp), same delegate interface as `FInsimulWSClient`. |

### Data

| Class | Description |
| --- | --- |
| `UInsimulSettings` | Config singleton (`UInsimulSettings::Get()`) holding all settings above. |
| `FInsimulExportedWorld` | Exported world data: characters + dialogue contexts. |
| `FInsimulDialogueContext` | Per-character: system prompt, greeting, voice, truths. |
| `FInsimulWorldExportLoader` | Loads world data from JSON (single-file or split-file layout). |

## Triggering a conversation

`AInsimulAICharacter` carries a `USphereComponent` (`InteractionSphere`, 200-unit
radius) that fires `OnPlayerInteract` when a player pawn overlaps. Call
`HandlePlayerInteract(Pawn)` to open the dialogue widget and start the conversation.

If your game already has its own interaction system, just bind your interaction
event to call `HandlePlayerInteract(Pawn)` on the NPC — you don't have to use the
built-in sphere.

## Server endpoints

When `ChatProvider = Server`, the plugin talks to these backend endpoints:

| Endpoint | Method | Purpose |
| --- | --- | --- |
| `/ws/conversation` | WebSocket | Streaming conversations (text + audio + visemes) |
| `/api/conversations/start` | POST | Start conversation (REST fallback) |
| `/api/conversations/{id}/continue` | POST | Get next utterance (REST fallback) |
| `/api/conversations/{id}/end` | POST | End conversation |
| `/api/worlds/{worldId}/characters` | GET | Fetch characters for the spawner |
| `/api/conversation/export/{worldId}` | GET | Export world data for offline mode |
| `/health` | GET | Server health check |
