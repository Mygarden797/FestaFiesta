// P2 인력 패널만 새 시안으로 제작한다. 다른 패널과 HUD는 이 명령에서 변경하지 않는다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"

#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "HAL/IConsoleManager.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelStaff.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPrepStaffSky"

namespace FFPrepStaffSkyBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* LogPrefix = TEXT("FF_PREP_STAFF_SKY");
	const FString Folder = TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels");
	const FString Package = Folder / TEXT("WBP_FF_PrepPanelStaff");

	void CostRow(UWidgetBlueprint& BP, UWidgetTree& Tree, UVerticalBox& Parent, const TCHAR* Name,
		const TCHAR* ValueName, const FText& Label, const FText& Preview, bool bMuted = false)
	{
		UHorizontalBox* Group = MakeWidget<UHorizontalBox>(BP, Tree, Name);
		const FLinearColor Tint = bMuted ? Muted : Ink;
		Column(*Group, *Text(BP, Tree, *(FString(Name) + TEXT("_Label")), Label, 17, Tint), FMargin(0.f), true);
		UTextBlock* Value = Text(BP, Tree, ValueName, Preview, bMuted ? 17 : 20, Tint, true);
		Value->SetJustification(ETextJustify::Right);
		Column(*Group, *Value);
		Row(Parent, *Group, FMargin(0.f, 0.f, 0.f, 10.f));
	}

	bool Build(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		const FPanelFrame Frame = MakeFrame(BP, Tree, LOCTEXT("Title", "인력"),
			TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Metrics/T_FF_LH_Icon_StaffCount_White_256"));
		UHorizontalBox* Columns = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_StaffColumns"));
		UVerticalBox* Current = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_CurrentStaff"));
		UVerticalBox* Hire = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_HireQuote"));
		Row(*Frame.Body, *Columns);
		Column(*Columns, *Current, FMargin(0.f, 0.f, 24.f, 0.f), true);
		Column(*Columns, *Rule(BP, Tree, TEXT("Border_ColumnDivider"), true));
		Column(*Columns, *Hire, FMargin(24.f, 0.f, 0.f, 0.f), true);

		Row(*Current, *Text(BP, Tree, TEXT("Text_CurrentLabel"), LOCTEXT("Current", "현재 인원")), FMargin(0.f, 0.f, 0.f, 12.f));
		Row(*Current, *Text(BP, Tree, TEXT("Text_StaffCapacity"), LOCTEXT("CapacityPreview", "6 / 12명"), 36, Ink, true), FMargin(0.f, 0.f, 0.f, 8.f));
		UTextBlock* Breakdown = Text(BP, Tree, TEXT("Text_StaffBreakdown"), LOCTEXT("BreakdownPreview", "기본 6명 · 추가 고용 0명"), 16, Muted, true);
		Breakdown->SetAutoWrapText(true);
		Row(*Current, *Breakdown, FMargin(0.f, 0.f, 0.f, 20.f));
		UProgressBar* Capacity = MakeWidget<UProgressBar>(BP, Tree, TEXT("Progress_StaffCapacity"), true);
		FProgressBarStyle Style = Capacity->GetWidgetStyle();
		Style.BackgroundImage = Brush(Color(225, 238, 243), 7.f);
		Style.FillImage = Brush(Green, 7.f);
		Capacity->SetWidgetStyle(Style);
		Capacity->SetFillColorAndOpacity(FLinearColor::White);
		Capacity->SetBarFillType(EProgressBarFillType::LeftToRight);
		Capacity->SetBarFillStyle(EProgressBarFillStyle::Scale);
		Capacity->SetPercent(.5f);
		USizeBox* BarSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_StaffCapacity"));
		BarSize->SetHeightOverride(14.f); BarSize->AddChild(Capacity);
		Row(*Current, *BarSize, FMargin(0.f, 0.f, 0.f, 25.f));
		Row(*Current, *Rule(BP, Tree, TEXT("Border_CurrentDivider")), FMargin(0.f, 0.f, 0.f, 18.f));
		CostRow(BP, Tree, *Current, TEXT("HorizontalBox_HireSpent"), TEXT("Text_HireSpentValue"),
			LOCTEXT("Spent", "누적 고용비"), LOCTEXT("SpentPreview", "0 Gold"));

		Row(*Hire, *Text(BP, Tree, TEXT("Text_HireLabel"), LOCTEXT("Hire", "추가 고용")), FMargin(0.f, 0.f, 0.f, 12.f));
		UHorizontalBox* Stepper = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Stepper"));
		for (int32 Index = 0; Index < 3; ++Index)
		{
			if (Index == 1)
			{
				UTextBlock* Count = Text(BP, Tree, TEXT("Text_SelectedCount"), LOCTEXT("CountPreview", "1명"), 30, Ink, true);
				Count->SetJustification(ETextJustify::Center);
				Column(*Stepper, *Count, FMargin(8.f, 0.f), true);
				CastChecked<UHorizontalBoxSlot>(Count->Slot)->SetVerticalAlignment(VAlign_Center);
				continue;
			}
			const bool bMinus = Index == 0;
			USizeBox* Size = MakeWidget<USizeBox>(BP, Tree, bMinus ? TEXT("SizeBox_Decrease") : TEXT("SizeBox_Increase"));
			Size->SetWidthOverride(48.f); Size->SetHeightOverride(44.f);
			Size->AddChild(Button(BP, Tree, bMinus ? TEXT("Button_Decrease") : TEXT("Button_Increase"),
				*Text(BP, Tree, bMinus ? TEXT("Text_Decrease") : TEXT("Text_Increase"),
					bMinus ? LOCTEXT("Minus", "−") : LOCTEXT("Plus", "+"), 25)));
			Column(*Stepper, *Size);
		}
		Row(*Hire, *Stepper, FMargin(0.f, 0.f, 0.f, 8.f));
		UTextBlock* Unit = Text(BP, Tree, TEXT("Text_HireUnitCost"), LOCTEXT("UnitPreview", "1명 × 450 Gold"), 16, Muted, true);
		Unit->SetJustification(ETextJustify::Center);
		Row(*Hire, *Unit, FMargin(0.f, 0.f, 0.f, 20.f));
		Row(*Hire, *Rule(BP, Tree, TEXT("Border_QuoteDivider")), FMargin(0.f, 0.f, 0.f, 18.f));
		CostRow(BP, Tree, *Hire, TEXT("HorizontalBox_HireCost"), TEXT("Text_HireCostTotal"),
			LOCTEXT("Cost", "고용 비용"), LOCTEXT("CostPreview", "450 Gold"));
		CostRow(BP, Tree, *Hire, TEXT("HorizontalBox_GoldAfter"), TEXT("Text_GoldAfterValue"),
			LOCTEXT("GoldAfter", "고용 후 잔액"), LOCTEXT("GoldAfterPreview", "9,250 Gold"), true);

		UTextBlock* Reason = Text(BP, Tree, TEXT("Text_HireBlockReason"), FText::GetEmpty(), 15, Color(162, 96, 15), true);
		Reason->SetAutoWrapText(true);
		Reason->SetVisibility(ESlateVisibility::Collapsed);
		Row(*Frame.Footer, *Reason, FMargin(0.f, 0.f, 0.f, 8.f));
		UHorizontalBox* Actions = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Actions"));
		UTextBlock* Notice = Text(BP, Tree, TEXT("Text_DailyNotice"), LOCTEXT("Notice", "고용한 인력은 축제 종료까지 함께합니다."), 15, Muted, true);
		Notice->SetAutoWrapText(true);
		Column(*Actions, *Notice, FMargin(0.f, 0.f, 24.f, 0.f), true);
		CastChecked<UHorizontalBoxSlot>(Notice->Slot)->SetVerticalAlignment(VAlign_Center);
		USizeBox* ActionSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_HireAction"));
		ActionSize->SetMinDesiredWidth(188.f); ActionSize->SetMinDesiredHeight(46.f);
		ActionSize->AddChild(Button(BP, Tree, TEXT("Button_Hire"),
			*Text(BP, Tree, TEXT("Text_HireAction"), LOCTEXT("ActionPreview", "1명 고용하기"), 19, FLinearColor::White, true), true));
		Column(*Actions, *ActionSize);
		Row(*Frame.Footer, *Actions);
		return true;
	}

	void Apply()
	{
		if (UWidgetBlueprint* Existing = LoadExistingBlueprint(Package))
		{
			// 최초 적용 이후 Designer에서 수정한 값을 명령 재실행으로 지우지 않는다.
			if (Existing->WidgetTree && Existing->WidgetTree->FindWidget(TEXT("Progress_StaffCapacity")))
			{
				UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_PREP_STAFF_SKY_REUSE: 이미 적용됨. Designer 편집 보존."));
				return;
			}
		}
		if (ReplaceWidgetBlueprint(LogPrefix, Package, Folder, *UFF_PrepPanelStaff::StaticClass(), Build))
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_PREP_STAFF_SKY_APPLIED: 인력 패널 1종 적용."));
		}
	}
	FAutoConsoleCommand Command(TEXT("FF.ApplyPrepStaffSkyDesign"),
		TEXT("P2 인력 WBP에 흰색/하늘색 2열 디자인을 최초 한 번 적용한다. 이후 Designer 편집 보존."),
		FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
