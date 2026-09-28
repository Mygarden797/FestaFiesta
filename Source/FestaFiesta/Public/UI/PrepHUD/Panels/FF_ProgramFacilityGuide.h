// FF_ProgramFacilityGuide.h
// 역할: 프로그램에 필요한 시설을 찾아 건설 목록이나 설치된 시설로 안내한다.

#pragma once

#include "CoreMinimal.h"
#include "Building/FF_BuildCatalogTypes.h"
#include "Economy/FF_FestivalEconomyTypes.h"

// 프로그램 편성 규칙과 건축목록 스냅샷으로 설치 안내를 만든다. 월드나 위젯을 생성하지 않는다.
struct FESTAFIESTA_API FFF_ProgramFacilityGuide
{
	// 카탈로그 순서를 유지한다. 허용 종류가 비어 있으면 이름 규칙이 유효한 모든 시설을 반환한다.
	static TArray<FFF_BuildCatalogEntry> FindCompatibleEntries(
		const FFF_ProgramCard& Card, const TArray<FFF_BuildCatalogEntry>& Entries);

	// 실제 건축목록과 같은 대분류·소분류 표시 이름을 사용한다. 내부 식별자는 표시하지 않는다.
	static FText GetCatalogPath(const FFF_BuildCatalogEntry& Entry);

	// Entries는 전체 건축목록이다. 설치된 시설 수가 음수면 설치 여부를 단정하지 않는다.
	// 긴 목록은 처음 세 시설과 나머지 개수로 요약하고 전체 선택지는 호출자가 표시한다.
	static FText BuildFacilityNotice(
		const FFF_ProgramCard& Card,
		const TArray<FFF_BuildCatalogEntry>& Entries,
		int32 EligibleFacilityCount);
};
