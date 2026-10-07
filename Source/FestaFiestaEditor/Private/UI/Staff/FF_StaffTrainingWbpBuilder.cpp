// P7 스태프 숙련도 최초 제작. 현황 위젯과 사용자 배치/CDO는 그대로 옮겨 보존한다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WidgetSwitcherSlot.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/LiveHUD/Panels/FF_LivePanelStaff.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelStaff.h"
#include "UI/Staff/FF_StaffPromotionRowWidget.h"
#include "UI/Staff/FF_StaffTrainingWidget.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FestaFiestaStaffTraining"

namespace FFStaffTrainingWbpBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* LogPrefix = TEXT("FF_STAFF_TRAINING");
	constexpr const TCHAR* VersionKey = TEXT("FF.StaffTraining.Version");
	const FString Folder = TEXT("/Game/FestaFiesta/UI/Staff");
	const FString TrainingPackage = Folder / TEXT("WBP_FF_StaffTraining");
	const FString RowPackage = Folder / TEXT("WBP_FF_StaffPromotionRow");
	const FString PrepPackage = TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelStaff");
	const FString LivePackage = TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelStaff");

	UTextBlock* Label(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		const FText& Preview, int32 FontSize = 17, FLinearColor Tint = Ink, bool bVariable = false)
	{
		UTextBlock* Result = Text(BP, Tree, Name, Preview, FontSize, Tint, bVariable);
		Result->SetAutoWrapText(true); Result->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return Result;
	}

	USizeBox* Size(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		UWidget& Child, float Width = 0.f, float MinHeight = 0.f)
	{
		USizeBox* Result = MakeWidget<USizeBox>(BP, Tree, Name);
		if (Width > 0.f) { Result->SetWidthOverride(Width); }
		if (MinHeight > 0.f) { Result->SetMinDesiredHeight(MinHeight); }
		Result->AddChild(&Child); return Result;
	}

	UBorder* Card(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, UWidget& Child,
		FMargin Padding = FMargin(16.f, 12.f), FLinearColor Fill = Paper)
	{
		UBorder* Result = MakeWidget<UBorder>(BP, Tree, Name);
		Result->SetBrush(Brush(Fill, 6.f, 1.f, SkyLine)); Result->SetPadding(Padding);
		Result->AddChild(&Child); return Result;
	}

	UScrollBox* Scroll(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, UWidget& Child)
	{
		UScrollBox* Result = MakeWidget<UScrollBox>(BP, Tree, Name);
		Result->SetAllowOverscroll(false); Result->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
		Result->SetScrollbarThickness(FVector2D(5.f, 5.f)); Result->SetClipping(EWidgetClipping::ClipToBounds);
		CastChecked<UScrollBoxSlot>(Result->AddChild(&Child))->SetHorizontalAlignment(HAlign_Fill);
		return Result;
	}

	void WeightedColumn(UHorizontalBox& Parent, UWidget& Child, float Weight, FMargin Padding = FMargin(12.f, 8.f))
	{
		Column(Parent, Child, Padding, true);
		FSlateChildSize Width(ESlateSizeRule::Fill); Width.Value = Weight;
		UHorizontalBoxSlot* Slot = CastChecked<UHorizontalBoxSlot>(Child.Slot);
		Slot->SetSize(Width); Slot->SetVerticalAlignment(VAlign_Center);
	}

	bool BuildPromotionRow(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		UHorizontalBox* Columns = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_StaffRow"));
		WeightedColumn(*Columns, *Label(BP, Tree, TEXT("Text_StaffName"), LOCTEXT("StaffPreview", "스태프"), 17, Ink, true), .30f);
		WeightedColumn(*Columns, *Label(BP, Tree, TEXT("Text_StaffKind"), LOCTEXT("KindPreview", "기본"), 16, Muted, true), .16f);
		WeightedColumn(*Columns, *Label(BP, Tree, TEXT("Text_StaffLevel"), LOCTEXT("LevelPreview", "Lv.1"), 18, Ink, true), .17f);
		UVerticalBox* Action = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_PromotionAction"));
		UButton* Promote = Button(BP, Tree, TEXT("Button_Promote"),
			*Label(BP, Tree, TEXT("Text_Promote"), LOCTEXT("Promote", "승급하기"), 16, Ink, true));
		Promote->SetToolTipText(LOCTEXT("PromoteTip", "이 스태프의 승급을 요청합니다."));
		Row(*Action, *Size(BP, Tree, TEXT("SizeBox_Promote"), *Promote, 0.f, 34.f));
		// 승급 불가 이유는 버튼 밖에 둔다. 비활성 버튼에서도 상태를 읽을 수 있다.
		Row(*Action, *Label(BP, Tree, TEXT("Text_PromotionStatus"), LOCTEXT("StatePreview", "승급 가능 여부 확인 중"), 13, Muted, true), FMargin(0.f, 4.f, 0.f, 0.f));
		WeightedColumn(*Columns, *Action, .37f);
		Tree.RootWidget = Size(BP, Tree, TEXT("SizeBox_StaffRow"),
			*Card(BP, Tree, TEXT("Border_StaffRow"), *Columns, FMargin(0.f)), 0.f, 74.f);
		BP.GetOutermost()->GetMetaData().SetValue(&BP, VersionKey, TEXT("1"));
		return true;
	}

	void Metric(UWidgetBlueprint& BP, UWidgetTree& Tree, UHorizontalBox& Parent,
		const TCHAR* Key, const FText& Caption, const TCHAR* Binding, bool bFirst = false)
	{
		const FString Prefix = FString(TEXT("Training_")) + Key;
		UVerticalBox* Body = MakeWidget<UVerticalBox>(BP, Tree, FName(Prefix + TEXT("_Body")));
		Row(*Body, *Label(BP, Tree, *(Prefix + TEXT("_Caption")), Caption, 16), FMargin(0.f, 0.f, 0.f, 6.f));
		Row(*Body, *Label(BP, Tree, Binding, LOCTEXT("Unknown", "—"), 28, Ink, true));
		Column(Parent, *Size(BP, Tree, *(Prefix + TEXT("_Size")),
			*Card(BP, Tree, *(Prefix + TEXT("_Card")), *Body, FMargin(16.f, 12.f), SkyWash), 0.f, 92.f),
			FMargin(bFirst ? 0.f : 12.f, 0.f, 0.f, 0.f), true);
	}

	bool BuildTraining(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		UVerticalBox* Sections = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_TrainingSections"));
		Tree.RootWidget = Scroll(BP, Tree, TEXT("ScrollBox_TrainingContent"), *Sections);
		UHorizontalBox* Metrics = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_TrainingMetrics"));
		Metric(BP, Tree, *Metrics, TEXT("Total"), LOCTEXT("Total", "전체 인원"), TEXT("Text_TotalStaff"), true);
		Metric(BP, Tree, *Metrics, TEXT("Base"), LOCTEXT("Base", "기본 인원"), TEXT("Text_BaseStaff"));
		Metric(BP, Tree, *Metrics, TEXT("Hired"), LOCTEXT("Hired", "고용 인원"), TEXT("Text_HiredStaff"));
		Row(*Sections, *Metrics, FMargin(0.f, 0.f, 0.f, 14.f));
		Row(*Sections, *Rule(BP, Tree, TEXT("Border_TrainingDivider")), FMargin(0.f, 0.f, 0.f, 12.f));
		Row(*Sections, *Label(BP, Tree, TEXT("Text_TrainingTitle"), LOCTEXT("TrainingTitle", "스태프 숙련도"), 23), FMargin(0.f, 0.f, 0.f, 4.f));
		Row(*Sections, *Label(BP, Tree, TEXT("Text_RosterNote"), LOCTEXT("RosterPreview", "스태프별 숙련도와 승급 가능 여부를 확인하세요."), 15, Muted, true), FMargin(0.f, 0.f, 0.f, 12.f));
		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_RosterHeader"));
		WeightedColumn(*Header, *Label(BP, Tree, TEXT("Text_NameHeader"), LOCTEXT("NameHeader", "스태프"), 16), .30f);
		WeightedColumn(*Header, *Label(BP, Tree, TEXT("Text_KindHeader"), LOCTEXT("KindHeader", "구분"), 16), .16f);
		WeightedColumn(*Header, *Label(BP, Tree, TEXT("Text_LevelHeader"), LOCTEXT("LevelHeader", "현재 레벨"), 16), .17f);
		WeightedColumn(*Header, *Label(BP, Tree, TEXT("Text_PromotionHeader"), LOCTEXT("PromotionHeader", "승급"), 16), .37f);
		Row(*Sections, *Card(BP, Tree, TEXT("Border_RosterHeader"), *Header, FMargin(0.f), SkyWash));
		UVerticalBox* Roster = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Box_StaffRoster"), true);
		USizeBox* RosterSize = Size(BP, Tree, TEXT("SizeBox_StaffRoster"),
			*Scroll(BP, Tree, TEXT("ScrollBox_StaffRoster"), *Roster), 0.f, 150.f);
		RosterSize->SetMaxDesiredHeight(320.f);
		Row(*Sections, *RosterSize);
		Row(*Sections, *Label(BP, Tree, TEXT("Text_RosterEmpty"), LOCTEXT("EmptyPreview", "스태프 목록을 불러오고 있습니다."), 16, Muted, true), FMargin(12.f, 8.f));
		Row(*Sections, *Card(BP, Tree, TEXT("Border_PromotionResult"),
			*Label(BP, Tree, TEXT("Text_PromotionResult"), FText::GetEmpty(), 15, Ink, true), FMargin(14.f, 10.f), SkyWash), FMargin(0.f, 10.f, 0.f, 0.f));
		return true;
	}

	USizeBox* Tab(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Key, const FText& Caption, bool bInitiallySelected)
	{
		const FString Suffix(Key);
		UVerticalBox* Content = MakeWidget<UVerticalBox>(BP, Tree, FName(TEXT("Staff_TabContent_") + Suffix));
		UTextBlock* TabLabel = Label(BP, Tree, *(TEXT("Text_Staff") + Suffix), Caption, 17);
		TabLabel->SetJustification(ETextJustify::Center);
		Row(*Content, *TabLabel, FMargin(0.f, 0.f, 0.f, 3.f));
		UBorder* Indicator = MakeWidget<UBorder>(BP, Tree, FName(TEXT("Indicator_Staff") + Suffix), true);
		Indicator->SetBrush(Brush(Ink, 1.f)); Indicator->SetPadding(FMargin(0.f));
		Indicator->SetVisibility(bInitiallySelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		USizeBox* IndicatorSize = Size(BP, Tree, *(TEXT("Staff_IndicatorSize_") + Suffix), *Indicator);
		IndicatorSize->SetHeightOverride(2.f);
		Row(*Content, *IndicatorSize);
		return Size(BP, Tree, *(TEXT("SizeBox_Staff") + Suffix + TEXT("Tab")),
			*Button(BP, Tree, *(TEXT("Button_Staff") + Suffix), *Content), 128.f, 40.f);
	}

	struct FHost
	{
		UWidgetBlueprint* Blueprint = nullptr;
		UVerticalBox* Sections = nullptr;
		TArray<UWidget*> OverviewChildren;
		bool bApplied = false;
	};

	bool PrepareHost(const FString& PackageName, bool bPrep, FHost& Host)
	{
		Host.Blueprint = LoadExistingBlueprint(PackageName);
		UClass* ParentClass = bPrep ? UFF_PrepPanelStaff::StaticClass() : UFF_LivePanelStaff::StaticClass();
		if (!Host.Blueprint || !Host.Blueprint->WidgetTree || !Host.Blueprint->ParentClass
			|| !Host.Blueprint->ParentClass->IsChildOf(ParentClass) || Host.Blueprint->GetOutermost()->IsDirty())
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 미저장 또는 부모가 다른 스태프 WBP를 보존합니다: %s"), LogPrefix, *PackageName); return false;
		}
		Host.bApplied = Host.Blueprint->GetOutermost()->GetMetaData().GetValue(Host.Blueprint, VersionKey) == TEXT("1");
		if (Host.bApplied) { return true; }
		UWidgetTree& Tree = *Host.Blueprint->WidgetTree;
		const TCHAR* Names[] = { TEXT("Staff_TabRow"), TEXT("Staff_OverviewContent"), TEXT("Switcher_StaffContent"), TEXT("Widget_StaffTraining"),
			TEXT("Button_StaffOverview"), TEXT("Button_StaffTraining"), TEXT("Indicator_StaffOverview"), TEXT("Indicator_StaffTraining") };
		for (const TCHAR* Name : Names)
		{
			if (Tree.FindWidget(Name))
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 기존 위젯/부분 적용 이름 보존: %s"), LogPrefix, Name); return false;
			}
		}
		Host.Sections = Cast<UVerticalBox>(Tree.FindWidget(bPrep ? TEXT("VerticalBox_Sections") : TEXT("Sky_VerticalBox_Sections")));
		Host.OverviewChildren.Add(Tree.FindWidget(bPrep ? TEXT("ScrollBox_Body") : TEXT("VerticalBox_Content")));
		if (bPrep) { Host.OverviewChildren.Add(Tree.FindWidget(TEXT("Border_Footer"))); }
		if (!Host.Sections) { return false; }
		int32 PreviousIndex = INDEX_NONE;
		for (UWidget* Child : Host.OverviewChildren)
		{
			if (!Child || Child->GetParent() != Host.Sections || !Cast<UVerticalBoxSlot>(Child->Slot)) { return false; }
			const int32 Index = Host.Sections->GetChildIndex(Child);
			if (Index <= PreviousIndex) { return false; }
			PreviousIndex = Index;
		}
		return true;
	}

	bool ApplyHost(FHost& Host, UClass* TrainingClass)
	{
		if (Host.bApplied) { return true; }
		UWidgetBlueprint& BP = *Host.Blueprint;
		UWidgetTree& Tree = *BP.WidgetTree;
		BP.Modify(); Tree.Modify(); Host.Sections->Modify();
		const int32 Index = Host.Sections->GetChildIndex(Host.OverviewChildren[0]);
		UHorizontalBox* Tabs = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Staff_TabRow"));
		Column(*Tabs, *Tab(BP, Tree, TEXT("Overview"), LOCTEXT("Overview", "현황"), true), FMargin(0.f, 0.f, 10.f, 0.f));
		Column(*Tabs, *Tab(BP, Tree, TEXT("Training"), LOCTEXT("Training", "숙련도"), false));
		UVerticalBoxSlot* TabSlot = CastChecked<UVerticalBoxSlot>(Host.Sections->InsertChildAt(Index, Tabs));
		TabSlot->SetPadding(FMargin(24.f, 12.f, 24.f, 0.f)); TabSlot->SetHorizontalAlignment(HAlign_Fill);
		UWidgetSwitcher* Switcher = MakeWidget<UWidgetSwitcher>(BP, Tree, TEXT("Switcher_StaffContent"), true);
		UVerticalBoxSlot* SwitcherSlot = CastChecked<UVerticalBoxSlot>(Host.Sections->InsertChildAt(Index + 1, Switcher));
		SwitcherSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); SwitcherSlot->SetHorizontalAlignment(HAlign_Fill);
		UVerticalBox* Overview = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Staff_OverviewContent"));
		UWidgetSwitcherSlot* OverviewSlot = CastChecked<UWidgetSwitcherSlot>(Switcher->AddChild(Overview));
		OverviewSlot->SetHorizontalAlignment(HAlign_Fill); OverviewSlot->SetVerticalAlignment(VAlign_Fill);
		for (UWidget* Child : Host.OverviewChildren)
		{
			Child->Modify(); Child->Slot->Modify();
			// 같은 VerticalBoxSlot의 템플릿을 복사하여 기존 본문/푸터의 여백과 Fill 설정을 유지한다.
			UPanelSlot* ExistingSlot = Child->Slot;
			if (!Overview->AddChild(Child, ExistingSlot)) { return false; }
		}
		UUserWidget* Training = MakeWidgetOfClass(BP, Tree, TEXT("Widget_StaffTraining"), TrainingClass);
		if (!Training) { return false; }
		UWidgetSwitcherSlot* TrainingSlot = CastChecked<UWidgetSwitcherSlot>(Switcher->AddChild(Training));
		TrainingSlot->SetHorizontalAlignment(HAlign_Fill); TrainingSlot->SetVerticalAlignment(VAlign_Fill);
		TrainingSlot->SetPadding(FMargin(24.f, 16.f, 24.f, 18.f));
		Switcher->SetActiveWidgetIndex(0);
		BP.GetOutermost()->GetMetaData().SetValue(&BP, VersionKey, TEXT("1"));
		if (!SaveNewBlueprint(LogPrefix, BP))
		{
			BP.GetOutermost()->GetMetaData().RemoveValue(&BP, VersionKey); return false;
		}
		return true;
	}

	bool CheckShared(const FString& PackageName, UClass& Parent)
	{
		if (!FPackageName::DoesPackageExist(PackageName)) { return true; }
		UWidgetBlueprint* BP = LoadExistingBlueprint(PackageName);
		return BP && BP->WidgetTree && BP->ParentClass && BP->ParentClass->IsChildOf(&Parent)
			&& !BP->GetOutermost()->IsDirty()
			&& BP->GetOutermost()->GetMetaData().GetValue(BP, VersionKey) == TEXT("1");
	}

	bool BackupPackage(const FString& PackageName, const FString& Directory)
	{
		if (!FPackageName::DoesPackageExist(PackageName)) { return true; }
		const FString File = FPackageName::LongPackageNameToFilename(PackageName, TEXT(".uasset"));
		return !IFileManager::Get().IsReadOnly(*File)
			&& IFileManager::Get().Copy(*(Directory / FPaths::GetCleanFilename(File)), *File, false, false) == COPY_OK;
	}

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: PIE 종료 후 실행하세요."), LogPrefix); return;
		}
		FHost Prep, Live;
		if (!PrepareHost(PrepPackage, true, Prep) || !PrepareHost(LivePackage, false, Live)
			|| !CheckShared(RowPackage, *UFF_StaffPromotionRowWidget::StaticClass())
			|| !CheckShared(TrainingPackage, *UFF_StaffTrainingWidget::StaticClass()))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 기존 구조/부모/미저장/부분 적용 에셋을 확인하세요. 덮어쓰지 않았습니다."), LogPrefix); return;
		}
		if (Prep.bApplied && Live.bApplied && FPackageName::DoesPackageExist(RowPackage) && FPackageName::DoesPackageExist(TrainingPackage))
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: 스태프 탭·숙련도 Designer/CDO 편집을 보존합니다."), LogPrefix); return;
		}
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/StaffTraining") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
		if (!IFileManager::Get().MakeDirectory(*Backup, true)) { return; }
		for (const FString& Package : { PrepPackage, LivePackage, RowPackage, TrainingPackage })
		{
			if (!BackupPackage(Package, Backup))
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 기존 에셋 백업 실패: %s"), LogPrefix, *Package); return;
			}
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_BACKUP: %s"), LogPrefix, *FPaths::ConvertRelativePathToFull(Backup));
		UWidgetBlueprint* StaffRow = EnsureWidgetBlueprint(LogPrefix, RowPackage, Folder, *UFF_StaffPromotionRowWidget::StaticClass(), BuildPromotionRow);
		if (!StaffRow || !StaffRow->GeneratedClass) { return; }
		const bool bNewTraining = !FPackageName::DoesPackageExist(TrainingPackage);
		UWidgetBlueprint* Training = EnsureWidgetBlueprint(LogPrefix, TrainingPackage, Folder, *UFF_StaffTrainingWidget::StaticClass(), BuildTraining);
		if (!Training || !Training->GeneratedClass) { return; }
		if (bNewTraining)
		{
			UFF_StaffTrainingWidget* Defaults = CastChecked<UFF_StaffTrainingWidget>(Training->GeneratedClass->GetDefaultObject());
			Defaults->Modify(); Defaults->ConfigureStaffRowDefaults(StaffRow->GeneratedClass.Get());
			Training->GetOutermost()->GetMetaData().SetValue(Training, VersionKey, TEXT("1"));
			if (!SaveAssetPackage(LogPrefix, *Training)) { return; }
		}
		if (!ApplyHost(Prep, Training->GeneratedClass.Get()) || !ApplyHost(Live, Training->GeneratedClass.Get()))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 탭 추가/저장 실패. 백업=%s"), LogPrefix, *Backup); return;
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 기존 현황을 보존하고 준비·라이브 스태프에 숙련도 탭을 추가했습니다."), LogPrefix);
	}

	FAutoConsoleCommand Command(TEXT("FF.ApplyStaffTrainingDesign"),
		TEXT("준비·라이브 인력 현황을 보존하고 숙련도 탭과 승급 행을 처음 한 번 추가한다."),
		FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
