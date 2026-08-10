// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"
#include "NPC/Destination/FF_UtilityDestinationSelector.h"
#include "NPC/Party/FF_NeedState.h"

/**
 * FF_ZoneSystem(한광훈) → Utility AI(김시리) 변환 지점.
 *
 * ZoneSystem 이 가지고 있는 시설·존·큐 상태를 
 * 목적지 선택기(김시리)에서 필요한 FFF_DestinationCandidate 로 바꾸는 변환 작업을 이곳에서만 한다.
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
	 *
	 * FacilityDefinitionId 는 팀 규칙 `대분류_소분류_고유이름` 형태다(대분류 = EFFZSFacilityCategory 이름).
	 * **7개 카테고리 전부 형태를 검증하고** 위반이면 시설당 1회 Warning 을 낸다 — 지금 취향 카테고리를
	 * 실제로 갈라내는 것은 Booth 소분류뿐이지만, 다른 카테고리 오타를 방치하면 나중에 소분류별 분기를
	 * 추가할 때 조용히 빠진다. 규칙 전문과 토큰 표는 data/metadata/handoff/facility_naming_convention.md.
	 *
	 * Rest/Performance/PhotoSpot 은 소분류와 무관하게 대분류 1:1, Toilet/Info/Safety 는 취향 매칭
	 * 대상이 아니라 NAME_None. NAME_None 이면 preference 항은 0으로 계산된다
	 * (크래시 없이 "취향 없음"으로 동작).
	 */
	static FName MapToPreferenceCategory(
		EFFZSFacilityCategory ZoneSystemCategory,
		FName FacilityDefinitionId);

	/**
	 * 이 카테고리가 해소 가능한 Need. FF_NeedState.h 매핑 표를 코드로 옮긴 것.
	 *
	 * Restroom(Toilet)·Fatigue(Rest)는 대분류만으로 판정. Performance·PhotoSpot은 Stimulation만.
	 * 소분류를 봐야 하는 것 2가지:
	 *   - Booth — Hunger/Thirst(FoodStall), Thirst·Stimulation(Nightlife)
	 *   - Rest  — Thirst 는 **Cooling(무더위 쉼터)만** 해소한다 (2026-08-06 결정).
	 *             리플렛의 "오아시스 존"(그늘+음수대)이 Rest 카테고리인데 팀 규칙의 Rest 소분류
	 *             4종에 음수대 토큰이 없어서, 좌석만 있는 시설이 갈증을 푸는 것을 막으려고 좁혔다.
	 *
	 * 소분류를 못 읽으면(컨벤션 위반) Thirst 는 해소하지 않는다 — 안전한 쪽으로 뺀다.
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
