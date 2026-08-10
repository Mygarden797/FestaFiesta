// FF_BuildModeSubsystem.h

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_BuildModeSubsystem.generated.h"

class AFFZSPlacementPreviewActor;
class APlayerController;
class UFF_BuildCatalogScreen;
class UFF_BuildCatalogSubsystem;
class UFFZSWorldSubsystem;

UENUM(BlueprintType)
enum class EFF_BuildModeState : uint8
{
	// 건축 UI와 배치 프리뷰가 모두 없고, 플레이어 입력 모드가 원래 상태로 복원된 상태. 
	Closed,
	// 시설 목록 UI가 열려 있으며 선택 이벤트를 기다리는 상태. 
	CatalogOpen,
	// 선택한 시설의 프리뷰를 커서 아래 Zone에 투영하고 설치 입력을 받는 상태. 
	Placing
};

// B 키 카탈로그와 완성형 시설 배치를 연결하는 플레이어용 월드 서브시스템.

// UI는 시설 정의를 선택할 뿐이며 실제 설치는 FF Zone System의 PreviewCommand와
// ExecuteCommand 경로만 사용한다. 따라서 단계·충돌·보호 동선·정책 검증을 우회하지 않는다.

// 상태 전환은 `Closed -> CatalogOpen -> Placing` 순서로 진행한다. 배치 취소는 카탈로그로,
// B 키 또는 UI 닫기는 Closed로 돌아간다. Tick은 로컬 플레이어의 입력을 폴링하고, Placing일
// 때만 일정 간격으로 커서 위치를 다시 검증한다. 이 클래스가 보관하는 UI와 프리뷰 액터는
// 세션용 Transient 객체이므로 월드 종료 시 반드시 제거하고 입력 모드도 복구한다.
 
UCLASS()
class FESTAFIESTA_API UFF_BuildModeSubsystem final : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// 지원 월드인지와 QA 전용 맵인지 확인해 이 서브시스템을 생성할지를 결정한다. 
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	// 실제 게임 플레이 입력을 제공하는 Game/PIE/GamePreview 월드만 허용한다. 
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	// Zone System을 먼저 초기화하도록 의존성을 등록한다. 
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	// UI·프리뷰를 제거하고 건축 세션이 바꾼 입력 상태를 원복한다. 
	virtual void Deinitialize() override;
	// 입력을 처리하고, Placing 상태에서는 제한된 주기로 커서 기반 프리뷰를 갱신한다. 
	virtual void Tick(float DeltaTime) override;
	// Tickable 통계에서 이 서브시스템의 Tick 비용을 식별할 stat ID를 반환한다. 
	virtual TStatId GetStatId() const override;

	// 현재 상태를 반환한다. 이 값으로 카탈로그/배치/종료 UI를 구분할 수 있다. 
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building")
	EFF_BuildModeState GetBuildModeState() const { return State; }

	// Closed이면 카탈로그를 열고, 이미 건축 중이면 전체 건축 모드를 종료한다. 
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building")
	void ToggleBuildMode();

	// 건축 세션을 시작하거나 배치 화면에서 카탈로그로 돌아간다.
	// 필수 서브시스템/Zone/UI가 없으면 부분 생성물을 정리하고 Closed 상태로 되돌린다.
	 
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
	// Placing 상태가 아니면 아무 일도 하지 않는다.
	 
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building")
	void RotatePlacement(int32 QuarterTurnDelta);

	//최신 커서 위치를 강제 재검증한 뒤 PreviewCommand를 실제 설치로 확정한다.
	//설치 명령이 성공했을 때만 true. 실패 시 현재 프리뷰는 유지되어 위치를 고칠 수 있다.
	 
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building")
	bool CommitPlacement();

private:
	// 현재 표시 중인 카탈로그 위젯. CatalogOpen 외 상태에서는 원칙적으로 null이다. 
	UPROPERTY(Transient)
	TObjectPtr<UFF_BuildCatalogScreen> CatalogScreen;

	// 설치 전 검증과 시각화를 담당하는 일회성 액터. Placing 동안만 존재한다. 
	UPROPERTY(Transient)
	TObjectPtr<AFFZSPlacementPreviewActor> PlacementPreview;

	// 카탈로그에서 선택한 정의와, 프리뷰 명령을 재구성하는 데 필요한 현재 배치 스냅샷이다.
	FFFZSFacilitySpec SelectedSpec;
	FFFZSGridPlacement CurrentPlacement;
	// 가장 최근 RefreshPreview 결과. CommitPlacement가 설치 가능 여부를 판단할 때 사용한다. 
	FFFZSPlacementValidation LastValidation;
	// 선택한 데이터 에셋의 조회 키. NAME_None은 선택된 시설이 없음을 뜻한다. 
	FName SelectedFacilityDefinitionId = NAME_None;
	// 마지막으로 해석한 Zone이자 경계 판정의 안정화 힌트. 등록 Zone을 찾지 못하거나 종료하면 NAME_None이다. 
	FName CurrentZoneId = NAME_None;

	// 공개 API와 입력 처리의 기준이 되는 건축 모드 상태기계의 현재 상태. 
	EFF_BuildModeState State = EFF_BuildModeState::Closed;
	// 매 프레임의 비싼 라인트레이스/검증을 피하기 위한 누적 시간이다. 
	float PreviewRefreshAccumulator = 0.0f;
	// 라인트레이스, 바닥 경사, Zone, 회전 면적 검사를 모두 통과한 커서 표적이 있는지 나타낸다. 
	bool bHasPlacementTarget = false;
	// 비동기 카탈로그 결과를 같은 화면에 중복 적용하지 않도록 하는 플래그다. 
	bool bCatalogWasApplied = false;
	// 현재 화면의 카드들이 가장 최근 썸네일 프리로드 종결 상태를 반영했는지 나타냅니다.
	bool bCatalogThumbnailsApplied = false;
	// GameInstance 카탈로그에 건설 세션용 썸네일 참조를 정확히 한 번 획득했는지 추적합니다.
	bool bThumbnailPreloadAcquired = false;
	// 입력 모드 복구에 필요한 원래 값을 한 번만 캡처했는지 나타낸다. 
	bool bBuildSessionActive = false;
	// 건축 세션 진입 직전 PlayerController의 커서 표시 상태. 
	bool bPreviousShowMouseCursor = false;
	// 같은 프리뷰 상태가 짧은 주기로 반복될 때 로그가 폭증하지 않도록 마지막 문구를 기억한다. 
	mutable FString LastLoggedStatusMessage;

	// 커서 추적과 전체 Zone 검증의 비용을 제한하면서도 프리뷰가 충분히 부드럽게 보이는 갱신 간격. 
	static constexpr float PreviewRefreshIntervalSeconds = 0.08f;
	// 1cm 단위 Zone 그리드에서 배치 원점을 맞추는 10cm 스냅 간격. 
	static constexpr int32 PlacementSnapCm = 10;

	// B/Escape/마우스/Q/E 키의 상태별 의미를 해석해 공개 전환 함수로 전달한다. 
	void HandleInput();
	// 디자이너 위젯을 생성하고 델리게이트를 연결한다. 바인딩이 깨졌으면 C++ 안전 UI로 폴백한다. 
	void CreateCatalogScreen(APlayerController& Controller);
	// 델리게이트를 먼저 해제한 뒤 위젯을 비활성화·제거한다. 여러 번 호출해도 안전하다. 
	void RemoveCatalogScreen();
	// 카탈로그 로드 상태를 현재 UI에 한 번 적용하며, 로딩 중에는 다음 Tick에서 다시 시도한다. 
	void ApplyCatalogIfReady();
	void AcquireCatalogThumbnailSession(UFF_BuildCatalogSubsystem& Catalog);
	void ReleaseCatalogThumbnailSession();
	// 카탈로그가 키보드/마우스 입력을 받도록 GameAndUI 모드와 포커스를 설정한다. 
	void SetCatalogInputMode(APlayerController& Controller) const;
	// 게임의 커서 라인트레이스와 배치 클릭을 함께 받을 수 있도록 GameAndUI 모드를 설정한다. 
	void SetPlacementInputMode(APlayerController& Controller) const;
	// 세션 시작 전에 저장한 커서 표시 상태를 복원하고 GameOnly 입력으로 돌아간다. 
	void RestoreInputMode(APlayerController& Controller);

	// 충돌에 관여하지 않는 Transient 프리뷰 액터를 만들고 자동 갱신을 끈다. 
	void SpawnPlacementPreview();
	// 유효한 프리뷰 액터를 파괴하고 포인터를 비운다. 여러 번 호출해도 안전하다. 
	void DestroyPlacementPreview();
	//커서 레이를 바닥/Zone/그리드 좌표로 변환하고 필요할 때 PreviewCommand를 다시 검증한다.
	// bForceCommandRebuild 위치가 같아도 회전·설치 후 revision 변화 등을 반영할지 여부.
	//프리뷰 명령과 LastValidation을 실제로 갱신했으면 true.
	 
	bool UpdatePlacementFromCursor(bool bForceCommandRebuild = false);
	// 선택 스펙, Zone, 배치, 최신 월드 revision으로 설치 PreviewCommand를 교체한다. 
	void RebuildPreviewCommand();
	// Zone System의 현재 단계가 시설 변경을 허용하는 InitialSetup/MorningCheck인지 확인한다. 
	bool CanReconfigure() const;

	// 카탈로그 델리게이트 어댑터: UI가 서브시스템의 상태 전환 API에 직접 의존하지 않게 한다.
	void HandleFacilitySelected(FName FacilityDefinitionId);
	// 카탈로그의 닫기 요청을 전체 건축 모드 종료로 변환한다. 
	void HandleCatalogCloseRequested();
	UFUNCTION()
	void HandleCatalogChanged();
	UFUNCTION()
	void HandleCatalogThumbnailsChanged();
	// 화면 메시지를 표시하되 같은 문구는 한 번만 로그로 남긴다. 게임 상태는 변경하지 않는다. 
	void ShowStatus(const FString& Message, bool bIsError = false, float DisplaySeconds = 2.5f) const;

	// 현재 월드에서 필요한 협력 객체를 안전하게 조회한다. 없으면 모두 nullptr를 반환한다.
	APlayerController* GetLocalPlayerController() const;
	UFF_BuildCatalogSubsystem* GetCatalogSubsystem() const;
	UFFZSWorldSubsystem* GetZoneSystem() const;
};
