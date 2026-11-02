// FF_FacilitySynergyTests.cpp
// 역할: FacilitySynergy 기능의 동작 계약을 자동 검사한다 (검사 경로: FestaFiesta.Building.Synergy.SharedTablePolicy, FestaFiesta.Building.Synergy.DistanceBoundaryAndInvalidData).

#include "Building/Synergy/FF_FacilitySynergyEvaluator.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Building/FacilityCollection/FF_FacilityModel.h"
#include "Building/Synergy/FF_FacilitySynergyRulePolicy.h"
#include "Engine/DataTable.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	FFF_FacilitySynergyPoint Point(int32 Id, FName Definition, double X, double Y = 0., double Z = 0.)
	{
		return {FGuid(0, 0, 0, Id), Definition, FVector(X, Y, Z)};
	}
	FFF_FacilitySynergyRule Rule(FName Id, FName Target, FName Partner, float Bonus = 10.f, FName Group = NAME_None)
	{
		FFF_FacilitySynergyRule Result;
		Result.Id = Id;
		Result.Row.TargetFacilityId = Target;
		Result.Row.PartnerFacilityId = Partner;
		Result.Row.RevenueBonusPercent = Bonus;
		Result.Row.MaxDistanceMeters = 15.f;
		Result.Row.StackingGroup = Group;
		return Result;
	}
}

// 데이터표 검증부터 실제 수익과 도감 표시까지 같은 조합 순서와 결과를 사용하는지 검증한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SynergySharedTablePolicyTest,
	"FestaFiesta.Building.Synergy.SharedTablePolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_SynergySharedTablePolicyTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UDataTable> Table(NewObject<UDataTable>());
	Table->RowStruct = FFF_FacilitySynergyRow::StaticStruct();
	Table->AddRow(TEXT("ValidZ"), Rule(TEXT("ValidZ"), TEXT("Food"), TEXT("Rest"), 20.f).Row);
	Table->AddRow(TEXT("ValidA"), Rule(TEXT("ValidA"), TEXT("Food"), TEXT("Rest"), 10.f).Row);
	FFF_FacilitySynergyRow Disabled = Rule(TEXT("Disabled"), TEXT("Food"), TEXT("Rest"), 99.f).Row;
	Disabled.bEnabled = false;
	Table->AddRow(TEXT("Disabled"), Disabled);
	Table->AddRow(TEXT("Invalid"), Rule(TEXT("Invalid"), TEXT("Food"), NAME_None).Row);
	TArray<FString> Warnings;
	const TArray<FFF_FacilitySynergyRule> Rules = FFFacilitySynergyRulePolicy::ReadTable(Table.Get(), Warnings);
	TestEqual(TEXT("Only malformed enabled rows produce a diagnostic"), Warnings.Num(), 1);
	if (!TestEqual(TEXT("Only valid enabled rows enter both models"), Rules.Num(), 2)) { return false; }
	TestEqual(TEXT("The same stable first row is used by every consumer"), Rules[0].Id, FName(TEXT("ValidA")));
	FFFZSReadModel World;
	for (const auto& PointValue : {Point(1, TEXT("Food"), 0.), Point(2, TEXT("Rest"), 100.)})
	{
		FFFZSFacilityReadModel& Facility = World.Facilities.AddDefaulted_GetRef();
		Facility.FacilityInstanceId = PointValue.InstanceId;
		Facility.FacilityDefinitionId = PointValue.DefinitionId;
		Facility.WorldLocation = PointValue.WorldLocation;
		Facility.State = EFFZSFacilityState::Closed;
		TestTrue(TEXT("Closed installations retain their combination eligibility"), FFFacilitySynergyRulePolicy::IsEligibleFacility(Facility));
	}
	FFF_FacilityCodexModel Codex;
	Codex.SetRules(Rules, 50.f);
	Codex.Evaluate(World, {});
	FFF_FacilitySynergyEvaluator Evaluator;
	Evaluator.SetRules(Rules, 50.f);
	Evaluator.SetFacilities({Point(1, TEXT("Food"), 0.), Point(2, TEXT("Rest"), 100.)});
	const auto Evaluation = Evaluator.Evaluate(Point(1, TEXT("Food"), 0.));
	TestEqual(TEXT("Income keeps both authored bonuses"), Evaluation.TotalPercent, 30.f);
	TestEqual(TEXT("Codex row order stays aligned with income RuleIndex"), Codex.GetEntries()[0].RuleId, Rules[0].Id);
	TestEqual(TEXT("Codex first bonus equals the actual evaluated rule"), Codex.GetEntries()[0].AppliedBonusPercent, Evaluation.Matches[0].AppliedPercent);
	World.Facilities[1].bNeedsRelocation = true;
	TestFalse(TEXT("Relocation excludes the partner from live bonuses"), FFFacilitySynergyRulePolicy::IsEligibleFacility(World.Facilities[1]));
	Codex.Evaluate(World, {});
	TestFalse(TEXT("Codex also stops promising the excluded partner's bonus"), Codex.GetEntries()[0].bEffectApplied);
	TStrongObjectPtr<UDataTable> WrongSchema(NewObject<UDataTable>());
	TestTrue(TEXT("A wrong table schema cannot be reinterpreted as a rule"), FFFacilitySynergyRulePolicy::ReadTable(WrongSchema.Get(), Warnings).IsEmpty());
	TestEqual(TEXT("A wrong table schema explains why it was rejected"), Warnings.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SynergyDistanceBoundaryTest,
	"FestaFiesta.Building.Synergy.DistanceBoundaryAndInvalidData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_SynergyDistanceBoundaryTest::RunTest(const FString& Parameters)
{
	FFF_FacilitySynergyEvaluator Evaluator;
	FFF_FacilitySynergyRule Invalid = Rule(TEXT("Invalid"), TEXT("Food"), NAME_None, 90.f);
	FFF_FacilitySynergyRule Disabled = Rule(TEXT("Disabled"), TEXT("Food"), TEXT("DisabledPartner"), 80.f);
	Disabled.Row.bEnabled = false;
	Evaluator.SetRules({Rule(TEXT("Valid"), TEXT("Food"), TEXT("Rest")), Invalid, Disabled}, 50.f);
	const TArray<FName>& RelatedToFood = Evaluator.GetRelatedFacilityDefinitionIds(TEXT("Food"));
	TestTrue(TEXT("Catalog relations work before world facilities exist and skip invalid/disabled rows"),
		RelatedToFood.Num() == 1 && RelatedToFood[0] == FName(TEXT("Rest")));
	TestTrue(TEXT("Catalog relations include the reverse helper-to-beneficiary direction"),
		Evaluator.GetRelatedFacilityDefinitionIds(TEXT("Rest")).Contains(TEXT("Food")));
	TestTrue(TEXT("An unknown definition has no catalog relation"), Evaluator.GetRelatedFacilityDefinitionIds(TEXT("Unknown")).IsEmpty());
	const FFF_FacilitySynergyPoint Target = Point(1, TEXT("Food"), 0.);
	Evaluator.SetFacilities({Target, Point(2, TEXT("Rest"), 1500., 0., 10000.)});
	TestEqual(TEXT("15m boundary included; height ignored; invalid/disabled rows skipped"), Evaluator.Evaluate(Target).TotalPercent, 10.f);
	Evaluator.SetFacilities({Target, Point(2, TEXT("Rest"), 1500.01)});
	TestEqual(TEXT("Just outside exact horizontal radius"), Evaluator.Evaluate(Target).TotalPercent, 0.f);
	TestEqual(TEXT("Direction is not reciprocal"), Evaluator.Evaluate(Point(2, TEXT("Rest"), 1500.)).TotalPercent, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SynergyStackingTest,
	"FestaFiesta.Building.Synergy.StrongestGroupDuplicatesAndCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_SynergyStackingTest::RunTest(const FString& Parameters)
{
	FFF_FacilitySynergyEvaluator Evaluator;
	Evaluator.SetRules({Rule(TEXT("Weak"), TEXT("Food"), TEXT("Rest"), 10.f, TEXT("Comfort")),
		Rule(TEXT("Strong"), TEXT("Food"), TEXT("Shade"), 20.f, TEXT("Comfort")),
		Rule(TEXT("Independent"), TEXT("Food"), TEXT("Rest"), 15.f)}, 30.f);
	TestEqual(TEXT("Several authored rows for one partner produce one catalog suggestion"),
		Evaluator.GetRelatedFacilityDefinitionIds(TEXT("Food")).Num(), 2);
	const FFF_FacilitySynergyPoint Target = Point(1, TEXT("Food"), 0.);
	Evaluator.SetFacilities({Point(3, TEXT("Rest"), 250.), Point(4, TEXT("Shade"), 500.), Target, Point(2, TEXT("Rest"), 100.)});
	const FFF_FacilitySynergyEvaluation Result = Evaluator.Evaluate(Target);
	TestEqual(TEXT("20 + 15 is capped to 30; duplicated rests do not stack"), Result.TotalPercent, 30.f);
	float AppliedSum = 0.f;
	for (const FFF_FacilitySynergyMatch& Match : Result.Matches) AppliedSum += Match.AppliedPercent;
	TestEqual(TEXT("Preview rule contributions add up to paid benefit"), AppliedSum, Result.TotalPercent);
	Evaluator.SetRules({Rule(TEXT("Independent"), TEXT("Food"), TEXT("Rest"), 15.f),
		Rule(TEXT("Strong"), TEXT("Food"), TEXT("Shade"), 20.f, TEXT("Comfort")),
		Rule(TEXT("Weak"), TEXT("Food"), TEXT("Rest"), 10.f, TEXT("Comfort"))}, 30.f);
	TestEqual(TEXT("Rule order does not affect result"), Evaluator.Evaluate(Target).TotalPercent, 30.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SynergyDiscoveryRewardRowTest,
	"FestaFiesta.Building.Synergy.DiscoveryRewardRowContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_SynergyDiscoveryRewardRowTest::RunTest(const FString& Parameters)
{
	FFF_FacilitySynergyRow Row = Rule(TEXT("Reward"), TEXT("Food"), TEXT("Rest")).Row;
	TestEqual(TEXT("New combinations default to a 100 Gold discovery reward"), Row.DiscoveryRewardGold, 100);
	Row.DiscoveryRewardGold = 0;
	TestTrue(TEXT("A designer may keep discovery enabled without a Gold reward"), Row.IsUsable());
	Row.DiscoveryRewardGold = 275;
	TestTrue(TEXT("A designer-authored integer reward is valid"), Row.IsUsable());
	Row.DiscoveryRewardGold = -1;
	FString Failure;
	TestFalse(TEXT("A negative discovery reward is invalid"), Row.IsUsable(&Failure));
	TestFalse(TEXT("Invalid reward explains the validation failure"), Failure.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SynergyPreviewOverlayTest,
	"FestaFiesta.Building.Synergy.MoveAndDuplicatePreviewConsistency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_SynergyPreviewOverlayTest::RunTest(const FString& Parameters)
{
	FFF_FacilitySynergyEvaluator Evaluator;
	Evaluator.SetRules({Rule(TEXT("Comfort"), TEXT("Food"), TEXT("Rest"))}, 50.f);
	const FFF_FacilitySynergyPoint Target = Point(1, TEXT("Food"), 0.);
	const FFF_FacilitySynergyPoint Partner = Point(2, TEXT("Rest"), 100.);
	Evaluator.SetFacilities({Target, Partner});
	FFF_FacilitySynergyPreview Preview;
	Evaluator.BuildPreview(TEXT("Rest"), FVector(200., 0., 0.), FGuid(), Preview);
	TestEqual(TEXT("Duplicate supporting installation shows existing target"), Preview.Entries.Num(), 1);
	if (!Preview.Entries.IsEmpty())
	{
		TestEqual(TEXT("Duplicate retains 10 before"), Preview.Entries[0].CurrentTotalBonusPercent, 10.f);
		TestEqual(TEXT("Duplicate does not promise 20 after"), Preview.Entries[0].ProposedTotalBonusPercent, 10.f);
	}
	Evaluator.BuildPreview(TEXT("Rest"), FVector(5000., 0., 0.), Partner.InstanceId, Preview);
	TestEqual(TEXT("Move includes beneficiary at old location"), Preview.Entries.Num(), 1);
	if (!Preview.Entries.IsEmpty())
	{
		TestEqual(TEXT("Old bonus shown"), Preview.Entries[0].CurrentTotalBonusPercent, 10.f);
		TestEqual(TEXT("Old moving instance excluded from proposed bonus"), Preview.Entries[0].ProposedTotalBonusPercent, 0.f);
		Evaluator.SetFacilities({Target, Point(2, TEXT("Rest"), 5000.)});
		TestEqual(TEXT("Committed evaluation equals preview"), Evaluator.Evaluate(Target).TotalPercent, Preview.Entries[0].ProposedTotalBonusPercent);
	}
	// A size-changing upgrade moves the beneficiary's centre even when its grid corner stays put.
	Evaluator.SetFacilities({Target, Point(2, TEXT("Rest"), -1500.)});
	const FFF_FacilitySynergyPoint GrownTarget = Point(1, TEXT("Food"), 100.);
	TestEqual(TEXT("Before resize the beneficiary is exactly in range"), Evaluator.Evaluate(Target).TotalPercent, 10.f);
	const float ProposedTargetBonus = Evaluator.Evaluate(GrownTarget, &GrownTarget, Target.InstanceId).TotalPercent;
	TestEqual(TEXT("Proposed resized centre loses the boundary bonus"), ProposedTargetBonus, 0.f);
	Evaluator.SetFacilities({GrownTarget, Point(2, TEXT("Rest"), -1500.)});
	TestEqual(TEXT("Beneficiary-only proposal agrees with committed resized centre"), Evaluator.Evaluate(GrownTarget).TotalPercent, ProposedTargetBonus);
	Evaluator.SetRules({Rule(TEXT("SameType"), TEXT("Rest"), TEXT("Rest"))}, 50.f);
	Evaluator.SetFacilities({Partner});
	Evaluator.BuildPreview(TEXT("Rest"), FVector(100., 0., 0.), Partner.InstanceId, Preview);
	TestEqual(TEXT("A same-type rule with only the moving self has one missing-partner row"), Preview.Entries.Num(), 1);
	for (const FFF_FacilitySynergyPreviewEntry& Entry : Preview.Entries)
		TestEqual(TEXT("Moving facility cannot count its old position as its own partner"), Entry.ProposedTotalBonusPercent, 0.f);
	return true;
}

#endif
