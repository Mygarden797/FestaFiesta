// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FF_OperationAlertTypes.generated.h"

/** 운영 알림 종류. 한 종류는 쿨다운 동안 다시 뜨지 않는다. */
UENUM(BlueprintType)
enum class EFF_OperationAlertKind : uint8
{
	FoodShortage,
	RestroomShortage,
	FunShortage,
	QueueAbandon,
	NoChoiceLeave,
	IncidentFinished,
	RestShortage,
	DrinkShortage,

	Count UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_OperationAlert
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|OperationAlert")
	EFF_OperationAlertKind Kind = EFF_OperationAlertKind::FoodShortage;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|OperationAlert")
	FText Message;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|OperationAlert")
	int32 Day = 1;

	/** 사건 종료 토스트는 HUD가 담당하고, 통합 소식에는 이력만 추가한다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|OperationAlert")
	bool bHistoryOnly = false;
};

/** 한 번 판정할 때 읽는 값. 퇴장·이탈은 지난 판정 이후 늘어난 수다. */
struct FFF_OperationAlertInputs
{
	// 시설 커버리지(하한~1). UFF_CrowdSimulationSubsystem::GetNeedCoverage
	float HungerCoverage = 1.f;
	float RestroomCoverage = 1.f;
	float StimulationCoverage = 1.f;
	float FatigueCoverage = 1.f;
	float ThirstCoverage = 1.f;

	int32 QueueAbandonsDelta = 0;
	int32 NoChoiceLeavesDelta = 0;
};

struct FFF_OperationAlertThresholds
{
	// 커버리지가 이 값보다 작으면 시설이 모자란다고 알린다. 커버리지 목표(음식 7 · 즐길 거리 10)는 많이 지은 배치의 기준이라
	// 1 미만마다 알리면 보통 배치에서도 운영 내내 알림이 뜬다. 목표의 절반에 못 미칠 때만 알린다
	float ShortageCoverageBelow = 0.5f;

	// 판정 사이에 줄 이탈·갈 곳 없음 퇴장이 이 수 이상 늘면 알린다
	int32 QueueAbandonsAtLeast = 5;
	int32 NoChoiceLeavesAtLeast = 5;
};

/** 입력만 보고 지금 조건이 맞는 알림 종류를 고른다. 쿨다운은 호출하는 쪽이 본다. */
struct FESTAFIESTA_API FFF_OperationAlertRules
{
	static TArray<EFF_OperationAlertKind> Evaluate(
		const FFF_OperationAlertInputs& Inputs, const FFF_OperationAlertThresholds& Thresholds);

	static FText GetMessage(EFF_OperationAlertKind Kind);
};
