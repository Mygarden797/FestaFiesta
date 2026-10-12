// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Panels/FF_LivePanelGuests.h"
#include "UI/LiveHUD/Panels/FF_LiveNeedRowWidget.h"

#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "UI/FF_UIDisplayTextLibrary.h"
#include "UI/FF_UILog.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLiveGuests"

// =============================================================================
// FFF_LiveGuestRules — 순수 함수
// =============================================================================

EFF_LiveNeedSeverity FFF_LiveGuestRules::ResolveSeverity(
	float UnmetRatioPercent, int32 SampleCount, float WatchPercent, float BadPercent)
{
	// 🔴 표본이 없으면 `모름` 이다. 집계는 표본이 0 일 때도 미해소율을 `0` 으로 돌려주는데,
	// 그것을 `괜찮음` 으로 그리면 화면이 "아무도 불편하지 않다" 는 **틀린 사실**을 말한다.
	if (SampleCount <= 0 || !FMath::IsFinite(UnmetRatioPercent))
	{
		return EFF_LiveNeedSeverity::Unknown;
	}

	// 임계를 잘못 넣어 뒤집혀 있으면 바로잡는다. 그대로 두면 `나쁨` 이 영영 안 뜨거나
	// 전부 `나쁨` 이 되는데, 둘 다 화면만 보고는 설정 실수인 줄 모른다.
	const float Watch = FMath::Min(WatchPercent, BadPercent);
	const float Bad = FMath::Max(WatchPercent, BadPercent);

	if (UnmetRatioPercent >= Bad)
	{
		return EFF_LiveNeedSeverity::Bad;
	}
	if (UnmetRatioPercent >= Watch)
	{
		return EFF_LiveNeedSeverity::Watch;
	}
	return EFF_LiveNeedSeverity::Ok;
}

void FFF_LiveGuestRules::SortBySeverity(TArray<FFF_LiveNeedRow>& Rows)
{
	// 급한 순서를 숫자로 매긴다. enum 선언 순서에 기대지 않는다 — 항목을 더할 때
	// 순서가 조용히 바뀌면 정렬이 뒤집힌다.
	auto Rank = [](EFF_LiveNeedSeverity Severity)
	{
		switch (Severity)
		{
		case EFF_LiveNeedSeverity::Bad:		return 3;
		case EFF_LiveNeedSeverity::Watch:	return 2;

		// ★ `모름` 을 `괜찮음` 보다 위에 둔다. 읽지 못한 것은 확인이 필요한 것이지
		//   괜찮다고 확인된 것이 아니다.
		case EFF_LiveNeedSeverity::Unknown:	return 1;
		default:							return 0;
		}
	};

	Rows.Sort([&Rank](const FFF_LiveNeedRow& A, const FFF_LiveNeedRow& B)
	{
		const int32 RankA = Rank(A.Severity);
		const int32 RankB = Rank(B.Severity);
		if (RankA != RankB)
		{
			return RankA > RankB;
		}

		const float PercentA = FMath::IsFinite(A.View.UnmetRatioPercent) ? A.View.UnmetRatioPercent : -1.f;
		const float PercentB = FMath::IsFinite(B.View.UnmetRatioPercent) ? B.View.UnmetRatioPercent : -1.f;
		if (!FMath::IsNearlyEqual(PercentA, PercentB))
		{
			return PercentA > PercentB;
		}

		// 값이 같으면 enum 순서로 고정한다. 안 그러면 갱신할 때마다 줄이 흔들려
		// 읽는 중에 눈이 따라가지 못한다.
		return static_cast<uint8>(A.View.NeedType) < static_cast<uint8>(B.View.NeedType);
	});
}

// =============================================================================
// UFF_LivePanelGuests
// =============================================================================

void UFF_LivePanelGuests::NativeOnPanelOpened()
{
	Super::NativeOnPanelOpened();

	SetTextOrHide(Text_MissingNote, FooterNoteText);

	TimeSinceRefresh = 0.f;
	RefreshAll();
}

void UFF_LivePanelGuests::NativeOnPanelClosed()
{
	Rows.Reset();
	Super::NativeOnPanelClosed();
}

void UFF_LivePanelGuests::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 🔴 닫혀 있으면 읽지 않는다. 패널은 캐시되어 살아 있다.
	if (GetVisibility() == ESlateVisibility::Collapsed)
	{
		return;
	}

	// 🔴 `OnNeedUnmetRatioChanged` 를 구독하지 않는다. 소유자 주석에 **초당 수십 번 뜬다**고
	// 적혀 있어서, 이벤트마다 다시 그리면 목록이 눈으로 못 읽을 만큼 흔들린다.
	TimeSinceRefresh += InDeltaTime;
	if (TimeSinceRefresh < RefreshIntervalSeconds)
	{
		return;
	}

	TimeSinceRefresh = 0.f;
	RefreshAll();
}

void UFF_LivePanelGuests::RefreshAll()
{
	const UGameInstance* GI = GetGameInstance();
	const UFF_FestivalViewModelSubsystem* ViewModel =
		GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;

	SetTextOrHide(Text_VisitorNote, VisitorNoteText);
	SetTextOrHide(Text_AdmittedNote, AdmittedNoteText);

	if (!ViewModel)
	{
		// 연결이 사라져도 이전 인원과 욕구를 현재 값처럼 남기지 않는다.
		Rows.Reset();
		SetTextOrHide(Text_VisitorCount, MissingValueText);
		SetTextOrHide(Text_AdmittedCount, MissingValueText);
		SetTextOrHide(Text_SampleCount, SampleUnknownText);
		SetTextOrHide(Text_Summary, CheckingText);
		RefreshRowWidgets();
		SetTextOrHide(Text_Empty, CheckingText);
		return;
	}

	// --- 인원 ---
	const int32 Visitors = ViewModel->GetConcurrentVisitorCount();
	const int32 Admitted = ViewModel->GetAdmittedVisitorsToday();
	const auto CountText = [this](int32 Count)
	{
		return Count >= 0
			? FText::Format(CountTextFormat, FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(Count)}})
			: MissingValueText;
	};
	SetTextOrHide(Text_VisitorCount, CountText(Visitors));
	SetTextOrHide(Text_AdmittedCount, CountText(Admitted));

	// --- 욕구 ---
	const TArray<FFF_LiveNeedView> Needs = ViewModel->GetNeedMetrics();
	const int32 SampleCount = ViewModel->GetNeedSampleCount();
	SetTextOrHide(Text_SampleCount, SampleCount > 0 && !Needs.IsEmpty()
		? FText::Format(SampleCountTextFormat, FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(SampleCount)}})
		: SampleUnknownText);

	Rows.Reset(Needs.Num());
	int32 BadCount = 0;
	int32 WatchCount = 0;
	int32 UnknownCount = 0;

	for (const FFF_LiveNeedView& Need : Needs)
	{
		FFF_LiveNeedRow& Row = Rows.AddDefaulted_GetRef();
		Row.View = Need;
		Row.Severity = FFF_LiveGuestRules::ResolveSeverity(
			Need.UnmetRatioPercent, SampleCount, WatchPercent, BadPercent);

		if (Row.Severity == EFF_LiveNeedSeverity::Bad)
		{
			++BadCount;
		}
		else if (Row.Severity == EFF_LiveNeedSeverity::Watch)
		{
			++WatchCount;
		}
		else if (Row.Severity == EFF_LiveNeedSeverity::Unknown)
		{
			++UnknownCount;
		}
	}

	FFF_LiveGuestRules::SortBySeverity(Rows);

	// 표본이나 항목이 없는 집계를 "급한 불편 없음"으로 오해하지 않도록 분리한다.
	const FFormatNamedArguments SummaryArgs{
		{TEXT("Sample"), FText::AsNumber(SampleCount)},
		{TEXT("Bad"), FText::AsNumber(BadCount)},
		{TEXT("Watch"), FText::AsNumber(WatchCount)},
		{TEXT("Unknown"), FText::AsNumber(UnknownCount)}};
	SetTextOrHide(Text_Summary, SampleCount <= 0 ? NoSampleText
		: Rows.IsEmpty() ? EmptyText
		: FText::Format(BadCount == 0 && WatchCount == 0 && UnknownCount == 0 ? AllOkTextFormat : SummaryTextFormat, SummaryArgs));
	RefreshRowWidgets();
	SetTextOrHide(Text_Empty, Rows.IsEmpty() ? EmptyText : FText::GetEmpty());
}

void UFF_LivePanelGuests::RefreshRowWidgets()
{
	if (!Box_Needs)
	{
		return;
	}

	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		if (RowWidgetClass)
		{
			if (!CachedRowWidgets.IsValidIndex(Index))
			{
				UFF_LiveNeedRowWidget* RowWidget = CreateWidget<UFF_LiveNeedRowWidget>(this, RowWidgetClass);
				if (!RowWidget)
				{
					break;
				}
				CachedRowWidgets.Add(RowWidget);
				if (UVerticalBoxSlot* RowSlot = Box_Needs->AddChildToVerticalBox(RowWidget))
				{
					RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
				}
			}
			CachedRowWidgets[Index]->SetRowData(Rows[Index]);
			CachedRowWidgets[Index]->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			continue;
		}

		if (!CachedFallbackLines.IsValidIndex(Index))
		{
			UTextBlock* Line = NewObject<UTextBlock>(this);
			CachedFallbackLines.Add(Line);
			if (UVerticalBoxSlot* LineSlot = Box_Needs->AddChildToVerticalBox(Line))
			{
				LineSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
			}
		}
		UpdateFallbackLine(*CachedFallbackLines[Index], Rows[Index]);
		CachedFallbackLines[Index]->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	// 표시 순서별 행을 재사용해 매초 위젯 트리와 스크롤 위치를 초기화하지 않는다.
	for (int32 Index = 0; Index < CachedRowWidgets.Num(); ++Index)
	{
		if (!RowWidgetClass || Index >= Rows.Num())
		{
			CachedRowWidgets[Index]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	for (int32 Index = 0; Index < CachedFallbackLines.Num(); ++Index)
	{
		if (RowWidgetClass || Index >= Rows.Num())
		{
			CachedFallbackLines[Index]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	Box_Needs->SetVisibility((Rows.Num() > 0)
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);

}

void UFF_LivePanelGuests::UpdateFallbackLine(UTextBlock& Line, const FFF_LiveNeedRow& Row) const
{
	const FText Name = UFF_UIDisplayTextLibrary::GetNeedTypeText(Row.View.NeedType);
	if (Row.Severity == EFF_LiveNeedSeverity::Unknown || !FMath::IsFinite(Row.View.UnmetRatioPercent))
	{
		Line.SetText(FText::Format(LOCTEXT("LiveGuests_RowPreparing", "{0}  —  집계 준비 중"), Name));
	}
	else
	{
		const FText Status = Row.Severity == EFF_LiveNeedSeverity::Bad ? LOCTEXT("LiveGuests_Bad", "나쁨")
			: Row.Severity == EFF_LiveNeedSeverity::Watch ? LOCTEXT("LiveGuests_Watch", "주의")
			: LOCTEXT("LiveGuests_Ok", "괜찮음");
		const FText Remedy = Row.Severity == EFF_LiveNeedSeverity::Ok ? FText::GetEmpty()
			: FText::Format(LOCTEXT("LiveGuests_RowRemedy", "  —  {0}"), UFF_UIDisplayTextLibrary::GetNeedRemedyText(Row.View.NeedType));
		Line.SetText(FText::Format(LOCTEXT("LiveGuests_RowWithStatus", "{0}  {1}% · {2}{3}"),
			Name, FText::AsNumber(FMath::RoundToInt(FMath::Clamp(Row.View.UnmetRatioPercent, 0.f, 100.f))), Status, Remedy));
	}
	ApplyDynamicRowStyle(Line);
}

void UFF_LivePanelGuests::SetTextOrHide(UTextBlock* TextBlock, const FText& Value)
{
	if (!TextBlock)
	{
		return;
	}

	TextBlock->SetText(Value);
	TextBlock->SetVisibility(Value.IsEmpty()
		? ESlateVisibility::Collapsed
		: ESlateVisibility::SelfHitTestInvisible);
}

#undef LOCTEXT_NAMESPACE
