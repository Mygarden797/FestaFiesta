// Explicit saved-WBP theme migration. Runtime binds data; it does not rebuild this layout.
#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/FacilityCodex/FF_FacilityCodexCard.h"
#include "UI/FacilityCodex/FF_FacilityCodexImage.h"
#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FFFacilityCodexFestivalBuilder"

namespace FFFacilityCodexFestivalBuilder
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Prefix = TEXT("FF_CODEX_FESTIVAL_THEME");
constexpr const TCHAR* Version = TEXT("FF.FacilityCodex.FestivalTheme20260921.1");
constexpr const TCHAR* CardPath = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexCard");
constexpr const TCHAR* ScreenPath = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexScreen");
constexpr const TCHAR* ThumbnailPath = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Materials/M_FF_Codex_DiscoveryThumbnail.M_FF_Codex_DiscoveryThumbnail");

template<class T> static T* Get(UWidgetTree& Tree, const TCHAR* Name)
{ return Cast<T>(Tree.FindWidget(FName(Name))); }

static FLinearColor Color(uint8 R, uint8 G, uint8 B)
{ return FLinearColor::FromSRGBColor(FColor(R, G, B)); }

static FSlateBrush Rounded(FLinearColor Fill, FLinearColor Outline, float Radius = 6.f, float Width = 1.f)
{
    FSlateBrush Result;
    Result.DrawAs = ESlateBrushDrawType::RoundedBox;
    Result.TintColor = FSlateColor(Fill);
    Result.OutlineSettings = FSlateBrushOutlineSettings(Radius, FSlateColor(Outline), Width);
    return Result;
}

static UMaterialInterface* Material(const TCHAR* Role)
{
    const FString Name = FString(TEXT("MI_FF_CodexFestival_")) + Role;
    const FString Path = FString(TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Materials/")) + Name + TEXT(".") + Name;
    return LoadObject<UMaterialInterface>(nullptr, *Path);
}

static void Surface(UBorder* Border, UMaterialInterface* Resource)
{
    FSlateBrush Brush;
    Brush.DrawAs = ESlateBrushDrawType::Image;
    Brush.SetResourceObject(Resource);
    Brush.ImageSize = FVector2D(128.f, 128.f);
    Brush.TintColor = FSlateColor(FLinearColor::White);
    Border->SetBrush(Brush);
    Border->SetBrushColor(FLinearColor::White);
}

static void ColorText(UWidgetTree& Tree, const TCHAR* Name, FLinearColor Ink, int32 Size = 0)
{
    if (auto* Label = Get<UTextBlock>(Tree, Name))
    {
        Label->SetColorAndOpacity(FSlateColor(Ink));
        if (Size > 0)
        {
            FSlateFontInfo Font = Label->GetFont();
            Font.Size = Size;
            Label->SetFont(Font);
        }
    }
}

static bool HasImages(UWidgetTree& Tree)
{
    return Get<UFF_FacilityCodexImage>(Tree, TEXT("Image_Target")) &&
        Get<UFF_FacilityCodexImage>(Tree, TEXT("Image_Partner"));
}

static void ApplyImages(UWidgetTree& Tree, UMaterialInterface* DiscoveryMaterial)
{
    for (const TCHAR* Name : {TEXT("Image_Target"), TEXT("Image_Partner")})
    {
        auto* Image = Get<UFF_FacilityCodexImage>(Tree, Name);
        Image->DiscoveryMaterial = DiscoveryMaterial;
        Image->SetIsEnabled(true);
    }
    for (const TCHAR* Name : {TEXT("Border_ThumbTarget"), TEXT("Border_ThumbPartner")})
    {
        if (auto* Tray = Get<UBorder>(Tree, Name))
        {
            Tray->SetBrush(Rounded(Color(241, 246, 231), Color(218, 225, 204), 5.f, 0.f));
            Tray->SetBrushColor(FLinearColor::White);
        }
    }
}

static bool EditCard(UWidgetBlueprint& BP, UMaterialInterface* DiscoveryMaterial)
{
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Card = Get<UBorder>(Tree, TEXT("Border_Card"));
    auto* Selection = Get<UBorder>(Tree, TEXT("Border_Selection"));
    auto* State = Get<UBorder>(Tree, TEXT("Border_State"));
    if (!Card || !Selection || !State || !HasImages(Tree) ||
        !Get<USizeBox>(Tree, TEXT("Size_RewardState"))) return false;
    // Card sizing, reward bounds and every existing action remain authored as-is.
    Surface(Card, Material(TEXT("Pass")));
    Surface(Selection, Material(TEXT("Selection")));
    State->SetBrush(Rounded(Color(224, 237, 212), Color(166, 194, 153), 4.f));
    State->SetBrushColor(FLinearColor::White);
    ColorText(Tree, TEXT("Text_Name"), Color(30, 69, 56));
    ColorText(Tree, TEXT("Text_Bonus"), Color(27, 79, 52));
    ColorText(Tree, TEXT("Text_State"), Color(49, 88, 55));
    ColorText(Tree, TEXT("Text_Plus"), Color(95, 123, 91));
    ApplyImages(Tree, DiscoveryMaterial);
    return true;
}

static bool EditScreen(UWidgetBlueprint& BP, UMaterialInterface* DiscoveryMaterial)
{
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Paper = Get<UBorder>(Tree, TEXT("Border_Paper"));
    auto* Header = Get<UBorder>(Tree, TEXT("Border_Header"));
    auto* Heading = Get<UVerticalBox>(Tree, TEXT("Vertical_Heading"));
    auto* HeaderRow = Get<UHorizontalBox>(Tree, TEXT("Horizontal_Header"));
    auto* FilterRow = Get<UHorizontalBox>(Tree, TEXT("Horizontal_Filter"));
    auto* Title = Get<UTextBlock>(Tree, TEXT("Text_Title"));
    auto* Intro = Get<UTextBlock>(Tree, TEXT("Text_Intro"));
    auto* Summary = Get<UTextBlock>(Tree, TEXT("Text_Summary"));
    auto* SummarySlot = Summary ? Cast<UHorizontalBoxSlot>(Summary->Slot) : nullptr;
    if (!Paper || !Header || !Heading || !HeaderRow || !FilterRow || !Title || !Intro ||
        !Summary || !SummarySlot || Summary->GetParent() != HeaderRow || !HasImages(Tree) ||
        !Tree.FindWidget(TEXT("Size_Dialog")) || !Tree.FindWidget(TEXT("Size_Detail")) ||
        Tree.FindWidget(TEXT("Progress_Discovery")) || Tree.FindWidget(TEXT("Text_VisualLegend"))) return false;

    Surface(Paper, Material(TEXT("Paper")));
    Surface(Header, Material(TEXT("Header")));
    Title->SetText(LOCTEXT("FestivalTitle", "축제 조합 도감"));
    Intro->SetText(LOCTEXT("FestivalIntro", "바닷가에서 모은, 함께 즐기는 발견"));
    Intro->SetVisibility(ESlateVisibility::HitTestInvisible);
    Intro->SetAutoWrapText(false);
    ColorText(Tree, TEXT("Text_Title"), Color(248, 244, 214), 22);
    ColorText(Tree, TEXT("Text_Intro"), Color(177, 212, 181), 12);
    ColorText(Tree, TEXT("Text_Summary"), Color(235, 229, 192), 13);
    if (auto* Slot = Cast<UVerticalBoxSlot>(Intro->Slot)) Slot->SetPadding(FMargin(0, 1, 0, 0));
    if (auto* Slot = Cast<UHorizontalBoxSlot>(Heading->Slot)) Slot->SetVerticalAlignment(VAlign_Center);

    const int32 SummaryIndex = HeaderRow->GetChildIndex(Summary);
    const FMargin SummaryPadding = SummarySlot->GetPadding();
    Summary->RemoveFromParent();
    auto* ProgressBounds = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_CollectionProgress"));
    ProgressBounds->SetWidthOverride(320.f);
    auto* ProgressSlot = HeaderRow->AddChildToHorizontalBox(ProgressBounds);
    ProgressSlot->SetPadding(SummaryPadding);
    ProgressSlot->SetVerticalAlignment(VAlign_Center);
    HeaderRow->ShiftChild(SummaryIndex, ProgressBounds);
    auto* ProgressColumn = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Vertical_CollectionProgress"));
    ProgressBounds->AddChild(ProgressColumn);
    Summary->SetJustification(ETextJustify::Right);
    Summary->SetAutoWrapText(false);
    ProgressColumn->AddChildToVerticalBox(Summary);
    auto* BarBounds = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_DiscoveryProgress"));
    BarBounds->SetHeightOverride(6.f);
    ProgressColumn->AddChildToVerticalBox(BarBounds)->SetPadding(FMargin(0, 5, 0, 0));
    auto* Progress = MakeWidget<UProgressBar>(BP, Tree, TEXT("Progress_Discovery"), true);
    FProgressBarStyle ProgressStyle;
    ProgressStyle.BackgroundImage = Rounded(Color(16, 49, 44), Color(72, 112, 82), 3.f, 0.5f);
    ProgressStyle.FillImage = Rounded(Color(238, 205, 121), Color(238, 205, 121), 3.f, 0.f);
    Progress->SetWidgetStyle(ProgressStyle);
    Progress->SetFillColorAndOpacity(FLinearColor::White);
    Progress->SetPercent(0.f);
    Progress->SetVisibility(ESlateVisibility::HitTestInvisible);
    BarBounds->AddChild(Progress);

    auto* Legend = MakeWidget<UTextBlock>(BP, Tree, TEXT("Text_VisualLegend"), true);
    FSlateFontInfo LegendFont = Summary->GetFont();
    LegendFont.Size = 12;
    Legend->SetFont(LegendFont);
    Legend->SetText(LOCTEXT("VisualLegend", "미발견: 실루엣·흑백   발견 완료: 컬러"));
    Legend->SetColorAndOpacity(FSlateColor(Color(81, 111, 88)));
    Legend->SetAutoWrapText(false);
    Legend->SetJustification(ETextJustify::Right);
    Legend->SetToolTipText(LOCTEXT("VisualLegendTooltip", "미발견 조합에서 아직 없는 시설은 실루엣, 설치한 시설은 흑백으로 표시합니다."));
    Legend->SetVisibility(ESlateVisibility::Visible);
    auto* LegendSlot = FilterRow->AddChildToHorizontalBox(Legend);
    LegendSlot->SetVerticalAlignment(VAlign_Center);
    LegendSlot->SetPadding(FMargin(14, 0, 0, 0));

    if (auto* Close = Get<UButton>(Tree, TEXT("Button_Close")))
    {
        FButtonStyle Style = Close->GetStyle();
        Style.Normal = Rounded(Color(25, 78, 64), Color(117, 157, 122));
        Style.Hovered = Rounded(Color(42, 101, 77), Color(214, 211, 148));
        Style.Pressed = Rounded(Color(19, 56, 46), Color(184, 181, 119));
        Close->SetStyle(Style);
        if (auto* Label = Cast<UTextBlock>(Close->GetContent()))
            Label->SetColorAndOpacity(FSlateColor(Color(247, 243, 216)));
    }
    if (auto* Detail = Get<UBorder>(Tree, TEXT("Panel_Detail")))
    {
        Detail->SetBrush(Rounded(Color(253, 253, 242), Color(164, 188, 153), 8.f));
        Detail->SetBrushColor(FLinearColor::White);
    }
    if (auto* Bonus = Get<UBorder>(Tree, TEXT("Border_Bonus")))
    {
        Bonus->SetBrush(Rounded(Color(226, 238, 215), Color(182, 199, 161)));
        Bonus->SetBrushColor(FLinearColor::White);
    }
    ColorText(Tree, TEXT("Text_DetailTitle"), Color(30, 78, 53));
    ColorText(Tree, TEXT("Text_Filter"), Color(89, 117, 93), 12);
    ApplyImages(Tree, DiscoveryMaterial);
    return true;
}

static bool Apply(const TCHAR* Path, UClass* Parent, bool(*Edit)(UWidgetBlueprint&, UMaterialInterface*), UMaterialInterface* DiscoveryMaterial)
{
    auto* BP = LoadExistingBlueprint(Path);
    if (!BP || !BP->WidgetTree || !BP->ParentClass || !BP->ParentClass->IsChildOf(Parent)) return false;
    auto& Meta = BP->GetOutermost()->GetMetaData();
    if (Meta.GetValue(BP, Version) == TEXT("1")) return true;
    BP->Modify(); BP->WidgetTree->Modify();
    if (!Edit(*BP, DiscoveryMaterial)) return false;
    Meta.SetValue(BP, Version, TEXT("1"));
    if (!SaveNewBlueprint(Prefix, *BP))
    {
        Meta.RemoveValue(BP, Version);
        return false;
    }
    return true;
}

static void Run()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry"))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: requires an idle Entry editor."), Prefix);
        return;
    }
    auto* Discovery = LoadObject<UMaterialInterface>(nullptr, ThumbnailPath);
    if (!Discovery || !Material(TEXT("Header")) || !Material(TEXT("Paper")) ||
        !Material(TEXT("Pass")) || !Material(TEXT("Selection")))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: create festival and discovery UI materials first."), Prefix);
        return;
    }
    const bool bSaved = Apply(CardPath, UFF_FacilityCodexCard::StaticClass(), EditCard, Discovery) &&
        Apply(ScreenPath, UFF_FacilityCodexScreen::StaticClass(), EditScreen, Discovery);
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_%s"), Prefix, bSaved ? TEXT("PASS") : TEXT("FAIL"));
}

static FAutoConsoleCommand Command(TEXT("FF.ApplyFacilityCodexFestivalTheme"),
    TEXT("Style the saved codex as a waterfront festival field-guide and bind discovery shaders, preserving actions and bounds."),
    FConsoleCommandDelegate::CreateStatic(&Run));
}

#undef LOCTEXT_NAMESPACE
#endif
