#include "FF_FFZSWorldSubsystem.h"

#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSQueueGeometry.h"

#define LOCTEXT_NAMESPACE "FFZSWorldQueueLayout"

namespace
{
    bool BuildQueueLayoutCandidate(const AFFZSFacilityActor& Facility, const FFFZSQueueSettings& Settings, const EFFZSQueueLayout Layout,
        FFFZSCommand& OutCommand, FText& OutError)
    {
        FFFZSFacilitySpec Base = Facility.GetBaseFacilitySpec();
        // Build the scaled authored shell once, then generate with the requested player
        // settings so a rejected U width/capacity retains its actionable geometry error.
        Base.QueueLayout = EFFZSQueueLayout::Authored;
        Base.QueueSettings.bEnabled = false;
        OutCommand.Type = EFFZSCommandType::Move;
        OutCommand.FacilityInstanceId = Facility.GetFacilityInstanceId();
        OutCommand.ZoneId = Facility.GetZoneId();
        OutCommand.Placement = Facility.GetGridPlacement();
        OutCommand.FacilitySpec = FFZSFacilityUpgradeRules::BuildEffectiveSpec(
            Base, Facility.GetUpgradeLevel(), Facility.GetUpgradeSizeMultiplier());
        OutCommand.FacilitySpec.QueueLayout = Layout;
        OutCommand.FacilitySpec.QueueSettings = Settings;
        OutCommand.bChangeQueueLayout = true;
        OutCommand.QueueLayout = Layout;
        OutCommand.bChangeQueueSettings = true;
        OutCommand.QueueSettings = Settings;
        if (!FFFZSQueueGeometry::ApplySelectedLayout(OutCommand.FacilitySpec, OutError)) return false;
        TArray<FText> Errors;
        if (!OutCommand.FacilitySpec.IsValid(Errors))
        {
            OutError = Errors.IsEmpty() ? LOCTEXT("InvalidSettings", "선택한 대기열 설정으로 배치할 수 없습니다.") : Errors[0];
            return false;
        }
        return true;
    }
}

FFFZSPlacementValidation UFFZSWorldSubsystem::ValidateQueueLayout(
    const FGuid FacilityInstanceId, const EFFZSQueueLayout Layout) const
{
    const AFFZSFacilityActor* Facility = FindFacility(FacilityInstanceId);
    return ValidateQueueSettings(FacilityInstanceId, Facility ? Facility->GetBaseFacilitySpec().QueueSettings : FFFZSQueueSettings(), Layout);
}

FFFZSPlacementValidation UFFZSWorldSubsystem::ValidateQueueSettings(
    const FGuid FacilityInstanceId, const FFFZSQueueSettings& Settings, const EFFZSQueueLayout Layout) const
{
    FFFZSPlacementValidation Result;
    const AFFZSFacilityActor* Facility = FindFacility(FacilityInstanceId);
    if (!Facility)
    {
        Result.AddFailure(EFFZSPlacementFailure::FacilityNotFound,
            LOCTEXT("MissingFacility", "대기열을 바꿀 시설을 선택하세요."));
        return Result;
    }
    if (!Facility->CanMoveOrRemove())
    {
        Result.AddFailure(EFFZSPlacementFailure::FacilityOccupied,
            LOCTEXT("Occupied", "시설을 닫고 모든 손님이 퇴장한 뒤 대기열을 바꿀 수 있습니다."));
        return Result;
    }
    FFFZSCommand Candidate;
    FText Error;
    if (!BuildQueueLayoutCandidate(*Facility, Settings, Layout, Candidate, Error))
    {
        Result.AddFailure(EFFZSPlacementFailure::DataInvalid, Error);
        return Result;
    }
    // Includes exact reservations, boundary/ground/world obstacles, Entry/Service/Exit and
    // every queue slot's full navigation path from Entry under the project's nav settings.
    Result = ValidatePlacementLocal(Candidate, Facility);
    if (Result.bIsValid)
    {
        FFFZSPolicyContext Context;
        Context.Command = Candidate;
        Context.Validation = Result;
        Context.FacilityActor = const_cast<AFFZSFacilityActor*>(Facility);
        Context.CurrentWorldRevision = WorldRevision;
        FFFZSPolicyDecision Decision;
        if (!RunPolicyPreValidation(Context, Decision))
        {
            Result.AddFailure(Decision.FailureReason == EFFZSPlacementFailure::None
                    ? EFFZSPlacementFailure::ExternalPolicyRejected : Decision.FailureReason,
                Decision.Message.IsEmpty() ? LOCTEXT("PolicyRejected", "이 대기열 형태를 배치할 공간이나 연결된 통로가 부족합니다.") : Decision.Message);
        }
    }
    return Result;
}

bool UFFZSWorldSubsystem::SetFacilityQueueLayout(
    const FGuid FacilityInstanceId, const EFFZSQueueLayout Layout, FText& OutError)
{
    const AFFZSFacilityActor* Facility = FindFacility(FacilityInstanceId);
    return SetFacilityQueueSettings(FacilityInstanceId, Facility ? Facility->GetBaseFacilitySpec().QueueSettings : FFFZSQueueSettings(), Layout, OutError);
}

bool UFFZSWorldSubsystem::SetFacilityQueueSettings(
    const FGuid FacilityInstanceId, const FFFZSQueueSettings& Settings, const EFFZSQueueLayout Layout, FText& OutError)
{
    OutError = FText::GetEmpty();
    if (IsAuthorityMutationBlocked() || WorldRevision >= MAX_int64 - 1)
    {
        OutError = LOCTEXT("Busy", "시설 변경을 처리 중입니다. 잠시 후 다시 시도해 주세요.");
        return false;
    }
    if (CurrentPhase != EFFZSChangePhase::InitialSetup && CurrentPhase != EFFZSChangePhase::MorningCheck)
    {
        OutError = LOCTEXT("PreparationOnly", "대기열 형태는 준비 시간에만 바꿀 수 있습니다.");
        return false;
    }
    AFFZSFacilityActor* Facility = FindFacility(FacilityInstanceId);
    if (!Facility || !Facility->CanMoveOrRemove())
    {
        OutError = LOCTEXT("NotEditable", "선택한 시설을 닫고 모든 손님이 퇴장한 뒤 다시 시도하세요.");
        return false;
    }
    if (Facility->GetQueueLayout() == Layout && Facility->GetBaseFacilitySpec().QueueSettings == Settings)
    {
        return true;
    }
    TArray<FText> IntegrationErrors;
    if (GetDefault<UFFZSDeveloperSettings>()->bRequireProductionPolicyDomains &&
        !ValidateIntegrationReadiness(IntegrationErrors, true))
    {
        OutError = IntegrationErrors.IsEmpty() ? LOCTEXT("IntegrationNotReady", "시설 변경을 위한 운영 시스템이 준비되지 않았습니다.") : IntegrationErrors[0];
        return false;
    }
    FFFZSCommand Command;
    if (!BuildQueueLayoutCandidate(*Facility, Settings, Layout, Command, OutError))
    {
        return false;
    }
    Command.CommandId = FGuid::NewGuid();
    Command.TransactionId = FGuid::NewGuid();
    Command.ExpectedWorldRevision = WorldRevision;
    TGuardValue<FGuid> CommandGuard(InFlightCommandId, Command.CommandId);
    TGuardValue<FString> FingerprintGuard(InFlightCommandFingerprint, UFFZSGridLibrary::BuildCommandFingerprint(Command));
    const FFFZSPlacementValidation Validation = ValidateQueueSettings(FacilityInstanceId, Settings, Layout);
    if (!Validation.bIsValid)
    {
        OutError = Validation.Summary;
        return false;
    }

    const FFFZSFacilityInstanceSnapshot Before = Facility->CaptureInstanceSnapshot();
    FFFZSPolicyContext Context;
    Context.Command = Command;
    Context.Validation = Validation;
    Context.FacilityActor = Facility;
    Context.CurrentWorldRevision = WorldRevision;
    FFFZSPolicyDecision Decision;
    TArray<TWeakObjectPtr<UObject>> AppliedPolicies;
    if (!ApplyTentativePolicies(Context, AppliedPolicies, Decision))
    {
        OutError = Decision.Message.IsEmpty() ? LOCTEXT("ReserveRejected", "대기열 변경을 확정할 수 없습니다.") : Decision.Message;
        return false;
    }
    ReleaseFacilityRegions(*Facility);
    if (!Facility->ApplyQueueSettings(Settings, Layout))
    {
        OccupyFacilityRegions(*Facility);
        RollbackPolicies(Context, AppliedPolicies);
        OutError = LOCTEXT("ApplyFailed", "대기열을 변경할 수 없어 기존 배치를 유지했습니다.");
        return false;
    }
    OccupyFacilityRegions(*Facility);
    ++FacilityRegistryRevision;
    CommitPolicies(Context, AppliedPolicies);
    FinalizeSuccess(Command, Facility, Facility->GetZoneId());
    RecordConstructionEdit(Command, Before);
    const TOptional<FFFZSWorldChangeEvent> EventToBroadcast = DeferredWorldChangeEvent;
    DeferredWorldChangeEvent.Reset();
    if (EventToBroadcast.IsSet())
    {
        OnWorldChanged.Broadcast(EventToBroadcast.GetValue());
    }
    return true;
}

FFFZSCommandResult UFFZSWorldSubsystem::ResetAllFacilityQueueSettingsToDefaults()
{
    if (IsAuthorityMutationBlocked() || bIsResettingAllQueueSettings)
        return MakeFailure(EFFZSPlacementFailure::InternalError, LOCTEXT("ResetBusy", "다른 시설 변경이 끝난 뒤 다시 시도하세요."));
    if (CurrentPhase != EFFZSChangePhase::InitialSetup && CurrentPhase != EFFZSChangePhase::MorningCheck)
        return MakeFailure(EFFZSPlacementFailure::WrongStage, LOCTEXT("ResetPhase", "대기열 통일은 준비 시간에만 할 수 있습니다."));
    FFFZSQueueSettings Defaults;
    Defaults.bEnabled = true;
    TArray<FGuid> Ids;
    FacilitiesById.GetKeys(Ids);
    Ids.Sort([](const FGuid& A, const FGuid& B) { return A < B; });
    const FGuid TransactionId = FGuid::NewGuid();
    TArray<FFFZSCommand> Commands;
    for (const FGuid& Id : Ids)
    {
        const AFFZSFacilityActor* Facility = FindFacility(Id);
        if (!Facility || !Facility->GetFacilitySpec().UsesServiceQueue() ||
            (Facility->GetBaseFacilitySpec().QueueSettings == Defaults && Facility->GetQueueLayout() == EFFZSQueueLayout::Authored)) continue;
        if (Commands.Num() == 4096)
            return MakeFailure(EFFZSPlacementFailure::DataInvalid, LOCTEXT("ResetLimit", "대기열을 통일할 시설은 한 번에 최대 4096개입니다. 아무것도 변경하지 않았습니다."));
        FFFZSCommand& Command = Commands.AddDefaulted_GetRef();
        Command.CommandId = FGuid::NewGuid();
        Command.TransactionId = TransactionId;
        Command.ExpectedWorldRevision = WorldRevision;
        Command.Type = EFFZSCommandType::Move;
        Command.FacilityInstanceId = Id;
        Command.ZoneId = Facility->GetZoneId();
        Command.Placement = Facility->GetGridPlacement();
        Command.bChangeQueueLayout = true;
        Command.QueueLayout = EFFZSQueueLayout::Authored;
        Command.bChangeQueueSettings = true;
        Command.QueueSettings = Defaults;
    }
    if (Commands.IsEmpty())
    {
        FFFZSCommandResult Result;
        Result.bSucceeded = true;
        Result.WorldRevision = WorldRevision;
        Result.Message = LOCTEXT("AlreadyUnified", "모든 시설이 공통 대기열 설정을 사용하고 있습니다.");
        return Result;
    }
    TGuardValue<bool> ResetGuard(bIsResettingAllQueueSettings, true);
    return ProcessConstructionBatch(Commands, nullptr, false);
}

#undef LOCTEXT_NAMESPACE
