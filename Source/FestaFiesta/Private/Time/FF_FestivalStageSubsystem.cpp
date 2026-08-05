// Fill out your copyright notice in the Description page of Project Settings.


#include "Time/FF_FestivalStageSubsystem.h"


void UFF_FestivalStageSubsystem::RequestTransition(EFF_FestivalStage NewStage)
{
	EFF_FestivalStage PreviousStage = CurrentStage;
	CurrentStage = NewStage;

	if (NewStage == EFF_FestivalStage::Live)
	{
		RemainingTime = LiveSlotDuration;
	}

	// todo: 허용 안 되는 전환 막는 검증 로직 필요

	OnStageChanged.Broadcast(PreviousStage, NewStage);
	// 바뀌었다고 알림
}

bool UFF_FestivalStageSubsystem::IsReconfigureAllowed() const
{
	return CurrentStage == EFF_FestivalStage::Setup
		|| CurrentStage == EFF_FestivalStage::Reconfigure
		|| CurrentStage == EFF_FestivalStage::MorningCheck;
	// 이 세 단계일 때만 배치 변경 허용
}

void UFF_FestivalStageSubsystem::Tick(float DeltaTime)
{
	if (CurrentStage != EFF_FestivalStage::Live)
	{
		return;
		// Live 아닐 땐 시간 안 흐르게 함
	}

	RemainingTime -= DeltaTime;
	// 프레임마다 지나간 시간만큼 깎기

	if (RemainingTime <= 0.f)
	{
		HandleSlotEnd();
		// 시간 다 되면 다음 처리로 넘김
	}
}

void UFF_FestivalStageSubsystem::HandleSlotEnd()
{
	if (CurrentSlot < 4)
	{
		CurrentSlot++;
		// 아직 4타임 안 됐으면 슬롯만 올리고 계속 Live로

		// todo: 지금은 바로 Live인데, 실제론 재구성 거쳐야 함
		RequestTransition(EFF_FestivalStage::Live);
	}
	else
	{
		CurrentSlot = 1;
		CurrentDay++;
		// 4타임 다 끝났으면 슬롯 리셋하고 날짜 하루 증가

		RequestTransition(EFF_FestivalStage::Closing);
		// 마감 단계로 이동
	}
}