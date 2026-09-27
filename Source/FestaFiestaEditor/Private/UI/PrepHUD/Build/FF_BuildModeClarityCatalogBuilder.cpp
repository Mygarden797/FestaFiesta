// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// FF_BuildModeClarityCatalogBuilder.cpp
// 역할: 건설 카탈로그의 기존 조작 연결을 유지하며 가독성 디자인을 적용한다.

// Explicit, one-time migration of the existing catalog; graphs and named controls survive.
#include "CoreMinimal.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#if WITH_EDITOR
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"
#include "UI/PrepHUD/Panels/FF_PrepChoiceButton.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "FFBuildModeClarityCatalog"
namespace FFBuildModeClarityCatalog
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Prefix = TEXT("FF_BUILD_CLARITY_CATALOG");
constexpr const TCHAR* VersionKey = TEXT("FF.BuildModeClarity.CatalogV1");
const FString PanelPackage = FFBuildUIAssetPaths::BuildPanelPackage;
const FString TilePackage = FFBuildUIAssetPaths::BuildTilePackage;
const FString ChoicePackage = FFBuildUIAssetPaths::CategoryChoicePackage;
const FLinearColor Ink = FLinearColor::FromSRGBColor(FColor(0x27, 0x4C, 0x3B));
const FLinearColor Muted = FLinearColor::FromSRGBColor(FColor(0x66, 0x7B, 0x6B));
const FLinearColor Paper = FLinearColor::FromSRGBColor(FColor(0xF0, 0xF5, 0xEC));
const FLinearColor Sage = FLinearColor::FromSRGBColor(FColor(0xE1, 0xEB, 0xDF));
const FLinearColor Green = FLinearColor::FromSRGBColor(FColor(0x30, 0x5C, 0x43));

FSlateBrush Brush(const FLinearColor Color, const TCHAR* MaterialRole = nullptr)
{
    FSlateBrush Result;
    Result.DrawAs = ESlateBrushDrawType::RoundedBox;
    Result.TintColor = Color;
    Result.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
    Result.OutlineSettings.CornerRadii = FVector4(8.f);
    Result.OutlineSettings.Color = FLinearColor::FromSRGBColor(FColor(0xC4, 0xD5, 0xC5));
    Result.OutlineSettings.Width = 1.f;
    if (MaterialRole)
    {
        const FString Name = FString::Printf(TEXT("MI_UI_BuildClarity_%s"), MaterialRole);
        const FString Path = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Controls/Clarity/") + Name + TEXT(".") + Name;
        if (auto* Material = LoadObject<UMaterialInterface>(nullptr, *Path))
        {
            Result.DrawAs = ESlateBrushDrawType::Image;
            Result.TintColor = FLinearColor::White;
            Result.SetResourceObject(Material);
        }
    }
    return Result;
}

void Label(UWidgetTree& Tree, const TCHAR* Name, int32 Size, const FLinearColor Color = Ink)
{
    if (auto* Text = Cast<UTextBlock>(Tree.FindWidget(Name)))
    {
        StyleText(*Text, Size, Color, TEXT("Medium"));
        Text->SetAutoWrapText(false);
        Text->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    }
}

void Height(UWidgetTree& Tree, const TCHAR* Name, float Value)
{
    if (auto* Size = Cast<USizeBox>(Tree.FindWidget(Name))) Size->SetHeightOverride(Value);
}

void Padding(UWidgetTree& Tree, const TCHAR* Name, const FMargin Value)
{
    UWidget* Widget = Tree.FindWidget(Name);
    if (auto* Slot = Widget ? Cast<UVerticalBoxSlot>(Widget->Slot) : nullptr) Slot->SetPadding(Value);
}

void Surface(UWidgetTree& Tree, const TCHAR* Name, const FLinearColor Color, const TCHAR* Role = nullptr)
{
    if (auto* Border = Cast<UBorder>(Tree.FindWidget(Name)))
    {
        Border->SetBrush(Brush(Color, Role));
        Border->SetBrushColor(FLinearColor::White);
    }
}

bool Completed(UWidgetBlueprint& BP)
{
    return BP.GetOutermost()->GetMetaData().GetValue(&BP, VersionKey) == TEXT("1");
}

bool Save(UWidgetBlueprint& BP)
{
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    Metadata.SetValue(&BP, VersionKey, TEXT("1"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, VersionKey);
    return false;
}

bool MigrateTile(UWidgetBlueprint& BP)
{
    if (Completed(BP)) return true;
    UWidgetTree& Tree = *BP.WidgetTree;
    if (!Tree.FindWidget(TEXT("SizeBox_Card")) || !Tree.FindWidget(TEXT("Border_AuroraCardGold"))) return false;
    BP.Modify();
    Tree.Modify();
    Height(Tree, TEXT("SizeBox_Card"), 174);
    // Three independent optional badges need separate vertical lanes inside the thumbnail.
    Height(Tree, TEXT("SizeBox_Thumbnail"), 92);
    Surface(Tree, TEXT("Border_Card"), Paper);
    Surface(Tree, TEXT("Border_ThumbnailBackdrop"), Sage);
    // Use iridescence once on the containing dock. Repeating it under every price was noisy.
    Surface(Tree, TEXT("Border_AuroraCardGold"), FLinearColor::Transparent);
    if (auto* Price = Cast<UBorder>(Tree.FindWidget(TEXT("Border_AuroraCardGold"))))
    {
        FSlateBrush Empty;
        Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
        Price->SetBrush(Empty);
        Price->SetPadding(FMargin(0, 2));
    }
    if (auto* Card = Cast<UBorder>(Tree.FindWidget(TEXT("Border_Card")))) Card->SetPadding(FMargin(10, 8));
    Label(Tree, TEXT("Text_Name"), 18);
    Label(Tree, TEXT("Text_Cost"), 18);
    Label(Tree, TEXT("Text_State"), 12, Muted);
    Label(Tree, TEXT("Text_ThumbnailPlaceholder"), 12, Muted);
    Label(Tree, TEXT("Text_Favorite"), 19, Muted);
    Padding(Tree, TEXT("Text_Name"), FMargin(0, 6, 0, 2));
    Padding(Tree, TEXT("HorizontalBox_Meta"), FMargin(0));
    // Existing program, requirement and synergy badges keep their independent lanes.
    for (const TCHAR* Name : { TEXT("Text_ProgramBadge"), TEXT("Text_RequiredMissing"),
        TEXT("Text_RequiredSatisfied"), TEXT("Text_SynergyBadge") })
    {
        if (auto* Text = Cast<UTextBlock>(Tree.FindWidget(Name)))
        {
            auto Font = Text->GetFont();
            Font.Size = 12;
            Text->SetFont(Font);
        }
    }
    return Save(BP);
}

bool MigratePanel(UWidgetBlueprint& BP)
{
    if (Completed(BP)) return true;
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Identity = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Column_AuroraSelection")));
    auto* Hint = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_SelectionHint")));
    auto* Tiles = Cast<UFF_PrepBuildTileView>(Tree.FindWidget(TEXT("TileView_Facilities")));
    if (!Identity || !Hint || !Tiles || !Tree.RootWidget || Tree.FindWidget(TEXT("Scale_ClarityCatalogDock"))) return false;
    BP.Modify();
    Tree.Modify();
    UWidget* PreviousRoot = Tree.RootWidget;
    auto* Dock = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_ClarityCatalogDock"), true);
    Dock->SetWidthOverride(1560);
    Dock->SetHeightOverride(450);
    Dock->AddChild(PreviousRoot);
    auto* Scale = MakeWidget<UScaleBox>(BP, Tree, TEXT("Scale_ClarityCatalogDock"));
    Scale->SetStretch(EStretch::ScaleToFit);
    Scale->SetStretchDirection(EStretchDirection::DownOnly);
    Scale->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    auto* ScaleSlot = CastChecked<UScaleBoxSlot>(Scale->AddChild(Dock));
    ScaleSlot->SetHorizontalAlignment(HAlign_Center);
    ScaleSlot->SetVerticalAlignment(VAlign_Bottom);
    Tree.RootWidget = Scale;
    Surface(Tree, TEXT("Border_PanelRoot"), Paper, TEXT("Paper"));
    Surface(Tree, TEXT("Border_Gold"), Sage);
    Surface(Tree, TEXT("Border_AuroraSelection"), Sage, TEXT("Header"));
    Surface(Tree, TEXT("Border_CatalogFooter"), Paper);
    Surface(Tree, TEXT("Border_CatalogRequiredGuide"), Sage);
    Surface(Tree, TEXT("Border_CatalogProgramLegend"), Sage);
    Height(Tree, TEXT("Size_CatalogHeader"), 62);
    Height(Tree, TEXT("Size_CategoryRow"), 34);
    Height(Tree, TEXT("Size_SubcategoryHeader"), 32);
    Tiles->SetEntryHeight(174);
    Tiles->SetHorizontalEntrySpacing(10);
    Tiles->ConfigureCatalogWheelRouting();
    if (auto* Frame = Cast<UBorder>(Tree.FindWidget(TEXT("Border_ApprovedCatalogV9")))) Frame->SetPadding(FMargin(0));
    if (auto* Divider = Tree.FindWidget(TEXT("Size_CategoryDivider"))) Divider->SetVisibility(ESlateVisibility::Collapsed);
    Padding(Tree, TEXT("Size_CategoryRow"), FMargin(0, 6, 0, 4));
    Padding(Tree, TEXT("Size_SubcategoryHeader"), FMargin(0, 0, 0, 6));
    Padding(Tree, TEXT("VerticalBox_CatalogBody"), FMargin(16, 0, 16, 2));
    Padding(Tree, TEXT("Border_AuroraSelection"), FMargin(16, 4, 16, 4));
    if (auto* Selection = Cast<UBorder>(Tree.FindWidget(TEXT("Border_AuroraSelection")))) Selection->SetPadding(FMargin(14, 6));
    if (auto* Footer = Cast<UBorder>(Tree.FindWidget(TEXT("Border_CatalogFooter")))) Footer->SetPadding(FMargin(16, 5));
    if (auto* SelectedLabel = Tree.FindWidget(TEXT("Label_SelectedFacility"))) SelectedLabel->SetVisibility(ESlateVisibility::Collapsed);
    Hint->RemoveFromParent();
    Identity->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0, 3, 0, 0));
    if (auto* Action = Tree.FindWidget(TEXT("Column_PlacementAction")))
    {
        if (auto* Slot = Cast<UHorizontalBoxSlot>(Action->Slot)) Slot->SetVerticalAlignment(VAlign_Center);
    }
    Label(Tree, TEXT("Text_PanelTitle"), 22);
    Label(Tree, TEXT("Text_Gold"), 18);
    Label(Tree, TEXT("Text_SelectedFacility"), 21);
    Label(Tree, TEXT("Text_SelectedCost"), 20);
    Label(Tree, TEXT("Text_AfterPlacementGold"), 20);
    Label(Tree, TEXT("Text_SelectionHint"), 13, Muted);
    Label(Tree, TEXT("Label_SelectedCost"), 12, Muted);
    Label(Tree, TEXT("Label_AfterPlacementGold"), 12, Muted);
    Label(Tree, TEXT("Text_Status"), 13, Muted);
    Label(Tree, TEXT("Text_StudioHint"), 12, Muted);
    Label(Tree, TEXT("Text_CatalogProgramLegend"), 12, Muted);
    Label(Tree, TEXT("Text_CatalogProgramGuide"), 12, Muted);
    Label(Tree, TEXT("Text_PlaceSelected"), 17, FLinearColor::White);
    for (const TCHAR* Name : { TEXT("Text_ItemCount"), TEXT("Text_PageIndex"), TEXT("Text_FavoritesOnly"),
        TEXT("Text_FavoriteCount"), TEXT("Text_InspectFacilities"), TEXT("Text_RevenueRanking") }) Label(Tree, Name, 13);
    if (auto* Text = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_PlaceSelected")))) Text->SetText(LOCTEXT("Place", "배치 시작  →"));
    if (auto* Text = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Status")))) Text->SetText(LOCTEXT("Browse", "좌우 드래그 / 휠로 넘기기  ·  ☆ 즐겨찾기"));
    if (auto* Text = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_SelectionHint")))) Text->SetText(LOCTEXT("Select", "카드 선택 후 배치 시작을 누르세요"));
    if (auto* Search = Cast<UEditableTextBox>(Tree.FindWidget(TEXT("Search_Facilities"))))
    {
        Search->SetMinDesiredWidth(190);
        auto Style = Search->GetWidgetStyle();
        Style.TextStyle.Font.Size = 14;
        Style.Padding = FMargin(10, 6);
        Style.BackgroundImageNormal = Brush(Paper);
        Style.BackgroundImageHovered = Brush(Sage);
        Style.BackgroundImageFocused = Brush(Sage);
        Search->SetWidgetStyle(Style);
    }
    for (const TCHAR* Name : { TEXT("Button_PlaceSelected"), TEXT("Button_PreviousPage"), TEXT("Button_NextPage"),
        TEXT("Button_InspectFacilities"), TEXT("Button_RevenueRanking"), TEXT("Button_Close") })
    {
        if (auto* Button = Cast<UButton>(Tree.FindWidget(Name)))
        {
            const bool bPrimary = FString(Name) == TEXT("Button_PlaceSelected");
            auto Style = Button->GetStyle();
            Style.Normal = Brush(bPrimary ? Green : Paper, bPrimary ? TEXT("Action") : nullptr);
            Style.Hovered = Brush(bPrimary ? Green : Sage);
            Style.Pressed = Brush(bPrimary ? Green : Sage);
            Style.Disabled = Brush(Sage);
            Style.NormalPadding = Style.PressedPadding = FMargin(bPrimary ? 24.f : 9.f, bPrimary ? 9.f : 5.f);
            Button->SetStyle(Style);
        }
    }
    if (!Save(BP)) return false;
    // Compile first, then save CDO values so Blueprint reinstancing cannot restore the old 600px height.
    for (const TPair<FName, float>& Value : { TPair<FName, float>(TEXT("CatalogHeight"), 450.f),
        TPair<FName, float>(TEXT("CompactHeight"), 42.f) })
    {
        auto* Property = FindFProperty<FFloatProperty>(BP.GeneratedClass.Get(), Value.Key);
        if (!Property) return false;
        Property->SetPropertyValue_InContainer(BP.GeneratedClass->GetDefaultObject(), Value.Value);
    }
    return SaveAssetPackage(Prefix, BP);
}

bool RefineRowClearance(UWidgetBlueprint& BP)
{
    constexpr const TCHAR* Key = TEXT("FF.BuildModeClarity.CatalogClearanceV2");
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, Key) == TEXT("1")) return true;
    auto& Tree = *BP.WidgetTree;
    auto* Header = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_CatalogHeader")));
    auto* Selection = Cast<UBorder>(Tree.FindWidget(TEXT("Border_AuroraSelection")));
    UWidget* Body = Tree.FindWidget(TEXT("VerticalBox_CatalogBody"));
    if (!Header || !Selection || !Body || !Cast<UVerticalBoxSlot>(Body->Slot) ||
        !Cast<UVerticalBoxSlot>(Selection->Slot)) return false;
    // Actual 720p font metrics left 8.6 logical units of the 174-unit card clipped.
    // Reclaim 14 units from decoration, preserving the dock's 450-unit world occlusion.
    BP.Modify();
    Tree.Modify();
    Header->SetHeightOverride(62.f);
    Selection->SetPadding(FMargin(14, 6));
    Padding(Tree, TEXT("VerticalBox_CatalogBody"), FMargin(16, 0, 16, 2));
    Padding(Tree, TEXT("Border_AuroraSelection"), FMargin(16, 4, 16, 4));
    Metadata.SetValue(&BP, Key, TEXT("1"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, Key);
    return false;
}

bool RefineHeaderDecoration(UWidgetBlueprint& BP)
{
    constexpr const TCHAR* Key = TEXT("FF.BuildModeClarity.HeaderDecorationV3");
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, Key) == TEXT("1")) return true;
    auto* Mark = Cast<UBorder>(BP.WidgetTree->FindWidget(TEXT("Border_BuildMark")));
    if (!Mark) return false;
    // The legacy four-block mark renders as an empty pale square with the new surface.
    // It carries no information: collapse its complete slot, preserving every named child.
    BP.Modify();
    BP.WidgetTree->Modify();
    Mark->SetVisibility(ESlateVisibility::Collapsed);
    Metadata.SetValue(&BP, Key, TEXT("1"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, Key);
    return false;
}

bool RefineDirectPlacementTile(UWidgetBlueprint& BP)
{
    constexpr const TCHAR* Key = TEXT("FF.BuildModeClarity.DirectPlacementV4");
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, Key) == TEXT("1")) return true;
    auto& Tree = *BP.WidgetTree;
    auto* Backdrop = Cast<UBorder>(Tree.FindWidget(TEXT("Border_ThumbnailBackdrop")));
    auto* Category = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_CategoryPath")));
    auto* Program = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_ProgramBadge")));
    const FSlateBrush Pearl = Brush(FLinearColor::White, TEXT("Program"));
    if (!Backdrop || !Category || !Program || !Pearl.GetResourceObject())
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: V4 requires the program material and existing card seams"), Prefix);
        return false;
    }
    BP.Modify();
    Tree.Modify();
    Backdrop->bIsVariable = true;
    Height(Tree, TEXT("SizeBox_Card"), 204.f);
    Height(Tree, TEXT("SizeBox_Thumbnail"), 104.f);
    if (auto* Card = Cast<UBorder>(Tree.FindWidget(TEXT("Border_Card")))) Card->SetPadding(FMargin(10, 8));
    Category->SetVisibility(ESlateVisibility::HitTestInvisible);
    Label(Tree, TEXT("Text_CategoryPath"), 11, Muted);
    Label(Tree, TEXT("Text_Name"), 17);
    if (auto* Name = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Name"))))
        StyleText(*Name, 17, Ink, TEXT("SemiBold"));
    Label(Tree, TEXT("Text_Cost"), 17);
    Padding(Tree, TEXT("Text_CategoryPath"), FMargin(0, 5, 0, 1));
    Padding(Tree, TEXT("Text_Name"), FMargin(0, 0, 0, 4));
    Surface(Tree, TEXT("Border_ProgramBadge"), FLinearColor::FromSRGBColor(FColor(0x64, 0x58, 0x7E)));
    Program->SetText(LOCTEXT("ProgramV4", "프로그램"));
    Label(Tree, TEXT("Text_ProgramBadge"), 12, FLinearColor::White);
    if (!SaveNewBlueprint(Prefix, BP)) return false;
    auto* Property = FindFProperty<FStructProperty>(BP.GeneratedClass.Get(), TEXT("ProgramThumbnailBrush"));
    if (!Property || Property->Struct != FSlateBrush::StaticStruct()) return false;
    *Property->ContainerPtrToValuePtr<FSlateBrush>(BP.GeneratedClass->GetDefaultObject()) = Pearl;
    Metadata.SetValue(&BP, Key, TEXT("1"));
    if (SaveAssetPackage(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, Key);
    return false;
}

bool RefineDirectPlacementPanel(UWidgetBlueprint& BP)
{
    constexpr const TCHAR* Key = TEXT("FF.BuildModeClarity.DirectPlacementV4");
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, Key) == TEXT("1")) return true;
    auto& Tree = *BP.WidgetTree;
    auto* Ranking = Cast<UButton>(Tree.FindWidget(TEXT("Button_RevenueRanking")));
    auto* Header = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("HorizontalBox_CatalogHeader")));
    auto* Launcher = Tree.FindWidget(TEXT("Widget_FacilityCodexCatalogLauncher"));
    auto* Tiles = Cast<UFF_PrepBuildTileView>(Tree.FindWidget(TEXT("TileView_Facilities")));
    auto* Selection = Tree.FindWidget(TEXT("Border_AuroraSelection"));
    if (!Ranking || !Header || !Launcher || !Tiles || !Selection) return false;
    BP.Modify();
    Tree.Modify();
    Selection->SetVisibility(ESlateVisibility::Collapsed);
    if (auto* Action = Tree.FindWidget(TEXT("Button_PlaceSelected"))) Action->SetVisibility(ESlateVisibility::Collapsed);
    Height(Tree, TEXT("Size_ClarityCatalogDock"), 392.f);
    Tiles->SetEntryHeight(204.f);
    Padding(Tree, TEXT("VerticalBox_CatalogBody"), FMargin(16, 0, 16, 4));
    auto Style = Ranking->GetStyle();
    Style.Normal = Brush(Paper);
    Style.Hovered = Brush(Sage);
    Style.Pressed = Brush(Sage);
    Style.NormalPadding = Style.PressedPadding = FMargin(10, 5);
    Ranking->SetStyle(Style);
    if (auto* LabelWidget = Ranking->GetContent())
    {
        if (auto* Slot = Cast<UButtonSlot>(LabelWidget->Slot))
        {
            Slot->SetPadding(FMargin(0));
            Slot->SetHorizontalAlignment(HAlign_Center);
            Slot->SetVerticalAlignment(VAlign_Center);
        }
    }
    Label(Tree, TEXT("Text_RevenueRanking"), 14);
    if (!Tree.FindWidget(TEXT("Size_ClarityRankingAction")))
    {
        const int32 Index = Header->GetChildIndex(Ranking);
        Ranking->RemoveFromParent();
        auto* Bounds = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_ClarityRankingAction"));
        Bounds->SetWidthOverride(112.f);
        Bounds->SetHeightOverride(32.f);
        Bounds->AddChild(Ranking);
        auto* Slot = Header->AddChildToHorizontalBox(Bounds);
        Slot->SetVerticalAlignment(VAlign_Center);
        Slot->SetPadding(FMargin(0, 0, 8, 0));
        Header->ShiftChild(Index, Bounds);
    }
    if (auto* Slot = Cast<UHorizontalBoxSlot>(Launcher->Slot))
    {
        Slot->SetVerticalAlignment(VAlign_Center);
        Slot->SetPadding(FMargin(0, 0, 8, 0));
    }
    Surface(Tree, TEXT("Border_CatalogProgramLegend"), FLinearColor::FromSRGBColor(FColor(0x64, 0x58, 0x7E)));
    Label(Tree, TEXT("Text_CatalogProgramLegend"), 12, FLinearColor::White);
    if (auto* Text = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Status"))))
        Text->SetText(LOCTEXT("DirectBrowseV4", "카드 선택 → 위치 지정  ·  확정 시 결제  ·  드래그 / 휠로 넘기기  ·  ☆ 즐겨찾기"));
    if (!SaveNewBlueprint(Prefix, BP)) return false;
    auto* HeightProperty = FindFProperty<FFloatProperty>(BP.GeneratedClass.Get(), TEXT("CatalogHeight"));
    if (!HeightProperty) return false;
    HeightProperty->SetPropertyValue_InContainer(BP.GeneratedClass->GetDefaultObject(), 392.f);
    Metadata.SetValue(&BP, Key, TEXT("1"));
    if (SaveAssetPackage(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, Key);
    return false;
}

bool RefineCatalogClearanceV5(UWidgetBlueprint& BP)
{
    constexpr const TCHAR* Key = TEXT("FF.BuildModeClarity.CatalogClearanceV5");
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, Key) == TEXT("1")) return true;
    auto& Tree = *BP.WidgetTree;
    auto* RankingBounds = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_ClarityRankingAction")));
    auto* RankingLabel = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_RevenueRanking")));
    auto* Footer = Cast<UBorder>(Tree.FindWidget(TEXT("Border_CatalogFooter")));
    if (!RankingBounds || !RankingLabel || !Footer) return false;
    BP.Modify();
    Tree.Modify();
    // The 720p font metrics consumed six logical units of the full card row.
    // Recover ten from empty gutters while keeping the dock's world occlusion unchanged.
    Padding(Tree, TEXT("Size_CategoryRow"), FMargin(0, 4, 0, 2));
    Padding(Tree, TEXT("Size_SubcategoryHeader"), FMargin(0, 0, 0, 4));
    Padding(Tree, TEXT("VerticalBox_CatalogBody"), FMargin(16, 0, 16, 2));
    Footer->SetPadding(FMargin(16, 4));
    // Noto's full line metrics need more room than the visible glyph silhouette.
    // The nested launcher copies these exact dimensions/typography at construction.
    RankingBounds->SetHeightOverride(40.f);
    RankingLabel->SetMinDesiredWidth(92.f);
    RankingLabel->SetJustification(ETextJustify::Center);
    Metadata.SetValue(&BP, Key, TEXT("1"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, Key);
    return false;
}

bool RefineHeaderGlyphClearanceV6(UWidgetBlueprint& BP)
{
    constexpr const TCHAR* Key = TEXT("FF.BuildModeClarity.HeaderGlyphClearanceV6");
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, Key) == TEXT("1")) return true;
    auto* Ranking = Cast<UButton>(BP.WidgetTree->FindWidget(TEXT("Button_RevenueRanking")));
    auto* Label = Ranking ? Ranking->GetContent() : nullptr;
    auto* ContentSlot = Label ? Cast<UButtonSlot>(Label->Slot) : nullptr;
    if (!Ranking || !ContentSlot) return false;
    BP.Modify();
    BP.WidgetTree->Modify();
    // Center alignment shrank the Noto label to its 21.02-unit desired height,
    // while full glyph measurement is 23. A 24-unit inner box preserves full
    // glyphs and keeps the text centered within the existing 40-unit button.
    auto Style = Ranking->GetStyle();
    Style.NormalPadding = Style.PressedPadding = FMargin(10, 8);
    Ranking->SetStyle(Style);
    ContentSlot->SetPadding(FMargin(0));
    ContentSlot->SetVerticalAlignment(VAlign_Fill);
    Metadata.SetValue(&BP, Key, TEXT("1"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, Key);
    return false;
}

bool RefineRequiredTileV7(UWidgetBlueprint& BP)
{
    constexpr const TCHAR* Key = TEXT("FF.BuildModeClarity.RequiredAccentV7");
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, Key) == TEXT("1")) return true;
    auto& Tree = *BP.WidgetTree;
    auto* Thumbnail = Cast<UOverlay>(Tree.FindWidget(TEXT("Overlay_Thumbnail")));
    auto* Missing = Cast<UBorder>(Tree.FindWidget(TEXT("Border_RequiredMissing")));
    auto* Satisfied = Cast<UBorder>(Tree.FindWidget(TEXT("Border_RequiredSatisfied")));
    auto* Program = Cast<UBorder>(Tree.FindWidget(TEXT("Border_ProgramBadge")));
    auto* MissingLabel = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_RequiredMissing")));
    auto* SatisfiedLabel = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_RequiredSatisfied")));
    const FSlateBrush Rose = Brush(FLinearColor::White, TEXT("Required"));
    if (!Thumbnail || !Missing || !Satisfied || !Program || !MissingLabel || !SatisfiedLabel || !Rose.GetResourceObject())
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: V7 requires the required material and existing badge seams"), Prefix);
        return false;
    }
    BP.Modify();
    Tree.Modify();
    auto* Badges = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("HorizontalBox_ClarityFacilityBadges")));
    if (!Badges) Badges = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_ClarityFacilityBadges"));
    Badges->SetVisibility(ESlateVisibility::HitTestInvisible);
    if (Badges->GetParent() != Thumbnail)
    {
        Badges->RemoveFromParent();
        Thumbnail->AddChildToOverlay(Badges);
    }
    auto* BadgeSlot = CastChecked<UOverlaySlot>(Badges->Slot);
    BadgeSlot->SetHorizontalAlignment(HAlign_Left);
    BadgeSlot->SetVerticalAlignment(VAlign_Bottom);
    BadgeSlot->SetPadding(FMargin(4, 0, 4, 3));
    // A single bottom row leaves the thumbnail center clear. Collapsed states use no width.
    for (auto* Badge : { Missing, Satisfied, Program })
    {
        Badge->RemoveFromParent();
        Badge->SetPadding(FMargin(6, 2));
        auto* RowSlot = Badges->AddChildToHorizontalBox(Badge);
        RowSlot->SetVerticalAlignment(VAlign_Center);
        RowSlot->SetPadding(FMargin(0, 0, Badge == Program ? 0.f : 4.f, 0));
    }
    const FLinearColor RoseInk = FLinearColor::FromSRGBColor(FColor(0x8C, 0x42, 0x4D));
    auto MissingBrush = Brush(FLinearColor::FromSRGBColor(FColor(0x9D, 0x51, 0x58)));
    MissingBrush.OutlineSettings.Color = RoseInk;
    Missing->SetBrush(MissingBrush);
    Missing->SetBrushColor(FLinearColor::White);
    auto SatisfiedBrush = Brush(FLinearColor::FromSRGBColor(FColor(0xF2, 0xD8, 0xD8)));
    SatisfiedBrush.OutlineSettings.Color = FLinearColor::FromSRGBColor(FColor(0xD0, 0xA0, 0xA3));
    Satisfied->SetBrush(SatisfiedBrush);
    Satisfied->SetBrushColor(FLinearColor::White);
    MissingLabel->SetText(LOCTEXT("RequiredMissingV7", "필수 설치"));
    SatisfiedLabel->SetText(LOCTEXT("RequiredSatisfiedV7", "필수 완료"));
    Label(Tree, TEXT("Text_RequiredMissing"), 12, FLinearColor::White);
    Label(Tree, TEXT("Text_RequiredSatisfied"), 12, RoseInk);
    if (!SaveNewBlueprint(Prefix, BP)) return false;
    auto* Property = FindFProperty<FStructProperty>(BP.GeneratedClass.Get(), TEXT("RequiredThumbnailBrush"));
    if (!Property || Property->Struct != FSlateBrush::StaticStruct()) return false;
    *Property->ContainerPtrToValuePtr<FSlateBrush>(BP.GeneratedClass->GetDefaultObject()) = Rose;
    Metadata.SetValue(&BP, Key, TEXT("1"));
    if (SaveAssetPackage(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, Key);
    return false;
}

bool RefineRequiredGuideV7(UWidgetBlueprint& BP)
{
    constexpr const TCHAR* Key = TEXT("FF.BuildModeClarity.RequiredAccentV7");
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, Key) == TEXT("2")) return true;
    auto& Tree = *BP.WidgetTree;
    auto* Guide = Cast<UBorder>(Tree.FindWidget(TEXT("Border_CatalogRequiredGuide")));
    auto* Hint = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_StudioHint")));
    if (!Guide || !Hint) return false;
    BP.Modify();
    Tree.Modify();
    // Preserve the existing guide binding: the complete badge + explanation collapses when fulfilled.
    Guide->bIsVariable = true;
    FSlateBrush Empty;
    Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
    Guide->SetBrush(Empty);
    Guide->SetPadding(FMargin(0));
    auto* Row = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("HorizontalBox_ClarityRequiredGuide")));
    if (!Row) Row = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_ClarityRequiredGuide"));
    auto* Badge = Cast<UBorder>(Tree.FindWidget(TEXT("Border_ClarityRequiredLegend")));
    if (!Badge) Badge = MakeWidget<UBorder>(BP, Tree, TEXT("Border_ClarityRequiredLegend"));
    auto* Caption = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_ClarityRequiredLegend")));
    if (!Caption) Caption = MakeWidget<UTextBlock>(BP, Tree, TEXT("Text_ClarityRequiredLegend"));
    Hint->RemoveFromParent();
    Guide->SetContent(Row);
    Row->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto BadgeBrush = Brush(FLinearColor::FromSRGBColor(FColor(0x9D, 0x51, 0x58)));
    BadgeBrush.OutlineSettings.Color = FLinearColor::FromSRGBColor(FColor(0x8C, 0x42, 0x4D));
    Badge->SetBrush(BadgeBrush);
    Badge->SetPadding(FMargin(6, 2));
    Badge->SetContent(Caption);
    Caption->SetText(LOCTEXT("RequiredLegendV7", "필수"));
    // The new badge is not in the root tree until the row slot below is attached.
    StyleText(*Caption, 12, FLinearColor::White, TEXT("Medium"));
    Caption->SetAutoWrapText(false);
    Caption->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    auto* LegendSlot = Row->AddChildToHorizontalBox(Badge);
    LegendSlot->SetVerticalAlignment(VAlign_Center);
    LegendSlot->SetPadding(FMargin(0, 0, 6, 0));
    Hint->SetText(LOCTEXT("RequiredGuideV7", "1일차 시작 전 설치"));
    Label(Tree, TEXT("Text_StudioHint"), 12, Muted);
    Row->AddChildToHorizontalBox(Hint)->SetVerticalAlignment(VAlign_Center);
    Metadata.SetValue(&BP, Key, TEXT("2"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, Key);
    return false;
}

bool RefineCategoryPictogramsV8(UWidgetBlueprint& BP)
{
    constexpr const TCHAR* Key = TEXT("FF.BuildModeClarity.CategoryPictogramsV8");
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, Key) == TEXT("1")) return true;
    auto& Tree = *BP.WidgetTree;
    auto* Icon = Cast<UImage>(Tree.FindWidget(TEXT("Image_Icon")));
    auto* Button = Cast<UButton>(Tree.FindWidget(TEXT("Button_Root")));
    auto* Bounds = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_Choice")));
    if (!Icon || !Button || !Bounds) return false;
    for (const TCHAR* Name : { TEXT("All"), TEXT("Booth"), TEXT("Toilet"), TEXT("Info"), TEXT("Rest"),
        TEXT("Performance"), TEXT("Safety"), TEXT("PhotoSpot"), TEXT("Lighting"), TEXT("Road"), TEXT("Tile") })
    {
        const FString Asset = FString::Printf(TEXT("MI_UI_CategoryClarity_%s"), Name);
        const FString Path = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Controls/Clarity/") + Asset + TEXT(".") + Asset;
        if (!LoadObject<UMaterialInterface>(nullptr, *Path))
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: run create_category_icons.py before V8"), Prefix);
            return false;
        }
    }
    BP.Modify();
    Tree.Modify();
    Icon->SetDesiredSizeOverride(FVector2D(22, 22));
    if (auto* IconSlot = Cast<UHorizontalBoxSlot>(Icon->Slot))
    {
        IconSlot->SetVerticalAlignment(VAlign_Center);
        IconSlot->SetPadding(FMargin(0, 0, 6, 0));
    }
    // Keep the existing single-row navigation and its height: icons add meaning, not another layer.
    Bounds->SetHeightOverride(32.f);
    if (auto* Focusable = FindFProperty<FBoolProperty>(UButton::StaticClass(), TEXT("IsFocusable")))
        Focusable->SetPropertyValue_InContainer(Button, true);
    auto Style = Button->GetStyle();
    Style.Normal = Brush(Paper);
    Style.Hovered = Brush(Sage);
    Style.Pressed = Brush(Green);
    Style.NormalPadding = Style.PressedPadding = FMargin(10, 3);
    Button->SetStyle(Style);
    Label(Tree, TEXT("Text_Label"), 12);
    Metadata.SetValue(&BP, Key, TEXT("1"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, Key);
    return false;
}

bool RefineCatalogRowPreviewV9(UWidgetBlueprint& BP)
{
    constexpr const TCHAR* Key = TEXT("FF.BuildModeClarity.RowPreviewV9");
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, Key) == TEXT("1")) return true;
    auto& Tree = *BP.WidgetTree;
    auto* Dock = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_ClarityCatalogDock")));
    auto* Tiles = Cast<UFF_PrepBuildTileView>(Tree.FindWidget(TEXT("TileView_Facilities")));
    if (!Dock || !Tiles) return false;
    BP.Modify();
    Tree.Modify();
    // Keep the six columns and full-size cards. The extra 62 logical pixels expose
    // approximately 30% of the next 204px row above the existing footer.
    Dock->SetHeightOverride(454.f);
    Tiles->SetClipping(EWidgetClipping::ClipToBounds);
    if (auto* Status = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Status"))))
        Status->SetText(LOCTEXT("BrowseRowsV9", "카드 선택 → 위치 지정  ·  확정 시 결제  ·  위아래 드래그 / 휠로 넘기기  ·  ☆ 즐겨찾기"));
    if (!SaveNewBlueprint(Prefix, BP)) return false;
    auto* HeightProperty = FindFProperty<FFloatProperty>(BP.GeneratedClass.Get(), TEXT("CatalogHeight"));
    if (!HeightProperty) return false;
    HeightProperty->SetPropertyValue_InContainer(BP.GeneratedClass->GetDefaultObject(), 454.f);
    Metadata.SetValue(&BP, Key, TEXT("1"));
    if (SaveAssetPackage(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, Key);
    return false;
}

void Apply()
{
    auto* Panel = LoadExistingBlueprint(PanelPackage);
    auto* Tile = LoadExistingBlueprint(TilePackage);
    auto* Choice = LoadExistingBlueprint(ChoicePackage);
    if (!Panel || !Tile || !Choice || !Panel->WidgetTree || !Tile->WidgetTree || !Choice->WidgetTree ||
        Panel->ParentClass != UFF_PrepPanelBuild::StaticClass() || Tile->ParentClass != UFF_PrepBuildTileWidget::StaticClass() ||
        Choice->ParentClass != UFF_PrepChoiceButton::StaticClass())
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: expected catalog and tile parents are required"), Prefix);
        return;
    }
    const bool bSaved = MigrateTile(*Tile) && MigratePanel(*Panel) && RefineRowClearance(*Panel) &&
        RefineHeaderDecoration(*Panel) && RefineDirectPlacementTile(*Tile) && RefineDirectPlacementPanel(*Panel) &&
        RefineCatalogClearanceV5(*Panel) && RefineHeaderGlyphClearanceV6(*Panel) &&
        RefineRequiredTileV7(*Tile) && RefineRequiredGuideV7(*Panel) && RefineCategoryPictogramsV8(*Choice) &&
        RefineCatalogRowPreviewV9(*Panel);
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_%s: direct preview, pearl program/required cards and six-column browsing with a partial next row"), Prefix,
        bSaved ? TEXT("DONE") : TEXT("FAIL"));
}
FAutoConsoleCommand Command(TEXT("FF.ApplyBuildModeClarityCatalog"),
    TEXT("Apply the six-column Clarity catalog with a partial next row once; keep subsequent Designer changes."),
    FConsoleCommandDelegate::CreateStatic(&Apply));
}
#undef LOCTEXT_NAMESPACE
#endif
