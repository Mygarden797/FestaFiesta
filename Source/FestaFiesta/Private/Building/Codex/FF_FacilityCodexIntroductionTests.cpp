// 역할: 첫 도감 안내 기록의 사용자별 저장과 재실행·옛 보상 기록 이전을 검증한다.
#include "Building/Codex/FF_FacilityCodexProgressSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Building/Codex/FF_FacilityCodexTestObserver.h"
#include "Building/Presentation/FF_BuildingFeatureDiscoverySubsystem.h"
#include "Building/Presentation/FF_BuildFeatureGuide.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Subsystems/SubsystemCollection.h"
#include "UObject/StrongObjectPtr.h"

namespace FFF_CodexIntroductionTests
{
	/** All profile writes stay in a temporary ini; the player's actual history is untouched. */
	class FScopedProfile
	{
	public:
		FScopedProfile()
			: Path(FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("CodexIntroductionTest"), TEXT(".ini")))
			, PreviousPath(GGameUserSettingsIni)
		{
			Path = FPaths::ConvertRelativePathToFull(Path);
			FPaths::NormalizeFilename(Path);
			GConfig->Add(Path, FConfigFile());
			GGameUserSettingsIni = Path;
		}
		~FScopedProfile()
		{
			GConfig->UnloadFile(Path);
			GGameUserSettingsIni = PreviousPath;
			IFileManager::Get().Delete(*Path);
		}
		void ReloadFromDisk() const
		{
			GConfig->UnloadFile(Path);
			// A lazy Find reload marks standalone ini files NoSave in UE 5.7.
			// Explicit loading reproduces the writable GameUserSettings branch on restart.
			GConfig->LoadFile(Path);
		}
		FString Path;
		FString PreviousPath;
	};

	struct FSession
	{
		FSession()
			: Game(NewObject<UGameInstance>())
			, Progress(NewObject<UFF_FacilityCodexProgressSubsystem>(Game.Get()))
		{
			Progress->Initialize(Collection);
		}
		~FSession() { Progress->Deinitialize(); }
		TStrongObjectPtr<UGameInstance> Game;
		TStrongObjectPtr<UFF_FacilityCodexProgressSubsystem> Progress;
		FSubsystemCollection<UGameInstanceSubsystem> Collection;
	};

	struct FFeatureSession
	{
		FFeatureSession()
			: Game(NewObject<UGameInstance>())
			, Discovery(NewObject<UFF_BuildingFeatureDiscoverySubsystem>(Game.Get()))
		{
			Discovery->Initialize(Collection);
		}
		~FFeatureSession() { Discovery->Deinitialize(); }
		TStrongObjectPtr<UGameInstance> Game;
		TStrongObjectPtr<UFF_BuildingFeatureDiscoverySubsystem> Discovery;
		FSubsystemCollection<UGameInstanceSubsystem> Collection;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexIntroductionPersistenceTest,
	"FestaFiesta.Building.Codex.Introduction.OneTimeProfileHistorySurvivesRestart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_FacilityCodexIntroductionPersistenceTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("Profile persistence is available"), GConfig)) { return false; }
	FFF_CodexIntroductionTests::FScopedProfile Profile;
	{
		FFF_CodexIntroductionTests::FSession Session;
		TestFalse(TEXT("A fresh profile has not opened the introduction"), Session.Progress->HasSeenIntroduction());
		Session.Progress->RecordDiscoveries({TEXT("DiscoveredBeforeOpening")});
		TestFalse(TEXT("Discovery does not silently consume first-open guidance"), Session.Progress->HasSeenIntroduction());
		TStrongObjectPtr<UFF_FacilityCodexTestObserver> Observer(NewObject<UFF_FacilityCodexTestObserver>());
		Session.Progress->OnProgressChanged.AddDynamic(Observer.Get(), &UFF_FacilityCodexTestObserver::HandleCodexChanged);
		TestTrue(TEXT("The first successful introduction records a change"), Session.Progress->MarkIntroductionSeen());
		TestTrue(TEXT("Every launcher can immediately read the seen state"), Session.Progress->HasSeenIntroduction());
		TestEqual(TEXT("Launchers receive one refresh"), Observer->NotificationCount, 1);
		TestFalse(TEXT("Repeated opens do not change onboarding history"), Session.Progress->MarkIntroductionSeen());
		TestEqual(TEXT("Repeated opens do not broadcast redundant refreshes"), Observer->NotificationCount, 1);
	}
	Profile.ReloadFromDisk();
	{
		FFF_CodexIntroductionTests::FSession Restarted;
		TestTrue(TEXT("A new game instance reloads the one-time guidance history"), Restarted.Progress->HasSeenIntroduction());
		TestTrue(TEXT("Guidance persistence preserves discovery history"), Restarted.Progress->IsDiscovered(TEXT("DiscoveredBeforeOpening")));
		TestTrue(TEXT("The restarted profile records a new discovery"),
			Restarted.Progress->RecordDiscoveries({TEXT("DiscoveredAfterRestart")}));
	}
	Profile.ReloadFromDisk();
	{
		FFF_CodexIntroductionTests::FSession Reloaded;
		TestTrue(TEXT("Later discovery saves preserve guidance history"), Reloaded.Progress->HasSeenIntroduction());
		TestTrue(TEXT("New discoveries still save normally"), Reloaded.Progress->IsDiscovered(TEXT("DiscoveredAfterRestart")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexIntroductionMigrationTest,
	"FestaFiesta.Building.Codex.Introduction.RewardMigrationPreservesSeenHistory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_FacilityCodexIntroductionMigrationTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("Profile persistence is available"), GConfig)) { return false; }
	FFF_CodexIntroductionTests::FScopedProfile Profile;
	GConfig->SetBool(TEXT("FestaFiesta.FacilityCodex"), TEXT("bHasSeenIntroduction"), true, Profile.Path);
	GConfig->SetArray(TEXT("FestaFiesta.FacilityCodex"), TEXT("DiscoveredRuleIds"), {TEXT("LegacyDiscovery")}, Profile.Path);
	{
		FFF_CodexIntroductionTests::FSession Migrating;
		TestTrue(TEXT("Legacy reward migration reads existing guidance first"), Migrating.Progress->HasSeenIntroduction());
		TestTrue(TEXT("Reward migration still preserves paid history"), Migrating.Progress->IsDiscoveryRewardClaimed(TEXT("LegacyDiscovery")));
	}
	Profile.ReloadFromDisk();
	{
		FFF_CodexIntroductionTests::FSession Reloaded;
		TestTrue(TEXT("Migration's save cannot erase existing guidance history"), Reloaded.Progress->HasSeenIntroduction());
		TestTrue(TEXT("Migrated reward history also survives restart"), Reloaded.Progress->IsDiscoveryRewardClaimed(TEXT("LegacyDiscovery")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildingLightingIntroductionPersistenceTest,
	"FestaFiesta.Building.Introduction.LightingHistorySurvivesRestartAndSharesInspectorHistory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_BuildingLightingIntroductionPersistenceTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("Profile persistence is available"), GConfig)) return false;
	const FName LightingId = UFF_BuildingFeatureDiscoverySubsystem::LightingControlsFeatureId();
	const TCHAR* LightingSection = TEXT("FestaFiesta.FacilityLighting");
	const TCHAR* LightingSeenKey = TEXT("bHasSeenColorIntroduction");
	{
		FFF_CodexIntroductionTests::FScopedProfile Profile;
		{
			FFF_CodexIntroductionTests::FFeatureSession Session;
			TestFalse(TEXT("A new profile is eligible for the catalog lighting explanation"), Session.Discovery->HasSeenFeature(LightingId));
			TestTrue(TEXT("Acknowledging the catalog explanation records it"), Session.Discovery->MarkFeatureSeen(LightingId));
			TestTrue(TEXT("Catalog guidance immediately reads its acknowledged state"), Session.Discovery->HasSeenFeature(LightingId));
			bool bSeenInInspector = false;
			TestTrue(TEXT("Catalog acknowledgement writes the inspector's existing history key"),
				GConfig->GetBool(LightingSection, LightingSeenKey, bSeenInInspector, Profile.Path));
			TestTrue(TEXT("The inspector will not repeat acknowledged catalog guidance"), bSeenInInspector);
			TestFalse(TEXT("Repeated acknowledgements do not record another change"), Session.Discovery->MarkFeatureSeen(LightingId));
		}
		Profile.ReloadFromDisk();
		{
			FFF_CodexIntroductionTests::FFeatureSession Restarted;
			TestTrue(TEXT("A fresh subsystem reloads the lighting explanation history from disk"), Restarted.Discovery->HasSeenFeature(LightingId));
			TestFalse(TEXT("Reloaded feature history makes acknowledgement idempotent"), Restarted.Discovery->MarkFeatureSeen(LightingId));
			bool bSeenInInspector = false;
			GConfig->GetBool(LightingSection, LightingSeenKey, bSeenInInspector, Profile.Path);
			TestTrue(TEXT("Shared inspector history also survives a restart"), bSeenInInspector);
		}
	}
	{
		// Simulate a player who already learned lighting in the facility inspector,
		// without any newer catalog feature-history entry in their profile.
		FFF_CodexIntroductionTests::FScopedProfile LegacyProfile;
		{
			FFF_CodexIntroductionTests::FFeatureSession Session;
			TestFalse(TEXT("A separate legacy fixture starts without lighting history"), Session.Discovery->HasSeenFeature(LightingId));
			GConfig->SetBool(LightingSection, LightingSeenKey, true, LegacyProfile.Path);
			GConfig->Flush(false, LegacyProfile.Path);
			TestTrue(TEXT("An inspector acknowledgement suppresses catalog guidance in the same session"), Session.Discovery->HasSeenFeature(LightingId));
			TestFalse(TEXT("Inspector lighting history does not consume other feature explanations"), Session.Discovery->HasSeenFeature(TEXT("UnrelatedFeature")));
		}
		LegacyProfile.ReloadFromDisk();
		{
			FFF_CodexIntroductionTests::FFeatureSession Restarted;
			TestTrue(TEXT("Existing inspector history suppresses catalog guidance after restart"), Restarted.Discovery->HasSeenFeature(LightingId));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildFeatureGuideAcknowledgementTest,
	"FestaFiesta.Building.Introduction.GuideRequiresConfirmationAndPreservesToolbarStyle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFFBuildFeatureGuideAcknowledgementTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("Profile persistence is available"), GConfig)) return false;
	FFF_CodexIntroductionTests::FScopedProfile Profile;
	FFF_CodexIntroductionTests::FFeatureSession Session;
	TStrongObjectPtr<UFF_BuildToolsWidget> Widget(NewObject<UFF_BuildToolsWidget>());
	Widget->WidgetTree = NewObject<UWidgetTree>(Widget.Get());
	Widget->FeatureDiscovery = Session.Discovery.Get();
	Widget->Panel_FeatureGuide = Widget->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel_FeatureGuide"));
	Widget->Text_FeatureGuideTitle = NewObject<UTextBlock>(Widget.Get());
	Widget->Text_FeatureGuideBody = NewObject<UTextBlock>(Widget.Get());
	Widget->Text_FeatureGuideHint = NewObject<UTextBlock>(Widget.Get());
	Widget->Button_FeatureGuideConfirm = NewObject<UButton>(Widget.Get());
	Widget->Button_FeatureGuideClose = NewObject<UButton>(Widget.Get());
	Widget->Combo_FeatureGuideTopic = NewObject<UComboBoxString>(Widget.Get());
	for (const auto& Topic : FFBuildFeatureGuide::Topics())
		Widget->Combo_FeatureGuideTopic->AddOption(Topic.Title.ToString());
	Widget->Panel_FeatureGuide->SetVisibility(ESlateVisibility::Collapsed);
	// UWidget::IsVisible reads the live Slate surface; retain it just as the viewport does.
	const TSharedRef<SWidget> GuideSurface = Widget->Panel_FeatureGuide->TakeWidget();
	UButton* Grid = Widget->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Grid"));
	Widget->WidgetTree->RootWidget = Grid;
	const FLinearColor AuthoredTint(.31f, .42f, .53f, 1.f);
	Grid->SetBackgroundColor(AuthoredTint);
	Grid->SetToolTipText(FText::FromString(TEXT("Authored grid tooltip")));
	// Match the toolbar hierarchy: a separate badge shares the unchanged icon canvas.
	auto* ButtonContents = Widget->WidgetTree->ConstructWidget<UVerticalBox>();
	Grid->SetContent(ButtonContents);
	auto* IconSize = Widget->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Size_BuildToolbarLine2"));
	ButtonContents->AddChildToVerticalBox(IconSize);
	auto* IconContents = Widget->WidgetTree->ConstructWidget<UCanvasPanel>();
	IconSize->SetContent(IconContents);
	auto* IconFill = Widget->WidgetTree->ConstructWidget<UBorder>();
	FSlateBrush AuthoredFill;
	AuthoredFill.DrawAs = ESlateBrushDrawType::Box;
	AuthoredFill.TintColor = FSlateColor(FLinearColor(.11f, .22f, .33f, .8f));
	AuthoredFill.OutlineSettings.Color = FSlateColor(FLinearColor::Transparent);
	IconFill->SetBrush(AuthoredFill);
	const FLinearColor AuthoredBrushColor(.65f, .75f, .85f, 1.f);
	IconFill->SetBrushColor(AuthoredBrushColor);
	IconContents->AddChildToCanvas(IconFill);
	auto* IconOutline = Widget->WidgetTree->ConstructWidget<UBorder>();
	FSlateBrush AuthoredOutline;
	AuthoredOutline.DrawAs = ESlateBrushDrawType::RoundedBox;
	AuthoredOutline.TintColor = FSlateColor(FLinearColor::Transparent);
	AuthoredOutline.OutlineSettings.Color = FSlateColor(FLinearColor(.21f, .32f, .43f, .7f));
	AuthoredOutline.OutlineSettings.Width = 2.f;
	IconOutline->SetBrush(AuthoredOutline);
	IconContents->AddChildToCanvas(IconOutline);
	auto* IconText = Widget->WidgetTree->ConstructWidget<UTextBlock>();
	const FLinearColor AuthoredIconText(.24f, .35f, .46f, 1.f);
	IconText->SetColorAndOpacity(FSlateColor(AuthoredIconText));
	IconContents->AddChildToCanvas(IconText);
	auto* Badge = Widget->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Badge_BuildGrid"));
	Badge->SetVisibility(ESlateVisibility::Collapsed);
	auto* BadgeSlot = IconContents->AddChildToCanvas(Badge);
	BadgeSlot->SetPosition(FVector2D(32.f, -3.f));
	BadgeSlot->SetSize(FVector2D(18.f, 18.f));
	auto* BadgeText = Widget->WidgetTree->ConstructWidget<UTextBlock>();
	BadgeText->SetText(FText::GetEmpty());
	BadgeText->SetVisibility(ESlateVisibility::Collapsed);
	Badge->SetContent(BadgeText);
	auto* ButtonLabel = Widget->WidgetTree->ConstructWidget<UTextBlock>();
	const FLinearColor AuthoredLabel(.14f, .25f, .36f, 1.f);
	ButtonLabel->SetText(FText::FromString(TEXT("격자")));
	ButtonLabel->SetColorAndOpacity(FSlateColor(AuthoredLabel));
	ButtonContents->AddChildToVerticalBox(ButtonLabel);
	Widget->RefreshFeatureHints();
	TestEqual(TEXT("Unread guidance displays a click-through dot badge"), Badge->GetVisibility(), ESlateVisibility::HitTestInvisible);
	TestTrue(TEXT("Unread guidance preserves the button background"), Grid->GetBackgroundColor().Equals(AuthoredTint));
	TestTrue(TEXT("Unread guidance preserves the caption below the icon"), ButtonLabel->GetColorAndOpacity().GetSpecifiedColor().Equals(AuthoredLabel));
	TestTrue(TEXT("Unread guidance preserves the icon's original fill brush"), IconFill->Background == AuthoredFill);
	TestTrue(TEXT("Unread guidance preserves the icon's original outline brush"), IconOutline->Background == AuthoredOutline);
	TestTrue(TEXT("Unread guidance preserves the icon's original brush color"), IconFill->GetBrushColor().Equals(AuthoredBrushColor));
	TestTrue(TEXT("Unread guidance preserves text inside the icon"), IconText->GetColorAndOpacity().GetSpecifiedColor().Equals(AuthoredIconText));
	Widget->RequestFeatureToolbarAction(TEXT("BuildGrid"));
	TestTrue(TEXT("A fresh toolbar click opens its explanation first"), Widget->IsFeatureGuideOpen());
	TestEqual(TEXT("The selected action is held until confirmation"), Widget->PendingGuideAction, FName(TEXT("BuildGrid")));
	TestFalse(TEXT("Showing the explanation does not mark it read"), Session.Discovery->HasSeenFeature(TEXT("BuildGrid")));
	TestEqual(TEXT("Opening the explanation keeps its unread badge"), Badge->GetVisibility(), ESlateVisibility::HitTestInvisible);
	TestFalse(TEXT("First-use topics cannot switch the pending action"), Widget->Combo_FeatureGuideTopic->GetIsEnabled());
	Widget->CloseFeatureGuide();
	TestTrue(TEXT("Dismissing cancels the pending action"), Widget->PendingGuideAction.IsNone());
	TestFalse(TEXT("Dismissal does not silently acknowledge it"), Session.Discovery->HasSeenFeature(TEXT("BuildGrid")));
	Widget->RefreshFeatureHints();
	TestEqual(TEXT("Dismissed guidance retains its unread badge"), Badge->GetVisibility(), ESlateVisibility::HitTestInvisible);
	TestTrue(TEXT("Dismissed guidance leaves the original icon fill unchanged"), IconFill->Background == AuthoredFill);
	Widget->RequestFeatureToolbarAction(TEXT("BuildGrid"));
	TestTrue(TEXT("Dismissed help remains available on the next click"), Widget->IsFeatureGuideOpen());
	Widget->ConfirmFeatureGuide();
	TestTrue(TEXT("Explicit confirmation records only the viewed feature"), Session.Discovery->HasSeenFeature(TEXT("BuildGrid")));
	TestFalse(TEXT("Other toolbar features remain unseen"), Session.Discovery->HasSeenFeature(TEXT("BuildSnap")));
	TestFalse(TEXT("Confirmation closes the panel"), Widget->IsFeatureGuideOpen());
	TestEqual(TEXT("Confirmation removes the acknowledged feature's badge"), Badge->GetVisibility(), ESlateVisibility::Collapsed);
	TestTrue(TEXT("Confirmation leaves the designer's button background unchanged"), Grid->GetBackgroundColor().Equals(AuthoredTint));
	TestTrue(TEXT("Confirmation leaves the caption color unchanged"), ButtonLabel->GetColorAndOpacity().GetSpecifiedColor().Equals(AuthoredLabel));
	TestTrue(TEXT("Confirmation preserves the icon's original fill brush"), IconFill->Background == AuthoredFill);
	TestTrue(TEXT("Confirmation preserves the icon's original outline brush"), IconOutline->Background == AuthoredOutline);
	TestTrue(TEXT("Confirmation preserves the icon's original brush color"), IconFill->GetBrushColor().Equals(AuthoredBrushColor));
	TestTrue(TEXT("Confirmation preserves the icon text color"), IconText->GetColorAndOpacity().GetSpecifiedColor().Equals(AuthoredIconText));
	TestEqual(TEXT("First-use tooltip is restored"), Grid->GetToolTipText().ToString(), FString(TEXT("Authored grid tooltip")));
	Widget->RequestFeatureToolbarAction(TEXT("BuildGrid"));
	TestFalse(TEXT("Later toolbar clicks no longer show the explanation"), Widget->IsFeatureGuideOpen());
	TestEqual(TEXT("Later toolbar clicks keep the acknowledged badge hidden"), Badge->GetVisibility(), ESlateVisibility::Collapsed);
	Widget->ShowFeatureGuide(TEXT("BuildGrid"), false);
	TestTrue(TEXT("The manual guide can reopen an acknowledged feature"), Widget->IsFeatureGuideOpen());
	TestTrue(TEXT("Manual browsing never queues an actual toolbar operation"), Widget->PendingGuideAction.IsNone());
	TestTrue(TEXT("Manual browsing permits topic selection"), Widget->Combo_FeatureGuideTopic->GetIsEnabled());
	Widget->HandleFeatureGuideTopicChanged(TEXT("즐겨찾기"), ESelectInfo::OnMouseClick);
	TestEqual(TEXT("The manual guide changes its topic"), Widget->CurrentGuideFeature, FName(TEXT("BuildFavorites")));
	TestFalse(TEXT("Browsing another explanation does not acknowledge it"), Session.Discovery->HasSeenFeature(TEXT("BuildFavorites")));
	Widget->CloseFeatureGuide();
	Profile.ReloadFromDisk();
	FFF_CodexIntroductionTests::FFeatureSession Restarted;
	TestTrue(TEXT("Confirmed toolbar history survives restart"), Restarted.Discovery->HasSeenFeature(TEXT("BuildGrid")));
	TestFalse(TEXT("Viewed but unconfirmed history stays unseen after restart"), Restarted.Discovery->HasSeenFeature(TEXT("BuildFavorites")));
	Widget->FeatureDiscovery = Restarted.Discovery.Get();
	Widget->RefreshFeatureHints();
	TestEqual(TEXT("Reloaded acknowledgement keeps its badge hidden"), Badge->GetVisibility(), ESlateVisibility::Collapsed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildFeatureGuideTopicsTest,
	"FestaFiesta.Building.Introduction.GuideTopicsAndLegacyCodexHistory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFFBuildFeatureGuideTopicsTest::RunTest(const FString& Parameters)
{
	TSet<FName> Ids, Buttons;
	for (const auto& Topic : FFBuildFeatureGuide::Topics())
	{
		TestFalse(TEXT("Feature IDs are unique and nonempty"), Topic.Id.IsNone() || Ids.Contains(Topic.Id));
		TestFalse(TEXT("Every feature has a title and instructions"), Topic.Title.IsEmpty() || Topic.Body.IsEmpty());
		Ids.Add(Topic.Id);
		if (!Topic.ToolbarButton.IsNone()) Buttons.Add(Topic.ToolbarButton);
	}
	TestEqual(TEXT("The guide includes all eleven requested subjects"), Ids.Num(), 11);
	TestEqual(TEXT("Seven toolbar controls have independent first-use history"), Buttons.Num(), 7);
	TestNull(TEXT("Unknown feature IDs do not show misleading guidance"), FFBuildFeatureGuide::Find(TEXT("Unknown")));
	if (!TestNotNull(TEXT("Profile persistence is available"), GConfig)) return false;
	FFF_CodexIntroductionTests::FScopedProfile Profile;
	FFF_CodexIntroductionTests::FFeatureSession Session;
	const FName CodexId = UFF_BuildingFeatureDiscoverySubsystem::FacilityCodexFeatureId();
	GConfig->SetBool(TEXT("FestaFiesta.FacilityCodex"), TEXT("bHasSeenIntroduction"), true, Profile.Path);
	TestTrue(TEXT("Manual guide shares existing codex learning history"), Session.Discovery->HasSeenFeature(CodexId));
	GConfig->SetBool(TEXT("FestaFiesta.FacilityCodex"), TEXT("bHasSeenIntroduction"), false, Profile.Path);
	Session.Discovery->MarkFeatureSeen(CodexId);
	FFF_CodexIntroductionTests::FSession CodexSession;
	TestTrue(TEXT("Guide acknowledgement suppresses the codex's own introduction"), CodexSession.Progress->HasSeenIntroduction());
	return true;
}

#endif
