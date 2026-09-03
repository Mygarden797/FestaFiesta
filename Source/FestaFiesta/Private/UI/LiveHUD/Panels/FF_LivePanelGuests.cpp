// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Panels/FF_LivePanelGuests.h"

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
	if (SampleCount <= 0)
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

		if (!FMath::IsNearlyEqual(A.View.UnmetRatioPercent, B.View.UnmetRatioPercent))
		{
			return A.View.UnmetRatioPercent > B.View.UnmetRatioPercent;
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

	// ★ 아직 못 보여주는 것을 적는다.
	SetTextOrHide(Text_MissingNote, LOCTEXT("LiveGuests_MissingNote",
		"관객 그룹 구성과 오늘 누적 방문객 수는 아직 읽을 수 없습니다."));

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

	if (!ViewModel)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] ViewModel 을 찾지 못했다. 관객 정보가 비어 보인다."), *GetName());
		return;
	}

	// --- 인원 ---
	const int32 Visitors = ViewModel->GetConcurrentVisitorCount();
	SetTextOrHide(Text_VisitorCount, (Visitors != INDEX_NONE)
		? FText::Format(LOCTEXT("LiveGuests_Visitors", "{0}명"), FText::AsNumber(Visitors))
		: LOCTEXT("LiveGuests_VisitorsUnknown", "—"));

	// ★ 동시 인원임을 밝힌다. 일일 목표(누적)와 다른 값이라 그냥 두면 목표 진행과
	//   숫자가 안 맞는 것으로 오해한다.
	SetTextOrHide(Text_VisitorNote, LOCTEXT("LiveGuests_VisitorNote",
		"지금 축제장 안에 있는 인원입니다. 오늘 누적 방문객과는 다릅니다."));

	// --- 욕구 ---
	const TArray<FFF_LiveNeedView> Needs = ViewModel->GetNeedMetrics();
	const int32 SampleCount = ViewModel->GetNeedSampleCount();

	Rows.Reset(Needs.Num());
	int32 BadCount = 0;
	int32 WatchCount = 0;

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
	}

	FFF_LiveGuestRules::SortBySeverity(Rows);

	// 🔴 표본이 없으면 그 사실을 크게 적는다. 숫자만 두면 전부 `0%` 로 보이고,
	//    그것은 "아무도 불편하지 않다" 로 읽힌다.
	SetTextOrHide(Text_Summary, (SampleCount <= 0)
		? LOCTEXT("LiveGuests_NoSample",
			"아직 관객 표본이 없어 욕구를 읽지 못했습니다. 아래 값은 실제가 아닙니다.")
		: ((BadCount == 0 && WatchCount == 0)
			? FText::Format(
				LOCTEXT("LiveGuests_AllOk", "관객 {0}명 기준 · 급한 불편은 없습니다."),
				FText::AsNumber(SampleCount))
			: FText::Format(
				LOCTEXT("LiveGuests_Summary", "관객 {0}명 기준 · 나쁨 {1} · 주의 {2}"),
				FText::AsNumber(SampleCount),
				FText::AsNumber(BadCount), FText::AsNumber(WatchCount))));

	// --- 목록 ---
	if (!Box_Needs)
	{
		return;
	}

	Box_Needs->ClearChildren();

	for (const FFF_LiveNeedRow& Row : Rows)
	{
		UTextBlock* Line = NewObject<UTextBlock>(this);
		if (!Line)
		{
			continue;
		}

		const FText Name = UFF_UIDisplayTextLibrary::GetNeedTypeText(Row.View.NeedType);

		if (Row.Severity == EFF_LiveNeedSeverity::Unknown)
		{
			// 값을 지어내지 않는다. 못 읽었다고만 적는다.
			Line->SetText(FText::Format(
				LOCTEXT("LiveGuests_RowUnknown", "{0}  —  아직 읽지 못했습니다"), Name));
		}
		else
		{
			// 🔴 숫자 옆에 **할 일**을 붙인다. `배고픔 62%` 만 두면 무엇을 해야 할지 모른다.
			//    괜찮은 줄에는 안 붙인다 — 전부에 붙으면 급한 줄이 안 도드라진다.
			const FText Remedy = (Row.Severity == EFF_LiveNeedSeverity::Ok)
				? FText::GetEmpty()
				: FText::Format(
					LOCTEXT("LiveGuests_RowRemedy", "  —  {0}"),
					UFF_UIDisplayTextLibrary::GetNeedRemedyText(Row.View.NeedType));

			Line->SetText(FText::Format(
				LOCTEXT("LiveGuests_Row", "{0}  {1}%{2}"),
				Name,
				FText::AsNumber(FMath::RoundToInt(Row.View.UnmetRatioPercent)),
				Remedy));
		}
		Line->SetAutoWrapText(true);

		if (UVerticalBoxSlot* LineSlot = Box_Needs->AddChildToVerticalBox(Line))
		{
			LineSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}
	}

	Box_Needs->SetVisibility((Rows.Num() > 0)
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);

	SetTextOrHide(Text_Empty, (Rows.Num() > 0)
		? FText::GetEmpty()
		: LOCTEXT("LiveGuests_Empty", "욕구 집계를 읽을 수 없습니다."));
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
