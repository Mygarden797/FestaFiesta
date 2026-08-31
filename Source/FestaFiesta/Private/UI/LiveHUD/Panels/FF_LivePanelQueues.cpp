// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Panels/FF_LivePanelQueues.h"

#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "UI/FF_UIDisplayTextLibrary.h"
#include "UI/FF_UILog.h"

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
	}

	if (Result.AverageSampleCount > 0)
	{
		Result.AverageWaitSeconds =
			static_cast<int32>(WaitSecondsSum / Result.AverageSampleCount);
	}

	// 🔴 줄이 하나도 없는 것과 시설이 없는 것을 구분한다. 대기 0 명인 시설만 있으면
	//    `가장 긴 줄` 은 0 명짜리가 되는데, 그것을 이름과 함께 띄우면 거기가 붐비는
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

	// ★ 아직 못 보여주는 것을 적는다. 안 적으면 빠진 줄 모른다.
	SetTextOrHide(Text_MissingNote, LOCTEXT("LiveQueue_MissingNote",
		"예상 대기 시간과 줄을 포기한 손님 수는 아직 읽을 수 없습니다."));

	// 열자마자 한 번 읽는다. 첫 갱신을 주기까지 기다리면 빈 목록이 한 번 보인다.
	TimeSinceRefresh = 0.f;
	RefreshAll();
}

void UFF_LivePanelQueues::NativeOnPanelClosed()
{
	Summary = FFF_LiveQueueSummary();
	Super::NativeOnPanelClosed();
}

void UFF_LivePanelQueues::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
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

void UFF_LivePanelQueues::RefreshAll()
{
	const UGameInstance* GI = GetGameInstance();
	const UFF_FestivalViewModelSubsystem* ViewModel =
		GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;

	if (!ViewModel)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] ViewModel 을 찾지 못했다. 대기열이 비어 보인다."), *GetName());
		return;
	}

	TArray<FFF_LiveFacilityView> Facilities = ViewModel->GetLiveFacilities();
	Summary = FFF_LiveQueueRules::BuildSummary(Facilities);
	FFF_LiveQueueRules::SortByWaiting(Facilities);

	// --- 요약 ---
	SetTextOrHide(Text_TotalWaiting, FText::Format(
		LOCTEXT("LiveQueue_TotalWaiting", "{0}명"), FText::AsNumber(Summary.TotalWaiting)));

	SetTextOrHide(Text_InService, FText::Format(
		LOCTEXT("LiveQueue_InService", "{0}명"), FText::AsNumber(Summary.TotalInService)));

	SetTextOrHide(Text_AverageWait, (Summary.AverageWaitSeconds != INDEX_NONE)
		? FText::Format(LOCTEXT("LiveQueue_AvgWait", "{0}초"), FText::AsNumber(Summary.AverageWaitSeconds))
		: LOCTEXT("LiveQueue_AvgUnknown", "—"));

	// 한 곳만 센 평균과 여섯 곳을 센 평균은 뜻이 다르다. 몇 곳을 셌는지 함께 적는다.
	SetTextOrHide(Text_AverageNote, (Summary.AverageSampleCount > 0)
		? FText::Format(
			LOCTEXT("LiveQueue_AvgNote", "줄을 끝낸 손님이 있는 {0}곳 기준"),
			FText::AsNumber(Summary.AverageSampleCount))
		: LOCTEXT("LiveQueue_AvgNoSample", "아직 줄을 끝낸 손님이 없어 평균을 낼 수 없습니다."));

	SetTextOrHide(Text_Longest, Summary.HasAnyQueue()
		? FText::Format(
			LOCTEXT("LiveQueue_Longest", "가장 긴 줄 · {0} {1}명"),
			Summary.LongestQueueName, FText::AsNumber(Summary.LongestQueueWaiting))
		: LOCTEXT("LiveQueue_NoQueue", "줄 서 있는 손님이 없습니다."));

	// 모름은 따로 적는다. 닫힘에 섞으면 못 읽은 것이 정상적으로 닫힌 것처럼 보인다.
	const FText UnknownSuffix = (Summary.UnknownCount > 0)
		? FText::Format(
			LOCTEXT("LiveQueue_UnknownSuffix", " · 모름 {0}"), FText::AsNumber(Summary.UnknownCount))
		: FText::GetEmpty();

	SetTextOrHide(Text_StateCounts, FText::Format(
		LOCTEXT("LiveQueue_StateCounts", "받는 중 {0} · 정원 참 {1} · 안 받음 {2} · 막힘 {3}{4}"),
		FText::AsNumber(Summary.OpenCount),
		FText::AsNumber(Summary.FullCount),
		FText::AsNumber(Summary.NotAcceptingCount),
		FText::AsNumber(Summary.BlockedCount),
		UnknownSuffix));

	// --- 목록 ---
	if (!Box_Queues)
	{
		return;
	}

	Box_Queues->ClearChildren();

	const int32 ShownCount = FMath::Min(Facilities.Num(), FMath::Max(1, MaxRows));
	for (int32 Index = 0; Index < ShownCount; ++Index)
	{
		const FFF_LiveFacilityView& Facility = Facilities[Index];

		UTextBlock* Line = NewObject<UTextBlock>(this);
		if (!Line)
		{
			continue;
		}

		const FText Wait = (Facility.AverageWaitSeconds != INDEX_NONE)
			? FText::Format(
				LOCTEXT("LiveQueue_RowWait", " · 평균 {0}초"),
				FText::AsNumber(Facility.AverageWaitSeconds))
			: FText::GetEmpty();

		// !! 숫자만 두지 않는다 !! 대기 0 명이 "한산하다" 인지 "닫혀 있다" 인지
		// 숫자로는 알 수 없다 (AT-011).
		Line->SetText(FText::Format(
			LOCTEXT("LiveQueue_Row", "{0}  대기 {1} · 이용 {2}{3}  —  {4}"),
			Facility.DisplayName,
			FText::AsNumber(Facility.WaitingCount),
			FText::AsNumber(Facility.ActiveServiceCount),
			Wait,
			UFF_UIDisplayTextLibrary::GetFacilityQueueStateText(Facility.QueueState)));
		Line->SetAutoWrapText(true);

		if (UVerticalBoxSlot* LineSlot = Box_Queues->AddChildToVerticalBox(Line))
		{
			LineSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}
	}

	// 🔴 잘린 줄이 있으면 반드시 적는다. 안 적으면 목록이 전부인 줄 안다.
	if (Facilities.Num() > ShownCount)
	{
		if (UTextBlock* More = NewObject<UTextBlock>(this))
		{
			More->SetText(FText::Format(
				LOCTEXT("LiveQueue_More", "…외 {0}개는 목록에 없습니다"),
				FText::AsNumber(Facilities.Num() - ShownCount)));
			Box_Queues->AddChildToVerticalBox(More);
		}
	}

	Box_Queues->SetVisibility((Facilities.Num() > 0)
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);

	SetTextOrHide(Text_Empty, (Facilities.Num() > 0)
		? FText::GetEmpty()
		: LOCTEXT("LiveQueue_Empty", "줄을 세울 시설이 없습니다."));
}

void UFF_LivePanelQueues::SetTextOrHide(UTextBlock* TextBlock, const FText& Value)
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
