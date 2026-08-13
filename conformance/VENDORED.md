# Vendored conformance corpus

A byte-for-byte mirror of `@insimul/core` `packages/core/conformance/` (the
cross-runtime source of truth), plus a small number of fixtures that are this
repository's own. Which is which is recorded in **`VENDORED.json`**, not in prose:

- `files` — every mirrored path, with its sha256 and the core commit it came from.
- `local` — this repo's own fixtures, which mirror nothing (`content/*`).
- `cases` — the exact per-area case count of the checked-in tree.
- `caseFloor` — the largest count ever vendored for each area. It never drops on
  its own.

Corpora that exist in core and are deliberately **not** vendored here are declared
in `NOT_MIRRORED` in `tools/vendor-conformance.mjs`, each with its reason, and are
printed with a file count on every `--core` run. Today: `editor/`, `generation/`,
`ai/`, `map/`, `grounding/` — see `RUNTIME_CORE_ADOPTION.md` §13.1.

## Verifying and re-vendoring

```sh
# needs no core checkout — runs in this repo's gates (ctest `corpus_manifest`)
node tools/vendor-conformance.mjs --check

# the real drift check: byte-for-byte against a core checkout
node tools/vendor-conformance.mjs --check --core ../babylon/packages/core

# re-vendor and rewrite the manifest
node tools/vendor-conformance.mjs --core ../babylon/packages/core
```

`--check` fails if a mirrored file is missing or hashes differently, if any area's
case count moved or fell below its floor, or if a file appears under
`conformance/` that is neither mirrored nor declared local.

## The floor, and why hashes were not enough

Hashes cannot see a corpus **shrink**. A file whose `cases` array is trimmed and
re-hashed passes every other check in the script — the manifest simply records the
new hash and the new mirror is "consistent". `cases` catches an edit to the
checked-in tree; `caseFloor` catches a *re-vendor* that legitimately rewrote
`cases` downward, because the floor is not rewritten downward with it. A re-vendor
that would lower one fails and names the area:

```
vendor-conformance: CORPUS SHRANK: combat: 23 case(s) vendored before, 19 now
```

`--allow-corpus-shrink` accepts it — deliberately a separate, visible act, to be
explained in the story notes rather than absorbed by a re-run.

## Executed, not just present

A vendored corpus nothing runs is a checked-in file, which is exactly what
`prolog/` was until tasklist 146 US-2. Every case under `conformance/prolog/`
(255, including the 125 band-120 `mechanic-*` cases) now runs through
`insimul::InsimulKB` — the plugin's own `libinsimul` wrapper — in ctest
**`prolog_corpus`**, and is diffed against core's golden solution sets. The
band-120 **decision** corpora (`combat/`, `items/`, `routines/`, `skills/`,
`stealth/`, `traversal/` — 212 cases) have no runner in any language yet;
`npm run check:mechanic-corpora` measures that and names the blocker.
`RUNTIME_CORE_ADOPTION.md` §13 is the write-up.

## Why the manifest exists

This file used to be three lines asserting the directory was a mirror, and
nothing verified it. Measured at the start of tasklist 99 it was not one:

| | before US-3 | after |
| --- | --- | --- |
| mirrored files | 27, unverified | **64, hash-pinned** (34 at tasklist 99) |
| `prolog/` cases | 41 of 76 (54%) | **76 (100%)** then; **255** today (146 US-2) |
| KINP `identity` / `equivalence` / `worlds` | absent | present (34 cases) |
| `prolog/gameplay.json` | pre-KINP atoms | `id/3` terms |
| `predicate-schema-hash.json`, `content-library/` | absent | mirrored |
| `README.md` | described tau-prolog as a second engine | current |
| `content/*` | claimed to mirror a core dir that does not exist | declared local |

The guard matters more than the copy: the rot happened because nothing ever ran
the diff. `tools/vendor-conformance.mjs` is a deliberate port of Godot's script
of the same name — one mechanism across the engine repos, not three lookalikes.
The mirrored set here is byte-identical to core's, which is what "one corpus,
three runtimes" is supposed to mean — and since tasklist 146 US-2 the per-area
counts and floors are Unity's mechanism too, ported unchanged.
