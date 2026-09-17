// FF_ZonePopulationBridgeSubsystem.cpp

// Crowd 에이전트 위치를 존별 인구로 집계해 FF_ZoneSystem에 주기적으로 보고한다.
// 흐름: 시뮬레이션 시계 차분을 누적하고 간격을 넘으면 슬롯을 훑어 세고 등록 존 전체에 표본을 넘긴다.
// 불변식: 인원 0인 등록 존도 매 사이클 보고하며 보고 순서는 ZoneId 사전순으로 결정적이다.
// 실패/성능: 전부 거부되면 경과 시간을 이월(상한 3사이클)하고, 슬롯별 직전 존 캐시로 경계 판정을 줄인다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Zone/FF_ZonePopulationBridgeSubsystem.h"

#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "Engine/World.h"
#include "FF_FFZSTypes.h"
#include "FF_FFZSWorldSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Subsystems/SubsystemCollection.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFZonePopulation, Log, All);

namespace FFZonePopulation
{
	// 결선 전후를 같은 빌드에서 비교할 수 있어야 한다. 0이면 Tick이 즉시 빠져나가 비용도 0이다.
	static TAutoConsoleVariable<int32> CVarReporting(
		TEXT("FF.Zone.PopulationReporting"),
		1,
		TEXT("1이면 Crowd 인원을 FF_ZoneSystem 존 인구로 보고한다. 0이면 완전히 멈춘다(A/B 비교용)."),
		ECVF_Default);

	// 밀도 승격 유지 시간이 5초(Warning)·3초(Critical)다. 그보다 충분히 짧아야 승격 판정에
	// 표본이 여러 개 들어간다. 실제 운영 주기는 Crowd 담당과 확정할 값이다.
	static TAutoConsoleVariable<float> CVarIntervalSeconds(
		TEXT("FF.Zone.PopulationIntervalSeconds"),
		1.0f,
		TEXT("존 인구 보고 간격(시뮬레이션 초). 밀도 유지 시간 5초·3초보다 짧게 유지할 것."),
		ECVF_Default);

	// 보고가 계속 거부될 때 경과 시간이 무한정 쌓이는 것을 막는다.
	// 표본은 "지금 몇 명"이라 과거 구간의 인원을 대신 말해주지 못한다. 30초를 한 표본에 실어 보내면
	// 유지 시간 임계를 단 한 번의 표본으로 충족시켜 버린다 — 그래서 이월에 상한을 둔다.
	constexpr float MaxCarryIntervalMultiplier = 3.f;

	// 0에 가까운 간격은 매 프레임 전 존을 보고하게 만든다. 오타로 0을 넣어도 그렇게 되지 않게 막는다.
	constexpr float MinIntervalSeconds = 0.05f;

	float GetIntervalSeconds()
	{
		return FMath::Max(CVarIntervalSeconds.GetValueOnGameThread(), MinIntervalSeconds);
	}

	bool IsReportingEnabled()
	{
		return CVarReporting.GetValueOnGameThread() != 0;
	}
}

void UFF_ZonePopulationBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// 보고 대상인 Zone 레지스트리를 먼저 세운다. Crowd는 월드 시작 시점에 따라 늦게 준비될 수 있어
	// 여기서 강제하지 않고 Tick에서 결합한다 — 초기화 순서 차이가 영구 실패가 되지 않게 한다.
	Collection.InitializeDependency<UFFZSWorldSubsystem>();
	Super::Initialize(Collection);

	UWorld* World = GetWorld();
	ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
}

void UFF_ZonePopulationBridgeSubsystem::Deinitialize()
{
	ZoneSystem = nullptr;
	Crowd = nullptr;
	LastZoneBySlot.Reset();
	SlotBuffer.Reset();
	PendingDeltaSeconds = 0.f;
	LastSimTimeSeconds = 0.f;
	Super::Deinitialize();
}

bool UFF_ZonePopulationBridgeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UFF_ZonePopulationBridgeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_ZonePopulationBridgeSubsystem, STATGROUP_Tickables);
}

void UFF_ZonePopulationBridgeSubsystem::Tick(const float DeltaTime)
{
	(void)DeltaTime;

	if (!FFZonePopulation::IsReportingEnabled() || !ZoneSystem)
	{
		return;
	}

	if (!Crowd)
	{
		UWorld* World = GetWorld();
		Crowd = World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;
	}

	// 시뮬레이션이 시작되기 전에는 보고할 인원 자체가 없다. 0을 보내면 "측정했더니 0명"과
	// "아직 측정하지 않았다"가 구분되지 않으므로 아무것도 보내지 않는다.
	if (!Crowd || !Crowd->IsSimulationInitialized())
	{
		return;
	}

	const float SimTimeSeconds = Crowd->GetSimTimeSeconds();
	const float AdvancedSeconds = SimTimeSeconds - LastSimTimeSeconds;
	LastSimTimeSeconds = SimTimeSeconds;

	// 레벨 재시작이나 시계 초기화로 되감기면 누적을 버리고 새 기준으로 다시 센다.
	if (AdvancedSeconds < 0.f)
	{
		PendingDeltaSeconds = 0.f;
		return;
	}

	const float IntervalSeconds = FFZonePopulation::GetIntervalSeconds();
	PendingDeltaSeconds = FMath::Min(
		PendingDeltaSeconds + AdvancedSeconds,
		IntervalSeconds * FFZonePopulation::MaxCarryIntervalMultiplier);

	if (PendingDeltaSeconds < IntervalSeconds)
	{
		return;
	}

	RunReportCycle(PendingDeltaSeconds);
}

bool UFF_ZonePopulationBridgeSubsystem::ReportNow()
{
	if (!ZoneSystem)
	{
		return false;
	}

	if (!Crowd)
	{
		UWorld* World = GetWorld();
		Crowd = World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;
	}

	if (!Crowd)
	{
		return false;
	}

	// 강제 보고는 간격을 무시하지만 경과 시간까지 지어내지는 않는다. 아직 시간이 흐르지 않았다면
	// 0초짜리 표본이 나가고, 그러면 유지 시간은 늘지 않고 현재 비율만 다시 평가된다.
	const float SimTimeSeconds = Crowd->GetSimTimeSeconds();
	PendingDeltaSeconds = FMath::Max(PendingDeltaSeconds + (SimTimeSeconds - LastSimTimeSeconds), 0.f);
	LastSimTimeSeconds = SimTimeSeconds;

	return RunReportCycle(PendingDeltaSeconds);
}

void UFF_ZonePopulationBridgeSubsystem::CaptureAgentSlots()
{
	const int32 SlotCount = Crowd->GetAgentPoolSize();
	SlotBuffer.SetNum(SlotCount, EAllowShrinking::No);

	for (int32 Slot = 0; Slot < SlotCount; ++Slot)
	{
		FFF_ZoneAgentSlot& Sample = SlotBuffer[Slot];
		Sample.bActive = Crowd->IsAgentActive(Slot);
		Sample.Location = Sample.bActive ? Crowd->GetAgentPosition(Slot) : FVector::ZeroVector;
	}

	// 풀이 줄어든 뒤 뒤쪽에 남은 원소가 활성으로 남아 있으면 유령 인원이 잡힌다.
	for (int32 Slot = SlotCount; Slot < SlotBuffer.Num(); ++Slot)
	{
		SlotBuffer[Slot].bActive = false;
	}
}

bool UFF_ZonePopulationBridgeSubsystem::RunReportCycle(const float DeltaSeconds)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FFZonePopulation_Report);

	const TArray<FName> ZoneIds = ZoneSystem->GetRegisteredZoneIds();
	if (ZoneIds.IsEmpty())
	{
		// 존이 없는 월드(군중 전용 테스트 맵 등)에서는 보고할 대상이 없다. 이월할 이유도 없으므로
		// 시간은 흘려보낸다 — 존이 나중에 등록되면 그때부터 새로 센다.
		PendingDeltaSeconds = 0.f;
		return false;
	}

	CaptureAgentSlots();

	TMap<FName, int32> PopulationByZoneId;
	int32 OutsideZoneCount = 0;
	TallyByZone(*ZoneSystem, ZoneIds, SlotBuffer, LastZoneBySlot, PopulationByZoneId, OutsideZoneCount);

	int32 ReportedZoneCount = 0;
	int32 RejectedZoneCount = 0;
	int32 SampledAgentCount = OutsideZoneCount;

	// ZoneIds 순서로 돈다. 맵 순회 순서는 보장되지 않아 로그 비교와 재현이 어려워진다.
	for (const FName ZoneId : ZoneIds)
	{
		const int32 Population = PopulationByZoneId.FindRef(ZoneId);
		SampledAgentCount += Population;

		if (ZoneSystem->ReportZonePopulation(ZoneId, Population, DeltaSeconds))
		{
			++ReportedZoneCount;
		}
		else
		{
			++RejectedZoneCount;
		}
	}

	LastCycle.SimTimeSeconds = LastSimTimeSeconds;
	LastCycle.DeltaSeconds = DeltaSeconds;
	LastCycle.SampledAgentCount = SampledAgentCount;
	LastCycle.OutsideZoneCount = OutsideZoneCount;
	LastCycle.ReportedZoneCount = ReportedZoneCount;
	LastCycle.RejectedZoneCount = RejectedZoneCount;
	LastCycle.PopulationByZoneId = MoveTemp(PopulationByZoneId);

	// 하나도 반영되지 않았다 = 권한 변경이 차단 중(명령 실행·스냅샷 복원)이다. 경과 시간을 소비하면
	// 그 구간만큼 밀도 승격이 늦어지므로 이월하고 다음 Tick에 다시 시도한다.
	if (ReportedZoneCount == 0)
	{
		UE_LOG(LogFFZonePopulation, Verbose,
			TEXT("존 인구 보고가 전부 거부됐습니다(존 %d개). 경과 %.2f초를 다음 보고로 이월합니다."),
			RejectedZoneCount, DeltaSeconds);
		return false;
	}

	++LastCycle.CycleIndex;
	PendingDeltaSeconds = 0.f;

	if (!bLoggedFirstCycle)
	{
		bLoggedFirstCycle = true;
		UE_LOG(LogFFZonePopulation, Log,
			TEXT("존 인구 보고 시작 — 존 %d개, 활성 %d명(존 밖 %d명), 표본 간격 %.2f초. ")
			TEXT("끄려면 FF.Zone.PopulationReporting 0, 현황은 FF.Zone.Population."),
			ZoneIds.Num(), SampledAgentCount, OutsideZoneCount, DeltaSeconds);
	}

	return true;
}

void UFF_ZonePopulationBridgeSubsystem::TallyByZone(
	const UFFZSWorldSubsystem& ZoneSystem,
	const TArray<FName>& RegisteredZoneIds,
	const TArrayView<const FFF_ZoneAgentSlot> SlotsByAgentIndex,
	TArray<FName>& InOutLastZoneBySlot,
	TMap<FName, int32>& OutPopulationByZoneId,
	int32& OutOutsideZoneCount)
{
	// 인원 0인 존을 빼면 안 된다. UpdatePopulationSample에는 감쇠가 없어서 보고를 멈춘 존은
	// 마지막 값을 영원히 유지한다 — 사람이 다 빠져나간 구역이 계속 혼잡한 것으로 남는다.
	OutPopulationByZoneId.Reset();
	OutPopulationByZoneId.Reserve(RegisteredZoneIds.Num());
	for (const FName ZoneId : RegisteredZoneIds)
	{
		OutPopulationByZoneId.Add(ZoneId, 0);
	}
	OutOutsideZoneCount = 0;

	if (InOutLastZoneBySlot.Num() < SlotsByAgentIndex.Num())
	{
		InOutLastZoneBySlot.SetNum(SlotsByAgentIndex.Num());
	}

	for (int32 Slot = 0; Slot < SlotsByAgentIndex.Num(); ++Slot)
	{
		const FFF_ZoneAgentSlot& Sample = SlotsByAgentIndex[Slot];
		if (!Sample.bActive)
		{
			// 슬롯은 재사용된다. 비워두지 않으면 퇴장자의 마지막 존이 새 방문객의 히스테리시스
			// 힌트가 되어, 경계 근처에서 엉뚱한 존에 붙어 있는 시간이 생긴다.
			InOutLastZoneBySlot[Slot] = NAME_None;
			continue;
		}

		// 직전 존을 넘기면 그 존만 먼저 검사하고 끝난다. 넘기지 않으면 매 사이클 등록 존 전체를
		// 폴리곤 판정하므로 인원 500 × 존 30 = 15,000회가 된다.
		const FName ZoneId = ZoneSystem.ResolveZoneAtWorldLocation(Sample.Location, InOutLastZoneBySlot[Slot]);
		InOutLastZoneBySlot[Slot] = ZoneId;

		if (int32* Population = OutPopulationByZoneId.Find(ZoneId))
		{
			++(*Population);
		}
		else
		{
			// ZoneId가 None이거나 이번 보고 대상에 없는 존이다. 어느 쪽이든 존 인구가 아니다.
			++OutOutsideZoneCount;
		}
	}
}

#if !UE_BUILD_SHIPPING

namespace FFZonePopulation
{
	const TCHAR* ToDisplayString(const EFFZSDensityState State)
	{
		switch (State)
		{
		case EFFZSDensityState::Caution:  return TEXT("Caution");
		case EFFZSDensityState::Warning:  return TEXT("Warning");
		case EFFZSDensityState::Critical: return TEXT("Critical");
		default:                          return TEXT("Normal");
		}
	}
}

static void ZonePopulationCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_ZonePopulationBridgeSubsystem* Bridge =
		World ? World->GetSubsystem<UFF_ZonePopulationBridgeSubsystem>() : nullptr;
	const UFFZSWorldSubsystem* ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	if (!Bridge || !ZoneSystem)
	{
		Ar.Logf(ELogVerbosity::Error, TEXT("Zone 인구 브리지를 찾을 수 없습니다 (PIE/게임 실행 중인지 확인)."));
		return;
	}

	if (Args.Num() > 0 && Args[0].Equals(TEXT("now"), ESearchCase::IgnoreCase))
	{
		Ar.Logf(TEXT("강제 보고: %s"), Bridge->ReportNow() ? TEXT("반영됨") : TEXT("거부됨"));
	}

	if (!FFZonePopulation::IsReportingEnabled())
	{
		Ar.Logf(TEXT("[주의] FF.Zone.PopulationReporting=0 — 보고가 꺼져 있어 아래 값은 갱신되지 않습니다."));
	}

	const FFF_ZonePopulationCycle Cycle = Bridge->GetLastCycle();
	if (Cycle.CycleIndex == 0)
	{
		Ar.Logf(TEXT("아직 보고된 사이클이 없습니다. 군중을 스폰했는지, 주기(%.2f초)가 지났는지 확인하세요."),
			FFZonePopulation::GetIntervalSeconds());
		return;
	}

	Ar.Logf(TEXT("=== Zone 인구 === cycle=%d simTime=%.1fs delta=%.2fs 활성=%d 존밖=%d 보고=%d 거부=%d 주기=%.2fs"),
		Cycle.CycleIndex, Cycle.SimTimeSeconds, Cycle.DeltaSeconds, Cycle.SampledAgentCount,
		Cycle.OutsideZoneCount, Cycle.ReportedZoneCount, Cycle.RejectedZoneCount,
		FFZonePopulation::GetIntervalSeconds());

	// 비율·밀도 단계는 플러그인이 계산한 값을 그대로 보여준다. 여기서 다시 계산하면 두 숫자가 갈린다.
	const FFFZSReadModel ReadModel = ZoneSystem->BuildReadModel();
	for (const FFFZSZoneReadModel& Zone : ReadModel.Zones)
	{
		Ar.Logf(TEXT("Zone=%-20s pop=%4d cap=%5d ratio=%.3f state=%-8s area=%.0fm2"),
			*Zone.ZoneId.ToString(), Zone.ReportedPopulation, Zone.SoftCapacity,
			Zone.DensityRatio, FFZonePopulation::ToDisplayString(Zone.DensityState),
			Zone.AreaSquareMeters);
	}
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GZonePopulationCommand(
	TEXT("FF.Zone.Population"),
	TEXT("존별 보고 인구·정원·밀도 비율·밀도 단계를 출력한다. FF.Zone.Population [now]"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&ZonePopulationCommand));

#endif // !UE_BUILD_SHIPPING
