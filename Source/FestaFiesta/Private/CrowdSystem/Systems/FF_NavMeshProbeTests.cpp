// Persistent release-map regression: validate the paths the crowd actually consumes.
// A top-down trace may hit roofs, and a coarse world grid does not prove connectivity.
// Neither is used as a pass/fail proxy for the production anchors and exit.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "CrowdSystem/FF_SpawnActor.h"
#include "CrowdSystem/Systems/FF_CrowdDecisionSubsystem.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/UnrealType.h"

namespace
{
bool HasCompleteNavigationPath(UNavigationSystemV1& Navigation, const ANavigationData& Data,
    const FVector& Start, const FVector& End)
{
    FPathFindingQuery Query(nullptr, Data, Start, End);
    Query.SetAllowPartialPaths(false);
    const FPathFindingResult Result = Navigation.FindPathSync(Query);
    return Result.IsSuccessful() && !Result.IsPartial() && Result.Path.IsValid()
        && Result.Path->GetPathPoints().Num() > 0;
}

class FFF_CheckProductionNavigation final : public IAutomationLatentCommand
{
public:
    explicit FFF_CheckProductionNavigation(FAutomationTestBase* InTest)
        : Test(InTest), StartedAt(FPlatformTime::Seconds()) {}

    virtual bool Update() override
    {
        UWorld* World = nullptr;
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::PIE && Context.World())
            {
                World = Context.World();
                break;
            }
        }
        UNavigationSystemV1* Navigation = World
            ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
        const ANavigationData* Data = Navigation
            ? Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
        if (!World || !World->HasBegunPlay() || !Data
            || UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World))
        {
            if (FPlatformTime::Seconds() - StartedAt < 60.0)
            {
                return false;
            }
            Test->AddError(TEXT("Production PIE navigation did not become ready within 60 seconds."));
            return true;
        }

        const UFFZSWorldSubsystem* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
        if (!Test->TestNotNull(TEXT("Runtime zone registry exists"), Zones))
        {
            return true;
        }
        const FFFZSReadModel ReadModel = Zones->BuildReadModel();
        Test->TestEqual(TEXT("All 31 authored release-map zones are registered"), ReadModel.Zones.Num(), 31);
        TSet<FName> UniqueIds;
        TArray<FVector> Points;
        TArray<FName> Ids;
        for (const FFFZSZoneReadModel& Zone : ReadModel.Zones)
        {
            Test->TestFalse(FString::Printf(TEXT("%s has a unique identity"), *Zone.ZoneId.ToString()),
                UniqueIds.Contains(Zone.ZoneId));
            UniqueIds.Add(Zone.ZoneId);
            const AFFZSGameplayZone* Actor = Zones->FindZone(Zone.ZoneId);
            if (!Test->TestNotNull(TEXT("Registered zone actor exists"), Actor))
            {
                continue;
            }
            const FVector Anchor = Zone.EntryExitAnchors.IsEmpty() ? Actor->GetActorLocation()
                : Actor->GetActorTransform().TransformPosition(Zone.EntryExitAnchors[0].GetLocation());
            FNavLocation Projected;
            if (Test->TestTrue(FString::Printf(TEXT("%s production anchor projects to navigation"),
                *Zone.ZoneId.ToString()), Navigation->ProjectPointToNavigation(
                    Anchor, Projected, FFF_CrowdNav::GetAnchorProjectionExtentCm())))
            {
                Points.Add(Projected.Location);
                Ids.Add(Zone.ZoneId);
            }
        }

        const AFF_SpawnActor* Spawn = nullptr;
        int32 SpawnCount = 0;
        for (TActorIterator<AFF_SpawnActor> It(World); It; ++It)
        {
            Spawn = *It;
            ++SpawnCount;
        }
        if (!Test->TestEqual(TEXT("Release map has exactly one crowd spawn origin"), SpawnCount, 1))
        {
            return true;
        }
        const UFF_CrowdSimulationSubsystem* CrowdSim = World->GetSubsystem<UFF_CrowdSimulationSubsystem>();
        if (!Test->TestNotNull(TEXT("Runtime crowd simulation exists"), CrowdSim))
        {
            return true;
        }
        const FStructProperty* OffsetProperty = CastField<FStructProperty>(
            AFF_SpawnActor::StaticClass()->FindPropertyByName(TEXT("Temp_ExitOffsetFromOrigin")));
        const FObjectPropertyBase* SpawnAnchorProperty = CastField<FObjectPropertyBase>(
            AFF_SpawnActor::StaticClass()->FindPropertyByName(TEXT("SpawnAchor")));
        const FObjectPropertyBase* DespawnAnchorProperty = CastField<FObjectPropertyBase>(
            AFF_SpawnActor::StaticClass()->FindPropertyByName(TEXT("DespawnAchor")));
        if (!Test->TestNotNull(TEXT("Production exit offset property exists"), OffsetProperty)
            || !Test->TestNotNull(TEXT("Production spawn anchor property exists"), SpawnAnchorProperty)
            || !Test->TestNotNull(TEXT("Production despawn anchor property exists"), DespawnAnchorProperty))
        {
            return true;
        }
        const AActor* SpawnAnchor = Cast<AActor>(SpawnAnchorProperty->GetObjectPropertyValue_InContainer(Spawn));
        const AActor* DespawnAnchor = Cast<AActor>(DespawnAnchorProperty->GetObjectPropertyValue_InContainer(Spawn));
        // Match AFF_SpawnActor::BeginPlay: the exit offset is relative to the actor,
        // not the optional spawn override. Runtime gates are normalized by GatherCrowdGates.
        const FVector SpawnPoint = IsValid(SpawnAnchor) ? SpawnAnchor->GetActorLocation() : Spawn->GetActorLocation();
        const FVector FallbackExit = IsValid(DespawnAnchor) ? DespawnAnchor->GetActorLocation()
            : Spawn->GetActorLocation() + *OffsetProperty->ContainerPtrToValuePtr<FVector>(Spawn);
        const float ArrivalRadius = CrowdSim->Temp_ExitArrivalRadiusUU;
        if (!Test->TestTrue(TEXT("Runtime exit arrival radius is finite and positive"),
            FMath::IsFinite(ArrivalRadius) && ArrivalRadius > 0.f))
        {
            return true;
        }
        TArray<FVector> Exits { FallbackExit };
        TArray<FString> ExitNames { TEXT("fallback exit") };
        // DrainVisitorUntil assigns EntryGateIndex from PickSpawnGate; Closing's
        // AllAgentLeavingTrigger returns to that gate even when bAllowExit is false.
        // Exit-only gates cannot be selected by this route. Include the synthesized
        // fallback spawn gate, which also becomes an agent's entry/closing target.
        for (const FFF_CrowdGate& Gate : CrowdSim->GetValidGates())
        {
            if (!Gate.bAllowSpawn)
            {
                continue;
            }
            Exits.Add(Gate.GateLocation);
            ExitNames.Add(FString::Printf(TEXT("gate %s"), *Gate.GateId.ToString()));
        }
        FNavLocation SpawnNav;
        const bool bSpawnProjects = Test->TestTrue(TEXT("Actual spawn origin projects to navigation"),
            Navigation->ProjectPointToNavigation(SpawnPoint, SpawnNav, FFF_CrowdNav::GetAnchorProjectionExtentCm()));
        for (int32 ExitIndex = 0; ExitIndex < Exits.Num(); ++ExitIndex)
        {
            const FVector& Exit = Exits[ExitIndex];
            const FString& ExitName = ExitNames[ExitIndex];
            FNavLocation ExitNav;
            if (!Test->TestTrue(FString::Printf(TEXT("%s projects to navigation"), *ExitName),
                Navigation->ProjectPointToNavigation(Exit, ExitNav, FVector(200.0, 200.0, 500.0))))
            {
                continue;
            }
            Test->TestTrue(FString::Printf(TEXT("%s projection stays inside runtime arrival radius %.1f cm"),
                *ExitName, ArrivalRadius), FVector::Dist2D(ExitNav.Location, Exit) <= ArrivalRadius);
            if (bSpawnProjects)
            {
                Test->TestTrue(FString::Printf(TEXT("Actual spawn has a complete path to %s"), *ExitName),
                    HasCompleteNavigationPath(*Navigation, *Data, SpawnNav.Location, ExitNav.Location));
            }
            for (int32 I = 0; I < Points.Num(); ++I)
            {
                Test->TestTrue(FString::Printf(TEXT("%s can reach %s without a partial path"),
                    *Ids[I].ToString(), *ExitName), HasCompleteNavigationPath(*Navigation, *Data, Points[I], ExitNav.Location));
                FPathFindingQuery ExitQuery(nullptr, *Data, Points[I], Exit);
                ExitQuery.SetAllowPartialPaths(true);
                ExitQuery.SetRequireNavigableEndLocation(false);
                const FPathFindingResult Result = Navigation->FindPathSync(ExitQuery);
                Test->TestTrue(FString::Printf(TEXT("%s actual crowd query to %s reaches the runtime arrival radius"),
                    *Ids[I].ToString(), *ExitName), Result.IsSuccessful() && !Result.IsPartial() && Result.Path.IsValid()
                    && !Result.Path->GetPathPoints().IsEmpty()
                    && FVector::Dist2D(Result.Path->GetPathPoints().Last().Location, Exit) <= ArrivalRadius);
            }
        }
        for (int32 I = 0; I < Points.Num(); ++I)
        {
            for (int32 J = 0; J < Points.Num(); ++J)
            {
                if (I != J)
                {
                    Test->TestTrue(FString::Printf(TEXT("Complete directed path: %s -> %s"),
                        *Ids[I].ToString(), *Ids[J].ToString()),
                        HasCompleteNavigationPath(*Navigation, *Data, Points[I], Points[J]));
                }
            }
        }
        Test->AddInfo(FString::Printf(TEXT("Checked %d zones against %d runtime exit targets and %d directed zone-to-zone paths; arrival radius %.1f cm."),
            Points.Num(), Exits.Num(), Points.Num() * (Points.Num() - 1), ArrivalRadius));
        return true;
    }

private:
    FAutomationTestBase* Test;
    double StartedAt;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NavMeshProbe, "FestaFiesta.Probe.NavMeshBakeStatus",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFF_NavMeshProbe::RunTest(const FString& Parameters)
{
    UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/FestaFiesta/Maps/Test_Main"));
    if (!TestNotNull(TEXT("Release map loads"), World))
    {
        return false;
    }
    UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!TestNotNull(TEXT("Release map has a navigation system"), Navigation))
    {
        return false;
    }
    const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(
        Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate));
    if (!TestNotNull(TEXT("Release map has baked Recast data"), Recast))
    {
        return false;
    }
    TestTrue(TEXT("Offline navigation contains valid baked tiles"), Recast->HasValidNavmesh());
    TestEqual(TEXT("Runtime facilities use modifiers on the offline bake"),
        static_cast<int32>(Recast->GetRuntimeGenerationMode()),
        static_cast<int32>(ERuntimeGenerationType::DynamicModifiersOnly));

    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FFF_CheckProductionNavigation(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
