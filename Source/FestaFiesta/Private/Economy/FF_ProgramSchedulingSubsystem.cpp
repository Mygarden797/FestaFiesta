// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/FF_ProgramSchedulingSubsystem.h"

#include "Building/FF_FacilityNamingLibrary.h"
#include "Economy/FF_EconomyLog.h"
#include "Economy/FF_StageEconomyBridgeSubsystem.h"
#include "Economy/Rules/FF_ProgramScheduleRules.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "NPC/Party/FF_ProgramReservationSubsystem.h"
#include "Time/FF_FestivalStageSubsystem.h"

#define LOCTEXT_NAMESPACE "FFProgramSchedulingSubsystem"

namespace
{
	EFF_PaidProgramScheduleResult MapEconomyResult(const EFF_ProgramPurchaseResult Result)
	{
		switch (Result)
		{
		case EFF_ProgramPurchaseResult::Success:
			return EFF_PaidProgramScheduleResult::Success;
		case EFF_ProgramPurchaseResult::FestivalNotStarted:
			return EFF_PaidProgramScheduleResult::FestivalNotStarted;
		case EFF_ProgramPurchaseResult::UnknownProgram:
			return EFF_PaidProgramScheduleResult::UnknownProgram;
		case EFF_ProgramPurchaseResult::AlreadyScheduled:
			return EFF_PaidProgramScheduleResult::AlreadyScheduled;
		case EFF_ProgramPurchaseResult::InsufficientGold:
			return EFF_PaidProgramScheduleResult::InsufficientGold;
		case EFF_ProgramPurchaseResult::FacilityTypeMismatch:
			return EFF_PaidProgramScheduleResult::FacilityTypeMismatch;
		}
		return EFF_PaidProgramScheduleResult::CommitFailed;
	}

	bool DoTimeRangesOverlap(
		const float FirstStart, const float FirstEnd,
		const float SecondStart, const float SecondEnd)
	{
		return FirstStart < SecondEnd && SecondStart < FirstEnd;
	}
}

bool UFF_ProgramSchedulingSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	const UWorld* World = Cast<UWorld>(Outer);
	return World != nullptr
		&& (World->WorldType == EWorldType::Game
			|| World->WorldType == EWorldType::PIE
			|| World->WorldType == EWorldType::GamePreview);
}

EFF_PaidProgramScheduleResult UFF_ProgramSchedulingSubsystem::EvaluateCommand(
	const FName ProgramId,
	const FGuid& FacilityInstanceId,
	UFF_FestivalProgressionSubsystem*& OutProgression) const
{
	OutProgression = nullptr;
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (GameInstance == nullptr)
	{
		return EFF_PaidProgramScheduleResult::FestivalNotStarted;
	}

	OutProgression = GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>();
	if (OutProgression == nullptr || !OutProgression->IsFestivalStarted())
	{
		return EFF_PaidProgramScheduleResult::FestivalNotStarted;
	}
	if (World->GetNetMode() == NM_Client)
	{
		return EFF_PaidProgramScheduleResult::NotAuthoritative;
	}

	const UFF_FestivalStageSubsystem* Stage =
		GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>();
	if (Stage == nullptr
		|| !UFF_StageEconomyBridgeSubsystem::IsProgramSchedulingStage(Stage->CurrentStage))
	{
		return EFF_PaidProgramScheduleResult::NotSchedulingStage;
	}
	if (!FacilityInstanceId.IsValid())
	{
		return EFF_PaidProgramScheduleResult::FacilityNotFound;
	}

	const UFFZSWorldSubsystem* Zone = World->GetSubsystem<UFFZSWorldSubsystem>();
	const AFFZSFacilityActor* Facility = Zone ? Zone->FindFacility(FacilityInstanceId) : nullptr;
	if (Facility == nullptr || Facility->GetFacilityInstanceId() != FacilityInstanceId)
	{
		return EFF_PaidProgramScheduleResult::FacilityNotFound;
	}
	if (Facility->NeedsRelocation())
	{
		return EFF_PaidProgramScheduleResult::FacilityNeedsRelocation;
	}
	if (!IsFacilityStateSchedulable(Facility->GetFacilityState()))
	{
		return EFF_PaidProgramScheduleResult::FacilityUnavailable;
	}

	const TArray<FFF_ProgramCard> Catalog = OutProgression->GetProgramCatalog();
	const FFF_ProgramCard* Card = FFF_ProgramScheduleRules::FindCard(Catalog, ProgramId);
	if (Card == nullptr || !Card->IsValid()
		|| !FMath::IsFinite(Card->StartMinutes)
		|| !FMath::IsFinite(Card->EndMinutes)
		|| Card->StartMinutes < 0.f
		|| Card->EndMinutes <= Card->StartMinutes)
	{
		return EFF_PaidProgramScheduleResult::UnknownProgram;
	}

	const FFFZSFacilityReadModel ReadModel = Facility->GetReadModel();
	FFF_FacilityNameParts NameParts;
	FString ParseFailure;
	if (!UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
		ReadModel.Category, ReadModel.FacilityDefinitionId, NameParts, ParseFailure)
		|| !FFF_ProgramScheduleRules::IsFacilityTypeAllowed(
			*Card, NameParts.MajorToken, NameParts.MinorToken))
	{
		return EFF_PaidProgramScheduleResult::FacilityTypeMismatch;
	}

	// 호환되지 않는 카드는 현재 Gold와 무관하게 picker가 일관되게 숨길 수 있도록
	// 시설 타입을 먼저 판정하고, 그 뒤 기존 경제 규칙(중복→Gold)을 적용한다.
	const EFF_PaidProgramScheduleResult EconomyResult =
		MapEconomyResult(OutProgression->CanScheduleProgramAtFacility(ProgramId, FacilityInstanceId));
	if (EconomyResult != EFF_PaidProgramScheduleResult::Success)
	{
		return EconomyResult;
	}

	const UFF_ProgramReservationSubsystem* Reservations =
		GameInstance->GetSubsystem<UFF_ProgramReservationSubsystem>();
	if (Reservations == nullptr)
	{
		return EFF_PaidProgramScheduleResult::ReservationUnavailable;
	}

	for (const FGuid ReservationId : Reservations->GetReservationIds())
	{
		FFF_ProgramReservation Reservation;
		if (Reservations->FindReservation(ReservationId, Reservation)
			&& Reservation.FacilityInstanceId == FacilityInstanceId
			&& DoTimeRangesOverlap(
				Card->StartMinutes, Card->EndMinutes,
				Reservation.StartMinutes, Reservation.EndMinutes))
		{
			return EFF_PaidProgramScheduleResult::FacilityTimeConflict;
		}
	}

	return EFF_PaidProgramScheduleResult::Success;
}

bool UFF_ProgramSchedulingSubsystem::IsFacilityStateSchedulable(
	const EFFZSFacilityState FacilityState)
{
	// MovePending은 실제 이동 중이 아니라 하루가 끝나 Queue가 빈 채로 자동 Close된 상태도
	// 포함한다(재오픈 판정과 동일하게 취급 — FF_ZoneSystemStageBridgeSubsystem 참고).
	// 진짜 이동 의도는 별도로 Facility->NeedsRelocation()에서 걸러진다.
	return FacilityState == EFFZSFacilityState::Validated
		|| FacilityState == EFFZSFacilityState::Closed
		|| FacilityState == EFFZSFacilityState::MovePending;
}

EFF_PaidProgramScheduleResult UFF_ProgramSchedulingSubsystem::CanScheduleProgramAtFacility(
	const FName ProgramId, const FGuid FacilityInstanceId) const
{
	if (bCommitInFlight)
	{
		return EFF_PaidProgramScheduleResult::CommitFailed;
	}
	UFF_FestivalProgressionSubsystem* Progression = nullptr;
	return EvaluateCommand(ProgramId, FacilityInstanceId, Progression);
}

EFF_PaidProgramScheduleResult UFF_ProgramSchedulingSubsystem::TryScheduleProgramAtFacility(
	const FName ProgramId, const FGuid FacilityInstanceId)
{
	if (bCommitInFlight)
	{
		return EFF_PaidProgramScheduleResult::CommitFailed;
	}
	TGuardValue<bool> CommitGuard(bCommitInFlight, true);
	UFF_FestivalProgressionSubsystem* Progression = nullptr;
	const EFF_PaidProgramScheduleResult Validation =
		EvaluateCommand(ProgramId, FacilityInstanceId, Progression);
	if (Validation != EFF_PaidProgramScheduleResult::Success || Progression == nullptr)
	{
		return Validation;
	}

	const EFF_PaidProgramScheduleResult CommitResult =
		MapEconomyResult(Progression->TryScheduleProgram(ProgramId, FacilityInstanceId));
	if (CommitResult != EFF_PaidProgramScheduleResult::Success)
	{
		UE_LOG(LogFestaFiestaEconomy, Warning,
			TEXT("[유료 프로그램] 선행 검증 뒤 '%s' / 시설 %s 커밋 실패: %s"),
			*ProgramId.ToString(), *FacilityInstanceId.ToString(),
			*UEnum::GetValueAsString(CommitResult));
	}
	return CommitResult;
}

TArray<FFF_ScheduledProgram>
UFF_ProgramSchedulingSubsystem::GetScheduledProgramsForFacility(
	const FGuid FacilityInstanceId) const
{
	TArray<FFF_ScheduledProgram> Result;
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UFF_FestivalProgressionSubsystem* Progression = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
	const UFF_ProgramReservationSubsystem* Reservations = GameInstance
		? GameInstance->GetSubsystem<UFF_ProgramReservationSubsystem>() : nullptr;
	if (!FacilityInstanceId.IsValid() || Progression == nullptr || Reservations == nullptr)
	{
		return Result;
	}

	for (const FFF_ScheduledProgram& Scheduled : Progression->GetScheduledPrograms())
	{
		FFF_ProgramReservation Reservation;
		if (Reservations->FindReservation(Scheduled.ReservationId, Reservation)
			&& Reservation.FacilityInstanceId == FacilityInstanceId)
		{
			Result.Add(Scheduled);
		}
	}
	return Result;
}

bool UFF_ProgramSchedulingSubsystem::HasScheduledProgramForFacility(
	const FGuid FacilityInstanceId) const
{
	return GetScheduledProgramsForFacility(FacilityInstanceId).Num() > 0;
}

FText UFF_ProgramSchedulingSubsystem::GetResultText(const EFF_PaidProgramScheduleResult Result)
{
	switch (Result)
	{
	case EFF_PaidProgramScheduleResult::Success: return LOCTEXT("Success", "프로그램을 구매하고 시설에 편성했습니다.");
	case EFF_PaidProgramScheduleResult::FestivalNotStarted: return LOCTEXT("FestivalNotStarted", "축제가 시작된 뒤 편성할 수 있습니다.");
	case EFF_PaidProgramScheduleResult::NotAuthoritative: return LOCTEXT("NotAuthoritative", "서버에서 편성을 처리해야 합니다.");
	case EFF_PaidProgramScheduleResult::NotSchedulingStage: return LOCTEXT("NotSchedulingStage", "준비 또는 아침 점검 단계에서만 편성할 수 있습니다.");
	case EFF_PaidProgramScheduleResult::FacilityNotFound: return LOCTEXT("FacilityNotFound", "선택한 시설을 찾을 수 없습니다.");
	case EFF_PaidProgramScheduleResult::FacilityNeedsRelocation: return LOCTEXT("FacilityNeedsRelocation", "시설을 유효한 위치로 옮긴 뒤 편성해 주세요.");
	case EFF_PaidProgramScheduleResult::FacilityUnavailable: return LOCTEXT("FacilityUnavailable", "설치가 확정되고 운영 전인 시설에만 편성할 수 있습니다.");
	case EFF_PaidProgramScheduleResult::UnknownProgram: return LOCTEXT("UnknownProgram", "선택한 프로그램 정보를 찾을 수 없습니다.");
	case EFF_PaidProgramScheduleResult::AlreadyScheduled: return LOCTEXT("AlreadyScheduled", "이 프로그램은 오늘 이미 편성했습니다.");
	case EFF_PaidProgramScheduleResult::InsufficientGold: return LOCTEXT("InsufficientGold", "프로그램 구매에 필요한 Gold가 부족합니다.");
	case EFF_PaidProgramScheduleResult::FacilityTypeMismatch: return LOCTEXT("FacilityTypeMismatch", "이 프로그램은 선택한 시설에서 운영할 수 없습니다.");
	case EFF_PaidProgramScheduleResult::FacilityTimeConflict: return LOCTEXT("FacilityTimeConflict", "이 시설의 다른 프로그램과 운영 시간이 겹칩니다.");
	case EFF_PaidProgramScheduleResult::ReservationUnavailable: return LOCTEXT("ReservationUnavailable", "예약 시스템을 준비하지 못해 편성할 수 없습니다.");
	case EFF_PaidProgramScheduleResult::CommitFailed: return LOCTEXT("CommitFailed", "구매·편성을 완료하지 못했습니다. 다시 시도해 주세요.");
	}
	return LOCTEXT("UnknownFailure", "프로그램을 편성할 수 없습니다.");
}

#undef LOCTEXT_NAMESPACE
