// One-time migration of the build synergy card's A5 styling into its WBP.
#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildSynergyWidget.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_BuildPearlStyle.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "WidgetBlueprint.h"

namespace FFBuildSynergyA5Designer
{
using namespace FFUIWbpBuilder;
static bool Apply(UWidgetBlueprint& BP)
{
    UWidgetTree* Tree=BP.WidgetTree;
    if (!Tree || !BP.ParentClass || !BP.ParentClass->IsChildOf(UFF_BuildSynergyWidget::StaticClass())) return false;
    if (Tree->FindWidget(TEXT("A5_SynergyDesignerLayout"))) return true;
    auto* Size=Cast<USizeBox>(Tree->FindWidget(TEXT("Size_Synergy")));
    auto* Position=Size?Cast<UCanvasPanelSlot>(Size->Slot):nullptr;
    auto* Panel=Cast<UBorder>(Tree->FindWidget(TEXT("Border_Synergy")));
    auto* Rows=Cast<UVerticalBox>(Tree->FindWidget(TEXT("Vertical_Synergy")));
    if (!Position || !Panel || !Rows || Panel->GetContent()!=Rows) return false;
    TArray<UTextBlock*> Labels;
    for (const TCHAR* Name : {TEXT("Text_SynergyHeading"),TEXT("Text_SynergyLegend"),TEXT("Text_SynergyFooter"),
        TEXT("Text_SynergyTitle_1"),TEXT("Text_SynergyTitle_2"),TEXT("Text_SynergyTitle_3"),
        TEXT("Text_SynergyDetail_1"),TEXT("Text_SynergyDetail_2"),TEXT("Text_SynergyDetail_3")})
    {
        auto* Label=Cast<UTextBlock>(Tree->FindWidget(Name)); if (!Label) return false; Labels.Add(Label);
    }
    BP.Modify(); Tree->Modify(); Position->Modify(); Panel->Modify(); Rows->Modify();
    Position->SetAnchors(FAnchors(1,0)); Position->SetAlignment(FVector2D(1,0)); Position->SetPosition(FVector2D(-512,104));
    Panel->SetBrush(FFBuildPearl::MaterialBrush(TEXT("Card"))); Panel->SetBrushColor(FLinearColor::White);
    for (int32 Index=0;Index<Labels.Num();++Index)
    {
        Labels[Index]->Modify();
        Labels[Index]->SetColorAndOpacity(FSlateColor(Index==0 || (Index>=3 && Index<=5) ? FFBuildPearl::Ink : FFBuildPearl::Muted));
    }
    auto* Marker=MakeWidget<USizeBox>(BP,*Tree,TEXT("A5_SynergyDesignerLayout"));
    Marker->SetVisibility(ESlateVisibility::Collapsed); Rows->AddChildToVerticalBox(Marker);
    return SaveNewBlueprint(TEXT("FF_BUILD_SYNERGY_A5_DESIGNER"),BP);
}
static void Run()
{
    UWidgetBlueprint* BP=LoadExistingBlueprint(FFBuildUIAssetPaths::BuildSynergyPackage);
    const bool Success=BP && Apply(*BP);
    UE_LOG(LogFFUIWbpBuilder,Display,TEXT("FF_BUILD_SYNERGY_A5_DESIGNER_%s"),Success?TEXT("PASS"):TEXT("FAIL"));
}
static FAutoConsoleCommand Command(TEXT("FF.ApplyBuildSynergyA5DesignerLayout"),
    TEXT("Save the approved synergy card position and style to its WBP."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
