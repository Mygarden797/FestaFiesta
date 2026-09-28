// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// FF_BuildReferenceCatalogBuilder.cpp
// Restyle the active construction catalog without rebuilding its WidgetTree or graphs.

#include "CoreMinimal.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"

#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"
#include "UI/PrepHUD/Panels/FF_PrepChoiceButton.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace FFBuildReferenceCatalog
{
using namespace FFUIWbpBuilder;

constexpr const TCHAR* Prefix = TEXT("FF_BUILD_REFERENCE_CATALOG");
constexpr const TCHAR* VersionKey = TEXT("FF.BuildReferenceCatalog.Style20260923.v1");

FLinearColor Color(const FColor Srgb, const float Alpha = 1.f)
{
    FLinearColor Result = FLinearColor::FromSRGBColor(Srgb);
    Result.A = Alpha;
    return Result;
}

const FLinearColor Ink = Color(FColor(0x24, 0x43, 0x38));
const FLinearColor Muted = Color(FColor(0x62, 0x7C, 0x70));
const FLinearColor Teal = Color(FColor(0x25, 0x59, 0x49));
const FLinearColor Line = Color(FColor(0xA6, 0xC1, 0xB1));
const FLinearColor Paper = Color(FColor(0xF7, 0xFA, 0xF4));

FSlateBrush Rounded(const FLinearColor Fill, const float Radius,
    const FLinearColor Outline = Line, const float OutlineWidth = 1.f)
{
    FSlateBrush Brush;
    Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
    Brush.TintColor = FSlateColor(Fill);
    Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
    Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
    Brush.OutlineSettings.Color = FSlateColor(Outline);
    Brush.OutlineSettings.Width = OutlineWidth;
    return Brush;
}

bool AlreadyApplied(UWidgetBlueprint& BP)
{
    return BP.GetOutermost()->GetMetaData().GetValue(&BP, VersionKey) == TEXT("1");
}

bool SaveVersioned(UWidgetBlueprint& BP)
{
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    Metadata.SetValue(&BP, VersionKey, TEXT("1"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, VersionKey);
    return false;
}

void RestyleButton(UButton& Button, const bool bSelected)
{
    FButtonStyle Style = Button.GetStyle();
    const FSlateBrush Idle = Rounded(Color(FColor(0xF4, 0xF9, 0xF2), 0.91f), 9.f);
    const FSlateBrush Hover = Rounded(Color(FColor(0xE2, 0xF0, 0xE4), 0.98f), 9.f,
        Color(FColor(0x6D, 0xA0, 0x84)), 1.25f);
    const FSlateBrush Selected = Rounded(Teal, 9.f,
        Color(FColor(0xA6, 0xDC, 0xBD)), 1.5f);
    Style.Normal = bSelected ? Selected : Idle;
    Style.Hovered = bSelected ? Selected : Hover;
    Style.Pressed = Selected;
    Style.Disabled = Rounded(Color(FColor(0xD8, 0xE2, 0xD8), 0.8f), 9.f);
    Button.SetStyle(Style);
}

bool RestylePanel(UWidgetBlueprint& BP)
{
    if (AlreadyApplied(BP)) return true;
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Tray = Cast<UBorder>(Tree.FindWidget(TEXT("Border_PanelRoot")));
    auto* Dock = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_ClarityCatalogDock")));
    auto* Footer = Cast<UBorder>(Tree.FindWidget(TEXT("Border_CatalogFooter")));
    auto* Search = Cast<UEditableTextBox>(Tree.FindWidget(TEXT("Search_Facilities")));
    auto* SelectedTab = Cast<UButton>(Tree.FindWidget(TEXT("Button_FilterAll")));
    if (!Tray || !Dock || !Footer || !Search || !SelectedTab ||
        !Tree.FindWidget(TEXT("HorizontalBox_Categories")) ||
        !Tree.FindWidget(TEXT("TileView_Facilities")))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: active catalog panel seams are missing"), Prefix);
        return false;
    }

    BP.Modify();
    Tree.Modify();
    // The world stays visible through the broad lower tray; nested controls stay opaque enough to read.
    Tray->SetBrush(Rounded(Color(FColor(0xF3, 0xF8, 0xEF), 0.88f), 17.f,
        Color(FColor(0x8C, 0xB4, 0x99), 0.95f), 1.5f));
    Tray->SetBrushColor(FLinearColor::White);
    Footer->SetBrush(Rounded(Color(FColor(0xE4, 0xF2, 0xE3), 0.88f), 10.f,
        Color(FColor(0x9A, 0xBC, 0xA4), 0.85f), 1.f));
    Footer->SetBrushColor(FLinearColor::White);
    if (auto* Gold = Cast<UBorder>(Tree.FindWidget(TEXT("Border_Gold"))))
        Gold->SetBrush(Rounded(Color(FColor(0xFC, 0xF2, 0xD6), 0.97f), 10.f,
            Color(FColor(0xD4, 0xB7, 0x6E)), 1.f));

    RestyleButton(*SelectedTab, true);
    for (const TCHAR* Name : {TEXT("Button_FilterBooth"), TEXT("Button_FilterPerformance")})
        if (auto* Button = Cast<UButton>(Tree.FindWidget(Name))) RestyleButton(*Button, false);
    for (const TCHAR* Name : {TEXT("Button_PreviousPage"), TEXT("Button_NextPage"),
        TEXT("Button_InspectFacilities"), TEXT("Button_RevenueRanking")})
        if (auto* Button = Cast<UButton>(Tree.FindWidget(Name))) RestyleButton(*Button, false);

    FEditableTextBoxStyle SearchStyle = Search->GetWidgetStyle();
    SearchStyle.BackgroundImageNormal = Rounded(Color(FColor(0xFF, 0xFF, 0xFC), 0.98f), 9.f);
    SearchStyle.BackgroundImageHovered = Rounded(Paper, 9.f, Color(FColor(0x75, 0xA6, 0x86)), 1.25f);
    SearchStyle.BackgroundImageFocused = Rounded(Paper, 9.f, Teal, 1.5f);
    Search->SetWidgetStyle(SearchStyle);
    if (auto* Title = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_PanelTitle"))))
        StyleText(*Title, 22, Ink, TEXT("SemiBold"));
    if (auto* Status = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Status"))))
        StyleText(*Status, 13, Muted, TEXT("Medium"));
    if (auto* Count = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_ItemCount"))))
        StyleText(*Count, 13, Teal, TEXT("SemiBold"));

    return SaveVersioned(BP);
}

bool RestyleTile(UWidgetBlueprint& BP)
{
    if (AlreadyApplied(BP)) return true;
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Card = Cast<UBorder>(Tree.FindWidget(TEXT("Border_Card")));
    auto* Thumbnail = Cast<UBorder>(Tree.FindWidget(TEXT("Border_ThumbnailBackdrop")));
    auto* ThumbnailSize = Cast<USizeBox>(Tree.FindWidget(TEXT("SizeBox_Thumbnail")));
    if (!Card || !Thumbnail || !ThumbnailSize || !Tree.FindWidget(TEXT("Text_Name")) ||
        !Tree.FindWidget(TEXT("Image_Thumbnail")))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: active catalog tile seams are missing"), Prefix);
        return false;
    }

    BP.Modify();
    Tree.Modify();
    // The existing virtualized tile owns hover and selection; its authored brush is the base state.
    Card->SetBrush(Rounded(Color(FColor(0xFF, 0xFF, 0xFC), 0.99f), 11.f,
        Color(FColor(0x79, 0xA8, 0x89)), 1.5f));
    Card->SetBrushColor(FLinearColor::White);
    Thumbnail->SetBrush(Rounded(Color(FColor(0xD7, 0xE9, 0xD4), 0.98f), 8.f,
        Color(FColor(0xB5, 0xCF, 0xB4)), 1.f));
    Thumbnail->SetBrushColor(FLinearColor::White);
    // Keep the tuned 204px card/104px thumbnail geometry so badges and cost never clip.
    if (auto* Name = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Name"))))
        StyleText(*Name, 17, Ink, TEXT("SemiBold"));
    if (auto* Cost = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Cost"))))
        StyleText(*Cost, 16, Teal, TEXT("SemiBold"));
    if (auto* Category = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_CategoryPath"))))
        StyleText(*Category, 11, Muted, TEXT("Medium"));

    // Compile the Designer style first, then save the runtime selected tint on the generated CDO.
    if (!SaveNewBlueprint(Prefix, BP)) return false;
    auto* SelectedTint = FindFProperty<FStructProperty>(BP.GeneratedClass.Get(), TEXT("SelectedTint"));
    if (!SelectedTint || SelectedTint->Struct != TBaseStructure<FLinearColor>::Get())
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: SelectedTint CDO property is missing"), Prefix);
        return false;
    }
    *SelectedTint->ContainerPtrToValuePtr<FLinearColor>(BP.GeneratedClass->GetDefaultObject()) =
        Color(FColor(0xC4, 0xE7, 0xD2));
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    Metadata.SetValue(&BP, VersionKey, TEXT("1"));
    if (SaveAssetPackage(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, VersionKey);
    return false;
}

bool RestyleCategoryChoice(UWidgetBlueprint& BP)
{
    if (AlreadyApplied(BP)) return true;
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Button = Cast<UButton>(Tree.FindWidget(TEXT("Button_Root")));
    auto* Label = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Label")));
    if (!Button || !Label || !Tree.FindWidget(TEXT("Size_Choice")))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: category choice seams are missing"), Prefix);
        return false;
    }
    BP.Modify();
    Tree.Modify();
    // UFF_HudTabButtonBase uses Pressed as the persistent selected brush.
    RestyleButton(*Button, false);
    StyleText(*Label, 13, Ink, TEXT("SemiBold"));
    return SaveVersioned(BP);
}

void Apply()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!GEditor || GEditor->PlayWorld || !World ||
        World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry"))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: use the idle Entry map"), Prefix);
        return;
    }
    auto* Panel = LoadExistingBlueprint(FFBuildUIAssetPaths::BuildPanelPackage);
    auto* Tile = LoadExistingBlueprint(FFBuildUIAssetPaths::BuildTilePackage);
    auto* Choice = LoadExistingBlueprint(FFBuildUIAssetPaths::CategoryChoicePackage);
    if (!Panel || !Tile || !Choice || !Panel->WidgetTree || !Tile->WidgetTree || !Choice->WidgetTree ||
        Panel->ParentClass != UFF_PrepPanelBuild::StaticClass() ||
        Tile->ParentClass != UFF_PrepBuildTileWidget::StaticClass() ||
        Choice->ParentClass != UFF_PrepChoiceButton::StaticClass())
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: expected active WBPs and parents are required"), Prefix);
        return;
    }
    const bool bSuccess = RestylePanel(*Panel) && RestyleTile(*Tile) && RestyleCategoryChoice(*Choice);
    if (bSuccess)
    {
        UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_PASS"), Prefix);
    }
    else
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL"), Prefix);
    }
}

FAutoConsoleCommand Command(TEXT("FF.ApplyBuildReferenceCatalog"),
    TEXT("Apply the translucent paper/teal reference styling to the active build catalog WBPs once."),
    FConsoleCommandDelegate::CreateStatic(&Apply));
}
#endif // WITH_EDITOR
