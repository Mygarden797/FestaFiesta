// FF_PrepProgramFacilityNavigationTests.cpp
// 역할: 준비 프로그램의 시설 찾기·열린 화면의 가격 갱신·닫힌 화면의 구독 해제를 자동 검증한다.

// Exercises the saved program WBP against real facility/program content in an isolated game instance.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Building/FF_BuildCatalogModel.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Camera/FF_FestivalPlayerController.h"
#include "Camera/FF_OverviewCameraPawn.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
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
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Tests/FF_ProjectZoneTestFixture.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
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
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/PrepHUD/Panels/FF_PrepChoiceButton.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelProgram.h"
#include "UI/PrepHUD/Panels/FF_PrepProgramCatalogChoice.h"
#include "UI/PrepHUD/Panels/FF_PrepProgramFacilityRow.h"
#include "UObject/StrongObjectPtr.h"
#include "UnrealClient.h"
#include "Widgets/SOverlay.h"

namespace FFPrepProgramFacilityNavigationTest
{
	// 창을 띄우지 않고 OS 포커스만 제공한다. 대상 명령과 카메라 이동은 실제 구현을 사용한다.
	class FLocateViewport final : public FViewport
	{
	public:
		explicit FLocateViewport(FViewportClient* Client) : FViewport(Client) {}
		virtual FIntPoint GetSizeXY() const override { return FIntPoint(800, 600); }
		virtual void* GetWindow() override { return nullptr; }
		virtual void MoveWindow(int32, int32, int32, int32) override {}
		virtual void Destroy() override {}
		virtual bool SetUserFocus(bool) override { return true; }
		virtual bool HasFocus() const override { return bFocused; }
		virtual bool IsForegroundWindow() const override { return true; }
		virtual bool KeyState(FKey) const override { return false; }
		virtual int32 GetMouseX() const override { return 400; }
		virtual int32 GetMouseY() const override { return 300; }
		virtual void GetMousePos(FIntPoint& Position, bool = true) override { Position = FIntPoint(400, 300); }
		virtual void SetMouse(int32, int32) override {}
		virtual void ProcessInput(float) override {}
		virtual FVector2D VirtualDesktopPixelToViewport(FIntPoint Point) const override
		{
			return FVector2D(Point) / FVector2D(GetSizeXY());
		}
		virtual FIntPoint ViewportToVirtualDesktopPixel(FVector2D Point) const override
		{
			return FIntPoint(FMath::RoundToInt(Point.X * 800), FMath::RoundToInt(Point.Y * 600));
		}
		virtual void InvalidateDisplay() override {}
		virtual FViewportFrame* GetViewportFrame() override { return nullptr; }
		bool bFocused = true;
	};

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
		const FIntPoint Size(1120, 600);
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
		UScrollBox* Scroll = Cast<UScrollBox>(Panel.GetWidgetFromName(TEXT("ScrollBox_Body")));
		if (!Scroll)
		{
			Scroll = Cast<UScrollBox>(Panel.GetWidgetFromName(TEXT("ScrollBox_Content")));
		}
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
	UFF_FestivalViewModelSubsystem* ViewModel = Game ? Game->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;
	UFF_BuildModeSubsystem* Build = World ? World->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
	UFFZSWorldSubsystem* Zones = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	auto* PanelClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepPanelProgram>(nullptr,
		TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelProgram.WBP_FF_PrepPanelProgram_C")));
	auto* CardClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepProgramCard>(nullptr,
		TEXT("/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepProgramCard.WBP_FF_PrepProgramCard_C")));
	const UFF_PrepPanelProgram* PanelDefaults = PanelClass ? PanelClass->GetDefaultObject<UFF_PrepPanelProgram>() : nullptr;
	auto* ChoiceClass = PanelDefaults ? Cast<UWidgetBlueprintGeneratedClass>(UFF_PrepPanelProgram::ResolveWidgetClass(
		PanelDefaults->FacilityChoiceClass, PanelDefaults->FacilityChoiceFallbackPath)) : nullptr;
	auto* CatalogChoiceClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepProgramCatalogChoice>(nullptr,
		TEXT("/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepProgramCatalogChoice.WBP_FF_PrepProgramCatalogChoice_C")));
	auto* FacilityRowClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepProgramFacilityRow>(nullptr,
		TEXT("/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepProgramFacilityRow.WBP_FF_PrepProgramFacilityRow_C")));
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
		!TestNotNull(TEXT("프로그램 바로가기 WBP"), CatalogChoiceClass) ||
		!TestNotNull(TEXT("설치 시설 행 WBP"), FacilityRowClass) || !TestNotNull(TEXT("프로그램 표시 모델"), ViewModel) ||
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
	FPlayerlessInitialization CatalogChoiceInitialization(*CatalogChoiceClass);
	FPlayerlessInitialization FacilityRowInitialization(*FacilityRowClass);
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
	TestFalse(TEXT("시설이 없을 때 안내 설명이 보임"), Notice->GetText().IsEmpty());
	UTextBlock* WorkshopShortcut = Cast<UTextBlock>(Panel->CatalogChoiceWidgets[0]->GetWidgetFromName(TEXT("Text_Label")));
	if (!TestNotNull(TEXT("시설 바로가기 이름"), WorkshopShortcut)) return false;
	TestEqual(TEXT("버튼에 실제 시설 이름을 표시"), WorkshopShortcut->GetText().ToString(), WorkshopFacility->Spec.DisplayName.ToString());
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
	UTextBlock* YogaShortcut = Cast<UTextBlock>(Panel->CatalogChoiceWidgets[0]->GetWidgetFromName(TEXT("Text_Label")));
	if (!TestNotNull(TEXT("다른 프로그램의 바로가기 이름"), YogaShortcut)) return false;
	TestEqual(TEXT("요가 선택 시 대응 시설로 변경"), YogaShortcut->GetText().ToString(), YogaFacility->Spec.DisplayName.ToString());

	// A minimal local viewport enables the normal build-session path without showing a window.
	FWorldContext* Context = GEngine->GetWorldContextFromWorld(World);
	if (!TestNotNull(TEXT("Isolated world context"), Context)) { return false; }
	UGameViewportClient* Viewport = NewObject<UGameViewportClient>(GEngine);
	Context->GameViewport = Viewport;
	Viewport->Init(*Context, Game, false);
	Viewport->SetViewportOverlayWidget(nullptr, ViewportOverlay);
	FLocateViewport CameraViewport(Viewport);
	Viewport->Viewport = &CameraViewport;
	ON_SCOPE_EXIT { Viewport->Viewport = nullptr; };
	FString PlayerError;
	ULocalPlayer* Player = Game->CreateLocalPlayer(0, PlayerError, false);
	AFF_FestivalPlayerController* Controller = World->SpawnActor<AFF_FestivalPlayerController>();
	if (!TestNotNull(*FString::Printf(TEXT("Local player: %s"), *PlayerError), Player) ||
		!TestNotNull(TEXT("Local controller"), Controller)) { return false; }
	Controller->SetPlayer(Player);
	Controller->PlayerInput = NewObject<UPlayerInput>(Controller);
	World->AddController(Controller);
	// Dev의 공통 테스트 구역을 사용하고, 기존 구역 제한·시설 허용 검증은 유지한다.
	const FName FixtureZoneId = FFProjectZoneTest::ZoneId();
	AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
	if (!TestNotNull(TEXT("Construction zone"), Zone) ||
		!TestTrue(TEXT("Zone uses the accepted preset"), Zone->ApplyAcceptedZonePreset(FixtureZoneId)) ||
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
		YogaFacility->Spec, FixtureZoneId, YogaPlacement, Zones->GetWorldRevision());
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
	TestNotEqual(TEXT("시설 설치 후에도 건축목록 바로가기 유지"), Guide->GetVisibility(), ESlateVisibility::Collapsed);
	if (!TestNotNull(TEXT("설치 시설 목록 영역"), Panel->VerticalBox_FacilityInstances.Get()) ||
		!TestEqual(TEXT("설치 시설을 행으로 표시"), Panel->FacilityRowWidgets.Num(), 1)) return false;
	UTextBlock* InstalledStatus = Cast<UTextBlock>(Panel->CatalogChoiceWidgets[0]->GetWidgetFromName(TEXT("Text_InstallationStatus")));
	if (!TestNotNull(TEXT("시설 종류의 설치 수 표시"), InstalledStatus)) return false;
	TestEqual(TEXT("설치 수는 실제 월드 시설 수"), InstalledStatus->GetText().ToString(), FString(TEXT("✓ 설치 1곳")));
	TestFalse(TEXT("존재하지 않는 시설 위치보기는 실패"), ViewModel->RequestLocateProgramFacility(FGuid::NewGuid()));
	UComboBoxString* FacilityCombo = Cast<UComboBoxString>(Panel->GetWidgetFromName(TEXT("Combo_Facilities")));
	UTextBlock* SelectedProgram = Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("Text_SelectedProgram")));
	UTextBlock* DayText = Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("Text_Day")));
	UTextBlock* EmptyText = Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("Text_EmptyCatalog")));
	if (!TestNotNull(TEXT("Saved sky panel facility dropdown"), FacilityCombo) ||
		!TestNotNull(TEXT("Selected program summary"), SelectedProgram) ||
		!TestNotNull(TEXT("Actual day label"), DayText) ||
		!TestNotNull(TEXT("Empty catalog explanation"), EmptyText)) return false;
	TestEqual(TEXT("One available facility is also selected in the actual dropdown"), FacilityCombo->GetSelectedIndex(), 1);
	TestEqual(TEXT("Summary follows the selected catalog name"), SelectedProgram->GetText().ToString(), Panel->Catalog[YogaIndex].DisplayName.ToString());
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

	// 표시 이후 시설이 사라진 경우를 UI 사본에 재현한다. 명령은 실제 소유자가 거부해야 한다.
	const int32 GoldBeforeRejectedSchedule = Progression->GetGold();
	Panel->FacilityOptions[Panel->SelectedFacilityIndex].FacilityInstanceId = FGuid::NewGuid();
	Panel->HandleScheduleClicked();
	TestEqual(TEXT("Failed scheduling does not spend money"), Progression->GetGold(), GoldBeforeRejectedSchedule);
	TestEqual(TEXT("Failed scheduling retains the program for retry"), Panel->SelectedCardIndex, YogaIndex);
	if (!TestTrue(TEXT("Failed scheduling leaves a valid refreshed facility choice"),
		Panel->FacilityOptions.IsValidIndex(Panel->SelectedFacilityIndex))) return false;
	TestEqual(TEXT("Failed scheduling refreshes a stale facility handle"),
		Panel->FacilityOptions[Panel->SelectedFacilityIndex].FacilityInstanceId, InstalledYogaId);
	TestFalse(TEXT("Failed scheduling displays a reason"), Panel->Text_BlockReason->GetText().IsEmpty());
	if (!TestNotNull(TEXT("새 완료 알림 영역"), Panel->Border_ScheduleToast.Get())) return false;
	TestEqual(TEXT("편성 실패는 성공 알림을 띄우지 않음"), Panel->Border_ScheduleToast->GetVisibility(), ESlateVisibility::Collapsed);

	// 같은 이름의 시설 두 곳이 있어도 콤보 순번은 실제 GUID에 연결되어야 한다.
	// Zone 셀은 1cm다. 고정 200셀 간격은 실제 무대와 대기열에 겹칠 수 있으므로
	// 첫 시설의 전체 예약 영역 폭만큼 떨어진 곳에 두 번째 시설을 둔다.
	const TArray<FFFZSCellRect> FirstReservedRects = UFFZSGridLibrary::GetReservedRects(YogaFacility->Spec, YogaPlacement);
	if (!TestFalse(TEXT("The first facility has placement regions"), FirstReservedRects.IsEmpty())) return false;
	int32 ReservedMinX = MAX_int32;
	int32 ReservedMaxX = MIN_int32;
	for (const FFFZSCellRect& Rect : FirstReservedRects)
	{
		ReservedMinX = FMath::Min(ReservedMinX, Rect.Min.X);
		ReservedMaxX = FMath::Max(ReservedMaxX, Rect.Max.X);
	}
	FFFZSGridPlacement SecondYogaPlacement;
	SecondYogaPlacement.OriginCell = YogaPlacement.OriginCell + FIntPoint(ReservedMaxX - ReservedMinX + 100, 0);
	if (!TestFalse(TEXT("The second fixture clears the first facility and its queue"),
		UFFZSGridLibrary::DoCellRectSetsOverlap(FirstReservedRects,
			UFFZSGridLibrary::GetReservedRects(YogaFacility->Spec, SecondYogaPlacement)))) return false;
	const FFFZSCommand InstallSecondYoga = UFFZSGridLibrary::MakeInstallCommand(
		YogaFacility->Spec, FixtureZoneId, SecondYogaPlacement, Zones->GetWorldRevision());
	const FFFZSCommandResult SecondInstallResult = Zones->ExecuteCommand(InstallSecondYoga);
	if (!TestTrue(*FString::Printf(TEXT("A second facility with the same authored name installs: %s"),
		*SecondInstallResult.Message.ToString()), SecondInstallResult.bSucceeded)) return false;
	// 열린 패널은 시설 설치 이벤트로 갱신되어야 한다. 수동 갱신 없이 두 번째 시설이 나타난다.
	if (!TestEqual(TEXT("Dropdown includes both installed facilities"), Panel->FacilityOptions.Num(), 2)) return false;
	if (!TestEqual(TEXT("같은 종류 시설도 별도 행으로 표시"), Panel->FacilityRowWidgets.Num(), 2)) return false;
	InstalledStatus = Cast<UTextBlock>(Panel->CatalogChoiceWidgets[0]->GetWidgetFromName(TEXT("Text_InstallationStatus")));
	if (!TestNotNull(TEXT("설치 후 갱신된 수량 칩"), InstalledStatus)) return false;
	TestEqual(TEXT("같은 시설 두 곳의 설치 수 반영"), InstalledStatus->GetText().ToString(), FString(TEXT("✓ 설치 2곳")));
	TestNotEqual(TEXT("동일 종류의 고정 번호 구분"), Panel->FacilityOptions[0].InstanceNumber, Panel->FacilityOptions[1].InstanceNumber);
	TestNotEqual(TEXT("동일 종류의 표시명 구분"), Panel->FacilityOptions[0].InstanceLabel.ToString(), Panel->FacilityOptions[1].InstanceLabel.ToString());
	const FString ExpectedZoneName = Zone->DisplayName.IsEmpty() ? FixtureZoneId.ToString() : Zone->DisplayName.ToString();
	for (const FFF_PrepFacilityOption& Option : Panel->FacilityOptions)
	{
		TestEqual(TEXT("시설 위치에는 좌표 없이 실제 구역명만 표시"), Option.LocationText.ToString(), ExpectedZoneName);
	}
	TestEqual(TEXT("Refresh retains the previous facility by GUID"),
		Panel->FacilityOptions[Panel->SelectedFacilityIndex].FacilityInstanceId, InstalledYogaId);
	TestNotEqual(TEXT("Duplicate facility names remain distinguishable dropdown entries"),
		FacilityCombo->GetOptionAtIndex(1), FacilityCombo->GetOptionAtIndex(2));
	FacilityCombo->SetSelectedIndex(0);
	TestEqual(TEXT("Placeholder clears the selected facility"), Panel->SelectedFacilityIndex, INDEX_NONE);
	TestEqual(TEXT("No facility selection hides an unconfirmed revenue amount"), Panel->GetQuote().MaxRevenue, INDEX_NONE);
	TestFalse(TEXT("Placeholder cannot schedule a program"), Panel->Button_Schedule->GetIsEnabled());
	const int32 SecondIndex = Panel->FacilityOptions.IndexOfByPredicate([&InstallSecondYoga](const FFF_PrepFacilityOption& Option)
	{
		return Option.FacilityInstanceId == InstallSecondYoga.FacilityInstanceId;
	});
	if (!TestTrue(TEXT("The second installed handle is in the dropdown"), SecondIndex != INDEX_NONE)) return false;
	UButton* SelectSecond = Cast<UButton>(Panel->FacilityRowWidgets[SecondIndex]->GetWidgetFromName(TEXT("Button_Select")));
	UButton* LocateSecond = Cast<UButton>(Panel->FacilityRowWidgets[SecondIndex]->GetWidgetFromName(TEXT("Button_Locate")));
	if (!TestNotNull(TEXT("시설 행 선택 버튼"), SelectSecond) || !TestNotNull(TEXT("시설 행 위치보기 버튼"), LocateSecond)) return false;
	SelectSecond->OnClicked.Broadcast();
	TestEqual(TEXT("행 선택이 실제 시설 GUID에 연결"), Panel->FacilityOptions[Panel->SelectedFacilityIndex].FacilityInstanceId, InstallSecondYoga.FacilityInstanceId);
	TestEqual(TEXT("행 선택이 기존 선택 컨트롤과 동기화"), FacilityCombo->GetSelectedIndex(), SecondIndex + 1);
	// 아직 관찰 카메라를 생성하지 않았다. 위치보기 실패가 편성 선택을 바꾸면 안 된다.
	LocateSecond->OnClicked.Broadcast();
	TestEqual(TEXT("위치보기 실패 후 프로그램 유지"), Panel->SelectedCardIndex, YogaIndex);
	TestEqual(TEXT("위치보기 실패 후 시설 유지"), Panel->FacilityOptions[Panel->SelectedFacilityIndex].FacilityInstanceId, InstallSecondYoga.FacilityInstanceId);
	const FFF_PrepFacilityOption SecondBeforeMove = Panel->FacilityOptions[SecondIndex];
	FFFZSCommand MoveSecond;
	MoveSecond.CommandId = FGuid::NewGuid();
	MoveSecond.TransactionId = FGuid::NewGuid();
	MoveSecond.Type = EFFZSCommandType::Move;
	MoveSecond.ExpectedWorldRevision = Zones->GetWorldRevision();
	MoveSecond.FacilityInstanceId = InstallSecondYoga.FacilityInstanceId;
	MoveSecond.FacilitySpec = YogaFacility->Spec;
	MoveSecond.ZoneId = FixtureZoneId;
	MoveSecond.Placement = SecondYogaPlacement;
	MoveSecond.Placement.OriginCell.Y += 100;
	const FFFZSCommandResult MoveResult = Zones->ExecuteCommand(MoveSecond);
	if (!TestTrue(*FString::Printf(TEXT("동일 시설 이동: %s"), *MoveResult.Message.ToString()), MoveResult.bSucceeded)) return false;
	TestEqual(TEXT("이동 후 시설 고정 번호 유지"), Panel->FacilityOptions[Panel->SelectedFacilityIndex].InstanceNumber, SecondBeforeMove.InstanceNumber);
	TestEqual(TEXT("이동 후 시설 표시명 유지"), Panel->FacilityOptions[Panel->SelectedFacilityIndex].InstanceLabel.ToString(), SecondBeforeMove.InstanceLabel.ToString());
	TestEqual(TEXT("같은 구역 안에서 이동하면 구역명 유지"), Panel->FacilityOptions[Panel->SelectedFacilityIndex].LocationText.ToString(), ExpectedZoneName);

	// 포커스가 없는 창에서 성공으로 오인하지 않고, 허용된 창에서는 실제 이동까지 이어진다.
	// 이 가짜 viewport는 OS 포커스만 대체하므로 Slate 버튼 -> viewport 포커스 전환은 PIE 확인 대상이다.
	if (!TestTrue(TEXT("카메라 입력 검사에 Slate 사용 가능"), FSlateApplication::IsInitialized())) return false;
	FSlateApplication& Slate = FSlateApplication::Get();
	const bool bPreviouslyActive = Slate.IsActive();
	Slate.OnApplicationActivationChanged(true);
	ON_SCOPE_EXIT { Slate.OnApplicationActivationChanged(bPreviouslyActive); };
	AFF_OverviewCameraPawn* Camera = World->SpawnActor<AFF_OverviewCameraPawn>();
	AFFZSFacilityActor* LocatedFacility = Zones->FindFacility(InstallSecondYoga.FacilityInstanceId);
	if (!TestNotNull(TEXT("위치보기의 실제 관찰 카메라"), Camera) ||
		!TestNotNull(TEXT("이동한 실제 대상 시설"), LocatedFacility)) return false;
	Camera->CameraSettings.bUseZoneBounds = false;
	Camera->CameraSettings.bConstrainFocusToZones = false;
	Camera->CameraSettings.bFollowGroundHeight = false;
	Camera->CameraSettings.bEnableEdgeScroll = false;
	const FVector TargetLocation = LocatedFacility->GetActorLocation();
	Camera->InitializeOverview(TargetLocation + FVector(1000, 1000, 0), 4000.f);
	constexpr float DeltaSeconds = 1.f / 60.f;
	Camera->Tick(DeltaSeconds);
	CameraViewport.bFocused = false;
	TestFalse(TEXT("뷰포트 포커스 없이 카메라 이동 성공을 반환하지 않음"),
		ViewModel->RequestLocateProgramFacility(InstallSecondYoga.FacilityInstanceId));
	CameraViewport.bFocused = true;
	if (!TestTrue(TEXT("설치 시설 GUID로 위치보기 요청 성공"),
		ViewModel->RequestLocateProgramFacility(InstallSecondYoga.FacilityInstanceId))) return false;
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		Controller->PlayerTick(DeltaSeconds);
		Camera->Tick(DeltaSeconds);
	}
	TestTrue(TEXT("Controller 입력 검사 후에도 실제 카메라가 선택 시설 위치에 도달"),
		Camera->GetCameraState().FocusLocation.Equals(TargetLocation, 1.f));
	TestEqual(TEXT("위치보기는 편성 대상 선택을 변경하지 않음"),
		Panel->FacilityOptions[Panel->SelectedFacilityIndex].FacilityInstanceId, InstallSecondYoga.FacilityInstanceId);
	Panel->HandleBuildCatalogChanged();
	TestEqual(TEXT("Rebuilding dropdown options preserves the selected second handle"),
		Panel->FacilityOptions[Panel->SelectedFacilityIndex].FacilityInstanceId, InstallSecondYoga.FacilityInstanceId);
	Panel->HandleScheduleClicked();
	const TArray<FFF_ScheduledProgram> Scheduled = Progression->GetScheduledPrograms();
	if (!TestEqual(TEXT("Dropdown submission creates exactly one schedule"), Scheduled.Num(), 1)) return false;
	TestEqual(TEXT("Schedule command uses the dropdown facility GUID"), Scheduled[0].FacilityInstanceId, InstallSecondYoga.FacilityInstanceId);
	TestEqual(TEXT("성공 후 방금 편성한 프로그램을 요약에서 확인"), Panel->SelectedCardIndex, YogaIndex);
	TestEqual(TEXT("완료 요약은 해당 프로그램 이름"), SelectedProgram->GetText().ToString(), Panel->Catalog[YogaIndex].DisplayName.ToString());
	TestFalse(TEXT("이미 편성한 시설에서 중복 구매 차단"), Panel->Button_Schedule->GetIsEnabled());
	TestTrue(TEXT("성공 알림에 정확한 프로그램 이름 표시"),
		(Panel->Text_ScheduleToastTitle->GetText().ToString() + Panel->Text_ScheduleToastDetail->GetText().ToString())
		.Contains(Panel->Catalog[YogaIndex].DisplayName.ToString()));
	TestNotEqual(TEXT("편성 성공 알림이 나타남"), Panel->Border_ScheduleToast->GetVisibility(), ESlateVisibility::Collapsed);
	TestTrue(TEXT("고정 완료 문구도 프로그램을 명시"),
		Panel->Text_ProgramCompletion->GetText().ToString().Contains(Panel->Catalog[YogaIndex].DisplayName.ToString()));
	UTextBlock* CompletionBadge = Cast<UTextBlock>(Panel->CardWidgets[YogaIndex]->GetWidgetFromName(TEXT("Text_CompletionBadge")));
	if (!TestNotNull(TEXT("프로그램별 완료 배지"), CompletionBadge) ||
		!TestNotNull(TEXT("완료 알림 닫기 버튼"), Panel->Button_DismissScheduleToast.Get())) return false;
	TestNotEqual(TEXT("알림과 별도로 완료 배지 표시"), CompletionBadge->GetVisibility(), ESlateVisibility::Collapsed);
	Panel->Button_DismissScheduleToast->OnClicked.Broadcast();
	TestEqual(TEXT("닫기 버튼으로 일시 알림 해제"), Panel->Border_ScheduleToast->GetVisibility(), ESlateVisibility::Collapsed);
	TestNotEqual(TEXT("일시 알림을 닫아도 배지가 유지됨"), CompletionBadge->GetVisibility(), ESlateVisibility::Collapsed);
	UButton* CurrentWorkshopButton = Cast<UButton>(Panel->CardWidgets[WorkshopIndex]->GetWidgetFromName(TEXT("Button_Root")));
	if (!TestNotNull(TEXT("갱신된 공방 카드 버튼"), CurrentWorkshopButton)) return false;
	TestTrue(TEXT("요가를 편성해도 다른 프로그램 카드는 활성"), CurrentWorkshopButton->GetIsEnabled());
	CurrentWorkshopButton->OnClicked.Broadcast();
	TestEqual(TEXT("완료 후 다른 프로그램 선택 가능"), Panel->SelectedCardIndex, WorkshopIndex);
	TestTrue(TEXT("다른 프로그램에 이전 완료 문구가 붙지 않음"),
		Panel->Text_ProgramCompletion->GetVisibility() == ESlateVisibility::Collapsed
		|| Panel->Text_ProgramCompletion->GetText().IsEmpty());
	// 같은 프로그램도 다른 시설에 추가 편성할 수 있다. 완료 표시가 전역 잠금으로 변하면 안 된다.
	Panel->HandleCardClicked(YogaIndex);
	const int32 FirstYogaIndex = Panel->FacilityOptions.IndexOfByPredicate([&InstalledYogaId](const FFF_PrepFacilityOption& Option)
	{
		return Option.FacilityInstanceId == InstalledYogaId;
	});
	if (!TestTrue(TEXT("미편성 시설을 다시 선택할 수 있음"), FirstYogaIndex != INDEX_NONE)) return false;
	Panel->HandleFacilityClicked(FirstYogaIndex);
	TestTrue(TEXT("다른 시설에는 같은 프로그램 추가 편성 가능"), Panel->Button_Schedule->GetIsEnabled());
	Panel->HandleScheduleClicked();
	TestEqual(TEXT("추가 시설 편성 성공"), Progression->GetScheduledPrograms().Num(), 2);
	TestTrue(TEXT("프로그램 카드에 완료한 시설 수 반영"), CompletionBadge->GetText().ToString().Contains(TEXT("2")));

	// 편성된 시설은 소유자 정책상 철거할 수 없다. 번호 검증 때문에 실제 편성 규칙을 우회하지 않는다.
	TMap<FGuid, int32> ScheduledFacilityNumbers;
	for (const FFF_PrepFacilityOption& Option : Panel->FacilityOptions)
	{
		ScheduledFacilityNumbers.Add(Option.FacilityInstanceId, Option.InstanceNumber);
	}
	FFFZSCommand RemoveSecond;
	RemoveSecond.CommandId = FGuid::NewGuid();
	RemoveSecond.TransactionId = FGuid::NewGuid();
	RemoveSecond.Type = EFFZSCommandType::Remove;
	RemoveSecond.ExpectedWorldRevision = Zones->GetWorldRevision();
	RemoveSecond.FacilityInstanceId = InstallSecondYoga.FacilityInstanceId;
	const FFFZSCommandResult RemoveResult = Zones->ExecuteCommand(RemoveSecond);
	if (!TestFalse(TEXT("편성된 시설의 철거는 기존 정책이 거부"), RemoveResult.bSucceeded)) return false;
	TestEqual(TEXT("철거 거부 시 편성 시설 두 곳 유지"), Panel->FacilityOptions.Num(), 2);

	// 미편성 세 번째 시설로 번호 수명주기를 확인한다. 철거 번호는 비워 두고 새 번호를 쓴다.
	FFFZSGridPlacement ThirdYogaPlacement;
	ThirdYogaPlacement.OriginCell = SecondYogaPlacement.OriginCell + FIntPoint(ReservedMaxX - ReservedMinX + 100, 0);
	const FFFZSCommand InstallThird = UFFZSGridLibrary::MakeInstallCommand(
		YogaFacility->Spec, FixtureZoneId, ThirdYogaPlacement, Zones->GetWorldRevision());
	const FFFZSCommandResult ThirdInstallResult = Zones->ExecuteCommand(InstallThird);
	if (!TestTrue(*FString::Printf(TEXT("미편성 세 번째 시설 설치: %s"), *ThirdInstallResult.Message.ToString()), ThirdInstallResult.bSucceeded)) return false;
	if (!TestEqual(TEXT("미편성 시설까지 목록에 표시"), Panel->FacilityOptions.Num(), 3)) return false;
	const FFF_PrepFacilityOption* ThirdOption = Panel->FacilityOptions.FindByPredicate([&InstallThird](const FFF_PrepFacilityOption& Option)
	{
		return Option.FacilityInstanceId == InstallThird.FacilityInstanceId;
	});
	if (!TestNotNull(TEXT("세 번째 시설 표시 번호 읽기"), ThirdOption)) return false;
	const int32 RemovedInstanceNumber = ThirdOption->InstanceNumber;
	FFFZSCommand RemoveThird;
	RemoveThird.CommandId = FGuid::NewGuid();
	RemoveThird.TransactionId = FGuid::NewGuid();
	RemoveThird.Type = EFFZSCommandType::Remove;
	RemoveThird.ExpectedWorldRevision = Zones->GetWorldRevision();
	RemoveThird.FacilityInstanceId = InstallThird.FacilityInstanceId;
	const FFFZSCommandResult RemoveThirdResult = Zones->ExecuteCommand(RemoveThird);
	if (!TestTrue(*FString::Printf(TEXT("미편성 시설 철거: %s"), *RemoveThirdResult.Message.ToString()), RemoveThirdResult.bSucceeded)) return false;
	if (!TestEqual(TEXT("철거 즉시 열린 시설 목록 갱신"), Panel->FacilityOptions.Num(), 2)) return false;
	for (const FFF_PrepFacilityOption& Option : Panel->FacilityOptions)
	{
		TestEqual(TEXT("남아 있는 편성 시설의 번호 유지"), Option.InstanceNumber, ScheduledFacilityNumbers.FindRef(Option.FacilityInstanceId));
	}
	TestFalse(TEXT("철거 시설 위치보기는 실패"), ViewModel->RequestLocateProgramFacility(InstallThird.FacilityInstanceId));
	const FFFZSCommand InstallReplacement = UFFZSGridLibrary::MakeInstallCommand(
		YogaFacility->Spec, FixtureZoneId, ThirdYogaPlacement, Zones->GetWorldRevision());
	const FFFZSCommandResult ReplacementResult = Zones->ExecuteCommand(InstallReplacement);
	if (!TestTrue(*FString::Printf(TEXT("새 시설 설치: %s"), *ReplacementResult.Message.ToString()), ReplacementResult.bSucceeded)) return false;
	const FFF_PrepFacilityOption* Replacement = Panel->FacilityOptions.FindByPredicate([&InstallReplacement](const FFF_PrepFacilityOption& Option)
	{
		return Option.FacilityInstanceId == InstallReplacement.FacilityInstanceId;
	});
	if (!TestNotNull(TEXT("새 시설이 열린 목록에 반영됨"), Replacement)) return false;
	TestTrue(TEXT("철거 번호를 새 시설에 재사용하지 않음"), Replacement->InstanceNumber > RemovedInstanceNumber);

	// 날짜/빈 목록은 더미 일차나 0 Gold로 메우지 않는다. 격리된 데이터에서 UI 갱신만 확인한다.
	UFF_FestivalStageSubsystem* Stage = Game->GetSubsystem<UFF_FestivalStageSubsystem>();
	if (!TestNotNull(TEXT("Fixture stage provides the current day"), Stage)) return false;
	Stage->CurrentDay = 2;
	Panel->HandleStageViewChanged(Stage->CurrentStage, Stage->CurrentDay);
	TestEqual(TEXT("Day label reads the current stage day"), DayText->GetText().ToString(),
		FText::Format(Panel->DayTextFormat, FText::AsNumber(2)).ToString());
	IsolatedFestival->ProgramCatalog.Reset();
	Panel->HandleFestivalDefinitionChanged();
	TestEqual(TEXT("Empty catalog shows an explicit explanation"), EmptyText->GetText().ToString(), Panel->EmptyCatalogText.ToString());
	TestFalse(TEXT("Empty catalog cannot schedule"), Panel->Button_Schedule->GetIsEnabled());
	TestEqual(TEXT("Empty catalog clears cost instead of displaying free"), Panel->GetQuote().CostGold, INDEX_NONE);
	const int32 QuoteBeforeClose = Panel->GetQuote().MaxRevenue;
	Panel->HandlePanelClosed();
	RevenueRow.ProgramFeesPerPerson.Add(TEXT("Program_SunsetYoga"), 100);
	RevenueTable->AddRow(YogaFacility->Spec.FacilityDefinitionId, RevenueRow);
	TestEqual(TEXT("Closing the panel detaches the table-change listener"), Panel->GetQuote().MaxRevenue, QuoteBeforeClose);
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
