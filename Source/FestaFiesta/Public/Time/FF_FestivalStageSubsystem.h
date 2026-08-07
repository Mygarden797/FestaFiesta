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
	// "공연 시작"이면 true — 배속 잠금 구간을 계산하는 기준점이
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


	virtual void Tick(float DeltaTime) override;
	// 매 프레임 자동으로 호출되는 함수

	virtual bool IsTickable() const override { return true; }
	// Tick을 실제로 돌릴지 여부, true면 계속 돌아감

	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_FestivalStageSubsystem, STATGROUP_Tickables);
	}

private:
	void HandleLiveEnd();
	// 타임 하나가 다 끝났을 때 다음으로 넘기는함수

	const TArray<FFF_LiveTriggerEvent>& GetEventsForCurrentDay() ;
	// 오늘 날짜에 맞는 이벤트 배열을 골라주는 함수

	float GetLiveDurationForCurrentDay() ;
	//오늘 Live 총 길이(마지막 이벤트 시각)를 알려주는 함수

	UPROPERTY(EditDefaultsOnly, Category = "Festival Stage|Schedule")
	TObjectPtr<UDataTable> ProgramScheduleTable;

	UPROPERTY()
	TArray<FFF_LiveTriggerEvent> CachedTodayEvents;
};