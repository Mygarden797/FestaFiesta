// FF_LiveHudWbpBuilder.cpp

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
#include "Components/ProgressBar.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/LiveHUD/FF_LiveHudScreen.h"
#include "UI/LiveHUD/FF_LivePanelBase.h"
#include "UI/LiveHUD/Panels/FF_LivePanelOperations.h"
#include "UI/LiveHUD/FF_LiveTabButton.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLiveHudWbpBuilder"

/**
 * 라이브 운영 HUD 의 WBP 골격 생성기 (U6).
 *
 * === 이번에는 생성기를 쓴다 ===
 * 2026-08-13 에 라이브 HUD 생성기를 한 번 폐기했었다. 그때는 항목마다 위치가 달라 코드가
 * 좌표를 들고 있게 됐고, 손으로 조정한 값과 갈라져 어느 쪽이 진짜인지 모르는 문서가 됐다
 * (`docs/memory/wbp-codegen-workflow.md`).
 *
 * **이번 구조는 그때와 다르다.** 준비 단계 바와 같은 골격이라 좌표가 아니라 계층으로 정해진다 —
 * 탭 7개가 같은 구조의 반복이고 `BindWidget` 대상이 25개가 넘는다. 손으로 만들면 이름 하나가
 * 어긋나도 조용히 빈 칸으로 끝난다. 좌표는 여전히 디자이너가 소유한다.
 *
 * === 여기까지가 생성기 담당이다 ===
 * 계층 · 위젯 이름 · `BindWidget` 대상 · **틀리기 쉬운 값** 까지다.
 * 색·패딩·폰트 마감은 디자이너에서 한다. 생성 후 이 `.cpp` 는 레이아웃의 원본이 아니다.
 *
 * === 틀리기 쉬워서 코드로 박아 두는 값 3개 ===
 * 1. 루트 `Canvas_Root` 를 `SelfHitTestInvisible` 로 만든다. `Visible` 이면 화면 전체가 월드
 *    클릭을 먹는다 — 라이브는 월드 조작이 준비 단계보다 잦아 더 치명적이다.
 * 2. 탭 버튼의 **선택 상태 브러시를 `Pressed` 슬롯**에 넣는다. `UFF_HudTabButtonBase` 가
 *    선택 중에 그것을 세 상태에 함께 적용한다. 비우면 색만으로 구분하게 된다 (AT-011).
 * 3. 상태 행을 `Overlay` 로 짠다. `HorizontalBox` 에 나열하면 값이 없어 접히는 칸 때문에
 *    가운데 시계가 좌우로 흔들린다 — 준비 단계에서 실제로 겪었다.
 *
 * **비파괴다.** 이미 있는 패키지는 레이아웃 콜백을 아예 실행하지 않는다.
 */
namespace FFLiveHudWbpBuilder
{
	using namespace FFUIWbpBuilder;

	constexpr const TCHAR* LiveLogPrefix = TEXT("FF_LIVEHUD_UMG");

	const FString LiveFolder = TEXT("/Game/FestaFiesta/UI/LiveHUD");
	const FString LivePanelsFolder = LiveFolder / TEXT("Panels");

	const FString TabButtonPackage = LiveFolder / TEXT("WBP_FF_LiveTabButton");
	const FString ScreenPackage = LiveFolder / TEXT("WBP_FF_LiveHudScreen");

	const FString PlaceholderPanelPackage = LivePanelsFolder / TEXT("WBP_FF_LivePlaceholderPanel");

	// ★ 이름이 계약이다. `UFF_LiveHudScreen::PanelClassNameFormat`(`WBP_FF_LivePanel{0}`) 의
	//   `{0}` 에 `EFF_LiveTab` 항목 이름이 들어간다. 이 이름이 아니면 자리표시가 뜬다.
	const FString OperationsPanelPackage = LivePanelsFolder / TEXT("WBP_FF_LivePanelOperations");

	/** 메뉴 행 + 상태 행. 준비 단계 바와 같은 58 이라 두 화면이 같은 높이로 끝난다. */
	constexpr float MenuRowHeight = 29.f;
	constexpr float StatusRowHeight = 29.f;
	constexpr float BarHeight = MenuRowHeight + StatusRowHeight;

	/** 패널 자리의 디자이너 미리보기 폭. 실제 값은 `UFF_LiveHudScreen` 의 레이아웃 설정이 정한다. */
	constexpr float PanelHostWidth = 520.f;
	constexpr float PanelHostHeight = 620.f;
	constexpr float PanelMargin = 12.f;

	constexpr float TabCornerRadius = 4.f;

	/**
	 * 🔴 텍스처 없이 단색을 칠하는 브러시.
	 *
	 * `FSlateBrush` 기본값은 `DrawAs = Image` + `ImageType = NoImage` 다. 그 상태로 색만 넣으면
	 * `UBorder` 는 아무것도 안 그리고 `UImage` 는 흰 사각형을 그린다. 단색을 텍스처 없이
	 * 칠하는 것은 `RoundedBox` 뿐이고, 기본 `RoundingType` 이 `HalfHeightRadius` 라
	 * 그대로 두면 58px 바가 알약이 된다.
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
				TEXT("%s_FAIL: UI 팔레트를 읽을 수 없어 탭 버튼을 만들지 않았다."), LiveLogPrefix);
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

		// ★ 선택 상태 브러시는 `Pressed` 슬롯이다. 베이스가 선택 중에 세 상태에 함께 적용한다.
		//   배경 형태가 바뀌어야 색맹 사용자도 선택을 구분할 수 있다 (AT-011).
		FButtonStyle Style = Root->GetStyle();
		Style.Normal = MakeSolidBrush(Palette->InGame.Paper, TabCornerRadius);
		Style.Hovered = MakeSolidBrush(Palette->InGame.Hover, TabCornerRadius);
		Style.Pressed = MakeSolidBrush(Palette->InGame.Teal, TabCornerRadius);
		Style.Disabled = MakeSolidBrush(Palette->InGame.PaperSunken, TabCornerRadius);
		Style.NormalPadding = FMargin(10.f, 4.f);
		Style.PressedPadding = FMargin(10.f, 4.f);
		Root->SetStyle(Style);

		Icon->SetColorAndOpacity(Palette->InGame.Ink);

		// ★ `SizeBox` 로 감싸지 않는다. 아이콘이 없을 때 베이스가 `Image_Icon` 을 접는데,
		//   감싸는 `SizeBox` 가 있으면 그것이 빈 자리를 계속 차지한다.
		Icon->SetDesiredSizeOverride(FVector2D(13.f, 13.f));
		if (UHorizontalBoxSlot* Slot_Icon = Content->AddChildToHorizontalBox(Icon))
		{
			Slot_Icon->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Icon->SetVerticalAlignment(VAlign_Center);
			Slot_Icon->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		}

		// 문구는 코드가 채운다. 여기 값은 디자이너 미리보기용이다.
		StyleText(*Label, 12, Palette->InGame.Ink, TEXT("Medium"));
		Label->SetText(LOCTEXT("LiveTab_Preview", "탭"));
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
	// 2) 자리표시 패널
	// =========================================================================

	bool BuildPlaceholderPanel(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: UI 팔레트를 읽을 수 없어 자리표시 패널을 만들지 않았다."), LiveLogPrefix);
			return false;
		}

		UBorder* Root = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_PanelRoot"), true);
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Content"));
		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Header"));
		UTextBlock* Title = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_PanelTitle"), true);
		UButton* Close = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Close"), true);
		UTextBlock* CloseGlyph = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CloseGlyph"));
		UTextBlock* Message = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Placeholder"), true);

		if (!Root || !Content || !Header || !Title || !Close || !CloseGlyph || !Message)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetBrush(MakeSolidBrush(Palette->InGame.Paper, 8.f));
		Root->SetPadding(FMargin(24.f, 20.f));

		StyleText(*Title, 18, Palette->InGame.Ink, TEXT("SemiBold"));
		Title->SetText(LOCTEXT("LivePanel_TitlePreview", "패널"));
		if (UHorizontalBoxSlot* Slot_Title = Header->AddChildToHorizontalBox(Title))
		{
			Slot_Title->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot_Title->SetVerticalAlignment(VAlign_Center);
		}

		FButtonStyle CloseStyle = Close->GetStyle();
		CloseStyle.Normal = MakeSolidBrush(Palette->InGame.PaperSunken, 4.f);
		CloseStyle.Hovered = MakeSolidBrush(Palette->InGame.Hover, 4.f);
		CloseStyle.Pressed = MakeSolidBrush(Palette->InGame.Line, 4.f);
		CloseStyle.NormalPadding = FMargin(8.f, 2.f);
		CloseStyle.PressedPadding = FMargin(8.f, 2.f);
		Close->SetStyle(CloseStyle);

		StyleText(*CloseGlyph, 14, Palette->InGame.InkMuted, TEXT("Medium"));
		CloseGlyph->SetText(LOCTEXT("LivePanel_Close", "×"));
		if (UButtonSlot* Slot_CloseGlyph = Cast<UButtonSlot>(Close->AddChild(CloseGlyph)))
		{
			Slot_CloseGlyph->SetHorizontalAlignment(HAlign_Center);
			Slot_CloseGlyph->SetVerticalAlignment(VAlign_Center);
			Slot_CloseGlyph->SetPadding(FMargin(0.f));
		}
		if (UHorizontalBoxSlot* Slot_Close = Header->AddChildToHorizontalBox(Close))
		{
			Slot_Close->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Close->SetVerticalAlignment(VAlign_Center);
		}

		if (UVerticalBoxSlot* Slot_Header = Content->AddChildToVerticalBox(Header))
		{
			Slot_Header->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Header->SetHorizontalAlignment(HAlign_Fill);
			Slot_Header->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
		}

		StyleText(*Message, 14, Palette->InGame.InkMuted, TEXT("Medium"));
		Message->SetText(LOCTEXT("LivePanel_MessagePreview", "이 패널은 아직 만드는 중입니다."));
		Message->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_Message = Content->AddChildToVerticalBox(Message))
		{
			Slot_Message->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Message->SetHorizontalAlignment(HAlign_Fill);
		}

		if (UBorderSlot* Slot_Content = Cast<UBorderSlot>(Root->AddChild(Content)))
		{
			Slot_Content->SetHorizontalAlignment(HAlign_Fill);
			Slot_Content->SetVerticalAlignment(VAlign_Top);
		}
		return true;
	}

	// =========================================================================
	// 3) 운영 패널
	// =========================================================================

	/** 지표 한 칸. `라벨 / 큰 숫자` 세로 두 줄이다. */
	bool AddMetricCell(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		UHorizontalBox& Row,
		const UFF_UIPalette& Palette,
		const FName TextName,
		const FText& Label,
		const FText& PreviewValue)
	{
		UVerticalBox* Cell = MakeWidget<UVerticalBox>(
			Blueprint, Tree, FName(*(TextName.ToString() + TEXT("_Cell"))));
		UTextBlock* LabelText = MakeWidget<UTextBlock>(
			Blueprint, Tree, FName(*(TextName.ToString() + TEXT("_Label"))));
		UTextBlock* ValueText = MakeWidget<UTextBlock>(Blueprint, Tree, TextName, true);

		if (!Cell || !LabelText || !ValueText)
		{
			return false;
		}

		StyleText(*LabelText, 12, Palette.InGame.InkMuted, TEXT("Medium"));
		LabelText->SetText(Label);
		if (UVerticalBoxSlot* Slot_Label = Cell->AddChildToVerticalBox(LabelText))
		{
			Slot_Label->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Label->SetHorizontalAlignment(HAlign_Left);
		}

		StyleText(*ValueText, 24, Palette.InGame.Ink, TEXT("SemiBold"));
		ValueText->SetText(PreviewValue);
		if (UVerticalBoxSlot* Slot_Value = Cell->AddChildToVerticalBox(ValueText))
		{
			Slot_Value->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Value->SetHorizontalAlignment(HAlign_Left);
			Slot_Value->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
		}

		// 네 칸이 같은 폭을 나눠 갖는다. `Automatic` 이면 숫자 자릿수에 따라 칸이 흔들린다.
		if (UHorizontalBoxSlot* Slot_Cell = Row.AddChildToHorizontalBox(Cell))
		{
			Slot_Cell->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot_Cell->SetVerticalAlignment(VAlign_Top);
		}
		return true;
	}

	bool BuildOperationsPanel(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: UI 팔레트를 읽을 수 없어 운영 패널을 만들지 않았다."), LiveLogPrefix);
			return false;
		}

		UBorder* Root = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_PanelRoot"), true);
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Content"));
		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Header"));
		UTextBlock* Title = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_PanelTitle"), true);
		UButton* Close = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Close"), true);
		UTextBlock* CloseGlyph = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CloseGlyph"));

		// 지표 4종은 2×2 로 놓는다. 한 줄에 넷을 넣으면 520 폭에서 숫자가 잘린다.
		UHorizontalBox* MetricRow1 = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_MetricRow1"));
		UHorizontalBox* MetricRow2 = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_MetricRow2"));

		UTextBlock* GoalTitle = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_GoalTitle"));
		UTextBlock* GoalProgress = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_GoalProgress"), true);
		UProgressBar* GoalBar = MakeWidget<UProgressBar>(Blueprint, Tree, TEXT("Bar_GoalProgress"), true);
		UTextBlock* GoalDetail = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_GoalDetail"), true);

		UHorizontalBox* WarningBox = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Box_Warning"), true);
		UTextBlock* WarningText = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Warning"), true);
		UTextBlock* DummyNotice = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_DummyNotice"), true);

		if (!Root || !Content || !Header || !Title || !Close || !CloseGlyph
			|| !MetricRow1 || !MetricRow2
			|| !GoalTitle || !GoalProgress || !GoalBar || !GoalDetail
			|| !WarningBox || !WarningText || !DummyNotice)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetBrush(MakeSolidBrush(Palette->InGame.Paper, 8.f));
		Root->SetPadding(FMargin(24.f, 20.f));

		// --- 헤더 ---
		StyleText(*Title, 18, Palette->InGame.Ink, TEXT("SemiBold"));
		Title->SetText(LOCTEXT("LiveOps_TitlePreview", "운영"));
		if (UHorizontalBoxSlot* Slot_Title = Header->AddChildToHorizontalBox(Title))
		{
			Slot_Title->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot_Title->SetVerticalAlignment(VAlign_Center);
		}

		FButtonStyle CloseStyle = Close->GetStyle();
		CloseStyle.Normal = MakeSolidBrush(Palette->InGame.PaperSunken, 4.f);
		CloseStyle.Hovered = MakeSolidBrush(Palette->InGame.Hover, 4.f);
		CloseStyle.Pressed = MakeSolidBrush(Palette->InGame.Line, 4.f);
		CloseStyle.NormalPadding = FMargin(8.f, 2.f);
		CloseStyle.PressedPadding = FMargin(8.f, 2.f);
		Close->SetStyle(CloseStyle);

		StyleText(*CloseGlyph, 14, Palette->InGame.InkMuted, TEXT("Medium"));
		CloseGlyph->SetText(LOCTEXT("LiveOps_Close", "×"));
		if (UButtonSlot* Slot_CloseGlyph = Cast<UButtonSlot>(Close->AddChild(CloseGlyph)))
		{
			Slot_CloseGlyph->SetHorizontalAlignment(HAlign_Center);
			Slot_CloseGlyph->SetVerticalAlignment(VAlign_Center);
			Slot_CloseGlyph->SetPadding(FMargin(0.f));
		}
		if (UHorizontalBoxSlot* Slot_Close = Header->AddChildToHorizontalBox(Close))
		{
			Slot_Close->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Close->SetVerticalAlignment(VAlign_Center);
		}

		if (UVerticalBoxSlot* Slot_Header = Content->AddChildToVerticalBox(Header))
		{
			Slot_Header->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Header->SetHorizontalAlignment(HAlign_Fill);
			Slot_Header->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
		}

		// --- 지표 4종 ---
		// ★ 라벨은 `관객 만족도` 다. `평가 점수`(ScoreSatisfaction)를 같이 두지 않는다 —
		//   Live 중에는 값이 같아서 다른 지표로 읽힌다.
		const bool bMetricsOk =
			AddMetricCell(Blueprint, Tree, *MetricRow1, *Palette,
				TEXT("Text_Satisfaction"), LOCTEXT("LiveOps_Satisfaction", "관객 만족도"),
				LOCTEXT("LiveOpsPreview_Satisfaction", "72"))
			&& AddMetricCell(Blueprint, Tree, *MetricRow1, *Palette,
				TEXT("Text_Safety"), LOCTEXT("LiveOps_Safety", "안전"),
				LOCTEXT("LiveOpsPreview_Safety", "88"))
			&& AddMetricCell(Blueprint, Tree, *MetricRow2, *Palette,
				TEXT("Text_TodayRevenue"), LOCTEXT("LiveOps_Revenue", "오늘 매출"),
				LOCTEXT("LiveOpsPreview_Revenue", "3,200"))
			&& AddMetricCell(Blueprint, Tree, *MetricRow2, *Palette,
				TEXT("Text_Gold"), LOCTEXT("LiveOps_Gold", "보유 금화"),
				LOCTEXT("LiveOpsPreview_Gold", "7,600"));

		if (!bMetricsOk)
		{
			return false;
		}

		for (UHorizontalBox* MetricRow : { MetricRow1, MetricRow2 })
		{
			if (UVerticalBoxSlot* Slot_Row = Content->AddChildToVerticalBox(MetricRow))
			{
				Slot_Row->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				Slot_Row->SetHorizontalAlignment(HAlign_Fill);
				Slot_Row->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
			}
		}

		// --- 일일 목표 ---
		StyleText(*GoalTitle, 13, Palette->InGame.InkMuted, TEXT("Medium"));
		GoalTitle->SetText(LOCTEXT("LiveOps_GoalTitle", "일일 목표"));
		if (UVerticalBoxSlot* Slot_GoalTitle = Content->AddChildToVerticalBox(GoalTitle))
		{
			Slot_GoalTitle->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_GoalTitle->SetHorizontalAlignment(HAlign_Fill);
			Slot_GoalTitle->SetPadding(FMargin(0.f, 6.f, 0.f, 4.f));
		}

		StyleText(*GoalProgress, 18, Palette->InGame.Ink, TEXT("SemiBold"));
		GoalProgress->SetText(LOCTEXT("LiveOpsPreview_GoalProgress", "320 / 500명"));
		if (UVerticalBoxSlot* Slot_GoalProgress = Content->AddChildToVerticalBox(GoalProgress))
		{
			Slot_GoalProgress->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_GoalProgress->SetHorizontalAlignment(HAlign_Fill);
		}

		GoalBar->SetPercent(0.64f);
		GoalBar->SetFillColorAndOpacity(Palette->InGame.Teal);
		if (UVerticalBoxSlot* Slot_GoalBar = Content->AddChildToVerticalBox(GoalBar))
		{
			Slot_GoalBar->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_GoalBar->SetHorizontalAlignment(HAlign_Fill);
			Slot_GoalBar->SetPadding(FMargin(0.f, 6.f, 0.f, 4.f));
		}

		// !! 막대만으로는 얼마나 남았는지 알 수 없다 !! (AT-011)
		StyleText(*GoalDetail, 13, Palette->InGame.InkMuted, TEXT("Medium"));
		GoalDetail->SetText(LOCTEXT("LiveOpsPreview_GoalDetail", "180명 더 오면 +300"));
		if (UVerticalBoxSlot* Slot_GoalDetail = Content->AddChildToVerticalBox(GoalDetail))
		{
			Slot_GoalDetail->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_GoalDetail->SetHorizontalAlignment(HAlign_Fill);
			Slot_GoalDetail->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
		}

		// --- 주의 항목. 없으면 줄째 접힌다 ---
		StyleText(*WarningText, 13, Palette->InGame.Amber, TEXT("Medium"));
		WarningText->SetText(LOCTEXT("LiveOpsPreview_Warning",
			"임대료 2,400 이 밀려 있습니다. 매출이 들어와도 잔액이 늘지 않습니다."));
		WarningText->SetAutoWrapText(true);
		if (UHorizontalBoxSlot* Slot_WarningText = WarningBox->AddChildToHorizontalBox(WarningText))
		{
			Slot_WarningText->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot_WarningText->SetVerticalAlignment(VAlign_Center);
		}
		if (UVerticalBoxSlot* Slot_WarningBox = Content->AddChildToVerticalBox(WarningBox))
		{
			Slot_WarningBox->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_WarningBox->SetHorizontalAlignment(HAlign_Fill);
			Slot_WarningBox->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		}

		// ★ 어떤 값이 아직 진짜가 아닌지 적는다. 안 적으면 더미 숫자를 실제로 믿게 된다.
		StyleText(*DummyNotice, 12, Palette->InGame.InkFaint, TEXT("Medium"));
		DummyNotice->SetText(LOCTEXT("LiveOpsPreview_DummyNotice",
			"관객 만족도·안전은 아직 임시 값이고, 관객 수는 소유자 API 가 없어 비어 있습니다."));
		DummyNotice->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_DummyNotice = Content->AddChildToVerticalBox(DummyNotice))
		{
			Slot_DummyNotice->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_DummyNotice->SetHorizontalAlignment(HAlign_Fill);
		}

		if (UBorderSlot* Slot_Content = Cast<UBorderSlot>(Root->AddChild(Content)))
		{
			Slot_Content->SetHorizontalAlignment(HAlign_Fill);
			Slot_Content->SetVerticalAlignment(VAlign_Top);
		}
		return true;
	}

	// =========================================================================
	// 4) 라이브 HUD 루트
	// =========================================================================

	/** 상태 행의 칸 하나. 값이 없으면 `Box_*` 묶음째 접히도록 두 위젯을 함께 만든다. */
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

		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
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
			Slot_Box->SetPadding(FMargin(0.f, 0.f, 18.f, 0.f));
		}
		return Palette != nullptr;
	}

	/** 배속 버튼 하나. */
	UButton* MakeSpeedButton(
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
		Style.Pressed = MakeSolidBrush(Palette.InGame.Teal, 4.f);
		Style.Disabled = MakeSolidBrush(Palette.InGame.Beige, 4.f);
		Style.NormalPadding = FMargin(10.f, 2.f);
		Style.PressedPadding = FMargin(10.f, 2.f);
		Button->SetStyle(Style);

		StyleText(*Label, 12, Palette.InGame.Ink, TEXT("SemiBold"));
		Label->SetText(Glyph);
		if (UButtonSlot* Slot_Label = Cast<UButtonSlot>(Button->AddChild(Label)))
		{
			Slot_Label->SetHorizontalAlignment(HAlign_Center);
			Slot_Label->SetVerticalAlignment(VAlign_Center);
			Slot_Label->SetPadding(FMargin(0.f));
		}
		return Button;
	}

	bool BuildScreen(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, UClass& TabButtonClass)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: UI 팔레트를 읽을 수 없어 라이브 HUD 를 만들지 않았다."), LiveLogPrefix);
			return false;
		}

		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("Canvas_Root"));
		UOverlay* PanelHost = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Panel_PanelHost"), true);
		USizeBox* BarBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Bar"));
		UVerticalBox* BarRows = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Bar"));
		UBorder* MenuRow = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_MenuRow"), true);
		UHorizontalBox* TabRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Tabs"));
		UBorder* StatusRow = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_StatusRow"), true);

		// === 상태 행은 Overlay 다 ===
		// `HorizontalBox` 에 나열하면 값이 없어 접히는 칸(관객 등) 때문에 가운데 시계가
		// 좌우로 흔들린다. 준비 단계에서 겪은 것과 같은 문제라 처음부터 겹쳐 둔다.
		UOverlay* StatusOverlay = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Status"));
		UHorizontalBox* StatusLeft = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_StatusLeft"));
		UHorizontalBox* StatusRight = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_StatusRight"));
		UBorder* DayStageChip = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_DayStage"), true);
		UHorizontalBox* DayStageRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_DayStage"));
		UImage* LiveDot = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_LiveDot"), true);
		UTextBlock* DayStageText = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_DayStage"), true);

		if (!Root || !PanelHost || !BarBox || !BarRows || !MenuRow || !TabRow || !StatusRow
			|| !StatusOverlay || !StatusLeft || !StatusRight
			|| !DayStageChip || !DayStageRow || !LiveDot || !DayStageText)
		{
			return false;
		}

		Tree.RootWidget = Root;

		// 🔴 `Visible` 이면 화면 전체가 월드 클릭을 먹는다. 라이브는 월드 조작이 더 잦다.
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		// --- 패널 자리. 왼쪽 고정이다 ---
		// 앵커·오프셋은 패널을 열 때 `UFF_LiveHudScreen::ApplyPanelLayout` 이 다시 쓴다.
		// 여기 값은 디자이너에서 자리를 보기 위한 기본 배치다.
		PanelHost->SetVisibility(ESlateVisibility::Collapsed);
		// 🔴 **두 축 다 점 앵커다.** 늘어난 축으로 두면 높이를 부모가 정하는데, HUD 루트가
		//    화면 높이를 못 받고 있어서 높이가 음수가 됐다 (2026-08-28 실측). 기준점은
		//    하단 바 위 가운데이고 거기서 위로 자란다 — 준비 단계와 같은 자리다.
		if (UCanvasPanelSlot* Slot_PanelHost = Cast<UCanvasPanelSlot>(Root->AddChild(PanelHost)))
		{
			Slot_PanelHost->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));
			Slot_PanelHost->SetAlignment(FVector2D(0.5f, 1.f));
			Slot_PanelHost->SetOffsets(FMargin(
				0.f, -(BarHeight + PanelMargin), PanelHostWidth, PanelHostHeight));
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

		// --- 메뉴 행. 배경이 버튼 폭까지만 온다 (준비 단계와 같은 판단) ---
		MenuRow->SetBrush(MakeSolidBrush(Palette->InGame.Paper));
		MenuRow->SetPadding(FMargin(4.f, 0.f));
		if (UVerticalBoxSlot* Slot_MenuRow = BarRows->AddChildToVerticalBox(MenuRow))
		{
			Slot_MenuRow->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot_MenuRow->SetHorizontalAlignment(HAlign_Center);
			Slot_MenuRow->SetVerticalAlignment(VAlign_Fill);
		}

		// 탭은 **디자이너에서 보이도록 여기서 직접 놓는다.** 런타임 생성으로 만들면 디자이너에
		// 하나도 안 보여 위치·크기를 손으로 못 만진다 (2026-08-13 에 겪은 문제).
		//
		// ★ 이름이 계약이다. `UFF_LiveHudScreen` 의 `BindWidgetOptional` 과 같아야 붙는다.
		const TCHAR* TabWidgetNames[] = {
			TEXT("Tab_Operations"), TEXT("Tab_Facilities"), TEXT("Tab_Guests"),
			TEXT("Tab_Queues"), TEXT("Tab_SNS"), TEXT("Tab_Report"), TEXT("Tab_Staff") };

		for (const TCHAR* TabWidgetName : TabWidgetNames)
		{
			UUserWidget* TabWidget = MakeWidgetOfClass(Blueprint, Tree, FName(TabWidgetName), &TabButtonClass);
			if (!TabWidget)
			{
				return false;
			}

			if (UHorizontalBoxSlot* Slot_Tab = TabRow->AddChildToHorizontalBox(TabWidget))
			{
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

		// --- 왼쪽 묶음. **준비 단계 바와 같은 순서다** (2026-08-27 백종태 요청) ---
		// 축제 → 금화 → 미납 → 오늘 매출 → 인력 → 관객.
		// ★ 미납은 잔액 바로 옆이어야 "매출이 들어와도 잔액이 안 느는" 것이 설명된다.
		const bool bLeftCellsOk =
			AddStatusCell(Blueprint, Tree, *StatusLeft,
				TEXT("Box_FestivalName"), TEXT("Text_FestivalName"),
				LOCTEXT("LiveStatusPreview_Festival", "다대포 바다축제"), Palette->InGame.Ink)
			&& AddStatusCell(Blueprint, Tree, *StatusLeft,
				TEXT("Box_Gold"), TEXT("Text_Gold"),
				LOCTEXT("LiveStatusPreview_Gold", "7,600"), Palette->InGame.Ink)
			&& AddStatusCell(Blueprint, Tree, *StatusLeft,
				TEXT("Box_Arrears"), TEXT("Text_Arrears"),
				LOCTEXT("LiveStatusPreview_Arrears", "미납 2,400"), Palette->InGame.Danger)
			&& AddStatusCell(Blueprint, Tree, *StatusLeft,
				TEXT("Box_TodayRevenue"), TEXT("Text_TodayRevenue"),
				LOCTEXT("LiveStatusPreview_Revenue", "3,200"), Palette->InGame.Ink)
			&& AddStatusCell(Blueprint, Tree, *StatusLeft,
				TEXT("Box_Staff"), TEXT("Text_Staff"),
				LOCTEXT("LiveStatusPreview_Staff", "6명 중 4명 사용 가능"), Palette->InGame.Ink)
			&& AddStatusCell(Blueprint, Tree, *StatusLeft,
				TEXT("Box_VisitorCount"), TEXT("Text_VisitorCount"),
				LOCTEXT("LiveStatusPreview_Visitors", "1,240"), Palette->InGame.Ink);

		// --- 오른쪽 묶음. 시계 → 배속 ---
		// ★ 시계를 배속 **왼쪽**에 둔다 (2026-08-27 요청). 관객 만족도·안전은 운영 패널로 갔다.
		const bool bRightCellsOk =
			AddStatusCell(Blueprint, Tree, *StatusRight,
				TEXT("Box_Clock"), TEXT("Text_Clock"),
				LOCTEXT("LiveStatusPreview_Clock", "19:42 · 남은 128초"), Palette->InGame.InkMuted);

		if (!bLeftCellsOk || !bRightCellsOk)
		{
			return false;
		}

		// --- 배속 ---
		UHorizontalBox* SpeedBox = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Box_Speed"), true);
		UTextBlock* SpeedReason = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_SpeedReason"), true);
		UButton* Speed1 = MakeSpeedButton(Blueprint, Tree, *Palette, TEXT("Button_Speed1"), LOCTEXT("LiveSpeed_x1", "x1"));
		UButton* Speed2 = MakeSpeedButton(Blueprint, Tree, *Palette, TEXT("Button_Speed2"), LOCTEXT("LiveSpeed_x2", "x2"));
		UButton* Speed4 = MakeSpeedButton(Blueprint, Tree, *Palette, TEXT("Button_Speed4"), LOCTEXT("LiveSpeed_x4", "x4"));

		if (!SpeedBox || !SpeedReason || !Speed1 || !Speed2 || !Speed4)
		{
			return false;
		}

		// !! 흐려짐만으로는 왜 못 바꾸는지 알 수 없다 !! 사유를 버튼 왼쪽에 둔다 (AT-011).
		StyleText(*SpeedReason, 11, Palette->InGame.Amber, TEXT("Medium"));
		SpeedReason->SetText(LOCTEXT("LiveSpeedPreview_Reason", "메인 공연 중에는 1배속으로 고정됩니다."));
		if (UHorizontalBoxSlot* Slot_Reason = SpeedBox->AddChildToHorizontalBox(SpeedReason))
		{
			Slot_Reason->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Reason->SetVerticalAlignment(VAlign_Center);
			Slot_Reason->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
		}

		for (UButton* SpeedButton : { Speed1, Speed2, Speed4 })
		{
			if (UHorizontalBoxSlot* Slot_Speed = SpeedBox->AddChildToHorizontalBox(SpeedButton))
			{
				Slot_Speed->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				Slot_Speed->SetVerticalAlignment(VAlign_Center);
				Slot_Speed->SetPadding(FMargin(0.f, 0.f, 2.f, 0.f));
			}
		}

		if (UHorizontalBoxSlot* Slot_SpeedBox = StatusRight->AddChildToHorizontalBox(SpeedBox))
		{
			Slot_SpeedBox->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_SpeedBox->SetVerticalAlignment(VAlign_Center);
		}

		// --- 중앙 칩 `● 1일차 · 라이브`. 어두운 판 + 흰 글씨라 이 한 줄만 도드라진다 ---
		DayStageChip->SetBrush(MakeSolidBrush(Palette->InGame.TealDeep, 4.f));
		DayStageChip->SetPadding(FMargin(14.f, 2.f));

		// 방송 중을 뜻하는 빨간 점. **카메라 녹화등처럼 아주 작다.**
		// !! 이것만으로 상태를 말하지 않는다 !! 바로 옆에 `라이브` 라는 글자가 있다 (AT-011).
		//
		// 🔴 크기는 `Brush.ImageSize` 로 준다. `SetDesiredSizeOverride` 는 런타임 값이라
		//    생성기가 넣어도 WBP 에 저장되지 않는다 — 그래서 브러시 기본 32x32 가 그대로 나와
		//    점이 아니라 빨간 상자가 됐다 (2026-08-27 실기 확인).
		//    반경을 절반으로 두면 정원이 된다.
		FSlateBrush DotBrush = MakeSolidBrush(Palette->InGame.Danger, 3.f);
		DotBrush.ImageSize = FVector2D(6.f, 6.f);
		LiveDot->SetBrush(DotBrush);
		if (UHorizontalBoxSlot* Slot_Dot = DayStageRow->AddChildToHorizontalBox(LiveDot))
		{
			Slot_Dot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Dot->SetVerticalAlignment(VAlign_Center);
			Slot_Dot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}

		StyleText(*DayStageText, 12, Palette->InGame.White, TEXT("SemiBold"));
		DayStageText->SetText(LOCTEXT("LiveStatusPreview_DayStage", "1일차 · 라이브"));
		DayStageText->SetJustification(ETextJustify::Center);
		if (UHorizontalBoxSlot* Slot_DayStageText = DayStageRow->AddChildToHorizontalBox(DayStageText))
		{
			Slot_DayStageText->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_DayStageText->SetVerticalAlignment(VAlign_Center);
		}

		if (UBorderSlot* Slot_DayStageRow = Cast<UBorderSlot>(DayStageChip->AddChild(DayStageRow)))
		{
			Slot_DayStageRow->SetHorizontalAlignment(HAlign_Center);
			Slot_DayStageRow->SetVerticalAlignment(VAlign_Center);
		}

		// --- 세 묶음을 겹친다. 중앙 칩이 좌우 길이와 무관하게 정중앙에 고정된다 ---
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
	// 실행
	// =========================================================================

	void RunLiveHudUmg()
	{
		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_BEGIN: 없는 WBP 만 만든다 (탭 버튼 → 자리표시 → 운영 패널 → 라이브 HUD)."),
			LiveLogPrefix);

		// 순서가 고정이다. 루트가 탭 버튼의 `GeneratedClass` 를 자식으로 넣는다.
		EnsureWidgetBlueprint(
			LiveLogPrefix,
			TabButtonPackage,
			LiveFolder,
			*UFF_LiveTabButton::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildTabButton(Blueprint, Tree);
			});

		EnsureWidgetBlueprint(
			LiveLogPrefix,
			PlaceholderPanelPackage,
			LivePanelsFolder,
			*UFF_LivePlaceholderPanel::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildPlaceholderPanel(Blueprint, Tree);
			});

		EnsureWidgetBlueprint(
			LiveLogPrefix,
			OperationsPanelPackage,
			LivePanelsFolder,
			*UFF_LivePanelOperations::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildOperationsPanel(Blueprint, Tree);
			});

		UClass* TabButtonClass = LoadGeneratedClass(TabButtonPackage);
		if (!TabButtonClass)
		{
			// 잘못된 기본 타입을 대신 넣지 않는다. 그러면 `BindWidget` 이 통과해 버리고
			// 런타임에 탭이 아무 반응도 하지 않는다.
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 탭 버튼 클래스를 얻지 못해 라이브 HUD 를 만들지 않았다 (%s)."),
				LiveLogPrefix, *TabButtonPackage);
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_END"), LiveLogPrefix);
			return;
		}

		EnsureWidgetBlueprint(
			LiveLogPrefix,
			ScreenPackage,
			LiveFolder,
			*UFF_LiveHudScreen::StaticClass(),
			[TabButtonClass](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildScreen(Blueprint, Tree, *TabButtonClass);
			});

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_END: 다음은 Project Settings > Game > FestaFiesta UI Flow 의 "
				 "Stage Screens 에서 `Live` 를 WBP_FF_LiveHudScreen 으로 바꾼다 "
				 "(지금은 WBP_FF_FlowStub 이다)."),
			LiveLogPrefix);
	}

	FAutoConsoleCommand LiveHudUmgCommand(
		TEXT("FF.LiveHudUMG"),
		TEXT("라이브 운영 HUD 의 WBP 4종(탭 버튼·자리표시 패널·운영 패널·라이브 HUD)을 만든다. "
			 "이미 있으면 건드리지 않는다."),
		FConsoleCommandDelegate::CreateStatic(&RunLiveHudUmg));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
