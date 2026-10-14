// 역할: 실제 도감 진입 버튼의 첫 사용 안내와 미수령 보상 표시를 각각 검증한다.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Building/Codex/FF_FacilityCodexProgressSubsystem.h"
#include "Building/Codex/FF_FacilityCodexSettings.h"
#include "Building/Codex/FF_FacilityCodexSubsystem.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Building/Synergy/FF_FacilitySynergySettings.h"
#include "Building/Synergy/FF_FacilitySynergyTypes.h"
#include "Camera/FF_FestivalPlayerController.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Slate/SceneViewport.h"
#include "Tests/AutomationCommon.h"
#include "TimerManager.h"
#include "UI/FacilityCodex/FF_FacilityCodexLauncher.h"
#include "UI/FacilityCodex/FF_FacilityCodexPresenterSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SViewport.h"

namespace
{
	/** Exercise real persistence without changing the player's profile. */
	class FRewardNotificationProfile
	{
	public:
		FRewardNotificationProfile()
			: Path(FPaths::ConvertRelativePathToFull(FPaths::CreateTempFilename(
				*FPaths::ProjectSavedDir(), TEXT("CodexNotificationTest"), TEXT(".ini"))))
			, PreviousPath(GGameUserSettingsIni)
		{
			FPaths::NormalizeFilename(Path);
			if (GConfig) GConfig->Add(Path, FConfigFile());
			GGameUserSettingsIni = Path;
		}
		~FRewardNotificationProfile()
		{
			if (GConfig) GConfig->UnloadFile(Path);
			GGameUserSettingsIni = PreviousPath;
			IFileManager::Get().Delete(*Path);
		}
	private:
		FString Path;
		FString PreviousPath;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexRewardNotificationTest,
	"FestaFiesta.FacilityCodex.UI.RewardNotificationPersistsUntilAllClaims",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityCodexRewardNotificationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FRewardNotificationProfile Profile;
	const FName FirstRule(TEXT("NotificationFirst"));
	const FName SecondRule(TEXT("NotificationSecond"));
	const FName LaterRule(TEXT("NotificationLater"));
	const FName LockedRule(TEXT("NotificationLocked"));
	const FName ZeroRule(TEXT("NotificationZero"));
	TStrongObjectPtr<UDataTable> Rules(NewObject<UDataTable>());
	Rules->RowStruct = FFF_FacilitySynergyRow::StaticStruct();
	FFF_FacilitySynergyRow Row;
	Row.TargetFacilityId = TEXT("Booth_FoodStall_NotificationFixture");
	Row.PartnerFacilityId = Row.TargetFacilityId;
	Row.DiscoveryRewardGold = 75;
	for (const FName RuleId : {FirstRule, SecondRule, LaterRule, LockedRule})
	{
		Row.DisplayName = FText::FromName(RuleId);
		Rules->AddRow(RuleId, Row);
	}
	Row.DiscoveryRewardGold = 0;
	Rules->AddRow(ZeroRule, Row);
	TGuardValue<TSoftObjectPtr<UDataTable>> TableSetting(
		GetMutableDefault<UFF_FacilitySynergySettings>()->SynergyTable, TSoftObjectPtr<UDataTable>(Rules.Get()));
	FFF_FacilityCodexCollectionReward ReachedReward;
	ReachedReward.MilestoneId = TEXT("NotificationCollectionReached");
	ReachedReward.RequiredDiscoveries = 3;
	ReachedReward.RewardGold = 200;
	FFF_FacilityCodexCollectionReward LockedReward;
	LockedReward.MilestoneId = TEXT("NotificationCollectionLocked");
	LockedReward.RequiredDiscoveries = 5;
	LockedReward.RewardGold = 400;
	TGuardValue<TArray<FFF_FacilityCodexCollectionReward>> CollectionSetting(
		GetMutableDefault<UFF_FacilityCodexSettings>()->CollectionRewards,
		TArray<FFF_FacilityCodexCollectionReward>{ReachedReward, LockedReward});
	FTestWorldWrapper Fixture;
	const uint64 OriginalFrameCounter = GFrameCounter;
	ON_SCOPE_EXIT { GFrameCounter = OriginalFrameCounter; };
	if (!Fixture.CreateTestWorld(EWorldType::Game))
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Fixture.GetTestWorld();
	UGameInstance* Game = World->GetGameInstance();
	auto* Codex = World->GetSubsystem<UFF_FacilityCodexSubsystem>();
	auto* Presenter = World->GetSubsystem<UFF_FacilityCodexPresenterSubsystem>();
	auto* Progress = Game ? Game->GetSubsystem<UFF_FacilityCodexProgressSubsystem>() : nullptr;
	auto* Economy = Game ? Game->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
	FWorldContext* Context = GEngine->GetWorldContextFromWorld(World);
	if (!TestNotNull(TEXT("Notification codex"), Codex) || !TestNotNull(TEXT("Notification presenter"), Presenter) ||
		!TestNotNull(TEXT("Isolated notification profile"), Progress) || !TestNotNull(TEXT("Notification wallet"), Economy) ||
		!TestNotNull(TEXT("Notification viewport context"), Context)) return false;
	// This fixture assigns its GameInstance after world subsystem initialization.
	// Bind the same profile boundary normally refreshed by OnWorldBeginPlay, while
	// leaving startup claims disabled for the rejected-claim check below.
	Codex->Refresh();
	if (!TestEqual(TEXT("The notification fixture projects all five authored recipes"),
		Codex->GetEntriesView().Num(), 5)) return false;

	UGameViewportClient* Viewport = NewObject<UGameViewportClient>(GEngine);
	Context->GameViewport = Viewport;
	Viewport->Init(*Context, Game, false);
	const TSharedRef<SOverlay> Overlay = SNew(SOverlay);
	const TSharedRef<SViewport> ViewportWidget = SNew(SViewport);
	Viewport->SetViewportOverlayWidget(nullptr, Overlay);
	TUniquePtr<FSceneViewport> SceneViewport = MakeUnique<FSceneViewport>(Viewport, ViewportWidget);
	Viewport->Viewport = SceneViewport.Get();
	ON_SCOPE_EXIT { Viewport->Viewport = nullptr; };
	FString PlayerError;
	ULocalPlayer* Player = Game->CreateLocalPlayer(0, PlayerError, false);
	auto* Controller = World->SpawnActor<AFF_FestivalPlayerController>();
	if (!TestNotNull(*FString::Printf(TEXT("Notification local player: %s"), *PlayerError), Player) ||
		!TestNotNull(TEXT("Notification controller"), Controller)) return false;
	Controller->SetPlayer(Player);
	Controller->PlayerInput = NewObject<UPlayerInput>(Controller);
	World->AddController(Controller);
	const TSharedPtr<SWidget> PreviousFocus = FSlateApplication::Get().GetKeyboardFocusedWidget();
	ON_SCOPE_EXIT
	{
		Presenter->CloseCodex();
		if (PreviousFocus) FSlateApplication::Get().SetKeyboardFocus(PreviousFocus);
		else FSlateApplication::Get().ClearKeyboardFocus();
	};
	const FString InspectorPath = FString(FFBuildUIAssetPaths::FacilityCodexLauncherPackage) +
		TEXT(".WBP_FF_FacilityCodexLauncher_C");
	UClass* LauncherClasses[] = {
		LoadClass<UFF_FacilityCodexLauncher>(nullptr, *InspectorPath),
		LoadClass<UFF_FacilityCodexLauncher>(nullptr, FFBuildUIAssetPaths::FacilityCodexCatalogLauncherClass)
	};
	if (!TestNotNull(TEXT("Authored inspector launcher"), LauncherClasses[0]) ||
		!TestNotNull(TEXT("Authored catalog launcher"), LauncherClasses[1])) return false;
	TStrongObjectPtr<UFF_FacilityCodexLauncher> InspectorLauncher(
		CreateWidget<UFF_FacilityCodexLauncher>(Controller, LauncherClasses[0]));
	TStrongObjectPtr<UFF_FacilityCodexLauncher> CatalogLauncher(
		CreateWidget<UFF_FacilityCodexLauncher>(Controller, LauncherClasses[1]));
	UFF_FacilityCodexLauncher* Launchers[] = {InspectorLauncher.Get(), CatalogLauncher.Get()};
	UBorder* Hints[2] = {};
	UBorder* IntroductionHints[2] = {};
	UButton* Buttons[2] = {};
	ON_SCOPE_EXIT { for (auto* Launcher : Launchers) if (Launcher) Launcher->RemoveFromParent(); };
	for (int32 Index = 0; Index < 2; ++Index)
	{
		if (!TestNotNull(TEXT("Production notification launcher"), Launchers[Index])) return false;
		Launchers[Index]->AddToViewport();
		IntroductionHints[Index] = Cast<UBorder>(Launchers[Index]->GetWidgetFromName(TEXT("Border_IntroductionHint")));
		Hints[Index] = Cast<UBorder>(Launchers[Index]->GetWidgetFromName(TEXT("Border_RewardHint")));
		Buttons[Index] = Cast<UButton>(Launchers[Index]->GetWidgetFromName(TEXT("Button_OpenCodex")));
		if (!TestNotNull(TEXT("Authored first-use notification icon"), IntroductionHints[Index]) ||
			!TestNotNull(TEXT("Authored unclaimed reward icon"), Hints[Index]) ||
			!TestNotNull(TEXT("Actual notification entry button"), Buttons[Index])) return false;
		const FLinearColor Color = IntroductionHints[Index]->Background.TintColor.GetSpecifiedColor() * IntroductionHints[Index]->GetBrushColor();
		TestTrue(TEXT("Each first-use invitation remains red"), Color.R > Color.G && Color.R > Color.B);
		const FLinearColor RewardColor = Hints[Index]->Background.TintColor.GetSpecifiedColor() * Hints[Index]->GetBrushColor();
		TestTrue(TEXT("Earned rewards use a separate gold indicator"), RewardColor.R > RewardColor.B && RewardColor.G > RewardColor.B);
	}
	const auto TestHints = [this, &Hints, &IntroductionHints](const TCHAR* Message, const bool bVisible, const bool bIntroduction = false)
	{
		for (int32 Index = 0; Index < 2; ++Index)
		{
			TestEqual(*FString::Printf(TEXT("%s (%s)"), Message, Index == 0 ? TEXT("inspector") : TEXT("catalog")),
				Hints[Index]->GetVisibility(), bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			TestEqual(*FString::Printf(TEXT("%s keeps first-use state independent (%s)"), Message, Index == 0 ? TEXT("inspector") : TEXT("catalog")),
				IntroductionHints[Index]->GetVisibility(), bIntroduction ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	};
	TestHints(TEXT("Fresh profiles preserve the first-use invitation without an earned reward"), false, true);
	TestFalse(TEXT("Undiscovered rewards are not pending"), Codex->HasUnclaimedRewards());
	Progress->MarkIntroductionSeen();
	TestHints(TEXT("Seen introduction with no earned rewards hides both icons"), false);
	Progress->RecordDiscoveries({ZeroRule});
	TestFalse(TEXT("Zero-gold discovery and unreached milestones do not notify"), Codex->HasUnclaimedRewards());
	TestHints(TEXT("Zero-gold discoveries leave both icons hidden"), false);
	Progress->RecordDiscoveries({FirstRule, SecondRule});
	TestTrue(TEXT("New positive rewards become pending immediately"), Codex->HasUnclaimedRewards());
	TestHints(TEXT("New discoveries notify both existing launchers"), true);
	Buttons[0]->OnClicked.Broadcast();
	if (!TestTrue(TEXT("The actual launcher opens the codex"), Presenter->IsCodexOpen())) return false;
	TestHints(TEXT("Opening the collection does not consume earned rewards"), true);
	TestFalse(TEXT("A pre-startup claim is rejected"), Codex->ClaimDiscoveryReward(FirstRule));
	TestHints(TEXT("A failed claim preserves both icons"), true);
	Presenter->CloseCodex();

	TStrongObjectPtr<UFF_FestivalDefinition> Definition(NewObject<UFF_FestivalDefinition>(Game));
	UDataTable* Costs = NewObject<UDataTable>(Definition.Get());
	Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
	Costs->AddRow(TEXT("Cost.Booth.FoodStall"), FFF_FacilityCostRow());
	Definition->FacilityCostTable = Costs;
	Definition->FestivalId = TEXT("CodexNotificationAutomation");
	Definition->InitialGold = 1000;
	Economy->StartFestival(Definition.Get());
	TestFalse(TEXT("New festival clears the previous reward notification"), Codex->HasUnclaimedRewards());
	TestHints(TEXT("New festival removes both old reward icons"), false);
	Progress->RecordDiscoveries({ZeroRule, FirstRule, SecondRule});
	Codex->OnWorldBeginPlay(*World);
	for (int32 Frame = 0; Frame < 2; ++Frame)
	{
		++GFrameCounter;
		World->GetTimerManager().Tick(.016f);
	}
	TestTrue(TEXT("The first discovery reward can be claimed"), Codex->ClaimDiscoveryReward(FirstRule));
	TestHints(TEXT("A partial claim leaves the other rewards highlighted"), true);
	TestTrue(TEXT("The second discovery reward can be claimed"), Codex->ClaimDiscoveryReward(SecondRule));
	TestTrue(TEXT("An earned collection milestone still counts as pending"), Codex->HasUnclaimedRewards());
	TestHints(TEXT("A remaining collection reward keeps both icons visible"), true);
	TestTrue(TEXT("The reached collection reward can be claimed"), Codex->ClaimCollectionReward(ReachedReward.MilestoneId));
	TestFalse(TEXT("All earned rewards are now claimed despite locked and zero-gold recipes"), Codex->HasUnclaimedRewards());
	TestHints(TEXT("The final reward claim immediately removes both icons"), false);
	InspectorLauncher->RemoveFromParent();
	InspectorLauncher->AddToViewport();
	TestHints(TEXT("Reconstructing a settled launcher does not restore its icon"), false);
	Progress->RecordDiscoveries({LaterRule});
	TestHints(TEXT("A later discovery restores both reward icons"), true);
	InspectorLauncher->RemoveFromParent();
	InspectorLauncher->AddToViewport();
	TestHints(TEXT("Reconstructing a pending launcher retains its icon"), true);
	TestTrue(TEXT("The later reward can be claimed"), Codex->ClaimDiscoveryReward(LaterRule));
	TestHints(TEXT("Receiving the last new reward removes both icons again"), false);
	TestEqual(TEXT("Only the three discovery rewards and one reached milestone are paid"), Economy->GetGold(), 1425);
	return true;
}

#endif
