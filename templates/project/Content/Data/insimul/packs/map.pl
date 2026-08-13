
% ═══════════════════════════════════════════════════════════════════════════
% Regions and jurisdiction — where am I, and whose is it
% packages/core/docs/geopolitical-layer.md §3
%
% COMPOSITION PACK. Consult AFTER ai/action-predicates.ts (which declares
% :- dynamic(forbids/4)) and ai/enforcement-predicates.ts (whose norm/2 the
% jurisdiction clause below calls).
%
% Every coordinate is an INTEGER on the geo grid (§4): world units multiplied
% by GEO_GRID_SCALE and rounded once, at mint time, by geoUnit() in
% geo/regions.ts. Nothing here rounds, and nothing here compares floats.
% ═══════════════════════════════════════════════════════════════════════════

% ── Extents (AUTHORED — projected from the World IR by geo/regions.ts) ────
% region(Id) — this world knows where Id is. Id is the DOCUMENT's atom: a
%   country, a state, a settlement, a terrain feature, a water body. There is no
%   region id space, which is why a region needs no name, no world and no
%   identifier of its own.
% region_kind(Id, country|state|settlement|terrain|water) — which axis the
%   extent belongs to. political_kind/1 and geographic_kind/1 below classify
%   them; core enumerates these five because they are the five the World IR
%   exports geometry for, not because a world may not have districts.
% region_bounds(Id, MinX, MinZ, MaxX, MaxZ) — an axis-aligned extent, inclusive
%   on all four edges. The World IR's BoundsIR.
% region_disc(Id, CenterX, CenterZ, Radius) — a settlement's footprint, which is
%   what SettlementIR actually carries (a position and a radius).
% region_biome(Id, Biome) — the biome of the land this extent covers.
% region_watershed(Id, WaterId) — the water body this extent drains to. A
%   watershed is a RELATION to a water region, not a third geometry: the water
%   body already has an extent of its own.
% region_feature(Id, FeatureType) — a terrain feature's own type (mountain,
%   ridge, canyon). Read by region_terrain/2 beside the terrain the state and
%   settlement blocks already carry.
:- dynamic(region/1).
:- dynamic(region_kind/2).
:- dynamic(region_bounds/5).
:- dynamic(region_disc/4).
:- dynamic(region_biome/2).
:- dynamic(region_watershed/2).
:- dynamic(region_feature/2).

% place_position(Place, X, Z) — where a location atom is on the map. The bridge
%   between at_location/2 (which carries a place, not a coordinate) and every
%   rule below (which takes a point). AUTHORED: a lot, a landmark and a camp all
%   have positions in the World IR.
:- dynamic(place_position/3).

% jurisdiction_law(RuleId, JurisdictionId) — AUTHORED. The rule is that
%   jurisdiction's law and binds an act committed inside it. The territorial
%   twin of faction_law/2: one binds by WHO you are, this one by WHERE you
%   stand, and a world needs both to say that smuggling is legal across the
%   border and heresy is not, whoever you are.
:- dynamic(jurisdiction_law/2).

% ── Adopted (declared so the pack loads alone; owned elsewhere) ───────────
% The political blocks are prolog/predicate-schema.ts's country/state/settlement
% entries, unchanged and never re-minted. at_location/2 is the world-state
% predicate traversal writes. entity_id/2 and entity_curie/2 are the KINP bridge
% identity-facts.ts emits. The three rule-block predicates belong to
% prolog/rule-converter.ts and ai/enforcement-predicates.ts.
:- dynamic(country/1).
:- dynamic(state/1).
:- dynamic(settlement/1).
:- dynamic(state_of_country/2).
:- dynamic(settlement_of_state/2).
:- dynamic(settlement_of_country/2).
:- dynamic(state_terrain/2).
:- dynamic(settlement_terrain/2).
:- dynamic(lot_of_settlement/2).
:- dynamic(residence_of_settlement/2).
:- dynamic(business_of_settlement/2).
:- dynamic(at_location/2).
:- dynamic(entity_id/2).
:- dynamic(entity_curie/2).
:- dynamic(rule_forbids_action/2).
:- dynamic(rule_applies/3).
:- dynamic(scoped_norm/1).

% ── The two axes ──────────────────────────────────────────────────────────
% Which extents answer "whose is it" and which answer "what is it". A terrain
% feature is never a jurisdiction and a country is never a biome, and keeping
% the two lists here is what lets one containment test serve both questions.

political_kind(country).
political_kind(state).
political_kind(settlement).

geographic_kind(terrain).
geographic_kind(water).

% ── Containment ───────────────────────────────────────────────────────────
% Integer comparison, on the grid the facts were minted on. A point on a border
% is INSIDE — inclusive on all four edges — so a coastline shared by two states
% resolves to both and jurisdiction_conflict/3 can say so, rather than to
% neither.

% A region's extent as ONE term, so the containment arithmetic can be stated
% once and asked twice. geo/history-predicates.ts resolves an extent through the
% world chain instead of reading these facts, and then calls the very same
% region_extent_contains/3 and region_extent_area/2 — a border that moved and a
% border that never did are compared by identical arithmetic, which is the whole
% reason this indirection exists.
region_extent(R, rect(MinX, MinZ, MaxX, MaxZ)) :- region_bounds(R, MinX, MinZ, MaxX, MaxZ).
region_extent(R, disc(Cx, Cz, Rad)) :- region_disc(R, Cx, Cz, Rad).

region_extent_contains(rect(MinX, MinZ, MaxX, MaxZ), X, Z) :-
  X >= MinX,
  X =< MaxX,
  Z >= MinZ,
  Z =< MaxZ.

region_extent_contains(disc(Cx, Cz, Rad), X, Z) :-
  DX is X - Cx,
  DZ is Z - Cz,
  DX * DX + DZ * DZ =< Rad * Rad.

% The area of an extent's AXIS-ALIGNED BOUND, so a rectangle and a disc are
% ordered in one commensurate unit. A disc's bound is its enclosing square,
% which is why a settlement inside a state always ranks smaller even when its
% radius is generous.
region_extent_area(rect(MinX, MinZ, MaxX, MaxZ), S) :- S is (MaxX - MinX) * (MaxZ - MinZ).
region_extent_area(disc(_, _, Rad), S) :- S is 4 * Rad * Rad.

in_region(X, Z, R) :-
  region_extent(R, E),
  region_extent_contains(E, X, Z).

region_size(R, S) :-
  region_extent(R, E),
  region_extent_area(E, S).

% A TOTAL order over extents: area first, atom order to break ties. This is the
% whole of AC4 — "the innermost region" is never "the first solution the engine
% enumerated", so two KBs holding the same facts in different orders answer the
% same thing. A region is never smaller than itself under either clause.
smaller_region(A, B) :-
  region_size(A, SA),
  region_size(B, SB),
  SA < SB.

smaller_region(A, B) :-
  region_size(A, SA),
  region_size(B, SB),
  SA =:= SB,
  A @< B.

% The innermost region of a KIND containing a point: the one no containing
% region of that kind is smaller than.
region_at(X, Z, Kind, R) :-
  in_region(X, Z, R),
  region_kind(R, Kind),
  \+ ( in_region(X, Z, Other),
        region_kind(Other, Kind),
        smaller_region(Other, R) ).

% ── What the land is ──────────────────────────────────────────────────────

% A region's terrain is the terrain the DOCUMENT already carries. The state and
% settlement blocks have had state_terrain/2 and settlement_terrain/2 since the
% first world was exported; a region_terrain/2 fact beside them would be the
% second vocabulary for one fact that docs/mechanic-predicates.md §10.1 exists
% to prevent. Only a terrain FEATURE — a thing with no document field for it —
% contributes a fact of its own.
region_terrain(R, T) :- settlement_terrain(R, T).
region_terrain(R, T) :- state_terrain(R, T).
region_terrain(R, T) :- region_feature(R, T).

% The three descriptors, each read off the INNERMOST containing region that
% carries one. A position inside a settlement inside a state answers with the
% settlement's terrain, and falls through to the state's when the settlement
% names none — which is what makes a sparse world answer at all.
position_terrain(X, Z, T) :-
  in_region(X, Z, R),
  region_terrain(R, T),
  \+ ( in_region(X, Z, Other),
        region_terrain(Other, _),
        smaller_region(Other, R) ).

position_biome(X, Z, B) :-
  in_region(X, Z, R),
  region_biome(R, B),
  \+ ( in_region(X, Z, Other),
        region_biome(Other, _),
        smaller_region(Other, R) ).

position_watershed(X, Z, W) :-
  in_region(X, Z, R),
  region_watershed(R, W),
  \+ ( in_region(X, Z, Other),
        region_watershed(Other, _),
        smaller_region(Other, R) ).

% ── Whose it is ───────────────────────────────────────────────────────────

jurisdiction_level(J, settlement) :- settlement(J).
jurisdiction_level(J, state) :- state(J).
jurisdiction_level(J, country) :- country(J).

% ONE step up the document chain: the superior the world's own blocks name. A
% settlement names its state and, when content authored both links, its country
% directly as well — normal authoring, and neither link is preferred.
jurisdiction_parent(S, St) :- settlement_of_state(S, St).
jurisdiction_parent(S, C) :- settlement_of_country(S, C).
jurisdiction_parent(St, C) :- state_of_country(St, C).

% The document chain above a jurisdiction: one step, or two. NO recursion — so it
% is acyclic by construction rather than by a depth guard, and a malformed world
% cannot make it loop. A settlement reaches its country both directly and through
% its state when content authored both; the duplicate solution is folded by the
% caller, which is cheaper than teaching the KB to prefer one path.
%
% geo/history-predicates.ts re-states these two clauses over a WORLD-resolved
% parent instead of jurisdiction_parent/2, which is the only part of the chain a
% regime can move — the shape of the chain is the same at every time.
jurisdiction_above(J, Above) :- jurisdiction_parent(J, Above).
jurisdiction_above(S, C) :- jurisdiction_parent(S, St), jurisdiction_parent(St, C).

% THE ONE POLITICAL QUESTION GEOMETRY IS ASKED: the smallest political extent
% containing this point, across all three levels at once. Inside a settlement
% that is the settlement; out in the province it is the state; in the marches it
% is the country.
innermost_jurisdiction(X, Z, J) :-
  political_kind(Kind),
  region_at(X, Z, Kind, J),
  \+ ( political_kind(OtherKind),
        region_at(X, Z, OtherKind, Other),
        smaller_region(Other, J) ).

% ...and everything above it is the DOCUMENTS' answer, never a second reading of
% the map. This is the predicate the rest of the ecosystem asks: the rule
% enforcer, fast travel's discovery, a quest that wants a border crossed.
position_jurisdiction(X, Z, Level, J) :-
  innermost_jurisdiction(X, Z, J),
  jurisdiction_level(J, Level).

position_jurisdiction(X, Z, Level, J) :-
  innermost_jurisdiction(X, Z, Inner),
  jurisdiction_above(Inner, J),
  jurisdiction_level(J, Level).

% Geometry and the documents disagree about this point: a political region
% contains it that the innermost jurisdiction's own chain never reaches. A
% content defect — a settlement placed across a border, a state whose bounds
% outgrew its country — NAMED rather than silently resolved, because a silent
% resolution is how a world ships with two capitals.
jurisdiction_conflict(X, Z, J) :-
  political_kind(Kind),
  region_at(X, Z, Kind, J),
  \+ position_jurisdiction(X, Z, _, J).

% ── Places, and the actors standing in them ───────────────────────────────
% at_location/2 carries a PLACE, not a coordinate, so a place reaches the map
% three ways: it has a position, it IS a jurisdiction, or the lot/residence/
% business blocks already say which settlement it belongs to. The third is the
% one that costs nothing — those facts have been exported since the beginning.

place_jurisdiction(P, Level, J) :-
  place_position(P, X, Z),
  position_jurisdiction(X, Z, Level, J).

place_jurisdiction(P, Level, J) :-
  jurisdiction_of(P, Level, J).

place_jurisdiction(P, Level, J) :-
  place_of_settlement(P, S),
  jurisdiction_of(S, Level, J).

% A jurisdiction, and everything the documents put above it. The third clause of
% place_jurisdiction/3 calls THIS rather than calling itself, so nothing in this
% pack is recursive: a world that authored lot_of_settlement/2 in a cycle gets a
% wrong answer, which is a content defect, instead of an engine that never
% returns, which is a crash in whatever frame asked.
jurisdiction_of(J, Level, J) :-
  jurisdiction_level(J, Level).

jurisdiction_of(J, Level, Above) :-
  jurisdiction_level(J, _),
  jurisdiction_above(J, Above),
  jurisdiction_level(Above, Level).

place_of_settlement(P, S) :- lot_of_settlement(P, S).
place_of_settlement(P, S) :- residence_of_settlement(P, S).
place_of_settlement(P, S) :- business_of_settlement(P, S).

% Every jurisdiction an actor is currently under. The agent must be BOUND, for
% the same reason candidate_action/3 requires it: an unbound one would enumerate
% every at_location/2 fact in the world.
agent_jurisdiction(A, Level, J) :-
  nonvar(A),
  at_location(A, P),
  place_jurisdiction(P, Level, J).

in_jurisdiction(A, J) :-
  agent_jurisdiction(A, _, J).

% ── The gate's input ──────────────────────────────────────────────────────
% One more clause for forbids/4, and no change to anything that reads it.
% permissible/3, forbidden_by/4, checkAction() and enforceActs() are untouched:
% permissibility differs by territory because content authored a jurisdiction's
% law, not because the gate learned about borders.
%
% A territorial law is SCOPED, so the enforcement pack's universal clause must
% not also match it — a law of Aldermark that bound every actor everywhere would
% be a more, and the whole point is that it is not. scoped_norm/1 is that pack's
% extension seam and this is the clause that registers the second scope; without
% it, every jurisdiction_law/2 in the world would silently become universal.
scoped_norm(Rule) :- jurisdiction_law(Rule, _).

% The law binds by where the ACTOR stands. Not the target's jurisdiction and not
% the act's: a smuggler in free waters is beyond a customs law even when the
% cargo's owner is not, and "where the act happened" is where its actor was.
forbids(Rule, A, Act, T) :-
  norm(Rule, _),
  jurisdiction_law(Rule, J),
  in_jurisdiction(A, J),
  rule_forbids_action(Rule, Act),
  rule_applies(Rule, A, T).

% ── Identity ──────────────────────────────────────────────────────────────
% A region is referable across worlds because the atom it is named by is an
% ENTITY, and entity_id/2 / entity_curie/2 already name every entity with its
% KINP identifier. These two rules exist so a caller asking "what is this region
% called elsewhere" has a predicate to ask rather than a convention to follow —
% and so nothing downstream is tempted to mint a region id space.
region_id(R, Id) :- region(R), entity_id(R, Id).
region_curie(R, Curie) :- region(R), entity_curie(R, Curie).
