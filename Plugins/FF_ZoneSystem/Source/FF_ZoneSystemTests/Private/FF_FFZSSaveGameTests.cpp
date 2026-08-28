//  FF_FFZSSaveGameTests.cpp

// Schema 5 Snapshot의 자유 Yaw와 1cm 압축 Region 값 그래프가 실제 SaveGame 바이트 왕복에서 손실되지 않는지 검증한다.
// Spec, Transform, FText, SoftObjectPath, Queue의 Waiting/Service/Exit, Receipt와 CurrentPhase를
// 서로 구별되는 sentinel 값으로 채운 뒤 SaveGameToMemory/LoadGameFromMemory 결과를 비교한다.
// 이 테스트의 SchemaVersion 97은 Runtime Restore 허용 버전이 아니라 직렬화 필드가 그대로
// 보존되는지 찾기 쉬운 값으로 확인하기 위한 sentinel이다.


#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "FF_FFZSSaveGameTestObject.h"
#include "Engine/StaticMesh.h"
#include "FF_FFZSGridLibrary.h"
#include "Kismet/GameplayStatics.h"

namespace
{
    FFFZSWorldSnapshot MakeDistinctSnapshot()
    {
        FFFZSWorldSnapshot Snapshot;
        // Runtime 허용 Schema가 아니라, archive가 임의 정수 값을 손실/치환하지 않는지 확인하는 sentinel이다.
        Snapshot.SchemaVersion = 97;
        Snapshot.WorldRevision = 918273645;
        Snapshot.CurrentPhase = EFFZSChangePhase::EveningSettlement;

        FFFZSFacilityInstanceSnapshot Facility;
        Facility.FacilityInstanceId = FGuid(0x01020304, 0x11121314, 0x21222324, 0x31323334);
        Facility.ZoneId = TEXT("GZ_SaveGameRoundTrip");
        Facility.Placement.OriginCell = FIntPoint(42, -17);
        Facility.Placement.QuarterTurns = 3;
        Facility.State = EFFZSFacilityState::MovePending;
        Facility.WorldTransform = FTransform(
            FRotator(10.0, 20.0, 30.0),
            FVector(1234.5, -678.25, 91.75),
            FVector(1.25, 0.75, 1.5));
        Facility.bNeedsRelocation = true;

        FFFZSFacilitySpec& Spec = Facility.FacilitySpec;
        Spec.SchemaVersion = 7;
        Spec.FacilityDefinitionId = TEXT("Facility.SaveGameProbe");
        Spec.DisplayName = FText::FromString(TEXT("SaveGame Probe Facility"));
        Spec.Category = EFFZSFacilityCategory::Safety;
        Spec.FootprintSizeCells = FIntPoint(6, 3);
        Spec.ClearanceCells = 2;
        // 직렬화 sentinel은 Runtime 유효성용이 아니므로 압축 Rect와 구 소형 Cell 필드를 모두 다른 값으로 채운다.
        Spec.QueueReservedRects = {FFFZSCellRect(FIntPoint(600, -20), FIntPoint(900, 80))};
        Spec.QueueReservedCells = {FIntPoint(6, 0), FIntPoint(7, 0), FIntPoint(8, 0)};
        Spec.QueueSplinePointsCm = {
            FVector(600.0, 50.0, 0.0),
            FVector(715.0, 75.0, 5.0),
            FVector(830.0, 100.0, 10.0)};
        Spec.QueueRules.MaxWaitingAgents = 23;
        Spec.QueueRules.SlotSpacingCm = 132.5f;
        Spec.QueueRules.ServiceCapacity = 4;
        Spec.QueueRules.ServiceDurationSeconds = 7.75f;
        Spec.QueueRules.AbandonAfterSeconds = 48.5f;
        Spec.QueueRules.SlotArrivalToleranceCm = 31.25f;
        Spec.QueueRules.NavigationProjectionExtentCm = 44.5f;
        Spec.QueueRules.BlockedTimeoutSeconds = 9.25f;
        Spec.QueueRules.MaxRepathAttempts = 5;

        FFFZSAnchorDefinition Anchor;
        Anchor.Type = EFFZSAnchorType::Exit;
        Anchor.Index = 4;
        Anchor.LocalTransform = FTransform(
            FRotator(0.0, 135.0, 0.0),
            FVector(450.0, 125.0, 15.0),
            FVector(0.9, 1.1, 1.0));
        Spec.Anchors.Add(Anchor);
        Spec.AllowedZoneIds = {TEXT("GZ_SaveGameRoundTrip"), TEXT("GZ_Overflow")};
        Spec.InstallCostKey = TEXT("Cost.SaveProbe");
        Spec.RevenueRuleKey = TEXT("Revenue.SaveProbe");
        Spec.ExperienceEffectKey = TEXT("Experience.SaveProbe");
        Spec.VisualMesh = TSoftObjectPtr<UStaticMesh>(
            FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
        Spec.PlacementObstacleHeightCm = 377.5f;

        Facility.Queue.State = EFFZSQueueState::Blocked;
        Facility.Queue.StateBeforeBlocked = EFFZSQueueState::Draining;
        Facility.Queue.NextJoinSequence = 123;
        Facility.Queue.CompletedCount = 9;
        Facility.Queue.AbandonedCount = 4;
        Facility.Queue.TotalCompletedWaitSeconds = 321.25f;
        Facility.Queue.BlockedElapsedSeconds = 2.75f;
        Facility.Queue.RepathAttemptCount = 2;
        Facility.Queue.bDrainRequested = true;

        FFFZSQueueEntrySnapshot FirstEntry;
        FirstEntry.TicketId = FGuid(0x41424344, 0x51525354, 0x61626364, 0x71727374);
        FirstEntry.AgentId = TEXT("Agent.SaveProbe.A");
        FirstEntry.WaitSeconds = 18.5f;
        FirstEntry.ServiceElapsedSeconds = 3.25f;
        FirstEntry.JoinSequence = 77;
        FirstEntry.bInService = true;
        FirstEntry.bArrivedAtAssignedSlot = true;
        FirstEntry.bArrivedAtServicePoint = true;
        FirstEntry.bExiting = true;
        FirstEntry.MovementRepathCount = 3;
        FirstEntry.AssignedQueueIndex = 8;
        FirstEntry.ServiceIndex = 2;
        FirstEntry.ExitIndex = 1;
        FirstEntry.PendingReleaseReason = EFFZSQueueReleaseReason::Abandoned;
        Facility.Queue.Entries.Add(FirstEntry);

        FFFZSQueueEntrySnapshot SecondEntry;
        SecondEntry.TicketId = FGuid(0x81828384, 0x91929394, 0xA1A2A3A4, 0xB1B2B3B4);
        SecondEntry.AgentId = TEXT("Agent.SaveProbe.B");
        SecondEntry.WaitSeconds = 6.0f;
        SecondEntry.JoinSequence = 78;
        SecondEntry.AssignedQueueIndex = 9;
        Facility.Queue.Entries.Add(SecondEntry);

        Snapshot.Facilities.Add(Facility);

        FFFZSCommandReceiptSnapshot Receipt;
        Receipt.CommandId = FGuid(0xC1C2C3C4, 0xD1D2D3D4, 0xE1E2E3E4, 0xF1F2F3F4);
        Receipt.PayloadFingerprint = TEXT("sha256:save-game-round-trip-probe");
        Receipt.bSucceeded = true;
        Receipt.FailureReason = EFFZSPlacementFailure::ExternalPolicyRejected;
        Receipt.Message = FText::FromString(TEXT("Receipt payload survived"));
        Receipt.ResultWorldRevision = 918273644;
        Receipt.FacilityInstanceId = Facility.FacilityInstanceId;
        Receipt.bResultReferencesFacility = true;
        Snapshot.CommandReceipts.Add(Receipt);

        return Snapshot;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSSaveGameFreeYawByteRoundTripTest,
    "FF_ZoneSystem.Unit.SaveGame.Schema5FreeYawByteRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSSaveGameFreeYawByteRoundTripTest::RunTest(const FString& Parameters)
{
    FFFZSWorldSnapshot Source;
    Source.SchemaVersion = 5;
    Source.WorldRevision = 37;
    Source.CurrentPhase = EFFZSChangePhase::MorningCheck;

    constexpr int32 YawCases[] = { 3725, 35999 };
    for (int32 CaseIndex = 0; CaseIndex < UE_ARRAY_COUNT(YawCases); ++CaseIndex)
    {
        FFFZSFacilityInstanceSnapshot Facility;
        Facility.FacilityInstanceId = FGuid(0x10000000 + CaseIndex, 0x20000000, 0x30000000, 0x40000000);
        Facility.ZoneId = TEXT("GZ_SaveGameRoundTrip");
        Facility.Placement.OriginCell = FIntPoint(100 + CaseIndex * 100, -50 - CaseIndex * 10);
        Facility.Placement.YawCentidegrees = YawCases[CaseIndex];
        Facility.Placement.QuarterTurns = 0;
        Facility.WorldTransform = FTransform(
            FRotator(0.0, static_cast<double>(YawCases[CaseIndex]) / 100.0, 0.0),
            FVector(123.25 + CaseIndex * 500.0, -456.75 - CaseIndex * 250.0, 18.5 + CaseIndex),
            FVector::OneVector);
        Source.Facilities.Add(Facility);
    }

    UFFZSSaveGameTestObject* SaveObject = NewObject<UFFZSSaveGameTestObject>(GetTransientPackage());
    if (!TestNotNull(TEXT("Schema 5 자유 Yaw 세이브 래퍼가 생성된다"), SaveObject))
    {
        return false;
    }
    SaveObject->Snapshot = Source;

    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Schema 5 자유 Yaw 스냅샷이 바이트로 직렬화된다"),
            UGameplayStatics::SaveGameToMemory(SaveObject, Bytes)) ||
        !TestTrue(TEXT("Schema 5 자유 Yaw 세이브 바이트가 비어 있지 않다"), !Bytes.IsEmpty()))
    {
        return false;
    }

    UFFZSSaveGameTestObject* LoadedObject = Cast<UFFZSSaveGameTestObject>(
        UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!TestNotNull(TEXT("Schema 5 자유 Yaw 세이브가 같은 타입으로 복원된다"), LoadedObject))
    {
        return false;
    }

    const FFFZSWorldSnapshot& Loaded = LoadedObject->Snapshot;
    TestEqual(TEXT("자유 Yaw 바이트 왕복은 Schema 5를 보존한다"), Loaded.SchemaVersion, 5);
    TestEqual(TEXT("자유 Yaw 바이트 왕복은 두 시설을 보존한다"), Loaded.Facilities.Num(), 2);
    TestTrue(TEXT("자유 Yaw 바이트 왕복은 전체 스냅샷 값을 보존한다"),
        FFFZSWorldSnapshot::StaticStruct()->CompareScriptStruct(&Loaded, &Source, 0));
    if (Loaded.Facilities.Num() != UE_ARRAY_COUNT(YawCases))
    {
        return false;
    }

    for (int32 CaseIndex = 0; CaseIndex < UE_ARRAY_COUNT(YawCases); ++CaseIndex)
    {
        const FString CaseLabel = FString::Printf(TEXT("Yaw %.2f도"), YawCases[CaseIndex] / 100.0);
        const FFFZSFacilityInstanceSnapshot& LoadedFacility = Loaded.Facilities[CaseIndex];
        TestEqual(*FString::Printf(TEXT("%s: centidegree 값이 보존된다"), *CaseLabel),
            LoadedFacility.Placement.YawCentidegrees, YawCases[CaseIndex]);
        TestEqual(*FString::Printf(TEXT("%s: legacy QuarterTurns는 0을 유지한다"), *CaseLabel),
            LoadedFacility.Placement.QuarterTurns, 0);
        TestTrue(*FString::Printf(TEXT("%s: WorldTransform이 보존된다"), *CaseLabel),
            LoadedFacility.WorldTransform.Equals(Source.Facilities[CaseIndex].WorldTransform, 1.0e-6));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSCommandFingerprintFreeYawCanonicalizationTest,
    "FF_ZoneSystem.Unit.SaveGame.CommandFingerprintFreeYawCanonicalization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSCommandFingerprintFreeYawCanonicalizationTest::RunTest(const FString& Parameters)
{
    FFFZSCommand Baseline;
    Baseline.TransactionId = FGuid(0x11111111, 0x22222222, 0x33333333, 0x44444444);
    Baseline.ExpectedWorldRevision = 17;
    Baseline.Type = EFFZSCommandType::Install;
    Baseline.FacilityInstanceId = FGuid(0xAAAAAAAA, 0xBBBBBBBB, 0xCCCCCCCC, 0xDDDDDDDD);
    Baseline.ZoneId = TEXT("GZ_FingerprintYaw");
    Baseline.Placement.OriginCell = FIntPoint(125, -275);
    Baseline.Placement.YawCentidegrees = 3725;

    FFFZSCommand DifferentYaw = Baseline;
    DifferentYaw.Placement.YawCentidegrees = 3726;
    TestNotEqual(TEXT("0.01도 차이는 서로 다른 command fingerprint를 만든다"),
        UFFZSGridLibrary::BuildCommandFingerprint(Baseline),
        UFFZSGridLibrary::BuildCommandFingerprint(DifferentYaw));

    FFFZSCommand WrappedYaw = Baseline;
    WrappedYaw.Placement.YawCentidegrees = 3725 + 2 * 36000;
    TestEqual(TEXT("완전한 두 바퀴를 더한 Yaw는 같은 command fingerprint를 만든다"),
        UFFZSGridLibrary::BuildCommandFingerprint(WrappedYaw),
        UFFZSGridLibrary::BuildCommandFingerprint(Baseline));

    FFFZSCommand NegativeYaw = Baseline;
    NegativeYaw.Placement.YawCentidegrees = -1;
    FFFZSCommand PositiveEquivalentYaw = Baseline;
    PositiveEquivalentYaw.Placement.YawCentidegrees = 35999;
    TestEqual(TEXT("-0.01도와 359.99도는 같은 command fingerprint를 만든다"),
        UFFZSGridLibrary::BuildCommandFingerprint(NegativeYaw),
        UFFZSGridLibrary::BuildCommandFingerprint(PositiveEquivalentYaw));

    FFFZSCommand LegacyQuarterTurn = Baseline;
    LegacyQuarterTurn.Placement.YawCentidegrees = 0;
    LegacyQuarterTurn.Placement.QuarterTurns = 1;
    FFFZSCommand ModernQuarterTurn = Baseline;
    ModernQuarterTurn.Placement.YawCentidegrees = 9000;
    ModernQuarterTurn.Placement.QuarterTurns = 0;
    TestEqual(TEXT("legacy q1과 현대 90도 Yaw는 같은 command fingerprint를 만든다"),
        UFFZSGridLibrary::BuildCommandFingerprint(LegacyQuarterTurn),
        UFFZSGridLibrary::BuildCommandFingerprint(ModernQuarterTurn));

    FFFZSGridPlacement NormalizedPlacement = WrappedYaw.Placement;
    UFFZSGridLibrary::NormalizePlacement(NormalizedPlacement);
    TestEqual(TEXT("NormalizePlacement는 Yaw를 0..35999로 정규화한다"),
        NormalizedPlacement.YawCentidegrees, 3725);
    TestEqual(TEXT("NormalizePlacement는 legacy QuarterTurns를 비운다"),
        NormalizedPlacement.QuarterTurns, 0);

    FFFZSGridPlacement NonNormalizedCommandPlacement;
    NonNormalizedCommandPlacement.OriginCell = FIntPoint(5, 7);
    NonNormalizedCommandPlacement.YawCentidegrees = -1;
    NonNormalizedCommandPlacement.QuarterTurns = 0;
    const FFFZSCommand NormalizedCommand = UFFZSGridLibrary::MakeInstallCommand(
        FFFZSFacilitySpec{}, TEXT("GZ_FingerprintYaw"), NonNormalizedCommandPlacement, 0);
    TestEqual(TEXT("명령 생성 경계는 음수 Yaw를 의미 동치 정규각으로 바꾼다"),
        NormalizedCommand.Placement.YawCentidegrees, 35999);
    TestEqual(TEXT("명령 생성 경계는 legacy QuarterTurns를 비운다"),
        NormalizedCommand.Placement.QuarterTurns, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSSaveGameByteRoundTripTest,
    "FF_ZoneSystem.Unit.SaveGame.WorldSnapshotByteRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSSaveGameByteRoundTripTest::RunTest(const FString& Parameters)
{
    UFFZSSaveGameTestObject* SaveObject = NewObject<UFFZSSaveGameTestObject>(GetTransientPackage());
    if (!TestNotNull(TEXT("세이브 게임 래퍼가 생성된다"), SaveObject))
    {
        return false;
    }

    // Arrange/Act: reflected 비교만 하지 않고 실제 Unreal SaveGame archive를 byte 배열까지 통과시킨다.
    SaveObject->Snapshot = MakeDistinctSnapshot();
    TArray<uint8> Bytes;
    if (!TestTrue(
            TEXT("UGameplayStatics가 세이브 게임을 바이트로 직렬화한다"),
            UGameplayStatics::SaveGameToMemory(SaveObject, Bytes)) ||
        !TestTrue(TEXT("직렬화된 세이브 게임에 바이트 데이터가 있다"), !Bytes.IsEmpty()))
    {
        return false;
    }

    USaveGame* LoadedBase = UGameplayStatics::LoadGameFromMemory(Bytes);
    UFFZSSaveGameTestObject* LoadedObject = Cast<UFFZSSaveGameTestObject>(LoadedBase);
    if (!TestNotNull(TEXT("UGameplayStatics가 지정된 세이브 게임 타입을 복원한다"), LoadedObject))
    {
        return false;
    }

    // Assert 1: 전체 reflected graph를 비교한 뒤, 장애 위치를 알 수 있도록 중요 계약을 필드별로 다시 확인한다.
    const FFFZSWorldSnapshot& Source = SaveObject->Snapshot;
    const FFFZSWorldSnapshot& Loaded = LoadedObject->Snapshot;
    TestTrue(
        TEXT("리플렉션된 전체 월드 스냅샷이 바이트 왕복 후에도 보존된다"),
        FFFZSWorldSnapshot::StaticStruct()->CompareScriptStruct(&Loaded, &Source, 0));

    TestEqual(TEXT("월드 스키마 버전이 보존된다"), Loaded.SchemaVersion, Source.SchemaVersion);
    TestEqual(TEXT("월드 리비전이 보존된다"), Loaded.WorldRevision, Source.WorldRevision);
    TestEqual(TEXT("변경 단계가 보존된다"), Loaded.CurrentPhase, Source.CurrentPhase);
    TestEqual(TEXT("시설 배열이 보존된다"), Loaded.Facilities.Num(), Source.Facilities.Num());
    TestEqual(TEXT("영수증 배열이 보존된다"), Loaded.CommandReceipts.Num(), Source.CommandReceipts.Num());
    if (Loaded.Facilities.Num() != 1 || Loaded.CommandReceipts.Num() != 1)
    {
        return false;
    }

    const FFFZSFacilityInstanceSnapshot& SourceFacility = Source.Facilities[0];
    const FFFZSFacilityInstanceSnapshot& LoadedFacility = Loaded.Facilities[0];
    TestTrue(
        TEXT("전체 시설 명세가 보존된다"),
        FFFZSFacilitySpec::StaticStruct()->CompareScriptStruct(
            &LoadedFacility.FacilitySpec,
            &SourceFacility.FacilitySpec,
            0));
    TestTrue(TEXT("시설 ID가 보존된다"),
        LoadedFacility.FacilityInstanceId == SourceFacility.FacilityInstanceId);
    TestTrue(TEXT("격자 배치가 보존된다"), LoadedFacility.Placement == SourceFacility.Placement);
    TestEqual(TEXT("시설 상태가 보존된다"), LoadedFacility.State, SourceFacility.State);
    TestTrue(TEXT("시설 트랜스폼이 보존된다"),
        LoadedFacility.WorldTransform.Equals(SourceFacility.WorldTransform));
    TestEqual(TEXT("재배치 플래그가 보존된다"),
        LoadedFacility.bNeedsRelocation, SourceFacility.bNeedsRelocation);

    const FFFZSFacilitySpec& SourceSpec = SourceFacility.FacilitySpec;
    const FFFZSFacilitySpec& LoadedSpec = LoadedFacility.FacilitySpec;
    TestEqual(TEXT("시설 명세 스키마가 보존된다"), LoadedSpec.SchemaVersion, SourceSpec.SchemaVersion);
    TestEqual(TEXT("시설 정의 ID가 보존된다"),
        LoadedSpec.FacilityDefinitionId, SourceSpec.FacilityDefinitionId);
    TestEqual(TEXT("시설 표시 이름이 보존된다"),
        LoadedSpec.DisplayName.ToString(), SourceSpec.DisplayName.ToString());
    TestTrue(TEXT("압축된 대기열 예약 사각형이 보존된다"),
        LoadedSpec.QueueReservedRects == SourceSpec.QueueReservedRects);
    TestTrue(TEXT("레거시 소규모 대기열 예약 셀이 보존된다"),
        LoadedSpec.QueueReservedCells == SourceSpec.QueueReservedCells);
    TestTrue(TEXT("대기열 스플라인 지점이 보존된다"),
        LoadedSpec.QueueSplinePointsCm == SourceSpec.QueueSplinePointsCm);
    TestEqual(TEXT("대기열 규칙의 슬롯 간격이 보존된다"),
        LoadedSpec.QueueRules.SlotSpacingCm, SourceSpec.QueueRules.SlotSpacingCm);
    TestEqual(TEXT("대기열 규칙의 서비스 용량이 보존된다"),
        LoadedSpec.QueueRules.ServiceCapacity, SourceSpec.QueueRules.ServiceCapacity);
    TestEqual(TEXT("앵커 배열이 보존된다"), LoadedSpec.Anchors.Num(), SourceSpec.Anchors.Num());
    if (LoadedSpec.Anchors.Num() == 1)
    {
        TestEqual(TEXT("앵커 타입이 보존된다"), LoadedSpec.Anchors[0].Type, SourceSpec.Anchors[0].Type);
        TestEqual(TEXT("앵커 인덱스가 보존된다"), LoadedSpec.Anchors[0].Index, SourceSpec.Anchors[0].Index);
        TestTrue(TEXT("앵커 트랜스폼이 보존된다"),
            LoadedSpec.Anchors[0].LocalTransform.Equals(SourceSpec.Anchors[0].LocalTransform));
    }
    TestTrue(TEXT("허용된 구역 ID가 보존된다"), LoadedSpec.AllowedZoneIds == SourceSpec.AllowedZoneIds);
    TestTrue(TEXT("비주얼 메시 소프트 경로가 보존된다"),
        LoadedSpec.VisualMesh.ToSoftObjectPath() == SourceSpec.VisualMesh.ToSoftObjectPath());

    // Assert 2: Queue는 배열 순서와 transient drain/repath 값이 의미이므로 단순 개수만 비교하지 않는다.
    const FFFZSQueueSnapshot& SourceQueue = SourceFacility.Queue;
    const FFFZSQueueSnapshot& LoadedQueue = LoadedFacility.Queue;
    TestEqual(TEXT("대기열 상태가 보존된다"), LoadedQueue.State, SourceQueue.State);
    TestEqual(TEXT("차단 전 대기열 상태가 보존된다"),
        LoadedQueue.StateBeforeBlocked, SourceQueue.StateBeforeBlocked);
    TestEqual(TEXT("대기열 항목 배열이 보존된다"), LoadedQueue.Entries.Num(), SourceQueue.Entries.Num());
    TestEqual(TEXT("다음 참가 시퀀스가 보존된다"),
        LoadedQueue.NextJoinSequence, SourceQueue.NextJoinSequence);
    TestEqual(TEXT("대기열 집계 지표가 보존된다"),
        LoadedQueue.TotalCompletedWaitSeconds, SourceQueue.TotalCompletedWaitSeconds);
    TestEqual(TEXT("배수 요청이 보존된다"), LoadedQueue.bDrainRequested, SourceQueue.bDrainRequested);
    if (LoadedQueue.Entries.Num() == 2)
    {
        const FFFZSQueueEntrySnapshot& SourceEntry = SourceQueue.Entries[0];
        const FFFZSQueueEntrySnapshot& LoadedEntry = LoadedQueue.Entries[0];
        TestTrue(
            TEXT("첫 번째 대기열 항목 전체가 보존된다"),
            FFFZSQueueEntrySnapshot::StaticStruct()->CompareScriptStruct(
                &LoadedEntry,
                &SourceEntry,
                0));
        TestTrue(TEXT("대기열 항목 순서가 보존된다"),
            LoadedQueue.Entries[1].TicketId == SourceQueue.Entries[1].TicketId &&
            LoadedQueue.Entries[1].AgentId == SourceQueue.Entries[1].AgentId &&
            LoadedQueue.Entries[1].JoinSequence == SourceQueue.Entries[1].JoinSequence);
    }

    // Assert 3: receipt가 사라지면 Load 후 동일 Command 재전송의 멱등성이 깨지므로 fingerprint까지 비교한다.
    const FFFZSCommandReceiptSnapshot& SourceReceipt = Source.CommandReceipts[0];
    const FFFZSCommandReceiptSnapshot& LoadedReceipt = Loaded.CommandReceipts[0];
    TestTrue(
        TEXT("명령 영수증 전체가 보존된다"),
        FFFZSCommandReceiptSnapshot::StaticStruct()->CompareScriptStruct(
            &LoadedReceipt,
            &SourceReceipt,
            0));
    TestEqual(TEXT("영수증 지문이 보존된다"),
        LoadedReceipt.PayloadFingerprint, SourceReceipt.PayloadFingerprint);
    TestEqual(TEXT("영수증 결과 리비전이 보존된다"),
        LoadedReceipt.ResultWorldRevision, SourceReceipt.ResultWorldRevision);
    TestTrue(TEXT("영수증의 시설 참조가 보존된다"),
        LoadedReceipt.FacilityInstanceId == SourceReceipt.FacilityInstanceId &&
        LoadedReceipt.bResultReferencesFacility == SourceReceipt.bResultReferencesFacility);

    return true;
}

#endif