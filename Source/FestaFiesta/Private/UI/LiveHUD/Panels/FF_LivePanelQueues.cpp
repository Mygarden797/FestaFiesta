// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Panels/FF_LivePanelQueues.h"
#include "UI/LiveHUD/Panels/FF_LiveQueueRowWidget.h"

#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "UI/FF_UIDisplayTextLibrary.h"


#define LOCTEXT_NAMESPACE "FestaFiestaLiveQueues"

// =============================================================================
// FFF_LiveQueueRules — 순수 함수
// =============================================================================

FFF_LiveQueueSummary FFF_LiveQueueRules::BuildSummary(const TArray<FFF_LiveFacilityView>& Facilities)
{
	FFF_LiveQueueSummary Result;

	int64 WaitSecondsSum = 0;

	for (const FFF_LiveFacilityView& Facility : Facilities)
	{
		Result.TotalWaiting += Facility.WaitingCount;
		Result.TotalInService += Facility.ActiveServiceCount;

		// 🔴 아직 아무도 못 끝낸 시설(`INDEX_NONE`)은 평균에서 뺀다. 0 으로 세면 평균이
		//    실제보다 낮아져서, 줄이 긴데도 안심되는 숫자가 뜬다.
		if (Facility.AverageWaitSeconds != INDEX_NONE)
		{
			WaitSecondsSum += Facility.AverageWaitSeconds;
			++Result.AverageSampleCount;
		}

		switch (Facility.QueueState)
		{
		case EFF_LiveFacilityQueueState::Open:		++Result.OpenCount; break;
		case EFF_LiveFacilityQueueState::Full:		++Result.FullCount; break;
		case EFF_LiveFacilityQueueState::Blocked:	++Result.BlockedCount; break;

		// 닫힘과 비우는 중은 둘 다 "지금 새 손님을 안 받는다" 다. 화면에서 구분해도
		// 사람이 할 일이 같아서 한 칸으로 묶는다.
		case EFF_LiveFacilityQueueState::Closed:
		case EFF_LiveFacilityQueueState::Draining:	++Result.NotAcceptingCount; break;

		// ★ 모름을 `닫힘` 에 섞지 않는다. 섞으면 못 읽은 것이 정상적으로 닫힌 것처럼 보인다.
		default:									++Result.UnknownCount; break;
		}

		// 가장 긴 줄. 같으면 이름이 앞서는 쪽으로 고정해 갱신할 때 흔들리지 않게 한다.
		const bool bLonger = Facility.WaitingCount > Result.LongestQueueWaiting;
		const bool bTieButEarlierName =
			Facility.WaitingCount == Result.LongestQueueWaiting
			&& Facility.DisplayName.CompareTo(Result.LongestQueueName) < 0;

		if (bLonger || bTieButEarlierName)
		{
			Result.LongestQueueWaiting = Facility.WaitingCount;
			Result.LongestQueueName = Facility.DisplayName;
		}

		// --- 지금 서면 가장 오래 걸리는 곳 ---
		// 못 읽은 시설(`INDEX_NONE`)은 세지 않는다. 0 으로 세면 "대기 없는 최고의 시설" 로
		// 보여서 실제로 붐비는 곳이 가려진다 — 평균에서 뺀 것과 같은 이유다.
		if (Facility.ExpectedWaitSeconds != INDEX_NONE)
		{
			++Result.ExpectedWaitSampleCount;

			const bool bWorse = Facility.ExpectedWaitSeconds > Result.WorstExpectedWaitSeconds;
			const bool bTieButEarlier =
				Facility.ExpectedWaitSeconds == Result.WorstExpectedWaitSeconds
				&& Facility.DisplayName.CompareTo(Result.WorstExpectedWaitName) < 0;

			if (bWorse || bTieButEarlier)
			{
				Result.WorstExpectedWaitSeconds = Facility.ExpectedWaitSeconds;
				Result.WorstExpectedWaitName = Facility.DisplayName;
			}
		}
	}

	if (Result.AverageSampleCount > 0)
	{
		Result.AverageWaitSeconds =
			static_cast<int32>(WaitSecondsSum / Result.AverageSampleCount);
	}

	// 🔴 줄이 하나도 없는 것과 시설이 없는 것을 구분한다. 대기 0 팀인 시설만 있으면
	//    `가장 긴 줄` 은 0 팀짜리가 되는데, 그것을 이름과 함께 띄우면 거기가 붐비는
	//    것처럼 읽힌다. 대기가 아예 없으면 없는 것으로 둔다.
	if (Result.TotalWaiting <= 0)
	{
		Result.LongestQueueWaiting = INDEX_NONE;
		Result.LongestQueueName = FText::GetEmpty();
	}

	return Result;
}

void FFF_LiveQueueRules::SortByWaiting(TArray<FFF_LiveFacilityView>& Facilities)
{
	Facilities.Sort([](const FFF_LiveFacilityView& A, const FFF_LiveFacilityView& B)
	{
		if (A.WaitingCount != B.WaitingCount)
		{
			return A.WaitingCount > B.WaitingCount;
		}

		// 값이 같으면 이름으로 고정한다. 안 그러면 갱신할 때마다 줄 순서가 흔들려
		// 읽는 중에 눈이 따라가지 못한다.
		return A.DisplayName.CompareTo(B.DisplayName) < 0;
	});
}

// =============================================================================
// UFF_LivePanelQueues
// =============================================================================

void UFF_LivePanelQueues::NativeOnPanelOpened()
{
	Super::NativeOnPanelOpened();
	SetTextOrHide(Text_MissingNote, FooterNoteText);
	TimeSinceRefresh = 0.f;
	RefreshAll();
}

void UFF_LivePanelQueues::NativeOnPanelClosed()
{
	Summary = FFF_LiveQueueSummary();
	Rows.Reset();
	Super::NativeOnPanelClosed();
}

void UFF_LivePanelQueues::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!IsVisible())
	{
		return;
	}
	TimeSinceRefresh += InDeltaTime;
	if (TimeSinceRefresh < RefreshIntervalSeconds)
	{
		return;
	}
	TimeSinceRefresh = 0.f;
	RefreshAll();
}

void UFF_LivePanelQueues::RefreshAll()
{
	const UGameInstance* GI = GetGameInstance();
	const UFF_FestivalViewModelSubsystem* ViewModel =
		GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;

	if (!ViewModel)
	{
		// 읽기 실패 때 이전 숫자와 행을 남겨 두지 않는다.
		Summary = FFF_LiveQueueSummary();
		Rows.Reset();
		SetTextOrHide(Text_TotalWaiting, MissingValueText);
		SetTextOrHide(Text_InService, MissingValueText);
		SetTextOrHide(Text_AverageWait, MissingValueText);
		SetTextOrHide(Text_AverageNote, CheckingText);
		SetTextOrHide(Text_Longest, CheckingText);
		SetTextOrHide(Text_WorstWait, ExpectedWaitUnknownText);
		SetTextOrHide(Text_StateCounts, MissingValueText);
		RefreshRowWidgets();
		SetTextOrHide(Text_Empty, CheckingText);
		return;
	}

	Rows = ViewModel->GetLiveFacilities();
	Summary = FFF_LiveQueueRules::BuildSummary(Rows);
	FFF_LiveQueueRules::SortByWaiting(Rows);

	SetTextOrHide(Text_TotalWaiting, FText::Format(TeamCountTextFormat,
		FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(Summary.TotalWaiting)}}));
	SetTextOrHide(Text_InService, FText::Format(TeamCountTextFormat,
		FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(Summary.TotalInService)}}));
	SetTextOrHide(Text_AverageWait, Summary.AverageWaitSeconds >= 0
		? FText::Format(WaitSecondsTextFormat,
			FFormatNamedArguments{{TEXT("Seconds"), FText::AsNumber(Summary.AverageWaitSeconds)}})
		: MissingValueText);

	// 고객 수 가중 평균이 아니라, 완료 기록이 있는 시설별 평균의 산술 평균이다.
	SetTextOrHide(Text_AverageNote, Summary.AverageSampleCount > 0
		? FText::Format(AverageNoteTextFormat,
			FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(Summary.AverageSampleCount)}})
		: AverageNoSampleText);

	SetTextOrHide(Text_Longest, Rows.IsEmpty() ? EmptyText : (Summary.HasAnyQueue()
		? FText::Format(LongestTextFormat, FFormatNamedArguments{
			{TEXT("Name"), Summary.LongestQueueName},
			{TEXT("Count"), FText::AsNumber(Summary.LongestQueueWaiting)}})
		: NoQueueText));
	// 가장 많은 팀과 가장 긴 예상 시간은 서로 다른 시설일 수 있다.
	SetTextOrHide(Text_WorstWait, Summary.WorstExpectedWaitSeconds >= 0
		? FText::Format(WorstWaitTextFormat, FFormatNamedArguments{
			{TEXT("Name"), Summary.WorstExpectedWaitName},
			{TEXT("Seconds"), FText::AsNumber(Summary.WorstExpectedWaitSeconds)}})
		: ExpectedWaitUnknownText);
	SetTextOrHide(Text_StateCounts, FText::Format(StateCountsTextFormat, FFormatNamedArguments{
		{TEXT("Open"), FText::AsNumber(Summary.OpenCount)},
		{TEXT("Full"), FText::AsNumber(Summary.FullCount)},
		{TEXT("NotAccepting"), FText::AsNumber(Summary.NotAcceptingCount)},
		{TEXT("Blocked"), FText::AsNumber(Summary.BlockedCount)},
		{TEXT("Unknown"), FText::AsNumber(Summary.UnknownCount)}}));

	RefreshRowWidgets();
	SetTextOrHide(Text_Empty, Rows.IsEmpty() ? EmptyText : FText::GetEmpty());
}

void UFF_LivePanelQueues::RefreshRowWidgets()
{
	if (!Box_Queues)
	{
		return;
	}

	const int32 ShownCount = MaxRows <= 0 ? Rows.Num() : FMath::Min(Rows.Num(), MaxRows);
	bool bAddedChild = false;
	for (int32 Index = 0; Index < ShownCount; ++Index)
	{
		const FFF_LiveFacilityView& Row = Rows[Index];
		if (RowWidgetClass)
		{
			if (!CachedRowWidgets.IsValidIndex(Index))
			{
				UFF_LiveQueueRowWidget* RowWidget = CreateWidget<UFF_LiveQueueRowWidget>(this, RowWidgetClass);
				if (!RowWidget)
				{
					break;
				}
				CachedRowWidgets.Add(RowWidget);
				if (UVerticalBoxSlot* RowSlot = Box_Queues->AddChildToVerticalBox(RowWidget))
				{
					RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
				}
				bAddedChild = true;
			}
			CachedRowWidgets[Index]->SetRowData(Row);
			CachedRowWidgets[Index]->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			continue;
		}

		// 행 WBP가 지정되지 않은 기존 화면도 같은 TextBlock을 재사용한다.
		if (!CachedFallbackLines.IsValidIndex(Index))
		{
			UTextBlock* Line = NewObject<UTextBlock>(this);
			CachedFallbackLines.Add(Line);
			if (UVerticalBoxSlot* LineSlot = Box_Queues->AddChildToVerticalBox(Line))
			{
				LineSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
			}
			bAddedChild = true;
		}
		UpdateFallbackLine(*CachedFallbackLines[Index], Row);
		CachedFallbackLines[Index]->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	for (int32 Index = 0; Index < CachedRowWidgets.Num(); ++Index)
	{
		if (!RowWidgetClass || Index >= ShownCount)
		{
			CachedRowWidgets[Index]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	for (int32 Index = 0; Index < CachedFallbackLines.Num(); ++Index)
	{
		if (RowWidgetClass || Index >= ShownCount)
		{
			CachedFallbackLines[Index]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (Rows.Num() > ShownCount && !MoreRowsText)
	{
		MoreRowsText = NewObject<UTextBlock>(this);
		ApplyDynamicRowStyle(*MoreRowsText, true);
		Box_Queues->AddChildToVerticalBox(MoreRowsText);
	}
	else if (MoreRowsText && bAddedChild)
	{
		MoreRowsText->RemoveFromParent();
		Box_Queues->AddChildToVerticalBox(MoreRowsText);
	}
	SetTextOrHide(MoreRowsText, Rows.Num() > ShownCount
		? FText::Format(MoreRowsTextFormat,
			FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(Rows.Num() - ShownCount)}})
		: FText::GetEmpty());
	Box_Queues->SetVisibility(Rows.IsEmpty()
		? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
}

void UFF_LivePanelQueues::UpdateFallbackLine(UTextBlock& Line, const FFF_LiveFacilityView& Row) const
{
	const FText Expected = Row.ExpectedWaitSeconds >= 0
		? FText::Format(WaitSecondsTextFormat,
			FFormatNamedArguments{{TEXT("Seconds"), FText::AsNumber(Row.ExpectedWaitSeconds)}})
		: MissingValueText;
	const FText Average = Row.AverageWaitSeconds >= 0
		? FText::Format(WaitSecondsTextFormat,
			FFormatNamedArguments{{TEXT("Seconds"), FText::AsNumber(Row.AverageWaitSeconds)}})
		: MissingValueText;
	Line.SetText(FText::Format(
		LOCTEXT("LiveQueue_FallbackRow", "{0}  대기 {1}팀 · 이용 {2}팀 · 지금 서면 {3}  —  {4}"),
		Row.DisplayName, FText::AsNumber(Row.WaitingCount), FText::AsNumber(Row.ActiveServiceCount),
		Expected, UFF_UIDisplayTextLibrary::GetFacilityQueueStateText(Row.QueueState)));
	Line.SetToolTipText(FText::Format(
		LOCTEXT("LiveQueue_FallbackAverage", "완료 고객의 실제 평균 대기: {0}"), Average));
	ApplyDynamicRowStyle(Line);
}

void UFF_LivePanelQueues::SetTextOrHide(UTextBlock* TextBlock, const FText& Value)
{
	if (!TextBlock)
	{
		return;
	}
	TextBlock->SetText(Value);
	TextBlock->SetVisibility(Value.IsEmpty()
		? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
}

#undef LOCTEXT_NAMESPACE
