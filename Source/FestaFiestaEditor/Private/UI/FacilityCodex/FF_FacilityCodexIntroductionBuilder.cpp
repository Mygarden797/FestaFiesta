// 첫 도감 안내에 필요한 위젯만 기존 WBP에 추가한다. 런타임에는 저장된 레이아웃을 사용한다.
#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Building/Presentation/FF_BuildTypography.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_BuildPearlStyle.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/FacilityCodex/FF_FacilityCodexLauncher.h"
#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace FFFacilityCodexIntroduction
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Version = TEXT("FF.FacilityCodex.Introduction20261007.1");
constexpr const TCHAR* Prefix = TEXT("FF_CODEX_INTRODUCTION");

template<class T> T* Get(UWidgetTree& Tree, const TCHAR* Name)
{
    return Cast<T>(Tree.FindWidget(Name));
}

bool Applied(UWidgetBlueprint& BP)
{
    return BP.GetOutermost()->GetMetaData().GetValue(&BP, Version) == TEXT("1");
}

bool NamesAvailable(UWidgetBlueprint& BP, std::initializer_list<const TCHAR*> Names)
{
    for (const TCHAR* Name : Names)
        if (BP.WidgetTree->FindWidget(Name))
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL %s: existing widget %s; no layout was replaced"),
                Prefix, *BP.GetName(), Name);
            return false;
        }
    return true;
}

bool LauncherContract(UWidgetBlueprint& BP)
{
    if (!BP.WidgetTree || !BP.WidgetTree->RootWidget || !BP.ParentClass ||
        !BP.ParentClass->IsChildOf(UFF_FacilityCodexLauncher::StaticClass())) return false;
    auto& Tree = *BP.WidgetTree;
    auto* Button = Get<UButton>(Tree, TEXT("Button_OpenCodex"));
    if (!Button || !Cast<UTextBlock>(Button->GetContent()) || !Button->GetParent() ||
        Button->GetParent()->GetChildIndex(Button) == INDEX_NONE) return false;
    if (Applied(BP))
        return Get<UBorder>(Tree, TEXT("Border_IntroductionHint")) &&
            Button->GetParent() == Get<UOverlay>(Tree, TEXT("Overlay_IntroductionLauncher"));
    return NamesAvailable(BP, {TEXT("Overlay_IntroductionLauncher"), TEXT("Size_IntroductionHint"),
        TEXT("Border_IntroductionHint"), TEXT("Text_IntroductionHint")});
}

bool ScreenContract(UWidgetBlueprint& BP)
{
    if (!BP.WidgetTree || !BP.WidgetTree->RootWidget || !BP.ParentClass ||
        !BP.ParentClass->IsChildOf(UFF_FacilityCodexScreen::StaticClass())) return false;
    if (Applied(BP))
        return Get<UBorder>(*BP.WidgetTree, TEXT("Border_Introduction")) &&
            Get<UButton>(*BP.WidgetTree, TEXT("Button_DismissIntroduction"));
    return NamesAvailable(BP, {TEXT("Overlay_IntroductionRoot"), TEXT("Border_Introduction"),
        TEXT("Size_IntroductionGuide"), TEXT("Border_IntroductionGuide"), TEXT("Vertical_IntroductionGuide"),
        TEXT("Text_IntroductionTitle"), TEXT("Text_IntroductionCard"), TEXT("Text_IntroductionDistance"),
        TEXT("Text_IntroductionRewards"), TEXT("Size_DismissIntroduction"), TEXT("Button_DismissIntroduction"),
        TEXT("Text_DismissIntroduction")});
}

UOverlaySlot* Fill(UOverlay& Overlay, UWidget& Widget)
{
    auto* Slot = Overlay.AddChildToOverlay(&Widget);
    Slot->SetHorizontalAlignment(HAlign_Fill);
    Slot->SetVerticalAlignment(VAlign_Fill);
    Slot->SetPadding(FMargin(0.f));
    return Slot;
}

UTextBlock* Text(UWidgetBlueprint& BP, const TCHAR* Name, const FText& Value,
    FFBuildTypography::ERole Role, int32 FontSize, FLinearColor Ink)
{
    auto* Label = MakeWidget<UTextBlock>(BP, *BP.WidgetTree, Name);
    Label->SetText(Value);
    Label->SetFont(FFBuildTypography::Make(Role, FontSize));
    Label->SetColorAndOpacity(FSlateColor(Ink));
    Label->SetVisibility(ESlateVisibility::HitTestInvisible);
    return Label;
}

bool Save(UWidgetBlueprint& BP)
{
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    Metadata.SetValue(&BP, Version, TEXT("1"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, Version);
    return false;
}

bool AddLauncherHint(UWidgetBlueprint& BP)
{
    if (Applied(BP)) return true;
    auto& Tree = *BP.WidgetTree;
    auto* Button = Get<UButton>(Tree, TEXT("Button_OpenCodex"));
    UPanelWidget* Parent = Button->GetParent();
    const int32 Index = Parent->GetChildIndex(Button);
    BP.Modify(); Tree.Modify(); Parent->Modify(); Button->Modify();
    if (Button->Slot) Button->Slot->Modify();
    auto* Overlay = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_IntroductionLauncher"));
    Overlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    // Keep the exact original parent slot and label-in-button relationship.
    // The wrapper only adds a sibling badge; no authored dimensions/styles change.
    if (!Parent->ReplaceChildAt(Index, Overlay)) return false;
    Button->Slot = nullptr;
    Fill(*Overlay, *Button);

    auto* Bounds = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_IntroductionHint"));
    Bounds->SetWidthOverride(18.f); Bounds->SetHeightOverride(18.f);
    Bounds->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* HintSlot = Overlay->AddChildToOverlay(Bounds);
    HintSlot->SetHorizontalAlignment(HAlign_Right); HintSlot->SetVerticalAlignment(VAlign_Top);
    // The 4px inset also contains the short native scale pulse inside the launcher.
    HintSlot->SetPadding(FMargin(0.f, 4.f, 4.f, 0.f));
    auto* Hint = MakeWidget<UBorder>(BP, Tree, TEXT("Border_IntroductionHint"), true);
    Hint->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Color(222, 54, 70), 9.f, FLinearColor::White, 1.f));
    Hint->SetBrushColor(FLinearColor::White); Hint->SetPadding(FMargin(0.f));
    Hint->SetHorizontalAlignment(HAlign_Center); Hint->SetVerticalAlignment(VAlign_Center);
    Hint->SetRenderTransformPivot(FVector2D(.5f, .5f));
    Hint->SetVisibility(ESlateVisibility::Collapsed); Hint->bHiddenInDesigner = true;
    Bounds->AddChild(Hint);
    auto* Label = Text(BP, TEXT("Text_IntroductionHint"), FText::FromString(TEXT("!")),
        FFBuildTypography::ERole::Heading, 13, FLinearColor::White);
    Label->SetJustification(ETextJustify::Center);
    Hint->AddChild(Label);
    return Save(BP);
}

void AddFullScreenLayer(UWidgetBlueprint& BP, UBorder& Layer)
{
    auto& Tree = *BP.WidgetTree;
    UWidget* Root = Tree.RootWidget;
    if (auto* Canvas = Cast<UCanvasPanel>(Root))
    {
        int32 TopZOrder = 0;
        for (UWidget* Child : Canvas->GetAllChildren())
            if (const auto* Slot = Cast<UCanvasPanelSlot>(Child->Slot))
                TopZOrder = FMath::Max(TopZOrder, Slot->GetZOrder());
        auto* Slot = StretchOnCanvas(*Canvas, Layer);
        Slot->SetZOrder(TopZOrder + 1);
    }
    else if (auto* Overlay = Cast<UOverlay>(Root)) Fill(*Overlay, Layer);
    else
    {
        // Wrapping preserves the entire previous root and all of its child slots.
        auto* RootOverlay = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_IntroductionRoot"));
        RootOverlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        Tree.RootWidget = RootOverlay;
        Fill(*RootOverlay, *Root);
        Fill(*RootOverlay, Layer);
    }
}

bool AddScreenIntroduction(UWidgetBlueprint& BP)
{
    if (Applied(BP)) return true;
    auto& Tree = *BP.WidgetTree;
    BP.Modify(); Tree.Modify(); Tree.RootWidget->Modify();
    auto* Layer = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Introduction"), true);
    Layer->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Color(30, 31, 45, .65f), 0.f));
    Layer->SetBrushColor(FLinearColor::White); Layer->SetPadding(FMargin(24.f));
    Layer->SetHorizontalAlignment(HAlign_Center); Layer->SetVerticalAlignment(VAlign_Center);
    // Native code uses Visible while presenting: the full-screen backdrop consumes
    // hit tests before the existing codex controls, while the guide button works.
    Layer->SetVisibility(ESlateVisibility::Collapsed); Layer->bHiddenInDesigner = true;
    AddFullScreenLayer(BP, *Layer);

    auto* Bounds = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_IntroductionGuide"));
    Bounds->SetWidthOverride(540.f);
    Layer->AddChild(Bounds);
    auto* Panel = MakeWidget<UBorder>(BP, Tree, TEXT("Border_IntroductionGuide"));
    Panel->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Color(249, 249, 255), 22.f,
        FFBuildPearl::Color(219, 222, 237), 1.f));
    Panel->SetBrushColor(FLinearColor::White); Panel->SetPadding(FMargin(32.f));
    Bounds->AddChild(Panel);
    auto* Column = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Vertical_IntroductionGuide"));
    Panel->AddChild(Column);
    auto* Title = Text(BP, TEXT("Text_IntroductionTitle"),
        NSLOCTEXT("FFCodexIntroduction", "Title", "시설 조합 도감을 확인해 보세요"),
        FFBuildTypography::ERole::Heading, 24, FFBuildPearl::Ink);
    Title->SetAutoWrapText(true);
    Column->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.f, 0.f, 0.f, 22.f));
    const auto AddStep = [&BP, Column](const TCHAR* Name, const FText& Value)
    {
        auto* Step = Text(BP, Name, Value, FFBuildTypography::ERole::Body, 17, FFBuildPearl::Ink);
        Step->SetAutoWrapText(true);
        Column->AddChildToVerticalBox(Step)->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
    };
    AddStep(TEXT("Text_IntroductionCard"), NSLOCTEXT("FFCodexIntroduction", "Card",
        "조합 카드를 선택해 짝 시설, 필요한 거리와 효과를 확인하세요."));
    AddStep(TEXT("Text_IntroductionDistance"), NSLOCTEXT("FFCodexIntroduction", "Distance",
        "조건에 맞는 거리로 시설을 배치하면 조합이 발견돼요."));
    AddStep(TEXT("Text_IntroductionRewards"), NSLOCTEXT("FFCodexIntroduction", "Rewards",
        "발견 보상과 수집 보상은 도감에서 직접 받아 주세요."));

    auto* ButtonBounds = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_DismissIntroduction"));
    ButtonBounds->SetHeightOverride(48.f);
    Column->AddChildToVerticalBox(ButtonBounds)->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
    auto* Dismiss = MakeWidget<UButton>(BP, Tree, TEXT("Button_DismissIntroduction"), true);
    FButtonStyle Style;
    Style.Normal = FFBuildPearl::Rounded(FFBuildPearl::Cyan, 12.f);
    Style.Hovered = FFBuildPearl::Rounded(FFBuildPearl::Color(175, 233, 255), 12.f);
    Style.Pressed = FFBuildPearl::Rounded(FFBuildPearl::Color(125, 204, 235), 12.f);
    Style.Disabled = Style.Normal;
    Style.NormalPadding = Style.PressedPadding = FMargin(16.f, 8.f);
    Dismiss->SetStyle(Style); Dismiss->SetCursor(EMouseCursor::Hand);
    ButtonBounds->AddChild(Dismiss);
    auto* Caption = Text(BP, TEXT("Text_DismissIntroduction"),
        NSLOCTEXT("FFCodexIntroduction", "Dismiss", "도감 살펴보기"),
        FFBuildTypography::ERole::Heading, 18, FFBuildPearl::Ink);
    Caption->SetJustification(ETextJustify::Center);
    Dismiss->AddChild(Caption);
    return Save(BP);
}

void Apply()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry"))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL requires idle Entry editor"), Prefix);
        return;
    }
    auto* Launcher = LoadExistingBlueprint(FFBuildUIAssetPaths::FacilityCodexLauncherPackage);
    auto* CatalogLauncher = LoadExistingBlueprint(FFBuildUIAssetPaths::FacilityCodexCatalogLauncherPackage);
    auto* Screen = LoadExistingBlueprint(FFBuildUIAssetPaths::FacilityCodexScreenPackage);
    // Preflight all three targets before touching or saving any authored layout.
    if (!Launcher || !CatalogLauncher || !Screen || !LauncherContract(*Launcher) ||
        !LauncherContract(*CatalogLauncher) || !ScreenContract(*Screen) ||
        !FFBuildTypography::Font(FFBuildTypography::ERole::Heading) ||
        !FFBuildTypography::Font(FFBuildTypography::ERole::Body))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL existing codex widgets or Korean fonts unavailable"), Prefix);
        return;
    }
    const bool bSaved = AddLauncherHint(*Launcher) && AddLauncherHint(*CatalogLauncher) && AddScreenIntroduction(*Screen);
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_%s"), Prefix, bSaved ? TEXT("PASS") : TEXT("FAIL"));
}

FAutoConsoleCommand Command(TEXT("FF.ApplyFacilityCodexIntroduction"),
    TEXT("Add editable first-use codex badges and introduction once while preserving existing authored widgets."),
    FConsoleCommandDelegate::CreateStatic(&Apply));
}
#endif
