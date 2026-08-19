// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CrowdSystem/Systems/FF_CrowdActorPool.h"
#include "CrowdSystem/Systems/FF_CrowdDensityGrid.h"
#include "CrowdSystem/Systems/FF_CrowdForces.h"
#include "CrowdSystem/Systems/FF_CrowdSpatialGrid.h"
#include "CrowdSystem/Types/FF_CrowdAvoidanceConfig.h"
#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"
#include "Math/RandomStream.h"
#include "NPC/Party/FF_NeedRules.h"
#include "NPC/Party/FF_VisitorDistributionBuilder.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/SubclassOf.h"
#include "FF_CrowdSimulationSubsystem.generated.h"

class AFF_BaseCrowd;
class UDataTable;

/**
 * "이 파티에 목적지가 필요하다"를 결정 계층에 묻는다.
 *
 * ### 멀티캐스트가 아닌 이유
 * 결정 권한은 하나여야 한다. 구독자가 둘이면 나중에 실행된 쪽이 먼저 실행된 쪽의 목적지를
 * 덮어쓰는데, 어느 쪽이 이겼는지 로그에 남지 않는다.
 *
 * ### Crowd가 조정자를 include하지 않기 위한 장치다
 * Crowd가 조정자를 직접 호출하면 Zone·Utility·Queue 헤더가 전이적으로 Crowd에 딸려 들어온다.
 * 방향을 뒤집으면 Crowd는 "누가 답하는지" 모르는 상태로 남는다.
 *
 * @return false = 목적지를 정하지 못했다(후보 없음 등). Crowd가 배회로 폴백한다.
 *         구독자가 아예 없을 때도 같은 경로를 탄다 — 그래서 조정자 없이도 단독으로 돌아간다.
 */
DECLARE_DELEGATE_RetVal_TwoParams(bool, FFF_OnPartyNeedsDecision,
	const FFF_PartyDecisionContext& , FFF_PartyDecisionResult& );

/**
 * 군중 시뮬레이션을 관리하는 TickableWorldSubsystem
 * Actor 풀과 Agent 풀을 분리 관리하여 필요한 Agent에만 Actor를 바인딩하는 방식으로 동작
 *
 * 한 프레임의 흐름:
 *   Tick
 *     -> StepSimulation을 고정 스텝으로 0~MaxStepsPerFrame회 (Agent 데이터만 갱신)
 *     -> DensityGrid.Update            (혼잡도 계측)
 *     -> PushPresentationToActors      (바인딩된 Actor에만 결과를 밀어 넣음)
 *
 * Actor는 시뮬레이션에 전혀 참여하지 않는다. 계산이 끝난 위치/회전/속도를 받기만 한다.
 */
UCLASS()
class FESTAFIESTA_API UFF_CrowdSimulationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;

	// 월드 시작 시 Actor/Agent 풀을 초기화
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// 통계 시스템(Stat Group)에 등록할 StatId 반환
	virtual TStatId GetStatId() const override;

	// 지정한 AgentIndex에 비어있는 Actor 슬롯을 바인딩
	void BindAgentToPool(int32 AgentIndex);

	// AgentIndex에 바인딩된 Actor를 다시 풀로 반환
	void ReleaseAgentToPool(int32 AgentIndex);

	// Agent 풀에서 빈 슬롯을 찾아 새 Agent를 스폰하고, 스폰된 Agent의 인덱스를 반환
	int32 SpawnAgent(UWorld& InWorld, FVector SpawnLocation, FRotator SpawnRotation);

	// ActorPoolSize만큼 CrowdActorClass Actor를 미리 생성하여 CrowdPool을 채움
	void InitializeCrowdActorPool(UWorld& InWorld, FVector ActorPoolLocation);

	// AgentPoolSize만큼 Agent 데이터(UnitDatas)를 미리 생성하여 Agent 풀을 채움
	void InitializeCrowdAgentPool(UWorld& InWorld, FVector AgentPoolLocation);

	// --- 시뮬레이션 ------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Crowd")
	void InitializeSimulation(FVector NewOrigin, float NewRadiusUU,
		int32 NewActorPoolSize, int32 NewAgentPoolSize, TSubclassOf<AFF_BaseCrowd> InCrowdActorClass);

	UFUNCTION(BlueprintCallable, Category = "Crowd")
	void ConfigureSimArea(FVector NewOrigin, float NewRadiusUU);

	// InitializeSimulation이 이미 끝났는지
	UFUNCTION(BlueprintPure, Category = "Crowd")
	bool IsSimulationInitialized() const { return bSimulationInitialized; }

	// Count명을 SpawnCenter 반경 SpawnRadiusUU 안에 흩뿌려 스폰하고, 실제 스폰된 수를 반환
	UFUNCTION(BlueprintCallable, Category = "Crowd")
	int32 SpawnAgents(int32 Count, FVector SpawnCenter, float SpawnRadiusUU);

	// AgentIndex의 목적지를 지정. 도착 판정은 ArrivalRadiusUU로 한다
	UFUNCTION(BlueprintCallable, Category = "Crowd")
	void SetAgentDestination(int32 AgentIndex, FVector Destination);

	// 활성 Agent 수. Actor 바인딩 여부와 무관하다
	UFUNCTION(BlueprintPure, Category = "Crowd")
	int32 GetActiveAgentCount() const { return ActiveAgentCount; }

	// 현재 화면에 실체화된(= Actor가 붙은) Agent 수. 항상 GetActiveAgentCount 이하다
	UFUNCTION(BlueprintPure, Category = "Crowd")
	int32 GetBoundActorCount() const { return PoolBinding.GetBoundCount(); }

	// AgentIndex의 현재 시뮬레이션 위치. 범위 밖이면 ZeroVector
	UFUNCTION(BlueprintPure, Category = "Crowd")
	FVector GetAgentPosition(int32 AgentIndex) const
	{
		return UnitDatas.IsValidIndex(AgentIndex) ? UnitDatas[AgentIndex].Position : FVector::ZeroVector;
	}

	// AgentIndex가 향하고 있는 목적지. 범위 밖이면 ZeroVector.
	// 파티 멤버의 목적지는 리더 목적지 + 대형 오프셋이다
	UFUNCTION(BlueprintPure, Category = "Crowd")
	FVector GetAgentDestination(int32 AgentIndex) const
	{
		return UnitDatas.IsValidIndex(AgentIndex) ? UnitDatas[AgentIndex].TargetLocation : FVector::ZeroVector;
	}

	// AgentIndex가 활성 상태인지
	UFUNCTION(BlueprintPure, Category = "Crowd")
	bool IsAgentActive(int32 AgentIndex) const
	{
		return UnitDatas.IsValidIndex(AgentIndex) && UnitDatas[AgentIndex].bIsActive;
	}

	UFUNCTION(BlueprintPure, Category = "Crowd")
	int32 GetAgentPoolSize() const { return UnitDatas.Num(); }
	UFUNCTION(BlueprintPure, Category = "Crowd")
	FName GetAgentArchetypeID(int32 AgentIndex) const
	{
		return UnitDatas.IsValidIndex(AgentIndex) ? UnitDatas[AgentIndex].ArchetypeID : NAME_None;
	}
	UFUNCTION(BlueprintPure, Category = "Crowd")
	FFF_NeedState GetAgentNeeds(int32 AgentIndex) const
	{
		return UnitDatas.IsValidIndex(AgentIndex) ? UnitDatas[AgentIndex].Needs : FFF_NeedState();
	}
	// --- 파티 조회 -------------------------------------------------------------------------

	// AgentIndex의 소속 파티 ID. 없으면 INDEX_NONE
	UFUNCTION(BlueprintPure, Category = "Crowd|Party")
	int32 GetAgentPartyID(int32 AgentIndex) const
	{
		return UnitDatas.IsValidIndex(AgentIndex) ? UnitDatas[AgentIndex].PartyID : INDEX_NONE;
	}

	UFUNCTION(BlueprintPure, Category = "Crowd|Party")
	bool IsPartyLeader(int32 AgentIndex) const
	{
		return UnitDatas.IsValidIndex(AgentIndex) && UnitDatas[AgentIndex].LeaderAgentIndex == AgentIndex;
	}

	// AgentIndex의 리더 Agent 인덱스. 리더 자신이면 자기 인덱스
	UFUNCTION(BlueprintPure, Category = "Crowd|Party")
	int32 GetPartyLeader(int32 AgentIndex) const
	{
		return UnitDatas.IsValidIndex(AgentIndex) ? UnitDatas[AgentIndex].LeaderAgentIndex : INDEX_NONE;
	}

	// 현재 활성 파티 수(= 활성 리더 수). 활성 Agent를 순회하므로 매 프레임 부를 용도는 아니다
	UFUNCTION(BlueprintPure, Category = "Crowd|Party")
	int32 GetActivePartyCount() const;

	// PartyID에 속한 활성 Agent 인덱스 목록. 파티 전체에 명령을 내릴 때 쓴다
	UFUNCTION(BlueprintPure, Category = "Crowd|Party")
	TArray<int32> GetPartyMembers(int32 PartyID) const;

	// --- 혼잡도 조회 -----------------------------------------------------------------------

	// 해당 위치의 밀도, 명/m^2
	UFUNCTION(BlueprintPure, Category = "Crowd|Metrics")
	float GetDensityAtLocation(FVector WorldLocation) const { return DensityGrid.GetDensityAtLocation(WorldLocation); }

	// 해당 위치의 Fruin LOS 등급
	UFUNCTION(BlueprintPure, Category = "Crowd|Metrics")
	ECrowdLOS GetLOSAtLocation(FVector WorldLocation) const { return DensityGrid.GetLOSAtLocation(WorldLocation); }

	// 현재 전체 셀 중 최대 밀도, 명/m^2
	UFUNCTION(BlueprintPure, Category = "Crowd|Metrics")
	float GetMaxDensity() const { return DensityGrid.GetMaxDensity(); }

	// LOS D 이상인 셀 개수 (혼잡 구역의 넓이)
	UFUNCTION(BlueprintPure, Category = "Crowd|Metrics")
	int32 GetCongestedCellCount() const { return DensityGrid.GetCongestedCellCount(); }

	const FCrowdDensityGrid& GetDensityGrid() const { return DensityGrid; }

	/**
	 * 결정·UI가 읽는 밀도 스냅샷. `SnapshotIntervalSteps` 고정 스텝마다 갱신된다.
	 *
	 * ### 폴링 금지 — 이 사본만 읽는다
	 * `GetDensityGrid()`를 매 틱 들여다보는 코드를 만들지 말 것. 그러면 소비자가
	 * Crowd 내부 상태에 매 프레임 묶이고, 나중에 힘 계산을 병렬화할 때 레이스가 된다.
	 * 스냅샷은 값 복사라 언제 읽어도 안전하고, `SnapshotFrame`이 로그에 남아
	 * "무엇을 보고 결정했는지"를 재현할 수 있다.
	 */
	const FFF_CrowdReadModel& GetCrowdReadModel() const { return CrowdReadModel; }

	// 발행된 스냅샷 수. 0에서 안 올라가면 발행 경로가 안 도는 것이다
	UFUNCTION(BlueprintPure, Category = "Crowd|Metrics")
	int32 GetSnapshotCount() const { return SnapshotCount; }

	/**
	 * 스냅샷 발행 주기, 고정 스텝 단위. 30 = 1초(30Hz).
	 *
	 * 유량(Throughput)의 집계 구간이기도 하다 — 값을 바꾸면 유량의 의미가 바뀐다.
	 * 짧게 하면 구간당 인원이 적어 잡음이 커지고, 길게 하면 순간 병목이 평균에 묻힌다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Metrics", meta = (ClampMin = "1"))
	int32 SnapshotIntervalSteps = 30;

	void DespawnParty(int32 PartyID);

	// --- 결정 -----------------------------------------------------------------------------

	// 조정자가 여기에 물어 목적지를 내려준다. 비어 있으면 Crowd가 배회로 폴백한다
	FFF_OnPartyNeedsDecision OnPartyNeedsDecision;

	/**
	 * 이 파티에 새 목적지가 필요하다고 표시한다. 실제 결정은 ProcessDecisions에서 일어난다.
	 *
	 * 스텝 루프 안에서 바로 결정하지 않는 이유: ConsumeFixedSteps 때문에 스텝 루프는 한 프레임에
	 * 최대 MaxStepsPerFrame회 돈다. 거기서 결정하면 같은 파티가 한 프레임에 3번 결정하고
	 * ChoiceRoundIndex가 3배로 뛴다.
	 *
	 * public인 이유: 도착 말고도 결정이 필요해지는 경우(대기열 거절, 시설 폐쇄)를
	 * 조정자가 외부에서 걸어야 한다.
	 *
	 * @param LeaderAgentIndex 파티 리더의 AgentIndex. 멤버 인덱스를 넣어도 ProcessDecisions가 걸러낸다
	 */
	void RequestDecision(int32 LeaderAgentIndex, EFF_DecisionReason Reason);

	// 결정 대기 중인 파티 수. 정상 상태에서 계속 늘어나면 예산이 모자란 것이다
	UFUNCTION(BlueprintPure, Category = "Crowd|Decision")
	int32 GetPendingDecisionCount() const { return DecisionQueue.Num(); }

	/**
	 * 한 프레임에 처리할 결정 개수 상한.
	 *
	 * 개장 직후나 공연 종료 직후에는 수백~수천 파티가 동시에 목적지를 필요로 한다. 상한이 없으면
	 * 그 프레임에 (파티 수 × 후보 생성 + 점수 계산)이 한꺼번에 돌아 프레임이 튄다.
	 * 처리하지 못한 파티는 다음 프레임 큐 맨 앞에 남으므로 굶지 않는다.
	 *
	 * 64는 500명(파티 약 200개) 스폰 시 4프레임 안에 전원이 출발하는 값이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Decision")
	int32 MaxDecisionsPerFrame = 64;

	/**
	 * 목적지를 못 받았을 때 배회할 반경, UU. 리더의 현재 위치 기준이다.
	 *
	 * SimRadiusUU(전역)를 쓰지 않는 이유가 둘이다.
	 *   1. 맵 반대편까지 직진하는 배회는 축제 방문객의 동선이 아니다
	 *   2. 도착해야 다시 결정하므로, 배회가 길면 인게임에서 새로 지은 시설을
	 *      수십 초 동안 발견하지 못한다
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Decision")
	float WanderRadiusUU = 2000.f;


private:
	void ProcessPendingDespawns();
	
	
public:
	// 실제 스폰되어 있는 Crowd Actor들의 풀
	UPROPERTY()
	TArray<AFF_BaseCrowd*> CrowdPool;

	// 풀에 채울 Actor 클래스. 지정하지 않으면 AFF_BaseCrowd를 그대로 쓴다(메시가 없어 보이지 않음).
	// 실제로는 SkeletalMesh와 AnimBP가 설정된 BP를 넘겨야 한다
	UPROPERTY()
	TSubclassOf<AFF_BaseCrowd> CrowdActorClass;

	// Agent 풀 중 현재 비어있는(미할당) 슬롯 인덱스 목록
	UPROPERTY()
	TArray<int32> FreeAgentSlotIndex;

	// Actor 풀의 최대 크기 (동시에 실체화 가능한 Crowd Actor 수)
	UPROPERTY()
	int32 ActorPoolSize = 500;

	// Agent 풀의 최대 크기 (Actor 바인딩 여부와 무관한 논리적 개체 수)
	UPROPERTY()
	int32 AgentPoolSize = 500;

	// Agent <-> Actor 슬롯 바인딩 장부. 실제 Actor 조작은 이 클래스가 하고, 장부만 여기 위임한다
	FCrowdActorPoolBinding PoolBinding;

	// --- 시뮬레이션 설정 -------------------------------------------------------------------

	// 시뮬레이션/밀도 그리드가 덮는 영역의 중심, 월드 좌표
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Sim")
	FVector SimOrigin = FVector::ZeroVector;

	// SimOrigin 기준 반경. 밀도 그리드는 이 반경을 감싸는 정사각형으로 만들어진다
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Sim")
	float SimRadiusUU = 10000.f;

	// 목적지에 이만큼 가까워지면 도착으로 보고 새 목적지를 뽑는다
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Sim")
	float ArrivalRadiusUU = 150.f;

	/**
	 * 목적지 앞에서 감속을 시작하는 거리, UU. 0이면 ArrivalRadiusUU * 3을 쓴다(기존 동작).
	 *
	 * ★ 회피와 정면으로 부딪치는 손잡이다.
	 * 기본값(150 * 3 = 450UU)은 파티 대형 간격 90UU를 통째로 덮는다. 즉 파티원 전원이
	 * 서로의 감속 구간 안에 들어가 목적지 앞에서 다 같이 느려지는데, 그 상태에서 회피가
	 * 밀어내면 감속과 밀어내기가 서로 싸워 목적지 앞에서 제자리 진동이 생긴다.
	 *
	 * 회피를 켠 뒤 목적지 근처에서 사람들이 떨면 계수를 만지기 전에 이 값을 먼저 줄일 것.
	 * 기본값을 0(=기존 동작)으로 둔 이유는 P0 기준선과의 회귀 비교를 깨지 않기 위해서다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Sim", meta = (ClampMin = "0.0"))
	float BrakingDistanceUU = 0.f;

	// --- 회피(사회력) ----------------------------------------------------------------------

	/**
	 * 회피를 켠다. ★ 기본값은 false다.
	 *
	 * 켜자마자 밀도·LOS·퇴장 타이밍이 전부 바뀐다. 그래서 A/B로 효과를 증명할 수 있어야 하고,
	 * 문제가 생기면 즉시 되돌릴 수 있어야 한다 — FF.Decision.Mode와 같은 성질의 손잡이다.
	 *
	 * off일 때 기존 수치와 다르면 이식 과정에서 다른 것을 건드린 것이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Avoidance")
	bool bEnableAvoidance = false;

	// 사회력 계수. 전부 SI(m, kg, N) 단위다 — 자세한 이유는 구조체 주석 참고.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Avoidance")
	FFF_CrowdAvoidanceConfig AvoidanceConfig;

	/**
	 * 시뮬 사각 경계(SimOrigin ± SimRadiusUU) 4벽을 벽 반발의 대상으로 넣는다.
	 *
	 * 지금 경계는 원형(SimRadiusUU)인데 ComputeWallForce는 선분을 받으므로 v0에서는
	 * 그 원을 감싸는 사각형 4변을 쓴다. 부스·펜스 벽은 NavMesh 단계의 몫이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Avoidance")
	bool bAvoidanceUseBoundaryWalls = true;

	// 마지막 스텝에서 회피가 실제로 돌았는지. off 회귀 검증과 콘솔 출력에 쓴다
	UFUNCTION(BlueprintPure, Category = "Crowd|Avoidance")
	bool WasAvoidanceAppliedLastStep() const { return bAvoidanceAppliedLastStep; }

	// 마지막 스텝의 이웃 수(자기 자신 제외). 회피가 꺼져 있으면 0이다
	UFUNCTION(BlueprintPure, Category = "Crowd|Avoidance")
	int32 GetAvoidanceNeighbourCount(int32 AgentIndex) const
	{
		return AvoidanceNeighbourCounts.IsValidIndex(AgentIndex) ? AvoidanceNeighbourCounts[AgentIndex] : 0;
	}

	/**
	 * 지금 위치로 격자를 한 번 지어 이웃 수를 세고, 실제로 격자에 들어간 개체 수를 반환한다.
	 * bEnableAvoidance와 무관하게 동작한다 — "격자가 정말 이웃을 찾는가"를
	 * 힘을 켜기 전에 확인하기 위한 진단용 경로다(FF.Crowd.DumpNeighbours).
	 */
	int32 DebugProbeAvoidanceNeighbours(TArray<int32>& OutCounts);

	// 출구의 도착 지점은 더 작은 범위에 들어올 수 있어야 한다.
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Sim")
	float ExitRadiusUU = 150.f;

	// 희망 속도까지 붙는 데 걸리는 시간, 초. 0에 가까울수록 즉시 최고 속도가 된다.
	// 사람은 순간적으로 1.5m/s가 되지 않으므로 이 값이 없으면 출발이 로봇처럼 보인다
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Sim")
	float VelocitySmoothTime = 0.35f;

	// 초당 회전 한계, 도. 이게 없으면 밀렸을 때 제자리에서 홱 돈다
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Sim")
	float MaxTurnRateDegrees = 360.f;

	/**
	 * 밀도 그리드 셀 한 변, UU. 200UU(2m) = 4㎡를 평균 창으로 쓴다.
	 *
	 * ★ 기본값을 숫자로 적지 않는다 — 지표 셀 크기의 단일 출처는 `FFF_CrowdMetrics`다.
	 * 여기에 200을 다시 적으면 ReadModel과 격자가 따로 움직일 수 있는 틈이 생기고,
	 * 그 순간 같은 밀도가 두 개의 면적으로 리포트된다(§FF_CrowdReadModel.h).
	 *
	 * 런타임에 바꿀 수는 있다. 바꾸면 ReadModel의 CellSizeMeters도 따라오지만,
	 * **그 시점 이전에 기록한 밀도 수치는 전부 폐기해야 한다.**
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Metrics")
	float DensityCellSizeUU = FFF_CrowdMetrics::AnalysisCellSizeUU;

	// --- 파티(동반 그룹) 설정 --------------------------------------------------------------

	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Party")
	bool bEnablePartyGrouping = true;

	// 파티원 간 대형 간격, UU. Radius(34UU)의 2배 이상이어야 서로 겹쳐 보이지 않는다
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Party")
	float PartySpacingUU = 90.f;

	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Party")
	float PartyCohesionRadiusUU = 400.f;

	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Party")
	float PartyMemberSpeedBoost = 1.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite,Category = "Crowd|Need", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InitialNeedMaxFraction = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Need", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InitialNeedModeFraction = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Need")
	FFF_NeedRiseRates NeedRiseRates;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Stay")
	float Temp_LiveStageMinutes = 240.f;
	UFUNCTION(BlueprintPure, Category = "Crowd|Stay")
	float GetPartyRemainingStayMinutes(int32 LeaderAgentIndex) const
	{
		return PartyRemainingStayMinutes.IsValidIndex(LeaderAgentIndex)
			? PartyRemainingStayMinutes[LeaderAgentIndex] : 0.f;
	}
	UFUNCTION(BlueprintPure, Category = "Crowd|Stay")
	int32 GetLeftPartyCount() const { return LeftPartyCount; }
	UFUNCTION(BlueprintPure, Category = "Crowd|Stay")
	int32 GetLeavingPartyCount() const;
	UFUNCTION(BlueprintPure, Category = "Crowd|Stay")
	int32 GetForcedLeaveCount() const { return ForcedLeaveCount; }
	UFUNCTION(BlueprintCallable, Category = "Crowd|Temp")
	void SetTemp_ExitWorldLocation(FVector NewExitWorldLocation);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Temp")
	float Temp_ExitArrivalRadiusUU = 60.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Temp")
	float Temp_LeaveTimeoutSeconds = 180.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Need")
	float Temp_FestivalMinutesPerRealSecond = 1.f;
	UFUNCTION(BlueprintPure, Category = "Crowd|Need")
	float GetLiveMinutes() const { return SimTimeSeconds * Temp_FestivalMinutesPerRealSecond; }

	// --- 디버그 ---------------------------------------------------------------------------

	// Agent의 목적지 선과 위치를 그린다. 에디터 확인용
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Debug")
	bool bDrawDebugAgents = false;

	// 밀도 그리드를 색 박스로 그린다. 셀 수만큼 드로우콜이 나가므로 시연용으로만
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Debug")
	bool bDrawDebugDensity = false;
	
	//해당 시뮬레이션 시간은 실제 진행한 시간(초단위)이 아니다.
	//시뮬레이션의 정합성을 위해 30Hz로 고정시키고, 최대 MaxStepsPerFrame을 초과할 경우 실시간보다 뒤쳐진다.
	UFUNCTION(BlueprintPure, Category = "Crowd|Sim")
	float GetSimTimeSeconds() const { return SimTimeSeconds; }
	
	UFUNCTION(BlueprintPure, Category = "Crowd|Sim")
	int32 GetGlobalSimulationSeed() const { return GlobalSimulationSeed; }

	
	
	
private:
	// 고정 스텝 한 번을 진행한다. DeltaTime이 아니라 항상 FixedStepSeconds를 받는다
	void StepSimulation(float FixedDt);

	// 바인딩된 Actor에 이번 프레임 표시 위치를 밀어 넣는다
	// @param InterpAlpha 직전 스텝과 현재 스텝 사이 보간 계수, 0~1
	void PushPresentationToActors(float DeltaTime, float InterpAlpha);

	/**
	 * 밀도 격자를 읽어 `CrowdReadModel`을 새로 만들고 유량 구간을 끊는다.
	 *
	 * 인원이 0인 칸은 넣지 않는다 — 실측 레벨 전역이면 대부분의 칸이 비어 있고,
	 * 200x200 격자면 4만 칸 중 수백 칸만 유효하다. 전부 담으면 스냅샷 하나가 수백 KB다.
	 *
	 * ★ 유량이 0이 아닌데 인원이 0인 칸도 담는다. "지나갔지만 지금은 없는" 칸이
	 * 통로다 — 그 칸을 버리면 유량 지도에서 동선이 끊겨 보인다.
	 */
	void PublishCrowdReadModel();

	// --- 회피 내부 구현 ---------------------------------------------------------------------

	/**
	 * UnitDatas(AoS, UU)를 미터 단위 미러 배열로 복사한다. ★ 단위 변환은 여기서만 일어난다.
	 *
	 * 미러를 '전체 풀 크기'로 유지하는 것이 요점이다. 그러면 격자 인덱스 == AgentIndex가 되어
     * 매핑 테이블이 필요 없다. 압축 배열로 만들면 인덱스 변환이 힘 계산 안쪽까지 들어가고,
	 * 거기서 한 번 어긋나면 엉뚱한 사람을 밀어낸다.
	 *
	 * 스텝당 O(N) 복사 하나는 5,000명에서 무시할 수 있다. Reset만 하고 재할당하지 않으므로
	 * 정상 상태에서 할당이 0이다.
	 */
	void UpdateAvoidanceMirrors();

	// 격자를 짓고 회피 가속도를 계산해 Unit.Force(UU/s^2)에 담는다. ★ 역변환은 여기서만.
	void ComputeAvoidanceForces();

	// 회피가 꺼진 스텝에서 낡은 Force를 0으로 되돌린다. 안 하면 끈 뒤에도 계속 밀린다
	void ClearAvoidanceForces();

	// 시뮬 사각 경계 4벽을 미터 단위로 다시 만든다. ConfigureSimArea에서 부른다
	void RebuildAvoidanceWalls();

	// SimOrigin/SimRadiusUU 안에서 목적지를 하나 뽑는다.
	// Rng를 소비하므로 스폰 경로에서만 쓴다 — 결정 경로는 PickWanderDestination을 쓴다
	FVector PickRandomDestination();

	// --- 결정 내부 구현 ---------------------------------------------------------------------

	// 큐에서 최대 MaxDecisionsPerFrame개를 꺼내 목적지를 확정한다. Tick에서만 부른다
	void ProcessDecisions();

	/**
	 * 결정 계층이 목적지를 주지 못했을 때의 폴백. 리더 위치 주변 WanderRadiusUU 안에서 뽑는다.
	 *
	 * ### Rng(FRandomStream)를 쓰지 않는다
	 * Rng는 스폰(파티 크기, 앵커 위치, 회전)과 같은 스트림이다. 결정이 그 스트림을 소비하면
	 *   1. 같은 시드인데 스폰 위치·파티 구성이 달라져 골든값 테스트가 깨진다
	 *   2. MaxDecisionsPerFrame을 바꾸면 시뮬레이션 결과가 바뀐다 — 성능 손잡이가 결과를
	 *      바꾸면 배치 A/B 비교가 무의미해진다
	 *   3. 프레임률이 결정 순서를 바꾸므로 재현성이 프레임률에 걸린다
	 *
	 * MakeDecisionSeed는 (시드, 개체, 회차)만의 함수이므로 언제 어떤 순서로 불려도 같은 값이다.
	 */
	FVector PickWanderDestination(int32 LeaderAgentIndex) const;

	// --- 파티 내부 구현 ---------------------------------------------------------------------

	FFF_VisitorDistribution BuildVisitorDistribution(int32 RequestedCount) const;
	FFF_NeedState MakeInitialNeeds(int32 StableAgentId) const;
	void AdvanceNeeds();

	FVector GetPartyFormationOffset(int32 SlotIndex) const;

	// 멤버의 목적지를 리더에게서 갱신한다. 리더가 유효하지 않으면 단독으로 승격시킨다
	bool UpdatePartyMemberTarget(int32 AgentIndex, FUnitData& Unit);

	void DrawDebugAgents() const;
	
	void DespawnAgentInternal(int32 AgentID);
	
public:

	
	
private:

	// 다음에 발급할 파티 ID. 슬롯 인덱스와 달리 재사용하지 않아 로그를 되짚을 수 있다
	int32 NextPartyID = 0;
	
	int32 NextStableAgentID = 0;

	static constexpr float FixedStepSeconds = 1.f / 30.f;

	static constexpr int32 MaxStepsPerFrame = 3;

	// 아직 소비하지 못한 프레임 시간, 초
	float SimAccumulator = 0.f;

	// 시뮬레이션 누적 시간, 초
	float SimTimeSeconds = 0.f;

	// 각 Agent의 실제 시뮬레이션 데이터(위치, 상태 등)를 저장하는 배열
	TArray<FUnitData> UnitDatas;

	// InitializeSimulation이 끝났는지. 중복 초기화로 Actor가 두 배 스폰되는 것을 막는다
	bool bSimulationInitialized = false;

	// 활성 Agent 수. UnitDatas를 매번 세지 않기 위해 증감으로 관리한다
	int32 ActiveAgentCount = 0;

	// 혼잡도 계측용 밀도 그리드
	FCrowdDensityGrid DensityGrid;

	// 결정·UI가 읽는 사본. 발행 주기 사이에는 값이 그대로 남아 있다
	FFF_CrowdReadModel CrowdReadModel;

	// 마지막 발행 이후 지난 고정 스텝 수. 프레임 수가 아니라 스텝 수다 —
	// 프레임률로 세면 유량 집계 구간이 기기 성능에 따라 달라진다
	int32 StepsSinceSnapshot = 0;

	int32 SnapshotCount = 0;

	// --- 회피 상태 -------------------------------------------------------------------------
	// 미러 배열. 크기는 항상 UnitDatas.Num()이고 인덱스는 AgentIndex와 같다(§UpdateAvoidanceMirrors)

	TArray<FVector2f> ScratchPos2DMeters;
	TArray<FVector2f> ScratchVel2DMeters;
	TArray<float> ScratchRadiusMeters;
	TArray<uint8> ScratchActive;

	// 활성 개체의 조밀 인덱스. 오름차순으로 채운다 — 힘 합산 순서가 결정성의 전제다
	TArray<int32> ScratchActiveIds;

	// 이번 스텝의 회피 가속도(m/s^2). UU 변환 전 값이다
	TArray<FVector2f> ScratchAccelMps2;

	// 진단용 이웃 수(자기 자신 제외)
	TArray<int32> AvoidanceNeighbourCounts;

	FFF_CrowdSpatialGrid AvoidanceGrid;

	// 이웃 버퍼를 청크별로 들고 있으므로 스텝 간 재사용을 위해 멤버로 둔다
	FFF_CrowdForceSolver ForceSolver;

	// 경계 4벽(m). ConfigureSimArea에서 갱신된다
	TArray<FFF_CrowdWallSegment> AvoidanceWalls;

	// 직전 스텝에 회피가 실제로 적용됐는지. 껐을 때 낡은 Force를 한 번만 지우기 위한 것이다
	bool bAvoidanceAppliedLastStep = false;

	// 목적지 추첨용. 시드를 고정해야 같은 조건에서 같은 결과가 재현된다
	FRandomStream Rng;
	
	int32 GlobalSimulationSeed = 0;
	
	UPROPERTY()
	TObjectPtr<UDataTable> CachedVariationTable;
	
	UPROPERTY()
	TObjectPtr<UDataTable> CachedGroupSizeTable;
	UPROPERTY()
	TObjectPtr<UDataTable> CachedChildCompositionTable;
	UPROPERTY()
	TObjectPtr<UDataTable> CachedArchetypeTable;
	FName CachedGroupScopeId;
	FName CachedArchetypeScenarioId;
	TArray<FFF_VisitHistory> PartyVisitHistories;
	TArray<FFF_NeedState> PartyAggregatedNeeds;
	TArray<int32> PartyMemberCounts;
	TArray<float> PartyRemainingStayMinutes;
	UPROPERTY()
	TObjectPtr<UDataTable> CachedCoefficientTable;
	float StayDrainMultiplierAtMaxNeed = 1.f;
	int32 LeftPartyCount = 0;
	int32 ForcedLeaveCount = 0;
	FVector Temp_ExitWorldLocation = FVector::ZeroVector;
	bool bTemp_ExitLocationSet = false;
	float LastNeedAdvanceSimTime = 0.f;
	float CachedCorrelationStrength = 0.3f;
	
	TMap<int32, int32> PartyLeaderByID;

	TSet<int32> PendingDespawnParty;

	/**
	 * 파티별 결정 상태. **리더 AgentIndex로 인덱싱한다** (PartyID가 아니다).
	 *
	 * PartyID는 단조 증가하며 회수되지 않으므로 그것으로 인덱싱하면 재입장이 반복될 때 배열이
	 * 무한히 자라고 free-list가 따로 필요해진다. 리더 인덱스는 크기가 AgentPoolSize로 고정이고,
	 * 슬롯 리셋 지점(SpawnAgent)이 이미 있어서 수명 관리가 공짜다.
	 *
	 * FUnitData와 분리한 것은 StepSimulation의 캐시 지역성 때문이다 — 배열 크기가 아니라
	 * "배열이 다르다"는 것 자체가 목적이다.
	 */
	TArray<FPartyDecisionState> PartyDecisionStates;

	// 결정을 기다리는 리더 AgentIndex, FIFO.
	// 앞에서 잘라내므로 예산에 걸려 밀린 파티가 다음 프레임 맨 앞이 된다 — 굶는 파티가 없다
	TArray<int32> DecisionQueue;
	

};
