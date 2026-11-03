#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Building/FacilityInfo/FF_FacilityInfoPanel.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Button.h"
#include "Components/UniformGridSlot.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityInfoCapabilitiesTest,
	"FestaFiesta.UI.FacilityInfo.CapabilityVisibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityInfoCapabilitiesTest::RunTest(const FString& Parameters)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_FacilityInfoPanel>(nullptr,
		FFBuildUIAssetPaths::FacilityInfoPanelClass));
	if (!TestNotNull(TEXT("Editor world"), World) || !TestNotNull(TEXT("Production inspector class"), Class)) return false;
	const bool bPreviousPlayerless = Class->bCanCallInitializedWithoutPlayerContext;
	Class->bCanCallInitializedWithoutPlayerContext = true;
	TStrongObjectPtr<UFF_FacilityInfoPanel> Panel(CreateWidget<UFF_FacilityInfoPanel>(World, Class));
	Class->bCanCallInitializedWithoutPlayerContext = bPreviousPlayerless;
	if (!TestNotNull(TEXT("Production inspector instance"), Panel.Get())) return false;
	const TSharedRef<SWidget> Slate = Panel->TakeWidget();
	Panel->bShowUnavailableBuildTabs = true; // Old serialized settings cannot restore unavailable controls.
	const auto CheckVisibility = [this, &Panel](const TCHAR* Name, const ESlateVisibility Expected)
	{
		UWidget* Widget = Panel->GetWidgetFromName(Name);
		if (TestNotNull(Name, Widget)) TestEqual(FString(Name) + TEXT(" visibility"), Widget->GetVisibility(), Expected);
	};
	FFF_FacilityInfoViewData View;
	View.Context = EFF_FacilityInfoContext::Build;
	View.FacilityInstanceId = FGuid::NewGuid();
	View.InstallCostText = FText::FromString(TEXT("300 Gold"));
	View.FootprintText = FText::FromString(TEXT("4 × 4 m"));
	View.OperationStateText = FText::FromString(TEXT("통행 전용"));
	// These legacy display strings must never determine capability or occupy rows.
	View.QueueCapacityText = FText::FromString(TEXT("대기열 없음"));
	View.QueueCapacityValueText = View.ServiceCapacityValueText = FText::FromString(TEXT("—"));
	View.ServiceDurationText = FText::FromString(TEXT("이용 서비스 없음"));
	View.WaitingPartiesText = View.OccupancyText = FText::FromString(TEXT("해당 없음"));
	Panel->ApplyViewData(View);
	for (const TCHAR* Name : {TEXT("Button_TabQueue"), TEXT("Button_TabSynergy"), TEXT("Button_TabStaff"),
		TEXT("Button_TabLighting"), TEXT("Button_TabRevenue"), TEXT("Button_TabPrograms"), TEXT("Button_TabUpgrade"),
		TEXT("Text_QueueCapacity"), TEXT("Text_QueueCapacityValue"), TEXT("Text_ServiceCapacityValue"),
		TEXT("Text_ServiceDuration"), TEXT("Text_WaitingParties"), TEXT("Text_Occupancy")})
		CheckVisibility(Name, ESlateVisibility::Collapsed);
	CheckVisibility(TEXT("Button_TabOperation"), ESlateVisibility::Collapsed);
	CheckVisibility(TEXT("V10_OverviewService"), ESlateVisibility::Collapsed);
	Panel->SetInfoPage(EFF_FacilityInfoPage::Lighting);
	TestEqual(TEXT("Direct navigation cannot open unsupported features"), Panel->GetInfoPage(), EFF_FacilityInfoPage::Overview);

	View.bHasService = View.bHasQueueSettings = true;
	View.QueueCapacityValueText = FText::FromString(TEXT("0팀"));
	View.ServiceCapacityValueText = FText::FromString(TEXT("1팀"));
	View.ServiceDurationText = FText::FromString(TEXT("팀당 10초"));
	View.bShowStaffSection = View.bShowProgramSection = View.bShowRevenueSection = true;
	View.bHasLightingRig = View.bHasFacilityLevels = true;
	View.SynergyEntries.AddDefaulted();
	Panel->ApplyViewData(View);
	CheckVisibility(TEXT("Text_QueueCapacityValue"), ESlateVisibility::HitTestInvisible);
	const TCHAR* TabNames[] = {TEXT("Button_TabOverview"), TEXT("Button_TabOperation"), TEXT("Button_TabQueue"),
		TEXT("Button_TabSynergy"), TEXT("Button_TabUpgrade"), TEXT("Button_TabRevenue"), TEXT("Button_TabLighting"),
		TEXT("Button_TabPrograms"), TEXT("Button_TabStaff")};
	for (const TCHAR* Name : TabNames) CheckVisibility(Name, ESlateVisibility::Visible);
	Panel->SetInfoPage(EFF_FacilityInfoPage::Queue);
	TestEqual(TEXT("Supported queue settings remain reachable"), Panel->GetInfoPage(), EFF_FacilityInfoPage::Queue);
	View.bHasQueueSettings = false;
	Panel->ApplyViewData(View);
	TestEqual(TEXT("Losing a selected capability returns to overview"), Panel->GetInfoPage(), EFF_FacilityInfoPage::Overview);
	CheckVisibility(TEXT("Panel_FacilityQueueSettings"), ESlateVisibility::Collapsed);
	Panel->SetInfoPage(EFF_FacilityInfoPage::Synergy);
	TestEqual(TEXT("Existing recipes remain reachable"), Panel->GetInfoPage(), EFF_FacilityInfoPage::Synergy);
	View.SynergyEntries.Reset();
	View.FootprintText = FText::GetEmpty();
	Panel->ApplyViewData(View);
	TestEqual(TEXT("Losing recipes removes the empty selected page"), Panel->GetInfoPage(), EFF_FacilityInfoPage::Overview);
	CheckVisibility(TEXT("Button_TabSynergy"), ESlateVisibility::Collapsed);
	CheckVisibility(TEXT("V10_Card_Text_Footprint"), ESlateVisibility::Collapsed);
	CheckVisibility(TEXT("V10_Row_Text_Footprint"), ESlateVisibility::Collapsed);
	int32 VisibleIndex = 0;
	for (const TCHAR* Name : TabNames)
	{
		auto* Tab = CastChecked<UButton>(Panel->GetWidgetFromName(Name));
		if (Tab->GetVisibility() == ESlateVisibility::Collapsed) continue;
		auto* Slot = Cast<UUniformGridSlot>(Tab->Slot);
		if (TestNotNull(TEXT("Visible feature has a grid slot"), Slot))
		{
			TestEqual(FString(Name) + TEXT(" packs its row"), Slot->GetRow(), VisibleIndex / Panel->BuildInspectorTabColumns);
			TestEqual(FString(Name) + TEXT(" packs its column"), Slot->GetColumn(), VisibleIndex % Panel->BuildInspectorTabColumns);
		}
		++VisibleIndex;
	}
	View.Context = EFF_FacilityInfoContext::Live;
	Panel->ApplyViewData(View);
	CheckVisibility(TEXT("Button_TabUpgrade"), ESlateVisibility::Collapsed);
	CheckVisibility(TEXT("Button_TabLighting"), ESlateVisibility::Visible);
	View.FacilityInstanceId.Invalidate();
	Panel->ApplyViewData(View);
	for (int32 Index = 1; Index < UE_ARRAY_COUNT(TabNames); ++Index)
		CheckVisibility(TabNames[Index], ESlateVisibility::Collapsed);
	Panel->ReleaseSlateResources(true);
	return true;
}

#endif
