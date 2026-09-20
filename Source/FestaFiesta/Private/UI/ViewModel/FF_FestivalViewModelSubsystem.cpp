// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

#include "Building/FF_FacilityNamingLibrary.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Economy/FF_StageEconomyBridgeSubsystem.h"
#include "Economy/Rules/FF_ProgramScheduleRules.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSWorldSubsystem.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "NPC/Insights/FF_CrowdInsightsSubsystem.h"
#include "NPC/Metrics/FF_NpcNeedMetricsSubsystem.h"
#include "NPC/Metrics/FF_SatisfactionMetricsSubsystem.h"
#include "Queue/FF_QueueSubsystem.h"
#include "Queue/FF_QueueTypes.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"
#include "UI/FF_UILog.h"
#include "UI/FF_UIDisplayTextLibrary.h"

void UFF_FestivalViewModelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 같은 GameInstance 안의 서브시스템이라 순서가 보장되지 않는다.
	// 지금 없으면 Tick에서 다시 시도한다.
	TryBindStageSubsystem();
	TryBindProgressionSubsystem();
	TryBindSatisfactionSubsystem();
	TryBindInsightsSubsystem();
}

void UFF_FestivalViewModelSubsystem::Deinitialize()
{
	// 해제는 조건부, 정리는 무조건. 조기 return으로 빠져나가지 않는다.
	if (CachedStageSubsystem)
	{
		CachedStageSubsystem->OnStageChanged.RemoveDynamic(this, &UFF_FestivalViewModelSubsystem::HandleStageChanged);
		CachedStageSubsystem->OnRemainingSecondsChanged.RemoveDynamic(this, &UFF_FestivalViewModelSubsystem::HandleRemainingSecondsChanged);
		CachedStageSubsystem->OnEnvironmentStateChanged.RemoveDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerEnvironmentChanged);
		CachedStageSubsystem->OnMainProgramStarted.RemoveDynamic(this, &UFF_FestivalViewModelSubsystem::HandleMainProgramStarted);
		CachedStageSubsystem->OnMainProgramEnded.RemoveDynamic(this, &UFF_FestivalViewModelSubsystem::HandleMainProgramEnded);
	}
	CachedStageSubsystem = nullptr;

	if (CachedProgressionSubsystem)
	{
		CachedProgressionSubsystem->OnGoldChanged.RemoveDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerGoldChanged);
		CachedProgressionSubsystem->OnTotalRevenueChanged.RemoveDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerTotalRevenueChanged);
		CachedProgressionSubsystem->OnTodayRevenueChanged.RemoveDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerTodayRevenueChanged);
		CachedProgressionSubsystem->OnExpectedVisitorsChanged.RemoveDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerExpectedVisitorsChanged);
		CachedProgressionSubsystem->OnTotalArrearsChanged.RemoveDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerArrearsChanged);
		CachedProgressionSubsystem->OnTotalStaffCountChanged.RemoveDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerStaffCountChanged);
		CachedProgressionSubsystem->OnScheduledProgramsChanged.RemoveDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerProgramScheduleChanged);
		CachedProgressionSubsystem->OnFinaleGradeChanged.RemoveDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerFinaleGradeChanged);
		CachedProgressionSubsystem->OnPromotionGradeChanged.RemoveDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerPromotionGradeChanged);
	}
	CachedProgressionSubsystem = nullptr;

	if (CachedSatisfactionSubsystem)
	{
		CachedSatisfactionSubsystem->OnSimSatisfactionChanged.RemoveDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerSimSatisfactionChanged);
	}
	CachedSatisfactionSubsystem = nullptr;

	if (CachedInsightsSubsystem)
	{
		CachedInsightsSubsystem->OnSnsPostAdded.RemoveDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerSnsPostAdded);
	}
	CachedInsightsSubsystem = nullptr;

	Super::Deinitialize();
}

#if !UE_BUILD_SHIPPING
namespace FFFestivalViewModelConsole
{
// Console names are process-wide, whereas ViewModels belong to individual game
// instances. Register once for the module and resolve the caller's world only
// when executing; overlapping PIE/test worlds must not replace each other's handles.
UFF_FestivalViewModelSubsystem* ResolveViewModel(UWorld* World)
{
	UGameInstance* GameInstance = IsValid(World) && !World->bIsTearingDown
		? World->GetGameInstance() : nullptr;
	return IsValid(GameInstance) ? GameInstance->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;
}

void SetMetrics(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 3)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("FF.SetMetrics <Sim> <Score> <Safety> — 인자 3개가 필요하다. 받은 것: %d개"), Args.Num());
		return;
	}
	if (UFF_FestivalViewModelSubsystem* ViewModel = ResolveViewModel(World))
	{
		ViewModel->Debug_SetMetrics(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]), FCString::Atoi(*Args[2]));
		ViewModel->Debug_DumpToLog();
	}
}

void SetEconomy(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 3)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("FF.SetEconomy <Gold> <TotalRevenue> <TodayRevenue> — 인자 3개가 필요하다. 받은 것: %d개"), Args.Num());
		return;
	}
	if (UFF_FestivalViewModelSubsystem* ViewModel = ResolveViewModel(World))
	{
		ViewModel->Debug_SetEconomy(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]), FCString::Atoi(*Args[2]));
		ViewModel->Debug_DumpToLog();
	}
}

void DumpMetrics(UWorld* World)
{
	if (UFF_FestivalViewModelSubsystem* ViewModel = ResolveViewModel(World))
	{
		ViewModel->Debug_DumpToLog();
	}
}

static FAutoConsoleCommandWithWorldAndArgs SetMetricsCommand(
		TEXT("FF.SetMetrics"),
		TEXT("0~100 지표를 설정한다. 사용법: FF.SetMetrics <SimSatisfaction> <ScoreSatisfaction> <Safety>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetMetrics),
		ECVF_Cheat);

static FAutoConsoleCommandWithWorldAndArgs SetEconomyCommand(
		TEXT("FF.SetEconomy"),
		TEXT("경제 값을 설정한다. 사용법: FF.SetEconomy <Gold> <TotalRevenue> <TodayRevenue>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetEconomy),
		ECVF_Cheat);

static FAutoConsoleCommandWithWorld DumpMetricsCommand(
		TEXT("FF.DumpMetrics"),
		TEXT("현재 지표·경제·Stage 값을 로그에 찍는다."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&DumpMetrics),
		ECVF_Cheat);
}
#endif

UFF_FestivalStageSubsystem* UFF_FestivalViewModelSubsystem::GetStageSubsystem() const
{
	if (CachedStageSubsystem)
	{
		return CachedStageSubsystem;
	}

	if (const UGameInstance* GI = GetGameInstance())
	{
		return GI->GetSubsystem<UFF_FestivalStageSubsystem>();
	}
	return nullptr;
}

void UFF_FestivalViewModelSubsystem::TryBindStageSubsystem()
{
	if (CachedStageSubsystem)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	UFF_FestivalStageSubsystem* StageSubsystem = GI->GetSubsystem<UFF_FestivalStageSubsystem>();
	if (!StageSubsystem)
	{
		++StageBindRetryCount;
		if (StageBindRetryCount == MaxStageBindRetry)
		{
			// 조용히 재시도만 반복하지 않는다. 상한에 닿으면 한 번 남긴다.
			UE_LOG(LogFestaFiestaUI, Error,
				TEXT("[ViewModel] UFF_FestivalStageSubsystem을 %d회 시도해도 찾지 못했다. Stage 표시가 동작하지 않는다."),
				MaxStageBindRetry);
		}
		return;
	}

	CachedStageSubsystem = StageSubsystem;

	// Bind + Sync — 구독하고 나서 현재값을 즉시 반영한다.
	// 이벤트는 앞으로의 변화만 알려주므로, 이미 지나간 현재 Stage를 놓치지 않으려면 필요하다.
	CachedStageSubsystem->OnStageChanged.AddUniqueDynamic(this, &UFF_FestivalViewModelSubsystem::HandleStageChanged);
	CachedStageSubsystem->OnRemainingSecondsChanged.AddUniqueDynamic(this, &UFF_FestivalViewModelSubsystem::HandleRemainingSecondsChanged);
	CachedStageSubsystem->OnEnvironmentStateChanged.AddUniqueDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerEnvironmentChanged);

	// 🔴 둘 다 받는다. 시작만 받으면 공연이 끝나도 화면은 계속 `잠김` 으로 남는다.
	CachedStageSubsystem->OnMainProgramStarted.AddUniqueDynamic(this, &UFF_FestivalViewModelSubsystem::HandleMainProgramStarted);
	CachedStageSubsystem->OnMainProgramEnded.AddUniqueDynamic(this, &UFF_FestivalViewModelSubsystem::HandleMainProgramEnded);

	OnStageViewChanged.Broadcast(GetCurrentStage(), GetCurrentDay());
	BroadcastRemainingSecondsIfChanged(FMath::CeilToInt(GetRemainingSeconds()));

	// 날씨는 방송을 놓쳤을 수 있으니 현재값을 한 번 반영한다. 중복이면 아무 일도 하지 않는다.
	LastBroadcastWeather = GetSelectedWeather();

	UE_LOG(LogFestaFiestaUI, Log,
		TEXT("[ViewModel] Stage 시스템 구독 완료. 현재 Stage=%s Day=%d 남은=%.1f초"),
		*GetCurrentStageDisplayText().ToString(), GetCurrentDay(), GetRemainingSeconds());
}

UFF_FestivalProgressionSubsystem* UFF_FestivalViewModelSubsystem::GetProgressionSubsystem() const
{
	if (CachedProgressionSubsystem)
	{
		return CachedProgressionSubsystem;
	}

	if (const UGameInstance* GI = GetGameInstance())
	{
		return GI->GetSubsystem<UFF_FestivalProgressionSubsystem>();
	}
	return nullptr;
}

UFF_StageEconomyBridgeSubsystem* UFF_FestivalViewModelSubsystem::GetStageEconomyBridgeSubsystem() const
{
	// 캐시하지 않는다. 구독할 델리게이트가 없고 판정 두 개만 읽는 창구라
	// 수명 관리를 늘릴 이유가 없다.
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UFF_StageEconomyBridgeSubsystem>() : nullptr;
}

void UFF_FestivalViewModelSubsystem::TryBindProgressionSubsystem()
{
	if (CachedProgressionSubsystem)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	UFF_FestivalProgressionSubsystem* Progression = GI->GetSubsystem<UFF_FestivalProgressionSubsystem>();
	if (!Progression)
	{
		++ProgressionBindRetryCount;
		if (ProgressionBindRetryCount == MaxStageBindRetry)
		{
			UE_LOG(LogFestaFiestaUI, Error,
				TEXT("[ViewModel] UFF_FestivalProgressionSubsystem을 %d회 시도해도 찾지 못했다. Gold가 더미로 남는다."),
				MaxStageBindRetry);
		}
		return;
	}

	CachedProgressionSubsystem = Progression;

	// Bind + Sync. StartFestival이 이미 지나갔다면 방송을 놓쳤으므로 현재값을 직접 읽는다.
	CachedProgressionSubsystem->OnGoldChanged.AddUniqueDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerGoldChanged);

	// 🔴 매출 둘은 2026-09-02 까지 **구독이 없어서 화면이 영원히 `0`** 이었다.
	// 소유자에는 값도 델리게이트도 처음부터 있었는데 ViewModel 이 안 받고 있었다.
	CachedProgressionSubsystem->OnTotalRevenueChanged.AddUniqueDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerTotalRevenueChanged);
	CachedProgressionSubsystem->OnTodayRevenueChanged.AddUniqueDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerTodayRevenueChanged);
	CachedProgressionSubsystem->OnExpectedVisitorsChanged.AddUniqueDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerExpectedVisitorsChanged);
	CachedProgressionSubsystem->OnTotalArrearsChanged.AddUniqueDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerArrearsChanged);
	CachedProgressionSubsystem->OnTotalStaffCountChanged.AddUniqueDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerStaffCountChanged);
	CachedProgressionSubsystem->OnScheduledProgramsChanged.AddUniqueDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerProgramScheduleChanged);
	CachedProgressionSubsystem->OnFinaleGradeChanged.AddUniqueDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerFinaleGradeChanged);
	CachedProgressionSubsystem->OnPromotionGradeChanged.AddUniqueDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerPromotionGradeChanged);

	// 구독 전에 이미 샀을 수 있다. 현재값을 한 번 맞춘다.
	PurchasedPromotion = ToPromotionChoice(CachedProgressionSubsystem->GetPurchasedPromotionGrade());

	// !! 축제가 시작되기 전에는 소유자 값을 쓰지 않는다 !!
	// StartFestival 전 Funds는 0이다. 그 0을 화면에 띄우면 "잔액이 없다"는 틀린 사실이 되고,
	// 검증 중에는 HUD가 고장난 것처럼 보인다. 시작되면 OnGoldChanged가 실제 값을 가져온다.
	if (CachedProgressionSubsystem->IsFestivalStarted())
	{
		HandleOwnerGoldChanged(CachedProgressionSubsystem->GetGold());
		HandleOwnerTotalRevenueChanged(CachedProgressionSubsystem->GetTotalRevenue());
		HandleOwnerTodayRevenueChanged(CachedProgressionSubsystem->GetTodayRevenue());
		HandleOwnerExpectedVisitorsChanged(CachedProgressionSubsystem->GetExpectedVisitors());
		HandleOwnerArrearsChanged(CachedProgressionSubsystem->GetTotalArrears());
		HandleOwnerStaffCountChanged(CachedProgressionSubsystem->GetTotalStaffCount());

		// 피날레는 이어하기로 들어오면 이미 사 둔 상태일 수 있다. 방송을 놓쳤으므로 현재값을
		// 한 번 반영한다. 아직 `None` 이면 중복 차단에 걸려 아무것도 쏘지 않는다.
		HandleOwnerFinaleGradeChanged(CachedProgressionSubsystem->GetPurchasedFinaleGrade());
	}
	else
	{
		UE_LOG(LogFestaFiestaUI, Log,
			TEXT("[ViewModel] 경제 시스템 구독 완료. 축제가 아직 시작되지 않아 Gold는 더미 %d를 유지한다."), Gold);
	}
}

void UFF_FestivalViewModelSubsystem::HandleOwnerGoldChanged(int32 NewValue)
{
	// 소유자 값이 들어오는 순간부터 더미는 의미가 없다. 변화 감지는 ApplyIntMetric이 한다.
	ApplyIntMetric(Gold, NewValue, OnGoldChanged);
}

void UFF_FestivalViewModelSubsystem::TryBindSatisfactionSubsystem()
{
	if (CachedSatisfactionSubsystem)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	UFF_SatisfactionMetricsSubsystem* Satisfaction = GI->GetSubsystem<UFF_SatisfactionMetricsSubsystem>();
	if (!Satisfaction)
	{
		++SatisfactionBindRetryCount;
		if (SatisfactionBindRetryCount == MaxStageBindRetry)
		{
			UE_LOG(LogFestaFiestaUI, Error,
				TEXT("[ViewModel] UFF_SatisfactionMetricsSubsystem을 %d회 시도해도 찾지 못했다. SimSatisfaction이 중립값으로 남는다."),
				MaxStageBindRetry);
		}
		return;
	}

	CachedSatisfactionSubsystem = Satisfaction;

	// Bind + Sync. 구독 전에 이미 한 번 이상 확정됐을 수 있으므로 현재값을 바로 반영한다 —
	// Gold와 다르게 "축제 시작 전엔 안 쓴다" 게이트는 없다. 만족도 중립값(50)은 Gold의
	// 더미 0과 달리 그 자체로 "아직 데이터 없음"을 오도하지 않는 값이기 때문이다.
	CachedSatisfactionSubsystem->OnSimSatisfactionChanged.AddUniqueDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerSimSatisfactionChanged);
	HandleOwnerSimSatisfactionChanged(CachedSatisfactionSubsystem->GetSimSatisfaction());
}

void UFF_FestivalViewModelSubsystem::HandleOwnerSimSatisfactionChanged(int32 NewValue)
{
	ApplyIntMetric(SimSatisfaction, NewValue, OnSimSatisfactionChanged);

	// 분해값 4종도 같은 확정 주기에 나온 값이라 여기서 같이 읽는다 — 소유자가 델리게이트를
	// 따로 넷 더 쏘게 만들지 않는다. 이 핸들러가 불렸다는 것 자체가 CachedSatisfactionSubsystem이
	// 존재한다는 뜻이다(구독도 그 포인터로 걸었고, Bind + Sync 호출도 그 직후에만 한다).
	if (CachedSatisfactionSubsystem)
	{
		ApplyIntMetric(SimSatisfactionWaitScore, CachedSatisfactionSubsystem->GetWaitScore(), OnSimSatisfactionWaitScoreChanged);
		ApplyIntMetric(SimSatisfactionDensityScore, CachedSatisfactionSubsystem->GetDensityScore(), OnSimSatisfactionDensityScoreChanged);
		ApplyIntMetric(SimSatisfactionPreferenceScore, CachedSatisfactionSubsystem->GetPreferenceScore(), OnSimSatisfactionPreferenceScoreChanged);
		ApplyIntMetric(SimSatisfactionNeedFulfillmentScore, CachedSatisfactionSubsystem->GetNeedFulfillmentScore(), OnSimSatisfactionNeedFulfillmentScoreChanged);
	}

	RecomputeScoreSatisfaction();
}

void UFF_FestivalViewModelSubsystem::TryBindInsightsSubsystem()
{
	if (CachedInsightsSubsystem)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	UFF_CrowdInsightsSubsystem* Insights = GI->GetSubsystem<UFF_CrowdInsightsSubsystem>();
	if (!Insights)
	{
		++InsightsBindRetryCount;
		if (InsightsBindRetryCount == MaxStageBindRetry)
		{
			UE_LOG(LogFestaFiestaUI, Error,
				TEXT("[ViewModel] UFF_CrowdInsightsSubsystem을 %d회 시도해도 찾지 못했다. SNS 피드가 비어 있다."),
				MaxStageBindRetry);
		}
		return;
	}

	CachedInsightsSubsystem = Insights;
	CachedInsightsSubsystem->OnSnsPostAdded.AddUniqueDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerSnsPostAdded);

	// SimSatisfaction과 다르게 "현재값 1개"가 아니라 "지금까지의 글 목록"이라, bind 시점에
	// 이미 쌓여 있던 글도 놓치지 않게 전부 변환해 채운다.
	RecentSnsPosts.Empty();
	for (const FFF_SnsPostRow& Row : CachedInsightsSubsystem->GetRecentPosts())
	{
		RecentSnsPosts.Add(ConvertSnsPostToLiveSocialPost(Row));
	}
}

void UFF_FestivalViewModelSubsystem::HandleOwnerSnsPostAdded(FFF_SnsPostRow NewPost)
{
	const FFF_LiveSocialPost Converted = ConvertSnsPostToLiveSocialPost(NewPost);

	RecentSnsPosts.Insert(Converted, 0);
	if (RecentSnsPosts.Num() > SnsPostViewCapacity)
	{
		RecentSnsPosts.SetNum(SnsPostViewCapacity);
	}

	OnSnsPostAdded.Broadcast(Converted);
}

FFF_LiveSocialPost UFF_FestivalViewModelSubsystem::ConvertSnsPostToLiveSocialPost(const FFF_SnsPostRow& Row)
{
	FFF_LiveSocialPost Converted;
	Converted.Author = FText::FromString(Row.DisplayHandle);
	Converted.Body = Row.Body;

	// TopicSalience0To100은 "이 화제를 고른 신호가 얼마나 강했는지"다 — 임의 상수가 아니라
	// 화제 선택에 실제로 쓰인 값을 그대로 세기로 재사용한다(utility_ai_todo.md §11).
	const int32 Magnitude = FMath::RoundToInt(Row.TopicSalience0To100);
	Converted.SentimentScore = (Row.Sentiment == EFF_SnsSentiment::Positive) ? Magnitude : -Magnitude;

	return Converted;
}

void UFF_FestivalViewModelSubsystem::HandleOwnerTotalRevenueChanged(int32 NewValue)
{
	ApplyIntMetric(TotalRevenue, NewValue, OnTotalRevenueChanged);
}

void UFF_FestivalViewModelSubsystem::HandleOwnerTodayRevenueChanged(int32 NewValue)
{
	ApplyIntMetric(TodayRevenue, NewValue, OnTodayRevenueChanged);
}

void UFF_FestivalViewModelSubsystem::HandleOwnerExpectedVisitorsChanged(int32 NewValue)
{
	// 🔴 소유자는 정의가 없을 때 `INDEX_NONE` 을 준다. 그대로 넘긴다 —
	// 0 으로 바꾸면 `아무도 안 온다` 는 확정 사실이 된다.
	ApplyIntMetric(ExpectedVisitors, NewValue, OnExpectedVisitorsChanged);
}

void UFF_FestivalViewModelSubsystem::HandleOwnerArrearsChanged(int32 NewValue)
{
	ApplyIntMetric(Arrears, NewValue, OnArrearsChanged);
}

void UFF_FestivalViewModelSubsystem::HandleOwnerStaffCountChanged(int32 NewValue)
{
	// 내역을 먼저 채운 뒤 총원을 방송한다. 순서가 반대면 위젯이 총원 방송을 받고 다시 그릴 때
	// 기본·알바가 아직 이전 값이라 "총 6명 · 기본 4 + 알바 0" 같은 한 프레임이 나온다.
	if (const UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem())
	{
		BaseStaffCount = Progression->GetBaseStaffCount();
		TemporaryStaffCount = Progression->GetTemporaryStaffCount();
	}

	ApplyIntMetric(TotalStaffCount, NewValue, OnTotalStaffCountChanged);

	// 알바를 고용하면 편성이 그대로여도 쓸 수 있는 사람이 늘어난다.
	// 편성 델리게이트는 이때 울리지 않으므로 여기서 함께 갱신해야 한다.
	RefreshAvailableStaffCount();
}

void UFF_FestivalViewModelSubsystem::HandleOwnerProgramScheduleChanged()
{
	// 가용 인원을 **먼저** 맞춘 뒤 편성 변경을 알린다. 순서가 반대면 프로그램 패널이
	// 편성 방송을 받고 다시 그릴 때 인원이 아직 이전 값이라 한 프레임 어긋난다.
	RefreshAvailableStaffCount();
	OnScheduledProgramsChanged.Broadcast();
}

void UFF_FestivalViewModelSubsystem::RefreshAvailableStaffCount()
{
	const UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();

	// 소유자 쪽 `GetAvailableStaffCount()` 에는 시작 전 가드가 없어 0 을 돌려준다.
	// 그 0 을 그대로 쓰면 "쓸 사람이 없다"는 확정 사실이 되므로 여기서 `모름`으로 올린다.
	const int32 NewValue = (Progression && Progression->IsFestivalStarted())
		? Progression->GetAvailableStaffCount()
		: INDEX_NONE;

	ApplyIntMetric(AvailableStaffCount, NewValue, OnAvailableStaffCountChanged);
}

int32 UFF_FestivalViewModelSubsystem::GetStaffAssignedToPrograms() const
{
	const UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	if (!Progression || !Progression->IsFestivalStarted())
	{
		return INDEX_NONE;
	}

	return Progression->GetStaffAssignedToPrograms();
}

EFF_WeatherType UFF_FestivalViewModelSubsystem::GetSelectedWeather() const
{
	const UFF_FestivalStageSubsystem* Stage = GetStageSubsystem();

	// 소유자가 없으면 기본값(맑음)을 돌려준다. 날씨는 항상 하나가 정해져 있는 값이라
	// `모름` 상태가 없다 — Stage 시스템도 `Clear` 로 시작한다.
	return Stage ? Stage->GetSelectedWeather() : EFF_WeatherType::Clear;
}

bool UFF_FestivalViewModelSubsystem::CanChangeWeather() const
{
	const UFF_FestivalStageSubsystem* Stage = GetStageSubsystem();

	// 소유자가 없으면 "바꿀 수 있다"고 알리지 않는다. 눌러도 아무 일이 없는 버튼이 더 나쁘다.
	return Stage ? Stage->IsReconfigureAllowed() : false;
}

void UFF_FestivalViewModelSubsystem::RequestSetWeather(EFF_WeatherType NewWeather)
{
	UFF_FestivalStageSubsystem* Stage = GetStageSubsystem();
	if (!Stage)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[ViewModel] Stage 시스템이 없어 날씨를 바꿀 수 없다."));
		return;
	}

	// 여기서 단계를 미리 검사하지 않는다. 허용 판정의 소유자는 Stage 시스템이고,
	// UI 가 한 번 더 판정하면 두 곳의 규칙이 갈라진다 (UI 규칙 2).
	Stage->SetSelectedWeather(NewWeather);
}

int32 UFF_FestivalViewModelSubsystem::GetDailyVisitorGoal() const
{
	const UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	const UFF_FestivalDefinition* Definition = Progression ? Progression->GetDefinition() : nullptr;
	return Definition ? Definition->DailyVisitorGoal : INDEX_NONE;
}

int32 UFF_FestivalViewModelSubsystem::GetDailyGoalBonusGold() const
{
	const UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	const UFF_FestivalDefinition* Definition = Progression ? Progression->GetDefinition() : nullptr;
	return Definition ? Definition->DailyGoalBonusGold : INDEX_NONE;
}

int32 UFF_FestivalViewModelSubsystem::GetAdmissionFeeGold() const
{
	const UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	const UFF_FestivalDefinition* Definition = Progression ? Progression->GetDefinition() : nullptr;
	return Definition ? Definition->AdmissionFeeGold : INDEX_NONE;
}

EFF_FinaleGrade UFF_FestivalViewModelSubsystem::GetPurchasedFinaleGrade() const
{
	const UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	return Progression ? Progression->GetPurchasedFinaleGrade() : EFF_FinaleGrade::None;
}

int32 UFF_FestivalViewModelSubsystem::GetFinaleSatisfactionBonus() const
{
	const UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	return Progression ? Progression->GetFinaleSatisfactionBonus() : 0;
}

TArray<FFF_FinaleTier> UFF_FestivalViewModelSubsystem::GetFinaleTiers() const
{
	const UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	const UFF_FestivalDefinition* Definition = Progression ? Progression->GetDefinition() : nullptr;

	// 빈 배열은 "살 수 있는 등급이 없다"가 아니라 "아직 모른다"에 가깝다.
	// 화면은 카드를 그리지 않고 안내 문구를 띄운다 — 0 Gold 카드를 만들지 않는다.
	return Definition ? Definition->FinaleTiers : TArray<FFF_FinaleTier>();
}

bool UFF_FestivalViewModelSubsystem::CanPurchaseFinaleNow() const
{
	const UFF_StageEconomyBridgeSubsystem* Bridge = GetStageEconomyBridgeSubsystem();

	// 소유자가 없으면 "살 수 있다"고 알리지 않는다. 눌러도 아무 일이 없는 버튼이 더 나쁘다.
	return Bridge ? Bridge->CanPurchaseFinaleNow() : false;
}

EFF_FinalePurchaseResult UFF_FestivalViewModelSubsystem::RequestPurchaseFinale(EFF_FinaleGrade Grade)
{
	UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	if (!Progression)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[ViewModel] 경제 시스템이 없어 피날레를 구매할 수 없다."));
		return EFF_FinalePurchaseResult::FestivalNotStarted;
	}

	// 여기서 단계·잔액·중복 구매를 미리 검사하지 않는다. 판정의 소유자는 경제 시스템이고,
	// UI 가 한 번 더 판정하면 두 곳의 규칙이 갈라진다 (UI 규칙 2).
	// 실패해도 저쪽은 상태를 전혀 바꾸지 않는다.
	const EFF_FinalePurchaseResult Result = Progression->TryPurchaseFinale(Grade);

	if (Result != EFF_FinalePurchaseResult::Success)
	{
		// 사유 문구는 화면이 표시 라이브러리에서 만든다. 여기서는 요청이 실패한 사실만 남긴다.
		UE_LOG(LogFestaFiestaUI, Log,
			TEXT("[ViewModel] 피날레 구매 요청이 거부됐다. 사유는 경제 로그를 본다."));
	}

	// 성공했으면 `OnFinaleGradeChanged` 가 이미 울렸고 그 중계로 화면이 갱신된다.
	// 여기서 값을 직접 고치지 않는다 — 낙관적 갱신 금지.
	return Result;
}

TArray<FFF_ProgramCard> UFF_FestivalViewModelSubsystem::GetProgramCatalog() const
{
	const UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	return Progression ? Progression->GetProgramCatalog() : TArray<FFF_ProgramCard>();
}

TArray<FFF_ProgramCard> UFF_FestivalViewModelSubsystem::GetProgramCatalogForFacility(
	const FGuid FacilityInstanceId) const
{
	const UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	return Progression ? Progression->GetProgramCatalogForFacility(FacilityInstanceId) : TArray<FFF_ProgramCard>();
}

TArray<FName> UFF_FestivalViewModelSubsystem::GetScheduledProgramIds() const
{
	TArray<FName> Ids;

	const UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	if (!Progression)
	{
		return Ids;
	}

	// 소유자의 `GetScheduledProgramIds()` 는 private 이라 공개 목록에서 뽑는다.
	// 같은 값이고, 여기서 판정을 하지 않으므로 규칙이 갈라지지 않는다.
	const TArray<FFF_ScheduledProgram> Scheduled = Progression->GetScheduledPrograms();
	Ids.Reserve(Scheduled.Num());
	for (const FFF_ScheduledProgram& One : Scheduled)
	{
		Ids.Add(One.ProgramId);
	}
	return Ids;
}

EFF_ProgramPurchaseResult UFF_FestivalViewModelSubsystem::CanScheduleProgram(FName ProgramId) const
{
	const UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	if (!Progression)
	{
		// 소유자가 없으면 "편성할 수 있다"고 알리지 않는다. 눌러도 아무 일이 없는 카드가 더 나쁘다.
		return EFF_ProgramPurchaseResult::FestivalNotStarted;
	}

	return Progression->CanScheduleProgram(ProgramId);
}

bool UFF_FestivalViewModelSubsystem::CanScheduleProgramNow() const
{
	const UFF_StageEconomyBridgeSubsystem* Bridge = GetStageEconomyBridgeSubsystem();
	return Bridge ? Bridge->CanScheduleProgramNow() : false;
}

TArray<FFF_PrepFacilityOption> UFF_FestivalViewModelSubsystem::GetEligibleFacilitiesForProgram(
	FName ProgramId) const
{
	TArray<FFF_PrepFacilityOption> Options;

	const UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	const UFF_FestivalDefinition* Definition = Progression ? Progression->GetDefinition() : nullptr;
	if (!Definition)
	{
		return Options;
	}

	const FFF_ProgramCard* Card = FFF_ProgramScheduleRules::FindCard(Definition->ProgramCatalog, ProgramId);
	if (!Card)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[ViewModel] 카탈로그에 없는 프로그램 '%s' 의 시설을 물었다."), *ProgramId.ToString());
		return Options;
	}

	// Zone 은 월드 서브시스템이라 GameInstance 에서 바로 못 꺼낸다.
	const UGameInstance* GI = GetGameInstance();
	const UWorld* World = GI ? GI->GetWorld() : nullptr;
	const UFFZSWorldSubsystem* Zone = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	if (!Zone)
	{
		// 월드가 없는 상황(테스트 등)이다. 빈 목록은 "시설이 없다"가 아니라 "못 읽었다"이므로
		// 화면이 그 둘을 구분할 수 있게 로그를 남긴다.
		UE_LOG(LogFestaFiestaUI, Log,
			TEXT("[ViewModel] Zone 월드 서브시스템이 없어 시설 목록을 만들 수 없다."));
		return Options;
	}

	const FFFZSReadModel ReadModel = Zone->BuildReadModel();
	for (const FFFZSFacilityReadModel& Facility : ReadModel.Facilities)
	{
		FFF_FacilityNameParts Parts;
		FString FailureReason;
		if (!UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
			Facility.Category, Facility.FacilityDefinitionId, Parts, FailureReason))
		{
			// 이름 규칙을 어긴 시설은 종류를 알 수 없다. 소유자 판정도 같은 이유로 거절하므로
			// 목록에 넣으면 눌러도 실패하는 항목이 된다.
			continue;
		}

		// 🔴 허용 판정을 여기서 다시 쓰지 않는다. 소유자의 순수 함수를 그대로 부른다.
		if (!FFF_ProgramScheduleRules::IsFacilityTypeAllowed(*Card, Parts.MajorToken, Parts.MinorToken))
		{
			continue;
		}

		FFF_PrepFacilityOption& Option = Options.AddDefaulted_GetRef();
		Option.FacilityInstanceId = Facility.FacilityInstanceId;
		Option.ZoneId = Facility.ZoneId;

		// Use the same authored name as the guide and construction catalog.
		const UFF_BuildCatalogSubsystem* BuildCatalog = GI->GetSubsystem<UFF_BuildCatalogSubsystem>();
		Option.DisplayName = BuildCatalog
			? BuildCatalog->ResolveFacilityDisplayName(Facility.FacilityDefinitionId)
			: NSLOCTEXT("FestaFiestaPrepProgram", "FacilityNameUnavailable", "프로그램 시설");
	}

	return Options;
}

TArray<FFF_LiveNeedView> UFF_FestivalViewModelSubsystem::GetNeedMetrics() const
{
	TArray<FFF_LiveNeedView> Views;

	const UGameInstance* GI = GetGameInstance();
	const UFF_NpcNeedMetricsSubsystem* Needs =
		GI ? GI->GetSubsystem<UFF_NpcNeedMetricsSubsystem>() : nullptr;
	if (!Needs)
	{
		return Views;
	}

	// 순서를 enum 과 같게 유지한다 — 화면이 매번 같은 자리에 같은 욕구를 그려야
	// 눈이 값의 변화를 따라갈 수 있다.
	for (uint8 Index = 0; Index < static_cast<uint8>(EFF_NeedType::Count); ++Index)
	{
		const EFF_NeedType NeedType = static_cast<EFF_NeedType>(Index);

		FFF_LiveNeedView& View = Views.AddDefaulted_GetRef();
		View.NeedType = NeedType;
		View.UnmetRatioPercent = Needs->GetNeedUnmetRatio(NeedType);
	}

	return Views;
}

int32 UFF_FestivalViewModelSubsystem::GetNeedSampleCount() const
{
	const UGameInstance* GI = GetGameInstance();
	const UFF_NpcNeedMetricsSubsystem* Needs =
		GI ? GI->GetSubsystem<UFF_NpcNeedMetricsSubsystem>() : nullptr;

	// 🔴 서브시스템이 없으면 `0` 이다 — 표본 없음과 같은 뜻이라 화면이 같게 다룬다.
	return Needs ? Needs->GetSampleCount() : 0;
}

int32 UFF_FestivalViewModelSubsystem::GetConcurrentVisitorCount() const
{
	// 군중은 월드 서브시스템이라 GameInstance 에서 바로 못 꺼낸다.
	const UGameInstance* GI = GetGameInstance();
	const UWorld* World = GI ? GI->GetWorld() : nullptr;
	const UFF_CrowdSimulationSubsystem* Crowd =
		World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;

	// 🔴 없으면 0 이 아니라 `모름` 이다. 0 을 돌려주면 화면이 `관객 0명` 이라는
	// **확정 사실**을 말하는데, 실제로는 시뮬레이션이 아직 없는 것뿐이다.
	return Crowd ? Crowd->GetActiveAgentCount() : INDEX_NONE;
}

namespace FFViewModelFacilityLocal
{
	/** ZoneSystem 의 설치 상태를 UI 값으로 옮긴다. 모르는 값은 지어내지 않고 `Unknown` 이다. */
	EFF_LiveFacilityState ToUIState(EFFZSFacilityState State)
	{
		switch (State)
		{
		case EFFZSFacilityState::Planned:		return EFF_LiveFacilityState::Planned;
		case EFFZSFacilityState::Validated:		return EFF_LiveFacilityState::Validated;
		case EFFZSFacilityState::Active:		return EFF_LiveFacilityState::Active;
		case EFFZSFacilityState::Closed:		return EFF_LiveFacilityState::Closed;
		case EFFZSFacilityState::MovePending:	return EFF_LiveFacilityState::MovePending;
		case EFFZSFacilityState::Removed:		return EFF_LiveFacilityState::Removed;
		default:								return EFF_LiveFacilityState::Unknown;
		}
	}

	EFF_LiveFacilityQueueState ToUIQueueState(EFFZSQueueState State)
	{
		switch (State)
		{
		case EFFZSQueueState::Open:		return EFF_LiveFacilityQueueState::Open;
		case EFFZSQueueState::Full:		return EFF_LiveFacilityQueueState::Full;
		case EFFZSQueueState::Draining:	return EFF_LiveFacilityQueueState::Draining;
		case EFFZSQueueState::Closed:	return EFF_LiveFacilityQueueState::Closed;
		case EFFZSQueueState::Blocked:	return EFF_LiveFacilityQueueState::Blocked;
		default:						return EFF_LiveFacilityQueueState::Unknown;
		}
	}
}

TArray<FFF_LiveFacilityView> UFF_FestivalViewModelSubsystem::GetLiveFacilities() const
{
	TArray<FFF_LiveFacilityView> Views;

	// Zone 은 월드 서브시스템이라 GameInstance 에서 바로 못 꺼낸다.
	const UGameInstance* GI = GetGameInstance();
	const UWorld* World = GI ? GI->GetWorld() : nullptr;
	const UFFZSWorldSubsystem* Zone = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	if (!Zone)
	{
		// 빈 목록은 "시설이 없다" 가 아니라 "못 읽었다" 이므로 화면이 그 둘을 구분할 수 있게
		// 로그를 남긴다. 월드가 없는 상황(테스트 등)에서 정상적으로 지나간다.
		UE_LOG(LogFestaFiestaUI, Log,
			TEXT("[ViewModel] Zone 월드 서브시스템이 없어 시설 현황을 만들 수 없다."));
		return Views;
	}

	// 🔴 **예상 대기 시간은 다리를 건너야 온다.**
	//   Queue 는 `int32 FacilityId`, Zone 은 `FGuid` 라 서로 못 알아본다.
	//   그 사이를 이어 주는 `FindQueueFacilityId()` 가 2026-09-03 에 확인됐다
	//   (`UI_API.md` 요청 4번 — 장원재·한광훈 님 공급). 둘 중 하나만 없어도
	//   예상 대기는 `모름` 이고, 나머지 값은 그대로 보여 준다.
	const UFF_ZoneSystemQueueBridgeSubsystem* QueueBridge =
		World->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>();
	const UFF_QueueSubsystem* Queue = World->GetSubsystem<UFF_QueueSubsystem>();

	if (!QueueBridge || !Queue)
	{
		UE_LOG(LogFestaFiestaUI, Log,
			TEXT("[ViewModel] 대기열 다리(%s)나 Queue(%s)가 없어 예상 대기를 못 붙인다."),
			QueueBridge ? TEXT("있음") : TEXT("없음"), Queue ? TEXT("있음") : TEXT("없음"));
	}

	const FFFZSReadModel ReadModel = Zone->BuildReadModel();
	Views.Reserve(ReadModel.Facilities.Num());

	for (const FFFZSFacilityReadModel& Facility : ReadModel.Facilities)
	{
		// 🔴 철거된 시설은 뺀다. 목록에 남기면 "폐쇄" 와 구분이 안 되어
		//    고칠 수 있는 것과 이미 없는 것이 섞인다.
		if (Facility.State == EFFZSFacilityState::Removed)
		{
			continue;
		}

		FFF_LiveFacilityView& View = Views.AddDefaulted_GetRef();
		View.FacilityInstanceId = Facility.FacilityInstanceId;
		View.ZoneId = Facility.ZoneId;
		View.State = FFViewModelFacilityLocal::ToUIState(Facility.State);
		View.QueueState = FFViewModelFacilityLocal::ToUIQueueState(Facility.QueueState);
		View.WaitingCount = FMath::Max(0, Facility.WaitingCount);
		View.ActiveServiceCount = FMath::Max(0, Facility.ActiveServiceCount);
		View.bNeedsRelocation = Facility.bNeedsRelocation;

		// 🔴 아무도 아직 못 끝냈으면 평균이 `0.0` 으로 온다. 그것을 `0초 대기` 로 그리면
		//    "기다림이 없다" 는 틀린 사실이 된다 — 실제로는 **아직 모르는 것**이다.
		View.AverageWaitSeconds = (Facility.AverageCompletedWaitSeconds > 0.f)
			? FMath::RoundToInt(Facility.AverageCompletedWaitSeconds)
			: INDEX_NONE;

		// --- 예상 대기: 지금 한 명이 서면 얼마나 기다리나 ---
		if (QueueBridge && Queue)
		{
			const int32 QueueFacilityId = QueueBridge->FindQueueFacilityId(Facility.FacilityInstanceId);
			if (QueueFacilityId != INDEX_NONE)
			{
				const float WaitSeconds = Queue->GetExpectedWaitSeconds(QueueFacilityId, /*IncomingMemberCount=*/1);

				// 🔴 `UnreachableWaitSeconds` 는 **시간이 아니라 표식**이다. Queue 쪽 주석에
				//    "미등록 시설에 0을 돌려주면 대기 없는 최고의 후보로 보인다" 고 적혀 있듯,
				//    저쪽은 안전한 쪽으로 넘어지려고 큰 값을 쓴다. 그것을 그대로 그리면
				//    화면에 `1440분` 이 뜬다. 여기서 `모름` 으로 바꾼다.
				const bool bReachable = WaitSeconds < FFF_QueueTimings::UnreachableWaitSeconds;

				View.ExpectedWaitSeconds = bReachable
					? FMath::Max(0, FMath::RoundToInt(WaitSeconds))
					: INDEX_NONE;

				View.ServiceRatePerMinute = Queue->GetFacilityServiceRatePerMinute(QueueFacilityId);
			}
		}

		// 사람이 읽을 이름이 시설 데이터에 없다. 고유 토큰이 그나마 구분되는 값이다 —
		// `GetEligibleFacilitiesForProgram` 과 같은 사정이고 같은 방식으로 만든다.
		FFF_FacilityNameParts Parts;
		FString FailureReason;
		const bool bParsed = UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
			Facility.Category, Facility.FacilityDefinitionId, Parts, FailureReason);

		View.DisplayName = (bParsed && !Parts.UniqueToken.IsEmpty())
			? FText::FromString(Parts.UniqueToken)
			: FText::FromName(Facility.FacilityDefinitionId);
	}

	return Views;
}

EFF_ProgramPurchaseResult UFF_FestivalViewModelSubsystem::RequestScheduleProgram(
	FName ProgramId, FGuid FacilityInstanceId)
{
	UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	if (!Progression)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[ViewModel] 경제 시스템이 없어 프로그램을 편성할 수 없다."));
		return EFF_ProgramPurchaseResult::FestivalNotStarted;
	}

	// 단계·잔액·인원·시설을 여기서 미리 검사하지 않는다. 판정의 소유자는 경제 시스템이다 (UI 규칙 2).
	const EFF_ProgramPurchaseResult Result = Progression->TryScheduleProgram(ProgramId, FacilityInstanceId);

	if (Result != EFF_ProgramPurchaseResult::Success)
	{
		UE_LOG(LogFestaFiestaUI, Log,
			TEXT("[ViewModel] 프로그램 '%s' 편성 요청이 거부됐다. 사유는 경제 로그를 본다."),
			*ProgramId.ToString());
	}

	// 성공했으면 `OnScheduledProgramsChanged` 가 이미 울렸다. 여기서 값을 고치지 않는다.
	return Result;
}

TArray<FFF_PromotionOptionView> UFF_FestivalViewModelSubsystem::GetPromotionOptions() const
{
	TArray<FFF_PromotionOptionView> Options;

	const UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	const UFF_FestivalDefinition* Definition = Progression ? Progression->GetDefinition() : nullptr;
	if (!Definition)
	{
		// 빈 배열은 "고를 게 없다"가 아니라 "아직 모른다"이다. 화면이 사유 문구로 설명한다.
		return Options;
	}

	// `안 함` 은 데이터가 아니라 미구매 상태다. 0 Gold · 배수 1.0 을 여기서 만든다 —
	// 데이터에 0원짜리 등급을 넣으면 검증(`IsDefinitionValid`)이 그것을 오류로 잡는다.
	FFF_PromotionOptionView& NoneOption = Options.AddDefaulted_GetRef();
	NoneOption.Choice = EFF_PromotionChoice::None;
	NoneOption.CostGold = 0;
	NoneOption.VisitorMultiplier = 1.f;

	FFF_PromotionOptionView& BasicOption = Options.AddDefaulted_GetRef();
	BasicOption.Choice = EFF_PromotionChoice::Basic;
	BasicOption.CostGold = Definition->PromotionBasic.CostGold;
	BasicOption.VisitorMultiplier = Definition->PromotionBasic.VisitorMultiplier;

	FFF_PromotionOptionView& FocusedOption = Options.AddDefaulted_GetRef();
	FocusedOption.Choice = EFF_PromotionChoice::Focused;
	FocusedOption.CostGold = Definition->PromotionFocused.CostGold;
	FocusedOption.VisitorMultiplier = Definition->PromotionFocused.VisitorMultiplier;

	return Options;
}

bool UFF_FestivalViewModelSubsystem::CanPurchasePromotionNow() const
{
	const UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	if (!Progression || !Progression->IsFestivalStarted())
	{
		return false;
	}

	// ⚠️ 판정 소유자가 없어서 단계를 여기서 본다. 브리지에 `CanPurchasePromotionNow()` 가
	// 생기면 중계로 바꾼다 — 그때 이 비교식을 지운다.
	// 셋업뿐인 이유는 홍보가 3일 전체에 적용되는 1회 선택이라서다.
	return GetCurrentStage() == EFF_FestivalStage::Setup;
}

bool UFF_FestivalViewModelSubsystem::RequestPurchasePromotion(EFF_PromotionChoice Choice)
{
	if (Choice == EFF_PromotionChoice::None)
	{
		// `안 함` 은 사는 것이 아니라 안 사는 것이다. 요청으로 오면 안 된다.
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[ViewModel] `안 함` 등급으로 홍보 구매 요청이 왔다. 버튼 활성 조건을 확인한다."));
		return false;
	}

	UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	if (!Progression)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[ViewModel] 경제 시스템이 없어 홍보를 살 수 없다."));
		return false;
	}

	// 판정·차감·등급 보관을 전부 소유자가 한다. 여기는 enum 을 옮겨 담을 뿐이다.
	const EFF_PromotionPurchaseResult Result =
		Progression->TryPurchasePromotion(ToPromotionGrade(Choice));

	if (Result != EFF_PromotionPurchaseResult::Success)
	{
		UE_LOG(LogFestaFiestaUI, Log,
			TEXT("[ViewModel] 홍보 구매 거부: %s"), *UEnum::GetValueAsString(Result));
		return false;
	}

	// 방송은 소유자의 OnPromotionGradeChanged 를 받아서 한다 — 여기서 또 쏘면 두 번 온다.
	return true;
}

EFF_PromotionGrade UFF_FestivalViewModelSubsystem::ToPromotionGrade(EFF_PromotionChoice Choice)
{
	switch (Choice)
	{
	case EFF_PromotionChoice::Basic:   return EFF_PromotionGrade::Basic;
	case EFF_PromotionChoice::Focused: return EFF_PromotionGrade::Focused;
	default:                           return EFF_PromotionGrade::None;
	}
}

EFF_PromotionChoice UFF_FestivalViewModelSubsystem::ToPromotionChoice(EFF_PromotionGrade Grade)
{
	switch (Grade)
	{
	case EFF_PromotionGrade::Basic:   return EFF_PromotionChoice::Basic;
	case EFF_PromotionGrade::Focused: return EFF_PromotionChoice::Focused;
	default:                          return EFF_PromotionChoice::None;
	}
}

void UFF_FestivalViewModelSubsystem::HandleOwnerPromotionGradeChanged(EFF_PromotionGrade NewGrade)
{
	PurchasedPromotion = ToPromotionChoice(NewGrade);
	OnPurchasedPromotionChanged.Broadcast(PurchasedPromotion);
}

void UFF_FestivalViewModelSubsystem::HandleOwnerFinaleGradeChanged(EFF_FinaleGrade NewGrade)
{
	// Bind + Sync 로 한 번, 실제 구매 방송으로 또 한 번 같은 값이 올 수 있다
	// 위젯 델리게이트는 값이 바뀔 때만 쏜다.
	if (NewGrade != LastBroadcastFinaleGrade)
	{
		LastBroadcastFinaleGrade = NewGrade;
		OnPurchasedFinaleGradeChanged.Broadcast(NewGrade);
	}

	// FinaleBonus는 등급에서 파생값이라 델리게이트가 없다 — 등급이 바뀔 때(Bind + Sync 포함)
	// ScoreSatisfaction도 같이 다시 잰다. 값이 실제로 그대로면 ApplyIntMetric이 조용히 넘어간다.
	RecomputeScoreSatisfaction();
}

void UFF_FestivalViewModelSubsystem::RecomputeScoreSatisfaction()
{
	const int32 FinaleBonus = GetFinaleSatisfactionBonus();
	const int32 NewScoreSatisfaction = FMath::Clamp(SimSatisfaction + FinaleBonus, 0, MetricMax);
	ApplyIntMetric(ScoreSatisfaction, NewScoreSatisfaction, OnScoreSatisfactionChanged);
}

void UFF_FestivalViewModelSubsystem::HandleOwnerEnvironmentChanged(EFF_WeatherType NewWeather, bool bIsNight)
{
	// 원본은 야간 전환에도 울린다. 날씨가 그대로면 위젯을 깨우지 않는다.
	if (NewWeather == LastBroadcastWeather)
	{
		return;
	}

	LastBroadcastWeather = NewWeather;
	OnWeatherChanged.Broadcast(NewWeather);
}

int32 UFF_FestivalViewModelSubsystem::GetAffordableTemporaryStaffCount() const
{
	const UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	if (!Progression || !Progression->IsFestivalStarted())
	{
		// 0 을 돌려주면 "고용할 수 없다"는 사실이 되어 버린다. 모르는 것과 구분한다.
		return INDEX_NONE;
	}

	return Progression->GetAffordableTemporaryStaffCount();
}

int32 UFF_FestivalViewModelSubsystem::GetRemainingTemporaryStaffCap() const
{
	const UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	if (!Progression || !Progression->IsFestivalStarted())
	{
		return INDEX_NONE;
	}

	// Definition 이 없으면 "모름"이다. GetRemainingHireCap() 은 그 경우 0 을 돌려주는데,
	// 0 은 이 화면에서 "상한 도달"이라는 다른 뜻이라 여기서 걸러야 한다.
	if (!Progression->GetDefinition())
	{
		return INDEX_NONE;
	}

	// 식을 여기서 다시 쓰지 않는다 — 상한 규칙의 소유자는 경제다.
	// **고용 가능 판정은 여기서 하지 않는다** — 이 값은 막힌 이유를 화면에 쓰기 위한 것이고,
	// 실제 허용 여부는 `TryHireTemporaryStaff` 가 정한다.
	return Progression->GetRemainingHireCap();
}

int32 UFF_FestivalViewModelSubsystem::GetDailyStaffWageGold() const
{
	const UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	const UFF_FestivalDefinition* Definition = Progression ? Progression->GetDefinition() : nullptr;
	return Definition ? Definition->DailyStaffWageGold : INDEX_NONE;
}

int32 UFF_FestivalViewModelSubsystem::GetMaxTemporaryStaffPerDay() const
{
	const UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	const UFF_FestivalDefinition* Definition = Progression ? Progression->GetDefinition() : nullptr;
	return Definition ? Definition->MaxHiredStaffPerDay : INDEX_NONE;
}

int32 UFF_FestivalViewModelSubsystem::GetDailyVenueRentGold() const
{
	const UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	const UFF_FestivalDefinition* Definition = Progression ? Progression->GetDefinition() : nullptr;
	return Definition ? Definition->DailyVenueRentGold : INDEX_NONE;
}

bool UFF_FestivalViewModelSubsystem::RequestHireTemporaryStaff(int32 Count)
{
	UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	if (!Progression)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[ViewModel] 경제 시스템이 없어 알바를 고용할 수 없다."));
		return false;
	}

	// 여기서 미리 막지 않는다. 상한·잔액 판정의 소유자는 경제 시스템이고, UI 가 한 번 더
	// 판정하면 두 곳의 규칙이 갈라진다 (UI 규칙 2).
	const bool bAccepted = Progression->TryHireTemporaryStaff(Count);
	if (!bAccepted)
	{
		// 거부 사유는 경제 쪽 로그에 남는다. 여기서는 요청이 실패한 사실만 남긴다.
		UE_LOG(LogFestaFiestaUI, Log,
			TEXT("[ViewModel] 알바 %d명 고용 요청이 거부됐다. 상한/잔액은 경제 로그를 본다."), Count);
	}
	return bAccepted;
}

bool UFF_FestivalViewModelSubsystem::CanAfford(int32 Amount) const
{
	const UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	if (!Progression)
	{
		// 소유자가 없으면 "살 수 있다"고 알리지 않는다. 잘못 눌러 실패하는 쪽이 더 나쁘다.
		return false;
	}

	return Progression->CanAfford(Amount);
}

void UFF_FestivalViewModelSubsystem::RequestSetTimeScale(float NewScale)
{
	UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();
	if (!StageSubsystem)
	{
		UE_LOG(LogFestaFiestaUI, Warning, TEXT("[ViewModel] Stage 시스템이 없어 배속을 바꿀 수 없다."));
		return;
	}

	// 잠금 구간이어도 요청은 보낸다. 무시 판정의 소유자는 Stage 시스템이다.
	// UI가 미리 막으면 잠금 규칙이 두 곳에 생긴다.
	StageSubsystem->SetTimeScale(NewScale);
}

float UFF_FestivalViewModelSubsystem::GetTimeScale() const
{
	const UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();
	return StageSubsystem ? StageSubsystem->TimeScale : 1.f;
}

bool UFF_FestivalViewModelSubsystem::IsTimeScaleLocked() const
{
	UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();

	// IsMainProgramLocked()가 const가 아니라 비const 포인터가 필요하다.
	return StageSubsystem ? StageSubsystem->IsMainProgramLocked() : false;
}

FText UFF_FestivalViewModelSubsystem::GetLiveClockText() const
{
	const UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();
	if (!StageSubsystem)
	{
		return FText::GetEmpty();
	}

	// Stage 쪽이 FString으로 준다. 화면 문구는 FText로만 다룬다.
	// 시각 표기(19:42)는 숫자 서식이라 현지화 대상이 아니므로 FromString으로 감싸도 손실이 없다.
	return FText::FromString(StageSubsystem->GetCurrentLiveClockString());
}

FText UFF_FestivalViewModelSubsystem::GetLiveClockTextAtMinutes(int32 LiveMinutes) const
{
	const UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();
	if (!StageSubsystem)
	{
		return FText::GetEmpty();
	}

	// 시각 표기는 숫자 서식이라 현지화 대상이 아니다. `GetLiveClockText()` 와 같은 이유로 감싼다.
	return FText::FromString(StageSubsystem->GetLiveClockStringFromMinutes(LiveMinutes));
}

void UFF_FestivalViewModelSubsystem::HandleMainProgramStarted(FString EventLabel)
{
	// 라벨은 쓰지 않는다. 화면이 알아야 하는 것은 `지금 잠겼는가` 하나다.
	OnTimeScaleLockChanged.Broadcast(true);
}

void UFF_FestivalViewModelSubsystem::HandleMainProgramEnded(FString EventLabel)
{
	OnTimeScaleLockChanged.Broadcast(false);
}

void UFF_FestivalViewModelSubsystem::HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage)
{
	OnStageViewChanged.Broadcast(NewStage, GetCurrentDay());

	// Stage가 바뀌면 남은 시간도 새 단계 기준으로 다시 시작한다.
	// Stage 쪽은 초가 "바뀔 때만" 쏘므로, 전환 직후 값이 우연히 같으면 방송이 오지 않는다.
	// 그래서 전환 시점에 한 번 직접 맞춘다 — Bind + Sync를 전환마다 다시 하는 셈이다.
	BroadcastRemainingSecondsIfChanged(FMath::CeilToInt(GetRemainingSeconds()));
}

void UFF_FestivalViewModelSubsystem::HandleRemainingSecondsChanged(int32 NewSeconds)
{
	BroadcastRemainingSecondsIfChanged(NewSeconds);
}

void UFF_FestivalViewModelSubsystem::BroadcastRemainingSecondsIfChanged(int32 NewSeconds)
{
	if (NewSeconds == LastBroadcastRemainingSeconds)
	{
		return;
	}

	LastBroadcastRemainingSeconds = NewSeconds;
	OnRemainingSecondsChanged.Broadcast(NewSeconds);
}

EFF_FestivalStage UFF_FestivalViewModelSubsystem::GetCurrentStage() const
{
	const UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();
	return StageSubsystem ? StageSubsystem->CurrentStage : EFF_FestivalStage::Setup;
}

int32 UFF_FestivalViewModelSubsystem::GetCurrentDay() const
{
	const UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();
	return StageSubsystem ? StageSubsystem->CurrentDay : 0;
}

float UFF_FestivalViewModelSubsystem::GetRemainingSeconds() const
{
	const UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();
	return StageSubsystem ? StageSubsystem->RemainingTime : 0.f;
}

FText UFF_FestivalViewModelSubsystem::GetCurrentStageDisplayText() const
{
	// 🔴 **`UMETA(DisplayName)` 을 리플렉션으로 읽지 않는다** (2026-08-28 정정).
	// 예전 주석은 "하드코딩 매핑하지 않는다" 였는데 그게 틀렸다 — `GetDisplayNameTextByValue` 는
	// 에디터 메타데이터를 읽고, 그 메타데이터는 패키징 빌드에 안 들어간다. 그래서 에디터에서는
	// `셋업` 으로 잘 보이다가 패키징하면 `1일차 · Setup` 이 됐다.
	//
	// 기획 7단계와 코드 8단계가 아직 다르므로, 모르는 값이 와도 빈 문자열이 되면 안 된다.
	const EFF_FestivalStage Stage = GetCurrentStage();
	const FText Text = UFF_UIDisplayTextLibrary::GetFestivalStageText(Stage);
	if (!Text.IsEmpty())
	{
		return Text;
	}

	UE_LOG(LogFestaFiestaUI, Warning,
		TEXT("[ViewModel] 표시 문구가 없는 Stage 값 %d. `UFF_UIDisplayTextLibrary` 에 항목을 더한다."),
		static_cast<int32>(Stage));
	return FText::AsNumber(static_cast<int32>(Stage));
}

void UFF_FestivalViewModelSubsystem::ApplyIntMetric(int32& Current, int32 NewValue, FFF_OnIntMetricChanged& ChangedDelegate)
{
	if (Current == NewValue)
	{
		return;
	}

	Current = NewValue;
	ChangedDelegate.Broadcast(Current);
}

void UFF_FestivalViewModelSubsystem::Debug_SetMetrics(int32 InSimSatisfaction, int32 InScoreSatisfaction, int32 InSafety)
{
	// 0~100 지표는 범위를 벗어난 값이 들어와도 화면이 깨지지 않게 잘라낸다.
	// 실제 소유자 시스템이 붙어도 이 방어는 남겨둔다. 계산 실수가 UI까지 오면 원인 추적이 어렵다.
	const auto Clamp01_100 = [](int32 Value) { return FMath::Clamp(Value, 0, MetricMax); };

	ApplyIntMetric(SimSatisfaction, Clamp01_100(InSimSatisfaction), OnSimSatisfactionChanged);
	ApplyIntMetric(ScoreSatisfaction, Clamp01_100(InScoreSatisfaction), OnScoreSatisfactionChanged);
	ApplyIntMetric(Safety, Clamp01_100(InSafety), OnSafetyChanged);
}

void UFF_FestivalViewModelSubsystem::Debug_SetEconomy(int32 InGold, int32 InTotalRevenue, int32 InTodayRevenue)
{
	// 금액이라 0~100 상한을 두지 않는다.
	// Gold는 음수가 되지 않는 것으로 확정됐다 (2026-08-18, `FFF_FestivalFunds` 주석) — 잔액이 모자라면
	// 구매를 거부한다. 임대료처럼 거절할 수 없는 의무비용만 있는 만큼 내고 부족분이
	// `FFF_FixedCostArrears`에 미납으로 쌓인다. 그래도 여기서 클램프하지는 않는다 — 값의 주인은 소유자다.
	ApplyIntMetric(Gold, InGold, OnGoldChanged);
	ApplyIntMetric(TotalRevenue, InTotalRevenue, OnTotalRevenueChanged);
	ApplyIntMetric(TodayRevenue, InTodayRevenue, OnTodayRevenueChanged);
}

void UFF_FestivalViewModelSubsystem::Debug_DumpToLog() const
{
	// 화면을 못 볼 때 값을 확인하는 유일한 경로다. 단위를 같이 적어 둔다.
	UE_LOG(LogFestaFiestaUI, Log,
		TEXT("[ViewModel] SimSat=%d/%d ScoreSat=%d/%d Safety=%d/%d | Gold=%d TotalRev=%d TodayRev=%d | Stage=%s Day=%d 남은=%.1f초"),
		SimSatisfaction, MetricMax,
		ScoreSatisfaction, MetricMax,
		Safety, MetricMax,
		Gold, TotalRevenue, TodayRevenue,
		*GetCurrentStageDisplayText().ToString(), GetCurrentDay(), GetRemainingSeconds());
}

void UFF_FestivalViewModelSubsystem::PollMetricsFromSources()
{
	// !! 이건 폴백이지 목표 설계가 아니다 !!
	//
	// 지표를 가져오는 방법은 둘이고 우선순위가 있다.
	//
	//   1순위 — 푸시. 소유자가 "값이 바뀌었다"고 델리게이트를 쏜다.
	//           그 델리게이트를 TryBind...에서 구독하고 이 함수에서는 손대지 않는다.
	//   2순위 — 풀(여기). 소유자가 Getter만 주고 델리게이트를 안 줄 때만 쓴다.
	//
	// UI_API.md 원칙 3이 "폴링하지 않는다"인 이유다. 델리게이트가 없어서 어쩔 수 없이 읽는 값만
	// 여기 둔다. 새 값을 추가할 때 습관적으로 여기 넣지 말고 소유자에게 델리게이트를 먼저 요청한다.
	// (선례: RemainingSeconds. 2026-08-06에 요청 → 2026-08-11에 강민서가 모든 Stage를 커버하는
	//  OnRemainingSecondsChanged를 제공 → 구독으로 바꾸고 폴링을 걷어냈다.
	//  요청하면 받을 수 있다는 뜻이다. 폴링을 기본값처럼 쓰지 않는다.)
	//
	// 어느 쪽이든 통지는 ApplyIntMetric을 거치므로 **위젯은 값이 실제로 바뀔 때만 갱신된다.**
	// 폴링 주기는 소스를 확인하는 간격이지 위젯 갱신 주기가 아니다.
	//
	// 아직 소유자 시스템이 하나도 없어 지금은 아무것도 하지 않는다. 생기면 이렇게 된다.
	//
	//   ApplyIntMetric(Gold, Progression->GetGold(), OnGoldChanged);   // 델리게이트가 없을 때만
	//
	// 소유자 현황은 리포 루트 UI_API.md의 "UI가 필요한데 아직 없는 것" 표에 있다.
}

void UFF_FestivalViewModelSubsystem::Tick(float DeltaTime)
{
	if (!CachedStageSubsystem)
	{
		TryBindStageSubsystem();
	}

	if (!CachedProgressionSubsystem)
	{
		TryBindProgressionSubsystem();
	}

	if (!CachedSatisfactionSubsystem)
	{
		TryBindSatisfactionSubsystem();
	}

	if (!CachedInsightsSubsystem)
	{
		TryBindInsightsSubsystem();
	}

	// 남은 시간은 여기서 폴링하지 않는다.
	// 2026-08-11 강민서 쪽에 모든 Stage를 커버하는 OnRemainingSecondsChanged가 생겨서
	// 구독으로 바꿨다(TryBindStageSubsystem). 폴링을 되살리지 않는다 — 중복 방송이 된다.

	// 지표는 아직 소유자 델리게이트가 없어 주기 확인이 남아 있다.
	// 값이 안 바뀌면 아무것도 쏘지 않는다.
	PollAccumulator += DeltaTime;
	if (PollAccumulator < PollInterval)
	{
		return;
	}
	PollAccumulator = 0.f;

	PollMetricsFromSources();
}

void UFF_FestivalViewModelSubsystem::RequestAdvanceToNextDay()
{
	if (!CanAdvanceToNextDay())
	{
		// 버튼이 잘못 활성화된 경우다. 조용히 넘기지 않고 원인을 남긴다.
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[ViewModel] 다음 날 요청이 거부됐다. Stage=%s Day=%d (Report 단계이고 마지막 날이 아닐 때만 가능)"),
			*GetCurrentStageDisplayText().ToString(), GetCurrentDay());
		return;
	}

	UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();
	StageSubsystem->AdvanceToNextDay();

	// 여기서 화면을 직접 갱신하지 않는다. 결과는 OnStageChanged → OnStageViewChanged로 돌아온다.
	// 낙관적 갱신 금지 (UI 규칙 2).
}

void UFF_FestivalViewModelSubsystem::RequestSkipCurrentStage()
{
	UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();
	if (!StageSubsystem)
	{
		UE_LOG(LogFestaFiestaUI, Warning, TEXT("[ViewModel] Stage 시스템이 없어 건너뛰기를 요청할 수 없다."));
		return;
	}

	// 스킵 가능 여부의 최종 판정은 Stage 시스템이 한다(DaySequence 표의 bSkippable).
	// CanSkipCurrentStage()로 여기서 미리 막지 않는다 — 판정 소유자를 두 곳에 두지 않기 위해서다.
	// 불가능한 단계면 저쪽이 무시하고 경고를 남긴다.
	StageSubsystem->SkipCurrentStage();
}

bool UFF_FestivalViewModelSubsystem::CanAdvanceToNextDay() const
{
	const UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();
	if (!StageSubsystem)
	{
		return false;
	}

	// 총 일수를 UI에 하드코딩하지 않는다. IsFinalDay()가 그 판단을 소유한다.
	return StageSubsystem->IsReportStage() && !StageSubsystem->IsFinalDay();
}

bool UFF_FestivalViewModelSubsystem::IsFinalDay() const
{
	const UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();
	return StageSubsystem && StageSubsystem->IsFinalDay();
}

bool UFF_FestivalViewModelSubsystem::IsReportStage() const
{
	const UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();
	return StageSubsystem && StageSubsystem->IsReportStage();
}

bool UFF_FestivalViewModelSubsystem::CanSkipCurrentStage() const
{
	// IsCurrentStageSkippable()이 non-const라 포인터도 non-const여야 한다.
	// GetStageSubsystem()은 const 함수지만 non-const 포인터를 돌려주므로 그대로 받는다.
	UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();
	if (!StageSubsystem)
	{
		return false;
	}

	// DaySequence 표의 bSkippable을 그대로 중계한다. 짐작하지 않는다.
	return StageSubsystem->IsCurrentStageSkippable();
}
