// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UI/LiveHUD/OperationAlert/FF_OperationAlertTypes.h"
#include "FF_OperationAlertSubsystem.generated.h"

struct FFF_IncidentClosedInfo;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnOperationAlertRaised, const FFF_OperationAlert&, Alert);

/**
 * Live 동안 운영 상태를 주기적으로 보고 "화장실이 부족해요" 같은 알림을 낸다. 화면은 UFF_OperationAlertPanel 이 그린다.
 *
 * 판정 주기·쿨다운은 Live 경과 시간(배속 반영)으로 센다. 같은 종류는 쿨다운 동안 다시 내지 않는다.
 * 줄 이탈·갈 곳 없음 퇴장은 지난 판정 이후 늘어난 수로 본다 — 누적값이 날마다 0 으로 돌아가도 음수가 되지 않게 그때는 지금 값을 그대로 쓴다.
 * 낸 알림은 History 에 남겨 "지난 알림" 목록이 다시 보여 준다.
 */
UCLASS()
class FESTAFIESTA_API UFF_OperationAlertSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** 조건을 보지 않고 알림을 낸다. 쿨다운도 보지 않는다(콘솔 미리보기·테스트용). */
	void RaiseAlert(EFF_OperationAlertKind Kind);

	/** HUD가 알릴 대상으로 고른 사건 종료를 통합 소식에 기록한다. 운영 토스트는 추가하지 않는다. */
	void RecordIncidentFinished(const FFF_IncidentClosedInfo& Info);

	/** Live 경과 시간과 입력으로 한 번 판정한다. Tick 과 테스트가 같은 길을 쓴다. */
	void EvaluateAt(float LiveSeconds, int32 Day, const FFF_OperationAlertInputs& Inputs);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|OperationAlert")
	const TArray<FFF_OperationAlert>& GetHistory() const { return History; }

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|OperationAlert")
	FFF_OnOperationAlertRaised OnOperationAlertRaised;

	UPROPERTY(EditAnywhere, Category = "FestaFiesta|OperationAlert", meta = (ClampMin = "1.0"))
	float EvaluateIntervalLiveSeconds = 15.f;

	/** 시설 부족은 배치를 바꾸기 전까지 계속 참이라 길게 둔다 */
	UPROPERTY(EditAnywhere, Category = "FestaFiesta|OperationAlert", meta = (ClampMin = "0.0"))
	float ShortageCooldownLiveSeconds = 120.f;

	UPROPERTY(EditAnywhere, Category = "FestaFiesta|OperationAlert", meta = (ClampMin = "0.0"))
	float LeaveCooldownLiveSeconds = 60.f;

	UPROPERTY(EditAnywhere, Category = "FestaFiesta|OperationAlert", meta = (ClampMin = "1"))
	int32 MaxHistory = 30;

	FFF_OperationAlertThresholds Thresholds;

private:
	// 커버리지와 지난 판정 이후 늘어난 퇴장·이탈 수를 채운다. 군중 시뮬레이션이 없으면 false
	bool ReadInputs(FFF_OperationAlertInputs& OutInputs);
	void ResetDay(int32 Day);
	void StoreAlert(const FFF_OperationAlert& Alert);

	TArray<FFF_OperationAlert> History;

	// 종류별 마지막으로 낸 Live 시각. 음수면 오늘 아직 안 냈다
	float LastRaisedLiveSeconds[static_cast<int32>(EFF_OperationAlertKind::Count)] = { -1.f, -1.f, -1.f, -1.f, -1.f, -1.f };

	int32 TrackedDay = INDEX_NONE;
	float LastEvaluatedLiveSeconds = -1.f;
	int32 LastQueueAbandons = 0;
	int32 LastNoChoiceLeaves = 0;
};
