// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Save/FF_Saveable.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "FF_FestivalStage.h"
#include "Engine/DataTable.h" 
#include "FF_FestivalStageSubsystem.generated.h"

// Stage 바뀌면 알림 보내는 이벤트 신호 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStageChanged, EFF_FestivalStage, PreviousStage, EFF_FestivalStage, NewStage);

// 초가 바뀔때 쓰는 딜리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRemainingSecondsChanged, int32, NewSeconds);

// 축제가 완전히 끝났을 때 쓰는 신호
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFestivalEnded);

// 메인 공연 시작되는 순간 알리는 신호
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMainProgramStarted, FString, EventLabel);

// 메인 공연 끝나는 순간 알리는 신호
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMainProgramEnded, FString, EventLabel);

//  밤 조명 켤 시간이 됐다고 알리는 신호
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNightLightingActivated);

// 하루가 끝날 때 다음 시스템(시설/경제)에 이월용 시간 정보를 알리는 신호
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDayEnded, int32, EndedDay, float, TodayLiveDuration);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFestivalStarted);
// 축제가 처음 시작됐음을 알리는 신호 (StartNewFestival 호출 시 발동)

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTimeScaleChange, float, TimeScale);

// 오늘 날씨 종류를 나타내는 enum
UENUM(BlueprintType)
enum class EFF_WeatherType : uint8
{
	Clear,
	// 맑음

	Rain,
	// 비

	Windy
	// 바람
};

// 환경 상태가 바뀔 때 다른 시스템(이동/시설/만족도)에 알리는 신호
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnvironmentStateChanged, EFF_WeatherType, Weather, bool, bIsNight);
// Weather : 지금 날씨 종류
// bIsNight : 야간 조명 켜진 상태인지 여부

USTRUCT(BlueprintType)
struct FFF_LiveTriggerEvent : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Day = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float TriggerSeconds = 0.f;
	// Live 시작 부터 몇 초 뒤에 이 이벤트가 일어나는지 (x1 기준)

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString EventLabel;
	// 이 이벤트의 이름 (예: "다대불꽃쇼 시작")

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bIsMainProgramLockStart = false;
	// "공연 시작"이면 true — 배속 잠금 구간을 계산하는 기준점
};

USTRUCT(BlueprintType)
// 배속 설정
struct FFF_TimeSettingRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float TimeScaleValue = 1.f;
	// 배속 후보값 (1/2/4 )

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float MainProgramLockLeadSeconds = 10.f;
	// 메인 공연 몇 초 전부터 배속을 잠글지

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float NightLightingTriggerSeconds = 90.f;
	// Live 시작 후 몇 초 뒤에 밤 조명을 켤지 
};

USTRUCT(BlueprintType)
// 셋업/모닝체크 등 단계별 시간 범위
struct FFF_PhaseTimeRange : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Day = 1;
	// 몇 일차 데이터인지 판단

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EFF_FestivalStage Stage = EFF_FestivalStage::Setup;
	// 어느 Stage에 대한 시간 범위인지

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float MinSeconds = 0.f;
	// 이 단계 최소 진행 시간

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float MaxSeconds = 0.f;
	// 이 단계 최대 제한 시간

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bSkippable = false;
	// 스킵 가능한 단계인지

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EFF_FestivalStage NextStage = EFF_FestivalStage::Live;
	//이 단계가 끝나면 다음에 어느 단계로 가는지
};

UCLASS()
/* 저장에 담기는 단계 상태.

   하루가 시작되는 순간만 저장하므로 단계는 언제나 Setup 이다. 그래도 담는
   이유는, 나중에 다른 시점에서 저장하게 됐을 때 형식을 안 바꿔도 되게 하려는 것이다.

   단계를 열거형 값이 아니라 이름으로 담는다. 값으로 담으면 항목을 중간에
   끼워 넣을 때 그 뒤가 전부 밀려서, 옛 저장이 조용히 다른 단계를 가리킨다.
*/
USTRUCT()
struct FFF_StageSave
{
	GENERATED_BODY()

	UPROPERTY() int32 Day = 1;
	UPROPERTY() FName StageId;
	UPROPERTY() uint8 Weather = 0;
};

class FESTAFIESTA_API UFF_FestivalStageSubsystem
	: public UGameInstanceSubsystem, public FTickableGameObject, public IFF_Saveable
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// ── 저장 참여 ───────────────────────────────────────────────────────────

	virtual FName GetSaveKey() const override { return TEXT("Stage"); }

	// 날짜는 다른 것이 기대는 값이라 가장 먼저 복원한다.
	virtual EFF_SaveLoadPhase GetSaveLoadPhase() const override
	{
		return EFF_SaveLoadPhase::Global;
	}

	virtual void WriteSaveData(TArray<uint8>& OutBytes) const override;
	virtual void ReadSaveData(const TArray<uint8>& InBytes) override;

	UPROPERTY(BlueprintReadOnly, Category = "Festival Stage")
	EFF_FestivalStage CurrentStage = EFF_FestivalStage::Setup;
	// 지금 몇 단계인지 저장 (기본값: Setup)

	UPROPERTY(BlueprintReadOnly, Category = "Festival Stage")
	float RemainingTime = 0.f;
	// 남은 시간

	UPROPERTY(BlueprintReadOnly, Category = "Festival Stage")
	int32 CurrentDay = 1;
	// 지금 며칠째인지 (1~3일차), CurrentDay가 기본값 1로 시작

	UPROPERTY(BlueprintReadOnly, Category = "Festival Stage")
	float ElapsedLiveSeconds = 0.f;
	// live 시작 후 몇 초 지났는지

	UPROPERTY(BlueprintReadOnly, Category = "Festival Stage")
	float TimeScale = 1.f;
	// 플레이어가 고른 배속값 저장

	UPROPERTY(EditDefaultsOnly, Category = "Festival Stage|Config")
	float NightLightingTriggerSeconds = 90.f;
	// 기본값이고 실제 값은 TimeSettingsTable에서 가져옴

	bool bNightLightingActivated = false;
	// 오늘 이미 밤 조명을 켰는지 여부 

	bool bMinigameInProgress = false;
	// 미니게임 진행 중이면 true. true면 배속을 무조건 1배속으로 고정

	bool bFestivalStarted = true;
	//이 값이 false면 Tick()에서 시간 진행을 아예 멈춤

	UPROPERTY(BlueprintReadOnly, Category = "Festival Stage|Environment")
	EFF_WeatherType SelectedWeather = EFF_WeatherType::Clear;
	// 플레이어가 Setup/MorningCheck에서 선택한 오늘의 날씨 (기본값: 맑음)

	UPROPERTY(BlueprintAssignable, Category = "Festival Stage")
	FOnStageChanged OnStageChanged;
	// 이벤트 신호 실제 변수 (다른 시스템이 여기 등록해두면 알림 받음)

	UPROPERTY(BlueprintAssignable, Category = "Festival Stage")
	FOnFestivalStarted OnFestivalStarted;
	// 축제가 새로 시작됐음을 UI 등 다른 시스템에게 알리는 신호

	UPROPERTY(BlueprintAssignable, Category = "Festival Stage")
	FOnRemainingSecondsChanged OnRemainingSecondsChanged;
	//초 단위로 바뀔 때만 알림 받음

	UPROPERTY(BlueprintAssignable, Category = "Festival Stage")
	FOnFestivalEnded OnFestivalEnded;
	// 다른 파트가 축제 종료 시점을 알 수 있음

	UPROPERTY(BlueprintAssignable, Category = "Festival Stage")
	FOnMainProgramStarted OnMainProgramStarted;
	// 메인 공연 시작 시점을 알림

	UPROPERTY(BlueprintAssignable, Category = "Festival Stage")
	FOnMainProgramEnded OnMainProgramEnded;
	// 메인 공연 종료 시점을 알림

	UPROPERTY(BlueprintAssignable, Category = "Festival Stage")
	FOnNightLightingActivated OnNightLightingActivated;
	// 밤 조명(글로우 프롭) 켤 시간이 됐음을 알리는 신호

	UPROPERTY(BlueprintAssignable, Category = "Festival Stage")
	FOnDayEnded OnDayEnded;
	// 하루가 끝났을 때, 다른 시스템(시설/경제)에게 이월용 시간 정보를 전달하는 신호

	UPROPERTY(BlueprintAssignable, Category = "Festival Stage|Environment")
	FOnEnvironmentStateChanged OnEnvironmentStateChanged;
	// 날씨 상태가 바뀌었음을 다른 시스템(이동/시설/만족도)에 전달하는 신호

	UFUNCTION(BlueprintCallable, Category = "Festival Stage")
	void RequestTransition(EFF_FestivalStage NewStage);
	// 단계 바꿔달라는 요청 함수

	// 필수 시설에 의한 전환 허용 여부와 UI 안내. 단계 표의 스킵 가능 여부는 별도다.
	UFUNCTION(BlueprintPure, Category = "Festival Stage|Setup")
	bool CanTransitionTo(EFF_FestivalStage NewStage, FText& OutBlockingReason) const;

	// 현재 단계의 다음 단계가 필수 시설 때문에 막혔을 때 안내 문구.
	UFUNCTION(BlueprintPure, Category = "Festival Stage|Setup")
	FText GetStageAdvanceBlockingReason();

private:
	// 직전에 전환이 막혔던 사유와 목표 단계. 로그 중복만 막는 용도이며 판정에는 쓰지 않는다.
	// 사유가 바뀔 때만 한 번 남긴다.
	FString LastBlockedTransitionReason;
	EFF_FestivalStage LastBlockedTransitionStage = EFF_FestivalStage::Setup;

public:

	UFUNCTION(BlueprintPure, Category = "Festival Stage")
	bool IsReconfigureAllowed() const;
	// 지금 배치 바꿔도 되는 단계인지 t/f로 알려줌 

	UFUNCTION(BlueprintCallable, Category = "Festival Stage")
	void SetTimeScale(float NewScale);
	// 배속 바꾸는 함수
	
	FOnTimeScaleChange OnTimeScaleChange;

	UFUNCTION(BlueprintCallable, Category = "Festival Stage|Environment")
	void SetSelectedWeather(EFF_WeatherType NewWeather);
	// 플레이어가 날씨를 선택하는 함수 (Setup/MorningCheck에서만 허용)

	
	UFUNCTION(BlueprintPure, Category = "Festival Stage|Environment")
	EFF_WeatherType GetSelectedWeather() const { return SelectedWeather; }
	// 지금 선택된 날씨를 바로 읽어오는 Getter 

	
	UFUNCTION(BlueprintPure, Category = "Festival Stage|Environment")
	bool GetIsNightActive() const { return bNightLightingActivated; }
	// 지금 밤(야간 조명 켜진 상태)인지 바로 읽어오는 Getter

	UFUNCTION(BlueprintPure, Category = "Festival Stage")
	bool IsMainProgramLocked();

	/**
	 * 지금 잠금 구간인 메인 공연의 실제 시작 시각(Live 경과 초).
	 *
	 * 잠금은 시작보다 MainProgramLockLeadSeconds 만큼 먼저 들어간다. 그 구간은 관객이
	 * 무대로 모이는 시간이지 공연 중이 아니다 — 둘을 같은 신호로 쓰면 리드타임을 늘린 만큼
	 * "공연 중"이 길어진다. 잠금 구간이 아니면 false 이고 OutSeconds 는 유지한다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Time")
	bool TryGetActiveMainProgramStartSeconds(float& OutSeconds);

	/**
	 * 아직 끝나지 않은 다음 메인 공연의 시작 시각(Live 경과 초). 잠금 구간과 무관하다.
	 *
	 * 배속 잠금 리드타임과 관객 집결 리드타임은 서로 다른 값이다 — 잠금은 "플레이어가
	 * 공연을 1배속으로 본다"이고 집결은 "먼 NPC 가 걸어올 시간"이라 훨씬 길어야 한다.
	 * 둘을 한 값으로 묶으면 집결을 늘리는 순간 하루 대부분이 1배속으로 잠긴다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Time")
	bool TryGetNextMainProgramStartSeconds(float& OutSeconds);

	/** 아직 끝나지 않은 다음 메인 공연의 종료 시각(Live 경과 초). 다음 이벤트가 없으면 그날 Live 끝이다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Time")
	bool TryGetNextMainProgramEndSeconds(float& OutSeconds);
	// 지금 메인 공연 배속 잠금 구간인지 확인

	UFUNCTION(BlueprintCallable, Category = "Festival Stage")
	void StartNewFestival();
	// 축제를 처음부터 다시 시작할 때 호출

	UFUNCTION(BlueprintCallable, Category = "Festival Stage")
	void SetMinigameInProgress(bool bInProgress);
	// 미니게임 시작할 때 true로, 끝날 때 false로 불러주는 함수

	UFUNCTION(BlueprintPure, Category = "Festival Stage")
	bool IsMinigameInProgress() const { return bMinigameInProgress; }
	// 지금 미니게임 중인지 바로 확인하는 함수

	UFUNCTION(BlueprintPure, Category = "Festival Stage")
	float GetEffectiveTimeScale() const;
	// 실제로 적용되는 배속. Crowd 등 다른 시스템은 TimeScale 대신 이 함수를 써야 함
	// (공연 잠금·미니게임·마감 등 모든 상황이 반영된 "진짜" 배속)

	UFUNCTION(BlueprintCallable, Category = "Festival Stage")
	void AdvanceToNextDay();
	//report에서 다음날로 누르면 호출

	UFUNCTION(BlueprintCallable, Category = "Festival Stage")
	void SkipCurrentStage();
	//  스킵 가능한 단계면 즉시 다음 단계로 넘기는 sjarla


	UFUNCTION(BlueprintPure, Category = "Festival Stage")
	FText GetCurrentStageDisplayName() const;
	// 지금 단계 이름을 한글로 반환

	UFUNCTION(BlueprintPure, Category = "Festival Stage")
	FString GetCurrentLiveClockString() const;
	// Live일 때 지금 몇 시 몇 분인지 반환

	UFUNCTION(BlueprintPure, Category = "Festival Stage")
	FString GetLiveClockStringFromMinutes(int32 LiveMinutes) const;
	// LiveMinutes(Live 시작 후 지난 분)을 넣으면 "17:30" 같은 시계 문자열로 바꿔주는 함수

	UFUNCTION(BlueprintPure, Category = "Festival Stage")
	bool IsLiveStage() const;
	// 지금 Live 단계인지

	UFUNCTION(BlueprintPure, Category = "Festival Stage")
	bool IsReportStage() const;
	// 지금 Report 단계인지

	UFUNCTION(BlueprintPure, Category = "Festival Stage")
	bool IsFinalDay() const;
	// 지금 3일차인지

	UFUNCTION(BlueprintPure, Category = "Festival Stage")
	bool IsCurrentStageSkippable();
	// 지금 이 단계가 스킵 가능한지 확인하는 함수 (private에서 public으로 이동)

	// Stage x1 초를 축제분으로 환산한다. 이미 배속 반영된 경과 시간에 배속을 다시 곱하지 않는다.
	static constexpr float StageSecondsToFestivalMinutes(float Seconds) { return Seconds; }

	// Live 단계의 경과 축제분. Live가 아니면 false이며 OutMinutes는 유지한다.
	bool TryGetLiveElapsedMinutes(float& OutMinutes) const;

	// 현재 일차의 Live 총 축제분. 단계와 무관하며 길이가 없으면 false, 출력 유지.
	bool TryGetLiveDurationMinutes(float& OutMinutes);

	float GetLiveDurationForCurrentDay() ;
	//그날 Live 총 길이(마지막 이벤트 시각, x1 기준 초)를 알려주는 함수
	// Crowd의 입장 곡선이 진행도 분모로 읽는다 (private에서 public으로 이동)

	virtual void Tick(float DeltaTime) override;
	// 매 프레임 자동으로 호출되는 함수

	virtual bool IsTickable() const override { return true; }
	// Tick을 실제로 돌릴지 여부, true면 계속 돌아감

	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_FestivalStageSubsystem, STATGROUP_Tickables);
	}

private:

	FFF_PhaseTimeRange GetPhaseTimeRange(EFF_FestivalStage Stage);
	// 지금 Stage에 맞는 시간 범위 + 다음단계를 표에서 찾아오는 함수

	void HandleLiveEnd();
	// 타임 하나가 다 끝났을 때 다음으로 넘기는함수

	const TArray<FFF_LiveTriggerEvent>& GetEventsForCurrentDay() ;
	// 오늘 날짜에 맞는 이벤트 배열을 골라주는 함수

	UPROPERTY(EditDefaultsOnly, Category = "Festival Stage|Schedule")
	TObjectPtr<UDataTable> ProgramScheduleTable;
	// 날짜별 공연 이벤트 시간표

	UPROPERTY(EditDefaultsOnly, Category = "Festival Stage|Config")
	TObjectPtr<UDataTable> TimeSettingsTable;
	// 배속·락 리드타임 설정 테이블

	UPROPERTY(EditDefaultsOnly, Category = "Festival Stage|Config")
	TObjectPtr<UDataTable> DaySequenceTable;
	// 셋업/모닝체크 등 단계별 시간 범위 담은 테이블

	UPROPERTY()
	TArray<FFF_LiveTriggerEvent> CachedTodayEvents;

	int32 LastBroadcastSeconds = -1;

	bool bFestivalEndedBroadcasted = false;
	// Day3 축제 종료 알림을 한 번만 보냄

	bool bWasMainProgramLocked = false;
	// 공연 시작 알림을 한 번만 보내기 위한 체크용
	bool bLastLoggedMainProgramLockState = false;
	int32 ActiveMainProgramEventIndex = INDEX_NONE;

	FString CachedMainProgramEndLabel;
	// 종료 알림에 쓸 라벨을 시작 시점에 미리 저장

};
