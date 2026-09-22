// FF_BuildUIAssetPaths.h
// 역할: 건설 화면·생성 도구·검증 코드가 함께 쓰는 기본 에셋 주소록이다.
// 패키지=에셋 파일, Object=저장된 에셋, Class=실행 시 만드는 위젯 클래스(_C)이다.
// 에셋 이동 시 이 주소록을 갱신하고, 이미 저장된 WBP/설정의 사용자 지정 경로는 별도로 확인한다.
#pragma once

#include "CoreMinimal.h"

namespace FFBuildUIAssetPaths
{

// /Game/FestaFiesta/Building/FF_WBP_Building
inline constexpr const TCHAR* CatalogFolder = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog");

// Catalog
inline constexpr const TCHAR* CatalogImageFolder = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Images");

// Catalog/Images
inline constexpr const TCHAR* FoodTruckImagePackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Images/T_UI_BuildCatalog_FoodTruck");
inline constexpr const TCHAR* FoodTruckImageObject = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Images/T_UI_BuildCatalog_FoodTruck.T_UI_BuildCatalog_FoodTruck");
inline constexpr const TCHAR* CategoryIconAtlasPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Images/T_UI_BuildCatalog_IconAtlas");
inline constexpr const TCHAR* CategoryIconAtlasObject = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Images/T_UI_BuildCatalog_IconAtlas.T_UI_BuildCatalog_IconAtlas");

// Catalog
inline constexpr const TCHAR* LegacyCatalogFolder = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Legacy");

// Catalog/Legacy
inline constexpr const TCHAR* CatalogScreenPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Legacy/WBP_FF_BuildCatalogScreen");
inline constexpr const TCHAR* CatalogScreenClass = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Legacy/WBP_FF_BuildCatalogScreen.WBP_FF_BuildCatalogScreen_C");
inline constexpr const TCHAR* CategoryButtonPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Legacy/WBP_FF_BuildCategoryButton");
inline constexpr const TCHAR* CategoryButtonClass = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Legacy/WBP_FF_BuildCategoryButton.WBP_FF_BuildCategoryButton_C");
inline constexpr const TCHAR* FacilityCardPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Legacy/WBP_FF_BuildFacilityCard");
inline constexpr const TCHAR* FacilityCardClass = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Legacy/WBP_FF_BuildFacilityCard.WBP_FF_BuildFacilityCard_C");
inline constexpr const TCHAR* MinorSectionPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Legacy/WBP_FF_BuildMinorSection");
inline constexpr const TCHAR* MinorSectionClass = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Legacy/WBP_FF_BuildMinorSection.WBP_FF_BuildMinorSection_C");
inline constexpr const TCHAR* TopButtonPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Legacy/WBP_FF_BuildTopButton");
inline constexpr const TCHAR* TopButtonClass = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Legacy/WBP_FF_BuildTopButton.WBP_FF_BuildTopButton_C");

// Catalog
inline constexpr const TCHAR* PAL_FF_PrepBuildCategoryDependenciesObject = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/PAL_FF_PrepBuildCategoryDependencies.PAL_FF_PrepBuildCategoryDependencies");
inline constexpr const TCHAR* PAL_FF_PrepBuildUIObject = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/PAL_FF_PrepBuildUI.PAL_FF_PrepBuildUI");
inline constexpr const TCHAR* BuildTilePackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepBuildTile");
inline constexpr const TCHAR* PrepBuildTileClass = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepBuildTile.WBP_FF_PrepBuildTile_C");
inline constexpr const TCHAR* BuildPanelPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepPanelBuild");
inline constexpr const TCHAR* BuildPanelClass = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepPanelBuild.WBP_FF_PrepPanelBuild_C");

// /Game/FestaFiesta/Building/FF_WBP_Building
inline constexpr const TCHAR* ControlsFolder = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Controls");

// Controls
inline constexpr const TCHAR* ClarityFolder = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Controls/Clarity");

// Controls/Clarity
inline constexpr const TCHAR* BuildClarity_HeaderObject = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Controls/Clarity/MI_UI_BuildClarity_Header.MI_UI_BuildClarity_Header");
inline constexpr const TCHAR* ClarityPaperObject = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Controls/Clarity/MI_UI_BuildClarity_Paper.MI_UI_BuildClarity_Paper");

// Controls
inline constexpr const TCHAR* CategoryChoicePackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Controls/WBP_FF_BuildCategoryChoice");
inline constexpr const TCHAR* BuildSynergyPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Controls/WBP_FF_BuildSynergy");
inline constexpr const TCHAR* BuildSynergyClass = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Controls/WBP_FF_BuildSynergy.WBP_FF_BuildSynergy_C");
inline constexpr const TCHAR* BuildToolsPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Controls/WBP_FF_BuildTools");
inline constexpr const TCHAR* BuildToolsObject = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Controls/WBP_FF_BuildTools.WBP_FF_BuildTools");
inline constexpr const TCHAR* BuildToolsClass = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Controls/WBP_FF_BuildTools.WBP_FF_BuildTools_C");

// /Game/FestaFiesta/Building/FF_WBP_Building
inline constexpr const TCHAR* DebugFolder = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Debug");

// Debug
inline constexpr const TCHAR* ZoneLifecycleTestPanelPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Debug/WBP_FF_ZoneLifecycleTestPanel");
inline constexpr const TCHAR* ZoneLifecycleTestPanelObject = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Debug/WBP_FF_ZoneLifecycleTestPanel.WBP_FF_ZoneLifecycleTestPanel");
inline constexpr const TCHAR* ZoneLifecycleTestPanelClass = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Debug/WBP_FF_ZoneLifecycleTestPanel.WBP_FF_ZoneLifecycleTestPanel_C");

// Facility
inline constexpr const TCHAR* FacilityInfoPanelPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityInfoPanel");
inline constexpr const TCHAR* FacilityInfoPanelObject = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityInfoPanel.WBP_FF_FacilityInfoPanel");
inline constexpr const TCHAR* FacilityInfoPanelClass = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityInfoPanel.WBP_FF_FacilityInfoPanel_C");
inline constexpr const TCHAR* FacilityLightingControlsPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityLightingControls");
inline constexpr const TCHAR* FacilityLightingControlsClass = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityLightingControls.WBP_FF_FacilityLightingControls_C");
inline constexpr const TCHAR* FacilityPaidProgramCardPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityPaidProgramCard");
inline constexpr const TCHAR* FacilityPaidProgramCardClass = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityPaidProgramCard.WBP_FF_FacilityPaidProgramCard_C");
inline constexpr const TCHAR* FacilityPaidProgramPickerPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityPaidProgramPicker");
inline constexpr const TCHAR* FacilityPaidProgramPickerClass = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityPaidProgramPicker.WBP_FF_FacilityPaidProgramPicker_C");
inline constexpr const TCHAR* FacilitySynergyEntryPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilitySynergyEntry");
inline constexpr const TCHAR* FacilitySynergyEntryClass = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilitySynergyEntry.WBP_FF_FacilitySynergyEntry_C");

// /Game/FestaFiesta/Building/FF_WBP_Building
inline constexpr const TCHAR* FacilityCodexFolder = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex");

// FacilityCodex/Icons
inline constexpr const TCHAR* T_FF_Codex_RewardGiftObject = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Icons/T_FF_Codex_RewardGift.T_FF_Codex_RewardGift");

// FacilityCodex/Materials
inline constexpr const TCHAR* M_FF_Codex_DiscoveryThumbnailObject = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Materials/M_FF_Codex_DiscoveryThumbnail.M_FF_Codex_DiscoveryThumbnail");

// FacilityCodex
inline constexpr const TCHAR* FacilityCodexCardPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexCard");
inline constexpr const TCHAR* FacilityCodexCatalogLauncherPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexCatalogLauncher");
inline constexpr const TCHAR* FacilityCodexCatalogLauncherClass = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexCatalogLauncher.WBP_FF_FacilityCodexCatalogLauncher_C");
inline constexpr const TCHAR* FacilityCodexLauncherPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexLauncher");
inline constexpr const TCHAR* FacilityCodexScreenPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexScreen");
inline constexpr const TCHAR* FacilityCodexScreenClass = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexScreen.WBP_FF_FacilityCodexScreen_C");

// /Game/FestaFiesta/Building/FF_WBP_Building
inline constexpr const TCHAR* FacilityRevenueFolder = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue");

// FacilityRevenue
inline constexpr const TCHAR* FacilityRevenueRankingPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue/WBP_FF_FacilityRevenueRanking");
inline constexpr const TCHAR* FacilityRevenueRankingObject = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue/WBP_FF_FacilityRevenueRanking.WBP_FF_FacilityRevenueRanking");
inline constexpr const TCHAR* FacilityRevenueRankingClass = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue/WBP_FF_FacilityRevenueRanking.WBP_FF_FacilityRevenueRanking_C");
inline constexpr const TCHAR* FacilityRevenueRankRowPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue/WBP_FF_FacilityRevenueRankRow");
inline constexpr const TCHAR* FacilityRevenueRankRowObject = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue/WBP_FF_FacilityRevenueRankRow.WBP_FF_FacilityRevenueRankRow");
inline constexpr const TCHAR* FacilityWorldBadgePackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue/WBP_FF_FacilityWorldBadge");
inline constexpr const TCHAR* FacilityWorldBadgeClass = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue/WBP_FF_FacilityWorldBadge.WBP_FF_FacilityWorldBadge_C");

// Feedback
inline constexpr const TCHAR* PlacementFailureFeedbackPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Feedback/WBP_FF_BuildPlacementFailureFeedback");
inline constexpr const TCHAR* PlacementFailureFeedbackObject = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Feedback/WBP_FF_BuildPlacementFailureFeedback.WBP_FF_BuildPlacementFailureFeedback");
inline constexpr const TCHAR* PlacementFailureFeedbackClass = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Feedback/WBP_FF_BuildPlacementFailureFeedback.WBP_FF_BuildPlacementFailureFeedback_C");

// Guide
inline constexpr const TCHAR* BuildModeGuidePackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Guide/WBP_FF_BuildModeGuide");
inline constexpr const TCHAR* BuildModeGuideClass = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Guide/WBP_FF_BuildModeGuide.WBP_FF_BuildModeGuide_C");

// /Game/FestaFiesta/Building/FF_WBP_Building
inline constexpr const TCHAR* SharedFolder = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Shared");

// Shared
inline constexpr const TCHAR* PrepChoiceButtonPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepChoiceButton");
inline constexpr const TCHAR* PrepChoiceButtonObject = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepChoiceButton.WBP_FF_PrepChoiceButton");
inline constexpr const TCHAR* PrepChoiceButtonClass = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepChoiceButton.WBP_FF_PrepChoiceButton_C");
inline constexpr const TCHAR* PrepHudBarPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar");
inline constexpr const TCHAR* PrepHudBarClass = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar.WBP_FF_PrepHudBar_C");
inline constexpr const TCHAR* PrepHudTabButtonPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudTabButton");

// Tooltips
inline constexpr const TCHAR* FacilityCostTooltipPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Tooltips/WBP_FF_BuildFacilityCostTooltip");
inline constexpr const TCHAR* FacilityCostTooltipObject = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Tooltips/WBP_FF_BuildFacilityCostTooltip.WBP_FF_BuildFacilityCostTooltip");
inline constexpr const TCHAR* FacilityCostTooltipClass = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Tooltips/WBP_FF_BuildFacilityCostTooltip.WBP_FF_BuildFacilityCostTooltip_C");

// /Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal
inline constexpr const TCHAR* FallbackCategoryIconFolder = TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons");

// 프로젝트 설정의 기본값. WBP의 Class Defaults와 프로젝트 설정에서 지정한 값이 우선한다.
inline constexpr const TCHAR* DefaultCatalogScreenClassPath = CatalogScreenClass;
inline constexpr const TCHAR* DefaultPlacementFailureFeedbackClassPath = PlacementFailureFeedbackClass;
inline constexpr const TCHAR* DefaultBuildModeGuideClassPath = BuildModeGuideClass;
inline constexpr const TCHAR* DefaultBuildToolsWidgetClassPath = BuildToolsClass;
inline constexpr const TCHAR* DefaultBuildSynergyWidgetClassPath = BuildSynergyClass;

// 과거 기본 경로만 이전한다. 디자이너가 지정한 별도 경로는 그대로 유지한다.
inline FString ResolveChoicePackage(const FString& Package)
{
    const FString OriginalPackage = FString(TEXT("/Game/FestaFiesta/UI/PrepHUD")) / TEXT("WBP_FF_PrepChoiceButton");
    return Package == OriginalPackage
        ? FString(SharedFolder) / TEXT("WBP_FF_PrepChoiceButton") : Package;
}

// 기본 이름 규칙을 쓰는 옛 화면만 호환 처리 대상인지 판별한다.
inline bool UsesOriginalPanelConvention(const FString& Folder, const FString& NameFormat)
{
    return Folder == FString(TEXT("/Game/FestaFiesta/UI/PrepHUD")) / TEXT("Panels") &&
        NameFormat == TEXT("WBP_FF_PrepPanel{0}");
}
}
