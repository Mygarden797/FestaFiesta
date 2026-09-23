// FF_FFZSAudienceWorldTests.cpp
// 역할: 시설 설치와 관람 영역이 실제 테스트 월드에서 서로 일관되게 연결되는지 검증한다.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "FF_FFZSAudienceLibrary.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "FF_FFZSSaveGameTestObject.h"
#include "Kismet/GameplayStatics.h"
#include "Tests/FF_FFZSTestFixtures.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSAudienceWorldTest, "FF_ZoneSystem.Audience.InstanceLookupAndSaveGame",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFFZSAudienceWorldTest::RunTest(const FString& Parameters)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* Zones = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("World exists"), World) || !TestNotNull(TEXT("Facility registry exists"), Zones)) return false;
    AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
    if (!Zone) return false;
    Zone->ApplyAcceptedZonePreset(TEXT("GZ_DadaPocha"));
    if (!Zones->RegisterZone(Zone)) return false;
    FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec();
    Spec.Audience.bEnabled = true;
    Spec.Audience.FirstSlotLocalCm = FVector(-760, 3000, 0);
    Spec.Audience.EntryLocalCm = FVector(-850, 4170, 0);
    Spec.Audience.ExitLocalCm = FVector(850, 4170, 0);
    AFFZSFacilityActor* Facility = World->SpawnActor<AFFZSFacilityActor>();
    const FGuid Id = FGuid::NewGuid();
    const FTransform Placement(FRotator(0, 90, 0), FVector(1000, 2000, 80));
    if (!TestTrue(TEXT("Facility initializes"), Facility && Facility->InitializeFacility(Id, Spec, Zone->ZoneId,
        FFFZSGridPlacement(), Placement, EFFZSFacilityState::Active))) return false;
    if (!TestTrue(TEXT("Facility registers"), Zones->RegisterFacility(Facility))) return false;
    FFFZSAudienceSnapshot Audience;
    FText Error;
    TestTrue(TEXT("Runtime audience resolves by instance ID"), UFFZSAudienceLibrary::GetFacilityAudienceSnapshot(World, Id, Audience, Error));
    TestEqual(TEXT("Runtime capacity is independent of FIFO capacity"), Audience.Capacity, 120);
    TestTrue(TEXT("Entry follows the installed transform"), Audience.EntryWorldCm.Equals(Placement.TransformPosition(Spec.Audience.EntryLocalCm), .01));
    UBoxComponent* Obstacle = nullptr;
    TInlineComponentArray<UBoxComponent*> Boxes;
    Facility->GetComponents(Boxes);
    for (UBoxComponent* Box : Boxes) if (Box->GetFName() == TEXT("FacilityNavigationObstacleBounds")) Obstacle = Box;
    TestTrue(TEXT("Audience never becomes a solid NavMesh obstacle"), Obstacle &&
        FMath::IsNearlyEqual(Obstacle->GetUnscaledBoxExtent().X, Spec.FootprintSizeCells.X * .5));
    UFFZSSaveGameTestObject* Save = NewObject<UFFZSSaveGameTestObject>();
    Save->Snapshot.Facilities.Add(Facility->CaptureInstanceSnapshot());
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("SaveGame archive writes audience"), UGameplayStatics::SaveGameToMemory(Save, Bytes))) return false;
    const UFFZSSaveGameTestObject* Loaded = Cast<UFFZSSaveGameTestObject>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!TestNotNull(TEXT("SaveGame archive reloads"), Loaded) || Loaded->Snapshot.Facilities.Num() != 1) return false;
    const auto& SavedFacility = Loaded->Snapshot.Facilities[0];
    TestTrue(TEXT("Saved audience equals the installed base layout"), FFFZSAudienceLayout::StaticStruct()->CompareScriptStruct(
        &Spec.Audience, &SavedFacility.FacilitySpec.Audience, 0));
    AFFZSFacilityActor* Restored = World->SpawnActor<AFFZSFacilityActor>();
    TestTrue(TEXT("Restored facility can use saved audience without loading the PDA"), Restored && Restored->InitializeFacility(
        FGuid::NewGuid(), SavedFacility.FacilitySpec, SavedFacility.ZoneId, SavedFacility.Placement,
        SavedFacility.WorldTransform, SavedFacility.State));
    Zones->UnregisterFacility(Facility);
    TestFalse(TEXT("Removed registry entry invalidates audience lookup"), UFFZSAudienceLibrary::GetFacilityAudienceSnapshot(World, Id, Audience, Error));
    TestEqual(TEXT("Failed lookup clears stale capacity"), Audience.Capacity, 0);
    TArray<int32> Failed;
    TestFalse(TEXT("Unavailable navigation is never reported as success"),
        UFFZSAudienceLibrary::CheckAudienceNavigationBatch(nullptr, Audience, 0, 1, Failed, Error));
    return true;
}

#endif
