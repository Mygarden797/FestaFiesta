// P3 시설 화면과 편집 가능한 목록 행 제작. 기존 조회·시간·입력 규칙은 유지한다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"

#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/LiveHUD/Panels/FF_LiveFacilityRowWidget.h"
#include "UI/LiveHUD/Panels/FF_LivePanelFacilities.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLiveFacilitiesSky"

namespace FFLiveFacilitiesSkyBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* LogPrefix = TEXT("FF_LIVE_FACILITIES_SKY");
	constexpr const TCHAR* VersionKey = TEXT("FF.LiveFacilitiesSky.Version");
	const FString PanelPackage = TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelFacilities");
	const FString RowFolder = TEXT("/Game/FestaFiesta/UI/LiveHUD/Rows");
	const FString RowPackage = RowFolder / TEXT("WBP_FF_LiveFacilityRow");

	UTextBlock* Label(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		const FText& Value, int32 Size = 17, FLinearColor Tint = Ink, bool bVariable = false)
	{
		UTextBlock* Result = Text(BP, Tree, Name, Value, Size, Tint, bVariable);
		Result->SetAutoWrapText(true);
		return Result;
	}

	// 머리글과 행은 같은 폭을 쓴다. 저장 후 양쪽 WBP의 SizeBox를 Designer에서 편집한다.
	void Cell(UWidgetBlueprint& BP, UWidgetTree& Tree, UHorizontalBox& Parent,
		const TCHAR* Name, UWidget& Content, float Width, bool bLast = false)
	{
		USizeBox* Box = MakeWidget<USizeBox>(BP, Tree, Name);
		if (Width > 0.f) Box->SetWidthOverride(Width);
		Box->AddChild(&Content);
		Column(Parent, *Box, FMargin(0.f, 0.f, bLast ? 0.f : 12.f, 0.f), Width <= 0.f);
		CastChecked<UHorizontalBoxSlot>(Box->Slot)->SetVerticalAlignment(VAlign_Center);
	}

	bool BuildRow(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		USizeBox* Size = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_Row"));
		Size->SetMinDesiredHeight(72.f); Tree.RootWidget = Size;
		UBorder* Frame = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Row"));
		Frame->SetBrush(Brush(Paper, 5.f, 1.f, SkyLine)); Frame->SetPadding(FMargin(12.f, 12.f)); Size->AddChild(Frame);
		UHorizontalBox* Columns = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Columns")); Frame->AddChild(Columns);
		Cell(BP, Tree, *Columns, TEXT("SizeBox_Name"), *Label(BP, Tree, TEXT("Text_Name"), LOCTEXT("Name", "시설명"), 18, Ink, true), 0.f);
		UVerticalBox* StatusBody = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Status"));
		UBorder* Badge = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Status"), true);
		Badge->SetBrush(Brush(FLinearColor::White, 5.f)); Badge->SetPadding(FMargin(9.f, 4.f));
		Badge->AddChild(Label(BP, Tree, TEXT("Text_Status"), LOCTEXT("Normal", "정상 운영"), 16, Ink, true));
		Row(*StatusBody, *Badge);
		UTextBlock* Reason = Label(BP, Tree, TEXT("Text_Reason"), FText::GetEmpty(), 14, Muted, true);
		Row(*StatusBody, *Reason, FMargin(0.f, 4.f, 0.f, 0.f));
		Cell(BP, Tree, *Columns, TEXT("SizeBox_Status"), *StatusBody, 240.f);
		const TCHAR* Names[] = { TEXT("Text_Waiting"), TEXT("Text_InService"), TEXT("Text_ExpectedWait") };
		const TCHAR* Sizes[] = { TEXT("SizeBox_Waiting"), TEXT("SizeBox_InService"), TEXT("SizeBox_ExpectedWait") };
		for (int32 Index = 0; Index < 3; ++Index)
		{
			UTextBlock* Value = Label(BP, Tree, Names[Index], LOCTEXT("Missing", "—"), 18, Ink, true);
			Value->SetJustification(ETextJustify::Center);
			Cell(BP, Tree, *Columns, Sizes[Index], *Value, Index == 2 ? 145.f : 100.f, Index == 2);
		}
		return true;
	}

	void Metric(UWidgetBlueprint& BP, UWidgetTree& Tree, UWrapBox& Parent,
		const TCHAR* Key, const FText& Caption, UTextBlock& Value)
	{
		const FString Prefix = FString(TEXT("Fac_")) + Key;
		USizeBox* Size = MakeWidget<USizeBox>(BP, Tree, FName(Prefix + TEXT("_Size")));
		Size->SetWidthOverride(336.f); Size->SetMinDesiredHeight(110.f);
		UBorder* Frame = MakeWidget<UBorder>(BP, Tree, FName(Prefix + TEXT("_Border")));
		Frame->SetBrush(Brush(SkyWash, 7.f, 1.f, SkyLine)); Frame->SetPadding(FMargin(20.f, 14.f)); Size->AddChild(Frame);
		UVerticalBox* Body = MakeWidget<UVerticalBox>(BP, Tree, FName(Prefix + TEXT("_Body"))); Frame->AddChild(Body);
		Row(*Body, *Label(BP, Tree, *(Prefix + TEXT("_Caption")), Caption, 16, Muted), FMargin(0.f, 0.f, 0.f, 6.f));
		StyleText(Value, 30, Ink, TEXT("SemiBold")); Value.SetText(LOCTEXT("MetricMissing", "—")); Value.SetAutoWrapText(true);
		Row(*Body, Value); Parent.AddChild(Size);
	}

	bool BuildPanel(UWidgetBlueprint& BP)
	{
		UWidgetTree& Tree = *BP.WidgetTree;
		UVerticalBox* Content = Cast<UVerticalBox>(Tree.FindWidget(TEXT("VerticalBox_Content")));
		UVerticalBox* Sections = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Sky_VerticalBox_Sections")));
		UScrollBox* OldScroll = Cast<UScrollBox>(Tree.FindWidget(TEXT("Sky_ScrollBox_Body")));
		UHorizontalBox* Header = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("HorizontalBox_Header")));
		USizeBox* HeaderSize = Cast<USizeBox>(Tree.FindWidget(TEXT("Sky_SizeBox_Header")));
		UWidget* Close = Tree.FindWidget(TEXT("Sky_SizeBox_Close"));
		UTextBlock* Title = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_PanelTitle")));
		UTextBlock* Count = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_FacilityCount")));
		UTextBlock* Summary = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Summary")));
		UTextBlock* Empty = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Empty")));
		UTextBlock* Note = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_MissingNote")));
		UVerticalBox* Rows = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Box_Facilities")));
		if (!Content || !Sections || !OldScroll || !Header || !HeaderSize || !Close || !Title || !Count || !Summary || !Empty || !Note || !Rows) return false;
		BP.Modify(); Tree.Modify();
		for (UWidget* Value : TArray<UWidget*>{Count, Summary, Empty, Note, Rows}) Value->RemoveFromParent();
		const TArray<UWidget*> OldChildren = Content->GetAllChildren(); Content->ClearChildren();
		UVerticalBox* Legacy = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Fac_LegacyDecorations"));
		Legacy->SetVisibility(ESlateVisibility::Collapsed);
		for (UWidget* Old : OldChildren) Legacy->AddChild(Old);
		Row(*Content, *Legacy);
		// 공통 외부 스크롤은 접어 두고, 시설 목록 하나만 스크롤한다.
		Content->RemoveFromParent(); OldScroll->SetVisibility(ESlateVisibility::Collapsed);
		Row(*Sections, *Content, FMargin(22.f, 16.f), true);
		Header->ClearChildren(); HeaderSize->SetHeightOverride(78.f);
		UVerticalBox* Heading = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Fac_Heading"));
		Column(*Header, *Heading, FMargin(0.f), true);
		CastChecked<UHorizontalBoxSlot>(Heading->Slot)->SetVerticalAlignment(VAlign_Center);
		StyleText(*Title, 26, Ink, TEXT("SemiBold")); Row(*Heading, *Title);
		Row(*Heading, *Label(BP, Tree, TEXT("Fac_Subtitle"), LOCTEXT("Subtitle", "배치한 시설의 운영 상태를 확인하세요."), 15, Muted), FMargin(0.f, 2.f, 0.f, 0.f));
		Column(*Header, *Close); CastChecked<UHorizontalBoxSlot>(Close->Slot)->SetVerticalAlignment(VAlign_Center);

		UWrapBox* Metrics = MakeWidget<UWrapBox>(BP, Tree, TEXT("Fac_Metrics")); Metrics->SetInnerSlotPadding(FVector2D(12.f, 12.f));
		Row(*Content, *Metrics, FMargin(0.f, 0.f, 0.f, 16.f));
		Metric(BP, Tree, *Metrics, TEXT("Total"), LOCTEXT("Total", "배치 시설"), *Count);
		Metric(BP, Tree, *Metrics, TEXT("Active"), LOCTEXT("Active", "운영 중"), *Label(BP, Tree, TEXT("Text_ActiveCount"), FText::GetEmpty(), 30, Ink, true));
		Metric(BP, Tree, *Metrics, TEXT("Attention"), LOCTEXT("Attention", "확인 필요"), *Label(BP, Tree, TEXT("Text_AttentionCount"), FText::GetEmpty(), 30, Ink, true));
		UBorder* TableHeading = MakeWidget<UBorder>(BP, Tree, TEXT("Fac_TableHeading"));
		TableHeading->SetBrush(Brush(SkyWash, 5.f, 1.f, SkyLine)); TableHeading->SetPadding(FMargin(12.f, 10.f));
		UHorizontalBox* Columns = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Fac_HeaderColumns")); TableHeading->AddChild(Columns);
		Cell(BP, Tree, *Columns, TEXT("Fac_NameColumn"), *Label(BP, Tree, TEXT("Fac_NameCaption"), LOCTEXT("NameCaption", "시설명")), 0.f);
		Cell(BP, Tree, *Columns, TEXT("Fac_StatusColumn"), *Label(BP, Tree, TEXT("Fac_StatusCaption"), LOCTEXT("StatusCaption", "운영 상태")), 240.f);
		const FText Captions[] = { LOCTEXT("Waiting", "대기"), LOCTEXT("Service", "이용 중"), LOCTEXT("Expected", "지금 서면") };
		const TCHAR* Keys[] = { TEXT("Fac_Waiting"), TEXT("Fac_InService"), TEXT("Fac_ExpectedWait") };
		for (int32 Index = 0; Index < 3; ++Index)
		{
			UTextBlock* Caption = Label(BP, Tree, *(FString(Keys[Index]) + TEXT("Caption")), Captions[Index]); Caption->SetJustification(ETextJustify::Center);
			Cell(BP, Tree, *Columns, Keys[Index], *Caption, Index == 2 ? 145.f : 100.f, Index == 2);
		}
		Row(*Content, *TableHeading, FMargin(0.f, 0.f, 0.f, 6.f));
		UScrollBox* Scroll = MakeWidget<UScrollBox>(BP, Tree, TEXT("Scroll_Facilities"));
		Scroll->SetAllowOverscroll(false); Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
		Scroll->SetScrollbarThickness(FVector2D(5.f, 5.f)); Scroll->SetClipping(EWidgetClipping::ClipToBounds);
		UVerticalBox* List = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Fac_ListContent"));
		CastChecked<UScrollBoxSlot>(Scroll->AddChild(List))->SetHorizontalAlignment(HAlign_Fill);
		Row(*List, *Rows);
		StyleText(*Empty, 18, Muted, TEXT("Medium")); Empty->SetAutoWrapText(true);
		Empty->SetText(LOCTEXT("Empty", "배치한 시설이 없습니다.")); Row(*List, *Empty, FMargin(12.f, 24.f));
		Row(*Content, *Scroll, FMargin(0.f, 0.f, 0.f, 12.f), true);
		UBorder* SummaryFrame = MakeWidget<UBorder>(BP, Tree, TEXT("Fac_SummaryBorder"));
		SummaryFrame->SetBrush(Brush(SkyWash, 6.f, 1.f, SkyLine)); SummaryFrame->SetPadding(FMargin(16.f, 10.f));
		StyleText(*Summary, 17, Ink, TEXT("Medium")); Summary->SetAutoWrapText(true);
		Summary->SetText(LOCTEXT("Summary", "시설 상태를 확인하고 있습니다.")); SummaryFrame->AddChild(Summary);
		Row(*Content, *SummaryFrame, FMargin(0.f, 0.f, 0.f, 8.f));
		StyleText(*Note, 14, Muted, TEXT("Medium")); Note->SetAutoWrapText(true);
		Note->SetText(LOCTEXT("Note", "시설의 운영 상태와 대기 현황을 확인할 수 있습니다.")); Row(*Content, *Note);
		BP.GetOutermost()->GetMetaData().SetValue(&BP, VersionKey, TEXT("1"));
		return true;
	}

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld) return;
		UWidgetBlueprint* BP = LoadExistingBlueprint(PanelPackage);
		if (!BP || !BP->WidgetTree || !BP->ParentClass->IsChildOf(UFF_LivePanelFacilities::StaticClass())) return;
		if (BP->GetOutermost()->GetMetaData().GetValue(BP, VersionKey) == TEXT("1"))
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: Designer 편집 보존"), LogPrefix); return;
		}
		const FString File = FPackageName::LongPackageNameToFilename(PanelPackage, TEXT(".uasset"));
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/LiveFacilitiesSky") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
		if (!IFileManager::Get().MakeDirectory(*Backup, true) || IFileManager::Get().Copy(*(Backup / FPaths::GetCleanFilename(File)), *File) != COPY_OK) return;
		UWidgetBlueprint* RowBP = EnsureWidgetBlueprint(LogPrefix, RowPackage, RowFolder, *UFF_LiveFacilityRowWidget::StaticClass(), BuildRow);
		if (!RowBP || !RowBP->GeneratedClass || !RowBP->GeneratedClass->IsChildOf(UFF_LiveFacilityRowWidget::StaticClass())) return;
		if (!BuildPanel(*BP) || !SaveNewBlueprint(LogPrefix, *BP))
		{
			BP->GetOutermost()->GetMetaData().RemoveValue(BP, VersionKey);
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 적용 실패. 백업=%s"), LogPrefix, *Backup); return;
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 시설 패널·목록 행 저장 완료"), LogPrefix);
	}
	FAutoConsoleCommand Command(TEXT("FF.ApplyLiveFacilitiesSkyDesign"),
		TEXT("P3 시설 요약과 목록 표를 최초 한 번 제작한다."), FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
