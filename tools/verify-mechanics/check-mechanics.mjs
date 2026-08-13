#!/usr/bin/env node
// check-mechanics.mjs — assert that the band-120 host interfaces this plugin declares
// are still the ones core's module manifest names, and that every one of them is
// implemented or explicitly stubbed (US-1 of tasklist 146).
//
// WHY THIS EXISTS. `Source/InsimulRuntime/Portable/InsimulMechanicContracts.h` is a
// hand-written mirror of core's `host-contracts.ts` / `system-contracts.ts`, and this
// repo has already shipped a hand-written mirror that rotted for as long as nothing
// diffed it: `conformance/` claimed to be core's corpus while carrying 41 of its 76
// Prolog cases (RUNTIME_CORE_ADOPTION.md §6.3). A mirror nothing checks rots. This is
// the check, and it covers the three things a mirror can silently lose:
//
//   1. a MODULE — core names seven in band 120-125 and this plugin's table has to
//      carry the same seven with the same host interfaces and decision layers;
//   2. a MEMBER — an interface that quietly drops a method still compiles here and
//      silently narrows the boundary;
//   3. an IMPLEMENTATION — an interface nothing implements is a sketch, and one
//      stubbed without a stated consequence is the "silent no-op" US-1 forbids.
//
// WHAT IT PROVES, AND WHAT IT DOES NOT. It proves NAMES and COUNTS: module ids, host
// interface names, member names, and that some C++ class declares each interface as a
// base. It says nothing about signatures, parameter order, semantics or behaviour. The
// portable half's BEHAVIOUR is executed by tools/verify-unreal (ctest `mechanic_hosts`,
// 106 checks) and the Unreal implementations in templates/source/mechanics/ are
// executed by nothing here — no gate in this repo can build a UE translation unit
// (verify-unreal/check.mjs states the same limit). VERIFICATION.md is their pass.
//
// IT NEEDS NO CORE CHECKOUT, AND IS BETTER WITH ONE. Default mode compares the C++
// against `MODULE_HOSTS.json`, which carries core's commit and the sha256 of the three
// files it was derived from — that runs anywhere, so `npm run check` requires it. Pass
// `--core <packages/core>` and it re-derives everything from core's own TypeScript and
// diffs the manifest too, which is the only way to catch core having MOVED. `--write`
// rewrites the manifest from a core checkout (the vendoring step).
//
// THE PARSERS ARE LINE-ORIENTED, DELIBERATELY. One member per line in the header and
// one brace-initialised entry per module: a declaration wrapped across lines is
// reported as a missing member rather than silently skipped, which is the failure
// direction a gate should have.
//
// IT IS A PORT OF UNITY'S SCRIPT OF THE SAME NAME, not a lookalike — one mechanism
// across the engine repos, the same discipline vendor-conformance.mjs is held to.
//
//   cd tools && node verify-mechanics/check-mechanics.mjs
//   cd tools && node verify-mechanics/check-mechanics.mjs --core ../../babylon/packages/core
//   cd tools && node verify-mechanics/check-mechanics.mjs --core <path> --write

import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import { existsSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
const REPO_ROOT = join(HERE, '..', '..');
const MANIFEST_PATH = join(HERE, 'MODULE_HOSTS.json');

/** The C++ this gate reads. Every one of them is this plugin's own. */
const CONTRACTS_H = 'Source/InsimulRuntime/Portable/InsimulMechanicContracts.h';
const SURFACE_CPP = 'Source/InsimulRuntime/Portable/InsimulMechanicSurface.cpp';
const HOSTS_CPP = 'Source/InsimulRuntime/Portable/InsimulMechanicHosts.cpp';

/** The core sources the manifest is derived from, relative to a `packages/core`. */
const CORE_SOURCES = {
  modules: 'src/modules/module-contract.ts',
  host: 'src/game-engine/host-contracts.ts',
  system: 'src/game-engine/system-contracts.ts',
};

/** The tasklists whose modules this plugin adopts. */
const BAND = ['120', '121', '122', '123', '124', '125'];

/**
 * Paths whose classes are NOT implementations of the boundary. A test fixture that
 * derives from an interface to drive a table is not an implementation of it — counting
 * test_mechanic_hosts.cpp's stubs would let every interface pass check 3 forever, which
 * is the decorative-gate failure the negative controls exist to catch.
 */
const NOT_AN_IMPLEMENTATION = ['tools/', 'Source/InsimulRuntime/Tests/', 'Source/InsimulEditor/Tests/'];

const sha256 = (text) => createHash('sha256').update(text).digest('hex');
const pascal = (name) => (name ? name[0].toUpperCase() + name.slice(1) : name);

// ── reading core (only with --core) ───────────────────────────────────────────

/**
 * Pull `{ id, tasklist, decisionLayer[], hostInterface[] }` out of core's
 * INSIMUL_MODULES for the band-120 modules only. `tasklist` is not a field of the
 * manifest entry — a conforming module has no `pending` block — so it comes from the
 * `US-n of NNN-...` citation in the entry's own comments, which is where core states it.
 */
export function deriveModulesFromCore(text) {
  const start = text.indexOf('export const INSIMUL_MODULES');
  if (start < 0) return { error: 'INSIMUL_MODULES not found in core' };
  const body = text.slice(start);

  const modules = [];
  const entry = /\n  \{\n    id: '([a-zA-Z]+)',/g;
  let m;
  const marks = [];
  while ((m = entry.exec(body)) !== null) marks.push({ id: m[1], at: m.index });
  for (let i = 0; i < marks.length; i++) {
    const chunk = body.slice(marks[i].at, i + 1 < marks.length ? marks[i + 1].at : body.length);
    const list = (key) => {
      const hit = new RegExp(`${key}: \\[([^\\]]*)\\]`).exec(chunk);
      if (!hit) return [];
      return [...hit[1].matchAll(/'([^']+)'/g)].map((x) => x[1]);
    };
    const tasklists = [...chunk.matchAll(/`(1[0-9]{2})-[a-z0-9-]+`/g)].map((x) => x[1]);
    const banded = tasklists.filter((t) => BAND.includes(t));
    modules.push({
      id: marks[i].id,
      tasklist: banded.length ? banded[0] : null,
      decisionLayer: list('decisionLayer'),
      hostInterface: list('hostInterface'),
    });
  }
  return { modules };
}

/**
 * Every `export interface X { … }` in one core file, as `name -> [member names]`.
 * Methods and fields both: a member is an identifier that opens a line and is followed
 * by `(`, `?(`, `:` or `?:`.
 */
export function deriveInterfacesFromCore(text) {
  const out = {};
  const decl = /export interface (\w+)[^{]*\{/g;
  let m;
  while ((m = decl.exec(text)) !== null) {
    const from = decl.lastIndex;
    const end = text.indexOf('\n}', from);
    if (end < 0) continue;
    const body = text.slice(from, end);
    const members = [];
    for (const line of body.split('\n')) {
      const hit = /^  (\w+)\??\s*[(:]/.exec(line);
      if (hit && !members.includes(hit[1])) members.push(hit[1]);
    }
    out[m[1]] = members;
  }
  return out;
}

/** Build the whole manifest from a core checkout. */
export function deriveManifest(coreDir) {
  const read = (rel) => readFileSync(join(coreDir, rel), 'utf8');
  const modulesText = read(CORE_SOURCES.modules);
  const hostText = read(CORE_SOURCES.host);
  const systemText = read(CORE_SOURCES.system);

  const derived = deriveModulesFromCore(modulesText);
  if (derived.error) throw new Error(derived.error);

  const hostIfaces = deriveInterfacesFromCore(hostText);
  const systemIfaces = deriveInterfacesFromCore(systemText);

  const band = derived.modules.filter((mod) => BAND.includes(mod.tasklist));
  const interfaces = {};
  for (const mod of band) {
    for (const name of mod.hostInterface) {
      if (interfaces[name]) continue;
      if (hostIfaces[name]) interfaces[name] = { source: CORE_SOURCES.host, members: hostIfaces[name] };
      else if (systemIfaces[name]) interfaces[name] = { source: CORE_SOURCES.system, members: systemIfaces[name] };
      else interfaces[name] = { source: '(NOT FOUND IN CORE)', members: [] };
    }
  }

  let coreCommit = '';
  try {
    coreCommit = execFileSync('git', ['rev-parse', 'HEAD'], { cwd: coreDir, encoding: 'utf8' }).trim();
  } catch {
    coreCommit = '';
  }

  return {
    description:
      'Provenance + drift guard for the band-120 host interfaces this plugin implements. ' +
      '`modules` and `interfaces` are derived from core; `stubbed` is this repo’s own side. ' +
      'Regenerate with `node verify-mechanics/check-mechanics.mjs --core <packages/core> --write`.',
    source: '@insimul/core (src/modules/module-contract.ts, src/game-engine/{host,system}-contracts.ts)',
    coreCommit,
    band: BAND.join(', '),
    files: {
      [CORE_SOURCES.modules]: sha256(modulesText),
      [CORE_SOURCES.host]: sha256(hostText),
      [CORE_SOURCES.system]: sha256(systemText),
    },
    modules: band.map((mod) => ({
      id: mod.id,
      tasklist: mod.tasklist,
      decisionLayer: mod.decisionLayer,
      hostInterface: mod.hostInterface,
    })),
    interfaces,
  };
}

// ── reading this plugin's C++ ────────────────────────────────────────────────

/** Git-tracked C/C++ sources, excluding the paths in NOT_AN_IMPLEMENTATION. */
export function trackedCpp() {
  const out = execFileSync('git', ['ls-files'], { cwd: REPO_ROOT, encoding: 'utf8' });
  return out
    .split('\n')
    .map((l) => l.trim())
    .filter(Boolean)
    .filter((p) => /\.(h|hpp|cpp|cc|inl)$/i.test(p))
    .filter((p) => !NOT_AN_IMPLEMENTATION.some((prefix) => p.startsWith(prefix)))
    .sort();
}

/**
 * `class IX { … };` in one C++ header, as `name -> [member names]`. A member is the
 * identifier immediately before the first `(` on a line that opens with `virtual`, and
 * the destructor is skipped — one member per line, which is how the file is written.
 */
export function parseCppInterfaces(text) {
  const out = {};
  const decl = /\nclass (I[A-Z]\w*) \{/g;
  let m;
  while ((m = decl.exec(text)) !== null) {
    const from = decl.lastIndex;
    const end = text.indexOf('\n};', from);
    if (end < 0) continue;
    const body = text.slice(from, end);
    const members = [];
    for (const line of body.split('\n')) {
      const hit = /^\tvirtual [\w:&<>,\s*]*?(\w+)\(/.exec(line);
      if (hit && hit[1] !== m[1].slice(1) && !members.includes(hit[1])) members.push(hit[1]);
    }
    out[m[1]] = members;
  }
  return out;
}

/**
 * The module table in InsimulMechanicSurface.cpp. Each entry is a brace initialiser:
 * `{"id", "tasklist", {decision…}, {interface…}, {row…}}`.
 */
export function parseCppModules(text) {
  const out = [];
  const entry =
    /\{"([a-zA-Z]+)", "(\d+)",\s*\{([^}]*)\},\s*\{([^}]*)\},\s*\{([^}]*)\}\}/g;
  let m;
  const atoms = (s) => [...s.matchAll(/"([^"]+)"/g)].map((x) => x[1]);
  while ((m = entry.exec(text)) !== null) {
    out.push({
      id: m[1],
      tasklist: m[2],
      decisionLayer: atoms(m[3]),
      hostInterface: atoms(m[4]),
      requiredMethods: atoms(m[5]),
    });
  }
  return out;
}

/** Interface names that have an arm in `ConsequenceOf`. */
export function parseConsequences(text) {
  const start = text.indexOf('std::string FInsimulHostAdapter::ConsequenceOf');
  if (start < 0) return [];
  const body = text.slice(start);
  const end = body.indexOf('\n}');
  return [...body.slice(0, end < 0 ? body.length : end).matchAll(/HostInterface == "(\w+)"/g)].map((m) => m[1]);
}

/**
 * `interface -> [files]` for every C++ class whose base list names it. Deliberately
 * regex-level: a base list is a comma-separated list of names on one line here.
 */
export function findImplementations(files, readFile) {
  const out = {};
  for (const rel of files) {
    const text = readFile(rel);
    for (const m of text.matchAll(/\bclass\s+\w+\s*:\s*([^\n{]+)/g)) {
      for (const base of m[1].split(',').map((s) => s.trim())) {
        const name = /^([A-Z]\w*)/.exec(base.replace(/^public\s+/, '').replace(/^[\w]*::/, ''));
        if (!name || !/^I[A-Z]/.test(name[1])) continue;
        if (!out[name[1]]) out[name[1]] = [];
        if (!out[name[1]].includes(rel)) out[name[1]].push(rel);
      }
    }
  }
  return out;
}

// ── the check ────────────────────────────────────────────────────────────────

/**
 * Compare the manifest against the C++. `sources` is injected so the negative controls
 * can mutate one file without touching the disk.
 */
export function checkAgainstManifest(manifest, sources) {
  const errors = [];
  const contracts = parseCppInterfaces(sources.contracts);
  const modules = parseCppModules(sources.surface);
  const consequences = parseConsequences(sources.hosts);
  const impls = sources.implementations;
  const stubbed = manifest.stubbed || {};

  // 1. the module table
  for (const want of manifest.modules) {
    const got = modules.find((mod) => mod.id === want.id);
    if (!got) {
      errors.push(`module '${want.id}' (tasklist ${want.tasklist}) is missing from ${SURFACE_CPP}`);
      continue;
    }
    if (got.tasklist !== want.tasklist) {
      errors.push(`module '${want.id}': tasklist '${got.tasklist}' != core's '${want.tasklist}'`);
    }
    for (const [label, a, b] of [
      ['hostInterface', got.hostInterface, want.hostInterface],
      ['decisionLayer', got.decisionLayer, want.decisionLayer],
    ]) {
      const missing = b.filter((x) => !a.includes(x));
      const extra = a.filter((x) => !b.includes(x));
      if (missing.length) errors.push(`module '${want.id}' ${label}: missing ${missing.join(', ')}`);
      if (extra.length) errors.push(`module '${want.id}' ${label}: declares ${extra.join(', ')}, which core does not`);
    }
    if (!got.requiredMethods.length) {
      errors.push(`module '${want.id}': no proposed bridge row, so "is it reachable" cannot be asked`);
    }
  }
  for (const got of modules) {
    if (!manifest.modules.some((mod) => mod.id === got.id)) {
      errors.push(`module '${got.id}' is in ${SURFACE_CPP} and not in core's band-120 manifest`);
    }
  }

  // 2. the interfaces, member by member
  for (const name of Object.keys(manifest.interfaces)) {
    const spec = manifest.interfaces[name];
    const members = contracts[name];
    if (!members) {
      errors.push(`interface ${name} is not declared in ${CONTRACTS_H}`);
      continue;
    }
    for (const member of spec.members) {
      if (!members.includes(pascal(member))) {
        errors.push(`${name}.${pascal(member)} (core's ${member}) is missing from ${CONTRACTS_H}`);
      }
    }
    const known = spec.members.map(pascal);
    for (const member of members) {
      if (!known.includes(member)) {
        errors.push(`${name}.${member} is declared here and is not a member of core's ${name}`);
      }
    }
  }

  // 3. an implementation, or a stub with a stated consequence
  for (const name of Object.keys(manifest.interfaces)) {
    const where = impls[name] || [];
    if (where.length === 0 && !stubbed[name]) {
      errors.push(
        `interface ${name} is implemented by nothing and is not listed in \`stubbed\` — ` +
          `US-1 forbids a silent no-op`,
      );
    }
    if (stubbed[name] && where.length > 0) {
      errors.push(`interface ${name} is listed as stubbed and is implemented by ${where.join(', ')}`);
    }
    if (stubbed[name] !== undefined && !String(stubbed[name]).trim()) {
      errors.push(`interface ${name} is stubbed with no documented consequence`);
    }
    if (!consequences.includes(name)) {
      errors.push(
        `interface ${name} has no arm in FInsimulHostAdapter::ConsequenceOf — an absent host must state its cost`,
      );
    }
  }

  return errors;
}

/** Diff a core checkout against the manifest — names AND bytes. */
export function checkAgainstCore(manifest, coreDir) {
  const errors = [];
  const derived = deriveManifest(coreDir);

  for (const rel of Object.keys(manifest.files)) {
    const want = manifest.files[rel];
    const got = derived.files[rel];
    if (got !== want) errors.push(`${rel}: sha256 ${got} != manifest's ${want} — re-vendor with --write`);
  }
  if (manifest.coreCommit && derived.coreCommit && manifest.coreCommit !== derived.coreCommit) {
    errors.push(
      `core is at ${derived.coreCommit} and the manifest was vendored at ${manifest.coreCommit} ` +
        `(not fatal on its own — the sha256s above are the real check)`,
    );
  }
  for (const want of derived.modules) {
    const got = manifest.modules.find((mod) => mod.id === want.id);
    if (!got) {
      errors.push(`core's band-120 module '${want.id}' is absent from the manifest`);
      continue;
    }
    if (JSON.stringify(got.hostInterface) !== JSON.stringify(want.hostInterface)) {
      errors.push(
        `module '${want.id}' hostInterface drifted: manifest ${JSON.stringify(got.hostInterface)} ` +
          `!= core ${JSON.stringify(want.hostInterface)}`,
      );
    }
  }
  for (const name of Object.keys(derived.interfaces)) {
    const spec = derived.interfaces[name];
    const got = manifest.interfaces[name];
    if (!got) {
      errors.push(`core's ${name} is absent from the manifest`);
      continue;
    }
    if (JSON.stringify(got.members) !== JSON.stringify(spec.members)) {
      errors.push(
        `${name} members drifted: manifest ${JSON.stringify(got.members)} != core ${JSON.stringify(spec.members)}`,
      );
    }
  }
  return errors;
}

/**
 * Prove the gate can fail. Four mutations, one per check above; a mutation that
 * produced no error would mean the check is decorative.
 */
export function negativeControls(manifest, sources) {
  const failures = [];
  const trial = (label, mutate) => {
    const mutated = {
      contracts: sources.contracts,
      surface: sources.surface,
      hosts: sources.hosts,
      implementations: Object.assign({}, sources.implementations),
    };
    mutate(mutated);
    if (checkAgainstManifest(manifest, mutated).length === 0) {
      failures.push(`negative control '${label}' produced NO error — the check is decorative`);
    }
  };

  const anIface = Object.keys(manifest.interfaces)[0];
  trial('a dropped interface member', (m) => {
    const member = pascal(manifest.interfaces[anIface].members[0]);
    m.contracts = m.contracts.replace(new RegExp(`\\b${member}\\(`), 'Renamed$&');
  });
  trial('a dropped module', (m) => {
    m.surface = m.surface.replace(`{"${manifest.modules[0].id}", "`, '{"other", "');
  });
  trial('an unimplemented interface', (m) => {
    delete m.implementations[anIface];
  });
  trial('a consequence with no arm', (m) => {
    m.hosts = m.hosts.replace(`HostInterface == "${anIface}"`, 'HostInterface == "Xx"');
  });
  return failures;
}

// ── entry point ──────────────────────────────────────────────────────────────

function argOf(flag) {
  const i = process.argv.indexOf(flag);
  return i >= 0 ? process.argv[i + 1] : null;
}

export function run() {
  const coreDir = argOf('--core');
  const write = process.argv.includes('--write');

  if (write) {
    if (!coreDir) return { ok: false, lines: ['--write needs --core <packages/core>'] };
    const derived = deriveManifest(coreDir);
    const existing = existsSync(MANIFEST_PATH) ? JSON.parse(readFileSync(MANIFEST_PATH, 'utf8')) : {};
    // `stubbed` is this repo's own and survives a re-vendor.
    const next = Object.assign({}, derived, { stubbed: existing.stubbed || {} });
    writeFileSync(MANIFEST_PATH, `${JSON.stringify(next, null, 2)}\n`);
    return { ok: true, lines: [`wrote ${MANIFEST_PATH} from core ${derived.coreCommit || '(no git)'}`] };
  }

  if (!existsSync(MANIFEST_PATH)) {
    return { ok: false, lines: [`missing ${MANIFEST_PATH} — vendor it with --core <path> --write`] };
  }
  const manifest = JSON.parse(readFileSync(MANIFEST_PATH, 'utf8'));
  const read = (rel) => readFileSync(join(REPO_ROOT, rel), 'utf8');
  const files = trackedCpp();
  const sources = {
    contracts: read(CONTRACTS_H),
    surface: read(SURFACE_CPP),
    hosts: read(HOSTS_CPP),
    implementations: findImplementations(files, read),
  };

  const lines = [];
  let ok = true;

  const errors = checkAgainstManifest(manifest, sources);
  if (errors.length) {
    ok = false;
    lines.push(`  ✗ manifest vs C++: ${errors.length} problem(s)`);
    for (const e of errors) lines.push(`      ${e}`);
  } else {
    const ifaces = Object.keys(manifest.interfaces);
    let members = 0;
    for (const name of ifaces) members += manifest.interfaces[name].members.length;
    lines.push(
      `  ✓ manifest vs C++: ${manifest.modules.length} module(s), ${ifaces.length} host interface(s), ` +
        `${members} member(s)`,
    );
    for (const name of ifaces) {
      const where = sources.implementations[name] || [];
      const note = where.length
        ? where.map((p) => p.replace(/^templates\/source\//, '')).join(', ')
        : `STUBBED: ${(manifest.stubbed || {})[name]}`;
      lines.push(`      ${name} <- ${note}`);
    }
  }

  const controls = negativeControls(manifest, sources);
  if (controls.length) {
    ok = false;
    for (const c of controls) lines.push(`  ✗ ${c}`);
  } else {
    lines.push('  ✓ negative controls: a dropped member, module, implementation and consequence each fail');
  }

  if (coreDir) {
    if (!existsSync(join(coreDir, CORE_SOURCES.modules))) {
      ok = false;
      lines.push(`  ✗ --core ${coreDir} does not look like a packages/core (no ${CORE_SOURCES.modules})`);
    } else {
      const drift = checkAgainstCore(manifest, coreDir);
      if (drift.length) {
        ok = false;
        lines.push(`  ✗ manifest vs core: ${drift.length} problem(s)`);
        for (const d of drift) lines.push(`      ${d}`);
      } else {
        lines.push('  ✓ manifest vs core: byte-identical sources, same modules, same members');
      }
    }
  } else {
    lines.push('  i no --core given, so drift against core itself was NOT checked');
  }

  return { ok, lines };
}

if (import.meta.url === `file://${process.argv[1]}`) {
  console.log('Band-120 mechanic host gate (names and counts — not signatures)');
  const res = run();
  for (const line of res.lines) console.log(line);
  process.exit(res.ok ? 0 : 1);
}
