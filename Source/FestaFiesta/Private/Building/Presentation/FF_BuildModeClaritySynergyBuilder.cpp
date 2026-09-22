// One-time refinement of the existing read-only combination preview, preserving its tree and bindings.
#if WITH_EDITOR
#include "Building/Presentation/FF_BuildSynergyWidget.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace FFBuildModeClaritySynergy
{
using namespace FFUIWbpBuilder;
static bool Apply(UWidgetBlueprint& BP)
{
    const TCHAR* Version=TEXT("FF.BuildClarity.SynergyV1");
    if(BP.GetOutermost()->GetMetaData().GetValue(&BP,Version)==TEXT("1")) return true;
    UWidgetTree* Tree=BP.WidgetTree;
    auto* Size=Tree?Cast<USizeBox>(Tree->FindWidget(TEXT("Size_Synergy"))):nullptr;
    auto* Panel=Tree?Cast<UBorder>(Tree->FindWidget(TEXT("Border_Synergy"))):nullptr;
    auto* Slot=Size?Cast<UCanvasPanelSlot>(Size->Slot):nullptr;
    if(!Size || !Panel || !Slot || !BP.ParentClass || !BP.ParentClass->IsChildOf(UFF_BuildSynergyWidget::StaticClass())) return false;
    for(const TCHAR* Name:{TEXT("Text_SynergyHeading"),TEXT("Text_SynergyLegend"),TEXT("Text_SynergyFooter"),
        TEXT("Text_SynergyTitle_1"),TEXT("Text_SynergyTitle_2"),TEXT("Text_SynergyTitle_3"),
        TEXT("Text_SynergyDetail_1"),TEXT("Text_SynergyDetail_2"),TEXT("Text_SynergyDetail_3")})
        if(!Cast<UTextBlock>(Tree->FindWidget(Name))) return false;
    BP.Modify(); Tree->Modify();
    Size->SetWidthOverride(412); Size->ClearHeightOverride();
    Slot->SetAnchors(FAnchors(1,0));Slot->SetAlignment(FVector2D(1,0));
    Slot->SetPosition(FVector2D(-28,56));Slot->SetAutoSize(true);
    const FLinearColor Ink=FLinearColor::FromSRGBColor(FColor(43,72,59));
    const FLinearColor Muted=FLinearColor::FromSRGBColor(FColor(95,115,102));
    FSlateBrush Brush;
    Brush.DrawAs=ESlateBrushDrawType::RoundedBox;
    Brush.TintColor=FLinearColor::FromSRGBColor(FColor(242,247,238));
    Brush.OutlineSettings=FSlateBrushOutlineSettings(10,FSlateColor(FLinearColor::FromSRGBColor(FColor(198,214,196))),1);
    if(auto* Material=LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Controls/Clarity/MI_UI_BuildClarity_Paper.MI_UI_BuildClarity_Paper")))
    { Brush.SetResourceObject(Material);Brush.DrawAs=ESlateBrushDrawType::Image;Brush.TintColor=FLinearColor::White; }
    Panel->SetBrush(Brush);Panel->SetBrushColor(FLinearColor::White);Panel->SetPadding(FMargin(14,10));
    Tree->ForEachWidget([&](UWidget* Widget)
    {
        auto* Text=Cast<UTextBlock>(Widget);if(!Text)return;
        const FString Name=Text->GetName();
        if(!Name.StartsWith(TEXT("Text_Synergy"))) return;
        const bool Heading=Name==TEXT("Text_SynergyHeading");
        const bool Title=Name.StartsWith(TEXT("Text_SynergyTitle_"));
        StyleText(*Text,Heading?15:(Title?14:13),Heading||Title?Ink:Muted,Heading||Title?TEXT("SemiBold"):TEXT("Regular"));
        // Explicit width stabilizes the first frame and per-character wrapping also
        // keeps an unusually long facility name inside the authored card.
        Text->SetAutoWrapText(true);Text->SetWrapTextAt(384);
        Text->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
        Text->SetTextOverflowPolicy(ETextOverflowPolicy::Clip);
        Text->SetLineHeightPercentage(1.f);Text->SetMinDesiredWidth(0);
        Text->SetVisibility(ESlateVisibility::HitTestInvisible);
        if(auto* TextSlot=Cast<UVerticalBoxSlot>(Text->Slot))
        {
            TextSlot->SetHorizontalAlignment(HAlign_Fill);
            TextSlot->SetPadding(FMargin(0,0,0,Heading?4:(Title?0:(Name==TEXT("Text_SynergyFooter")?0:4))));
        }
    });
    BP.GetOutermost()->GetMetaData().SetValue(&BP,Version,TEXT("1"));
    return SaveNewBlueprint(TEXT("FF_BUILD_CLARITY_SYNERGY"),BP);
}
static void Run()
{
    auto* BP=LoadExistingBlueprint(TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Controls/WBP_FF_BuildSynergy"));
    const bool Success=BP&&Apply(*BP);
    UE_LOG(LogFFUIWbpBuilder,Display,TEXT("FF_BUILD_CLARITY_SYNERGY_%s"),Success?TEXT("PASS"):TEXT("FAIL"));
}
static FAutoConsoleCommand Command(TEXT("FF.ApplyBuildModeClaritySynergy"),
    TEXT("Refine the saved synergy preview to compact sage typography and full-width wrapped content once, preserving its native bindings."),
    FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
