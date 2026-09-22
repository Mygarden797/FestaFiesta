// Saved presentation only: authoritative discovery and wallet logic remain native.
#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace FFFacilityCodexRewardAffordance
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Root = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/");
constexpr const TCHAR* Version = TEXT("FF.FacilityCodex.RewardAffordance20260921.1");
template<class T> T* Get(UWidgetTree& Tree, const TCHAR* Name) { return Cast<T>(Tree.FindWidget(Name)); }
static FLinearColor Color(uint8 R, uint8 G, uint8 B)
{ return FLinearColor::FromSRGBColor(FColor(R, G, B)); }
static FSlateBrush Rounded(FLinearColor Fill, FLinearColor Line, float Width, float Radius)
{
    FSlateBrush Brush;
    Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
    Brush.TintColor = Fill;
    Brush.OutlineSettings = FSlateBrushOutlineSettings(Radius, FSlateColor(Line), Width);
    return Brush;
}
static void Label(UWidgetTree& Tree, const TCHAR* Name, FLinearColor Ink)
{
    if (auto* Text = Get<UTextBlock>(Tree, Name))
    {
        Text->SetColorAndOpacity(FSlateColor(Ink));
        Text->SetShadowOffset(FVector2D::ZeroVector);
    }
}
static bool Contract(UWidgetBlueprint& BP, bool Screen)
{
    if (!BP.WidgetTree || !BP.GeneratedClass || BP.GetOutermost()->GetMetaData().GetValue(
        &BP, TEXT("FF.FacilityCodex.White20260921.3")) != TEXT("1")) return false;
    auto& Tree = *BP.WidgetTree;
    const FString Prefix = Screen ? TEXT("Detail") : TEXT("");
    if (!Get<UButton>(Tree, *(TEXT("Button_") + Prefix + TEXT("ClaimReward"))) ||
        !Get<UBorder>(Tree, *(TEXT("Border_") + Prefix + TEXT("RewardClaimed"))) ||
        !Get<UBorder>(Tree, *(TEXT("Border_") + Prefix + TEXT("RewardLocked"))) ||
        !Get<UImage>(Tree, *(TEXT("Image_") + Prefix + TEXT("RewardLocked"))) ||
        !Get<UImage>(Tree, *(TEXT("Image_") + Prefix + TEXT("RewardClaimed")))) return false;
    auto* Bounds = Get<USizeBox>(Tree, Screen ? TEXT("Size_DetailRewardState") : TEXT("Size_RewardState"));
    if (!Bounds || Bounds->GetWidthOverride() != 72.f || Bounds->GetHeightOverride() != 32.f) return false;
    return !Screen || (Get<UOverlay>(Tree, TEXT("Overlay_GlassScreen")) &&
        Get<UVerticalBox>(Tree, TEXT("Vertical_ShowcaseRewardInfo")) &&
        Get<UTextBlock>(Tree, TEXT("Text_DetailRewardLabel")));
}
static void StyleStates(UWidgetBlueprint& BP, bool Screen, UMaterialInterface* LockedGift)
{
    auto& Tree = *BP.WidgetTree;
    const auto& Palette = UFF_UIPalette::Get()->InGame;
    const FString Prefix = Screen ? TEXT("Detail") : TEXT("");
    Label(Tree, *(TEXT("Text_") + Prefix + TEXT("ClaimReward")), Color(83, 49, 10));
    Label(Tree, *(TEXT("Text_") + Prefix + TEXT("RewardClaimed")), Palette.TealDeep);
    Label(Tree, *(TEXT("Text_") + Prefix + TEXT("RewardLocked")), Color(88, 99, 92));
    Get<UImage>(Tree, *(TEXT("Image_") + Prefix + TEXT("RewardClaimed")))->SetColorAndOpacity(Palette.TealDeep);
    auto* LockedIcon = Get<UImage>(Tree, *(TEXT("Image_") + Prefix + TEXT("RewardLocked")));
    LockedIcon->SetBrushFromMaterial(LockedGift);
    LockedIcon->SetDesiredSizeOverride(FVector2D(24.f));
    LockedIcon->SetColorAndOpacity(FLinearColor::White);
    LockedIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* Locked = Get<UBorder>(Tree, *(TEXT("Border_") + Prefix + TEXT("RewardLocked")));
    Locked->SetBrush(Rounded(Palette.PaperSunken, Palette.LineStrong, 1.f, 7.5f));
    Locked->SetBrushColor(FLinearColor::White);
}
static void AddScreenFeedback(UWidgetBlueprint& BP)
{
    auto& Tree = *BP.WidgetTree;
    const auto& Palette = UFF_UIPalette::Get()->InGame;
    auto Font = Get<UTextBlock>(Tree, TEXT("Text_DetailRewardLabel"))->GetFont();
    auto* RewardColumn = Get<UVerticalBox>(Tree, TEXT("Vertical_ShowcaseRewardInfo"));
    if (!Tree.FindWidget(TEXT("Text_DetailRewardHint")))
    {
        auto* Hint = MakeWidget<UTextBlock>(BP, Tree, TEXT("Text_DetailRewardHint"), true);
        Font.Size = 12;
        Hint->SetFont(Font);
        Hint->SetColorAndOpacity(FSlateColor(Color(78, 94, 85)));
        Hint->SetText(FText::FromString(TEXT("조합을 발견하면 받을 수 있어요")));
        Hint->SetAutoWrapText(true);
        Hint->SetVisibility(ESlateVisibility::HitTestInvisible);
        RewardColumn->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0, 0, 0, 10));
        RewardColumn->ShiftChild(3, Hint);
    }
    // Optional native bindings must also be designer variables after compilation.
    Get<UTextBlock>(Tree, TEXT("Text_DetailRewardLabel"))->bIsVariable = true;
    if (Tree.FindWidget(TEXT("Border_RewardFeedback"))) return;
    auto* Bounds = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_RewardFeedback"));
    Bounds->SetWidthOverride(440.f); Bounds->SetHeightOverride(48.f);
    Bounds->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    auto* ToastSlot = Get<UOverlay>(Tree, TEXT("Overlay_GlassScreen"))->AddChildToOverlay(Bounds);
    ToastSlot->SetHorizontalAlignment(HAlign_Center); ToastSlot->SetVerticalAlignment(VAlign_Bottom);
    ToastSlot->SetPadding(FMargin(0, 0, 0, 24));
    auto* Toast = MakeWidget<UBorder>(BP, Tree, TEXT("Border_RewardFeedback"), true);
    Toast->SetBrush(Rounded(Palette.Paper, Palette.Teal, 1.8f, 12.f));
    Toast->SetBrushColor(FLinearColor::White); Toast->SetPadding(FMargin(16, 8));
    Toast->SetVisibility(ESlateVisibility::Collapsed);
    Toast->SetHorizontalAlignment(HAlign_Center); Toast->SetVerticalAlignment(VAlign_Center);
    Bounds->AddChild(Toast);
    auto* Row = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Horizontal_RewardFeedback"));
    Toast->AddChild(Row);
    auto* Check = MakeWidget<UImage>(BP, Tree, TEXT("Image_RewardFeedbackCheck"));
    Check->SetBrush(Get<UImage>(Tree, TEXT("Image_DetailRewardClaimed"))->GetBrush());
    Check->SetColorAndOpacity(Palette.TealDeep); Check->SetDesiredSizeOverride(FVector2D(24.f));
    Row->AddChildToHorizontalBox(Check)->SetVerticalAlignment(VAlign_Center);
    auto* Message = MakeWidget<UTextBlock>(BP, Tree, TEXT("Text_RewardFeedback"), true);
    Font.Size = 14; Message->SetFont(Font);
    Message->SetColorAndOpacity(FSlateColor(Palette.TealDeep));
    Message->SetText(FText::GetEmpty()); Message->SetVisibility(ESlateVisibility::Hidden);
    auto* MessageSlot = Row->AddChildToHorizontalBox(Message);
    MessageSlot->SetPadding(FMargin(10, 0, 0, 0)); MessageSlot->SetVerticalAlignment(VAlign_Center);
}
static bool Save(UWidgetBlueprint& BP, bool Screen, UMaterialInterface* LockedGift)
{
    auto& Meta = BP.GetOutermost()->GetMetaData();
    if (Meta.GetValue(&BP, Version) == TEXT("1")) return true;
    BP.Modify(); BP.WidgetTree->Modify();
    StyleStates(BP, Screen, LockedGift);
    if (Screen) AddScreenFeedback(BP);
    Meta.SetValue(&BP, Version, TEXT("1"));
    if (SaveNewBlueprint(TEXT("FF_CODEX_REWARD_AFFORDANCE"), BP)) return true;
    Meta.RemoveValue(&BP, Version); return false;
}
static void Run()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry")) return;
    auto* Screen = LoadExistingBlueprint(FString(Root) + TEXT("WBP_FF_FacilityCodexScreen"));
    auto* Card = LoadExistingBlueprint(FString(Root) + TEXT("WBP_FF_FacilityCodexCard"));
    const FString GiftPath = FString(Root) + TEXT("Materials/MI_FF_CodexReward_LockedGift.MI_FF_CodexReward_LockedGift");
    auto* LockedGift = LoadObject<UMaterialInterface>(nullptr, *GiftPath);
    if (!Screen || !Card || !LockedGift || !LockedGift->IsUIMaterial() || !Contract(*Screen, true) || !Contract(*Card, false))
    { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_CODEX_REWARD_AFFORDANCE_FAIL: expected white codex and grayscale gift material")); return; }
    const bool bSaved = Save(*Screen, true, LockedGift) && Save(*Card, false, LockedGift);
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_CODEX_REWARD_AFFORDANCE_%s"), bSaved ? TEXT("PASS") : TEXT("FAIL"));
}
static FAutoConsoleCommand Command(TEXT("FF.ApplyFacilityCodexRewardAffordance"),
    TEXT("Clarify ready, claimed and locked rewards and author unobtrusive success feedback."),
    FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
