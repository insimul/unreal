# Vendored conformance corpus

A byte-for-byte mirror of `@insimul/core` `packages/core/conformance/` (the
cross-runtime source of truth), plus a small number of fixtures that are this
repository's own. Which is which is recorded in **`VENDORED.json`**, not in prose:

- `files` — every mirrored path, with its sha256 and the core commit it came from.
- `local` — this repo's own fixtures, which mirror nothing (`content/*`).

## Verifying and re-vendoring

```sh
# needs no core checkout — runs in this repo's gates (ctest `corpus_manifest`)
node tools/vendor-conformance.mjs --check

# the real drift check: byte-for-byte against a core checkout
node tools/vendor-conformance.mjs --check --core ../babylon/packages/core

# re-vendor and rewrite the manifest
node tools/vendor-conformance.mjs --core ../babylon/packages/core
```

`--check` fails if a mirrored file is missing or hashes differently, if the
Prolog case count moved, or if a file appears under `conformance/` that is
neither mirrored nor declared local.

## Why the manifest exists

This file used to be three lines asserting the directory was a mirror, and
nothing verified it. Measured at the start of tasklist 99 it was not one:

| | before US-3 | after |
| --- | --- | --- |
| mirrored files | 27, unverified | **34, hash-pinned** |
| `prolog/` cases | 41 of 76 (54%) | **76 (100%)** |
| KINP `identity` / `equivalence` / `worlds` | absent | present (34 cases) |
| `prolog/gameplay.json` | pre-KINP atoms | `id/3` terms |
| `predicate-schema-hash.json`, `content-library/` | absent | mirrored |
| `README.md` | described tau-prolog as a second engine | current |
| `content/*` | claimed to mirror a core dir that does not exist | declared local |

The guard matters more than the copy: the rot happened because nothing ever ran
the diff. `tools/vendor-conformance.mjs` is a deliberate port of Godot's script
of the same name — one mechanism across the engine repos, not three lookalikes.
The mirrored set here is byte-identical to the one Godot carries (same 34 files,
same 34 hashes), which is what "one corpus, three runtimes" is supposed to mean.
