
% ═══════════════════════════════════════════════════════════════════════════
% Skills — what exists, what it costs, what it requires
% docs/mechanic-predicates.md §6
% ═══════════════════════════════════════════════════════════════════════════

% ── Authored facts (world content — never in a save) ──────────────────────
% skill_defined(SkillId, Category).
% skill_max_level(SkillId, Max).
% skill_requires(SkillId, PrereqSkillId, MinLevel).
% skill_level_xp(SkillId, Level, XpNeeded) — the progression CURVE, kept in
%   content. Without it the curve is a constant in four engines and the same
%   save levels up in three of them.
:- dynamic(skill_defined/2).
:- dynamic(skill_max_level/2).
:- dynamic(skill_requires/3).
:- dynamic(skill_level_xp/3).

% ── Runtime facts (per playthrough) ───────────────────────────────────────
:- dynamic(skill_xp/3).

% ── Facts owned elsewhere that these rules read ──────────────────────────
:- dynamic(has_skill/3).

% ── Rules ─────────────────────────────────────────────────────────────────

% At the world's cap for this skill.
skill_capped(A, S) :-
  has_skill(A, S, L),
  skill_max_level(S, L).

% A skill the world declares, that this actor does not have yet, and whose every
% prerequisite is met. The double negation is "no prerequisite is unmet", so a
% skill with no skill_requires/3 facts qualifies.
can_learn_skill(A, S) :-
  skill_defined(S, _),
  \+ has_skill(A, S, _),
  \+ (skill_requires(S, P, Min), \+ (has_skill(A, P, L), L >= Min)).

% Enough XP banked for the next level, and not already capped.
can_advance(A, S) :-
  has_skill(A, S, L),
  \+ skill_capped(A, S),
  Next is L + 1,
  skill_level_xp(S, Next, Need),
  skill_xp(A, S, X),
  X >= Need.

% ═══════════════════════════════════════════════════════════════════════════
% Skill trees — nodes, what they ask, what they cost, what they do
% US-1 of 123-skill-trees-and-progression
% ═══════════════════════════════════════════════════════════════════════════

% ── Authored facts (world content — never in a save) ──────────────────────
% skill_tree(TreeId, SkillId) — the skill whose levels FUND this tree. It is
%   the whole coupling between the two halves of progression: advancing
%   smithing grants points to the trees rooted on smithing, and nothing else
%   does. A guild or faction tree is authored the same way over a standing.
% skill_node(NodeId, TreeId).
% skill_node_cost(NodeId, Points).
% skill_node_requires(NodeId, Goal) — Goal is a Prolog goal evaluated with the
%   actor bound, the convention traversal_requires/3 uses. An authored parent
%   edge arrives here as skill_node_requires(N, skill_unlocked(Actor, Parent)),
%   so tree structure and an arbitrary condition are ONE gate.
% skill_node_effect(NodeId, Effect) — what taking it does, as a term:
%   unlocks(Action), modifies(Param, Amount), permits(Thing). The set is
%   authored; a world that wants a fourth writes it and a rule reads it.
:- dynamic(skill_tree/2).
:- dynamic(skill_node/2).
:- dynamic(skill_node_cost/2).
:- dynamic(skill_node_requires/2).
:- dynamic(skill_node_effect/2).

% ── Runtime facts (per playthrough) ───────────────────────────────────────
% What is unspent, per tree, and what has been taken. Both are per-playthrough:
% two saves of one world differ by exactly this (§3).
:- dynamic(skill_points/3).
:- dynamic(skill_unlocked/2).

% ── Rules ─────────────────────────────────────────────────────────────────

% An authored requirement is written with the ACTOR as its first argument —
% skill_node_requires(guild_forge, reputation(Actor, smiths_guild, 50)). That
% variable is a FRESH one every time the fact is read, so it is not the caller's
% actor and call/1 on the term as stored would be satisfied by anybody at all
% with the standing — one guild member's reputation unlocking the whole roster's
% node. The goal is therefore rebuilt with A in the first argument position
% before it is called. A requirement authored as a plain atom (bridge_repaired)
% says nothing about the actor and is called as written.
skill_goal_met(_, Goal) :-
  atom(Goal),
  !,
  call(Goal).
skill_goal_met(A, Goal) :-
  Goal =.. [F, _ | Rest],
  Bound =.. [F, A | Rest],
  call(Bound).

% What a node costs. A loaded world publishes a cost for every node (the
% world's default applied), so the second clause is what keeps the pack
% answerable about a tree asserted by hand.
skill_node_price(N, C) :- skill_node_cost(N, C), !.
skill_node_price(N, 0) :- skill_node(N, _).

% What is unspent in a tree the actor has never earned a point in.
skill_points_held(A, T, P) :- skill_points(A, T, P), !.
skill_points_held(_, T, 0) :- skill_tree(T, _).

% A node this actor has not taken, can pay for, and whose every requirement
% holds. The double negation is "no requirement is unmet", so a node with no
% skill_node_requires/2 fact qualifies. Whether it is PERMITTED is forbids/4's,
% exactly as a climb's is: a guild's own rules refuse an unlock without this
% rule knowing that guilds exist.
can_unlock(A, N) :-
  skill_node(N, T),
  \+ skill_unlocked(A, N),
  skill_node_price(N, C),
  skill_points_held(A, T, P),
  P >= C,
  \+ (skill_node_requires(N, G), \+ skill_goal_met(A, G)).

% What an actor's skills DO — the one thing another module asks. A module reads
% effects; it never reads nodes, which is what keeps a skill tree data over
% combat, traversal and crafting rather than a system inside them.
skill_effect(A, E) :- skill_unlocked(A, N), skill_node_effect(N, E).
