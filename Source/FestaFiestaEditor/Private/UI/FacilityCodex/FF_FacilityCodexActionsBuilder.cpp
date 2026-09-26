// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// FF_FacilityCodexActionsBuilder.cpp
// 역할: 도감의 기존 화면에 행동 버튼 연결 지점을 추가한다.

// Explicit editor migration: retain the authored codex and add only its action seams.
#include "CoreMinimal.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"

#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/FacilityCodex/FF_FacilityCodexCard.h"
#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FFFacilityCodexActionsBuilder"

namespace FFFacilityCodexActionsBuilder
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Prefix = TEXT("FF_FACILITY_CODEX_ACTIONS");
constexpr const TCHAR* Version = TEXT("FF.FacilityCodex.Actions20260921.1");
constexpr const TCHAR* IconVersion = TEXT("FF.FacilityCodex.RewardIcon20260921.1");
constexpr const TCHAR* StatusVersion = TEXT("FF.FacilityCodex.RewardStatus20260921.1");
constexpr const TCHAR* IridescenceVersion = TEXT("FF.FacilityCodex.RewardIridescence20260921.1");
constexpr const TCHAR* RewardBoundsVersion = TEXT("FF.FacilityCodex.RewardBounds20260921.1");
constexpr const TCHAR* RewardHintVersion = TEXT("FF.FacilityCodex.RewardHint20260921.1");
constexpr const TCHAR* IconPath = FFBuildUIAssetPaths::T_FF_Codex_RewardGiftObject;
constexpr const TCHAR* CardPath = FFBuildUIAssetPaths::FacilityCodexCardPackage;
constexpr const TCHAR* ScreenPath = FFBuildUIAssetPaths::FacilityCodexScreenPackage;

template<class T> static T* Get(UWidgetTree& Tree, const TCHAR* Name)
{ return Cast<T>(Tree.FindWidget(FName(Name))); }

static FLinearColor Color(uint8 R, uint8 G, uint8 B)
{ return FLinearColor::FromSRGBColor(FColor(R, G, B)); }

static FSlateBrush Brush(FLinearColor Fill, FLinearColor Outline)
{
    FSlateBrush Result;
    Result.DrawAs = ESlateBrushDrawType::RoundedBox;
    Result.TintColor = FSlateColor(Fill);
    Result.OutlineSettings = FSlateBrushOutlineSettings(5.f, FSlateColor(Outline), 1.f);
    return Result;
}

static void StyleAction(UButton& Button, bool bCatalog)
{
    const FLinearColor Ink = Color(32, 76, 54), Forest = Color(42, 87, 65),
        Sage = Color(226, 236, 221), Line = Color(195, 212, 191);
    FButtonStyle Style;
    Style.Normal = Brush(bCatalog ? FLinearColor::Transparent : Forest,
        bCatalog ? FLinearColor::Transparent : Forest);
    Style.Hovered = Brush(bCatalog ? Sage : Color(52, 107, 78), Forest);
    Style.Pressed = Brush(bCatalog ? Color(210, 228, 202) : Ink, Ink);
    Style.Disabled = Brush(Sage, Line);
    // The catalog wrappers add no space around the existing ingredient compositions.
    Style.NormalPadding = Style.PressedPadding = bCatalog ? FMargin(0) : FMargin(6, 4);
    Button.SetStyle(Style);
    Button.SetCursor(EMouseCursor::Hand);
    Button.SetToolTipText(bCatalog
        ? LOCTEXT("CatalogTooltip", "건축 카탈로그에서 확인")
        : LOCTEXT("RewardTooltip", "발견 보상 받기"));
}

static bool IsApplied(UWidgetBlueprint& BP)
{ return BP.GetOutermost()->GetMetaData().GetValue(&BP, Version) == TEXT("1"); }

static bool ScreenContract(UWidgetBlueprint& BP, bool bApplied)
{
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Ingredients = Get<UHorizontalBox>(Tree, TEXT("Horizontal_Ingredients"));
    if (!Ingredients || Ingredients->GetChildrenCount() != 3 ||
        !Get<USizeBox>(Tree, TEXT("Size_Dialog")) || !Get<USizeBox>(Tree, TEXT("Size_Detail")) ||
        !Get<UFF_FacilityCodexTileView>(Tree, TEXT("TileView_Entries"))) return false;
    for (const FString Side : {FString(TEXT("Target")), FString(TEXT("Partner"))})
    {
        auto* Ingredient = Get<UVerticalBox>(Tree, *(TEXT("Vertical_") + Side));
        auto* Thumb = Get<USizeBox>(Tree, *(TEXT("Size_Thumb") + Side));
        auto* Label = Get<UTextBlock>(Tree, *(TEXT("Text_") + Side + TEXT("Name")));
        UWidget* Existing = Tree.FindWidget(FName(TEXT("Button_") + Side + TEXT("Catalog")));
        if (!Ingredient || !Thumb || !Label || Ingredient->GetChildrenCount() != 2 ||
            Thumb->GetParent() != Ingredient || Label->GetParent() != Ingredient) return false;
        if (bApplied)
        {
            auto* Button = Cast<UButton>(Existing);
            if (!Button || !Button->bIsVariable || Button->GetParent() != Ingredients ||
                Button->GetContent() != Ingredient) return false;
        }
        else if (Existing || Ingredient->GetParent() != Ingredients || !Cast<UHorizontalBoxSlot>(Ingredient->Slot)) return false;
    }
    return true;
}

static bool CardContract(UWidgetBlueprint& BP, bool bApplied)
{
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Column = Get<UVerticalBox>(Tree, TEXT("Vertical_Card"));
    auto* Bonus = Get<UTextBlock>(Tree, TEXT("Text_Bonus"));
    if (!Column || !Bonus || !Get<USizeBox>(Tree, TEXT("Size_Card")) ||
        !Tree.FindWidget(TEXT("Border_State"))) return false;
    if (!bApplied)
    {
        return Bonus->GetParent() == Column && Cast<UVerticalBoxSlot>(Bonus->Slot) &&
            !Tree.FindWidget(TEXT("Scale_RewardActions")) && !Tree.FindWidget(TEXT("Horizontal_RewardActions")) &&
            !Tree.FindWidget(TEXT("Button_ClaimReward")) && !Tree.FindWidget(TEXT("Text_ClaimReward"));
    }
    auto* Scale = Get<UScaleBox>(Tree, TEXT("Scale_RewardActions"));
    auto* Row = Get<UHorizontalBox>(Tree, TEXT("Horizontal_RewardActions"));
    auto* Button = Get<UButton>(Tree, TEXT("Button_ClaimReward"));
    auto* Label = Get<UTextBlock>(Tree, TEXT("Text_ClaimReward"));
    auto* RewardBounds = Get<USizeBox>(Tree, TEXT("Size_RewardState"));
    auto* RewardStates = Get<UOverlay>(Tree, TEXT("Overlay_RewardState"));
    const bool bSharedRewardBounds = Row && RewardBounds && RewardStates &&
        Row->GetChildrenCount() == 2 && Row->GetChildAt(1) == RewardBounds &&
        RewardBounds->GetContent() == RewardStates &&
        (RewardStates->GetChildrenCount() == 2 || (RewardStates->GetChildrenCount() == 3 &&
            RewardStates->GetChildAt(2) == Get<UBorder>(Tree, TEXT("Border_RewardLocked")))) &&
        RewardStates->GetChildAt(0) == Button && RewardStates->GetChildAt(1) == Get<UBorder>(Tree, TEXT("Border_RewardClaimed"));
    return Scale && Row && Button && Label && Button->bIsVariable && Label->bIsVariable &&
        Scale->GetParent() == Column && Scale->GetContent() == Row &&
        (Row->GetChildrenCount() == 2 || (Row->GetChildrenCount() == 3 && Get<UBorder>(Tree, TEXT("Border_RewardClaimed")))) &&
        Row->GetChildAt(0) == Bonus && (Row->GetChildAt(1) == Button || bSharedRewardBounds) &&
        (Button->GetContent() == Label || (Get<UOverlay>(Tree, TEXT("Overlay_ClaimReward")) &&
            Button->GetContent() == Get<UOverlay>(Tree, TEXT("Overlay_ClaimReward")) &&
            (Label->GetParent() == Button->GetContent() || Label->GetParent() == Get<UHorizontalBox>(Tree, TEXT("Horizontal_RewardContent"))) &&
            Get<UImage>(Tree, TEXT("Image_ClaimReward"))));
}

static void EditScreen(UWidgetBlueprint& BP)
{
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Ingredients = Get<UHorizontalBox>(Tree, TEXT("Horizontal_Ingredients"));
    for (const FString Side : {FString(TEXT("Target")), FString(TEXT("Partner"))})
    {
        auto* Ingredient = Get<UVerticalBox>(Tree, *(TEXT("Vertical_") + Side));
        auto* OldSlot = CastChecked<UHorizontalBoxSlot>(Ingredient->Slot);
        const int32 Index = Ingredients->GetChildIndex(Ingredient);
        const FMargin Padding = OldSlot->GetPadding();
        const FSlateChildSize Size = OldSlot->GetSize();
        const EHorizontalAlignment HAlign = OldSlot->GetHorizontalAlignment();
        const EVerticalAlignment VAlign = OldSlot->GetVerticalAlignment();
        Ingredient->RemoveFromParent();
        auto* Button = MakeWidget<UButton>(BP, Tree, FName(TEXT("Button_") + Side + TEXT("Catalog")), true);
        StyleAction(*Button, true);
        auto* Slot = Ingredients->AddChildToHorizontalBox(Button);
        Slot->SetPadding(Padding); Slot->SetSize(Size);
        Slot->SetHorizontalAlignment(HAlign); Slot->SetVerticalAlignment(VAlign);
        Ingredients->ShiftChild(Index, Button);
        auto* ContentSlot = CastChecked<UButtonSlot>(Button->AddChild(Ingredient));
        ContentSlot->SetPadding(FMargin(0));
        ContentSlot->SetHorizontalAlignment(HAlign_Fill); ContentSlot->SetVerticalAlignment(VAlign_Fill);
        // Both the thumbnail and its name route input to this one catalog action.
        Ingredient->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
}

static void EditCard(UWidgetBlueprint& BP)
{
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Column = Get<UVerticalBox>(Tree, TEXT("Vertical_Card"));
    auto* Bonus = Get<UTextBlock>(Tree, TEXT("Text_Bonus"));
    auto* OldSlot = CastChecked<UVerticalBoxSlot>(Bonus->Slot);
    const int32 Index = Column->GetChildIndex(Bonus);
    const FMargin Padding = OldSlot->GetPadding();
    const FSlateChildSize Size = OldSlot->GetSize();
    const EHorizontalAlignment HAlign = OldSlot->GetHorizontalAlignment();
    const EVerticalAlignment VAlign = OldSlot->GetVerticalAlignment();
    Bonus->RemoveFromParent();
    auto* Scale = MakeWidget<UScaleBox>(BP, Tree, TEXT("Scale_RewardActions"));
    Scale->SetStretch(EStretch::ScaleToFit); Scale->SetStretchDirection(EStretchDirection::DownOnly);
    auto* Slot = Column->AddChildToVerticalBox(Scale);
    Slot->SetPadding(Padding); Slot->SetSize(Size);
    Slot->SetHorizontalAlignment(HAlign); Slot->SetVerticalAlignment(VAlign);
    Column->ShiftChild(Index, Scale);
    auto* Row = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Horizontal_RewardActions"));
    auto* ScaleSlot = CastChecked<UScaleBoxSlot>(Scale->AddChild(Row));
    ScaleSlot->SetHorizontalAlignment(HAlign_Left); ScaleSlot->SetVerticalAlignment(VAlign_Center);
    auto* BonusSlot = Row->AddChildToHorizontalBox(Bonus);
    BonusSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    BonusSlot->SetVerticalAlignment(VAlign_Center);
    Bonus->SetAutoWrapText(false);
    auto* Button = MakeWidget<UButton>(BP, Tree, TEXT("Button_ClaimReward"), true);
    StyleAction(*Button, false);
    // Runtime presents this only for a discovered recipe and updates the claimed state.
    Button->SetVisibility(ESlateVisibility::Collapsed);
    auto* ButtonSlot = Row->AddChildToHorizontalBox(Button);
    ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    ButtonSlot->SetPadding(FMargin(6, 0, 0, 0)); ButtonSlot->SetVerticalAlignment(VAlign_Center);
    auto* Label = MakeWidget<UTextBlock>(BP, Tree, TEXT("Text_ClaimReward"), true);
    // Inherit the actual saved font asset, including its Korean glyph coverage.
    FSlateFontInfo Font = Bonus->GetFont(); Font.Size = 12; Label->SetFont(Font);
    Label->SetColorAndOpacity(FSlateColor(Color(252, 254, 249)));
    Label->SetText(LOCTEXT("ClaimReward", "보상 받기"));
    Label->SetAutoWrapText(false); Label->SetJustification(ETextJustify::Center);
    Label->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* ContentSlot = CastChecked<UButtonSlot>(Button->AddChild(Label));
    ContentSlot->SetPadding(FMargin(0));
    ContentSlot->SetHorizontalAlignment(HAlign_Center); ContentSlot->SetVerticalAlignment(VAlign_Center);
}

static bool Save(UWidgetBlueprint& BP, bool bCard)
{
    if (IsApplied(BP)) return true;
    BP.Modify(); BP.WidgetTree->Modify();
    if (bCard) EditCard(BP); else EditScreen(BP);
    if (!(bCard ? CardContract(BP, true) : ScreenContract(BP, true))) return false;
    auto& Meta = BP.GetOutermost()->GetMetaData();
    Meta.SetValue(&BP, Version, TEXT("1"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    Meta.RemoveValue(&BP, Version);
    return false;
}

static void Run()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry"))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_COMPLETE_FAIL: requires idle /Engine/Maps/Entry."), Prefix);
        return;
    }
    UWidgetBlueprint* Card = LoadExistingBlueprint(CardPath);
    UWidgetBlueprint* Screen = LoadExistingBlueprint(ScreenPath);
    // Validate both assets before changing either; partial or unexpected trees fail closed.
    if (!Card || !Screen || !Card->WidgetTree || !Screen->WidgetTree || !Card->ParentClass || !Screen->ParentClass ||
        !Card->ParentClass->IsChildOf(UFF_FacilityCodexCard::StaticClass()) ||
        !Screen->ParentClass->IsChildOf(UFF_FacilityCodexScreen::StaticClass()) ||
        !CardContract(*Card, IsApplied(*Card)) || !ScreenContract(*Screen, IsApplied(*Screen)))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_COMPLETE_FAIL: unexpected codex widget contract; no assets changed."), Prefix);
        return;
    }
    const bool bSaved = Save(*Card, true) && Save(*Screen, false);
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_COMPLETE_%s"), Prefix, bSaved ? TEXT("PASS") : TEXT("FAIL"));
}

static FAutoConsoleCommand Command(TEXT("FF.ApplyFacilityCodexActions"),
    TEXT("Add codex ingredient catalog links and a discovery reward action beside each card fee, preserving the saved layout."),
    FConsoleCommandDelegate::CreateStatic(&Run));

static void RunRewardIcon()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry")) return;
    UWidgetBlueprint* BP = LoadExistingBlueprint(CardPath);
    UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, IconPath);
    if (!BP || !BP->WidgetTree || !Texture || !CardContract(*BP, true))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_CODEX_REWARD_ICON_FAIL: requires migrated card and imported gift texture."));
        return;
    }
    UWidgetTree& Tree = *BP->WidgetTree;
    if (BP->GetOutermost()->GetMetaData().GetValue(BP, IconVersion) == TEXT("1"))
    {
        auto* Icon = Get<UImage>(Tree, TEXT("Image_ClaimReward"));
        if (Icon && Icon->GetBrush().GetResourceObject() == Texture)
        {
            UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_CODEX_REWARD_ICON_PASS: already applied."));
        }
        else
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_CODEX_REWARD_ICON_FAIL: saved icon contract is invalid."));
        }
        return;
    }
    auto* Button = Get<UButton>(Tree, TEXT("Button_ClaimReward"));
    auto* Label = Get<UTextBlock>(Tree, TEXT("Text_ClaimReward"));
    if (Button->GetContent() != Label || Tree.FindWidget(TEXT("Image_ClaimReward"))) return;
    BP->Modify(); Tree.Modify();
    Label->RemoveFromParent();
    auto* Overlay = MakeWidget<UOverlay>(*BP, Tree, TEXT("Overlay_ClaimReward"));
    auto* ContentSlot = CastChecked<UButtonSlot>(Button->AddChild(Overlay));
    ContentSlot->SetPadding(FMargin(2));
    ContentSlot->SetHorizontalAlignment(HAlign_Center); ContentSlot->SetVerticalAlignment(VAlign_Center);
    auto* Size = MakeWidget<USizeBox>(*BP, Tree, TEXT("Size_ClaimRewardIcon"));
    Size->SetWidthOverride(28.f); Size->SetHeightOverride(28.f);
    Overlay->AddChildToOverlay(Size);
    auto* Icon = MakeWidget<UImage>(*BP, Tree, TEXT("Image_ClaimReward"), true);
    Icon->SetBrushFromTexture(Texture);
    Icon->SetDesiredSizeOverride(FVector2D(28.f));
    Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
    Size->AddChild(Icon);
    // Retain the legacy binding as a hidden fallback so older saved cards remain compatible.
    Overlay->AddChildToOverlay(Label);
    Label->SetVisibility(ESlateVisibility::Collapsed);
    Overlay->SetVisibility(ESlateVisibility::HitTestInvisible);
    FButtonStyle Style = Button->GetStyle();
    Style.Normal = Brush(FLinearColor::Transparent, FLinearColor::Transparent);
    Style.Hovered = Brush(Color(226, 236, 221), Color(195, 212, 191));
    Style.Pressed = Brush(Color(210, 228, 202), Color(42, 87, 65));
    Style.Disabled = Style.Normal;
    Style.NormalPadding = Style.PressedPadding = FMargin(0);
    Button->SetStyle(Style);
    BP->GetOutermost()->GetMetaData().SetValue(BP, IconVersion, TEXT("1"));
    if (SaveNewBlueprint(TEXT("FF_CODEX_REWARD_ICON"), *BP))
    {
        UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_CODEX_REWARD_ICON_PASS"));
    }
    else { BP->GetOutermost()->GetMetaData().RemoveValue(BP, IconVersion); }
}

static FAutoConsoleCommand RewardIconCommand(TEXT("FF.ApplyFacilityCodexRewardIcon"),
    TEXT("Replace the saved codex reward caption with its gift icon while keeping its existing claim action."),
    FConsoleCommandDelegate::CreateStatic(&RunRewardIcon));

static void RunRewardStatus()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry")) return;
    UWidgetBlueprint* BP = LoadExistingBlueprint(CardPath);
    UTexture2D* Check = LoadObject<UTexture2D>(nullptr, TEXT(
        "/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Status/T_FF_LH_Icon_StatusCheck_White_256.T_FF_LH_Icon_StatusCheck_White_256"));
    if (!BP || !BP->WidgetTree || !Check || !CardContract(*BP, true)) return;
    auto& Meta = BP->GetOutermost()->GetMetaData();
    if (Meta.GetValue(BP, StatusVersion) == TEXT("1")) return;
    UWidgetTree& Tree = *BP->WidgetTree;
    auto* Row = Get<UHorizontalBox>(Tree, TEXT("Horizontal_RewardActions"));
    auto* Overlay = Get<UOverlay>(Tree, TEXT("Overlay_ClaimReward"));
    auto* GiftSize = Get<USizeBox>(Tree, TEXT("Size_ClaimRewardIcon"));
    auto* Gift = Get<UImage>(Tree, TEXT("Image_ClaimReward"));
    auto* Button = Get<UButton>(Tree, TEXT("Button_ClaimReward"));
    auto* Label = Get<UTextBlock>(Tree, TEXT("Text_ClaimReward"));
    if (!Overlay || !GiftSize || !Gift || GiftSize->GetParent() != Overlay || Label->GetParent() != Overlay) return;
    BP->Modify(); Tree.Modify();
    GiftSize->RemoveFromParent(); Label->RemoveFromParent();
    auto* Content = MakeWidget<UHorizontalBox>(*BP, Tree, TEXT("Horizontal_RewardContent"));
    Overlay->AddChildToOverlay(Content);
    GiftSize->SetWidthOverride(24.f); GiftSize->SetHeightOverride(24.f);
    Gift->SetDesiredSizeOverride(FVector2D(24.f));
    Content->AddChildToHorizontalBox(GiftSize)->SetVerticalAlignment(VAlign_Center);
    auto* LabelSlot = Content->AddChildToHorizontalBox(Label);
    LabelSlot->SetVerticalAlignment(VAlign_Center); LabelSlot->SetPadding(FMargin(3, 0, 0, 0));
    Label->SetText(LOCTEXT("RewardReady", "받기"));
    Label->SetVisibility(ESlateVisibility::HitTestInvisible);
    Label->SetColorAndOpacity(FSlateColor(Color(102, 65, 12)));
    CastChecked<UButtonSlot>(Overlay->Slot)->SetPadding(FMargin(5, 2));
    FButtonStyle Style = Button->GetStyle();
    Style.Normal = Brush(Color(255, 239, 181), Color(225, 183, 63));
    Style.Hovered = Brush(Color(255, 224, 132), Color(196, 146, 35));
    Style.Pressed = Brush(Color(246, 210, 113), Color(164, 119, 30));
    Button->SetStyle(Style);
    auto* Received = MakeWidget<UBorder>(*BP, Tree, TEXT("Border_RewardClaimed"), true);
    Received->SetBrush(Brush(Color(221, 240, 225), Color(164, 199, 173)));
    Received->SetBrushColor(FLinearColor::White); Received->SetPadding(FMargin(5, 2));
    Received->SetToolTipText(LOCTEXT("RewardReceived", "발견 보상 수령 완료"));
    Received->SetVisibility(ESlateVisibility::Collapsed);
    auto* ReceivedSlot = Row->AddChildToHorizontalBox(Received);
    ReceivedSlot->SetPadding(FMargin(6, 0, 0, 0)); ReceivedSlot->SetVerticalAlignment(VAlign_Center);
    auto* ReceivedContent = MakeWidget<UHorizontalBox>(*BP, Tree, TEXT("Horizontal_RewardClaimed"));
    Received->AddChild(ReceivedContent);
    ReceivedContent->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* CheckImage = MakeWidget<UImage>(*BP, Tree, TEXT("Image_RewardClaimed"), true);
    CheckImage->SetBrushFromTexture(Check); CheckImage->SetDesiredSizeOverride(FVector2D(24.f));
    CheckImage->SetColorAndOpacity(Color(32, 99, 59));
    ReceivedContent->AddChildToHorizontalBox(CheckImage)->SetVerticalAlignment(VAlign_Center);
    auto* Completed = MakeWidget<UTextBlock>(*BP, Tree, TEXT("Text_RewardClaimed"));
    Completed->SetFont(Label->GetFont()); Completed->SetColorAndOpacity(FSlateColor(Color(32, 99, 59)));
    Completed->SetText(LOCTEXT("RewardComplete", "완료")); Completed->SetAutoWrapText(false);
    auto* CompleteSlot = ReceivedContent->AddChildToHorizontalBox(Completed);
    CompleteSlot->SetPadding(FMargin(3, 0, 0, 0)); CompleteSlot->SetVerticalAlignment(VAlign_Center);
    Meta.SetValue(BP, StatusVersion, TEXT("1"));
    if (SaveNewBlueprint(TEXT("FF_CODEX_REWARD_STATUS"), *BP))
    {
        UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_CODEX_REWARD_STATUS_PASS"));
    }
    else { Meta.RemoveValue(BP, StatusVersion); }
}

static FAutoConsoleCommand RewardStatusCommand(TEXT("FF.ApplyFacilityCodexRewardStatus"),
    TEXT("Show an amber gift claim action and a separate green checked completion badge."),
    FConsoleCommandDelegate::CreateStatic(&RunRewardStatus));

static void RunRewardIridescence()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry")) return;
    UWidgetBlueprint* BP = LoadExistingBlueprint(CardPath);
    if (!BP || !BP->WidgetTree || !CardContract(*BP, true)) return;
    UWidgetTree& Tree = *BP->WidgetTree;
    auto* Button = Get<UButton>(Tree, TEXT("Button_ClaimReward"));
    auto* Overlay = Get<UOverlay>(Tree, TEXT("Overlay_ClaimReward"));
    auto* Label = Get<UTextBlock>(Tree, TEXT("Text_ClaimReward"));
    auto* Received = Get<UBorder>(Tree, TEXT("Border_RewardClaimed"));
    auto* Check = Get<UImage>(Tree, TEXT("Image_RewardClaimed"));
    auto* Completed = Get<UTextBlock>(Tree, TEXT("Text_RewardClaimed"));
    if (!Button || !Overlay || !Label || !Received || !Check || !Completed ||
        !Cast<UButtonSlot>(Overlay->Slot)) return;

    TArray<UMaterialInterface*> Materials;
    for (const TCHAR* State : {TEXT("Ready"), TEXT("Hover"), TEXT("Pressed"), TEXT("Claimed")})
    {
        const FString Name = FString(TEXT("MI_FF_CodexReward_")) + State;
        const FString Path = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Materials/") + Name + TEXT(".") + Name;
        auto* Material = LoadObject<UMaterialInterface>(nullptr, *Path);
        if (!Material || Material->GetMaterial()->MaterialDomain != MD_UI)
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_CODEX_REWARD_IRIDESCENCE_FAIL: missing UI material %s"), *Path);
            return;
        }
        Materials.Add(Material);
    }
    auto& Meta = BP->GetOutermost()->GetMetaData();
    if (Meta.GetValue(BP, IridescenceVersion) == TEXT("1")) return;
    BP->Modify(); Tree.Modify();
    auto MaterialBrush = [](UMaterialInterface* Material)
    {
        FSlateBrush Result;
        Result.DrawAs = ESlateBrushDrawType::Image;
        Result.SetResourceObject(Material);
        Result.ImageSize = FVector2D(68.f, 32.f);
        Result.TintColor = FSlateColor(FLinearColor::White);
        return Result;
    };
    FButtonStyle Style = Button->GetStyle();
    Style.Normal = MaterialBrush(Materials[0]);
    Style.Hovered = MaterialBrush(Materials[1]);
    Style.Pressed = MaterialBrush(Materials[2]);
    Style.Disabled = MaterialBrush(Materials[3]);
    Style.NormalPadding = Style.PressedPadding = FMargin(0);
    Button->SetStyle(Style);
    CastChecked<UButtonSlot>(Overlay->Slot)->SetPadding(FMargin(6, 4));
    Label->SetColorAndOpacity(FSlateColor(Color(255, 241, 201)));
    Label->SetShadowColorAndOpacity(Color(10, 32, 29));
    Label->SetShadowOffset(FVector2D(0, 1));
    Received->SetBrush(MaterialBrush(Materials[3]));
    Received->SetBrushColor(FLinearColor::White);
    Received->SetPadding(FMargin(6, 4));
    Check->SetColorAndOpacity(Color(196, 234, 213));
    Completed->SetColorAndOpacity(FSlateColor(Color(196, 234, 213)));
    // Brushes live in the saved WBP: normal Slate hover/press switches material states,
    // and the claimed badge stays static without timers or per-card dynamic instances.
    Meta.SetValue(BP, IridescenceVersion, TEXT("1"));
    if (SaveNewBlueprint(TEXT("FF_CODEX_REWARD_IRIDESCENCE"), *BP))
    {
        UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_CODEX_REWARD_IRIDESCENCE_PASS"));
    }
    else { Meta.RemoveValue(BP, IridescenceVersion); }
}

static FAutoConsoleCommand RewardIridescenceCommand(TEXT("FF.ApplyFacilityCodexRewardIridescence"),
    TEXT("Apply iridescent game reward materials to the saved claim button and static checked badge."),
    FConsoleCommandDelegate::CreateStatic(&RunRewardIridescence));

static void RunRewardBounds()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry")) return;
    UWidgetBlueprint* BP = LoadExistingBlueprint(CardPath);
    if (!BP || !BP->WidgetTree || !CardContract(*BP, true)) return;
    auto& Meta = BP->GetOutermost()->GetMetaData();
    if (Meta.GetValue(BP, RewardBoundsVersion) == TEXT("1")) return;
    UWidgetTree& Tree = *BP->WidgetTree;
    auto* Row = Get<UHorizontalBox>(Tree, TEXT("Horizontal_RewardActions"));
    auto* Button = Get<UButton>(Tree, TEXT("Button_ClaimReward"));
    auto* Received = Get<UBorder>(Tree, TEXT("Border_RewardClaimed"));
    auto* OldSlot = Button ? Cast<UHorizontalBoxSlot>(Button->Slot) : nullptr;
    if (!Row || !Button || !Received || !OldSlot || Button->GetParent() != Row || Received->GetParent() != Row ||
        Tree.FindWidget(TEXT("Size_RewardState")) || Tree.FindWidget(TEXT("Overlay_RewardState"))) return;
    const FMargin Padding = OldSlot->GetPadding();
    BP->Modify(); Tree.Modify();
    Button->RemoveFromParent(); Received->RemoveFromParent();
    auto* Bounds = MakeWidget<USizeBox>(*BP, Tree, TEXT("Size_RewardState"), true);
    Bounds->SetWidthOverride(72.f); Bounds->SetHeightOverride(32.f);
    Bounds->SetVisibility(ESlateVisibility::Collapsed);
    auto* RowSlot = Row->AddChildToHorizontalBox(Bounds);
    RowSlot->SetPadding(Padding); RowSlot->SetVerticalAlignment(VAlign_Center);
    RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    auto* States = MakeWidget<UOverlay>(*BP, Tree, TEXT("Overlay_RewardState"));
    Bounds->AddChild(States);
    // Both backgrounds fill the very same allocation. Different glyph sizes and
    // SButton/SBorder desired-size rules can no longer move the fee when claimed.
    for (UWidget* State : {static_cast<UWidget*>(Button), static_cast<UWidget*>(Received)})
    {
        auto* Slot = States->AddChildToOverlay(State);
        Slot->SetHorizontalAlignment(HAlign_Fill); Slot->SetVerticalAlignment(VAlign_Fill);
        Slot->SetPadding(FMargin(0));
    }
    if (!CardContract(*BP, true)) return;
    Meta.SetValue(BP, RewardBoundsVersion, TEXT("1"));
    if (SaveNewBlueprint(TEXT("FF_CODEX_REWARD_BOUNDS"), *BP))
    {
        UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_CODEX_REWARD_BOUNDS_PASS"));
    }
    else { Meta.RemoveValue(BP, RewardBoundsVersion); }
}

static FAutoConsoleCommand RewardBoundsCommand(TEXT("FF.ApplyFacilityCodexRewardBounds"),
    TEXT("Keep the available reward button and completed badge in identical fixed bounds."),
    FConsoleCommandDelegate::CreateStatic(&RunRewardBounds));

static void RunRewardHint()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry")) return;
    UWidgetBlueprint* BP = LoadExistingBlueprint(CardPath);
    UTexture2D* Gift = LoadObject<UTexture2D>(nullptr, IconPath);
    if (!BP || !BP->WidgetTree || !Gift || !CardContract(*BP, true)) return;
    auto& Meta = BP->GetOutermost()->GetMetaData();
    if (Meta.GetValue(BP, RewardHintVersion) == TEXT("1")) return;
    UWidgetTree& Tree = *BP->WidgetTree;
    auto* States = Get<UOverlay>(Tree, TEXT("Overlay_RewardState"));
    auto* Label = Get<UTextBlock>(Tree, TEXT("Text_ClaimReward"));
    if (!States || !Label || States->GetChildrenCount() != 2 || Tree.FindWidget(TEXT("Border_RewardLocked"))) return;
    BP->Modify(); Tree.Modify();
    auto* Locked = MakeWidget<UBorder>(*BP, Tree, TEXT("Border_RewardLocked"), true);
    FSlateBrush Surface = Brush(Color(236, 230, 214), Color(186, 172, 135));
    Surface.OutlineSettings = FSlateBrushOutlineSettings(7.5f, FSlateColor(Color(186, 172, 135)), 1.f);
    Locked->SetBrush(Surface); Locked->SetBrushColor(FLinearColor::White);
    Locked->SetPadding(FMargin(6, 4));
    Locked->SetHorizontalAlignment(HAlign_Center); Locked->SetVerticalAlignment(VAlign_Center);
    Locked->SetVisibility(ESlateVisibility::Collapsed);
    auto* StateSlot = States->AddChildToOverlay(Locked);
    StateSlot->SetHorizontalAlignment(HAlign_Fill); StateSlot->SetVerticalAlignment(VAlign_Fill);
    auto* Content = MakeWidget<UHorizontalBox>(*BP, Tree, TEXT("Horizontal_RewardLocked"));
    Locked->AddChild(Content); Content->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* GiftSize = MakeWidget<USizeBox>(*BP, Tree, TEXT("Size_RewardLockedIcon"));
    GiftSize->SetWidthOverride(24.f); GiftSize->SetHeightOverride(24.f);
    Content->AddChildToHorizontalBox(GiftSize)->SetVerticalAlignment(VAlign_Center);
    auto* Icon = MakeWidget<UImage>(*BP, Tree, TEXT("Image_RewardLocked"));
    Icon->SetBrushFromTexture(Gift); Icon->SetDesiredSizeOverride(FVector2D(24.f));
    Icon->SetColorAndOpacity(FLinearColor(.7f, .7f, .7f, 1.f));
    Icon->SetVisibility(ESlateVisibility::HitTestInvisible); GiftSize->AddChild(Icon);
    auto* Caption = MakeWidget<UTextBlock>(*BP, Tree, TEXT("Text_RewardLocked"));
    Caption->SetFont(Label->GetFont()); Caption->SetColorAndOpacity(FSlateColor(Color(101, 90, 66)));
    Caption->SetText(LOCTEXT("RewardLocked", "잠김")); Caption->SetAutoWrapText(false);
    Caption->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* CaptionSlot = Content->AddChildToHorizontalBox(Caption);
    CaptionSlot->SetPadding(FMargin(3, 0, 0, 0)); CaptionSlot->SetVerticalAlignment(VAlign_Center);
    if (!CardContract(*BP, true)) return;
    Meta.SetValue(BP, RewardHintVersion, TEXT("1"));
    if (SaveNewBlueprint(TEXT("FF_CODEX_REWARD_HINT"), *BP))
    {
        UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_CODEX_REWARD_HINT_PASS"));
    }
    else { Meta.RemoveValue(BP, RewardHintVersion); }
}

static FAutoConsoleCommand RewardHintCommand(TEXT("FF.ApplyFacilityCodexRewardHint"),
    TEXT("Show a passive locked gift hint before a recipe's discovery in the existing reward bounds."),
    FConsoleCommandDelegate::CreateStatic(&RunRewardHint));
}

#undef LOCTEXT_NAMESPACE
#endif
