// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// FF_BuildModeClaritySynergyBuilder.cpp
// 역할: 시설 조합 미리보기 화면의 글자와 배치를 명시적으로 수정한다.

// One-time refinement of the existing read-only combination preview, preserving its tree and bindings.
#if WITH_EDITOR
#include "Building/Presentation/FF_BuildSynergyWidget.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Core/FF_BuildPearlStyle.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace FFBuildModeClaritySynergy
{
using namespace FFUIWbpBuilder;
static bool Apply(UWidgetBlueprint& BP)
{
    const TCHAR* Version=TEXT("FF.BuildPearl.SynergyV3");
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
    Slot->SetPosition(FVector2D(-28,440));Slot->SetAutoSize(true);
    const FLinearColor Ink=FFBuildPearl::Ink;
    const FLinearColor Muted=FFBuildPearl::Muted;
    FSlateBrush Brush;
    Brush.DrawAs=ESlateBrushDrawType::RoundedBox;
    Brush.TintColor=FLinearColor::FromSRGBColor(FColor(242,247,238));
    Brush.OutlineSettings=FSlateBrushOutlineSettings(10,FSlateColor(FLinearColor::FromSRGBColor(FColor(198,214,196))),1);
    if(auto* Material=LoadObject<UMaterialInterface>(nullptr,
        FFBuildUIAssetPaths::ClarityPaperObject))
    { Brush.SetResourceObject(Material);Brush.DrawAs=ESlateBrushDrawType::Image;Brush.TintColor=FLinearColor::White; }
    Panel->SetBrush(FFBuildPearl::MaterialBrush(TEXT("Card")));Panel->SetBrushColor(FLinearColor::White);Panel->SetPadding(FMargin(14,10));
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
    auto* BP=LoadExistingBlueprint(FFBuildUIAssetPaths::BuildSynergyPackage);
    const bool Success=BP&&Apply(*BP);
    UE_LOG(LogFFUIWbpBuilder,Display,TEXT("FF_BUILD_CLARITY_SYNERGY_%s"),Success?TEXT("PASS"):TEXT("FAIL"));
}
static FAutoConsoleCommand Command(TEXT("FF.ApplyBuildModeClaritySynergy"),
    TEXT("Refine the saved synergy preview to compact sage typography and full-width wrapped content once, preserving its native bindings."),
    FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
