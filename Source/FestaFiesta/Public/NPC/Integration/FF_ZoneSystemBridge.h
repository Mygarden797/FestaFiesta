// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"
#include "NPC/Destination/FF_UtilityDestinationSelector.h"
#include "NPC/Integration/FF_WalkDistanceMatrix.h"
#include "NPC/Party/FF_NeedState.h"

/**
 * FF_ZoneSystem(한광훈) → Utility AI(김시리) 변환 지점.
 *
 * ZoneSystem 이 가지고 있는 시설·존·큐 상태를 
 * 목적지 선택기(김시리)에서 필요한 FFF_DestinationCandidate 로 바꾸는 변환 작업을 이곳에서만 한다.
 *
 * ### 단위 환산은 이 파일에서만 한다.
 *
 * |			| ZoneSystem							| Utility AI				|
 * | 길이		| cm									| m (`distance_ref_meters`) |
 * | 시간		| 초 (`AverageCompletedWaitSeconds`)	| 분 (`wait_ref_minutes`)	|
 *
 * 
 * ### Zone 인접·앵커·면적 입력 계약
 *
 * `FFFZSZoneReadModel`은 `AFFZSGameplayZone`의 아래 3가지 정보를 값 복사본으로 제공한다.
 *   - `NeighborZoneIds`      — Zone 인접 그래프 (후보 필터링에 그대로 씀)
 *   - `EntryExitAnchors`     — Zone 로컬 cm 출입 기준점 (거리 행렬의 시작·끝점)
 *   - `AreaSquareMeters`     — `GetBoundaryAreaSquareMeters()`의 실제 경계 면적(㎡)
 *
 * 후보 거리는 기본이 시설 대표점까지의 직선거리다. `WalkDistances`를 넘기면
 * 존 앵커 보행거리 행렬을 경유한 근사로 바뀐다 — 강·무대·펜스 건너편이 멀게 나온다.
 * 넘기지 않으면(nullptr) 직선거리 그대로이므로 기존 수치와 완전히 같다(A/B의 근거).
 *
 * ⛔ 이동을 NavMesh 경로 추종으로 바꿀 때는 이 행렬을 **반드시 같이 켜야 한다.**
 *   직선거리로 "가깝다"고 고른 시설을 이동이 크게 우회하면 결정과 이동이 어긋나고,
 *   그 상태의 혼잡은 원인 추적이 불가능하다(계획 문서 §4.2의 금지 항목).
 *
 *
 * ### 시설 거리 입력 계약
 *
 * `FFFZSFacilityReadModel::WorldLocation`은 시설 Actor의 대표 월드 지점(cm)이다.
 * `BuildCandidates`는 Party 월드 위치와의 직선거리를 계산하고 이 파일에서만 m로 환산한다.
 */


class FFF_ZoneSystemBridge
{
public:
	// !!! 실측 근거 없음 !!! - 실측하면 이 값을 계수 CSV로 옮긴다.
	// 동시 Party 수 × 후보 수가 매 재평가마다 도는 비용이라 프레임 예산이 여기 걸려 있다.
	static constexpr int32 DefaultMaxCandidates = 12;

	/**
	 * ZoneSystem 읽기 모델을 목적지 후보 배열로 바꾼다.
	 *
	 * ### 이 함수는 "점수를 매기지 않는다" — 목적지 결정은 2단계다
	 *
	 *   1단계 BuildCandidates : 전체 시설 → MaxCandidates 개.  **Zone·거리만 본다**
	 *   2단계 Choose()        : 후보 → 목적지 1개.              6항(매력·취향·Need·거리·대기·밀집) 전부 본다
	 *
	 * 1단계에서 취향·Need를 보지 않는 것은 의도다. 그것들을 보려면 후보마다 preference_vector 조회를
	 * 해야 하고, 그러면 걸러내는 비용이 점수 계산 비용과 같아져서 걸러낼 이유가 사라진다.
	 *
	 * **그래서 생기는 한계**: 상한에 걸려 잘릴 때 "멀지만 내 취향에 딱 맞는 부스"가 후보에 아예
	 * 들어오지 못한다. 2단계에서 취향을 봐도 이미 늦다. 이걸 메우는 것이 아직 미구현인
	 * "전역 인기 상위 K"(utility_ai_todo.md 2절)이다. 현재 전역 예외는 공연 중 메인 무대뿐이다.
	 *
	 * ### 좁히는 방법
	 *
	 * 현재 Zone + 인접 Zone(FFFZSZoneReadModel::NeighborZoneIds)으로 먼저 좁힌다.
	 * 이 범위에 열린 방문 시설이 없으면 연결된 먼 구역까지 탐색한다. 보행 행렬이 있으면 실제
	 * 경로가 확인된 구역만, 없으면 구역 연결 그래프에 속한 구역만 허용한다. 빈 입장 구역에서도
	 * 손님이 영업 중인 부스를 발견할 수 있고, 도달할 수 없는 구역은 새 후보로 넣지 않는다.
	 * **그 결과가 MaxCandidates 이하면 정렬조차 하지 않고 전부 돌려준다** — 자르기와 거리 우선순위는
	 * 후보가 상한을 넘을 때만 개입한다. 넘을 때의 기준은 (같은 Zone 우선 → 가까운 순 → ID 순)이고,
	 * 마지막 ID 단계는 재현성을 위한 것이다(같은 스냅샷이면 항상 같은 후보가 남아야 한다).
	 *
	 * ### 무엇을 걸러내는가
	 * (1) 닫힌 시설 — Choose()가 bIsOpen == false 를 건너뛰므로 넣어두면 상한만 잡아먹는다
	 * (2) Info·Safety 카테고리 — 취향 카테고리도 없고 해소하는 Need도 없어서 점수가 모든 NPC에게
	 *     항상 같다. 고를 이유가 생기지 않는데 상한 한 칸을 먹으므로 뺀다.
	 * (3) 공연 시간이 아닐 때의 메인 무대 — bMainStageOpen 참조.
	 *
	 * ### EstimatedWaitMinutes는 폴백이다 (이중 계산 주의)
	 *
	 * 여기서 채우는 값은 ReadModel 스냅샷 기반 근사다. Queue에 등록된 시설은
	 * `FFF_QueueWaitAdapter::ApplyToCandidates`가 권위 있는 값으로 덮어쓴다.
	 * 이 함수의 계산은 "Queue가 모르는 시설"용이므로, 여기서 Queue 모델을 다시 구현하지 말 것.
	 *
	 * ### PartySize를 받지 않는다
	 *
	 * 일행 수는 대기시간 계산(줄을 몇 칸 먹는가)에만 필요했고, 그 계산은 Queue가 소유한다 —
	 * `FFF_QueueWaitAdapter::ApplyToCandidates`의 `IncomingMemberCount`로 L3가 넘긴다.
	 * 후보를 고르는 데는 일행 수가 쓰이지 않으므로 파라미터로 받지 않는다.
	 *
	 * @param ReadModel              ZoneSystem 스냅샷
	 * @param CurrentZoneId          이 Party가 지금 있는 Zone. ReadModel에 없으면 빈 배열 + Warning
	 * @param PartyWorldLocation 거리 계산 기준이 되는 Party 월드 위치(cm)
	 * @param MaxCandidates      지역 후보 상한. 0 이하면 상한 없음. 전역 메인 무대는 별도로 추가.
	 */
	/**
	 * @param WalkDistances 존 앵커 보행거리 행렬. nullptr이면 직선거리를 쓴다(기존 거동).
	 *                      소유·갱신은 호출자(UFF_CrowdDecisionSubsystem)의 몫이다 —
	 *                      행렬을 지으려면 존 액터의 월드 트랜스폼과 NavigationSystem이 필요하고,
	 *                      이 클래스는 월드를 모르는 순수 변환기로 남겨야 테스트가 싸다.
	 *
	 * @param bMainStageOpen 지금이 메인 공연 시간인가. false면 `Performance_MainStage_SunsetMainStage` 시설을
	 *                       후보에서 뺀다.
	 *
	 *                       "공연 시간에 무대로 몰린다"(AT-016)는 셋이 같이 만든다:
	 *                         ① Stimulation 축적 — 무대를 고를 **이유**. 완만한 추세만 만든다
	 *                            (FF_NeedRules.h StimulationMinutesToLimit [특징 1])
	 *                         ② 이 게이팅 — **계단**. 직전까지 없던 고득점 후보가 갑자기 생긴다.
	 *                            선형 누적기인 Need 계수로는 만들 수 없는 유일한 불연속이다
	 *                         ③ `UFF_CrowdSimulationSubsystem::RequestRedecisionForIdleParties` —
	 *                            그 계단을 **같은 순간에** 모두에게 보이게 한다. 없으면 각자
	 *                            다음 도착 때나 무대를 보게 되어 회전율만큼 서서히 차기만 한다
	 *
	 *                       ②만으로는 계단이 관측되지 않고, ③만으로는 보여줄 계단이 없다.
	 *
	 *						[참고]
	 *                       판정 자체는 여기서 하지 않는다. 시각의 유일한 출처는
	 *                       UFF_FestivalStageSubsystem 이고 이 클래스는 월드를 모르므로,
	 *                       WalkDistances 와 같은 방식으로 호출자가 읽어서 넘긴다.
	 *
	 *                       기본값 true = 게이팅 없음(기존 거동). 안 넘기면 기존 결과와 완전히 같다.
	 *
	 *                       Performance_MainStage_SunsetMainStage만 닫는다. StreetStage,
	 *                       DanceBattle, LocalArtist 등 다른 공연 시설은 일반 지역 후보로 유지한다.
	 *                       별도로 편성된 프로그램의 예약 전용 필터는 Decision 계층에서 적용한다.
	 * @param bIncludeGlobalMainStages 실제 메인 공연 중일 때만 true. 열린 SunsetMainStage를 구역/지역 상한과
	 *                                무관하게 한 번씩 추가한다. 거리·밀집 비용과 이후 예약 필터는 유지한다.
	 *                                기본 false라 Stage 없는 월드/기존 호출자의 지역 탐색은 유지된다.
	 */
	static TArray<FFF_DestinationCandidate> BuildCandidates(
		const FFFZSReadModel& ReadModel,
		FName CurrentZoneId,
		const FVector& PartyWorldLocation,
		int32 MaxCandidates = DefaultMaxCandidates,
		const FFF_WalkDistanceMatrix* WalkDistances = nullptr,
		bool bMainStageOpen = true,
		bool bIncludeGlobalMainStages = false);

	// 시간 게이트와 전역 후보·진단이 동일한 SunsetMainStage 시설 ID 판정을 사용한다.
	static bool IsMainStageFacility(EFFZSFacilityCategory Category, FName FacilityDefinitionId);

	/**
	 * 시설 카테고리 → preference_vector 키.
	 *
	 * FacilityDefinitionId 는 팀 규칙 `대분류_소분류_고유이름` 형태다(대분류 = EFFZSFacilityCategory 이름).
	 * **7개 카테고리 전부 형태를 검증하고** 위반이면 시설당 1회 Warning 을 낸다 — 지금 취향 카테고리를
	 * 실제로 갈라내는 것은 Booth 소분류뿐이지만, 다른 카테고리 오타를 방치하면 나중에 소분류별 분기를
	 * 추가할 때 조용히 빠진다. 규칙 전문과 토큰 표는 data/metadata/request/facility_naming_convention.md.
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
	 *   - Rest  — Thirst 는 Cooling(무더위 쉼터)만 해소한다 (2026-08-06 결정).
	 *             리플렛의 "오아시스 존"(그늘+음수대)이 Rest 카테고리인데 팀 규칙의 Rest 소분류
	 *             4종에 음수대 토큰이 없어서, 좌석만 있는 시설이 갈증을 푸는 것을 막으려고 좁혔다.
	 *
	 * 소분류를 못 읽으면(컨벤션 위반) Thirst 는 해소하지 않는다 — 안전한 쪽으로 뺀다.
	 */
	static FFF_NeedReliefFlags GetNeedReliefFlags(
		EFFZSFacilityCategory ZoneSystemCategory,
		FName FacilityDefinitionId);

	/**
	 * 시설의 분당 팀 처리율(ServiceCapacity / 분 단위 팀 서비스시간).
	 * FacilityDefinitionId 로 UFFZSFacilityDefinition(PrimaryDataAsset)을 Asset Manager로 조회한다.
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
