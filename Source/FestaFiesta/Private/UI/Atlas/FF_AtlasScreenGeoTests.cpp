// Fill out your copyright notice in the Description page of Project Settings.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/Atlas/FF_AtlasScreenGeo.h"

/**
 * 화면 좌표 계산 고정 테스트 (U3-b 토대).
 *
 * === 왜 이 테스트가 있어야 하나 ===
 * 여기 계산이 틀리면 **개발 PC 에서는 맞고 다른 창 크기·해상도에서만 틀린다.**
 * 그건 만드는 동안에는 절대 안 보이고 남이 실행할 때 처음 드러나는 종류다.
 * 세 가지를 못으로 박는다 —
 *   ① DPI 배율 나눗셈 (빼먹으면 UI 배율 1 이 아닌 환경에서 핀이 전부 밀린다)
 *   ② 레터박스 여백 (무시하면 비율이 다른 창에서 핀이 지도 밖으로 나간다)
 *   ③ 클릭/드래그 임계 (없으면 핀 클릭이 지구 회전으로도 먹힌다)
 *
 * ⚠️ LWC — `FVector2D` 성분은 double 이다. 기대값·허용오차에 `f` 를 붙이면
 *    float·double 오버로드가 모두 후보가 되어 C2666 으로 컴파일이 막힌다.
 */
namespace FFF_AtlasScreenGeoTestHelpers
{
	constexpr double AtlasScreenTolerance = 1.e-6;

	/** 흔한 창 크기·이미지 비율 조합. 정사각·가로긴·세로긴을 섞는다. */
	inline FString DescribeSize(const FVector2D& Size)
	{
		return FString::Printf(TEXT("(%.0f x %.0f)"), Size.X, Size.Y);
	}
}
// 레터박스. 비율이 다른 창에서 핀이 지도 밖으로 나가는 것을 막는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasScreenGeo_ImageRectLetterboxes, "FestaFiesta.UI.AtlasScreenGeo.ImageRectLetterboxes", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_AtlasScreenGeo_ImageRectLetterboxes::RunTest(const FString& Parameters)
{
	using namespace FFF_AtlasScreenGeoTestHelpers;

	const FVector2D Image(2000.0, 1000.0);	// 2:1

	// --- Contain: 위젯이 이미지보다 세로로 길면 위아래에 여백 ---
	{
		const FVector2D Widget(1000.0, 1000.0);	// 1:1
		const FFF_AtlasImageRect Rect =
			FFF_AtlasScreenGeo::ComputeImageRect(Widget, Image, EFF_AtlasImageFit::Contain);

		TestTrue(TEXT("Contain: 가로를 꽉 채운다"), Rect.Size.Equals(FVector2D(1000.0, 500.0), AtlasScreenTolerance));
		TestTrue(TEXT("Contain: 위아래 여백이 250 씩"), Rect.Offset.Equals(FVector2D(0.0, 250.0), AtlasScreenTolerance));
	}

	// --- Contain: 위젯이 이미지보다 가로로 길면 좌우에 여백 ---
	{
		const FVector2D Widget(3000.0, 1000.0);	// 3:1
		const FFF_AtlasImageRect Rect =
			FFF_AtlasScreenGeo::ComputeImageRect(Widget, Image, EFF_AtlasImageFit::Contain);

		TestTrue(TEXT("Contain: 세로를 꽉 채운다"), Rect.Size.Equals(FVector2D(2000.0, 1000.0), AtlasScreenTolerance));
		TestTrue(TEXT("Contain: 좌우 여백이 500 씩"), Rect.Offset.Equals(FVector2D(500.0, 0.0), AtlasScreenTolerance));
	}

	// --- 비율이 같으면 여백이 없다 ---
	{
		const FFF_AtlasImageRect Rect =
			FFF_AtlasScreenGeo::ComputeImageRect(FVector2D(1000.0, 500.0), Image, EFF_AtlasImageFit::Contain);
		TestTrue(TEXT("비율이 같으면 여백이 0"), Rect.Offset.Equals(FVector2D::ZeroVector, AtlasScreenTolerance));
	}

	// --- Cover: 넘쳐서 잘린다. Offset 이 음수다 ---
	{
		const FVector2D Widget(1000.0, 1000.0);
		const FFF_AtlasImageRect Rect =
			FFF_AtlasScreenGeo::ComputeImageRect(Widget, Image, EFF_AtlasImageFit::Cover);

		TestTrue(TEXT("Cover: 세로를 채우려 가로가 넘친다"),
			Rect.Size.Equals(FVector2D(2000.0, 1000.0), AtlasScreenTolerance));
		TestTrue(TEXT("Cover: Offset 이 음수 — 잘린다는 뜻"), Rect.Offset.X < 0.0);
	}

	// --- Stretch: 비율을 버리고 꽉 채운다 ---
	{
		const FVector2D Widget(1000.0, 1000.0);
		const FFF_AtlasImageRect Rect =
			FFF_AtlasScreenGeo::ComputeImageRect(Widget, Image, EFF_AtlasImageFit::Stretch);

		TestTrue(TEXT("Stretch: 위젯 크기 그대로"), Rect.Size.Equals(Widget, AtlasScreenTolerance));
		TestTrue(TEXT("Stretch: 여백 없음"), Rect.Offset.Equals(FVector2D::ZeroVector, AtlasScreenTolerance));
	}

	// --- 크기가 0 이면 빈 사각형. 0 으로 나누지 않는다 (첫 프레임에 실제로 들어온다) ---
	for (const FVector2D& BadWidget : { FVector2D(0.0, 0.0), FVector2D(1000.0, 0.0), FVector2D(0.0, 1000.0) })
	{
		const FFF_AtlasImageRect Rect =
			FFF_AtlasScreenGeo::ComputeImageRect(BadWidget, Image, EFF_AtlasImageFit::Contain);
		TestTrue(*FString::Printf(TEXT("위젯 %s 는 빈 사각형"), *DescribeSize(BadWidget)),
			Rect.Size.IsNearlyZero());
	}

	const FFF_AtlasImageRect NoImage =
		FFF_AtlasScreenGeo::ComputeImageRect(FVector2D(1000.0, 1000.0), FVector2D::ZeroVector, EFF_AtlasImageFit::Contain);
	TestTrue(TEXT("이미지 크기가 0 이면 빈 사각형"), NoImage.Size.IsNearlyZero());

	return true;
}

// 🔴 핀 위치의 핵심 성질 — 창 크기가 바뀌어도 핀이 지도의 같은 지점에 있다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasScreenGeo_PinStaysOnMapAcrossSizes, "FestaFiesta.UI.AtlasScreenGeo.PinStaysOnMapAcrossSizes", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_AtlasScreenGeo_PinStaysOnMapAcrossSizes::RunTest(const FString& Parameters)
{
	using namespace FFF_AtlasScreenGeoTestHelpers;

	const FVector2D Image(2000.0, 1000.0);
	const FVector2D Pin(0.62, 0.38);	// 다대포쯤이라고 치자

	// 창 크기를 크게 바꿔도 **정규화 좌표로 되돌리면 같은 값**이어야 한다.
	// 이것이 MapPosition 을 픽셀이 아니라 0~1 로 둔 이유다.
	for (const FVector2D& Widget : {
			FVector2D(1920.0, 1080.0),
			FVector2D(1280.0, 720.0),
			FVector2D(1000.0, 1000.0),	// 정사각 — 레터박스가 크다
			FVector2D(3440.0, 1440.0),	// 울트라와이드
			FVector2D(800.0, 1200.0) })	// 세로 모니터
	{
		const FFF_AtlasImageRect Rect =
			FFF_AtlasScreenGeo::ComputeImageRect(Widget, Image, EFF_AtlasImageFit::Contain);

		const FVector2D Local = FFF_AtlasScreenGeo::NormalizedToWidgetLocal(Pin, Rect);
		const FVector2D Back = FFF_AtlasScreenGeo::WidgetLocalToNormalized(Local, Rect);

		TestTrue(*FString::Printf(TEXT("창 %s 에서 핀이 같은 지점"), *DescribeSize(Widget)),
			Back.Equals(Pin, 1.e-9));

		// 그리고 그 지점은 **그려진 사각형 안**에 있어야 한다. 위젯 전체를 기준으로
		// 계산했다면 레터박스가 큰 창에서 여기가 깨진다.
		TestTrue(*FString::Printf(TEXT("창 %s 에서 핀이 지도 안에 있다"), *DescribeSize(Widget)),
			Local.X >= Rect.Offset.X && Local.X <= Rect.Offset.X + Rect.Size.X &&
			Local.Y >= Rect.Offset.Y && Local.Y <= Rect.Offset.Y + Rect.Size.Y);
	}

	// 네 모서리가 그려진 사각형의 네 모서리에 정확히 맞는다.
	{
		const FFF_AtlasImageRect Rect =
			FFF_AtlasScreenGeo::ComputeImageRect(FVector2D(1000.0, 1000.0), Image, EFF_AtlasImageFit::Contain);

		TestTrue(TEXT("(0,0) 은 그려진 사각형의 좌상단"),
			FFF_AtlasScreenGeo::NormalizedToWidgetLocal(FVector2D(0.0, 0.0), Rect)
				.Equals(Rect.Offset, AtlasScreenTolerance));
		TestTrue(TEXT("(1,1) 은 그려진 사각형의 우하단"),
			FFF_AtlasScreenGeo::NormalizedToWidgetLocal(FVector2D(1.0, 1.0), Rect)
				.Equals(Rect.Offset + Rect.Size, AtlasScreenTolerance));
		TestTrue(TEXT("(0.5,0.5) 는 위젯 가운데"),
			FFF_AtlasScreenGeo::NormalizedToWidgetLocal(FVector2D(0.5, 0.5), Rect)
				.Equals(FVector2D(500.0, 500.0), AtlasScreenTolerance));
	}

	// 사각형이 없으면 되돌릴 기준이 없다. 0 으로 나누지 않는다.
	TestTrue(TEXT("빈 사각형에서 역변환은 0"),
		FFF_AtlasScreenGeo::WidgetLocalToNormalized(FVector2D(100.0, 100.0), FFF_AtlasImageRect())
			.IsNearlyZero());

	return true;
}

// 클릭과 드래그를 구분한다. 없으면 핀 클릭이 지구 회전으로도 먹힌다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasScreenGeo_DragTrackerSeparatesClick, "FestaFiesta.UI.AtlasScreenGeo.DragTrackerSeparatesClick", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_AtlasScreenGeo_DragTrackerSeparatesClick::RunTest(const FString& Parameters)
{
	using namespace FFF_AtlasScreenGeoTestHelpers;

	// --- 임계 안에서 떼면 클릭이다 ---
	{
		FFF_AtlasDragTracker Tracker;
		Tracker.DragThresholdPixels = 5.f;

		Tracker.Press(FVector2D(100.0, 100.0));
		const FVector2D Delta = Tracker.AddMove(FVector2D(103.0, 102.0));	// 거리 3.6

		TestTrue(TEXT("임계 안의 이동은 회전량이 0 이다 (데드존)"), Delta.IsNearlyZero());
		TestFalse(TEXT("아직 드래그가 아니다"), Tracker.IsDragging());
		TestTrue(TEXT("떼면 클릭이다"), Tracker.Release());
	}

	// --- 임계를 넘으면 드래그다. 떼도 클릭이 아니다 ---
	{
		FFF_AtlasDragTracker Tracker;
		Tracker.DragThresholdPixels = 5.f;

		Tracker.Press(FVector2D(100.0, 100.0));
		Tracker.AddMove(FVector2D(120.0, 100.0));	// 거리 20 → 승격

		TestTrue(TEXT("임계를 넘으면 드래그로 승격된다"), Tracker.IsDragging());

		// 승격 이후의 이동은 그대로 회전량이 된다.
		const FVector2D Delta = Tracker.AddMove(FVector2D(130.0, 105.0));
		TestTrue(TEXT("승격 뒤에는 이동량이 그대로 나온다"),
			Delta.Equals(FVector2D(10.0, 5.0), AtlasScreenTolerance));

		TestFalse(TEXT("드래그였으면 떼도 클릭이 아니다"), Tracker.Release());
	}

	// --- 🔴 승격 순간에 쌓인 이동을 한꺼번에 적용하지 않는다 (지구가 툭 튀지 않게) ---
	{
		FFF_AtlasDragTracker Tracker;
		Tracker.DragThresholdPixels = 5.f;

		Tracker.Press(FVector2D(0.0, 0.0));
		const FVector2D PromotionDelta = Tracker.AddMove(FVector2D(100.0, 0.0));	// 크게 한 번

		TestTrue(TEXT("승격되는 프레임의 이동량은 0 이다"), PromotionDelta.IsNearlyZero());
		TestTrue(TEXT("그래도 드래그 상태다"), Tracker.IsDragging());
	}

	// --- 대각선. 축별로 재면 여기가 깨진다 ---
	{
		FFF_AtlasDragTracker Tracker;
		Tracker.DragThresholdPixels = 5.f;

		Tracker.Press(FVector2D(0.0, 0.0));
		// 각 축은 4 로 임계 5 보다 작지만 총 거리는 5.66 이라 넘는다.
		Tracker.AddMove(FVector2D(4.0, 4.0));

		TestTrue(TEXT("대각선은 총 거리로 판정한다"), Tracker.IsDragging());
	}

	// --- 누르지 않고 움직이는 것은 호버다. 회전시키지 않는다 ---
	{
		FFF_AtlasDragTracker Tracker;
		const FVector2D Delta = Tracker.AddMove(FVector2D(500.0, 500.0));

		TestTrue(TEXT("누르지 않은 이동은 무시한다"), Delta.IsNearlyZero());
		TestFalse(TEXT("드래그가 아니다"), Tracker.IsDragging());
		TestFalse(TEXT("누른 적 없이 떼면 클릭이 아니다"), Tracker.Release());
	}

	// --- 임계 0 이면 아주 작은 이동도 드래그다 ---
	{
		FFF_AtlasDragTracker Tracker;
		Tracker.DragThresholdPixels = 0.f;

		Tracker.Press(FVector2D(0.0, 0.0));
		Tracker.AddMove(FVector2D(0.5, 0.0));
		TestTrue(TEXT("임계 0 이면 즉시 드래그"), Tracker.IsDragging());
	}

	// --- Reset 뒤에는 아무 상태도 남지 않는다 ---
	{
		FFF_AtlasDragTracker Tracker;
		Tracker.Press(FVector2D(0.0, 0.0));
		Tracker.AddMove(FVector2D(100.0, 0.0));
		Tracker.Reset();

		TestFalse(TEXT("Reset 뒤에는 누른 상태가 아니다"), Tracker.IsPressed());
		TestFalse(TEXT("Reset 뒤에는 드래그가 아니다"), Tracker.IsDragging());
		TestTrue(TEXT("Reset 뒤 이동은 무시된다"), Tracker.AddMove(FVector2D(200.0, 0.0)).IsNearlyZero());
	}

	// --- 다시 누르면 이전 드래그 상태가 남지 않는다 ---
	{
		FFF_AtlasDragTracker Tracker;
		Tracker.DragThresholdPixels = 5.f;

		Tracker.Press(FVector2D(0.0, 0.0));
		Tracker.AddMove(FVector2D(100.0, 0.0));	// 드래그
		Tracker.Press(FVector2D(0.0, 0.0));		// 새로 누른다

		TestFalse(TEXT("새로 누르면 드래그 상태가 초기화된다"), Tracker.IsDragging());
		TestTrue(TEXT("그리고 짧게 떼면 클릭이다"), Tracker.Release());
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
