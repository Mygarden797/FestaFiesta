// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FF_AtlasScreenGeo.generated.h"

/**
 * 이미지를 위젯 사각형 안에 어떻게 맞출지.
 *
 * UMG 의 `UImage` 는 위젯 크기에 그냥 늘려 그린다 — `ScaleToFit` 같은 것이 없다.
 * 그래서 **우리가 실제로 그려질 사각형을 계산해서** 이미지 슬롯 크기와 핀 위치에
 * 같은 값을 쓴다. 둘이 다른 계산을 하면 핀이 지도에서 어긋난다.
 */
UENUM(BlueprintType)
enum class EFF_AtlasImageFit : uint8
{
	/** 위젯을 꽉 채운다. 비율이 깨진다. */
	Stretch		UMETA(DisplayName = "늘리기"),

	/**
	 * 비율을 지키며 **전부 보이게** 넣는다. 남는 쪽에 여백(레터박스)이 생긴다.
	 * 국가 지도의 기본값이다 — 지도가 잘리면 그 지역의 핀에 도달할 수 없다.
	 */
	Contain		UMETA(DisplayName = "전체 보이기"),

	/** 비율을 지키며 위젯을 덮는다. 남는 쪽이 잘린다. */
	Cover		UMETA(DisplayName = "덮기")
};

/**
 * 이미지가 위젯 안에서 **실제로 그려지는 사각형.**
 *
 * 핀은 이 사각형을 기준으로 배치한다. 위젯 전체를 기준으로 하면 레터박스 여백만큼
 * 전부 밀린다 — 창 크기를 바꿀 때마다 핀이 지도에서 벗어나는 원인이 그것이다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_AtlasImageRect
{
	GENERATED_BODY()

	/** 위젯 로컬 좌상단. 레터박스가 있으면 0 이 아니다. */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Screen")
	FVector2D Offset = FVector2D::ZeroVector;

	/** 실제로 그려진 크기. */
	UPROPERTY(BlueprintReadOnly, Category = "Atlas|Screen")
	FVector2D Size = FVector2D::ZeroVector;
};

/**
 * 지구본·국가 지도 **화면 쪽** 좌표 계산. U3-b 가 쓴다.
 *
 * === 왜 또 순수 함수인가 ===
 * `FFF_AtlasGeo`(구면 좌표)와 같은 이유다. 위젯 안에 두면 검증하려고 Slate 를 세워야 하고,
 * 그러면 **창 크기·DPI·비율이 바뀔 때만 깨지는** 버그가 남는다. 그건 개발 중 창 하나
 * 크기로만 보다가 다른 해상도에서 처음 발견되는 종류다.
 *
 * === 🔴 이 파일이 막는 것 둘 ===
 * 1. **레터박스 여백을 무시하는 것.** 위젯 전체를 기준으로 핀을 놓으면 비율이 다른
 *    창에서 핀이 지도 밖으로 나간다.
 * 2. **클릭과 드래그를 구분하지 않는 것.** 넓은 핀 버튼 뒤에 전면 드래그 표면을 깔면
 *    같은 클릭이 둘 다에 먹힌다.
 *
 * === 🔴 DPI 배율을 여기서 다루지 않는 이유 — 두 번 나누지 마라 ===
 * 월드 좌표를 화면에 얹을 때는 **`UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition`
 * 하나만 쓴다.** 그 함수가 내부에서 `USlateBlueprintLibrary::ScreenToViewport` 를 거쳐
 * **이미 배율을 제거한** 슬레이트 좌표를 돌려준다. 캔버스 슬롯에 그대로 넣으면 된다.
 *
 * 그래서 여기 `ViewportPixelToCanvasLocal` 같은 함수를 두지 않는다 — 있으면 누군가
 * 엔진 함수 결과에 한 번 더 적용해서 **배율이 두 번 들어간다.** 그건 배율 1 인 환경에서는
 * 티가 안 나고 창을 리사이즈할 때만 드러나는 종류다.
 *
 * `APlayerController::ProjectWorldLocationToScreen`(raw 픽셀)을 직접 쓰는 경로를 만들지 않는다.
 * 엔진 함수는 픽셀을 반올림한 **뒤에** 배율을 걷어내서 레이아웃 반올림에 의한 흔들림도 줄여 준다.
 */
struct FESTAFIESTA_API FFF_AtlasScreenGeo
{
	/**
	 * 이미지가 위젯 안에서 실제로 그려지는 사각형.
	 *
	 * 위젯이나 이미지 크기가 0 이면 빈 사각형을 돌려준다 — 0 으로 나누지 않는다.
	 * 레이아웃이 확정되기 전 첫 프레임에 크기가 0 으로 들어오는 일이 실제로 있다.
	 */
	static FFF_AtlasImageRect ComputeImageRect(
		const FVector2D& WidgetSize,
		const FVector2D& ImageSize,
		EFF_AtlasImageFit Fit);

	/**
	 * 정규화 좌표(0~1) → 위젯 로컬 좌표.
	 *
	 * ★ **"지도 비율이 바뀌어도 핀 위치 유지" 의 구현이 이것이다.** 카탈로그의
	 * `MapPosition` 을 픽셀이 아니라 0~1 로 둔 이유가 여기서 쓰인다.
	 * `(0, 0)` 이 이미지 좌상단, `(1, 1)` 이 우하단이다.
	 */
	static FVector2D NormalizedToWidgetLocal(
		const FVector2D& Normalized,
		const FFF_AtlasImageRect& ImageRect);

	/**
	 * 위의 역. 위젯 로컬 → 정규화 좌표.
	 * **에디터에서 핀 위치를 찍어 볼 때 쓴다** — 지도를 클릭해 그 자리의 `MapPosition` 을 얻는다.
	 * 사각형 밖을 클릭하면 0~1 밖의 값이 나온다. 클램프하지 않는다 —
	 * 밖이라는 사실을 호출부가 알아야 한다.
	 */
	static FVector2D WidgetLocalToNormalized(
		const FVector2D& WidgetLocal,
		const FFF_AtlasImageRect& ImageRect);

	/**
	 * 2D 국가 지도에서 입력 방향에 가장 알맞은 다음 핀을 찾는다.
	 *
	 * `PinPositions` 는 **화면 로컬 좌표**여야 한다. 정규화 좌표를 그대로 넘기면
	 * 가로·세로 비가 다른 지도에서 방향 각도가 달라지므로, 호출부가 `MapRect.Size` 를
	 * 곱해 종횡비를 반영한다.
	 *
	 * 현재 핀 앞쪽 반평면에 있는 후보만 비교하고, 가까우면서 입력 방향에 잘 정렬된 핀을
	 * 고른다. 후보가 없거나 입력이 유효하지 않으면 `INDEX_NONE` 이다. 배열 순서는
	 * 카탈로그 순서를 유지해야 동점 결과가 재현 가능하다.
	 */
	static int32 FindDirectionalPinIndex(
		const TArray<FVector2D>& PinPositions,
		int32 CurrentIndex,
		const FVector2D& ScreenDirection);
};

/**
 * 클릭과 드래그를 구분하는 상태 기계.
 *
 * === 왜 필요한가 ===
 * 확정안이 "국가 클릭 판정 = 넓은 UMG 핀 버튼" 인데, 지구를 드래그로 돌리려면 그 뒤에
 * 전면 드래그 표면이 필요하다. 그러면 **같은 마우스 클릭이 핀 클릭과 드래그 시작에
 * 둘 다 먹힌다** — 핀을 누르려고 살짝 움직였는데 지구가 돌아가거나, 지구를 돌리려고
 * 끌었는데 핀이 선택된다.
 *
 * 임계 픽셀 안에서 떼면 **클릭**, 넘으면 **드래그**로 승격한다.
 *
 * === UObject 가 아니다 ===
 * 위젯 멤버로 들고 쓰는 순수 구조체다. 그래서 Slate 없이 테스트된다
 * (`FFF_OverviewCameraRig` 와 같은 방식).
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_AtlasDragTracker
{
	GENERATED_BODY()

	/**
	 * 이 거리를 넘으면 드래그로 본다(픽셀). 축별이 아니라 **총 이동 거리**다 —
	 * 축별로 재면 대각선으로 천천히 끌 때 임계를 넘지 못한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Screen", meta = (ClampMin = "0.0"))
	float DragThresholdPixels = 5.f;

	/** 눌렀다. 이전 상태는 버린다. */
	void Press(const FVector2D& Position);

	/**
	 * 커서가 움직였다. **드래그로 승격된 뒤의 이동량만** 돌려준다 (승격 전에는 0).
	 *
	 * ★ 임계 안의 이동을 버리는 것은 의도다. 승격 순간에 쌓인 이동을 한꺼번에 적용하면
	 *   지구가 임계 거리만큼 **툭 튄다.** 작은 데드존이 낫다.
	 */
	FVector2D AddMove(const FVector2D& Position);

	/**
	 * 뗐다. **클릭으로 처리할지**를 돌려준다.
	 * 누른 적이 없으면 false — 위젯 밖에서 눌러 안에서 뗀 경우가 클릭이 되면 안 된다.
	 */
	bool Release();

	/** 지금 드래그 중인가. */
	bool IsDragging() const { return bDragging; }

	/** 누르고 있는 중인가. */
	bool IsPressed() const { return bPressed; }

	/** 상태를 비운다. 화면이 내려갈 때 부른다. */
	void Reset();

private:
	FVector2D PressPosition = FVector2D::ZeroVector;
	FVector2D LastPosition = FVector2D::ZeroVector;
	bool bPressed = false;
	bool bDragging = false;
};
