#include "Building/Presentation/FF_BuildCategoryIcons.h"

#include "Engine/Texture2D.h"

FSlateBrush FFBuildCategoryIcons::Resolve(const bool bAll, const EFFZSFacilityCategory Category)
{
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
