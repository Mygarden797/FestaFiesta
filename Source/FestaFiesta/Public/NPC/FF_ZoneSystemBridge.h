// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"
#include "NPC/FF_UtilityDestinationSelector.h"

/**
 * FF_ZoneSystem(한광훈) → Utility AI(김시리) 변환 지점.
 *
 * ZoneSystem 이 가지고 있는 시설·존·큐 상태를 
 * 목적지 선택기(김시리)에서 필요한 FFF_DestinationCandidate 로 바꾸는 변환 작업을 이곳에서만 한다.
 * (여러 곳에서 하면 단위 환산과 카테고리 매핑이 제각각 어긋난다.)
 *
 * ### 미해결 — 단위 (통합 시 터지는 부분)
 *
 * |			| ZoneSystem							| Utility AI				|
 * | 길이		| cm									| m (`distance_ref_meters`) |
 * | 시간		| 초 (`AverageCompletedWaitSeconds`)	| 분 (`wait_ref_minutes`)	|
 *
 * 환산은 이 파일에서만 한다.
 *
 *
 * ### 미해결 — Zone 인접·앵커·면적 관련
 *
 * `AFFZSGameplayZone`(`FF_FFZSZoneActor.h`)에 있는 아래 3가지 정보를 `FFFZSZoneReadModel`(외부 공개 스냅샷)에 추가 요청함
 *   - `NeighborZoneIds`      — Zone 인접 그래프 (후보 필터링에 그대로 씀)
 *   - `EntryExitAnchors`     — Zone 출입 기준점 (거리 행렬의 시작·끝점)
 *   - `GetBoundaryAreaSquareMeters()` — 폴리곤 신발끈 공식으로 실제 면적(㎡) 산출
 *
 * ### 미해결 — Agent ID 타입이 다르다
 *
 * ZoneSystem 큐는 `FName AgentId`, 시설은 `FGuid FacilityInstanceId` 를 쓴다.
 * Utility AI 는 `int32 StableAgentId`, 목적지는 `FName DestinationId` 를 쓴다.
 * 어느 쪽에 맞출지 정하고 변환을 이 파일에 가둔다. int32 → FName 은 매 프레임 문자열을
 * 만들면 비싸므로, 스폰 시 1회 만들어 캐시하는 형태가 될 가능성이 높다.
 */


// 시설 카테고리가 해소 가능한 FFF_NeedState 성분 플래그.
struct FFF_NeedReliefFlags
{
	bool bRelievesHunger = false;
	bool bRelievesThirst = false;
	bool bRelievesRestroom = false;
	bool bRelievesFatigue = false;
	bool bRelievesStimulation = false;
};

class FFF_ZoneSystemBridge
{
public:
	/**
	 * ZoneSystem 읽기 모델을 목적지 후보 배열로 바꾼다.
	 *
	 * 후보를 전부 넣지 않는다 — Party 수가 많아질수록 매번 전체 시설을 평가하면 프레임이 무너진다.
	 * 정확한 동시 Party 상한은 아직 실측이 없다
	 * 현재 Zone + 인접 Zone + 전역 인기 상위 K개로 걸러 보통 8~12개로 줄인다.
	 * 인접 Zone 정보는 AFFZSGameplayZone::NeighborZoneIds 에 이미 있다 
	 * (ZoneReadModel에 노출만 되면 바로 쓸 수 있다.
	 *
	 * @param ReadModel       ZoneSystem 스냅샷 (한광훈 소유)
	 * @param CurrentZoneId   이 Party 가 지금 있는 Zone
	 * @param PartySize       ExpectedWait 계산에 필요 — 4인 가족은 줄이 4칸 더 필요하다
	 */
	static TArray<FFF_DestinationCandidate> BuildCandidates(
		const FFFZSReadModel& ReadModel,
		FName CurrentZoneId,
		int32 PartySize);

	/**
	 * 시설 카테고리 → preference_vector 키. 
	 * Rest/Performance/PhotoSpot 은 1:1, 
	 * Toilet/Info/Safety 는 취향 매칭 대상이 아니라 NAME_None. 
	 * Booth 는 FacilityDefinitionId 를 "Booth_<PreferenceToken>_<이름>" 네이밍 컨벤션으로 파싱한다(.cpp 의 토큰 표 참고)
	 * — 컨벤션을 안 따르거나 토큰이 표에 없으면 NAME_None. NAME_None 이면 preference 항은 0으로 계산된다
	 * (크래시 없이 "취향 없음"으로 동작).
	 */
	static FName MapToPreferenceCategory(
		EFFZSFacilityCategory ZoneSystemCategory,
		FName FacilityDefinitionId);

	/**
	 * 이 카테고리가 해소 가능한 Need. FF_NeedState.h 매핑 표를 코드로 옮긴 것.
	 * Restroom(Toilet)·Fatigue(Rest)·Stimulation(Performance·PhotoSpot)은 카테고리만으로 판정하고,
	 * Hunger·Thirst(Booth)는 MapToPreferenceCategory 와 같은 네이밍 컨벤션으로 FacilityDefinitionId
	 * 를 파싱해 판정한다.
	 */
	static FFF_NeedReliefFlags GetNeedReliefFlags(
		EFFZSFacilityCategory ZoneSystemCategory,
		FName FacilityDefinitionId);

	/**
	 * 시설의 분당 처리율. ExpectedWait = (대기인원 + PartySize) / 이 값.
	 * FacilityDefinitionId 로 UFFZSFacilityDefinition(PrimaryDataAsset)을 Asset Manager로
	 * 조회해 QueueRules.ServiceCapacity/ServiceDurationSeconds 를 분 단위로 환산한다.
	 * 자산이 아직 로드되지 않았으면 0을 반환한다 — 호출부가 0을 "모름"으로 처리할 것.
	 */
	static float GetServiceRatePerMinute(FName FacilityDefinitionId);

	// ZoneSystem cm → Utility AI m. 환산은 여기서만 한다
	static float CentimetersToMeters(float Centimeters) { return Centimeters * 0.01f; }

	// ZoneSystem 초 → Utility AI 분. 환산은 여기서만 한다
	static float SecondsToMinutes(float Seconds) { return Seconds / 60.f; }
};
