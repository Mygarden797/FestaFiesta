#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Button.h"
#include "Components/UniformGridSlot.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityInfoBuildTabsTest,
	"FestaFiesta.UI.FacilityInfo.BuildTabLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityInfoBuildTabsTest::RunTest(const FString& Parameters)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_FacilityInfoPanel>(nullptr,
		FFBuildUIAssetPaths::FacilityInfoPanelClass));
	if (!TestNotNull(TEXT("Editor world"), World) || !TestNotNull(TEXT("Facility info WBP"), Class)) return false;
	const bool bPreviousPlayerless = Class->bCanCallInitializedWithoutPlayerContext;
	Class->bCanCallInitializedWithoutPlayerContext = true;
	TStrongObjectPtr<UFF_FacilityInfoPanel> Panel(CreateWidget<UFF_FacilityInfoPanel>(World, Class));
	Class->bCanCallInitializedWithoutPlayerContext = bPreviousPlayerless;
	if (!TestNotNull(TEXT("Facility info instance"), Panel.Get())) return false;
	const TSharedRef<SWidget> Slate = Panel->TakeWidget();
	Panel->BuildInspectorTabColumns = 5;
	Panel->bShowUnavailableBuildTabs = true;
	FFF_FacilityInfoViewData View;
	View.Context = EFF_FacilityInfoContext::Build;
	View.FacilityInstanceId = FGuid::NewGuid();
	Panel->ApplyViewData(View);
	const TCHAR* Names[] = {TEXT("Button_TabOverview"), TEXT("Button_TabOperation"), TEXT("Button_TabQueue"),
		TEXT("Button_TabSynergy"), TEXT("Button_TabUpgrade"), TEXT("Button_TabRevenue"), TEXT("Button_TabLighting"),
		TEXT("Button_TabPrograms"), TEXT("Button_TabStaff")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
	{
		auto* Tab = Cast<UButton>(Panel->GetWidgetFromName(Names[Index]));
		if (!TestNotNull(Names[Index], Tab)) return false;
		auto* Slot = Cast<UUniformGridSlot>(Tab->Slot);
		if (!TestNotNull(TEXT("Tab uses the authored uniform grid"), Slot)) return false;
		TestEqual(FString::Printf(TEXT("%s row"), Names[Index]), Slot->GetRow(), Index / 5);
		TestEqual(FString::Printf(TEXT("%s column"), Names[Index]), Slot->GetColumn(), Index % 5);
		TestEqual(FString::Printf(TEXT("%s visible"), Names[Index]), Tab->GetVisibility(), ESlateVisibility::Visible);
	}
	auto* Lighting = CastChecked<UButton>(Panel->GetWidgetFromName(TEXT("Button_TabLighting")));
	auto* Queue = CastChecked<UButton>(Panel->GetWidgetFromName(TEXT("Button_TabQueue")));
	TestFalse(TEXT("Unsupported lighting is disabled"), Lighting->GetIsEnabled());
	TestFalse(TEXT("Unsupported queue is disabled"), Queue->GetIsEnabled());
	View.bHasLightingRig = true;
	View.bHasQueueSettings = true;
	Panel->ApplyViewData(View);
	TestTrue(TEXT("Supported lighting becomes enabled"), Lighting->GetIsEnabled());
	TestTrue(TEXT("Supported queue becomes enabled"), Queue->GetIsEnabled());
	View.Context = EFF_FacilityInfoContext::Live;
	View.bHasLightingRig = false;
	View.bHasQueueSettings = false;
	Panel->ApplyViewData(View);
	TestEqual(TEXT("Live mode keeps legacy unsupported-tab visibility"), Lighting->GetVisibility(), ESlateVisibility::Collapsed);
	Panel->ReleaseSlateResources(true);
	return true;
}

#endif
