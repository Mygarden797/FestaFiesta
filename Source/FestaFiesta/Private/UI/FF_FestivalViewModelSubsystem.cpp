// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/FF_FestivalViewModelSubsystem.h"

#include "HAL/IConsoleManager.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/FF_UILog.h"

void UFF_FestivalViewModelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 같은 GameInstance 안의 서브시스템이라 순서가 보장되지 않는다.
	// 지금 없으면 Tick에서 다시 시도한다.
	TryBindStageSubsystem();

#if !UE_BUILD_SHIPPING
	RegisterDebugConsoleCommands();
#endif
}

void UFF_FestivalViewModelSubsystem::Deinitialize()
{
	// 해제는 조건부, 정리는 무조건. 조기 return으로 빠져나가지 않는다.
	if (CachedStageSubsystem)
	{
		CachedStageSubsystem->OnStageChanged.RemoveDynamic(this, &UFF_FestivalViewModelSubsystem::HandleStageChanged);
	}
	CachedStageSubsystem = nullptr;

#if !UE_BUILD_SHIPPING
	UnregisterDebugConsoleCommands();
#endif

	Super::Deinitialize();
}

#if !UE_BUILD_SHIPPING
void UFF_FestivalViewModelSubsystem::RegisterDebugConsoleCommands()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();

	// 중복 등록 방어. 어떤 이유로 해제가 안 됐어도 두 번째 PIE가 조용히 망가지지 않게 한다.
	UnregisterDebugConsoleCommands();

	SetMetricsConsoleCommand = ConsoleManager.RegisterConsoleCommand(
		TEXT("FF.SetMetrics"),
		TEXT("0~100 지표를 설정한다. 사용법: FF.SetMetrics <SimSatisfaction> <ScoreSatisfaction> <Safety>"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			if (Args.Num() < 3)
			{
				UE_LOG(LogFestaFiestaUI, Warning,
					TEXT("FF.SetMetrics <Sim> <Score> <Safety> — 인자 3개가 필요하다. 받은 것: %d개"), Args.Num());
				return;
			}

			Debug_SetMetrics(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]), FCString::Atoi(*Args[2]));
			Debug_DumpToLog();
		}),
		ECVF_Cheat);

	SetEconomyConsoleCommand = ConsoleManager.RegisterConsoleCommand(
		TEXT("FF.SetEconomy"),
		TEXT("경제 값을 설정한다. 사용법: FF.SetEconomy <Gold> <TotalRevenue> <TodayRevenue>"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			if (Args.Num() < 3)
			{
				UE_LOG(LogFestaFiestaUI, Warning,
					TEXT("FF.SetEconomy <Gold> <TotalRevenue> <TodayRevenue> — 인자 3개가 필요하다. 받은 것: %d개"), Args.Num());
				return;
			}

			Debug_SetEconomy(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]), FCString::Atoi(*Args[2]));
			Debug_DumpToLog();
		}),
		ECVF_Cheat);

	DumpMetricsConsoleCommand = ConsoleManager.RegisterConsoleCommand(
		TEXT("FF.DumpMetrics"),
		TEXT("현재 지표·경제·Stage 값을 로그에 찍는다."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			Debug_DumpToLog();
		}),
		ECVF_Cheat);

	UE_LOG(LogFestaFiestaUI, Log,
		TEXT("[ViewModel] 디버그 콘솔 명령 등록 완료 — FF.SetMetrics / FF.SetEconomy / FF.DumpMetrics"));
}

void UFF_FestivalViewModelSubsystem::UnregisterDebugConsoleCommands()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();

	// 해제는 조건부, 정리(nullptr 대입)는 무조건.
	if (SetMetricsConsoleCommand)
	{
		ConsoleManager.UnregisterConsoleObject(SetMetricsConsoleCommand);
	}
	SetMetricsConsoleCommand = nullptr;

	if (SetEconomyConsoleCommand)
	{
		ConsoleManager.UnregisterConsoleObject(SetEconomyConsoleCommand);
	}
	SetEconomyConsoleCommand = nullptr;

	if (DumpMetricsConsoleCommand)
	{
		ConsoleManager.UnregisterConsoleObject(DumpMetricsConsoleCommand);
	}
	DumpMetricsConsoleCommand = nullptr;
}
#endif

UFF_FestivalStageSubsystem* UFF_FestivalViewModelSubsystem::GetStageSubsystem() const
{
	if (CachedStageSubsystem)
	{
		return CachedStageSubsystem;
	}

	if (const UGameInstance* GI = GetGameInstance())
	{
		return GI->GetSubsystem<UFF_FestivalStageSubsystem>();
	}
	return nullptr;
}

void UFF_FestivalViewModelSubsystem::TryBindStageSubsystem()
{
	if (CachedStageSubsystem)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	UFF_FestivalStageSubsystem* StageSubsystem = GI->GetSubsystem<UFF_FestivalStageSubsystem>();
	if (!StageSubsystem)
	{
		++StageBindRetryCount;
		if (StageBindRetryCount == MaxStageBindRetry)
		{
			// 조용히 재시도만 반복하지 않는다. 상한에 닿으면 한 번 남긴다.
			UE_LOG(LogFestaFiestaUI, Error,
				TEXT("[ViewModel] UFF_FestivalStageSubsystem을 %d회 시도해도 찾지 못했다. Stage 표시가 동작하지 않는다."),
				MaxStageBindRetry);
		}
		return;
	}

	CachedStageSubsystem = StageSubsystem;

	// Bind + Sync — 구독하고 나서 현재값을 즉시 반영한다.
	// 이벤트는 앞으로의 변화만 알려주므로, 이미 지나간 현재 Stage를 놓치지 않으려면 필요하다.
	CachedStageSubsystem->OnStageChanged.AddUniqueDynamic(this, &UFF_FestivalViewModelSubsystem::HandleStageChanged);

	OnStageViewChanged.Broadcast(GetCurrentStage(), GetCurrentDay());

	UE_LOG(LogFestaFiestaUI, Log,
		TEXT("[ViewModel] Stage 시스템 구독 완료. 현재 Stage=%s Day=%d"),
		*GetCurrentStageDisplayText().ToString(), GetCurrentDay());
}

void UFF_FestivalViewModelSubsystem::HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage)
{
	OnStageViewChanged.Broadcast(NewStage, GetCurrentDay());
}

EFF_FestivalStage UFF_FestivalViewModelSubsystem::GetCurrentStage() const
{
	const UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();
	return StageSubsystem ? StageSubsystem->CurrentStage : EFF_FestivalStage::Setup;
}

int32 UFF_FestivalViewModelSubsystem::GetCurrentDay() const
{
	const UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();
	return StageSubsystem ? StageSubsystem->CurrentDay : 0;
}

float UFF_FestivalViewModelSubsystem::GetRemainingSeconds() const
{
	const UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();
	return StageSubsystem ? StageSubsystem->RemainingTime : 0.f;
}

FText UFF_FestivalViewModelSubsystem::GetCurrentStageDisplayText() const
{
	// enum 값을 하드코딩 매핑하지 않는다. UMETA(DisplayName)을 리플렉션으로 읽는다.
	// 기획 7단계와 코드 8단계가 아직 다르므로, 모르는 값이 와도 빈 문자열이 되면 안 된다.
	if (const UEnum* StageEnum = StaticEnum<EFF_FestivalStage>())
	{
		const int64 Value = static_cast<int64>(GetCurrentStage());
		if (StageEnum->IsValidEnumValue(Value))
		{
			return StageEnum->GetDisplayNameTextByValue(Value);
		}

		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[ViewModel] 알 수 없는 Stage 값 %lld. enum이 바뀌었을 수 있다."), Value);
		return FText::AsNumber(Value);
	}

	return FText::GetEmpty();
}

void UFF_FestivalViewModelSubsystem::ApplyIntMetric(int32& Current, int32 NewValue, FFF_OnIntMetricChanged& ChangedDelegate)
{
	if (Current == NewValue)
	{
		return;
	}

	Current = NewValue;
	ChangedDelegate.Broadcast(Current);
}

void UFF_FestivalViewModelSubsystem::Debug_SetMetrics(int32 InSimSatisfaction, int32 InScoreSatisfaction, int32 InSafety)
{
	// 0~100 지표는 범위를 벗어난 값이 들어와도 화면이 깨지지 않게 잘라낸다.
	// 실제 소유자 시스템이 붙어도 이 방어는 남겨둔다. 계산 실수가 UI까지 오면 원인 추적이 어렵다.
	const auto Clamp01_100 = [](int32 Value) { return FMath::Clamp(Value, 0, MetricMax); };

	ApplyIntMetric(SimSatisfaction, Clamp01_100(InSimSatisfaction), OnSimSatisfactionChanged);
	ApplyIntMetric(ScoreSatisfaction, Clamp01_100(InScoreSatisfaction), OnScoreSatisfactionChanged);
	ApplyIntMetric(Safety, Clamp01_100(InSafety), OnSafetyChanged);
}

void UFF_FestivalViewModelSubsystem::Debug_SetEconomy(int32 InGold, int32 InTotalRevenue, int32 InTodayRevenue)
{
	// 금액이라 0~100 상한을 두지 않는다. Gold는 음수가 될 수 있는지 미확정이라 클램프하지 않는다.
	ApplyIntMetric(Gold, InGold, OnGoldChanged);
	ApplyIntMetric(TotalRevenue, InTotalRevenue, OnTotalRevenueChanged);
	ApplyIntMetric(TodayRevenue, InTodayRevenue, OnTodayRevenueChanged);
}

void UFF_FestivalViewModelSubsystem::Debug_DumpToLog() const
{
	// 화면을 못 볼 때 값을 확인하는 유일한 경로다. 단위를 같이 적어 둔다.
	UE_LOG(LogFestaFiestaUI, Log,
		TEXT("[ViewModel] SimSat=%d/%d ScoreSat=%d/%d Safety=%d/%d | Gold=%d TotalRev=%d TodayRev=%d | Stage=%s Day=%d 남은=%.1f초"),
		SimSatisfaction, MetricMax,
		ScoreSatisfaction, MetricMax,
		Safety, MetricMax,
		Gold, TotalRevenue, TodayRevenue,
		*GetCurrentStageDisplayText().ToString(), GetCurrentDay(), GetRemainingSeconds());
}

void UFF_FestivalViewModelSubsystem::PollMetricsFromSources()
{
	// !! 이건 폴백이지 목표 설계가 아니다 !!
	//
	// 지표를 가져오는 방법은 둘이고 우선순위가 있다.
	//
	//   1순위 — 푸시. 소유자가 "값이 바뀌었다"고 델리게이트를 쏜다.
	//           그 델리게이트를 TryBind...에서 구독하고 이 함수에서는 손대지 않는다.
	//   2순위 — 풀(여기). 소유자가 Getter만 주고 델리게이트를 안 줄 때만 쓴다.
	//
	// UI_API.md 원칙 3이 "폴링하지 않는다"인 이유다. 델리게이트가 없어서 어쩔 수 없이 읽는 값만
	// 여기 둔다. 새 값을 추가할 때 습관적으로 여기 넣지 말고 소유자에게 델리게이트를 먼저 요청한다.
	// (선례: RemainingSeconds. 강민서 쪽에 초 단위 델리게이트가 없어 Tick에서 폴링 중이고
	//  UI_API.md에 요청으로 남겨 뒀다. 델리게이트가 생기면 폴링을 걷어낸다.)
	//
	// 어느 쪽이든 통지는 ApplyIntMetric을 거치므로 **위젯은 값이 실제로 바뀔 때만 갱신된다.**
	// 폴링 주기는 소스를 확인하는 간격이지 위젯 갱신 주기가 아니다.
	//
	// 아직 소유자 시스템이 하나도 없어 지금은 아무것도 하지 않는다. 생기면 이렇게 된다.
	//
	//   ApplyIntMetric(Gold, Progression->GetGold(), OnGoldChanged);   // 델리게이트가 없을 때만
	//
	// 소유자 현황은 리포 루트 UI_API.md의 "UI가 필요한데 아직 없는 것" 표에 있다.
}

void UFF_FestivalViewModelSubsystem::Tick(float DeltaTime)
{
	if (!CachedStageSubsystem)
	{
		TryBindStageSubsystem();
	}

	// 남은 시간은 매 틱 바뀐다. 그대로 델리게이트로 쏘면 위젯이 초당 수십 번 갱신된다.
	// 0.1초마다 확인하되 "초 단위 값이 바뀔 때만" 통지한다.
	PollAccumulator += DeltaTime;
	if (PollAccumulator < PollInterval)
	{
		return;
	}
	PollAccumulator = 0.f;

	const int32 RemainingSeconds = FMath::CeilToInt(GetRemainingSeconds());
	if (RemainingSeconds != LastBroadcastRemainingSeconds)
	{
		LastBroadcastRemainingSeconds = RemainingSeconds;
		OnRemainingSecondsChanged.Broadcast(RemainingSeconds);
	}

	// 지표도 같은 주기로 확인한다. 값이 안 바뀌면 아무것도 쏘지 않는다.
	PollMetricsFromSources();
}
