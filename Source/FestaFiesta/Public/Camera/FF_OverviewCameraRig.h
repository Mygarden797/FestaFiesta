// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/FF_CameraTypes.h"

// 입력 지속 여부와 일회성 명령을 위치/회전/줌별로 관리한다.
// 예: 줌 전환 중 이동 키를 놓아도 줌은 완료하고 수평 이동은 멈출 수 있다.
enum class EFF_CameraMotionChannel : uint8
{
	None = 0,
	Focus = 1 << 0,
	Rotation = 1 << 1,
	Zoom = 1 << 2,
	All = (1 << 0) | (1 << 1) | (1 << 2)
};
ENUM_CLASS_FLAGS(EFF_CameraMotionChannel);

// 탑다운 카메라의 계산 부분만 떼어낸 것.
//
// UObject 도 Actor 도 아니고 월드를 모른다. 에디터를 안 켜도 단위 테스트가 되는 게 목적이다.
// 카메라 버그는 "왜 여기서 안 넘어가지", "왜 확대하면 홱 날아가지"처럼 수치 문제가 대부분인데,
// 그건 화면을 보면서 잡는 것보다 값을 찍어보는 게 빠르다.
//
// Pawn 은 이 클래스가 계산한 결과를 SpringArm 에 옮겨 담기만 한다.
class FFF_OverviewCameraRig
{
public:
	void Initialize(const FFF_OverviewCameraSettings& InSettings, const FVector& InFocusLocation, float InArmLength);

	// 화면 기준 입력을 월드 이동으로 바꾼다. X 는 오른쪽, Y 는 화면 위쪽이다.
	// 현재 Yaw 를 기준으로 회전시켜야 "위로 밀면 보이는 쪽으로 간다"가 성립한다.
	// SpeedScale 은 속도에 곱한다(Shift 가속). 입력에 곱하면 대각선 정규화에 잘려 사라진다.
	void Pan(const FVector2D& ScreenInput, float DeltaSeconds, float SpeedScale = 1.f);

	// 화면 가장자리 스크롤. 키보드와 속도 곡선이 다르다.
	// 배율을 입력에 곱해서 Pan 에 넘기면 안 된다 — 그쪽은 크기 1 로 정규화해서 배율이 사라진다.
	void EdgeScroll(const FVector2D& ScreenInput, float DeltaSeconds);

	// Steps 가 양수면 당겨오고 음수면 물러난다. 곱셈이라 어느 거리에서도 체감이 같다.
	bool Zoom(float Steps);

	// 커서 아래 월드 지점을 화면에 고정한 채 줌한다.
	//
	// 원리는 간단하다. 화면에서 앵커가 중심으로부터 떨어진 거리는 스프링암 길이에 비례한다.
	// 그래서 팔 길이가 k 배로 줄면 초점을 앵커 쪽으로 (1-k) 만큼 당기면 앵커가 제자리에 남는다.
	// 결과적으로 새 초점은 앵커와 옛 초점 사이를 k 로 보간한 지점이 된다.
	bool ZoomTowards(float Steps, const FVector& WorldAnchor);

	// 커서 광선을 초점 높이의 수평면에 떨어뜨린다.
	//
	// 실제 지오메트리를 트레이스하지 않는 이유는, 천막 지붕이나 간판에 맞으면 앵커가 공중으로
	// 튀어서 줌이 이상하게 빨려들기 때문이다. 평면 교차가 예측 가능하고 비용도 없다.
	// 광선이 평면과 안 만나거나 뒤쪽에서 만나면 false 를 돌려준다.
	static bool ProjectRayToHorizontalPlane(const FVector& RayOrigin, const FVector& RayDirection,
		float PlaneZ, FVector& OutHit);

	// 마우스 위치를 화면 기준 팬 입력으로 바꾼다. 가장자리에 가까울수록 세진다.
	// 뷰포트 밖이거나 가장자리가 아니면 0 벡터다.
	static FVector2D ComputeEdgeScrollInput(const FVector2D& MousePosition, const FVector2D& ViewportSize,
		float MarginPixels);

	// 홀드 회전.
	void Rotate(float Direction, float DeltaSeconds);

	// 탭 회전. 가장 가까운 스냅 각도로 붙인다.
	void SnapRotate(int32 Direction);

	// 지면 높이를 알려준다. 리그는 월드를 모르므로 트레이스는 Pawn 이 하고 결과만 받는다.
	//
	// 즉시 붙이지 않고 GroundFollowSpeed 로 따라간다. 계단이나 턱을 지날 때
	// 초점이 순간이동하면 화면이 튄다.
	void ApplyGroundHeight(float GroundZ, float DeltaSeconds);

	// 지면을 못 찾았을 때. 마지막 높이를 유지한다.
	// 0 으로 되돌리면 절벽이나 물 위를 지날 때 카메라가 바닥으로 떨어진다.
	void OnGroundHeightUnavailable();

	// 특정 지점을 화면 중앙에 둔다. NPC 추적이나 시설 포커스에 쓴다.
	void FocusOn(const FVector& WorldLocation);

	// 초기 구도로 복원한다.
	void ResetToInitial();

	// 목표 상태를 향해 한 프레임 보간한다. InterpSpeed 가 0 이하면 즉시 스냅한다.
	void AdvanceInterpolation(float DeltaSeconds);

	// 보간 없이 현재 목표를 그대로 표시 상태로 만든다. 초기화 직후에 쓴다.
	void SnapToTarget();

	// 정지는 목표로 순간이동하는 것이 아니라, 현재 화면을 새로운 목표로 삼는 것이다.
	void HoldChannels(EFF_CameraMotionChannel Channels);
	void SnapChannelsToTarget(EFF_CameraMotionChannel Channels);
	bool HasReachedTarget(EFF_CameraMotionChannel Channels) const;
	bool HasReachedGroundHeight(float GroundZ) const;

	const FFF_OverviewCameraState& GetDisplayState() const { return Display; }
	const FFF_OverviewCameraState& GetTargetState() const { return Target; }
	const FFF_OverviewCameraSettings& GetSettings() const { return Settings; }

	// 설정을 갈아끼운다. 초점과 줌 상태는 그대로 두고 규칙만 바꾼다.
	//
	// Zone 경계처럼 레벨이 뜬 뒤에야 알 수 있는 값을 넣을 때 쓴다.
	// Initialize 를 다시 부르면 구도가 초기화되어 화면이 튄다.
	void UpdateSettings(const FFF_OverviewCameraSettings& InSettings);

	// ArmLength 를 0(가장 가까움)~1(가장 멂) 로 정규화한 값.
	float GetNormalizedZoom() const;

	// 현재 줌에서의 키보드 팬 속도. 확대할수록 크게 느려진다.
	float GetCurrentPanSpeed() const;

	// 현재 줌에서의 가장자리 스크롤 속도.
	// 감속 폭이 작아서 확대 상태에서도 멀리 넘어갈 수 있다.
	float GetCurrentEdgeScrollSpeed() const;

	// ArmLength 에 대응하는 피치. 줌과 각도를 따로 조작하게 두면 플레이어가 구도를 망친다.
	float ResolvePitchForArmLength(float InArmLength) const;

private:
	// 화면 기준 입력을 주어진 속도로 월드 이동으로 바꾼다. Pan 과 EdgeScroll 이 공유한다.
	void ApplyPanWithSpeed(const FVector2D& ScreenInput, float DeltaSeconds, float Speed);

	// 줌인으로 각도가 누웠을 때 카메라가 지면으로 내려오는 것을 막는다.
	// 거리마다 보정하지 않고 곡선의 시작 각도를 올린다. 거리별 보정은 꺾임을 만든다.
	void ApplyMinHeightConstraint(float& InOutStartPitch, float& InOutEndPitch) const;

	void ClampFocusToBounds(FVector& InOutFocus) const;
	static float NormalizeYaw(float InYaw);

	FFF_OverviewCameraSettings Settings;
	FFF_OverviewCameraState Target;
	FFF_OverviewCameraState Display;

	// 지면 높이를 한 번이라도 받았는지. 받기 전에는 초기 Z 를 그대로 쓴다.
	bool bHasGroundHeight = false;

	FVector InitialFocusLocation = FVector::ZeroVector;
	float InitialArmLength = 8000.f;
};
