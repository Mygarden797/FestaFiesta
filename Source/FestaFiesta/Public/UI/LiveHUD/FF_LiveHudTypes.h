// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Time/FF_FestivalStage.h"
#include "FF_LiveHudTypes.generated.h"

struct FGeometry;

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
	Guests		UMETA(DisplayName = "현황"),
	Queues		UMETA(DisplayName = "대기열"),
	SNS			UMETA(DisplayName = "SNS"),
	Staff		UMETA(DisplayName = "스태프"),
	Density		UMETA(DisplayName = "밀집"),

	// === 아래 둘은 **마감·리포트에서만** 보인다 (2026-09-02 통합) ===
	//
	// 🔴 라이브 중에는 숨긴다. 하루가 끝나야 나오는 결과라 라이브에 두면 빈 표만 보이고,
	// 실제로 볼 곳(마감)이 따로 있는데 탭이 둘이 되면 어디서 보는 것인지 헷갈린다.

	Report		UMETA(DisplayName = "리포트"),
	Finale		UMETA(DisplayName = "피날레"),

	// 기존 WBP에 저장된 enum 값을 보존하기 위해 끝에 추가한다. 실제 배치는 시설 탭 다음이다.
	RevenueRanking UMETA(DisplayName = "매출 순위")
};

/**
 * 라이브 패널 하나가 열릴 자리. **탭 하나 = 한 줄**이다.
 *
 * === 2026-08-28 정정 — 준비 단계와 같은 자리에서 연다 ===
 * 원래 계획은 `전부 왼쪽` 이었다 (운영 중에는 월드를 계속 봐야 한다는 이유). 실기에서 보니
 * **같은 동작인데 자리가 다른 것**이 더 걸렸다 — 셋업에서 탭을 누르면 버튼 위 가운데에 뜨는데
 * 라이브만 왼쪽에서 나왔다. 화면이 바뀌었다고 조작 규칙까지 바뀌면 안 된다.
 *
 * 그래서 방향을 `EFF_LivePanelDock` 으로 두고 기본을 `Center` 로 한다. 월드를 계속 봐야 하는
 * 탭이 나오면 그 줄만 `Left` 로 뺀다 — 준비 단계에서 `배치` 만 `Bottom` 인 것과 같은 기준이다.
 *
 * ★ 가운데에 열려도 **모달이 아니다.** 시간은 계속 흐르고 하단 바도 위에 남는다 (AGENTS.md 규칙 1).
 *
 * 좌표 기준은 1920x1080 이고 화면 배율은 엔진이 처리한다 (`UIScaleRule=ShortestSide`).
 */
/**
 * 라이브 패널이 열리는 쪽.
 *
 * === 2026-08-28 결정 — 준비 단계와 같은 자리에서 연다 ===
 * 처음에는 왼쪽 고정이었다. 실기에서 보니 **같은 동작인데 자리가 다른 것**이 이상했다 —
 * 셋업에서 탭을 누르면 버튼 위 가운데에 뜨는데 라이브만 왼쪽에서 나왔다.
 * 화면이 바뀌었다고 조작 규칙까지 바뀌면 안 된다.
 *
 * 그래서 기본은 `Center` 다. 다만 **월드를 봐야 하는 탭은 `Left` 로 뺀다** — 준비 단계의
 * `배치` 를 `Bottom` 으로 둔 것과 같은 기준이다 (`EFF_PrepPanelDock` 주석의 표).
 * 지금은 `운영` 하나뿐이라 전부 기본값을 쓴다.
 *
 * 🔴 **어느 값이든 크기는 명시한다.** 늘어난 축으로 두면 높이를 부모가 정하는데,
 * HUD 루트가 화면 높이를 못 받고 있어서 음수 높이가 나온 적이 있다 (2026-08-28).
 */
UENUM(BlueprintType)
enum class EFF_LivePanelDock : uint8
{
	/** 라이브 7패널은 화면 중앙. 리포트·피날레는 기존 하단 도킹을 유지한다. */
	Center	UMETA(DisplayName = "가운데"),

	/** 왼쪽. 월드를 계속 봐야 하는 탭용 — 화면 가운데를 비워 둔다. */
	Left	UMETA(DisplayName = "왼쪽"),

	/** 오른쪽. */
	Right	UMETA(DisplayName = "오른쪽"),

	/** 코드가 자리를 건드리지 않는다. 디자이너에서 잡은 위치를 그대로 쓴다. */
	Designer	UMETA(DisplayName = "디자이너 지정")
};

USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_LivePanelLayout
{
	GENERATED_BODY()

	/** 어느 쪽에 열지. 기본은 준비 단계와 같은 `가운데` 다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Panel Layout")
	EFF_LivePanelDock Dock = EFF_LivePanelDock::Center;

	/** 패널 폭(px). 0 이하면 패널이 사라지므로 그때는 기본값으로 되돌린다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Panel Layout")
	float Width = 520.f;

	/**
	 * 패널 높이(px). 라이브 7패널의 Center는 화면 중앙에서 상하로 자란다.
	 * 작은 창에서는 표시만 축소하고 이 저장값은 보존한다.
	 *
	 * === 🔴 왜 "화면 높이에서 빼서" 쓰지 않나 (2026-08-28 실측으로 판명) ===
	 * 처음에는 세로를 늘어난 축(`Anchors.Y = 0~1`)으로 두고 위아래 여백만 줬다. 그러면
	 * 높이를 **부모가** 정하는데, HUD 루트가 화면 높이를 못 받고 자기 내용 크기(약 70px)만
	 * 받고 있었다. 결과는 높이 `-12` 였고 화면에는 닫기 버튼만 한 흰 상자가 떴다.
	 *
	 * 준비 단계 패널이 멀쩡했던 것은 실제로 쓰는 도킹(`Bottom`·`Center`)이 전부
	 * **점 앵커 + 명시 크기**여서 부모 높이를 안 보기 때문이다. `Right` 를 썼다면 같이 깨졌다.
	 *
	 * ⚠️ HUD 루트가 화면을 못 채우는 것 자체는 **따로 남은 문제**다. 여기서는 그것에
	 * 의존하지 않게만 해 둔다 — `docs/memory/ui-session-handoff.md` 참고.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Panel Layout")
	float Height = 620.f;

	/**
	 * 기준점에서 미는 양(px). 양수 X 는 오른쪽, 양수 Y 는 아래.
	 *
	 * 기준점은 `Dock` 이 정한다 — 라이브 7패널의 Center는 실제 화면 중앙,
	 * 리포트·피날레의 Center는 기존 하단 기준, Left·Right는 그쪽 모서리.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Panel Layout")
	FVector2D Offset = FVector2D::ZeroVector;

	/** 실제 화면 중앙을 사용하는 라이브 정보 패널만 고른다. Designer/좌우 도킹은 유지한다. */
	bool UsesViewportCenter(EFF_LiveTab Tab) const;

	/**
	 * 창/DPI를 포함한 viewport 좌표를 호스트 Canvas로 변환한다.
	 * 작은 창에서는 표시 크기·위치만 보정하며 저장된 Width/Height/Offset은 수정하지 않는다.
	 * BottomReserve와 Margin은 Canvas 단위다. 아직 배치되지 않은 첫 프레임은 false.
	 */
	bool TryResolveCenteredLayout(const FGeometry& ViewportGeometry, const FGeometry& CanvasGeometry,
		float BottomReserve, float Margin, FVector2D& OutPosition, FVector2D& OutSize) const;
};

/**
 * 하단 바 오른쪽 CTA 의 상태.
 *
 * === 2026-09-02 통합 ===
 * 마감 HUD 를 없애고 이 바 하나로 합치면서 결산 쪽 CTA 판정을 여기로 옮겼다.
 * 화면이 단계마다 갈리면 같은 정보를 두 벌 그리게 된다.
 *
 * !! 비활성만 하지 않는다 !! (AT-011) 막힌 이유를 문구로 함께 띄운다.
 */
UENUM(BlueprintType)
enum class EFF_LiveCtaState : uint8
{
	/** CTA 를 아예 안 띄운다. 라이브 중에는 넘기는 버튼이 없다. */
	Hidden				UMETA(DisplayName = "숨김"),

	/** 다음 날로. 리포트 단계에서만. */
	ReadyNextDay		UMETA(DisplayName = "다음 날로"),

	/** 축제 마치기 → 나라 지도. 마지막 날은 마감이 마지막 화면이다. */
	ReadyFinishFestival	UMETA(DisplayName = "축제 마치기"),

	/** 피날레 연출을 건너뛴다. 날짜를 넘기는 것이 아니라 단계를 건너뛴다. */
	ReadyFinaleSkip		UMETA(DisplayName = "다음으로"),

	/** 마감이라 아직 못 넘긴다. 리포트 단계로 가야 한다. */
	BlockedNotReport	UMETA(DisplayName = "리포트 단계 아님")
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
	LockedMainProgram	UMETA(DisplayName = "메인 공연 잠금")

	// 🔴 `마지막 날 잠금` 을 뺐다 (2026-09-02 백종태 결정).
	//
	// 웹 프로토타입에 `마지막 날은 배속을 쓸 수 없습니다` 라는 규칙이 있어서 그대로
	// 옮겼는데, **소유자 코드에는 그런 잠금이 없다.** `SetTimeScale()` 은 값을 저장하고
	// `Tick` 의 Live 분기는 메인 공연 잠금만 본다 — 즉 UI 혼자 막고 있었다.
	//
	// 3일차는 가장 긴 날이라 오히려 배속이 필요하다는 판단이다.
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
	 * **시계가 실제로 흐르는** 라이브 단계인지. `Live` 하나뿐이다.
	 *
	 * 배속 판정에 쓴다 — 시간이 안 흐르는 단계에서 배속 버튼을 띄우면 눌러도 아무 일이 없다.
	 */
	static bool IsLiveStage(EFF_FestivalStage Stage);

	/**
	 * 라이브 HUD 를 **화면에 띄울** 단계인지. `IsLiveStage` 보다 넓다.
	 *
	 * 🔴 홍보 프리뷰 · 노을 전환 · 라이브 셋이다 (2026-08-27 백종태 결정).
	 * 셋업에서 `확정하고 Day 1 시작` 을 누른 순간부터 **축제는 이미 시작된 것**이라,
	 * 거기서 준비 단계 바가 다시 나오면 "확정했는데 왜 또 준비 화면인가" 가 된다.
	 * 노을 전환도 마찬가지로 라이브로 넘어가는 중이라 같은 HUD 를 유지한다.
	 *
	 * 마감(`Closing`)과 결산(`Report`)은 풀스크린 시트라 이 HUD 가 아니다 (U7).
	 */
	static bool IsLiveHudStage(EFF_FestivalStage Stage);

	/** 결과를 읽는 단계인가 — 마감·리포트. `리포트`·`피날레` 탭이 여기서만 열린다. */
	static bool IsResultStage(EFF_FestivalStage Stage);

	/** 피날레 연출 구간인가. 탭을 전부 숨기고 CTA 만 띄운다. */
	static bool IsFinaleShowStage(EFF_FestivalStage Stage) { return Stage == EFF_FestivalStage::Finale; }

	/**
	 * 이 탭을 지금 보여줄지.
	 *
	 * 라이브 탭 7종은 **탭끼리 갈리지 않는다** — HUD 가 뜨는 단계면 전부 보인다.
	 * 홍보 프리뷰·노을에서도 열 수 있게 두는 이유는 전부 **읽기 전용 정보**라서다.
	 * 미리 열어 둔다고 잘못될 것이 없고, 단계가 넘어갈 때 패널이 닫혔다 다시 열리면
	 * 오히려 눈이 매번 다시 찾는다.
	 *
	 * `피날레` 탭은 2026-10-06부터 어느 단계에서도 띄우지 않는다. 인자는 호환을 위해 남겼다.
	 */
	static bool IsTabVisible(EFF_LiveTab Tab, EFF_FestivalStage Stage, bool bHasPurchasedFinale);

	/**
	 * 라이브가 끝나 탭이 사라졌으면 선택을 푼다.
	 *
	 * 이게 없으면 마감으로 넘어갔을 때 **탭은 사라졌는데 패널만 남는다.**
	 * 닫을 수단이 화면에서 없어진다.
	 */
	static FFF_LiveTabSelection ClampSelectionToVisible(
		const FFF_LiveTabSelection& Current,
		EFF_FestivalStage Stage,
		bool bHasPurchasedFinale);

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
	 * 잠기는 곳은 **메인 공연 구간 하나뿐**이다. 마지막 날 잠금은 2026-09-02 에 뺐다 —
	 * 소유자 코드에 그런 잠금이 없어서 UI 혼자 막고 있었다 (`EFF_LiveSpeedState` 주석).
	 *
	 * @param bIsMainProgramLocked `UFF_FestivalStageSubsystem::IsMainProgramLocked()`
	 */
	static EFF_LiveSpeedState ResolveSpeedState(EFF_FestivalStage Stage, bool bIsMainProgramLocked);

	/**
	 * 하단 바 CTA 상태.
	 *
	 * 🔴 **순서가 뜻을 만든다** —
	 *   ① 결산·피날레 단계가 아니면 숨김 (라이브 중에는 넘기는 버튼이 없다)
	 *   ② 피날레 연출 → `다음으로` (건너뛸 수 있을 때만. 아니면 숨김)
	 *   ③ 마지막 날 → `축제 마치기`. **리포트 단계를 기다리지 않는다** —
	 *      3일차에는 리포트 단계가 없어서, 기다리게 하면 게임이 안 끝난다
	 *   ④ 리포트 단계가 아니면 막힘
	 *   ⑤ `다음 날로`
	 */
	static EFF_LiveCtaState ResolveCtaState(
		EFF_FestivalStage Stage, bool bIsFinalDay, bool bCanSkipStage);
};
