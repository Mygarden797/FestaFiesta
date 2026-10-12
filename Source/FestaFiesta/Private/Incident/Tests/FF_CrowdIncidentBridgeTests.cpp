#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Incident/FF_CrowdIncidentBridgeSubsystem.h"
#include "Incident/FF_IncidentSettings.h"
#include "Incident/FF_IncidentSubsystem.h"
#include "Incident/FF_IncidentTags.h"
#include "Incident/Rules/FF_IncidentRules.h"
#include "Templates/UnrealTemplate.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdIncidentBridgeTest,
	"FestaFiesta.Incident.CrowdBridge.SustainedClearAndHandleSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_CrowdIncidentBridgeTest::RunTest(const FString& Parameters)
{
	UFF_CrowdIncidentBridgeSubsystem* Bridge = NewObject<UFF_CrowdIncidentBridgeSubsystem>();
	UFF_IncidentSubsystem* Incident = NewObject<UFF_IncidentSubsystem>();
	Bridge->Incident = Incident;
	UFF_IncidentSettings* Settings = GetMutableDefault<UFF_IncidentSettings>();
	TGuardValue<int32> RestoreClearSnapshots(Settings->CongestionClearSnapshots, 5);
	const FIntPoint Cell(7, 12);

	// 월드를 시작하지 않고 실제 사건 슬롯·중복 키를 세워 철회 경로만 검사한다.
	auto Reset = [Bridge, Incident]()
	{
		Bridge->ResetCongestionTracking(false);
		Incident->IncidentSlots.Reset();
		Incident->FreeSlots.Reset();
		Incident->ActiveByKey.Reset();
	};
	auto SeedIncident = [Bridge, Incident, Cell]()
	{
		FFF_IncidentHandle Handle;
		Handle.Index = Incident->IncidentSlots.AddDefaulted();
		UFF_IncidentSubsystem::FIncidentSlot& Slot = Incident->IncidentSlots[Handle.Index];
		Slot.bInUse = true;
		Slot.IncidentTag = FFIncidentTags::Crowd::Congestion;
		Slot.Anchor.Kind = EFF_IncidentAnchorKind::DensityCell;
		Slot.Anchor.DensityCell = Cell;
		Slot.Anchor.WorldPoint = FVector(1500.f, 2500.f, 0.f);
		Slot.DedupeKey = FFF_IncidentRules::MakeDedupeKey(Slot.IncidentTag, Slot.Anchor);
		Incident->ActiveByKey.Add(Slot.DedupeKey, Handle);
		Bridge->OpenCongestionHandles.Add(Cell, Handle);
		Bridge->CongestionStreakByCell.Add(Cell, 10);
		return Handle;
	};
	auto Sample = [Bridge, Cell](int32 Index, float Minutes, bool bCongested, float CellSize = 2.f)
	{
		FFF_CrowdCongestionSnapshot Snapshot;
		Snapshot.SnapshotIndex = Index;
		Snapshot.SimTimeMinutes = Minutes;
		Snapshot.CellSizeMeters = CellSize;
		if (bCongested)
		{
			FFF_CrowdCongestedCell& SampleCell = Snapshot.Cells.AddDefaulted_GetRef();
			SampleCell.Cell = Cell;
			SampleCell.CenterWorldUU = FVector(1500.f, 2500.f, 0.f);
			SampleCell.Occupancy = 5;
			SampleCell.DensityPerSqm = 1.25f;
		}
		Bridge->HandleCongestionSampled(Snapshot);
	};

	FFF_IncidentHandle Handle = SeedIncident();
	Sample(1, 1.f, true);
	Sample(2, 2.f, false);
	TestTrue(TEXT("One clear sample keeps the incident and marker source active"), Incident->IsHandleValid(Handle));
	Sample(2, 2.f, false);
	TestEqual(TEXT("A duplicate sample does not count as another clear"), Bridge->CongestionClearStreakByCell.FindRef(Cell), 1);
	Sample(3, 3.f, true);
	TestFalse(TEXT("Returning congestion cancels pending clearance"), Bridge->CongestionClearStreakByCell.Contains(Cell));
	for (int32 Index = 4; Index <= 7; ++Index)
	{
		Sample(Index, static_cast<float>(Index), false);
	}
	TestTrue(TEXT("Four new clear samples still keep the incident active"), Incident->IsHandleValid(Handle));
	Sample(8, 8.f, false);
	TestFalse(TEXT("Five consecutive clear samples naturally close the incident"), Incident->IsHandleValid(Handle));
	TestEqual(TEXT("Natural clearance preserves the existing expired state"),
		Incident->IncidentSlots[Handle.Index].State, EFF_IncidentState::Expired);
	TestTrue(TEXT("Clearance releases tracking"), Bridge->OpenCongestionHandles.IsEmpty());

	Reset();
	Handle = SeedIncident();
	Sample(1, 1.f, true);
	Sample(2, 2.f, false);
	Sample(3, 3.f, false);
	Sample(7, 7.f, false);
	TestEqual(TEXT("A sampling gap starts clearance again at one"), Bridge->CongestionClearStreakByCell.FindRef(Cell), 1);
	for (int32 Index = 8; Index <= 10; ++Index)
	{
		Sample(Index, static_cast<float>(Index), false);
	}
	TestTrue(TEXT("Missing samples are not evidence of sustained clearance"), Incident->IsHandleValid(Handle));
	Sample(11, 11.f, false);
	TestFalse(TEXT("Five actual samples after the gap close the incident"), Incident->IsHandleValid(Handle));

	Reset();
	Handle = SeedIncident();
	Sample(1, 1.f, true);
	UFF_IncidentSubsystem::FIncidentSlot& ReusedSlot = Incident->IncidentSlots[Handle.Index];
	++ReusedSlot.Serial;
	FFF_IncidentHandle Replacement = Handle;
	Replacement.Serial = ReusedSlot.Serial;
	Incident->ActiveByKey.Add(ReusedSlot.DedupeKey, Replacement);
	TestFalse(TEXT("The completed handle no longer addresses its reused slot"), Incident->IsHandleValid(Handle));
	for (int32 Index = 2; Index <= 12; ++Index)
	{
		Sample(Index, static_cast<float>(Index), true);
	}
	TestTrue(TEXT("An already completed episode is not reported again while congestion persists"),
		Bridge->OpenCongestionHandles.FindRef(Cell) == Handle);
	for (int32 Index = 13; Index <= 17; ++Index)
	{
		Sample(Index, static_cast<float>(Index), false);
	}
	TestTrue(TEXT("Clearing an old episode cannot withdraw a replacement with the same key"), Incident->IsHandleValid(Replacement));
	TestTrue(TEXT("Completed episode tracking ends after sustained clearance"), Bridge->OpenCongestionHandles.IsEmpty());
	Sample(18, 18.f, true);
	TestEqual(TEXT("Congestion after a fully cleared episode starts a fresh creation streak"), Bridge->CongestionStreakByCell.FindRef(Cell), 1);

	Reset();
	Handle = SeedIncident();
	Sample(20, 20.f, true);
	Sample(21, 21.f, false);
	Sample(22, 0.f, false);
	TestFalse(TEXT("A new live clock does not retain a previous session incident"), Incident->IsHandleValid(Handle));
	TestTrue(TEXT("A new live clock clears pending clearance counts"), Bridge->CongestionClearStreakByCell.IsEmpty());

	Reset();
	Handle = SeedIncident();
	Sample(20, 20.f, true);
	Sample(1, 21.f, true);
	TestFalse(TEXT("A replacement snapshot sequence closes previous tracking"), Incident->IsHandleValid(Handle));
	TestEqual(TEXT("The replacement sequence begins its own creation streak"), Bridge->CongestionStreakByCell.FindRef(Cell), 1);

	Reset();
	Handle = SeedIncident();
	Sample(1, 1.f, true);
	Sample(2, 2.f, false, 0.f);
	TestEqual(TEXT("An uninitialized grid is ignored rather than treated as clear"), Bridge->LastCongestionSnapshotIndex, 1);
	TestTrue(TEXT("An uninitialized grid leaves the incident active"), Incident->IsHandleValid(Handle));
	Sample(2, 2.f, false, 4.f);
	TestFalse(TEXT("A different grid does not reuse old cell incidents"), Incident->IsHandleValid(Handle));

	Reset();
	Handle = SeedIncident();
	Sample(1, 1.f, true);
	Settings->CongestionClearSnapshots = 1;
	Sample(2, 2.f, false);
	TestFalse(TEXT("The configured clearance count is used"), Incident->IsHandleValid(Handle));
	return true;
}

#endif
