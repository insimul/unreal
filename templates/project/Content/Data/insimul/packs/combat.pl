
% ═══════════════════════════════════════════════════════════════════════════
% Combat — reach, legality and effectiveness
% docs/mechanic-predicates.md §4
% ═══════════════════════════════════════════════════════════════════════════

% ── Authored facts (world content — never in a save) ──────────────────────
% faction(EntityId, FactionId).
% faction_stance(FactionA, FactionB, hostile|neutral|allied).
% resists_damage(EntityId, DamageType) / vulnerable_to(EntityId, DamageType) —
%   DamageType is an atom the WORLD declares (slashing, fire, silver, …); core
%   fixes no list, because the set is content.
:- dynamic(faction/2).
:- dynamic(faction_stance/3).
:- dynamic(resists_damage/2).
:- dynamic(vulnerable_to/2).

% ── Runtime facts (per playthrough — save.currentState.prologFacts) ───────
% in_combat/1, combat_target/2, threat/3 and incapacitated/1 differ between two
% playthroughs of the same world, so authoring any of them on a world template
% is a design error, not a schema entry (§3).
:- dynamic(in_combat/1).
:- dynamic(combat_target/2).
:- dynamic(threat/3).
:- dynamic(incapacitated/1).

% ── Facts owned elsewhere that these rules read ──────────────────────────
% alive/1 is the character block's (see the module header); near/3 and
% has_equipped/3 are runtime gameplay state (§10.1); item_range/2 and
% item_damage_type/2 are authored weapon stats on the item block.
% Declared here so this pack answers "no" instead of raising in a KB that
% carries none of them.
:- dynamic(alive/1).
:- dynamic(near/3).
:- dynamic(has_equipped/3).
:- dynamic(item_range/2).
:- dynamic(item_damage_type/2).

% ── Rules ─────────────────────────────────────────────────────────────────

% Hostility is a property of the factions, not of the pair: two members of
% mutually hostile factions are hostile without anyone authoring the pair.
hostile_to(A, B) :-
  faction(A, FA),
  faction(B, FB),
  faction_stance(FA, FB, hostile).

% A legal target: still standing, not already down, and an enemy. incapacitated/1
% is deliberately distinct from dead/1 — "down but not dead" is its own decision
% state (lootable, untalkable, and NOT a legal attack target).
can_attack(A, T) :-
  alive(T),
  \+ incapacitated(T),
  hostile_to(A, T).

% "You may only swing at something you can actually reach with what you're
% holding." W is the weapon that reaches, so a caller can name it.
in_reach(A, T, W) :-
  has_equipped(A, weapon, W),
  item_range(W, R),
  near(A, T, R).

% The weapon's damage type means something against this target. The world
% authors what resists what; core owns no damage formula.
effective_against(W, T) :-
  item_damage_type(W, DT),
  \+ resists_damage(T, DT).
