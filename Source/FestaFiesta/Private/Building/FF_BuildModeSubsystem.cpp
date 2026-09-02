// FF_BuildModeSubsystem.cpp

// 건축 모드의 입력·UI·미리보기와 Zone System 권위 배치/이동/철거 명령을 조정한다.
// 흐름: 세션 진입 -> 카탈로그 선택 -> 커서 기반 검증 -> 명령 커밋 또는 실패 피드백.
// 불변식: 한 시점에는 하나의 배치/이동/철거 선택만 유지하며 월드·단계 변경 시 임시 상태를 정리한다.
// 실패/성능: 무효 권위 조건은 커밋 전에 차단하고 커서 기반 전체 검증은 제한된 주기로 실행한다.

#include "Building/FF_BuildModeSubsystem.h"

#include "Building/FF_BuildCatalogEconomyReadModel.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/FF_BuildCatalogTypes.h"
#include "Building/Presentation/FF_BuildModeGuide.h"
#include "Building/Presentation/FF_BuildPlacementFailureFeedback.h"
#include "Building/Testing/FF_ZoneLifecycleTestPanelSubsystem.h"
#include "Economy/FF_ProgramSchedulingSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSPlacementPreviewActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SViewport.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/Facility/FF_FacilityInfoSubsystem.h"
#include "Building/UI/FF_BuildCatalogScreen.h"
#include "TimerManager.h"
#include "UI/Settings/FF_FestivalSettingsSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFBuildMode, Log, All);

namespace
{
	bool IsEditableSlateTextFocused()
	{
		if (!FSlateApplication::IsInitialized())
		{
			return false;
		}

		// UEditableTextBox focuses its inner SEditableText. Walking parents also covers
		// multiline/search wrappers without coupling BuildMode to a particular UMG screen.
		for (TSharedPtr<SWidget> Widget = FSlateApplication::Get().GetKeyboardFocusedWidget();
			Widget; Widget = Widget->GetParentWidget())
		{
			if (FFBuildModeModel::IsEditableSlateTextWidgetType(Widget->GetType()))
			{
				return true;
			}
		}
		return false;
	}

	bool IsSlateWidgetOnVisiblePath(const TSharedPtr<SWidget>& Widget)
	{
		if (!Widget.IsValid() || !FSlateApplication::IsInitialized())
		{
			return false;
		}

		// 위젯 풀에 살아 있어도 현재 창의 visible path에서 빠진 Slate 객체는 포커스 대상으로 쓰지 않는다.
		FWidgetPath VisiblePath;
		return FSlateApplication::Get().GeneratePathToWidgetUnchecked(
			Widget.ToSharedRef(), VisiblePath, EVisibility::Visible);
	}

	// 디자이너가 만든 기본 카탈로그 화면을 지연 참조한다. 패키지 로드 실패 시 아래 생성 함수가
	// UFF_BuildCatalogScreen의 C++ 구현으로 폴백하므로 에셋 누락이 건축 모드 전체를 중단시키지 않는다.
	const TSoftClassPtr<UFF_BuildCatalogScreen> BuildCatalogScreenClass(
		FSoftObjectPath(TEXT(
			"/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildCatalogScreen.WBP_FF_BuildCatalogScreen_C")));
	const TSoftClassPtr<UFF_BuildPlacementFailureFeedback> PlacementFailureFeedbackClass(
		FSoftObjectPath(TEXT(
			"/Game/FestaFiesta/Building/Feedback/WBP_FF_BuildPlacementFailureFeedback."
			"WBP_FF_BuildPlacementFailureFeedback_C")));
	// FF.BuildModeGuideUMG 콘솔 명령이 만든 좌하단 조작 안내 패널. 없으면 조용히 건너뛴다.
	const TSoftClassPtr<UFF_BuildModeGuide> BuildModeGuideClass(
		FSoftObjectPath(TEXT(
			"/Game/FestaFiesta/Building/Guide/WBP_FF_BuildModeGuide."
			"WBP_FF_BuildModeGuide_C")));
}

bool UFF_BuildModeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// 상위 클래스가 거부한 Outer에는 서브시스템을 강제로 만들지 않는다.
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	const UWorld* World = Cast<UWorld>(Outer);
	if (!World || !DoesSupportWorldType(World->WorldType))
	{
		return false;
	}

	return true;
}

bool UFF_BuildModeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// 에디터 미리보기나 썸네일 월드에는 로컬 PlayerController가 없으며 UI를 생성해서도 안 된다.
	return WorldType == EWorldType::Game ||
		WorldType == EWorldType::PIE ||
		WorldType == EWorldType::GamePreview;
}

void UFF_BuildModeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// 프리뷰/설치 명령은 Zone System의 read model과 revision을 즉시 사용하므로 초기화 순서를 보장한다.
	Collection.InitializeDependency<UFFZSWorldSubsystem>();
	Collection.InitializeDependency<UFF_FacilityInfoSubsystem>();
	Super::Initialize(Collection);
	if (UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem())
	{
		ZoneSystem->OnWorldChanged.AddUniqueDynamic(this, &ThisClass::HandleWorldChanged);
		ZoneSystem->OnWorldSnapshotRestored.AddUniqueDynamic(
			this, &ThisClass::HandleWorldSnapshotRestored);
	}
	if (UFF_FacilityInfoSubsystem* FacilityInfo = GetFacilityInfoSubsystem())
	{
		FacilityInfo->OnMoveRequested.AddUObject(this, &ThisClass::HandleMoveRequested);
		FacilityInfo->OnRemoveRequested.AddUObject(this, &ThisClass::HandleRemoveRequested);
		FacilityInfo->OnCloseRequested.AddUObject(this, &ThisClass::HandleRemovalCancelRequested);
	}
	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UFF_FestivalStageSubsystem* Stage =
			GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>())
		{
			Stage->OnStageChanged.AddUniqueDynamic(this, &ThisClass::HandleFestivalStageChanged);
		}
	}
}

void UFF_BuildModeSubsystem::Deinitialize()
{
	const bool bShouldRestoreInput = bBuildSessionActive;
	ClearPauseMenuInputSuspension();
	TGuardValue<bool> ExitGuard(bIsExitingBuildMode, true);
	SetState(EFF_BuildModeState::Closed);
	bBuildSessionActive = false;
	if (UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem())
	{
		ZoneSystem->OnWorldChanged.RemoveAll(this);
		ZoneSystem->OnWorldSnapshotRestored.RemoveAll(this);
	}
	if (UFF_FacilityInfoSubsystem* FacilityInfo = GetFacilityInfoSubsystem())
	{
		FacilityInfo->OnMoveRequested.RemoveAll(this);
		FacilityInfo->OnRemoveRequested.RemoveAll(this);
		FacilityInfo->OnCloseRequested.RemoveAll(this);
	}
	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UFF_FestivalStageSubsystem* Stage =
			GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>())
		{
			Stage->OnStageChanged.RemoveAll(this);
		}
	}
	// 월드 종료 중에는 일부 협력 객체가 이미 사라졌을 수 있다. 소유한 객체부터 정리하고,
	// 실제로 입력을 변경한 세션에서만 남아 있는 로컬 컨트롤러의 입력 모드를 복구한다.
	RemoveCatalogScreen();
	DismissPlacementFailureFeedback(true);
	RemoveBuildModeGuide();
	DestroyPlacementPreview();
	ClearPendingRemovalSelection();
	ResetMovingSelection();
	SetFacilityRangeVisibility(false);
	ReleaseCatalogThumbnailSession();
	if (bShouldRestoreInput)
	{
		if (APlayerController* Controller = GetLocalPlayerController())
		{
			RestoreInputMode(*Controller);
		}
	}
	ResetPlacementSelection();
	bSuppressWorldClickUntilReleased = false;
	bSuppressPlacementConfirmUntilReleased = false;
	bSuppressBuildToggleUntilReleased = false;
	ResetPlacementRotationInput();
	PreviousFocusedWidget.Reset();
	Super::Deinitialize();
}

void UFF_BuildModeSubsystem::Tick(const float DeltaTime)
{
	// 이 기능은 첫 로컬 플레이어용이다. 서버 월드나 컨트롤러 생성 전 Tick에서는 아무 작업도 하지 않는다.
	APlayerController* Controller = GetLocalPlayerController();
	if (!Controller)
	{
		return;
	}
	if (bInputSuspendedForPauseMenu)
	{
		// Pause 중에는 상태·선택·프리뷰 액터를 그대로 보존하고 입력 및 비싼 커서 재검증만 멈춘다.
		ResetPlacementRotationInput();
		return;
	}
	if (FFBuildModeModel::IsBuildSessionActive(State) && !IsBuildEntryAllowed())
	{
		// Stage/Zone 이벤트 순서와 관계없이 Live에서 건축 입력이 남지 않게 한다.
		ExitBuildMode();
		return;
	}

	// 입력 처리에서 상태가 바뀔 수 있으므로 처리 직후의 State를 기준으로 아래 작업을 선택한다.
	HandleInput(*Controller, DeltaTime);
	RefreshBuildModeGuide();

	if (!FFBuildModeModel::IsPreviewActive(State))
	{
		return;
	}

	// 라인트레이스와 전체 배치 정책 검증은 비용이 있으므로 렌더 프레임보다 느린 고정 간격으로 제한한다.
	PreviewRefreshAccumulator += DeltaTime;
	if (PreviewRefreshAccumulator >= PreviewRefreshIntervalSeconds)
	{
		PreviewRefreshAccumulator = 0.0f;
		const bool bForceRefresh = bPreviewWorldDirty;
		bPreviewWorldDirty = false;
		UpdatePlacementFromCursor(bForceRefresh);
	}
}

TStatId UFF_BuildModeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_BuildModeSubsystem, STATGROUP_Tickables);
}

bool UFF_BuildModeSubsystem::SuspendInputForPauseMenu()
{
	if (!FFBuildModeModel::CanSuspendInputForPauseMenu(
		State, bInputSuspendedForPauseMenu, bIsExitingBuildMode))
	{
		return false;
	}

	const bool bWasAlreadySuspended = bInputSuspendedForPauseMenu;
	// 닫았다가 같은 프레임에 다시 연 경우 이전 resume 예약이 최신 Pause 세션을 깨우지 않게 무효화한다.
	if (UWorld* World = GetWorld(); World && PauseMenuResumeTimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(PauseMenuResumeTimerHandle);
	}
	PauseMenuResumeTimerHandle.Invalidate();
	bPauseMenuResumePending = false;
	bInputSuspendedForPauseMenu = true;
	if (!bWasAlreadySuspended && FSlateApplication::IsInitialized())
	{
		// Pause 화면이 focus를 가져가기 전에 실제 카탈로그 버튼/타일을 저장한다.
		PauseMenuFocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
	}
	ResetPlacementRotationInput();
	return true;
}

void UFF_BuildModeSubsystem::ResumeInputAfterPauseMenu()
{
	if (!bInputSuspendedForPauseMenu || bPauseMenuResumePending)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		bPauseMenuResumePending = true;
		PauseMenuResumeTimerHandle = World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &ThisClass::RestoreInputAfterPauseMenu));
		return;
	}

	// 월드가 이미 해제된 경로에는 복원할 Controller도 없다.
	bInputSuspendedForPauseMenu = false;
	PauseMenuFocusedWidget.Reset();
}

void UFF_BuildModeSubsystem::RestoreInputAfterPauseMenu()
{
	bPauseMenuResumePending = false;
	PauseMenuResumeTimerHandle.Invalidate();
	if (!bInputSuspendedForPauseMenu)
	{
		return;
	}

	TSharedPtr<SWidget> FocusToRestore = PauseMenuFocusedWidget.Pin();
	PauseMenuFocusedWidget.Reset();
	if (!IsSlateWidgetOnVisiblePath(FocusToRestore))
	{
		FocusToRestore.Reset();
	}
	bInputSuspendedForPauseMenu = false;
	if (bIsExitingBuildMode)
	{
		return;
	}

	APlayerController* Controller = GetLocalPlayerController();
	if (!Controller)
	{
		return;
	}

	switch (FFBuildModeModel::ResolvePauseResumeInputMode(State))
	{
	case FFBuildModeModel::EPauseResumeInputMode::Catalog:
		SetCatalogInputMode(*Controller, FocusToRestore);
		if (!FocusToRestore.IsValid() && bExternalCatalogPresentationActive)
		{
			OnBuildCatalogFocusRestoreRequested.Broadcast();
		}
		break;
	case FFBuildModeModel::EPauseResumeInputMode::World:
		SetBuildInputMode(*Controller, FocusToRestore);
		break;
	case FFBuildModeModel::EPauseResumeInputMode::None:
	default:
		// Pause 중 stage-policy로 건축이 종료됐으면 Hub가 닫힌 지금 세션 이전 입력으로 돌아간다.
		RestoreInputMode(*Controller);
		return;
	}

	// Resume 버튼의 mouse-up이나 Enter/Space가 곧바로 시설 선택·설치·B 토글로 관통하지 않게 한다.
	bSuppressWorldClickUntilReleased = true;
	bSuppressPlacementConfirmUntilReleased = true;
	bSuppressBuildToggleUntilReleased = true;
	ResetPlacementRotationInput();
	RefreshBuildModeGuide();
}

void UFF_BuildModeSubsystem::ClearPauseMenuInputSuspension()
{
	if (UWorld* World = GetWorld(); World && PauseMenuResumeTimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(PauseMenuResumeTimerHandle);
	}
	PauseMenuResumeTimerHandle.Invalidate();
	bPauseMenuResumePending = false;
	bInputSuspendedForPauseMenu = false;
	PauseMenuFocusedWidget.Reset();
}

void UFF_BuildModeSubsystem::SetExternalCatalogPresentationActive(const bool bActive)
{
	if (bExternalCatalogPresentationActive == bActive)
	{
		return;
	}

	bExternalCatalogPresentationActive = bActive;
	if (State != EFF_BuildModeState::CatalogOpen || bIsExitingBuildMode)
	{
		return;
	}

	if (bExternalCatalogPresentationActive)
	{
		// 실행 중 opt-in도 안전하게 지원한다. 레거시 위젯만 제거하고 세션·입력·프리로드는 유지한다.
		RemoveCatalogScreen();
		if (APlayerController* Controller = GetLocalPlayerController())
		{
			SetCatalogInputMode(*Controller);
		}
		return;
	}

	// 외부 표현이 해제될 때 CatalogOpen이면 기존 화면을 즉시 복구한다.
	OpenCatalog();
}

void UFF_BuildModeSubsystem::SetState(const EFF_BuildModeState NewState)
{
	if (State == NewState)
	{
		return;
	}

	const EFF_BuildModeState PreviousState = State;
	State = NewState;
	OnBuildModeStateChanged.Broadcast(PreviousState, State);
}

void UFF_BuildModeSubsystem::ToggleBuildMode()
{
	if (bInputSuspendedForPauseMenu)
	{
		return;
	}
	if (bIsExitingBuildMode)
	{
		return;
	}
	switch (FFBuildModeModel::ResolveBuildKeyState(State))
	{
	case EFF_BuildModeState::BuildMode:
		EnterBuildMode();
		break;
	case EFF_BuildModeState::CatalogOpen:
		OpenCatalog();
		break;
	default:
		break;
	}
}

void UFF_BuildModeSubsystem::EnterBuildMode()
{
	if (bInputSuspendedForPauseMenu)
	{
		return;
	}
	if (bIsExitingBuildMode)
	{
		return;
	}
	if (!IsBuildEntryAllowed())
	{
		if (FFBuildModeModel::IsBuildSessionActive(State))
		{
			ExitBuildMode();
		}
		ShowStatus(TEXT("현재 단계에서는 건축 모드에 들어갈 수 없습니다."), true);
		return;
	}
	APlayerController* Controller = GetLocalPlayerController();
	UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
	if (!Controller || !ZoneSystem)
	{
		ShowStatus(TEXT("건축 모드를 초기화할 수 없습니다."), true);
		return;
	}

	BeginBuildSession(*Controller);
	DismissPlacementFailureFeedback();

	RemoveCatalogScreen();
	DestroyPlacementPreview();
	ClearPendingRemovalSelection();
	ResetMovingSelection();
	ResetPlacementSelection();
	SetState(EFF_BuildModeState::BuildMode);
	SetFacilityRangeVisibility(true);
	SetBuildInputMode(*Controller);
	// UI 버튼 콜백이 mouse-up에서 오더라도 다음 Tick까지는 같은 클릭을 월드 입력으로 사용하지 않는다.
	bSuppressWorldClickUntilReleased = true;
	ShowStatus(TEXT("건축 모드 · 좌클릭 시설 선택 · B 카탈로그 · ESC/F5 일시정지"), false, 2.5f);
}

void UFF_BuildModeSubsystem::OpenCatalog()
{
	if (bInputSuspendedForPauseMenu)
	{
		return;
	}
	if (bIsExitingBuildMode)
	{
		return;
	}
	if (!IsBuildEntryAllowed())
	{
		if (FFBuildModeModel::IsBuildSessionActive(State))
		{
			ExitBuildMode();
		}
		ShowStatus(TEXT("현재 단계에서는 건축 카탈로그를 열 수 없습니다."), true);
		return;
	}
	APlayerController* Controller = GetLocalPlayerController();
	UFF_BuildCatalogSubsystem* Catalog = GetCatalogSubsystem();
	UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();

	// Closed에서 카탈로그를 곧바로 열어도 입력 복구값과 세션 수명은 BuildMode 진입 경로에서 한 번 초기화한다.
	if (State == EFF_BuildModeState::Closed)
	{
		EnterBuildMode();
		Controller = GetLocalPlayerController();
	}

	const auto AbortOpen = [this](const FString& Message)
	{
		EnterBuildMode();
		ShowStatus(Message, true);
	};
	if (!Controller || !Catalog || !ZoneSystem)
	{
		AbortOpen(TEXT("건축 카탈로그를 초기화할 수 없습니다."));
		return;
	}
	if (!ZoneSystem->HasRegisteredZones())
	{
		// 전체 시설 읽기 모델을 복사·정렬하지 않고 등록 Zone 존재 여부만 확인한다.
		AbortOpen(TEXT("이 월드에는 건축 가능한 Zone이 없습니다."));
		return;
	}

	BeginBuildSession(*Controller);
	DismissPlacementFailureFeedback();

	SetFacilityRangeVisibility(true);
	DestroyPlacementPreview();
	ClearPendingRemovalSelection();
	ResetMovingSelection();
	ResetPlacementSelection();
	RemoveCatalogScreen();
	SetState(EFF_BuildModeState::CatalogOpen);
	bCatalogWasApplied = false;
	bCatalogThumbnailsApplied = false;
	if (!bExternalCatalogPresentationActive)
	{
		CreateCatalogScreen(*Controller);
		if (!CatalogScreen)
		{
			AbortOpen(TEXT("건축 카탈로그 화면을 열지 못했습니다."));
			return;
		}
		Catalog->OnCatalogChanged.AddUniqueDynamic(this, &ThisClass::HandleCatalogChanged);
		Catalog->OnCatalogThumbnailsChanged.AddUniqueDynamic(this, &ThisClass::HandleCatalogThumbnailsChanged);
	}
	SetCatalogInputMode(*Controller);
	AcquireCatalogThumbnailSession(*Catalog);

	const EFF_BuildCatalogLoadState LoadState = Catalog->GetLoadState();
	if (LoadState == EFF_BuildCatalogLoadState::NotRequested ||
		LoadState == EFF_BuildCatalogLoadState::Failed)
	{
		// 첫 진입은 로드를 시작하고, 이전 실패 뒤 다시 연 경우에는 명시적으로 재시도한다.
		Catalog->RefreshCatalog();
	}
	if (!bExternalCatalogPresentationActive)
	{
		ApplyCatalogIfReady();
	}
}

bool UFF_BuildModeSubsystem::StartPlacement(const FName FacilityDefinitionId)
{
	if (bInputSuspendedForPauseMenu)
	{
		return false;
	}
	// NAME_None은 UI에서 아직 항목을 선택하지 않은 상태이므로 오류 UI 없이 단순 실패로 처리한다.
	UFF_BuildCatalogSubsystem* Catalog = GetCatalogSubsystem();
	if (bIsExitingBuildMode || State != EFF_BuildModeState::CatalogOpen ||
		!Catalog || FacilityDefinitionId.IsNone())
	{
		return false;
	}
	if (!CanReconfigure())
	{
		// 시간대/단계 정책은 프리뷰 생성 전에도 막아 불필요한 액터 및 입력 상태 변경을 피한다.
		if (CatalogScreen)
		{
			CatalogScreen->SetStatusText(
				FText::FromString(TEXT("현재 단계에서는 건축할 수 없습니다.")), true);
		}
		return false;
	}

	UFFZSFacilityDefinition* Definition = Catalog->FindDefinition(FacilityDefinitionId);
	if (!Definition)
	{
		// 카탈로그 항목과 실제 데이터 에셋 사이 연결이 끊긴 경우 현재 목록을 유지해 다른 시설을 고를 수 있게 한다.
		if (CatalogScreen)
		{
			CatalogScreen->SetStatusText(FText::FromString(TEXT("시설 정의를 불러오지 못했습니다.")), true);
		}
		return false;
	}

	TArray<FText> DefinitionErrors;
	if (!Definition->IsDefinitionValid(DefinitionErrors))
	{
		// 정의 자체의 스펙 오류는 Zone 배치 검증으로 넘기지 않는다. 여러 이유 중 첫 항목만 간결하게 노출한다.
		const FString Reason = DefinitionErrors.IsEmpty()
			? TEXT("시설 정의가 유효하지 않습니다.")
			: DefinitionErrors[0].ToString();
		if (CatalogScreen)
		{
			CatalogScreen->SetStatusText(FText::FromString(Reason), true);
		}
		return false;
	}

	// 카탈로그 스냅샷이 열린 뒤 다른 설치가 상한을 채울 수 있으므로 프리뷰 생성 직전에 현재 수를 다시 읽습니다.
	// 이 차단은 UX용이며 최종 권위 판정은 ExecuteCommand의 공통 ValidatePlacement가 다시 수행합니다.
	if (Definition->Spec.MaxInstancesPerWorld > 0)
	{
		const UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
		const int32 CurrentCount = ZoneSystem
			? ZoneSystem->CountFacilitiesByDefinitionId(FacilityDefinitionId)
			: 0;
		if (CurrentCount >= Definition->Spec.MaxInstancesPerWorld)
		{
			const FText LimitMessage = FText::Format(
				NSLOCTEXT("FFBuildMode", "StartPlacementBuildLimitReached", "이 시설의 건축 한도에 도달했습니다. (설치 {0}/{1})"),
				FText::AsNumber(CurrentCount),
				FText::AsNumber(Definition->Spec.MaxInstancesPerWorld));
			if (CatalogScreen)
			{
				CatalogScreen->SetStatusText(LimitMessage, true);
			}
			ShowStatus(LimitMessage.ToString(), true);
			return false;
		}
	}

	// 새 선택은 이전 Zone/원점/검증 결과와 독립적이어야 한다. 선택 스펙만 복사하고 배치 표적은 초기화한다.
	SelectedFacilityDefinitionId = FacilityDefinitionId;
	DismissPlacementFailureFeedback();
	SelectedSpec = Definition->Spec;
	CurrentPlacement = FFFZSGridPlacement{};
	CurrentZoneId = NAME_None;
	bHasPlacementTarget = false;

	RemoveCatalogScreen();
	SetState(EFF_BuildModeState::Placing);
	SetFacilityRangeVisibility(true);
	SpawnPlacementPreview();
	if (!PlacementPreview)
	{
		// 프리뷰 없이는 정책 검증과 CommitPreview를 수행할 수 없으므로 카탈로그로 안전하게 복귀한다.
		ShowStatus(TEXT("시설 배치 프리뷰를 생성하지 못했습니다."), true);
		ReturnToCatalog();
		return false;
	}
	if (APlayerController* Controller = GetLocalPlayerController())
	{
		SetBuildInputMode(*Controller);
		bSuppressWorldClickUntilReleased = true;
		bSuppressPlacementConfirmUntilReleased = true;
	}
	// 첫 프레임부터 현재 커서 위치와 회전을 명령에 반영하기 위해 위치 변화 여부와 무관하게 강제 갱신한다.
	UpdatePlacementFromCursor(true);
	ShowStatus(TEXT("좌클릭 설치 · Q/E 회전 · Ctrl 정밀 · Alt 자유 · G 격자 · 우클릭 목록 · ESC/F5 일시정지"));
	return PlacementPreview != nullptr;
}

void UFF_BuildModeSubsystem::ReturnToCatalog()
{
	if (bInputSuspendedForPauseMenu)
	{
		return;
	}
	// Closed에서 호출된 늦은 UI 이벤트는 이미 끝난 세션을 다시 열지 않도록 무시한다.
	if (bIsExitingBuildMode || State == EFF_BuildModeState::Closed)
	{
		return;
	}
	OpenCatalog();
}

void UFF_BuildModeSubsystem::ExitBuildMode()
{
	if (bIsExitingBuildMode || (!bBuildSessionActive && State == EFF_BuildModeState::Closed))
	{
		return;
	}

	const bool bDeferInputRestoreUntilPauseCloses = bInputSuspendedForPauseMenu;
	TGuardValue<bool> ExitGuard(bIsExitingBuildMode, true);
	if (!bDeferInputRestoreUntilPauseCloses)
	{
		ClearPauseMenuInputSuspension();
	}
	// 어느 활성 상태에서 호출해도 같은 Closed 불변식(UI/프리뷰/선택/표적 없음)을 만든다.
	// 아래 정리 중 썸네일 상태 방송이 동기 재진입해도 공개 전환 API는 Closed 상태와 guard를 본다.
	SetState(EFF_BuildModeState::Closed);
	bBuildSessionActive = false;
	bSuppressWorldClickUntilReleased = false;
	bSuppressPlacementConfirmUntilReleased = false;
	bSuppressBuildToggleUntilReleased = false;
	ResetPlacementRotationInput();
	RemoveCatalogScreen();
	DismissPlacementFailureFeedback(true);
	RemoveBuildModeGuide();
	DestroyPlacementPreview();
	ClearPendingRemovalSelection();
	ResetMovingSelection();
	SetFacilityRangeVisibility(false);
	ReleaseCatalogThumbnailSession();
	ResetPlacementSelection();

	// 컨트롤러가 월드 종료 등으로 사라졌다면 복구할 대상도 없으므로 정리만 완료한다.
	if (!bDeferInputRestoreUntilPauseCloses)
	{
		if (APlayerController* Controller = GetLocalPlayerController())
		{
			RestoreInputMode(*Controller);
		}
	}
	ShowStatus(TEXT("건축 모드를 종료했습니다."), false, 1.2f);
}

void UFF_BuildModeSubsystem::RotatePlacement(const int32 QuarterTurnDelta)
{
	RotatePlacementDegrees(static_cast<float>(QuarterTurnDelta) * 90.0f);
}

void UFF_BuildModeSubsystem::RotatePlacementDegrees(const float DeltaDegrees)
{
	if (bInputSuspendedForPauseMenu)
	{
		return;
	}
	if (!FFBuildModeModel::IsPreviewActive(State))
	{
		return;
	}
	if (!FMath::IsFinite(DeltaDegrees) || FMath::IsNearlyZero(DeltaDegrees))
	{
		return;
	}

	SetPlacementYawDegrees(GetPlacementYawDegrees() + DeltaDegrees);
}

void UFF_BuildModeSubsystem::SetPlacementYawDegrees(const float YawDegrees)
{
	if (bInputSuspendedForPauseMenu)
	{
		return;
	}
	if (!FFBuildModeModel::IsPreviewActive(State) || !FMath::IsFinite(YawDegrees))
	{
		return;
	}

	const int32 NewYawCentidegrees = UFFZSGridLibrary::NormalizeYawCentidegrees(
		UFFZSGridLibrary::DegreesToYawCentidegrees(YawDegrees));
	if (CurrentPlacement.YawCentidegrees == NewYawCentidegrees &&
		CurrentPlacement.QuarterTurns == 0)
	{
		return;
	}

	// BuildMode에서는 centidegree가 canonical이다. QuarterTurns는 레거시 호출 승격에만 사용되므로
	// 새 각도를 쓰는 순간 지워 FFZS 명령/액터 경계에서 두 표현이 경쟁하지 않게 한다.
	CurrentPlacement.YawCentidegrees = NewYawCentidegrees;
	CurrentPlacement.QuarterTurns = 0;
	// 자유 회전은 매 프레임 이 함수를 호출할 수 있다. 비싼 커서 Trace와 전체 권위 검증을 여기서
	// 강제하지 않고, Tick의 기존 0.08초 주기가 가장 최신 각도를 한 번만 반영하게 한다.
	bPreviewWorldDirty = true;
	RefreshBuildModeGuide();
}

float UFF_BuildModeSubsystem::GetPlacementYawDegrees() const
{
	return UFFZSGridLibrary::YawCentidegreesToDegrees(
		UFFZSGridLibrary::GetEffectiveYawCentidegrees(CurrentPlacement));
}

bool UFF_BuildModeSubsystem::CommitPlacement()
{
	if (bInputSuspendedForPauseMenu)
	{
		return false;
	}
	// 프리뷰가 없는 Placing/Moving은 불완전한 상태이므로 권위 명령을 만들지 않는다.
	if (!FFBuildModeModel::IsPreviewActive(State) || !PlacementPreview)
	{
		return false;
	}
	const bool bIsMoving = State == EFF_BuildModeState::Moving;
	if (!CanReconfigure())
	{
		ShowStatus(TEXT("현재 단계에서는 시설을 변경할 수 없습니다."), true);
		return false;
	}

	UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
	AFFZSFacilityActor* MoveTarget = bIsMoving ? MovingFacility.Get() : nullptr;
	if (bIsMoving && (!ZoneSystem || !MoveTarget || !MovingFacilityInstanceId.IsValid() ||
		MoveTarget->GetFacilityInstanceId() != MovingFacilityInstanceId ||
		ZoneSystem->FindFacility(MovingFacilityInstanceId) != MoveTarget))
	{
		CancelMovingToBuildMode(TEXT("이동 대상이 변경되어 이동을 취소했습니다."), true);
		return false;
	}
	if (bIsMoving && !MoveTarget->CanMoveOrRemove())
	{
		ShowStatus(TEXT("운영 중이거나 이용객이 남은 시설은 이동할 수 없습니다."), true);
		return false;
	}

	// 클릭 직전 커서와 월드 revision을 강제로 반영해, 마지막 주기 갱신 이후의 이동/월드 변경으로 인한
	// 오래된 유효성 결과를 사용하지 않는다.
	UpdatePlacementFromCursor(true);
	if (bIsMoving && State != EFF_BuildModeState::Moving)
	{
		// 커서 갱신 도중 외부 Move/Remove 또는 레지스트리 교체를 감지해 취소된 경우다.
		return false;
	}
	if (!bHasPlacementTarget)
	{
		ShowStatus(bIsMoving
			? TEXT("이동 가능한 Zone의 지면을 가리켜 주세요.")
			: TEXT("건축 가능한 Zone의 지면을 가리켜 주세요."), true);
		return false;
	}
	if (bIsMoving && CurrentZoneId == MoveOriginZoneId &&
		CurrentPlacement == MoveOriginPlacement)
	{
		ShowStatus(TEXT("기존 위치와 방향이 같습니다. 다른 위치로 이동해 주세요."), true);
		return false;
	}
	if (!LastValidation.bIsValid)
	{
		const EFFZSPlacementFailure FailureReason = LastValidation.Reasons.IsEmpty()
			? EFFZSPlacementFailure::InternalError
			: LastValidation.Reasons[0];
		PresentPlacementFailure(FailureReason, LastValidation.Summary);
		ShowStatus(FString::Printf(TEXT("%s 불가: %s"),
			bIsMoving ? TEXT("이동") : TEXT("배치"),
			*LastValidation.Summary.ToString()), true);
		return false;
	}

	// 커서 갱신 경로에서 대상이 외부 변경되었을 가능성까지 닫고 실행 직전에 등록·점유 상태를 다시 본다.
	MoveTarget = bIsMoving ? MovingFacility.Get() : nullptr;
	if (bIsMoving && (!ZoneSystem || !MoveTarget ||
		ZoneSystem->FindFacility(MovingFacilityInstanceId) != MoveTarget ||
		!MoveTarget->CanMoveOrRemove()))
	{
		ShowStatus(TEXT("시설 상태가 변경되어 이동을 확정할 수 없습니다."), true);
		return false;
	}

	FFFZSCommandResult Result;
	FGuid CommittingMoveFacilityId;
	FGuid CommittingMoveTransactionId;
	if (bIsMoving)
	{
		const FFFZSCommand& MoveCommand = PlacementPreview->PreviewCommand;
		if (MoveCommand.Type != EFFZSCommandType::Move ||
			MoveCommand.FacilityInstanceId != MovingFacilityInstanceId ||
			MoveCommand.ZoneId != CurrentZoneId || !(MoveCommand.Placement == CurrentPlacement) ||
			MoveCommand.ExpectedWorldRevision != ZoneSystem->GetWorldRevision())
		{
			ShowStatus(TEXT("이동 프리뷰가 최신 상태가 아닙니다. 잠시 후 다시 시도해 주세요."), true);
			bPreviewWorldDirty = true;
			return false;
		}

		// ExecuteCommand는 성공 이벤트를 반환 전 동기 방송한다. 이 구간의 일치 ID만 자체 Move로 취급한다.
		CommittingMoveFacilityId = MovingFacilityInstanceId;
		CommittingMoveTransactionId = MoveTransactionId;
		TGuardValue<bool> CommitGuard(bMoveCommitInFlight, true);
		TGuardValue<FGuid> CommandIdGuard(InFlightMoveCommandId, MoveCommand.CommandId);
		Result = PlacementPreview->CommitPreview();
	}
	else
	{
		Result = PlacementPreview->CommitPreview();
	}
	if (!Result.bSucceeded)
	{
		if (bIsMoving && !FFBuildModeModel::IsCurrentMoveSession(
			State,
			MovingFacilityInstanceId,
			MoveTransactionId,
			CommittingMoveFacilityId,
			CommittingMoveTransactionId))
		{
			// 실패 이벤트 구독자가 세션을 전환했다면 종료된 UI에 오류를 다시 표시하지 않는다.
			return false;
		}
		PresentPlacementFailure(Result.FailureReason, Result.Message);
		ShowStatus(FString::Printf(TEXT("%s 실패: %s"),
			bIsMoving ? TEXT("이동") : TEXT("설치"),
			*Result.Message.ToString()), true);
		if (bIsMoving)
		{
			// 실패는 Moving을 유지하고 다음 주기에 새 revision으로 프리뷰한다.
			bPreviewWorldDirty = true;
		}
		return false;
	}

	if (bIsMoving)
	{
		if (!FFBuildModeModel::IsCurrentMoveSession(
			State,
			MovingFacilityInstanceId,
			MoveTransactionId,
			CommittingMoveFacilityId,
			CommittingMoveTransactionId))
		{
			// 성공 이벤트는 동기 방송된다. 다른 구독자가 이미 Closed/CatalogOpen 등으로 전환했다면
			// 권위 Move 성공만 반환하고 그 구독자가 만든 최신 UI 상태를 덮어쓰지 않는다.
			return true;
		}
		const FString FacilityName = SelectedSpec.DisplayName.ToString();
		SetState(EFF_BuildModeState::BuildMode);
		DestroyPlacementPreview();
		ResetPlacementSelection();
		ResetMovingSelection();
		SetFacilityRangeVisibility(true);
		if (APlayerController* Controller = GetLocalPlayerController())
		{
			SetBuildInputMode(*Controller);
			bSuppressWorldClickUntilReleased = true;
		}
		ShowStatus(FString::Printf(TEXT("%s 이동 완료"), *FacilityName), false);
		DismissPlacementFailureFeedback();
		return true;
	}

	ShowStatus(FString::Printf(TEXT("%s 설치 완료"), *SelectedSpec.DisplayName.ToString()), false);
	DismissPlacementFailureFeedback();
	// 연속 배치를 지원하므로 성공 후 Placing 상태를 유지한다. 설치로 revision/점유 셀이 바뀌었으므로
	// 같은 위치의 다음 프리뷰도 즉시 다시 검증한다.
	UpdatePlacementFromCursor(true);
	return true;
}

void UFF_BuildModeSubsystem::HandleInput(APlayerController& Controller, const float DeltaTime)
{
	const bool bBlockBuildToggleThisFrame = bSuppressBuildToggleUntilReleased;
	if (bSuppressBuildToggleUntilReleased)
	{
		bSuppressBuildToggleUntilReleased = Controller.IsInputKeyDown(EKeys::B);
	}
	if (FFBuildModeModel::ShouldProcessRawBuildKey(
		bBlockBuildToggleThisFrame,
		Controller.WasInputKeyJustPressed(EKeys::B),
		IsEditableSlateTextFocused()))
	{
		ToggleBuildMode();
		return;
	}
	const bool bBlockWorldClickThisFrame = bSuppressWorldClickUntilReleased;
	if (bSuppressWorldClickUntilReleased)
	{
		bSuppressWorldClickUntilReleased = Controller.IsInputKeyDown(EKeys::LeftMouseButton);
	}
	const bool bBlockPlacementConfirmThisFrame = bSuppressPlacementConfirmUntilReleased;
	if (bSuppressPlacementConfirmUntilReleased)
	{
		bSuppressPlacementConfirmUntilReleased =
			Controller.IsInputKeyDown(EKeys::Enter) ||
			Controller.IsInputKeyDown(EKeys::SpaceBar);
	}
	const UFF_ZoneLifecycleTestPanelSubsystem* DebugPanel = GetWorld()
		? GetWorld()->GetSubsystem<UFF_ZoneLifecycleTestPanelSubsystem>()
		: nullptr;
	const bool bPointerOverDebugPanel = DebugPanel && DebugPanel->IsPointerOverPanel();

	if (FFBuildModeModel::CanRemoveInstalledFacility(State))
	{
		if (Controller.WasInputKeyJustPressed(EKeys::M) &&
			PendingRemovalFacility.IsValid())
		{
			StartMovingSelectedFacility();
			return;
		}
		if (Controller.WasInputKeyJustPressed(EKeys::RightMouseButton))
		{
			if (PendingRemovalFacility.IsValid())
			{
				ClearPendingRemovalSelection();
				ShowStatus(TEXT("시설 선택을 취소했습니다."), false, 1.5f);
			}
			return;
		}
		if (!bBlockPlacementConfirmThisFrame &&
			Controller.WasInputKeyJustPressed(EKeys::Enter) &&
			PendingRemovalFacility.IsValid())
		{
			ConfirmPendingRemoval();
			return;
		}
		const UFF_FacilityInfoSubsystem* FacilityInfo = GetFacilityInfoSubsystem();
		const bool bPointerOverFacilityPanel = FacilityInfo && FacilityInfo->IsPointerOverPanel();
		if (!bBlockWorldClickThisFrame && !bPointerOverFacilityPanel
			&& !bPointerOverDebugPanel &&
			Controller.WasInputKeyJustPressed(EKeys::LeftMouseButton))
		{
			HandleRemovalClickUnderCursor(Controller);
		}
		return;
	}

	if (!FFBuildModeModel::IsPreviewActive(State))
	{
		ResetPlacementRotationInput();
		return;
	}
	const bool bControlDown =
		Controller.IsInputKeyDown(EKeys::LeftControl)
		|| Controller.IsInputKeyDown(EKeys::RightControl);
	if (!bControlDown && Controller.WasInputKeyJustPressed(EKeys::G))
	{
		TogglePlacementGrid();
		return;
	}

	if (Controller.WasInputKeyJustPressed(EKeys::RightMouseButton))
	{
		if (State == EFF_BuildModeState::Moving)
		{
			CancelMovingToBuildMode(TEXT("시설 이동을 취소했습니다."));
		}
		else
		{
			// 신규 배치의 우클릭은 세션을 끝내지 않고 카탈로그로 돌아간다.
			ReturnToCatalog();
		}
		return;
	}
	// 회전과 설치 입력은 독립적으로 검사하므로 같은 프레임에 들어오면 최신 회전을 포함한 배치를 설치한다.
	HandlePlacementRotationInput(Controller, DeltaTime);
	if ((!bBlockWorldClickThisFrame && !bPointerOverDebugPanel &&
		Controller.WasInputKeyJustPressed(EKeys::LeftMouseButton)) ||
		(!bBlockPlacementConfirmThisFrame &&
			Controller.WasInputKeyJustPressed(EKeys::Enter)))
	{
		CommitPlacement();
	}
}

void UFF_BuildModeSubsystem::TogglePlacementGrid()
{
	if (!FFBuildModeModel::ShouldShowPlacementGrid(State) || !PlacementPreview)
	{
		return;
	}

	bPlacementGridVisible = !bPlacementGridVisible;
	PlacementPreview->bDrawPlacementGrid = bPlacementGridVisible;
	// 사용자가 명시적으로 누른 토글은 다음 제한 주기를 기다리지 않고 격자만 즉시 반영한다.
	// 권위 검증·외부 정책·메시 갱신은 위치/회전이 바뀌지 않았으므로 다시 실행하지 않는다.
	PlacementPreview->RefreshPlacementGridVisual();
	RefreshBuildModeGuide();
	ShowStatus(bPlacementGridVisible
		? TEXT("로컬 격자를 켰습니다. 배치 대상 주변에만 표시됩니다.")
		: TEXT("로컬 격자를 숨겼습니다. 자동 맞춤은 그대로 유지됩니다."), false, 1.8f);
}

void UFF_BuildModeSubsystem::HandlePlacementRotationInput(
	APlayerController& Controller,
	const float DeltaTime)
{
	if (!FFBuildModeModel::IsPreviewActive(State))
	{
		ResetPlacementRotationInput();
		return;
	}

	const bool bShiftDown = Controller.IsInputKeyDown(EKeys::LeftShift) ||
		Controller.IsInputKeyDown(EKeys::RightShift);
	const bool bLeftKeyDown = Controller.IsInputKeyDown(EKeys::Q);
	const bool bRightKeyDown = Controller.IsInputKeyDown(EKeys::E);
	const bool bLeftJustPressed = Controller.WasInputKeyJustPressed(EKeys::Q);
	const bool bRightJustPressed = Controller.WasInputKeyJustPressed(EKeys::E);

	if (!bLeftKeyDown)
	{
		bFreeRotateLeftGesture = false;
	}
	if (!bRightKeyDown)
	{
		bFreeRotateRightGesture = false;
	}

	if (bLeftJustPressed)
	{
		bFreeRotateLeftGesture = FFBuildModeModel::ShouldStartFreeRotationGesture(
			bLeftJustPressed, bShiftDown);
		if (FFBuildModeModel::ShouldApplySnappedRotation(bLeftJustPressed, bShiftDown))
		{
			SnapPlacementRotation(-1);
		}
	}
	if (bRightJustPressed)
	{
		bFreeRotateRightGesture = FFBuildModeModel::ShouldStartFreeRotationGesture(
			bRightJustPressed, bShiftDown);
		if (FFBuildModeModel::ShouldApplySnappedRotation(bRightJustPressed, bShiftDown))
		{
			SnapPlacementRotation(1);
		}
	}

	const float FreeRotationDirection = FFBuildModeModel::ResolveFreeRotationDirection(
		bFreeRotateLeftGesture,
		bLeftKeyDown,
		bFreeRotateRightGesture,
		bRightKeyDown,
		bShiftDown);
	if (!FMath::IsNearlyZero(FreeRotationDirection) &&
		FMath::IsFinite(DeltaTime) && DeltaTime > 0.0f)
	{
		RotatePlacementDegrees(
			FreeRotationDirection * GetFreePlacementRotationSpeedDegreesPerSecond() * DeltaTime);
	}
}

void UFF_BuildModeSubsystem::ResetPlacementRotationInput()
{
	bFreeRotateLeftGesture = false;
	bFreeRotateRightGesture = false;
}

void UFF_BuildModeSubsystem::SnapPlacementRotation(const int32 Direction)
{
	if (!FFBuildModeModel::IsPreviewActive(State) || Direction == 0)
	{
		return;
	}

	SetPlacementYawDegrees(FFBuildModeModel::ResolveNextSnappedYawDegrees(
		GetPlacementYawDegrees(),
		GetPlacementRotationSnapDegrees(),
		Direction));
}

void UFF_BuildModeSubsystem::CreateCatalogScreen(APlayerController& Controller)
{
	// SoftClass를 여기서 동기 로드하는 이유는 건축 모드 진입 때만 UI 에셋 비용을 지불하기 위해서다.
	UClass* ResolvedScreenClass = BuildCatalogScreenClass.LoadSynchronous();
	if (!ResolvedScreenClass)
	{
		UE_LOG(LogFFBuildMode, Warning,
			TEXT("WBP_FF_BuildCatalogScreen을 불러오지 못해 네이티브 안전 화면을 사용합니다."));
		// 패키징 누락이나 경로 변경이 있어도 최소 기능을 제공하는 네이티브 화면으로 폴백한다.
		ResolvedScreenClass = UFF_BuildCatalogScreen::StaticClass();
	}

	CatalogScreen = CreateWidget<UFF_BuildCatalogScreen>(&Controller, ResolvedScreenClass);
	if (!CatalogScreen)
	{
		ShowStatus(TEXT("건축 메뉴 위젯 생성에 실패했습니다."), true);
		return;
	}

	// Viewport에 붙이기 전에 WidgetTree를 구성해 BindWidget 결과를 검사한다. 디자이너 위젯의 이름 변경이나
	// 오타로 필수 바인딩이 빠졌다면 빈 화면을 노출하지 않고 네이티브 안전 화면으로 다시 생성한다.
	CatalogScreen->TakeWidget();
	if (ResolvedScreenClass != UFF_BuildCatalogScreen::StaticClass() &&
		!CatalogScreen->IsUsingDesignerWidgetTree())
	{
		UE_LOG(LogFFBuildMode, Error,
			TEXT("WBP_FF_BuildCatalogScreen 바인딩 계약이 잘못되어 네이티브 안전 화면으로 전환합니다."));
		CatalogScreen = CreateWidget<UFF_BuildCatalogScreen>(
			&Controller, UFF_BuildCatalogScreen::StaticClass());
	}
	if (!CatalogScreen)
	{
		// 두 생성 경로가 모두 실패했음을 호출자에게 null로 알린다. OpenCatalog가 입력 모드까지 복구한다.
		ShowStatus(TEXT("건축 메뉴 안전 화면 생성에도 실패했습니다."), true);
		return;
	}

	// AddUObject는 UObject 수명과 연결되지만, 화면을 다시 열 때 중복 구독하지 않도록 제거 함수에서도 명시 해제한다.
	CatalogScreen->OnFacilitySelected.AddUObject(this, &UFF_BuildModeSubsystem::HandleFacilitySelected);
	CatalogScreen->OnCloseRequested.AddUObject(this, &UFF_BuildModeSubsystem::HandleCatalogCloseRequested);
	CatalogScreen->OnBuildModeKeyRequested.AddUObject(
		this, &UFF_BuildModeSubsystem::HandleCatalogBuildModeKeyRequested);
	CatalogScreen->OnPauseMenuKeyRequested.AddUObject(
		this, &UFF_BuildModeSubsystem::HandleCatalogPauseMenuKeyRequested);
	CatalogScreen->AddToPlayerScreen(80);
	CatalogScreen->ActivateWidget();
}

void UFF_BuildModeSubsystem::RemoveCatalogScreen()
{
	if (UFF_BuildCatalogSubsystem* Catalog = GetCatalogSubsystem())
	{
		Catalog->OnCatalogChanged.RemoveAll(this);
		Catalog->OnCatalogThumbnailsChanged.RemoveAll(this);
	}
	// 생성 도중 실패하거나 이미 정리된 경우에도 호출할 수 있는 멱등 정리 함수다.
	if (!CatalogScreen)
	{
		return;
	}
	// 위젯 제거 과정에서 뒤늦은 선택/닫기 이벤트가 상태를 다시 바꾸지 않도록 델리게이트를 먼저 끊는다.
	CatalogScreen->OnFacilitySelected.RemoveAll(this);
	CatalogScreen->OnCloseRequested.RemoveAll(this);
	CatalogScreen->OnBuildModeKeyRequested.RemoveAll(this);
	CatalogScreen->OnPauseMenuKeyRequested.RemoveAll(this);
	CatalogScreen->DeactivateWidget();
	CatalogScreen->RemoveFromParent();
	CatalogScreen = nullptr;
}

void UFF_BuildModeSubsystem::ApplyCatalogIfReady()
{
	if (!CatalogScreen)
	{
		return;
	}
	UFF_BuildCatalogSubsystem* Catalog = GetCatalogSubsystem();
	if (!Catalog)
	{
		return;
	}
	if (Catalog->GetLoadState() == EFF_BuildCatalogLoadState::Failed)
	{
		// 로더가 구체적 오류를 제공하면 보존하고, 없을 때만 사용자 행동(B로 재진입)을 알려 주는 기본 문구를 쓴다.
		const FText Error = Catalog->GetLastError().IsEmpty()
			? FText::FromString(TEXT("시설 목록을 불러오지 못했습니다. B로 닫은 뒤 다시 시도해 주세요."))
			: Catalog->GetLastError();
		CatalogScreen->SetStatusText(Error, true);
		if (!bCatalogWasApplied)
		{
			CatalogScreen->SetCatalogEntries(TArray<FFF_BuildCatalogEntry>());
		}
		bCatalogWasApplied = true;
		bCatalogThumbnailsApplied = true;
		return;
	}
	if (!Catalog->IsCatalogReady())
	{
		// 비동기 로딩이 끝나면 OnCatalogChanged 이벤트가 이 함수를 다시 호출한다.
		CatalogScreen->SetStatusText(FText::FromString(TEXT("시설 목록을 불러오는 중입니다…")));
		return;
	}

	// 준비 완료 시 값 복사본을 UI에 한 번 넘긴다. 썸네일 완료 시에는 카드만 갱신해 선택 분류를 보존한다.
	const TArray<FFF_BuildCatalogEntry>& Entries = Catalog->GetEntriesView();
	if (!bCatalogWasApplied)
	{
		// GI 카탈로그 캐시는 Festival보다 오래 살 수 있으므로 가격을 영구 저장하지 않습니다.
		// 화면을 열 때 현재 Festival의 비용표로 스냅샷을 만들어 설치 정책과 동일한 Resolver를 사용합니다.
		TArray<FFF_BuildCatalogEntry> PricedEntries = Entries;
		UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
		const UFF_BuildCatalogEconomyReadModel* EconomyReadModel = GameInstance
			? GameInstance->GetSubsystem<UFF_BuildCatalogEconomyReadModel>()
			: nullptr;
		const UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();

		for (FFF_BuildCatalogEntry& Entry : PricedEntries)
		{
			Entry.CurrentInstanceCount = ZoneSystem
				? ZoneSystem->CountFacilitiesByDefinitionId(Entry.FacilityDefinitionId)
				: 0;
			Entry.bBuildLimitReached = Entry.MaxInstancesPerWorld > 0 &&
				Entry.CurrentInstanceCount >= Entry.MaxInstancesPerWorld;

			const UFFZSFacilityDefinition* Definition = Entry.Definition.Get();
			FString FailureReason = TEXT("Build catalog economy read model is unavailable.");
			if (EconomyReadModel && EconomyReadModel->TryProjectFacilityCosts(
				Entry, Definition, FailureReason))
			{
				continue;
			}

			UE_LOG(LogFFBuildMode, Warning,
				TEXT("시설 '%s'의 카탈로그 가격을 표시할 수 없습니다: %s"),
				*Entry.FacilityDefinitionId.ToString(),
				*FailureReason);
		}

		CatalogScreen->SetCatalogEntries(PricedEntries);
		bCatalogWasApplied = true;
		const EFF_BuildCatalogThumbnailLoadState ThumbnailState = Catalog->GetThumbnailLoadState();
		bCatalogThumbnailsApplied = Entries.IsEmpty() ||
			ThumbnailState == EFF_BuildCatalogThumbnailLoadState::Ready ||
			ThumbnailState == EFF_BuildCatalogThumbnailLoadState::Failed;
	}
	if (Entries.IsEmpty())
	{
		CatalogScreen->SetStatusText(FText::FromString(TEXT("등록된 시설 정의가 없습니다.")), true);
		return;
	}

	const EFF_BuildCatalogThumbnailLoadState ThumbnailState = Catalog->GetThumbnailLoadState();
	if (ThumbnailState == EFF_BuildCatalogThumbnailLoadState::WaitingForCatalog ||
		ThumbnailState == EFF_BuildCatalogThumbnailLoadState::Loading ||
		ThumbnailState == EFF_BuildCatalogThumbnailLoadState::NotRequested)
	{
		CatalogScreen->SetStatusText(FText::FromString(TEXT("시설 이미지를 불러오는 중입니다…")));
		return;
	}

	if (!bCatalogThumbnailsApplied)
	{
		CatalogScreen->RefreshLoadedThumbnails();
		bCatalogThumbnailsApplied = true;
	}
	if (ThumbnailState == EFF_BuildCatalogThumbnailLoadState::Failed)
	{
		CatalogScreen->SetStatusText(
			FText::FromString(TEXT("일부 시설 이미지를 불러오지 못해 자리표시자로 표시합니다.")),
			true);
		return;
	}
	if (!CanReconfigure())
	{
		// 목록 탐색은 허용하되 현재 게임 단계에서 설치할 수 없음을 선택 전에 명확히 알린다.
		CatalogScreen->SetStatusText(
			FText::FromString(TEXT("현재 단계에서는 시설을 설치할 수 없습니다. 셋업 또는 모닝체크에서 이용하세요.")),
			true);
	}
	else
	{
		CatalogScreen->SetStatusText(FText::FromString(TEXT("시설을 선택하면 배치 모드로 전환됩니다.")));
	}
}

void UFF_BuildModeSubsystem::AcquireCatalogThumbnailSession(UFF_BuildCatalogSubsystem& Catalog)
{
	if (bThumbnailPreloadAcquired)
	{
		return;
	}

	bThumbnailPreloadAcquired = true;
	// Acquire가 상태 델리게이트를 동기 방송할 수 있으므로, 구독자가 즉시 ExitBuildMode를 호출해도
	// Release 경로가 이번 참조를 볼 수 있게 플래그를 먼저 세웁니다.
	Catalog.AcquireThumbnailPreload();
}

void UFF_BuildModeSubsystem::ReleaseCatalogThumbnailSession()
{
	if (!bThumbnailPreloadAcquired)
	{
		return;
	}

	// Release가 썸네일 상태 델리게이트를 동기 방송하므로 소유 플래그를 먼저 내린다. 방송 중
	// 재진입한 새 세션은 자신의 참조를 정상적으로 다시 획득할 수 있다.
	bThumbnailPreloadAcquired = false;
	if (UFF_BuildCatalogSubsystem* Catalog = GetCatalogSubsystem())
	{
		Catalog->ReleaseThumbnailPreload();
	}
}

void UFF_BuildModeSubsystem::BeginBuildSession(APlayerController& Controller)
{
	if (bBuildSessionActive)
	{
		return;
	}

	bBuildSessionActive = true;
	bPlacementGridVisible = true;
	CurrentPositionSnapCm = DefaultPlacementPositionSnapCm;
	bPreviousShowMouseCursor = Controller.bShowMouseCursor;
	PreviousFocusedWidget.Reset();
	if (FSlateApplication::IsInitialized())
	{
		PreviousFocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
	}
	CreateBuildModeGuide(Controller);
}

void UFF_BuildModeSubsystem::CreateBuildModeGuide(APlayerController& Controller)
{
	if (BuildModeGuideWidget)
	{
		return;
	}

	UClass* GuideClass = BuildModeGuideClass.LoadSynchronous();
	if (!GuideClass || !GuideClass->IsChildOf(UFF_BuildModeGuide::StaticClass()))
	{
		return;
	}

	BuildModeGuideWidget = CreateWidget<UFF_BuildModeGuide>(&Controller, GuideClass);
	if (BuildModeGuideWidget)
	{
		BuildModeGuideWidget->AddToPlayerScreen(90);
		RefreshBuildModeGuide();
	}
}

void UFF_BuildModeSubsystem::RefreshBuildModeGuide()
{
	if (!BuildModeGuideWidget)
	{
		return;
	}

	// PrepHUD 배치 패널은 같은 조작 안내를 자체 표시한다. 외부 표현이 활성인 동안 레거시
	// 좌하단 가이드를 함께 띄우면 하단 도킹 패널 뒤에 겹치므로 한 표현 소유자만 남긴다.
	BuildModeGuideWidget->SetVisibility(
		(State == EFF_BuildModeState::CatalogOpen || bExternalCatalogPresentationActive)
		? ESlateVisibility::Collapsed
		: ESlateVisibility::HitTestInvisible);
	BuildModeGuideWidget->SetPlacementGuideState(
		FFBuildModeModel::IsPreviewActive(State),
		State == EFF_BuildModeState::Moving,
		GetPlacementYawDegrees(),
		GetPlacementRotationSnapDegrees(),
		GetPlacementPositionSnapCm(),
		bPlacementGridVisible);
}

void UFF_BuildModeSubsystem::RemoveBuildModeGuide()
{
	if (!BuildModeGuideWidget)
	{
		return;
	}
	BuildModeGuideWidget->RemoveFromParent();
	BuildModeGuideWidget = nullptr;
}

void UFF_BuildModeSubsystem::SetCatalogInputMode(
	APlayerController& Controller,
	TSharedPtr<SWidget> PreferredFocus) const
{
	if (bInputSuspendedForPauseMenu)
	{
		return;
	}
	// GameAndUI를 사용해 B 키 폴링은 유지하면서 위젯의 마우스/키보드 탐색도 가능하게 한다.
	Controller.bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	if (PreferredFocus.IsValid())
	{
		// PrepHUD 외부 카탈로그와 레거시 카탈로그 모두 Pause 진입 직전의 정확한 항목으로 돌아간다.
		InputMode.SetWidgetToFocus(PreferredFocus);
	}
	else if (CatalogScreen)
	{
		// TakeWidget으로 얻은 Slate 위젯을 포커스 대상으로 지정해 첫 키보드 입력이 게임에만 빠지지 않게 한다.
		InputMode.SetWidgetToFocus(CatalogScreen->TakeWidget());
	}
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	Controller.SetInputMode(InputMode);
}

void UFF_BuildModeSubsystem::SetBuildInputMode(
	APlayerController& Controller,
	TSharedPtr<SWidget> PreferredFocus) const
{
	if (bInputSuspendedForPauseMenu)
	{
		return;
	}
	// 카탈로그 없는 건축 상태에서도 커서와 게임 입력을 함께 유지한다. 일반 전환은 viewport를,
	// Pause 복귀는 사용자가 보던 시설 액션을 우선해 키보드와 마우스 흐름을 모두 보존한다.
	Controller.bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	if (PreferredFocus.IsValid())
	{
		// 시설 이동/철거 패널에서 Pause를 열었다면 같은 액션 버튼으로 키보드 포커스를 돌려준다.
		InputMode.SetWidgetToFocus(PreferredFocus);
	}
	else if (const UWorld* World = GetWorld())
	{
		if (const UGameViewportClient* ViewportClient = World->GetGameViewport())
		{
			// 제거된 카탈로그 위젯의 약한 focus path를 남기지 않고 게임 뷰포트를 명시적으로 되찾는다.
			InputMode.SetWidgetToFocus(ViewportClient->GetGameViewportWidget());
		}
	}
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	Controller.SetInputMode(InputMode);
}

void UFF_BuildModeSubsystem::RestoreInputMode(APlayerController& Controller)
{
	// 커서 가시성과 진입 전 UI 포커스를 복원한다. 이전 포커스가 없거나 게임 뷰포트였다면
	// 이 게임의 기본 GameOnly 정책으로 돌아간다.
	Controller.bShowMouseCursor = bPreviousShowMouseCursor;
	const TSharedPtr<SWidget> PreviousFocus = PreviousFocusedWidget.Pin();
	TSharedPtr<SWidget> GameViewportWidget;
	if (const UWorld* World = GetWorld())
	{
		if (const UGameViewportClient* ViewportClient = World->GetGameViewport())
		{
			GameViewportWidget = ViewportClient->GetGameViewportWidget();
		}
	}
	if (PreviousFocus != GameViewportWidget && IsSlateWidgetOnVisiblePath(PreviousFocus))
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(PreviousFocus);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		Controller.SetInputMode(InputMode);
	}
	else
	{
		Controller.SetInputMode(FInputModeGameOnly());
	}
	PreviousFocusedWidget.Reset();
}

void UFF_BuildModeSubsystem::SpawnPlacementPreview()
{
	// 이전 프리뷰가 남아 있는 비정상 호출에도 액터가 중복되지 않게 먼저 정리한다.
	DestroyPlacementPreview();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 프리뷰는 저장/복제 대상이 아니고 실제 충돌 배치가 아니므로 Transient + AlwaysSpawn으로 생성한다.
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PlacementPreview = World->SpawnActor<AFFZSPlacementPreviewActor>(
		AFFZSPlacementPreviewActor::StaticClass(), FTransform::Identity, SpawnParameters);
	if (PlacementPreview)
	{
		// 갱신 주기와 디버그 표현은 이 서브시스템이 통제한다. 첫 유효 표적을 찾기 전까지 숨겨
		// 월드 원점에 프리뷰가 한 프레임 나타나는 현상도 방지한다.
		PlacementPreview->bAutoRefreshPreview = false;
		PlacementPreview->bDrawDebugCells = true;
		PlacementPreview->bDrawPlacementGrid = bPlacementGridVisible;
		const int32 ActiveSnapCm = GetPlacementPositionSnapCm();
		PlacementPreview->PlacementGridStepCm = GetPlacementVisualGridStepCm(ActiveSnapCm);
		PlacementPreview->SetActorHiddenInGame(true);
	}
}

void UFF_BuildModeSubsystem::DestroyPlacementPreview()
{
	// TObjectPtr가 non-null이어도 PendingKill일 수 있으므로 IsValid일 때만 Destroy를 호출한다.
	if (IsValid(PlacementPreview))
	{
		PlacementPreview->Destroy();
	}
	PlacementPreview = nullptr;
}

void UFF_BuildModeSubsystem::ResetPlacementSelection()
{
	SelectedFacilityDefinitionId = NAME_None;
	SelectedSpec = FFFZSFacilitySpec{};
	CurrentPlacement = FFFZSGridPlacement{};
	LastValidation = FFFZSPlacementValidation{};
	CurrentZoneId = NAME_None;
	bHasPlacementTarget = false;
	bPreviewWorldDirty = false;
	PreviewRefreshAccumulator = 0.0f;
	CurrentPositionSnapCm = DefaultPlacementPositionSnapCm;
	ResetPlacementRotationInput();
}

void UFF_BuildModeSubsystem::ResetMovingSelection()
{
	MovingFacility.Reset();
	MovingFacilityInstanceId.Invalidate();
	MoveOriginZoneId = NAME_None;
	MoveOriginPlacement = FFFZSGridPlacement{};
	MoveTransactionId.Invalidate();
	InFlightMoveCommandId.Invalidate();
	bMoveCommitInFlight = false;
}

void UFF_BuildModeSubsystem::SetFacilityRangeVisibility(const bool bVisible) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 상태가 바뀔 때만 순회한다. 배치 프리뷰의 0.08초 갱신 경로에는 전역 액터 순회를 넣지 않는다.
	for (TActorIterator<AFFZSFacilityActor> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			It->SetBuildModeRangeVisible(bVisible);
		}
	}
}

bool UFF_BuildModeSubsystem::TraceGroundUnderCursor(
	APlayerController& Controller,
	FHitResult& OutGroundHit) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FVector RayOrigin = FVector::ZeroVector;
	FVector RayDirection = FVector::ForwardVector;
	if (!Controller.DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		return false;
	}

	// Facility/ProtectedArea 범위 컴포넌트는 NoCollision이며 Zone은 Spline 표현뿐이다. 이전처럼 매 갱신마다
	// 세 액터 종류를 전부 열거해 ignore 목록을 만들지 않고 실제 충돌 가능한 프리뷰와 Pawn만 제외한다.
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FFBuildGroundTrace), false);
	QueryParams.AddIgnoredActor(PlacementPreview);
	if (Controller.GetPawn())
	{
		QueryParams.AddIgnoredActor(Controller.GetPawn());
	}

	const FVector RayEnd = RayOrigin + RayDirection * 500000.0f;
	return World->LineTraceSingleByChannel(
		OutGroundHit, RayOrigin, RayEnd, ECC_Visibility, QueryParams) &&
		OutGroundHit.ImpactNormal.Z >= 0.45f;
}

void UFF_BuildModeSubsystem::SelectFacilityForRemoval(AFFZSFacilityActor& Facility)
{
	if (PendingRemovalFacility.Get() == &Facility)
	{
		return;
	}

	ClearPendingRemovalSelection();
	PendingRemovalFacility = &Facility;
	Facility.SetBuildModeActionSelected(true);
	const FText FacilityDisplayName = Facility.GetFacilitySpec().DisplayName;
	ShowRemovalConfirmation(FacilityDisplayName);
	const FString FacilityName = FacilityDisplayName.ToString();
	ShowStatus(FString::Printf(
		TEXT("%s 선택됨 · M/이동 버튼: 이동 · Enter/철거 버튼: 철거 · 우클릭: 취소"),
		*FacilityName), false, 5.0f);
}

void UFF_BuildModeSubsystem::ClearPendingRemovalSelection()
{
	AFFZSFacilityActor* ClearingFacility = PendingRemovalFacility.Get();
	FGuid ClearingFacilityId;
	if (ClearingFacility)
	{
		ClearingFacilityId = ClearingFacility->GetFacilityInstanceId();
	}
	PendingRemovalFacility.Reset();
	if (ClearingFacility)
	{
		ClearingFacility->SetBuildModeActionSelected(false);
	}
	RemoveRemovalConfirmation(ClearingFacilityId);
}

void UFF_BuildModeSubsystem::ShowRemovalConfirmation(const FText& FacilityName)
{
	(void)FacilityName;
	APlayerController* Controller = GetLocalPlayerController();
	AFFZSFacilityActor* Facility = PendingRemovalFacility.Get();
	UFF_FacilityInfoSubsystem* FacilityInfo = GetFacilityInfoSubsystem();
	if (!Controller || !Facility || !FacilityInfo ||
		!FacilityInfo->SelectFacility(Controller, Facility, EFF_FacilityInfoContext::Build))
	{
		ShowStatus(TEXT("시설 정보 패널을 열 수 없습니다."), true);
	}
}

void UFF_BuildModeSubsystem::RemoveRemovalConfirmation(const FGuid FacilityInstanceId)
{
	if (FacilityInstanceId.IsValid())
	{
		if (UFF_FacilityInfoSubsystem* FacilityInfo = GetFacilityInfoSubsystem())
		{
			FacilityInfo->ClearSelectionIfMatches(
				FacilityInstanceId, EFF_FacilityInfoContext::Build);
		}
	}
}

void UFF_BuildModeSubsystem::ShowFacilityInfoFeedback(
	const FText& Feedback,
	const bool bIsError) const
{
	if (UFF_FacilityInfoSubsystem* FacilityInfo = GetFacilityInfoSubsystem())
	{
		FacilityInfo->SetFeedback(Feedback, bIsError);
	}
}

bool UFF_BuildModeSubsystem::HandleRemovalClickUnderCursor(APlayerController& Controller)
{
	if (!FFBuildModeModel::CanRemoveInstalledFacility(State))
	{
		return false;
	}
	if (!CanReconfigure())
	{
		ShowStatus(TEXT("현재 단계에서는 시설을 철거할 수 없습니다."), true);
		ShowFacilityInfoFeedback(
			FText::FromString(TEXT("현재 단계에서는 철거할 수 없습니다.")), true);
		return false;
	}

	UFF_FacilityInfoSubsystem* FacilityInfo = GetFacilityInfoSubsystem();
	if (!FacilityInfo)
	{
		ShowStatus(TEXT("시설 선택 시스템을 찾을 수 없습니다."), true);
		return false;
	}
	// Live 선택과 같은 padded proxy/가림/등록 검증을 사용한다. 지면 Visibility hit를 정확한
	// footprint에 다시 대입하면 메시가 지면 trace를 막거나 여백을 눌렀을 때 시설을 놓친다.
	AFFZSFacilityActor* Facility = FacilityInfo->ResolveFacilityUnderCursor(Controller);
	if (!Facility)
	{
		if (PendingRemovalFacility.IsValid())
		{
			ClearPendingRemovalSelection();
			ShowStatus(TEXT("시설 선택을 해제했습니다."), false, 1.5f);
		}
		return false;
	}
	if (FFBuildModeModel::ShouldKeepActionSelection(
		State, PendingRemovalFacility.IsValid(), PendingRemovalFacility.Get() == Facility))
	{
		// 선택 실수로 즉시 철거되지 않도록 동일 시설 재클릭은 선택만 유지한다.
		return true;
	}
	if (PendingRemovalFacility.Get() != Facility)
	{
		SelectFacilityForRemoval(*Facility);
		return true;
	}
	return true;
}

bool UFF_BuildModeSubsystem::ConfirmPendingRemoval()
{
	if (bInputSuspendedForPauseMenu)
	{
		return false;
	}
	if (!FFBuildModeModel::CanRemoveInstalledFacility(State))
	{
		return false;
	}
	if (!CanReconfigure())
	{
		ShowStatus(TEXT("현재 단계에서는 시설을 철거할 수 없습니다."), true);
		ShowFacilityInfoFeedback(
			FText::FromString(TEXT("현재 단계에서는 철거할 수 없습니다.")), true);
		return false;
	}

	AFFZSFacilityActor* Facility = PendingRemovalFacility.Get();
	UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
	if (!Facility || !ZoneSystem)
	{
		ClearPendingRemovalSelection();
		ShowStatus(TEXT("작업할 시설을 다시 선택해 주세요."), true);
		return false;
	}

	const FGuid FacilityInstanceId = Facility->GetFacilityInstanceId();
	if (!FacilityInstanceId.IsValid() || ZoneSystem->FindFacility(FacilityInstanceId) != Facility)
	{
		ClearPendingRemovalSelection();
		ShowStatus(TEXT("선택한 시설이 변경되었습니다. 다시 선택해 주세요."), true);
		return false;
	}
	if (!Facility->CanMoveOrRemove())
	{
		ShowStatus(TEXT("운영 중이거나 이용객이 남아 있는 시설은 철거할 수 없습니다."), true);
		ShowFacilityInfoFeedback(
			FText::FromString(TEXT("운영 중이거나 이용객이 남아 있어 철거할 수 없습니다.")), true);
		return false;
	}

	// 오늘 프로그램의 예약은 시설 GUID를 목적지로 보유한다. 이 시설을 없애면 편성은 남고
	// 손님이 갈 Actor만 사라지므로 Remove 권위 명령 직전에 다시 확인한다.
	const UFF_ProgramSchedulingSubsystem* ProgramScheduling = GetWorld()
		? GetWorld()->GetSubsystem<UFF_ProgramSchedulingSubsystem>()
		: nullptr;
	if (ProgramScheduling == nullptr)
	{
		ShowStatus(TEXT("프로그램 편성 상태를 확인할 수 없어 시설을 철거할 수 없습니다."), true);
		ShowFacilityInfoFeedback(
			FText::FromString(TEXT("프로그램 편성 상태를 확인할 수 없어 철거할 수 없습니다.")), true);
		return false;
	}
	if (!FFBuildModeModel::CanRemoveFacilityWithTodaysProgram(
		ProgramScheduling->HasScheduledProgramForFacility(FacilityInstanceId)))
	{
		ShowStatus(TEXT("오늘 프로그램이 편성된 시설은 철거할 수 없습니다."), true);
		ShowFacilityInfoFeedback(
			FText::FromString(TEXT("오늘 프로그램이 편성되어 있어 철거할 수 없습니다.")), true);
		return false;
	}

	const FString FacilityName = Facility->GetFacilitySpec().DisplayName.ToString();
	FFFZSCommand RemoveCommand;
	RemoveCommand.CommandId = FGuid::NewGuid();
	RemoveCommand.TransactionId = FGuid::NewGuid();
	RemoveCommand.ExpectedWorldRevision = ZoneSystem->GetWorldRevision();
	RemoveCommand.Type = EFFZSCommandType::Remove;
	RemoveCommand.FacilityInstanceId = FacilityInstanceId;

	const FFFZSCommandResult Result = ZoneSystem->ExecuteCommand(RemoveCommand);
	if (!Result.bSucceeded)
	{
		ShowStatus(FString::Printf(TEXT("철거 실패: %s"), *Result.Message.ToString()), true);
		ShowFacilityInfoFeedback(Result.Message, true);
		return false;
	}

	ClearPendingRemovalSelection();
	ShowStatus(FString::Printf(TEXT("%s 철거 완료"), *FacilityName), false);
	return true;
}

bool UFF_BuildModeSubsystem::StartMovingSelectedFacility()
{
	if (bInputSuspendedForPauseMenu)
	{
		return false;
	}
	if (bIsExitingBuildMode || State != EFF_BuildModeState::BuildMode)
	{
		return false;
	}

	const auto ReportFailure = [this](const FString& Message, const bool bClearSelection)
	{
		ShowFacilityInfoFeedback(FText::FromString(Message), true);
		ShowStatus(Message, true);
		if (bClearSelection)
		{
			ClearPendingRemovalSelection();
		}
	};

	if (!CanReconfigure())
	{
		ReportFailure(TEXT("현재 단계에서는 시설을 이동할 수 없습니다."), false);
		return false;
	}

	AFFZSFacilityActor* Facility = PendingRemovalFacility.Get();
	UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
	if (!Facility || !ZoneSystem)
	{
		ReportFailure(TEXT("이동할 시설을 다시 선택해 주세요."), true);
		return false;
	}

	const FGuid FacilityInstanceId = Facility->GetFacilityInstanceId();
	const FName SourceZoneId = Facility->GetZoneId();
	if (!FacilityInstanceId.IsValid() || SourceZoneId.IsNone() ||
		ZoneSystem->FindFacility(FacilityInstanceId) != Facility ||
		!ZoneSystem->FindZone(SourceZoneId))
	{
		ReportFailure(TEXT("선택한 시설이 변경되었습니다. 다시 선택해 주세요."), true);
		return false;
	}
	if (!Facility->CanMoveOrRemove())
	{
		ReportFailure(TEXT("운영 중이거나 이용객이 남은 시설은 이동할 수 없습니다."), false);
		return false;
	}

	// 이동 중에는 카탈로그 정의를 다시 조회하지 않는다. 설치 시점에 복사된 권위 액터 Spec을 세션 스냅샷으로 사용한다.
	const FFFZSFacilitySpec FacilitySpec = Facility->GetFacilitySpec();
	const FFFZSGridPlacement SourcePlacement = Facility->GetGridPlacement();
	ResetPlacementSelection();
	ResetMovingSelection();
	MovingFacility = Facility;
	MovingFacilityInstanceId = FacilityInstanceId;
	MoveOriginZoneId = SourceZoneId;
	MoveOriginPlacement = SourcePlacement;
	MoveTransactionId = FGuid::NewGuid();
	SelectedSpec = FacilitySpec;
	CurrentZoneId = SourceZoneId;
	CurrentPlacement = SourcePlacement;
	ClearPendingRemovalSelection();

	SetState(EFF_BuildModeState::Moving);
	SetFacilityRangeVisibility(true);
	SpawnPlacementPreview();
	if (!PlacementPreview)
	{
		SetState(EFF_BuildModeState::BuildMode);
		ResetPlacementSelection();
		ResetMovingSelection();
		ShowStatus(TEXT("시설 이동 프리뷰를 생성하지 못했습니다."), true);
		return false;
	}
	if (APlayerController* Controller = GetLocalPlayerController())
	{
		SetBuildInputMode(*Controller);
		bSuppressWorldClickUntilReleased = true;
		bSuppressPlacementConfirmUntilReleased = true;
	}
	UpdatePlacementFromCursor(true);
	ShowStatus(TEXT("좌클릭/Enter 이동 · Q/E 회전 · Ctrl 정밀 · Alt 자유 · G 격자 · 우클릭 이동 취소"));
	return State == EFF_BuildModeState::Moving && PlacementPreview != nullptr;
}

void UFF_BuildModeSubsystem::CancelMovingToBuildMode(
	const FString& StatusMessage,
	const bool bIsError)
{
	if (State != EFF_BuildModeState::Moving)
	{
		return;
	}

	SetState(FFBuildModeModel::ResolvePreviewCancelState(State));
	DestroyPlacementPreview();
	ResetPlacementSelection();
	ResetMovingSelection();
	bSuppressPlacementConfirmUntilReleased = false;
	SetFacilityRangeVisibility(true);
	if (APlayerController* Controller = GetLocalPlayerController())
	{
		SetBuildInputMode(*Controller);
	}
	if (!StatusMessage.IsEmpty())
	{
		ShowStatus(StatusMessage, bIsError, bIsError ? 3.0f : 1.5f);
	}
}

void UFF_BuildModeSubsystem::HandleMoveRequested(const FGuid FacilityInstanceId)
{
	if (bInputSuspendedForPauseMenu || !PendingRemovalFacility.IsValid() ||
		PendingRemovalFacility->GetFacilityInstanceId() != FacilityInstanceId)
	{
		return;
	}
	bSuppressWorldClickUntilReleased = true;
	bSuppressPlacementConfirmUntilReleased = true;
	StartMovingSelectedFacility();
}

void UFF_BuildModeSubsystem::HandleRemoveRequested(const FGuid FacilityInstanceId)
{
	if (bInputSuspendedForPauseMenu || !PendingRemovalFacility.IsValid() ||
		PendingRemovalFacility->GetFacilityInstanceId() != FacilityInstanceId)
	{
		return;
	}
	bSuppressWorldClickUntilReleased = true;
	bSuppressPlacementConfirmUntilReleased = true;
	ConfirmPendingRemoval();
}

void UFF_BuildModeSubsystem::HandleRemovalCancelRequested(const FGuid FacilityInstanceId)
{
	if (bInputSuspendedForPauseMenu || !PendingRemovalFacility.IsValid() ||
		PendingRemovalFacility->GetFacilityInstanceId() != FacilityInstanceId)
	{
		return;
	}
	bSuppressWorldClickUntilReleased = true;
	bSuppressPlacementConfirmUntilReleased = true;
	ClearPendingRemovalSelection();
	ShowStatus(TEXT("시설 선택을 취소했습니다."), false, 1.5f);
}

bool UFF_BuildModeSubsystem::UpdatePlacementFromCursor(const bool bForceCommandRebuild)
{
	// 이 함수는 활성 프리뷰의 커서 스냅샷만 갱신한다. 필요한 객체가 없으면
	// 기존 배치 세션을 건드리지 않고 갱신 실패만 반환한다.
	APlayerController* Controller = GetLocalPlayerController();
	UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
	if (!FFBuildModeModel::IsPreviewActive(State) ||
		!Controller || !ZoneSystem || !PlacementPreview)
	{
		return false;
	}

	if (State == EFF_BuildModeState::Placing)
	{
		// 신규 설치는 권위 프리뷰를 만들 때마다 Primary Data Asset을 다시 읽어 편집 중 변경을 반영한다.
		UFF_BuildCatalogSubsystem* Catalog = GetCatalogSubsystem();
		const UFFZSFacilityDefinition* SelectedDefinition = Catalog
			? Catalog->FindDefinition(SelectedFacilityDefinitionId)
			: nullptr;
		TArray<FText> DefinitionErrors;
		if (!SelectedDefinition || !SelectedDefinition->IsDefinitionValid(DefinitionErrors))
		{
			bHasPlacementTarget = false;
			PlacementPreview->SetActorHiddenInGame(true);
			ShowStatus(TEXT("선택한 시설 정의를 사용할 수 없습니다."), true);
			return false;
		}
		SelectedSpec = SelectedDefinition->Spec;
	}
	else
	{
		// Moving은 설치 시점의 액터 Spec을 고정 스냅샷으로 쓴다. 카탈로그 정의를 재조회하면 기존 인스턴스와 메시가 달라질 수 있다.
		AFFZSFacilityActor* Facility = MovingFacility.Get();
		if (!Facility || !MovingFacilityInstanceId.IsValid() ||
			Facility->GetFacilityInstanceId() != MovingFacilityInstanceId ||
			ZoneSystem->FindFacility(MovingFacilityInstanceId) != Facility)
		{
			CancelMovingToBuildMode(TEXT("이동 대상이 변경되어 이동을 취소했습니다."), true);
			return false;
		}
	}

	FHitResult GroundHit;
	if (!TraceGroundUnderCursor(*Controller, GroundHit))
	{
		// 지면을 못 찾거나 지나치게 수직인 표면은 배치 좌표로 쓰지 않는다. 프리뷰를 숨겨 설치 가능하다는 오해를 막는다.
		bHasPlacementTarget = false;
		PlacementPreview->SetActorHiddenInGame(true);
		ShowStatus(TEXT("바닥 위로 마우스를 옮겨 주세요."), true,
			PreviewRefreshIntervalSeconds * 2.5f);
		return false;
	}

	// 경계에서 Zone이 매 프레임 흔들리지 않도록 현재 Zone을 힌트로 넘긴 뒤, 실제 등록된 Zone인지 다시 확인한다.
	const FName NewZoneId = ZoneSystem->ResolveZoneAtWorldLocation(GroundHit.ImpactPoint, CurrentZoneId);
	AFFZSGameplayZone* Zone = ZoneSystem->FindZone(NewZoneId);
	if (!Zone)
	{
		// 유효 Zone을 잃었음을 명시해 다음 유효 위치가 같은 원점이어도 bChanged가 true가 되게 한다.
		bHasPlacementTarget = false;
		CurrentZoneId = NAME_None;
		PlacementPreview->SetActorHiddenInGame(true);
		ShowStatus(TEXT("건축 구역 안쪽으로 옮겨 주세요."), true,
			PreviewRefreshIntervalSeconds * 2.5f);
		return false;
	}
	PlacementPreview->SetPlacementGridSurface(
		GroundHit.ImpactPoint,
		GroundHit.ImpactNormal);

	// Zone 권위 격자는 1cm 정수 좌표를 유지하고, 커서 조작만 DeveloperSettings의 간격으로 양자화한다.
	FIntPoint TargetCell = Zone->WorldToGridCell1Cm(GroundHit.ImpactPoint);
	const int32 PositionSnapCm = GetPlacementPositionSnapCm();
	const bool bSnapModeChanged = PositionSnapCm != CurrentPositionSnapCm;
	CurrentPositionSnapCm = PositionSnapCm;
	PlacementPreview->bDrawPlacementGrid = bPlacementGridVisible;
	PlacementPreview->PlacementGridStepCm = GetPlacementVisualGridStepCm(PositionSnapCm);
	TargetCell.X = FMath::RoundToInt(static_cast<double>(TargetCell.X) / PositionSnapCm) * PositionSnapCm;
	TargetCell.Y = FMath::RoundToInt(static_cast<double>(TargetCell.Y) / PositionSnapCm) * PositionSnapCm;

	if (SelectedSpec.FootprintSizeCells.X <= 0 || SelectedSpec.FootprintSizeCells.Y <= 0)
	{
		// 크기가 0 이하인 잘못된 정의는 여기서도 방어해 유효하지 않은 PreviewCommand 생성을 막는다.
		bHasPlacementTarget = false;
		PlacementPreview->SetActorHiddenInGame(true);
		return false;
	}

	// 첫 셀 중심 (0.5, 0.5)을 기준으로 (FootprintSize - 1) / 2 벡터만 임의 yaw만큼
	// 회전한다. QuarterTurns용 외접 Rect 중심을 사용하면 45도에서 커서와 시설 중심이 어긋난다.
	const FVector2D CenterOffset = FFBuildModeModel::ResolveRotatedFootprintCenterOffset(
		SelectedSpec.FootprintSizeCells,
		GetPlacementYawDegrees());
	const FIntPoint NewOrigin(
		FMath::RoundToInt(static_cast<double>(TargetCell.X) - CenterOffset.X),
		FMath::RoundToInt(static_cast<double>(TargetCell.Y) - CenterOffset.Y));

	// 표적이 한 번 끊겼다가 같은 좌표로 돌아온 경우도 다시 표시/검증해야 하므로 이전 bHasPlacementTarget을 비교한다.
	const bool bChanged = NewOrigin != CurrentPlacement.OriginCell ||
		NewZoneId != CurrentZoneId || !bHasPlacementTarget || bSnapModeChanged;
	CurrentPlacement.OriginCell = NewOrigin;
	CurrentZoneId = NewZoneId;
	bHasPlacementTarget = true;
	PlacementPreview->SetActorHiddenInGame(false);
	if (!bChanged && !bForceCommandRebuild)
	{
		// 위치가 그대로면 비싼 command 생성과 정책 검증을 건너뛴다. 프리뷰의 현재 표시 상태는 이미 유효하다.
		return false;
	}

	// PreviewCommand가 현재 선택/Zone/배치/revision을 담도록 만든 후, Zone System의 동일 검증 경로로 시각 상태를 갱신한다.
	RebuildPreviewCommand();
	LastValidation = PlacementPreview->RefreshPreview();
	const bool bIsMoving = State == EFF_BuildModeState::Moving;
	const EFFZSPlacementFailure PrimaryFailure = LastValidation.Reasons.IsEmpty()
		? EFFZSPlacementFailure::None
		: LastValidation.Reasons[0];
	const FText UserFacingFailure =
		UFF_BuildPlacementFailureFeedback::MakeUserFacingReason(
			PrimaryFailure,
			LastValidation.Summary,
			bIsMoving);
	ShowStatus(LastValidation.bIsValid
		? bIsMoving
			? TEXT("이동 가능 · 좌클릭/Enter로 확정")
			: TEXT("배치 가능 · 좌클릭/Enter로 설치")
		: FString::Printf(TEXT("%s 불가 · %s"),
			bIsMoving ? TEXT("이동") : TEXT("배치"),
			*UserFacingFailure.ToString()),
		!LastValidation.bIsValid,
		PreviewRefreshIntervalSeconds * 2.5f);
	return true;
}

void UFF_BuildModeSubsystem::RebuildPreviewCommand()
{
	UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
	if (!PlacementPreview || !ZoneSystem || CurrentZoneId.IsNone())
	{
		return;
	}

	if (State == EFF_BuildModeState::Moving)
	{
		if (!MovingFacilityInstanceId.IsValid() || !MoveTransactionId.IsValid())
		{
			return;
		}

		FFFZSCommand MoveCommand;
		MoveCommand.CommandId = FGuid::NewGuid();
		MoveCommand.TransactionId = MoveTransactionId;
		MoveCommand.ExpectedWorldRevision = ZoneSystem->GetWorldRevision();
		MoveCommand.Type = EFFZSCommandType::Move;
		MoveCommand.FacilityInstanceId = MovingFacilityInstanceId;
		// 프리뷰 Actor가 FacilitySpec.VisualMesh를 직접 읽으므로 기존 액터에서 복사한 전체 Spec을 반드시 담는다.
		MoveCommand.FacilitySpec = SelectedSpec;
		MoveCommand.ZoneId = CurrentZoneId;
		MoveCommand.Placement = CurrentPlacement;
		PlacementPreview->PreviewCommand = MoveTemp(MoveCommand);
		return;
	}

	if (State != EFF_BuildModeState::Placing)
	{
		return;
	}
	UFF_BuildCatalogSubsystem* Catalog = GetCatalogSubsystem();
	const UFFZSFacilityDefinition* Definition = Catalog
		? Catalog->FindDefinition(SelectedFacilityDefinitionId)
		: nullptr;
	if (!Definition)
	{
		return;
	}

	// 월드 revision을 명령에 넣어 프리뷰 이후 다른 설치/철거가 발생한 오래된 명령이 Commit에서 통과하지 않게 한다.
	SelectedSpec = Definition->Spec;
	PlacementPreview->PreviewCommand = UFFZSGridLibrary::MakeInstallCommandFromDefinition(
		Definition,
		CurrentZoneId,
		CurrentPlacement,
		ZoneSystem->GetWorldRevision());
}

float UFF_BuildModeSubsystem::GetPlacementRotationSnapDegrees() const
{
	const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
	const float Value = Settings
		? Settings->PlacementRotationSnapDegrees
		: DefaultPlacementRotationSnapDegrees;
	return FMath::IsFinite(Value) && Value >= 0.01f && Value <= 180.0f
		? Value
		: DefaultPlacementRotationSnapDegrees;
}

float UFF_BuildModeSubsystem::GetFreePlacementRotationSpeedDegreesPerSecond() const
{
	const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
	const float Value = Settings
		? Settings->FreePlacementRotationSpeedDegreesPerSecond
		: DefaultFreePlacementRotationSpeedDegreesPerSecond;
	return FMath::IsFinite(Value) && Value >= 1.0f && Value <= 720.0f
		? Value
		: DefaultFreePlacementRotationSpeedDegreesPerSecond;
}

int32 UFF_BuildModeSubsystem::GetPlacementPositionSnapCm() const
{
	const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
	const int32 DefaultSnapCm = Settings
		? Settings->PlacementPositionSnapCm
		: DefaultPlacementPositionSnapCm;
	const int32 PrecisionSnapCm = Settings
		? Settings->PrecisionPlacementPositionSnapCm
		: DefaultPrecisionPlacementPositionSnapCm;
	const int32 FreeSnapCm = Settings
		? Settings->FreePlacementPositionSnapCm
		: DefaultFreePlacementPositionSnapCm;
	const APlayerController* Controller = GetLocalPlayerController();
	const bool bPrecisionModifierDown = Controller &&
		(Controller->IsInputKeyDown(EKeys::LeftControl) ||
			Controller->IsInputKeyDown(EKeys::RightControl));
	const bool bFreeModifierDown = Controller &&
		(Controller->IsInputKeyDown(EKeys::LeftAlt) ||
			Controller->IsInputKeyDown(EKeys::RightAlt));
	return FFBuildModeModel::ResolvePlacementPositionSnapCm(
		bPrecisionModifierDown,
		bFreeModifierDown,
		DefaultSnapCm,
		PrecisionSnapCm,
		FreeSnapCm);
}

int32 UFF_BuildModeSubsystem::GetPlacementVisualGridStepCm(
	const int32 ActivePositionSnapCm) const
{
	const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
	const int32 DefaultSnapCm = Settings
		? FFBuildModeModel::SanitizePositionSnapCm(
			Settings->PlacementPositionSnapCm,
			DefaultPlacementPositionSnapCm)
		: DefaultPlacementPositionSnapCm;
	return FFBuildModeModel::ResolveVisualGridStepCm(
		ActivePositionSnapCm,
		DefaultSnapCm);
}

bool UFF_BuildModeSubsystem::CanReconfigure() const
{
	const UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
	if (!ZoneSystem)
	{
		return false;
	}
	// 시설 변경 가능 단계는 Zone System이 소유한 정책을 그대로 따른다. UI에서 별도 규칙을 만들지 않는다.
	const EFFZSChangePhase Phase = ZoneSystem->GetChangePhase();
	return Phase == EFFZSChangePhase::InitialSetup || Phase == EFFZSChangePhase::MorningCheck;
}

bool UFF_BuildModeSubsystem::IsBuildEntryAllowed() const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>()
		: nullptr;
	return FFBuildModeModel::CanEnterBuildSession(
		Stage && Stage->IsReconfigureAllowed(), CanReconfigure());
}

void UFF_BuildModeSubsystem::HandleFacilitySelected(const FName FacilityDefinitionId)
{
	// 델리게이트 반환값은 없으므로 실패 안내는 StartPlacement가 현재 카탈로그 화면에 직접 표시한다.
	StartPlacement(FacilityDefinitionId);
}

void UFF_BuildModeSubsystem::HandleCatalogCloseRequested()
{
	// 닫기 버튼은 카탈로그만 접고 범위 확인/철거가 가능한 건축 상태를 유지한다.
	EnterBuildMode();
}

void UFF_BuildModeSubsystem::HandleCatalogBuildModeKeyRequested()
{
	// Slate가 처리한 B와 raw PlayerController 상태가 같은 프레임에 모두 관찰되더라도 한 번만 복귀한다.
	bSuppressBuildToggleUntilReleased = true;
	EnterBuildMode();
}

void UFF_BuildModeSubsystem::HandleCatalogPauseMenuKeyRequested()
{
	if (UFF_FestivalSettingsSubsystem* Settings = UFF_FestivalSettingsSubsystem::Get(this))
	{
		Settings->OpenPauseMenu(GetLocalPlayerController());
	}
}

void UFF_BuildModeSubsystem::HandleCatalogChanged()
{
	if (State != EFF_BuildModeState::CatalogOpen || !CatalogScreen)
	{
		return;
	}

	// 열린 화면에서 명시적 RefreshCatalog가 끝난 경우에도 새 스냅샷을 한 번 다시 적용합니다.
	bCatalogWasApplied = false;
	bCatalogThumbnailsApplied = false;
	ApplyCatalogIfReady();
}

void UFF_BuildModeSubsystem::HandleCatalogThumbnailsChanged()
{
	if (State != EFF_BuildModeState::CatalogOpen || !CatalogScreen)
	{
		return;
	}

	bCatalogThumbnailsApplied = false;
	ApplyCatalogIfReady();
}

void UFF_BuildModeSubsystem::HandleWorldChanged(const FFFZSWorldChangeEvent& Event)
{
	if (!FFBuildModeModel::IsBuildSessionActive(State))
	{
		return;
	}

	if (FFBuildModeModel::IsPreviewActive(State))
	{
		// 콜백 안에서 정책 검증에 재진입하지 않고 다음 제한 주기에서 최신 revision으로 다시 만든다.
		bPreviewWorldDirty = true;
	}
	else if (State == EFF_BuildModeState::CatalogOpen && CatalogScreen &&
		(Event.Type == EFFZSCommandType::Install || Event.Type == EFFZSCommandType::Remove))
	{
		// 수량이 변하는 설치·철거에만 열린 목록을 다시 투영한다. 이동·운영 상태 변경은 건축 한도와 무관하다.
		bCatalogWasApplied = false;
		ApplyCatalogIfReady();
	}

	const bool bTargetsMovingFacility = State == EFF_BuildModeState::Moving &&
		MovingFacilityInstanceId.IsValid() &&
		Event.FacilityInstanceId == MovingFacilityInstanceId;
	const bool bOwnMoveEvent = bTargetsMovingFacility &&
		Event.Type == EFFZSCommandType::Move && bMoveCommitInFlight &&
		InFlightMoveCommandId.IsValid() && Event.CommandId == InFlightMoveCommandId;
	if (bTargetsMovingFacility && !bOwnMoveEvent &&
		(Event.Type == EFFZSCommandType::Move || Event.Type == EFFZSCommandType::Remove))
	{
		// 타 시스템이 대상을 먼저 이동·삭제하면 이전 원본 스냅샷으로 커밋하지 않도록 즉시 취소한다.
		CancelMovingToBuildMode(TEXT("이동 대상이 외부에서 변경되어 현재 이동을 취소했습니다."), true);
	}
	if ((Event.Type == EFFZSCommandType::Remove || Event.Type == EFFZSCommandType::Move) &&
		PendingRemovalFacility.IsValid() &&
		PendingRemovalFacility->GetFacilityInstanceId() == Event.FacilityInstanceId)
	{
		ClearPendingRemovalSelection();
	}

	if (Event.Type != EFFZSCommandType::Install && Event.Type != EFFZSCommandType::Move)
	{
		return;
	}
	if (UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem())
	{
		if (AFFZSFacilityActor* Facility = ZoneSystem->FindFacility(Event.FacilityInstanceId))
		{
			// 전체 시설을 다시 훑지 않고 새로 설치되거나 이동한 한 시설의 선 캐시만 동기화한다.
			Facility->SetBuildModeRangeVisible(true);
		}
	}
}

void UFF_BuildModeSubsystem::HandleWorldSnapshotRestored()
{
	if (!FFBuildModeModel::IsBuildSessionActive(State))
	{
		return;
	}

	// 스냅샷 복원은 시설 Actor 레지스트리를 원자적으로 교체하므로 이전 Actor를 가리키는 확인 대상을 먼저 비운다.
	ClearPendingRemovalSelection();
	if (State == EFF_BuildModeState::Moving)
	{
		CancelMovingToBuildMode(TEXT("월드 복원으로 이동 대상이 교체되어 이동을 취소했습니다."), true);
		return;
	}
	SetFacilityRangeVisibility(true);
	if (State == EFF_BuildModeState::CatalogOpen && CatalogScreen)
	{
		bCatalogWasApplied = false;
		ApplyCatalogIfReady();
		return;
	}
	if (State == EFF_BuildModeState::Placing)
	{
		bPreviewWorldDirty = true;
	}
}

void UFF_BuildModeSubsystem::PresentPlacementFailure(
	const EFFZSPlacementFailure FailureReason,
	const FText& Detail)
{
	UFF_BuildPlacementFailureFeedback* Feedback = GetOrCreatePlacementFailureFeedback();
	if (!Feedback)
	{
		return;
	}

	const EFF_BuildPlacementFailureType FailureType =
		UFF_BuildPlacementFailureFeedback::ClassifyFailure(FailureReason);
	const FText Title = UFF_BuildPlacementFailureFeedback::MakeFailureTitle(
		FailureReason, State == EFF_BuildModeState::Moving);

	const FText SafeDetail = Detail.IsEmpty()
		? UFFZSGridLibrary::PlacementFailureToText(FailureReason)
		: Detail;
	const FText UserFacingDetail = UFF_BuildPlacementFailureFeedback::MakeUserFacingReason(
		FailureReason,
		SafeDetail,
		State == EFF_BuildModeState::Moving);
	Feedback->PresentFailure(FailureType, Title, UserFacingDetail);
}

UFF_BuildPlacementFailureFeedback*
UFF_BuildModeSubsystem::GetOrCreatePlacementFailureFeedback()
{
	if (PlacementFailureFeedback)
	{
		return PlacementFailureFeedback;
	}

	APlayerController* Controller = GetLocalPlayerController();
	UClass* FeedbackClass = PlacementFailureFeedbackClass.LoadSynchronous();
	if (!Controller || !FeedbackClass ||
		!FeedbackClass->IsChildOf(UFF_BuildPlacementFailureFeedback::StaticClass()))
	{
		UE_LOG(LogFFBuildMode, Error,
			TEXT("건축 실패 알림 WBP를 사용할 수 없습니다: %s"),
			*PlacementFailureFeedbackClass.ToSoftObjectPath().ToString());
		return nullptr;
	}

	PlacementFailureFeedback =
		CreateWidget<UFF_BuildPlacementFailureFeedback>(Controller, FeedbackClass);
	if (!PlacementFailureFeedback)
	{
		UE_LOG(LogFFBuildMode, Error, TEXT("건축 실패 알림 WBP 인스턴스 생성에 실패했습니다."));
		return nullptr;
	}

	PlacementFailureFeedback->SetVisibility(ESlateVisibility::Collapsed);
	if (!PlacementFailureFeedback->AddToPlayerScreen(110))
	{
		UE_LOG(LogFFBuildMode, Error, TEXT("건축 실패 알림 WBP를 플레이어 화면에 추가하지 못했습니다."));
		PlacementFailureFeedback = nullptr;
	}
	return PlacementFailureFeedback;
}

void UFF_BuildModeSubsystem::DismissPlacementFailureFeedback(const bool bRemoveFromParent)
{
	if (!PlacementFailureFeedback)
	{
		return;
	}

	PlacementFailureFeedback->DismissFeedback();
	if (bRemoveFromParent)
	{
		PlacementFailureFeedback->RemoveFromParent();
		PlacementFailureFeedback = nullptr;
	}
}

void UFF_BuildModeSubsystem::ShowStatus(
	const FString& Message,
	const bool bIsError,
	const float DisplaySeconds) const
{
	if (Message != LastLoggedStatusMessage)
	{
		// 커서 검증은 초당 여러 번 같은 결과를 만들 수 있다. 화면 표시는 갱신하되 파일/콘솔 로그는 문구가 바뀔 때만 남긴다.
		LastLoggedStatusMessage = Message;
		UE_LOG(LogFFBuildMode, Log, TEXT("%s"), *Message);
	}
	if (GEngine)
	{
		// 고정 키를 사용해 이전 상태 문구를 같은 슬롯에서 교체하고 메시지가 화면에 누적되지 않게 한다.
		GEngine->AddOnScreenDebugMessage(
			620013,
			DisplaySeconds,
			bIsError ? FColor::Red : FColor::Cyan,
			Message);
	}
}

APlayerController* UFF_BuildModeSubsystem::GetLocalPlayerController() const
{
	// 현재 건축 UI는 단일 로컬 플레이어용이다. 첫 컨트롤러가 원격이면 서버에서 UI/입력을 만들지 않는다.
	UWorld* World = GetWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	return Controller && Controller->IsLocalController() ? Controller : nullptr;
}

UFF_BuildCatalogSubsystem* UFF_BuildModeSubsystem::GetCatalogSubsystem() const
{
	// 카탈로그는 월드보다 긴 GameInstance 수명의 캐시이므로 GameInstance를 통해 조회한다.
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UFF_BuildCatalogSubsystem>() : nullptr;
}

UFFZSWorldSubsystem* UFF_BuildModeSubsystem::GetZoneSystem() const
{
	// Zone 상태와 revision은 월드별이므로 현재 월드의 WorldSubsystem을 사용한다.
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
}

void UFF_BuildModeSubsystem::HandleFestivalStageChanged(
	const EFF_FestivalStage PreviousStage,
	const EFF_FestivalStage NewStage)
{
	(void)PreviousStage;
	(void)NewStage;
	if (FFBuildModeModel::ShouldExitWhenReconfigureCloses(
		State, IsBuildEntryAllowed()))
	{
		// 건축 세션이 마우스를 소유하는 동안 Live 선택은 의도적으로 차단한다. 단계 전환 시
		// 오래된 건축 UI를 남기지 않도록 세션 전체를 원자적으로 닫는다.
		ExitBuildMode();
	}
}

UFF_FacilityInfoSubsystem* UFF_BuildModeSubsystem::GetFacilityInfoSubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UFF_FacilityInfoSubsystem>() : nullptr;
}
