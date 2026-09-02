// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/FF_ProgramEconomyBridgeSubsystem.h"

#include "Economy/FF_EconomyLog.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Time/FF_FestivalStageSubsystem.h"

bool UFF_ProgramEconomyBridgeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	// 게임 월드가 아니면 걷을 체험비도 없다. 엔진이 부팅 중에 만드는 월드에까지
	// 만들어지면 Stage 를 못 찾아 경고만 남긴다.
	const UWorld* World = Cast<UWorld>(Outer);
	return World != nullptr && World->IsGameWorld();
}

void UFF_ProgramEconomyBridgeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Initialize 에서 찾으면 안 된다. Stage 는 GameInstance 스코프라 이 월드의
	// 컬렉션에 없고, InitializeDependency 로 순서를 강제할 수도 없기 때문이다.
	UGameInstance* GameInstance = InWorld.GetGameInstance();
	Stage = GameInstance ? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;

	if (Stage == nullptr)
	{
		UE_LOG(LogFestaFiestaEconomy, Warning,
			TEXT("[경제] Stage 서브시스템이 없다. 프로그램 체험비가 걷히지 않는다."));
		return;
	}

	Stage->OnStageChanged.AddDynamic(
		this, &UFF_ProgramEconomyBridgeSubsystem::HandleStageChanged);
	Stage->OnRemainingSecondsChanged.AddDynamic(
		this, &UFF_ProgramEconomyBridgeSubsystem::HandleRemainingSecondsChanged);

	UE_LOG(LogFestaFiestaEconomy, Log,
		TEXT("[경제] 프로그램 정산 브리지가 Stage 를 구독했다."));
}

void UFF_ProgramEconomyBridgeSubsystem::Deinitialize()
{
	if (Stage != nullptr)
	{
		Stage->OnStageChanged.RemoveDynamic(
			this, &UFF_ProgramEconomyBridgeSubsystem::HandleStageChanged);
		Stage->OnRemainingSecondsChanged.RemoveDynamic(
			this, &UFF_ProgramEconomyBridgeSubsystem::HandleRemainingSecondsChanged);
		Stage = nullptr;
	}

	Super::Deinitialize();
}

void UFF_ProgramEconomyBridgeSubsystem::HandleStageChanged(
	const EFF_FestivalStage PreviousStage, const EFF_FestivalStage NewStage)
{
	ProcessStageEntry(NewStage);
}

void UFF_ProgramEconomyBridgeSubsystem::HandleRemainingSecondsChanged(const int32 NewSeconds)
{
	// 이 신호는 모든 단계에서 온다. 시각을 읽는 것은 창구 하나뿐이고, 그 창구가 Live 가
	// 아닌 단계를 걸러 준다 — ElapsedLiveSeconds 는 Setup·Closing 에서도 누적되기 때문이다.
	float NowMinutes = 0.f;
	if (!Stage || !Stage->TryGetLiveElapsedMinutes(NowMinutes))
	{
		return;
	}

	UFF_FestivalProgressionSubsystem* Progression = ResolveFestivalProgression();
	if (Progression == nullptr) return;

	if (Progression->GetScheduledPrograms().Num() == 0) return;

	Progression->SettleStartedPrograms(NowMinutes);
}

int32 UFF_ProgramEconomyBridgeSubsystem::ProcessStageEntry(const EFF_FestivalStage NewStage)
{
	if (!IsProgramSettlementStage(NewStage)) return 0;

	UFF_FestivalProgressionSubsystem* Progression = ResolveFestivalProgression();
	if (Progression == nullptr) return 0;

	if (Progression->GetScheduledPrograms().Num() == 0) return 0;

	return Progression->SettleScheduledPrograms();
}

UFF_FestivalProgressionSubsystem*
UFF_ProgramEconomyBridgeSubsystem::ResolveFestivalProgression() const
{
	// 경제는 GameInstanceSubsystem 이라 InitializeDependency 로 순서를 보장할 수 없다.
	// Initialize 시점의 nullptr 캐시를 피하려고 필요할 때마다 조회한다.
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>()
		: nullptr;
}
