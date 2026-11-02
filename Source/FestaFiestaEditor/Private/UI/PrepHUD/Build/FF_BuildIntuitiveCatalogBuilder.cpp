// Apply the compact picture catalog and its editable filtering rail to the production WBPs.
#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Brushes/SlateNoResource.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "FFBuildIntuitiveCatalog"
namespace FFBuildIntuitiveCatalog
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Prefix = TEXT("FF_BUILD_INTUITIVE_CATALOG");

FLinearColor Color(uint8 R, uint8 G, uint8 B, float A = 1.f)
{
    FLinearColor C = FLinearColor::FromSRGBColor(FColor(R, G, B));
    C.A = A;
    return C;
}
const FLinearColor Ink = Color(32, 66, 64);
const FLinearColor Muted = Color(103, 127, 126);
const FLinearColor Teal = Color(30, 124, 122);
const FLinearColor Line = Color(207, 224, 222);

FSlateBrush Rounded(FLinearColor Fill, float Radius = 8.f, FLinearColor Outline = Line, float Width = 1.f)
{
    FSlateBrush B;
    B.DrawAs = ESlateBrushDrawType::RoundedBox;
    B.TintColor = Fill;
    B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
    B.OutlineSettings.CornerRadii = FVector4(Radius);
    B.OutlineSettings.Color = Outline;
    B.OutlineSettings.Width = Width;
    return B;
}

template<typename T> T* GetOrMake(UWidgetBlueprint& BP, const TCHAR* Name, bool Variable = false)
{
    if (auto* W = Cast<T>(BP.WidgetTree->FindWidget(Name))) return W;
    if (auto* W = FindObject<T>(BP.WidgetTree, Name)) return W;
    return MakeWidget<T>(BP, *BP.WidgetTree, Name, Variable);
}

void H(UHorizontalBox& Row, UWidget& W, bool Fill = false, FMargin Padding = FMargin(0))
{
    W.RemoveFromParent();
    auto* S = Row.AddChildToHorizontalBox(&W);
    S->SetSize(FSlateChildSize(Fill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
    S->SetPadding(Padding);
    S->SetVerticalAlignment(VAlign_Center);
}
void V(UVerticalBox& Column, UWidget& W, bool Fill = false, FMargin Padding = FMargin(0))
{
    W.RemoveFromParent();
    auto* S = Column.AddChildToVerticalBox(&W);
    S->SetSize(FSlateChildSize(Fill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
    S->SetPadding(Padding);
    S->SetHorizontalAlignment(HAlign_Fill);
}
UTextBlock* Label(UWidgetBlueprint& BP, const TCHAR* Name, const FText& Value, int32 Size, FLinearColor Tint = Ink)
{
    auto* T = GetOrMake<UTextBlock>(BP, Name, true);
    T->SetText(Value);
    StyleText(*T, Size, Tint, TEXT("Medium"));
    T->SetAutoWrapText(false);
    T->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    return T;
}
void StyleButton(UButton& B)
{
    FButtonStyle S = B.GetStyle();
    S.Normal = Rounded(Color(255, 255, 255, .8f));
    S.Hovered = Rounded(Color(230, 245, 242), 8, Teal);
    S.Pressed = Rounded(Color(211, 235, 231), 8, Teal);
    S.NormalPadding = S.PressedPadding = FMargin(10, 4);
    B.SetStyle(S);
}
void SetFloat(UObject& Object, const TCHAR* Name, float Value)
{ if (auto* P = FindFProperty<FFloatProperty>(Object.GetClass(), Name)) P->SetPropertyValue_InContainer(&Object, Value); }
void SetInt(UObject& Object, const TCHAR* Name, int32 Value)
{ if (auto* P = FindFProperty<FIntProperty>(Object.GetClass(), Name)) P->SetPropertyValue_InContainer(&Object, Value); }
void SetColor(UObject& Object, const TCHAR* Name, FLinearColor Value)
{ if (auto* P = FindFProperty<FStructProperty>(Object.GetClass(), Name)) *P->ContainerPtrToValuePtr<FLinearColor>(&Object) = Value; }

bool Panel(UWidgetBlueprint& BP)
{
    auto& T = *BP.WidgetTree;
    auto* Catalog = Cast<UVerticalBox>(T.FindWidget(TEXT("Panel_Catalog")));
    auto* Dock = Cast<USizeBox>(T.FindWidget(TEXT("Size_ClarityCatalogDock")));
    auto* Tray = Cast<UBorder>(T.FindWidget(TEXT("Border_PanelRoot")));
    auto* Categories = Cast<USizeBox>(T.FindWidget(TEXT("Size_CategoryRow")));
    auto* Minor = Cast<USizeBox>(T.FindWidget(TEXT("Size_SubcategoryViewport")));
    auto* Cards = Cast<USizeBox>(T.FindWidget(TEXT("Size_A5CatalogRows")));
    auto* List = Cast<UFF_PrepBuildTileView>(T.FindWidget(TEXT("TileView_Facilities")));
    auto* Search = Cast<USizeBox>(T.FindWidget(TEXT("Size_PearlSearch")));
    auto* Favorites = Cast<USizeBox>(T.FindWidget(TEXT("Size_FavoritesOnly")));
    auto* Ranking = Cast<USizeBox>(T.FindWidget(TEXT("Size_PearlRevenueRanking")));
    auto* Codex = Cast<UHorizontalBox>(T.FindWidget(TEXT("Row_BuildCodexCategory")));
    auto* Heading = Cast<UTextBlock>(T.FindWidget(TEXT("Text_A5CatalogHeading")));
    auto* Footer = Cast<UHorizontalBox>(T.FindWidget(TEXT("Row_A5CatalogFooter")));
    auto* Reset = Cast<UButton>(T.FindWidget(TEXT("Button_ResetFilters")));
    auto* LegacyBody = T.FindWidget(TEXT("VerticalBox_CatalogBody"));
    if (!Catalog || !Dock || !Tray || !Categories || !Minor || !Cards || !List ||
        !Search || !Favorites || !Ranking || !Codex || !Heading || !Footer || !Reset) return false;
    BP.Modify(); T.Modify();
    // Reparent the named production controls; retain their graphs, icons, tooltips and bindings.
    for (UWidget* W : TArray<UWidget*>{Categories, Minor, Cards, Search, Favorites, Ranking, Codex, Heading, Footer, Reset})
        W->RemoveFromParent();
    Catalog->ClearChildren();
    Tray->SetBrush(Rounded(Color(244, 250, 248, .97f), 14, Color(189, 214, 210)));
    Tray->SetBrushColor(FLinearColor::White);
    Tray->SetPadding(FMargin(12, 8));
    if (auto* Outer = Cast<UBorder>(T.FindWidget(TEXT("Border_ApprovedCatalogV9"))))
    { Outer->SetBrush(Rounded(FLinearColor::Transparent, 0, FLinearColor::Transparent, 0)); Outer->SetPadding(FMargin(0)); }
    Dock->SetHeightOverride(420);

    auto* Header = GetOrMake<UHorizontalBox>(BP, TEXT("Row_IntuitiveCatalogHeader"));
    Header->ClearChildren();
    Heading->SetText(LOCTEXT("Heading", "시설 카탈로그"));
    StyleText(*Heading, 19, Ink, TEXT("SemiBold"));
    H(*Header, *Heading, false, FMargin(2, 0, 10, 0));
    H(*Header, *Label(BP, TEXT("Text_ItemCount"), LOCTEXT("Count", "전체 시설"), 12, Muted), true);
    Search->SetWidthOverride(220); Search->SetHeightOverride(34);
    H(*Header, *Search, false, FMargin(8, 0));
    Favorites->SetWidthOverride(144); Favorites->SetHeightOverride(34);
    H(*Header, *Favorites, false, FMargin(0, 0, 8, 0));
    Ranking->SetWidthOverride(106); Ranking->SetHeightOverride(34);
    H(*Header, *Ranking, false, FMargin(0, 0, 8, 0));
    H(*Header, *Codex);
    V(*Catalog, *Header, false, FMargin(0, 0, 0, 7));

    Categories->SetHeightOverride(36);
    V(*Catalog, *Categories, false, FMargin(0, 0, 0, 5));
    auto* MinorRow = GetOrMake<UHorizontalBox>(BP, TEXT("Row_IntuitiveCatalogMinor"));
    MinorRow->ClearChildren();
    Minor->SetHeightOverride(30);
    Minor->ClearWidthOverride();
    H(*MinorRow, *Minor, true);
    H(*MinorRow, *Label(BP, TEXT("Text_ActiveFilterCount"), LOCTEXT("NoFilters", "필터 없음"), 11, Muted));
    V(*Catalog, *MinorRow, false, FMargin(0, 0, 0, 5));

    auto* Content = GetOrMake<UHorizontalBox>(BP, TEXT("Row_IntuitiveCatalogContent"));
    Content->ClearChildren();
    Cards->ClearHeightOverride();
    Cards->SetMinDesiredHeight(260);
    H(*Content, *Cards, true);
    if (auto* S = Cast<UHorizontalBoxSlot>(Cards->Slot)) S->SetVerticalAlignment(VAlign_Fill);
    List->SetEntryHeight(130);
    List->SetHorizontalEntrySpacing(6);
    List->SetVerticalEntrySpacing(0);
    List->SetClipping(EWidgetClipping::ClipToBoundsAlways);
    if (auto* P = FindFProperty<FStructProperty>(List->GetClass(), TEXT("ScrollBarStyle")))
    {
        auto& S = *P->ContainerPtrToValuePtr<FScrollBarStyle>(List);
        S.NormalThumbImage = Rounded(Color(100, 157, 149), 3, FLinearColor::Transparent, 0);
        S.HoveredThumbImage = Rounded(Teal, 3, FLinearColor::Transparent, 0);
        S.DraggedThumbImage = S.HoveredThumbImage;
        S.VerticalBackgroundImage = Rounded(Color(220, 234, 230), 3, FLinearColor::Transparent, 0);
        S.VerticalTopSlotImage = S.VerticalBottomSlotImage = FSlateNoResource();
        S.Thickness = 6.f;
        for (FSlateBrush* B : {&S.NormalThumbImage, &S.HoveredThumbImage, &S.DraggedThumbImage, &S.VerticalBackgroundImage})
            B->ImageSize = FVector2D(6.f);
    }
    auto* RailBounds = GetOrMake<USizeBox>(BP, TEXT("Size_CatalogFilters"));
    RailBounds->SetWidthOverride(198);
    auto* Rail = GetOrMake<UBorder>(BP, TEXT("Border_CatalogFilters"));
    Rail->SetBrush(Rounded(Color(232, 242, 239), 10, Color(214, 230, 226)));
    Rail->SetPadding(FMargin(12, 8));
    RailBounds->SetContent(Rail);
    auto* Filters = GetOrMake<UVerticalBox>(BP, TEXT("Column_CatalogFilters"));
    Filters->ClearChildren(); Rail->SetContent(Filters);
    auto* FilterHeader = GetOrMake<UHorizontalBox>(BP, TEXT("Row_CatalogFilterTitle"));
    FilterHeader->ClearChildren();
    H(*FilterHeader, *Label(BP, TEXT("Text_CatalogFilterTitle"), LOCTEXT("Filter", "아이템 필터"), 14), true);
    StyleButton(*Reset);
    Label(BP, TEXT("Text_ResetFilters"), LOCTEXT("Reset", "초기화"), 11, Teal);
    H(*FilterHeader, *Reset);
    V(*Filters, *FilterHeader, false, FMargin(0, 0, 0, 5));
    auto* Sort = GetOrMake<UComboBoxString>(BP, TEXT("ComboBox_CatalogSort"), true);
    if (auto* P = FindFProperty<FArrayProperty>(Sort->GetClass(), TEXT("DefaultOptions")))
        *P->ContainerPtrToValuePtr<TArray<FString>>(Sort) = {TEXT("추천순"), TEXT("이름순"), TEXT("낮은 가격순"), TEXT("높은 가격순")};
    Sort->ClearOptions();
    for (const TCHAR* Option : {TEXT("추천순"), TEXT("이름순"), TEXT("낮은 가격순"), TEXT("높은 가격순")}) Sort->AddOption(Option);
    Sort->SetSelectedIndex(0);
    FSlateFontInfo SortFont = Heading->GetFont(); SortFont.Size = 12;
    // UE 5.7 exposes these serialized properties without public runtime setters.
    if (auto* P = FindFProperty<FStructProperty>(Sort->GetClass(), TEXT("Font")))
        *P->ContainerPtrToValuePtr<FSlateFontInfo>(Sort) = SortFont;
    if (auto* P = FindFProperty<FStructProperty>(Sort->GetClass(), TEXT("ForegroundColor")))
        *P->ContainerPtrToValuePtr<FSlateColor>(Sort) = FSlateColor(Ink);
    auto SortStyle = Sort->GetWidgetStyle();
    SortStyle.ComboButtonStyle.ButtonStyle.Normal = Rounded(Color(255, 255, 255, .95f));
    SortStyle.ComboButtonStyle.ButtonStyle.Hovered = Rounded(Color(241, 250, 247), 8, Teal);
    SortStyle.ComboButtonStyle.ButtonStyle.NormalPadding = FMargin(8, 4);
    Sort->SetWidgetStyle(SortStyle);
    V(*Filters, *Sort, false, FMargin(0, 0, 0, 8));
    struct FFilter { const TCHAR* Name; FText Text; FText Tip; };
    for (const FFilter& F : {
        FFilter{TEXT("Check_FilterRequired"), LOCTEXT("Required", "필수 시설"), LOCTEXT("RequiredTip", "축제 운영에 필요한 시설만 표시합니다.")},
        FFilter{TEXT("Check_FilterAffordable"), LOCTEXT("Affordable", "배치 가능"), LOCTEXT("AffordableTip", "보유 금화와 설치 수 제한을 기준으로 지금 배치 가능한 시설을 표시합니다.")},
        FFilter{TEXT("Check_FilterPrograms"), LOCTEXT("Programs", "프로그램 지원"), LOCTEXT("ProgramsTip", "프로그램을 진행할 수 있는 시설만 표시합니다.")},
        FFilter{TEXT("Check_FilterLighting"), LOCTEXT("Lighting", "조명 시설"), LOCTEXT("LightingTip", "조명을 지원하는 시설만 표시합니다.")}})
    {
        auto* Check = GetOrMake<UCheckBox>(BP, F.Name, true);
        FCheckBoxStyle S = Check->GetWidgetStyle();
        S.CheckBoxType = ESlateCheckBoxType::ToggleButton;
        S.UncheckedImage = Rounded(Color(248, 252, 250), 6);
        S.UncheckedHoveredImage = Rounded(Color(235, 248, 244), 6, Teal);
        S.UncheckedPressedImage = S.UncheckedHoveredImage;
        S.CheckedImage = Rounded(Color(208, 235, 228), 6, Teal);
        S.CheckedHoveredImage = S.CheckedImage; S.CheckedPressedImage = S.CheckedImage;
        S.Padding = FMargin(10, 5);
        Check->SetWidgetStyle(S); Check->SetToolTipText(F.Tip);
        const FString TextName = FString(F.Name).Replace(TEXT("Check_"), TEXT("Text_"));
        Check->SetContent(Label(BP, *TextName, F.Text, 12));
        V(*Filters, *Check, false, FMargin(0, 0, 0, 5));
    }
    V(*Filters, *Label(BP, TEXT("Text_FilterCombineHint"), LOCTEXT("Combine", "여러 조건을 함께 선택할 수 있어요"), 10, Muted), false, FMargin(0, 3, 0, 0));
    H(*Content, *RailBounds, false, FMargin(10, 0, 0, 0));
    if (auto* S = Cast<UHorizontalBoxSlot>(RailBounds->Slot)) S->SetVerticalAlignment(VAlign_Fill);
    V(*Catalog, *Content, true);
    V(*Catalog, *Footer, false, FMargin(2, 6, 2, 0));
    // Keep the old, now empty layout hosts attached for Blueprint references.
    // The builder rejects orphaned widgets, even when their former contents moved.
    if (LegacyBody)
    { LegacyBody->SetVisibility(ESlateVisibility::Collapsed); V(*Catalog, *LegacyBody); }
    Label(BP, TEXT("Text_A5CatalogDragHint"), LOCTEXT("DragHint", "드래그 / 휠로 둘러보기 · 클릭하여 배치"), 10, Muted);
    for (const TCHAR* N : {TEXT("Text_A5LegendRequired"), TEXT("Text_A5LegendLighting"), TEXT("Text_A5LegendProgram")})
        if (auto* L = Cast<UTextBlock>(T.FindWidget(N))) StyleText(*L, 10, Muted, TEXT("Medium"));
    for (const TCHAR* N : {TEXT("Button_FavoritesOnly"), TEXT("Button_RevenueRanking")})
        if (auto* B = Cast<UButton>(T.FindWidget(N))) StyleButton(*B);
    Label(BP, TEXT("Text_FavoritesOnly"), LOCTEXT("Favorites", "즐겨찾기"), 12);
    Label(BP, TEXT("Text_RevenueRanking"), LOCTEXT("Revenue", "매출 순위"), 12);
    if (auto* Edit = Cast<UEditableTextBox>(T.FindWidget(TEXT("Search_Facilities"))))
    {
        Edit->SetHintText(LOCTEXT("Search", "이름·종류로 검색"));
        auto S = Edit->GetWidgetStyle();
        S.BackgroundImageNormal = Rounded(Color(255, 255, 255));
        S.BackgroundImageHovered = Rounded(Color(247, 253, 250), 8, Teal);
        S.BackgroundImageFocused = Rounded(Color(255, 255, 255), 8, Teal, 1.5f);
        S.TextStyle.Font.Size = 12; S.ForegroundColor = Ink; S.Padding = FMargin(28, 5, 8, 5);
        Edit->SetWidgetStyle(S);
    }
    if (!SaveNewBlueprint(Prefix, BP)) return false;
    UObject& CDO = *BP.GeneratedClass->GetDefaultObject();
    SetFloat(CDO, TEXT("CatalogHeight"), 420); SetInt(CDO, TEXT("CardsPerPage"), 8);
    SetInt(CDO, TEXT("VisibleCatalogRows"), 2);
    return SaveAssetPackage(Prefix, BP);
}

bool Tile(UWidgetBlueprint& BP)
{
    auto& T = *BP.WidgetTree;
    auto* Card = Cast<UBorder>(T.FindWidget(TEXT("Border_Card")));
    auto* Size = Cast<USizeBox>(T.FindWidget(TEXT("SizeBox_Card")));
    auto* Thumb = Cast<USizeBox>(T.FindWidget(TEXT("SizeBox_Thumbnail")));
    auto* Name = Cast<UTextBlock>(T.FindWidget(TEXT("Text_Name")));
    auto* NameSize = Cast<USizeBox>(T.FindWidget(TEXT("Size_PearlFacilityName")));
    auto* Meta = Cast<UHorizontalBox>(T.FindWidget(TEXT("HorizontalBox_Meta")));
    if (!Card || !Size || !Thumb || !Name || !NameSize || !Meta) return false;
    BP.Modify(); T.Modify();
    Size->ClearWidthOverride(); Size->SetHeightOverride(120); Size->SetMinDesiredWidth(100);
    Card->SetBrush(Rounded(Color(253, 255, 254), 9, Color(213, 227, 224)));
    Card->SetBrushColor(FLinearColor::White); Card->SetPadding(FMargin(6, 5));
    Thumb->ClearWidthOverride(); Thumb->SetHeightOverride(70);
    if (auto* Back = Cast<UBorder>(T.FindWidget(TEXT("Border_ThumbnailBackdrop"))))
    { Back->SetBrush(Rounded(Color(238, 246, 243), 6, FLinearColor::Transparent, 0)); Back->SetBrushColor(FLinearColor::White); }
    NameSize->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    NameSize->ClearMinDesiredHeight(); NameSize->SetHeightOverride(22);
    StyleText(*Name, 12, Ink, TEXT("SemiBold"));
    Name->SetVisibility(ESlateVisibility::HitTestInvisible); Name->SetAutoWrapText(false);
    Name->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis); Name->SetJustification(ETextJustify::Left);
    if (auto* S = Cast<UVerticalBoxSlot>(NameSize->Slot)) S->SetPadding(FMargin(2, 1, 2, 0));
    Meta->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (auto* S = Cast<UVerticalBoxSlot>(Meta->Slot)) S->SetPadding(FMargin(2, 0));
    if (auto* Cost = Cast<UTextBlock>(T.FindWidget(TEXT("Text_Cost"))))
    { StyleText(*Cost, 11, Teal, TEXT("SemiBold")); Cost->SetAutoWrapText(false); Cost->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis); }
    if (auto* Gold = Cast<UBorder>(T.FindWidget(TEXT("Border_AuroraCardGold"))))
    { Gold->SetBrush(Rounded(FLinearColor::Transparent, 0, FLinearColor::Transparent, 0)); Gold->SetPadding(FMargin(0)); }
    if (auto* GoldIcon = Cast<USizeBox>(T.FindWidget(TEXT("Size_CostGold"))))
    { GoldIcon->SetWidthOverride(12); GoldIcon->SetHeightOverride(12); }
    for (const TCHAR* N : {TEXT("Text_CategoryPath"), TEXT("Text_BuildLimit")})
        if (auto* W = T.FindWidget(N)) W->SetVisibility(ESlateVisibility::Collapsed);
    if (auto* State = Cast<UTextBlock>(T.FindWidget(TEXT("Text_State"))))
    { StyleText(*State, 10, Color(170, 62, 56), TEXT("Medium")); State->SetAutoWrapText(false); }
    if (auto* S = Cast<USizeBox>(T.FindWidget(TEXT("Size_Favorite"))))
    { S->SetWidthOverride(22); S->SetHeightOverride(22); }
    if (!SaveNewBlueprint(Prefix, BP)) return false;
    auto& CDO = *BP.GeneratedClass->GetDefaultObject();
    SetColor(CDO, TEXT("HoverTint"), Color(231, 247, 242));
    SetColor(CDO, TEXT("SelectedTint"), Color(208, 236, 229));
    return SaveAssetPackage(Prefix, BP);
}

void Apply()
{
    if (GEditor && GEditor->PlayWorld) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL stop PIE before applying"), Prefix); return; }
    auto* P = LoadExistingBlueprint(FFBuildUIAssetPaths::BuildPanelPackage);
    auto* T = LoadExistingBlueprint(FFBuildUIAssetPaths::BuildTilePackage);
    if (!P || !T || !Tile(*T) || !Panel(*P))
    { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL"), Prefix); return; }
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_PASS"), Prefix);
}
FAutoConsoleCommand Command(TEXT("FF.ApplyBuildIntuitiveCatalog"),
    TEXT("Apply the compact two-row facility catalog with a filter rail; preserves production control bindings."),
    FConsoleCommandDelegate::CreateStatic(&Apply));
}
#undef LOCTEXT_NAMESPACE
#endif
