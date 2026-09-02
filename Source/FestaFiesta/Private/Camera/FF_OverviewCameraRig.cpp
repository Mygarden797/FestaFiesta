// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/FF_OverviewCameraRig.h"

DEFINE_LOG_CATEGORY(LogFestaFiestaCamera);

void FFF_OverviewCameraRig::Initialize(const FFF_OverviewCameraSettings& InSettings, const FVector& InFocusLocation, float InArmLength)
{
	Settings = InSettings;

	// 설정이 뒤집혀 있으면 줌이 반대로 돌거나 0으로 나눈다. 조용히 이상하게 도느니 고쳐서 쓴다.
	if (!Settings.IsValid())
	{
		UE_LOG(LogFestaFiestaCamera, Warning,
			TEXT("[카메라] 설정이 유효하지 않아 기본값으로 대체한다. Min=%.1f Max=%.1f Step=%.2f"),
			Settings.MinArmLength, Settings.MaxArmLength, Settings.ZoomStepMultiplier);
		Settings.MinArmLength = FMath::Max(100.f, Settings.MinArmLength);
		Settings.MaxArmLength = FMath::Max(Settings.MinArmLength * 2.f, Settings.MaxArmLength);
		Settings.ZoomStepMultiplier = FMath::Max(1.01f, Settings.ZoomStepMultiplier);
	}

	Target.FocusLocation = InFocusLocation;
	Target.ArmLength = FMath::Clamp(InArmLength, Settings.MinArmLength, Settings.MaxArmLength);
	Target.Yaw = 0.f;
	Target.Pitch = ResolvePitchForArmLength(Target.ArmLength);

	ClampFocusToBounds(Target.FocusLocation);

	InitialFocusLocation = Target.FocusLocation;
	InitialArmLength = Target.ArmLength;

	SnapToTarget();
}

float FFF_OverviewCameraRig::NormalizeYaw(float InYaw)
{
	// -180 ~ 180 으로 접는다. 계속 더하기만 하면 값이 무한정 커져서 보간이 이상해진다.
	return FMath::UnwindDegrees(InYaw);
}

float FFF_OverviewCameraRig::GetNormalizedZoom() const
{
	const float Range = Settings.MaxArmLength - Settings.MinArmLength;
	if (Range <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	return FMath::Clamp((Target.ArmLength - Settings.MinArmLength) / Range, 0.f, 1.f);
}

float FFF_OverviewCameraRig::ResolvePitchForArmLength(float InArmLength) const
{
	// 각도가 다 눕는 지점. 지정이 없으면 줌 상한을 그대로 쓴다.
	float SaturationLength = Settings.PitchSaturationArmLength;
	if (SaturationLength <= 0.f)
	{
		SaturationLength = Settings.MaxArmLength;
	}

	// 최소 거리보다 작게 잡아두면 0 으로 나누거나 각도가 뒤집힌다. 조용히 이상하게 도느니 되돌린다.
	SaturationLength = FMath::Max(SaturationLength, Settings.MinArmLength + KINDA_SMALL_NUMBER);

	float StartPitch = Settings.PitchAtMinZoom;
	float EndPitch = Settings.PitchAtMaxZoom;
	ApplyMinHeightConstraint(StartPitch, EndPitch);

	const float Range = SaturationLength - Settings.MinArmLength;
	if (Range <= KINDA_SMALL_NUMBER)
	{
		return EndPitch;
	}

	// Clamp 덕분에 SaturationLength 를 넘어서면 각도는 그대로 유지되고 높이만 올라간다.
	const float Alpha = FMath::Clamp((InArmLength - Settings.MinArmLength) / Range, 0.f, 1.f);
	return FMath::Lerp(StartPitch, EndPitch, Alpha);
}

void FFF_OverviewCameraRig::ApplyMinHeightConstraint(float& InOutStartPitch, float& InOutEndPitch) const
{
	const float MinHeight = Settings.MinCameraHeightAboveFocus;
	if (MinHeight <= 0.f || Settings.MinArmLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// 거리마다 따로 보정하면 안 된다. 필요한 각도는 거리가 멀수록 완만해지는데 기본 곡선은
	// 반대로 가팔라져서, 둘 중 센 쪽을 그때그때 고르면 곡선에 V 자 꺾임이 생긴다.
	// 실제로는 "최대 줌인에서 잠깐 세워졌다가 조금 물러나면 도로 눕는" 현상으로 보인다.
	// 그래서 시작 각도 자체를 한 번만 올리고 곡선 전체를 옮긴다.
	//
	// 시작점에서 최소 높이를 만족하면 그 위로는 각도도 거리도 함께 커지므로 자동으로 만족된다.
	const float SinNeeded = FMath::Clamp(MinHeight / Settings.MinArmLength, 0.f, 1.f);
	const float RequiredStartPitch = -FMath::RadiansToDegrees(FMath::Asin(SinNeeded));

	// 세우는 방향으로만 움직인다. 눕히는 쪽으로는 절대 가지 않는다.
	InOutStartPitch = FMath::Min(InOutStartPitch, RequiredStartPitch);

	// 요구 높이가 지나치게 커서 시작 각도가 최종 각도보다 가팔라진 경우.
	// 그대로 두면 줌아웃할수록 각도가 눕는다. 끝도 같이 세워서 단조성을 지킨다.
	InOutEndPitch = FMath::Min(InOutEndPitch, InOutStartPitch);
}

float FFF_OverviewCameraRig::GetCurrentPanSpeed() const
{
	// 확대할수록 느리게. 화면에서 느껴지는 이동량을 비슷하게 맞추기 위한 것이다.
	const float Scale = FMath::Lerp(Settings.PanSpeedMinScale, 1.f, GetNormalizedZoom());
	return Settings.PanSpeedAtMaxZoom * Scale;
}

float FFF_OverviewCameraRig::GetCurrentEdgeScrollSpeed() const
{
	// 키보드와 감속 곡선을 따로 둔다. 가장자리는 멀리 넘어가는 용도라 확대 상태에서도
	// 어느 정도 속도가 남아 있어야 한다.
	const float Scale = FMath::Lerp(Settings.EdgeScrollMinSpeedScale, 1.f, GetNormalizedZoom());
	return Settings.PanSpeedAtMaxZoom * Scale * Settings.EdgeScrollSpeedScale;
}

void FFF_OverviewCameraRig::ClampFocusToBounds(FVector& InOutFocus) const
{
	if (!Settings.FocusBounds.IsValid)
	{
		return;
	}
	InOutFocus.X = FMath::Clamp(InOutFocus.X, Settings.FocusBounds.Min.X, Settings.FocusBounds.Max.X);
	InOutFocus.Y = FMath::Clamp(InOutFocus.Y, Settings.FocusBounds.Min.Y, Settings.FocusBounds.Max.Y);
	InOutFocus.Z = FMath::Clamp(InOutFocus.Z, Settings.FocusBounds.Min.Z, Settings.FocusBounds.Max.Z);
}

void FFF_OverviewCameraRig::Pan(const FVector2D& ScreenInput, float DeltaSeconds)
{
	ApplyPanWithSpeed(ScreenInput, DeltaSeconds, GetCurrentPanSpeed());
}

void FFF_OverviewCameraRig::EdgeScroll(const FVector2D& ScreenInput, float DeltaSeconds)
{
	ApplyPanWithSpeed(ScreenInput, DeltaSeconds, GetCurrentEdgeScrollSpeed());
}

void FFF_OverviewCameraRig::ApplyPanWithSpeed(const FVector2D& ScreenInput, float DeltaSeconds, float Speed)
{
	if (ScreenInput.IsNearlyZero() || DeltaSeconds <= 0.f)
	{
		return;
	}

	// 화면 입력을 현재 Yaw 만큼 돌려서 월드 방향으로 바꾼다.
	// 이걸 안 하면 카메라를 90도 돌린 뒤 "위로" 밀었을 때 옆으로 간다.
	const FRotator YawOnly(0.f, Target.Yaw, 0.f);
	const FVector Forward = YawOnly.RotateVector(FVector::ForwardVector);
	const FVector Right = YawOnly.RotateVector(FVector::RightVector);

	// 대각선 입력이 축 입력보다 빠르지 않게 정규화한다.
	// 속도 배율은 여기서 곱하지 않고 Speed 로 받는다. 입력에 미리 곱해서 넘기면
	// 이 정규화에 잘려서 배율이 통째로 사라진다.
	FVector2D Normalized = ScreenInput;
	if (Normalized.SizeSquared() > 1.f)
	{
		Normalized.Normalize();
	}

	const FVector Delta = (Forward * Normalized.Y + Right * Normalized.X) * Speed * DeltaSeconds;
	Target.FocusLocation += Delta;
	ClampFocusToBounds(Target.FocusLocation);
}

void FFF_OverviewCameraRig::Zoom(float Steps)
{
	if (FMath::IsNearlyZero(Steps))
	{
		return;
	}

	// 곱셈 줌. Steps 가 양수면 가까워진다.
	const float Factor = FMath::Pow(Settings.ZoomStepMultiplier, -Steps);
	Target.ArmLength = FMath::Clamp(Target.ArmLength * Factor, Settings.MinArmLength, Settings.MaxArmLength);
	Target.Pitch = ResolvePitchForArmLength(Target.ArmLength);
}

void FFF_OverviewCameraRig::ZoomTowards(float Steps, const FVector& WorldAnchor)
{
	if (FMath::IsNearlyZero(Steps))
	{
		return;
	}

	const float PreviousArmLength = Target.ArmLength;
	Zoom(Steps);

	// 클램프에 걸려 실제로 안 움직였으면 초점도 건드리면 안 된다.
	// 안 그러면 최대 줌 상태에서 휠을 계속 굴릴 때 화면이 커서 쪽으로 계속 기어간다.
	if (FMath::IsNearlyEqual(PreviousArmLength, Target.ArmLength))
	{
		return;
	}

	if (PreviousArmLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float Ratio = Target.ArmLength / PreviousArmLength;
	const float Strength = FMath::Clamp(Settings.ZoomToCursorStrength, 0.f, 1.f);

	// Ratio 로 보간하면 앵커가 완전히 고정된다. Strength 로 그 효과를 약하게 만들 수 있다.
	const FVector FullyAnchored = FMath::Lerp(WorldAnchor, Target.FocusLocation, Ratio);
	Target.FocusLocation = FMath::Lerp(Target.FocusLocation, FullyAnchored, Strength);

	ClampFocusToBounds(Target.FocusLocation);
}

bool FFF_OverviewCameraRig::ProjectRayToHorizontalPlane(const FVector& RayOrigin, const FVector& RayDirection,
	float PlaneZ, FVector& OutHit)
{
	// 광선이 평면과 거의 평행하면 교점이 무한대로 날아간다.
	if (FMath::Abs(RayDirection.Z) < KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float Distance = (PlaneZ - RayOrigin.Z) / RayDirection.Z;

	// 카메라 뒤쪽에서 만나는 경우. 위를 보고 있을 때 생긴다.
	if (Distance < 0.f)
	{
		return false;
	}

	OutHit = RayOrigin + RayDirection * Distance;
	return true;
}

FVector2D FFF_OverviewCameraRig::ComputeEdgeScrollInput(const FVector2D& MousePosition, const FVector2D& ViewportSize,
	float MarginPixels)
{
	if (ViewportSize.X <= 0.f || ViewportSize.Y <= 0.f || MarginPixels <= 0.f)
	{
		return FVector2D::ZeroVector;
	}

	// 창 밖으로 나간 커서는 무시한다. 다른 창을 쓰는 중에 화면이 흘러가면 안 된다.
	if (MousePosition.X < 0.f || MousePosition.Y < 0.f
		|| MousePosition.X > ViewportSize.X || MousePosition.Y > ViewportSize.Y)
	{
		return FVector2D::ZeroVector;
	}

	// 가장자리에 가까울수록 1에 가까워진다. 경계에서 갑자기 최대 속도가 되면 튀어 보인다.
	FVector2D Input = FVector2D::ZeroVector;

	if (MousePosition.X < MarginPixels)
	{
		Input.X = -(1.f - MousePosition.X / MarginPixels);
	}
	else if (MousePosition.X > ViewportSize.X - MarginPixels)
	{
		Input.X = 1.f - (ViewportSize.X - MousePosition.X) / MarginPixels;
	}

	// 화면 좌표는 아래로 갈수록 Y 가 커진다. 팬 입력의 Y 는 화면 위쪽이 양수라 부호를 뒤집는다.
	if (MousePosition.Y < MarginPixels)
	{
		Input.Y = 1.f - MousePosition.Y / MarginPixels;
	}
	else if (MousePosition.Y > ViewportSize.Y - MarginPixels)
	{
		Input.Y = -(1.f - (ViewportSize.Y - MousePosition.Y) / MarginPixels);
	}

	return Input;
}

void FFF_OverviewCameraRig::Rotate(float Direction, float DeltaSeconds)
{
	if (FMath::IsNearlyZero(Direction) || DeltaSeconds <= 0.f)
	{
		return;
	}
	Target.Yaw = NormalizeYaw(Target.Yaw + Direction * Settings.YawSpeedDegrees * DeltaSeconds);
}

void FFF_OverviewCameraRig::SnapRotate(int32 Direction)
{
	if (Direction == 0)
	{
		return;
	}

	const float Step = FMath::Max(1.f, Settings.YawSnapDegrees);

	// 이미 스냅 각도에 서 있으면 한 칸 옮기고, 어중간하게 서 있으면 진행 방향의 다음 칸으로 붙인다.
	// 반올림만 쓰면 45도에서 46도로 살짝 돌아간 상태에서 오른쪽 탭이 제자리로 되돌아가 버린다.
	const float Current = Target.Yaw / Step;
	const float Next = (Direction > 0) ? FMath::FloorToFloat(Current + 1.f) : FMath::CeilToFloat(Current - 1.f);

	Target.Yaw = NormalizeYaw(Next * Step);
}

void FFF_OverviewCameraRig::ApplyGroundHeight(const float GroundZ, const float DeltaSeconds)
{
	if (!Settings.bFollowGroundHeight)
	{
		return;
	}

	const float DesiredZ = GroundZ + Settings.GroundFocusOffset;

	// 처음 받았으면 보간 없이 붙인다. 시작하자마자 카메라가 천천히 내려오면
	// 레벨이 로딩 중인 것처럼 보인다.
	if (!bHasGroundHeight || Settings.GroundFollowSpeed <= 0.f || DeltaSeconds <= 0.f)
	{
		Target.FocusLocation.Z = DesiredZ;
		bHasGroundHeight = true;
	}
	else
	{
		const double PreviousZ = Target.FocusLocation.Z;
		double NextZ = FMath::FInterpTo(PreviousZ, DesiredZ, DeltaSeconds, Settings.GroundFollowSpeed);

		// 보간만 쓰면 단차가 클수록 첫 프레임에 크게 튄다. 그게 방지턱처럼 느껴진다.
		// 초당 이동량에 상한을 두면 단차 크기와 무관하게 일정한 속도로 올라간다.
		if (Settings.GroundMaxClimbSpeed > 0.f)
		{
			const double MaxStep = Settings.GroundMaxClimbSpeed * DeltaSeconds;
			NextZ = FMath::Clamp(NextZ, PreviousZ - MaxStep, PreviousZ + MaxStep);
		}

		Target.FocusLocation.Z = NextZ;
	}

	// 경계가 Z 를 제한하고 있으면 그쪽이 이긴다. 지형을 따라가더라도
	// 축제장 밖 높이로 나가면 안 된다.
	ClampFocusToBounds(Target.FocusLocation);
}

void FFF_OverviewCameraRig::OnGroundHeightUnavailable()
{
	// 아무것도 하지 않는다. 마지막 높이가 그대로 유지된다.
	//
	// 지면을 못 찾는 경우는 절벽 위, 물 위, 콜리전이 없는 구역 등이다.
	// 그때 기본 높이로 되돌리면 카메라가 갑자기 떨어지는데, 그건 파고드는 것보다 나쁘다.
}

void FFF_OverviewCameraRig::FocusOn(const FVector& WorldLocation)
{
	Target.FocusLocation = WorldLocation;
	ClampFocusToBounds(Target.FocusLocation);
}

void FFF_OverviewCameraRig::ResetToInitial()
{
	Target.FocusLocation = InitialFocusLocation;

	// 초기 구도로 돌아가면 지면도 다시 잡는다. 안 그러면 다음 트레이스까지
	// 초기 Z 와 현재 지형이 어긋난 채로 있다.
	bHasGroundHeight = false;
	Target.ArmLength = InitialArmLength;
	Target.Yaw = 0.f;
	Target.Pitch = ResolvePitchForArmLength(Target.ArmLength);
}

void FFF_OverviewCameraRig::SnapToTarget()
{
	Display = Target;
}

void FFF_OverviewCameraRig::AdvanceInterpolation(float DeltaSeconds)
{
	if (Settings.InterpSpeed <= 0.f || DeltaSeconds <= 0.f)
	{
		SnapToTarget();
		return;
	}

	Display.FocusLocation = FMath::VInterpTo(Display.FocusLocation, Target.FocusLocation, DeltaSeconds, Settings.InterpSpeed);
	Display.ArmLength = FMath::FInterpTo(Display.ArmLength, Target.ArmLength, DeltaSeconds, Settings.InterpSpeed);
	Display.Pitch = FMath::FInterpTo(Display.Pitch, Target.Pitch, DeltaSeconds, Settings.InterpSpeed);

	// Yaw 는 최단 경로로 돌아야 한다. 그냥 보간하면 179도에서 -179도로 갈 때 한 바퀴 돈다.
	Display.Yaw = NormalizeYaw(FMath::FInterpTo(
		Display.Yaw,
		Display.Yaw + NormalizeYaw(Target.Yaw - Display.Yaw),
		DeltaSeconds,
		Settings.InterpSpeed));
}
