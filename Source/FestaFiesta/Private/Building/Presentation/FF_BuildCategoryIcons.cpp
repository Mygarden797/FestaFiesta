// FF_BuildCategoryIcons.cpp
// 역할: 시설 분류에 맞는 아이콘을 찾고, 새 도형 아이콘이 없으면 기존 그림으로 대신 표시한다.

#include "Building/Presentation/FF_BuildCategoryIcons.h"

#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"

namespace FFBuildCategoryIconDetails
{
    // 한 줄이 한 분류의 새 아이콘과 예비 아이콘을 함께 정의한다. 분류를 추가할 때 이 표를 수정한다.
    struct FIconDefinition
    {
        EFFZSFacilityCategory Category;
        const TCHAR* Glyph;
        const TCHAR* FallbackFolder;
        const TCHAR* FallbackIcon;
    };

    constexpr FIconDefinition Definitions[] = {
        { EFFZSFacilityCategory::Booth,       TEXT("Booth"),       TEXT("Facilities"), TEXT("FacilityFood") },
        { EFFZSFacilityCategory::Toilet,      TEXT("Toilet"),      TEXT("Needs"),      TEXT("NeedRestroom") },
        { EFFZSFacilityCategory::Info,        TEXT("Info"),        TEXT("Facilities"), TEXT("FacilityInfo") },
        { EFFZSFacilityCategory::Rest,        TEXT("Rest"),        TEXT("Needs"),      TEXT("NeedRest") },
        { EFFZSFacilityCategory::Performance, TEXT("Performance"), TEXT("Facilities"), TEXT("FacilityStage") },
        { EFFZSFacilityCategory::Safety,      TEXT("Safety"),      TEXT("Metrics"),    TEXT("Safety") },
        { EFFZSFacilityCategory::PhotoSpot,   TEXT("PhotoSpot"),   TEXT("Actions"),    TEXT("Locate") },
        { EFFZSFacilityCategory::Lighting,    TEXT("Lighting"),    TEXT("Status"),     TEXT("StatusLive") },
        { EFFZSFacilityCategory::Road,        TEXT("Road"),        TEXT("Navigation"), TEXT("Queues") },
        { EFFZSFacilityCategory::Tile,        TEXT("Tile"),        TEXT("Navigation"), TEXT("Facilities") },
    };
    constexpr FIconDefinition AllCategories = {
        EFFZSFacilityCategory::Booth, TEXT("All"), TEXT("Navigation"), TEXT("Facilities")
    };
    constexpr float GlyphSize = 22.0f;
    constexpr float FallbackSize = 16.0f;

    // '전체'와 아직 등록되지 않은 분류는 기존 동작처럼 전체 시설 아이콘으로 표시한다.
    const FIconDefinition& FindDefinition(const bool bAll, const EFFZSFacilityCategory Category)
    {
        if (!bAll)
        {
            for (const FIconDefinition& Definition : Definitions)
            {
                if (Definition.Category == Category) return Definition;
            }
        }
        return AllCategories;
    }

    // 재질과 텍스처 모두 같은 브러시 설정을 사용한다. 자원이 없으면 빈 브러시를 돌려준다.
    FSlateBrush MakeBrush(UObject* Resource, const float Size)
    {
        FSlateBrush Brush;
        Brush.SetImageSize(FVector2D(Size, Size));
        Brush.TintColor = FSlateColor(FLinearColor::White);
        if (Resource)
        {
            Brush.SetResourceObject(Resource);
            Brush.DrawAs = ESlateBrushDrawType::Image;
        }
        return Brush;
    }
}

FSlateBrush FFBuildCategoryIcons::Resolve(const bool bAll, const EFFZSFacilityCategory Category)
{
    using namespace FFBuildCategoryIconDetails;
    const FIconDefinition& Definition = FindDefinition(bAll, Category);

    const FString GlyphAsset = FString::Printf(TEXT("MI_UI_CategoryClarity_%s"), Definition.Glyph);
    const FString GlyphPath = FString(FFBuildUIAssetPaths::ClarityFolder) /
        (GlyphAsset + TEXT(".") + GlyphAsset);
    if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *GlyphPath, nullptr, LOAD_NoWarn))
    {
        return MakeBrush(Material, GlyphSize);
    }

    // 에셋 이관 전에도 분류 버튼을 쓸 수 있도록 기존 LiveHUD 그림을 예비 자원으로 사용한다.
    const FString AssetName = FString::Printf(TEXT("T_FF_LH_Icon_%s_White_256"), Definition.FallbackIcon);
    const FString ObjectPath = FString(FFBuildUIAssetPaths::FallbackCategoryIconFolder) /
        Definition.FallbackFolder / (AssetName + TEXT(".") + AssetName);
    return MakeBrush(LoadObject<UTexture2D>(nullptr, *ObjectPath), FallbackSize);
}
