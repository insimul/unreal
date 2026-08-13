#!/usr/bin/env node
// check-mechanic-corpora.mjs — the DECISION-LAYER half of the band-120 corpus
// adoption (US-2 of tasklist 146): what is vendored, what actually executes, and
// what does not, with the reason and the count.
//
// TWO HALVES, TWO GATES. Adopting a band-120 module brings two corpora:
//
//   * the PREDICATE half — conformance/prolog/mechanic-*.json, 125 cases. Pure
//     Prolog over libinsimul. tools/verify-unreal/test_prolog_corpus.cpp EXECUTES
//     every one of them (all 255 in the directory, in fact) through the plugin's
//     own insimul::InsimulKB and diffs the golden solution sets. ctest
//     `prolog_corpus`.
//   * the DECISION half — conformance/{combat,items,routines,skills,stealth,
//     traversal}/*.json, 212 cases. These are core's decision layers
//     (CombatResolver, Market, DetectionTracker, ...) called with an input and
//     compared to an expected result. Reaching one means a row in
//     native/corebridge/js/entry.js. THERE ARE NO SUCH ROWS: the shipped
//     libinsimulcore answers core.methods with five names and not one of them is
//     a mechanic (measured in US-1, and pinned by ctest `mechanic_bridge`).
//
// So this gate does not pretend to execute them. It MEASURES, and it is built so
// that the measurement cannot quietly stop being true:
//
//   1. the vendored band-120 areas, their files and their case counts must match
//      MECHANIC_CORPORA.json — a corpus that appears, grows or shrinks is a
//      failure that names itself;
//   2. every area with an `executedBy` runner must name a file that EXISTS and a
//      ctest target that is REGISTERED. A runner named in a manifest and absent
//      from CMakeLists.txt is the same rot one level up;
//   3. an area with no runner must carry a stated `blocker` — never a silent
//      null, because a silent null reads as "nothing to do here";
//   4. the pinned bridge method list must match the one
//      tools/verify-unreal/test_mechanic_hosts.cpp asks the real library for. That
//      test is the single measurement site; this is a mirror of it, and a mirror
//      nothing checks is how this repo's corpus rotted to 41 of 76 cases;
//   5. the vendored module-activation table (conformance/modules/genre-activation.json)
//      must agree with the host interfaces this repo implements
//      (verify-mechanics/MODULE_HOSTS.json). That one IS a live check today, and
//      it is what catches core moving a host interface out from under the plugin;
//   6. NEGATIVE CONTROLS prove each of those can fail, so "0 of 212 executed" is
//      a statement about the bridge and not about this file being decorative.
//
//   cd tools && npm run check:mechanic-corpora
//   cd tools && node verify-mechanics/check-mechanic-corpora.mjs --core <packages/core> --write
//
// NO BINARIES NEEDED, ON PURPOSE. The one thing here that would need the shipped
// library — asking core.methods — is already done by ctest `mechanic_bridge`
// under --require-binaries. Duplicating the driver in Node would give this repo
// two places to measure one fact and two places for them to disagree; pinning
// the C++ list and checking the mirror gives it one.
//
// RUNTIME_CORE_ADOPTION.md section 13 is the prose: the two halves and their
// counts, what is blocked, why it is core-side, and what a row would look like.

import { existsSync, readFileSync, readdirSync, writeFileSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
const REPO = join(HERE, '..', '..');
const CORPUS = join(REPO, 'conformance');
const MANIFEST = join(HERE, 'MECHANIC_CORPORA.json');
const HOSTS = join(HERE, 'MODULE_HOSTS.json');
const BRIDGE_TEST = join(REPO, 'tools', 'verify-unreal', 'test_mechanic_hosts.cpp');
const CMAKE = join(REPO, 'tools', 'verify-unreal', 'CMakeLists.txt');

/**
 * The band-120 DECISION-layer corpora, area to the module(s) whose decision
 * layers produce them. `modules/` is a TABLE, not a case list, and is handled
 * separately below.
 */
const BAND_AREAS = {
  combat: ['combat', 'stamina'],
  items: ['equipment'],
  routines: ['routine'],
  skills: ['skill'],
  stealth: ['perception'],
  traversal: ['traversal'],
};

const ACTIVATION = 'modules/genre-activation.json';
const PROLOG = 'prolog';

const args = process.argv.slice(2);
const coreArg = argValue('--core') ?? process.env.INSIMUL_CORE_DIR ?? null;
const doWrite = args.includes('--write');

function argValue(flag) {
  const i = args.indexOf(flag);
  return i >= 0 && i + 1 < args.length ? args[i + 1] : null;
}

// ---- the vendored corpus ----------------------------------------------------

/** { area: { file: caseCount } } for the band-120 decision corpora under `root`. */
export function surveyAreas(root = CORPUS) {
  const survey = {};
  for (const area of Object.keys(BAND_AREAS).sort()) {
    const dir = join(root, area);
    if (!existsSync(dir)) continue;
    const files = {};
    for (const f of readdirSync(dir).filter((n) => n.endsWith('.json')).sort()) {
      const doc = JSON.parse(readFileSync(join(dir, f), 'utf8'));
      files[f] = Array.isArray(doc.cases) ? doc.cases.length : 0;
    }
    survey[area] = files;
  }
  return survey;
}

/** { file: caseCount } for the band-120 PREDICATE packs under `root`. */
export function surveyPredicatePacks(root = CORPUS) {
  const dir = join(root, PROLOG);
  const packs = {};
  if (!existsSync(dir)) return packs;
  for (const f of readdirSync(dir).filter((n) => n.startsWith('mechanic-') && n.endsWith('.json')).sort()) {
    const doc = JSON.parse(readFileSync(join(dir, f), 'utf8'));
    packs[f] = Array.isArray(doc.cases) ? doc.cases.length : 0;
  }
  return packs;
}

const areaTotal = (files) => Object.values(files).reduce((a, b) => a + b, 0);
const total = (survey) => Object.values(survey).reduce((sum, f) => sum + areaTotal(f), 0);

// ---- the pinned bridge surface ---------------------------------------------

/**
 * The EXPECTED_METHODS[] table from test_mechanic_hosts.cpp — the list this repo
 * asserts the SHIPPED libinsimulcore answers with. Re-derived rather than
 * re-declared, so the manifest cannot drift from the site that measures.
 */
export function bridgeMethodsFromCpp(cpp = readFileSync(BRIDGE_TEST, 'utf8')) {
  const start = cpp.indexOf('EXPECTED_METHODS[] = {');
  if (start < 0) return { fail: 'EXPECTED_METHODS[] was not found in tools/verify-unreal/test_mechanic_hosts.cpp' };
  const end = cpp.indexOf('};', start);
  if (end < 0) return { fail: 'EXPECTED_METHODS[] in test_mechanic_hosts.cpp could not be delimited' };
  const methods = [...cpp.slice(start, end).matchAll(/"([^"]+)"/g)].map((m) => m[1]);
  if (methods.length === 0) return { fail: 'EXPECTED_METHODS[] is empty — the bridge pin would assert nothing' };
  return { methods: methods.sort() };
}

/** The ctest target names CMakeLists.txt registers. */
function registeredTests(cmake = readFileSync(CMAKE, 'utf8')) {
  return [...cmake.matchAll(/add_test\(NAME\s+([A-Za-z0-9_]+)/g)].map((m) => m[1]);
}

// ---- the checks -------------------------------------------------------------

/** The corpus survey vs what the manifest pins. */
export function checkSurvey(manifest, survey = surveyAreas(), packs = surveyPredicatePacks()) {
  const problems = [];
  const pinned = manifest.areas ?? {};

  for (const area of Object.keys(survey)) {
    if (!(area in pinned)) {
      problems.push(
        `conformance/${area}/ is vendored (${areaTotal(survey[area])} case(s)) and MECHANIC_CORPORA.json ` +
          'pins no such area — an ARRIVING corpus is as loud as a vanishing one; re-run with --core --write ' +
          'and say in the notes whether anything can execute it',
      );
    }
  }
  for (const [area, entry] of Object.entries(pinned)) {
    const files = survey[area];
    if (!files) {
      problems.push(`MECHANIC_CORPORA.json pins conformance/${area}/, which is not vendored`);
      continue;
    }
    for (const [file, n] of Object.entries(entry.files ?? {})) {
      if (!(file in files)) {
        problems.push(`conformance/${area}/${file} is pinned at ${n} case(s) and is not vendored`);
      } else if (files[file] !== n) {
        problems.push(`conformance/${area}/${file} holds ${files[file]} case(s), the manifest pins ${n}`);
      }
    }
    for (const file of Object.keys(files)) {
      if (!(file in (entry.files ?? {}))) {
        problems.push(`conformance/${area}/${file} is vendored (${files[file]} case(s)) and is not pinned`);
      }
    }
    if (areaTotal(files) !== entry.cases) {
      problems.push(`conformance/${area}/ holds ${areaTotal(files)} case(s), the manifest pins ${entry.cases}`);
    }
    const modules = BAND_AREAS[area] ?? [];
    const sameList = (a, b) => JSON.stringify([...(a ?? [])].sort()) === JSON.stringify([...(b ?? [])].sort());
    if (!sameList(entry.modules, modules)) {
      problems.push(`conformance/${area}/ is pinned to modules [${(entry.modules ?? []).join(', ')}], this gate maps it to [${modules.join(', ')}]`);
    }
  }

  // The predicate half. It is not this gate's runner, but it IS the reason the
  // decision half being blocked is not the whole story, so it is pinned here too.
  const pinnedPacks = manifest.predicateHalf?.packs ?? {};
  for (const [file, n] of Object.entries(pinnedPacks)) {
    if (!(file in packs)) {
      problems.push(`conformance/prolog/${file} is pinned at ${n} case(s) and is not vendored`);
    } else if (packs[file] !== n) {
      problems.push(`conformance/prolog/${file} holds ${packs[file]} case(s), the manifest pins ${n}`);
    }
  }
  for (const file of Object.keys(packs)) {
    if (!(file in pinnedPacks)) {
      problems.push(`conformance/prolog/${file} is vendored (${packs[file]} case(s)) and is not pinned`);
    }
  }
  return problems;
}

/** Every runner an entry names must exist and be registered; every null must say why. */
export function checkRunners(manifest, tests = registeredTests()) {
  const problems = [];
  const entries = [
    ...Object.entries(manifest.areas ?? {}).map(([k, v]) => [`conformance/${k}/`, v]),
    ['the predicate half', manifest.predicateHalf ?? {}],
  ];
  for (const [label, entry] of entries) {
    const runner = entry.executedBy ?? null;
    if (runner === null) {
      if (!entry.blocker) {
        problems.push(`${label} names no runner and states no blocker — a silent null reads as "nothing to do here"`);
      }
      continue;
    }
    if (!existsSync(join(REPO, runner.file ?? ''))) {
      problems.push(`${label} is executed by '${runner.file}', which does not exist`);
    }
    if (runner.ctest && !tests.includes(runner.ctest)) {
      problems.push(`${label} names ctest target '${runner.ctest}', which CMakeLists.txt does not register`);
    }
  }
  return problems;
}

/** The manifest's bridge pin vs the C++ site that actually asks the library. */
export function checkBridgePin(manifest, derived = bridgeMethodsFromCpp()) {
  if (derived.fail) return [derived.fail];
  const pinned = [...(manifest.bridgeMethods ?? [])].sort();
  if (JSON.stringify(pinned) !== JSON.stringify(derived.methods)) {
    return [
      'the bridge method pin has drifted from tools/verify-unreal/test_mechanic_hosts.cpp:',
      `      C++      [${derived.methods.join(', ')}]`,
      `      manifest [${pinned.join(', ')}]`,
    ];
  }
  if (derived.methods.some((m) => Object.keys(BAND_AREAS).some((a) => m.startsWith(`${a}.`)))) {
    return ['a mechanic row has ARRIVED on the bridge — the decision corpora are now reachable and this gate must be pointed at them'];
  }
  return [];
}

/**
 * The vendored genre-activation table is core's own emission of INSIMUL_MODULES.
 * MODULE_HOSTS.json is what this repo implements against. They are two views of
 * one contract, so they must agree — and this is the one band-120 corpus check
 * that runs TODAY, with no bridge row needed.
 */
export function checkActivationTable(table = null, hosts = null) {
  const problems = [];
  if (table === null || hosts === null) {
    const tablePath = join(CORPUS, ACTIVATION);
    if (!existsSync(tablePath)) return [`conformance/${ACTIVATION} is not vendored`];
    if (!existsSync(HOSTS)) return ['verify-mechanics/MODULE_HOSTS.json is missing'];
    table = table ?? JSON.parse(readFileSync(tablePath, 'utf8'));
    hosts = hosts ?? JSON.parse(readFileSync(HOSTS, 'utf8'));
  }

  // Union the per-genre module lists — a module may appear under several genres,
  // and every appearance must describe it identically.
  const byId = new Map();
  for (const [genre, entry] of Object.entries(table.genres ?? {})) {
    for (const m of entry.modules ?? []) {
      const prev = byId.get(m.id);
      if (!prev) {
        byId.set(m.id, { ...m, genres: [genre] });
        continue;
      }
      prev.genres.push(genre);
      const same = (a, b) => JSON.stringify([...(a ?? [])].sort()) === JSON.stringify([...(b ?? [])].sort());
      if (!same(prev.hostInterface, m.hostInterface) || !same(prev.decisionLayer, m.decisionLayer)) {
        problems.push(`module '${m.id}' is described differently under genre '${genre}' than under '${prev.genres[0]}'`);
      }
    }
  }

  const same = (a, b) => JSON.stringify([...(a ?? [])].sort()) === JSON.stringify([...(b ?? [])].sort());
  for (const m of hosts.modules ?? []) {
    const t = byId.get(m.id);
    if (!t) {
      problems.push(`MODULE_HOSTS.json pins module '${m.id}', the vendored activation table names no such module`);
      continue;
    }
    if (!same(t.hostInterface, m.hostInterface)) {
      problems.push(
        `module '${m.id}': the activation table declares host interfaces ` +
          `[${[...(t.hostInterface ?? [])].sort().join(', ')}], MODULE_HOSTS.json pins ` +
          `[${[...(m.hostInterface ?? [])].sort().join(', ')}]`,
      );
    }
    if (!same(t.decisionLayer, m.decisionLayer)) {
      problems.push(
        `module '${m.id}': the activation table declares decision layers ` +
          `[${[...(t.decisionLayer ?? [])].sort().join(', ')}], MODULE_HOSTS.json pins ` +
          `[${[...(m.decisionLayer ?? [])].sort().join(', ')}]`,
      );
    }
    if (t.conforms !== true) {
      problems.push(`module '${m.id}' is marked conforms=${t.conforms} in the activation table`);
    }
  }
  return problems;
}

// ---- negative controls ------------------------------------------------------

/**
 * Prove each data check can fail. Nothing here needs a bridge row, so there is
 * no excuse for any of them being decorative — and "0 of 212 executed" is only
 * an honest measurement if the measuring could have come out differently.
 */
export function negativeControls(manifest) {
  const failures = [];
  const clone = (o) => JSON.parse(JSON.stringify(o));

  const shrunk = surveyAreas();
  const anyArea = Object.keys(shrunk)[0];
  const anyFile = Object.keys(shrunk[anyArea])[0];
  shrunk[anyArea][anyFile] -= 1;
  if (checkSurvey(manifest, shrunk, surveyPredicatePacks()).length === 0) {
    failures.push('a SHRUNK corpus passed the survey check');
  }

  const grown = surveyAreas();
  grown[anyArea]['a-corpus-that-arrived.json'] = 7;
  if (checkSurvey(manifest, grown, surveyPredicatePacks()).length === 0) {
    failures.push('an ARRIVING corpus file passed the survey check');
  }

  const packsShrunk = surveyPredicatePacks();
  packsShrunk[Object.keys(packsShrunk)[0]] -= 1;
  if (checkSurvey(manifest, surveyAreas(), packsShrunk).length === 0) {
    failures.push('a SHRUNK predicate pack passed the survey check');
  }

  const noBlocker = clone(manifest);
  for (const entry of Object.values(noBlocker.areas ?? {})) {
    delete entry.blocker;
  }
  if (checkRunners(noBlocker).length === 0) {
    failures.push('an area with neither a runner nor a blocker passed the runner check');
  }

  const ghostRunner = clone(manifest);
  ghostRunner.predicateHalf.executedBy = { file: 'tools/verify-unreal/no-such-runner.cpp', ctest: 'no_such_target' };
  if (checkRunners(ghostRunner).length === 0) {
    failures.push('a runner that does not exist passed the runner check');
  }

  const unregistered = clone(manifest);
  unregistered.predicateHalf.executedBy = { file: 'tools/verify-mechanics/MODULE_HOSTS.json', ctest: 'not_registered' };
  if (checkRunners(unregistered).length === 0) {
    failures.push('a ctest target CMakeLists does not register passed the runner check');
  }

  const drifted = clone(manifest);
  drifted.bridgeMethods = [...drifted.bridgeMethods, 'combat.attack'];
  if (checkBridgePin(drifted).length === 0) {
    failures.push('a bridge pin that disagrees with the C++ passed the pin check');
  }

  const table = JSON.parse(readFileSync(join(CORPUS, ACTIVATION), 'utf8'));
  const hosts = JSON.parse(readFileSync(HOSTS, 'utf8'));

  const dropped = clone(table);
  for (const g of Object.values(dropped.genres ?? {})) {
    g.modules = (g.modules ?? []).filter((m) => m.id !== (hosts.modules ?? [])[0]?.id);
  }
  if (checkActivationTable(dropped, hosts).length === 0) {
    failures.push('an activation table missing an implemented module passed the table check');
  }

  const moved = clone(table);
  for (const g of Object.values(moved.genres ?? {})) {
    for (const m of g.modules ?? []) {
      if (m.id === (hosts.modules ?? [])[0]?.id) m.hostInterface = ['IMovedSomewhereElse'];
    }
  }
  if (checkActivationTable(moved, hosts).length === 0) {
    failures.push('an activation table that moved a host interface passed the table check');
  }

  const nonconforming = clone(table);
  for (const g of Object.values(nonconforming.genres ?? {})) {
    for (const m of g.modules ?? []) m.conforms = false;
  }
  if (checkActivationTable(nonconforming, hosts).length === 0) {
    failures.push('an activation table declaring conforms=false passed the table check');
  }

  return failures;
}

// ---- --write ----------------------------------------------------------------

function gitCommit(dir) {
  try {
    return execFileSync('git', ['-C', dir, 'rev-parse', 'HEAD'], { encoding: 'utf8' }).trim();
  } catch {
    return 'unknown';
  }
}

const BLOCKER =
  'no bridge row: native/corebridge/js/entry.js exposes radiant.* and quest.* only, so this ' +
  'area has no runner in any language (RUNTIME_CORE_ADOPTION.md sections 12.2 and 13)';

function writeManifest(coreDir) {
  const survey = surveyAreas();
  const packs = surveyPredicatePacks();
  const prev = existsSync(MANIFEST) ? JSON.parse(readFileSync(MANIFEST, 'utf8')) : {};
  const derived = bridgeMethodsFromCpp();
  if (derived.fail) {
    console.error(`check-mechanic-corpora: ${derived.fail}`);
    process.exit(1);
  }
  const areas = {};
  for (const [area, files] of Object.entries(survey)) {
    const before = prev.areas?.[area] ?? {};
    areas[area] = {
      modules: BAND_AREAS[area] ?? [],
      files,
      cases: areaTotal(files),
      executedBy: before.executedBy ?? null,
      blocker: before.executedBy ? undefined : (before.blocker ?? BLOCKER),
    };
  }
  const manifest = {
    description:
      'What the band-120 DECISION-layer corpora are, and which of them this engine can actually ' +
      'execute. `executedBy` is the runner for an area, or null with a stated blocker. ' +
      '`bridgeMethods` mirrors test_mechanic_hosts.cpp so a mechanic row ARRIVING is as loud as ' +
      'one disappearing. Regenerate with `node verify-mechanics/check-mechanic-corpora.mjs ' +
      '--core <packages/core> --write`.',
    source: '@insimul/core (packages/core/conformance)',
    coreCommit: coreDir ? gitCommit(coreDir) : (prev.coreCommit ?? 'unknown'),
    bridgeMethods: derived.methods,
    predicateHalf: {
      note:
        'conformance/prolog/mechanic-*.json is the other half of this adoption and IS executed — ' +
        'all 255 cases in conformance/prolog/ run through insimul::InsimulKB, the plugin’s own ' +
        'libinsimul wrapper, and are diffed against core’s golden solution sets.',
      packs,
      cases: Object.values(packs).reduce((a, b) => a + b, 0),
      executedBy: { file: 'tools/verify-unreal/test_prolog_corpus.cpp', ctest: 'prolog_corpus' },
    },
    areas,
  };
  writeFileSync(MANIFEST, `${JSON.stringify(manifest, null, 2)}\n`);
  console.log(`check-mechanic-corpora: wrote ${MANIFEST.slice(REPO.length + 1)}`);
}

// ---- run --------------------------------------------------------------------

function run() {
  if (!existsSync(MANIFEST)) {
    console.error('check-mechanic-corpora: MECHANIC_CORPORA.json is missing — run with --core <packages/core> --write');
    process.exit(1);
  }
  const manifest = JSON.parse(readFileSync(MANIFEST, 'utf8'));
  console.log('Band-120 decision-corpus survey (what is vendored, and what can run it)');

  const problems = [
    ...checkSurvey(manifest),
    ...checkRunners(manifest),
    ...checkBridgePin(manifest),
    ...checkActivationTable(),
  ];

  // Drift against core itself, when a checkout is at hand.
  if (coreArg) {
    const coreCorpus = join(coreArg, 'conformance');
    const theirs = surveyAreas(coreCorpus);
    const ours = surveyAreas();
    for (const area of new Set([...Object.keys(theirs), ...Object.keys(ours)])) {
      const t = theirs[area] ? areaTotal(theirs[area]) : 0;
      const o = ours[area] ? areaTotal(ours[area]) : 0;
      if (t !== o) problems.push(`DRIFT: core's conformance/${area}/ holds ${t} case(s), this repo vendors ${o}`);
    }
    const theirPacks = surveyPredicatePacks(coreCorpus);
    for (const [file, n] of Object.entries(theirPacks)) {
      const ourPacks = surveyPredicatePacks();
      if (ourPacks[file] !== n) problems.push(`DRIFT: core's conformance/prolog/${file} holds ${n} case(s), this repo vendors ${ourPacks[file] ?? 0}`);
    }
  }

  const controlFailures = negativeControls(manifest);
  for (const f of controlFailures) problems.push(`NEGATIVE CONTROL: ${f}`);

  const survey = surveyAreas();
  const packs = surveyPredicatePacks();
  const decisionCases = total(survey);
  const predicateCases = Object.values(packs).reduce((a, b) => a + b, 0);
  const executable = Object.values(manifest.areas ?? {}).filter((a) => a.executedBy).reduce((n, a) => n + a.cases, 0);

  console.log(
    `  decision half: ${decisionCases} case(s) in ${Object.keys(survey).length} area(s), ` +
      `${executable} executable here`,
  );
  for (const [area, entry] of Object.entries(manifest.areas ?? {})) {
    const how = entry.executedBy ? `run by ${entry.executedBy.ctest}` : `NOT RUN — ${entry.blocker}`;
    console.log(`    ${area.padEnd(10)} ${String(entry.cases).padStart(3)} case(s)  ${how}`);
  }
  console.log(
    `  predicate half: ${predicateCases} case(s) in ${Object.keys(packs).length} pack(s), ` +
      `run by ctest ${manifest.predicateHalf?.executedBy?.ctest}`,
  );
  console.log(`  bridge pin: [${(manifest.bridgeMethods ?? []).join(', ')}] — no mechanic row`);
  console.log(`  negative controls: ${11 - controlFailures.length}/11 fired`);

  if (problems.length > 0) {
    for (const p of problems) console.error(`  x ${p}`);
    console.error(`check-mechanic-corpora: ${problems.length} problem(s)`);
    process.exit(1);
  }
  console.log('  ✓ the vendored band-120 corpora match the manifest, and every unexecuted area names its blocker');
  if (!coreArg) console.log('  i no --core given, so drift against core itself was NOT checked');
}

if (doWrite) {
  writeManifest(coreArg);
} else {
  run();
}
