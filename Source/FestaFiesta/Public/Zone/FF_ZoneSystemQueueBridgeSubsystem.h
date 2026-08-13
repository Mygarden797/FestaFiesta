// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSExternalPolicy.h"
#include "Queue/FF_QueueTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_ZoneSystemQueueBridgeSubsystem.generated.h"

class AActor;
class AFFZSFacilityActor;
class UFF_QueueSubsystem;
class UFFZSWorldSubsystem;

/**
 * FF_ZoneSystem 시설을 프로젝트 UFF_QueueSubsystem에 등록하는 단방향 호환 브리지다.
 *
 * 권한 구분:
 * - FF_ZoneSystem: 시설 ID·배치·설정·설치/이동/개폐 수명주기
 * - UFF_QueueSubsystem: 프로젝트의 Party 단위 실제 대기열과 로그
 *
 * 두 구현의 런타임 Queue 항목은 서로 복사하지 않는다. 같은 운영 시설에 대해 프로젝트 코드는
 * 이 브리지의 TryJoinZoneFacility façade만 사용해야 하며, FFZS의 Agent Queue 참가 API를 함께
 * 호출하면 안 된다. 브리지는 Navigation/Safety의 Blocked 상태를 권한 잠금으로 가로채지 않고,
 * 계약 밖의 이중 점유가 감지되면 프로젝트 Queue만 fail-closed 한다.
 */
UCLASS()
class FESTAFIESTA_API UFF_ZoneSystemQueueBridgeSubsystem final
	: public UTickableWorldSubsystem
	, public IFFZSExternalPolicy
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Zone Facility GUID에 배정한 프로젝트 Queue의 int32 ID. 아직 본 적 없는 GUID면 INDEX_NONE이다. */
	UFUNCTION(BlueprintPure, Category = "FF Zone System|Integration|Queue")
	int32 FindQueueFacilityId(FGuid ZoneFacilityId) const;

	/** 프로젝트 Queue ID에 대응하는 Zone Facility GUID. 브리지 소유 ID가 아니면 무효 GUID다. */
	UFUNCTION(BlueprintPure, Category = "FF Zone System|Integration|Queue")
	FGuid FindZoneFacilityId(int32 QueueFacilityId) const;

	/**
	 * FFZS Snapshot 복원처럼 OnWorldChanged가 발생하지 않은 뒤 전체 등록을 다시 만든다.
	 * 기존 브리지 Queue를 재구성하므로 진행 중 Party가 없는 복원 경계에서만 호출해야 한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FF Zone System|Integration|Queue",
		meta = (DisplayName = "Refresh Queue Registrations After Zone Restore"))
	int32 RefreshQueueRegistrations();

	/**
	 * Zone Snapshot을 복원하고 Queue 등록과 FestivalStage phase를 같은 호출 안에서 다시 맞춘다.
	 * FFZS Snapshot에는 프로젝트 Party Queue가 없으므로 Party가 남아 있으면 복원을 거절한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FF Zone System|Integration|Save")
	bool RestoreZoneWorldSnapshotAndSynchronize(
		const FFFZSWorldSnapshot& Snapshot,
		TArray<FText>& OutErrors);

	/**
	 * Zone GUID 시설에 Party 가입을 요청하는 프로젝트의 단일 진입점이다.
	 * FacilityId와 포기 제한은 Zone 사양에서 덮어쓰며, 비활성·drain 시설은 새 가입을 거절한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FF Zone System|Integration|Queue")
	FFF_QueueJoinResponse TryJoinZoneFacility(FGuid ZoneFacilityId, FFF_JoinRequest Request);

	UFUNCTION(BlueprintCallable, Category = "FF Zone System|Integration|Queue")
	bool LeaveZoneFacilityQueue(FGuid ZoneFacilityId, int32 PartyId);

	UFUNCTION(BlueprintCallable, Category = "FF Zone System|Integration|Queue")
	void NotifyZoneFacilityPartyArrived(FGuid ZoneFacilityId, int32 PartyId);

	UFUNCTION(BlueprintPure, Category = "FF Zone System|Integration|Queue")
	FFF_QueueState GetZoneFacilityQueueState(FGuid ZoneFacilityId) const;

	/** FFZS 설정을 Legacy Queue의 동일 장기 처리율 설정으로 변환한다. */
	static FFF_QueueConfig ToProjectQueueConfig(
		const FFFZSQueueRules& Rules,
		const FVector& ExitLocation);

	/** Party 개인 제한과 Zone 시설 제한 중 더 이른 양수 포기 시간을 선택한다. */
	static float ToProjectMaxWaitSeconds(float RequestedSeconds, float ZoneAbandonSeconds);

	// IFFZSExternalPolicy: Move/Remove 전에 프로젝트 Party Queue 점유도 함께 검사한다.
	virtual FName GetFFZoneSystemPolicyName_Implementation() const override;
	virtual EFFZSPolicyDomain GetFFZoneSystemPolicyDomain_Implementation() const override;
	virtual FFFZSPolicyDecision PreValidateFFZoneSystemChange_Implementation(
		const FFFZSPolicyContext& Context) const override;
	virtual FFFZSPolicyDecision ApplyTentativeFFZoneSystemChange_Implementation(
		const FFFZSPolicyContext& Context) override;
	virtual void CommitFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context) override;
	virtual void RollbackFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context) override;

private:
	UFUNCTION()
	void HandleZoneWorldChanged(const FFFZSWorldChangeEvent& Event);

	int32 FindOrAllocateQueueFacilityId(FGuid ZoneFacilityId);
	bool RegisterOrRefreshFacility(AFFZSFacilityActor& Facility);
	void UnregisterFacility(FGuid ZoneFacilityId);
	void HandleActorSpawned(AActor* Actor);
	void HandleActorDestroyed(AActor* Actor);
	bool HasObservedWorldChanged() const;
	void CacheObservedWorld();
	void FinishDrainedQueues();
	void CancelDrainsThatCannotAdvance();
	void EnforceProjectQueueAuthority();
	bool CanProjectQueuesAdvanceNaturally() const;

	UPROPERTY(Transient)
	TObjectPtr<UFFZSWorldSubsystem> ZoneSystem;

	UPROPERTY(Transient)
	TObjectPtr<UFF_QueueSubsystem> QueueSystem;

	// 제거 후에도 매핑을 tombstone처럼 유지해 같은 GUID의 int32 ID가 실행 중 바뀌지 않게 한다.
	TMap<FGuid, int32> ZoneToQueueFacilityIds;
	TMap<int32, FGuid> QueueToZoneFacilityIds;
	/** 실제로 이 브리지가 등록한 ID만 삭제·재구성하기 위한 소유권 집합이다. */
	TSet<int32> ActiveBridgeQueueFacilityIds;
	/** 일반 Close 뒤 기존 Party만 자연 처리 중인 Queue다. */
	TSet<int32> DrainingQueueFacilityIds;
	/** Party façade가 실제 항목을 보유해 프로젝트 Queue가 런타임 권위인 시설이다. */
	TSet<int32> ProjectQueueAuthorityFacilityIds;
	/** 조용한 Snapshot 복원도 같은 GUID의 Actor 세대 교체로 감지한다. */
	TMap<FGuid, TWeakObjectPtr<AFFZSFacilityActor>> ObservedFacilityActors;
	int64 NextQueueFacilityId = -2;
	int64 LastObservedWorldRevision = INDEX_NONE;
	FDelegateHandle ActorSpawnedHandle;
	FDelegateHandle ActorDestroyedHandle;
	bool bActorGenerationCheckPending = false;
	bool bQueueEventsNeedFlush = false;
	bool bPolicyRegistered = false;
};
