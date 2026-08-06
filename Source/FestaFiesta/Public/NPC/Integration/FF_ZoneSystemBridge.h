// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"
#include "NPC/Destination/FF_UtilityDestinationSelector.h"

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
 * ### Zone 인접·앵커·면적 입력 계약
 *
 * `FFFZSZoneReadModel`은 `AFFZSGameplayZone`의 아래 3가지 정보를 값 복사본으로 제공한다.
 *   - `NeighborZoneIds`      — Zone 인접 그래프 (후보 필터링에 그대로 씀)
 *   - `EntryExitAnchors`     — Zone 로컬 cm 출입 기준점 (거리 행렬의 시작·끝점)
 *   - `AreaSquareMeters`     — `GetBoundaryAreaSquareMeters()`의 실제 경계 면적(㎡)
 *
 * 이 파일의 `BuildCandidates` 조립과 로컬 앵커의 월드 좌표 변환은 별도 구현 대상이다.
 *
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
	 * 인접 Zone 정보는 FFFZSZoneReadModel::NeighborZoneIds에서 읽는다.
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
	 * Restroom(Toilet)은 카테고리만으로 판정. Rest는 Fatigue와 Thirst 둘 다 해소한다
	 * (8/6 리플렛 확인 — "오아시스 존"이 그늘+음수대 복합 시설). Performance·PhotoSpot은 Stimulation만 해소.
	 * Hunger·Thirst(Booth FoodStall)·Stimulation(Booth Nightlife)은 MapToPreferenceCategory 와
	 * 같은 네이밍 컨벤션으로 FacilityDefinitionId 를 파싱해 판정한다.
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

	// FFFZSFacilityReadModel::FacilityInstanceId(FGuid) → FFF_DestinationCandidate::DestinationId(FName)
	static FName FacilityInstanceIdToDestinationId(const FGuid& FacilityInstanceId)
	{
		return FName(*FacilityInstanceId.ToString());
	}

	// 위 변환의 역방향. 실패하면(형식이 다르면) false
	// DestinationId가 애초에 시설 인스턴스가 아닌 값(예: 잘못된 입력)일 수 있어서 호출부가 확인해야 한다.
	static bool TryParseDestinationId(FName DestinationId, FGuid& OutFacilityInstanceId)
	{
		return FGuid::Parse(DestinationId.ToString(), OutFacilityInstanceId);
	}
};
