// FF_ZoneQueueProjection.cpp
// 역할: 시설의 대기열 규칙과 로컬 동선을 프로젝트의 팀 대기열 설정·월드 좌표로 변환한다.
// 수정 지점: 정원/이용 시간은 ToProjectQueueConfig, 줄 위치는 BuildProjectQueueLaneGeometry,
// 포기 시간은 ToProjectMaxWaitSeconds에서 변환한다. 등록·삭제와 실제 입장은 Subsystem이 담당한다.

#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSQueueGeometry.h"

namespace
{
	// slot 위치는 호출자가 이미 계산하므로 같은 sampler에서 해당 거리의 접선만 안전하게 얻는다.
	bool TryGetQueueTailDirectionAtDistance(
		const TArray<FVector>& Points,
		const double DistanceCm,
		FVector& OutTailDirection)
	{
		FVector SlotPoint = FVector::ZeroVector;
		return FFFZSQueueGeometry::TrySampleLinearPolylineAtDistanceWithTangent(
			Points, DistanceCm, SlotPoint, OutTailDirection);
	}
}

FFF_QueueConfig UFF_ZoneSystemQueueBridgeSubsystem::ToProjectQueueConfig(
	const FFFZSQueueRules& Rules,
	const FVector& ExitLocation)
{
	FFF_QueueConfig Config;
	// 저장된 MaxWaitingAgents는 팀별 대기 지점 수다. 이용 중인 팀도 종료까지 한 자리를 유지한다.
	Config.MaxCapacityParties = static_cast<int32>(FMath::Clamp<int64>(
		static_cast<int64>(Rules.MaxWaitingAgents), 1, MAX_int32));

	// 처리 정원은 동시에 이용하는 팀 수, 시간은 팀 전체의 이용 시간이다.
	// 인원이 많은 팀도 같은 시간이 걸리며 실제 이용 인원은 완료 이벤트로 별도 정산한다.
	Config.ConcurrentServiceSlots = FMath::Max(1, Rules.ServiceCapacity);
	Config.ServiceSecondsPerParty = FMath::Max(Rules.ServiceDurationSeconds, KINDA_SMALL_NUMBER);
	Config.ExitLocation = ExitLocation;

	// 실제 일행이 배정된 자리에 도착한 뒤에만 이용 시간을 센다.
	Config.bRequireArrivalConfirmation = true;
	return Config;
}

bool UFF_ZoneSystemQueueBridgeSubsystem::BuildProjectQueueLaneGeometry(
	const FFFZSFacilitySpec& Spec,
	const FTransform& FacilityTransform,
	FFF_QueueLaneGeometry& OutLane)
{
	// 어느 실패 경로에서도 이전 호출의 부분 geometry가 노출되지 않게 출력부터 초기화한다.
	OutLane = FFF_QueueLaneGeometry();
	const FFFZSQueueRules& Rules = Spec.QueueRules;
	if (FacilityTransform.ContainsNaN()
		|| Spec.QueueSplinePointsCm.Num() < 2
		|| Rules.MaxWaitingAgents <= 0
		|| Rules.MaxWaitingAgents > UFFZSDeveloperSettings::MaxQueueWaitingAgents
		|| Spec.QueueSplinePointsCm.Num() > UFFZSDeveloperSettings::MaxQueueSplinePoints
		|| !FMath::IsFinite(Rules.SlotSpacingCm)
		|| Rules.SlotSpacingCm <= 0.0f)
	{
		return false;
	}

	int32 FirstNonFinitePointIndex = INDEX_NONE;
	const double PolylineLengthCm = FFFZSQueueGeometry::GetLinearPolylineLengthCm(
		Spec.QueueSplinePointsCm, &FirstNonFinitePointIndex);
	const double RequiredLengthCm =
		(static_cast<double>(Rules.MaxWaitingAgents) - 1.0) *
		static_cast<double>(Rules.SlotSpacingCm);
	if (FirstNonFinitePointIndex != INDEX_NONE
		|| PolylineLengthCm + UE_KINDA_SMALL_NUMBER < RequiredLengthCm)
	{
		return false;
	}

	TArray<FVector> LocalSlotPoints;
	if (!FFFZSQueueGeometry::BuildLinearQueueSlotPoints(
		Spec.QueueSplinePointsCm,
		Rules.MaxWaitingAgents,
		Rules.SlotSpacingCm,
		LocalSlotPoints)
		|| LocalSlotPoints.Num() != Rules.MaxWaitingAgents)
	{
		return false;
	}

	// 저작 polyline과 파생 slot을 같은 시설 Transform으로 옮겨 두 Queue의 월드 동선을 일치시킨다.
	OutLane.SlotSpacingCm = Rules.SlotSpacingCm;
	OutLane.PolylinePoints.Reserve(Spec.QueueSplinePointsCm.Num());
	for (const FVector& LocalPoint : Spec.QueueSplinePointsCm)
	{
		const FVector WorldPoint = FacilityTransform.TransformPosition(LocalPoint);
		if (WorldPoint.ContainsNaN())
		{
			OutLane = FFF_QueueLaneGeometry();
			return false;
		}
		OutLane.PolylinePoints.Add(WorldPoint);
	}

	OutLane.ExplicitSlotTransforms.Reserve(LocalSlotPoints.Num());
	for (int32 SlotIndex = 0; SlotIndex < LocalSlotPoints.Num(); ++SlotIndex)
	{
		FVector LocalTailDirection = FVector::ZeroVector;
		const double SlotDistanceCm = static_cast<double>(SlotIndex) * Rules.SlotSpacingCm;
		if (!TryGetQueueTailDirectionAtDistance(
			Spec.QueueSplinePointsCm, SlotDistanceCm, LocalTailDirection))
		{
			// Waiting 슬롯이 하나면 필요한 길이는 0이라 완전히 퇴화한 polyline도
			// 유효하다. 위치는 명확하므로 Service[0]→slot 방향을 우선 사용하고,
			// 둘도 같으면 시설 저작 계약의 customer-facing +Y를 결정적 yaw로 쓴다.
			if (Rules.MaxWaitingAgents != 1)
			{
				OutLane = FFF_QueueLaneGeometry();
				return false;
			}

			for (const FFFZSAnchorDefinition& Anchor : Spec.Anchors)
			{
				if (Anchor.Type == EFFZSAnchorType::Service && Anchor.Index == 0)
				{
					LocalTailDirection = LocalSlotPoints[SlotIndex]
						- Anchor.LocalTransform.GetLocation();
					break;
				}
			}
			if (!LocalTailDirection.Normalize())
			{
				LocalTailDirection = FVector::YAxisVector;
			}
		}

		FVector WorldTailDirection = FacilityTransform.TransformVectorNoScale(LocalTailDirection);
		if (!WorldTailDirection.Normalize())
		{
			OutLane = FFF_QueueLaneGeometry();
			return false;
		}
		const FVector WorldLocation = FacilityTransform.TransformPosition(LocalSlotPoints[SlotIndex]);
		// Party는 대기열 머리 방향을 바라보므로 진행 접선의 반대 방향을 slot yaw로 사용한다.
		const float WorldYawDegrees = (-WorldTailDirection).Rotation().Yaw;
		OutLane.ExplicitSlotTransforms.Add(FTransform(
			FRotator(0.0f, WorldYawDegrees, 0.0f), WorldLocation));
	}

	OutLane.HeadLocation = OutLane.ExplicitSlotTransforms[0].GetLocation();
	OutLane.TailDirection = FacilityTransform.TransformVectorNoScale(FVector::ForwardVector);
	if (OutLane.ExplicitSlotTransforms.Num() > 1)
	{
		OutLane.TailDirection =
			OutLane.ExplicitSlotTransforms[1].GetLocation() - OutLane.HeadLocation;
	}
	if (!OutLane.TailDirection.Normalize())
	{
		OutLane.TailDirection = FVector::ForwardVector;
	}
	OutLane.SlotYawDegrees = OutLane.ExplicitSlotTransforms[0].Rotator().Yaw;
	return true;
}

float UFF_ZoneSystemQueueBridgeSubsystem::ToProjectMaxWaitSeconds(
	const float RequestedSeconds,
	const float ZoneAbandonSeconds)
{
	if (ZoneAbandonSeconds <= 0.0f)
	{
		// FFZS의 0은 자동 포기 비활성화다. Legacy Queue의 같은 의미인 음수로 바꾼다.
		return -1.0f;
	}
	return RequestedSeconds < 0.0f
		? ZoneAbandonSeconds
		: FMath::Min(RequestedSeconds, ZoneAbandonSeconds);
}

