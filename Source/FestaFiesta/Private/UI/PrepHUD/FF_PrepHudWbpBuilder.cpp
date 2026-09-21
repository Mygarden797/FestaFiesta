// FF_PrepHudWbpBuilder.cpp

#include "CoreMinimal.h"

#if WITH_EDITOR

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
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/PrepHUD/FF_PrepHudBar.h"
#include "UI/PrepHUD/FF_PrepHudTabButton.h"
#include "UI/PrepHUD/FF_PrepPanelBase.h"
#include "UI/PrepHUD/Panels/FF_PrepChoiceButton.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelCheck.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelProgram.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelPromotion.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelStaff.h"
#include "UI/PrepHUD/Panels/FF_PrepProgramCard.h"
#include "UI/PrepHUD/Panels/FF_PrepPromotionCard.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPrepHudWbpBuilder"

/**
 * 준비 단계 HUD 의 WBP 골격 생성기 (U4).
 *
 * === 생성기를 쓰는 이유 ===
 * 탭 6개가 **같은 구조의 반복**이고, `BindWidget` 대상이 20개가 넘어 손으로 만들면 이름 하나가
 * 어긋나도 조용히 빈 칸으로 끝난다. 반대로 라이브 HUD 는 항목마다 위치가 달라 생성기를
 * 폐기했다 — 판단 기준표는 `docs/memory/wbp-codegen-workflow.md` 에 있다.
 *
 * === 여기까지가 생성기 담당이다 ===
 * 계층 · 위젯 이름 · `BindWidget` 대상 · **틀리기 쉬운 값** 까지다.
 * 색·패딩·폰트 크기 마감은 디자이너에서 한다. 한 번 생성한 뒤 이 `.cpp` 는 레이아웃의 원본이 아니다.
 *
 * === 틀리기 쉬워서 코드로 박아 두는 값 3개 ===
 * 1. 루트 `Canvas_Root` 를 `SelfHitTestInvisible` 로 만든다. `Visible` 이면 화면 전체가 월드
 *    클릭을 먹어 **건축 배치가 죽는다** (2026-08-12 `WBP_TimeHud` 에서 실제 발생).
 * 2. 탭 버튼의 **선택 상태 브러시를 `Pressed` 슬롯**에 넣는다. `UFF_HudTabButtonBase` 가
 *    선택 중에 그것을 Normal·Hovered·Pressed 에 함께 적용한다. 빈 채로 두면 선택해도 배경이
 *    안 바뀌어 색만으로 구분하게 된다 (AT-011 위반).
 * 3. 바 높이를 `SizeBox`(58 = 메뉴 29 + 상태 29)로 고정한다.
 *
 * **비파괴다.** 이미 있는 패키지는 레이아웃 콜백을 아예 실행하지 않는다. 여러 번 실행해도 안전하고,
 * 에디터에서 손댄 WBP 를 덮어쓰지 않는다.
 */
namespace FFPrepHudWbpBuilder
{
	using namespace FFUIWbpBuilder;

	constexpr const TCHAR* PrepLogPrefix = TEXT("FF_PREPHUD_UMG");

	const FString PrepFolder = TEXT("/Game/FestaFiesta/UI/PrepHUD");
	const FString PrepPanelsFolder = PrepFolder / TEXT("Panels");

	const FString TabButtonPackage = PrepFolder / TEXT("WBP_FF_PrepHudTabButton");
	const FString BarPackage = PrepFolder / TEXT("WBP_FF_PrepHudBar");
	const FString PlaceholderPanelPackage = PrepPanelsFolder / TEXT("WBP_FF_PrepPlaceholderPanel");

	// ★ 이름이 계약이다. `UFF_PrepHudBar::PanelClassNameFormat`(`WBP_FF_PrepPanel{0}`) 이
	//   `EFF_PrepHudTab` 항목 이름으로 이 에셋을 찾는다. 이름을 바꾸면 두 곳을 함께 고친다.
	const FString StaffPanelPackage = PrepPanelsFolder / TEXT("WBP_FF_PrepPanelStaff");
	const FString CheckPanelPackage = PrepPanelsFolder / TEXT("WBP_FF_PrepPanelCheck");
	const FString ProgramPanelPackage = PrepPanelsFolder / TEXT("WBP_FF_PrepPanelProgram");

	const FString PromotionPanelPackage = PrepPanelsFolder / TEXT("WBP_FF_PrepPanelPromotion");

	// ★ 이 두 경로는 각 패널의 `CardWidgetFallbackPath` 기본값과 같아야 한다.
	//   그래야 패널이 손으로 배선하지 않아도 카드를 찾는다.
	const FString ProgramCardPackage = PrepFolder / TEXT("WBP_FF_PrepProgramCard");
	const FString PromotionCardPackage = PrepFolder / TEXT("WBP_FF_PrepPromotionCard");

	/** 패널 안에서 여러 개 중 하나를 고르는 버튼. 날씨 선택이 첫 사용처다. */
	const FString ChoiceButtonPackage = PrepFolder / TEXT("WBP_FF_PrepChoiceButton");

	/** 목업 규격. 메뉴 29 + 상태 29 = 58. 대략만 맞추고 마감은 에디터에서 한다. */
	constexpr float MenuRowHeight = 29.f;
	constexpr float StatusRowHeight = 29.f;
	constexpr float BarHeight = MenuRowHeight + StatusRowHeight;

	/** 패널 자리의 디자이너 미리보기 크기. 실제 값은 `UFF_PrepHudBar` 의 레이아웃 설정이 정한다. */
	constexpr float PanelHostHeight = 320.f;
	constexpr float PanelMargin = 12.f;

	/** 탭 아이콘 표시 크기. `UFF_PrepHudBar::TabIconSize` 와 같게 유지한다. */
	constexpr float TabIconSize = 13.f;

	/** 메뉴 행 칩의 모서리 반경. `UFF_PrepHudBar::MenuRowCornerRadius` 와 같게 유지한다. */
	constexpr float MenuRowCornerRadius = 0.f;

	/**
	 * 탭 버튼 모서리 반경. 각지게 하려면 0 이다.
	 *
	 * 이 값은 `WBP_FF_PrepHudTabButton` 의 버튼 스타일에 **구워진다** — 런타임 설정이 아니다.
	 * 이미 만들어진 WBP 는 디자이너에서 직접 고친다 (생성기는 비파괴).
	 */
	constexpr float TabCornerRadius = 4.f;

	/**
	 * 🔴 텍스처 없이 단색을 칠하는 브러시.
	 *
	 * `FSlateBrush` 기본값은 `DrawAs = Image` + `ImageType = NoImage` 다. 그 상태로 색만 넣으면
	 * **`UBorder` 는 아무것도 그리지 않고 `UImage` 는 흰 사각형을 그린다.** 둘 다 2026-08-26 에
	 * 실제로 겪었다. 단색을 텍스처 없이 칠하는 것은 `RoundedBox` 뿐이다.
	 *
	 * 기본 `RoundingType` 이 `HalfHeightRadius` 라 **그대로 두면 58px 바가 알약 모양이 된다.**
	 * 반경을 고정으로 바꾸고 0 을 넣는다.
	 */
	FSlateBrush MakeSolidBrush(const FLinearColor& Color, const float CornerRadius = 0.f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Color);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(CornerRadius, CornerRadius, CornerRadius, CornerRadius);
		return Brush;
	}

	// =========================================================================
	// 1) 탭 버튼
	// =========================================================================

	bool BuildTabButton(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: UI 팔레트를 읽을 수 없어 탭 버튼을 만들지 않았다."), PrepLogPrefix);
			return false;
		}

		UButton* Root = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Root"), true);
		UHorizontalBox* Content = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Content"));
		UImage* Icon = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Icon"), true);
		UTextBlock* Label = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Label"), true);

		if (!Root || !Content || !Icon || !Label)
		{
			return false;
		}

		Tree.RootWidget = Root;

		// ★ 선택 상태 브러시는 `Pressed` 슬롯이다. 베이스가 선택 중에 세 상태 모두에 적용한다.
		//   배경 형태가 바뀌어야 색맹 사용자도 선택을 구분할 수 있다 (AT-011).
		//
		// 🔴 **엔진 기본 버튼 텍스처를 틴트하지 않는다.** 처음엔 `TintColor` + `DrawAs = Box` 로
		//    했는데, 그 텍스처가 둥근 9-slice 라 모서리가 우리가 정하지 않은 모양으로 나온다.
		//    버튼 폭에 따라 9-slice 가 눌려서 **끝 버튼만 다르게 보이는** 일이 실제로 있었다
		//    (2026-08-26). 다른 버튼들과 같이 `MakeSolidBrush` 로 모서리를 명시한다.
		FButtonStyle Style = Root->GetStyle();
		Style.Normal = MakeSolidBrush(Palette->InGame.Paper, TabCornerRadius);
		Style.Hovered = MakeSolidBrush(Palette->InGame.Hover, TabCornerRadius);
		Style.Pressed = MakeSolidBrush(Palette->InGame.Teal, TabCornerRadius);
		Style.Disabled = MakeSolidBrush(Palette->InGame.PaperSunken, TabCornerRadius);
		Style.NormalPadding = FMargin(10.f, 4.f);
		Style.PressedPadding = FMargin(10.f, 4.f);
		Root->SetStyle(Style);

		Icon->SetColorAndOpacity(Palette->InGame.Ink);

		// ★ `SizeBox` 로 감싸지 않는다. 아이콘이 아직 없을 때 `UFF_HudTabButtonBase` 가
		//   `Image_Icon` 을 접는데, 감싸는 `SizeBox` 가 있으면 그것이 13px 빈 자리를 계속 차지한다.
		//   `SetDesiredSizeOverride` 로 크기를 주면 래퍼 없이도 13x13 이 된다.
		//   (`SetBrushSize` 는 5.7 에서 폐기됐다.)
		Icon->SetDesiredSizeOverride(FVector2D(TabIconSize, TabIconSize));
		if (UHorizontalBoxSlot* Slot_Icon = Content->AddChildToHorizontalBox(Icon))
		{
			Slot_Icon->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Icon->SetVerticalAlignment(VAlign_Center);
			Slot_Icon->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		}

		// 문구는 코드가 채운다. 여기 값은 디자이너 미리보기용이다.
		StyleText(*Label, 12, Palette->InGame.Ink, TEXT("Medium"));
		Label->SetText(LOCTEXT("PrepTab_Preview", "탭"));
		if (UHorizontalBoxSlot* Slot_Label = Content->AddChildToHorizontalBox(Label))
		{
			Slot_Label->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Label->SetVerticalAlignment(VAlign_Center);
		}

		if (UButtonSlot* Slot_Content = Cast<UButtonSlot>(Root->AddChild(Content)))
		{
			Slot_Content->SetHorizontalAlignment(HAlign_Center);
			Slot_Content->SetVerticalAlignment(VAlign_Center);
			Slot_Content->SetPadding(FMargin(0.f));
		}
		return true;
	}

	// =========================================================================
	// 패널 공용 헤더
	// =========================================================================

	/**
	 * 제목 + 오른쪽 위 닫기 버튼 한 줄. 패널 세 종이 같은 것을 쓴다.
	 *
	 * `Text_PanelTitle` 과 `Button_Close` 는 `UFF_PrepPanelBase` 가 이름으로 잡는다 —
	 * 여기 이름을 바꾸면 베이스의 `BindWidgetOptional` 도 함께 고쳐야 한다.
	 */
	UHorizontalBox* MakePanelHeader(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const UFF_UIPalette& Palette,
		const FText& TitlePreview)
	{
		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Header"));
		UTextBlock* Title = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_PanelTitle"), true);
		UButton* Close = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Close"), true);
		UTextBlock* CloseLabel = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CloseLabel"));

		if (!Header || !Title || !Close || !CloseLabel)
		{
			return nullptr;
		}

		StyleText(*Title, 22, Palette.InGame.Ink, TEXT("SemiBold"));
		Title->SetText(TitlePreview);
		if (UHorizontalBoxSlot* Slot_Title = Header->AddChildToHorizontalBox(Title))
		{
			// 제목이 남는 폭을 먹어 닫기 버튼을 오른쪽 끝으로 민다. Spacer 를 따로 두지 않는다.
			Slot_Title->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot_Title->SetVerticalAlignment(VAlign_Center);
		}

		FButtonStyle CloseStyle = Close->GetStyle();
		CloseStyle.Normal = MakeSolidBrush(Palette.InGame.PaperSunken, 4.f);
		CloseStyle.Hovered = MakeSolidBrush(Palette.InGame.Danger, 4.f);
		CloseStyle.Pressed = MakeSolidBrush(Palette.InGame.Danger, 4.f);
		CloseStyle.Disabled = MakeSolidBrush(Palette.InGame.PaperSunken, 4.f);
		CloseStyle.NormalPadding = FMargin(9.f, 2.f);
		CloseStyle.PressedPadding = FMargin(9.f, 2.f);
		Close->SetStyle(CloseStyle);

		// ★ 글리프는 `×`(U+00D7) 다. `✕`(U+2715) 같은 기호는 한글 폰트에 없을 수 있어
		//   **없는 글자 네모**로 나온다. 아이콘이 생기면 디자이너에서 이미지로 바꾼다.
		StyleText(*CloseLabel, 18, Palette.InGame.InkMuted, TEXT("Medium"));
		CloseLabel->SetText(LOCTEXT("PrepPanel_Close", "\u00D7"));
		if (UButtonSlot* Slot_CloseLabel = Cast<UButtonSlot>(Close->AddChild(CloseLabel)))
		{
			Slot_CloseLabel->SetHorizontalAlignment(HAlign_Center);
			Slot_CloseLabel->SetVerticalAlignment(VAlign_Center);
			Slot_CloseLabel->SetPadding(FMargin(0.f));
		}

		// 🔴 닫기 버튼에 툴팁을 달지 않는다 (2026-08-29 백종태 결정).
		// `×` 가 무엇인지 모르는 사람은 없어서, 뜨는 것은 안내가 아니라 가림이다.
		if (UHorizontalBoxSlot* Slot_Close = Header->AddChildToHorizontalBox(Close))
		{
			Slot_Close->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Close->SetVerticalAlignment(VAlign_Center);
		}
		return Header;
	}

	// =========================================================================
	// 2) 자리표시 패널
	// =========================================================================

	bool BuildPlaceholderPanel(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: UI 팔레트를 읽을 수 없어 자리표시 패널을 만들지 않았다."), PrepLogPrefix);
			return false;
		}

		UBorder* Root = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_PanelRoot"), true);
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Content"));
		UHorizontalBox* Header = MakePanelHeader(
			Blueprint, Tree, *Palette, LOCTEXT("PrepPanel_TitlePreview", "패널 제목"));
		UTextBlock* Message = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Placeholder"), true);

		if (!Root || !Content || !Header || !Message)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetBrush(MakeSolidBrush(Palette->InGame.Paper, 6.f));
		Root->SetPadding(FMargin(20.f, 16.f));

		if (UVerticalBoxSlot* Slot_Header = Content->AddChildToVerticalBox(Header))
		{
			Slot_Header->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Header->SetHorizontalAlignment(HAlign_Fill);
		}

		StyleText(*Message, 14, Palette->InGame.InkMuted, TEXT("Medium"));
		Message->SetText(LOCTEXT("PrepPanel_MessagePreview", "이 패널은 아직 만드는 중입니다."));
		Message->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_Message = Content->AddChildToVerticalBox(Message))
		{
			Slot_Message->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Message->SetHorizontalAlignment(HAlign_Fill);
			Slot_Message->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
		}

		if (UBorderSlot* Slot_Content = Cast<UBorderSlot>(Root->AddChild(Content)))
		{
			Slot_Content->SetHorizontalAlignment(HAlign_Fill);
			Slot_Content->SetVerticalAlignment(VAlign_Top);
		}
		return true;
	}

	// =========================================================================
	// 3) 인력 패널
	// =========================================================================

	/** 지출 표의 한 줄. `이름 ....... 금액` 두 칸이다. */
	bool AddCostRow(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		UVerticalBox& Table,
		const UFF_UIPalette& Palette,
		const FName RowBoxName,
		const FName ValueTextName,
		const FText& Label,
		const FText& PreviewValue,
		const FLinearColor& ValueColor)
	{
		UHorizontalBox* Row = MakeWidget<UHorizontalBox>(Blueprint, Tree, RowBoxName, true);
		UTextBlock* LabelText = MakeWidget<UTextBlock>(
			Blueprint, Tree, FName(*(RowBoxName.ToString() + TEXT("_Label"))));
		USpacer* Fill = MakeWidget<USpacer>(
			Blueprint, Tree, FName(*(RowBoxName.ToString() + TEXT("_Fill"))));
		UTextBlock* ValueText = MakeWidget<UTextBlock>(Blueprint, Tree, ValueTextName, true);

		if (!Row || !LabelText || !Fill || !ValueText)
		{
			return false;
		}

		StyleText(*LabelText, 14, Palette.InGame.InkMuted, TEXT("Medium"));
		LabelText->SetText(Label);
		if (UHorizontalBoxSlot* Slot_Label = Row->AddChildToHorizontalBox(LabelText))
		{
			Slot_Label->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Label->SetVerticalAlignment(VAlign_Center);
		}

		if (UHorizontalBoxSlot* Slot_Fill = Row->AddChildToHorizontalBox(Fill))
		{
			Slot_Fill->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		StyleText(*ValueText, 14, ValueColor, TEXT("SemiBold"));
		ValueText->SetText(PreviewValue);
		if (UHorizontalBoxSlot* Slot_Value = Row->AddChildToHorizontalBox(ValueText))
		{
			Slot_Value->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Value->SetVerticalAlignment(VAlign_Center);
		}

		if (UVerticalBoxSlot* Slot_Row = Table.AddChildToVerticalBox(Row))
		{
			Slot_Row->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Row->SetHorizontalAlignment(HAlign_Fill);
			Slot_Row->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}
		return true;
	}

	/** 스텝퍼 버튼 하나. */
	UButton* MakeStepperButton(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const UFF_UIPalette& Palette,
		const FName ButtonName,
		const FText& Glyph)
	{
		UButton* Button = MakeWidget<UButton>(Blueprint, Tree, ButtonName, true);
		UTextBlock* Label = MakeWidget<UTextBlock>(
			Blueprint, Tree, FName(*(ButtonName.ToString() + TEXT("_Label"))));
		if (!Button || !Label)
		{
			return nullptr;
		}

		FButtonStyle Style = Button->GetStyle();
		Style.Normal = MakeSolidBrush(Palette.InGame.PaperSunken, 4.f);
		Style.Hovered = MakeSolidBrush(Palette.InGame.Hover, 4.f);
		Style.Pressed = MakeSolidBrush(Palette.InGame.Line, 4.f);
		Style.Disabled = MakeSolidBrush(Palette.InGame.PaperSunken, 4.f);
		Style.NormalPadding = FMargin(14.f, 4.f);
		Style.PressedPadding = FMargin(14.f, 4.f);
		Button->SetStyle(Style);

		StyleText(*Label, 20, Palette.InGame.Ink, TEXT("SemiBold"));
		Label->SetText(Glyph);
		if (UButtonSlot* Slot_Label = Cast<UButtonSlot>(Button->AddChild(Label)))
		{
			Slot_Label->SetHorizontalAlignment(HAlign_Center);
			Slot_Label->SetVerticalAlignment(VAlign_Center);
			Slot_Label->SetPadding(FMargin(0.f));
		}
		return Button;
	}

	bool BuildStaffPanel(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: UI 팔레트를 읽을 수 없어 인력 패널을 만들지 않았다."), PrepLogPrefix);
			return false;
		}

		UBorder* Root = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_PanelRoot"), true);
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Content"));
		UHorizontalBox* Header = MakePanelHeader(
			Blueprint, Tree, *Palette, LOCTEXT("PrepStaff_TitlePreview", "인력"));

		UTextBlock* CurrentStaff = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CurrentStaff"), true);
		UTextBlock* MaxStaff = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_MaxStaff"), true);

		UHorizontalBox* StepperRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Stepper"));
		UTextBlock* StepperLabel = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_StepperLabel"));
		UButton* Decrease = MakeStepperButton(
			Blueprint, Tree, *Palette, TEXT("Button_Decrease"), LOCTEXT("PrepStaff_Minus", "-"));
		UTextBlock* SelectedCount = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_SelectedCount"), true);
		UButton* Increase = MakeStepperButton(
			Blueprint, Tree, *Palette, TEXT("Button_Increase"), LOCTEXT("PrepStaff_Plus", "+"));

		UTextBlock* WagePreview = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_WagePreview"), true);
		UTextBlock* GoldAfterHire = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_GoldAfterHire"), true);

		UVerticalBox* CostTable = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_CostTable"));
		UTextBlock* CostTableTitle = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CostTableTitle"));

		UTextBlock* DailyNotice = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_DailyNotice"), true);

		UHorizontalBox* ActionRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Action"));
		UTextBlock* BlockReason = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_HireBlockReason"), true);
		UButton* Hire = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Hire"), true);
		UTextBlock* HireLabel = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_HireLabel"));

		if (!Root || !Content || !Header || !CurrentStaff || !MaxStaff || !StepperRow || !StepperLabel
			|| !Decrease || !SelectedCount || !Increase || !WagePreview || !GoldAfterHire
			|| !CostTable || !CostTableTitle || !DailyNotice
			|| !ActionRow || !BlockReason || !Hire || !HireLabel)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetBrush(MakeSolidBrush(Palette->InGame.Paper, 8.f));
		Root->SetPadding(FMargin(28.f, 24.f));

		if (UVerticalBoxSlot* Slot_Header = Content->AddChildToVerticalBox(Header))
		{
			Slot_Header->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Header->SetHorizontalAlignment(HAlign_Fill);
			Slot_Header->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
		}

		StyleText(*CurrentStaff, 28, Palette->InGame.Ink, TEXT("SemiBold"));
		CurrentStaff->SetText(LOCTEXT("PrepStaff_CurrentPreview", "6명 (기본 6 · 고용 0)"));
		if (UVerticalBoxSlot* Slot_Current = Content->AddChildToVerticalBox(CurrentStaff))
		{
			Slot_Current->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Current->SetHorizontalAlignment(HAlign_Fill);
		}

		StyleText(*MaxStaff, 14, Palette->InGame.InkMuted, TEXT("Medium"));
		MaxStaff->SetText(LOCTEXT("PrepStaff_MaxPreview", "최대 12명 (기본 6 + 고용 6)"));
		if (UVerticalBoxSlot* Slot_Max = Content->AddChildToVerticalBox(MaxStaff))
		{
			Slot_Max->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Max->SetHorizontalAlignment(HAlign_Fill);
			Slot_Max->SetPadding(FMargin(0.f, 4.f, 0.f, 22.f));
		}

		// --- 스텝퍼 ---
		StyleText(*StepperLabel, 14, Palette->InGame.Ink, TEXT("Medium"));
		StepperLabel->SetText(LOCTEXT("PrepStaff_StepperLabel", "인원 고용"));
		if (UHorizontalBoxSlot* Slot_StepperLabel = StepperRow->AddChildToHorizontalBox(StepperLabel))
		{
			Slot_StepperLabel->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_StepperLabel->SetVerticalAlignment(VAlign_Center);
			Slot_StepperLabel->SetPadding(FMargin(0.f, 0.f, 18.f, 0.f));
		}
		if (UHorizontalBoxSlot* Slot_Decrease = StepperRow->AddChildToHorizontalBox(Decrease))
		{
			Slot_Decrease->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Decrease->SetVerticalAlignment(VAlign_Center);
		}

		StyleText(*SelectedCount, 22, Palette->InGame.Ink, TEXT("SemiBold"));
		SelectedCount->SetText(LOCTEXT("PrepStaff_SelectedPreview", "1"));
		SelectedCount->SetJustification(ETextJustify::Center);
		SelectedCount->SetMinDesiredWidth(56.f);
		if (UHorizontalBoxSlot* Slot_Selected = StepperRow->AddChildToHorizontalBox(SelectedCount))
		{
			Slot_Selected->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Selected->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* Slot_Increase = StepperRow->AddChildToHorizontalBox(Increase))
		{
			Slot_Increase->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Increase->SetVerticalAlignment(VAlign_Center);
		}
		if (UVerticalBoxSlot* Slot_StepperRow = Content->AddChildToVerticalBox(StepperRow))
		{
			Slot_StepperRow->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_StepperRow->SetHorizontalAlignment(HAlign_Left);
		}

		// --- 견적 ---
		StyleText(*WagePreview, 16, Palette->InGame.Ink, TEXT("Medium"));
		WagePreview->SetText(LOCTEXT("PrepStaff_WagePreviewText", "1명 x 450 = 450 Gold"));
		if (UVerticalBoxSlot* Slot_Wage = Content->AddChildToVerticalBox(WagePreview))
		{
			Slot_Wage->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Wage->SetHorizontalAlignment(HAlign_Fill);
			Slot_Wage->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
		}

		StyleText(*GoldAfterHire, 14, Palette->InGame.InkMuted, TEXT("Medium"));
		GoldAfterHire->SetText(LOCTEXT("PrepStaff_GoldAfterPreview", "고용 후 잔액 7,150"));
		if (UVerticalBoxSlot* Slot_GoldAfter = Content->AddChildToVerticalBox(GoldAfterHire))
		{
			Slot_GoldAfter->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_GoldAfter->SetHorizontalAlignment(HAlign_Fill);
			Slot_GoldAfter->SetPadding(FMargin(0.f, 4.f, 0.f, 24.f));
		}

		// --- 고용비 표 ---
		// "오늘" 이 아니다. 고용이 1회 구매라 이 값은 축제 누계이고, Day2 에 아무도 안 뽑아도
		// 어제 쓴 돈이 그대로 남아 있다. 제목에 "오늘" 을 쓰면 그 숫자가 거짓말이 된다.
		StyleText(*CostTableTitle, 14, Palette->InGame.Ink, TEXT("SemiBold"));
		CostTableTitle->SetText(LOCTEXT("PrepStaff_CostTableTitle", "고용비"));
		if (UVerticalBoxSlot* Slot_CostTitle = CostTable->AddChildToVerticalBox(CostTableTitle))
		{
			Slot_CostTitle->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_CostTitle->SetHorizontalAlignment(HAlign_Fill);
			Slot_CostTitle->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		}

		// 장소 임대료·미납 줄은 임대료를 없애면서 같이 뺐다. 오늘 나가는 돈은 고용비 하나뿐이다
		const bool bRowsOk =
			AddCostRow(Blueprint, Tree, *CostTable, *Palette,
				TEXT("Box_WageRow"), TEXT("Text_WageCost"),
				LOCTEXT("PrepStaff_HireCostLabel", "고용비"),
				LOCTEXT("PrepStaff_HireCostPreviewRow", "0 (누계)"), Palette->InGame.Ink);

		if (!bRowsOk)
		{
			return false;
		}

		if (UVerticalBoxSlot* Slot_CostTable = Content->AddChildToVerticalBox(CostTable))
		{
			Slot_CostTable->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_CostTable->SetHorizontalAlignment(HAlign_Fill);
		}

		// --- 영구 고용 안내. 문구는 코드가 채운다 ---
		StyleText(*DailyNotice, 13, Palette->InGame.InkMuted, TEXT("Medium"));
		DailyNotice->SetText(LOCTEXT("PrepStaff_DailyNoticePreview",
			"고용한 인원은 축제가 끝날 때까지 남습니다. 일찍 뽑을수록 오래 씁니다."));
		DailyNotice->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_Notice = Content->AddChildToVerticalBox(DailyNotice))
		{
			Slot_Notice->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Notice->SetHorizontalAlignment(HAlign_Fill);
			Slot_Notice->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));
		}

		// --- 이유 + 고용 버튼 ---
		StyleText(*BlockReason, 13, Palette->InGame.InkMuted, TEXT("Medium"));
		BlockReason->SetText(LOCTEXT("PrepStaff_ReasonPreview", "고용 상한 6명을 다 썼습니다."));
		BlockReason->SetAutoWrapText(true);
		if (UHorizontalBoxSlot* Slot_Reason = ActionRow->AddChildToHorizontalBox(BlockReason))
		{
			Slot_Reason->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot_Reason->SetVerticalAlignment(VAlign_Center);
			Slot_Reason->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
		}

		FButtonStyle HireStyle = Hire->GetStyle();
		HireStyle.Normal = MakeSolidBrush(Palette->InGame.Teal, 6.f);
		HireStyle.Hovered = MakeSolidBrush(Palette->InGame.TealDeep, 6.f);
		HireStyle.Pressed = MakeSolidBrush(Palette->InGame.TealDeep, 6.f);
		HireStyle.Disabled = MakeSolidBrush(Palette->InGame.LineStrong, 6.f);
		HireStyle.NormalPadding = FMargin(22.f, 8.f);
		HireStyle.PressedPadding = FMargin(22.f, 8.f);
		Hire->SetStyle(HireStyle);

		StyleText(*HireLabel, 15, Palette->InGame.White, TEXT("SemiBold"));
		HireLabel->SetText(LOCTEXT("PrepStaff_HireLabel", "고용하기"));
		if (UButtonSlot* Slot_HireLabel = Cast<UButtonSlot>(Hire->AddChild(HireLabel)))
		{
			Slot_HireLabel->SetHorizontalAlignment(HAlign_Center);
			Slot_HireLabel->SetVerticalAlignment(VAlign_Center);
			Slot_HireLabel->SetPadding(FMargin(0.f));
		}
		if (UHorizontalBoxSlot* Slot_Hire = ActionRow->AddChildToHorizontalBox(Hire))
		{
			Slot_Hire->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Hire->SetVerticalAlignment(VAlign_Center);
		}
		if (UVerticalBoxSlot* Slot_ActionRow = Content->AddChildToVerticalBox(ActionRow))
		{
			Slot_ActionRow->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_ActionRow->SetHorizontalAlignment(HAlign_Fill);
			Slot_ActionRow->SetPadding(FMargin(0.f, 24.f, 0.f, 0.f));
		}

		if (UBorderSlot* Slot_Content = Cast<UBorderSlot>(Root->AddChild(Content)))
		{
			Slot_Content->SetHorizontalAlignment(HAlign_Fill);
			Slot_Content->SetVerticalAlignment(VAlign_Top);
		}
		return true;
	}

	// =========================================================================
	// 4) 선택 버튼 · 점검 패널
	// =========================================================================

	bool BuildChoiceButton(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: UI 팔레트를 읽을 수 없어 선택 버튼을 만들지 않았다."), PrepLogPrefix);
			return false;
		}

		UButton* Root = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Root"), true);
		UTextBlock* Label = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Label"), true);
		if (!Root || !Label)
		{
			return false;
		}

		Tree.RootWidget = Root;

		// ★ 선택 상태 브러시는 `Pressed` 슬롯이다. 베이스가 선택 중에 세 상태에 함께 적용한다.
		//   배경이 바뀌어야 색맹 사용자도 어느 날씨가 골라졌는지 알 수 있다 (AT-011).
		FButtonStyle Style = Root->GetStyle();
		Style.Normal = MakeSolidBrush(Palette->InGame.PaperSunken, 4.f);
		Style.Hovered = MakeSolidBrush(Palette->InGame.Hover, 4.f);
		Style.Pressed = MakeSolidBrush(Palette->InGame.Teal, 4.f);
		Style.Disabled = MakeSolidBrush(Palette->InGame.PaperSunken, 4.f);
		Style.NormalPadding = FMargin(12.f, 4.f);
		Style.PressedPadding = FMargin(12.f, 4.f);
		Root->SetStyle(Style);

		StyleText(*Label, 13, Palette->InGame.Ink, TEXT("Medium"));
		Label->SetText(LOCTEXT("PrepChoice_Preview", "선택"));
		if (UButtonSlot* Slot_Label = Cast<UButtonSlot>(Root->AddChild(Label)))
		{
			Slot_Label->SetHorizontalAlignment(HAlign_Center);
			Slot_Label->SetVerticalAlignment(VAlign_Center);
			Slot_Label->SetPadding(FMargin(0.f));
		}
		return true;
	}

	/**
	 * 카드 한 장. 위에 항목 이름, 아래에 값이다.
	 * 돌려주는 것은 카드 안쪽 상자다 — 날씨 카드처럼 값 밑에 더 붙일 것이 있는 경우에 쓴다.
	 */
	UVerticalBox* AddInfoCard(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		UHorizontalBox& Row,
		const UFF_UIPalette& Palette,
		const FName CardName,
		const FName ValueTextName,
		const FText& Label,
		const FText& PreviewValue,
		const float FillWeight = 1.f)
	{
		UBorder* Card = MakeWidget<UBorder>(Blueprint, Tree, CardName, true);
		UVerticalBox* Body = MakeWidget<UVerticalBox>(
			Blueprint, Tree, FName(*(CardName.ToString() + TEXT("_Body"))));
		UTextBlock* LabelText = MakeWidget<UTextBlock>(
			Blueprint, Tree, FName(*(CardName.ToString() + TEXT("_Label"))));
		UTextBlock* ValueText = MakeWidget<UTextBlock>(Blueprint, Tree, ValueTextName, true);

		if (!Card || !Body || !LabelText || !ValueText)
		{
			return nullptr;
		}

		Card->SetBrush(MakeSolidBrush(Palette.InGame.PaperSunken, 6.f));
		Card->SetPadding(FMargin(16.f, 12.f));

		StyleText(*LabelText, 12, Palette.InGame.InkMuted, TEXT("Medium"));
		LabelText->SetText(Label);
		if (UVerticalBoxSlot* Slot_Label = Body->AddChildToVerticalBox(LabelText))
		{
			Slot_Label->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Label->SetHorizontalAlignment(HAlign_Fill);
		}

		// 🔴 카드 폭이 좁아서 20 이면 문구가 카드를 넘어 옆 카드와 겹친다 (2026-08-26 실제 발생).
		StyleText(*ValueText, 15, Palette.InGame.Ink, TEXT("SemiBold"));
		ValueText->SetText(PreviewValue);
		ValueText->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_Value = Body->AddChildToVerticalBox(ValueText))
		{
			Slot_Value->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Value->SetHorizontalAlignment(HAlign_Fill);
			Slot_Value->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		}

		if (UBorderSlot* Slot_Body = Cast<UBorderSlot>(Card->AddChild(Body)))
		{
			Slot_Body->SetHorizontalAlignment(HAlign_Fill);
			Slot_Body->SetVerticalAlignment(VAlign_Top);
		}

		if (UHorizontalBoxSlot* Slot_Card = Row.AddChildToHorizontalBox(Card))
		{
			// 폭을 가중치로 나눈다. Automatic 으로 두면 문구 길이에 따라 들쭉날쭉해지고,
			// 전부 1 로 두면 선택 버튼이 들어가는 날씨 카드가 좁아서 버튼이 넘친다.
			FSlateChildSize CardSize(ESlateSizeRule::Fill);
			CardSize.Value = FillWeight;
			Slot_Card->SetSize(CardSize);
			Slot_Card->SetVerticalAlignment(VAlign_Fill);
			Slot_Card->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
		}
		return Body;
	}

	bool BuildCheckPanel(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, UClass& ChoiceButtonClass)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: UI 팔레트를 읽을 수 없어 점검 패널을 만들지 않았다."), PrepLogPrefix);
			return false;
		}

		UBorder* Root = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_PanelRoot"), true);

		// === 🔴 세로 구조가 3단이다 (2026-08-26) ===
		// 한 `VerticalBox` 에 다 넣었더니 내용이 패널 높이를 넘어 **`개장하기` 버튼이 화면
		// 밖으로 잘려 누를 수 없었다.** 본문만 스크롤하고 버튼 행은 항상 보이게 분리한다.
		//   Root ─ VerticalBox_Root
		//            ├ Text_PanelTitle      (자동 높이)
		//            ├ ScrollBox_Content    (남는 높이 전부)
		//            └ HorizontalBox_Action (자동 높이 · 항상 보인다)
		UVerticalBox* RootBox = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Root"));
		UScrollBox* Content = MakeWidget<UScrollBox>(Blueprint, Tree, TEXT("ScrollBox_Content"));
		UHorizontalBox* Header = MakePanelHeader(
			Blueprint, Tree, *Palette, LOCTEXT("PrepCheck_TitlePreview", "점검"));
		UHorizontalBox* CardRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Cards"));

		if (!Root || !RootBox || !Content || !Header || !CardRow)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetBrush(MakeSolidBrush(Palette->InGame.Paper, 8.f));
		Root->SetPadding(FMargin(28.f, 24.f));

		if (UVerticalBoxSlot* Slot_Header = RootBox->AddChildToVerticalBox(Header))
		{
			Slot_Header->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Header->SetHorizontalAlignment(HAlign_Fill);
			Slot_Header->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
		}

		// --- 카드 4장 ---
		UVerticalBox* WeatherBody = AddInfoCard(Blueprint, Tree, *CardRow, *Palette,
			TEXT("Card_Weather"), TEXT("Text_Weather"),
			LOCTEXT("PrepCheck_WeatherLabel", "날씨"),
			LOCTEXT("PrepCheck_WeatherPreview", "맑음"),
			// 선택 버튼 3개가 들어가므로 다른 카드보다 넓게 잡는다.
			/*FillWeight*/ 1.8f);

		const bool bCardsOk = WeatherBody != nullptr
			&& AddInfoCard(Blueprint, Tree, *CardRow, *Palette,
				TEXT("Card_DailyGoal"), TEXT("Text_DailyGoal"),
				LOCTEXT("PrepCheck_GoalLabel", "일일 목표"),
				LOCTEXT("PrepCheck_GoalPreview", "500명 · 달성 시 +300")) != nullptr
			&& AddInfoCard(Blueprint, Tree, *CardRow, *Palette,
				TEXT("Card_ExpectedVisitors"), TEXT("Text_ExpectedVisitors"),
				LOCTEXT("PrepCheck_ExpectedLabel", "예상 방문객"),
				LOCTEXT("PrepCheck_ExpectedPreview", "아직 알 수 없습니다")) != nullptr
			&& AddInfoCard(Blueprint, Tree, *CardRow, *Palette,
				TEXT("Card_Gold"), TEXT("Text_Gold"),
				LOCTEXT("PrepCheck_GoldLabel", "보유 금화"),
				LOCTEXT("PrepCheck_GoldPreview", "7,600")) != nullptr;

		if (!bCardsOk)
		{
			return false;
		}

		// --- 날씨 카드 안쪽에 선택지 3개 + 잠금 이유 ---
		// 🔴 `HorizontalBox` 로 두면 카드보다 넓어질 때 **옆 카드 위로 넘쳐 나간다**
		//    (2026-08-26 실제 발생). `WrapBox` 는 폭이 부족하면 다음 줄로 내린다.
		UWrapBox* WeatherChoices = MakeWidget<UWrapBox>(
			Blueprint, Tree, TEXT("WrapBox_WeatherChoices"));
		UTextBlock* WeatherLock = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_WeatherLockReason"), true);

		if (!WeatherChoices || !WeatherLock)
		{
			return false;
		}

		// ★ 이름이 계약이다. `UFF_PrepPanelCheck::CollectWeatherChoices` 가
		//   `Choice_<EFF_WeatherType 항목 이름>` 으로 찾는다.
		const TCHAR* ChoiceNames[] = { TEXT("Choice_Clear"), TEXT("Choice_Rain"), TEXT("Choice_Windy") };
		for (const TCHAR* ChoiceName : ChoiceNames)
		{
			UUserWidget* Choice = MakeWidgetOfClass(Blueprint, Tree, FName(ChoiceName), &ChoiceButtonClass);
			if (!Choice)
			{
				return false;
			}

			if (UWrapBoxSlot* Slot_Choice = WeatherChoices->AddChildToWrapBox(Choice))
			{
				Slot_Choice->SetVerticalAlignment(VAlign_Center);
				Slot_Choice->SetPadding(FMargin(0.f, 0.f, 4.f, 4.f));
			}
		}

		if (UVerticalBoxSlot* Slot_Choices = WeatherBody->AddChildToVerticalBox(WeatherChoices))
		{
			Slot_Choices->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Choices->SetHorizontalAlignment(HAlign_Left);
			Slot_Choices->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		}

		StyleText(*WeatherLock, 11, Palette->InGame.InkMuted, TEXT("Medium"));
		WeatherLock->SetText(LOCTEXT("PrepCheck_WeatherLockPreview", "지금 단계에서는 날씨를 바꿀 수 없습니다."));
		WeatherLock->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_Lock = WeatherBody->AddChildToVerticalBox(WeatherLock))
		{
			Slot_Lock->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Lock->SetHorizontalAlignment(HAlign_Fill);
			Slot_Lock->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		}

		if (UScrollBoxSlot* Slot_CardRow = Cast<UScrollBoxSlot>(Content->AddChild(CardRow)))
		{
			Slot_CardRow->SetHorizontalAlignment(HAlign_Fill);
			Slot_CardRow->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
		}

		// --- 수용 위험 ---
		UBorder* RiskBox = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_CapacityRisk"), true);
		UVerticalBox* RiskBody = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_CapacityRisk"));
		UTextBlock* RiskText = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CapacityRisk"), true);
		UTextBlock* RiskDetail = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CapacityDetail"), true);

		if (!RiskBox || !RiskBody || !RiskText || !RiskDetail)
		{
			return false;
		}

		RiskBox->SetBrush(MakeSolidBrush(Palette->InGame.PaperSunken, 6.f));
		RiskBox->SetPadding(FMargin(16.f, 12.f));

		// 색은 런타임에 구간별로 갈아치울 자리다. 여기 값은 디자이너 미리보기용이다.
		StyleText(*RiskText, 16, Palette->InGame.Ink, TEXT("SemiBold"));
		RiskText->SetText(LOCTEXT("PrepCheck_RiskPreview", "? 판정 불가 — 예상 방문객이나 정원을 아직 읽지 못했습니다"));
		RiskText->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_Risk = RiskBody->AddChildToVerticalBox(RiskText))
		{
			Slot_Risk->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Risk->SetHorizontalAlignment(HAlign_Fill);
		}

		StyleText(*RiskDetail, 12, Palette->InGame.InkMuted, TEXT("Medium"));
		RiskDetail->SetText(LOCTEXT("PrepCheck_RiskDetailPreview", "정원 900명 대비 최대 예상 480명"));
		RiskDetail->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_Detail = RiskBody->AddChildToVerticalBox(RiskDetail))
		{
			Slot_Detail->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Detail->SetHorizontalAlignment(HAlign_Fill);
			Slot_Detail->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}

		if (UBorderSlot* Slot_RiskBody = Cast<UBorderSlot>(RiskBox->AddChild(RiskBody)))
		{
			Slot_RiskBody->SetHorizontalAlignment(HAlign_Fill);
			Slot_RiskBody->SetVerticalAlignment(VAlign_Top);
		}
		if (UScrollBoxSlot* Slot_RiskBox = Cast<UScrollBoxSlot>(Content->AddChild(RiskBox)))
		{
			Slot_RiskBox->SetHorizontalAlignment(HAlign_Fill);
			Slot_RiskBox->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
		}

		// --- 요약 3줄 ---
		UVerticalBox* SummaryTable = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Summary"));
		UTextBlock* SummaryTitle = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_SummaryTitle"));
		if (!SummaryTable || !SummaryTitle)
		{
			return false;
		}

		StyleText(*SummaryTitle, 14, Palette->InGame.Ink, TEXT("SemiBold"));
		SummaryTitle->SetText(LOCTEXT("PrepCheck_SummaryTitle", "준비 상황"));
		if (UVerticalBoxSlot* Slot_SummaryTitle = SummaryTable->AddChildToVerticalBox(SummaryTitle))
		{
			Slot_SummaryTitle->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_SummaryTitle->SetHorizontalAlignment(HAlign_Fill);
			Slot_SummaryTitle->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		}

		const bool bSummaryOk =
			AddCostRow(Blueprint, Tree, *SummaryTable, *Palette,
				TEXT("Box_StaffSummaryRow"), TEXT("Text_StaffSummary"),
				LOCTEXT("PrepCheck_StaffLabel", "인력"),
				LOCTEXT("PrepCheck_StaffPreview", "6명 (기본 6 · 고용 0)"), Palette->InGame.Ink)
			&& AddCostRow(Blueprint, Tree, *SummaryTable, *Palette,
				TEXT("Box_ProgramSummaryRow"), TEXT("Text_ProgramSummary"),
				LOCTEXT("PrepCheck_ProgramLabel", "유료 프로그램"),
				LOCTEXT("PrepCheck_ProgramPreview", "0개 편성"), Palette->InGame.Ink)
			// 마지막 날이 아니면 코드가 이 줄을 접는다.
			&& AddCostRow(Blueprint, Tree, *SummaryTable, *Palette,
				TEXT("Box_FinaleSummaryRow"), TEXT("Text_FinaleSummary"),
				LOCTEXT("PrepCheck_FinaleLabel", "피날레"),
				LOCTEXT("PrepCheck_FinalePreview", "미구매 — 최소 연출만 실행됩니다"), Palette->InGame.Ink);

		if (!bSummaryOk)
		{
			return false;
		}

		if (UScrollBoxSlot* Slot_Summary = Cast<UScrollBoxSlot>(Content->AddChild(SummaryTable)))
		{
			Slot_Summary->SetHorizontalAlignment(HAlign_Fill);
			Slot_Summary->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
		}

		// --- 견적표 ---
		UVerticalBox* EstimateTable = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Estimate"));
		UTextBlock* EstimateTitle = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_EstimateTitle"));
		UTextBlock* EstimateNotice = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_EstimateNotice"), true);
		if (!EstimateTable || !EstimateTitle || !EstimateNotice)
		{
			return false;
		}

		StyleText(*EstimateTitle, 14, Palette->InGame.Ink, TEXT("SemiBold"));
		EstimateTitle->SetText(LOCTEXT("PrepCheck_EstimateTitle", "오늘 견적"));
		if (UVerticalBoxSlot* Slot_EstimateTitle = EstimateTable->AddChildToVerticalBox(EstimateTitle))
		{
			Slot_EstimateTitle->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_EstimateTitle->SetHorizontalAlignment(HAlign_Fill);
			Slot_EstimateTitle->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		}

		const bool bEstimateOk =
			AddCostRow(Blueprint, Tree, *EstimateTable, *Palette,
				TEXT("Box_WageRow"), TEXT("Text_StaffWageCost"),
				LOCTEXT("PrepCheck_WageLabel", "고용비"),
				LOCTEXT("PrepCheck_WagePreview", "0"), Palette->InGame.Ink)
			&& AddCostRow(Blueprint, Tree, *EstimateTable, *Palette,
				TEXT("Box_AdmissionRow"), TEXT("Text_ExpectedAdmission"),
				LOCTEXT("PrepCheck_AdmissionLabel", "예상 입장료 수입"),
				LOCTEXT("PrepCheck_AdmissionPreview", "아직 알 수 없습니다"), Palette->InGame.Ink)
			&& AddCostRow(Blueprint, Tree, *EstimateTable, *Palette,
				TEXT("Box_GoalBonusRow"), TEXT("Text_ExpectedGoalBonus"),
				LOCTEXT("PrepCheck_BonusLabel", "일일 목표 보너스"),
				LOCTEXT("PrepCheck_BonusPreview", "아직 알 수 없습니다"), Palette->InGame.Ink);

		if (!bEstimateOk)
		{
			return false;
		}

		StyleText(*EstimateNotice, 11, Palette->InGame.InkMuted, TEXT("Medium"));
		EstimateNotice->SetText(LOCTEXT("PrepCheck_NoticePreview",
			"부스 수수료와 미니게임 보상은 운영 결과에 따라 달라지므로 이 표에 넣지 않았습니다."));
		EstimateNotice->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_Notice = EstimateTable->AddChildToVerticalBox(EstimateNotice))
		{
			Slot_Notice->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Notice->SetHorizontalAlignment(HAlign_Fill);
			Slot_Notice->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		}

		if (UScrollBoxSlot* Slot_Estimate = Cast<UScrollBoxSlot>(Content->AddChild(EstimateTable)))
		{
			Slot_Estimate->SetHorizontalAlignment(HAlign_Fill);
		}

		// --- 버튼 두 개 ---
		UHorizontalBox* ActionRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Action"));
		USpacer* ActionFill = MakeWidget<USpacer>(Blueprint, Tree, TEXT("Spacer_ActionFill"));
		UButton* ContinueBuild = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_ContinueBuild"), true);
		UTextBlock* ContinueLabel = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_ContinueBuildLabel"));
		UButton* OpenGate = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_OpenGate"), true);
		UTextBlock* OpenGateLabel = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_OpenGateLabel"));

		if (!ActionRow || !ActionFill || !ContinueBuild || !ContinueLabel || !OpenGate || !OpenGateLabel)
		{
			return false;
		}

		if (UHorizontalBoxSlot* Slot_Fill = ActionRow->AddChildToHorizontalBox(ActionFill))
		{
			Slot_Fill->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		// 되돌아가는 버튼은 약하게, 진행하는 버튼은 강하게. 둘이 같은 무게면 잘못 누른다.
		FButtonStyle ContinueStyle = ContinueBuild->GetStyle();
		ContinueStyle.Normal = MakeSolidBrush(Palette->InGame.PaperSunken, 6.f);
		ContinueStyle.Hovered = MakeSolidBrush(Palette->InGame.Hover, 6.f);
		ContinueStyle.Pressed = MakeSolidBrush(Palette->InGame.Line, 6.f);
		ContinueStyle.Disabled = MakeSolidBrush(Palette->InGame.PaperSunken, 6.f);
		ContinueStyle.NormalPadding = FMargin(20.f, 8.f);
		ContinueStyle.PressedPadding = FMargin(20.f, 8.f);
		ContinueBuild->SetStyle(ContinueStyle);

		StyleText(*ContinueLabel, 14, Palette->InGame.Ink, TEXT("Medium"));
		ContinueLabel->SetText(LOCTEXT("PrepCheck_ContinueBuild", "배치 계속하기"));
		if (UButtonSlot* Slot_ContinueLabel = Cast<UButtonSlot>(ContinueBuild->AddChild(ContinueLabel)))
		{
			Slot_ContinueLabel->SetHorizontalAlignment(HAlign_Center);
			Slot_ContinueLabel->SetVerticalAlignment(VAlign_Center);
			Slot_ContinueLabel->SetPadding(FMargin(0.f));
		}
		if (UHorizontalBoxSlot* Slot_Continue = ActionRow->AddChildToHorizontalBox(ContinueBuild))
		{
			Slot_Continue->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Continue->SetVerticalAlignment(VAlign_Center);
			Slot_Continue->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
		}

		FButtonStyle OpenStyle = OpenGate->GetStyle();
		OpenStyle.Normal = MakeSolidBrush(Palette->InGame.Teal, 6.f);
		OpenStyle.Hovered = MakeSolidBrush(Palette->InGame.TealDeep, 6.f);
		OpenStyle.Pressed = MakeSolidBrush(Palette->InGame.TealDeep, 6.f);
		OpenStyle.Disabled = MakeSolidBrush(Palette->InGame.LineStrong, 6.f);
		OpenStyle.NormalPadding = FMargin(24.f, 8.f);
		OpenStyle.PressedPadding = FMargin(24.f, 8.f);
		OpenGate->SetStyle(OpenStyle);

		StyleText(*OpenGateLabel, 15, Palette->InGame.White, TEXT("SemiBold"));
		OpenGateLabel->SetText(LOCTEXT("PrepCheck_OpenGate", "개장하기"));
		if (UButtonSlot* Slot_OpenLabel = Cast<UButtonSlot>(OpenGate->AddChild(OpenGateLabel)))
		{
			Slot_OpenLabel->SetHorizontalAlignment(HAlign_Center);
			Slot_OpenLabel->SetVerticalAlignment(VAlign_Center);
			Slot_OpenLabel->SetPadding(FMargin(0.f));
		}
		if (UHorizontalBoxSlot* Slot_Open = ActionRow->AddChildToHorizontalBox(OpenGate))
		{
			Slot_Open->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Open->SetVerticalAlignment(VAlign_Center);
		}

		// 본문 스크롤은 남는 높이를 전부 받는다. 버튼 행보다 먼저 넣어야 순서가 맞는다.
		if (UVerticalBoxSlot* Slot_Scroll = RootBox->AddChildToVerticalBox(Content))
		{
			Slot_Scroll->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot_Scroll->SetHorizontalAlignment(HAlign_Fill);
		}

		// ★ 버튼 행은 **스크롤 밖**이다. 안에 넣으면 내용이 길어질 때 다시 잘린다.
		if (UVerticalBoxSlot* Slot_ActionRow = RootBox->AddChildToVerticalBox(ActionRow))
		{
			Slot_ActionRow->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_ActionRow->SetHorizontalAlignment(HAlign_Fill);
			Slot_ActionRow->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
		}

		// `VAlign_Top` 이 아니라 `Fill` 이다. Top 이면 스크롤 상자가 높이를 못 받아
		// 내용 전체가 그대로 늘어나고 잘림이 그대로 남는다.
		if (UBorderSlot* Slot_RootBox = Cast<UBorderSlot>(Root->AddChild(RootBox)))
		{
			Slot_RootBox->SetHorizontalAlignment(HAlign_Fill);
			Slot_RootBox->SetVerticalAlignment(VAlign_Fill);
		}
		return true;
	}

	// =========================================================================
	// 5) 하단 바
	// =========================================================================

	/** 상태 행의 한 칸. 값이 없을 때 묶음째 접히도록 `Box_` + `Text_` 짝으로 만든다. */
	bool AddStatusCell(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		UHorizontalBox& Row,
		const FName BoxName,
		const FName TextName,
		const FText& PreviewText,
		const FLinearColor& TextColor)
	{
		UHorizontalBox* Box = MakeWidget<UHorizontalBox>(Blueprint, Tree, BoxName, true);
		UTextBlock* Text = MakeWidget<UTextBlock>(Blueprint, Tree, TextName, true);
		if (!Box || !Text)
		{
			return false;
		}

		StyleText(*Text, 12, TextColor, TEXT("Medium"));
		Text->SetText(PreviewText);
		if (UHorizontalBoxSlot* Slot_Text = Box->AddChildToHorizontalBox(Text))
		{
			Slot_Text->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Text->SetVerticalAlignment(VAlign_Center);
		}

		if (UHorizontalBoxSlot* Slot_Box = Row.AddChildToHorizontalBox(Box))
		{
			Slot_Box->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Box->SetVerticalAlignment(VAlign_Center);
			Slot_Box->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
		}
		return true;
	}

	bool BuildBar(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, UClass& TabButtonClass)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: UI 팔레트를 읽을 수 없어 하단 바를 만들지 않았다."), PrepLogPrefix);
			return false;
		}

		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("Canvas_Root"));
		UOverlay* PanelHost = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Panel_PanelHost"), true);
		USizeBox* BarBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Bar"));
		UVerticalBox* BarRows = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Bar"));
		UBorder* MenuRow = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_MenuRow"), true);
		UHorizontalBox* TabRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Tabs"));
		UBorder* StatusRow = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_StatusRow"), true);

		// === 상태 행은 Overlay 다 (2026-08-26) ===
		// `HorizontalBox` 에 나열하면 **앞의 칸이 접힐 때마다 일차·단계가 좌우로 흔들린다.**
		// 관객처럼 값이 없으면 숨는 칸이 앞에 있어서 매번 위치가 달라진다.
		// 왼쪽 묶음 / 중앙 칩 / 오른쪽 묶음을 겹쳐 두면 중앙은 **화면 정중앙에 고정**된다.
		UOverlay* StatusOverlay = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Status"));
		UHorizontalBox* StatusLeft = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_StatusLeft"));
		UHorizontalBox* StatusRight = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_StatusRight"));
		UBorder* DayStageChip = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_DayStage"), true);
		UTextBlock* DayStageText = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_DayStage"), true);

		if (!Root || !PanelHost || !BarBox || !BarRows || !MenuRow || !TabRow || !StatusRow
			|| !StatusOverlay || !StatusLeft || !StatusRight || !DayStageChip || !DayStageText)
		{
			return false;
		}

		Tree.RootWidget = Root;

		// 🔴 `Visible` 이면 화면 전체가 월드 클릭을 먹어 건축 배치가 죽는다.
		//    실제 클릭을 받아야 하는 것은 버튼뿐이고 그것들은 자식이라 그대로 동작한다.
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		// --- 패널 자리 ---
		// 앵커·오프셋은 **패널을 열 때** `UFF_PrepHudBar::ApplyPanelDock` 가 탭별 방향으로 다시 쓴다.
		// 여기 값은 디자이너에서 자리를 보기 위한 기본 배치(하단)일 뿐이다.
		PanelHost->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* Slot_PanelHost = Cast<UCanvasPanelSlot>(Root->AddChild(PanelHost)))
		{
			Slot_PanelHost->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
			Slot_PanelHost->SetAlignment(FVector2D(0.f, 1.f));
			Slot_PanelHost->SetOffsets(FMargin(
				PanelMargin, -(BarHeight + PanelMargin), PanelMargin, PanelHostHeight));
		}

		// --- 하단 바. 가로로 꽉 채우고 높이는 58 고정 ---
		BarBox->SetHeightOverride(BarHeight);
		if (UCanvasPanelSlot* Slot_Bar = Cast<UCanvasPanelSlot>(Root->AddChild(BarBox)))
		{
			Slot_Bar->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
			Slot_Bar->SetAlignment(FVector2D(0.f, 1.f));
			Slot_Bar->SetOffsets(FMargin(0.f, 0.f, 0.f, BarHeight));
		}
		BarBox->AddChild(BarRows);

		// --- 메뉴 행 ---
		//
		// ★ 배경이 **버튼 폭까지만** 온다 (2026-08-26 결정).
		//   `HAlign_Fill` 이면 흰 띠가 화면을 가로질러 흐르고 버튼만 가운데 떠 있어 보인다.
		//   `HAlign_Center` 로 두면 Border 가 자식(탭 행) 크기로 줄어들어 떠 있는 칩이 된다.
		//   탭이 조건부로 숨을 때(홍보·피날레) 칩 폭도 같이 줄어든다.
		MenuRow->SetBrush(MakeSolidBrush(Palette->InGame.Paper, MenuRowCornerRadius));
		MenuRow->SetPadding(FMargin(4.f, 0.f));
		if (UVerticalBoxSlot* Slot_MenuRow = BarRows->AddChildToVerticalBox(MenuRow))
		{
			Slot_MenuRow->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot_MenuRow->SetHorizontalAlignment(HAlign_Center);
			Slot_MenuRow->SetVerticalAlignment(VAlign_Fill);
		}

		// 탭은 **디자이너에서 보이도록 여기서 직접 놓는다.** 런타임 생성으로 만들면 디자이너에
		// 하나도 안 보여 위치·크기를 손으로 못 만진다 (2026-08-13 라이브 HUD 에서 겪은 문제).
		const TCHAR* TabWidgetNames[] = {
			TEXT("Tab_Build"), TEXT("Tab_Program"), TEXT("Tab_Staff"),
			TEXT("Tab_Promotion"), TEXT("Tab_Finale"), TEXT("Tab_Check") };

		for (const TCHAR* TabWidgetName : TabWidgetNames)
		{
			UUserWidget* TabWidget = MakeWidgetOfClass(Blueprint, Tree, FName(TabWidgetName), &TabButtonClass);
			if (!TabWidget)
			{
				return false;
			}

			if (UHorizontalBoxSlot* Slot_Tab = TabRow->AddChildToHorizontalBox(TabWidget))
			{
				// 고정 폭이다. 행 전체를 가로로 늘리지 않는다 (목업 v2 공통 규격).
				Slot_Tab->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				Slot_Tab->SetVerticalAlignment(VAlign_Fill);
				Slot_Tab->SetPadding(FMargin(0.f, 0.f, 2.f, 0.f));
			}
		}

		if (UBorderSlot* Slot_TabRow = Cast<UBorderSlot>(MenuRow->AddChild(TabRow)))
		{
			Slot_TabRow->SetHorizontalAlignment(HAlign_Center);
			Slot_TabRow->SetVerticalAlignment(VAlign_Fill);
		}

		// --- 상태 행 ---
		StatusRow->SetBrush(MakeSolidBrush(Palette->InGame.StatusBar));
		StatusRow->SetPadding(FMargin(16.f, 0.f));
		if (UVerticalBoxSlot* Slot_StatusRow = BarRows->AddChildToVerticalBox(StatusRow))
		{
			Slot_StatusRow->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot_StatusRow->SetHorizontalAlignment(HAlign_Fill);
		}

		// --- 왼쪽 묶음. 축제 → 금화 순서다 ---
		const bool bLeftCellsOk =
			AddStatusCell(Blueprint, Tree, *StatusLeft,
				TEXT("Box_FestivalName"), TEXT("Text_FestivalName"),
				LOCTEXT("PrepStatusPreview_Festival", "부산 다대포 바다축제"), Palette->InGame.Ink)
			&& AddStatusCell(Blueprint, Tree, *StatusLeft,
				TEXT("Box_Gold"), TEXT("Text_Gold"),
				LOCTEXT("PrepStatusPreview_Gold", "10,000"), Palette->InGame.Ink)
			&& AddStatusCell(Blueprint, Tree, *StatusLeft,
				TEXT("Box_VisitorCount"), TEXT("Text_VisitorCount"),
				LOCTEXT("PrepStatusPreview_Visitors", "0"), Palette->InGame.Ink)
			&& AddStatusCell(Blueprint, Tree, *StatusLeft,
				TEXT("Box_Staff"), TEXT("Text_Staff"),
				LOCTEXT("PrepStatusPreview_Staff", "6명 (기본 6 · 고용 0)"), Palette->InGame.Ink);

		// --- 오른쪽 묶음. 예상 방문객 → CTA ---
		const bool bRightCellsOk =
			AddStatusCell(Blueprint, Tree, *StatusRight,
				TEXT("Box_ExpectedVisitors"), TEXT("Text_ExpectedVisitors"),
				LOCTEXT("PrepStatusPreview_Expected", "예상 320~480명"), Palette->InGame.InkMuted);

		if (!bLeftCellsOk || !bRightCellsOk)
		{
			return false;
		}

		// --- 중앙 칩. 어두운 판 + 흰 글씨라 밝은 상태 행 위에서 이 한 줄만 도드라진다 ---
		DayStageChip->SetBrush(MakeSolidBrush(Palette->InGame.TealDeep, 4.f));
		DayStageChip->SetPadding(FMargin(14.f, 2.f));

		StyleText(*DayStageText, 12, Palette->InGame.White, TEXT("SemiBold"));
		DayStageText->SetText(LOCTEXT("PrepStatusPreview_DayStage", "1일차 · 셋업"));
		DayStageText->SetJustification(ETextJustify::Center);
		if (UBorderSlot* Slot_DayStageText = Cast<UBorderSlot>(DayStageChip->AddChild(DayStageText)))
		{
			Slot_DayStageText->SetHorizontalAlignment(HAlign_Center);
			Slot_DayStageText->SetVerticalAlignment(VAlign_Center);
		}

		// --- CTA ---
		UHorizontalBox* CtaBox = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Box_Cta"), true);
		UTextBlock* CtaReason = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CtaReason"), true);
		UButton* CtaButton = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Cta"), true);
		UTextBlock* CtaLabel = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CtaLabel"), true);

		if (!CtaBox || !CtaReason || !CtaButton || !CtaLabel)
		{
			return false;
		}

		// !! 비활성 흐려짐만으로는 왜 못 누르는지 알 수 없다 !! (AT-011)
		// 이유 문구 자리를 버튼 왼쪽에 반드시 둔다. 값이 없으면 코드가 접는다.
		StyleText(*CtaReason, 11, Palette->InGame.InkMuted, TEXT("Medium"));
		CtaReason->SetText(LOCTEXT("PrepCtaPreview_Reason", "시설이 하나도 없습니다."));
		if (UHorizontalBoxSlot* Slot_Reason = CtaBox->AddChildToHorizontalBox(CtaReason))
		{
			Slot_Reason->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Reason->SetVerticalAlignment(VAlign_Center);
			Slot_Reason->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
		}

		FButtonStyle CtaStyle = CtaButton->GetStyle();
		CtaStyle.Normal.TintColor = FSlateColor(Palette->InGame.Teal);
		CtaStyle.Normal.DrawAs = ESlateBrushDrawType::Box;
		CtaStyle.Hovered.TintColor = FSlateColor(Palette->InGame.TealDeep);
		CtaStyle.Hovered.DrawAs = ESlateBrushDrawType::Box;
		CtaStyle.Pressed.TintColor = FSlateColor(Palette->InGame.TealDeep);
		CtaStyle.Pressed.DrawAs = ESlateBrushDrawType::Box;
		CtaStyle.Disabled.TintColor = FSlateColor(Palette->InGame.LineStrong);
		CtaStyle.Disabled.DrawAs = ESlateBrushDrawType::Box;
		CtaStyle.NormalPadding = FMargin(14.f, 3.f);
		CtaStyle.PressedPadding = FMargin(14.f, 3.f);
		CtaButton->SetStyle(CtaStyle);

		StyleText(*CtaLabel, 12, Palette->InGame.White, TEXT("SemiBold"));
		CtaLabel->SetText(LOCTEXT("PrepCtaPreview_Label", "확정하고 Day 1 시작"));
		if (UButtonSlot* Slot_CtaLabel = Cast<UButtonSlot>(CtaButton->AddChild(CtaLabel)))
		{
			Slot_CtaLabel->SetHorizontalAlignment(HAlign_Center);
			Slot_CtaLabel->SetVerticalAlignment(VAlign_Center);
			Slot_CtaLabel->SetPadding(FMargin(0.f));
		}

		if (UHorizontalBoxSlot* Slot_CtaButton = CtaBox->AddChildToHorizontalBox(CtaButton))
		{
			Slot_CtaButton->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_CtaButton->SetVerticalAlignment(VAlign_Center);
		}

		if (UHorizontalBoxSlot* Slot_CtaBox = StatusRight->AddChildToHorizontalBox(CtaBox))
		{
			Slot_CtaBox->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_CtaBox->SetVerticalAlignment(VAlign_Center);
		}

		// --- 세 묶음을 겹친다. 중앙 칩이 좌우 내용 길이와 무관하게 정중앙에 고정된다 ---
		//
		// ⚠️ 왼쪽 묶음이 화면 절반을 넘게 길어지면 중앙 칩과 겹친다. 1920 기준으로는
		//    왼쪽이 ~450px 이라 여유가 있다. 칸이 더 늘어나면 그때 폭을 확인한다.
		if (UOverlaySlot* Slot_Left = StatusOverlay->AddChildToOverlay(StatusLeft))
		{
			Slot_Left->SetHorizontalAlignment(HAlign_Left);
			Slot_Left->SetVerticalAlignment(VAlign_Center);
		}
		if (UOverlaySlot* Slot_Chip = StatusOverlay->AddChildToOverlay(DayStageChip))
		{
			Slot_Chip->SetHorizontalAlignment(HAlign_Center);
			Slot_Chip->SetVerticalAlignment(VAlign_Center);
		}
		if (UOverlaySlot* Slot_Right = StatusOverlay->AddChildToOverlay(StatusRight))
		{
			Slot_Right->SetHorizontalAlignment(HAlign_Right);
			Slot_Right->SetVerticalAlignment(VAlign_Center);
		}

		if (UBorderSlot* Slot_StatusOverlay = Cast<UBorderSlot>(StatusRow->AddChild(StatusOverlay)))
		{
			Slot_StatusOverlay->SetHorizontalAlignment(HAlign_Fill);
			Slot_StatusOverlay->SetVerticalAlignment(VAlign_Fill);
		}
		return true;
	}

	// =========================================================================
	// 5) 프로그램 카드 · 프로그램 패널
	// =========================================================================

	/**
	 * 카드 한 장. `UFF_PrepProgramCard` 가 이름으로 잡는다 — 여기 이름을 바꾸면 그 헤더의
	 * `BindWidgetOptional` 도 함께 고쳐야 한다. `Button_Root` 만 베이스가 강제한다.
	 */
	bool BuildProgramCard(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: UI 팔레트를 읽을 수 없어 프로그램 카드를 만들지 않았다."), PrepLogPrefix);
			return false;
		}

		UButton* Root = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Root"), true);
		USizeBox* Frame = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Card"));
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Card"));
		UTextBlock* Label = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Label"), true);
		UTextBlock* TimeRange = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_TimeRange"), true);
		UTextBlock* Cost = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Cost"), true);
		UTextBlock* Capacity = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Capacity"), true);
		UTextBlock* Badge = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Badge"), true);

		if (!Root || !Frame || !Content || !Label || !TimeRange || !Cost || !Capacity || !Badge)
		{
			return false;
		}

		Tree.RootWidget = Root;

		// ★ 선택 상태 브러시는 `Pressed` 슬롯이다. 베이스가 선택 중에 세 상태에 함께 적용한다.
		//   배경 형태가 바뀌어야 색맹 사용자도 어느 카드가 골라졌는지 안다 (AT-011).
		FButtonStyle Style = Root->GetStyle();
		Style.Normal = MakeSolidBrush(Palette->InGame.PaperSunken, 6.f);
		Style.Hovered = MakeSolidBrush(Palette->InGame.Hover, 6.f);
		Style.Pressed = MakeSolidBrush(Palette->InGame.TealDeep, 6.f);
		Style.Disabled = MakeSolidBrush(Palette->InGame.Beige, 6.f);
		Style.NormalPadding = FMargin(16.f, 14.f);
		Style.PressedPadding = FMargin(16.f, 14.f);
		Root->SetStyle(Style);

		// 폭을 고정하지 않으면 카드마다 글자 길이만큼 달라져 줄이 들쭉날쭉해진다.
		// 236 은 1160 폭 패널에서 네 장까지 한 줄에 들어가는 값이다.
		Frame->SetWidthOverride(236.f);

		// 문구는 전부 코드가 채운다. 여기 값은 디자이너 미리보기용이다.
		StyleText(*Label, 16, Palette->InGame.Ink, TEXT("SemiBold"));
		Label->SetText(LOCTEXT("PrepProgram_CardLabelPreview", "선셋 요가"));
		Label->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_Label = Content->AddChildToVerticalBox(Label))
		{
			Slot_Label->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Label->SetHorizontalAlignment(HAlign_Fill);
		}

		StyleText(*TimeRange, 12, Palette->InGame.InkMuted, TEXT("Medium"));
		TimeRange->SetText(LOCTEXT("PrepProgram_TimePreview", "19:30 ~ 20:00"));
		if (UVerticalBoxSlot* Slot_Time = Content->AddChildToVerticalBox(TimeRange))
		{
			Slot_Time->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Time->SetHorizontalAlignment(HAlign_Fill);
			Slot_Time->SetPadding(FMargin(0.f, 2.f, 0.f, 10.f));
		}

		StyleText(*Cost, 15, Palette->InGame.Ink, TEXT("SemiBold"));
		Cost->SetText(LOCTEXT("PrepProgram_CostPreviewCard", "250 Gold"));
		if (UVerticalBoxSlot* Slot_Cost = Content->AddChildToVerticalBox(Cost))
		{
			Slot_Cost->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Cost->SetHorizontalAlignment(HAlign_Fill);
		}

		StyleText(*Capacity, 13, Palette->InGame.InkMuted, TEXT("Medium"));
		Capacity->SetText(LOCTEXT("PrepProgram_CapacityPreview", "정원 16명 · 확정 400"));
		Capacity->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_Capacity = Content->AddChildToVerticalBox(Capacity))
		{
			Slot_Capacity->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Capacity->SetHorizontalAlignment(HAlign_Fill);
			Slot_Capacity->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
		}

		StyleText(*Badge, 12, Palette->InGame.Teal, TEXT("SemiBold"));
		Badge->SetText(LOCTEXT("PrepProgram_BadgePreview", "✓ 편성함"));
		if (UVerticalBoxSlot* Slot_Badge = Content->AddChildToVerticalBox(Badge))
		{
			Slot_Badge->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Badge->SetHorizontalAlignment(HAlign_Fill);
			Slot_Badge->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		}

		Frame->AddChild(Content);

		if (UButtonSlot* Slot_Frame = Cast<UButtonSlot>(Root->AddChild(Frame)))
		{
			Slot_Frame->SetHorizontalAlignment(HAlign_Fill);
			Slot_Frame->SetVerticalAlignment(VAlign_Fill);
			Slot_Frame->SetPadding(FMargin(0.f));
		}
		return true;
	}

	bool BuildProgramPanel(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: UI 팔레트를 읽을 수 없어 프로그램 패널을 만들지 않았다."), PrepLogPrefix);
			return false;
		}

		UBorder* Root = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_PanelRoot"), true);
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Content"));
		UHorizontalBox* Header = MakePanelHeader(
			Blueprint, Tree, *Palette, LOCTEXT("PrepProgram_TitlePreview", "프로그램"));

		UTextBlock* DailyNotice = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_DailyNotice"), true);

		// 🔴 스크롤을 두는 이유 — 점검 패널에서 한 `VerticalBox` 에 다 넣었더니 내용이 패널 높이를
		//    넘어 하단 버튼이 화면 밖으로 밀렸다. 액션 행은 **스크롤 밖**에 둔다.
		UScrollBox* Scroll = MakeWidget<UScrollBox>(Blueprint, Tree, TEXT("ScrollBox_Content"));

		UWrapBox* Cards = MakeWidget<UWrapBox>(Blueprint, Tree, TEXT("Box_ProgramCards"), true);

		UVerticalBox* FacilityRow = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Box_FacilityRow"), true);
		UTextBlock* FacilityLabel = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_FacilityLabel"));
		UWrapBox* FacilityChoices = MakeWidget<UWrapBox>(Blueprint, Tree, TEXT("Box_FacilityChoices"), true);
		UTextBlock* FacilityNotice = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_FacilityNotice"), true);
		UWrapBox* CatalogChoices = MakeWidget<UWrapBox>(Blueprint, Tree, TEXT("Box_CatalogChoices"), true);

		UVerticalBox* QuoteBox = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Quote"));
		UTextBlock* CostPreview = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CostPreview"), true);
		UTextBlock* RevenuePreview = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_RevenuePreview"), true);
		UTextBlock* GoldAfter = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_GoldAfter"), true);

		UHorizontalBox* ActionRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Action"));
		UTextBlock* BlockReason = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_BlockReason"), true);
		UButton* Schedule = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Schedule"), true);
		UTextBlock* ScheduleLabel = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_ScheduleLabel"));

		if (!Root || !Content || !Header || !DailyNotice || !Scroll || !Cards
			|| !FacilityRow || !FacilityLabel || !FacilityChoices || !FacilityNotice || !CatalogChoices
			|| !QuoteBox || !CostPreview || !RevenuePreview || !GoldAfter
			|| !ActionRow || !BlockReason || !Schedule || !ScheduleLabel)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetBrush(MakeSolidBrush(Palette->InGame.Paper, 8.f));
		Root->SetPadding(FMargin(28.f, 24.f));

		if (UVerticalBoxSlot* Slot_Header = Content->AddChildToVerticalBox(Header))
		{
			Slot_Header->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Header->SetHorizontalAlignment(HAlign_Fill);
			Slot_Header->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
		}

		StyleText(*DailyNotice, 13, Palette->InGame.InkMuted, TEXT("Medium"));
		DailyNotice->SetText(LOCTEXT("PrepProgram_NoticePreview",
			"프로그램은 하루짜리입니다. 다음 날에는 다시 편성해야 합니다."));
		DailyNotice->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_Notice = Content->AddChildToVerticalBox(DailyNotice))
		{
			Slot_Notice->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Notice->SetHorizontalAlignment(HAlign_Fill);
			Slot_Notice->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
		}

		// --- 카드 (런타임에 채운다) ---
		Cards->SetInnerSlotPadding(FVector2D(12.f, 12.f));
		if (UScrollBoxSlot* Slot_Cards = Cast<UScrollBoxSlot>(Scroll->AddChild(Cards)))
		{
			Slot_Cards->SetHorizontalAlignment(HAlign_Fill);
			Slot_Cards->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
		}

		// --- 시설 (카드를 고르기 전에는 패널이 접는다) ---
		StyleText(*FacilityLabel, 14, Palette->InGame.Ink, TEXT("Medium"));
		FacilityLabel->SetText(LOCTEXT("PrepProgram_FacilityLabel", "열 장소"));
		if (UVerticalBoxSlot* Slot_FacilityLabel = FacilityRow->AddChildToVerticalBox(FacilityLabel))
		{
			Slot_FacilityLabel->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_FacilityLabel->SetHorizontalAlignment(HAlign_Fill);
			Slot_FacilityLabel->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}

		FacilityChoices->SetInnerSlotPadding(FVector2D(8.f, 8.f));
		if (UVerticalBoxSlot* Slot_FacilityChoices = FacilityRow->AddChildToVerticalBox(FacilityChoices))
		{
			Slot_FacilityChoices->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_FacilityChoices->SetHorizontalAlignment(HAlign_Fill);
		}

		if (UScrollBoxSlot* Slot_FacilityRow = Cast<UScrollBoxSlot>(Scroll->AddChild(FacilityRow)))
		{
			Slot_FacilityRow->SetHorizontalAlignment(HAlign_Fill);
			Slot_FacilityRow->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		}

		StyleText(*FacilityNotice, 13, Palette->InGame.Amber, TEXT("Medium"));
		FacilityNotice->SetText(LOCTEXT("PrepProgram_FacilityNoticePreview",
			"프로그램을 선택하면 필요한 시설과 건축목록 위치를 안내합니다."));
		FacilityNotice->SetAutoWrapText(true);
		if (UScrollBoxSlot* Slot_FacilityNotice = Cast<UScrollBoxSlot>(Scroll->AddChild(FacilityNotice)))
		{
			Slot_FacilityNotice->SetHorizontalAlignment(HAlign_Fill);
			Slot_FacilityNotice->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
		}

		CatalogChoices->SetInnerSlotPadding(FVector2D(8.f, 8.f));
		CatalogChoices->SetVisibility(ESlateVisibility::Collapsed);
		if (UScrollBoxSlot* Slot_Catalog = Cast<UScrollBoxSlot>(Scroll->AddChild(CatalogChoices)))
		{
			Slot_Catalog->SetHorizontalAlignment(HAlign_Fill);
			Slot_Catalog->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
		}

		// --- 견적 ---
		StyleText(*CostPreview, 18, Palette->InGame.Ink, TEXT("SemiBold"));
		CostPreview->SetText(LOCTEXT("PrepProgram_CostPreviewText", "250 Gold"));
		if (UVerticalBoxSlot* Slot_Cost = QuoteBox->AddChildToVerticalBox(CostPreview))
		{
			Slot_Cost->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Cost->SetHorizontalAlignment(HAlign_Fill);
		}

		StyleText(*RevenuePreview, 14, Palette->InGame.BandGood, TEXT("SemiBold"));
		RevenuePreview->SetText(LOCTEXT("PrepProgram_RevenuePreviewText", "확정 매출 400"));
		if (UVerticalBoxSlot* Slot_Revenue = QuoteBox->AddChildToVerticalBox(RevenuePreview))
		{
			Slot_Revenue->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Revenue->SetHorizontalAlignment(HAlign_Fill);
			Slot_Revenue->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}

		StyleText(*GoldAfter, 14, Palette->InGame.InkMuted, TEXT("Medium"));
		GoldAfter->SetText(LOCTEXT("PrepProgram_GoldAfterPreview", "편성 후 잔액 7,350"));
		if (UVerticalBoxSlot* Slot_Gold = QuoteBox->AddChildToVerticalBox(GoldAfter))
		{
			Slot_Gold->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Gold->SetHorizontalAlignment(HAlign_Fill);
			Slot_Gold->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}

		if (UScrollBoxSlot* Slot_Quote = Cast<UScrollBoxSlot>(Scroll->AddChild(QuoteBox)))
		{
			Slot_Quote->SetHorizontalAlignment(HAlign_Fill);
		}

		// 🔴 스크롤은 남은 높이를 전부 받아야 한다. `Automatic` 이면 내용만큼 커져서
		//    액션 행을 다시 밀어낸다 — 스크롤을 둔 이유가 사라진다.
		if (UVerticalBoxSlot* Slot_Scroll = Content->AddChildToVerticalBox(Scroll))
		{
			Slot_Scroll->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot_Scroll->SetHorizontalAlignment(HAlign_Fill);
		}

		// --- 액션 행 (스크롤 밖) ---
		StyleText(*BlockReason, 13, Palette->InGame.Amber, TEXT("Medium"));
		BlockReason->SetText(LOCTEXT("PrepProgram_ReasonPreview", "편성할 프로그램을 고르세요."));
		BlockReason->SetAutoWrapText(true);
		if (UHorizontalBoxSlot* Slot_Reason = ActionRow->AddChildToHorizontalBox(BlockReason))
		{
			Slot_Reason->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot_Reason->SetVerticalAlignment(VAlign_Center);
			Slot_Reason->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
		}

		FButtonStyle ScheduleStyle = Schedule->GetStyle();
		ScheduleStyle.Normal = MakeSolidBrush(Palette->InGame.Teal, 6.f);
		ScheduleStyle.Hovered = MakeSolidBrush(Palette->InGame.TealDeep, 6.f);
		ScheduleStyle.Pressed = MakeSolidBrush(Palette->InGame.TealDeep, 6.f);
		ScheduleStyle.Disabled = MakeSolidBrush(Palette->InGame.LineStrong, 6.f);
		ScheduleStyle.NormalPadding = FMargin(22.f, 8.f);
		ScheduleStyle.PressedPadding = FMargin(22.f, 8.f);
		Schedule->SetStyle(ScheduleStyle);

		StyleText(*ScheduleLabel, 15, Palette->InGame.White, TEXT("SemiBold"));
		ScheduleLabel->SetText(LOCTEXT("PrepProgram_ScheduleLabel", "편성하기"));
		if (UButtonSlot* Slot_ScheduleLabel = Cast<UButtonSlot>(Schedule->AddChild(ScheduleLabel)))
		{
			Slot_ScheduleLabel->SetHorizontalAlignment(HAlign_Center);
			Slot_ScheduleLabel->SetVerticalAlignment(VAlign_Center);
			Slot_ScheduleLabel->SetPadding(FMargin(0.f));
		}
		if (UHorizontalBoxSlot* Slot_Schedule = ActionRow->AddChildToHorizontalBox(Schedule))
		{
			Slot_Schedule->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Schedule->SetVerticalAlignment(VAlign_Center);
		}
		if (UVerticalBoxSlot* Slot_ActionRow = Content->AddChildToVerticalBox(ActionRow))
		{
			Slot_ActionRow->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_ActionRow->SetHorizontalAlignment(HAlign_Fill);
			Slot_ActionRow->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));
		}

		// 🔴 `VAlign_Fill` 이다. `Top` 이면 스크롤이 높이를 못 받아 액션 행이 다시 밀린다.
		if (UBorderSlot* Slot_Content = Cast<UBorderSlot>(Root->AddChild(Content)))
		{
			Slot_Content->SetHorizontalAlignment(HAlign_Fill);
			Slot_Content->SetVerticalAlignment(VAlign_Fill);
		}
		return true;
	}

	// =========================================================================
	// 6) 홍보 카드 · 홍보 패널
	// =========================================================================

	/** 홍보 카드 한 장. 프로그램 카드보다 줄이 적어 폭도 좁다. */
	bool BuildPromotionCard(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: UI 팔레트를 읽을 수 없어 홍보 카드를 만들지 않았다."), PrepLogPrefix);
			return false;
		}

		UButton* Root = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Root"), true);
		USizeBox* Frame = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Card"));
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Card"));
		UTextBlock* Label = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Label"), true);
		UTextBlock* Cost = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Cost"), true);
		UTextBlock* Effect = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Effect"), true);
		UTextBlock* Badge = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Badge"), true);

		if (!Root || !Frame || !Content || !Label || !Cost || !Effect || !Badge)
		{
			return false;
		}

		Tree.RootWidget = Root;

		// ★ 선택 상태 브러시는 `Pressed` 슬롯이다. 베이스가 선택 중에 세 상태에 함께 적용한다 (AT-011).
		FButtonStyle Style = Root->GetStyle();
		Style.Normal = MakeSolidBrush(Palette->InGame.PaperSunken, 6.f);
		Style.Hovered = MakeSolidBrush(Palette->InGame.Hover, 6.f);
		Style.Pressed = MakeSolidBrush(Palette->InGame.TealDeep, 6.f);
		Style.Disabled = MakeSolidBrush(Palette->InGame.Beige, 6.f);
		Style.NormalPadding = FMargin(16.f, 14.f);
		Style.PressedPadding = FMargin(16.f, 14.f);
		Root->SetStyle(Style);

		Frame->SetWidthOverride(220.f);

		StyleText(*Label, 16, Palette->InGame.Ink, TEXT("SemiBold"));
		Label->SetText(LOCTEXT("PrepPromotion_CardLabelPreview", "집중"));
		if (UVerticalBoxSlot* Slot_Label = Content->AddChildToVerticalBox(Label))
		{
			Slot_Label->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Label->SetHorizontalAlignment(HAlign_Fill);
		}

		StyleText(*Cost, 15, Palette->InGame.Ink, TEXT("SemiBold"));
		Cost->SetText(LOCTEXT("PrepPromotion_CardCostPreview", "1,200 Gold"));
		if (UVerticalBoxSlot* Slot_Cost = Content->AddChildToVerticalBox(Cost))
		{
			Slot_Cost->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Cost->SetHorizontalAlignment(HAlign_Fill);
			Slot_Cost->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		}

		StyleText(*Effect, 13, Palette->InGame.InkMuted, TEXT("Medium"));
		Effect->SetText(LOCTEXT("PrepPromotion_CardEffectPreview", "방문객 ×1.18"));
		if (UVerticalBoxSlot* Slot_Effect = Content->AddChildToVerticalBox(Effect))
		{
			Slot_Effect->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Effect->SetHorizontalAlignment(HAlign_Fill);
			Slot_Effect->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
		}

		StyleText(*Badge, 12, Palette->InGame.Teal, TEXT("SemiBold"));
		Badge->SetText(LOCTEXT("PrepPromotion_CardBadgePreview", "✓ 구매함"));
		if (UVerticalBoxSlot* Slot_Badge = Content->AddChildToVerticalBox(Badge))
		{
			Slot_Badge->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Badge->SetHorizontalAlignment(HAlign_Fill);
			Slot_Badge->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		}

		Frame->AddChild(Content);

		if (UButtonSlot* Slot_Frame = Cast<UButtonSlot>(Root->AddChild(Frame)))
		{
			Slot_Frame->SetHorizontalAlignment(HAlign_Fill);
			Slot_Frame->SetVerticalAlignment(VAlign_Fill);
			Slot_Frame->SetPadding(FMargin(0.f));
		}
		return true;
	}

	bool BuildPromotionPanel(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: UI 팔레트를 읽을 수 없어 홍보 패널을 만들지 않았다."), PrepLogPrefix);
			return false;
		}

		UBorder* Root = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_PanelRoot"), true);
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Content"));
		UHorizontalBox* Header = MakePanelHeader(
			Blueprint, Tree, *Palette, LOCTEXT("PrepPromotion_TitlePreview", "홍보"));

		UTextBlock* OnceNotice = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_OnceNotice"), true);
		UTextBlock* NotApplied = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_NotAppliedNotice"), true);

		UScrollBox* Scroll = MakeWidget<UScrollBox>(Blueprint, Tree, TEXT("ScrollBox_Content"));
		UWrapBox* Cards = MakeWidget<UWrapBox>(Blueprint, Tree, TEXT("Box_PromotionCards"), true);

		UVerticalBox* QuoteBox = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Quote"));
		UTextBlock* CostPreview = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CostPreview"), true);
		UTextBlock* GoldAfter = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_GoldAfter"), true);

		UHorizontalBox* ActionRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Action"));
		UTextBlock* BlockReason = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_BlockReason"), true);
		UButton* Purchase = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Purchase"), true);
		UTextBlock* PurchaseLabel = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_PurchaseLabel"));

		if (!Root || !Content || !Header || !OnceNotice || !NotApplied || !Scroll || !Cards
			|| !QuoteBox || !CostPreview || !GoldAfter
			|| !ActionRow || !BlockReason || !Purchase || !PurchaseLabel)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetBrush(MakeSolidBrush(Palette->InGame.Paper, 8.f));
		Root->SetPadding(FMargin(28.f, 24.f));

		if (UVerticalBoxSlot* Slot_Header = Content->AddChildToVerticalBox(Header))
		{
			Slot_Header->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Header->SetHorizontalAlignment(HAlign_Fill);
			Slot_Header->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
		}

		StyleText(*OnceNotice, 13, Palette->InGame.InkMuted, TEXT("Medium"));
		OnceNotice->SetText(LOCTEXT("PrepPromotion_OncePreview",
			"홍보는 축제당 한 번만 고를 수 있고 3일 전체에 적용됩니다. 셋업이 지나면 살 수 없습니다."));
		OnceNotice->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_Once = Content->AddChildToVerticalBox(OnceNotice))
		{
			Slot_Once->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Once->SetHorizontalAlignment(HAlign_Fill);
		}

		// ★ 아직 안 먹는다는 안내는 `Amber` 다. 흘려보내면 안 되는 사실이라 본문보다 눈에 띄어야 한다.
		StyleText(*NotApplied, 13, Palette->InGame.Amber, TEXT("Medium"));
		NotApplied->SetText(LOCTEXT("PrepPromotion_NotAppliedPreview",
			"예상 방문객에는 반영되며, 실제 입장 인원 연동은 준비 중입니다."));
		NotApplied->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_NotApplied = Content->AddChildToVerticalBox(NotApplied))
		{
			Slot_NotApplied->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_NotApplied->SetHorizontalAlignment(HAlign_Fill);
			Slot_NotApplied->SetPadding(FMargin(0.f, 4.f, 0.f, 16.f));
		}

		Cards->SetInnerSlotPadding(FVector2D(12.f, 12.f));
		if (UScrollBoxSlot* Slot_Cards = Cast<UScrollBoxSlot>(Scroll->AddChild(Cards)))
		{
			Slot_Cards->SetHorizontalAlignment(HAlign_Fill);
			Slot_Cards->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
		}

		StyleText(*CostPreview, 18, Palette->InGame.Ink, TEXT("SemiBold"));
		CostPreview->SetText(LOCTEXT("PrepPromotion_CostPreviewText", "1,200 Gold · 방문객 ×1.18"));
		if (UVerticalBoxSlot* Slot_Cost = QuoteBox->AddChildToVerticalBox(CostPreview))
		{
			Slot_Cost->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Cost->SetHorizontalAlignment(HAlign_Fill);
		}

		StyleText(*GoldAfter, 14, Palette->InGame.InkMuted, TEXT("Medium"));
		GoldAfter->SetText(LOCTEXT("PrepPromotion_GoldAfterPreview", "구매 후 잔액 6,400"));
		if (UVerticalBoxSlot* Slot_Gold = QuoteBox->AddChildToVerticalBox(GoldAfter))
		{
			Slot_Gold->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Gold->SetHorizontalAlignment(HAlign_Fill);
			Slot_Gold->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}

		if (UScrollBoxSlot* Slot_Quote = Cast<UScrollBoxSlot>(Scroll->AddChild(QuoteBox)))
		{
			Slot_Quote->SetHorizontalAlignment(HAlign_Fill);
		}

		// 🔴 스크롤이 남은 높이를 전부 받아야 액션 행이 안 밀린다.
		if (UVerticalBoxSlot* Slot_Scroll = Content->AddChildToVerticalBox(Scroll))
		{
			Slot_Scroll->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot_Scroll->SetHorizontalAlignment(HAlign_Fill);
		}

		StyleText(*BlockReason, 13, Palette->InGame.Amber, TEXT("Medium"));
		BlockReason->SetText(LOCTEXT("PrepPromotion_ReasonPreview", "살 홍보 등급을 고르세요."));
		BlockReason->SetAutoWrapText(true);
		if (UHorizontalBoxSlot* Slot_Reason = ActionRow->AddChildToHorizontalBox(BlockReason))
		{
			Slot_Reason->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot_Reason->SetVerticalAlignment(VAlign_Center);
			Slot_Reason->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
		}

		FButtonStyle PurchaseStyle = Purchase->GetStyle();
		PurchaseStyle.Normal = MakeSolidBrush(Palette->InGame.Teal, 6.f);
		PurchaseStyle.Hovered = MakeSolidBrush(Palette->InGame.TealDeep, 6.f);
		PurchaseStyle.Pressed = MakeSolidBrush(Palette->InGame.TealDeep, 6.f);
		PurchaseStyle.Disabled = MakeSolidBrush(Palette->InGame.LineStrong, 6.f);
		PurchaseStyle.NormalPadding = FMargin(22.f, 8.f);
		PurchaseStyle.PressedPadding = FMargin(22.f, 8.f);
		Purchase->SetStyle(PurchaseStyle);

		StyleText(*PurchaseLabel, 15, Palette->InGame.White, TEXT("SemiBold"));
		PurchaseLabel->SetText(LOCTEXT("PrepPromotion_PurchaseLabel", "구매하기"));
		if (UButtonSlot* Slot_PurchaseLabel = Cast<UButtonSlot>(Purchase->AddChild(PurchaseLabel)))
		{
			Slot_PurchaseLabel->SetHorizontalAlignment(HAlign_Center);
			Slot_PurchaseLabel->SetVerticalAlignment(VAlign_Center);
			Slot_PurchaseLabel->SetPadding(FMargin(0.f));
		}
		if (UHorizontalBoxSlot* Slot_Purchase = ActionRow->AddChildToHorizontalBox(Purchase))
		{
			Slot_Purchase->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Purchase->SetVerticalAlignment(VAlign_Center);
		}
		if (UVerticalBoxSlot* Slot_ActionRow = Content->AddChildToVerticalBox(ActionRow))
		{
			Slot_ActionRow->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_ActionRow->SetHorizontalAlignment(HAlign_Fill);
			Slot_ActionRow->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));
		}

		// 🔴 `VAlign_Fill` 이다. `Top` 이면 스크롤이 높이를 못 받는다.
		if (UBorderSlot* Slot_Content = Cast<UBorderSlot>(Root->AddChild(Content)))
		{
			Slot_Content->SetHorizontalAlignment(HAlign_Fill);
			Slot_Content->SetVerticalAlignment(VAlign_Fill);
		}
		return true;
	}

	// =========================================================================
	// 실행
	// =========================================================================

	void RunPrepHudUmg()
	{
		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_BEGIN: 없는 WBP 만 만든다 "
				 "(탭 버튼 → 자리표시 → 인력 → 선택 버튼 → 점검 → 프로그램 → 홍보 → 하단 바)."),
			PrepLogPrefix);

		// 순서가 고정이다. 바가 탭 버튼의 `GeneratedClass` 를 자식으로 넣는다.
		EnsureWidgetBlueprint(
			PrepLogPrefix,
			TabButtonPackage,
			PrepFolder,
			*UFF_PrepHudTabButton::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildTabButton(Blueprint, Tree);
			});

		EnsureWidgetBlueprint(
			PrepLogPrefix,
			PlaceholderPanelPackage,
			PrepPanelsFolder,
			*UFF_PrepPlaceholderPanel::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildPlaceholderPanel(Blueprint, Tree);
			});

		EnsureWidgetBlueprint(
			PrepLogPrefix,
			StaffPanelPackage,
			PrepPanelsFolder,
			*UFF_PrepPanelStaff::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildStaffPanel(Blueprint, Tree);
			});

		EnsureWidgetBlueprint(
			PrepLogPrefix,
			ChoiceButtonPackage,
			PrepFolder,
			*UFF_PrepChoiceButton::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildChoiceButton(Blueprint, Tree);
			});

		// 점검 패널이 선택 버튼의 `GeneratedClass` 를 자식으로 넣는다. 순서가 고정이다.
		UClass* ChoiceButtonClass = LoadGeneratedClass(ChoiceButtonPackage);
		if (ChoiceButtonClass)
		{
			EnsureWidgetBlueprint(
				PrepLogPrefix,
				CheckPanelPackage,
				PrepPanelsFolder,
				*UFF_PrepPanelCheck::StaticClass(),
				[ChoiceButtonClass](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
				{
					return BuildCheckPanel(Blueprint, Tree, *ChoiceButtonClass);
				});
		}
		else
		{
			// 잘못된 기본 타입을 대신 넣지 않는다. 그러면 BindWidget 이 통과해 버리고
			// 런타임에 날씨 버튼이 아무 반응도 하지 않는다.
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 선택 버튼 클래스를 얻지 못해 점검 패널을 만들지 않았다 (%s)."),
				PrepLogPrefix, *ChoiceButtonPackage);
		}

		// 프로그램 카드와 패널. 패널은 카드를 **런타임에** 만들어 넣으므로 여기서는 순서 의존이
		// 없다 — 패널이 이름 경로(`UFF_PrepPanelProgram::CardWidgetFallbackPath`)로 찾는다.
		EnsureWidgetBlueprint(
			PrepLogPrefix,
			ProgramCardPackage,
			PrepFolder,
			*UFF_PrepProgramCard::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildProgramCard(Blueprint, Tree);
			});

		EnsureWidgetBlueprint(
			PrepLogPrefix,
			ProgramPanelPackage,
			PrepPanelsFolder,
			*UFF_PrepPanelProgram::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildProgramPanel(Blueprint, Tree);
			});

		EnsureWidgetBlueprint(
			PrepLogPrefix,
			PromotionCardPackage,
			PrepFolder,
			*UFF_PrepPromotionCard::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildPromotionCard(Blueprint, Tree);
			});

		EnsureWidgetBlueprint(
			PrepLogPrefix,
			PromotionPanelPackage,
			PrepPanelsFolder,
			*UFF_PrepPanelPromotion::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildPromotionPanel(Blueprint, Tree);
			});

		UClass* TabButtonClass = LoadGeneratedClass(TabButtonPackage);
		if (!TabButtonClass)
		{
			// 잘못된 기본 타입을 대신 넣지 않는다. 그러면 `BindWidget` 이 통과해 버리고
			// 런타임에 탭이 아무 반응도 하지 않는다.
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 탭 버튼 클래스를 얻지 못해 하단 바를 만들지 않았다 (%s)."),
				PrepLogPrefix, *TabButtonPackage);
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_END"), PrepLogPrefix);
			return;
		}

		EnsureWidgetBlueprint(
			PrepLogPrefix,
			BarPackage,
			PrepFolder,
			*UFF_PrepHudBar::StaticClass(),
			[TabButtonClass](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildBar(Blueprint, Tree, *TabButtonClass);
			});

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_END: 다음은 Project Settings > Game > FestaFiesta UI Flow 의 "
				 "Stage Screens 에 Setup·PromotionPreview·MorningCheck 를 WBP_FF_PrepHudBar 로 배선한다 "
				 "(Layer 는 HUD 레이어)."),
			PrepLogPrefix);
	}

	/**
	 * Staff 영구 고용·임대료 제거로 레이아웃이 달라진 WBP 5종만 다시 굽는다.
	 *
	 * === 왜 따로 있어야 하나 ===
	 * `FF.PrepHudUMG` 는 **이미 있으면 손대지 않는다.** 디자이너 수정을 지키려고 그렇게
	 * 만든 것이라 옳은데, 그 대신 **위젯이 사라진 변경은 자산에 절대 반영되지 않는다.**
	 * C++ 에서 `Text_Staff` 를 지워도 자산에는 그대로 남고, 이제 아무도 안 채우므로
	 * WBP 에 구워 둔 미리보기 문구(`인원 2명`·`장소 임대료 2,400 / 일`)가 화면에 그대로
	 * 나간다 — 코드에 없는 규칙이 화면에서만 살아 있는 상태가 된다.
	 *
	 * !! 🔴 파괴적이다 !! 이들의 디자이너 수정은 사라진다. 그래서 `FF.PrepHudUMG` 에
	 * 섞지 않고 따로 부르게 뒀다. 나머지는 건드리지 않는다.
	 */
	void RebuildPrepHudStaffUmg()
	{
		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_REBUILD_BEGIN: 인력·점검·프로그램 패널·프로그램 카드·하단 바를 다시 굽는다. "
				 "이들의 디자이너 수정은 사라진다."),
			PrepLogPrefix);

		ReplaceWidgetBlueprint(
			PrepLogPrefix,
			StaffPanelPackage,
			PrepPanelsFolder,
			*UFF_PrepPanelStaff::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildStaffPanel(Blueprint, Tree);
			});

		// 점검 패널의 견적 표에도 `장소 임대료` 줄이 남아 있다. 빌더는 이미 안 만드는데
		// 자산에만 시체로 남아, 없어진 고정비를 화면이 계속 말한다.
		// 선택 버튼 클래스가 있어야 만들 수 있으므로 못 얻으면 점검만 건너뛴다.
		UClass* ChoiceButtonClass = LoadGeneratedClass(ChoiceButtonPackage);
		if (ChoiceButtonClass)
		{
			ReplaceWidgetBlueprint(
				PrepLogPrefix,
				CheckPanelPackage,
				PrepPanelsFolder,
				*UFF_PrepPanelCheck::StaticClass(),
				[ChoiceButtonClass](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
				{
					return BuildCheckPanel(Blueprint, Tree, *ChoiceButtonClass);
				});
		}
		else
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REBUILD_FAIL: 선택 버튼 클래스를 얻지 못해 점검 패널은 그대로 뒀다 (%s). "
					 "견적 표에 임대료 줄이 남는다."),
				PrepLogPrefix, *ChoiceButtonPackage);
		}

		ReplaceWidgetBlueprint(
			PrepLogPrefix,
			ProgramCardPackage,
			PrepFolder,
			*UFF_PrepProgramCard::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildProgramCard(Blueprint, Tree);
			});

		ReplaceWidgetBlueprint(
			PrepLogPrefix,
			ProgramPanelPackage,
			PrepPanelsFolder,
			*UFF_PrepPanelProgram::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildProgramPanel(Blueprint, Tree);
			});

		// 하단 바에도 상단 미납 칸(Box_Arrears)이 남아 있다. 탭 버튼 클래스가 있어야
		// 만들 수 있으므로 못 얻으면 바만 건너뛴다 — 위 셋은 이미 처리됐다.
		UClass* TabButtonClass = LoadGeneratedClass(TabButtonPackage);
		if (TabButtonClass)
		{
			ReplaceWidgetBlueprint(
				PrepLogPrefix,
				BarPackage,
				PrepFolder,
				*UFF_PrepHudBar::StaticClass(),
				[TabButtonClass](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
				{
					return BuildBar(Blueprint, Tree, *TabButtonClass);
				});
		}
		else
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REBUILD_FAIL: 탭 버튼 클래스를 얻지 못해 하단 바는 그대로 뒀다 (%s). "
					 "상단 미납 칸이 남는다."),
				PrepLogPrefix, *TabButtonPackage);
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_REBUILD_END: 임대료·미납·알바·필요 인력 칸이 자산에서 빠졌다."),
			PrepLogPrefix);
	}

	FAutoConsoleCommand PrepHudUmgCommand(
		TEXT("FF.PrepHudUMG"),
		TEXT("준비 단계 HUD 의 WBP 10종"
			 "(탭 버튼·자리표시·인력·선택 버튼·점검·프로그램 카드/패널·홍보 카드/패널·하단 바)을 만든다. "
			 "이미 있으면 건드리지 않는다."),
		FConsoleCommandDelegate::CreateStatic(&RunPrepHudUmg));

	FAutoConsoleCommand RebuildPrepHudStaffUmgCommand(
		TEXT("FF.RebuildPrepHudStaffUI"),
		TEXT("인력·점검·프로그램 패널·프로그램 카드·하단 바를 다시 굽는다. "
			 "Staff 가 영구 고용이 되고 임대료가 없어지면서 자산에만 남은 칸을 걷어낸다. "
			 "🔴 이들의 디자이너 수정은 사라진다."),
		FConsoleCommandDelegate::CreateStatic(&RebuildPrepHudStaffUmg));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
