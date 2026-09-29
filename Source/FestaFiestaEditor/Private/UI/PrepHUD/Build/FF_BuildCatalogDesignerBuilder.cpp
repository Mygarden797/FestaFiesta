// Remove retired catalog surfaces and save the real category tabs in the editable WBP.
#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Building/Presentation/FF_BuildCategoryIcons.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "TextureCompiler.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/PrepHUD/Build/FF_PrepBuildCategoryChoice.h"
#include "UI/PrepHUD/Build/FF_PrepBuildPresentationRules.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprintEditorUtils.h"

namespace FFBuildCatalogDesigner
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Prefix = TEXT("FF_BUILD_CATALOG_DESIGNER");
constexpr const TCHAR* Version = TEXT("FF.BuildCatalogDesigner.20260929.v3");
constexpr const TCHAR* PreviousVersion = TEXT("FF.BuildCatalogDesigner.20260929.v2");

bool Applied(UWidgetBlueprint& BP)
{ return BP.GetOutermost()->GetMetaData().GetValue(&BP, Version) == TEXT("1"); }

bool AppliedPreviousVersion(UWidgetBlueprint& BP)
{ return BP.GetOutermost()->GetMetaData().GetValue(&BP, PreviousVersion) == TEXT("1"); }

bool SaveVersionOnly(UWidgetBlueprint& BP)
{
    BP.Modify();
    BP.GetOutermost()->GetMetaData().SetValue(&BP, Version, TEXT("1"));
    if (SaveAssetPackage(Prefix, BP)) return true;
    BP.GetOutermost()->GetMetaData().RemoveValue(&BP, Version);
    return false;
}

bool Save(UWidgetBlueprint& BP)
{
    BP.GetOutermost()->GetMetaData().SetValue(&BP, Version, TEXT("1"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    BP.GetOutermost()->GetMetaData().RemoveValue(&BP, Version);
    return false;
}

TSet<UWidget*> RetiredWidgets(UWidgetBlueprint& BP, std::initializer_list<const TCHAR*> Names)
{
    TSet<UWidget*> Result;
    for (const TCHAR* Name : Names)
        if (UWidget* Widget = BP.WidgetTree->FindWidget(Name)) Result.Add(Widget);
    return Result;
}

bool CanRemove(UWidgetBlueprint& BP, const TSet<UWidget*>& Widgets)
{
    TArray<UWidget*> Removed;
    for (UWidget* Widget : Widgets)
    {
        Removed.Add(Widget);
        UWidgetTree::GetChildWidgets(Widget, Removed);
    }
    for (UWidget* Widget : Removed)
    {
        if (FBlueprintEditorUtils::IsVariableUsed(&BP, Widget->GetFName()))
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL %s: retired widget %s is used by a custom Blueprint graph"),
                Prefix, *BP.GetName(), *Widget->GetName());
            return false;
        }
        const FProperty* Property = FindFProperty<FProperty>(BP.ParentClass.Get(), Widget->GetFName());
        if (Property && Property->HasMetaData(TEXT("BindWidget")) && !Property->HasMetaData(TEXT("BindWidgetOptional")))
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL required native binding %s"), Prefix, *Widget->GetName());
            return false;
        }
    }
    return true;
}

bool ApplyChoice(UWidgetBlueprint& BP)
{
    if (Applied(BP)) return true;
    if (AppliedPreviousVersion(BP)) return SaveVersionOnly(BP);
    BP.Modify();
    // This WBP is referenced only by the build panel. The new native parent still
    // inherits PrepChoiceButton, preserving every existing binding and button style.
    BP.ParentClass = UFF_PrepBuildCategoryChoice::StaticClass();
    return Save(BP);
}

bool ApplyPanel(UWidgetBlueprint& BP, UClass* ChoiceClass, const TSet<UWidget*>& Retired)
{
    if (Applied(BP)) return true;
    if (AppliedPreviousVersion(BP)) return SaveVersionOnly(BP);
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Categories = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("HorizontalBox_Categories")));
    auto* Minors = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("HorizontalBox_Subcategories")));
    auto* Tiles = Cast<UFF_PrepBuildTileView>(Tree.FindWidget(TEXT("TileView_Facilities")));
    if (!Categories || !Minors || !Tiles || !ChoiceClass ||
        !ChoiceClass->IsChildOf(UFF_PrepBuildCategoryChoice::StaticClass()))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL expected authored category hosts"), Prefix);
        return false;
    }
    // v2 also runs after v1. Keep existing saved tabs and any designer adjustments.
    for (UWidget* Child : Categories->GetAllChildren())
    {
        if (!Cast<UFF_PrepBuildCategoryChoice>(Child))
        { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL unexpected custom category child"), Prefix); return false; }
    }
    BP.Modify(); Tree.Modify();
    const auto AddChoice = [&](UHorizontalBox& Host, FName Name, int32 Id, const FText& Label,
        const FSlateBrush& Icon, bool Selected, FMargin Padding)
    {
        auto* Choice = Cast<UFF_PrepBuildCategoryChoice>(MakeWidgetOfClass(BP, Tree, Name, ChoiceClass));
        if (!Choice) return false;
        Choice->AuthoredChoiceId = Id;
        Choice->AuthoredLabel = Label;
        Choice->AuthoredIcon = Icon;
        Choice->bPreviewSelected = Selected;
        auto* Slot = Host.AddChildToHorizontalBox(Choice);
        Slot->SetPadding(Padding);
        Slot->SetVerticalAlignment(VAlign_Center);
        return true;
    };
    if (Categories->GetChildrenCount() == 0)
    {
    if (!AddChoice(*Categories, TEXT("Choice_CategoryAll"), INDEX_NONE,
        NSLOCTEXT("FestaFiestaPrepBuild", "FilterAll", "전체"),
        FFBuildCategoryIcons::Resolve(true, EFFZSFacilityCategory::Booth), true, FMargin(0, 0, 5, 0))) return false;
    for (const EFFZSFacilityCategory Category : FFF_PrepBuildPresentationRules::GetCategoryOrder())
    {
        const FName Name(*FString::Printf(TEXT("Choice_Category%d"), static_cast<int32>(Category)));
        if (!AddChoice(*Categories, Name, static_cast<int32>(Category),
            FFF_PrepBuildPresentationRules::ResolveCategoryDisplayName(Category),
            FFBuildCategoryIcons::Resolve(false, Category), false, FMargin(0, 0, 5, 0))) return false;
    }
    }
    if (Minors->GetChildrenCount() == 0)
    {
    if (!AddChoice(*Minors, TEXT("Choice_AllSubcategories"), 0,
        NSLOCTEXT("FestaFiestaPrepBuild", "AllFacilities", "전체 시설"), FSlateBrush(), true, FMargin(0, 0, 6, 0))) return false;
    }

    // Use the editor's deletion operation so GUIDs, optional variables and serialized
    // child objects disappear too. Merely RemoveFromParent leaves stale named objects.
    FWidgetBlueprintEditorUtils::DeleteWidgets(&BP, Retired,
        FWidgetBlueprintEditorUtils::EDeleteWidgetWarningType::DeleteSilently);
    if (auto* PreviewCount = FindFProperty<FIntProperty>(Tiles->GetClass(), TEXT("NumDesignerPreviewEntries")))
        PreviewCount->SetPropertyValue_InContainer(Tiles, 8);
    if (UWidget* Empty = Tree.FindWidget(TEXT("Panel_Empty"))) Empty->bHiddenInDesigner = true;
    Tiles->bHiddenInDesigner = false;
    if (!Save(BP)) return false;
    UObject* CDO = BP.GeneratedClass->GetDefaultObject();
    FindFProperty<FBoolProperty>(BP.GeneratedClass, TEXT("bUseDesignerCatalogLayout"))->SetPropertyValue_InContainer(CDO, true);
    FindFProperty<FBoolProperty>(BP.GeneratedClass, TEXT("bUsePagination"))->SetPropertyValue_InContainer(CDO, false);
    auto* PanelCDO = CastChecked<UFF_PrepPanelBuild>(CDO);
    PanelCDO->DesignSizeMode = EDesignPreviewSizeMode::Desired;
    PanelCDO->bPreviewEmptyCatalog = false;
    return SaveAssetPackage(Prefix, BP);
}

bool RepairThumbnailBrushSize(UWidgetBlueprint& BP, bool& bChanged)
{
    bChanged = false;
    auto* Thumbnail = Cast<UImage>(BP.WidgetTree->FindWidget(TEXT("Image_Thumbnail")));
    if (!Thumbnail) return false;
    FSlateBrush Brush = Thumbnail->GetBrush();
    // SetBrushFromTexture skips size matching when its resource is unchanged.
    // Designer has no runtime desired-size override, so a zero brush stays invisible.
    // Preserve every authored nonzero size and the currently selected resource.
    if (Brush.ImageSize.X != 0.f && Brush.ImageSize.Y != 0.f) return true;
    auto* Texture = Cast<UTexture2D>(Brush.GetResourceObject());
    if (!Texture) return true;
    FTextureCompilingManager::Get().FinishCompilation({Texture});
    int32 Width = Texture->GetSizeX();
    int32 Height = Texture->GetSizeY();
    // Headless -nullrhi migrations may have no platform texture dimensions.
    // The imported editor source still owns the original image dimensions.
    if ((Width <= 0 || Height <= 0) && Texture->Source.IsValid())
    {
        Width = static_cast<int32>(Texture->Source.GetSizeX());
        Height = static_cast<int32>(Texture->Source.GetSizeY());
    }
    if (Width <= 0 || Height <= 0)
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL thumbnail texture has no valid size: %s"),
            Prefix, *Texture->GetPathName());
        return false;
    }
    BP.Modify(); BP.WidgetTree->Modify(); Thumbnail->Modify();
    Brush.ImageSize = FVector2D(Width, Height);
    Thumbnail->SetBrush(Brush);
    bChanged = true;
    return true;
}

bool ApplyTile(UWidgetBlueprint& BP, const TSet<UWidget*>& Retired)
{
    if (Applied(BP)) return true;
    if (AppliedPreviousVersion(BP))
    {
        bool bChanged = false;
        if (!RepairThumbnailBrushSize(BP, bChanged)) return false;
        // v2 already owns the editable structure and preview defaults. Do not
        // delete nodes, replace textures, or reset the designer's chosen state.
        return bChanged ? Save(BP) : SaveVersionOnly(BP);
    }
    BP.Modify(); BP.WidgetTree->Modify();
    FWidgetBlueprintEditorUtils::DeleteWidgets(&BP, Retired,
        FWidgetBlueprintEditorUtils::EDeleteWidgetWarningType::DeleteSilently);
    auto& Tree = *BP.WidgetTree;
    for (const TCHAR* Name : { TEXT("Border_RequiredMissing"), TEXT("Border_RequiredSatisfied"),
        TEXT("Border_ProgramBadge"), TEXT("Border_SynergyHighlight"), TEXT("Text_SynergyBadge"),
        TEXT("Image_CodexFocusRim"), TEXT("Border_CodexFocusBadge"), TEXT("Text_State") })
        if (UWidget* Widget = Tree.FindWidget(Name)) Widget->bHiddenInDesigner = true;
    if (auto* Thumbnail = Cast<UImage>(Tree.FindWidget(TEXT("Image_Thumbnail"))))
    {
        if (!Thumbnail->GetBrush().GetResourceObject())
            if (auto* Texture = LoadObject<UTexture2D>(nullptr, FFBuildUIAssetPaths::FoodTruckImageObject))
                Thumbnail->SetBrushFromTexture(Texture, false);
        const bool bHasThumbnail = Thumbnail->GetBrush().GetResourceObject() != nullptr;
        Thumbnail->bHiddenInDesigner = !bHasThumbnail;
        if (UWidget* Placeholder = Tree.FindWidget(TEXT("Text_ThumbnailPlaceholder")))
            Placeholder->bHiddenInDesigner = bHasThumbnail;
    }
    bool bThumbnailSizeChanged = false;
    if (!RepairThumbnailBrushSize(BP, bThumbnailSizeChanged)) return false;
    // SizeBox_Card's height and the TileView pitch stay authored. Desired preview
    // displays the card at its real width instead of stretching it to a full screen.
    if (!Save(BP)) return false;
    auto* TileCDO = CastChecked<UFF_PrepBuildTileWidget>(BP.GeneratedClass->GetDefaultObject());
    TileCDO->DesignSizeMode = EDesignPreviewSizeMode::Desired;
    TileCDO->DesignerPreviewState = EFF_PrepBuildTilePreviewState::Available;
    return SaveAssetPackage(Prefix, BP);
}

void Apply()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry"))
    { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL requires idle Entry editor"), Prefix); return; }
    auto* Panel = LoadExistingBlueprint(FFBuildUIAssetPaths::BuildPanelPackage);
    auto* Tile = LoadExistingBlueprint(FFBuildUIAssetPaths::BuildTilePackage);
    auto* Choice = LoadExistingBlueprint(FFBuildUIAssetPaths::CategoryChoicePackage);
    if (!Panel || !Tile || !Choice || !Panel->WidgetTree || !Tile->WidgetTree || !Choice->WidgetTree ||
        Panel->ParentClass != UFF_PrepPanelBuild::StaticClass() ||
        Tile->ParentClass != UFF_PrepBuildTileWidget::StaticClass() ||
        !Choice->ParentClass->IsChildOf(UFF_PrepChoiceButton::StaticClass()))
    { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL active catalog asset contract"), Prefix); return; }
    if (Applied(*Panel) && Applied(*Tile) && Applied(*Choice))
    { UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_PASS already applied; Designer edits preserved"), Prefix); return; }
    auto PanelRetired = RetiredWidgets(*Panel, { TEXT("Box_Header"), TEXT("Border_AuroraSelection"),
        TEXT("Border_CatalogFooter"), TEXT("HorizontalBox_Tools"), TEXT("Size_CategoryDivider"),
        TEXT("Panel_PlacementCompact"), TEXT("Text_CategoryBreadcrumb") });
    auto TileRetired = RetiredWidgets(*Tile, { TEXT("Text_CategoryPath"), TEXT("Text_BuildLimit"),
        TEXT("Overlay_RequiredStatus") });
    if ((!AppliedPreviousVersion(*Panel) && !CanRemove(*Panel, PanelRetired)) ||
        (!AppliedPreviousVersion(*Tile) && !CanRemove(*Tile, TileRetired))) return;
    if (ApplyChoice(*Choice) && ApplyPanel(*Panel, Choice->GeneratedClass, PanelRetired) && ApplyTile(*Tile, TileRetired))
    {
        UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_PASS panel + tile + category choice; no legacy asset packages deleted"), Prefix);
    }
    else
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL migration incomplete"), Prefix);
    }
}

FAutoConsoleCommand Command(TEXT("FF.ApplyBuildCatalogDesigner"),
    TEXT("Delete retired active-catalog widgets and save editable category tabs. Run once in idle Entry after backing up the three catalog assets."),
    FConsoleCommandDelegate::CreateStatic(&Apply));
}
#endif
