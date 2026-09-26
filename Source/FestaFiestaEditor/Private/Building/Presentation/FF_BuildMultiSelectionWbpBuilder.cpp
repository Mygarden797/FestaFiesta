// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// One-time editable UMG scaffold for the build multi-selection strip.
// Runtime also has a native fallback, so the asset is optional until this command is run.

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Building/Presentation/FF_BuildMultiSelectionWidget.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "UI/Core/FF_BuildPearlStyle.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FFBuildMultiSelectionWbpBuilder"

namespace FFBuildMultiSelectionWbpBuilder
{
using namespace FFUIWbpBuilder;

const FString WidgetFolder(FFBuildUIAssetPaths::ControlsFolder);
const FString WidgetPackage(WidgetFolder / TEXT("WBP_FF_BuildMultiSelection"));

FSlateBrush Surface(const FLinearColor Color, const float Radius,
	const FLinearColor Outline = FLinearColor::Transparent)
{
	return FFBuildPearl::Rounded(Color, Radius, Outline, Outline.A > 0.f ? 1.f : 0.f);
}

void AddToRow(UHorizontalBox& Row, UWidget& Child, const float RightPadding = 0.0f,
	const bool bFill = false)
{
	UHorizontalBoxSlot* Slot = Row.AddChildToHorizontalBox(&Child);
	Slot->SetPadding(FMargin(0.0f, 0.0f, RightPadding, 0.0f));
	Slot->SetVerticalAlignment(VAlign_Center);
	Slot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
}

UTextBlock* MakeLabel(UWidgetBlueprint& BP, UWidgetTree& Tree, const FName Name,
	const FText& Value, const int32 FontSize, const FLinearColor Color, const bool bVariable = false)
{
	UTextBlock* Label = MakeWidget<UTextBlock>(BP, Tree, Name, bVariable);
	StyleText(*Label, FontSize, Color, TEXT("Medium"));
	Label->SetText(Value);
	Label->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	Label->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Label;
}

UButton* MakeAction(UWidgetBlueprint& BP, UWidgetTree& Tree, const FName Name,
	const FText& Caption, const bool bPrimary)
{
	UButton* Button = MakeWidget<UButton>(BP, Tree, Name, true);
	FButtonStyle Style;
	Style.Normal = Surface(bPrimary ? FFBuildPearl::Charcoal : FFBuildPearl::Pearl, 20.0f,
		bPrimary ? FFBuildPearl::Cyan : FFBuildPearl::Line);
	Style.Hovered = Surface(bPrimary ? FFBuildPearl::Charcoal : FFBuildPearl::Pearl, 20.0f, FFBuildPearl::Cyan);
	Style.Pressed = Surface(FFBuildPearl::Charcoal, 20.0f, FFBuildPearl::Cyan);
	Style.Disabled = Surface(FFBuildPearl::Pearl, 20.0f, FFBuildPearl::Line);
	Style.NormalPadding = Style.PressedPadding = FMargin(9.0f, 5.0f);
	Button->SetStyle(Style);
	UButtonSlot* Slot = CastChecked<UButtonSlot>(Button->AddChild(MakeLabel(BP, Tree,
		FName(*FString::Printf(TEXT("Text_%s"), *Name.ToString())), Caption, 12,
		bPrimary ? FLinearColor::White : FFBuildPearl::Ink)));
	Slot->SetHorizontalAlignment(HAlign_Center);
	Slot->SetVerticalAlignment(VAlign_Center);
	return Button;
}

bool BuildLayout(UWidgetBlueprint& BP, UWidgetTree& Tree)
{
	UCanvasPanel* Canvas = MakeWidget<UCanvasPanel>(BP, Tree, TEXT("Canvas_MultiSelection"));
	USizeBox* Width = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_MultiSelection"));
	UBorder* Panel = MakeWidget<UBorder>(BP, Tree, TEXT("Border_MultiSelection"), true);
	UVerticalBox* Body = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Column_MultiSelection"));
	UHorizontalBox* Header = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_MultiSelectionHeader"));
	USizeBox* ScrollSize = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_MultiSelectionCards"));
	UScrollBox* Scroll = MakeWidget<UScrollBox>(BP, Tree, TEXT("Scroll_SelectedFacilities"));
	UHorizontalBox* Cards = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_SelectedFacilities"), true);
	if (!Canvas || !Width || !Panel || !Body || !Header || !ScrollSize || !Scroll || !Cards)
	{
		return false;
	}
	Tree.RootWidget = Canvas;
	Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Width->SetWidthOverride(920.0f);
	Width->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Width->AddChild(Panel);
	UCanvasPanelSlot* Position = Canvas->AddChildToCanvas(Width);
	Position->SetAnchors(FAnchors(0.5f, 1.0f));
	Position->SetAlignment(FVector2D(0.5f, 1.0f));
	Position->SetPosition(FVector2D(0.0f, -108.0f));
	Position->SetAutoSize(true);
	Panel->SetBrush(FFBuildPearl::MaterialBrush(TEXT("Panel")));
	Panel->SetBrushColor(FLinearColor::White);
	Panel->SetPadding(FMargin(13.0f, 9.0f));
	Panel->AddChild(Body);
	Body->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
	AddToRow(*Header, *MakeLabel(BP, Tree, TEXT("Text_MultiSelectionTitle"),
		LOCTEXT("Title", "다중 선택"), 15, FFBuildPearl::Ink), 10.0f);
	AddToRow(*Header, *MakeLabel(BP, Tree, TEXT("Text_MultiSelectionCount"),
		LOCTEXT("Count", "선택 0개"), 12, FFBuildPearl::Muted, true));
	AddToRow(*Header, *MakeWidget<USpacer>(BP, Tree, TEXT("Spacer_MultiSelectionHeader")),
		0.0f, true);
	AddToRow(*Header, *MakeAction(BP, Tree, TEXT("Button_InspectSelection"),
		LOCTEXT("Inspect", "상세 보기"), false), 7.0f);
	AddToRow(*Header, *MakeAction(BP, Tree, TEXT("Button_ClearSelection"),
		LOCTEXT("Clear", "전체 해제"), false), 7.0f);
	AddToRow(*Header, *MakeAction(BP, Tree, TEXT("Button_ExitMultiSelection"),
		LOCTEXT("Exit", "종료 ×"), true));
	ScrollSize->SetHeightOverride(64.0f);
	ScrollSize->AddChild(Scroll);
	Body->AddChildToVerticalBox(ScrollSize);
	Scroll->SetOrientation(Orient_Horizontal);
	Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	Scroll->SetAllowOverscroll(false);
	Scroll->AddChild(Cards);
	AddToRow(*Cards, *MakeLabel(BP, Tree, TEXT("Text_MultiSelectionDesignerHint"),
		LOCTEXT("DesignerHint", "시설을 클릭하거나 드래그하여 선택하세요"),
		13, FFBuildPearl::Muted));
	return true;
}

bool ValidateContract(const UWidgetBlueprint& BP)
{
	if (!BP.GeneratedClass || !BP.GeneratedClass->IsChildOf(UFF_BuildMultiSelectionWidget::StaticClass()) ||
		!BP.WidgetTree || !BP.WidgetTree->RootWidget) { return false; }
	return Cast<UBorder>(BP.WidgetTree->FindWidget(TEXT("Border_MultiSelection"))) &&
		Cast<UTextBlock>(BP.WidgetTree->FindWidget(TEXT("Text_MultiSelectionCount"))) &&
		Cast<UHorizontalBox>(BP.WidgetTree->FindWidget(TEXT("Row_SelectedFacilities"))) &&
		Cast<UButton>(BP.WidgetTree->FindWidget(TEXT("Button_InspectSelection"))) &&
		Cast<UButton>(BP.WidgetTree->FindWidget(TEXT("Button_ClearSelection"))) &&
		Cast<UButton>(BP.WidgetTree->FindWidget(TEXT("Button_ExitMultiSelection")));
}

void CreateWidgetAsset()
{
	if (FPackageName::DoesPackageExist(WidgetPackage) || FindPackage(nullptr, *WidgetPackage))
	{
		UWidgetBlueprint* Existing = LoadExistingBlueprint(WidgetPackage);
		if (Existing && ValidateContract(*Existing))
		{
			if(auto* Hint=Cast<UTextBlock>(Existing->WidgetTree->FindWidget(TEXT("Text_MultiSelectionDesignerHint"))))
			{
				Existing->Modify(); Hint->Modify();
				Hint->SetText(LOCTEXT("DesignerHint", "시설을 클릭하거나 드래그하여 선택하세요"));
				SaveNewBlueprint(TEXT("FF_BUILD_MULTI_UMG"),*Existing);
			}
			UE_LOG(LogFFUIWbpBuilder, Display,
				TEXT("FF_BUILD_MULTI_UMG_REUSE: existing Designer asset preserved: %s"), *WidgetPackage);
		}
		else
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("FF_BUILD_MULTI_UMG_FAIL: existing asset preserved; check parent and named widgets: %s"), *WidgetPackage);
		}
		return;
	}
	UWidgetBlueprint* BP = EnsureWidgetBlueprint(TEXT("FF_BUILD_MULTI_UMG"), WidgetPackage,
		WidgetFolder, *UFF_BuildMultiSelectionWidget::StaticClass(), BuildLayout);
	if (!BP || !ValidateContract(*BP))
	{
		UE_LOG(LogFFUIWbpBuilder, Error,
			TEXT("FF_BUILD_MULTI_UMG_FAIL: the new asset does not satisfy its binding contract: %s"), *WidgetPackage);
		return;
	}
	// Native instances start collapsed; the WBP remains visible for Designer authoring.
	UFF_BuildMultiSelectionWidget* Defaults = BP->GeneratedClass->GetDefaultObject<UFF_BuildMultiSelectionWidget>();
	Defaults->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	SaveAssetPackage(TEXT("FF_BUILD_MULTI_UMG"), *BP);
}

FAutoConsoleCommand CreateWidgetCommand(TEXT("FF.CreateBuildMultiSelectionWidget"),
	TEXT("Creates the missing editable build multi-selection WBP and preserves existing Designer changes."),
	FConsoleCommandDelegate::CreateStatic(&CreateWidgetAsset));
}

#undef LOCTEXT_NAMESPACE
#endif // WITH_EDITOR
