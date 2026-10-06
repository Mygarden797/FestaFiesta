// P7 스태프: 요약·정원 구성·사건 대응 읽기 전용 화면. 저장 후 외형은 Designer에서 편집한다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
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
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/LiveHUD/Panels/FF_LivePanelStaff.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLiveStaffSky"

namespace FFLiveStaffSkyBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* LogPrefix = TEXT("FF_LIVE_STAFF_SKY");
	constexpr const TCHAR* VersionKey = TEXT("FF.LiveStaffSky.Version");
	const FString PanelPackage = TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelStaff");
	const FString Icons = TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons");

	UTextBlock* Label(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		const FText& Value, int32 Size = 17, FLinearColor Tint = Ink, bool bVariable = false)
	{
		UTextBlock* Result = Text(BP, Tree, Name, Value, Size, Tint, bVariable);
		Result->SetAutoWrapText(true); return Result;
	}

	void StyleValue(UTextBlock& Value, int32 Size, FLinearColor Tint = Ink)
	{
		StyleText(Value, Size, Tint, Size >= 20 ? TEXT("SemiBold") : TEXT("Medium"));
		Value.SetAutoWrapText(true); Value.SetText(LOCTEXT("Missing", "—"));
	}

	void CenterColumn(UHorizontalBox& Parent, UWidget& Child, FMargin Padding = FMargin(0.f), bool bFill = false)
	{
		Column(Parent, Child, Padding, bFill);
		CastChecked<UHorizontalBoxSlot>(Child.Slot)->SetVerticalAlignment(VAlign_Center);
	}

	void Metric(UWidgetBlueprint& BP, UWidgetTree& Tree, UWrapBox& Parent,
		const TCHAR* Key, const FText& Caption, UTextBlock& Value, UTextBlock* Note = nullptr)
	{
		const FString Prefix = FString(TEXT("Staff_")) + Key;
		USizeBox* Size = MakeWidget<USizeBox>(BP, Tree, FName(Prefix + TEXT("_Size")));
		Size->SetWidthOverride(320.f); Size->SetMinDesiredHeight(128.f);
		UBorder* Frame = MakeWidget<UBorder>(BP, Tree, FName(Prefix + TEXT("_Border")));
		Frame->SetBrush(Brush(SkyWash, 7.f, 1.f, SkyLine)); Frame->SetPadding(FMargin(18.f, 14.f)); Size->AddChild(Frame);
		UVerticalBox* Body = MakeWidget<UVerticalBox>(BP, Tree, FName(Prefix + TEXT("_Body"))); Frame->AddChild(Body);
		Row(*Body, *Label(BP, Tree, *(Prefix + TEXT("_Caption")), Caption, 16), FMargin(0.f, 0.f, 0.f, 8.f));
		StyleValue(Value, 30); Row(*Body, Value); Parent.AddChild(Size);
		if (Note) { StyleValue(*Note, 15, Muted); Row(*Body, *Note, FMargin(0.f, 6.f, 0.f, 0.f)); }
	}

	void Detail(UWidgetBlueprint& BP, UWidgetTree& Tree, UVerticalBox& Parent,
		const TCHAR* Key, const FText& Caption, const TCHAR* ValueName)
	{
		UHorizontalBox* Line = MakeWidget<UHorizontalBox>(BP, Tree, Key);
		CenterColumn(*Line, *Label(BP, Tree, *(FString(Key) + TEXT("_Caption")), Caption), FMargin(0.f, 0.f, 12.f, 0.f), true);
		UTextBlock* Value = Label(BP, Tree, ValueName, LOCTEXT("DetailMissing", "—"), 20, Ink, true);
		Value->SetJustification(ETextJustify::Right); CenterColumn(*Line, *Value);
		Row(Parent, *Line, FMargin(0.f, 0.f, 0.f, 8.f));
	}

	UProgressBar* CapacityBar(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		FLinearColor Fill, bool bBackground)
	{
		UProgressBar* Progress = MakeWidget<UProgressBar>(BP, Tree, Name, true);
		FProgressBarStyle Style = Progress->GetWidgetStyle();
		Style.BackgroundImage = Brush(bBackground ? Color(225, 238, 243) : FLinearColor::Transparent, 4.f);
		Style.FillImage = Brush(Fill, 4.f); Progress->SetWidgetStyle(Style);
		Progress->SetFillColorAndOpacity(FLinearColor::White);
		Progress->SetBarFillType(EProgressBarFillType::LeftToRight);
		Progress->SetBarFillStyle(EProgressBarFillStyle::Scale); Progress->SetPercent(0.f);
		Progress->SetVisibility(ESlateVisibility::HitTestInvisible); return Progress;
	}

	bool BuildPanel(UWidgetBlueprint& BP, UTexture2D& HeaderIcon)
	{
		UWidgetTree& Tree = *BP.WidgetTree;
		UVerticalBox* Content = Cast<UVerticalBox>(Tree.FindWidget(TEXT("VerticalBox_Content")));
		UVerticalBox* Sections = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Sky_VerticalBox_Sections")));
		UScrollBox* OldScroll = Cast<UScrollBox>(Tree.FindWidget(TEXT("Sky_ScrollBox_Body")));
		UHorizontalBox* Header = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("HorizontalBox_Header")));
		USizeBox* HeaderSize = Cast<USizeBox>(Tree.FindWidget(TEXT("Sky_SizeBox_Header")));
		UWidget* Close = Tree.FindWidget(TEXT("Sky_SizeBox_Close"));
		UTextBlock* Title = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_PanelTitle")));
		UTextBlock* Total = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_TotalStaff")));
		UTextBlock* Breakdown = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_StaffBreakdown")));
		UTextBlock* Assigned = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_AssignedStaff")));
		UTextBlock* Idle = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_IdleStaff")));
		UTextBlock* IdleNote = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_IdleNote")));
		UTextBlock* Empty = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_AssignmentsEmpty")));
		UTextBlock* Wage = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_DailyWage")));
		UTextBlock* HireNote = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_HireNote")));
		UTextBlock* Missing = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_MissingNote")));
		UVerticalBox* Assignments = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Box_Assignments")));
		if (!Content || !Sections || !OldScroll || !Header || !HeaderSize || !Close || !Title || !Total || !Breakdown || !Assigned || !Idle || !IdleNote || !Empty || !Wage || !HireNote || !Missing || !Assignments) return false;
		BP.Modify(); Tree.Modify();
		for (UWidget* Value : TArray<UWidget*>{Total, Breakdown, Assigned, Idle, IdleNote, Empty, Wage, HireNote, Missing, Assignments}) Value->RemoveFromParent();
		const TArray<UWidget*> OldChildren = Content->GetAllChildren(); Content->ClearChildren();
		UVerticalBox* Legacy = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Staff_LegacyDecorations")); Legacy->SetVisibility(ESlateVisibility::Collapsed);
		for (UWidget* Old : OldChildren) Legacy->AddChild(Old);
		Row(*Content, *Legacy);
		Content->RemoveFromParent(); OldScroll->SetVisibility(ESlateVisibility::Collapsed);
		Row(*Sections, *Content, FMargin(22.f, 16.f), true);
		Header->ClearChildren(); HeaderSize->SetHeightOverride(80.f);
		USizeBox* IconSize = MakeWidget<USizeBox>(BP, Tree, TEXT("Staff_SizeBox_TitleIcon")); IconSize->SetWidthOverride(32.f); IconSize->SetHeightOverride(32.f);
		UImage* Icon = MakeWidget<UImage>(BP, Tree, TEXT("Staff_Image_Title")); Icon->SetBrushFromTexture(&HeaderIcon); Icon->SetColorAndOpacity(Ink); IconSize->AddChild(Icon);
		CenterColumn(*Header, *IconSize, FMargin(0.f, 0.f, 14.f, 0.f));
		UVerticalBox* Heading = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Staff_Heading")); CenterColumn(*Header, *Heading, FMargin(0.f), true);
		StyleText(*Title, 26, Ink, TEXT("SemiBold")); Row(*Heading, *Title);
		Row(*Heading, *Label(BP, Tree, TEXT("Staff_Subtitle"), LOCTEXT("Subtitle", "현장 인원과 사건 대응 현황"), 15, Muted), FMargin(0.f, 3.f, 0.f, 0.f)); CenterColumn(*Header, *Close);
		UWrapBox* Metrics = MakeWidget<UWrapBox>(BP, Tree, TEXT("Staff_Metrics")); Metrics->SetInnerSlotPadding(FVector2D(12.f, 12.f));
		Row(*Content, *Metrics, FMargin(0.f, 0.f, 0.f, 14.f));
		Metric(BP, Tree, *Metrics, TEXT("Total"), LOCTEXT("Total", "전체 인원"), *Total, Breakdown);
		Metric(BP, Tree, *Metrics, TEXT("Busy"), LOCTEXT("Busy", "사건 대응 중"), *Assigned);
		Metric(BP, Tree, *Metrics, TEXT("Idle"), LOCTEXT("Idle", "대응 중이 아닌 인원"), *Idle);

		UScrollBox* Scroll = MakeWidget<UScrollBox>(BP, Tree, TEXT("Staff_Scroll_Details"));
		Scroll->SetAllowOverscroll(false); Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
		Scroll->SetScrollbarThickness(FVector2D(5.f, 5.f)); Scroll->SetClipping(EWidgetClipping::ClipToBounds);
		UHorizontalBox* Columns = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Staff_Columns"));
		CastChecked<UScrollBoxSlot>(Scroll->AddChild(Columns))->SetHorizontalAlignment(HAlign_Fill);
		UVerticalBox* Composition = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Staff_Composition"));
		UVerticalBox* Responses = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Staff_Responses"));
		Column(*Columns, *Composition, FMargin(0.f, 0.f, 20.f, 0.f), true);
		Column(*Columns, *Rule(BP, Tree, TEXT("Staff_ColumnDivider"), true));
		Column(*Columns, *Responses, FMargin(20.f, 0.f, 0.f, 0.f), true);
		Row(*Composition, *Label(BP, Tree, TEXT("Staff_CompositionTitle"), LOCTEXT("Composition", "인원 구성"), 22), FMargin(0.f, 0.f, 0.f, 10.f));
		Row(*Composition, *Rule(BP, Tree, TEXT("Staff_CompositionRule")), FMargin(0.f, 0.f, 0.f, 14.f));
		UHorizontalBox* CapacityRow = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Staff_CapacityRow"));
		USizeBox* BarSize = MakeWidget<USizeBox>(BP, Tree, TEXT("Staff_SizeBox_CapacityBar")); BarSize->SetHeightOverride(20.f);
		UOverlay* Bars = MakeWidget<UOverlay>(BP, Tree, TEXT("Staff_CapacityBars")); BarSize->AddChild(Bars);
		Bars->AddChildToOverlay(CapacityBar(BP, Tree, TEXT("Progress_StaffCapacity"), Color(136, 218, 174), true));
		Bars->AddChildToOverlay(CapacityBar(BP, Tree, TEXT("Progress_BaseCapacity"), Green, false));
		CenterColumn(*CapacityRow, *BarSize, FMargin(0.f, 0.f, 12.f, 0.f), true);
		CenterColumn(*CapacityRow, *Label(BP, Tree, TEXT("Text_Capacity"), LOCTEXT("CapacityPreview", "—"), 20, Ink, true));
		Row(*Composition, *CapacityRow, FMargin(0.f, 0.f, 0.f, 8.f));
		Row(*Composition, *Label(BP, Tree, TEXT("Text_CapacityNote"), FText::GetEmpty(), 14, Muted, true), FMargin(0.f, 0.f, 0.f, 8.f));
		Detail(BP, Tree, *Composition, TEXT("Staff_BaseRow"), LOCTEXT("Base", "기본 인원"), TEXT("Text_BaseStaff"));
		Detail(BP, Tree, *Composition, TEXT("Staff_HiredRow"), LOCTEXT("Hired", "고용 인원"), TEXT("Text_HiredStaff"));
		UBorder* Unassigned = MakeWidget<UBorder>(BP, Tree, TEXT("Staff_UnassignedBorder")); Unassigned->SetBrush(Brush(SkyWash, 5.f, 1.f, SkyLine)); Unassigned->SetPadding(FMargin(12.f, 8.f));
		UVerticalBox* UnassignedBody = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Staff_UnassignedBody")); Unassigned->AddChild(UnassignedBody);
		Detail(BP, Tree, *UnassignedBody, TEXT("Staff_UnassignedRow"), LOCTEXT("Unassigned", "미배정 인원"), TEXT("Text_UnassignedStaff"));
		Row(*Composition, *Unassigned, FMargin(0.f, 0.f, 0.f, 8.f));
		StyleValue(*Missing, 14, Muted); Row(*Composition, *Missing);

		Row(*Responses, *Label(BP, Tree, TEXT("Staff_ResponseTitle"), LOCTEXT("Response", "사건 대응 현황"), 22), FMargin(0.f, 0.f, 0.f, 10.f));
		Row(*Responses, *Rule(BP, Tree, TEXT("Staff_ResponseRule")), FMargin(0.f, 0.f, 0.f, 22.f));
		USizeBox* ResponseSize = MakeWidget<USizeBox>(BP, Tree, TEXT("Staff_SizeBox_ResponseIcon")); ResponseSize->SetWidthOverride(60.f); ResponseSize->SetHeightOverride(60.f);
		UImage* ResponseIcon = MakeWidget<UImage>(BP, Tree, TEXT("Image_ResponseStatus"), true); ResponseIcon->SetColorAndOpacity(Color(104, 184, 216));
		ResponseIcon->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr, *MakeObjectPath(Icons / TEXT("Status/T_FF_LH_Icon_StatusPending_White_256")))); ResponseSize->AddChild(ResponseIcon);
		Row(*Responses, *ResponseSize, FMargin(0.f, 0.f, 0.f, 14.f)); CastChecked<UVerticalBoxSlot>(ResponseSize->Slot)->SetHorizontalAlignment(HAlign_Center);
		StyleValue(*Empty, 20); Empty->SetJustification(ETextJustify::Center); Row(*Responses, *Empty, FMargin(12.f, 0.f, 12.f, 12.f)); Row(*Responses, *Assignments);
		Row(*Content, *Scroll, FMargin(0.f, 0.f, 0.f, 12.f), true);
		UBorder* Footer = MakeWidget<UBorder>(BP, Tree, TEXT("Staff_FooterBorder")); Footer->SetBrush(Brush(SkyWash, 6.f, 1.f, SkyLine)); Footer->SetPadding(FMargin(14.f, 10.f));
		UHorizontalBox* FooterColumns = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Staff_FooterColumns")); Footer->AddChild(FooterColumns);
		StyleValue(*IdleNote, 14); CenterColumn(*FooterColumns, *IdleNote, FMargin(0.f, 0.f, 16.f, 0.f), true);
		Column(*FooterColumns, *Rule(BP, Tree, TEXT("Staff_FooterDivider"), true));
		UVerticalBox* Hiring = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Staff_HireInfo")); CenterColumn(*FooterColumns, *Hiring, FMargin(16.f, 0.f, 0.f, 0.f), true);
		StyleValue(*HireNote, 14, Muted); StyleValue(*Wage, 14, Muted); Row(*Hiring, *HireNote); Row(*Hiring, *Wage, FMargin(0.f, 4.f, 0.f, 0.f)); Row(*Content, *Footer);
		BP.GetOutermost()->GetMetaData().SetValue(&BP, VersionKey, TEXT("1")); return true;
	}

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld) return;
		UWidgetBlueprint* BP = LoadExistingBlueprint(PanelPackage);
		UTexture2D* Icon = LoadObject<UTexture2D>(nullptr, *MakeObjectPath(Icons / TEXT("Navigation/T_FF_LH_Icon_Staff_White_256")));
		if (!BP || !BP->WidgetTree || !Icon || !BP->ParentClass->IsChildOf(UFF_LivePanelStaff::StaticClass())) return;
		if (BP->GetOutermost()->GetMetaData().GetValue(BP, VersionKey) == TEXT("1")) { UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: Designer 편집 보존"), LogPrefix); return; }
		if (BP->GetOutermost()->IsDirty()) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 미저장 스태프 WBP"), LogPrefix); return; }
		const FString File = FPackageName::LongPackageNameToFilename(PanelPackage, TEXT(".uasset"));
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/LiveStaffSky") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
		if (!IFileManager::Get().MakeDirectory(*Backup, true) || IFileManager::Get().Copy(*(Backup / FPaths::GetCleanFilename(File)), *File) != COPY_OK) return;
		if (!BuildPanel(*BP, *Icon) || !SaveNewBlueprint(LogPrefix, *BP))
		{
			BP->GetOutermost()->GetMetaData().RemoveValue(BP, VersionKey);
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 제작 실패. 백업=%s"), LogPrefix, *Backup); return;
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 스태프 요약·정원 구성·대응 현황 저장 완료"), LogPrefix);
	}
	FAutoConsoleCommand Command(TEXT("FF.ApplyLiveStaffSkyDesign"), TEXT("P7 스태프 화면을 최초 한 번 제작한다."), FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
