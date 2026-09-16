#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/BoxComponent.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FFF_CrowdRoadSurfaceTest,
    "FestaFiesta.Crowd.Road.SupportUpdatesAndAvoidance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FFF_CrowdRoadSurfaceTest::GetTests(TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("Road"));
    OutTestCommands.Add(TEXT("Road"));
    OutBeautifiedNames.Add(TEXT("Tile"));
    OutTestCommands.Add(TEXT("Tile"));
}

bool FFF_CrowdRoadSurfaceTest::RunTest(const FString& Parameters)
{
    UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
    TGuardValue<bool> Ground(Settings->bEnableGroundChecks, false);
    TGuardValue<bool> Collision(Settings->bEnableWorldCollisionChecks, false);
    TGuardValue<bool> Navigation(Settings->bEnableNavigationProjectionChecks, false);
    TGuardValue<bool> Policies(Settings->bRequireProductionPolicyDomains, false);
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* System = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Road world subsystem"), System)) return false;
    AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
    Zone->ApplyAcceptedZonePreset(TEXT("GZ_DadaPocha"));
    Zone->bAllowAllFacilityCategories = true;
    if (!TestTrue(TEXT("Road zone registers"), System->RegisterZone(Zone))) return false;

    AActor* Terrain = World->SpawnActor<AActor>();
    UBoxComponent* Box = NewObject<UBoxComponent>(Terrain);
    Terrain->SetRootComponent(Box);
    Terrain->AddInstanceComponent(Box);
    Box->SetBoxExtent(FVector(6000, 6000, 10));
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionObjectType(ECC_WorldStatic);
    Box->SetCollisionResponseToAllChannels(ECR_Block);
    Box->SetCanEverAffectNavigation(false);
    Box->RegisterComponent();
    Terrain->SetActorLocation(FVector(5000, 5000, -10));

    UFF_CrowdSimulationSubsystem* Crowd = NewObject<UFF_CrowdSimulationSubsystem>(World);
    Crowd->HeightFieldSpacingUU = 500.0f;
    Crowd->ConfigureSimAreaWithBounds(FVector(2500, 2500, 0), FBox::BuildAABB(FVector(2500, 2500, 0), FVector(2000.0, 2000.0, 2000.0)));
    double Height = -1.0;
    TestTrue(TEXT("Terrain height exists before roads"), Crowd->TryGetGroundZ(FVector(1200, 1200, 0), Height));
    TestEqual(TEXT("Initial terrain top is zero"), Height, 0.0);

    FFFZSFacilitySpec Spec;
    Spec.FacilityDefinitionId = FName(*(Parameters + TEXT("_Surface_CrowdTest")));
    Spec.DisplayName = FText::FromString(TEXT("Crowd ground surface support test"));
    Spec.Category = Parameters == TEXT("Tile") ? EFFZSFacilityCategory::Tile : EFFZSFacilityCategory::Road;
    Spec.bAllowFacilityPlacementOnTop = false;
    Spec.FootprintSizeCells = FIntPoint(400, 400);
    Spec.ClearanceCells = 0;
    Spec.PlacementObstacleHeightCm = 8.0f;
    Spec.QueueRules.MaxWaitingAgents = 0;
    Spec.QueueRules.ServiceCapacity = 0;
    TArray<FText> SpecErrors;
    if (!TestTrue(TEXT("Road test definition satisfies production authoring rules"), Spec.IsValid(SpecErrors)))
    {
        for (const FText& Error : SpecErrors) AddError(Error.ToString());
        return false;
    }
    const auto ExecuteChecked = [this, System](const TCHAR* Context, const FFFZSCommand& Command)
    {
        const FFFZSCommandResult Result = System->ExecuteCommand(Command);
        if (!Result.bSucceeded)
        {
            AddError(FString::Printf(TEXT("%s: %s (%s)"), Context, *Result.Message.ToString(),
                *UEnum::GetValueAsString(Result.FailureReason)));
        }
        return TestTrue(Context, Result.bSucceeded);
    };
    FFFZSGridPlacement Placement;
    Placement.OriginCell = FIntPoint(1000, 1000);
    const FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommand(Spec, Zone->ZoneId, Placement);
    if (!ExecuteChecked(TEXT("Road installs with no loaded mesh"), Install)) return false;
    TestTrue(TEXT("Road supports NPC before visual loading"), Crowd->TryGetGroundZ(FVector(1200, 1200, 0), Height));
    TestEqual(TEXT("NPC feet reach the surface top regardless of upper-facility permission"), Height, 8.0);
    Crowd->EnsureRoadLanes();
    TestEqual(TEXT("Both ground categories participate in crowd lane generation"), Crowd->GetRoadLaneStats().TileCount, 1);
    Crowd->TryGetGroundZ(FVector(1410, 1200, 0), Height);
    TestEqual(TEXT("Same spatial bucket outside footprint stays terrain"), Height, 0.0);
    const FFFZSWorldSnapshot Original = System->CaptureWorldSnapshot();

    FFFZSCommand Move;
    Move.CommandId = FGuid::NewGuid();
    Move.TransactionId = FGuid::NewGuid();
    Move.Type = EFFZSCommandType::Move;
    Move.FacilityInstanceId = Install.FacilityInstanceId;
    Move.ZoneId = Zone->ZoneId;
    Move.Placement.OriginCell = FIntPoint(2000, 1000);
    Move.Placement.YawCentidegrees = 4500;
    if (!ExecuteChecked(TEXT("Road moves and rotates"), Move)) return false;
    Crowd->TryGetGroundZ(FVector(1200, 1200, 0), Height);
    TestEqual(TEXT("Move invalidates old support height"), Height, 0.0);
    const FVector MovedCenter = System->FindFacility(Install.FacilityInstanceId)->GetActorLocation();
    Crowd->TryGetGroundZ(MovedCenter, Height);
    TestEqual(TEXT("Rotated road center retains support"), Height, 8.0);
    Crowd->TryGetGroundZ(MovedCenter + FVector(250, 250, 0), Height);
    TestEqual(TEXT("Rotated road broad-phase corner is not a support surface"), Height, 0.0);

    FFFZSWorldSnapshot Restored = Original;
    Restored.WorldRevision = System->GetWorldRevision();
    TArray<FText> Errors;
    if (!TestTrue(TEXT("Restore may reuse current revision"), System->RestoreWorldSnapshot(Restored, Errors)))
    {
        for (const FText& Error : Errors) AddError(Error.ToString());
        return false;
    }
    Crowd->TryGetGroundZ(FVector(1200, 1200, 0), Height);
    TestEqual(TEXT("Snapshot event refreshes support at the same revision"), Height, 8.0);
    Crowd->TryGetGroundZ(MovedCenter, Height);
    TestEqual(TEXT("Restored road removes previous support location"), Height, 0.0);
    Crowd->ConfigureSimAreaWithBounds(FVector(2500, 2500, 0), FBox::BuildAABB(FVector(2500, 2500, 0), FVector(2000.0, 2000.0, 2000.0)));
    TestEqual(TEXT("Road creates no crowd avoidance wall"), Crowd->GetFacilityWallCount(), 0);

    FFFZSFacilitySpec Lighting = Spec;
    Lighting.FacilityDefinitionId = TEXT("Lighting_Decorative_CrowdTest");
    Lighting.DisplayName = FText::FromString(TEXT("Solid lighting obstacle test"));
    Lighting.Category = EFFZSFacilityCategory::Lighting;
    Lighting.PlacementObstacleHeightCm = 250.0f;
    Placement.OriginCell = FIntPoint(3000, 3000);
    if (!ExecuteChecked(TEXT("Ordinary solid facility installs"),
        UFFZSGridLibrary::MakeInstallCommand(Lighting, Zone->ZoneId, Placement))) return false;
    Crowd->ConfigureSimAreaWithBounds(FVector(2500, 2500, 0), FBox::BuildAABB(FVector(2500, 2500, 0), FVector(2000.0, 2000.0, 2000.0)));
    TestEqual(TEXT("Other facility avoidance wall remains"), Crowd->GetFacilityWallCount(), 1);
    Crowd->TryGetGroundZ(FVector(3200, 3200, 0), Height);
    TestEqual(TEXT("Other facility roofs do not support NPC height"), Height, 0.0);

    FFFZSCommand Remove;
    Remove.CommandId = FGuid::NewGuid();
    Remove.TransactionId = FGuid::NewGuid();
    Remove.Type = EFFZSCommandType::Remove;
    Remove.FacilityInstanceId = Install.FacilityInstanceId;
    if (!ExecuteChecked(TEXT("Unsupported road removes"), Remove)) return false;
    Crowd->TryGetGroundZ(FVector(1200, 1200, 0), Height);
    TestEqual(TEXT("Removal immediately returns NPC height to terrain"), Height, 0.0);
    Crowd->EnsureRoadLanes();
    TestEqual(TEXT("Removing the surface clears its lane input"), Crowd->GetRoadLaneStats().TileCount, 0);
    return true;
}

#endif
