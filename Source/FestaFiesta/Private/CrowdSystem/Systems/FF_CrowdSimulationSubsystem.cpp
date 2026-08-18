// Fill out your copyright notice in the Description page of Project Settings.


#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "NPC/IndividualVariation/FF_DeterministicRandom.h"
#include "CrowdSystem/FF_BaseCrowd.h"
#include "CrowdSystem/Systems/FF_CrowdSteering.h"
#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"
#include "DrawDebugHelpers.h"
#include "CrowdSystem/Types/FF_SimulationSettings.h"
#include "Engine/DataTable.h"
#include "NPC/Destination/FF_UtilityCoefficientHelpers.h"
#include "NPC/Party/FF_NeedRules.h"
namespace
{
	constexpr int32 NoSpawnGeneration = 0;
}


void UFF_CrowdSimulationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);


	// 풀 생성은 여기서 하지 않는다. 어떤 Actor 클래스를 쓸지 아직 모르고,
	// 군중이 없는 레벨에서까지 Actor 수백 개를 스폰할 이유도 없다.
	// 레벨의 AFF_SpawnActor가 BeginPlay에서 InitializeSimulation을 부른다.
}

void UFF_CrowdSimulationSubsystem::InitializeSimulation(FVector NewOrigin, float NewRadiusUU,
	int32 NewActorPoolSize, int32 NewAgentPoolSize, TSubclassOf<AFF_BaseCrowd> InCrowdActorClass)
{
	if (bSimulationInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] Already initialized, ignoring"));
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] World is inValid"));
		return;
	}
	const UFF_SimulationSettings* Settings = UFF_SimulationSettings::Get();
	
	CachedVariationTable = Settings->IndividualVariationTable.LoadSynchronous();
	CachedCorrelationStrength = Settings->VariationCorrelationStrength;
	
	if (!CachedVariationTable)
	{

		UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] IndividualVariationTable 미설정 — 개체 변주가 전부 1.0으로 동작한다"));
	}
	
	GlobalSimulationSeed = Settings->GlobalSimulationSeed;
	Rng.Initialize(GlobalSimulationSeed);
	CachedGroupSizeTable = Settings->GroupSizeTable.LoadSynchronous();
	CachedChildCompositionTable = Settings->ChildCompositionTable.LoadSynchronous();
	CachedArchetypeTable = Settings->ArchetypeTable.LoadSynchronous();
	CachedGroupScopeId = Settings->GroupScopeId;
	CachedArchetypeScenarioId = Settings->ArchetypeScenarioId;
	CachedCoefficientTable = Settings->UtilityCoefficientTable.LoadSynchronous();
	if (CachedCoefficientTable)
	{
		NeedRiseRates = FFF_NeedRules::LoadRiseRates(CachedCoefficientTable.Get());
		StayDrainMultiplierAtMaxNeed =
			FFF_NeedRules::LoadStayDrainMultiplierAtMaxNeed(CachedCoefficientTable.Get());
		FFF_NeedRules::LoadInitialNeedFractions(
			CachedCoefficientTable.Get(), InitialNeedModeFraction, InitialNeedMaxFraction);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CrowdSimulationSubsystem] UtilityCoefficientTable 미설정 — Need·체류 계수가 손잡이 기본값으로 동작한다"));
	}
	if (!CachedGroupSizeTable || !CachedArchetypeTable)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[CrowdSimulationSubsystem] GroupSizeTable/ArchetypeTable 미설정 — 방문객 분포를 만들 수 없다. ")
			TEXT("전원 1인 파티·ArchetypeID=NAME_None으로 스폰되고 preference 항이 꺼진다"));
	}
	else if (!CachedChildCompositionTable)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CrowdSimulationSubsystem] ChildCompositionTable 미설정 — 아동 0명으로 구성된다"));
	}
	
	ActorPoolSize = FMath::Max(NewActorPoolSize, 0);
	AgentPoolSize = FMath::Max(NewAgentPoolSize, 0);
	CrowdActorClass = InCrowdActorClass;

	ConfigureSimArea(NewOrigin, NewRadiusUU);

	InitializeCrowdActorPool(*World, NewOrigin);
	InitializeCrowdAgentPool(*World, NewOrigin);

	// 장부 크기는 실제로 스폰된 Actor 수에 맞춘다.
	// ActorPoolSize를 그대로 쓰면 스폰이 중간에 실패했을 때 없는 슬롯을 빈 슬롯으로 잘못 셈한다.
	PoolBinding.Initialise(CrowdPool.Num(), UnitDatas.Num());

	bSimulationInitialized = true;

	UE_LOG(LogTemp, Log, TEXT("[CrowdSimulationSubsystem] Initialized: %d actors, %d agents"),
		CrowdPool.Num(), UnitDatas.Num());
}

void UFF_CrowdSimulationSubsystem::ConfigureSimArea(FVector NewOrigin, float NewRadiusUU)
{
	SimOrigin = NewOrigin;
	SimRadiusUU = FMath::Max(NewRadiusUU, 100.f);

	// 밀도 그리드는 SimOrigin을 중심으로 반경을 감싸는 정사각형으로 만든다.
	// Build의 Origin은 중심이 아니라 좌하단이므로 반경만큼 빼서 넘긴다.
	const FVector GridOrigin = SimOrigin - FVector(SimRadiusUU, SimRadiusUU, 0.f);
	const FVector2D GridSize(SimRadiusUU * 2.f, SimRadiusUU * 2.f);
	DensityGrid.Build(GridOrigin, GridSize, DensityCellSizeUU);
	if (!bTemp_ExitLocationSet)
	{
		Temp_ExitWorldLocation = SimOrigin + FVector(-SimRadiusUU * 0.8f, 0.f, 0.f);
	}
}

TStatId UFF_CrowdSimulationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_CrowdSimulationSubsystem, STATGROUP_Tickables);
}

void UFF_CrowdSimulationSubsystem::Tick(float DeltaTime)
{
	//성능 확인을 위한 코드
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_Tick);

	Super::Tick(DeltaTime);
	
	
	//인구가 0일 지라도 시간 계산의 정합성을 위해서 시간 계산을 하고 밀도도 갱신한다.
	//인구 조건 조기 반환은 금지
	if (!bSimulationInitialized)
	{
		return;
	}
	
	// --- 고정 스텝 루프 --------------------------------------------------------------------
	// 프레임 시간을 모아 두었다가 FixedStepSeconds 단위로만 소비한다.
	// 남는 시간(SimAccumulator)은 다음 프레임으로 이월되고, 동시에 렌더링 보간 계수가 된다.
	const int32 StepsThisFrame = CrowdSteering::ConsumeFixedSteps(
		SimAccumulator, DeltaTime, FixedStepSeconds, MaxStepsPerFrame);

	for (int32 Step = 0; Step < StepsThisFrame; ++Step)
	{
		StepSimulation(FixedStepSeconds);
		SimTimeSeconds += FixedStepSeconds;
	}
	
	ProcessPendingDespawns();

	// --- 혼잡도 계측 -----------------------------------------------------------------------
	// 스텝이 한 번도 안 돌았어도(고프레임률) 갱신 비용은 셀 순회 한 번이라 그냥 매 프레임 돌린다.
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_Density);
		DensityGrid.Update(UnitDatas);
	}

	AdvanceNeeds();
	// --- 결정 ------------------------------------------------------------------------------
	// 퇴장 뒤여야 죽은 파티에 목적지를 주지 않고,
	// 밀도 갱신 뒤여야 결정 계층이 이번 프레임의 혼잡도를 보고 판단한다.
	// 스텝 루프 밖인 것도 의도다 — 한 프레임에 결정은 한 번이다.
	ProcessDecisions();

	// --- 표시 ------------------------------------------------------------------------------
	// 0이면 직전 스텝 위치, 1이면 현재 스텝 위치. 그 사이를 보간해서 30Hz 시뮬을 매끄럽게 보여준다.
	const float InterpAlpha = FMath::Clamp(SimAccumulator / FixedStepSeconds, 0.f, 1.f);
	PushPresentationToActors(DeltaTime, InterpAlpha);

	if (bDrawDebugDensity)
	{
		DensityGrid.DrawDebug(GetWorld());
	}
	if (bDrawDebugAgents)
	{
		DrawDebugAgents();
	}
}

void UFF_CrowdSimulationSubsystem::StepSimulation(float FixedDt)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_Step);

	// 지수 평활 계수. FixedDt가 고정이라 매번 같은 값이지만,
	// exp를 Agent마다 부르지 않기 위해 루프 밖에서 한 번만 구한다.
	const float VelocityAlpha = CrowdSteering::ComputeVelocityAlpha(VelocitySmoothTime, FixedDt);

	CrowdSteering::FStepParams StepParams;
	StepParams.ArrivalRadiusUU = ArrivalRadiusUU;
	StepParams.MaxTurnRateDegrees = MaxTurnRateDegrees;
	// 목적지 앞에서 감속을 시작하는 거리. 도착 반경의 3배면 한 번에 멈추지 않고 자연스럽게 줄어든다.
	StepParams.BrakingDistanceUU = FMath::Max(ArrivalRadiusUU * 3.f, 1.f);

	for (int32 AgentIndex = 0; AgentIndex < UnitDatas.Num(); ++AgentIndex)
	{
		FUnitData& Unit = UnitDatas[AgentIndex];
		if (!Unit.bIsActive)
		{
			continue;
		}

		// 결정 단위는 파티, 이동 단위는 Agent다.
		// 리더만 목적지를 정하고 멤버는 그것을 대형 오프셋만큼 옮겨 물려받는다(v0 리더 상속 모델).
		if (Unit.LeaderAgentIndex == AgentIndex)
		{
			const FPartyDecisionState& State = PartyDecisionStates[AgentIndex];
			if (State.bIsLeaving)
			{
				const bool bReachedExit = CrowdSteering::HasArrived(Unit, Temp_ExitArrivalRadiusUU);
				const bool bTimedOut = (Temp_LeaveTimeoutSeconds > 0.f)
					&& (SimTimeSeconds - State.LeaveStartedSimTime > Temp_LeaveTimeoutSeconds);
				if (bReachedExit || bTimedOut)
				{
					DespawnParty(Unit.PartyID);
					++LeftPartyCount;
					if (bTimedOut)
					{
						++ForcedLeaveCount;
					}
				}
			}
			// 도착을 감지하기만 한다. 결정은 ProcessDecisions에서 프레임당 1회 예산 안에서 한다 —
			// 이 루프는 한 프레임에 최대 MaxStepsPerFrame회 돌기 때문에 여기서 결정하면
			// 같은 파티가 한 프레임에 3번 결정하고 ChoiceRoundIndex가 3배로 뛴다.
			else if (CrowdSteering::HasArrived(Unit, ArrivalRadiusUU))
			{
				RequestDecision(AgentIndex, EFF_DecisionReason::Arrived);
			}
		}
		else
		{
			if (!UpdatePartyMemberTarget(AgentIndex, Unit))
			{
				continue;
			}
		}

		CrowdSteering::StepAgent(Unit, StepParams, VelocityAlpha, FixedDt);
	}
}

void UFF_CrowdSimulationSubsystem::PushPresentationToActors(float DeltaTime, float InterpAlpha)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_Presentation);

	for (int32 AgentIndex = 0; AgentIndex < UnitDatas.Num(); ++AgentIndex)
	{
		const FUnitData& Unit = UnitDatas[AgentIndex];
		if (!Unit.bIsActive)
		{
			continue;
		}

		// Actor가 바인딩되지 않은 Agent는 시뮬레이션만 되고 화면에는 나오지 않는다.
		// 이 분리가 이 시스템의 핵심이다 — Agent 수와 Actor 수가 독립적으로 정해진다.
		const int32 ActorSlot = PoolBinding.GetBoundSlot(AgentIndex);
		if (ActorSlot == INDEX_NONE || !CrowdPool.IsValidIndex(ActorSlot))
		{
			continue;
		}

		AFF_BaseCrowd* Actor = CrowdPool[ActorSlot];
		if (!IsValid(Actor))
		{
			continue;
		}

		const FVector RenderPosition = FMath::Lerp(Unit.PrevPosition, Unit.Position, InterpAlpha);
		Actor->PushPresentation(RenderPosition, Unit.Rotation, Unit.Velocity.Size());
	}
}

FVector UFF_CrowdSimulationSubsystem::PickRandomDestination()
{
	// 원 안에서 균일하게 뽑는다. 반지름을 그냥 균일 추첨하면 중심 쪽에 몰리므로 제곱근을 씌운다.
	const float Angle = Rng.FRandRange(0.f, 2.f * PI);
	const float Radius = SimRadiusUU * FMath::Sqrt(Rng.FRand());

	return SimOrigin + FVector(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), 0.f);
}

void UFF_CrowdSimulationSubsystem::RequestDecision(int32 LeaderAgentIndex, EFF_DecisionReason Reason)
{
	if (!PartyDecisionStates.IsValidIndex(LeaderAgentIndex))
	{
		return;
	}

	FPartyDecisionState& State = PartyDecisionStates[LeaderAgentIndex];

	if (State.bIsLeaving)
	{
		return;
	}
	if (Reason == EFF_DecisionReason::Arrived
		&& !State.CurrentDestinationId.IsNone()
		&& PartyVisitHistories.IsValidIndex(LeaderAgentIndex))
	{
		PartyVisitHistories[LeaderAgentIndex].RecordVisit(State.CurrentDestinationId, GetLiveMinutes());
		State.CurrentDestinationId = NAME_None;
	}
	State.bNeedsDecision = true;

	// 사유는 항상 최신 것으로 덮는다. 이미 대기 중인데 시설이 닫혔다면
	// 그 파티는 Arrived가 아니라 Interrupted로 처리돼야 한다.
	State.PendingReason = Reason;

	// bQueued 없이 그냥 Add하면 큐가 무한히 자란다 — 목적지를 기다리는 동안 도착 판정이 계속
	if (!State.bQueued)
	{
		State.bQueued = true;
		DecisionQueue.Add(LeaderAgentIndex);
	}
}

FVector UFF_CrowdSimulationSubsystem::PickWanderDestination(int32 LeaderAgentIndex) const
{
	const FUnitData& Unit = UnitDatas[LeaderAgentIndex];
	const FPartyDecisionState& State = PartyDecisionStates[LeaderAgentIndex];

	// 회차 하나에서 독립된 난수 둘이 필요하므로 회차를 2배로 접어 서로 다른 시드를 만든다.
	// 같은 회차에 같은 시드를 두 번 쓰면 각도와 반지름이 상관돼 대각선에만 목적지가 생긴다.
	const uint32 SeedAngle = FFF_DeterministicRandom::MakeDecisionSeed(
		GlobalSimulationSeed, Unit.StableAgentID, State.ChoiceRoundIndex * 2);
	const uint32 SeedRadius = FFF_DeterministicRandom::MakeDecisionSeed(
		GlobalSimulationSeed, Unit.StableAgentID, State.ChoiceRoundIndex * 2 + 1);

	const float Angle = FFF_DeterministicRandom::MakeUniformFraction(SeedAngle) * 2.f * PI;
	const float Radius = WanderRadiusUU * FMath::Sqrt(FFF_DeterministicRandom::MakeUniformFraction(SeedRadius));

	FVector Destination = Unit.Position + FVector(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), 0.f);

	// 시뮬 영역 밖으로 나가면 밀도 그리드가 그 사람을 세지 않는다. 경계 안으로 되접는다.
	const FVector FromOrigin(Destination.X - SimOrigin.X, Destination.Y - SimOrigin.Y, 0.f);
	if (FromOrigin.SizeSquared() > FMath::Square(SimRadiusUU))
	{
		Destination = SimOrigin + FromOrigin.GetSafeNormal() * SimRadiusUU;
		Destination.Z = Unit.Position.Z;
	}

	return Destination;
}

void UFF_CrowdSimulationSubsystem::ProcessDecisions()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_Decisions);

	if (DecisionQueue.IsEmpty())
	{
		return;
	}

	// 0이나 음수로 설정되면 아무도 목적지를 받지 못해 군중 전체가 제자리에 선다.
	// 손잡이 하나가 시뮬레이션을 정지시키는 상태를 만들지 않는다.
	const int32 Budget = FMath::Max(MaxDecisionsPerFrame, 1);

	
	//Head는 DecisionQueue에서 진행한 정도, Proceed는 실제로 Decision이 실행된 정도를 의미한다.
	int32 Head = 0;
	int32 Processed = 0;

	while (Head < DecisionQueue.Num() && Processed < Budget)
	{
		const int32 LeaderIndex = DecisionQueue[Head++];

		if (!UnitDatas.IsValidIndex(LeaderIndex))
		{
			continue;
		}

		FUnitData& Unit = UnitDatas[LeaderIndex];
		FPartyDecisionState& State = PartyDecisionStates[LeaderIndex];

		// 큐에서 뺐으므로 무조건 내린다. 아래에서 버려지는 경우에도 내려야
		// 다음 RequestDecision이 다시 큐에 넣을 수 있다.
		State.bQueued = false;

		// 큐에 들어간 뒤 퇴장했거나(퇴장은 지연 큐라 프레임 끝에 처리된다), 멤버로 덮인 인덱스는
		// 버린다. 퇴장 시점에 큐를 뒤져 지우지 않는 이유는 그게 O(큐 길이)이고,
		// 여기서 거르면 O(1)이기 때문이다.
		if (!Unit.bIsActive || Unit.LeaderAgentIndex != LeaderIndex || !State.bNeedsDecision)
		{
			continue;
		}

		// 결정 계층에 묻는다. 구독자가 없거나 후보를 못 찾으면 배회로 떨어진다 —
		// 인게임에 시설을 짓기 전에는 후보가 0개이므로 이게 정상 경로다.
		FFF_PartyDecisionContext Context;
		Context.PartyId = Unit.PartyID;
		Context.StableAgentId = Unit.StableAgentID;
		Context.ChoiceRoundIndex = State.ChoiceRoundIndex;
		Context.PartyWorldLocation = Unit.Position;
		Context.Reason = State.PendingReason;
		Context.ArchetypeId = Unit.ArchetypeID;
		Context.ConsecutiveNoChoiceCount = State.ConsecutiveNoChoiceCount;
		Context.NowLiveMinutes = GetLiveMinutes();
		Context.Needs = PartyAggregatedNeeds.IsValidIndex(LeaderIndex)
			? PartyAggregatedNeeds[LeaderIndex]
			: Unit.Needs;
		Context.PartySize = PartyMemberCounts.IsValidIndex(LeaderIndex)
			? FMath::Max(PartyMemberCounts[LeaderIndex], 1)
			: 1;
		if (PartyVisitHistories.IsValidIndex(LeaderIndex))
		{
			Context.VisitHistory = PartyVisitHistories[LeaderIndex];
		}
		Context.RemainingStayMinutes = GetPartyRemainingStayMinutes(LeaderIndex);
		FFF_PartyDecisionResult Result;

		const bool bDecided = OnPartyNeedsDecision.IsBound()
			&& OnPartyNeedsDecision.Execute(Context, Result);
		if (bDecided)
		{
			Unit.TargetLocation = Result.Destination;
		}
		else if (Result.FallbackAction == EFF_PartyFallbackAction::Leave)
		{
			State.bIsLeaving = true;
			State.LeaveStartedSimTime = SimTimeSeconds;
			Unit.TargetLocation = Temp_ExitWorldLocation;
			State.CurrentDestinationId = NAME_None;
			State.bNeedsDecision = false;
			State.LastDecisionSimTime = SimTimeSeconds;
			++Processed;
			continue;
		}
		else
		{
			Unit.TargetLocation = PickWanderDestination(LeaderIndex);
		}
		State.CurrentDestinationId = bDecided ? Result.DestinationId : NAME_None;

		State.ConsecutiveNoChoiceCount = bDecided
			? 0
			: (Result.bCountAsNoChoice ? State.ConsecutiveNoChoiceCount + 1 : State.ConsecutiveNoChoiceCount);

		// 이걸 빠뜨리면 같은 파티가 매 프레임 예산을 먹고 다른 파티가 굶는다.
		State.bNeedsDecision = false;
		State.LastDecisionSimTime = SimTimeSeconds;
		++State.ChoiceRoundIndex;
		++Processed;
	}

	// 처리한 만큼만 앞에서 잘라낸다. 남은 것이 다음 프레임 맨 앞이 되므로 FIFO가 유지된다.
	// 용량은 유지한다 — 매 프레임 재사용되는 컨테이너다.
	DecisionQueue.RemoveAt(0, Head, EAllowShrinking::No);
}

FVector UFF_CrowdSimulationSubsystem::GetPartyFormationOffset(int32 SlotIndex) const
{
	// 리더는 대형의 중심이다.
	if (SlotIndex <= 0)
	{
		return FVector::ZeroVector;
	}

	constexpr float GoldenAngleDegrees = 137.5f;
	const float Angle = FMath::DegreesToRadians(GoldenAngleDegrees * SlotIndex);
	const float Radius = PartySpacingUU * FMath::Sqrt(static_cast<float>(SlotIndex));

	return FVector(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), 0.f);
}

bool UFF_CrowdSimulationSubsystem::UpdatePartyMemberTarget(int32 AgentIndex, FUnitData& Unit)
{
	const int32 LeaderIndex = Unit.LeaderAgentIndex;

	// 리더가 사라졌을 경우, 해당 파티 또한 제거한다.
	if (!UnitDatas.IsValidIndex(LeaderIndex)
		|| !UnitDatas[LeaderIndex].bIsActive
		|| UnitDatas[LeaderIndex].PartyID != Unit.PartyID)
	{
		
		ensureMsgf(false, TEXT("고아 멤버 %d (파티 %d) — DespawnParty가 일부만 처리했다"),
		AgentIndex, Unit.PartyID);
		DespawnParty(Unit.PartyID);
		return false;
	}

	const FUnitData& Leader = UnitDatas[LeaderIndex];
	const FVector Offset = GetPartyFormationOffset(Unit.PartySlotIndex);

	// 너무 벌어졌으면 목적지가 아니라 리더 자신을 향한다. 이게 응집을 유지하는 유일한 장치다
	// — 목적지만 물려받으면 한 번 갈라진 파티는 다시 모이지 않는다.
	if (FVector::DistSquared2D(Unit.Position, Leader.Position) > FMath::Square(PartyCohesionRadiusUU))
	{
		Unit.TargetLocation = Leader.Position + Offset;
	}
	else
	{
		Unit.TargetLocation = Leader.TargetLocation + Offset;
	}
	
	return true;
}

int32 UFF_CrowdSimulationSubsystem::GetActivePartyCount() const
{
	int32 PartyCount = 0;
	for (int32 AgentIndex = 0; AgentIndex < UnitDatas.Num(); ++AgentIndex)
	{
		// 리더 한 명 = 파티 하나. 리더가 승격된 경우까지 포함해 실제 이동 단위 수를 센다.
		if (UnitDatas[AgentIndex].bIsActive && UnitDatas[AgentIndex].LeaderAgentIndex == AgentIndex)
		{
			++PartyCount;
		}
	}
	return PartyCount;
}

TArray<int32> UFF_CrowdSimulationSubsystem::GetPartyMembers(int32 PartyID) const
{
	TArray<int32> Members;
	if (PartyID == INDEX_NONE)
	{
		return Members;
	}

	for (int32 AgentIndex = 0; AgentIndex < UnitDatas.Num(); ++AgentIndex)
	{
		if (UnitDatas[AgentIndex].bIsActive && UnitDatas[AgentIndex].PartyID == PartyID)
		{
			Members.Add(AgentIndex);
		}
	}
	return Members;
}

void UFF_CrowdSimulationSubsystem::DespawnParty(int32 PartyID)
{
	
	if (PartyID == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] PartyID is InValid"));
		return;
	}
	
	PendingDespawnParty.Add(PartyID);
	
	
	
}

void UFF_CrowdSimulationSubsystem::ProcessPendingDespawns()
{
	if (PendingDespawnParty.IsEmpty())
	{
		return;
	}
	
	TSet<int32> DeleteParty = MoveTemp(PendingDespawnParty);
	PendingDespawnParty.Reset();

	for (int32 AgentIndex = 0;AgentIndex < UnitDatas.Num();++AgentIndex)
	{
		if (UnitDatas[AgentIndex].bIsActive && DeleteParty.Contains(UnitDatas[AgentIndex].PartyID))
		{
			DespawnAgentInternal(AgentIndex);
		}
	}
	
	
}

void UFF_CrowdSimulationSubsystem::SetAgentDestination(int32 AgentIndex, FVector Destination)
{
	if (!UnitDatas.IsValidIndex(AgentIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] SetAgentDestination: Invalid AgentIndex %d"), AgentIndex);
		return;
	}

	// 파티 멤버에게 걸면 다음 스텝에 리더 목적지로 덮인다. 파티 전체를 옮기려면 리더에게 걸어야 한다.
	if (UnitDatas[AgentIndex].LeaderAgentIndex != AgentIndex)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CrowdSimulationSubsystem] SetAgentDestination: Agent %d는 파티 멤버다 (리더 %d). 다음 스텝에 덮인다"),
			AgentIndex, UnitDatas[AgentIndex].LeaderAgentIndex);
	}

	UnitDatas[AgentIndex].TargetLocation = Destination;
}

void UFF_CrowdSimulationSubsystem::BindAgentToPool(int32 AgentIndex)
{
	if (!UnitDatas.IsValidIndex(AgentIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] Invalid AgentIndex"));
		return;
	}

	// 장부가 슬롯을 골라 준다. 이미 바인딩됐거나 풀이 고갈됐으면 INDEX_NONE.
	// 풀 고갈은 오류가 아니다 — Agent는 계속 시뮬레이션되고 밀도에도 반영되므로 분석 결과는 그대로다.
	const int32 ActorSlot = PoolBinding.Bind(AgentIndex);
	if (ActorSlot == INDEX_NONE || !CrowdPool.IsValidIndex(ActorSlot))
	{
		return;
	}

	AFF_BaseCrowd* Actor = CrowdPool[ActorSlot];
	if (!IsValid(Actor))
	{
		// Actor가 사라졌다면 장부를 되돌린다. 안 그러면 슬롯이 영영 회수되지 않는다.
		PoolBinding.Release(AgentIndex);
		return;
	}

	Actor->BindToAgent(AgentIndex, UnitDatas[AgentIndex].Position, UnitDatas[AgentIndex].Rotation);
	Actor->SetActorHiddenInGame(false);
	Actor->SetActorEnableCollision(true);
}

void UFF_CrowdSimulationSubsystem::ReleaseAgentToPool(int32 AgentIndex)
{
	if (!UnitDatas.IsValidIndex(AgentIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] Invalid AgentIndex"));
		return;
	}

	const int32 ActorSlot = PoolBinding.Release(AgentIndex);
	if (ActorSlot == INDEX_NONE)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[CrowdSimulationSubsystem] Agent %d is already released"), AgentIndex);
		return;
	}

	if (CrowdPool.IsValidIndex(ActorSlot) && IsValid(CrowdPool[ActorSlot]))
	{
		AFF_BaseCrowd* Actor = CrowdPool[ActorSlot];
		Actor->ReleaseFromAgent();
		Actor->SetActorHiddenInGame(true);
		Actor->SetActorEnableCollision(false);
	}
}


void UFF_CrowdSimulationSubsystem::SetTemp_ExitWorldLocation(FVector NewExitWorldLocation)
{
	Temp_ExitWorldLocation = NewExitWorldLocation;
	bTemp_ExitLocationSet = true;
	if (NewExitWorldLocation.Equals(SimOrigin, 1.f))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CrowdSimulationSubsystem] 퇴장 지점이 스폰 원점과 같다 — 퇴장 동선이 생기지 않는다. ")
			TEXT("AFF_SpawnActor의 Temp_ExitOffsetFromOrigin을 확인할 것"));
	}
}
int32 UFF_CrowdSimulationSubsystem::GetLeavingPartyCount() const
{
	int32 Count = 0;
	for (int32 AgentIndex = 0; AgentIndex < UnitDatas.Num(); ++AgentIndex)
	{
		const FUnitData& Unit = UnitDatas[AgentIndex];
		if (Unit.bIsActive && Unit.LeaderAgentIndex == AgentIndex
			&& PartyDecisionStates.IsValidIndex(AgentIndex) && PartyDecisionStates[AgentIndex].bIsLeaving)
		{
			++Count;
		}
	}
	return Count;
}
FFF_VisitorDistribution UFF_CrowdSimulationSubsystem::BuildVisitorDistribution(int32 RequestedCount) const
{
	return FFF_VisitorDistributionBuilder::Build(
		CachedGroupSizeTable.Get(),
		CachedChildCompositionTable.Get(),
		CachedArchetypeTable.Get(),
		CachedGroupScopeId,
		CachedArchetypeScenarioId,
		RequestedCount,
		GlobalSimulationSeed);
}
FFF_NeedState UFF_CrowdSimulationSubsystem::MakeInitialNeeds(int32 StableAgentId) const
{
	return FFF_NeedRules::MakeInitialNeeds(
		GlobalSimulationSeed,
		StableAgentId,
		NoSpawnGeneration,
		NeedRiseRates,
		InitialNeedModeFraction,
		InitialNeedMaxFraction);
}
void UFF_CrowdSimulationSubsystem::AdvanceNeeds()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_Needs);
	const float ElapsedMinutes =
		(SimTimeSeconds - LastNeedAdvanceSimTime) * Temp_FestivalMinutesPerRealSecond;
	LastNeedAdvanceSimTime = SimTimeSeconds;
	for (FFF_NeedState& Aggregated : PartyAggregatedNeeds)
	{
		Aggregated = FFF_NeedState();
	}
	for (int32& MemberCount : PartyMemberCounts)
	{
		MemberCount = 0;
	}
	for (int32 AgentIndex = 0; AgentIndex < UnitDatas.Num(); ++AgentIndex)
	{
		FUnitData& Unit = UnitDatas[AgentIndex];
		if (!Unit.bIsActive)
		{
			continue;
		}
		Unit.Needs = FFF_NeedRules::Advance(Unit.Needs, ElapsedMinutes, NeedRiseRates);
		const int32 LeaderIndex = Unit.LeaderAgentIndex;
		if (!PartyAggregatedNeeds.IsValidIndex(LeaderIndex))
		{
			continue;
		}
		PartyAggregatedNeeds[LeaderIndex] =
			FFF_NeedState::MakeAggregated(PartyAggregatedNeeds[LeaderIndex], Unit.Needs);
		++PartyMemberCounts[LeaderIndex];
	}
	if (ElapsedMinutes > 0.f)
	{
		for (int32 LeaderIndex = 0; LeaderIndex < PartyRemainingStayMinutes.Num(); ++LeaderIndex)
		{
			if (PartyMemberCounts[LeaderIndex] <= 0)
			{
				continue;
			}
			const FFF_StayConsumption Consumption = FFF_NeedRules::ConsumeStay(
				PartyAggregatedNeeds[LeaderIndex], ElapsedMinutes, StayDrainMultiplierAtMaxNeed);
			PartyRemainingStayMinutes[LeaderIndex] = FMath::Max(
				PartyRemainingStayMinutes[LeaderIndex] - Consumption.ConsumedStayBudgetMinutes, 0.f);
		}
	}
}
int32 UFF_CrowdSimulationSubsystem::SpawnAgent(UWorld& InWorld, FVector SpawnLocation, FRotator SpawnRotation)
{
	if (FreeAgentSlotIndex.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] No free agent slot available"));
		return INDEX_NONE;
	}

	int32 index = FreeAgentSlotIndex.Pop();

	FUnitData& Unit = UnitDatas[index];
	
	Unit = FUnitData();
	
	Unit.Position = SpawnLocation;
	Unit.PrevPosition = SpawnLocation;
	Unit.Rotation = SpawnRotation;
	Unit.Velocity = FVector::ZeroVector;
	Unit.Force = FVector::ZeroVector;
	Unit.StableAgentID = NextStableAgentID++;
	Unit.Needs = MakeInitialNeeds(Unit.StableAgentID);

	// 결정을 받기 전까지의 목적지는 제자리이다.
	Unit.TargetLocation = SpawnLocation;

	// 기본값은 1인 파티(자기 자신이 리더)다. 파티로 묶는 것은 SpawnAgents가 이 뒤에 덮어쓴다.다.
	Unit.PartyID = NextPartyID++;
	Unit.LeaderAgentIndex = index;
	Unit.PartySlotIndex = 0;
	

	// 슬롯을 실제로 쓰기 시작하는 지점. 여기서 켜야 풀에 놀고 있는 슬롯이 시뮬레이션에 끌려들어가지 않는다.
	Unit.bIsActive = true;
	++ActiveAgentCount;

	// 슬롯은 재사용되므로 결정 상태도 통째로 되돌린다. 안 하면 재입장한 방문객이
	// 퇴장자의 결정 회차와 대기 플래그를 물려받는다.
	if (PartyDecisionStates.IsValidIndex(index))
	{
		PartyDecisionStates[index] = FPartyDecisionState();
	}

	if (PartyVisitHistories.IsValidIndex(index))
	{
		PartyVisitHistories[index].Reset();
	}
	// 스폰 직후에는 아직 목적지가 없다. 파티로 묶이는 멤버의 요청은
	// ProcessDecisions가 "리더가 아니다"로 걸러낸다.
	RequestDecision(index, EFF_DecisionReason::Spawned);

	// 화면에 보이도록 Actor를 붙인다. 풀이 고갈되면 시뮬레이션만 된다.
	BindAgentToPool(index);

	// 스폰 방향에서도 인원 보존을 본다. 퇴장 쪽만 검사하면 FreeAgentSlotIndex.Pop과
	// ++ActiveAgentCount 사이에 새 return 경로가 끼어드는 순간 조용히 어긋난다.
	ensureMsgf(ActiveAgentCount + FreeAgentSlotIndex.Num() == UnitDatas.Num(),
		TEXT("인원 보존 위반(스폰): active %d + free %d != pool %d"),
		ActiveAgentCount, FreeAgentSlotIndex.Num(), UnitDatas.Num());

	return index;
}

int32 UFF_CrowdSimulationSubsystem::SpawnAgents(int32 Count, FVector SpawnCenter, float SpawnRadiusUU)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return 0;
	}

	const FFF_VisitorDistribution Distribution = BuildVisitorDistribution(Count);
	if (Distribution.Visitors.Num() == 0)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[CrowdSimulationSubsystem] 방문객 분포가 비었다 — 입력 테이블이나 ScopeId('%s')를 확인할 것. 스폰하지 않는다"),
			*CachedGroupScopeId.ToString());
		return 0;
	}
	if (Distribution.ActualVisitorCount < Distribution.RequestedVisitorCount)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[CrowdSimulationSubsystem] 요청 %d명 중 %d명 — 그룹을 통째로만 만들어 %d명이 남았다"),
			Distribution.RequestedVisitorCount, Distribution.ActualVisitorCount,
			Distribution.RequestedVisitorCount - Distribution.ActualVisitorCount);
	}
	int32 SpawnedCount = 0;
	int32 PartiesFormed = 0;
	int32 ChildrenSpawned = 0;
	bool bAgentPoolExhausted = false;
	const float BaseSpeed = FUnitData().Speed;
	int32 VisitorCursor = 0;
	while (VisitorCursor < Distribution.Visitors.Num() && !bAgentPoolExhausted)
	{
		const FFF_TaggedVisitor& LeadVisitor = Distribution.Visitors[VisitorCursor];
		const int32 PlannedSize = bEnablePartyGrouping ? FMath::Max(LeadVisitor.TargetGroupSize, 1) : 1;
		const int32 PartySize = FMath::Min(PlannedSize, Distribution.Visitors.Num() - VisitorCursor);
		checkf(!LeadVisitor.bIsDependentChildSlot || !bEnablePartyGrouping,
			TEXT("SpawnAgents: 그룹 선두가 아동 슬롯이다 — 빌더가 성인을 먼저 배출한다는 전제가 깨졌다. ")
			TEXT("아동이 리더가 되면 목적지를 상속할 대상이 없다"));
		// PickRandomDestination과 같은 방식 — 제곱근을 씌워 원 안에 고르게 흩뿌린다.
		const float Angle = Rng.FRandRange(0.f, 2.f * PI);
		const float Radius = SpawnRadiusUU * FMath::Sqrt(Rng.FRand());
		const FVector Anchor = SpawnCenter + FVector(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), 0.f);

		// 같이 온 사람들은 같은 방향을 보고 있다. 파티당 한 번만 뽑는다.
		const FRotator PartyRotation(0.f, Rng.FRandRange(0.f, 360.f), 0.f);

		int32 LeaderIndex = INDEX_NONE;
		int32 PartyID = INDEX_NONE;

		for (int32 Slot = 0; Slot < PartySize; ++Slot)
		{
			const FFF_TaggedVisitor& Visitor = Distribution.Visitors[VisitorCursor + Slot];
			checkf(!bEnablePartyGrouping || Visitor.TargetGroupSize == LeadVisitor.TargetGroupSize,
				TEXT("SpawnAgents: 그룹 %d번째 멤버의 TargetGroupSize(%d)가 선두(%d)와 다르다 — ")
				TEXT("빌더의 배출 순서가 바뀌었다. 태그 기준 재조합으로 바꿀 것"),
				Slot, Visitor.TargetGroupSize, LeadVisitor.TargetGroupSize);
			const FVector SpawnLocation = Anchor + GetPartyFormationOffset(Slot);

			const int32 AgentIndex = SpawnAgent(*World, SpawnLocation, PartyRotation);
			if (AgentIndex == INDEX_NONE)
			{
				// Agent 풀이 고갈됐다. 더 시도해도 같은 결과다.
				bAgentPoolExhausted = true;
				break;
			}

			FUnitData& Unit = UnitDatas[AgentIndex];
			Unit.ArchetypeID = Visitor.ArchetypeId;
			Unit.bIsDependentChild = Visitor.bIsDependentChildSlot;
			if (Unit.bIsDependentChild)
			{
				++ChildrenSpawned;
			}
			if (Slot == 0)
			{
				// 리더. SpawnAgent가 이미 자기 자신을 리더로, 새 PartyID를 부여해 뒀다.
				LeaderIndex = AgentIndex;
				PartyID = Unit.PartyID;
				PartyLeaderByID.Add(PartyID, LeaderIndex);
			}
			else
			{
				Unit.PartyID = PartyID;
				Unit.LeaderAgentIndex = LeaderIndex;
				Unit.PartySlotIndex = Slot;
				// 첫 스텝에서 어차피 갱신되지만, 그 한 프레임 동안 엉뚱한 방향으로 몸이 돌아가는 것을 막는다.
				Unit.TargetLocation = UnitDatas[LeaderIndex].TargetLocation + GetPartyFormationOffset(Slot);
			}
			// 개체 변주는 GroupId로 PartyID를 쓴다. 멤버의 PartyID는 위 else 블록에서 확정되므로
			// 이 호출을 if/else 앞으로 옮기면 멤버마다 다른 GroupId가 들어가 그룹 공유가 조용히 깨진다
			// (같은 파티가 서로 다른 속도로 걸어 계속 벌어지고, PartyCohesionRadiusUU 문제로 오진하게 된다).
			checkf(Slot == 0 || Unit.PartyID == PartyID,
				TEXT("SpawnAgents: 멤버(Slot %d)의 PartyID가 확정되기 전에 개체 변주를 계산했다. "
					 "ResolveWithGroupSharing 호출을 if/else 블록 뒤로 옮길 것"), Slot);

			// 안 채워지면 MakeDecisionSeed(seed, -1, round)로 전원이 같은 시드를 받아
			// 한 라운드에 모두 같은 곳으로 간다. 조용히 잘못되는 종류이므로 여기서 막는다.
			checkf(Unit.StableAgentID != INDEX_NONE,
				TEXT("SpawnAgents: Agent %d의 StableAgentID가 발급되지 않았다 (SpawnAgent 확인)"), AgentIndex);

			checkf(Slot != 0 || Unit.LeaderAgentIndex == AgentIndex,
				TEXT("SpawnAgents: 리더 %d가 자기 자신을 리더로 갖고 있지 않다"), AgentIndex);

			const FFF_ResolvedIndividualTraits Traits =
				FFF_IndividualVariationResolver::ResolveWithGroupSharing(
					CachedVariationTable.Get(),
					GetGlobalSimulationSeed(),
					Unit.StableAgentID,
					Unit.PartyID,
					0,
					CachedCorrelationStrength);
			const float MemberBoost = (Slot == 0) ? 1.f : FMath::Max(PartyMemberSpeedBoost,1.f);
			Unit.Speed = BaseSpeed * Traits.WalkSpeedMultiplier* MemberBoost;
			Unit.MaxSpeed = Unit.Speed;

			if (Slot == 0 && PartyRemainingStayMinutes.IsValidIndex(LeaderIndex))
			{
				const float StayShare = FFF_UtilityCoefficientHelpers::GetCoefficient(
					CachedCoefficientTable.Get(), TEXT("stay_duration_share_of_live_stage_mean"), 0.5f);
				PartyRemainingStayMinutes[LeaderIndex] =
					FMath::Max(Temp_LiveStageMinutes * StayShare * Traits.StayDurationMultiplier, 0.f);
			}

			++SpawnedCount;
		}

		if (LeaderIndex != INDEX_NONE)
		{
			++PartiesFormed;

			// 멤버들도 SpawnAgent에서 각자 ID를 하나씩 소비했다. 그만큼 되돌려 ID가 건너뛰지 않게 한다.
			NextPartyID = PartyID + 1;
		}
		VisitorCursor += PartySize;
	}

	UE_LOG(LogTemp, Log,
		TEXT("[CrowdSimulationSubsystem] Spawned %d/%d agents in %d parties (아동 %d명 %.1f%%, active: %d)"),
		SpawnedCount, Count, PartiesFormed, ChildrenSpawned,
		SpawnedCount > 0 ? 100.f * ChildrenSpawned / SpawnedCount : 0.f, ActiveAgentCount);

	return SpawnedCount;
}

void UFF_CrowdSimulationSubsystem::InitializeCrowdActorPool(UWorld& InWorld, FVector PoolLocation)
{
	if (!IsValid(&InWorld))
	{
		UE_LOG(LogTemp,Warning, TEXT("[CrowdSimulationSubsystem] World is inValid"))
		return;
	}

	// 지정이 없으면 베이스 클래스로 떨어진다. 메시가 없어 화면에는 아무것도 안 보이므로
	// 시연할 때는 반드시 SkeletalMesh + AnimBP가 설정된 BP를 넘겨야 한다.
	UClass* SpawnClass = CrowdActorClass ? CrowdActorClass.Get() : AFF_BaseCrowd::StaticClass();
	if (!CrowdActorClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CrowdSimulationSubsystem] CrowdActorClass is not set - spawning bare AFF_BaseCrowd (no mesh, nothing will be visible)"));
	}

	CrowdPool.Reserve(ActorPoolSize);

	// APawn의 기본값은 AdjustIfPossibleButDontSpawnIfColliding이다.
	// 풀 액터는 전부 같은 자리에 겹쳐 만들기 때문에 그대로 두면 두 번째부터 스폰이 실패할 수 있다.
	// 어차피 만들자마자 숨기고 충돌을 끄는 비활성 객체이므로 항상 스폰시킨다.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 i=0;i<ActorPoolSize;i++)
	{
		AFF_BaseCrowd* NewCrowd = InWorld.SpawnActor<AFF_BaseCrowd>(SpawnClass,PoolLocation,FRotator::ZeroRotator,SpawnParams);
		if (!IsValid(NewCrowd))
		{
			// 스폰 실패. 여기서 그냥 Add하면 CrowdPool에 null이 섞이고 인덱스가 어긋난다.
			// 장부는 이 루프가 끝난 뒤 CrowdPool.Num() 기준으로 만들어지므로 여기서 멈추면 된다.
			UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] Failed to spawn crowd actor %d, pool size is %d"), i, i);
			break;
		}

		NewCrowd->SetActorHiddenInGame(true);
		NewCrowd->SetActorEnableCollision(false);
		CrowdPool.Add(NewCrowd);
	}
}

void UFF_CrowdSimulationSubsystem::InitializeCrowdAgentPool(UWorld& InWorld, FVector AgentPoolLocation)
{
	if (!IsValid(&InWorld))
	{
		UE_LOG(LogTemp,Warning, TEXT("[CrowdSimulationSubsystem] World is inValid"))
		return;
	}

	UnitDatas.Reserve(AgentPoolSize);
	FreeAgentSlotIndex.Reset(AgentPoolSize);

	for (int32 i=0;i<AgentPoolSize;i++)
	{
		FUnitData NewUnit;
		NewUnit.Position = AgentPoolLocation;
		NewUnit.PrevPosition = AgentPoolLocation;

		UnitDatas.Add(NewUnit);

		//todo...Group 결정과 Archtype에 의한 스폰 비율 결정을 추가할 것.
	}

	// 뒤에서 꺼내 쓰므로(Pop) 역순으로 넣어 0번 Agent부터 나가게 한다. 로그를 읽기 쉬워진다.
	for (int32 i = AgentPoolSize - 1; i >= 0; --i)
	{
		FreeAgentSlotIndex.Add(i);
	}

	// 리더 AgentIndex로 인덱싱하므로 Agent 풀과 크기가 같다.
	// UnitDatas.Num()이 아니라 AgentPoolSize를 쓰면 스폰이 중간에 실패했을 때 어긋난다.
	PartyDecisionStates.Reset();
	PartyDecisionStates.SetNum(UnitDatas.Num());
	PartyVisitHistories.Reset();
	PartyVisitHistories.SetNum(UnitDatas.Num());
	PartyAggregatedNeeds.Reset();
	PartyAggregatedNeeds.SetNum(UnitDatas.Num());
	PartyMemberCounts.Reset();
	PartyMemberCounts.SetNumZeroed(UnitDatas.Num());
	PartyRemainingStayMinutes.Reset();
	PartyRemainingStayMinutes.SetNumZeroed(UnitDatas.Num());
	DecisionQueue.Reset();

	ActiveAgentCount = 0;
}

void UFF_CrowdSimulationSubsystem::DrawDebugAgents() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 파티 색 팔레트. PartyID로 나눠 쓰므로 인접한 두 파티가 같은 색일 수는 있지만,
	// 한 파티가 여러 색으로 보이는 일은 없다 — 묶임 여부를 눈으로 확인하는 데는 이걸로 충분하다.
	static const FColor PartyColors[] = {
		FColor(255, 96, 96), FColor(96, 255, 96), FColor(96, 160, 255), FColor(255, 220, 80),
		FColor(220, 110, 255), FColor(80, 240, 240), FColor(255, 150, 60), FColor(180, 255, 140)
	};
	constexpr int32 NumPartyColors = UE_ARRAY_COUNT(PartyColors);

	for (int32 AgentIndex = 0; AgentIndex < UnitDatas.Num(); ++AgentIndex)
	{
		const FUnitData& Unit = UnitDatas[AgentIndex];
		if (!Unit.bIsActive)
		{
			continue;
		}

		const bool bIsLeader = (Unit.LeaderAgentIndex == AgentIndex);
		const FColor PartyColor = (Unit.PartyID >= 0)
			? PartyColors[Unit.PartyID % NumPartyColors]
			: FColor::Cyan;

		// 위치는 반경 원으로, 진행 방향은 짧은 선으로, 목적지는 옅은 선으로 잇는다.
		// 원 색은 파티 색이고, 리더는 선을 굵게 그려 구분한다.
		DrawDebugCircle(World, Unit.Position + FVector(0, 0, 5.f), Unit.Radius, 12, PartyColor,
			false, -1.f, 0, bIsLeader ? 3.f : 1.f, FVector(1, 0, 0), FVector(0, 1, 0), false);

		DrawDebugLine(World, Unit.Position, Unit.Position + Unit.Velocity * 0.5f, FColor::Yellow, false, -1.f, 0, 2.f);

		if (bIsLeader)
		{
			// 리더만 목적지 선을 그린다. 멤버 것은 리더 것과 겹쳐서 화면만 지저분해진다.
			DrawDebugLine(World, Unit.Position, Unit.TargetLocation, FColor(60, 60, 60), false, -1.f, 0, 0.5f);
		}
		else if (UnitDatas.IsValidIndex(Unit.LeaderAgentIndex))
		{
			// 멤버는 리더와 선으로 잇는다. 파티가 실제로 붙어 다니는지 이 선 길이로 바로 보인다.
			DrawDebugLine(World, Unit.Position + FVector(0, 0, 5.f),
				UnitDatas[Unit.LeaderAgentIndex].Position + FVector(0, 0, 5.f), PartyColor, false, -1.f, 0, 1.5f);
		}
	}
}

void UFF_CrowdSimulationSubsystem::DespawnAgentInternal(int32 AgentID)
{

	if (!UnitDatas.IsValidIndex(AgentID))
	{
		return;
	}
	
	FUnitData& Unit = UnitDatas[AgentID];
	
	if (!Unit.bIsActive) return;
	
	ReleaseAgentToPool(AgentID);
	
	//리더일 경우, PartyLeaderByID를 제거한다.
	if (PartyLeaderByID.FindRef(Unit.PartyID) == AgentID)
	{
		PartyLeaderByID.Remove(Unit.PartyID);
	}
	
	Unit.bIsActive = false;
	--ActiveAgentCount;
	
	FreeAgentSlotIndex.Add(AgentID);
	Unit.PartyID = INDEX_NONE;
	Unit.LeaderAgentIndex = INDEX_NONE;

	// 결정 상태도 되돌린다. DecisionQueue에는 이 인덱스가 남아 있을 수 있는데,
	// ProcessDecisions가 bIsActive로 걸러내므로 큐를 뒤져 지우지 않는다(그건 O(큐 길이)다).
	//
	// 그래서 bQueued는 "큐에 있다"의 정확한 표현이 아니라 무한 증가를 막는 힌트다 —
	// 퇴장 직후 재스폰되면 같은 인덱스가 큐에 두 번 들어갈 수 있다. 두 번째는
	// bNeedsDecision == false로 걸러지고 예산도 소비하지 않으므로 파티당 최대 하나로 묶인다.
	if (PartyDecisionStates.IsValidIndex(AgentID))
	{
		PartyDecisionStates[AgentID] = FPartyDecisionState();
	}
	
	ensureMsgf(ActiveAgentCount + FreeAgentSlotIndex.Num() == UnitDatas.Num(),
	   TEXT("인원 보존 위반: active %d + free %d != pool %d"),
	   ActiveAgentCount, FreeAgentSlotIndex.Num(), UnitDatas.Num());
	
	
	
	
	
	
}
