// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Economy/FF_FestivalEconomyTypes.h"
#include "FF_FFZSTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_ProgramSchedulingSubsystem.generated.h"

class UFF_FestivalProgressionSubsystem;

/** 기존 경제 결과를 바꾸지 않고 월드·시설 검증까지 표현하는 상위 명령 결과. */
UENUM(BlueprintType)
enum class EFF_PaidProgramScheduleResult : uint8
{
	Success                 UMETA(DisplayName = "구매·편성 완료"),
	FestivalNotStarted      UMETA(DisplayName = "축제가 시작되지 않았다"),
	NotAuthoritative        UMETA(DisplayName = "서버 권한이 필요하다"),
	NotSchedulingStage      UMETA(DisplayName = "현재 단계에서는 편성할 수 없다"),
	FacilityNotFound        UMETA(DisplayName = "시설을 찾을 수 없다"),
	FacilityNeedsRelocation UMETA(DisplayName = "시설을 먼저 재배치해야 한다"),
	FacilityUnavailable     UMETA(DisplayName = "현재 상태의 시설에는 편성할 수 없다"),
	UnknownProgram          UMETA(DisplayName = "프로그램을 찾을 수 없다"),
	AlreadyScheduled        UMETA(DisplayName = "오늘 이미 편성한 프로그램이다"),
	NotEnoughStaff          UMETA(DisplayName = "배치 가능한 인원이 부족하다"),
	InsufficientGold        UMETA(DisplayName = "Gold가 부족하다"),
	FacilityTypeMismatch    UMETA(DisplayName = "이 시설에서 운영할 수 없는 프로그램이다"),
	FacilityTimeConflict    UMETA(DisplayName = "이 시설의 다른 프로그램과 시간이 겹친다"),
	ReservationUnavailable UMETA(DisplayName = "프로그램 예약 시스템을 사용할 수 없다"),
	CommitFailed            UMETA(DisplayName = "구매·편성을 완료하지 못했다")
};

/** 시설 기반 유료 프로그램 구매·편성의 단일 권위 명령 경계. */
UCLASS()
class FESTAFIESTA_API UFF_ProgramSchedulingSubsystem final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Paid Program")
	EFF_PaidProgramScheduleResult CanScheduleProgramAtFacility(
		FName ProgramId, FGuid FacilityInstanceId) const;

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Paid Program")
	EFF_PaidProgramScheduleResult TryScheduleProgramAtFacility(
		FName ProgramId, FGuid FacilityInstanceId);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Paid Program")
	bool HasScheduledProgramForFacility(FGuid FacilityInstanceId) const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Paid Program")
	TArray<FFF_ScheduledProgram> GetScheduledProgramsForFacility(
		FGuid FacilityInstanceId) const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Paid Program")
	static FText GetResultText(EFF_PaidProgramScheduleResult Result);

	static bool IsFacilityStateSchedulable(EFFZSFacilityState FacilityState);

private:
	EFF_PaidProgramScheduleResult EvaluateCommand(
		FName ProgramId,
		const FGuid& FacilityInstanceId,
		UFF_FestivalProgressionSubsystem*& OutProgression) const;

	bool bCommitInFlight = false;
};
