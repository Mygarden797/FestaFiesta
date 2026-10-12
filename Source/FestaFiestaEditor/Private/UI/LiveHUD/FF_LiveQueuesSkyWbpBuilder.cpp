// P5 대기열 화면과 편집 가능한 목록 행 제작. 기존 조회·시간·입력 규칙은 유지한다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"

#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
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
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/LiveHUD/Panels/FF_LiveQueueRowWidget.h"
#include "UI/LiveHUD/Panels/FF_LivePanelQueues.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLiveQueuesSky"

namespace FFLiveQueuesSkyBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* LogPrefix = TEXT("FF_LIVE_QUEUES_SKY");
	constexpr const TCHAR* VersionKey = TEXT("FF.LiveQueuesSky.Version");
	const FString PanelPackage = TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelQueues");
	const FString RowFolder = TEXT("/Game/FestaFiesta/UI/LiveHUD/Rows");
	const FString RowPackage = RowFolder / TEXT("WBP_FF_LiveQueueRow");
	const FString Icons = TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons");

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
		Size->SetMinDesiredHeight(56.f); Tree.RootWidget = Size;
		UBorder* Frame = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Row"));
		Frame->SetBrush(Brush(Paper, 5.f, 1.f, SkyLine)); Frame->SetPadding(FMargin(12.f, 10.f)); Size->AddChild(Frame);
		UHorizontalBox* Columns = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Columns")); Frame->AddChild(Columns);
		Cell(BP, Tree, *Columns, TEXT("SizeBox_Name"), *Label(BP, Tree, TEXT("Text_Name"), LOCTEXT("Name", "시설명"), 18, Ink, true), 0.f);
		const TCHAR* Names[] = { TEXT("Text_Waiting"), TEXT("Text_InService"), TEXT("Text_ExpectedWait") };
		const TCHAR* Sizes[] = { TEXT("SizeBox_Waiting"), TEXT("SizeBox_InService"), TEXT("SizeBox_ExpectedWait") };
		for (int32 Index = 0; Index < 3; ++Index)
		{
			UTextBlock* Value = Label(BP, Tree, Names[Index], LOCTEXT("Missing", "—"), 18, Ink, true);
			Value->SetJustification(ETextJustify::Center);
			Cell(BP, Tree, *Columns, Sizes[Index], *Value, Index == 2 ? 145.f : 100.f);
		}
		UBorder* Badge = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Status"), true);
		Badge->SetBrush(Brush(FLinearColor::White, 5.f)); Badge->SetPadding(FMargin(8.f, 5.f));
		UHorizontalBox* Status = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Status")); Badge->AddChild(Status);
		USizeBox* IconSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_StatusIcon"));
		IconSize->SetWidthOverride(20.f); IconSize->SetHeightOverride(20.f);
		UImage* Icon = MakeWidget<UImage>(BP, Tree, TEXT("Image_Status"), true);
		Icon->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr, *MakeObjectPath(Icons / TEXT("Status/T_FF_LH_Icon_StatusPending_White_256"))));
		Icon->SetColorAndOpacity(Ink); IconSize->AddChild(Icon);
		Column(*Status, *IconSize, FMargin(0.f, 0.f, 8.f, 0.f));
		CastChecked<UHorizontalBoxSlot>(IconSize->Slot)->SetVerticalAlignment(VAlign_Center);
		UTextBlock* StatusText = Label(BP, Tree, TEXT("Text_Status"), LOCTEXT("StatusUnknown", "미확인"), 16, Ink, true);
		Column(*Status, *StatusText, FMargin(0.f), true);
		CastChecked<UHorizontalBoxSlot>(StatusText->Slot)->SetVerticalAlignment(VAlign_Center);
		Cell(BP, Tree, *Columns, TEXT("SizeBox_Status"), *Badge, 180.f, true);
		return true;
	}

	void Metric(UWidgetBlueprint& BP, UWidgetTree& Tree, UWrapBox& Parent,
		const TCHAR* Key, const FText& Caption, UTextBlock& Value, UTextBlock* Note = nullptr)
	{
		const FString Prefix = FString(TEXT("Queue_")) + Key;
		USizeBox* Size = MakeWidget<USizeBox>(BP, Tree, FName(Prefix + TEXT("_Size")));
		Size->SetWidthOverride(336.f); Size->SetMinDesiredHeight(132.f);
		UBorder* Frame = MakeWidget<UBorder>(BP, Tree, FName(Prefix + TEXT("_Border")));
		Frame->SetBrush(Brush(SkyWash, 7.f, 1.f, SkyLine)); Frame->SetPadding(FMargin(20.f, 14.f)); Size->AddChild(Frame);
		UVerticalBox* Body = MakeWidget<UVerticalBox>(BP, Tree, FName(Prefix + TEXT("_Body"))); Frame->AddChild(Body);
		Row(*Body, *Label(BP, Tree, *(Prefix + TEXT("_Caption")), Caption, 16, Muted), FMargin(0.f, 0.f, 0.f, 6.f));
		StyleText(Value, 30, Ink, TEXT("SemiBold")); Value.SetText(LOCTEXT("MetricMissing", "—")); Value.SetAutoWrapText(true);
		Row(*Body, Value); Parent.AddChild(Size);
		if (Note)
		{
			StyleText(*Note, 13, Muted, TEXT("Medium")); Note->SetAutoWrapText(true);
			Note->SetText(LOCTEXT("AverageNote", "완료 기록이 있는 시설별 평균을 사용합니다."));
			Row(*Body, *Note, FMargin(0.f, 5.f, 0.f, 0.f));
		}
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
		UTextBlock* Waiting = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_TotalWaiting")));
		UTextBlock* InService = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_InService")));
		UTextBlock* Average = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_AverageWait")));
		UTextBlock* AverageNote = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_AverageNote")));
		UTextBlock* Longest = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Longest")));
		UTextBlock* Worst = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_WorstWait")));
		UTextBlock* StateCounts = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_StateCounts")));
		UTextBlock* Empty = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Empty")));
		UTextBlock* Note = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_MissingNote")));
		UVerticalBox* Rows = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Box_Queues")));
		if (!Content || !Sections || !OldScroll || !Header || !HeaderSize || !Close || !Title || !Waiting || !InService || !Average || !AverageNote || !Longest || !Worst || !StateCounts || !Empty || !Note || !Rows) return false;
		BP.Modify(); Tree.Modify();
		for (UWidget* Value : TArray<UWidget*>{Waiting, InService, Average, AverageNote, Longest, Worst, StateCounts, Empty, Note, Rows}) Value->RemoveFromParent();
		const TArray<UWidget*> OldChildren = Content->GetAllChildren(); Content->ClearChildren();
		UVerticalBox* Legacy = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Queue_LegacyDecorations"));
		Legacy->SetVisibility(ESlateVisibility::Collapsed);
		for (UWidget* Old : OldChildren) Legacy->AddChild(Old);
		Row(*Content, *Legacy);
		// 공통 외부 스크롤은 접어 두고, 시설별 대기열 목록 하나만 스크롤한다.
		Content->RemoveFromParent(); OldScroll->SetVisibility(ESlateVisibility::Collapsed);
		Row(*Sections, *Content, FMargin(22.f, 16.f), true);
		Header->ClearChildren(); HeaderSize->SetHeightOverride(78.f);
		UVerticalBox* Heading = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Queue_Heading"));
		Column(*Header, *Heading, FMargin(0.f), true);
		CastChecked<UHorizontalBoxSlot>(Heading->Slot)->SetVerticalAlignment(VAlign_Center);
		StyleText(*Title, 26, Ink, TEXT("SemiBold")); Row(*Heading, *Title);
		Row(*Heading, *Label(BP, Tree, TEXT("Queue_Subtitle"), LOCTEXT("Subtitle", "시설별 대기 흐름과 예상 대기 시간을 확인하세요."), 15, Muted), FMargin(0.f, 2.f, 0.f, 0.f));
		Column(*Header, *Close); CastChecked<UHorizontalBoxSlot>(Close->Slot)->SetVerticalAlignment(VAlign_Center);

		UWrapBox* Metrics = MakeWidget<UWrapBox>(BP, Tree, TEXT("Queue_Metrics")); Metrics->SetInnerSlotPadding(FVector2D(12.f, 12.f));
		Row(*Content, *Metrics, FMargin(0.f, 0.f, 0.f, 12.f));
		Metric(BP, Tree, *Metrics, TEXT("Total"), LOCTEXT("Total", "총 대기"), *Waiting);
		Metric(BP, Tree, *Metrics, TEXT("InService"), LOCTEXT("InService", "이용 중"), *InService);
		Metric(BP, Tree, *Metrics, TEXT("Average"), LOCTEXT("Average", "완료 고객 평균 대기"), *Average, AverageNote);
		UBorder* Highlight = MakeWidget<UBorder>(BP, Tree, TEXT("Queue_HighlightBorder"));
		Highlight->SetBrush(Brush(SkyWash, 6.f, 1.f, SkyLine)); Highlight->SetPadding(FMargin(16.f, 10.f));
		UVerticalBox* Highlights = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Queue_Highlights")); Highlight->AddChild(Highlights);
		StyleText(*Longest, 19, Ink, TEXT("SemiBold")); Longest->SetAutoWrapText(true);
		Longest->SetText(LOCTEXT("Longest", "대기열 정보를 확인하고 있습니다.")); Row(*Highlights, *Longest);
		StyleText(*Worst, 15, Muted, TEXT("Medium")); Worst->SetAutoWrapText(true);
		Worst->SetText(LOCTEXT("Worst", "예상 대기는 현재 줄에 섰을 때의 대기 시간입니다."));
		Row(*Highlights, *Worst, FMargin(0.f, 5.f, 0.f, 0.f));
		Row(*Content, *Highlight, FMargin(0.f, 0.f, 0.f, 12.f));
		UBorder* TableHeading = MakeWidget<UBorder>(BP, Tree, TEXT("Queue_TableHeading"));
		TableHeading->SetBrush(Brush(SkyWash, 5.f, 1.f, SkyLine)); TableHeading->SetPadding(FMargin(12.f, 10.f));
		UHorizontalBox* Columns = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Queue_HeaderColumns")); TableHeading->AddChild(Columns);
		Cell(BP, Tree, *Columns, TEXT("Queue_NameColumn"), *Label(BP, Tree, TEXT("Queue_NameCaption"), LOCTEXT("NameCaption", "시설명")), 0.f);
		const FText Captions[] = { LOCTEXT("Waiting", "대기"), LOCTEXT("Service", "이용 중"), LOCTEXT("Expected", "지금 서면") };
		const TCHAR* Keys[] = { TEXT("Queue_Waiting"), TEXT("Queue_InService"), TEXT("Queue_ExpectedWait") };
		for (int32 Index = 0; Index < 3; ++Index)
		{
			UTextBlock* Caption = Label(BP, Tree, *(FString(Keys[Index]) + TEXT("Caption")), Captions[Index]); Caption->SetJustification(ETextJustify::Center);
			Cell(BP, Tree, *Columns, Keys[Index], *Caption, Index == 2 ? 145.f : 100.f);
		}
		Cell(BP, Tree, *Columns, TEXT("Queue_StatusColumn"), *Label(BP, Tree, TEXT("Queue_StatusCaption"), LOCTEXT("StatusCaption", "접수 상태")), 180.f, true);
		Row(*Content, *TableHeading, FMargin(0.f, 0.f, 0.f, 6.f));
		UScrollBox* Scroll = MakeWidget<UScrollBox>(BP, Tree, TEXT("Scroll_Queues"));
		Scroll->SetAllowOverscroll(false); Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
		Scroll->SetScrollbarThickness(FVector2D(5.f, 5.f)); Scroll->SetClipping(EWidgetClipping::ClipToBounds);
		UVerticalBox* List = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Queue_ListContent"));
		CastChecked<UScrollBoxSlot>(Scroll->AddChild(List))->SetHorizontalAlignment(HAlign_Fill);
		Row(*List, *Rows);
		StyleText(*Empty, 18, Muted, TEXT("Medium")); Empty->SetAutoWrapText(true);
		Empty->SetText(LOCTEXT("Empty", "줄을 세울 시설이 없습니다.")); Row(*List, *Empty, FMargin(12.f, 24.f));
		Row(*Content, *Scroll, FMargin(0.f, 0.f, 0.f, 12.f), true);
		StyleText(*StateCounts, 15, Ink, TEXT("Medium")); StateCounts->SetAutoWrapText(true);
		StateCounts->SetText(LOCTEXT("States", "접수 상태를 확인하고 있습니다."));
		Row(*Content, *StateCounts, FMargin(0.f, 0.f, 0.f, 6.f));
		StyleText(*Note, 13, Muted, TEXT("Medium")); Note->SetAutoWrapText(true);
		Note->SetText(LOCTEXT("Note", "대기 팀이 많은 순으로 표시합니다. 시설별 예상 대기는 지금 줄을 서면 기다릴 시간입니다.")); Row(*Content, *Note);
		BP.GetOutermost()->GetMetaData().SetValue(&BP, VersionKey, TEXT("1"));
		return true;
	}

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld) return;
		UWidgetBlueprint* BP = LoadExistingBlueprint(PanelPackage);
		if (!BP || !BP->WidgetTree || !BP->ParentClass->IsChildOf(UFF_LivePanelQueues::StaticClass())) return;
		if (BP->GetOutermost()->GetMetaData().GetValue(BP, VersionKey) == TEXT("1"))
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: Designer 편집 보존"), LogPrefix); return;
		}
		const FString File = FPackageName::LongPackageNameToFilename(PanelPackage, TEXT(".uasset"));
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/LiveQueuesSky") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
		if (!IFileManager::Get().MakeDirectory(*Backup, true) || IFileManager::Get().Copy(*(Backup / FPaths::GetCleanFilename(File)), *File) != COPY_OK) return;
		UWidgetBlueprint* RowBP = EnsureWidgetBlueprint(LogPrefix, RowPackage, RowFolder, *UFF_LiveQueueRowWidget::StaticClass(), BuildRow);
		if (!RowBP || !RowBP->GeneratedClass || !RowBP->GeneratedClass->IsChildOf(UFF_LiveQueueRowWidget::StaticClass())) return;
		if (!BuildPanel(*BP) || !SaveNewBlueprint(LogPrefix, *BP))
		{
			BP->GetOutermost()->GetMetaData().RemoveValue(BP, VersionKey);
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 적용 실패. 백업=%s"), LogPrefix, *Backup); return;
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 대기열 패널·목록 행 저장 완료"), LogPrefix);
	}
	FAutoConsoleCommand Command(TEXT("FF.ApplyLiveQueuesSkyDesign"),
		TEXT("P5 대기열 요약과 목록 표를 최초 한 번 제작한다."), FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
