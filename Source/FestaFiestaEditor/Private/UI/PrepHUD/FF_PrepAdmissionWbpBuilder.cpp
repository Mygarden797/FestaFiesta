// P3 입장료: 기존 점검 화면에 편집 영역만 추가한다. 기존 트리·CDO·HUD 배치는 유지한다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelCheck.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPrepAdmission"

namespace FFPrepAdmissionWbpBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* LogPrefix = TEXT("FF_PREP_ADMISSION");
	constexpr const TCHAR* VersionKey = TEXT("FF_PrepAdmissionVersion");
	const FString PanelPackage = TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelCheck");

	UTextBlock* Wrapped(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		const FText& Preview, int32 FontSize = 17, FLinearColor Tint = Ink, bool bVariable = true)
	{
		UTextBlock* Result = Text(BP, Tree, Name, Preview, FontSize, Tint, bVariable);
		Result->SetAutoWrapText(true);
		return Result;
	}

	USizeBox* Sized(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		UWidget& Child, float Width = 0.f, float MinHeight = 0.f)
	{
		USizeBox* Result = MakeWidget<USizeBox>(BP, Tree, Name);
		if (Width > 0.f) { Result->SetWidthOverride(Width); }
		if (MinHeight > 0.f) { Result->SetMinDesiredHeight(MinHeight); }
		Result->AddChild(&Child);
		return Result;
	}

	bool BuildSection(UWidgetBlueprint& BP, UVerticalBox& Body, UWrapBox& Metrics)
	{
		UWidgetTree& Tree = *BP.WidgetTree;
		const int32 InsertIndex = Body.GetChildIndex(&Metrics) + 1;
		if (InsertIndex <= 0) { return false; }
		BP.Modify(); Tree.Modify(); Body.Modify(); Metrics.Modify();

		UBorder* Section = MakeWidget<UBorder>(BP, Tree, TEXT("Border_AdmissionSection"));
		Section->SetBrush(Brush(Paper, 8.f, 1.f, SkyLine));
		Section->SetPadding(FMargin(18.f, 16.f));
		UHorizontalBox* Columns = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_AdmissionSection"));
		Section->AddChild(Columns);
		UVerticalBox* Editor = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_AdmissionEditor"));
		UVerticalBox* Preview = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_AdmissionPreview"));
		Column(*Columns, *Editor, FMargin(0.f, 0.f, 22.f, 0.f), true);
		FSlateChildSize EditorWidth(ESlateSizeRule::Fill); EditorWidth.Value = .66f;
		CastChecked<UHorizontalBoxSlot>(Editor->Slot)->SetSize(EditorWidth);
		Column(*Columns, *Rule(BP, Tree, TEXT("Border_AdmissionDivider"), true));
		Column(*Columns, *Preview, FMargin(22.f, 0.f, 0.f, 0.f), true);
		FSlateChildSize PreviewWidth(ESlateSizeRule::Fill); PreviewWidth.Value = .34f;
		CastChecked<UHorizontalBoxSlot>(Preview->Slot)->SetSize(PreviewWidth);

		Row(*Editor, *Text(BP, Tree, TEXT("Text_AdmissionTitle"), LOCTEXT("AdmissionTitle", "입장료"), 22), FMargin(0.f, 0.f, 0.f, 9.f));
		UHorizontalBox* Values = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_AdmissionValues"));
		Column(*Values, *Sized(BP, Tree, TEXT("SizeBox_AdmissionValue"),
			*Wrapped(BP, Tree, TEXT("Text_AdmissionFeeValue"), LOCTEXT("FeePreview", "—"), 32), 110.f));
		UVerticalBox* Applied = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_AppliedAdmission"));
		Row(*Applied, *Wrapped(BP, Tree, TEXT("Text_AppliedAdmissionFee"), LOCTEXT("AppliedPreview", "현재 적용 입장료 —"), 16, Muted));
		Row(*Applied, *Text(BP, Tree, TEXT("Text_AdmissionUnit"), LOCTEXT("Unit", "금액 단위: 금화"), 13, Muted), FMargin(0.f, 4.f, 0.f, 0.f));
		Column(*Values, *Applied, FMargin(12.f, 0.f, 0.f, 0.f), true);
		CastChecked<UHorizontalBoxSlot>(Applied->Slot)->SetVerticalAlignment(VAlign_Center);
		Row(*Editor, *Values, FMargin(0.f, 0.f, 0.f, 9.f));

		UHorizontalBox* Controls = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_AdmissionControls"));
		UButton* Decrease = Button(BP, Tree, TEXT("Button_AdmissionDecrease"),
			*Text(BP, Tree, TEXT("Text_AdmissionDecreaseLabel"), LOCTEXT("DecreaseGlyph", "−"), 24));
		Decrease->SetToolTipText(LOCTEXT("DecreaseTip", "입장료 낮추기"));
		UButton* Increase = Button(BP, Tree, TEXT("Button_AdmissionIncrease"),
			*Text(BP, Tree, TEXT("Text_AdmissionIncreaseLabel"), LOCTEXT("IncreaseGlyph", "+"), 24));
		Increase->SetToolTipText(LOCTEXT("IncreaseTip", "입장료 높이기"));
		for (UButton* StepButton : { Decrease, Increase })
		{
			FButtonStyle Style = StepButton->GetStyle();
			Style.NormalPadding = Style.PressedPadding = FMargin(1.f);
			StepButton->SetStyle(Style);
		}
		Column(*Controls, *Sized(BP, Tree, TEXT("SizeBox_AdmissionDecrease"), *Decrease, 38.f, 38.f));
		USlider* Slider = MakeWidget<USlider>(BP, Tree, TEXT("Slider_AdmissionFee"), true);
		FSliderStyle SliderStyle = Slider->GetWidgetStyle();
		SliderStyle.NormalBarImage = Brush(SkyLine, 4.f);
		SliderStyle.HoveredBarImage = Brush(Color(143, 196, 217), 4.f);
		SliderStyle.DisabledBarImage = Brush(Color(217, 226, 222), 4.f);
		SliderStyle.NormalThumbImage = Brush(Paper, 11.f, 2.f, Green);
		SliderStyle.NormalThumbImage.ImageSize = FVector2D(22.f, 22.f);
		SliderStyle.HoveredThumbImage = Brush(SkyWash, 12.f, 2.f, Ink);
		SliderStyle.HoveredThumbImage.ImageSize = FVector2D(24.f, 24.f);
		SliderStyle.DisabledThumbImage = Brush(SkyWash, 11.f, 1.f, Muted);
		SliderStyle.DisabledThumbImage.ImageSize = FVector2D(22.f, 22.f);
		SliderStyle.BarThickness = 8.f;
		Slider->SetWidgetStyle(SliderStyle);
		Slider->SetSliderBarColor(FLinearColor::White);
		Slider->SetSliderHandleColor(FLinearColor::White);
		// 실제 입장료 한계·단위는 런타임이 원본 설정을 읽어 정규화한다.
		Slider->SetMinValue(0.f); Slider->SetMaxValue(1.f); Slider->SetValue(0.f);
		Slider->SetIndentHandle(true);
		Slider->SetToolTipText(LOCTEXT("SliderTip", "변경 금액을 미리 본 뒤 적용하세요."));
		Column(*Controls, *Slider, FMargin(12.f, 0.f), true);
		CastChecked<UHorizontalBoxSlot>(Slider->Slot)->SetVerticalAlignment(VAlign_Center);
		Column(*Controls, *Sized(BP, Tree, TEXT("SizeBox_AdmissionIncrease"), *Increase, 38.f, 38.f));
		Row(*Editor, *Controls, FMargin(0.f, 0.f, 0.f, 7.f));
		Row(*Editor, *Wrapped(BP, Tree, TEXT("Text_AdmissionFeeRange"), LOCTEXT("RangePreview", "입장료 범위를 확인하고 있습니다."), 14, Muted), FMargin(0.f, 0.f, 0.f, 12.f));

		UHorizontalBox* Actions = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_AdmissionActions"));
		UButton* ApplyButton = Button(BP, Tree, TEXT("Button_AdmissionApply"),
			*Text(BP, Tree, TEXT("Text_AdmissionApplyLabel"), LOCTEXT("Apply", "적용"), 16, FLinearColor::White), true);
		UButton* ResetButton = Button(BP, Tree, TEXT("Button_AdmissionReset"),
			*Text(BP, Tree, TEXT("Text_AdmissionResetLabel"), LOCTEXT("Reset", "되돌리기"), 16));
		Column(*Actions, *Sized(BP, Tree, TEXT("SizeBox_AdmissionApply"), *ApplyButton, 110.f, 36.f), FMargin(0.f, 0.f, 8.f, 0.f));
		Column(*Actions, *Sized(BP, Tree, TEXT("SizeBox_AdmissionReset"), *ResetButton, 110.f, 36.f));
		Row(*Editor, *Actions, FMargin(0.f, 0.f, 0.f, 9.f));
		Row(*Editor, *Wrapped(BP, Tree, TEXT("Text_AdmissionFeeState"), LOCTEXT("StatePreview", "입장료를 조절하면 예상 방문객과 수입을 미리 볼 수 있습니다."), 14, Muted));

		// 예상 방문객은 기존 카드 자체를 옮긴다. 텍스트 바인딩·그림·폰트·사용자 치수를 그대로 쓴다.
		UWidget* ForecastCard = Tree.FindWidget(TEXT("SizeBox_ExpectedVisitorsCard"));
		if (ForecastCard && ForecastCard->GetParent() == &Metrics)
		{
			ForecastCard->Modify();
			FMargin ExistingPadding(0.f);
			if (UWrapBoxSlot* ExistingSlot = Cast<UWrapBoxSlot>(ForecastCard->Slot)) { ExistingPadding = ExistingSlot->GetPadding(); }
			Metrics.RemoveChild(ForecastCard);
			Row(*Preview, *ForecastCard, ExistingPadding);
		}
		else
		{
			// 사용자가 다른 컨테이너로 옮긴 카드는 건드리지 않는다. 같은 바인딩을 새로 만들지 않는다.
			Row(*Preview, *Text(BP, Tree, TEXT("Text_AdmissionPreviewTitle"), LOCTEXT("PreviewTitle", "선택 금액 미리보기"), 18), FMargin(0.f, 0.f, 0.f, 8.f));
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_KEEP_FORECAST: 기존 예상 방문객 카드의 사용자 위치 보존."), LogPrefix);
		}
		Row(*Preview, *Wrapped(BP, Tree, TEXT("Text_AdmissionPreviewDelta"), LOCTEXT("DeltaPreview", "현재 적용 금액 대비 변동 없음"), 14, Muted), FMargin(0.f, 8.f, 0.f, 12.f));
		UBorder* RevenueCard = MakeWidget<UBorder>(BP, Tree, TEXT("Border_AdmissionPreviewRevenue"));
		RevenueCard->SetBrush(Brush(SkyWash, 7.f, 1.f, SkyLine));
		RevenueCard->SetPadding(FMargin(16.f, 12.f));
		UVerticalBox* Revenue = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_AdmissionPreviewRevenue"));
		RevenueCard->AddChild(Revenue);
		Row(*Revenue, *Text(BP, Tree, TEXT("Text_AdmissionPreviewRevenueCaption"), LOCTEXT("RevenueCaption", "예상 입장 수입"), 15, Muted), FMargin(0.f, 0.f, 0.f, 6.f));
		Row(*Revenue, *Wrapped(BP, Tree, TEXT("Text_AdmissionPreviewRevenue"), LOCTEXT("RevenuePreview", "—"), 25));
		Row(*Preview, *RevenueCard);

		UVerticalBoxSlot* SectionSlot = Cast<UVerticalBoxSlot>(Body.InsertChildAt(InsertIndex, Section));
		if (!SectionSlot) { return false; }
		SectionSlot->SetHorizontalAlignment(HAlign_Fill);
		SectionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		SectionSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
		return true;
	}

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: PIE 실행 중에는 점검 화면을 수정하지 않습니다."), LogPrefix); return;
		}
		UWidgetBlueprint* BP = LoadExistingBlueprint(PanelPackage);
		if (!BP || !BP->WidgetTree || !BP->ParentClass || !BP->ParentClass->IsChildOf(UFF_PrepPanelCheck::StaticClass()))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 기존 FF_PrepPanelCheck 점검 WBP가 필요합니다."), LogPrefix); return;
		}
		UPackage* Package = BP->GetOutermost();
		UWidgetTree& Tree = *BP->WidgetTree;
		if (Package->GetMetaData().GetValue(BP, VersionKey) == TEXT("1") ||
			(Tree.FindWidget(TEXT("Border_AdmissionSection")) && Tree.FindWidget(TEXT("Slider_AdmissionFee"))))
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: 입장료 화면과 기존 Designer 편집값을 보존합니다."), LogPrefix); return;
		}
		if (Package->IsDirty())
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 미저장 점검 WBP가 있습니다. 저장 후 다시 실행하세요."), LogPrefix); return;
		}
		// 실패했던 적용이나 수동 생성 이름이 있으면 덮어쓰지 않는다.
		const TCHAR* ReservedNames[] = {
			TEXT("Border_AdmissionSection"), TEXT("Slider_AdmissionFee"),
			TEXT("Button_AdmissionDecrease"), TEXT("Button_AdmissionIncrease"), TEXT("Button_AdmissionApply"), TEXT("Button_AdmissionReset"),
			TEXT("Text_AdmissionFeeValue"), TEXT("Text_AppliedAdmissionFee"), TEXT("Text_AdmissionFeeRange"), TEXT("Text_AdmissionFeeState"),
			TEXT("Text_AdmissionPreviewDelta"), TEXT("Text_AdmissionPreviewRevenue")
		};
		for (const TCHAR* Name : ReservedNames)
		{
			if (Tree.FindWidget(Name))
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 기존 위젯 이름 %s와 겹칩니다. 자동 덮어쓰기를 중단합니다."), LogPrefix, Name); return;
			}
		}
		UWrapBox* Metrics = Cast<UWrapBox>(Tree.FindWidget(TEXT("WrapBox_Metrics")));
		UVerticalBox* Body = Metrics ? Cast<UVerticalBox>(Metrics->GetParent()) : nullptr;
		UWidget* Capacity = Tree.FindWidget(TEXT("SizeBox_CapacitySummary"));
		if (!Body || !Capacity || Capacity->GetParent() != Body || Body->GetChildIndex(Capacity) <= Body->GetChildIndex(Metrics) || !Tree.FindWidget(TEXT("Text_ExpectedVisitors")))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 기존 요약/수용도 구조가 달라 자동 삽입을 중단합니다."), LogPrefix); return;
		}
		const FString File = FPackageName::LongPackageNameToFilename(PanelPackage, FPackageName::GetAssetPackageExtension());
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/PrepAdmission") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
		if (IFileManager::Get().IsReadOnly(*File) || !IFileManager::Get().MakeDirectory(*Backup, true) ||
			IFileManager::Get().Copy(*(Backup / FPaths::GetCleanFilename(File)), *File, false, false) != COPY_OK)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 점검 WBP 백업 또는 쓰기 권한 확인 실패."), LogPrefix); return;
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_BACKUP: %s"), LogPrefix, *FPaths::ConvertRelativePathToFull(Backup));
		if (!BuildSection(*BP, *Body, *Metrics))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 입장료 영역 제작 실패. 디스크 원본은 저장하지 않았습니다. 백업=%s"), LogPrefix, *Backup); return;
		}
		Package->GetMetaData().SetValue(BP, VersionKey, TEXT("1"));
		if (!SaveNewBlueprint(LogPrefix, *BP))
		{
			Package->GetMetaData().RemoveValue(BP, VersionKey);
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 컴파일/저장 실패. 백업=%s"), LogPrefix, *Backup); return;
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 기존 점검 화면에 입장료·예측 영역을 추가했습니다. 다른 위젯/CDO/HUD는 보존합니다."), LogPrefix);
	}
	FAutoConsoleCommand Command(TEXT("FF.ApplyPrepAdmissionDesign"),
		TEXT("기존 점검 화면에 입장료 조절/예측 영역을 최초 한 번 추가한다. 기존 Designer 편집값을 보존한다."),
		FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
