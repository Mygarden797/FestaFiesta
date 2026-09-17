// LightTeal PrepBuild 프레젠테이션 규칙과 UTileView 데이터 재사용 계약을 검증한다.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Building/FF_BuildCatalogModel.h"
#include "Building/FF_FacilityNamingLibrary.h"
#include "UI/PrepHUD/Build/FF_PrepBuildPresentationModel.h"
#include "UI/PrepHUD/Build/FF_PrepBuildPresentationRules.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"

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
		EFFZSFacilityCategory::PhotoSpot,
		EFFZSFacilityCategory::Lighting,
		EFFZSFacilityCategory::Road,
		EFFZSFacilityCategory::Tile
	};
	const TCHAR* ExpectedLabels[] = {
		TEXT("부스"), TEXT("화장실"), TEXT("안내"), TEXT("휴게"),
		TEXT("공연"), TEXT("안전"), TEXT("포토스팟"), TEXT("조명"), TEXT("길"), TEXT("타일")
	};

	const TConstArrayView<EFFZSFacilityCategory> Categories =
		FFF_PrepBuildPresentationRules::GetCategoryOrder();
	TestEqual(TEXT("시설 대분류 칩은 길과 타일을 포함한 열 종류다"), Categories.Num(), 10);

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
	TestEqual(TEXT("전체 칩은 열 카테고리를 모두 통과시킨다"), VisibleCount, 10);

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
	FFF_PrepBuildPresentation_LightingCatalogTest,
	"FestaFiesta.UI.PrepBuild.LightingCatalogAndSearch",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildPresentation_LightingCatalogTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFF_PrepBuildPresentationTestHelpers;
	const TCHAR* LightingIds[] = {
		TEXT("Lighting_Decorative_CurtainArch"), TEXT("Lighting_Decorative_Festoon"),
		TEXT("Lighting_Decorative_GoldenTree"), TEXT("Lighting_Pendant_RattanPendant"),
		TEXT("Lighting_Decorative_StarTree"), TEXT("Lighting_Streetlamp_FestivalStreetlamp")
	};
	TArray<FFF_BuildCatalogEntry> Entries;
	for (const TCHAR* Id : LightingIds)
	{
		FFF_FacilityNameParts Parts;
		FString Failure;
		if (!TestTrue(*FString::Printf(TEXT("조명 정의 ID %s가 카탈로그 규칙을 통과한다"), Id),
			UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
				EFFZSFacilityCategory::Lighting, FName(Id), Parts, Failure)))
		{
			continue;
		}
		Entries.Add(MakeEntry(Id, *Parts.UniqueToken, EFFZSFacilityCategory::Lighting,
			*Parts.MinorToken.ToString(), 100));
	}
	Entries.Add(MakeEntry(TEXT("Booth_FoodStall_Test"), TEXT("먹거리 부스"),
		EFFZSFacilityCategory::Booth, TEXT("FoodStall"), 100));
	UFF_PrepBuildPresentationModel* Model = NewObject<UFF_PrepBuildPresentationModel>();
	FFF_PrepBuildFilter Filter;
	Filter.bFilterByCategory = true;
	Filter.Category = EFFZSFacilityCategory::Lighting;
	Filter.SearchText = TEXT("조명");
	Model->Reconcile(EFF_BuildCatalogLoadState::Ready, Entries, Filter, 1000, true, FText::GetEmpty());
	TestEqual(TEXT("조명 분류와 한글 검색에서 여섯 종류를 모두 표시한다"), Model->GetVisibleTiles().Num(), 6);
	for (const UFF_PrepBuildTileData* Tile : Model->GetVisibleTiles())
	{
		TestTrue(TEXT("조명 카드는 설치 명령용 정의 ID를 보존한다"), !Tile->GetFacilityDefinitionId().IsNone());
		TestTrue(TEXT("잔액이 충분한 조명 카드는 배치 가능하다"), Tile->IsAffordable());
	}
	Filter.MinorToken = TEXT("Streetlamp");
	Model->Reconcile(EFF_BuildCatalogLoadState::Ready, Entries, Filter, 1000, true, FText::GetEmpty());
	TestEqual(TEXT("가로등 소분류는 FestivalStreetlamp 한 종류만 표시한다"), Model->GetVisibleTiles().Num(), 1);
	if (Model->GetVisibleTiles().Num() == 1)
	{
		TestEqual(TEXT("가로등 카드는 올바른 시설을 배치한다"),
			Model->GetVisibleTiles()[0]->GetFacilityDefinitionId(), FName(LightingIds[5]));
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
	Filter.SearchText = TEXT("먹거리");
	TestTrue(TEXT("화면에 보이는 한글 소분류로 검색한다"),
		FFF_PrepBuildPresentationRules::IsEntryVisible(FoodTruck, Filter));
	Filter.SearchText = TEXT("부스");
	TestTrue(TEXT("화면에 보이는 한글 대분류로 검색한다"),
		FFF_PrepBuildPresentationRules::IsEntryVisible(FoodTruck, Filter));
	Filter.SearchText.Reset();
	Filter.MinorToken = TEXT("MarketGoods");
	TestFalse(TEXT("같은 대분류여도 다른 소분류는 제외한다"),
		FFF_PrepBuildPresentationRules::IsEntryVisible(FoodTruck, Filter));

	FFF_BuildCatalogEntry InvalidEntry = FoodTruck;
	InvalidEntry.FacilityDefinitionId = NAME_None;
	Filter = FFF_PrepBuildFilter();
	TestFalse(TEXT("배치 요청 ID가 없는 항목은 카드로 만들지 않는다"),
		FFF_PrepBuildPresentationRules::IsEntryVisible(InvalidEntry, Filter));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_PrepBuildPresentation_FavoritesFilterTest,
	"FestaFiesta.UI.PrepBuild.FavoritesFilterComposition",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildPresentation_FavoritesFilterTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFF_PrepBuildPresentationTestHelpers;
	const TArray<FFF_BuildCatalogEntry> Entries = {
		MakeEntry(TEXT("Booth_CraftExperience_SandArt"), TEXT("바다향기 공방"),
			EFFZSFacilityCategory::Booth, TEXT("CraftExperience"), 300),
		MakeEntry(TEXT("Booth_FoodStall_Burger"), TEXT("해변 버거"),
			EFFZSFacilityCategory::Booth, TEXT("FoodStall"), 300),
		MakeEntry(TEXT("Toilet_Standard_Default"), TEXT("일반 화장실"),
			EFFZSFacilityCategory::Toilet, TEXT("Standard"), 300)
	};
	UFF_PrepBuildPresentationModel* Model = NewObject<UFF_PrepBuildPresentationModel>();
	FFF_PrepBuildFilter Filter;
	const auto Reconcile = [&]()
	{
		Model->Reconcile(EFF_BuildCatalogLoadState::Ready, Entries, Filter, 1000, true, FText::GetEmpty());
	};
	Filter.FavoriteIds = { Entries[0].FacilityDefinitionId, Entries[2].FacilityDefinitionId, FName(TEXT("Retired_Facility")) };
	Reconcile();
	TestEqual(TEXT("Saving favorites alone does not hide the rest of the catalog"), Model->GetVisibleTiles().Num(), 3);
	UFF_PrepBuildTileData* OriginalSand = Model->FindTile(Entries[0].FacilityDefinitionId);
	Filter.bFavoritesOnly = true;
	Reconcile();
	TestEqual(TEXT("Favorites-only ignores stale IDs and retains the two existing favorites"), Model->GetVisibleTiles().Num(), 2);
	Filter.bFilterByCategory = true;
	Filter.Category = EFFZSFacilityCategory::Booth;
	Filter.MinorToken = TEXT("CraftExperience");
	Filter.SearchText = TEXT(" 바다향기 ");
	Reconcile();
	TestEqual(TEXT("Favorite, major category, minor category and search compose with AND"), Model->GetVisibleTiles().Num(), 1);
	TestTrue(TEXT("Filtering reuses the existing tile object"), Model->FindTile(Entries[0].FacilityDefinitionId) == OriginalSand);
	Filter.SearchText = TEXT("버거");
	Reconcile();
	TestEqual(TEXT("Search cannot reintroduce a nonfavorite facility"), Model->GetVisibleTiles().Num(), 0);
	Filter.SearchText = TEXT("바다향기");
	Filter.FavoriteIds.Remove(Entries[0].FacilityDefinitionId);
	Reconcile();
	TestEqual(TEXT("Unstarring the final matching facility leaves an empty list"), Model->GetVisibleTiles().Num(), 0);
	TestTrue(TEXT("Unstarring preserves favorites-only and the user's other filters"),
		Filter.bFavoritesOnly && Filter.bFilterByCategory && Filter.MinorToken == TEXT("CraftExperience") && Filter.SearchText == TEXT("바다향기"));
	Filter.FavoriteIds.Reset();
	Filter.bFilterByCategory = false;
	Filter.MinorToken = NAME_None;
	Filter.SearchText.Reset();
	Reconcile();
	TestEqual(TEXT("An empty favorites collection produces an empty list"), Model->GetVisibleTiles().Num(), 0);
	Filter.bFavoritesOnly = false;
	Reconcile();
	TestEqual(TEXT("Turning off favorites-only restores the complete catalog"), Model->GetVisibleTiles().Num(), 3);
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
	TestTrue(TEXT("카드는 대분류와 소분류 경로를 이름과 분리해 보존한다"),
		Tile->GetCategoryPathText().EqualTo(FText::FromString(TEXT("부스 > 먹거리·음료"))));
	TestEqual(TEXT("고유 이름 토큰은 전체 툴팁에 사용할 수 있다"),
		Tile->GetUniqueNameToken(), FString(TEXT("Booth_FoodStall_FoodTruck")));
	TestTrue(TEXT("가격이 해결됐고 잔액이 충분하면 구매 가능하다"), Tile->IsAffordable());
	TestTrue(TEXT("가격 해결 여부를 별도로 노출한다"), Tile->HasResolvedCost());

	Tile->RefreshGold(699, true);
	TestFalse(TEXT("잔액 이벤트만으로 구매 가능 상태가 갱신된다"), Tile->IsAffordable());
	TestEqual(TEXT("부족 원인이 유지된다"),
		Tile->GetTileState(), EFF_PrepBuildTileState::InsufficientFunds);

	Entry.MaxInstancesPerWorld = 1;
	Entry.CurrentInstanceCount = 0;
	Tile->Initialize(Entry, 900, true);
	TestEqual(TEXT("미설치 제한 시설은 상태 칸에 0/1을 표시한다"), Tile->GetStateLabelText().ToString(), FString(TEXT("0/1")));
	TestTrue(TEXT("0/1 시설은 잔액이 충분하면 배치할 수 있다"), Tile->IsAffordable());
	Entry.CurrentInstanceCount = 1;
	Entry.bBuildLimitReached = true;
	Tile->Initialize(Entry, 900, true);
	TestEqual(TEXT("설치 후 상태 칸은 1/1로 갱신된다"), Tile->GetStateLabelText().ToString(), FString(TEXT("1/1")));
	TestFalse(TEXT("1/1 시설은 한도에 도달해 추가 배치할 수 없다"), Tile->IsAffordable());
	Entry.CurrentInstanceCount = 0;
	Entry.bBuildLimitReached = false;
	Tile->Initialize(Entry, 900, true);
	TestEqual(TEXT("철거 후 상태 칸은 다시 0/1로 돌아온다"), Tile->GetStateLabelText().ToString(), FString(TEXT("0/1")));
	Tile->RefreshGold(699, true);
	TestTrue(TEXT("잔액 부족이어도 카운트는 유지하고 배치는 막는다"),
		Tile->GetStateLabelText().ToString() == TEXT("0/1") && !Tile->IsAffordable() &&
		Tile->GetTileState() == EFF_PrepBuildTileState::InsufficientFunds);
	Entry.MaxInstancesPerWorld = 0;
	Tile->Initialize(Entry, 900, true);
	TestEqual(TEXT("무제한 시설은 기존 배치 가능 라벨로 돌아온다"), Tile->GetStateLabelText(),
		FFF_PrepBuildPresentationRules::ResolveTileStateLabel(EFF_PrepBuildTileState::Available));
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

	FFF_RequiredFacilityRow Requirement;
	Requirement.FacilityDefinitionId = Entries[0].FacilityDefinitionId;
	Requirement.MinimumCount = 2;
	Requirement.InstalledCount = 1;
	Requirement.MissingCount = 1;
	Requirement.bDefinitionValid = true;
	FFF_RequiredFacilityRow InvalidDuplicate = Requirement;
	InvalidDuplicate.bDefinitionValid = false;
	InvalidDuplicate.MinimumCount = 99;
	FFF_RequiredFacilityRow InvalidKnown;
	InvalidKnown.FacilityDefinitionId = Entries[1].FacilityDefinitionId;
	InvalidKnown.MinimumCount = 1;
	FFF_RequiredFacilitiesStatus RequiredStatus;
	RequiredStatus.Facilities = { InvalidDuplicate, Requirement, InvalidKnown, InvalidDuplicate };
	Model->SetSynergyHighlightIds({ Entries[0].FacilityDefinitionId });
	auto ReconcileRequirements = [&]()
	{
		FFBuildCatalogModel::ApplyRequiredFacilities(Entries, RequiredStatus);
		Model->Reconcile(EFF_BuildCatalogLoadState::Ready, Entries, Filter, 100, true, FText::GetEmpty());
	};
	ReconcileRequirements();
	TestTrue(TEXT("권위 필수 상태를 같은 풀 객체에 투영한다"),
		Model->FindTile(Entries[0].FacilityDefinitionId) == OriginalFoodTruck &&
		OriginalFoodTruck->IsRequiredFacility() && !OriginalFoodTruck->IsRequiredFacilitySatisfied());
	TestEqual(TEXT("필수 시설 현재 설치 개수를 보존한다"), OriginalFoodTruck->GetRequiredFacilityCurrentCount(), 1);
	TestEqual(TEXT("앞뒤 무효 중복 행이 정상 행의 최소 개수를 덮지 않는다"), OriginalFoodTruck->GetRequiredFacilityMinimumCount(), 2);
	const UFF_PrepBuildTileData* InvalidTile = Model->FindTile(Entries[1].FacilityDefinitionId);
	TestTrue(TEXT("ID가 있는 무효 필수 행은 누락 개수가 0이어도 충족으로 표시하지 않는다"),
		InvalidTile && InvalidTile->IsRequiredFacility() && !InvalidTile->IsRequiredFacilitySatisfied());
	TestTrue(TEXT("필수 표시는 조합 추천과 잔액 부족 판정을 유지한다"),
		OriginalFoodTruck->IsSynergyHighlighted() && !OriginalFoodTruck->IsAffordable());
	RequiredStatus.Facilities[1].InstalledCount = 2;
	RequiredStatus.Facilities[1].MissingCount = 0;
	ReconcileRequirements();
	TestTrue(TEXT("중복 무효 행이 있어도 정상 행의 최신 충족 상태를 표시한다"),
		OriginalFoodTruck->IsRequiredFacilitySatisfied() && OriginalFoodTruck->GetRequiredFacilityCurrentCount() == 2);
	Model->RefreshGold(1000, true);
	TestTrue(TEXT("골드만 바뀌어도 필수 충족과 조합 추천은 유지한다"),
		OriginalFoodTruck->IsAffordable() && OriginalFoodTruck->IsRequiredFacilitySatisfied() && OriginalFoodTruck->IsSynergyHighlighted());
	RequiredStatus.Facilities.Reset();
	ReconcileRequirements();
	TestTrue(TEXT("필수 목록에서 제거되면 풀 객체의 이전 표시와 개수를 모두 초기화한다"),
		Model->FindTile(Entries[0].FacilityDefinitionId) == OriginalFoodTruck &&
		!OriginalFoodTruck->IsRequiredFacility() && !OriginalFoodTruck->IsRequiredFacilitySatisfied() &&
		OriginalFoodTruck->GetRequiredFacilityCurrentCount() == 0 && OriginalFoodTruck->GetRequiredFacilityMinimumCount() == 0);
	TestTrue(TEXT("필수 표시 초기화는 조합 추천이나 가격 판정을 바꾸지 않는다"),
		OriginalFoodTruck->IsSynergyHighlighted() && !OriginalFoodTruck->IsAffordable());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_PrepBuildPresentation_SynergyHighlightTest,
	"FestaFiesta.UI.PrepBuild.SynergyHighlightPreservesFiltersAndPool",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildPresentation_SynergyHighlightTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFF_PrepBuildPresentationTestHelpers;
	TArray<FFF_BuildCatalogEntry> Entries = {
		MakeEntry(TEXT("Booth_FoodStall_Burger"), TEXT("버거 부스"), EFFZSFacilityCategory::Booth, TEXT("FoodStall"), 100),
		MakeEntry(TEXT("Rest_Seating_Seating"), TEXT("휴식 벤치"), EFFZSFacilityCategory::Rest, TEXT("Seating"), 700),
		MakeEntry(TEXT("Toilet_Standard_Default"), TEXT("일반 화장실"), EFFZSFacilityCategory::Toilet, TEXT("Standard"), 100),
		MakeEntry(TEXT("Rest_Shade_Shade"), TEXT("그늘 쉼터"), EFFZSFacilityCategory::Rest, TEXT("Shade"), 300)
	};
	Entries[3].MaxInstancesPerWorld = 1;
	Entries[3].CurrentInstanceCount = 1;
	Entries[3].bBuildLimitReached = true;
	UFF_PrepBuildPresentationModel* Model = NewObject<UFF_PrepBuildPresentationModel>();
	FFF_PrepBuildFilter Filter;
	auto Reconcile = [&]() { Model->Reconcile(EFF_BuildCatalogLoadState::Ready, Entries, Filter, 500, true, FText::GetEmpty()); };
	Reconcile();
	const TArray<UObject*> OriginalItems = Model->GetListItems();
	if (!TestEqual(TEXT("The source catalog starts with four distinct entries"), OriginalItems.Num(), 4)) return false;
	UFF_PrepBuildTileData* Seating = Model->FindTile(Entries[1].FacilityDefinitionId);
	UFF_PrepBuildTileData* Shade = Model->FindTile(Entries[3].FacilityDefinitionId);
	if (!TestNotNull(TEXT("The catalog has the bench tile"), Seating) || !TestNotNull(TEXT("The catalog has the shade tile"), Shade)) return false;

	// Reversed recommendation input, duplicates and unknown IDs must not change stable catalog order.
	Model->SetSynergyHighlightIds({Entries[3].FacilityDefinitionId, Entries[1].FacilityDefinitionId,
		Entries[1].FacilityDefinitionId, FName(TEXT("Unknown_Facility")), NAME_None});
	Reconcile();
	const TArray<UObject*> HighlightedItems = Model->GetListItems();
	if (!TestEqual(TEXT("Highlighting does not add or remove catalog entries"), HighlightedItems.Num(), 4)) return false;
	TestTrue(TEXT("Recommended items come first, preserving the order within both groups"),
		HighlightedItems[0] == OriginalItems[1] && HighlightedItems[1] == OriginalItems[3]
		&& HighlightedItems[2] == OriginalItems[0] && HighlightedItems[3] == OriginalItems[2]);
	TestTrue(TEXT("Both matching pooled items receive the highlight"), Seating->IsSynergyHighlighted() && Shade->IsSynergyHighlighted());
	TestEqual(TEXT("A recommendation does not waive insufficient funds"), Seating->GetTileState(), EFF_PrepBuildTileState::InsufficientFunds);
	TestFalse(TEXT("A recommended item at its construction cap stays unavailable"), Shade->IsAffordable());
	Model->RefreshGold(1000, true);
	TestTrue(TEXT("Gold refresh preserves the highlight and permits the now affordable bench"), Seating->IsSynergyHighlighted() && Seating->IsAffordable());
	TestTrue(TEXT("Gold refresh preserves the highlight without overriding the construction cap"), Shade->IsSynergyHighlighted() && !Shade->IsAffordable());
	TestTrue(TEXT("Gold refresh preserves ordered pooled identities"), Model->GetListItems() == HighlightedItems);

	Filter.bFilterByCategory = true;
	Filter.Category = EFFZSFacilityCategory::Rest;
	Filter.MinorToken = TEXT("Seating");
	Filter.SearchText = TEXT("벤치");
	Filter.bFavoritesOnly = true;
	Filter.FavoriteIds = {Entries[1].FacilityDefinitionId};
	Reconcile();
	TestTrue(TEXT("Recommendations cannot bypass category, minor, search or favorites filters"),
		Model->GetVisibleTiles().Num() == 1 && Model->GetVisibleTiles()[0] == Seating);
	TestTrue(TEXT("The hidden recommended shade remains the same pooled object"), Model->FindTile(Entries[3].FacilityDefinitionId) == Shade);

	Model->SetSynergyHighlightIds({});
	Filter = FFF_PrepBuildFilter();
	Reconcile();
	TestTrue(TEXT("Clearing the selected combination restores the original order and all pooled identities"), Model->GetListItems() == OriginalItems);
	TestFalse(TEXT("Clearing removes a previously visible highlight"), Seating->IsSynergyHighlighted());
	TestFalse(TEXT("Clearing also removes a previously filtered-out highlight"), Shade->IsSynergyHighlighted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_PrepBuildPresentation_TaxonomyTest,
	"FestaFiesta.UI.PrepBuild.TaxonomyLabelsAndNameFallback",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildPresentation_TaxonomyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FName KnownMinorTokens[] = {
		TEXT("FoodStall"), TEXT("MarketGoods"), TEXT("CraftExperience"), TEXT("ActivityGame"), TEXT("Nightlife"),
		TEXT("Standard"), TEXT("Accessible"), TEXT("Family"), TEXT("Temporary"),
		TEXT("GeneralInfo"), TEXT("Ticketing"), TEXT("LostFound"), TEXT("Translation"),
		TEXT("Seating"), TEXT("Shade"), TEXT("Picnic"), TEXT("Cooling"),
		TEXT("MainStage"), TEXT("SubStage"), TEXT("Busking"), TEXT("ExperienceStage"),
		TEXT("FirstAid"), TEXT("Security"), TEXT("FireSafety"), TEXT("Evacuation"),
		TEXT("Landmark"), TEXT("Theme"), TEXT("Scenic"), TEXT("Interactive"),
		TEXT("Decorative"), TEXT("Pendant"), TEXT("Streetlamp")
	};
	for (const FName Token : KnownMinorTokens)
	{
		const FString Label = FFF_PrepBuildPresentationRules::ResolveMinorDisplayName(Token).ToString();
		TestTrue(*FString::Printf(TEXT("%s 소분류는 읽을 수 있는 번역이 있다"), *Token.ToString()),
			!Label.IsEmpty() && Label != Token.ToString());
	}
	TestTrue(TEXT("새 소분류는 빈 라벨 대신 원래 토큰을 보존한다"),
		FFF_PrepBuildPresentationRules::ResolveMinorDisplayName(TEXT("FutureMinor"))
			.EqualTo(FText::FromString(TEXT("FutureMinor"))));
	TestTrue(TEXT("소분류가 없는 데이터는 미분류로 안내한다"),
		FFF_PrepBuildPresentationRules::ResolveMinorDisplayName(NAME_None)
			.EqualTo(FText::FromString(TEXT("미분류"))));

	FFF_BuildCatalogEntry Entry;
	Entry.FacilityDefinitionId = TEXT("Booth_FoodStall_UniqueTruck");
	Entry.UniqueNameToken = TEXT("UniqueTruck");
	Entry.DisplayName = FText::FromString(TEXT("이름이 긴 우리 동네 푸드 트럭"));
	TestEqual(TEXT("표시 이름이 있으면 고유 토큰을 중복해서 붙이지 않는다"),
		FFF_PrepBuildPresentationRules::ResolveDisplayName(Entry), Entry.DisplayName);
	Entry.DisplayName = FText::FromString(TEXT("   "));
	TestTrue(TEXT("표시 이름이 비어 있으면 고유 이름으로 대체한다"),
		FFF_PrepBuildPresentationRules::ResolveDisplayName(Entry)
			.EqualTo(FText::FromString(TEXT("UniqueTruck"))));
	Entry.UniqueNameToken.Reset();
	TestTrue(TEXT("고유 이름도 없으면 유효한 배치 ID로 구분한다"),
		FFF_PrepBuildPresentationRules::ResolveDisplayName(Entry)
			.EqualTo(FText::FromName(Entry.FacilityDefinitionId)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_PrepBuildPresentation_ThumbnailLayoutTest,
	"FestaFiesta.UI.PrepBuild.ThumbnailAspectFitsFixedFrame",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildPresentation_ThumbnailLayoutTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FIntPoint Sizes[] = {
		FIntPoint(32, 32), FIntPoint(4096, 4096), FIntPoint(1024, 256), FIntPoint(256, 2048)
	};
	for (const FIntPoint Size : Sizes)
	{
		const FVector2D Fit = FFPrepBuildTileLayout::FitThumbnailSize(Size.X, Size.Y);
		TestTrue(TEXT("해상도와 무관하게 썸네일이 150x70 프레임을 넘지 않는다"),
			Fit.X > 0.0 && Fit.X <= FFPrepBuildTileLayout::ThumbnailWidth
			&& Fit.Y > 0.0 && Fit.Y <= FFPrepBuildTileLayout::ThumbnailHeight);
		TestTrue(TEXT("원본 이미지의 가로세로 비율을 유지한다"),
			FMath::IsNearlyEqual(Fit.X / Fit.Y, static_cast<double>(Size.X) / Size.Y));
		TestTrue(TEXT("프레임 안에서 적어도 한 축은 가득 채운다"),
			FMath::IsNearlyEqual(Fit.X, static_cast<double>(FFPrepBuildTileLayout::ThumbnailWidth))
			|| FMath::IsNearlyEqual(Fit.Y, static_cast<double>(FFPrepBuildTileLayout::ThumbnailHeight)));
	}
	TestEqual(TEXT("작거나 큰 정사각 이미지의 표시 크기는 같다"),
		FFPrepBuildTileLayout::FitThumbnailSize(32, 32), FFPrepBuildTileLayout::FitThumbnailSize(4096, 4096));
	TestEqual(TEXT("이미지 미로딩 상태도 프레임 크기가 유효하다"),
		FFPrepBuildTileLayout::FitThumbnailSize(0, 0), FVector2D(150.0, 70.0));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
