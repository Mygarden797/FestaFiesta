// 기존 시설 정보 WBP에 조명 색상 진입 버튼과 최초 안내 카드만 추가한다.
#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Building/Presentation/FF_BuildTypography.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_BuildPearlStyle.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace FFFacilityLightingIntroduction
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Version = TEXT("FF.FacilityLighting.Introduction20261007.1");
constexpr const TCHAR* BadgeVersion = TEXT("FF.FacilityLighting.IntroductionBadge20261009.1");
constexpr const TCHAR* Prefix = TEXT("FF_LIGHTING_INTRODUCTION");

const TCHAR* const WidgetNames[] = {
    TEXT("Panel_LightingColorHelp"), TEXT("Overlay_LightingColorHelp"),
    TEXT("Button_LightingColorHelp"), TEXT("Text_LightingColorHelp"),
    TEXT("Size_LightingIntroductionHint"), TEXT("Border_LightingIntroductionHint"),
    TEXT("Text_LightingIntroductionHint"), TEXT("Panel_LightingIntroduction"),
    TEXT("Vertical_LightingIntroduction"), TEXT("Text_LightingIntroductionTitle"),
    TEXT("Text_LightingIntroductionPick"), TEXT("Text_LightingIntroductionPreview"),
    TEXT("Text_LightingIntroductionApply"), TEXT("Text_LightingIntroductionRestore"),
    TEXT("Size_DismissLightingIntroduction"), TEXT("Button_DismissLightingIntroduction"),
    TEXT("Text_DismissLightingIntroduction")
};

template<class T> T* Get(UWidgetTree& Tree, const TCHAR* Name)
{
    return Cast<T>(Tree.FindWidget(Name));
}

bool Applied(UWidgetBlueprint& BP)
{
    return BP.GetOutermost()->GetMetaData().GetValue(&BP, Version) == TEXT("1");
}

bool Contract(UWidgetBlueprint& BP)
{
    if (!BP.WidgetTree || !BP.WidgetTree->RootWidget || !BP.ParentClass ||
        !BP.ParentClass->IsChildOf(UFF_FacilityInfoPanel::StaticClass())) return false;
    auto& Tree = *BP.WidgetTree;
    auto* Content = Get<UVerticalBox>(Tree, TEXT("Panel_Content"));
    if (!Content || !Get<UButton>(Tree, TEXT("Button_TabLighting")) ||
        !Tree.FindWidget(TEXT("Widget_LightingControls"))) return false;
    if (Applied(BP))
    {
        auto* Help = Get<UBorder>(Tree, TEXT("Panel_LightingColorHelp"));
        auto* Guide = Get<UBorder>(Tree, TEXT("Panel_LightingIntroduction"));
        auto* Open = Get<UButton>(Tree, TEXT("Button_LightingColorHelp"));
        auto* Dismiss = Get<UButton>(Tree, TEXT("Button_DismissLightingIntroduction"));
        for (const TCHAR* Name : WidgetNames)
            if (!Tree.FindWidget(Name)) return false;
        return Help && Guide && Open && Dismiss &&
            Get<UBorder>(Tree, TEXT("Border_LightingIntroductionHint")) &&
            Help->GetParent() == Content && Guide->GetParent() == Content &&
            Cast<UTextBlock>(Open->GetContent()) && Cast<UTextBlock>(Dismiss->GetContent());
    }
    for (const TCHAR* Name : WidgetNames)
        if (Tree.FindWidget(Name))
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL existing widget %s; authored layout was preserved"),
                Prefix, Name);
            return false;
        }
    return true;
}

UTextBlock* Text(UWidgetBlueprint& BP, const TCHAR* Name, const FText& Value,
    FFBuildTypography::ERole Role, int32 Size, FLinearColor Ink = FFBuildPearl::Ink)
{
    auto* Label = MakeWidget<UTextBlock>(BP, *BP.WidgetTree, Name);
    Label->SetText(Value);
    Label->SetFont(FFBuildTypography::Make(Role, Size));
    Label->SetColorAndOpacity(FSlateColor(Ink));
    Label->SetVisibility(ESlateVisibility::HitTestInvisible);
    Label->SetAutoWrapText(true);
    return Label;
}

void Collapsed(UWidget& Widget)
{
    Widget.SetVisibility(ESlateVisibility::Collapsed);
    Widget.bHiddenInDesigner = true;
}

void StyleHint(UBorder& Hint, UTextBlock& Mark)
{
    Hint.SetBrush(FFBuildPearl::Rounded(FLinearColor::Red, 12.f,
        FLinearColor::Transparent, 0.f));
    Hint.SetBrushColor(FLinearColor::White);
    Mark.SetText(FText::GetEmpty());
    Collapsed(Mark);
}

bool UpgradeHint(UWidgetBlueprint& BP)
{
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, BadgeVersion) == TEXT("1")) return true;
    auto* Hint = Get<UBorder>(*BP.WidgetTree, TEXT("Border_LightingIntroductionHint"));
    auto* Mark = Get<UTextBlock>(*BP.WidgetTree, TEXT("Text_LightingIntroductionHint"));
    if (!Hint || !Mark) return false;
    BP.Modify(); BP.WidgetTree->Modify(); Hint->Modify(); Mark->Modify();
    // Update only the badge paint; authored widgets, slots and first-use state remain intact.
    StyleHint(*Hint, *Mark);
    Metadata.SetValue(&BP, BadgeVersion, TEXT("1"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, BadgeVersion);
    return false;
}

UButton* Button(UWidgetBlueprint& BP, const TCHAR* Name, const TCHAR* LabelName,
    const FText& Caption)
{
    auto* Result = MakeWidget<UButton>(BP, *BP.WidgetTree, Name, true);
    FButtonStyle Style;
    Style.Normal = FFBuildPearl::Rounded(FFBuildPearl::Color(224, 244, 254), 12.f,
        FFBuildPearl::Color(155, 213, 240), 1.f);
    Style.Hovered = FFBuildPearl::Rounded(FFBuildPearl::Color(190, 233, 253), 12.f,
        FFBuildPearl::Cyan, 1.f);
    Style.Pressed = FFBuildPearl::Rounded(FFBuildPearl::Color(160, 221, 247), 12.f);
    Style.Disabled = Style.Normal;
    Style.NormalPadding = Style.PressedPadding = FMargin(32.f, 10.f);
    Result->SetStyle(Style);
    Result->SetCursor(EMouseCursor::Hand);
    auto* Label = Text(BP, LabelName, Caption, FFBuildTypography::ERole::Heading, 17);
    Label->SetAutoWrapText(false);
    Label->SetJustification(ETextJustify::Center);
    Result->AddChild(Label);
    return Result;
}

UBorder* MakeHelp(UWidgetBlueprint& BP)
{
    auto& Tree = *BP.WidgetTree;
    auto* Help = MakeWidget<UBorder>(BP, Tree, TEXT("Panel_LightingColorHelp"), true);
    Help->SetBrush(FFBuildPearl::Rounded(FLinearColor::Transparent, 0.f));
    Help->SetBrushColor(FLinearColor::White);
    Help->SetPadding(FMargin(0.f));
    Collapsed(*Help);
    auto* Overlay = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_LightingColorHelp"));
    Help->AddChild(Overlay);
    auto* Open = Button(BP, TEXT("Button_LightingColorHelp"), TEXT("Text_LightingColorHelp"),
        NSLOCTEXT("FFLightingIntroduction", "Open", "조명 색상 변경"));
    Open->SetToolTipText(NSLOCTEXT("FFLightingIntroduction", "OpenTip",
        "색상환이나 미리 정해진 색으로 선택한 시설의 조명 색상을 바꿔 보세요."));
    auto* OpenSlot = Overlay->AddChildToOverlay(Open);
    OpenSlot->SetHorizontalAlignment(HAlign_Fill);
    OpenSlot->SetVerticalAlignment(VAlign_Fill);

    // The badge is a sibling, leaving the existing button/text contract intact.
    auto* Bounds = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_LightingIntroductionHint"));
    Bounds->SetWidthOverride(24.f);
    Bounds->SetHeightOverride(24.f);
    Bounds->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* HintSlot = Overlay->AddChildToOverlay(Bounds);
    HintSlot->SetHorizontalAlignment(HAlign_Right);
    HintSlot->SetVerticalAlignment(VAlign_Center);
    HintSlot->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
    auto* Hint = MakeWidget<UBorder>(BP, Tree, TEXT("Border_LightingIntroductionHint"), true);
    Hint->SetPadding(FMargin(0.f));
    Hint->SetHorizontalAlignment(HAlign_Center);
    Hint->SetVerticalAlignment(VAlign_Center);
    Collapsed(*Hint);
    Bounds->AddChild(Hint);
    auto* Mark = Text(BP, TEXT("Text_LightingIntroductionHint"), FText::GetEmpty(),
        FFBuildTypography::ERole::Heading, 16, FLinearColor::White);
    Mark->SetJustification(ETextJustify::Center);
    Hint->AddChild(Mark);
    StyleHint(*Hint, *Mark);
    return Help;
}

UBorder* MakeIntroduction(UWidgetBlueprint& BP)
{
    auto& Tree = *BP.WidgetTree;
    auto* Card = MakeWidget<UBorder>(BP, Tree, TEXT("Panel_LightingIntroduction"), true);
    Card->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Color(249, 249, 255), 18.f,
        FFBuildPearl::Line, 1.f));
    Card->SetBrushColor(FLinearColor::White);
    Card->SetPadding(FMargin(20.f));
    Collapsed(*Card);
    auto* Column = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Vertical_LightingIntroduction"));
    Card->AddChild(Column);
    auto* Title = Text(BP, TEXT("Text_LightingIntroductionTitle"),
        NSLOCTEXT("FFLightingIntroduction", "Title", "조명 색상을 바꿀 수 있어요"),
        FFBuildTypography::ERole::Heading, 21);
    Column->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
    const auto Step = [&BP, Column](const TCHAR* Name, const FText& Value)
    {
        auto* Label = Text(BP, Name, Value, FFBuildTypography::ERole::Body, 16);
        Column->AddChildToVerticalBox(Label)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    };
    Step(TEXT("Text_LightingIntroductionPick"), NSLOCTEXT("FFLightingIntroduction", "Pick",
        "1. 색상환이나 추천 색상을 눌러 원하는 색을 골라 주세요."));
    Step(TEXT("Text_LightingIntroductionPreview"), NSLOCTEXT("FFLightingIntroduction", "Preview",
        "2. 선택한 시설의 조명에 바뀐 색이 바로 미리 보여요."));
    Step(TEXT("Text_LightingIntroductionApply"), NSLOCTEXT("FFLightingIntroduction", "Apply",
        "3. 마음에 들면 ‘설정 적용’을 눌러 저장하세요."));
    auto* Restore = Text(BP, TEXT("Text_LightingIntroductionRestore"),
        NSLOCTEXT("FFLightingIntroduction", "Restore",
            "‘취소’는 편집 전 상태로, ‘기본값’은 시설의 기본 조명으로 되돌려요."),
        FFBuildTypography::ERole::Body, 15, FFBuildPearl::Muted);
    Column->AddChildToVerticalBox(Restore)->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
    auto* Bounds = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_DismissLightingIntroduction"));
    Bounds->SetMinDesiredHeight(46.f);
    Column->AddChildToVerticalBox(Bounds);
    Bounds->AddChild(Button(BP, TEXT("Button_DismissLightingIntroduction"),
        TEXT("Text_DismissLightingIntroduction"),
        NSLOCTEXT("FFLightingIntroduction", "Dismiss", "색상 바꾸기")));
    return Card;
}

bool AddIntroduction(UWidgetBlueprint& BP)
{
    if (Applied(BP)) return UpgradeHint(BP);
    auto& Tree = *BP.WidgetTree;
    auto* Content = Get<UVerticalBox>(Tree, TEXT("Panel_Content"));
    const TArray<UWidget*> OriginalChildren = Content->GetAllChildren();
    TArray<UPanelSlot*> OriginalSlots;
    for (UWidget* Child : OriginalChildren) OriginalSlots.Add(Child->Slot);
    BP.Modify();
    Tree.Modify();
    Content->Modify();
    // Only insert new slots. Existing widget trees, brushes and slots are retained.
    auto* GuideSlot = Cast<UVerticalBoxSlot>(Content->InsertChildAt(0, MakeIntroduction(BP)));
    auto* HelpSlot = Cast<UVerticalBoxSlot>(Content->InsertChildAt(1, MakeHelp(BP)));
    if (!GuideSlot || !HelpSlot) return false;
    GuideSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    HelpSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    for (int32 Index = 0; Index < OriginalChildren.Num(); ++Index)
        if (Content->GetChildAt(Index + 2) != OriginalChildren[Index] ||
            OriginalChildren[Index]->Slot != OriginalSlots[Index]) return false;
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    Metadata.SetValue(&BP, Version, TEXT("1"));
    Metadata.SetValue(&BP, BadgeVersion, TEXT("1"));
    if (Contract(BP) && SaveNewBlueprint(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, Version);
    Metadata.RemoveValue(&BP, BadgeVersion);
    return false;
}

void Apply()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry"))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL requires idle Entry editor"), Prefix);
        return;
    }
    auto* BP = LoadExistingBlueprint(FFBuildUIAssetPaths::FacilityInfoPanelPackage);
    if (!BP || !Contract(*BP) || !FFBuildTypography::Font(FFBuildTypography::ERole::Heading) ||
        !FFBuildTypography::Font(FFBuildTypography::ERole::Body))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL existing inspector widgets or Korean fonts unavailable"), Prefix);
        return;
    }
    const bool bSaved = AddIntroduction(*BP);
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_%s"), Prefix, bSaved ? TEXT("PASS") : TEXT("FAIL"));
}

FAutoConsoleCommand Command(TEXT("FF.ApplyFacilityLightingIntroduction"),
    TEXT("Add the first-use lighting color entry and guide once, preserving the authored inspector."),
    FConsoleCommandDelegate::CreateStatic(&Apply));
}
#endif
