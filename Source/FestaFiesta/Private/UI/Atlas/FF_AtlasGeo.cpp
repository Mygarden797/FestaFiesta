// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Atlas/FF_AtlasGeo.h"

namespace
{
	/** 각도 하나를 -180~180 으로 감는다. `FMath::UnwindDegrees` 를 이름으로 감싸 의도를 남긴다. */
	float AtlasWrapDegrees(float Degrees)
	{
		return FMath::UnwindDegrees(Degrees);
	}

	/**
	 * UE5 의 `FVector` 는 **double** 이고 이 파일의 공개 각도는 float 다.
	 * 계산은 double 로 하고 **좁히는 지점을 이 한 곳으로 모은다** — 경계에서 암묵 변환을
	 * 흘리면 경고가 쌓이고 `FMath::Clamp` 같은 템플릿은 아예 추론에 실패한다.
	 */
	float AtlasToFloat(double Value)
	{
		return static_cast<float>(Value);
	}
}

FVector FFF_AtlasGeo::LatLongToUnitVector(const FFF_AtlasLatLong& LatLong)
{
	const double LatitudeRad = FMath::DegreesToRadians(static_cast<double>(LatLong.Latitude));
	const double LongitudeRad = FMath::DegreesToRadians(static_cast<double>(LatLong.Longitude));

	const double CosLatitude = FMath::Cos(LatitudeRad);

	// 규약: 경도 0 = +X, 경도 +90(동쪽) = **-Y**, 위도 +90 = +Z.
	// Y 의 음수 부호가 "동쪽이 화면 오른쪽" 을 만든다. 헤더의 왼손 좌표계 항목 참고.
	return FVector(
		CosLatitude * FMath::Cos(LongitudeRad),
		-CosLatitude * FMath::Sin(LongitudeRad),
		FMath::Sin(LatitudeRad));
}

FFF_AtlasLatLong FFF_AtlasGeo::UnitVectorToLatLong(const FVector& Direction)
{
	// 영벡터를 정규화하면 (0,0,0) 이 남는다. 결과는 아래에서 (0, 0) 이 되므로
	// 호출부가 "정규화 실패" 를 신경 쓰지 않아도 된다.
	const FVector Normal = Direction.GetSafeNormal();

	// 수평 성분. `FMath::Hypot` 은 UE 에 없으므로 엔진이 하는 대로 직접 쓴다.
	const double Horizontal = FMath::Sqrt(Normal.X * Normal.X + Normal.Y * Normal.Y);

	// Asin(Z) 이 아니라 Atan2 다. 이유는 헤더 주석에 있다 (극점 근처 정밀도).
	const double LatitudeRad = FMath::Atan2(Normal.Z, Horizontal);

	// 극점에서는 경도가 정의되지 않는다(모든 경도가 같은 점이다). 0 으로 고정한다.
	// 동쪽이 -Y 이므로 Atan2 의 첫 인자에 부호를 되돌려 넣는다.
	const double LongitudeRad = (Horizontal <= PoleEpsilon)
		? 0.0
		: FMath::Atan2(-Normal.Y, Normal.X);

	return FFF_AtlasLatLong(
		AtlasToFloat(FMath::RadiansToDegrees(LatitudeRad)),
		AtlasToFloat(FMath::RadiansToDegrees(LongitudeRad)));
}

FVector FFF_AtlasGeo::LatLongToSurfacePosition(const FFF_AtlasLatLong& LatLong, float Radius)
{
	return LatLongToUnitVector(LatLong) * static_cast<double>(Radius);
}

FFF_AtlasLatLong FFF_AtlasGeo::NormalizeLatLong(const FFF_AtlasLatLong& LatLong)
{
	// 위도는 감지 않고 자른다. 91도를 89도로 감으면 반대쪽 경도로 넘어가는 것이 수학적으로 맞지만,
	// 데이터 입력 실수를 그렇게 "고쳐" 주면 핀이 지구 반대편에 조용히 찍힌다.
	return FFF_AtlasLatLong(
		FMath::Clamp(LatLong.Latitude, -90.f, 90.f),
		AtlasWrapDegrees(LatLong.Longitude));
}

FFF_AtlasGlobeOrientation FFF_AtlasGeo::MakeOrientationFacing(const FFF_AtlasLatLong& LatLong)
{
	const FFF_AtlasLatLong Normalized = NormalizeLatLong(LatLong);

	// Rz(경도) 가 그 경도를 +X 로 가져오고, 이어지는 Ry(위도) 가 위도를 0 으로 눕힌다.
	return FFF_AtlasGlobeOrientation(Normalized.Longitude, Normalized.Latitude);
}

FFF_AtlasGlobeOrientation FFF_AtlasGeo::MakeFocusOrientation(
	const FFF_AtlasLatLong& LatLong,
	float MaxTiltDegrees)
{
	return NormalizeOrientation(MakeOrientationFacing(LatLong), MaxTiltDegrees);
}

FFF_AtlasGlobeOrientation FFF_AtlasGeo::MakeFocusOrientationToward(
	const FFF_AtlasLatLong& LatLong,
	const FVector& DesiredLocalDirection,
	float MaxTiltDegrees)
{
	const FFF_AtlasLatLong SafeLocation = NormalizeLatLong(FFF_AtlasLatLong(
		FMath::IsFinite(LatLong.Latitude) ? LatLong.Latitude : 0.f,
		// UnwindDegrees는 반복 감기이므로 극단적으로 큰 유한 입력을 먼저 한 바퀴로 줄인다.
		FMath::IsFinite(LatLong.Longitude) ? FMath::Fmod(LatLong.Longitude, 360.f) : 0.f));
	const float SafeMaxTilt = FMath::IsFinite(MaxTiltDegrees)
		? FMath::Clamp(FMath::Abs(MaxTiltDegrees), 0.f, 90.f) : 0.f;
	const FFF_AtlasGlobeOrientation Fallback = MakeFocusOrientation(SafeLocation, SafeMaxTilt);
	if (!FMath::IsFinite(DesiredLocalDirection.X) || !FMath::IsFinite(DesiredLocalDirection.Y)
		|| !FMath::IsFinite(DesiredLocalDirection.Z))
	{
		return Fallback;
	}

	// 먼저 최대 성분으로 나누면 큰 유한 벡터의 길이 제곱이 넘치는 경우도 안전하다.
	const double DirectionScale = DesiredLocalDirection.GetAbsMax();
	if (DirectionScale <= UE_DOUBLE_SMALL_NUMBER)
	{
		return Fallback;
	}
	const FVector Desired = (DesiredLocalDirection / DirectionScale).GetSafeNormal();
	if (Desired.Equals(FVector::ForwardVector, UE_DOUBLE_SMALL_NUMBER))
	{
		return Fallback;
	}

	const double Latitude = FMath::DegreesToRadians(static_cast<double>(SafeLocation.Latitude));
	const double Longitude = FMath::DegreesToRadians(static_cast<double>(SafeLocation.Longitude));
	const double MaxTilt = FMath::DegreesToRadians(static_cast<double>(SafeMaxTilt));
	const double CosLatitude = FMath::Cos(Latitude);
	const double SinLatitude = FMath::Sin(Latitude);
	FFF_AtlasGlobeOrientation Best = Fallback;
	double BestDot = FVector::DotProduct(
		OrientationToQuat(Best).RotateVector(LatLongToUnitVector(SafeLocation)), Desired);

	// 기울기 하나가 정해지면 역회전한 목표의 XY 방향이 최적 자전각이다.
	// 최대 내적 = cos(위도)*sqrt(1-A*A) + sin(위도)*A,
	// A = Desired.X*sin(Tilt) + Desired.Z*cos(Tilt).
	// 최적점은 A=sin(위도)의 정확한 해, Tilt 구간 끝, 또는 A의 극값 중 하나다.
	const auto ConsiderTilt = [&](double Tilt)
	{
		if (Tilt < -MaxTilt - 1.e-10 || Tilt > MaxTilt + 1.e-10)
		{
			return;
		}
		Tilt = FMath::Clamp(Tilt, -MaxTilt, MaxTilt);
		const double SinTilt = FMath::Sin(Tilt);
		const double CosTilt = FMath::Cos(Tilt);
		const double TargetX = Desired.X * CosTilt - Desired.Z * SinTilt;
		const double TargetZ = Desired.X * SinTilt + Desired.Z * CosTilt;
		const double Horizontal = FMath::Sqrt(TargetX * TargetX + Desired.Y * Desired.Y);
		const double Dot = CosLatitude * Horizontal + SinLatitude * TargetZ;
		if (Dot > BestDot + 1.e-12)
		{
			// 극점 또는 목표가 Z축인 동률에서는 경도를 그대로 두어 자전각이 튀지 않게 한다.
			const double SpinOffset = CosLatitude <= PoleEpsilon || Horizontal <= PoleEpsilon
				? 0.0 : FMath::Atan2(Desired.Y, TargetX);
			Best = FFF_AtlasGlobeOrientation(
				AtlasWrapDegrees(AtlasToFloat(FMath::RadiansToDegrees(Longitude + SpinOffset))),
				AtlasToFloat(FMath::RadiansToDegrees(Tilt)));
			BestDot = Dot;
		}
	};

	ConsiderTilt(FMath::DegreesToRadians(static_cast<double>(Fallback.Tilt)));
	// Ry는 Y를 바꾸지 않는다. sin(Spin-경도)=Desired.Y/cos(위도)의 두 가지 해를 본다.
	if (CosLatitude > PoleEpsilon && FMath::Abs(Desired.Y) <= CosLatitude + 1.e-12)
	{
		const double Offset = FMath::Asin(FMath::Clamp(Desired.Y / CosLatitude, -1.0, 1.0));
		for (const double SpinOffset : { Offset, UE_DOUBLE_PI - Offset })
		{
			const double Tilt = FMath::Atan2(SinLatitude, CosLatitude * FMath::Cos(SpinOffset))
				- FMath::Atan2(Desired.Z, Desired.X);
			ConsiderTilt(FMath::UnwindRadians(Tilt));
		}
	}

	ConsiderTilt(-MaxTilt);
	ConsiderTilt(MaxTilt);
	ConsiderTilt(0.0);
	const double Extremum = FMath::Atan2(Desired.X, Desired.Z);
	ConsiderTilt(FMath::UnwindRadians(Extremum));
	ConsiderTilt(FMath::UnwindRadians(Extremum + UE_DOUBLE_PI));
	return Best;
}

FFF_AtlasLatLong FFF_AtlasGeo::OrientationToFrontLatLong(const FFF_AtlasGlobeOrientation& Orientation)
{
	return FFF_AtlasLatLong(
		FMath::Clamp(Orientation.Tilt, -90.f, 90.f),
		AtlasWrapDegrees(Orientation.Spin));
}

FQuat FFF_AtlasGeo::OrientationToQuat(const FFF_AtlasGlobeOrientation& Orientation)
{
	// ⚠️ 축에 대한 **오른손** 회전이다. `FRotator` 의 Pitch 는 +Y 에 대해 부호가 반대이므로
	//    이 식을 `FRotator(Tilt, Spin, 0).Quaternion()` 으로 바꾸면 기울기가 뒤집힌다.
	const FQuat SpinQuat(FVector::ZAxisVector, FMath::DegreesToRadians(static_cast<double>(Orientation.Spin)));
	const FQuat TiltQuat(FVector::YAxisVector, FMath::DegreesToRadians(static_cast<double>(Orientation.Tilt)));

	// UE 의 쿼터니언 곱은 **오른쪽을 먼저** 적용한다. 자전(Z) 이 먼저, 기울기(Y) 가 나중이다.
	return TiltQuat * SpinQuat;
}

FFF_AtlasGlobeOrientation FFF_AtlasGeo::NormalizeOrientation(
	const FFF_AtlasGlobeOrientation& Orientation,
	float MaxTiltDegrees)
{
	const float SafeMaxTilt = FMath::Clamp(FMath::Abs(MaxTiltDegrees), 0.f, 90.f);

	return FFF_AtlasGlobeOrientation(
		AtlasWrapDegrees(Orientation.Spin),
		FMath::Clamp(Orientation.Tilt, -SafeMaxTilt, SafeMaxTilt));
}

FFF_AtlasGlobeOrientation FFF_AtlasGeo::ApplyDrag(
	const FFF_AtlasGlobeOrientation& Orientation,
	const FVector2D& DragPixels,
	float DegreesPerPixel,
	float MaxTiltDegrees)
{
	// 오른쪽으로 끌면 정면의 지표가 화면 오른쪽(월드 -Y)으로 가야 한다. Rz 는 +X 를 +Y 로
	// 보내므로 자전각은 **줄어야** 한다 — 즉 정면 경도가 서쪽으로 간다.
	// 아래로 끌면 북쪽이 정면으로 온다 (UMG 의 +Y 는 아래쪽이다).
	const FFF_AtlasGlobeOrientation Dragged(
		Orientation.Spin - AtlasToFloat(DragPixels.X) * DegreesPerPixel,
		Orientation.Tilt + AtlasToFloat(DragPixels.Y) * DegreesPerPixel);

	return NormalizeOrientation(Dragged, MaxTiltDegrees);
}

FFF_AtlasGlobeOrientation FFF_AtlasGeo::InterpolateOrientation(
	const FFF_AtlasGlobeOrientation& From,
	const FFF_AtlasGlobeOrientation& To,
	float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.f, 1.f);

	// 자전각은 최단 경로로 돈다. 170 → -170 을 선형 보간하면 지구가 340도 되돌아간다.
	const float SpinDelta = FMath::FindDeltaAngleDegrees(From.Spin, To.Spin);

	// 기울기는 ±90 안에 클램프된 **감기지 않는 축**이다. 최단 각도차를 쓰면 동작은 같지만
	// "이 축도 감긴다" 는 잘못된 신호를 준다. 그냥 선형이다.
	return FFF_AtlasGlobeOrientation(
		AtlasWrapDegrees(From.Spin + SpinDelta * ClampedAlpha),
		FMath::Lerp(From.Tilt, To.Tilt, ClampedAlpha));
}

float FFF_AtlasGeo::HorizonMinDot(float Radius, float ViewerDistance)
{
	// 카메라가 지구 안이거나 값이 이상하면 전부 숨긴다. 뒤편 핀이 앞으로 나오는 쪽보다 안전하다.
	if (ViewerDistance <= Radius || Radius <= 0.f)
	{
		return 1.f;
	}

	return FMath::Clamp(Radius / ViewerDistance, 0.f, 1.f);
}

float FFF_AtlasGeo::ElevatedHorizonMinDot(float SphereRadius, float PointRadius, float ViewerDistance)
{
	if (SphereRadius <= 0.f || PointRadius < SphereRadius || ViewerDistance <= SphereRadius)
	{
		return 1.f;
	}

	const double Radius = static_cast<double>(SphereRadius);
	const double Point = static_cast<double>(PointRadius);
	const double Distance = static_cast<double>(ViewerDistance);
	const double RadiusSquared = Radius * Radius;

	// 관측자-꼭대기 선분이 안쪽 구에 접하는 경계의 `-` 해다.
	// `+` 해는 바깥 반지름 구의 가까운 쪽 교점이라 뒤편 조형물을 너무 늦게 켠다.
	const double RootTerm = FMath::Max(
		0.0,
		(Distance * Distance - RadiusSquared) * (Point * Point - RadiusSquared));
	const double MinDot = (RadiusSquared - FMath::Sqrt(RootTerm)) / (Distance * Point);

	return static_cast<float>(FMath::Clamp(MinDot, -1.0, 1.0));
}

bool FFF_AtlasGeo::IsFacingViewer(const FVector& Normal, const FVector& ViewerDirection, float MinDot)
{
	const FVector SafeNormal = Normal.GetSafeNormal();
	const FVector SafeViewer = ViewerDirection.GetSafeNormal();

	if (SafeNormal.IsNearlyZero() || SafeViewer.IsNearlyZero())
	{
		// 방향을 모르면 숨긴다. 보이게 두면 지구 뒤편의 핀이 앞으로 튀어나온 것처럼 보인다.
		return false;
	}

	return AtlasToFloat(FVector::DotProduct(SafeNormal, SafeViewer)) >= MinDot;
}

bool FFF_AtlasGeo::IsPinFacingFront(
	const FFF_AtlasLatLong& Pin,
	const FFF_AtlasGlobeOrientation& Orientation,
	float MinDot)
{
	const FVector RotatedNormal = OrientationToQuat(Orientation).RotateVector(LatLongToUnitVector(Pin));

	// 앞면은 회전 루트의 +X 쪽이다. 단위벡터를 +X 와 내적하는 것은 X 성분을 읽는 것과 같으므로
	// 정규화를 다시 하지 않는다 — 핀마다 매 프레임 제곱근을 두 번 아낀다.
	return AtlasToFloat(RotatedNormal.X) >= MinDot;
}

FQuat FFF_AtlasGeo::MakeSurfaceUpQuat(const FVector& SurfaceNormal, const FVector& ReferenceNorth)
{
	const FVector SafeNormal = SurfaceNormal.GetSafeNormal();
	if (SafeNormal.IsNearlyZero())
	{
		return FQuat::Identity;
	}

	// MakeFromZX 는 +Z 를 법선에 맞추고 남은 축을 `ReferenceNorth` 로 정한다.
	// 법선과 기준이 평행한 극점에서는 엔진이 임의 축으로 대체하므로 여기서 따로 막지 않는다.
	return FRotationMatrix::MakeFromZX(SafeNormal, ReferenceNorth).ToQuat();
}

int32 FFF_AtlasGeo::FindNearestPinIndex(const TArray<FFF_AtlasLatLong>& Pins, const FFF_AtlasLatLong& Target)
{
	if (Pins.Num() == 0)
	{
		return INDEX_NONE;
	}

	const FVector TargetNormal = LatLongToUnitVector(Target);

	int32 BestIndex = INDEX_NONE;
	double BestDot = -TNumericLimits<double>::Max();

	for (int32 Index = 0; Index < Pins.Num(); ++Index)
	{
		// 내적이 클수록 가깝다. 각도로 바꾸지 않는 이유는 Acos 가 필요 없고, 경도 경계·극점에서
		// 각도 뺄셈이 만드는 함정을 아예 지나가기 때문이다.
		const double Dot = FVector::DotProduct(TargetNormal, LatLongToUnitVector(Pins[Index]));
		if (Dot > BestDot)
		{
			BestDot = Dot;
			BestIndex = Index;
		}
	}

	return BestIndex;
}

int32 FFF_AtlasGeo::FindAdjacentPinIndex(
	const TArray<FFF_AtlasLatLong>& Pins,
	float FrontLongitude,
	int32 ScreenDirection,
	int32 CurrentIndex)
{
	if (Pins.Num() == 0)
	{
		return INDEX_NONE;
	}

	// 화면 오른쪽(+1) = 동쪽 = 경도가 커지는 쪽. 좌표 규약이 그렇게 되어 있어 부호를 뒤집지 않는다.
	const float LongitudeSign = (ScreenDirection >= 0) ? 1.f : -1.f;

	const int32 PinCount = Pins.Num();
	const bool bHasCurrent = Pins.IsValidIndex(CurrentIndex);

	// 회전량이 완전히 같은 핀들끼리 순서를 정하는 값. `CurrentIndex` 바로 다음이 0 이다.
	const int32 RotateFrom = bHasCurrent ? CurrentIndex : PinCount - 1;

	int32 BestIndex = INDEX_NONE;
	float BestTravel = TNumericLimits<float>::Max();
	int32 BestOrder = TNumericLimits<int32>::Max();

	for (int32 Index = 0; Index < PinCount; ++Index)
	{
		// 정면에서 그 핀까지 진행 방향으로 얼마나 돌아야 하나.
		const float Delta = AtlasWrapDegrees(Pins[Index].Longitude - FrontLongitude) * LongitudeSign;
		const bool bSameMeridian = FMath::Abs(Delta) <= UE_KINDA_SMALL_NUMBER;

		// 🔴 겹친 핀을 다루는 규칙. 좌표를 아직 채우지 않은 국가가 둘이면 (0,0) 이 겹치는데,
		//    회전량만으로 고르면 그 둘이 서로를 계속 주고받아 **나머지 핀에 영영 도달하지 못한다.**
		//    겹친 핀은 **인덱스 순서로 한 줄로 세워** 지나가게 하고, 그 줄의 끝에서 경도로 넘어간다.
		float Travel;
		if (bHasCurrent && Index == CurrentIndex)
		{
			// 자기 자신은 한 바퀴 끝. 이래야 순회가 닫힌다.
			Travel = 360.f;
		}
		else if (bSameMeridian)
		{
			// 같은 자오선의 다른 핀. 인덱스가 뒤면 바로 다음 차례, 앞이면 이미 지난 것으로 본다.
			// 기준 핀을 모를 때는(CurrentIndex 없음) 정면의 핀을 다시 고르지 않는 쪽이 맞다.
			Travel = (bHasCurrent && Index > CurrentIndex) ? 0.f : 360.f;
		}
		else if (Delta > 0.f)
		{
			Travel = Delta;
		}
		else
		{
			Travel = Delta + 360.f;
		}

		// 회전량 → 순서의 사전식 비교.
		const int32 Order = ((Index - RotateFrom - 1) + PinCount * 2) % PinCount;

		if (Travel < BestTravel || (Travel == BestTravel && Order < BestOrder))
		{
			BestTravel = Travel;
			BestOrder = Order;
			BestIndex = Index;
		}
	}

	return BestIndex;
}

float FFF_AtlasGeo::StepReferenceLongitude(
	float FrontLongitude,
	float PendingTargetLongitude,
	bool bTransitioning)
{
	// 전환 중이면 목표가 기준이다. 보간 중인 각도를 기준으로 하면 같은 핀을 다시 고른다.
	return bTransitioning ? AtlasWrapDegrees(PendingTargetLongitude) : AtlasWrapDegrees(FrontLongitude);
}
