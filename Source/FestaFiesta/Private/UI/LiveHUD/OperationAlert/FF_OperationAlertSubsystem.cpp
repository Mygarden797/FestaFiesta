// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/OperationAlert/FF_OperationAlertSubsystem.h"

#include "CrowdSystem/Systems/FF_CrowdDecisionSubsystem.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/FF_UILog.h"

namespace
{
	bool IsShortageKind(EFF_OperationAlertKind Kind)
	{
		return Kind == EFF_OperationAlertKind::FoodShortage
			|| Kind == EFF_OperationAlertKind::RestroomShortage
			|| Kind == EFF_OperationAlertKind::FunShortage;
	}

	// 누적 카운터가 날마다 0 으로 돌아가면 지금 값이 곧 늘어난 수다
	int32 DeltaSince(int32 Current, int32& InOutLast)
	{
		const int32 Delta = Current >= InOutLast ? Current - InOutLast : Current;
		InOutLast = Current;
		return Delta;
	}
}

bool UFF_OperationAlertSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UFF_OperationAlertSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_OperationAlertSubsystem, STATGROUP_Tickables);
}

void UFF_OperationAlertSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UFF_FestivalStageSubsystem* Stage =
		GameInstance ? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
	float LiveSeconds = 0.f;
	if (!Stage || !Stage->TryGetLiveElapsedMinutes(LiveSeconds))
	{
		return;
	}

	// 새 날이거나 Live 가 처음부터 다시 시작했다. 어제 쌓인 퇴장은 세지 않도록 기준을 지금 값으로 맞춘다
	if (Stage->CurrentDay != TrackedDay || LiveSeconds < LastEvaluatedLiveSeconds)
	{
		ResetDay(Stage->CurrentDay);
		FFF_OperationAlertInputs Baseline;
		ReadInputs(Baseline);
		LastEvaluatedLiveSeconds = LiveSeconds;
		return;
	}

	if (LiveSeconds - LastEvaluatedLiveSeconds < EvaluateIntervalLiveSeconds)
	{
		return;
	}

	FFF_OperationAlertInputs Inputs;
	if (ReadInputs(Inputs))
	{
		EvaluateAt(LiveSeconds, Stage->CurrentDay, Inputs);
	}
}

bool UFF_OperationAlertSubsystem::ReadInputs(FFF_OperationAlertInputs& OutInputs)
{
	const UWorld* World = GetWorld();
	const UFF_CrowdSimulationSubsystem* Crowd = World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;
	if (!Crowd)
	{
		return false;
	}

	OutInputs.HungerCoverage = Crowd->GetNeedCoverage(EFF_NeedType::Hunger);
	OutInputs.RestroomCoverage = Crowd->GetNeedCoverage(EFF_NeedType::Restroom);
	OutInputs.StimulationCoverage = Crowd->GetNeedCoverage(EFF_NeedType::Stimulation);
	OutInputs.QueueAbandonsDelta = DeltaSince(Crowd->GetQueuePatienceAbandons(), LastQueueAbandons);

	if (const UFF_CrowdDecisionSubsystem* Decision = World->GetSubsystem<UFF_CrowdDecisionSubsystem>())
	{
		OutInputs.NoChoiceLeavesDelta = DeltaSince(Decision->GetLeaveByNoChoicePatienceCount(), LastNoChoiceLeaves);
	}
	return true;
}

void UFF_OperationAlertSubsystem::ResetDay(int32 Day)
{
	TrackedDay = Day;
	for (float& LastRaised : LastRaisedLiveSeconds)
	{
		LastRaised = -1.f;
	}
}

void UFF_OperationAlertSubsystem::EvaluateAt(float LiveSeconds, int32 Day, const FFF_OperationAlertInputs& Inputs)
{
	if (Day != TrackedDay)
	{
		ResetDay(Day);
	}
	LastEvaluatedLiveSeconds = LiveSeconds;

	// 시설 부족은 한 번에 하나만 낸다. 시설이 거의 없는 배치에서 세 장이 한꺼번에 화면을 덮지 않게 한다
	bool bRaisedShortage = false;
	for (const EFF_OperationAlertKind Kind : FFF_OperationAlertRules::Evaluate(Inputs, Thresholds))
	{
		const bool bShortage = IsShortageKind(Kind);
		if (bShortage && bRaisedShortage)
		{
			continue;
		}
		const float LastRaised = LastRaisedLiveSeconds[static_cast<int32>(Kind)];
		const float Cooldown = bShortage ? ShortageCooldownLiveSeconds : LeaveCooldownLiveSeconds;
		if (LastRaised >= 0.f && LiveSeconds - LastRaised < Cooldown)
		{
			continue;
		}
		LastRaisedLiveSeconds[static_cast<int32>(Kind)] = LiveSeconds;
		bRaisedShortage |= bShortage;
		RaiseAlert(Kind);
	}
}

void UFF_OperationAlertSubsystem::RaiseAlert(EFF_OperationAlertKind Kind)
{
	FFF_OperationAlert Alert;
	Alert.Kind = Kind;
	Alert.Message = FFF_OperationAlertRules::GetMessage(Kind);
	Alert.Day = TrackedDay != INDEX_NONE ? TrackedDay : 1;

	History.Add(Alert);
	if (History.Num() > MaxHistory)
	{
		History.RemoveAt(0, History.Num() - MaxHistory);
	}

	UE_LOG(LogFestaFiestaUI, Log, TEXT("[운영 알림] %d일차 %s"), Alert.Day, *Alert.Message.ToString());
	OnOperationAlertRaised.Broadcast(Alert);
}

static FAutoConsoleCommandWithWorldAndArgs GFF_OperationAlertTestCommand(
	TEXT("FF.Alert.Test"),
	TEXT("운영 알림을 조건 없이 띄운다. 0 음식 · 1 화장실 · 2 즐길 거리 · 3 줄 이탈 · 4 갈 곳 없음. 예: FF.Alert.Test 1"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UFF_OperationAlertSubsystem* Alerts = World ? World->GetSubsystem<UFF_OperationAlertSubsystem>() : nullptr;
		if (!Alerts)
		{
			return;
		}
		const int32 Index = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1;
		const int32 Clamped = FMath::Clamp(Index, 0, static_cast<int32>(EFF_OperationAlertKind::Count) - 1);
		Alerts->RaiseAlert(static_cast<EFF_OperationAlertKind>(Clamped));
	}));
