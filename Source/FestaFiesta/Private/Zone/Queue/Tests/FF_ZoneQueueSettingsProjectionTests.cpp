#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"
#include "FF_FFZSQueueGeometry.h"
#include "Queue/FF_FacilityQueue.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ZoneQueueSettingsProjectionTest,
	"FestaFiesta.Queue.Settings.WidthProtectsPartyMembers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_ZoneQueueSettingsProjectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	for (const float WidthCm : {100.f, 600.f})
	{
		FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec();
		Spec.QueueLayout = EFFZSQueueLayout::Right;
		Spec.QueueSettings.bEnabled = true;
		Spec.QueueSettings.MaxWaitingTeams = 4;
		Spec.QueueSettings.SlotSpacingCm = 160.f;
		Spec.QueueSettings.LaneWidthCm = WidthCm;
		FText Error;
		if (!TestTrue(*FString::Printf(TEXT("%.0fcm lane generates: %s"), WidthCm, *Error.ToString()),
			FFFZSQueueGeometry::ApplySelectedLayout(Spec, Error))) return false;

		const FTransform Transform(FRotator(0, 37, 0), FVector(1200, 1800, 0));
		FFF_QueueLaneGeometry Lane;
		if (!TestTrue(TEXT("Configured queue projects into Party geometry"),
			UFF_ZoneSystemQueueBridgeSubsystem::BuildProjectQueueLaneGeometry(Spec, Transform, Lane))) return false;
		TestEqual(TEXT("Projected slot count matches the user's team capacity"), Lane.ExplicitSlotTransforms.Num(), 4);
		TestEqual(TEXT("Companion offset preserves the complete safety radius"), Lane.MaxMemberLateralOffsetCm,
			FMath::Clamp(WidthCm * .5f - FMath::Max(FUnitData().Radius, Lane.MemberPlacementClearanceCm), 0.f, 270.f));

		FFF_FacilityQueue Queue;
		Queue.Configure(1, UFF_ZoneSystemQueueBridgeSubsystem::ToProjectQueueConfig(Spec.QueueRules, FVector::ZeroVector), Lane);
		FFF_JoinRequest Request;
		Request.PartyId = 10;
		Request.MemberCount = 4;
		Request.MemberAgentIds = {100, 101, 102, 103};
		int32 Slot = INDEX_NONE;
		float Wait = 0.f;
		const EFF_QueueJoinResult Joined = Queue.TryJoin(Request, 0.f, Slot, Wait);
		if (WidthCm == 100.f)
		{
			TestEqual(TEXT("A narrow lane rejects a four-person party rather than spilling beyond its reservation"),
				Joined, EFF_QueueJoinResult::RejectedFull);
			Request.MemberCount = 1;
			Request.MemberAgentIds = {100};
			TestEqual(TEXT("The same narrow lane still accepts a solo visitor"),
				Queue.TryJoin(Request, 0.f, Slot, Wait), EFF_QueueJoinResult::Accepted);
		}
		else
		{
			TestEqual(TEXT("The common 600cm lane accommodates a full four-person party"), Joined, EFF_QueueJoinResult::Accepted);
		}

		TArray<FFF_SlotAssignment> Assignments;
		Queue.BuildSlotAssignments(Assignments);
		TestEqual(TEXT("Every accepted member has its own assignment"), Assignments.Num(), WidthCm == 100.f ? 1 : 4);
		for (const FFF_SlotAssignment& Assignment : Assignments)
		{
			const FVector Local = Transform.InverseTransformPosition(Assignment.TargetLocation);
			for (const float X : {-FUnitData().Radius, FUnitData().Radius})
			{
				for (const float Y : {-FUnitData().Radius, FUnitData().Radius})
				{
					TestTrue(TEXT("A companion's entire Crowd body remains in reserved queue space"),
						FFFZSQueueGeometry::IsLocalPointInsideReservation(Local + FVector(X, Y, 0),
							Spec.FootprintSizeCells, Spec.QueueReservedRects));
				}
			}
		}
	}

	FFFZSFacilitySpec Legacy = FFZSTest::MakeValidFacilitySpec();
	Legacy.QueueSettings.bEnabled = false;
	FFF_QueueLaneGeometry LegacyLane;
	TestTrue(TEXT("Legacy authored geometry remains projectable"),
		UFF_ZoneSystemQueueBridgeSubsystem::BuildProjectQueueLaneGeometry(Legacy, FTransform::Identity, LegacyLane));
	TestEqual(TEXT("Legacy saves retain the established party formation behavior"), LegacyLane.MaxMemberLateralOffsetCm, 270.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ZoneQueueMinimumSpacingTest,
	"FestaFiesta.Queue.Settings.MinimumSpacingAcceptsFullCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_ZoneQueueMinimumSpacingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec();
	Spec.QueueLayout = EFFZSQueueLayout::Authored;
	Spec.QueueSettings.bEnabled = true;
	Spec.QueueSettings.MaxWaitingTeams = 12;
	Spec.QueueSettings.SlotSpacingCm = 90.f;
	Spec.QueueSettings.LaneWidthCm = 600.f;
	FText Error;
	if (!TestTrue(TEXT("The minimum supported spacing generates valid queue geometry"),
		FFFZSQueueGeometry::ApplySelectedLayout(Spec, Error))) return false;
	FFF_QueueLaneGeometry Lane;
	if (!TestTrue(TEXT("The minimum-spacing lane projects into the live Party Queue"),
		UFF_ZoneSystemQueueBridgeSubsystem::BuildProjectQueueLaneGeometry(Spec, FTransform::Identity, Lane))) return false;
	TestEqual(TEXT("The minimum-spacing lane retains all twelve configured slots"), Lane.ExplicitSlotTransforms.Num(), 12);
	FFF_FacilityQueue Queue;
	Queue.Configure(1, UFF_ZoneSystemQueueBridgeSubsystem::ToProjectQueueConfig(Spec.QueueRules, FVector::ZeroVector), Lane);
	for (int32 PartyIndex = 0; PartyIndex < 12; ++PartyIndex)
	{
		FFF_JoinRequest Request;
		Request.PartyId = PartyIndex + 100;
		Request.MemberCount = 1;
		Request.MemberAgentIds = {PartyIndex + 1000};
		int32 Slot = INDEX_NONE;
		float Wait = 0.f;
		TestEqual(*FString::Printf(TEXT("Solo party %d can use every current and future slot at 90cm spacing"), PartyIndex + 1),
			Queue.TryJoin(Request, 0.f, Slot, Wait), EFF_QueueJoinResult::Accepted);
		TestEqual(TEXT("Numeric capacity maps to real sequential team slots"), Slot, PartyIndex);
	}
	TestEqual(TEXT("Minimum spacing still supports the entire configured capacity"), Queue.GetWaitingPartyCount(), 12);
	TestFalse(TEXT("A filled minimum-spacing queue stops admission"), Queue.IsAcceptingNew());
	return true;
}

#endif
