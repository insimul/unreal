
% ═══════════════════════════════════════════════════════════════════════════
% NPC routines — a standing goal, and the clock that decides when it is held
% packages/core/docs/npc-routines.md §3
%
% COMPOSITION PACK. Consult AFTER ai/planning-predicates.ts: this pack adds
% clauses for goal_requires/2 and condition_met/2, which that pack declares
% :- dynamic, and a dynamic directive arriving after clauses for the same
% predicate is a permission_error on a strict ISO engine.
% ═══════════════════════════════════════════════════════════════════════════

% ── Authored facts (world content — never in a save) ──────────────────────
% routine(RoutineId, Name) — a routine this world knows about.
% routine_block(RoutineId, BlockId) — the blocks it is made of.
% routine_block_window(BlockId, StartHour, EndHour) — whole hours. Start is
%   inclusive, End exclusive, and End =< Start WRAPS midnight, so the night
%   watch is one block rather than two.
% routine_block_day(BlockId, Weekday) — the days it runs, counted
%   day_number mod routine_week_length. NO fact at all means every day.
% routine_block_goal(BlockId, GoalId) — the goal/2 it puts the NPC in pursuit
%   of. The whole of what a routine decides.
% routine_block_place(BlockId, Location) — where that goal is pursued.
% routine_block_priority(BlockId, 0..100) — what it is worth against a quest,
%   an errand or a threat, on agent_goal/3's own scale.
% routine_week_length(N) — how many days the week has.
:- dynamic(routine/2).
:- dynamic(routine_block/2).
:- dynamic(routine_block_window/3).
:- dynamic(routine_block_day/2).
:- dynamic(routine_block_goal/2).
:- dynamic(routine_block_place/2).
:- dynamic(routine_block_priority/2).
:- dynamic(routine_week_length/1).

% ── Runtime facts (per playthrough) ───────────────────────────────────────
% follows_routine(Agent, RoutineId) — whose routine this is. Runtime, because
%   an NPC takes a job, loses one and retires, and which routine they keep at a
%   tick belongs in the save file.
% routine_suspended(Agent, Reason) — the routine is preempted: a threat, a
%   conversation, a rule violation. While it holds, the routine adopts nothing
%   and whatever displaced it is the only goal in play.
:- dynamic(follows_routine/2).
:- dynamic(routine_suspended/2).

% ── Adopted (declared so the pack loads alone; owned elsewhere) ───────────
% The clock is gameplay-predicates.ts's, at_location/2 is what traversal
% writes, and the two planning predicates are ai/planning-predicates.ts's.
:- dynamic(time_of_day/1).
:- dynamic(day_number/1).
:- dynamic(at_location/2).
:- dynamic(goal_requires/2).
:- dynamic(condition_met/2).

% ── The clock ─────────────────────────────────────────────────────────────

% Which day of the week it is. The week's length is AUTHORED rather than seven:
% a world with a ten-day week should not need three engines recompiled.
routine_weekday(W) :-
  day_number(D),
  routine_week_length(N),
  N > 0,
  W is D mod N.

% Is this hour inside that window? Semidet either way — a window that matched
% twice would make a due block a duplicate solution and a routine's priority
% depend on how many ways it happened to match.
%
% Start < End is the ordinary window. Otherwise it WRAPS, and a window whose
% ends coincide (22 to 22) is the whole day, which is how a routine with one
% block is authored.
routine_hour_in(H, Start, End) :- Start < End, !, H >= Start, H < End.
routine_hour_in(H, Start, End) :- ( H >= Start ; H < End ), !.

routine_window_open(B) :-
  routine_block_window(B, Start, End),
  time_of_day(H),
  routine_hour_in(H, Start, End).

% A block with no routine_block_day/2 fact runs every day. Semidet, for the
% same reason routine_hour_in/3 is.
routine_day_matches(B) :- \+ routine_block_day(B, _), !.
routine_day_matches(B) :- routine_weekday(W), routine_block_day(B, W), !.

% ── What is due ───────────────────────────────────────────────────────────

% A block of this agent's routine whose time condition holds right now.
%
% The agent must be BOUND, exactly as pursuing/3 and candidate_action/3 require:
% an unbound agent would enumerate every follows_routine/2 fact, which is the
% roster being decided by which facts happened to load.
routine_due(A, B) :-
  nonvar(A),
  follows_routine(A, R),
  routine_block(R, B),
  routine_window_open(B),
  routine_day_matches(B).

% The goal a routine wants held, and what it is worth. THIS IS THE WHOLE OF
% WHAT A ROUTINE DECIDES: a goal id and a number, which RoutineDirector writes
% as one agent_goal/3 fact. Everything after that — which goal wins, which
% actions serve it, whether they are permitted — is the substrate's, unchanged.
routine_goal(A, G, P) :-
  routine_due(A, B),
  \+ routine_suspended(A, _),
  routine_block_goal(B, G),
  routine_block_priority(B, P).

% ── Place ─────────────────────────────────────────────────────────────────

% A block's place is a REQUIREMENT of its goal, stated in the planner's own
% condition vocabulary — so "get to the forge" is planned by the planner, out of
% the actions the world already authored, rather than by a routine that walks
% people around.
%
% The condition atom IS the location atom: an action the world authored
% action_achieves(walk_to_forge, forge) for is what the planner commits to.
% Because the requirement hangs on the GOAL, two blocks pursuing one goal in two
% places are an authoring error (routineIssues says so) — that is two goals.
goal_requires(G, Place) :-
  routine_block_goal(B, G),
  routine_block_place(B, Place).

% ...and it is met exactly when the actor is there. at_location/2 is the
% predicate traversal already writes; nothing here moves anybody.
condition_met(A, Place) :-
  routine_block_place(_, Place),
  at_location(A, Place).

% Where the routine wants this agent to be, and whether they are there. US-2
% turns the gap between the two into movement intent; this pack states it and
% executes nothing.
routine_destination(A, Place) :-
  routine_due(A, B),
  routine_block_place(B, Place).

routine_in_place(A) :-
  routine_destination(A, Place),
  at_location(A, Place).
