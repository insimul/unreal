# `Content/Data/insimul/` — the data this game's mechanics run on

Three vendored things an exported game reads at boot. None of them is hand-written,
and none of them should be edited here: each is a byte-for-byte mirror of something
`@insimul/core` emits, and a gate in this plugin fails if one drifts.

| Directory | What it is | Re-vendor with |
| --- | --- | --- |
| `modules/genre-activation.json` | Genre bundle → active modules, their rule packs, their host interfaces. Core emits it with `npm run module-activation`. | `node tools/vendor-conformance.mjs --core <packages/core>`, then copy it here |
| `packs/*.pl` + `PACKS.json` | The rule packs themselves — core's own Prolog, with a sha256 and consult order per pack. | `node tools/vendor-packs/vendor-packs.mjs --core <packages/core> --write` |
| `scenarios/*.json` | The sample scene's script. This one IS this repository's own; it is not mirrored from core. | edit it — `AInsimulMechanicSampleScene` and ctest `activation_witness` both read it |

## How they are used

`UInsimulModuleActivator` (a game instance subsystem) reads the genre out of
`Content/Data/WorldIR.json` (`meta.genreConfig.id`), looks it up in
`modules/genre-activation.json`, and consults exactly the packs that entry names —
**in `PACKS.json`'s `consultOrder`**, which is a hard constraint rather than a
preference: a `:- dynamic` arriving after a clause for the same predicate is a
`permission_error`.

A module the bundle does not select costs something, and that is the point: its pack
is never consulted (its vocabulary does not exist in the KB at all) and
`UInsimulMechanicHostBinder` unregisters every host interface no active module names.

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
npm run check:host:binaries # ctest activation_witness — the KB witness + the scene
```
