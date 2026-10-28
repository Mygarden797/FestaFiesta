// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// FF_BuildModeClarityToolsBuilder.cpp
// 역할: 건설 도구 모음의 명확한 버튼 배치를 에디터 명령으로 적용한다.

// Explicit saved-asset migration; gameplay never constructs a replacement widget tree.
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
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "WidgetBlueprintEditorUtils.h"

namespace FFBuildClarityTools
{
using namespace FFUIWbpBuilder;
const FLinearColor Ink = FLinearColor::FromSRGBColor(FColor(43, 72, 59));
const FLinearColor Muted = FLinearColor::FromSRGBColor(FColor(95, 115, 102));
const FLinearColor Paper = FLinearColor::FromSRGBColor(FColor(242, 247, 238));

FSlateBrush Brush(const TCHAR* Role)
{
    FSlateBrush B;
    const FString Path = FString(FFBuildUIAssetPaths::ClarityFolder) / (FString(TEXT("MI_UI_BuildClarity_")) + Role);
    if (auto* M = LoadObject<UMaterialInterface>(nullptr, *Path))
    {
        B.SetResourceObject(M); B.DrawAs = ESlateBrushDrawType::Image; B.TintColor = FLinearColor::White;
    }
    else
    {
        B.DrawAs = ESlateBrushDrawType::RoundedBox; B.TintColor = Paper;
        B.OutlineSettings.CornerRadii = FVector4(12); B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
    }
    return B;
}
UTextBlock* Text(UWidgetBlueprint& BP, UWidgetTree& T, FName Name, const TCHAR* Value, int32 Size = 16, FLinearColor Color = Ink)
{
    auto* W = MakeWidget<UTextBlock>(BP, T, Name, true);
    StyleText(*W, Size, Color, TEXT("Medium")); W->SetText(FText::FromString(Value));
    W->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis); W->SetVisibility(ESlateVisibility::HitTestInvisible);
    return W;
}
void V(UVerticalBox* Parent, UWidget* Child, float Bottom = 8)
{
    auto* S = Parent->AddChildToVerticalBox(Child); S->SetPadding(FMargin(0, 0, 0, Bottom));
}
void H(UHorizontalBox* Parent, UWidget* Child, bool Fill = true, float Right = 8)
{
    auto* S = Parent->AddChildToHorizontalBox(Child); S->SetSize(FSlateChildSize(Fill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
    S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0, 0, Right, 0));
}
UBorder* Panel(UWidgetBlueprint& BP, UWidgetTree& T, FName Name, UWidget* Child, const TCHAR* Role = TEXT("Paper"))
{
    auto* W = MakeWidget<UBorder>(BP, T, Name, true); W->SetBrush(Brush(Role));
    W->SetBrushColor(FLinearColor::White); W->SetPadding(FMargin(18, 14)); W->AddChild(Child); return W;
}
UFF_BuildToolsActionButton* Button(UWidgetBlueprint& BP, UWidgetTree& T, FName Name, FName Label, const TCHAR* Value, bool Primary = false)
{
    auto* W = MakeWidget<UFF_BuildToolsActionButton>(BP, T, Name, true);
    FButtonStyle S;
    S.Normal = Brush(Primary ? TEXT("Action") : TEXT("Header"));
    S.Hovered = Brush(Primary ? TEXT("Dark") : TEXT("Gold")); S.Pressed = Brush(TEXT("Header")); S.Disabled = Brush(TEXT("Paper"));
    S.NormalPadding = S.PressedPadding = FMargin(10, 10); W->SetStyle(S);
    W->bUseSelectionStyle = true; W->SelectionStyle = S; W->SelectionStyle.Normal = Brush(TEXT("Action"));
    W->SelectionStyle.Hovered = Brush(TEXT("Dark")); W->SelectionTextColor = FLinearColor::White;
    auto* Slot = CastChecked<UButtonSlot>(W->AddChild(Text(BP, T, Label, Value, 16, Primary ? FLinearColor::White : Ink)));
    Slot->SetHorizontalAlignment(HAlign_Center); Slot->SetVerticalAlignment(VAlign_Center); return W;
}
void Dock(UWidgetBlueprint& BP, UWidgetTree& T, UCanvasPanel* Root, FName Name, UWidget* Child, float Width, FVector2D Anchor, FVector2D Pos, FVector2D Align)
{
    auto* Size = MakeWidget<USizeBox>(BP, T, Name); Size->SetWidthOverride(Width); Size->AddChild(Child);
    auto* Slot = Root->AddChildToCanvas(Size); Slot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
    Slot->SetAlignment(Align); Slot->SetPosition(Pos); Slot->SetAutoSize(true);
}
bool Layout(UWidgetBlueprint& BP, UWidgetTree& T)
{
    auto* Root = MakeWidget<UCanvasPanel>(BP, T, TEXT("Canvas_BuildClarity")); T.RootWidget = Root;
    Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    auto* Summary = MakeWidget<UVerticalBox>(BP, T, TEXT("Column_ClaritySummary"));
    auto* SummaryHeader = MakeWidget<UHorizontalBox>(BP, T, TEXT("Row_ClarityMode"));
    H(SummaryHeader, Text(BP, T, TEXT("Text_Mode"), TEXT("시설 배치"), 15, Muted));
    H(SummaryHeader, Button(BP, T, TEXT("Button_Facts"), TEXT("Label_Facts"), TEXT("시설 규격")), false, 0);
    V(Summary, SummaryHeader, 6);
    V(Summary, Text(BP, T, TEXT("Text_FacilityName"), TEXT("시설을 선택하세요"), 23), 10);
    V(Summary, Text(BP, T, TEXT("Text_Cost"), TEXT("설치 비용 확인 중"), 21), 6);
    V(Summary, Text(BP, T, TEXT("Text_Balance"), TEXT(""), 15, Muted), 10);
    auto* Status = Text(BP, T, TEXT("Text_Legend"), TEXT("배치 위치를 선택하세요"), 16);
    Status->SetAutoWrapText(true); Status->SetWrapTextAt(358); V(Summary, Status, 0);
    auto* Facts = MakeWidget<UVerticalBox>(BP, T, TEXT("Panel_PlacementFacts"), true);
    Facts->SetVisibility(ESlateVisibility::Collapsed);
    V(Facts, Text(BP, T, TEXT("Text_Footprint"), TEXT(""), 16), 8);
    V(Facts, Text(BP, T, TEXT("Text_Queue"), TEXT(""), 16), 8);
    V(Facts, Text(BP, T, TEXT("Text_RangeLegend"), TEXT("실선 시설 · 청록 대기열"), 13, Muted), 0);
    V(Summary, Facts, 0);
    Dock(BP, T, Root, TEXT("Size_PlacementInfo"), Panel(BP, T, TEXT("Border_PlacementInfo"), Summary),
        400, FVector2D(0, 0), FVector2D(28, 56), FVector2D::ZeroVector);

    auto* Quick = MakeWidget<UVerticalBox>(BP, T, TEXT("Column_ClarityTools"));
    auto* HelpText = Text(BP, T, TEXT("Text_ControlHelp"), TEXT(""), 15, Muted);
    HelpText->SetAutoWrapText(true); HelpText->SetWrapTextAt(332); HelpText->SetLineHeightPercentage(1.2f);
    auto* Help = Panel(BP, T, TEXT("Border_PlacementHelp"), HelpText, TEXT("Header")); Help->SetVisibility(ESlateVisibility::Collapsed); V(Quick, Help);
    auto* State = MakeWidget<UHorizontalBox>(BP, T, TEXT("Row_ClarityHint"));
    H(State, Text(BP, T, TEXT("Text_ActiveSnap"), TEXT("100 cm 맞춤"), 15, Muted));
    H(State, Button(BP, T, TEXT("Button_Help"), TEXT("Label_Help"), TEXT("조작 안내")), false, 0); V(Quick, State, 8);
    auto* Actions = MakeWidget<UHorizontalBox>(BP, T, TEXT("Row_ClarityActions"));
    H(Actions, Button(BP, T, TEXT("Button_Catalog"), TEXT("Label_Catalog"), TEXT("목록 B")));
    H(Actions, Button(BP, T, TEXT("Button_Confirm"), TEXT("Text_Confirm"), TEXT("배치 확정"), true), true, 0); V(Quick, Actions, 0);
    Dock(BP, T, Root, TEXT("Size_Tools"), Panel(BP, T, TEXT("Border_Tools"), Quick),
        300, FVector2D(1, 1), FVector2D(-28, -96), FVector2D(1, 1));

    auto* PingText = Text(BP, T, TEXT("Text_ContextPing"), TEXT("배치 가능 · 클릭 / Enter"), 15);
    PingText->SetAutoWrapText(false);
    PingText->SetWrapTextAt(336.f);
    PingText->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
    auto* Ping = Panel(BP, T, TEXT("Border_ContextPing"), PingText, TEXT("Header")); Ping->SetPadding(FMargin(12, 8));
    Ping->SetVisibility(ESlateVisibility::Collapsed);
    auto* PingSlot = Root->AddChildToCanvas(Ping); PingSlot->SetAutoSize(true);
    return true;
}
bool RefineLeftHelp(UWidgetBlueprint& BP)
{
    const TCHAR* Key = TEXT("FF.BuildClarity.LeftHelpV2");
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, Key) == TEXT("1")) return true;
    auto& Tree = *BP.WidgetTree;
    auto* InfoSize = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_PlacementInfo")));
    auto* Info = Cast<UBorder>(Tree.FindWidget(TEXT("Border_PlacementInfo")));
    auto* Help = Cast<UBorder>(Tree.FindWidget(TEXT("Border_PlacementHelp")));
    auto* Mode = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("Row_ClarityMode")));
    auto* HelpButton = Cast<UFF_BuildToolsActionButton>(Tree.FindWidget(TEXT("Button_Help")));
    if (!InfoSize || !Info || !Help || !Mode || !HelpButton) return false;
    BP.Modify(); Tree.Modify();
    Info->RemoveFromParent(); Help->RemoveFromParent(); HelpButton->RemoveFromParent();
    auto* Left = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Column_ClarityLeftDock"));
    V(Left, Info, 0);
    auto* HelpSlot = Left->AddChildToVerticalBox(Help);
    HelpSlot->SetPadding(FMargin(0, 10, 0, 0));
    InfoSize->AddChild(Left);
    H(Mode, HelpButton, false, 0);
    if (auto* Facts = Tree.FindWidget(TEXT("Button_Facts")))
        if (auto* FactsSlot = Cast<UHorizontalBoxSlot>(Facts->Slot)) FactsSlot->SetPadding(FMargin(0, 0, 6, 0));
    Help->SetBrush(Brush(TEXT("Paper")));
    Help->SetPadding(FMargin(18, 14));
    if (auto* Label = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_ControlHelp"))))
    {
        StyleText(*Label, 14, Muted, TEXT("Medium"));
        Label->SetWrapTextAt(364); Label->SetLineHeightPercentage(1.25f);
    }
    for (const TCHAR* Name : { TEXT("Button_Facts"), TEXT("Button_Help") })
        if (auto* B = Cast<UFF_BuildToolsActionButton>(Tree.FindWidget(Name)))
        {
            auto Style = B->GetStyle(); Style.NormalPadding = Style.PressedPadding = FMargin(9, 9);
            B->SetStyle(Style);
            if (auto* Label = Cast<UTextBlock>(B->GetContent())) StyleText(*Label, 14, Ink, TEXT("Medium"));
        }
    Metadata.SetValue(&BP, Key, TEXT("1"));
    if (SaveNewBlueprint(TEXT("FF_BUILD_CLARITY_LEFT_HELP"), BP)) return true;
    Metadata.RemoveValue(&BP, Key); return false;
}
bool RefinePlacementAssistance(UWidgetBlueprint& BP)
{
    const TCHAR* Key = TEXT("FF.BuildClarity.PlacementAssistanceV3");
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, Key) == TEXT("1")) return true;
    auto& Tree = *BP.WidgetTree;
    auto* Summary = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Column_ClaritySummary")));
    if (!Summary) return false;
    BP.Modify(); Tree.Modify();
    // Existing summary/facts/help remain editable and keep their bindings.
    for (const TCHAR* Name : { TEXT("Text_PlacementAdvice"), TEXT("Text_PlacementObstruction"), TEXT("Text_AssistanceMessage") })
    {
        auto* Label = Text(BP, Tree, Name, TEXT(""), 14, Muted);
        Label->SetAutoWrapText(true); Label->SetWrapTextAt(364);
        Label->SetTextOverflowPolicy(ETextOverflowPolicy::Clip);
        Label->SetVisibility(ESlateVisibility::Collapsed);
        V(Summary, Label, 5);
    }
    auto* Direct = Button(BP, Tree, TEXT("Button_DirectAdjustment"), TEXT("Label_DirectAdjustment"), TEXT("직접 조정  T"));
    V(Summary, Direct, 5);
    auto* Actions = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_PlacementAssistance"));
    auto* Nearby = Button(BP, Tree, TEXT("Button_FindNearby"), TEXT("Label_FindNearby"), TEXT("가까운 빈자리"));
    auto* Next = Button(BP, Tree, TEXT("Button_NextPlacementProblem"), TEXT("Label_NextPlacementProblem"), TEXT("다른 원인  ›"));
    H(Actions, Nearby); H(Actions, Next, true, 0); V(Summary, Actions, 0);
    Nearby->SetVisibility(ESlateVisibility::Collapsed); Next->SetVisibility(ESlateVisibility::Collapsed);
    for (auto* Action : {Direct, Nearby, Next})
    {
        auto Style = Action->GetStyle(); Style.NormalPadding = Style.PressedPadding = FMargin(8, 8);
        Action->SetStyle(Style);
        if (auto* Label = Cast<UTextBlock>(Action->GetContent())) StyleText(*Label, 14, Ink, TEXT("Medium"));
    }
    if (auto* PingText = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_ContextPing"))))
    {
        PingText->SetAutoWrapText(true);
        PingText->SetWrapTextAt(336);
    }
    Metadata.SetValue(&BP, Key, TEXT("1"));
    if (SaveNewBlueprint(TEXT("FF_BUILD_PLACEMENT_ASSISTANCE"), BP)) return true;
    Metadata.RemoveValue(&BP, Key); return false;
}

bool RemoveRadialTools(UWidgetBlueprint& BP)
{
    const TCHAR* Key = TEXT("FF.BuildClarity.RemoveRadialV4");
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, Key) == TEXT("1")) return true;
    auto& Tree = *BP.WidgetTree;
    auto* Size = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_Tools")));
    if (!Size) return false;
    BP.Modify(); Tree.Modify();
    TSet<UWidget*> Removed;
    for (FName Name : {TEXT("Button_Radial"), TEXT("Panel_Radial"), TEXT("Panel_Details")})
        if (UWidget* Widget = Tree.FindWidget(Name)) Removed.Add(Widget);
    // The editor utility removes descendants, bindings and orphaned widget variables too.
    FWidgetBlueprintEditorUtils::DeleteWidgets(&BP, Removed,
        FWidgetBlueprintEditorUtils::EDeleteWidgetWarningType::DeleteSilently);
    Size->SetWidthOverride(300);
    Metadata.SetValue(&BP, Key, TEXT("1"));
    if (SaveNewBlueprint(TEXT("FF_BUILD_REMOVE_RADIAL"), BP)) return true;
    Metadata.RemoveValue(&BP, Key); return false;
}

void Apply()
{
    const FString Package(FFBuildUIAssetPaths::BuildToolsPackage);
    if (auto* Existing = LoadExistingBlueprint(Package))
        if (Existing->WidgetTree && Existing->WidgetTree->FindWidget(TEXT("Canvas_BuildClarity")))
        { UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_BUILD_CLARITY_TOOLS_%s"), RefineLeftHelp(*Existing) && RefinePlacementAssistance(*Existing) && RemoveRadialTools(*Existing) ? TEXT("PASS") : TEXT("FAIL")); return; }
    if (ReplaceWidgetBlueprint(TEXT("FF_BUILD_CLARITY_TOOLS"), Package, FFBuildUIAssetPaths::ControlsFolder,
        *UFF_BuildToolsWidget::StaticClass(), Layout))
        if (auto* Existing = LoadExistingBlueprint(Package)) { RefineLeftHelp(*Existing); RefinePlacementAssistance(*Existing); RemoveRadialTools(*Existing); }
}
FAutoConsoleCommand Command(TEXT("FF.ApplyBuildModeClarityTools"), TEXT("Apply compact construction HUD without the retired R menu to saved WBP."), FConsoleCommandDelegate::CreateStatic(&Apply));
}
#endif
