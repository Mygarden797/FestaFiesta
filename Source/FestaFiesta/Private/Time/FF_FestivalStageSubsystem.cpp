// Fill out your copyright notice in the Description page of Project Settings.


#include "Time/FF_FestivalStageSubsystem.h"

#include "Save/FF_SaveArchive.h"
#include "Save/FF_SaveSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Festival/FF_RequiredFacilitiesSubsystem.h"


void UFF_FestivalStageSubsystem::RequestTransition(EFF_FestivalStage NewStage)
{
	FText BlockingReason;
	if (!CanTransitionTo(NewStage, BlockingReason))
	{
		// 자동 시간 만료도 같은 경계를 통과하므로 매 Tick 경고를 반복하면 로그가 넘친다.
		// 막힌 사유가 바뀔 때만 한 번 남긴다.
		const FString ReasonText = BlockingReason.IsEmpty()
			? TEXT("(사유 미제공)") : BlockingReason.ToString();
		if (LastBlockedTransitionReason != ReasonText || LastBlockedTransitionStage != NewStage)
		{
			LastBlockedTransitionReason = ReasonText;
			LastBlockedTransitionStage = NewStage;

			UE_LOG(LogTemp, Log,
				TEXT("[축제 단계] %d -> %d 전환이 막혔다: %s"),
				static_cast<int32>(CurrentStage), static_cast<int32>(NewStage), *ReasonText);

			// 디버그(매일 재검사 원인 조사)
			if (const UWorld* World = GetWorld())
			{
				if (const auto* Required = World->GetSubsystem<UFF_RequiredFacilitiesSubsystem>())
				{
					UE_LOG(LogTemp, Log, TEXT("[축제 단계] 필수 시설 실제 상태: %s"),
						*Required->DebugDescribeRequiredFacilities());
				}
			}
		}
		return;
	}

	// 통과했으니 다음 차단은 다시 한 번 찍혀야 한다.
	LastBlockedTransitionReason.Reset();
	LastBlockedTransitionStage = CurrentStage;
	EFF_FestivalStage PreviousStage = CurrentStage;
	CurrentStage = NewStage;
	ElapsedLiveSeconds = 0.f;
	// 날마다 길이가 달라, 시계를 0으로 리셋함
	bWasMainProgramLocked = false;
	bLastLoggedMainProgramLockState = false;
	ActiveMainProgramEventIndex = INDEX_NONE;
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


bool UFF_FestivalStageSubsystem::CanTransitionTo(EFF_FestivalStage NewStage, FText& OutBlockingReason) const
{
	OutBlockingReason = FText::GetEmpty();
	const bool bNextAllowsBuilding = NewStage == EFF_FestivalStage::Setup
		|| NewStage == EFF_FestivalStage::PromotionPreview || NewStage == EFF_FestivalStage::MorningCheck;
	const bool bRequiresFacilities = NewStage != CurrentStage &&
		(CurrentStage == EFF_FestivalStage::Setup || NewStage == EFF_FestivalStage::Live
			|| (IsReconfigureAllowed() && !bNextAllowsBuilding));
	if (!bRequiresFacilities) { return true; }

	const UGameInstance* GI = GetGameInstance();
	const auto* Progression = GI ? GI->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
	// 축제 정의가 없는 독립 시간 테스트/프리뷰의 기존 흐름은 유지한다.
	if (!Progression || !Progression->GetDefinition()) { return true; }
	const UWorld* World = GetWorld();
	const auto* Required = World ? World->GetSubsystem<UFF_RequiredFacilitiesSubsystem>() : nullptr;
	const FFF_RequiredFacilitiesStatus Status = Required ? Required->GetRequiredFacilitiesStatus()
		: UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(Progression->GetDefinition(), FFFZSReadModel(), false);
	OutBlockingReason = Status.BlockingReason;
	return Status.bCanProceed;
}

FText UFF_FestivalStageSubsystem::GetStageAdvanceBlockingReason()
{
	FText Reason;
	CanTransitionTo(GetPhaseTimeRange(CurrentStage).NextStage, Reason);
	return Reason;
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
	OnTimeScaleChange.Broadcast(GetEffectiveTimeScale());
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

			if (ElapsedLiveSeconds >= LockStart && ElapsedLiveSeconds <= LockEnd)
			{
				return true;
			}
		}
	}
	return false;
	// 메인 공연 이벤트 자체가 없으면 항상 false
}


bool UFF_FestivalStageSubsystem::TryGetMainProgramQueueBreakWindow(
	float& OutStartSeconds, float& OutEndSeconds) const
{
	if (CurrentStage != EFF_FestivalStage::Live || !ProgramScheduleTable)
	{
		return false;
	}
	TArray<FFF_LiveTriggerEvent*> Rows;
	ProgramScheduleTable->GetAllRows<FFF_LiveTriggerEvent>(TEXT("MainProgramQueueBreak"), Rows);
	Rows.RemoveAll([this](const FFF_LiveTriggerEvent* Row) { return !Row || Row->Day != CurrentDay; });
	Rows.Sort([](const FFF_LiveTriggerEvent& A, const FFF_LiveTriggerEvent& B)
	{
		return A.TriggerSeconds < B.TriggerSeconds;
	});
	for (int32 Index = 0; Index + 1 < Rows.Num(); ++Index)
	{
		if (!Rows[Index]->bIsMainProgramLockStart) continue;
		const float StartSeconds = Rows[Index]->TriggerSeconds;
		const float EndSeconds = Rows[Index + 1]->TriggerSeconds;
		const float MinutesUntilStart = StageSecondsToFestivalMinutes(StartSeconds - ElapsedLiveSeconds);
		if (EndSeconds > StartSeconds && MinutesUntilStart <= MainProgramQueueBreakLeadMinutes
			&& ElapsedLiveSeconds < EndSeconds)
		{
			OutStartSeconds = StartSeconds;
			OutEndSeconds = EndSeconds;
			return true;
		}
	}
	return false;
}

bool UFF_FestivalStageSubsystem::IsMainProgramQueueBreakActive() const
{
	float StartSeconds = 0.f;
	float EndSeconds = 0.f;
	return TryGetMainProgramQueueBreakWindow(StartSeconds, EndSeconds);
}

bool UFF_FestivalStageSubsystem::TryGetNextMainProgramEndSeconds(float& OutSeconds)
{
	const TArray<FFF_LiveTriggerEvent>& Events = GetEventsForCurrentDay();
	for (int32 i = 0; i < Events.Num(); i++)
	{
		if (!Events[i].bIsMainProgramLockStart)
		{
			continue;
		}
		const float ShowEnd = Events.IsValidIndex(i + 1)
			? Events[i + 1].TriggerSeconds
			: GetLiveDurationForCurrentDay();
		if (ElapsedLiveSeconds <= ShowEnd)
		{
			OutSeconds = ShowEnd;
			return true;
		}
	}
	return false;
}

bool UFF_FestivalStageSubsystem::TryGetNextMainProgramStartSeconds(float& OutSeconds)
{
	const TArray<FFF_LiveTriggerEvent>& Events = GetEventsForCurrentDay();
	for (int32 i = 0; i < Events.Num(); i++)
	{
		if (!Events[i].bIsMainProgramLockStart)
		{
			continue;
		}
		const float ShowEnd = Events.IsValidIndex(i + 1)
			? Events[i + 1].TriggerSeconds
			: GetLiveDurationForCurrentDay();
		if (ElapsedLiveSeconds <= ShowEnd)
		{
			OutSeconds = Events[i].TriggerSeconds;
			return true;
		}
	}
	return false;
}

bool UFF_FestivalStageSubsystem::TryGetActiveMainProgramStartSeconds(float& OutSeconds)
{
	float LeadSeconds = 10.f;
	if (TimeSettingsTable)
	{
		TArray<FFF_TimeSettingRow*> AllSettings;
		TimeSettingsTable->GetAllRows<FFF_TimeSettingRow>(TEXT("TryGetActiveMainProgramStartSeconds"), AllSettings);
		if (!AllSettings.IsEmpty())
		{
			LeadSeconds = AllSettings[0]->MainProgramLockLeadSeconds;
		}
	}

	const TArray<FFF_LiveTriggerEvent>& Events = GetEventsForCurrentDay();
	for (int32 i = 0; i < Events.Num(); i++)
	{
		if (!Events[i].bIsMainProgramLockStart)
		{
			continue;
		}
		const float LockStart = Events[i].TriggerSeconds - LeadSeconds;
		const float LockEnd = Events.IsValidIndex(i + 1)
			? Events[i + 1].TriggerSeconds
			: GetLiveDurationForCurrentDay();
		if (ElapsedLiveSeconds >= LockStart && ElapsedLiveSeconds <= LockEnd)
		{
			OutSeconds = Events[i].TriggerSeconds;
			return true;
		}
	}
	return false;
}

void UFF_FestivalStageSubsystem::Tick(float DeltaTime)
{
	if (!bFestivalStarted) { return; }

	if (CurrentStage == EFF_FestivalStage::Live)
		// Live 단계일 때 처리
	{

		const bool bIsLockedNow = IsMainProgramLocked();
		// 지금 메인 공연 락(1배속만가능한) 구간인지 확인

		if (bIsLockedNow != bLastLoggedMainProgramLockState)
		{
			UE_LOG(LogTemp, Log, TEXT("락 상태 변화: %s, ElapsedLiveSeconds=%f"), bIsLockedNow ? TEXT("true") : TEXT("false"), ElapsedLiveSeconds);
			bLastLoggedMainProgramLockState = bIsLockedNow;
		}

		const TArray<FFF_LiveTriggerEvent>& TodayEvents = GetEventsForCurrentDay();
		// 오늘 이벤트 목록 가져옴

		float LeadSeconds = 10.f;
		if (TimeSettingsTable)
		{
			TArray<FFF_TimeSettingRow*> Settings;
			TimeSettingsTable->GetAllRows<FFF_TimeSettingRow>(TEXT("Tick_MainProgram"), Settings);
			if (!Settings.IsEmpty()) LeadSeconds = Settings[0]->MainProgramLockLeadSeconds;
		}
		int32 NewActiveEventIndex = INDEX_NONE;
		for (int32 EventIndex = 0; EventIndex < TodayEvents.Num(); ++EventIndex)
		{
			if (!TodayEvents[EventIndex].bIsMainProgramLockStart) continue;
			const float LockStart = TodayEvents[EventIndex].TriggerSeconds - LeadSeconds;
			const float LockEnd = TodayEvents.IsValidIndex(EventIndex + 1)
				? TodayEvents[EventIndex + 1].TriggerSeconds
				: GetLiveDurationForCurrentDay();
			if (ElapsedLiveSeconds >= LockStart && ElapsedLiveSeconds <= LockEnd)
			{
				NewActiveEventIndex = EventIndex;
				break;
			}
		}
		if (NewActiveEventIndex != ActiveMainProgramEventIndex)
		{
			// 구독자가 핸들러 안에서 GetEffectiveTimeScale() 을 읽는다. 잠금 상태를 먼저 세우지
			// 않으면 공연 시작 핸들러가 공연 전 배속을 보고 판단한다.
			bWasMainProgramLocked = NewActiveEventIndex != INDEX_NONE;
			if (TodayEvents.IsValidIndex(ActiveMainProgramEventIndex))
			{
				const FString EndLabel = TodayEvents.IsValidIndex(ActiveMainProgramEventIndex + 1)
					? TodayEvents[ActiveMainProgramEventIndex + 1].EventLabel : CachedMainProgramEndLabel;
				UE_LOG(LogTemp, Log, TEXT("공연 종료 감지! 라벨: %s"), *EndLabel);
				OnMainProgramEnded.Broadcast(EndLabel);
			}
			ActiveMainProgramEventIndex = NewActiveEventIndex;
			if (TodayEvents.IsValidIndex(ActiveMainProgramEventIndex))
			{
				CachedMainProgramEndLabel = TodayEvents.IsValidIndex(ActiveMainProgramEventIndex + 1)
					? TodayEvents[ActiveMainProgramEventIndex + 1].EventLabel : TEXT("");
				UE_LOG(LogTemp, Log, TEXT("공연 시작 감지! ElapsedLiveSeconds: %f"), ElapsedLiveSeconds);
				OnMainProgramStarted.Broadcast(TodayEvents[ActiveMainProgramEventIndex].EventLabel);
			}
			OnTimeScaleChange.Broadcast(GetEffectiveTimeScale());
		}
		else
		{
			bWasMainProgramLocked = bIsLockedNow;
		}
		// 다음 프레임 비교용으로 저장

		const float EffectiveScale = GetEffectiveTimeScale();
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

	// 필수 시설 때문에 시간이 만료된 채 대기할 수 있다. UI에는 음수 시간을 보내지 않는다.
	RemainingTime = FMath::Max(0.f, RemainingTime);
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

void UFF_FestivalStageSubsystem::StartNewFestival()
{
	CurrentDay = 1;
	CurrentStage = EFF_FestivalStage::Setup;
	ElapsedLiveSeconds = 0.f;
	bFestivalEndedBroadcasted = false;
	bNightLightingActivated = false;
	// 밤 조명 상태도 리셋 안 하면 재시작 시 낮인데 조명 켜진 채로 남을 수 있음

	bFestivalStarted = true;
	// 이 줄이 있어야 Tick()이 다시 돌기 시작함

	UE_LOG(LogTemp, Log, TEXT("[Festival] 새 축제 시작! Day=1, Stage=Setup"));

	OnFestivalStarted.Broadcast();
	// 축제가 시작됐다고 UI 등 구독자들에게 알림
}

void UFF_FestivalStageSubsystem::SetMinigameInProgress(bool bInProgress)
{
	bMinigameInProgress = bInProgress;
	UE_LOG(LogTemp, Log, TEXT("[Festival] 미니게임 상태 변경: %s"), bInProgress ? TEXT("시작(1배속 고정)") : TEXT("종료(배속 해제)"));
	OnTimeScaleChange.Broadcast(GetEffectiveTimeScale());
}

float UFF_FestivalStageSubsystem::GetEffectiveTimeScale() const
{
	// 미니게임 중이면 어느 단계든 무조건 1배속
	if (bMinigameInProgress)
	{
		return 1.f;
	}

	// Live 단계에서 메인 공연 잠금 구간이면 1배속
	// (Tick()에서 매 프레임 갱신해두는 bWasMainProgramLocked를 사용 - 한 프레임 지연은 무시 가능한 수준)
	if (CurrentStage == EFF_FestivalStage::Live && bWasMainProgramLocked)
	{
		return 1.f;
	}

	return TimeScale;
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
	// 저장에 참여한다.
	if (UFF_SaveSubsystem* Save = GetGameInstance()->GetSubsystem<UFF_SaveSubsystem>())
	{
		Save->RegisterSaveable(this);
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
	FText BlockingReason;
	return Range.bSkippable && CanTransitionTo(Range.NextStage, BlockingReason);
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
	//  수정 - UMETA(DisplayName) 직접 읽기는 패키징 빌드에서 값이 사라져서 하드코딩 매핑으로 교체
	switch (CurrentStage)
	{
	case EFF_FestivalStage::Setup:
		return FText::FromString(TEXT("셋업"));
		// Setup 단계일 때 반환할 한글 이름

	case EFF_FestivalStage::PromotionPreview:
		return FText::FromString(TEXT("홍보 프리뷰"));
		// PromotionPreview 단계일 때 반환할 한글 이름

	case EFF_FestivalStage::SunsetTransition:
		return FText::FromString(TEXT("노을 전환"));
		// SunsetTransition 단계일 때 반환할 한글 이름

	case EFF_FestivalStage::MorningCheck:
		return FText::FromString(TEXT("모닝체크"));
		// MorningCheck 단계일 때 반환할 한글 이름

	case EFF_FestivalStage::Live:
		return FText::FromString(TEXT("라이브 운영"));
		// Live 단계일 때 반환할 한글 이름

	case EFF_FestivalStage::Closing:
		return FText::FromString(TEXT("마감"));
		// Closing 단계일 때 반환할 한글 이름

	case EFF_FestivalStage::Report:
		return FText::FromString(TEXT("리포트"));
		// Report 단계일 때 반환할 한글 이름

	case EFF_FestivalStage::Finale:
		return FText::FromString(TEXT("피날레"));
		// Finale 단계일 때 반환할 한글 이름

	default:
		return FText::FromString(TEXT("알 수 없음"));
		// 혹시 모를 예외 상황 (enum에 없는 값)
	}
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


FString UFF_FestivalStageSubsystem::GetLiveClockStringFromMinutes(int32 LiveMinutes) const
{
	const int32 StartHour = 17;
	// Live 시작 시각(오후 5시)

	const int32 TotalMinutes = StartHour * 60 + LiveMinutes;
	// 시작 시각(17:00)에 진행 시간(분)을 더함

	const int32 Hour = (TotalMinutes / 60) % 24;
	// 시 계산 (24시간 제, 24시 넘어가면 0시로 순환)

	const int32 Minute = TotalMinutes % 60;
	// 분 계산

	return FString::Printf(TEXT("%02d:%02d"), Hour, Minute);
	// "HH:MM" 형태로 반환
}

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

bool UFF_FestivalStageSubsystem::TryGetLiveElapsedMinutes(float& OutMinutes) const
{
	if (!IsLiveStage()) return false;
	OutMinutes = StageSecondsToFestivalMinutes(ElapsedLiveSeconds);
	return true;
}

bool UFF_FestivalStageSubsystem::TryGetLiveDurationMinutes(float& OutMinutes)
{
	const float DurationSeconds = GetLiveDurationForCurrentDay();
	if (DurationSeconds <= 0.f) return false;
	OutMinutes = StageSecondsToFestivalMinutes(DurationSeconds);
	return true;
}

void UFF_FestivalStageSubsystem::WriteSaveData(TArray<uint8>& OutBytes) const
{
	FFF_StageSave State;
	State.Day = CurrentDay;
	State.Weather = static_cast<uint8>(SelectedWeather);

	if (const UEnum* EnumClass = StaticEnum<EFF_FestivalStage>())
	{
		State.StageId = FName(*EnumClass->GetNameStringByValue(
			static_cast<int64>(CurrentStage)));
	}

	UFF_SaveArchive::WriteStruct(OutBytes, State);
}

void UFF_FestivalStageSubsystem::ReadSaveData(const TArray<uint8>& InBytes)
{
	FFF_StageSave State;
	if (!UFF_SaveArchive::ReadStruct(InBytes, State))
	{
		return;
	}

	CurrentDay = FMath::Max(1, State.Day);
	SelectedWeather = static_cast<EFF_WeatherType>(State.Weather);

	/* 단계는 이름으로 되돌린다.

	   못 찾으면 Setup 으로 둔다. 저장은 정산 직후에만 하므로 불러온 시점은
	   언제나 하루의 시작이고, 그때가 Setup 이다.
	*/
	EFF_FestivalStage Restored = EFF_FestivalStage::Setup;

	if (!State.StageId.IsNone())
	{
		if (const UEnum* EnumClass = StaticEnum<EFF_FestivalStage>())
		{
			const int64 Found = EnumClass->GetValueByNameString(State.StageId.ToString());
			if (Found != INDEX_NONE)
			{
				Restored = static_cast<EFF_FestivalStage>(Found);
			}
		}
	}

	const EFF_FestivalStage Previous = CurrentStage;
	CurrentStage = Restored;

	/* 화면이 따라오게 알린다.

	   값만 바꾸면 UI 도 사운드도 옛 단계를 계속 붙들고 있다. 전이 함수를
	   부르지 않는 이유는, 그쪽이 전이 규칙을 검사해서 Report 에서 Setup 으로
	   바로 못 갈 수 있기 때문이다.
	*/
	OnStageChanged.Broadcast(Previous, CurrentStage);
}
