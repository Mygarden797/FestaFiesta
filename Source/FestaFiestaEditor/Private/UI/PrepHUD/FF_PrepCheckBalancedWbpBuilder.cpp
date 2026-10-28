// 기존 점검 위젯을 재배치한다. 데이터·명령·HUD 외곽 치수는 변경하지 않는다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelCheck.h"
#include "UI/PrepHUD/Panels/FF_PrepRecommendedFacilityRow.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPrepCheckBalanced"

namespace FFPrepCheckBalancedBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* Prefix = TEXT("FF_PREP_CHECK_BALANCED");
	constexpr const TCHAR* VersionKey = TEXT("FF_PrepCheckBalancedVersion");
	const FString PanelPackage = TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelCheck");
	const FString RowPackage = TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepRecommendedFacilityRow");

	template <class T> T* Find(UWidgetTree& Tree, const TCHAR* Name)
	{
		return Cast<T>(Tree.FindWidget(Name));
	}

	void FontSize(UTextBlock& Widget, int32 Size)
	{
		FSlateFontInfo Font = Widget.GetFont(); Font.Size = Size; Widget.SetFont(Font);
	}

	void Hide(UWidgetTree& Tree, const TCHAR* Name)
	{
		if (UWidget* Widget = Tree.FindWidget(Name)) Widget->SetVisibility(ESlateVisibility::Collapsed);
	}

	void ApplyPanel(UWidgetBlueprint& BP)
	{
		UWidgetTree& Tree = *BP.WidgetTree;
		auto* Body = Find<UVerticalBox>(Tree, TEXT("VerticalBox_Body"));
		auto* Metrics = Find<UWrapBox>(Tree, TEXT("WrapBox_Metrics"));
		const int32 MetricIndex = Body->GetChildIndex(Metrics);
		auto* BalancedMetrics = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_BalancedMetrics"));
		Row(*Body, *BalancedMetrics, FMargin(0.f, 0.f, 0.f, 16.f));
		Body->ShiftChild(MetricIndex, BalancedMetrics);
		const TCHAR* CardNames[] = { TEXT("SizeBox_WeatherCard"), TEXT("SizeBox_DailyGoalCard"), TEXT("SizeBox_ExpectedVisitorsCard") };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(CardNames); ++Index)
		{
			auto* Card = Find<USizeBox>(Tree, CardNames[Index]);
			Card->ClearWidthOverride(); Card->ClearHeightOverride();
			Card->ClearMinDesiredWidth(); Card->ClearMaxDesiredWidth(); Card->SetMinDesiredHeight(138.f);
			Column(*BalancedMetrics, *Card, FMargin(Index == 0 ? 0.f : 6.f, 0.f, Index == 2 ? 0.f : 6.f, 0.f), true);
		}
		Metrics->SetVisibility(ESlateVisibility::Collapsed);
		Hide(Tree, TEXT("SizeBox_GoldCard"));
		auto* Forecast = Find<UVerticalBox>(Tree, TEXT("VerticalBox_ExpectedVisitorsCard"));
		auto* Risk = Find<UTextBlock>(Tree, TEXT("Text_CapacityRisk"));
		FontSize(*Risk, 15); Row(*Forecast, *Risk, FMargin(0.f, 8.f, 0.f, 0.f));
		auto* CapacityDetail = Find<UTextBlock>(Tree, TEXT("Text_CapacityDetail"));
		FontSize(*CapacityDetail, 13); CapacityDetail->SetAutoWrapText(true);
		Row(*Forecast, *CapacityDetail, FMargin(0.f, 4.f, 0.f, 0.f));
		auto* Delta = Find<UTextBlock>(Tree, TEXT("Text_AdmissionPreviewDelta"));
		FontSize(*Delta, 13); Row(*Forecast, *Delta, FMargin(0.f, 4.f, 0.f, 0.f));
		Hide(Tree, TEXT("SizeBox_CapacitySummary"));

		// 입장료 값·조절·확정을 한 줄에 놓고 초안/실패 문구는 필요할 때만 다음 줄에 나타낸다.
		auto* Columns = Find<UHorizontalBox>(Tree, TEXT("HorizontalBox_AdmissionSection"));
		auto* Editor = Find<UVerticalBox>(Tree, TEXT("VerticalBox_AdmissionEditor"));
		auto* Price = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_BalancedAdmissionPrice"));
		auto* PriceSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_BalancedAdmissionPrice"));
		PriceSize->SetWidthOverride(170.f); PriceSize->AddChild(Price);
		Row(*Price, *Tree.FindWidget(TEXT("Text_AdmissionTitle")), FMargin(0.f, 0.f, 0.f, 4.f));
		auto* Value = Find<UTextBlock>(Tree, TEXT("Text_AdmissionFeeValue"));
		FontSize(*Value, 28); Row(*Price, *Value, FMargin(0.f, 0.f, 0.f, 4.f));
		Row(*Price, *Tree.FindWidget(TEXT("Text_AdmissionFeeRange")));
		Column(*Columns, *PriceSize, FMargin(0.f, 0.f, 20.f, 0.f)); Columns->ShiftChild(0, PriceSize);
		CastChecked<UHorizontalBoxSlot>(PriceSize->Slot)->SetVerticalAlignment(VAlign_Center);
		Column(*Columns, *Editor, FMargin(0.f, 0.f, 20.f, 0.f), true);
		CastChecked<UHorizontalBoxSlot>(Editor->Slot)->SetVerticalAlignment(VAlign_Center);
		auto* Controls = Find<UHorizontalBox>(Tree, TEXT("HorizontalBox_AdmissionControls"));
		CastChecked<UVerticalBoxSlot>(Controls->Slot)->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
		auto* Endpoints = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_AdmissionEndpoints"));
		auto* Minimum = Text(BP, Tree, TEXT("Text_AdmissionFeeMin"), FText::GetEmpty(), 13, Muted, true);
		auto* Maximum = Text(BP, Tree, TEXT("Text_AdmissionFeeMax"), FText::GetEmpty(), 13, Muted, true);
		Maximum->SetJustification(ETextJustify::Right);
		Column(*Endpoints, *Minimum, FMargin(0.f), true); Column(*Endpoints, *Maximum, FMargin(0.f), true);
		Row(*Editor, *Endpoints, FMargin(50.f, 0.f, 50.f, 0.f));
		Editor->ShiftChild(Editor->GetChildIndex(Controls) + 1, Endpoints);
		auto* Actions = Find<UHorizontalBox>(Tree, TEXT("HorizontalBox_AdmissionActions"));
		Column(*Columns, *Actions);
		CastChecked<UHorizontalBoxSlot>(Actions->Slot)->SetVerticalAlignment(VAlign_Center);
		// 적용/되돌리기와 하단 개장 버튼의 사용자 지정 크기는 그대로 유지한다.
		const TCHAR* AdmissionHidden[] = { TEXT("VerticalBox_AdmissionPreview"), TEXT("Border_AdmissionDivider_Size"),
			TEXT("HorizontalBox_AdmissionValues"), TEXT("VerticalBox_AppliedAdmission"), TEXT("Text_AdmissionUnit") };
		for (const TCHAR* Name : AdmissionHidden) Hide(Tree, Name);
		CastChecked<UVerticalBoxSlot>(Tree.FindWidget(TEXT("Border_AdmissionSection"))->Slot)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

		// 기존 바인딩이 담긴 두 요약 열을 동일한 카드로 감싼다. 마지막 날 피날레 줄도 보존한다.
		auto* Details = Find<UHorizontalBox>(Tree, TEXT("HorizontalBox_Details"));
		const TCHAR* DetailNames[] = { TEXT("VerticalBox_Preparation"), TEXT("VerticalBox_Estimate") };
		const TCHAR* FrameNames[] = { TEXT("Border_BalancedPreparation"), TEXT("Border_BalancedEstimate") };
		for (int32 Index = 0; Index < 2; ++Index)
		{
			UWidget* Content = Tree.FindWidget(DetailNames[Index]);
			auto* Frame = MakeWidget<UBorder>(BP, Tree, FrameNames[Index]);
			Frame->SetBrush(Brush(Paper, 7.f, 1.f, SkyLine)); Frame->SetPadding(FMargin(16.f, 12.f));
			Frame->AddChild(Content);
			Column(*Details, *Frame, FMargin(Index == 0 ? 0.f : 8.f, 0.f, Index == 0 ? 8.f : 0.f, 0.f), true);
		}
		Hide(Tree, TEXT("Border_DetailsDivider_Size"));
		Find<UTextBlock>(Tree, TEXT("Box_StaffSummaryRow_Label"))->SetText(LOCTEXT("Staff", "스태프"));
		Find<UTextBlock>(Tree, TEXT("Box_ProgramSummaryRow_Label"))->SetText(LOCTEXT("Programs", "프로그램"));

		auto* Warning = Find<UVerticalBox>(Tree, TEXT("VerticalBox_RecommendedContent"));
		auto* Hint = Find<UTextBlock>(Tree, TEXT("Text_RecommendedHint"));
		Hint->SetText(LOCTEXT("RecommendedNavigation", "선택하면 배치로 이동"));
		Hint->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		auto* Heading = Find<UHorizontalBox>(Tree, TEXT("HorizontalBox_RecommendedHeading"));
		Column(*Heading, *Hint, FMargin(12.f, 0.f, 0.f, 0.f), true);
		CastChecked<UHorizontalBoxSlot>(Hint->Slot)->SetHorizontalAlignment(HAlign_Right);
		CastChecked<UHorizontalBoxSlot>(Hint->Slot)->SetVerticalAlignment(VAlign_Center);
		CastChecked<UVerticalBoxSlot>(Heading->Slot)->SetPadding(FMargin(16.f, 12.f, 16.f, 12.f));
		auto* Wrap = Find<UWrapBox>(Tree, TEXT("WrapBox_RecommendedRows"));
		Wrap->SetInnerSlotPadding(FVector2D(12.f, 12.f));
		CastChecked<UVerticalBoxSlot>(Wrap->Slot)->SetPadding(FMargin(16.f, 0.f, 16.f, 14.f));
		ensure(Wrap->GetParent() == Warning);
		const TCHAR* Kinds[] = { TEXT("Lighting"), TEXT("Food"), TEXT("Toilet"), TEXT("Entertainment"), TEXT("Rest"), TEXT("Drink"), TEXT("FirstAid"), TEXT("FireSafety") };
		const FText Titles[] = { LOCTEXT("Lighting", "조명"), LOCTEXT("Food", "음식"), LOCTEXT("Toilet", "화장실"), LOCTEXT("Entertainment", "즐길거리"),
			LOCTEXT("Rest", "휴게"), LOCTEXT("Drink", "음료"), LOCTEXT("FirstAid", "응급"), LOCTEXT("FireSafety", "소방") };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Kinds); ++Index)
		{
			auto* Item = Find<UFF_PrepRecommendedFacilityRow>(Tree, *(FString(TEXT("RecommendedRow_")) + Kinds[Index]));
			Item->bCompactPresentation = true; Item->CompactTitleText = Titles[Index];
			Item->CompactActionText = LOCTEXT("PlaceArrow", "›");
		}

		// 필수 시설 실패 사유는 줄바꿈 높이를 유지하면서 실행 버튼 왼쪽으로 모은다.
		auto* Footer = Find<UVerticalBox>(Tree, TEXT("VerticalBox_Footer"));
		auto* FooterLine = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_BalancedFooter"));
		auto* Reasons = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_BalancedGateReasons"));
		Row(*Reasons, *Tree.FindWidget(TEXT("HorizontalBox_OpenGateReason")));
		Row(*Reasons, *Tree.FindWidget(TEXT("Text_RecommendedSummary")), FMargin(0.f, 4.f, 0.f, 0.f));
		Column(*FooterLine, *Reasons, FMargin(0.f, 0.f, 16.f, 0.f), true);
		CastChecked<UHorizontalBoxSlot>(Reasons->Slot)->SetVerticalAlignment(VAlign_Center);
		Column(*FooterLine, *Tree.FindWidget(TEXT("HorizontalBox_CheckActions")));
		Hide(Tree, TEXT("Spacer_Actions")); Row(*Footer, *FooterLine);
		auto* Scroll = Find<UScrollBox>(Tree, TEXT("ScrollBox_Body"));
		FScrollBoxStyle ScrollStyle = Scroll->GetWidgetStyle();
		ScrollStyle.TopShadowBrush.DrawAs = ScrollStyle.BottomShadowBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		Scroll->SetWidgetStyle(ScrollStyle);
	}

	void ApplyRow(UWidgetBlueprint& BP)
	{
		UWidgetTree& Tree = *BP.WidgetTree;
		auto* Root = Find<UVerticalBox>(Tree, TEXT("VerticalBox_Row"));
		auto* Size = Find<USizeBox>(Tree, TEXT("SizeBox_Row"));
		auto* Card = Find<UBorder>(Tree, TEXT("Border_CompactCard"));
		auto* OldBody = Find<UVerticalBox>(Tree, TEXT("VerticalBox_CompactCard"));
		auto* Header = Find<UHorizontalBox>(Tree, TEXT("HorizontalBox_Content"));
		auto* Copy = Find<UVerticalBox>(Tree, TEXT("VerticalBox_Copy"));
		auto* Place = Find<UButton>(Tree, TEXT("Button_Place"));
		auto* Title = Find<UTextBlock>(Tree, TEXT("Text_Title"));
		auto* Action = Find<UTextBlock>(Tree, TEXT("Text_Action"));
		// 같은 버튼으로 카드 전체를 누르게 하며 클릭/비활성/기존 설명 툴팁을 보존한다.
		Header->RemoveFromParent(); Place->RemoveFromParent(); Action->RemoveFromParent(); OldBody->RemoveFromParent();
		Row(*Root, *OldBody); OldBody->SetVisibility(ESlateVisibility::Collapsed);
		Card->SetPadding(FMargin(0.f)); Card->SetBrush(Brush(FLinearColor::Transparent)); Card->AddChild(Place);
		auto* ButtonSlot = CastChecked<UButtonSlot>(Place->AddChild(Header));
		ButtonSlot->SetPadding(FMargin(0.f)); ButtonSlot->SetHorizontalAlignment(HAlign_Fill); ButtonSlot->SetVerticalAlignment(VAlign_Center);
		FButtonStyle Style = Place->GetStyle();
		const FLinearColor AmberLine = Color(238, 207, 155);
		Style.Normal = Brush(Paper, 6.f, 1.f, AmberLine);
		Style.Hovered = Brush(Color(255, 244, 221), 6.f, 2.f, Color(196, 113, 12));
		Style.Pressed = Brush(Color(255, 235, 195), 6.f, 2.f, Color(196, 113, 12));
		Style.Disabled = Brush(SkyWash, 6.f, 1.f, SkyLine);
		Style.NormalPadding = Style.PressedPadding = FMargin(12.f, 10.f); Place->SetStyle(Style); Place->SetCursor(EMouseCursor::Hand);
		Size->SetWidthOverride(250.f); Size->ClearHeightOverride(); Size->SetMinDesiredHeight(70.f);
		Row(*Copy, *Title); Copy->ShiftChild(0, Title); FontSize(*Title, 17); Title->SetAutoWrapText(true);
		auto* Status = Text(BP, Tree, TEXT("Text_BalancedMissingStatus"), LOCTEXT("Missing", "없음"), 13, Muted);
		Row(*Copy, *Status, FMargin(0.f, 3.f, 0.f, 0.f)); Copy->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Column(*Header, *Copy, FMargin(0.f, 0.f, 8.f, 0.f), true);
		Column(*Header, *Action); FontSize(*Action, 24);
		for (UWidget* Child : Header->GetAllChildren()) CastChecked<UHorizontalBoxSlot>(Child->Slot)->SetVerticalAlignment(VAlign_Center);
	}

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld) return;
		auto* PanelBP = LoadExistingBlueprint(PanelPackage);
		auto* RowBP = LoadExistingBlueprint(RowPackage);
		if (!PanelBP || !RowBP || !PanelBP->WidgetTree || !RowBP->WidgetTree || !PanelBP->GeneratedClass || !RowBP->GeneratedClass ||
			!PanelBP->GeneratedClass->IsChildOf(UFF_PrepPanelCheck::StaticClass()) || !RowBP->GeneratedClass->IsChildOf(UFF_PrepRecommendedFacilityRow::StaticClass())) return;
		auto* BalancedProperty = FindFProperty<FBoolProperty>(PanelBP->GeneratedClass, TEXT("bUseBalancedLayout"));
		auto* ValueProperty = FindFProperty<FTextProperty>(PanelBP->GeneratedClass, TEXT("AdmissionValueFormat"));
		auto* RangeProperty = FindFProperty<FTextProperty>(PanelBP->GeneratedClass, TEXT("AdmissionRangeFormat"));
		if (PanelBP->GetOutermost()->GetMetaData().GetValue(PanelBP, VersionKey) == TEXT("1") &&
			RowBP->GetOutermost()->GetMetaData().GetValue(RowBP, VersionKey) == TEXT("1"))
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: 적용된 디자인과 이후 Designer 편집을 보존합니다."), Prefix); return;
		}
		if (!BalancedProperty || !ValueProperty || !RangeProperty || PanelBP->GetOutermost()->IsDirty() || RowBP->GetOutermost()->IsDirty() ||
			PanelBP->WidgetTree->FindWidget(TEXT("HorizontalBox_BalancedMetrics")) || RowBP->WidgetTree->FindWidget(TEXT("Text_BalancedMissingStatus")))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 런타임 속성, 미저장 에셋 또는 부분 적용 상태를 확인하세요."), Prefix); return;
		}
		// 전체 선행 구조와 이름을 확인한 후에만 두 에셋을 수정한다.
		bool bValid = true;
		auto Require = [&bValid](UWidgetTree& Tree, const TCHAR* Name, UClass* Class)
		{
			const UWidget* Widget = Tree.FindWidget(Name);
			if (!Widget || !Widget->IsA(Class))
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 기존 위젯 %s 구조가 다릅니다."), Prefix, Name); bValid = false;
			}
		};
		UWidgetTree& PT = *PanelBP->WidgetTree; UWidgetTree& RT = *RowBP->WidgetTree;
		for (const TCHAR* Name : { TEXT("VerticalBox_Body"), TEXT("VerticalBox_ExpectedVisitorsCard"), TEXT("VerticalBox_AdmissionEditor"), TEXT("VerticalBox_Preparation"), TEXT("VerticalBox_Estimate"), TEXT("VerticalBox_RecommendedContent"), TEXT("VerticalBox_Footer") }) Require(PT, Name, UVerticalBox::StaticClass());
		for (const TCHAR* Name : { TEXT("HorizontalBox_AdmissionSection"), TEXT("HorizontalBox_AdmissionControls"), TEXT("HorizontalBox_AdmissionActions"), TEXT("HorizontalBox_Details"), TEXT("HorizontalBox_RecommendedHeading"), TEXT("HorizontalBox_OpenGateReason"), TEXT("HorizontalBox_CheckActions") }) Require(PT, Name, UHorizontalBox::StaticClass());
		for (const TCHAR* Name : { TEXT("Text_CapacityRisk"), TEXT("Text_CapacityDetail"), TEXT("Text_AdmissionPreviewDelta"), TEXT("Text_AdmissionTitle"), TEXT("Text_AdmissionFeeValue"), TEXT("Text_AdmissionFeeRange"), TEXT("Box_StaffSummaryRow_Label"), TEXT("Box_ProgramSummaryRow_Label"), TEXT("Text_RecommendedHint"), TEXT("Text_RecommendedSummary") }) Require(PT, Name, UTextBlock::StaticClass());
		for (const TCHAR* Name : { TEXT("SizeBox_WeatherCard"), TEXT("SizeBox_DailyGoalCard"), TEXT("SizeBox_ExpectedVisitorsCard") }) Require(PT, Name, USizeBox::StaticClass());
		Require(PT, TEXT("WrapBox_Metrics"), UWrapBox::StaticClass()); Require(PT, TEXT("WrapBox_RecommendedRows"), UWrapBox::StaticClass());
		Require(PT, TEXT("Border_AdmissionSection"), UBorder::StaticClass()); Require(PT, TEXT("ScrollBox_Body"), UScrollBox::StaticClass());
		for (const TCHAR* Kind : { TEXT("Lighting"), TEXT("Food"), TEXT("Toilet"), TEXT("Entertainment"), TEXT("Rest"), TEXT("Drink"), TEXT("FirstAid"), TEXT("FireSafety") }) Require(PT, *(FString(TEXT("RecommendedRow_")) + Kind), UFF_PrepRecommendedFacilityRow::StaticClass());
		for (const TCHAR* Name : { TEXT("VerticalBox_Row"), TEXT("VerticalBox_CompactCard"), TEXT("VerticalBox_Copy") }) Require(RT, Name, UVerticalBox::StaticClass());
		Require(RT, TEXT("SizeBox_Row"), USizeBox::StaticClass()); Require(RT, TEXT("Border_CompactCard"), UBorder::StaticClass());
		Require(RT, TEXT("HorizontalBox_Content"), UHorizontalBox::StaticClass()); Require(RT, TEXT("Button_Place"), UButton::StaticClass());
		Require(RT, TEXT("Text_Title"), UTextBlock::StaticClass()); Require(RT, TEXT("Text_Action"), UTextBlock::StaticClass());
		if (!bValid || PT.FindWidget(TEXT("WrapBox_Metrics"))->GetParent() != PT.FindWidget(TEXT("VerticalBox_Body")) ||
			!Cast<UVerticalBoxSlot>(PT.FindWidget(TEXT("HorizontalBox_AdmissionControls"))->Slot) ||
			!Cast<UVerticalBoxSlot>(PT.FindWidget(TEXT("Border_AdmissionSection"))->Slot) ||
			!Cast<UVerticalBoxSlot>(PT.FindWidget(TEXT("HorizontalBox_RecommendedHeading"))->Slot) ||
			!Cast<UVerticalBoxSlot>(PT.FindWidget(TEXT("WrapBox_RecommendedRows"))->Slot)) return;
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/PrepCheckBalanced") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
		if (!IFileManager::Get().MakeDirectory(*Backup, true)) return;
		for (const FString& Package : { PanelPackage, RowPackage })
		{
			const FString File = FPackageName::LongPackageNameToFilename(Package, FPackageName::GetAssetPackageExtension());
			if (IFileManager::Get().IsReadOnly(*File) || IFileManager::Get().Copy(*(Backup / FPaths::GetCleanFilename(File)), *File, false, false) != COPY_OK) return;
		}
		for (UWidgetBlueprint* BP : { RowBP, PanelBP })
		{
			BP->Modify(); BP->WidgetTree->Modify();
			BP->WidgetTree->ForEachWidget([](UWidget* Widget) { Widget->Modify(); if (Widget->Slot) Widget->Slot->Modify(); });
		}
		ApplyPanel(*PanelBP); ApplyRow(*RowBP);
		for (UWidgetBlueprint* BP : { RowBP, PanelBP })
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); FKismetEditorUtilities::CompileBlueprint(BP);
			if (BP->Status == BS_Error || !BP->GeneratedClass)
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 컴파일 실패로 저장하지 않았습니다. 백업=%s"), Prefix, *Backup); return;
			}
		}
		// 컴파일의 클래스 재생성 이후 속성을 다시 얻는다.
		UObject* Defaults = PanelBP->GeneratedClass->GetDefaultObject(); Defaults->Modify();
		BalancedProperty = FindFProperty<FBoolProperty>(PanelBP->GeneratedClass, TEXT("bUseBalancedLayout"));
		ValueProperty = FindFProperty<FTextProperty>(PanelBP->GeneratedClass, TEXT("AdmissionValueFormat"));
		RangeProperty = FindFProperty<FTextProperty>(PanelBP->GeneratedClass, TEXT("AdmissionRangeFormat"));
		if (!BalancedProperty || !ValueProperty || !RangeProperty) return;
		BalancedProperty->SetPropertyValue_InContainer(Defaults, true);
		ValueProperty->SetPropertyValue_InContainer(Defaults, LOCTEXT("AdmissionValue", "{Amount} Gold"));
		RangeProperty->SetPropertyValue_InContainer(Defaults, LOCTEXT("AdmissionRange", "기준 {Reference} Gold"));
		for (UWidgetBlueprint* BP : { RowBP, PanelBP }) BP->GetOutermost()->GetMetaData().SetValue(BP, VersionKey, TEXT("1"));
		if (!SaveAssetPackage(Prefix, *RowBP) || !SaveAssetPackage(Prefix, *PanelBP))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 저장 실패. 백업=%s"), Prefix, *Backup); return;
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 상단 3열·입장료 한 줄·권장 시설 4열 배치 완료. 백업=%s"), Prefix, *Backup);
	}
	FAutoConsoleCommand Command(TEXT("FF.ApplyPrepCheckBalancedDesign"),
		TEXT("점검 패널과 권장 시설 행만 간결한 균등 배치로 변경한다. 기존 헤더·버튼·HUD 치수를 보존한다."),
		FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
