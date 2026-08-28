// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NPC/Party/FF_ProgramReservation.h"
#include "Time/FF_FestivalStage.h"
#include "FF_ProgramEconomyBridgeSubsystem.generated.h"

class UFF_FestivalProgressionSubsystem;
class UFF_FestivalStageSubsystem;

/**
 * 그날 프로그램의 체험비를 걷는 창구.
 *
 * Crowd 는 World 스코프이고 경제는 GameInstance 스코프라, 이 브리지는 World 에 살면서
 * 경제를 호출 시점에 찾는다.
 *
 * ### 언제 걷는가 — 프로그램이 열리는 시각이다
 * 프로그램을 사면 정원만큼의 손님이 추가로 방문하므로, 프로그램이 열리는 순간
 * 참가자도 체험비도 확정돼 있다. 마감까지 기다릴 이유가 없다 — 부스 수수료도 이미
 * Live 중 실시간이라 시점이 맞고, 정원 미달 경고가 Live 중에 뜬다.
 *
 * 배정은 여기서 하지 않는다. 예약 그룹은 스폰될 때 확정된다(`ConfirmReservedParty`).
 *
 * Closing 진입은 안전망으로 남긴다. 시작 시각 정산이 놓친 카드(카드 시각이 그날
 * Live 길이를 넘었거나 Live 를 건너뛴 경우)를 마감에 걷고 Warning 을 남긴다. 하루 경계까지
 * 미루면 원장이 이미 리셋된 뒤라 그날 매출로 안 들어가므로, Closing 이 마지막 기회다.
 */
UCLASS()
class FESTAFIESTA_API UFF_ProgramEconomyBridgeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	// Stage 구독은 Initialize 가 아니라 여기서 건다.
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	// 이 단계 진입이 안전망 정산 시점인가.
	static bool IsProgramSettlementStage(EFF_FestivalStage Stage)
	{
		return Stage == EFF_FestivalStage::Closing;
	}

	// 테스트가 Stage 없이 진입만 재현할 수 있게 열어 둔다.
	int32 ProcessStageEntry(EFF_FestivalStage NewStage);

private:
	UFUNCTION()
	void HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage);

	// Live 동안 초가 바뀔 때마다 온다. 시작 시각이 지난 프로그램을 그때 걷는다.
	UFUNCTION()
	void HandleRemainingSecondsChanged(int32 NewSeconds);

	// 경제는 GameInstance 스코프라 호출 시점에 찾는다. 초기화 순서에 기대지 않는다.
	UFF_FestivalProgressionSubsystem* ResolveFestivalProgression() const;

	UPROPERTY()
	TObjectPtr<UFF_FestivalStageSubsystem> Stage;
};
