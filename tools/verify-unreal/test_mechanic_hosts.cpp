// test_mechanic_hosts.cpp — the band-120 host half, executed (US-1 of tasklist 146).
//
// WHAT IT COVERS. The portable layer of the adoption: the eight-interface mirror
// (Source/InsimulRuntime/Portable/InsimulMechanicContracts.h), the fallbacks core
// documents per interface (InsimulMechanicHosts), and the module surface that asks a
// build what it can actually do (InsimulMechanicSurface). Every one of those runs
// here under plain clang++ with no Unreal and no native library.
//
// WHAT IT DOES NOT COVER. The UE implementations in templates/source/mechanics/ — the
// raycasts, the navmesh queries, the AIController dispatch, the survival subsystem
// bridge. Those need a real engine and a real level; VERIFICATION.md is their pass.
// The gate that checks they EXIST and declare the right members without compiling
// them is tools/verify-mechanics/check-mechanics.mjs. That is the same coverage
// statement this repo's structural C++ gate carries, stated rather than implied.
//
// ── THE TWO LEGS ────────────────────────────────────────────────────────────
//
//   (default)   no native library. Drives the fallbacks and the surface's
//               NoNativeBridge / BridgeHasNoRow / NoHost / Ready states over stub
//               callers, so every branch — including the one a shipping build takes
//               today — is exercised. ctest target `mechanic_hosts`.
//
//   --bridge    the REAL libinsimulcore. Asks `core.methods` of the artifact that
//               ships and asserts what the surface makes of the answer. This is the
//               measurement §12.1 reports, as a test rather than a paragraph: it
//               fails if a mechanic row APPEARS as loudly as if one disappeared,
//               because an arriving row means the host half must be re-checked
//               against a real caller. ctest target `mechanic_bridge`.

#include "InsimulMechanicHosts.h"
#include "InsimulMechanicSurface.h"

#if INSIMUL_HAVE_CORE_BRIDGE
#include "InsimulCoreBridge.h"
#endif

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

int Failures = 0;
int Checks = 0;

void Check(bool bCondition, const std::string& What) {
	Checks++;
	if (!bCondition) {
		Failures++;
		std::printf("  x %s\n", What.c_str());
	}
}

/** A caller that answers `core.methods` with whatever it was handed. */
class FStubCaller : public insimul::ICoreCaller {
public:
	explicit FStubCaller(const std::string& InMethodsJson, bool bInAvailable = true)
		: MethodsJson(InMethodsJson), bAvailable(bInAvailable) {}

	bool IsAvailable() const override { return bAvailable; }

	bool Call(const std::string& Method, const std::string& ArgsJson, std::string& OutJson) override {
		LastMethod = Method;
		LastArgs = ArgsJson;
		if (!bAvailable || MethodsJson.empty()) {
			LastErrorText = "stub caller was configured to fail";
			return false;
		}
		OutJson = MethodsJson;
		return true;
	}

	std::string LastError() const override { return LastErrorText; }
	std::string Version() const override { return "stub"; }

	std::string LastMethod;
	std::string LastArgs;

private:
	std::string MethodsJson;
	std::string LastErrorText;
	bool bAvailable = true;
};

/** A probe that always answers, so a reading can be distinguished from a fallback. */
class FAnsweringTrajectory : public insimul::ITrajectoryProbe {
public:
	bool Query(const insimul::FTrajectoryQuery& InQuery, insimul::FTrajectoryReading& OutReading) override {
		SeenAttacker = InQuery.Attacker;
		OutReading.bClear = false;
		OutReading.bHasSeparation = true;
		OutReading.Separation = 12.5;
		OutReading.BlockedBy = "pillar";
		return true;
	}
	std::string SeenAttacker;
};

/** A probe that cannot answer — the case core's contract calls "a probe that throws". */
class FSilentTrajectory : public insimul::ITrajectoryProbe {
public:
	bool Query(const insimul::FTrajectoryQuery&, insimul::FTrajectoryReading& OutReading) override {
		OutReading.bClear = false; // deliberately poisoned: a false return must discard it
		return false;
	}
};

class FSilentPerception : public insimul::IPerceptionProbe {
public:
	bool Sense(const insimul::FPerceptionQuery&, insimul::FPerceptionReading&) override { return false; }
};

class FAnsweringPerception : public insimul::IPerceptionProbe {
public:
	bool Sense(const insimul::FPerceptionQuery& InQuery, insimul::FPerceptionReading& OutReading) override {
		OutReading.Visibility = 0.75;
		OutReading.bHasLight = true;
		OutReading.Light = 30.0;
		OutReading.Stance = "crouching";
		SeenTick = InQuery.Tick;
		return true;
	}
	long long SeenTick = -1;
};

class FSilentTraversal : public insimul::ITraversalProbe {
public:
	bool Query(const insimul::FTraversalQuery&, insimul::FTraversalReading& OutReading) override {
		OutReading.bPassable = false; // poisoned, and must be discarded
		return false;
	}
};

class FRefusingLocomotion : public insimul::ILocomotionHost {
public:
	bool Travel(const insimul::FLocomotionOrder& Order, insimul::FArrivalReport& OutReport) override {
		SeenUrgency = Order.Urgency;
		OutReport.bArrived = false;
		OutReport.Location = "ledge";
		OutReport.Reason = "the ledge gave way";
		return true;
	}
	std::string SeenUrgency;
};

class FUnusableLocomotion : public insimul::ILocomotionHost {
public:
	bool Travel(const insimul::FLocomotionOrder&, insimul::FArrivalReport& OutReport) override {
		OutReport.bArrived = false; // poisoned
		return false;
	}
};

class FRecordingSkillSink : public insimul::ISkillModifierSink {
public:
	void ApplyModifiers(const std::string& ActorId, const insimul::FSkillModifiers& Modifiers) override {
		Calls++;
		LastActor = ActorId;
		LastSet = Modifiers;
	}
	int Calls = 0;
	std::string LastActor;
	insimul::FSkillModifiers LastSet;
};

class FRecordingStatSink : public insimul::ICombatStatSink {
public:
	bool GetBaseStats(const std::string& EntityId, insimul::FCombatStats& OutStats) const override {
		if (EntityId != "nessa") {
			return false;
		}
		OutStats.AttackPower = 4.0;
		return true;
	}
	void ApplyStats(const std::string& EntityId, const insimul::FCombatStats& Stats) override {
		LastEntity = EntityId;
		LastStats = Stats;
		Calls++;
	}
	int Calls = 0;
	std::string LastEntity;
	insimul::FCombatStats LastStats;
};

class FRecordingCombat : public insimul::ICombatSystem {
public:
	void RegisterEntity(const insimul::FCombatEntityData& Entity) override { Health = Entity.Health; }
	void UnregisterEntity(const std::string&) override {}
	bool ExecuteAttack(const std::string&, const std::string&, insimul::FDamageResult&) override { return false; }
	void ApplyDamage(const std::string& TargetId, double Damage) override {
		LastTarget = TargetId;
		Health -= Damage;
		Calls++;
	}
	bool IsCombatEnabled() const override { return true; }
	double GetHealth(const std::string&) const override { return Health; }
	void Heal(const std::string&, double Amount) override { Health += Amount; }
	void Dispose() override {}

	int Calls = 0;
	double Health = 20.0;
	std::string LastTarget;
};

class FMeterSurvival : public insimul::ISurvivalSystem {
public:
	void Update(double) override {}
	void RestoreNeed(const std::string&, double) override {}
	bool ConsumeStamina(double Amount) override {
		Spends++;
		if (Amount > Stamina) {
			return false;
		}
		Stamina -= Amount;
		return true;
	}
	void RecoverStamina(double Amount) override { Stamina += Amount; }
	void SetTemperature(double) override {}
	void AddModifier(const insimul::FNeedModifier&) override {}
	void RemoveModifier(const std::string&) override {}
	bool GetNeed(const std::string&, insimul::FNeedState&) const override { return false; }
	void GetAllNeeds(std::vector<insimul::FNeedState>&) const override {}
	double GetNeedPercent(const std::string&) const override { return 0.0; }
	bool IsAnyCritical() const override { return false; }
	bool IsAnyWarning() const override { return false; }
	void SetEnabled(bool) override {}
	bool IsEnabled() const override { return true; }
	void SetOnNeedChanged(void (*)(const insimul::FNeedState&)) override {}
	void SetOnSurvivalEvent(void (*)(const insimul::FSurvivalEvent&)) override {}
	void SetOnDamageFromNeed(void (*)(const std::string&, double)) override {}
	void Dispose() override {}

	int Spends = 0;
	double Stamina = 10.0;
};

const insimul::FMechanicModuleReport* ReportFor(
	const std::vector<insimul::FMechanicModuleReport>& Reports, const std::string& Id) {
	for (const insimul::FMechanicModuleReport& Report : Reports) {
		if (Report.ModuleId == Id) {
			return &Report;
		}
	}
	return nullptr;
}

bool Mentions(const std::string& Haystack, const std::string& Needle) {
	return Haystack.find(Needle) != std::string::npos;
}

// ── the mirror ──────────────────────────────────────────────────────────────

void TestTheModuleTable() {
	const std::vector<insimul::FMechanicModule>& Modules = insimul::MechanicModules();
	Check(Modules.size() == 7, "core's band 120-125 names seven modules");
	Check(insimul::MechanicHostInterfaces().size() == 8,
		"those seven name eight DISTINCT host interfaces (traversal and routine share ILocomotionHost)");

	for (const insimul::FMechanicModule& Module : Modules) {
		Check(!Module.DecisionLayers.empty(), Module.Id + " names at least one decision layer");
		Check(!Module.HostInterfaces.empty(), Module.Id + " names at least one host interface");
		Check(Module.RequiredMethods.size() == 2,
			Module.Id + " proposes a create + verb row pair, so reachability is askable");
		for (const std::string& Interface : Module.HostInterfaces) {
			const std::string Consequence = insimul::FInsimulHostAdapter::ConsequenceOf(Interface);
			Check(Consequence != "unknown host interface",
				Interface + " states what leaving it empty COSTS — no silent no-op");
		}
	}

	// Every slot the container can hold is an interface the band actually names, and
	// vice versa. A slot that fell out of Slots() would silently stop being restricted.
	for (const std::string& Slot : insimul::FInsimulMechanicHosts::Slots()) {
		bool bNamed = false;
		for (const std::string& Interface : insimul::MechanicHostInterfaces()) {
			bNamed = bNamed || Interface == Slot;
		}
		Check(bNamed, Slot + " is a slot AND an interface the band names");
	}
	Check(insimul::FInsimulMechanicHosts::Slots().size() == insimul::MechanicHostInterfaces().size(),
		"Slots() and the band's interfaces are the same set");

	Check(insimul::FindMechanicModule("combat") != nullptr, "combat is findable by id");
	Check(insimul::FindMechanicModule("nonesuch") == nullptr, "an unknown module id finds nothing");
}

void TestTheVocabularies() {
	Check(insimul::MovementUrgencies.size() == 4, "four urgency rungs, and never a speed");
	Check(insimul::MovementStances.size() == 3, "three stances, shared with PerceptionReading.stance");
	Check(insimul::NeedTypes.size() == 5, "core's five needs");
	Check(insimul::IsInVocabulary(insimul::MovementUrgencies, "hurried"), "hurried is an urgency");
	Check(!insimul::IsInVocabulary(insimul::MovementUrgencies, "sprinting"),
		"a speed-shaped word is NOT an urgency");
	Check(insimul::IsInVocabulary(insimul::MovementStances, "crouching"), "crouching is a stance");
}

// ── the fallbacks core documents ────────────────────────────────────────────

void TestTheFallbacks() {
	insimul::FInsimulMechanicHosts Empty;

	insimul::FTrajectoryQuery Shot;
	Shot.Attacker = "nessa";
	Shot.Target = "wolf";
	Shot.Action = "bow_shot";
	Check(Empty.Ask(Shot).bClear, "no trajectory probe: the line reads CLEAR");

	insimul::FPerceptionQuery Look;
	Look.Observer = "guard";
	Look.Target = "nessa";
	insimul::FPerceptionReading Reading;
	Check(!Empty.Sense(Look, Reading), "no perception probe: NO READING, not zero visibility");

	insimul::FTraversalQuery Gap;
	Gap.Actor = "nessa";
	Gap.From = "ledge";
	Gap.To = "ridge";
	Check(Empty.Ask(Gap).bPassable, "no traversal probe: the geometric link reads PASSABLE");

	insimul::FLocomotionOrder Walk;
	Walk.Actor = "nessa";
	Walk.To = "forge_gate";
	Walk.Urgency = "ordinary";
	Walk.Stance = "standing";
	Check(Empty.Travel(Walk).bArrived, "no locomotion host: the movement counts as ARRIVED");

	Check(Empty.ForwardStaminaSpend("nessa", 3.0),
		"no survival host: the spend is reported as having gone through, so core's own meter rules");

	// A host that is wired but cannot answer must fall back identically. This is the
	// case core states as "a probe that throws"; here it is a false return, because
	// Unreal compiles with exceptions disabled (see InsimulMechanicContracts.h).
	FSilentTrajectory SilentShot;
	FSilentPerception SilentLook;
	FSilentTraversal SilentGap;
	FUnusableLocomotion SilentWalk;
	insimul::FInsimulMechanicHosts Silent;
	Silent.Adapter.Trajectory = &SilentShot;
	Silent.Adapter.Perception = &SilentLook;
	Silent.Adapter.Traversal = &SilentGap;
	Silent.Adapter.Locomotion = &SilentWalk;

	Check(Silent.Ask(Shot).bClear, "an unanswering trajectory probe reads CLEAR, not blocked");
	Check(!Silent.Sense(Look, Reading), "an unanswering perception probe reads as no reading");
	Check(Silent.Ask(Gap).bPassable, "an unanswering traversal probe reads PASSABLE, not blocked");
	Check(Silent.Travel(Walk).bArrived, "an unanswering locomotion host reads ARRIVED");
}

void TestAWiredHostIsNotOverridden() {
	FAnsweringTrajectory Shot;
	FAnsweringPerception Look;
	FRefusingLocomotion Walk;
	FRecordingSkillSink Skills;
	FRecordingStatSink Stats;
	FRecordingCombat Combat;
	FMeterSurvival Survival;

	insimul::FInsimulMechanicHosts Hosts;
	Hosts.Adapter.Trajectory = &Shot;
	Hosts.Adapter.Perception = &Look;
	Hosts.Adapter.Locomotion = &Walk;
	Hosts.Adapter.SkillModifiers = &Skills;
	Hosts.Adapter.CombatStats = &Stats;
	Hosts.Combat = &Combat;
	Hosts.Survival = &Survival;
	Hosts.SurvivalActorId = "nessa";

	insimul::FTrajectoryQuery Query;
	Query.Attacker = "nessa";
	Query.Target = "wolf";
	const insimul::FTrajectoryReading Answer = Hosts.Ask(Query);
	Check(!Answer.bClear && Answer.BlockedBy == "pillar",
		"a probe's real reading survives the fallback layer unchanged");
	Check(Answer.bHasSeparation && Answer.Separation == 12.5, "and so does its separation");
	Check(Shot.SeenAttacker == "nessa", "the query reached the probe intact");

	insimul::FPerceptionQuery Sense;
	Sense.Observer = "guard";
	Sense.Target = "nessa";
	Sense.Tick = 42;
	insimul::FPerceptionReading Reading;
	Check(Hosts.Sense(Sense, Reading), "a wired perception probe answers");
	Check(Reading.Stance == "crouching" && Look.SeenTick == 42, "and the tick and stance cross intact");

	insimul::FLocomotionOrder Order;
	Order.Actor = "nessa";
	Order.To = "ridge";
	Order.Urgency = "urgent";
	Order.Stance = "standing";
	const insimul::FArrivalReport Report = Hosts.Travel(Order);
	Check(!Report.bArrived && Report.Location == "ledge",
		"`arrived: false` is an ANSWER and is NOT overwritten by the fallback");
	Check(Walk.SeenUrgency == "urgent", "the urgency atom reached the host");

	insimul::FSkillModifiers Set;
	Set.push_back({"move_speed", 5.0});
	Hosts.ApplySkillModifiers("nessa", Set);
	Check(Skills.Calls == 1 && Skills.LastSet.size() == 1, "the whole modifier set reaches the sink");
	Hosts.ApplySkillModifiers("", Set);
	Check(Skills.Calls == 1, "an empty actor id reaches nothing");

	insimul::FCombatStats Totals;
	Totals.AttackPower = 9.0;
	Hosts.ApplyCombatStats("nessa", Totals);
	Check(Stats.Calls == 1 && Stats.LastStats.AttackPower == 9.0, "equipment totals reach the stat sink");

	Hosts.ApplyDamage("wolf", 6.0);
	Check(Combat.Calls == 1 && Combat.Health == 14.0,
		"damage core resolved is SUBTRACTED, not re-rolled");

	Check(Hosts.ForwardStaminaSpend("nessa", 4.0) && Survival.Spends == 1,
		"the owning actor's spend reaches the host meter");
	Check(Hosts.ForwardStaminaSpend("bram", 4.0) && Survival.Spends == 1,
		"another actor's spend does not — the host meter has no actor argument");
	Check(!Hosts.ForwardStaminaSpend("nessa", 99.0),
		"a refusal is reported as a refusal, not silently swallowed");

	// Has() must agree with what was wired, in both containers.
	Check(Hosts.Has("ICombatSystem") && Hosts.Has("ISurvivalSystem"), "the two ported systems report wired");
	Check(Hosts.Has("ITrajectoryProbe") && Hosts.Has("ICombatStatSink"), "adapter hooks report wired");
	Check(!Hosts.Has("ITraversalProbe"), "an unwired hook reports unwired");
	Check(!Hosts.Has("INotAnInterface"), "an unknown name reports unwired rather than asserting");
}

// ── the surface ─────────────────────────────────────────────────────────────

void TestTheSurface() {
	insimul::FInsimulMechanicHosts Hosts;

	// 1. No bridge at all.
	{
		insimul::FInsimulMechanicSurface Surface(nullptr);
		const std::vector<insimul::FMechanicModuleReport> Reports = Surface.Probe(Hosts);
		Check(Reports.size() == 7, "every band module is reported, wired or not");
		Check(!Surface.BridgeAnswered(), "a null caller is not an answer");
		const insimul::FMechanicModuleReport* Combat = ReportFor(Reports, "combat");
		Check(Combat != nullptr && Combat->State == insimul::EMechanicState::NoNativeBridge,
			"no library: NoNativeBridge");
		Check(Combat != nullptr && Combat->MissingMethods.size() == 2,
			"and both proposed rows are reported missing");
	}

	// 2. The bridge this repo actually ships: five methods, no mechanic row.
	{
		FStubCaller Shipped(
			"{\"methods\":[\"core.methods\",\"quest.hydrate\",\"quest.radiantTick\","
			"\"radiant.baseTemplates\",\"radiant.generate\"]}");
		insimul::FInsimulMechanicSurface Surface(&Shipped);
		const std::vector<insimul::FMechanicModuleReport> Reports = Surface.Probe(Hosts);
		Check(Surface.BridgeAnswered() && Surface.Methods().size() == 5,
			"the shipped surface is five methods");
		Check(Shipped.LastMethod == "core.methods" && Shipped.LastArgs == "{}",
			"the surface asks core.methods and nothing else");
		for (const insimul::FMechanicModuleReport& Report : Reports) {
			Check(Report.State == insimul::EMechanicState::BridgeHasNoRow,
				Report.ModuleId + ": a bridge with no mechanic row is BridgeHasNoRow");
			Check(Mentions(Report.Message, "entry.js"),
				Report.ModuleId + ": the message names where the row would be added");
		}
	}

	// 3. A bridge that DOES carry combat's rows, with no host wired.
	{
		FStubCaller WithCombat("{\"methods\":[\"core.methods\",\"combat.create\",\"combat.attack\"]}");
		insimul::FInsimulMechanicSurface Surface(&WithCombat);
		// Keep the vector alive: ReportFor returns a pointer INTO it.
		const std::vector<insimul::FMechanicModuleReport> Reports = Surface.Probe(Hosts);
		const insimul::FMechanicModuleReport* Combat = ReportFor(Reports, "combat");
		Check(Combat != nullptr && Combat->State == insimul::EMechanicState::NoHost,
			"rows present and no host wired: NoHost, which is legitimate and not a gap");
		Check(Combat != nullptr && Combat->MissingHosts.size() == 2,
			"both of combat's host interfaces are named as missing");
		Check(Combat != nullptr && Mentions(Combat->MissingHosts[0], "ranged attacks resolve on reach") == false,
			"the first missing host is ICombatSystem, whose consequence is its own");
		Check(Combat != nullptr && Mentions(Combat->Message, "wired no host"),
			"and the message says what is missing rather than 'unavailable'");
	}

	// 4. The same bridge with both of combat's hosts wired: Ready.
	{
		FRecordingCombat Combat;
		FAnsweringTrajectory Shot;
		insimul::FInsimulMechanicHosts Wired;
		Wired.Combat = &Combat;
		Wired.Adapter.Trajectory = &Shot;

		FStubCaller WithCombat("{\"methods\":[\"core.methods\",\"combat.create\",\"combat.attack\"]}");
		insimul::FInsimulMechanicSurface Surface(&WithCombat);
		const std::vector<insimul::FMechanicModuleReport> Reports = Surface.Probe(Wired);
		const insimul::FMechanicModuleReport* Report = ReportFor(Reports, "combat");
		Check(Report != nullptr && Report->State == insimul::EMechanicState::Ready,
			"rows present AND hosts wired: Ready — the only state that means adopted");
		const insimul::FMechanicModuleReport* Skill = ReportFor(Reports, "skill");
		Check(Skill != nullptr && Skill->State == insimul::EMechanicState::BridgeHasNoRow,
			"and a module whose rows are still absent is unaffected by combat's arrival");
	}

	// 5. A caller that is present but fails, and a malformed answer.
	{
		FStubCaller Broken("", /*bInAvailable=*/true);
		insimul::FInsimulMechanicSurface Surface(&Broken);
		Surface.Probe(Hosts);
		Check(!Surface.BridgeAnswered() && Mentions(Surface.BridgeError(), "cannot list its methods"),
			"a failing call is reported with its reason, never as an empty method list");

		FStubCaller Garbage("{\"methods\":\"not-an-array\"}");
		insimul::FInsimulMechanicSurface Garbled(&Garbage);
		Garbled.Probe(Hosts);
		Check(!Garbled.BridgeAnswered(), "a malformed answer is not an answer");
	}

	// 6. The parser, on its own.
	{
		const std::vector<std::string> Parsed =
			insimul::FInsimulMechanicSurface::ParseMethodList("{\"methods\":[\"b\",\"a\",\"b\"]}");
		Check(Parsed.size() == 2 && Parsed[0] == "b" && Parsed[1] == "a",
			"the method list is de-duplicated and left in the bridge's own order");
		Check(insimul::FInsimulMechanicSurface::ParseMethodList("nonsense").empty(),
			"an unparseable document yields no methods rather than a crash");
	}
}

#if INSIMUL_HAVE_CORE_BRIDGE
// ── the bridge leg: what the artifact that SHIPS actually answers ───────────

/** The whole method set this repo's libinsimulcore is expected to carry. Pinned, so a
 *  row that ARRIVES fails this test as loudly as one that disappears — an arriving
 *  mechanic row means the host half must be re-checked against a real caller, and a
 *  gate that only notices losses would let that land silently. */
const char* const EXPECTED_METHODS[] = {
	"core.methods", "quest.hydrate", "quest.radiantTick", "radiant.baseTemplates", "radiant.generate",
};

void TestTheRealBridge(insimul::ICoreCaller& Caller) {
	std::printf("  libinsimulcore: %s\n", Caller.Version().c_str());
	Check(Caller.IsAvailable(), "the real libinsimulcore starts");

	insimul::FInsimulMechanicHosts Hosts;
	insimul::FInsimulMechanicSurface Surface(&Caller);
	const std::vector<insimul::FMechanicModuleReport> Reports = Surface.Probe(Hosts);

	Check(Surface.BridgeAnswered(), "it answers core.methods");
	const std::size_t Expected = sizeof(EXPECTED_METHODS) / sizeof(EXPECTED_METHODS[0]);
	Check(Surface.Methods().size() == Expected,
		"it offers exactly the adopted surface this repo knows about");
	for (std::size_t Index = 0; Index < Expected; ++Index) {
		bool bFound = false;
		for (const std::string& Method : Surface.Methods()) {
			bFound = bFound || Method == EXPECTED_METHODS[Index];
		}
		Check(bFound, std::string("the bridge carries ") + EXPECTED_METHODS[Index]);
	}
	for (const std::string& Method : Surface.Methods()) {
		bool bKnown = false;
		for (std::size_t Index = 0; Index < Expected; ++Index) {
			bKnown = bKnown || Method == EXPECTED_METHODS[Index];
		}
		Check(bKnown,
			"the bridge offers '" + Method + "', which this repo did not know about — if it is a "
			"mechanic row, RUNTIME_CORE_ADOPTION.md §12 and the host half both need re-checking");
	}

	int NoRow = 0;
	for (const insimul::FMechanicModuleReport& Report : Reports) {
		if (Report.State == insimul::EMechanicState::BridgeHasNoRow) {
			NoRow++;
		}
		std::printf("  - %s\n", Report.Message.c_str());
	}
	Check(NoRow == 7,
		"MEASURED: every band-120 module is unreachable in the artifact that ships, because the "
		"bridge carries no mechanic rows (RUNTIME_CORE_ADOPTION.md §12.1)");
}
#endif

} // namespace

int main(int argc, char** argv) {
	// Unbuffered: a check that crashes must still have printed everything before it.
	std::setvbuf(stdout, nullptr, _IONBF, 0);

	bool bBridge = false;
	for (int Index = 1; Index < argc; ++Index) {
		if (std::strcmp(argv[Index], "--bridge") == 0) {
			bBridge = true;
		}
	}

	std::printf("band-120 mechanic host gate (%s)\n", bBridge ? "real libinsimulcore" : "portable, no native library");

	TestTheModuleTable();
	TestTheVocabularies();
	TestTheFallbacks();
	TestAWiredHostIsNotOverridden();
	TestTheSurface();

	if (bBridge) {
#if INSIMUL_HAVE_CORE_BRIDGE
		insimul::FInsimulCoreBridge Bridge;
		TestTheRealBridge(Bridge);
#else
		std::printf("FAILED: --bridge was asked for and this binary was built without libinsimulcore\n");
		return 1;
#endif
	}

	if (Failures > 0) {
		std::printf("FAILED: %d of %d check(s)\n", Failures, Checks);
		return 1;
	}
	std::printf("OK: %d check(s)\n", Checks);
	return 0;
}
