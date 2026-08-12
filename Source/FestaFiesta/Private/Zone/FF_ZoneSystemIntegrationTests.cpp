// Copyright Epic Games, Inc. All Rights Reserved.

#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"
#include "Zone/FF_ZoneSystemStageBridgeSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneSystemStageMappingTest,
	"FestaFiesta.ZoneIntegration.Stage.Mapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneSystemStageMappingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	using FStageBridge = UFF_ZoneSystemStageBridgeSubsystem;
	TestEqual(TEXT("Setup은 최초 배치 단계다"),
		FStageBridge::ToZoneChangePhase(EFF_FestivalStage::Setup),
		EFFZSChangePhase::InitialSetup);
	TestEqual(TEXT("MorningCheck는 점검 배치 단계다"),
		FStageBridge::ToZoneChangePhase(EFF_FestivalStage::MorningCheck),
		EFFZSChangePhase::MorningCheck);
	TestEqual(TEXT("PromotionPreview는 배치를 잠근다"),
		FStageBridge::ToZoneChangePhase(EFF_FestivalStage::PromotionPreview),
		EFFZSChangePhase::LiveOperation);
	TestEqual(TEXT("Live는 라이브 운영 단계다"),
		FStageBridge::ToZoneChangePhase(EFF_FestivalStage::Live),
		EFFZSChangePhase::LiveOperation);
	TestEqual(TEXT("Closing은 정산 단계다"),
		FStageBridge::ToZoneChangePhase(EFF_FestivalStage::Closing),
		EFFZSChangePhase::EveningSettlement);
	TestEqual(TEXT("Report는 정산 단계다"),
		FStageBridge::ToZoneChangePhase(EFF_FestivalStage::Report),
		EFFZSChangePhase::EveningSettlement);
	TestEqual(TEXT("Finale는 정산 단계다"),
		FStageBridge::ToZoneChangePhase(EFF_FestivalStage::Finale),
		EFFZSChangePhase::EveningSettlement);

	TestTrue(TEXT("Stage 브리지는 FFZS Policy 구현이다"),
		UFF_ZoneSystemStageBridgeSubsystem::StaticClass()->ImplementsInterface(
			UFFZSExternalPolicy::StaticClass()));
	TestTrue(TEXT("Queue 브리지는 FFZS Policy 구현이다"),
		UFF_ZoneSystemQueueBridgeSubsystem::StaticClass()->ImplementsInterface(
			UFFZSExternalPolicy::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneSystemQueueProjectionTest,
	"FestaFiesta.ZoneIntegration.Queue.Projection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneSystemQueueProjectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FFFZSQueueRules Rules;
	Rules.MaxWaitingAgents = 12;
	Rules.ServiceCapacity = 4;
	Rules.ServiceDurationSeconds = 8.0f;
	Rules.AbandonAfterSeconds = 30.0f;
	const FVector ExitLocation(100.0, 200.0, 300.0);
	const FFF_QueueConfig Config =
		UFF_ZoneSystemQueueBridgeSubsystem::ToProjectQueueConfig(Rules, ExitLocation);

	TestEqual(TEXT("물리 waiting 슬롯 수를 넘겨 배정하지 않는다"), Config.MaxCapacityMembers, 12);
	TestEqual(TEXT("Legacy 선두 Party 모델은 단일 처리 슬롯을 쓴다"), Config.ConcurrentServiceSlots, 1);
	TestEqual(TEXT("4개 창구 처리율을 2초/인원으로 환산한다"), Config.ServiceSecondsPerMember, 2.0f);
	TestEqual(TEXT("Legacy 호환 처리율은 ServiceCapacity / Duration을 보존한다"),
		static_cast<float>(Config.ConcurrentServiceSlots) / Config.ServiceSecondsPerMember,
		0.5f,
		0.001f);
	TestEqual(TEXT("Exit Anchor 위치를 전달한다"), Config.ExitLocation, ExitLocation);
	TestFalse(TEXT("도착 통지 연결 전에는 서비스 정지를 만들지 않는다"), Config.bRequireArrivalConfirmation);

	using FQueueBridge = UFF_ZoneSystemQueueBridgeSubsystem;
	TestEqual(TEXT("시설 포기 제한이 Party 제한보다 빠르면 시설 값을 쓴다"),
		FQueueBridge::ToProjectMaxWaitSeconds(60.0f, 30.0f), 30.0f);
	TestEqual(TEXT("Party 제한이 더 빠르면 Party 값을 보존한다"),
		FQueueBridge::ToProjectMaxWaitSeconds(10.0f, 30.0f), 10.0f);
	TestEqual(TEXT("무제한 Party도 시설 제한을 따른다"),
		FQueueBridge::ToProjectMaxWaitSeconds(-1.0f, 30.0f), 30.0f);
	TestEqual(TEXT("시설 제한 0은 자동 포기를 끈다"),
		FQueueBridge::ToProjectMaxWaitSeconds(600.0f, 0.0f), -1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneSystemQueueExplicitGeometryProjectionTest,
	"FestaFiesta.ZoneIntegration.Queue.ExplicitGeometryProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneSystemQueueExplicitGeometryProjectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FFFZSFacilitySpec Spec;
	Spec.QueueRules.MaxWaitingAgents = 4;
	Spec.QueueRules.SlotSpacingCm = 100.0f;
	Spec.QueueSplinePointsCm = {
		FVector(0.0, 0.0, 0.0),
		FVector(100.0, 0.0, 0.0),
		FVector(100.0, 200.0, 0.0)
	};
	const FTransform FacilityTransform(
		FRotator(0.0f, 90.0f, 0.0f),
		FVector(1000.0, 2000.0, 50.0));

	FFF_QueueLaneGeometry Lane;
	TestTrue(TEXT("A valid frozen facility spec projects into the Party Queue lane"),
		UFF_ZoneSystemQueueBridgeSubsystem::BuildProjectQueueLaneGeometry(
			Spec, FacilityTransform, Lane));
	TestEqual(TEXT("Every authored polyline point is preserved"),
		Lane.PolylinePoints.Num(), Spec.QueueSplinePointsCm.Num());
	TestEqual(TEXT("Exactly MaxWaitingAgents explicit slots are generated"),
		Lane.ExplicitSlotTransforms.Num(), Spec.QueueRules.MaxWaitingAgents);

	const TArray<FVector> ExpectedLocalSlots = {
		FVector(0.0, 0.0, 0.0),
		FVector(100.0, 0.0, 0.0),
		FVector(100.0, 100.0, 0.0),
		FVector(100.0, 200.0, 0.0)
	};
	for (int32 SlotIndex = 0;
		SlotIndex < FMath::Min(ExpectedLocalSlots.Num(), Lane.ExplicitSlotTransforms.Num());
		++SlotIndex)
	{
		TestTrue(*FString::Printf(TEXT("Slot %d follows the full transformed polyline"), SlotIndex),
			Lane.ExplicitSlotTransforms[SlotIndex].GetLocation().Equals(
				FacilityTransform.TransformPosition(ExpectedLocalSlots[SlotIndex]),
				UE_KINDA_SMALL_NUMBER));
	}
	if (Lane.ExplicitSlotTransforms.Num() == 4)
	{
		TestEqual(TEXT("The bend gives later slots a distinct world-space facing"),
			Lane.ExplicitSlotTransforms[2].Rotator().Yaw, 0.0, 0.01);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneSystemQueueShortGeometryFailsClosedTest,
	"FestaFiesta.ZoneIntegration.Queue.ShortGeometryFailsClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneSystemQueueShortGeometryFailsClosedTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FFFZSFacilitySpec Spec;
	Spec.QueueRules.MaxWaitingAgents = 4;
	Spec.QueueRules.SlotSpacingCm = 100.0f;
	Spec.QueueSplinePointsCm = { FVector::ZeroVector, FVector(200.0, 0.0, 0.0) };

	FFF_QueueLaneGeometry Lane;
	TestFalse(TEXT("A path too short for all physical waiting slots is rejected"),
		UFF_ZoneSystemQueueBridgeSubsystem::BuildProjectQueueLaneGeometry(
			Spec, FTransform::Identity, Lane));
	TestTrue(TEXT("Failed projection leaves no partial explicit geometry"),
		Lane.ExplicitSlotTransforms.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneSystemSingleWaitingSlotDegenerateGeometryTest,
	"FestaFiesta.ZoneIntegration.Queue.SingleWaitingSlotDegenerateGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneSystemSingleWaitingSlotDegenerateGeometryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FFFZSFacilitySpec Spec;
	Spec.QueueRules.MaxWaitingAgents = 1;
	Spec.QueueRules.SlotSpacingCm = 100.0f;
	Spec.QueueSplinePointsCm = { FVector(25.0, 50.0, 0.0), FVector(25.0, 50.0, 0.0) };

	FFFZSAnchorDefinition Service;
	Service.Type = EFFZSAnchorType::Service;
	Service.Index = 0;
	Service.LocalTransform.SetLocation(FVector(25.0, 0.0, 0.0));
	Spec.Anchors.Add(Service);

	FFF_QueueLaneGeometry Lane;
	TestTrue(TEXT("A valid one-slot queue does not require a non-zero spline tangent"),
		UFF_ZoneSystemQueueBridgeSubsystem::BuildProjectQueueLaneGeometry(
			Spec, FTransform::Identity, Lane));
	TestEqual(TEXT("Exactly one explicit waiting slot is projected"),
		Lane.ExplicitSlotTransforms.Num(), 1);
	if (Lane.ExplicitSlotTransforms.Num() == 1)
	{
		TestTrue(TEXT("The single slot preserves the authored position"),
			Lane.ExplicitSlotTransforms[0].GetLocation().Equals(
				Spec.QueueSplinePointsCm[0], UE_KINDA_SMALL_NUMBER));
		TestEqual(TEXT("Facing is derived deterministically from Service[0]"),
			Lane.ExplicitSlotTransforms[0].Rotator().Yaw, -90.0, 0.01);
	}
	return true;
}

#endif
