// FF_FFZSNavigationObstacleTests.cpp

// 시설의 고형 footprint만 동적 NavArea_Null modifier로 게시되는 수명주기 계약을 검증한다.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"

#include "Components/BoxComponent.h"
#include "AI/Navigation/NavAreaBase.h"
#include "Engine/World.h"
#include "NavAreas/NavArea_Null.h"
#include "NavMesh/RecastNavMesh.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Tests/FF_FFZSTestFixtures.h"

namespace
{
    class FScopedNavigationObstacleTestSettings
    {
    public:
        FScopedNavigationObstacleTestSettings()
            : Settings(GetMutableDefault<UFFZSDeveloperSettings>())
            , bGround(Settings->bEnableGroundChecks)
            , bCollision(Settings->bEnableWorldCollisionChecks)
            , bNavigation(Settings->bEnableNavigationProjectionChecks)
            , bProductionPolicies(Settings->bRequireProductionPolicyDomains)
            , MaxEnumeratedCells(Settings->MaxEnumeratedCellsPerFacility)
        {
            Settings->bEnableGroundChecks = false;
            Settings->bEnableWorldCollisionChecks = false;
            Settings->bEnableNavigationProjectionChecks = false;
            Settings->bRequireProductionPolicyDomains = false;
            Settings->MaxEnumeratedCellsPerFacility = 1'000'000;
        }

        ~FScopedNavigationObstacleTestSettings()
        {
            Settings->bEnableGroundChecks = bGround;
            Settings->bEnableWorldCollisionChecks = bCollision;
            Settings->bEnableNavigationProjectionChecks = bNavigation;
            Settings->bRequireProductionPolicyDomains = bProductionPolicies;
            Settings->MaxEnumeratedCellsPerFacility = MaxEnumeratedCells;
        }

    private:
        UFFZSDeveloperSettings* Settings;
        bool bGround;
        bool bCollision;
        bool bNavigation;
        bool bProductionPolicies;
        int32 MaxEnumeratedCells;
    };

    UBoxComponent* FindNavigationObstacle(AFFZSFacilityActor& Facility)
    {
        TInlineComponentArray<UBoxComponent*> Boxes;
        Facility.GetComponents(Boxes);
        for (UBoxComponent* Box : Boxes)
        {
            if (Box && Box->GetFName() == TEXT("FacilityNavigationObstacleBounds"))
            {
                return Box;
            }
        }
        return nullptr;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSFacilityNavigationObstacleLifecycleTest,
    "FF_ZoneSystem.Integration.Navigation.FacilityFootprintObstacleLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSFacilityNavigationObstacleLifecycleTest::RunTest(const FString& Parameters)
{
    FScopedNavigationObstacleTestSettings ScopedSettings;
    TestEqual(TEXT("호스트 Recast는 modifier 전용 런타임 갱신을 사용한다"),
        GetDefault<ARecastNavMesh>()->GetRuntimeGenerationMode(),
        ERuntimeGenerationType::DynamicModifiersOnly);
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* Subsystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("자동화 테스트 World"), World) ||
        !TestNotNull(TEXT("권위 World Subsystem"), Subsystem))
    {
        return false;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>(
        AFFZSGameplayZone::StaticClass(), FTransform::Identity, SpawnParameters);
    const FName ZoneId(TEXT("GZ_DadaPocha"));
    if (!TestNotNull(TEXT("시험 Zone"), Zone))
    {
        return false;
    }
    Zone->ApplyAcceptedZonePreset(ZoneId);
    if (!TestTrue(TEXT("시험 Zone이 등록된다"), Subsystem->RegisterZone(Zone)))
    {
        return false;
    }

    FFFZSGridPlacement Placement;
    Placement.OriginCell = FIntPoint(1000, 1000);
    const FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommand(
        FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck),
        ZoneId,
        Placement);
    if (!TestTrue(TEXT("시설 설치가 성공한다"), Subsystem->ExecuteCommand(Install).bSucceeded))
    {
        return false;
    }

    AFFZSFacilityActor* Facility = Subsystem->FindFacility(Install.FacilityInstanceId);
    UBoxComponent* NavigationObstacle = Facility ? FindNavigationObstacle(*Facility) : nullptr;
    if (!TestNotNull(TEXT("설치된 시설"), Facility) ||
        !TestNotNull(TEXT("Nav 전용 footprint Box"), NavigationObstacle))
    {
        return false;
    }

    TestEqual(TEXT("Nav Box는 물리 충돌이 없다"),
        NavigationObstacle->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
    TestFalse(TEXT("Nav Box는 overlap 이벤트를 만들지 않는다"),
        NavigationObstacle->GetGenerateOverlapEvents());
    TestTrue(TEXT("Nav Box는 dynamic obstacle이다"), NavigationObstacle->bDynamicObstacle != 0);
    TestEqual(TEXT("Nav Box는 NavArea_Null을 사용한다"),
        NavigationObstacle->GetDesiredAreaClass().Get(), UNavArea_Null::StaticClass());
    TestTrue(TEXT("등록 완료된 시설은 Navigation에 반영된다"),
        NavigationObstacle->CanEverAffectNavigation());
    TestEqual(TEXT("Nav Box는 확장 없이 footprint 크기를 사용한다"),
        NavigationObstacle->GetUnscaledBoxExtent(), Facility->FootprintBounds->GetUnscaledBoxExtent());
    TestEqual(TEXT("Nav Box는 footprint와 같은 높이에 놓인다"),
        NavigationObstacle->GetRelativeLocation(), Facility->FootprintBounds->GetRelativeLocation());

    const FTransform PreviousObstacleTransform = NavigationObstacle->GetComponentTransform();
    FFFZSCommand Move;
    Move.CommandId = FGuid::NewGuid();
    Move.TransactionId = FGuid::NewGuid();
    Move.Type = EFFZSCommandType::Move;
    Move.FacilityInstanceId = Install.FacilityInstanceId;
    Move.ZoneId = ZoneId;
    Move.Placement.OriginCell = FIntPoint(3000, 1000);
    TestTrue(TEXT("시설 이동이 성공한다"), Subsystem->ExecuteCommand(Move).bSucceeded);
    TestFalse(TEXT("이동 후 Nav Box Transform이 갱신된다"),
        NavigationObstacle->GetComponentTransform().Equals(PreviousObstacleTransform));
    TestTrue(TEXT("이동 후에도 Navigation 반영이 유지된다"),
        NavigationObstacle->CanEverAffectNavigation());

    Subsystem->UnregisterFacility(Facility);
    TestFalse(TEXT("등록 해제된 시설은 Navigation에서 제거된다"),
        NavigationObstacle->CanEverAffectNavigation());
    TestTrue(TEXT("재등록이 성공한다"), Subsystem->RegisterFacility(Facility));
    TestTrue(TEXT("재등록된 시설은 Navigation에 다시 반영된다"),
        NavigationObstacle->CanEverAffectNavigation());

    const FFFZSWorldSnapshot Snapshot = Subsystem->CaptureWorldSnapshot();
    TArray<FText> RestoreErrors;
    TestTrue(TEXT("스냅샷 복원이 성공한다"),
        Subsystem->RestoreWorldSnapshot(Snapshot, RestoreErrors));
    AFFZSFacilityActor* RestoredFacility = Subsystem->FindFacility(Install.FacilityInstanceId);
    UBoxComponent* RestoredObstacle = RestoredFacility
        ? FindNavigationObstacle(*RestoredFacility)
        : nullptr;
    if (!TestNotNull(TEXT("복원된 시설의 Nav Box"), RestoredObstacle))
    {
        return false;
    }
    TestTrue(TEXT("복원 게시 시점에 Nav Box가 활성화된다"),
        RestoredObstacle->CanEverAffectNavigation());

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
