# The Prolog subsystem — a real logic engine in your game

Most game "knowledge bases" are a bag of booleans and a pile of `if` statements.
This plugin ships something different: a genuine **Prolog** engine your game can
`consult`, `assert`, `retract`, and `query` at runtime, so world rules and facts
live as logic clauses instead of hand-written branching. It is a real unification
engine — `libinsimul` (Trealla under the hood) — not the substring matcher some
earlier prototypes used.

`UInsimulPrologSubsystem` is a **GameInstance subsystem** that owns one long-lived
knowledge base (KB) per GameInstance. The subsystem itself is a **thin marshalling
layer**: all engine logic lives in the Unreal-Engine-free `insimul::InsimulKB` core
(`Source/InsimulRuntime/Private/Prolog/InsimulKB.h`); the subsystem only converts
between Unreal's reflected types and that core and enforces game-thread affinity.

## Getting the subsystem

**Blueprint:** `Get Game Instance Subsystem` → class `InsimulPrologSubsystem`.

**C++:**
```cpp
UInsimulPrologSubsystem* Prolog =
    GetGameInstance()->GetSubsystem<UInsimulPrologSubsystem>();
```

The KB is created in `Initialize` and released in `Deinitialize` — one KB for the
life of the GameInstance. Always check `IsPrologReady()` before use.

## Blueprint / C++ surface

| Function | Kind | Description |
| --- | --- | --- |
| `ConsultWorldData(PrologSource)` → `bool` | Callable | Load a block of Prolog clauses/directives (e.g. the exported world `*.pl`). `false` on syntax error — nothing is loaded then (see `GetLastError`). |
| `AssertFact(Fact)` → `bool` | Callable | Assert one clause as term text **without** a trailing `.` (e.g. `quest(find_sword, active)`). |
| `RetractFact(Fact)` → `bool` | Callable | Retract the first clause unifying with `Fact`. `true` only when a clause was actually removed; a no-match is `false` without setting an error. |
| `QueryFirst(Goal, out Binding)` → `bool` | Callable | First solution of `Goal` into `Binding`. `false` on zero solutions **or** a start error — disambiguate via `GetLastError` (empty vs set). |
| `QueryAll(Goal, out Solutions)` → `bool` | Callable | Every solution of `Goal`. `false` only if the query failed to start; `true` with an empty array means zero solutions. |
| `SnapshotToString()` → `FString` | Callable | Serialize the KB's dynamic state to a canonical Prolog-text image (for a save file). `""` on error. **Clauses only — not `:- op/3` directives.** |
| `RestoreFromString(Image)` → `bool` | Callable | Replace the KB's dynamic state from a snapshot image. `false` on a malformed image (KB unchanged). |
| `GetLastError()` → `FString` | Pure | Message for the last operation, or `""` on success. |
| `IsPrologReady()` → `bool` | Pure | `true` once the KB is created and ready. |
| `GetPrologVersion()` → `FString` | Pure (static) | `libinsimul` version string. |
| `GetBoundValue(Binding, VarName, out Value)` → `bool` | Pure (static) | Look up a named variable in a binding set. |

Goals and facts are term text **without** a trailing `.` (e.g. `parent(tom, X)`).

## Reading solutions

A solution is an `FInsimulPrologBinding` — an array of `FInsimulPrologVar`
(`Name` + `Value`). `FInsimulPrologValue` is a flattened term:

- `Type` (`EInsimulPrologValueType`: `Atom` / `Int` / `Float` / `List` /
  `Compound` / `Null`)
- `Text` — atom text or a compound's functor
- `IntValue` / `FloatValue` — numeric payloads
- `DisplayString` — the canonical rendered term (always populated)
- `Elements` — rendered list items / compound arguments

Use the static `GetBoundValue(Binding, "X", ...)` to pull a variable out of a
solution.

## Example (C++)

```cpp
UInsimulPrologSubsystem* Prolog =
    GetGameInstance()->GetSubsystem<UInsimulPrologSubsystem>();
if (!Prolog || !Prolog->IsPrologReady()) return;

// Load world data + assert a runtime fact.
Prolog->ConsultWorldData(TEXT("parent(tom, bob). parent(bob, ann)."));
Prolog->AssertFact(TEXT("quest(find_sword, active)"));

// Query every solution.
TArray<FInsimulPrologBinding> Solutions;
if (Prolog->QueryAll(TEXT("parent(tom, X)"), Solutions))
{
    for (const FInsimulPrologBinding& Sol : Solutions)
    {
        FInsimulPrologValue X;
        if (UInsimulPrologSubsystem::GetBoundValue(Sol, TEXT("X"), X))
        {
            UE_LOG(LogTemp, Log, TEXT("child = %s"), *X.DisplayString);
        }
    }
}

// Save / load round-trip.
FString Image = Prolog->SnapshotToString();
// ... later, into a fresh KB ...
Prolog->RestoreFromString(Image);
```

> **Thread affinity:** the KB is single-thread-owned. Every call must run on the
> game thread — off-thread calls are logged and ignored (they return `false` / `""`).

## What is already in the KB — the active module's rule packs

An exported game does not start with an empty knowledge base. At boot,
`UInsimulModuleActivator` reads the genre out of `Content/Data/WorldIR.json`
(`meta.genreConfig.id`), looks it up in the table core emitted
(`Content/Data/insimul/modules/genre-activation.json`) and **consults exactly the
rule packs the modules that genre selects own** — combat's reach and legality rules,
perception's detection rules, and so on — in core's own consult order.

Two consequences are worth knowing before you query anything:

- **A module the bundle did not select is ABSENT, not empty.** Its pack was never
  consulted, so `current_predicate(can_afford_stamina/2)` has no solutions in a world
  whose genre does not select the module that owns it — and a goal naming that
  vocabulary **raises** rather than failing. Check `current_predicate/1` first if you
  are writing rules that may span modules.
- **The active set is data.** Adding a module to a genre bundle is a change in core
  plus a re-vendor here; no engine code names a mechanic. The boot log
  (`LogInsimulActivation`) prints the genre, where it came from, the modules, the
  packs consulted and the packs deliberately skipped.

The packs themselves are vendored under `Content/Data/insimul/packs/` with a
`PACKS.json` that pins core's commit and a sha256 per pack — see that directory's
`README.md`, and `RUNTIME_CORE_ADOPTION.md` §14 for why the text is shipped as data
at all.

## Building it: the native library

The engine is a native library, `libinsimul`, vendored under
`Source/ThirdParty/InsimulLibrary/`. To build and run the Prolog features in a real
editor, stage the library for your platform into
`Source/ThirdParty/InsimulLibrary/lib/<Mac|Linux|Win64>/` — see that module's
`lib/README.md`. The in-editor smoke checklist for the subsystem is in
[`../VERIFICATION.md`](../VERIFICATION.md) (US-XP3); the activation pass above is
US-M2 in the same file.
