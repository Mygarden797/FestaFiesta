// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/FF_ZoneSystemEconomyBridgeSubsystem.h"

#include "Economy/FF_EconomyLog.h"
#include "Economy/FF_FacilityCostResolver.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Save/FF_SaveSubsystem.h"
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

	PendingReceipts.Reset();
	CommittedReceipts.Reset();
	GoldBeforePendingCommits.Reset();
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

bool UFF_ZoneSystemEconomyBridgeSubsystem::IsRestoringSave() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UFF_SaveSubsystem* Save = GameInstance
		? GameInstance->GetSubsystem<UFF_SaveSubsystem>()
		: nullptr;
	return Save && Save->IsRestoring();
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

FFFZSPolicyDecision UFF_ZoneSystemEconomyBridgeSubsystem::ResolveConstructionReceipt(
	const FFFZSPolicyContext& Context, FFF_ConstructionEconomyReceipt& OutReceipt) const
{
	OutReceipt = {};
	if (IsRestoringSave()) return {};
	FGuid OriginalId;
	bool bUndo = false;
	if (ZoneSystem && ZoneSystem->GetConstructionReplaySource(Context.Command.CommandId, OriginalId, bUndo))
	{
		const FFF_ConstructionEconomyReceipt* Original = CommittedReceipts.Find(OriginalId);
		if (!Original)
			return MakeRejectedDecision(EFFZSPlacementFailure::DataInvalid,
				LOCTEXT("MissingBuildReceipt", "원래 건축 비용 기록을 찾을 수 없어 복구할 수 없습니다."));
		OutReceipt.InstallSpend = bUndo ? -Original->InstallSpend : Original->InstallSpend;
		OutReceipt.DemolitionRefund = bUndo ? -Original->DemolitionRefund : Original->DemolitionRefund;
		return {};
	}
	if (Context.Command.Type == EFFZSCommandType::Install)
	{
		FName Row; int32 Gold = 0; FString Table;
		return ValidateInstallCost(Context, OutReceipt.InstallSpend, Row, Gold, Table);
	}
	if (Context.Command.Type == EFFZSCommandType::Remove)
	{
		FName Row; FString Error;
		ResolveDemolitionRefund(Context, OutReceipt.DemolitionRefund, Row, Error);
		// Record exactly what can enter the wallet, including an int32 upper bound.
		if (const UFF_FestivalProgressionSubsystem* Progression = ResolveFestivalProgression())
			OutReceipt.DemolitionRefund = FMath::Min(OutReceipt.DemolitionRefund, MAX_int32 - Progression->GetGold());
	}
	return {};
}

FFFZSPolicyDecision UFF_ZoneSystemEconomyBridgeSubsystem::ValidateConstructionReceipt(
	const FFF_ConstructionEconomyReceipt& Receipt) const
{
	int64 Spend = Receipt.InstallSpend;
	int64 Refund = Receipt.DemolitionRefund;
	for (const auto& Entry : PendingReceipts)
	{
		Spend += Entry.Value.InstallSpend;
		Refund += Entry.Value.DemolitionRefund;
	}
	return ValidateConstructionTotals(Spend, Refund);
}

FFFZSPolicyDecision UFF_ZoneSystemEconomyBridgeSubsystem::ValidateConstructionTotals(
	const int64 Spend, const int64 Refund) const
{
	if (Spend == 0 && Refund == 0) return {};
	const UFF_FestivalProgressionSubsystem* Progression = ResolveFestivalProgression();
	if (!Progression || !Progression->CanApplyConstructionDelta(Spend, Refund))
		return MakeRejectedDecision(EFFZSPlacementFailure::InsufficientFunds,
			LOCTEXT("BuildHistoryFunds", "선택한 시설 전체의 비용 또는 철거 환급 회수액을 지불할 수 없습니다. 잔액을 확인하세요."));
	return {};
}

FFFZSPolicyDecision UFF_ZoneSystemEconomyBridgeSubsystem::PreValidateFFZoneSystemChange_Implementation(
	const FFFZSPolicyContext& Context) const
{
	if (ZoneSystem)
	{
		if (const TArray<FFFZSPolicyContext>* Batch = ZoneSystem->GetConstructionBatchPolicyContexts())
		{
			int64 Spend = 0, Refund = 0;
			for (const FFFZSPolicyContext& Member : *Batch)
			{
				FFF_ConstructionEconomyReceipt Receipt;
				const FFFZSPolicyDecision Decision = ResolveConstructionReceipt(Member, Receipt);
				if (!Decision.bAllowed) return Decision;
				Spend += Receipt.InstallSpend;
				Refund += Receipt.DemolitionRefund;
			}
			// This is a read-only full-group calculation. Pending tentative reservations are
			// deliberately excluded; adding them would charge the same batch twice.
			return ValidateConstructionTotals(Spend, Refund);
		}
	}
	FFF_ConstructionEconomyReceipt Receipt;
	const FFFZSPolicyDecision Decision = ResolveConstructionReceipt(Context, Receipt);
	return Decision.bAllowed ? ValidateConstructionReceipt(Receipt) : Decision;
}

FFFZSPolicyDecision UFF_ZoneSystemEconomyBridgeSubsystem::ApplyTentativeFFZoneSystemChange_Implementation(
	const FFFZSPolicyContext& Context)
{
	if (IsRestoringSave()) return {};
	if (PendingReceipts.Contains(Context.Command.CommandId))
		return MakeRejectedDecision(EFFZSPlacementFailure::InternalError,
			LOCTEXT("DuplicateBuildReceipt", "같은 건축 명령의 자금 예약이 이미 있습니다."));
	FFF_ConstructionEconomyReceipt Receipt;
	FFFZSPolicyDecision Decision = ResolveConstructionReceipt(Context, Receipt);
	if (Decision.bAllowed) Decision = ValidateConstructionReceipt(Receipt);
	if (Decision.bAllowed) PendingReceipts.Add(Context.Command.CommandId, Receipt);
	return Decision;
}

void UFF_ZoneSystemEconomyBridgeSubsystem::CommitFFZoneSystemChange_Implementation(
	const FFFZSPolicyContext& Context)
{
	if (IsRestoringSave()) return;
	FFF_ConstructionEconomyReceipt Receipt;
	if (!ensure(PendingReceipts.RemoveAndCopyValue(Context.Command.CommandId, Receipt))) return;
	// All batch reservations have succeeded before any wallet mutation is published.
	if (Receipt.InstallSpend != 0 || Receipt.DemolitionRefund != 0)
	{
		UFF_FestivalProgressionSubsystem* Progression = ResolveFestivalProgression();
		if (!ensure(Progression)) return;
		if (!GoldBeforePendingCommits.IsSet()) GoldBeforePendingCommits = Progression->GetGold();
		if (!ensure(Progression->ApplyConstructionDelta(Receipt.InstallSpend, Receipt.DemolitionRefund, false))) return;
	}
	FGuid OriginalId; bool bUndo = false;
	if (!ZoneSystem || !ZoneSystem->GetConstructionReplaySource(Context.Command.CommandId, OriginalId, bUndo))
		CommittedReceipts.Add(Context.Command.CommandId, Receipt);
	// All members (including free moves) were reserved before Commit. Notify once after the
	// final receipt, so reentrant Gold listeners cannot spend another member's reserved funds.
	if (PendingReceipts.IsEmpty() && GoldBeforePendingCommits.IsSet())
	{
		const int32 PreviousGold = GoldBeforePendingCommits.GetValue();
		GoldBeforePendingCommits.Reset();
		if (UFF_FestivalProgressionSubsystem* Progression = ResolveFestivalProgression())
			Progression->PublishConstructionGoldChange(PreviousGold);
	}
}

void UFF_ZoneSystemEconomyBridgeSubsystem::RollbackFFZoneSystemChange_Implementation(
	const FFFZSPolicyContext& Context)
{
	PendingReceipts.Remove(Context.Command.CommandId);
}

#undef LOCTEXT_NAMESPACE