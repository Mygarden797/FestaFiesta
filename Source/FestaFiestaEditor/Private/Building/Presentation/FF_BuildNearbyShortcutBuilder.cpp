// Add the nearby-placement shortcut to the saved guide without rebuilding the WBP.
#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_BuildPearlStyle.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FFBuildNearbyShortcut"

namespace FFBuildNearbyShortcut
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Prefix = TEXT("FF_BUILD_NEARBY_SHORTCUT");
constexpr const TCHAR* Version = TEXT("FF.BuildNearbyShortcut.20261001.v1");

UTextBlock* Label(UWidgetBlueprint& BP, UWidgetTree& Tree, const FName Name,
    const FText& Caption, int32 FontSize, const FLinearColor& Color, const FName Typeface)
{
    auto* Text = MakeWidget<UTextBlock>(BP, Tree, Name);
    StyleText(*Text, FontSize, Color, Typeface);
    Text->SetText(Caption);
    Text->SetVisibility(ESlateVisibility::HitTestInvisible);
    Text->SetShadowOffset(FVector2D::ZeroVector);
    return Text;
}

void AddRow(UHorizontalBox& Row, UWidget& Child, float RightPadding, bool bFill = false)
{
    auto* Slot = Row.AddChildToHorizontalBox(&Child);
    Slot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
    Slot->SetVerticalAlignment(VAlign_Center);
    Slot->SetPadding(FMargin(0.f, 0.f, RightPadding, 0.f));
}

// Match the keyboard guide's inset face and lower lip so N reads as a physical key.
USizeBox* NearbyKeycap(UWidgetBlueprint& BP, UWidgetTree& Tree)
{
    auto* Size = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_BuildHelpKey_N"));
    Size->SetMinDesiredWidth(25.f);
    Size->SetHeightOverride(28.f);
    auto* Edge = MakeWidget<UBorder>(BP, Tree, TEXT("Border_BuildHelpKey_N"));
    Edge->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Color(158, 169, 193), 5.f));
    Edge->SetPadding(FMargin(1.f, 1.f, 1.f, 3.f));
    auto* Face = MakeWidget<UBorder>(BP, Tree, TEXT("Face_BuildHelpKey_N"));
    Face->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Color(247, 249, 255), 4.f));
    Face->SetPadding(FMargin(5.f, 1.f));
    Face->SetHorizontalAlignment(HAlign_Center);
    Face->SetVerticalAlignment(VAlign_Center);
    Face->AddChild(Label(BP, Tree, TEXT("Text_BuildHelpKey_N"), FText::FromString(TEXT("N")),
        14, FFBuildPearl::Ink, TEXT("SemiBold")));
    Edge->AddChild(Face);
    Size->AddChild(Edge);
    return Size;
}

bool Verify(UWidgetBlueprint& BP)
{
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Column = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Column_BuildIconHelp")));
    auto* Rotate = Tree.FindWidget(TEXT("Row_BuildHelpRotate"));
    auto* Row = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("Row_BuildHelpNearby")));
    auto* Lane = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_BuildHelpNearbyIcon")));
    auto* Key = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_BuildHelpKey_N")));
    auto* Caption = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_BuildHelpNearby")));
    auto* Button = Cast<UButton>(Tree.FindWidget(TEXT("Button_FindNearby")));
    auto* ButtonLabel = Button ? Cast<UTextBlock>(Button->GetContent()) : nullptr;
    return Column && Rotate && Row && Lane && Key && Caption && ButtonLabel &&
        Row->GetParent() == Column && Column->GetChildIndex(Row) == Column->GetChildIndex(Rotate) + 1 &&
        Row->GetVisibility() == ESlateVisibility::SelfHitTestInvisible && !Row->bHiddenInDesigner &&
        Lane->GetParent() == Row && Lane->GetWidthOverride() == 124.f &&
        Key->GetText().ToString() == TEXT("N") && Key->GetFont().Size == 14 &&
        Caption->GetParent() == Row && Caption->GetText().ToString() == TEXT("가까운 빈자리로 이동") &&
        ButtonLabel->GetText().ToString() == TEXT("[N] 가까운 빈자리") &&
        Button->GetToolTipText().ToString() == TEXT("N 또는 클릭으로 가까운 빈자리로 이동합니다. Enter로 배치를 확정하세요.");
}

bool Migrate(UWidgetBlueprint& BP)
{
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, Version) == TEXT("1"))
    {
        if (!Verify(BP)) return false;
        UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: saved shortcut is current; no save"), Prefix);
        return true;
    }

    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Column = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Column_BuildIconHelp")));
    auto* Rotate = Tree.FindWidget(TEXT("Row_BuildHelpRotate"));
    auto* Button = Cast<UButton>(Tree.FindWidget(TEXT("Button_FindNearby")));
    auto* ButtonLabel = Button ? Cast<UTextBlock>(Button->GetContent()) : nullptr;
    if (!Column || !Rotate || Rotate->GetParent() != Column || !ButtonLabel)
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: expected authored guide and nearby button"), Prefix);
        return false;
    }
    // Refuse partial/colliding branches before touching any existing widgets.
    for (const TCHAR* Name : {TEXT("Row_BuildHelpNearby"), TEXT("Size_BuildHelpNearbyIcon"),
        TEXT("Row_BuildHelpNearbyKeys"), TEXT("Size_BuildHelpKey_N"), TEXT("Border_BuildHelpKey_N"),
        TEXT("Face_BuildHelpKey_N"), TEXT("Text_BuildHelpKey_N"), TEXT("Text_BuildHelpNearby")})
    {
        if (Tree.FindWidget(Name))
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: unversioned shortcut widget %s already exists"), Prefix, Name);
            return false;
        }
    }

    BP.Modify();
    Tree.Modify();
    Column->Modify();
    Button->Modify();
    ButtonLabel->Modify();
    auto* Row = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_BuildHelpNearby"));
    Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Row->bHiddenInDesigner = false;
    auto* Keys = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_BuildHelpNearbyKeys"));
    AddRow(*Keys, *NearbyKeycap(BP, Tree), 4.f);
    auto* Lane = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_BuildHelpNearbyIcon"));
    Lane->SetWidthOverride(124.f);
    Lane->SetHeightOverride(30.f);
    auto* LaneSlot = CastChecked<USizeBoxSlot>(Lane->AddChild(Keys));
    LaneSlot->SetHorizontalAlignment(HAlign_Left);
    LaneSlot->SetVerticalAlignment(VAlign_Center);
    AddRow(*Row, *Lane, 10.f);
    auto* Caption = Label(BP, Tree, TEXT("Text_BuildHelpNearby"),
        LOCTEXT("NearbyHelp", "가까운 빈자리로 이동"), 13, FLinearColor::White, TEXT("Medium"));
    Caption->SetAutoWrapText(true);
    AddRow(*Row, *Caption, 0.f, true);
    auto* RowSlot = Column->AddChildToVerticalBox(Row);
    RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
    Column->ShiftChild(Column->GetChildIndex(Rotate) + 1, Row);

    // Preserve the existing button, content widget, style and bindings.
    ButtonLabel->SetText(LOCTEXT("NearbyButton", "[N] 가까운 빈자리"));
    Button->SetToolTipText(LOCTEXT("NearbyTooltip", "N 또는 클릭으로 가까운 빈자리로 이동합니다. Enter로 배치를 확정하세요."));
    if (!Verify(BP)) return false;
    Metadata.SetValue(&BP, Version, TEXT("1"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, Version);
    return false;
}

void Apply()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry"))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: requires idle Entry editor"), Prefix);
        return;
    }
    auto* BP = LoadExistingBlueprint(FFBuildUIAssetPaths::BuildToolsPackage);
    if (!BP || !BP->WidgetTree || BP->ParentClass != UFF_BuildToolsWidget::StaticClass() || !Migrate(*BP))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: saved BuildTools migration failed"), Prefix);
        return;
    }
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_PASS: N guide and nearby button ready"), Prefix);
}

FAutoConsoleCommand Command(TEXT("FF.ApplyBuildNearbyShortcut"),
    TEXT("Add the N nearby-placement guide and label to the current saved BuildTools WBP only."),
    FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
#endif
