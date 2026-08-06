// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "FF_FestivalStage.h"
#include "FF_FestivalStageSubsystem.generated.h"

// Stage 바뀌면 알림 보내는 이벤트 신호 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStageChanged, EFF_FestivalStage, PreviousStage, EFF_FestivalStage, NewStage);

USTRUCT(BlueprintType)
struct FFF_LiveTriggerEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float TriggerSeconds = 0.f;
	// Live 시작 부터 몇 초 뒤에 이 이벤트가 일어나는지 (x1 기준)

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString EventLabel;
	// 이 이벤트의 이름 (예: "다대불꽃쇼 시작") — 로그·디버깅용 텍스트

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bIsMainProgramLockStart = false;
	// 이 이벤트가 "메인 공연 시작"이면 true — 배속 잠금 구간을 계산하는 기준점이 됨
};

UCLASS()
class FESTAFIESTA_API UFF_FestivalStageSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
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
	bool IsMainProgramLocked() const;
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

	const TArray<FFF_LiveTriggerEvent>& GetEventsForCurrentDay() const;
	// 오늘 날짜에 맞는 이벤트 배열을 골라주는 함수

	float GetLiveDurationForCurrentDay() const;
	//오늘 Live 총 길이(마지막 이벤트 시각)를 알려주는 함수

	UPROPERTY(EditDefaultsOnly, Category = "Festival Stage|Schedule")
	TArray<FFF_LiveTriggerEvent> Day1Events = {
		{0.f,   TEXT("오픈"), false},
		{60.f,  TEXT("파차쿠닉 / EUNA"), false},
		{130.f, TEXT("부산대 소리터"), false},
		{150.f, TEXT("동아대 프릭스"), false},
		{170.f, TEXT("공식행사"), false},
		{180.f, TEXT("다대불꽃쇼 시작"), true},
		{210.f, TEXT("다대불꽃쇼 종료"), false},
		{240.f, TEXT("전체 종료"), false},
	};
	// Day1 타임테이블 데이터

	UPROPERTY(EditDefaultsOnly, Category = "Festival Stage|Schedule")
	TArray<FFF_LiveTriggerEvent> Day2Events = {
		{0.f,   TEXT("오픈"), false},
		{60.f,  TEXT("음스 / Hyesoo"), false},
		{120.f, TEXT("제니스 / Grupo Ouro"), false},
		{180.f, TEXT("헤이즈 무대 시작"), true},
		{240.f, TEXT("헤이즈 종료"), false},
		{300.f, TEXT("전체 종료"), false},
	};
	//Day2 타임테이블 데이터

	UPROPERTY(EditDefaultsOnly, Category = "Festival Stage|Schedule")
	TArray<FFF_LiveTriggerEvent> Day3Events = {
		{0.f,   TEXT("오픈"), false},
		{60.f,  TEXT("겟츠 / Danielle"), false},
		{120.f, TEXT("밴드기린 / 쿠나"), false},
		{180.f, TEXT("폴킴 무대 시작"), true},
		{240.f, TEXT("폴킴 종료"), false},
		{300.f, TEXT("전체 종료"), false},
	};
	// Day3 타임테이블 데이터
};