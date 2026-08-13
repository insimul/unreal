# `Content/Data/insimul/` — the data this game's mechanics and its UI run on

Four things an exported game reads at boot. The first two are byte-for-byte mirrors
of something `@insimul/core` emits and should not be edited here (a gate in this
plugin fails if one drifts); the last two are this repository's own.

| Directory | What it is | Re-vendor with |
| --- | --- | --- |
| `modules/genre-activation.json` | Genre bundle → active modules, their rule packs, their host interfaces. Core emits it with `npm run module-activation`. | `node tools/vendor-conformance.mjs --core <packages/core>`, then copy it here |
| `packs/*.pl` + `PACKS.json` | The rule packs themselves — core's own Prolog, with a sha256 and consult order per pack. | `node tools/vendor-packs/vendor-packs.mjs --core <packages/core> --write` |
| `ui/panels.json` | The default-UI panel catalog: panel key → widget, and the module that OWNS each panel. This one is this port's own — core emits no UI surface per module. | edit it — that is the whole of "move a panel under a different module" |
| `scenarios/*.json` | The sample scene's script. This one IS this repository's own; it is not mirrored from core. | edit it — `AInsimulMechanicSampleScene` and ctest `activation_witness` both read it |

## How they are used

`UInsimulModuleActivator` (a game instance subsystem) reads the genre out of
`Content/Data/WorldIR.json` (`meta.genreConfig.id`), looks it up in
`modules/genre-activation.json`, and consults exactly the packs that entry names —
**in `PACKS.json`'s `consultOrder`**, which is a hard constraint rather than a
preference: a `:- dynamic` arriving after a clause for the same predicate is a
`permission_error`.

A module the bundle does not select costs something, and that is the point: its pack
is never consulted (its vocabulary does not exist in the KB at all),
`UInsimulMechanicHostBinder` unregisters every host interface no active module names,
and `UInsimulUIPanelSurface` withholds the panels `ui/panels.json` says that module
owns — a panel over predicates with no solutions is an empty box, so the UI answers
the same question the KB does. A genre core has never heard of gets no module-owned
panel; a game that declares no genre at all is UNGATED, exactly as its pack consult
is (it activates every pack), and the boot log says which of the two happened.

## Why the pack text is shipped as data at all

Core's module contract §7.2 says a plugin "reads the genre out of the World IR, looks
it up in that file, and knows which rule packs to consult" — and stops there. The
TEXT of a pack is a TypeScript constant inside core's bundle, and the C ABI carries no
row that returns one. Vendoring is the workaround, and
`RUNTIME_CORE_ADOPTION.md` §14.1 is the write-up, including the `prolog.packs` bridge
row that should delete this directory.

## Checks

```sh
cd tools
npm run check:packs        # the packs hash what PACKS.json records
npm run check:activation   # the table, the resolution, and no hardcoded module list
npm run check:host         # ctest ui_registry — the panel catalog and the module gate
npm run check:host:binaries # ctest activation_witness — the KB witness + the scene
```
