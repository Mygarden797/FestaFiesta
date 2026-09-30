// FF_FacilityUpgradeSubsystem.cpp
// 역할: 강화 견적을 만들고 공간·운영 상태·잔액 확인 후 비용 예약과 확정을 연결한다.

#include "Building/FF_FacilityUpgradeSubsystem.h"
#include "Building/FF_FacilityNamingLibrary.h"
#include "Building/Synergy/FF_FacilitySynergySubsystem.h"

#include "Economy/FF_FacilityCostResolver.h"
#include "Economy/FF_ProgramSchedulingSubsystem.h"
#include "Economy/Rules/FF_BoothRevenueRules.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSQueueComponent.h"
#include "FF_FFZSWorldSubsystem.h"
#include "Queue/FF_QueueSubsystem.h"
#include "Subsystems/SubsystemCollection.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"

#define LOCTEXT_NAMESPACE "FFFacilityUpgrade"

namespace
{
	FFFZSPolicyDecision RejectUpgrade(const FText& Message)
	{
		FFFZSPolicyDecision Decision;
		Decision.bAllowed = false;
		Decision.FailureReason = EFFZSPlacementFailure::ExternalPolicyRejected;
		Decision.Message = Message;
		return Decision;
	}
}

void UFF_FacilityUpgradeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	ZoneSystem = Collection.InitializeDependency<UFFZSWorldSubsystem>();
	Collection.InitializeDependency<UFF_ZoneSystemQueueBridgeSubsystem>();
	Collection.InitializeDependency<UFF_ProgramSchedulingSubsystem>();
	Super::Initialize(Collection);
	bPolicyRegistered = ZoneSystem && ZoneSystem->RegisterExternalPolicy(this);
	if (ZoneSystem)
	{
		ZoneSystem->OnWorldChanged.AddDynamic(this, &ThisClass::HandleWorldChanged);
	}
}

void UFF_FacilityUpgradeSubsystem::Deinitialize()
{
	if (UFF_FestivalProgressionSubsystem* Progression = ResolveProgression())
	{
		Progression->RollbackFacilityUpgradeSpend(PendingPurchaseCommand);
	}
	if (ZoneSystem)
	{
		ZoneSystem->OnWorldChanged.RemoveDynamic(this, &ThisClass::HandleWorldChanged);
		if (bPolicyRegistered)
		{
			ZoneSystem->UnregisterExternalPolicy(this);
		}
	}
	PendingPurchaseCommand.Invalidate();
	bPolicyRegistered = false;
	ZoneSystem = nullptr;
	Super::Deinitialize();
}

bool UFF_FacilityUpgradeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::GamePreview;
}

UFF_FestivalProgressionSubsystem* UFF_FacilityUpgradeSubsystem::ResolveProgression() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
}

// 현재 시설과 다음 강화 단계를 비교한다. 표시용 견적이며 이 함수 자체는 돈이나 시설을 바꾸지 않는다.
bool UFF_FacilityUpgradeSubsystem::GetUpgradePreview(
	const FGuid FacilityInstanceId, FFF_FacilityUpgradePreview& OutPreview) const
{
	OutPreview = FFF_FacilityUpgradePreview();
	OutPreview.FacilityInstanceId = FacilityInstanceId;
	const UWorld* World = GetWorld();
	const AFFZSFacilityActor* Facility = ZoneSystem ? ZoneSystem->FindFacility(FacilityInstanceId) : nullptr;
	if (!Facility)
	{
		OutPreview.FailureReason = LOCTEXT("MissingFacility", "선택한 시설을 찾을 수 없습니다.");
		return false;
	}
	OutPreview.CurrentLevel = Facility->GetUpgradeLevel();
	const FFFZSFacilitySpec& BaseSpec = Facility->GetBaseFacilitySpec();
	OutPreview.bHasLevels = BaseSpec.bUsesFacilityLevels;
	OutPreview.CurrentFootprintCentimeters = Facility->GetFacilitySpec().FootprintSizeCells;
	OutPreview.NextFootprintCentimeters = OutPreview.CurrentFootprintCentimeters;
	OutPreview.TargetLevel = OutPreview.CurrentLevel;
	OutPreview.MaxLevel = OutPreview.CurrentLevel;
	const FFFZSQueueRules CurrentRules = Facility->GetEffectiveQueueRules();
	OutPreview.CurrentServiceDurationSeconds = CurrentRules.ServiceDurationSeconds;
	OutPreview.NextServiceDurationSeconds = CurrentRules.ServiceDurationSeconds;
	OutPreview.CurrentThroughputPerMinute = FMath::Min(CurrentRules.ServiceCapacity, CurrentRules.MaxWaitingAgents)
		* 60.f / CurrentRules.ServiceDurationSeconds;
	OutPreview.NextThroughputPerMinute = OutPreview.CurrentThroughputPerMinute;
	const UFF_FacilitySynergySubsystem* Synergies = World ? World->GetSubsystem<UFF_FacilitySynergySubsystem>() : nullptr;
	const float SynergyMultiplier = Synergies ? Synergies->GetRevenueMultiplier(FacilityInstanceId) : 1.f;
	const UFF_FestivalProgressionSubsystem* Progression = ResolveProgression();
	const UFF_FestivalDefinition* Definition = Progression ? Progression->GetDefinition() : nullptr;
	const UDataTable* RevenueTable = Definition ? Definition->GetFacilityRevenueTable() : nullptr;
	const float CommissionRate = Definition ? FFF_BoothRevenueRules::ResolveCommissionRate(
		BaseSpec, Definition->BoothCommissionRate, RevenueTable) : 0.f;
	OutPreview.CurrentRevenueMultiplier = FFF_BoothRevenueRules::ResolveRevenueMultiplier(
		BaseSpec, OutPreview.CurrentLevel, Facility->GetUpgradeRevenueMultiplier(), RevenueTable);
	OutPreview.NextRevenueMultiplier = OutPreview.CurrentRevenueMultiplier;
	int32 RevenuePerUse = 0;
	if (Definition)
	{
		FFF_FacilityNameParts Parts;
		FString NamingError;
		OutPreview.bRevenueAvailable = FFF_BoothRevenueRules::HasFacilityRevenueRow(BaseSpec, RevenueTable)
			|| BaseSpec.bOverrideRevenuePerUseGold || UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
			BaseSpec.Category, BaseSpec.FacilityDefinitionId, Parts, NamingError);
		if (OutPreview.bRevenueAvailable)
		{
			RevenuePerUse = FFF_BoothRevenueRules::ResolveRevenuePerUse(
				BaseSpec, Definition->RevenuePerUseByMinorToken, RevenueTable);
			OutPreview.BaseRevenuePerUseGold = FFF_BoothRevenueRules::ResolveCommission(
				RevenuePerUse, 1, CommissionRate,
				FFF_BoothRevenueRules::ResolveRevenueMultiplier(BaseSpec, 1, 1.f, RevenueTable));
			OutPreview.CurrentRevenuePerUseGold = FFF_BoothRevenueRules::ResolveCommission(
				RevenuePerUse, 1, CommissionRate, static_cast<double>(OutPreview.CurrentRevenueMultiplier) * SynergyMultiplier);
			// Quote representative team sizes from the original inputs. Multiplying the
			// rounded one-person estimate would disagree with actual team payments.
			OutPreview.CurrentRevenueByTeamSizeGold.Reserve(6);
			for (int32 MemberCount = 1; MemberCount <= 6; ++MemberCount)
			{
				OutPreview.CurrentRevenueByTeamSizeGold.Add(FFF_BoothRevenueRules::ResolveCommission(
					RevenuePerUse, MemberCount, CommissionRate,
					static_cast<double>(OutPreview.CurrentRevenueMultiplier) * SynergyMultiplier));
			}
			OutPreview.NextRevenuePerUseGold = OutPreview.CurrentRevenuePerUseGold;
		}
	}
	auto Unavailable = [&OutPreview](const FText& Reason)
	{
		OutPreview.FailureReason = Reason;
		return true;
	};
	if (!OutPreview.bHasLevels)
	{
		return Unavailable(LOCTEXT("NoFacilityLevels", "이 시설은 레벨을 사용하지 않습니다."));
	}
	if (!Definition)
	{
		return Unavailable(LOCTEXT("NotStarted", "축제가 시작된 뒤 강화할 수 있습니다."));
	}
	OutPreview.GoldAfterUpgrade = Progression->GetGold();
	FText DataError;
	if (!FFZSFacilityUpgradeRules::ValidateTiers(BaseSpec.UpgradeTiers, DataError))
	{
		return Unavailable(DataError);
	}
	OutPreview.bSupported = true;
	OutPreview.MaxLevel = FMath::Max(OutPreview.CurrentLevel,
		FFZSFacilityUpgradeRules::GetMaxLevel(BaseSpec.UpgradeTiers));
	const FFFZSFacilityUpgradeTier* Tier = FFZSFacilityUpgradeRules::FindNextTier(
		BaseSpec.UpgradeTiers, OutPreview.CurrentLevel);
	if (!Tier)
	{
		return Unavailable(LOCTEXT("MaxLevel", "최대 강화 단계입니다."));
	}
	OutPreview.TargetLevel = Tier->TargetLevel;
	const bool bChangesSize = Tier->SizeMultiplier != Facility->GetUpgradeSizeMultiplier();
	FFFZSPlacementValidation UpgradePlacement;
	float NextSynergyMultiplier = SynergyMultiplier;
	if (bChangesSize)
	{
		OutPreview.NextFootprintCentimeters = FFZSFacilityUpgradeRules::BuildEffectiveSpec(
			BaseSpec, OutPreview.TargetLevel, Tier->SizeMultiplier).FootprintSizeCells;
		// Growing a corner-anchored footprint moves its centre. Resolve the authoritative
		// candidate once, before a disabled-but-visible quote can return on stage/funds checks.
		// The same validation is reused below; revenue-only upgrades perform no spatial query.
		UpgradePlacement = ZoneSystem->ValidateUpgradePlacement(FacilityInstanceId, Tier->SizeMultiplier);
		if (Synergies && !UpgradePlacement.FootprintOrientedRects.IsEmpty())
		{
			NextSynergyMultiplier = Synergies->GetRevenueMultiplierAt(
				FacilityInstanceId, UpgradePlacement.PlacementTransform.GetLocation());
		}
	}
	const FFFZSQueueRules NextRules = AFFZSFacilityActor::BuildEffectiveQueueRules(
		BaseSpec.QueueRules, Tier->ServiceDurationMultiplier);
	OutPreview.NextServiceDurationSeconds = NextRules.ServiceDurationSeconds;
	OutPreview.NextThroughputPerMinute = FMath::Min(NextRules.ServiceCapacity, NextRules.MaxWaitingAgents)
		* 60.f / NextRules.ServiceDurationSeconds;
	OutPreview.NextRevenueMultiplier = FFF_BoothRevenueRules::ResolveRevenueMultiplier(
		BaseSpec, Tier->TargetLevel, Tier->RevenueMultiplier, RevenueTable);
	if (OutPreview.bRevenueAvailable)
	{
		OutPreview.NextRevenuePerUseGold = FFF_BoothRevenueRules::ResolveCommission(
			RevenuePerUse, 1, CommissionRate, static_cast<double>(OutPreview.NextRevenueMultiplier) * NextSynergyMultiplier);
	}
	int32 InstallCost = 0;
	FName CostRow;
	FString CostError;
	if (Tier->CostMultiplier != 0.f &&
		(!FFF_FacilityCostResolver::FindInstallCost(BaseSpec, Definition->FacilityCostTable.Get(), InstallCost, CostRow, CostError)
			|| !FFFacilityUpgradeRules::ResolveCost(InstallCost, Tier->CostMultiplier, OutPreview.CostGold)))
	{
		return Unavailable(LOCTEXT("InvalidCost", "시설 강화 비용을 확인할 수 없습니다."));
	}
	OutPreview.GoldAfterUpgrade = Progression->GetGold() - OutPreview.CostGold;
	OutPreview.bHasUpgradeQuote = true;
	if (!bPolicyRegistered || World->GetNetMode() == NM_Client)
	{
		return Unavailable(LOCTEXT("Authority", "시설을 관리하는 게임에서 강화해야 합니다."));
	}
	const UGameInstance* GameInstance = World->GetGameInstance();
	const UFF_FestivalStageSubsystem* Stage = GameInstance ? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
	if (!Stage || (Stage->CurrentStage != EFF_FestivalStage::Setup && Stage->CurrentStage != EFF_FestivalStage::MorningCheck)
		|| (ZoneSystem->GetChangePhase() != EFFZSChangePhase::InitialSetup && ZoneSystem->GetChangePhase() != EFFZSChangePhase::MorningCheck))
	{
		return Unavailable(LOCTEXT("WrongStage", "준비 또는 아침 점검 시간에 강화할 수 있습니다."));
	}
	if (Facility->NeedsRelocation())
	{
		return Unavailable(LOCTEXT("Relocation", "시설을 유효한 위치로 옮긴 뒤 강화해 주세요."));
	}
	if (Facility->GetFacilityState() != EFFZSFacilityState::Validated
		&& Facility->GetFacilityState() != EFFZSFacilityState::Closed
		&& Facility->GetFacilityState() != EFFZSFacilityState::MovePending)
	{
		return Unavailable(LOCTEXT("FacilityOpen", "운영을 마치고 닫힌 시설만 강화할 수 있습니다."));
	}
	const UFFZSFacilityQueueComponent* ZoneQueue = Facility->GetQueueComponent();
	const UFF_ZoneSystemQueueBridgeSubsystem* Bridge = World->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>();
	const UFF_QueueSubsystem* Queue = World->GetSubsystem<UFF_QueueSubsystem>();
	const int32 QueueId = Bridge ? Bridge->FindQueueFacilityId(FacilityInstanceId) : INDEX_NONE;
	if (!ZoneQueue || !Bridge || !Queue || QueueId == INDEX_NONE || !Queue->IsFacilityRegistered(QueueId))
	{
		return Unavailable(LOCTEXT("QueueUnavailable", "시설 운영 정보를 준비하지 못했습니다."));
	}
	const FFF_QueueState PartyState = Queue->GetQueueState(QueueId);
	if (ZoneQueue->GetQueueState() != EFFZSQueueState::Closed || ZoneQueue->IsOccupied()
		|| PartyState.OccupiedMemberCount > 0 || PartyState.bIsOpen)
	{
		return Unavailable(LOCTEXT("Occupied", "대기와 이용이 모두 끝나고 시설이 닫힌 뒤 강화할 수 있습니다."));
	}
	const UFF_ProgramSchedulingSubsystem* Scheduling = World->GetSubsystem<UFF_ProgramSchedulingSubsystem>();
	if (!Scheduling || Scheduling->HasScheduledProgramForFacility(FacilityInstanceId))
	{
		return Unavailable(LOCTEXT("ProgramLocked", "오늘 프로그램이 편성된 시설은 강화할 수 없습니다."));
	}
	if (!Progression->CanAfford(OutPreview.CostGold))
	{
		return Unavailable(LOCTEXT("InsufficientGold", "강화에 필요한 Gold가 부족합니다."));
	}
	// Reuse quote geometry; the actual Zone command validates again before committing.
	if (bChangesSize && !UpgradePlacement.bIsValid)
	{
		return Unavailable(UpgradePlacement.Summary.IsEmpty()
			? LOCTEXT("NoUpgradeSpace", "강화 후 시설을 배치할 공간이 부족합니다.") : UpgradePlacement.Summary);
	}
	OutPreview.bCanUpgrade = true;
	return true;
}

// 최신 견적과 사용자가 본 레벨이 일치할 때만 권위 Zone 명령을 보낸다. 중복 구매 중에는 재진입을 막는다.
bool UFF_FacilityUpgradeSubsystem::TryUpgradeFacility(
	const FGuid FacilityInstanceId, const int32 ExpectedCurrentLevel, FText& OutFailureReason)
{
	OutFailureReason = FText::GetEmpty();
	if (!IsInGameThread() || bCommitInFlight || PendingPurchaseCommand.IsValid())
	{
		OutFailureReason = LOCTEXT("InProgress", "시설 강화를 처리하고 있습니다.");
		return false;
	}
	TGuardValue<bool> PurchaseGuard(bCommitInFlight, true);
	FFF_FacilityUpgradePreview Preview;
	if (!GetUpgradePreview(FacilityInstanceId, Preview) || !Preview.bCanUpgrade)
	{
		OutFailureReason = Preview.FailureReason;
		return false;
	}
	if (ExpectedCurrentLevel != Preview.CurrentLevel)
	{
		OutFailureReason = LOCTEXT("StaleLevel", "시설 단계가 바뀌었습니다. 새 강화 내용을 확인해 주세요.");
		return false;
	}
	const AFFZSFacilityActor* Facility = ZoneSystem->FindFacility(FacilityInstanceId);
	const FFFZSFacilityUpgradeTier* Tier = Facility
		? FFZSFacilityUpgradeRules::FindTier(Facility->GetBaseFacilitySpec().UpgradeTiers, Preview.TargetLevel) : nullptr;
	if (!Tier)
	{
		OutFailureReason = LOCTEXT("TierChanged", "시설 강화 설정이 바뀌었습니다. 다시 확인해 주세요.");
		return false;
	}
	FFFZSCommand Command;
	Command.CommandId = FGuid::NewGuid();
	Command.TransactionId = Command.CommandId;
	Command.ExpectedWorldRevision = ZoneSystem->GetWorldRevision();
	Command.Type = EFFZSCommandType::Upgrade;
	Command.FacilityInstanceId = FacilityInstanceId;
	Command.TargetUpgradeLevel = Preview.TargetLevel;
	Command.UpgradeServiceDurationMultiplier = Tier->ServiceDurationMultiplier;
	Command.UpgradeSizeMultiplier = Tier->SizeMultiplier;
	Command.UpgradeRevenueMultiplier = Tier->RevenueMultiplier;
	const FFFZSCommandResult Result = ZoneSystem->ExecuteCommand(Command);
	OutFailureReason = Result.bSucceeded ? FText::GetEmpty() : Result.Message;
	return Result.bSucceeded;
}

FName UFF_FacilityUpgradeSubsystem::GetFFZoneSystemPolicyName_Implementation() const
{
	return TEXT("FestaFiesta.FacilityUpgrade");
}

EFFZSPolicyDomain UFF_FacilityUpgradeSubsystem::GetFFZoneSystemPolicyDomain_Implementation() const
{
	return EFFZSPolicyDomain::Custom;
}

// 외부에서 들어온 강화 명령도 실제 데이터표와 비교해 가격·효과 조작을 차단한다.
FFFZSPolicyDecision UFF_FacilityUpgradeSubsystem::ValidatePurchase(const FFFZSPolicyContext& Context, int32& OutCost) const
{
	OutCost = 0;
	if (Context.Command.Type != EFFZSCommandType::Upgrade)
	{
		return FFFZSPolicyDecision();
	}
	FFF_FacilityUpgradePreview Preview;
	if (!GetUpgradePreview(Context.Command.FacilityInstanceId, Preview) || !Preview.bCanUpgrade)
	{
		return RejectUpgrade(Preview.FailureReason);
	}
	const AFFZSFacilityActor* Facility = ZoneSystem->FindFacility(Context.Command.FacilityInstanceId);
	const FFFZSFacilityUpgradeTier* Tier = Facility
		? FFZSFacilityUpgradeRules::FindTier(Facility->GetBaseFacilitySpec().UpgradeTiers, Preview.TargetLevel) : nullptr;
	if (!Tier || Context.Command.TargetUpgradeLevel != Preview.TargetLevel
		|| Context.Command.UpgradeServiceDurationMultiplier != Tier->ServiceDurationMultiplier
		|| Context.Command.UpgradeSizeMultiplier != Tier->SizeMultiplier
		|| Context.Command.UpgradeRevenueMultiplier != Tier->RevenueMultiplier)
	{
		return RejectUpgrade(LOCTEXT("InvalidPurchase", "시설 강화 요청이 현재 강화 정보와 다릅니다."));
	}
	OutCost = Preview.CostGold;
	return FFFZSPolicyDecision();
}

FFFZSPolicyDecision UFF_FacilityUpgradeSubsystem::PreValidateFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context) const
{
	int32 Cost = 0;
	return ValidatePurchase(Context, Cost);
}

// 강화가 실행되기 직전에 비용을 예약한다. 이후 다른 정책이 실패하면 Rollback이 예약을 돌려준다.
FFFZSPolicyDecision UFF_FacilityUpgradeSubsystem::ApplyTentativeFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context)
{
	int32 Cost = 0;
	const FFFZSPolicyDecision Decision = ValidatePurchase(Context, Cost);
	if (!Decision.bAllowed || Context.Command.Type != EFFZSCommandType::Upgrade)
	{
		return Decision;
	}
	UFF_FestivalProgressionSubsystem* Progression = ResolveProgression();
	if (PendingPurchaseCommand.IsValid() || !Progression || !Progression->ReserveFacilityUpgradeSpend(Context.Command.CommandId, Cost))
	{
		return RejectUpgrade(LOCTEXT("ReserveFailed", "시설 강화 비용을 확보하지 못했습니다."));
	}
	PendingPurchaseCommand = Context.Command.CommandId;
	return FFFZSPolicyDecision();
}

// 권위 시설 변경이 성공했을 때만 예약한 비용을 확정한다. 화면 알림은 월드 변경 확정 뒤 전달한다.
void UFF_FacilityUpgradeSubsystem::CommitFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context)
{
	if (Context.Command.Type == EFFZSCommandType::Upgrade && PendingPurchaseCommand == Context.Command.CommandId)
	{
		if (UFF_FestivalProgressionSubsystem* Progression = ResolveProgression())
		{
			Progression->CommitFacilityUpgradeSpend(Context.Command.CommandId);
		}
	}
}

// 강화 명령 실패 시 같은 명령의 비용 예약만 취소하여 다른 구매에 영향을 주지 않는다.
void UFF_FacilityUpgradeSubsystem::RollbackFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context)
{
	if (Context.Command.Type == EFFZSCommandType::Upgrade && PendingPurchaseCommand == Context.Command.CommandId)
	{
		if (UFF_FestivalProgressionSubsystem* Progression = ResolveProgression())
		{
			Progression->RollbackFacilityUpgradeSpend(Context.Command.CommandId);
		}
		PendingPurchaseCommand.Invalidate();
	}
}

// 완료된 강화의 구매 상태를 먼저 정리한 후 잔액 변경을 방송해 재진입에도 중복 차감되지 않게 한다.
void UFF_FacilityUpgradeSubsystem::HandleWorldChanged(const FFFZSWorldChangeEvent& Event)
{
	if (Event.Type == EFFZSCommandType::Upgrade && PendingPurchaseCommand == Event.CommandId)
	{
		const FGuid CommandId = PendingPurchaseCommand;
		PendingPurchaseCommand.Invalidate();
		if (UFF_FestivalProgressionSubsystem* Progression = ResolveProgression())
		{
			Progression->PublishFacilityUpgradeSpend(CommandId);
		}
	}
}

#undef LOCTEXT_NAMESPACE
