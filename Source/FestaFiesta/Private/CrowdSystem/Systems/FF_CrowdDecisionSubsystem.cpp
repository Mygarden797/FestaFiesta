// Copyright Epic Games, Inc. All Rights Reserved.

#include "CrowdSystem/Systems/FF_CrowdDecisionSubsystem.h"

#include "CoreGlobals.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"
#include "CrowdSystem/Types/FF_SimulationSettings.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "FF_FFZSWorldSubsystem.h"
#include "NPC/Destination/FF_UtilityDestinationSelector.h"
#include "NPC/Integration/FF_ZoneSystemBridge.h"

DEFINE_LOG_CATEGORY_STATIC(LogCrowdDecision, Log, All);

void UFF_CrowdDecisionSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	UFF_CrowdSimulationSubsystem* Crowd = InWorld.GetSubsystem<UFF_CrowdSimulationSubsystem>();
	if (!Crowd)
	{
		UE_LOG(LogCrowdDecision, Warning,
			TEXT("CrowdSimulationSubsystem이 없다. 목적지 결정이 연결되지 않는다"));
		return;
	}

	// 단일 델리게이트다. 조용히 덮으면 어느 쪽이 결정권을 가졌는지 로그에 남지 않는다.
	if (Crowd->OnPartyNeedsDecision.IsBound())
	{
		UE_LOG(LogCrowdDecision, Warning,
			TEXT("이미 다른 결정자가 물려 있다. 덮어쓴다 — 결정 권한이 둘이면 안 된다"));
	}

	Crowd->OnPartyNeedsDecision.BindUObject(this, &UFF_CrowdDecisionSubsystem::HandlePartyNeedsDecision);

	// 계수 테이블은 UFF_SimulationSettings 한 곳이 소유한다. 여기서 또 핀을 만들면
	// 레벨마다 다른 계수가 꽂혀 같은 시드에서 다른 결과가 나온다.
	const UFF_SimulationSettings* Settings = UFF_SimulationSettings::Get();
	CachedCoefficientTable = Settings ? Settings->UtilityCoefficientTable.LoadSynchronous() : nullptr;

	if (!CachedCoefficientTable)
	{
		UE_LOG(LogCrowdDecision, Warning,
			TEXT("UtilityCoefficientTable 미설정 — Choose가 코드 기본 계수로 동작한다. ")
			TEXT("동작은 하지만 계수 튜닝 결과가 반영되지 않는다"));
	}

	UE_LOG(LogCrowdDecision, Log, TEXT("목적지 결정 연결 완료 (후보 상한 %d)"), MaxCandidates);
}

void UFF_CrowdDecisionSubsystem::Deinitialize()
{
	if (const UWorld* World = GetWorld())
	{
		if (UFF_CrowdSimulationSubsystem* Crowd = World->GetSubsystem<UFF_CrowdSimulationSubsystem>())
		{
			// 이 객체가 물어놓은 것만 푼다. 남이 물어놨으면 그쪽 결정권을 빼앗지 않는다.
			if (Crowd->OnPartyNeedsDecision.IsBoundToObject(this))
			{
				Crowd->OnPartyNeedsDecision.Unbind();
			}
		}
	}

	Super::Deinitialize();
}

void UFF_CrowdDecisionSubsystem::SetDecisionEnabled(bool bEnabled)
{
	const UWorld* World = GetWorld();
	UFF_CrowdSimulationSubsystem* Crowd = World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;
	if (!Crowd)
	{
		return;
	}

	if (bEnabled)
	{
		if (!Crowd->OnPartyNeedsDecision.IsBoundToObject(this))
		{
			Crowd->OnPartyNeedsDecision.BindUObject(this, &UFF_CrowdDecisionSubsystem::HandlePartyNeedsDecision);
		}
	}
	else if (Crowd->OnPartyNeedsDecision.IsBoundToObject(this))
	{
		Crowd->OnPartyNeedsDecision.Unbind();
	}

	// 이미 목적지를 받아 걸어가는 파티는 그 목적지를 유지한다. 도착해서 다음 결정을 할 때부터
	// 새 설정이 적용되므로, 전환 직후 몇 초는 두 경로가 섞인 상태다.
	UE_LOG(LogCrowdDecision, Display,
		TEXT("결정 연결 %s — 이미 이동 중인 파티는 다음 도착부터 반영된다"),
		bEnabled ? TEXT("켜짐") : TEXT("꺼짐 (배회만)"));
}

bool UFF_CrowdDecisionSubsystem::IsDecisionEnabled() const
{
	const UWorld* World = GetWorld();
	const UFF_CrowdSimulationSubsystem* Crowd = World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;

	return Crowd && Crowd->OnPartyNeedsDecision.IsBoundToObject(this);
}

int32 UFF_CrowdDecisionSubsystem::Debug_OpenAllValidatedFacilities(int32& OutFailedCount)
{
	OutFailedCount = 0;

	UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
	if (!ZoneSystem)
	{
		return 0;
	}

	const FFFZSReadModel ReadModel = ZoneSystem->BuildReadModel();

	int32 Opened = 0;

	for (const FFFZSFacilityReadModel& Facility : ReadModel.Facilities)
	{
		if (Facility.State == EFFZSFacilityState::Active)
		{
			continue;
		}

		FFFZSCommand Command;
		Command.Type = EFFZSCommandType::Open;
		Command.FacilityInstanceId = Facility.FacilityInstanceId;

		// CommandId와 TransactionId는 **둘 다** 필수다. 하나라도 비면 ExecuteCommand가
		// DataInvalid로 거절한다 (FF_FFZSWorldSubsystem.cpp:1521).
		// 매번 새로 발급해야 같은 시설을 닫았다 열 때 중복 재생으로 무시되지 않는다.
		Command.CommandId = FGuid::NewGuid();

		// 시설마다 따로 발급한다 — 하나로 묶으면 한 시설이 실패할 때 나머지까지 롤백된다.
		Command.TransactionId = FGuid::NewGuid();

		// -1 = 실행 시점의 최신 리비전 허용. 연속 개장은 리비전을 매번 올리므로
		// 고정 값을 넣으면 두 번째부터 낙관적 동시성 검사에 걸린다.
		Command.ExpectedWorldRevision = -1;

		const FFFZSCommandResult Result = ZoneSystem->ExecuteCommand(Command);
		if (Result.bSucceeded)
		{
			++Opened;
			continue;
		}

		++OutFailedCount;

		// 숫자 코드만 남기면 헤더를 열어 enum을 세야 원인을 안다. 이름과 Message를 같이 찍는다.
		const UEnum* StateEnum = StaticEnum<EFFZSFacilityState>();
		const UEnum* FailureEnum = StaticEnum<EFFZSPlacementFailure>();

		UE_LOG(LogCrowdDecision, Warning,
			TEXT("개장 실패: %s | 상태 %s | 사유 %s | %s"),
			*Facility.FacilityDefinitionId.ToString(),
			StateEnum ? *StateEnum->GetNameStringByValue(static_cast<int64>(Facility.State)) : TEXT("?"),
			FailureEnum ? *FailureEnum->GetNameStringByValue(static_cast<int64>(Result.FailureReason)) : TEXT("?"),
			*Result.Message.ToString());
	}

	return Opened;
}

UFFZSWorldSubsystem* UFF_CrowdDecisionSubsystem::GetZoneSystem() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
}

const FFFZSReadModel& UFF_CrowdDecisionSubsystem::GetReadModelForThisFrame()
{
	if (CachedReadModelFrame == GFrameCounter)
	{
		return CachedReadModel;
	}

	CachedReadModelFrame = GFrameCounter;

	// 실패해도 빈 스냅샷을 남긴다 — 직전 프레임의 낡은 시설 목록으로 결정하면
	// 이미 철거된 시설로 사람을 보낸다.
	CachedReadModel = FFFZSReadModel();

	if (const UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem())
	{
		CachedReadModel = ZoneSystem->BuildReadModel();
	}

	return CachedReadModel;
}

bool UFF_CrowdDecisionSubsystem::HandlePartyNeedsDecision(
	const FFF_PartyDecisionContext& Context, FVector& OutDestination)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdDecision_Decide);

	++Stats.Requests;

	const FFFZSReadModel& ReadModel = GetReadModelForThisFrame();

	// 시설을 아직 짓지 않은 상태. 인게임 배치 구조에서는 이게 시작 상태이므로
	// 경고를 내지 않는다 — Crowd가 배회로 돌아간다.
	if (ReadModel.Facilities.IsEmpty())
	{
		++Stats.NoFacility;
		return false;
	}

	/**
	 * 후보가 될 수 있는 시설이 하나라도 있는지 먼저 본다.
	 *
	 * BuildCandidates는 후보 0개일 때 시설당 Warning을 낸다. 결정은 프레임당 최대
	 * MaxDecisionsPerFrame번 일어나므로, 개장된 시설이 없는 동안 그 경고가 초당 수천 줄
	 * 쏟아져 로그가 못 쓰게 된다 — 정작 봐야 할 다른 경고가 묻힌다.
	 *
	 * 판정 기준은 BuildCandidates의 상태 필터와 같아야 한다(Active이고 재배치 대기 아님).
	 * 어긋나면 이 사전 필터를 통과했는데 후보가 0개인 상태가 생겨 경고가 다시 새어 나온다.
	 */
	const bool bAnyOpenFacility = ReadModel.Facilities.ContainsByPredicate(
		[](const FFFZSFacilityReadModel& Facility)
		{
			return Facility.State == EFFZSFacilityState::Active && !Facility.bNeedsRelocation;
		});

	if (!bAnyOpenFacility)
	{
		++Stats.NoOpenFacility;
		return false;
	}

	const UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
	if (!ZoneSystem)
	{
		++Stats.NoZoneSystem;
		return false;
	}

	// PreviousZoneId(히스테리시스)를 넘기지 않는다. 그러려면 파티별 이전 Zone을 이 계층이
	// 들고 있어야 하는데, 조정자는 파티 퇴장을 모르므로 그 맵이 무한히 자란다.
	// 넘기지 않아도 판정은 ZoneId 사전순으로 결정적이다 — 경계에 정확히 선 파티만 흔들린다.
	const FName CurrentZoneId = ZoneSystem->ResolveZoneAtWorldLocation(Context.PartyWorldLocation);
	if (CurrentZoneId.IsNone())
	{
		// Zone 경계 밖에 있다. Zone을 넓히거나 스폰 위치를 Zone 안으로 옮겨야 한다.
		++Stats.OutsideZone;
		return false;
	}

	const TArray<FFF_DestinationCandidate> Candidates = FFF_ZoneSystemBridge::BuildCandidates(
		ReadModel, CurrentZoneId, Context.PartyWorldLocation, MaxCandidates);

	// 현재·인접 Zone에 열린 시설이 없다. Info·Safety는 후보에서 제외되므로
	// 그 둘만 지었을 때도 여기로 온다.
	if (Candidates.IsEmpty())
	{
		++Stats.NoCandidate;
		return false;
	}

	// 취향 벡터는 아키타입에서 합성된다. FUnitData::ArchetypeID가 아직 NAME_None이라
	// 빈 맵을 넘긴다 — Choose는 preference 항을 0으로 계산하고 크래시하지 않는다.
	// 결과적으로 6항 중 attraction·distance·density 셋만 동작한다.
	static const TMap<FName, float> EmptyPreferenceVector;

	if (!bWarnedNoPreferenceVector)
	{
		bWarnedNoPreferenceVector = true;
		UE_LOG(LogCrowdDecision, Warning,
			TEXT("취향 벡터가 비어 있다 (아키타입 미구현) — preference·need·revisit 항이 0이다. ")
			TEXT("시설 쏠림은 재현되지만 개인차는 나오지 않는다"));
	}

	const UFF_SimulationSettings* Settings = UFF_SimulationSettings::Get();
	const int32 GlobalSeed = Settings ? Settings->GlobalSimulationSeed : 0;

	// ChoiceRoundIndex를 그대로 넘기는 것이 중요하다. 이 값이 고정되면 같은 파티가
	// 매번 같은 시드를 받아 같은 곳만 반복해서 고른다.
	const FFF_DestinationDecision Decision = FFF_UtilityDestinationSelector::Choose(
		Candidates,
		EmptyPreferenceVector,
		CachedCoefficientTable.Get(),
		GlobalSeed,
		Context.StableAgentId,
		Context.ChoiceRoundIndex);

	// no_choice는 오류가 아니라 판단이다 — "지금은 아무 데도 안 간다".
	// Crowd의 배회 폴백이 곧 Utility 스펙의 no_choice → Wander다.
	if (Decision.bIsNoChoice || Decision.ChosenDestinationId.IsNone())
	{
		++Stats.NoChoice;
		return false;
	}

	FGuid FacilityInstanceId;
	if (!FFF_ZoneSystemBridge::TryParseDestinationId(Decision.ChosenDestinationId, FacilityInstanceId))
	{
		// 후보를 만든 쪽과 파싱하는 쪽이 같은 규칙을 쓰지 않는다는 뜻이다. 계약 위반.
		++Stats.ParseFailed;
		UE_LOG(LogCrowdDecision, Warning,
			TEXT("DestinationId '%s'를 FGuid로 파싱하지 못했다 (파티 %d)"),
			*Decision.ChosenDestinationId.ToString(), Context.PartyId);
		return false;
	}

	const FFFZSFacilityReadModel* Facility = ReadModel.Facilities.FindByPredicate(
		[&FacilityInstanceId](const FFFZSFacilityReadModel& Candidate)
		{
			return Candidate.FacilityInstanceId == FacilityInstanceId;
		});

	if (!Facility)
	{
		// 같은 스냅샷에서 후보를 만들었으므로 정상적으로는 도달하지 않는다.
		++Stats.FacilityMissing;
		return false;
	}

	// ★ bHasApproachWorldTransform을 확인하지 않고 Identity로 폴백하면
	// NPC가 조용히 월드 원점으로 걸어간다. false는 반드시 재평가로 처리한다.
	if (!Facility->bHasApproachWorldTransform)
	{
		++Stats.InvalidApproach;
		UE_LOG(LogCrowdDecision, Warning,
			TEXT("시설 %s의 Entry[0] 접근점이 파생되지 않았다 — FacilityDefinition 확인 필요"),
			*Facility->FacilityDefinitionId.ToString());
		return false;
	}

	// Actor 중심(WorldLocation)이 아니라 Entry[0] 접근점을 쓴다.
	// Actor origin은 배치·회전의 기준점일 뿐이고, 큰 무대·부스에서는 메시 내부일 수 있다.
	OutDestination = Facility->ApproachWorldTransform.GetLocation();

	++Stats.Chosen;

	UE_LOG(LogCrowdDecision, Verbose,
		TEXT("파티 %d (개체 %d, 회차 %d, 사유 %d) → %s, 후보 %d개"),
		Context.PartyId, Context.StableAgentId, Context.ChoiceRoundIndex,
		static_cast<int32>(Context.Reason),
		*Facility->FacilityDefinitionId.ToString(), Candidates.Num());

	return true;
}

// --- 콘솔 -----------------------------------------------------------------------------------

namespace FFDecisionConsole
{
	/**
	 * 콘솔 명령은 UE_LOG가 아니라 FOutputDevice에 찍는다.
	 *
	 * UE_LOG만 쓰면 에디터 Output Log에만 남고, 명령을 입력한 인게임 콘솔에는 아무것도 안 뜬다.
	 * 화면을 보고 있던 사람은 "명령이 먹지 않았다"고 판단한다.
	 * 기존 FF.Crowd.* 명령들이 쓰는 패턴과 맞춘 것이기도 하다.
	 */
	UFF_CrowdDecisionSubsystem* GetDecision(UWorld* World, FOutputDevice& Ar)
	{
		UFF_CrowdDecisionSubsystem* Decision = World ? World->GetSubsystem<UFF_CrowdDecisionSubsystem>() : nullptr;
		if (!Decision)
		{
			Ar.Logf(TEXT("CrowdDecisionSubsystem이 없다 (PIE가 실행 중인지 확인)"));
		}
		return Decision;
	}
}

static void DecisionStatsCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_CrowdDecisionSubsystem* Decision = FFDecisionConsole::GetDecision(World, Ar);
	if (!Decision)
	{
		return;
	}

	if (Args.Num() > 0 && Args[0].Equals(TEXT("reset"), ESearchCase::IgnoreCase))
	{
		Decision->ResetStats();
		Ar.Logf(TEXT("통계를 초기화했다"));
		return;
	}

	const FFF_CrowdDecisionStats& S = Decision->GetStats();

	Ar.Logf(TEXT("--- 목적지 결정 ---"));
	Ar.Logf(TEXT("연결             : %s"),
		Decision->IsDecisionEnabled() ? TEXT("켜짐") : TEXT("꺼짐 — 배회만 한다"));
	Ar.Logf(TEXT("요청             : %d"), S.Requests);
	Ar.Logf(TEXT("시설 좌표 지급   : %d   <- 0이면 Utility가 한 번도 안 먹었다"), S.Chosen);
	Ar.Logf(TEXT("[폴백] 시설 없음     : %d   (인게임에 짓기 전에는 정상)"), S.NoFacility);
	Ar.Logf(TEXT("[폴백] 개장 안 됨    : %d   <- FF.Decision.OpenFacilities"), S.NoOpenFacility);
	Ar.Logf(TEXT("[폴백] Zone 밖       : %d   (Zone 경계 밖에 스폰됐다)"), S.OutsideZone);
	Ar.Logf(TEXT("[폴백] 후보 0개      : %d   (인접 Zone에 열린 시설이 없다)"), S.NoCandidate);
	Ar.Logf(TEXT("[폴백] no_choice     : %d   (Choose의 정상 판단)"), S.NoChoice);
	Ar.Logf(TEXT("[문제] ZoneSystem 없음: %d"), S.NoZoneSystem);
	Ar.Logf(TEXT("[문제] GUID 파싱 실패 : %d"), S.ParseFailed);
	Ar.Logf(TEXT("[문제] 시설 소실      : %d"), S.FacilityMissing);
	Ar.Logf(TEXT("[문제] 접근점 무효    : %d   (FacilityDefinition의 Entry[0] 확인)"), S.InvalidApproach);
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GDecisionStatsCommand(
	TEXT("FF.Decision.Stats"),
	TEXT("목적지 결정이 어디서 끝났는지 센다. FF.Decision.Stats [reset]"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DecisionStatsCommand));

static void DecisionEnableCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_CrowdDecisionSubsystem* Decision = FFDecisionConsole::GetDecision(World, Ar);
	if (!Decision)
	{
		return;
	}

	// 인자가 없으면 토글. 같은 명령을 반복해 A/B를 왕복하기 쉽다
	const bool bEnable = (Args.Num() > 0)
		? (Args[0] != TEXT("0"))
		: !Decision->IsDecisionEnabled();

	Decision->SetDecisionEnabled(bEnable);

	Ar.Logf(TEXT("결정 연결 %s — 이미 이동 중인 파티는 다음 도착부터 반영된다"),
		bEnable ? TEXT("켜짐") : TEXT("꺼짐 (배회만)"));
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GDecisionEnableCommand(
	TEXT("FF.Decision.Enable"),
	TEXT("목적지 결정 연결을 켜고 끈다(대조군 만들기). FF.Decision.Enable [0|1], 인자 없으면 토글"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DecisionEnableCommand));

namespace
{
	/**
	 * Validated 상태로 남아 있는 시설을 전부 개장한다.
	 *
	 * ### 왜 이 명령이 필요한가
	 * 시설 수명주기는 Install → Validated → (Open 명령) → Active이고, BuildCandidates는
	 * Active만 후보로 받는다. 그런데 빌드 모드가 발행하는 명령은 Install·Move·Remove뿐이고
	 * **Open을 발행하는 프로덕션 코드가 없다.** 그래서 인게임에서 지은 시설은 계속 Validated에
	 * 머물고 목적지 후보가 되지 않는다.
	 *
	 * 이건 임시 도구다. 개장 시점(축제 오픈 시각, 운영자 조작)은 게임플레이 설계 사안이므로
	 * 정식 경로가 정해지면 이 명령은 지워야 한다.
	 */
	void OpenAllFacilities(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		UFF_CrowdDecisionSubsystem* Decision = FFDecisionConsole::GetDecision(World, Ar);
		if (!Decision)
		{
			return;
		}

		// 실제 구현은 BlueprintCallable 멤버 함수에 있다. 콘솔과 데모 BP가 같은 코드를 타야
		// "콘솔로는 되는데 BP로는 안 된다"가 생기지 않는다.
		int32 Failed = 0;
		const int32 Opened = Decision->Debug_OpenAllValidatedFacilities(Failed);

		if (Opened == 0 && Failed == 0)
		{
			Ar.Logf(TEXT("개장할 시설이 없다 - 등록된 시설이 없거나 이미 전부 Active다"));
			return;
		}

		Ar.Logf(TEXT("시설 개장: 성공 %d / 실패 %d  (실패 사유는 위 Warning 참조)"), Opened, Failed);
	}
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GOpenAllFacilitiesCommand(
	TEXT("FF.Decision.OpenFacilities"),
	TEXT("Validated 상태 시설을 전부 Active로 개장한다(빌드 모드가 Open을 발행하지 않으므로 필요한 임시 도구)"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&OpenAllFacilities));
