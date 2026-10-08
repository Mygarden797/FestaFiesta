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

namespace FFF_CrowdNav
{
	inline FVector GetAnchorProjectionExtentCm()
	{
		return FVector(300.0, 300.0, 2000.0);
	}
}

UENUM(BlueprintType)
enum class EFF_DecisionMode : uint8
{
	Random,
	Greedy,
	Full
};
/**
 * 결정이 어디서 끝났는지 세는 계수기.
 *
 * 이 계층은 실패해도 크래시하지 않고 조용히 false를 돌려주므로(= 배회로 폴백),
 * 계수기가 없으면 "Utility가 도는지 안 도는지" 자체를 알 수 없다.
 * 각 항목은 서로 다른 원인을 가리키므로 하나로 합치지 않는다.
 */
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

	/**
	 * 시설은 있는데 Active인 것이 없다.
	 *
	 * NoFacility와 나눠 세는 이유: 원인이 완전히 다르다. 이건 "짓기만 하고 개장하지 않았다"는
	 * 뜻이고, 시설 수명주기가 Install → Validated → (Open 명령) → Active이므로
	 * 배치만 해서는 후보가 되지 않는다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Decision")
	int32 NoOpenFacility = 0;

	// ZoneSystem 서브시스템이 없다. 플러그인 미로드나 잘못된 월드
	UPROPERTY(BlueprintReadOnly, Category = "Decision")
	int32 NoZoneSystem = 0;

	// 파티가 어떤 Zone에도 속하지 않고, 가까운 Zone 도 없다
	UPROPERTY(BlueprintReadOnly, Category = "Decision")
	int32 OutsideZone = 0;

	// Zone 밖이었지만 가까운 Zone 을 현재 Zone 으로 보고 결정했다(입구·존 사이 길)
	UPROPERTY(BlueprintReadOnly, Category = "Decision")
	int32 OutsideZoneUsedNearest = 0;

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

	/**
	 * 후보에 실제로 대기열 대기시간이 반영된 횟수(후보 단위 누적).
	 *
	 * Requests가 아니라 후보 개수로 세는 이유: 대기열이 붙은 시설만 반영되므로
	 * "결정은 돌았는데 대기시간은 하나도 안 붙었다"를 이 값 0으로 구분할 수 있다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Decision")
	int32 QueueWaitApplied = 0;

	/**
	 * 직전 목적지를 실패 후보로 표시한 횟수.
	 *
	 * Crowd 의 GetStuckDecisionRequests()와 짝이 맞는지로 배선을 확인한다 — 그쪽이 늘어나는데
	 * 이 값이 0이면 사유(MoveFailed)나 직전 목적지가 컨텍스트에 안 실려 오고 있다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Decision")
	int32 FailedDestinationPenalized = 0;

	/**
	 * 이동 중 인원이 0이 아니게 실린 후보 수(후보 단위 누적).
	 *
	 * QueueWaitApplied 와 같은 이유로 후보 개수로 센다 — "결정은 돌았는데 이동 중 인원은
	 * 하나도 안 붙었다"를 이 값 0으로 구분할 수 있다. Crowd 원장이 안 넘어오거나
	 * DecisionMode 가 Full 이 아니면 0이다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Decision")
	int32 InTransitApplied = 0;
};

/**
 * Crowd와 Utility AI·ZoneSystem을 잇는 조정자.
 *
 * ### 이 클래스만 양쪽을 안다
 * `FFF_UtilityDestinationSelector`와 `FFF_ZoneSystemBridge` include는 여기에만 둔다.
 * Crowd는 `FFF_OnPartyNeedsDecision` 델리게이트만 노출하고 누가 답하는지 모른다 —
 * 그래서 이 서브시스템이 없어도(또는 실패해도) Crowd는 배회로 계속 돌아간다.
 *
 * ### CrowdSystem 폴더에 있지만 Crowd가 아니다
 * 같은 폴더에 있다고 `UFF_CrowdSimulationSubsystem`이 이 헤더를 include해도 되는 것은 아니다.
 * 경계는 폴더가 아니라 include 방향으로 지켜진다 — Crowd → 조정자 include가 한 번이라도
 * 생기면 Zone·Utility 헤더가 Crowd에 전이적으로 딸려 들어오고, 그 시점에 이 설계의
 * 유일한 이점(조정자를 떼어내도 Crowd가 도는 것)이 사라진다.
 *
 * ### 흐름
 * ```
 * Crowd: 목적지가 필요하다 (도착/스폰)
 *   → ResolveZoneAtWorldLocation  현재 Zone
 *   → BuildReadModel              시설 스냅샷 (프레임당 1회 캐시)
 *   → BuildCandidates             Zone·거리로 최대 12개
 *   → Choose                      6항 점수로 1개
 *   → TryParseDestinationId       FName → FGuid
 *   → ApproachWorldTransform      Entry[0] 월드 좌표
 * Crowd: 그 좌표로 이동
 * ```
 *
 * ### 개체 변주는 Crowd 가 실어 보낸다
 * 취향강도·신규성선호·개인 밀집임계는 이 조정자가 계산하지 않는다. Crowd 가 스폰 때
 * `ResolveWithGroupSharing` 으로 확정해 `FFF_PartyDecisionContext` 에 담아 넘긴다.
 *
 * 취향강도만 예외적으로 이 클래스에서 곱한다(`GetScaledPreferenceVector`). 
 * 정규화 클램프 전에 벡터에 걸어야 하는 값이라 Choose 인자로 넘길 수 없기 때문이다.
 *
 * ### 대기열·서비스·퇴장
 * `ApproachWorldTransform`은 대기열 가입 전 접근점이다. 가입 후의 Slot 추종·Service·
 * Exit 는 Crowd 와 Queue 브리지가 맡고, 이 조정자는 관여하지 않는다.
 * 줄에서의 이탈(reneging)도 Crowd 쪽 판정이며, 그 결과가
 * `EFF_DecisionReason::QueuePatienceExpired` 요청으로 여기 되돌아온다.
 */
UCLASS()
class FESTAFIESTA_API UFF_CrowdDecisionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	const FFF_CrowdDecisionStats& GetStats() const { return Stats; }

	// BP·위젯용. 콘솔은 위의 const 참조 버전을 쓴다
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

	/**
	 * 결정 연결을 끊거나 다시 잇는다. 끊으면 Crowd가 배회로만 돌아간다.
	 *
	 * ### 왜 필요한가 — 대조군 없이는 인과가 증명되지 않는다
	 * "사람들이 부스로 모인다"는 관찰만으로는 Utility가 동작하는 증거가 못 된다.
	 * 같은 시드에서 껐을 때와 켰을 때의 밀도를 비교해야 그 차이가 결정 계층 때문이라고
	 * 말할 수 있다.
	 *
	 * 델리게이트를 끊는 방식이지 플래그로 내부에서 조기 반환하는 방식이 아니다 —
	 * 그래야 "조정자가 아예 없는 상태"와 완전히 같은 경로를 타고,
	 * Crowd 쪽 폴백이 실제로 동작하는지도 같이 검증된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Decision")
	void SetDecisionEnabled(bool bEnabled);

	// 현재 이 조정자가 Crowd의 결정 델리게이트를 물고 있는지
	UFUNCTION(BlueprintPure, Category = "Decision")
	bool IsDecisionEnabled() const;

	/**
	 * Validated 상태 시설을 전부 Active로 개장하고 성공 개수를 반환한다.
	 *
	 * ### 이건 임시 도구다
	 * 시설 수명주기는 Install → Validated → (Open 명령) → Active이고 BuildCandidates는
	 * Active만 후보로 받는다. 그런데 개장 명령을 발행하는 프로덕션 코드가 없다 —
	 * 빌드 모드는 Install·Move·Remove만 발행하고, UFF_FestivalStageSubsystem이
	 * Live로 전이할 때도 개장하지 않는다.
	 *
	 * 개장 시점(축제 개장 시각인지, 설치 즉시인지, 준비시간 후인지)은 게임플레이 설계 사안이다.
	 * 정식 경로가 정해지면 **이 함수와 FF.Decision.OpenFacilities를 함께 삭제한다.**
	 *
	 * 이름에 Debug_를 붙인 이유: 데모 BP에서 부르기 쉽게 열어둔 것이므로,
	 * 프로덕션 흐름에 섞이면 개장 권한이 둘로 갈라진다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Decision|Debug")
	int32 Debug_OpenAllValidatedFacilities(int32& OutFailedCount);

#if !UE_BUILD_SHIPPING
	// Read-only candidate/nav/queue snapshot. Does not request a decision or open facilities.
	void DebugDumpStageFlow(int32 LeaderAgentIndex, FOutputDevice& Ar) const;
#endif

	/**
	 * 지역 후보 생성 상한. 공연 중 전역 메인 무대 후보는 이 상한과 별도로 더한다.
	 *
	 * 프레임 예산이 여기 걸려 있다 — (프레임당 결정 수 × 이 값)이 실제 계산량이다.
	 * 0 이하면 상한 없음이므로 시설이 늘어날수록 결정 비용이 선형으로 증가한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decision")
	int32 MaxCandidates = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decision")
	EFF_DecisionMode DecisionMode = EFF_DecisionMode::Full;

	/**
	 * 후보 거리를 존 앵커 **보행거리**로 잰다. 끄면 직선거리다. ★ 기본값은 false다.
	 *
	 * 켜기 전에 `distance_ref_meters` 재보정이 선행 조건이다 — 보행거리는 직선보다 크므로
	 * 기존 정규화 기준으로는 포화되어 먼 시설의 거리 항이 전부 같은 값이 된다.
	 * 전환은 FF.Decision.WalkDistance, 상태 확인은 FF.Decision.Stats.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decision|Distance")
	bool bEnableWalkDistances = false;

	// 행렬에 등록된 존 수. 2개 미만이면 행렬이 할 일이 없어 직선거리로 돌아간다
	UFUNCTION(BlueprintPure, Category = "Decision|Distance")
	int32 GetWalkDistanceZoneCount() const { return WalkDistances.GetZoneCount(); }

	// 존 인접 그래프에는 이웃인데 NavMesh로는 경로가 없는 쌍의 수. 0이 아니면 배치와 지형이 어긋났다
	UFUNCTION(BlueprintPure, Category = "Decision|Distance")
	int32 GetWalkDistanceMismatchCount() const { return WalkDistanceMismatchCount; }

	// EntryExitAnchors가 저작되지 않아 존 중심을 대표점으로 쓴 존 수
	UFUNCTION(BlueprintPure, Category = "Decision|Distance")
	int32 GetUnauthoredAnchorCount() const { return WalkDistanceUnauthoredAnchors; }

	// 앵커가 NavMesh에 투영되지 않은 존 수. 그 존이 걸린 쌍은 직선거리로 폴백한다
	UFUNCTION(BlueprintPure, Category = "Decision|Distance")
	int32 GetAnchorProjectionFailureCount() const { return WalkDistanceProjectionFailures; }

	// 앵커 투영 최대 낙차(cm). 크면 존 액터가 지면에서 떠 있다
	UFUNCTION(BlueprintPure, Category = "Decision|Distance")
	float GetMaxAnchorProjectionDropCm() const { return WalkDistanceMaxProjectionDropCm; }

	// NavMesh 재굽기가 안 끝나 행렬 빌드를 미룬 횟수. 배치 변경 직후 몇 번 오르는 것이 정상이고,
	// 계속 오르기만 하면 nav 빌드가 끝나지 않는다는 뜻이다(행렬이 영구히 낡은다)
	UFUNCTION(BlueprintPure, Category = "Decision|Distance")
	int32 GetWalkDistanceNavBuildDeferrals() const { return WalkDistanceNavBuildDeferrals; }

	// 지금 메인 무대가 후보로 열려 있는가. BuildCandidates 에 그대로 넘어가는 값이다.
	UFUNCTION(BlueprintPure, Category = "Decision|Stage")
	bool IsMainStageOpen() const;

	// Stage 없는 월드의 상시 개방과 실제 메인 공연 중 전역 안내를 구분한다.
	bool IsGlobalMainStageDiscoveryActive() const;

	/**
	 * 무대가 열리는 순간 재결정을 밀어 넣을 파티 수 상한. 0 이하면 상한 없음.
	 *
	 * 기본 0: 전체 적격 파티에 알린다. 실제 결정은 Crowd.MaxDecisionsPerFrame 예산으로 나눈다.
	 * 양수는 디버깅용 제한이며 나머지 파티를 후속 배치로 요청하지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decision|Stage")
	int32 MaxPartiesRedecidedOnStageOpen = 0;

private:
	friend struct FFF_CrowdProgramDecisionTestAccess;
	// UFF_FestivalStageSubsystem 은 GameInstance 스코프라 포인터를 캐시하면 수명이 어긋난다
	// 공연 시간표 게이팅 — Stage 델리게이트로 불리언만 유지한다.
	UFUNCTION()
	void HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage);

	UFUNCTION()
	void HandleMainProgramStarted(FString EventLabel);

	UFUNCTION()
	void HandleMainProgramEnded(FString EventLabel);

	// Crowd의 OnPartyNeedsDecision에 물리는 함수. false면 Crowd가 배회로 폴백한다
	bool HandlePartyNeedsDecision(const FFF_PartyDecisionContext& Context, FFF_PartyDecisionResult& OutResult);
	EFF_PartyFallbackAction ResolveFallbackAction(const FFF_PartyDecisionContext& Context);
	// (아키타입, 동반유형) 조합의 개체 변주가 안 걸린 기준 취향 벡터. 조합 수가 유한해서 캐시된다
	const TMap<FName, float>& GetPreferenceVector(FName ArchetypeId, FName CompanionType);

	/**
	 * 기준 취향 벡터에 이 개체의 preference_strength_multiplier 를 곱한 것.
	 *
	 * 곱셈을 Choose 안에서 하지 않고 벡터에 미리 거는 이유: Choose 는 성분을
	 * `Clamp(값 / preference_vector_scale_ref, -1, 1)` 로 정규화한다. 클램프가 걸린 뒤에
	 * 곱하면 포화된 성분에서 배수가 먹지 않아, 취향이 강한 개체일수록 오히려 변주가
	 * 사라진다. 클램프 전에 곱해야 "취향이 강하다"가 정규화 한계까지 반영된다.
	 *
	 * 배수가 1 에 가까우면 캐시된 기준 벡터를 그대로 돌려준다 — 복사를 아끼려는 것이다.
	 * 그 외에는 ScaledPreferenceVector 에 채워 돌려주므로, 반환된 참조는 다음 호출까지만
	 * 유효하다. 결정 하나를 처리하는 동안만 쓰고 보관하지 말 것.
	 */
	const TMap<FName, float>& GetScaledPreferenceVector(
		FName ArchetypeId, FName CompanionType, float PreferenceStrengthMultiplier);

	/**
	 * 이번 프레임의 ZoneSystem 스냅샷. 프레임당 한 번만 만든다.
	 *
	 * BuildReadModel은 전체 존·시설을 ID 기준으로 정렬해 값 복사하는 함수다.
	 * 결정마다 부르면 (프레임당 결정 수 × 시설 수) 복사가 돌아, 예산을 둔 의미가 사라진다.
	 */
	const FFFZSReadModel& GetReadModelForThisFrame();

	UFFZSWorldSubsystem* GetZoneSystem() const;

	/**
	 * 보행거리 행렬을 필요할 때만 다시 짓는다. 결정 경로에서 매번 부르되 대개 즉시 반환한다.
	 *
	 * 재구축 조건은 ZoneSystem의 WorldRevision 변화뿐이다 — N^2 NavMesh 질의라
	 * 결정마다 지으면 프레임 예산이 통째로 날아간다. 끄면 행렬을 비우고 계수기도 되돌린다.
	 */
	void EnsureWalkDistanceMatrix(const UFFZSWorldSubsystem& ZoneSystem, const FFFZSReadModel& ReadModel);

	// Zone 밖 위치에서 MaxOutsideZoneFallbackMeters 안의 가장 가까운 Zone(액터 원점 기준). 없으면 NAME_None
	static FName FindNearestZoneId(const UFFZSWorldSubsystem& ZoneSystem, const FFFZSReadModel& ReadModel, const FVector& WorldLocation);
	static constexpr float MaxOutsideZoneFallbackMeters = 200.f;

	// 게이팅이 도는 구간인가. Live 안에서만 켠다.
	bool bMainStageGatingActive = false;

	// 게이팅이 도는 동안 무대가 열려 있는가. 공연 시작~종료 사이에만 true 다.
	bool bMainStageOpen = false;

	// 무대의 일반 후보 개방 여부와 공연 중 전역 발견은 별개다. 무대는 Live 내내 열고,
	// 전역 발견/강제 집결만 메인 공연 구간에 한정한다.
	bool bMainProgramActive = false;

	// 이번 Live 에서 공연 시작 신호를 한 번이라도 받았는가.
	// Live 를 빠져나갈 때까지 false 면 표에 메인 공연이 저작되지 않은 것이다 — Warning 을 낸다.
	bool bSawMainProgramThisLive = false;

	FFFZSReadModel CachedReadModel;

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

	/**
	 * GetScaledPreferenceVector 의 작업 버퍼. 개체별 배수는 연속값이라 캐시할 수 없어서
	 * 결정마다 다시 채우는데, 멤버로 두면 Reset 이 용량을 유지하므로 초반 이후 재할당이 없다.
	 * 카테고리 수만큼(10개 남짓)의 작은 맵이다.
	 */
	TMap<FName, float> ScaledPreferenceVector;

	int32 NoChoiceLeavePatienceRounds = 5;
	int32 LeaveByStayExpiredCount = 0;
	int32 LeaveByNoChoicePatienceCount = 0;
	int32 RelocateRequestedCount = 0;

	// --- 보행거리 -------------------------------------------------------------------------
	// USTRUCT가 아니므로 UPROPERTY로 만들 수 없다. GC가 추적할 대상도 없는 순수 계산 캐시다.
	FFF_WalkDistanceMatrix WalkDistances;

	// 행렬을 마지막으로 지은 시점의 ZoneSystem WorldRevision. -1은 "아직 짓지 않았다"
	int64 WalkDistanceBuiltRevision = -1;

	// 마지막 빌드의 진단값. 조용히 직선거리로 폴백하는 것이 가장 위험하므로 전부 남긴다
	int32 WalkDistanceMismatchCount = 0;
	int32 WalkDistanceUnauthoredAnchors = 0;
	int32 WalkDistanceProjectionFailures = 0;
	float WalkDistanceMaxProjectionDropCm = 0.f;

	// nav 빌드가 안 끝나 빌드를 미룬 누적 횟수. 위 진단값과 달리 빌드마다 초기화되지 않는다
	int32 WalkDistanceNavBuildDeferrals = 0;
};
