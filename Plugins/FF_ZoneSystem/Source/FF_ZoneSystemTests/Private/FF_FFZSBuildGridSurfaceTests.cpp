// FF_FFZSBuildGridSurfaceTests.cpp
// 역할: 지면 추적, 캐시 만료, 지형 변경, 추적 예산이 화면 격자에 올바르게 반영되는지 검증한다.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "FF_FFZSBuildableGrid.h"
#include "FF_FFZSBuildGridSurface.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "HAL/PlatformProcess.h"

namespace
{
    struct FScopedBuildGridGroundSettings
    {
        UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
        bool bGround = Settings->bEnableGroundChecks;
        bool bEvery = Settings->bRequireEveryGroundSample;
        float Slope = Settings->MaxGroundSlopeDegrees;
        float Variance = Settings->MaxGroundHeightVarianceCm;

        FScopedBuildGridGroundSettings()
        {
            Settings->bEnableGroundChecks = true;
            Settings->bRequireEveryGroundSample = true;
            Settings->MaxGroundSlopeDegrees = 5.0f;
            Settings->MaxGroundHeightVarianceCm = 20.0f;
        }
        ~FScopedBuildGridGroundSettings()
        {
            Settings->bEnableGroundChecks = bGround;
            Settings->bRequireEveryGroundSample = bEvery;
            Settings->MaxGroundSlopeDegrees = Slope;
            Settings->MaxGroundHeightVarianceCm = Variance;
        }
    };

    UBoxComponent* AddGroundBox(AActor& Actor, const FVector& Extent)
    {
        UBoxComponent* Box = NewObject<UBoxComponent>(&Actor);
        if (Actor.GetRootComponent())
        {
            Box->SetupAttachment(Actor.GetRootComponent());
        }
        else
        {
            Actor.SetRootComponent(Box);
        }
        Actor.AddInstanceComponent(Box);
        Box->SetBoxExtent(Extent);
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Box->SetCollisionObjectType(ECC_WorldStatic);
        Box->SetCollisionResponseToAllChannels(ECR_Ignore);
        Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Box->RegisterComponent();
        return Box;
    }

    AActor* SpawnFloor(UWorld& World, const FVector& Location, const FVector& Extent)
    {
        AActor* Actor = World.SpawnActor<AActor>();
        if (Actor)
        {
            AddGroundBox(*Actor, Extent);
            Actor->SetActorLocation(Location);
        }
        return Actor;
    }

    FFFZSBuildableGridMask MakeGroundMask(const int32 WidthCells = 1)
    {
        FFFZSBuildableGridMask Mask;
        Mask.Initialize(FVector2D::ZeroVector, FVector2D(WidthCells * 100.0), 100);
        for (int32 Y = 0; Y < WidthCells; ++Y)
        {
            for (int32 X = 0; X < WidthCells; ++X)
            {
                Mask.SetBuildable(X, Y, true);
            }
        }
        return Mask;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSBuildGridSurfaceBudgetTest,
    "FF_ZoneSystem.Integration.BuildableGrid.Surface.BudgetAndCenterPriority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSBuildGridSurfaceBudgetTest::RunTest(const FString& Parameters)
{
    FScopedBuildGridGroundSettings Settings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("World"), World) ||
        !TestNotNull(TEXT("Floor"), SpawnFloor(*World, FVector(150.0, 150.0, 90.0), FVector(500.0, 500.0, 10.0))))
    {
        return false;
    }
    FFFZSBuildGridSurfaceCache Cache;
    FFFZSBuildableGridMask Mask = MakeGroundMask(3);
    Cache.Update(*World, FTransform::Identity, 100.0, Mask, 3);
    TestEqual(TEXT("Three samples cannot invent a four-corner ground cell"), Mask.GetBuildableCellCount(), 0);
    TestTrue(TEXT("Budget-limited work remains pending"), Cache.HasPendingSamples());
    Mask = MakeGroundMask(3);
    Cache.Update(*World, FTransform::Identity, 105.0, Mask, 1);
    TestTrue(TEXT("The central cell appears first and a small height change reuses samples"), Mask.IsBuildable(1, 1));
    TestEqual(TEXT("Only the four sampled corners produce one cell"), Mask.GetBuildableCellCount(), 1);
    TestTrue(TEXT("Grid follows the actual floor rather than cursor plane"),
        FMath::IsNearlyEqual(Cache.GetWorldPoint(FVector2D(100.0)).Z, 100.0, 0.01));
    TestTrue(TEXT("An edge fade point interpolates sampled floor endpoints"),
        FMath::IsNearlyEqual(Cache.GetWorldPoint(FVector2D(100.0, 150.0)).Z, 100.0, 0.01));
    Mask = MakeGroundMask(3);
    Cache.Update(*World, FTransform::Identity, 100.0, Mask, 512);
    TestEqual(TEXT("Remaining shared vertices complete the local window"), Mask.GetBuildableCellCount(), 9);
    TestFalse(TEXT("Completed samples stop the fast refresh path"), Cache.HasPendingSamples());
    Mask = MakeGroundMask(3);
    Cache.Update(*World, FTransform::Identity, 100.0, Mask, 0);
    TestEqual(TEXT("A stationary grid needs no duplicate traces"), Mask.GetBuildableCellCount(), 9);
    Mask = MakeGroundMask(3);
    Cache.Update(*World, FTransform::Identity, 10000.0, Mask, 0);
    TestEqual(TEXT("Cursor height cannot move the authoritative zone-ground trace band"), Mask.GetBuildableCellCount(), 9);
    Cache.Reset();
    Mask = MakeGroundMask();
    Cache.Update(*World, FTransform::Identity, 10000.0, Mask, 4);
    TestEqual(TEXT("A new sample with a high cursor still traces the authority zone plane"), Mask.GetBuildableCellCount(), 1);
    TestTrue(TEXT("Cold high-cursor queries resolve the real ground"),
        FMath::IsNearlyEqual(Cache.GetWorldPoint(FVector2D::ZeroVector).Z, 100.0, 0.01));
    Cache.Reset();
    Mask = MakeGroundMask();
    Cache.Update(*World, FTransform::Identity, 100.0, Mask, 0);
    TestEqual(TEXT("Reset prevents old floor samples from showing"), Mask.GetBuildableCellCount(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSBuildGridSurfaceGroundRulesTest,
    "FF_ZoneSystem.Integration.BuildableGrid.Surface.GroundRulesAndIgnoredRoofs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSBuildGridSurfaceGroundRulesTest::RunTest(const FString& Parameters)
{
    FScopedBuildGridGroundSettings Settings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("World"), World))
    {
        return false;
    }
    FFFZSBuildGridSurfaceCache Cache;
    FFFZSBuildableGridMask Mask = MakeGroundMask();
    Cache.Update(*World, FTransform::Identity, 100.0, Mask);
    TestEqual(TEXT("Missing floor receives no grid"), Mask.GetBuildableCellCount(), 0);
    Settings.Settings->bRequireEveryGroundSample = false;
    Mask = MakeGroundMask();
    Cache.Update(*World, FTransform::Identity, 100.0, Mask);
    TestEqual(TEXT("Permissive authority sampling still does not invent visual ground"), Mask.GetBuildableCellCount(), 0);

    AActor* Floor = SpawnFloor(*World, FVector(50.0, 50.0, 90.0), FVector(500.0, 500.0, 10.0));
    AFFZSFacilityActor* Facility = World->SpawnActor<AFFZSFacilityActor>();
    if (!TestNotNull(TEXT("Floor"), Floor) || !TestNotNull(TEXT("Facility"), Facility))
    {
        return false;
    }
    AddGroundBox(*Facility, FVector(300.0, 300.0, 20.0));
    Facility->SetActorLocation(FVector(50.0, 50.0, 400.0));
    Cache.Reset();
    Mask = MakeGroundMask();
    Cache.Update(*World, FTransform::Identity, 100.0, Mask);
    TestTrue(TEXT("Facility geometry is excluded from ground queries"),
        FMath::IsNearlyEqual(Cache.GetWorldPoint(FVector2D::ZeroVector).Z, 100.0, 0.01));
    TestEqual(TEXT("Real flat ground remains visible under ignored presentation geometry"), Mask.GetBuildableCellCount(), 1);

    Floor->SetActorRotation(FRotator(20.0, 0.0, 0.0));
    Settings.Settings->MaxGroundHeightVarianceCm = 1000.0f;
    Mask = MakeGroundMask();
    Cache.Update(*World, FTransform::Identity, 100.0, Mask);
    TestEqual(TEXT("Too-steep ground hides its cells"), Mask.GetBuildableCellCount(), 0);
    Settings.Settings->MaxGroundSlopeDegrees = 45.0f;
    Settings.Settings->MaxGroundHeightVarianceCm = 1.0f;
    Mask = MakeGroundMask();
    Cache.Update(*World, FTransform::Identity, 100.0, Mask);
    TestEqual(TEXT("Corner height variance is checked independently of slope"), Mask.GetBuildableCellCount(), 0);
    Settings.Settings->bEnableGroundChecks = false;
    Mask = MakeGroundMask();
    Cache.Update(*World, FTransform::Identity, 120.0, Mask, 0);
    TestEqual(TEXT("Explicit ground-check override retains spatial mask without traces"), Mask.GetBuildableCellCount(), 1);
    TestEqual(TEXT("Override uses supplied presentation height"), Cache.GetWorldPoint(FVector2D::ZeroVector).Z, 120.0);
    TestFalse(TEXT("Override has no pending physical work"), Cache.HasPendingSamples());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSBuildGridSurfaceExpiryTest,
    "FF_ZoneSystem.Integration.BuildableGrid.Surface.ConfirmedFloorPersistsUntilChanged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSBuildGridSurfaceExpiryTest::RunTest(const FString& Parameters)
{
    FScopedBuildGridGroundSettings Settings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("World"), World))
    {
        return false;
    }
    AActor* Floor = SpawnFloor(*World, FVector(50.0, 50.0, 90.0), FVector(500.0, 500.0, 10.0));
    if (!TestNotNull(TEXT("Floor"), Floor))
    {
        return false;
    }
    FFFZSBuildGridSurfaceCache Cache;
    FFFZSBuildableGridMask Mask = MakeGroundMask();
    Cache.Update(*World, FTransform::Identity, 100.0, Mask);
    TestEqual(TEXT("Initial floor is visible"), Mask.GetBuildableCellCount(), 1);
    FPlatformProcess::Sleep(2.55f);
    Mask = MakeGroundMask();
    Cache.Update(*World, FTransform::Identity, 100.0, Mask, 0);
    TestEqual(TEXT("A delayed verification cannot hide already confirmed unchanged ground"), Mask.GetBuildableCellCount(), 1);
    TestTrue(TEXT("Overdue verification still requests a bounded follow-up"), Cache.HasPendingSamples());
    TestEqual(TEXT("Confirmed vertices retain their actual height past the verification deadline"),
        Cache.GetWorldPoint(FVector2D::ZeroVector).Z, 100.0);
    Mask = MakeGroundMask();
    Cache.Update(*World, FTransform::Identity, 100.0, Mask, 4);
    TestEqual(TEXT("Verifying unchanged terrain preserves the visible cell"), Mask.GetBuildableCellCount(), 1);

    Floor->SetActorLocation(FVector(50.0, 50.0, 140.0));
    Mask = MakeGroundMask();
    Cache.Update(*World, FTransform::Identity, 100.0, Mask, 0);
    TestEqual(TEXT("Moving the real support invalidates its previous height without waiting for a timer"), Mask.GetBuildableCellCount(), 0);
    Mask = MakeGroundMask();
    Cache.Update(*World, FTransform::Identity, 100.0, Mask, 4);
    TestEqual(TEXT("A moved support is resampled at its new height"), Cache.GetWorldPoint(FVector2D::ZeroVector).Z, 150.0);
    auto* Ground = Cast<UBoxComponent>(Floor->GetRootComponent());
    Ground->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mask = MakeGroundMask();
    Cache.Update(*World, FTransform::Identity, 100.0, Mask, 0);
    TestEqual(TEXT("Disabling support collision invalidates confirmed samples immediately"), Mask.GetBuildableCellCount(), 0);
    Ground->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Mask = MakeGroundMask();
    Cache.Update(*World, FTransform::Identity, 100.0, Mask, 4);
    TestEqual(TEXT("Repaired collision restores the real floor"), Mask.GetBuildableCellCount(), 1);

    Floor->Destroy();
    Mask = MakeGroundMask();
    Cache.Update(*World, FTransform::Identity, 100.0, Mask, 0);
    TestEqual(TEXT("Destroyed support cannot retain a confirmed sample"), Mask.GetBuildableCellCount(), 0);
    Mask = MakeGroundMask();
    Cache.Update(*World, FTransform::Identity, 100.0, Mask, 4);
    TestEqual(TEXT("Resampling confirms the floor is gone"), Mask.GetBuildableCellCount(), 0);
    TestFalse(TEXT("Known holes do not spin in the fast refresh path"), Cache.HasPendingSamples());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSBuildGridSurfaceLifetimeBudgetTest,
    "FF_ZoneSystem.Integration.BuildableGrid.Surface.AdaptiveLifetimeCoversFullRefresh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSBuildGridSurfaceLifetimeBudgetTest::RunTest(const FString& Parameters)
{
    for (const int32 Vertices : { 4, 16641, 32770 })
    {
        for (const int32 Budget : { 128, 512 })
        {
            const double PassSeconds = FMath::CeilToDouble(static_cast<double>(Vertices) / Budget) * 0.08;
            const double Lifetime = FFZSBuildGridSurface::CalculateSampleLifetimeSeconds(Vertices, Budget);
            const double Lead = FFZSBuildGridSurface::CalculateRefreshLeadSeconds(Vertices, Budget);
            TestTrue(TEXT("Freshness covers two complete sampling passes and scheduling allowance"),
                Lifetime >= PassSeconds * 2.0 + 0.5);
            TestTrue(TEXT("Renewal starts a full pass before expiration"), Lead >= PassSeconds + 0.5);
            TestTrue(TEXT("A renewed sample remains usable before its next refresh window"), Lifetime > Lead);
        }
    }
    TestEqual(TEXT("Small windows retain a two-second minimum lifetime"),
        FFZSBuildGridSurface::CalculateSampleLifetimeSeconds(4), 2.0);
    TestEqual(TEXT("A caller cannot inflate the physical per-update trace cap"),
        FFZSBuildGridSurface::CalculateSampleLifetimeSeconds(16641, 50000),
        FFZSBuildGridSurface::CalculateSampleLifetimeSeconds(16641, 512));
    return true;
}

#endif
