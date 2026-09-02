// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_CameraTypes.generated.h"

// 탑다운 관찰 카메라 관련 타입 모음.
//
// 이 모듈이 소유하는 것은 "플레이어가 축제를 어떻게 바라보는가"뿐이다.
// 시설 배치, NPC 결정, 대기열은 각자 주인이 따로 있고 여기서는 읽기만 한다.

DECLARE_LOG_CATEGORY_EXTERN(LogFestaFiestaCamera, Log, All);

// 지금 무엇을 보고 있는가.
UENUM(BlueprintType)
enum class EFF_CameraViewMode : uint8
{
	// 축제 전체를 내려다보는 관찰 시점. 기본값이다.
	Overview  UMETA(DisplayName = "관찰 시점"),
	// 다른 액터의 시점을 빌려 보는 중. 조종은 하지 않는다. 관찰 카메라는 제자리에 떠 있다.
	Actor     UMETA(DisplayName = "액터 시점"),
	// 캐릭터에 빙의해 직접 조종하는 중. 관찰 카메라는 월드에 남아 상태를 유지한다.
	Character UMETA(DisplayName = "캐릭터 시점")
};

// 카메라 입력을 눌러둬야 하는 이유.
// 건축 모드가 커서와 클릭을 쓰고 있는데 카메라가 같은 입력을 먹으면
// 배치 클릭이 드래그로 먹히거나 커서 표시가 서로 덮어써진다.
UENUM(BlueprintType)
enum class EFF_CameraInputBlock : uint8
{
	// 아무도 안 막는 상태. 팬, 줌, 회전, 피킹 전부 동작한다.
	None            UMETA(DisplayName = "없음"),
	// 건축 세션 진행 중. 팬과 줌은 살리고 피킹을 막는다.
	// 시설 프리뷰가 활성인 동안에는 별도 정책으로 카메라 회전도 막는다.
	BuildSession    UMETA(DisplayName = "건축 세션"),
	// 모달 UI가 떠 있음. 카메라 입력 전체를 막는다.
	ModalUi         UMETA(DisplayName = "모달 UI"),
	// 다른 액터 시점을 보는 중. 관찰 카메라 조작은 화면에 안 보이므로 막는다.
	ActorView       UMETA(DisplayName = "액터 시점"),
	// 캐릭터를 조종하는 중. 같은 WASD 가 카메라 팬으로도 먹으면 안 된다.
	CharacterControl UMETA(DisplayName = "캐릭터 조종"),
	// 시네마틱이나 자동 연출 중. 피날레 불꽃처럼 화면을 보여주는 구간이다.
	//
	// 지금은 조작을 전부 막는다. 연출을 보는 시간이라 그게 맞다고 봤는데,
	// 나중에 "구경하면서 돌려보고 싶다" 가 되면 회전만 열어주면 된다.
	Scripted        UMETA(DisplayName = "연출 중")
};

namespace FFCameraInputModel
{
	// 건축 중에도 배치 위치를 볼 수 있도록 팬과 줌은 유지한다.
	inline bool IsMovementAllowed(const EFF_CameraInputBlock Reason)
	{
		return Reason != EFF_CameraInputBlock::ModalUi
			&& Reason != EFF_CameraInputBlock::ActorView
			&& Reason != EFF_CameraInputBlock::CharacterControl
			&& Reason != EFF_CameraInputBlock::Scripted;
	}

	// 시설 배치/이동 프리뷰가 Q/E 회전을 소유하므로 그때만 카메라 회전을 추가로 막는다.
	inline bool IsRotationAllowed(
		const EFF_CameraInputBlock Reason,
		const bool bBuildPreviewActive)
	{
		return IsMovementAllowed(Reason) && !bBuildPreviewActive;
	}

	// 캐릭터를 직접 조종하는 동안에도 월드 시설 선택은 살아 있어야 한다.
	// WASD/시점 조작과 좌클릭 피킹은 서로 다른 입력이므로 CharacterControl을
	// 카메라 이동 차단과 시설 선택 차단에 똑같이 적용하지 않는다.
	inline bool IsPickingAllowed(
		const EFF_CameraInputBlock Reason,
		const bool bBuildSessionActive)
	{
		return !bBuildSessionActive &&
			(Reason == EFF_CameraInputBlock::None ||
			 Reason == EFF_CameraInputBlock::CharacterControl);
	}

	// CommonUI나 런타임 Mapping Context가 Enhanced Input action을 소비하더라도 기본 좌클릭은
	// 선택 신호를 잃지 않아야 한다. Action이 이미 처리된 프레임에는 중복 방송하지 않는다.
	inline bool ShouldUsePrimaryPointerPickFallback(
		const bool bPickActionHandledThisTick,
		const bool bPrimaryPointerJustPressed)
	{
		return !bPickActionHandledThisTick && bPrimaryPointerJustPressed;
	}
}

// 디자이너가 조정하는 값들. 전부 게임 단위(cm, 도, 초)다.
USTRUCT(BlueprintType)
struct FFF_OverviewCameraSettings
{
	GENERATED_BODY()

	// 가장 가까이 붙었을 때의 스프링암 길이. NPC 표정이 보이는 거리.
	// 지형 추종이 붙으면서 더 낮춰도 파고들지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zoom", meta = (ClampMin = "100.0"))
	float MinArmLength = 450.f;

	// 가장 멀리 뺐을 때. 축제장 전체가 한 화면에 들어오는 거리.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zoom", meta = (ClampMin = "100.0"))
	float MaxArmLength = 16000.f;

	// 휠 한 칸당 줌 비율. 곱셈이라 어느 거리에서도 체감이 같다.
	// 덧셈으로 하면 가까울 때 너무 크게 튀고 멀 때 안 움직인다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zoom", meta = (ClampMin = "1.01", ClampMax = "2.0"))
	float ZoomStepMultiplier = 1.15f;

	// 가까울 때 피치. 얕게 눕혀야 부스 간판과 NPC가 보인다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch", meta = (ClampMin = "-89.0", ClampMax = "-5.0"))
	float PitchAtMinZoom = -35.f;

	// 멀 때 피치. 거의 수직으로 내려다봐야 배치와 동선이 읽힌다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch", meta = (ClampMin = "-89.0", ClampMax = "-5.0"))
	float PitchAtMaxZoom = -72.f;

	// 피치가 PitchAtMaxZoom 에 도달하는 거리. 여기서부터는 더 물러나도 각도가 안 변한다.
	//
	// 각도 변화 구간과 줌 가능 범위를 분리하기 위한 값이다. 둘을 MaxArmLength 하나로 묶으면,
	// 큰 맵이라 줌아웃 여유를 넉넉히 주는 순간 실사용 거리대의 각도가 전부 눕는다.
	// 예를 들어 최대 16000 인데 평소 4000 에서 논다면, 4000 은 곡선의 20% 지점이라
	// 아직 한참 누워 있다. 그럴 때 이 값을 4000 으로 두면 4000 에서 이미 최종 각도가 되고
	// 그 위로는 높이만 올라간다.
	//
	// 0 이하면 MaxArmLength 를 기준으로 쓴다. 기존 동작과 같다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch", meta = (ClampMin = "0.0"))
	float PitchSaturationArmLength = 0.f;

	// 카메라가 초점 높이 기준으로 이 높이보다 아래로 내려오지 않게 한다.
	//
	// 줌인하면 각도가 눕는데, 그러면 카메라가 피벗과 거의 같은 높이까지 내려온다.
	// 예를 들어 거리 800 에 피치 -35 도면 카메라 높이는 800 * sin(35) = 459cm 뿐이고,
	// 뒤로 655cm 물러난 지점이라 거기 지형이 조금만 솟아 있어도 파고든다.
	// 그래서 필요한 만큼 피치를 세워서 높이를 확보한다.
	// 거리를 늘리는 방법도 있지만, 그러면 줌인을 했는데 안 당겨지는 느낌이 난다.
	//
	// 0 이하면 보정하지 않는다.
	//
	// 초점이 지형을 따라다니게 되면서 이 값을 크게 잡을 이유가 줄었다.
	// 예전에는 초점 Z 가 고정이라 언덕에서 파고드는 걸 각도로 막아야 했는데,
	// 이제는 초점 자체가 올라가므로 낮춰도 안전하다.
	// 낮출수록 NPC 를 가까이서 볼 수 있다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch", meta = (ClampMin = "0.0"))
	float MinCameraHeightAboveFocus = 250.f;

	// 최대 줌아웃 상태에서의 초당 팬 속도.
	// 실제 속도는 현재 줌에 비례해서 줄어든다. 안 그러면 확대 상태에서 화면이 홱 날아간다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pan", meta = (ClampMin = "100.0"))
	float PanSpeedAtMaxZoom = 6000.f;

	// 줌인 상태에서 팬 속도가 이 비율까지 줄어든다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pan", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float PanSpeedMinScale = 0.18f;

	// 초당 회전 속도(도). 드래그나 키 홀드에 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation", meta = (ClampMin = "10.0"))
	float YawSpeedDegrees = 120.f;

	// 탭 회전 각도. 45도로 두면 격자 배치와 시선이 맞아떨어진다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation", meta = (ClampMin = "5.0", ClampMax = "180.0"))
	float YawSnapDegrees = 45.f;

	// 목표값을 따라가는 보간 속도. 0 이하면 보간 없이 즉시 반영한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feel", meta = (ClampMin = "0.0"))
	float InterpSpeed = 12.f;

	// true 면 커서 아래 지점을 향해 줌한다. false 면 화면 중앙 기준이다.
	// 배치 작업에서는 커서 기준이 훨씬 편하다. "저기 확대해서 보자"가 한 동작으로 끝난다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zoom")
	bool bZoomToCursor = true;

	// 커서 기준 줌이 초점을 끌어당기는 비율. 1이면 커서 아래 지점이 화면에서 완전히 고정되고,
	// 낮추면 중앙 기준 줌에 가까워진다. 1로 두면 화면 끝을 확대할 때 카메라가 크게 튀어서
	// 멀미가 나는 경우가 있어 조절 여지를 둔다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zoom", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ZoomToCursorStrength = 0.85f;

	// 마우스를 화면 가장자리에 대면 그쪽으로 팬한다. 배치 게임의 표준 조작이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edge Scroll")
	bool bEnableEdgeScroll = true;

	// 가장자리로 인식하는 픽셀 폭. 너무 좁으면 안 걸리고 너무 넓으면 UI 를 누르러 갈 때 화면이 움직인다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edge Scroll", meta = (ClampMin = "1.0"))
	float EdgeScrollMarginPixels = 24.f;

	// 가장자리 스크롤 세기. 키보드 팬 대비 배수다. 1 보다 크면 더 빠르다.
	//
	// 두 조작의 역할이 다르다. 키보드는 보고 있는 곳 근처를 다듬는 세밀 조작이고,
	// 가장자리는 멀리 있는 데로 훅 넘어가는 이동이다. 그래서 기본값을 1 이상으로 둔다.
	// 상한을 열어둔 건 맵이 커지면 더 필요해지기 때문이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edge Scroll", meta = (ClampMin = "0.05", ClampMax = "8.0"))
	float EdgeScrollSpeedScale = 2.5f;

	// 가장자리 스크롤이 확대 상태에서 느려지는 정도.
	//
	// 키보드는 확대할수록 크게 느려져야 세밀 조작이 되는데, 가장자리는 그러면 곤란하다.
	// 부스 하나를 들여다보다가 반대편으로 넘어가려고 마우스를 끝에 댔는데 기어가면
	// 결국 줌아웃하고 다시 줌인하게 된다. 그래서 감속을 따로 둔다.
	// 1 이면 줌과 무관하게 항상 최대 속도다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edge Scroll", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float EdgeScrollMinSpeedScale = 0.6f;

	// 초점을 지형 표면에 붙여 다닐지.
	//
	// 끄면 초점 높이가 고정이라 팬으로 언덕을 넘어갈 때 카메라가 지형을 파고든다.
	// 평지 테스트 맵에서는 안 드러나고 실제 지형에서만 나타난다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	bool bFollowGroundHeight = true;

	// 지면에서 이만큼 띄운 지점을 초점으로 삼는다.
	// 0 이면 지면에 딱 붙는데, 그러면 얕은 각도에서 카메라가 지면을 스친다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground", meta = (ClampMin = "0.0"))
	float GroundFocusOffset = 100.f;

	// 지면 높이가 갑자기 바뀔 때 따라가는 속도.
	// 즉시 붙이면 계단이나 턱을 지날 때 화면이 튄다. 0 이하면 보간 없이 즉시 반영한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground", meta = (ClampMin = "0.0"))
	float GroundFollowSpeed = 6.f;

	// 초점 위아래로 이 범위 안에서만 지면을 찾는다.
	// 너무 좁으면 절벽에서 지면을 놓치고, 너무 넓으면 아래층 바닥을 잡는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground", meta = (ClampMin = "100.0"))
	float GroundTraceHalfHeight = 20000.f;

	// 지면을 찾을 때 쓸 충돌 채널.
	//
	// 기본 Visibility 는 물 표면이나 얇은 소품에도 맞는다. 그런 걸 지면으로 잡으면
	// 카메라가 그 위로 올라탔다 내려온다. 지형만 잡고 싶으면 WorldStatic 이나
	// 전용 채널로 바꾼다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_Visibility;

	// 초점 주변을 몇 군데 더 찍을지. 0 이면 중심 한 점만 본다.
	//
	// 한 점만 보면 벽 끝이나 물가를 지날 때 높이가 뚝 끊긴다. 그 순간 카메라가
	// 방지턱을 넘는 것처럼 출렁인다. 주변을 같이 재서 평균을 내면 경계를 지나는 동안
	// 값이 부드럽게 이어진다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground", meta = (ClampMin = "0", ClampMax = "8"))
	int32 GroundSampleCount = 4;

	// 주변 표본을 얼마나 떨어뜨려 찍을지.
	// 좁으면 경계를 못 걸러내고, 넓으면 멀리 있는 지형까지 끌어와 초점이 붕 뜬다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground", meta = (ClampMin = "10.0"))
	float GroundSampleRadius = 250.f;

	// 중심에서 이만큼 넘게 차이 나는 표본은 버린다.
	//
	// 구멍이나 절벽 아래를 찍은 표본이 평균을 끌어내리면 카메라가 갑자기 가라앉는다.
	// 0 이하면 버리지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground", meta = (ClampMin = "0.0"))
	float GroundSampleMaxDeviation = 800.f;

	// 초점 높이가 초당 이 이상으로는 변하지 않는다.
	//
	// 보간만으로는 큰 단차에서 처음에 확 움직였다가 서서히 멎는다. 그게 방지턱처럼 느껴진다.
	// 상한을 두면 단차가 크든 작든 일정한 속도로 올라가서 예측 가능해진다.
	// 0 이하면 제한하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground", meta = (ClampMin = "0.0"))
	float GroundMaxClimbSpeed = 700.f;

	// 레벨의 Zone 을 모두 감싸는 범위를 경계로 삼는다.
	//
	// 좌표를 손으로 넣으면 맵이 바뀔 때마다 다시 재야 한다. Zone 이 이미 축제장 범위를
	// 알고 있으므로 거기서 가져오는 편이 어긋나지 않는다.
	// 끄면 아래 FocusBounds 를 직접 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounds")
	bool bUseZoneBounds = true;

	// Zone 범위 바깥으로 이만큼은 더 나갈 수 있다.
	// 0 이면 경계에 딱 붙어서 가장자리 시설이 화면 끝에 걸린다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounds", meta = (ClampMin = "0.0"))
	float ZoneBoundsPadding = 3000.f;

	// 카메라 초점이 벗어나지 못하는 영역. 유효하지 않으면 클램프하지 않는다.
	// 축제장 밖으로 나가면 빈 지형만 보여서 길을 잃는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounds")
	FBox FocusBounds = FBox(ForceInit);

	bool IsValid() const
	{
		return MaxArmLength > MinArmLength
			&& MinArmLength > 0.f
			&& ZoomStepMultiplier > 1.f;
	}
};

// 카메라의 현재 목표 지점. 실제 컴포넌트 값이 아니라 "가려는 곳"이다.
USTRUCT(BlueprintType)
struct FFF_OverviewCameraState
{
	GENERATED_BODY()

	// 스프링암이 매달린 지점. 지면 위의 한 점이라고 보면 된다.
	UPROPERTY(BlueprintReadOnly, Category = "Camera")
	FVector FocusLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Camera")
	float ArmLength = 8000.f;

	UPROPERTY(BlueprintReadOnly, Category = "Camera")
	float Yaw = 0.f;

	// ArmLength 에서 파생된다. 따로 저장하지 않고 매번 계산한다.
	UPROPERTY(BlueprintReadOnly, Category = "Camera")
	float Pitch = -60.f;
};
