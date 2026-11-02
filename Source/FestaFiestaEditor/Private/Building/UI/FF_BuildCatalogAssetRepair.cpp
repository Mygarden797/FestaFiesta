// 역할: 누락된 구형 카탈로그 자원을 현재 프로젝트의 저작 아이콘으로 복구한다.
// 기존 자원은 덮어쓰지 않는다. 원본 아이콘과 5열×2행 인덱스 계약을 확인한 뒤 신규 패키지만 저장한다.
#include "CoreMinimal.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Engine/Texture2D.h"
#include "FileHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFBuildCatalogAssetRepair, Log, All);

namespace FFBuildCatalogAssetRepair
{
constexpr int32 CellSize = 256;
constexpr int32 Columns = 5;
constexpr int32 Rows = 2;

static bool ReadIcon(const TCHAR* Relative, TArray64<uint8>& Pixels)
{
    const FString Name = FString::Printf(TEXT("T_FF_LH_Icon_%s_White_256"), *FPaths::GetCleanFilename(Relative));
    const FString Path = TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/") +
        FPaths::GetPath(Relative) / (Name + TEXT(".") + Name);
    UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *Path);
    TArray64<uint8> Source;
    if (!Texture || Texture->Source.GetSizeX() != CellSize || Texture->Source.GetSizeY() != CellSize ||
        !Texture->Source.GetMipData(Source, 0))
    {
        UE_LOG(LogFFBuildCatalogAssetRepair, Error, TEXT("원본 아이콘을 읽을 수 없습니다: %s"), *Path);
        return false;
    }
    Pixels.SetNumUninitialized(CellSize * CellSize * 4);
    if (Texture->Source.GetFormat() == TSF_BGRA8 && Source.Num() == Pixels.Num())
    {
        FMemory::Memcpy(Pixels.GetData(), Source.GetData(), Pixels.Num());
        return true;
    }
    if (Texture->Source.GetFormat() == TSF_G8 && Source.Num() == CellSize * CellSize)
    {
        for (int32 Index = 0; Index < CellSize * CellSize; ++Index)
        {
            Pixels[Index * 4] = Pixels[Index * 4 + 1] = Pixels[Index * 4 + 2] = 255;
            Pixels[Index * 4 + 3] = Source[Index];
        }
        return true;
    }
    UE_LOG(LogFFBuildCatalogAssetRepair, Error, TEXT("지원하지 않는 아이콘 원본 형식: %s"), *Path);
    return false;
}

static UTexture2D* CreateMissing(const TCHAR* Path, int32 Width, int32 Height, const TArray64<uint8>& Pixels)
{
    const FString PackageName(Path);
    if (FPackageName::DoesPackageExist(PackageName))
    {
        return LoadObject<UTexture2D>(nullptr, *(PackageName + TEXT(".") + FPackageName::GetLongPackageAssetName(PackageName)));
    }
    UPackage* Package = CreatePackage(*PackageName);
    UTexture2D* Texture = NewObject<UTexture2D>(Package,
        *FPackageName::GetLongPackageAssetName(PackageName), RF_Public | RF_Standalone);
    Texture->Source.Init(Width, Height, 1, 1, TSF_BGRA8, Pixels.GetData());
    Texture->LODGroup = TEXTUREGROUP_UI;
    Texture->SRGB = true;
    Texture->MipGenSettings = TMGS_NoMipmaps;
    Texture->CompressionSettings = TC_EditorIcon;
    Texture->NeverStream = true;
    Texture->PostEditChange();
    Texture->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Texture);
    return Texture;
}

static void Repair()
{
    // MakeAtlasIconBrush의 0=건설, 1=꾸미기, 2=전체, 3~9=시설 분류 순서를 그대로 사용한다.
    // 꾸미기 탭은 현재 저작된 조절 아이콘으로 표시하고 분류 아이콘은 실제 HUD 자원을 공유한다.
    const TCHAR* Icons[] = {
        TEXT("Navigation/Facilities"), TEXT("Actions/Equalizer"), TEXT("Navigation/Facilities"),
        TEXT("Facilities/FacilityFood"), TEXT("Needs/NeedRestroom"), TEXT("Facilities/FacilityInfo"),
        TEXT("Needs/NeedRest"), TEXT("Facilities/FacilityStage"), TEXT("Metrics/Safety"), TEXT("Actions/Locate")
    };
    TArray64<uint8> Atlas;
    Atlas.SetNumZeroed(CellSize * Columns * CellSize * Rows * 4);
    TArray64<uint8> Food;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Icons); ++Index)
    {
        TArray64<uint8> Icon;
        if (!ReadIcon(Icons[Index], Icon)) return;
        if (Index == 3) Food = Icon;
        for (int32 Y = 0; Y < CellSize; ++Y)
        {
            const int64 Destination = ((Index / Columns * CellSize + Y) * CellSize * Columns +
                Index % Columns * CellSize) * 4LL;
            FMemory::Memcpy(Atlas.GetData() + Destination, Icon.GetData() + Y * CellSize * 4LL, CellSize * 4);
        }
    }
    TArray64<uint8> Backdrop;
    // WBP의 Background_Dim 브러시 색·투명도가 색상을 결정한다. 흰 원본은 디자이너 팔레트를 보존한다.
    Backdrop.Init(255, 16 * 16 * 4);
    const FString BackdropPackage = FString(FFBuildUIAssetPaths::CatalogImageFolder) / TEXT("T_UI_BuildCatalog_Backdrop");
    TArray<UTexture2D*> Textures = {
        CreateMissing(FFBuildUIAssetPaths::CategoryIconAtlasPackage, CellSize * Columns, CellSize * Rows, Atlas),
        CreateMissing(FFBuildUIAssetPaths::FoodTruckImagePackage, CellSize, CellSize, Food),
        CreateMissing(*BackdropPackage, 16, 16, Backdrop)
    };
    TArray<UPackage*> Packages;
    for (UTexture2D* Texture : Textures)
    {
        if (!Texture || !Texture->Source.IsValid())
        {
            UE_LOG(LogFFBuildCatalogAssetRepair, Error, TEXT("카탈로그 자원 복구 검증 실패"));
            return;
        }
        if (Texture->GetOutermost()->IsDirty()) Packages.AddUnique(Texture->GetOutermost());
    }
    const bool Saved = Packages.IsEmpty() || UEditorLoadingAndSavingUtils::SavePackages(Packages, true);
    UE_LOG(LogFFBuildCatalogAssetRepair, Display, TEXT("FF_BUILD_CATALOG_ASSET_REPAIR_%s: %d packages"),
        Saved ? TEXT("PASS") : TEXT("FAIL"), Packages.Num());
}

static FAutoConsoleCommand RepairCommand(TEXT("FF.RepairBuildCatalogAssets"),
    TEXT("Restore only missing catalog atlas/food-placeholder/backdrop resources from the current authored HUD icons."),
    FConsoleCommandDelegate::CreateStatic(&Repair));
}
