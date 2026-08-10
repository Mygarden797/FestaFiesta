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
	float LeadSeconds = 10.f;
	// 표가 비어있을 때 기본값

	if (TimeSettingsTable)
		// 표가 실제로 연결되어 있는지 확인
	{
		TArray<FFF_TimeSettingRow*> AllSettings;
		// 표의 모든 행을 담을 임시 배열

		TimeSettingsTable->GetAllRows<FFF_TimeSettingRow>(TEXT("IsMainProgramLocked"), AllSettings);
		// 표에 있는 모든 행을 가져와서 위 배열에 담음

		if (AllSettings.Num() > 0)
			// 행이 하나라도 있으면
		{
			LeadSeconds = AllSettings[0]->MainProgramLockLeadSeconds;
			// 첫 번째 행의 리드타임 값을 실제로 가져다 씀
		}
	}

	const TArray<FFF_LiveTriggerEvent>& Events = GetEventsForCurrentDay();
	// 날짜에 맞는 이벤트 목록 가져오기

	for (int32 i = 0; i < Events.Num(); i++)
	{
		if (Events[i].bIsMainProgramLockStart)
			// 이 이벤트가 "메인 공연 시작"이면
		{
			const float LockStart = Events[i].TriggerSeconds - LeadSeconds;
			// 공연 시작 시각 -  표의 리드타임만큼 뺀 시점 = 락 시작 시점

			const float LockEnd = Events.IsValidIndex(i + 1) ? Events[i + 1].TriggerSeconds : Events[i].TriggerSeconds;
			// 다음 이벤트 시각 = 락 끝나는 시점

			return ElapsedLiveSeconds >= LockStart && ElapsedLiveSeconds <= LockEnd;
			// 지금이 그 구간 안에 있는지 true/false로 반환
		}
	}
	return false;
	// 메인 공연 이벤트 자체가 없으면 항상 false
}


void UFF_FestivalStageSubsystem::Tick(float DeltaTime)
{
	if (CurrentStage == EFF_FestivalStage::Live)
	{
		const float EffectiveScale = IsMainProgramLocked() ? 1.f : TimeScale;
		// 메인 공연 구간이면 배속을 강제로 1배속으로 고정

		ElapsedLiveSeconds += DeltaTime * EffectiveScale;
		// 흐른 시간을 누적하는 방식

		RemainingTime = GetLiveDurationForCurrentDay() - ElapsedLiveSeconds;
		// 오늘 총 길이 - 흐른 시간 = 남은 시간을 매번 다시 계산

		const int32 CurrentSeconds = FMath::CeilToInt(RemainingTime);
		if (CurrentSeconds != LastBroadcastSeconds)
		{
			LastBroadcastSeconds = CurrentSeconds;
			OnRemainingSecondsChanged.Broadcast(CurrentSeconds);
		}
		// 정수 초가 바뀌었을 때만 알림 보냄

		if (RemainingTime <= 0.f)
		{
			HandleLiveEnd();
		}
		return;
		// Live 처리 끝났으면 여기서 함수 종료
	}

	// Setup/Promotion/MorningCheck/Closing/Report 처리
	FFF_PhaseTimeRange Range = GetPhaseTimeRange(CurrentStage);
	// 지금 단계에 맞는 시간 범위를 표에서 찾아옴

	if (Range.MaxSeconds <= 0.f)
	{
		return;
		// 이 단계에 대한 시간 데이터가 표에 없으면 아무것도 안 함
	}

	ElapsedLiveSeconds += DeltaTime;
	// 흐른 시간 누적 (이 단계들은 배속 적용 안 함, 항상 1배속)

	RemainingTime = Range.MaxSeconds - ElapsedLiveSeconds;
	// 이 단계의 최대 시간 - 흐른 시간 = 남은 시간

	if (RemainingTime <= 0.f)
	{
		if (CurrentDay == 3 && CurrentStage == EFF_FestivalStage::Report)
		{
			UE_LOG(LogTemp, Log, TEXT("축제 종료!"));
			OnFestivalEnded.Broadcast();
			// 3일차 리포트가 끝나면 다음 단계로 안 넘어가고 종료 신호만 방송
		}
		else
		{
			RequestTransition(Range.NextStage);
			// 그 외에는 표에 적힌 다음 단계로 자동 전환
		}
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

FFF_PhaseTimeRange UFF_FestivalStageSubsystem::GetPhaseTimeRange(EFF_FestivalStage Stage)
{
	if (DaySequenceTable)
		// 표가 실제로 연결되어 있는지 확인
	{
		TArray<FFF_PhaseTimeRange*> AllRows;
		// 표의 모든 행을 담을 임시 배열
		DaySequenceTable->GetAllRows<FFF_PhaseTimeRange>(TEXT("GetPhaseTimeRange"), AllRows);
		// 표에 있는 모든 행을 가져옴
		for (const FFF_PhaseTimeRange* Row : AllRows)
		{
			if (Row && Row->Day == CurrentDay && Row->Stage == Stage)
				// 오늘 날짜 + 찾는 Stage가 일치하면
			{
				return *Row;
				// 그 행을 그대로 반환
			}
		}
	}
	return FFF_PhaseTimeRange();
	// 못 찾으면 빈 기본값 반환
}

bool UFF_FestivalStageSubsystem::IsCurrentStageSkippable()
{
	FFF_PhaseTimeRange Range = GetPhaseTimeRange(CurrentStage);
	// 지금 Stage의 시간 정보를 가져와서
	return Range.bSkippable;
	// 그 안의 스킵 가능 여부만 꺼내서 반환
}