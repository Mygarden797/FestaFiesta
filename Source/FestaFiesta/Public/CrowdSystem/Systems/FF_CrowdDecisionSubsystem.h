// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"
#include "FF_FFZSTypes.h"
#include "NPC/Integration/FF_WalkDistanceMatrix.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_CrowdDecisionSubsystem.generated.h"
class UDataTable;
class UFFZSWorldSubsystem;

UENUM(BlueprintType)
enum class EFF_DecisionMode : uint8
{
	Random,
	Greedy,
	Full
};
// 결정이 어디서 끝났는지 세는 계수기.
//
// 이 계층은 실패해도 조용히 false를 돌려주므로(배회로 폴백) 계수기가 없으면
// Utility가 도는지 안 도는지 자체를 알 수 없다.
// 각 항목이 서로 다른 원인을 가리키므로 하나로 합치지 않는다.
USTRUCT(BlueprintType)
struct FFF_CrowdDecisionStats
{
	GENERATED_BODY()

	// 요청 총수. Crowd의 ProcessDecisions가 부른 횟수다
	UPROPERTY(BlueprintReadOnly, Category = "Decision")
	int32 Requests = 0;

	// 시설이 아직 하나도 없다. 인게임에 짓기 전에는 이게 전부다 — 정상 상태
	UPROPERTY(BlueprintReadOnly, Category = "Decision")
	int32 NoFacility = 0;

	// 시설은 있는데 Active인 것이 없다.
	UPROPERTY(BlueprintReadOnly, Category = "Decision")
	int32 NoOpenFacility = 0;

	// ZoneSystem 서브시스템이 없다. 플러그인 미로드나 잘못된 월드
	UPROPERTY(BlueprintReadOnly, Category = "Decision")
	int32 NoZoneSystem = 0;

	// 파티가 어떤 Zone에도 속하지 않는다. Zone 경계 밖에 스폰됐다는 뜻
	UPROPERTY(BlueprintReadOnly, Category = "Decision")
	int32 OutsideZone = 0;

	// Zone은 찾았는데 후보가 0개다. 현재·인접 Zone에 열린 시설이 없다
	UPROPERTY(BlueprintReadOnly, Category = "Decision")
	int32 NoCandidate = 0;

	// Choose가 no_choice를 반환했다. 후보가 다 혼잡하거나 매력도가 낮다 — 정상 판단
	UPROPERTY(BlueprintReadOnly, Category = "Decision")
	int32 NoChoice = 0;

	// DestinationId를 FGuid로 파싱하지 못했다. 계약 위반이므로 로그를 봐야 한다
	UPROPERTY(BlueprintReadOnly, Category = "Decision")
	int32 ParseFailed = 0;

	// 고른 시설이 이번 프레임 ReadModel에 없다. 결정 도중 철거됐다
	UPROPERTY(BlueprintReadOnly, Category = "Decision")
	int32 FacilityMissing = 0;

	// Entry[0] 접근점이 파생되지 않았다. FacilityDefinition 데이터 문제
	UPROPERTY(BlueprintReadOnly, Category = "Decision")
	int32 InvalidApproach = 0;

	// 실제로 시설 좌표를 내려준 횟수. 이 값이 0이면 Utility가 한 번도 안 먹은 것이다
	UPROPERTY(BlueprintReadOnly, Category = "Decision")
	int32 Chosen = 0;
};

// Crowd와 Utility AI·ZoneSystem을 잇는 조정자. 이 클래스만 양쪽을 안다.
//
// Crowd는 OnPartyNeedsDecision 델리게이트만 노출하고 누가 답하는지 모른다 —
// 그래서 이 서브시스템이 없거나 실패해도 Crowd는 배회로 계속 돌아간다.
//
// ★ 경계는 폴더가 아니라 include 방향으로 지킨다. Crowd가 이 헤더를 include하면
//   Zone·Utility 헤더가 전이적으로 딸려 들어오고 위 이점이 사라진다.
//
// 흐름
//   ResolveZoneAtWorldLocation -> BuildReadModel -> BuildCandidates(최대 12개)
//   -> Choose -> TryParseDestinationId -> ApproachWorldTransform
//
// 아직 없는 것: 취향(ArchetypeID 미설정), Need(시간 압축비 미정), 방문 이력, Queue 대기시간.
// 그래서 Choose의 6항 중 attraction·distance·density 셋만 동작한다.
//
// ApproachWorldTransform은 대기열 가입 '전' 접근점이다. Join·Slot 추종·Service·Exit는 다음 단계다.
UCLASS()
class FESTAFIESTA_API UFF_CrowdDecisionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	const FFF_CrowdDecisionStats& GetStats() const { return Stats; }

	UFUNCTION(BlueprintPure, Category = "Decision")
	FFF_CrowdDecisionStats GetDecisionStats() const { return Stats; }

	UFUNCTION(BlueprintCallable, Category = "Decision")
	void ResetStats()
	{
		Stats = FFF_CrowdDecisionStats();
		LeaveByStayExpiredCount = 0;
		LeaveByNoChoicePatienceCount = 0;
		RelocateRequestedCount = 0;
	}
	UFUNCTION(BlueprintPure, Category = "Decision")
	int32 GetLeaveByStayExpiredCount() const { return LeaveByStayExpiredCount; }
	UFUNCTION(BlueprintPure, Category = "Decision")
	int32 GetLeaveByNoChoicePatienceCount() const { return LeaveByNoChoicePatienceCount; }
	UFUNCTION(BlueprintPure, Category = "Decision")
	int32 GetRelocateRequestedCount() const { return RelocateRequestedCount; }
	UFUNCTION(BlueprintPure, Category = "Decision")
	int32 GetNoChoiceLeavePatienceRounds() const { return NoChoiceLeavePatienceRounds; }

	UFUNCTION(BlueprintCallable, Category = "Decision")
	void SetDecisionEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Decision")
	bool IsDecisionEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "Decision|Debug")
	int32 Debug_OpenAllValidatedFacilities(int32& OutFailedCount);

	// 후보 생성 상한. 파티 하나당 이 개수만큼 점수를 계산한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decision")
	int32 MaxCandidates = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decision")
	EFF_DecisionMode DecisionMode = EFF_DecisionMode::Full;

	// 후보 거리를 직선거리 대신 **존 앵커 보행거리**로 계산한다. ★ 기본값은 false다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decision")
	bool bEnableWalkDistances = false;

	UFUNCTION(BlueprintPure, Category = "Decision")
	int32 GetWalkDistanceZoneCount() const { return WalkDistances.GetZoneCount(); }

	UFUNCTION(BlueprintPure, Category = "Decision")
	int32 GetWalkDistanceMismatchCount() const { return WalkDistanceMismatchCount; }

	int32 GetUnauthoredAnchorCount() const { return WalkDistanceUnauthoredAnchors; }
	int32 GetAnchorProjectionFailureCount() const { return WalkDistanceProjectionFailures; }
	float GetMaxAnchorProjectionDropCm() const { return WalkDistanceMaxProjectionDropCm; }

private:
	bool HandlePartyNeedsDecision(const FFF_PartyDecisionContext& Context, FFF_PartyDecisionResult& OutResult);
	EFF_PartyFallbackAction ResolveFallbackAction(const FFF_PartyDecisionContext& Context);
	const TMap<FName, float>& GetPreferenceVector(FName ArchetypeId, FName CompanionType);

	const FFFZSReadModel& GetReadModelForThisFrame();

	UFFZSWorldSubsystem* GetZoneSystem() const;

	void EnsureWalkDistanceMatrix(const UFFZSWorldSubsystem& ZoneSystem, const FFFZSReadModel& ReadModel);

	FFFZSReadModel CachedReadModel;

	// 존 앵커 보행거리 캐시. bEnableWalkDistances가 꺼져 있으면 비어 있다
	FFF_WalkDistanceMatrix WalkDistances;

	// 행렬을 지을 때의 WorldRevision. -1이면 아직 지은 적이 없다
	int64 WalkDistanceBuiltRevision = -1;

	int32 WalkDistanceMismatchCount = 0;

	// 앵커 품질(마지막 재구축 기준). 진단 전용이며 행렬 계산에는 쓰이지 않는다
	int32 WalkDistanceUnauthoredAnchors = 0;
	int32 WalkDistanceProjectionFailures = 0;
	float WalkDistanceMaxProjectionDropCm = 0.f;

	// GFrameCounter가 0에서 시작하므로 초기값을 0으로 두면 첫 프레임에 빈 스냅샷을 캐시로 오인한다
	uint64 CachedReadModelFrame = TNumericLimits<uint64>::Max();

	// 계수 CSV. 미설정이면 Choose가 코드에 박힌 기본값으로 돈다(동작은 한다)
	UPROPERTY()
	TObjectPtr<UDataTable> CachedCoefficientTable;

	FFF_CrowdDecisionStats Stats;

	// 취향 벡터 미구현 경고는 세션 1회만. 결정마다 찍으면 로그가 못 쓰게 된다
	bool bWarnedNoPreferenceVector = false;
	UPROPERTY()
	TObjectPtr<UDataTable> CachedArchetypeTable;
	UPROPERTY()
	TObjectPtr<UDataTable> CachedCompanionModifierTable;
	TMap<FName, TMap<FName, float>> PreferenceVectorCache;
	int32 NoChoiceLeavePatienceRounds = 5;
	int32 LeaveByStayExpiredCount = 0;
	int32 LeaveByNoChoicePatienceCount = 0;
	int32 RelocateRequestedCount = 0;
};
