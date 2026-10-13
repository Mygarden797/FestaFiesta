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
#include "Festival/FF_FestivalDefinition.h"
#include "Save/FF_SaveArchive.h"
#include "Save/FF_SaveSubsystem.h"
#include "Finale/FF_FinaleShowSubsystem.h"
#include "NPC/Integration/FF_ZoneSystemBridge.h"
#include "Queue/FF_QueueSubsystem.h"
#include "Queue/FF_QueueTypes.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSTypes.h"

void UFF_CrowdInsightsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UFF_SaveSubsystem* Save = Collection.InitializeDependency<UFF_SaveSubsystem>())
	{
		Save->RegisterSaveable(this);
	}

	if (const UFF_SimulationSettings* Settings = UFF_SimulationSettings::Get())
	{
		GlobalSimulationSeed = Settings->GlobalSimulationSeed;
		SnsPostTemplateTable = Settings->SnsPostTemplateTable.LoadSynchronous();
	}

	if (UFF_FestivalStageSubsystem* Stage = GetStageSubsystem())
	{
		Stage->OnFestivalEnded.AddDynamic(this, &UFF_CrowdInsightsSubsystem::HandleFestivalEnded);
		Stage->OnFestivalStarted.AddUniqueDynamic(this, &UFF_CrowdInsightsSubsystem::HandleFestivalStarted);
		Stage->OnStageChanged.AddUniqueDynamic(this, &UFF_CrowdInsightsSubsystem::HandleStageChanged);
		Stage->OnMainProgramStarted.AddUniqueDynamic(this, &UFF_CrowdInsightsSubsystem::HandleMainProgramStarted);
		Stage->OnMainProgramEnded.AddUniqueDynamic(this, &UFF_CrowdInsightsSubsystem::HandleMainProgramEnded);
	}

	ScheduleNextPost();
}

void UFF_CrowdInsightsSubsystem::Deinitialize()
{
	if (UFF_FestivalStageSubsystem* Stage = GetStageSubsystem())
	{
		Stage->OnFestivalEnded.RemoveDynamic(this, &UFF_CrowdInsightsSubsystem::HandleFestivalEnded);
		Stage->OnFestivalStarted.RemoveDynamic(this, &UFF_CrowdInsightsSubsystem::HandleFestivalStarted);
		Stage->OnStageChanged.RemoveDynamic(this, &UFF_CrowdInsightsSubsystem::HandleStageChanged);
		Stage->OnMainProgramStarted.RemoveDynamic(this, &UFF_CrowdInsightsSubsystem::HandleMainProgramStarted);
		Stage->OnMainProgramEnded.RemoveDynamic(this, &UFF_CrowdInsightsSubsystem::HandleMainProgramEnded);
	}

	Super::Deinitialize();
}

void UFF_CrowdInsightsSubsystem::HandleFestivalStarted()
{
	if (const UFF_SimulationSettings* Settings = UFF_SimulationSettings::Get())
	{
		GlobalSimulationSeed = Settings->GlobalSimulationSeed;
	}
	RecentPosts.Reset();
	ReactedPostIds.Reset();
	PostSequenceIndex = 0;
	LastObservedFailedDestinationPenalized = 0;
	ScheduleNextPost();

	bFinaleFeedActive = false;
	FinalePosts.Reset();
	FinaleLineCycleByTopic.Reset();
	FinaleUsedCyclesByTopic.Reset();
	FinalePostCount = 0;
	OnSnsFeedReset.Broadcast();
}

float UFF_CrowdInsightsSubsystem::GetReactionBuzzReward() const
{
	const UGameInstance* Game = GetGameInstance();
	const UFF_FestivalProgressionSubsystem* Progression = Game
		? Game->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
	const UFF_FestivalDefinition* Definition = Progression ? Progression->GetDefinition() : nullptr;
	return Definition && FMath::IsFinite(Definition->BuzzPerSnsReaction)
		? FMath::Max(0.f, Definition->BuzzPerSnsReaction) : 0.f;
}

bool UFF_CrowdInsightsSubsystem::IsReactionAvailable() const
{
	const UGameInstance* Game = GetGameInstance();
	const UFF_FestivalProgressionSubsystem* Progression = Game
		? Game->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
	const UFF_FestivalStageSubsystem* Stage = GetStageSubsystem();
	const UFF_SaveSubsystem* Save = Game ? Game->GetSubsystem<UFF_SaveSubsystem>() : nullptr;
	return !bReactionInFlight && Game && Game->GetSubsystem<UFF_FestivalBuzzSubsystem>()
		&& Progression && Progression->IsFestivalStarted() && !Progression->IsFestivalEvaluationFrozen()
		&& Stage && Stage->bFestivalStarted && !Stage->HasFestivalEnded()
		&& (!Save || !Save->IsRestoring());
}

FFF_SnsReactionState UFF_CrowdInsightsSubsystem::GetReactionState(const FGuid PostId) const
{
	FFF_SnsReactionState State;
	State.bExists = PostId.IsValid() && RecentPosts.ContainsByPredicate(
		[PostId](const FFF_SnsPostRow& Post) { return Post.PostId == PostId; });
	State.bReacted = PostId.IsValid() && ReactedPostIds.Contains(PostId);
	State.bCanReact = State.bExists && !State.bReacted && IsReactionAvailable();
	State.BuzzReward = GetReactionBuzzReward();
	return State;
}

FFF_SnsReactionOutcome UFF_CrowdInsightsSubsystem::TryReactToPost(const FGuid PostId)
{
	FFF_SnsReactionOutcome Outcome;
	const FFF_SnsReactionState State = GetReactionState(PostId);
	if (!State.bExists)
	{
		Outcome.Result = EFF_SnsReactionResult::PostUnavailable;
		return Outcome;
	}
	if (State.bReacted)
	{
		Outcome.Result = EFF_SnsReactionResult::AlreadyReacted;
		return Outcome;
	}
	if (!State.bCanReact)
	{
		return Outcome;
	}

	UFF_FestivalBuzzSubsystem* Buzz = GetGameInstance()->GetSubsystem<UFF_FestivalBuzzSubsystem>();
	// 값 변경 알림에서 재호출되더라도 같은 글이나 다른 글에 중복 지급하지 않는다.
	{
		TGuardValue<bool> CommandGuard(bReactionInFlight, true);
		ReactedPostIds.Add(PostId);
		Outcome.AppliedBuzz = Buzz->AddBuzzWithReceipt(State.BuzzReward, TEXT("SnsReaction"));
		Outcome.Result = EFF_SnsReactionResult::Success;
	}
	OnSnsReactionChanged.Broadcast(PostId);
	return Outcome;
}

void UFF_CrowdInsightsSubsystem::WriteSaveData(TArray<uint8>& OutBytes) const
{
	FFF_SnsFeedSave State;
	State.GlobalSimulationSeed = GlobalSimulationSeed;
	State.PostSequenceIndex = PostSequenceIndex;
	State.RecentPosts = RecentPosts;
	State.ReactedPostIds = ReactedPostIds.Array();
	UFF_SaveArchive::WriteStruct(OutBytes, State);
}

void UFF_CrowdInsightsSubsystem::ReadSaveData(const TArray<uint8>& InBytes)
{
	FFF_SnsFeedSave State;
	if (!UFF_SaveArchive::ReadStruct(InBytes, State) || State.Version != 1)
	{
		// 공감 기능 이전의 저장에는 피드도 공감도 없다. 이전 세션 상태를 남기지 않는다.
		if (const UFF_SimulationSettings* Settings = UFF_SimulationSettings::Get())
		{
			GlobalSimulationSeed = Settings->GlobalSimulationSeed;
		}
		HandleFestivalStarted();
		return;
	}

	GlobalSimulationSeed = State.GlobalSimulationSeed;
	PostSequenceIndex = FMath::Max(0, State.PostSequenceIndex);
	RecentPosts.Reset();
	TSet<FGuid> RestoredIds;
	for (const FFF_SnsPostRow& Post : State.RecentPosts)
	{
		if (RecentPosts.Num() >= RingBufferCapacity) break;
		if (!Post.PostId.IsValid() || RestoredIds.Contains(Post.PostId)) continue;
		RestoredIds.Add(Post.PostId);
		RecentPosts.Add(Post);
	}
	ReactedPostIds.Reset();
	for (const FGuid& PostId : State.ReactedPostIds)
	{
		if (PostId.IsValid()) ReactedPostIds.Add(PostId);
	}
	LastObservedFailedDestinationPenalized = 0;
	ScheduleNextPost();
	bFinaleFeedActive = false;
	FinalePosts.Reset();
	FinaleLineCycleByTopic.Reset();
	FinaleUsedCyclesByTopic.Reset();
	FinalePostCount = 0;
	UsedFinaleBodies.Reset();
	// PushPost는 생성 시 화제성 효과를 실행하므로 복원에는 사용하지 않는다.
	OnSnsFeedReset.Broadcast();
}

void UFF_CrowdInsightsSubsystem::HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage)
{
	(void)PreviousStage;
	if (NewStage == EFF_FestivalStage::Live)
	{
		// 하루의 시작. 어제 시설을 썼다거나 어제 공연이 끝났다는 사실을 오늘로 끌고 오지 않는다
		bFacilityUsedToday = false;
		bMainShowRunning = false;
		MainShowEndedAtLiveSeconds = -1.f;
	}
	if (NewStage != EFF_FestivalStage::Finale)
	{
		bFinaleFeedActive = false;
		return;
	}

	const UFF_FestivalProgressionSubsystem* Progression = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
	StartFinaleFeed(Progression ? Progression->GetPurchasedFinaleGrade() : EFF_FinaleGrade::None);
}

void UFF_CrowdInsightsSubsystem::Debug_StartFinaleFeed(EFF_FinaleGrade Grade)
{
	StartFinaleFeed(Grade);
}

void UFF_CrowdInsightsSubsystem::StartFinaleFeed(EFF_FinaleGrade Grade)
{
	FinaleGrade = Grade;
	bFinaleFeedActive = true;
	bFinaleShowSeen = false;
	FinaleQuietSeconds = 0.f;
	FinaleElapsedSeconds = 0.f;
	FinaleLineCycleByTopic.Reset();
	FinaleUsedCyclesByTopic.Reset();
	UsedFinaleBodies.Reset();
	TimeUntilNextFinalePostSeconds = NextFinaleIntervalSeconds(/*bHype=*/true);
}

void UFF_CrowdInsightsSubsystem::MaybeGenerateFinalePost(float DeltaTime)
{
	if (!bFinaleFeedActive)
	{
		return;
	}

	FinaleElapsedSeconds += DeltaTime;

	// 불꽃이 다 끝나고도 단계는 플레이어가 넘길 때까지 남는다. 끝난 뒤 조금만 더 올리고 멈춘다.
	const UFF_FinaleShowSubsystem* Show = GetFinaleShowSubsystem();
	if (Show && Show->IsShowInProgress())
	{
		bFinaleShowSeen = true;
		FinaleQuietSeconds = 0.f;
	}
	else
	{
		FinaleQuietSeconds += DeltaTime;
		if (FinaleQuietSeconds >= (bFinaleShowSeen ? FinaleAfterShowSeconds : FinaleWithoutShowSeconds))
		{
			bFinaleFeedActive = false;
			return;
		}
	}

	TimeUntilNextFinalePostSeconds -= DeltaTime;
	if (TimeUntilNextFinalePostSeconds > 0.f)
	{
		return;
	}

	EnsureTemplatesLoaded();

	const UDataTable* CoefficientTable = nullptr;
	if (const UFF_SimulationSettings* Settings = UFF_SimulationSettings::Get())
	{
		CoefficientTable = Settings->UtilityCoefficientTable.LoadSynchronous();
	}
	const float HypeSeconds = FFF_UtilityCoefficientHelpers::GetCoefficient(CoefficientTable, TEXT("sns_finale_hype_seconds"), 6.f);
	const int32 FeedEvery = FMath::Max(1, FMath::RoundToInt32(
		FFF_UtilityCoefficientHelpers::GetCoefficient(CoefficientTable, TEXT("sns_finale_feed_every"), 5.f)));

	const bool bHype = FinaleElapsedSeconds < HypeSeconds;
	const int32 TopicId = bHype
		? FFF_SnsFeedGenerator::FinaleHypeTopicId
		: FFF_SnsFeedGenerator::GetFinaleVerdictTopicId(FinaleGrade);
	int32& LineCycle = FinaleLineCycleByTopic.FindOrAdd(TopicId);
	TSet<int32>& UsedCycles = FinaleUsedCyclesByTopic.FindOrAdd(TopicId);

	// 태극기·"끝났다" 같은 문장은 그 장면이 지난 뒤에만 쓴다. 건너뛴 문장은 남겨 두었다가 시점이 맞으면 나온다.
	const bool bShowEnded = bFinaleShowSeen && !(Show && Show->IsShowInProgress());
	const int32 Cycle = FFF_SnsFeedGenerator::FindTimelyFinaleCycle(GlobalSimulationSeed, TopicId, LineCycle,
		TemplatesByTopic, UsedCycles, Show && Show->HasFiredFinalShape(), bShowEnded);

	const UFF_FestivalStageSubsystem* Stage = GetStageSubsystem();
	const FFF_SnsFeedSignals Signals; // 화제를 고정하므로 신호는 안 쓴다
	FFF_SnsPostRow NewPost = FFF_SnsFeedGenerator::GeneratePost(
		GlobalSimulationSeed, PostSequenceIndex, Signals, TemplatesByTopic,
		Stage ? Stage->ElapsedLiveSeconds : 0.f, TopicId, Cycle);
	++PostSequenceIndex;

	// 풀을 한 바퀴 다 쓰면 그다음 바퀴부터 추임새를 붙인다. 이미 나간 문장과 같으면 다음 조합으로 넘긴다.
	const TArray<const FFF_SnsPostTemplateRow*>* Pool = TemplatesByTopic.Find(TopicId);
	const int32 PoolSize = Pool ? FMath::Max(1, Pool->Num()) : 1;
	NewPost.Body = FText::FromString(FFF_SnsFeedGenerator::MakeUniqueDecoratedBody(
		NewPost.Body.ToString(), NewPost.Sentiment, Cycle / PoolSize,
		GetTypeHash(NewPost.MessageTemplateId), UsedFinaleBodies));
	UsedCycles.Add(Cycle);
	while (UsedCycles.Remove(LineCycle) > 0)
	{
		++LineCycle;
	}

	FinalePosts.Insert(NewPost, 0);
	if (FinalePosts.Num() > FinalePostCapacity)
	{
		FinalePosts.SetNum(FinalePostCapacity);
	}
	OnFinalePostAdded.Broadcast(NewPost);

	if (FinalePostCount % FeedEvery == 0)
	{
		PushPost(NewPost);
	}
	++FinalePostCount;

	// 프레임이 튀어도 밀린 글을 한꺼번에 쏟지 않는다
	TimeUntilNextFinalePostSeconds = FMath::Max(TimeUntilNextFinalePostSeconds, 0.f) + NextFinaleIntervalSeconds(bHype);
}

float UFF_CrowdInsightsSubsystem::NextFinaleIntervalSeconds(bool bHype) const
{
	const UDataTable* CoefficientTable = nullptr;
	if (const UFF_SimulationSettings* Settings = UFF_SimulationSettings::Get())
	{
		CoefficientTable = Settings->UtilityCoefficientTable.LoadSynchronous();
	}

	const TCHAR* Key = TEXT("sns_finale_hype_interval_seconds");
	float Fallback = 1.f;
	if (!bHype)
	{
		switch (FinaleGrade)
		{
		case EFF_FinaleGrade::C: Key = TEXT("sns_finale_interval_c_seconds"); Fallback = 1.2f; break;
		case EFF_FinaleGrade::B: Key = TEXT("sns_finale_interval_b_seconds"); Fallback = 0.8f; break;
		case EFF_FinaleGrade::A: Key = TEXT("sns_finale_interval_a_seconds"); Fallback = 0.55f; break;
		case EFF_FinaleGrade::S: Key = TEXT("sns_finale_interval_s_seconds"); Fallback = 0.35f; break;
		default:                 Key = TEXT("sns_finale_interval_none_seconds"); Fallback = 1.6f; break;
		}
	}
	const float Mean = FMath::Max(0.05f, FFF_UtilityCoefficientHelpers::GetCoefficient(CoefficientTable, Key, Fallback));
	const float Jitter = FMath::Clamp(
		FFF_UtilityCoefficientHelpers::GetCoefficient(CoefficientTable, TEXT("sns_finale_interval_jitter"), 0.4f), 0.f, 0.9f);

	return FFF_SnsFeedGenerator::NextIntervalSeconds(
		GlobalSimulationSeed, PostSequenceIndex, Mean * (1.f - Jitter), Mean * (1.f + Jitter));
}

void UFF_CrowdInsightsSubsystem::HandleFestivalEnded()
{
	UFF_FestivalStageSubsystem* Stage = GetStageSubsystem();
	if (!Stage)
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
	MaybeGenerateFinalePost(DeltaTime);
}

void UFF_CrowdInsightsSubsystem::MaybeGeneratePost(float DeltaTime)
{
	UFF_FestivalStageSubsystem* Stage = GetStageSubsystem();
	if (!Stage || !Stage->IsLiveStage())
	{
		return;
	}

	const float ScaledDelta = DeltaTime * FMath::Max(0.f, Stage->GetEffectiveTimeScale());
	TimeUntilNextPostSeconds -= ScaledDelta;
	if (TimeUntilNextPostSeconds > 0.f)
	{
		return;
	}

	EnsureTemplatesLoaded();

	FFF_SnsFeedSignals Signals;
	FillLiveSignals(Signals);

	// ⑤·⑥은 지난 글 이후 쌓인 이동 실패 수다. 기준값은 글을 낼 때만 옮긴다 — 매 틱 옮기면
	// 글을 내는 순간에는 한 프레임치 변화만 남아 두 화제가 거의 안 나온다.
	if (UFF_CrowdDecisionSubsystem* Decision = GetCrowdDecisionSubsystem())
	{
		const int32 Current = Decision->GetDecisionStats().FailedDestinationPenalized;
		Signals.FailedDestinationDeltaThisWindow = FMath::Max(0, Current - LastObservedFailedDestinationPenalized);
		LastObservedFailedDestinationPenalized = Current;
	}

	const FFF_SnsPostRow NewPost = FFF_SnsFeedGenerator::GeneratePost(
		GlobalSimulationSeed, PostSequenceIndex, Signals, TemplatesByTopic, Stage->ElapsedLiveSeconds);
	++PostSequenceIndex;

	// 지금 조건에 맞는 화제가 하나도 없으면 이번 차례는 건너뛴다
	if (!NewPost.MessageTemplateId.IsNone())
	{
		PushPost(NewPost);
	}
	ScheduleNextPost();
}

void UFF_CrowdInsightsSubsystem::FillLiveSignals(FFF_SnsFeedSignals& Signals)
{
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
		for (int32 Index = 0; Index < static_cast<int32>(EFF_NeedType::Count); ++Index)
		{
			Signals.NeedCoverage[Index] = Crowd->GetNeedCoverage(static_cast<EFF_NeedType>(Index));
		}
	}
	if (UFF_FestivalStageSubsystem* Stage = GetStageSubsystem())
	{
		Signals.bMainProgramLocked = Stage->IsMainProgramLocked();
		Signals.CurrentDay = Stage->CurrentDay;
		const float LiveDuration = Stage->GetLiveDurationForCurrentDay();
		Signals.LiveElapsedFraction = LiveDuration > 0.f
			? FMath::Clamp(Stage->ElapsedLiveSeconds / LiveDuration, 0.f, 1.f) : 0.f;
		Signals.bMainShowRunning = bMainShowRunning;
		Signals.bMainShowJustEnded = !bMainShowRunning && MainShowEndedAtLiveSeconds >= 0.f
			&& Stage->ElapsedLiveSeconds - MainShowEndedAtLiveSeconds <= MainShowAfterglowLiveSeconds;
	}
	FillFacilitySignals(Signals);
	FillAdmissionFeeSignals(Signals);

	// 지금 이용 중인 손님을 한 번이라도 보면 그날은 "시설을 쓴 날"로 남긴다
	bFacilityUsedToday |= Signals.bFacilityUsedToday;
	Signals.bFacilityUsedToday = bFacilityUsedToday;
}

void UFF_CrowdInsightsSubsystem::HandleMainProgramStarted(FString EventLabel)
{
	(void)EventLabel;
	bMainShowRunning = true;
}

void UFF_CrowdInsightsSubsystem::HandleMainProgramEnded(FString EventLabel)
{
	(void)EventLabel;
	bMainShowRunning = false;
	const UFF_FestivalStageSubsystem* Stage = GetStageSubsystem();
	MainShowEndedAtLiveSeconds = Stage ? Stage->ElapsedLiveSeconds : -1.f;
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
	// 화제9(축제 총평)와 피날레 연출 후기는 평가 직전에 나가는 글이라 점수를 움직이지 않게 뺀다.
	UFF_FestivalBuzzSubsystem* Buzz = NewPost.TopicId != 9 && !FFF_SnsFeedGenerator::IsFinaleShowTopic(NewPost.TopicId)
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
	FillLiveSignals(Signals);

	const float Timestamp = GetStageSubsystem() ? GetStageSubsystem()->ElapsedLiveSeconds : 0.f;
	FFF_SnsPostRow NewPost = FFF_SnsFeedGenerator::GeneratePost(
		GlobalSimulationSeed, PostSequenceIndex, Signals, TemplatesByTopic, Timestamp);
	++PostSequenceIndex;

	if (!NewPost.MessageTemplateId.IsNone())
	{
		PushPost(NewPost);
	}
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

	// 줄은 팀 대기열이 가진다. ZoneSystem 읽기 모델의 대기·이용 수는 팀 대기열을 쓰는 시설에서
	// 비어 있으므로, 등록된 시설은 ViewModel 과 같이 팀 대기열 값을 읽는다.
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	const UFF_ZoneSystemQueueBridgeSubsystem* QueueBridge = World ? World->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>() : nullptr;
	const UFF_QueueSubsystem* Queue = World ? World->GetSubsystem<UFF_QueueSubsystem>() : nullptr;

	const FFFZSReadModel ReadModel = ZoneSystem->BuildReadModel();

	// 어제 매출 1위 종류. 원장은 종류(FacilityDefinitionId)로 남으므로 같은 종류의 배치된 시설에서 이름을 읽는다
	FName YesterdayTopDefinitionId;
	if (Signals.CurrentDay >= 2)
	{
		const UFF_FestivalProgressionSubsystem* Progression = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
		if (Progression && Progression->HasLedgerForDay(Signals.CurrentDay - 1))
		{
			int32 TopAmount = 0;
			for (const FFF_FacilityRevenueRow& Row : Progression->GetRevenueByFacilityRowsForDay(Signals.CurrentDay - 1))
			{
				if (Row.Amount > TopAmount)
				{
					TopAmount = Row.Amount;
					YesterdayTopDefinitionId = Row.FacilityId;
				}
			}
		}
	}

	float WorstWaitSeconds = 0.f;
	int32 MaxWaiting = 0;
	for (const FFFZSFacilityReadModel& Facility : ReadModel.Facilities)
	{
		if (Facility.State == EFFZSFacilityState::Removed
			|| !FFF_ZoneSystemBridge::IsGuestDestinationCategory(Facility.Category))
		{
			continue;
		}
		// 길·타일·조명·안내소까지 세면 "시설이 적다"는 글이 절대 안 나온다
		++Signals.FacilityCount;

		if (!YesterdayTopDefinitionId.IsNone() && Facility.FacilityDefinitionId == YesterdayTopDefinitionId
			&& Signals.YesterdayTopFacilityDisplayName.IsEmpty())
		{
			Signals.YesterdayTopFacilityDisplayName = Facility.DisplayName.ToString();
		}

		int32 Waiting = Facility.WaitingCount;
		int32 Active = Facility.ActiveServiceCount;
		float AverageWait = Facility.AverageCompletedWaitSeconds;
		if (QueueBridge && Queue)
		{
			const int32 QueueFacilityId = QueueBridge->FindQueueFacilityId(Facility.FacilityInstanceId);
			if (QueueFacilityId != INDEX_NONE && Queue->IsFacilityRegistered(QueueFacilityId))
			{
				const FFF_QueueState TeamQueue = QueueBridge->GetZoneFacilityQueueState(Facility.FacilityInstanceId);
				Waiting = FMath::Max(0, TeamQueue.WaitingPartyCount - TeamQueue.ActiveServicePartyCount);
				Active = TeamQueue.ActiveServicePartyCount;
				AverageWait = TeamQueue.CompletedPartyCount > 0 ? TeamQueue.AverageCompletedWaitSeconds : 0.f;
			}
		}

		if (AverageWait > WorstWaitSeconds)
		{
			WorstWaitSeconds = AverageWait;
			Signals.WorstWaitFacilityDisplayName = Facility.DisplayName.ToString();
		}
		if (Waiting > MaxWaiting)
		{
			MaxWaiting = Waiting;
			Signals.MostQueuedFacilityDisplayName = Facility.DisplayName.ToString();
		}
		Signals.bFacilityUsedToday |= Active > 0;
	}
	Signals.MaxWaitingCount = MaxWaiting;
}

UFFZSWorldSubsystem* UFF_CrowdInsightsSubsystem::GetZoneSystemSubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
}

UFF_FinaleShowSubsystem* UFF_CrowdInsightsSubsystem::GetFinaleShowSubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UFF_FinaleShowSubsystem>() : nullptr;
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
