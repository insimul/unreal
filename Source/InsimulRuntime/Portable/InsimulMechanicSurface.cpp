// Copyright 2024 Insimul. All Rights Reserved.

#include "InsimulMechanicSurface.h"

#include "InsimulJson.h"

namespace insimul {

const char* const FInsimulMechanicSurface::MethodMethods = "core.methods";

namespace {

std::string JoinAtoms(const std::vector<std::string>& Atoms) {
	std::string Out;
	for (std::size_t Index = 0; Index < Atoms.size(); ++Index) {
		if (Index > 0) {
			Out += ", ";
		}
		Out += Atoms[Index];
	}
	return Out;
}

bool Contains(const std::vector<std::string>& Haystack, const std::string& Needle) {
	for (const std::string& Item : Haystack) {
		if (Item == Needle) {
			return true;
		}
	}
	return false;
}

} // namespace

const std::vector<FMechanicModule>& MechanicModules() {
	static const std::vector<FMechanicModule> Modules = {
		{"combat", "120",
		 {"CombatResolver"},
		 {"ICombatSystem", "ITrajectoryProbe"},
		 {"combat.create", "combat.attack"}},

		{"stamina", "120",
		 {"StaminaPool"},
		 {"ISurvivalSystem"},
		 {"stamina.create", "stamina.spend"}},

		{"perception", "121",
		 {"DetectionTracker"},
		 {"IPerceptionProbe"},
		 {"perception.create", "perception.observe"}},

		{"traversal", "122",
		 {"TraversalPlanner", "VehicleRegistry", "FastTravelDirector"},
		 {"ITraversalProbe", "ILocomotionHost"},
		 {"traversal.create", "traversal.traverse"}},

		{"skill", "123",
		 {"SkillProgression"},
		 {"ISkillModifierSink"},
		 {"skill.create", "skill.unlock"}},

		{"equipment", "124",
		 {"EquipmentManager", "ItemLedger", "Market", "ItemPlacer"},
		 {"ICombatStatSink"},
		 {"equipment.create", "equipment.equip"}},

		{"routine", "125",
		 {"RoutineDirector", "LocomotionDirector"},
		 {"ILocomotionHost"},
		 {"routine.create", "routine.tick"}},
	};
	return Modules;
}

const FMechanicModule* FindMechanicModule(const std::string& Id) {
	for (const FMechanicModule& Module : MechanicModules()) {
		if (Module.Id == Id) {
			return &Module;
		}
	}
	return nullptr;
}

const std::vector<std::string>& MechanicHostInterfaces() {
	static const std::vector<std::string> Names = [] {
		std::vector<std::string> Seen;
		for (const FMechanicModule& Module : MechanicModules()) {
			for (const std::string& Interface : Module.HostInterfaces) {
				if (!Contains(Seen, Interface)) {
					Seen.push_back(Interface);
				}
			}
		}
		return Seen;
	}();
	return Names;
}

std::vector<std::string> FInsimulMechanicSurface::ParseMethodList(const std::string& Json) {
	std::vector<std::string> Out;
	const FJsonParseResult Parsed = ParseJson(Json);
	if (!Parsed.bOk || !Parsed.Root || !Parsed.Root->IsObject()) {
		return Out;
	}
	const FJsonValue* Methods = Parsed.Root->Find("methods");
	if (Methods == nullptr || !Methods->IsArray()) {
		return Out;
	}
	for (const FJsonValuePtr& Item : Methods->ArrayItems) {
		if (Item && Item->IsString() && !Contains(Out, Item->StringValue)) {
			Out.push_back(Item->StringValue);
		}
	}
	return Out;
}

void FInsimulMechanicSurface::AskBridge() {
	MethodList.clear();
	BridgeErrorText.clear();
	bBridgeAnswered = false;

	if (Caller == nullptr || !Caller->IsAvailable()) {
		BridgeErrorText = "libinsimulcore is not loadable in this build";
		if (Caller != nullptr && !Caller->LastError().empty()) {
			BridgeErrorText += " (" + Caller->LastError() + ")";
		}
		return;
	}

	std::string ResultJson;
	if (!Caller->Call(MethodMethods, "{}", ResultJson)) {
		BridgeErrorText = "the core bridge cannot list its methods";
		if (!Caller->LastError().empty()) {
			BridgeErrorText += " (" + Caller->LastError() + ")";
		}
		return;
	}

	MethodList = ParseMethodList(ResultJson);
	if (MethodList.empty()) {
		BridgeErrorText = "the core bridge answered core.methods with no method list";
		return;
	}
	bBridgeAnswered = true;
}

FMechanicModuleReport FInsimulMechanicSurface::ReportFor(
	const FMechanicModule& Module, const FInsimulMechanicHosts& Hosts) const {
	FMechanicModuleReport Report;
	Report.ModuleId = Module.Id;

	for (const std::string& Interface : Module.HostInterfaces) {
		if (!Hosts.Has(Interface)) {
			Report.MissingHosts.push_back(Interface + " — " + FInsimulHostAdapter::ConsequenceOf(Interface));
		}
	}

	if (!bBridgeAnswered) {
		Report.State = EMechanicState::NoNativeBridge;
		Report.MissingMethods = Module.RequiredMethods;
		Report.Message = Module.Id + ": no core bridge, so nothing decides this mechanic (" +
		                 BridgeErrorText +
		                 "). Remedy: build or fetch libinsimulcore and re-check the ABI stamp.";
		return Report;
	}

	for (const std::string& Method : Module.RequiredMethods) {
		if (!Contains(MethodList, Method)) {
			Report.MissingMethods.push_back(Method);
		}
	}

	if (!Report.MissingMethods.empty()) {
		Report.State = EMechanicState::BridgeHasNoRow;
		Report.Message = Module.Id + ": this build's core bridge carries no " + Module.Id +
		                 " rows (missing " + JoinAtoms(Report.MissingMethods) + "), so " +
		                 JoinAtoms(Module.DecisionLayers) +
		                 " cannot be reached from C++. The host half is implemented and inert. "
		                 "Remedy: native/corebridge/js/entry.js — see RUNTIME_CORE_ADOPTION.md §12.";
		return Report;
	}

	if (!Report.MissingHosts.empty()) {
		Report.State = EMechanicState::NoHost;
		Report.Message = Module.Id + ": core can decide this mechanic and this game wired no host for " +
		                 JoinAtoms(Report.MissingHosts) + ".";
		return Report;
	}

	Report.State = EMechanicState::Ready;
	Report.Message = Module.Id + ": " + JoinAtoms(Module.DecisionLayers) + " is reachable and " +
	                 JoinAtoms(Module.HostInterfaces) + " is wired.";
	return Report;
}

std::vector<FMechanicModuleReport> FInsimulMechanicSurface::Probe(const FInsimulMechanicHosts& Hosts) {
	AskBridge();
	std::vector<FMechanicModuleReport> Reports;
	for (const FMechanicModule& Module : MechanicModules()) {
		Reports.push_back(ReportFor(Module, Hosts));
	}
	return Reports;
}

} // namespace insimul
