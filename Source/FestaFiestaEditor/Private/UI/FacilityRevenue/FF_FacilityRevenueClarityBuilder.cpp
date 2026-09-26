// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// FF_FacilityRevenueClarityBuilder.cpp
// 역할: 시설 수입 화면의 가독성과 저장된 스타일을 조정한다.

// Explicit refinement of saved revenue widgets. Gameplay data and bindings are retained.
#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Facility/FF_FacilityProgramMarkerWidget.h"
#include "UI/FacilityRevenue/FF_FacilityRevenueRanking.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace FFFacilityRevenueClarity
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Prefix = TEXT("FF_BUILD_CLARITY_REVENUE");
constexpr const TCHAR* Version = TEXT("FF.BuildClarity.Revenue20260921.1");
constexpr const TCHAR* FrameFitVersion = TEXT("FF.BuildClarity.Revenue20260921.2");
static FLinearColor C(uint8 R, uint8 G, uint8 B) { return FLinearColor::FromSRGBColor(FColor(R,G,B)); }
static const FLinearColor Ink = C(32,76,54), Muted = C(98,119,106), Paper = C(245,248,241),
    White = C(252,254,249), Sage = C(228,237,223), Line = C(205,217,200), Forest = C(42,87,65);

static FSlateBrush Round(FLinearColor Fill, float Radius = 8.f)
{
    FSlateBrush B; B.DrawAs = ESlateBrushDrawType::RoundedBox; B.TintColor = Fill;
    B.OutlineSettings = FSlateBrushOutlineSettings(Radius, FSlateColor(Line), 1.f);
    return B;
}
static FSlateBrush Material(const TCHAR* Role, FLinearColor Fallback)
{
    const FString Name = FString(TEXT("MI_UI_BuildClarity_")) + Role;
    const FString Path = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Controls/Clarity/") + Name + TEXT(".") + Name;
    FSlateBrush B = Round(Fallback, 12);
    if (auto* M = LoadObject<UMaterialInterface>(nullptr, *Path))
    { B = FSlateBrush(); B.DrawAs = ESlateBrushDrawType::Image; B.SetResourceObject(M); B.ImageSize = FVector2D(128,48); }
    return B;
}
template<typename T> static T* Get(UWidgetTree& TTree, const TCHAR* Name) { return Cast<T>(TTree.FindWidget(FName(Name))); }
static void Font(UWidgetTree& Tree, const TCHAR* Name, int32 Size, bool bMuted = false)
{
    if (auto* W = Get<UTextBlock>(Tree, Name))
    { auto F = W->GetFont(); F.Size = Size; W->SetFont(F); W->SetColorAndOpacity(FSlateColor(bMuted ? Muted : Ink)); }
}
static void Surface(UWidgetTree& Tree, const TCHAR* Name, FSlateBrush Brush, FMargin Padding)
{
    if (auto* W = Get<UBorder>(Tree, Name)) { W->SetBrush(Brush); W->SetBrushColor(FLinearColor::White); W->SetPadding(Padding); }
}
static void VSpace(UWidget* W, FMargin Padding, bool bFill = false)
{
    if (auto* S = W ? Cast<UVerticalBoxSlot>(W->Slot) : nullptr)
    { S->SetPadding(Padding); S->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic)); }
}
static void HSpace(UWidget* W, FMargin Padding)
{ if (auto* S = W ? Cast<UHorizontalBoxSlot>(W->Slot) : nullptr) S->SetPadding(Padding); }

// Wrap the saved button, retaining its parent order and slot sizing. The text remains
// its direct content, so native selected-period color updates still address the label.
static bool Button40(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, bool bPrimary = false)
{
    auto* Button = Get<UButton>(Tree, Name);
    if (!Button) return false;
    const FName BoundsName(FString(TEXT("Size_Clarity_")) + Name);
    if (!Tree.FindWidget(BoundsName))
    {
        UPanelWidget* Parent = Button->GetParent();
        const int32 Index = Parent ? Parent->GetChildIndex(Button) : INDEX_NONE;
        auto* HS = Cast<UHorizontalBoxSlot>(Button->Slot);
        auto* VS = Cast<UVerticalBoxSlot>(Button->Slot);
        if (!HS && !VS) return false;
        const FMargin Padding = HS ? HS->GetPadding() : VS->GetPadding();
        const FSlateChildSize Size = HS ? HS->GetSize() : VS->GetSize();
        auto* Bounds = MakeWidget<USizeBox>(BP, Tree, BoundsName);
        Bounds->SetHeightOverride(40);
        Button->RemoveFromParent();
        if (auto* H = Cast<UHorizontalBox>(Parent))
        { auto* S = H->AddChildToHorizontalBox(Bounds); S->SetPadding(Padding); S->SetSize(Size); S->SetVerticalAlignment(VAlign_Center); }
        else if (auto* V = Cast<UVerticalBox>(Parent))
        { auto* S = V->AddChildToVerticalBox(Bounds); S->SetPadding(Padding); S->SetSize(Size); }
        Parent->ShiftChild(Index, Bounds); Bounds->AddChild(Button);
    }
    FButtonStyle Style = Button->GetStyle();
    Style.Normal = bPrimary ? Material(TEXT("Action"), Forest) : Round(White, 6);
    Style.Hovered = Round(bPrimary ? C(53,104,78) : Sage, 6);
    Style.Pressed = Round(Forest, 6); Style.Disabled = Round(bPrimary ? Sage : Forest, 6);
    Style.NormalPadding = Style.PressedPadding = FMargin(12, 6);
    Button->SetStyle(Style);
    if (auto* Label = Cast<UTextBlock>(Button->GetContent()))
    { auto F = Label->GetFont(); F.Size = 14; Label->SetFont(F); Label->SetAutoWrapText(false); Label->SetColorAndOpacity(FSlateColor(bPrimary ? White : Ink)); }
    return true;
}

static bool Ranking(UWidgetBlueprint& BP, UWidgetTree& Tree)
{
    auto* Canvas = Get<UCanvasPanel>(Tree, TEXT("Canvas_Root"));
    auto* Body = Get<UBorder>(Tree, TEXT("Border_Panel"));
    auto* Totals = Get<UVerticalBox>(Tree, TEXT("Column_Total"));
    auto* Label = Get<UTextBlock>(Tree, TEXT("Text_TotalLabel"));
    auto* Amount = Get<UTextBlock>(Tree, TEXT("Text_Total"));
    if (!Canvas || !Body || !Totals || !Label || !Amount || !Tree.FindWidget(TEXT("Overlay_AuroraRevenueList"))) return false;
    auto* Frame = Get<USizeBox>(Tree, TEXT("Size_ClarityFrame"));
    if (!Frame)
    {
        Body->RemoveFromParent();
        Frame = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_ClarityFrame"), true);
        auto* Slot = Canvas->AddChildToCanvas(Frame);
        Slot->SetAnchors(FAnchors(1,0)); Slot->SetAlignment(FVector2D(1,0));
        Slot->SetOffsets(FMargin(-24,96,0,0)); Slot->SetAutoSize(true);
        Frame->AddChild(Body);
    }
    Frame->SetWidthOverride(500); Frame->SetHeightOverride(600);
    Surface(Tree, TEXT("Border_Panel"), Material(TEXT("Paper"), Paper), FMargin(16));
    Surface(Tree, TEXT("Border_Total"), Round(Sage, 8), FMargin(12,10));
    Font(Tree,TEXT("Text_Title"),20); Font(Tree,TEXT("Text_TotalLabel"),12,true);
    Font(Tree,TEXT("Text_Total"),24); Font(Tree,TEXT("Text_Count"),12,true); Font(Tree,TEXT("Text_Empty"),14,true);
    Label->SetText(FText::FromString(TEXT("매출 합계"))); Label->SetAutoWrapText(false);
    Amount->SetAutoWrapText(false); Amount->SetJustification(ETextJustify::Right);
    auto* TotalRow = Get<UHorizontalBox>(Tree, TEXT("Row_ClarityTotal"));
    if (!TotalRow)
    {
        TotalRow = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_ClarityTotal"));
        Label->RemoveFromParent(); Amount->RemoveFromParent();
        auto* LS = TotalRow->AddChildToHorizontalBox(Label); LS->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); LS->SetVerticalAlignment(VAlign_Center);
        TotalRow->AddChildToHorizontalBox(Amount)->SetVerticalAlignment(VAlign_Center);
        Totals->AddChildToVerticalBox(TotalRow); Totals->ShiftChild(0, TotalRow);
    }
    VSpace(Tree.FindWidget(TEXT("Text_Count")), FMargin(0,4,0,0));
    VSpace(Tree.FindWidget(TEXT("Row_Header")), FMargin(0,0,0,10));
    VSpace(Tree.FindWidget(TEXT("Row_Period")), FMargin(0,0,0,10));
    VSpace(Tree.FindWidget(TEXT("Border_Total")), FMargin(0,0,0,12));
    VSpace(Tree.FindWidget(TEXT("Overlay_AuroraRevenueList")), FMargin(0), true);
    // The selected row already names the facility and gives its rank/share. Repeating
    // that whole card underneath created most of the original panel's wasted height.
    for (const TCHAR* Name : {TEXT("Border_Selected"),TEXT("Text_Order"),TEXT("Text_Footnote"),TEXT("Text_FirstRevenueTitle")})
        if (UWidget* W = Tree.FindWidget(FName(Name))) W->SetVisibility(ESlateVisibility::Collapsed);
    Surface(Tree,TEXT("Border_FirstRevenue"),Round(Sage,6),FMargin(10,8));
    Font(Tree,TEXT("Text_FirstRevenueDescription"),12,true);
    if (auto* Hint = Get<UTextBlock>(Tree,TEXT("Text_FirstRevenueDescription")))
    { Hint->SetText(FText::FromString(TEXT("운영이 시작되면 매출이 여기에 표시됩니다."))); Hint->SetAutoWrapText(false); VSpace(Hint,FMargin(0)); }
    VSpace(Tree.FindWidget(TEXT("Border_FirstRevenue")),FMargin(0,8,0,0));
    for (const TCHAR* Name : {TEXT("Button_Close"),TEXT("Button_Today"),TEXT("Button_Total")})
        if (!Button40(BP,Tree,Name)) return false;
    if (!Button40(BP,Tree,TEXT("Button_Inspect"),true)) return false;
    VSpace(Tree.FindWidget(TEXT("Size_Clarity_Button_Inspect")),FMargin(0,12,0,0));
    return true;
}

static bool RankRow(UWidgetBlueprint&, UWidgetTree& Tree)
{
    auto* Bounds = Get<USizeBox>(Tree,TEXT("Size_Row"));
    auto* Thumb = Get<USizeBox>(Tree,TEXT("Size_Thumbnail"));
    auto* Amount = Get<USizeBox>(Tree,TEXT("Size_AuroraRevenueAmount"));
    if (!Bounds || !Thumb || !Amount) return false;
    Bounds->SetHeightOverride(88); Thumb->SetWidthOverride(52); Thumb->SetHeightOverride(52); Amount->SetWidthOverride(108);
    if (auto* Rank = Get<USizeBox>(Tree,TEXT("Size_Rank"))) Rank->SetWidthOverride(24);
    Surface(Tree,TEXT("Border_Row"),Round(White,7),FMargin(10,8));
    Surface(Tree,TEXT("Border_Thumbnail"),Round(Sage,6),FMargin(3));
    Font(Tree,TEXT("Text_Rank"),18); Font(Tree,TEXT("Text_Name"),16); Font(Tree,TEXT("Text_Location"),11,true);
    Font(Tree,TEXT("Text_Amount"),17); Font(Tree,TEXT("Text_Share"),11,true); Font(Tree,TEXT("Text_ThumbnailFallback"),12,true);
    for (const TCHAR* Name : {TEXT("Text_Name"),TEXT("Text_Location"),TEXT("Text_Amount"),TEXT("Text_Share")})
        if (auto* T = Get<UTextBlock>(Tree,Name)) { T->SetAutoWrapText(false); T->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis); }
    HSpace(Thumb,FMargin(0,0,10,0)); HSpace(Amount,FMargin(10,0,0,0));
    VSpace(Tree.FindWidget(TEXT("Text_Location")),FMargin(0,3,0,0));
    VSpace(Tree.FindWidget(TEXT("Size_RevenueBar")),FMargin(0,6,0,0));
    VSpace(Tree.FindWidget(TEXT("Text_Share")),FMargin(0,4,0,0));
    return true;
}

static bool Badge(UWidgetBlueprint&, UWidgetTree& Tree)
{
    auto* Bounds = Get<USizeBox>(Tree,TEXT("Size_Badge"));
    auto* Program = Get<USizeBox>(Tree,TEXT("Panel_Program"));
    if (!Bounds || !Program || !Tree.FindWidget(TEXT("Scale_AuroraWorldRevenueAmount"))) return false;
    Bounds->SetWidthOverride(220); Program->SetHeightOverride(32);
    Surface(Tree,TEXT("Border_Badge"),Material(TEXT("Paper"),Paper),FMargin(1));
    for (const TCHAR* Name : {TEXT("Border_Available"),TEXT("Border_Scheduled")})
        Surface(Tree,Name,Material(TEXT("Program"),C(230,226,241)),FMargin(8,5));
    Surface(Tree,TEXT("Panel_Revenue"),Round(Paper,5),FMargin(8,6));
    Font(Tree,TEXT("Text_Available"),13); Font(Tree,TEXT("Text_Scheduled"),13);
    Font(Tree,TEXT("Text_RevenueLabel"),11,true); Font(Tree,TEXT("Text_RevenueValue"),13);
    return true;
}

static bool FrameFit(UWidgetBlueprint&, UWidgetTree& Tree)
{
    auto* Frame = Get<USizeBox>(Tree,TEXT("Size_ClarityFrame"));
    if (!Frame || !Get<UBorder>(Tree,TEXT("Border_Panel"))) return false;
    // Start with the compact loading/empty size. Runtime then reserves the measured
    // fixed controls plus complete rows, including first-revenue help when present.
    Frame->SetWidthOverride(500); Frame->SetHeightOverride(378);
    return true;
}
static bool Apply(const TCHAR* Path, UClass* Expected, bool(*Edit)(UWidgetBlueprint&,UWidgetTree&), const TCHAR* AppliedVersion = Version)
{
    auto* BP = LoadExistingBlueprint(Path);
    if (!BP || !BP->WidgetTree || !BP->ParentClass || !BP->ParentClass->IsChildOf(Expected)) return false;
    auto& Meta = BP->GetOutermost()->GetMetaData();
    if (Meta.GetValue(BP,AppliedVersion) == TEXT("1")) return true;
    BP->Modify(); BP->WidgetTree->Modify();
    if (!Edit(*BP,*BP->WidgetTree)) return false;
    Meta.SetValue(BP,AppliedVersion,TEXT("1"));
    if (!SaveNewBlueprint(Prefix,*BP)) { Meta.RemoveValue(BP,AppliedVersion); return false; }
    return true;
}
static void Run()
{
    bool Ok = Apply(FFBuildUIAssetPaths::FacilityRevenueRankRowPackage,UFF_FacilityRevenueRankRow::StaticClass(),RankRow);
    Ok &= Apply(FFBuildUIAssetPaths::FacilityRevenueRankingPackage,UFF_FacilityRevenueRankingPanel::StaticClass(),Ranking);
    Ok &= Apply(FFBuildUIAssetPaths::FacilityRevenueRankingPackage,UFF_FacilityRevenueRankingPanel::StaticClass(),FrameFit,FrameFitVersion);
    Ok &= Apply(FFBuildUIAssetPaths::FacilityWorldBadgePackage,UFF_FacilityProgramMarkerWidget::StaticClass(),Badge);
    UE_LOG(LogFFUIWbpBuilder,Display,TEXT("%s_COMPLETE_%s"),Prefix,Ok ? TEXT("PASS") : TEXT("FAIL"));
}
static void RunFrameFit()
{
    const bool Ok = Apply(FFBuildUIAssetPaths::FacilityRevenueRankingPackage,
        UFF_FacilityRevenueRankingPanel::StaticClass(),FrameFit,FrameFitVersion);
    UE_LOG(LogFFUIWbpBuilder,Display,TEXT("FF_BUILD_CLARITY_REVENUE_FIT_COMPLETE_%s"),Ok ? TEXT("PASS") : TEXT("FAIL"));
}
static FAutoConsoleCommand Command(TEXT("FF.ApplyBuildModeClarityRevenue"),
    TEXT("Refine existing ranking, rows, and world badges once into compact Clarity surfaces; retain bindings and ranking data."),
    FConsoleCommandDelegate::CreateStatic(&Run));
static FAutoConsoleCommand FitCommand(TEXT("FF.RefineBuildModeClarityRevenueFit"),
    TEXT("Refine only the existing compact ranking frame once for complete sparse rows and a compact initial state."),
    FConsoleCommandDelegate::CreateStatic(&RunFrameFit));
}
#endif
