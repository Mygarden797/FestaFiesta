#pragma once

#include "CoreMinimal.h"
#include "Economy/FF_FestivalEconomyTypes.h"

/**
 * 설치 시설별(인스턴스 GUID) 매출 장부. 순수 값 타입이다.
 *
 * FFF_DayLedger 의 부스별 표는 FacilityDefinitionId(종류) 단위라 같은 종류의 부스 여러 개가
 * 한 줄로 합쳐진다. 시설 정보 패널은 "이 시설 한 곳이 얼마를 벌었나"를 묻기 때문에
 * 인스턴스 GUID 로 따로 쌓는다. 하루 경계에서 오늘 값만 0 으로 돌아가고 누적은 남는다.
 *
 * 매출의 원천은 FFF_FestivalFunds·FFF_DayLedger 와 같은 호출(RecordRevenue)이라 세 장부의 합은
 * 항상 맞는다 — 여기서 따로 계산하는 값은 없다.
 */
struct FESTAFIESTA_API FFF_FacilityRevenueBook
{
	// 시설 한 곳에 매출을 귀속시킨다. 무효 GUID 나 0 이하 금액은 기록하지 않고 false 를 돌려준다.
	// int32 상한은 포화한다 (FFF_FestivalFunds 와 같은 규칙).
	bool Record(const FGuid& FacilityInstanceId, int32 Amount);

	// 하루 경계에서 호출. 오늘 값만 0 이 되고 누적·누적 건수는 유지된다.
	void BeginNewDay();

	// 새 축제. 전부 비운다.
	void Reset();

	// 기록이 없으면 false 이고 OutRevenue 는 기본값(전부 0)이다.
	bool Find(const FGuid& FacilityInstanceId, FFF_FacilityInstanceRevenue& OutRevenue) const;

	int32 Num() const { return Entries.Num(); }

	// 저장·복원용. 이어하기 뒤에도 시설 정보 패널의 누적 매출이 이어지게 한다.
	void Export(TArray<FGuid>& OutIds, TArray<FFF_FacilityInstanceRevenue>& OutRevenues) const;
	void Import(const TArray<FGuid>& InIds, const TArray<FFF_FacilityInstanceRevenue>& InRevenues);

private:
	TMap<FGuid, FFF_FacilityInstanceRevenue> Entries;
};
