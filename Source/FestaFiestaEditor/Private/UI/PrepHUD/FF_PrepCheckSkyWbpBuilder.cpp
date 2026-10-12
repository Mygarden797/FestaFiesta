// P5 점검 패널의 최초 제작. 개장 판단과 명령은 기존 HUD/런타임이 소유한다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/PrepHUD/Panels/FF_PrepChoiceButton.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelCheck.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPrepCheckSky"

namespace FFPrepCheckSkyBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* LogPrefix = TEXT("FF_PREP_CHECK_SKY");
	const FString Folder = TEXT("/Game/FestaFiesta/UI/PrepHUD");
	const FString PanelPackage = Folder / TEXT("Panels/WBP_FF_PrepPanelCheck");
	const FString ChoicePackage = Folder / TEXT("WBP_FF_PrepCheckSkyWeatherChoice");
	const FString IconFolder = TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons");

	UTextBlock* Wrapped(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, const FText& Preview,
		int32 Size = 17, FLinearColor Tint = Ink, bool bVariable = true)
	{
		UTextBlock* Result = Text(BP, Tree, Name, Preview, Size, Tint, bVariable);
		Result->SetAutoWrapText(true);
		return Result;
	}

	UButton* StaffLink(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name)
	{
		UButton* Result = MakeWidget<UButton>(BP, Tree, Name, true);
		FButtonStyle Style = Result->GetStyle();
		Style.Normal = Style.Disabled = Brush(FLinearColor::Transparent);
		Style.Hovered = Brush(FLinearColor(.05f, .55f, .55f, .08f), 4.f);
		Style.Pressed = Brush(FLinearColor(.05f, .55f, .55f, .15f), 4.f);
		Style.NormalPadding = Style.PressedPadding = FMargin(0.f);
		Result->SetStyle(Style);
		return Result;
	}

	void SetStaffLinkContent(UButton& Link, UWidget& Content)
	{
		UButtonSlot* Slot = CastChecked<UButtonSlot>(Link.AddChild(&Content));
		Slot->SetPadding(FMargin(0.f));
		Slot->SetHorizontalAlignment(HAlign_Fill);
		Slot->SetVerticalAlignment(VAlign_Fill);
	}

	UButton* StaffLink(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, UWidget& Content)
	{
		UButton* Result = StaffLink(BP, Tree, Name);
		SetStaffLinkContent(*Result, Content);
		return Result;
	}

	void AddInlineStaffAction(UWidgetBlueprint& BP, UWidgetTree& Tree, UHorizontalBox& Row,
		UTextBlock& Reason, UButton& Action)
	{
		Column(Row, Reason, FMargin(0.f, 0.f, 16.f, 0.f), true);
		CastChecked<UHorizontalBoxSlot>(Reason.Slot)->SetVerticalAlignment(VAlign_Center);
		auto* Label = Text(BP, Tree, TEXT("Text_OpenGateStaffLink"),
			LOCTEXT("StaffAssignmentAction", "스태프 배정 →"), 16, FLinearColor::White);
		Label->SetJustification(ETextJustify::Center);
		SetStaffLinkContent(Action, *Label);
		FButtonStyle Style = Action.GetStyle();
		Style.Normal = Brush(Color(33, 112, 139), 6.f);
		Style.Hovered = Brush(Color(40, 136, 164), 6.f);
		Style.Pressed = Brush(Color(26, 89, 112), 6.f);
		Style.NormalPadding = Style.PressedPadding = FMargin(14.f, 10.f);
		Action.SetStyle(Style);
		Action.SetCursor(EMouseCursor::Hand);
		Column(Row, Action);
		CastChecked<UHorizontalBoxSlot>(Action.Slot)->SetVerticalAlignment(VAlign_Center);
	}

	USizeBox* Icon(UWidgetBlueprint& BP, UWidgetTree& Tree, const FString& Name,
		const FString& RelativePackage, float Size = 27.f)
	{
		USizeBox* Frame = MakeWidget<USizeBox>(BP, Tree, FName(Name + TEXT("_Size")));
		Frame->SetWidthOverride(Size); Frame->SetHeightOverride(Size);
		UImage* Image = MakeWidget<UImage>(BP, Tree, FName(Name));
		Image->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr, *MakeObjectPath(IconFolder / RelativePackage)));
		Image->SetColorAndOpacity(Ink); Frame->AddChild(Image);
		return Frame;
	}

	bool BuildWeatherChoice(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		USizeBox* Size = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_Choice"));
		Size->SetWidthOverride(62.f); Size->SetHeightOverride(30.f); Tree.RootWidget = Size;
		UTextBlock* Label = Text(BP, Tree, TEXT("Text_Label"), LOCTEXT("Clear", "맑음"), 14, Ink, true);
		Label->SetJustification(ETextJustify::Center);
		UButton* Root = Button(BP, Tree, TEXT("Button_Root"), *Label);
		FButtonStyle Style = Root->GetStyle();
		Style.Normal = Brush(Paper, 5.f, 1.f, SkyLine);
		Style.Hovered = Brush(SkyWash, 5.f, 2.f, Color(95, 172, 203));
		// 선택 여부는 색뿐 아니라 테두리 두께로도 구분한다.
		Style.Pressed = Brush(Green, 5.f, 2.f, Ink);
		Style.NormalPadding = Style.PressedPadding = FMargin(5.f, 2.f);
		Root->SetStyle(Style); Size->AddChild(Root);
		return true;
	}

	UWidgetBlueprint* EnsureWeatherChoice()
	{
		if (UWidgetBlueprint* Existing = LoadExistingBlueprint(ChoicePackage)) { return Existing; }
		UWidgetBlueprint* BP = EnsureWidgetBlueprint(LogPrefix, ChoicePackage, Folder,
			*UFF_PrepChoiceButton::StaticClass(), BuildWeatherChoice);
		if (!BP || !BP->GeneratedClass) { return nullptr; }
		UFF_PrepChoiceButton* Defaults = CastChecked<UFF_PrepChoiceButton>(BP->GeneratedClass->GetDefaultObject());
		Defaults->Modify(); Defaults->SetContentColors(Ink, FLinearColor::White);
		FKismetEditorUtilities::CompileBlueprint(BP);
		return SaveAssetPackage(LogPrefix, *BP) ? BP : nullptr;
	}

	UVerticalBox* MetricCard(UWidgetBlueprint& BP, UWidgetTree& Tree, UWrapBox& Parent,
		const TCHAR* Key, const FText& Caption)
	{
		const FString Prefix(Key);
		USizeBox* Size = MakeWidget<USizeBox>(BP, Tree, FName(TEXT("SizeBox_") + Prefix + TEXT("Card")));
		Size->SetWidthOverride(240.f); Size->SetMinDesiredHeight(136.f);
		UBorder* Card = MakeWidget<UBorder>(BP, Tree, FName(TEXT("Border_") + Prefix + TEXT("Card")));
		Card->SetBrush(Brush(SkyWash, 7.f, 1.f, SkyLine)); Card->SetPadding(FMargin(16.f, 12.f));
		Size->AddChild(Card);
		UVerticalBox* Body = MakeWidget<UVerticalBox>(BP, Tree, FName(TEXT("VerticalBox_") + Prefix + TEXT("Card")));
		Card->AddChild(Body);
		Row(*Body, *Text(BP, Tree, *(TEXT("Text_") + Prefix + TEXT("Caption")), Caption, 15, Muted),
			FMargin(0.f, 0.f, 0.f, 10.f));
		UWrapBoxSlot* Slot = Parent.AddChildToWrapBox(Size);
		Slot->SetVerticalAlignment(VAlign_Fill);
		return Body;
	}

	void MetricValue(UWidgetBlueprint& BP, UWidgetTree& Tree, UVerticalBox& Parent,
		const TCHAR* Name, const FText& Preview, const FString& IconPackage)
	{
		UHorizontalBox* Group = MakeWidget<UHorizontalBox>(BP, Tree, FName(FString(Name) + TEXT("_Row")));
		USizeBox* ValueIcon = Icon(BP, Tree, FString(Name) + TEXT("_Icon"), IconPackage);
		Column(*Group, *ValueIcon, FMargin(0.f, 0.f, 12.f, 0.f));
		CastChecked<UHorizontalBoxSlot>(ValueIcon->Slot)->SetVerticalAlignment(VAlign_Center);
		UTextBlock* Value = Wrapped(BP, Tree, Name, Preview, 23);
		Column(*Group, *Value, FMargin(0.f), true);
		CastChecked<UHorizontalBoxSlot>(Value->Slot)->SetVerticalAlignment(VAlign_Center);
		Row(Parent, *Group);
	}

	void DetailRow(UWidgetBlueprint& BP, UWidgetTree& Tree, UVerticalBox& Parent,
		const TCHAR* Name, const TCHAR* ValueName, const FText& Caption, const FText& Preview,
		bool bGroupBinding = false)
	{
		UHorizontalBox* Group = MakeWidget<UHorizontalBox>(BP, Tree, Name, bGroupBinding);
		UTextBlock* Label = Wrapped(BP, Tree, *(FString(Name) + TEXT("_Label")), Caption, 16, Muted, false);
		Column(*Group, *Label, FMargin(0.f, 0.f, 16.f, 0.f), true);
		CastChecked<UHorizontalBoxSlot>(Label->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UTextBlock* Value = Wrapped(BP, Tree, ValueName, Preview, 17);
		Value->SetJustification(ETextJustify::Right); Column(*Group, *Value, FMargin(0.f), true);
		FSlateChildSize ValueSize(ESlateSizeRule::Fill); ValueSize.Value = 1.5f;
		CastChecked<UHorizontalBoxSlot>(Value->Slot)->SetSize(ValueSize);
		Row(Parent, *Group, FMargin(0.f, 0.f, 0.f, 9.f));
		Row(Parent, *Rule(BP, Tree, *(FString(Name) + TEXT("_Rule"))), FMargin(0.f, 0.f, 0.f, 9.f));
	}

	bool BuildPanel(UWidgetBlueprint& BP, UWidgetTree& Tree, UClass* ChoiceClass)
	{
		const FPanelFrame Frame = MakeFrame(BP, Tree, LOCTEXT("Title", "개장 전 점검"),
			IconFolder / TEXT("Status/T_FF_LH_Icon_StatusCheck_White_256"));
		// 기본 700 높이에서는 시설 조건과 견적 안내까지 한 번에 읽히도록 점검만 간격을 줄인다.
		UWidget* Scroll = Tree.FindWidget(TEXT("ScrollBox_Body"));
		CastChecked<UVerticalBoxSlot>(Scroll->Slot)->SetPadding(FMargin(24.f, 14.f, 24.f, 14.f));
		UHorizontalBox* Header = CastChecked<UHorizontalBox>(Tree.FindWidget(TEXT("HorizontalBox_Header")));
		UWidget* Title = Tree.FindWidget(TEXT("Text_PanelTitle"));
		CastChecked<UHorizontalBoxSlot>(Title->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		UBorder* Day = MakeWidget<UBorder>(BP, Tree, TEXT("Border_DayBadge"));
		Day->SetBrush(Brush(SkyWash, 14.f, 1.f, SkyLine)); Day->SetPadding(FMargin(12.f, 4.f));
		Day->AddChild(Text(BP, Tree, TEXT("Text_DayLabel"), LOCTEXT("Day", "1일차"), 17, Ink, true));
		UHorizontalBoxSlot* DaySlot = CastChecked<UHorizontalBoxSlot>(Header->InsertChildAt(2, Day));
		DaySlot->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f)); DaySlot->SetVerticalAlignment(VAlign_Center);
		USpacer* Space = MakeWidget<USpacer>(BP, Tree, TEXT("Spacer_Header"));
		CastChecked<UHorizontalBoxSlot>(Header->InsertChildAt(3, Space))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		UWrapBox* Metrics = MakeWidget<UWrapBox>(BP, Tree, TEXT("WrapBox_Metrics"));
		Metrics->SetInnerSlotPadding(FVector2D(12.f, 12.f)); Metrics->SetExplicitWrapSize(false);
		Row(*Frame.Body, *Metrics, FMargin(0.f, 0.f, 0.f, 18.f));
		UVerticalBox* Weather = MetricCard(BP, Tree, *Metrics, TEXT("Weather"), LOCTEXT("WeatherLabel", "날씨"));
		Row(*Weather, *Wrapped(BP, Tree, TEXT("Text_Weather"), LOCTEXT("Clear", "맑음"), 23), FMargin(0.f, 0.f, 0.f, 10.f));
		UWrapBox* Choices = MakeWidget<UWrapBox>(BP, Tree, TEXT("WrapBox_WeatherChoices"));
		Choices->SetInnerSlotPadding(FVector2D(4.f, 4.f)); Choices->SetExplicitWrapSize(false);
		const TCHAR* Names[] = { TEXT("Choice_Clear"), TEXT("Choice_Rain"), TEXT("Choice_Windy") };
		const FText Labels[] = { LOCTEXT("Clear", "맑음"), LOCTEXT("Rain", "비"), LOCTEXT("Windy", "바람") };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
		{
			UFF_PrepChoiceButton* Choice = Cast<UFF_PrepChoiceButton>(MakeWidgetOfClass(BP, Tree, Names[Index], ChoiceClass));
			if (!Choice) { return false; }
			Choice->ConfigureChoice(Index, Labels[Index]); Choices->AddChildToWrapBox(Choice);
		}
		Row(*Weather, *Choices);
		UTextBlock* WeatherLock = Wrapped(BP, Tree, TEXT("Text_WeatherLockReason"), FText::GetEmpty(), 13, Muted);
		WeatherLock->SetVisibility(ESlateVisibility::Collapsed); Row(*Weather, *WeatherLock, FMargin(0.f, 6.f, 0.f, 0.f));
		UVerticalBox* Goal = MetricCard(BP, Tree, *Metrics, TEXT("DailyGoal"), LOCTEXT("GoalLabel", "일일 목표"));
		MetricValue(BP, Tree, *Goal, TEXT("Text_DailyGoalValue"), LOCTEXT("GoalPreview", "500명"), TEXT("Metrics/T_FF_LH_Icon_Visitors_White_256"));
		Row(*Goal, *Wrapped(BP, Tree, TEXT("Text_DailyGoalBonus"), LOCTEXT("GoalBonusPreview", "달성 시 +300 Gold"), 14, Muted), FMargin(0.f, 8.f, 0.f, 0.f));
		UVerticalBox* Visitors = MetricCard(BP, Tree, *Metrics, TEXT("ExpectedVisitors"), LOCTEXT("VisitorsLabel", "예상 방문객"));
		MetricValue(BP, Tree, *Visitors, TEXT("Text_ExpectedVisitors"), LOCTEXT("VisitorsPreview", "425명"), TEXT("Navigation/T_FF_LH_Icon_Guests_White_256"));
		UVerticalBox* Gold = MetricCard(BP, Tree, *Metrics, TEXT("Gold"), LOCTEXT("GoldLabel", "보유 금화"));
		MetricValue(BP, Tree, *Gold, TEXT("Text_Gold"), LOCTEXT("GoldPreview", "10,000 Gold"), TEXT("Metrics/T_FF_LH_Icon_Gold_White_256"));
		// 금화 리소스에는 원래 색이 들어 있으므로 단색 아이콘의 잉크 틴트를 곱하지 않는다.
		CastChecked<UImage>(Tree.FindWidget(TEXT("Text_Gold_Icon")))->SetColorAndOpacity(FLinearColor::White);

		UBorder* Capacity = MakeWidget<UBorder>(BP, Tree, TEXT("Border_CapacitySummary"));
		Capacity->SetBrush(Brush(SkyWash, 7.f, 1.f, SkyLine)); Capacity->SetPadding(FMargin(16.f, 12.f));
		UHorizontalBox* CapacityContent = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_CapacitySummary"));
		Capacity->AddChild(CapacityContent);
		UTextBlock* Risk = Wrapped(BP, Tree, TEXT("Text_CapacityRisk"), LOCTEXT("RiskPreview", "○ 여유"), 19);
		Column(*CapacityContent, *Risk, FMargin(0.f, 0.f, 20.f, 0.f), true);
		UHorizontalBoxSlot* RiskSlot = CastChecked<UHorizontalBoxSlot>(Risk->Slot);
		FSlateChildSize RiskWidth(ESlateSizeRule::Fill); RiskWidth.Value = .4f;
		RiskSlot->SetSize(RiskWidth); RiskSlot->SetVerticalAlignment(VAlign_Center);
		UTextBlock* CapacityDetail = Wrapped(BP, Tree, TEXT("Text_CapacityDetail"),
			LOCTEXT("CapacityPreview", "정원 62,887명 · 예상 425명"), 15, Muted);
		Column(*CapacityContent, *CapacityDetail, FMargin(0.f), true);
		UHorizontalBoxSlot* DetailSlot = CastChecked<UHorizontalBoxSlot>(CapacityDetail->Slot);
		FSlateChildSize DetailWidth(ESlateSizeRule::Fill); DetailWidth.Value = .6f;
		DetailSlot->SetSize(DetailWidth); DetailSlot->SetVerticalAlignment(VAlign_Center);
		// 미확인 사유가 길면 높이가 늘어난다. 고정 높이로 잘라 버리지 않는다.
		USizeBox* CapacitySize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_CapacitySummary"));
		CapacitySize->SetMinDesiredHeight(56.f); CapacitySize->AddChild(Capacity);
		Row(*Frame.Body, *CapacitySize, FMargin(0.f, 0.f, 0.f, 20.f));

		UHorizontalBox* Details = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Details"));
		UVerticalBox* Summary = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Preparation"));
		UVerticalBox* Estimate = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Estimate"));
		Column(*Details, *Summary, FMargin(0.f, 0.f, 24.f, 0.f), true);
		Column(*Details, *Rule(BP, Tree, TEXT("Border_DetailsDivider"), true));
		Column(*Details, *Estimate, FMargin(24.f, 0.f, 0.f, 0.f), true); Row(*Frame.Body, *Details);
		Row(*Summary, *Text(BP, Tree, TEXT("Text_PreparationTitle"), LOCTEXT("Preparation", "준비 현황"), 20), FMargin(0.f, 0.f, 0.f, 10.f));
		Row(*Summary, *Rule(BP, Tree, TEXT("Border_PreparationRule")), FMargin(0.f, 0.f, 0.f, 12.f));
		DetailRow(BP, Tree, *Summary, TEXT("Box_StaffSummaryRow"), TEXT("Text_StaffSummary"), LOCTEXT("Staff", "인력"), LOCTEXT("StaffPreview", "6명 (기본 6 · 고용 0)"));
		DetailRow(BP, Tree, *Summary, TEXT("Box_ProgramSummaryRow"), TEXT("Text_ProgramSummary"), LOCTEXT("Programs", "유료 프로그램"), LOCTEXT("ProgramsPreview", "0개 편성"));
		// 필수 시설은 축제별로 여러 조건이 올 수 있으므로 전체 폭에서 줄바꿈한다.
		Row(*Summary, *Text(BP, Tree, TEXT("Text_RequiredFacilitiesCaption"), LOCTEXT("Required", "필수 시설"), 16, Muted), FMargin(0.f, 0.f, 0.f, 5.f));
		Row(*Summary, *StaffLink(BP, Tree, TEXT("Button_RequiredFacilities"),
			*Wrapped(BP, Tree, TEXT("Text_RequiredFacilities"), LOCTEXT("RequiredPreview", "메인공연장 · 1개 설치 · ! 추가 설치 필요"), 16)), FMargin(0.f, 0.f, 0.f, 12.f));
		DetailRow(BP, Tree, *Summary, TEXT("Box_FinaleSummaryRow"), TEXT("Text_FinaleSummary"), LOCTEXT("Finale", "피날레"), LOCTEXT("FinalePreview", "미구매 — 최소 연출만 실행됩니다"), true);
		Tree.FindWidget(TEXT("Box_FinaleSummaryRow"))->SetVisibility(ESlateVisibility::Collapsed);
		Tree.FindWidget(TEXT("Box_FinaleSummaryRow_Rule_Size"))->SetVisibility(ESlateVisibility::Collapsed);
		Row(*Estimate, *Text(BP, Tree, TEXT("Text_EstimateTitle"), LOCTEXT("Estimate", "오늘 견적"), 20), FMargin(0.f, 0.f, 0.f, 10.f));
		Row(*Estimate, *Rule(BP, Tree, TEXT("Border_EstimateRule")), FMargin(0.f, 0.f, 0.f, 12.f));
		DetailRow(BP, Tree, *Estimate, TEXT("Box_WageRow"), TEXT("Text_StaffWageCost"), LOCTEXT("HireCost", "고용비"), LOCTEXT("ZeroGold", "0 Gold"));
		DetailRow(BP, Tree, *Estimate, TEXT("Box_AdmissionRow"), TEXT("Text_ExpectedAdmission"), LOCTEXT("Admission", "예상 입장료 수입"), LOCTEXT("AdmissionPreview", "2,125 Gold"));
		DetailRow(BP, Tree, *Estimate, TEXT("Box_GoalBonusRow"), TEXT("Text_ExpectedGoalBonus"), LOCTEXT("Bonus", "일일 목표 보너스"), LOCTEXT("ZeroGold", "0 Gold"));
		Row(*Estimate, *Wrapped(BP, Tree, TEXT("Text_EstimateNotice"), LOCTEXT("EstimateNotice", "부스 매출과 미니게임 보상은 포함되지 않습니다."), 14, Muted));

		UHorizontalBox* ReasonRow = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_OpenGateReason"));
		AddInlineStaffAction(BP, Tree, *ReasonRow,
			*Wrapped(BP, Tree, TEXT("Text_OpenGateReason"), LOCTEXT("ReasonPreview", "! 필수 시설을 설치하면 시작할 수 있어요."), 16),
			*StaffLink(BP, Tree, TEXT("Button_OpenGateReason")));
		Row(*Frame.Footer, *ReasonRow, FMargin(0.f, 0.f, 0.f, 9.f));
		UHorizontalBox* Actions = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_CheckActions"));
		Column(*Actions, *MakeWidget<USpacer>(BP, Tree, TEXT("Spacer_Actions")), FMargin(0.f), true);
		USizeBox* ContinueSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_ContinueBuild"));
		ContinueSize->SetMinDesiredWidth(190.f); ContinueSize->SetMinDesiredHeight(46.f);
		ContinueSize->AddChild(Button(BP, Tree, TEXT("Button_ContinueBuild"),
			*Text(BP, Tree, TEXT("Text_ContinueBuildLabel"), LOCTEXT("Continue", "배치 계속하기"), 18)));
		Column(*Actions, *ContinueSize, FMargin(0.f, 0.f, 12.f, 0.f));
		USizeBox* OpenSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_OpenGate"));
		OpenSize->SetMinDesiredWidth(236.f); OpenSize->SetMinDesiredHeight(46.f);
		OpenSize->AddChild(Button(BP, Tree, TEXT("Button_OpenGate"),
			*Text(BP, Tree, TEXT("Text_OpenGateLabel"), LOCTEXT("OpenPreview", "확정하고 Day 1 시작"), 18, FLinearColor::White, true), true));
		Column(*Actions, *OpenSize); Row(*Frame.Footer, *Actions);
		return true;
	}

	void MigrateStaffLinks()
	{
		constexpr const TCHAR* MigrationLog = TEXT("FF_PREP_CHECK_STAFF_LINKS");
		if (GEditor && GEditor->PlayWorld)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: PIE 실행 중에는 점검 화면을 수정하지 않습니다."), MigrationLog);
			return;
		}
		UWidgetBlueprint* BP = LoadExistingBlueprint(PanelPackage);
		if (!BP || !BP->WidgetTree || !BP->GeneratedClass || !BP->ParentClass ||
			!BP->ParentClass->IsChildOf(UFF_PrepPanelCheck::StaticClass()))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 기존 FF_PrepPanelCheck 점검 WBP가 필요합니다."), MigrationLog);
			return;
		}
		UWidgetTree& Tree = *BP->WidgetTree;
		struct FLink { const TCHAR* TextName; const TCHAR* ButtonName; };
		const FLink Links[] = {
			{ TEXT("Text_RequiredFacilities"), TEXT("Button_RequiredFacilities") }
		};
		UTextBlock* Reason = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_OpenGateReason")));
		UWidget* ExistingAction = Tree.FindWidget(TEXT("Button_OpenGateReason"));
		UWidget* ExistingRow = Tree.FindWidget(TEXT("HorizontalBox_OpenGateReason"));
		const bool bInlineAction = ExistingRow && Cast<UHorizontalBox>(ExistingRow) &&
			Reason && Reason->GetParent() == ExistingRow && Cast<UButton>(ExistingAction) &&
			ExistingAction->GetParent() == ExistingRow &&
			Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_OpenGateStaffLink"))) &&
			Cast<UButton>(ExistingAction)->GetContent() == Tree.FindWidget(TEXT("Text_OpenGateStaffLink"));
		if (!Reason || !Reason->GetParent() || !Reason->Slot ||
			(ExistingRow && !bInlineAction) ||
			(!ExistingRow && ((ExistingAction && (!Cast<UButton>(ExistingAction) || Reason->GetParent() != ExistingAction ||
				!ExistingAction->GetParent() || !ExistingAction->Slot)) || Tree.FindWidget(TEXT("Text_OpenGateStaffLink")))))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 개장 사유의 기존 구조 또는 배정 버튼 이름이 예상과 다릅니다."), MigrationLog);
			return;
		}
		bool bNeedsWrap = false;
		for (const FLink& Link : Links)
		{
			UTextBlock* Label = Cast<UTextBlock>(Tree.FindWidget(Link.TextName));
			UWidget* Existing = Tree.FindWidget(Link.ButtonName);
			if (!Label || !Label->GetParent() || !Label->Slot ||
				(Existing && (!Cast<UButton>(Existing) || Label->GetParent() != Existing)))
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: %s의 기존 구조 또는 %s 이름이 예상과 다릅니다."),
					MigrationLog, Link.TextName, Link.ButtonName);
				return;
			}
			bNeedsWrap |= !Existing;
		}
		FTextProperty* FormatProperty = FindFProperty<FTextProperty>(BP->GeneratedClass, TEXT("RequiredFacilityFormat"));
		UObject* Defaults = BP->GeneratedClass->GetDefaultObject();
		const bool bLegacyFormat = FormatProperty && FormatProperty->GetPropertyValue_InContainer(Defaults).ToString()
			== TEXT("{Name} · {Installed}/{Required} · {State}");
		if (!bNeedsWrap && !bLegacyFormat && bInlineAction)
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: 점검 링크와 Designer 편집값을 보존합니다."), MigrationLog);
			return;
		}
		UPackage* Package = BP->GetOutermost();
		if (Package->IsDirty())
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 미저장 점검 WBP가 있습니다. 저장 후 다시 실행하세요."), MigrationLog);
			return;
		}
		const FString File = FPackageName::LongPackageNameToFilename(PanelPackage, FPackageName::GetAssetPackageExtension());
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/PrepCheckStaffLinks") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
		if (IFileManager::Get().IsReadOnly(*File) || !IFileManager::Get().MakeDirectory(*Backup, true) ||
			IFileManager::Get().Copy(*(Backup / FPaths::GetCleanFilename(File)), *File, false, false) != COPY_OK)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 점검 WBP 백업 또는 쓰기 권한 확인 실패."), MigrationLog);
			return;
		}
		BP->Modify();
		Tree.Modify();
		for (const FLink& Link : Links)
		{
			if (Tree.FindWidget(Link.ButtonName)) { continue; }
			UWidget* Label = Tree.FindWidget(Link.TextName);
			UPanelWidget* Parent = Label->GetParent();
			Parent->Modify();
			Label->Modify();
			Label->Slot->Modify();
			UButton* Wrapper = StaffLink(*BP, Tree, Link.ButtonName);
			// ReplaceChild keeps the original slot object, including all authored layout values.
			if (!Parent->ReplaceChild(Label, Wrapper))
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: %s 감싸기 실패. 저장하지 않았습니다. 백업=%s"), MigrationLog, Link.TextName, *Backup);
				return;
			}
			// ReplaceChild does not clear the outgoing child's Slot. Clear it before AddChild
			// so UButton does not remove the wrapper's preserved slot from the old parent.
			Label->Slot = nullptr;
			SetStaffLinkContent(*Wrapper, *Label);
		}
		if (!bInlineAction)
		{
			// The old asset wrapped the whole reason in a button. Keep its authored outer
			// slot and text styling, then place a separate compact action on the right.
			UWidget* Previous = ExistingAction ? ExistingAction : Reason;
			UPanelWidget* Parent = Previous->GetParent();
			Parent->Modify(); Previous->Modify(); Previous->Slot->Modify(); Reason->Modify();
			UHorizontalBox* InlineRow = MakeWidget<UHorizontalBox>(*BP, Tree, TEXT("HorizontalBox_OpenGateReason"));
			if (!Parent->ReplaceChild(Previous, InlineRow))
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 사유/배정 버튼 행 구성 실패. 저장하지 않았습니다. 백업=%s"), MigrationLog, *Backup);
				return;
			}
			Previous->Slot = nullptr;
			if (ExistingAction) Reason->RemoveFromParent();
			UButton* Action = ExistingAction ? CastChecked<UButton>(ExistingAction)
				: StaffLink(*BP, Tree, TEXT("Button_OpenGateReason"));
			AddInlineStaffAction(*BP, Tree, *InlineRow, *Reason, *Action);
		}
		if (bLegacyFormat)
		{
			Defaults->Modify();
			FormatProperty->SetPropertyValue_InContainer(Defaults,
				LOCTEXT("RequiredInstallFormat", "{Name} · {Required}개 설치 · {State}"));
		}
		if (!SaveNewBlueprint(MigrationLog, *BP)) { return; }
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 점검 안내에 스태프 배정 바로가기를 추가했습니다. 기존 슬롯과 편집값 보존. 백업=%s"),
			MigrationLog, *FPaths::ConvertRelativePathToFull(Backup));
	}

	void Apply()
	{
		if (UWidgetBlueprint* Existing = LoadExistingBlueprint(PanelPackage))
		{
			if (Existing->WidgetTree && Existing->WidgetTree->FindWidget(TEXT("WrapBox_Metrics")))
			{
				UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: 점검 패널의 Designer 편집 보존."), LogPrefix);
				return;
			}
		}
		UWidgetBlueprint* Choice = EnsureWeatherChoice();
		if (!Choice || !Choice->GeneratedClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 날씨 선택지 생성 실패."), LogPrefix);
			return;
		}
		if (ReplaceWidgetBlueprint(LogPrefix, PanelPackage, Folder / TEXT("Panels"), *UFF_PrepPanelCheck::StaticClass(),
			[Choice](UWidgetBlueprint& BP, UWidgetTree& Tree) { return BuildPanel(BP, Tree, Choice->GeneratedClass); }))
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 점검 패널과 전용 날씨 선택지 생성 완료."), LogPrefix);
		}
	}
	FAutoConsoleCommand Command(TEXT("FF.ApplyPrepCheckSkyDesign"),
		TEXT("P5 점검 WBP에 하늘색 카드·준비 현황·견적 디자인을 최초 적용한다. 이후 Designer 편집 보존."),
		FConsoleCommandDelegate::CreateStatic(&Apply));
	FAutoConsoleCommand StaffLinksCommand(TEXT("FF.MigratePrepCheckStaffLinks"),
		TEXT("점검 안내 오른쪽에 스태프 배정 버튼을 배치하고 기본 필수 시설 문구의 설치 비율을 제거한다. 기존 Designer 편집 보존."),
		FConsoleCommandDelegate::CreateStatic(&MigrateStaffLinks));
}

#undef LOCTEXT_NAMESPACE
