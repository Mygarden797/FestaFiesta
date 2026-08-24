// FF_FrontendWbpBuilder.cpp

#include "CoreMinimal.h"

#if WITH_EDITOR

// 프론트엔드 화면의 WBP 골격을 만드는 에디터 전용 도구다. 콘솔 명령은 `FF.FrontendUMG`.
//
// **없는 WBP만 만든다.** 규칙과 함정은 FF_UIWbpBuilderCommon.h 상단에 있다.
//
// === 타이틀 버튼을 재사용하지 않는다 (2026-08-20 백종태 확인) ===
// `WBP_TitleMenuButton_GlassOrbit` 은 **임시 테스트 메뉴**의 버튼이고, 메인 메뉴는 컨셉에 맞게
// 다시 만든다. 그 버튼을 재사용하면 타이틀을 다시 만들 때 프론트엔드 화면 전부가 함께 깨진다.
// 그래서 `WBP_FF_MenuButton` 을 따로 만들고 색은 `UFF_UIPalette.Frontend` 에서 읽는다.
//
// === CommonButtonStyle(CBS_) 이 없다 ===
// `UCommonButtonBase` 는 배경 브러시를 스타일 에셋에 맡기는데 아직 그 에셋이 없다.
// 그래서 WBP 안의 `Border_Background` 가 배경을 그린다. 스타일 에셋이 생기면 그쪽으로 옮긴다.
//
// === 생성기가 하지 않는 것 ===
// 화면의 `RowWidgetClass` · `CardWidgetClass` · `ToggleRowClass` 는 **에디터에서 지정한다.**
// CDO 프로퍼티는 컴파일 뒤에야 존재해서 생성 흐름 안에서 넣기 어렵다.
// 빠뜨리면 화면이 비는데, 각 화면이 그때 경고를 남기도록 해 두었다.

#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/CheckBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/FF_CommonButtonBase.h"
#include "UI/Frontend/FF_ConfirmDialog.h"
#include "UI/Frontend/FF_OptionRowToggle.h"
#include "UI/Frontend/FF_OptionsScreen.h"
#include "UI/Frontend/FF_PlayHubScreen.h"
#include "UI/Frontend/FF_SaveListScreen.h"
#include "UI/Frontend/FF_SaveSlotCard.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

namespace FFFrontendWbpBuilder
{
	using namespace FFUIWbpBuilder;

	const TCHAR* const FrontendLogPrefix = TEXT("FF_FRONTEND_UMG");

	const FString FrontendFolder(TEXT("/Game/FestaFiesta/UI/Frontend"));
	const FString MenuButtonPackage(FrontendFolder / TEXT("WBP_FF_MenuButton"));
	const FString OptionRowTogglePackage(FrontendFolder / TEXT("WBP_FF_OptionRowToggle"));
	const FString SaveSlotCardPackage(FrontendFolder / TEXT("WBP_FF_SaveSlotCard"));
	const FString ConfirmDialogPackage(FrontendFolder / TEXT("WBP_FF_ConfirmDialog"));
	const FString PlayHubPackage(FrontendFolder / TEXT("WBP_FF_PlayHub"));
	const FString SaveListPackage(FrontendFolder / TEXT("WBP_FF_SaveList"));
	const FString OptionsPackage(FrontendFolder / TEXT("WBP_FF_Options"));

	// === 대략 크기 ===
	// 확정 좌표는 docs/ui/glass-orbit-unreal-spec.md 5절이다. 여기 값은 그 절의 규격을 따르되
	// 픽셀 단위로 맞추지 않는다 — 에디터에서 보면서 조절한다.
	constexpr float MenuColumnWidth = 620.f;
	constexpr float MenuButtonHeight = 120.f;   // 규격 5절: 버튼 620x120
	constexpr float MenuButtonGap = 14.f;       // 규격 5절: 간격 14
	constexpr float IconSize = 44.f;
	constexpr float ChevronSize = 24.f;
	constexpr float ScreenWidth = 1920.f;
	constexpr float ScreenHeight = 1080.f;

	/** 브러시 표시 크기를 박는다. 텍스처 기본값으로 두면 아이콘을 꽂는 순간 줄이 부푼다. */
	void LockImageSize(UImage& Image, const float Size)
	{
		FSlateBrush Brush = Image.GetBrush();
		Brush.SetImageSize(FVector2D(Size, Size));
		Image.SetBrush(Brush);
	}

	/** 포커스 링. 전역 RenderFocusRule=Never 라 직접 그린다 (C-024). 시작은 항상 숨김이다. */
	UImage* MakeFocusRing(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, const FLinearColor& Color)
	{
		UImage* Ring = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_FocusRing"), true);
		if (Ring)
		{
			Ring->SetColorAndOpacity(Color);

			// Collapsed 가 아니라 Hidden 이다. 링은 겹쳐 그리는 장식이라 자리를 차지하지 않고,
			// Collapsed 로 껐다 켜면 레이아웃이 흔들린다.
			Ring->SetVisibility(ESlateVisibility::Hidden);
		}
		return Ring;
	}

	// =========================================================================
	// WBP_FF_MenuButton — 프론트엔드 메뉴 버튼. 타이틀 버튼과 별개다
	// =========================================================================
	bool BuildMenuButton(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 팔레트를 읽을 수 없어 WBP_FF_MenuButton 을 만들지 않았다."), FrontendLogPrefix);
			return false;
		}

		USizeBox* Root = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Root"));
		UOverlay* Overlay = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Root"));
		UBorder* Background = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Background"));
		UHorizontalBox* Row = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Content"));
		USizeBox* IconBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Icon"));
		USizeBox* ChevronBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Chevron"));

		// UFF_CommonButtonBase 의 바인딩 이름과 정확히 같아야 한다.
		// Text_Label 은 UCommonTextBlock 이어야 한다 — UTextBlock 을 넣으면 바인딩이 실패한다.
		UCommonTextBlock* Label = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_Label"), true);
		UImage* Icon = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Icon"), true);
		UImage* Chevron = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Chevron"), true);
		UImage* FocusRing = MakeFocusRing(Blueprint, Tree, Palette->Frontend.EdgeStrong);

		if (!Root || !Overlay || !Background || !Row || !IconBox || !ChevronBox
			|| !Label || !Icon || !Chevron || !FocusRing)
		{
			return false;
		}

		Tree.RootWidget = Root;

		Root->SetWidthOverride(MenuColumnWidth);
		Root->SetHeightOverride(MenuButtonHeight);
		Root->AddChild(Overlay);

		// CBS_ 스타일 에셋이 없어 배경을 여기서 그린다. 에셋이 생기면 그쪽으로 옮긴다.
		Background->SetBrushColor(Palette->Frontend.Glass);
		Background->SetPadding(FMargin(34.f, 0.f, 34.f, 0.f));
		Background->AddChild(Row);
		if (UOverlaySlot* BackgroundSlot = Overlay->AddChildToOverlay(Background))
		{
			BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
			BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
		}

		LockImageSize(*Icon, IconSize);
		IconBox->SetWidthOverride(IconSize);
		IconBox->SetHeightOverride(IconSize);
		IconBox->AddChild(Icon);
		if (UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(IconBox))
		{
			IconSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			IconSlot->SetVerticalAlignment(VAlign_Center);
			IconSlot->SetPadding(FMargin(0.f, 0.f, 26.f, 0.f));
		}

		// 문구는 화면 코드가 SetButtonLabel 로 넣는다. 여기 값은 디자이너용 자리표시자다.
		Label->SetText(LOCTEXT("MenuButtonPlaceholder", "메뉴 항목"));
		if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label))
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}

		LockImageSize(*Chevron, ChevronSize);
		ChevronBox->SetWidthOverride(ChevronSize);
		ChevronBox->SetHeightOverride(ChevronSize);
		ChevronBox->AddChild(Chevron);
		if (UHorizontalBoxSlot* ChevronSlot = Row->AddChildToHorizontalBox(ChevronBox))
		{
			ChevronSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			ChevronSlot->SetVerticalAlignment(VAlign_Center);
		}

		// 링은 배경 위에 겹친다. 입력을 먹지 않게 HitTestInvisible 로 둔다.
		if (UOverlaySlot* RingSlot = Overlay->AddChildToOverlay(FocusRing))
		{
			RingSlot->SetHorizontalAlignment(HAlign_Fill);
			RingSlot->SetVerticalAlignment(VAlign_Fill);
		}

		return true;
	}

	// =========================================================================
	// WBP_FF_OptionRowToggle — 옵션의 켜고/끄는 설정 한 줄
	// =========================================================================
	bool BuildOptionRowToggle(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 팔레트를 읽을 수 없어 WBP_FF_OptionRowToggle 을 만들지 않았다."), FrontendLogPrefix);
			return false;
		}

		USizeBox* Root = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Root"));
		UOverlay* Overlay = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Root"));
		UHorizontalBox* Row = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Content"));
		UVerticalBox* TextColumn = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Text"));

		UTextBlock* Label = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Label"), true);
		UTextBlock* Description = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Description"), true);
		UCheckBox* Value = MakeWidget<UCheckBox>(Blueprint, Tree, TEXT("CheckBox_Value"), true);
		UImage* FocusRing = MakeFocusRing(Blueprint, Tree, Palette->Frontend.EdgeStrong);

		if (!Root || !Overlay || !Row || !TextColumn || !Label || !Description || !Value || !FocusRing)
		{
			return false;
		}

		Tree.RootWidget = Root;

		// 폭은 우측 영역에 맞춰 늘어나야 하므로 높이만 고정한다.
		Root->SetHeightOverride(72.f);
		Root->AddChild(Overlay);

		if (UOverlaySlot* RowSlot = Overlay->AddChildToOverlay(Row))
		{
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
			RowSlot->SetVerticalAlignment(VAlign_Fill);
			RowSlot->SetPadding(FMargin(20.f, 8.f, 20.f, 8.f));
		}

		StyleText(*Label, 18, Palette->Frontend.Text, TEXT("SemiBold"));
		Label->SetText(LOCTEXT("OptionRowLabelPlaceholder", "설정 이름"));
		if (UVerticalBoxSlot* LabelSlot = TextColumn->AddChildToVerticalBox(Label))
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}

		// 이름만으로는 무엇이 바뀌는지 알 수 없다. 켜면 무슨 일이 생기는지 적는 자리다.
		StyleText(*Description, 13, Palette->Frontend.Sub, TEXT("Medium"));
		Description->SetText(LOCTEXT("OptionRowDescPlaceholder", "이 설정이 무엇을 바꾸는지"));
		if (UVerticalBoxSlot* DescSlot = TextColumn->AddChildToVerticalBox(Description))
		{
			DescSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			DescSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
		}

		if (UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(TextColumn))
		{
			TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			TextSlot->SetVerticalAlignment(VAlign_Center);
		}

		if (UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(Value))
		{
			ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			ValueSlot->SetVerticalAlignment(VAlign_Center);
		}

		if (UOverlaySlot* RingSlot = Overlay->AddChildToOverlay(FocusRing))
		{
			RingSlot->SetHorizontalAlignment(HAlign_Fill);
			RingSlot->SetVerticalAlignment(VAlign_Fill);
		}

		return true;
	}

	// =========================================================================
	// WBP_FF_SaveSlotCard — 저장 기록 한 건
	// =========================================================================
	bool BuildSaveSlotCard(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 팔레트를 읽을 수 없어 WBP_FF_SaveSlotCard 을 만들지 않았다."), FrontendLogPrefix);
			return false;
		}

		USizeBox* Root = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Root"));
		UOverlay* Overlay = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Root"));
		UBorder* Card = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Card"));
		UHorizontalBox* Row = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Content"));
		USizeBox* KindIconBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_KindIcon"));
		UVerticalBox* InfoColumn = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Info"));
		UVerticalBox* MetaColumn = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Meta"));

		UTextBlock* FestivalName = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_FestivalName"), true);
		UTextBlock* Region = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Region"), true);
		UTextBlock* DayStage = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_DayStage"), true);
		UTextBlock* SavedAt = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_SavedAt"), true);
		UTextBlock* Kind = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Kind"), true);
		UImage* KindIcon = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_KindIcon"), true);
		UImage* FocusRing = MakeFocusRing(Blueprint, Tree, Palette->Frontend.EdgeStrong);

		if (!Root || !Overlay || !Card || !Row || !KindIconBox || !InfoColumn || !MetaColumn
			|| !FestivalName || !Region || !DayStage || !SavedAt || !Kind || !KindIcon || !FocusRing)
		{
			return false;
		}

		Tree.RootWidget = Root;

		Root->SetHeightOverride(112.f);
		Root->AddChild(Overlay);

		Card->SetBrushColor(Palette->Frontend.Glass);
		Card->SetPadding(FMargin(20.f, 14.f, 20.f, 14.f));
		Card->AddChild(Row);
		if (UOverlaySlot* CardSlot = Overlay->AddChildToOverlay(Card))
		{
			CardSlot->SetHorizontalAlignment(HAlign_Fill);
			CardSlot->SetVerticalAlignment(VAlign_Fill);
		}

		// 자동/수동 아이콘. 흑백으로 뽑았을 때 실루엣이 달라야 한다 (AT-011).
		LockImageSize(*KindIcon, 28.f);
		KindIconBox->SetWidthOverride(28.f);
		KindIconBox->SetHeightOverride(28.f);
		KindIconBox->AddChild(KindIcon);
		if (UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(KindIconBox))
		{
			IconSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			IconSlot->SetVerticalAlignment(VAlign_Center);
			IconSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
		}

		StyleText(*FestivalName, 20, Palette->Frontend.Title, TEXT("SemiBold"));
		FestivalName->SetText(LOCTEXT("SaveCardNamePlaceholder", "축제 이름"));
		if (UVerticalBoxSlot* NameSlot = InfoColumn->AddChildToVerticalBox(FestivalName))
		{
			NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}

		StyleText(*DayStage, 14, Palette->Frontend.Sub, TEXT("Medium"));
		DayStage->SetText(LOCTEXT("SaveCardDayStagePlaceholder", "Day 1 · 셋업"));
		if (UVerticalBoxSlot* DayStageSlot = InfoColumn->AddChildToVerticalBox(DayStage))
		{
			DayStageSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			DayStageSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}

		if (UHorizontalBoxSlot* InfoSlot = Row->AddChildToHorizontalBox(InfoColumn))
		{
			InfoSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			InfoSlot->SetVerticalAlignment(VAlign_Center);
		}

		// 자동/수동은 **문구**가 주된 표현이다. 색은 보조다.
		StyleText(*Kind, 13, Palette->Frontend.Text, TEXT("SemiBold"));
		Kind->SetText(LOCTEXT("SaveCardKindPlaceholder", "자동 저장"));
		if (UVerticalBoxSlot* KindSlot = MetaColumn->AddChildToVerticalBox(Kind))
		{
			KindSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			KindSlot->SetHorizontalAlignment(HAlign_Right);
		}

		StyleText(*Region, 13, Palette->Frontend.Dim, TEXT("Medium"));
		Region->SetText(LOCTEXT("SaveCardRegionPlaceholder", "지역"));
		if (UVerticalBoxSlot* RegionSlot = MetaColumn->AddChildToVerticalBox(Region))
		{
			RegionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			RegionSlot->SetHorizontalAlignment(HAlign_Right);
			RegionSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}

		StyleText(*SavedAt, 13, Palette->Frontend.Dim, TEXT("Medium"));
		SavedAt->SetText(LOCTEXT("SaveCardSavedAtPlaceholder", "저장 시각"));
		if (UVerticalBoxSlot* SavedAtSlot = MetaColumn->AddChildToVerticalBox(SavedAt))
		{
			SavedAtSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			SavedAtSlot->SetHorizontalAlignment(HAlign_Right);
		}

		if (UHorizontalBoxSlot* MetaSlot = Row->AddChildToHorizontalBox(MetaColumn))
		{
			MetaSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			MetaSlot->SetVerticalAlignment(VAlign_Center);
		}

		if (UOverlaySlot* RingSlot = Overlay->AddChildToOverlay(FocusRing))
		{
			RingSlot->SetHorizontalAlignment(HAlign_Fill);
			RingSlot->SetVerticalAlignment(VAlign_Fill);
		}

		return true;
	}

	// =========================================================================
	// WBP_FF_ConfirmDialog — 되돌릴 수 없는 행동 전용
	// =========================================================================
	bool BuildConfirmDialog(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, UClass* ButtonClass)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette || !ButtonClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 팔레트 또는 버튼 클래스가 없어 WBP_FF_ConfirmDialog 을 만들지 않았다."),
				FrontendLogPrefix);
			return false;
		}

		UOverlay* Root = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Root"));
		UBorder* Scrim = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Scrim"));
		USizeBox* DialogBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Dialog"));
		UBorder* Dialog = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Dialog"));
		UVerticalBox* Column = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Root"));
		UHorizontalBox* Buttons = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Buttons"));

		UTextBlock* Title = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Title"), true);
		UTextBlock* Body = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Body"), true);
		UVerticalBox* Rows = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Rows"), true);
		UUserWidget* Cancel = MakeWidgetOfClass(Blueprint, Tree, TEXT("Button_Cancel"), ButtonClass);
		UUserWidget* Confirm = MakeWidgetOfClass(Blueprint, Tree, TEXT("Button_Confirm"), ButtonClass);

		if (!Root || !Scrim || !DialogBox || !Dialog || !Column || !Buttons
			|| !Title || !Body || !Rows || !Cancel || !Confirm)
		{
			return false;
		}

		Tree.RootWidget = Root;

		// 뒤를 덮어 지금 무엇을 물어보는지에 시선을 모은다. 확인창은 시간이 멈추는 곳이 아니지만
		// 프론트엔드에서만 쓰므로 시뮬레이션과 무관하다.
		Scrim->SetBrushColor(Palette->Frontend.Glass);
		if (UOverlaySlot* ScrimSlot = Root->AddChildToOverlay(Scrim))
		{
			ScrimSlot->SetHorizontalAlignment(HAlign_Fill);
			ScrimSlot->SetVerticalAlignment(VAlign_Fill);
		}

		DialogBox->SetWidthOverride(720.f);
		DialogBox->AddChild(Dialog);
		if (UOverlaySlot* DialogSlot = Root->AddChildToOverlay(DialogBox))
		{
			DialogSlot->SetHorizontalAlignment(HAlign_Center);
			DialogSlot->SetVerticalAlignment(VAlign_Center);
		}

		Dialog->SetBrushColor(Palette->Frontend.GlassRaised);
		Dialog->SetPadding(FMargin(40.f, 36.f, 40.f, 32.f));
		Dialog->AddChild(Column);

		StyleText(*Title, 28, Palette->Frontend.Title, TEXT("SemiBold"));
		Title->SetText(LOCTEXT("ConfirmTitlePlaceholder", "확인이 필요합니다"));
		if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title))
		{
			TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}

		// 되돌릴 수 없다는 사실과 그 결과를 적는 자리다. 비우면 확인창의 의미가 없다.
		StyleText(*Body, 16, Palette->Frontend.Sub, TEXT("Medium"));
		Body->SetText(LOCTEXT("ConfirmBodyPlaceholder", "이 행동은 되돌릴 수 없습니다"));
		Body->SetAutoWrapText(true);
		if (UVerticalBoxSlot* BodySlot = Column->AddChildToVerticalBox(Body))
		{
			BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			BodySlot->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
		}

		// 수치 표. 행이 없으면 화면 코드가 이 컨테이너를 숨긴다.
		if (UVerticalBoxSlot* RowsSlot = Column->AddChildToVerticalBox(Rows))
		{
			RowsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			RowsSlot->SetPadding(FMargin(0.f, 20.f, 0.f, 20.f));
		}

		// 취소가 **왼쪽**이다. 기본 포커스도 취소이며 그건 C++ 이 강제한다.
		if (UHorizontalBoxSlot* CancelSlot = Buttons->AddChildToHorizontalBox(Cancel))
		{
			CancelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			CancelSlot->SetPadding(FMargin(0.f, 0.f, MenuButtonGap, 0.f));
		}
		if (UHorizontalBoxSlot* ConfirmSlot = Buttons->AddChildToHorizontalBox(Confirm))
		{
			ConfirmSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		if (UVerticalBoxSlot* ButtonsSlot = Column->AddChildToVerticalBox(Buttons))
		{
			ButtonsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}

		return true;
	}

	// =========================================================================
	// WBP_FF_PlayHub — 플레이 안에서 무엇을 할지 고른다
	// =========================================================================
	bool BuildPlayHub(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, UClass* ButtonClass)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette || !ButtonClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 팔레트 또는 버튼 클래스가 없어 WBP_FF_PlayHub 을 만들지 않았다."), FrontendLogPrefix);
			return false;
		}

		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Root"));
		UVerticalBox* Menu = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Menu"));
		UHorizontalBox* LockRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Lock"));
		USizeBox* LockIconBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_LockIcon"));
		UTextBlock* ScreenTitle = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_ScreenTitle"));

		UUserWidget* Continue = MakeWidgetOfClass(Blueprint, Tree, TEXT("Button_Continue"), ButtonClass);
		UUserWidget* NewFestival = MakeWidgetOfClass(Blueprint, Tree, TEXT("Button_NewFestival"), ButtonClass);
		UUserWidget* SaveList = MakeWidgetOfClass(Blueprint, Tree, TEXT("Button_SaveList"), ButtonClass);
		UUserWidget* Back = MakeWidgetOfClass(Blueprint, Tree, TEXT("Button_Back"), ButtonClass);
		UTextBlock* LockedNotice = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_LockedNotice"), true);
		UImage* LockedIcon = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_LockedIcon"), true);
		UVerticalBox* Recent = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_Recent"), true);
		UTextBlock* RecentEmpty = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_RecentEmpty"), true);

		if (!Root || !Menu || !LockRow || !LockIconBox || !ScreenTitle
			|| !Continue || !NewFestival || !SaveList || !Back
			|| !LockedNotice || !LockedIcon || !Recent || !RecentEmpty)
		{
			return false;
		}

		Tree.RootWidget = Root;

		// 좌표는 docs/ui/glass-orbit-unreal-spec.md 5절 기준이다 (브랜드 X152 Y96 · 메뉴 X152 Y520).
		StyleText(*ScreenTitle, 44, Palette->Frontend.Title, TEXT("SemiBold"));
		ScreenTitle->SetText(LOCTEXT("PlayHubScreenTitle", "플레이"));
		PlaceOnCanvas(*Root, *ScreenTitle, 152.f, 96.f, MenuColumnWidth, 80.f);

		// 메뉴 3개 + 잠금 안내. 잠금은 자물쇠 자리 + 문구를 함께 쓴다 (AT-011).
		if (UVerticalBoxSlot* ContinueSlot = Menu->AddChildToVerticalBox(Continue))
		{
			ContinueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			ContinueSlot->SetPadding(FMargin(0.f, 0.f, 0.f, MenuButtonGap));
		}

		LockImageSize(*LockedIcon, 24.f);
		LockIconBox->SetWidthOverride(24.f);
		LockIconBox->SetHeightOverride(24.f);
		LockIconBox->AddChild(LockedIcon);
		if (UHorizontalBoxSlot* LockIconSlot = LockRow->AddChildToHorizontalBox(LockIconBox))
		{
			LockIconSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			LockIconSlot->SetVerticalAlignment(VAlign_Center);
			LockIconSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
		}

		StyleText(*LockedNotice, 14, Palette->Frontend.Dim, TEXT("Medium"));
		LockedNotice->SetText(LOCTEXT("PlayHubLockPlaceholder", "기록 없음"));
		if (UHorizontalBoxSlot* NoticeSlot = LockRow->AddChildToHorizontalBox(LockedNotice))
		{
			NoticeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			NoticeSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UVerticalBoxSlot* LockRowSlot = Menu->AddChildToVerticalBox(LockRow))
		{
			LockRowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			LockRowSlot->SetPadding(FMargin(34.f, 0.f, 0.f, MenuButtonGap));
		}

		if (UVerticalBoxSlot* NewSlot = Menu->AddChildToVerticalBox(NewFestival))
		{
			NewSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			NewSlot->SetPadding(FMargin(0.f, 0.f, 0.f, MenuButtonGap));
		}
		if (UVerticalBoxSlot* SaveListSlot = Menu->AddChildToVerticalBox(SaveList))
		{
			SaveListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}

		PlaceOnCanvas(*Root, *Menu, 152.f, 420.f, MenuColumnWidth, 520.f);
		PlaceOnCanvas(*Root, *Back, 152.f, 960.f, MenuColumnWidth, MenuButtonHeight);

		// 우측 최근 기록 자리. 기록이 없으면 화면 코드가 숨기고 빈 상태 문구만 남긴다.
		PlaceOnCanvas(*Root, *Recent, 920.f, 420.f, 840.f, 520.f);

		StyleText(*RecentEmpty, 18, Palette->Frontend.Dim, TEXT("Medium"));
		RecentEmpty->SetText(LOCTEXT("PlayHubRecentEmptyPlaceholder", "저장 기록이 없습니다"));
		PlaceOnCanvas(*Root, *RecentEmpty, 920.f, 420.f, 840.f, 60.f);

		return true;
	}

	// =========================================================================
	// WBP_FF_SaveList — 자동/수동 그룹으로 나눈 저장 기록 목록
	// =========================================================================
	bool BuildSaveList(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, UClass* ButtonClass)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette || !ButtonClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 팔레트 또는 버튼 클래스가 없어 WBP_FF_SaveList 을 만들지 않았다."), FrontendLogPrefix);
			return false;
		}

		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Root"));
		UTextBlock* ScreenTitle = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_ScreenTitle"));
		UScrollBox* List = MakeWidget<UScrollBox>(Blueprint, Tree, TEXT("ScrollBox_List"));
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Content"));
		UTextBlock* AutoHeader = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_AutoHeader"));
		UTextBlock* ManualHeader = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_ManualHeader"));

		UVerticalBox* AutoGroup = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_AutoGroup"), true);
		UVerticalBox* ManualGroup = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_ManualGroup"), true);
		UVerticalBox* AutoBox = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Auto"), true);
		UVerticalBox* ManualBox = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Manual"), true);
		UTextBlock* Empty = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Empty"), true);
		UTextBlock* Notice = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_SaveSystemNotice"), true);
		UUserWidget* Back = MakeWidgetOfClass(Blueprint, Tree, TEXT("Button_Back"), ButtonClass);

		if (!Root || !ScreenTitle || !List || !Content || !AutoHeader || !ManualHeader
			|| !AutoGroup || !ManualGroup || !AutoBox || !ManualBox || !Empty || !Notice || !Back)
		{
			return false;
		}

		Tree.RootWidget = Root;

		StyleText(*ScreenTitle, 44, Palette->Frontend.Title, TEXT("SemiBold"));
		ScreenTitle->SetText(LOCTEXT("SaveListScreenTitle", "저장 기록"));
		PlaceOnCanvas(*Root, *ScreenTitle, 152.f, 96.f, MenuColumnWidth, 80.f);

		// 저장 기능이 없다는 사실을 목록 위에 둔다. 아래에 두면 목록에 가려 안 읽힌다.
		StyleText(*Notice, 16, Palette->InGame.Amber, TEXT("SemiBold"));
		Notice->SetText(LOCTEXT("SaveListNoticePlaceholder", "저장 기능 준비 중"));
		PlaceOnCanvas(*Root, *Notice, 152.f, 184.f, 1200.f, 44.f);

		// --- 자동 저장 그룹 ---
		StyleText(*AutoHeader, 20, Palette->Frontend.Sub, TEXT("SemiBold"));
		AutoHeader->SetText(LOCTEXT("SaveListAutoHeader", "자동 저장"));
		if (UVerticalBoxSlot* HeaderSlot = AutoGroup->AddChildToVerticalBox(AutoHeader))
		{
			HeaderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			HeaderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		}
		if (UVerticalBoxSlot* AutoBoxSlot = AutoGroup->AddChildToVerticalBox(AutoBox))
		{
			AutoBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
		if (UVerticalBoxSlot* AutoGroupSlot = Content->AddChildToVerticalBox(AutoGroup))
		{
			AutoGroupSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			AutoGroupSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 28.f));
		}

		// --- 수동 저장 그룹 ---
		StyleText(*ManualHeader, 20, Palette->Frontend.Sub, TEXT("SemiBold"));
		ManualHeader->SetText(LOCTEXT("SaveListManualHeader", "수동 저장"));
		if (UVerticalBoxSlot* HeaderSlot = ManualGroup->AddChildToVerticalBox(ManualHeader))
		{
			HeaderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			HeaderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		}
		if (UVerticalBoxSlot* ManualBoxSlot = ManualGroup->AddChildToVerticalBox(ManualBox))
		{
			ManualBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
		if (UVerticalBoxSlot* ManualGroupSlot = Content->AddChildToVerticalBox(ManualGroup))
		{
			ManualGroupSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}

		List->AddChild(Content);
		PlaceOnCanvas(*Root, *List, 152.f, 250.f, 1200.f, 640.f);

		StyleText(*Empty, 18, Palette->Frontend.Dim, TEXT("Medium"));
		Empty->SetText(LOCTEXT("SaveListEmptyPlaceholder", "저장 기록이 없습니다"));
		PlaceOnCanvas(*Root, *Empty, 152.f, 250.f, 1200.f, 60.f);

		PlaceOnCanvas(*Root, *Back, 152.f, 930.f, MenuColumnWidth, MenuButtonHeight);

		return true;
	}

	// =========================================================================
	// WBP_FF_Options — 좌측 분류 6개 + 우측 설정 행
	// =========================================================================
	bool BuildOptions(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, UClass* ButtonClass)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette || !ButtonClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 팔레트 또는 버튼 클래스가 없어 WBP_FF_Options 을 만들지 않았다."), FrontendLogPrefix);
			return false;
		}

		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Root"));
		UTextBlock* ScreenTitle = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_ScreenTitle"));
		UVerticalBox* Categories = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Categories"));

		// 분류 버튼을 **런타임에 만들지 않는다.** 컨테이너에 런타임으로 채우면 디자이너에서
		// 하나도 보이지 않아 위치·크기를 손으로 만질 수 없다 (Live HUD 탭 9개에서 겪은 문제).
		const TCHAR* CategoryNames[] = {
			TEXT("Button_Language"), TEXT("Button_Accessibility"), TEXT("Button_Sound"),
			TEXT("Button_Graphics"), TEXT("Button_Controls"), TEXT("Button_Credits")
		};

		UTextBlock* CategoryTitle = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CategoryTitle"), true);
		UVerticalBox* Rows = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Rows"), true);
		UTextBlock* NotImplemented = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_NotImplemented"), true);
		UUserWidget* Back = MakeWidgetOfClass(Blueprint, Tree, TEXT("Button_Back"), ButtonClass);

		if (!Root || !ScreenTitle || !Categories || !CategoryTitle || !Rows || !NotImplemented || !Back)
		{
			return false;
		}

		Tree.RootWidget = Root;

		StyleText(*ScreenTitle, 44, Palette->Frontend.Title, TEXT("SemiBold"));
		ScreenTitle->SetText(LOCTEXT("OptionsScreenTitle", "옵션"));
		PlaceOnCanvas(*Root, *ScreenTitle, 152.f, 96.f, MenuColumnWidth, 80.f);

		for (const TCHAR* CategoryName : CategoryNames)
		{
			UUserWidget* Button = MakeWidgetOfClass(Blueprint, Tree, FName(CategoryName), ButtonClass);
			if (!Button)
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_FAIL: 분류 버튼 %s 생성 실패."), FrontendLogPrefix, CategoryName);
				return false;
			}

			if (UVerticalBoxSlot* Slot = Categories->AddChildToVerticalBox(Button))
			{
				Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				Slot->SetPadding(FMargin(0.f, 0.f, 0.f, MenuButtonGap));
			}
		}

		// 6개 x (120 + 14) = 804. Y200 에서 시작해 1004 에 끝난다. 1080 에 겨우 들어가므로
		// 여기가 가장 먼저 조절이 필요한 곳이다.
		PlaceOnCanvas(*Root, *Categories, 152.f, 200.f, MenuColumnWidth, 810.f);

		StyleText(*CategoryTitle, 28, Palette->Frontend.Title, TEXT("SemiBold"));
		CategoryTitle->SetText(LOCTEXT("OptionsCategoryPlaceholder", "접근성"));
		PlaceOnCanvas(*Root, *CategoryTitle, 860.f, 200.f, 900.f, 60.f);

		PlaceOnCanvas(*Root, *Rows, 860.f, 290.f, 900.f, 600.f);

		// 만져도 아무 일이 없는 조작 UI 는 고장으로 보인다. 이유를 화면에 적는다.
		StyleText(*NotImplemented, 16, Palette->Frontend.Dim, TEXT("Medium"));
		NotImplemented->SetText(LOCTEXT("OptionsNotImplPlaceholder", "반영 사항 없음"));
		PlaceOnCanvas(*Root, *NotImplemented, 860.f, 290.f, 900.f, 60.f);

		PlaceOnCanvas(*Root, *Back, 860.f, 930.f, MenuColumnWidth, MenuButtonHeight);

		return true;
	}

	void RunFrontendUmg()
	{
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_BEGIN: 없는 WBP만 생성한다."), FrontendLogPrefix);

		// 버튼이 먼저다. 화면들이 그 GeneratedClass 를 자식으로 넣는다.
		UWidgetBlueprint* MenuButton = EnsureWidgetBlueprint(
			FrontendLogPrefix, MenuButtonPackage, FrontendFolder,
			*UFF_CommonButtonBase::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree) { return BuildMenuButton(Blueprint, Tree); });

		if (!MenuButton || !MenuButton->GeneratedClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_ABORT: WBP_FF_MenuButton 이 없어 화면을 만들 수 없다."), FrontendLogPrefix);
			return;
		}

		UClass* ButtonClass = MenuButton->GeneratedClass.Get();

		// 행·카드는 화면과 독립이다. 화면의 RowWidgetClass 지정은 에디터에서 한다.
		EnsureWidgetBlueprint(
			FrontendLogPrefix, OptionRowTogglePackage, FrontendFolder,
			*UFF_OptionRowToggle::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree) { return BuildOptionRowToggle(Blueprint, Tree); });

		EnsureWidgetBlueprint(
			FrontendLogPrefix, SaveSlotCardPackage, FrontendFolder,
			*UFF_SaveSlotCard::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree) { return BuildSaveSlotCard(Blueprint, Tree); });

		EnsureWidgetBlueprint(
			FrontendLogPrefix, ConfirmDialogPackage, FrontendFolder,
			*UFF_ConfirmDialog::StaticClass(),
			[ButtonClass](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{ return BuildConfirmDialog(Blueprint, Tree, ButtonClass); });

		EnsureWidgetBlueprint(
			FrontendLogPrefix, PlayHubPackage, FrontendFolder,
			*UFF_PlayHubScreen::StaticClass(),
			[ButtonClass](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{ return BuildPlayHub(Blueprint, Tree, ButtonClass); });

		EnsureWidgetBlueprint(
			FrontendLogPrefix, SaveListPackage, FrontendFolder,
			*UFF_SaveListScreen::StaticClass(),
			[ButtonClass](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{ return BuildSaveList(Blueprint, Tree, ButtonClass); });

		EnsureWidgetBlueprint(
			FrontendLogPrefix, OptionsPackage, FrontendFolder,
			*UFF_OptionsScreen::StaticClass(),
			[ButtonClass](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{ return BuildOptions(Blueprint, Tree, ButtonClass); });

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_END: 끝났다. 에디터에서 해야 하는 것 — "
				 "WBP_FF_SaveList 의 CardWidgetClass, WBP_FF_Options 의 ToggleRowClass, "
				 "WBP_FF_ConfirmDialog 의 RowWidgetClass 를 지정한다."), FrontendLogPrefix);
	}

	FAutoConsoleCommand FrontendUmgCommand(
		TEXT("FF.FrontendUMG"),
		TEXT("프론트엔드 WBP 7종(MenuButton / OptionRowToggle / SaveSlotCard / ConfirmDialog / "
			 "PlayHub / SaveList / Options)을 만든다. 이미 있는 에셋은 건드리지 않는다."),
		FConsoleCommandDelegate::CreateStatic(&RunFrontendUmg));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
