
% ═══════════════════════════════════════════════════════════════════════════
% Game-AI substrate — perception provenance
% packages/core/docs/game-ai-substrate.md §2
% ═══════════════════════════════════════════════════════════════════════════

% ── Runtime facts (per playthrough) ───────────────────────────────────────
% agent_belief_world(Agent, World) — the world an agent's beliefs are claimed
%   at. Emitted by beliefWorldFacts/2; one per agent that can believe anything.
% percept(PerceptId, Subject, Predicate, Object) — an observable published this
%   tick: the SPO triple an observer would learn from perceiving it.
% percept_channel(PerceptId, sight|hearing|smell|touch) — the sense it presents
%   on. percept_actor(PerceptId, Actor) — who is responsible, when the percept
%   is an act; that is the hook a witnessing rule hangs on.
% perceived(Agent, PerceptId, Channel, clear|partial) — core's decision that
%   this agent perceived it, and how well. Written by perceive/1, never by hand.
:- dynamic(agent_belief_world/2).
:- dynamic(percept/4).
:- dynamic(percept_channel/2).
:- dynamic(percept_actor/2).
:- dynamic(perceived/4).

% ── Rules ─────────────────────────────────────────────────────────────────

% "Did this agent perceive it at all?" — the question most rules ask, written
% over perceived/4 so a new fidelity band tightens nothing.
perceived(A, E) :- perceived(A, E, _, _).

perceived_clearly(A, E) :- perceived(A, E, _, clear).

% Through which sense. Distinct from the fidelity so "the guard HEARD something"
% and "the guard saw it clearly" are different answers.
perceived_via(A, E, Channel) :- perceived(A, E, Channel, _).

% A witness is someone OTHER than the actor who perceived the act. The actor
% perceiving their own act is not a witness to it — otherwise every crime is
% self-reported and nothing is ever secret.
witness(A, E) :-
  perceived(A, E, _, _),
  percept_actor(E, Actor),
  A \== Actor.

% Nobody saw it. This is the predicate that makes stealth mean something: an
% enforcement rule that conjoins a violation with witness/2 has no consequence
% to produce when this holds.
unwitnessed(E) :- percept_actor(E, _), \+ witness(_, E).

% The triple an agent would have learned, restricted to what it actually
% perceived. The BELIEF it produces is claimed at the agent's belief world —
% see the bridge pack — this is only the perceived content.
perceived_content(A, S, P, O) :-
  perceived(A, E, _, _),
  percept(E, S, P, O).


% ═══════════════════════════════════════════════════════════════════════════
% Game-AI substrate — belief as a world (composition pack)
% packages/core/docs/game-ai-substrate.md §1
% Requires: identity/world-predicates.ts (holds/4) + ai/perception-predicates.ts
% ═══════════════════════════════════════════════════════════════════════════

% What the agent holds true, reasoning in its own world. A belief world is a
% ROOT world, so this returns what the agent has been told or has perceived —
% never the world's truths by default.
believes(A, S, P, O) :-
  agent_belief_world(A, W),
  holds(S, P, O, '@world'(W)).

% The agent has no belief about (S, P) at all. Ignorance, as distinct from
% believing something false. A must be bound.
unaware_of(A, S, P) :-
  agent_belief_world(A, _),
  \+ believes(A, S, P, _).

% The agent believes something the world does not: the predicate stealth,
% deception and every "he thinks the ledger is still in the drawer" plot depend
% on. Truth is read at the world the caller names, so "mistaken about canon" and
% "mistaken about this playthrough" are different questions.
mistaken(A, TruthWorld, S, P, Believed, Actual) :-
  believes(A, S, P, Believed),
  holds(S, P, Actual, '@world'(TruthWorld)),
  Believed \== Actual.

% True in the world, and this agent does not know it.
ignorant_of(A, TruthWorld, S, P, O) :-
  holds(S, P, O, '@world'(TruthWorld)),
  agent_belief_world(A, _),
  \+ believes(A, S, P, O).


% ═══════════════════════════════════════════════════════════════════════════
% Game-AI substrate — candidate actions and the legality gate
% packages/core/docs/game-ai-substrate.md §4
% ═══════════════════════════════════════════════════════════════════════════

% ── The action block (AUTHORED — prolog/action-converter.ts) ──────────────
% action(ActionId, Name, Type, EnergyCost) — the action itself.
% action_requires_target(ActionId) — it must be aimed at something.
% action_target_type(ActionId, Kind) — at something of this kind.
% action_prerequisite(ActionId, Goal) — the feasibility goals the converter
%   folds into can_perform/2 (targetless) or can_perform/3 (targeted). This pack
%   never re-evaluates a stored Goal term: its variables are fresh on retrieval
%   and there is no portable way to bind Actor/Target by name, which is exactly
%   why the converter generates a RULE whose head does the binding.
% action_appeal(ActionId, Motive, 0..100) — how strongly the action serves a
%   motive. Authored: "brawling serves anger, not hunger" is world design.
% forbids(RuleId, Agent, ActionId, Target) — the legality gate's ONLY input.
%   Content defines facts or rules for it; US-3's IRuleEnforcer composes social
%   mores, faction law and taboo into it. A creator changes what an NPC may do
%   by changing content, never by changing core.
:- dynamic(action/4).
:- dynamic(action_requires_target/1).
:- dynamic(action_target_type/2).
:- dynamic(action_prerequisite/2).
:- dynamic(can_perform/2).
:- dynamic(can_perform/3).
:- dynamic(action_appeal/3).
:- dynamic(forbids/4).

% ── Runtime facts (per playthrough) ───────────────────────────────────────
% target_kind(Target, Kind) — what exists to aim at right now, and what it is.
%   Runtime, not authored: a spawned NPC and a dropped sword are a playthrough's,
%   not a template's.
% agent_motive(Agent, Motive, 0..100) — how strongly the agent wants that thing
%   at this tick. Drives move; that is the point of them.
% target_appeal(Agent, Target, -100..100) — how much more, or less, this agent
%   wants THAT target: a grudge, a debt, a bond, a mark it has been watching.
%   Without it every target of an action scores alike and the layer cannot tell
%   "hit the guard" from "hit the priest", which is most of what a decision is.
% action_bias(Agent, ActionId, -100..100) — a nudge from outside the KB: the
%   blackboard's commitment to last tick's plan, or a local SLM's proposal. An
%   ADVISOR's slot — it moves a score and can never make a forbidden action
%   permissible, because it is not read by the gate below at all.
:- dynamic(target_kind/2).
:- dynamic(agent_motive/3).
:- dynamic(target_appeal/3).
:- dynamic(action_bias/3).

% ── Candidate enumeration ─────────────────────────────────────────────────

% What this agent could do: every action, aimed at every target it admits, that
% the agent is capable of right now. The roster is the CALLER's — core hands the
% agent in — so A is required to be bound; enumerating agents from the KB here
% would make the answer depend on what the KB happens to hold.
candidate_action(A, Act, T) :-
  nonvar(A),
  action(Act, _, _, _),
  action_target_choice(Act, T),
  feasible(A, Act, T).

% A targetless action's target is the atom `none`, not an unbound variable: a
% caller enumerating solutions must never receive a free variable, and `none`
% is a value four engines spell the same way.
action_target_choice(Act, T) :-
  action_requires_target(Act),
  action_target_type(Act, Kind),
  target_kind(T, Kind).
action_target_choice(Act, none) :-
  \+ action_requires_target(Act).

% CAPABILITY, which is not permission. An action that declares no prerequisite
% is unconstrained; one that does is exactly as feasible as the action block's
% own can_perform rule says.
feasible(_, Act, _) :-
  \+ action_prerequisite(Act, _).
feasible(A, Act, T) :-
  action_prerequisite(Act, _),
  prerequisites_hold(A, Act, T).

% The three clauses are mutually exclusive, so a candidate is never enumerated
% twice: targetless goes to can_perform/2, targeted to can_perform/3, and a
% targeted action whose content only wrote the targetless rule falls back to it.
prerequisites_hold(A, Act, none) :-
  can_perform(A, Act).
prerequisites_hold(A, Act, T) :-
  T \== none,
  can_perform(A, Act, T).
prerequisites_hold(A, Act, T) :-
  T \== none,
  \+ can_perform(A, Act, T),
  can_perform(A, Act).

% ── The legality gate ─────────────────────────────────────────────────────

% May the agent do it? A forbidden action is not a penalised one — it is not a
% choice. The utility layer scores it and then cannot pick it.
permissible(A, Act, T) :-
  candidate_action(A, Act, T),
  \+ forbidden(A, Act, T).

forbidden(A, Act, T) :-
  forbids(_, A, Act, T).

% Which rule forbade it — the reason the decision carries, so a refusal is
% legible ("faction law", "hearth taboo") rather than a silent absence.
forbidden_by(A, Act, T, Rule) :-
  forbids(Rule, A, Act, T).

% ── Scoring inputs, projected as scalars ──────────────────────────────────
% A query binding collapses a compound term to its functor (see
% prolog/wasm-engine.ts), so every value the selection layer reads is projected
% through a rule that yields an atom or a number. Never bind to action/4 itself.

action_cost(Act, Cost) :-
  action(Act, _, _, Cost).

% One row of the utility sum: this agent's drive for a motive, and how far this
% action serves it. Joined here rather than in TypeScript so the join is the
% KB's answer, and so a native engine reads one predicate instead of two.
motive_appeal(A, Act, Motive, Drive, Appeal) :-
  agent_motive(A, Motive, Drive),
  action_appeal(Act, Motive, Appeal).


% ═══════════════════════════════════════════════════════════════════════════
% Game-AI substrate — rule enforcement (COMPOSITION pack)
% packages/core/docs/game-ai-substrate.md §6
% Requires, in this order: identity/identity-predicates.ts (id_local/2),
%   ai/perception-predicates.ts (witness/2), ai/action-predicates.ts (the
%   :- dynamic(forbids/4) declaration this pack writes clauses for).
% ═══════════════════════════════════════════════════════════════════════════

% ── The rule block (AUTHORED — prolog/rule-converter.ts) ──────────────────
% rule_active(RuleId) — the rule is in force. A disabled more is not a deleted
%   one: content keeps it and stops declaring it active.
% rule_category(RuleId, Category) — social, faction, legal, taboo, … The atom
%   is the WORLD's; core enumerates no list of moral categories, for the same
%   reason it enumerates no list of damage types.
% rule_applies(RuleId, Actor, Target) — the rule block's own conditions, as the
%   converter generates them. An unconditional more is authored as the fact
%   rule_applies(RuleId, _, _), which is how "murder is always wrong" is spelled.
:- dynamic(rule_active/1).
:- dynamic(rule_category/2).
:- dynamic(rule_applies/3).

% ── The norm vocabulary this pack adds (AUTHORED) ─────────────────────────
% rule_forbids_action(RuleId, ActionId) — the link the rule block has no place
%   for: which action of the action block this rule prohibits. This is the ONE
%   predicate that turns an editor rule into a legality gate, and it is content.
% faction_law(RuleId, FactionId) — the rule is that faction's law and binds its
%   members only. Absent = a norm binding on everyone (a social more, a taboo).
% faction_law_protects(RuleId, hostile|neutral|allied) — a law that protects a
%   CLASS of target rather than naming conditions: "do not raise a hand against
%   an ally" is a stance, not a when-clause, and authoring it as one would mean
%   re-authoring the law every time the diplomatic map moves.
% norm_truth(RuleId, Predicate, Object) — what a WITNESSED violation makes true
%   of its actor. The consequence is a claim the rest of the simulation reads
%   like any other truth, which is what "legible to the simulation" means.
% norm_reputation(RuleId, FactionId, Delta) — the reputation with FactionId that
%   a witnessed violation costs. A negative Delta is the usual authoring; a
%   positive one is a world where the thieves' guild approves.
:- dynamic(rule_forbids_action/2).
:- dynamic(faction_law/2).
:- dynamic(faction_law_protects/2).
:- dynamic(norm_truth/3).
:- dynamic(norm_reputation/3).

% ── The combat block's faction facts (AUTHORED, adopted not re-minted) ────
% faction(EntityId, FactionId) and faction_stance(A, B, hostile|neutral|allied)
% are docs/mechanic-predicates.md §4's, unchanged. Declared here so this pack
% loads standalone-ish; never redefined.
:- dynamic(faction/2).
:- dynamic(faction_stance/3).

% ── Runtime facts (per playthrough) ───────────────────────────────────────
% act(EventId, Actor, ActionId, Target) — an act that HAPPENED this tick. The
%   EventId is the PERCEPT id of the same event (ai/perception.ts), not a second
%   identifier: witnessing is then perception's answer rather than a parallel
%   ledger, and an act nobody published as a percept is an act nobody could have
%   seen. Target is the atom `none` for a targetless action, matching
%   action_target_choice/2 in the decision pack.
% violation_record(EventId, RuleId, Actor, Witness) — the durable memory of a
%   witnessed violation, written by enforceActs/2. A percept lasts a tick; the
%   fact that the guard saw Renaud take the ledger has to outlive it, or a
%   consequence could only ever be immediate.
:- dynamic(act/4).
:- dynamic(violation_record/4).

% ── Norms: the rule block, read as prohibitions ───────────────────────────

% A NORM is an active rule that prohibits an action. Its KIND is the rule's own
% category, so "which body of rule refused this" is answerable without core
% holding an opinion about what bodies of rule exist.
norm(Rule, Kind) :-
  rule_active(Rule),
  rule_forbids_action(Rule, _),
  norm_kind(Rule, Kind).

norm_kind(Rule, Kind) :- rule_category(Rule, Kind).
norm_kind(Rule, unclassified) :- rule_active(Rule), \+ rule_category(Rule, _).

% A norm is binding on EVERYONE unless something scoped it. faction_law/2 is one
% scope; geo/region-predicates.ts adds a second (jurisdiction_law/2 — a law of
% the ground you stand on rather than of the house you belong to). So the
% exclusion is a predicate instead of a lengthening list of \+ goals in the
% universal clause below, and it is declared :- dynamic so a pack consulted LATER
% can add a clause for it — the same extension seam this pack itself reaches
% forbids/4 through. A scope that forgot to register here would make its norm
% bind the whole world, which is why this is one predicate and not a convention.
:- dynamic(scoped_norm/1).
scoped_norm(Rule) :- faction_law(Rule, _).

% ── The gate's input ──────────────────────────────────────────────────────
% forbids/4 is the ONLY thing US-2's permissible/3 reads. Everything above this
% line exists to define these three clauses, and everything below reads them.

% A norm binding on everyone, whose own conditions hold for this actor and
% target. Most social mores and every taboo are this clause.
forbids(Rule, A, Act, T) :-
  norm(Rule, _),
  \+ scoped_norm(Rule),
  rule_forbids_action(Rule, Act),
  rule_applies(Rule, A, T).

% Faction law, with its own conditions: binding on the faction's members, and
% only on them. An outsider is not bound by a law they are not under — that is
% the difference between a law and a more, and collapsing the two would make
% every faction's code universal.
forbids(Rule, A, Act, T) :-
  norm(Rule, _),
  faction_law(Rule, F),
  faction(A, F),
  rule_forbids_action(Rule, Act),
  rule_applies(Rule, A, T).

% Faction law by stance: forbidden because of who the TARGET is to the actor's
% faction. Reads the diplomatic map at query time, so an alliance signed this
% session changes what is permitted without re-authoring the law.
forbids(Rule, A, Act, T) :-
  norm(Rule, _),
  faction_law(Rule, F),
  faction(A, F),
  rule_forbids_action(Rule, Act),
  faction_law_protects(Rule, Stance),
  faction(T, G),
  faction_stance(F, G, Stance).

% ── Violations: a norm broken by an act that actually happened ────────────

% The gate refuses an action BEFORE it is taken; a violation is what a broken
% norm looks like AFTER one was. Both read the same forbids/4, so an NPC that
% was gated and a player who was not are judged by one body of rule.
violation(E, Rule, Actor, Act, T) :-
  act(E, Actor, Act, T),
  forbids(Rule, Actor, Act, T).

% The same question with every column projected to an atom — the form a caller
% binds, since a query binding collapses a compound to its functor. The actor is
% the only column that can be one; Act and T are already atoms of the action
% block.
violation_of(E, Rule, Actor, Act, T) :-
  violation(E, Rule, Subject, Act, T),
  agent_atom(Subject, Actor).

% ── Witnessing: the gate on social consequence ────────────────────────────

% Who saw a violation. witness/2 already excludes the actor from witnessing
% their own act, so a crime is never self-reported.
witnessed_violation(E, Rule, W) :-
  violation(E, Rule, _, _, _),
  witness(Agent, E),
  agent_atom(Agent, W).

% Nobody saw it. A violation with no witness has NO social consequence, which is
% what makes stealth worth anything — and note it is still a VIOLATION, so a
% world that wants a private guilt or a divine observer has a predicate to hang
% it on rather than needing core to relax the gate.
secret_violation(E, Rule) :-
  violation(E, Rule, _, _, _),
  \+ witness(_, E).

% ── Consequences: structured, never a boolean ────────────────────────────

% What a witnessed violation makes TRUE of its actor. The enforcement layer's
% output is a fact the rest of the simulation reads like any other, not a
% refusal: a reaction can be authored against claim/4 with no knowledge that an
% enforcer produced it.
enforced_truth(E, Rule, Actor, P, O) :-
  violation(E, Rule, Subject, _, _),
  witness(_, E),
  norm_truth(Rule, P, O),
  agent_atom(Subject, Actor).

% What it costs. reputation_change/3 (prolog/helper-predicates.ts) is the
% existing standing mechanism and reputation/3 already sums it; enforcement
% contributes a row rather than minting a second scale.
enforced_reputation(E, Rule, Actor, Faction, Delta) :-
  violation(E, Rule, Subject, _, _),
  witness(_, E),
  norm_reputation(Rule, Faction, Delta),
  agent_atom(Subject, Actor).

% ── Scalar projection ─────────────────────────────────────────────────────
% A query binding collapses a compound term to its functor (prolog/wasm-engine.ts),
% so every column this pack yields to a caller must be an atom. Content spells an
% agent either way — a legacy id atom, or the KINP id/3 term ai/perception.ts
% emits — and both are projected here, in the pack, so a native engine reads one
% rule instead of re-deriving the mapping.
%
% An id/3 term projects to its CURIE, because the CURIE is what the rest of the
% substrate keys an agent by: agentRefKey() in ai/action-selection.ts, the
% blackboard's slot key, and a perception's agent column are all the same string.
% curie/2 is a ground fact identity-facts.ts emits per identifier, so this is a
% lookup and not string surgery. A KB carrying no identity bridge falls back to
% the local id — a degraded answer rather than no answer, since a witness core
% cannot name is a witness the world cannot react to.
agent_atom(A, A) :- atom(A).
agent_atom(Id, Curie) :- nonvar(Id), \+ atom(Id), curie(Id, Curie).
agent_atom(Id, Local) :- nonvar(Id), \+ atom(Id), \+ curie(Id, _), id_local(Id, Local).


% ═══════════════════════════════════════════════════════════════════════════
% Game-AI substrate — goals, plan steps and the planning frontier
% packages/core/docs/game-ai-substrate.md §7
%
% COMPOSITION PACK. Consult AFTER ai/action-predicates.ts: this pack reads
% permissible/3, forbidden/3 and action_target_choice/2, which that pack owns.
% ═══════════════════════════════════════════════════════════════════════════

% ── Goals (AUTHORED — world content) ──────────────────────────────────────
% goal(GoalId, Name) — a goal this world knows about. Authored, because what an
%   NPC may want is world design: "keep the shop stocked", "get home by dark".
% goal_requires(GoalId, Condition) — the conditions that must hold for the goal
%   to be met. A goal with no requirement is met the moment it is adopted, which
%   is a legitimate way to author a goal that is only a label.
% action_achieves(ActionId, Condition) — what taking the action makes true. The
%   link from the action block to the condition vocabulary, and the reason a plan
%   can be built at all.
% action_needs(ActionId, Condition) — what must be true BEFORE the action can be
%   taken, in the same vocabulary. Deliberately NOT action_prerequisite/2: a
%   prerequisite is arbitrary world state a plan cannot bring about, while a
%   condition is exactly the thing another step can achieve.
:- dynamic(goal/2).
:- dynamic(goal_requires/2).
:- dynamic(action_achieves/2).
:- dynamic(action_needs/2).

% ── Runtime facts (per playthrough) ───────────────────────────────────────
% agent_goal(Agent, GoalId, Priority 0..100) — what this agent is pursuing now.
%   Runtime: an NPC adopts and drops goals as the world moves, and which goal it
%   holds at a tick belongs in the save file, never on a world template.
% condition_met(Agent, Condition) — what is already true for it. Agent-relative
%   because most conditions are ("has the key", "is at the forge"); a world with
%   a global condition authors the rule condition_met(_, door_open) :- ... over
%   its own state, which is the intended escape hatch and the reason nothing here
%   assumes this predicate is a plain fact.
:- dynamic(agent_goal/3).
:- dynamic(condition_met/2).

% ── Goals ─────────────────────────────────────────────────────────────────

% The goals this agent holds. The roster is the CALLER's, exactly as in the
% decision pack: an unbound agent enumerates nothing, so "who is planning" never
% depends on which facts happened to load.
pursuing(A, G, P) :-
  nonvar(A),
  agent_goal(A, G, P),
  goal(G, _).

% What the goal still wants — the planning frontier, one condition per solution.
goal_unmet(A, G, C) :-
  goal_requires(G, C),
  \+ condition_met(A, C).

% A goal with nothing unmet. Checked against the goal block so a typo'd goal id
% is unsatisfied rather than vacuously satisfied.
goal_satisfied(A, G) :-
  goal(G, _),
  \+ goal_unmet(A, G, _).

% ── Steps ─────────────────────────────────────────────────────────────────

% A step the agent can take THIS TICK: it achieves the condition, the decision
% layer's gate both enumerates it and permits it, and nothing the PLAN can see
% blocks it. The only kind of step a plan may start with, because a plan whose
% first step cannot be taken is a plan that never moves.
%
% step_ready/2 is not redundant with the gate. permissible/3 goes through
% can_perform/2,3 — the action block's own feasibility — which knows nothing
% about the condition vocabulary: an action with no action_prerequisite/2 is
% feasible by default and would otherwise be reported takeable while the plan
% still owed it a bucket.
plan_step(A, C, Act, T) :-
  action_achieves(Act, C),
  permissible(A, Act, T),
  step_ready(A, Act).

% A step the plan may REACH: it achieves the condition and is not forbidden, but
% its feasibility is left to the tick it actually runs on. Without this, chaining
% is impossible — "open the door" is not feasible until "take the key" has run,
% so a planner that only saw plan_step/4 could never plan past length one.
%
% The gate is applied here too, and that is the point: a forbidden action is not
% a candidate at any depth of any plan.
projected_step(A, C, Act, T) :-
  action_achieves(Act, C),
  action_target_choice(Act, T),
  \+ forbidden(A, Act, T).

% What a step still needs before it can be taken. The planner's chaining edge:
% every solution here is a sub-goal an earlier step has to achieve.
step_blocked(A, Act, C) :-
  action_needs(Act, C),
  \+ condition_met(A, C).

% A step nothing blocks. Act is required to be bound: an unbound action would ask
% "is there an action nothing blocks", which is a different question and one whose
% answer depends on what the KB happens to hold.
step_ready(A, Act) :-
  nonvar(Act),
  \+ step_blocked(A, Act, _).

% One row of the frontier, joined: an unmet condition of the goal and a step that
% would achieve it. The planner reads this to seed its search and then bounds the
% search itself — see ./planner, and §7 for why the SEARCH is TypeScript's while
% the truth and the permission stay the KB's.
goal_step(A, G, Act, T, C) :-
  goal_unmet(A, G, C),
  plan_step(A, C, Act, T).
