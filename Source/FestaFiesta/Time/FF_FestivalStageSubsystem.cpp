// Fill out your copyright notice in the Description page of Project Settings.


#include "FF_FestivalStageSubsystem.h"


void UFF_FestivalStageSubsystem::RequestTransition(EFF_FestivalStage NewStage)
{
	EFF_FestivalStage PreviousStage = CurrentStage;
	CurrentStage = NewStage;

	// todo: 허용되지 않는 Stage 전환을 막는 검증 로직 추가 필요
	// todo: RemainingTime을 실제로 감소시키는 Tick 로직 추가 필요

	OnStageChanged.Broadcast(PreviousStage, NewStage);
}