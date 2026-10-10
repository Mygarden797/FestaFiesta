// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Debug/FF_DebugConsole.h"
#include "GameFramework/Pawn.h"
#include "Camera/FF_CameraTypes.h"
#include "Camera/FF_OverviewCameraRig.h"
#include "FF_OverviewCameraPawn.generated.h"


class UCameraComponent;
class USceneComponent;
class USpringArmComponent;

// 축제를 내려다보는 관찰 카메라. 심즈류 배치 게임의 시점을 목표로 한다.
//
// 이 Pawn 은 계산을 하지 않는다. FFF_OverviewCameraRig 가 계산한 결과를
// SpringArm 에 옮겨 담는 역할만 한다. 그래야 카메라 수치를 에디터 없이 테스트할 수 있다.
//
// 충돌은 끄고 다닌다. 시설이나 지형에 카메라가 걸리면 배치 작업이 불가능해진다.
//
// 참고: Zone 플레이테스트에도 관찰용 Pawn 이 하나 있는데 그건 ZoneTestMap 전용이고
// 헤더가 Private 에 있어 밖에서 못 쓴다. 이건 게임 본편용이라 별도로 둔다.
UCLASS(Blueprintable)
class FESTAFIESTA_API AFF_OverviewCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	AFF_OverviewCameraPawn();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	// 축제장 중심과 전체가 들어오는 거리를 받아 첫 구도를 잡는다.
	// 레벨 크기를 아는 쪽(GameMode 나 Zone 시스템)이 불러준다.
	// 높이 보존을 켜면 최초 유효 지면 대비 초기 초점 높이를 이후 지형 추종의 기준으로 쓴다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Camera")
	void InitializeOverview(const FVector& FocusLocation, float SuggestedArmLength, bool bPreserveGroundClearance = false);

	// Zone 배치가 바뀌었을 때 이동 경계를 다시 계산한다. 초기 높이 기준은 유지한다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Camera")
	void RefreshZoneBounds();

	// 아래 넷은 PlayerController 가 입력을 해석해서 불러준다.
	// Pawn 이 직접 키를 읽지 않는 이유는 입력 차단 판단이 Controller 쪽 책임이기 때문이다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Camera")
	void AddPanInput(const FVector2D& ScreenInput);

	// 화면 가장자리 스크롤. 키보드 팬과 속도가 달라서 따로 쌓는다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Camera")
	void AddEdgeScrollInput(const FVector2D& ScreenInput);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Camera")
	void AddZoomInput(float Steps);

	// 커서 아래 지점을 화면에 고정한 채 줌한다. 설정에서 커서 줌을 끄면 중앙 기준으로 넘어간다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Camera")
	void AddZoomInputAtAnchor(float Steps, const FVector& WorldAnchor);

	// 커서 광선을 떨어뜨릴 수평면의 높이. 현재 초점 높이를 쓴다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Camera")
	float GetFocusPlaneZ() const;

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Camera")
	void AddRotateInput(float Direction);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Camera")
	void SnapRotate(int32 Direction);

	// 특정 지점을 화면 중앙으로. NPC 를 클릭했을 때 쓴다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Camera")
	void FocusOnLocation(const FVector& WorldLocation);

	// 이 액터를 계속 따라간다. nullptr 을 넣으면 추적을 끊는다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Camera")
	void SetFollowTarget(AActor* NewTarget);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Camera")
	AActor* GetFollowTarget() const { return FollowTarget.Get(); }

	// 초기 구도 복원. Home 키.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Camera")
	void ResetView();

	// UI 차단, 창 포커스 상실, 시점 전환에서 호출한다. 남은 명령/추적도 취소한다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Camera")
	void StopCameraMotion();

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Camera")
	EFF_OverviewCameraMotionState GetMotionState() const { return MotionState; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Camera")
	FFF_OverviewCameraState GetCameraState() const { return Rig.GetDisplayState(); }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Camera")
	float GetNormalizedZoom() const { return Rig.GetNormalizedZoom(); }

	// 헤드리스 스모크 테스트용. 카메라가 실제로 붙어 있는지 확인한다.
	bool HasActiveCamera() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|Camera")
	FFF_OverviewCameraSettings CameraSettings;

protected:
	UPROPERTY(VisibleAnywhere, Category = "FestaFiesta|Camera")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "FestaFiesta|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "FestaFiesta|Camera")
	TObjectPtr<UCameraComponent> OverviewCamera;

private:
	void ApplyRigToComponents();
	void RequestTransition(EFF_CameraMotionChannel Channels);
	void CompleteSettledTransitions();

	// 초점이 Zone 밖으로 나갔으면 되돌린다.
	//
	// 사각형 경계는 거친 1차 방어고, 이게 실제 모양을 지킨다.
	// 벽에 비스듬히 밀면 축을 나눠 시도해서 벽을 따라 미끄러지게 한다.
	void ConstrainFocusToZones();

	// 이 지점이 어느 Zone 안인지 본다. 밖이면 NAME_None.
	FName ResolveZoneAt(const FVector& WorldLocation) const;

	// 지금 경계가 어떤 상태인지 로그로 남긴다. 콘솔에서 부른다.
	void LogBoundsState() const;

	// 콘솔 이름은 프로세스 전역인데 Pawn 은 레벨마다 새로 생긴다.
	// 이미 있는 이름이면 손대지 않고, 해제할 때도 내가 등록한 것만 지운다.
	void RegisterDebugConsoleCommands();
	void UnregisterDebugConsoleCommands();

	// 레벨의 Zone 을 모두 감싸는 범위를 구해 경계로 삼는다.
	//
	// 한 번만 계산한다. Zone 은 설치·철거로 늘고 줄지만 축제장 자체가 넓어지지는 않는다.
	void ApplyZoneBoundsIfNeeded(float DeltaSeconds);

	// 한 지점의 지면 높이를 잰다. 못 찾으면 false.
	bool TraceGroundAt(const FVector2D& Where, float AroundZ, float& OutGroundZ) const;

	// 입력 프레임에서만 새 지면을 측정한다. 일회성 명령은 이 표본을 향해 보간한다.
	void UpdateGroundHeight(float DeltaSeconds, bool bRefreshSample);
	bool SampleGroundHeight(float& OutGroundZ) const;
	FFF_OverviewCameraSettings GetRuntimeCameraSettings() const;
	FVector ResolveGroundFocus(const FVector& WorldLocation) const;


	FFF_OverviewCameraRig Rig;
	EFF_OverviewCameraMotionState MotionState = EFF_OverviewCameraMotionState::Idle;
	EFF_CameraMotionChannel TransitionChannels = EFF_CameraMotionChannel::None;
	TOptional<float> GroundHeightSample;
	// 저작 설정을 바꾸지 않는 인스턴스별 지면 기준. Reset/Stop/경계 갱신은 보존하며
	// InitializeOverview만 지운다. 첫 표본이 늦어도 Focus/Follow의 Z를 초기값으로 오인하지 않는다.
	TOptional<float> PreservedGroundFocusOffset;
	double InitialGroundReferenceZ = 0.0;
	bool bPreserveInitialGroundClearance = false;
	bool bRefreshGroundRequested = false;
	bool bNeedsInitialPlacement = false;

	// 이번 프레임에 쌓인 팬 입력. Tick 에서 소비하고 비운다.
	FVector2D PendingPanInput = FVector2D::ZeroVector;
	FVector2D PendingEdgeScrollInput = FVector2D::ZeroVector;
	float PendingRotateInput = 0.f;

	TWeakObjectPtr<AActor> FollowTarget;

	bool bInitialized = false;

	// Zone 경계를 이미 잡았는지. 매 틱 액터를 순회하지 않기 위해서다.
	bool bZoneBoundsApplied = false;

	// 경계를 못 잡은 채 흐른 시간. 일정 시간이 지나면 한 번만 경고한다.
	float ZoneBoundsWaitSeconds = 0.f;
	bool bZoneBoundsWarned = false;

	// 마지막으로 Zone 안이었던 초점. 밖으로 나가면 여기로 되돌린다.
	FVector LastValidFocus = FVector::ZeroVector;
	bool bHasValidFocus = false;

	// 직전에 있던 Zone. 히스테리시스에 넘겨서 매번 전체를 훑지 않게 한다.
	FName LastZoneId = NAME_None;

	// 제한에 걸린 횟수. 0 에서 안 올라가면 제한이 동작하지 않는 것이다.
	int32 ConstrainBlockCount = 0;
	int32 ConstrainSlideCount = 0;

	// 개발용 콘솔 명령. 이름 충돌·해제 규칙은 FFF_DebugConsoleCommands 가 맡는다.
	FFF_DebugConsoleCommands DebugCommands;
};
