// Fill out your copyright notice in the Description page of Project Settings.

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
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/ClosingHUD/FF_ClosingHudScreen.h"
#include "UI/ClosingHUD/FF_ClosingPanelBase.h"
#include "UI/ClosingHUD/Panels/FF_ClosingPanelFinale.h"
#include "UI/ClosingHUD/Panels/FF_ClosingPanelReport.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "FestaFiestaClosingHudBuilder"

/**
 * `FF.ClosingHudUMG` — 마감·리포트 HUD 의 WBP 골격을 만든다.
 *
 * 라이브 HUD 생성기와 같은 규칙이다 —
 *   · 계층·이름·`BindWidget` 대상은 **생성기가 소유**한다
 *   · 좌표와 마감은 **디자이너가 소유**한다
 *   · 이미 있는 패키지는 **건드리지 않는다** (`EnsureWidgetBlueprint`)
 *
 * 그래서 이름이나 앵커를 바꾸면 해당 WBP 를 지우고 다시 돌려야 한다.
 */
namespace FFClosingHudBuilder
{
	using namespace FFUIWbpBuilder;

	const TCHAR* ClosingLogPrefix = TEXT("FF_CLOSING_UMG");

	const FString ClosingFolder = TEXT("/Game/FestaFiesta/UI/ClosingHUD");
	const FString ClosingPanelsFolder = ClosingFolder / TEXT("Panels");

	const FString ScreenPackage = ClosingFolder / TEXT("WBP_FF_ClosingHudScreen");
	const FString PlaceholderPackage = ClosingPanelsFolder / TEXT("WBP_FF_ClosingPlaceholderPanel");
	const FString ReportPanelPackage = ClosingPanelsFolder / TEXT("WBP_FF_ClosingPanelReport");
	const FString FinalePanelPackage = ClosingPanelsFolder / TEXT("WBP_FF_ClosingPanelFinale");

	/** 라이브 바의 탭 버튼을 그대로 쓴다. 생김새가 갈라지면 안 된다. */
	const FString TabButtonPackage = TEXT("/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveTabButton");

	constexpr float MenuRowHeight = 29.f;
	constexpr float StatusRowHeight = 29.f;
	constexpr float BarHeight = MenuRowHeight + StatusRowHeight;

	constexpr float PanelHostWidth = 1080.f;
	constexpr float PanelHostHeight = 620.f;

	/**
	 * 🔴 텍스처 없이 단색을 칠하는 브러시.
	 *
	 * `FSlateBrush` 기본값은 `DrawAs = Image` + `ImageType = NoImage` 다. 색만 넣으면
	 * `UBorder` 는 아무것도 안 그리고 `UImage` 는 흰 사각형을 그린다.
	 */
	FSlateBrush MakeSolidBrush(const FLinearColor& Color, const float CornerRadius = 0.f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Color);
		Brush.OutlineSettings.CornerRadii = FVector4(CornerRadius, CornerRadius, CornerRadius, CornerRadius);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return Brush;
	}

	/** 패널 공통 머리(제목 + 닫기). 세 패널이 같은 모양이어야 한다. */
	bool AddPanelHeader(
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

		// 툴팁을 달지 않는다. `×` 를 모르는 사람은 없어서 안내가 아니라 가림이다.
		StyleText(*CloseGlyph, 14, Palette.InGame.InkMuted, TEXT("Medium"));
		CloseGlyph->SetText(LOCTEXT("Closing_Close", "×"));
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

	/** 값 한 칸 — 라벨 위, 숫자 아래. */
	bool AddCell(
		UWidgetBlueprint& Blueprint, UWidgetTree& Tree, UHorizontalBox& Row,
		const UFF_UIPalette& Palette, const FName ValueName,
		const FText& Label, const FText& PreviewValue)
	{
		UVerticalBox* Cell = MakeWidget<UVerticalBox>(
			Blueprint, Tree, FName(*(ValueName.ToString() + TEXT("_Cell"))));
		UTextBlock* LabelText = MakeWidget<UTextBlock>(
			Blueprint, Tree, FName(*(ValueName.ToString() + TEXT("_Label"))));
		UTextBlock* ValueText = MakeWidget<UTextBlock>(Blueprint, Tree, ValueName, true);

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
		}

		if (UHorizontalBoxSlot* Slot_Cell = Row.AddChildToHorizontalBox(Cell))
		{
			Slot_Cell->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot_Cell->SetVerticalAlignment(VAlign_Top);
		}
		return true;
	}

	/** 설명 한 줄. 값이 없으면 런타임에 접힌다. */
	UTextBlock* AddNote(
		UWidgetBlueprint& Blueprint, UWidgetTree& Tree, UVerticalBox& Content,
		const UFF_UIPalette& Palette, const FName Name,
		const FLinearColor& Color, const int32 FontSize,
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

	/** 패널 껍데기 — 배경 + 내용 상자. 세 패널이 공유한다. */
	bool BeginPanel(
		UWidgetBlueprint& Blueprint, UWidgetTree& Tree,
		const UFF_UIPalette& Palette, const FText& TitlePreview,
		UBorder*& OutRoot, UVerticalBox*& OutContent)
	{
		OutRoot = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_PanelRoot"), true);
		OutContent = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Content"));
		if (!OutRoot || !OutContent)
		{
			return false;
		}

		Tree.RootWidget = OutRoot;
		OutRoot->SetBrush(MakeSolidBrush(Palette.InGame.Paper, 8.f));
		OutRoot->SetPadding(FMargin(24.f, 20.f));

		return AddPanelHeader(Blueprint, Tree, *OutContent, Palette, TitlePreview);
	}

	void EndPanel(UBorder& Root, UVerticalBox& Content)
	{
		if (UBorderSlot* Slot_Content = Cast<UBorderSlot>(Root.AddChild(&Content)))
		{
			Slot_Content->SetHorizontalAlignment(HAlign_Fill);
			Slot_Content->SetVerticalAlignment(VAlign_Top);
		}
	}

	// =========================================================================
	// 1) 자리표시 패널
	// =========================================================================

	bool BuildPlaceholderPanel(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			return false;
		}

		UBorder* Root = nullptr;
		UVerticalBox* Content = nullptr;
		if (!BeginPanel(Blueprint, Tree, *Palette, LOCTEXT("ClosingPh_Title", "패널"), Root, Content))
		{
			return false;
		}

		AddNote(Blueprint, Tree, *Content, *Palette, TEXT("Text_Placeholder"),
			Palette->InGame.InkMuted, 13,
			LOCTEXT("ClosingPh_Message", "이 패널은 아직 만드는 중입니다."), 0.f);

		EndPanel(*Root, *Content);
		return true;
	}

	// =========================================================================
	// 2) 리포트 패널
	// =========================================================================

	bool BuildReportPanel(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			return false;
		}

		UBorder* Root = nullptr;
		UVerticalBox* Content = nullptr;
		if (!BeginPanel(Blueprint, Tree, *Palette, LOCTEXT("ClosingReport_Title", "리포트"), Root, Content))
		{
			return false;
		}

		UHorizontalBox* MoneyRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Money"));
		UHorizontalBox* ScoreRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Score"));
		UHorizontalBox* OpsRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Ops"));
		if (!MoneyRow || !ScoreRow || !OpsRow)
		{
			return false;
		}

		// --- 돈 ---
		const bool bMoneyOk =
			AddCell(Blueprint, Tree, *MoneyRow, *Palette, TEXT("Text_TodayRevenue"),
				LOCTEXT("ClosingReport_TodayRevenue", "오늘 매출"), LOCTEXT("ClosingReportPv_Today", "3,200"))
			&& AddCell(Blueprint, Tree, *MoneyRow, *Palette, TEXT("Text_TotalRevenue"),
				LOCTEXT("ClosingReport_TotalRevenue", "전체 매출"), LOCTEXT("ClosingReportPv_Total", "9,800"))
			&& AddCell(Blueprint, Tree, *MoneyRow, *Palette, TEXT("Text_Gold"),
				LOCTEXT("ClosingReport_Gold", "보유 금화"), LOCTEXT("ClosingReportPv_Gold", "7,100"));

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
		AddNote(Blueprint, Tree, *Content, *Palette, TEXT("Text_Arrears"),
			Palette->InGame.Danger, 13, LOCTEXT("ClosingReportPv_Arrears", "미납 2,400"), 18.f);

		// --- 평가 ---
		const bool bScoreOk =
			AddCell(Blueprint, Tree, *ScoreRow, *Palette, TEXT("Text_SimSatisfaction"),
				LOCTEXT("ClosingReport_Sim", "관객 만족도"), LOCTEXT("ClosingReportPv_Sim", "72"))
			&& AddCell(Blueprint, Tree, *ScoreRow, *Palette, TEXT("Text_ScoreSatisfaction"),
				LOCTEXT("ClosingReport_Score", "평가 점수"), LOCTEXT("ClosingReportPv_Score", "80"))
			&& AddCell(Blueprint, Tree, *ScoreRow, *Palette, TEXT("Text_Safety"),
				LOCTEXT("ClosingReport_Safety", "안전"), LOCTEXT("ClosingReportPv_Safety", "88"));

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
		AddNote(Blueprint, Tree, *Content, *Palette, TEXT("Text_SatisfactionNote"),
			Palette->InGame.InkMuted, 12,
			LOCTEXT("ClosingReportPv_SatNote",
				"평가 점수는 관객 만족도에 피날레·연출 보정을 더한 값입니다 (8 차이)."), 18.f);

		// --- 운영 ---
		const bool bOpsOk =
			AddCell(Blueprint, Tree, *OpsRow, *Palette, TEXT("Text_Staff"),
				LOCTEXT("ClosingReport_Staff", "인력"), LOCTEXT("ClosingReportPv_Staff", "12명 (알바 6)"))
			&& AddCell(Blueprint, Tree, *OpsRow, *Palette, TEXT("Text_Programs"),
				LOCTEXT("ClosingReport_Programs", "편성한 프로그램"), LOCTEXT("ClosingReportPv_Programs", "2개"));

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

		// ★ 아직 못 하는 것을 적는다.
		AddNote(Blueprint, Tree, *Content, *Palette, TEXT("Text_MissingNote"),
			Palette->InGame.InkFaint, 12,
			LOCTEXT("ClosingReportPv_Missing",
				"어느 시설이 만족도를 깎았는지는 아직 볼 수 없습니다. 원인 집계가 붙으면 여기에 들어갑니다."), 0.f);

		EndPanel(*Root, *Content);
		return true;
	}

	// =========================================================================
	// 3) 피날레 패널
	// =========================================================================

	bool BuildFinalePanel(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			return false;
		}

		UBorder* Root = nullptr;
		UVerticalBox* Content = nullptr;
		if (!BeginPanel(Blueprint, Tree, *Palette, LOCTEXT("ClosingFinale_Title", "피날레"), Root, Content))
		{
			return false;
		}

		UHorizontalBox* Row = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Finale"));
		if (!Row)
		{
			return false;
		}

		const bool bRowOk =
			AddCell(Blueprint, Tree, *Row, *Palette, TEXT("Text_Grade"),
				LOCTEXT("ClosingFinale_Grade", "등급"), LOCTEXT("ClosingFinalePv_Grade", "대형"))
			&& AddCell(Blueprint, Tree, *Row, *Palette, TEXT("Text_Bonus"),
				LOCTEXT("ClosingFinale_Bonus", "보너스"), LOCTEXT("ClosingFinalePv_Bonus", "만족도 +8"));

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
		AddNote(Blueprint, Tree, *Content, *Palette, TEXT("Text_PhaseNote"),
			Palette->InGame.Ink, 13,
			LOCTEXT("ClosingFinalePv_Phase",
				"3일차 라이브가 끝나면 실행됩니다. 보너스는 그때 평가 점수에 들어갑니다."), 12.f);

		AddNote(Blueprint, Tree, *Content, *Palette, TEXT("Text_MissingNote"),
			Palette->InGame.InkFaint, 12,
			LOCTEXT("ClosingFinalePv_Missing",
				"피날레는 셋업에서만 고를 수 있고 축제당 한 번입니다. 여기서는 결과만 봅니다."), 0.f);

		EndPanel(*Root, *Content);
		return true;
	}

	// =========================================================================
	// 4) 결산 HUD 루트
	// =========================================================================

	bool BuildScreen(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, UClass& TabButtonClass)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			return false;
		}

		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("Canvas_Root"));
		UOverlay* PanelHost = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Panel_PanelHost"), true);
		USizeBox* BarBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Bar"));
		UVerticalBox* BarRows = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Bar"));
		UBorder* MenuRow = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_MenuRow"), true);
		UHorizontalBox* TabRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Tabs"));
		UBorder* StatusRow = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_StatusRow"), true);
		UHorizontalBox* StatusBox = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Status"));

		UTextBlock* DayStage = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_DayStage"), true);
		UTextBlock* Gold = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Gold"), true);
		UTextBlock* Remaining = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Remaining"), true);

		// 🔴 CTA 를 오른쪽에 **고정**하는 칸. 사유 문구가 접혀도 버튼이 왼쪽으로 딸려
		//    오지 않게 한다 — 리포트 단계에서는 사유가 비어서 실제로 그랬다 (2026-09-02).
		USpacer* Spacer = MakeWidget<USpacer>(Blueprint, Tree, TEXT("Spacer_CtaPush"));
		UTextBlock* CtaReason = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CtaReason"), true);
		UButton* Cta = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Cta"), true);
		UTextBlock* CtaLabel = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CtaLabel"), true);

		if (!Root || !PanelHost || !BarBox || !BarRows || !MenuRow || !TabRow || !StatusRow
			|| !StatusBox || !DayStage || !Gold || !Remaining || !Spacer || !CtaReason || !Cta || !CtaLabel)
		{
			return false;
		}

		Tree.RootWidget = Root;

		// 🔴 `Visible` 이면 화면 전체가 월드 클릭을 먹는다.
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		// --- 패널 자리 ---
		// 🔴 **두 축 다 점 앵커다.** 늘어난 축을 쓰면 크기를 부모가 정하는데 HUD 루트가
		//    화면 높이를 못 받아 음수 높이가 된다. 기준점은 상태 행 위 가운데다.
		PanelHost->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* Slot_PanelHost = Cast<UCanvasPanelSlot>(Root->AddChild(PanelHost)))
		{
			Slot_PanelHost->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));
			Slot_PanelHost->SetAlignment(FVector2D(0.5f, 1.f));
			Slot_PanelHost->SetOffsets(FMargin(0.f, -StatusRowHeight, PanelHostWidth, PanelHostHeight));
		}

		// --- 하단 바. 패널보다 **나중에** 추가해 탭이 위에 그려지게 한다 ---
		BarBox->SetHeightOverride(BarHeight);
		if (UCanvasPanelSlot* Slot_Bar = Cast<UCanvasPanelSlot>(Root->AddChild(BarBox)))
		{
			Slot_Bar->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
			Slot_Bar->SetAlignment(FVector2D(0.f, 1.f));
			Slot_Bar->SetOffsets(FMargin(0.f, 0.f, 0.f, BarHeight));
		}
		BarBox->AddChild(BarRows);

		// --- 메뉴 행: 탭 둘 ---
		MenuRow->SetBrush(MakeSolidBrush(Palette->InGame.Paper));
		MenuRow->SetPadding(FMargin(4.f, 0.f));
		if (UVerticalBoxSlot* Slot_MenuRow = BarRows->AddChildToVerticalBox(MenuRow))
		{
			Slot_MenuRow->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot_MenuRow->SetHorizontalAlignment(HAlign_Center);
			Slot_MenuRow->SetVerticalAlignment(VAlign_Fill);
		}
		MenuRow->AddChild(TabRow);

		// 🔴 탭을 런타임에 만들지 않는다. 디자이너에서 안 보여 자리를 손으로 못 만진다.
		const TCHAR* TabNames[] = { TEXT("Tab_Report"), TEXT("Tab_Finale") };
		for (const TCHAR* TabName : TabNames)
		{
			UUserWidget* TabButton = MakeWidgetOfClass(Blueprint, Tree, FName(TabName), &TabButtonClass, true);
			if (!TabButton)
			{
				return false;
			}
			if (UHorizontalBoxSlot* Slot_Tab = TabRow->AddChildToHorizontalBox(TabButton))
			{
				Slot_Tab->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				Slot_Tab->SetVerticalAlignment(VAlign_Fill);
			}
		}

		// --- 상태 행: 일차·단계 / 금화 / 막힌 이유 / CTA ---
		StatusRow->SetBrush(MakeSolidBrush(Palette->InGame.PaperSunken));
		StatusRow->SetPadding(FMargin(12.f, 0.f));
		if (UVerticalBoxSlot* Slot_StatusRow = BarRows->AddChildToVerticalBox(StatusRow))
		{
			Slot_StatusRow->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot_StatusRow->SetHorizontalAlignment(HAlign_Fill);
			Slot_StatusRow->SetVerticalAlignment(VAlign_Fill);
		}
		if (UBorderSlot* Slot_StatusBox = Cast<UBorderSlot>(StatusRow->AddChild(StatusBox)))
		{
			Slot_StatusBox->SetHorizontalAlignment(HAlign_Fill);
			Slot_StatusBox->SetVerticalAlignment(VAlign_Fill);
		}

		StyleText(*DayStage, 13, Palette->InGame.Ink, TEXT("SemiBold"));
		DayStage->SetText(LOCTEXT("ClosingPv_DayStage", "1일차 · 마감"));
		if (UHorizontalBoxSlot* Slot_DayStage = StatusBox->AddChildToHorizontalBox(DayStage))
		{
			Slot_DayStage->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_DayStage->SetVerticalAlignment(VAlign_Center);
			Slot_DayStage->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
		}

		StyleText(*Gold, 13, Palette->InGame.Ink, TEXT("Medium"));
		Gold->SetText(LOCTEXT("ClosingPv_Gold", "7,100"));
		if (UHorizontalBoxSlot* Slot_Gold = StatusBox->AddChildToHorizontalBox(Gold))
		{
			Slot_Gold->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Gold->SetVerticalAlignment(VAlign_Center);
		}

		// 남은 시간. 마감·리포트도 시간이 흘러 자동으로 넘어간다.
		StyleText(*Remaining, 13, Palette->InGame.InkMuted, TEXT("Medium"));
		Remaining->SetText(LOCTEXT("ClosingPv_Remaining", "남은 42초"));
		if (UHorizontalBoxSlot* Slot_Remaining = StatusBox->AddChildToHorizontalBox(Remaining))
		{
			Slot_Remaining->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Remaining->SetVerticalAlignment(VAlign_Center);
			Slot_Remaining->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
		}

		// 🔴 **빈 칸이 남는 공간을 먹는다.** 이 자리를 사유 문구가 맡으면, 사유가 없는
		//    리포트 단계에서 문구가 접히면서 CTA 가 왼쪽으로 딸려 온다.
		if (UHorizontalBoxSlot* Slot_Spacer = StatusBox->AddChildToHorizontalBox(Spacer))
		{
			Slot_Spacer->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		// !! 흐려짐만으로는 왜 못 누르는지 알 수 없다 !! (AT-011) 사유를 CTA 왼쪽에 둔다.
		StyleText(*CtaReason, 12, Palette->InGame.InkMuted, TEXT("Medium"));
		CtaReason->SetText(LOCTEXT("ClosingPv_CtaReason",
			"결과를 먼저 확인하세요. 남은 마감 시간이 지나면 리포트로 넘어갑니다."));
		if (UHorizontalBoxSlot* Slot_CtaReason = StatusBox->AddChildToHorizontalBox(CtaReason))
		{
			Slot_CtaReason->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_CtaReason->SetVerticalAlignment(VAlign_Center);
			Slot_CtaReason->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		}

		FButtonStyle CtaStyle = Cta->GetStyle();
		CtaStyle.Normal = MakeSolidBrush(Palette->InGame.Teal, 4.f);
		CtaStyle.Hovered = MakeSolidBrush(Palette->InGame.Hover, 4.f);
		CtaStyle.Pressed = MakeSolidBrush(Palette->InGame.Line, 4.f);
		CtaStyle.Disabled = MakeSolidBrush(Palette->InGame.PaperSunken, 4.f);
		CtaStyle.NormalPadding = FMargin(14.f, 3.f);
		CtaStyle.PressedPadding = FMargin(14.f, 3.f);
		Cta->SetStyle(CtaStyle);

		StyleText(*CtaLabel, 13, Palette->InGame.Paper, TEXT("SemiBold"));
		CtaLabel->SetText(LOCTEXT("ClosingPv_CtaLabel", "다음 날로"));
		if (UButtonSlot* Slot_CtaLabel = Cast<UButtonSlot>(Cta->AddChild(CtaLabel)))
		{
			Slot_CtaLabel->SetHorizontalAlignment(HAlign_Center);
			Slot_CtaLabel->SetVerticalAlignment(VAlign_Center);
			Slot_CtaLabel->SetPadding(FMargin(0.f));
		}
		if (UHorizontalBoxSlot* Slot_Cta = StatusBox->AddChildToHorizontalBox(Cta))
		{
			Slot_Cta->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot_Cta->SetVerticalAlignment(VAlign_Center);
		}

		return true;
	}

	// =========================================================================
	// 실행
	// =========================================================================

	void RunClosingHudUmg()
	{
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_BEGIN: 없는 WBP만 만든다."), ClosingLogPrefix);

		EnsureWidgetBlueprint(
			ClosingLogPrefix, PlaceholderPackage, ClosingPanelsFolder,
			*UFF_ClosingPlaceholderPanel::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree) { return BuildPlaceholderPanel(Blueprint, Tree); });

		EnsureWidgetBlueprint(
			ClosingLogPrefix, ReportPanelPackage, ClosingPanelsFolder,
			*UFF_ClosingPanelReport::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree) { return BuildReportPanel(Blueprint, Tree); });

		EnsureWidgetBlueprint(
			ClosingLogPrefix, FinalePanelPackage, ClosingPanelsFolder,
			*UFF_ClosingPanelFinale::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree) { return BuildFinalePanel(Blueprint, Tree); });

		// 🔴 탭 버튼은 라이브 HUD 것을 쓴다. 없으면 화면을 만들지 않는다 — 잘못된 기본
		//    타입을 대신 넣으면 `BindWidget` 이 통과해 버리고 런타임에 탭이 무반응이 된다.
		UClass* TabButtonClass = LoadGeneratedClass(TabButtonPackage);
		if (!TabButtonClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 탭 버튼 클래스를 얻지 못했다 (%s). `FF.LiveHudUMG` 를 먼저 실행한다."),
				ClosingLogPrefix, *TabButtonPackage);
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_END"), ClosingLogPrefix);
			return;
		}

		EnsureWidgetBlueprint(
			ClosingLogPrefix, ScreenPackage, ClosingFolder,
			*UFF_ClosingHudScreen::StaticClass(),
			[TabButtonClass](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildScreen(Blueprint, Tree, *TabButtonClass);
			});

		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_END"), ClosingLogPrefix);
	}

	FAutoConsoleCommand ClosingHudUmgCommand(
		TEXT("FF.ClosingHudUMG"),
		TEXT("마감·리포트 HUD 의 WBP 4종을 만든다. 이미 있는 에셋은 건드리지 않는다. "
			 "`FF.LiveHudUMG` 를 먼저 실행해 탭 버튼이 있어야 한다."),
		FConsoleCommandDelegate::CreateStatic(&RunClosingHudUmg));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
