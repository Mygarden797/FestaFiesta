// FF_FacilitySynergyRulePolicy.h
// 역할: 수익 계산과 시설 도감이 동일한 조합 데이터·정렬·시설 자격 기준을 사용하도록 묶는다.
#pragma once

#include "Building/Synergy/FF_FacilitySynergyEvaluator.h"
#include "FF_FFZSTypes.h"

class UDataTable;

namespace FFFacilitySynergyRulePolicy
{
	// 데이터표의 유효한 행만 복사한다. 잘못된 형식/행은 제외하고 협업자가 고칠 수 있는 이유를 남긴다.
	TArray<FFF_FacilitySynergyRule> ReadTable(const UDataTable* Table, TArray<FString>& OutWarnings);
	// RuleIndex가 도감과 평가기에서 같은 행을 가리키도록 필터와 정렬 순서를 공유한다.
	void Normalize(TArray<FFF_FacilitySynergyRule>& Rules);
	// 영업 중단 시설도 설치된 배치의 조합 효과는 유지한다. 철거·계획·재배치 대상은 제외한다.
	bool IsEligibleFacility(const FFFZSFacilityReadModel& Facility);
}
