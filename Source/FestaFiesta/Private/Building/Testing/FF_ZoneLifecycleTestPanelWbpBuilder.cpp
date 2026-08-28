// FF_ZoneLifecycleTestPanelWbpBuilder.cpp

// Test_Main 시설 디버그 패널을 인게임 LightTeal 톤으로 생성·명시적 리프레시한다.
// 흐름: Blueprint 생성/로드 -> Designer 편집 단위로 레이아웃 조립 -> 계약 검증 -> 컴파일·저장.
// 불변식: 기본 명령은 기존 WBP를 보존하고 Refresh 명령만 승인된 WidgetTree 교체를 수행한다.
// 실패/수명: 새 트리 생성·컴파일이 실패하면 기존 트리와 바인딩을 복구하고 에셋을 저장하지 않는다.

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Building/Testing/FF_ZoneLifecycleTestPanel.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFZoneLifecycleTestWbpBuilder, Log, All);

namespace FFZoneLifecycleTestWbpBuilder
{
	const FString PanelPackage(
		TEXT("/Game/FestaFiesta/Building/Debug/WBP_FF_ZoneLifecycleTestPanel"));
	const FString PanelFolder(TEXT("/Game/FestaFiesta/Building/Debug"));

	constexpr float PanelWidth = 480.0f;
	constexpr float PanelHeight = 540.0f;
	constexpr float PanelLeftMargin = 24.0f;
	constexpr float PanelTopMargin = 72.0f;

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
		return FFUIWbpBuilder::MakeWidget<TWidget>(Blueprint, Tree, Name, bVariable);
	}

	void StyleLightText(
		UCommonTextBlock& Text,
		const int32 FontSize,
		const FLinearColor& Color,
		const FName Typeface = TEXT("Regular"),
		const ETextJustify::Type Justification = ETextJustify::Left,
		const bool bAutoWrap = false)
	{
		FFUIWbpBuilder::StyleText(Text, FontSize, Color, Typeface);
		Text.SetJustification(Justification);
		Text.SetAutoWrapText(bAutoWrap);
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
		const ESlateSizeRule::Type SizeRule = ESlateSizeRule::Automatic,
		const EHorizontalAlignment HorizontalAlignment = HAlign_Fill)
	{
		UHorizontalBoxSlot* Slot = Parent.AddChildToHorizontalBox(&Child);
		if (Slot)
		{
			Slot->SetPadding(Padding);
			Slot->SetSize(FSlateChildSize(SizeRule));
			Slot->SetHorizontalAlignment(HorizontalAlignment);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		return Slot;
	}

	UBorder* MakeChip(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const FName BorderName,
		const FName TextName,
		const FText& Label,
		const FLinearColor& Fill,
		const FLinearColor& TextColor,
		const FLinearColor& OutlineColor)
	{
		UBorder* Chip = MakeWidget<UBorder>(Blueprint, Tree, BorderName);
		UCommonTextBlock* Text = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TextName);
		if (!Chip || !Text)
		{
			return nullptr;
		}

		PaintBorder(*Chip, MakeSolidBrush(Fill, 4.0f, OutlineColor, 1.0f));
		Chip->SetPadding(FMargin(9.0f, 5.0f));
		Text->SetText(Label);
		StyleLightText(*Text, 11, TextColor, TEXT("SemiBold"), ETextJustify::Center);
		Chip->AddChild(Text);
		return Chip;
	}

	UButton* MakeLightButton(
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
		const FLinearColor& OutlineColor)
	{
		UButton* Button = MakeWidget<UButton>(Blueprint, Tree, ButtonName, true);
		UCommonTextBlock* Text = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TextName);
		if (!Button || !Text)
		{
			return nullptr;
		}

		FButtonStyle Style;
		Style.Normal = MakeSolidBrush(NormalColor, 6.0f, OutlineColor, 1.0f);
		Style.Hovered = MakeSolidBrush(HoverColor, 6.0f, OutlineColor, 1.0f);
		Style.Pressed = MakeSolidBrush(PressedColor, 6.0f, OutlineColor, 1.0f);
		Style.Disabled = MakeSolidBrush(DisabledColor, 6.0f, OutlineColor, 1.0f);
		Style.NormalPadding = FMargin(8.0f, 8.0f);
		Style.PressedPadding = FMargin(8.0f, 9.0f, 8.0f, 7.0f);
		Button->SetStyle(Style);

		Text->SetText(Label);
		StyleLightText(*Text, 12, TextColor, TEXT("SemiBold"), ETextJustify::Center);
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Button->AddChild(Text)))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		return Button;
	}

	bool AddButtonRow(UHorizontalBox& Row, const TArray<UButton*>& Buttons)
	{
		for (int32 Index = 0; Index < Buttons.Num(); ++Index)
		{
			UButton* Button = Buttons[Index];
			const FMargin Padding = Index + 1 < Buttons.Num()
				? FMargin(0.0f, 0.0f, 6.0f, 0.0f)
				: FMargin(0.0f);
			if (!Button || !AddHorizontal(Row, *Button, Padding, ESlateSizeRule::Fill))
			{
				return false;
			}
		}
		return true;
	}

	bool AddSectionHeading(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		UVerticalBox& Parent,
		const FName TitleName,
		const FName CaptionName,
		const FText& TitleText,
		const FText& CaptionText,
		const UFF_UIPalette& Palette)
	{
		UCommonTextBlock* Title = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TitleName);
		UCommonTextBlock* Caption = MakeWidget<UCommonTextBlock>(Blueprint, Tree, CaptionName);
		if (!Title || !Caption)
		{
			return false;
		}

		Title->SetText(TitleText);
		StyleLightText(*Title, 14, Palette.InGame.TealDeep, TEXT("SemiBold"));
		Caption->SetText(CaptionText);
		StyleLightText(*Caption, 11, Palette.InGame.InkMuted, TEXT("Regular"),
			ETextJustify::Left, true);
		return AddVertical(Parent, *Title, FMargin(0.0f, 0.0f, 0.0f, 2.0f))
			&& AddVertical(Parent, *Caption, FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	bool BuildLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			return false;
		}

		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("Root_Canvas"));
		USizeBox* PanelSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Panel_Size"));
		UBorder* PanelBorder = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Panel"), true);
		UVerticalBox* Shell = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_Shell"));
		if (!Root || !PanelSize || !PanelBorder || !Shell)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		PanelSize->SetWidthOverride(PanelWidth);
		PanelSize->SetHeightOverride(PanelHeight);
		PaintBorder(*PanelBorder, MakeSolidBrush(
			Palette->InGame.Paper, 8.0f, Palette->InGame.LineStrong, 1.0f));
		PanelBorder->SetPadding(FMargin(6.0f));
		PanelBorder->AddChild(Shell);
		PanelSize->AddChild(PanelBorder);
		UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(PanelSize);
		if (!PanelSlot)
		{
			return false;
		}
		PanelSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		PanelSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		PanelSlot->SetPosition(FVector2D(PanelLeftMargin, PanelTopMargin));
		PanelSlot->SetSize(FVector2D(PanelWidth, PanelHeight));
		PanelSlot->SetAutoSize(false);

		UBorder* HeaderBorder = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Header"));
		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Panel_Header"));
		UVerticalBox* HeaderCopy = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_HeaderCopy"));
		UCommonTextBlock* Eyebrow = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("TXT_Eyebrow"));
		UCommonTextBlock* Title = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("TXT_Title"));
		UBorder* ShortcutChip = MakeChip(
			Blueprint, Tree, TEXT("Chip_Shortcut"), TEXT("TXT_Shortcut"),
			FText::FromString(TEXT("CTRL + G")), Palette->InGame.Teal,
			Palette->InGame.White, Palette->InGame.TealSoft);
		if (!HeaderBorder || !Header || !HeaderCopy || !Eyebrow || !Title || !ShortcutChip)
		{
			return false;
		}
		PaintBorder(*HeaderBorder, MakeSolidBrush(Palette->InGame.TealDeep, 6.0f));
		HeaderBorder->SetPadding(FMargin(16.0f, 11.0f));
		Eyebrow->SetText(FText::FromString(TEXT("DEVELOPER CONTROLS · TEST_MAIN")));
		StyleLightText(*Eyebrow, 10, Palette->InGame.TealSoft, TEXT("Medium"));
		Title->SetText(FText::FromString(TEXT("시설 운영 디버그")));
		StyleLightText(*Title, 20, Palette->InGame.White, TEXT("SemiBold"));
		AddVertical(*HeaderCopy, *Eyebrow, FMargin(0.0f, 0.0f, 0.0f, 2.0f));
		AddVertical(*HeaderCopy, *Title);
		AddHorizontal(*Header, *HeaderCopy, FMargin(0.0f), ESlateSizeRule::Fill);
		AddHorizontal(*Header, *ShortcutChip, FMargin(12.0f, 0.0f, 0.0f, 0.0f));
		HeaderBorder->AddChild(Header);
		AddVertical(*Shell, *HeaderBorder, FMargin(0.0f, 0.0f, 0.0f, 6.0f));

		UScrollBox* ControlsScroll = MakeWidget<UScrollBox>(Blueprint, Tree, TEXT("ScrollBox_Controls"));
		UVerticalBox* Sections = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_ControlSections"));
		if (!ControlsScroll || !Sections)
		{
			return false;
		}
		if (UScrollBoxSlot* Slot = Cast<UScrollBoxSlot>(ControlsScroll->AddChild(Sections)))
		{
			Slot->SetPadding(FMargin(6.0f, 4.0f, 10.0f, 4.0f));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}
		else
		{
			return false;
		}

		UBorder* StatusBorder = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_StatusSection"));
		UVerticalBox* Status = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_StatusSection"));
		UCommonTextBlock* StatusTitle = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("TXT_StatusSectionTitle"));
		UCommonTextBlock* Stage = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("TXT_Stage"), true);
		UCommonTextBlock* PhaseRevision = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("TXT_PhaseRevision"), true);
		UCommonTextBlock* FacilityCounts = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("TXT_FacilityCounts"), true);
		UCommonTextBlock* QueueCounts = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("TXT_QueueCounts"), true);
		if (!StatusBorder || !Status || !StatusTitle || !Stage || !PhaseRevision
			|| !FacilityCounts || !QueueCounts)
		{
			return false;
		}
		PaintBorder(*StatusBorder, MakeSolidBrush(
			Palette->InGame.PaperSunken, 6.0f, Palette->InGame.Line, 1.0f));
		StatusBorder->SetPadding(FMargin(12.0f, 10.0f));
		StatusTitle->SetText(FText::FromString(TEXT("현재 상태")));
		Stage->SetText(FText::FromString(TEXT("Festival Stage: -")));
		PhaseRevision->SetText(FText::FromString(TEXT("FFZS Phase: -  |  Revision: -")));
		FacilityCounts->SetText(FText::FromString(TEXT("시설 상태를 불러오는 중...")));
		QueueCounts->SetText(FText::FromString(TEXT("Queue 상태를 불러오는 중...")));
		StyleLightText(*StatusTitle, 12, Palette->InGame.InkMuted, TEXT("SemiBold"));
		StyleLightText(*Stage, 15, Palette->InGame.TealDeep, TEXT("SemiBold"), ETextJustify::Left, true);
		StyleLightText(*PhaseRevision, 11, Palette->InGame.InkMuted, TEXT("Medium"), ETextJustify::Left, true);
		StyleLightText(*FacilityCounts, 12, Palette->InGame.Ink, TEXT("Medium"), ETextJustify::Left, true);
		StyleLightText(*QueueCounts, 11, Palette->InGame.InkMuted, TEXT("Regular"), ETextJustify::Left, true);
		AddVertical(*Status, *StatusTitle, FMargin(0.0f, 0.0f, 0.0f, 5.0f));
		AddVertical(*Status, *Stage, FMargin(0.0f, 0.0f, 0.0f, 2.0f));
		AddVertical(*Status, *PhaseRevision, FMargin(0.0f, 0.0f, 0.0f, 7.0f));
		AddVertical(*Status, *FacilityCounts, FMargin(0.0f, 0.0f, 0.0f, 3.0f));
		AddVertical(*Status, *QueueCounts);
		StatusBorder->AddChild(Status);
		AddVertical(*Sections, *StatusBorder, FMargin(0.0f, 0.0f, 0.0f, 8.0f));

		UBorder* StageBorder = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_StageSection"));
		UVerticalBox* StageSection = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_StageSection"));
		UHorizontalBox* StageRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Stage_Button_Row"));
		if (!StageBorder || !StageSection || !StageRow)
		{
			return false;
		}
		PaintBorder(*StageBorder, MakeSolidBrush(
			Palette->InGame.Paper, 6.0f, Palette->InGame.Line, 1.0f));
		StageBorder->SetPadding(FMargin(12.0f));
		if (!AddSectionHeading(
				Blueprint, Tree, *StageSection,
				TEXT("TXT_StageSectionTitle"), TEXT("TXT_StageSectionCaption"),
				FText::FromString(TEXT("축제 단계")),
				FText::FromString(TEXT("실제 Festival Stage 전환 API를 호출합니다.")), *Palette)
			|| !AddButtonRow(*StageRow, {
				MakeLightButton(Blueprint, Tree, TEXT("BTN_Setup"), TEXT("TXT_BTN_Setup"),
					FText::FromString(TEXT("설치 준비")), Palette->InGame.PaperSunken,
					Palette->InGame.Hover, Palette->InGame.TealSoft, Palette->InGame.PaperSunken,
					Palette->InGame.TealDeep, Palette->InGame.Line),
				MakeLightButton(Blueprint, Tree, TEXT("BTN_MorningCheck"), TEXT("TXT_BTN_MorningCheck"),
					FText::FromString(TEXT("개장 점검")), Palette->InGame.PaperSunken,
					Palette->InGame.Hover, Palette->InGame.TealSoft, Palette->InGame.PaperSunken,
					Palette->InGame.TealDeep, Palette->InGame.Line),
				MakeLightButton(Blueprint, Tree, TEXT("BTN_Live"), TEXT("TXT_BTN_Live"),
					FText::FromString(TEXT("축제 운영")), Palette->InGame.PaperSunken,
					Palette->InGame.Hover, Palette->InGame.TealSoft, Palette->InGame.PaperSunken,
					Palette->InGame.TealDeep, Palette->InGame.Line),
				MakeLightButton(Blueprint, Tree, TEXT("BTN_Closing"), TEXT("TXT_BTN_Closing"),
					FText::FromString(TEXT("마감")), Palette->InGame.PaperSunken,
					Palette->InGame.Hover, Palette->InGame.TealSoft, Palette->InGame.PaperSunken,
					Palette->InGame.TealDeep, Palette->InGame.Line)}))
		{
			return false;
		}
		AddVertical(*StageSection, *StageRow);
		StageBorder->AddChild(StageSection);
		AddVertical(*Sections, *StageBorder, FMargin(0.0f, 0.0f, 0.0f, 8.0f));

		UBorder* LifecycleBorder = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_LifecycleSection"));
		UVerticalBox* LifecycleSection = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_LifecycleSection"));
		UHorizontalBox* LifecycleRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Lifecycle_Button_Row"));
		UHorizontalBox* UtilityRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Utility_Button_Row"));
		if (!LifecycleBorder || !LifecycleSection || !LifecycleRow || !UtilityRow)
		{
			return false;
		}
		PaintBorder(*LifecycleBorder, MakeSolidBrush(
			Palette->InGame.Paper, 6.0f, Palette->InGame.Line, 1.0f));
		LifecycleBorder->SetPadding(FMargin(12.0f));
		FLinearColor DangerHover = Palette->InGame.Danger * 0.86f;
		DangerHover.A = Palette->InGame.Danger.A;
		FLinearColor DangerPressed = Palette->InGame.Danger * 0.72f;
		DangerPressed.A = Palette->InGame.Danger.A;
		if (!AddSectionHeading(
				Blueprint, Tree, *LifecycleSection,
				TEXT("TXT_LifecycleSectionTitle"), TEXT("TXT_LifecycleSectionCaption"),
				FText::FromString(TEXT("시설 운영")),
				FText::FromString(TEXT("검증된 시설에 실제 개장·폐쇄 명령을 일괄 적용합니다.")), *Palette)
			|| !AddButtonRow(*LifecycleRow, {
				MakeLightButton(Blueprint, Tree, TEXT("BTN_OpenAll"), TEXT("TXT_BTN_OpenAll"),
					FText::FromString(TEXT("전체 개장")), Palette->InGame.Teal,
					Palette->InGame.TealDeep, Palette->InGame.TealDeep, Palette->InGame.LineStrong,
					Palette->InGame.White, Palette->InGame.TealDeep),
				MakeLightButton(Blueprint, Tree, TEXT("BTN_CloseAll"), TEXT("TXT_BTN_CloseAll"),
					FText::FromString(TEXT("정상 폐쇄")), Palette->InGame.PaperSunken,
					Palette->InGame.Hover, Palette->InGame.TealSoft, Palette->InGame.PaperSunken,
					Palette->InGame.TealDeep, Palette->InGame.Line)})
			|| !AddButtonRow(*UtilityRow, {
				MakeLightButton(Blueprint, Tree, TEXT("BTN_EmergencyCloseAll"),
					TEXT("TXT_BTN_EmergencyCloseAll"), FText::FromString(TEXT("긴급 전체 폐쇄")),
					Palette->InGame.Danger, DangerHover, DangerPressed, Palette->InGame.LineStrong,
					Palette->InGame.White, Palette->InGame.Danger),
				MakeLightButton(Blueprint, Tree, TEXT("BTN_Refresh"), TEXT("TXT_BTN_Refresh"),
					FText::FromString(TEXT("상태 새로고침")), Palette->InGame.PaperSunken,
					Palette->InGame.Hover, Palette->InGame.TealSoft, Palette->InGame.PaperSunken,
					Palette->InGame.TealDeep, Palette->InGame.Line)}))
		{
			return false;
		}
		AddVertical(*LifecycleSection, *LifecycleRow, FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		AddVertical(*LifecycleSection, *UtilityRow);
		LifecycleBorder->AddChild(LifecycleSection);
		AddVertical(*Sections, *LifecycleBorder);
		AddVertical(*Shell, *ControlsScroll, FMargin(0.0f), ESlateSizeRule::Fill, VAlign_Fill);

		UBorder* ResultBorder = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_LastResult"));
		UVerticalBox* Result = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_LastResult"));
		UCommonTextBlock* ResultLabel = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("TXT_LastResultLabel"));
		UCommonTextBlock* LastResult = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("TXT_LastResult"), true);
		if (!ResultBorder || !Result || !ResultLabel || !LastResult)
		{
			return false;
		}
		PaintBorder(*ResultBorder, MakeSolidBrush(
			Palette->InGame.StatusBar, 6.0f, Palette->InGame.Line, 1.0f));
		ResultBorder->SetPadding(FMargin(12.0f, 8.0f));
		ResultLabel->SetText(FText::FromString(TEXT("LAST RESULT")));
		LastResult->SetText(FText::FromString(TEXT("준비 완료")));
		StyleLightText(*ResultLabel, 10, Palette->InGame.Teal, TEXT("SemiBold"));
		StyleLightText(*LastResult, 12, Palette->InGame.Ink, TEXT("Medium"), ETextJustify::Left, true);
		AddVertical(*Result, *ResultLabel, FMargin(0.0f, 0.0f, 0.0f, 2.0f));
		AddVertical(*Result, *LastResult);
		ResultBorder->AddChild(Result);
		AddVertical(*Shell, *ResultBorder);
		return true;
	}

	UWidgetBlueprint* LoadExistingBlueprint()
	{
		return FFUIWbpBuilder::LoadExistingBlueprint(PanelPackage);
	}

	bool HasRequiredLayout(const UWidgetBlueprint& Blueprint)
	{
		const UWidgetTree* Tree = Blueprint.WidgetTree;
		if (!Tree)
		{
			return false;
		}
		for (const FName TextName : {
			FName(TEXT("TXT_Stage")), FName(TEXT("TXT_PhaseRevision")),
			FName(TEXT("TXT_FacilityCounts")), FName(TEXT("TXT_QueueCounts")),
			FName(TEXT("TXT_LastResult"))})
		{
			const UCommonTextBlock* Text = Cast<UCommonTextBlock>(Tree->FindWidget(TextName));
			if (!Text || !Text->bIsVariable)
			{
				return false;
			}
		}
		for (const FName ButtonName : {
			FName(TEXT("BTN_Setup")), FName(TEXT("BTN_MorningCheck")),
			FName(TEXT("BTN_Live")), FName(TEXT("BTN_Closing")),
			FName(TEXT("BTN_OpenAll")), FName(TEXT("BTN_CloseAll")),
			FName(TEXT("BTN_EmergencyCloseAll")), FName(TEXT("BTN_Refresh"))})
		{
			const UButton* Button = Cast<UButton>(Tree->FindWidget(ButtonName));
			if (!Button || !Button->bIsVariable)
			{
				return false;
			}
		}
		return Cast<UCanvasPanel>(Tree->FindWidget(TEXT("Root_Canvas")))
			&& Cast<USizeBox>(Tree->FindWidget(TEXT("Panel_Size")))
			&& Cast<UBorder>(Tree->FindWidget(TEXT("Border_Panel")))
			&& Cast<UScrollBox>(Tree->FindWidget(TEXT("ScrollBox_Controls")))
			&& Cast<UBorder>(Tree->FindWidget(TEXT("Border_StatusSection")))
			&& Cast<UBorder>(Tree->FindWidget(TEXT("Border_StageSection")))
			&& Cast<UBorder>(Tree->FindWidget(TEXT("Border_LifecycleSection")))
			&& Cast<UBorder>(Tree->FindWidget(TEXT("Border_LastResult")));
	}

	bool SaveBlueprint(UWidgetBlueprint& Blueprint)
	{
		return FFUIWbpBuilder::SaveNewBlueprint(TEXT("FF_ZONE_LIFECYCLE_TEST_UMG"), Blueprint);
	}

	void BuildZoneLifecycleTestPanel()
	{
		if (FPackageName::DoesPackageExist(PanelPackage))
		{
			UWidgetBlueprint* Existing = LoadExistingBlueprint();
			if (Existing && Existing->GeneratedClass
				&& Existing->GeneratedClass->IsChildOf(UFF_ZoneLifecycleTestPanel::StaticClass()))
			{
				UE_LOG(LogFFZoneLifecycleTestWbpBuilder, Display,
					TEXT("FF_ZONE_LIFECYCLE_TEST_UMG_REUSE: %s (designer changes preserved)"),
					*PanelPackage);
			}
			else
			{
				UE_LOG(LogFFZoneLifecycleTestWbpBuilder, Error,
					TEXT("FF_ZONE_LIFECYCLE_TEST_UMG_FAIL: invalid existing parent: %s"), *PanelPackage);
			}
			return;
		}

		UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
		Factory->ParentClass = UFF_ZoneLifecycleTestPanel::StaticClass();
		FAssetToolsModule& AssetTools =
			FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		const FString AssetName = FPackageName::GetLongPackageAssetName(PanelPackage);
		UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(AssetTools.Get().CreateAsset(
			AssetName, PanelFolder, UWidgetBlueprint::StaticClass(), Factory));
		if (!Blueprint || !Blueprint->WidgetTree
			|| !BuildLayout(*Blueprint, *Blueprint->WidgetTree)
			|| !HasRequiredLayout(*Blueprint)
			|| !SaveBlueprint(*Blueprint))
		{
			UE_LOG(LogFFZoneLifecycleTestWbpBuilder, Error,
				TEXT("FF_ZONE_LIFECYCLE_TEST_UMG_FAIL: could not create %s"), *PanelPackage);
			return;
		}
		UE_LOG(LogFFZoneLifecycleTestWbpBuilder, Display,
			TEXT("FF_ZONE_LIFECYCLE_TEST_UMG_CREATED: %s"), *PanelPackage);
	}

	bool RefreshWidgetTree(UWidgetBlueprint& Blueprint)
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
				*DiscardedName.ToString(), &Blueprint,
				REN_DontCreateRedirectors | REN_NonTransactional))
		{
			return false;
		}

		Blueprint.Modify();
		OriginalTree->SetFlags(RF_Transient);
		Blueprint.Bindings.Reset();
		Blueprint.WidgetVariableNameToGuidMap.Reset();
		Blueprint.WidgetTree = NewObject<UWidgetTree>(
			&Blueprint, UWidgetTree::StaticClass(), TEXT("WidgetTree"), RF_Transactional);

		auto Rollback = [&]()
		{
			if (UWidgetTree* FailedTree = Blueprint.WidgetTree)
			{
				const FName FailedName = MakeUniqueObjectName(
					&Blueprint, UWidgetTree::StaticClass(), TEXT("FailedWidgetTree"));
				FailedTree->Rename(
					*FailedName.ToString(), &Blueprint,
					REN_DontCreateRedirectors | REN_NonTransactional);
				FailedTree->SetFlags(RF_Transient);
			}
			if (!bOriginalTreeWasTransient)
			{
				OriginalTree->ClearFlags(RF_Transient);
			}
			OriginalTree->Rename(
				*OriginalTreeName.ToString(), &Blueprint,
				REN_DontCreateRedirectors | REN_NonTransactional);
			Blueprint.WidgetTree = OriginalTree;
			Blueprint.Bindings = OriginalBindings;
			Blueprint.WidgetVariableNameToGuidMap = OriginalVariableMap;
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&Blueprint);
			FKismetEditorUtilities::CompileBlueprint(&Blueprint);
		};

		if (!Blueprint.WidgetTree
			|| !BuildLayout(Blueprint, *Blueprint.WidgetTree)
			|| !HasRequiredLayout(Blueprint)
			|| !SaveBlueprint(Blueprint))
		{
			Rollback();
			return false;
		}
		return true;
	}

	void RefreshZoneLifecycleTestPanel()
	{
		if (!FPackageName::DoesPackageExist(PanelPackage))
		{
			BuildZoneLifecycleTestPanel();
			return;
		}

		UWidgetBlueprint* Existing = LoadExistingBlueprint();
		if (!Existing || !Existing->GeneratedClass
			|| !Existing->GeneratedClass->IsChildOf(UFF_ZoneLifecycleTestPanel::StaticClass()))
		{
			UE_LOG(LogFFZoneLifecycleTestWbpBuilder, Error,
				TEXT("FF_ZONE_LIFECYCLE_TEST_UMG_REFRESH_FAIL: parent contract mismatch: %s"),
				*PanelPackage);
			return;
		}

		if (!RefreshWidgetTree(*Existing))
		{
			UE_LOG(LogFFZoneLifecycleTestWbpBuilder, Error,
				TEXT("FF_ZONE_LIFECYCLE_TEST_UMG_REFRESH_FAIL: restored previous WidgetTree: %s"),
				*PanelPackage);
			return;
		}

		UE_LOG(LogFFZoneLifecycleTestWbpBuilder, Display,
			TEXT("FF_ZONE_LIFECYCLE_TEST_UMG_REFRESHED: LightTeal designer layout saved: %s"),
			*PanelPackage);
	}

	FAutoConsoleCommand BuildZoneLifecycleTestPanelCommand(
		TEXT("FF.ZoneLifecycleTestUMG"),
		TEXT("Creates the editable Test_Main facility lifecycle panel when absent."),
		FConsoleCommandDelegate::CreateStatic(&BuildZoneLifecycleTestPanel));

	FAutoConsoleCommand RefreshZoneLifecycleTestPanelCommand(
		TEXT("FF.RefreshZoneLifecycleTestUMG"),
		TEXT("Explicitly rebuilds the facility debug WidgetTree with the LightTeal template."),
		FConsoleCommandDelegate::CreateStatic(&RefreshZoneLifecycleTestPanel));
}

#endif // WITH_EDITOR
