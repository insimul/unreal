
% ═══════════════════════════════════════════════════════════════════════════
% Traversal — links, blockage, and reachability
% docs/mechanic-predicates.md §7
% ═══════════════════════════════════════════════════════════════════════════

% ── Authored facts (world content — never in a save) ──────────────────────
% traversal_link(FromLoc, ToLoc, walk|climb|swim|ride|boat) — DIRECTED. Mode is
%   authored, and it is where mounts and boats land: the decision layer's whole
%   interest in a boat is "this link needs boat and you have it", so there is no
%   vehicle vocabulary (§12).
% traversal_cost(FromLoc, ToLoc, Cost) — read by authored rules (travel time,
%   stamina spend); the pack imposes no cost model of its own.
% traversal_requires(FromLoc, ToLoc, Goal) — Goal is a Prolog goal evaluated
%   with Actor bound, the same convention radiant_precondition/3 uses.
:- dynamic(traversal_link/3).
:- dynamic(traversal_cost/3).
:- dynamic(traversal_requires/3).

% ── Runtime facts (per playthrough) ───────────────────────────────────────
% "The landslide closed the pass" is a playthrough event, never world content.
:- dynamic(traversal_blocked/2).
:- dynamic(movement_mode/2).

% ── Rules ─────────────────────────────────────────────────────────────────

% Pack-local, library-free membership (see the module header).
traversal_member(X, [X|_]).
traversal_member(X, [_|T]) :- traversal_member(X, T).

% An authored requirement is written with the ACTOR as its first argument —
% traversal_requires(camp, ford, has_item(Actor, rope, 1)). That variable is a
% FRESH one every time the fact is read, so it is not the caller's actor and
% call/1 on the term as stored would be satisfied by anyone at all holding a
% rope. The goal is therefore rebuilt with A in the first argument position
% before it is called. A requirement authored as a plain atom (bridge_repaired)
% says nothing about the actor and is called as written.
traversal_goal_met(_, Goal) :-
  atom(Goal),
  !,
  call(Goal).
traversal_goal_met(A, Goal) :-
  Goal =.. [F, _ | Rest],
  Bound =.. [F, A | Rest],
  call(Bound).

% One link, open, in a mode this actor is using, whose requirement (if any)
% holds. The \+ (Requirement, \+ Met) shape means a link with no
% traversal_requires/3 fact passes; calling an authored goal is the trust
% boundary radiant_precondition/3 already accepts.
can_traverse(A, From, To) :-
  traversal_link(From, To, Mode),
  \+ traversal_blocked(From, To),
  movement_mode(A, Mode),
  \+ (traversal_requires(From, To, Goal), \+ traversal_goal_met(A, Goal)).

% Reachability over one or more links. Cycle-safe by carrying the visited set,
% so a loop of links terminates instead of spinning.
reachable(A, From, To) :- reachable_via(A, From, To, [From]).

reachable_via(A, From, To, _) :- can_traverse(A, From, To).
reachable_via(A, From, To, Seen) :-
  can_traverse(A, From, Mid),
  \+ traversal_member(Mid, Seen),
  reachable_via(A, Mid, To, [Mid | Seen]).

% ═══════════════════════════════════════════════════════════════════════════
% Vehicles — ridden entities, in the same vocabulary
% US-2 of 122-traversal-and-travel
% ═══════════════════════════════════════════════════════════════════════════

% ── Authored ──────────────────────────────────────────────────────────────
% vehicle_mode(VehicleId, ride|boat|...) — what the thing IS, said as the
%   traversal mode it lends whoever is driving it. §12 cut vehicle/1 and that cut
%   stands: a kind predicate would say strictly less than this one does.
:- dynamic(vehicle_mode/2).

% ── Runtime (per playthrough) ─────────────────────────────────────────────
% Ownership is runtime because a horse is bought, inherited, confiscated and
% stolen; the world's authored owner is where the playthrough STARTS (§3).
% at_location/2 is the world-state predicate, not a vehicle-private one: a
% vehicle is an entity, and a rule asking what is at the dock must see the boat.
:- dynamic(vehicle_owner/2).
:- dynamic(vehicle_occupant/2).
:- dynamic(vehicle_driver/2).
:- dynamic(at_location/2).

% ── Rules ─────────────────────────────────────────────────────────────────

% The mode a vehicle lends its driver — the ONLY thing a vehicle changes about
% traversal, and the reason there is no vehicle mechanic beside this one. A
% passenger is carried; the driver is the one movement_mode/2 becomes true of.
riding_mode(A, Mode) :- vehicle_driver(V, A), vehicle_mode(V, Mode).

% Boarding: same place, not already aboard. Colocation is asked of the location
% ATOMS — "three metres from the horse" is the host's question, not this one's.
% Capacity is deliberately absent: §12 cut vehicle_capacity/2, so a world that
% wants to know whether the cart is full counts vehicle_occupant/2 itself.
can_board(A, V) :-
  vehicle_mode(V, _),
  at_location(V, L),
  at_location(A, L),
  \+ vehicle_occupant(V, A).

% Taking the reins: aboard, and nobody else at the helm. Permission is NOT here
% — whether taking somebody else's cart is a crime is forbids/4's answer, which
% is what lets one world police the roads and another leave them common.
can_drive(A, V) :-
  vehicle_occupant(V, A),
  \+ (vehicle_driver(V, Other), Other \== A).

% Nobody has the reins — what an NPC's candidate_action/3 keys on to notice that
% the cart at the mill is there for the taking.
vehicle_unattended(V) :- vehicle_mode(V, _), \+ vehicle_driver(V, _).

% "It is not theirs" — the FACT a theft norm is authored over
% (rule_applies(cart_theft, Actor, V) :- vehicle_not_owned_by(Actor, V)). A
% vehicle nobody owns is nobody's to steal, so an absent owner FAILS rather than
% succeeds: core states ownership and never invents a prohibition.
vehicle_not_owned_by(A, V) :- vehicle_owner(V, Owner), Owner \== A.

% ═══════════════════════════════════════════════════════════════════════════
% Fast travel — you may go where you have BEEN and can still GET to
% US-3 of 122-traversal-and-travel
% ═══════════════════════════════════════════════════════════════════════════

% location_discovered/1 is gameplay-predicates.ts's, already asserted whenever a
% player reaches somewhere. It is DECLARED here rather than re-minted as a
% traversal-private "known destination" fact, for §10.1's reason: two vocabularies
% for one fact is how a place discovered through a rumour, a map or a quest stops
% counting as discovered for travel.
:- dynamic(location_discovered/1).

% §7's own worked example, promoted from an authored can_perform/2 clause to a
% pack rule because the decision layer asks it: fast travel is gated by DISCOVERY
% and by REACHABILITY, and reachability is what makes it a world operation rather
% than a teleport — you cannot fast travel past a landslide you could not have
% walked past, and a route that needs a rope needs the rope either way.
%
% Whether it is PERMITTED is forbids/4's, exactly as a climb's is, and is not here:
% a closed border, a quarantine and 123's skill gates refuse a journey without this
% rule knowing they exist. What is also not here is time — how long the journey
% takes is arithmetic over traversal_cost/3 and belongs in src/traversal/, never in
% a predicate a save could carry a stale copy of.
can_fast_travel(A, Dest) :-
  location_discovered(Dest),
  at_location(A, Here),
  Here \== Dest,
  reachable(A, Here, Dest).
