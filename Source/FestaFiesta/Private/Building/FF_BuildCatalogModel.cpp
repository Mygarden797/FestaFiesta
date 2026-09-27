// FF_BuildCatalogModel.cpp
// 역할: 원본 시설을 바꾸지 않고 비용, 필수시설, 프로그램 안내와 목록 순서를 계산한다.
// 화면과 자동화 검사가 이 구현을 함께 사용하여 표시 규칙이 서로 달라지지 않게 한다.

#include "Building/FF_BuildCatalogModel.h"

#include "Building/FF_FacilityNamingLibrary.h"
#include "Economy/FF_FacilityCostResolver.h"
#include "Economy/Rules/FF_ProgramScheduleRules.h"
#include "FF_FFZSFacilityDefinition.h"
#include "Festival/FF_RequiredFacilityTypes.h"

namespace
{
	// 이름 변환은 항목마다 한 번만 수행하고 정렬 중에는 이 작은 키만 비교한다.
	struct FCatalogSortKey
	{
		uint8 Category;
		FString MinorToken;
		FString DisplayName;
		FString DefinitionId;
		int32 EntryIndex;
	};

	FCatalogSortKey MakeSortKey(const FFF_BuildCatalogEntry& Entry, const int32 EntryIndex)
	{
		return {static_cast<uint8>(Entry.Category), Entry.MinorToken.ToString(),
			Entry.DisplayName.ToString(), Entry.FacilityDefinitionId.ToString(), EntryIndex};
	}

	// 개별 비교와 전체 목록 정렬이 같은 우선순위를 사용하도록 비교 규칙을 모은다.
	bool SortKeyComesBefore(const FCatalogSortKey& A, const FCatalogSortKey& B)
	{
		if (A.Category != B.Category)
		{
			return A.Category < B.Category;
		}
		const int32 MinorComparison = FFBuildCatalogModel::CompareStrings(A.MinorToken, B.MinorToken);
		if (MinorComparison != 0)
		{
			return MinorComparison < 0;
		}
		const int32 NameComparison = FFBuildCatalogModel::CompareStrings(A.DisplayName, B.DisplayName);
		if (NameComparison != 0)
		{
			return NameComparison < 0;
		}
		return FFBuildCatalogModel::CompareStrings(A.DefinitionId, B.DefinitionId) < 0;
	}
}

namespace FFBuildCatalogModel
{
	// 시설의 실제 정의 ID와 편성 도메인 규칙으로 프로그램 기능을 투영합니다.
	// 카탈로그 교체/비가용 상태에서도 이전 배지가 남지 않도록 매번 초기화합니다.
	void ApplyCompatiblePrograms(
		FFF_BuildCatalogEntry& Entry,
		const TArray<FFF_ProgramCard>& Catalog)
	{
		Entry.CompatibleProgramCount = 0;
		Entry.CompatibleProgramNames = FText::GetEmpty();
		FFF_FacilityNameParts Parts;
		FString ParseFailure;
		if (!UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
			Entry.Category, Entry.FacilityDefinitionId, Parts, ParseFailure)) { return; }

		TSet<FName> SeenProgramIds;
		TArray<FText> Names;
		SeenProgramIds.Reserve(Catalog.Num());
		Names.Reserve(Catalog.Num());
		for (const FFF_ProgramCard& Card : Catalog)
		{
			// 실제 편성의 FindCard와 동일하게 중복 ID의 첫 번째 카드만 사용합니다.
			bool bAlreadySeen = false;
			SeenProgramIds.Add(Card.ProgramId, &bAlreadySeen);
			if (bAlreadySeen) { continue; }
			if (!Card.IsValid() || !FMath::IsFinite(Card.StartMinutes) || !FMath::IsFinite(Card.EndMinutes)
				|| Card.StartMinutes < 0.f
				|| !FFF_ProgramScheduleRules::IsFacilityTypeAllowed(Card, Parts.MajorToken, Parts.MinorToken))
			{
				continue;
			}
			++Entry.CompatibleProgramCount;
			Names.Add(Card.DisplayName.IsEmpty() || Card.DisplayName.ToString() == Card.ProgramId.ToString()
				? NSLOCTEXT("FestaFiestaBuildCatalog", "UnnamedCompatibleProgram", "이름 없는 프로그램")
				: Card.DisplayName);
		}
		Entry.CompatibleProgramNames = FText::Join(FText::FromString(TEXT(", ")), Names);
	}

	// 이미 조회한 권위 상태를 O(시설 수 + 필수 행 수)로 투영합니다. 설치 개수나 충족 판정을
	// 다시 계산하지 않으며, 축제 변경으로 목록에서 빠진 시설의 이전 표시는 초기화합니다.
	void ApplyRequiredFacilities(
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
	bool ApplyFacilityCosts(
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

	int32 CompareStrings(const FString& A, const FString& B)
	{
		return A.Compare(B, ESearchCase::CaseSensitive);
	}


	// 두 항목의 엄격한 정렬 우선순위를 판정한다.
	// 우선순위는 대분류 enum 값 -> 소분류 토큰 -> 표시 이름 -> 시설 정의 ID 순이다.
	// 마지막 ID 비교는 표시 이름까지 같은 항목에도 항상 재현 가능한 타이브레이커를 제공하여,
	// 에셋 레지스트리가 항목을 돌려준 순서가 UI 결과에 영향을 주지 않게 한다.

	bool EntrySortsBefore(const FFF_BuildCatalogEntry& A, const FFF_BuildCatalogEntry& B)
	{
		return SortKeyComesBefore(MakeSortKey(A, 0), MakeSortKey(B, 0));
	}

	// 비교할 때마다 문자열을 다시 만드는 대신, 키를 정렬하고 실제 항목을 한 번씩 옮긴다.
	void SortEntries(TArray<FFF_BuildCatalogEntry>& Entries)
	{
		if (Entries.Num() < 2)
		{
			return;
		}
		TArray<FCatalogSortKey> Keys;
		Keys.Reserve(Entries.Num());
		for (int32 Index = 0; Index < Entries.Num(); ++Index)
		{
			Keys.Add(MakeSortKey(Entries[Index], Index));
		}
		Keys.Sort([](const FCatalogSortKey& A, const FCatalogSortKey& B)
		{
			return SortKeyComesBefore(A, B);
		});

		TArray<FFF_BuildCatalogEntry> SortedEntries;
		SortedEntries.Reserve(Entries.Num());
		for (const FCatalogSortKey& Key : Keys)
		{
			SortedEntries.Add(MoveTemp(Entries[Key.EntryIndex]));
		}
		Entries = MoveTemp(SortedEntries);
	}

	// 이미 SortEntries로 정렬된 평면 항목을 `대분류 > 소분류 > 항목` 계층으로 변환합니다.
	// SortedEntries: 정렬 규칙상 같은 대분류/소분류가 서로 인접해 있는 입력 배열
	// OutGroups: 기존 내용을 비운 뒤 새 계층을 기록할 출력 배열
	// 정렬되어 있다는 전제를 이용해 한 번만 순회하며 마지막 그룹에 이어 붙입니다. 따라서
	// 이 함수 안에서 다시 검색하거나 정렬하지 않으며, 각 항목은 출력 그룹에 값으로 복사됩니다.

	void BuildCategoryGroups(
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
}
