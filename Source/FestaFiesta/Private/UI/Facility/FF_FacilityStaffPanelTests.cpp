#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "AssetCompilingManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityStaffPanelTest,
	"FestaFiesta.UI.FacilityInfo.StaffAssignmentControls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityStaffPanelTest::RunTest(const FString& Parameters)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_FacilityInfoPanel>(nullptr,
		FFBuildUIAssetPaths::FacilityInfoPanelClass));
	if (!TestNotNull(TEXT("Editor fixture world"), World) || !TestNotNull(TEXT("Shipped facility info WBP"), Class)) return false;
	const bool bPreviousPlayerless = Class->bCanCallInitializedWithoutPlayerContext;
	Class->bCanCallInitializedWithoutPlayerContext = true;
	TStrongObjectPtr<UFF_FacilityInfoPanel> Panel(CreateWidget<UFF_FacilityInfoPanel>(World, Class));
	Class->bCanCallInitializedWithoutPlayerContext = bPreviousPlayerless;
	if (!TestNotNull(TEXT("Facility info instance"), Panel.Get())) return false;
	const TSharedRef<SWidget> Slate = Panel->TakeWidget();
	UWidget* Section = Panel->GetWidgetFromName(TEXT("Panel_StaffSection"));
	auto* Summary = Cast<UCommonTextBlock>(Panel->GetWidgetFromName(TEXT("Text_StaffSummary")));
	auto* Assign = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_AssignStaff")));
	auto* Unassign = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_UnassignStaff")));
	auto* StaffTab = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_TabStaff")));
	auto* Scroll = Cast<UScrollBox>(Panel->GetWidgetFromName(TEXT("ScrollBox_Details")));
	if (!TestNotNull(TEXT("Staff controls exist without migrating the WBP asset"), Section) ||
		!TestNotNull(TEXT("Staff summary"), Summary) || !TestNotNull(TEXT("Assign button"), Assign) ||
		!TestNotNull(TEXT("Unassign button"), Unassign) || !TestNotNull(TEXT("Staff tab"), StaffTab) ||
		!TestNotNull(TEXT("Authored scroll content remains available"), Scroll)) return false;

	FFF_FacilityInfoViewData Data;
	Data.FacilityInstanceId = FGuid::NewGuid();
	Data.Context = EFF_FacilityInfoContext::Build;
	Data.DisplayName = FText::FromString(TEXT("안내 시설 스태프 배정"));
	Data.CategoryText = FText::FromString(TEXT("안내 · 종합 안내"));
	Data.bShowStaffSection = true;
	Data.StaffCapacity = 3;
	Data.AssignedStaffCount = 1;
	Data.bCanAssignStaff = true;
	Data.bCanUnassignStaff = true;
	Data.StaffSummaryText = FText::FromString(TEXT("스태프 배정 1 / 3명 · 출동 중 0명"));
	Data.StaffAssignmentHint = FText::FromString(TEXT("시설 설치는 정원만 늘립니다. 고용 후 + 배정하세요."));
	Panel->ApplyViewData(Data);
	Panel->SetPanelOpen(true);
	TestEqual(TEXT("Supported facility shows its staff tab"), StaffTab->GetVisibility(), ESlateVisibility::Visible);
	TestEqual(TEXT("Info overview keeps staffing on its own page"), Section->GetVisibility(), ESlateVisibility::Collapsed);
	StaffTab->OnClicked.Broadcast();
	TestEqual(TEXT("Staff tab opens the assignment page"), Panel->GetInfoPage(), EFF_FacilityInfoPage::Staff);
	TestEqual(TEXT("Staff page exposes staff section"), Section->GetVisibility(), ESlateVisibility::Visible);
	TestEqual(TEXT("Staff snapshot reaches the actual WBP text"), Summary->GetText().ToString(), Data.StaffSummaryText.ToString());
	TestTrue(TEXT("Buttons reflect authoritative eligibility"), Assign->GetIsEnabled() && Unassign->GetIsEnabled());

	int32 AssignRequests = 0;
	int32 UnassignRequests = 0;
	const FDelegateHandle AssignHandle = Panel->OnAssignStaffRequested.AddLambda([&AssignRequests]() { ++AssignRequests; });
	const FDelegateHandle UnassignHandle = Panel->OnUnassignStaffRequested.AddLambda([&UnassignRequests]() { ++UnassignRequests; });
	Assign->OnClicked.Broadcast();
	Unassign->OnClicked.Broadcast();
	TestEqual(TEXT("Assign click emits one gameplay request"), AssignRequests, 1);
	TestEqual(TEXT("Unassign click emits one gameplay request"), UnassignRequests, 1);
	TestEqual(TEXT("Widget cannot invent a roster change before gameplay confirms it"), Panel->GetCurrentViewData().AssignedStaffCount, 1);

	Data.bCanAssignStaff = Data.bCanUnassignStaff = false;
	Panel->ApplyViewData(Data);
	Assign->OnClicked.Broadcast();
	Unassign->OnClicked.Broadcast();
	TestTrue(TEXT("Full facility / busy staff disable both actions"), !Assign->GetIsEnabled() && !Unassign->GetIsEnabled());
	TestTrue(TEXT("Programmatic click cannot bypass command eligibility"), AssignRequests == 1 && UnassignRequests == 1);
	Panel->OnAssignStaffRequested.Remove(AssignHandle);
	Panel->OnUnassignStaffRequested.Remove(UnassignHandle);

	Data.Context = EFF_FacilityInfoContext::Live;
	Data.bCanAssignStaff = Data.bCanUnassignStaff = true;
	Panel->ApplyViewData(Data);
	Panel->SetInfoPage(EFF_FacilityInfoPage::Operation);
	TestEqual(TEXT("Operation keeps staffing on its own page"), Section->GetVisibility(), ESlateVisibility::Collapsed);
	StaffTab->OnClicked.Broadcast();
	TestEqual(TEXT("Live context permits staffing through the Staff tab"), Section->GetVisibility(), ESlateVisibility::Visible);
	Data.bHasLightingRig = true;
	Panel->ApplyViewData(Data);
	Panel->SetInfoPage(EFF_FacilityInfoPage::Lighting);
	TestEqual(TEXT("Lighting editor remains free of unrelated staff controls"), Section->GetVisibility(), ESlateVisibility::Collapsed);
	Panel->SetInfoPage(EFF_FacilityInfoPage::Staff);
	Data.bShowStaffSection = false;
	Panel->ApplyViewData(Data);
	TestEqual(TEXT("Losing staff support returns the selected staff page to basic facts"), Panel->GetInfoPage(), EFF_FacilityInfoPage::Overview);
	TestEqual(TEXT("Non-Info facility removes its staff tab from the row"), StaffTab->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Non-Info facility hides staffing controls"), Section->GetVisibility(), ESlateVisibility::Collapsed);
	TestTrue(TEXT("Repeated refresh reuses the same controls"), Panel->GetWidgetFromName(TEXT("Panel_StaffSection")) == Section);
	Data.FacilityInstanceId = FGuid::NewGuid();
	Data.bShowStaffSection = true;
	Data.AssignedStaffCount = 0;
	Data.bCanUnassignStaff = false;
	Panel->ApplyViewData(Data);
	TestEqual(TEXT("An unstaffed supported facility restores the same visible tab"), StaffTab->GetVisibility(), ESlateVisibility::Visible);
	StaffTab->OnClicked.Broadcast();
	TestEqual(TEXT("Zero assigned staff can still open the assignment page"), Panel->GetInfoPage(), EFF_FacilityInfoPage::Staff);

	if (FParse::Param(FCommandLine::Get(), TEXT("StaffFacilityCapture")) && !FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
	{
		Data.bShowStaffSection = true;
		Data.Context = EFF_FacilityInfoContext::Build;
		Panel->ApplyViewData(Data);
		Panel->SetPanelOpen(true);
		Panel->SetInfoPage(EFF_FacilityInfoPage::Staff);
		const FVector2D Size(1280, 720);
		FWidgetRenderer Renderer(false, true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, true));
		if (!TestNotNull(TEXT("Staff panel capture target"), Target.Get())) return false;
		Renderer.DrawWidget(Target.Get(), Slate, Size, 0.f, false);
		FlushRenderingCommands();
		FAssetCompilingManager::Get().FinishAllCompilation();
		FlushRenderingCommands();
		Scroll->ScrollWidgetIntoView(Section, false, EDescendantScrollDestination::IntoView);
		FSlateApplication::Get().InvalidateAllWidgets(false);
		Renderer.DrawWidget(Target.Get(), Slate, Size, 0.f, false);
		FlushRenderingCommands();
		FSlateApplication::Get().InvalidateAllWidgets(false);
		Renderer.DrawWidget(Target.Get(), Slate, Size, 0.f, false);
		FlushRenderingCommands();
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags Flags;
		Flags.SetLinearToGamma(false);
		if (!TestTrue(TEXT("Real renderer supplies staff panel pixels"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags))) return false;
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(1280, 720, Pixels, Png);
		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/StaffFacilities"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString Path = Directory / TEXT("StaffAssignmentControls.png");
		TestTrue(TEXT("Staff panel screenshot saved"), FFileHelper::SaveArrayToFile(Png, *Path));
		AddInfo(FPaths::ConvertRelativePathToFull(Path));
	}
	Panel->ReleaseSlateResources(true);
	return true;
}

#endif
