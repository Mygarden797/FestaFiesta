// FF_ZoneAccessExitTests.cpp
// 역할: 실제 군중 스폰 액터의 속성으로 Zone의 출구 우선순위와 BeginPlay 이전 조회를 검증한다.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "FF_FFZSZoneAccessSubsystem.h"
#include "Components/SceneComponent.h"
#include "CrowdSystem/FF_SpawnActor.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ZoneAccessCrowdExitContract,
    "FestaFiesta.ZoneIntegration.Exit.CrowdSpawnContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFF_ZoneAccessCrowdExitContract::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game))
    {
        Fixture.ForwardErrorMessages(this);
        return false;
    }
    UWorld* World = Fixture.GetTestWorld();
    UFFZSZoneAccessSubsystem* Access = World->GetSubsystem<UFFZSZoneAccessSubsystem>();
    if (!TestNotNull(TEXT("Game world provides Zone access"), Access))
    {
        return false;
    }

    // 없는 스폰 액터에 임의 좌표를 부여하면 엉뚱한 NavMesh 조각으로 존을 유도할 수 있다.
    FVector Resolved(11.0, 22.0, 33.0);
    TestFalse(TEXT("A missing crowd origin cannot resolve an exit"), Access->ResolveExitLocation(Resolved));
    TestTrue(TEXT("Failed lookup preserves the caller's output"), Resolved.Equals(FVector(11.0, 22.0, 33.0)));

    AFF_SpawnActor* Spawn = World->SpawnActor<AFF_SpawnActor>();
    ATargetPoint* Entrance = World->SpawnActor<ATargetPoint>();
    ATargetPoint* Exit = World->SpawnActor<ATargetPoint>();
    if (!TestNotNull(TEXT("Actual project crowd spawn actor exists"), Spawn)
        || !TestNotNull(TEXT("Entrance exists"), Entrance)
        || !TestNotNull(TEXT("Exit exists"), Exit))
    {
        return false;
    }

    // Native 스폰 클래스는 루트를 만들지 않는다. 저장된 BP처럼 위치를 가진 transient 액터를 만든다.
    USceneComponent* Root = NewObject<USceneComponent>(Spawn);
    Spawn->AddInstanceComponent(Root);
    Spawn->SetRootComponent(Root);
    Root->RegisterComponent();
    const FVector Origin(1200.0, -800.0, 250.0);
    Spawn->SetActorLocationAndRotation(Origin, FRotator(0.0, 90.0, 0.0));
    Spawn->SetActorScale3D(FVector(2.0));
    Entrance->SetActorLocation(FVector(7000.0, 8000.0, 900.0));
    Exit->SetActorLocation(FVector(-1500.0, 3400.0, 600.0));

    // 군중 파일은 수정하지 않는다. 실제 UPROPERTY 이름/형식의 변경도 이 통합 검사가 발견한다.
    FStructProperty* OffsetProperty = FindFProperty<FStructProperty>(Spawn->GetClass(), TEXT("Temp_ExitOffsetFromOrigin"));
    FObjectPropertyBase* EntranceProperty = FindFProperty<FObjectPropertyBase>(Spawn->GetClass(), TEXT("SpawnAchor"));
    FObjectPropertyBase* ExitProperty = FindFProperty<FObjectPropertyBase>(Spawn->GetClass(), TEXT("DespawnAchor"));
    if (!TestNotNull(TEXT("Crowd offset property exists"), OffsetProperty)
        || !TestNotNull(TEXT("Crowd entrance property exists"), EntranceProperty)
        || !TestNotNull(TEXT("Crowd exit property exists"), ExitProperty)
        || !TestTrue(TEXT("Crowd offset remains FVector"), OffsetProperty->Struct == TBaseStructure<FVector>::Get()))
    {
        return false;
    }

    FVector& AuthoredOffset = *OffsetProperty->ContainerPtrToValuePtr<FVector>(Spawn);
    TestFalse(TEXT("Exit is available before crowd actor BeginPlay"), Spawn->HasActorBegunPlay());
    TestTrue(TEXT("Default crowd offset resolves"), Access->ResolveExitLocation(Resolved));
    TestTrue(TEXT("Default is read from the actual instance"), Resolved.Equals(Origin + AuthoredOffset));

    AuthoredOffset = FVector(325.0, -675.0, 75.0);
    EntranceProperty->SetObjectPropertyValue_InContainer(Spawn, Entrance);
    TestTrue(TEXT("Authored crowd offset resolves with an entrance override"), Access->ResolveExitLocation(Resolved));
    TestTrue(TEXT("Fallback uses world addition; entrance, rotation and scale do not change the exit"),
        Resolved.Equals(Origin + AuthoredOffset));

    ExitProperty->SetObjectPropertyValue_InContainer(Spawn, Exit);
    TestTrue(TEXT("Explicit despawn anchor resolves"), Access->ResolveExitLocation(Resolved));
    TestTrue(TEXT("Despawn anchor takes priority over crowd offset and entrance"), Resolved.Equals(Exit->GetActorLocation()));
    const FVector MovedExit(-2200.0, 4100.0, 350.0);
    Exit->SetActorLocation(MovedExit);
    TestTrue(TEXT("Moved anchor is read without a cached location"), Access->ResolveExitLocation(Resolved));
    TestTrue(TEXT("Resolver follows the current anchor location"), Resolved.Equals(MovedExit));

    TestTrue(TEXT("The fixture destroys the exit anchor"), Exit->Destroy());
    TestFalse(TEXT("Destroyed anchors fail the crowd IsValid condition"), IsValid(Exit));
    TestTrue(TEXT("Destroyed anchor falls back to the authored offset"), Access->ResolveExitLocation(Resolved));
    TestTrue(TEXT("Fallback still belongs to the original spawn actor"), Resolved.Equals(Origin + AuthoredOffset));
    TestTrue(TEXT("The resolver does not rewrite the authored offset"), AuthoredOffset.Equals(FVector(325.0, -675.0, 75.0)));
    return true;
}

#endif
