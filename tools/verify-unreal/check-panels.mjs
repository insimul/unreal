#!/usr/bin/env node
// check-panels.mjs — is there a WIDGET behind every panel the catalog promises
// (US-2 of tasklist 190)?
//
// WHAT THIS GATE IS FOR. The panel catalog (Content/Data/insimul/ui/panels.json) is
// the registry's data: panel key -> the WBP that serves it -> the module that owns
// it. ctest `ui_registry` proves the RESOLUTION over that data — the override layer,
// the module gate, the diagnostics. What it cannot see is whether the widget path a
// row names is ever built: an exported game creates its WBPs from
// templates/scripts/GenerateInsimulContent.py's WIDGET_SPECS, and a catalog row
// whose WBP has no spec resolves to an asset path that does not exist. That is the
// worst failure this suite can have — a panel key that answers "available" and then
// renders nothing — so it is a gate rather than a promise.
//
// WHAT IT CHECKS.
//   1. every catalog row's widget path is /Game/UI/WBP_<Name>, and either
//      WIDGET_SPECS carries WBP_<Name> bound to that panel key, or the key is in
//      PENDING below WITH the tasklist story that owes it;
//   2. no spec claims a panel key the catalog does not have (a key nobody resolves
//      is a widget nobody reaches);
//   3. every spec's `parent` names a C++ UUserWidget this repository actually
//      ships — the failure US-1 found in the UI host tests, which named a runner
//      that does not exist, was exactly this shape.
//
// PENDING is a declaration, not a silence: a key here prints as PENDING with the
// story that owes it, so "22 of 22 rows have a widget" cannot be read off a gate
// that quietly skipped four of them.

import { readFileSync, existsSync, readdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = join(HERE, '..', '..');
const CATALOG = join(ROOT, 'templates', 'project', 'Content', 'Data', 'insimul', 'ui', 'panels.json');
const GENERATOR = join(ROOT, 'templates', 'scripts', 'GenerateInsimulContent.py');
const PLUGIN_PUBLIC = join(ROOT, 'Source', 'InsimulRuntime', 'Public');
const EXPORT_UI = join(ROOT, 'templates', 'source', 'ui');

/**
 * Panel keys the catalog carries that no WBP spec serves yet, each with the story
 * that owes it. 190 US-3 cleared `main_menu`, `pause_menu` and `save_load` — the
 * three UMG panels it landed are generated now, so they are gated rather than
 * declared. What is left is `quest_journal`, whose widget is the export module's
 * own UQuestJournalWidget: it predates the bound-widget convention this script
 * generates for and declares no BindWidget children to name.
 */
const PENDING = {
  quest_journal: 'UQuestJournalWidget (export module) declares no BindWidget children',
};

const WIDGET_PREFIX = '/Game/UI/';

function readCatalog() {
  return JSON.parse(readFileSync(CATALOG, 'utf8'));
}

/**
 * The WIDGET_SPECS dict, read as text rather than executed: this is a Node gate and
 * the file is Python. Only three facts are needed per entry — its WBP name, its
 * `parent` class and its `panel_key` — and each is a single quoted token, so a
 * line scan is exact here rather than an approximation of a parser.
 */
export function readSpecs(source = readFileSync(GENERATOR, 'utf8')) {
  const start = source.indexOf('WIDGET_SPECS = {');
  if (start < 0) return [];
  const body = source.slice(start);
  const specs = [];
  let current = null;
  for (const line of body.split('\n')) {
    const entry = line.match(/^ {4}"(WBP_[A-Za-z0-9_]+)":\s*\{/);
    if (entry) {
      current = { wbp: entry[1], parent: '', panelKey: '' };
      specs.push(current);
      continue;
    }
    if (!current) continue;
    const parent = line.match(/^\s*"parent":\s*"([A-Za-z0-9_]+)"/);
    if (parent) current.parent = parent[1];
    const key = line.match(/^\s*"panel_key":\s*"([a-z0-9_]+)"/);
    if (key) current.panelKey = key[1];
  }
  return specs;
}

/** Every C++ UUserWidget class this repository ships, by class name without the U. */
function shippedWidgetClasses() {
  const classes = new Set();
  for (const dir of [PLUGIN_PUBLIC, EXPORT_UI]) {
    if (!existsSync(dir)) continue;
    for (const file of readdirSync(dir)) {
      if (!file.endsWith('.h')) continue;
      const text = readFileSync(join(dir, file), 'utf8');
      for (const match of text.matchAll(/class\s+\w+_API\s+U(\w+)\s*:\s*public\s+U(?:UserWidget|\w*Widget)/g)) {
        classes.add(match[1]);
      }
    }
  }
  return classes;
}

/** The three checks, over data rather than over the filesystem, so a control can
 *  hand them a mutated world and watch them fail. */
export function evaluate({ rows, specs, classes, pending = PENDING }) {
  const problems = [];
  const served = [];
  const deferred = [];

  const byKey = new Map(specs.filter((s) => s.panelKey).map((s) => [s.panelKey, s]));
  const byWbp = new Map(specs.map((s) => [s.wbp, s]));

  // 1. Every catalog row has a widget, or names the story that owes it.
  for (const row of rows) {
    const widget = String(row.widget ?? '');
    if (!widget.startsWith(WIDGET_PREFIX)) {
      problems.push(`panel '${row.key}' names a widget outside ${WIDGET_PREFIX}: '${widget}'`);
      continue;
    }
    // "/Game/UI/WBP_X.WBP_X_C" -> "WBP_X"
    const asset = widget.slice(WIDGET_PREFIX.length).split('.')[0];
    const spec = byWbp.get(asset);
    if (!spec) {
      if (pending[row.key]) {
        deferred.push(`${row.key} -> ${asset} — ${pending[row.key]}`);
        continue;
      }
      problems.push(`panel '${row.key}' names ${asset}, which no WIDGET_SPECS entry generates`);
      continue;
    }
    if (spec.panelKey !== row.key) {
      problems.push(
        `${asset} is generated for panel key '${spec.panelKey || '(none)'}' but the catalog binds it to '${row.key}'`,
      );
      continue;
    }
    served.push(row.key);
  }

  // 2. No spec claims a key the catalog does not have.
  const catalogKeys = new Set(rows.map((r) => r.key));
  for (const [key, spec] of byKey) {
    if (!catalogKeys.has(key)) {
      problems.push(`${spec.wbp} registers panel key '${key}', which the catalog does not carry`);
    }
  }

  // 3. Every spec's parent class exists.
  for (const spec of specs) {
    if (!spec.parent) {
      problems.push(`${spec.wbp} declares no parent class`);
      continue;
    }
    if (!classes.has(spec.parent)) {
      problems.push(`${spec.wbp} names parent U${spec.parent}, which this repository does not ship`);
    }
  }

  return { problems, served, deferred, keyed: byKey.size };
}

/**
 * A check that cannot fail is a decoration (CLAUDE.md). Each trial mutates one
 * thing and asserts the corresponding check above goes red.
 */
function negativeControls(rows, specs, classes) {
  const failures = [];
  const fires = (name, input) => {
    if (evaluate(input).problems.length === 0) failures.push(name);
  };

  // A catalog row whose WBP nothing generates, and which is not declared PENDING.
  fires('an ungenerated, undeclared panel row', {
    rows: [...rows, { key: 'a_panel_nothing_serves', widget: '/Game/UI/WBP_Nothing.WBP_Nothing_C' }],
    specs,
    classes,
  });
  // PENDING is what silences the row(s) it names, and nothing else: drop the
  // declaration and they are failures again, so the list cannot hide a regression.
  fires('an undeclared PENDING row', { rows, specs, classes, pending: {} });
  // A spec bound to the wrong key.
  fires('a spec generated for a different panel key', {
    rows,
    specs: specs.map((s) => (s.panelKey === 'skill_tree' ? { ...s, panelKey: 'inventory' } : s)),
    classes,
  });
  // A spec whose parent class nobody ships — the US-1 failure shape.
  fires('a spec naming a parent class this repo does not ship', {
    rows,
    specs: specs.map((s) => (s.wbp === 'WBP_HUD' ? { ...s, parent: 'InsimulNoSuchWidget' } : s)),
    classes,
  });
  // A widget path outside the UI package.
  fires('a widget path outside /Game/UI', {
    rows: rows.map((r) => (r.key === 'hud' ? { ...r, widget: 'WBP_HUD' } : r)),
    specs,
    classes,
  });
  return failures;
}

export function run() {
  const catalog = readCatalog();
  const rows = catalog.panels ?? [];
  if (rows.length === 0) return { ok: false, lines: ['  x the panel catalog carries no rows'] };

  const specs = readSpecs();
  if (specs.length === 0) {
    return { ok: false, lines: ['  x no WIDGET_SPECS entries found in GenerateInsimulContent.py'] };
  }
  const classes = shippedWidgetClasses();

  const { problems, served, deferred, keyed } = evaluate({ rows, specs, classes });
  if (problems.length > 0) {
    return { ok: false, lines: problems.map((p) => `  x ${p}`) };
  }

  const out = [];
  out.push(
    `  ok  ${served.length} of ${rows.length} catalog row(s) are generated by a WIDGET_SPECS entry bound to that key`,
  );
  for (const line of deferred) out.push(`  .   PENDING ${line}`);
  out.push(`  ok  no spec registers a panel key the catalog does not carry (${keyed} keyed spec(s))`);
  out.push(`  ok  every spec's parent is a UUserWidget this repository ships (${classes.size} candidate(s))`);

  const controls = negativeControls(rows, specs, classes);
  if (controls.length > 0) {
    return { ok: false, lines: [...out, ...controls.map((c) => `  x negative control did not fire: ${c}`)] };
  }
  out.push('  ok  negative controls: 5/5 fired');
  out.push('  .   the RESOLUTION over this data — override, module gate, diagnostics — is ctest `ui_registry`');
  return { ok: true, lines: out };
}

const isMain = process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1];
if (isMain) {
  console.log('check-panels: a widget behind every panel the catalog promises');
  const r = run();
  for (const l of r.lines) console.log(l);
  if (!r.ok) {
    console.error('check-panels: FAILED');
    process.exit(1);
  }
  console.log('check-panels: OK');
}
