#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UI/Facility/FF_FacilityInfoSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "Slate/WidgetRenderer.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "Tests/FF_ProjectZoneTestFixture.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SVirtualWindow.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueueSettingsInspectorTest,
	"FestaFiesta.UI.FacilityInfo.QueueSettingsLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityQueueSettingsInspectorTest::RunTest(const FString& Parameters)
{
	auto* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
	TGuardValue<bool> Ground(Settings->bEnableGroundChecks, false);
	TGuardValue<bool> Collision(Settings->bEnableWorldCollisionChecks, false);
	TGuardValue<bool> Navigation(Settings->bEnableNavigationProjectionChecks, false);
	TGuardValue<bool> Policies(Settings->bRequireProductionPolicyDomains, false);
	TGuardValue<EFFZSChangePhase> Phase(Settings->InitialPhase, EFFZSChangePhase::InitialSetup);
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
	UWorld* World = Fixture.GetTestWorld();
	UGameInstance* Game = World->GetGameInstance();
	auto* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
	auto* Info = World->GetSubsystem<UFF_FacilityInfoSubsystem>();
	if (!TestNotNull(TEXT("Zone authority"), Zones) || !TestNotNull(TEXT("Facility inspector authority"), Info)) return false;
	FWorldContext* Context = GEngine->GetWorldContextFromWorld(World);
	if (!TestNotNull(TEXT("Game viewport context"), Context)) return false;
	auto* Viewport = NewObject<UGameViewportClient>(GEngine);
	Context->GameViewport = Viewport;
	Viewport->Init(*Context, Game, false);
	const TSharedRef<SOverlay> Overlay = SNew(SOverlay);
	Viewport->SetViewportOverlayWidget(nullptr, Overlay);
	FString PlayerError;
	ULocalPlayer* Player = Game->CreateLocalPlayer(0, PlayerError, false);
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("Local player"), Player) || !TestNotNull(TEXT("Controller"), Controller)) return false;
	Controller->SetPlayer(Player);
	World->AddController(Controller);
	auto* Progression = Game->GetSubsystem<UFF_FestivalProgressionSubsystem>();
	if (!TestNotNull(TEXT("Festival economy"), Progression)) return false;
	auto* Festival = NewObject<UFF_FestivalDefinition>(Game);
	auto* Costs = NewObject<UDataTable>(Game);
	Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
	FFF_FacilityCostRow Cost; Cost.InstallCost = 100;
	Costs->AddRow(TEXT("Cost.Booth.FoodStall.QueueInspectorFixture"), Cost);
	Costs->AddRow(TEXT("Cost.Performance.MainStage.SunsetMainStage"), Cost);
	Festival->FestivalId = TEXT("FacilityQueueSettingsInspectorTest");
	Festival->FacilityCostTable = Costs;
	Festival->InitialGold = 10000;
	Festival->RequiredSetupFacilities.Reset();
	Progression->StartFestival(Festival);
	Zones->SetChangePhase(EFFZSChangePhase::InitialSetup);
	auto* Zone = World->SpawnActor<AFFZSGameplayZone>();
	if (!FFProjectZoneTest::RegisterZone(*this, Zone, Zones)) return false;
	const auto Install = [&](const int32 X, const bool bMainStage)
	{
		FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec();
		Spec.InstallCostKey = NAME_None;
		Spec.FacilityDefinitionId = bMainStage ? TEXT("Performance_MainStage_SunsetMainStage") : TEXT("Booth_FoodStall_QueueInspectorFixture");
		Spec.DisplayName = FText::FromString(bMainStage ? TEXT("메인스테이지") : TEXT("대기열 설정 확인 시설"));
		if (bMainStage) { Spec.Category = EFFZSFacilityCategory::Performance; Spec.NormalizeNonServiceGeometry(); }
		else Spec.QueueSettings.bEnabled = true;
		FFFZSGridPlacement Placement;
		Placement.OriginCell = FIntPoint(X, 2500);
		const FFFZSCommand Command = UFFZSGridLibrary::MakeInstallCommand(Spec, Zone->ZoneId, Placement, Zones->GetWorldRevision());
		const auto Result = Zones->ExecuteCommand(Command);
		TestTrue(*FString::Printf(TEXT("Inspector fixture installs: %s"), *Result.Message.ToString()), Result.bSucceeded);
		return Zones->FindFacility(Command.FacilityInstanceId);
	};
	AFFZSFacilityActor* First = Install(2500, false);
	AFFZSFacilityActor* Second = Install(6500, false);
	AFFZSFacilityActor* MainStage = Install(8500, true);
	if (!First || !Second || !MainStage) return false;
	const FGuid FirstId = First->GetFacilityInstanceId();
	if (!TestTrue(TEXT("Actual selection opens the production inspector"), Info->SelectFacility(Controller, First, EFF_FacilityInfoContext::Build))) return false;
	TArray<UUserWidget*> Panels;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Panels, UFF_FacilityInfoPanel::StaticClass(), false);
	auto* Panel = Panels.Num() == 1 ? Cast<UFF_FacilityInfoPanel>(Panels[0]) : nullptr;
	if (!TestNotNull(TEXT("One production facility inspector"), Panel)) return false;
	ON_SCOPE_EXIT { Info->ClearSelection(); };
	Panel->TakeWidget();
	auto* Teams = Cast<UEditableTextBox>(Panel->GetWidgetFromName(TEXT("Editable_FacilityQueueTeams")));
	auto* Spacing = Cast<UEditableTextBox>(Panel->GetWidgetFromName(TEXT("Editable_FacilityQueueSpacing")));
	auto* Width = Cast<UEditableTextBox>(Panel->GetWidgetFromName(TEXT("Editable_FacilityQueueWidth")));
	auto* Apply = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_ApplyFacilityQueueSettings")));
	UWidget* Section = Panel->GetWidgetFromName(TEXT("Panel_FacilityQueueSettings"));
	auto* Scroll = Cast<UScrollBox>(Panel->GetWidgetFromName(TEXT("ScrollBox_Details")));
	if (!TestNotNull(TEXT("Team capacity input"), Teams) || !TestNotNull(TEXT("Spacing input"), Spacing) ||
		!TestNotNull(TEXT("Lane width input"), Width) || !TestNotNull(TEXT("Atomic apply button"), Apply) ||
		!TestNotNull(TEXT("Queue section"), Section) || !TestNotNull(TEXT("Scrollable details"), Scroll)) return false;
	bool bInsideDetailsScroll = false;
	for (UWidget* Parent = Section->GetParent(); Parent; Parent = Parent->GetParent())
		if (Parent == Scroll) bInsideDetailsScroll = true;
	if (!TestTrue(TEXT("Queue page uses the shared details scroller"), bInsideDetailsScroll)) return false;
	UWidget* Footer = Panel->GetWidgetFromName(TEXT("Panel_Actions"));
	UWidget* Card = Panel->GetWidgetFromName(TEXT("Border_Panel"));
	if (!TestNotNull(TEXT("Fixed action footer"), Footer) || !TestNotNull(TEXT("Facility card"), Card)) return false;
	TestFalse(TEXT("Basic facts no longer include queue controls"), Section->IsVisible());
	auto* QueueTab = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_TabQueue")));
	if (!TestNotNull(TEXT("Authored queue tab"), QueueTab)) return false;
	QueueTab->OnClicked.Broadcast();
	TestEqual(TEXT("Queue tab opens its own page"), Panel->GetInfoPage(), EFF_FacilityInfoPage::Queue);
	TestTrue(TEXT("Queue page exposes editable settings"), Section->IsVisible() && Apply->GetIsEnabled());
	TestEqual(TEXT("Initial queue capacity is six teams"), Teams->GetText().ToString(), FString(TEXT("6")));
	TestEqual(TEXT("Initial spacing reads the actual installed value"), FCString::Atof(*Spacing->GetText().ToString()), 115.f);
	TestEqual(TEXT("Initial width reads the actual installed value"), FCString::Atof(*Width->GetText().ToString()), 300.f);
	const auto Type = [](UEditableTextBox& Input, const TCHAR* Text)
	{
		Input.SetText(FText::FromString(Text));
		Input.OnTextChanged.Broadcast(Input.GetText());
	};
	const int64 InitialRevision = Zones->GetWorldRevision();
	Type(*Teams, TEXT("8"));
	Teams->OnTextCommitted.Broadcast(Teams->GetText(), ETextCommit::OnUserMovedFocus);
	Info->Tick(1.f);
	TestEqual(TEXT("Periodic status refresh preserves an unsubmitted draft"), Teams->GetText().ToString(), FString(TEXT("8")));
	TestEqual(TEXT("Changing fields never submits a partial authority mutation"), Zones->GetWorldRevision(), InitialRevision);
	Type(*Spacing, TEXT("140.5"));
	Type(*Width, TEXT("350"));
	Apply->OnClicked.Broadcast();
	FFFZSQueueSettings Applied;
	Applied.bEnabled = true; Applied.MaxWaitingTeams = 8; Applied.SlotSpacingCm = 140.5f; Applied.LaneWidthCm = 350.f;
	TestTrue(TEXT("One inspector click atomically applies all three fields to the selected actor"), First->GetBaseFacilitySpec().QueueSettings == Applied);
	TestTrue(TEXT("Authority feedback refreshes current inspector values"), Panel->GetCurrentViewData().QueueSettings == Applied);
	TestEqual(TEXT("One click adds one authority revision"), Zones->GetWorldRevision(), InitialRevision + 1);
	TestEqual(TEXT("Another facility retains its own capacity"), Second->GetBaseFacilitySpec().QueueSettings.MaxWaitingTeams, 6);
	Type(*Teams, TEXT("99")); Type(*Width, TEXT("99"));
	Apply->OnClicked.Broadcast();
	Info->Tick(1.f);
	TestTrue(TEXT("Malformed width never applies a partial capacity change"), First->GetBaseFacilitySpec().QueueSettings == Applied);
	TestEqual(TEXT("Invalid input remains available for correction"), Width->GetText().ToString(), FString(TEXT("99")));
	if (!TestTrue(TEXT("Inspector switches to the second installed facility"), Info->SelectFacility(Controller, Second, EFF_FacilityInfoContext::Build))) return false;
	TestEqual(TEXT("Switching the selected facility discards the previous draft"), Teams->GetText().ToString(), FString(TEXT("6")));
	TestFalse(TEXT("A stale reviewed facility ID cannot edit the newly selected facility"), Info->ApplySelectedQueueSettings(First->GetFacilityInstanceId(), Applied));
	TestEqual(TEXT("Stale command leaves the new facility intact"), Second->GetBaseFacilitySpec().QueueSettings.MaxWaitingTeams, 6);
	if (!TestTrue(TEXT("Inspector returns to the first installed facility"), Info->SelectFacility(Controller, First, EFF_FacilityInfoContext::Build))) return false;
	FText UndoError;
	TestTrue(*FString::Printf(TEXT("Queue edits use existing construction undo: %s"), *UndoError.ToString()), Zones->UndoConstruction(UndoError));
	// Construction history stages replacement actors while preserving their stable IDs.
	// Query the committed registry after each history operation, as the inspector does.
	First = Zones->FindFacility(FirstId);
	if (!TestNotNull(TEXT("Undo publishes the restored facility instance"), First)) return false;
	Info->Tick(1.f);
	TestEqual(TEXT("Undo restores the installed capacity"), First->GetBaseFacilitySpec().QueueSettings.MaxWaitingTeams, 6);
	TestEqual(TEXT("Undo refresh reaches the displayed capacity"), Teams->GetText().ToString(), FString(TEXT("6")));
	TestTrue(TEXT("Queue edit can be redone"), Zones->RedoConstruction(UndoError));
	First = Zones->FindFacility(FirstId);
	if (!TestNotNull(TEXT("Redo publishes the restored facility instance"), First)) return false;
	Info->Tick(1.f);
	TestEqual(TEXT("Redo refresh reaches the displayed capacity"), Teams->GetText().ToString(), FString(TEXT("8")));
	Zones->SetChangePhase(EFFZSChangePhase::LiveOperation);
	Info->Tick(1.f);
	TestTrue(TEXT("Operation still displays queue settings"), Section->IsVisible());
	TestTrue(TEXT("Operation prevents edits and disables apply"), Teams->GetIsReadOnly() && !Apply->GetIsEnabled());
	FFFZSQueueSettings Other = Applied; Other.MaxWaitingTeams = 9;
	TestFalse(TEXT("Direct calls cannot bypass the preparation-time restriction"), Info->ApplySelectedQueueSettings(First->GetFacilityInstanceId(), Other));
	Zones->SetChangePhase(EFFZSChangePhase::InitialSetup);
	Info->Tick(1.f);
	Info->SetPauseMenuPresentationSuppressed(true);
	TestFalse(TEXT("Paused inspector cannot apply an old request"), Info->ApplySelectedQueueSettings(First->GetFacilityInstanceId(), Other));
	Info->SetPauseMenuPresentationSuppressed(false);
	if (!TestTrue(TEXT("Inspector opens the audience-only main stage"), Info->SelectFacility(Controller, MainStage, EFF_FacilityInfoContext::Build))) return false;
	TestFalse(TEXT("Audience-only main stage has no queue settings"), Panel->GetCurrentViewData().bHasQueueSettings);
	TestEqual(TEXT("Audience-only main stage hides all queue controls"), Section->GetVisibility(), ESlateVisibility::Collapsed);
	TestFalse(TEXT("Inspector cannot recreate the removed main-stage queue"), Info->ApplySelectedQueueSettings(MainStage->GetFacilityInstanceId(), Applied));
	if (!TestTrue(TEXT("Inspector reopens the current first facility after history"), Info->SelectFacility(Controller, First, EFF_FacilityInfoContext::Build))) return false;
	Panel->SetInfoPage(EFF_FacilityInfoPage::Operation);
	TestFalse(TEXT("Operation facts no longer include queue controls"), Section->IsVisible());
	Panel->SetInfoPage(EFF_FacilityInfoPage::Synergy);
	TestEqual(TEXT("Unrelated pages hide queue controls"), Section->GetVisibility(), ESlateVisibility::Collapsed);
	Panel->SetInfoPage(EFF_FacilityInfoPage::Queue);
	TestTrue(TEXT("Repeated page changes reuse the same inputs"), Panel->GetWidgetFromName(TEXT("Editable_FacilityQueueTeams")) == Teams);

	if (!FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
	{
		const FVector2D Size(1280, 720);
		FWidgetRenderer Renderer(false, true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, true));
		if (!TestNotNull(TEXT("Facility queue capture target"), Target.Get())) return false;
		Renderer.DrawWidget(Target.Get(), Overlay, Size, 0.f, false);
		if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
		FlushRenderingCommands();
		// All queue controls must be usable immediately, without scrolling past
		// facility details to reach the final field or the Apply button.
		for (int32 Frame = 0; Frame < 3; ++Frame) Renderer.DrawWidget(Target.Get(), Overlay, Size, .016f, false);
		FlushRenderingCommands();
		const FGeometry& ApplyGeometry = Apply->GetCachedGeometry();
		TestTrue(TEXT("Apply button has a usable rendered target"), ApplyGeometry.GetLocalSize().X > 100 && ApplyGeometry.GetLocalSize().Y > 20);
		const FSlateRect CardBounds = Card->GetCachedGeometry().GetRenderBoundingRect();
		const FSlateRect FooterBounds = Footer->GetCachedGeometry().GetRenderBoundingRect();
		TestTrue(TEXT("Queue controls need no scrolling"), FMath::IsNearlyZero(Scroll->GetScrollOffset(), 1.f));
		for (UWidget* Control : {static_cast<UWidget*>(Teams), static_cast<UWidget*>(Spacing), static_cast<UWidget*>(Width), static_cast<UWidget*>(Apply)})
		{
			const FSlateRect Bounds = Control->GetCachedGeometry().GetRenderBoundingRect();
			TestTrue(*FString::Printf(TEXT("%s is completely visible above the action footer"), *Control->GetName()),
				Bounds.Left >= CardBounds.Left && Bounds.Right <= CardBounds.Right &&
				Bounds.Top >= CardBounds.Top && Bounds.Bottom <= FooterBounds.Top + 1.f &&
				Bounds.Bottom <= Size.Y && Bounds.Bottom > Bounds.Top);
		}
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
		if (!TestTrue(TEXT("Rendered queue panel supplies pixels"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags))) return false;
		if (!TestEqual(TEXT("Capture covers the full viewport"), Pixels.Num(), 1280 * 720)) return false;
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(1280, 720, Pixels, Png);
		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/FacilityInfo"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString Path = Directory / TEXT("QueueSettings.png");
		TestTrue(TEXT("Facility queue settings screenshot saved"), FFileHelper::SaveArrayToFile(Png, *Path));
		AddInfo(FPaths::ConvertRelativePathToFull(Path));
	}
	return true;
}

#endif
