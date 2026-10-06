// P5 최종 결산 최초 제작 도구. 기존 화면·HUD·게임 상태는 변경하지 않는다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/Report/FF_FinalReportDayRowWidget.h"
#include "UI/Report/FF_ReportScreen.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FestaFiestaFinalReportSky"

namespace FFFinalReportSkyWbpBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* LogPrefix = TEXT("FF_FINAL_REPORT_SKY");
	constexpr const TCHAR* VersionKey = TEXT("FF.FinalReportSky.Version");
	const FString Folder = TEXT("/Game/FestaFiesta/UI/Report");
	const FString PanelPackage = Folder / TEXT("WBP_FF_FinalReportScreen");
	const FString RowPackage = Folder / TEXT("WBP_FF_FinalReportDayRow");
	const FString IconFolder = TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons");

	UTextBlock* Label(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		const FText& Preview, int32 FontSize = 18, FLinearColor Tint = Ink, bool bVariable = false)
	{
		UTextBlock* Result = Text(BP, Tree, Name, Preview, FontSize, Tint, bVariable);
		Result->SetAutoWrapText(true);
		Result->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return Result;
	}

	UTextBlock* Value(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		int32 FontSize = 24, FLinearColor Tint = Ink)
	{
		return Label(BP, Tree, Name, LOCTEXT("Unknown", "—"), FontSize, Tint, true);
	}

	USizeBox* Size(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		UWidget& Child, float Width = 0.f, float MinHeight = 0.f)
	{
		USizeBox* Result = MakeWidget<USizeBox>(BP, Tree, Name);
		if (Width > 0.f) { Result->SetWidthOverride(Width); }
		if (MinHeight > 0.f) { Result->SetMinDesiredHeight(MinHeight); }
		Result->AddChild(&Child);
		return Result;
	}

	UBorder* Card(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		UWidget& Child, FMargin Padding = FMargin(18.f, 14.f), FLinearColor Fill = Paper)
	{
		UBorder* Result = MakeWidget<UBorder>(BP, Tree, Name);
		Result->SetBrush(Brush(Fill, 7.f, 1.f, SkyLine));
		Result->SetPadding(Padding);
		Result->AddChild(&Child);
		return Result;
	}

	USizeBox* Icon(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		const FString& RelativePath, float Dimension, bool bOriginalColor = false)
	{
		UImage* Image = MakeWidget<UImage>(BP, Tree, Name);
		Image->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr, *MakeObjectPath(IconFolder / RelativePath)));
		Image->SetColorAndOpacity(bOriginalColor ? FLinearColor::White : Ink);
		USizeBox* Bounds = Size(BP, Tree, *(FString(Name) + TEXT("_Size")), *Image, Dimension);
		Bounds->SetHeightOverride(Dimension);
		return Bounds;
	}

	void CenterChildren(UHorizontalBox& Parent)
	{
		for (int32 Index = 0; Index < Parent.GetChildrenCount(); ++Index)
		{
			CastChecked<UHorizontalBoxSlot>(Parent.GetChildAt(Index)->Slot)->SetVerticalAlignment(VAlign_Center);
		}
	}

	UScrollBox* Scroll(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, UWidget& Child)
	{
		UScrollBox* Result = MakeWidget<UScrollBox>(BP, Tree, Name);
		Result->SetAllowOverscroll(false);
		Result->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
		Result->SetScrollbarThickness(FVector2D(5.f, 5.f));
		Result->SetClipping(EWidgetClipping::ClipToBounds);
		CastChecked<UScrollBoxSlot>(Result->AddChild(&Child))->SetHorizontalAlignment(HAlign_Fill);
		return Result;
	}

	void TableCell(UWidgetBlueprint& BP, UWidgetTree& Tree, UHorizontalBox& Parent,
		const TCHAR* Name, const FText& Caption, bool bHeader, bool bFirst)
	{
		UTextBlock* TextCell = Label(BP, Tree, Name, Caption, bHeader ? 16 : 18,
			bHeader ? Muted : Ink, !bHeader);
		TextCell->SetJustification(bFirst ? ETextJustify::Center : ETextJustify::Right);
		Column(Parent, *TextCell, FMargin(14.f, 9.f), true);
	}

	bool BuildDayRow(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		UHorizontalBox* Cells = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_DayCells"));
		TableCell(BP, Tree, *Cells, TEXT("Text_Day"), LOCTEXT("UnknownDay", "—일차"), false, true);
		TableCell(BP, Tree, *Cells, TEXT("Text_Revenue"), LOCTEXT("Unknown", "—"), false, false);
		TableCell(BP, Tree, *Cells, TEXT("Text_Expense"), LOCTEXT("Unknown", "—"), false, false);
		TableCell(BP, Tree, *Cells, TEXT("Text_NetProfit"), LOCTEXT("Unknown", "—"), false, false);
		TableCell(BP, Tree, *Cells, TEXT("Text_AdmittedVisitors"), LOCTEXT("Unknown", "—"), false, false);
		CenterChildren(*Cells);
		UBorder* Backdrop = Card(BP, Tree, TEXT("Border_DayRow"), *Cells, FMargin(0.f));
		Backdrop->SetBrush(Brush(Paper, 0.f, 0.5f, SkyLine));
		Tree.RootWidget = Size(BP, Tree, TEXT("SizeBox_DayRow"), *Backdrop, 0.f, 44.f);
		BP.GetOutermost()->GetMetaData().SetValue(&BP, VersionKey, TEXT("1"));
		return true;
	}

	void Axis(UWidgetBlueprint& BP, UWidgetTree& Tree, UVerticalBox& Parent,
		const TCHAR* Key, const FText& Caption)
	{
		const FString Prefix = FString(TEXT("Axis_")) + Key;
		UHorizontalBox* Content = MakeWidget<UHorizontalBox>(BP, Tree, FName(Prefix + TEXT("_Row")));
		Column(*Content, *Size(BP, Tree, *(Prefix + TEXT("_CaptionSize")),
			*Label(BP, Tree, *(Prefix + TEXT("_Caption")), Caption, 17), 95.f));
		UProgressBar* Progress = MakeWidget<UProgressBar>(BP, Tree, FName(FString(TEXT("Progress_")) + Key), true);
		FProgressBarStyle Style = Progress->GetWidgetStyle();
		Style.BackgroundImage = Brush(Color(224, 236, 241), 9.f);
		Style.FillImage = Brush(FLinearColor::White, 9.f);
		Style.MarqueeImage = Style.FillImage;
		Progress->SetWidgetStyle(Style);
		Progress->SetFillColorAndOpacity(Green);
		Progress->SetPercent(0.f);
		Progress->SetVisibility(ESlateVisibility::HitTestInvisible);
		USizeBox* BarSize = Size(BP, Tree, *(Prefix + TEXT("_BarSize")), *Progress);
		BarSize->SetHeightOverride(18.f);
		Column(*Content, *BarSize, FMargin(10.f, 0.f, 12.f, 0.f), true);
		UTextBlock* Number = Value(BP, Tree, *(FString(TEXT("Text_")) + Key), 20);
		Number->SetJustification(ETextJustify::Right);
		Column(*Content, *Size(BP, Tree, *(Prefix + TEXT("_ValueSize")), *Number, 48.f));
		CenterChildren(*Content);
		Row(Parent, *Content, FMargin(0.f, 4.f, 0.f, 7.f));
	}

	void Metric(UWidgetBlueprint& BP, UWidgetTree& Tree, UHorizontalBox& Parent,
		const TCHAR* Key, const FText& Caption, const TCHAR* Binding, bool bFirst = false)
	{
		const FString Prefix = FString(TEXT("Metric_")) + Key;
		UVerticalBox* Content = MakeWidget<UVerticalBox>(BP, Tree, FName(Prefix + TEXT("_Body")));
		Row(*Content, *Label(BP, Tree, *(Prefix + TEXT("_Caption")), Caption, 18), FMargin(0.f, 0.f, 0.f, 9.f));
		Row(*Content, *Value(BP, Tree, Binding, 34));
		Column(Parent, *Size(BP, Tree, *(Prefix + TEXT("_Size")),
			*Card(BP, Tree, *(Prefix + TEXT("_Card")), *Content, FMargin(18.f, 12.f), SkyWash), 0.f, 102.f),
			FMargin(bFirst ? 0.f : 12.f, 0.f, 0.f, 0.f), true);
	}

	bool BuildPanel(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(BP, Tree, TEXT("CanvasPanel_Root"));
		Tree.RootWidget = Root;
		UBorder* Backdrop = MakeWidget<UBorder>(BP, Tree, TEXT("Border_ScreenBackdrop"));
		Backdrop->SetBrush(Brush(FLinearColor(0.015f, 0.025f, 0.025f, .38f)));
		Backdrop->SetPadding(FMargin(0.f));
		StretchOnCanvas(*Root, *Backdrop);
		UScaleBox* Scale = MakeWidget<UScaleBox>(BP, Tree, TEXT("ScaleBox_Viewport"));
		Scale->SetStretch(EStretch::ScaleToFit);
		Scale->SetStretchDirection(EStretchDirection::DownOnly);
		UCanvasPanelSlot* Placement = StretchOnCanvas(*Root, *Scale);
		Placement->SetOffsets(FMargin(32.f));
		Placement->SetZOrder(1);
		UVerticalBox* Sections = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Sections"));
		// 제목과 하단 안내도 길어질 수 있으므로 패널 전체를 스크롤할 수 있게 한다.
		UBorder* Panel = Card(BP, Tree, TEXT("Border_ReportPanel"),
			*Scroll(BP, Tree, TEXT("ScrollBox_ReportContent"), *Sections), FMargin(24.f));
		Panel->SetBrush(Brush(Paper, 10.f, 1.5f, SkyLine));
		Panel->SetClipping(EWidgetClipping::ClipToBounds);
		USizeBox* PanelSize = Size(BP, Tree, TEXT("SizeBox_ReportPanel"), *Panel, 1280.f);
		PanelSize->SetHeightOverride(900.f);
		UScaleBoxSlot* ScaleSlot = CastChecked<UScaleBoxSlot>(Scale->AddChild(PanelSize));
		ScaleSlot->SetHorizontalAlignment(HAlign_Center);
		ScaleSlot->SetVerticalAlignment(VAlign_Center);

		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Header"));
		Column(*Header, *Icon(BP, Tree, TEXT("Image_ReportIcon"), TEXT("Navigation/T_FF_LH_Icon_Report_White_256"), 42.f), FMargin(0.f, 0.f, 18.f, 0.f));
		UVerticalBox* Heading = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Heading"));
		Row(*Heading, *Label(BP, Tree, TEXT("Text_Title"), LOCTEXT("Title", "축제 최종 결산"), 34, Ink, true));
		Row(*Heading, *Label(BP, Tree, TEXT("Text_FestivalSummary"), LOCTEXT("FestivalSummary", "축제 운영 기록"), 19, Muted, true), FMargin(0.f, 4.f, 0.f, 0.f));
		Column(*Header, *Heading, FMargin(0.f), true);
		Column(*Header, *Label(BP, Tree, TEXT("Text_MoneyUnit"), LOCTEXT("MoneyUnit", "금액 단위: 금화"), 15, Muted), FMargin(16.f, 0.f, 0.f, 0.f));
		CenterChildren(*Header);
		Row(*Sections, *Size(BP, Tree, TEXT("SizeBox_Header"), *Header, 0.f, 74.f), FMargin(0.f, 0.f, 0.f, 10.f));
		Row(*Sections, *Rule(BP, Tree, TEXT("Border_HeaderRule")), FMargin(0.f, 0.f, 0.f, 14.f));

		UHorizontalBox* Evaluation = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Evaluation"));
		UVerticalBox* GradeContent = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Grade"));
		UTextBlock* GradeLetter = Value(BP, Tree, TEXT("Text_Grade"), 64);
		GradeLetter->SetJustification(ETextJustify::Center);
		UBorder* Grade = Card(BP, Tree, TEXT("Border_GradeBadge"), *GradeLetter, FMargin(5.f), Color(249, 244, 226));
		Grade->SetHorizontalAlignment(HAlign_Center); Grade->SetVerticalAlignment(VAlign_Center);
		Grade->SetBrush(Brush(Color(249, 244, 226), 12.f, 2.f, Color(212, 182, 104)));
		Row(*GradeContent, *Size(BP, Tree, TEXT("SizeBox_GradeBadge"), *Grade, 118.f, 118.f));
		UTextBlock* GradeCaption = Label(BP, Tree, TEXT("Text_GradeCaption"), LOCTEXT("GradeCaption", "종합 평가"), 16, Muted);
		GradeCaption->SetJustification(ETextJustify::Center);
		Row(*GradeContent, *GradeCaption, FMargin(0.f, 6.f, 0.f, 0.f));
		Column(*Evaluation, *GradeContent, FMargin(0.f, 0.f, 24.f, 0.f));
		UVerticalBox* Overall = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_OverallScore"));
		Row(*Overall, *Value(BP, Tree, TEXT("Text_TotalScore"), 60));
		Row(*Overall, *Label(BP, Tree, TEXT("Text_Headline"), FText::GetEmpty(), 20, Ink, true), FMargin(0.f, 7.f, 0.f, 0.f));
		Column(*Evaluation, *Overall, FMargin(0.f, 0.f, 24.f, 0.f), true);
		UVerticalBox* Axes = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_ScoreAxes"));
		Axis(BP, Tree, *Axes, TEXT("SatisfactionScore"), LOCTEXT("Satisfaction", "만족도"));
		Axis(BP, Tree, *Axes, TEXT("VisitorScore"), LOCTEXT("Visitors", "방문객"));
		Axis(BP, Tree, *Axes, TEXT("FinanceScore"), LOCTEXT("Finance", "재무"));
		Axis(BP, Tree, *Axes, TEXT("BuzzScore"), LOCTEXT("Buzz", "화제성"));
		Column(*Evaluation, *Axes, FMargin(0.f), true);
		CenterChildren(*Evaluation);
		Row(*Sections, *Card(BP, Tree, TEXT("Border_Evaluation"), *Evaluation), FMargin(0.f, 0.f, 0.f, 14.f));

		UHorizontalBox* Metrics = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Metrics"));
		Metric(BP, Tree, *Metrics, TEXT("Visitors"), LOCTEXT("TotalVisitors", "누적 입장객"), TEXT("Text_TotalAdmittedVisitors"), true);
		Metric(BP, Tree, *Metrics, TEXT("Revenue"), LOCTEXT("TotalRevenue", "총 매출"), TEXT("Text_TotalRevenue"));
		Metric(BP, Tree, *Metrics, TEXT("Expense"), LOCTEXT("TotalExpense", "총 지출"), TEXT("Text_TotalExpense"));
		Metric(BP, Tree, *Metrics, TEXT("NetProfit"), LOCTEXT("NetProfit", "순이익"), TEXT("Text_NetProfit"));
		Row(*Sections, *Metrics, FMargin(0.f, 0.f, 0.f, 14.f));

		UVerticalBox* DailyTable = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_DailyTable"));
		Row(*DailyTable, *Label(BP, Tree, TEXT("Text_DaysSummary"), LOCTEXT("DaysSummary", "일별 결산"), 23, Ink, true), FMargin(0.f, 0.f, 0.f, 10.f));
		UHorizontalBox* TableHeader = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_TableHeader"));
		TableCell(BP, Tree, *TableHeader, TEXT("Text_DayHeader"), LOCTEXT("Day", "날짜"), true, true);
		TableCell(BP, Tree, *TableHeader, TEXT("Text_RevenueHeader"), LOCTEXT("Revenue", "매출"), true, false);
		TableCell(BP, Tree, *TableHeader, TEXT("Text_ExpenseHeader"), LOCTEXT("Expense", "지출"), true, false);
		TableCell(BP, Tree, *TableHeader, TEXT("Text_NetProfitHeader"), LOCTEXT("NetProfit", "순이익"), true, false);
		TableCell(BP, Tree, *TableHeader, TEXT("Text_VisitorsHeader"), LOCTEXT("Admitted", "입장객"), true, false);
		Row(*DailyTable, *Card(BP, Tree, TEXT("Border_TableHeader"), *TableHeader, FMargin(0.f), SkyWash));
		UVerticalBox* DayRows = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Box_DayRows"), true);
		USizeBox* DayRowsSize = Size(BP, Tree, TEXT("SizeBox_DayRows"),
			*Scroll(BP, Tree, TEXT("ScrollBox_DayRows"), *DayRows), 0.f, 132.f);
		DayRowsSize->SetMaxDesiredHeight(184.f);
		Row(*DailyTable, *DayRowsSize);
		Row(*DailyTable, *Label(BP, Tree, TEXT("Text_VisitorHistoryNote"), FText::GetEmpty(), 14, Muted, true), FMargin(0.f, 8.f, 0.f, 0.f));
		Row(*Sections, *Card(BP, Tree, TEXT("Border_DailyTable"), *DailyTable, FMargin(16.f, 12.f)), FMargin(0.f, 0.f, 0.f, 14.f));

		UHorizontalBox* Footer = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Footer"));
		Column(*Footer, *Card(BP, Tree, TEXT("Border_WeakestAxis"),
			*Label(BP, Tree, TEXT("Text_WeakestAxisComment"), FText::GetEmpty(), 18, Ink, true), FMargin(16.f), SkyWash), FMargin(0.f, 0.f, 12.f, 0.f), true);
		UHorizontalBox* Balance = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Balance"));
		UVerticalBox* Gold = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_FinalGold"));
		Row(*Gold, *Label(BP, Tree, TEXT("Text_GoldCaption"), LOCTEXT("Gold", "최종 보유 금화"), 15, Muted), FMargin(0.f, 0.f, 0.f, 5.f));
		UHorizontalBox* GoldValue = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_FinalGoldValue"));
		Column(*GoldValue, *Icon(BP, Tree, TEXT("Image_FinalGold"), TEXT("Metrics/T_FF_LH_Icon_Gold_White_256"), 27.f, true), FMargin(0.f, 0.f, 8.f, 0.f));
		Column(*GoldValue, *Value(BP, Tree, TEXT("Text_Gold"), 27));
		CenterChildren(*GoldValue); Row(*Gold, *GoldValue);
		Column(*Balance, *Gold, FMargin(0.f, 0.f, 16.f, 0.f), true);
		Column(*Balance, *Rule(BP, Tree, TEXT("Border_RefundDivider"), true), FMargin(0.f, 4.f, 16.f, 4.f));
		UVerticalBox* Refund = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Refund"));
		Row(*Refund, *Label(BP, Tree, TEXT("Text_RefundCaption"), LOCTEXT("Refund", "철거 환급"), 14, Muted), FMargin(0.f, 0.f, 0.f, 5.f));
		Row(*Refund, *Value(BP, Tree, TEXT("Text_Refund"), 17));
		Row(*Refund, *Label(BP, Tree, TEXT("Text_RefundNote"), LOCTEXT("RefundNote", "매출과 별도 집계"), 12, Muted), FMargin(0.f, 3.f, 0.f, 0.f));
		Column(*Balance, *Refund);
		Column(*Footer, *Size(BP, Tree, TEXT("SizeBox_FinalBalance"), *Card(BP, Tree, TEXT("Border_FinalBalance"), *Balance, FMargin(16.f, 12.f), SkyWash), 346.f), FMargin(0.f, 0.f, 14.f, 0.f));
		UButton* Lobby = Button(BP, Tree, TEXT("Button_Lobby"),
			*Label(BP, Tree, TEXT("Text_LobbyLabel"), LOCTEXT("Lobby", "로비로"), 24, FLinearColor::White, true), true);
		Lobby->SetToolTipText(LOCTEXT("LobbyTip", "축제를 마치고 로비로 돌아갑니다."));
		Column(*Footer, *Size(BP, Tree, TEXT("SizeBox_LobbyButton"), *Lobby, 180.f, 70.f));
		Row(*Sections, *Footer);
		Row(*Sections, *Label(BP, Tree, TEXT("Text_Status"), FText::GetEmpty(), 15, Muted, true), FMargin(0.f, 10.f, 0.f, 0.f));
		return true;
	}

	bool CheckExisting(const FString& Package, UClass& Parent, const FString& BackupFolder)
	{
		if (!FPackageName::DoesPackageExist(Package)) { return true; }
		UWidgetBlueprint* Existing = LoadExistingBlueprint(Package);
		if (!Existing || !Existing->ParentClass || !Existing->ParentClass->IsChildOf(&Parent) || Existing->GetOutermost()->IsDirty())
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 부모 불일치 또는 미저장 에셋 보존: %s"), LogPrefix, *Package);
			return false;
		}
		const FString File = FPackageName::LongPackageNameToFilename(Package, TEXT(".uasset"));
		if (!IFileManager::Get().MakeDirectory(*BackupFolder, true)
			|| IFileManager::Get().Copy(*(BackupFolder / FPaths::GetCleanFilename(File)), *File, false, false) != COPY_OK)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 기존 에셋 백업 실패: %s"), LogPrefix, *Package);
			return false;
		}
		if (Existing->GetOutermost()->GetMetaData().GetValue(Existing, VersionKey) != TEXT("1"))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 같은 경로의 사용자/미완성 에셋을 덮어쓰지 않습니다: %s"), LogPrefix, *Package);
			return false;
		}
		return true;
	}

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: PIE 종료 후 실행하세요."), LogPrefix);
			return;
		}
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/FinalReportSky") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
		if (!CheckExisting(RowPackage, *UFF_FinalReportDayRowWidget::StaticClass(), Backup)
			|| !CheckExisting(PanelPackage, *UFF_ReportScreen::StaticClass(), Backup)) { return; }
		const bool bNewPanel = !FPackageName::DoesPackageExist(PanelPackage);
		UWidgetBlueprint* DayRow = EnsureWidgetBlueprint(LogPrefix, RowPackage, Folder, *UFF_FinalReportDayRowWidget::StaticClass(), BuildDayRow);
		if (!DayRow || !DayRow->GeneratedClass) { return; }
		UWidgetBlueprint* Panel = EnsureWidgetBlueprint(LogPrefix, PanelPackage, Folder, *UFF_ReportScreen::StaticClass(), BuildPanel);
		if (!Panel || !Panel->GeneratedClass) { return; }
		if (bNewPanel)
		{
			UFF_ReportScreen* Defaults = CastChecked<UFF_ReportScreen>(Panel->GeneratedClass->GetDefaultObject());
			Defaults->Modify(); Defaults->ConfigureDayRowDefaults(DayRow->GeneratedClass.Get());
			Panel->GetOutermost()->GetMetaData().SetValue(Panel, VersionKey, TEXT("1"));
			if (!SaveAssetPackage(LogPrefix, *Panel)) { return; }
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 최종 결산·일별 행 준비 완료. 기존 Designer/CDO 편집 보존, 최종 금화 아이콘 1개."), LogPrefix);
	}

	FAutoConsoleCommand Command(TEXT("FF.ApplyFinalReportSkyDesign"),
		TEXT("최종 결산 화면과 일별 행을 처음 한 번 생성한다. 기존 에디터 편집을 보존한다."),
		FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
