// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "NPC/Insights/FF_SnsTypes.h"
#include "Time/FF_FestivalStage.h"
#include "FF_CrowdInsightsSubsystem.generated.h"

class UDataTable;
struct FFF_SnsPostTemplateRow;
class UFF_SatisfactionMetricsSubsystem;
class UFF_NpcNeedMetricsSubsystem;
class UFF_FestivalStageSubsystem;
class UFF_CrowdSimulationSubsystem;
class UFF_CrowdDecisionSubsystem;
class UFFZSWorldSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnSnsPostAdded, FFF_SnsPostRow, NewPost);

/**
 * SNS 라이브 피드 생성기 — `Live` 단계 동안 주기적으로 글 한 건을 만들어 링버퍼에 쌓는다.
 * 화제·문장 선택의 실제 계산은 `FFF_SnsFeedGenerator`(순수 함수 모음)가 한다.
 * GameInstanceSubsystem은 테스트에서 만들 수 없으므로 계산 로직을 UObject 밖으로 뺐다.
 *
 * 신호 출처는 `FF_SnsFeedGenerator.h`·`FillFacilitySignals` 주석 참조. 재방문(화제14)만
 * 아직 스코프 밖(고정 낮은 점수) — §10 결정대로 실측 전까지 보류.
 */
UCLASS()
class FESTAFIESTA_API UFF_CrowdInsightsSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
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

	// 테스트 전용 — 타이머를 기다리지 않고 지금 한 건 강제로 만들어 큐에 넣는다
	FFF_SnsPostRow Debug_ForceGeneratePost();

private:
	// Live 단계인지 확인하고, 간격 카운트다운이 0 이하면 신호를 모아 GeneratePost 호출
	void MaybeGeneratePost(float DeltaTime);
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

	// Report 단계 + 마지막 날이면 화제9(피날레)를 강제로 한 번만 생성
	UFUNCTION()
	void HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage);

	// 이하 다섯 개는 다른 서브시스템을 매번 새로 조회하는 헬퍼 — 캐시하지 않는다
	// (subsystem-scope-rule: GameInstance보다 짧게 사는 World 스코프 포인터를 들고 있지 않는다)
	UFF_SatisfactionMetricsSubsystem* GetSatisfactionSubsystem() const;
	UFF_NpcNeedMetricsSubsystem* GetNeedMetricsSubsystem() const;
	UFF_FestivalStageSubsystem* GetStageSubsystem() const;
	UFF_CrowdSimulationSubsystem* GetCrowdSimulationSubsystem() const;
	UFF_CrowdDecisionSubsystem* GetCrowdDecisionSubsystem() const;
	UFFZSWorldSubsystem* GetZoneSystemSubsystem() const;

	static constexpr int32 RingBufferCapacity = 80;

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
};
