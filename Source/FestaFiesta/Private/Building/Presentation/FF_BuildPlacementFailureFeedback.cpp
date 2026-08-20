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
	switch (ClassifyFailure(FailureReason))
	{
	case EFF_BuildPlacementFailureType::InsufficientGold:
		return LOCTEXT("InsufficientGoldTitle", "골드 부족");
	case EFF_BuildPlacementFailureType::ConfigurationError:
		return LOCTEXT("ConfigurationErrorTitle", "시설 데이터 오류");
	case EFF_BuildPlacementFailureType::WorldStateChanged:
		return LOCTEXT("WorldStateChangedTitle", "설치 조건 변경");
	case EFF_BuildPlacementFailureType::Unknown:
		return LOCTEXT("UnknownFailureTitle", "설치 오류");
	default:
		return bIsMoving
			? LOCTEXT("MoveRejectedTitle", "이동할 수 없음")
			: LOCTEXT("InstallRejectedTitle", "설치할 수 없음");
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
