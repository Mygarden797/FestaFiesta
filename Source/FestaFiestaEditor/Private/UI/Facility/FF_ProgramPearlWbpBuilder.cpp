// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// FF_ProgramPearlWbpBuilder.cpp
// Refine the saved paid-program selection, review and receipt in the construction pearl theme.

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_BuildPearlStyle.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Facility/FF_FacilityPaidProgramCard.h"
#include "UI/Facility/FF_FacilityPaidProgramLayout.h"
#include "UI/Facility/FF_FacilityPaidProgramPicker.h"
#include "UObject/MetaData.h"
#include "WidgetBlueprint.h"

namespace FFProgramPearlWbpBuilder
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Prefix = TEXT("FF_PROGRAM_PEARL");
constexpr const TCHAR* Version = TEXT("FF.ProgramPearl.V4");

template <typename T>
T* Find(UWidgetTree& Tree, const TCHAR* Name)
{
    return Cast<T>(Tree.FindWidget(Name));
}

void PaintText(UWidgetTree& Tree, const TCHAR* Name, const int32 Size,
    const FLinearColor Color, const FName Weight = TEXT("SemiBold"))
{
    if (UTextBlock* Text = Find<UTextBlock>(Tree, Name))
    {
        Text->Modify();
        if (auto* CommonText = Cast<UCommonTextBlock>(Text)) CommonText->SetStyle(nullptr);
        StyleText(*Text, Size, Color, Weight);
        Text->SetLineHeightPercentage(1.f);
        Text->SetWrapTextAt(0.f);
    }
}

void Variable(UWidgetBlueprint& BP, UWidget& Widget)
{
    Widget.bIsVariable = true;
    if (!BP.WidgetVariableNameToGuidMap.Contains(Widget.GetFName()))
        BP.OnVariableAdded(Widget.GetFName());
}

void Vertical(UVerticalBox& Parent, UWidget& Child, const FMargin Padding,
    const ESlateSizeRule::Type Size = ESlateSizeRule::Automatic)
{
    Child.RemoveFromParent();
    if (auto* Slot = Parent.AddChildToVerticalBox(&Child))
    {
        Slot->SetPadding(Padding);
        Slot->SetSize(FSlateChildSize(Size));
        Slot->SetHorizontalAlignment(HAlign_Fill);
        Slot->SetVerticalAlignment(VAlign_Fill);
    }
}

void PaintBorder(UWidgetTree& Tree, const TCHAR* Name, const TCHAR* Role,
    const FMargin Padding)
{
    if (UBorder* Border = Find<UBorder>(Tree, Name))
    {
        Border->Modify();
        Border->SetBrush(FFBuildPearl::MaterialBrush(Role));
        Border->SetBrushColor(FLinearColor::White);
        Border->SetPadding(Padding);
    }
}

void PaintButton(UWidgetTree& Tree, const TCHAR* Name, const bool bPrimary)
{
    UButton* Button = Find<UButton>(Tree, Name);
    if (!Button) return;
    Button->Modify();
    FButtonStyle Style = Button->GetStyle();
    Style.Normal = bPrimary
        ? FFBuildPearl::Rounded(FFBuildPearl::Charcoal, 19.f)
        : FFBuildPearl::Rounded(FFBuildPearl::Color(248, 249, 255, .96f),
            19.f, FFBuildPearl::Line, 1.f);
    Style.Hovered = bPrimary
        ? FFBuildPearl::Rounded(FFBuildPearl::Color(77, 79, 109), 19.f,
            FFBuildPearl::Cyan, 1.f)
        : FFBuildPearl::Rounded(FFBuildPearl::Color(228, 245, 255), 19.f,
            FFBuildPearl::Cyan, 1.f);
    Style.Pressed = bPrimary
        ? FFBuildPearl::Rounded(FFBuildPearl::Color(64, 71, 98), 19.f,
            FFBuildPearl::Cyan, 1.f)
        : FFBuildPearl::Rounded(FFBuildPearl::Cyan, 19.f);
    Style.Disabled = FFBuildPearl::Rounded(bPrimary
        ? FFBuildPearl::Color(117, 119, 143, .78f)
        : FFBuildPearl::Color(238, 239, 246, .75f), 19.f);
    Style.NormalPadding = Style.PressedPadding = FMargin(12.f, 10.f);
    Button->SetStyle(Style);
    Button->SetBackgroundColor(FLinearColor::White);
    if (auto* Content = Button->GetContent())
        if (auto* Slot = Cast<UButtonSlot>(Content->Slot))
        {
            Slot->SetHorizontalAlignment(HAlign_Fill);
            Slot->SetVerticalAlignment(VAlign_Center);
        }
}

bool PaidCard(UWidgetBlueprint& BP, UWidgetTree& Tree)
{
    UButton* Select = Find<UButton>(Tree, TEXT("Button_Select"));
    if (!Select || !Find<UTextBlock>(Tree, TEXT("Text_ProgramName")) ||
        !Find<UTextBlock>(Tree, TEXT("Text_ProgramMeta")) ||
        !Find<UTextBlock>(Tree, TEXT("Text_ProgramAvailability"))) return false;
    auto* Content = Find<UVerticalBox>(Tree, TEXT("Panel_CardContent"));
    if (!Content) return false;
    auto* Cost = Find<UCommonTextBlock>(Tree, TEXT("Text_ProgramCost"));
    if (!Cost) Cost = MakeWidget<UCommonTextBlock>(BP, Tree, TEXT("Text_ProgramCost"), true);
    auto* Footer = Find<UHorizontalBox>(Tree, TEXT("Pearl_ProgramCardFooter"));
    if (!Footer) Footer = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Pearl_ProgramCardFooter"));
    if (!Cost || !Footer) return false;
    Variable(BP, *Cost);
    Cost->SetText(FText::FromString(TEXT("0 Gold")));
    Cost->SetAutoWrapText(false);
    Cost->SetJustification(ETextJustify::Right);
    Cost->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* Availability = Find<UTextBlock>(Tree, TEXT("Text_ProgramAvailability"));
    Availability->RemoveFromParent();
    Footer->ClearChildren();
    if (auto* Slot = Footer->AddChildToHorizontalBox(Availability))
    {
        Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        Slot->SetPadding(FMargin(0, 0, 10, 0));
        Slot->SetVerticalAlignment(VAlign_Center);
    }
    if (auto* Slot = Footer->AddChildToHorizontalBox(Cost))
    {
        Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        Slot->SetHorizontalAlignment(HAlign_Right);
        Slot->SetVerticalAlignment(VAlign_Center);
    }
    // FindWidget walks the attached root. Attach new price widgets before styling by name.
    Vertical(*Content, *Tree.FindWidget(TEXT("Text_ProgramName")), FMargin(2, 0, 2, 5));
    Vertical(*Content, *Tree.FindWidget(TEXT("Text_ProgramMeta")), FMargin(2, 0, 2, 7));
    Vertical(*Content, *Footer, FMargin(2, 0, 2, 0));
    PaintButton(Tree, TEXT("Button_Select"), false);
    PaintText(Tree, TEXT("Text_ProgramName"), 16, FFBuildPearl::Ink, TEXT("Bold"));
    PaintText(Tree, TEXT("Text_ProgramMeta"), 12, FFBuildPearl::Muted, TEXT("Regular"));
    PaintText(Tree, TEXT("Text_ProgramAvailability"), 12, FFBuildPearl::Ink);
    PaintText(Tree, TEXT("Text_ProgramCost"), 15, FFBuildPearl::Color(73, 129, 198), TEXT("Bold"));
    Find<UTextBlock>(Tree, TEXT("Text_ProgramName"))->SetAutoWrapText(true);
    return true;
}

bool PaidPicker(UWidgetBlueprint& BP, UWidgetTree& Tree)
{
    if (!Find<UBorder>(Tree, TEXT("Border_Picker")) ||
        !Find<UButton>(Tree, TEXT("Button_Confirm")) ||
        !Find<UTextBlock>(Tree, TEXT("Text_PreviewName"))) return false;
    auto* Header = Find<UHorizontalBox>(Tree, TEXT("Panel_Header"));
    auto* Body = Find<UVerticalBox>(Tree, TEXT("Panel_PickerContent"));
    auto* Footer = Find<UVerticalBox>(Tree, TEXT("Panel_Footer"));
    auto* ListSize = Find<USizeBox>(Tree, TEXT("Size_ProgramList"));
    auto* Eyebrow = Find<UCommonTextBlock>(Tree, TEXT("Text_Eyebrow"));
    if (!Header || !Body || !Footer || !ListSize || !Eyebrow) return false;
    // Keep existing widget objects and variable GUIDs so the transaction bindings survive.
    Variable(BP, *Eyebrow);
    auto* Back = Find<UButton>(Tree, TEXT("Button_BackToPrograms"));
    if (!Back)
    {
        Back = MakeWidget<UButton>(BP, Tree, TEXT("Button_BackToPrograms"), true);
        auto* Copy = MakeWidget<UCommonTextBlock>(BP, Tree, TEXT("Text_BackToPrograms"));
        if (!Back || !Copy) return false;
        Copy->SetText(FText::FromString(TEXT("‹ 목록")));
        Back->AddChild(Copy);
        Header->InsertChildAt(0, Back);
    }
    for (const TCHAR* Name : {TEXT("Size_ProgramList"), TEXT("Border_Preview"), TEXT("Text_Instruction")})
        if (auto* Widget = Tree.FindWidget(Name)) Variable(BP, *Widget);
    if (auto* Slot = Cast<UCanvasPanelSlot>(Tree.FindWidget(TEXT("Border_Picker"))->Slot))
        FFFacilityPaidProgramLayout::Apply(*Slot, FFFacilityPaidProgramLayout::EVariant::ClarityV2);
    PaintBorder(Tree, TEXT("Border_Picker"), TEXT("Panel"), FMargin(0.f));
    PaintBorder(Tree, TEXT("Border_Header"), TEXT("Hero"), FMargin(16.f, 10.f));
    PaintBorder(Tree, TEXT("Border_Body"), TEXT("Card"), FMargin(16.f, 12.f));
    PaintBorder(Tree, TEXT("Border_ProgramList"), TEXT("Panel"), FMargin(8.f));
    PaintBorder(Tree, TEXT("Border_Preview"), TEXT("Card"), FMargin(14.f, 12.f));
    PaintBorder(Tree, TEXT("Border_Footer"), TEXT("Card"), FMargin(16.f, 12.f));
    PaintText(Tree, TEXT("Text_Eyebrow"), 11, FLinearColor(1.f, 1.f, 1.f, .9f));
    PaintText(Tree, TEXT("Text_Title"), 20, FFBuildPearl::Ink, TEXT("Bold"));
    PaintText(Tree, TEXT("Text_FacilityName"), 15, FFBuildPearl::Ink, TEXT("Bold"));
    PaintText(Tree, TEXT("Text_Instruction"), 12, FFBuildPearl::Muted, TEXT("Regular"));
    PaintText(Tree, TEXT("Text_Empty"), 14, FFBuildPearl::Muted, TEXT("Regular"));
    PaintText(Tree, TEXT("Text_PreviewName"), 20, FFBuildPearl::Ink, TEXT("Bold"));
    PaintText(Tree, TEXT("Text_PreviewTime"), 13, FFBuildPearl::Muted);
    PaintText(Tree, TEXT("Text_PreviewDescription"), 12, FFBuildPearl::Muted, TEXT("Regular"));
    PaintText(Tree, TEXT("Text_PreviewAvailability"), 12, FFBuildPearl::Ink);
    PaintText(Tree, TEXT("Text_PreviewDetails"), 12, FFBuildPearl::Ink, TEXT("Regular"));
    PaintText(Tree, TEXT("Text_Feedback"), 12, FFBuildPearl::Color(215, 78, 67));
    PaintText(Tree, TEXT("Text_Capacity"), 20, FFBuildPearl::Ink, TEXT("Bold"));
    PaintText(Tree, TEXT("Text_TicketFee"), 20, FFBuildPearl::Ink, TEXT("Bold"));
    PaintText(Tree, TEXT("Text_MaxRevenue"), 12, FFBuildPearl::Muted, TEXT("Regular"));
    PaintText(Tree, TEXT("Text_PurchaseCost"), 18, FFBuildPearl::Ink, TEXT("Bold"));
    PaintText(Tree, TEXT("Text_PurchaseCostLabel"), 12, FFBuildPearl::Muted);
    PaintText(Tree, TEXT("Text_ResourcePreview"), 12, FFBuildPearl::Color(73, 129, 198));
    PaintText(Tree, TEXT("Text_NoRefundNotice"), 11, FFBuildPearl::Muted, TEXT("Regular"));
    PaintText(Tree, TEXT("Text_PurchaseCompleteTitle"), 16, FFBuildPearl::Color(73, 129, 198), TEXT("Bold"));
    PaintText(Tree, TEXT("Text_ReceiptProgramName"), 22, FFBuildPearl::Ink, TEXT("Bold"));
    PaintText(Tree, TEXT("Text_PurchaseReceipt"), 14, FFBuildPearl::Muted, TEXT("Regular"));
    PaintText(Tree, TEXT("Text_ReceiptResources"), 15, FFBuildPearl::Ink);
    PaintText(Tree, TEXT("Text_Confirm"), 16, FLinearColor::White, TEXT("Bold"));
    PaintText(Tree, TEXT("Text_Close"), 14, FFBuildPearl::Ink);
    PaintText(Tree, TEXT("Text_BackToPrograms"), 14, FFBuildPearl::Ink);
    PaintText(Tree, TEXT("Text_ReturnToFacility"), 16, FLinearColor::White, TEXT("Bold"));
    PaintButton(Tree, TEXT("Button_Close"), false);
    PaintButton(Tree, TEXT("Button_BackToPrograms"), false);
    PaintButton(Tree, TEXT("Button_ReturnToFacility"), true);
    PaintButton(Tree, TEXT("Button_Confirm"), true);
    Find<UTextBlock>(Tree, TEXT("Text_Title"))->SetText(FText::FromString(TEXT("유료 프로그램 편성")));
    Find<UTextBlock>(Tree, TEXT("Text_Close"))->SetText(FText::FromString(TEXT("×")));
    Find<UButton>(Tree, TEXT("Button_Close"))->SetToolTipText(FText::FromString(TEXT("시설 정보로 돌아가기")));
    Eyebrow->SetText(FText::FromString(TEXT("01 · 프로그램 선택")));
    Back->SetVisibility(ESlateVisibility::Collapsed);
    if (auto* Slot = Cast<UHorizontalBoxSlot>(Back->Slot))
        Slot->SetPadding(FMargin(0, 0, 10, 0));
    // The scrollable content shrinks while the payment action remains visible.
    ListSize->ClearMaxDesiredHeight();
    ListSize->SetHeightOverride(390.f);
    ListSize->SetVisibility(ESlateVisibility::Visible);
    Tree.FindWidget(TEXT("Border_Preview"))->SetVisibility(ESlateVisibility::Collapsed);
    Tree.FindWidget(TEXT("Border_Footer"))->SetVisibility(ESlateVisibility::Collapsed);
    if (auto* Resources = Tree.FindWidget(TEXT("Text_ResourcePreview")))
    {
        Resources->RemoveFromParent();
        if (auto* Slot = Cast<UVerticalBoxSlot>(Footer->InsertChildAt(1, Resources)))
            Slot->SetPadding(FMargin(0, 0, 0, 8));
    }
    if (auto* Preview = Find<UVerticalBox>(Tree, TEXT("Panel_Preview")))
        for (UWidget* Child : Preview->GetAllChildren())
            if (auto* Slot = Cast<UVerticalBoxSlot>(Child->Slot))
                Slot->SetPadding(FMargin(0, 0, 0, 7));
    for (const TCHAR* Name : {TEXT("Text_Capacity_Card"), TEXT("Text_TicketFee_Card")})
        if (auto* Metric = Find<UBorder>(Tree, Name))
        {
            Metric->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Color(231, 243, 253, .96f),
                14.f, FFBuildPearl::Color(190, 222, 245, .8f), 1.f));
            Metric->SetBrushColor(FLinearColor::White);
            Metric->SetPadding(FMargin(10, 8));
        }
    for (const TCHAR* Name : {TEXT("Text_Capacity_Label"), TEXT("Text_TicketFee_Label")})
        PaintText(Tree, Name, 11, FFBuildPearl::Muted, TEXT("Regular"));
    if (UTextBlock* Cost = Find<UTextBlock>(Tree, TEXT("Text_PurchaseCost")))
    {
        Cost->SetAutoWrapText(false);
        Cost->SetJustification(ETextJustify::Right);
    }
    for (const TCHAR* Name : {TEXT("Text_Close"), TEXT("Text_BackToPrograms"),
        TEXT("Text_Confirm"), TEXT("Text_ReturnToFacility")})
        if (auto* Copy = Find<UTextBlock>(Tree, Name))
        {
            Copy->SetAutoWrapText(false);
            Copy->SetJustification(ETextJustify::Center);
        }
    for (const TCHAR* Name : {TEXT("ScrollBox_Programs"),
        TEXT("ScrollBox_PurchaseDetails"), TEXT("ScrollBox_PurchaseReceipt")})
        if (UScrollBox* Scroll = Find<UScrollBox>(Tree, Name))
        {
            Scroll->SetAnimateWheelScrolling(false);
            Scroll->SetScrollBarVisibility(ESlateVisibility::Visible);
            Scroll->SetScrollbarThickness(FVector2D(3, 3));
        }
    return true;
}

bool ApplyOne(const TCHAR* Package, UClass& Parent,
    bool (*Edit)(UWidgetBlueprint&, UWidgetTree&))
{
    UWidgetBlueprint* BP = LoadExistingBlueprint(Package);
    if (!BP || !BP->WidgetTree || !BP->GeneratedClass ||
        !BP->GeneratedClass->IsChildOf(&Parent))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_CONTRACT_FAIL %s"), Prefix, Package);
        return false;
    }
    auto& Metadata = BP->GetOutermost()->GetMetaData();
    if (Metadata.GetValue(BP, Version) == TEXT("1")) return true;
    BP->Modify();
    BP->WidgetTree->Modify();
    if (!Edit(*BP, *BP->WidgetTree))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_LAYOUT_FAIL %s"), Prefix, Package);
        return false;
    }
    Metadata.SetValue(BP, Version, TEXT("1"));
    if (SaveNewBlueprint(Prefix, *BP)) return true;
    Metadata.RemoveValue(BP, Version);
    return false;
}

void Apply()
{
    const bool bPaidCard = ApplyOne(FFBuildUIAssetPaths::FacilityPaidProgramCardPackage,
        *UFF_FacilityPaidProgramCard::StaticClass(), PaidCard);
    const bool bPaidPicker = ApplyOne(FFBuildUIAssetPaths::FacilityPaidProgramPickerPackage,
        *UFF_FacilityPaidProgramPicker::StaticClass(), PaidPicker);
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_COMPLETE_%s (paid card=%d, picker=%d)"),
        Prefix, bPaidCard && bPaidPicker ? TEXT("PASS") : TEXT("FAIL"), bPaidCard, bPaidPicker);
}

FAutoConsoleCommand Command(TEXT("FF.ApplyProgramPearlUI"),
    TEXT("Restyle saved facility paid-program widgets with the construction pearl palette."),
    FConsoleCommandDelegate::CreateStatic(&Apply));
}

#endif
