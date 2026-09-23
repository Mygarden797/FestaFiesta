// FF_PrepProgramFacilityNavigationTests.cpp
// 역할: 준비 프로그램의 시설 찾기·열린 화면의 가격 갱신·닫힌 화면의 구독 해제를 자동 검증한다.

// Exercises the saved program WBP against real facility/program content in an isolated game instance.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Blueprint/WidgetTree.h"
#include "Building/FF_BuildCatalogModel.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "DataTables/FF_FacilityRevenueTableRow.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StreamableManager.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Slate/WidgetRenderer.h"
#include "Tests/AutomationCommon.h"
#include "TextureResource.h"
#include "UI/PrepHUD/Panels/FF_PrepChoiceButton.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelProgram.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SOverlay.h"

namespace FFPrepProgramFacilityNavigationTest
{
	class FPlayerlessInitialization
	{
	public:
		explicit FPlayerlessInitialization(UWidgetBlueprintGeneratedClass& InClass)
			: Class(InClass), bPrevious(Class.bCanCallInitializedWithoutPlayerContext)
		{
			Class.bCanCallInitializedWithoutPlayerContext = true;
		}
		~FPlayerlessInitialization() { Class.bCanCallInitializedWithoutPlayerContext = bPrevious; }
	private:
		UWidgetBlueprintGeneratedClass& Class;
		bool bPrevious;
	};

	// 촬영은 호출자가 소유한 화면을 잠시 빌린다. 촬영 완료가 패널 종료로 이어지면 안 된다.
	bool CaptureGuide(FAutomationTestBase& Test, UFF_PrepPanelProgram& Panel, const TSharedRef<SWidget>& PresentedPanel)
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi"))) { return true; }
		const FIntPoint Size(880, 600);
		FWidgetRenderer Renderer(false, true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(
			FWidgetRenderer::CreateTargetFor(FVector2D(Size.X, Size.Y), TF_Bilinear, true));
		if (!Test.TestNotNull(TEXT("Program guidance render target"), Target.Get())) { return false; }
		// WrapBox learns its allotted width on its first paint/tick. Capture the settled
		// layout instead of the provisional desired height that stacks every card.
		for (int32 Frame = 0; Frame < 3; ++Frame)
		{
			Panel.ForceLayoutPrepass();
			Renderer.DrawWidget(Target.Get(), PresentedPanel, FVector2D(Size.X, Size.Y), 1.f / 60.f, false);
		}
		UScrollBox* Scroll = Cast<UScrollBox>(Panel.GetWidgetFromName(TEXT("ScrollBox_Content")));
		UPanelWidget* Guide = Cast<UPanelWidget>(Panel.GetWidgetFromName(TEXT("Box_CatalogChoices")));
		if (!Test.TestNotNull(TEXT("Visible program content scroll viewport"), Scroll) ||
			!Test.TestNotNull(TEXT("Rendered compatible-facility actions"), Guide) ||
			!Test.TestTrue(TEXT("Rendered facility guidance has an action"), Guide->GetChildrenCount() > 0)) { return false; }
		UWidget* Choice = Guide->GetChildAt(0);
		const FSlateRect ScrollRect = Scroll->GetCachedGeometry().GetLayoutBoundingRect();
		const FSlateRect ChoiceRect = Choice->GetCachedGeometry().GetLayoutBoundingRect();
		Test.TestTrue(TEXT("Facility navigation action keeps a usable rendered size"),
			ChoiceRect.GetSize().X >= 24.f && ChoiceRect.GetSize().Y >= 24.f);
		Test.TestTrue(TEXT("Facility navigation action is visible without scrolling after selecting a program"),
			ChoiceRect.Left >= ScrollRect.Left - 0.5f && ChoiceRect.Right <= ScrollRect.Right + 0.5f &&
			ChoiceRect.Top >= ScrollRect.Top - 0.5f && ChoiceRect.Bottom <= ScrollRect.Bottom + 0.5f);
		Test.AddInfo(FString::Printf(TEXT("Program guide bounds: action=(%.1f, %.1f)-(%.1f, %.1f), viewport=(%.1f, %.1f)-(%.1f, %.1f)"),
			ChoiceRect.Left, ChoiceRect.Top, ChoiceRect.Right, ChoiceRect.Bottom,
			ScrollRect.Left, ScrollRect.Top, ScrollRect.Right, ScrollRect.Bottom));
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags Flags;
		Flags.SetLinearToGamma(false);
		if (!Test.TestTrue(TEXT("Actual program WBP renders complete pixels"),
			Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags) &&
			Pixels.Num() == Size.X * Size.Y)) { return false; }
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString Path = FPaths::Combine(Directory, TEXT("PrepProgramFacilityGuide.png"));
		const bool bSaved = FFileHelper::SaveArrayToFile(Png, *Path);
		Test.TestTrue(TEXT("Program facility guidance screenshot is saved"), bSaved);
		Test.AddInfo(FString::Printf(TEXT("Program facility guidance capture: %s"),
			*FPaths::ConvertRelativePathToFull(Path)));
		return bSaved;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepProgramFacilityNavigationTest,
	"FestaFiesta.UI.PrepProgram.FacilityNavigation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_PrepProgramFacilityNavigationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFPrepProgramFacilityNavigationTest;
	UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
	TGuardValue<bool> GroundChecks(Settings->bEnableGroundChecks, false);
	TGuardValue<bool> CollisionChecks(Settings->bEnableWorldCollisionChecks, false);
	TGuardValue<bool> NavigationChecks(Settings->bEnableNavigationProjectionChecks, false);
	TGuardValue<bool> ProductionPolicies(Settings->bRequireProductionPolicyDomains, false);
	TGuardValue<EFFZSChangePhase> InitialPhase(Settings->InitialPhase, EFFZSChangePhase::InitialSetup);
	const TSharedRef<SOverlay> ViewportOverlay = SNew(SOverlay);
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game))
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Fixture.GetTestWorld();
	UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
	UFF_BuildCatalogSubsystem* Catalog = Game ? Game->GetSubsystem<UFF_BuildCatalogSubsystem>() : nullptr;
	UFF_FestivalProgressionSubsystem* Progression = Game ? Game->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
	UFF_BuildModeSubsystem* Build = World ? World->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
	UFFZSWorldSubsystem* Zones = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	auto* PanelClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepPanelProgram>(nullptr,
		TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelProgram.WBP_FF_PrepPanelProgram_C")));
	auto* CardClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepProgramCard>(nullptr,
		TEXT("/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepProgramCard.WBP_FF_PrepProgramCard_C")));
	auto* ChoiceClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepChoiceButton>(nullptr,
		FFBuildUIAssetPaths::PrepChoiceButtonClass));
	const UFF_FestivalDefinition* Festival = LoadObject<UFF_FestivalDefinition>(nullptr,
		TEXT("/Game/FestaFiesta/Data/Festivals/DA_Festival_Busan.DA_Festival_Busan"));
	UFFZSFacilityDefinition* WorkshopFacility = LoadObject<UFFZSFacilityDefinition>(nullptr,
		TEXT("/Game/FestaFiesta/Building/FF_Data_Building/Booth/PDA_FFZS_Booth_CraftExperience_SandArt.PDA_FFZS_Booth_CraftExperience_SandArt"));
	UFFZSFacilityDefinition* YogaFacility = LoadObject<UFFZSFacilityDefinition>(nullptr,
		TEXT("/Game/FestaFiesta/Building/FF_Data_Building/Performance/PDA_FFZS_Performance_ExperienceStage_Experience.PDA_FFZS_Performance_ExperienceStage_Experience"));
	if (!TestNotNull(TEXT("Isolated game instance"), Game) || !TestNotNull(TEXT("Production catalog"), Catalog) ||
		!TestNotNull(TEXT("Production progression"), Progression) || !TestNotNull(TEXT("Production build mode"), Build) ||
		!TestNotNull(TEXT("Production zone subsystem"), Zones) || !TestNotNull(TEXT("Saved program panel WBP"), PanelClass) ||
		!TestNotNull(TEXT("Saved program card WBP"), CardClass) || !TestNotNull(TEXT("Saved choice WBP"), ChoiceClass) ||
		!TestNotNull(TEXT("Source Busan festival"), Festival) || !TestNotNull(TEXT("Source workshop facility"), WorkshopFacility) ||
		!TestNotNull(TEXT("Source yoga facility"), YogaFacility)) { return false; }

	// Keep the real content conversion, but scope discovery to two definitions and the fixture's GI.
	FFBuildCatalogModel::BeginNextRequest(Catalog->RequestGeneration);
	if (Catalog->ActiveLoadHandle.IsValid() && !Catalog->ActiveLoadHandle->HasLoadCompleted())
	{
		Catalog->ActiveLoadHandle->CancelHandle();
	}
	Catalog->ActiveLoadHandle.Reset();
	Catalog->RequestedAssetIds.Reset();
	Catalog->Entries.Reset();
	Catalog->EntryIndexByDefinitionId.Reset();
	for (UFFZSFacilityDefinition* Definition : { WorkshopFacility, YogaFacility })
	{
		FFF_BuildCatalogEntry Entry;
		FString Failure;
		if (!TestTrue(*FString::Printf(TEXT("Source facility produces a valid construction entry: %s"), *Definition->GetName()),
			Catalog->TryMakeEntry(Definition->GetPrimaryAssetId(), *Definition, Entry, Failure)))
		{
			AddError(Failure);
			return false;
		}
		Catalog->EntryIndexByDefinitionId.Add(Entry.FacilityDefinitionId, Catalog->Entries.Add(Entry));
	}
	Catalog->RebuildCategoryGroups();
	Catalog->FinishRefresh(EFF_BuildCatalogLoadState::Ready);
	Progression->StartFestival(Festival);

	const bool bPackageWasDirty = PanelClass->GetOutermost()->IsDirty();
	FPlayerlessInitialization PanelInitialization(*PanelClass);
	FPlayerlessInitialization CardInitialization(*CardClass);
	FPlayerlessInitialization ChoiceInitialization(*ChoiceClass);
	TStrongObjectPtr<UFF_PrepPanelProgram> Panel(CreateWidget<UFF_PrepPanelProgram>(World, PanelClass));
	if (!TestNotNull(TEXT("Saved program panel initializes without asset regeneration"), Panel.Get())) { return false; }
	// 실제 HUD처럼 열린 동안 Slate 화면도 소유한다. UObject만 유지하면 촬영 함수 반환 시
	// SObjectWidget이 파괴되어 NativeDestruct가 구독을 해제한다. 아래 close 정리보다 먼저 선언해
	// 패널을 연 뒤 조기 반환해도 패널을 닫고 Slate, UObject, 테스트 월드 순서로 정리한다.
	const TSharedRef<SWidget> PresentedPanel = Panel->TakeWidget();
	UPanelWidget* Guide = Cast<UPanelWidget>(Panel->GetWidgetFromName(TEXT("Box_CatalogChoices")));
	UTextBlock* Notice = Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("Text_FacilityNotice")));
	if (!TestNotNull(TEXT("Native initialization supplies compatible-facility guide container"), Guide) ||
		!TestNotNull(TEXT("Saved facility notice"), Notice) ||
		!TestNotNull(TEXT("Guide is attached to the existing designer hierarchy"), Guide->GetParent())) { return false; }
	TestEqual(TEXT("Guide sits immediately after its facility explanation"),
		Guide->GetParent()->GetChildIndex(Guide), Guide->GetParent()->GetChildIndex(Notice) + 1);
	TestEqual(TEXT("Runtime initialization does not dirty the saved WBP"), PanelClass->GetOutermost()->IsDirty(), bPackageWasDirty);
	Panel->InitializePanel(nullptr, EFF_PrepHudTab::Program, FText::FromString(TEXT("프로그램")));
	Panel->HandlePanelOpened();
	ON_SCOPE_EXIT { Panel->HandlePanelClosed(); Build->ExitBuildMode(); };
	const int32 WorkshopIndex = Panel->Catalog.IndexOfByPredicate([](const FFF_ProgramCard& Card)
	{
		return Card.ProgramId == TEXT("Program_SandArtWorkshop");
	});
	const int32 YogaIndex = Panel->Catalog.IndexOfByPredicate([](const FFF_ProgramCard& Card)
	{
		return Card.ProgramId == TEXT("Program_SunsetYoga");
	});
	if (!TestTrue(TEXT("Both source programs are shown by the actual panel"),
		Panel->CardWidgets.IsValidIndex(WorkshopIndex) && Panel->CardWidgets.IsValidIndex(YogaIndex))) { return false; }
	TestEqual(TEXT("No card selection hides construction guidance"), Guide->GetVisibility(), ESlateVisibility::Collapsed);
	UButton* WorkshopButton = Cast<UButton>(Panel->CardWidgets[WorkshopIndex]->GetWidgetFromName(TEXT("Button_Root")));
	if (!TestNotNull(TEXT("Actual workshop card button"), WorkshopButton)) { return false; }
	WorkshopButton->OnClicked.Broadcast();
	TestEqual(TEXT("Clicking the real card selects its program"), Panel->SelectedCardIndex, WorkshopIndex);
	TestEqual(TEXT("Only compatible workshop facilities are offered"), Guide->GetChildrenCount(), 1);
	TestEqual(TEXT("Workshop guide becomes visible"), Guide->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	TestTrue(TEXT("The guide explains the construction catalog with the authored facility name"),
		Notice->GetText().ToString().Contains(WorkshopFacility->Spec.DisplayName.ToString()));
	if (!TestEqual(TEXT("Workshop has exactly one compatible source entry"), Panel->CompatibleCatalogEntries.Num(), 1)) { return false; }
	TestEqual(TEXT("Workshop resolves its exact source definition"),
		Panel->CompatibleCatalogEntries[0].FacilityDefinitionId, WorkshopFacility->Spec.FacilityDefinitionId);
	if (!CaptureGuide(*this, *Panel, PresentedPanel)) { return false; }
	if (!TestEqual(TEXT("Capturing guidance keeps the open panel subscribed to festival changes"),
		Panel->BoundProgression.Get(), Progression)) { return false; }

	WorkshopButton->OnClicked.Broadcast();
	TestEqual(TEXT("Deselecting clears stale guide buttons"), Guide->GetChildrenCount(), 0);
	TestEqual(TEXT("Deselecting hides the guide"), Guide->GetVisibility(), ESlateVisibility::Collapsed);
	TestTrue(TEXT("Deselecting clears the facility explanation"), Notice->GetText().IsEmpty());
	UButton* YogaButton = Cast<UButton>(Panel->CardWidgets[YogaIndex]->GetWidgetFromName(TEXT("Button_Root")));
	if (!TestNotNull(TEXT("Actual yoga card button"), YogaButton)) { return false; }
	YogaButton->OnClicked.Broadcast();
	if (!TestEqual(TEXT("Switching programs rebuilds compatible choices"), Panel->CompatibleCatalogEntries.Num(), 1)) { return false; }
	TestEqual(TEXT("Yoga points to its stage, not the earlier workshop"),
		Panel->CompatibleCatalogEntries[0].FacilityDefinitionId, YogaFacility->Spec.FacilityDefinitionId);
	TestTrue(TEXT("Switching programs replaces the facility explanation"),
		Notice->GetText().ToString().Contains(YogaFacility->Spec.DisplayName.ToString()));

	// A minimal local viewport enables the normal build-session path without showing a window.
	FWorldContext* Context = GEngine->GetWorldContextFromWorld(World);
	if (!TestNotNull(TEXT("Isolated world context"), Context)) { return false; }
	UGameViewportClient* Viewport = NewObject<UGameViewportClient>(GEngine);
	Context->GameViewport = Viewport;
	Viewport->Init(*Context, Game, false);
	Viewport->SetViewportOverlayWidget(nullptr, ViewportOverlay);
	FString PlayerError;
	ULocalPlayer* Player = Game->CreateLocalPlayer(0, PlayerError, false);
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	if (!TestNotNull(*FString::Printf(TEXT("Local player: %s"), *PlayerError), Player) ||
		!TestNotNull(TEXT("Local controller"), Controller)) { return false; }
	Controller->SetPlayer(Player);
	World->AddController(Controller);
	AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
	if (!TestNotNull(TEXT("Construction zone"), Zone) ||
		!TestTrue(TEXT("Zone uses the accepted preset"), Zone->ApplyAcceptedZonePreset(TEXT("GZ_DadaPocha"))) ||
		!TestTrue(TEXT("Construction zone registers"), Zones->RegisterZone(Zone))) { return false; }
	Zones->SetChangePhase(EFFZSChangePhase::InitialSetup);
	Build->SetExternalCatalogPresentationActive(true);
	if (!TestEqual(TEXT("Yoga produces one construction action"), Panel->CatalogChoiceWidgets.Num(), 1)) { return false; }
	UButton* CatalogButton = Cast<UButton>(Panel->CatalogChoiceWidgets[0]->GetWidgetFromName(TEXT("Button_Root")));
	if (!TestNotNull(TEXT("Actual construction navigation button"), CatalogButton)) { return false; }
	const int32 GoldBefore = Progression->GetGold();
	CatalogButton->OnClicked.Broadcast();
	TestEqual(TEXT("Guide action opens the construction catalog"), Build->GetBuildModeState(), EFF_BuildModeState::CatalogOpen);
	TestEqual(TEXT("Guide action focuses the exact compatible facility"),
		Build->GetPendingCatalogFacilityId(), YogaFacility->Spec.FacilityDefinitionId);
	TestEqual(TEXT("Browsing construction guidance does not buy a facility"), Progression->GetGold(), GoldBefore);

	// Keep the visible WBP open and edit only transient balancing data. No manual refresh
	// occurs after AddRow: the table change delegate must invalidate the selected quote.
	Build->ExitBuildMode();
	FFFZSGridPlacement YogaPlacement;
	YogaPlacement.OriginCell = FIntPoint(1000, 1000);
	const FFFZSCommand InstallYoga = UFFZSGridLibrary::MakeInstallCommand(
		YogaFacility->Spec, TEXT("GZ_DadaPocha"), YogaPlacement, Zones->GetWorldRevision());
	const FFFZSCommandResult InstallResult = Zones->ExecuteCommand(InstallYoga);
	if (!TestTrue(*FString::Printf(TEXT("The quote's real facility installs: %s"), *InstallResult.Message.ToString()), InstallResult.bSucceeded)) return false;
	const FGuid InstalledYogaId = InstallYoga.FacilityInstanceId;
	TStrongObjectPtr<UDataTable> RevenueTable(NewObject<UDataTable>());
	RevenueTable->RowStruct = FFF_FacilityRevenueTableRow::StaticStruct();
	FFF_FacilityRevenueTableRow RevenueRow;
	RevenueRow.ProgramFeesPerPerson.Add(TEXT("Program_SunsetYoga"), 25);
	RevenueTable->AddRow(YogaFacility->Spec.FacilityDefinitionId, RevenueRow);
	TStrongObjectPtr<UFF_FestivalDefinition> IsolatedFestival(DuplicateObject<UFF_FestivalDefinition>(Festival, Game));
	IsolatedFestival->FacilityRevenueTable = RevenueTable.Get();
	Progression->StartFestival(IsolatedFestival.Get());
	Panel->SelectedCardIndex = YogaIndex;
	Panel->RebuildFacilityChoices();
	Panel->RefreshAll();
	if (!TestEqual(TEXT("The open panel listens to the replacement revenue table"),
		Panel->BoundRevenueTable.Get(), RevenueTable.Get())) { return false; }
	if (!TestEqual(TEXT("The real panel finds its installed yoga facility"), Panel->FacilityOptions.Num(), 1)) return false;
	if (!TestTrue(TEXT("The installed facility selection is valid"), Panel->FacilityOptions.IsValidIndex(Panel->SelectedFacilityIndex))) return false;
	TestEqual(TEXT("The sole installed facility is selected"), Panel->FacilityOptions[Panel->SelectedFacilityIndex].FacilityInstanceId, InstalledYogaId);
	const int32 YogaCapacity = Panel->Catalog[YogaIndex].Capacity;
	TestEqual(TEXT("The open panel initially quotes the facility's authored fee"), Panel->GetQuote().MaxRevenue, YogaCapacity * 25);
	RevenueRow.ProgramFeesPerPerson.Add(TEXT("Program_SunsetYoga"), 45);
	RevenueTable->AddRow(YogaFacility->Spec.FacilityDefinitionId, RevenueRow);
	TestEqual(TEXT("A table edit automatically updates the already open program quote"), Panel->GetQuote().MaxRevenue, YogaCapacity * 45);
	TestEqual(TEXT("A live fee refresh preserves the selected program"), Panel->SelectedCardIndex, YogaIndex);
	TestEqual(TEXT("A live fee refresh preserves the selected installed facility"), Panel->SelectedFacilityIndex, 0);
	RevenueRow.ProgramFeesPerPerson.Add(TEXT("Program_SunsetYoga"), 0);
	RevenueTable->AddRow(YogaFacility->Spec.FacilityDefinitionId, RevenueRow);
	TestEqual(TEXT("An open quote immediately reflects a newly free program"), Panel->GetQuote().MaxRevenue, 0);
	Panel->HandlePanelClosed();
	RevenueRow.ProgramFeesPerPerson.Add(TEXT("Program_SunsetYoga"), 100);
	RevenueTable->AddRow(YogaFacility->Spec.FacilityDefinitionId, RevenueRow);
	TestEqual(TEXT("Closing the panel detaches the table-change listener"), Panel->GetQuote().MaxRevenue, 0);
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
