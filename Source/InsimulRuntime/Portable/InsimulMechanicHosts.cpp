// Copyright 2024 Insimul. All Rights Reserved.

#include "InsimulMechanicHosts.h"

namespace insimul {

// ── the closed vocabularies (declared in InsimulMechanicContracts.h) ────────

const std::vector<std::string> MovementUrgencies = {"idle", "ordinary", "hurried", "urgent"};

const std::vector<std::string> MovementStances = {"standing", "crouching", "prone"};

const std::vector<std::string> NeedTypes = {"hunger", "thirst", "temperature", "stamina", "sleep"};

bool IsInVocabulary(const std::vector<std::string>& Vocabulary, const std::string& Atom) {
	for (const std::string& Known : Vocabulary) {
		if (Known == Atom) {
			return true;
		}
	}
	return false;
}

// ── FInsimulHostAdapter ────────────────────────────────────────────────────

bool FInsimulHostAdapter::Has(const std::string& HostInterface) const {
	if (HostInterface == "ICombatStatSink") return CombatStats != nullptr;
	if (HostInterface == "ITrajectoryProbe") return Trajectory != nullptr;
	if (HostInterface == "IPerceptionProbe") return Perception != nullptr;
	if (HostInterface == "ITraversalProbe") return Traversal != nullptr;
	if (HostInterface == "ILocomotionHost") return Locomotion != nullptr;
	if (HostInterface == "ISkillModifierSink") return SkillModifiers != nullptr;
	return false;
}

std::string FInsimulHostAdapter::ConsequenceOf(const std::string& HostInterface) {
	if (HostInterface == "ICombatStatSink") {
		return "equipment tracked, stats not applied — core still totals a loadout, "
		       "nothing in the engine reads the totals";
	}
	if (HostInterface == "ITrajectoryProbe") {
		return "ranged attacks resolve on reach and accuracy alone; nothing obstructs them";
	}
	if (HostInterface == "IPerceptionProbe") {
		return "the caller must hand readings to DetectionTracker.observe itself, "
		       "or no guard senses anything";
	}
	if (HostInterface == "ITraversalProbe") {
		return "every geometric link is treated as passable";
	}
	if (HostInterface == "ILocomotionHost") {
		return "every ordered movement counts as arrived — world state moves, nothing is animated";
	}
	if (HostInterface == "ISkillModifierSink") {
		return "only the parameters naming a field of a core snapshot take effect; "
		       "move_speed and reach reach nobody";
	}
	if (HostInterface == "ICombatSystem") {
		return "combat decisions are made and tracked, and nothing is executed";
	}
	if (HostInterface == "ISurvivalSystem") {
		return "core's own energy/3 meter still moves; the host's needs clock is "
		       "never told about a spend";
	}
	return "unknown host interface";
}

// ── FInsimulMechanicHosts ──────────────────────────────────────────────────

bool FInsimulMechanicHosts::Has(const std::string& HostInterface) const {
	if (HostInterface == "ICombatSystem") return Combat != nullptr;
	if (HostInterface == "ISurvivalSystem") return Survival != nullptr;
	return Adapter.Has(HostInterface);
}

const std::vector<std::string>& FInsimulMechanicHosts::Slots() {
	static const std::vector<std::string> Names = {
		"ICombatSystem", "ISurvivalSystem", "ICombatStatSink", "ITrajectoryProbe",
		"IPerceptionProbe", "ITraversalProbe", "ILocomotionHost", "ISkillModifierSink",
	};
	return Names;
}

std::vector<std::string> FInsimulMechanicHosts::RestrictTo(const std::vector<std::string>& ActiveInterfaces) {
	// Slots() drives the loop so a slot added to the container without a line here
	// is dropped by default rather than silently surviving every restriction.
	std::vector<std::string> Dropped;
	for (const std::string& Name : Slots()) {
		bool bActive = false;
		for (const std::string& Active : ActiveInterfaces) {
			if (Active == Name) {
				bActive = true;
				break;
			}
		}
		if (bActive || !Has(Name)) {
			continue;
		}
		// Clearing a slot UNREGISTERS. It never destroys — every pointer here is
		// borrowed and the game still owns the object (see the header).
		if (Name == "ICombatSystem") Combat = nullptr;
		else if (Name == "ISurvivalSystem") Survival = nullptr;
		else if (Name == "ICombatStatSink") Adapter.CombatStats = nullptr;
		else if (Name == "ITrajectoryProbe") Adapter.Trajectory = nullptr;
		else if (Name == "IPerceptionProbe") Adapter.Perception = nullptr;
		else if (Name == "ITraversalProbe") Adapter.Traversal = nullptr;
		else if (Name == "ILocomotionHost") Adapter.Locomotion = nullptr;
		else if (Name == "ISkillModifierSink") Adapter.SkillModifiers = nullptr;
		Dropped.push_back(Name);
	}
	// A slot that survived the clear would make this a no-op that reads as a drop.
	std::string Survivor;
	for (const std::string& Name : Dropped) {
		if (Has(Name)) {
			Survivor = Name;
			break;
		}
	}
	if (!Survivor.empty()) {
		Dropped.push_back("INTERNAL: " + Survivor + " reported dropped and is still registered");
	}
	return Dropped;
}

FTrajectoryReading FInsimulMechanicHosts::Ask(const FTrajectoryQuery& Query) const {
	FTrajectoryReading Reading;
	Reading.bClear = true;
	if (Adapter.Trajectory == nullptr) {
		return Reading;
	}
	FTrajectoryReading Answer;
	if (!Adapter.Trajectory->Query(Query, Answer)) {
		return Reading;
	}
	return Answer;
}

bool FInsimulMechanicHosts::Sense(const FPerceptionQuery& Query, FPerceptionReading& OutReading) const {
	if (Adapter.Perception == nullptr) {
		return false;
	}
	return Adapter.Perception->Sense(Query, OutReading);
}

FTraversalReading FInsimulMechanicHosts::Ask(const FTraversalQuery& Query) const {
	FTraversalReading Reading;
	Reading.bPassable = true;
	if (Adapter.Traversal == nullptr) {
		return Reading;
	}
	FTraversalReading Answer;
	if (!Adapter.Traversal->Query(Query, Answer)) {
		return Reading;
	}
	return Answer;
}

FArrivalReport FInsimulMechanicHosts::Travel(const FLocomotionOrder& Order) const {
	FArrivalReport Report;
	Report.bArrived = true;
	if (Adapter.Locomotion == nullptr) {
		return Report;
	}
	FArrivalReport Answer;
	if (!Adapter.Locomotion->Travel(Order, Answer)) {
		return Report;
	}
	return Answer;
}

void FInsimulMechanicHosts::ApplySkillModifiers(const std::string& ActorId, const FSkillModifiers& Modifiers) const {
	if (Adapter.SkillModifiers == nullptr || ActorId.empty()) {
		return;
	}
	Adapter.SkillModifiers->ApplyModifiers(ActorId, Modifiers);
}

void FInsimulMechanicHosts::ApplyCombatStats(const std::string& EntityId, const FCombatStats& Stats) const {
	if (Adapter.CombatStats == nullptr || EntityId.empty()) {
		return;
	}
	Adapter.CombatStats->ApplyStats(EntityId, Stats);
}

void FInsimulMechanicHosts::ApplyDamage(const std::string& TargetId, double Damage) const {
	if (Combat == nullptr || TargetId.empty()) {
		return;
	}
	Combat->ApplyDamage(TargetId, Damage);
}

bool FInsimulMechanicHosts::ForwardStaminaSpend(const std::string& ActorId, double Amount) const {
	if (Survival == nullptr) {
		return true;
	}
	if (SurvivalActorId.empty() || SurvivalActorId != ActorId) {
		return true;
	}
	return Survival->ConsumeStamina(Amount);
}

} // namespace insimul
