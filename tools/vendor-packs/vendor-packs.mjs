#!/usr/bin/env node
// vendor-packs.mjs — vendor (and verify) core's PREDICATE RULE PACKS as the data an
// exported game consults (US-3 of tasklist 146).
//
// WHY THIS EXISTS, AND WHY IT IS NOT A HAND-PORT. `conformance/modules/genre-activation.json`
// tells a plugin WHICH rule packs a genre consults; core's module contract §7.2 says
// a Unity/Unreal/Godot plugin "reads the genre out of the World IR, looks it up in
// that file, and knows which rule packs to consult". It does not say where the TEXT
// of those packs comes from, and for a native adapter the answer today is nowhere:
// the packs are TypeScript string constants inside core's bundle and the C ABI
// carries no row that returns one (`core.methods` answers with five names — ctest
// `mechanic_bridge` re-measures it every run). So an engine that resolves an active
// module set and then consults nothing has implemented the half of §7.3 that costs
// nothing.
//
// Vendoring the pack TEXT is the same act as vendoring `conformance/` — a
// byte-for-byte mirror behind a manifest that pins core's commit and a sha256 per
// file, with a `--core` mode that does the real diff. No rule is re-expressed in
// C++: libinsimul consults core's own Prolog. RUNTIME_CORE_ADOPTION.md §14.1 is the
// write-up, including the bridge row (`prolog.packs`) that should replace this.
//
//   node vendor-packs/vendor-packs.mjs --check
//       Verify the vendored packs against PACKS.json — every declared pack present,
//       hashing what the manifest records, and nothing extra in the directory. Needs
//       no core checkout, so it runs in `npm run check`.
//
//   node vendor-packs/vendor-packs.mjs --check --core <packages/core>
//       The REAL drift check: re-derive every pack from core and byte-diff.
//
//   node vendor-packs/vendor-packs.mjs --core <packages/core> --write
//       Re-vendor: rewrite every <area>.pl and the manifest.
//
// THE PACKS ARE EXECUTED OUT OF CORE, NOT PARSED — see emit-packs.ts and
// `emitFromCore` below, which measures which runner this host actually has rather
// than requiring the one the Unity probe happened to have.

import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
export const REPO = resolve(HERE, '..', '..');
/** Where an EXPORTED game reads its packs from: Content/Data/insimul/packs, beside
 *  the world data every template loader already reads (FPaths::ProjectContentDir()
 *  / "Data"). templates/project/ is copied verbatim to the project root. */
export const PACKS_DIR = join(REPO, 'templates', 'project', 'Content', 'Data', 'insimul', 'packs');
export const MANIFEST = join(PACKS_DIR, 'PACKS.json');
const EMITTER = join(HERE, 'emit-packs.ts');

const sha256 = (buf) => createHash('sha256').update(buf).digest('hex');

// ---- reading the vendored artifact -----------------------------------------

export function readManifest(path = MANIFEST) {
  if (!existsSync(path)) return null;
  return JSON.parse(readFileSync(path, 'utf8'));
}

/**
 * Verify the checked-in packs against the manifest. Pure: no core, no network, no
 * compiler. Returns a list of problems (empty = clean).
 */
export function checkVendored(manifest = readManifest(), dir = PACKS_DIR) {
  if (!manifest) return ['templates/.../Content/Data/insimul/packs/PACKS.json is missing — the rule packs are not vendored'];
  const problems = [];
  const declared = Object.keys(manifest.packs ?? {});
  if (declared.length === 0) problems.push('PACKS.json declares ZERO packs');

  const order = manifest.consultOrder ?? [];
  if (order.length !== declared.length || order.some((a) => !declared.includes(a))) {
    problems.push(
      `PACKS.json consultOrder [${order.join(', ')}] does not name exactly the declared packs ` +
        `[${declared.slice().sort().join(', ')}]`,
    );
  }
  for (const area of manifest.alwaysActivePacks ?? []) {
    if (!declared.includes(area)) problems.push(`alwaysActivePacks names '${area}', which is not vendored`);
  }

  for (const [area, entry] of Object.entries(manifest.packs ?? {})) {
    const file = join(dir, entry.file);
    if (!existsSync(file)) {
      problems.push(`pack '${area}': ${entry.file} is declared and absent`);
      continue;
    }
    const buf = readFileSync(file);
    if (buf.length !== entry.bytes) {
      problems.push(`pack '${area}': ${entry.file} is ${buf.length} bytes, the manifest records ${entry.bytes}`);
    }
    const got = sha256(buf);
    if (got !== entry.sha256) {
      problems.push(
        `pack '${area}': ${entry.file} hashes ${got.slice(0, 12)}…, the manifest records ${entry.sha256.slice(0, 12)}…`,
      );
    }
    if (buf.length === 0) problems.push(`pack '${area}': ${entry.file} is EMPTY`);
  }

  // Anything in the directory the manifest does not declare is drift in the other
  // direction — a local pack that would be consulted and never checked.
  const known = new Set([...Object.values(manifest.packs ?? {}).map((p) => p.file), 'PACKS.json']);
  if (existsSync(dir)) {
    for (const name of readdirSync(dir).sort()) {
      if (!known.has(name)) problems.push(`${name} sits beside the vendored packs and PACKS.json does not declare it`);
    }
  }
  return problems;
}

// ---- reading core ------------------------------------------------------------

function gitCommit(dir) {
  try {
    return execFileSync('git', ['rev-parse', 'HEAD'], { cwd: dir, encoding: 'utf8' }).trim();
  } catch {
    return 'unknown';
  }
}

/** The nearest `node_modules/.bin/<tool>` at or above `dir`, or null. */
function findBin(dir, tool) {
  let at = resolve(dir);
  for (;;) {
    const candidate = join(at, 'node_modules', '.bin', tool);
    if (existsSync(candidate)) return candidate;
    const up = dirname(at);
    if (up === at) return null;
    at = up;
  }
}

const PAYLOAD_TAG = '@@INSIMUL_PACKS@@';

function payloadOf(stdout, how) {
  const line = (stdout ?? '').split('\n').find((l) => l.startsWith(PAYLOAD_TAG));
  if (!line) throw new Error(`emit-packs.ts produced no payload via ${how}`);
  return JSON.parse(line.slice(PAYLOAD_TAG.length));
}

/**
 * Run emit-packs.ts against a core checkout and return its payload.
 *
 * TWO RUNNERS, BECAUSE THIS HOST HAS ONE OF THEM. The Unity probe shells out to
 * `npx vite-node` inside the core checkout; that needs core's own node_modules,
 * which a core checkout in this workspace does not have. Rather than inherit the
 * requirement, this measures: vite-node when it is resolvable, otherwise an esbuild
 * bundle of the emitter executed by node. Both EXECUTE core's TypeScript — the
 * property that matters (see emit-packs.ts) — and the second needs nothing but the
 * esbuild binary that ships in the monorepo it sits in. The runner used is reported,
 * never guessed at.
 */
export function emitFromCore(coreDir) {
  const core = resolve(coreDir);
  if (!existsSync(join(core, 'package.json'))) throw new Error(`${core} is not a package checkout`);
  const tried = [];

  const viteNode = findBin(core, 'vite-node');
  if (viteNode) {
    try {
      const out = execFileSync(viteNode, [EMITTER], { cwd: core, encoding: 'utf8', maxBuffer: 64 << 20 });
      const payload = payloadOf(out, 'vite-node');
      payload.coreCommit = gitCommit(core);
      payload.runner = `vite-node (${viteNode})`;
      return payload;
    } catch (e) {
      tried.push(`vite-node: ${String(e.stderr ?? e.message ?? e).trim().split('\n').slice(-3).join(' | ')}`);
    }
  } else {
    tried.push('vite-node: not installed at or above the core checkout');
  }

  const esbuild = findBin(core, 'esbuild');
  if (esbuild) {
    const dir = mkdtempSync(join(tmpdir(), 'insimul-packs-'));
    try {
      const bundle = join(dir, 'emit-packs.mjs');
      execFileSync(
        esbuild,
        [
          EMITTER,
          '--bundle',
          '--platform=node',
          '--format=esm',
          `--outfile=${bundle}`,
          // core's package self-reference (`@insimul/core/*` → `src/*`), which is
          // what its own package.json `exports` map declares.
          `--alias:@insimul/core=${join(core, 'src')}`,
          '--log-level=warning',
        ],
        { encoding: 'utf8', maxBuffer: 64 << 20 },
      );
      const out = execFileSync(process.execPath, [bundle], { cwd: core, encoding: 'utf8', maxBuffer: 64 << 20 });
      const payload = payloadOf(out, 'esbuild + node');
      payload.coreCommit = gitCommit(core);
      payload.runner = `esbuild + node (${esbuild})`;
      return payload;
    } catch (e) {
      tried.push(`esbuild: ${String(e.stderr ?? e.message ?? e).trim().split('\n').slice(-3).join(' | ')}`);
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  } else {
    tried.push('esbuild: not installed at or above the core checkout');
  }

  throw new Error(`no runner could execute core's packs:\n      ${tried.join('\n      ')}`);
}

/** Byte-diff the vendored packs against a freshly emitted set. */
export function diffAgainstCore(payload, manifest = readManifest(), dir = PACKS_DIR) {
  const problems = [];
  const emitted = new Map(payload.packs.map((p) => [p.area, p]));

  for (const [area, p] of emitted) {
    const entry = manifest?.packs?.[area];
    if (!entry) {
      problems.push(`core carries pack '${area}' and it is NOT vendored here`);
      continue;
    }
    const file = join(dir, entry.file);
    const have = existsSync(file) ? readFileSync(file, 'utf8') : null;
    if (have === null) {
      problems.push(`pack '${area}': ${entry.file} is absent`);
      continue;
    }
    if (have !== p.prolog) {
      problems.push(`pack '${area}': ${entry.file} DIFFERS from core (${have.length} vs ${p.prolog.length} chars)`);
    }
    const same =
      JSON.stringify([...(entry.runtimePredicates ?? [])].sort()) === JSON.stringify([...p.runtimePredicates].sort());
    if (!same) problems.push(`pack '${area}': the manifest's runtimePredicates differ from core's`);
  }
  for (const area of Object.keys(manifest?.packs ?? {})) {
    if (!emitted.has(area)) problems.push(`pack '${area}' is vendored here and core no longer carries it`);
  }

  const order = manifest?.consultOrder ?? [];
  const coreOrder = payload.packs.map((p) => p.area);
  if (JSON.stringify(order) !== JSON.stringify(coreOrder)) {
    problems.push(
      `consult ORDER has drifted — core consults [${coreOrder.join(', ')}], the manifest records [${order.join(', ')}]. ` +
        'The order is a hard constraint (a `:- dynamic` after a clause is a permission_error), not a preference.',
    );
  }
  const always = JSON.stringify([...(manifest?.alwaysActivePacks ?? [])].sort());
  if (always !== JSON.stringify([...payload.alwaysActivePacks].sort())) {
    problems.push(`alwaysActivePacks has drifted — core says [${payload.alwaysActivePacks.join(', ')}]`);
  }
  return problems;
}

// ---- writing -----------------------------------------------------------------

export function write(payload, dir = PACKS_DIR) {
  mkdirSync(dir, { recursive: true });
  const manifest = {
    description:
      'Provenance + drift guard for the vendored @insimul/core rule packs. Each <area>.pl is a ' +
      'byte-for-byte mirror of that pack’s `prolog` text, and `consultOrder` is core’s own ' +
      'PREDICATE_PACKS order — a hard constraint, not a preference (the routine and map packs write ' +
      'clauses for predicates the substrate declares `:- dynamic`). The plugin consults the packs a ' +
      'genre activates, in this order, and nothing else. Regenerate with ' +
      '`node vendor-packs/vendor-packs.mjs --core <packages/core> --write`.',
    source: '@insimul/core (src/prolog/predicate-packs.ts — PREDICATE_PACKS, EXECUTED rather than parsed)',
    coreCommit: payload.coreCommit ?? 'unknown',
    consultOrder: payload.packs.map((p) => p.area),
    alwaysActivePacks: [...payload.alwaysActivePacks],
    packs: {},
  };

  const keep = new Set(['PACKS.json']);
  for (const p of payload.packs) {
    const file = `${p.area}.pl`;
    writeFileSync(join(dir, file), p.prolog);
    keep.add(file);
    manifest.packs[p.area] = {
      file,
      bytes: Buffer.byteLength(p.prolog),
      sha256: sha256(Buffer.from(p.prolog)),
      runtimePredicates: [...p.runtimePredicates],
    };
  }
  // A pack core dropped must leave, or it stays consultable forever.
  for (const name of readdirSync(dir)) if (!keep.has(name)) rmSync(join(dir, name));

  writeFileSync(MANIFEST, `${JSON.stringify(manifest, null, 2)}\n`);
  return manifest;
}

// ---- entry point ---------------------------------------------------------------

const isMain = process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1];
if (isMain) {
  const args = process.argv.slice(2);
  const at = args.indexOf('--core');
  const coreDir = at >= 0 ? args[at + 1] : process.env.INSIMUL_CORE_DIR ?? null;
  const checkOnly = args.includes('--check');
  const doWrite = args.includes('--write');

  console.log('vendor-packs: core’s rule packs, vendored as the data the exported game consults');

  if (doWrite) {
    if (!coreDir) {
      console.error('  x --write needs --core <packages/core>');
      process.exit(1);
    }
    const payload = emitFromCore(coreDir);
    const manifest = write(payload);
    console.log(`  ok  vendored ${Object.keys(manifest.packs).length} pack(s) from core ${manifest.coreCommit.slice(0, 7)}`);
    console.log(`      runner: ${payload.runner}`);
    console.log(`      consult order: ${manifest.consultOrder.join(' → ')}`);
    process.exit(0);
  }

  const manifest = readManifest();
  const problems = checkVendored(manifest);
  let runner = null;
  if (coreDir && checkOnly) {
    try {
      const payload = emitFromCore(coreDir);
      runner = payload.runner;
      problems.push(...diffAgainstCore(payload, manifest));
    } catch (e) {
      problems.push(`could not read core: ${e.message}`);
    }
  }
  for (const p of problems) console.log(`  x ${p}`);
  if (problems.length > 0) {
    console.error('vendor-packs: FAILED');
    process.exit(1);
  }
  const n = Object.keys(manifest.packs).length;
  console.log(`  ok  ${n} pack(s) present and hashing what PACKS.json records (core ${String(manifest.coreCommit).slice(0, 7)})`);
  if (runner) console.log(`  ok  byte-for-byte against core, re-derived with ${runner}`);
  if (!coreDir && checkOnly) console.log('  .   pass --core <packages/core> for the byte-for-byte drift check');
  console.log('vendor-packs: OK');
}
