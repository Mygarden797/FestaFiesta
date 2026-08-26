// Fill out your copyright notice in the Description page of Project Settings.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/Atlas/FF_AtlasGeo.h"

/**
 * 지구본 좌표 규약 고정 테스트 (U3-a).
 *
 * === 왜 이 테스트가 있어야 하나 ===
 * 구면 좌표는 **눈으로 보면 그럴싸한데 특정 좌표에서만 틀린다.** 실제로 이 파일을 만드는 동안
 * 나온 것만 세 가지다 —
 *   ① 동쪽 부호를 지리 감각대로 두면 왼손 좌표계 때문에 **지구가 좌우로 뒤집혀** 렌더된다.
 *      수식은 전부 맞고 화면만 틀리므로 리뷰에서 빠져나간다.
 *   ② 위도를 `Asin` 으로 구하면 위도 89.99 에서 허용오차를 넘는다.
 *   ③ 경도 ±180 은 두 값이 모두 살아남아 뺄셈 비교가 340도 오차를 만든다.
 * 셋 다 화면에서 발견하려면 그 좌표에 나라가 있어야 한다.
 *
 * 월드도 액터도 세우지 않는다. `FFF_AtlasGeo` 가 상태 없는 계산만 갖는 이유가 이것이다.
 *
 * ⚠️ LWC — `FVector` 성분은 double 이다. 기대값·허용오차에 `f` 를 붙이면 float·double
 *    오버로드가 모두 후보가 되어 C2666 으로 컴파일이 막힌다. double 성분을 볼 때는 `1.0` 처럼
 *    쓰고, `FFF_AtlasLatLong` 의 float 멤버를 볼 때는 `f` 를 붙인다.
 */
namespace FFF_AtlasGeoTestHelpers
{
	/** 왕복 변환 허용오차(도). double 로 계산하므로 넉넉하다. */
	constexpr float AtlasGeoAngleTolerance = 0.01f;

	/** 벡터 성분 허용오차 (double). */
	constexpr double AtlasGeoVectorTolerance = 1.e-6;

	/**
	 * 왕복 검사용 표본. **경계를 일부러 포함한다** — 적도·본초자오선·양 반구·경도 ±180 직전·
	 * 극점 바로 아래(`Asin` 이 무너지는 구간)·다대포 실제 값.
	 *
	 * 정확한 극점(±90)은 넣지 않는다 — 경도가 정의되지 않으므로 별 테스트에서 따로 본다.
	 */
	inline TArray<FFF_AtlasLatLong> MakeAtlasGeoSamples()
	{
		return {
			FFF_AtlasLatLong(0.f, 0.f),			// 원점
			FFF_AtlasLatLong(0.f, 90.f),		// 동쪽 90
			FFF_AtlasLatLong(0.f, -90.f),		// 서쪽 90
			FFF_AtlasLatLong(0.f, 179.9f),		// 경도 경계 바로 앞
			FFF_AtlasLatLong(0.f, -179.9f),		// 반대쪽 경계
			FFF_AtlasLatLong(45.f, 45.f),
			FFF_AtlasLatLong(-45.f, 135.f),
			FFF_AtlasLatLong(89.99f, -120.f),	// Asin 이 허용오차를 넘기던 구간
			FFF_AtlasLatLong(-89.99f, 30.f),
			FFF_AtlasLatLong(35.04f, 128.97f)	// 다대포 — 지금 카탈로그에 들어 있는 실제 값
		};
	}

	inline FString DescribeAtlasGeoSample(const FFF_AtlasLatLong& LatLong)
	{
		return FString::Printf(TEXT("(위도 %.4f, 경도 %.4f)"), LatLong.Latitude, LatLong.Longitude);
	}
}

// 위경도 → 벡터 → 위경도, 그리고 벡터 → 위경도 → 벡터 왕복이 오차 안에서 제자리로 온다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasGeo_RoundTripsLatLong, "FestaFiesta.UI.AtlasGeo.RoundTripsLatLong", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_AtlasGeo_RoundTripsLatLong::RunTest(const FString& Parameters)
{
	using namespace FFF_AtlasGeoTestHelpers;

	for (const FFF_AtlasLatLong& Sample : MakeAtlasGeoSamples())
	{
		const FString What = DescribeAtlasGeoSample(Sample);
		const FVector Unit = FFF_AtlasGeo::LatLongToUnitVector(Sample);

		TestEqual(*FString::Printf(TEXT("%s 의 단위벡터 길이가 1 이다"), *What),
			Unit.Length(), 1.0, AtlasGeoVectorTolerance);

		const FFF_AtlasLatLong Back = FFF_AtlasGeo::UnitVectorToLatLong(Unit);

		TestEqual(*FString::Printf(TEXT("%s 위도가 돌아온다"), *What),
			Back.Latitude, Sample.Latitude, AtlasGeoAngleTolerance);

		// 경도는 감기므로 뺄셈이 아니라 최단 각도차로 본다. 179.9 와 -180.1 은 같은 자오선이다.
		TestEqual(*FString::Printf(TEXT("%s 경도가 돌아온다"), *What),
			FMath::FindDeltaAngleDegrees(Back.Longitude, Sample.Longitude), 0.f, AtlasGeoAngleTolerance);

		// 벡터 왕복이 **진짜 불변식**이다. 극점에서도 성립한다 — 위경도 왕복은 그렇지 않다.
		TestTrue(*FString::Printf(TEXT("%s 벡터 왕복이 같은 방향이다"), *What),
			FFF_AtlasGeo::LatLongToUnitVector(Back).Equals(Unit, AtlasGeoVectorTolerance));
	}

	return true;
}

// 규약이 실제로 그 축을 가리킨다. 이게 틀리면 지구가 뒤집히거나 텍스처와 핀이 90도 어긋난다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasGeo_AxisConvention, "FestaFiesta.UI.AtlasGeo.AxisConvention", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_AtlasGeo_AxisConvention::RunTest(const FString& Parameters)
{
	using namespace FFF_AtlasGeoTestHelpers;

	const FVector Origin = FFF_AtlasGeo::LatLongToUnitVector(FFF_AtlasLatLong(0.f, 0.f));
	TestTrue(TEXT("경도 0 · 위도 0 은 로컬 +X 다 (정면)"),
		Origin.Equals(FVector::ForwardVector, AtlasGeoVectorTolerance));

	// 🔴 이 한 줄이 "지구가 뒤집히지 않는다" 를 지킨다.
	// UE 는 왼손 좌표계라 +X 에서 -X 를 보는 카메라의 오른쪽이 월드 -Y 다. 동쪽을 +Y 로 두면
	// 유럽이 아메리카 쪽에 가고 일본이 한국 서쪽에 붙는다.
	const FVector East = FFF_AtlasGeo::LatLongToUnitVector(FFF_AtlasLatLong(0.f, 90.f));
	TestTrue(TEXT("경도 +90(동쪽)은 로컬 -Y 다 — 화면 오른쪽"),
		East.Equals(-FVector::RightVector, AtlasGeoVectorTolerance));

	const FVector West = FFF_AtlasGeo::LatLongToUnitVector(FFF_AtlasLatLong(0.f, -90.f));
	TestTrue(TEXT("경도 -90(서쪽)은 로컬 +Y 다 — 화면 왼쪽"),
		West.Equals(FVector::RightVector, AtlasGeoVectorTolerance));

	const FVector NorthPole = FFF_AtlasGeo::LatLongToUnitVector(FFF_AtlasLatLong(90.f, 0.f));
	TestTrue(TEXT("위도 +90 은 로컬 +Z 다 (북극)"),
		NorthPole.Equals(FVector::UpVector, AtlasGeoVectorTolerance));

	const FVector SouthPole = FFF_AtlasGeo::LatLongToUnitVector(FFF_AtlasLatLong(-90.f, 0.f));
	TestTrue(TEXT("위도 -90 은 로컬 -Z 다 (남극)"),
		SouthPole.Equals(-FVector::UpVector, AtlasGeoVectorTolerance));

	// 표면 위치는 방향이 같고 길이만 반지름이 된다.
	const FVector Surface = FFF_AtlasGeo::LatLongToSurfacePosition(FFF_AtlasLatLong(0.f, 90.f), 500.f);
	TestTrue(TEXT("표면 위치는 반지름만 곱한 것이다"),
		Surface.Equals(-FVector::RightVector * 500.0, 0.001));

	return true;
}

// 극점과 이상한 입력. 경도는 극점에서 정의되지 않는다 — 값이 튀지 않게 0 으로 고정한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasGeo_HandlesPolesAndDegenerateInput, "FestaFiesta.UI.AtlasGeo.HandlesPolesAndDegenerateInput", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_AtlasGeo_HandlesPolesAndDegenerateInput::RunTest(const FString& Parameters)
{
	using namespace FFF_AtlasGeoTestHelpers;

	// 극점은 경도를 아무 값으로 줘도 같은 점이다. 위도는 정확해야 하고 경도는 0 이어야 한다.
	for (const float Longitude : { 0.f, 90.f, -170.f, 180.f })
	{
		const FFF_AtlasLatLong North = FFF_AtlasGeo::UnitVectorToLatLong(
			FFF_AtlasGeo::LatLongToUnitVector(FFF_AtlasLatLong(90.f, Longitude)));
		TestEqual(TEXT("북극의 위도는 +90 이다"), North.Latitude, 90.f, AtlasGeoAngleTolerance);
		TestEqual(TEXT("북극의 경도는 0 으로 고정된다"), North.Longitude, 0.f, AtlasGeoAngleTolerance);

		const FFF_AtlasLatLong South = FFF_AtlasGeo::UnitVectorToLatLong(
			FFF_AtlasGeo::LatLongToUnitVector(FFF_AtlasLatLong(-90.f, Longitude)));
		TestEqual(TEXT("남극의 위도는 -90 이다"), South.Latitude, -90.f, AtlasGeoAngleTolerance);
		TestEqual(TEXT("남극의 경도도 0 으로 고정된다"), South.Longitude, 0.f, AtlasGeoAngleTolerance);
	}

	// 영벡터는 크래시하지 않고 (0, 0) 이다.
	const FFF_AtlasLatLong Zero = FFF_AtlasGeo::UnitVectorToLatLong(FVector::ZeroVector);
	TestEqual(TEXT("영벡터의 위도는 0"), Zero.Latitude, 0.f, AtlasGeoAngleTolerance);
	TestEqual(TEXT("영벡터의 경도는 0"), Zero.Longitude, 0.f, AtlasGeoAngleTolerance);

	// 범위를 벗어난 데이터가 들어와도 계산이 깨지지 않는다.
	const FFF_AtlasLatLong Wrapped = FFF_AtlasGeo::NormalizeLatLong(FFF_AtlasLatLong(120.f, 200.f));
	TestEqual(TEXT("위도 120 은 90 으로 잘린다"), Wrapped.Latitude, 90.f, AtlasGeoAngleTolerance);
	TestEqual(TEXT("경도 200 은 -160 으로 감긴다"), Wrapped.Longitude, -160.f, AtlasGeoAngleTolerance);

	// 정규화되지 않은 입력도 방향만 본다.
	const FFF_AtlasLatLong Scaled = FFF_AtlasGeo::UnitVectorToLatLong(-FVector::RightVector * 1234.0);
	TestEqual(TEXT("길이가 1 이 아니어도 경도 +90 이다"), Scaled.Longitude, 90.f, AtlasGeoAngleTolerance);

	return true;
}

// 어떤 위경도든 정면(로컬 +X)으로 온다. 그리고 자전축은 절대 화면에서 기울지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasGeo_OrientationBringsPinToFront, "FestaFiesta.UI.AtlasGeo.OrientationBringsPinToFront", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_AtlasGeo_OrientationBringsPinToFront::RunTest(const FString& Parameters)
{
	using namespace FFF_AtlasGeoTestHelpers;

	for (const FFF_AtlasLatLong& Sample : MakeAtlasGeoSamples())
	{
		const FString What = DescribeAtlasGeoSample(Sample);

		const FFF_AtlasGlobeOrientation Orientation = FFF_AtlasGeo::MakeOrientationFacing(Sample);
		const FQuat Quat = FFF_AtlasGeo::OrientationToQuat(Orientation);
		const FVector Rotated = Quat.RotateVector(FFF_AtlasGeo::LatLongToUnitVector(Sample));

		TestTrue(*FString::Printf(TEXT("%s 가 정면 +X 로 온다 — 실제 %s"), *What, *Rotated.ToCompactString()),
			Rotated.Equals(FVector::ForwardVector, 1.e-4));

		// 🔴 자전축이 화면에서 수직으로 선다. 어떤 Spin 에서도 Y 성분이 0 이어야 한다 —
		//    이것이 각도 둘로 상태를 드는 이유이고, 쿼터니언 누적에는 없는 성질이다.
		const FVector RotatedPole = Quat.RotateVector(FVector::UpVector);
		TestEqual(*FString::Printf(TEXT("%s 에서도 자전축이 기울지 않는다"), *What),
			RotatedPole.Y, 0.0, 1.e-6);

		// 회전 상태 → 정면 위경도 역변환이 같은 지점을 가리킨다.
		const FFF_AtlasLatLong Front = FFF_AtlasGeo::OrientationToFrontLatLong(Orientation);
		TestEqual(*FString::Printf(TEXT("%s 정면 위도가 일치한다"), *What),
			Front.Latitude, FFF_AtlasGeo::NormalizeLatLong(Sample).Latitude, AtlasGeoAngleTolerance);
		TestEqual(*FString::Printf(TEXT("%s 정면 경도가 일치한다"), *What),
			FMath::FindDeltaAngleDegrees(Front.Longitude, Sample.Longitude), 0.f, AtlasGeoAngleTolerance);
	}

	// 정면 위경도가 곧 (Tilt, Spin) 이라는 규약을 못으로 박는다.
	const FFF_AtlasGlobeOrientation Dadaepo = FFF_AtlasGeo::MakeOrientationFacing(FFF_AtlasLatLong(35.04f, 128.97f));
	TestEqual(TEXT("Spin 은 경도와 같다"), Dadaepo.Spin, 128.97f, AtlasGeoAngleTolerance);
	TestEqual(TEXT("Tilt 는 위도와 같다"), Dadaepo.Tilt, 35.04f, AtlasGeoAngleTolerance);

	return true;
}

// 드래그 방향이 손으로 잡아 끄는 감각과 같다. 부호가 반대면 조작이 즉시 이상해진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasGeo_DragFollowsGrabDirection, "FestaFiesta.UI.AtlasGeo.DragFollowsGrabDirection", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_AtlasGeo_DragFollowsGrabDirection::RunTest(const FString& Parameters)
{
	using namespace FFF_AtlasGeoTestHelpers;

	constexpr float MaxTilt = 80.f;
	constexpr float DegreesPerPixel = 0.25f;

	const FFF_AtlasGlobeOrientation Start(0.f, 0.f);

	// 오른쪽으로 끈다 → 정면에 있던 지표가 화면 오른쪽(월드 -Y)으로 간다.
	const FFF_AtlasGlobeOrientation DraggedRight =
		FFF_AtlasGeo::ApplyDrag(Start, FVector2D(100.0, 0.0), DegreesPerPixel, MaxTilt);
	const FVector MovedRight =
		FFF_AtlasGeo::OrientationToQuat(DraggedRight).RotateVector(FVector::ForwardVector);
	TestTrue(TEXT("오른쪽으로 끌면 지표가 -Y(화면 오른쪽)로 간다"), MovedRight.Y < -UE_KINDA_SMALL_NUMBER);
	TestTrue(TEXT("오른쪽 드래그는 위아래로 움직이지 않는다"), FMath::IsNearlyZero(MovedRight.Z, 1.e-6));

	// 그리고 그때 정면에 오는 것은 서쪽이다. 지구를 오른쪽으로 밀면 서쪽이 나타난다.
	TestEqual(TEXT("오른쪽 드래그 25도면 정면 경도가 -25 다 (서쪽)"),
		FFF_AtlasGeo::OrientationToFrontLatLong(DraggedRight).Longitude, -25.f, AtlasGeoAngleTolerance);

	// 아래로 끈다 → 북쪽이 정면으로 온다.
	const FFF_AtlasGlobeOrientation DraggedDown =
		FFF_AtlasGeo::ApplyDrag(Start, FVector2D(0.0, 100.0), DegreesPerPixel, MaxTilt);
	TestEqual(TEXT("아래로 끌면 정면 위도가 북쪽으로 간다"),
		FFF_AtlasGeo::OrientationToFrontLatLong(DraggedDown).Latitude, 25.f, AtlasGeoAngleTolerance);

	// 기울기는 클램프된다. 극점을 넘겨 지구가 뒤집히면 되돌릴 방법이 없다.
	const FFF_AtlasGlobeOrientation Overdragged =
		FFF_AtlasGeo::ApplyDrag(Start, FVector2D(0.0, 10000.0), DegreesPerPixel, MaxTilt);
	TestEqual(TEXT("아무리 끌어도 기울기가 최대치를 넘지 않는다"), Overdragged.Tilt, MaxTilt, AtlasGeoAngleTolerance);

	const FFF_AtlasGlobeOrientation Underdragged =
		FFF_AtlasGeo::ApplyDrag(Start, FVector2D(0.0, -10000.0), DegreesPerPixel, MaxTilt);
	TestEqual(TEXT("반대쪽도 대칭으로 잘린다"), Underdragged.Tilt, -MaxTilt, AtlasGeoAngleTolerance);

	// 자전각은 감긴다 — 계속 끌어도 각도가 무한히 커지지 않는다.
	const FFF_AtlasGlobeOrientation Spun =
		FFF_AtlasGeo::ApplyDrag(FFF_AtlasGlobeOrientation(170.f, 0.f), FVector2D(-100.0, 0.0), 0.5f, MaxTilt);
	TestTrue(TEXT("자전각이 -180~180 안으로 감긴다"), Spun.Spin >= -180.f && Spun.Spin <= 180.f);
	TestEqual(TEXT("170 에서 왼쪽으로 50도 끌면 -140 이다"), Spun.Spin, -140.f, AtlasGeoAngleTolerance);

	// 🔴 감도와 기울기 한계가 **실제로 반영되는지**. 한 값만 쓰면 함수 안에 상수를 박아도
	//    테스트가 통과한다 — Project Settings 에서 바꿔도 런타임에 안 먹는 회귀를 놓친다.
	const FFF_AtlasGlobeOrientation Sensitive =
		FFF_AtlasGeo::ApplyDrag(Start, FVector2D(100.0, 0.0), DegreesPerPixel * 2.f, MaxTilt);
	TestEqual(TEXT("감도를 2배로 하면 회전량도 2배다"),
		FFF_AtlasGeo::OrientationToFrontLatLong(Sensitive).Longitude, -50.f, AtlasGeoAngleTolerance);

	const FFF_AtlasGlobeOrientation TighterClamp =
		FFF_AtlasGeo::ApplyDrag(Start, FVector2D(0.0, 10000.0), DegreesPerPixel, 45.f);
	TestEqual(TEXT("기울기 한계를 45 로 주면 45 에서 잘린다"),
		TighterClamp.Tilt, 45.f, AtlasGeoAngleTolerance);

	return true;
}

// 경도 경계를 지나갈 때 짧은 쪽으로 돈다. 이게 없으면 지구가 340도 되돌아간다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasGeo_InterpolationTakesShortPath, "FestaFiesta.UI.AtlasGeo.InterpolationTakesShortPath", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_AtlasGeo_InterpolationTakesShortPath::RunTest(const FString& Parameters)
{
	using namespace FFF_AtlasGeoTestHelpers;

	const FFF_AtlasGlobeOrientation From(170.f, 0.f);
	const FFF_AtlasGlobeOrientation To(-170.f, 0.f);

	// 최단 경로는 +20도다. 절반이면 경계(±180)를 딱 넘은 자리다.
	const FFF_AtlasGlobeOrientation Half = FFF_AtlasGeo::InterpolateOrientation(From, To, 0.5f);
	TestEqual(TEXT("절반 지점이 경계(±180)다"),
		FMath::Abs(FMath::FindDeltaAngleDegrees(Half.Spin, 180.f)), 0.f, AtlasGeoAngleTolerance);

	// 전체 이동량이 20도여야 한다. 340도로 갔다면 이 값이 크게 벗어난다.
	const float Travel = FMath::Abs(FMath::FindDeltaAngleDegrees(From.Spin, Half.Spin)) * 2.f;
	TestEqual(TEXT("총 이동량이 20도다 — 340도로 되돌지 않는다"), Travel, 20.f, 0.1f);

	// 양 끝은 정확히 시작·끝이다.
	TestEqual(TEXT("Alpha 0 은 시작점"),
		FFF_AtlasGeo::InterpolateOrientation(From, To, 0.f).Spin, From.Spin, AtlasGeoAngleTolerance);
	TestEqual(TEXT("Alpha 1 은 끝점"),
		FMath::FindDeltaAngleDegrees(FFF_AtlasGeo::InterpolateOrientation(From, To, 1.f).Spin, To.Spin),
		0.f, AtlasGeoAngleTolerance);

	// 범위를 벗어난 Alpha 는 잘린다 — 연출이 목표를 지나쳐 흔들리지 않게 한다.
	TestEqual(TEXT("Alpha 가 1 을 넘어도 끝점에서 멈춘다"),
		FMath::FindDeltaAngleDegrees(FFF_AtlasGeo::InterpolateOrientation(From, To, 5.f).Spin, To.Spin),
		0.f, AtlasGeoAngleTolerance);

	// 기울기는 감기지 않는 축이라 그냥 선형이다. -40 → 40 의 1/4 은 -20 이다
	// (최단 각도차로 착각해 320도를 돌면 여기가 어긋난다).
	const FFF_AtlasGlobeOrientation Tilted = FFF_AtlasGeo::InterpolateOrientation(
		FFF_AtlasGlobeOrientation(0.f, -40.f), FFF_AtlasGlobeOrientation(0.f, 40.f), 0.25f);
	TestEqual(TEXT("기울기 보간이 선형이다"), Tilted.Tilt, -20.f, AtlasGeoAngleTolerance);

	return true;
}

// 지구 뒤편으로 넘어간 핀을 숨기는 판정과, 원근 카메라의 실루엣 기준.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasGeo_FarSidePinsAreHidden, "FestaFiesta.UI.AtlasGeo.FarSidePinsAreHidden", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_AtlasGeo_FarSidePinsAreHidden::RunTest(const FString& Parameters)
{
	using namespace FFF_AtlasGeoTestHelpers;

	const FFF_AtlasGlobeOrientation NoRotation(0.f, 0.f);

	TestTrue(TEXT("정면의 핀은 보인다"),
		FFF_AtlasGeo::IsPinFacingFront(FFF_AtlasLatLong(0.f, 0.f), NoRotation, 0.f));
	TestFalse(TEXT("지구 반대편(경도 180)의 핀은 숨는다"),
		FFF_AtlasGeo::IsPinFacingFront(FFF_AtlasLatLong(0.f, 180.f), NoRotation, 0.f));

	// 실루엣에 딱 붙은 핀. 여유가 없으면 한 픽셀 단위로 깜빡인다.
	const FFF_AtlasLatLong Silhouette(0.f, 89.f);
	TestTrue(TEXT("기준 0 이면 실루엣 핀이 보인다"),
		FFF_AtlasGeo::IsPinFacingFront(Silhouette, NoRotation, 0.f));
	TestFalse(TEXT("기준에 여유를 주면 실루엣 핀은 미리 숨는다"),
		FFF_AtlasGeo::IsPinFacingFront(Silhouette, NoRotation, 0.2f));

	// 돌리면 보이는 핀이 바뀐다.
	const FFF_AtlasGlobeOrientation TurnedAround =
		FFF_AtlasGeo::MakeOrientationFacing(FFF_AtlasLatLong(0.f, 180.f));
	TestTrue(TEXT("반대편을 정면으로 돌리면 그 핀이 보인다"),
		FFF_AtlasGeo::IsPinFacingFront(FFF_AtlasLatLong(0.f, 180.f), TurnedAround, 0.f));
	TestFalse(TEXT("그때 원래 정면이던 핀은 숨는다"),
		FFF_AtlasGeo::IsPinFacingFront(FFF_AtlasLatLong(0.f, 0.f), TurnedAround, 0.f));

	// 🔴 원근 카메라에서 보이는 것은 반구가 아니다. 거리 2R 이면 실루엣 기준이 0.5 다 —
	//    기준을 0 으로 두면 지구 테두리 바깥에서 핀이 튀어나온다.
	TestEqual(TEXT("거리가 반지름의 2배면 실루엣 기준이 0.5"),
		FFF_AtlasGeo::HorizonMinDot(5000.f, 10000.f), 0.5f, 1.e-4f);
	TestEqual(TEXT("거리가 반지름의 10배면 0.1 — 멀어지면 반구에 가까워진다"),
		FFF_AtlasGeo::HorizonMinDot(5000.f, 50000.f), 0.1f, 1.e-4f);
	TestEqual(TEXT("카메라가 지구 안이면 전부 숨긴다"),
		FFF_AtlasGeo::HorizonMinDot(5000.f, 1000.f), 1.f, 1.e-4f);
	TestEqual(TEXT("반지름이 0 이면 전부 숨긴다"),
		FFF_AtlasGeo::HorizonMinDot(0.f, 10000.f), 1.f, 1.e-4f);

	// 🔴 IsFacingViewer 의 **정상 경로**. 가드 단정만 두면 `>= MinDot` 을 `<=` 로 뒤집어도
	//    테스트가 전부 통과한다 — U3-b 가 이 함수로 핀 버튼을 걸러 내는 순간 앞뒤가 반대로 나온다.
	TestTrue(TEXT("법선이 시선과 같으면 보인다"),
		FFF_AtlasGeo::IsFacingViewer(FVector::ForwardVector, FVector::ForwardVector, 0.f));
	TestFalse(TEXT("법선이 시선의 반대면 숨는다"),
		FFF_AtlasGeo::IsFacingViewer(-FVector::ForwardVector, FVector::ForwardVector, 0.f));
	TestFalse(TEXT("직각이면 기준 0 초과에서 숨는다"),
		FFF_AtlasGeo::IsFacingViewer(FVector::RightVector, FVector::ForwardVector, 0.01f));

	// 기준과 정확히 같은 내적은 보인다 (`>=` 이다). 60도 법선의 내적이 0.5 다.
	const FVector Sixty(0.5, 0.0, FMath::Sqrt(3.0) * 0.5);
	TestTrue(TEXT("내적이 기준과 같으면 보인다 (경계 포함)"),
		FFF_AtlasGeo::IsFacingViewer(Sixty, FVector::ForwardVector, 0.5f - 1.e-4f));
	TestFalse(TEXT("기준을 조금만 올리면 숨는다"),
		FFF_AtlasGeo::IsFacingViewer(Sixty, FVector::ForwardVector, 0.5f + 1.e-3f));

	// 방향을 모르면 숨긴다 — 뒤편 핀이 앞으로 튀어나오는 쪽보다 안전하다.
	TestFalse(TEXT("영벡터 법선은 숨는다"),
		FFF_AtlasGeo::IsFacingViewer(FVector::ZeroVector, FVector::ForwardVector, 0.f));
	TestFalse(TEXT("영벡터 시선은 숨는다"),
		FFF_AtlasGeo::IsFacingViewer(FVector::ForwardVector, FVector::ZeroVector, 0.f));

	return true;
}

// 지표보다 솟은 조형물의 꼭대기는 밑면보다 먼저 지평선을 넘어온다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasGeo_ElevatedLandmarkAppearsBeforeSurface,
	"FestaFiesta.UI.AtlasGeo.ElevatedLandmarkAppearsBeforeSurface",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_AtlasGeo_ElevatedLandmarkAppearsBeforeSurface::RunTest(const FString& Parameters)
{
	constexpr float Tolerance = 1.e-4f;

	TestEqual(TEXT("높이가 0이면 표면 지평선 식과 같다"),
		FFF_AtlasGeo::ElevatedHorizonMinDot(3.f, 3.f, 5.f), 0.6f, Tolerance);
	TestEqual(TEXT("높이 0.75면 꼭대기가 정확히 90도에서 보이기 시작한다"),
		FFF_AtlasGeo::ElevatedHorizonMinDot(3.f, 3.75f, 5.f), 0.f, Tolerance);
	TestEqual(TEXT("더 높은 점은 90도 뒤편에서도 보인다"),
		FFF_AtlasGeo::ElevatedHorizonMinDot(3.f, 5.f, 5.f), -0.28f, Tolerance);
	TestEqual(TEXT("카메라가 꼭대기 반경 안이어도 구 밖이면 유한한 경계를 낸다"),
		FFF_AtlasGeo::ElevatedHorizonMinDot(3.f, 5.f, 4.f), -0.07915f, Tolerance);

	const float SurfaceThreshold = FFF_AtlasGeo::ElevatedHorizonMinDot(3.f, 3.f, 5.f);
	const float TallThreshold = FFF_AtlasGeo::ElevatedHorizonMinDot(3.f, 5.f, 5.f);
	TestTrue(TEXT("높이가 커질수록 임계가 낮아져 더 일찍 보인다"), TallThreshold < SurfaceThreshold);

	TestEqual(TEXT("점이 구 안이면 안전하게 숨긴다"),
		FFF_AtlasGeo::ElevatedHorizonMinDot(3.f, 2.f, 5.f), 1.f, Tolerance);
	TestEqual(TEXT("관측자가 구 안이면 안전하게 숨긴다"),
		FFF_AtlasGeo::ElevatedHorizonMinDot(3.f, 5.f, 2.f), 1.f, Tolerance);

	return true;
}

// 조형물이 지표에 서고, 방향이 제멋대로 돌아가지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasGeo_LandmarkStandsOnSurface, "FestaFiesta.UI.AtlasGeo.LandmarkStandsOnSurface", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_AtlasGeo_LandmarkStandsOnSurface::RunTest(const FString& Parameters)
{
	using namespace FFF_AtlasGeoTestHelpers;

	for (const FFF_AtlasLatLong& Sample : MakeAtlasGeoSamples())
	{
		const FString What = DescribeAtlasGeoSample(Sample);
		const FVector Normal = FFF_AtlasGeo::LatLongToUnitVector(Sample);
		const FQuat Quat = FFF_AtlasGeo::MakeSurfaceUpQuat(Normal);

		TestTrue(*FString::Printf(TEXT("%s 에서 조형물의 위쪽이 지표 법선이다"), *What),
			Quat.GetUpVector().Equals(Normal, 1.e-4));

		// 남은 축이 임의로 채워지면 조형물마다 제멋대로 돌아간다. 기준(북극)을 줬으므로
		// 앞면의 북쪽 성분이 항상 양수여야 한다 — 극점 바로 아래 표본도 포함이다.
		TestTrue(*FString::Printf(TEXT("%s 에서 조형물 앞면이 북쪽을 향한다"), *What),
			Quat.GetForwardVector().Z > 0.0);
	}

	// 영벡터는 항등 회전이다. 크래시하거나 NaN 을 내지 않는다.
	TestTrue(TEXT("영벡터 법선은 항등 회전"),
		FFF_AtlasGeo::MakeSurfaceUpQuat(FVector::ZeroVector).Equals(FQuat::Identity));

	// 극점은 법선과 기준이 평행해 남은 축을 정할 수 없다. 그래도 위쪽만은 맞아야 한다.
	const FQuat AtPole = FFF_AtlasGeo::MakeSurfaceUpQuat(FVector::UpVector);
	TestTrue(TEXT("북극에서도 위쪽은 법선이다"),
		AtPole.GetUpVector().Equals(FVector::UpVector, 1.e-4));

	return true;
}

// 좌우 핀 이동. 화면 기준 방향과 한 바퀴 순환을 고정한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasGeo_PinNavigation, "FestaFiesta.UI.AtlasGeo.PinNavigation", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_AtlasGeo_PinNavigation::RunTest(const FString& Parameters)
{
	using namespace FFF_AtlasGeoTestHelpers;

	// 경도만 다른 핀 넷. 순서를 **일부러 섞어** 담는다 — 인덱스 의존을 잡기 위한 픽스처다.
	const TArray<FFF_AtlasLatLong> Pins = {
		FFF_AtlasLatLong(0.f, 90.f),	// 0: 동쪽
		FFF_AtlasLatLong(0.f, -90.f),	// 1: 서쪽
		FFF_AtlasLatLong(0.f, 0.f),		// 2: 정면
		FFF_AtlasLatLong(0.f, 180.f)	// 3: 반대편
	};

	// 픽스처가 의도대로 섞여 있는지 먼저 본다. 정렬돼 있으면 이 테스트가 아무것도 증명하지 않는다.
	TestTrue(TEXT("0번 원소가 경도 0 이 아니다 — 인덱스와 순서가 다르다"),
		!FMath::IsNearlyEqual(Pins[0].Longitude, 0.f));

	// 화면 오른쪽은 동쪽이다.
	TestEqual(TEXT("오른쪽 입력은 동쪽(+90) 핀으로 간다"),
		FFF_AtlasGeo::FindAdjacentPinIndex(Pins, 0.f, 1), 0);
	TestEqual(TEXT("왼쪽 입력은 서쪽(-90) 핀으로 간다"),
		FFF_AtlasGeo::FindAdjacentPinIndex(Pins, 0.f, -1), 1);

	// 한 바퀴가 닫힌다 — 0 → 90 → 180 → -90 → 0.
	TestEqual(TEXT("동쪽에서 오른쪽은 반대편(180)"),
		FFF_AtlasGeo::FindAdjacentPinIndex(Pins, 90.f, 1), 3);
	TestEqual(TEXT("반대편에서 오른쪽은 서쪽(-90) — 경도 경계를 넘는다"),
		FFF_AtlasGeo::FindAdjacentPinIndex(Pins, 180.f, 1), 1);
	TestEqual(TEXT("서쪽에서 오른쪽은 정면(0) — 한 바퀴가 닫힌다"),
		FFF_AtlasGeo::FindAdjacentPinIndex(Pins, -90.f, 1), 2);

	// 핀이 하나면 어느 방향이든 그 핀이다. 입력이 먹히지 않는 것처럼 보이지 않게 한다.
	const TArray<FFF_AtlasLatLong> Single = { FFF_AtlasLatLong(0.f, 0.f) };
	TestEqual(TEXT("핀이 하나면 자기 자신"), FFF_AtlasGeo::FindAdjacentPinIndex(Single, 0.f, 1), 0);

	// 목록이 비면 INDEX_NONE. 호출부가 인덱스 0 을 만지지 않게 한다.
	TestEqual(TEXT("빈 목록은 INDEX_NONE"),
		FFF_AtlasGeo::FindAdjacentPinIndex(TArray<FFF_AtlasLatLong>(), 0.f, 1), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("빈 목록의 최근접도 INDEX_NONE"),
		FFF_AtlasGeo::FindNearestPinIndex(TArray<FFF_AtlasLatLong>(), FFF_AtlasLatLong(0.f, 0.f)), static_cast<int32>(INDEX_NONE));

	// 최근접은 위도까지 본다. 경도만 보면 남북이 뒤집힌 핀을 고른다.
	const TArray<FFF_AtlasLatLong> NorthSouth = {
		FFF_AtlasLatLong(60.f, 10.f),
		FFF_AtlasLatLong(-60.f, 10.f)
	};
	TestEqual(TEXT("북쪽을 보고 있으면 북쪽 핀이 가깝다"),
		FFF_AtlasGeo::FindNearestPinIndex(NorthSouth, FFF_AtlasLatLong(55.f, 12.f)), 0);
	TestEqual(TEXT("남쪽을 보고 있으면 남쪽 핀이 가깝다"),
		FFF_AtlasGeo::FindNearestPinIndex(NorthSouth, FFF_AtlasLatLong(-55.f, 12.f)), 1);

	// 경도 경계를 넘어서도 최근접이 맞다. 각도 뺄셈으로 구현하면 여기가 틀린다.
	const TArray<FFF_AtlasLatLong> AcrossBoundary = {
		FFF_AtlasLatLong(0.f, 175.f),
		FFF_AtlasLatLong(0.f, 0.f)
	};
	TestEqual(TEXT("경도 -178 에서 가장 가까운 핀은 175 다"),
		FFF_AtlasGeo::FindNearestPinIndex(AcrossBoundary, FFF_AtlasLatLong(0.f, -178.f)), 0);

	// 경도차가 작은 실제 이웃 국가(영국 -1.5 / 프랑스 2.3)도 건너뛰지 않는다.
	// 자기 자신 제외 임계값을 몇 도 수준으로 키우면 여기가 깨진다.
	const TArray<FFF_AtlasLatLong> CloseNeighbours = {
		FFF_AtlasLatLong(51.5f, -1.5f),
		FFF_AtlasLatLong(48.9f, 2.3f)
	};
	TestEqual(TEXT("경도차 3.8도인 이웃도 오른쪽 한 칸이다"),
		FFF_AtlasGeo::FindAdjacentPinIndex(CloseNeighbours, -1.5f, 1), 1);
	TestEqual(TEXT("그 반대 방향도 한 칸이다"),
		FFF_AtlasGeo::FindAdjacentPinIndex(CloseNeighbours, 2.3f, -1), 0);

	return true;
}

// 🔴 경도가 겹치는 핀도 순회에서 빠지지 않는다. 좌표를 아직 안 채운 국가가 둘이면 실제로 겹친다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasGeo_PinTraversalVisitsEveryPin, "FestaFiesta.UI.AtlasGeo.PinTraversalVisitsEveryPin", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_AtlasGeo_PinTraversalVisitsEveryPin::RunTest(const FString& Parameters)
{
	using namespace FFF_AtlasGeoTestHelpers;

	// 0번·1번이 **완전히 같은 좌표**다. `Location` 을 채우지 않은 국가 두 개가 이 상태다.
	const TArray<FFF_AtlasLatLong> Pins = {
		FFF_AtlasLatLong(0.f, 0.f),
		FFF_AtlasLatLong(0.f, 0.f),
		FFF_AtlasLatLong(35.04f, 128.97f)
	};

	// 경도만 보고 첫 최소값만 남기면 1번은 좌우 어느 방향으로도 선택되지 않는다.
	// 한 방향으로 핀 개수만큼 밟으면 **모든 핀을 정확히 한 번씩** 지나야 한다.
	TSet<int32> Visited;
	int32 Current = 0;
	float Reference = Pins[0].Longitude;

	for (int32 Step = 0; Step < Pins.Num(); ++Step)
	{
		const int32 Next = FFF_AtlasGeo::FindAdjacentPinIndex(Pins, Reference, 1, Current);
		if (!TestTrue(TEXT("다음 핀이 유효하다"), Pins.IsValidIndex(Next)))
		{
			return false;
		}

		Visited.Add(Next);
		Current = Next;
		Reference = Pins[Next].Longitude;
	}

	TestEqual(TEXT("한 바퀴에 모든 핀을 지난다 — 겹친 핀도 빠지지 않는다"), Visited.Num(), Pins.Num());

	// 반대 방향도 같아야 한다.
	TSet<int32> VisitedBack;
	Current = 0;
	Reference = Pins[0].Longitude;
	for (int32 Step = 0; Step < Pins.Num(); ++Step)
	{
		const int32 Next = FFF_AtlasGeo::FindAdjacentPinIndex(Pins, Reference, -1, Current);
		VisitedBack.Add(Next);
		Current = Next;
		Reference = Pins.IsValidIndex(Next) ? Pins[Next].Longitude : Reference;
	}
	TestEqual(TEXT("왼쪽 방향도 모든 핀을 지난다"), VisitedBack.Num(), Pins.Num());

	// 경도가 다 다르면 `CurrentIndex` 를 주든 말든 결과가 같아야 한다 — 겹침 규칙이
	// 정상 데이터의 판정에 끼어들면 안 된다.
	const TArray<FFF_AtlasLatLong> Distinct = {
		FFF_AtlasLatLong(0.f, 90.f),	// 0
		FFF_AtlasLatLong(0.f, -90.f),	// 1
		FFF_AtlasLatLong(0.f, 0.f)		// 2
	};
	TestEqual(TEXT("정상 데이터: 0번 정면에서 오른쪽은 1번(180도)"),
		FFF_AtlasGeo::FindAdjacentPinIndex(Distinct, 90.f, 1, 0), 1);
	TestEqual(TEXT("CurrentIndex 를 안 줘도 같은 결과"),
		FFF_AtlasGeo::FindAdjacentPinIndex(Distinct, 90.f, 1), 1);

	// 정상 데이터에서 한 방향 순회도 모든 핀을 지난다.
	TSet<int32> DistinctVisited;
	int32 DistinctCurrent = 2;
	float DistinctReference = Distinct[2].Longitude;
	for (int32 Step = 0; Step < Distinct.Num(); ++Step)
	{
		const int32 Next = FFF_AtlasGeo::FindAdjacentPinIndex(Distinct, DistinctReference, 1, DistinctCurrent);
		DistinctVisited.Add(Next);
		DistinctCurrent = Next;
		DistinctReference = Distinct[Next].Longitude;
	}
	TestEqual(TEXT("정상 데이터도 한 바퀴에 모든 핀을 지난다"), DistinctVisited.Num(), Distinct.Num());

	return true;
}

// 전환 중 좌우 입력의 기준. 이게 틀리면 연타가 삼켜진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasGeo_StepReferenceUsesPendingTarget, "FestaFiesta.UI.AtlasGeo.StepReferenceUsesPendingTarget", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_AtlasGeo_StepReferenceUsesPendingTarget::RunTest(const FString& Parameters)
{
	using namespace FFF_AtlasGeoTestHelpers;

	// 전환이 없으면 지금 정면이 기준이다.
	TestEqual(TEXT("전환이 없으면 정면 경도가 기준"),
		FFF_AtlasGeo::StepReferenceLongitude(30.f, 90.f, false), 30.f, AtlasGeoAngleTolerance);

	// 전환 중이면 목표가 기준이다.
	TestEqual(TEXT("전환 중이면 목표 경도가 기준"),
		FFF_AtlasGeo::StepReferenceLongitude(30.f, 90.f, true), 90.f, AtlasGeoAngleTolerance);

	// 기준이 감긴다 — 누적된 목표 각도가 범위를 벗어나도 비교가 깨지지 않는다.
	TestEqual(TEXT("기준 경도가 -180~180 으로 감긴다"),
		FFF_AtlasGeo::StepReferenceLongitude(0.f, 200.f, true), -160.f, AtlasGeoAngleTolerance);

	// 🔴 연타가 한 칸씩 앞으로 나간다. KR → JP 로 가는 중에 오른쪽을 다시 누르면 US 로 가야 한다.
	const TArray<FFF_AtlasLatLong> Pins = {
		FFF_AtlasLatLong(35.f, 128.97f),	// 0: KR
		FFF_AtlasLatLong(36.f, 139.f),		// 1: JP
		FFF_AtlasLatLong(38.f, -98.f)		// 2: US
	};

	// KR 정면에서 오른쪽 → JP.
	const int32 First = FFF_AtlasGeo::FindAdjacentPinIndex(Pins, 128.97f, 1, 0);
	TestEqual(TEXT("KR 에서 오른쪽은 JP"), First, 1);

	// JP 로 가는 중에 오른쪽을 다시 누르면 US 로 나아간다.
	const float Reference = FFF_AtlasGeo::StepReferenceLongitude(134.f, Pins[First].Longitude, true);
	TestEqual(TEXT("전환 중 기준은 JP 경도다"), Reference, 139.f, AtlasGeoAngleTolerance);
	TestEqual(TEXT("전환 중 오른쪽 연타가 US 로 나아간다"),
		FFF_AtlasGeo::FindAdjacentPinIndex(Pins, Reference, 1, First), 2);

	// ★ 기준이 목표로 고정되므로 **연출이 얼마나 진행됐든 같은 핀이 나온다.**
	//   보간 중 각도를 기준으로 하면 누르는 시점에 따라 답이 달라져, 빠른 연타와 느린 연타가
	//   서로 다른 나라로 간다.
	for (const float Progress : { 0.f, 0.25f, 0.5f, 0.9f })
	{
		const float MidFront = FMath::Lerp(128.97f, 139.f, Progress);
		const float StableReference = FFF_AtlasGeo::StepReferenceLongitude(MidFront, Pins[First].Longitude, true);
		TestEqual(*FString::Printf(TEXT("연출 %.0f%% 지점에서도 다음 핀이 US 다"), Progress * 100.f),
			FFF_AtlasGeo::FindAdjacentPinIndex(Pins, StableReference, 1, First), 2);
	}

	return true;
}

// 목표 회전이 기울기 한계까지 눌린 상태로 만들어진다 — 도착 순간의 팝을 막는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasGeo_FocusOrientationIsClampedAtSource, "FestaFiesta.UI.AtlasGeo.FocusOrientationIsClampedAtSource", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_AtlasGeo_FocusOrientationIsClampedAtSource::RunTest(const FString& Parameters)
{
	using namespace FFF_AtlasGeoTestHelpers;

	constexpr float MaxTilt = 80.f;

	// 한계 안의 국가는 그대로 정면에 온다.
	const FFF_AtlasGlobeOrientation Normal =
		FFF_AtlasGeo::MakeFocusOrientation(FFF_AtlasLatLong(35.04f, 128.97f), MaxTilt);
	TestEqual(TEXT("한계 안이면 위도가 그대로 기울기가 된다"), Normal.Tilt, 35.04f, AtlasGeoAngleTolerance);
	TestEqual(TEXT("자전각은 경도 그대로"), Normal.Spin, 128.97f, AtlasGeoAngleTolerance);

	// 한계를 넘는 고위도 국가는 눌린다. **목표가 눌려 있어야** 보간 끝에 튀지 않는다.
	for (const float Latitude : { 85.f, 89.99f })
	{
		const FFF_AtlasGlobeOrientation High =
			FFF_AtlasGeo::MakeFocusOrientation(FFF_AtlasLatLong(Latitude, 0.f), MaxTilt);
		TestEqual(*FString::Printf(TEXT("위도 %.2f 목표가 한계로 눌린다"), Latitude),
			High.Tilt, MaxTilt, AtlasGeoAngleTolerance);

		// 보간 전 구간이 한계 안에 머문다 — 중간에 넘었다가 끝에서 되돌아오지 않는다.
		const FFF_AtlasGlobeOrientation Start(0.f, 0.f);
		for (const float Alpha : { 0.25f, 0.5f, 0.75f, 1.f })
		{
			const FFF_AtlasGlobeOrientation Mid = FFF_AtlasGeo::InterpolateOrientation(Start, High, Alpha);
			TestTrue(*FString::Printf(TEXT("위도 %.2f · Alpha %.2f 에서 한계를 넘지 않는다"), Latitude, Alpha),
				FMath::Abs(Mid.Tilt) <= MaxTilt + AtlasGeoAngleTolerance);
		}
	}

	// 남쪽도 대칭이다.
	const FFF_AtlasGlobeOrientation South =
		FFF_AtlasGeo::MakeFocusOrientation(FFF_AtlasLatLong(-85.f, 0.f), MaxTilt);
	TestEqual(TEXT("남쪽 고위도도 대칭으로 눌린다"), South.Tilt, -MaxTilt, AtlasGeoAngleTolerance);

	// 클램프를 하지 않는 MakeOrientationFacing 과 실제로 다르다 (둘이 같으면 이 테스트가 무의미하다).
	TestEqual(TEXT("클램프 없는 쪽은 위도 그대로다"),
		FFF_AtlasGeo::MakeOrientationFacing(FFF_AtlasLatLong(85.f, 0.f)).Tilt, 85.f, AtlasGeoAngleTolerance);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
