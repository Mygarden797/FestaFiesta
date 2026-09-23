// Fill out your copyright notice in the Description page of Project Settings.

#include "NPC/Insights/FF_CrowdInsightsSubsystem.h"

#include "NPC/Insights/FF_SnsFeedGenerator.h"
#include "DataTables/FF_SnsPostTemplateRow.h"
#include "NPC/Metrics/FF_SatisfactionMetricsSubsystem.h"
#include "NPC/Metrics/FF_NpcNeedMetricsSubsystem.h"
#include "NPC/Party/FF_NeedState.h"
#include "NPC/Destination/FF_UtilityCoefficientHelpers.h"
#include "NPC/Destination/FF_UtilityDestinationSelector.h" // LogFestaFiestaNpc
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "CrowdSystem/Systems/FF_CrowdDecisionSubsystem.h"
#include "CrowdSystem/Types/FF_SimulationSettings.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "Festival/FF_FestivalBuzzSubsystem.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSTypes.h"

void UFF_CrowdInsightsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (const UFF_SimulationSettings* Settings = UFF_SimulationSettings::Get())
	{
		GlobalSimulationSeed = Settings->GlobalSimulationSeed;
		SnsPostTemplateTable = Settings->SnsPostTemplateTable.LoadSynchronous();
	}

	if (UFF_FestivalStageSubsystem* Stage = GetStageSubsystem())
	{
		Stage->OnStageChanged.AddDynamic(this, &UFF_CrowdInsightsSubsystem::HandleStageChanged);
	}

	ScheduleNextPost();
}

void UFF_CrowdInsightsSubsystem::Deinitialize()
{
	if (UFF_FestivalStageSubsystem* Stage = GetStageSubsystem())
	{
		Stage->OnStageChanged.RemoveDynamic(this, &UFF_CrowdInsightsSubsystem::HandleStageChanged);
	}

	Super::Deinitialize();
}

void UFF_CrowdInsightsSubsystem::HandleStageChanged(EFF_FestivalStage /*PreviousStage*/, EFF_FestivalStage /*NewStage*/)
{
	UFF_FestivalStageSubsystem* Stage = GetStageSubsystem();
	if (!Stage || !Stage->IsReportStage() || !Stage->IsFinalDay())
	{
		return;
	}

	EnsureTemplatesLoaded();

	const FFF_SnsFeedSignals Signals; // 피날레는 ForcedTopicId라 나머지 신호는 안 쓴다
	FFF_SnsPostRow NewPost = FFF_SnsFeedGenerator::GeneratePost(
		GlobalSimulationSeed, PostSequenceIndex, Signals, TemplatesByTopic,
		Stage->ElapsedLiveSeconds, /*ForcedTopicId=*/9);
	++PostSequenceIndex;

	PushPost(NewPost);
}

void UFF_CrowdInsightsSubsystem::Tick(float DeltaTime)
{
	MaybeGeneratePost(DeltaTime);
}

void UFF_CrowdInsightsSubsystem::MaybeGeneratePost(float DeltaTime)
{
	UFF_FestivalStageSubsystem* Stage = GetStageSubsystem();
	if (!Stage || !Stage->IsLiveStage())
	{
		return;
	}

	EnsureTemplatesLoaded();

	FFF_SnsFeedSignals Signals;

	// ⑤·⑥의 델타 신호는 Live 동안 계속 갱신한다(포스트가 안 나가는 tick에도 최신값을 유지해야
	// 다음 포스트 시점에 그 사이의 변화량을 정확히 반영한다)
	if (UFF_CrowdDecisionSubsystem* Decision = GetCrowdDecisionSubsystem())
	{
		const int32 Current = Decision->GetDecisionStats().FailedDestinationPenalized;
		Signals.FailedDestinationDeltaThisWindow = FMath::Max(0, Current - LastObservedFailedDestinationPenalized);
		LastObservedFailedDestinationPenalized = Current;
	}

	const float ScaledDelta = DeltaTime * FMath::Max(0.f, Stage->GetEffectiveTimeScale());
	TimeUntilNextPostSeconds -= ScaledDelta;
	if (TimeUntilNextPostSeconds > 0.f)
	{
		return;
	}

	if (const UFF_SatisfactionMetricsSubsystem* Satisfaction = GetSatisfactionSubsystem())
	{
		Signals.SimSatisfaction0To100 = Satisfaction->GetSimSatisfaction();
		Signals.WaitScore0To100 = Satisfaction->GetWaitScore();
		Signals.DensityScore0To100 = Satisfaction->GetDensityScore();
		Signals.PreferenceScore0To100 = Satisfaction->GetPreferenceScore();
		Signals.NeedFulfillmentScore0To100 = Satisfaction->GetNeedFulfillmentScore();
	}
	if (const UFF_NpcNeedMetricsSubsystem* NeedMetrics = GetNeedMetricsSubsystem())
	{
		Signals.StimulationAverage0To1 = NeedMetrics->GetNeedAverage(EFF_NeedType::Stimulation);
	}
	if (UFF_CrowdSimulationSubsystem* Crowd = GetCrowdSimulationSubsystem())
	{
		Signals.CongestedCellCount = Crowd->GetCongestedCellCount();
	}
	Signals.bMainProgramLocked = Stage->IsMainProgramLocked();
	Signals.CurrentDay = Stage->CurrentDay;
	FillFacilitySignals(Signals);
	FillAdmissionFeeSignals(Signals);

	FFF_SnsPostRow NewPost = FFF_SnsFeedGenerator::GeneratePost(
		GlobalSimulationSeed, PostSequenceIndex, Signals, TemplatesByTopic, Stage->ElapsedLiveSeconds);
	++PostSequenceIndex;

	PushPost(NewPost);
	ScheduleNextPost();
}

void UFF_CrowdInsightsSubsystem::PushPost(const FFF_SnsPostRow& NewPost)
{
	RecentPosts.Insert(NewPost, 0);
	if (RecentPosts.Num() > RingBufferCapacity)
	{
		RecentPosts.SetNum(RingBufferCapacity);
	}

	OnSnsPostAdded.Broadcast(NewPost);

	// 글이 화제성을 움직인다. 같은 GameInstance 스코프라 매번 조회해도 된다.
	// 화제9(피날레)는 축제가 끝난 뒤 강제로 한 번 나가는 글이라 평가 직전에 점수를 올리지 않게 뺀다.
	UFF_FestivalBuzzSubsystem* Buzz = NewPost.TopicId != 9
		? GetGameInstance()->GetSubsystem<UFF_FestivalBuzzSubsystem>() : nullptr;
	if (Buzz != nullptr)
	{
		Buzz->ApplySnsPost(NewPost);
	}
}

FFF_SnsPostRow UFF_CrowdInsightsSubsystem::Debug_ForceGeneratePost()
{
	EnsureTemplatesLoaded();

	FFF_SnsFeedSignals Signals;
	if (const UFF_SatisfactionMetricsSubsystem* Satisfaction = GetSatisfactionSubsystem())
	{
		Signals.SimSatisfaction0To100 = Satisfaction->GetSimSatisfaction();
		Signals.WaitScore0To100 = Satisfaction->GetWaitScore();
		Signals.DensityScore0To100 = Satisfaction->GetDensityScore();
		Signals.PreferenceScore0To100 = Satisfaction->GetPreferenceScore();
		Signals.NeedFulfillmentScore0To100 = Satisfaction->GetNeedFulfillmentScore();
	}
	FillFacilitySignals(Signals);

	const float Timestamp = GetStageSubsystem() ? GetStageSubsystem()->ElapsedLiveSeconds : 0.f;
	FFF_SnsPostRow NewPost = FFF_SnsFeedGenerator::GeneratePost(
		GlobalSimulationSeed, PostSequenceIndex, Signals, TemplatesByTopic, Timestamp);
	++PostSequenceIndex;

	PushPost(NewPost);
	return NewPost;
}

void UFF_CrowdInsightsSubsystem::ScheduleNextPost()
{
	const UFF_SimulationSettings* Settings = UFF_SimulationSettings::Get();
	const UDataTable* CoefficientTable = Settings ? Settings->UtilityCoefficientTable.LoadSynchronous() : nullptr;
	const float MinSeconds = FFF_UtilityCoefficientHelpers::GetCoefficient(CoefficientTable, TEXT("sns_post_interval_min_seconds"), 3.f);
	const float MaxSeconds = FFF_UtilityCoefficientHelpers::GetCoefficient(CoefficientTable, TEXT("sns_post_interval_max_seconds"), 10.f);

	TimeUntilNextPostSeconds = FFF_SnsFeedGenerator::NextIntervalSeconds(
		GlobalSimulationSeed, PostSequenceIndex, MinSeconds, MaxSeconds);
}

void UFF_CrowdInsightsSubsystem::EnsureTemplatesLoaded()
{
	if (bTemplatesLoaded)
	{
		return;
	}
	bTemplatesLoaded = true;

	if (!SnsPostTemplateTable)
	{
		return;
	}

	TArray<FFF_SnsPostTemplateRow*> AllRows;
	SnsPostTemplateTable->GetAllRows<FFF_SnsPostTemplateRow>(TEXT("UFF_CrowdInsightsSubsystem"), AllRows);
	for (const FFF_SnsPostTemplateRow* Row : AllRows)
	{
		if (!Row)
		{
			continue;
		}

		// 콘텐츠 작성 실수(화제와 다른 극성 라벨)를 로드 시점에 한 번씩 잡는다 — 실제 극성
		// 판정에는 영향 없다(GetTopicPolarity가 여전히 유일한 권위), 문장 라벨만 검증한다
		if (!FFF_SnsFeedGenerator::DoesSentimentMatchTopic(*Row))
		{
			UE_LOG(LogFestaFiestaNpc, Warning,
				TEXT("[SNS] TemplateId=%s의 Sentiment('%s')가 화제 %d의 실제 극성과 다르다 — CSV를 확인할 것"),
				*Row->TemplateId.ToString(), *Row->Sentiment.ToString(), Row->TopicId);
		}

		if (FFF_SnsFeedGenerator::ShouldIncludeTemplateRow(*Row))
		{
			TemplatesByTopic.FindOrAdd(Row->TopicId).Add(Row);
		}
	}
}

void UFF_CrowdInsightsSubsystem::FillAdmissionFeeSignals(FFF_SnsFeedSignals& Signals) const
{
	const UFF_FestivalProgressionSubsystem* Progression = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
	if (Progression == nullptr || !Progression->IsFestivalStarted())
	{
		return;
	}
	int32 MinGold = 0;
	int32 MaxGold = 0;
	int32 ReferenceGold = 0;
	Progression->GetAdmissionFeeRange(MinGold, MaxGold, ReferenceGold);
	Signals.AdmissionFeeGold = Progression->GetAdmissionFee();
	Signals.ReferenceAdmissionFeeGold = ReferenceGold;
	Signals.MaxAdmissionFeeGold = MaxGold;
}

void UFF_CrowdInsightsSubsystem::FillFacilitySignals(FFF_SnsFeedSignals& Signals) const
{
	const UFFZSWorldSubsystem* ZoneSystem = GetZoneSystemSubsystem();
	if (!ZoneSystem)
	{
		return;
	}

	const FFFZSReadModel ReadModel = ZoneSystem->BuildReadModel();
	Signals.FacilityCount = ReadModel.Facilities.Num();

	float WorstWaitSeconds = 0.f;
	int32 MaxWaiting = 0;
	for (const FFFZSFacilityReadModel& Facility : ReadModel.Facilities)
	{
		if (Facility.AverageCompletedWaitSeconds > WorstWaitSeconds)
		{
			WorstWaitSeconds = Facility.AverageCompletedWaitSeconds;
			Signals.WorstWaitFacilityDisplayName = Facility.DisplayName.ToString();
		}
		if (Facility.WaitingCount > MaxWaiting)
		{
			MaxWaiting = Facility.WaitingCount;
			Signals.MostQueuedFacilityDisplayName = Facility.DisplayName.ToString();
		}
	}
	Signals.MaxWaitingCount = MaxWaiting;
}

UFFZSWorldSubsystem* UFF_CrowdInsightsSubsystem::GetZoneSystemSubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
}

UFF_SatisfactionMetricsSubsystem* UFF_CrowdInsightsSubsystem::GetSatisfactionSubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UFF_SatisfactionMetricsSubsystem>() : nullptr;
}

UFF_NpcNeedMetricsSubsystem* UFF_CrowdInsightsSubsystem::GetNeedMetricsSubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UFF_NpcNeedMetricsSubsystem>() : nullptr;
}

UFF_FestivalStageSubsystem* UFF_CrowdInsightsSubsystem::GetStageSubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
}

UFF_CrowdSimulationSubsystem* UFF_CrowdInsightsSubsystem::GetCrowdSimulationSubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;
}

UFF_CrowdDecisionSubsystem* UFF_CrowdInsightsSubsystem::GetCrowdDecisionSubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UFF_CrowdDecisionSubsystem>() : nullptr;
}
