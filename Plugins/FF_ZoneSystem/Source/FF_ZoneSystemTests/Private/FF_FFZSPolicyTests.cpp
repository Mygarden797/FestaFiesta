// FF_FFZSPolicyTests.cpp


// 외부 Policy의 사전 검증·임시 적용·Commit·Rollback 참여 계약을 검증한다.
// 테스트 행렬:
 // 성공 경로에서 모든 Policy가 PreValidate/Tentative를 거친 뒤 한 번씩 Commit되는가.
 // Tentative 실패를 낸 Policy도 부분 적용 가능성을 고려해 Rollback 대상에 포함되는가.
 // 실패 전에 적용을 시도한 Policy들이 모두 Rollback에 참여하는가.
 // Policy callback 중 Zone·보호영역·Phase·Policy registry 변경이 fail-closed되는가.

 // 1 cm Cell 계약에서도 기존 100 cm Cell 테스트와 같은 물리 위치를 사용하도록 QuarterTurns=0
 // 배치 원점은 기존 셀 좌표에 100을 곱한다. 좌표 해상도만 바꾸고 Policy 시나리오 간 거리는 유지한다.
 
 // Fixture는 지면·충돌·Nav 같은 외부 환경 변수를 잠시 끄고 종료 시 원래 CDO 값을 복구한다.
 // 따라서 이 파일은 트랜잭션 경계만 격리하며 Production Adapter 예제로 사용하지 않는다.

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
    if (!TestNotNull(TEXT("자동화 테스트 월드가 생성된다"), World))
    {
        return false;
    }

    UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>();
    if (!TestNotNull(TEXT("FF_ZoneSystem 서브시스템이 존재한다"), Subsystem) ||
        !TestNotNull(TEXT("게임플레이 구역이 존재한다"), AddTestZone(*World, *Subsystem)))
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

    TestTrue(TEXT("경제 정책이 등록된다"), Subsystem->RegisterExternalPolicy(Economy));
    TestTrue(TEXT("시뮬레이션 시간 정책이 등록된다"), Subsystem->RegisterExternalPolicy(SimulationTime));

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
    TestFalse(TEXT("임시 정책 거부로 명령이 실패한다"), RejectedResult.bSucceeded);
    TestEqual(
        TEXT("정책 거부 사유가 일관되게 반환된다"),
        RejectedResult.FailureReason,
        EFFZSPlacementFailure::ExternalPolicyRejected);
    TestEqual(TEXT("롤백 후 월드 리비전이 변경되지 않는다"), Subsystem->GetWorldRevision(), int64{0});
    TestEqual(TEXT("앞선 정책이 한 번 적용된다"), Economy->TentativeApplyCount, 1);
    TestEqual(TEXT("앞선 정책이 한 번 롤백된다"), Economy->RollbackCount, 1);
    TestEqual(TEXT("거부한 정책도 부분 적용 내용을 롤백한다"), SimulationTime->RollbackCount, 1);
    TestEqual(TEXT("실패 시 어떤 정책도 커밋되지 않는다"), Economy->CommitCount + SimulationTime->CommitCount, 0);

    // 두 번째 Act는 같은 Adapter를 허용 상태로 바꿔 Commit 경로가 정확히 한 번 실행되는지 분리 검증한다.
    SimulationTime->bAllowTentativeApply = true;
    // q0, 1 Cell=1 cm: 기존 (30,10)은 같은 물리 위치를 뜻하는 (3000,1000) Cell로 이관한다.
    FFFZSCommand Accepted = UFFZSGridLibrary::MakeInstallCommand(
        Spec,
        TEXT("GZ_DadaPocha"),
        FFFZSGridPlacement{FIntPoint(3000, 1000), 0},
        Subsystem->GetWorldRevision());

    const FFFZSCommandResult AcceptedResult = Subsystem->ExecuteCommand(Accepted);
    TestTrue(TEXT("모든 정책이 원자적 설치를 허용한다"), AcceptedResult.bSucceeded);
    TestNotNull(TEXT("성공한 명령이 시설을 반환한다"), AcceptedResult.FacilityActor.Get());
    TestEqual(TEXT("성공한 명령이 리비전을 하나 증가시킨다"), Subsystem->GetWorldRevision(), int64{1});
    TestEqual(TEXT("경제 정책이 정확히 한 번 커밋된다"), Economy->CommitCount, 1);
    TestEqual(TEXT("시뮬레이션 시간 정책이 정확히 한 번 커밋된다"), SimulationTime->CommitCount, 1);
    TestEqual(TEXT("성공 시 추가 롤백이 발생하지 않는다"), Economy->RollbackCount, 1);

    // Reentrancy probe: callback 안에서 registry/phase를 바꾸려 해도 안정된 Policy snapshot과 guard가 유지돼야 한다.
    AFFZSGameplayZone* CandidateZone = World->SpawnActor<AFFZSGameplayZone>();
    TestNotNull(TEXT("재진입 등록용 후보 구역이 존재한다"), CandidateZone);
    if (CandidateZone)
    {
        CandidateZone->ApplyAcceptedZonePreset(TEXT("GZ_Operations"));
    }
    AFFZSProtectedArea* RegisteredArea = World->SpawnActor<AFFZSProtectedArea>();
    AFFZSProtectedArea* CandidateArea = World->SpawnActor<AFFZSProtectedArea>();
    if (!TestNotNull(TEXT("등록된 보호 구역이 존재한다"), RegisteredArea) ||
        !TestNotNull(TEXT("후보 보호 구역이 존재한다"), CandidateArea))
    {
        return false;
    }
    RegisteredArea->ProtectedAreaId = TEXT("PA_PolicyRegistered");
    RegisteredArea->LinkedZoneId = TEXT("GZ_DadaPocha");
    RegisteredArea->SetActorLocation(FVector(200000.0, 200000.0, 0.0));
    CandidateArea->ProtectedAreaId = TEXT("PA_PolicyCandidate");
    CandidateArea->LinkedZoneId = TEXT("GZ_DadaPocha");
    CandidateArea->SetActorLocation(FVector(210000.0, 210000.0, 0.0));
    TestTrue(TEXT("기준 보호 구역이 등록된다"), Subsystem->RegisterProtectedArea(RegisteredArea));

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
    TestTrue(TEXT("재진입 변경 정책이 명령 전에 등록된다"), Subsystem->RegisterExternalPolicy(Mutator));
    TestTrue(TEXT("검증 대상 정책이 명령 전에 등록된다"), Subsystem->RegisterExternalPolicy(Victim));

    // q0, 1 Cell=1 cm: 기존 (50,10)은 같은 물리 위치를 뜻하는 (5000,1000) Cell로 이관한다.
    FFFZSCommand Guarded = UFFZSGridLibrary::MakeInstallCommand(
        Spec,
        TEXT("GZ_DadaPocha"),
        FFFZSGridPlacement{FIntPoint(5000, 1000), 0},
        Subsystem->GetWorldRevision());
    const FFFZSCommandResult GuardedResult = Subsystem->ExecuteCommand(Guarded);
    TestTrue(TEXT("차단된 재진입 변경이 명령을 무효화하지 않는다"), GuardedResult.bSucceeded);
    TestFalse(TEXT("정책 콜백은 구역을 등록할 수 없다"), Mutator->bRegisterZoneAttemptSucceeded);
    TestFalse(TEXT("정책 콜백은 구역 등록을 해제할 수 없다"), Mutator->bUnregisterZoneAttemptSucceeded);
    TestFalse(TEXT("정책 콜백은 보호 구역을 등록할 수 없다"), Mutator->bRegisterProtectedAreaAttemptSucceeded);
    TestFalse(TEXT("정책 콜백은 보호 구역 등록을 해제할 수 없다"), Mutator->bUnregisterProtectedAreaAttemptSucceeded);
    TestFalse(TEXT("정책 콜백은 워크플로 단계를 변경할 수 없다"), Mutator->bSetPhaseAttemptSucceeded);
    TestFalse(TEXT("정책 콜백은 다른 정책을 등록할 수 없다"), Mutator->bRegisterPolicyAttemptSucceeded);
    TestFalse(TEXT("정책 콜백은 다른 정책의 등록을 해제할 수 없다"), Mutator->bUnregisterPolicyAttemptSucceeded);
    TestEqual(TEXT("워크플로 단계가 변경되지 않는다"), Subsystem->GetChangePhase(), EFFZSChangePhase::InitialSetup);
    TestNotNull(TEXT("등록된 구역이 권위 레지스트리에 유지된다"), Subsystem->FindZone(TEXT("GZ_DadaPocha")));
    TestNull(TEXT("후보 구역이 삽입되지 않는다"), Subsystem->FindZone(TEXT("GZ_Operations")));
    TestTrue(TEXT("등록된 보호 구역이 계속 등록되어 있다"), Subsystem->UnregisterProtectedArea(RegisteredArea));
    TestFalse(TEXT("후보 보호 구역이 삽입되지 않는다"), Subsystem->UnregisterProtectedArea(CandidateArea));
    TestTrue(TEXT("검증 대상 정책이 계속 등록되어 있다"), Subsystem->UnregisterExternalPolicy(Victim));
    TestFalse(TEXT("후보 정책이 삽입되지 않는다"), Subsystem->UnregisterExternalPolicy(CandidatePolicy));

    return true;
}

#endif
