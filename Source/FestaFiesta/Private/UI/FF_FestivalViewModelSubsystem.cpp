// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/FF_FestivalViewModelSubsystem.h"

#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/FF_UILog.h"

void UFF_FestivalViewModelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 같은 GameInstance 안의 서브시스템이라 순서가 보장되지 않는다.
	// 지금 없으면 Tick에서 다시 시도한다.
	TryBindStageSubsystem();
	TryBindProgressionSubsystem();

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
		CachedStageSubsystem->OnRemainingSecondsChanged.RemoveDynamic(this, &UFF_FestivalViewModelSubsystem::HandleRemainingSecondsChanged);
	}
	CachedStageSubsystem = nullptr;

	if (CachedProgressionSubsystem)
	{
		CachedProgressionSubsystem->OnGoldChanged.RemoveDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerGoldChanged);
	}
	CachedProgressionSubsystem = nullptr;

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
	CachedStageSubsystem->OnRemainingSecondsChanged.AddUniqueDynamic(this, &UFF_FestivalViewModelSubsystem::HandleRemainingSecondsChanged);

	OnStageViewChanged.Broadcast(GetCurrentStage(), GetCurrentDay());
	BroadcastRemainingSecondsIfChanged(FMath::CeilToInt(GetRemainingSeconds()));

	UE_LOG(LogFestaFiestaUI, Log,
		TEXT("[ViewModel] Stage 시스템 구독 완료. 현재 Stage=%s Day=%d 남은=%.1f초"),
		*GetCurrentStageDisplayText().ToString(), GetCurrentDay(), GetRemainingSeconds());
}

UFF_FestivalProgressionSubsystem* UFF_FestivalViewModelSubsystem::GetProgressionSubsystem() const
{
	if (CachedProgressionSubsystem)
	{
		return CachedProgressionSubsystem;
	}

	if (const UGameInstance* GI = GetGameInstance())
	{
		return GI->GetSubsystem<UFF_FestivalProgressionSubsystem>();
	}
	return nullptr;
}

void UFF_FestivalViewModelSubsystem::TryBindProgressionSubsystem()
{
	if (CachedProgressionSubsystem)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	UFF_FestivalProgressionSubsystem* Progression = GI->GetSubsystem<UFF_FestivalProgressionSubsystem>();
	if (!Progression)
	{
		++ProgressionBindRetryCount;
		if (ProgressionBindRetryCount == MaxStageBindRetry)
		{
			UE_LOG(LogFestaFiestaUI, Error,
				TEXT("[ViewModel] UFF_FestivalProgressionSubsystem을 %d회 시도해도 찾지 못했다. Gold가 더미로 남는다."),
				MaxStageBindRetry);
		}
		return;
	}

	CachedProgressionSubsystem = Progression;

	// Bind + Sync. StartFestival이 이미 지나갔다면 방송을 놓쳤으므로 현재값을 직접 읽는다.
	CachedProgressionSubsystem->OnGoldChanged.AddUniqueDynamic(this, &UFF_FestivalViewModelSubsystem::HandleOwnerGoldChanged);

	// !! 축제가 시작되기 전에는 소유자 값을 쓰지 않는다 !!
	// StartFestival 전 Funds는 0이다. 그 0을 화면에 띄우면 "잔액이 없다"는 틀린 사실이 되고,
	// 검증 중에는 HUD가 고장난 것처럼 보인다. 시작되면 OnGoldChanged가 실제 값을 가져온다.
	if (CachedProgressionSubsystem->IsFestivalStarted())
	{
		HandleOwnerGoldChanged(CachedProgressionSubsystem->GetGold());
	}
	else
	{
		UE_LOG(LogFestaFiestaUI, Log,
			TEXT("[ViewModel] 경제 시스템 구독 완료. 축제가 아직 시작되지 않아 Gold는 더미 %d를 유지한다."), Gold);
	}
}

void UFF_FestivalViewModelSubsystem::HandleOwnerGoldChanged(int32 NewValue)
{
	// 소유자 값이 들어오는 순간부터 더미는 의미가 없다. 변화 감지는 ApplyIntMetric이 한다.
	ApplyIntMetric(Gold, NewValue, OnGoldChanged);
}

bool UFF_FestivalViewModelSubsystem::CanAfford(int32 Amount) const
{
	const UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	if (!Progression)
	{
		// 소유자가 없으면 "살 수 있다"고 알리지 않는다. 잘못 눌러 실패하는 쪽이 더 나쁘다.
		return false;
	}

	return Progression->CanAfford(Amount);
}

void UFF_FestivalViewModelSubsystem::RequestSetTimeScale(float NewScale)
{
	UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();
	if (!StageSubsystem)
	{
		UE_LOG(LogFestaFiestaUI, Warning, TEXT("[ViewModel] Stage 시스템이 없어 배속을 바꿀 수 없다."));
		return;
	}

	// 잠금 구간이어도 요청은 보낸다. 무시 판정의 소유자는 Stage 시스템이다.
	// UI가 미리 막으면 잠금 규칙이 두 곳에 생긴다.
	StageSubsystem->SetTimeScale(NewScale);
}

float UFF_FestivalViewModelSubsystem::GetTimeScale() const
{
	const UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();
	return StageSubsystem ? StageSubsystem->TimeScale : 1.f;
}

bool UFF_FestivalViewModelSubsystem::IsTimeScaleLocked() const
{
	UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();

	// IsMainProgramLocked()가 const가 아니라 비const 포인터가 필요하다.
	return StageSubsystem ? StageSubsystem->IsMainProgramLocked() : false;
}

FText UFF_FestivalViewModelSubsystem::GetLiveClockText() const
{
	const UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();
	if (!StageSubsystem)
	{
		return FText::GetEmpty();
	}

	// Stage 쪽이 FString으로 준다. 화면 문구는 FText로만 다룬다.
	// 시각 표기(19:42)는 숫자 서식이라 현지화 대상이 아니므로 FromString으로 감싸도 손실이 없다.
	return FText::FromString(StageSubsystem->GetCurrentLiveClockString());
}

void UFF_FestivalViewModelSubsystem::HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage)
{
	OnStageViewChanged.Broadcast(NewStage, GetCurrentDay());

	// Stage가 바뀌면 남은 시간도 새 단계 기준으로 다시 시작한다.
	// Stage 쪽은 초가 "바뀔 때만" 쏘므로, 전환 직후 값이 우연히 같으면 방송이 오지 않는다.
	// 그래서 전환 시점에 한 번 직접 맞춘다 — Bind + Sync를 전환마다 다시 하는 셈이다.
	BroadcastRemainingSecondsIfChanged(FMath::CeilToInt(GetRemainingSeconds()));
}

void UFF_FestivalViewModelSubsystem::HandleRemainingSecondsChanged(int32 NewSeconds)
{
	BroadcastRemainingSecondsIfChanged(NewSeconds);
}

void UFF_FestivalViewModelSubsystem::BroadcastRemainingSecondsIfChanged(int32 NewSeconds)
{
	if (NewSeconds == LastBroadcastRemainingSeconds)
	{
		return;
	}

	LastBroadcastRemainingSeconds = NewSeconds;
	OnRemainingSecondsChanged.Broadcast(NewSeconds);
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
	// (선례: RemainingSeconds. 2026-08-06에 요청 → 2026-08-11에 강민서가 모든 Stage를 커버하는
	//  OnRemainingSecondsChanged를 제공 → 구독으로 바꾸고 폴링을 걷어냈다.
	//  요청하면 받을 수 있다는 뜻이다. 폴링을 기본값처럼 쓰지 않는다.)
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

	if (!CachedProgressionSubsystem)
	{
		TryBindProgressionSubsystem();
	}

	// 남은 시간은 여기서 폴링하지 않는다.
	// 2026-08-11 강민서 쪽에 모든 Stage를 커버하는 OnRemainingSecondsChanged가 생겨서
	// 구독으로 바꿨다(TryBindStageSubsystem). 폴링을 되살리지 않는다 — 중복 방송이 된다.

	// 지표는 아직 소유자 델리게이트가 없어 주기 확인이 남아 있다.
	// 값이 안 바뀌면 아무것도 쏘지 않는다.
	PollAccumulator += DeltaTime;
	if (PollAccumulator < PollInterval)
	{
		return;
	}
	PollAccumulator = 0.f;

	PollMetricsFromSources();
}

void UFF_FestivalViewModelSubsystem::RequestAdvanceToNextDay()
{
	if (!CanAdvanceToNextDay())
	{
		// 버튼이 잘못 활성화된 경우다. 조용히 넘기지 않고 원인을 남긴다.
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[ViewModel] 다음 날 요청이 거부됐다. Stage=%s Day=%d (Report 단계이고 마지막 날이 아닐 때만 가능)"),
			*GetCurrentStageDisplayText().ToString(), GetCurrentDay());
		return;
	}

	UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();
	StageSubsystem->AdvanceToNextDay();

	// 여기서 화면을 직접 갱신하지 않는다. 결과는 OnStageChanged → OnStageViewChanged로 돌아온다.
	// 낙관적 갱신 금지 (UI 규칙 2).
}

void UFF_FestivalViewModelSubsystem::RequestSkipCurrentStage()
{
	UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();
	if (!StageSubsystem)
	{
		UE_LOG(LogFestaFiestaUI, Warning, TEXT("[ViewModel] Stage 시스템이 없어 건너뛰기를 요청할 수 없다."));
		return;
	}

	// 스킵 가능 여부의 최종 판정은 Stage 시스템이 한다(DaySequence 표의 bSkippable).
	// 여기서 미리 막지 않는다 — CanSkipCurrentStage()가 근사값이라 잘못 막을 수 있다.
	StageSubsystem->SkipCurrentStage();
}

bool UFF_FestivalViewModelSubsystem::CanAdvanceToNextDay() const
{
	const UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();
	if (!StageSubsystem)
	{
		return false;
	}

	// 총 일수를 UI에 하드코딩하지 않는다. IsFinalDay()가 그 판단을 소유한다.
	return StageSubsystem->IsReportStage() && !StageSubsystem->IsFinalDay();
}

bool UFF_FestivalViewModelSubsystem::IsFinalDay() const
{
	const UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();
	return StageSubsystem && StageSubsystem->IsFinalDay();
}

bool UFF_FestivalViewModelSubsystem::IsReportStage() const
{
	const UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();
	return StageSubsystem && StageSubsystem->IsReportStage();
}

bool UFF_FestivalViewModelSubsystem::CanSkipCurrentStage() const
{
	const UFF_FestivalStageSubsystem* StageSubsystem = GetStageSubsystem();
	if (!StageSubsystem)
	{
		return false;
	}

	// !! 근사값 !! IsCurrentStageSkippable()이 private이라 C++에서 못 부른다.
	// Live는 배속으로 넘기고 Report는 다음 날 버튼이 담당하므로 둘은 제외한다.
	// 실제 표에서 bSkippable=false인 단계도 true로 보일 수 있다 — 그때는 Stage 시스템이 무시하고 경고만 남긴다.
	// 헤더 주석의 교체 지침을 따른다.
	return !StageSubsystem->IsLiveStage() && !StageSubsystem->IsReportStage();
}
