// Construction assets have their own folders; other preparation panels stay shared.
#pragma once

#include "CoreMinimal.h"

namespace FFBuildUIAssetPaths
{
inline constexpr const TCHAR* SharedFolder = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Shared");
inline constexpr const TCHAR* CatalogFolder = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog");
inline constexpr const TCHAR* BuildPanelClass = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepPanelBuild.WBP_FF_PrepPanelBuild_C");

inline FString ResolveChoicePackage(const FString& Package)
{
    // These old values can survive in an existing WBP as plain FString data;
    // AssetTools reference remapping does not rewrite strings. Match only the
    // shipped choice, so a designer's custom fallback remains authoritative.
    const FString OriginalPackage = FString(TEXT("/Game/FestaFiesta/UI/PrepHUD")) / TEXT("WBP_FF_PrepChoiceButton");
    return Package == OriginalPackage
        ? FString(SharedFolder) / TEXT("WBP_FF_PrepChoiceButton") : Package;
}

inline bool UsesOriginalPanelConvention(const FString& Folder, const FString& NameFormat)
{
    return Folder == FString(TEXT("/Game/FestaFiesta/UI/PrepHUD")) / TEXT("Panels") &&
        NameFormat == TEXT("WBP_FF_PrepPanel{0}");
}
}
