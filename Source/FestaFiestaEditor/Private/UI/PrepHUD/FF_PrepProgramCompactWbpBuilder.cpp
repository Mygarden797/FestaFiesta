// 기존 프로그램 패널의 선택 기능·사용자 치수를 보존하고 오늘 편성을 분리한다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelProgram.h"
#include "UI/PrepHUD/Panels/FF_PrepProgramCatalogChoice.h"
#include "UI/PrepHUD/Panels/FF_PrepScheduledProgramRow.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPrepProgramCompact"

namespace FFPrepProgramCompactBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* Prefix = TEXT("FF_PREP_PROGRAM_COMPACT");
	constexpr const TCHAR* VersionKey = TEXT("FF_PrepProgramCompactVersion");
	const FString Folder = TEXT("/Game/FestaFiesta/UI/PrepHUD");
	const FString PanelPackage = Folder / TEXT("Panels/WBP_FF_PrepPanelProgram");
	const FString ChoicePackage = Folder / TEXT("WBP_FF_PrepProgramCatalogChoice");
	const FString RowPackage = Folder / TEXT("WBP_FF_PrepScheduledProgramRow");
	const TCHAR* HiddenNames[] = {
		TEXT("Text_SelectedLabel"), TEXT("Text_SelectedProgram"), TEXT("Text_SummaryStatus"),
		TEXT("HorizontalBox_ProgramCost"), TEXT("Text_RevenuePreview"), TEXT("Text_FacilitySelectionHint"),
		TEXT("Text_FacilityNotice"), TEXT("Text_Instruction"), TEXT("Text_ProgramCompletion")
	};

	bool HasVersion(UWidgetBlueprint& BP)
	{
		return BP.GetOutermost()->GetMetaData().GetValue(&BP, VersionKey) == TEXT("1");
	}

	bool BackupPackage(const FString& PackageName, const FString& Directory)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		if (IFileManager::Get().Copy(*(Directory / FPaths::GetCleanFilename(Filename)), *Filename, false, false) == COPY_OK) return true;
		UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 원본 백업 실패: %s"), Prefix, *Filename);
		return false;
	}

	UTextBlock* Wrapped(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		const FText& Preview, int32 FontSize, FLinearColor Tint, bool bVariable = true)
	{
		auto* Result = Text(BP, Tree, Name, Preview, FontSize, Tint, bVariable);
		Result->SetAutoWrapText(true);
		return Result;
	}

	bool BuildScheduledRow(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		auto* Frame = MakeWidget<UBorder>(BP, Tree, TEXT("Border_ScheduledRow"));
		Frame->SetBrush(Brush(Paper, 6.f, 1.f, SkyLine));
		Frame->SetPadding(FMargin(12.f));
		Tree.RootWidget = Frame;
		auto* Content = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_ScheduledRow"));
		Frame->AddChild(Content);
		auto* Mark = Text(BP, Tree, TEXT("Text_CompletedMark"), FText::FromString(TEXT("✓")), 20, Green);
		Column(*Content, *Mark, FMargin(0.f, 0.f, 10.f, 0.f));
		CastChecked<UHorizontalBoxSlot>(Mark->Slot)->SetVerticalAlignment(VAlign_Top);
		auto* Copy = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_ScheduledCopy"));
		Column(*Content, *Copy, FMargin(0.f), true);
		Row(*Copy, *Wrapped(BP, Tree, TEXT("Text_ProgramName"), LOCTEXT("ProgramPreview", "선셋 요가"), 17, Ink), FMargin(0.f, 0.f, 0.f, 4.f));
		Row(*Copy, *Wrapped(BP, Tree, TEXT("Text_FacilityName"), LOCTEXT("FacilityPreview", "체험장 #01"), 14, Muted), FMargin(0.f, 0.f, 0.f, 3.f));
		Row(*Copy, *Wrapped(BP, Tree, TEXT("Text_TimeRange"), LOCTEXT("TimePreview", "18:30 ~ 19:00"), 13, Muted));
		BP.GetOutermost()->GetMetaData().SetValue(&BP, VersionKey, TEXT("1"));
		return true;
	}

	void ApplyPanel(UWidgetBlueprint& BP)
	{
		UWidgetTree& Tree = *BP.WidgetTree;
		BP.Modify(); Tree.Modify();
		Tree.ForEachWidget([](UWidget* Widget) { Widget->Modify(); if (Widget->Slot) Widget->Slot->Modify(); });
		for (const TCHAR* Name : HiddenNames) Tree.FindWidget(Name)->SetVisibility(ESlateVisibility::Collapsed);
		auto* Summary = CastChecked<UVerticalBox>(Tree.FindWidget(TEXT("VerticalBox_Summary")));
		auto* Catalog = CastChecked<UVerticalBox>(Tree.FindWidget(TEXT("VerticalBox_Catalog")));
		auto* Facility = CastChecked<UVerticalBox>(Tree.FindWidget(TEXT("Box_FacilityRow")));
		Facility->RemoveFromParent();
		Row(*Catalog, *Facility, FMargin(0.f, 18.f, 0.f, 0.f));
		CastChecked<UTextBlock>(Tree.FindWidget(TEXT("Text_SummaryTitle")))->SetText(LOCTEXT("Today", "오늘 편성"));
		auto* Count = Text(BP, Tree, TEXT("Text_ScheduledCount"), LOCTEXT("CountPreview", "총 0건"), 30, Ink, true);
		Row(*Summary, *Count, FMargin(0.f, 0.f, 0.f, 12.f));
		Summary->ShiftChild(1, Count);
		auto* Scheduled = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_ScheduledPrograms"), true);
		auto* ScrollSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_ScheduledPrograms"));
		ScrollSize->SetMaxDesiredHeight(340.f);
		auto* Scroll = MakeWidget<UScrollBox>(BP, Tree, TEXT("ScrollBox_ScheduledPrograms"));
		Scroll->SetScrollbarThickness(FVector2D(5.f));
		Scroll->SetScrollbarPadding(FMargin(5.f, 0.f, 0.f, 0.f));
		Scroll->SetAlwaysShowScrollbar(false);
		Scroll->SetAllowOverscroll(false);
		Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
		Scroll->SetClipping(EWidgetClipping::ClipToBounds);
		FScrollBoxStyle ScrollStyle = Scroll->GetWidgetStyle();
		ScrollStyle.TopShadowBrush = Brush(FLinearColor::Transparent);
		ScrollStyle.BottomShadowBrush = Brush(FLinearColor::Transparent);
		Scroll->SetWidgetStyle(ScrollStyle);
		FScrollBarStyle BarStyle = Scroll->GetWidgetBarStyle();
		BarStyle.VerticalBackgroundImage = Brush(Color(234, 241, 242), 2.f);
		for (FSlateBrush* Segment : { &BarStyle.VerticalTopSlotImage, &BarStyle.VerticalBottomSlotImage,
			&BarStyle.HorizontalTopSlotImage, &BarStyle.HorizontalBottomSlotImage })
			*Segment = Brush(FLinearColor::Transparent);
		BarStyle.NormalThumbImage = Brush(Color(136, 185, 200), 2.f);
		BarStyle.HoveredThumbImage = Brush(Color(110, 166, 184), 2.f);
		BarStyle.DraggedThumbImage = Brush(Color(90, 149, 168), 2.f);
		Scroll->SetWidgetBarStyle(BarStyle);
		Scroll->AddChild(Scheduled);
		ScrollSize->AddChild(Scroll);
		Row(*Summary, *ScrollSize);
		auto* Empty = Wrapped(BP, Tree, TEXT("Text_ScheduledEmpty"), LOCTEXT("Empty", "아직 편성한 프로그램이 없습니다."), 15, Muted);
		Row(*Summary, *Empty, FMargin(0.f, 6.f, 0.f, 8.f));
		CastChecked<UHorizontalBoxSlot>(Tree.FindWidget(TEXT("SizeBox_Summary"))->Slot)->SetVerticalAlignment(VAlign_Top);
		CastChecked<UUniformGridPanel>(Tree.FindWidget(TEXT("Box_CatalogChoices")))->SetMinDesiredSlotHeight(54.f);

		// 원본 날짜와 하단 버튼 높이는 그대로 둔다. 하루 규칙은 짧은 배지로 한 번만 남긴다.
		auto* Daily = CastChecked<UTextBlock>(Tree.FindWidget(TEXT("Text_DailyNotice")));
		auto* Intro = CastChecked<UHorizontalBox>(Tree.FindWidget(TEXT("HorizontalBox_Instruction")));
		auto* Day = Tree.FindWidget(TEXT("Border_Day"));
		Daily->SetToolTipText(Daily->GetText());
		Daily->SetText(LOCTEXT("Daily", "오늘만 운영"));
		Daily->SetAutoWrapText(false);
		Daily->RemoveFromParent();
		auto* DailyBadge = MakeWidget<UBorder>(BP, Tree, TEXT("Border_DailyRule"));
		DailyBadge->SetBrush(Brush(SkyWash, 5.f, 1.f, SkyLine));
		DailyBadge->SetPadding(FMargin(10.f, 3.f));
		DailyBadge->AddChild(Daily);
		auto* Spacer = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_CompactIntroSpacer"));
		Column(*Intro, *Spacer, FMargin(0.f), true);
		Intro->ShiftChild(0, Spacer);
		Column(*Intro, *DailyBadge, FMargin(0.f, 0.f, 10.f, 0.f));
		Intro->ShiftChild(Intro->GetChildIndex(Day), DailyBadge);
		CastChecked<UHorizontalBoxSlot>(DailyBadge->Slot)->SetVerticalAlignment(VAlign_Center);

		auto* Gold = CastChecked<UTextBlock>(Tree.FindWidget(TEXT("Text_GoldAfter")));
		auto* Actions = CastChecked<UHorizontalBox>(Tree.FindWidget(TEXT("HorizontalBox_Actions")));
		Gold->RemoveFromParent();
		Column(*Actions, *Gold, FMargin(0.f, 0.f, 20.f, 0.f), true);
		Actions->ShiftChild(0, Gold);
		CastChecked<UHorizontalBoxSlot>(Gold->Slot)->SetVerticalAlignment(VAlign_Center);
		auto* ScheduleSize = CastChecked<USizeBox>(Tree.FindWidget(TEXT("SizeBox_ScheduleAction")));
		// 비용이 포함된 라벨의 가로 공간만 확보한다. 사용자가 조정한 세로 치수는 바꾸지 않는다.
		if (ScheduleSize->GetWidthOverride() > 0.f && ScheduleSize->GetWidthOverride() < 250.f)
			ScheduleSize->SetWidthOverride(250.f);
		ScheduleSize->SetMinDesiredWidth(FMath::Max(ScheduleSize->GetMinDesiredWidth(), 250.f));
		CastChecked<UTextBlock>(Tree.FindWidget(TEXT("Text_ScheduleAction")))->SetAutoWrapText(false);
		CastChecked<UButton>(Tree.FindWidget(TEXT("Button_Schedule")))->SetToolTipText(
			LOCTEXT("ScheduleTooltip", "선택한 시설에 오늘 운영할 프로그램을 편성합니다."));
	}

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: PIE 종료 후 실행하세요."), Prefix); return;
		}
		auto* Panel = LoadExistingBlueprint(PanelPackage);
		auto* Choice = LoadExistingBlueprint(ChoicePackage);
		auto* ExistingRow = LoadExistingBlueprint(RowPackage);
		if (!Panel || !Choice || !Panel->WidgetTree || !Choice->WidgetTree || !Panel->GeneratedClass ||
			!Panel->ParentClass || !Panel->ParentClass->IsChildOf(UFF_PrepPanelProgram::StaticClass()) ||
			!Choice->ParentClass || !Choice->ParentClass->IsChildOf(UFF_PrepProgramCatalogChoice::StaticClass()) ||
			(ExistingRow && (!ExistingRow->ParentClass || !ExistingRow->ParentClass->IsChildOf(UFF_PrepScheduledProgramRow::StaticClass()))))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 원본 프로그램 위젯 또는 부모 클래스가 다릅니다."), Prefix); return;
		}
		if (HasVersion(*Panel) && HasVersion(*Choice) && ExistingRow && HasVersion(*ExistingRow))
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: 적용된 프로그램 디자인과 이후 Designer 편집을 보존합니다."), Prefix); return;
		}
		UWidgetTree& Tree = *Panel->WidgetTree;
		auto* Compact = FindFProperty<FBoolProperty>(Panel->GeneratedClass, TEXT("bUseCompactLayout"));
		auto* RowClass = FindFProperty<FClassProperty>(Panel->GeneratedClass, TEXT("SummaryRowClass"));
		auto* Summary = Cast<UVerticalBox>(Tree.FindWidget(TEXT("VerticalBox_Summary")));
		auto* Catalog = Cast<UVerticalBox>(Tree.FindWidget(TEXT("VerticalBox_Catalog")));
		auto* Facility = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Box_FacilityRow")));
		auto* Actions = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("HorizontalBox_Actions")));
		auto* Intro = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("HorizontalBox_Instruction")));
		auto* Daily = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_DailyNotice")));
		auto* Gold = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_GoldAfter")));
		auto* ChoiceSubtitle = Cast<UTextBlock>(Choice->WidgetTree->FindWidget(TEXT("Text_Subtitle")));
		auto* ChoiceRoot = Cast<UButton>(Choice->WidgetTree->FindWidget(TEXT("Button_Root")));
		auto* ChoiceSize = Cast<USizeBox>(Choice->WidgetTree->FindWidget(TEXT("SizeBox_Choice")));
		bool bValid = Compact && RowClass && Summary && Catalog && Facility && Actions && Intro && Daily && Gold &&
			ChoiceSubtitle && ChoiceRoot && ChoiceSize && Facility->GetParent() == Summary && Gold->GetParent() == Summary && Daily->GetParent() == Actions &&
			Tree.FindWidget(TEXT("Border_Day")) && Tree.FindWidget(TEXT("Text_SummaryTitle")) &&
			Tree.FindWidget(TEXT("SizeBox_Summary")) && Cast<UHorizontalBoxSlot>(Tree.FindWidget(TEXT("SizeBox_Summary"))->Slot) &&
			Cast<UUniformGridPanel>(Tree.FindWidget(TEXT("Box_CatalogChoices"))) &&
			Cast<USizeBox>(Tree.FindWidget(TEXT("SizeBox_ScheduleAction"))) && Tree.FindWidget(TEXT("Text_ScheduleAction")) &&
			Cast<UButton>(Tree.FindWidget(TEXT("Button_Schedule"))) &&
			!Tree.FindWidget(TEXT("Text_ScheduledCount")) && !Tree.FindWidget(TEXT("VerticalBox_ScheduledPrograms")) &&
			!Tree.FindWidget(TEXT("Text_ScheduledEmpty")) && !Panel->GetOutermost()->IsDirty() && !Choice->GetOutermost()->IsDirty() &&
			(!ExistingRow || (!ExistingRow->GetOutermost()->IsDirty() && HasVersion(*ExistingRow)));
		for (const TCHAR* Name : HiddenNames) bValid = bValid && Tree.FindWidget(Name);
		if (!bValid)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 원본 구조·compact 속성·미저장 변경을 확인하세요. 자동 교체하지 않습니다."), Prefix); return;
		}
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/PrepProgramCompact") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
		if (!IFileManager::Get().MakeDirectory(*Backup, true) || !BackupPackage(PanelPackage, Backup) || !BackupPackage(ChoicePackage, Backup) ||
			(ExistingRow && !BackupPackage(RowPackage, Backup))) return;
		auto* ScheduledRow = EnsureWidgetBlueprint(Prefix, RowPackage, Folder, *UFF_PrepScheduledProgramRow::StaticClass(), BuildScheduledRow);
		if (!ScheduledRow || !ScheduledRow->GeneratedClass || ScheduledRow->Status == BS_Error)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 오늘 편성 행 생성 실패. 백업=%s"), Prefix, *Backup); return;
		}
		ApplyPanel(*Panel);
		Choice->Modify(); Choice->WidgetTree->Modify(); ChoiceSubtitle->Modify(); ChoiceRoot->Modify(); ChoiceSize->Modify();
		ChoiceSubtitle->SetVisibility(ESlateVisibility::Collapsed);
		ChoiceRoot->SetToolTipText(LOCTEXT("CatalogTooltip", "건축목록에서 보기"));
		ChoiceSize->SetMinDesiredHeight(54.f);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Choice);
		FKismetEditorUtilities::CompileBlueprint(Choice);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Panel);
		FKismetEditorUtilities::CompileBlueprint(Panel);
		if (Choice->Status == BS_Error || Panel->Status == BS_Error || !Panel->GeneratedClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 프로그램 위젯 컴파일 실패. 원본을 저장하지 않았습니다. 백업=%s"), Prefix, *Backup); return;
		}
		UObject* Defaults = Panel->GeneratedClass->GetDefaultObject();
		Defaults->Modify();
		Compact = FindFProperty<FBoolProperty>(Panel->GeneratedClass, TEXT("bUseCompactLayout"));
		RowClass = FindFProperty<FClassProperty>(Panel->GeneratedClass, TEXT("SummaryRowClass"));
		if (!Compact || !RowClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 컴파일 후 compact 기본값 속성이 없습니다. 백업=%s"), Prefix, *Backup); return;
		}
		Compact->SetPropertyValue_InContainer(Defaults, true);
		RowClass->SetObjectPropertyValue_InContainer(Defaults, ScheduledRow->GeneratedClass);
		Panel->GetOutermost()->GetMetaData().SetValue(Panel, VersionKey, TEXT("1"));
		Choice->GetOutermost()->GetMetaData().SetValue(Choice, VersionKey, TEXT("1"));
		if (!SaveAssetPackage(Prefix, *Choice) || !SaveAssetPackage(Prefix, *Panel))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 프로그램 에셋 저장 실패. 백업=%s"), Prefix, *Backup); return;
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 오늘 편성 목록·시설 선택 위치·비용 버튼을 정리했습니다. HUD·헤더·버튼 높이 보존. 백업=%s"), Prefix, *Backup);
	}

	FAutoConsoleCommand Command(TEXT("FF.ApplyPrepProgramCompactDesign"),
		TEXT("기존 프로그램 선택 기능을 보존하며 오늘 편성 요약을 한 번 적용한다."),
		FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
