// FF_ZoneLifecycleTestPanel.cpp

// Test_Main 개발 패널에서 Festival 단계와 시설 Lifecycle 권위 명령을 실행하고 상태를 요약한다.
// 흐름: 버튼·이벤트 바인딩 -> 명령 요청 -> 배치 결과 기록 -> 현재 단계/시설 수 화면 갱신.
// 불변식: 제품용 상태를 우회하지 않고 실제 Stage/Zone 서브시스템의 공개 API만 호출한다.
// 실패/수명: 선택적 의존성이 없으면 상태에 표시하고 Destruct에서 모든 이벤트 구독을 해제한다.

#include "Building/Testing/FF_ZoneLifecycleTestPanel.h"

#include "FF_ZoneLifecycleDebugDayControl.h"

#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSWorldSubsystem.h"
#include "Queue/FF_QueueSubsystem.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"

#define LOCTEXT_NAMESPACE "FFZoneLifecycleTestPanel"

namespace
{
	FText LifecycleActionToText(const EFF_ZoneFacilityLifecycleAction Action)
	{
		switch (Action)
		{
		case EFF_ZoneFacilityLifecycleAction::AutomaticOpen:
			return LOCTEXT("AutomaticOpen", "자동 개장");
		case EFF_ZoneFacilityLifecycleAction::AutomaticClose:
			return LOCTEXT("AutomaticClose", "자동 폐쇄");
		case EFF_ZoneFacilityLifecycleAction::ManualOpen:
			return LOCTEXT("ManualOpen", "수동 개장");
		case EFF_ZoneFacilityLifecycleAction::ManualClose:
			return LOCTEXT("ManualClose", "수동 폐쇄");
		case EFF_ZoneFacilityLifecycleAction::ManualEmergencyClose:
			return LOCTEXT("ManualEmergencyClose", "긴급 폐쇄");
		default:
			return LOCTEXT("NoLifecycleAction", "처리 없음");
		}
	}
}

bool FFZoneLifecycleDebugDayControl::TryMoveToPreviousDay(
	UFF_FestivalStageSubsystem& Stage)
{
#if UE_BUILD_SHIPPING
	(void)Stage;
	return false;
#else
	if (Stage.CurrentDay <= 1 || Stage.CurrentDay > 3)
	{
		return false;
	}

	const int32 SourceDay = Stage.CurrentDay;
	--Stage.CurrentDay;
	const EFF_FestivalStage EntryStage = Stage.CurrentDay == 1
		? EFF_FestivalStage::Setup
		: EFF_FestivalStage::MorningCheck;
	Stage.RequestTransition(EntryStage);

	// RequestTransition resets the night flag, but non-Live transitions do not emit
	// an environment edge. Re-apply the selected weather through the public API.
	Stage.SetSelectedWeather(Stage.GetSelectedWeather());

	UE_LOG(LogTemp, Log,
		TEXT("[ZoneLifecycleDebug] Day %d -> Day %d (%s). External state was not rolled back."),
		SourceDay,
		Stage.CurrentDay,
		*UEnum::GetValueAsString(EntryStage));
	return true;
#endif
}

bool FFZoneLifecycleDebugDayControl::TryMoveToNextDay(
	UFF_FestivalStageSubsystem& Stage)
{
#if UE_BUILD_SHIPPING
	(void)Stage;
	return false;
#else
	if (Stage.CurrentDay < 1 || Stage.CurrentDay >= 3)
	{
		return false;
	}

	const int32 SourceDay = Stage.CurrentDay;
	// AdvanceToNextDay is the production day-boundary authority and accepts Report only.
	if (!Stage.IsReportStage())
	{
		if (Stage.CurrentStage != EFF_FestivalStage::Closing)
		{
			Stage.RequestTransition(EFF_FestivalStage::Closing);
		}
		Stage.RequestTransition(EFF_FestivalStage::Report);
	}
	Stage.AdvanceToNextDay();

	return Stage.CurrentDay == SourceDay + 1
		&& Stage.CurrentStage == EFF_FestivalStage::MorningCheck;
#endif
}

void UFF_ZoneLifecycleTestPanel::NativeConstruct()
{
	Super::NativeConstruct();

	BTN_Setup->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSetupClicked);
	BTN_MorningCheck->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMorningCheckClicked);
	BTN_Live->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLiveClicked);
	BTN_Closing->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClosingClicked);
	BTN_PreviousDay->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePreviousDayClicked);
	BTN_NextDay->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleNextDayClicked);
	BTN_OpenAll->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleOpenAllClicked);
	BTN_CloseAll->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseAllClicked);
	BTN_EmergencyCloseAll->OnClicked.AddUniqueDynamic(
		this, &ThisClass::HandleEmergencyCloseAllClicked);
	BTN_Refresh->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRefreshClicked);

	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>()
		: nullptr)
	{
		Stage->OnStageChanged.AddUniqueDynamic(this, &ThisClass::HandleStageChanged);
	}
	if (UFFZSWorldSubsystem* ZoneSystem = World
		? World->GetSubsystem<UFFZSWorldSubsystem>()
		: nullptr)
	{
		ZoneSystem->OnWorldChanged.AddUniqueDynamic(this, &ThisClass::HandleZoneWorldChanged);
	}
	if (UFF_ZoneSystemStageBridgeSubsystem* StageBridge = World
		? World->GetSubsystem<UFF_ZoneSystemStageBridgeSubsystem>()
		: nullptr)
	{
		StageBridge->OnFacilityLifecycleBatchCompleted.AddUniqueDynamic(
			this, &ThisClass::HandleLifecycleBatchCompleted);
	}

	SetLastResult(LOCTEXT(
		"Ready",
		"준비 완료: 시설을 설치한 뒤 Live 전환으로 자동 개장을 확인하세요."));
	RefreshStatus();
}

void UFF_ZoneLifecycleTestPanel::NativeDestruct()
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>()
		: nullptr)
	{
		Stage->OnStageChanged.RemoveDynamic(this, &ThisClass::HandleStageChanged);
	}
	if (UFFZSWorldSubsystem* ZoneSystem = World
		? World->GetSubsystem<UFFZSWorldSubsystem>()
		: nullptr)
	{
		ZoneSystem->OnWorldChanged.RemoveDynamic(this, &ThisClass::HandleZoneWorldChanged);
	}
	if (UFF_ZoneSystemStageBridgeSubsystem* StageBridge = World
		? World->GetSubsystem<UFF_ZoneSystemStageBridgeSubsystem>()
		: nullptr)
	{
		StageBridge->OnFacilityLifecycleBatchCompleted.RemoveDynamic(
			this, &ThisClass::HandleLifecycleBatchCompleted);
	}

	Super::NativeDestruct();
}

void UFF_ZoneLifecycleTestPanel::RefreshStatus()
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>()
		: nullptr;
	UFFZSWorldSubsystem* ZoneSystem = World
		? World->GetSubsystem<UFFZSWorldSubsystem>()
		: nullptr;
	BTN_PreviousDay->SetIsEnabled(
		Stage && Stage->CurrentDay > 1 && Stage->CurrentDay <= 3);
	BTN_NextDay->SetIsEnabled(Stage && Stage->CurrentDay >= 1 && Stage->CurrentDay < 3);
	if (!Stage || !ZoneSystem)
	{
		TXT_Stage->SetText(LOCTEXT("MissingSubsystems", "Stage/Zone Subsystem 없음"));
		return;
	}

	TXT_Stage->SetText(FText::Format(
		LOCTEXT("StageFormat", "Day {0}  |  Festival Stage: {1}"),
		FText::AsNumber(Stage->CurrentDay),
		UEnum::GetDisplayValueAsText(Stage->CurrentStage)));
	TXT_PhaseRevision->SetText(FText::Format(
		LOCTEXT("PhaseRevisionFormat", "FFZS Phase: {0}  |  Revision: {1}"),
		UEnum::GetDisplayValueAsText(ZoneSystem->GetChangePhase()),
		FText::AsNumber(ZoneSystem->GetWorldRevision())));

	int32 Planned = 0;
	int32 Validated = 0;
	int32 Active = 0;
	int32 Closed = 0;
	int32 MovePending = 0;
	int32 NeedsRelocation = 0;
	int32 QueueOpen = 0;
	int32 QueueDraining = 0;
	int32 QueueClosed = 0;
	int32 ProjectRegistered = 0;
	int32 ProjectOpen = 0;

	UFF_ZoneSystemQueueBridgeSubsystem* QueueBridge = World
		? World->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>()
		: nullptr;
	UFF_QueueSubsystem* QueueSystem = World
		? World->GetSubsystem<UFF_QueueSubsystem>()
		: nullptr;
	const FFFZSReadModel ReadModel = ZoneSystem->BuildReadModel();
	for (const FFFZSFacilityReadModel& Facility : ReadModel.Facilities)
	{
		switch (Facility.State)
		{
		case EFFZSFacilityState::Planned: ++Planned; break;
		case EFFZSFacilityState::Validated: ++Validated; break;
		case EFFZSFacilityState::Active: ++Active; break;
		case EFFZSFacilityState::Closed: ++Closed; break;
		case EFFZSFacilityState::MovePending: ++MovePending; break;
		default: break;
		}
		NeedsRelocation += Facility.bNeedsRelocation ? 1 : 0;

		switch (Facility.QueueState)
		{
		case EFFZSQueueState::Open:
		case EFFZSQueueState::Full:
			++QueueOpen;
			break;
		case EFFZSQueueState::Draining:
			++QueueDraining;
			break;
		default:
			++QueueClosed;
			break;
		}

		const int32 QueueFacilityId = QueueBridge
			? QueueBridge->FindQueueFacilityId(Facility.FacilityInstanceId)
			: INDEX_NONE;
		if (QueueSystem && QueueFacilityId != INDEX_NONE
			&& QueueSystem->IsFacilityRegistered(QueueFacilityId))
		{
			++ProjectRegistered;
			const FFF_QueueState ProjectState = QueueSystem->GetQueueState(QueueFacilityId);
			ProjectOpen += ProjectState.bIsOpen && ProjectState.bAcceptingNew ? 1 : 0;
		}
	}

	TXT_FacilityCounts->SetText(FText::Format(
		LOCTEXT(
			"FacilityCountsFormat",
			"시설 {0} | Planned {1} / Validated {2} / Active {3} / Closed {4} / MovePending {5} / 재배치 {6}"),
		FText::AsNumber(ReadModel.Facilities.Num()),
		FText::AsNumber(Planned),
		FText::AsNumber(Validated),
		FText::AsNumber(Active),
		FText::AsNumber(Closed),
		FText::AsNumber(MovePending),
		FText::AsNumber(NeedsRelocation)));
	TXT_QueueCounts->SetText(FText::Format(
		LOCTEXT(
			"QueueCountsFormat",
			"FFZS Queue Open {0} / Draining {1} / Closed·Blocked {2} | Project Queue 등록 {3} / 개방 {4}"),
		FText::AsNumber(QueueOpen),
		FText::AsNumber(QueueDraining),
		FText::AsNumber(QueueClosed),
		FText::AsNumber(ProjectRegistered),
		FText::AsNumber(ProjectOpen)));
}

bool UFF_ZoneLifecycleTestPanel::IsPointerOverInteractivePanel() const
{
	return IsVisible() && Border_Panel && Border_Panel->IsHovered();
}

void UFF_ZoneLifecycleTestPanel::HandleSetupClicked()
{
	RequestStage(EFF_FestivalStage::Setup);
}

void UFF_ZoneLifecycleTestPanel::HandleMorningCheckClicked()
{
	RequestStage(EFF_FestivalStage::MorningCheck);
}

void UFF_ZoneLifecycleTestPanel::HandleLiveClicked()
{
	RequestStage(EFF_FestivalStage::Live);
}

void UFF_ZoneLifecycleTestPanel::HandleClosingClicked()
{
	RequestStage(EFF_FestivalStage::Closing);
}

void UFF_ZoneLifecycleTestPanel::HandlePreviousDayClicked()
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>()
		: nullptr;
	if (!Stage)
	{
		SetLastResult(LOCTEXT("PreviousDayMissingStage", "이전 날 실패: Stage Subsystem 없음"));
		return;
	}
	if (Stage->CurrentDay <= 1)
	{
		SetLastResult(LOCTEXT("PreviousDayFirstDay", "이전 날 없음: 현재가 첫날입니다."));
		RefreshStatus();
		return;
	}

	const int32 SourceDay = Stage->CurrentDay;
	const bool bWasLiveStage = Stage->IsLiveStage();
	UFF_ZoneSystemStageBridgeSubsystem* StageBridge = World
		? World->GetSubsystem<UFF_ZoneSystemStageBridgeSubsystem>()
		: nullptr;
	if (!StageBridge)
	{
		SetLastResult(LOCTEXT(
			"PreviousDayMissingBridge",
			"이전 날 실패: 시설 Stage Bridge 없음"));
		return;
	}
	const FFF_ZoneFacilityLifecycleBatchResult CloseResult =
		StageBridge->CloseActiveFacilities(false);
	if (CloseResult.FailedCount > 0)
	{
		SetLastResult(FText::Format(
			LOCTEXT("PreviousDayCloseFailed", "이전 날 중단: 시설 폐쇄 실패 · {0}"),
			CloseResult.Summary));
		RefreshStatus();
		return;
	}

	if (!FFZoneLifecycleDebugDayControl::TryMoveToPreviousDay(*Stage))
	{
		SetLastResult(LOCTEXT("PreviousDayFailed", "이전 날 전환을 완료하지 못했습니다."));
		RefreshStatus();
		return;
	}
	bSuppressNextEmptyAutomaticCloseResult = bWasLiveStage;

	SetLastResult(FText::Format(
		LOCTEXT(
			"PreviousDaySucceeded",
			"운영 시간축 Day {0} -> Day {1} {2} (경제·예약은 롤백하지 않음)"),
		FText::AsNumber(SourceDay),
		FText::AsNumber(Stage->CurrentDay),
		Stage->GetCurrentStageDisplayName()));
	RefreshStatus();
}

void UFF_ZoneLifecycleTestPanel::HandleNextDayClicked()
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>()
		: nullptr;
	if (!Stage)
	{
		SetLastResult(LOCTEXT("NextDayMissingStage", "다음 날 실패: Stage Subsystem 없음"));
		return;
	}
	if (Stage->CurrentDay >= 3)
	{
		SetLastResult(LOCTEXT("NextDayFinalDay", "다음 날 없음: 현재가 마지막 날입니다."));
		RefreshStatus();
		return;
	}

	const int32 EndedDay = Stage->CurrentDay;
	const bool bAdvanced = FFZoneLifecycleDebugDayControl::TryMoveToNextDay(*Stage);
	SetLastResult(bAdvanced
		? FText::Format(
			LOCTEXT("NextDaySucceeded", "Day {0} 종료 -> Day {1} 개장 점검"),
			FText::AsNumber(EndedDay),
			FText::AsNumber(Stage->CurrentDay))
		: LOCTEXT("NextDayFailed", "다음 날 전환을 완료하지 못했습니다."));
	RefreshStatus();
}

void UFF_ZoneLifecycleTestPanel::HandleOpenAllClicked()
{
	if (UWorld* World = GetWorld())
	{
		if (UFF_ZoneSystemStageBridgeSubsystem* Bridge =
			World->GetSubsystem<UFF_ZoneSystemStageBridgeSubsystem>())
		{
			Bridge->OpenEligibleFacilities(true);
		}
	}
}

void UFF_ZoneLifecycleTestPanel::HandleCloseAllClicked()
{
	if (UWorld* World = GetWorld())
	{
		if (UFF_ZoneSystemStageBridgeSubsystem* Bridge =
			World->GetSubsystem<UFF_ZoneSystemStageBridgeSubsystem>())
		{
			Bridge->CloseActiveFacilities(false);
		}
	}
}

void UFF_ZoneLifecycleTestPanel::HandleEmergencyCloseAllClicked()
{
	if (UWorld* World = GetWorld())
	{
		if (UFF_ZoneSystemStageBridgeSubsystem* Bridge =
			World->GetSubsystem<UFF_ZoneSystemStageBridgeSubsystem>())
		{
			Bridge->CloseActiveFacilities(true);
		}
	}
}

void UFF_ZoneLifecycleTestPanel::HandleRefreshClicked()
{
	RefreshStatus();
}

void UFF_ZoneLifecycleTestPanel::HandleStageChanged(
	const EFF_FestivalStage PreviousStage,
	const EFF_FestivalStage NewStage)
{
	(void)PreviousStage;
	SetLastResult(FText::Format(
		LOCTEXT("StageChanged", "Stage 전환: {0} (자동 개폐는 다음 Tick에 실행)"),
		UEnum::GetDisplayValueAsText(NewStage)));
	RefreshStatus();
}

void UFF_ZoneLifecycleTestPanel::HandleZoneWorldChanged(const FFFZSWorldChangeEvent& Event)
{
	(void)Event;
	RefreshStatus();
}

void UFF_ZoneLifecycleTestPanel::HandleLifecycleBatchCompleted(
	const FFF_ZoneFacilityLifecycleBatchResult& Result)
{
	if (bSuppressNextEmptyAutomaticCloseResult)
	{
		bSuppressNextEmptyAutomaticCloseResult = false;
		if (Result.Action == EFF_ZoneFacilityLifecycleAction::AutomaticClose
			&& Result.AttemptedCount == 0
			&& Result.FailedCount == 0)
		{
			RefreshStatus();
			return;
		}
	}
	ApplyBatchResult(Result);
}

void UFF_ZoneLifecycleTestPanel::RequestStage(const EFF_FestivalStage StageValue)
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>()
		: nullptr)
	{
		Stage->RequestTransition(StageValue);
	}
}

void UFF_ZoneLifecycleTestPanel::ApplyBatchResult(
	const FFF_ZoneFacilityLifecycleBatchResult& Result)
{
	SetLastResult(FText::Format(
		LOCTEXT("BatchResult", "{0}: {1}"),
		LifecycleActionToText(Result.Action),
		Result.Summary));
	RefreshStatus();
}

void UFF_ZoneLifecycleTestPanel::SetLastResult(const FText& Text)
{
	if (TXT_LastResult)
	{
		TXT_LastResult->SetText(Text);
	}
}

#undef LOCTEXT_NAMESPACE
