// 역할: 축제 맵 로딩 훅에서 실제 시설 이미지를 준비하고 축제 수명 동안 유지하는지 검사한다.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Building/FF_BuildCatalogSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"
#include "UI/Core/FF_LoadingScreenWidget.h"
#include "UI/Core/FF_UIFlowSettings.h"
#include "UI/Core/FF_UIFlowSubsystem.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FestivalThumbnailLoadingTest,
	"FestaFiesta.UI.Loading.FestivalThumbnailLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FestivalThumbnailLoadingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// GameInstance 초기화의 저장 목록 정리가 개인 세이브를 건드리지 않도록 빈 격리 경로만 허용한다.
	FString IsolatedUserDirectory;
	const FString OrdinarySaved = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved")));
	if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), IsolatedUserDirectory)
		|| IsolatedUserDirectory.IsEmpty() || FPaths::IsRelative(IsolatedUserDirectory)
		|| FPaths::IsSamePath(FPaths::ProjectSavedDir(), OrdinarySaved))
	{
		AddError(TEXT("Festival thumbnail test requires -UserDir=<isolated absolute directory>."));
		return false;
	}
	TArray<FString> ExistingSaves;
	IFileManager::Get().FindFilesRecursive(ExistingSaves,
		*FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames")), TEXT("*.sav"), true, false);
	if (!ExistingSaves.IsEmpty())
	{
		AddError(TEXT("Festival thumbnail test requires an isolated directory without existing saves."));
		return false;
	}

	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game))
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Fixture.GetTestWorld();
	UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
	if (!TestNotNull(TEXT("Isolated game instance"), Game)) return false;
	// UE 5.7 테스트 월드가 만든 엔진 문맥을 GI에도 연결한다. BeginPlay나 엔진 틱은 실행하지 않는다.
	Game->OnWorldChanged(nullptr, World);
	FWorldContext* Context = Game->GetWorldContext();
	if (!TestNotNull(TEXT("Game instance resolves its own world context"), Context)) return false;
	const FString PreviousTravelURL = Context->TravelURL;
	const auto PreviousTravelType = Context->TravelType;
	ON_SCOPE_EXIT
	{
		// ReturnToAtlas의 실제 OpenLevel은 목적지만 예약한다. 테스트 종료 전 예약을 되돌린다.
		Context->TravelURL = PreviousTravelURL;
		Context->TravelType = PreviousTravelType;
	};

	auto* Flow = Game->GetSubsystem<UFF_UIFlowSubsystem>();
	auto* Catalog = Game->GetSubsystem<UFF_BuildCatalogSubsystem>();
	if (!TestNotNull(TEXT("Production UI flow"), Flow)
		|| !TestNotNull(TEXT("Production build catalog"), Catalog)) return false;
	ON_SCOPE_EXIT { Flow->ReleaseFestivalThumbnailPreload(); };

	// 다른 GI의 문맥/실패 이벤트를 만들되 두 번째 GI의 서브시스템은 초기화하지 않는다.
	TStrongObjectPtr<UGameInstance> ForeignGame(NewObject<UGameInstance>());
	TStrongObjectPtr<UWorld> ForeignWorld(NewObject<UWorld>());
	ForeignWorld->SetGameInstance(ForeignGame.Get());
	FWorldContext ForeignContext;
	ForeignContext.OwningGameInstance = ForeignGame.Get();
	ForeignContext.SetCurrentWorld(ForeignWorld.Get());

	const FString Destination(TEXT("/Game/FestaFiesta/Automation/ThumbnailDestination"));
	Flow->Mode = EFF_UIFlowMode::Festival;
	Flow->PendingLoadingMapPackage = Destination;
	Flow->PendingLoadingScreenClass = nullptr;
	Flow->bPendingFestivalStart = false;
	Flow->HandlePreLoadMapWithContext(*Context, Destination);
	TestEqual(TEXT("No festival request leaves thumbnails untouched"), Catalog->GetThumbnailLoadState(),
		EFF_BuildCatalogThumbnailLoadState::NotRequested);

	Flow->bPendingFestivalStart = true;
	Flow->HandlePreLoadMapWithContext(ForeignContext, Destination);
	TestEqual(TEXT("Another game instance cannot request these thumbnails"), Catalog->GetThumbnailLoadState(),
		EFF_BuildCatalogThumbnailLoadState::NotRequested);
	TestEqual(TEXT("Foreign travel preserves the pending destination"), Flow->PendingLoadingMapPackage, Destination);
	Flow->HandlePreLoadMapWithContext(*Context, TEXT("/Engine/Maps/Entry"));
	TestEqual(TEXT("An unrelated destination cannot request thumbnails"), Catalog->GetThumbnailLoadState(),
		EFF_BuildCatalogThumbnailLoadState::NotRequested);
	TestEqual(TEXT("Unrelated travel preserves the pending destination"), Flow->PendingLoadingMapPackage, Destination);

	// 로딩 WBP가 없는 경로를 사용해 NullRHI에서도 실제 에셋 로드가 훅 반환 전에 끝나야 한다.
	Flow->HandlePreLoadMapWithContext(*Context, TEXT("/Game/FestaFiesta/Automation/UEDPIE_7_ThumbnailDestination"));
	TestTrue(TEXT("Matching travel consumes its destination once"), Flow->PendingLoadingMapPackage.IsEmpty());
	TestEqual(TEXT("Map-load fallback completes the real definition catalog"), Catalog->GetLoadState(),
		EFF_BuildCatalogLoadState::Ready);
	TestEqual(TEXT("Map-load fallback prepares thumbnails before the first build tab"), Catalog->GetThumbnailLoadState(),
		EFF_BuildCatalogThumbnailLoadState::Ready);
	TestFalse(TEXT("Synchronous fallback leaves no pending warmup"), Flow->bPendingFestivalThumbnailWarmup);
	TestTrue(TEXT("The project catalog contains real facility entries"), !Catalog->GetEntriesView().IsEmpty());
	int32 ConfiguredThumbnailCount = 0;
	for (const FFF_BuildCatalogEntry& Entry : Catalog->GetEntriesView())
	{
		if (Entry.Thumbnail.IsNull()) continue;
		++ConfiguredThumbnailCount;
		TestTrue(*FString::Printf(TEXT("Configured thumbnail is ready without a card-side load: %s"),
			*Entry.FacilityDefinitionId.ToString()), Entry.Thumbnail.IsValid());
		if (const UTexture2D* Thumbnail = Entry.Thumbnail.Get())
		{
			TestFalse(TEXT("Prepared thumbnail is a real content asset"), Thumbnail->HasAnyFlags(RF_Transient));
		}
	}
	TestTrue(TEXT("Real project thumbnails were exercised"), ConfiguredThumbnailCount > 0);

	Flow->HandleLoadingScreenFinished();
	TestEqual(TEXT("Closing the loading screen preserves the festival thumbnail lease"), Catalog->GetThumbnailLoadState(),
		EFF_BuildCatalogThumbnailLoadState::Ready);
	// 배치 탭의 기존 참조를 추가/반환해도 축제 참조가 유지되고, 흐름의 중복 획득은 누수되지 않아야 한다.
	Catalog->AcquireThumbnailPreload();
	Flow->AcquireFestivalThumbnailPreload();
	Flow->AcquireFestivalThumbnailPreload();
	Catalog->ReleaseThumbnailPreload();
	TestEqual(TEXT("Closing a build tab keeps festival thumbnails ready"), Catalog->GetThumbnailLoadState(),
		EFF_BuildCatalogThumbnailLoadState::Ready);
	Flow->ReleaseFestivalThumbnailPreload();
	TestEqual(TEXT("Festival release balances duplicate acquisition"), Catalog->GetThumbnailLoadState(),
		EFF_BuildCatalogThumbnailLoadState::NotRequested);
	Flow->ReleaseFestivalThumbnailPreload();
	TestEqual(TEXT("A repeated flow release is harmless"), Catalog->GetThumbnailLoadState(),
		EFF_BuildCatalogThumbnailLoadState::NotRequested);

	Flow->PendingLoadingMapPackage = Destination;
	Flow->HandlePreLoadMapWithContext(*Context, Destination);
	TestEqual(TEXT("A later festival reacquires and prepares thumbnails"), Catalog->GetThumbnailLoadState(),
		EFF_BuildCatalogThumbnailLoadState::Ready);
	Flow->HandleFestivalTravelFailure(ForeignWorld.Get(), ETravelFailure::LoadMapFailure, TEXT("Foreign test travel"));
	TestEqual(TEXT("Another game's travel failure preserves this festival lease"), Catalog->GetThumbnailLoadState(),
		EFF_BuildCatalogThumbnailLoadState::Ready);
	Flow->HandleFestivalTravelFailure(World, ETravelFailure::LoadMapFailure, TEXT("Own test travel"));
	TestEqual(TEXT("Own travel failure releases festival thumbnails"), Catalog->GetThumbnailLoadState(),
		EFF_BuildCatalogThumbnailLoadState::NotRequested);

	// 실패 뒤 재시작도 가능하며 실제 로비 복귀 명령은 축제 참조를 반환해야 한다.
	Flow->PendingLoadingMapPackage = Destination;
	Flow->HandlePreLoadMapWithContext(*Context, Destination);
	TestEqual(TEXT("Restart after failure prepares thumbnails again"), Catalog->GetThumbnailLoadState(),
		EFF_BuildCatalogThumbnailLoadState::Ready);
	auto* Settings = GetMutableDefault<UFF_UIFlowSettings>();
	TGuardValue<TSoftObjectPtr<UWorld>> FrontendSetting(Settings->FrontendLevel,
		TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Engine/Maps/Entry.Entry"))));
	Flow->SetActiveCountry(TEXT("ThumbnailTestCountry"));
	Flow->ReturnToAtlas();
	TestEqual(TEXT("Returning to Atlas releases the festival lease"), Catalog->GetThumbnailLoadState(),
		EFF_BuildCatalogThumbnailLoadState::NotRequested);
	TestEqual(TEXT("Returning to Atlas restores frontend mode"), Flow->GetMode(), EFF_UIFlowMode::Frontend);
	TestTrue(TEXT("Return queues the configured frontend map"), Context->TravelURL.Contains(TEXT("/Engine/Maps/Entry")));
	TestEqual(TEXT("No actual map travel occurred"), Game->GetWorld(), World);
	return true;
}

#endif
