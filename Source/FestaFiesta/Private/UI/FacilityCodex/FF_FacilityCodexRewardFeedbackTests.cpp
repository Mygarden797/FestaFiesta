#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Building/Codex/FF_FacilityCodexProgressSubsystem.h"
#include "Building/Codex/FF_FacilityCodexSubsystem.h"
#include "Building/Synergy/FF_FacilitySynergySettings.h"
#include "Building/Synergy/FF_FacilitySynergyTypes.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "Tests/AutomationCommon.h"
#include "TextureResource.h"
#include "TimerManager.h"
#include "UI/FacilityCodex/FF_FacilityCodexCard.h"
#include "UI/FacilityCodex/FF_FacilityCodexPresenterSubsystem.h"
#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Layout/SDPIScaler.h"

namespace
{
	/** Real claim persistence, confined to a disposable profile owned by this test. */
	class FRewardFeedbackProfile
	{
	public:
		FRewardFeedbackProfile()
			: Path(FPaths::ConvertRelativePathToFull(FPaths::CreateTempFilename(
				*FPaths::ProjectSavedDir(), TEXT("CodexFeedbackTest"), TEXT(".ini"))))
			, PreviousPath(GGameUserSettingsIni)
		{
			FPaths::NormalizeFilename(Path);
			if (GConfig) GConfig->Add(Path, FConfigFile());
			GGameUserSettingsIni = Path;
		}
		~FRewardFeedbackProfile()
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexRewardFeedbackTest,
	"FestaFiesta.FacilityCodex.UI.RewardFeedbackMatchesWallet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityCodexRewardFeedbackTest::RunTest(const FString& Parameters)
{
	FRewardFeedbackProfile Profile;
	TStrongObjectPtr<UDataTable> Rules(NewObject<UDataTable>());
	Rules->RowStruct = FFF_FacilitySynergyRow::StaticStruct();
	const FName FirstRule(TEXT("RewardFeedbackFirst"));
	const FName SecondRule(TEXT("RewardFeedbackSecond"));
	FFF_FacilitySynergyRow Row;
	Row.TargetFacilityId = TEXT("Booth_FoodStall_RewardFeedbackFixture");
	Row.PartnerFacilityId = Row.TargetFacilityId;
	Row.DiscoveryRewardGold = 333;
	Row.DisplayName = FText::FromString(TEXT("첫 발견 보상"));
	Rules->AddRow(FirstRule, Row);
	Row.DisplayName = FText::FromString(TEXT("두 번째 발견 보상"));
	Rules->AddRow(SecondRule, Row);
	TGuardValue<TSoftObjectPtr<UDataTable>> TableSetting(
		GetMutableDefault<UFF_FacilitySynergySettings>()->SynergyTable, TSoftObjectPtr<UDataTable>(Rules.Get()));
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game))
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Fixture.GetTestWorld();
	UGameInstance* Game = World->GetGameInstance();
	UFF_FacilityCodexSubsystem* Codex = World->GetSubsystem<UFF_FacilityCodexSubsystem>();
	UFF_FacilityCodexProgressSubsystem* Progress = Game->GetSubsystem<UFF_FacilityCodexProgressSubsystem>();
	UFF_FestivalProgressionSubsystem* Economy = Game->GetSubsystem<UFF_FestivalProgressionSubsystem>();
	if (!TestNotNull(TEXT("Real codex projection"), Codex) || !TestNotNull(TEXT("Isolated discovery profile"), Progress) ||
		!TestNotNull(TEXT("Real game wallet"), Economy)) return false;
	// Discovery/installation is covered by the backend suite. Seed completed history
	// through its real persistence API, then exercise the authored UI's claim boundary.
	Progress->RecordDiscoveries(TSet<FName>{FirstRule, SecondRule});
	Codex->Refresh();
	FFF_FacilityCodexEntry UnclaimedEntry;
	if (!TestTrue(TEXT("The authoritative recipe is pending"), Codex->FindEntry(FirstRule, UnclaimedEntry) &&
		UnclaimedEntry.bDiscovered && !UnclaimedEntry.bDiscoveryRewardClaimed)) return false;
	UClass* ScreenClass = UFF_FacilityCodexPresenterSubsystem::GetDefaultScreenClassPath().TryLoadClass<UFF_FacilityCodexScreen>();
	if (!TestNotNull(TEXT("Authored codex screen"), ScreenClass)) return false;
	auto* BlueprintClass = Cast<UWidgetBlueprintGeneratedClass>(ScreenClass);
	if (!TestNotNull(TEXT("Authored screen generated class"), BlueprintClass)) return false;
	// Match the other playerless UMG fixtures: run the real NativeOnInitialized
	// so its authored button delegates are bound, then restore the class setting.
	const bool bPreviousPlayerlessInitialization = BlueprintClass->bCanCallInitializedWithoutPlayerContext;
	BlueprintClass->bCanCallInitializedWithoutPlayerContext = true;
	ON_SCOPE_EXIT { BlueprintClass->bCanCallInitializedWithoutPlayerContext = bPreviousPlayerlessInitialization; };
	// FTestWorldWrapper does not initialize GameInstance::WorldContext. Use the
	// fixture world as outer so UMG resolves this wallet, not the editor's world.
	// Initialize is the same authored/native initialization used by CreateWidget.
	TStrongObjectPtr<UFF_FacilityCodexScreen> Screen(NewObject<UFF_FacilityCodexScreen>(World, ScreenClass));
	if (!TestNotNull(TEXT("Real codex screen instance"), Screen.Get())) return false;
	if (!TestTrue(TEXT("Authored screen initializes normally"), Screen->Initialize()) ||
		!TestTrue(TEXT("Screen resolves the isolated gameplay world"), Screen->GetWorld() == World) ||
		!TestTrue(TEXT("Screen resolves the isolated wallet owner"), Screen->GetGameInstance() == Game)) return false;
	TSharedPtr<SWidget> Slate = Screen->TakeWidget();
	ON_SCOPE_EXIT { Slate.Reset(); Screen->ReleaseSlateResources(true); };
	auto* Tile = Cast<UFF_FacilityCodexTileView>(Screen->GetWidgetFromName(TEXT("TileView_Entries")));
	auto* ClaimButton = Cast<UButton>(Screen->GetWidgetFromName(TEXT("Button_DetailClaimReward")));
	auto* Feedback = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_RewardFeedback")));
	auto* FeedbackPanel = Cast<UBorder>(Screen->GetWidgetFromName(TEXT("Border_RewardFeedback")));
	auto* RewardLabel = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_DetailRewardLabel")));
	auto* RewardHint = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_DetailRewardHint")));
	auto* ReadyLabel = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Button_RewardReady_Label")));
	if (!TestNotNull(TEXT("Collection list"), Tile) || !TestNotNull(TEXT("Actual claim button"), ClaimButton) ||
		!TestNotNull(TEXT("Authored success feedback"), Feedback) || !TestNotNull(TEXT("Feedback surface"), FeedbackPanel) ||
		!TestNotNull(TEXT("Authored reward label"), RewardLabel) ||
		!TestNotNull(TEXT("Authored reward hint"), RewardHint) || !TestNotNull(TEXT("Authored ready count"), ReadyLabel)) return false;
	if (!TestTrue(TEXT("The real claim button has its native click handler"), ClaimButton->OnClicked.IsBound())) return false;
	UFF_FacilityCodexItem* FirstItem = nullptr;
	for (UObject* Object : Tile->GetListItems())
	{
		auto* Item = Cast<UFF_FacilityCodexItem>(Object);
		if (Item && Item->Entry.RuleId == FirstRule) FirstItem = Item;
	}
	if (!TestNotNull(TEXT("Selectable authoritative reward"), FirstItem)) return false;
	Tile->SetSelectedItem(FirstItem);
	TestEqual(TEXT("The actual detail button targets the chosen recipe"), Screen->GetSelectedRuleId(), FirstRule);
	TestTrue(TEXT("Pending reward clearly enables its claim action"), ClaimButton->GetIsEnabled() && ClaimButton->IsVisible());
	TestFalse(TEXT("Pending reward has an explanatory hint"), RewardHint->GetText().IsEmpty());
	TestFalse(TEXT("There is no success announcement before payment"), Feedback->IsVisible());
	TestTrue(TEXT("The filter advertises both pending rewards"), ReadyLabel->GetText().ToString().Contains(TEXT("2")));
	// The game instance auto-starts its configured festival, but this isolated
	// world has not received the codex startup callback. Claims must still fail.
	const int32 BeforeStartupGold = Economy->GetGold();
	ClaimButton->OnClicked.Broadcast();
	TestEqual(TEXT("A pre-startup button claim leaves the wallet unchanged"), Economy->GetGold(), BeforeStartupGold);
	TestFalse(TEXT("A rejected claim does not consume the reward"), Progress->IsDiscoveryRewardClaimed(FirstRule));
	TestFalse(TEXT("A rejected claim never shows success feedback"), Feedback->IsVisible());

	TStrongObjectPtr<UFF_FestivalDefinition> Definition(NewObject<UFF_FestivalDefinition>(Game));
	UDataTable* Costs = NewObject<UDataTable>(Definition.Get());
	Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
	Costs->AddRow(TEXT("Cost.Booth.FoodStall"), FFF_FacilityCostRow());
	Definition->FacilityCostTable = Costs;
	Definition->FestivalId = TEXT("CodexFeedbackAutomation");
	Definition->InitialGold = 1000;
	Economy->StartFestival(Definition.Get());
	Codex->OnWorldBeginPlay(*World);
	bool bStartupQueueAdvanced = false;
	FTimerHandle StartupProbe = World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([&bStartupQueueAdvanced]()
	{
		bStartupQueueAdvanced = true;
	}));
	{
		// UE can queue next-tick timers as pending until the end of the first tick.
		// Distinct frame numbers exercise both pending promotion and execution.
		TGuardValue<uint64> Frame(GFrameCounter, GFrameCounter);
		for (int32 Index = 0; Index < 2; ++Index)
		{
			++GFrameCounter;
			World->GetTimerManager().Tick(.016f);
		}
	}
	World->GetTimerManager().ClearTimer(StartupProbe);
	if (!TestTrue(TEXT("The real codex startup timer queue completed"), bStartupQueueAdvanced)) return false;
	TestEqual(TEXT("Startup completion does not automatically pay discovery rewards"), Economy->GetGold(), 1000);
	const FText ReadyHint = RewardHint->GetText();
	ClaimButton->OnClicked.Broadcast();
	TestEqual(TEXT("The actual button pays exactly the authored reward once"), Economy->GetGold(), 1333);
	TestTrue(TEXT("Successful payment persists the claim"), Progress->IsDiscoveryRewardClaimed(FirstRule));
	TestTrue(TEXT("Successful payment displays its credited amount"), FeedbackPanel->IsVisible() && Feedback->IsVisible() &&
		Feedback->GetText().ToString().Contains(FText::AsNumber(333).ToString()));
	TestFalse(TEXT("Claimed reward can no longer be clicked"), ClaimButton->GetIsEnabled());
	TestTrue(TEXT("The detail hint changes to the claimed explanation"), !RewardHint->GetText().IsEmpty() && !RewardHint->GetText().EqualTo(ReadyHint));
	TestTrue(TEXT("The ready count drops immediately after payment"), ReadyLabel->GetText().ToString().Contains(TEXT("1")));
	if (!FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
	{
		constexpr int32 Width = 1920, Height = 1080;
		const FVector2D Size(Width, Height);
		const float Dpi = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(Width, Height));
		FWidgetRenderer Renderer(false, true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, true));
		if (!TestNotNull(TEXT("Actual reward-feedback capture target"), Target.Get())) return false;
		const TSharedRef<SWidget> CaptureWidget = SNew(SDPIScaler).DPIScale(Dpi)[Slate.ToSharedRef()];
		for (int32 Frame = 0; Frame < 8; ++Frame)
		{
			Screen->ForceLayoutPrepass();
			Renderer.DrawWidget(Target.Get(), CaptureWidget, Size, .016f, false);
		}
		FlushRenderingCommands();
		TestTrue(TEXT("The actual 333-gold success announcement remains visible in the capture"),
			FeedbackPanel->IsVisible() && Feedback->IsVisible() && Feedback->GetText().ToString().Contains(TEXT("333")));
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags ReadFlags;
		ReadFlags.SetLinearToGamma(false);
		if (!TestTrue(TEXT("Actual reward-feedback pixels are readable"),
			Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, ReadFlags))) return false;
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Width, Height, Pixels, Png);
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/FacilityCodex");
		IFileManager::Get().MakeDirectory(*Directory, true);
		TestTrue(TEXT("Actual reward-feedback screenshot saved"), FFileHelper::SaveArrayToFile(
			Png, *(Directory / TEXT("FacilityCodexRewardFeedback1080.png"))));
	}
	const FText FirstFeedback = Feedback->GetText();
	const FGeometry TickGeometry = FGeometry::MakeRoot(FVector2D(1280, 720), FSlateLayoutTransform());
	Slate->Tick(TickGeometry, 0.0, 2.f);
	ClaimButton->OnClicked.Broadcast();
	FirstItem->OnClaimRewardRequested.ExecuteIfBound(TEXT("Missing.RewardFeedbackRule"));
	TestEqual(TEXT("Duplicate and stale requests cannot pay again"), Economy->GetGold(), 1333);
	TestTrue(TEXT("Duplicate and stale requests do not replace the successful feedback"), Feedback->GetText().EqualTo(FirstFeedback));
	Slate->Tick(TickGeometry, 2.0, 1.1f);
	TestFalse(TEXT("Duplicate and stale requests cannot restart the success-feedback timer"), Feedback->IsVisible() || FeedbackPanel->IsVisible());

	Screen->SetDiscoveryFilter(EFF_FacilityCodexFilterState::RewardReady);
	TestEqual(TEXT("The ready filter contains only the unpaid recipe"), Screen->GetVisibleEntryCount(), 1);
	TestEqual(TEXT("The remaining reward is selected"), Screen->GetSelectedRuleId(), SecondRule);
	Economy->GrantDiscoveryGold(MAX_int32 - Economy->GetGold() - 7);
	ClaimButton->OnClicked.Broadcast();
	TestEqual(TEXT("The wallet safely reaches its cap"), Economy->GetGold(), MAX_int32);
	TestTrue(TEXT("Feedback reports only the seven gold actually credited at the cap"), Feedback->IsVisible() &&
		Feedback->GetText().ToString().Contains(FText::AsNumber(7).ToString()) &&
		!Feedback->GetText().ToString().Contains(FText::AsNumber(333).ToString()));
	TestEqual(TEXT("The claimed recipe leaves the ready filter immediately"), Screen->GetVisibleEntryCount(), 0);
	TestTrue(TEXT("The ready filter visibly reports zero pending rewards"), ReadyLabel->GetText().ToString().Contains(TEXT("0")));
	TestTrue(TEXT("Success remains visible even after the ready filter removes its selected row"), Feedback->IsVisible());
	Screen->SetPreviewEntries({UnclaimedEntry});
	TestFalse(TEXT("Entering preview clears real payment feedback"), Feedback->IsVisible());
	TestTrue(TEXT("Preview still demonstrates a ready reward button"), ClaimButton->GetIsEnabled());
	ClaimButton->OnClicked.Broadcast();
	TestEqual(TEXT("Preview button never changes the live wallet"), Economy->GetGold(), MAX_int32);
	TestFalse(TEXT("Preview button cannot fabricate payment feedback"), Feedback->IsVisible());
	return true;
}

#endif
