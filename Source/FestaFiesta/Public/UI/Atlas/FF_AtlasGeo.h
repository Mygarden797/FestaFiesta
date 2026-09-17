// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FF_AtlasGeo.generated.h"

/**
 * 지구본 위도·경도 한 쌍.
 *
 * `float` 두 개를 따로 넘기지 않는다. 인자 순서를 바꿔 부르면 (경도, 위도) 가 조용히 들어가고
 * 핀이 엉뚱한 데 찍힌다 — 컴파일러가 잡아 주지 않는 종류의 실수다.
 *
 * 필드는 `float` 다. 데이터 에셋에 사람이 넣는 값이라 double 정밀도가 의미가 없다.
 * 계산은 `FFF_AtlasGeo` 안에서 double 로 하고 경계에서만 좁힌다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_AtlasLatLong
{
	GENERATED_BODY()

	/** 위도. -90(남극) ~ +90(북극). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Geo", meta = (ClampMin = "-90.0", ClampMax = "90.0"))
	float Latitude = 0.f;

	/** 경도. -180 ~ +180. 커지면 동쪽이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Geo", meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float Longitude = 0.f;

	FFF_AtlasLatLong() = default;

	FFF_AtlasLatLong(float InLatitude, float InLongitude)
		: Latitude(InLatitude)
		, Longitude(InLongitude)
	{
	}
};

/**
 * 지구가 지금 어떻게 돌아가 있는가. **각도 두 개뿐이다.**
 *
 * === 왜 FQuat 이나 FRotator 를 상태로 들지 않는가 ===
 * 자유 회전을 쿼터니언으로 누적하면 **Roll 이 샌다.** 드래그를 몇 번 하면 지구가 비스듬히
 * 기울고 북극이 옆으로 눕는다. 되돌릴 방법도 없다.
 *
 * 각도 둘로 들면 그 일이 **구조적으로 불가능**하다 —
 * `Q * (0,0,1) = (sin Tilt, 0, cos Tilt)` 이고 `Spin` 이 무엇이든 Y 성분이 항상 정확히 0 이다.
 * 즉 자전축이 언제나 화면에서 수직으로 선다. 쿼터니언 상태에는 이 성질이 없다.
 * 두 회전 사이를 Slerp 하면 중간 구간에서 극이 기울어지는 것도 같은 이유다.
 *
 * 스냅·보간·클램프도 각도라서 그냥 된다. 쿼터니언 상태로는 "기울기만 제한" 을 표현할 수 없다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_AtlasGlobeOrientation
{
	GENERATED_BODY()

	/**
	 * 자전각(도). 로컬 +Z 축(자전축) 회전이다.
	 * **정면에 오는 경도가 곧 `Spin`** 이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Geo")
	float Spin = 0.f;

	/**
	 * 기울기(도). 로컬 +Y 축 회전이다. 커지면 북쪽이 정면으로 온다 —
	 * **정면에 오는 위도가 곧 `Tilt`** 다. 극점 근처에서 조작이 무너지므로 클램프한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Geo")
	float Tilt = 0.f;

	FFF_AtlasGlobeOrientation() = default;

	FFF_AtlasGlobeOrientation(float InSpin, float InTilt)
		: Spin(InSpin)
		, Tilt(InTilt)
	{
	}
};

/**
 * Festival Atlas 지구본의 좌표 규약과 그 계산. **이 파일이 규약의 유일한 원본이다.**
 *
 * === 왜 액터가 아니라 여기인가 ===
 * 액터의 멤버 함수로 두면 검증하려고 월드·컴포넌트·Tick 을 세워야 한다. 그러면 아무도
 * 테스트를 쓰지 않고, 극점·경도 경계처럼 **눈으로는 안 보이고 특정 좌표에서만 깨지는** 버그가
 * 남는다. 상태가 없는 계산만 떼어 두면 테스트가 한 줄이다 (`FF_AtlasGeoTests.cpp`).
 * `FFF_OverviewCameraRig` 를 카메라 Pawn 에서 떼어 둔 것과 같은 이유다.
 *
 * === 🔴 좌표 규약 — 여기 적힌 것만 믿는다 ===
 * 지구(정확히는 `AFF_AtlasGlobeActor` 의 회전 루트) 로컬 공간에서
 *
 * | 무엇 | 어디 |
 * | --- | --- |
 * | 위도 +90 (북극) | 로컬 `+Z` |
 * | 경도 0 (본초자오선) | 로컬 `+X` |
 * | 경도 +90 (동쪽) | 로컬 **`-Y`** |
 * | **화면 정면 (관측자 쪽)** | 로컬 `+X` |
 *
 * 즉 회전이 없을 때 위경도 (0, 0) 지점이 카메라를 향한다.
 * **액터의 +X 화살표가 카메라를 향하게 레벨에 놓는다.**
 *
 * === 🔴 왜 동쪽이 `-Y` 인가 — 왼손 좌표계 때문이다 ===
 * 지리 좌표계 감각대로 동쪽을 `+Y` 로 두면 **지구가 좌우로 뒤집혀 렌더된다.**
 * UE 는 왼손 좌표계이고 카메라 기저가 `우측 = 상방 × 전방` 이므로, `+X` 에서 `-X` 를 보는
 * 카메라의 **오른쪽은 월드 `-Y`** 다. 동쪽을 `+Y` 로 두면 동쪽이 화면 왼쪽에 오고,
 * 그리니치를 정면에 놓았을 때 유럽이 아메리카 쪽에 가 있게 된다.
 * 일본이 한국 서쪽에 붙는 종류의 오류이고, **수식은 전부 맞는데 화면만 틀리므로** 리뷰에서
 * 잘 빠져나간다. 부호 하나로 막는다.
 *
 * === 🔴 회전 합성 — `Ry` 의 부호 규약 ===
 * `Q = Ry(Tilt) * Rz(Spin)` 이다. `Rz`·`Ry` 는 **해당 축에 대한 오른손 회전**이며
 * `FQuat(FVector::YAxisVector, ...)` · `FQuat(FVector::ZAxisVector, ...)` 가 그것이다.
 *
 * ⚠️ **`Ry` 는 `FRotator` 의 `Pitch` 와 부호가 반대다.** UE 의 Pitch 는 +Y 축에 대한
 * *음의* 오른손 회전이다. 이 식을 "보기 좋게" `FRotator(Tilt, 0, 0).Quaternion()` 으로
 * 바꾸면 **기울기가 조용히 뒤집혀 모든 핀이 반대 반구로 간다.**
 *
 * `FQuat::operator*` 는 **오른쪽을 먼저** 적용하므로 자전(Z) 이 먼저, 기울기(Y) 가 나중이다.
 * 순서를 바꾸면 기울어진 축으로 자전해서 고위도에서 지구가 흔들린다.
 */
struct FESTAFIESTA_API FFF_AtlasGeo
{
	/** 위경도 왕복 비교에 쓰는 기본 허용오차(도). */
	static constexpr double DefaultAngleTolerance = 0.01;

	/**
	 * 극점 판정 여유. 수평 성분(√(X²+Y²))이 이보다 작으면 경도를 정의하지 않고 0 으로 둔다.
	 * 극점에서는 모든 경도가 같은 점이므로 이것이 유일하게 안전한 답이다.
	 */
	static constexpr double PoleEpsilon = 1.e-8;

	// --- 위경도 ↔ 벡터 ---

	/** 위경도 → 로컬 단위벡터. 길이는 항상 1 이다. */
	static FVector LatLongToUnitVector(const FFF_AtlasLatLong& LatLong);

	/**
	 * 단위벡터 → 위경도. 입력이 정규화돼 있지 않아도 방향만 본다. 영벡터는 (0, 0) 이다.
	 *
	 * 위도를 `Asin(Z)` 로 구하지 않고 엔진의 `FVector::ToOrientationRotator` 와 같은
	 * `Atan2(Z, √(X²+Y²))` 를 쓴다. `Asin` 은 극점에 가까워질수록 조건수가 `1/√(1-z²)` 로
	 * 나빠지는데 이쪽은 극점까지 균일하고 비용도 같다.
	 *
	 * ⚠️ 단 **이 선택을 테스트가 지켜 주지는 않는다.** 내부 계산이 double 이라 두 방식의
	 * 차이가 반환형(float)의 정밀도 아래로 묻힌다. 계산을 float 로 되돌리면 그때부터
	 * 위도 89.99 에서 실제로 0.02도가 틀리므로, **double 로 계산하는 것과 짝인 선택**이다.
	 */
	static FFF_AtlasLatLong UnitVectorToLatLong(const FVector& Direction);

	/** 위경도 → 반지름 `Radius` 구면 위의 로컬 위치. 핀·조형물을 붙일 자리다. */
	static FVector LatLongToSurfacePosition(const FFF_AtlasLatLong& LatLong, float Radius);

	/** 위도 -90~90 클램프, 경도 -180~180 감기. 데이터가 범위를 벗어나도 계산이 깨지지 않게 한다. */
	static FFF_AtlasLatLong NormalizeLatLong(const FFF_AtlasLatLong& LatLong);

	// --- 회전 ---

	/**
	 * 이 위경도를 **정면(로컬 +X)** 으로 가져오는 회전. `Spin = 경도`, `Tilt = 위도` 다.
	 *
	 * 기울기 클램프는 하지 않는다 — 클램프는 조작(`ApplyDrag`)의 몫이고, 데이터가 가리키는
	 * 지점은 극점이어도 정직하게 돌려준다.
	 */
	static FFF_AtlasGlobeOrientation MakeOrientationFacing(const FFF_AtlasLatLong& LatLong);

	/**
	 * 연출의 **목표**로 쓸 회전. `MakeOrientationFacing` 에 기울기 클램프까지 적용한 것이다.
	 *
	 * ★ 클램프를 여기서 하는 이유 — 목표를 클램프하지 않고 도착할 때만 누르면
	 *   보간 중에 기울기가 한계를 넘어갔다가 **마지막 프레임에 툭 되돌아온다.**
	 *   시작·중간·끝이 같은 클램프를 봐야 그 팝이 사라진다.
	 *
	 * 위도가 한계를 넘는 국가는 정면에 완전히 오지 않는다. 그게 클램프의 정의이므로
	 * 액터는 **도착 판정을 이 함수의 결과로** 한다 — 위도 85 국가를 넣고 도착을 못 알리는
	 * 쪽보다, 80까지 눕히고 도착으로 보는 쪽이 맞다.
	 */
	static FFF_AtlasGlobeOrientation MakeFocusOrientation(
		const FFF_AtlasLatLong& LatLong,
		float MaxTiltDegrees);

	/**
	 * 국가 지점을 지정 방향으로 가져온다. 방향은 **회전 전 액터의 고정 로컬 공간**이며,
	 * 화면 목표 위치의 구면 교점 법선을 호출부에서 변환해 넘긴다. 카메라·뷰포트를 읽지 않는다.
	 *
	 * 기존 `Ry(Tilt) * Rz(Spin)` 두 축만 사용한다. 도달 가능한 방향은 정확히 맞추고,
	 * 고위도의 수평 이동 또는 기울기 제한 때문에 불가능하면 허용 범위에서 각도 오차가
	 * 가장 작은 회전을 선택한다. 동률이면 기존 +X 정렬을 먼저 고려한 후보 순서를 유지한다.
	 * `+X` 입력은 기존 `MakeFocusOrientation`과 같다. 영벡터·비유한 방향도 +X로 대체하고,
	 * 비유한 위경도 성분은 0, 비유한 기울기 한계는 0으로 처리한다.
	 */
	static FFF_AtlasGlobeOrientation MakeFocusOrientationToward(
		const FFF_AtlasLatLong& LatLong,
		const FVector& DesiredLocalDirection,
		float MaxTiltDegrees);

	/** 지금 정면에 와 있는 위경도. `MakeOrientationFacing` 의 역이다. */
	static FFF_AtlasLatLong OrientationToFrontLatLong(const FFF_AtlasGlobeOrientation& Orientation);

	/** 회전 상태 → 쿼터니언. 액터가 회전 루트에 그대로 넣는다. */
	static FQuat OrientationToQuat(const FFF_AtlasGlobeOrientation& Orientation);

	/** 자전각을 -180~180 으로 감고 기울기를 클램프한다. 누적 오차로 각도가 커지는 것을 막는다. */
	static FFF_AtlasGlobeOrientation NormalizeOrientation(
		const FFF_AtlasGlobeOrientation& Orientation,
		float MaxTiltDegrees);

	/**
	 * 드래그 픽셀 → 새 회전 상태.
	 *
	 * **지표를 손으로 잡아 끄는 방향**이다. 오른쪽으로 끌면 정면의 지표가 화면 오른쪽으로 가고
	 * (= 서쪽이 정면으로 온다), 아래로 끌면 북쪽이 정면으로 온다.
	 * 관성은 넣지 않는다 (지금 필요 없다).
	 */
	static FFF_AtlasGlobeOrientation ApplyDrag(
		const FFF_AtlasGlobeOrientation& Orientation,
		const FVector2D& DragPixels,
		float DegreesPerPixel,
		float MaxTiltDegrees);

	/**
	 * 두 회전 사이 보간. **자전각은 짧은 쪽으로 돈다.**
	 *
	 * 자전각 170 에서 -170 으로 갈 때 단순 선형 보간은 지구를 340도 되돌린다.
	 * 기울기는 감기지 않는 축이라 그냥 선형 보간한다.
	 */
	static FFF_AtlasGlobeOrientation InterpolateOrientation(
		const FFF_AtlasGlobeOrientation& From,
		const FFF_AtlasGlobeOrientation& To,
		float Alpha);

	// --- 가시성 ---

	/**
	 * 원근 카메라에서 지구 테두리(림)에 해당하는 내적 값.
	 *
	 * 반지름 `r` 인 구를 거리 `d` 에서 보면 **보이는 것은 반구가 아니다** — 접점의 내적이
	 * `r/d` 이므로 그보다 작은 핀은 실루엣 뒤에 있다. 기준을 0 으로 두면 지구 테두리
	 * **바깥쪽에서 핀이 튀어나온다.** 카메라가 가까울수록 차이가 커진다.
	 *
	 * 거리가 반지름보다 작으면(카메라가 지구 안) 1 을 돌려 전부 숨긴다.
	 */
	static float HorizonMinDot(float Radius, float ViewerDistance);

	/**
	 * 반지름 `SphereRadius`인 구 위로 솟은 점(`PointRadius`)이 지평선 너머에서 처음 보이는 내적 값.
	 *
	 * `HorizonMinDot(PointRadius, ViewerDistance)`를 쓰면 높이가 클수록 임계가 올라가 오히려
	 * 늦게 보인다. 실제 경계는 관측자-꼭대기 선분이 안쪽 구에 접하는 순간이며 다음과 같다.
	 *
	 * `cos(acos(R / D) + acos(R / P))`
	 *
	 * `PointRadius == SphereRadius`면 `HorizonMinDot`과 정확히 같다. 관측자는 구 밖이어야 하고
	 * 점은 구 표면 이상이어야 한다. 조건이 잘못되면 안전하게 1을 돌린다.
	 */
	static float ElevatedHorizonMinDot(float SphereRadius, float PointRadius, float ViewerDistance);

	/**
	 * 이 방향이 관측자를 향하고 있나. 두 벡터의 내적이 `MinDot` 이상이면 true.
	 * 공간을 가리지 않는다 — 둘 다 로컬이거나 둘 다 월드이면 된다.
	 */
	static bool IsFacingViewer(const FVector& Normal, const FVector& ViewerDirection, float MinDot);

	/**
	 * 이 핀이 지금 앞면인가. 지구 뒤편으로 넘어간 핀·조형물을 숨기는 판정이다.
	 *
	 * 월드 변환을 거치지 않는다 — 액터가 어디에 놓여 있어도 `앞면` 은 회전 루트의 +X 쪽이므로
	 * 로컬에서 판정하는 것이 맞고, 카메라 방향을 몰라도 된다.
	 * `MinDot` 은 `HorizonMinDot` 으로 구한 값을 넣는다.
	 */
	static bool IsPinFacingFront(
		const FFF_AtlasLatLong& Pin,
		const FFF_AtlasGlobeOrientation& Orientation,
		float MinDot);

	/**
	 * 로컬 `+Z` 를 이 법선에 맞추는 회전. 조형물의 **위쪽을 지표 법선에 세우는** 데 쓴다.
	 * 이게 없으면 조형물이 전부 월드 위쪽을 보고 서서 적도 근처만 제대로 보인다.
	 *
	 * `ReferenceNorth` 는 남은 한 축을 정하는 기준이다. 기본값(로컬 +Z = 북극 방향)이면
	 * 조형물의 앞면이 **북쪽을 향해** 서고, 지구가 돌아도 방향이 흔들리지 않는다.
	 * 기준 없이 `MakeFromZ` 만 쓰면 남은 두 축이 임의로 채워져 **조형물마다 제멋대로 돌아간다.**
	 * 극점에서는 법선과 기준이 평행해지는데, 그때는 엔진이 임의 축으로 대체해 준다.
	 */
	static FQuat MakeSurfaceUpQuat(
		const FVector& SurfaceNormal,
		const FVector& ReferenceNorth = FVector::ZAxisVector);

	// --- 핀 이동 ---

	/**
	 * 목표 지점에서 가장 가까운 핀. 대권거리(내적)로 재므로 경도 경계에서도 맞다.
	 * 목록이 비면 `INDEX_NONE`.
	 *
	 * 스틱으로 자유 회전한 뒤 **가장 가까운 핀으로 재스냅** 하는 경로가 이걸 쓴다.
	 * 완전히 같은 좌표가 둘이면 앞 인덱스를 돌려준다 (여기서는 어느 쪽이든 같은 지점이다).
	 */
	static int32 FindNearestPinIndex(const TArray<FFF_AtlasLatLong>& Pins, const FFF_AtlasLatLong& Target);

	/**
	 * 정면 기준 좌우로 다음 핀. `ScreenDirection` 은 **화면 기준**이며 `+1` 이 오른쪽이다.
	 *
	 * 화면 오른쪽은 동쪽(경도가 커지는 쪽)이다 — 좌표 규약이 그렇게 되도록 동쪽을 `-Y` 로
	 * 잡아 두었기 때문에 여기서 부호를 뒤집을 일이 없다.
	 *
	 * `CurrentIndex` 는 지금 포커스된 핀이다(모르면 `INDEX_NONE`).
	 * ★ **경도가 같은 핀이 둘 이상일 때 이것이 필요하다.** 경도만으로 고르면 회전량이 완전히
	 *   같아서 언제나 앞 인덱스만 뽑히고 **뒤 인덱스에는 좌우 입력으로 영영 도달할 수 없다.**
	 *   좌표를 아직 채우지 않은 국가가 둘이면 (0, 0) 이 겹쳐 실제로 그렇게 된다.
	 *   동률이면 `CurrentIndex` 다음 인덱스부터 고르므로 순회가 모든 핀을 지난다.
	 *
	 * 한 바퀴를 돌아 자기 자신으로 돌아온다. 핀이 하나뿐이면 그 핀을 돌려준다.
	 * 잠긴 국가도 후보에 넣는다 — **잠긴 핀에도 포커스는 간다.** 선택만 막고 사유를 띄운다.
	 * 목록이 비면 `INDEX_NONE`.
	 */
	static int32 FindAdjacentPinIndex(
		const TArray<FFF_AtlasLatLong>& Pins,
		float FrontLongitude,
		int32 ScreenDirection,
		int32 CurrentIndex = INDEX_NONE);

	/**
	 * 좌우 입력이 기준으로 삼을 경도.
	 *
	 * ★ **전환 중에는 목표를 기준으로 삼는다.** 보간 중인 실제 각도를 기준으로 하면,
	 *   이미 향해 가고 있는 핀이 여전히 그 방향의 최근접이라서 **같은 핀을 다시 고르고
	 *   연출만 처음부터 되돌린다.** 게임패드로 전환 시간보다 빠르게 연타하면 지구가 첫 핀
	 *   근처를 기어가기만 하고 도착 알림이 영영 울리지 않는다.
	 *
	 * 액터의 멤버 함수로 두면 이 판정을 테스트할 수 없어서 여기 둔다.
	 */
	static float StepReferenceLongitude(
		float FrontLongitude,
		float PendingTargetLongitude,
		bool bTransitioning);
};
