// Fill out your copyright notice in the Description page of Project Settings.



#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"

#include "CrowdSystem/FF_BaseCrowd.h"
#include "CrowdSystem/Systems/FF_CrowdSteering.h"
#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"
#include "DrawDebugHelpers.h"

namespace CrowdSim
{
	static constexpr int32 DefaultSeed = 20260812;
}

void UFF_CrowdSimulationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	Rng.Initialize(CrowdSim::DefaultSeed);

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

	if (ActiveAgentCount <= 0)
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

	// --- 혼잡도 계측 -----------------------------------------------------------------------
	// 스텝이 한 번도 안 돌았어도(고프레임률) 갱신 비용은 셀 순회 한 번이라 그냥 매 프레임 돌린다.
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_Density);
		DensityGrid.Update(UnitDatas);
	}

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
			// 도착 판정과 새 목적지 선정은 여기(난수를 쥔 쪽)에서 한다.
			// CrowdSteering은 목적지가 정해진 뒤의 이동 계산만 맡는다.
			if (CrowdSteering::HasArrived(Unit, ArrivalRadiusUU))
			{
				Unit.TargetLocation = PickRandomDestination();
			}
		}
		else
		{
			UpdatePartyMemberTarget(AgentIndex, Unit);
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

int32 UFF_CrowdSimulationSubsystem::PickPartySize()
{
	if (!bEnablePartyGrouping || PartySizeWeights.Num() == 0)
	{
		return 1;
	}

	float TotalWeight = 0.f;
	for (const float Weight : PartySizeWeights)
	{
		TotalWeight += FMath::Max(Weight, 0.f);
	}

	// 전부 0이거나 음수면 추첨할 것이 없다. 1인으로 떨어뜨린다.
	if (TotalWeight <= 0.f)
	{
		return 1;
	}

	float Roll = Rng.FRandRange(0.f, TotalWeight);
	for (int32 i = 0; i < PartySizeWeights.Num(); ++i)
	{
		Roll -= FMath::Max(PartySizeWeights[i], 0.f);
		if (Roll <= 0.f)
		{
			return i + 1;
		}
	}

	return PartySizeWeights.Num();
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

void UFF_CrowdSimulationSubsystem::UpdatePartyMemberTarget(int32 AgentIndex, FUnitData& Unit)
{
	const int32 LeaderIndex = Unit.LeaderAgentIndex;

	// 리더가 사라졌거나 그 슬롯이 다른 파티로 재활용됐으면 단독으로 승격시킨다.
	// 그냥 두면 없는 리더를 영원히 따라가느라 제자리에 멈춘 Agent가 생긴다.
	if (!UnitDatas.IsValidIndex(LeaderIndex)
		|| !UnitDatas[LeaderIndex].bIsActive
		|| UnitDatas[LeaderIndex].PartyID != Unit.PartyID)
	{
		Unit.LeaderAgentIndex = AgentIndex;
		Unit.PartySlotIndex = 0;
		return;
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
		UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] Agent %d is already released"), AgentIndex);
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


int32 UFF_CrowdSimulationSubsystem::SpawnAgent(UWorld& InWorld, FVector SpawnLocation, FRotator SpawnRotation)
{
	if (FreeAgentSlotIndex.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] No free agent slot available"));
		return INDEX_NONE;
	}

	int32 index = FreeAgentSlotIndex.Pop();

	FUnitData& Unit = UnitDatas[index];
	Unit.Position = SpawnLocation;
	Unit.PrevPosition = SpawnLocation;
	Unit.Rotation = SpawnRotation;
	Unit.Velocity = FVector::ZeroVector;
	Unit.Force = FVector::ZeroVector;
	Unit.TargetLocation = PickRandomDestination();

	// 기본값은 1인 파티(자기 자신이 리더)다. 파티로 묶는 것은 SpawnAgents가 이 뒤에 덮어쓴다.
	// 여기서 반드시 되돌려야 하는 이유: 슬롯은 재사용되므로, 초기화하지 않으면 재활용된 슬롯이
	// 이전 파티의 리더 인덱스를 그대로 물고 남의 파티를 따라간다.
	Unit.PartyID = NextPartyID++;
	Unit.LeaderAgentIndex = index;
	Unit.PartySlotIndex = 0;

	// 속도도 기본값으로 되돌린다. 이전에 파티 멤버였던 슬롯은 가속 배율이 곱해진 상태이므로,
	// 초기화하지 않으면 재사용될 때마다 배율이 누적되어 시간이 갈수록 빨라진다.
	{
		const FUnitData DefaultUnit;
		Unit.Speed = DefaultUnit.Speed;
		Unit.MaxSpeed = DefaultUnit.MaxSpeed;
	}

	// 슬롯을 실제로 쓰기 시작하는 지점. 여기서 켜야 풀에 놀고 있는 슬롯이 시뮬레이션에 끌려들어가지 않는다.
	Unit.bIsActive = true;
	++ActiveAgentCount;

	// 화면에 보이도록 Actor를 붙인다. 풀이 고갈되면 시뮬레이션만 된다.
	BindAgentToPool(index);

	return index;
}

int32 UFF_CrowdSimulationSubsystem::SpawnAgents(int32 Count, FVector SpawnCenter, float SpawnRadiusUU)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return 0;
	}

	int32 SpawnedCount = 0;
	int32 PartiesFormed = 0;
	bool bAgentPoolExhausted = false;

	// 파티 단위로 스폰한다. 개인별로 위치를 뽑으면 같은 파티가 맵 반대편에 흩어져 시작하므로
	// 기준점은 파티당 한 번만 뽑고, 멤버는 그 기준점 주변 대형 자리에 놓는다.
	while (SpawnedCount < Count && !bAgentPoolExhausted)
	{
		// PickRandomDestination과 같은 방식 — 제곱근을 씌워 원 안에 고르게 흩뿌린다.
		const float Angle = Rng.FRandRange(0.f, 2.f * PI);
		const float Radius = SpawnRadiusUU * FMath::Sqrt(Rng.FRand());
		const FVector Anchor = SpawnCenter + FVector(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), 0.f);

		// 같이 온 사람들은 같은 방향을 보고 있다. 파티당 한 번만 뽑는다.
		const FRotator PartyRotation(0.f, Rng.FRandRange(0.f, 360.f), 0.f);

		// 남은 인원보다 큰 파티는 잘라 넣는다. 요청 인원을 정확히 맞춰야 테스트와 시연 수치가 어긋나지 않는다.
		const int32 PartySize = FMath::Min(PickPartySize(), Count - SpawnedCount);

		int32 LeaderIndex = INDEX_NONE;
		int32 PartyID = INDEX_NONE;

		for (int32 Slot = 0; Slot < PartySize; ++Slot)
		{
			const FVector SpawnLocation = Anchor + GetPartyFormationOffset(Slot);

			const int32 AgentIndex = SpawnAgent(*World, SpawnLocation, PartyRotation);
			if (AgentIndex == INDEX_NONE)
			{
				// Agent 풀이 고갈됐다. 더 시도해도 같은 결과다.
				// 여기서 끊긴 파티는 이미 스폰된 멤버만으로 유지된다(리더는 항상 먼저 스폰되므로 리더 없는 파티는 없다).
				bAgentPoolExhausted = true;
				break;
			}

			FUnitData& Unit = UnitDatas[AgentIndex];

			if (Slot == 0)
			{
				// 리더. SpawnAgent가 이미 자기 자신을 리더로, 새 PartyID를 부여해 뒀다.
				LeaderIndex = AgentIndex;
				PartyID = Unit.PartyID;
			}
			else
			{
				Unit.PartyID = PartyID;
				Unit.LeaderAgentIndex = LeaderIndex;
				Unit.PartySlotIndex = Slot;

				// 리더보다 조금 빠르게 걸을 수 있어야 벌어진 간격을 좁힐 수 있다.
				// Speed는 희망 속도, MaxSpeed는 상한이라 둘 다 올려야 실제로 빨라진다.
				const float SpeedBoost = FMath::Max(PartyMemberSpeedBoost, 1.f);
				Unit.Speed *= SpeedBoost;
				Unit.MaxSpeed *= SpeedBoost;

				// 첫 스텝에서 어차피 갱신되지만, 그 한 프레임 동안 엉뚱한 방향으로 몸이 돌아가는 것을 막는다.
				Unit.TargetLocation = UnitDatas[LeaderIndex].TargetLocation + GetPartyFormationOffset(Slot);
			}

			++SpawnedCount;
		}

		if (LeaderIndex != INDEX_NONE)
		{
			++PartiesFormed;

			// 멤버들도 SpawnAgent에서 각자 ID를 하나씩 소비했다. 그만큼 되돌려 ID가 건너뛰지 않게 한다.
			NextPartyID = PartyID + 1;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[CrowdSimulationSubsystem] Spawned %d/%d agents in %d parties (active: %d)"),
		SpawnedCount, Count, PartiesFormed, ActiveAgentCount);

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
