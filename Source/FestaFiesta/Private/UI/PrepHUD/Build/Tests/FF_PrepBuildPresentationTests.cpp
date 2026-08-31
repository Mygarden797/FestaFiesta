// LightTeal PrepBuild 프레젠테이션 규칙과 UTileView 데이터 재사용 계약을 검증한다.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/PrepHUD/Build/FF_PrepBuildPresentationModel.h"
#include "UI/PrepHUD/Build/FF_PrepBuildPresentationRules.h"

namespace FFF_PrepBuildPresentationTestHelpers
{
	FFF_BuildCatalogEntry MakeEntry(
		const TCHAR* FacilityId,
		const TCHAR* DisplayName,
		const EFFZSFacilityCategory Category,
		const TCHAR* MinorToken,
		const int32 InstallCost,
		const bool bHasResolvedCost = true)
	{
		FFF_BuildCatalogEntry Entry;
		Entry.FacilityDefinitionId = FName(FacilityId);
		Entry.DisplayName = FText::FromString(DisplayName);
		Entry.Category = Category;
		Entry.MinorToken = FName(MinorToken);
		Entry.UniqueNameToken = FacilityId;
		Entry.InstallCost = InstallCost;
		Entry.bHasResolvedInstallCost = bHasResolvedCost;
		return Entry;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_PrepBuildPresentation_CategoryContractTest,
	"FestaFiesta.UI.PrepBuild.CategoryContract",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildPresentation_CategoryContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFF_PrepBuildPresentationTestHelpers;

	const EFFZSFacilityCategory ExpectedCategories[] = {
		EFFZSFacilityCategory::Booth,
		EFFZSFacilityCategory::Toilet,
		EFFZSFacilityCategory::Info,
		EFFZSFacilityCategory::Rest,
		EFFZSFacilityCategory::Performance,
		EFFZSFacilityCategory::Safety,
		EFFZSFacilityCategory::PhotoSpot
	};
	const TCHAR* ExpectedLabels[] = {
		TEXT("부스"), TEXT("화장실"), TEXT("안내"), TEXT("휴게"),
		TEXT("공연"), TEXT("안전"), TEXT("포토스팟")
	};

	const TConstArrayView<EFFZSFacilityCategory> Categories =
		FFF_PrepBuildPresentationRules::GetCategoryOrder();
	TestEqual(TEXT("시설 대분류 칩은 일곱 종류다"), Categories.Num(), 7);

	TArray<FFF_BuildCatalogEntry> Entries;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedCategories); ++Index)
	{
		if (Categories.IsValidIndex(Index))
		{
			TestEqual(
				*FString::Printf(TEXT("카테고리 %d의 결정적 순서"), Index),
				Categories[Index], ExpectedCategories[Index]);
		}
		TestTrue(
			*FString::Printf(TEXT("카테고리 %d의 한글 칩 라벨"), Index),
			FFF_PrepBuildPresentationRules::ResolveCategoryDisplayName(
				ExpectedCategories[Index]).EqualTo(FText::FromString(ExpectedLabels[Index])));

		Entries.Add(MakeEntry(
			*FString::Printf(TEXT("Category_%d_Test"), Index),
			ExpectedLabels[Index], ExpectedCategories[Index], TEXT("Test"), 100));
	}

	FFF_PrepBuildFilter Filter;
	int32 VisibleCount = 0;
	for (const FFF_BuildCatalogEntry& Entry : Entries)
	{
		VisibleCount += FFF_PrepBuildPresentationRules::IsEntryVisible(Entry, Filter) ? 1 : 0;
	}
	TestEqual(TEXT("전체 칩은 일곱 카테고리를 모두 통과시킨다"), VisibleCount, 7);

	Filter.bFilterByCategory = true;
	for (const EFFZSFacilityCategory Category : Categories)
	{
		Filter.Category = Category;
		VisibleCount = 0;
		for (const FFF_BuildCatalogEntry& Entry : Entries)
		{
			VisibleCount += FFF_PrepBuildPresentationRules::IsEntryVisible(Entry, Filter) ? 1 : 0;
		}
		TestEqual(
			*FString::Printf(TEXT("%s 칩은 자기 시설만 남긴다"),
				*UEnum::GetValueAsString(Category)),
			VisibleCount, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_PrepBuildPresentation_FilterTest,
	"FestaFiesta.UI.PrepBuild.FilterRules",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildPresentation_FilterTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFF_PrepBuildPresentationTestHelpers;

	const FFF_BuildCatalogEntry FoodTruck = MakeEntry(
		TEXT("Booth_FoodStall_FoodTruck"), TEXT("푸드 트럭"),
		EFFZSFacilityCategory::Booth, TEXT("FoodStall"), 700);

	FFF_PrepBuildFilter Filter;
	TestTrue(TEXT("필터가 비어 있으면 유효 시설을 표시한다"),
		FFF_PrepBuildPresentationRules::IsEntryVisible(FoodTruck, Filter));

	Filter.bFilterByCategory = true;
	Filter.Category = EFFZSFacilityCategory::Toilet;
	TestFalse(TEXT("다른 대분류는 제외한다"),
		FFF_PrepBuildPresentationRules::IsEntryVisible(FoodTruck, Filter));

	Filter.Category = EFFZSFacilityCategory::Booth;
	Filter.MinorToken = TEXT("FoodStall");
	Filter.SearchText = TEXT(" foodtruck ");
	TestTrue(TEXT("검색은 공백을 정리하고 정의 ID를 대소문자 구분 없이 찾는다"),
		FFF_PrepBuildPresentationRules::IsEntryVisible(FoodTruck, Filter));

	Filter.SearchText = TEXT("화장실");
	TestFalse(TEXT("검색어가 어느 표시 필드에도 없으면 제외한다"),
		FFF_PrepBuildPresentationRules::IsEntryVisible(FoodTruck, Filter));

	FFF_BuildCatalogEntry InvalidEntry = FoodTruck;
	InvalidEntry.FacilityDefinitionId = NAME_None;
	Filter = FFF_PrepBuildFilter();
	TestFalse(TEXT("배치 요청 ID가 없는 항목은 카드로 만들지 않는다"),
		FFF_PrepBuildPresentationRules::IsEntryVisible(InvalidEntry, Filter));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_PrepBuildPresentation_TileStateTest,
	"FestaFiesta.UI.PrepBuild.TileStates",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildPresentation_TileStateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFF_PrepBuildPresentationTestHelpers;

	FFF_BuildCatalogEntry Entry = MakeEntry(
		TEXT("Booth_FoodStall_FoodTruck"), TEXT("푸드 트럭"),
		EFFZSFacilityCategory::Booth, TEXT("FoodStall"), 700);

	TestEqual(TEXT("잔액이 설치비와 같아도 배치 가능하다"),
		FFF_PrepBuildPresentationRules::ResolveTileState(Entry, 700, true),
		EFF_PrepBuildTileState::Available);
	TestEqual(TEXT("1 Gold 모자라면 잔액 부족이다"),
		FFF_PrepBuildPresentationRules::ResolveTileState(Entry, 699, true),
		EFF_PrepBuildTileState::InsufficientFunds);
	TestEqual(TEXT("권위 잔액을 못 읽었으면 0으로 취급하지 않는다"),
		FFF_PrepBuildPresentationRules::ResolveTileState(Entry, 0, false),
		EFF_PrepBuildTileState::GoldUnavailable);

	Entry.bHasResolvedInstallCost = false;
	Entry.InstallCost = 0;
	TestEqual(TEXT("가격 미확정은 잔액 미확정보다 먼저 구분한다"),
		FFF_PrepBuildPresentationRules::ResolveTileState(Entry, 0, false),
		EFF_PrepBuildTileState::PriceUnavailable);
	TestFalse(TEXT("미확정 가격을 0 Gold로 표시하지 않는다"),
		FFF_PrepBuildPresentationRules::ResolveCostText(Entry).EqualTo(
			FText::FromString(TEXT("0 Gold"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_PrepBuildPresentation_SurfaceStateTest,
	"FestaFiesta.UI.PrepBuild.SurfaceStates",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildPresentation_SurfaceStateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FFF_PrepBuildPresentationSnapshot Snapshot = FFF_PrepBuildPresentationRules::BuildSnapshot(
		EFF_BuildCatalogLoadState::Loading, 0, 0, FText::GetEmpty());
	TestEqual(TEXT("카탈로그 Loading은 로딩 표면이다"),
		Snapshot.SurfaceState, EFF_PrepBuildSurfaceState::Loading);
	TestFalse(TEXT("로딩 중 목록은 누를 수 없다"), Snapshot.bCanInteract);

	Snapshot = FFF_PrepBuildPresentationRules::BuildSnapshot(
		EFF_BuildCatalogLoadState::Ready, 4, 0, FText::GetEmpty());
	TestEqual(TEXT("원본은 있지만 필터 결과가 없으면 빈 표면이다"),
		Snapshot.SurfaceState, EFF_PrepBuildSurfaceState::Empty);

	Snapshot = FFF_PrepBuildPresentationRules::BuildSnapshot(
		EFF_BuildCatalogLoadState::Ready, 4, 2, FText::GetEmpty());
	TestEqual(TEXT("보이는 항목이 있으면 Ready다"),
		Snapshot.SurfaceState, EFF_PrepBuildSurfaceState::Ready);
	TestTrue(TEXT("Ready 목록만 상호작용 가능하다"), Snapshot.bCanInteract);

	const FText OwnerFailure = FText::FromString(TEXT("테스트 오류"));
	Snapshot = FFF_PrepBuildPresentationRules::BuildSnapshot(
		EFF_BuildCatalogLoadState::Failed, 0, 0, OwnerFailure);
	TestEqual(TEXT("소유자가 준 실패 문구를 보존한다"), Snapshot.StatusText, OwnerFailure);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_PrepBuildPresentation_TileDataTest,
	"FestaFiesta.UI.PrepBuild.TileDataSnapshot",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildPresentation_TileDataTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFF_PrepBuildPresentationTestHelpers;

	FFF_BuildCatalogEntry Entry = MakeEntry(
		TEXT("Booth_FoodStall_FoodTruck"), TEXT("푸드 트럭"),
		EFFZSFacilityCategory::Booth, TEXT("FoodStall"), 700);
	UFF_PrepBuildTileData* Tile = NewObject<UFF_PrepBuildTileData>(GetTransientPackage());
	TestNotNull(TEXT("TileData를 UObject 목록 항목으로 생성할 수 있다"), Tile);
	if (!Tile)
	{
		return false;
	}

	Tile->Initialize(Entry, 900, true);
	Entry.DisplayName = FText::FromString(TEXT("외부 변경"));
	TestEqual(TEXT("시설 ID를 값 스냅샷으로 보존한다"),
		Tile->GetFacilityDefinitionId(), FName(TEXT("Booth_FoodStall_FoodTruck")));
	TestTrue(TEXT("표시명은 원본 배열 수명과 분리된 복사본이다"),
		Tile->GetDisplayName().EqualTo(FText::FromString(TEXT("푸드 트럭"))));
	TestTrue(TEXT("가격이 해결됐고 잔액이 충분하면 구매 가능하다"), Tile->IsAffordable());
	TestTrue(TEXT("가격 해결 여부를 별도로 노출한다"), Tile->HasResolvedCost());

	Tile->RefreshGold(699, true);
	TestFalse(TEXT("잔액 이벤트만으로 구매 가능 상태가 갱신된다"), Tile->IsAffordable());
	TestEqual(TEXT("부족 원인이 유지된다"),
		Tile->GetTileState(), EFF_PrepBuildTileState::InsufficientFunds);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_PrepBuildPresentation_ModelReuseTest,
	"FestaFiesta.UI.PrepBuild.ModelReusesTileObjects",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildPresentation_ModelReuseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFF_PrepBuildPresentationTestHelpers;

	TArray<FFF_BuildCatalogEntry> Entries;
	Entries.Add(MakeEntry(TEXT("Booth_FoodStall_FoodTruck"), TEXT("푸드 트럭"),
		EFFZSFacilityCategory::Booth, TEXT("FoodStall"), 700));
	Entries.Add(MakeEntry(TEXT("Toilet_Standard_Default"), TEXT("일반 화장실"),
		EFFZSFacilityCategory::Toilet, TEXT("Standard"), 300));

	UFF_PrepBuildPresentationModel* Model =
		NewObject<UFF_PrepBuildPresentationModel>(GetTransientPackage());
	TestNotNull(TEXT("프레젠테이션 모델을 생성할 수 있다"), Model);
	if (!Model)
	{
		return false;
	}

	FFF_PrepBuildFilter Filter;
	Model->Reconcile(EFF_BuildCatalogLoadState::Ready, Entries, Filter, 1'000, true, FText::GetEmpty());
	TestEqual(TEXT("입력 순서를 보존해 두 항목을 노출한다"), Model->GetVisibleTiles().Num(), 2);
	TestEqual(TEXT("UTileView용 UObject 배열도 같은 개수다"), Model->GetListItems().Num(), 2);

	UFF_PrepBuildTileData* OriginalFoodTruck =
		Model->FindTile(TEXT("Booth_FoodStall_FoodTruck"));
	TestNotNull(TEXT("시설 ID로 TileData를 찾는다"), OriginalFoodTruck);

	Filter.bFilterByCategory = true;
	Filter.Category = EFFZSFacilityCategory::Toilet;
	Model->Reconcile(EFF_BuildCatalogLoadState::Ready, Entries, Filter, 1'000, true, FText::GetEmpty());
	TestEqual(TEXT("대분류 필터는 한 항목만 남긴다"), Model->GetVisibleTiles().Num(), 1);

	Filter.Category = EFFZSFacilityCategory::Booth;
	Entries[0].DisplayName = FText::FromString(TEXT("새 푸드 트럭"));
	Model->Reconcile(EFF_BuildCatalogLoadState::Ready, Entries, Filter, 1'000, true, FText::GetEmpty());
	TestTrue(TEXT("필터에서 돌아와도 같은 UObject identity를 재사용한다"),
		Model->FindTile(TEXT("Booth_FoodStall_FoodTruck")) == OriginalFoodTruck);
	TestTrue(TEXT("재사용 객체의 최신 스냅샷은 갱신한다"),
		OriginalFoodTruck->GetDisplayName().EqualTo(FText::FromString(TEXT("새 푸드 트럭"))));

	Model->RefreshGold(100, true);
	TestEqual(TEXT("모델 잔액 경량 갱신이 풀의 항목까지 전달된다"),
		OriginalFoodTruck->GetTileState(), EFF_PrepBuildTileState::InsufficientFunds);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
