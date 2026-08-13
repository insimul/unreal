
% ═══════════════════════════════════════════════════════════════════════════
% Equipment — slots, requirements, weight and armour
% docs/mechanic-predicates.md §8
% ═══════════════════════════════════════════════════════════════════════════

% ── Authored facts (world content — never in a save) ──────────────────────
% equip_slot(ItemId, Slot) / equip_slot_capacity(Slot, N) — "two rings, not five".
% item_requires(ItemId, SkillId, MinLevel) — "heavy plate isn't something you
%   just put on".
:- dynamic(equip_slot/2).
:- dynamic(equip_slot_capacity/2).
:- dynamic(item_requires/3).

% ── Runtime facts (per playthrough) ───────────────────────────────────────
:- dynamic(carry_capacity/2).
:- dynamic(item_condition/2).

% ── Facts owned elsewhere that these rules read ──────────────────────────
% has_equipped/3 and has_item/3 are runtime gameplay state; item_weight/2 and
% item_armor/2 are authored stats on the item block.
:- dynamic(has_equipped/3).
:- dynamic(has_item/3).
:- dynamic(has_skill/3).
:- dynamic(item_weight/2).
:- dynamic(item_armor/2).

% ── Rules ─────────────────────────────────────────────────────────────────

% The compat bridge for helper-predicates.ts's objective_complete/3 clauses —
% note the argument order differs from has_equipped/3, which is why this rule
% exists at all (§10.3).
equipped(Actor, Item, Slot) :- has_equipped(Actor, Slot, Item).

% Pack-local total. findall/3 and length/2 are ISO builtins the shared engine
% has; aggregate_all/3 is NOT (it lives in library(aggregate), which a pack may
% not depend on — see the module header), and the name is prefixed so this pack
% cannot shadow a builtin the way sum_list/2 once did.
equipment_sum([], 0).
equipment_sum([X | T], S) :-
  equipment_sum(T, S0),
  S is S0 + X.

% Room left in a slot the world gave a capacity to.
slot_free(A, Slot) :-
  equip_slot_capacity(Slot, N),
  findall(I, has_equipped(A, Slot, I), Equipped),
  length(Equipped, Used),
  Used < N.

% Held, has a slot, the slot has room, and every skill requirement is met. The
% comparison is inline rather than helper-predicates.ts's skill_gte/3 so this
% pack consults standalone (see the module header).
can_equip(A, I) :-
  has_item(A, I, _),
  equip_slot(I, Slot),
  slot_free(A, Slot),
  \+ (item_requires(I, S, Min), \+ (has_skill(A, S, L), L >= Min)).

% Everything carried, by weight. A rule and not a fact: a stored total goes
% stale against has_item/3 the moment anything is picked up (§12). An actor
% carrying nothing weighs 0 rather than failing, so a caller never has to
% distinguish "empty" from "unknown".
carried_weight(A, W) :-
  findall(Wi, (has_item(A, I, Q), item_weight(I, U), Wi is U * Q), Weights),
  equipment_sum(Weights, W).

encumbered(A) :-
  carried_weight(A, W),
  carry_capacity(A, Max),
  W > Max.

% Armour from everything worn, whatever slot it is in.
armor_value(A, V) :-
  findall(Av, (has_equipped(A, _, I), item_armor(I, Av)), Values),
  equipment_sum(Values, V).

% ═══════════════════════════════════════════════════════════════════════════
% Trade — who sells what, whether the shelf is bare, and who is paid
% docs/mechanic-predicates.md §8, US-2 of 124-items-equipment-economy
% ═══════════════════════════════════════════════════════════════════════════

% ── Authored facts (world content — never in a save) ──────────────────────
% vendor(VendorId, BusinessId) — who trades, and on whose behalf. A vendor
%   trading for themselves is said with their own atom in both places, so
%   vendor(V, B), business_owner(B, O) is one query rather than two shapes.
% vendor_markup(BusinessId, Percent) — the SHOP's margin: the business sets the
%   price and the clerk does not.
% item_stock_normal(ItemId, Qty) — what a well-supplied shelf carries. Scarcity
%   is meaningless without it: three left is a glut of warhorses and a famine of
%   arrows.
% item_loot_weight(ItemId, Weight) — how likely the thing is to turn up at all.
:- dynamic(vendor/2).
:- dynamic(vendor_markup/2).
:- dynamic(item_stock_normal/2).
:- dynamic(item_loot_weight/2).

% ── Facts owned elsewhere that these rules read ──────────────────────────
% gold/2 belongs to the gameplay-state block and predates this pack; it is
% declared here so this pack stands alone, exactly as has_item/3 above is, and
% gameplay-state-predicates.ts is what OWNS the signature (§10.1).
:- dynamic(gold/2).
% A merchant's shelf is a CONTAINER: container_contains/3 already says "this
% many of that, in that place", and a shelf is a place — so trade mints no
% stock predicate of its own. business_owner/2 is the business block's.
:- dynamic(container_contains/3).
:- dynamic(business_owner/2).
:- dynamic(item_tradeable/1).

% ── Rules ─────────────────────────────────────────────────────────────────

% What is on the shelf, with "none" as an answer rather than a failure — the
% same stance carried_weight/2 takes, so a caller never has to distinguish
% "empty" from "unknown".
stock_level(V, I, Q) :-
  container_contains(V, I, Q),
  !.
stock_level(_, _, 0).

in_stock(V, I, Q) :-
  vendor(V, _),
  stock_level(V, I, Q),
  Q > 0.

% Availability: they have one AND it is merchandise. A quest letter in a
% merchant's pack is not for sale.
sells(V, I) :-
  in_stock(V, I, _),
  item_tradeable(I).

% Fewer on the shelf than the world calls a normal stock. HOW MUCH that costs is
% arithmetic and stays out of the vocabulary (§11) — src/items/economy.ts — for
% the reason §12 gives: a stored price goes stale the moment a shelf moves.
scarce(V, I) :-
  vendor(V, _),
  item_stock_normal(I, N),
  stock_level(V, I, Q),
  Q < N.

% Who the money belongs to. A vendor with no separate owner is their own.
proceeds_to(V, Owner) :-
  vendor(V, B),
  business_owner(B, Owner),
  !.
proceeds_to(V, V) :-
  vendor(V, _).

% Whether a purse covers an amount. Named can_pay/2 rather than can_afford/2
% because the latter belongs to the ToTT economics vocabulary and a pack may not
% quietly redefine another's rule.
can_pay(A, Amount) :-
  gold(A, G),
  G >= Amount.

% ═══════════════════════════════════════════════════════════════════════════
% Placement — things in the world before anybody owns them
% docs/mechanic-predicates.md §8, US-3 of 124-items-equipment-economy
% ═══════════════════════════════════════════════════════════════════════════

% ── Runtime facts (per playthrough) ───────────────────────────────────────
% item_at(ItemId, LocationId, Qty) — lying in the world, owned by nobody. The
%   fourth place an item can be and the ONLY one that had no predicate: carried
%   is has_item/3, in a chest is container_contains/3, worn is has_equipped/3.
%   It carries a quantity because those two do and because the ledger holds one:
%   forty arrows on a barrow floor said as item_at(arrow, barrow) would be a KB
%   that means "some arrows" about a stack core knows the size of.
%   Runtime, not authored: where the world STARTS its things is the template's,
%   where they are on the fortieth day is the playthrough's (§3).
:- dynamic(item_at/3).

% ── Facts owned elsewhere that these rules read ──────────────────────────
% Containers already had a vocabulary — helper-predicates.ts declares five
% container predicates and reasons over them with container_accessible/2 — so
% placement mints none of its own and declares them the way it declares
% has_item/3 above. at_location/2 is the world-state predicate, not a
% placement-private one: a chest is an entity, and a rule asking what is in the
% crypt must see it (traversal-predicates.ts makes the same call for a boat).
:- dynamic(container/1).
:- dynamic(container_locked/1).
:- dynamic(at_location/2).

% ── Rules ─────────────────────────────────────────────────────────────────

% Something lying where this actor is standing — what a "take" is offered over.
% Whether they MAY take it is permissible/3's answer, asked by whoever takes it.
item_here(A, I) :-
  at_location(A, L),
  item_at(I, L, _).

% A container within reach. Whether it opens is container_accessible/2, which
% belongs to helper-predicates.ts and is not called here: a pack that calls
% another pack's rule is unusable without it (the AC4 coupling §8 refuses).
container_here(A, C) :-
  at_location(A, L),
  container(C),
  at_location(C, L).
