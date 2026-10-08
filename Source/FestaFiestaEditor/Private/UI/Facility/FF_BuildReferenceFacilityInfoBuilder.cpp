// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// FF_BuildReferenceFacilityInfoBuilder.cpp
// Editor-only visual pass for the authored build-mode facility inspector.

#if WITH_EDITOR

#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_BuildPearlStyle.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

namespace FFBuildReferenceFacilityInfo
{
using namespace FFUIWbpBuilder;

constexpr const TCHAR* Version = TEXT("FF.BuildReference.FacilityInfo.20260923.v4");
constexpr const TCHAR* PearlVersion = TEXT("FF.BuildReference.FacilityInfo.PearlV6");
constexpr const TCHAR* Prefix = TEXT("FF_BUILD_REFERENCE_FACILITY_INFO");

static FLinearColor Color(const uint8 R, const uint8 G, const uint8 B)
{
    return FLinearColor::FromSRGBColor(FColor(R, G, B));
}

const FLinearColor Paper = Color(0xF7, 0xF8, 0xF6);
const FLinearColor PaperSunken = Color(0xEE, 0xF2, 0xEF);
const FLinearColor Teal = Color(0x52, 0x7C, 0x6B);
const FLinearColor TealDeep = Color(0x31, 0x57, 0x47);
const FLinearColor TealSoft = Color(0xD7, 0xE7, 0xDE);
const FLinearColor TealHero = Color(0x3E, 0x70, 0x60);
const FLinearColor Ink = Color(0x24, 0x33, 0x2E);
const FLinearColor InkMuted = Color(0x6F, 0x7B, 0x75);
const FLinearColor Line = Color(0xCC, 0xD5, 0xCF);
const FLinearColor Danger = Color(0xD7, 0x4E, 0x43);

static FSlateBrush Surface(const FLinearColor& Fill, const float Radius,
    const FLinearColor& Edge = FLinearColor::Transparent, const float Stroke = 0.0f)
{
    FSlateBrush Brush;
    Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
    Brush.TintColor = FSlateColor(Fill);
    Brush.OutlineSettings = FSlateBrushOutlineSettings(Radius, FSlateColor(Edge), Stroke);
    return Brush;
}

template <typename T>
static T* Find(UWidgetTree& Tree, const TCHAR* Name)
{
    return Cast<T>(Tree.FindWidget(Name));
}

static void Paint(UBorder* Border, const FSlateBrush& Brush)
{
    if (!Border) return;
    Border->Modify();
    Border->SetBrush(Brush);
    Border->SetBrushColor(FLinearColor::White);
}

static void Text(UCommonTextBlock* Label, const int32 Size, const FLinearColor& Foreground,
    const bool bStrong = false)
{
    if (!Label) return;
    Label->Modify();
    // CommonTextBlock otherwise reapplies its old style during SynchronizeProperties.
    Label->SetStyle(nullptr);
    StyleText(*Label, Size, Foreground, bStrong ? TEXT("SemiBold") : TEXT("Regular"));
    Label->SetLineHeightPercentage(1.0f);
}

static void Button(UButton* Control, const FSlateBrush& Normal,
    const FSlateBrush& Hovered, const FSlateBrush& Pressed,
    const FSlateBrush& Disabled, const FLinearColor& LabelColor)
{
    if (!Control) return;
    Control->Modify();
    FButtonStyle Style = Control->GetStyle();
    Style.Normal = Normal;
    Style.Hovered = Hovered;
    Style.Pressed = Pressed;
    Style.Disabled = Disabled;
    Control->SetStyle(Style);
    if (auto* Label = Cast<UCommonTextBlock>(Control->GetContent()))
    {
        Text(Label, 14, LabelColor, true);
        Label->SetAutoWrapText(false);
        Label->SetJustification(ETextJustify::Center);
    }
}

static bool HasAuthoredClarityContract(UWidgetBlueprint& Blueprint)
{
    UWidgetTree* Tree = Blueprint.WidgetTree;
    return Blueprint.ParentClass && Blueprint.ParentClass->IsChildOf(UFF_FacilityInfoPanel::StaticClass()) &&
        Tree && Tree->FindWidget(TEXT("Clarity_InfoV1")) &&
        Find<UBorder>(*Tree, TEXT("Border_Panel")) &&
        Find<UBorder>(*Tree, TEXT("Header_Surface")) &&
        Find<UBorder>(*Tree, TEXT("Panel_PersistentIdentity")) &&
        Find<UHorizontalBox>(*Tree, TEXT("V2_IdentityRow")) &&
        Find<UVerticalBox>(*Tree, TEXT("V2_IdentityMeta")) &&
        Find<UVerticalBox>(*Tree, TEXT("Clarity_BasicFacts")) &&
        Find<UHorizontalBox>(*Tree, TEXT("Clarity_Row_Text_QueueCapacityValue")) &&
        Find<UHorizontalBox>(*Tree, TEXT("Clarity_Row_Text_ServiceCapacityValue")) &&
        Find<UHorizontalBox>(*Tree, TEXT("Clarity_Row_Text_InstanceLimit")) &&
        Find<UBorder>(*Tree, TEXT("Body_Surface")) &&
        Find<UBorder>(*Tree, TEXT("Panel_BuildSection")) &&
        Find<UBorder>(*Tree, TEXT("Panel_LiveSection")) &&
        Find<UBorder>(*Tree, TEXT("Clarity_RevenueCard")) &&
        Find<UCommonTextBlock>(*Tree, TEXT("V2_InspectorTitle")) &&
        Find<UCommonTextBlock>(*Tree, TEXT("Text_FacilityName")) &&
        Find<UCommonTextBlock>(*Tree, TEXT("Clarity_Label_Text_QueueCapacityValue")) &&
        Find<UButton>(*Tree, TEXT("Button_Move")) &&
        Find<UButton>(*Tree, TEXT("Button_Remove"));
}

static bool MakeBasicStatPills(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
{
    UVerticalBox* Basics = Find<UVerticalBox>(Tree, TEXT("Clarity_BasicFacts"));
    UHorizontalBox* Queue = Find<UHorizontalBox>(Tree, TEXT("Clarity_Row_Text_QueueCapacityValue"));
    UHorizontalBox* Service = Find<UHorizontalBox>(Tree, TEXT("Clarity_Row_Text_ServiceCapacityValue"));
    UHorizontalBox* Limit = Find<UHorizontalBox>(Tree, TEXT("Clarity_Row_Text_InstanceLimit"));
    if (!Basics || !Queue || !Service || !Limit) return false;

    auto WidenCaption = [&](const TCHAR* CaptionName, const TCHAR* ValueName)
    {
        auto* Caption = Find<UCommonTextBlock>(Tree, CaptionName);
        auto* Value = Find<UCommonTextBlock>(Tree, ValueName);
        auto* CaptionSlot = Caption ? Cast<UHorizontalBoxSlot>(Caption->Slot) : nullptr;
        auto* ValueSlot = Value ? Cast<UHorizontalBoxSlot>(Value->Slot) : nullptr;
        if (!CaptionSlot || !ValueSlot) return false;
        FSlateChildSize CaptionWidth(ESlateSizeRule::Fill);
        CaptionWidth.Value = 1.5f;
        CaptionSlot->SetSize(CaptionWidth);
        ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        return true;
    };

    // v3 already built these wrappers. Reuse them when refining the existing
    // authored WBP, rather than creating duplicate named widgets.
    if (Tree.FindWidget(TEXT("Reference_BasicStatPair")))
        return WidenCaption(TEXT("Clarity_Label_Text_QueueCapacityValue"),
                   TEXT("Text_QueueCapacityValue")) &&
               WidenCaption(TEXT("Clarity_Label_Text_ServiceCapacityValue"),
                   TEXT("Text_ServiceCapacityValue"));

    auto* Pair = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Reference_BasicStatPair"));
    auto* QueuePill = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Reference_QueuePill"));
    auto* ServicePill = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Reference_ServicePill"));
    auto* LimitPill = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Reference_LimitPill"));
    if (!Pair || !QueuePill || !ServicePill || !LimitPill) return false;

    auto AddPill = [&](UBorder& Pill, UHorizontalBox& Row, const bool bEmphasis)
    {
        Paint(&Pill, Surface(bEmphasis ? TealSoft : PaperSunken, 7,
            bEmphasis ? Teal : Line, 1));
        Pill.SetPadding(FMargin(7, 9));
        Row.RemoveFromParent();
        Pill.AddChild(&Row);
    };
    AddPill(*QueuePill, *Queue, false);
    AddPill(*ServicePill, *Service, false);
    AddPill(*LimitPill, *Limit, true);

    if (auto* Slot = Pair->AddChildToHorizontalBox(QueuePill))
    {
        Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        Slot->SetPadding(FMargin(0, 0, 4, 0));
    }
    if (auto* Slot = Pair->AddChildToHorizontalBox(ServicePill))
    {
        Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        Slot->SetPadding(FMargin(4, 0, 0, 0));
    }
    Basics->InsertChildAt(0, Pair);
    Basics->InsertChildAt(1, LimitPill);
    if (auto* Slot = Cast<UVerticalBoxSlot>(Pair->Slot))
        Slot->SetPadding(FMargin(0, 0, 0, 7));
    if (auto* Slot = Cast<UVerticalBoxSlot>(LimitPill->Slot))
        Slot->SetPadding(FMargin(0, 0, 0, 7));
    return WidenCaption(TEXT("Clarity_Label_Text_QueueCapacityValue"),
               TEXT("Text_QueueCapacityValue")) &&
           WidenCaption(TEXT("Clarity_Label_Text_ServiceCapacityValue"),
               TEXT("Text_ServiceCapacityValue"));
}

static bool Apply(UWidgetBlueprint& Blueprint)
{
    if (!HasAuthoredClarityContract(Blueprint)) return false;
    UWidgetTree& Tree = *Blueprint.WidgetTree;
    UPackage* Package = Blueprint.GetOutermost();
    if (Package->GetMetaData().GetValue(&Blueprint, Version) == TEXT("1")) return true;

    Blueprint.Modify();
    Tree.Modify();

    // The compact title and real facility identity form one continuous hero.
    // The existing BindWidget objects, buttons, and scrolling body stay intact.
    Paint(Find<UBorder>(Tree, TEXT("Border_Panel")), Surface(Paper, 12, Line, 1));
    Paint(Find<UBorder>(Tree, TEXT("Header_Surface")), Surface(TealDeep, 10));
    Paint(Find<UBorder>(Tree, TEXT("Panel_PersistentIdentity")), Surface(TealHero, 0));
    Paint(Find<UBorder>(Tree, TEXT("Body_Surface")), Surface(Paper, 8));
    Paint(Find<UBorder>(Tree, TEXT("Thumbnail_Background")),
        Surface(Color(0x5B, 0x8E, 0x78), 8));
    Paint(Find<UBorder>(Tree, TEXT("Category_Chip")),
        Surface(FLinearColor::Transparent, 0));
    Paint(Find<UBorder>(Tree, TEXT("State_Chip")), Surface(Paper, 5, Line, 1));
    if (auto* Category = Find<UBorder>(Tree, TEXT("Category_Chip")))
        Category->SetPadding(FMargin(0, 2));
    if (auto* Identity = Find<UBorder>(Tree, TEXT("Panel_PersistentIdentity")))
    {
        Identity->SetPadding(FMargin(14, 8, 12, 8));
        if (auto* Slot = Cast<UVerticalBoxSlot>(Identity->Slot))
            Slot->SetPadding(FMargin(0, 0, 0, 5));
    }
    if (auto* Header = Find<USizeBox>(Tree, TEXT("Header_Size")))
        Header->SetHeightOverride(42);
    if (auto* Thumbnail = Find<USizeBox>(Tree, TEXT("Thumbnail_Size")))
    {
        Thumbnail->SetWidthOverride(94);
        Thumbnail->SetHeightOverride(88);
    }
    if (auto* Row = Find<UHorizontalBox>(Tree, TEXT("V2_IdentityRow")))
        if (auto* Meta = Find<UVerticalBox>(Tree, TEXT("V2_IdentityMeta")))
            Row->ShiftChild(0, Meta); // name/type at left, real thumbnail at right
    if (auto* Meta = Find<UVerticalBox>(Tree, TEXT("V2_IdentityMeta")))
        if (auto* Level = Find<UCommonTextBlock>(Tree, TEXT("Text_LevelSummary")))
        {
            Level->RemoveFromParent();
            if (auto* Slot = Meta->AddChildToVerticalBox(Level))
                Slot->SetPadding(FMargin(0, 4, 0, 0));
        }

    // Pale inset stat cards use real live values. Other named rows remain in
    // the same scroll section so all existing inspection data stays reachable.
    for (const TCHAR* Name : {TEXT("Panel_BuildSection"), TEXT("Panel_LiveSection"),
        TEXT("Clarity_RevenueCard"), TEXT("Upgrade_Card")})
    {
        Paint(Find<UBorder>(Tree, Name),
            Surface(FCString::Strcmp(Name, TEXT("Panel_BuildSection")) == 0 ? Paper : PaperSunken,
                8, FCString::Strcmp(Name, TEXT("Panel_BuildSection")) == 0 ?
                FLinearColor::Transparent : Line,
                FCString::Strcmp(Name, TEXT("Panel_BuildSection")) == 0 ? 0 : 1));
    }
    for (const TCHAR* Name : {TEXT("Panel_Actions"), TEXT("Panel_UpgradeActions")})
        Paint(Find<UBorder>(Tree, Name), Surface(Paper, 8));

    Text(Find<UCommonTextBlock>(Tree, TEXT("V2_InspectorTitle")), 19, FLinearColor::White, true);
    Text(Find<UCommonTextBlock>(Tree, TEXT("Close_Label")), 20, FLinearColor::White, true);
    Text(Find<UCommonTextBlock>(Tree, TEXT("Text_FacilityName")), 22, FLinearColor::White, true);
    Text(Find<UCommonTextBlock>(Tree, TEXT("Text_Category")), 13, FLinearColor::White, true);
    Text(Find<UCommonTextBlock>(Tree, TEXT("Text_Zone")), 13,
        Color(0xDA, 0xED, 0xE2));
    Text(Find<UCommonTextBlock>(Tree, TEXT("Text_LevelSummary")), 12,
        Color(0xDA, 0xED, 0xE2));
    Text(Find<UCommonTextBlock>(Tree, TEXT("Text_ThumbnailPlaceholder")), 12,
        FLinearColor::White, true);
    Button(Find<UButton>(Tree, TEXT("Button_Favorite")),
        Surface(TealDeep, 6), Surface(Teal, 6), Surface(TealDeep, 6),
        Surface(TealDeep, 6), FLinearColor::White);
    Text(Find<UCommonTextBlock>(Tree, TEXT("Text_Favorite")), 19,
        FLinearColor::White, true);

    for (const TCHAR* Name : {TEXT("Text_QueueCapacityValue"), TEXT("Text_ServiceCapacityValue"),
        TEXT("Text_ServiceDuration"), TEXT("Text_Footprint"),
        TEXT("Text_InstallCost"), TEXT("Text_RecoveryCost"), TEXT("Text_InstanceLimit"),
        TEXT("Text_Occupancy"), TEXT("Text_WaitingParties"), TEXT("Text_RemainingService"),
        TEXT("Text_ExpectedWait"), TEXT("Text_Throughput"),
        TEXT("Text_FacilityTodayRevenue"), TEXT("Text_FacilityTotalRevenue"),
        TEXT("Text_FacilityRevenueCount"), TEXT("Text_FacilityRevenueShare")})
    {
        Text(Find<UCommonTextBlock>(Tree, Name),
            FCString::Strcmp(Name, TEXT("Text_QueueCapacityValue")) == 0 ||
            FCString::Strcmp(Name, TEXT("Text_Occupancy")) == 0 ||
            FCString::Strcmp(Name, TEXT("Text_FacilityTodayRevenue")) == 0 ? 18 : 14,
            Ink, true);
        const FString CaptionName = FString(TEXT("Clarity_Label_")) + Name;
        Text(Find<UCommonTextBlock>(Tree, *CaptionName), 13, InkMuted);
        const FString RowName = FString(TEXT("Clarity_Row_")) + Name;
        if (auto* Row = Find<UHorizontalBox>(Tree, *RowName))
        {
            Row->Modify();
            if (auto* Slot = Cast<UVerticalBoxSlot>(Row->Slot))
            {
                Slot->Modify();
                Slot->SetPadding(FMargin(2, 3, 2, 3));
            }
            // Caption/value split from Clarity remains intact at 34/66 percent.
        }
    }

    if (!MakeBasicStatPills(Blueprint, Tree)) return false;

    const FSlateBrush Primary = Surface(TealDeep, 6);
    const FSlateBrush PrimaryHover = Surface(Teal, 6);
    const FSlateBrush Secondary = Surface(PaperSunken, 6, Line, 1);
    const FSlateBrush SecondaryHover = Surface(TealSoft, 6, Teal, 1);
    const FSlateBrush Destructive = Surface(Paper, 6, Danger, 1);
    const FSlateBrush DestructiveHover = Surface(Color(0xFA, 0xE9, 0xE7), 6, Danger, 1);

    for (const TCHAR* Name : {TEXT("Button_Move"), TEXT("Button_Upgrade"),
        TEXT("Button_ScheduleProgram")})
        Button(Find<UButton>(Tree, Name), Primary, PrimaryHover, Primary, Secondary,
            FLinearColor::White);
    for (const TCHAR* Name : {TEXT("Button_RevenueRanking"), TEXT("Button_RemoveCancel")})
        Button(Find<UButton>(Tree, Name), Secondary, SecondaryHover, Primary, Secondary, Ink);
    for (const TCHAR* Name : {TEXT("Button_Remove"), TEXT("Button_RemoveConfirm")})
        Button(Find<UButton>(Tree, Name), Destructive, DestructiveHover, DestructiveHover,
            Secondary, Danger);
    Button(Find<UButton>(Tree, TEXT("Button_Close")), Surface(TealDeep, 6),
        Surface(Teal, 6), Surface(Teal, 6), Surface(TealDeep, 6), FLinearColor::White);
    if (auto* Actions = Find<UBorder>(Tree, TEXT("Panel_Actions")))
        Actions->SetPadding(FMargin(10, 10, 10, 10));
    if (auto* Move = Find<UButton>(Tree, TEXT("Button_Move")))
        if (auto* Slot = Cast<UHorizontalBoxSlot>(Move->Slot))
        {
            FSlateChildSize Weight(ESlateSizeRule::Fill);
            Weight.Value = 2.2f;
            Slot->SetSize(Weight);
        }
    if (auto* Remove = Find<UButton>(Tree, TEXT("Button_Remove")))
        if (auto* Slot = Cast<UHorizontalBoxSlot>(Remove->Slot))
            Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    // The native inspector caches authored tab brushes and uses Disabled as the
    // selected brush. Keep that contract while making each state readable.
    for (const TCHAR* Name : {TEXT("Button_TabOverview"), TEXT("Button_TabSynergy"),
        TEXT("Button_TabOperation"), TEXT("Button_TabUpgrade"), TEXT("Button_TabRevenue"),
        TEXT("Button_TabPrograms"), TEXT("Button_TabLighting")})
    {
        UButton* Tab = Find<UButton>(Tree, Name);
        if (!Tab) continue;
        Tab->Modify();
        FButtonStyle Style = Tab->GetStyle();
        Style.Normal = Secondary;
        Style.Hovered = SecondaryHover;
        Style.Pressed = PrimaryHover;
        Style.Disabled = Primary;
        Tab->SetStyle(Style);
        if (auto* Label = Cast<UCommonTextBlock>(Tab->GetContent()))
            Text(Label, 13, TealDeep, true);
    }

    Package->GetMetaData().SetValue(&Blueprint, Version, TEXT("1"));
    if (SaveNewBlueprint(Prefix, Blueprint)) return true;
    Package->GetMetaData().RemoveValue(&Blueprint, Version);
    return false;
}

static bool RefinePearl(UWidgetBlueprint& Blueprint)
{
    UWidgetTree& Tree = *Blueprint.WidgetTree;
    UPackage* Package = Blueprint.GetOutermost();
    if (Package->GetMetaData().GetValue(&Blueprint, PearlVersion) == TEXT("1")) return true;
    if (!HasAuthoredClarityContract(Blueprint) ||
        !Find<UBorder>(Tree, TEXT("Reference_QueuePill")) ||
        !Find<UBorder>(Tree, TEXT("Reference_ServicePill")) ||
        !Find<UBorder>(Tree, TEXT("Reference_LimitPill"))) return false;
    Blueprint.Modify();
    Tree.Modify();

    // The same soft material language as the build toolbar, preserving the
    // inspector's compact right-side layout and all live facility bindings.
    Paint(Find<UBorder>(Tree, TEXT("Border_Panel")), FFBuildPearl::MaterialBrush(TEXT("Panel")));
    Paint(Find<UBorder>(Tree, TEXT("Header_Surface")), FFBuildPearl::MaterialBrush(TEXT("Hero")));
    Paint(Find<UBorder>(Tree, TEXT("Panel_PersistentIdentity")),
        FFBuildPearl::Rounded(FFBuildPearl::Color(73, 129, 198, .96f), 14.f));
    Paint(Find<UBorder>(Tree, TEXT("Body_Surface")),
        FFBuildPearl::Rounded(FFBuildPearl::Color(251, 251, 255, .96f), 18.f));
    Paint(Find<UBorder>(Tree, TEXT("Thumbnail_Background")),
        FFBuildPearl::Rounded(FFBuildPearl::Color(220, 238, 253, .7f), 18.f,
            FFBuildPearl::Color(246, 251, 255, .85f), 1.f));
    Paint(Find<UBorder>(Tree, TEXT("State_Chip")),
        FFBuildPearl::Rounded(FFBuildPearl::Color(239, 247, 255, .95f), 16.f,
            FFBuildPearl::Line, 1.f));
    Paint(Find<UBorder>(Tree, TEXT("Category_Chip")),
        FFBuildPearl::Rounded(FLinearColor::Transparent, 0.f));
    for (const TCHAR* Name : {TEXT("Reference_QueuePill"), TEXT("Reference_ServicePill"),
        TEXT("Reference_LimitPill")})
        if (auto* Pill = Find<UBorder>(Tree, Name))
        {
            Paint(Pill, FFBuildPearl::Rounded(FFBuildPearl::Color(231, 243, 253, .96f), 15.f,
                FFBuildPearl::Color(190, 222, 245, .8f), 1.f));
            Pill->SetPadding(FMargin(8, 6));
        }
    for (const TCHAR* Name : {TEXT("Panel_BuildSection"), TEXT("Panel_LiveSection"),
        TEXT("Clarity_RevenueCard"), TEXT("Upgrade_Card")})
        Paint(Find<UBorder>(Tree, Name),
            FFBuildPearl::Rounded(FFBuildPearl::Color(246, 248, 255, .9f), 16.f,
                FFBuildPearl::Line, 1.f));
    for (const TCHAR* Name : {TEXT("Panel_Actions"), TEXT("Panel_UpgradeActions")})
        Paint(Find<UBorder>(Tree, Name),
            FFBuildPearl::Rounded(FFBuildPearl::Color(250, 250, 255, .92f), 16.f));

    const FSlateBrush Primary = FFBuildPearl::Rounded(FFBuildPearl::Charcoal, 22.f,
        FFBuildPearl::Color(140, 215, 247), 1.f);
    const FSlateBrush PrimaryHover = FFBuildPearl::Rounded(FFBuildPearl::Color(83, 82, 112),
        22.f, FFBuildPearl::Cyan, 1.f);
    const FSlateBrush Secondary = FFBuildPearl::Rounded(FFBuildPearl::Color(241, 244, 253),
        19.f, FFBuildPearl::Line, 1.f);
    const FSlateBrush SecondaryHover = FFBuildPearl::Rounded(FFBuildPearl::Color(222, 239, 253),
        19.f, FFBuildPearl::Cyan, 1.f);
    const FSlateBrush DangerButton = FFBuildPearl::Rounded(FFBuildPearl::Color(251, 245, 249),
        19.f, FFBuildPearl::Color(220, 163, 183), 1.f);
    const FSlateBrush DangerHover = FFBuildPearl::Rounded(FFBuildPearl::Color(250, 226, 235),
        19.f, FFBuildPearl::Color(205, 122, 151), 1.f);
    for (const TCHAR* Name : {TEXT("Button_Move"), TEXT("Button_Upgrade"),
        TEXT("Button_ScheduleProgram")})
        Button(Find<UButton>(Tree, Name), Primary, PrimaryHover, PrimaryHover, Secondary,
            FLinearColor::White);
    for (const TCHAR* Name : {TEXT("Button_RevenueRanking"), TEXT("Button_RemoveCancel")})
        Button(Find<UButton>(Tree, Name), Secondary, SecondaryHover, PrimaryHover, Secondary,
            FFBuildPearl::Ink);
    for (const TCHAR* Name : {TEXT("Button_Remove"), TEXT("Button_RemoveConfirm")})
        Button(Find<UButton>(Tree, Name), DangerButton, DangerHover, DangerHover, Secondary,
            FFBuildPearl::Color(165, 69, 100));
    Button(Find<UButton>(Tree, TEXT("Button_Close")), Primary, PrimaryHover, PrimaryHover,
        Primary, FLinearColor::White);
    Button(Find<UButton>(Tree, TEXT("Button_Favorite")), Primary, PrimaryHover, PrimaryHover,
        Primary, FLinearColor::White);
    for (const TCHAR* Name : {TEXT("Button_TabOverview"), TEXT("Button_TabSynergy"),
        TEXT("Button_TabOperation"), TEXT("Button_TabUpgrade"), TEXT("Button_TabRevenue"),
        TEXT("Button_TabPrograms"), TEXT("Button_TabLighting")})
    {
        UButton* Tab = Find<UButton>(Tree, Name);
        if (!Tab) continue;
        FButtonStyle Style = Tab->GetStyle();
        Style.Normal = Secondary;
        Style.Hovered = SecondaryHover;
        Style.Pressed = PrimaryHover;
        Style.Disabled = Primary; // Native selected-tab state samples Disabled.
        Tab->SetStyle(Style);
        if (auto* Caption = Cast<UCommonTextBlock>(Tab->GetContent()))
            Text(Caption, 13, FFBuildPearl::Ink, true);
    }
    for (const TCHAR* Name : {TEXT("Text_QueueCapacityValue"), TEXT("Text_FacilityTodayRevenue")})
        Text(Find<UCommonTextBlock>(Tree, Name), 18, FFBuildPearl::Ink, true);
    for (const TCHAR* Name : {TEXT("Text_ServiceCapacityValue"), TEXT("Text_InstanceLimit")})
        Text(Find<UCommonTextBlock>(Tree, Name), 14, FFBuildPearl::Ink, true);

    Package->GetMetaData().SetValue(&Blueprint, PearlVersion, TEXT("1"));
    if (SaveNewBlueprint(TEXT("FF_BUILD_REFERENCE_FACILITY_INFO_PEARL_V5"), Blueprint)) return true;
    Package->GetMetaData().RemoveValue(&Blueprint, PearlVersion);
    return false;
}

static void Run()
{
    UWidgetBlueprint* Blueprint = LoadExistingBlueprint(FFBuildUIAssetPaths::FacilityInfoPanelPackage);
    const bool bSuccess = Blueprint && Apply(*Blueprint) && RefinePearl(*Blueprint);
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_%s"), Prefix,
        bSuccess ? TEXT("PASS") : TEXT("FAIL"));
}

static FAutoConsoleCommand Command(
    TEXT("FF.ApplyBuildReferenceFacilityInfo"),
    TEXT("Style the facility hero, stat pills, and action rail in the pearl build theme; preserve bindings and behavior."),
    FConsoleCommandDelegate::CreateStatic(&Run));
}

#endif // WITH_EDITOR
