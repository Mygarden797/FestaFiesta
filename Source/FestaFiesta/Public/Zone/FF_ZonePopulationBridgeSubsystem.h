// FF_ZonePopulationBridgeSubsystem.h

// Crowd 시뮬레이션의 에이전트 위치를 FF_ZoneSystem의 존별 인구 표본으로 바꾸는 단일 지점이다.
// 흐름: 시뮬레이션 시계가 보고 간격만큼 진행하면 슬롯을 훑어 존별로 세고 등록된 모든 존에 보고한다.
// 불변식: 등록된 존은 인원이 0이어도 매 사이클 보고한다 — 플러그인에 감쇠가 없어 빠뜨리면 마지막 값이 남는다.
// 실패/성능: 보고가 거부되면 경과 시간을 이월하고, 슬롯별 직전 존 캐시로 경계 판정을 보통 존 1개로 줄인다.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_ZonePopulationBridgeSubsystem.generated.h"

class UFF_CrowdSimulationSubsystem;
class UFFZSWorldSubsystem;

/**
 * 집계 한 슬롯의 입력. 인덱스는 Crowd의 AgentIndex 공간과 같아야 한다.
 *
 * 슬롯별 직전 존 캐시가 이 인덱스로 값을 기억하므로, 압축된 "활성 에이전트만의 배열"을 넘기면
 * 캐시와 슬롯이 어긋나 매 사이클 전체 존을 다시 판정하게 된다. 비활성 슬롯도 자리를 유지한다.
 */
struct FFF_ZoneAgentSlot
{
	FVector Location = FVector::ZeroVector;
	bool bActive = false;
};

/** 인구 보고 한 사이클의 관찰값. 콘솔 명령과 테스트가 읽는 진단 표면이다. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_ZonePopulationCycle
{
	GENERATED_BODY()

	// 보고가 성립한 횟수. 0이면 아직 한 번도 보고하지 않았다는 뜻이다
	UPROPERTY(BlueprintReadOnly, Category = "FF Zone System|Population")
	int32 CycleIndex = 0;

	// 이 사이클을 발행한 시점의 Crowd 시뮬레이션 시계(초)
	UPROPERTY(BlueprintReadOnly, Category = "FF Zone System|Population")
	float SimTimeSeconds = 0.f;

	// 직전 사이클 이후 시뮬레이션이 실제로 진행한 시간(초). 밀도 유지 시간 판정에 그대로 들어간다
	UPROPERTY(BlueprintReadOnly, Category = "FF Zone System|Population")
	float DeltaSeconds = 0.f;

	// 이번 사이클에 위치를 읽은 활성 에이전트 수
	UPROPERTY(BlueprintReadOnly, Category = "FF Zone System|Population")
	int32 SampledAgentCount = 0;

	// 어느 존에도 속하지 않은 활성 에이전트 수. 스폰 지점이 존 밖이면 이 값이 크게 잡힌다
	UPROPERTY(BlueprintReadOnly, Category = "FF Zone System|Population")
	int32 OutsideZoneCount = 0;

	// 표본이 실제로 반영된 존 수
	UPROPERTY(BlueprintReadOnly, Category = "FF Zone System|Population")
	int32 ReportedZoneCount = 0;

	// 플러그인이 거부한 존 수. 0이 아니면 그 사이클의 경과 시간은 다음 보고로 이월된다
	UPROPERTY(BlueprintReadOnly, Category = "FF Zone System|Population")
	int32 RejectedZoneCount = 0;

	// 존별 인원. 등록된 존은 인원 0이어도 반드시 들어 있다
	UPROPERTY(BlueprintReadOnly, Category = "FF Zone System|Population")
	TMap<FName, int32> PopulationByZoneId;
};

/**
 * Crowd(이정원)가 아는 "지금 어디에 몇 명"을 FF_ZoneSystem(한광훈)의 존 인구 표본으로 옮긴다.
 *
 * ### 왜 별도 서브시스템인가
 * 두 시스템 모두 이 변환의 주인이 아니다. Crowd는 존 경계를 모르고, ZoneSystem은 에이전트를 모른다.
 * 어느 한쪽에 코드를 넣으면 그쪽이 상대의 개념을 알아야 하므로, 기존 Queue·Stage 브리지와 같은
 * 위치에 변환 전용 subsystem을 둔다. Crowd 쪽은 public getter만 읽고 한 줄도 고치지 않는다.
 *
 * ### 왜 프레임 시간이 아니라 시뮬레이션 시계인가
 * `UFF_CrowdSimulationSubsystem`은 배속과 `MaxStepsPerFrame` 클램프를 거쳐 고정 스텝을 소비하므로
 * 실시간 DeltaTime과 시뮬레이션이 진행한 시간이 다르다. 밀도 단계 승격은 유지 "시간"으로 판정하니
 * 실시간을 넘기면 배속에서 승격 시점이 어긋난다. 그래서 `GetSimTimeSeconds()`의 차분만 쓴다.
 *
 * ### 끄는 방법
 * `FF.Zone.PopulationReporting 0` — 결선 전후를 같은 빌드에서 비교할 수 있어야 한다.
 * 보고 주기는 `FF.Zone.PopulationIntervalSeconds`(기본 1초)다. 밀도 유지 시간이 5초·3초이므로
 * 그보다 충분히 짧아야 승격 판정에 표본이 여러 개 들어간다.
 */
UCLASS()
class FESTAFIESTA_API UFF_ZonePopulationBridgeSubsystem final : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/**
	 * 간격을 기다리지 않고 지금 한 사이클을 보고한다. 콘솔 명령과 테스트용이다.
	 *
	 * @return 표본이 하나 이상의 존에 반영됐으면 true. 전부 거부되면 false이고 경과 시간은 이월된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FF Zone System|Population")
	bool ReportNow();

	/** 마지막으로 성립한 보고 사이클의 관찰값. 아직 보고 전이면 CycleIndex가 0이다. */
	UFUNCTION(BlueprintPure, Category = "FF Zone System|Population")
	FFF_ZonePopulationCycle GetLastCycle() const { return LastCycle; }

	/**
	 * 슬롯 위치를 존별 인원으로 집계하는 순수 함수. Crowd 없이 테스트할 수 있도록 분리했다.
	 *
	 * @param RegisteredZoneIds        보고 대상 존 전체. 인원 0인 존도 결과 맵에 0으로 들어간다
	 * @param SlotsByAgentIndex        풀 전체. 비활성 슬롯은 자리를 유지한 채 무시된다
	 * @param InOutLastZoneBySlot      슬롯별 직전 존. 히스테리시스 힌트로 재사용하고 이번 결과로 갱신한다
	 * @param OutPopulationByZoneId    존별 인원. 호출 시 초기화된다
	 * @param OutOutsideZoneCount      어느 존에도 속하지 않은 활성 에이전트 수
	 */
	static void TallyByZone(
		const UFFZSWorldSubsystem& ZoneSystem,
		const TArray<FName>& RegisteredZoneIds,
		TArrayView<const FFF_ZoneAgentSlot> SlotsByAgentIndex,
		TArray<FName>& InOutLastZoneBySlot,
		TMap<FName, int32>& OutPopulationByZoneId,
		int32& OutOutsideZoneCount);

private:
	/** 한 사이클을 실행한다. 실패하면 DeltaSeconds를 소비하지 않는다. */
	bool RunReportCycle(float DeltaSeconds);

	/** 활성 슬롯 위치를 SlotBuffer로 옮긴다. Crowd에서 읽는 유일한 지점이다. */
	void CaptureAgentSlots();

	UPROPERTY(Transient)
	TObjectPtr<UFFZSWorldSubsystem> ZoneSystem;

	UPROPERTY(Transient)
	TObjectPtr<UFF_CrowdSimulationSubsystem> Crowd;

	// 직전 Tick에서 읽은 시뮬레이션 시계. 차분이 이번 프레임에 시뮬레이션이 진행한 시간이다
	float LastSimTimeSeconds = 0.f;

	// 아직 보고에 반영되지 않은 시뮬레이션 경과 시간. 보고가 거부되면 여기 남아 다음 사이클로 넘어간다
	float PendingDeltaSeconds = 0.f;

	// 슬롯별 직전 존. 인덱스는 Crowd의 AgentIndex 공간이다
	TArray<FName> LastZoneBySlot;

	// 사이클마다 재사용하는 집계 입력 버퍼. 매번 새로 할당하지 않는다
	TArray<FFF_ZoneAgentSlot> SlotBuffer;

	UPROPERTY(Transient)
	FFF_ZonePopulationCycle LastCycle;

	// 첫 보고에서 한 번만 요약 로그를 남긴다. 매 초 로그를 뱉으면 다른 로그가 묻힌다
	bool bLoggedFirstCycle = false;
};
