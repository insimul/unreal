#!/usr/bin/env node
// vendor-conformance.mjs — re-vendor (and verify) the shared conformance corpus.
//
// WHY THIS EXISTS. `conformance/` is a MIRROR of `packages/core/conformance/`,
// the cross-runtime source of truth. This repository is standalone by design, so
// it cannot path-resolve into core — it carries a copy. A copy with no guard
// silently rots: at the start of tasklist 99 the vendored Prolog corpus was
// **41 of core's 76 cases**, `gameplay.json` was a pre-KINP snapshot,
// `conformance/README.md` still described tau-prolog as a second engine, and
// `conformance/content/README.md` claimed to mirror a core directory that does
// not exist. Nothing failed, because nothing was checking.
//
// THIS IS A PORT, ON PURPOSE. Godot's tools/vendor-conformance.mjs hit the same
// rot from the same cause and answered it first (tasklist 100 / its §10.2). The
// manifest shape, the two modes and the flag names are kept IDENTICAL here so
// the three engine repos share one mechanism rather than three lookalikes — the
// same reasoning that made US-1 bind libinsimulcore instead of inventing a
// second bridge. Fix a bug here and it is worth fixing there.
//
//   node tools/vendor-conformance.mjs --core <path-to-packages/core>
//       Re-vendor from a core checkout: copy every mirrored file and rewrite
//       conformance/VENDORED.json with the source commit + per-file sha256.
//
//   node tools/vendor-conformance.mjs --check
//       Verify the checked-in corpus against VENDORED.json — every mirrored file
//       present, hashing what the manifest records, nothing extra hiding inside
//       a mirrored directory, and every area at or above its recorded case
//       FLOOR. Needs no core checkout, so it runs in this repo's gates (ctest
//       `corpus_manifest`, `npm run check:corpus`). Pass --core as well for the
//       REAL drift check: a byte-for-byte diff against the source tree.
//
// COUNTS, THEN A FLOOR (146 US-2). Hashes alone cannot see a corpus SHRINK: a
// file whose `cases` array is trimmed and re-hashed passes every check above.
// `cases` records the exact per-area count of the checked-in tree and is
// re-derived on --check; `caseFloor` is the largest count ever vendored and does
// NOT drop on its own — a re-vendor that would lower one fails and names the
// area, and --allow-corpus-shrink is the explicit, visible act of accepting it.
// Ported from Unity's copy (tasklist 145 US-2), same mechanism, same flag names.
//
// LOCAL, NOT MIRRORED. A few paths under conformance/ are this repo's own and
// have no counterpart in core; they are listed in `local` in the manifest and
// are skipped by both modes. Anything not mirrored and not declared local is an
// error — that is how an undeclared local file (this repo's `content/` fixtures,
// which describe themselves as a mirror of a directory core does not have)
// stops looking like a mirror.

import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const REPO = path.resolve(HERE, '..');
const CORPUS = path.join(REPO, 'conformance');
const MANIFEST = path.join(CORPUS, 'VENDORED.json');

// Paths (relative to conformance/) that are this repo's own, not a mirror.
// Kept here as well as in the manifest so `--core` can regenerate the manifest
// without being told again.
//
// `content/*` is the Unreal content-library import corpus, written in this repo
// for tools/verify-unreal/test_content_library.cpp. Core's shared content-library
// golden is `content-library/*.json` — a DIFFERENT and current shape — which is
// mirrored beside it. The two are not interchangeable; see conformance/content/README.md.
const LOCAL = [
  'VENDORED.json',
  'VENDORED.md',
  'content/README.md',
  'content/golden-vectors.json',
  'content/invalid-cases.json',
  'content/library-basic.json',
  'content/library-golden.json',
];

// CORE-SIDE PATHS DELIBERATELY NOT MIRRORED HERE — the mirror image of LOCAL.
// LOCAL declares files that are this repo's own and have no counterpart in
// core; this declares corpora that DO exist in core and are deliberately not
// vendored, with the reason. Without it, every corpus core adds for a surface
// this repo has not adopted reads as DRIFT, and the real signal drowns.
//
// Prefix match. Every exclusion is PRINTED on each --core run, with a count:
// an exclusion nobody sees is exactly how a corpus silently stops being
// checked. Adding a prefix here is a visible act, not a quiet one.
const NOT_MIRRORED = [
  {
    prefix: 'editor/',
    why:
      'the editor-core corpora (tasklist 101). This repo ships the RUNTIME; ' +
      'editor-core adoption is a later wave — see RUNTIME_CORE_ADOPTION.md. ' +
      'Vendor these when this repo implements the editor core, not before: a ' +
      'corpus with no runner here would be a checked-in file nothing executes.',
  },
  {
    prefix: 'generation/',
    why:
      'the generation-pack corpora (tasklist 134). That track\u2019s surface is the ' +
      'Generation Console and its local tier; the shipped libinsimulcore answers ' +
      'core.methods with no generation.* row (ctest mechanic_bridge pins the list), ' +
      'so these vectors have no runner here. Vendor them with the row, not before.',
  },
  {
    prefix: 'ai/',
    why:
      'the agentAi decision-layer corpus. agentAi is OUT of band 120\u2013125 ' +
      '(this tasklist adopts combat / stamina / perception / traversal / skill / ' +
      'equipment / routine), so no host interface here implements it. Its PREDICATE ' +
      'half IS vendored and executed: conformance/prolog/agent-ai.json.',
  },
  {
    prefix: 'map/',
    why:
      'the map decision-layer corpus. map is OUT of band 120\u2013125; its part-4 host ' +
      'interface (ILocomotionHost) is already implemented for traversal, but nothing ' +
      'here drives map resolution. Its PREDICATE half IS vendored and executed: ' +
      'conformance/prolog/geo-map.json.',
  },
  {
    prefix: 'grounding/',
    why:
      'the KGP grounding packs (tasklist 152). They are pack DATA \u2014 no `cases` ' +
      'array, no golden vectors \u2014 consumed by core\u2019s koine alignment, and this ' +
      'repo has adopted no grounding surface to run them against.',
  },
];

/** The NOT_MIRRORED entry covering `rel`, or undefined. */
function excludedBy(rel) {
  return NOT_MIRRORED.find((n) => rel.startsWith(n.prefix));
}

/** Print every exclusion that actually matched something, so it stays visible. */
function reportExclusions(srcFiles) {
  for (const n of NOT_MIRRORED) {
    const hits = srcFiles.filter((rel) => rel.startsWith(n.prefix));
    if (hits.length > 0) {
      console.log(
        `vendor-conformance: NOT MIRRORED: ${hits.length} file(s) under conformance/${n.prefix} — ${n.why}`,
      );
    }
  }
}

const args = process.argv.slice(2);
const coreArg = argValue('--core') ?? process.env.INSIMUL_CORE_DIR ?? null;
const checkOnly = args.includes('--check');

function argValue(flag) {
  const i = args.indexOf(flag);
  return i >= 0 && i + 1 < args.length ? args[i + 1] : null;
}

function fail(message) {
  console.error(`vendor-conformance: ${message}`);
  process.exit(1);
}

function sha256(buf) {
  return createHash('sha256').update(buf).digest('hex');
}

/** Every file under `dir`, as POSIX paths relative to it, sorted. */
function walk(dir, prefix = '') {
  const out = [];
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const rel = prefix ? `${prefix}/${entry.name}` : entry.name;
    if (entry.isDirectory()) out.push(...walk(path.join(dir, entry.name), rel));
    else if (entry.isFile()) out.push(rel);
  }
  return out.sort();
}

function corpusDir(coreDir) {
  const dir = path.resolve(coreDir, 'conformance');
  if (!fs.existsSync(path.join(dir, 'prolog'))) {
    fail(`--core ${coreDir} does not look like packages/core (no conformance/prolog)`);
  }
  return dir;
}

function gitCommit(dir) {
  try {
    return execFileSync('git', ['-C', dir, 'rev-parse', 'HEAD'], { encoding: 'utf8' }).trim();
  } catch {
    return 'unknown';
  }
}

/**
 * Case counts per corpus area — recorded in the manifest and re-checked, so a
 * SHRINKING corpus is a failure rather than a quiet subset. Hashes alone would
 * not catch it: a file that is deleted and un-declared shows up, but a file
 * whose `cases` array is trimmed and re-hashed does not.
 *
 * A top-level file (README.md, predicate-schema-hash.json) belongs to no area
 * and is not counted; so is any mirrored JSON with no `cases` array (the
 * genre-activation TABLE, the content-library fixtures).
 */
function caseCounts(mirrored) {
  const counts = {};
  for (const rel of mirrored) {
    if (!rel.includes('/') || !rel.endsWith('.json')) continue;
    const area = rel.slice(0, rel.indexOf('/'));
    const p = path.join(CORPUS, rel);
    if (!fs.existsSync(p)) continue;
    let doc;
    try {
      doc = JSON.parse(fs.readFileSync(p, 'utf8'));
    } catch {
      continue; // a malformed mirror is the hash check's problem, not this one
    }
    if (!Array.isArray(doc.cases)) continue;
    counts[area] = (counts[area] ?? 0) + doc.cases.length;
  }
  return counts;
}

/**
 * The FLOOR: per area, the largest case count this repo has ever vendored.
 *
 * `cases` is an exact re-check of the checked-in tree and is rewritten on every
 * re-vendor — so a corpus that SHRINKS core-side re-vendors to a smaller number
 * and the exact check passes against it. The floor does not move down on its
 * own: a re-vendor that would lower one fails and names the area, and
 * --allow-corpus-shrink is the explicit, visible act of accepting it. That is
 * the difference between recording a count and guarding one.
 */
function nextFloor(prevFloor, counts, allowShrink) {
  const floor = { ...(prevFloor ?? {}) };
  const shrunk = [];
  for (const [area, n] of Object.entries(counts)) {
    const was = floor[area] ?? 0;
    if (n < was) shrunk.push(`${area}: ${was} case(s) vendored before, ${n} now`);
    floor[area] = Math.max(was, n);
  }
  for (const area of Object.keys(floor)) {
    if (!(area in counts)) shrunk.push(`${area}: ${floor[area]} case(s) vendored before, the area is GONE now`);
  }
  if (shrunk.length > 0 && !allowShrink) {
    for (const s of shrunk) console.error(`vendor-conformance: CORPUS SHRANK: ${s}`);
    fail(
      `${shrunk.length} area(s) would vendor fewer cases than before. If core really did ` +
        'drop them, re-run with --allow-corpus-shrink and say so in the story notes.',
    );
  }
  if (allowShrink) {
    for (const s of shrunk) console.log(`vendor-conformance: corpus shrink ACCEPTED (--allow-corpus-shrink): ${s}`);
  }
  return floor;
}

function write(coreDir) {
  const src = corpusDir(coreDir);
  const srcFiles = walk(src);
  reportExclusions(srcFiles);
  const files = srcFiles.filter((rel) => !excludedBy(rel));

  // Files this repo mirrored BEFORE this run that core no longer carries (or
  // that a new NOT_MIRRORED prefix now excludes): the mirror is a mirror, so
  // they go. Declared-local paths are never touched.
  const prev = fs.existsSync(MANIFEST) ? JSON.parse(fs.readFileSync(MANIFEST, 'utf8')) : {};
  const keep = new Set(files);
  for (const rel of Object.keys(prev.files ?? {})) {
    if (keep.has(rel)) continue;
    const dead = path.join(CORPUS, rel);
    if (fs.existsSync(dead)) {
      fs.rmSync(dead);
      console.log(`vendor-conformance: removed conformance/${rel} \u2014 core no longer mirrors it here`);
    }
  }

  for (const rel of files) {
    const dest = path.join(CORPUS, rel);
    fs.mkdirSync(path.dirname(dest), { recursive: true });
    fs.copyFileSync(path.join(src, rel), dest);
  }
  const counts = caseCounts(files);
  const floor = nextFloor(prev.caseFloor, counts, args.includes('--allow-corpus-shrink'));
  writeManifest(coreDir, files, counts, floor);
  console.log(`vendor-conformance: mirrored ${files.length} file(s) from core ${gitCommit(path.resolve(coreDir))}`);
  for (const [area, n] of Object.entries(counts)) {
    console.log(`  ${area} cases now: ${n} (floor ${floor[area]})`);
  }
}

function writeManifest(coreDir, files, counts, floor) {
  const manifest = {
    description:
      'Provenance + drift guard for the vendored conformance corpus. `files` is a ' +
      'byte-for-byte mirror of packages/core/conformance; `local` is this repo’s own ' +
      'and mirrors nothing. `cases` is the exact per-area count of the checked-in tree; ' +
      '`caseFloor` is the largest count ever vendored and never drops on its own. ' +
      'Regenerate with `npm run vendor:conformance -- --core <packages/core>`.',
    source: '@insimul/core (packages/core/conformance)',
    coreCommit: coreDir ? gitCommit(path.resolve(coreDir)) : 'unknown',
    cases: counts,
    caseFloor: floor,
    files: Object.fromEntries(
      files.map((rel) => [rel, sha256(fs.readFileSync(path.join(CORPUS, rel)))]),
    ),
    local: LOCAL,
  };
  fs.writeFileSync(MANIFEST, `${JSON.stringify(manifest, null, 2)}\n`);
}

function check(coreDir) {
  if (!fs.existsSync(MANIFEST)) fail('conformance/VENDORED.json is missing — run without --check');
  const manifest = JSON.parse(fs.readFileSync(MANIFEST, 'utf8'));
  const mirrored = Object.keys(manifest.files);
  if (mirrored.length === 0) fail('VENDORED.json records ZERO mirrored files — the guard would check nothing');

  const problems = [];
  for (const rel of mirrored) {
    const p = path.join(CORPUS, rel);
    if (!fs.existsSync(p)) {
      problems.push(`missing mirrored file conformance/${rel}`);
      continue;
    }
    const actual = sha256(fs.readFileSync(p));
    if (actual !== manifest.files[rel]) {
      problems.push(`conformance/${rel} hashes ${actual}, manifest records ${manifest.files[rel]}`);
    }
  }

  // Undeclared files: neither mirrored nor declared local. An unnoticed extra
  // file under a mirrored directory is how a "mirror" stops being one.
  const declared = new Set([...mirrored, ...(manifest.local ?? [])]);
  for (const rel of walk(CORPUS)) {
    if (!declared.has(rel)) {
      problems.push(`conformance/${rel} is neither mirrored nor declared local in VENDORED.json`);
    }
  }

  const counts = caseCounts(mirrored);
  for (const [area, recorded] of Object.entries(manifest.cases ?? {})) {
    if (counts[area] !== recorded) {
      problems.push(`${area} corpus holds ${counts[area] ?? 0} case(s), manifest records ${recorded}`);
    }
  }
  for (const area of Object.keys(counts)) {
    if (!(area in (manifest.cases ?? {}))) {
      problems.push(`${area} holds ${counts[area]} case(s) but the manifest records no count for it`);
    }
  }

  // The floor. `cases` above catches an edit to the checked-in tree; this
  // catches a re-vendor that legitimately rewrote `cases` downward (nextFloor).
  const floor = manifest.caseFloor ?? {};
  if (Object.keys(floor).length === 0) {
    problems.push('VENDORED.json records no caseFloor \u2014 re-vendor with --core to establish one');
  }
  for (const [area, min] of Object.entries(floor)) {
    const have = counts[area] ?? 0;
    if (have < min) {
      problems.push(`${area} corpus holds ${have} case(s), below the recorded floor of ${min}`);
    }
  }

  if (problems.length > 0) {
    for (const p of problems) console.error(`vendor-conformance: ${p}`);
    fail(`${problems.length} corpus drift problem(s) — re-vendor with --core <packages/core>`);
  }
  const total = Object.values(counts).reduce((a, b) => a + b, 0);
  console.log(
    `vendor-conformance: ${mirrored.length} mirrored file(s) consistent, ${total} case(s) in ` +
      `${Object.keys(counts).length} area(s) at or above floor \u2014 ` +
      `${Object.entries(counts).map(([a, n]) => `${n} ${a}`).join(' / ')}, core ${manifest.coreCommit}`,
  );

  if (coreDir) {
    const src = corpusDir(coreDir);
    const srcFiles = walk(src);
    reportExclusions(srcFiles);
    const drift = [];
    for (const rel of srcFiles) {
      if (excludedBy(rel)) continue;
      const here = path.join(CORPUS, rel);
      if (!fs.existsSync(here)) {
        drift.push(`core has conformance/${rel}, this repo does not`);
      } else if (!fs.readFileSync(here).equals(fs.readFileSync(path.join(src, rel)))) {
        drift.push(`conformance/${rel} differs from core's copy`);
      }
    }
    for (const rel of mirrored) {
      if (!srcFiles.includes(rel)) drift.push(`conformance/${rel} is recorded as a mirror but core has no such file`);
    }
    if (drift.length > 0) {
      for (const d of drift) console.error(`vendor-conformance: DRIFT: ${d}`);
      fail(`the vendored corpus differs from ${coreDir} in ${drift.length} place(s) — re-vendor with --core`);
    }
    console.log(`vendor-conformance: byte-identical to ${path.relative(process.cwd(), src) || src}`);
  } else {
    console.log('vendor-conformance: no --core given, so drift against core itself was NOT checked');
  }
}

if (checkOnly) {
  check(coreArg);
} else if (coreArg) {
  write(coreArg);
} else {
  fail('usage: vendor-conformance.mjs --core <path-to-packages/core> [--allow-corpus-shrink] | --check [--core <path>]');
}
