// emit-packs.ts — read core's PREDICATE_PACKS and print them as JSON.
//
// RUN BY vendor-packs.mjs, NOT BY HAND. That script picks a runner (see its
// `emitFromCore`): `vite-node` inside the core checkout when one is installed,
// otherwise an `esbuild` bundle of THIS file executed by node. Both EXECUTE core;
// neither parses it.
//
// WHY IT EXECUTES CORE RATHER THAN PARSING IT. The pack texts are TypeScript
// template literals assembled by several registries (`MECHANIC_PREDICATE_PACKS`,
// `AI_PREDICATE_PACKS`, `ROUTINE_PREDICATE_PACKS`, `MAP_PREDICATE_PACKS`,
// `SCAFFOLD_PREDICATE_PACKS`), and the ORDER they are consulted in is a hard
// constraint core states twice: the routine and map packs write clauses for
// predicates the substrate declares `:- dynamic`, and a `:- dynamic` arriving after
// a clause for the same predicate is a permission_error on a strict ISO engine
// (trealla is one). A regex over the sources would have to re-implement that
// assembly and would silently disagree the day core reorders it. Executing the
// module gives the real value — text, area, runtime predicates and consult order —
// and core's own tests are what keep THAT honest.
//
// This file is the only part of the vendoring that needs a core checkout, which is
// why it is a separate script: `vendor-packs.mjs --check` runs against the vendored
// artifact alone and needs no core, no TypeScript and no bundler.

import { PREDICATE_PACKS } from '@insimul/core/prolog/predicate-packs';
import { ALWAYS_ACTIVE_PACKS, MODULE_ACTIVATION_TABLE } from '@insimul/core/modules/module-activation';

const payload = {
  // Consult order is the array order — see the header.
  packs: PREDICATE_PACKS.map((pack) => ({
    area: pack.area,
    prolog: pack.prolog,
    runtimePredicates: [...pack.runtimePredicates],
  })),
  alwaysActivePacks: [...ALWAYS_ACTIVE_PACKS],
  // Only the per-genre pack lists: the whole table is vendored separately by
  // vendor-conformance.mjs (conformance/modules/genre-activation.json) and two
  // copies of it here would be the duplication this repo keeps removing.
  genrePacks: Object.fromEntries(
    Object.entries(MODULE_ACTIVATION_TABLE).map(([genre, set]) => [genre, [...set.predicatePacks]]),
  ),
};

// A sentinel prefix, because a runner may write its own noise to stdout.
console.log(`@@INSIMUL_PACKS@@${JSON.stringify(payload)}`);
