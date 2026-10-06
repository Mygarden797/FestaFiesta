// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Panels/FF_LivePanelFacilities.h"
#include "UI/LiveHUD/Panels/FF_LiveFacilityRowWidget.h"

#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "UI/FF_UIDisplayTextLibrary.h"
#include "UI/FF_UILog.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLiveFacilities"

// =============================================================================
// FFF_LiveFacilityRules — 순수 함수
// =============================================================================

EFF_LiveFacilityAlert FFF_LiveFacilityRules::ResolveAlert(
	const FFF_LiveFacilityView& View, int32 WaitingThreshold)
{
	// ① 모르는 것을 정상으로 치지 않는다. 화면이 `정상` 이라고 말하면 사람은 안 본다.
	if (View.State == EFF_LiveFacilityState::Unknown)
	{
		return EFF_LiveFacilityAlert::Unknown;
	}

	// ② 자리부터 고쳐야 나머지가 의미 있다. 겹쳐 놓인 시설은 큐 숫자도 못 믿는다.
	if (View.bNeedsRelocation)
	{
		return EFF_LiveFacilityAlert::Blocked;
	}

	// ③ 대기열이 막힌 것은 손님이 아예 못 들어간다는 뜻이다.
	if (View.QueueState == EFF_LiveFacilityQueueState::Blocked)
	{
		return EFF_LiveFacilityAlert::Blocked;
	}

	// ④ 라이브인데 운영 중이 아니면 손님을 못 받는다. `준비 중` 도 여기 걸린다 —
	//    셋업이 끝났는데 아직 안 열린 시설이 그 경우다.
	if (View.State != EFF_LiveFacilityState::Active)
	{
		return EFF_LiveFacilityAlert::Blocked;
	}

	// ⑤ 정원이 찬 것은 지금 당장 못 받는 것이지만 곧 풀린다. 막힘보다는 약하다.
	if (View.QueueState == EFF_LiveFacilityQueueState::Full)
	{
		return EFF_LiveFacilityAlert::Watch;
	}

	// ⑥ 임계가 0 이하면 대기 팀 수을 보지 않는다 — 끄는 방법을 남겨 둔다.
	if (WaitingThreshold > 0 && View.WaitingCount >= WaitingThreshold)
	{
		return EFF_LiveFacilityAlert::Watch;
	}

	return EFF_LiveFacilityAlert::Ok;
}

void FFF_LiveFacilityRules::SortByUrgency(TArray<FFF_LiveFacilityRow>& Rows)
{
	// 급한 순서를 숫자로 매긴다. enum 선언 순서(모름 < 정상 < 주의 < 막힘)에 기대지 않는다 —
	// 항목을 더할 때 순서가 조용히 바뀌면 정렬이 뒤집힌다.
	auto Rank = [](EFF_LiveFacilityAlert Alert)
	{
		switch (Alert)
		{
		case EFF_LiveFacilityAlert::Blocked:	return 3;
		case EFF_LiveFacilityAlert::Watch:		return 2;

		// ★ `모름` 을 `정상` 보다 위에 둔다. 읽지 못한 것은 확인이 필요한 것이지
		//    괜찮다고 확인된 것이 아니다.
		case EFF_LiveFacilityAlert::Unknown:	return 1;
		default:								return 0;
		}
	};

	Rows.Sort([&Rank](const FFF_LiveFacilityRow& A, const FFF_LiveFacilityRow& B)
	{
		const int32 RankA = Rank(A.Alert);
		const int32 RankB = Rank(B.Alert);
		if (RankA != RankB)
		{
			return RankA > RankB;
		}

		if (A.View.WaitingCount != B.View.WaitingCount)
		{
			return A.View.WaitingCount > B.View.WaitingCount;
		}

		// 값이 같으면 이름으로 고정한다. 안 그러면 갱신할 때마다 줄 순서가 흔들려
		// 읽는 중에 눈이 따라가지 못한다.
		return A.View.DisplayName.CompareTo(B.View.DisplayName) < 0;
	});
}

FText FFF_LiveFacilityRules::DescribeAlert(const FFF_LiveFacilityRow& Row)
{
	// !! 색만으로 구분하지 않는다 !! (AT-011) 급한 줄에는 왜 급한지 문구가 붙는다.
	switch (Row.Alert)
	{
	case EFF_LiveFacilityAlert::Blocked:
		if (Row.View.bNeedsRelocation)
		{
			return LOCTEXT("LiveFac_NeedsRelocation", "자리를 다시 잡아야 합니다");
		}
		if (Row.View.QueueState == EFF_LiveFacilityQueueState::Blocked)
		{
			return LOCTEXT("LiveFac_QueueBlocked", "줄이 막혀 손님을 못 받습니다");
		}
		return FText::Format(
			LOCTEXT("LiveFac_NotActive", "운영 중이 아닙니다 ({0})"),
			UFF_UIDisplayTextLibrary::GetFacilityStateText(Row.View.State));

	case EFF_LiveFacilityAlert::Watch:
		if (Row.View.QueueState == EFF_LiveFacilityQueueState::Full)
		{
			return LOCTEXT("LiveFac_QueueFull", "정원이 찼습니다");
		}
		return FText::Format(
			LOCTEXT("LiveFac_LongQueue", "{0}팀이 줄 서 있습니다"),
			FText::AsNumber(Row.View.WaitingCount));

	case EFF_LiveFacilityAlert::Unknown:
		return LOCTEXT("LiveFac_UnknownState", "상태를 읽지 못했습니다");

	default:
		return FText::GetEmpty();
	}
}

// =============================================================================
// UFF_LivePanelFacilities
// =============================================================================

void UFF_LivePanelFacilities::NativeOnPanelOpened()
{
	Super::NativeOnPanelOpened();

	// ★ 여기서 무엇을 할 수 없는지 적는다. 목록만 두면 눌러 볼 수 있다고 오해한다.
	SetTextOrHide(Text_MissingNote, FooterNoteText);

	// 열자마자 한 번 읽는다. 첫 갱신을 주기까지 기다리면 빈 목록이 한 번 보인다.
	TimeSinceRefresh = 0.f;
	RefreshAll();
}

void UFF_LivePanelFacilities::NativeOnPanelClosed()
{
	Rows.Reset();
	Super::NativeOnPanelClosed();
}

void UFF_LivePanelFacilities::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 🔴 **닫혀 있으면 읽지 않는다.** 패널은 캐시되어 살아 있으므로, 이 검사가 없으면
	// 안 보이는 패널이 라이브 내내 시설 전부를 훑는다.
	if (GetVisibility() == ESlateVisibility::Collapsed)
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

void UFF_LivePanelFacilities::RefreshAll()
{
	const UGameInstance* GI = GetGameInstance();
	const UFF_FestivalViewModelSubsystem* ViewModel =
		GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;

	if (!ViewModel)
	{
		Rows.Reset();
		SetTextOrHide(Text_FacilityCount, MissingValueText);
		SetTextOrHide(Text_ActiveCount, MissingValueText);
		SetTextOrHide(Text_AttentionCount, MissingValueText);
		SetTextOrHide(Text_Summary, CheckingText);
		RefreshRowWidgets();
		SetTextOrHide(Text_Empty, CheckingText);
		return;
	}

	const TArray<FFF_LiveFacilityView> Views = ViewModel->GetLiveFacilities();

	Rows.Reset(Views.Num());
	int32 BlockedCount = 0;
	int32 WatchCount = 0;
	int32 UnknownCount = 0;
	int32 ActiveCount = 0;

	for (const FFF_LiveFacilityView& View : Views)
	{
		FFF_LiveFacilityRow& Row = Rows.AddDefaulted_GetRef();
		Row.View = View;
		Row.Alert = FFF_LiveFacilityRules::ResolveAlert(View, WaitingThreshold);
		ActiveCount += View.State == EFF_LiveFacilityState::Active ? 1 : 0;

		if (Row.Alert == EFF_LiveFacilityAlert::Blocked)
		{
			++BlockedCount;
		}
		else if (Row.Alert == EFF_LiveFacilityAlert::Watch)
		{
			++WatchCount;
		}
		else if (Row.Alert == EFF_LiveFacilityAlert::Unknown)
		{
			++UnknownCount;
		}
	}

	FFF_LiveFacilityRules::SortByUrgency(Rows);

	SetTextOrHide(Text_FacilityCount, FText::Format(CountTextFormat,
		FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(Rows.Num())}}));
	SetTextOrHide(Text_ActiveCount, FText::Format(CountTextFormat,
		FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(ActiveCount)}}));
	SetTextOrHide(Text_AttentionCount, FText::Format(CountTextFormat,
		FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(BlockedCount + WatchCount + UnknownCount)}}));

	// 숫자만 두면 "많은 건가" 를 사람이 판단해야 한다. 급한 것이 몇 개인지 함께 적는다.
	SetTextOrHide(Text_Summary, (Rows.Num() == 0)
		? FText::GetEmpty()
		: ((BlockedCount == 0 && WatchCount == 0 && UnknownCount == 0)
			? AllOkText
			: FText::Format(SummaryTextFormat, FFormatNamedArguments{
				{TEXT("Blocked"), FText::AsNumber(BlockedCount)},
				{TEXT("Watch"), FText::AsNumber(WatchCount)},
				{TEXT("Unknown"), FText::AsNumber(UnknownCount)}})));

	RefreshRowWidgets();
	SetTextOrHide(Text_Empty, Rows.IsEmpty() ? EmptyText : FText::GetEmpty());
}

void UFF_LivePanelFacilities::RefreshRowWidgets()
{
	if (!Box_Facilities)
	{
		return;
	}

	const int32 ShownCount = MaxRows <= 0 ? Rows.Num() : FMath::Min(Rows.Num(), MaxRows);
	bool bAddedChild = false;
	for (int32 Index = 0; Index < ShownCount; ++Index)
	{
		const FFF_LiveFacilityRow& Row = Rows[Index];
		if (RowWidgetClass)
		{
			if (!CachedRowWidgets.IsValidIndex(Index))
			{
				UFF_LiveFacilityRowWidget* RowWidget = CreateWidget<UFF_LiveFacilityRowWidget>(this, RowWidgetClass);
				if (!RowWidget)
				{
					break;
				}
				CachedRowWidgets.Add(RowWidget);
				if (UVerticalBoxSlot* RowSlot = Box_Facilities->AddChildToVerticalBox(RowWidget))
				{
					RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
				}
				bAddedChild = true;
			}
			CachedRowWidgets[Index]->SetRowData(Row);
			CachedRowWidgets[Index]->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			continue;
		}

		// 기존 행 WBP가 없는 프로젝트/에셋도 작동하며, 이 경로 역시 같은 TextBlock을 재사용한다.
		if (!CachedFallbackLines.IsValidIndex(Index))
		{
			UTextBlock* Line = NewObject<UTextBlock>(this);
			CachedFallbackLines.Add(Line);
			if (UVerticalBoxSlot* LineSlot = Box_Facilities->AddChildToVerticalBox(Line))
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

	// 행이 추가된 때만 마지막 안내를 뒤로 옮긴다. 매초 Remove/Add나 ClearChildren을 하지 않는다.
	if (Rows.Num() > ShownCount && !MoreRowsText)
	{
		MoreRowsText = NewObject<UTextBlock>(this);
		ApplyDynamicRowStyle(*MoreRowsText, true);
		Box_Facilities->AddChildToVerticalBox(MoreRowsText);
	}
	else if (MoreRowsText && bAddedChild)
	{
		MoreRowsText->RemoveFromParent();
		Box_Facilities->AddChildToVerticalBox(MoreRowsText);
	}
	SetTextOrHide(MoreRowsText, Rows.Num() > ShownCount
		? FText::Format(MoreRowsTextFormat, FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(Rows.Num() - ShownCount)}})
		: FText::GetEmpty());

	Box_Facilities->SetVisibility((Rows.Num() > 0)
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);
}

void UFF_LivePanelFacilities::UpdateFallbackLine(UTextBlock& Line, const FFF_LiveFacilityRow& Row) const
{
	const FText Wait = Row.View.AverageWaitSeconds >= 0
		? FText::Format(LOCTEXT("LiveFac_AvgWait", " · 평균 {0}초"), FText::AsNumber(Row.View.AverageWaitSeconds))
		: FText::GetEmpty();
	const FText Expected = Row.View.ExpectedWaitSeconds >= 0
		? FText::Format(LOCTEXT("LiveFac_ExpectedWait", " · 지금 서면 {0}초"), FText::AsNumber(Row.View.ExpectedWaitSeconds))
		: FText::GetEmpty();
	const FText Reason = FFF_LiveFacilityRules::DescribeAlert(Row);
	const FText ReasonSuffix = Reason.IsEmpty()
		? FText::GetEmpty()
		: FText::Format(LOCTEXT("LiveFac_ReasonSuffix", "  —  {0}"), Reason);
	Line.SetText(FText::Format(LOCTEXT("LiveFac_Row", "{0}  대기 {1}팀 · 이용 {2}팀{3}{4}{5}"),
		Row.View.DisplayName, FText::AsNumber(Row.View.WaitingCount), FText::AsNumber(Row.View.ActiveServiceCount),
		Wait, Expected, ReasonSuffix));
	ApplyDynamicRowStyle(Line);
}

void UFF_LivePanelFacilities::SetTextOrHide(UTextBlock* TextBlock, const FText& Value)
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
