// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Panels/FF_LivePanelDensity.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "Engine/World.h"
#include "UI/FF_UILog.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLiveDensity"

namespace FFDensityLocal
{
	/**
	 * 범례 글자 크기. 런타임에 만드는 `UTextBlock` 은 **꾸미지 않으면 기본 폰트**라
	 * 패널 안에서 혼자 크게 나온다 (2026-09-03 실기에서 확인).
	 */
	constexpr int32 LegendFontSize = 13;

	/**
	 * 범례 수치 자릿수.
	 *
	 * 🔴 경계는 `DensityToLOS()` 를 훑어서 찾으므로 **한 스텝 지나서 잡힌다** —
	 *    간격이 0.001 이면 `0.31` 이 아니라 `0.311` 이 나온다. 값 자체는 맞지만
	 *    화면에 `0.311명/㎡` 로 찍히면 지저분하고, 원래 기준값이 뭔지도 흐려진다.
	 *
	 *    간격을 더 줄이는 대신 **표시만 반올림한다.** 경계를 역산하는 방식은 그대로 두고
	 *    보기만 다듬는 쪽이 안전하다 — 소유자가 `0.315` 같은 값을 쓰더라도 그대로 보인다.
	 */
	FNumberFormattingOptions MakeDensityFormat()
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 0;
		Options.MaximumFractionalDigits = 2;
		return Options;
	}

	/**
	 * 히트맵 상태를 쥔 곳.
	 *
	 * 🔴 처음엔 `FF.Crowd.Debug` 콘솔 **변수**를 찾으려 했는데 그건 변수가 아니라
	 *    **명령**(`FAutoConsoleCommandWithWorldArgsAndOutputDevice`)이라
	 *    `FindConsoleVariable()` 이 항상 null 을 돌려줬다. 그래서 버튼이 꺼진 채로 떴다.
	 *
	 *    실제 상태는 서브시스템의 `bDrawDebugDensity` 이고 `BlueprintReadWrite` 라 공개돼
	 *    있다. 콘솔을 흉내 내지 말고 그 값을 직접 읽고 쓴다.
	 *
	 * ⚠️ 이것은 월드·경제 상태가 아니라 **디버그 표시 플래그**다. UI 가 상태를 직접
	 *    바꾸지 않는다는 규칙(AGENTS.md 2)에 걸리지 않는다. 제품용 토글 API 가 오면
	 *    그쪽으로 옮긴다 (`UI_API.md` 11번).
	 */
	UFF_CrowdSimulationSubsystem* FindCrowd(const UObject* WorldContext)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		return World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;
	}
}

// =============================================================================
// FFF_DensityRules — 순수 함수
// =============================================================================

FText FFF_DensityRules::GetGradeLetter(const ECrowdLOS Grade)
{
	switch (Grade)
	{
	case ECrowdLOS::A: return LOCTEXT("LOS_A", "A");
	case ECrowdLOS::B: return LOCTEXT("LOS_B", "B");
	case ECrowdLOS::C: return LOCTEXT("LOS_C", "C");
	case ECrowdLOS::D: return LOCTEXT("LOS_D", "D");
	case ECrowdLOS::E: return LOCTEXT("LOS_E", "E");
	case ECrowdLOS::F: return LOCTEXT("LOS_F", "F");
	}

	return FText::GetEmpty();
}

FText FFF_DensityRules::GetGradeName(const ECrowdLOS Grade)
{
	// 🔴 `UMETA(DisplayName)` 을 런타임에 읽지 않는다 — 패키징 빌드에서 사라진다.
	//    말은 Fruin 보행로 기준을 축제장 표현으로 옮긴 것이라 이정원 님 확인 대기 중이다
	//    (`UI_API.md` 11번). 바뀌어도 여기만 고치면 된다.
	switch (Grade)
	{
	case ECrowdLOS::A: return LOCTEXT("LOSName_A", "여유");
	case ECrowdLOS::B: return LOCTEXT("LOSName_B", "원활");
	case ECrowdLOS::C: return LOCTEXT("LOSName_C", "보통");
	case ECrowdLOS::D: return LOCTEXT("LOSName_D", "혼잡");
	case ECrowdLOS::E: return LOCTEXT("LOSName_E", "매우 혼잡");
	case ECrowdLOS::F: return LOCTEXT("LOSName_F", "위험");
	}

	return FText::GetEmpty();
}

TArray<FFF_DensityLegendRow> FFF_DensityRules::BuildLegend(
	const float MaxScanDensity, const float StepDensity)
{
	TArray<FFF_DensityLegendRow> Rows;

	if (StepDensity <= 0.f || MaxScanDensity <= 0.f)
	{
		// 잘못된 인자로 무한 루프를 돌지 않는다. 빈 범례는 화면이 "못 읽었다" 로 처리한다.
		return Rows;
	}

	// 0 에서 시작하는 첫 등급을 먼저 연다.
	ECrowdLOS CurrentGrade = FCrowdDensityGrid::DensityToLOS(0.f);

	FFF_DensityLegendRow& First = Rows.AddDefaulted_GetRef();
	First.Grade = CurrentGrade;
	First.GradeLetter = GetGradeLetter(CurrentGrade);
	First.GradeName = GetGradeName(CurrentGrade);
	First.MinDensity = 0.f;

	// 🔴 **경계를 베끼지 않고 물어본다.** 자세한 이유는 헤더 주석에.
	for (float Density = StepDensity; Density <= MaxScanDensity; Density += StepDensity)
	{
		const ECrowdLOS Grade = FCrowdDensityGrid::DensityToLOS(Density);
		if (Grade == CurrentGrade)
		{
			continue;
		}

		// 등급이 바뀌었다. 직전 줄을 닫고 새 줄을 연다.
		Rows.Last().MaxDensity = Density;

		FFF_DensityLegendRow& Next = Rows.AddDefaulted_GetRef();
		Next.Grade = Grade;
		Next.GradeLetter = GetGradeLetter(Grade);
		Next.GradeName = GetGradeName(Grade);
		Next.MinDensity = Density;

		CurrentGrade = Grade;
	}

	// 마지막 등급은 위가 없다. `MaxDensity` 를 음수로 두어 화면이 `이상` 으로 적게 한다 —
	// 0 으로 두면 `0 명/㎡ 까지` 로 읽혀서 정반대 뜻이 된다.
	if (Rows.Num() > 0)
	{
		Rows.Last().MaxDensity = -1.f;
	}

	return Rows;
}

// =============================================================================
// UFF_LivePanelDensity
// =============================================================================

void UFF_LivePanelDensity::NativeOnPanelOpened()
{
	Super::NativeOnPanelOpened();

	if (Button_Toggle)
	{
		Button_Toggle->OnClicked.AddUniqueDynamic(this, &UFF_LivePanelDensity::HandleToggleClicked);
	}

	// ★ 못 하는 것을 화면에 적는다. 안 적으면 패키징에서 안 켜지는 이유를 아무도 모른다.
	// ★ 별표를 쓰지 않는다. UMG 는 마크다운을 모르므로 `**` 가 글자로 그대로 나온다.
	SetTextOrHide(Text_MissingNote, LOCTEXT("LiveDensity_MissingNote",
		"히트맵은 지금 디버그 표시라 에디터에서만 보입니다. 제품 빌드용 표시를 요청해 뒀습니다."));

	RebuildLegend();
	RefreshToggleLabel();
	RefreshSummary();

	TimeSinceRefresh = 0.f;
}

void UFF_LivePanelDensity::NativeOnPanelClosed()
{
	if (Button_Toggle)
	{
		Button_Toggle->OnClicked.RemoveDynamic(this, &UFF_LivePanelDensity::HandleToggleClicked);
	}

	// ★ 히트맵은 **끄지 않는다.** 패널을 닫아도 지도는 계속 보고 싶을 수 있다 —
	//   패널은 켜고 끄는 수단일 뿐이고 표시 자체를 소유하지 않는다.

	Super::NativeOnPanelClosed();
}

void UFF_LivePanelDensity::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 접혀 있으면 세지 않는다. 패널은 캐시되어 살아 있다.
	if (GetVisibility() == ESlateVisibility::Collapsed)
	{
		return;
	}

	// 🔴 밀도는 매 프레임 바뀌지만 **매 프레임 다시 그리지 않는다.** 숫자가 계속 떨리면
	//    읽을 수가 없고, 눈으로 좇을 값도 아니다.
	TimeSinceRefresh += InDeltaTime;
	if (TimeSinceRefresh < RefreshIntervalSeconds)
	{
		return;
	}

	TimeSinceRefresh = 0.f;
	RefreshSummary();
}

void UFF_LivePanelDensity::HandleToggleClicked()
{
	UFF_CrowdSimulationSubsystem* Crowd = FFDensityLocal::FindCrowd(this);
	if (!Crowd)
	{
		// 조용히 넘기지 않는다 — 버튼이 눌리는데 아무 일도 안 나는 것이 제일 헷갈린다.
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] Crowd 서브시스템이 없어 히트맵을 켤 수 없다."), *GetName());
		return;
	}

	Crowd->bDrawDebugDensity = !Crowd->bDrawDebugDensity;

	RefreshToggleLabel();
}

void UFF_LivePanelDensity::RefreshToggleLabel()
{
	const UFF_CrowdSimulationSubsystem* Crowd = FFDensityLocal::FindCrowd(this);
	const bool bOn = Crowd && Crowd->bDrawDebugDensity;

	// !! 상태를 색으로만 말하지 않는다 !! 라벨이 지금 상태를 글자로 적는다 (AT-011).
	SetTextOrHide(Text_ToggleLabel, bOn
		? LOCTEXT("LiveDensity_ToggleOn", "히트맵 끄기")
		: LOCTEXT("LiveDensity_ToggleOff", "히트맵 켜기"));

	if (Button_Toggle)
	{
		Button_Toggle->SetIsEnabled(Crowd != nullptr);
	}
}

void UFF_LivePanelDensity::RebuildLegend()
{
	if (!Box_Legend)
	{
		return;
	}

	Box_Legend->ClearChildren();

	const TArray<FFF_DensityLegendRow> Rows = FFF_DensityRules::BuildLegend();
	if (Rows.IsEmpty())
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 밀도 등급 구간을 읽지 못해 범례를 만들 수 없다."), *GetName());
		return;
	}

	for (const FFF_DensityLegendRow& Row : Rows)
	{
		UTextBlock* Line = NewObject<UTextBlock>(this);
		if (!Line)
		{
			continue;
		}

		// 🔴 **넷을 같이 적는다** — 문자·이름·수치·(색은 히트맵 쪽). 색을 못 봐도
		//    `D · 0.72~1.08명/㎡ · 혼잡` 을 읽으면 무엇인지 안다 (AT-011).
		const FNumberFormattingOptions Format = FFDensityLocal::MakeDensityFormat();

		const FText Range = Row.HasUpperBound()
			? FText::Format(
				LOCTEXT("LiveDensity_Range", "{0}~{1}명/㎡"),
				FText::AsNumber(Row.MinDensity, &Format),
				FText::AsNumber(Row.MaxDensity, &Format))
			: FText::Format(
				LOCTEXT("LiveDensity_RangeTop", "{0}명/㎡ 이상"),
				FText::AsNumber(Row.MinDensity, &Format));

		Line->SetText(FText::Format(
			LOCTEXT("LiveDensity_LegendRow", "{0} · {1} · {2}"),
			Row.GradeLetter, Range, Row.GradeName));

		// 🔴 **꾸미지 않으면 기본 폰트**라 패널 안에서 혼자 크게 나온다.
		FSlateFontInfo Font = Line->GetFont();
		Font.Size = FFDensityLocal::LegendFontSize;
		Line->SetFont(Font);

		Line->SetAutoWrapText(true);

		if (UVerticalBoxSlot* LineSlot = Box_Legend->AddChildToVerticalBox(Line))
		{
			LineSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
		}
	}
}

void UFF_LivePanelDensity::RefreshSummary()
{
	const UWorld* World = GetWorld();
	const UFF_CrowdSimulationSubsystem* Crowd =
		World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;

	if (!Crowd)
	{
		// 못 읽은 것과 `0명/㎡` 는 다르다. 0 으로 적으면 "한산하다" 는 틀린 사실이 된다.
		SetTextOrHide(Text_Peak, LOCTEXT("LiveDensity_NoCrowd", "밀도를 아직 읽지 못했습니다."));
		SetTextOrHide(Text_Congested, FText::GetEmpty());
		return;
	}

	const float MaxDensity = Crowd->GetMaxDensity();
	const ECrowdLOS PeakGrade = FCrowdDensityGrid::DensityToLOS(MaxDensity);

	const FNumberFormattingOptions Format = FFDensityLocal::MakeDensityFormat();

	SetTextOrHide(Text_Peak, FText::Format(
		LOCTEXT("LiveDensity_Peak", "가장 붐비는 곳 {0}명/㎡ · {1} ({2})"),
		FText::AsNumber(MaxDensity, &Format),
		FFF_DensityRules::GetGradeLetter(PeakGrade),
		FFF_DensityRules::GetGradeName(PeakGrade)));

	// ★ 0 이어도 적는다. 접으면 "아직 안 셌다" 와 "없다" 가 구분되지 않는다.
	const int32 CongestedCells = Crowd->GetCongestedCellCount();

	SetTextOrHide(Text_Congested, (CongestedCells > 0)
		? FText::Format(
			LOCTEXT("LiveDensity_Congested", "혼잡 구간 {0}칸"),
			FText::AsNumber(CongestedCells))
		: LOCTEXT("LiveDensity_NoCongestion", "혼잡 구간 없음"));
}

#undef LOCTEXT_NAMESPACE
