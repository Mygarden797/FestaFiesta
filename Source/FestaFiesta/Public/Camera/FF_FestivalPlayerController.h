// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Camera/FF_CameraTypes.h"
#include "FF_FestivalPlayerController.generated.h"

class AFF_FinaleViewpoint;
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
//   2. 건축 중에도 팬과 줌은 살린다. 부스를 놓으려면 화면을 움직여야 한다.
//   3. 시설 배치/이동 프리뷰 중에는 회전을 막는다. Q/E 는 이때 시설 회전 입력이다.
//   4. 건축 중에는 피킹을 막는다. 클릭은 배치 입력이지 선택 입력이 아니다.
//   5. 건축 모드 상태는 GetBuildModeState() 로 읽기만 한다. 건축 쪽 코드는 수정하지 않는다.
//   6. 관찰 시점이 다시 조작 가능해졌는데 다른 화면이 남긴 마우스 캡처 모드가 그대로면 이쪽 입력 모드를
//      다시 건다(RestoreOverviewInputModeIfLost). 건축 · 모달 · 연출 · CommonUI 화면이 쥐고 있을 때는 그 화면 몫이다.
//
// 이 규칙이 깨지면 "시설과 카메라가 함께 돈다"거나 "배치 클릭이 NPC 선택으로 먹힌다"로
// 나타난다. 둘 다 통합 데모에서야 보이는 종류라 미리 막아둔다.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class FESTAFIESTA_API AFF_FestivalPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
	virtual void FlushPressedKeys() override;
	AFF_FestivalPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	// 지금 카메라 입력을 왜 막고 있는지. 디버그 HUD 에 찍기 좋다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Camera")
	EFF_CameraInputBlock GetInputBlockReason() const;

	// 팬, 줌, 가장자리 스크롤을 받아도 되는 상태인가.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Camera")
	bool IsCameraMovementAllowed() const;

	// 클릭으로 대상을 고를 수 있는 상태인가. 관찰·캐릭터 시점에서는 true,
	// 건축·모달·액터 관람·연출 중에는 false 다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Camera")
	bool IsPickingAllowed() const;

	// ── 연출 시점 ────────────────────────────────────────────────────────────
	//
	// 피날레처럼 화면을 보여주는 구간에서 쓴다. 관찰 카메라는 월드에 그대로 남아
	// 있으므로, 연출이 끝나면 떠날 때 구도로 돌아온다.

	// 지정한 시점으로 전환한다. nullptr 이면 레벨에서 우선순위가 가장 높은 것을 찾는다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Camera")
	bool EnterScriptedView(AFF_FinaleViewpoint* Viewpoint, float BlendTime = 1.5f);

	// 연출을 끝내고 원래 보던 곳으로 돌아온다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Camera")
	void ExitScriptedView(float BlendTime = 1.5f);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Camera")
	bool IsInScriptedView() const { return bScriptedViewActive; }

	// 모달 UI 를 띄우는 쪽에서 켜고 끈다. 카메라 입력 전체가 멈춘다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Camera")
	void SetModalUiActive(bool bActive);

	// 빙의 대상이 바뀌어도 관찰 카메라는 월드에 그대로 남아 있다.
	// GetPawn() 을 캐스팅하면 캐릭터에 빙의한 순간 null 이 되므로 참조를 따로 들고 있는다.
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

	// 캐릭터에 빙의해 직접 조종한다. 관찰 카메라는 월드에 남아 구도를 유지한다.
	//
	// 관람은 SwitchToActorView, 조종은 이것이다. 둘을 하나로 합치지 않은 이유는
	// 필요한 처리가 다르기 때문이다. 관람은 뷰타깃만 옮기면 되지만 조종은 빙의를 옮겨야 하고,
	// 빙의를 옮기면 입력이 그 폰으로 가면서 카메라 팬이 자동으로 멈춘다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Camera")
	bool PossessCharacter(APawn* TargetCharacter, float BlendTime = 0.5f);

	// 전환 키가 기본으로 들어갈 캐릭터. GameMode 가 스폰하거나 찾아서 넘겨준다.
	//
	// 이게 없으면 ToggleView 가 되돌아갈 대상을 몰라서 조용히 아무것도 안 한다.
	// 처음에 이 누락 때문에 V 를 눌러도 반응이 없었다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Camera")
	void SetVisitorCharacter(APawn* InCharacter);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Camera")
	APawn* GetVisitorCharacter() const { return VisitorCharacter.Get(); }

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

	// 가장자리 이동은 이 키를 누르는 동안만 허용한다. 기본값은 휠 버튼이다.
	// 커서만 가장자리에 두고 손을 떼면 움직이지 않는다. 무효 키는 가장자리 이동을 끈다.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Input")
	FKey EdgeScrollActivationKey = EKeys::MiddleMouseButton;

	// 누른 채 끌면 잡은 땅이 커서를 따라오게 관찰 카메라를 옮긴다. 키는 IMC 가 정한다(IMC_Camera 에서 오른쪽 버튼).
	// 건축 중에는 쓰지 않는다(TickDragPan 참고). 비어 있으면 끌기 이동이 없다.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Input")
	TObjectPtr<UInputAction> DragPanAction;

	// 시점 전환에 쓸 블렌드 시간. 0 이면 즉시 전환한다.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Camera", meta = (ClampMin = "0.0"))
	float ViewSwitchBlendTime = 0.5f;

	// 피킹 트레이스 길이.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Camera", meta = (ClampMin = "1000.0"))
	float PickTraceDistance = 100000.f;

	/* 지금 시점과 시야 고정 상태에 맞는 입력 모드를 건다.

	   관찰 카메라는 커서가 필요하다. 시설을 클릭하고 UI 를 누르기 때문이다.

	   캐릭터 조작은 두 갈래다. 평소에는 커서를 두어 클릭할 수 있게 하고,
	   시야 고정을 켜면 커서를 치워 마우스 움직임이 전부 시점으로 가게 한다.

	   커서가 떠 있으면 그 아래 위젯이 마우스 움직임을 먼저 가져간다.
	   그래서 커서를 켠 채로는 시점 회전을 마우스 버튼과 함께 눌러야 한다.
	*/
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Camera")
	void RefreshViewInputMode();

	void ApplyViewInputMode(EFF_CameraViewMode Mode);

private:
	void HandlePan(const FInputActionValue& Value);
	void HandleZoom(const FInputActionValue& Value);
	void HandleRotate(const FInputActionValue& Value);
	void HandleSnapRotate(const FInputActionValue& Value);
	void HandlePick(const FInputActionValue& Value);
	void ProcessPickRequest();
	void HandleResetView(const FInputActionValue& Value);
	void HandleToggleView(const FInputActionValue& Value);
	void HandleDragPanStarted(const FInputActionValue& Value);
	void HandleDragPanEnded(const FInputActionValue& Value);

	bool IsBuildSessionActive() const;
	bool IsBuildPreviewActive() const;
	bool IsCameraRotationAllowed() const;
	bool IsCameraViewportFocused() const;
	void StopOverviewCameraMotion();

	// 커서 아래 월드 지점을 구한다. 커서가 없거나 광선이 지면을 안 만나면 false.
	bool ResolveCursorWorldAnchor(FVector& OutAnchor) const;

	// Enhanced Input과 native 좌클릭 fallback이 같은 PlayerTick에 함께 들어와도 한 번만 방송한다.
	bool bPickRequestHandledThisTick = false;

	// 활성화 키를 누른 채 마우스가 화면 가장자리에 있으면 그쪽으로 팬한다.
	void TickEdgeScroll(float DeltaTime);

	// 끌기 입력을 누르는 동안, 누른 순간 커서 아래에 있던 땅 지점이 계속 커서 아래에 오도록 초점을 옮긴다.
	void TickDragPan();

	// 끌기를 시작할 때 잡은 땅 지점(초점 평면 위). 입력을 떼거나 끌기가 막히면 풀린다.
	FVector DragPanAnchor = FVector::ZeroVector;
	bool bDragPanActive = false;

	// 끌기 입력(DragPanAction)을 누르고 있는가. 막 눌렀으면 다음 TickDragPan 이 끌기를 시작할지 본다.
	bool bDragPanHeld = false;
	bool bDragPanStartPending = false;

	// 관찰 시점으로 돌아왔는데 다른 화면이 바꾼 마우스 캡처가 그대로면 이쪽 입력 모드를 다시 건다.
	void RestoreOverviewInputModeIfLost();

	// 위에서 입력 모드를 다시 걸지 못한 이유(로그를 이유가 바뀔 때만 한 번 남긴다).
	FString LastInputModeRestoreSkip;

	// 보고 있던 대상이 파괴됐는지 확인하고, 그렇다면 관찰 시점으로 되돌린다.
	// 액터가 사라지면 뷰타깃이 없어져서 화면이 원점을 비추게 된다.
	void EnsureViewTargetAlive();

	bool bModalUiActive = false;

	// 연출 시점 진행 중인지. 켜져 있으면 카메라 조작이 전부 막힌다.
	bool bScriptedViewActive = false;

	EFF_CameraViewMode ViewMode = EFF_CameraViewMode::Overview;

	// 현재 보고 있는 대상.
	TWeakObjectPtr<AActor> ViewedActor;

	// ToggleView 로 되돌아갈 대상. 관찰 시점으로 나와도 지워지지 않는다.
	TWeakObjectPtr<AActor> LastViewedActor;

	// 관찰 카메라 참조. BeginPlay 에서 잡아두고 빙의가 바뀌어도 유지한다.
	TWeakObjectPtr<AFF_OverviewCameraPawn> OverviewPawn;

	// 캐시가 비었거나 대상이 파괴됐으면 월드에서 다시 찾는다.
	// 다른 시스템이 컨트롤러를 임의의 Pawn 으로 빙의시키는 경우가 있어서,
	// BeginPlay 한 번만 잡아두면 그 사이에 캐시가 낡을 수 있다.
	AFF_OverviewCameraPawn* ResolveOverviewPawn() const;

	// 전환 키의 기본 대상. LastViewedActor 가 비어 있을 때 여기로 간다.
	TWeakObjectPtr<APawn> VisitorCharacter;
};
