
% ═══════════════════════════════════════════════════════════════════════════
% Stamina — thresholds, affordability and regeneration
% docs/mechanic-predicates.md §9
% ═══════════════════════════════════════════════════════════════════════════

% ── Authored facts (world content — never in a save) ──────────────────────
% stamina_regen_base(EntityId, RatePerTick) — the rate the HOST applies per tick.
% stamina_threshold(winded|exhausted, Pct) — the world's balance numbers.
:- dynamic(stamina_regen_base/2).
:- dynamic(stamina_threshold/2).

% ── Facts owned elsewhere that these rules read ──────────────────────────
% energy/3 is runtime gameplay state, in_combat/1 is §4's runtime fact, and
% action/4 is the authored action catalogue whose 4th argument IS the energy
% cost — which is why no stamina_cost/2 exists (§12).
:- dynamic(energy/3).
:- dynamic(in_combat/1).
:- dynamic(action/4).

% ── Rules ─────────────────────────────────────────────────────────────────

% Integer arithmetic on both sides (C * 100 =< M * P), so no float rounding can
% make two engines disagree about who is winded. M > 0 keeps a zero-max actor
% out of the comparison instead of dividing by it.
winded(A) :-
  energy(A, C, M),
  M > 0,
  stamina_threshold(winded, P),
  C * 100 =< M * P.

exhausted(A) :-
  energy(A, C, M),
  M > 0,
  stamina_threshold(exhausted, P),
  C * 100 =< M * P.

% Enough in the tank for what action/4 says this action costs.
can_afford_stamina(A, ActionId) :-
  action(ActionId, _, _, Cost),
  energy(A, C, _),
  C >= Cost.

% The rate the host applies per tick; core decides only the modifier. You don't
% catch your breath mid-fight — hence the cut on the first clause.
stamina_regen(A, 0) :- in_combat(A), !.
stamina_regen(A, Rate) :- stamina_regen_base(A, Rate).
