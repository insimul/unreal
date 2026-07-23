# Content-library conformance corpus

Vendored mirror of `@insimul/core` `packages/core/conformance/content/` (source of
truth). Language-neutral, data-only fixtures that pin the **content-library import
contract** — the portable, world-independent bundle of authored content (items,
quests, characters, towns, narratives) every native engine's importer materializes
into the same entity set.

This is the shared half of the author-once/use-anywhere proof: the Unreal leg lives
in `Source/InsimulRuntime/Portable/InsimulContentLibrary.{h,cpp}` and is exercised by
`tools/verify-unreal/test_content_library.cpp` against these files.

## Files

- `library-basic.json` — the golden content library. Importing it materializes
  3 items, 2 quests, 2 characters, 2 towns and 2 narratives (11 native entities).
  `quests`/`characters`/`towns` reuse the world-snapshot shapes so imported content
  is indistinguishable from world-sourced content at the boundary.
- `invalid-cases.json` — schema-validation rejection cases. Each case is
  `{ name, library, expectedErrorContains }`; the importer MUST reject `library`
  with an error whose message contains `expectedErrorContains`. Covers the
  schema-version gate (missing / unsupported), missing top-level identity
  (`id` / `name`), and a missing required `id` in each entity collection.

## Library shape

```jsonc
{
  "schemaVersion": 1,          // gated to the importer's supported range
  "id": "conformance-basic",   // required, non-empty
  "name": "…",                 // required, non-empty
  "description": "…",
  "targetLanguage": "es",
  "items":      [ { "id", "name", "itemType", "description", "value" } ],
  "quests":     [ { "id", "name", "description", "giverNpcId", "status",
                    "content", "questType", "difficulty", "targetLanguage" } ],
  "characters": [ { "id", "firstName", "lastName", "gender", "occupation",
                    "currentLocation", "isAlive" } ],
  "towns":      [ { "id", "name", "settlementType", "population", "countryId" } ],
  "narratives": [ { "id", "title", "body", "language" } ]
}
```
