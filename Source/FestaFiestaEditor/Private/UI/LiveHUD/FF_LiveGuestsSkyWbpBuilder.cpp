// P4 관객 화면: 인원 요약과 불편 현황을 분리하고 저장된 Designer 편집을 유지한다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"

#include "Components/Border.h"
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
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/LiveHUD/Panels/FF_LiveNeedRowWidget.h"
#include "UI/LiveHUD/Panels/FF_LivePanelGuests.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLiveGuestsSky"

namespace FFLiveGuestsSkyBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* LogPrefix = TEXT("FF_LIVE_GUESTS_SKY");
	constexpr const TCHAR* VersionKey = TEXT("FF.LiveGuestsSky.Version");
	const FString PanelPackage = TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelGuests");
	const FString RowFolder = TEXT("/Game/FestaFiesta/UI/LiveHUD/Rows");
	const FString RowPackage = RowFolder / TEXT("WBP_FF_LiveNeedRow");
	const FString Icons = TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons");

	UTextBlock* Label(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		const FText& Value, int32 Size = 17, FLinearColor Tint = Ink, bool bVariable = false)
	{
		UTextBlock* Result = Text(BP, Tree, Name, Value, Size, Tint, bVariable);
		Result->SetAutoWrapText(true);
		return Result;
	}

	void Cell(UWidgetBlueprint& BP, UWidgetTree& Tree, UHorizontalBox& Parent,
		const TCHAR* Name, UWidget& Content, float Width, FMargin Padding = FMargin(0.f))
	{
		USizeBox* Box = MakeWidget<USizeBox>(BP, Tree, Name);
		if (Width > 0.f) Box->SetWidthOverride(Width);
		Box->AddChild(&Content);
		Column(Parent, *Box, Padding, Width <= 0.f);
		CastChecked<UHorizontalBoxSlot>(Box->Slot)->SetVerticalAlignment(VAlign_Center);
	}

	USizeBox* Icon(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		const TCHAR* Path, float Size, bool bVariable = false)
	{
		USizeBox* Box = MakeWidget<USizeBox>(BP, Tree, FName(FString(Name) + TEXT("_Size")));
		Box->SetWidthOverride(Size); Box->SetHeightOverride(Size);
		UImage* Image = MakeWidget<UImage>(BP, Tree, Name, bVariable);
		Image->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr, *MakeObjectPath(Icons / Path)));
		Image->SetColorAndOpacity(Ink); Box->AddChild(Image);
		return Box;
	}

	bool BuildRow(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		USizeBox* Size = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_Row"));
		Size->SetMinDesiredHeight(64.f); Tree.RootWidget = Size;
		UBorder* Frame = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Row"));
		Frame->SetBrush(Brush(Paper, 5.f, 1.f, SkyLine)); Frame->SetPadding(FMargin(12.f, 10.f)); Size->AddChild(Frame);
		UVerticalBox* Body = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Row")); Frame->AddChild(Body);
		UHorizontalBox* Values = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Values")); Row(*Body, *Values);
		USizeBox* NeedIcon = Icon(BP, Tree, TEXT("Image_NeedIcon"), TEXT("Needs/T_FF_LH_Icon_NeedRestroom_White_256"), 26.f, true);
		Column(*Values, *NeedIcon, FMargin(0.f, 0.f, 10.f, 0.f));
		CastChecked<UHorizontalBoxSlot>(NeedIcon->Slot)->SetVerticalAlignment(VAlign_Center);
		Cell(BP, Tree, *Values, TEXT("SizeBox_Name"),
			*Label(BP, Tree, TEXT("Text_Name"), LOCTEXT("NeedName", "화장실"), 18, Ink, true), 108.f, FMargin(0.f, 0.f, 12.f, 0.f));
		UProgressBar* Progress = MakeWidget<UProgressBar>(BP, Tree, TEXT("Progress_Unmet"), true);
		FProgressBarStyle ProgressStyle = Progress->GetWidgetStyle();
		ProgressStyle.BackgroundImage = Brush(SkyWash, 4.f);
		ProgressStyle.FillImage = Brush(FLinearColor::White, 4.f);
		Progress->SetWidgetStyle(ProgressStyle); Progress->SetBarFillStyle(EProgressBarFillStyle::Scale);
		Progress->SetFillColorAndOpacity(Color(124, 206, 179)); Progress->SetPercent(0.f);
		USizeBox* ProgressSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_Progress"));
		ProgressSize->SetHeightOverride(16.f); ProgressSize->AddChild(Progress);
		Column(*Values, *ProgressSize, FMargin(0.f, 0.f, 12.f, 0.f), true);
		CastChecked<UHorizontalBoxSlot>(ProgressSize->Slot)->SetVerticalAlignment(VAlign_Center);
		UTextBlock* Percent = Label(BP, Tree, TEXT("Text_Percent"), LOCTEXT("Unknown", "—"), 18, Ink, true);
		Percent->SetJustification(ETextJustify::Right);
		Cell(BP, Tree, *Values, TEXT("SizeBox_Percent"), *Percent, 64.f, FMargin(0.f, 0.f, 12.f, 0.f));
		UBorder* Status = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Status"), true);
		Status->SetBrush(Brush(FLinearColor::White, 5.f)); Status->SetPadding(FMargin(6.f, 4.f));
		UTextBlock* StatusText = Label(BP, Tree, TEXT("Text_Status"), LOCTEXT("Waiting", "집계 준비 중"), 13, Ink, true);
		StatusText->SetJustification(ETextJustify::Center); Status->AddChild(StatusText);
		Cell(BP, Tree, *Values, TEXT("SizeBox_Status"), *Status, 90.f);
		UTextBlock* Remedy = Label(BP, Tree, TEXT("Text_Remedy"), FText::GetEmpty(), 14, Muted, true);
		Remedy->SetVisibility(ESlateVisibility::Collapsed);
		Row(*Body, *Remedy, FMargin(36.f, 6.f, 0.f, 0.f));
		return true;
	}

	void Card(UWidgetBlueprint& BP, UWidgetTree& Tree, UVerticalBox& Parent, const TCHAR* Key,
		const FText& Caption, UTextBlock& Value, UTextBlock& Note, bool bLast = false)
	{
		const FString Prefix = FString(TEXT("Guests_")) + Key;
		UBorder* Frame = MakeWidget<UBorder>(BP, Tree, FName(Prefix + TEXT("_Border")));
		Frame->SetBrush(Brush(SkyWash, 7.f, 1.f, SkyLine)); Frame->SetPadding(FMargin(22.f, 20.f));
		Row(Parent, *Frame, FMargin(0.f, 0.f, 0.f, bLast ? 0.f : 16.f), true);
		UVerticalBox* Body = MakeWidget<UVerticalBox>(BP, Tree, FName(Prefix + TEXT("_Body"))); Frame->AddChild(Body);
		Row(*Body, *Label(BP, Tree, *(Prefix + TEXT("_Caption")), Caption, 21), FMargin(0.f, 0.f, 0.f, 22.f));
		UHorizontalBox* Numbers = MakeWidget<UHorizontalBox>(BP, Tree, FName(Prefix + TEXT("_Numbers")));
		USizeBox* PeopleIcon = Icon(BP, Tree, *(Prefix + TEXT("_Icon")), TEXT("Metrics/T_FF_LH_Icon_Visitors_White_256"), 38.f);
		Column(*Numbers, *PeopleIcon, FMargin(0.f, 0.f, 14.f, 0.f));
		CastChecked<UHorizontalBoxSlot>(PeopleIcon->Slot)->SetVerticalAlignment(VAlign_Center);
		StyleText(Value, 36, Ink, TEXT("SemiBold")); Value.SetAutoWrapText(true); Value.SetText(LOCTEXT("MissingCount", "—"));
		Column(*Numbers, Value, FMargin(0.f), true); Row(*Body, *Numbers, FMargin(0.f, 0.f, 0.f, 18.f));
		StyleText(Note, 16, Muted, TEXT("Medium")); Note.SetAutoWrapText(true); Row(*Body, Note);
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
		UTextBlock* Visitors = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_VisitorCount")));
		UTextBlock* VisitorNote = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_VisitorNote")));
		UTextBlock* Summary = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Summary")));
		UTextBlock* Empty = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Empty")));
		UTextBlock* Note = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_MissingNote")));
		UVerticalBox* Rows = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Box_Needs")));
		if (!Content || !Sections || !OldScroll || !Header || !HeaderSize || !Close || !Title || !Visitors || !VisitorNote || !Summary || !Empty || !Note || !Rows) return false;
		BP.Modify(); Tree.Modify();
		for (UWidget* Value : TArray<UWidget*>{Visitors, VisitorNote, Summary, Empty, Note, Rows}) Value->RemoveFromParent();
		const TArray<UWidget*> OldChildren = Content->GetAllChildren(); Content->ClearChildren();
		UVerticalBox* Legacy = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Guests_LegacyDecorations"));
		Legacy->SetVisibility(ESlateVisibility::Collapsed);
		for (UWidget* Old : OldChildren) Legacy->AddChild(Old);
		Row(*Content, *Legacy);
		Content->RemoveFromParent(); OldScroll->SetVisibility(ESlateVisibility::Collapsed);
		Row(*Sections, *Content, FMargin(22.f, 20.f), true);

		Header->ClearChildren(); HeaderSize->SetHeightOverride(78.f);
		USizeBox* HeaderIcon = Icon(BP, Tree, TEXT("Guests_HeaderIcon"), TEXT("Navigation/T_FF_LH_Icon_Guests_White_256"), 32.f);
		Column(*Header, *HeaderIcon, FMargin(0.f, 0.f, 14.f, 0.f));
		CastChecked<UHorizontalBoxSlot>(HeaderIcon->Slot)->SetVerticalAlignment(VAlign_Center);
		UVerticalBox* Heading = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Guests_Heading")); Column(*Header, *Heading, FMargin(0.f), true);
		CastChecked<UHorizontalBoxSlot>(Heading->Slot)->SetVerticalAlignment(VAlign_Center);
		StyleText(*Title, 26, Ink, TEXT("SemiBold")); Row(*Heading, *Title);
		Row(*Heading, *Label(BP, Tree, TEXT("Guests_Subtitle"), LOCTEXT("Subtitle", "현재 관객과 불편 사항을 확인하세요."), 15, Muted), FMargin(0.f, 2.f, 0.f, 0.f));
		Column(*Header, *Close); CastChecked<UHorizontalBoxSlot>(Close->Slot)->SetVerticalAlignment(VAlign_Center);

		UHorizontalBox* Columns = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Guests_Columns")); Row(*Content, *Columns, FMargin(0.f), true);
		USizeBox* CountsSize = MakeWidget<USizeBox>(BP, Tree, TEXT("Guests_SizeBox_Counts")); CountsSize->SetWidthOverride(330.f);
		UVerticalBox* Counts = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Guests_Counts")); CountsSize->AddChild(Counts);
		Column(*Columns, *CountsSize, FMargin(0.f, 0.f, 18.f, 0.f));
		VisitorNote->SetText(LOCTEXT("VisitorNote", "지금 축제장 안에 있는 인원입니다."));
		Card(BP, Tree, *Counts, TEXT("Current"), LOCTEXT("Current", "현재 관객"), *Visitors, *VisitorNote);
		Card(BP, Tree, *Counts, TEXT("Admitted"), LOCTEXT("Admitted", "오늘 누적 입장객"),
			*Label(BP, Tree, TEXT("Text_AdmittedCount"), FText::GetEmpty(), 36, Ink, true),
			*Label(BP, Tree, TEXT("Text_AdmittedNote"), LOCTEXT("AdmittedNote", "오늘 입장한 누적 인원입니다."), 16, Muted, true), true);
		Column(*Columns, *Rule(BP, Tree, TEXT("Guests_Divider"), true), FMargin(0.f, 0.f, 22.f, 0.f));
		UVerticalBox* Needs = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Guests_Needs")); Column(*Columns, *Needs, FMargin(0.f), true);
		Row(*Needs, *Label(BP, Tree, TEXT("Guests_NeedsTitle"), LOCTEXT("NeedsTitle", "관객 불편 현황"), 22), FMargin(0.f, 0.f, 0.f, 6.f));
		Row(*Needs, *Label(BP, Tree, TEXT("Text_SampleCount"), LOCTEXT("Sample", "집계 준비 중"), 15, Muted, true), FMargin(0.f, 0.f, 0.f, 12.f));
		UScrollBox* Scroll = MakeWidget<UScrollBox>(BP, Tree, TEXT("Scroll_Needs"));
		Scroll->SetAllowOverscroll(false); Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
		Scroll->SetScrollbarThickness(FVector2D(5.f, 5.f)); Scroll->SetClipping(EWidgetClipping::ClipToBounds);
		UVerticalBox* List = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Guests_ListContent"));
		CastChecked<UScrollBoxSlot>(Scroll->AddChild(List))->SetHorizontalAlignment(HAlign_Fill);
		Row(*List, *Rows); StyleText(*Empty, 17, Muted, TEXT("Medium")); Empty->SetAutoWrapText(true);
		Empty->SetText(LOCTEXT("Empty", "관객 불편 정보를 확인하고 있습니다.")); Row(*List, *Empty, FMargin(12.f, 20.f));
		Row(*Needs, *Scroll, FMargin(0.f, 0.f, 0.f, 12.f), true);
		UBorder* SummaryFrame = MakeWidget<UBorder>(BP, Tree, TEXT("Guests_SummaryBorder"));
		SummaryFrame->SetBrush(Brush(SkyWash, 6.f, 1.f, SkyLine)); SummaryFrame->SetPadding(FMargin(16.f, 12.f));
		StyleText(*Summary, 18, Ink, TEXT("SemiBold")); Summary->SetAutoWrapText(true);
		Summary->SetText(LOCTEXT("Summary", "관객 불편 정보를 확인하고 있습니다.")); SummaryFrame->AddChild(Summary);
		Row(*Needs, *SummaryFrame, FMargin(0.f, 0.f, 0.f, 10.f));
		StyleText(*Note, 14, Muted, TEXT("Medium")); Note->SetAutoWrapText(true);
		Note->SetText(LOCTEXT("Note", "각 항목은 집계된 관객 중 불편을 겪는 비율입니다.")); Row(*Needs, *Note);
		BP.GetOutermost()->GetMetaData().SetValue(&BP, VersionKey, TEXT("1"));
		return true;
	}

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld) return;
		UWidgetBlueprint* BP = LoadExistingBlueprint(PanelPackage);
		if (!BP || !BP->WidgetTree || !BP->ParentClass->IsChildOf(UFF_LivePanelGuests::StaticClass())) return;
		if (BP->GetOutermost()->GetMetaData().GetValue(BP, VersionKey) == TEXT("1"))
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: Designer 편집 보존"), LogPrefix); return;
		}
		const FString File = FPackageName::LongPackageNameToFilename(PanelPackage, TEXT(".uasset"));
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/LiveGuestsSky") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
		if (!IFileManager::Get().MakeDirectory(*Backup, true) || IFileManager::Get().Copy(*(Backup / FPaths::GetCleanFilename(File)), *File) != COPY_OK) return;
		UWidgetBlueprint* RowBP = EnsureWidgetBlueprint(LogPrefix, RowPackage, RowFolder, *UFF_LiveNeedRowWidget::StaticClass(), BuildRow);
		if (!RowBP || !RowBP->GeneratedClass || !RowBP->GeneratedClass->IsChildOf(UFF_LiveNeedRowWidget::StaticClass())) return;
		if (!BuildPanel(*BP) || !SaveNewBlueprint(LogPrefix, *BP))
		{
			BP->GetOutermost()->GetMetaData().RemoveValue(BP, VersionKey);
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 적용 실패. 백업=%s"), LogPrefix, *Backup); return;
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 관객 패널·불편 항목 행 저장 완료"), LogPrefix);
	}
	FAutoConsoleCommand Command(TEXT("FF.ApplyLiveGuestsSkyDesign"),
		TEXT("P4 관객 요약과 불편 비율 목록을 최초 한 번 제작한다."), FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
