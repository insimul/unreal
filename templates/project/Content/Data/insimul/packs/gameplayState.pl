
% ═══════════════════════════════════════════════════════════════════════════
% Runtime gameplay state — declarations only, no rules
% docs/mechanic-predicates.md §10.1
% ═══════════════════════════════════════════════════════════════════════════
%
% Declared so a KB that carries none of them FAILS a query rather than raising
% existence_error, and so a fact restored from a save validates instead of being
% dropped. Per-playthrough state: these belong in save.currentState.prologFacts
% and never on a world template (§3).

:- dynamic(health/3).            % health(Actor, Current, Max).
:- dynamic(energy/3).            % energy(Actor, Current, Max).
:- dynamic(has_equipped/3).      % has_equipped(Actor, Slot, ItemId).
:- dynamic(has_status/3).        % has_status(Actor, Status, Duration).
:- dynamic(has_ability/2).       % has_ability(Actor, Ability).
:- dynamic(near/3).              % near(Actor, Target, Distance).
:- dynamic(at_location_type/2).  % at_location_type(Actor, LocationType).
:- dynamic(level/2).             % level(Actor, Level).
:- dynamic(xp/3).                % xp(Actor, Current, Max).
:- dynamic(gold/2).              % gold(Actor, Amount).
