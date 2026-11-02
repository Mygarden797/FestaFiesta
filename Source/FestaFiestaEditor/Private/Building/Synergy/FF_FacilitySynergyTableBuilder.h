// FF_FacilitySynergyTableBuilder.h
// 역할: 기존 조합 표 편집을 보존하면서 누락된 기본 행만 추가하는 에디터 도구이다.

#pragma once

#include "CoreMinimal.h"

class UDataTable;

namespace FFFacilitySynergyTableBuilder
{
/** 켜진 조합의 시설 ID가 실제 Primary Asset을 가리키는지 확인한다. 표시 이름은 제작자가 관리한다. */
bool ValidateReferencedDefinitions(const UDataTable& Table, FString& OutFailureReason);

/** 합친 결과를 먼저 검증하고 누락 행만 추가한다. 기존 행을 덮어쓰거나 에셋을 저장하지 않는다.
 * 저장·실패 시 원복은 콘솔 명령 호출부가 맡아 데이터 변경과 디스크 작업을 구분한다. */
bool AppendMissingRows(UDataTable& Destination, const UDataTable& Source,
	TArray<FName>& OutAddedRows, FString& OutFailureReason);
}
