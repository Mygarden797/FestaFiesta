// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Time/FF_FestivalStage.h"
#include "FF_LiveHudTypes.generated.h"

/**
 * 라이브 운영 HUD 의 탭 7종.
 *
 * 순서가 곧 화면 왼쪽부터의 순서다. `unreal-port-plan.md` U6 의 목록과 같게 유지한다.
 * **enum ordinal 에 의존하는 코드를 만들지 않는다** — 탭이 늘거나 순서가 바뀌면 조용히 틀린다.
 *
 * === 기존 `EFF_LiveHudTab`(9종)과 다른 것이다 ===
 * 그쪽은 2026-08-12 목업 기준이고 `사건`·`밀집도` 를 탭으로 갖고 있다. 이 둘은 탭이 아니다 —
 *   - 사건 : **우측 상단 상시 트레이.** 탭에 넣으면 열어야만 보이는데, 놓치면 안 되는 정보다
 *   - 밀집도 : **독립 온/오프 토글.** 다른 탭을 보면서도 켜 둘 수 있어야 한다
 * 기존 위젯 3종(`UFF_LiveHudMenuBar`·`StatusBar`·`TabButton`)은 건드리지 않고 남겨 둔다
 * (2026-08-27 백종태 지시). 새 HUD 가 자리를 잡으면 그때 정리한다.
 */
UENUM(BlueprintType)
enum class EFF_LiveTab : uint8
{
	Operations	UMETA(DisplayName = "운영"),
	Facilities	UMETA(DisplayName = "시설"),
	Guests		UMETA(DisplayName = "관객"),
	Queues		UMETA(DisplayName = "대기열"),
	SNS			UMETA(DisplayName = "SNS"),
	Report		UMETA(DisplayName = "리포트"),
	Staff		UMETA(DisplayName = "스태프")
};

/**
 * 라이브 패널 하나가 열릴 자리. **탭 하나 = 한 줄**이다.
 *
 * === 준비 단계와 달리 방향이 하나다 ===
 * 라이브 패널은 **전부 왼쪽에 연다** (U6 계획 · 리포트도 왼쪽이다). 운영 중에는 월드를 계속
 * 봐야 해서 화면 가운데를 덮으면 안 되고, 자리가 탭마다 다르면 눈이 매번 다시 찾아야 한다.
 * 그래서 준비 단계의 `EFF_PrepPanelDock` 같은 방향 값을 두지 않았다 — 폭만 정하면 된다.
 *
 * ★ 왼쪽에 열려도 **모달이 아니다.** 시간은 계속 흐르고 하단 바도 위에 남는다 (AGENTS.md 규칙 1).
 *
 * 좌표 기준은 1920x1080 이고 화면 배율은 엔진이 처리한다 (`UIScaleRule=ShortestSide`).
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_LivePanelLayout
{
	GENERATED_BODY()

	/**
	 * 패널 폭(px). 세로는 화면 높이에서 하단 바와 여백을 뺀 값이라 여기서 정하지 않는다.
	 * 0 이하면 패널이 사라지므로 그때는 기본값으로 되돌린다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Panel Layout")
	float Width = 520.f;

	/**
	 * 왼쪽 기준에서 미는 양(px). 양수 X 는 오른쪽, 양수 Y 는 아래.
	 *
	 * 🔴 **`X` 만 자리를 옮긴다.** 세로는 화면이 정하므로 `Y` 를 키우면 위아래 여백이
	 * 같이 줄어드는 것이 아니라 패널이 아래로 밀리고 그만큼 잘린다. 준비 단계에서 같은
	 * 함정을 밟았다 — 늘어나는 축은 값이 아니라 화면이 정한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Panel Layout")
	FVector2D Offset = FVector2D::ZeroVector;
};

/**
 * 배속 버튼의 상태.
 *
 * !! 비활성만 하지 않는다 !! (AT-011) 흐려짐만으로는 왜 못 누르는지 알 수 없으므로
 * 막힌 상태마다 화면에 띄울 이유 문구가 하나씩 붙는다.
 */
UENUM(BlueprintType)
enum class EFF_LiveSpeedState : uint8
{
	/** 라이브가 아니다. 배속 줄째 숨긴다. */
	Hidden				UMETA(DisplayName = "없음"),

	/** 바꿀 수 있다. */
	Ready				UMETA(DisplayName = "가능"),

	/**
	 * 메인 공연 구간이라 1배속으로 잠겼다.
	 * 판정은 `UFF_FestivalStageSubsystem::IsMainProgramLocked()` 가 소유한다 — UI 가 세지 않는다.
	 */
	LockedMainProgram	UMETA(DisplayName = "메인 공연 잠금"),

	/** 마지막 날은 배속을 막는다. 결말을 빨리 감기로 넘기지 않게 한다. */
	LockedFinalDay		UMETA(DisplayName = "마지막 날 잠금")
};

/**
 * 지금 열려 있는 탭.
 *
 * === enum 에 `None` 을 넣지 않은 이유 ===
 * `TMap<EFF_LiveTab, 패널 클래스>` 같은 표에 의미 없는 키가 생기고, 아이콘·라벨 해석에서도
 * 예외 하나를 계속 달고 다니게 된다. "선택 없음"은 상태이지 탭이 아니다.
 * 준비 단계 `FFF_PrepTabSelection` 과 같은 판단이다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_LiveTabSelection
{
	GENERATED_BODY()

	/** false 면 아무 패널도 열려 있지 않다. `Tab` 값은 의미가 없다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	bool bHasSelection = false;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	EFF_LiveTab Tab = EFF_LiveTab::Operations;

	bool IsSelected(EFF_LiveTab InTab) const { return bHasSelection && Tab == InTab; }

	static FFF_LiveTabSelection None() { return FFF_LiveTabSelection(); }

	static FFF_LiveTabSelection Of(EFF_LiveTab InTab)
	{
		FFF_LiveTabSelection Result;
		Result.bHasSelection = true;
		Result.Tab = InTab;
		return Result;
	}

	bool operator==(const FFF_LiveTabSelection& Other) const
	{
		// 선택이 없으면 `Tab` 은 쓰레기값이므로 비교에 넣지 않는다.
		return bHasSelection == Other.bHasSelection && (!bHasSelection || Tab == Other.Tab);
	}

	bool operator!=(const FFF_LiveTabSelection& Other) const { return !(*this == Other); }
};

/**
 * 라이브 HUD 의 판단 규칙. **순수 함수만 둔다.**
 *
 * === 왜 위젯 밖으로 뺐나 ===
 * "마지막 날에는 배속을 막는다" 같은 규칙은 **3일차까지 실제로 플레이해야** 확인되는 자리다.
 * 여기 있으면 자동화 테스트가 못으로 박는다. `FFF_PrepHudRules` 와 같은 이유다.
 *
 * 월드 상태를 읽지 않는다. 필요한 값은 전부 인자로 받는다.
 */
struct FESTAFIESTA_API FFF_LiveHudRules
{
	/**
	 * 라이브 운영 HUD 를 띄울 단계인지.
	 *
	 * `Live` 하나뿐이다. 마감(`Closing`)과 결산(`Report`)은 풀스크린 시트라 이 HUD 가 아니고
	 * (U7), 노을 전환(`SunsetTransition`)은 20초짜리 순수 연출이라 HUD 를 띄우지 않는다
	 * (2026-08-27 결정 — 그 단계는 `StageScreens` 에서 아예 뺐다).
	 */
	static bool IsLiveStage(EFF_FestivalStage Stage);

	/**
	 * 이 탭을 지금 보여줄지.
	 *
	 * 라이브 탭 7종은 **단계로 갈리지 않는다** — 라이브면 전부 보인다. 준비 단계와 다른 점이다
	 * (그쪽은 홍보·피날레가 단계로 갈렸다). 그래도 함수를 두는 이유는, 나중에 조건이 붙을 때
	 * 위젯이 아니라 여기가 바뀌게 하기 위해서다.
	 */
	static bool IsTabVisible(EFF_LiveTab Tab, EFF_FestivalStage Stage);

	/**
	 * 라이브가 끝나 탭이 사라졌으면 선택을 푼다.
	 *
	 * 이게 없으면 마감으로 넘어갔을 때 **탭은 사라졌는데 패널만 남는다.**
	 * 닫을 수단이 화면에서 없어진다.
	 */
	static FFF_LiveTabSelection ClampSelectionToVisible(
		const FFF_LiveTabSelection& Current,
		EFF_FestivalStage Stage);

	/**
	 * 탭을 눌렀을 때의 다음 선택. **패널 배타 규칙의 본체다.**
	 *
	 * - 다른 탭을 누르면 그 탭으로 바뀐다 (앞의 패널은 닫힌다)
	 * - 같은 탭을 다시 누르면 닫힌다
	 */
	static FFF_LiveTabSelection ToggleTab(const FFF_LiveTabSelection& Current, EFF_LiveTab Clicked);

	/**
	 * 배속 버튼 상태.
	 *
	 * ★ **메인 공연 잠금을 마지막 날보다 먼저 본다.** 둘 다 걸린 3일차 공연 중에
	 *   `마지막 날이라 못 바꿉니다` 라고 하면, 공연이 끝나면 풀린다는 사실을 못 알린다.
	 *   더 빨리 풀리는 이유를 먼저 알려 준다.
	 *
	 * @param bIsMainProgramLocked `UFF_FestivalStageSubsystem::IsMainProgramLocked()`
	 * @param bIsFinalDay          `IsFinalDay()`. UI 가 총 일수를 알지 못하게 중계값을 받는다
	 */
	static EFF_LiveSpeedState ResolveSpeedState(
		EFF_FestivalStage Stage,
		bool bIsMainProgramLocked,
		bool bIsFinalDay);
};
