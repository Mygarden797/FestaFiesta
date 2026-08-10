// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
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
class FESTAFIESTA_API UFF_FestivalStageSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

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

	UPROPERTY(BlueprintAssignable, Category = "Festival Stage")
	FOnStageChanged OnStageChanged;
	// 이벤트 신호 실제 변수 (다른 시스템이 여기 등록해두면 알림 받음)

	UPROPERTY(BlueprintAssignable, Category = "Festival Stage")
	FOnRemainingSecondsChanged OnRemainingSecondsChanged;
	//초 단위로 바뀔 때만 알림 받음

	UPROPERTY(BlueprintAssignable, Category = "Festival Stage")
	FOnFestivalEnded OnFestivalEnded;
	// 다른 파트분들이 축제 종료 시점을 알 수 있음

	UFUNCTION(BlueprintCallable, Category = "Festival Stage")
	void RequestTransition(EFF_FestivalStage NewStage);
	// 단계 바꿔달라는 요청 함수

	UFUNCTION(BlueprintPure, Category = "Festival Stage")
	bool IsReconfigureAllowed() const;
	// 지금 배치 바꿔도 되는 단계인지 t/f로 알려줌 

	UFUNCTION(BlueprintCallable, Category = "Festival Stage")
	void SetTimeScale(float NewScale);
	// 배속 바꾸는 함수

	UFUNCTION(BlueprintPure, Category = "Festival Stage")
	bool IsMainProgramLocked();
	// 지금 메인 공연 배속 잠금 구간인지 확인

	UFUNCTION(BlueprintCallable, Category = "Festival Stage")
	void AdvanceToNextDay();
	//report에서 다음날로 누르면 호출

	UFUNCTION(BlueprintCallable, Category = "Festival Stage")
	void SkipCurrentStage();
	//  스킵 가능한 단계면 즉시 다음 단계로 넘기는 함수

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

	UFUNCTION(BlueprintPure, Category = "Festival Stage")
	bool IsCurrentStageSkippable();
	// 지금 이 단계가 스킵 가능한지 확인하는 함수

	void HandleLiveEnd();
	// 타임 하나가 다 끝났을 때 다음으로 넘기는함수

	const TArray<FFF_LiveTriggerEvent>& GetEventsForCurrentDay() ;
	// 오늘 날짜에 맞는 이벤트 배열을 골라주는 함수

	float GetLiveDurationForCurrentDay() ;
	//오늘 Live 총 길이(마지막 이벤트 시각)를 알려주는 함수

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


};