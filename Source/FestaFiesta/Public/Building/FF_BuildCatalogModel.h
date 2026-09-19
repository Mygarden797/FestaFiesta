// FF_BuildCatalogModel.h

// 런타임 서브시스템과 자동화 테스트가 함께 사용하는 카탈로그 순수 변환 규칙을 제공한다.
// 흐름: 비용 투영 -> 결정적 정렬 -> 인접 항목 그룹화 -> 비동기 요청 세대 비교.
// 불변식: 입력 에셋은 변경하지 않으며 같은 입력에는 같은 항목 순서와 그룹 구조를 반환한다.
// 실패/성능: 무효 비용은 이유를 반환하고, 정렬 뒤 그룹은 한 번의 선형 순회로 구축한다.

#pragma once

#include "Building/FF_BuildCatalogTypes.h"
#include "Economy/FF_FacilityCostResolver.h"
#include "FF_FFZSFacilityDefinition.h"
#include "Festival/FF_RequiredFacilityTypes.h"

class UDataTable;

namespace FFBuildCatalogModel
{
	// 이미 조회한 권위 상태를 O(시설 수 + 필수 행 수)로 투영합니다. 설치 개수나 충족 판정을
	// 다시 계산하지 않으며, 축제 변경으로 목록에서 빠진 시설의 이전 표시는 초기화합니다.
	inline void ApplyRequiredFacilities(
		TArray<FFF_BuildCatalogEntry>& Entries,
		const FFF_RequiredFacilitiesStatus& Status)
	{
		TMap<FName, const FFF_RequiredFacilityRow*> RowsById;
		RowsById.Reserve(Status.Facilities.Num());
		for (const FFF_RequiredFacilityRow& Row : Status.Facilities)
		{
			if (Row.FacilityDefinitionId.IsNone()) { continue; }
			const FFF_RequiredFacilityRow*& Selected = RowsById.FindOrAdd(Row.FacilityDefinitionId);
			// 중복 설정 때문에 생긴 무효 행이 정상 행의 설치 개수/충족 표시를 덮지 않습니다.
			if (!Selected || (!Selected->bDefinitionValid && Row.bDefinitionValid)) { Selected = &Row; }
		}
		for (FFF_BuildCatalogEntry& Entry : Entries)
		{
			const FFF_RequiredFacilityRow* const* Found = RowsById.Find(Entry.FacilityDefinitionId);
			const FFF_RequiredFacilityRow* Row = Found ? *Found : nullptr;
			Entry.bRequiredForSetup = Row != nullptr;
			Entry.bRequiredSetupSatisfied = Row && Row->bDefinitionValid && Row->MissingCount == 0;
			Entry.RequiredInstalledCount = Row ? Row->InstalledCount : 0;
			Entry.RequiredMinimumCount = Row ? Row->MinimumCount : 0;
		}
	}

	// 현재 Festival 비용표를 화면용 카탈로그 스냅샷에 투영합니다. 원본 GI 카탈로그 캐시는
	// 변경하지 않으므로 Festival 교체 뒤 화면을 다시 열면 항상 최신 설치비와 회수비를 사용합니다.
	inline bool ApplyFacilityCosts(
		FFF_BuildCatalogEntry& Entry,
		const UFFZSFacilityDefinition* Definition,
		const UDataTable* CostTable,
		const float DemolitionRefundRate,
		FString& OutFailureReason)
	{
		Entry.InstallCost = 0;
		Entry.bHasResolvedInstallCost = false;
		Entry.RecoveryCost = 0;
		Entry.bHasResolvedRecoveryCost = false;
		Entry.MatchedInstallCostRow = NAME_None;
		OutFailureReason.Reset();

		if (!Definition)
		{
			OutFailureReason = TEXT("시설 정의가 로드되지 않았습니다.");
			return false;
		}
		if (!CostTable)
		{
			OutFailureReason = TEXT("현재 Festival의 비용표가 없습니다.");
			return false;
		}

		if (!FFF_FacilityCostResolver::FindInstallCost(
			Definition->Spec,
			CostTable,
			Entry.InstallCost,
			Entry.MatchedInstallCostRow,
			OutFailureReason))
		{
			return false;
		}
		Entry.bHasResolvedInstallCost = true;
		Entry.RecoveryCost = FFF_FacilityCostResolver::CalculateDemolitionRefund(
			Entry.InstallCost,
			DemolitionRefundRate);
		Entry.bHasResolvedRecoveryCost = true;
		return true;
	}
	// 두 문자열을 대소문자를 구분해 비교한다.
	// 문화권별 자연어 대조 규칙을 적용하는 정렬이 아니라, 현재 전달된 문자열 값에 대해 같은
	// 결과를 내는 내부 카탈로그 순서가 목적이므로 이후의 문자열 비교 기준을 이 함수로 통일한다.
	// 반환값은 A가 앞이면 음수, 같으면 0, 뒤면 양수다.
	 
	inline int32 CompareStrings(const FString& A, const FString& B)
	{
		return A.Compare(B, ESearchCase::CaseSensitive);
	}

	
	// 두 항목의 엄격한 정렬 우선순위를 판정한다.
	// 우선순위는 대분류 enum 값 -> 소분류 토큰 -> 표시 이름 -> 시설 정의 ID 순이다.
	// 마지막 ID 비교는 표시 이름까지 같은 항목에도 항상 재현 가능한 타이브레이커를 제공하여,
	// 에셋 레지스트리가 항목을 돌려준 순서가 UI 결과에 영향을 주지 않게 한다.
	 
	inline bool EntrySortsBefore(const FFF_BuildCatalogEntry& A, const FFF_BuildCatalogEntry& B)
	{
		// enum 선언 순서를 대분류의 제품 정렬 순서로 사용합니다.
		const uint8 CategoryA = static_cast<uint8>(A.Category);
		const uint8 CategoryB = static_cast<uint8>(B.Category);
		if (CategoryA != CategoryB)
		{
			return CategoryA < CategoryB;
		}

		const int32 MinorComparison = CompareStrings(A.MinorToken.ToString(), B.MinorToken.ToString());
		if (MinorComparison != 0)
		{
			return MinorComparison < 0;
		}

		const int32 DisplayNameComparison = CompareStrings(A.DisplayName.ToString(), B.DisplayName.ToString());
		if (DisplayNameComparison != 0)
		{
			return DisplayNameComparison < 0;
		}

		// 앞선 모든 키가 같은 경우에만 안정적인 최종 식별자를 비교합니다.
		return CompareStrings(A.FacilityDefinitionId.ToString(), B.FacilityDefinitionId.ToString()) < 0;
	}

	// Entries를 위의 카탈로그 규칙에 따라 제자리 정렬합니다.
	// 입력 배열 자체가 변경되며 별도 복사본을 만들지 않습니다.

	inline void SortEntries(TArray<FFF_BuildCatalogEntry>& Entries)
	{
		Entries.Sort([](const FFF_BuildCatalogEntry& A, const FFF_BuildCatalogEntry& B)
		{
			return EntrySortsBefore(A, B);
		});
	}
	
	// 이미 SortEntries로 정렬된 평면 항목을 `대분류 > 소분류 > 항목` 계층으로 변환합니다.
	// SortedEntries: 정렬 규칙상 같은 대분류/소분류가 서로 인접해 있는 입력 배열
	// OutGroups: 기존 내용을 비운 뒤 새 계층을 기록할 출력 배열
	// 정렬되어 있다는 전제를 이용해 한 번만 순회하며 마지막 그룹에 이어 붙입니다. 따라서
	// 이 함수 안에서 다시 검색하거나 정렬하지 않으며, 각 항목은 출력 그룹에 값으로 복사됩니다.

	inline void BuildCategoryGroups(
		const TArray<FFF_BuildCatalogEntry>& SortedEntries,
		TArray<FFF_BuildCatalogCategoryGroup>& OutGroups)
	{
		// 호출자가 이전 새로고침 결과를 재사용하더라도 오래된 그룹이 남지 않도록 완전히 초기화합니다.
		OutGroups.Reset();

		for (const FFF_BuildCatalogEntry& Entry : SortedEntries)  
		{
			// 대분류가 바뀌는 경계에서만 새 최상위 그룹을 만듭니다.
			if (OutGroups.IsEmpty() || OutGroups.Last().Category != Entry.Category)
			{
				FFF_BuildCatalogCategoryGroup& NewCategory = OutGroups.AddDefaulted_GetRef();
				NewCategory.Category = Entry.Category;
			}

			FFF_BuildCatalogCategoryGroup& CategoryGroup = OutGroups.Last();
			// 같은 대분류 안에서 소분류 토큰이 바뀌는 경계에 새 행을 추가합니다.
			if (CategoryGroup.MinorGroups.IsEmpty()
				|| CategoryGroup.MinorGroups.Last().MinorToken != Entry.MinorToken)
			{
				FFF_BuildCatalogMinorGroup& NewMinor = CategoryGroup.MinorGroups.AddDefaulted_GetRef();
				NewMinor.MinorToken = Entry.MinorToken;
			}

			CategoryGroup.MinorGroups.Last().Items.Add(Entry);
		}
	}
	
	// 새 비동기 요청을 시작하면서 활성 세대값을 증가시키고 그 값을 반환한다.
	// 반환된 세대를 캡처한 콜백만 현재 상태를 반영할 수 있으므로, 새로고침이나 종료 이후 늦게
	// 도착한 이전 요청의 콜백이 최신 카탈로그를 덮어쓰는 경쟁 상태를 막을 수 있다.
	// InOutGeneration은 호출의 부작용으로 갱신된다.
	 
	inline uint64 BeginNextRequest(uint64& InOutGeneration)
	{
		++InOutGeneration;
		// uint64가 한 바퀴 도는 극단적인 경우에도 0은 "아직 요청하지 않음" 상태로 예약합니다.
		if (InOutGeneration == 0)
		{
			++InOutGeneration;
		}
		return InOutGeneration;
	}

	// 콜백이 캡처한 세대가 현재 활성 요청과 같은지 부작용 없이 확인합니다. 
	inline bool IsCurrentRequest(const uint64 CallbackGeneration, const uint64 ActiveGeneration)
	{
		return CallbackGeneration == ActiveGeneration;
	}
}
