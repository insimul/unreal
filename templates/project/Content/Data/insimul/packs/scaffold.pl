
% ═══════════════════════════════════════════════════════════════════════════
% Whole-game scaffolding — the pacing and the endings, evaluated
% packages/core/docs/whole-game-scaffolding.md §7
%
% The authored facts are src/scaffold/scaffold-document.ts's sixteen; the rules
% here are the only thing in the build that READS a gate. Nothing below names a
% mechanic, a threshold or an ending: a scaffold that needed a rule of its own
% per genre would be a game designer, which §5 of the design refuses.
% ═══════════════════════════════════════════════════════════════════════════

% ── Authored facts (the scaffold document — world content, never in a save) ─
% scaffold(ScaffoldId) — the scaffold itself.
% scaffold_genre/2, scaffold_world/2, scaffold_seed/2 — provenance.
% scaffold_module(S, ModuleId) — the mechanics this game HAS.
% scaffold_start_place/2, scaffold_start_role/2, scaffold_start_time/2 — where
%   the player begins, as what, and when.
% scaffold_start_because(S, Goal) — a goal that was true of the world when the
%   start was chosen. Evidence a creator can re-read and a rule can re-check.
% scaffold_objective(S, Key, Source, Ref, Order) — one initial objective, by
%   reference to the machinery that owns it (quest / radiant / arc).
% scaffold_stage(S, Stage, Order), scaffold_stage_gate(S, Stage, Goal),
%   scaffold_stage_unlocks(S, Stage, ObjectiveKey) — the pacing.
% scaffold_ending(S, Key, Outcome), scaffold_ending_when(S, Key, Goal) — what
%   finishes this game, and on what.
% scaffold_edited(S, Section) — the section a creator has taken over.
:- dynamic(scaffold/1).
:- dynamic(scaffold_genre/2).
:- dynamic(scaffold_world/2).
:- dynamic(scaffold_seed/2).
:- dynamic(scaffold_module/2).
:- dynamic(scaffold_start_place/2).
:- dynamic(scaffold_start_role/2).
:- dynamic(scaffold_start_time/2).
:- dynamic(scaffold_start_because/2).
:- dynamic(scaffold_objective/5).
:- dynamic(scaffold_stage/3).
:- dynamic(scaffold_stage_gate/3).
:- dynamic(scaffold_stage_unlocks/3).
:- dynamic(scaffold_ending/3).
:- dynamic(scaffold_ending_when/3).
:- dynamic(scaffold_edited/2).

% ── Runtime facts (per playthrough) ───────────────────────────────────────
% scaffold_active(ScaffoldId) — which scaffold this playthrough is running.
% The ONLY thing this layer remembers. Which stage is open, which objectives
% are available and whether the game has ended are questions about the world,
% and the world already answers them; a second copy could disagree.
:- dynamic(scaffold_active/1).

% ── The meta-call ─────────────────────────────────────────────────────────

% Does this authored goal hold right now?
%
% Every gate, ending and justification below is reached through here, and the
% catch is load-bearing: a goal naming a predicate no active pack defines
% RAISES under ISO rather than failing, so a mechanic the creator switched off
% would end a playthrough with an existence error instead of a closed gate.
scaffold_holds(G) :- catch(call(G), _, fail).

% ── Which scaffold is in play ─────────────────────────────────────────────

% The running scaffold: the one a host declared, or — in a KB carrying exactly
% the scaffold it was built from — that one. Semidet either way, so a host that
% never declares one still gets an answer rather than an enumeration.
scaffold_in_play(S) :- scaffold_active(S), !, scaffold(S).
scaffold_in_play(S) :- \+ scaffold_active(_), scaffold(S), !.

% Where the player begins, as one question.
scaffold_start(S, Place, Role, Time) :-
  scaffold_start_place(S, Place),
  scaffold_start_role(S, Role),
  scaffold_start_time(S, Time).

% A reason the start was chosen for that is no longer true of the world.
%
% Not an error — worlds move, and a mayor dying does not invalidate a start.
% It is the question a creator regenerating a scaffold wants answered, and it
% is only askable because US-1 stored the reasoning rather than the result.
scaffold_start_unjustified(S, Goal) :-
  scaffold_start_because(S, Goal),
  \+ scaffold_holds(Goal).

% ── Pacing ────────────────────────────────────────────────────────────────

% A stage whose gate holds. The gate is a GOAL, so faction standing, genealogy
% and the world clock reach this slot exactly as a quest count does, and
% nothing here knows the difference.
scaffold_stage_open(S, Stage) :-
  scaffold_stage(S, Stage, _),
  scaffold_stage_gate(S, Stage, Gate),
  scaffold_holds(Gate).

scaffold_stage_blocked(S, Stage) :-
  scaffold_stage(S, Stage, _),
  \+ scaffold_stage_open(S, Stage).

% The furthest-along open stage — what a host shows as "where you are".
% Defined by the authored ORDER and not by arrival, because a gate that closes
% again (a faction made peace) must be able to take the game back a step.
scaffold_current_stage(S, Stage) :-
  scaffold_stage_open(S, Stage),
  scaffold_stage(S, Stage, N),
  \+ ( scaffold_stage_open(S, Other),
        scaffold_stage(S, Other, M),
        M > N ).

% ── Objectives ────────────────────────────────────────────────────────────

scaffold_objective_gated(S, Key) :- scaffold_stage_unlocks(S, _, Key).

% Semidet on purpose: two open stages unlocking one objective is an authoring
% choice, not two objectives, and a duplicate solution here would double it in
% every list a host draws.
scaffold_objective_unlocked(S, Key) :- \+ scaffold_objective_gated(S, Key), !.
scaffold_objective_unlocked(S, Key) :-
  scaffold_stage_unlocks(S, Stage, Key),
  scaffold_stage_open(S, Stage),
  !.

scaffold_objective_available(S, Key) :-
  scaffold_objective(S, Key, _, _, _),
  scaffold_objective_unlocked(S, Key).

scaffold_objective_locked(S, Key) :-
  scaffold_objective(S, Key, _, _, _),
  \+ scaffold_objective_unlocked(S, Key).

% How much of the game is open. Total is what the scaffold seeded, so a
% progress readout can never promise more than exists.
scaffold_progress(S, Available, Total) :-
  scaffold(S),
  findall(A, scaffold_objective_available(S, A), Open),
  length(Open, Available),
  findall(T, scaffold_objective(S, T, _, _, _), All),
  length(All, Total).

% ── Endings ───────────────────────────────────────────────────────────────

% An end condition that has been met, and what reaching it means.
scaffold_ending_reached(S, Key, Outcome) :-
  scaffold_ending(S, Key, Outcome),
  scaffold_ending_when(S, Key, When),
  scaffold_holds(When).

% Is this game finished? Semidet — a world may satisfy two endings at once and
% that is still one finished game.
scaffold_over(S) :- scaffold_ending_reached(S, _, _), !.

% What the finished game was. Enumerates when more than one ending holds: which
% of them the host announces is a presentation decision, and the authored order
% is the answer core hands it rather than one it invents here.
scaffold_outcome(S, Outcome) :- scaffold_ending_reached(S, _, Outcome).
