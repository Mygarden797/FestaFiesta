// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Time/FF_FestivalStage.h"
#include "FF_PrepHudTypes.generated.h"

struct FGeometry;
struct FFF_RequiredFacilitiesStatus;

/**
 * 준비 단계 하단 바의 탭 6종.
 *
 * 순서가 곧 화면 왼쪽부터의 순서다. `unreal-port-plan.md` U4 의 목록과 같게 유지한다.
 * **enum ordinal 에 의존하는 코드를 만들지 않는다** — 탭이 늘거나 순서가 바뀌면 조용히 틀린다.
 */
UENUM(BlueprintType)
enum class EFF_PrepHudTab : uint8
{
	Build		UMETA(DisplayName = "배치"),
	Program		UMETA(DisplayName = "프로그램"),
	Staff		UMETA(DisplayName = "스태프"),
	Promotion	UMETA(DisplayName = "홍보"),
	Finale		UMETA(DisplayName = "피날레"),
	Check		UMETA(DisplayName = "점검")
};

/**
 * 패널이 열리는 쪽.
 *
 * === 2026-08-26 결정 — 탭 성격에 따라 다르게 연다 ===
 * 초안은 `전부 왼쪽` 이었으나 실제 화면을 보고 바꿨다. 기준은 **그 탭이 월드를 만지는가**다.
 *
 * | 성격 | 예 | 어디에 | 왜 |
 * | --- | --- | --- | --- |
 * | 월드를 만진다 | 배치 | `Bottom` | 지면이 보여야 시설을 놓을 수 있다 |
 * | 읽고 결정한다 | 프로그램·인력·홍보·피날레·점검 | `Center` | 표·카드가 가로로 넓고, 읽는 동안 월드는 필요 없다 |
 *
 * 탭마다 다르게 두는 것이 임의로 섞는 것과 다른 이유는 **규칙 한 줄로 설명되기 때문**이다.
 * 규칙이 없으면 어디서 열릴지 예측이 안 된다.
 *
 * ★ `Center` 도 **모달이 아니다.** 시간은 계속 흐르고 하단 바는 위에 남는다 (AGENTS.md 규칙 1).
 */
UENUM(BlueprintType)
enum class EFF_PrepPanelDock : uint8
{
	Left	UMETA(DisplayName = "왼쪽"),
	Right	UMETA(DisplayName = "오른쪽"),

	/** 하단 바 바로 위. 월드를 만지는 탭용 — 지면이 가려지지 않는다. */
	Bottom	UMETA(DisplayName = "하단"),

	/** 화면 중앙에 크게. 표·카드를 읽는 탭용. 하단 바는 가리지 않는다. */
	Center	UMETA(DisplayName = "중앙"),

	/**
	 * 코드가 자리를 건드리지 않는다. **디자이너에서 잡은 위치를 그대로 쓴다.**
	 *
	 * 다른 값은 패널을 열 때마다 앵커·오프셋을 코드가 다시 쓰기 때문에 디자이너에서 끌어 옮긴
	 * 위치가 사라진다. 손으로 자리를 잡고 싶을 때 이걸 고른다 — 대신 **탭마다 다른 자리로
	 * 열 수는 없다** (호스트가 하나뿐이다).
	 */
	Designer	UMETA(DisplayName = "디자이너 지정")
};

/**
 * 탭 하나가 열릴 자리와 크기. **탭 하나 = 한 줄**이다.
 *
 * === 🔴 도킹 방향마다 쓰이는 성분이 다르다 ===
 * 늘어나는 축은 화면이 정하고, 점 앵커 축만 이 값이 정한다. 이걸 모르면 값을 바꿨는데
 * 아무 일도 안 일어나는 것처럼 보인다.
 *
 * | 방향 | `Size` | `Offset` |
 * | --- | --- | --- |
 * | `중앙` | X=폭 · Y=높이 **둘 다 쓴다** | 정중앙에서 미는 양. 둘 다 쓴다 |
 * | `하단` | **Y=높이만.** 가로는 화면 폭에서 여백을 뺀 값 | **Y만.** 바 위로 더 띄우는 양 |
 * | `왼쪽`·`오른쪽` | **X=폭만.** 세로는 화면 높이에서 바와 여백을 뺀 값 | **X만.** 좌우 미세 조정 |
 * | `디자이너 지정` | 안 쓴다 | 안 쓴다 |
 *
 * 좌표 기준은 1920x1080 이고 화면 배율은 엔진이 처리한다 (`UIScaleRule=ShortestSide`).
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_PrepPanelLayout
{
	GENERATED_BODY()

	/** 어디에 열지. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prep Panel Layout")
	EFF_PrepPanelDock Dock = EFF_PrepPanelDock::Center;

	/**
	 * 크기(Slate 단위, DPI 적용 전). 방향에 따라 쓰이는 성분이 다르다 — 구조체 주석의 표를 본다.
	 * 0 이하면 패널이 사라지므로 그때는 기본 레이아웃 값으로 되돌린다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prep Panel Layout")
	FVector2D Size = FVector2D(1160.0, 640.0);

	/**
	 * 기준 위치에서 미는 양(Slate 단위, DPI 적용 전). 양수 X 는 오른쪽, 양수 Y 는 아래.
	 * 프로그램·인력·홍보·점검의 Center에서는 (0,0)이 하단 HUD를 포함한 게임 화면 정중앙이다.
	 *
	 * ★ 게임에서 패널을 끌어 옮기면 **이 값이 바뀐다.** 마음에 드는 자리를 찾은 뒤
	 *   `FF.PrepHud.Layout` 으로 숫자를 확인해 WBP 기본값에 적으면 확정된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prep Panel Layout")
	FVector2D Offset = FVector2D::ZeroVector;

	/** 화면 중앙을 Canvas 0.5 앵커의 위치 오프셋으로 바꾼다. 배치 전 geometry는 false를 돌린다. */
	bool TryResolveCenteredPosition(const FGeometry& ViewportGeometry, const FGeometry& CanvasGeometry,
		FVector2D& OutPosition) const;
};

/**
 * CTA 버튼의 상태.
 *
 * !! 비활성만 하지 않는다 !! (AT-011) 흐려짐만으로는 왜 못 누르는지 알 수 없으므로
 * 각 상태마다 화면에 띄울 이유 문구가 하나씩 붙는다.
 */
UENUM(BlueprintType)
enum class EFF_PrepCtaState : uint8
{
	/** 이 단계에는 CTA 가 없다. 버튼째 숨긴다. */
	Hidden				UMETA(DisplayName = "없음"),

	/** 누를 수 있다. */
	Ready				UMETA(DisplayName = "가능"),

	/** 시설이 하나도 없다. 배치 탭으로 안내한다. */
	BlockedNoFacility	UMETA(DisplayName = "시설 없음"),

	/**
	 * 단계 표(`DT_DaySequence`)가 이 단계를 넘길 수 없다고 한다.
	 * 남은 시간이 지나면 자동으로 넘어간다는 것을 문구로 알린다.
	 */
	BlockedStageLocked	UMETA(DisplayName = "단계 잠김")
};

/**
 * 지금 열려 있는 탭.
 *
 * === enum 에 `None` 을 넣지 않은 이유 ===
 * `TMap<EFF_PrepHudTab, 패널 클래스>` 같은 표에 의미 없는 키가 생기고, 아이콘·라벨 해석에서도
 * 예외 하나를 계속 달고 다니게 된다. "선택 없음"은 상태이지 탭이 아니다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_PrepTabSelection
{
	GENERATED_BODY()

	/** false 면 아무 패널도 열려 있지 않다. `Tab` 값은 의미가 없다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep")
	bool bHasSelection = false;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep")
	EFF_PrepHudTab Tab = EFF_PrepHudTab::Build;

	bool IsSelected(EFF_PrepHudTab InTab) const { return bHasSelection && Tab == InTab; }

	static FFF_PrepTabSelection None() { return FFF_PrepTabSelection(); }

	static FFF_PrepTabSelection Of(EFF_PrepHudTab InTab)
	{
		FFF_PrepTabSelection Result;
		Result.bHasSelection = true;
		Result.Tab = InTab;
		return Result;
	}

	bool operator==(const FFF_PrepTabSelection& Other) const
	{
		// 선택이 없으면 `Tab` 은 쓰레기값이므로 비교에 넣지 않는다.
		return bHasSelection == Other.bHasSelection && (!bHasSelection || Tab == Other.Tab);
	}

	bool operator!=(const FFF_PrepTabSelection& Other) const { return !(*this == Other); }
};

/**
 * 준비 단계 HUD 의 판단 규칙. **순수 함수만 둔다.**
 *
 * === 왜 위젯 밖으로 뺐나 ===
 * "홍보 탭이 셋업에서만 보인다" 같은 규칙은 **에디터를 열지 않으면 확인할 수 없는 자리**에
 * 두면 안 된다. 여기 있으면 자동화 테스트가 못으로 박는다. U3-a 에서 좌표 계산을
 * `FFF_AtlasGeo` 로 뺀 것과 같은 이유다 — 그때 화면에서만 틀리는 버그를 실제로 잡았다.
 *
 * 월드 상태를 읽지 않는다. 필요한 값은 전부 인자로 받는다.
 */
struct FESTAFIESTA_API FFF_PrepHudRules
{
	/** 소유자의 미충족 행을 안내 건수로 센다. 설치 후 배정은 같은 시설의 한 준비 항목이다. */
	static int32 CountPendingRequiredTasks(const FFF_RequiredFacilitiesStatus& Status);

	/** 탭 선택/열람과 무관하게 실제 미충족 상태가 있는 동안 표시한다. */
	static bool NeedsRequiredAttention(EFF_PrepHudTab Tab, EFF_FestivalStage Stage,
		const FFF_RequiredFacilitiesStatus& Status);

	/**
	 * 준비 단계인지. 셋업 · 홍보 · 개장 준비 셋이다.
	 *
	 * `SunsetTransition`(노을 전환)은 **준비 단계가 아니다.** 연출용 전환이라 조작할 것이 없다.
	 * Stage 가 8종이라 7종으로 적힌 문서와 어긋나기 쉬운 자리다.
	 */
	static bool IsPrepStage(EFF_FestivalStage Stage);

	/**
	 * 이 탭을 지금 보여줄지.
	 *
	 * - 홍보 : 셋업 단계에서만. 홍보를 산 뒤 개장 준비에서 또 파는 화면이 되면 안 된다
	 * - 피날레 : 마지막 날 개장 준비에서만. 축제당 1회 구매라 그 전에 열면 오해를 만든다
	 * - 나머지 : 준비 단계면 언제나
	 *
	 * `bIsFinalDay` 를 인자로 받는 이유는 **UI 가 총 일수를 알면 안 되기 때문**이다.
	 * `Day == 3` 을 박아 두면 2일짜리 축제가 추가될 때 피날레 탭이 영영 안 뜬다.
	 * 값의 원천은 `UFF_FestivalViewModelSubsystem::IsFinalDay()` 이고, 그것은 Stage 시스템의
	 * 판단을 그대로 중계한다.
	 */
	static bool IsTabVisible(EFF_PrepHudTab Tab, EFF_FestivalStage Stage, bool bIsFinalDay);

	/**
	 * 단계가 바뀌어 지금 열린 탭이 사라졌으면 선택을 푼다.
	 *
	 * 이게 없으면 셋업에서 홍보 패널을 열어 둔 채 단계가 넘어갔을 때 **탭은 사라졌는데 패널만
	 * 남는다.** 닫을 수단이 화면에서 없어진다.
	 */
	static FFF_PrepTabSelection ClampSelectionToVisible(
		const FFF_PrepTabSelection& Current,
		EFF_FestivalStage Stage,
		bool bIsFinalDay);

	/**
	 * 탭을 눌렀을 때의 다음 선택. **패널 배타 규칙의 본체다.**
	 *
	 * - 다른 탭을 누르면 그 탭으로 바뀐다 (앞의 패널은 닫힌다)
	 * - 같은 탭을 다시 누르면 닫힌다
	 */
	static FFF_PrepTabSelection ToggleTab(const FFF_PrepTabSelection& Current, EFF_PrepHudTab Clicked);

	/**
	 * CTA 버튼 상태.
	 *
	 * @param bHasCtaLabel			이 단계에 배선된 CTA 문구가 있는지. 없으면 버튼째 숨긴다
	 * @param bStageAdvanceAvailable `UFF_FestivalViewModelSubsystem::CanSkipCurrentStage()`
	 * @param FacilityCount			설치된 시설 수. 모르면 `INDEX_NONE` 을 넣는다
	 * @param bRequireFacility		시설 0개일 때 막을지
	 *
	 * `FacilityCount` 가 `INDEX_NONE`(= 아직 못 읽음)이면 **막지 않는다.**
	 * 읽지 못한 것을 "0개"로 취급하면 시설을 놓고도 시작할 수 없는 화면이 된다.
	 */
	static EFF_PrepCtaState ResolveCtaState(
		EFF_FestivalStage Stage,
		bool bHasCtaLabel,
		bool bStageAdvanceAvailable,
		int32 FacilityCount,
		bool bRequireFacility);
};
