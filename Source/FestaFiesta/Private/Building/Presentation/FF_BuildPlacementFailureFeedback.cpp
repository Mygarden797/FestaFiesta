// FF_BuildPlacementFailureFeedback.cpp

// Zone 배치 실패 코드를 사용자용 유형과 제목으로 분류하고 WBP 토스트에 반영한다.
// 흐름: 실패 표시 요청 -> 텍스트 갱신 -> 기존 타이머 교체 -> 만료 시 숨김.
// 불변식: 안정 실패 코드는 하나의 UI 유형으로 결정적으로 매핑되고 최신 실패만 화면에 남는다.
// 실패/수명: 위젯 수명 종료 시 타이머를 제거해 해제된 UI에 지연 콜백이 도달하지 않게 한다.

#include "Building/Presentation/FF_BuildPlacementFailureFeedback.h"

#include "CommonTextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "FFBuildPlacementFailureFeedback"

EFF_BuildPlacementFailureType UFF_BuildPlacementFailureFeedback::ClassifyFailure(
	const EFFZSPlacementFailure FailureReason)
{
	// 디자이너 분기 수를 안정적으로 유지하도록 세부 Zone 실패를 소수의 UI 범주로 축약한다.
	switch (FailureReason)
	{
	case EFFZSPlacementFailure::InsufficientFunds:
		return EFF_BuildPlacementFailureType::InsufficientGold;
	case EFFZSPlacementFailure::DataInvalid:
		return EFF_BuildPlacementFailureType::ConfigurationError;
	case EFFZSPlacementFailure::StaleWorldRevision:
	case EFFZSPlacementFailure::WrongStage:
		return EFF_BuildPlacementFailureType::WorldStateChanged;
	case EFFZSPlacementFailure::InternalError:
		return EFF_BuildPlacementFailureType::Unknown;
	default:
		return EFF_BuildPlacementFailureType::PlacementRejected;
	}
}

FText UFF_BuildPlacementFailureFeedback::MakeFailureTitle(
	const EFFZSPlacementFailure FailureReason,
	const bool bIsMoving)
{
	if (FailureReason == EFFZSPlacementFailure::BuildLimitReached)
	{
		return LOCTEXT("BuildLimitReachedTitle", "건축 한도에 도달했어요");
	}

	switch (ClassifyFailure(FailureReason))
	{
	case EFF_BuildPlacementFailureType::InsufficientGold:
		return LOCTEXT("InsufficientGoldTitle", "골드가 부족해요");
	case EFF_BuildPlacementFailureType::ConfigurationError:
		return LOCTEXT("ConfigurationErrorTitle", "시설 정보를 불러오지 못했어요");
	case EFF_BuildPlacementFailureType::WorldStateChanged:
		return LOCTEXT("WorldStateChangedTitle", "주변 상태가 바뀌었어요");
	case EFF_BuildPlacementFailureType::Unknown:
		return LOCTEXT("UnknownFailureTitle", "설치를 완료하지 못했어요");
	default:
		return bIsMoving
			? LOCTEXT("MoveRejectedTitle", "여기로 옮길 수 없어요")
			: LOCTEXT("InstallRejectedTitle", "여기에 설치할 수 없어요");
	}
}

FText UFF_BuildPlacementFailureFeedback::MakeBoundaryAdvice()
{
	return LOCTEXT("BoundaryAdvice", "붉은 사선 부분을 흰색 건축 경계 안으로 옮겨 주세요. 구역 사이 빈틈에도 놓을 수 없어요.");
}

FText UFF_BuildPlacementFailureFeedback::MakeValidationReason(const EFFZSPlacementFailure FailureReason,
	const FFFZSPlacementValidation& Validation, const bool bIsMoving)
{
	if (const FFFZSPlacementObstruction* GroundIssue = Validation.Obstructions.FindByPredicate(
		[FailureReason](const FFFZSPlacementObstruction& Issue)
		{
			return Issue.Reason == FailureReason && Issue.Kind == EFFZSPlacementObstructionKind::GroundSupport;
		}))
	{
		const FText GroundAdvice = FText::Format(
			LOCTEXT("GroundSupportAdvice", "{0}\n붉은 사선 영역의 바닥을 확인하고, 더 평평하고 빈틈없는 곳으로 옮겨 주세요."),
			GroundIssue->Detail.IsEmpty() ? GroundIssue->DisplayName : GroundIssue->Detail);
		const bool bAlsoBlocked = Validation.Obstructions.ContainsByPredicate(
			[FailureReason](const FFFZSPlacementObstruction& Issue)
			{
				return Issue.Reason == FailureReason && Issue.Kind != EFFZSPlacementObstructionKind::GroundSupport;
			});
		return bAlsoBlocked ? FText::Format(
			LOCTEXT("GroundAndObstacleAdvice", "{0}\n주황 선으로 표시한 장애물과도 겹쳐요."), GroundAdvice) : GroundAdvice;
	}
	if (FailureReason == EFFZSPlacementFailure::OutsideZone)
	{
		const FText BoundaryFailure = Validation.GetBoundaryFailureText();
		if (!BoundaryFailure.IsEmpty())
			return FText::Format(LOCTEXT("BoundaryFailureReason", "{0} · {1}"), BoundaryFailure, MakeBoundaryAdvice());
	}
	// Summary describes only the primary failure, never a later selected problem.
	const FText Detail = !Validation.Reasons.IsEmpty() && Validation.Reasons[0] == FailureReason
		? Validation.Summary : FText::GetEmpty();
	return MakeUserFacingReason(FailureReason, Detail, bIsMoving);
}

FText UFF_BuildPlacementFailureFeedback::MakeUserFacingReason(
	const EFFZSPlacementFailure FailureReason,
	const FText& AuthoritativeDetail,
	const bool bIsMoving)
{
	(void)bIsMoving;
	switch (FailureReason)
	{
	case EFFZSPlacementFailure::ZoneNotFound:
		return LOCTEXT("ChooseBuildableGroundReason", "건축 구역의 바닥을 가리켜 주세요 · 흰색 경계선 안쪽으로 옮겨 주세요");
	case EFFZSPlacementFailure::OutsideZone:
		return AuthoritativeDetail.IsEmpty()
			? LOCTEXT("OutsideZoneReason", "필요한 공간이 건축 구역을 벗어났어요 · 흰색 경계선 안쪽으로 모든 영역이 들어오게 옮겨 주세요")
			: FText::Format(LOCTEXT("OutsideZoneAuthoritativeReason", "{0} · {1}"), AuthoritativeDetail, MakeBoundaryAdvice());
	case EFFZSPlacementFailure::CategoryNotAllowed:
		return LOCTEXT("CategoryNotAllowedReason", "이 시설은 이 구역에 놓을 수 없어요 · 다른 건축 구역을 선택해 주세요");
	case EFFZSPlacementFailure::InvalidGround:
		return LOCTEXT("InvalidGroundReason", "바닥을 찾지 못했어요 · 땅 위로 옮겨 주세요");
	case EFFZSPlacementFailure::ExcessSlope:
		return LOCTEXT("ExcessSlopeReason", "땅이 너무 가팔라요 · 더 평평한 곳으로 옮겨 주세요");
	case EFFZSPlacementFailure::HeightVarianceExceeded:
		return LOCTEXT("HeightVarianceReason", "땅이 고르지 않아요 · 더 평평한 곳으로 옮겨 주세요");
	case EFFZSPlacementFailure::PlacementCollision:
		return LOCTEXT("PlacementCollisionReason", "주황 선은 장애물 판정 범위, 붉은 사선은 겹친 부분이에요. 사선이 사라지도록 옮겨 주세요.");
	case EFFZSPlacementFailure::InsufficientClearance:
		return LOCTEXT("AudienceSpaceReason", "관객석 공간과 겹쳐요 · 보라색 관객석의 붉은 사선이 사라지도록 옮겨 주세요");
	case EFFZSPlacementFailure::QueueFootprintBlocked:
		return LOCTEXT("QueueBlockedReason", "청록색 대기열의 붉은 사선을 확인하세요. 위치를 옮기거나 7·8·9·0으로 모양을 바꿔 보세요.");
	case EFFZSPlacementFailure::ProtectedRouteBlocked:
		return LOCTEXT("ProtectedRouteReason", "통행로를 막고 있어요 · 표시된 길 밖으로 옮겨 주세요");
	case EFFZSPlacementFailure::AccessibilityRouteBlocked:
		return LOCTEXT("AccessibilityRouteReason", "이동에 필요한 길을 막고 있어요 · 표시된 길 밖으로 옮겨 주세요");
	case EFFZSPlacementFailure::RequiredRouteDisconnected:
		return LOCTEXT("DisconnectedRouteReason", "입구까지 갈 수 없어요 · 입구가 길과 이어지게 놓아 주세요");
	case EFFZSPlacementFailure::WrongStage:
		return LOCTEXT("WrongStageReason", "지금은 건축할 수 없어요 · 준비 시간에 다시 시도해 주세요");
	case EFFZSPlacementFailure::FacilityOccupied:
	case EFFZSPlacementFailure::InvalidFacilityState:
		return LOCTEXT("OccupiedReason", "이용 중인 시설은 바꿀 수 없어요 · 손님이 나갈 때까지 기다려 주세요");
	case EFFZSPlacementFailure::StaleWorldRevision:
		return LOCTEXT("StaleWorldReason", "주변 상태가 방금 바뀌었어요 · 위치를 다시 확인해 주세요");
	case EFFZSPlacementFailure::NavigationUnavailable:
	case EFFZSPlacementFailure::NavRebuildTimeout:
		return LOCTEXT("NavigationReason", "길을 확인하는 중이에요 · 잠시 후 다시 시도해 주세요");
	case EFFZSPlacementFailure::InsufficientFunds:
		return AuthoritativeDetail.IsEmpty()
			? LOCTEXT("InsufficientFundsReason", "설치 비용이 부족해요 · 카탈로그에서 필요한 골드를 확인해 주세요")
			: AuthoritativeDetail;
	case EFFZSPlacementFailure::BuildLimitReached:
		return AuthoritativeDetail.IsEmpty()
			? LOCTEXT("BuildLimitReachedReason", "이 시설은 더 설치할 수 없어요 · 기존 시설을 철거한 뒤 다시 시도해 주세요")
			: AuthoritativeDetail;
	case EFFZSPlacementFailure::FacilityNotFound:
	case EFFZSPlacementFailure::DuplicateZoneId:
	case EFFZSPlacementFailure::DuplicateFacilityId:
	case EFFZSPlacementFailure::DuplicatePayloadMismatch:
	case EFFZSPlacementFailure::DataInvalid:
	case EFFZSPlacementFailure::InternalError:
		return LOCTEXT("DataReason", "설치 정보를 불러오지 못했어요 · 다른 시설을 선택하거나 다시 시도해 주세요");
	case EFFZSPlacementFailure::ExternalPolicyRejected:
		return LOCTEXT("PolicyReason", "현재 조건에서는 놓을 수 없어요 · 위치를 바꾸거나 잠시 후 다시 시도해 주세요");
	case EFFZSPlacementFailure::None:
	default:
		return LOCTEXT("GenericReason", "위치나 회전을 바꾼 뒤 다시 시도해 주세요");
	}
}

void UFF_BuildPlacementFailureFeedback::PresentFailure(
	const EFF_BuildPlacementFailureType InFailureType,
	const FText& InTitle,
	const FText& InReason)
{
	// 위젯을 새로 만들지 않고 최신 내용과 타이머만 교체해 연속 실패에서도 토스트가 중첩되지 않게 한다.
	FailureType = InFailureType;
	FailureTitle = InTitle;
	FailureReason = InReason;
	bHasRuntimePresentation = true;
	ApplyPresentation();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	BP_OnFailurePresented(FailureType, FailureTitle, FailureReason);
	RestartAutoDismissTimer();
}

void UFF_BuildPlacementFailureFeedback::DismissFeedback()
{
	ClearAutoDismissTimer();
	if (GetVisibility() != ESlateVisibility::Collapsed)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		BP_OnFailureDismissed();
	}
}

FText UFF_BuildPlacementFailureFeedback::GetDisplayedFailureTitle() const
{
	return TXT_FailureTitle ? TXT_FailureTitle->GetText() : FText::GetEmpty();
}

FText UFF_BuildPlacementFailureFeedback::GetDisplayedFailureReason() const
{
	return TXT_FailureReason ? TXT_FailureReason->GetText() : FText::GetEmpty();
}

void UFF_BuildPlacementFailureFeedback::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyPresentation();
	// 런타임에서 아직 실패를 받은 적이 없는 토스트는 한 프레임도 빈 상태로 노출하지 않는다.
	if (!IsDesignTime() && !bHasRuntimePresentation)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UFF_BuildPlacementFailureFeedback::NativeDestruct()
{
	ClearAutoDismissTimer();
	Super::NativeDestruct();
}

void UFF_BuildPlacementFailureFeedback::ApplyPresentation()
{
	const bool bUseDesignerPreview = IsDesignTime() && !bHasRuntimePresentation;
	const FText& Title = bUseDesignerPreview ? DesignerPreviewTitle : FailureTitle;
	const FText& Reason = bUseDesignerPreview ? DesignerPreviewReason : FailureReason;
	if (TXT_FailureTitle)
	{
		TXT_FailureTitle->SetText(Title);
	}
	if (TXT_FailureReason)
	{
		TXT_FailureReason->SetText(Reason);
	}
}

void UFF_BuildPlacementFailureFeedback::RestartAutoDismissTimer()
{
	// 재표시마다 이전 만료 시점을 버려 최신 실패가 설정된 시간만큼 온전히 보이게 한다.
	ClearAutoDismissTimer();
	if (AutoDismissSeconds <= 0.0f)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			AutoDismissTimer,
			this,
			&ThisClass::DismissFeedback,
			AutoDismissSeconds,
			false);
	}
}

void UFF_BuildPlacementFailureFeedback::ClearAutoDismissTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoDismissTimer);
	}
	AutoDismissTimer.Invalidate();
}

#undef LOCTEXT_NAMESPACE
