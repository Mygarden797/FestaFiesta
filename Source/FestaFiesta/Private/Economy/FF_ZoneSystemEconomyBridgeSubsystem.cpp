// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/FF_ZoneSystemEconomyBridgeSubsystem.h"

#include "Economy/FF_EconomyLog.h"
#include "Economy/FF_FacilityCostResolver.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "Subsystems/SubsystemCollection.h"
#include "Economy/FF_FestivalEconomyTypes.h"

#define LOCTEXT_NAMESPACE "FFZoneSystemEconomyBridge"

namespace
{
	FFFZSPolicyDecision MakeRejectedDecision(
		const EFFZSPlacementFailure FailureReason,
		const FText& Message)
	{
		FFFZSPolicyDecision Decision;
		Decision.bAllowed = false;
		Decision.FailureReason = FailureReason;
		Decision.Message = Message;
		return Decision;
	}
}

void UFF_ZoneSystemEconomyBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UFFZSWorldSubsystem>();
	Super::Initialize(Collection);

	UWorld* World = GetWorld();
	ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	if (!ZoneSystem)
	{
		UE_LOG(LogFestaFiestaEconomy, Error,
			TEXT("[경제] FF_ZoneSystem을 찾을 수 없어 시설 설치비 정책을 등록하지 못했습니다."));
		return;
	}

	bPolicyRegistered = ZoneSystem->RegisterExternalPolicy(this);
	if (!bPolicyRegistered)
	{
		UE_LOG(LogFestaFiestaEconomy, Error,
			TEXT("[경제] 시설 설치비 정책을 FF_ZoneSystem Economy 도메인에 등록하지 못했습니다."));
	}
}

void UFF_ZoneSystemEconomyBridgeSubsystem::Deinitialize()
{
	if (bPolicyRegistered && ZoneSystem)
	{
		ZoneSystem->UnregisterExternalPolicy(this);
	}

	PendingInstallCosts.Reset();
	PendingMatchedCostRows.Reset();
	PendingValidatedGold.Reset();
	PendingCostTablePaths.Reset();
	bPolicyRegistered = false;
	ZoneSystem = nullptr;
	Super::Deinitialize();
}

bool UFF_ZoneSystemEconomyBridgeSubsystem::DoesSupportWorldType(
	const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game
		|| WorldType == EWorldType::PIE
		|| WorldType == EWorldType::GamePreview;
}

FName UFF_ZoneSystemEconomyBridgeSubsystem::GetFFZoneSystemPolicyName_Implementation() const
{
	return TEXT("FestaFiesta.FacilityEconomy");
}

EFFZSPolicyDomain UFF_ZoneSystemEconomyBridgeSubsystem::GetFFZoneSystemPolicyDomain_Implementation() const
{
	return EFFZSPolicyDomain::Economy;
}

UFF_FestivalProgressionSubsystem*
UFF_ZoneSystemEconomyBridgeSubsystem::ResolveFestivalProgression() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>()
		: nullptr;
}

FFFZSPolicyDecision UFF_ZoneSystemEconomyBridgeSubsystem::ValidateInstallCost(
	const FFFZSPolicyContext& Context,
	int32& OutInstallCost,
	FName& OutMatchedCostRow,
	int32& OutValidatedGold,
	FString& OutCostTablePath) const
{
	OutInstallCost = 0;
	OutMatchedCostRow = NAME_None;
	OutValidatedGold = 0;
	OutCostTablePath.Reset();

	UFF_FestivalProgressionSubsystem* Progression = ResolveFestivalProgression();
	if (!Progression)
	{
		return MakeRejectedDecision(
			EFFZSPlacementFailure::ExternalPolicyRejected,
			LOCTEXT("MissingProgression", "축제 경제 서브시스템을 찾을 수 없어 시설을 설치할 수 없습니다."));
	}

	const UFF_FestivalDefinition* Festival = Progression->GetDefinition();
	if (!Progression->IsFestivalStarted() || !Festival)
	{
		return MakeRejectedDecision(
			EFFZSPlacementFailure::ExternalPolicyRejected,
			LOCTEXT("FestivalNotStarted", "축제가 시작되지 않아 시설 설치비를 검증할 수 없습니다."));
	}

	const UDataTable* CostTable = Festival->FacilityCostTable.Get();
	if (!CostTable)
	{
		return MakeRejectedDecision(
			EFFZSPlacementFailure::DataInvalid,
			LOCTEXT("MissingCostTable", "축제 정의에 시설 비용 DataTable이 없어 시설을 설치할 수 없습니다."));
	}
	OutCostTablePath = CostTable->GetPathName();

	FString FailureReason;
	if (!FFF_FacilityCostResolver::FindInstallCost(
		Context.Command.FacilitySpec,
		CostTable,
		OutInstallCost,
		OutMatchedCostRow,
		FailureReason))
	{
		return MakeRejectedDecision(
			EFFZSPlacementFailure::DataInvalid,
			FText::Format(
				LOCTEXT("CostLookupFailed", "시설 설치비를 확인할 수 없습니다: {0}"),
				FText::FromString(FailureReason)));
	}

	if (!Progression->CanAfford(OutInstallCost))
	{
		return MakeRejectedDecision(
			EFFZSPlacementFailure::InsufficientFunds,
			FText::Format(
				LOCTEXT("InsufficientGold", "시설 설치비 {0} Gold가 필요하지만 현재 잔액은 {1} Gold입니다."),
				FText::AsNumber(OutInstallCost),
				FText::AsNumber(Progression->GetGold())));
	}
	OutValidatedGold = Progression->GetGold();

	return FFFZSPolicyDecision();
}

FFFZSPolicyDecision
UFF_ZoneSystemEconomyBridgeSubsystem::PreValidateFFZoneSystemChange_Implementation(
	const FFFZSPolicyContext& Context) const
{
	if (Context.Command.Type != EFFZSCommandType::Install)
	{
		return FFFZSPolicyDecision();
	}

	int32 InstallCost = 0;
	FName MatchedCostRow = NAME_None;
	int32 ValidatedGold = 0;
	FString CostTablePath;
	return ValidateInstallCost(
		Context, InstallCost, MatchedCostRow, ValidatedGold, CostTablePath);
}

FFFZSPolicyDecision
UFF_ZoneSystemEconomyBridgeSubsystem::ApplyTentativeFFZoneSystemChange_Implementation(
	const FFFZSPolicyContext& Context)
{
	if (Context.Command.Type != EFFZSCommandType::Install)
	{
		return FFFZSPolicyDecision();
	}

	int32 InstallCost = 0;
	FName MatchedCostRow = NAME_None;
	int32 ValidatedGold = 0;
	FString CostTablePath;
	const FFFZSPolicyDecision Decision = ValidateInstallCost(
		Context, InstallCost, MatchedCostRow, ValidatedGold, CostTablePath);
	if (!Decision.bAllowed)
	{
		UE_LOG(LogFestaFiestaEconomy, Warning,
			TEXT("[경제] 시설 설치비 예약 거부: CommandId=%s, 사유=%s"),
			*Context.Command.CommandId.ToString(),
			*Decision.Message.ToString());
		return Decision;
	}

	if (PendingInstallCosts.Contains(Context.Command.CommandId)
		|| PendingMatchedCostRows.Contains(Context.Command.CommandId)
		|| PendingValidatedGold.Contains(Context.Command.CommandId)
		|| PendingCostTablePaths.Contains(Context.Command.CommandId))
	{
		UE_LOG(LogFestaFiestaEconomy, Error,
			TEXT("[경제] 같은 CommandId의 시설 설치비 예약이 이미 존재합니다: CommandId=%s"),
			*Context.Command.CommandId.ToString());
		return MakeRejectedDecision(
			EFFZSPlacementFailure::InternalError,
			LOCTEXT("DuplicatePendingCost", "같은 건설 명령의 시설 설치비 예약이 이미 존재합니다."));
	}

	PendingInstallCosts.Add(Context.Command.CommandId, InstallCost);
	PendingMatchedCostRows.Add(Context.Command.CommandId, MatchedCostRow);
	PendingValidatedGold.Add(Context.Command.CommandId, ValidatedGold);
	PendingCostTablePaths.Add(Context.Command.CommandId, CostTablePath);
	UE_LOG(LogFestaFiestaEconomy, Log,
		TEXT("[경제] 시설 설치비 예약: CommandId=%s, 비용표=%s, 매칭 행=%s, 비용=%d Gold (자금 변경 없음)"),
		*Context.Command.CommandId.ToString(),
		*CostTablePath,
		*MatchedCostRow.ToString(),
		InstallCost);

	return FFFZSPolicyDecision();
}

bool UFF_ZoneSystemEconomyBridgeSubsystem::ResolveDemolitionRefund(
	const FFFZSPolicyContext& Context,
	int32& OutRefund,
	FName& OutMatchedCostRow,
	FString& OutFailureReason) const
{
	OutRefund = 0;
	OutMatchedCostRow = NAME_None;
	OutFailureReason.Reset();

	// Remove 명령은 FacilityInstanceId만 싣고 오므로 비용 키는 권위 액터의 Spec에서 읽는다.
	const AFFZSFacilityActor* Facility = Context.FacilityActor;
	if (!Facility)
	{
		OutFailureReason = TEXT("철거 대상 시설 액터가 없습니다.");
		return false;
	}

	const UFF_FestivalProgressionSubsystem* Progression = ResolveFestivalProgression();
	const UFF_FestivalDefinition* Festival = Progression ? Progression->GetDefinition() : nullptr;
	if (!Festival || !Progression->IsFestivalStarted())
	{
		OutFailureReason = TEXT("축제가 시작되지 않아 환급 비율을 확인할 수 없습니다.");
		return false;
	}

	const UDataTable* CostTable = Festival->FacilityCostTable.Get();
	if (!CostTable)
	{
		OutFailureReason = TEXT("축제 정의에 시설 비용 DataTable이 없습니다.");
		return false;
	}

	int32 InstallCost = 0;
	if (!FFF_FacilityCostResolver::FindInstallCost(
		Facility->GetFacilitySpec(), CostTable, InstallCost, OutMatchedCostRow, OutFailureReason))
	{
		return false;
	}

	OutRefund = FFF_FacilityCostResolver::CalculateDemolitionRefund(
		InstallCost,
		Festival->FacilityDemolitionRefundRate);
	return true;
}

void UFF_ZoneSystemEconomyBridgeSubsystem::CommitDemolitionRefund(const FFFZSPolicyContext& Context)
{
	int32 Refund = 0;
	FName MatchedCostRow = NAME_None;
	FString FailureReason;
	if (!ResolveDemolitionRefund(Context, Refund, MatchedCostRow, FailureReason))
	{
		// 철거는 이미 확정됐다. 여기서 되돌릴 수 없으므로 환급 0으로 두고 원인만 남긴다.
		UE_LOG(LogFestaFiestaEconomy, Warning,
			TEXT("[경제] 시설 철거 환급을 계산하지 못해 0 Gold를 환급합니다: CommandId=%s, 사유=%s"),
			*Context.Command.CommandId.ToString(),
			*FailureReason);
		return;
	}

	UFF_FestivalProgressionSubsystem* Progression = ResolveFestivalProgression();
	if (!Progression)
	{
		UE_LOG(LogFestaFiestaEconomy, Error,
			TEXT("[경제] 시설 철거 환급 중 축제 경제 서브시스템을 찾을 수 없습니다: CommandId=%s, 환급=%d Gold"),
			*Context.Command.CommandId.ToString(),
			Refund);
		return;
	}

	Progression->Refund(Refund);
	UE_LOG(LogFestaFiestaEconomy, Log,
		TEXT("[경제] 시설 철거 환급 완료: CommandId=%s, 매칭 행=%s, 환급=%d Gold, 잔액=%d Gold"),
		*Context.Command.CommandId.ToString(),
		*MatchedCostRow.ToString(),
		Refund,
		Progression->GetGold());
}

void UFF_ZoneSystemEconomyBridgeSubsystem::CommitFFZoneSystemChange_Implementation(
	const FFFZSPolicyContext& Context)
{
	if (Context.Command.Type == EFFZSCommandType::Remove)
	{
		CommitDemolitionRefund(Context);
		return;
	}
	if (Context.Command.Type != EFFZSCommandType::Install)
	{
		return;
	}

	const int32* PendingCost = PendingInstallCosts.Find(Context.Command.CommandId);
	const FName* PendingCostRow = PendingMatchedCostRows.Find(Context.Command.CommandId);
	const int32* ValidatedGold = PendingValidatedGold.Find(Context.Command.CommandId);
	const FString* PendingCostTablePath = PendingCostTablePaths.Find(Context.Command.CommandId);
	const bool bHasCompleteReservation = PendingCost && PendingCostRow && ValidatedGold
		&& PendingCostTablePath;
	ensureAlwaysMsgf(
		bHasCompleteReservation,
		TEXT("확정할 시설 설치비 예약이 완전하지 않습니다. CommandId=%s"),
		*Context.Command.CommandId.ToString());
	if (!bHasCompleteReservation)
	{
		UE_LOG(LogFestaFiestaEconomy, Error,
			TEXT("[경제] 확정할 시설 설치비 예약을 찾을 수 없습니다: CommandId=%s"),
			*Context.Command.CommandId.ToString());
		PendingInstallCosts.Remove(Context.Command.CommandId);
		PendingMatchedCostRows.Remove(Context.Command.CommandId);
		PendingValidatedGold.Remove(Context.Command.CommandId);
		PendingCostTablePaths.Remove(Context.Command.CommandId);
		return;
	}

	// TrySpend의 OnGoldChanged 콜백보다 먼저 예약을 제거해 확정 처리를 정확히 한 번으로 제한한다.
	const int32 InstallCost = *PendingCost;
	const FName MatchedCostRow = *PendingCostRow;
	const int32 GoldAtValidation = *ValidatedGold;
	const FString CostTablePath = *PendingCostTablePath;
	PendingInstallCosts.Remove(Context.Command.CommandId);
	PendingMatchedCostRows.Remove(Context.Command.CommandId);
	PendingValidatedGold.Remove(Context.Command.CommandId);
	PendingCostTablePaths.Remove(Context.Command.CommandId);

	UFF_FestivalProgressionSubsystem* Progression = ResolveFestivalProgression();
	ensureAlwaysMsgf(
		Progression != nullptr,
		TEXT("시설 설치비 Commit 중 축제 경제 서브시스템이 사라졌습니다. CommandId=%s, 비용 키=%s, 비용=%d Gold"),
		*Context.Command.CommandId.ToString(),
		*MatchedCostRow.ToString(),
		InstallCost);
	if (!Progression)
	{
		UE_LOG(LogFestaFiestaEconomy, Error,
			TEXT("[경제] 시설 설치 확정 중 축제 경제 서브시스템을 찾을 수 없습니다: CommandId=%s, 비용표=%s, 매칭 행=%s, 비용=%d Gold"),
			*Context.Command.CommandId.ToString(),
			*CostTablePath,
			*MatchedCostRow.ToString(),
			InstallCost);
		return;
	}

	ensureAlwaysMsgf(
		Progression->GetGold() == GoldAtValidation,
		TEXT("ApplyTentative와 Commit 사이에 Gold가 변경되었습니다. CommandId=%s, 검증 잔액=%d, 현재 잔액=%d"),
		*Context.Command.CommandId.ToString(),
		GoldAtValidation,
		Progression->GetGold());

	const bool bSpent = Progression->TrySpend(InstallCost, EFF_SpendReason::FacilityInstall);
	ensureAlwaysMsgf(
		bSpent,
		TEXT("검증을 통과한 시설 설치비 차감에 실패했습니다. CommandId=%s, 비용 키=%s, 비용=%d Gold, 현재 잔액=%d Gold"),
		*Context.Command.CommandId.ToString(),
		*MatchedCostRow.ToString(),
		InstallCost,
		Progression->GetGold());
	if (!bSpent)
	{
		// FFZS는 동기·비재진입이므로 ApplyTentative 이후 잔액 변경은 계약 위반이다.
		UE_LOG(LogFestaFiestaEconomy, Error,
			TEXT("[경제] 검증을 통과한 시설 설치비 차감에 실패했습니다: CommandId=%s, 비용표=%s, 매칭 행=%s, 비용=%d Gold, 현재 잔액=%d Gold"),
			*Context.Command.CommandId.ToString(),
			*CostTablePath,
			*MatchedCostRow.ToString(),
			InstallCost,
			Progression->GetGold());
		return;
	}

	UE_LOG(LogFestaFiestaEconomy, Log,
		TEXT("[경제] 시설 설치비 차감 완료: CommandId=%s, 비용표=%s, 매칭 행=%s, 비용=%d Gold, 잔액=%d Gold"),
		*Context.Command.CommandId.ToString(),
		*CostTablePath,
		*MatchedCostRow.ToString(),
		InstallCost,
		Progression->GetGold());
}

void UFF_ZoneSystemEconomyBridgeSubsystem::RollbackFFZoneSystemChange_Implementation(
	const FFFZSPolicyContext& Context)
{
	const int32* PendingCost = PendingInstallCosts.Find(Context.Command.CommandId);
	const FName* PendingCostRow = PendingMatchedCostRows.Find(Context.Command.CommandId);
	const FString* PendingCostTablePath = PendingCostTablePaths.Find(Context.Command.CommandId);
	if (PendingCost && PendingCostRow && PendingCostTablePath)
	{
		UE_LOG(LogFestaFiestaEconomy, Log,
			TEXT("[경제] 시설 설치비 예약 취소: CommandId=%s, 비용표=%s, 매칭 행=%s, 비용=%d Gold (자금 변경 없음)"),
			*Context.Command.CommandId.ToString(),
			**PendingCostTablePath,
			*PendingCostRow->ToString(),
			*PendingCost);
	}

	PendingInstallCosts.Remove(Context.Command.CommandId);
	PendingMatchedCostRows.Remove(Context.Command.CommandId);
	PendingValidatedGold.Remove(Context.Command.CommandId);
	PendingCostTablePaths.Remove(Context.Command.CommandId);
}

#undef LOCTEXT_NAMESPACE
