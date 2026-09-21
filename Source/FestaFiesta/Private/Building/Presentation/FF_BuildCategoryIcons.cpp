#include "Building/Presentation/FF_BuildCategoryIcons.h"

#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"

FSlateBrush FFBuildCategoryIcons::Resolve(const bool bAll, const EFFZSFacilityCategory Category)
{
	const TCHAR* GlyphName = TEXT("All");
	if (!bAll)
	{
		switch (Category)
		{
		case EFFZSFacilityCategory::Booth: GlyphName = TEXT("Booth"); break;
		case EFFZSFacilityCategory::Toilet: GlyphName = TEXT("Toilet"); break;
		case EFFZSFacilityCategory::Info: GlyphName = TEXT("Info"); break;
		case EFFZSFacilityCategory::Rest: GlyphName = TEXT("Rest"); break;
		case EFFZSFacilityCategory::Performance: GlyphName = TEXT("Performance"); break;
		case EFFZSFacilityCategory::Safety: GlyphName = TEXT("Safety"); break;
		case EFFZSFacilityCategory::PhotoSpot: GlyphName = TEXT("PhotoSpot"); break;
		case EFFZSFacilityCategory::Lighting: GlyphName = TEXT("Lighting"); break;
		case EFFZSFacilityCategory::Road: GlyphName = TEXT("Road"); break;
		case EFFZSFacilityCategory::Tile: GlyphName = TEXT("Tile"); break;
		default: break;
		}
	}
	const FString GlyphAsset = FString::Printf(TEXT("MI_UI_CategoryClarity_%s"), GlyphName);
	const FString GlyphPath = TEXT("/Game/FestaFiesta/UI/Building/Clarity/") + GlyphAsset + TEXT(".") + GlyphAsset;
	if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *GlyphPath, nullptr, LOAD_NoWarn))
	{
		FSlateBrush Pictogram;
		Pictogram.SetResourceObject(Material);
		Pictogram.DrawAs = ESlateBrushDrawType::Image;
		Pictogram.SetImageSize(FVector2D(22.f, 22.f));
		Pictogram.TintColor = FSlateColor(FLinearColor::White);
		return Pictogram;
	}
	// Existing artwork is a safe fallback until the explicit asset migration runs.
	const TCHAR* Folder = TEXT("Navigation");
	const TCHAR* IconName = TEXT("Facilities");
	if (!bAll)
	{
		switch (Category)
		{
		case EFFZSFacilityCategory::Booth:
			Folder = TEXT("Facilities");
			IconName = TEXT("FacilityFood");
			break;
		case EFFZSFacilityCategory::Toilet:
			Folder = TEXT("Needs");
			IconName = TEXT("NeedRestroom");
			break;
		case EFFZSFacilityCategory::Info:
			Folder = TEXT("Facilities");
			IconName = TEXT("FacilityInfo");
			break;
		case EFFZSFacilityCategory::Rest:
			Folder = TEXT("Needs");
			IconName = TEXT("NeedRest");
			break;
		case EFFZSFacilityCategory::Performance:
			Folder = TEXT("Facilities");
			IconName = TEXT("FacilityStage");
			break;
		case EFFZSFacilityCategory::Safety:
			Folder = TEXT("Metrics");
			IconName = TEXT("Safety");
			break;
		case EFFZSFacilityCategory::PhotoSpot:
			Folder = TEXT("Actions");
			IconName = TEXT("Locate");
			break;
		case EFFZSFacilityCategory::Lighting:
			Folder = TEXT("Status");
			IconName = TEXT("StatusLive");
			break;
		case EFFZSFacilityCategory::Road:
			IconName = TEXT("Queues");
			break;
		case EFFZSFacilityCategory::Tile:
			IconName = TEXT("Facilities");
			break;
		default:
			break;
		}
	}

	const FString AssetName = FString::Printf(TEXT("T_FF_LH_Icon_%s_White_256"), IconName);
	const FString ObjectPath = FString::Printf(
		TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/%s/%s.%s"),
		Folder, *AssetName, *AssetName);

	FSlateBrush Brush;
	Brush.SetImageSize(FVector2D(16.f, 16.f));
	Brush.TintColor = FSlateColor(FLinearColor::White);
	if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *ObjectPath))
	{
		Brush.SetResourceObject(Texture);
		Brush.DrawAs = ESlateBrushDrawType::Image;
	}
	return Brush;
}
