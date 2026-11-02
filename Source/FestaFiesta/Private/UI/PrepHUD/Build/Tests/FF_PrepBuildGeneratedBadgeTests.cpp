#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "UI/Core/FF_BuildAtlasImage.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFPrepBuildGeneratedBadgesTest,
	"FestaFiesta.UI.PrepBuild.GeneratedBadgeAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFFPrepBuildGeneratedBadgesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepBuildTileWidget>(nullptr,
		TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepBuildTile.WBP_FF_PrepBuildTile_C")));
	if (!TestNotNull(TEXT("Editor world"), World) || !TestNotNull(TEXT("Actual facility-card WBP"), Class)) return false;
	const bool Previous = Class->bCanCallInitializedWithoutPlayerContext;
	Class->bCanCallInitializedWithoutPlayerContext = true;
	TStrongObjectPtr<UFF_PrepBuildTileWidget> Tile(CreateWidget<UFF_PrepBuildTileWidget>(World, Class));
	Class->bCanCallInitializedWithoutPlayerContext = Previous;
	if (!TestNotNull(TEXT("Facility-card instance"), Tile.Get())) return false;
	const TSharedRef<SWidget> Slate = Tile->TakeWidget();
	(void)Slate;
	auto* Required = Cast<UFF_BuildAtlasImage>(Tile->GetWidgetFromName(TEXT("Image_ApprovedRequired")));
	auto* Lighting = Cast<UFF_BuildAtlasImage>(Tile->GetWidgetFromName(TEXT("Image_ApprovedLighting")));
	auto* Favorite = Cast<UFF_BuildAtlasImage>(Tile->GetWidgetFromName(TEXT("Image_ApprovedFavorite")));
	auto* Program = Cast<UImage>(Tile->GetWidgetFromName(TEXT("Image_A5Program")));
	auto* RequiredContainer = Cast<UBorder>(Tile->GetWidgetFromName(TEXT("Badge_RequiredIcon")));
	auto* LegacyFavorite = Cast<UTextBlock>(Tile->GetWidgetFromName(TEXT("Text_Favorite")));
	if (!TestNotNull(TEXT("Required icon"), Required) || !TestNotNull(TEXT("Lighting icon"), Lighting) ||
		!TestNotNull(TEXT("Favorite icon"), Favorite) || !TestNotNull(TEXT("Program icon"), Program) ||
		!TestNotNull(TEXT("Required state container"), RequiredContainer) ||
		!TestNotNull(TEXT("Legacy favorite fallback"), LegacyFavorite)) return false;
	const auto CheckAtlas = [this](const TCHAR* Name, UFF_BuildAtlasImage* Image,
		const TCHAR* TexturePath, const int32 Columns, const int32 Rows, const int32 Index)
	{
		TestTrue(FString::Printf(TEXT("%s uses approved atlas and cell"), Name), Image->AtlasTexture &&
			Image->AtlasTexture->GetPathName() == TexturePath && Image->AtlasColumns == Columns &&
			Image->AtlasRows == Rows && Image->AtlasIndex == Index &&
			Image->GetBrush().GetResourceObject() == Image->AtlasTexture.Get());
	};
	CheckAtlas(TEXT("Required"), Required,
		TEXT("/Game/FestaFiesta/UI/Generated/Building/T_FF_BuildIconAtlas.T_FF_BuildIconAtlas"), 5, 5, 22);
	CheckAtlas(TEXT("Lighting"), Lighting,
		TEXT("/Game/FestaFiesta/UI/Generated/Building/T_FF_BuildIconAtlas.T_FF_BuildIconAtlas"), 5, 5, 23);
	CheckAtlas(TEXT("Favorite"), Favorite,
		TEXT("/Game/FestaFiesta/UI/Generated/Building/T_FF_BuildUtilityAtlas.T_FF_BuildUtilityAtlas"), 4, 3, 0);
	TestTrue(TEXT("Program uses the separately approved transparent icon"),
		Program->GetBrush().GetResourceObject() &&
		Program->GetBrush().GetResourceObject()->GetPathName() ==
			TEXT("/Game/FestaFiesta/UI/Generated/Building/T_FF_ProgramBadge.T_FF_ProgramBadge"));
	Tile->SetRequiredFacilityStatus(true, false, 0, 1);
	TestEqual(TEXT("Required icon appears for required facilities"), RequiredContainer->GetVisibility(), ESlateVisibility::HitTestInvisible);
	Tile->SetRequiredFacilityStatus(false, false, 0, 0);
	TestEqual(TEXT("Required icon hides for other facilities"), RequiredContainer->GetVisibility(), ESlateVisibility::Collapsed);
	TestTrue(TEXT("Favorite exposes selected cell"), Favorite->SetAtlasIndex(1));
	const FBox2f SelectedUV = Favorite->GetBrush().GetUVRegion();
	TestTrue(TEXT("Favorite selected state uses generated gold star"),
		SelectedUV.Min.Equals(FVector2f(.25f, 0.f), .0001f) && SelectedUV.Max.Equals(FVector2f(.5f, 1.f / 3.f), .0001f));
	Tile->ReleaseSlateResources(true);
	return true;
}

#endif
