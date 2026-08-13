// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Camera/FF_CameraTypes.h"
#include "FF_FestivalPlayerController.generated.h"

class AFF_OverviewCameraPawn;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnActorPicked, AActor*, PickedActor);

// 시점이 바뀔 때. UI 가 HUD 를 갈아끼우거나 입력 힌트를 바꾸는 데 쓴다.
// ViewedActor 는 관찰 시점으로 돌아왔을 때 nullptr 이다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFF_OnViewModeChanged,
	EFF_CameraViewMode, NewMode, AActor*, ViewedActor);

// 축제 관찰 시점의 PlayerController.
//
// ─────────────────────────────────────────────────────────────────────────────
// 입력 소유권에 대하여
//
// 건축 모드(UFF_BuildModeSubsystem)가 이미 PlayerController 를 강하게 쓰고 있다.
// 입력 모드를 바꾸고, 마우스 커서 표시 상태를 저장했다가 되돌리고, 커서 아래
// 지면을 트레이스한다. 여기서 카메라가 같은 것들을 건드리면 서로 덮어쓰게 된다.
//
// 그래서 규칙을 이렇게 둔다.
//
//   1. 건축 세션이 열려 있는 동안 이 컨트롤러는 커서 표시와 입력 모드를 건드리지 않는다.
//      그건 건축 모드가 저장했다가 되돌리는 값이다. 양쪽이 쓰면 복원 순서 싸움이 난다.
//   2. 건축 중에도 팬과 줌과 회전은 살린다. 부스를 놓으려면 화면을 움직여야 한다.
//   3. 건축 중에는 피킹만 막는다. 클릭은 배치 입력이지 선택 입력이 아니다.
//   4. 건축 모드 상태는 GetBuildModeState() 로 읽기만 한다. 건축 쪽 코드는 수정하지 않는다.
//
// 이 규칙이 깨지면 "건축 중에 카메라가 돈다"거나 "배치 클릭이 NPC 선택으로 먹힌다"로
// 나타난다. 둘 다 통합 데모에서야 보이는 종류라 미리 막아둔다.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class FESTAFIESTA_API AFF_FestivalPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AFF_FestivalPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	// 지금 카메라 입력을 왜 막고 있는지. 디버그 HUD 에 찍기 좋다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Camera")
	EFF_CameraInputBlock GetInputBlockReason() const;

	// 팬, 줌, 회전을 받아도 되는 상태인가.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Camera")
	bool IsCameraMovementAllowed() const;

	// 클릭으로 대상을 고를 수 있는 상태인가. 건축 중에는 false 다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Camera")
	bool IsPickingAllowed() const;

	// 모달 UI 를 띄우는 쪽에서 켜고 끈다. 카메라 입력 전체가 멈춘다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Camera")
	void SetModalUiActive(bool bActive);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Camera")
	AFF_OverviewCameraPawn* GetOverviewCameraPawn() const;

	// ── 시점 전환 ────────────────────────────────────────────────────────────
	//
	// Possess 가 아니라 SetViewTargetWithBlend 를 쓴다. 빙의를 옮기면 입력 바인딩이
	// 통째로 갈리고, 돌아왔을 때 관찰 카메라가 새로 초기화되어 보던 곳을 잃는다.
	// 시점만 바꾸면 관찰 카메라는 제자리에 그대로 떠 있어서 돌아오면 그 구도가 유지된다.
	//
	// 대상 폰의 조종은 이 컨트롤러가 하지 않는다. 그건 그 폰을 만든 쪽 몫이다.
	// 여기서 제공하는 건 "무엇을 화면에 보여줄 것인가"까지다.

	// 지정한 액터의 시점으로 넘어간다. 그 액터에 카메라 컴포넌트가 있으면 그것을 쓰고,
	// 없으면 액터 위치에서 본다. nullptr 이면 아무것도 하지 않는다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Camera")
	bool SwitchToActorView(AActor* TargetActor, float BlendTime = 0.5f);

	// 관찰 시점으로 돌아온다. 떠나기 전 구도가 그대로 남아 있다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Camera")
	void SwitchToOverview(float BlendTime = 0.5f);

	// 관찰 시점이면 마지막으로 봤던 액터로, 액터 시점이면 관찰로 돌아간다.
	// 마지막 대상이 사라졌으면 관찰 시점에 머문다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Camera")
	void ToggleView(float BlendTime = 0.5f);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Camera")
	EFF_CameraViewMode GetViewMode() const { return ViewMode; }

	// 액터 시점일 때 보고 있는 대상. 관찰 시점이면 nullptr.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Camera")
	AActor* GetViewedActor() const { return ViewedActor.Get(); }

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Camera")
	FFF_OnViewModeChanged OnViewModeChanged;

	// 클릭으로 무언가를 골랐을 때. 결정 이유 표시 UI 가 여기에 붙으면 된다.
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Camera")
	FFF_OnActorPicked OnActorPicked;

	// 아래 자산들은 에디터에서 지정한다. 비어 있으면 경고만 남기고 조용히 아무것도 안 한다.
	// 자산이 없다고 크래시가 나면 다른 사람이 이 컨트롤러를 못 쓴다.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Input")
	TObjectPtr<UInputMappingContext> CameraMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Input")
	int32 CameraMappingPriority = 0;

	// Vector2D. 화면 기준 팬.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Input")
	TObjectPtr<UInputAction> PanAction;

	// float. 양수면 당겨온다.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Input")
	TObjectPtr<UInputAction> ZoomAction;

	// float. 홀드 회전.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Input")
	TObjectPtr<UInputAction> RotateAction;

	// float. 탭 회전. 부호가 방향이다.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Input")
	TObjectPtr<UInputAction> SnapRotateAction;

	// bool. 커서 아래 대상 선택.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Input")
	TObjectPtr<UInputAction> PickAction;

	// bool. 초기 구도 복원.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Input")
	TObjectPtr<UInputAction> ResetViewAction;

	// bool. 관찰 시점과 마지막으로 본 액터 시점을 왕복한다.
	// 되돌아갈 대상이 없으면 아무 일도 일어나지 않는다.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Input")
	TObjectPtr<UInputAction> ToggleViewAction;

	// 시점 전환에 쓸 블렌드 시간. 0 이면 즉시 전환한다.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Camera", meta = (ClampMin = "0.0"))
	float ViewSwitchBlendTime = 0.5f;

	// 피킹 트레이스 길이.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Camera", meta = (ClampMin = "1000.0"))
	float PickTraceDistance = 100000.f;

private:
	void HandlePan(const FInputActionValue& Value);
	void HandleZoom(const FInputActionValue& Value);
	void HandleRotate(const FInputActionValue& Value);
	void HandleSnapRotate(const FInputActionValue& Value);
	void HandlePick(const FInputActionValue& Value);
	void HandleResetView(const FInputActionValue& Value);
	void HandleToggleView(const FInputActionValue& Value);

	bool IsBuildSessionActive() const;

	// 커서 아래 월드 지점을 구한다. 커서가 없거나 광선이 지면을 안 만나면 false.
	bool ResolveCursorWorldAnchor(FVector& OutAnchor) const;

	// 마우스가 화면 가장자리에 있으면 그쪽으로 팬한다.
	void TickEdgeScroll(float DeltaTime);

	// 보고 있던 대상이 파괴됐는지 확인하고, 그렇다면 관찰 시점으로 되돌린다.
	// 액터가 사라지면 뷰타깃이 없어져서 화면이 원점을 비추게 된다.
	void EnsureViewTargetAlive();

	bool bModalUiActive = false;

	EFF_CameraViewMode ViewMode = EFF_CameraViewMode::Overview;

	// 현재 보고 있는 대상.
	TWeakObjectPtr<AActor> ViewedActor;

	// ToggleView 로 되돌아갈 대상. 관찰 시점으로 나와도 지워지지 않는다.
	TWeakObjectPtr<AActor> LastViewedActor;
};
