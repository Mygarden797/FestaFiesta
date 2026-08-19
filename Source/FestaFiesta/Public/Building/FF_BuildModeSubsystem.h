// FF_BuildModeSubsystem.h

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "Time/FF_FestivalStage.h"
#include "FF_BuildModeSubsystem.generated.h"

class AFFZSFacilityActor;
class AFFZSPlacementPreviewActor;
class APlayerController;
class UFF_BuildCatalogScreen;
class UFF_BuildCatalogSubsystem;
class UFF_BuildModeGuide;
class UFF_BuildPlacementFailureFeedback;
class UFF_FacilityInfoSubsystem;
class UFFZSWorldSubsystem;
class SWidget;
struct FHitResult;

UENUM(BlueprintType)
enum class EFF_BuildModeState : uint8
{
	// 건축 UI와 배치 프리뷰가 모두 없고, 플레이어 입력 모드가 원래 상태로 복원된 상태. 
	Closed,
	// 카탈로그 없이 설치된 시설의 범위를 확인하고 시설을 제거할 수 있는 기본 건축 상태.
	BuildMode,
	// 시설 목록 UI가 열려 있으며 선택 이벤트를 기다리는 상태. 
	CatalogOpen,
	// 선택한 시설의 프리뷰를 커서 아래 Zone에 투영하고 설치 입력을 받는 상태. 
	Placing,
	// 기존 시설의 원본은 유지한 채 새 배치를 프리뷰하고 Move 확정 입력을 받는 상태.
	Moving
};

// 입력과 부작용을 분리해 자동화 테스트에서도 실제 B 키 상태표를 검증할 수 있게 한다.
namespace FFBuildModeModel
{
	inline EFF_BuildModeState ResolveBuildKeyState(const EFF_BuildModeState CurrentState)
	{
		switch (CurrentState)
		{
		case EFF_BuildModeState::Closed:
		case EFF_BuildModeState::CatalogOpen:
			return EFF_BuildModeState::BuildMode;
		case EFF_BuildModeState::BuildMode:
		case EFF_BuildModeState::Placing:
		case EFF_BuildModeState::Moving:
			return EFF_BuildModeState::CatalogOpen;
		default:
			return EFF_BuildModeState::Closed;
		}
	}

	inline bool IsBuildSessionActive(const EFF_BuildModeState State)
	{
		return State != EFF_BuildModeState::Closed;
	}

	inline bool CanRemoveInstalledFacility(const EFF_BuildModeState State)
	{
		return State == EFF_BuildModeState::BuildMode;
	}

	inline bool IsPreviewActive(const EFF_BuildModeState State)
	{
		return State == EFF_BuildModeState::Placing ||
			State == EFF_BuildModeState::Moving;
	}

	inline EFF_BuildModeState ResolvePreviewCancelState(const EFF_BuildModeState State)
	{
		return State == EFF_BuildModeState::Moving
			? EFF_BuildModeState::BuildMode
			: State == EFF_BuildModeState::Placing
				? EFF_BuildModeState::CatalogOpen
				: State;
	}

	inline bool IsCurrentMoveSession(
		const EFF_BuildModeState State,
		const FGuid& CurrentFacilityId,
		const FGuid& CurrentTransactionId,
		const FGuid& ExpectedFacilityId,
		const FGuid& ExpectedTransactionId)
	{
		return State == EFF_BuildModeState::Moving &&
			ExpectedFacilityId.IsValid() && ExpectedTransactionId.IsValid() &&
			CurrentFacilityId == ExpectedFacilityId &&
			CurrentTransactionId == ExpectedTransactionId;
	}

	inline bool ShouldKeepActionSelection(
		const EFF_BuildModeState State,
		const bool bHasPendingFacility,
		const bool bClickedPendingFacility)
	{
		return CanRemoveInstalledFacility(State) &&
			bHasPendingFacility && bClickedPendingFacility;
	}

	inline bool ShouldProcessRawBuildKey(
		const bool bUiBuildKeyHandledThisFrame,
		const bool bRawBuildKeyJustPressed)
	{
		return !bUiBuildKeyHandledThisFrame && bRawBuildKeyJustPressed;
	}

	inline bool ShouldExitOnEscape(const EFF_BuildModeState State)
	{
		return IsBuildSessionActive(State);
	}

	inline bool ShouldExitWhenReconfigureCloses(
		const EFF_BuildModeState State,
		const bool bReconfigureAllowed)
	{
		return IsBuildSessionActive(State) && !bReconfigureAllowed;
	}

	inline bool CanEnterBuildSession(
		const bool bFestivalStageAllowsReconfigure,
		const bool bZonePhaseAllowsReconfigure)
	{
		return bFestivalStageAllowsReconfigure && bZonePhaseAllowsReconfigure;
	}
}

// B 키, 카탈로그, 시설 배치/제거를 연결하는 플레이어용 월드 서브시스템.

// UI는 시설 정의를 선택할 뿐이며 실제 설치는 FF Zone System의 PreviewCommand와
// ExecuteCommand 경로만 사용한다. 따라서 단계·충돌·보호 동선·정책 검증을 우회하지 않는다.

// 상태 전환은 `Closed -> BuildMode <-> CatalogOpen -> Placing`과
// `BuildMode -> Moving -> BuildMode`를 따른다. B는 활성 프리뷰를 정리하고 카탈로그를
// 열고 닫으며 Esc만 세션 전체를 종료한다. BuildMode에서는 설치 시설의 예약 범위를 보여 주고
// 선택 후 액션 패널에서 이동·철거·취소를 선택한다. Placing과 Moving은 제한된 주기로 커서 위치를 다시 검증한다.
 
UCLASS()
class FESTAFIESTA_API UFF_BuildModeSubsystem final : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// 1cm 단위 Zone 그리드에서 배치 원점을 맞추는 10cm 스냅 간격.
	// 커서 입력만 양자화하며 권한 격자 자체는 1cm로 유지된다. QA 플레이테스트 경로도 같은 값을 써야
	// 실제 건축 모드와 배치 결과가 어긋나지 않는다.
	static constexpr int32 PlacementSnapCm = 10;

	// 지원 월드인지와 QA 전용 맵인지 확인해 이 서브시스템을 생성할지를 결정한다.
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	// 실제 게임 플레이 입력을 제공하는 Game/PIE/GamePreview 월드만 허용한다. 
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	// Zone System을 먼저 초기화하도록 의존성을 등록한다. 
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	// UI·프리뷰를 제거하고 건축 세션이 바꾼 입력 상태를 원복한다. 
	virtual void Deinitialize() override;
	// 입력을 처리하고, Placing/Moving에서 제한된 주기로 커서 기반 프리뷰를 갱신한다.
	virtual void Tick(float DeltaTime) override;
	// Tickable 통계에서 이 서브시스템의 Tick 비용을 식별할 stat ID를 반환한다. 
	virtual TStatId GetStatId() const override;

	// 현재 상태를 반환한다. 이 값으로 카탈로그/배치/종료 UI를 구분할 수 있다. 
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building")
	EFF_BuildModeState GetBuildModeState() const { return State; }

	// B 키 상태표에 따라 Closed/CatalogOpen은 BuildMode로, BuildMode/Placing/Moving은 CatalogOpen으로 전환한다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building")
	void ToggleBuildMode();

	// 카탈로그가 닫힌 기본 건축 상태로 들어간다. 시설 범위와 이동·철거 작업 선택을 활성화한다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building")
	void EnterBuildMode();

	// 현재 건축 세션에서 카탈로그를 연다. 실패하면 전체 세션을 끝내지 않고 BuildMode를 유지한다.
	 
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building")
	void OpenCatalog();

	
	// 카탈로그 ID에 해당하는 시설 정의를 검증하고 Placing 상태로 전환한다.
	// 정의·단계·프리뷰 생성이 모두 유효해 배치를 시작했으면 true, 아니면 false.
	// 성공 시 카탈로그를 제거하고 입력 모드 및 선택/배치 상태를 변경하는 부작용이 있다.
	 
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building")
	bool StartPlacement(FName FacilityDefinitionId);

	// 현재 배치를 취소하고 선택 정보를 비운 뒤 동일 건축 세션의 카탈로그로 돌아간다. 
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building")
	void ReturnToCatalog();

	// 카탈로그/프리뷰/선택을 모두 정리하고 플레이어 입력을 복구해 Closed로 전환한다. 
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building")
	void ExitBuildMode();

	// 현재 배치 방향에 90도 단위 증분을 더한다. 음수는 반대 방향이며 결과는 0~3으로 정규화된다.
	// Placing/Moving이 아니면 아무 일도 하지 않는다.
	 
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building")
	void RotatePlacement(int32 QuarterTurnDelta);

	// 최신 커서·revision·대상 등록을 재검증한 뒤 PreviewCommand를 Install 또는 Move로 확정한다.
	// 명령 성공 시만 true. 설치는 연속 배치를 유지하고, 이동은 BuildMode로 복귀하며 실패 시 프리뷰를 유지한다.
	 
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building")
	bool CommitPlacement();

private:
	// 현재 표시 중인 카탈로그 위젯. CatalogOpen 외 상태에서는 원칙적으로 null이다. 
	UPROPERTY(Transient)
	TObjectPtr<UFF_BuildCatalogScreen> CatalogScreen;

	// Install/Move 전 검증과 시각화를 담당하는 일회성 액터. Placing/Moving 동안만 존재한다.
	UPROPERTY(Transient)
	TObjectPtr<AFFZSPlacementPreviewActor> PlacementPreview;

	// 첫 클릭으로 선택해 이동·철거·취소 액션을 기다리는 시설이다. 권위 소유권은 WorldSubsystem에 있다.
	TWeakObjectPtr<AFFZSFacilityActor> PendingRemovalFacility;
	// Moving 세션이 대상으로 삼은 권위 액터. 외부 이동·삭제 및 스냅샷 복원 시 무효화한다.
	TWeakObjectPtr<AFFZSFacilityActor> MovingFacility;

	// 기존 팀 화면과 분리된 상단 토스트. 최초 실패 때만 만들고 건축 세션 동안 재사용한다.
	UPROPERTY(Transient)
	TObjectPtr<UFF_BuildPlacementFailureFeedback> PlacementFailureFeedback;

	// 좌하단 조작 안내 패널. 건축 세션 동안만 존재하며 카탈로그 열림 여부와 무관하게 유지된다.
	UPROPERTY(Transient)
	TObjectPtr<UFF_BuildModeGuide> BuildModeGuideWidget;

	// 카탈로그에서 선택한 정의와, 프리뷰 명령을 재구성하는 데 필요한 현재 배치 스냅샷이다.
	FFFZSFacilitySpec SelectedSpec;
	FFFZSGridPlacement CurrentPlacement;
	// 가장 최근 RefreshPreview 결과. CommitPlacement가 설치 가능 여부를 판단할 때 사용한다. 
	FFFZSPlacementValidation LastValidation;
	// 선택한 데이터 에셋의 조회 키. NAME_None은 선택된 시설이 없음을 뜻한다. 
	FName SelectedFacilityDefinitionId = NAME_None;
	// 마지막으로 해석한 Zone이자 경계 판정의 안정화 힌트. 등록 Zone을 찾지 못하거나 종료하면 NAME_None이다. 
	FName CurrentZoneId = NAME_None;
	// Move 대상의 안정 ID와 시작 위치. 동일 Zone·배치 커밋을 차단하고 등록 일치를 재검증한다.
	FGuid MovingFacilityInstanceId;
	FName MoveOriginZoneId = NAME_None;
	FFFZSGridPlacement MoveOriginPlacement;
	// 프리뷰를 재생성해도 하나의 사용자 이동 작업임을 유지하는 상관관계 ID.
	FGuid MoveTransactionId;
	// ExecuteCommand가 동기 방송하는 자체 Move 이벤트를 외부 변경과 구분한다.
	FGuid InFlightMoveCommandId;

	// 공개 API와 입력 처리의 기준이 되는 건축 모드 상태기계의 현재 상태. 
	EFF_BuildModeState State = EFF_BuildModeState::Closed;
	// 매 프레임의 비싼 라인트레이스/검증을 피하기 위한 누적 시간이다. 
	float PreviewRefreshAccumulator = 0.0f;
	// 라인트레이스, 바닥 경사, Zone, 회전 면적 검사를 모두 통과한 커서 표적이 있는지 나타낸다. 
	bool bHasPlacementTarget = false;
	// 외부 월드 변경 뒤 커서가 멈춰 있어도 다음 주기에 revision과 범위 색을 다시 검증하게 한다.
	bool bPreviewWorldDirty = false;
	// 비동기 카탈로그 결과를 같은 화면에 중복 적용하지 않도록 하는 플래그다. 
	bool bCatalogWasApplied = false;
	// 현재 화면의 카드들이 가장 최근 썸네일 프리로드 종결 상태를 반영했는지 나타냅니다.
	bool bCatalogThumbnailsApplied = false;
	// GameInstance 카탈로그에 건설 세션용 썸네일 참조를 정확히 한 번 획득했는지 추적합니다.
	bool bThumbnailPreloadAcquired = false;
	// 입력 모드 복구에 필요한 원래 값을 한 번만 캡처했는지 나타낸다. 
	bool bBuildSessionActive = false;
	// 카탈로그 카드/닫기 버튼을 누른 좌클릭이 같은 프레임에 설치나 철거로 전달되지 않게 한다.
	bool bSuppressWorldClickUntilReleased = false;
	// Enter/Space로 카탈로그 카드를 연 키가 같은 프레임의 설치 확정으로 전달되지 않게 한다.
	bool bSuppressPlacementConfirmUntilReleased = false;
	// 포커스된 카탈로그가 처리한 B를 PlayerController가 같은 프레임에 다시 토글하지 않게 한다.
	bool bSuppressBuildToggleUntilReleased = false;
	// 썸네일 해제 방송 중 공개 전환 API가 재진입해 Closed 불변식을 깨뜨리지 않게 한다.
	bool bIsExitingBuildMode = false;
	// 현재 CommitPreview의 Move 이벤트가 돌아오는 동안만 true다.
	bool bMoveCommitInFlight = false;
	// 건축 세션 진입 직전 PlayerController의 커서 표시 상태. 
	bool bPreviousShowMouseCursor = false;
	// 건축 진입 전에 포커스된 UI가 남아 있으면 종료 시 GameAndUI 포커스를 복원한다.
	TWeakPtr<SWidget> PreviousFocusedWidget;
	// 같은 프리뷰 상태가 짧은 주기로 반복될 때 로그가 폭증하지 않도록 마지막 문구를 기억한다. 
	mutable FString LastLoggedStatusMessage;

	// 커서 추적과 전체 Zone 검증의 비용을 제한하면서도 프리뷰가 충분히 부드럽게 보이는 갱신 간격.
	static constexpr float PreviewRefreshIntervalSeconds = 0.08f;

	// B/Escape/마우스/Q/E 키의 상태별 의미를 해석해 공개 전환 함수로 전달한다.
	void HandleInput(APlayerController& Controller);
	// 디자이너 위젯을 생성하고 델리게이트를 연결한다. 바인딩이 깨졌으면 C++ 안전 UI로 폴백한다. 
	void CreateCatalogScreen(APlayerController& Controller);
	// 델리게이트를 먼저 해제한 뒤 위젯을 비활성화·제거한다. 여러 번 호출해도 안전하다. 
	void RemoveCatalogScreen();
	// 카탈로그 로드/썸네일 이벤트의 현재 결과를 UI에 적용한다.
	void ApplyCatalogIfReady();
	void AcquireCatalogThumbnailSession(UFF_BuildCatalogSubsystem& Catalog);
	void ReleaseCatalogThumbnailSession();
	// 입력 복구용 커서와 Slate 포커스를 건축 세션당 한 번만 캡처한다.
	void BeginBuildSession(APlayerController& Controller);
	// 건축 세션 시작 시 좌하단 조작 안내 패널을 만들어 붙인다. 이미 있으면 아무 일도 하지 않는다.
	void CreateBuildModeGuide(APlayerController& Controller);
	// 조작 안내 패널을 뷰포트에서 제거한다. 여러 번 호출해도 안전하다.
	void RemoveBuildModeGuide();
	// 카탈로그가 키보드/마우스 입력을 받도록 GameAndUI 모드와 포커스를 설정한다. 
	void SetCatalogInputMode(APlayerController& Controller) const;
	// 카탈로그 없는 BuildMode/Placing/Moving에서 커서 기반 월드 입력을 받도록 GameAndUI 모드를 설정한다.
	void SetBuildInputMode(APlayerController& Controller) const;
	// 세션 시작 전에 저장한 커서 표시 상태를 복원하고 GameOnly 입력으로 돌아간다. 
	void RestoreInputMode(APlayerController& Controller);

	// 충돌에 관여하지 않는 Transient 프리뷰 액터를 만들고 자동 갱신을 끈다. 
	void SpawnPlacementPreview();
	// 유효한 프리뷰 액터를 파괴하고 포인터를 비운다. 여러 번 호출해도 안전하다. 
	void DestroyPlacementPreview();
	// 선택/원점/검증 캐시를 한 번에 초기화해 상태 전환마다 오래된 값이 남지 않게 한다.
	void ResetPlacementSelection();
	// Move 대상·원본 배치·명령 상관관계를 함께 초기화한다.
	void ResetMovingSelection();
	// 모든 설치 시설의 footprint/clearance/queue 범위 표현을 건축 세션 수명과 동기화한다.
	void SetFacilityRangeVisibility(bool bVisible) const;
	// 충돌 없는 시설 표현과 무관하게 커서 아래 실제 지면을 찾는다.
	bool TraceGroundUnderCursor(APlayerController& Controller, FHitResult& OutGroundHit) const;
	// BuildMode에서 커서 아래 시설을 작업 패널 대상으로 선택한다. 같은 시설 재클릭은 no-op이다.
	bool HandleRemovalClickUnderCursor(APlayerController& Controller);
	// 현재 작업 대상을 범위로 강조하고, 이전 대상이 있으면 강조를 해제한다.
	void SelectFacilityForRemoval(AFFZSFacilityActor& Facility);
	// 선택 강조와 약한 참조를 함께 정리한다. 카탈로그/종료/스냅샷 복원에서도 호출한다.
	void ClearPendingRemovalSelection();
	// 공용 우측 시설 정보 WBP를 건축 컨텍스트로 열거나 닫는다.
	void ShowRemovalConfirmation(const FText& FacilityName);
	void RemoveRemovalConfirmation(FGuid FacilityInstanceId);
	void ShowFacilityInfoFeedback(const FText& Feedback, bool bIsError) const;
	// 선택 대상을 최신 단계·레지스트리·점유 상태로 재검증한 뒤 권위 Remove 명령을 실행한다.
	bool ConfirmPendingRemoval();
	// 현재 선택된 시설을 재검증하고 액터 Spec·GUID·회전을 복사해 Moving을 시작한다.
	bool StartMovingSelectedFacility();
	// 원본 시설을 변경하지 않고 프리뷰만 정리한 뒤 BuildMode로 복귀한다.
	void CancelMovingToBuildMode(const FString& StatusMessage, bool bIsError = false);
	// 공용 시설 정보 패널의 선택 ID를 기존 건축 명령 경계에서 다시 검증하는 어댑터.
	void HandleMoveRequested(FGuid FacilityInstanceId);
	void HandleRemoveRequested(FGuid FacilityInstanceId);
	void HandleRemovalCancelRequested(FGuid FacilityInstanceId);
	// 커서 레이를 바닥/Zone/그리드 좌표로 변환하고 필요할 때 Install/Move PreviewCommand를 다시 검증한다.
	// bForceCommandRebuild 위치가 같아도 회전·월드 revision 변화 등을 반영할지 여부.
	//프리뷰 명령과 LastValidation을 실제로 갱신했으면 true.
	 
	bool UpdatePlacementFromCursor(bool bForceCommandRebuild = false);
	// 선택 Spec, GUID, Zone, 배치, 최신 revision으로 Install/Move PreviewCommand를 교체한다.
	void RebuildPreviewCommand();
	// Zone System의 현재 단계가 시설 변경을 허용하는 InitialSetup/MorningCheck인지 확인한다. 
	bool CanReconfigure() const;
	// B 입력으로 세션 자체를 열 수 있는지 Festival Stage와 Zone 정책을 함께 확인한다.
	bool IsBuildEntryAllowed() const;

	// 카탈로그 델리게이트 어댑터: UI가 서브시스템의 상태 전환 API에 직접 의존하지 않게 한다.
	void HandleFacilitySelected(FName FacilityDefinitionId);
	// 카탈로그의 닫기 요청을 범위/철거가 가능한 BuildMode 복귀로 변환한다.
	void HandleCatalogCloseRequested();
	// 포커스된 카탈로그의 B 요청을 BuildMode 복귀로 변환하고 raw B 폴링을 키 해제까지 차단한다.
	void HandleCatalogBuildModeKeyRequested();
	// CommonUI가 먼저 받은 Esc 요청을 건축 세션 전체 종료로 변환한다.
	void HandleCatalogExitKeyRequested();
	UFUNCTION()
	void HandleCatalogChanged();
	UFUNCTION()
	void HandleCatalogThumbnailsChanged();
	UFUNCTION()
	void HandleWorldChanged(const FFFZSWorldChangeEvent& Event);
	UFUNCTION()
	void HandleWorldSnapshotRestored();
	UFUNCTION()
	void HandleFestivalStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage);
	// 권위 실패 코드와 상세 사유를 신규 UMG 토스트에 표시한다. 프리뷰 Tick에서는 호출하지 않는다.
	void PresentPlacementFailure(EFFZSPlacementFailure FailureReason, const FText& Detail);
	UFF_BuildPlacementFailureFeedback* GetOrCreatePlacementFailureFeedback();
	UFF_FacilityInfoSubsystem* GetFacilityInfoSubsystem() const;
	void DismissPlacementFailureFeedback(bool bRemoveFromParent = false);
	// 화면 메시지를 표시하되 같은 문구는 한 번만 로그로 남긴다. 게임 상태는 변경하지 않는다. 
	void ShowStatus(const FString& Message, bool bIsError = false, float DisplaySeconds = 2.5f) const;

	// 현재 월드에서 필요한 협력 객체를 안전하게 조회한다. 없으면 모두 nullptr를 반환한다.
	APlayerController* GetLocalPlayerController() const;
	UFF_BuildCatalogSubsystem* GetCatalogSubsystem() const;
	UFFZSWorldSubsystem* GetZoneSystem() const;
};
