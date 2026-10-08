// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// FF_FacilityAuroraBuildWbpBuilder.cpp
// 역할: 시설 카탈로그와 도구 모음에 저장된 Aurora 디자인을 적용한다.

// Explicit V7 migration. Existing catalog graphs, category surfaces and program badges survive.
#include "CoreMinimal.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#if WITH_EDITOR
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"
#include "UObject/UnrealType.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FFFacilityAuroraBuild"
namespace FFFacilityAuroraBuild
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Prefix = TEXT("FF_FACILITY_AURORA_BUILD");
const FString PanelPackage = FFBuildUIAssetPaths::BuildPanelPackage;
const FString TilePackage = FFBuildUIAssetPaths::BuildTilePackage;
const FString ToolsPackage = FFBuildUIAssetPaths::BuildToolsPackage;
const FLinearColor Ink = FLinearColor::FromSRGBColor(FColor(0x2B, 0x51, 0x3C));
const FLinearColor Paper = FLinearColor::FromSRGBColor(FColor(0xEC, 0xF5, 0xE6));
const FLinearColor Green = FLinearColor::FromSRGBColor(FColor(0x2F, 0x63, 0x45));
const FLinearColor Gold = FLinearColor::FromSRGBColor(FColor(0x72, 0x55, 0x1D));
const FLinearColor GoldPaper = FLinearColor::FromSRGBColor(FColor(0xF4, 0xE7, 0xB8));

FSlateBrush Brush(FLinearColor Color)
{
    FSlateBrush B;
    B.DrawAs = ESlateBrushDrawType::RoundedBox;
    B.TintColor = Color;
    B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
    B.OutlineSettings.CornerRadii = FVector4(10.f);
    B.OutlineSettings.Color = FLinearColor::FromSRGBColor(FColor(0x97, 0xBE, 0xA3));
    B.OutlineSettings.Width = 1;
    return B;
}
UTextBlock* Text(UWidgetBlueprint& BP, UWidgetTree& Tree, FName Name, const FText& Value,
    int32 FontSize = 18, FLinearColor Color = Ink, bool Variable = true)
{
    auto* T = MakeWidget<UTextBlock>(BP, Tree, Name, Variable);
    StyleText(*T, FontSize, Color, TEXT("Medium"));
    T->SetText(Value);
    T->SetVisibility(ESlateVisibility::HitTestInvisible);
    T->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    return T;
}
void V(UVerticalBox& Parent, UWidget& Child, float Bottom = 10.f)
{
    auto* S = Parent.AddChildToVerticalBox(&Child);
    S->SetPadding(FMargin(0, 0, 0, Bottom));
}
void H(UHorizontalBox& Parent, UWidget& Child, bool Fill = false, float Right = 8.f)
{
    auto* S = Parent.AddChildToHorizontalBox(&Child);
    S->SetSize(FSlateChildSize(Fill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
    S->SetVerticalAlignment(VAlign_Center);
    S->SetPadding(FMargin(0, 0, Right, 0));
}
UBorder* Panel(UWidgetBlueprint& BP, UWidgetTree& Tree, FName Name, UWidget& Child,
    FLinearColor Color = Paper)
{
    auto* B = MakeWidget<UBorder>(BP, Tree, Name, true);
    B->SetBrush(Brush(Color));
    B->SetBrushColor(FLinearColor::White);
    B->SetPadding(FMargin(18, 14));
    B->AddChild(&Child);
    return B;
}
UFF_BuildToolsActionButton* Button(UWidgetBlueprint& BP, UWidgetTree& Tree, FName Name,
    FName LabelName, FText Label, bool Primary = false)
{
    auto* B = MakeWidget<UFF_BuildToolsActionButton>(BP, Tree, Name, true);
    FButtonStyle S;
    S.Normal = Brush(Primary ? Green : Paper);
    S.Hovered = Brush(FLinearColor::FromSRGBColor(FColor(0xA3, 0xDC, 0xBB)));
    S.Pressed = Brush(Green);
    S.Disabled = Brush(FLinearColor::FromSRGBColor(FColor(0xB6, 0xC4, 0xB5)));
    S.NormalPadding = S.PressedPadding = FMargin(13, 9);
    B->SetStyle(S);
    B->bUseSelectionStyle = true;
    B->SelectionStyle = S;
    B->SelectionStyle.Normal = Brush(Green);
    B->SelectionStyle.Hovered = Brush(Green);
    B->SelectionTextColor = FLinearColor::White;
    auto* T = Text(BP, Tree, LabelName, Label, 17, Primary ? FLinearColor::White : Ink);
    auto* Slot = CastChecked<UButtonSlot>(B->AddChild(T));
    Slot->SetHorizontalAlignment(HAlign_Center);
    Slot->SetVerticalAlignment(VAlign_Center);
    return B;
}
USizeBox* CanvasPanel(UWidgetBlueprint& BP, UWidgetTree& Tree, UCanvasPanel& Canvas, FName Name,
    UWidget& Child, float Width, FVector2D Anchor, FVector2D Position, FVector2D Alignment = FVector2D::ZeroVector)
{
    auto* Size = MakeWidget<USizeBox>(BP, Tree, Name);
    Size->SetWidthOverride(Width);
    Size->AddChild(&Child);
    auto* Slot = Canvas.AddChildToCanvas(Size);
    Slot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
    Slot->SetAlignment(Alignment);
    Slot->SetPosition(Position);
    Slot->SetAutoSize(true);
    return Size;
}

bool MigrateCatalog(UWidgetBlueprint& BP)
{
    UWidgetTree& Tree = *BP.WidgetTree;
    if (Tree.FindWidget(TEXT("Border_AuroraSelection"))) return true;
    auto* Catalog = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Panel_Catalog")));
    if (!Catalog || !Tree.FindWidget(TEXT("TileView_Facilities"))) return false;
    auto* Row = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_AuroraSelection"));
    auto* Identity = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Column_AuroraSelection"));
    V(*Identity, *Text(BP, Tree, TEXT("Label_SelectedFacility"), LOCTEXT("Selected", "선택한 시설"), 15), 4);
    V(*Identity, *Text(BP, Tree, TEXT("Text_SelectedFacility"), LOCTEXT("Select", "시설을 선택하세요"), 25), 0);
    H(*Row, *Identity, true, 18);
    auto* Cost = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Column_SelectedGold"));
    V(*Cost, *Text(BP, Tree, TEXT("Label_SelectedCost"), LOCTEXT("Install", "설치 비용"), 15), 4);
    V(*Cost, *Text(BP, Tree, TEXT("Text_SelectedCost"), FText::FromString(TEXT("—")), 29, Gold), 0);
    H(*Row, *Cost, false, 35);
    auto* After = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Column_AfterGold"));
    V(*After, *Text(BP, Tree, TEXT("Label_AfterPlacementGold"), LOCTEXT("After", "설치 후 잔액"), 15), 4);
    V(*After, *Text(BP, Tree, TEXT("Text_AfterPlacementGold"), FText::FromString(TEXT("—")), 29), 0);
    H(*Row, *After, false, 35);
    auto* Action = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Column_PlacementAction"));
    auto* Place = Button(BP, Tree, TEXT("Button_PlaceSelected"), TEXT("Text_PlaceSelected"), LOCTEXT("Place", "선택해 배치  →"), true);
    // Catalog supports keyboard/gamepad focus, unlike the nonmodal placement toolbar.
    Place->ConfigureFocusable(true);
    Place->SetIsEnabled(false);
    V(*Action, *Place, 5);
    V(*Action, *Text(BP, Tree, TEXT("Text_SelectionHint"), LOCTEXT("Hint", "카드를 선택하고 설치 비용을 확인하세요."), 14), 0);
    H(*Row, *Action, false, 0);
    auto* Selection = Panel(BP, Tree, TEXT("Border_AuroraSelection"), *Row);
    auto* SelectionSlot = Cast<UVerticalBoxSlot>(Catalog->InsertChildAt(FMath::Max(0, Catalog->GetChildrenCount() - 1), Selection));
    if (!SelectionSlot) return false;
    SelectionSlot->SetPadding(FMargin(18, 8));
    if (auto* Wallet = Cast<UBorder>(Tree.FindWidget(TEXT("Border_Gold"))))
    {
        Wallet->SetBrush(Brush(GoldPaper));
        Wallet->SetBrushColor(FLinearColor::White);
    }
    if (auto* T = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Gold")))) StyleText(*T, 30, Gold, TEXT("Bold"));
    if (auto* Header = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_CatalogHeader")))) Header->SetHeightOverride(88);
    if (auto* Tiles = Cast<UFF_PrepBuildTileView>(Tree.FindWidget(TEXT("TileView_Facilities")))) Tiles->SetEntryHeight(218);
    return SaveNewBlueprint(Prefix, BP);
}

bool MigrateTile(UWidgetBlueprint& BP)
{
    auto& Tree = *BP.WidgetTree;
    if (Tree.FindWidget(TEXT("Border_AuroraCardGold"))) return true;
    auto* Cost = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Cost")));
    auto* Meta = Cost ? Cast<UHorizontalBox>(Cost->GetParent()) : nullptr;
    if (!Cost || !Meta) return false;
    const int32 Index = Meta->GetChildIndex(Cost);
    Cost->RemoveFromParent();
    auto* Price = Panel(BP, Tree, TEXT("Border_AuroraCardGold"), *Cost, GoldPaper);
    Price->SetPadding(FMargin(8, 4));
    auto* S = Cast<UHorizontalBoxSlot>(Meta->InsertChildAt(Index, Price));
    if (!S) return false;
    S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    S->SetPadding(FMargin(0, 0, 5, 0));
    StyleText(*Cost, 27, Gold, TEXT("Bold"));
    if (auto* Name = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Name")))) StyleText(*Name, 20, Ink, TEXT("SemiBold"));
    if (auto* State = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_State")))) StyleText(*State, 15, Ink, TEXT("Medium"));
    if (auto* Size = Cast<USizeBox>(Tree.FindWidget(TEXT("SizeBox_Card")))) Size->SetHeightOverride(218);
    if (auto* Thumb = Cast<USizeBox>(Tree.FindWidget(TEXT("SizeBox_Thumbnail")))) Thumb->SetHeightOverride(102);
    return SaveNewBlueprint(Prefix, BP);
}

bool BuildTools(UWidgetBlueprint& BP, UWidgetTree& Tree)
{
    auto* Canvas = MakeWidget<UCanvasPanel>(BP, Tree, TEXT("Canvas_AuroraBuildV7"));
    Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Tree.RootWidget = Canvas;
    auto* Info = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Column_PlacementInfo"));
    V(*Info, *Text(BP, Tree, TEXT("Text_Mode"), LOCTEXT("Mode", "시설 배치"), 23), 10);
    V(*Info, *Text(BP, Tree, TEXT("Text_Legend"), LOCTEXT("Ready", "배치 위치를 선택하세요"), 17), 14);
    V(*Info, *Panel(BP, Tree, TEXT("Border_PlacementGold"), *Text(BP, Tree, TEXT("Text_Cost"), LOCTEXT("Cost", "설치 비용 확인 중"), 33, Gold), GoldPaper), 10);
    V(*Info, *Text(BP, Tree, TEXT("Text_Balance"), LOCTEXT("Balance", "보유 골드 확인 중"), 17), 15);
    V(*Info, *Text(BP, Tree, TEXT("Text_Footprint"), FText::GetEmpty(), 18), 15);
    V(*Info, *Text(BP, Tree, TEXT("Label_Queue"), LOCTEXT("Queue", "대기열 정보"), 20), 8);
    V(*Info, *Panel(BP, Tree, TEXT("Border_Queue"), *Text(BP, Tree, TEXT("Text_Queue"), FText::GetEmpty(), 18)), 10);
    V(*Info, *Text(BP, Tree, TEXT("Text_RangeLegend"), LOCTEXT("Legend", "실선 시설 · 청록 대기열"), 14), 0);
    CanvasPanel(BP, Tree, *Canvas, TEXT("Size_PlacementInfo"), *Panel(BP, Tree, TEXT("Border_PlacementInfo"), *Info),
        454, FVector2D::ZeroVector, FVector2D(36, 70));

    auto* Help = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Column_PlacementHelp"));
    V(*Help, *Text(BP, Tree, TEXT("Label_Help"), LOCTEXT("Help", "조작 안내"), 20), 12);
    auto* ControlHelp = Text(BP, Tree, TEXT("Text_ControlHelp"), FText::GetEmpty(), 17);
    ControlHelp->SetLineHeightPercentage(1.35f);
    V(*Help, *ControlHelp, 12);
    auto* HelperButtons = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_HelpButtons"));
    H(*HelperButtons, *Button(BP, Tree, TEXT("Button_Catalog"), TEXT("Label_Catalog"), LOCTEXT("Catalog", "시설 목록  B")), true);
    H(*HelperButtons, *Button(BP, Tree, TEXT("Button_Details"), TEXT("Label_Details"), LOCTEXT("Details", "세부 조정 +")), true, 0);
    V(*Help, *HelperButtons, 0);
    CanvasPanel(BP, Tree, *Canvas, TEXT("Size_PlacementHelp"), *Panel(BP, Tree, TEXT("Border_PlacementHelp"), *Help),
        454, FVector2D(0, 1), FVector2D(36, -88), FVector2D(0, 1));

    auto* Quick = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Column_PlacementQuick"));
    V(*Quick, *Text(BP, Tree, TEXT("Label_CurrentFacility"), LOCTEXT("Current", "선택한 시설"), 15), 5);
    V(*Quick, *Text(BP, Tree, TEXT("Text_FacilityName"), LOCTEXT("Facility", "시설을 선택하세요"), 28), 15);
    auto* RowOne = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_ConfirmRotation"));
    H(*RowOne, *Button(BP, Tree, TEXT("Button_Confirm"), TEXT("Text_Confirm"), LOCTEXT("Confirm", "배치 확정"), true), true);
    H(*RowOne, *Button(BP, Tree, TEXT("Button_RotateLeft"), TEXT("Label_RotateLeft"), LOCTEXT("RotateLeft", "Q 회전")), true);
    H(*RowOne, *Button(BP, Tree, TEXT("Button_RotateRight"), TEXT("Label_RotateRight"), LOCTEXT("RotateRight", "회전 E")), true, 0);
    V(*Quick, *RowOne);
    auto* RowTwo = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_SnapGrid"));
    H(*RowTwo, *Button(BP, Tree, TEXT("Button_SnapMeter"), TEXT("Text_SnapMeter"), FText::FromString(TEXT("1 m"))), true);
    H(*RowTwo, *Button(BP, Tree, TEXT("Button_SnapPrecision"), TEXT("Text_SnapPrecision"), FText::FromString(TEXT("25 cm"))), true);
    H(*RowTwo, *Button(BP, Tree, TEXT("Button_SnapFree"), TEXT("Text_SnapFree"), FText::FromString(TEXT("1 cm"))), true);
    V(*Quick, *RowTwo);
    V(*Quick, *Button(BP, Tree, TEXT("Button_Grid"), TEXT("Text_Grid"), LOCTEXT("Grid", "그리드  G")));
    V(*Quick, *Text(BP, Tree, TEXT("Text_ActiveSnap"), FText::GetEmpty(), 15), 4);
    V(*Quick, *Text(BP, Tree, TEXT("Label_ChargeHint"), LOCTEXT("Charge", "설치 확정 시 골드 차감"), 14), 0);

    auto* Details = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Panel_Details"), true);
    Details->SetVisibility(ESlateVisibility::Collapsed);
    auto* Rotation = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_FreeRotation"));
    H(*Rotation, *Button(BP, Tree, TEXT("Button_FreeRotateLeft"), TEXT("Label_FreeLeft"), FText::FromString(TEXT("Shift + Q"))), true);
    auto* Angle = MakeWidget<UEditableTextBox>(BP, Tree, TEXT("Editable_Angle"), true);
    Angle->SetText(FText::FromString(TEXT("0.00")));
    Angle->SetHintText(LOCTEXT("Angle", "각도 0~360°"));
    H(*Rotation, *Angle, true);
    H(*Rotation, *Button(BP, Tree, TEXT("Button_FreeRotateRight"), TEXT("Label_FreeRight"), FText::FromString(TEXT("Shift + E"))), true, 0);
    V(*Details, *Rotation);
    V(*Details, *Text(BP, Tree, TEXT("Text_RotationHint"), FText::GetEmpty(), 14));
    auto* Nudge = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_Nudge"));
    H(*Nudge, *Button(BP, Tree, TEXT("Button_NudgeLeft"), TEXT("Label_NudgeLeft"), FText::FromString(TEXT("− X"))), true);
    H(*Nudge, *Button(BP, Tree, TEXT("Button_NudgeRight"), TEXT("Label_NudgeRight"), FText::FromString(TEXT("+ X"))), true);
    H(*Nudge, *Button(BP, Tree, TEXT("Button_NudgeDown"), TEXT("Label_NudgeDown"), FText::FromString(TEXT("− Y"))), true);
    H(*Nudge, *Button(BP, Tree, TEXT("Button_NudgeUp"), TEXT("Label_NudgeUp"), FText::FromString(TEXT("+ Y"))), true, 0);
    V(*Details, *Nudge, 0);
    V(*Quick, *Details, 0);
    CanvasPanel(BP, Tree, *Canvas, TEXT("Size_Tools"), *Panel(BP, Tree, TEXT("Border_Tools"), *Quick),
        456, FVector2D(0.68, 0.43), FVector2D::ZeroVector);
    return true;
}

bool RefineRotationLabels(UWidgetBlueprint& BP)
{
    constexpr const TCHAR* Key = TEXT("FFFacilityAuroraBuildRotationLabelsV1");
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, Key) == TEXT("1")) return true;
    auto* Left = Cast<UTextBlock>(BP.WidgetTree->FindWidget(TEXT("Label_RotateLeft")));
    auto* Right = Cast<UTextBlock>(BP.WidgetTree->FindWidget(TEXT("Label_RotateRight")));
    if (!Left || !Right) return false;
    Left->SetText(LOCTEXT("RotateLeft", "Q 회전"));
    Right->SetText(LOCTEXT("RotateRight", "회전 E"));
    Metadata.SetValue(&BP, Key, TEXT("1"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, Key);
    return false;
}

bool RefineCatalogHeight(UWidgetBlueprint& BP)
{
    constexpr const TCHAR* Key = TEXT("FFFacilityAuroraBuildCatalogHeightV1");
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, Key) == TEXT("1")) return true;
    if (!BP.GeneratedClass) return false;
    auto* Height = FindFProperty<FFloatProperty>(BP.GeneratedClass.Get(), TEXT("CatalogHeight"));
    if (!Height) return false;
    // Leave room for the full 218-unit cards after the wallet, filters and explicit selection row.
    // At the project 720p DPI the old 573-unit height clipped the card price by 10.7 pixels.
    Height->SetPropertyValue_InContainer(BP.GeneratedClass->GetDefaultObject(), 600.f);
    Metadata.SetValue(&BP, Key, TEXT("1"));
    if (SaveAssetPackage(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, Key);
    return false;
}

bool RefinePlacementTextWrapping(UWidgetBlueprint& BP)
{
    constexpr const TCHAR* Key = TEXT("FFFacilityAuroraBuildTextWrappingV1");
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, Key) == TEXT("1")) return true;
    auto* Status = Cast<UTextBlock>(BP.WidgetTree->FindWidget(TEXT("Text_Legend")));
    auto* RotationHint = Cast<UTextBlock>(BP.WidgetTree->FindWidget(TEXT("Text_RotationHint")));
    auto* ControlHelp = Cast<UTextBlock>(BP.WidgetTree->FindWidget(TEXT("Text_ControlHelp")));
    if (!Status || !RotationHint || !ControlHelp) return false;
    // These panels have 418/420 units inside their borders. A fixed wrapping width also
    // makes the first layout pass correct, before automatic wrapping has cached its width.
    for (UTextBlock* Label : { Status, RotationHint, ControlHelp })
    {
        FSlateFontInfo Font = Label->GetFont();
        Font.Size = 16;
        Label->SetFont(Font);
        Label->SetAutoWrapText(true);
        Label->SetWrapTextAt(410.f);
        Label->SetTextOverflowPolicy(ETextOverflowPolicy::Clip);
    }
    // Recover space for the wrapped placement status above the independently anchored
    // help panel. Preserve the panel bounds, brushes and all existing action bindings.
    for (const TPair<FName, float>& Row : {
        TPair<FName, float>(TEXT("Text_Mode"), 8.f),
        TPair<FName, float>(TEXT("Text_Legend"), 8.f),
        TPair<FName, float>(TEXT("Border_PlacementGold"), 8.f),
        TPair<FName, float>(TEXT("Text_Balance"), 10.f),
        TPair<FName, float>(TEXT("Text_Footprint"), 10.f),
        TPair<FName, float>(TEXT("Label_Queue"), 6.f),
        TPair<FName, float>(TEXT("Border_Queue"), 6.f) })
    {
        UWidget* Widget = BP.WidgetTree->FindWidget(Row.Key);
        auto* Slot = Widget ? Cast<UVerticalBoxSlot>(Widget->Slot) : nullptr;
        if (!Slot) return false;
        FMargin Padding = Slot->GetPadding();
        Padding.Bottom = Row.Value;
        Slot->SetPadding(Padding);
    }
    Metadata.SetValue(&BP, Key, TEXT("1"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, Key);
    return false;
}

void Apply()
{
    auto* Catalog = LoadExistingBlueprint(PanelPackage);
    auto* Tile = LoadExistingBlueprint(TilePackage);
    auto* Tools = LoadExistingBlueprint(ToolsPackage);
    if (!Catalog || !Tile || !Tools || !Catalog->WidgetTree || !Tile->WidgetTree || !Tools->WidgetTree ||
        Catalog->ParentClass != UFF_PrepPanelBuild::StaticClass() || Tile->ParentClass != UFF_PrepBuildTileWidget::StaticClass() ||
        Tools->ParentClass != UFF_BuildToolsWidget::StaticClass())
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: expected native parents and saved build WBP trees are required"), Prefix);
        return;
    }
    constexpr const TCHAR* CompletionKey = TEXT("FFFacilityAuroraBuildV7Completed");
    auto& Metadata = Catalog->GetOutermost()->GetMetaData();
    if (Metadata.GetValue(Catalog, CompletionKey) == TEXT("1"))
    {
        if (!RefineRotationLabels(*Tools) || !RefineCatalogHeight(*Catalog) ||
            !RefinePlacementTextWrapping(*Tools)) return;
        UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: V7 layout already completed; preserving Designer changes"), Prefix);
        return;
    }
    if (!MigrateTile(*Tile) || !MigrateCatalog(*Catalog))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: catalog migration failed"), Prefix);
        return;
    }
    if (!Tools->WidgetTree->FindWidget(TEXT("Canvas_AuroraBuildV7")))
    {
        Tools = ReplaceWidgetBlueprint(Prefix, ToolsPackage, FFBuildUIAssetPaths::ControlsFolder,
            *UFF_BuildToolsWidget::StaticClass(), BuildTools);
        if (!Tools) return;
    }
    if (!RefineRotationLabels(*Tools) || !RefinePlacementTextWrapping(*Tools)) return;
    if (!RefineCatalogHeight(*Catalog)) return;
    // Persist completion with the final CDO height, only after all three asset migrations succeed.
    // A rerun must neither reset subsequent Designer height edits nor reserialize unchanged assets.
    Metadata.SetValue(Catalog, CompletionKey, TEXT("1"));
    if (!SaveAssetPackage(Prefix, *Catalog))
    {
        Metadata.RemoveValue(Catalog, CompletionKey);
        return;
    }
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_DONE: selected-placement catalog, Gold emphasis and nonmodal placement panels saved"), Prefix);
}
FAutoConsoleCommand Command(TEXT("FF.ApplyFacilityAuroraBuild"),
    TEXT("Apply approved green Aurora V7 catalog and placement controls once; preserve existing migrated Designer edits."),
    FConsoleCommandDelegate::CreateStatic(&Apply));
}
#undef LOCTEXT_NAMESPACE
#endif
