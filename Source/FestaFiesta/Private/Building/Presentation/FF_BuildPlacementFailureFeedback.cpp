// Copyright Epic Games, Inc. All Rights Reserved.

#include "Building/Presentation/FF_BuildPlacementFailureFeedback.h"

#include "CommonTextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "FFBuildPlacementFailureFeedback"

EFF_BuildPlacementFailureType UFF_BuildPlacementFailureFeedback::ClassifyFailure(
	const EFFZSPlacementFailure FailureReason)
{
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
