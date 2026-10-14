// 역할: 실제 순위 버튼의 시설 정보 전환과 실패 시 표시 보존을 검증한다.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
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
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Slate/SceneViewport.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_ProjectZoneTestFixture.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/Core/FF_UIFlowSubsystem.h"
#include "UI/Core/FF_UIForegroundRules.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UI/Facility/FF_FacilityInfoSubsystem.h"
#include "UI/FacilityRevenue/FF_FacilityRevenueRanking.h"
#include "UI/LiveHUD/FF_LiveHudScreen.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SViewport.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityRevenueInspectionTest,
	"FestaFiesta.UI.FacilityRevenue.InspectCurrentStageAndFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityRevenueInspectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// 첫 사용 안내가 실제 사용자 설정에 기록되지 않도록 독립 설정 파일을 쓴다.
	FString SettingsPath = FPaths::ConvertRelativePathToFull(FPaths::CreateTempFilename(
		*FPaths::ProjectSavedDir(), TEXT("RevenueInspectionTest"), TEXT(".ini")));
	FPaths::NormalizeFilename(SettingsPath);
	if (GConfig) GConfig->Add(SettingsPath, FConfigFile());
	TGuardValue<FString> UserSettings(GGameUserSettingsIni, SettingsPath);
	ON_SCOPE_EXIT
	{
		if (GConfig) GConfig->UnloadFile(SettingsPath);
		IFileManager::Get().Delete(*SettingsPath);
	};
	UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
	TGuardValue<bool> GroundChecks(Settings->bEnableGroundChecks, false);
	TGuardValue<bool> CollisionChecks(Settings->bEnableWorldCollisionChecks, false);
	TGuardValue<bool> NavigationChecks(Settings->bEnableNavigationProjectionChecks, false);
	TGuardValue<bool> PolicyDomains(Settings->bRequireProductionPolicyDomains, false);
	TGuardValue<EFFZSChangePhase> InitialPhase(Settings->InitialPhase, EFFZSChangePhase::InitialSetup);
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game))
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Fixture.GetTestWorld();
	UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
	if (!TestNotNull(TEXT("독립 게임 월드"), World) || !TestNotNull(TEXT("게임 인스턴스"), Game)) return false;
	auto* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
	auto* Info = World->GetSubsystem<UFF_FacilityInfoSubsystem>();
	auto* Ranking = World->GetSubsystem<UFF_FacilityRevenueRankingSubsystem>();
	auto* Build = World->GetSubsystem<UFF_BuildModeSubsystem>();
	auto* Stage = Game->GetSubsystem<UFF_FestivalStageSubsystem>();
	if (!TestNotNull(TEXT("시설 원본"), Zones) || !TestNotNull(TEXT("시설 정보 시스템"), Info) ||
		!TestNotNull(TEXT("순위 시스템"), Ranking) || !TestNotNull(TEXT("건축 시스템"), Build) ||
		!TestNotNull(TEXT("단계 시스템"), Stage)) return false;

	// 실제 AddToViewport와 로컬 소유자를 사용하되 창·PIE·운영 맵을 열지 않는다.
	FWorldContext* Context = GEngine->GetWorldContextFromWorld(World);
	if (!TestNotNull(TEXT("월드 문맥"), Context)) return false;
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
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	if (!TestNotNull(*FString::Printf(TEXT("로컬 플레이어: %s"), *PlayerError), Player) ||
		!TestNotNull(TEXT("플레이어 컨트롤러"), Controller)) return false;
	Controller->SetPlayer(Player);
	World->AddController(Controller);
	const TSharedPtr<SWidget> PreviousFocus = FSlateApplication::Get().GetKeyboardFocusedWidget();
	ON_SCOPE_EXIT
	{
		Ranking->Close();
		Build->ExitBuildMode();
		Info->ClearSelection();
		if (PreviousFocus.IsValid()) FSlateApplication::Get().SetKeyboardFocus(PreviousFocus);
		else FSlateApplication::Get().ClearKeyboardFocus();
	};

	AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
	if (!FFProjectZoneTest::RegisterZone(*this, Zone, Zones)) return false;
	// 기존 FacilityInspection 검사와 같은 합성 축제·카테고리 비용 행을 공급한다.
	// 설치비 조회는 실제 정책을 통과시키며 프로젝트의 원본 데이터는 변경하지 않는다.
	auto* Progression = Game->GetSubsystem<UFF_FestivalProgressionSubsystem>();
	if (!TestNotNull(TEXT("축제 경제 원본"), Progression)) return false;
	UFF_FestivalDefinition* Festival = NewObject<UFF_FestivalDefinition>(Game);
	UDataTable* Costs = NewObject<UDataTable>(Game);
	Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
	FFF_FacilityCostRow CostRow;
	CostRow.InstallCost = 100;
	Costs->AddRow(TEXT("Cost.Booth.Default"), CostRow);
	Festival->FestivalId = TEXT("RevenueInspectionAutomation");
	Festival->FacilityCostTable = Costs;
	Festival->InitialGold = 10000;
	Progression->StartFestival(Festival);
	FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
	Spec.FacilityDefinitionId = TEXT("Booth_FoodStall_RevenueInspectionFixture");
	Spec.InstallCostKey = TEXT("Cost.Booth.Default");
	FFFZSGridPlacement Placement;
	Placement.OriginCell = FIntPoint(1000, 1000);
	const FFFZSCommand Command = UFFZSGridLibrary::MakeInstallCommand(
		Spec, FFProjectZoneTest::ZoneId(), Placement, Zones->GetWorldRevision());
	const FFFZSCommandResult Result = Zones->ExecuteCommand(Command);
	if (!TestTrue(*FString::Printf(TEXT("검사 시설 설치: %s"), *Result.Message.ToString()), Result.bSucceeded)) return false;
	AFFZSFacilityActor* Facility = Zones->FindFacility(Command.FacilityInstanceId);
	if (!TestNotNull(TEXT("실제 등록 시설"), Facility)) return false;
	const int64 Revision = Zones->GetWorldRevision();
	const FTransform Transform = Facility->GetActorTransform();

	Stage->CurrentStage = EFF_FestivalStage::Live;
	auto* Flow = Game->GetSubsystem<UFF_UIFlowSubsystem>();
	UClass* HudClass = LoadClass<UFF_LiveHudScreen>(nullptr,
		TEXT("/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen.WBP_FF_LiveHudScreen_C"));
	if (!TestNotNull(TEXT("HUD 소유자"), Flow) || !TestNotNull(TEXT("실제 Live HUD WBP"), HudClass)) return false;
	UFF_LiveHudScreen* Hud = CreateWidget<UFF_LiveHudScreen>(Controller, HudClass);
	if (!TestNotNull(TEXT("실제 Live HUD"), Hud)) return false;
	// 테스트 friend는 HUD 소유자 연결만 담당한다. 전경 판정과 클릭 처리는 실제 구현이다.
	Flow->StageHudWidget = Hud;
	Hud->AddToViewport();
	ON_SCOPE_EXIT
	{
		Hud->CloseCurrentPanel();
		Hud->RemoveFromParent();
		Flow->StageHudWidget.Reset();
	};

	// 매출 자체는 이 검사의 대상이 아니다. 실제 등록 GUID를 가진 표시 행만 공급한다.
	TStrongObjectPtr<UFF_FacilityRevenueRankItem> Item(NewObject<UFF_FacilityRevenueRankItem>());
	Item->FacilityId = Facility->GetFacilityInstanceId();
	Item->Name = FText::FromString(TEXT("검사 시설"));
	Item->Rank = 1;
	TArray<UFF_FacilityRevenueRankItem*> Rows{Item.Get()};
	UFF_FacilityRevenueRankingPanel* Panel = nullptr;
	UButton* Inspect = nullptr;
	UTextBlock* Footnote = nullptr;
	const auto OpenRanking = [&]() -> bool
	{
		Hud->CloseCurrentPanel();
		if (Stage->CurrentStage == EFF_FestivalStage::Live) Hud->ToggleTab(EFF_LiveTab::RevenueRanking);
		else Ranking->Open();
		if (!TestTrue(TEXT("실제 순위창 열림"), Ranking->IsOpen())) return false;
		TArray<UUserWidget*> Widgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UFF_FacilityRevenueRankingPanel::StaticClass(), false);
		for (UUserWidget* Widget : Widgets)
			if (Widget->IsInViewport()) Panel = Cast<UFF_FacilityRevenueRankingPanel>(Widget);
		if (!TestNotNull(TEXT("저장된 순위 WBP"), Panel)) return false;
		Panel->Present(Rows, 0, true, Item->FacilityId);
		if (auto* Dismiss = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_DismissIntroduction"))))
			if (Panel->IsIntroductionVisible()) Dismiss->OnClicked.Broadcast();
		Inspect = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_Inspect")));
		Footnote = Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("Text_Footnote")));
		return TestNotNull(TEXT("실제 정보 보기 버튼"), Inspect) && TestNotNull(TEXT("기존 하단 안내문"), Footnote);
	};
	const auto CheckVisibleInfo = [&]()
	{
		TArray<UUserWidget*> Widgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UFF_FacilityInfoPanel::StaticClass(), false);
		bool bVisible = false;
		for (UUserWidget* Widget : Widgets)
			bVisible |= Widget->IsInViewport() && Widget->IsVisible();
		TestTrue(TEXT("선택 성공 뒤 시설 정보가 실제 표시됨"), bVisible);
		TestFalse(TEXT("정보 보기 성공 뒤 순위창 닫힘"), Ranking->IsOpen());
		TestFalse(TEXT("순위 표시 억제 해제"), Info->IsRevenueRankingPresentationOpen());
		TestEqual(TEXT("선택된 시설 GUID"), Info->GetSelectedFacilityId(), Item->FacilityId);
	};

	Stage->CurrentStage = EFF_FestivalStage::Live;
	TestEqual(TEXT("이전 선택 없는 Live 문맥"), Info->GetSelectionContext(), EFF_FacilityInfoContext::None);
	TestEqual(TEXT("Live의 건축 모드는 닫힘"), Build->GetBuildModeState(), EFF_BuildModeState::Closed);
	if (!OpenRanking()) return false;
	TestTrue(TEXT("Live 하단 매출 탭은 시설 정보의 전경창으로 판정됨"), FFUIForegroundRules::HasForegroundWindow(Info));
	const FText OriginalFootnote = Footnote->GetText();
	Inspect->OnClicked.Broadcast();
	CheckVisibleInfo();
	TestEqual(TEXT("None 이력에서 Live 선택"), Info->GetSelectionContext(), EFF_FacilityInfoContext::Live);

	// 이전 Build 선택이 남아 있어도 현재 Live 단계를 따라야 한다.
	if (!TestTrue(TEXT("이전 Build 선택 준비"), Info->SelectFacility(Controller, Facility, EFF_FacilityInfoContext::Build))) return false;
	if (!OpenRanking()) return false;
	Inspect->OnClicked.Broadcast();
	CheckVisibleInfo();
	TestEqual(TEXT("이전 Build 이력에서 Live 선택"), Info->GetSelectionContext(), EFF_FacilityInfoContext::Live);

	// 준비 단계에서 건축 세션이 닫혀 있으면 조용히 사라지지 않고 재시도할 수 있다.
	Stage->CurrentStage = EFF_FestivalStage::Setup;
	if (!OpenRanking()) return false;
	Inspect->OnClicked.Broadcast();
	TestTrue(TEXT("실패하면 순위창 유지"), Ranking->IsOpen());
	TestEqual(TEXT("실패하면 선택 GUID 유지"), Ranking->GetSelectedFacilityId(), Item->FacilityId);
	TestTrue(TEXT("실패 사유를 기존 안내문에 표시"), !Footnote->GetText().IsEmpty() && !Footnote->GetText().EqualTo(OriginalFootnote));
	TestTrue(TEXT("실패 중에는 기존 정보창 표시 억제 유지"), Info->IsRevenueRankingPresentationOpen());
	const FText FailureFootnote = Footnote->GetText();
	Panel->Present(Rows, 0, true, Item->FacilityId);
	TestTrue(TEXT("같은 시설의 매출 갱신이 실패 안내를 지우지 않음"), Footnote->GetText().EqualTo(FailureFootnote));
	Panel->Present({}, 0, true, FGuid());
	TestTrue(TEXT("시설 선택이 없어지면 원래 안내 복원"), Footnote->GetText().EqualTo(OriginalFootnote));
	Panel->Present(Rows, 0, true, Item->FacilityId);

	// 현재 준비 단계를 기준으로 기존 건축 요청을 사용하며, 이전 Live 이력은 무시한다.
	Build->EnterBuildMode();
	if (!TestEqual(TEXT("준비 건축 세션 시작"), Build->GetBuildModeState(), EFF_BuildModeState::BuildMode)) return false;
	Stage->CurrentStage = EFF_FestivalStage::Live;
	if (!TestTrue(TEXT("이전 Live 선택 준비"), Info->SelectFacility(Controller, Facility, EFF_FacilityInfoContext::Live))) return false;
	Stage->CurrentStage = EFF_FestivalStage::Setup;
	if (!Ranking->IsOpen() && !OpenRanking()) return false;
	Inspect->OnClicked.Broadcast();
	CheckVisibleInfo();
	TestEqual(TEXT("준비 단계에서는 Build 경로 유지"), Info->GetSelectionContext(), EFF_FacilityInfoContext::Build);
	TestEqual(TEXT("정보 보기는 월드 리비전을 변경하지 않음"), Zones->GetWorldRevision(), Revision);
	TestTrue(TEXT("정보 보기는 시설 위치를 변경하지 않음"), Facility->GetActorTransform().Equals(Transform));
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
