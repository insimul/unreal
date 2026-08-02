# Game templates & releasing (for maintainers)

Two parts of this repository are not the plugin you drop into a project — they are
the machinery that ships worlds *into* generated games and packages the plugin for
distribution. If you are only *using* the plugin, you can skip this page.

## The game-template tree (`templates/`)

Everything under `templates/` is a **game-template tree**: a complete Unreal project
skeleton (a `.uproject`, `Config/`, module glue, fonts, and starter source) that the
Insimul platform's export pipeline copies **verbatim** into a freshly generated
Unreal game project when a creator exports a world for this engine. After copying,
the pipeline substitutes `{{UPPER_SNAKE_CASE}}` placeholder tokens in the text files
with values derived from the exported world — world name, genre, entity counts,
palette colors, player/combat tuning, and so on.

- **The contract is machine-checked.** `templates/TEMPLATE_MANIFEST.json` is the
  authoritative list of every file the pipeline copies and the exact placeholders
  each file bears. A drift guard (in the parent workspace) fails the build if the
  manifest and the tree disagree, if a placeholder-bearing file is unlisted, or if
  any template file reaches into another engine's package.
- **Placeholder syntax:** `{{TOKEN}}`, upper snake case — e.g. `{{WORLD_NAME}}`,
  `{{PLAYER_INITIAL_HEALTH}}`, `{{ROAD_COLOR_R}}`. The manifest's top-level
  `placeholders` array is the full set this package uses.
- **`templates/` is distinct from the plugin `Source/`.** Only `templates/` is
  consumed by the export pipeline; template files depend only on this package,
  generated code, and exported world-data JSON — never on a sibling engine's
  package.

`templates/` also carries its own `MIGRATION.md`, `INTEGRATION_GUIDE.md`, and
`README.md` for working inside the generated-project skeleton.

## Releasing the plugin

`scripts/release/build-plugin-zip.mjs` produces the distributable:

- it stages the plugin into `dist/Insimul/` — the `.uplugin` at the root plus
  `Source/`, **excluding** `templates/` and build intermediates,
- zips it to `dist/Insimul-<version>.zip` in Fab/Marketplace layout, and
- asserts the resulting file set.

It does **not** publish. The version is the `VersionName` in `Insimul.uplugin`, kept
in sync with the top-level `VERSION` file (see [`../CHANGELOG.md`](../CHANGELOG.md)).
`EngineVersion` is intentionally left unset — this is a **source plugin** that builds
against whatever engine you install it into, and is pinned per target only at
submission time.
