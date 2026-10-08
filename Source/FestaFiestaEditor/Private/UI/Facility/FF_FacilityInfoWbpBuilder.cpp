// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// FF_FacilityInfoWbpBuilder.cpp
// 역할: 시설 정보·조명·유료 프로그램 화면의 초기 생성과 명시적인 에셋 수정을 담당한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Facility/FF_FacilityInfoWbpBuilder.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "UI/Facility/FF_FacilityPaidProgramLayout.h"

#if WITH_EDITOR

#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/Slider.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "Components/PanelWidget.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UI/Facility/FF_FacilityInfoUpgradeLayout.h"
#include "UI/Facility/FF_FacilityPaidProgramCard.h"
#include "UI/Facility/FF_FacilityPaidProgramPicker.h"
#include "UI/Facility/FF_FacilitySynergyEntryWidget.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditorUtils.h"
#include "WidgetBlueprintFactory.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFFacilityInfoWbpBuilder, Log, All);

namespace FFFacilityInfoWbpBuilder
{
	const FString PanelPackage(FFBuildUIAssetPaths::FacilityInfoPanelPackage);
	const FString LightingPackage(FFBuildUIAssetPaths::FacilityLightingControlsPackage);
	const FString SynergyEntryPackage(FFBuildUIAssetPaths::FacilitySynergyEntryPackage);
	bool SaveBlueprint(UWidgetBlueprint& Blueprint);
	bool HasNamedVariable(const UWidgetBlueprint& Blueprint, FName Name, const UClass& ExpectedClass);
	bool MigrateInspectorControls(UWidgetBlueprint& Blueprint, UWidgetTree& Tree);
	bool MigratePurchaseControls(UWidgetBlueprint& Blueprint, UWidgetTree& Tree);
	const FString PaidProgramPickerPackage(
		FFBuildUIAssetPaths::FacilityPaidProgramPickerPackage);
	const FString PaidProgramCardPackage(
		FFBuildUIAssetPaths::FacilityPaidProgramCardPackage);

	const FLinearColor ColorPanel(0.018f, 0.047f, 0.070f, 0.985f);
	const FLinearColor ColorSection(0.027f, 0.080f, 0.102f, 0.96f);
	const FLinearColor ColorPrimary(0.91f, 0.96f, 1.0f, 1.0f);
	const FLinearColor ColorMuted(0.58f, 0.70f, 0.76f, 1.0f);
	const FLinearColor ColorAccent(0.08f, 0.95f, 0.88f, 1.0f);
	const FLinearColor ColorWarning(1.0f, 0.73f, 0.23f, 1.0f);

	constexpr float PanelWidth = FFFacilityPaidProgramLayout::InspectorWidth;
	constexpr float PanelHeight = 500.0f;
	constexpr float PanelRightMargin = FFFacilityPaidProgramLayout::RightMargin;
	constexpr float PanelTopMargin = FFFacilityPaidProgramLayout::TopMargin;

	FSlateBrush MakeSolidBrush(
		const FLinearColor& Fill,
		const float CornerRadius,
		const FLinearColor& OutlineColor = FLinearColor::Transparent,
		const float OutlineWidth = 0.0f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(
			CornerRadius, FSlateColor(OutlineColor), OutlineWidth);
		return Brush;
	}

	void PaintBorder(UBorder& Border, const FSlateBrush& Brush)
	{
		Border.SetBrush(Brush);
		Border.SetBrushColor(FLinearColor::White);
	}

	using FFUIWbpBuilder::MakeWidget;

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

	void StyleLightText(
		UCommonTextBlock& Text,
		const int32 FontSize,
		const FLinearColor& Color,
		const FName Typeface = TEXT("Regular"),
		const ETextJustify::Type Justification = ETextJustify::Left)
	{
		FFUIWbpBuilder::StyleText(Text, FontSize, Color, Typeface);
		Text.SetJustification(Justification);
	}

	UVerticalBoxSlot* AddVertical(
		UVerticalBox& Parent,
		UWidget& Child,
		const FMargin Padding = FMargin(0.0f),
		const ESlateSizeRule::Type SizeRule = ESlateSizeRule::Automatic,
		const EVerticalAlignment VerticalAlignment = VAlign_Center)
	{
		UVerticalBoxSlot* Slot = Parent.AddChildToVerticalBox(&Child);
		if (Slot)
		{
			Slot->SetPadding(Padding);
			Slot->SetSize(FSlateChildSize(SizeRule));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VerticalAlignment);
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
		const UFF_UIPalette& Palette,
		const TCHAR* RowName,
		const FText& LabelText,
		const FName ValueName,
		UCommonTextBlock*& OutValue)
	{
		UBorder* Surface = MakeWidget<UBorder>(
			Blueprint, Tree, FName(*FString::Printf(TEXT("%s_Surface"), RowName)));
		UHorizontalBox* Row = MakeWidget<UHorizontalBox>(Blueprint, Tree, FName(RowName));
		UCommonTextBlock* Label = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, FName(*FString::Printf(TEXT("%s_Label"), RowName)));
		OutValue = MakeWidget<UCommonTextBlock>(Blueprint, Tree, ValueName, true);
		if (!Surface || !Row || !Label || !OutValue)
		{
			return false;
		}
		PaintBorder(*Surface, MakeSolidBrush(Palette.InGame.PaperSunken, 5.0f));
		Surface->SetPadding(FMargin(10.0f, 6.0f));
		Label->SetText(LabelText);
		StyleLightText(*Label, 12, Palette.InGame.InkMuted, TEXT("Medium"));
		OutValue->SetText(FText::GetEmpty());
		StyleLightText(
			*OutValue, 13, Palette.InGame.Ink, TEXT("SemiBold"), ETextJustify::Right);
		AddHorizontal(*Row, *Label, FMargin(0.0f), ESlateSizeRule::Fill);
		AddHorizontal(*Row, *OutValue);
		Surface->AddChild(Row);
		AddVertical(Parent, *Surface, FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		return true;
	}

	UButton* MakeTextButton(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const FName ButtonName,
		const FName TextName,
		const FText& Label,
		const FLinearColor& Color,
		const bool bVariable,
		const bool bTextVariable = false)
	{
		UButton* Button = MakeWidget<UButton>(Blueprint, Tree, ButtonName, bVariable);
		UCommonTextBlock* Text = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TextName, bTextVariable);
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

	UButton* MakeLightTextButton(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const FName ButtonName,
		const FName TextName,
		const FText& Label,
		const FLinearColor& NormalColor,
		const FLinearColor& HoverColor,
		const FLinearColor& PressedColor,
		const FLinearColor& DisabledColor,
		const FLinearColor& TextColor,
		const bool bVariable,
		const bool bTextVariable = false,
		const FLinearColor& OutlineColor = FLinearColor::Transparent)
	{
		UButton* Button = MakeWidget<UButton>(Blueprint, Tree, ButtonName, bVariable);
		UCommonTextBlock* Text = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TextName, bTextVariable);
		if (!Button || !Text)
		{
			return nullptr;
		}

		FButtonStyle Style;
		Style.Normal = MakeSolidBrush(NormalColor, 6.0f, OutlineColor, 1.0f);
		Style.Hovered = MakeSolidBrush(HoverColor, 6.0f, OutlineColor, 1.0f);
		Style.Pressed = MakeSolidBrush(PressedColor, 6.0f, OutlineColor, 1.0f);
		Style.Disabled = MakeSolidBrush(DisabledColor, 6.0f, OutlineColor, 1.0f);
		Style.NormalPadding = FMargin(12.0f, 8.0f);
		Style.PressedPadding = FMargin(12.0f, 8.0f);
		Button->SetStyle(Style);

		Text->SetText(Label);
		StyleLightText(*Text, 13, TextColor, TEXT("SemiBold"), ETextJustify::Center);
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Button->AddChild(Text)))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		return Button;
	}

	const TArray<FName>& GetProgramContractVariableNames()
	{
		static const TArray<FName> Names = {
			TEXT("Panel_ProgramSection"),
			TEXT("Text_ProgramStatus"),
			TEXT("Text_ScheduledProgramSummary"),
			TEXT("Text_ProgramScheduleReason"),
			TEXT("Button_ScheduleProgram"),
			TEXT("Text_ScheduleProgram")
		};
		return Names;
	}

	bool HasProgramWidgetContract(const UWidgetBlueprint& Blueprint)
	{
		const UWidgetTree* Tree = Blueprint.WidgetTree;
		if (!Tree)
		{
			return false;
		}
		for (const FName Name : GetProgramContractVariableNames())
		{
			const UWidget* Widget = Tree->FindWidget(Name);
			if (!Widget || !Widget->bIsVariable ||
				!Blueprint.WidgetVariableNameToGuidMap.Contains(Name))
			{
				return false;
			}
		}

		return Tree->FindWidget(TEXT("Panel_ProgramSection")) &&
			Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("Text_ProgramStatus"))) &&
			Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("Text_ScheduledProgramSummary"))) &&
			Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("Text_ProgramScheduleReason"))) &&
			Cast<UButton>(Tree->FindWidget(TEXT("Button_ScheduleProgram"))) &&
			Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("Text_ScheduleProgram")));
	}

	void EnsureVariableContract(
		UWidgetBlueprint& Blueprint,
		UWidget& Widget,
		const FName Name,
		bool& bOutModified)
	{
		if (!Widget.bIsVariable)
		{
			Widget.bIsVariable = true;
			bOutModified = true;
		}
		if (!Blueprint.WidgetVariableNameToGuidMap.Contains(Name))
		{
			Blueprint.OnVariableAdded(Name);
			bOutModified = true;
		}
	}

	/**
	 * Idempotent, non-destructive migration for the paid-program block.
	 *
	 * Existing designer nodes are never rebuilt or restyled. Missing nodes are appended only when
	 * the known `Panel_Content`/VerticalBox seam is available; an incompatible partial designer
	 * layout fails validation instead of being overwritten.
	 */
	bool EnsureProgramWidgetContract(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		bool& bOutModified)
	{
		bOutModified = false;
		if (HasProgramWidgetContract(Blueprint))
		{
			return true;
		}

		UWidget* ExistingProgramSection = Tree.FindWidget(TEXT("Panel_ProgramSection"));
		UVerticalBox* ProgramSection = Cast<UVerticalBox>(ExistingProgramSection);
		if (ExistingProgramSection && !ProgramSection)
		{
			return false;
		}

		if (!ProgramSection)
		{
			UVerticalBox* Content = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Panel_Content")));
			if (!Content)
			{
				return false;
			}

			ProgramSection = MakeWidget<UVerticalBox>(
				Blueprint, Tree, TEXT("Panel_ProgramSection"), true);
			UCommonTextBlock* ProgramTitle = MakeWidget<UCommonTextBlock>(
				Blueprint, Tree, TEXT("Program_Title"));
			if (!ProgramSection || !ProgramTitle)
			{
				return false;
			}
			ProgramSection->SetVisibility(ESlateVisibility::Collapsed);
			ProgramTitle->SetText(FText::FromString(TEXT("유료 프로그램")));
			StyleText(*ProgramTitle, 16, ColorAccent);
			AddVertical(*ProgramSection, *ProgramTitle, FMargin(0.0f, 0.0f, 0.0f, 5.0f));
			AddVertical(*Content, *ProgramSection, FMargin(0.0f, 14.0f, 0.0f, 0.0f));
			bOutModified = true;
		}
		else
		{
			EnsureVariableContract(
				Blueprint, *ProgramSection, TEXT("Panel_ProgramSection"), bOutModified);
		}

		auto EnsureText = [&](
			const FName Name,
			const FText& DefaultText,
			const int32 FontSize,
			const FLinearColor& Color,
			const FMargin Padding) -> UCommonTextBlock*
		{
			UWidget* Existing = Tree.FindWidget(Name);
			UCommonTextBlock* Text = Cast<UCommonTextBlock>(Existing);
			if (Existing && !Text)
			{
				return nullptr;
			}
			if (!Text)
			{
				Text = MakeWidget<UCommonTextBlock>(Blueprint, Tree, Name, true);
				if (!Text)
				{
					return nullptr;
				}
				Text->SetText(DefaultText);
				Text->SetAutoWrapText(true);
				StyleText(*Text, FontSize, Color);
				AddVertical(*ProgramSection, *Text, Padding);
				bOutModified = true;
			}
			else
			{
				EnsureVariableContract(Blueprint, *Text, Name, bOutModified);
			}
			return Text;
		};

		if (!EnsureText(
				TEXT("Text_ProgramStatus"),
				FText::FromString(TEXT("오늘 편성 없음")),
				14,
				ColorPrimary,
				FMargin(0.0f, 0.0f, 0.0f, 4.0f)) ||
			!EnsureText(
				TEXT("Text_ScheduledProgramSummary"),
				FText::GetEmpty(),
				13,
				ColorPrimary,
				FMargin(0.0f, 0.0f, 0.0f, 4.0f)) ||
			!EnsureText(
				TEXT("Text_ProgramScheduleReason"),
				FText::GetEmpty(),
				12,
				ColorWarning,
				FMargin(0.0f, 0.0f, 0.0f, 5.0f)))
		{
			return false;
		}

		UWidget* ExistingButtonWidget = Tree.FindWidget(TEXT("Button_ScheduleProgram"));
		UButton* ScheduleButton = Cast<UButton>(ExistingButtonWidget);
		UWidget* ExistingButtonTextWidget = Tree.FindWidget(TEXT("Text_ScheduleProgram"));
		UCommonTextBlock* ScheduleButtonText = Cast<UCommonTextBlock>(ExistingButtonTextWidget);
		if ((ExistingButtonWidget && !ScheduleButton) ||
			(ExistingButtonTextWidget && !ScheduleButtonText) ||
			(static_cast<bool>(ScheduleButton) != static_cast<bool>(ScheduleButtonText)))
		{
			return false;
		}
		if (!ScheduleButton)
		{
			ScheduleButton = MakeTextButton(
				Blueprint,
				Tree,
				TEXT("Button_ScheduleProgram"),
				TEXT("Text_ScheduleProgram"),
				FText::FromString(TEXT("유료 프로그램 편성")),
				FLinearColor(0.02f, 0.55f, 0.72f, 1.0f),
				true,
				true);
			if (!ScheduleButton)
			{
				return false;
			}
			AddVertical(*ProgramSection, *ScheduleButton, FMargin(0.0f, 5.0f, 0.0f, 0.0f));
			bOutModified = true;
		}
		else
		{
			EnsureVariableContract(
				Blueprint, *ScheduleButton, TEXT("Button_ScheduleProgram"), bOutModified);
			EnsureVariableContract(
				Blueprint, *ScheduleButtonText, TEXT("Text_ScheduleProgram"), bOutModified);
		}

		return HasProgramWidgetContract(Blueprint);
	}

	const TArray<FName>& GetUpgradeContractVariableNames()
	{
		static const TArray<FName> Names = {
			TEXT("Panel_UpgradeSection"), TEXT("Text_FacilityLevel"),
			TEXT("Text_UpgradeEffect"), TEXT("Text_UpgradeCost"), TEXT("Text_UpgradeReason"),
			TEXT("Button_Upgrade"), TEXT("Text_Upgrade"), TEXT("Text_FacilityRevenue")
		};
		return Names;
	}

	bool HasUpgradeWidgetContract(const UWidgetBlueprint& Blueprint)
	{
		const UWidgetTree* Tree = Blueprint.WidgetTree;
		if (!Tree)
		{
			return false;
		}
		for (const FName Name : GetUpgradeContractVariableNames())
		{
			const UWidget* Widget = Tree->FindWidget(Name);
			const UClass* ExpectedClass = Name == TEXT("Panel_UpgradeSection")
				? UVerticalBox::StaticClass()
				: (Name == TEXT("Button_Upgrade") ? UButton::StaticClass() : UCommonTextBlock::StaticClass());
			if (!Widget || !Widget->IsA(ExpectedClass) || !Widget->bIsVariable ||
				!Blueprint.WidgetVariableNameToGuidMap.Contains(Name))
			{
				return false;
			}
		}
		return true;
	}

	/** Preserve authored nodes/styles; add missing controls and correct only the upgrade section order. */
	bool EnsureUpgradeWidgetContract(
		UWidgetBlueprint& Blueprint, UWidgetTree& Tree, bool& bOutModified)
	{
		bOutModified = false;
		if (HasUpgradeWidgetContract(Blueprint))
		{
			return FFFacilityInfoUpgradeLayout::PlaceAfterSummary(
				Tree, *Tree.FindWidget(TEXT("Panel_UpgradeSection")), bOutModified);
		}
		UVerticalBox* Content = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Panel_Content")));
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Content || !Palette)
		{
			return false;
		}
		// Validate partial designer assets before adding anything to them.
		for (const FName Name : GetUpgradeContractVariableNames())
		{
			const UWidget* Existing = Tree.FindWidget(Name);
			const UClass* ExpectedClass = Name == TEXT("Panel_UpgradeSection")
				? UVerticalBox::StaticClass()
				: (Name == TEXT("Button_Upgrade") ? UButton::StaticClass() : UCommonTextBlock::StaticClass());
			if (Existing && !Existing->IsA(ExpectedClass))
			{
				return false;
			}
		}
		const UButton* ExistingButton = Cast<UButton>(Tree.FindWidget(TEXT("Button_Upgrade")));
		UVerticalBox* SummaryMeta = FFFacilityInfoUpgradeLayout::FindRevenueParent(Tree);
		if (!Tree.FindWidget(TEXT("Text_FacilityRevenue")))
		{
			if (!SummaryMeta)
			{
				return false;
			}
			UCommonTextBlock* Revenue = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_FacilityRevenue"), true);
			Revenue->SetAutoWrapText(true);
			StyleLightText(*Revenue, 11, Palette->InGame.Ink, TEXT("Medium"));
			AddVertical(*SummaryMeta, *Revenue, FMargin(0.f, 4.f, 0.f, 0.f));
			if (UWidget* State = FFFacilityInfoUpgradeLayout::FindContentChild(Tree.FindWidget(TEXT("Text_FacilityState")), *SummaryMeta))
			{
				SummaryMeta->ShiftChild(SummaryMeta->GetChildIndex(State) + 1, Revenue);
			}
			bOutModified = true;
		}
		const UWidget* ExistingButtonText = Tree.FindWidget(TEXT("Text_Upgrade"));
		if ((!ExistingButton && ExistingButtonText) ||
			(ExistingButton && !ExistingButtonText && ExistingButton->GetContent()))
		{
			return false;
		}
		UVerticalBox* Section = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Panel_UpgradeSection")));
		if (!Section)
		{
			Section = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_UpgradeSection"), true);
			AddVertical(*Content, *Section, FMargin(2.0f, 8.0f, 2.0f, 10.0f));
			bOutModified = true;
		}
		for (const FName Name : {FName(TEXT("Text_FacilityLevel")), FName(TEXT("Text_UpgradeEffect")),
			FName(TEXT("Text_UpgradeCost")), FName(TEXT("Text_UpgradeReason"))})
		{
			UCommonTextBlock* Text = Cast<UCommonTextBlock>(Tree.FindWidget(Name));
			if (!Text)
			{
				Text = MakeWidget<UCommonTextBlock>(Blueprint, Tree, Name, true);
				Text->SetAutoWrapText(true);
				const bool bTitle = Name == TEXT("Text_FacilityLevel");
				const bool bReason = Name == TEXT("Text_UpgradeReason");
				Text->SetText(bTitle ? FText::FromString(TEXT("시설 강화 · Lv.1")) : FText::GetEmpty());
				StyleLightText(*Text, bTitle ? 14 : 12,
					bTitle ? Palette->InGame.TealDeep : (bReason ? Palette->InGame.Amber : Palette->InGame.Ink),
					bTitle ? FName(TEXT("SemiBold")) : FName(TEXT("Medium")));
				AddVertical(*Section, *Text, FMargin(0.0f, 0.0f, 0.0f, 6.0f));
				bOutModified = true;
			}
		}
		UButton* Button = Cast<UButton>(Tree.FindWidget(TEXT("Button_Upgrade")));
		UCommonTextBlock* ButtonText = Cast<UCommonTextBlock>(Tree.FindWidget(TEXT("Text_Upgrade")));
		if (!Button)
		{
			if (ButtonText)
			{
				return false;
			}
			Button = MakeLightTextButton(Blueprint, Tree, TEXT("Button_Upgrade"), TEXT("Text_Upgrade"),
				FText::FromString(TEXT("시설 강화")), Palette->InGame.Teal, Palette->InGame.TealDeep,
				Palette->InGame.TealDeep, Palette->InGame.LineStrong, Palette->InGame.White, true, true);
			Button->SetIsEnabled(false);
			AddVertical(*Section, *Button);
			bOutModified = true;
		}
		else if (!ButtonText)
		{
			if (Button->GetContent())
			{
				return false;
			}
			ButtonText = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_Upgrade"), true);
			ButtonText->SetText(FText::FromString(TEXT("시설 강화")));
			StyleLightText(*ButtonText, 13, Palette->InGame.White, TEXT("SemiBold"), ETextJustify::Center);
			Button->AddChild(ButtonText);
			bOutModified = true;
		}
		for (const FName Name : GetUpgradeContractVariableNames())
		{
			if (UWidget* Widget = Tree.FindWidget(Name))
			{
				EnsureVariableContract(Blueprint, *Widget, Name, bOutModified);
			}
		}
		return HasUpgradeWidgetContract(Blueprint) &&
			FFFacilityInfoUpgradeLayout::PlaceAfterSummary(Tree, *Section, bOutModified);
	}

	bool HasInstanceLimitWidgetContract(const UWidgetBlueprint& Blueprint)
	{
		const UWidgetTree* Tree = Blueprint.WidgetTree;
		const UWidget* Widget = Tree
			? Tree->FindWidget(TEXT("Text_InstanceLimit"))
			: nullptr;
		return Cast<UCommonTextBlock>(Widget) && Widget->bIsVariable &&
			Blueprint.WidgetVariableNameToGuidMap.Contains(FName(TEXT("Text_InstanceLimit")));
	}

	/** Adds only the missing build-limit row to an existing designer-owned information panel. */
	bool EnsureInstanceLimitWidgetContract(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		bool& bOutModified)
	{
		bOutModified = false;
		UWidget* ExistingWidget = Tree.FindWidget(TEXT("Text_InstanceLimit"));
		if (ExistingWidget)
		{
			UCommonTextBlock* ExistingText = Cast<UCommonTextBlock>(ExistingWidget);
			if (!ExistingText)
			{
				return false;
			}
			EnsureVariableContract(
				Blueprint, *ExistingText, TEXT("Text_InstanceLimit"), bOutModified);
			return HasInstanceLimitWidgetContract(Blueprint);
		}

		// Refuse an incompatible partial designer row rather than duplicating or replacing it.
		if (Tree.FindWidget(TEXT("InstanceLimit_Row")) ||
			Tree.FindWidget(TEXT("InstanceLimit_Row_Surface")))
		{
			return false;
		}

		UVerticalBox* BuildContent = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Build_Content")));
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		UCommonTextBlock* InstanceLimit = nullptr;
		if (!BuildContent || !Palette ||
			!AddInfoRow(
				Blueprint,
				Tree,
				*BuildContent,
				*Palette,
				TEXT("InstanceLimit_Row"),
				FText::FromString(TEXT("건축 한도")),
				TEXT("Text_InstanceLimit"),
				InstanceLimit))
		{
			return false;
		}

		bOutModified = true;
		return HasInstanceLimitWidgetContract(Blueprint);
	}

	[[maybe_unused]] bool BuildLegacyLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			return false;
		}
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
		if (!AddInfoRow(Blueprint, Tree, *BuildSection, *Palette, TEXT("Footprint_Row"),
			FText::FromString(TEXT("시설 크기")), TEXT("Text_Footprint"), Footprint) ||
			!AddInfoRow(Blueprint, Tree, *BuildSection, *Palette, TEXT("InstallCost_Row"),
				FText::FromString(TEXT("설치 비용")), TEXT("Text_InstallCost"), InstallCost) ||
			!AddInfoRow(Blueprint, Tree, *BuildSection, *Palette, TEXT("RecoveryCost_Row"),
				FText::FromString(TEXT("철거 회수액")), TEXT("Text_RecoveryCost"), RecoveryCost) ||
			!AddInfoRow(Blueprint, Tree, *BuildSection, *Palette, TEXT("QueueCapacity_Row"),
				FText::FromString(TEXT("이용 규모")), TEXT("Text_QueueCapacity"), QueueCapacity) ||
			!AddInfoRow(Blueprint, Tree, *BuildSection, *Palette, TEXT("ServiceDuration_Row"),
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
		if (!AddInfoRow(Blueprint, Tree, *LiveSection, *Palette, TEXT("WaitingParties_Row"),
			FText::FromString(TEXT("대기/처리 중 일행")), TEXT("Text_WaitingParties"), WaitingParties) ||
			!AddInfoRow(Blueprint, Tree, *LiveSection, *Palette, TEXT("Occupancy_Row"),
				FText::FromString(TEXT("점유 팀")), TEXT("Text_Occupancy"), Occupancy) ||
			!AddInfoRow(Blueprint, Tree, *LiveSection, *Palette, TEXT("RemainingService_Row"),
				FText::FromString(TEXT("남은 서비스 팀")), TEXT("Text_RemainingService"), RemainingService) ||
			!AddInfoRow(Blueprint, Tree, *LiveSection, *Palette, TEXT("ExpectedWait_Row"),
				FText::FromString(TEXT("1인 기준 예상 대기")), TEXT("Text_ExpectedWait"), ExpectedWait) ||
			!AddInfoRow(Blueprint, Tree, *LiveSection, *Palette, TEXT("Throughput_Row"),
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

		bool bProgramContractAdded = false;
		if (!EnsureProgramWidgetContract(Blueprint, Tree, bProgramContractAdded))
		{
			return false;
		}
		(void)bProgramContractAdded;

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

	USizeBox* BuildLightHeader(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const UFF_UIPalette& Palette)
	{
		USizeBox* HeaderSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Header_Size"));
		UBorder* HeaderSurface = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Header_Surface"));
		UHorizontalBox* HeaderRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Header_Row"));
		UVerticalBox* Identity = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Header_Identity"));
		UCommonTextBlock* Context = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_Context"), true);
		UCommonTextBlock* FacilityName = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_FacilityName"), true);
		USizeBox* CloseSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Close_Size"));
		UButton* Close = MakeLightTextButton(
			Blueprint, Tree, TEXT("Button_Close"), TEXT("Close_Label"),
			FText::FromString(TEXT("\u00D7")),
			FLinearColor::Transparent, Palette.InGame.Teal, Palette.InGame.Teal,
			FLinearColor::Transparent, Palette.InGame.White, true);
		if (!HeaderSize || !HeaderSurface || !HeaderRow || !Identity || !Context ||
			!FacilityName || !CloseSize || !Close)
		{
			return nullptr;
		}

		HeaderSize->SetHeightOverride(60.0f);
		PaintBorder(*HeaderSurface, MakeSolidBrush(Palette.InGame.TealDeep, 7.0f));
		HeaderSurface->SetPadding(FMargin(16.0f, 10.0f, 10.0f, 10.0f));
		FLinearColor ContextColor = Palette.InGame.White;
		ContextColor.A = 0.78f;
		Context->SetText(FText::FromString(TEXT("배치 시설")));
		StyleLightText(*Context, 10, ContextColor, TEXT("Medium"));
		FacilityName->SetText(FText::FromString(TEXT("시설을 선택하세요")));
		FacilityName->SetAutoWrapText(false);
		FacilityName->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		StyleLightText(*FacilityName, 19, Palette.InGame.White, TEXT("SemiBold"));
		AddVertical(*Identity, *Context, FMargin(0.0f, 0.0f, 0.0f, 1.0f));
		AddVertical(*Identity, *FacilityName);
		AddHorizontal(*HeaderRow, *Identity, FMargin(0.0f), ESlateSizeRule::Fill);
		CloseSize->SetWidthOverride(36.0f);
		CloseSize->SetHeightOverride(36.0f);
		CloseSize->AddChild(Close);
		AddHorizontal(*HeaderRow, *CloseSize);
		HeaderSurface->AddChild(HeaderRow);
		HeaderSize->AddChild(HeaderSurface);
		return HeaderSize;
	}

	UBorder* BuildIdentitySummary(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const UFF_UIPalette& Palette)
	{
		UBorder* Card = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Summary_Card"));
		UHorizontalBox* Row = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Summary_Row"));
		USizeBox* ThumbnailSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Thumbnail_Size"));
		UBorder* ThumbnailBackground = MakeWidget<UBorder>(
			Blueprint, Tree, TEXT("Thumbnail_Background"));
		UOverlay* ThumbnailOverlay = MakeWidget<UOverlay>(
			Blueprint, Tree, TEXT("Thumbnail_Overlay"));
		UImage* Thumbnail = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Thumbnail"), true);
		UScaleBox* ThumbnailFit = MakeWidget<UScaleBox>(Blueprint, Tree, TEXT("Thumbnail_Fit"));
		UCommonTextBlock* ThumbnailPlaceholder = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_ThumbnailPlaceholder"), true);
		UVerticalBox* Meta = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Summary_Meta"));
		UHorizontalBox* MetaRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Meta_Row"));
		UBorder* CategoryChip = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Category_Chip"));
		UCommonTextBlock* Category = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_Category"), true);
		UBorder* ZoneChip = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Zone_Chip"));
		UCommonTextBlock* Zone = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_Zone"), true);
		UBorder* StateChip = MakeWidget<UBorder>(Blueprint, Tree, TEXT("State_Chip"));
		UCommonTextBlock* FacilityState = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_FacilityState"), true);
		UCommonTextBlock* Revenue = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_FacilityRevenue"), true);
		if (!Card || !Row || !ThumbnailSize || !ThumbnailBackground || !ThumbnailOverlay ||
			!Thumbnail || !ThumbnailFit || !ThumbnailPlaceholder || !Meta || !MetaRow || !CategoryChip ||
			!Category || !ZoneChip || !Zone || !StateChip || !FacilityState || !Revenue)
		{
			return nullptr;
		}

		PaintBorder(*Card, MakeSolidBrush(
			Palette.InGame.Paper, 10.0f, Palette.InGame.Line, 1.0f));
		Card->SetPadding(FMargin(10.0f));
		ThumbnailSize->SetWidthOverride(120.0f);
		ThumbnailSize->SetHeightOverride(112.0f);
		PaintBorder(*ThumbnailBackground, MakeSolidBrush(Palette.InGame.TealSoft, 8.0f));
		ThumbnailBackground->SetPadding(FMargin(7.0f));
		ThumbnailPlaceholder->SetText(FText::FromString(TEXT("시설 이미지 준비 중")));
		ThumbnailPlaceholder->SetAutoWrapText(true);
		StyleLightText(
			*ThumbnailPlaceholder, 11, Palette.InGame.InkMuted, TEXT("Medium"),
			ETextJustify::Center);
		ThumbnailFit->SetStretch(EStretch::ScaleToFit);
		ThumbnailFit->AddChild(Thumbnail);
		if (UOverlaySlot* Slot = ThumbnailOverlay->AddChildToOverlay(ThumbnailFit))
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
		AddHorizontal(*Row, *ThumbnailSize, FMargin(0.0f, 0.0f, 10.0f, 0.0f));

		PaintBorder(*CategoryChip, MakeSolidBrush(Palette.InGame.TealSoft, 4.0f));
		CategoryChip->SetPadding(FMargin(7.0f, 3.0f));
		Category->SetText(FText::FromString(TEXT("부스")));
		Category->SetAutoWrapText(true);
		StyleLightText(*Category, 11, Palette.InGame.TealDeep, TEXT("SemiBold"));
		CategoryChip->AddChild(Category);
		PaintBorder(*ZoneChip, MakeSolidBrush(FLinearColor::Transparent, 0.0f));
		ZoneChip->SetPadding(FMargin(1.0f, 2.0f));
		Zone->SetText(FText::FromString(TEXT("구역 정보")));
		Zone->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		StyleLightText(*Zone, 11, Palette.InGame.InkMuted, TEXT("Medium"));
		ZoneChip->AddChild(Zone);
		AddHorizontal(*MetaRow, *CategoryChip, FMargin(0.0f), ESlateSizeRule::Fill);
		AddVertical(*Meta, *MetaRow, FMargin(0.0f, 0.0f, 0.0f, 2.0f));
		AddVertical(*Meta, *ZoneChip, FMargin(0.0f, 0.0f, 0.0f, 4.0f));

		PaintBorder(*StateChip, MakeSolidBrush(
			Palette.InGame.Paper, 5.0f, Palette.InGame.LineStrong, 1.0f));
		StateChip->SetPadding(FMargin(7.0f, 4.0f));
		FacilityState->SetText(FText::FromString(TEXT("배치 완료")));
		StyleLightText(*FacilityState, 11, Palette.InGame.TealDeep, TEXT("SemiBold"));
		FacilityState->SetAutoWrapText(true);
		StateChip->AddChild(FacilityState);
		AddVertical(*Meta, *StateChip, FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		StyleLightText(*Revenue, 10, Palette.InGame.InkMuted, TEXT("Medium"));
		Revenue->SetAutoWrapText(true);
		AddVertical(*Meta, *Revenue);
		AddHorizontal(*Row, *Meta, FMargin(0.0f), ESlateSizeRule::Fill);
		Card->AddChild(Row);
		return Card;
	}

	UBorder* BuildStatCard(
		UWidgetBlueprint& Blueprint, UWidgetTree& Tree, const UFF_UIPalette& Palette,
		const FName ValueName, const TCHAR* LabelText)
	{
		const FString Prefix = ValueName.ToString();
		UBorder* Card = MakeWidget<UBorder>(Blueprint, Tree, FName(*(Prefix + TEXT("_Card"))));
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, FName(*(Prefix + TEXT("_Content"))));
		UCommonTextBlock* Label = MakeWidget<UCommonTextBlock>(Blueprint, Tree, FName(*(Prefix + TEXT("_Label"))));
		UCommonTextBlock* Value = MakeWidget<UCommonTextBlock>(Blueprint, Tree, ValueName, true);
		if (!Card || !Content || !Label || !Value)
		{
			return nullptr;
		}
		PaintBorder(*Card, MakeSolidBrush(Palette.InGame.PaperSunken, 7.0f));
		Card->SetPadding(FMargin(10.0f, 8.0f));
		Label->SetText(FText::FromString(LabelText));
		StyleLightText(*Label, 10, Palette.InGame.InkMuted, TEXT("Medium"));
		Value->SetText(FText::FromString(TEXT("—")));
		Value->SetAutoWrapText(true);
		StyleLightText(*Value, 15, Palette.InGame.TealDeep, TEXT("SemiBold"));
		AddVertical(*Content, *Label, FMargin(0.0f, 0.0f, 0.0f, 3.0f));
		AddVertical(*Content, *Value);
		Card->AddChild(Content);
		return Card;
	}

	bool AddStatPair(
		UWidgetBlueprint& Blueprint, UWidgetTree& Tree, UVerticalBox& Parent,
		const UFF_UIPalette& Palette, const FName LeftName, const TCHAR* LeftLabel,
		const FName RightName = NAME_None, const TCHAR* RightLabel = nullptr)
	{
		UHorizontalBox* Row = MakeWidget<UHorizontalBox>(Blueprint, Tree,
			FName(*(LeftName.ToString() + TEXT("_StatRow"))));
		UBorder* Left = BuildStatCard(Blueprint, Tree, Palette, LeftName, LeftLabel);
		if (!Row || !Left)
		{
			return false;
		}
		UHorizontalBoxSlot* LeftSlot = AddHorizontal(*Row, *Left,
			FMargin(0.0f, 0.0f, RightLabel ? 3.0f : 0.0f, 0.0f), ESlateSizeRule::Fill);
		if (LeftSlot) LeftSlot->SetVerticalAlignment(VAlign_Fill);
		if (RightLabel)
		{
			UBorder* Right = BuildStatCard(Blueprint, Tree, Palette, RightName, RightLabel);
			if (!Right) return false;
			UHorizontalBoxSlot* RightSlot = AddHorizontal(*Row, *Right,
				FMargin(3.0f, 0.0f, 0.0f, 0.0f), ESlateSizeRule::Fill);
			if (RightSlot) RightSlot->SetVerticalAlignment(VAlign_Fill);
		}
		AddVertical(Parent, *Row, FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		return true;
	}

	UVerticalBox* BuildUpgradeSection(
		UWidgetBlueprint& Blueprint, UWidgetTree& Tree, const UFF_UIPalette& Palette)
	{
		UVerticalBox* Section = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_UpgradeSection"), true);
		UBorder* Card = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Upgrade_Card"));
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Upgrade_Content"));
		UCommonTextBlock* Level = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_FacilityLevel"), true);
		UHorizontalBox* Comparison = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Panel_UpgradeComparison"), true);
		UCommonTextBlock* Effects = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_UpgradeEffect"), true);
		UCommonTextBlock* Cost = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_UpgradeCost"), true);
		UCommonTextBlock* Reason = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_UpgradeReason"), true);
		UButton* Upgrade = MakeLightTextButton(Blueprint, Tree, TEXT("Button_Upgrade"), TEXT("Text_Upgrade"),
			FText::FromString(TEXT("시설 강화")), Palette.InGame.TealDeep, Palette.InGame.Teal,
			Palette.InGame.TealDeep, Palette.InGame.LineStrong, Palette.InGame.White, true, true);
		if (!Section || !Card || !Content || !Level || !Comparison || !Effects || !Cost || !Reason || !Upgrade)
		{
			return nullptr;
		}
		PaintBorder(*Card, MakeSolidBrush(Palette.InGame.Paper, 9.0f, Palette.InGame.Amber, 1.0f));
		Card->SetPadding(FMargin(10.0f, 8.0f));
		StyleLightText(*Level, 12, Palette.InGame.TealDeep, TEXT("SemiBold"));
		Level->SetText(FText::FromString(TEXT("시설 강화 · Lv.1")));
		AddVertical(*Content, *Level, FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		for (const bool bNext : {false, true})
		{
			UBorder* Surface = MakeWidget<UBorder>(Blueprint, Tree, bNext ? TEXT("Upgrade_NextCard") : TEXT("Upgrade_CurrentCard"));
			UVerticalBox* Column = MakeWidget<UVerticalBox>(Blueprint, Tree, bNext ? TEXT("Upgrade_NextColumn") : TEXT("Upgrade_CurrentColumn"));
			UCommonTextBlock* Caption = MakeWidget<UCommonTextBlock>(Blueprint, Tree,
				bNext ? TEXT("Upgrade_NextCaption") : TEXT("Upgrade_CurrentCaption"));
			UCommonTextBlock* Values = MakeWidget<UCommonTextBlock>(Blueprint, Tree,
				bNext ? TEXT("Text_UpgradeNext") : TEXT("Text_UpgradeCurrent"), true);
			if (!Surface || !Column || !Caption || !Values) return nullptr;
			PaintBorder(*Surface, MakeSolidBrush(bNext ? Palette.InGame.TealSoft : Palette.InGame.PaperSunken, 6.0f));
			Surface->SetPadding(FMargin(8.0f, 5.0f));
			Caption->SetText(FText::FromString(bNext ? TEXT("→ 강화 후") : TEXT("현재")));
			StyleLightText(*Caption, 10, Palette.InGame.InkMuted, TEXT("SemiBold"));
			Values->SetAutoWrapText(true);
			StyleLightText(*Values, 11, Palette.InGame.TealDeep, TEXT("Medium"));
			AddVertical(*Column, *Caption, FMargin(0.0f, 0.0f, 0.0f, 2.0f));
			AddVertical(*Column, *Values);
			Surface->AddChild(Column);
			UHorizontalBoxSlot* Slot = AddHorizontal(*Comparison, *Surface,
				bNext ? FMargin(3.0f, 0.0f, 0.0f, 0.0f) : FMargin(0.0f, 0.0f, 3.0f, 0.0f), ESlateSizeRule::Fill);
			if (Slot) Slot->SetVerticalAlignment(VAlign_Fill);
		}
		AddVertical(*Content, *Comparison, FMargin(0.0f, 0.0f, 0.0f, 5.0f));
		for (UCommonTextBlock* Text : {Effects, Cost, Reason})
		{
			Text->SetAutoWrapText(true);
			StyleLightText(*Text, 10, Text == Reason ? Palette.InGame.Danger : Palette.InGame.InkMuted, TEXT("Medium"));
			Text->SetVisibility(ESlateVisibility::Collapsed);
			AddVertical(*Content, *Text, FMargin(0.0f, 0.0f, 0.0f, 5.0f));
		}
		AddVertical(*Content, *Upgrade);
		Card->AddChild(Content);
		AddVertical(*Section, *Card);
		Section->SetVisibility(ESlateVisibility::Collapsed);
		return Section;
	}

	const TArray<FName>& GetRevenueContractVariableNames()
	{
		static const TArray<FName> Names = {
			TEXT("Panel_RevenueSection"), TEXT("Text_FacilityTodayRevenue"), TEXT("Text_FacilityTotalRevenue"),
			TEXT("Text_FacilityRevenueCount"), TEXT("Text_FacilityRevenueShare"), TEXT("Text_FacilityRevenueStatus")
		};
		return Names;
	}

	bool HasRevenueWidgetContract(const UWidgetBlueprint& Blueprint)
	{
		for (const FName Name : GetRevenueContractVariableNames())
		{
			// Designers may re-parent the section into any panel; only the text nodes are typed.
			const UClass& ExpectedClass = Name == TEXT("Panel_RevenueSection")
				? *UPanelWidget::StaticClass()
				: *UCommonTextBlock::StaticClass();
			if (!HasNamedVariable(Blueprint, Name, ExpectedClass))
			{
				return false;
			}
		}
		return true;
	}

	/** 시설 수익 카드. 오늘/누적 매출과 결제 건수·비중을 stat 카드로, 안내 문구를 한 줄로 둔다. */
	UVerticalBox* BuildRevenueSection(
		UWidgetBlueprint& Blueprint, UWidgetTree& Tree, const UFF_UIPalette& Palette)
	{
		UVerticalBox* Section = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_RevenueSection"), true);
		UBorder* Card = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Revenue_Card"));
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Revenue_Content"));
		UCommonTextBlock* Title = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Revenue_Title"));
		UCommonTextBlock* Status = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_FacilityRevenueStatus"), true);
		if (!Section || !Card || !Content || !Title || !Status)
		{
			return nullptr;
		}
		PaintBorder(*Card, MakeSolidBrush(Palette.InGame.Paper, 6.0f, Palette.InGame.Line, 1.0f));
		Card->SetPadding(FMargin(10.0f));
		Title->SetText(FText::FromString(TEXT("시설 수익")));
		StyleLightText(*Title, 14, Palette.InGame.TealDeep, TEXT("SemiBold"));
		AddVertical(*Content, *Title, FMargin(2.0f, 0.0f, 0.0f, 7.0f));
		if (!AddStatPair(Blueprint, Tree, *Content, Palette,
				TEXT("Text_FacilityTodayRevenue"), TEXT("오늘 매출"),
				TEXT("Text_FacilityTotalRevenue"), TEXT("누적 매출")) ||
			!AddStatPair(Blueprint, Tree, *Content, Palette,
				TEXT("Text_FacilityRevenueCount"), TEXT("결제 건수"),
				TEXT("Text_FacilityRevenueShare"), TEXT("오늘 매출 비중")))
		{
			return nullptr;
		}
		Status->SetText(FText::FromString(TEXT("아직 결제가 없습니다. 손님이 이용을 마치면 수수료가 들어옵니다.")));
		Status->SetAutoWrapText(true);
		StyleLightText(*Status, 11, Palette.InGame.InkMuted, TEXT("Medium"));
		AddVertical(*Content, *Status, FMargin(2.0f, 0.0f, 2.0f, 0.0f));
		Card->AddChild(Content);
		AddVertical(*Section, *Card);
		return Section;
	}

	/** Adds only the missing revenue card right after the upgrade block; designer nodes and slots stay intact. */
	bool EnsureRevenueWidgetContract(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, bool& bOutModified)
	{
		bOutModified = false;
		if (HasRevenueWidgetContract(Blueprint))
		{
			return true;
		}
		// Refuse a partially authored designer card rather than duplicating or replacing it.
		for (const FName Name : GetRevenueContractVariableNames())
		{
			if (Tree.FindWidget(Name))
			{
				return false;
			}
		}
		if (Tree.FindWidget(TEXT("Revenue_Card")))
		{
			return false;
		}
		UVerticalBox* Content = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Panel_Content")));
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Content || !Palette)
		{
			return false;
		}
		Blueprint.Modify();
		Content->Modify();
		UVerticalBox* Section = BuildRevenueSection(Blueprint, Tree, *Palette);
		if (!Section || !AddVertical(*Content, *Section, FMargin(0.0f, 0.0f, 0.0f, 8.0f)))
		{
			return false;
		}
		// Sit right after the upgrade block (or the identity summary) so both the overview and the
		// operation page open with earnings directly under the facility header.
		UWidget* Anchor = FFFacilityInfoUpgradeLayout::FindContentChild(
			Tree.FindWidget(TEXT("Panel_UpgradeSection")), *Content);
		if (!Anchor)
		{
			Anchor = FFFacilityInfoUpgradeLayout::FindContentChild(Tree.FindWidget(TEXT("Summary_Card")), *Content);
		}
		if (!Anchor)
		{
			Anchor = FFFacilityInfoUpgradeLayout::FindContentChild(Tree.FindWidget(TEXT("Image_Thumbnail")), *Content);
		}
		if (Anchor && Anchor != Section)
		{
			Content->ShiftChild(Content->GetChildIndex(Anchor) + 1, Section);
		}
		bOutModified = true;
		return HasRevenueWidgetContract(Blueprint);
	}

	UBorder* BuildInformationSection(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const UFF_UIPalette& Palette,
		const bool bLive)
	{
		const FName PanelName = bLive ? TEXT("Panel_LiveSection") : TEXT("Panel_BuildSection");
		const FName ContentName = bLive ? TEXT("Live_Content") : TEXT("Build_Content");
		const FName TitleName = bLive ? TEXT("Live_Title") : TEXT("Build_Title");
		UBorder* Section = MakeWidget<UBorder>(Blueprint, Tree, PanelName, true);
		UVerticalBox* SectionContent = MakeWidget<UVerticalBox>(Blueprint, Tree, ContentName);
		UCommonTextBlock* Title = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TitleName);
		if (!Section || !SectionContent || !Title)
		{
			return nullptr;
		}

		PaintBorder(*Section, MakeSolidBrush(
			Palette.InGame.Paper, 6.0f, Palette.InGame.Line, 1.0f));
		Section->SetPadding(FMargin(10.0f));
		Title->SetText(FText::FromString(bLive ? TEXT("실시간 운영") : TEXT("배치 정보")));
		StyleLightText(*Title, 14, Palette.InGame.TealDeep, TEXT("SemiBold"));
		AddVertical(*SectionContent, *Title, FMargin(2.0f, 0.0f, 0.0f, bLive ? 5.0f : 8.0f));

		if (bLive)
		{
			UCommonTextBlock* OperationState = MakeWidget<UCommonTextBlock>(
				Blueprint, Tree, TEXT("Text_OperationState"), true);
			if (!OperationState)
			{
				return nullptr;
			}
			OperationState->SetText(FText::GetEmpty());
			StyleLightText(*OperationState, 13, Palette.InGame.TealDeep, TEXT("SemiBold"));
			AddVertical(*SectionContent, *OperationState, FMargin(2.0f, 0.0f, 0.0f, 7.0f));
			if (!AddStatPair(Blueprint, Tree, *SectionContent, Palette,
				TEXT("Text_WaitingParties"), TEXT("대기 / 처리 중 일행"), TEXT("Text_Occupancy"), TEXT("점유 팀")) ||
				!AddStatPair(Blueprint, Tree, *SectionContent, Palette,
				TEXT("Text_RemainingService"), TEXT("남은 서비스 팀"), TEXT("Text_ExpectedWait"), TEXT("한 팀 예상 대기")) ||
				!AddStatPair(Blueprint, Tree, *SectionContent, Palette, TEXT("Text_Throughput"), TEXT("처리 속도")))
			{
				return nullptr;
			}
			Section->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			if (!AddStatPair(Blueprint, Tree, *SectionContent, Palette,
				TEXT("Text_Footprint"), TEXT("시설 크기")) ||
				!AddStatPair(Blueprint, Tree, *SectionContent, Palette,
				TEXT("Text_InstallCost"), TEXT("설치 비용"), TEXT("Text_RecoveryCost"), TEXT("철거 회수액")) ||
				!AddStatPair(Blueprint, Tree, *SectionContent, Palette,
				TEXT("Text_QueueCapacity"), TEXT("이용 규모"), TEXT("Text_ServiceDuration"), TEXT("서비스 시간")) ||
				!AddStatPair(Blueprint, Tree, *SectionContent, Palette, TEXT("Text_InstanceLimit"), TEXT("건축 한도")))
			{
				return nullptr;
			}
		}

		Section->AddChild(SectionContent);
		return Section;
	}

	UVerticalBox* BuildProgramSection(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const UFF_UIPalette& Palette)
	{
		UVerticalBox* Section = MakeWidget<UVerticalBox>(
			Blueprint, Tree, TEXT("Panel_ProgramSection"), true);
		UBorder* Card = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Program_Card"));
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Program_Content"));
		UCommonTextBlock* Title = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Program_Title"));
		UCommonTextBlock* Status = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_ProgramStatus"), true);
		UCommonTextBlock* Summary = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_ScheduledProgramSummary"), true);
		UCommonTextBlock* Reason = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_ProgramScheduleReason"), true);
		UButton* Schedule = MakeLightTextButton(
			Blueprint, Tree, TEXT("Button_ScheduleProgram"), TEXT("Text_ScheduleProgram"),
			FText::FromString(TEXT("유료 프로그램 편성")),
			Palette.InGame.Teal, Palette.InGame.TealDeep, Palette.InGame.TealDeep,
			Palette.InGame.LineStrong, Palette.InGame.White, true, true);
		if (!Section || !Card || !Content || !Title || !Status || !Summary || !Reason || !Schedule)
		{
			return nullptr;
		}

		PaintBorder(*Card, MakeSolidBrush(
			Palette.InGame.Paper, 6.0f, Palette.InGame.Line, 1.0f));
		Card->SetPadding(FMargin(10.0f));
		Title->SetText(FText::FromString(TEXT("유료 프로그램")));
		StyleLightText(*Title, 14, Palette.InGame.TealDeep, TEXT("SemiBold"));
		Status->SetText(FText::FromString(TEXT("오늘 편성 없음")));
		StyleLightText(*Status, 13, Palette.InGame.Ink, TEXT("SemiBold"));
		Summary->SetText(FText::GetEmpty());
		Summary->SetAutoWrapText(true);
		StyleLightText(*Summary, 12, Palette.InGame.InkMuted, TEXT("Medium"));
		Reason->SetText(FText::GetEmpty());
		Reason->SetAutoWrapText(true);
		StyleLightText(*Reason, 11, Palette.InGame.Amber, TEXT("Medium"));
		AddVertical(*Content, *Title, FMargin(2.0f, 0.0f, 0.0f, 7.0f));
		AddVertical(*Content, *Status, FMargin(2.0f, 0.0f, 0.0f, 3.0f));
		AddVertical(*Content, *Summary, FMargin(2.0f, 0.0f, 0.0f, 3.0f));
		AddVertical(*Content, *Reason, FMargin(2.0f, 0.0f, 0.0f, 7.0f));
		AddVertical(*Content, *Schedule);
		Card->AddChild(Content);
		AddVertical(*Section, *Card);
		Section->SetVisibility(ESlateVisibility::Collapsed);
		return Section;
	}

	UBorder* BuildActionRail(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const UFF_UIPalette& Palette)
	{
		UBorder* Actions = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Panel_Actions"), true);
		UOverlay* Switcher = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Actions_Switcher"));
		UHorizontalBox* BuildActions = MakeWidget<UHorizontalBox>(
			Blueprint, Tree, TEXT("Panel_BuildActions"), true);
		UButton* Move = MakeLightTextButton(
			Blueprint, Tree, TEXT("Button_Move"), TEXT("Move_Label"),
			FText::FromString(TEXT("이동  M")),
			Palette.InGame.TealDeep, Palette.InGame.Teal, Palette.InGame.TealDeep,
			Palette.InGame.LineStrong, Palette.InGame.White, true);
		UButton* Remove = MakeLightTextButton(
			Blueprint, Tree, TEXT("Button_Remove"), TEXT("Remove_Label"),
			FText::FromString(TEXT("철거  Enter")),
			Palette.InGame.Paper, Palette.InGame.PaperSunken, Palette.InGame.PaperSunken,
			Palette.InGame.LineStrong, Palette.InGame.Danger, true, false, Palette.InGame.Line);
		if (!Actions || !Switcher || !BuildActions || !Move || !Remove)
		{
			return nullptr;
		}

		PaintBorder(*Actions, MakeSolidBrush(Palette.InGame.PaperSunken, 0.0f));
		Actions->SetPadding(FMargin(12.0f, 9.0f));
		AddHorizontal(*BuildActions, *Move, FMargin(0.0f, 0.0f, 5.0f, 0.0f), ESlateSizeRule::Fill);
		AddHorizontal(*BuildActions, *Remove, FMargin(5.0f, 0.0f, 0.0f, 0.0f), ESlateSizeRule::Fill);
		if (UOverlaySlot* Slot = Switcher->AddChildToOverlay(BuildActions))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		Actions->AddChild(Switcher);
		return Actions;
	}

	bool HasLightingControlsContract(const UWidgetBlueprint& Blueprint)
	{
		for (const TCHAR* Name : { TEXT("Button_LightingToggle"), TEXT("Button_LightingOriginal"),
			TEXT("Button_LightingTeal"), TEXT("Button_LightingSunset"), TEXT("Button_LightingViolet"), TEXT("Button_LightingReset") })
		{
			if (!HasNamedVariable(Blueprint, Name, *UButton::StaticClass())) return false;
		}
		for (const TCHAR* Name : { TEXT("Text_LightingToggle"), TEXT("Text_LightingBrightness"), TEXT("Text_LightingColorStatus") })
		{
			if (!HasNamedVariable(Blueprint, Name, *UCommonTextBlock::StaticClass())) return false;
		}
		return HasNamedVariable(Blueprint, TEXT("Slider_LightingBrightness"), *USlider::StaticClass()) &&
			HasNamedVariable(Blueprint, TEXT("Input_LightingColor"), *UEditableTextBox::StaticClass());
	}

	bool BuildLightingControls(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette) return false;
		const FFF_UIPaletteInGame& P = Palette->InGame;
		UBorder* Card = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Lighting_Card"));
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Lighting_Content"));
		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Lighting_Header"));
		UCommonTextBlock* Title = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_LightingTitle"));
		UButton* Toggle = MakeLightTextButton(Blueprint, Tree, TEXT("Button_LightingToggle"), TEXT("Text_LightingToggle"),
			FText::FromString(TEXT("켜짐 · 끄기")), P.Teal, P.TealDeep, P.TealDeep, P.LineStrong, P.White, true, true);
		UHorizontalBox* BrightnessRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Lighting_BrightnessRow"));
		UCommonTextBlock* BrightnessLabel = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_LightingBrightnessLabel"));
		UCommonTextBlock* BrightnessValue = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_LightingBrightness"), true);
		USlider* Slider = MakeWidget<USlider>(Blueprint, Tree, TEXT("Slider_LightingBrightness"), true);
		UHorizontalBox* Colors = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Lighting_ColorPresets"));
		UCommonTextBlock* ColorStatus = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_LightingColorStatus"), true);
		UEditableTextBox* ColorInput = MakeWidget<UEditableTextBox>(Blueprint, Tree, TEXT("Input_LightingColor"), true);
		UButton* Reset = MakeLightTextButton(Blueprint, Tree, TEXT("Button_LightingReset"), TEXT("Text_LightingReset"),
			FText::FromString(TEXT("기본 설정으로 되돌리기")), P.PaperSunken, P.Hover, P.TealSoft, P.Line, P.TealDeep, true);
		if (!Card || !Content || !Header || !Title || !Toggle || !BrightnessRow || !BrightnessLabel ||
			!BrightnessValue || !Slider || !Colors || !ColorStatus || !ColorInput || !Reset) return false;
		PaintBorder(*Card, MakeSolidBrush(P.Paper, 8.0f, P.Line, 1.0f));
		Card->SetPadding(FMargin(10.0f, 8.0f));
		Title->SetText(FText::FromString(TEXT("조명 설정")));
		StyleLightText(*Title, 14, P.TealDeep, TEXT("SemiBold"));
		AddHorizontal(*Header, *Title, FMargin(0), ESlateSizeRule::Fill);
		AddHorizontal(*Header, *Toggle);
		AddVertical(*Content, *Header, FMargin(0, 0, 0, 8));
		BrightnessLabel->SetText(FText::FromString(TEXT("전체 밝기")));
		StyleLightText(*BrightnessLabel, 11, P.Ink, TEXT("Medium"));
		BrightnessValue->SetText(FText::FromString(TEXT("100%")));
		StyleLightText(*BrightnessValue, 11, P.TealDeep, TEXT("SemiBold"));
		AddHorizontal(*BrightnessRow, *BrightnessLabel, FMargin(0), ESlateSizeRule::Fill);
		AddHorizontal(*BrightnessRow, *BrightnessValue);
		AddVertical(*Content, *BrightnessRow);
		Slider->SetMinValue(0.0f);
		Slider->SetMaxValue(5.0f);
		Slider->SetStepSize(0.05f);
		Slider->SetValue(1.0f);
		Slider->SetSliderBarColor(P.TealSoft);
		Slider->SetSliderHandleColor(P.TealDeep);
		Slider->SetToolTipText(FText::FromString(TEXT("0~500%. 100%는 Blueprint에 설정한 밝기입니다.")));
		AddVertical(*Content, *Slider, FMargin(0, 5, 0, 9));
		const TCHAR* ButtonNames[] = {TEXT("Button_LightingOriginal"), TEXT("Button_LightingTeal"), TEXT("Button_LightingSunset"), TEXT("Button_LightingViolet")};
		const TCHAR* Labels[] = {TEXT("원래 색"), TEXT("청록"), TEXT("노을"), TEXT("보라")};
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const FName LabelName(*FString::Printf(TEXT("Text_LightingPreset%d"), Index));
			UButton* Button = MakeLightTextButton(Blueprint, Tree, ButtonNames[Index], LabelName,
				FText::FromString(Labels[Index]), P.PaperSunken, P.Hover, P.TealSoft, P.Line, P.TealDeep, true, false, P.Line);
			if (!Button) return false;
			FButtonStyle Style = Button->GetStyle();
			Style.NormalPadding = FMargin(3.0f, 6.0f);
			Style.PressedPadding = Style.NormalPadding;
			Button->SetStyle(Style);
			AddHorizontal(*Colors, *Button, FMargin(0, 0, Index < 3 ? 4.0f : 0.0f, 0), ESlateSizeRule::Fill);
		}
		AddVertical(*Content, *Colors, FMargin(0, 0, 0, 5));
		ColorStatus->SetText(FText::FromString(TEXT("Blueprint의 개별 조명 색상 사용 중")));
		ColorStatus->SetAutoWrapText(true);
		StyleLightText(*ColorStatus, 10, P.InkMuted, TEXT("Medium"));
		AddVertical(*Content, *ColorStatus, FMargin(0, 0, 0, 5));
		FEditableTextBoxStyle InputStyle = ColorInput->GetWidgetStyle();
		InputStyle.BackgroundImageNormal = MakeSolidBrush(P.PaperSunken, 4, P.Line, 1);
		InputStyle.BackgroundImageHovered = MakeSolidBrush(P.Hover, 4, P.TealSoft, 1);
		InputStyle.BackgroundImageFocused = MakeSolidBrush(P.Paper, 4, P.TealDeep, 1);
		InputStyle.ForegroundColor = FSlateColor(P.Ink);
		InputStyle.TextStyle.SetFont(BrightnessLabel->GetFont());
		InputStyle.Padding = FMargin(8, 6);
		ColorInput->SetWidgetStyle(InputStyle);
		ColorInput->SetHintText(FText::FromString(TEXT("직접 입력: #56DCE7 · Enter로 적용")));
		ColorInput->SetToolTipText(FText::FromString(TEXT("6자리 색상 코드로 원하는 색을 적용합니다. ‘원래 색’은 개별 조명의 색을 복원합니다.")));
		ColorInput->SetClearKeyboardFocusOnCommit(true);
		AddVertical(*Content, *ColorInput, FMargin(0, 0, 0, 7));
		AddVertical(*Content, *Reset);
		Card->AddChild(Content);
		Tree.RootWidget = Card;
		return HasLightingControlsContract(Blueprint);
	}

	UWidgetBlueprint* EnsureLightingControlsBlueprint()
	{
		const FString Name = FPackageName::GetLongPackageAssetName(LightingPackage);
		if (FPackageName::DoesPackageExist(LightingPackage))
		{
			UWidgetBlueprint* Existing = LoadObject<UWidgetBlueprint>(nullptr, *(LightingPackage + TEXT(".") + Name));
			if (Existing && Existing->GeneratedClass && Existing->GeneratedClass->IsChildOf(UFF_FacilityLightingControls::StaticClass()) &&
				HasLightingControlsContract(*Existing))
			{
				// Generalize the original stage-only caption while retaining any designer custom title.
				if (UCommonTextBlock* Title = Cast<UCommonTextBlock>(Existing->WidgetTree->FindWidget(TEXT("Text_LightingTitle"))))
				{
					if (Title->GetText().ToString() == TEXT("무대 조명"))
					{
						Existing->Modify();
						Title->Modify();
						Title->SetText(FText::FromString(TEXT("조명 설정")));
						if (!SaveBlueprint(*Existing)) return nullptr;
					}
				}
				UE_LOG(LogFFFacilityInfoWbpBuilder, Display, TEXT("FF_FACILITY_LIGHTING_UMG_REUSE: %s (designer changes preserved)"), *LightingPackage);
				return Existing;
			}
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error, TEXT("FF_FACILITY_LIGHTING_UMG_FAIL: existing designer asset contract mismatch: %s"), *LightingPackage);
			return nullptr;
		}
		UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
		Factory->ParentClass = UFF_FacilityLightingControls::StaticClass();
		FAssetToolsModule& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		UWidgetBlueprint* Created = Cast<UWidgetBlueprint>(AssetTools.Get().CreateAsset(Name,
			FPackageName::GetLongPackagePath(LightingPackage), UWidgetBlueprint::StaticClass(), Factory));
		if (!Created || !Created->WidgetTree || !BuildLightingControls(*Created, *Created->WidgetTree) || !SaveBlueprint(*Created)) return nullptr;
		UE_LOG(LogFFFacilityInfoWbpBuilder, Display, TEXT("FF_FACILITY_LIGHTING_UMG_CREATED: %s"), *LightingPackage);
		return Created;
	}

	bool EnsureLightingWidgetContract(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, bool& bAdded)
	{
		bAdded = false;
		if (Tree.FindWidget(TEXT("Widget_LightingControls")))
			return HasNamedVariable(Blueprint, TEXT("Widget_LightingControls"), *UFF_FacilityLightingControls::StaticClass());
		UVerticalBox* Content = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Panel_Content")));
		UWidgetBlueprint* Lighting = Content ? EnsureLightingControlsBlueprint() : nullptr;
		if (!Content || !Lighting || !Lighting->GeneratedClass) return false;
		Blueprint.Modify();
		Content->Modify();
		UFF_FacilityLightingControls* Controls = Tree.ConstructWidget<UFF_FacilityLightingControls>(
			Lighting->GeneratedClass.Get(), TEXT("Widget_LightingControls"));
		if (!Controls) return false;
		Controls->bIsVariable = true;
		Blueprint.OnVariableAdded(Controls->GetFName());
		// Append without reparenting or resetting any designer-owned child or slot.
		if (!AddVertical(*Content, *Controls, FMargin(0, 0, 0, 8))) return false;
		Controls->SetVisibility(ESlateVisibility::Collapsed);
		bAdded = true;
		return true;
	}

	bool HasSynergyEntryContract(const UWidgetBlueprint& Blueprint)
	{
		for (const FName Name : {FName(TEXT("Text_Title")), FName(TEXT("Text_Partner")),
			FName(TEXT("Text_Effect")), FName(TEXT("Text_Condition")), FName(TEXT("Text_Status"))})
		{
			if (!HasNamedVariable(Blueprint, Name, *UTextBlock::StaticClass())) return false;
		}
		return true;
	}

	bool EnsureSynergyPartnerLink(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, bool& bAdded)
	{
		bAdded = false;
		UTextBlock* Partner = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Partner")));
		UWidget* Existing = Tree.FindWidget(TEXT("Button_OpenPartnerCatalog"));
		if (!Partner || !Partner->Slot || !Partner->GetParent()) return false;
		if (Existing)
		{
			UButton* Button = Cast<UButton>(Existing);
			if (!Button || !Button->bIsVariable) return false;
			bool bInsideButton = false;
			for (UWidget* Ancestor = Partner->GetParent(); Ancestor; Ancestor = Ancestor->GetParent())
			{
				bInsideButton |= Ancestor == Button;
				if (Ancestor->GetVisibility() == ESlateVisibility::HitTestInvisible) return false;
			}
			return bInsideButton;
		}
		UPanelWidget* Parent = Partner->GetParent();
		const int32 Index = Parent->GetChildIndex(Partner);
		// Do not wrap or replace an already authored button with another action.
		if (Index == INDEX_NONE || Parent->IsA<UButton>()) return false;
		Blueprint.Modify();
		Tree.Modify();
		Parent->Modify();
		Partner->Modify();
		Partner->Slot->Modify();
		UButton* Button = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_OpenPartnerCatalog"), true);
		if (!Button) return false;
		const FFF_UIPaletteInGame& Palette = UFF_UIPalette::Get()->InGame;
		FButtonStyle Style;
		Style.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.Disabled.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.Hovered = MakeSolidBrush(Palette.Hover, 4.0f);
		Style.Pressed = MakeSolidBrush(Palette.PaperSunken, 4.0f);
		Style.NormalPadding = FMargin(0.0f);
		Style.PressedPadding = FMargin(0.0f);
		Button->SetStyle(Style);
		Button->SetCursor(EMouseCursor::Hand);
		Button->SetToolTipText(NSLOCTEXT("FFFacilitySynergy", "OpenPartnerCatalog", "건축 목록에서 이 시설 보기"));
		// ReplaceChildAt retains the existing parent slot object, its index and every
		// authored layout value. Only the text's direct parent becomes a zero-pad button.
		if (!Parent->ReplaceChildAt(Index, Button)) return false;
		Partner->Slot = nullptr;
		UButtonSlot* TextSlot = Cast<UButtonSlot>(Button->AddChild(Partner));
		if (!TextSlot) return false;
		TextSlot->SetPadding(FMargin(0.0f));
		TextSlot->SetHorizontalAlignment(HAlign_Fill);
		TextSlot->SetVerticalAlignment(VAlign_Center);
		// The original read-only row root blocked all child hit testing. Convert only
		// blocking ancestors on this button's path; retain their visual/layout values.
		for (UWidget* Ancestor = Button->GetParent(); Ancestor; Ancestor = Ancestor->GetParent())
		{
			if (Ancestor->GetVisibility() == ESlateVisibility::HitTestInvisible)
			{
				Ancestor->Modify();
				Ancestor->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			}
		}
		bAdded = true;
		return true;
	}

	bool BuildSynergyEntryLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const FFF_UIPaletteInGame& P = UFF_UIPalette::Get()->InGame;
		UBorder* Spacing = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_EntrySpacing"));
		UBorder* Card = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Entry"));
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Vertical_Entry"));
		if (!Spacing || !Card || !Content) return false;
		Spacing->SetBrushColor(FLinearColor::Transparent);
		Spacing->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		Spacing->SetVisibility(ESlateVisibility::HitTestInvisible);
		PaintBorder(*Card, MakeSolidBrush(P.PaperSunken, 6.0f, P.Line, 1.0f));
		Card->SetPadding(FMargin(12.0f, 10.0f));
		auto AddText = [&](const FName Name, const TCHAR* Text, int32 FontSize, const FLinearColor& Color, const FName Typeface)
		{
			UCommonTextBlock* Label = MakeWidget<UCommonTextBlock>(Blueprint, Tree, Name, true);
			if (!Label) return false;
			Label->SetText(FText::FromString(Text));
			Label->SetAutoWrapText(true);
			StyleLightText(*Label, FontSize, Color, Typeface);
			AddVertical(*Content, *Label, FMargin(0, 0, 0, 5));
			return true;
		};
		if (!AddText(TEXT("Text_Title"), TEXT("Burger + Seating"), 12, P.InkMuted, TEXT("Medium")) ||
			!AddText(TEXT("Text_Partner"), TEXT("함께 놓을 시설: Seating"), 16, P.Ink, TEXT("SemiBold")) ||
			!AddText(TEXT("Text_Effect"), TEXT("Burger 이용 수수료 +10%"), 12, P.TealDeep, TEXT("SemiBold")) ||
			!AddText(TEXT("Text_Condition"), TEXT("시설 중심 간 수평 거리 15m 이내"), 11, P.InkMuted, TEXT("Regular")) ||
			!AddText(TEXT("Text_Status"), TEXT("성립 · 현재 조합 보너스 적용 중"), 11, P.Ink, TEXT("Medium"))) return false;
		Card->AddChild(Content);
		Spacing->AddChild(Card);
		Tree.RootWidget = Spacing;
		bool bAddedPartnerLink = false;
		return HasSynergyEntryContract(Blueprint) && EnsureSynergyPartnerLink(Blueprint, Tree, bAddedPartnerLink);
	}

	UWidgetBlueprint* EnsureSynergyEntryBlueprint()
	{
		UWidgetBlueprint* Blueprint = FFUIWbpBuilder::EnsureWidgetBlueprint(TEXT("FF_FACILITY_SYNERGY_ENTRY"),
			SynergyEntryPackage, FPackageName::GetLongPackagePath(SynergyEntryPackage),
			*UFF_FacilitySynergyEntryWidget::StaticClass(), BuildSynergyEntryLayout);
		if (!Blueprint || !Blueprint->GeneratedClass ||
			!Blueprint->GeneratedClass->IsChildOf(UFF_FacilitySynergyEntryWidget::StaticClass()) ||
			!HasSynergyEntryContract(*Blueprint))
		{
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error, TEXT("FF_FACILITY_SYNERGY_ENTRY_FAIL: existing or generated entry contract is invalid; no tree reset performed."));
			return nullptr;
		}
		return Blueprint;
	}

	bool HasSynergyInspectorContract(const UWidgetBlueprint& Blueprint)
	{
		return HasNamedVariable(Blueprint, TEXT("Button_TabSynergy"), *UButton::StaticClass()) &&
			HasNamedVariable(Blueprint, TEXT("Panel_SynergySection"), *UVerticalBox::StaticClass()) &&
			HasNamedVariable(Blueprint, TEXT("Text_SynergySummary"), *UCommonTextBlock::StaticClass()) &&
			HasNamedVariable(Blueprint, TEXT("VerticalBox_SynergyEntries"), *UVerticalBox::StaticClass());
	}

	bool EnsureSynergyInspectorContract(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, bool& bAdded)
	{
		bAdded = false;
		if (HasSynergyInspectorContract(Blueprint)) return true;
		// Refuse incompatible/partial designer edits before touching any existing nodes.
		for (const FName Name : {FName(TEXT("Button_TabSynergy")), FName(TEXT("Text_TabSynergy")),
			FName(TEXT("Panel_SynergySection")), FName(TEXT("Text_SynergySummary")), FName(TEXT("VerticalBox_SynergyEntries"))})
			if (Tree.FindWidget(Name)) return false;
		UHorizontalBox* Tabs = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("Panel_InspectorTabs")));
		UVerticalBox* Content = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Panel_Content")));
		UWidget* Overview = Tree.FindWidget(TEXT("Button_TabOverview"));
		if (!Tabs || !Content || !Overview || Overview->GetParent() != Tabs ||
			!Cast<UScrollBox>(Content->GetParent())) return false;
		const FFF_UIPaletteInGame& P = UFF_UIPalette::Get()->InGame;
		Blueprint.Modify();
		Tabs->Modify();
		Content->Modify();
		UButton* Tab = MakeLightTextButton(Blueprint, Tree, TEXT("Button_TabSynergy"), TEXT("Text_TabSynergy"),
			FText::FromString(TEXT("조합")), P.PaperSunken, P.TealSoft, P.TealSoft, P.Line, P.TealDeep, true, false);
		UVerticalBox* Section = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_SynergySection"), true);
		UCommonTextBlock* Summary = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_SynergySummary"), true);
		UVerticalBox* Entries = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_SynergyEntries"), true);
		if (!Tab || !Section || !Summary || !Entries) return false;
		// ShiftChild preserves all preexisting tab objects and their slot/style properties.
		AddHorizontal(*Tabs, *Tab, FMargin(2.0f, 0.0f), ESlateSizeRule::Fill);
		Tabs->ShiftChild(Tabs->GetChildIndex(Overview) + 1, Tab);
		Summary->SetText(FText::FromString(TEXT("시설의 조합 상태와 추가 수익을 확인할 수 있습니다.")));
		Summary->SetAutoWrapText(true);
		StyleLightText(*Summary, 12, P.InkMuted, TEXT("Regular"));
		AddVertical(*Section, *Summary, FMargin(2, 2, 2, 10), ESlateSizeRule::Automatic, VAlign_Top);
		AddVertical(*Section, *Entries);
		AddVertical(*Content, *Section, FMargin(0, 0, 0, 8), ESlateSizeRule::Automatic, VAlign_Top);
		Section->SetVisibility(ESlateVisibility::Collapsed);
		bAdded = true;
		return HasSynergyInspectorContract(Blueprint);
	}

	bool EnsureStaffNavigation(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, bool& bOutModified)
	{
		UWidget* Operation = Tree.FindWidget(TEXT("Button_TabOperation"));
		auto* Tabs = Operation ? Cast<UHorizontalBox>(Operation->GetParent()) : nullptr;
		if (!Tabs) return false;
		TArray<UWidget*> Primary;
		for (const TCHAR* Name : {TEXT("Button_TabOverview"), TEXT("Button_TabSynergy"),
			TEXT("Button_TabOperation"), TEXT("Button_TabStaff")})
		{
			UWidget* Tab = Tree.FindWidget(Name);
			if (!Tab || Tab->GetParent() != Tabs) return false;
			Primary.Add(Tab);
		}
		TArray<UWidget*> Optional;
		for (const TCHAR* Name : {TEXT("Button_TabUpgrade"), TEXT("Button_TabRevenue"),
			TEXT("Button_TabPrograms"), TEXT("Button_TabLighting")})
			if (UWidget* Tab = Tree.FindWidget(Name))
				if (Tab->GetParent() == Tabs) Optional.Add(Tab);
		if (!Optional.IsEmpty())
		{
			UWidget* ExistingTools = Tree.FindWidget(TEXT("Reference_AdditionalFacilityPages"));
			if (!ExistingTools) ExistingTools = Tree.FindWidget(TEXT("Clarity_SecondaryTabs"));
			if (!ExistingTools) ExistingTools = Tree.FindWidget(TEXT("Panel_InspectorTools"));
			auto* Tools = Cast<UHorizontalBox>(ExistingTools);
			if (ExistingTools && (!Tools || Tools == Tabs)) return false;
			if (!Tools)
				for (const TCHAR* Name : {TEXT("Button_TabRevenue"), TEXT("Button_TabPrograms")})
					if (UWidget* Tab = Tree.FindWidget(Name))
						if (auto* Row = Cast<UHorizontalBox>(Tab->GetParent()))
							if (Row != Tabs) { Tools = Row; break; }
			// The primary row can be wrapped in an authored SizeBox. Only a newly
			// created tools row needs its nearest vertical ancestor and wrapper anchor.
			UWidget* NavigationAnchor = Tabs;
			UVerticalBox* NavigationColumn = nullptr;
			if (!Tools)
			{
				while (NavigationAnchor->GetParent())
				{
					NavigationColumn = Cast<UVerticalBox>(NavigationAnchor->GetParent());
					if (NavigationColumn) break;
					NavigationAnchor = NavigationAnchor->GetParent();
				}
				if (!NavigationColumn) return false;
			}
			Blueprint.Modify();
			Tree.Modify();
			Tabs->Modify();
			if (!Tools)
			{
				NavigationColumn->Modify();
				Tools = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Panel_InspectorTools"));
				if (!Tools) return false;
				AddVertical(*NavigationColumn, *Tools, FMargin(8, 0, 8, 6));
				NavigationColumn->ShiftChild(NavigationColumn->GetChildIndex(NavigationAnchor) + 1, Tools);
			}
			Tools->Modify();
			for (UWidget* Tab : Optional)
			{
				Tab->Modify();
				Tab->RemoveFromParent();
				AddHorizontal(*Tools, *Tab, FMargin(2.f, 0.f), ESlateSizeRule::Fill);
			}
			bOutModified = true;
		}
		for (int32 Index = 0; Index < Primary.Num(); ++Index)
			if (Tabs->GetChildIndex(Primary[Index]) != Index)
			{
				Blueprint.Modify();
				Tabs->Modify();
				Tabs->ShiftChild(Index, Primary[Index]);
				bOutModified = true;
			}
		return Tabs->GetChildrenCount() == Primary.Num();
	}

	bool EnsureStaffWidgetContract(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, bool& bOutModified)
	{
		bOutModified = false;
		const auto HasContract = [&Blueprint]()
		{
			return HasNamedVariable(Blueprint, TEXT("Button_TabStaff"), *UButton::StaticClass()) &&
				HasNamedVariable(Blueprint, TEXT("Panel_StaffSection"), *UVerticalBox::StaticClass()) &&
				HasNamedVariable(Blueprint, TEXT("Text_StaffSummary"), *UCommonTextBlock::StaticClass()) &&
				HasNamedVariable(Blueprint, TEXT("Button_AssignStaff"), *UButton::StaticClass()) &&
				HasNamedVariable(Blueprint, TEXT("Button_UnassignStaff"), *UButton::StaticClass());
		};
		// Repair a previously saved staff section whose older inspector had no secondary
		// navigation row; retaining the binding contract must not skip layout migration.
		if (HasContract()) return EnsureStaffNavigation(Blueprint, Tree, bOutModified);
		// Runtime fallback controls are not saved to the WBP. Refuse conflicting authored
		// names instead of replacing the designer's widgets or their event bindings.
		for (const FName Name : {FName(TEXT("Button_TabStaff")), FName(TEXT("Text_TabStaff")),
			FName(TEXT("Panel_StaffSection")), FName(TEXT("Text_StaffHeading")), FName(TEXT("Text_StaffSummary")),
			FName(TEXT("Text_StaffHelp")), FName(TEXT("Panel_StaffActions")), FName(TEXT("Button_AssignStaff")),
			FName(TEXT("Text_AssignStaff")), FName(TEXT("Button_UnassignStaff")), FName(TEXT("Text_UnassignStaff"))})
			if (Tree.FindWidget(Name)) return false;
		auto* Operation = Cast<UButton>(Tree.FindWidget(TEXT("Button_TabOperation")));
		auto* Tabs = Operation ? Cast<UHorizontalBox>(Operation->GetParent()) : nullptr;
		auto* Content = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Panel_Content")));
		if (!Operation || !Tabs || !Content) return false;
		const FFF_UIPaletteInGame& P = UFF_UIPalette::Get()->InGame;
		Blueprint.Modify();
		Tree.Modify();
		Tabs->Modify();
		Content->Modify();
		auto MakeStaffButton = [&](const FName Name, const FName TextName, const TCHAR* Caption, UButton* Reference)
		{
			UButton* Button = MakeLightTextButton(Blueprint, Tree, Name, TextName, FText::FromString(Caption),
				P.PaperSunken, P.TealSoft, P.TealSoft, P.Line, P.TealDeep, true);
			if (Button && Reference)
			{
				Button->SetStyle(Reference->GetStyle());
				if (auto* Label = Cast<UCommonTextBlock>(Button->GetContent()))
					if (auto* ReferenceLabel = Cast<UCommonTextBlock>(Reference->GetContent()))
					{
						Label->SetFont(ReferenceLabel->GetFont());
						Label->SetColorAndOpacity(ReferenceLabel->GetColorAndOpacity());
					}
			}
			return Button;
		};
		UButton* Tab = MakeStaffButton(TEXT("Button_TabStaff"), TEXT("Text_TabStaff"), TEXT("스태프"), Operation);
		auto* Section = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_StaffSection"), true);
		auto* Heading = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_StaffHeading"));
		auto* Summary = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_StaffSummary"), true);
		auto* Help = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_StaffHelp"));
		auto* Actions = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Panel_StaffActions"));
		UButton* Assign = MakeStaffButton(TEXT("Button_AssignStaff"), TEXT("Text_AssignStaff"), TEXT("+ 배정"),
			Cast<UButton>(Tree.FindWidget(TEXT("Button_Move"))));
		UButton* Unassign = MakeStaffButton(TEXT("Button_UnassignStaff"), TEXT("Text_UnassignStaff"), TEXT("- 해제"), Operation);
		if (!Tab || !Section || !Heading || !Summary || !Help || !Actions || !Assign || !Unassign) return false;
		AddHorizontal(*Tabs, *Tab, FMargin(2.f, 0.f), ESlateSizeRule::Fill);
		Tabs->ShiftChild(Tabs->GetChildIndex(Operation) + 1, Tab);
		Heading->SetText(FText::FromString(TEXT("스태프 배정")));
		StyleLightText(*Heading, 16, P.Ink, TEXT("SemiBold"));
		Summary->SetText(FText::FromString(TEXT("스태프 배정 0 / 1명 · 출동 중 0명")));
		Summary->SetAutoWrapText(true);
		StyleLightText(*Summary, 14, P.Ink, TEXT("Regular"));
		Help->SetText(FText::FromString(TEXT("미배정 스태프를 + 배정으로 이 시설에 배치하세요. 출동 중인 스태프는 복귀 후 해제할 수 있습니다.")));
		Help->SetAutoWrapText(true);
		StyleLightText(*Help, 12, P.InkMuted, TEXT("Regular"));
		AddVertical(*Section, *Heading, FMargin(8, 12, 8, 12));
		AddVertical(*Section, *Summary, FMargin(8, 0, 8, 10));
		AddVertical(*Section, *Help, FMargin(8, 0, 8, 16));
		// Both actions share the taller move-style button's hit target, including
		// at 720p DPI. Centering the smaller tab-style action shrinks it to 21px.
		AddHorizontal(*Actions, *Assign, FMargin(0, 0, 6, 0), ESlateSizeRule::Fill)->SetVerticalAlignment(VAlign_Fill);
		AddHorizontal(*Actions, *Unassign, FMargin(6, 0, 0, 0), ESlateSizeRule::Fill)->SetVerticalAlignment(VAlign_Fill);
		AddVertical(*Section, *Actions, FMargin(8, 0, 8, 12));
		AddVertical(*Content, *Section, FMargin(0, 0, 0, 8));
		Section->SetVisibility(ESlateVisibility::Collapsed);
		bOutModified = true;
		return HasContract() && EnsureStaffNavigation(Blueprint, Tree, bOutModified);
	}

	bool BuildLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			return false;
		}

		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("Root_Canvas"));
		USizeBox* ShadowSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Panel_ShadowSize"));
		UBorder* Shadow = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_PanelShadow"));
		USizeBox* PanelSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Panel_Size"));
		UBorder* Panel = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Panel"), true);
		UVerticalBox* Shell = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_Shell"));
		USizeBox* Header = BuildLightHeader(Blueprint, Tree, *Palette);
		UBorder* Body = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Body_Surface"));
		UScrollBox* DetailsScroll = MakeWidget<UScrollBox>(
			Blueprint, Tree, TEXT("ScrollBox_Details"));
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_Content"));
		UBorder* Summary = BuildIdentitySummary(Blueprint, Tree, *Palette);
		UVerticalBox* UpgradeSection = BuildUpgradeSection(Blueprint, Tree, *Palette);
		UVerticalBox* RevenueSection = BuildRevenueSection(Blueprint, Tree, *Palette);
		UCommonTextBlock* RelocationWarning = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_RelocationWarning"), true);
		UBorder* BuildSection = BuildInformationSection(Blueprint, Tree, *Palette, false);
		UBorder* LiveSection = BuildInformationSection(Blueprint, Tree, *Palette, true);
		UVerticalBox* ProgramSection = BuildProgramSection(Blueprint, Tree, *Palette);
		UCommonTextBlock* Feedback = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_Feedback"), true);
		UBorder* Actions = BuildActionRail(Blueprint, Tree, *Palette);
		if (!Root || !ShadowSize || !Shadow || !PanelSize || !Panel || !Shell || !Header ||
			!Body || !DetailsScroll || !Content || !Summary || !UpgradeSection || !RevenueSection ||
			!RelocationWarning || !BuildSection || !LiveSection || !ProgramSection || !Feedback || !Actions)
		{
			return false;
		}

		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		ShadowSize->SetWidthOverride(PanelWidth);
		ShadowSize->SetHeightOverride(PanelHeight);
		PaintBorder(*Shadow, MakeSolidBrush(FLinearColor(0.04f, 0.08f, 0.06f, 0.16f), 10.0f));
		Shadow->SetVisibility(ESlateVisibility::HitTestInvisible);
		ShadowSize->AddChild(Shadow);
		PanelSize->SetWidthOverride(PanelWidth);
		PanelSize->SetHeightOverride(PanelHeight);
		PaintBorder(*Panel, MakeSolidBrush(
			Palette->InGame.Paper, 8.0f, Palette->InGame.Line, 1.0f));
		Panel->SetPadding(FMargin(0.0f));
		Panel->SetClipping(EWidgetClipping::ClipToBoundsAlways);

		PaintBorder(*Body, MakeSolidBrush(Palette->InGame.Paper, 0.0f));
		Body->SetPadding(FMargin(10.0f, 8.0f, 10.0f, 6.0f));
		DetailsScroll->SetAnimateWheelScrolling(true);
		if (UScrollBoxSlot* Slot = Cast<UScrollBoxSlot>(DetailsScroll->AddChild(Content)))
		{
			Slot->SetPadding(FMargin(0.0f));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}
		else
		{
			return false;
		}
		Body->AddChild(DetailsScroll);

		AddVertical(*Content, *Summary, FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		AddVertical(*Content, *UpgradeSection, FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		AddVertical(*Content, *RevenueSection, FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		RelocationWarning->SetText(FText::FromString(TEXT("! 재배치가 필요합니다.")));
		RelocationWarning->SetAutoWrapText(true);
		RelocationWarning->SetVisibility(ESlateVisibility::Collapsed);
		StyleLightText(*RelocationWarning, 12, Palette->InGame.Amber, TEXT("SemiBold"));
		AddVertical(*Content, *RelocationWarning, FMargin(2.0f, 0.0f, 2.0f, 8.0f));
		AddVertical(*Content, *BuildSection, FMargin(0.0f, 0.0f, 0.0f, 10.0f));
		AddVertical(*Content, *LiveSection, FMargin(0.0f, 0.0f, 0.0f, 10.0f));
		AddVertical(*Content, *ProgramSection, FMargin(0.0f, 0.0f, 0.0f, 10.0f));
		Feedback->SetText(FText::GetEmpty());
		Feedback->SetAutoWrapText(true);
		Feedback->SetVisibility(ESlateVisibility::Collapsed);
		StyleLightText(
			*Feedback, 12, Palette->InGame.Amber, TEXT("Medium"), ETextJustify::Center);
		AddVertical(*Content, *Feedback, FMargin(2.0f, 0.0f, 2.0f, 2.0f));

		AddVertical(*Shell, *Header);
		AddVertical(*Shell, *Body, FMargin(0.0f), ESlateSizeRule::Fill, VAlign_Fill);
		AddVertical(*Shell, *Actions);
		Panel->AddChild(Shell);
		PanelSize->AddChild(Panel);

		if (UCanvasPanelSlot* ShadowSlot = Root->AddChildToCanvas(ShadowSize))
		{
			ShadowSlot->SetAnchors(FAnchors(1.0f, 0.0f));
			ShadowSlot->SetAlignment(FVector2D(1.0f, 0.0f));
			ShadowSlot->SetPosition(FVector2D(
				-(PanelRightMargin - 4.0f), PanelTopMargin + 4.0f));
			ShadowSlot->SetSize(FVector2D(PanelWidth, PanelHeight));
			ShadowSlot->SetAutoSize(false);
		}
		else
		{
			return false;
		}

		if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(PanelSize))
		{
			PanelSlot->SetAnchors(FAnchors(1.0f, 0.0f));
			PanelSlot->SetAlignment(FVector2D(1.0f, 0.0f));
			PanelSlot->SetPosition(FVector2D(-PanelRightMargin, PanelTopMargin));
			PanelSlot->SetSize(FVector2D(PanelWidth, PanelHeight));
			PanelSlot->SetAutoSize(false);
		}
		else
		{
			return false;
		}

		Tree.RootWidget = Root;
		bool bUpgradeContractAdded = false;
		bool bLightingContractAdded = false;
		bool bSynergyContractAdded = false;
		return EnsureUpgradeWidgetContract(Blueprint, Tree, bUpgradeContractAdded) &&
			EnsureLightingWidgetContract(Blueprint, Tree, bLightingContractAdded) &&
			MigrateInspectorControls(Blueprint, Tree) && EnsureSynergyEntryBlueprint() &&
			EnsureSynergyInspectorContract(Blueprint, Tree, bSynergyContractAdded);
	}

	// 화면 주소와 오브젝트 경로 변환은 공통 창구를 사용한다.
	UWidgetBlueprint* LoadExistingBlueprint()
	{
		return FFUIWbpBuilder::LoadExistingBlueprint(PanelPackage);
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
		// Geometry belongs to the designer. Validate the binding surface, not template pixels.

		const TArray<FName> RequiredVariables = {
			TEXT("Border_Panel"), TEXT("Button_Close"), TEXT("Button_Move"),
			TEXT("Button_Remove"),
			TEXT("Image_Thumbnail"), TEXT("Text_ThumbnailPlaceholder"),
			TEXT("Panel_BuildSection"), TEXT("Panel_LiveSection"), TEXT("Text_Context"),
			TEXT("Panel_Actions"), TEXT("Panel_BuildActions"),
			TEXT("Panel_UpgradeComparison"), TEXT("Text_UpgradeCurrent"), TEXT("Text_UpgradeNext"),
			TEXT("Text_FacilityName"), TEXT("Text_Category"), TEXT("Text_Zone"),
			TEXT("Text_FacilityState"), TEXT("Text_RelocationWarning"), TEXT("Text_Footprint"),
			TEXT("Text_InstallCost"), TEXT("Text_RecoveryCost"),
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
			Cast<UScrollBox>(Tree->FindWidget(TEXT("ScrollBox_Details"))) &&
			Cast<UVerticalBox>(Tree->FindWidget(TEXT("Panel_Content"))) &&
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

	// 부모 없는 위젯과 컴파일 오류를 검사한 뒤 팀 공통 저장 절차를 수행한다.
	bool SaveBlueprint(UWidgetBlueprint& Blueprint)
	{
		return FFUIWbpBuilder::SaveNewBlueprint(TEXT("FF_FACILITY_INFO_UMG"), Blueprint);
	}

	bool HasNamedVariable(
		const UWidgetBlueprint& Blueprint,
		const FName Name,
		const UClass& ExpectedClass)
	{
		const UWidget* Widget = Blueprint.WidgetTree
			? Blueprint.WidgetTree->FindWidget(Name)
			: nullptr;
		return Widget && Widget->IsA(&ExpectedClass) && Widget->bIsVariable &&
			Blueprint.WidgetVariableNameToGuidMap.Contains(Name);
	}

	bool HasPaidProgramCardContract(const UWidgetBlueprint& Blueprint)
	{
		return HasNamedVariable(Blueprint, TEXT("Button_Select"), *UButton::StaticClass()) &&
			HasNamedVariable(Blueprint, TEXT("Text_ProgramName"), *UCommonTextBlock::StaticClass()) &&
			HasNamedVariable(Blueprint, TEXT("Text_ProgramMeta"), *UCommonTextBlock::StaticClass()) &&
			HasNamedVariable(Blueprint, TEXT("Text_ProgramAvailability"), *UCommonTextBlock::StaticClass());
	}

	bool BuildPaidProgramCardLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		UButton* Select = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Select"), true);
		UVerticalBox* Content = MakeWidget<UVerticalBox>(
			Blueprint, Tree, TEXT("Panel_CardContent"));
		UCommonTextBlock* Name = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_ProgramName"), true);
		UCommonTextBlock* Meta = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_ProgramMeta"), true);
		UCommonTextBlock* Availability = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_ProgramAvailability"), true);
		if (!Palette || !Select || !Content || !Name || !Meta || !Availability)
		{
			return false;
		}
		Tree.RootWidget = Select;
		FButtonStyle CardStyle;
		CardStyle.Normal = MakeSolidBrush(
			Palette->InGame.White, 6.0f, Palette->InGame.Line, 1.0f);
		CardStyle.Hovered = MakeSolidBrush(
			Palette->InGame.Hover, 6.0f, Palette->InGame.TealSoft, 1.0f);
		CardStyle.Pressed = MakeSolidBrush(
			Palette->InGame.TealSoft, 6.0f, Palette->InGame.TealDeep, 1.0f);
		CardStyle.Disabled = MakeSolidBrush(
			Palette->InGame.PaperSunken, 6.0f, Palette->InGame.Line, 1.0f);
		CardStyle.NormalPadding = FMargin(0.0f);
		CardStyle.PressedPadding = FMargin(0.0f, 1.0f, 0.0f, 0.0f);
		Select->SetStyle(CardStyle);
		Select->SetBackgroundColor(Palette->InGame.White);
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Select->AddChild(Content)))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		Name->SetText(FText::FromString(TEXT("프로그램 이름")));
		StyleLightText(*Name, 15, Palette->InGame.Ink, TEXT("SemiBold"));
		Meta->SetText(FText::FromString(TEXT("개장 +0분 ~ +0분 · 0 Gold")));
		Meta->SetAutoWrapText(true);
		StyleLightText(*Meta, 11, Palette->InGame.InkMuted, TEXT("Medium"));
		Availability->SetText(FText::FromString(TEXT("편성 가능")));
		Availability->SetAutoWrapText(true);
		StyleLightText(*Availability, 11, Palette->InGame.TealDeep, TEXT("SemiBold"));
		AddVertical(*Content, *Name, FMargin(12.0f, 9.0f, 12.0f, 2.0f));
		AddVertical(*Content, *Meta, FMargin(12.0f, 0.0f, 12.0f, 2.0f));
		AddVertical(*Content, *Availability, FMargin(12.0f, 0.0f, 12.0f, 9.0f));
		return HasPaidProgramCardContract(Blueprint);
	}

	bool HasPaidProgramPickerContract(const UWidgetBlueprint& Blueprint)
	{
		const UWidgetTree* Tree = Blueprint.WidgetTree;
		const UCanvasPanel* Root = Tree ? Cast<UCanvasPanel>(Tree->RootWidget) : nullptr;
		const UBorder* Picker = Tree
			? Cast<UBorder>(Tree->FindWidget(TEXT("Border_Picker")))
			: nullptr;
		const UCanvasPanelSlot* PickerSlot = Picker
			? Cast<UCanvasPanelSlot>(Picker->Slot)
			: nullptr;
		const bool bSafeDockContract = Root && PickerSlot &&
			Root->GetVisibility() == ESlateVisibility::SelfHitTestInvisible;
		return bSafeDockContract &&
			HasNamedVariable(Blueprint, TEXT("Border_Picker"), *UBorder::StaticClass()) &&
			HasNamedVariable(Blueprint, TEXT("Button_Close"), *UButton::StaticClass()) &&
			HasNamedVariable(Blueprint, TEXT("Text_FacilityName"), *UCommonTextBlock::StaticClass()) &&
			HasNamedVariable(Blueprint, TEXT("Text_Empty"), *UCommonTextBlock::StaticClass()) &&
			HasNamedVariable(Blueprint, TEXT("ScrollBox_Programs"), *UScrollBox::StaticClass()) &&
			HasNamedVariable(Blueprint, TEXT("Text_PreviewName"), *UCommonTextBlock::StaticClass()) &&
			HasNamedVariable(Blueprint, TEXT("Text_PreviewDescription"), *UCommonTextBlock::StaticClass()) &&
			HasNamedVariable(Blueprint, TEXT("Text_PreviewDetails"), *UCommonTextBlock::StaticClass()) &&
			HasNamedVariable(Blueprint, TEXT("Text_PreviewAvailability"), *UCommonTextBlock::StaticClass()) &&
			HasNamedVariable(Blueprint, TEXT("Text_NoRefundNotice"), *UCommonTextBlock::StaticClass()) &&
			HasNamedVariable(Blueprint, TEXT("Text_Feedback"), *UCommonTextBlock::StaticClass()) &&
			HasNamedVariable(Blueprint, TEXT("Button_Confirm"), *UButton::StaticClass()) &&
			HasNamedVariable(Blueprint, TEXT("Text_Confirm"), *UCommonTextBlock::StaticClass());
	}

	bool BuildPaidProgramPickerLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("Root_Canvas"));
		UBorder* PickerBorder = MakeWidget<UBorder>(
			Blueprint, Tree, TEXT("Border_Picker"), true);
		UVerticalBox* Shell = MakeWidget<UVerticalBox>(
			Blueprint, Tree, TEXT("Panel_PickerShell"));
		if (!Palette || !Root || !PickerBorder || !Shell)
		{
			return false;
		}
		Tree.RootWidget = Root;
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		PaintBorder(*PickerBorder, MakeSolidBrush(
			Palette->InGame.Paper, 8.0f, Palette->InGame.LineStrong, 1.0f));
		PickerBorder->SetPadding(FMargin(0.0f));
		PickerBorder->SetClipping(EWidgetClipping::ClipToBoundsAlways);
		PickerBorder->AddChild(Shell);
		UCanvasPanelSlot* PickerSlot = Root->AddChildToCanvas(PickerBorder);
		if (!PickerSlot)
		{
			return false;
		}
		FFFacilityPaidProgramLayout::Apply(*PickerSlot, FFFacilityPaidProgramLayout::EVariant::InspectorDock);

		UBorder* HeaderSurface = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Header"));
		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Panel_Header"));
		UVerticalBox* HeaderCopy = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_HeaderCopy"));
		UCommonTextBlock* Eyebrow = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_Eyebrow"));
		UCommonTextBlock* Title = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_Title"));
		UButton* Close = MakeLightTextButton(
			Blueprint, Tree, TEXT("Button_Close"), TEXT("Text_Close"),
			FText::FromString(TEXT("닫기  ×")), FLinearColor::Transparent,
			Palette->InGame.Teal, Palette->InGame.TealDeep, Palette->InGame.TealDeep,
			Palette->InGame.White, true, false);
		if (!HeaderSurface || !Header || !HeaderCopy || !Eyebrow || !Title || !Close)
		{
			return false;
		}
		PaintBorder(*HeaderSurface, MakeSolidBrush(Palette->InGame.TealDeep, 6.0f));
		HeaderSurface->SetPadding(FMargin(16.0f, 10.0f));
		Eyebrow->SetText(FText::FromString(TEXT("시설 유료 프로그램")));
		StyleLightText(*Eyebrow, 9, Palette->InGame.TealSoft, TEXT("Medium"));
		Title->SetText(FText::FromString(TEXT("구매·편성")));
		StyleLightText(*Title, 22, Palette->InGame.White, TEXT("SemiBold"));
		AddVertical(*HeaderCopy, *Eyebrow, FMargin(0.0f, 0.0f, 0.0f, 1.0f));
		AddVertical(*HeaderCopy, *Title);
		AddHorizontal(*Header, *HeaderCopy, FMargin(0.0f), ESlateSizeRule::Fill);
		AddHorizontal(*Header, *Close);
		HeaderSurface->AddChild(Header);
		AddVertical(*Shell, *HeaderSurface);

		UBorder* BodySurface = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Body"));
		UVerticalBox* Body = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_PickerContent"));
		UCommonTextBlock* FacilityName = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_FacilityName"), true);
		UCommonTextBlock* Instruction = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_Instruction"));
		if (!BodySurface || !Body || !FacilityName || !Instruction)
		{
			return false;
		}
		PaintBorder(*BodySurface, MakeSolidBrush(Palette->InGame.Paper, 0.0f));
		BodySurface->SetPadding(FMargin(12.0f, 10.0f));
		FacilityName->SetText(FText::FromString(TEXT("선택 시설 전용 프로그램")));
		FacilityName->SetAutoWrapText(true);
		StyleLightText(*FacilityName, 14, Palette->InGame.TealDeep, TEXT("SemiBold"));
		Instruction->SetText(FText::FromString(
			TEXT("카드를 선택해 내용을 확인한 뒤 아래 버튼으로 구매하세요.")));
		Instruction->SetAutoWrapText(true);
		StyleLightText(*Instruction, 11, Palette->InGame.InkMuted, TEXT("Medium"));
		AddVertical(*Body, *FacilityName, FMargin(2.0f, 0.0f, 2.0f, 2.0f));
		AddVertical(*Body, *Instruction, FMargin(2.0f, 0.0f, 2.0f, 8.0f));

		UBorder* ListSurface = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_ProgramList"));
		UOverlay* ListOverlay = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_ProgramList"));
		UScrollBox* ProgramList = MakeWidget<UScrollBox>(
			Blueprint, Tree, TEXT("ScrollBox_Programs"), true);
		UCommonTextBlock* Empty = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_Empty"), true);
		if (!ListSurface || !ListOverlay || !ProgramList || !Empty)
		{
			return false;
		}
		PaintBorder(*ListSurface, MakeSolidBrush(
			Palette->InGame.PaperSunken, 6.0f, Palette->InGame.Line, 1.0f));
		ListSurface->SetPadding(FMargin(7.0f));
		ProgramList->SetAnimateWheelScrolling(true);
		Empty->SetText(FText::FromString(TEXT("호환 프로그램이 없습니다.")));
		StyleLightText(*Empty, 12, Palette->InGame.InkMuted, TEXT("Medium"), ETextJustify::Center);
		if (UOverlaySlot* Slot = ListOverlay->AddChildToOverlay(ProgramList))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
		if (UOverlaySlot* Slot = ListOverlay->AddChildToOverlay(Empty))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		ListSurface->AddChild(ListOverlay);
		AddVertical(*Body, *ListSurface, FMargin(0.0f, 0.0f, 0.0f, 8.0f),
			ESlateSizeRule::Fill, VAlign_Fill);

		UBorder* PreviewSurface = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Preview"));
		UVerticalBox* Preview = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_Preview"));
		UCommonTextBlock* PreviewName = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_PreviewName"), true);
		UCommonTextBlock* PreviewDescription = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_PreviewDescription"), true);
		UCommonTextBlock* PreviewDetails = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_PreviewDetails"), true);
		UCommonTextBlock* PreviewAvailability = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_PreviewAvailability"), true);
		UCommonTextBlock* Feedback = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_Feedback"), true);
		if (!PreviewSurface || !Preview || !PreviewName || !PreviewDescription ||
			!PreviewDetails || !PreviewAvailability || !Feedback)
		{
			return false;
		}
		PaintBorder(*PreviewSurface, MakeSolidBrush(
			Palette->InGame.White, 6.0f, Palette->InGame.Line, 1.0f));
		PreviewSurface->SetPadding(FMargin(10.0f, 8.0f));
		PreviewName->SetText(FText::FromString(TEXT("프로그램을 선택하세요")));
		StyleLightText(*PreviewName, 14, Palette->InGame.Ink, TEXT("SemiBold"));
		PreviewDescription->SetText(FText::GetEmpty());
		PreviewDescription->SetAutoWrapText(true);
		StyleLightText(*PreviewDescription, 11, Palette->InGame.InkMuted, TEXT("Medium"));
		PreviewDetails->SetText(FText::GetEmpty());
		PreviewDetails->SetAutoWrapText(true);
		StyleLightText(*PreviewDetails, 11, Palette->InGame.Ink, TEXT("Medium"));
		PreviewAvailability->SetText(FText::GetEmpty());
		PreviewAvailability->SetAutoWrapText(true);
		StyleLightText(*PreviewAvailability, 11, Palette->InGame.TealDeep, TEXT("SemiBold"));
		Feedback->SetText(FText::GetEmpty());
		Feedback->SetAutoWrapText(true);
		Feedback->SetVisibility(ESlateVisibility::Collapsed);
		StyleLightText(*Feedback, 11, Palette->InGame.Danger, TEXT("SemiBold"));
		AddVertical(*Preview, *PreviewName, FMargin(0.0f, 0.0f, 0.0f, 2.0f));
		AddVertical(*Preview, *PreviewDescription, FMargin(0.0f, 0.0f, 0.0f, 2.0f));
		AddVertical(*Preview, *PreviewDetails, FMargin(0.0f, 0.0f, 0.0f, 2.0f));
		AddVertical(*Preview, *PreviewAvailability);
		PreviewSurface->AddChild(Preview);
		AddVertical(*Body, *PreviewSurface, FMargin(0.0f, 0.0f, 0.0f, 5.0f));
		AddVertical(*Body, *Feedback);
		BodySurface->AddChild(Body);
		AddVertical(*Shell, *BodySurface, FMargin(0.0f), ESlateSizeRule::Fill, VAlign_Fill);

		UBorder* FooterSurface = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Footer"));
		UVerticalBox* Footer = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_Footer"));
		UCommonTextBlock* NoRefund = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_NoRefundNotice"), true);
		UButton* Confirm = MakeLightTextButton(
			Blueprint, Tree, TEXT("Button_Confirm"), TEXT("Text_Confirm"),
			FText::FromString(TEXT("구매·편성")), Palette->InGame.Teal,
			Palette->InGame.TealDeep, Palette->InGame.TealDeep, Palette->InGame.LineStrong,
			Palette->InGame.White, true, true);
		if (!FooterSurface || !Footer || !NoRefund || !Confirm)
		{
			return false;
		}
		PaintBorder(*FooterSurface, MakeSolidBrush(Palette->InGame.StatusBar, 0.0f));
		FooterSurface->SetPadding(FMargin(12.0f, 8.0f));
		NoRefund->SetText(FText::FromString(
			TEXT("구매·편성 후 당일 취소 및 환불은 불가합니다.")));
		NoRefund->SetAutoWrapText(true);
		StyleLightText(*NoRefund, 10, Palette->InGame.Amber, TEXT("SemiBold"), ETextJustify::Center);
		Confirm->SetIsEnabled(false);
		AddVertical(*Footer, *NoRefund, FMargin(0.0f, 0.0f, 0.0f, 5.0f));
		AddVertical(*Footer, *Confirm);
		FooterSurface->AddChild(Footer);
		AddVertical(*Shell, *FooterSurface);
		return MigratePurchaseControls(Blueprint, Tree) && HasPaidProgramPickerContract(Blueprint);
	}

	void EnsurePaidProgramBlueprint(
		const FString& PackageName,
		UClass& ParentClass,
		const TFunctionRef<bool(UWidgetBlueprint&, UWidgetTree&)> Build,
		const TFunctionRef<bool(const UWidgetBlueprint&)> Validate)
	{
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		if (FPackageName::DoesPackageExist(PackageName))
		{
			UWidgetBlueprint* Existing = LoadObject<UWidgetBlueprint>(
				nullptr, *FString::Printf(TEXT("%s.%s"), *PackageName, *AssetName));
			if (Existing && Existing->GeneratedClass &&
				Existing->GeneratedClass->IsChildOf(&ParentClass) && Validate(*Existing))
			{
				UE_LOG(LogFFFacilityInfoWbpBuilder, Display,
					TEXT("FF_FACILITY_PAID_PROGRAM_UMG_REUSE: %s"), *PackageName);
			}
			else
			{
				UE_LOG(LogFFFacilityInfoWbpBuilder, Error,
					TEXT("FF_FACILITY_PAID_PROGRAM_UMG_FAIL: existing asset contract mismatch: %s"),
					*PackageName);
			}
			return;
		}

		UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
		Factory->ParentClass = &ParentClass;
		FAssetToolsModule& AssetTools =
			FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(AssetTools.Get().CreateAsset(
			AssetName,
			FPackageName::GetLongPackagePath(PackageName),
			UWidgetBlueprint::StaticClass(),
			Factory));
		if (!Blueprint || !Blueprint->WidgetTree ||
			!Build(*Blueprint, *Blueprint->WidgetTree) || !Validate(*Blueprint) ||
			!SaveBlueprint(*Blueprint))
		{
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error,
				TEXT("FF_FACILITY_PAID_PROGRAM_UMG_FAIL: could not create %s"), *PackageName);
			return;
		}
		UE_LOG(LogFFFacilityInfoWbpBuilder, Display,
			TEXT("FF_FACILITY_PAID_PROGRAM_UMG_CREATED: %s"), *PackageName);
	}

	bool RefreshPaidProgramBlueprint(
		const FString& PackageName,
		UClass& ParentClass,
		const TFunctionRef<bool(UWidgetBlueprint&, UWidgetTree&)> Build,
		const TFunctionRef<bool(const UWidgetBlueprint&)> Validate)
	{
		if (!FPackageName::DoesPackageExist(PackageName))
		{
			EnsurePaidProgramBlueprint(PackageName, ParentClass, Build, Validate);
		}

		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(
			nullptr, *FString::Printf(TEXT("%s.%s"), *PackageName, *AssetName));
		if (!Blueprint || !Blueprint->GeneratedClass ||
			!Blueprint->GeneratedClass->IsChildOf(&ParentClass) || !Blueprint->WidgetTree)
		{
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error,
				TEXT("FF_FACILITY_PAID_PROGRAM_UMG_REFRESH_FAIL: parent/widget contract mismatch: %s"),
				*PackageName);
			return false;
		}

		UWidgetTree* OriginalTree = Blueprint->WidgetTree;
		const FName OriginalTreeName = OriginalTree->GetFName();
		const bool bOriginalTreeWasTransient = OriginalTree->HasAnyFlags(RF_Transient);
		const auto OriginalBindings = Blueprint->Bindings;
		const auto OriginalVariableMap = Blueprint->WidgetVariableNameToGuidMap;
		const FName DiscardedName = MakeUniqueObjectName(
			Blueprint, UWidgetTree::StaticClass(), TEXT("DiscardedWidgetTree"));
		if (!OriginalTree->Rename(
				*DiscardedName.ToString(),
				Blueprint,
				REN_DontCreateRedirectors | REN_NonTransactional))
		{
			return false;
		}

		Blueprint->Modify();
		OriginalTree->SetFlags(RF_Transient);
		Blueprint->Bindings.Reset();
		Blueprint->WidgetVariableNameToGuidMap.Reset();
		Blueprint->WidgetTree = NewObject<UWidgetTree>(
			Blueprint,
			UWidgetTree::StaticClass(),
			TEXT("WidgetTree"),
			RF_Transactional);

		auto Rollback = [&]()
		{
			if (UWidgetTree* FailedTree = Blueprint->WidgetTree)
			{
				const FName FailedName = MakeUniqueObjectName(
					Blueprint, UWidgetTree::StaticClass(), TEXT("FailedWidgetTree"));
				FailedTree->Rename(
					*FailedName.ToString(),
					Blueprint,
					REN_DontCreateRedirectors | REN_NonTransactional);
				FailedTree->SetFlags(RF_Transient);
			}

			if (!bOriginalTreeWasTransient)
			{
				OriginalTree->ClearFlags(RF_Transient);
			}
			OriginalTree->Rename(
				*OriginalTreeName.ToString(),
				Blueprint,
				REN_DontCreateRedirectors | REN_NonTransactional);
			Blueprint->WidgetTree = OriginalTree;
			Blueprint->Bindings = OriginalBindings;
			Blueprint->WidgetVariableNameToGuidMap = OriginalVariableMap;
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			FKismetEditorUtilities::CompileBlueprint(Blueprint);
		};

		if (!Blueprint->WidgetTree ||
			!Build(*Blueprint, *Blueprint->WidgetTree) ||
			!Validate(*Blueprint) ||
			!SaveBlueprint(*Blueprint))
		{
			Rollback();
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error,
				TEXT("FF_FACILITY_PAID_PROGRAM_UMG_REFRESH_FAIL: restored previous WidgetTree: %s"),
				*PackageName);
			return false;
		}

		UE_LOG(LogFFFacilityInfoWbpBuilder, Display,
			TEXT("FF_FACILITY_PAID_PROGRAM_UMG_REFRESHED: LightTeal layout saved: %s"),
			*PackageName);
		return true;
	}

	bool RefreshPaidProgramWidgetBlueprints()
	{
		const bool bCardRefreshed = RefreshPaidProgramBlueprint(
			PaidProgramCardPackage,
			*UFF_FacilityPaidProgramCard::StaticClass(),
			BuildPaidProgramCardLayout,
			HasPaidProgramCardContract);
		const bool bPickerRefreshed = RefreshPaidProgramBlueprint(
			PaidProgramPickerPackage,
			*UFF_FacilityPaidProgramPicker::StaticClass(),
			BuildPaidProgramPickerLayout,
			HasPaidProgramPickerContract);
		return bCardRefreshed && bPickerRefreshed;
	}

	void RefreshFacilityPaidProgramUmg()
	{
		RefreshPaidProgramWidgetBlueprints();
	}

	void BuildPaidProgramWidgetBlueprints()
	{
		EnsurePaidProgramBlueprint(
			PaidProgramCardPackage,
			*UFF_FacilityPaidProgramCard::StaticClass(),
			BuildPaidProgramCardLayout,
			HasPaidProgramCardContract);
		EnsurePaidProgramBlueprint(
			PaidProgramPickerPackage,
			*UFF_FacilityPaidProgramPicker::StaticClass(),
			BuildPaidProgramPickerLayout,
			HasPaidProgramPickerContract);
	}

	void BuildFacilityInfoUmg()
	{
		if (!EnsureLightingControlsBlueprint()) return;
		BuildPaidProgramWidgetBlueprints();
		// Never rebuild an existing designer-owned asset. A versioned contract migration may append
		// missing named nodes at the known Panel_Content seam and normalize the upgrade block order.
		// Existing designer nodes, their style/graph data, and their own slot properties stay intact.
		if (FPackageName::DoesPackageExist(PanelPackage))
		{
			UWidgetBlueprint* Existing = LoadExistingBlueprint();
			if (Existing && Existing->GeneratedClass &&
				Existing->GeneratedClass->IsChildOf(UFF_FacilityInfoPanel::StaticClass()) &&
				HasRequiredLayout(*Existing))
			{
				bool bMigratedProgramContract = false;
				bool bMigratedInstanceLimitContract = false;
				bool bMigratedUpgradeContract = false;
				bool bMigratedLightingContract = false;
				bool bMigratedRevenueContract = false;
				if (!EnsureProgramWidgetContract(
						*Existing,
						*Existing->WidgetTree,
						bMigratedProgramContract) ||
					!HasProgramWidgetContract(*Existing) ||
					!EnsureInstanceLimitWidgetContract(
						*Existing,
						*Existing->WidgetTree,
						bMigratedInstanceLimitContract) ||
					!HasInstanceLimitWidgetContract(*Existing) ||
					!EnsureUpgradeWidgetContract(*Existing, *Existing->WidgetTree, bMigratedUpgradeContract) ||
					!HasUpgradeWidgetContract(*Existing) ||
					!EnsureLightingWidgetContract(*Existing, *Existing->WidgetTree, bMigratedLightingContract) ||
					!EnsureRevenueWidgetContract(*Existing, *Existing->WidgetTree, bMigratedRevenueContract) ||
					!HasRevenueWidgetContract(*Existing))
				{
					UE_LOG(LogFFFacilityInfoWbpBuilder, Error,
						TEXT("FF_FACILITY_INFO_UMG_FAIL: existing asset cannot accept a non-destructive widget contract migration: %s"),
						*PanelPackage);
					return;
				}
				const bool bMigratedAnyContract = bMigratedProgramContract || bMigratedInstanceLimitContract ||
					bMigratedUpgradeContract || bMigratedLightingContract || bMigratedRevenueContract;
				if (bMigratedAnyContract && !SaveBlueprint(*Existing))
				{
					UE_LOG(LogFFFacilityInfoWbpBuilder, Error,
						TEXT("FF_FACILITY_INFO_UMG_FAIL: could not save migrated asset: %s"),
						*PanelPackage);
					return;
				}

				if (bMigratedAnyContract)
				{
					UE_LOG(LogFFFacilityInfoWbpBuilder, Display,
						TEXT("FF_FACILITY_INFO_UMG_MIGRATED: %s (existing designer nodes preserved)"),
						*PanelPackage);
				}
				else
				{
					UE_LOG(LogFFFacilityInfoWbpBuilder, Display,
						TEXT("FF_FACILITY_INFO_UMG_REUSE: %s (designer changes preserved)"),
						*PanelPackage);
				}
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
			!HasProgramWidgetContract(*Blueprint) ||
			!HasInstanceLimitWidgetContract(*Blueprint) ||
			!HasUpgradeWidgetContract(*Blueprint) ||
			!HasRevenueWidgetContract(*Blueprint) ||
			!SaveBlueprint(*Blueprint))
		{
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error,
				TEXT("FF_FACILITY_INFO_UMG_FAIL: could not create %s"), *PanelPackage);
			return;
		}

		UE_LOG(LogFFFacilityInfoWbpBuilder, Display,
			TEXT("FF_FACILITY_INFO_UMG_CREATED: %s"), *PanelPackage);
	}

	bool RefreshFacilityInfoWidgetTree(UWidgetBlueprint& Blueprint)
	{
		UWidgetTree* OriginalTree = Blueprint.WidgetTree;
		if (!OriginalTree)
		{
			return false;
		}

		const FName OriginalTreeName = OriginalTree->GetFName();
		const bool bOriginalTreeWasTransient = OriginalTree->HasAnyFlags(RF_Transient);
		const auto OriginalBindings = Blueprint.Bindings;
		const auto OriginalVariableMap = Blueprint.WidgetVariableNameToGuidMap;
		const FName DiscardedName = MakeUniqueObjectName(
			&Blueprint, UWidgetTree::StaticClass(), TEXT("DiscardedWidgetTree"));
		if (!OriginalTree->Rename(
				*DiscardedName.ToString(),
				&Blueprint,
				REN_DontCreateRedirectors | REN_NonTransactional))
		{
			return false;
		}

		Blueprint.Modify();
		OriginalTree->SetFlags(RF_Transient);
		Blueprint.Bindings.Reset();
		Blueprint.WidgetVariableNameToGuidMap.Reset();
		Blueprint.WidgetTree = NewObject<UWidgetTree>(
			&Blueprint,
			UWidgetTree::StaticClass(),
			TEXT("WidgetTree"),
			RF_Transactional);

		auto Rollback = [&]()
		{
			if (UWidgetTree* FailedTree = Blueprint.WidgetTree)
			{
				const FName FailedName = MakeUniqueObjectName(
					&Blueprint, UWidgetTree::StaticClass(), TEXT("FailedWidgetTree"));
				FailedTree->Rename(
					*FailedName.ToString(),
					&Blueprint,
					REN_DontCreateRedirectors | REN_NonTransactional);
				FailedTree->SetFlags(RF_Transient);
			}

			if (!bOriginalTreeWasTransient)
			{
				OriginalTree->ClearFlags(RF_Transient);
			}
			OriginalTree->Rename(
				*OriginalTreeName.ToString(),
				&Blueprint,
				REN_DontCreateRedirectors | REN_NonTransactional);
			Blueprint.WidgetTree = OriginalTree;
			Blueprint.Bindings = OriginalBindings;
			Blueprint.WidgetVariableNameToGuidMap = OriginalVariableMap;
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&Blueprint);
			FKismetEditorUtilities::CompileBlueprint(&Blueprint);
		};

		if (!Blueprint.WidgetTree ||
			!BuildLayout(Blueprint, *Blueprint.WidgetTree) ||
			!HasRequiredLayout(Blueprint) ||
			!HasProgramWidgetContract(Blueprint) ||
			!HasInstanceLimitWidgetContract(Blueprint) ||
			!HasUpgradeWidgetContract(Blueprint) ||
			!HasRevenueWidgetContract(Blueprint) ||
			!SaveBlueprint(Blueprint))
		{
			Rollback();
			return false;
		}
		return true;
	}

	void RefreshFacilityInfoUmg()
	{
		if (!RefreshPaidProgramWidgetBlueprints())
		{
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error,
				TEXT("FF_FACILITY_INFO_UMG_REFRESH_FAIL: paid-program WBP refresh failed."));
			return;
		}

		if (!FPackageName::DoesPackageExist(PanelPackage))
		{
			BuildFacilityInfoUmg();
			return;
		}

		UWidgetBlueprint* Existing = LoadExistingBlueprint();
		if (!Existing || !Existing->GeneratedClass ||
			!Existing->GeneratedClass->IsChildOf(UFF_FacilityInfoPanel::StaticClass()))
		{
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error,
				TEXT("FF_FACILITY_INFO_UMG_REFRESH_FAIL: parent contract mismatch: %s"),
				*PanelPackage);
			return;
		}

		if (!RefreshFacilityInfoWidgetTree(*Existing))
		{
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error,
				TEXT("FF_FACILITY_INFO_UMG_REFRESH_FAIL: restored previous WidgetTree: %s"),
				*PanelPackage);
			return;
		}

		UE_LOG(LogFFFacilityInfoWbpBuilder, Display,
			TEXT("FF_FACILITY_INFO_UMG_REFRESHED: LightTeal designer layout saved: %s"),
			*PanelPackage);
	}

	void RefreshFacilityInfoGameUmg()
	{
		// Explicitly replaces only this panel. The existing create command continues to
		// preserve designer assets, and paid-program layouts are outside this refresh.
		UWidgetBlueprint* Existing = LoadExistingBlueprint();
		if (!Existing || !Existing->GeneratedClass ||
			!Existing->GeneratedClass->IsChildOf(UFF_FacilityInfoPanel::StaticClass()) ||
			!RefreshFacilityInfoWidgetTree(*Existing))
		{
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error,
				TEXT("FF_FACILITY_INFO_GAME_UMG_FAIL: panel-only refresh failed: %s"), *PanelPackage);
			return;
		}
		UE_LOG(LogFFFacilityInfoWbpBuilder, Display,
			TEXT("FF_FACILITY_INFO_GAME_UMG_REFRESHED: %s"), *PanelPackage);
	}

	void SetApprovedDock(UWidget& Widget, const bool bShadow = false)
	{
		if (USizeBox* Size = Cast<USizeBox>(&Widget))
		{
			Size->SetWidthOverride(PanelWidth);
			Size->ClearHeightOverride();
		}
		if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget.Slot))
		{
			FFFacilityPaidProgramLayout::Apply(*Slot, FFFacilityPaidProgramLayout::EVariant::InspectorDock);
			if (bShadow)
			{
				FMargin Offsets = Slot->GetOffsets();
				Offsets.Left += 4.0f;
				Offsets.Top += 4.0f;
				Offsets.Bottom -= 4.0f;
				Slot->SetOffsets(Offsets);
			}
		}
	}

	bool MigrateInspectorControls(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		// Presence is a version marker. Repeat execution never repositions or repaints authored nodes.
		if (Tree.FindWidget(TEXT("Panel_InspectorNavigation")))
		{
			return HasNamedVariable(Blueprint, TEXT("Button_TabOverview"), *UButton::StaticClass()) &&
				HasNamedVariable(Blueprint, TEXT("Button_TabOperation"), *UButton::StaticClass()) &&
				HasNamedVariable(Blueprint, TEXT("Button_TabUpgrade"), *UButton::StaticClass()) &&
				HasNamedVariable(Blueprint, TEXT("Button_Favorite"), *UButton::StaticClass()) &&
				HasNamedVariable(Blueprint, TEXT("Button_RemoveConfirm"), *UButton::StaticClass());
		}
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		UVerticalBox* Shell = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Panel_Shell")));
		UVerticalBox* Content = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Panel_Content")));
		if (!Palette || !Shell || !Content) return false;
		UCommonTextBlock* LevelSummary = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_LevelSummary"), true);
		UVerticalBox* Identity = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Summary_Meta")));
		if (!LevelSummary || !Identity) return false;
		LevelSummary->SetText(FText::FromString(TEXT("시설 강화 · Lv.1")));
		StyleLightText(*LevelSummary, 12, Palette->InGame.TealDeep, TEXT("SemiBold"));
		AddVertical(*Identity, *LevelSummary, FMargin(0.0f, 5.0f, 0.0f, 0.0f));
		UVerticalBox* Navigation = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_InspectorNavigation"));
		UHorizontalBox* Tabs = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Panel_InspectorTabs"));
		UButton* Favorite = MakeLightTextButton(Blueprint, Tree,
			TEXT("Button_Favorite"), TEXT("Text_Favorite"), FText::FromString(TEXT("☆ 즐겨찾기")),
			Palette->InGame.Paper, Palette->InGame.TealSoft, Palette->InGame.TealSoft,
			Palette->InGame.Line, Palette->InGame.TealDeep, true, true);
		if (!Navigation || !Tabs || !Favorite) return false;
		AddVertical(*Navigation, *Favorite, FMargin(10.0f, 7.0f, 10.0f, 5.0f));
		const TCHAR* Names[] = {TEXT("Overview"), TEXT("Operation"), TEXT("Upgrade")};
		const TCHAR* Labels[] = {TEXT("시설 정보"), TEXT("운영"), TEXT("강화")};
		for (int32 Index = 0; Index < 3; ++Index)
		{
			UButton* Tab = MakeLightTextButton(Blueprint, Tree,
				FName(*FString::Printf(TEXT("Button_Tab%s"), Names[Index])),
				FName(*FString::Printf(TEXT("Text_Tab%s"), Names[Index])), FText::FromString(Labels[Index]),
				Palette->InGame.PaperSunken, Palette->InGame.TealSoft, Palette->InGame.TealSoft,
				Palette->InGame.Line, Palette->InGame.TealDeep, true, false);
			if (!Tab) return false;
			AddHorizontal(*Tabs, *Tab, FMargin(2.0f, 0.0f), ESlateSizeRule::Fill);
		}
		AddVertical(*Navigation, *Tabs, FMargin(8.0f, 0.0f, 8.0f, 6.0f));
		Shell->InsertChildAt(1, Navigation);

		UVerticalBox* Confirmation = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_RemoveConfirmation"), true);
		UCommonTextBlock* Copy = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_RemoveConfirmation"), true);
		UButton* Confirm = MakeLightTextButton(Blueprint, Tree,
			TEXT("Button_RemoveConfirm"), TEXT("Text_RemoveConfirm"), FText::FromString(TEXT("회수 금액을 받고 철거")),
			Palette->InGame.Danger, Palette->InGame.Danger, Palette->InGame.Danger,
			Palette->InGame.Danger, Palette->InGame.White, true, false);
		UButton* Cancel = MakeLightTextButton(Blueprint, Tree,
			TEXT("Button_RemoveCancel"), TEXT("Text_RemoveCancel"), FText::FromString(TEXT("시설 유지")),
			Palette->InGame.Paper, Palette->InGame.TealSoft, Palette->InGame.TealSoft,
			Palette->InGame.Line, Palette->InGame.TealDeep, true, false);
		if (!Confirmation || !Copy || !Confirm || !Cancel) return false;
		Copy->SetText(FText::FromString(TEXT("시설을 철거할까요?\n강화에 사용한 비용은 환불되지 않습니다.")));
		Copy->SetAutoWrapText(true);
		StyleLightText(*Copy, 14, Palette->InGame.Ink, TEXT("Medium"));
		AddVertical(*Confirmation, *Copy, FMargin(6.0f, 12.0f, 6.0f, 16.0f));
		AddVertical(*Confirmation, *Cancel, FMargin(0.0f, 0.0f, 0.0f, 7.0f));
		AddVertical(*Confirmation, *Confirm);
		AddVertical(*Content, *Confirmation);
		Confirmation->SetVisibility(ESlateVisibility::Collapsed);
		if (UWidget* PanelSize = Tree.FindWidget(TEXT("Panel_Size"))) SetApprovedDock(*PanelSize);
		if (UWidget* Shadow = Tree.FindWidget(TEXT("Panel_ShadowSize"))) SetApprovedDock(*Shadow, true);
		// Show only the initial overview in Designer. Runtime projects state into these same nodes.
		for (const FName Name : {FName(TEXT("Panel_UpgradeSection")), FName(TEXT("Panel_LiveSection"))})
			if (UWidget* Section = Tree.FindWidget(Name)) Section->SetVisibility(ESlateVisibility::Collapsed);
		return true;
	}

	bool MigratePurchaseControls(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		if (Tree.FindWidget(TEXT("Panel_PurchaseComplete")))
		{
			return HasNamedVariable(Blueprint, TEXT("Text_PurchaseReceipt"), *UCommonTextBlock::StaticClass()) &&
				HasNamedVariable(Blueprint, TEXT("Button_ReturnToFacility"), *UButton::StaticClass()) &&
				HasNamedVariable(Blueprint, TEXT("Text_ResourcePreview"), *UCommonTextBlock::StaticClass()) &&
				HasNamedVariable(Blueprint, TEXT("Text_PreviewTime"), *UCommonTextBlock::StaticClass()) &&
				HasNamedVariable(Blueprint, TEXT("Text_Capacity"), *UCommonTextBlock::StaticClass()) &&
				HasNamedVariable(Blueprint, TEXT("Text_TicketFee"), *UCommonTextBlock::StaticClass()) &&
				HasNamedVariable(Blueprint, TEXT("Text_MaxRevenue"), *UCommonTextBlock::StaticClass()) &&
				HasNamedVariable(Blueprint, TEXT("Text_PurchaseCost"), *UCommonTextBlock::StaticClass());
		}
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		UVerticalBox* Shell = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Panel_PickerShell")));
		UVerticalBox* Body = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Panel_PickerContent")));
		UVerticalBox* Preview = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Panel_Preview")));
		UVerticalBox* Footer = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Panel_Footer")));
		UBorder* BodySurface = Cast<UBorder>(Tree.FindWidget(TEXT("Border_Body")));
		UWidget* FooterSurface = Tree.FindWidget(TEXT("Border_Footer"));
		UBorder* ListSurface = Cast<UBorder>(Tree.FindWidget(TEXT("Border_ProgramList")));
		if (!Palette || !Shell || !Body || !BodySurface || !FooterSurface || !ListSurface || !Preview || !Footer) return false;
		bool bModified = false;
		EnsureVariableContract(Blueprint, *BodySurface, TEXT("Border_Body"), bModified);
		EnsureVariableContract(Blueprint, *FooterSurface, TEXT("Border_Footer"), bModified);
		UCommonTextBlock* Resources = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_ResourcePreview"), true);
		UVerticalBox* Complete = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_PurchaseComplete"), true);
		UCommonTextBlock* Title = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_PurchaseCompleteTitle"));
		UCommonTextBlock* Receipt = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_PurchaseReceipt"), true);
		UCommonTextBlock* ReceiptName = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_ReceiptProgramName"), true);
		UCommonTextBlock* ReceiptResources = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_ReceiptResources"), true);
		UCommonTextBlock* Time = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_PreviewTime"), true);
		UCommonTextBlock* MaxRevenue = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_MaxRevenue"), true);
		UCommonTextBlock* PurchaseCost = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_PurchaseCost"), true);
		UCommonTextBlock* PurchaseLabel = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_PurchaseCostLabel"));
		UHorizontalBox* Metrics = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Panel_ProgramMetrics"));
		UHorizontalBox* CostRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Panel_PurchaseCost"));
		UButton* Return = MakeLightTextButton(Blueprint, Tree,
			TEXT("Button_ReturnToFacility"), TEXT("Text_ReturnToFacility"), FText::FromString(TEXT("시설 정보로 돌아가기")),
			Palette->InGame.TealDeep, Palette->InGame.Teal, Palette->InGame.Teal,
			Palette->InGame.TealDeep, Palette->InGame.White, true, false);
		UScrollBox* Details = MakeWidget<UScrollBox>(Blueprint, Tree, TEXT("ScrollBox_PurchaseDetails"));
		USizeBox* ListSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Size_ProgramList"));
		if (!Resources || !Complete || !Title || !Receipt || !Return || !Details || !ListSize ||
			!ReceiptName || !ReceiptResources || !Time || !MaxRevenue || !PurchaseCost || !PurchaseLabel || !Metrics || !CostRow) return false;
		for (const FName Name : {FName(TEXT("Text_PreviewDetails")), FName(TEXT("Text_PreviewDescription"))})
			if (UWidget* LegacyText = Tree.FindWidget(Name)) LegacyText->SetVisibility(ESlateVisibility::Collapsed);
		if (UCommonTextBlock* PreviewName = Cast<UCommonTextBlock>(Tree.FindWidget(TEXT("Text_PreviewName"))))
			StyleLightText(*PreviewName, 21, Palette->InGame.TealDeep, TEXT("SemiBold"));
		Time->SetText(FText::FromString(TEXT("개장 후 운영 시간")));
		Time->SetAutoWrapText(true);
		StyleLightText(*Time, 13, Palette->InGame.InkMuted, TEXT("Medium"));
		Preview->InsertChildAt(1, Time);
		// 프로그램은 인력을 쓰지 않아 `필요 인력` 칸을 뺐다.
		const FName ValueNames[] = {TEXT("Text_Capacity"), TEXT("Text_TicketFee")};
		const TCHAR* MetricLabels[] = {TEXT("참가 정원"), TEXT("1인 참가비")};
		for (int32 Index = 0; Index < 2; ++Index)
		{
			UBorder* Metric = BuildStatCard(Blueprint, Tree, *Palette, ValueNames[Index], MetricLabels[Index]);
			if (!Metric) return false;
			AddHorizontal(*Metrics, *Metric, FMargin(Index == 0 ? 0.0f : 3.0f, 0.0f,
				Index == 1 ? 0.0f : 3.0f, 0.0f), ESlateSizeRule::Fill);
		}
		if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Preview->InsertChildAt(2, Metrics)))
			Slot->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 10.0f));
		MaxRevenue->SetText(FText::FromString(TEXT("최대 매출 · 정원 모두 참여 시")));
		MaxRevenue->SetAutoWrapText(true);
		StyleLightText(*MaxRevenue, 13, Palette->InGame.TealDeep, TEXT("Medium"));
		if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Preview->InsertChildAt(3, MaxRevenue)))
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
		PurchaseLabel->SetText(FText::FromString(TEXT("구매 비용")));
		StyleLightText(*PurchaseLabel, 12, Palette->InGame.InkMuted, TEXT("Medium"));
		PurchaseCost->SetText(FText::FromString(TEXT("—")));
		StyleLightText(*PurchaseCost, 26, Palette->InGame.TealDeep, TEXT("SemiBold"));
		AddHorizontal(*CostRow, *PurchaseLabel, FMargin(0.0f), ESlateSizeRule::Fill);
		AddHorizontal(*CostRow, *PurchaseCost);
		if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Footer->InsertChildAt(0, CostRow)))
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		Resources->SetText(FText::FromString(TEXT("보유금")));
		Resources->SetAutoWrapText(true);
		StyleLightText(*Resources, 13, Palette->InGame.TealDeep, TEXT("SemiBold"));
		AddVertical(*Body, *Resources, FMargin(2.0f, 10.0f));
		// The entire review scrolls at short viewports; confirmation remains in its fixed footer.
		const int32 ListIndex = Body->GetChildIndex(ListSurface);
		ListSurface->RemoveFromParent();
		ListSize->SetMaxDesiredHeight(145.0f);
		ListSize->AddChild(ListSurface);
		Body->InsertChildAt(ListIndex, ListSize);
		Body->RemoveFromParent();
		Details->SetAnimateWheelScrolling(true);
		Details->AddChild(Body);
		BodySurface->AddChild(Details);
		Title->SetText(FText::FromString(TEXT("구매·편성 완료")));
		StyleLightText(*Title, 14, Palette->InGame.TealDeep, TEXT("SemiBold"));
		ReceiptName->SetText(FText::FromString(TEXT("프로그램명")));
		ReceiptName->SetAutoWrapText(true);
		StyleLightText(*ReceiptName, 24, Palette->InGame.TealDeep, TEXT("SemiBold"));
		ReceiptResources->SetText(FText::FromString(TEXT("남은 보유금")));
		ReceiptResources->SetAutoWrapText(true);
		StyleLightText(*ReceiptResources, 16, Palette->InGame.TealDeep, TEXT("SemiBold"));
		Receipt->SetText(FText::FromString(TEXT("프로그램 구매가 완료되었습니다.")));
		Receipt->SetAutoWrapText(true);
		StyleLightText(*Receipt, 14, Palette->InGame.Ink, TEXT("Medium"));
		AddVertical(*Complete, *Title, FMargin(18.0f, 24.0f, 18.0f, 8.0f));
		AddVertical(*Complete, *ReceiptName, FMargin(18.0f, 0.0f, 18.0f, 18.0f));
		UScrollBox* ReceiptScroll = MakeWidget<UScrollBox>(Blueprint, Tree, TEXT("ScrollBox_PurchaseReceipt"));
		if (!ReceiptScroll) return false;
		ReceiptScroll->AddChild(Receipt);
		AddVertical(*Complete, *ReceiptScroll, FMargin(18.0f, 0.0f, 18.0f, 18.0f), ESlateSizeRule::Fill, VAlign_Fill);
		AddVertical(*Complete, *ReceiptResources, FMargin(18.0f, 0.0f, 18.0f, 12.0f));
		AddVertical(*Complete, *Return, FMargin(12.0f, 8.0f, 12.0f, 12.0f));
		AddVertical(*Shell, *Complete, FMargin(0.0f), ESlateSizeRule::Fill, VAlign_Fill);
		Complete->SetVisibility(ESlateVisibility::Collapsed);
		if (UWidget* Picker = Tree.FindWidget(TEXT("Border_Picker"))) SetApprovedDock(*Picker);
		return true;
	}

	void MigrateFacilityInspectorUmg()
	{
		// This is the safe approved-layout command. Legacy Refresh commands explicitly rebuild.
		BuildFacilityInfoUmg();
		UWidgetBlueprint* Inspector = LoadExistingBlueprint();
		const FString PickerName = FPackageName::GetLongPackageAssetName(PaidProgramPickerPackage);
		UWidgetBlueprint* Picker = LoadObject<UWidgetBlueprint>(nullptr,
			*FString::Printf(TEXT("%s.%s"), *PaidProgramPickerPackage, *PickerName));
		bool bSuccess = true;
		if (!Inspector || !Inspector->WidgetTree) bSuccess = false;
		else if (!Inspector->WidgetTree->FindWidget(TEXT("Panel_InspectorNavigation")))
			bSuccess &= MigrateInspectorControls(*Inspector, *Inspector->WidgetTree) && SaveBlueprint(*Inspector);
		else bSuccess &= MigrateInspectorControls(*Inspector, *Inspector->WidgetTree);
		if (!Picker || !Picker->WidgetTree) bSuccess = false;
		else if (!Picker->WidgetTree->FindWidget(TEXT("Panel_PurchaseComplete")))
			bSuccess &= MigratePurchaseControls(*Picker, *Picker->WidgetTree) && SaveBlueprint(*Picker);
		else bSuccess &= MigratePurchaseControls(*Picker, *Picker->WidgetTree);
		if (bSuccess)
		{
			UE_LOG(LogFFFacilityInfoWbpBuilder, Display,
				TEXT("FF_FACILITY_INSPECTOR_MIGRATED: editable inspector and purchase contracts ready; existing edits preserved."));
		}
		else
		{
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error,
				TEXT("FF_FACILITY_INSPECTOR_MIGRATION_FAIL: incompatible or partially authored contract; no destructive rebuild performed."));
		}
	}

	// 기존 시설 화면에서 폐기된 게임 시작 영역만 제거한다. 다른 페이지나 슬롯은 재생성하지 않는다.
	void RemoveFacilityMinigameUI()
	{
		UWidgetBlueprint* Inspector = LoadExistingBlueprint();
		if (!Inspector || !Inspector->WidgetTree || !Inspector->GeneratedClass ||
			!Inspector->GeneratedClass->IsChildOf(UFF_FacilityInfoPanel::StaticClass()))
		{
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error,
				TEXT("FF_FACILITY_MINIGAME_REMOVE_FAIL: inspector or native parent contract is invalid; no save."));
			return;
		}

		UWidgetTree& Tree = *Inspector->WidgetTree;
		const TSet<FName> RemovedNames = {
			TEXT("Panel_LiveActions"), TEXT("Button_StartMinigame"), TEXT("Text_StartMinigame")};
		UWidget* ActionsWidget = Tree.FindWidget(TEXT("Panel_LiveActions"));
		UWidget* ButtonWidget = Tree.FindWidget(TEXT("Button_StartMinigame"));
		UWidget* TextWidget = Tree.FindWidget(TEXT("Text_StartMinigame"));
		if (!ActionsWidget && !ButtonWidget && !TextWidget)
		{
			for (const FName Name : RemovedNames)
			{
				if (Inspector->WidgetVariableNameToGuidMap.Contains(Name))
				{
					UE_LOG(LogFFFacilityInfoWbpBuilder, Error,
						TEXT("FF_FACILITY_MINIGAME_REMOVE_FAIL: orphan variable %s; no save."), *Name.ToString());
					return;
				}
			}
			UE_LOG(LogFFFacilityInfoWbpBuilder, Display,
				TEXT("FF_FACILITY_MINIGAME_REMOVE_REUSE: no retired widgets; no save."));
			return;
		}

		// 저장된 전용 영역이 예상한 세 위젯일 때만 진행한다. 디자이너가 다른 내용을 넣었다면 중단한다.
		UVerticalBox* Actions = Cast<UVerticalBox>(ActionsWidget);
		UButton* Button = Cast<UButton>(ButtonWidget);
		UCommonTextBlock* Label = Cast<UCommonTextBlock>(TextWidget);
		if (!Actions || !Button || !Label || !Actions->GetParent() ||
			Actions->GetChildrenCount() != 1 || Actions->GetChildAt(0) != Button || Button->GetContent() != Label ||
			Actions->GetOuter() != &Tree || Button->GetOuter() != &Tree || Label->GetOuter() != &Tree)
		{
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error,
				TEXT("FF_FACILITY_MINIGAME_REMOVE_FAIL: retired subtree differs from the three-widget contract; no save."));
			return;
		}

		TArray<UWidget*> BeforeWidgets;
		Tree.GetAllWidgets(BeforeWidgets);
		TMap<UWidget*, UPanelSlot*> PreservedSlots;
		for (UWidget* Widget : BeforeWidgets)
		{
			if (!RemovedNames.Contains(Widget->GetFName())) PreservedSlots.Add(Widget, Widget->Slot);
		}
		Inspector->Modify();
		Inspector->Bindings.RemoveAll([&RemovedNames](const FDelegateEditorBinding& Binding)
		{
			return RemovedNames.Contains(FName(*Binding.ObjectName));
		});
		// UE 편집기의 삭제 절차는 RemoveWidget, 변수/GUID 정리, 자식의 transient 패키지 이동을 함께 수행한다.
		// 단순 RemoveFromParent와 달리 저장 패키지에 제거된 위젯 객체가 고아로 남지 않는다.
		FWidgetBlueprintEditorUtils::DeleteWidgets(Inspector, TSet<UWidget*>{Actions},
			FWidgetBlueprintEditorUtils::EDeleteWidgetWarningType::DeleteSilently);

		TArray<UWidget*> AfterWidgets;
		Tree.GetAllWidgets(AfterWidgets);
		bool bPreserved = AfterWidgets.Num() == BeforeWidgets.Num() - RemovedNames.Num();
		for (const TPair<UWidget*, UPanelSlot*>& Entry : PreservedSlots)
		{
			bPreserved &= AfterWidgets.Contains(Entry.Key) && Entry.Key->Slot == Entry.Value;
		}
		for (const FName Name : RemovedNames)
		{
			bPreserved &= !Tree.FindWidget(Name) && !Inspector->WidgetVariableNameToGuidMap.Contains(Name);
		}
		for (UWidget* Removed : {ActionsWidget, ButtonWidget, TextWidget})
		{
			bPreserved &= Removed->GetOutermost() == GetTransientPackage();
		}
		if (!bPreserved || !SaveBlueprint(*Inspector))
		{
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error,
				TEXT("FF_FACILITY_MINIGAME_REMOVE_FAIL: preserved widget/slot, compile, or save validation failed."));
			return;
		}
		UE_LOG(LogFFFacilityInfoWbpBuilder, Display,
			TEXT("FF_FACILITY_MINIGAME_REMOVE_SAVED: removed three widgets; other %d widgets and slots retained."),
			AfterWidgets.Num());
	}

	FAutoConsoleCommand RemoveFacilityMinigameUICommand(
		TEXT("FF.RemoveFacilityMinigameUI"),
		TEXT("Removes only the retired three-widget minigame entry from the existing facility inspector; repeats do not save."),
		FConsoleCommandDelegate::CreateStatic(&RemoveFacilityMinigameUI));

	void MigrateFacilityRevenueSection()
	{
		UWidgetBlueprint* Inspector = LoadExistingBlueprint();
		if (!Inspector || !Inspector->WidgetTree || !Inspector->GeneratedClass ||
			!Inspector->GeneratedClass->IsChildOf(UFF_FacilityInfoPanel::StaticClass()))
		{
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error,
				TEXT("FF_FACILITY_REVENUE_SECTION_FAIL: installed-facility inspector is missing or has a different native parent."));
			return;
		}
		bool bAdded = false;
		if (!EnsureRevenueWidgetContract(*Inspector, *Inspector->WidgetTree, bAdded) ||
			(bAdded && !SaveBlueprint(*Inspector)))
		{
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error,
				TEXT("FF_FACILITY_REVENUE_SECTION_FAIL: incompatible inspector seam or save failure; no tree reset performed."));
			return;
		}
		UE_LOG(LogFFFacilityInfoWbpBuilder, Display,
			TEXT("FF_FACILITY_REVENUE_SECTION_%s: %s; existing Designer widgets, slots and events preserved."),
			bAdded ? TEXT("ADDED") : TEXT("REUSE"), *PanelPackage);
	}

	FAutoConsoleCommand MigrateFacilityRevenueSectionCommand(
		TEXT("FF.MigrateFacilityRevenueSection"),
		TEXT("Adds the editable per-facility revenue card (today/total earnings, payment count, share) after the upgrade block; reruns preserve Designer edits."),
		FConsoleCommandDelegate::CreateStatic(&MigrateFacilityRevenueSection));

	void MigrateFacilitySynergyTab()
	{
		if (!EnsureSynergyEntryBlueprint()) return;
		UWidgetBlueprint* Inspector = LoadExistingBlueprint();
		if (!Inspector || !Inspector->WidgetTree || !Inspector->GeneratedClass ||
			!Inspector->GeneratedClass->IsChildOf(UFF_FacilityInfoPanel::StaticClass()))
		{
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error, TEXT("FF_FACILITY_SYNERGY_TAB_FAIL: installed-facility inspector is missing or has a different native parent."));
			return;
		}
		bool bAdded = false;
		if (!EnsureSynergyInspectorContract(*Inspector, *Inspector->WidgetTree, bAdded) ||
			(bAdded && !SaveBlueprint(*Inspector)))
		{
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error, TEXT("FF_FACILITY_SYNERGY_TAB_FAIL: incompatible inspector seam or save failure; no tree reset performed."));
			return;
		}
		UE_LOG(LogFFFacilityInfoWbpBuilder, Display,
			TEXT("FF_FACILITY_SYNERGY_TAB_%s: %s; existing Designer widgets, slots and events preserved."),
			bAdded ? TEXT("ADDED") : TEXT("REUSE"), *PanelPackage);
	}

	FAutoConsoleCommand MigrateFacilitySynergyTabCommand(
		TEXT("FF.MigrateFacilitySynergyTab"),
		TEXT("Adds the editable synergy tab immediately after facility info and creates its row WBP; reruns preserve Designer edits."),
		FConsoleCommandDelegate::CreateStatic(&MigrateFacilitySynergyTab));

	void MigrateFacilityStaffTab()
	{
		UWidgetBlueprint* Inspector = LoadExistingBlueprint();
		bool bAdded = false;
		if (!Inspector || !Inspector->WidgetTree || !Inspector->GeneratedClass ||
			!Inspector->GeneratedClass->IsChildOf(UFF_FacilityInfoPanel::StaticClass()) ||
			!EnsureStaffWidgetContract(*Inspector, *Inspector->WidgetTree, bAdded) ||
			(bAdded && !SaveBlueprint(*Inspector)))
		{
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error, TEXT("FF_FACILITY_STAFF_TAB_FAIL: incompatible inspector or save failure; no tree reset performed."));
			return;
		}
		UE_LOG(LogFFFacilityInfoWbpBuilder, Display, TEXT("FF_FACILITY_STAFF_TAB_%s: %s; existing Designer widgets preserved."),
			bAdded ? TEXT("ADDED") : TEXT("REUSE"), *PanelPackage);
	}

	FAutoConsoleCommand MigrateFacilityStaffTabCommand(
		TEXT("FF.MigrateFacilityStaffTab"),
		TEXT("Adds the staff tab after Operation and an editable assignment page; reruns preserve Designer edits."),
		FConsoleCommandDelegate::CreateStatic(&MigrateFacilityStaffTab));

	void MigrateFacilitySynergyPartnerLink()
	{
		UWidgetBlueprint* Entry = FFUIWbpBuilder::LoadExistingBlueprint(SynergyEntryPackage);
		if (!Entry || !Entry->WidgetTree || !Entry->ParentClass ||
			!Entry->ParentClass->IsChildOf(UFF_FacilitySynergyEntryWidget::StaticClass()) || !HasSynergyEntryContract(*Entry))
		{
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error, TEXT("FF_FACILITY_SYNERGY_LINK_FAIL: existing row contract is missing/incompatible; asset preserved."));
			return;
		}
		bool bAdded = false;
		if (!EnsureSynergyPartnerLink(*Entry, *Entry->WidgetTree, bAdded) || (bAdded && !SaveBlueprint(*Entry)))
		{
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error, TEXT("FF_FACILITY_SYNERGY_LINK_FAIL: partner link or ancestor input contract incompatible; no tree reset performed."));
			return;
		}
		UE_LOG(LogFFFacilityInfoWbpBuilder, Display, TEXT("FF_FACILITY_SYNERGY_LINK_%s: %s; text styling and original parent slot retained."),
			bAdded ? TEXT("ADDED") : TEXT("REUSE"), *SynergyEntryPackage);
	}

	FAutoConsoleCommand MigrateFacilitySynergyPartnerLinkCommand(
		TEXT("FF.MigrateFacilitySynergyPartnerLink"),
		TEXT("Makes the synergy partner name open its build card; retains text/style/slot and never resaves on repeat."),
		FConsoleCommandDelegate::CreateStatic(&MigrateFacilitySynergyPartnerLink));

	FAutoConsoleCommand MigrateFacilityInspectorUmgCommand(
		TEXT("FF.MigrateFacilityInspectorUMG"),
		TEXT("Adds approved inspector tabs, favorites, remove confirmation and paid-program receipt once; reruns preserve designer changes."),
		FConsoleCommandDelegate::CreateStatic(&MigrateFacilityInspectorUmg));

	bool ApplyInspectorV2Design(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, bool& bOutModified)
	{
		bOutModified = false;
		// The named surface is a version marker. Never repaint/reparent a designer's V2 edits.
		if (Tree.FindWidget(TEXT("Panel_PersistentIdentity")))
			return HasNamedVariable(Blueprint, TEXT("Panel_PersistentIdentity"), *UBorder::StaticClass()) &&
				HasNamedVariable(Blueprint, TEXT("Panel_UpgradeMetricRows"), *UVerticalBox::StaticClass()) &&
				HasNamedVariable(Blueprint, TEXT("Button_RevenueRanking"), *UButton::StaticClass());
		auto* Shell = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Panel_Shell")));
		auto* HeaderIdentity = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Header_Identity")));
		auto* ThumbnailSize = Cast<USizeBox>(Tree.FindWidget(TEXT("Thumbnail_Size")));
		auto* Name = Cast<UCommonTextBlock>(Tree.FindWidget(TEXT("Text_FacilityName")));
		auto* CategoryRow = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("Meta_Row")));
		auto* Favorite = Cast<UButton>(Tree.FindWidget(TEXT("Button_Favorite")));
		auto* UpgradeContent = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Upgrade_Content")));
		auto* Comparison = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("Panel_UpgradeComparison")));
		auto* Confirmation = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Panel_RemoveConfirmation")));
		auto* Confirm = Cast<UButton>(Tree.FindWidget(TEXT("Button_RemoveConfirm")));
		auto* Cancel = Cast<UButton>(Tree.FindWidget(TEXT("Button_RemoveCancel")));
		auto* Synergy = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Panel_SynergySection")));
		auto* SynergySummary = Cast<UCommonTextBlock>(Tree.FindWidget(TEXT("Text_SynergySummary")));
		auto* RevenueContent = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Revenue_Content")));
		auto* LiveContent = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Live_Content")));
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette || !Shell || !HeaderIdentity || !ThumbnailSize || !Name || !CategoryRow || !Favorite ||
			!UpgradeContent || !Comparison || !Confirmation || !Confirm || !Cancel || !Synergy ||
			!SynergySummary || !RevenueContent || !LiveContent || !HasRequiredLayout(Blueprint) ||
			!HasRevenueWidgetContract(Blueprint) || !HasSynergyInspectorContract(Blueprint)) return false;
		const FFF_UIPaletteInGame& P = Palette->InGame;
		Blueprint.Modify();
		Tree.Modify();
		auto Text = [&](const FName WidgetName, const TCHAR* Copy, const int32 Size,
			const bool bVariable = false, const bool bStrong = false)
		{
			auto* Widget = MakeWidget<UCommonTextBlock>(Blueprint, Tree, WidgetName, bVariable);
			Widget->SetText(FText::FromString(Copy));
			Widget->SetAutoWrapText(true);
			StyleLightText(*Widget, Size, bStrong ? P.TealDeep : P.InkMuted, bStrong ? TEXT("SemiBold") : TEXT("Regular"));
			return Widget;
		};
		// Reuse the actual thumbnail, facility name and category bindings in a fixed shared header.
		auto* Identity = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Panel_PersistentIdentity"), true);
		auto* IdentityRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("V2_IdentityRow"));
		auto* IdentityMeta = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("V2_IdentityMeta"));
		PaintBorder(*Identity, MakeSolidBrush(P.PaperSunken, 7.f, P.Line, .5f));
		Identity->SetPadding(FMargin(8.f));
		ThumbnailSize->RemoveFromParent();
		ThumbnailSize->SetWidthOverride(80.f);
		ThumbnailSize->SetHeightOverride(76.f);
		AddHorizontal(*IdentityRow, *ThumbnailSize, FMargin(0, 0, 10, 0));
		Name->RemoveFromParent();
		StyleLightText(*Name, 20, P.Ink, TEXT("SemiBold"));
		AddVertical(*IdentityMeta, *Name, FMargin(0, 0, 0, 6));
		CategoryRow->RemoveFromParent();
		AddVertical(*IdentityMeta, *CategoryRow);
		AddHorizontal(*IdentityRow, *IdentityMeta, FMargin(0), ESlateSizeRule::Fill);
		Favorite->RemoveFromParent();
		auto* FavoriteSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("V2_FavoriteSize"));
		FavoriteSize->SetWidthOverride(36.f);
		FavoriteSize->SetHeightOverride(34.f);
		FavoriteSize->AddChild(Favorite);
		AddHorizontal(*IdentityRow, *FavoriteSize);
		Identity->AddChild(IdentityRow);
		AddVertical(*Shell, *Identity, FMargin(10, 0, 10, 6));
		Shell->ShiftChild(1, Identity);
		if (auto* Context = Tree.FindWidget(TEXT("Text_Context"))) Context->SetVisibility(ESlateVisibility::Collapsed);
		AddVertical(*HeaderIdentity, *Text(TEXT("V2_InspectorTitle"), TEXT("시설 정보"), 17, false, true));
		if (auto* Header = Cast<UBorder>(Tree.FindWidget(TEXT("Header_Surface"))))
		{
			PaintBorder(*Header, MakeSolidBrush(P.Paper, 7.f));
			Header->SetPadding(FMargin(12, 6, 8, 6));
		}
		if (auto* Header = Cast<USizeBox>(Tree.FindWidget(TEXT("Header_Size")))) Header->SetHeightOverride(44.f);
		if (auto* CloseLabel = Cast<UCommonTextBlock>(Tree.FindWidget(TEXT("Close_Label"))))
			StyleLightText(*CloseLabel, 20, P.TealDeep, TEXT("SemiBold"));
		for (const FName TabName : {FName(TEXT("Button_TabOverview")), FName(TEXT("Button_TabSynergy")),
			FName(TEXT("Button_TabOperation")), FName(TEXT("Button_TabUpgrade"))})
			if (auto* Tab = Cast<UButton>(Tree.FindWidget(TabName)))
			{
				FButtonStyle Style = Tab->GetStyle();
				Style.Disabled = MakeSolidBrush(P.TealDeep, 5.f);
				Style.NormalPadding = FMargin(4, 8);
				Style.PressedPadding = Style.NormalPadding;
				Tab->SetStyle(Style);
			}
		// New comparison rows carry separate native values; legacy columns remain for older assets.
		auto* Metrics = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_UpgradeMetricRows"), true);
		auto* Captions = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("V2_UpgradeCaptions"));
		for (const auto& Entry : TArray<TPair<FName, FString>>{
			{TEXT("V2_UpgradeMetricCaption"), TEXT("항목")}, {TEXT("V2_UpgradeCurrentCaption"), TEXT("현재")},
			{TEXT("V2_UpgradeNextCaption"), TEXT("강화 후")}})
			AddHorizontal(*Captions, *Text(Entry.Key, *Entry.Value, 11, false, true), FMargin(5, 4), ESlateSizeRule::Fill);
		AddVertical(*Metrics, *Captions, FMargin(0, 0, 0, 4));
		const TCHAR* Keys[] = {TEXT("Revenue"), TEXT("Time"), TEXT("Throughput"), TEXT("Size")};
		const TCHAR* Labels[] = {TEXT("1인 수익"), TEXT("팀당 시간"), TEXT("처리 속도"), TEXT("크기")};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Keys); ++Index)
		{
			auto* Row = MakeWidget<UHorizontalBox>(Blueprint, Tree, FName(FString(TEXT("V2_UpgradeRow")) + Keys[Index]));
			AddHorizontal(*Row, *Text(FName(FString(TEXT("V2_UpgradeLabel")) + Keys[Index]), Labels[Index], 11, false, true),
				FMargin(5, 5), ESlateSizeRule::Fill);
			for (const bool bNext : {false, true})
			{
				const FString CellName = FString(bNext ? TEXT("Text_UpgradeNext") : TEXT("Text_UpgradeCurrent")) + Keys[Index];
				auto* Cell = MakeWidget<UBorder>(Blueprint, Tree, FName(CellName + TEXT("_Cell")));
				PaintBorder(*Cell, MakeSolidBrush(bNext ? P.TealSoft : P.PaperSunken, 4.f));
				Cell->SetPadding(FMargin(5, 6));
				Cell->AddChild(Text(FName(CellName), TEXT("—"), 12, true, true));
				AddHorizontal(*Row, *Cell, FMargin(2, 0), ESlateSizeRule::Fill);
			}
			AddVertical(*Metrics, *Row, FMargin(0, 0, 0, 4));
		}
		AddVertical(*UpgradeContent, *Metrics, FMargin(0, 0, 0, 8));
		UpgradeContent->ShiftChild(UpgradeContent->GetChildIndex(Comparison), Metrics);
		auto* Quote = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_UpgradeQuote"), true);
		AddStatPair(Blueprint, Tree, *Quote, *Palette, TEXT("Text_UpgradePrice"), TEXT("강화 비용"));
		auto* BalanceRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("V2_UpgradeBalanceRow"));
		AddHorizontal(*BalanceRow, *Text(TEXT("Text_UpgradeBalanceLabel"), TEXT("구매 후 잔액"), 11, true), FMargin(0), ESlateSizeRule::Fill);
		AddHorizontal(*BalanceRow, *Text(TEXT("Text_UpgradeBalance"), TEXT("—"), 13, true, true));
		AddVertical(*Quote, *BalanceRow, FMargin(8, 2, 8, 8));
		AddVertical(*Quote, *Text(TEXT("V2_UpgradeRefundNotice"), TEXT("강화비는 철거 시 환급되지 않습니다."), 11), FMargin(2, 0, 2, 8));
		AddVertical(*UpgradeContent, *Quote);
		UpgradeContent->ShiftChild(UpgradeContent->GetChildIndex(Metrics) + 1, Quote);
		// The operation model's occupancy includes waiting/processing parties, not concurrent service capacity.
		if (auto* Label = Cast<UCommonTextBlock>(Tree.FindWidget(TEXT("Text_Occupancy_Label"))))
			Label->SetText(FText::FromString(TEXT("대기·처리 중 팀")));
		if (auto* Label = Cast<UCommonTextBlock>(Tree.FindWidget(TEXT("Text_WaitingParties_Label"))))
			Label->SetText(FText::FromString(TEXT("대기·처리 중 일행")));
		if (auto* Occupancy = Tree.FindWidget(TEXT("Text_Occupancy")))
			Occupancy->SetToolTipText(FText::FromString(TEXT("대기·처리 중인 팀 수와 최대 팀 수입니다. 인원수와 관계없이 한 팀이 한 자리를 사용합니다.")));
		if (auto* Today = Cast<UCommonTextBlock>(Tree.FindWidget(TEXT("Text_FacilityTodayRevenue"))))
			StyleLightText(*Today, 25, P.TealDeep, TEXT("SemiBold"));
		auto* Ranking = MakeLightTextButton(Blueprint, Tree, TEXT("Button_RevenueRanking"), TEXT("V2_RevenueRankingLabel"),
			FText::FromString(TEXT("시설별 매출 순위  →")), P.PaperSunken, P.TealSoft, P.TealSoft, P.Line, P.TealDeep, true, false);
		AddVertical(*LiveContent, *Ranking, FMargin(0, 6, 0, 0));
		// A summary card is also the honest empty/loading state; real entries keep their existing pool.
		SynergySummary->RemoveFromParent();
		auto* SummaryCard = MakeWidget<UBorder>(Blueprint, Tree, TEXT("V2_SynergySummaryCard"));
		PaintBorder(*SummaryCard, MakeSolidBrush(P.PaperSunken, 7.f));
		SummaryCard->SetPadding(FMargin(14, 22));
		StyleLightText(*SynergySummary, 14, P.TealDeep, TEXT("Medium"), ETextJustify::Center);
		SummaryCard->AddChild(SynergySummary);
		AddVertical(*Synergy, *SummaryCard, FMargin(0, 0, 0, 12));
		Synergy->ShiftChild(0, SummaryCard);
		if (UWidget* Launcher = Tree.FindWidget(TEXT("Widget_FacilityCodexLauncher")))
			Synergy->ShiftChild(1, Launcher);
		// Destructive confirmation keeps its original guarded buttons, now in a fixed footer.
		Cancel->RemoveFromParent();
		Confirm->RemoveFromParent();
		if (auto* ConfirmLabel = Cast<UCommonTextBlock>(Confirm->GetContent()))
		{
			ConfirmLabel->SetAutoWrapText(true);
			StyleLightText(*ConfirmLabel, 12, P.White, TEXT("SemiBold"), ETextJustify::Center);
		}
		auto* RemoveActions = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Panel_RemoveActions"), true);
		AddHorizontal(*RemoveActions, *Cancel, FMargin(0, 0, 5, 0), ESlateSizeRule::Fill);
		AddHorizontal(*RemoveActions, *Confirm, FMargin(5, 0, 0, 0), ESlateSizeRule::Fill);
		AddVertical(*Shell, *RemoveActions, FMargin(10, 8, 10, 10));
		RemoveActions->SetVisibility(ESlateVisibility::Collapsed);
		AddStatPair(Blueprint, Tree, *Confirmation, *Palette, TEXT("Text_RemoveRecovery"), TEXT("회수 금액"));
		if (auto* Recovery = Cast<UCommonTextBlock>(Tree.FindWidget(TEXT("Text_RemoveRecovery"))))
			StyleLightText(*Recovery, 28, P.TealDeep, TEXT("SemiBold"));
		AddStatPair(Blueprint, Tree, *Confirmation, *Palette, TEXT("Text_RemoveInstallCost"), TEXT("설치 비용"));
		auto* RemoveWarning = MakeWidget<UBorder>(Blueprint, Tree, TEXT("V2_RemoveWarning"));
		FLinearColor WarningColor = P.Danger; WarningColor.A = .08f;
		PaintBorder(*RemoveWarning, MakeSolidBrush(WarningColor, 6.f, P.Danger, .5f));
		RemoveWarning->SetPadding(FMargin(10, 12));
		auto* Warning = Text(TEXT("V2_RemoveRefundNotice"), TEXT("강화에 사용한 비용은 환불되지 않습니다."), 12);
		Warning->SetColorAndOpacity(FSlateColor(P.Danger));
		RemoveWarning->AddChild(Warning);
		AddVertical(*Confirmation, *RemoveWarning, FMargin(0, 8, 0, 8));
		AddVertical(*Confirmation, *Text(TEXT("V2_RemoveExplanation"), TEXT("철거하면 시설이 제거됩니다."), 12));
		// Bindings, input routes, outer Canvas slots, scroll body and normal footer remain intact.
		bOutModified = true;
		return true;
	}

	bool RefineInspectorV2Layout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, bool& bOutModified)
	{
		bOutModified = false;
		if (Tree.FindWidget(TEXT("Panel_UpgradeActions")))
			return HasNamedVariable(Blueprint, TEXT("Panel_UpgradeActions"), *UBorder::StaticClass());
		auto* Shell = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Panel_Shell")));
		auto* NormalActions = Tree.FindWidget(TEXT("Panel_Actions"));
		auto* Upgrade = Cast<UButton>(Tree.FindWidget(TEXT("Button_Upgrade")));
		auto* Notice = Cast<UCommonTextBlock>(Tree.FindWidget(TEXT("V2_UpgradeRefundNotice")));
		auto* PriceContent = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Text_UpgradePrice_Content")));
		auto* PriceLabel = Cast<UCommonTextBlock>(Tree.FindWidget(TEXT("Text_UpgradePrice_Label")));
		auto* PriceValue = Cast<UCommonTextBlock>(Tree.FindWidget(TEXT("Text_UpgradePrice")));
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette || !Shell || !NormalActions || NormalActions->GetParent() != Shell || !Upgrade ||
			!Notice || !PriceContent || !PriceLabel || !PriceValue || !Tree.FindWidget(TEXT("Panel_PersistentIdentity"))) return false;
		Blueprint.Modify();
		Tree.Modify();
		const auto& P = Palette->InGame;
		// The quote can grow with localization and failure reasons. Keep its existing guarded action
		// and the non-refundable-cost notice visible above the unchanged move/remove footer.
		auto* Actions = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Panel_UpgradeActions"), true);
		auto* ActionsContent = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("V2_UpgradeFooterContent"));
		PaintBorder(*Actions, MakeSolidBrush(P.Paper, 0.f));
		Actions->SetPadding(FMargin(12, 4, 12, 4));
		Notice->RemoveFromParent();
		AddVertical(*ActionsContent, *Notice, FMargin(0, 0, 0, 4));
		Upgrade->RemoveFromParent();
		AddVertical(*ActionsContent, *Upgrade);
		Actions->AddChild(ActionsContent);
		AddVertical(*Shell, *Actions);
		Shell->ShiftChild(Shell->GetChildIndex(NormalActions), Actions);
		Actions->SetVisibility(ESlateVisibility::Collapsed);
		// A horizontal cost row matches the approved comparison design and leaves useful scroll space.
		auto* PriceRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("V2_UpgradePriceRow"));
		PriceLabel->RemoveFromParent();
		PriceValue->RemoveFromParent();
		StyleLightText(*PriceLabel, 11, P.TealDeep, TEXT("Medium"));
		StyleLightText(*PriceValue, 15, P.TealDeep, TEXT("SemiBold"), ETextJustify::Right);
		AddHorizontal(*PriceRow, *PriceLabel, FMargin(0, 0, 8, 0), ESlateSizeRule::Fill);
		AddHorizontal(*PriceRow, *PriceValue);
		AddVertical(*PriceContent, *PriceRow);
		if (auto* Card = Cast<UBorder>(Tree.FindWidget(TEXT("Text_UpgradePrice_Card")))) Card->SetPadding(FMargin(8, 6));
		for (const TCHAR* Key : {TEXT("Revenue"), TEXT("Time"), TEXT("Throughput"), TEXT("Size")})
			for (const TCHAR* Period : {TEXT("Current"), TEXT("Next")})
				if (auto* Cell = Cast<UBorder>(Tree.FindWidget(FName(FString(TEXT("Text_Upgrade")) + Period + Key + TEXT("_Cell")))))
					Cell->SetPadding(FMargin(5, 3));
		if (auto* Level = Cast<UCommonTextBlock>(Tree.FindWidget(TEXT("Text_FacilityLevel"))))
		{
			StyleLightText(*Level, 11, P.TealDeep, TEXT("SemiBold"));
			if (auto* Slot = Cast<UVerticalBoxSlot>(Level->Slot)) Slot->SetPadding(FMargin(0, 0, 0, 2));
		}
		if (UWidget* Metrics = Tree.FindWidget(TEXT("Panel_UpgradeMetricRows")))
			if (auto* Slot = Cast<UVerticalBoxSlot>(Metrics->Slot)) Slot->SetPadding(FMargin(0, 0, 0, 4));
		bOutModified = true;
		return true;
	}

	void RefineFacilityInfoV2Umg()
	{
		UWidgetBlueprint* Inspector = LoadExistingBlueprint();
		if (!Inspector || !Inspector->WidgetTree) return;
		auto* Probe = DuplicateObject<UWidgetBlueprint>(Inspector, GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UWidgetBlueprint::StaticClass(), TEXT("FacilityInfoV2LayoutProbe")));
		if (!Probe || !Probe->WidgetTree) return;
		Probe->SetFlags(RF_Transient);
		bool bProbeChanged = false;
		if (!RefineInspectorV2Layout(*Probe, *Probe->WidgetTree, bProbeChanged))
		{
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error, TEXT("FF_FACILITY_INFO_V2_LAYOUT_FAIL: incompatible V2 inspector; original unchanged."));
			return;
		}
		bool bChanged = false;
		if (!RefineInspectorV2Layout(*Inspector, *Inspector->WidgetTree, bChanged) || (bChanged && !SaveBlueprint(*Inspector)))
		{
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error, TEXT("FF_FACILITY_INFO_V2_LAYOUT_FAIL: save failed."));
			return;
		}
		UE_LOG(LogFFFacilityInfoWbpBuilder, Display, TEXT("FF_FACILITY_INFO_V2_LAYOUT_%s"), bChanged ? TEXT("APPLIED") : TEXT("REUSE"));
	}

	FAutoConsoleCommand RefineFacilityInfoV2Command(TEXT("FF.FixFacilityInfoV2Layout"),
		TEXT("One-time inspector-only V2 refinement: fixed upgrade action and compact quote; preserves subsequent Designer edits."),
		FConsoleCommandDelegate::CreateStatic(&RefineFacilityInfoV2Umg));

	void ApplyFacilityInfoV2Umg()
	{
		UWidgetBlueprint* Inspector = LoadExistingBlueprint();
		if (!Inspector || !Inspector->WidgetTree || !Inspector->ParentClass ||
			!Inspector->ParentClass->IsChildOf(UFF_FacilityInfoPanel::StaticClass())) return;
		// Preflight a transient copy first. No other Blueprint is created, refreshed or saved.
		UWidgetBlueprint* Probe = DuplicateObject<UWidgetBlueprint>(Inspector, GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UWidgetBlueprint::StaticClass(), TEXT("FacilityInfoV2Probe")));
		if (!Probe) return;
		Probe->SetFlags(RF_Transient);
		bool bProbeChanged = false;
		if (!Probe->WidgetTree || !ApplyInspectorV2Design(*Probe, *Probe->WidgetTree, bProbeChanged))
		{
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error, TEXT("FF_FACILITY_INFO_V2_FAIL: incompatible inspector; original asset unchanged."));
			return;
		}
		bool bChanged = false;
		if (!ApplyInspectorV2Design(*Inspector, *Inspector->WidgetTree, bChanged) || (bChanged && !SaveBlueprint(*Inspector)))
		{
			UE_LOG(LogFFFacilityInfoWbpBuilder, Error, TEXT("FF_FACILITY_INFO_V2_FAIL: inspector save failed."));
			return;
		}
		UE_LOG(LogFFFacilityInfoWbpBuilder, Display, TEXT("FF_FACILITY_INFO_V2_%s: inspector only; Designer layout retained on reuse."),
			bChanged ? TEXT("APPLIED") : TEXT("REUSE"));
	}

	FAutoConsoleCommand ApplyFacilityInfoV2Command(TEXT("FF.ApplyFacilityInfoV2UMG"),
		TEXT("Applies the approved V2 inspector design once; saves only FacilityInfoPanel, never Picker/Card or HUD."),
		FConsoleCommandDelegate::CreateStatic(&ApplyFacilityInfoV2Umg));

	FAutoConsoleCommand RefreshFacilityInfoGameUmgCommand(
		TEXT("FF.RefreshFacilityInfoGameUMG"),
		TEXT("Explicitly rebuilds only the facility information panel using the editable festival-game template."),
		FConsoleCommandDelegate::CreateStatic(&RefreshFacilityInfoGameUmg));

	FAutoConsoleCommand BuildFacilityInfoUmgCommand(
		TEXT("FF.BuildFacilityInfoUMG"),
		TEXT("Creates the editable right-side installed-facility information WBP when absent."),
		FConsoleCommandDelegate::CreateStatic(&BuildFacilityInfoUmg));

	FAutoConsoleCommand RefreshFacilityInfoUmgCommand(
		TEXT("FF.RefreshFacilityInfoUMG"),
		TEXT("Rebuilds the facility-info, paid-program picker, and paid-program card WidgetTrees with the editable LightTeal templates."),
		FConsoleCommandDelegate::CreateStatic(&RefreshFacilityInfoUmg));

	FAutoConsoleCommand RefreshFacilityPaidProgramUmgCommand(
		TEXT("FF.RefreshFacilityPaidProgramUMG"),
		TEXT("Rebuilds only the paid-program picker and card WidgetTrees with the editable LightTeal templates."),
		FConsoleCommandDelegate::CreateStatic(&RefreshFacilityPaidProgramUmg));
}

#endif // WITH_EDITOR
