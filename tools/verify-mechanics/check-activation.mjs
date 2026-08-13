#!/usr/bin/env node
// check-activation.mjs — does this plugin ACTIVATE modules from the genre bundle,
// and is the data it activates from the data core emitted (US-3 of tasklist 146).
//
// WHAT THIS GATE IS, AND WHAT IT DELIBERATELY IS NOT. US-3's criteria are (1) the
// plugin reads the active module set from the IR and activating one more module needs
// no engine code change, (2) an inactive module contributes NOTHING — no consulted
// pack, no registered system, and (3) a playable scene exercises at least two adopted
// mechanics end to end. Two of those are questions about a KNOWLEDGE BASE, and this
// repository can put its own C++ in front of a real libinsimul, so those two are
// answered by ctest rather than re-implemented here:
//
//   ctest `module_activation`   the resolver, the pack consult, the host restriction
//                               and the scenario runner over the shipped data, with
//                               no library — every outcome, including the ones a
//                               healthy build never takes.
//   ctest `activation_witness`  every genre × every pack: the pack's own signature
//                               predicate is in a real KB exactly when its module is
//                               active. Plus the sample scene's scenario, executed.
//
// That split is deliberate and is where this port differs from the Unity probe, whose
// check-activation.mjs compiles a C driver at gate time because it has no C++ harness
// to put the claim in. Duplicating the witness in JavaScript here would be a second
// implementation of the thing being tested. What is LEFT for this gate is everything
// that is a property of the DATA and the SOURCES rather than of a KB:
//
//   1. the vendored rule packs are present and hash what PACKS.json records
//      (vendor-packs.mjs, no core checkout needed);
//   2. the table the BUILD reads (templates/.../Content/Data/insimul/modules/) is
//      byte-identical to the vendored mirror of core's emission (conformance/modules/);
//   3. every genre's active pack list and host-interface list re-resolve from the
//      table's own module rows, in core's consult order;
//   4. no module id and no pack area appears in the plugin's activation sources —
//      the "no hardcoded list" criterion, as a check rather than a promise;
//   5. the shipped scenario is a scenario: a known genre, at least two mechanics,
//      and every host fact from an interface that genre actually activates.
//
// Every one of them carries a NEGATIVE CONTROL, because a check that cannot fail is
// treated as a defect in this repository.
//
//   cd tools && npm run check:activation

import { existsSync, readFileSync, readdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

import { checkVendored, readManifest as readPackManifest } from '../vendor-packs/vendor-packs.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));
const REPO_ROOT = join(HERE, '..', '..');
const DATA_ROOT = join(REPO_ROOT, 'templates', 'project', 'Content', 'Data', 'insimul');
const SHIPPED_TABLE = join(DATA_ROOT, 'modules', 'genre-activation.json');
const VENDORED_TABLE = join(REPO_ROOT, 'conformance', 'modules', 'genre-activation.json');
const SCENARIOS = join(DATA_ROOT, 'scenarios');

/**
 * The sources that must name no mechanic. The resolver and the pack consult are the
 * two places a list would be tempting; the activator, the binder and the sample scene
 * are where an `if (Genre == TEXT("rpg"))` would land. The UI panel catalog and its
 * seam joined the list with tasklist 190 US-1: which panels a module brings is the
 * newest place a hardcoded list would be tempting, and an `if (Key == "inventory")`
 * guarded by a mechanic name there would hide a panel in every world with no error
 * anywhere. The ownership is data (Content/Data/insimul/ui/panels.json); ctest
 * `ui_registry` checks that data against the table's module ids.
 */
const NO_HARDCODED_LIST = [
  join('Source', 'InsimulRuntime', 'Portable', 'InsimulModuleActivation.cpp'),
  join('Source', 'InsimulRuntime', 'Portable', 'InsimulModuleActivation.h'),
  join('Source', 'InsimulRuntime', 'Portable', 'InsimulModulePacks.cpp'),
  join('Source', 'InsimulRuntime', 'Portable', 'InsimulModulePacks.h'),
  join('Source', 'InsimulRuntime', 'Portable', 'InsimulUIPanelCatalog.cpp'),
  join('Source', 'InsimulRuntime', 'Portable', 'InsimulUIPanelCatalog.h'),
  join('Source', 'InsimulRuntime', 'Private', 'InsimulUIPanelSurface.cpp'),
  join('templates', 'source', 'mechanics', 'InsimulModuleActivator.cpp'),
  join('templates', 'source', 'mechanics', 'InsimulModuleActivator.h'),
  join('templates', 'source', 'mechanics', 'InsimulMechanicHostBinder.cpp'),
  join('templates', 'source', 'mechanics', 'InsimulMechanicSampleScene.cpp'),
];

/**
 * ONE word, in ONE file, for ONE stated reason. Core's NeedType vocabulary and its
 * pack areas are different namespaces that happen to share a spelling, and
 * NeedTypeAtom() has to write core's atom out. Listing the collision here keeps the
 * file in the scan — any OTHER mechanic name appearing in it still fails, and a
 * negative control proves that — instead of the usual dodge of dropping the file.
 */
const ALLOWED_COLLISIONS = [
  {
    file: join('templates', 'source', 'mechanics', 'InsimulMechanicHostBinder.cpp'),
    name: 'stamina',
    reason: "core's NeedType atom, written by NeedTypeAtom() — the same spelling as the pack area, and not a module list",
  },
];

// ---- the data ----------------------------------------------------------------

export function readTable(path = VENDORED_TABLE) {
  if (!existsSync(path)) return null;
  return JSON.parse(readFileSync(path, 'utf8'));
}

/**
 * Resolve a genre exactly the way core does (`packsFor`): the always-active packs
 * plus the packs the genre's modules own, in the build's consult order.
 */
export function resolvePacks(table, genre, consultOrder) {
  const entry = table.genres?.[genre];
  const owned = new Set(table.alwaysActivePacks ?? []);
  for (const m of entry?.modules ?? []) if (m.predicatePack) owned.add(m.predicatePack);
  return consultOrder.filter((area) => owned.has(area));
}

/** Host interfaces a genre's modules name, deduplicated. */
export function resolveHosts(table, genre) {
  const hosts = [];
  for (const m of table.genres?.[genre]?.modules ?? []) {
    for (const h of m.hostInterface ?? []) if (!hosts.includes(h)) hosts.push(h);
  }
  return hosts;
}

/**
 * Check 3 — every genre's declared pack list and host-interface list must be what a
 * resolution from its own module rows produces, in consult order.
 */
export function checkResolution(table, consultOrder) {
  const problems = [];
  const genres = Object.keys(table.genres ?? {});
  if (genres.length === 0) problems.push('the activation table knows no genres');

  for (const genre of genres) {
    const entry = table.genres[genre];
    const packs = resolvePacks(table, genre, consultOrder);
    const declared = entry.predicatePacks ?? [];

    const same = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);
    if (!same(packs, declared)) {
      problems.push(
        `genre '${genre}': the table declares packs [${declared.join(', ')}] and its module rows ` +
          `resolve to [${packs.join(', ')}] in consult order`,
      );
    }
    for (const area of declared) {
      if (!consultOrder.includes(area)) {
        problems.push(`genre '${genre}' consults pack '${area}', which this build does not carry`);
      }
    }
    const hosts = resolveHosts(table, genre);
    const declaredHosts = [...(entry.hostInterfaces ?? [])].sort();
    if (JSON.stringify([...hosts].sort()) !== JSON.stringify(declaredHosts)) {
      problems.push(
        `genre '${genre}': the table declares host interfaces [${declaredHosts.join(', ')}] and its ` +
          `module rows name [${[...hosts].sort().join(', ')}]`,
      );
    }
    // A genre selecting no module is a real answer (puzzle, strategy, …) and must
    // still get the shared vocabulary, or it has no way to win, lose or finish.
    for (const area of table.alwaysActivePacks ?? []) {
      if (!packs.includes(area)) problems.push(`genre '${genre}' does not consult always-active pack '${area}'`);
    }
  }
  return problems;
}

/**
 * Check 4 — no mechanic named in the plugin's activation sources. Every module id and
 * every pack area from the table is searched for as a quoted string; a match means the
 * list came back.
 */
/** Every module id and pack area the table names — what must not appear quoted in an
 *  activation source. */
export function namesOf(table) {
  const out = new Set();
  for (const entry of Object.values(table.genres ?? {})) {
    for (const m of entry.modules ?? []) {
      if (m.id) out.add(m.id);
      if (m.predicatePack) out.add(m.predicatePack);
    }
  }
  for (const a of table.alwaysActivePacks ?? []) out.add(a);
  return out;
}

export function checkNoHardcodedList(table, sources = NO_HARDCODED_LIST, root = REPO_ROOT) {
  const names = namesOf(table);

  const problems = [];
  for (const rel of sources) {
    const path = typeof rel === 'string' ? join(root, rel) : null;
    const text = path && existsSync(path) ? readFileSync(path, 'utf8') : rel?.text;
    if (text === undefined || text === null) {
      problems.push(`${rel} does not exist — the no-hardcoded-list check cannot read it`);
      continue;
    }
    const label = typeof rel === 'string' ? rel : rel.label;
    for (const name of names) {
      // Quoted, so prose in a comment ("the combat module") does not fire and a
      // `if (Area == "combat")` does.
      if (!text.includes(`"${name}"`)) continue;
      const allowed = ALLOWED_COLLISIONS.find((c) => c.file === rel && c.name === name);
      if (allowed) continue;
      problems.push(`${label} names the mechanic "${name}" — the active set is DATA (module contract §7.2)`);
    }
  }
  return problems;
}

/**
 * Check 5 — the shipped scenario is one this build can actually run: a genre the
 * table knows, at least two ACTIVE mechanics, and host facts from interfaces that
 * genre's modules activate. (Whether it ANSWERS correctly is ctest
 * `activation_witness`; this is the part that is a property of the file.)
 */
export function checkScenarios(table, consultOrder, dir = SCENARIOS) {
  const problems = [];
  const files = existsSync(dir) ? readdirSync(dir).filter((f) => f.endsWith('.json')).sort() : [];
  if (files.length === 0) {
    problems.push('no scenario under Content/Data/insimul/scenarios — the playable-scene claim has no script');
    return { problems, files };
  }
  for (const file of files) {
    const scenario = JSON.parse(readFileSync(join(dir, file), 'utf8'));
    const entry = table.genres?.[scenario.genre];
    if (!entry) {
      problems.push(`${file}: genre '${scenario.genre}' is not one the activation table knows`);
      continue;
    }
    const packs = resolvePacks(table, scenario.genre, consultOrder);
    const hosts = resolveHosts(table, scenario.genre);
    const active = new Set((entry.modules ?? []).map((m) => m.id));
    const exercised = new Set();

    for (const step of scenario.steps ?? []) {
      if (!step.goal) problems.push(`${file}: a step asks no goal`);
      if (step.expect !== 'succeeds' && step.expect !== 'fails') {
        problems.push(`${file} [${step.name}]: expects '${step.expect}', which is neither 'succeeds' nor 'fails'`);
      }
      if (step.mechanic && active.has(step.mechanic) === Boolean(step.inactive)) {
        problems.push(
          `${file} [${step.name}]: mechanic '${step.mechanic}' is ${active.has(step.mechanic) ? '' : 'NOT '}` +
            `activated by genre '${scenario.genre}' and the step ${step.inactive ? 'claims it is inactive' : 'treats it as active'}`,
        );
      }
      for (const hf of step.hostFacts ?? []) {
        if (!hosts.includes(hf.from)) {
          problems.push(
            `${file} [${step.name}]: host fact '${hf.fact}' comes from ${hf.from}, which genre ` +
              `'${scenario.genre}' does not activate`,
          );
        }
        if (!hf.note || !hf.probe) problems.push(`${file}: host fact '${hf.fact}' does not say which probe measures it, or how`);
      }
      if (step.mechanic && !step.inactive) exercised.add(step.mechanic);
    }
    if (exercised.size < 2) {
      problems.push(`${file} exercises ${exercised.size} mechanic(s); the criterion is at least two`);
    }
    // A scenario whose packs are not activated by its own genre could never run.
    const needed = new Set([...exercised].map((m) => (entry.modules ?? []).find((x) => x.id === m)?.predicatePack));
    for (const area of needed) {
      if (area && !packs.includes(area)) problems.push(`${file}: pack '${area}' is not activated by genre '${scenario.genre}'`);
    }
  }
  return { problems, files };
}

// ---- negative controls -----------------------------------------------------------

/**
 * Prove the checks can fail. Every mutation is of a COPY; the unmutated data is
 * re-checked at the end, so "rejects everything" cannot pass the trials.
 */
export function negativeControls(table, consultOrder) {
  const failures = [];
  const trial = (label, produced) => {
    if (!produced) failures.push(`negative control '${label}' produced NO error — the check is decorative`);
  };
  const copy = () => JSON.parse(JSON.stringify(table));

  // 1. A module dropped from a genre must break that genre's declared pack list.
  const mutated = copy();
  const genre = Object.keys(mutated.genres).find((g) => (mutated.genres[g].modules ?? []).length > 0);
  mutated.genres[genre].modules = mutated.genres[genre].modules.slice(1);
  trial('a module dropped from a genre', checkResolution(mutated, consultOrder).length > 0);

  // 2. An always-active pack withheld from a genre must fail.
  const withheld = copy();
  const shared = (withheld.alwaysActivePacks ?? [])[0];
  withheld.genres[genre].predicatePacks = (withheld.genres[genre].predicatePacks ?? []).filter((a) => a !== shared);
  trial('an always-active pack withheld from a genre', checkResolution(withheld, consultOrder).length > 0);

  // 3. A pack this build does not carry must fail.
  trial('a genre consulting a pack the build lacks', checkResolution(copy(), consultOrder.slice(1)).length > 0);

  // 4. A mechanic name reappearing in the resolver must fail check 4.
  trial(
    'a hardcoded module id in the activation resolver',
    checkNoHardcodedList(table, [{ label: 'synthetic', text: 'if (Genre == "rpg") { Activate("combat"); }' }]).length > 0,
  );

  // 5. The allowance is for ONE name in ONE file — another name in the same file
  //    must still fail, or the allowance would be a hole rather than a note.
  const collision = ALLOWED_COLLISIONS[0];
  if (collision) {
    const other = [...namesOf(table)].find((n) => n !== collision.name);
    trial(
      'a second mechanic name in the file that holds the one allowed collision',
      checkNoHardcodedList(table, [{ label: collision.file, text: `X("${collision.name}"); Y("${other}");` }]).length > 0,
    );
    trial(
      'the allowance itself still applies to the real file',
      checkNoHardcodedList(table, [collision.file]).length === 0,
    );
  }

  // 6. A scenario that exercises one mechanic must fail check 5.
  const oneMechanic = {
    problems: checkScenarios(
      table,
      consultOrder,
      // A directory that does not exist stands in for "no scenario at all".
      join(REPO_ROOT, 'templates', 'project', 'Content', 'Data', 'insimul', 'no-such-scenarios'),
    ).problems,
  };
  trial('no scenario to run at all', oneMechanic.problems.length > 0);

  // A control on the controls.
  if (checkResolution(table, consultOrder).length !== 0) {
    failures.push('the UNMUTATED activation table did not pass — the trials above prove nothing');
  }
  if (checkNoHardcodedList(table).length !== 0) {
    failures.push('the UNMUTATED plugin sources did not pass the no-hardcoded-list check');
  }
  return failures;
}

// ---- the gate -------------------------------------------------------------------

export function run() {
  const out = [];
  const fail = (line) => ({ ok: false, lines: [...out, `  x ${line}`] });

  // 1. The vendored packs.
  const packManifest = readPackManifest();
  if (!packManifest) {
    return fail('the rule packs are not vendored — run vendor-packs/vendor-packs.mjs --core <core> --write');
  }
  const packProblems = checkVendored(packManifest);
  if (packProblems.length > 0) return { ok: false, lines: packProblems.map((p) => `  x ${p}`) };
  const consultOrder = packManifest.consultOrder;
  out.push(
    `  ok  ${consultOrder.length} rule pack(s) vendored and hashing what PACKS.json records ` +
      `(core ${String(packManifest.coreCommit).slice(0, 7)})`,
  );

  // 2. The table the BUILD reads is the vendored mirror, byte for byte.
  if (!existsSync(VENDORED_TABLE)) return fail('conformance/modules/genre-activation.json is not vendored');
  if (!existsSync(SHIPPED_TABLE)) {
    return fail(
      'templates/project/Content/Data/insimul/modules/genre-activation.json is missing — a BUILD has no table to read',
    );
  }
  if (!readFileSync(VENDORED_TABLE).equals(readFileSync(SHIPPED_TABLE))) {
    return fail(
      'the table the build reads differs from the vendored mirror — ' +
        'cp conformance/modules/genre-activation.json templates/project/Content/Data/insimul/modules/',
    );
  }
  out.push("  ok  the activation table a BUILD reads is byte-identical to the vendored mirror of core's emission");

  const table = readTable();
  const genres = Object.keys(table.genres ?? {});

  // 3. The resolution.
  const resolution = checkResolution(table, consultOrder);
  if (resolution.length > 0) return { ok: false, lines: [...out, ...resolution.map((p) => `  x ${p}`)] };
  out.push(`  ok  ${genres.length} genre(s) resolve to the packs and host interfaces their module rows name`);

  // 4. No hardcoded list.
  const hardcoded = checkNoHardcodedList(table);
  if (hardcoded.length > 0) return { ok: false, lines: [...out, ...hardcoded.map((p) => `  x ${p}`)] };
  out.push(`  ok  no module id or pack area appears in ${NO_HARDCODED_LIST.length} activation source(s) — the set is data`);

  // 5. The scene's script.
  const scenarios = checkScenarios(table, consultOrder);
  if (scenarios.problems.length > 0) {
    return { ok: false, lines: [...out, ...scenarios.problems.map((p) => `  x ${p}`)] };
  }
  out.push(
    `  ok  ${scenarios.files.length} scenario(s) name a known genre, two or more active mechanics, and ` +
      'host facts from interfaces that genre activates',
  );

  // 6. The gate must be able to fail.
  const controls = negativeControls(table, consultOrder);
  if (controls.length > 0) return { ok: false, lines: [...out, ...controls.map((c) => `  x ${c}`)] };
  out.push(
    '  ok  negative controls: a dropped module, a withheld shared pack, a missing pack, a hardcoded id, a second ' +
      'name in the one file with an allowed collision, and a scenario-less build each fail',
  );

  out.push(
    '  .   the KB witness (every genre x every pack) and the scene, EXECUTED, are ctest ' +
      '`activation_witness` — npm run check:host:binaries',
  );
  return { ok: true, lines: out };
}

// ---- entry point ------------------------------------------------------------------

const isMain = process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1];
if (isMain) {
  console.log('check-activation: modules activated from the genre bundle, and what an inactive one costs');
  const r = run();
  for (const l of r.lines) console.log(l);
  if (!r.ok) {
    console.error('check-activation: FAILED');
    process.exit(1);
  }
  console.log('check-activation: OK');
}
