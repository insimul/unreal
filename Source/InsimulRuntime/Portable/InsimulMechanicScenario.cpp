// Copyright 2024 Insimul. All Rights Reserved.
//
// See InsimulMechanicScenario.h.

#include "InsimulMechanicScenario.h"

#include "InsimulJson.h"
#include "InsimulModuleActivation.h"

namespace insimul {

namespace {

std::vector<std::string> ReadStringArray(const FJsonValue& Owner, const std::string& Key)
{
	std::vector<std::string> Out;
	const FJsonValue* Arr = Owner.Find(Key);
	if (Arr == nullptr || !Arr->IsArray())
	{
		return Out;
	}
	for (const FJsonValuePtr& Item : Arr->ArrayItems)
	{
		if (Item && Item->IsString())
		{
			Out.push_back(Item->StringValue);
		}
	}
	return Out;
}

FInsimulScenarioHostFact ReadHostFact(const FJsonValue& Value)
{
	FInsimulScenarioHostFact Fact;
	Fact.Fact = Value.GetString("fact");
	Fact.From = Value.GetString("from");
	Fact.Probe = Value.GetString("probe");
	Fact.Note = Value.GetString("note");
	const FJsonValue* Args = Value.Find("args");
	if (Args != nullptr && Args->IsObject())
	{
		for (const auto& Entry : Args->ObjectItems)
		{
			if (Entry.second)
			{
				Fact.Args.emplace_back(Entry.first, Entry.second->AsString());
			}
		}
	}
	return Fact;
}

} // namespace

std::string FInsimulScenarioHostFact::Arg(const std::string& Key) const
{
	for (const auto& Pair : Args)
	{
		if (Pair.first == Key)
		{
			return Pair.second;
		}
	}
	return std::string();
}

std::vector<std::string> FInsimulMechanicScenario::Mechanics() const
{
	std::vector<std::string> Out;
	for (const FInsimulScenarioStep& Step : Steps)
	{
		if (Step.bInactive || Step.Mechanic.empty())
		{
			continue;
		}
		bool bSeen = false;
		for (const std::string& Have : Out)
		{
			if (Have == Step.Mechanic)
			{
				bSeen = true;
				break;
			}
		}
		if (!bSeen)
		{
			Out.push_back(Step.Mechanic);
		}
	}
	return Out;
}

bool FInsimulMechanicScenario::Parse(
	const std::string& Json, FInsimulMechanicScenario& OutScenario, std::string& OutError)
{
	OutScenario = FInsimulMechanicScenario();
	OutError.clear();

	const FJsonParseResult Parsed = ParseJson(Json);
	if (!Parsed.bOk || !Parsed.Root)
	{
		OutError = "the scenario is not JSON: " + Parsed.Error;
		return false;
	}
	if (!Parsed.Root->IsObject())
	{
		OutError = "the scenario is not a JSON object";
		return false;
	}

	OutScenario.Id = Parsed.Root->GetString("id");
	OutScenario.Genre = Parsed.Root->GetString("genre");
	OutScenario.Description = Parsed.Root->GetString("description");
	OutScenario.World = ReadStringArray(*Parsed.Root, "world");

	const FJsonValue* Steps = Parsed.Root->Find("steps");
	if (Steps == nullptr || !Steps->IsArray())
	{
		OutError = "the scenario declares no 'steps' array";
		return false;
	}
	for (const FJsonValuePtr& Item : Steps->ArrayItems)
	{
		if (!Item || !Item->IsObject())
		{
			continue;
		}
		FInsimulScenarioStep Step;
		Step.Name = Item->GetString("name");
		Step.Mechanic = Item->GetString("mechanic");
		Step.bInactive = Item->GetBool("inactive");
		Step.Retract = ReadStringArray(*Item, "retract");
		Step.Facts = ReadStringArray(*Item, "facts");
		Step.Goal = Item->GetString("goal");
		Step.Expect = Item->GetString("expect");
		Step.Then = Item->GetString("then");

		const FJsonValue* HostFacts = Item->Find("hostFacts");
		if (HostFacts != nullptr && HostFacts->IsArray())
		{
			for (const FJsonValuePtr& HF : HostFacts->ArrayItems)
			{
				if (HF && HF->IsObject())
				{
					Step.HostFacts.push_back(ReadHostFact(*HF));
				}
			}
		}

		if (Step.Goal.empty())
		{
			OutError = "step '" + Step.Name + "' asks no goal";
			return false;
		}
		// "succeeds" or "fails" and nothing else — a typo here would silently become
		// "expected to fail", which is the answer most goals give.
		if (Step.Expect != "succeeds" && Step.Expect != "fails")
		{
			OutError = "step '" + Step.Name + "' expects '" + Step.Expect + "', which is neither 'succeeds' nor 'fails'";
			return false;
		}
		OutScenario.Steps.push_back(Step);
	}

	if (OutScenario.Steps.empty())
	{
		OutError = "the scenario has no steps";
		return false;
	}
	return true;
}

// ── the report ───────────────────────────────────────────────────────────────

bool FInsimulScenarioReport::IsOk() const
{
	if (!SetupErrors.empty())
	{
		return false;
	}
	if (Steps.empty())
	{
		return false;
	}
	for (const FInsimulScenarioStepResult& Result : Steps)
	{
		if (Result.Outcome != EInsimulScenarioOutcome::Matched)
		{
			return false;
		}
	}
	return true;
}

std::vector<std::string> FInsimulScenarioReport::MechanicsExercised() const
{
	std::vector<std::string> Out;
	for (const FInsimulScenarioStepResult& Result : Steps)
	{
		if (Result.Outcome != EInsimulScenarioOutcome::Matched)
		{
			continue;
		}
		if (Result.Step.bInactive || Result.Step.Mechanic.empty())
		{
			continue;
		}
		bool bSeen = false;
		for (const std::string& Have : Out)
		{
			if (Have == Result.Step.Mechanic)
			{
				bSeen = true;
				break;
			}
		}
		if (!bSeen)
		{
			Out.push_back(Result.Step.Mechanic);
		}
	}
	return Out;
}

std::string FInsimulScenarioReport::Describe() const
{
	std::string Out = "scenario '" + Scenario.Id + "' (genre '" + Scenario.Genre + "'): ";
	Out += std::to_string(Steps.size()) + " step(s), mechanics exercised [" + JoinNames(MechanicsExercised()) + "]";
	for (const std::string& Error : SetupErrors)
	{
		Out += "\n  SETUP  " + Error;
	}
	for (const FInsimulScenarioStepResult& Result : Steps)
	{
		Out += "\n  ";
		switch (Result.Outcome)
		{
		case EInsimulScenarioOutcome::Matched:     Out += "ok     "; break;
		case EInsimulScenarioOutcome::Mismatched:  Out += "WRONG  "; break;
		case EInsimulScenarioOutcome::Raised:      Out += "RAISED "; break;
		case EInsimulScenarioOutcome::SetupFailed: Out += "SETUP  "; break;
		case EInsimulScenarioOutcome::HostSilent:  Out += "SILENT "; break;
		}
		Out += "[" + Result.Step.Mechanic + "] " + Result.Step.Name;
		if (Result.Outcome == EInsimulScenarioOutcome::Matched)
		{
			Out += " → " + Result.Step.Then;
		}
		else if (!Result.Detail.empty())
		{
			Out += " — " + Result.Detail;
		}
		else
		{
			Out += " — " + Result.Step.Goal + " " + (Result.bHolds ? "held" : "did not hold") +
				", the step expects it to " + Result.Step.Expect;
		}
	}
	return Out;
}

// ── the runner ───────────────────────────────────────────────────────────────

FInsimulScenarioReport RunScenario(
	const FInsimulMechanicScenario& Scenario,
	IInsimulScenarioKb& Kb,
	IInsimulScenarioHostSupplier* HostSupplier)
{
	FInsimulScenarioReport Report;
	Report.Scenario = Scenario;

	for (const std::string& Fact : Scenario.World)
	{
		std::string Error;
		if (!Kb.Assert(Fact, Error))
		{
			Report.SetupErrors.push_back("world fact '" + Fact + "' was refused: " + Error);
		}
	}
	if (!Report.SetupErrors.empty())
	{
		return Report;
	}

	for (const FInsimulScenarioStep& Step : Scenario.Steps)
	{
		FInsimulScenarioStepResult Result;
		Result.Step = Step;
		Result.bUsedRecordedHostFacts = HostSupplier == nullptr;

		bool bSetupOk = true;
		for (const std::string& Clause : Step.Retract)
		{
			std::string Error;
			if (Kb.Retract(Clause, Error))
			{
				continue;
			}
			Result.Outcome = EInsimulScenarioOutcome::SetupFailed;
			Result.Detail = "retract '" + Clause + "': " + Error;
			bSetupOk = false;
			break;
		}

		if (bSetupOk)
		{
			for (const FInsimulScenarioHostFact& Declared : Step.HostFacts)
			{
				std::string Clause = Declared.Fact;
				if (HostSupplier != nullptr && !HostSupplier->Supply(Declared, Clause))
				{
					Result.Outcome = EInsimulScenarioOutcome::HostSilent;
					Result.Detail = Declared.From + " supplied no reading for '" + Declared.Fact + "'";
					bSetupOk = false;
					break;
				}
				if (Clause.empty())
				{
					// Measured, and it does not hold. Assert nothing and ask anyway.
					Result.HostFactsNotHolding.push_back(Declared.Fact);
					continue;
				}
				std::string Error;
				if (!Kb.Assert(Clause, Error))
				{
					Result.Outcome = EInsimulScenarioOutcome::SetupFailed;
					Result.Detail = "host fact '" + Clause + "' from " + Declared.From + " was refused: " + Error;
					bSetupOk = false;
					break;
				}
				Result.HostFactsUsed.push_back(Clause);
			}
		}

		if (bSetupOk)
		{
			for (const std::string& Fact : Step.Facts)
			{
				std::string Error;
				if (Kb.Assert(Fact, Error))
				{
					continue;
				}
				Result.Outcome = EInsimulScenarioOutcome::SetupFailed;
				Result.Detail = "fact '" + Fact + "' was refused: " + Error;
				bSetupOk = false;
				break;
			}
		}

		if (!bSetupOk)
		{
			Report.Steps.push_back(Result);
			continue;
		}

		bool bHolds = false;
		std::string QueryError;
		if (!Kb.Ask(Step.Goal, bHolds, QueryError))
		{
			Result.Outcome = EInsimulScenarioOutcome::Raised;
			Result.Detail = QueryError;
			Report.Steps.push_back(Result);
			continue;
		}
		Result.bHolds = bHolds;
		Result.Outcome = bHolds == Step.ExpectsSuccess()
			? EInsimulScenarioOutcome::Matched
			: EInsimulScenarioOutcome::Mismatched;
		Report.Steps.push_back(Result);
	}

	return Report;
}

} // namespace insimul
