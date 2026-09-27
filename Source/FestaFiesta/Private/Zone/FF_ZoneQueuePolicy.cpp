// FF_ZoneQueuePolicy.cpp
// 역할: 사람이 이용 중인 시설의 이동·삭제·강화를 막고, 강화 대기열을 안전하게 확정·복구한다.
// 흐름: PreValidate(검사) → ApplyTentative(임시 변경) → Commit(확정) / Rollback(원상 복구).
// 이 파일은 같은 브리지 클래스의 구현이다. Blueprint 이름과 외부 호출 방법은 그대로 유지한다.

#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSQueueComponent.h"
#include "FF_FFZSWorldSubsystem.h"
#include "Queue/FF_QueueSubsystem.h"

#define LOCTEXT_NAMESPACE "FFZoneSystemQueueBridge"

FName UFF_ZoneSystemQueueBridgeSubsystem::GetFFZoneSystemPolicyName_Implementation() const
{
	return TEXT("FestaFiesta.QueueSubsystem");
}

EFFZSPolicyDomain UFF_ZoneSystemQueueBridgeSubsystem::GetFFZoneSystemPolicyDomain_Implementation() const
{
	// Queue 전용 기본 Domain이 없으므로 생산 6개 Domain과 충돌하지 않는 Custom을 사용한다.
	return EFFZSPolicyDomain::Custom;
}

FFFZSPolicyDecision UFF_ZoneSystemQueueBridgeSubsystem::PreValidateFFZoneSystemChange_Implementation(
	const FFFZSPolicyContext& Context) const
{
	FFFZSPolicyDecision Decision;
	// 동선 변경과 강화 재구성은 두 Queue 모두 완전히 비어 있어야 한다.
	const bool bNeedsEmptyQueue = Context.Command.Type == EFFZSCommandType::Move
		|| Context.Command.Type == EFFZSCommandType::Remove
		|| Context.Command.Type == EFFZSCommandType::Upgrade;
	if (Context.Command.Type == EFFZSCommandType::Upgrade)
	{
		const AFFZSFacilityActor* Facility = ZoneSystem
			? ZoneSystem->FindFacility(Context.Command.FacilityInstanceId) : nullptr;
		const int32 QueueFacilityId = FindQueueFacilityId(Context.Command.FacilityInstanceId);
		if (!Facility || !Facility->GetQueueComponent() || !QueueSystem
			|| !ActiveBridgeQueueFacilityIds.Contains(QueueFacilityId)
			|| !QueueSystem->IsFacilityRegistered(QueueFacilityId))
		{
			Decision.bAllowed = false;
			Decision.FailureReason = EFFZSPlacementFailure::ExternalPolicyRejected;
			Decision.Message = LOCTEXT("UpgradeQueueUnavailable", "시설 대기열 연결을 확인한 뒤 강화할 수 있습니다.");
			return Decision;
		}
		if (Facility->GetQueueComponent()->IsOccupied())
		{
			Decision.bAllowed = false;
			Decision.FailureReason = EFFZSPlacementFailure::FacilityOccupied;
			Decision.Message = LOCTEXT("UpgradeAgentQueueOccupied", "시설의 대기·서비스·퇴장이 모두 끝나야 강화할 수 있습니다.");
			return Decision;
		}
	}
	if (!bNeedsEmptyQueue || !QueueSystem)
	{
		return Decision;
	}

	const int32 QueueFacilityId = FindQueueFacilityId(Context.Command.FacilityInstanceId);
	if (QueueFacilityId != INDEX_NONE
		&& ActiveBridgeQueueFacilityIds.Contains(QueueFacilityId)
		&& QueueSystem->IsFacilityRegistered(QueueFacilityId))
	{
		const FFF_QueueState State = QueueSystem->GetQueueState(QueueFacilityId);
		if (State.OccupiedMemberCount > 0)
		{
			Decision.bAllowed = false;
			Decision.FailureReason = EFFZSPlacementFailure::FacilityOccupied;
			Decision.Message = LOCTEXT(
				"ProjectQueueOccupied",
				"프로젝트 Party 대기열이 비기 전에는 시설을 이동·제거·강화할 수 없습니다.");
		}
	}
	return Decision;
}

FFFZSPolicyDecision UFF_ZoneSystemQueueBridgeSubsystem::ApplyTentativeFFZoneSystemChange_Implementation(
	const FFFZSPolicyContext& Context)
{
	FFFZSPolicyDecision Decision = PreValidateFFZoneSystemChange_Implementation(Context);
	if (!Decision.bAllowed || Context.Command.Type != EFFZSCommandType::Upgrade
		|| PendingUpgradeQueues.Contains(Context.Command.CommandId))
	{
		return Decision;
	}

	const int32 QueueFacilityId = FindQueueFacilityId(Context.Command.FacilityInstanceId);
	FFF_FacilityQueue* Existing = QueueSystem->FindQueue(QueueFacilityId);
	// The Zone authority has already resolved the new footprint, spline, anchors and actor center.
	FFFZSFacilitySpec CandidateSpec = Context.Command.FacilitySpec;
	const FTransform& CandidateTransform = Context.Validation.PlacementTransform;
	CandidateSpec.QueueRules = AFFZSFacilityActor::BuildEffectiveQueueRules(
		CandidateSpec.QueueRules, Context.Command.UpgradeServiceDurationMultiplier);
	const FFFZSAnchorDefinition* ExitAnchor = CandidateSpec.Anchors.FindByPredicate(
		[](const FFFZSAnchorDefinition& Anchor)
		{
			return Anchor.Type == EFFZSAnchorType::Exit && Anchor.Index == 0;
		});
	TArray<FText> Errors;
	FFF_QueueLaneGeometry Lane;
	if (!Existing || !ExitAnchor || !CandidateSpec.IsValid(Errors)
		|| !BuildProjectQueueLaneGeometry(CandidateSpec, CandidateTransform, Lane))
	{
		Decision.bAllowed = false;
		Decision.FailureReason = EFFZSPlacementFailure::DataInvalid;
		Decision.Message = LOCTEXT("UpgradeQueueInvalid", "강화 후 대기열 설정이 유효하지 않습니다.");
		return Decision;
	}

	FPendingUpgradeQueue& Pending = PendingUpgradeQueues.Add(Context.Command.CommandId);
	Pending.QueueFacilityId = QueueFacilityId;
	Pending.PreviousQueue = *Existing;
	Pending.bWasSlotDirty = QueueSystem->DirtySlotFacilities.Contains(QueueFacilityId);
	const FFF_QueueConfig Config = ToProjectQueueConfig(
		CandidateSpec.QueueRules, CandidateTransform.TransformPosition(ExitAnchor->LocalTransform.GetLocation()));
	if (!QueueSystem->RegisterFacility(QueueFacilityId, Config, Lane))
	{
		Decision.bAllowed = false;
		Decision.FailureReason = EFFZSPlacementFailure::DataInvalid;
		Decision.Message = LOCTEXT("UpgradeQueueRejected", "강화 후 대기열을 구성할 수 없습니다.");
		return Decision;
	}
	// 공개 Queue 이벤트는 아직 내보내지 않는다. 기존 큐는 비어 있으므로 취소도 발생하지 않는다.
	QueueSystem->FindQueue(QueueFacilityId)->CloseFacility(
		EFF_QueueCloseReason::Reconfigure, QueueSystem->GetSimTimeSeconds());
	return Decision;
}

void UFF_ZoneSystemQueueBridgeSubsystem::CommitFFZoneSystemChange_Implementation(
	const FFFZSPolicyContext& Context)
{
	if (FPendingUpgradeQueue* Pending = PendingUpgradeQueues.Find(Context.Command.CommandId))
	{
		// 재구성 전 남아 있던 관찰 이벤트/로그도 유실시키지 않는다. 이 단계는 실패하지 않는다.
		Pending->PreviousQueue.ConsumePendingEvents(QueueSystem->PendingEvents);
		Pending->PreviousQueue.ConsumePendingLogs(QueueSystem->QueueLogs);
		PendingUpgradeQueues.Remove(Context.Command.CommandId);
	}
}

void UFF_ZoneSystemQueueBridgeSubsystem::RollbackFFZoneSystemChange_Implementation(
	const FFFZSPolicyContext& Context)
{
	if (FPendingUpgradeQueue* Pending = PendingUpgradeQueues.Find(Context.Command.CommandId))
	{
		if (FFF_FacilityQueue* Existing = QueueSystem->FindQueue(Pending->QueueFacilityId))
		{
			*Existing = MoveTemp(Pending->PreviousQueue);
		}
		if (!Pending->bWasSlotDirty)
		{
			QueueSystem->DirtySlotFacilities.Remove(Pending->QueueFacilityId);
		}
		PendingUpgradeQueues.Remove(Context.Command.CommandId);
	}
}

#undef LOCTEXT_NAMESPACE
