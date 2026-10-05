// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Panels/FF_LivePanelDensity.h"

#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "CrowdSystem/Systems/FF_CrowdDensityHeatmapSubsystem.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "Engine/World.h"
#include "UI/FF_UILog.h"
#include "UI/LiveHUD/Panels/FF_LiveDensityLegendRowWidget.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLiveDensity"

namespace FFDensityLocal
{
	/**
	 * 범례 글자 크기. 런타임에 만드는 `UTextBlock` 은 **꾸미지 않으면 기본 폰트**라
	 * 패널 안에서 혼자 크게 나온다 (2026-09-03 실기에서 확인).
	 */
	constexpr int32 LegendFontSize = 13;
	const FLinearColor TextColor(0.10f, 0.15f, 0.13f, 1.f);
	const FLinearColor MutedColor(0.28f, 0.35f, 0.31f, 1.f);
	const FLinearColor RowColor(0.92f, 0.95f, 0.93f, 1.f);
	const FLinearColor PeakRowColor(0.80f, 0.88f, 0.83f, 1.f);

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

	UFF_CrowdDensityHeatmapSubsystem* FindHeatmap(const UObject* WorldContext)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		return World ? World->GetSubsystem<UFF_CrowdDensityHeatmapSubsystem>() : nullptr;
	}

	UTextBlock* MakeLegendText(UObject* Outer, const UTextBlock* FontSource,
		const FText& Text, const int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Block = NewObject<UTextBlock>(Outer);
		FSlateFontInfo Font = FontSource ? FontSource->GetFont() : Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		Block->SetText(Text);
		Block->SetColorAndOpacity(FSlateColor(Color));
		Block->SetAutoWrapText(true);
		Block->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Block;
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

void UFF_LivePanelDensity::NativeDestruct()
{
	if (Button_Toggle)
	{
		Button_Toggle->OnClicked.RemoveDynamic(this, &UFF_LivePanelDensity::HandleToggleClicked);
	}
	Super::NativeDestruct();
}

void UFF_LivePanelDensity::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 접혀 있으면 세지 않는다. 패널은 캐시되어 살아 있다.
	if (!IsVisible())
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
	RefreshToggleLabel();
	RefreshSummary();
}

void UFF_LivePanelDensity::HandleToggleClicked()
{
	UFF_CrowdDensityHeatmapSubsystem* Heatmap = FFDensityLocal::FindHeatmap(this);
	if (!Heatmap)
	{
		UE_LOG(LogFestaFiestaUI, Warning, TEXT("[%s] 히트맵 서브시스템이 없어 히트맵을 켤 수 없다."), *GetName());
		RefreshToggleLabel();
		return;
	}

	const bool bTurnOn = !Heatmap->IsHeatmapVisible();
	if (Heatmap->SetHeatmapVisible(bTurnOn) != bTurnOn)
	{
		// 조용히 넘기지 않는다 — 버튼이 눌리는데 아무 일도 안 나는 것이 제일 헷갈린다.
		UE_LOG(LogFestaFiestaUI, Warning, TEXT("[%s] 히트맵을 켜지 못했다."), *GetName());
	}
	RefreshToggleLabel();
}

void UFF_LivePanelDensity::RefreshToggleLabel()
{
	// 판정은 히트맵 서브시스템(이정원 님, 출시 빌드에서도 켜진다)이 갖는다. 글자는 Class Defaults 의 편집 칸을 쓴다.
	const UFF_CrowdDensityHeatmapSubsystem* Heatmap = FFDensityLocal::FindHeatmap(this);
	const bool bHasMaterial = Heatmap && Heatmap->HasHeatmapMaterial();
	const bool bGridReady = Heatmap && Heatmap->IsDensityGridReady();
	const bool bOn = Heatmap && Heatmap->IsHeatmapVisible();

	// !! 상태를 색으로만 말하지 않는다 !! 라벨이 지금 상태를 글자로 적는다 (AT-011).
	SetTextOrHide(Text_ToggleLabel, bOn ? ToggleOnText : ToggleOffText);
	SetTextOrHide(Text_HeatmapStatus, bOn ? HeatmapOnText
		: !bHasMaterial ? HeatmapUnavailableText
		: !bGridReady ? HeatmapPendingText
		: HeatmapOffText);

	if (Button_Toggle)
	{
		Button_Toggle->SetIsEnabled(bOn || (bHasMaterial && bGridReady));
	}

	if (!bHasMaterial)
	{
		SetTextOrHide(Text_MissingNote, NoMaterialNoteText);
	}
	else if (!bGridReady)
	{
		SetTextOrHide(Text_MissingNote, GridPendingNoteText);
	}
	else
	{
		SetTextOrHide(Text_MissingNote, FText::GetEmpty());
	}
}

void UFF_LivePanelDensity::RebuildLegend()
{
	if (!Box_Legend)
	{
		return;
	}

	const TArray<FFF_DensityLegendRow> Rows = FFF_DensityRules::BuildLegend();
	if (Rows.IsEmpty())
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 밀도 등급 구간을 읽지 못해 범례를 만들 수 없다."), *GetName());
		return;
	}

	if (LegendRowWidgetClass)
	{
		if (CachedLegendRowWidgetClass != LegendRowWidgetClass)
		{
			Box_Legend->ClearChildren();
			LegendRowWidgets.Reset();
			LegendRowBorders.Reset();
			LegendPeakLabels.Reset();
			CachedLegendRowWidgetClass = LegendRowWidgetClass;
		}

		LegendGrades.Reset();
		for (int32 Index = 0; Index < Rows.Num(); ++Index)
		{
			if (!LegendRowWidgets.IsValidIndex(Index))
			{
				UFF_LiveDensityLegendRowWidget* RowWidget = GetOwningPlayer()
					? CreateWidget<UFF_LiveDensityLegendRowWidget>(GetOwningPlayer(), LegendRowWidgetClass)
					: CreateWidget<UFF_LiveDensityLegendRowWidget>(this, LegendRowWidgetClass);
				if (!RowWidget)
				{
					UE_LOG(LogFestaFiestaUI, Warning, TEXT("[%s] 밀도 범례 행을 생성하지 못했다."), *GetName());
					break;
				}
				LegendRowWidgets.Add(RowWidget);
			}

			UFF_LiveDensityLegendRowWidget* RowWidget = LegendRowWidgets[Index];
			if (RowWidget->GetParent() != Box_Legend)
			{
				Box_Legend->AddChildToVerticalBox(RowWidget);
			}
			if (UVerticalBoxSlot* RowSlot = Cast<UVerticalBoxSlot>(RowWidget->Slot))
			{
				RowSlot->SetPadding(LegendRowPadding);
			}
			RowWidget->SetRowData(Rows[Index]);
			RowWidget->SetCurrentPeak(false);
			RowWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
			LegendGrades.Add(Rows[Index].Grade);
		}
		for (int32 Index = LegendGrades.Num(); Index < LegendRowWidgets.Num(); ++Index)
		{
			LegendRowWidgets[Index]->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	// 이전 WBP가 아직 행 클래스를 지정하지 않은 경우에도 범례를 유지한다.
	Box_Legend->ClearChildren();
	LegendRowWidgets.Reset();
	CachedLegendRowWidgetClass = nullptr;
	LegendRowBorders.Reset();
	LegendPeakLabels.Reset();
	LegendGrades.Reset();

	for (const FFF_DensityLegendRow& Row : Rows)
	{
		// 문자·이름·수치·색상칩을 같이 적고, 색은 바닥 표시의 원본 함수를 사용한다.
		const FNumberFormattingOptions Format = FFDensityLocal::MakeDensityFormat();

		const FText Range = Row.HasUpperBound()
			? FText::Format(
				LOCTEXT("LiveDensity_RangeExclusive", "{0} 이상 · {1} 미만 명/㎡"),
				FText::AsNumber(Row.MinDensity, &Format),
				FText::AsNumber(Row.MaxDensity, &Format))
			: FText::Format(
				LOCTEXT("LiveDensity_RangeTop", "{0}명/㎡ 이상"),
				FText::AsNumber(Row.MinDensity, &Format));

		UBorder* Background = NewObject<UBorder>(this);
		Background->SetBrushColor(FFDensityLocal::RowColor);
		Background->SetPadding(FMargin(10.f, 5.f));
		Background->SetVisibility(ESlateVisibility::HitTestInvisible);
		UHorizontalBox* Line = NewObject<UHorizontalBox>(this);
		Background->SetContent(Line);

		USizeBox* ChipSize = NewObject<USizeBox>(this);
		ChipSize->SetWidthOverride(8.f);
		ChipSize->SetHeightOverride(26.f);
		UBorder* Chip = NewObject<UBorder>(this);
		Chip->SetBrushColor(FLinearColor::FromSRGBColor(FCrowdDensityGrid::LOSToColor(Row.Grade)));
		Chip->SetPadding(FMargin(0.f));
		ChipSize->SetContent(Chip);
		if (UHorizontalBoxSlot* ChipSlot = Line->AddChildToHorizontalBox(ChipSize))
		{
			ChipSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
			ChipSlot->SetVerticalAlignment(VAlign_Center);
		}

		UVerticalBox* Details = NewObject<UVerticalBox>(this);
		Details->AddChildToVerticalBox(FFDensityLocal::MakeLegendText(this, Text_Congested,
			FText::Format(LOCTEXT("LiveDensity_LegendName", "{0} · {1}"), Row.GradeLetter, Row.GradeName),
			FFDensityLocal::LegendFontSize, FFDensityLocal::TextColor));
		Details->AddChildToVerticalBox(FFDensityLocal::MakeLegendText(this, Text_Congested,
			Range, 11, FFDensityLocal::MutedColor));
		if (UHorizontalBoxSlot* DetailSlot = Line->AddChildToHorizontalBox(Details))
		{
			DetailSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			DetailSlot->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* PeakLabel = FFDensityLocal::MakeLegendText(this, Text_Congested,
			LOCTEXT("LiveDensity_CurrentPeak", "현재 최대"), 11, FFDensityLocal::TextColor);
		// Hidden으로 자리를 유지하여 최대 등급 변경 때 수치 열 폭이 흔들리지 않게 한다.
		PeakLabel->SetVisibility(ESlateVisibility::Hidden);
		if (UHorizontalBoxSlot* PeakSlot = Line->AddChildToHorizontalBox(PeakLabel))
		{
			PeakSlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
			PeakSlot->SetVerticalAlignment(VAlign_Center);
		}

		if (UVerticalBoxSlot* LineSlot = Box_Legend->AddChildToVerticalBox(Background))
		{
			LineSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
		}
		LegendRowBorders.Add(Background);
		LegendPeakLabels.Add(PeakLabel);
		LegendGrades.Add(Row.Grade);
	}
}

void UFF_LivePanelDensity::RefreshLegendHighlight(const TOptional<ECrowdLOS> PeakGrade)
{
	for (int32 Index = 0; Index < LegendGrades.Num(); ++Index)
	{
		const bool bPeak = PeakGrade.IsSet() && LegendGrades[Index] == PeakGrade.GetValue();
		if (LegendRowWidgets.IsValidIndex(Index))
		{
			LegendRowWidgets[Index]->SetCurrentPeak(bPeak);
		}
		else if (LegendRowBorders.IsValidIndex(Index) && LegendPeakLabels.IsValidIndex(Index))
		{
			LegendRowBorders[Index]->SetBrushColor(bPeak ? FFDensityLocal::PeakRowColor : FFDensityLocal::RowColor);
			LegendPeakLabels[Index]->SetVisibility(bPeak ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		}
	}
}

void UFF_LivePanelDensity::RefreshSummary()
{
	const UWorld* World = GetWorld();
	const UFF_CrowdSimulationSubsystem* Crowd =
		World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;

	if (!Crowd || !Crowd->GetDensityGrid().IsBuilt())
	{
		// 못 읽은 것과 `0명/㎡` 는 다르다. 0 으로 적으면 "한산하다" 는 틀린 사실이 된다.
		SetTextOrHide(Text_Peak, MissingValueText);
		SetTextOrHide(Text_PeakGrade, PendingGradeText);
		SetTextOrHide(Text_Congested, MissingValueText);
		SetTextOrHide(Text_CellSize, MissingValueText);
		SetTextOrHide(Text_CellArea, MissingValueText);
		RefreshLegendHighlight(TOptional<ECrowdLOS>());
		return;
	}

	const float MaxDensity = Crowd->GetMaxDensity();
	const FNumberFormattingOptions Format = FFDensityLocal::MakeDensityFormat();
	if (!FMath::IsFinite(MaxDensity) || MaxDensity < 0.f)
	{
		SetTextOrHide(Text_Peak, MissingValueText);
		SetTextOrHide(Text_PeakGrade, InvalidGradeText);
		RefreshLegendHighlight(TOptional<ECrowdLOS>());
	}
	else
	{
		const ECrowdLOS PeakGrade = FCrowdDensityGrid::DensityToLOS(MaxDensity);
		const FText PeakValue = FText::Format(PeakValueTextFormat,
			FFormatNamedArguments{{TEXT("Density"), FText::AsNumber(MaxDensity, &Format)}});
		const FText GradeValue = FText::Format(PeakGradeTextFormat, FFormatNamedArguments{
			{TEXT("Grade"), FFF_DensityRules::GetGradeLetter(PeakGrade)},
			{TEXT("Name"), FFF_DensityRules::GetGradeName(PeakGrade)}});
		SetTextOrHide(Text_Peak, Text_PeakGrade ? PeakValue : FText::Format(
			LOCTEXT("LiveDensity_LegacyPeak", "가장 붐비는 곳 {0} · {1}"), PeakValue, GradeValue));
		SetTextOrHide(Text_PeakGrade, GradeValue);
		RefreshLegendHighlight(PeakGrade);
	}

	// ★ 0 이어도 적는다. 접으면 "아직 안 셌다" 와 "없다" 가 구분되지 않는다.
	const int32 CongestedCells = Crowd->GetCongestedCellCount();

	const FText CongestedValue = CongestedCells >= 0
		? FText::Format(CongestedTextFormat, FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(CongestedCells)}})
		: MissingValueText;
	SetTextOrHide(Text_Congested, Text_PeakGrade ? CongestedValue
		: FText::Format(LOCTEXT("LiveDensity_LegacyCongested", "혼잡 구간 {0}"), CongestedValue));

	const FCrowdDensityGrid& Grid = Crowd->GetDensityGrid();
	const float Size = Grid.GetCellSizeMeters();
	const float Area = Grid.GetCellAreaSqM();
	const FText SizeValue = FMath::IsFinite(Size) && Size > 0.f
		? FText::Format(CellSizeTextFormat, FFormatNamedArguments{{TEXT("Size"), FText::AsNumber(Size, &Format)}})
		: MissingValueText;
	const FText AreaValue = FMath::IsFinite(Area) && Area > 0.f
		? FText::Format(CellAreaTextFormat, FFormatNamedArguments{{TEXT("Area"), FText::AsNumber(Area, &Format)}})
		: MissingValueText;
	SetTextOrHide(Text_CellSize, Text_CellArea ? SizeValue
		: FText::Format(LOCTEXT("LiveDensity_LegacyCell", "{0} · {1}"), SizeValue, AreaValue));
	SetTextOrHide(Text_CellArea, AreaValue);
}

#undef LOCTEXT_NAMESPACE
