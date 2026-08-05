/**
 * @file FF_FFZSSaveGameTests.cpp
 * @brief Schema 4 Snapshot과 1cm 압축 Region 값 그래프가 실제 SaveGame 바이트 왕복에서 손실되지 않는지 검증한다.
 *
 * Spec, Transform, FText, SoftObjectPath, Queue의 Waiting/Service/Exit, Receipt와 CurrentPhase를
 * 서로 구별되는 sentinel 값으로 채운 뒤 SaveGameToMemory/LoadGameFromMemory 결과를 비교한다.
 * 이 테스트의 SchemaVersion 97은 Runtime Restore 허용 버전이 아니라 직렬화 필드가 그대로
 * 보존되는지 찾기 쉬운 값으로 확인하기 위한 sentinel이다.
 */

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "FF_FFZSSaveGameTestObject.h"
#include "Engine/StaticMesh.h"
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
    FFFZSSaveGameByteRoundTripTest,
    "FF_ZoneSystem.Unit.SaveGame.WorldSnapshotByteRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSSaveGameByteRoundTripTest::RunTest(const FString& Parameters)
{
    UFFZSSaveGameTestObject* SaveObject = NewObject<UFFZSSaveGameTestObject>(GetTransientPackage());
    if (!TestNotNull(TEXT("SaveGame wrapper"), SaveObject))
    {
        return false;
    }

    // Arrange/Act: reflected 비교만 하지 않고 실제 Unreal SaveGame archive를 byte 배열까지 통과시킨다.
    SaveObject->Snapshot = MakeDistinctSnapshot();
    TArray<uint8> Bytes;
    if (!TestTrue(
            TEXT("UGameplayStatics serializes the SaveGame to bytes"),
            UGameplayStatics::SaveGameToMemory(SaveObject, Bytes)) ||
        !TestTrue(TEXT("Serialized SaveGame contains bytes"), !Bytes.IsEmpty()))
    {
        return false;
    }

    USaveGame* LoadedBase = UGameplayStatics::LoadGameFromMemory(Bytes);
    UFFZSSaveGameTestObject* LoadedObject = Cast<UFFZSSaveGameTestObject>(LoadedBase);
    if (!TestNotNull(TEXT("UGameplayStatics reconstructs the typed SaveGame"), LoadedObject))
    {
        return false;
    }

    // Assert 1: 전체 reflected graph를 비교한 뒤, 장애 위치를 알 수 있도록 중요 계약을 필드별로 다시 확인한다.
    const FFFZSWorldSnapshot& Source = SaveObject->Snapshot;
    const FFFZSWorldSnapshot& Loaded = LoadedObject->Snapshot;
    TestTrue(
        TEXT("The complete reflected world snapshot survives the byte round trip"),
        FFFZSWorldSnapshot::StaticStruct()->CompareScriptStruct(&Loaded, &Source, 0));

    TestEqual(TEXT("World schema version survives"), Loaded.SchemaVersion, Source.SchemaVersion);
    TestEqual(TEXT("World revision survives"), Loaded.WorldRevision, Source.WorldRevision);
    TestEqual(TEXT("Change phase survives"), Loaded.CurrentPhase, Source.CurrentPhase);
    TestEqual(TEXT("Facility array survives"), Loaded.Facilities.Num(), Source.Facilities.Num());
    TestEqual(TEXT("Receipt array survives"), Loaded.CommandReceipts.Num(), Source.CommandReceipts.Num());
    if (Loaded.Facilities.Num() != 1 || Loaded.CommandReceipts.Num() != 1)
    {
        return false;
    }

    const FFFZSFacilityInstanceSnapshot& SourceFacility = Source.Facilities[0];
    const FFFZSFacilityInstanceSnapshot& LoadedFacility = Loaded.Facilities[0];
    TestTrue(
        TEXT("The complete facility specification survives"),
        FFFZSFacilitySpec::StaticStruct()->CompareScriptStruct(
            &LoadedFacility.FacilitySpec,
            &SourceFacility.FacilitySpec,
            0));
    TestTrue(TEXT("Facility ID survives"),
        LoadedFacility.FacilityInstanceId == SourceFacility.FacilityInstanceId);
    TestTrue(TEXT("Grid placement survives"), LoadedFacility.Placement == SourceFacility.Placement);
    TestEqual(TEXT("Facility state survives"), LoadedFacility.State, SourceFacility.State);
    TestTrue(TEXT("Facility transform survives"),
        LoadedFacility.WorldTransform.Equals(SourceFacility.WorldTransform));
    TestEqual(TEXT("Relocation flag survives"),
        LoadedFacility.bNeedsRelocation, SourceFacility.bNeedsRelocation);

    const FFFZSFacilitySpec& SourceSpec = SourceFacility.FacilitySpec;
    const FFFZSFacilitySpec& LoadedSpec = LoadedFacility.FacilitySpec;
    TestEqual(TEXT("Facility spec schema survives"), LoadedSpec.SchemaVersion, SourceSpec.SchemaVersion);
    TestEqual(TEXT("Facility definition ID survives"),
        LoadedSpec.FacilityDefinitionId, SourceSpec.FacilityDefinitionId);
    TestEqual(TEXT("Facility display name survives"),
        LoadedSpec.DisplayName.ToString(), SourceSpec.DisplayName.ToString());
    TestTrue(TEXT("Compressed queue reservation rects survive"),
        LoadedSpec.QueueReservedRects == SourceSpec.QueueReservedRects);
    TestTrue(TEXT("Legacy small queue reserved cells survive"),
        LoadedSpec.QueueReservedCells == SourceSpec.QueueReservedCells);
    TestTrue(TEXT("Queue spline points survive"),
        LoadedSpec.QueueSplinePointsCm == SourceSpec.QueueSplinePointsCm);
    TestEqual(TEXT("Queue rule spacing survives"),
        LoadedSpec.QueueRules.SlotSpacingCm, SourceSpec.QueueRules.SlotSpacingCm);
    TestEqual(TEXT("Queue rule capacity survives"),
        LoadedSpec.QueueRules.ServiceCapacity, SourceSpec.QueueRules.ServiceCapacity);
    TestEqual(TEXT("Anchor array survives"), LoadedSpec.Anchors.Num(), SourceSpec.Anchors.Num());
    if (LoadedSpec.Anchors.Num() == 1)
    {
        TestEqual(TEXT("Anchor type survives"), LoadedSpec.Anchors[0].Type, SourceSpec.Anchors[0].Type);
        TestEqual(TEXT("Anchor index survives"), LoadedSpec.Anchors[0].Index, SourceSpec.Anchors[0].Index);
        TestTrue(TEXT("Anchor transform survives"),
            LoadedSpec.Anchors[0].LocalTransform.Equals(SourceSpec.Anchors[0].LocalTransform));
    }
    TestTrue(TEXT("Allowed Zone IDs survive"), LoadedSpec.AllowedZoneIds == SourceSpec.AllowedZoneIds);
    TestTrue(TEXT("Visual mesh soft path survives"),
        LoadedSpec.VisualMesh.ToSoftObjectPath() == SourceSpec.VisualMesh.ToSoftObjectPath());

    // Assert 2: Queue는 배열 순서와 transient drain/repath 값이 의미이므로 단순 개수만 비교하지 않는다.
    const FFFZSQueueSnapshot& SourceQueue = SourceFacility.Queue;
    const FFFZSQueueSnapshot& LoadedQueue = LoadedFacility.Queue;
    TestEqual(TEXT("Queue state survives"), LoadedQueue.State, SourceQueue.State);
    TestEqual(TEXT("Pre-block queue state survives"),
        LoadedQueue.StateBeforeBlocked, SourceQueue.StateBeforeBlocked);
    TestEqual(TEXT("Queue entry array survives"), LoadedQueue.Entries.Num(), SourceQueue.Entries.Num());
    TestEqual(TEXT("Next join sequence survives"),
        LoadedQueue.NextJoinSequence, SourceQueue.NextJoinSequence);
    TestEqual(TEXT("Queue aggregate metrics survive"),
        LoadedQueue.TotalCompletedWaitSeconds, SourceQueue.TotalCompletedWaitSeconds);
    TestEqual(TEXT("Drain request survives"), LoadedQueue.bDrainRequested, SourceQueue.bDrainRequested);
    if (LoadedQueue.Entries.Num() == 2)
    {
        const FFFZSQueueEntrySnapshot& SourceEntry = SourceQueue.Entries[0];
        const FFFZSQueueEntrySnapshot& LoadedEntry = LoadedQueue.Entries[0];
        TestTrue(
            TEXT("The complete first queue entry survives"),
            FFFZSQueueEntrySnapshot::StaticStruct()->CompareScriptStruct(
                &LoadedEntry,
                &SourceEntry,
                0));
        TestTrue(TEXT("Queue entry ordering survives"),
            LoadedQueue.Entries[1].TicketId == SourceQueue.Entries[1].TicketId &&
            LoadedQueue.Entries[1].AgentId == SourceQueue.Entries[1].AgentId &&
            LoadedQueue.Entries[1].JoinSequence == SourceQueue.Entries[1].JoinSequence);
    }

    // Assert 3: receipt가 사라지면 Load 후 동일 Command 재전송의 멱등성이 깨지므로 fingerprint까지 비교한다.
    const FFFZSCommandReceiptSnapshot& SourceReceipt = Source.CommandReceipts[0];
    const FFFZSCommandReceiptSnapshot& LoadedReceipt = Loaded.CommandReceipts[0];
    TestTrue(
        TEXT("The complete command receipt survives"),
        FFFZSCommandReceiptSnapshot::StaticStruct()->CompareScriptStruct(
            &LoadedReceipt,
            &SourceReceipt,
            0));
    TestEqual(TEXT("Receipt fingerprint survives"),
        LoadedReceipt.PayloadFingerprint, SourceReceipt.PayloadFingerprint);
    TestEqual(TEXT("Receipt result revision survives"),
        LoadedReceipt.ResultWorldRevision, SourceReceipt.ResultWorldRevision);
    TestTrue(TEXT("Receipt facility reference survives"),
        LoadedReceipt.FacilityInstanceId == SourceReceipt.FacilityInstanceId &&
        LoadedReceipt.bResultReferencesFacility == SourceReceipt.bResultReferencesFacility);

    return true;
}

#endif