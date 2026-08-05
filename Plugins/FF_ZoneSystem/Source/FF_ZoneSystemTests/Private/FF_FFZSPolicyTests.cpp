/**
 * @file FF_FFZSPolicyTests.cpp
 * @brief 외부 Policy의 사전 검증·임시 적용·Commit·Rollback 참여 계약을 검증한다.
 *
 * 테스트 행렬:
 * - 성공 경로에서 모든 Policy가 PreValidate/Tentative를 거친 뒤 한 번씩 Commit되는가.
 * - Tentative 실패를 낸 Policy도 부분 적용 가능성을 고려해 Rollback 대상에 포함되는가.
 * - 실패 전에 적용을 시도한 Policy들이 모두 Rollback에 참여하는가.
 * - Policy callback 중 Zone·보호영역·Phase·Policy registry 변경이 fail-closed되는가.
 *
 * 1 cm Cell 계약에서도 기존 100 cm Cell 테스트와 같은 물리 위치를 사용하도록 QuarterTurns=0
 * 배치 원점은 기존 셀 좌표에 100을 곱한다. 좌표 해상도만 바꾸고 Policy 시나리오 간 거리는 유지한다.
 *
 * Fixture는 지면·충돌·Nav 같은 외부 환경 변수를 잠시 끄고 종료 시 원래 CDO 값을 복구한다.
 * 따라서 이 파일은 트랜잭션 경계만 격리하며 Production Adapter 예제로 사용하지 않는다.
 */

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSProtectedArea.h"
#include "FF_FFZSTestExternalPolicy.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Tests/AutomationEditorCommon.h"

namespace
{
    // Arrange 격리: 테스트가 바꾼 CDO flag를 scope 종료 시 반드시 원복해 다른 테스트 순서에 의존하지 않는다.
    struct FScopedPolicyTestSettings
    {
        UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
        bool bGround = Settings->bEnableGroundChecks;
        bool bCollision = Settings->bEnableWorldCollisionChecks;
        bool bNavigation = Settings->bEnableNavigationProjectionChecks;
        bool bProductionPolicies = Settings->bRequireProductionPolicyDomains;

        FScopedPolicyTestSettings()
        {
            Settings->bEnableGroundChecks = false;
            Settings->bEnableWorldCollisionChecks = false;
            Settings->bEnableNavigationProjectionChecks = false;
            Settings->bRequireProductionPolicyDomains = false;
        }

        ~FScopedPolicyTestSettings()
        {
            Settings->bEnableGroundChecks = bGround;
            Settings->bEnableWorldCollisionChecks = bCollision;
            Settings->bEnableNavigationProjectionChecks = bNavigation;
            Settings->bRequireProductionPolicyDomains = bProductionPolicies;
        }
    };

    AFFZSGameplayZone* AddTestZone(UWorld& World, UFFZSWorldSubsystem& Subsystem)
    {
        AFFZSGameplayZone* Zone = World.SpawnActor<AFFZSGameplayZone>();
        if (Zone)
        {
            Zone->ZoneId = TEXT("GZ_DadaPocha");
            Zone->DisplayName = FText::FromString(TEXT("Policy Test Zone"));
            Subsystem.RegisterZone(Zone);
        }
        return Zone;
    }
}

// 등록명은 기존 필터 호환을 위해 ReverseRollback을 유지한다. 현재 Assert는 모든 적용 Policy의
// Rollback 참여를 확인하며, 정확한 호출 순서를 바꿀 때는 별도 order probe를 추가해야 한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSExternalPolicyTransactionTest,
    "FF_ZoneSystem.Integration.Policy.AtomicCommitAndReverseRollback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSExternalPolicyTransactionTest::RunTest(const FString& Parameters)
{
    FScopedPolicyTestSettings SettingsGuard;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("Automation world"), World))
    {
        return false;
    }

    UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>();
    if (!TestNotNull(TEXT("FF_ZoneSystem subsystem"), Subsystem) ||
        !TestNotNull(TEXT("Gameplay Zone"), AddTestZone(*World, *Subsystem)))
    {
        return false;
    }
    Subsystem->SetChangePhase(EFFZSChangePhase::InitialSetup);

    // Arrange: 앞 Policy는 성공하고 뒤 Policy는 Tentative에서 실패하도록 구성해 두 Policy의 보상 참여를 관찰한다.
    UFFZSTestExternalPolicy* Economy =
        NewObject<UFFZSTestExternalPolicy>(GetTransientPackage());
    Economy->PolicyName = TEXT("Policy.Economy");
    Economy->PolicyDomain = EFFZSPolicyDomain::Economy;

    UFFZSTestExternalPolicy* SimulationTime =
        NewObject<UFFZSTestExternalPolicy>(GetTransientPackage());
    SimulationTime->PolicyName = TEXT("Policy.SimulationTime");
    SimulationTime->PolicyDomain = EFFZSPolicyDomain::SimulationTime;
    SimulationTime->bAllowTentativeApply = false;

    TestTrue(TEXT("Economy policy registers"), Subsystem->RegisterExternalPolicy(Economy));
    TestTrue(TEXT("Simulation-time policy registers"), Subsystem->RegisterExternalPolicy(SimulationTime));

    const FFFZSFacilitySpec Spec =
        UFFZSGridLibrary::MakeBuiltInFacilitySpec(EFFZSBuiltInFacilityPreset::FoodTruckBasic);
    // q0, 1 Cell=1 cm: 기존 (10,10) 100 cm Cell 위치인 월드 (10 m,10 m)를 (1000,1000)으로 보존한다.
    FFFZSCommand Rejected = UFFZSGridLibrary::MakeInstallCommand(
        Spec,
        TEXT("GZ_DadaPocha"),
        FFFZSGridPlacement{FIntPoint(1000, 1000), 0},
        Subsystem->GetWorldRevision());

    // Act/Assert: 실패해도 Revision과 Facility는 확정되지 않고, 실패 Policy까지 Rollback되어야 한다.
    const FFFZSCommandResult RejectedResult = Subsystem->ExecuteCommand(Rejected);
    TestFalse(TEXT("A tentative policy rejection fails the command"), RejectedResult.bSucceeded);
    TestEqual(
        TEXT("Policy rejection has a stable reason"),
        RejectedResult.FailureReason,
        EFFZSPlacementFailure::ExternalPolicyRejected);
    TestEqual(TEXT("World revision is unchanged after rollback"), Subsystem->GetWorldRevision(), int64{0});
    TestEqual(TEXT("Earlier policy applied once"), Economy->TentativeApplyCount, 1);
    TestEqual(TEXT("Earlier policy rolled back once"), Economy->RollbackCount, 1);
    TestEqual(TEXT("Rejecting policy also rolls back partial work"), SimulationTime->RollbackCount, 1);
    TestEqual(TEXT("No policy commits on failure"), Economy->CommitCount + SimulationTime->CommitCount, 0);

    // 두 번째 Act는 같은 Adapter를 허용 상태로 바꿔 Commit 경로가 정확히 한 번 실행되는지 분리 검증한다.
    SimulationTime->bAllowTentativeApply = true;
    // q0, 1 Cell=1 cm: 기존 (30,10)은 같은 물리 위치를 뜻하는 (3000,1000) Cell로 이관한다.
    FFFZSCommand Accepted = UFFZSGridLibrary::MakeInstallCommand(
        Spec,
        TEXT("GZ_DadaPocha"),
        FFFZSGridPlacement{FIntPoint(3000, 1000), 0},
        Subsystem->GetWorldRevision());

    const FFFZSCommandResult AcceptedResult = Subsystem->ExecuteCommand(Accepted);
    TestTrue(TEXT("All policies allow an atomic install"), AcceptedResult.bSucceeded);
    TestNotNull(TEXT("Successful command returns the Facility"), AcceptedResult.FacilityActor.Get());
    TestEqual(TEXT("Successful command advances one revision"), Subsystem->GetWorldRevision(), int64{1});
    TestEqual(TEXT("Economy commits exactly once"), Economy->CommitCount, 1);
    TestEqual(TEXT("Simulation-time commits exactly once"), SimulationTime->CommitCount, 1);
    TestEqual(TEXT("No new rollback occurs on success"), Economy->RollbackCount, 1);

    // Reentrancy probe: callback 안에서 registry/phase를 바꾸려 해도 안정된 Policy snapshot과 guard가 유지돼야 한다.
    AFFZSGameplayZone* CandidateZone = World->SpawnActor<AFFZSGameplayZone>();
    TestNotNull(TEXT("Candidate Zone for reentrant registration"), CandidateZone);
    if (CandidateZone)
    {
        CandidateZone->ApplyAcceptedZonePreset(TEXT("GZ_Operations"));
    }
    AFFZSProtectedArea* RegisteredArea = World->SpawnActor<AFFZSProtectedArea>();
    AFFZSProtectedArea* CandidateArea = World->SpawnActor<AFFZSProtectedArea>();
    if (!TestNotNull(TEXT("Registered protected area"), RegisteredArea) ||
        !TestNotNull(TEXT("Candidate protected area"), CandidateArea))
    {
        return false;
    }
    RegisteredArea->ProtectedAreaId = TEXT("PA_PolicyRegistered");
    RegisteredArea->LinkedZoneId = TEXT("GZ_DadaPocha");
    RegisteredArea->SetActorLocation(FVector(200000.0, 200000.0, 0.0));
    CandidateArea->ProtectedAreaId = TEXT("PA_PolicyCandidate");
    CandidateArea->LinkedZoneId = TEXT("GZ_DadaPocha");
    CandidateArea->SetActorLocation(FVector(210000.0, 210000.0, 0.0));
    TestTrue(TEXT("Baseline protected area registers"), Subsystem->RegisterProtectedArea(RegisteredArea));

    UFFZSTestExternalPolicy* Mutator = NewObject<UFFZSTestExternalPolicy>(GetTransientPackage());
    UFFZSTestExternalPolicy* Victim = NewObject<UFFZSTestExternalPolicy>(GetTransientPackage());
    UFFZSTestExternalPolicy* CandidatePolicy = NewObject<UFFZSTestExternalPolicy>(GetTransientPackage());
    Mutator->PolicyName = TEXT("Policy.Custom.AAA_Mutator");
    Victim->PolicyName = TEXT("Policy.Custom.ZZZ_Victim");
    CandidatePolicy->PolicyName = TEXT("Policy.Custom.Candidate");
    Mutator->bAttemptAuthorityMutationsOnPreValidate = true;
    Mutator->MutationSubsystem = Subsystem;
    Mutator->ZoneToRegister = CandidateZone;
    Mutator->ZoneToUnregister = Subsystem->FindZone(TEXT("GZ_DadaPocha"));
    Mutator->ProtectedAreaToRegister = CandidateArea;
    Mutator->ProtectedAreaToUnregister = RegisteredArea;
    Mutator->PolicyToRegister = CandidatePolicy;
    Mutator->PolicyToUnregister = Victim;
    TestTrue(TEXT("Reentrant mutator policy registers before the command"), Subsystem->RegisterExternalPolicy(Mutator));
    TestTrue(TEXT("Victim policy registers before the command"), Subsystem->RegisterExternalPolicy(Victim));

    // q0, 1 Cell=1 cm: 기존 (50,10)은 같은 물리 위치를 뜻하는 (5000,1000) Cell로 이관한다.
    FFFZSCommand Guarded = UFFZSGridLibrary::MakeInstallCommand(
        Spec,
        TEXT("GZ_DadaPocha"),
        FFFZSGridPlacement{FIntPoint(5000, 1000), 0},
        Subsystem->GetWorldRevision());
    const FFFZSCommandResult GuardedResult = Subsystem->ExecuteCommand(Guarded);
    TestTrue(TEXT("Blocked reentrant mutations do not invalidate the command"), GuardedResult.bSucceeded);
    TestFalse(TEXT("Policy callback cannot register a Zone"), Mutator->bRegisterZoneAttemptSucceeded);
    TestFalse(TEXT("Policy callback cannot unregister a Zone"), Mutator->bUnregisterZoneAttemptSucceeded);
    TestFalse(TEXT("Policy callback cannot register a protected area"), Mutator->bRegisterProtectedAreaAttemptSucceeded);
    TestFalse(TEXT("Policy callback cannot unregister a protected area"), Mutator->bUnregisterProtectedAreaAttemptSucceeded);
    TestFalse(TEXT("Policy callback cannot change the workflow phase"), Mutator->bSetPhaseAttemptSucceeded);
    TestFalse(TEXT("Policy callback cannot register another policy"), Mutator->bRegisterPolicyAttemptSucceeded);
    TestFalse(TEXT("Policy callback cannot unregister another policy"), Mutator->bUnregisterPolicyAttemptSucceeded);
    TestEqual(TEXT("Workflow phase remains unchanged"), Subsystem->GetChangePhase(), EFFZSChangePhase::InitialSetup);
    TestNotNull(TEXT("Registered Zone remains in the authoritative registry"), Subsystem->FindZone(TEXT("GZ_DadaPocha")));
    TestNull(TEXT("Candidate Zone was not injected"), Subsystem->FindZone(TEXT("GZ_Operations")));
    TestTrue(TEXT("Registered protected area remains registered"), Subsystem->UnregisterProtectedArea(RegisteredArea));
    TestFalse(TEXT("Candidate protected area was not injected"), Subsystem->UnregisterProtectedArea(CandidateArea));
    TestTrue(TEXT("Victim policy remains registered"), Subsystem->UnregisterExternalPolicy(Victim));
    TestFalse(TEXT("Candidate policy was not injected"), Subsystem->UnregisterExternalPolicy(CandidatePolicy));

    return true;
}

#endif
