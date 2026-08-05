#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_FFZSTypes.h"
#include "Zone/FF_ZoneSystemPlaytestTypes.h"
#include "FF_ZoneSystemPlaytestSubsystem.generated.h"

class APlayerController;
class AStaticMeshActor;
class AFFZSFacilityActor;
class AFFZSGameplayZone;
class AFFZSPlacementPreviewActor;
class AFFZSProtectedArea;
class AFF_ZoneSystemPlaytestOverviewPawn;
class UFFZSWorldSubsystem;
class UFF_ZoneSystemPlaytestHUD;

/**
 * /Game/FestaFiesta/Zone/Maps/ZoneTestMap에서만 생성되는 런타임 상호작용 테스트 하네스다.
 * 팀의 기본 GameMode나 맵 자산을 덮어쓰지 않고 Zone 환경, 관찰 Pawn과 WBP를 임시로 연결한다.
 */
UCLASS()
class FESTAFIESTA_API UFF_ZoneSystemPlaytestSubsystem final : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    UFF_ZoneSystemPlaytestSubsystem();

    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    UFUNCTION(BlueprintPure, Category = "Zone Playtest")
    FFF_ZoneSystemPlaytestHudState GetHudState() const;

    UFUNCTION(BlueprintCallable, Category = "Zone Playtest") void SelectPreviousPreset();
    UFUNCTION(BlueprintCallable, Category = "Zone Playtest") void SelectNextPreset();
    UFUNCTION(BlueprintCallable, Category = "Zone Playtest") void InstallAtPreview();
    UFUNCTION(BlueprintCallable, Category = "Zone Playtest") void MoveSelectedFacility();
    UFUNCTION(BlueprintCallable, Category = "Zone Playtest") void RemoveSelectedFacility();
    UFUNCTION(BlueprintCallable, Category = "Zone Playtest") void OpenSelectedFacility();
    UFUNCTION(BlueprintCallable, Category = "Zone Playtest") void CloseSelectedFacility();
    UFUNCTION(BlueprintCallable, Category = "Zone Playtest") void EmergencyCloseSelectedFacility();
    UFUNCTION(BlueprintCallable, Category = "Zone Playtest") void JoinSelectedQueue();
    UFUNCTION(BlueprintCallable, Category = "Zone Playtest") void AdvanceSelectedQueue();
    UFUNCTION(BlueprintCallable, Category = "Zone Playtest") void ToggleSelectedQueueBlocked();
    UFUNCTION(BlueprintCallable, Category = "Zone Playtest") void SavePlaytestSnapshot();
    UFUNCTION(BlueprintCallable, Category = "Zone Playtest") void LoadPlaytestSnapshot();
    UFUNCTION(BlueprintCallable, Category = "Zone Playtest") void ResetPlaytestScenario();
    UFUNCTION(BlueprintCallable, Category = "Zone Playtest") void ToggleWorldDebug();
    UFUNCTION(BlueprintCallable, Category = "Zone Playtest") void ToggleHelp();
    UFUNCTION(BlueprintCallable, Category = "Zone Playtest") void ToggleUiInteractionMode();

private:
    // 다른 팀 맵에서 의도치 않게 실행되지 않도록 생성 대상 패키지를 정확히 제한한다.
    static const TCHAR* TargetMapPackage;
    static const FName DefaultZoneId;

    // 테스트 월드에서 찾거나 임시 생성한 Zone, 보호 구역, 프리뷰와 바닥이다.
    UPROPERTY(Transient)
    TObjectPtr<AFFZSGameplayZone> TestZone;

    UPROPERTY(Transient)
    TObjectPtr<AFFZSProtectedArea> TestProtectedArea;

    UPROPERTY(Transient)
    TObjectPtr<AFFZSPlacementPreviewActor> PlacementPreview;

    UPROPERTY(Transient)
    TObjectPtr<AStaticMeshActor> RuntimeFloor;

    // 로컬 플레이어가 소유하는 전체 관찰 Pawn과 디자이너가 편집 가능한 WBP 인스턴스다.
    UPROPERTY(Transient)
    TObjectPtr<AFF_ZoneSystemPlaytestOverviewPawn> PlaytestPawn;

    UPROPERTY(Transient)
    TObjectPtr<UFF_ZoneSystemPlaytestHUD> HudWidget;

    UPROPERTY(EditDefaultsOnly, Category = "Zone Playtest")
    TSoftClassPtr<UFF_ZoneSystemPlaytestHUD> HudWidgetClass;

    // 선택 프리셋, 현재 배치 좌표와 플러그인 검증·스냅샷 상태를 보관한다.
    FFFZSGridPlacement CurrentPlacement;
    EFFZSBuiltInFacilityPreset SelectedPreset = EFFZSBuiltInFacilityPreset::FoodTruckBasic;
    FFFZSPlacementValidation LastValidation;
    FFFZSWorldSnapshot InitialSnapshot;
    FFFZSWorldSnapshot SavedSnapshot;

    FGuid LastFacilityId;
    FName ActiveZoneId = NAME_None;
    FString StatusMessage;
    FColor StatusColor = FColor::White;

    // 입력 모드, HUD 표시, 디버그와 명령행 스모크의 런타임 상태다.
    float PreviewRefreshElapsed = 0.0f;
    int32 NextAgentNumber = 1;
    bool bEnvironmentReady = false;
    bool bEnableFullNavigationValidation = false;
    bool bShowHelp = true;
    bool bShowWorldDebug = false;
    bool bUiInteractionMode = false;
    bool bHudVisible = true;
    bool bHasSavedSnapshot = false;
    bool bHasPlacementTarget = true;
    bool bOwnsTestZone = false;
    bool bSmokeExitPending = false;
    bool bExperienceSmokeRequested = false;
    int32 SmokeExitCode = 0;

    // Zone, 바닥, 보호 구역과 배치 프리뷰를 준비한다.
    bool BuildTestEnvironment(UWorld& World);
    AFFZSGameplayZone* FindOrCreateTestZone(UWorld& World, UFFZSWorldSubsystem& ZoneSystem);
    void EnsureTestFloor(UWorld& World, const AFFZSGameplayZone& Zone);
    void CreateProtectedArea(UWorld& World, UFFZSWorldSubsystem& ZoneSystem);
    void CreatePlacementPreview(UWorld& World);

    // 로컬 PlayerController에 관찰 Pawn과 WBP를 생성·연결한다.
    void EnsurePlayerExperience();
    void SpawnPlaytestPawn(APlayerController& Controller);
    void CreatePlaytestHud(APlayerController& Controller);
    void ApplyUiInputMode();
    bool UpdatePlacementFromPlayer();

    // 화면 중앙 타깃을 시설 좌표로 변환하고 프리뷰 명령·시각화를 갱신한다.
    void SetSelectedPreset(EFFZSBuiltInFacilityPreset NewPreset);
    void SetPlacement(const FIntPoint& OriginCell, int32 QuarterTurns);
    void RebuildPreviewCommand();
    void RefreshPreview();
    void ApplyPreviewVisualScale();
    void ApplyFacilityVisualScale(AFFZSFacilityActor* Facility) const;
    void ApplyAllFacilityVisuals() const;

    // 설치, 생명주기, 대기열과 스냅샷 테스트 명령을 실행한다.
    void HandleInput();
    void CommitPreview();
    void ExecuteLifecycleCommand(EFFZSCommandType Type);
    void MoveLastFacility();
    void ToggleQueueBlocked();
    void JoinQueueAgent();
    void AdvanceQueueAgents();
    void CapturePlaytestSnapshot();
    void RestorePlaytestSnapshot();
    void ResetPlaytest();

    // WBP가 없을 때의 폴백 표시와 자동 스모크 검증을 담당한다.
    void DrawWorldDebug() const;
    void DrawHud() const;
    void SetStatus(const FString& Message, const FColor& Color = FColor::White);
    void RunCommandLineSmokeTest();
    void CompleteExperienceSmokeTest();

    UFFZSWorldSubsystem* GetZoneSystem() const;
    AFFZSFacilityActor* GetLastFacility() const;
};


