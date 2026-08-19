// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "NPC/Party/FF_ProgramReservationSubsystem.h"

/**
 * 예약 런타임 소유자 검증.
 *
 * 배정 계산 자체는 `FF_ProgramReservationTests.cpp`(순수 규칙)가 갖는다. 여기서 보는 것은
 * **소유자만 할 수 있는 것들**이다 — 중복 배정 차단, 파티 역색인, 배정과 슬롯의 수명 차이.
 *
 * 월드를 만들지 않는다. GameInstance 를 Outer 로 NewObject 하고 Initialize 는 부르지 않는다.
 */
namespace FFF_ProgramReservationSubsystemTestHelpers
{
	// Unity Build 동명 헬퍼 충돌을 피하려고 파일 전용 이름을 쓴다.
	struct FReservationHarness
	{
		bool Initialize()
		{
			GameInstance = NewObject<UGameInstance>(GetTransientPackage());
			if (GameInstance == nullptr)
			{
				return false;
			}

			Subsystem = NewObject<UFF_ProgramReservationSubsystem>(GameInstance);
			return Subsystem != nullptr;
		}

		FGuid RegisterProgram(float StartMinutes, float EndMinutes, int32 Capacity)
		{
			return Subsystem->RegisterProgram(FGuid::NewGuid(), StartMinutes, EndMinutes, Capacity);
		}

		UGameInstance* GameInstance = nullptr;
		UFF_ProgramReservationSubsystem* Subsystem = nullptr;
	};

	FFF_ReservationCandidateGroup MakeGroup(int32 GroupId, int32 MemberCount)
	{
		FFF_ReservationCandidateGroup Group;
		Group.GroupId = GroupId;
		Group.MemberCount = MemberCount;
		return Group;
	}
}

// 저작이 틀린 프로그램은 등록되지 않는다.
// 0 으로 폴백하면 아무도 배정되지 않는 프로그램이 목록에 조용히 남는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramReservationSubsystem_RejectsInvalidProgram, "FestaFiesta.NPC.ProgramReservationSubsystem.RejectsInvalidProgram", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramReservationSubsystem_RejectsInvalidProgram::RunTest(const FString& Parameters)
{
	FFF_ProgramReservationSubsystemTestHelpers::FReservationHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize()))
	{
		return false;
	}

	TestFalse(TEXT("정원 0 은 거부된다"),
		Harness.RegisterProgram(60.f, 120.f, 0).IsValid());
	TestFalse(TEXT("종료가 시작보다 빠르면 거부된다"),
		Harness.RegisterProgram(120.f, 60.f, 12).IsValid());
	TestFalse(TEXT("무효 시설 ID 는 거부된다"),
		Harness.Subsystem->RegisterProgram(FGuid(), 60.f, 120.f, 12).IsValid());

	TestEqual(TEXT("거부된 것은 목록에 남지 않는다"),
		Harness.Subsystem->GetReservationIds().Num(), 0);

	TestTrue(TEXT("정상 입력은 등록된다"),
		Harness.RegisterProgram(60.f, 120.f, 12).IsValid());
	TestEqual(TEXT("정상 등록만 목록에 남는다"),
		Harness.Subsystem->GetReservationIds().Num(), 1);

	return true;
}

// 한 파티가 두 프로그램에 동시에 들어가지 않는다.
// 겹치면 잠금 구간이 두 개가 되어 어느 시각으로 판정할지가 없어진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramReservationSubsystem_PartyCannotBookTwoPrograms, "FestaFiesta.NPC.ProgramReservationSubsystem.PartyCannotBookTwoPrograms", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramReservationSubsystem_PartyCannotBookTwoPrograms::RunTest(const FString& Parameters)
{
	FFF_ProgramReservationSubsystemTestHelpers::FReservationHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize()))
	{
		return false;
	}

	const FGuid Morning = Harness.RegisterProgram(60.f, 120.f, 4);
	const FGuid Afternoon = Harness.RegisterProgram(200.f, 260.f, 4);

	TArray<FFF_ReservationCandidateGroup> Groups;
	Groups.Add(FFF_ProgramReservationSubsystemTestHelpers::MakeGroup(10, 2));
	Groups.Add(FFF_ProgramReservationSubsystemTestHelpers::MakeGroup(20, 2));

	TestEqual(TEXT("오전 프로그램이 정원을 채운다"),
		Harness.Subsystem->AssignGroups(Morning, Groups), 4);

	// 같은 후보 목록을 오후에도 넘긴다. 이미 배정된 둘은 걸러져야 한다.
	TestEqual(TEXT("이미 배정된 그룹은 오후에 배정되지 않는다"),
		Harness.Subsystem->AssignGroups(Afternoon, Groups), 0);

	FFF_ProgramReservation AfternoonReservation;
	TestTrue(TEXT("오후 예약을 찾는다"),
		Harness.Subsystem->FindReservation(Afternoon, AfternoonReservation));
	TestEqual(TEXT("오후는 배정 인원이 0 이다"), AfternoonReservation.AssignedHeadcount, 0);
	TestEqual(TEXT("오후는 정원 전체가 빈자리다"), AfternoonReservation.UnfilledSeats, 4);

	// 역색인도 오전만 가리켜야 한다.
	FFF_ProgramReservation Found;
	TestTrue(TEXT("파티 10 의 활성 예약을 찾는다"),
		Harness.Subsystem->FindActiveReservationForParty(10, 0.f, Found));
	TestEqual(TEXT("찾은 것이 오전 프로그램이다"), Found.StartMinutes, 60.f);

	return true;
}

// 잠금과 퇴장 금지는 구간이 다르다.
// 초안에서 이 둘을 같은 것으로 취급해 시작 전 자유 행동 중의 퇴장을 못 막고 있었다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramReservationSubsystem_LockAndExitBlockDifferInWindow, "FestaFiesta.NPC.ProgramReservationSubsystem.LockAndExitBlockDifferInWindow", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramReservationSubsystem_LockAndExitBlockDifferInWindow::RunTest(const FString& Parameters)
{
	FFF_ProgramReservationSubsystemTestHelpers::FReservationHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize()))
	{
		return false;
	}

	const FGuid Program = Harness.RegisterProgram(60.f, 120.f, 2);
	TArray<FFF_ReservationCandidateGroup> Groups;
	Groups.Add(FFF_ProgramReservationSubsystemTestHelpers::MakeGroup(7, 2));
	Harness.Subsystem->AssignGroups(Program, Groups);

	// 시작 전 — 자유 행동이므로 재평가는 하고, 퇴장만 막힌다.
	TestFalse(TEXT("시작 전에는 잠기지 않는다"), Harness.Subsystem->IsPartyLockedAt(7, 30.f));
	TestTrue(TEXT("시작 전에도 퇴장은 막힌다"), Harness.Subsystem->IsPartyExitBlockedAt(7, 30.f));

	// 진행 중 — 둘 다 막힌다.
	TestTrue(TEXT("진행 중에는 잠긴다"), Harness.Subsystem->IsPartyLockedAt(7, 90.f));
	TestTrue(TEXT("진행 중에는 퇴장도 막힌다"), Harness.Subsystem->IsPartyExitBlockedAt(7, 90.f));

	// 종료 후 — 둘 다 풀린다.
	TestFalse(TEXT("종료 후에는 잠기지 않는다"), Harness.Subsystem->IsPartyLockedAt(7, 150.f));
	TestFalse(TEXT("종료 후에는 퇴장이 가능하다"), Harness.Subsystem->IsPartyExitBlockedAt(7, 150.f));

	// 예약이 없는 파티는 아무것도 막히지 않는다.
	TestFalse(TEXT("예약 없는 파티는 잠기지 않는다"), Harness.Subsystem->IsPartyLockedAt(999, 90.f));
	TestFalse(TEXT("예약 없는 파티는 퇴장이 가능하다"), Harness.Subsystem->IsPartyExitBlockedAt(999, 90.f));

	return true;
}

// 하루 경계는 배정만 지우고 슬롯은 남긴다.
// 프로그램은 남은 축제 기간 전체 구매(E-10)이고, 방문객은 날마다 새로 온다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramReservationSubsystem_ClearAssignmentsKeepsSlots, "FestaFiesta.NPC.ProgramReservationSubsystem.ClearAssignmentsKeepsSlots", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramReservationSubsystem_ClearAssignmentsKeepsSlots::RunTest(const FString& Parameters)
{
	FFF_ProgramReservationSubsystemTestHelpers::FReservationHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize()))
	{
		return false;
	}

	const FGuid Program = Harness.RegisterProgram(60.f, 120.f, 3);
	TArray<FFF_ReservationCandidateGroup> Groups;
	Groups.Add(FFF_ProgramReservationSubsystemTestHelpers::MakeGroup(1, 3));
	TestEqual(TEXT("정원을 채운다"), Harness.Subsystem->AssignGroups(Program, Groups), 3);

	Harness.Subsystem->ClearAssignments();

	TestEqual(TEXT("슬롯은 남는다"), Harness.Subsystem->GetReservationIds().Num(), 1);

	FFF_ProgramReservation Reservation;
	TestTrue(TEXT("예약을 찾는다"), Harness.Subsystem->FindReservation(Program, Reservation));
	TestEqual(TEXT("정원은 그대로다"), Reservation.Capacity, 3);
	TestEqual(TEXT("배정 인원이 0 으로 돌아간다"), Reservation.AssignedHeadcount, 0);
	TestEqual(TEXT("빈자리가 정원 전체가 된다"), Reservation.UnfilledSeats, 3);
	TestEqual(TEXT("배정 목록이 비워진다"), Reservation.AssignedGroupIds.Num(), 0);

	// 어제 배정된 파티가 오늘까지 잠긴 채로 남으면 안 된다.
	FFF_ProgramReservation Found;
	TestFalse(TEXT("어제 파티는 더 이상 예약을 갖지 않는다"),
		Harness.Subsystem->FindActiveReservationForParty(1, 90.f, Found));
	TestFalse(TEXT("어제 파티의 퇴장 금지가 풀린다"),
		Harness.Subsystem->IsPartyExitBlockedAt(1, 90.f));

	// 같은 슬롯에 오늘 방문객을 다시 배정할 수 있다.
	TArray<FFF_ReservationCandidateGroup> TodayGroups;
	TodayGroups.Add(FFF_ProgramReservationSubsystemTestHelpers::MakeGroup(2, 3));
	TestEqual(TEXT("같은 슬롯에 오늘 그룹을 배정한다"),
		Harness.Subsystem->AssignGroups(Program, TodayGroups), 3);

	return true;
}

// 슬롯을 지우면 배정도 같이 풀린다. 없는 예약을 가리키는 파티가 남으면 영원히 잠긴다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramReservationSubsystem_RemoveReleasesParties, "FestaFiesta.NPC.ProgramReservationSubsystem.RemoveReleasesParties", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramReservationSubsystem_RemoveReleasesParties::RunTest(const FString& Parameters)
{
	FFF_ProgramReservationSubsystemTestHelpers::FReservationHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize()))
	{
		return false;
	}

	const FGuid Program = Harness.RegisterProgram(60.f, 120.f, 2);
	TArray<FFF_ReservationCandidateGroup> Groups;
	Groups.Add(FFF_ProgramReservationSubsystemTestHelpers::MakeGroup(5, 2));
	Harness.Subsystem->AssignGroups(Program, Groups);

	TestTrue(TEXT("지우기 전에는 퇴장이 막혀 있다"),
		Harness.Subsystem->IsPartyExitBlockedAt(5, 30.f));

	TestTrue(TEXT("예약이 지워진다"), Harness.Subsystem->RemoveReservation(Program));
	TestFalse(TEXT("없는 예약을 다시 지우면 false"), Harness.Subsystem->RemoveReservation(Program));

	TestFalse(TEXT("지운 뒤에는 퇴장 금지가 풀린다"),
		Harness.Subsystem->IsPartyExitBlockedAt(5, 30.f));
	TestFalse(TEXT("지운 뒤에는 잠금도 풀린다"),
		Harness.Subsystem->IsPartyLockedAt(5, 90.f));

	// 그 파티는 이제 다른 프로그램에 배정될 수 있다.
	const FGuid Other = Harness.RegisterProgram(200.f, 260.f, 2);
	TestEqual(TEXT("풀린 파티는 다른 프로그램에 배정된다"),
		Harness.Subsystem->AssignGroups(Other, Groups), 2);

	return true;
}

// 목록은 시작 시각 오름차순으로 나온다. TMap 순회 순서를 노출하면 화면 순서가 실행마다 바뀐다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramReservationSubsystem_ListIsSortedByStartTime, "FestaFiesta.NPC.ProgramReservationSubsystem.ListIsSortedByStartTime", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramReservationSubsystem_ListIsSortedByStartTime::RunTest(const FString& Parameters)
{
	FFF_ProgramReservationSubsystemTestHelpers::FReservationHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize()))
	{
		return false;
	}

	// 일부러 뒤섞어 넣는다.
	Harness.RegisterProgram(200.f, 260.f, 4);
	Harness.RegisterProgram(60.f, 120.f, 4);
	Harness.RegisterProgram(130.f, 190.f, 4);

	const TArray<FGuid> Ids = Harness.Subsystem->GetReservationIds();
	if (!TestEqual(TEXT("세 개가 나온다"), Ids.Num(), 3))
	{
		return false;
	}

	float PreviousStart = -1.f;
	for (const FGuid& Id : Ids)
	{
		FFF_ProgramReservation Reservation;
		if (!TestTrue(TEXT("목록의 핸들로 예약을 찾는다"),
			Harness.Subsystem->FindReservation(Id, Reservation)))
		{
			return false;
		}
		TestTrue(TEXT("시작 시각이 오름차순이다"), Reservation.StartMinutes > PreviousStart);
		PreviousStart = Reservation.StartMinutes;
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
