// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_CrowdEconomyBridgeSubsystem.generated.h"

class UFF_CrowdSimulationSubsystem;
class UFF_FestivalProgressionSubsystem;

/**
 * Crowd 와 경제 사이를 잇는 양방향 브리지 — 방향은 둘이고 서로 독립적이다.
 *
 * 1) NPC 입장 → 입장료 매출 (Crowd 가 이벤트를 쏘고 경제가 구독한다)
 * 2) 홍보 등급 → 실제 방문객 수 (경제가 값을 밀어 넣고 Crowd 는 출처를 모른다)
 *
 * Crowd 는 World 스코프이고 경제는 GameInstance 스코프라,
 * 이 브리지는 World 에 살면서 경제를 호출 시점에 찾는다.
 */
UCLASS()
class FESTAFIESTA_API UFF_CrowdEconomyBridgeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	// 이번 호출에서 입장한 인원 수만큼 입장료를 매긴다.
	UFUNCTION()
	void HandleVisitorsAdmitted(int32 AdmittedCount);

	// 축제가 (재)시작되면 그 시점의 예상 방문객으로 Crowd 를 채운다 — 홍보를 안 사도
	// BaseDailyVisitors 가 반영되게 하기 위함이다.
	UFUNCTION()
	void HandleFestivalDefinitionChanged();

	// 홍보 등급이 바뀌어 예상 방문객이 바뀔 때마다 Crowd 에 다시 채운다.
	UFUNCTION()
	void HandleExpectedVisitorsChanged(int32 NewExpectedVisitors);

	// 경제는 GameInstance 스코프라 초기화 순서를 보장할 수 없다 — 필요할 때마다 다시 찾고,
	// 대상이 바뀌면(=처음 잡히면) 구독을 새로 건다. 델리게이트를 구독해야 해서(입장료 읽기 전용
	// 조회와 달리) 매번 새로 찾지 않고 안정적인 참조를 들고 있어야 한다.
	void BindProgression();

	UPROPERTY()
	TObjectPtr<UFF_CrowdSimulationSubsystem> Crowd;

	UPROPERTY()
	TObjectPtr<UFF_FestivalProgressionSubsystem> Progression;
};
