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
#include "UI/LiveHUD/Panels/FF_LivePanelFacilities.h"
#include "UI/LiveHUD/Panels/FF_LivePanelGuests.h"
#include "UI/LiveHUD/Panels/FF_LivePanelQueues.h"
#include "UI/LiveHUD/Panels/FF_LivePanelFinale.h"
#include "UI/LiveHUD/Panels/FF_LivePanelReport.h"
#include "UI/LiveHUD/Panels/FF_LivePanelSNS.h"
#include "UI/LiveHUD/Panels/FF_LivePanelStaff.h"
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
	const FString StaffPanelPackage = LivePanelsFolder / TEXT("WBP_FF_LivePanelStaff");
	const FString FacilitiesPanelPackage = LivePanelsFolder / TEXT("WBP_FF_LivePanelFacilities");
	const FString QueuesPanelPackage = LivePanelsFolder / TEXT("WBP_FF_LivePanelQueues");
	const FString GuestsPanelPackage = LivePanelsFolder / TEXT("WBP_FF_LivePanelGuests");
	const FString SNSPanelPackage = LivePanelsFolder / TEXT("WBP_FF_LivePanelSNS");
	const FString ReportPanelPackage = LivePanelsFolder / TEXT("WBP_FF_LivePanelReport");
	const FString FinalePanelPackage = LivePanelsFolder / TEXT("WBP_FF_LivePanelFinale");

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
	// 3-2) 스태프 패널
	// =========================================================================

	bool BuildStaffPanel(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: UI 팔레트를 읽을 수 없어 스태프 패널을 만들지 않았다."), LiveLogPrefix);
			return false;
		}

		UBorder* Root = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_PanelRoot"), true);
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Content"));
		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Header"));
		UTextBlock* Title = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_PanelTitle"), true);
		UButton* Close = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Close"), true);
		UTextBlock* CloseGlyph = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CloseGlyph"));

		// 요약은 3칸이라 한 줄에 들어간다. 운영 패널이 2×2 인 것은 넷이라서다.
		UHorizontalBox* SummaryRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Summary"));
		UTextBlock* Breakdown = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_StaffBreakdown"), true);
		UTextBlock* IdleNote = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_IdleNote"), true);

		UTextBlock* AssignTitle = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_AssignmentsTitle"));
		UVerticalBox* AssignBox = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Box_Assignments"), true);
		UTextBlock* AssignEmpty = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_AssignmentsEmpty"), true);

		UTextBlock* Wage = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_DailyWage"), true);
		UTextBlock* HireNote = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_HireNote"), true);
		UTextBlock* MissingNote = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_MissingNote"), true);

		if (!Root || !Content || !Header || !Title || !Close || !CloseGlyph
			|| !SummaryRow || !Breakdown || !IdleNote
			|| !AssignTitle || !AssignBox || !AssignEmpty
			|| !Wage || !HireNote || !MissingNote)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetBrush(MakeSolidBrush(Palette->InGame.Paper, 8.f));
		Root->SetPadding(FMargin(24.f, 20.f));

		// --- 헤더 ---
		StyleText(*Title, 18, Palette->InGame.Ink, TEXT("SemiBold"));
		Title->SetText(LOCTEXT("LiveStaff_TitlePreview", "스태프"));
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
		CloseGlyph->SetText(LOCTEXT("LiveStaff_Close", "×"));
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

		// --- 요약 3칸 ---
		const bool bSummaryOk =
			AddMetricCell(Blueprint, Tree, *SummaryRow, *Palette,
				TEXT("Text_TotalStaff"), LOCTEXT("LiveStaff_Total", "전체 인원"),
				LOCTEXT("LiveStaffPreview_Total", "12명"))
			&& AddMetricCell(Blueprint, Tree, *SummaryRow, *Palette,
				TEXT("Text_AssignedStaff"), LOCTEXT("LiveStaff_Assigned", "프로그램 배치"),
				LOCTEXT("LiveStaffPreview_Assigned", "5명"))
			&& AddMetricCell(Blueprint, Tree, *SummaryRow, *Palette,
				TEXT("Text_IdleStaff"), LOCTEXT("LiveStaff_Idle", "남는 인원"),
				LOCTEXT("LiveStaffPreview_Idle", "7명"));

		if (!bSummaryOk)
		{
			return false;
		}
		if (UVerticalBoxSlot* Slot_Summary = Content->AddChildToVerticalBox(SummaryRow))
		{
			Slot_Summary->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Summary->SetHorizontalAlignment(HAlign_Fill);
			Slot_Summary->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}

		// 전체 숫자만으로는 내일 몇 명이 남는지 알 수 없다. 알바는 하루짜리다.
		StyleText(*Breakdown, 13, Palette->InGame.InkMuted, TEXT("Medium"));
		Breakdown->SetText(LOCTEXT("LiveStaffPreview_Breakdown", "기본 6 · 알바 6"));
		if (UVerticalBoxSlot* Slot_Breakdown = Content->AddChildToVerticalBox(Breakdown))
		{
			Slot_Breakdown->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Breakdown->SetHorizontalAlignment(HAlign_Fill);
		}

		// !! 숫자만 두지 않는다 !! 남는 것이 좋은 일인지 나쁜 일인지 숫자로는 알 수 없다 (AT-011).
		StyleText(*IdleNote, 13, Palette->InGame.Amber, TEXT("Medium"));
		IdleNote->SetText(LOCTEXT("LiveStaffPreview_IdleNote",
			"임금은 그대로 나가는데 아무 프로그램에도 묶여 있지 않습니다."));
		IdleNote->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_IdleNote = Content->AddChildToVerticalBox(IdleNote))
		{
			Slot_IdleNote->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_IdleNote->SetHorizontalAlignment(HAlign_Fill);
			Slot_IdleNote->SetPadding(FMargin(0.f, 4.f, 0.f, 18.f));
		}

		// --- 오늘 편성 ---
		StyleText(*AssignTitle, 13, Palette->InGame.InkMuted, TEXT("Medium"));
		AssignTitle->SetText(LOCTEXT("LiveStaff_AssignmentsTitle", "오늘 편성"));
		if (UVerticalBoxSlot* Slot_AssignTitle = Content->AddChildToVerticalBox(AssignTitle))
		{
			Slot_AssignTitle->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_AssignTitle->SetHorizontalAlignment(HAlign_Fill);
			Slot_AssignTitle->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}

		// 줄은 코드가 런타임에 채운다. 여기서는 자리만 만든다.
		if (UVerticalBoxSlot* Slot_AssignBox = Content->AddChildToVerticalBox(AssignBox))
		{
			Slot_AssignBox->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_AssignBox->SetHorizontalAlignment(HAlign_Fill);
		}

		StyleText(*AssignEmpty, 13, Palette->InGame.InkFaint, TEXT("Medium"));
		AssignEmpty->SetText(LOCTEXT("LiveStaffPreview_AssignEmpty",
			"오늘 편성한 유료 프로그램이 없어 인력이 묶여 있지 않습니다."));
		AssignEmpty->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_AssignEmpty = Content->AddChildToVerticalBox(AssignEmpty))
		{
			Slot_AssignEmpty->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_AssignEmpty->SetHorizontalAlignment(HAlign_Fill);
			Slot_AssignEmpty->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
		}

		// --- 비용과 안내 ---
		StyleText(*Wage, 13, Palette->InGame.Ink, TEXT("Medium"));
		Wage->SetText(LOCTEXT("LiveStaffPreview_Wage", "오늘 알바 임금 900 / 일"));
		if (UVerticalBoxSlot* Slot_Wage = Content->AddChildToVerticalBox(Wage))
		{
			Slot_Wage->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Wage->SetHorizontalAlignment(HAlign_Fill);
			Slot_Wage->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		}

		// ★ 못 하는 것은 흐려 놓지 말고 이유를 적는다 (AT-011 과 같은 이유).
		StyleText(*HireNote, 12, Palette->InGame.InkMuted, TEXT("Medium"));
		HireNote->SetText(LOCTEXT("LiveStaffPreview_HireNote",
			"알바는 하루짜리라 셋업과 개장 준비에서만 뽑습니다. 라이브 중에는 늘릴 수 없습니다."));
		HireNote->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_HireNote = Content->AddChildToVerticalBox(HireNote))
		{
			Slot_HireNote->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_HireNote->SetHorizontalAlignment(HAlign_Fill);
			Slot_HireNote->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}

		StyleText(*MissingNote, 12, Palette->InGame.InkFaint, TEXT("Medium"));
		MissingNote->SetText(LOCTEXT("LiveStaffPreview_MissingNote",
			"시설별 배치와 스태프 개인 상태는 아직 읽을 수 없습니다."));
		MissingNote->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_MissingNote = Content->AddChildToVerticalBox(MissingNote))
		{
			Slot_MissingNote->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_MissingNote->SetHorizontalAlignment(HAlign_Fill);
		}

		if (UBorderSlot* Slot_Content = Cast<UBorderSlot>(Root->AddChild(Content)))
		{
			Slot_Content->SetHorizontalAlignment(HAlign_Fill);
			Slot_Content->SetVerticalAlignment(VAlign_Top);
		}
		return true;
	}

	// =========================================================================
	// 3-3) 시설 패널
	// =========================================================================

	bool BuildFacilitiesPanel(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: UI 팔레트를 읽을 수 없어 시설 패널을 만들지 않았다."), LiveLogPrefix);
			return false;
		}

		UBorder* Root = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_PanelRoot"), true);
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Content"));
		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Header"));
		UTextBlock* Title = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_PanelTitle"), true);
		UButton* Close = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Close"), true);
		UTextBlock* CloseGlyph = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CloseGlyph"));

		UHorizontalBox* SummaryRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Summary"));
		UTextBlock* Summary = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Summary"), true);

		UVerticalBox* ListBox = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Box_Facilities"), true);
		UTextBlock* Empty = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Empty"), true);
		UTextBlock* MissingNote = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_MissingNote"), true);

		if (!Root || !Content || !Header || !Title || !Close || !CloseGlyph
			|| !SummaryRow || !Summary || !ListBox || !Empty || !MissingNote)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetBrush(MakeSolidBrush(Palette->InGame.Paper, 8.f));
		Root->SetPadding(FMargin(24.f, 20.f));

		// --- 헤더 ---
		StyleText(*Title, 18, Palette->InGame.Ink, TEXT("SemiBold"));
		Title->SetText(LOCTEXT("LiveFac_TitlePreview", "시설"));
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
		CloseGlyph->SetText(LOCTEXT("LiveFac_Close", "×"));
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

		// --- 요약. 개수 한 칸 + 급한 것 몇 개인지 한 줄 ---
		if (!AddMetricCell(Blueprint, Tree, *SummaryRow, *Palette,
			TEXT("Text_FacilityCount"), LOCTEXT("LiveFac_CountLabel", "놓인 시설"),
			LOCTEXT("LiveFacPreview_Count", "6개")))
		{
			return false;
		}
		if (UVerticalBoxSlot* Slot_SummaryRow = Content->AddChildToVerticalBox(SummaryRow))
		{
			Slot_SummaryRow->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_SummaryRow->SetHorizontalAlignment(HAlign_Fill);
		}

		// 숫자만 두면 "많은 건가" 를 사람이 판단해야 한다. 급한 것이 몇 개인지 함께 적는다.
		StyleText(*Summary, 13, Palette->InGame.Amber, TEXT("Medium"));
		Summary->SetText(LOCTEXT("LiveFacPreview_Summary", "막힘 1개 · 주의 2개"));
		Summary->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_Summary = Content->AddChildToVerticalBox(Summary))
		{
			Slot_Summary->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Summary->SetHorizontalAlignment(HAlign_Fill);
			Slot_Summary->SetPadding(FMargin(0.f, 4.f, 0.f, 18.f));
		}

		// --- 목록. 줄은 코드가 런타임에 채운다 ---
		if (UVerticalBoxSlot* Slot_ListBox = Content->AddChildToVerticalBox(ListBox))
		{
			Slot_ListBox->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_ListBox->SetHorizontalAlignment(HAlign_Fill);
		}

		StyleText(*Empty, 13, Palette->InGame.InkFaint, TEXT("Medium"));
		Empty->SetText(LOCTEXT("LiveFacPreview_Empty", "놓인 시설이 없습니다."));
		Empty->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_Empty = Content->AddChildToVerticalBox(Empty))
		{
			Slot_Empty->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Empty->SetHorizontalAlignment(HAlign_Fill);
			Slot_Empty->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
		}

		// ★ 여기서 무엇을 할 수 없는지 적는다. 목록만 두면 눌러 볼 수 있다고 오해한다.
		StyleText(*MissingNote, 12, Palette->InGame.InkFaint, TEXT("Medium"));
		MissingNote->SetText(LOCTEXT("LiveFacPreview_MissingNote",
			"시설을 열고 닫거나 옮기는 것은 건축 화면에서 합니다. 여기서는 상태만 봅니다."));
		MissingNote->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_MissingNote = Content->AddChildToVerticalBox(MissingNote))
		{
			Slot_MissingNote->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_MissingNote->SetHorizontalAlignment(HAlign_Fill);
		}

		if (UBorderSlot* Slot_Content = Cast<UBorderSlot>(Root->AddChild(Content)))
		{
			Slot_Content->SetHorizontalAlignment(HAlign_Fill);
			Slot_Content->SetVerticalAlignment(VAlign_Top);
		}
		return true;
	}

	// =========================================================================
	// 3-4) 대기열 패널
	// =========================================================================

	bool BuildQueuesPanel(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: UI 팔레트를 읽을 수 없어 대기열 패널을 만들지 않았다."), LiveLogPrefix);
			return false;
		}

		UBorder* Root = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_PanelRoot"), true);
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Content"));
		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Header"));
		UTextBlock* Title = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_PanelTitle"), true);
		UButton* Close = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Close"), true);
		UTextBlock* CloseGlyph = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CloseGlyph"));

		UHorizontalBox* SummaryRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Summary"));
		UTextBlock* AverageNote = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_AverageNote"), true);
		UTextBlock* Longest = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Longest"), true);
		UTextBlock* StateCounts = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_StateCounts"), true);

		UTextBlock* ListTitle = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_ListTitle"));
		UVerticalBox* ListBox = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Box_Queues"), true);
		UTextBlock* Empty = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Empty"), true);
		UTextBlock* MissingNote = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_MissingNote"), true);

		if (!Root || !Content || !Header || !Title || !Close || !CloseGlyph
			|| !SummaryRow || !AverageNote || !Longest || !StateCounts
			|| !ListTitle || !ListBox || !Empty || !MissingNote)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetBrush(MakeSolidBrush(Palette->InGame.Paper, 8.f));
		Root->SetPadding(FMargin(24.f, 20.f));

		// --- 헤더 ---
		StyleText(*Title, 18, Palette->InGame.Ink, TEXT("SemiBold"));
		Title->SetText(LOCTEXT("LiveQueue_TitlePreview", "대기열"));
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
		CloseGlyph->SetText(LOCTEXT("LiveQueue_Close", "×"));
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

		// --- 요약 3칸 ---
		const bool bSummaryOk =
			AddMetricCell(Blueprint, Tree, *SummaryRow, *Palette,
				TEXT("Text_TotalWaiting"), LOCTEXT("LiveQueue_TotalLabel", "총 대기"),
				LOCTEXT("LiveQueuePreview_Total", "26명"))
			&& AddMetricCell(Blueprint, Tree, *SummaryRow, *Palette,
				TEXT("Text_InService"), LOCTEXT("LiveQueue_ServiceLabel", "이용 중"),
				LOCTEXT("LiveQueuePreview_Service", "7명"))
			&& AddMetricCell(Blueprint, Tree, *SummaryRow, *Palette,
				TEXT("Text_AverageWait"), LOCTEXT("LiveQueue_AvgLabel", "평균 대기"),
				LOCTEXT("LiveQueuePreview_Avg", "62초"));

		if (!bSummaryOk)
		{
			return false;
		}
		if (UVerticalBoxSlot* Slot_SummaryRow = Content->AddChildToVerticalBox(SummaryRow))
		{
			Slot_SummaryRow->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_SummaryRow->SetHorizontalAlignment(HAlign_Fill);
		}

		// 한 곳만 센 평균과 여섯 곳을 센 평균은 뜻이 다르다. 몇 곳을 셌는지 적는다.
		StyleText(*AverageNote, 12, Palette->InGame.InkFaint, TEXT("Medium"));
		AverageNote->SetText(LOCTEXT("LiveQueuePreview_AvgNote", "줄을 끝낸 손님이 있는 4곳 기준"));
		AverageNote->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_AverageNote = Content->AddChildToVerticalBox(AverageNote))
		{
			Slot_AverageNote->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_AverageNote->SetHorizontalAlignment(HAlign_Fill);
			Slot_AverageNote->SetPadding(FMargin(0.f, 4.f, 0.f, 12.f));
		}

		StyleText(*Longest, 14, Palette->InGame.Ink, TEXT("SemiBold"));
		Longest->SetText(LOCTEXT("LiveQueuePreview_Longest", "가장 긴 줄 · 화장실A 14명"));
		Longest->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_Longest = Content->AddChildToVerticalBox(Longest))
		{
			Slot_Longest->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Longest->SetHorizontalAlignment(HAlign_Fill);
		}

		StyleText(*StateCounts, 13, Palette->InGame.InkMuted, TEXT("Medium"));
		StateCounts->SetText(LOCTEXT("LiveQueuePreview_StateCounts",
			"받는 중 4 · 정원 참 1 · 안 받음 1 · 막힘 0"));
		StateCounts->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_StateCounts = Content->AddChildToVerticalBox(StateCounts))
		{
			Slot_StateCounts->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_StateCounts->SetHorizontalAlignment(HAlign_Fill);
			Slot_StateCounts->SetPadding(FMargin(0.f, 4.f, 0.f, 18.f));
		}

		// --- 줄별 목록. 줄은 코드가 런타임에 채운다 ---
		StyleText(*ListTitle, 13, Palette->InGame.InkMuted, TEXT("Medium"));
		ListTitle->SetText(LOCTEXT("LiveQueue_ListTitle", "줄이 긴 순"));
		if (UVerticalBoxSlot* Slot_ListTitle = Content->AddChildToVerticalBox(ListTitle))
		{
			Slot_ListTitle->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_ListTitle->SetHorizontalAlignment(HAlign_Fill);
			Slot_ListTitle->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}

		if (UVerticalBoxSlot* Slot_ListBox = Content->AddChildToVerticalBox(ListBox))
		{
			Slot_ListBox->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_ListBox->SetHorizontalAlignment(HAlign_Fill);
		}

		StyleText(*Empty, 13, Palette->InGame.InkFaint, TEXT("Medium"));
		Empty->SetText(LOCTEXT("LiveQueuePreview_Empty", "줄을 세울 시설이 없습니다."));
		Empty->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_Empty = Content->AddChildToVerticalBox(Empty))
		{
			Slot_Empty->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Empty->SetHorizontalAlignment(HAlign_Fill);
			Slot_Empty->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
		}

		// ★ 아직 못 보여주는 것을 적는다. 안 적으면 빠진 줄 모른다.
		StyleText(*MissingNote, 12, Palette->InGame.InkFaint, TEXT("Medium"));
		MissingNote->SetText(LOCTEXT("LiveQueuePreview_MissingNote",
			"예상 대기 시간과 줄을 포기한 손님 수는 아직 읽을 수 없습니다."));
		MissingNote->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_MissingNote = Content->AddChildToVerticalBox(MissingNote))
		{
			Slot_MissingNote->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_MissingNote->SetHorizontalAlignment(HAlign_Fill);
		}

		if (UBorderSlot* Slot_Content = Cast<UBorderSlot>(Root->AddChild(Content)))
		{
			Slot_Content->SetHorizontalAlignment(HAlign_Fill);
			Slot_Content->SetVerticalAlignment(VAlign_Top);
		}
		return true;
	}

	// =========================================================================
	// 3-5) 관객 패널
	// =========================================================================

	bool BuildGuestsPanel(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: UI 팔레트를 읽을 수 없어 관객 패널을 만들지 않았다."), LiveLogPrefix);
			return false;
		}

		UBorder* Root = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_PanelRoot"), true);
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Content"));
		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Header"));
		UTextBlock* Title = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_PanelTitle"), true);
		UButton* Close = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Close"), true);
		UTextBlock* CloseGlyph = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CloseGlyph"));

		UHorizontalBox* SummaryRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Summary"));
		UTextBlock* VisitorNote = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_VisitorNote"), true);
		UTextBlock* Summary = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Summary"), true);

		UTextBlock* ListTitle = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_NeedsTitle"));
		UVerticalBox* NeedsBox = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Box_Needs"), true);
		UTextBlock* Empty = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Empty"), true);
		UTextBlock* MissingNote = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_MissingNote"), true);

		if (!Root || !Content || !Header || !Title || !Close || !CloseGlyph
			|| !SummaryRow || !VisitorNote || !Summary
			|| !ListTitle || !NeedsBox || !Empty || !MissingNote)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetBrush(MakeSolidBrush(Palette->InGame.Paper, 8.f));
		Root->SetPadding(FMargin(24.f, 20.f));

		// --- 헤더 ---
		StyleText(*Title, 18, Palette->InGame.Ink, TEXT("SemiBold"));
		Title->SetText(LOCTEXT("LiveGuests_TitlePreview", "관객"));
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
		CloseGlyph->SetText(LOCTEXT("LiveGuests_Close", "×"));
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

		// --- 인원 ---
		if (!AddMetricCell(Blueprint, Tree, *SummaryRow, *Palette,
			TEXT("Text_VisitorCount"), LOCTEXT("LiveGuests_VisitorLabel", "지금 관객"),
			LOCTEXT("LiveGuestsPv_Visitor", "84명")))
		{
			return false;
		}
		if (UVerticalBoxSlot* Slot_SummaryRow = Content->AddChildToVerticalBox(SummaryRow))
		{
			Slot_SummaryRow->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_SummaryRow->SetHorizontalAlignment(HAlign_Fill);
		}

		// ★ 동시 인원임을 밝힌다. 일일 목표(누적)와 다른 값이라 그냥 두면 오해한다.
		StyleText(*VisitorNote, 12, Palette->InGame.InkFaint, TEXT("Medium"));
		VisitorNote->SetText(LOCTEXT("LiveGuestsPv_VisitorNote",
			"지금 축제장 안에 있는 인원입니다. 오늘 누적 방문객과는 다릅니다."));
		VisitorNote->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_VisitorNote = Content->AddChildToVerticalBox(VisitorNote))
		{
			Slot_VisitorNote->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_VisitorNote->SetHorizontalAlignment(HAlign_Fill);
			Slot_VisitorNote->SetPadding(FMargin(0.f, 4.f, 0.f, 18.f));
		}

		// --- 욕구 ---
		StyleText(*ListTitle, 13, Palette->InGame.InkMuted, TEXT("Medium"));
		ListTitle->SetText(LOCTEXT("LiveGuests_NeedsTitle", "관객이 못 하고 있는 것"));
		if (UVerticalBoxSlot* Slot_ListTitle = Content->AddChildToVerticalBox(ListTitle))
		{
			Slot_ListTitle->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_ListTitle->SetHorizontalAlignment(HAlign_Fill);
			Slot_ListTitle->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
		}

		// 🔴 표본이 없으면 여기에 그 사실이 크게 뜬다. 숫자만 두면 전부 `0%` 로 보이고
		//    그것은 "아무도 불편하지 않다" 로 읽힌다.
		StyleText(*Summary, 13, Palette->InGame.Amber, TEXT("Medium"));
		Summary->SetText(LOCTEXT("LiveGuestsPv_Summary", "관객 84명 기준 · 나쁨 1 · 주의 1"));
		Summary->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_Summary = Content->AddChildToVerticalBox(Summary))
		{
			Slot_Summary->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Summary->SetHorizontalAlignment(HAlign_Fill);
			Slot_Summary->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		}

		// 줄은 코드가 런타임에 채운다.
		if (UVerticalBoxSlot* Slot_NeedsBox = Content->AddChildToVerticalBox(NeedsBox))
		{
			Slot_NeedsBox->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_NeedsBox->SetHorizontalAlignment(HAlign_Fill);
		}

		StyleText(*Empty, 13, Palette->InGame.InkFaint, TEXT("Medium"));
		Empty->SetText(LOCTEXT("LiveGuestsPv_Empty", "욕구 집계를 읽을 수 없습니다."));
		Empty->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_Empty = Content->AddChildToVerticalBox(Empty))
		{
			Slot_Empty->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Empty->SetHorizontalAlignment(HAlign_Fill);
			Slot_Empty->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
		}

		StyleText(*MissingNote, 12, Palette->InGame.InkFaint, TEXT("Medium"));
		MissingNote->SetText(LOCTEXT("LiveGuestsPv_MissingNote",
			"관객 그룹 구성과 오늘 누적 방문객 수는 아직 읽을 수 없습니다."));
		MissingNote->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_MissingNote = Content->AddChildToVerticalBox(MissingNote))
		{
			Slot_MissingNote->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_MissingNote->SetHorizontalAlignment(HAlign_Fill);
		}

		if (UBorderSlot* Slot_Content = Cast<UBorderSlot>(Root->AddChild(Content)))
		{
			Slot_Content->SetHorizontalAlignment(HAlign_Fill);
			Slot_Content->SetVerticalAlignment(VAlign_Top);
		}
		return true;
	}

	// =========================================================================
	// 3-6) SNS 패널 — 지금은 빈 화면이고 그것이 의도다
	// =========================================================================

	bool BuildSNSPanel(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: UI 팔레트를 읽을 수 없어 SNS 패널을 만들지 않았다."), LiveLogPrefix);
			return false;
		}

		UBorder* Root = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_PanelRoot"), true);
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Content"));
		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Header"));
		UTextBlock* Title = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_PanelTitle"), true);
		UButton* Close = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Close"), true);
		UTextBlock* CloseGlyph = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CloseGlyph"));

		UVerticalBox* PostsBox = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Box_Posts"), true);
		UTextBlock* Empty = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Empty"), true);
		UTextBlock* MissingNote = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_MissingNote"), true);

		if (!Root || !Content || !Header || !Title || !Close || !CloseGlyph
			|| !PostsBox || !Empty || !MissingNote)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetBrush(MakeSolidBrush(Palette->InGame.Paper, 8.f));
		Root->SetPadding(FMargin(24.f, 20.f));

		// --- 헤더 ---
		StyleText(*Title, 18, Palette->InGame.Ink, TEXT("SemiBold"));
		Title->SetText(LOCTEXT("LiveSNS_TitlePreview", "SNS"));
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
		CloseGlyph->SetText(LOCTEXT("LiveSNS_Close", "×"));
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

		// 🔴 **가짜 반응을 미리보기로도 넣지 않는다.** 디자이너에서 그럴듯한 글이 보이면
		//    팀이 "SNS 는 됐다" 고 생각해서 진짜 API 가 안 만들어진다.
		if (UVerticalBoxSlot* Slot_PostsBox = Content->AddChildToVerticalBox(PostsBox))
		{
			Slot_PostsBox->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_PostsBox->SetHorizontalAlignment(HAlign_Fill);
		}

		StyleText(*Empty, 13, Palette->InGame.InkMuted, TEXT("Medium"));
		Empty->SetText(LOCTEXT("LiveSNSPv_Empty", "아직 올라온 반응이 없습니다."));
		Empty->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_Empty = Content->AddChildToVerticalBox(Empty))
		{
			Slot_Empty->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Empty->SetHorizontalAlignment(HAlign_Fill);
			Slot_Empty->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
		}

		// ★ 이 패널의 본론이다.
		StyleText(*MissingNote, 12, Palette->InGame.InkFaint, TEXT("Medium"));
		MissingNote->SetText(LOCTEXT("LiveSNSPv_MissingNote",
			"관객 반응을 만드는 시스템이 아직 없습니다. 화면 자리만 잡아 둔 상태이고, "
			"반응 데이터가 생기면 여기에 그대로 들어갑니다."));
		MissingNote->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_MissingNote = Content->AddChildToVerticalBox(MissingNote))
		{
			Slot_MissingNote->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_MissingNote->SetHorizontalAlignment(HAlign_Fill);
		}

		if (UBorderSlot* Slot_Content = Cast<UBorderSlot>(Root->AddChild(Content)))
		{
			Slot_Content->SetHorizontalAlignment(HAlign_Fill);
			Slot_Content->SetVerticalAlignment(VAlign_Top);
		}
		return true;
	}

	// =========================================================================
	// 3-7) 결산 패널 둘 — 마감·리포트에서만 열린다
	// =========================================================================

	/** 패널 공통 머리(제목 + 닫기). 결산 패널 둘이 같은 모양이어야 한다. */
	bool AddResultPanelHeader(
		UWidgetBlueprint& Blueprint, UWidgetTree& Tree, UVerticalBox& Content,
		const UFF_UIPalette& Palette, const FText& TitlePreview)
	{
		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Header"));
		UTextBlock* Title = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_PanelTitle"), true);
		UButton* Close = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Close"), true);
		UTextBlock* CloseGlyph = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CloseGlyph"));

		if (!Header || !Title || !Close || !CloseGlyph)
		{
			return false;
		}

		StyleText(*Title, 18, Palette.InGame.Ink, TEXT("SemiBold"));
		Title->SetText(TitlePreview);
		if (UHorizontalBoxSlot* Slot_Title = Header->AddChildToHorizontalBox(Title))
		{
			Slot_Title->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot_Title->SetVerticalAlignment(VAlign_Center);
		}

		FButtonStyle CloseStyle = Close->GetStyle();
		CloseStyle.Normal = MakeSolidBrush(Palette.InGame.PaperSunken, 4.f);
		CloseStyle.Hovered = MakeSolidBrush(Palette.InGame.Hover, 4.f);
		CloseStyle.Pressed = MakeSolidBrush(Palette.InGame.Line, 4.f);
		CloseStyle.NormalPadding = FMargin(8.f, 2.f);
		CloseStyle.PressedPadding = FMargin(8.f, 2.f);
		Close->SetStyle(CloseStyle);

		StyleText(*CloseGlyph, 14, Palette.InGame.InkMuted, TEXT("Medium"));
		CloseGlyph->SetText(LOCTEXT("LiveResult_Close", "×"));
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
		if (UVerticalBoxSlot* Slot_Header = Content.AddChildToVerticalBox(Header))
		{
			Slot_Header->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Header->SetHorizontalAlignment(HAlign_Fill);
			Slot_Header->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
		}
		return true;
	}

	/** 설명 한 줄. 값이 없으면 런타임에 접힌다. */
	UTextBlock* AddResultNote(
		UWidgetBlueprint& Blueprint, UWidgetTree& Tree, UVerticalBox& Content,
		const FName Name, const FLinearColor& Color, const int32 FontSize,
		const FText& Preview, const float BottomPadding)
	{
		UTextBlock* Note = MakeWidget<UTextBlock>(Blueprint, Tree, Name, true);
		if (!Note)
		{
			return nullptr;
		}

		StyleText(*Note, FontSize, Color, TEXT("Medium"));
		Note->SetText(Preview);
		Note->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot_Note = Content.AddChildToVerticalBox(Note))
		{
			Slot_Note->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Note->SetHorizontalAlignment(HAlign_Fill);
			Slot_Note->SetPadding(FMargin(0.f, 0.f, 0.f, BottomPadding));
		}
		return Note;
	}

	bool BuildReportPanel(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			return false;
		}

		UBorder* Root = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_PanelRoot"), true);
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Content"));
		UHorizontalBox* MoneyRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Money"));
		UHorizontalBox* ScoreRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Score"));
		UHorizontalBox* OpsRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Ops"));
		if (!Root || !Content || !MoneyRow || !ScoreRow || !OpsRow)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetBrush(MakeSolidBrush(Palette->InGame.Paper, 8.f));
		Root->SetPadding(FMargin(24.f, 20.f));

		if (!AddResultPanelHeader(Blueprint, Tree, *Content, *Palette,
			LOCTEXT("LiveReport_Title", "리포트")))
		{
			return false;
		}

		// --- 돈 ---
		const bool bMoneyOk =
			AddMetricCell(Blueprint, Tree, *MoneyRow, *Palette, TEXT("Text_TodayRevenue"),
				LOCTEXT("LiveReport_TodayRevenue", "오늘 매출"), LOCTEXT("LiveReportPv_Today", "3,200"))
			&& AddMetricCell(Blueprint, Tree, *MoneyRow, *Palette, TEXT("Text_TotalRevenue"),
				LOCTEXT("LiveReport_TotalRevenue", "전체 매출"), LOCTEXT("LiveReportPv_Total", "9,800"))
			&& AddMetricCell(Blueprint, Tree, *MoneyRow, *Palette, TEXT("Text_Gold"),
				LOCTEXT("LiveReport_Gold", "보유 금화"), LOCTEXT("LiveReportPv_Gold", "7,100"));
		if (!bMoneyOk)
		{
			return false;
		}
		if (UVerticalBoxSlot* Slot_Money = Content->AddChildToVerticalBox(MoneyRow))
		{
			Slot_Money->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Money->SetHorizontalAlignment(HAlign_Fill);
		}

		// 미납은 0 이면 런타임에 접힌다.
		AddResultNote(Blueprint, Tree, *Content, TEXT("Text_Arrears"),
			Palette->InGame.Danger, 13, LOCTEXT("LiveReportPv_Arrears", "미납 2,400"), 18.f);

		// --- 평가 ---
		const bool bScoreOk =
			AddMetricCell(Blueprint, Tree, *ScoreRow, *Palette, TEXT("Text_SimSatisfaction"),
				LOCTEXT("LiveReport_Sim", "관객 만족도"), LOCTEXT("LiveReportPv_Sim", "72"))
			&& AddMetricCell(Blueprint, Tree, *ScoreRow, *Palette, TEXT("Text_ScoreSatisfaction"),
				LOCTEXT("LiveReport_Score", "평가 점수"), LOCTEXT("LiveReportPv_Score", "80"))
			&& AddMetricCell(Blueprint, Tree, *ScoreRow, *Palette, TEXT("Text_Safety"),
				LOCTEXT("LiveReport_Safety", "안전"), LOCTEXT("LiveReportPv_Safety", "88"));
		if (!bScoreOk)
		{
			return false;
		}
		if (UVerticalBoxSlot* Slot_Score = Content->AddChildToVerticalBox(ScoreRow))
		{
			Slot_Score->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Score->SetHorizontalAlignment(HAlign_Fill);
		}

		// 두 만족도가 같으면 런타임에 접힌다.
		AddResultNote(Blueprint, Tree, *Content, TEXT("Text_SatisfactionNote"),
			Palette->InGame.InkMuted, 12,
			LOCTEXT("LiveReportPv_SatNote",
				"평가 점수는 관객 만족도에 피날레·연출 보정을 더한 값입니다 (8 차이)."), 18.f);

		// --- 운영 ---
		const bool bOpsOk =
			AddMetricCell(Blueprint, Tree, *OpsRow, *Palette, TEXT("Text_Staff"),
				LOCTEXT("LiveReport_Staff", "인력"), LOCTEXT("LiveReportPv_Staff", "12명 (알바 6)"))
			&& AddMetricCell(Blueprint, Tree, *OpsRow, *Palette, TEXT("Text_Programs"),
				LOCTEXT("LiveReport_Programs", "편성한 프로그램"), LOCTEXT("LiveReportPv_Programs", "2개"));
		if (!bOpsOk)
		{
			return false;
		}
		if (UVerticalBoxSlot* Slot_Ops = Content->AddChildToVerticalBox(OpsRow))
		{
			Slot_Ops->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Ops->SetHorizontalAlignment(HAlign_Fill);
			Slot_Ops->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
		}

		AddResultNote(Blueprint, Tree, *Content, TEXT("Text_MissingNote"),
			Palette->InGame.InkFaint, 12,
			LOCTEXT("LiveReportPv_Missing",
				"어느 시설이 만족도를 깎았는지는 아직 볼 수 없습니다. 원인 집계가 붙으면 여기에 들어갑니다."), 18.f);

		// --- 다음 날로. 결과를 읽는 자리 **바로 아래**에 둔다 ---
		AddResultNote(Blueprint, Tree, *Content, TEXT("Text_AdvanceReason"),
			Palette->InGame.InkMuted, 12,
			LOCTEXT("LiveReportPv_AdvanceReason", "누르면 다음 날로 넘어갑니다"), 6.f);

		UButton* Advance = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Advance"), true);
		UTextBlock* AdvanceLabel = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_AdvanceLabel"), true);
		if (!Advance || !AdvanceLabel)
		{
			return false;
		}

		FButtonStyle AdvanceStyle = Advance->GetStyle();
		// 🔴 이 버튼만 **라벨이 흰 글씨**다 (아래 `StyleText` 참고). 그래서 네 상태가 전부
		//    어두워야 한다. 원래는 닫기 버튼 묶음을 그대로 베껴 왔는데 그쪽은 라벨이
		//    `Ink`(어두움)라 밝은 판이 맞았다 — 여기서는 정반대가 된다.
		//
		//    `Hover`(거의 흰색)와 `Line`(밝은 회색)을 쓰던 탓에 **마우스를 올리거나 누르는
		//    순간 글씨가 사라졌다** (2026-09-03 백종태 발견). 꺼짐 상태도 같은 이유로
		//    한 번 고쳤는데, 그때 나머지 둘을 같이 안 본 것이 이 버그다.
		AdvanceStyle.Normal = MakeSolidBrush(Palette->InGame.Teal, 4.f);
		AdvanceStyle.Hovered = MakeSolidBrush(Palette->InGame.TealDeep, 4.f);
		AdvanceStyle.Pressed = MakeSolidBrush(Palette->InGame.Ink, 4.f);
		// 🔴 밝은 판을 쓰면 안 된다. 라벨이 흰 글씨라 꺼진 순간 **흰 바탕에 흰 글씨**가 되어
		//    버튼이 통째로 사라진 것처럼 보였다 (2026-09-03 백종태 발견 — `다음 날로` 가
		//    없다고 한 것이 사실은 안 보인 것이었다). 회색 판이면 흰 글씨가 읽힌다.
		AdvanceStyle.Disabled = MakeSolidBrush(Palette->InGame.InkFaint, 4.f);
		AdvanceStyle.NormalPadding = FMargin(18.f, 6.f);
		AdvanceStyle.PressedPadding = FMargin(18.f, 6.f);
		Advance->SetStyle(AdvanceStyle);

		StyleText(*AdvanceLabel, 14, Palette->InGame.Paper, TEXT("SemiBold"));
		AdvanceLabel->SetText(LOCTEXT("LiveReportPv_Advance", "다음 날로"));
		if (UButtonSlot* Slot_AdvanceLabel = Cast<UButtonSlot>(Advance->AddChild(AdvanceLabel)))
		{
			Slot_AdvanceLabel->SetHorizontalAlignment(HAlign_Center);
			Slot_AdvanceLabel->SetVerticalAlignment(VAlign_Center);
			Slot_AdvanceLabel->SetPadding(FMargin(0.f));
		}
		if (UVerticalBoxSlot* Slot_Advance = Content->AddChildToVerticalBox(Advance))
		{
			Slot_Advance->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Advance->SetHorizontalAlignment(HAlign_Left);
		}

		if (UBorderSlot* Slot_Content = Cast<UBorderSlot>(Root->AddChild(Content)))
		{
			Slot_Content->SetHorizontalAlignment(HAlign_Fill);
			Slot_Content->SetVerticalAlignment(VAlign_Top);
		}
		return true;
	}

	bool BuildFinalePanel(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			return false;
		}

		UBorder* Root = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_PanelRoot"), true);
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Content"));
		UHorizontalBox* Row = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Finale"));
		if (!Root || !Content || !Row)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetBrush(MakeSolidBrush(Palette->InGame.Paper, 8.f));
		Root->SetPadding(FMargin(24.f, 20.f));

		if (!AddResultPanelHeader(Blueprint, Tree, *Content, *Palette,
			LOCTEXT("LiveFinale_Title", "피날레")))
		{
			return false;
		}

		const bool bRowOk =
			AddMetricCell(Blueprint, Tree, *Row, *Palette, TEXT("Text_Grade"),
				LOCTEXT("LiveFinale_Grade", "등급"), LOCTEXT("LiveFinalePv_Grade", "대형"))
			&& AddMetricCell(Blueprint, Tree, *Row, *Palette, TEXT("Text_Bonus"),
				LOCTEXT("LiveFinale_Bonus", "보너스"), LOCTEXT("LiveFinalePv_Bonus", "만족도 +8"));
		if (!bRowOk)
		{
			return false;
		}
		if (UVerticalBoxSlot* Slot_Row = Content->AddChildToVerticalBox(Row))
		{
			Slot_Row->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Row->SetHorizontalAlignment(HAlign_Fill);
			Slot_Row->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		}

		// !! 숫자만 두지 않는다 !! 이미 터졌는지 앞으로 터질지는 숫자로 알 수 없다 (AT-011).
		AddResultNote(Blueprint, Tree, *Content, TEXT("Text_PhaseNote"),
			Palette->InGame.Ink, 13,
			LOCTEXT("LiveFinalePv_Phase",
				"3일차 라이브가 끝나면 실행됩니다. 보너스는 그때 평가 점수에 들어갑니다."), 12.f);

		AddResultNote(Blueprint, Tree, *Content, TEXT("Text_MissingNote"),
			Palette->InGame.InkFaint, 12,
			LOCTEXT("LiveFinalePv_Missing",
				"피날레는 셋업에서만 고를 수 있고 축제당 한 번입니다. 여기서는 결과만 봅니다."), 0.f);

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
	/** 상태 행 아이콘 폴더. 이름 규칙으로 찾으므로 표를 따로 두지 않는다. */
	const FString MetricIconFolder =
		TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Metrics");

	/**
	 * 🔴 숫자만 두면 무엇인지 모른다 (2026-09-02 백종태 지적).
	 *
	 * 실기 화면이 `다대포 바다축제 6,785 35 7명 중 7명 사용 가능 0` 이었는데
	 * `35`(오늘 매출)와 `0`(관객)이 무엇인지 알 방법이 없었다. 라벨을 붙이면 바가
	 * 길어지므로 아이콘을 앞에 둔다.
	 *
	 * ⚠️ 아이콘은 **보조**다. 아이콘만으로 뜻을 전하지 않는다 — 숫자에 단위(`명`)가
	 * 붙어 있고, 못 읽은 칸은 아예 접힌다 (AT-011 과 같은 이유).
	 */
	UTexture2D* LoadMetricIcon(const TCHAR* IconName)
	{
		const FString Path = FString::Printf(
			TEXT("%s/T_FF_LH_Icon_%s_White_256.T_FF_LH_Icon_%s_White_256"),
			*MetricIconFolder, IconName, IconName);

		UTexture2D* Icon = LoadObject<UTexture2D>(nullptr, *Path);
		if (!Icon)
		{
			// 아이콘이 없어도 화면은 뜬다. 조용히 넘기지 않는 이유는 이름 규칙을
			// 어긴 파일을 여기서만 잡을 수 있어서다.
			UE_LOG(LogFFUIWbpBuilder, Warning,
				TEXT("%s_WARN: 상태 행 아이콘을 못 찾았다 (%s). 그 칸은 숫자만 뜬다."),
				LiveLogPrefix, *Path);
		}
		return Icon;
	}

	bool AddStatusCell(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		UHorizontalBox& Row,
		const FName BoxName,
		const FName TextName,
		const FText& PreviewText,
		const FLinearColor& TextColor,
		const TCHAR* IconName = nullptr)
	{
		UHorizontalBox* Box = MakeWidget<UHorizontalBox>(Blueprint, Tree, BoxName, true);
		UTextBlock* Text = MakeWidget<UTextBlock>(Blueprint, Tree, TextName, true);
		if (!Box || !Text)
		{
			return false;
		}

		const UFF_UIPalette* Palette = UFF_UIPalette::Get();

		// 아이콘을 **텍스트 앞**에 둔다. 칸이 접힐 때 아이콘도 같이 접혀야 하므로
		// 같은 `Box_*` 안에 넣는다 — 밖에 두면 값이 없는 칸의 아이콘만 남는다.
		if (IconName)
		{
			if (UTexture2D* IconTexture = LoadMetricIcon(IconName))
			{
				UImage* Icon = MakeWidget<UImage>(
					Blueprint, Tree, FName(*(BoxName.ToString() + TEXT("_Icon"))));
				if (Icon)
				{
					FSlateBrush IconBrush;
					IconBrush.SetResourceObject(IconTexture);
					IconBrush.ImageSize = FVector2D(14.f, 14.f);
					IconBrush.TintColor = FSlateColor(Palette
						? Palette->InGame.InkMuted : FLinearColor::White);
					Icon->SetBrush(IconBrush);

					if (UHorizontalBoxSlot* Slot_Icon = Box->AddChildToHorizontalBox(Icon))
					{
						Slot_Icon->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
						Slot_Icon->SetVerticalAlignment(VAlign_Center);
						Slot_Icon->SetPadding(FMargin(0.f, 0.f, 5.f, 0.f));
					}
				}
			}
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
		//    상태 행 위 가운데이고 거기서 위로 자란다 — 아래 끝이 탭 줄 밑으로 들어가서
		//    버튼이 패널에 얹힌 것처럼 보인다 (준비 단계와 같은 자리).
		if (UCanvasPanelSlot* Slot_PanelHost = Cast<UCanvasPanelSlot>(Root->AddChild(PanelHost)))
		{
			Slot_PanelHost->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));
			Slot_PanelHost->SetAlignment(FVector2D(0.5f, 1.f));
			Slot_PanelHost->SetOffsets(FMargin(
				0.f, -StatusRowHeight, PanelHostWidth, PanelHostHeight));
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
			TEXT("Tab_Queues"), TEXT("Tab_SNS"), TEXT("Tab_Staff"),
			// 결산 탭. 마감·리포트에서만 보인다 (2026-09-02 통합).
			TEXT("Tab_Report"), TEXT("Tab_Finale") };

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
				LOCTEXT("LiveStatusPreview_Gold", "7,600"), Palette->InGame.Ink, TEXT("Gold"))
			&& AddStatusCell(Blueprint, Tree, *StatusLeft,
				TEXT("Box_Arrears"), TEXT("Text_Arrears"),
				LOCTEXT("LiveStatusPreview_Arrears", "미납 2,400"), Palette->InGame.Danger)
			&& AddStatusCell(Blueprint, Tree, *StatusLeft,
				TEXT("Box_TodayRevenue"), TEXT("Text_TodayRevenue"),
				LOCTEXT("LiveStatusPreview_Revenue", "3,200"), Palette->InGame.Ink, TEXT("Revenue"))
			&& AddStatusCell(Blueprint, Tree, *StatusLeft,
				TEXT("Box_Staff"), TEXT("Text_Staff"),
				LOCTEXT("LiveStatusPreview_Staff", "6명 중 4명 사용 가능"), Palette->InGame.Ink,
				TEXT("StaffCount"))
			&& AddStatusCell(Blueprint, Tree, *StatusLeft,
				TEXT("Box_VisitorCount"), TEXT("Text_VisitorCount"),
				LOCTEXT("LiveStatusPreview_Visitors", "1,240"), Palette->InGame.Ink,
				TEXT("Visitors"));

		// --- 오른쪽 묶음. 시계 → 배속 ---
		// ★ 시계를 배속 **왼쪽**에 둔다 (2026-08-27 요청). 관객 만족도·안전은 운영 패널로 갔다.
		const bool bRightCellsOk =
			AddStatusCell(Blueprint, Tree, *StatusRight,
				TEXT("Box_Clock"), TEXT("Text_Clock"),
				LOCTEXT("LiveStatusPreview_Clock", "19:42 · 남은 128초"), Palette->InGame.InkMuted,
				TEXT("Clock"));

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

		EnsureWidgetBlueprint(
			LiveLogPrefix,
			StaffPanelPackage,
			LivePanelsFolder,
			*UFF_LivePanelStaff::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildStaffPanel(Blueprint, Tree);
			});

		EnsureWidgetBlueprint(
			LiveLogPrefix,
			FacilitiesPanelPackage,
			LivePanelsFolder,
			*UFF_LivePanelFacilities::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildFacilitiesPanel(Blueprint, Tree);
			});

		EnsureWidgetBlueprint(
			LiveLogPrefix,
			QueuesPanelPackage,
			LivePanelsFolder,
			*UFF_LivePanelQueues::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildQueuesPanel(Blueprint, Tree);
			});

		EnsureWidgetBlueprint(
			LiveLogPrefix,
			GuestsPanelPackage,
			LivePanelsFolder,
			*UFF_LivePanelGuests::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildGuestsPanel(Blueprint, Tree);
			});

		EnsureWidgetBlueprint(
			LiveLogPrefix,
			SNSPanelPackage,
			LivePanelsFolder,
			*UFF_LivePanelSNS::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildSNSPanel(Blueprint, Tree);
			});

		EnsureWidgetBlueprint(
			LiveLogPrefix,
			ReportPanelPackage,
			LivePanelsFolder,
			*UFF_LivePanelReport::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildReportPanel(Blueprint, Tree);
			});

		EnsureWidgetBlueprint(
			LiveLogPrefix,
			FinalePanelPackage,
			LivePanelsFolder,
			*UFF_LivePanelFinale::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildFinalePanel(Blueprint, Tree);
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
