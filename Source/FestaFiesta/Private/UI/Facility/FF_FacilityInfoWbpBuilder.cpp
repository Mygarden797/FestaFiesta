// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
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
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UI/Facility/FF_FacilityInfoUpgradeLayout.h"
#include "UI/Facility/FF_FacilityPaidProgramCard.h"
#include "UI/Facility/FF_FacilityPaidProgramPicker.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFFacilityInfoWbpBuilder, Log, All);

namespace FFFacilityInfoWbpBuilder
{
	const FString PanelPackage(TEXT("/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityInfoPanel"));
	const FString PaidProgramPickerPackage(
		TEXT("/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityPaidProgramPicker"));
	const FString PaidProgramCardPackage(
		TEXT("/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityPaidProgramCard"));

	const FLinearColor ColorPanel(0.018f, 0.047f, 0.070f, 0.985f);
	const FLinearColor ColorSection(0.027f, 0.080f, 0.102f, 0.96f);
	const FLinearColor ColorPrimary(0.91f, 0.96f, 1.0f, 1.0f);
	const FLinearColor ColorMuted(0.58f, 0.70f, 0.76f, 1.0f);
	const FLinearColor ColorAccent(0.08f, 0.95f, 0.88f, 1.0f);
	const FLinearColor ColorWarning(1.0f, 0.73f, 0.23f, 1.0f);

	constexpr float PanelWidth = 376.0f;
	constexpr float PanelHeight = 500.0f;
	constexpr float PanelRightMargin = 18.0f;
	constexpr float PanelTopMargin = 18.0f;
	// PrepHUD Build host(320), 하단 바/여백(70), 안전 간격(10)을 예약한다.
	// Picker는 세로 stretch라 1080p뿐 아니라 더 작은 뷰포트에서도 하단 패널과 겹치지 않는다.
	constexpr float PaidProgramPickerWidth = 520.0f;
	constexpr float PaidProgramPickerRightMargin = 24.0f;
	constexpr float PaidProgramPickerTopMargin = 72.0f;
	constexpr float PaidProgramPickerBottomReserve = 400.0f;

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
		if (!AddInfoRow(Blueprint, Tree, *BuildSection, *Palette, TEXT("Footprint_Row"),
			FText::FromString(TEXT("시설 크기")), TEXT("Text_Footprint"), Footprint) ||
			!AddInfoRow(Blueprint, Tree, *BuildSection, *Palette, TEXT("Clearance_Row"),
				FText::FromString(TEXT("안전 여유")), TEXT("Text_Clearance"), Clearance) ||
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
				FText::FromString(TEXT("점유 인원")), TEXT("Text_Occupancy"), Occupancy) ||
			!AddInfoRow(Blueprint, Tree, *LiveSection, *Palette, TEXT("RemainingService_Row"),
				FText::FromString(TEXT("남은 서비스 인원")), TEXT("Text_RemainingService"), RemainingService) ||
			!AddInfoRow(Blueprint, Tree, *LiveSection, *Palette, TEXT("ExpectedWait_Row"),
				FText::FromString(TEXT("1인 기준 예상 대기")), TEXT("Text_ExpectedWait"), ExpectedWait) ||
			!AddInfoRow(Blueprint, Tree, *LiveSection, *Palette, TEXT("Throughput_Row"),
				FText::FromString(TEXT("처리 속도")), TEXT("Text_Throughput"), Throughput))
		{
			return false;
		}
		// New facilities get the authored button. The runtime panel also creates this button when
		// loading the already-shipped WBP, so adding the MVP never overwrites designer work.
		UButton* StartMinigame = MakeTextButton(
			Blueprint, Tree, TEXT("Button_StartMinigame"), TEXT("Text_StartMinigame"),
			FText::FromString(TEXT("게임 시작")),
			FLinearColor(0.02f, 0.55f, 0.72f, 1.0f), true, true);
		if (!StartMinigame)
		{
			return false;
		}
		AddVertical(*LiveSection, *StartMinigame, FMargin(0.0f, 10.0f, 0.0f, 0.0f));

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
				TEXT("Text_WaitingParties"), TEXT("대기 / 처리 중 일행"), TEXT("Text_Occupancy"), TEXT("이용 중 인원")) ||
				!AddStatPair(Blueprint, Tree, *SectionContent, Palette,
				TEXT("Text_RemainingService"), TEXT("남은 서비스 인원"), TEXT("Text_ExpectedWait"), TEXT("1인 예상 대기")) ||
				!AddStatPair(Blueprint, Tree, *SectionContent, Palette, TEXT("Text_Throughput"), TEXT("처리 속도")))
			{
				return nullptr;
			}
			Section->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			if (!AddStatPair(Blueprint, Tree, *SectionContent, Palette,
				TEXT("Text_Footprint"), TEXT("시설 크기"), TEXT("Text_Clearance"), TEXT("안전 여유")) ||
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
		UVerticalBox* LiveActions = MakeWidget<UVerticalBox>(
			Blueprint, Tree, TEXT("Panel_LiveActions"), true);
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
		UButton* StartMinigame = MakeLightTextButton(
			Blueprint, Tree, TEXT("Button_StartMinigame"), TEXT("Text_StartMinigame"),
			FText::FromString(TEXT("게임 시작")),
			Palette.InGame.Teal, Palette.InGame.TealDeep, Palette.InGame.TealDeep,
			Palette.InGame.LineStrong, Palette.InGame.White, true, true);
		if (!Actions || !Switcher || !BuildActions || !LiveActions || !Move || !Remove ||
			!StartMinigame)
		{
			return nullptr;
		}

		PaintBorder(*Actions, MakeSolidBrush(Palette.InGame.PaperSunken, 0.0f));
		Actions->SetPadding(FMargin(12.0f, 9.0f));
		AddHorizontal(*BuildActions, *Move, FMargin(0.0f, 0.0f, 5.0f, 0.0f), ESlateSizeRule::Fill);
		AddHorizontal(*BuildActions, *Remove, FMargin(5.0f, 0.0f, 0.0f, 0.0f), ESlateSizeRule::Fill);
		AddVertical(*LiveActions, *StartMinigame);
		LiveActions->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* Slot = Switcher->AddChildToOverlay(BuildActions))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		if (UOverlaySlot* Slot = Switcher->AddChildToOverlay(LiveActions))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		Actions->AddChild(Switcher);
		return Actions;
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
		UCommonTextBlock* RelocationWarning = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_RelocationWarning"), true);
		UBorder* BuildSection = BuildInformationSection(Blueprint, Tree, *Palette, false);
		UBorder* LiveSection = BuildInformationSection(Blueprint, Tree, *Palette, true);
		UVerticalBox* ProgramSection = BuildProgramSection(Blueprint, Tree, *Palette);
		UCommonTextBlock* Feedback = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_Feedback"), true);
		UBorder* Actions = BuildActionRail(Blueprint, Tree, *Palette);
		if (!Root || !ShadowSize || !Shadow || !PanelSize || !Panel || !Shell || !Header ||
			!Body || !DetailsScroll || !Content || !Summary || !UpgradeSection || !RelocationWarning ||
			!BuildSection || !LiveSection || !ProgramSection || !Feedback || !Actions)
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
		return EnsureUpgradeWidgetContract(Blueprint, Tree, bUpgradeContractAdded);
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
		if (!Anchors.Minimum.Equals(FVector2D(1.0f, 0.0f)) ||
			!Anchors.Maximum.Equals(FVector2D(1.0f, 0.0f)) ||
			!PanelSlot->GetAlignment().Equals(FVector2D(1.0f, 0.0f)) ||
			!PanelSlot->GetPosition().Equals(FVector2D(-PanelRightMargin, PanelTopMargin)) ||
			PanelSlot->GetAutoSize() ||
			!PanelSlot->GetSize().Equals(FVector2D(PanelWidth, PanelHeight)) ||
			!FMath::IsNearlyEqual(PanelSize->GetWidthOverride(), PanelWidth) ||
			!FMath::IsNearlyEqual(PanelSize->GetHeightOverride(), PanelHeight))
		{
			return false;
		}

		const TArray<FName> RequiredVariables = {
			TEXT("Border_Panel"), TEXT("Button_Close"), TEXT("Button_Move"),
			TEXT("Button_Remove"), TEXT("Button_StartMinigame"), TEXT("Text_StartMinigame"),
			TEXT("Image_Thumbnail"), TEXT("Text_ThumbnailPlaceholder"),
			TEXT("Panel_BuildSection"), TEXT("Panel_LiveSection"), TEXT("Text_Context"),
			TEXT("Panel_Actions"), TEXT("Panel_BuildActions"), TEXT("Panel_LiveActions"),
			TEXT("Panel_UpgradeComparison"), TEXT("Text_UpgradeCurrent"), TEXT("Text_UpgradeNext"),
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
			Cast<UScrollBox>(Tree->FindWidget(TEXT("ScrollBox_Details"))) &&
			Cast<UVerticalBox>(Tree->FindWidget(TEXT("Panel_Content"))) &&
			Cast<UButton>(Tree->FindWidget(TEXT("Button_Close"))) &&
			Cast<UButton>(Tree->FindWidget(TEXT("Button_Move"))) &&
			Cast<UButton>(Tree->FindWidget(TEXT("Button_Remove"))) &&
			Cast<UButton>(Tree->FindWidget(TEXT("Button_StartMinigame"))) &&
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

	bool SaveBlueprint(UWidgetBlueprint& Blueprint)
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
			Root->GetVisibility() == ESlateVisibility::SelfHitTestInvisible &&
			PickerSlot->GetAnchors().Minimum.Equals(FVector2D(1.0f, 0.0f)) &&
			PickerSlot->GetAnchors().Maximum.Equals(FVector2D(1.0f, 1.0f)) &&
			PickerSlot->GetAlignment().Equals(FVector2D(1.0f, 0.0f)) &&
			FMath::IsNearlyEqual(PickerSlot->GetOffsets().Left, -PaidProgramPickerRightMargin) &&
			FMath::IsNearlyEqual(PickerSlot->GetOffsets().Top, PaidProgramPickerTopMargin) &&
			FMath::IsNearlyEqual(PickerSlot->GetOffsets().Right, PaidProgramPickerWidth) &&
			FMath::IsNearlyEqual(PickerSlot->GetOffsets().Bottom, PaidProgramPickerBottomReserve);
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
		PickerSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 1.0f));
		PickerSlot->SetAlignment(FVector2D(1.0f, 0.0f));
		PickerSlot->SetOffsets(FMargin(
			-PaidProgramPickerRightMargin,
			PaidProgramPickerTopMargin,
			PaidProgramPickerWidth,
			PaidProgramPickerBottomReserve));
		PickerSlot->SetAutoSize(false);

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
		return HasPaidProgramPickerContract(Blueprint);
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
					!HasUpgradeWidgetContract(*Existing))
				{
					UE_LOG(LogFFFacilityInfoWbpBuilder, Error,
						TEXT("FF_FACILITY_INFO_UMG_FAIL: existing asset cannot accept a non-destructive widget contract migration: %s"),
						*PanelPackage);
					return;
				}
				if ((bMigratedProgramContract || bMigratedInstanceLimitContract || bMigratedUpgradeContract) &&
					!SaveBlueprint(*Existing))
				{
					UE_LOG(LogFFFacilityInfoWbpBuilder, Error,
						TEXT("FF_FACILITY_INFO_UMG_FAIL: could not save migrated asset: %s"),
						*PanelPackage);
					return;
				}

				if (bMigratedProgramContract || bMigratedInstanceLimitContract || bMigratedUpgradeContract)
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
