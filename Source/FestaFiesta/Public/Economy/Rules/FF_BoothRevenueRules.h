#pragma once

#include "CoreMinimal.h"

struct FFFZSFacilitySpec;
class UDataTable;

// 부스 이용 1회의 매출·수수료 판정 용 순수 구조체
struct FESTAFIESTA_API FFF_BoothRevenueRules
{
	// 시설 ID와 같은 표의 행 → 시설 Data Asset의 개별 설정 → 축제의 소분류 기본값 순서다.
	// 명시한 0은 무료다. 표의 잘못된 행 구조체 또는 선택한 행의 음수는 0으로 막는다.
	// 매번 표를 조회하므로 이미 설치하거나 저장 후 복원한 시설도 다음 계산부터 수정값을 사용한다.
	static int32 ResolveRevenuePerUse(const FFFZSFacilitySpec& Spec, const TMap<FName, int32>& LegacyRevenueTable,
		const UDataTable* FacilityRevenueTable = nullptr);

	// 소분류 네이밍을 해석하지 않아도 매출을 조회할 수 있는 시설별 행이 있는지 확인한다.
	static bool HasFacilityRevenueRow(const FFFZSFacilitySpec& Spec, const UDataTable* FacilityRevenueTable);

	// 표에서 사용을 켠 시설별 수수료율이 우선한다. 표/행이 없거나 사용을 끄면 축제값을 사용한다.
	static float ResolveCommissionRate(const FFFZSFacilitySpec& Spec, float LegacyRate,
		const UDataTable* FacilityRevenueTable = nullptr);

	// 표의 해당 레벨 배율이 우선한다. 레벨 항목이 없으면 시설/강화 단계의 기존 배율을 사용한다.
	static float ResolveRevenueMultiplier(const FFFZSFacilitySpec& Spec, int32 Level, float LegacyMultiplier,
		const UDataTable* FacilityRevenueTable = nullptr);

	// 시설 행의 프로그램 참가비가 우선한다. 프로그램 항목이 없으면 기존 카드 참가비를 사용한다.
	static int32 ResolveProgramFeePerPerson(FName FacilityDefinitionId, FName ProgramId, int32 LegacyFee,
		const UDataTable* FacilityRevenueTable = nullptr);

	// 표에서 사용을 켠 시설별 미니게임 보상이 우선한다. 표/행이 없거나 사용을 끄면 기존 보상을 쓴다.
	static int32 ResolveMinigameRewardGold(FName FacilityDefinitionId, int32 LegacyReward,
		const UDataTable* FacilityRevenueTable = nullptr);

	// 소분류 토큰의 1회 이용 매출(Gold). 표에 없으면 0 — 무료 시설과 값이 같다.
	// 둘을 구분해야 하는 쪽은 호출부다(네이밍 위반 진단).
	static int32 FindRevenuePerUse(const TMap<FName, int32>& RevenueTable, FName MinorToken);

	// 파티 하나가 부스를 한 번 이용했을 때 플레이어에게 들어오는 수수료(Gold).
	// 인원·수수료율·시설 수익 배율까지 곱한 뒤 한 번만 반올림한다. int32 상한은 포화한다.
	static int32 ResolveCommission(int32 RevenuePerUse, int32 MemberCount, float CommissionRate, double RevenueMultiplier = 1.0);
};
