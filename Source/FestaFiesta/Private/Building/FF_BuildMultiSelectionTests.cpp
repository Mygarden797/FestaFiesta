// FF_BuildMultiSelectionTests.cpp
// 역할: 클릭·사각형 드래그·모드 종료가 시설 선택을 올바르게 갱신하는지 검사한다.
// 선택만으로 실제 배치가 바뀌지 않고, 폐기한 F1~F7 단축키가 도구를 실행하지 않는지도 확인한다.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Building/FF_BuildModeSubsystem.h"
#include "Building/FF_BuildMultiSelectionModel.h"
#include "Camera/FF_FestivalPlayerController.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputKeyEventArgs.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Kismet/GameplayStatics.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "UnrealClient.h"
#include "Widgets/SOverlay.h"

namespace
{
class FMultiSelectionViewport final : public FViewport
{
public:
	explicit FMultiSelectionViewport(FViewportClient* Client) : FViewport(Client) {}
	virtual FIntPoint GetSizeXY() const override { return FIntPoint(800, 600); }
	virtual void* GetWindow() override { return nullptr; }
	virtual void MoveWindow(int32, int32, int32, int32) override {}
	virtual void Destroy() override {}
	virtual bool SetUserFocus(bool) override { return true; }
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
};

struct FScopedMultiSelectionSettings
{
	UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
	bool bGround = Settings->bEnableGroundChecks;
	bool bCollision = Settings->bEnableWorldCollisionChecks;
	bool bNavigation = Settings->bEnableNavigationProjectionChecks;
	bool bPolicies = Settings->bRequireProductionPolicyDomains;
	EFFZSChangePhase Phase = Settings->InitialPhase;
	FScopedMultiSelectionSettings()
	{
		Settings->bEnableGroundChecks = false;
		Settings->bEnableWorldCollisionChecks = false;
		Settings->bEnableNavigationProjectionChecks = false;
		Settings->bRequireProductionPolicyDomains = false;
		Settings->InitialPhase = EFFZSChangePhase::InitialSetup;
	}
	~FScopedMultiSelectionSettings()
	{
		Settings->bEnableGroundChecks = bGround;
		Settings->bEnableWorldCollisionChecks = bCollision;
		Settings->bEnableNavigationProjectionChecks = bNavigation;
		Settings->bRequireProductionPolicyDomains = bPolicies;
		Settings->InitialPhase = Phase;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildMultiSelectionGeometryTest,
	"FestaFiesta.Building.BuildMode.MultiSelectionGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildMultiSelectionGeometryTest::RunTest(const FString& Parameters)
{
	using namespace FFBuildMultiSelectionModel;
	TestFalse(TEXT("A short click stays a click"), IsMarqueeDrag(FVector2D(10, 10), FVector2D(16, 14)));
	TestTrue(TEXT("An eight-pixel gesture becomes a marquee"), IsMarqueeDrag(FVector2D(10, 10), FVector2D(18, 10)));
	const FBox2D Reverse = MakeScreenRect(FVector2D(300, 250), FVector2D(100, 50));
	TestEqual(TEXT("Reverse drag normalizes its minimum"), Reverse.Min, FVector2D(100, 50));
	TestEqual(TEXT("Reverse drag normalizes its maximum"), Reverse.Max, FVector2D(300, 250));
	TestTrue(TEXT("A partly enclosed footprint is selected"), Intersects(Reverse,
		FVector2D(280, 180), FVector2D(340, 270)));
	TestTrue(TEXT("A touching footprint edge is selected"), Intersects(Reverse,
		FVector2D(300, 100), FVector2D(360, 150)));
	TestFalse(TEXT("A separated footprint is not selected"), Intersects(Reverse,
		FVector2D(301, 100), FVector2D(360, 150)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildMultiSelectionSessionTest,
	"FestaFiesta.Building.BuildMode.MultiSelectionSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildMultiSelectionSessionTest::RunTest(const FString& Parameters)
{
	FScopedMultiSelectionSettings Settings;
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game))
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Fixture.GetTestWorld();
	UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
	UFF_BuildModeSubsystem* Build = World ? World->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
	UFFZSWorldSubsystem* Zones = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	if (!TestNotNull(TEXT("World"), World) || !TestNotNull(TEXT("Game"), Game) ||
		!TestNotNull(TEXT("Build subsystem"), Build) || !TestNotNull(TEXT("Zone registry"), Zones)) return false;

	FWorldContext* Context = GEngine->GetWorldContextFromWorld(World);
	if (!TestNotNull(TEXT("World context"), Context)) return false;
	UGameViewportClient* Viewport = NewObject<UGameViewportClient>(GEngine);
	Context->GameViewport = Viewport;
	Viewport->Init(*Context, Game, false);
	Viewport->SetViewportOverlayWidget(nullptr, SNew(SOverlay));
	FMultiSelectionViewport InputViewport(Viewport);
	Viewport->Viewport = &InputViewport;
	ON_SCOPE_EXIT { Viewport->Viewport = nullptr; };
	FString PlayerError;
	ULocalPlayer* Player = Game->CreateLocalPlayer(0, PlayerError, false);
	APlayerController* Controller = World->SpawnActor<AFF_FestivalPlayerController>();
	if (!TestNotNull(*FString::Printf(TEXT("Player: %s"), *PlayerError), Player) ||
		!TestNotNull(TEXT("Controller"), Controller)) return false;
	Controller->SetPlayer(Player);
	Player->Origin = FVector2D::ZeroVector;
	Player->Size = FVector2D(1.0f, 1.0f);
	World->AddController(Controller);
	UFF_FestivalProgressionSubsystem* Progression = Game->GetSubsystem<UFF_FestivalProgressionSubsystem>();
	if (!TestNotNull(TEXT("Festival progression"), Progression)) return false;
	UFF_FestivalDefinition* Festival = NewObject<UFF_FestivalDefinition>(Game);
	UDataTable* Costs = NewObject<UDataTable>(Game);
	Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
	FFF_FacilityCostRow CostRow;
	CostRow.InstallCost = 100;
	Costs->AddRow(TEXT("Cost.Booth.Default"), CostRow);
	Festival->FestivalId = TEXT("BuildMultiSelectionAutomation");
	Festival->FacilityCostTable = Costs;
	Festival->InitialGold = 10000;
	Progression->StartFestival(Festival);

	const FName ZoneId(TEXT("GZ_DadaPocha"));
	AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
	if (!TestNotNull(TEXT("Build zone"), Zone) ||
		!TestTrue(TEXT("Zone preset"), Zone->ApplyAcceptedZonePreset(ZoneId)) ||
		!TestTrue(TEXT("Zone registered"), Zones->RegisterZone(Zone))) return false;
	const auto Install = [&](int32 X) -> AFFZSFacilityActor*
	{
		FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
		Spec.FacilityDefinitionId = TEXT("Booth_FoodStall_Fixture");
		Spec.InstallCostKey = NAME_None;
		FFFZSGridPlacement Placement;
		Placement.OriginCell = FIntPoint(X, 1000);
		const FFFZSCommand Command = UFFZSGridLibrary::MakeInstallCommand(
			Spec, ZoneId, Placement, Zones->GetWorldRevision());
		const FFFZSCommandResult Result = Zones->ExecuteCommand(Command);
		TestTrue(TEXT("Registered facility installed"), Result.bSucceeded);
		return Result.bSucceeded ? Zones->FindFacility(Command.FacilityInstanceId) : nullptr;
	};
	AFFZSFacilityActor* First = Install(1000);
	AFFZSFacilityActor* Second = Install(4000);
	if (!TestNotNull(TEXT("First facility"), First) || !TestNotNull(TEXT("Second facility"), Second)) return false;
	const int64 RevisionBefore = Zones->GetWorldRevision();

	// Seed the ordinary build state. Former shortcuts must stay inert even at
	// the controller boundary; only the explicit toolbar action starts selection.
	Build->State = EFF_BuildModeState::BuildMode;
	Build->bBuildSessionActive = true;
	const bool bGridBeforeKeys = Build->IsPlacementGridVisible();
	const int32 SnapBeforeKeys = Build->GetPreferredPositionSnapCm();
	for(const FKey& Key : {EKeys::F1,EKeys::F2,EKeys::F3,EKeys::F4,EKeys::F5,EKeys::F6,EKeys::F7})
	{
		TestFalse(TEXT("Retired function-key dispatcher never activates a tool"), Build->HandleBuildToolbarKey(Key));
		for(const EInputEvent Event : {IE_Pressed, IE_Repeat, IE_Released})
			TestTrue(TEXT("Retired key is consumed before engine debug/input bindings"), Controller->InputKey(
				FInputKeyEventArgs(&InputViewport, FInputDeviceId::CreateFromInternalId(0), Key, Event, 1.f, false, 0u)));
	}
	TestFalse(TEXT("Function keys cannot start multi-selection"), Build->IsMultiSelectionActive());
	TestFalse(TEXT("Function keys cannot open settings"), Build->IsBuildSettingsOpen());
	TestEqual(TEXT("Function keys preserve grid visibility"), Build->IsPlacementGridVisible(), bGridBeforeKeys);
	TestEqual(TEXT("Function keys preserve position snap"), Build->GetPreferredPositionSnapCm(), SnapBeforeKeys);
	Build->ToggleMultiSelection();
	TestTrue(TEXT("Toolbar action enables world multi-selection"), Build->IsMultiSelectionActive());
	Build->ToggleMultiSelectedFacility(First);
	TestEqual(TEXT("Click selects a registered facility"), Build->GetMultiSelectionCount(), 1);
	Build->ToggleMultiSelectedFacility(First);
	TestEqual(TEXT("Second click deselects it"), Build->GetMultiSelectionCount(), 0);

	Controller->SetInitialLocationAndRotation(FVector(2700.0f, 1000.0f, 12000.0f),
		FRotator(-90.0f, 0.0f, 0.0f));
	FVector2D FirstPoint, SecondPoint;
	if (TestTrue(TEXT("First facility projects into the marquee"),
		Controller->ProjectWorldLocationToScreen(First->GetActorLocation(), FirstPoint, true)) &&
		TestTrue(TEXT("Second facility projects into the marquee"),
		Controller->ProjectWorldLocationToScreen(Second->GetActorLocation(), SecondPoint, true)))
	{
		const FVector2D Min(FMath::Min(FirstPoint.X, SecondPoint.X) - 50.0f,
			FMath::Min(FirstPoint.Y, SecondPoint.Y) - 50.0f);
		const FVector2D Max(FMath::Max(FirstPoint.X, SecondPoint.X) + 50.0f,
			FMath::Max(FirstPoint.Y, SecondPoint.Y) + 50.0f);
		const FBox2D Rect = FFBuildMultiSelectionModel::MakeScreenRect(Min, Max);
		Build->SelectFacilitiesInScreenRect(*Controller, Rect);
		TestEqual(TEXT("Marquee selects both real installed facilities"), Build->GetMultiSelectionCount(), 2);
	}
	Build->ClearMultiSelection();
	TestTrue(TEXT("Clear keeps F1 mode active"), Build->IsMultiSelectionActive());
	TestEqual(TEXT("Clear removes every selection"), Build->GetMultiSelectionCount(), 0);
	Build->ToggleMultiSelectedFacility(First);
	Build->ExitBuildMode();
	TestFalse(TEXT("Build exit disables F1 mode"), Build->IsMultiSelectionActive());
	TestEqual(TEXT("Build exit removes selected facility references"), Build->GetMultiSelectionCount(), 0);
	TestEqual(TEXT("Build exit hides the facility selection indicator"),
		First->GetSelectionIndicatorState(), EFFZSFacilitySelectionIndicatorState::Hidden);
	TestEqual(TEXT("All selection operations leave world authority unchanged"), Zones->GetWorldRevision(), RevisionBefore);

	// Group preview changes no live facility; cancelling must never produce a history entry.
	Build->State = EFF_BuildModeState::BuildMode;
	Build->bBuildSessionActive = true;
	Build->ToggleMultiSelection();
	Build->AddMultiSelectedFacility(First);
	Build->AddMultiSelectedFacility(Second);
	const FGuid FirstId = First->GetFacilityInstanceId();
	const FGuid SecondId = Second->GetFacilityInstanceId();
	const FVector FirstOriginal = First->GetActorLocation();
	const FVector SecondOriginal = Second->GetActorLocation();
	TestTrue(TEXT("Group movement starts with a preview"), Build->BeginGroupMove());
	TestEqual(TEXT("Preview holds both selected IDs"), Build->GroupSourceIds.Num(), 2);
	Build->RefreshGroupPlacement(Build->GroupTargetCenter + FVector(0.f, 3000.f, 0.f));
	Build->RotateGroupPlacement(90.f);
	if (TestEqual(TEXT("Rotation retains both candidate placements"), Build->GroupCommands.Num(), 2))
	{
		const FFFZSCommand& A = Build->GroupCommands[0];
		const FFFZSCommand& B = Build->GroupCommands[1];
		const FVector ACenter = Zones->BuildPlacementTransform(*Zone, A.FacilitySpec, A.Placement).GetLocation();
		const FVector BCenter = Zones->BuildPlacementTransform(*Zone, B.FacilitySpec, B.Placement).GetLocation();
		const FVector ExpectedOffset = FQuat(FVector::UpVector, HALF_PI).RotateVector(SecondOriginal - FirstOriginal);
		TestTrue(TEXT("Group rotation rotates the offset around one pivot without changing spacing"),
			(BCenter - ACenter).Equals(ExpectedOffset, 1.1f));
	}
	TestEqual(TEXT("Rotation preview leaves the first actor untouched"), First->GetActorLocation(), FirstOriginal);
	TestEqual(TEXT("Rotation preview leaves authority revision untouched"), Zones->GetWorldRevision(), RevisionBefore);
	Build->CancelGroupPlacement();
	TestFalse(TEXT("Cancel releases group ghosts"), Build->IsGroupPlacementActive());
	TestTrue(TEXT("Cancelled group destroys every preview actor"), Build->GroupPreviews.IsEmpty());
	TestEqual(TEXT("Cancel keeps selected facilities"), Build->GetMultiSelectionCount(), 2);

	TestTrue(TEXT("Move begins again"), Build->BeginGroupMove());
	const FVector Target = Build->GroupTargetCenter + FVector(500.f, 500.f, 0.f);
	TestTrue(TEXT("Batch validates nearby group placement"), Build->RefreshGroupPlacement(Target));
	TestTrue(TEXT("Group commits as one action"), Build->CommitGroupPlacement());
	First = Zones->FindFacility(FirstId);
	Second = Zones->FindFacility(SecondId);
	if (!TestNotNull(TEXT("First survives group move"), First) || !TestNotNull(TEXT("Second survives group move"), Second)) return false;
	TestTrue(TEXT("Group movement preserves relative centers"),
		(Second->GetActorLocation() - First->GetActorLocation()).Equals(SecondOriginal - FirstOriginal, 1.f));
	FText HistoryError;
	TestTrue(TEXT("One undo restores the whole move"), Zones->UndoConstruction(HistoryError));
	First = Zones->FindFacility(FirstId);
	Second = Zones->FindFacility(SecondId);
	if (!TestNotNull(TEXT("First restored"), First) || !TestNotNull(TEXT("Second restored"), Second)) return false;
	TestTrue(TEXT("First returns to original position"), First->GetActorLocation().Equals(FirstOriginal, 1.f));
	TestTrue(TEXT("Second returns to original position"), Second->GetActorLocation().Equals(SecondOriginal, 1.f));

	// Presets serialize named, reusable relative geometry without live instance IDs.
	Build->ClearMultiSelection();
	Build->AddMultiSelectedFacility(First);
	Build->AddMultiSelectedFacility(Second);
	TArray<FFF_BuildLayoutEntry> Entries;
	FVector CapturedCenter;
	TestTrue(TEXT("Selected layout captures"), Build->CaptureSelectedGroup(Entries, CapturedCenter));
	UFF_BuildLayoutPresetSave* Saved = NewObject<UFF_BuildLayoutPresetSave>();
	FFF_BuildLayoutPreset& Preset = Saved->Presets.AddDefaulted_GetRef();
	Preset.Name = TEXT("먹거리 거리");
	Preset.Entries = Entries;
	FFF_BuildLayoutPreset& LegacyPreset = Saved->Presets.AddDefaulted_GetRef();
	LegacyPreset.Name = TEXT("기존 조명 배치");
	FFF_BuildLayoutEntry& LegacyEntry = LegacyPreset.Entries.AddDefaulted_GetRef();
	LegacyEntry.Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
	LegacyEntry.Spec.Category = EFFZSFacilityCategory::Lighting;
	LegacyEntry.Spec.ClearanceCells = 150;
	LegacyEntry.Spec.QueueLayout = EFFZSQueueLayout::Left;
	TArray<uint8> Bytes;
	TestTrue(TEXT("Layout serializes to save data"), UGameplayStatics::SaveGameToMemory(Saved, Bytes));
	UFF_BuildLayoutPresetSave* Loaded = Cast<UFF_BuildLayoutPresetSave>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!TestNotNull(TEXT("Layout reloads"), Loaded)) return false;
	TestEqual(TEXT("Named layout survives restart serialization"), Loaded->Presets[0].Name, FString(TEXT("먹거리 거리")));
	TestEqual(TEXT("Saved group keeps both entries"), Loaded->Presets[0].Entries.Num(), 2);
	TestEqual(TEXT("Saved group preserves spacing"), Loaded->Presets[0].Entries[1].RelativeCenter - Loaded->Presets[0].Entries[0].RelativeCenter,
		SecondOriginal - FirstOriginal);
	Build->LayoutPresets = Loaded;
	TestTrue(TEXT("Saved layout starts an install preview"), Build->BeginGroupPreset(TEXT("먹거리 거리")));
	TestTrue(TEXT("Preset uses fresh facility IDs"), !Build->GroupSourceIds.Contains(FirstId) && !Build->GroupSourceIds.Contains(SecondId));
	Build->CancelGroupPlacement();
	TestTrue(TEXT("Legacy lighting preset starts a normalized placement copy"), Build->BeginGroupPreset(TEXT("기존 조명 배치")));
	if (TestEqual(TEXT("Legacy preset retains its single placement entry"), Build->GroupEntries.Num(), 1))
	{
		const FFFZSFacilitySpec& PlacementSpec = Build->GroupEntries[0].Spec;
		TestEqual(TEXT("Legacy lighting placement no longer reserves clearance"), PlacementSpec.ClearanceCells, 0);
		TestEqual(TEXT("Legacy lighting placement has no waiting capacity"), PlacementSpec.QueueRules.MaxWaitingAgents, 0);
		TestEqual(TEXT("Legacy lighting placement has no service capacity"), PlacementSpec.QueueRules.ServiceCapacity, 0);
		TestEqual(TEXT("Legacy lighting placement clears the saved queue layout"), PlacementSpec.QueueLayout, EFFZSQueueLayout::Authored);
		TestTrue(TEXT("Legacy lighting placement clears queue geometry and anchors"),
			PlacementSpec.QueueReservedRects.IsEmpty() && PlacementSpec.QueueReservedCells.IsEmpty() &&
			PlacementSpec.QueueSplinePointsCm.IsEmpty() && PlacementSpec.Anchors.IsEmpty());
	}
	const FFFZSFacilitySpec& SavedLegacySpec = Loaded->Presets[1].Entries[0].Spec;
	TestEqual(TEXT("Beginning placement preserves the original saved clearance"), SavedLegacySpec.ClearanceCells, 150);
	TestEqual(TEXT("Beginning placement preserves the original saved queue layout"), SavedLegacySpec.QueueLayout, EFFZSQueueLayout::Left);
	TestTrue(TEXT("Beginning placement preserves the original saved queue data"),
		SavedLegacySpec.QueueRules.MaxWaitingAgents > 0 && !SavedLegacySpec.QueueSplinePointsCm.IsEmpty() && !SavedLegacySpec.Anchors.IsEmpty());
	Build->CancelGroupPlacement();
	Build->ExitBuildMode();
	return true;
}

#endif
