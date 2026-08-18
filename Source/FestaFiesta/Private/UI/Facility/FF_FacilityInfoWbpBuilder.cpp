// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFFacilityInfoWbpBuilder, Log, All);

namespace FFFacilityInfoWbpBuilder
{
	const FString PanelPackage(TEXT("/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityInfoPanel"));

	const FLinearColor ColorPanel(0.018f, 0.047f, 0.070f, 0.985f);
	const FLinearColor ColorSection(0.027f, 0.080f, 0.102f, 0.96f);
	const FLinearColor ColorPrimary(0.91f, 0.96f, 1.0f, 1.0f);
	const FLinearColor ColorMuted(0.58f, 0.70f, 0.76f, 1.0f);
	const FLinearColor ColorAccent(0.08f, 0.95f, 0.88f, 1.0f);
	const FLinearColor ColorWarning(1.0f, 0.73f, 0.23f, 1.0f);

	template <typename TWidget>
	TWidget* MakeWidget(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const FName Name,
		const bool bVariable = false)
	{
		TWidget* Widget = Tree.ConstructWidget<TWidget>(TWidget::StaticClass(), Name);
		if (Widget)
		{
			Widget->bIsVariable = bVariable;
			// UE 5.7's compiler requires a stable GUID for every WidgetTree node, not only
			// nodes exposed as Blueprint variables.
			Blueprint.OnVariableAdded(Name);
		}
		return Widget;
	}

	void StyleText(
		UCommonTextBlock& Text,
		const int32 FontSize,
		const FLinearColor& Color,
		const ETextJustify::Type Justification = ETextJustify::Left)
	{
		FSlateFontInfo Font = Text.GetFont();
		Font.Size = FontSize;
		Text.SetFont(Font);
		Text.SetColorAndOpacity(FSlateColor(Color));
		Text.SetJustification(Justification);
	}

	UVerticalBoxSlot* AddVertical(
		UVerticalBox& Parent,
		UWidget& Child,
		const FMargin Padding = FMargin(0.0f),
		const ESlateSizeRule::Type SizeRule = ESlateSizeRule::Automatic)
	{
		UVerticalBoxSlot* Slot = Parent.AddChildToVerticalBox(&Child);
		if (Slot)
		{
			Slot->SetPadding(Padding);
			Slot->SetSize(FSlateChildSize(SizeRule));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		return Slot;
	}

	UHorizontalBoxSlot* AddHorizontal(
		UHorizontalBox& Parent,
		UWidget& Child,
		const FMargin Padding = FMargin(0.0f),
		const ESlateSizeRule::Type SizeRule = ESlateSizeRule::Automatic)
	{
		UHorizontalBoxSlot* Slot = Parent.AddChildToHorizontalBox(&Child);
		if (Slot)
		{
			Slot->SetPadding(Padding);
			Slot->SetSize(FSlateChildSize(SizeRule));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		return Slot;
	}

	bool AddInfoRow(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		UVerticalBox& Parent,
		const TCHAR* RowName,
		const FText& LabelText,
		const FName ValueName,
		UCommonTextBlock*& OutValue)
	{
		UHorizontalBox* Row = MakeWidget<UHorizontalBox>(Blueprint, Tree, FName(RowName));
		UCommonTextBlock* Label = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, FName(*FString::Printf(TEXT("%s_Label"), RowName)));
		OutValue = MakeWidget<UCommonTextBlock>(Blueprint, Tree, ValueName, true);
		if (!Row || !Label || !OutValue)
		{
			return false;
		}
		Label->SetText(LabelText);
		StyleText(*Label, 13, ColorMuted);
		OutValue->SetText(FText::GetEmpty());
		StyleText(*OutValue, 14, ColorPrimary, ETextJustify::Right);
		AddHorizontal(*Row, *Label, FMargin(0.0f), ESlateSizeRule::Fill);
		AddHorizontal(*Row, *OutValue);
		AddVertical(Parent, *Row, FMargin(0.0f, 4.0f));
		return true;
	}

	UButton* MakeTextButton(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const FName ButtonName,
		const FName TextName,
		const FText& Label,
		const FLinearColor& Color,
		const bool bVariable)
	{
		UButton* Button = MakeWidget<UButton>(Blueprint, Tree, ButtonName, bVariable);
		UCommonTextBlock* Text = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TextName);
		if (!Button || !Text)
		{
			return nullptr;
		}
		Button->SetBackgroundColor(Color);
		Text->SetText(Label);
		StyleText(*Text, 14, FLinearColor::White, ETextJustify::Center);
		Button->AddChild(Text);
		return Button;
	}

	bool BuildLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("Root_Canvas"));
		USizeBox* PanelSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Panel_Size"));
		UBorder* Panel = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Panel"), true);
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_Content"));
		if (!Root || !PanelSize || !Panel || !Content)
		{
			return false;
		}

		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		PanelSize->SetWidthOverride(400.0f);
		PanelSize->SetMaxDesiredHeight(760.0f);
		Panel->SetBrushColor(ColorPanel);
		Panel->SetPadding(FMargin(18.0f));

		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Header_Row"));
		UCommonTextBlock* Context = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_Context"), true);
		USpacer* HeaderSpacer = MakeWidget<USpacer>(Blueprint, Tree, TEXT("Header_Spacer"));
		UButton* Close = MakeTextButton(
			Blueprint, Tree, TEXT("Button_Close"), TEXT("Close_Label"),
			FText::FromString(TEXT("닫기 ×")), FLinearColor(0.10f, 0.18f, 0.22f, 1.0f), true);
		if (!Header || !Context || !HeaderSpacer || !Close)
		{
			return false;
		}
		Context->SetText(FText::FromString(TEXT("시설 정보")));
		StyleText(*Context, 13, ColorAccent);
		HeaderSpacer->SetSize(FVector2D(8.0f, 1.0f));
		AddHorizontal(*Header, *Context);
		AddHorizontal(*Header, *HeaderSpacer, FMargin(0.0f), ESlateSizeRule::Fill);
		AddHorizontal(*Header, *Close);
		AddVertical(*Content, *Header, FMargin(0.0f, 0.0f, 0.0f, 12.0f));

		USizeBox* ThumbnailSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Thumbnail_Size"));
		UBorder* ThumbnailBackground = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Thumbnail_Background"));
		UOverlay* ThumbnailOverlay = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Thumbnail_Overlay"));
		UImage* Thumbnail = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Thumbnail"), true);
		UCommonTextBlock* ThumbnailPlaceholder = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_ThumbnailPlaceholder"), true);
		if (!ThumbnailSize || !ThumbnailBackground || !ThumbnailOverlay || !Thumbnail || !ThumbnailPlaceholder)
		{
			return false;
		}
		ThumbnailSize->SetHeightOverride(138.0f);
		ThumbnailBackground->SetBrushColor(FLinearColor(0.035f, 0.105f, 0.125f, 1.0f));
		ThumbnailPlaceholder->SetText(FText::FromString(TEXT("시설 이미지 준비 중")));
		StyleText(*ThumbnailPlaceholder, 13, ColorMuted, ETextJustify::Center);
		if (UOverlaySlot* Slot = ThumbnailOverlay->AddChildToOverlay(Thumbnail))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
		if (UOverlaySlot* Slot = ThumbnailOverlay->AddChildToOverlay(ThumbnailPlaceholder))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		ThumbnailBackground->AddChild(ThumbnailOverlay);
		ThumbnailSize->AddChild(ThumbnailBackground);
		AddVertical(*Content, *ThumbnailSize, FMargin(0.0f, 0.0f, 0.0f, 12.0f));

		UCommonTextBlock* FacilityName = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_FacilityName"), true);
		UHorizontalBox* MetaRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Meta_Row"));
		UCommonTextBlock* Category = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_Category"), true);
		UCommonTextBlock* Zone = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_Zone"), true);
		UCommonTextBlock* FacilityState = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_FacilityState"), true);
		UCommonTextBlock* RelocationWarning = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_RelocationWarning"), true);
		if (!FacilityName || !MetaRow || !Category || !Zone || !FacilityState || !RelocationWarning)
		{
			return false;
		}
		FacilityName->SetText(FText::GetEmpty());
		FacilityName->SetAutoWrapText(true);
		StyleText(*FacilityName, 24, ColorPrimary);
		Category->SetText(FText::GetEmpty());
		Zone->SetText(FText::GetEmpty());
		StyleText(*Category, 13, ColorAccent);
		StyleText(*Zone, 12, ColorMuted, ETextJustify::Right);
		FacilityState->SetText(FText::GetEmpty());
		StyleText(*FacilityState, 14, ColorPrimary);
		RelocationWarning->SetText(FText::GetEmpty());
		StyleText(*RelocationWarning, 13, ColorWarning);
		AddHorizontal(*MetaRow, *Category, FMargin(0.0f), ESlateSizeRule::Fill);
		AddHorizontal(*MetaRow, *Zone);
		AddVertical(*Content, *FacilityName, FMargin(0.0f, 0.0f, 0.0f, 5.0f));
		AddVertical(*Content, *MetaRow);
		AddVertical(*Content, *FacilityState, FMargin(0.0f, 5.0f, 0.0f, 0.0f));
		AddVertical(*Content, *RelocationWarning, FMargin(0.0f, 5.0f, 0.0f, 0.0f));

		UVerticalBox* BuildSection = MakeWidget<UVerticalBox>(
			Blueprint, Tree, TEXT("Panel_BuildSection"), true);
		UCommonTextBlock* BuildTitle = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Build_Title"));
		UCommonTextBlock* Footprint = nullptr;
		UCommonTextBlock* Clearance = nullptr;
		UCommonTextBlock* InstallCost = nullptr;
		UCommonTextBlock* RecoveryCost = nullptr;
		UCommonTextBlock* QueueCapacity = nullptr;
		UCommonTextBlock* ServiceDuration = nullptr;
		if (!BuildSection || !BuildTitle)
		{
			return false;
		}
		BuildTitle->SetText(FText::FromString(TEXT("배치 정보")));
		StyleText(*BuildTitle, 16, ColorAccent);
		AddVertical(*BuildSection, *BuildTitle, FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		if (!AddInfoRow(Blueprint, Tree, *BuildSection, TEXT("Footprint_Row"),
			FText::FromString(TEXT("시설 크기")), TEXT("Text_Footprint"), Footprint) ||
			!AddInfoRow(Blueprint, Tree, *BuildSection, TEXT("Clearance_Row"),
				FText::FromString(TEXT("안전 여유")), TEXT("Text_Clearance"), Clearance) ||
			!AddInfoRow(Blueprint, Tree, *BuildSection, TEXT("InstallCost_Row"),
				FText::FromString(TEXT("설치 비용")), TEXT("Text_InstallCost"), InstallCost) ||
			!AddInfoRow(Blueprint, Tree, *BuildSection, TEXT("RecoveryCost_Row"),
				FText::FromString(TEXT("철거 회수액")), TEXT("Text_RecoveryCost"), RecoveryCost) ||
			!AddInfoRow(Blueprint, Tree, *BuildSection, TEXT("QueueCapacity_Row"),
				FText::FromString(TEXT("이용 규모")), TEXT("Text_QueueCapacity"), QueueCapacity) ||
			!AddInfoRow(Blueprint, Tree, *BuildSection, TEXT("ServiceDuration_Row"),
				FText::FromString(TEXT("서비스 시간")), TEXT("Text_ServiceDuration"), ServiceDuration))
		{
			return false;
		}
		UHorizontalBox* ActionRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Build_ActionRow"));
		UButton* Move = MakeTextButton(
			Blueprint, Tree, TEXT("Button_Move"), TEXT("Move_Label"),
			FText::FromString(TEXT("이동  M")), FLinearColor(0.04f, 0.48f, 0.58f, 1.0f), true);
		UButton* Remove = MakeTextButton(
			Blueprint, Tree, TEXT("Button_Remove"), TEXT("Remove_Label"),
			FText::FromString(TEXT("철거  Enter")), FLinearColor(0.72f, 0.10f, 0.08f, 1.0f), true);
		if (!ActionRow || !Move || !Remove)
		{
			return false;
		}
		AddHorizontal(*ActionRow, *Move, FMargin(0.0f, 0.0f, 5.0f, 0.0f), ESlateSizeRule::Fill);
		AddHorizontal(*ActionRow, *Remove, FMargin(5.0f, 0.0f, 0.0f, 0.0f), ESlateSizeRule::Fill);
		AddVertical(*BuildSection, *ActionRow, FMargin(0.0f, 10.0f, 0.0f, 0.0f));

		UVerticalBox* LiveSection = MakeWidget<UVerticalBox>(
			Blueprint, Tree, TEXT("Panel_LiveSection"), true);
		UCommonTextBlock* LiveTitle = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Live_Title"));
		UCommonTextBlock* OperationState = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_OperationState"), true);
		UCommonTextBlock* WaitingParties = nullptr;
		UCommonTextBlock* Occupancy = nullptr;
		UCommonTextBlock* RemainingService = nullptr;
		UCommonTextBlock* ExpectedWait = nullptr;
		UCommonTextBlock* Throughput = nullptr;
		if (!LiveSection || !LiveTitle || !OperationState)
		{
			return false;
		}
		LiveTitle->SetText(FText::FromString(TEXT("실시간 운영")));
		StyleText(*LiveTitle, 16, ColorAccent);
		OperationState->SetText(FText::GetEmpty());
		StyleText(*OperationState, 15, ColorPrimary);
		AddVertical(*LiveSection, *LiveTitle, FMargin(0.0f, 0.0f, 0.0f, 5.0f));
		AddVertical(*LiveSection, *OperationState, FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		if (!AddInfoRow(Blueprint, Tree, *LiveSection, TEXT("WaitingParties_Row"),
			FText::FromString(TEXT("대기/처리 중 일행")), TEXT("Text_WaitingParties"), WaitingParties) ||
			!AddInfoRow(Blueprint, Tree, *LiveSection, TEXT("Occupancy_Row"),
				FText::FromString(TEXT("점유 인원")), TEXT("Text_Occupancy"), Occupancy) ||
			!AddInfoRow(Blueprint, Tree, *LiveSection, TEXT("RemainingService_Row"),
				FText::FromString(TEXT("남은 서비스 인원")), TEXT("Text_RemainingService"), RemainingService) ||
			!AddInfoRow(Blueprint, Tree, *LiveSection, TEXT("ExpectedWait_Row"),
				FText::FromString(TEXT("1인 기준 예상 대기")), TEXT("Text_ExpectedWait"), ExpectedWait) ||
			!AddInfoRow(Blueprint, Tree, *LiveSection, TEXT("Throughput_Row"),
				FText::FromString(TEXT("처리 속도")), TEXT("Text_Throughput"), Throughput))
		{
			return false;
		}

		UBorder* SectionBackground = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Section_Background"));
		UOverlay* SectionSwitcher = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Section_Switcher"));
		if (!SectionBackground || !SectionSwitcher)
		{
			return false;
		}
		SectionBackground->SetBrushColor(ColorSection);
		SectionBackground->SetPadding(FMargin(14.0f, 12.0f));
		if (UOverlaySlot* Slot = SectionSwitcher->AddChildToOverlay(BuildSection))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Top);
		}
		if (UOverlaySlot* Slot = SectionSwitcher->AddChildToOverlay(LiveSection))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Top);
		}
		SectionBackground->AddChild(SectionSwitcher);
		AddVertical(*Content, *SectionBackground, FMargin(0.0f, 14.0f, 0.0f, 0.0f));

		UCommonTextBlock* Feedback = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_Feedback"), true);
		if (!Feedback)
		{
			return false;
		}
		Feedback->SetText(FText::GetEmpty());
		Feedback->SetAutoWrapText(true);
		StyleText(*Feedback, 12, ColorWarning, ETextJustify::Center);
		AddVertical(*Content, *Feedback, FMargin(0.0f, 10.0f, 0.0f, 0.0f));

		Panel->AddChild(Content);
		PanelSize->AddChild(Panel);
		if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(PanelSize))
		{
			PanelSlot->SetAnchors(FAnchors(1.0f, 0.5f));
			PanelSlot->SetAlignment(FVector2D(1.0f, 0.5f));
			PanelSlot->SetPosition(FVector2D(-32.0f, 0.0f));
			PanelSlot->SetAutoSize(true);
		}
		else
		{
			return false;
		}
		Tree.RootWidget = Root;
		return true;
	}

	UWidgetBlueprint* LoadExistingBlueprint()
	{
		const FString AssetName = FPackageName::GetLongPackageAssetName(PanelPackage);
		return LoadObject<UWidgetBlueprint>(
			nullptr, *FString::Printf(TEXT("%s.%s"), *PanelPackage, *AssetName));
	}

	bool HasRequiredLayout(const UWidgetBlueprint& Blueprint)
	{
		const UWidgetTree* Tree = Blueprint.WidgetTree;
		if (!Tree || Blueprint.Status == BS_Error)
		{
			return false;
		}

		const UCanvasPanel* Root = Cast<UCanvasPanel>(Tree->RootWidget);
		const USizeBox* PanelSize = Cast<USizeBox>(Tree->FindWidget(TEXT("Panel_Size")));
		const UCanvasPanelSlot* PanelSlot = PanelSize
			? Cast<UCanvasPanelSlot>(PanelSize->Slot)
			: nullptr;
		if (!Root || Root->GetVisibility() != ESlateVisibility::SelfHitTestInvisible || !PanelSlot)
		{
			return false;
		}
		const FAnchors Anchors = PanelSlot->GetAnchors();
		if (!Anchors.Minimum.Equals(FVector2D(1.0f, 0.5f)) ||
			!Anchors.Maximum.Equals(FVector2D(1.0f, 0.5f)) ||
			!PanelSlot->GetAlignment().Equals(FVector2D(1.0f, 0.5f)) ||
			PanelSlot->GetPosition().ContainsNaN() ||
			!PanelSlot->GetAutoSize())
		{
			return false;
		}

		const TArray<FName> RequiredVariables = {
			TEXT("Border_Panel"), TEXT("Button_Close"), TEXT("Button_Move"),
			TEXT("Button_Remove"), TEXT("Image_Thumbnail"), TEXT("Text_ThumbnailPlaceholder"),
			TEXT("Panel_BuildSection"), TEXT("Panel_LiveSection"), TEXT("Text_Context"),
			TEXT("Text_FacilityName"), TEXT("Text_Category"), TEXT("Text_Zone"),
			TEXT("Text_FacilityState"), TEXT("Text_RelocationWarning"), TEXT("Text_Footprint"),
			TEXT("Text_Clearance"), TEXT("Text_InstallCost"), TEXT("Text_RecoveryCost"),
			TEXT("Text_QueueCapacity"), TEXT("Text_ServiceDuration"), TEXT("Text_OperationState"),
			TEXT("Text_WaitingParties"), TEXT("Text_Occupancy"), TEXT("Text_RemainingService"),
			TEXT("Text_ExpectedWait"), TEXT("Text_Throughput"), TEXT("Text_Feedback")
		};
		for (const FName Name : RequiredVariables)
		{
			const UWidget* Widget = Tree->FindWidget(Name);
			if (!Widget || !Widget->bIsVariable ||
				!Blueprint.WidgetVariableNameToGuidMap.Contains(Name))
			{
				return false;
			}
		}
		return Cast<UBorder>(Tree->FindWidget(TEXT("Border_Panel"))) &&
			Cast<UButton>(Tree->FindWidget(TEXT("Button_Close"))) &&
			Cast<UButton>(Tree->FindWidget(TEXT("Button_Move"))) &&
			Cast<UButton>(Tree->FindWidget(TEXT("Button_Remove"))) &&
			Cast<UImage>(Tree->FindWidget(TEXT("Image_Thumbnail"))) &&
			Tree->FindWidget(TEXT("Text_ThumbnailPlaceholder")) &&
			Tree->FindWidget(TEXT("Panel_BuildSection")) &&
			Tree->FindWidget(TEXT("Panel_LiveSection")) &&
			Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("Text_Context"))) &&
			Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("Text_FacilityName"))) &&
			Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("Text_Category"))) &&
			Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("Text_Zone"))) &&
			Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("Text_FacilityState"))) &&
			Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("Text_RelocationWarning"))) &&
			Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("Text_Footprint"))) &&
			Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("Text_Clearance"))) &&
			Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("Text_InstallCost"))) &&
			Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("Text_RecoveryCost"))) &&
			Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("Text_QueueCapacity"))) &&
			Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("Text_ServiceDuration"))) &&
			Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("Text_OperationState"))) &&
			Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("Text_WaitingParties"))) &&
			Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("Text_Occupancy"))) &&
			Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("Text_RemainingService"))) &&
			Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("Text_ExpectedWait"))) &&
			Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("Text_Throughput"))) &&
			Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("Text_Feedback")));
	}

	bool SaveNewBlueprint(UWidgetBlueprint& Blueprint)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&Blueprint);
		FKismetEditorUtilities::CompileBlueprint(&Blueprint);
		if (Blueprint.Status == BS_Error)
		{
			return false;
		}
		Blueprint.MarkPackageDirty();
		UPackage* Package = Blueprint.GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, &Blueprint, *Filename, Args);
	}

	void BuildFacilityInfoUmg()
	{
		// Never rewrite an existing designer-owned asset. Validate its public contract instead.
		if (FPackageName::DoesPackageExist(PanelPackage))
		{
			UWidgetBlueprint* Existing = LoadExistingBlueprint();
			if (Existing && Existing->GeneratedClass &&
				Existing->GeneratedClass->IsChildOf(UFF_FacilityInfoPanel::StaticClass()) &&
				HasRequiredLayout(*Existing))
			{
				UE_LOG(LogFFFacilityInfoWbpBuilder, Display,
					TEXT("FF_FACILITY_INFO_UMG_REUSE: %s (designer changes preserved)"), *PanelPackage);
			}
			else
			{
				UE_LOG(LogFFFacilityInfoWbpBuilder, Error,
					TEXT("FF_FACILITY_INFO_UMG_FAIL: existing asset violates parent/widget contract: %s"),
					*PanelPackage);
			}
			return;
		}

		UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
		Factory->ParentClass = UFF_FacilityInfoPanel::StaticClass();
		FAssetToolsModule& AssetTools =
			FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		const FString AssetName = FPackageName::GetLongPackageAssetName(PanelPackage);
		const FString PackagePath = FPackageName::GetLongPackagePath(PanelPackage);
		UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(AssetTools.Get().CreateAsset(
			AssetName, PackagePath, UWidgetBlueprint::StaticClass(), Factory));
		if (!Blueprint || !Blueprint->WidgetTree ||
			!BuildLayout(*Blueprint, *Blueprint->WidgetTree) ||
			!HasRequiredLayout(*Blueprint) ||
			!SaveNewBlueprint(*Blueprint))
		{
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error,
				TEXT("FF_FACILITY_INFO_UMG_FAIL: could not create %s"), *PanelPackage);
			return;
		}

		UE_LOG(LogFFFacilityInfoWbpBuilder, Display,
			TEXT("FF_FACILITY_INFO_UMG_CREATED: %s"), *PanelPackage);
	}

	FAutoConsoleCommand BuildFacilityInfoUmgCommand(
		TEXT("FF.BuildFacilityInfoUMG"),
		TEXT("Creates the editable right-side installed-facility information WBP when absent."),
		FConsoleCommandDelegate::CreateStatic(&BuildFacilityInfoUmg));
}

#endif // WITH_EDITOR
