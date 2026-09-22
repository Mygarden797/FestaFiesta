// FF_CodexCatalogFocusBuilder.cpp
// 역할: 도감에서 이동해 온 시설 카드를 강조하는 표시를 추가한다.

// Explicit editor migration for the codex destination accent; runtime only projects state.
#include "CoreMinimal.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"

#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FFCodexCatalogFocus"
namespace FFCodexCatalogFocus
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Prefix = TEXT("FF_CODEX_CATALOG_FOCUS");
constexpr const TCHAR* Version = TEXT("FF.FacilityCodex.CatalogFocus20260921.1");
constexpr const TCHAR* TilePath = FFBuildUIAssetPaths::BuildTilePackage;

FSlateBrush MaterialBrush(UMaterialInterface* Material)
{
    FSlateBrush Brush;
    Brush.DrawAs = ESlateBrushDrawType::Image;
    Brush.TintColor = FLinearColor::White;
    Brush.ImageSize = FVector2D::ZeroVector;
    Brush.SetResourceObject(Material);
    return Brush;
}

bool ApplyToTile(UWidgetBlueprint& BP)
{
    auto& Tree = *BP.WidgetTree;
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, Version) == TEXT("1"))
    {
        return Cast<UImage>(Tree.FindWidget(TEXT("Image_CodexFocusRim"))) &&
            Cast<UBorder>(Tree.FindWidget(TEXT("Border_CodexFocusBadge"))) &&
            Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_CodexFocusBadge")));
    }
    if (!Tree.RootWidget || !Tree.FindWidget(TEXT("SizeBox_Card")) || !Tree.FindWidget(TEXT("Border_Card"))) return false;
    for (const TCHAR* Name : {TEXT("Overlay_CodexCatalogFocus"), TEXT("Image_CodexFocusRim"),
        TEXT("Border_CodexFocusBadge"), TEXT("Text_CodexFocusBadge")})
    {
        if (Tree.FindWidget(Name)) return false;
    }
    const FString Folder = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Materials/");
    auto* RimMaterial = LoadObject<UMaterialInterface>(nullptr,
        *(Folder + TEXT("MI_FF_CodexCatalogFocus_Rim.MI_FF_CodexCatalogFocus_Rim")));
    auto* BadgeMaterial = LoadObject<UMaterialInterface>(nullptr,
        *(Folder + TEXT("MI_FF_CodexCatalogFocus_Badge.MI_FF_CodexCatalogFocus_Badge")));
    if (!RimMaterial || !BadgeMaterial) return false;
    BP.Modify();
    Tree.Modify();
    UWidget* OriginalRoot = Tree.RootWidget.Get();
    auto* Overlay = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_CodexCatalogFocus"));
    auto* Rim = MakeWidget<UImage>(BP, Tree, TEXT("Image_CodexFocusRim"), true);
    auto* Badge = MakeWidget<UBorder>(BP, Tree, TEXT("Border_CodexFocusBadge"), true);
    auto* Label = MakeWidget<UTextBlock>(BP, Tree, TEXT("Text_CodexFocusBadge"));
    Tree.RootWidget = Overlay;
    Overlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    auto* OriginalSlot = Overlay->AddChildToOverlay(OriginalRoot);
    OriginalSlot->SetHorizontalAlignment(HAlign_Fill);
    OriginalSlot->SetVerticalAlignment(VAlign_Fill);
    Rim->SetBrush(MaterialBrush(RimMaterial));
    Rim->SetVisibility(ESlateVisibility::Collapsed);
    auto* RimSlot = Overlay->AddChildToOverlay(Rim);
    RimSlot->SetHorizontalAlignment(HAlign_Fill);
    RimSlot->SetVerticalAlignment(VAlign_Fill);
    Badge->SetBrush(MaterialBrush(BadgeMaterial));
    Badge->SetBrushColor(FLinearColor::White);
    Badge->SetPadding(FMargin(7, 3));
    Badge->SetVisibility(ESlateVisibility::Collapsed);
    Badge->AddChild(Label);
    StyleText(*Label, 12, FLinearColor::FromSRGBColor(FColor(0xFF, 0xF3, 0xCE)), TEXT("SemiBold"));
    Label->SetText(LOCTEXT("Destination", "도감에서 찾은 시설"));
    Label->SetAutoWrapText(false);
    Label->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* BadgeSlot = Overlay->AddChildToOverlay(Badge);
    BadgeSlot->SetHorizontalAlignment(HAlign_Left);
    BadgeSlot->SetVerticalAlignment(VAlign_Top);
    BadgeSlot->SetPadding(FMargin(8, 8, 0, 0));
    Metadata.SetValue(&BP, Version, TEXT("1"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, Version);
    return false;
}

void Apply()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!GEditor || GEditor->PlayWorld || !World || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry"))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: use the idle Entry map"), Prefix);
        return;
    }
    auto* BP = LoadExistingBlueprint(TilePath);
    if (!BP || !BP->WidgetTree || BP->ParentClass != UFF_PrepBuildTileWidget::StaticClass() || !ApplyToTile(*BP))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: missing expected tile or focus materials"), Prefix);
        return;
    }
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_PASS"), Prefix);
}
FAutoConsoleCommand Command(TEXT("FF.ApplyCodexCatalogFocus"),
    TEXT("Adds the authored iridescent codex destination accent to the existing catalog tile."),
    FConsoleCommandDelegate::CreateStatic(&Apply));
}
#undef LOCTEXT_NAMESPACE
#endif
