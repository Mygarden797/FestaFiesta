// 역할: 도감 목록·상세 화면 이동과 수집 보상의 중복 지급 방지를 실제 위젯으로 검증한다.
// 임시 저장 파일을 사용하므로 테스트가 플레이어의 수집 기록을 바꾸지 않는다.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Building/Codex/FF_FacilityCodexProgressSubsystem.h"
#include "Building/Codex/FF_FacilityCodexSettings.h"
#include "Building/Codex/FF_FacilityCodexSubsystem.h"
#include "Building/FF_FacilityUpgradeTestPolicy.h"
#include "Building/Synergy/FF_FacilitySynergySettings.h"
#include "Building/Synergy/FF_FacilitySynergyTypes.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"
#include "TimerManager.h"
#include "UI/FacilityCodex/FF_FacilityCodexCard.h"
#include "UI/FacilityCodex/FF_FacilityCodexPresenterSubsystem.h"
#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"
#include "UI/PrepHUD/Panels/FF_PrepChoiceButton.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
struct FPlayerlessWidgetClassContext
{
	explicit FPlayerlessWidgetClassContext(UWidgetBlueprintGeneratedClass& InClass)
		: Class(InClass), bPrevious(InClass.bCanCallInitializedWithoutPlayerContext)
	{
		Class.bCanCallInitializedWithoutPlayerContext = true;
	}
	~FPlayerlessWidgetClassContext()
	{
		Class.bCanCallInitializedWithoutPlayerContext = bPrevious;
	}
	UWidgetBlueprintGeneratedClass& Class;
	bool bPrevious;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexCollectionScreenTest,
	"FestaFiesta.FacilityCodex.UI.CollectionPagesAndMilestoneClaims",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityCodexCollectionScreenTest::RunTest(const FString& Parameters)
{
	const FString PreviousProfile = GGameUserSettingsIni;
	FString Profile = FPaths::ConvertRelativePathToFull(FPaths::CreateTempFilename(
		*FPaths::ProjectSavedDir(), TEXT("CollectionUiTest"), TEXT(".ini")));
	FPaths::NormalizeFilename(Profile);
	if (!TestNotNull(TEXT("Profile persistence is available"), GConfig)) return false;
	GConfig->Add(Profile, FConfigFile());
	GGameUserSettingsIni = Profile;
	ON_SCOPE_EXIT { GConfig->UnloadFile(Profile); GGameUserSettingsIni = PreviousProfile; IFileManager::Get().Delete(*Profile); };
	TStrongObjectPtr<UDataTable> Rules(NewObject<UDataTable>());
	Rules->RowStruct = FFF_FacilitySynergyRow::StaticStruct();
	TArray<FFF_FacilityCodexCollectionReward> Rewards;
	TSet<FName> Discoveries;
	for (int32 Index = 1; Index <= 3; ++Index)
	{
		FFF_FacilitySynergyRow Row;
		Row.TargetFacilityId = FName(*FString::Printf(TEXT("CollectionUiTarget%d"), Index));
		Row.PartnerFacilityId = FName(*FString::Printf(TEXT("CollectionUiPartner%d"), Index));
		Row.DisplayName = FText::FromString(FString::Printf(TEXT("Collection %d"), Index));
		Row.DiscoveryRewardGold = 100;
		const FName RuleId(*FString::Printf(TEXT("CollectionUiRule%d"), Index));
		Rules->AddRow(RuleId, Row);
		Discoveries.Add(RuleId);
		auto& Reward = Rewards.AddDefaulted_GetRef();
		Reward.MilestoneId = FName(*FString::Printf(TEXT("CollectionUiStage%d"), Index));
		Reward.RequiredDiscoveries = Index;
		Reward.RewardGold = Index * 111;
	}
	TGuardValue<TSoftObjectPtr<UDataTable>> RuleGuard(
		GetMutableDefault<UFF_FacilitySynergySettings>()->SynergyTable, TSoftObjectPtr<UDataTable>(Rules.Get()));
	TGuardValue<TArray<FFF_FacilityCodexCollectionReward>> RewardGuard(
		GetMutableDefault<UFF_FacilityCodexSettings>()->CollectionRewards, Rewards);
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
	UWorld* World = Fixture.GetTestWorld();
	UGameInstance* Game = World->GetGameInstance();
	auto* Codex = World->GetSubsystem<UFF_FacilityCodexSubsystem>();
	auto* Progress = Game->GetSubsystem<UFF_FacilityCodexProgressSubsystem>();
	auto* Economy = Game->GetSubsystem<UFF_FestivalProgressionSubsystem>();
	if (!TestNotNull(TEXT("Codex"), Codex) || !TestNotNull(TEXT("Progress"), Progress) || !TestNotNull(TEXT("Economy"), Economy)) return false;
	UFF_FestivalDefinition* Definition = NewObject<UFF_FestivalDefinition>(Game);
	UDataTable* Costs = NewObject<UDataTable>(Definition);
	Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
	Costs->AddRow(TEXT("Cost.Booth.FoodStall"), FFF_FacilityCostRow());
	Definition->FacilityCostTable = Costs;
	Definition->FestivalId = TEXT("CollectionUiAutomation");
	Definition->InitialGold = 1000;
	Economy->StartFestival(Definition);
	Progress->RecordDiscoveries(Discoveries);
	Codex->OnWorldBeginPlay(*World);
	World->GetTimerManager().Tick(.016f);

	auto* ScreenClass = Cast<UWidgetBlueprintGeneratedClass>(
		UFF_FacilityCodexPresenterSubsystem::GetDefaultScreenClassPath().TryLoadClass<UFF_FacilityCodexScreen>());
	const FString Root = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Controls/");
	auto* CategoryClass = Cast<UWidgetBlueprintGeneratedClass>(FSoftClassPath(Root +
		TEXT("WBP_FF_CodexCollectionCategory.WBP_FF_CodexCollectionCategory_C")).TryLoadClass<UFF_PrepChoiceButton>());
	auto* RewardClass = Cast<UWidgetBlueprintGeneratedClass>(FSoftClassPath(Root +
		TEXT("WBP_FF_CodexCollectionReward.WBP_FF_CodexCollectionReward_C")).TryLoadClass<UFF_PrepChoiceButton>());
	if (!TestNotNull(TEXT("Saved collection screen"), ScreenClass) || !TestNotNull(TEXT("Saved category button"), CategoryClass)
		|| !TestNotNull(TEXT("Saved milestone button"), RewardClass)) return false;
	FPlayerlessWidgetClassContext ScreenContext(*ScreenClass);
	FPlayerlessWidgetClassContext CategoryContext(*CategoryClass);
	FPlayerlessWidgetClassContext RewardContext(*RewardClass);
	TStrongObjectPtr<UFF_FacilityCodexScreen> Screen(NewObject<UFF_FacilityCodexScreen>(World, ScreenClass));
	if (!Screen->Initialize()) return false;
	// Match the real presenter: launch filtering happens before AddToViewport/NativeConstruct.
	Screen->SetFacilityFilter(NAME_None);
	TSharedPtr<SWidget> Slate = Screen->TakeWidget();
	ON_SCOPE_EXIT { Slate.Reset(); Screen->ReleaseSlateResources(true); };
	auto* Tile = Cast<UFF_FacilityCodexTileView>(Screen->GetWidgetFromName(TEXT("TileView_Entries")));
	auto* Close = Cast<UButton>(Screen->GetWidgetFromName(TEXT("Button_Close")));
	auto* Next = Cast<UButton>(Screen->GetWidgetFromName(TEXT("Button_CollectionNext")));
	auto* Claim = Cast<UButton>(Screen->GetWidgetFromName(TEXT("Button_DetailClaimReward")));
	auto* Categories = Cast<UVerticalBox>(Screen->GetWidgetFromName(TEXT("Column_CollectionCategories")));
	auto* Stages = Cast<UHorizontalBox>(Screen->GetWidgetFromName(TEXT("Row_CollectionMilestones")));
	auto* Feedback = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_RewardFeedback")));
	if (!TestNotNull(TEXT("Tile view"), Tile) || !TestNotNull(TEXT("Back"), Close) || !TestNotNull(TEXT("Next"), Next)
		|| !TestNotNull(TEXT("Claim"), Claim) || !TestNotNull(TEXT("Categories"), Categories)
		|| !TestNotNull(TEXT("Collection stages"), Stages) || !TestNotNull(TEXT("Reward feedback"), Feedback)) return false;
	if (!TestEqual(TEXT("All recipes are initially listed"), Tile->GetNumItems(), 3)) return false;
	TestEqual(TEXT("The collection sidebar contains only All combinations after the pre-construction refresh"),
		Categories->GetChildrenCount(), 1);
	TestFalse(TEXT("The catalog opens on its list page"), Screen->IsCombinationDetailOpen());
	int32 CloseRequests = 0;
	Screen->OnCloseRequested.AddLambda([&CloseRequests]() { ++CloseRequests; });
	ON_SCOPE_EXIT { Screen->OnCloseRequested.Clear(); };
	Tile->OnItemClicked().Broadcast(Tile->GetListItems()[0]);
	TestTrue(TEXT("Activating a card opens a separate detail page"), Screen->IsCombinationDetailOpen());
	Next->OnClicked.Broadcast();
	const auto* Selected = Tile->GetSelectedItem<UFF_FacilityCodexItem>();
	TestTrue(TEXT("Next detail also selects its corresponding list card"), Selected && Selected->Entry.RuleId == Screen->GetSelectedRuleId());
	Close->OnClicked.Broadcast();
	TestFalse(TEXT("First back returns to the list"), Screen->IsCombinationDetailOpen());
	TestEqual(TEXT("Detail back does not close the modal"), CloseRequests, 0);
	Tile->OnItemClicked().Broadcast(Tile->GetSelectedItem<UFF_FacilityCodexItem>());
	TestTrue(TEXT("Clicking the already selected card opens detail again"), Screen->IsCombinationDetailOpen());
	Screen->SetDiscoveryFilter(EFF_FacilityCodexFilterState::RewardReady);
	for (int32 Index = 0; Index < 3; ++Index) Claim->OnClicked.Broadcast();
	TestEqual(TEXT("All three individual rewards remain claimable through detail"), Economy->GetGold(), 1300);
	TestEqual(TEXT("The reward-ready list is empty after the final individual claim"), Screen->GetVisibleEntryCount(), 0);
	TestFalse(TEXT("Final filtered claim returns to list controls instead of an empty detail"), Screen->IsCombinationDetailOpen());
	Screen->SetDiscoveryFilter(EFF_FacilityCodexFilterState::All);
	Screen->SetFacilityFilter(TEXT("CollectionUiTarget1"));
	TestEqual(TEXT("Inspector launch filtering selects one recipe"), Screen->GetVisibleEntryCount(), 1);
	auto* AllCategory = Categories->GetChildrenCount() ? Cast<UFF_PrepChoiceButton>(Categories->GetChildAt(0)) : nullptr;
	auto* AllButton = AllCategory ? Cast<UButton>(AllCategory->GetWidgetFromName(TEXT("Button_Root"))) : nullptr;
	if (!TestNotNull(TEXT("All combinations control"), AllButton)) return false;
	auto* AllLabel = Cast<UTextBlock>(AllCategory->GetWidgetFromName(TEXT("Text_Label")));
	if (!TestNotNull(TEXT("All combinations label"), AllLabel)) return false;
	TestEqual(TEXT("The single navigation action is labelled All combinations"), AllLabel->GetText().ToString(), FString(TEXT("전체 조합")));
	const auto* FilteredItem = Tile->GetListItems().IsEmpty() ? nullptr : Cast<UFF_FacilityCodexItem>(Tile->GetListItems()[0]);
	if (!TestNotNull(TEXT("Facility-filtered recipe remains available before testing the legacy category API"), FilteredItem)) return false;
	const EFFZSFacilityCategory DifferentCategory = FilteredItem->Entry.TargetCategory == EFFZSFacilityCategory::Booth
		? EFFZSFacilityCategory::Rest : EFFZSFacilityCategory::Booth;
	Screen->SetCategoryFilter(true, DifferentCategory);
	TestEqual(TEXT("The legacy category API still filters recipes without adding sidebar controls"), Screen->GetVisibleEntryCount(), 0);
	TestEqual(TEXT("Legacy category filtering does not recreate detailed category buttons"), Categories->GetChildrenCount(), 1);
	TestFalse(TEXT("All combinations is not selected while facility or category restrictions remain"), AllCategory->IsSelected());
	AllButton->OnClicked.Broadcast();
	TestEqual(TEXT("All combinations clears both facility and category restrictions"), Screen->GetVisibleEntryCount(), 3);
	TestTrue(TEXT("The same single All combinations button becomes selected after resetting filters"),
		Categories->GetChildrenCount() == 1 && Categories->GetChildAt(0) == AllCategory && AllCategory->IsSelected());
	if (!TestEqual(TEXT("Milestone controls reflect authored stages"), Stages->GetChildrenCount(), 3)) return false;
	const auto StageButton = [Stages](int32 Index) -> UButton*
	{
		auto* Choice = Cast<UFF_PrepChoiceButton>(Stages->GetChildAt(Index));
		return Choice ? Cast<UButton>(Choice->GetWidgetFromName(TEXT("Button_Root"))) : nullptr;
	};
	if (!TestNotNull(TEXT("First milestone action"), StageButton(0)) || !TestNotNull(TEXT("Second milestone action"), StageButton(1))
		|| !TestNotNull(TEXT("Third milestone action"), StageButton(2))) return false;
	StageButton(0)->OnClicked.Broadcast();
	StageButton(0)->OnClicked.Broadcast();
	TestEqual(TEXT("The collection UI pays its stage once alongside individual rewards"), Economy->GetGold(), 1411);
	TestTrue(TEXT("The collection stage is persisted"), Progress->IsCollectionRewardClaimed(Rewards[0].MilestoneId));
	Definition->InitialGold = MAX_int32 - 10;
	Economy->RestartFestival();
	TestEqual(TEXT("New festival clears collection UI discovery progress"), Codex->GetSummary().DiscoveredCount, 0);
	TestFalse(TEXT("New festival clears collection reward claims"), Progress->IsCollectionRewardClaimed(Rewards[0].MilestoneId));
	Progress->RecordDiscoveries(Discoveries);
	StageButton(1)->OnClicked.Broadcast();
	TestEqual(TEXT("Milestone reward safely saturates the existing Gold wallet"), Economy->GetGold(), MAX_int32);
	TestTrue(TEXT("The toast reports the ten Gold actually credited"), Feedback->GetText().ToString().Contains(TEXT("10")));
	TestFalse(TEXT("The toast does not advertise the unpaid authored amount"), Feedback->GetText().ToString().Contains(TEXT("222")));
	Definition->InitialGold = 1000;
	Economy->RestartFestival();
	Progress->RecordDiscoveries(Discoveries);
	TStrongObjectPtr<UFF_FacilityUpgradeTestPolicy> Observer(NewObject<UFF_FacilityUpgradeTestPolicy>(Game));
	Observer->ObserveGoldChanged = [&]() { Screen->SetPreviewEntries({}); };
	Economy->OnGoldChanged.AddDynamic(Observer.Get(), &UFF_FacilityUpgradeTestPolicy::HandleGoldChanged);
	StageButton(2)->OnClicked.Broadcast();
	Economy->OnGoldChanged.RemoveDynamic(Observer.Get(), &UFF_FacilityUpgradeTestPolicy::HandleGoldChanged);
	TestTrue(TEXT("Reentrant preview replacement does not recreate a stale success toast"), Feedback->GetText().IsEmpty());
	TestTrue(TEXT("The authoritative claim still completes before screen replacement"), Progress->IsCollectionRewardClaimed(Rewards[2].MilestoneId));
	Screen->OnCloseRequested.Clear();
	return true;
}
#endif
