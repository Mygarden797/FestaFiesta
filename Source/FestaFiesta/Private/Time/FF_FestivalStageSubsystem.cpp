// Fill out your copyright notice in the Description page of Project Settings.


#include "Time/FF_FestivalStageSubsystem.h"


void UFF_FestivalStageSubsystem::RequestTransition(EFF_FestivalStage NewStage)
{
	EFF_FestivalStage PreviousStage = CurrentStage;
	CurrentStage = NewStage;
	ElapsedLiveSeconds = 0.f;
		// 날마다 길이가 달라, 시계를 0으로 리셋함
	bWasMainProgramLocked = false;
	// 날짜 바뀔 때 공연 시작 체크도 초기화
	bNightLightingActivated = false;
	// 단계/날짜 바뀔 때 밤 조명 트리거도 다시 켤 수 있게 초기화
	LastBroadcastSeconds = -1;
	// 단계 바뀔 때마다 리셋해서 새 단계 첫 초도 반드시 방송되게 함
	

	UE_LOG(LogTemp, Log, TEXT("[Stage] %s -> %s"),
		*UEnum::GetValueAsString(PreviousStage),
		*UEnum::GetValueAsString(NewStage));
	// Stage 바뀔 때마다 Output Log에 기록

	OnStageChanged.Broadcast(PreviousStage, NewStage);
	// 바뀌었다고 알림

	// Live에 진입하는 순간, 오늘 확정된 날씨를 다른 시스템에 알림
	if (NewStage == EFF_FestivalStage::Live)
	{
		OnEnvironmentStateChanged.Broadcast(SelectedWeather, bNightLightingActivated);
		// 이 시점엔 아직 밤이 아니니 bIsNight는 false로 나감 
	}
}


bool UFF_FestivalStageSubsystem::IsReconfigureAllowed() const
{
	return CurrentStage == EFF_FestivalStage::Setup
		|| CurrentStage == EFF_FestivalStage::PromotionPreview
		|| CurrentStage == EFF_FestivalStage::MorningCheck;
	// 이 세 단계일 때만 배치 변경 허용
}

void UFF_FestivalStageSubsystem::SetTimeScale(float NewScale)
{
	TimeScale = NewScale;
}
// 배속값을 저장하는 함수

void UFF_FestivalStageSubsystem::SetSelectedWeather(EFF_WeatherType NewWeather)
{
	if (!IsReconfigureAllowed())
		// Setup / PromotionPreview / MorningCheck가 아니면
	{
		UE_LOG(LogTemp, Warning, TEXT("지금은 날씨를 바꿀 수 없는 단계입니다."));
		return;
		// 아무것도 안 하고 함수 종료
	}

	SelectedWeather = NewWeather;
	// 선택한 날씨를 저장

	UE_LOG(LogTemp, Log, TEXT("날씨 선택됨: %s"), *UEnum::GetValueAsString(SelectedWeather));
	
	OnEnvironmentStateChanged.Broadcast(SelectedWeather, bNightLightingActivated);
	// Setup/MorningCheck에서 고른 순간 바로 화면에 반영되게 함
}

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
// CurrentDay(1/2/3)에 맞는 이벤트 배열을 골라주는 함수

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

			const float LockEnd = Events.IsValidIndex(i + 1)
				? Events[i + 1].TriggerSeconds
				: GetLiveDurationForCurrentDay();
			// 다음 이벤트가 있으면 그 시각, 없으면 그날 Live 전체 끝까지를 락 구간으로 봄

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
		// Live 단계일 때 처리
	{

		const bool bIsLockedNow = IsMainProgramLocked();
		// 지금 메인 공연 락(1배속만가능한) 구간인지 확인

		static bool bLastLoggedState = false;
		if (bIsLockedNow != bLastLoggedState)
		{
			UE_LOG(LogTemp, Log, TEXT("락 상태 변화: %s, ElapsedLiveSeconds=%f"), bIsLockedNow ? TEXT("true") : TEXT("false"), ElapsedLiveSeconds);
			bLastLoggedState = bIsLockedNow;
		}

		const TArray<FFF_LiveTriggerEvent>& TodayEvents = GetEventsForCurrentDay();
		// 오늘 이벤트 목록 가져옴

		if (bIsLockedNow && !bWasMainProgramLocked)
			
		{
			UE_LOG(LogTemp, Log, TEXT("공연 시작 감지! ElapsedLiveSeconds: %f"), ElapsedLiveSeconds);
			// 디버그용

			for (int32 i = 0; i < TodayEvents.Num(); i++)
				// 이벤트들 중에서 
			{
				if (TodayEvents[i].bIsMainProgramLockStart)
					// 공연 시작 이벤트를 찾으면
				{
					OnMainProgramStarted.Broadcast(TodayEvents[i].EventLabel);
					// 시작 라벨과 함께 알림 보냄

					CachedMainProgramEndLabel = TodayEvents.IsValidIndex(i + 1) ? TodayEvents[i + 1].EventLabel : TEXT("");
					// 바로 다음 이벤트를 종료 라벨로 저장해둠

					break;
					// 찾았으니 반복 종료
				}
			}
		}
		else if (!bIsLockedNow && bWasMainProgramLocked)
			// 방금 막 잠금 구간에서 벗어난 순간이면
		{
			UE_LOG(LogTemp, Log, TEXT("공연 종료 감지! 라벨: %s"), *CachedMainProgramEndLabel);
			// 디버그용 - 실제로 이 코드가 실행되는지 확인용 로그
			OnMainProgramEnded.Broadcast(CachedMainProgramEndLabel);
			// 저장해둔 종료 라벨로 알림 보냄
		}

		bWasMainProgramLocked = bIsLockedNow;
		// 다음 프레임 비교용으로 저장

		const float EffectiveScale = bIsLockedNow ? 1.f : TimeScale;
		// 메인 공연 구간이면 배속을 1배속으로 고정

		ElapsedLiveSeconds += DeltaTime * EffectiveScale;
		// 흐른 시간을 누적하는 방식


		float EffectiveNightLightingTrigger = NightLightingTriggerSeconds;
		// 일단 기본값으로 시작

		if (TimeSettingsTable)
			// 표가 실제로 연결되어 있으면
		{
			TArray<FFF_TimeSettingRow*> AllSettings;
			TimeSettingsTable->GetAllRows<FFF_TimeSettingRow>(TEXT("Tick_NightLighting"), AllSettings);

			if (AllSettings.Num() > 0)
				// 행이 하나라도 있으면
			{
				EffectiveNightLightingTrigger = AllSettings[0]->NightLightingTriggerSeconds;
				// 표에 있는 값으로 덮어쓰기
			}
		}

		// 밤 조명 켤 시간이 됐는지 매 프레임 확인
		if (!bNightLightingActivated && ElapsedLiveSeconds >= EffectiveNightLightingTrigger)
		{
			bNightLightingActivated = true;
			// 다시 안 켜지게 표시 

			UE_LOG(LogTemp, Log, TEXT("밤 조명 활성화! ElapsedLiveSeconds=%f"), ElapsedLiveSeconds);
			

			OnNightLightingActivated.Broadcast();
			//  Light Manager에게 신호 보냄

			
			OnEnvironmentStateChanged.Broadcast(SelectedWeather, bNightLightingActivated);
		}
		RemainingTime = FMath::Max(GetLiveDurationForCurrentDay() - ElapsedLiveSeconds, 0.f);
		// 오늘 총 길이 - 흐른 시간 = 남은 시간을 매번 다시 계산
	}
	else
		//(Setup/Promotion/MorningCheck/Closing/Report) 처리
	{
		FFF_PhaseTimeRange Range = GetPhaseTimeRange(CurrentStage);
		// 지금 단계에 맞는 시간 범위를 표에서 찾아옴

		if (Range.MaxSeconds <= 0.f)
		{
			return;
			// 이 단계에 대한 시간 데이터가 표에 없으면 아무것도 안 함
		}

		ElapsedLiveSeconds += DeltaTime;
		// 흐른 시간 누적 (항상 1배속)

		RemainingTime = FMath::Max(Range.MaxSeconds - ElapsedLiveSeconds, 0.f);
		// 이 단계의 최대 시간 - 흐른 시간 = 남은 시간 
	}

	const int32 CurrentSeconds = FMath::CeilToInt(RemainingTime);
	
	if (CurrentSeconds != LastBroadcastSeconds)
	{
		LastBroadcastSeconds = CurrentSeconds;
		OnRemainingSecondsChanged.Broadcast(CurrentSeconds);
	}
	// 정수 초가 바뀌었을 때만 알림 보냄 (이제 모든 단계에서 작동)

	if (RemainingTime <= 0.f)
	{
		if (CurrentStage == EFF_FestivalStage::Live)
			// Live면 HandleLiveEnd 호출
		{
			HandleLiveEnd();
		}
		else if (CurrentStage == EFF_FestivalStage::Report)
			// 시간이 다 돼도 자동 전환 안 함
		{
			if (CurrentDay == 3 && !bFestivalEndedBroadcasted)
				// 한 번만 알림 가도록 설정
			{
				bFestivalEndedBroadcasted = true;
				UE_LOG(LogTemp, Log, TEXT("축제 종료!"));
				OnFestivalEnded.Broadcast();
			}
			// Day1/2 Report는 여기서 아무것도 안 함 - 대기
		}
		else
		{
			FFF_PhaseTimeRange NextRange = GetPhaseTimeRange(CurrentStage);
			
			RequestTransition(NextRange.NextStage);
			// 그 외는 표에 적힌대로 실행
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

	if (CurrentDay >= 3)
	{
		UE_LOG(LogTemp, Warning, TEXT("이미 마지막 날(Day3)입니다. AdvanceToNextDay 무시됨."));
		return;
	}

	const float TodayLiveDuration = GetLiveDurationForCurrentDay();
	// 오늘 Live 총 길이를 미리 구해둠 (CurrentDay가 바뀌기 전에 구해야 오늘 값이 맞음)

	UE_LOG(LogTemp, Log, TEXT("Day %d 종료! TodayLiveDuration=%f"), CurrentDay, TodayLiveDuration);


	OnDayEnded.Broadcast(CurrentDay, TodayLiveDuration);
	// 며칠째가 끝났고, 그날 Live가 몇 초였는지 신호로 전달

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

void UFF_FestivalStageSubsystem::SkipCurrentStage()
{
	if (!IsCurrentStageSkippable())
	{
		UE_LOG(LogTemp, Warning, TEXT("이 단계는 스킵할 수 없습니다."));
		return;
	}
	FFF_PhaseTimeRange Range = GetPhaseTimeRange(CurrentStage);
	RequestTransition(Range.NextStage);
	// 스킵 가능하면 시간 안 기다리고 바로 다음 단계로
}



FText UFF_FestivalStageSubsystem::GetCurrentStageDisplayName() const
{
	UEnum* EnumPtr = StaticEnum<EFF_FestivalStage>();
	if (EnumPtr)
	{
		return EnumPtr->GetDisplayNameTextByValue((int64)CurrentStage);
	}
	return FText::FromString(TEXT("알 수 없음"));
}
// enum에 붙여둔 UMETA(DisplayName)을 그대로 가져와서 반환

FString UFF_FestivalStageSubsystem::GetCurrentLiveClockString() const
{
	const int32 StartHour = 17;
	// Live 시작 시각(오후 5시). todo: 나중에 표로 빼기

	const int32 TotalMinutes = StartHour * 60 + FMath::FloorToInt(ElapsedLiveSeconds);
	const int32 Hour = (TotalMinutes / 60) % 24;
	const int32 Minute = TotalMinutes % 60;

	return FString::Printf(TEXT("%02d:%02d"), Hour, Minute);
}
// ElapsedLiveSeconds(경과 시간)를 그대로 "게임 속 분"으로 취급해서
// 17:00에 더한 뒤 시:분 형식 문자열로 변환

bool UFF_FestivalStageSubsystem::IsLiveStage() const
{
	return CurrentStage == EFF_FestivalStage::Live;
}

bool UFF_FestivalStageSubsystem::IsReportStage() const
{
	return CurrentStage == EFF_FestivalStage::Report;
}

bool UFF_FestivalStageSubsystem::IsFinalDay() const
{
	return CurrentDay == 3;
}