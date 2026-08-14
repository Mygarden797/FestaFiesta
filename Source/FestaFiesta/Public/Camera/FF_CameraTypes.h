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
	// 건축 세션 진행 중. 팬과 줌은 살리고 피킹만 막는다.
	BuildSession    UMETA(DisplayName = "건축 세션"),
	// 모달 UI가 떠 있음. 카메라 입력 전체를 막는다.
	ModalUi         UMETA(DisplayName = "모달 UI"),
	// 다른 액터 시점을 보는 중. 관찰 카메라 조작은 화면에 안 보이므로 막는다.
	ActorView       UMETA(DisplayName = "액터 시점"),
	// 캐릭터를 조종하는 중. 같은 WASD 가 카메라 팬으로도 먹으면 안 된다.
	CharacterControl UMETA(DisplayName = "캐릭터 조종"),
	// 시네마틱이나 자동 연출 중.
	Scripted        UMETA(DisplayName = "연출 중")
};

// 디자이너가 조정하는 값들. 전부 게임 단위(cm, 도, 초)다.
USTRUCT(BlueprintType)
struct FFF_OverviewCameraSettings
{
	GENERATED_BODY()

	// 가장 가까이 붙었을 때의 스프링암 길이. NPC 표정이 보이는 거리.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zoom", meta = (ClampMin = "100.0"))
	float MinArmLength = 800.f;

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch", meta = (ClampMin = "0.0"))
	float MinCameraHeightAboveFocus = 600.f;

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
