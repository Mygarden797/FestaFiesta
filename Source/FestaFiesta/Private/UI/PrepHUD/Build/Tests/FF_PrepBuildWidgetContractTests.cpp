// FF_PrepBuildWidgetContractTests.cpp
// 역할: 준비 단계 건설 화면의 동작과 저장된 화면 연결을 자동 검증한다.

// Generated PrepBuild WBP assets and cook-label contracts.

#include "Misc/AutomationTest.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Engine/PrimaryAssetLabel.h"
#include "Components/HorizontalBox.h"
#include "UI/PrepHUD/Build/FF_PrepBuildCategoryChoice.h"
#include "UI/PrepHUD/Build/FF_PrepBuildPresentationRules.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"
#include "UI/PrepHUD/Panels/FF_PrepChoiceButton.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"

namespace FFPrepBuildWidgetContractTest
{
	constexpr const TCHAR* TileClassPath =
		FFBuildUIAssetPaths::PrepBuildTileClass;
	constexpr const TCHAR* PanelClassPath =
		FFBuildUIAssetPaths::BuildPanelClass;
	constexpr const TCHAR* CookLabelPath =
		FFBuildUIAssetPaths::PAL_FF_PrepBuildUIObject;
	constexpr const TCHAR* ChoiceClassPath =
		FFBuildUIAssetPaths::PrepChoiceButtonClass;
	constexpr const TCHAR* ChoiceAssetPath =
		FFBuildUIAssetPaths::PrepChoiceButtonObject;
	constexpr const TCHAR* CategoryDependencyCookLabelPath =
		FFBuildUIAssetPaths::PAL_FF_PrepBuildCategoryDependenciesObject;

	UWidgetTree* ResolveWidgetTree(UClass* WidgetClass)
	{
		const UWidgetBlueprintGeneratedClass* GeneratedClass =
			Cast<UWidgetBlueprintGeneratedClass>(WidgetClass);
		return GeneratedClass ? GeneratedClass->GetWidgetTreeArchetype() : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_PrepBuildWidgetContract_Assets,
	"FestaFiesta.UI.PrepBuild.WidgetAssets",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildWidgetContract_Assets::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFPrepBuildWidgetContractTest;

	const UFF_BuildModeSubsystem* BuildModeDefaults = GetDefault<UFF_BuildModeSubsystem>();
	TestFalse(TEXT("기존 전체화면 카탈로그 경로가 기본값으로 유지된다"),
		BuildModeDefaults->IsExternalCatalogPresentationActive());
	TestFalse(TEXT("텍스트 입력 포커스 중 B 키는 BuildMode 토글로 소비하지 않는다"),
		FFBuildModeModel::ShouldProcessRawBuildKey(false, true, true));
	TestTrue(TEXT("Slate 단일행 편집 위젯을 텍스트 입력으로 식별한다"),
		FFBuildModeModel::IsEditableSlateTextWidgetType(TEXT("SEditableText")));
	TestFalse(TEXT("일반 버튼 포커스는 B 단축키를 막지 않는다"),
		FFBuildModeModel::IsEditableSlateTextWidgetType(TEXT("SButton")));
	const TArray<FKey> PauseKeys = { EKeys::Escape, EKeys::F5 };
	TestTrue(TEXT("검색창 child보다 먼저 ESC를 Pause Hub 입력으로 처리한다"),
		FFPrepBuildPauseModel::ShouldHandlePauseMenuKey(
			EKeys::Escape, false, true, false, EFF_BuildModeState::CatalogOpen, PauseKeys));
	TestFalse(TEXT("건축 중 F5는 Pause Hub 단축키로 처리하지 않는다"),
		FFPrepBuildPauseModel::ShouldHandlePauseMenuKey(
			EKeys::F5, false, true, false, EFF_BuildModeState::CatalogOpen, PauseKeys));
	TestFalse(TEXT("키 반복은 Pause Hub를 중복 요청하지 않는다"),
		FFPrepBuildPauseModel::ShouldHandlePauseMenuKey(
			EKeys::Escape, true, true, false, EFF_BuildModeState::CatalogOpen, PauseKeys));
	TestFalse(TEXT("카탈로그가 아닌 건축 상태에서는 panel이 Pause 키를 가로채지 않는다"),
		FFPrepBuildPauseModel::ShouldHandlePauseMenuKey(
			EKeys::Escape, false, true, false, EFF_BuildModeState::BuildMode, PauseKeys));
	TestFalse(TEXT("일반 검색 입력은 기존 child 처리로 보낸다"),
		FFPrepBuildPauseModel::ShouldHandlePauseMenuKey(
			EKeys::A, false, true, false, EFF_BuildModeState::CatalogOpen, PauseKeys));

	UClass* TileClass = LoadClass<UFF_PrepBuildTileWidget>(nullptr, TileClassPath);
	TestNotNull(TEXT("가상화 타일 WBP 클래스를 로드한다"), TileClass);
	TestTrue(TEXT("타일 WBP는 전용 네이티브 타일을 상속한다"),
		TileClass && TileClass->IsChildOf(UFF_PrepBuildTileWidget::StaticClass()));

	UWidgetTree* TileTree = ResolveWidgetTree(TileClass);
	TestNotNull(TEXT("타일 WBP의 WidgetTree가 저장됐다"), TileTree);
	if (TileTree)
	{
		for (const FName RequiredName : {
			FName(TEXT("Border_Card")),
			FName(TEXT("Image_Thumbnail")),
			FName(TEXT("Text_Name")),
			FName(TEXT("Text_Cost")),
			FName(TEXT("Text_State")),
			FName(TEXT("Button_Favorite")),
			FName(TEXT("Text_Favorite")),
			FName(TEXT("Border_RequiredMissing")),
			FName(TEXT("Text_RequiredMissing")),
			FName(TEXT("Border_RequiredSatisfied")),
			FName(TEXT("Text_RequiredSatisfied")),
			FName(TEXT("Border_ProgramBadge")),
			FName(TEXT("Text_ProgramBadge")),
			FName(TEXT("SizeBox_Card")) })
		{
			TestNotNull(
				*FString::Printf(TEXT("타일 계약 위젯이 존재한다: %s"), *RequiredName.ToString()),
				TileTree->FindWidget(RequiredName));
		}
#if WITH_EDITORONLY_DATA
		for (const FName Conditional : { FName(TEXT("Border_RequiredMissing")), FName(TEXT("Border_RequiredSatisfied")),
			FName(TEXT("Border_ProgramBadge")), FName(TEXT("Border_SynergyHighlight")),
			FName(TEXT("Image_CodexFocusRim")), FName(TEXT("Border_CodexFocusBadge")), FName(TEXT("Text_State")) })
		{
			const UWidget* Widget = TileTree->FindWidget(Conditional);
			TestTrue(TEXT("Default Designer card hides optional states instead of stacking them"), Widget && Widget->bHiddenInDesigner);
		}
#endif
	}

	UClass* PanelClass = LoadClass<UFF_PrepPanelBuild>(nullptr, PanelClassPath);
	TestNotNull(TEXT("PrepHUD Build 패널 WBP 클래스를 로드한다"), PanelClass);
	TestTrue(TEXT("패널 WBP는 PrepPanelBuild를 상속한다"),
		PanelClass && PanelClass->IsChildOf(UFF_PrepPanelBuild::StaticClass()));

	UWidgetTree* PanelTree = ResolveWidgetTree(PanelClass);
	TestNotNull(TEXT("패널 WBP의 WidgetTree가 저장됐다"), PanelTree);
	if (PanelTree)
	{
		for (const FName RequiredName : {
			FName(TEXT("Panel_Catalog")),
			FName(TEXT("HorizontalBox_Categories")),
			FName(TEXT("HorizontalBox_Subcategories")),
			FName(TEXT("Choice_AllSubcategories")),
			FName(TEXT("Row_BuildCodexCategory")),
			FName(TEXT("Widget_FacilityCodexCatalogLauncher")),
			FName(TEXT("Button_RevenueRanking")),
			FName(TEXT("Button_FavoritesOnly")),
			FName(TEXT("Text_FavoritesOnly")),
			FName(TEXT("TileView_Facilities")),
			FName(TEXT("Search_Facilities")) })
		{
			TestNotNull(
				*FString::Printf(TEXT("패널 계약 위젯이 존재한다: %s"), *RequiredName.ToString()),
				PanelTree->FindWidget(RequiredName));
		}
		for (const FName RetiredName : { FName(TEXT("Text_Gold")), FName(TEXT("Box_Header")),
			FName(TEXT("Border_AuroraSelection")), FName(TEXT("HorizontalBox_Tools")),
			FName(TEXT("Panel_PlacementCompact")), FName(TEXT("Border_CatalogFooter")) })
			TestNull(*FString::Printf(TEXT("Retired surface is deleted: %s"), *RetiredName.ToString()), PanelTree->FindWidget(RetiredName));
		const auto* Categories = Cast<UHorizontalBox>(PanelTree->FindWidget(TEXT("HorizontalBox_Categories")));
		if (TestNotNull(TEXT("Saved editable category row"), Categories))
		{
			TSet<int32> Ids;
			for (UWidget* Child : Categories->GetAllChildren())
			{
				const auto* Choice = Cast<UFF_PrepBuildCategoryChoice>(Child);
				if (TestNotNull(TEXT("Category is a saved editable choice"), Choice))
				{
					TestFalse(TEXT("Each saved tab has an authored label"), Choice->AuthoredLabel.IsEmpty());
					Ids.Add(Choice->AuthoredChoiceId);
				}
			}
			TestTrue(TEXT("Saved categories contain All"), Ids.Contains(INDEX_NONE));
			for (auto Category : FFF_PrepBuildPresentationRules::GetCategoryOrder())
				TestTrue(TEXT("Saved categories contain every production category"), Ids.Contains(static_cast<int32>(Category)));
			TestEqual(TEXT("Saved tabs have no duplicated category IDs"), Ids.Num(), Categories->GetChildrenCount());
		}

		const UFF_PrepBuildTileView* TileView =
			Cast<UFF_PrepBuildTileView>(PanelTree->FindWidget(TEXT("TileView_Facilities")));
		TestNotNull(TEXT("시설 목록은 UTileView 기반 전용 가상화 위젯이다"), TileView);
		TestTrue(TEXT("TileView entry class는 생성된 LightTeal 타일 WBP다"),
			TileView && TileView->GetEntryWidgetClass() == TileClass);
#if WITH_EDITORONLY_DATA
		const UWidget* Empty = PanelTree->FindWidget(TEXT("Panel_Empty"));
		TestTrue(TEXT("Default Designer catalog hides the empty-results overlay"), Empty && Empty->bHiddenInDesigner);
		TestTrue(TEXT("Default Designer catalog displays its preview card list"), TileView && !TileView->bHiddenInDesigner);
#endif
	}

	UClass* ChoiceClass = LoadClass<UFF_PrepChoiceButton>(nullptr, ChoiceClassPath);
	TestNotNull(TEXT("전체 카테고리 행이 재사용할 LightTeal ChoiceButton을 로드한다"), ChoiceClass);
	TestTrue(TEXT("카테고리 칩은 PrepChoiceButton 계약을 따른다"),
		ChoiceClass && ChoiceClass->IsChildOf(UFF_PrepChoiceButton::StaticClass()));
	UWidgetTree* ChoiceTree = ResolveWidgetTree(ChoiceClass);
	TestNotNull(TEXT("카테고리 ChoiceButton의 WidgetTree가 저장됐다"), ChoiceTree);
	if (ChoiceTree)
	{
		TestNotNull(TEXT("카테고리 ChoiceButton의 클릭 루트가 존재한다"),
			ChoiceTree->FindWidget(TEXT("Button_Root")));
		TestNotNull(TEXT("카테고리 ChoiceButton의 한글 라벨 자리가 존재한다"),
			ChoiceTree->FindWidget(TEXT("Text_Label")));
	}

	const UPrimaryAssetLabel* CookLabel =
		LoadObject<UPrimaryAssetLabel>(nullptr, CookLabelPath);
	TestNotNull(TEXT("PrepBuild AlwaysCook 라벨을 로드한다"), CookLabel);
	if (CookLabel)
	{
		TestTrue(TEXT("Cook label 자체를 런타임 에셋으로 보존한다"), CookLabel->bIsRuntimeLabel);
		TestEqual(TEXT("두 신규 WBP를 명시적으로 참조한다"), CookLabel->ExplicitAssets.Num(), 2);
		TestEqual(TEXT("패키징 규칙은 AlwaysCook이다"),
			CookLabel->Rules.CookRule, EPrimaryAssetCookRule::AlwaysCook);
	}

	const UPrimaryAssetLabel* CategoryDependencyCookLabel =
		LoadObject<UPrimaryAssetLabel>(nullptr, CategoryDependencyCookLabelPath);
	TestNotNull(TEXT("카테고리 칩 패키징 라벨을 로드한다"), CategoryDependencyCookLabel);
	if (CategoryDependencyCookLabel)
	{
		TestTrue(TEXT("카테고리 의존성 라벨은 런타임 라벨이다"),
			CategoryDependencyCookLabel->bIsRuntimeLabel);
		TestEqual(TEXT("카테고리 의존성은 공용 ChoiceButton 하나만 명시한다"),
			CategoryDependencyCookLabel->ExplicitAssets.Num(), 1);
		TestEqual(TEXT("카테고리 의존성은 AlwaysCook이다"),
			CategoryDependencyCookLabel->Rules.CookRule, EPrimaryAssetCookRule::AlwaysCook);
		TestTrue(TEXT("카테고리 의존성 라벨이 ChoiceButton WBP를 가리킨다"),
			CategoryDependencyCookLabel->ExplicitAssets.ContainsByPredicate(
				[](const TSoftObjectPtr<UObject>& Asset)
				{
					return Asset.ToSoftObjectPath() == FSoftObjectPath(ChoiceAssetPath);
				}));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
