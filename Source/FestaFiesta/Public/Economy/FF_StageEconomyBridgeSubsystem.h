// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Time/FF_FestivalStage.h"
#include "FF_StageEconomyBridgeSubsystem.generated.h"

class UFF_FestivalStageSubsystem;
class UFF_FestivalProgressionSubsystem;

/**
 * Stage 변화를 하루 경계 정산으로 옮기는 브리지
 * 경제가 Stage 를 구독하는 단방향 방식
 * Day1 은 Setup 으로 시작해 MorningCheck 을 통과하지 않으므로 여기에 오지 않는다.
 */

UCLASS()
class FESTAFIESTA_API UFF_StageEconomyBridgeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	
	// Stage·Progression 서브시스템의 초기화를 보장하고 Stage 변경 델리게이트를 구독한다.
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	// 해당 Stage 진입이 하루 경계 정산 대상(MorningCheck 진입)인지 판정한다.
	static bool IsDayBoundaryStage(EFF_FestivalStage NewStage)
	{
		return NewStage == EFF_FestivalStage::MorningCheck;
	}
	
	// Stage 진입을 경제 시스템의 하루 경계 정산으로 전달한다.
	void ProcessStageEntry(EFF_FestivalStage NewStage, int32 Day);

	// 하루 경계 정산을 수행할 Progression 서브시스템을 지정한다.
	void SetProgression(UFF_FestivalProgressionSubsystem* InProgression);

private:
	
	// Stage 변경 이벤트를 받아 현재 날짜와 함께 경제 정산 처리로 전달한다.
	UFUNCTION()
	void HandleStageChanged(
		EFF_FestivalStage PreviousStage,
		EFF_FestivalStage NewStage);
	
	UPROPERTY()
	TObjectPtr<UFF_FestivalStageSubsystem> Stage;
	
	UPROPERTY()
	TObjectPtr<UFF_FestivalProgressionSubsystem> Progression;
};
