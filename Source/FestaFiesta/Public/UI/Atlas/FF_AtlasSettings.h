// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FF_AtlasSettings.generated.h"

/**
 * Festival Atlas 연출 수치. `Project Settings > Game > FestaFiesta Atlas` 에서 조정한다.
 *
 * === 왜 `UFF_UIFlowSettings` 에 넣지 않는가 ===
 * 흐름 설정은 **어느 화면을 올리는가** 하는 배선표다. 여기 값은 **얼마나 오래 움직이는가**이고
 * 고치는 사람도 고치는 시점도 다르다. 한 페이지에 섞으면 연출을 손볼 때마다 화면 배선표를
 * 스크롤하게 된다. 팔레트를 색과 모션만 갖게 둔 것과 같은 이유다.
 *
 * === 액터가 아니라 여기인 것과 액터에 남은 것 ===
 * **`언제 얼마나 움직이나` 는 여기, `무엇으로 그리나·얼마나 크나` 는 액터**다.
 * 지구 접근·국가 지도 전환 시간은 액터가 아니라 화면(U3-b·U3-c)이 쓰므로 액터에 두면
 * 화면이 액터를 찾아야 한다. 반지름·메시 슬롯처럼 액터 하나에만 의미가 있는 값은 액터에 있다.
 *
 * 기본값은 `docs/ui/visual-direction.md` 4절 「연출 기본값」 표를 그대로 옮긴 것이다.
 * 표에 `0.6~1.0초` 처럼 범위로 적힌 것은 중간값을 넣었다.
 *
 * === 🔴 움직임 감소 ===
 * 여기 적힌 초를 액터가 **그대로 쓰지 않는다.** `움직임 감소` 가 켜져 있으면 전환은 0초,
 * 자동 자전은 정지다. 그 판정은 `UFF_UIAccessibilitySubsystem` 한 곳에만 있다 —
 * 판정을 여기저기 두면 어디 하나가 빠지고 그 화면만 계속 움직인다.
 *
 * === ⚠️ 여기를 에디터에서 고치면 팀 전체에 퍼진다 ===
 * `Config = Game` 이라 값을 바꾸면 **팀 공용 `Config/DefaultGame.ini`** 에 기록된다.
 * 코드의 기본값만 쓰는 동안은 아무 일도 없지만, Project Settings 에서 숫자를 만지면
 * 그 커밋이 전원의 연출을 바꾼다. 혼자 실험할 때는 액터 인스턴스 쪽을 만지거나 커밋에서 뺀다.
 * (`bUseDummySaveSlots=True` 가 이렇게 팀에 퍼진 전례가 있다.)
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "FestaFiesta Atlas"))
class FESTAFIESTA_API UFF_AtlasSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** CDO 를 돌려주므로 항상 유효하다. `DefaultGame.ini` 값이 이미 반영된 상태다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	static const UFF_AtlasSettings* Get();

	// --- 자동 자전 ---

	/**
	 * 타이틀에서 지구가 도는 속도(도/초). **매우 느리게** 가 기본 방향이다.
	 * 1도/초면 한 바퀴에 6분이다 — 배경으로 살아 있는 느낌만 주고 시선을 끌지 않는다.
	 *
	 * ★ 양수가 **실제 지구 자전 방향**(동쪽)이다. 액터가 부호를 맞춰 넣는다.
	 *   음수를 넣으면 해가 서쪽에서 뜨는 지구가 된다.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Auto Spin", meta = (ClampMin = "0.0", ClampMax = "60.0", ForceUnits = "deg/s"))
	float AutoSpinDegreesPerSecond = 1.f;

	/**
	 * 입력이 끊긴 뒤 자동 자전이 다시 시작되기까지의 시간(초).
	 * 너무 짧으면 손을 뗀 순간 지구가 스스로 움직여 조작을 빼앗긴 느낌이 된다.
	 *
	 * ★ **첫 진입에는 적용되지 않는다.** 지구본에 들어오면 처음부터 돌고 있고,
	 *   이 지연은 플레이어가 한 번이라도 조작한 뒤에만 걸린다 (2026-08-23 백종태 요청).
	 *   그러지 않으면 화면에 들어온 직후 이 시간만큼 지구가 멈춰 있어 배경이 죽은 것처럼 보인다.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Auto Spin", meta = (ClampMin = "0.0", ClampMax = "30.0", ForceUnits = "s"))
	float AutoSpinResumeDelaySeconds = 4.f;

	/**
	 * 구름이 **지표에 대해** 도는 속도(도/초).
	 *
	 * 지구 자전이 자전각을 줄이는 방향이므로, 반대로 흐르려면 이 값은 **양수**다.
	 * 지표보다 느리게 반대로 흐르면 대기가 도는 것처럼 보인다.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Auto Spin", meta = (ClampMin = "-60.0", ClampMax = "60.0", ForceUnits = "deg/s"))
	float CloudRelativeSpinDegreesPerSecond = 0.6f;

	// --- 조작 ---

	/** 마우스 1픽셀당 회전량(도). 화면 크기와 무관하게 같은 감각이 되도록 픽셀 기준이다. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Input", meta = (ClampMin = "0.01", ClampMax = "2.0"))
	float DragDegreesPerPixel = 0.25f;

	/**
	 * 기울기 한계(도). 극점을 넘겨 지구가 뒤집히면 되돌릴 방법이 없다.
	 * 80도면 남북 양 극단의 핀까지 보이면서도 자전축이 화면과 나란해지지는 않는다.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Input", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float MaxTiltDegrees = 80.f;

	// --- 전환 시간 ---

	/**
	 * 핀을 고르면 그 국가를 정면으로 돌리는 시간(초).
	 * ★ **거리와 무관하게 같은 시간**이다. 거리에 비례시키면 가까운 나라는 튀고 먼 나라는 늘어진다.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Transitions", meta = (ClampMin = "0.0", ClampMax = "5.0", ForceUnits = "s"))
	float FocusRotationSeconds = 0.8f;

	/**
	 * `새 축제 시작` 에서 카메라가 지구로 다가가는 시간(초). **U3-c 에서 쓴다.**
	 * 로딩 화면을 만들지 않고 이 연출로 대체한다.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Transitions", meta = (ClampMin = "0.0", ClampMax = "5.0", ForceUnits = "s"))
	float GlobeApproachSeconds = 1.f;

	/** 지구본 → 국가 지도 전환 시간(초). **U3-b 에서 쓴다.** */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Transitions", meta = (ClampMin = "0.0", ClampMax = "5.0", ForceUnits = "s"))
	float CountryMapTransitionSeconds = 0.6f;

	// --- 타이틀 인트로 ---

	/**
	 * 인트로 「아무 키」 구도에서 지구 지름이 화면 높이의 몇 배인가. 위 로고와 아래 안내 자리를 남긴다.
	 * 카메라 리그의 `IntroCamera` 를 비워 둘 때만 쓴다.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Title Intro", meta = (ClampMin = "0.2", ClampMax = "1.0"))
	float IntroGlobeScreenHeightFraction = 0.56f;

	/** 카메라가 지구 중심보다 반지름의 몇 배만큼 위를 본다. 양수면 지구가 화면에서 조금 내려간다. 글자가 없으니 0(정가운데). */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Title Intro", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float IntroGlobeAimOffset = 0.f;

	/** 영상의 흰 빛이 걷히며 지구가 살짝 다가와 자리 잡는 시간(초). 변신은 영상이 했으니 크게 당기지 않는다. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Title Intro", meta = (ClampMin = "0.0", ClampMax = "3.0", ForceUnits = "s"))
	float IntroRevealSeconds = 0.9f;

	/** 당기기 시작할 때 카메라 거리 배율. 1 이면 당기지 않는다. 영상 마지막 구도와 크게 다르지 않게 작게 둔다. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Title Intro", meta = (ClampMin = "1.0", ClampMax = "6.0"))
	float IntroRevealStartDistanceScale = 1.08f;

	/** 끝에서 살짝 지나쳤다 돌아오는 정도(ease-out-back 계수). 0 이면 넘치지 않는다. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Title Intro", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float IntroRevealOvershoot = 0.f;

	/** 「아무 키」 뒤 가운데 구도에서 타이틀 구도로 옮겨 가는 시간(초). 지구가 오른쪽으로 비켜 가는 움직임이다. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Title Intro", meta = (ClampMin = "0.0", ClampMax = "5.0", ForceUnits = "s"))
	float IntroToTitleSeconds = 1.4f;

	// --- 표시 ---

	/**
	 * 지구 위 국가 조형물(건물)과 선택 조명을 만든다.
	 * 2026-10-13 사용자 결정으로 기본은 끔 — 타이틀·국가 선택 어디에도 건물을 세우지 않고 메시도 읽지 않는다.
	 * 국가 위치(목록·영역 선택·카메라 접근)는 그대로다. 켜면 예전처럼 타이틀에 조형물이 서고,
	 * 국가 선택 화면에서는 영역 데이터가 없는 나라만 조형물이 보인다.
	 * 바꾼 값은 지구본이 핀을 다시 만들 때(레벨 시작·`FF.Atlas.Rebuild`) 반영된다.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Presentation")
	bool bShowLandmarks = false;

	/**
	 * 선택된 조형물이 커지는 배율. 확정 방향은 `10~15% 크게` 다.
	 * ★ **크기만으로 선택을 알리지 않는다** (AT-011). 조명과 화면 왼쪽 패널의 이름이 함께 바뀐다.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "1.0", ClampMax = "2.0"))
	float SelectedLandmarkScale = 1.12f;

	/** 정면은 1배, 가장자리는 이 배율로 부드럽게 작아진다. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "0.3", ClampMax = "1.0"))
	float LandmarkEdgeScale = 0.65f;

	/** 선택 표시는 정보 패널과 카메라가 주도하고 조명은 은은하게만 보조한다. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LandmarkSelectionLightScale = 0.12f;

	/** 선택은 조형물 확대보다 카메라 접근으로 강조한다. 0이면 접근하지 않는다. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "0.0", ClampMax = "0.2"))
	float CountryInspectionApproachFraction = 0.08f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float CountryInspectionSeconds = 0.65f;

	/**
	 * 핀이 지구 테두리에서 얼마나 안쪽으로 들어와야 나타나는가 (내적 여유).
	 *
	 * ★ 실루엣 기준 자체는 여기 있지 않다. 원근 카메라에서 보이는 것은 반구가 아니라
	 *   `반지름 / 카메라거리` 까지이므로 액터가 매 프레임 `FFF_AtlasGeo::HorizonMinDot` 으로
	 *   계산하고, **이 값을 거기에 더한다.** 기준을 상수로 두면 카메라가 가까워질 때
	 *   지구 테두리 바깥에서 핀이 튀어나온다.
	 *
	 * 0 이 아닌 이유는 테두리에 딱 붙은 핀이 **한 픽셀 단위로 깜빡이기** 때문이다.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float PinVisibleDotMargin = 0.03f;

	/**
	 * 조형물 꼭대기의 정확한 지평선 임계에 더하는 연출 여유.
	 * 0이 물리적으로 맞는 값이다. 음수면 메시를 조금 더 일찍 켜 팝을 줄이고, 양수면 늦춘다.
	 * UMG 핀과 선택 조명에는 적용하지 않는다 — 뒤편 입력·빛이 지구를 뚫고 나오면 안 된다.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "-0.5", ClampMax = "0.5"))
	float LandmarkVisibleDotMargin = 0.f;
};
