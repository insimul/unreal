
% ═══════════════════════════════════════════════════════════════════════════
% Stealth and perception — detection and trespass
% docs/mechanic-predicates.md §5
% ═══════════════════════════════════════════════════════════════════════════

% ── Authored facts (world content — never in a save) ──────────────────────
% senses(EntityId, sight|hearing|smell, Range) — what this creature can use.
% restricted_area(LocationId) / access_permitted(Actor, LocationId): "the
%   innkeeper's family may enter the back room" is a fact about the WORLD, not
%   about a playthrough, so both are authored. A key picked up mid-game asserts
%   nothing here — a trespass rule reads has/2 separately.
:- dynamic(senses/3).
:- dynamic(restricted_area/1).
:- dynamic(access_permitted/2).

% ── Runtime facts (per playthrough) ───────────────────────────────────────
% line_of_sight/2 is HOST-SUPPLIED: the engine raycasts and asserts it.
% light_level/2 is runtime and not authored on purpose (§3's test case): a cave's
%   darkness looks like template data, but the value that decides the query is
%   the one at query time — night, a doused torch, a spell.
% Level is 0–100 for both noise_level/2 and light_level/2.
:- dynamic(line_of_sight/2).
:- dynamic(concealed/1).
:- dynamic(noise_level/2).
:- dynamic(light_level/2).
:- dynamic(alerted/2).

% ── The graded detection state (per playthrough) ──────────────────────────
% awareness(Observer, Target, Level) — 0–100, accumulated over ticks by
%   game-engine/logic/DetectionTracker.ts out of what the host's senses reported.
% detection_state(Observer, Target, unaware|suspicious|searching|alerted).
:- dynamic(awareness/3).
:- dynamic(detection_state/3).

% ── Authored bands (world content — published at load, never in a save) ────
% detection_threshold(suspicious|searching|alerted, Level) — the world's
%   thresholds, out of WorldIR.perception, the way stamina_threshold/2 is (§9).
:- dynamic(detection_threshold/2).

% ── Facts owned elsewhere that these rules read ──────────────────────────
:- dynamic(near/3).
:- dynamic(at_location/2).

% ── Rules ─────────────────────────────────────────────────────────────────

% Detection, and the sense that produced it. detects_via/3 is the one that
% carries the reason; detects/2 is the question every authored rule asks.
detects(O, T) :- detects_via(O, T, _).

detects_via(O, T, sight) :-
  senses(O, sight, R),
  near(O, T, R),
  line_of_sight(O, T),
  \+ concealed(T),
  at_location(T, L),
  light_level(L, Lx),
  Lx >= 20.

detects_via(O, T, hearing) :-
  senses(O, hearing, R),
  near(O, T, R),
  noise_level(T, N),
  N >= 30.

% Nobody detects T. Written over detects/2 so a new sense clause tightens it
% automatically.
unseen(T) :- \+ detects(_, T).

% Somewhere you are not allowed to be. Being SEEN there is a separate question —
% the guard's rule conjoins trespassing/2 with detects/2 (§5).
trespassing(A, L) :-
  at_location(A, L),
  restricted_area(L),
  \+ access_permitted(A, L).

% ── The rungs of the ladder ───────────────────────────────────────────────
% Named, so an authored rule says "the guard is searching for the thief" rather
% than comparing a number to a band it would have to know the value of.
suspicious_of(O, T) :- detection_state(O, T, suspicious).

searching_for(O, T) :- detection_state(O, T, searching).

% alerted/2 stays dynamic — a scripted alarm or a quest script asserts it — AND
% is derived from the ladder, so a graded state and the boolean every existing
% rule already asks can never contradict each other.
alerted(O, T) :- detection_state(O, T, alerted).

% Noticed at all: any rung above unaware. Written over detection_state/3 so a
% world that adds a rung gets it here for free.
noticed(O, T) :-
  detection_state(O, T, S),
  S \== unaware.

% Nobody has noticed T. unseen/1 asks that of THIS instant's sensing;
% undetected/1 asks it of the accumulated state, which is the one a stealth
% mechanic actually pays out on.
undetected(T) :- \+ noticed(_, T).
