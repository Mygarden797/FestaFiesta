// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FF_UIPalette.generated.h"

/**
 * 인게임 화면의 색 (LightTeal).
 *
 * 쓰는 곳: 셋업 · 홍보 · 개장 준비 · 라이브 · 마감 · 결산 · 피날레.
 *
 * === 값의 원천 ===
 * `docs/ui/prototypes/full-cycle/styles/tokens.css` 한 곳이다. 필드마다 대응하는 CSS 변수
 * 이름을 적어 두었으니 색을 바꿀 때 양쪽을 같이 본다. 그 파일의 원천은 다시
 * `SourceArt/UI/LiveHUD/LightTealParts_v1/style-tokens.json`이다.
 *
 * === 반드시 FromSRGBColor를 거친다 ===
 * 헥스 비율을 `FLinearColor`에 직접 넣으면 감마가 틀려 화면이 밝게 뜬다.
 * `FF_UIMetricBandSettings`가 실제로 그 상태였고 2026-08-20에 고쳤다.
 * 알파는 `FColor`의 4번째 인자로 넘긴다 — `FromSRGBColor`는 RGB만 감마 변환하고
 * 알파는 `A/255`로 선형 통과시킨다. 생성 뒤에 `.A`를 따로 대입하면 한쪽만 고치는 실수가 난다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_UIPaletteInGame
{
	GENERATED_BODY()

	// --- 기본 팔레트 (style-tokens.json palette와 1:1) ---

	/** `--white`. 패널 최상단·선택된 탭 글자. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Base")
	FLinearColor White = FLinearColor::FromSRGBColor(FColor(0xFF, 0xFF, 0xFF));

	/** `--teal`. 패널 머리 바. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Base")
	FLinearColor Teal = FLinearColor::FromSRGBColor(FColor(0x52, 0x7C, 0x6B));

	/** `--teal-deep`. 포커스 링 외곽. 전역 RenderFocusRule=Never라 직접 그린다 (C-024). */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Base")
	FLinearColor TealDeep = FLinearColor::FromSRGBColor(FColor(0x31, 0x57, 0x47));

	/** `--teal-soft`. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Base")
	FLinearColor TealSoft = FLinearColor::FromSRGBColor(FColor(0xB4, 0xC8, 0xBB));

	/** `--beige`. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Base")
	FLinearColor Beige = FLinearColor::FromSRGBColor(FColor(0xE7, 0xE3, 0xE3));

	// --- 글자 ---

	/** `--ink`. 본문 글자. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Text")
	FLinearColor Ink = FLinearColor::FromSRGBColor(FColor(0x24, 0x33, 0x2E));

	/** `--ink-muted`. 보조 문구·단위. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Text")
	FLinearColor InkMuted = FLinearColor::FromSRGBColor(FColor(0x6F, 0x7B, 0x75));

	/** `--ink-faint`. 비활성 글자. **색만으로 비활성을 알리지 않는다** (AT-011). */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Text")
	FLinearColor InkFaint = FLinearColor::FromSRGBColor(FColor(0x94, 0xA0, 0x9A));

	// --- 면과 선 ---

	/** `--paper`. 패널 본문 배경. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Surface")
	FLinearColor Paper = FLinearColor::FromSRGBColor(FColor(0xF7, 0xF8, 0xF6));

	/** `--paper-sunken`. 표 줄무늬·비활성 트랙. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Surface")
	FLinearColor PaperSunken = FLinearColor::FromSRGBColor(FColor(0xEE, 0xF2, 0xEF));

	/** `--status-bg`. HUD 하단 상태행 (58px 중 아래 29px). */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Surface")
	FLinearColor StatusBar = FLinearColor::FromSRGBColor(FColor(0xE8, 0xED, 0xE9));

	/** `--hover`. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Surface")
	FLinearColor Hover = FLinearColor::FromSRGBColor(FColor(0xE9, 0xF0, 0xEC));

	/** `--line`. 경계선. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Surface")
	FLinearColor Line = FLinearColor::FromSRGBColor(FColor(0xCC, 0xD5, 0xCF));

	/** `--line-strong`. 표 머리·구분이 강해야 하는 선. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Surface")
	FLinearColor LineStrong = FLinearColor::FromSRGBColor(FColor(0x9D, 0xB1, 0xA5));

	/**
	 * `--scrim`. 화면을 덮는 어둠.
	 * **시간이 멈추는 화면에서만 쓴다** — MorningCheck과 ESC 둘뿐이다.
	 * 비모달 패널에 스크림을 깔면 멈춘 것처럼 보인다.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Surface")
	FLinearColor Scrim = FLinearColor::FromSRGBColor(FColor(0x14, 0x22, 0x1D, 140));

	// --- 의미색 ---

	/** `--amber`. 주의. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Semantic")
	FLinearColor Amber = FLinearColor::FromSRGBColor(FColor(0xE3, 0x9A, 0x22));

	/** `--danger`. 위험·실패·지출. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Semantic")
	FLinearColor Danger = FLinearColor::FromSRGBColor(FColor(0xD7, 0x4E, 0x43));

	/** `--info`. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Semantic")
	FLinearColor Info = FLinearColor::FromSRGBColor(FColor(0x3D, 0x86, 0xB4));

	/** `--neutral`. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Semantic")
	FLinearColor Neutral = FLinearColor::FromSRGBColor(FColor(0x77, 0x7F, 0x7B));

	// --- 지표 구간 ---
	//
	// 구간 **경계값·라벨·아이콘**은 여기가 아니라 `UFF_UIMetricBandSettings`가 소유한다.
	// 여기 있는 것은 원색뿐이다. 두 곳에 경계값을 두면 조용히 갈라진다.
	// 값을 바꾸면 그쪽 생성자도 같이 맞춘다.

	/** `--band-good`. 70~100. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Metric Band")
	FLinearColor BandGood = FLinearColor::FromSRGBColor(FColor(0x3F, 0x8F, 0x6D));

	/** `--band-caution`. 40~69. 지금은 Amber와 같지만 갈라질 수 있어 따로 둔다. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Metric Band")
	FLinearColor BandCaution = FLinearColor::FromSRGBColor(FColor(0xE3, 0x9A, 0x22));

	/** `--band-bad`. 0~39. 지금은 Danger와 같지만 갈라질 수 있어 따로 둔다. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Metric Band")
	FLinearColor BandBad = FLinearColor::FromSRGBColor(FColor(0xD7, 0x4E, 0x43));

	// --- 증감 ---

	/** `--up`. 전일 대비 증가. **화살표 아이콘과 함께 쓴다** (AT-011). */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Trend")
	FLinearColor Up = FLinearColor::FromSRGBColor(FColor(0x2F, 0x7D, 0x5C));

	/** `--down`. 전일 대비 감소. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Trend")
	FLinearColor Down = FLinearColor::FromSRGBColor(FColor(0xD7, 0x4E, 0x43));
};

/**
 * 프론트엔드 화면의 색 (Glass Orbit).
 *
 * 쓰는 곳: 타이틀 · 플레이 허브 · 저장 기록 · 옵션 · Festival Atlas(지구본·국가 지도).
 *
 * === 왜 인게임과 다른가 ===
 * 2026-08-18에 **인게임을 LightTeal로 통일**하기로 정했지만 프론트엔드는 그 대상이 아니다.
 * 타이틀 배경이 어두운 지구본이고 `WBP_TitleMain_GlassOrbit`이 이미 그 정체성으로 구현·커밋돼
 * 있다 (`docs/ui/visual-direction.md` 4절). **프론트엔드를 밝게 바꾸려면 이 구조체만 고치면 된다.**
 *
 * === 이미 같은 값을 들고 있는 곳이 둘 있다 ===
 * `FF_CommonButtonBase.h`의 Normal/Highlighted/Disabled 콘텐츠 색과
 * `FF_LiveHudMenuBar.h`의 Normal/Selected 색이 `EditDefaultsOnly` 프로퍼티로 같은 값을 갖는다.
 * **그쪽을 이 구조체로 끌어오지 않는다** — WBP별 개별 조정이 사라진다.
 * 값이 어긋나지 않게 유지만 한다: `Text`=0xECF4FF · `Ink`=0x102B4E · `Dim`=0x8192A8.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_UIPaletteFrontend
{
	GENERATED_BODY()

	// --- 배경 ---

	/** `--go-void`. 가장 어두운 바닥. 지구 뒤 우주. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Backdrop")
	FLinearColor Void = FLinearColor::FromSRGBColor(FColor(0x06, 0x0C, 0x16));

	/** `--go-deep`. Void보다 한 단 밝은 면. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Backdrop")
	FLinearColor Deep = FLinearColor::FromSRGBColor(FColor(0x0B, 0x15, 0x26));

	// --- 유리 패널 ---

	/** `--go-glass`. 반투명 패널 면. 알파 0.62. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Glass")
	FLinearColor Glass = FLinearColor::FromSRGBColor(FColor(0x12, 0x20, 0x34, 158));

	/** `--go-glass-raised`. 떠 있는 패널·호버 상태. 알파 0.72. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Glass")
	FLinearColor GlassRaised = FLinearColor::FromSRGBColor(FColor(0x1C, 0x2E, 0x48, 184));

	/** `--go-edge`. 유리 테두리. 알파 0.16. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Glass")
	FLinearColor Edge = FLinearColor::FromSRGBColor(FColor(0xBE, 0xD7, 0xF5, 41));

	/** `--go-edge-strong`. 선택·포커스된 테두리. 알파 0.32. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Glass")
	FLinearColor EdgeStrong = FLinearColor::FromSRGBColor(FColor(0xBE, 0xD7, 0xF5, 82));

	// --- 글자 ---

	/** `--go-title`. 로고 · 화면 제목 · 저장 카드 제목. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Text")
	FLinearColor Title = FLinearColor::FromSRGBColor(FColor(0xF4, 0xF7, 0xFB));

	/** `--go-text`. 메뉴 문구. Normal과 Hovered가 같은 값이다 (규격 6절). */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Text")
	FLinearColor Text = FLinearColor::FromSRGBColor(FColor(0xEC, 0xF4, 0xFF));

	/** `--go-sub`. 보조 정보. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Text")
	FLinearColor Sub = FLinearColor::FromSRGBColor(FColor(0xBF, 0xD0, 0xE5));

	/** `--go-dim`. 비활성. **자물쇠 아이콘·문구와 함께 쓴다** (AT-011). */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Text")
	FLinearColor Dim = FLinearColor::FromSRGBColor(FColor(0x81, 0x92, 0xA8));

	/** `--go-ink`. 지속 선택 상태(흰 배경) 위의 글자. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Text")
	FLinearColor Ink = FLinearColor::FromSRGBColor(FColor(0x10, 0x2B, 0x4E));
};

/**
 * 전환 길이 3단계. 위젯이 숫자를 직접 고르지 않고 이 중 하나를 지정한다.
 *
 * 실제 초는 `UFF_UIAccessibilitySubsystem::GetMotionDuration()` 에서 받는다.
 * **팔레트를 직접 읽지 않는다** — `접근성 > 움직임 감소` 가 켜지면 0 을 돌려줘야 하고,
 * 그 판정을 위젯마다 두면 어디 하나가 빠진다.
 */
UENUM(BlueprintType)
enum class EFF_UIMotionSpeed : uint8
{
	/** 호버·눌림 같은 즉각 반응. */
	Fast	UMETA(DisplayName = "빠름"),

	/** 패널 열기·닫기. */
	Base	UMETA(DisplayName = "기본"),

	/** 화면 전환. */
	Screen	UMETA(DisplayName = "화면 전환")
};

/**
 * 전환 애니메이션 길이. **초 단위다.**
 *
 * tokens.css는 ms(`--dur-fast: 110ms`)이고 UE는 초다. 변환을 한 곳에서만 하기 위해
 * 여기서 이미 초로 적어 둔다. 위젯이 ms를 받아 나누는 코드를 만들지 않는다.
 *
 * === 왜 색과 같이 두는가 ===
 * 옵션의 `접근성 > 움직임 감소`가 실제로 동작해야 하고(U2), 그러려면 코드가 읽을 수 있는
 * 한 곳이 필요하다. 간격·반경·타입 스케일은 여기 넣지 않는다 — 그건 에디터 마감 영역이고,
 * 코드에 두면 손으로 조절한 값과 갈라져 어느 쪽이 진짜인지 모르게 된다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_UIMotionTiming
{
	GENERATED_BODY()

	/** `--dur-fast` 110ms. 호버·눌림 같은 즉각 반응. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Motion", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float Fast = 0.11f;

	/** `--dur-base` 180ms. 패널 열기·닫기. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Motion", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float Base = 0.18f;

	/** `--dur-screen` 320ms. 화면 전환. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Motion", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float Screen = 0.32f;
};

/**
 * UI 색과 모션의 단일 원천.
 *
 * `Project Settings > Game > FestaFiesta UI Palette`에서 조정한다.
 *
 * === 위젯 안에 색을 하드코딩하지 않는다 ===
 * 화면이 12개로 늘어난 뒤에는 색 하나를 바꾸려고 WBP를 전부 열어야 한다.
 * 팔레트가 둘(인게임 LightTeal · 프론트엔드 Glass Orbit)이라 교체 지점이 하나여야 한다.
 *
 * === 팔레트에 없는 것 ===
 * - 지표 구간 **경계값·라벨·아이콘** — `UFF_UIMetricBandSettings`가 소유한다
 * - 간격 · 반경 · 폰트 크기 — 에디터 마감 영역이다 (`docs/memory/wbp-codegen-workflow.md` 규칙 1)
 * - 버튼 상태색 — `FF_CommonButtonBase` · `FF_LiveHudMenuBar`가 WBP별로 들고 있다
 *
 * === 색만으로 상태를 구분하지 않는다 (AT-011) ===
 * 이 클래스는 색을 제공할 뿐이고 규칙은 위젯이 지킨다. `UFF_StateChip`처럼 색 + 아이콘 + 문구를
 * 함께 받는 API를 쓴다. 색만 받는 오버로드를 만들지 않는다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "FestaFiesta UI Palette"))
class FESTAFIESTA_API UFF_UIPalette : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/**
	 * 위젯이 `GetDefault<>`를 직접 부르지 않게 하는 단일 접근점.
	 * 나중에 축제별 테마가 생기면 이 함수만 고치면 된다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	static const UFF_UIPalette* Get();

	/** 셋업 · 홍보 · 개장 준비 · 라이브 · 마감 · 결산 · 피날레. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "In Game (LightTeal)")
	FFF_UIPaletteInGame InGame;

	/** 타이틀 · 플레이 허브 · 저장 기록 · 옵션 · Atlas. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Frontend (Glass Orbit)")
	FFF_UIPaletteFrontend Frontend;

	/** 전환 길이. 초 단위. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Motion")
	FFF_UIMotionTiming Motion;
};
