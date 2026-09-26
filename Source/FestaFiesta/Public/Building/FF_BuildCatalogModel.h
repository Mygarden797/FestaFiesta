// FF_BuildCatalogModel.h
// 역할: 시설 목록을 화면에 표시할 데이터로 바꾸는 공통 규칙을 선언한다.
// 구현은 .cpp에 두어 비용표나 프로그램 규칙을 수정할 때 모든 화면을 다시 컴파일하지 않게 한다.

#pragma once

#include "Building/FF_BuildCatalogTypes.h"

class UDataTable;
struct FFF_ProgramCard;
struct FFF_RequiredFacilitiesStatus;

namespace FFBuildCatalogModel
{
	// 이 시설에서 열 수 있는 프로그램의 수와 이름을 갱신한다. 이전 표시는 먼저 지운다.
	FESTAFIESTA_API void ApplyCompatiblePrograms(
		FFF_BuildCatalogEntry& Entry, const TArray<FFF_ProgramCard>& Catalog);

	// 축제에서 요구하는 설치 개수와 현재 충족 여부를 목록에 표시한다.
	FESTAFIESTA_API void ApplyRequiredFacilities(
		TArray<FFF_BuildCatalogEntry>& Entries, const FFF_RequiredFacilitiesStatus& Status);

	// 비용표를 읽어 설치비와 철거 환급액을 계산한다. 실패하면 이전 금액을 무효화하고 이유를 돌려준다.
	FESTAFIESTA_API bool ApplyFacilityCosts(
		FFF_BuildCatalogEntry& Entry,
		const UFFZSFacilityDefinition* Definition,
		const UDataTable* CostTable,
		float DemolitionRefundRate,
		FString& OutFailureReason);

	// 대소문자를 구분하는 일정한 순서다. 앞이면 음수, 같으면 0, 뒤면 양수를 돌려준다.
	FESTAFIESTA_API int32 CompareStrings(const FString& A, const FString& B);

	// 대분류, 소분류, 표시 이름, 시설 ID 순서로 두 항목을 비교한다.
	FESTAFIESTA_API bool EntrySortsBefore(const FFF_BuildCatalogEntry& A, const FFF_BuildCatalogEntry& B);

	// 비교에 쓸 문자열을 한 번씩 준비한 뒤 목록을 정렬한다. 입력 배열의 순서가 변경된다.
	FESTAFIESTA_API void SortEntries(TArray<FFF_BuildCatalogEntry>& Entries);

	// 정렬된 항목을 한 번 순회해 대분류 > 소분류 > 시설 목록으로 묶는다.
	FESTAFIESTA_API void BuildCategoryGroups(
		const TArray<FFF_BuildCatalogEntry>& SortedEntries,
		TArray<FFF_BuildCatalogCategoryGroup>& OutGroups);

	// 새 요청의 번호를 발급한다. 번호가 바뀌면 늦게 도착한 이전 로드 결과는 무시한다.
	inline uint64 BeginNextRequest(uint64& InOutGeneration)
	{
		++InOutGeneration;
		// 정수가 한 바퀴 돌아도 0은 "아직 요청하지 않음"이라는 의미로 남긴다.
		if (InOutGeneration == 0)
		{
			++InOutGeneration;
		}
		return InOutGeneration;
	}

	// 전달받은 로드 결과가 가장 최근 요청의 것인지 확인한다.
	inline bool IsCurrentRequest(const uint64 CallbackGeneration, const uint64 ActiveGeneration)
	{
		return CallbackGeneration == ActiveGeneration;
	}
}
