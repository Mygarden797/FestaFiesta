// P4 일일 결산 화면. 보고서 에셋만 꾸미고 다른 탭·하단 HUD·게임 상태는 변경하지 않는다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/LiveHUD/Panels/FF_LivePanelReport.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLiveReportSky"

namespace FFLiveReportSkyWbpBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* LogPrefix = TEXT("FF_LIVE_REPORT_SKY");
	constexpr const TCHAR* VersionKey = TEXT("FF.LiveReportSky.Version");
	const FString Folder = TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels");
	const FString PanelPackage = Folder / TEXT("WBP_FF_LivePanelReport");
	const FString RowPackage = Folder / TEXT("WBP_FF_DailyReportLedgerRow");
	const FString IconFolder = TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons");

	UTextBlock* Value(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		const FText& Preview, int32 FontSize = 17, FLinearColor Tint = Ink)
	{
		UTextBlock* Result = Cast<UTextBlock>(Tree.FindWidget(Name));
		if (Result)
		{
			Result->Modify(); Result->RemoveFromParent();
			StyleText(*Result, FontSize, Tint, FontSize >= 20 ? TEXT("SemiBold") : TEXT("Medium"));
			Result->SetText(Preview);
		}
		else { Result = Text(BP, Tree, Name, Preview, FontSize, Tint, true); }
		Result->SetAutoWrapText(true);
		Result->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return Result;
	}

	USizeBox* Sized(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		UWidget& Child, float Width = 0.f, float MinHeight = 0.f)
	{
		USizeBox* Result = MakeWidget<USizeBox>(BP, Tree, Name);
		if (Width > 0.f) { Result->SetWidthOverride(Width); }
		if (MinHeight > 0.f) { Result->SetMinDesiredHeight(MinHeight); }
		Result->AddChild(&Child); return Result;
	}

	USizeBox* Icon(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		const FString& RelativePath, float Size, bool bOriginalColor = false)
	{
		UImage* Image = MakeWidget<UImage>(BP, Tree, Name);
		Image->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr, *MakeObjectPath(IconFolder / RelativePath)));
		Image->SetColorAndOpacity(bOriginalColor ? FLinearColor::White : Ink);
		USizeBox* Bounds = Sized(BP, Tree, *(FString(Name) + TEXT("_Size")), *Image, Size);
		Bounds->SetHeightOverride(Size); return Bounds;
	}

	UBorder* Card(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, UWidget& Content,
		FMargin Padding = FMargin(18.f, 14.f), FLinearColor Fill = Paper)
	{
		UBorder* Result = MakeWidget<UBorder>(BP, Tree, Name);
		Result->SetBrush(Brush(Fill, 7.f, 1.f, SkyLine));
		Result->SetPadding(Padding); Result->AddChild(&Content); return Result;
	}

	void Heading(UWidgetBlueprint& BP, UWidgetTree& Tree, UVerticalBox& Parent,
		const TCHAR* Name, const FText& Caption)
	{
		Row(Parent, *Text(BP, Tree, Name, Caption, 20), FMargin(0.f, 0.f, 0.f, 9.f));
		Row(Parent, *Rule(BP, Tree, *(FString(Name) + TEXT("_Rule"))), FMargin(0.f, 0.f, 0.f, 12.f));
	}

	void Metric(UWidgetBlueprint& BP, UWidgetTree& Tree, UWrapBox& Metrics,
		const TCHAR* Key, const FText& Caption, const TCHAR* Binding, FLinearColor Tint = Ink)
	{
		const FString Prefix = FString(TEXT("Report_")) + Key;
		UVerticalBox* Content = MakeWidget<UVerticalBox>(BP, Tree, FName(Prefix + TEXT("_Body")));
		Row(*Content, *Text(BP, Tree, *(Prefix + TEXT("_Caption")), Caption, 17), FMargin(0.f, 0.f, 0.f, 10.f));
		Row(*Content, *Value(BP, Tree, Binding, LOCTEXT("Unknown", "—"), 31, Tint));
		Metrics.AddChild(Sized(BP, Tree, *(Prefix + TEXT("_Size")),
			*Card(BP, Tree, *(Prefix + TEXT("_Card")), *Content, FMargin(18.f, 14.f), SkyWash), 254.f, 112.f));
	}

	void SatisfactionRow(UWidgetBlueprint& BP, UWidgetTree& Tree, UVerticalBox& Parent,
		const TCHAR* Key, const FText& Caption)
	{
		const FString Prefix = FString(TEXT("Report_")) + Key;
		UHorizontalBox* Line = MakeWidget<UHorizontalBox>(BP, Tree, FName(Prefix + TEXT("_Row")));
		Column(*Line, *Sized(BP, Tree, *(Prefix + TEXT("_LabelSize")),
			*Text(BP, Tree, *(Prefix + TEXT("_Label")), Caption, 16), 96.f));
		UProgressBar* Progress = MakeWidget<UProgressBar>(BP, Tree, FName(FString(TEXT("Progress_")) + Key), true);
		FProgressBarStyle Style = Progress->GetWidgetStyle();
		Style.BackgroundImage = Brush(Color(224, 236, 241), 8.f);
		Style.FillImage = Brush(FLinearColor::White, 8.f); Style.MarqueeImage = Style.FillImage;
		Progress->SetWidgetStyle(Style); Progress->SetFillColorAndOpacity(Green); Progress->SetPercent(0.f);
		Progress->SetVisibility(ESlateVisibility::HitTestInvisible);
		USizeBox* BarSize = Sized(BP, Tree, *(Prefix + TEXT("_BarSize")), *Progress); BarSize->SetHeightOverride(16.f);
		Column(*Line, *BarSize, FMargin(8.f, 0.f, 12.f, 0.f), true);
		UTextBlock* Number = Value(BP, Tree, *(FString(TEXT("Text_")) + Key), LOCTEXT("Unknown", "—"), 17);
		Number->SetJustification(ETextJustify::Right);
		Column(*Line, *Sized(BP, Tree, *(Prefix + TEXT("_ValueSize")), *Number, 46.f));
		for (int32 Index = 0; Index < Line->GetChildrenCount(); ++Index) { CastChecked<UHorizontalBoxSlot>(Line->GetChildAt(Index)->Slot)->SetVerticalAlignment(VAlign_Center); }
		Row(Parent, *Line, FMargin(0.f, 0.f, 0.f, 12.f));
	}

	bool BuildLedgerRow(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		UHorizontalBox* Content = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Row"));
		Tree.RootWidget = Sized(BP, Tree, TEXT("SizeBox_Row"), *Content, 0.f, 30.f);
		Column(*Content, *Value(BP, Tree, TEXT("Text_Label"), LOCTEXT("LedgerPreview", "정산 항목"), 16), FMargin(0.f, 2.f, 16.f, 2.f), true);
		UTextBlock* Amount = Value(BP, Tree, TEXT("Text_Amount"), LOCTEXT("Unknown", "—"), 17);
		Amount->SetJustification(ETextJustify::Right);
		USizeBox* AmountSize = Sized(BP, Tree, TEXT("SizeBox_Amount"), *Amount);
		AmountSize->SetMinDesiredWidth(105.f);
		Column(*Content, *AmountSize, FMargin(0.f, 2.f));
		return true;
	}

	bool BuildPanel(UWidgetBlueprint& BP, UClass* LedgerClass)
	{
		UWidgetTree& Tree = *BP.WidgetTree;
		UBorder* Root = Cast<UBorder>(Tree.RootWidget);
		UButton* Close = Cast<UButton>(Tree.FindWidget(TEXT("Button_Close")));
		UButton* Advance = Cast<UButton>(Tree.FindWidget(TEXT("Button_Advance")));
		if (!Root || !Root->GetContent() || !Close || !Advance || !Tree.FindWidget(TEXT("Text_AdvanceLabel"))) { return false; }
		BP.Modify(); Tree.Modify(); Root->Modify();
		UWidget* OldContent = Root->GetContent(); Root->RemoveChild(OldContent);
		UVerticalBox* Sections = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Report_Sections"));
		Root->AddChild(Sections); Root->SetPadding(FMargin(2.f));
		Root->SetBrush(Brush(Paper, 9.f, 1.5f, SkyLine)); Root->SetClipping(EWidgetClipping::ClipToBounds);
		UVerticalBox* Legacy = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Report_Legacy"));
		Legacy->SetVisibility(ESlateVisibility::Collapsed); Legacy->AddChild(OldContent); Row(*Sections, *Legacy);

		// 이름/GUID/바인딩이 있는 기존 객체는 남기고 표시할 값과 버튼만 새 레이아웃으로 옮긴다.
		Close->Modify(); Close->RemoveFromParent(); Advance->Modify(); Advance->RemoveFromParent();
		// Advance의 기존 라벨도 FindWidget으로 다시 찾을 수 있도록 임시 보관함에 연결한다.
		Legacy->AddChild(Advance);
		if (UWidget* OldCloseGlyph = Close->GetContent()) { Close->RemoveChild(OldCloseGlyph); Legacy->AddChild(OldCloseGlyph); }
		USizeBox* CloseIconSize = Icon(BP, Tree, TEXT("Report_CloseIcon"), TEXT("Actions/T_FF_LH_Icon_Close_White_256"), 18.f);
		UButtonSlot* CloseSlot = CastChecked<UButtonSlot>(Close->AddChild(CloseIconSize));
		CloseSlot->SetHorizontalAlignment(HAlign_Center); CloseSlot->SetVerticalAlignment(VAlign_Center); CloseSlot->SetPadding(FMargin(0.f));
		FButtonStyle CloseStyle = Close->GetStyle();
		CloseStyle.Normal = Brush(Paper, 6.f, 1.f, SkyLine); CloseStyle.Hovered = Brush(SkyWash, 6.f, 2.f, SkyLine);
		CloseStyle.Pressed = Brush(Color(221, 239, 246), 6.f, 2.f, Ink); CloseStyle.Disabled = CloseStyle.Normal;
		CloseStyle.NormalPadding = CloseStyle.PressedPadding = FMargin(0.f); Close->SetStyle(CloseStyle);

		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Report_Header"));
		USizeBox* ReportIcon = Icon(BP, Tree, TEXT("Report_HeaderIcon"), TEXT("Navigation/T_FF_LH_Icon_Report_White_256"), 34.f);
		Column(*Header, *ReportIcon, FMargin(0.f, 0.f, 16.f, 0.f)); CastChecked<UHorizontalBoxSlot>(ReportIcon->Slot)->SetVerticalAlignment(VAlign_Center);
		UVerticalBox* HeadingBox = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Report_Heading"));
		Row(*HeadingBox, *Value(BP, Tree, TEXT("Text_DayLabel"), LOCTEXT("DayPreview", "일일 결산"), 28));
		Row(*HeadingBox, *Text(BP, Tree, TEXT("Report_Subtitle"), LOCTEXT("Subtitle", "오늘의 운영 결과를 확인하세요."), 16, Muted), FMargin(0.f, 3.f, 0.f, 0.f));
		Column(*Header, *HeadingBox, FMargin(0.f), true); CastChecked<UHorizontalBoxSlot>(HeadingBox->Slot)->SetVerticalAlignment(VAlign_Center);
		UTextBlock* Unit = Text(BP, Tree, TEXT("Report_MoneyUnit"), LOCTEXT("Unit", "금액 단위: 금화"), 14, Muted);
		Column(*Header, *Unit, FMargin(0.f, 0.f, 16.f, 0.f)); CastChecked<UHorizontalBoxSlot>(Unit->Slot)->SetVerticalAlignment(VAlign_Center);
		Column(*Header, *Sized(BP, Tree, TEXT("Report_CloseSize"), *Close, 36.f, 36.f));
		CastChecked<UHorizontalBoxSlot>(Close->GetParent()->Slot)->SetVerticalAlignment(VAlign_Center);
		USizeBox* HeaderSize = Sized(BP, Tree, TEXT("Report_HeaderSize"), *Header); HeaderSize->SetHeightOverride(88.f);
		Row(*Sections, *HeaderSize, FMargin(22.f, 0.f));
		Row(*Sections, *Rule(BP, Tree, TEXT("Report_HeaderRule")), FMargin(22.f, 0.f));

		UScrollBox* Scroll = MakeWidget<UScrollBox>(BP, Tree, TEXT("Report_ScrollBody"));
		Scroll->SetAllowOverscroll(false); Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
		Scroll->SetScrollbarThickness(FVector2D(5.f, 5.f)); Scroll->SetClipping(EWidgetClipping::ClipToBounds);
		UVerticalBox* Body = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Report_Body"));
		CastChecked<UScrollBoxSlot>(Scroll->AddChild(Body))->SetHorizontalAlignment(HAlign_Fill);
		Row(*Sections, *Scroll, FMargin(20.f, 14.f), true);
		UWrapBox* Summary = MakeWidget<UWrapBox>(BP, Tree, TEXT("Report_SummaryCards")); Summary->SetInnerSlotPadding(FVector2D(12.f, 12.f));
		Metric(BP, Tree, *Summary, TEXT("Revenue"), LOCTEXT("Revenue", "오늘 매출"), TEXT("Text_TodayRevenue"));
		Metric(BP, Tree, *Summary, TEXT("Expense"), LOCTEXT("Expense", "오늘 지출"), TEXT("Text_TodayExpense"));
		Metric(BP, Tree, *Summary, TEXT("NetProfit"), LOCTEXT("Profit", "순이익"), TEXT("Text_TodayNetProfit"), Green);
		Metric(BP, Tree, *Summary, TEXT("Satisfaction"), LOCTEXT("Satisfaction", "관객 만족도"), TEXT("Text_SimSatisfaction"));
		Row(*Body, *Summary, FMargin(0.f, 0.f, 0.f, 16.f));

		UHorizontalBox* Details = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Report_DetailColumns"));
		UVerticalBox* Ledger = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Report_LedgerBody"));
		Heading(BP, Tree, *Ledger, TEXT("Report_LedgerTitle"), LOCTEXT("LedgerTitle", "오늘의 수입과 지출"));
		Row(*Ledger, *Text(BP, Tree, TEXT("Report_EarnCaption"), LOCTEXT("Earn", "수입"), 15, Muted), FMargin(0.f, 0.f, 0.f, 5.f));
		Row(*Ledger, *MakeWidget<UVerticalBox>(BP, Tree, TEXT("Box_EarnRows"), true));
		Row(*Ledger, *Rule(BP, Tree, TEXT("Report_SpendRule")), FMargin(0.f, 12.f, 0.f, 10.f));
		Row(*Ledger, *Text(BP, Tree, TEXT("Report_SpendCaption"), LOCTEXT("Spend", "지출"), 15, Muted), FMargin(0.f, 0.f, 0.f, 5.f));
		Row(*Ledger, *MakeWidget<UVerticalBox>(BP, Tree, TEXT("Box_SpendRows"), true));
		UHorizontalBox* Refund = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Report_RefundRow"));
		UVerticalBox* RefundLabels = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Report_RefundLabels"));
		Row(*RefundLabels, *Text(BP, Tree, TEXT("Report_RefundCaption"), LOCTEXT("Refund", "철거 환급"), 16));
		Row(*RefundLabels, *Text(BP, Tree, TEXT("Report_RefundNote"), LOCTEXT("RefundNote", "매출과 별도 집계"), 13, Muted));
		Column(*Refund, *RefundLabels, FMargin(0.f, 0.f, 12.f, 0.f), true);
		Column(*Refund, *Value(BP, Tree, TEXT("Text_Refund"), LOCTEXT("Unknown", "—"), 23, Green));
		Row(*Ledger, *Card(BP, Tree, TEXT("Report_RefundCard"), *Refund, FMargin(12.f, 10.f), SkyWash), FMargin(0.f, 12.f, 0.f, 10.f));
		Row(*Ledger, *Value(BP, Tree, TEXT("Text_LedgerNote"), FText::GetEmpty(), 14, Muted));
		Column(*Details, *Card(BP, Tree, TEXT("Report_LedgerCard"), *Ledger), FMargin(0.f, 0.f, 14.f, 0.f), true);

		UVerticalBox* Right = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Report_RightColumn"));
		UVerticalBox* Satisfaction = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Report_SatisfactionBody"));
		Heading(BP, Tree, *Satisfaction, TEXT("Report_SatisfactionTitle"), LOCTEXT("SatisfactionParts", "관객 만족도 구성"));
		SatisfactionRow(BP, Tree, *Satisfaction, TEXT("WaitScore"), LOCTEXT("Wait", "대기"));
		SatisfactionRow(BP, Tree, *Satisfaction, TEXT("DensityScore"), LOCTEXT("Density", "밀도"));
		SatisfactionRow(BP, Tree, *Satisfaction, TEXT("PreferenceScore"), LOCTEXT("Preference", "취향 매칭"));
		SatisfactionRow(BP, Tree, *Satisfaction, TEXT("NeedScore"), LOCTEXT("Need", "욕구 충족"));
		UHorizontalBox* Score = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Report_ScoreRow"));
		Column(*Score, *Text(BP, Tree, TEXT("Report_ScoreCaption"), LOCTEXT("Score", "평가 만족도"), 15, Muted), FMargin(0.f), true);
		Column(*Score, *Value(BP, Tree, TEXT("Text_ScoreSatisfaction"), LOCTEXT("Unknown", "—"), 19));
		Row(*Satisfaction, *Score, FMargin(0.f, 2.f, 0.f, 8.f));
		Row(*Satisfaction, *Value(BP, Tree, TEXT("Text_SatisfactionNote"), FText::GetEmpty(), 14, Muted));
		Row(*Right, *Card(BP, Tree, TEXT("Report_SatisfactionCard"), *Satisfaction), FMargin(0.f, 0.f, 0.f, 14.f));
		UVerticalBox* Goal = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Report_GoalBody"));
		Heading(BP, Tree, *Goal, TEXT("Report_GoalTitle"), LOCTEXT("Goal", "일일 목표"));
		Row(*Goal, *Value(BP, Tree, TEXT("Text_DailyGoal"), LOCTEXT("UnknownGoal", "오늘 입장객 —"), 22), FMargin(0.f, 0.f, 0.f, 8.f));
		Row(*Goal, *Value(BP, Tree, TEXT("Text_DailyGoalState"), LOCTEXT("GoalPending", "집계 확인 중"), 17, Green));
		Row(*Right, *Card(BP, Tree, TEXT("Report_GoalCard"), *Goal));
		Column(*Details, *Right, FMargin(0.f), true); Row(*Body, *Details);
		Row(*Body, *Value(BP, Tree, TEXT("Text_MissingNote"), FText::GetEmpty(), 14, Muted), FMargin(0.f, 12.f, 0.f, 0.f));

		Row(*Sections, *Rule(BP, Tree, TEXT("Report_FooterRule")), FMargin(22.f, 0.f));
		UVerticalBox* Footer = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Report_Footer"));
		UHorizontalBox* FooterMain = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Report_FooterMain"));
		UHorizontalBox* Gold = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Report_GoldGroup"));
		Column(*Gold, *Text(BP, Tree, TEXT("Report_GoldCaption"), LOCTEXT("Gold", "보유 금화"), 18), FMargin(0.f, 0.f, 12.f, 0.f));
		Column(*Gold, *Icon(BP, Tree, TEXT("Image_ReportGold"), TEXT("Metrics/T_FF_LH_Icon_Gold_White_256"), 27.f, true), FMargin(0.f, 0.f, 8.f, 0.f));
		Column(*Gold, *Value(BP, Tree, TEXT("Text_Gold"), LOCTEXT("Unknown", "—"), 26));
		for (int32 Index = 0; Index < Gold->GetChildrenCount(); ++Index) { CastChecked<UHorizontalBoxSlot>(Gold->GetChildAt(Index)->Slot)->SetVerticalAlignment(VAlign_Center); }
		Column(*FooterMain, *Gold, FMargin(0.f, 0.f, 20.f, 0.f), true); CastChecked<UHorizontalBoxSlot>(Gold->Slot)->SetVerticalAlignment(VAlign_Center);
		UTextBlock* AdvanceLabel = Value(BP, Tree, TEXT("Text_AdvanceLabel"), LOCTEXT("Advance", "다음 날 준비하기"), 20, FLinearColor::White);
		Advance->RemoveFromParent();
		if (UWidget* OldAdvanceChild = Advance->GetContent()) { Advance->RemoveChild(OldAdvanceChild); Legacy->AddChild(OldAdvanceChild); }
		UButtonSlot* AdvanceSlot = CastChecked<UButtonSlot>(Advance->AddChild(AdvanceLabel));
		AdvanceSlot->SetHorizontalAlignment(HAlign_Center); AdvanceSlot->SetVerticalAlignment(VAlign_Center); AdvanceSlot->SetPadding(FMargin(0.f));
		FButtonStyle AdvanceStyle = Advance->GetStyle();
		AdvanceStyle.Normal = Brush(Green, 7.f); AdvanceStyle.Hovered = Brush(Color(21, 122, 80), 7.f);
		AdvanceStyle.Pressed = Brush(Ink, 7.f); AdvanceStyle.Disabled = Brush(Color(138, 164, 153), 7.f);
		AdvanceStyle.NormalPadding = AdvanceStyle.PressedPadding = FMargin(18.f, 8.f); Advance->SetStyle(AdvanceStyle);
		Column(*FooterMain, *Sized(BP, Tree, TEXT("Report_AdvanceSize"), *Advance, 270.f, 48.f));
		Row(*Footer, *FooterMain);
		Row(*Footer, *Value(BP, Tree, TEXT("Text_AdvanceReason"), FText::GetEmpty(), 14, Muted), FMargin(0.f, 7.f, 0.f, 0.f));
		Row(*Sections, *Footer, FMargin(22.f, 12.f, 22.f, 14.f));

		UFF_LivePanelReport* Defaults = Cast<UFF_LivePanelReport>(BP.GeneratedClass ? BP.GeneratedClass->GetDefaultObject() : nullptr);
		if (!Defaults) { return false; }
		Defaults->Modify(); Defaults->ConfigureLedgerRowDefaults(LedgerClass);
		Defaults->DynamicRowFont = AdvanceLabel->GetFont(); Defaults->DynamicRowFont.Size = 16; Defaults->DynamicRowFont.TypefaceFontName = TEXT("Medium");
		Defaults->DynamicRowColor = Ink; Defaults->DynamicRowNoteColor = Muted;
		BP.GetOutermost()->GetMetaData().SetValue(&BP, VersionKey, TEXT("1"));
		return true;
	}

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: PIE 종료 후 실행하세요."), LogPrefix); return; }
		UWidgetBlueprint* BP = LoadExistingBlueprint(PanelPackage);
		if (!BP || !BP->WidgetTree || !BP->ParentClass || !BP->ParentClass->IsChildOf(UFF_LivePanelReport::StaticClass())) { return; }
		if (BP->GetOutermost()->GetMetaData().GetValue(BP, VersionKey) == TEXT("1")) { UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: 결산 Designer 편집값 보존."), LogPrefix); return; }
		if (BP->GetOutermost()->IsDirty() || BP->WidgetTree->FindWidget(TEXT("Report_Sections"))) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 미저장/부분 적용 결산 WBP를 덮어쓰지 않습니다."), LogPrefix); return; }
		const FString File = FPackageName::LongPackageNameToFilename(PanelPackage, TEXT(".uasset"));
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/LiveReportSky") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
		if (IFileManager::Get().IsReadOnly(*File) || !IFileManager::Get().MakeDirectory(*Backup, true) || IFileManager::Get().Copy(*(Backup / FPaths::GetCleanFilename(File)), *File, false, false) != COPY_OK) { return; }
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_BACKUP: %s"), LogPrefix, *FPaths::ConvertRelativePathToFull(Backup));
		UWidgetBlueprint* Ledger = EnsureWidgetBlueprint(LogPrefix, RowPackage, Folder, *UFF_DailyReportLedgerRowWidget::StaticClass(), BuildLedgerRow);
		if (!Ledger || !Ledger->GeneratedClass || !Ledger->GeneratedClass->IsChildOf(UFF_DailyReportLedgerRowWidget::StaticClass())) { return; }
		if (!BuildPanel(*BP, Ledger->GeneratedClass.Get()) || !SaveNewBlueprint(LogPrefix, *BP))
		{
			BP->GetOutermost()->GetMetaData().RemoveValue(BP, VersionKey);
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 결산 제작/저장 실패. 백업=%s"), LogPrefix, *Backup); return;
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 일일 결산·정산 행 저장 완료. 보유 금화 아이콘 1개."), LogPrefix);
	}
	FAutoConsoleCommand Command(TEXT("FF.ApplyLiveReportSkyDesign"), TEXT("일일 결산 화면과 정산 행 스타일을 최초 한 번 적용한다."), FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
