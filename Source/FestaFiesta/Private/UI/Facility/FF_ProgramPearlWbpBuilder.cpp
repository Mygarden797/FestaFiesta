// FF_ProgramPearlWbpBuilder.cpp
// Restyle the saved facility paid-program widgets without replacing their bindings.

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_BuildPearlStyle.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Facility/FF_FacilityPaidProgramCard.h"
#include "UI/Facility/FF_FacilityPaidProgramPicker.h"
#include "UObject/MetaData.h"
#include "WidgetBlueprint.h"

namespace FFProgramPearlWbpBuilder
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Prefix = TEXT("FF_PROGRAM_PEARL");
constexpr const TCHAR* Version = TEXT("FF.ProgramPearl.V2");

template <typename T>
T* Find(UWidgetTree& Tree, const TCHAR* Name)
{
    return Cast<T>(Tree.FindWidget(Name));
}

void PaintText(UWidgetTree& Tree, const TCHAR* Name, const int32 Size,
    const FLinearColor Color, const FName Weight = TEXT("SemiBold"))
{
    if (UTextBlock* Text = Find<UTextBlock>(Tree, Name))
        StyleText(*Text, Size, Color, Weight);
}

void PaintBorder(UWidgetTree& Tree, const TCHAR* Name, const TCHAR* Role,
    const FMargin Padding)
{
    if (UBorder* Border = Find<UBorder>(Tree, Name))
    {
        Border->SetBrush(FFBuildPearl::MaterialBrush(Role));
        Border->SetBrushColor(FLinearColor::White);
        Border->SetPadding(Padding);
    }
}

void PaintButton(UWidgetTree& Tree, const TCHAR* Name, const bool bPrimary)
{
    UButton* Button = Find<UButton>(Tree, Name);
    if (!Button) return;
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
    Style.Pressed = FFBuildPearl::Rounded(FFBuildPearl::Cyan, 19.f);
    Style.Disabled = FFBuildPearl::Rounded(bPrimary
        ? FFBuildPearl::Color(117, 119, 143, .78f)
        : FFBuildPearl::Color(238, 239, 246, .75f), 19.f);
    Style.NormalPadding = Style.PressedPadding = FMargin(13.f, 7.f);
    Button->SetStyle(Style);
    Button->SetBackgroundColor(FLinearColor::White);
}

bool PaidCard(UWidgetBlueprint& BP, UWidgetTree& Tree)
{
    UButton* Select = Find<UButton>(Tree, TEXT("Button_Select"));
    if (!Select || !Find<UTextBlock>(Tree, TEXT("Text_ProgramName")) ||
        !Find<UTextBlock>(Tree, TEXT("Text_ProgramMeta")) ||
        !Find<UTextBlock>(Tree, TEXT("Text_ProgramAvailability"))) return false;
    PaintButton(Tree, TEXT("Button_Select"), false);
    PaintText(Tree, TEXT("Text_ProgramName"), 20, FFBuildPearl::Ink, TEXT("Bold"));
    PaintText(Tree, TEXT("Text_ProgramMeta"), 14, FFBuildPearl::Muted);
    PaintText(Tree, TEXT("Text_ProgramAvailability"), 14, FFBuildPearl::Ink);
    if (UVerticalBox* Content = Find<UVerticalBox>(Tree, TEXT("Panel_CardContent")))
    {
        for (UWidget* Child : Content->GetAllChildren())
            if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Child->Slot))
                Slot->SetPadding(FMargin(17.f, 5.f, 17.f, 5.f));
    }
    return true;
}

bool PaidPicker(UWidgetBlueprint& BP, UWidgetTree& Tree)
{
    if (!Find<UBorder>(Tree, TEXT("Border_Picker")) ||
        !Find<UButton>(Tree, TEXT("Button_Confirm")) ||
        !Find<UTextBlock>(Tree, TEXT("Text_PreviewName"))) return false;
    PaintBorder(Tree, TEXT("Border_Picker"), TEXT("Card"), FMargin(0.f));
    PaintBorder(Tree, TEXT("Border_Header"), TEXT("Dark"), FMargin(18.f, 14.f));
    PaintBorder(Tree, TEXT("Border_Body"), TEXT("Card"), FMargin(17.f, 12.f));
    PaintBorder(Tree, TEXT("Border_ProgramList"), TEXT("Panel"), FMargin(10.f));
    PaintBorder(Tree, TEXT("Border_Preview"), TEXT("Card"), FMargin(16.f, 14.f));
    PaintBorder(Tree, TEXT("Border_Footer"), TEXT("Card"), FMargin(18.f, 12.f));
    PaintText(Tree, TEXT("Text_Eyebrow"), 11, FFBuildPearl::Cyan);
    PaintText(Tree, TEXT("Text_Title"), 26, FLinearColor::White, TEXT("Bold"));
    PaintText(Tree, TEXT("Text_FacilityName"), 19, FFBuildPearl::Ink, TEXT("Bold"));
    PaintText(Tree, TEXT("Text_Instruction"), 14, FFBuildPearl::Muted, TEXT("Regular"));
    PaintText(Tree, TEXT("Text_Empty"), 14, FFBuildPearl::Muted, TEXT("Regular"));
    PaintText(Tree, TEXT("Text_PreviewName"), 25, FFBuildPearl::Ink, TEXT("Bold"));
    PaintText(Tree, TEXT("Text_PreviewTime"), 16, FFBuildPearl::Muted);
    PaintText(Tree, TEXT("Text_PreviewDescription"), 15, FFBuildPearl::Muted, TEXT("Regular"));
    PaintText(Tree, TEXT("Text_Capacity"), 22, FFBuildPearl::Ink, TEXT("Bold"));
    PaintText(Tree, TEXT("Text_TicketFee"), 22, FFBuildPearl::Ink, TEXT("Bold"));
    PaintText(Tree, TEXT("Text_MaxRevenue"), 16, FFBuildPearl::Ink);
    PaintText(Tree, TEXT("Text_PurchaseCost"), 31, FFBuildPearl::Ink, TEXT("Bold"));
    PaintText(Tree, TEXT("Text_PurchaseCostLabel"), 15, FFBuildPearl::Muted);
    PaintText(Tree, TEXT("Text_ResourcePreview"), 16, FFBuildPearl::Ink);
    PaintText(Tree, TEXT("Text_NoRefundNotice"), 13, FFBuildPearl::Muted, TEXT("Regular"));
    PaintText(Tree, TEXT("Text_PurchaseCompleteTitle"), 20, FFBuildPearl::Ink, TEXT("Bold"));
    PaintText(Tree, TEXT("Text_ReceiptProgramName"), 25, FFBuildPearl::Ink, TEXT("Bold"));
    PaintText(Tree, TEXT("Text_PurchaseReceipt"), 15, FFBuildPearl::Muted, TEXT("Regular"));
    PaintText(Tree, TEXT("Text_ReceiptResources"), 17, FFBuildPearl::Ink);
    PaintText(Tree, TEXT("Text_Confirm"), 18, FLinearColor::White, TEXT("Bold"));
    PaintText(Tree, TEXT("Text_Close"), 14, FFBuildPearl::Ink);
    PaintText(Tree, TEXT("Text_BackToPrograms"), 14, FFBuildPearl::Ink);
    PaintText(Tree, TEXT("Text_ReturnToFacility"), 16, FLinearColor::White, TEXT("Bold"));
    PaintButton(Tree, TEXT("Button_Close"), false);
    PaintButton(Tree, TEXT("Button_BackToPrograms"), false);
    PaintButton(Tree, TEXT("Button_ReturnToFacility"), true);
    PaintButton(Tree, TEXT("Button_Confirm"), true);
    if (UTextBlock* Cost = Find<UTextBlock>(Tree, TEXT("Text_PurchaseCost")))
    {
        Cost->SetAutoWrapText(false);
        Cost->SetJustification(ETextJustify::Right);
    }
    for (const TCHAR* Name : {TEXT("ScrollBox_Programs"),
        TEXT("ScrollBox_PurchaseDetails"), TEXT("ScrollBox_PurchaseReceipt")})
        if (UScrollBox* Scroll = Find<UScrollBox>(Tree, Name))
            Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
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
