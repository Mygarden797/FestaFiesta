// Fill out your copyright notice in the Description page of Project Settings.


#include "Time/FF_FestivalStageSubsystem.h"


void UFF_FestivalStageSubsystem::RequestTransition(EFF_FestivalStage NewStage)
{
	EFF_FestivalStage PreviousStage = CurrentStage;
	CurrentStage = NewStage;

	if (NewStage == EFF_FestivalStage::Live)
	{
		ElapsedLiveSeconds = 0.f;
		// 날마다 길이가 달라, 시계를 0으로 리셋함
	}

	UE_LOG(LogTemp, Log, TEXT("[Stage] %s -> %s"),
		*UEnum::GetValueAsString(PreviousStage),
		*UEnum::GetValueAsString(NewStage));
	// Stage 바뀔 때마다 Output Log에 기록

	OnStageChanged.Broadcast(PreviousStage, NewStage);
	// 바뀌었다고 알림
}

bool UFF_FestivalStageSubsystem::IsReconfigureAllowed() const
{
	return CurrentStage == EFF_FestivalStage::Setup
		|| CurrentStage == EFF_FestivalStage::MorningCheck;
	// 이 세 단계일 때만 배치 변경 허용
}

void UFF_FestivalStageSubsystem::SetTimeScale(float NewScale)
{
	TimeScale = NewScale;
}
// 배속값을 그냥 저장하는 함수

const TArray<FFF_LiveTriggerEvent>& UFF_FestivalStageSubsystem::GetEventsForCurrentDay()
{
	CachedTodayEvents.Empty();

	if (!ProgramScheduleTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("ProgramScheduleTable이 비어있습니다!"));
		return CachedTodayEvents;
	}

	TArray<FFF_LiveTriggerEvent*> AllRows;
	ProgramScheduleTable->GetAllRows<FFF_LiveTriggerEvent>(TEXT("GetEventsForCurrentDay"), AllRows);

	for (const FFF_LiveTriggerEvent* Row : AllRows)
	{
		if (Row && Row->Day == CurrentDay)
		{
			CachedTodayEvents.Add(*Row);
		}
	}

	CachedTodayEvents.Sort([](const FFF_LiveTriggerEvent& A, const FFF_LiveTriggerEvent& B)
		{
			return A.TriggerSeconds < B.TriggerSeconds;
		});

	return CachedTodayEvents;

}
// CurrentDay(1/2/3)에 맞는 이벤트 배열을 골라서 돌려주는 함수

float UFF_FestivalStageSubsystem::GetLiveDurationForCurrentDay()
{
	const TArray<FFF_LiveTriggerEvent>& Events = GetEventsForCurrentDay();
	return Events.Num() > 0 ? Events.Last().TriggerSeconds : 0.f;
}
//그날 이벤트 중 마지막 시각 = 그날 Live 총 길이

bool UFF_FestivalStageSubsystem::IsMainProgramLocked()
{
	const TArray<FFF_LiveTriggerEvent>& Events = GetEventsForCurrentDay();

	for (int32 i = 0; i < Events.Num(); i++)
	{
		if (Events[i].bIsMainProgramLockStart)
		{
			const float LockStart = Events[i].TriggerSeconds - 10.f;
			const float LockEnd = Events.IsValidIndex(i + 1) ? Events[i + 1].TriggerSeconds : Events[i].TriggerSeconds;
			return ElapsedLiveSeconds >= LockStart && ElapsedLiveSeconds <= LockEnd;
		}
	}
	return false;
}
//그 시각 10초 전부터 다음 이벤트 시각까지가 배속 잠금 구간인지 판단


void UFF_FestivalStageSubsystem::Tick(float DeltaTime)
{
	if (CurrentStage != EFF_FestivalStage::Live)
	{
		return;
		// Live 아닐 땐 시간 안 흐르게 함
	}

	const float EffectiveScale = IsMainProgramLocked() ? 1.f : TimeScale;
	// 메인 공연 구간이면 배속을 강제로 1배속으로 고정

	ElapsedLiveSeconds += DeltaTime * EffectiveScale;
	// 흐른 시간을 누적하는 방식

	RemainingTime = GetLiveDurationForCurrentDay() - ElapsedLiveSeconds;
	//오늘 총 길이 - 흐른 시간 =  남은 시간을 매번 다시 계산

	const int32 CurrentSeconds = FMath::CeilToInt(RemainingTime);
	if (CurrentSeconds != LastBroadcastSeconds)
	{
		LastBroadcastSeconds = CurrentSeconds;
		OnRemainingSecondsChanged.Broadcast(CurrentSeconds);
	}

	if (RemainingTime <= 0.f)
	{
		HandleLiveEnd();
	}
}

void UFF_FestivalStageSubsystem::HandleLiveEnd()
{
	if (CurrentDay == 3)
	{
		RequestTransition(EFF_FestivalStage::Finale);
		// Day3만 Finale로 먼저 이동
	}
	else
	{
		RequestTransition(EFF_FestivalStage::Closing);
	}
}

void UFF_FestivalStageSubsystem::AdvanceToNextDay()
{
	if (CurrentStage != EFF_FestivalStage::Report)
	{
		UE_LOG(LogTemp, Warning, TEXT("AdvanceToNextDay called outside Report stage. Ignored."));
		return;
		// Report 단계가 아닐 때 잘못 호출되는 것 방지
	}

	CurrentDay++;
	RequestTransition(EFF_FestivalStage::MorningCheck);
}

void UFF_FestivalStageSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ProgramScheduleTable = LoadObject<UDataTable>(
		nullptr,
		TEXT("/Game/FestaFiesta/Data/Time/DT_ProgramSchedule.DT_ProgramSchedule")
	);
	// 날짜별 공연 이벤트 시간표 불러오기

	if (ProgramScheduleTable)
	{
		UE_LOG(LogTemp, Log, TEXT("ProgramScheduleTable 로드 성공!"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ProgramScheduleTable 로드 실패! 경로를 확인하세요."));
	}

	// 배속 설정 테이블 불러오기
	TimeSettingsTable = LoadObject<UDataTable>(
		nullptr,
		TEXT("/Game/FestaFiesta/Data/Time/DT_TimeSettings.DT_TimeSettings")
	);

	if (TimeSettingsTable)
	{
		UE_LOG(LogTemp, Log, TEXT("TimeSettingsTable 로드 성공!"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("TimeSettingsTable 로드 실패! 경로를 확인하세요."));
	}

	// 단계별 시간 범위 테이블 불러오기
	DaySequenceTable = LoadObject<UDataTable>(
		nullptr,
		TEXT("/Game/FestaFiesta/Data/Time/DT_DaySequence.DT_DaySequence")
	);

	if (DaySequenceTable)
	{
		UE_LOG(LogTemp, Log, TEXT("DaySequenceTable 로드 성공!"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("DaySequenceTable 로드 실패! 경로를 확인하세요."));
	}
}