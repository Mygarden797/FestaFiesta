// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "NPC/Insights/FF_SnsTypes.h"
#include "Time/FF_FestivalStage.h"
#include "Economy/FF_FestivalEconomyTypes.h"
#include "Save/FF_Saveable.h"
#include "FF_CrowdInsightsSubsystem.generated.h"

class UDataTable;
struct FFF_SnsPostTemplateRow;
class UFF_SatisfactionMetricsSubsystem;
class UFF_NpcNeedMetricsSubsystem;
class UFF_FestivalStageSubsystem;
class UFF_CrowdSimulationSubsystem;
class UFF_CrowdDecisionSubsystem;
class UFFZSWorldSubsystem;
class UFF_FinaleShowSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnSnsPostAdded, FFF_SnsPostRow, NewPost);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnSnsReactionChanged, FGuid, PostId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnSnsFeedReset);

// 글 식별자와 공감 이력은 함께 복원한다. 순번만 초기화하면 다른 글에 같은 PostId가 재사용된다.
USTRUCT()
struct FFF_SnsFeedSave
{
	GENERATED_BODY()

	UPROPERTY() int32 Version = 1;
	UPROPERTY() int32 GlobalSimulationSeed = 0;
	UPROPERTY() int32 PostSequenceIndex = 0;
	UPROPERTY() TArray<FFF_SnsPostRow> RecentPosts;
	UPROPERTY() TArray<FGuid> ReactedPostIds;
};

/**
 * SNS 라이브 피드 생성기 — `Live` 단계 동안 주기적으로 글 한 건을 만들어 링버퍼에 쌓는다.
 * 화제·문장 선택의 실제 계산은 `FFF_SnsFeedGenerator`(순수 함수 모음)가 한다.
 * GameInstanceSubsystem은 테스트에서 만들 수 없으므로 계산 로직을 UObject 밖으로 뺐다.
 *
 * 신호 출처는 `FF_SnsFeedGenerator.h`·`FillFacilitySignals` 주석 참조. 재방문(화제14)만
 * 아직 스코프 밖(고정 낮은 점수) — §10 결정대로 실측 전까지 보류.
 */
UCLASS()
class FESTAFIESTA_API UFF_CrowdInsightsSubsystem : public UGameInstanceSubsystem, public FTickableGameObject, public IFF_Saveable
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 매 프레임 호출 — 실제 판단은 MaybeGeneratePost에 위임
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_CrowdInsightsSubsystem, STATGROUP_Tickables);
	}

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|SNS")
	FFF_OnSnsPostAdded OnSnsPostAdded;

	// 최신 글이 [0]. 최대 `RingBufferCapacity`개
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|SNS")
	const TArray<FFF_SnsPostRow>& GetRecentPosts() const { return RecentPosts; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|SNS")
	FFF_SnsReactionState GetReactionState(FGuid PostId) const;

	// 실제 피드의 글만 한 번 공감한다. 공감은 불만을 해결하거나 게시글 생성 효과를 다시 실행하지 않는다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|SNS")
	FFF_SnsReactionOutcome TryReactToPost(FGuid PostId);

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|SNS")
	FFF_OnSnsReactionChanged OnSnsReactionChanged;

	// 새 축제·저장 복원 시 현재 피드를 다시 읽는다. 글 생성 이벤트와 분리해 보상을 재적용하지 않는다.
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|SNS")
	FFF_OnSnsFeedReset OnSnsFeedReset;

	virtual void WriteSaveData(TArray<uint8>& OutBytes) const override;
	virtual void ReadSaveData(const TArray<uint8>& InBytes) override;
	virtual FName GetSaveKey() const override { return TEXT("SnsFeed"); }
	virtual EFF_SaveLoadPhase GetSaveLoadPhase() const override { return EFF_SaveLoadPhase::Presentation; }

	// 테스트 전용 — 타이머를 기다리지 않고 지금 한 건 강제로 만들어 큐에 넣는다
	FFF_SnsPostRow Debug_ForceGeneratePost();

	/**
	 * 피날레 연출 중 후기. 피날레 단계 동안 몇 초 간격으로 계속 나온다.
	 * 처음 몇 초는 기대감, 그 뒤로는 구매 등급에 맞는 평가이고, 등급이 높을수록 간격이 짧다.
	 * 전부 피날레 목록에 쌓고, 피드에는 몇 건에 한 건만 넣어 낮 후기가 밀려나지 않게 한다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|SNS")
	FFF_OnSnsPostAdded OnFinalePostAdded;

	// 최신 글이 [0]. 새 축제를 시작할 때 비운다
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|SNS")
	const TArray<FFF_SnsPostRow>& GetFinalePosts() const { return FinalePosts; }

	// 확인용 — 피날레 단계를 기다리지 않고 지정 등급으로 연출 후기를 시작한다
	void Debug_StartFinaleFeed(EFF_FinaleGrade Grade);

private:
	friend class FFF_SnsReactionPersistenceTest;
	friend class FFF_SnsReactionRewardTest;

	float GetReactionBuzzReward() const;
	bool IsReactionAvailable() const;

	// Live 단계인지 확인하고, 간격 카운트다운이 0 이하면 신호를 모아 GeneratePost 호출
	void MaybeGeneratePost(float DeltaTime);
	// 만족도·혼잡·단계·시설·입장료·공연 상태를 한 번에 채운다. 주기 글과 강제 글이 같은 신호를 쓰게 한다
	void FillLiveSignals(struct FFF_SnsFeedSignals& Signals);

	UFUNCTION()
	void HandleMainProgramStarted(FString EventLabel);
	UFUNCTION()
	void HandleMainProgramEnded(FString EventLabel);

	// 링버퍼 맨 앞에 삽입하고 용량 초과분을 잘라낸 뒤 델리게이트 방송
	void PushPost(const FFF_SnsPostRow& NewPost);
	// 계수 테이블에서 Min/Max를 읽어 다음 간격을 결정론적으로 예약
	void ScheduleNextPost();
	// DataTable을 최초 1회만 읽어 화제별 풀(TemplatesByTopic)로 캐싱, 자리표시자 행은 제외
	void EnsureTemplatesLoaded();
	// ZoneSystem의 시설 목록을 훑어 시설 수·최장 대기 시설·최다 대기줄 시설을 Signals에 채운다
	void FillFacilitySignals(struct FFF_SnsFeedSignals& Signals) const;
	// 오늘 입장료와 기준값을 Signals에 채운다(화제15·16). 축제 전이면 둘 다 0 이라 두 화제 점수가 0 이다
	void FillAdmissionFeeSignals(struct FFF_SnsFeedSignals& Signals) const;

	// 축제 종료(마지막 날)에 화제9(피날레)를 강제로 한 번만 생성. 종료 이벤트가 한 번만 나간다
	UFUNCTION()
	void HandleFestivalEnded();

	// 새 축제를 시작하면 이전 판의 피드·시퀀스·델타 기준을 비운다. 이 서브시스템은 레벨을 건너 산다
	UFUNCTION()
	void HandleFestivalStarted();

	// 피날레 단계에 들어가면 연출 후기를 시작하고, 나가면 멈춘다
	UFUNCTION()
	void HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage);

	void StartFinaleFeed(EFF_FinaleGrade Grade);
	void MaybeGenerateFinalePost(float DeltaTime);
	// 다음 피날레 후기까지의 간격. 기대감 구간인지와 등급에 따라 평균이 다르다
	float NextFinaleIntervalSeconds(bool bHype) const;

	// 이하 다섯 개는 다른 서브시스템을 매번 새로 조회하는 헬퍼 — 캐시하지 않는다
	// (subsystem-scope-rule: GameInstance보다 짧게 사는 World 스코프 포인터를 들고 있지 않는다)
	UFF_SatisfactionMetricsSubsystem* GetSatisfactionSubsystem() const;
	UFF_NpcNeedMetricsSubsystem* GetNeedMetricsSubsystem() const;
	UFF_FestivalStageSubsystem* GetStageSubsystem() const;
	UFF_CrowdSimulationSubsystem* GetCrowdSimulationSubsystem() const;
	UFF_CrowdDecisionSubsystem* GetCrowdDecisionSubsystem() const;
	UFFZSWorldSubsystem* GetZoneSystemSubsystem() const;
	UFF_FinaleShowSubsystem* GetFinaleShowSubsystem() const;

	static constexpr int32 RingBufferCapacity = 80;

	// The settings hold only a soft path; keep the loaded table and cached row storage alive.
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> SnsPostTemplateTable;
	// topic id -> 그 화제에 속한 행들. 
	// DataTable을 매번 다시 훑지 않으려 첫 사용 시 한 번만 채운다
	TMap<int32, TArray<const FFF_SnsPostTemplateRow*>> TemplatesByTopic;
	bool bTemplatesLoaded = false;

	// FF_SnsFeedGenerator에 그대로 넘겨줄 결정론 재료
	int32 GlobalSimulationSeed = 0;
	int32 PostSequenceIndex = 0;
	
	// 다음 글까지 남은 시간(카운트다운)
	float TimeUntilNextPostSeconds = 0.f;

	// §10 화제 5·6 델타 계산용 — 마지막으로 관측한 누적값
	int32 LastObservedFailedDestinationPenalized = 0;

	// 실제 글이 쌓이는 배열
	TArray<FFF_SnsPostRow> RecentPosts;
	TSet<FGuid> ReactedPostIds;
	bool bReactionInFlight = false;

	// 오늘 이용 중인 손님을 한 번이라도 봤는지. Live 가 시작될 때 비운다
	bool bFacilityUsedToday = false;

	// 메인 공연 진행 여부와 끝난 시각(Live 경과 초). 끝나고 MainShowAfterglowLiveSeconds 동안 여운 글이 나간다
	bool bMainShowRunning = false;
	float MainShowEndedAtLiveSeconds = -1.f;
	static constexpr float MainShowAfterglowLiveSeconds = 40.f;

	static constexpr int32 FinalePostCapacity = 400;

	bool bFinaleFeedActive = false;
	EFF_FinaleGrade FinaleGrade = EFF_FinaleGrade::None;
	// 피날레 단계에 들어간 뒤 흐른 실제 시간. 배속과 상관없이 같은 속도로 읽히게 한다
	float FinaleElapsedSeconds = 0.f;
	float TimeUntilNextFinalePostSeconds = 0.f;
	// 불꽃이 멈춘 뒤 흐른 시간. 연출이 끝나고 FinaleAfterShowSeconds, 연출이 한 번도 안 돌았으면
	// FinaleWithoutShowSeconds 가 지나면 후기를 멈춘다
	float FinaleQuietSeconds = 0.f;
	bool bFinaleShowSeen = false;
	static constexpr float FinaleAfterShowSeconds = 10.f;
	static constexpr float FinaleWithoutShowSeconds = 20.f;
	int32 FinalePostCount = 0;
	// 화제별로 지금까지 몇 번째 문장까지 썼는지. 한 바퀴 돌기 전에는 같은 문장이 다시 안 나온다
	TMap<int32, int32> FinaleLineCycleByTopic;
	// 시점이 안 맞아 건너뛰었다가 앞질러 쓴 차례. LineCycle 이 여기 닿으면 건너뛴다
	TMap<int32, TSet<int32>> FinaleUsedCyclesByTopic;
	// 이번 피날레에서 이미 나간 문장. 추임새를 붙여서라도 같은 문장이 두 번 보이지 않게 한다
	TSet<FString> UsedFinaleBodies;
	TArray<FFF_SnsPostRow> FinalePosts;
};
