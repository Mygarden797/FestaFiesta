#include "Zone/FF_ZoneSystemPlaytestSubsystem.h"

#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSPlacementPreviewActor.h"
#include "FF_FFZSQueueComponent.h"
#include "FF_FFZSWorldSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"
#include "Zone/FF_ZoneSystemPlaytestHUD.h"

// 이 파일은 관찰 카메라 입력과 FF Zone System 명령을 연결한다.
// 프로젝트의 Enhanced Input 설정을 건드리지 않기 위해 로컬 PlayerController 키 상태를 직접 조회한다.
namespace
{
    class FScopedPlaytestNavigationValidation
    {
    public:
        explicit FScopedPlaytestNavigationValidation(const bool bEnable)
            : Settings(GetMutableDefault<UFFZSDeveloperSettings>())
            , bPreviousValue(Settings ? Settings->bEnableNavigationProjectionChecks : false)
        {
            if (Settings)
            {
                Settings->bEnableNavigationProjectionChecks = bEnable;
            }
        }

        ~FScopedPlaytestNavigationValidation()
        {
            if (Settings)
            {
                Settings->bEnableNavigationProjectionChecks = bPreviousValue;
            }
        }

    private:
        UFFZSDeveloperSettings* Settings = nullptr;
        bool bPreviousValue = false;
    };

    FString PlaytestEnumName(const UEnum* Enum, const int64 Value)
    {
        return Enum ? Enum->GetNameStringByValue(Value) : TEXT("Unknown");
    }

    FFFZSCommand MakePlaytestLifecycleCommand(const EFFZSCommandType Type, const FGuid FacilityId)
    {
        FFFZSCommand Command;
        Command.CommandId = FGuid::NewGuid();
        Command.TransactionId = FGuid::NewGuid();
        Command.ExpectedWorldRevision = -1;
        Command.Type = Type;
        Command.FacilityInstanceId = FacilityId;
        return Command;
    }
}

// 숫자 프리셋, 설치·회전, 시설 생명주기, 대기열과 스냅샷 단축키를 한곳에서 처리한다.
void UFF_ZoneSystemPlaytestSubsystem::HandleInput()
{
    UWorld* World = GetWorld();
    APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
    if (!Controller)
    {
        return;
    }

    const auto Pressed = [Controller](const FKey& Key)
    {
        return Controller->WasInputKeyJustPressed(Key);
    };

    if (Pressed(EKeys::Tab)) ToggleUiInteractionMode();
    if (Pressed(EKeys::H)) ToggleHelp();
    if (Pressed(EKeys::F2)) ToggleWorldDebug();
    if (Pressed(EKeys::F10))
    {
        bHudVisible = !bHudVisible;
        if (HudWidget)
        {
            HudWidget->SetVisibility(bHudVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        }
    }

    if (Pressed(EKeys::One)) SetSelectedPreset(EFFZSBuiltInFacilityPreset::FoodTruckBasic);
    if (Pressed(EKeys::Two)) SetSelectedPreset(EFFZSBuiltInFacilityPreset::ToiletBasic);
    if (Pressed(EKeys::Three)) SetSelectedPreset(EFFZSBuiltInFacilityPreset::InfoBasic);
    if (Pressed(EKeys::Four)) SetSelectedPreset(EFFZSBuiltInFacilityPreset::RestBasic);
    if (Pressed(EKeys::Five)) SetSelectedPreset(EFFZSBuiltInFacilityPreset::PerformanceBasic);
    if (Pressed(EKeys::Six)) SetSelectedPreset(EFFZSBuiltInFacilityPreset::SafetyBasic);
    if (Pressed(EKeys::Seven)) SetSelectedPreset(EFFZSBuiltInFacilityPreset::PhotoSpotBasic);

    if (Pressed(EKeys::Q) || Pressed(EKeys::E))
    {
        CurrentPlacement.QuarterTurns = UFFZSGridLibrary::NormalizeQuarterTurns(
            CurrentPlacement.QuarterTurns + (Pressed(EKeys::E) ? 1 : -1));
        UpdatePlacementFromPlayer();
        RebuildPreviewCommand();
        RefreshPreview();
    }

    if ((!bUiInteractionMode && Pressed(EKeys::LeftMouseButton)) || Pressed(EKeys::I) || Pressed(EKeys::Enter))
    {
        CommitPreview();
    }
    if (Pressed(EKeys::M)) MoveLastFacility();
    if (Pressed(EKeys::Delete) || Pressed(EKeys::BackSpace)) ExecuteLifecycleCommand(EFFZSCommandType::Remove);
    if (Pressed(EKeys::O)) ExecuteLifecycleCommand(EFFZSCommandType::Open);
    if (Pressed(EKeys::C)) ExecuteLifecycleCommand(EFFZSCommandType::Close);
    if (Pressed(EKeys::X)) ExecuteLifecycleCommand(EFFZSCommandType::EmergencyClose);
    if (Pressed(EKeys::B)) ToggleQueueBlocked();
    if (Pressed(EKeys::K)) JoinQueueAgent();
    if (Pressed(EKeys::J)) AdvanceQueueAgents();
    if (Pressed(EKeys::F5)) CapturePlaytestSnapshot();
    if (Pressed(EKeys::F9)) RestorePlaytestSnapshot();
    if (Pressed(EKeys::R)) ResetPlaytest();

    if (Pressed(EKeys::N))
    {
        bEnableFullNavigationValidation = !bEnableFullNavigationValidation;
        RefreshPreview();
        SetStatus(bEnableFullNavigationValidation
            ? TEXT("Full navigation validation enabled.")
            : TEXT("Core playtest mode enabled; navigation projection is scoped off."),
            bEnableFullNavigationValidation ? FColor::Yellow : FColor::Cyan);
    }
}

// 화면 중앙의 최신 지면 타깃과 플러그인 검증을 모두 통과한 경우에만 설치 명령을 커밋한다.
void UFF_ZoneSystemPlaytestSubsystem::CommitPreview()
{
    if (!PlacementPreview)
    {
        return;
    }

    // 카메라는 0.1초 프리뷰 갱신 주기보다 훨씬 빠르게 이동·회전할 수 있다.
    // 클릭 직전에 다시 투영해 명령 좌표가 항상 현재 중앙 표시점과 일치하게 한다.
    UpdatePlacementFromPlayer();
    RefreshPreview();

    if (!bHasPlacementTarget)
    {
        SetStatus(TEXT("Aim the center marker at visible ground before installing."), FColor::Yellow);
        return;
    }
    if (!LastValidation.bIsValid)
    {
        SetStatus(FString::Printf(TEXT("Placement is invalid: %s"), *LastValidation.Summary.ToString()), FColor::Red);
        return;
    }

    FFFZSCommandResult Result;
    {
        const FScopedPlaytestNavigationValidation NavigationScope(bEnableFullNavigationValidation);
        Result = PlacementPreview->CommitPreview();
    }

    if (Result.bSucceeded && Result.FacilityActor)
    {
        LastFacilityId = Result.FacilityActor->GetFacilityInstanceId();
        ApplyFacilityVisualScale(Result.FacilityActor);
        SetStatus(FString::Printf(TEXT("Installed facility. Revision %lld."), Result.WorldRevision), FColor::Green);
        RebuildPreviewCommand();
        RefreshPreview();
        return;
    }

    SetStatus(FString::Printf(TEXT("Install rejected: %s - %s"),
        *PlaytestEnumName(StaticEnum<EFFZSPlacementFailure>(), static_cast<int64>(Result.FailureReason)),
        *Result.Message.ToString()), FColor::Red);
}

// Open, Close, EmergencyClose와 Remove를 동일한 플러그인 명령 경로로 실행한다.
void UFF_ZoneSystemPlaytestSubsystem::ExecuteLifecycleCommand(const EFFZSCommandType Type)
{
    UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
    if (!ZoneSystem || !LastFacilityId.IsValid())
    {
        SetStatus(TEXT("Install or restore a facility first."), FColor::Yellow);
        return;
    }

    const FFFZSCommandResult Result = ZoneSystem->ExecuteCommand(
        MakePlaytestLifecycleCommand(Type, LastFacilityId));
    if (Result.bSucceeded)
    {
        SetStatus(FString::Printf(TEXT("%s succeeded. Revision %lld."),
            *PlaytestEnumName(StaticEnum<EFFZSCommandType>(), static_cast<int64>(Type)),
            Result.WorldRevision), FColor::Green);
        if (Type == EFFZSCommandType::Remove)
        {
            LastFacilityId.Invalidate();
        }
        RefreshPreview();
        return;
    }

    SetStatus(FString::Printf(TEXT("%s rejected: %s - %s"),
        *PlaytestEnumName(StaticEnum<EFFZSCommandType>(), static_cast<int64>(Type)),
        *PlaytestEnumName(StaticEnum<EFFZSPlacementFailure>(), static_cast<int64>(Result.FailureReason)),
        *Result.Message.ToString()), FColor::Red);
}

void UFF_ZoneSystemPlaytestSubsystem::MoveLastFacility()
{
    UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
    if (!ZoneSystem || !LastFacilityId.IsValid())
    {
        SetStatus(TEXT("Install or restore a facility first."), FColor::Yellow);
        return;
    }
    UpdatePlacementFromPlayer();
    if (!bHasPlacementTarget)
    {
        SetStatus(TEXT("Aim the center marker at visible ground before moving a facility."), FColor::Yellow);
        return;
    }

    FFFZSCommand Command = MakePlaytestLifecycleCommand(EFFZSCommandType::Move, LastFacilityId);
    Command.ZoneId = ActiveZoneId;
    Command.Placement = CurrentPlacement;

    FFFZSCommandResult Result;
    {
        const FScopedPlaytestNavigationValidation NavigationScope(bEnableFullNavigationValidation);
        Result = ZoneSystem->ExecuteCommand(Command);
    }
    if (Result.bSucceeded)
    {
        ApplyFacilityVisualScale(Result.FacilityActor);
        SetStatus(FString::Printf(TEXT("Moved facility. Revision %lld."), Result.WorldRevision), FColor::Green);
        RefreshPreview();
        return;
    }

    SetStatus(FString::Printf(TEXT("Move rejected: %s - %s"),
        *PlaytestEnumName(StaticEnum<EFFZSPlacementFailure>(), static_cast<int64>(Result.FailureReason)),
        *Result.Message.ToString()), FColor::Red);
}

void UFF_ZoneSystemPlaytestSubsystem::ToggleQueueBlocked()
{
    UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
    AFFZSFacilityActor* Facility = GetLastFacility();
    const UFFZSFacilityQueueComponent* Queue = Facility ? Facility->GetQueueComponent() : nullptr;
    if (!ZoneSystem || !Facility || !Queue)
    {
        SetStatus(TEXT("Install a facility first."), FColor::Yellow);
        return;
    }

    const bool bBlock = Queue->GetQueueState() != EFFZSQueueState::Blocked;
    const bool bSucceeded = ZoneSystem->SetFacilityQueueBlocked(LastFacilityId, bBlock);
    SetStatus(bSucceeded
        ? (bBlock ? TEXT("Queue blocked.") : TEXT("Queue unblocked."))
        : TEXT("Queue block state change was rejected."),
        bSucceeded ? FColor::Green : FColor::Red);
}

void UFF_ZoneSystemPlaytestSubsystem::JoinQueueAgent()
{
    UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
    if (!ZoneSystem || !LastFacilityId.IsValid())
    {
        SetStatus(TEXT("Install and open a facility first."), FColor::Yellow);
        return;
    }

    const FName AgentId(*FString::Printf(TEXT("PlaytestAgent_%02d"), NextAgentNumber++));
    FGuid TicketId;
    const EFFZSQueueJoinResult Result = ZoneSystem->JoinFacilityQueue(
        LastFacilityId, AgentId, nullptr, TicketId);
    SetStatus(FString::Printf(TEXT("Queue join %s: %s"), *AgentId.ToString(),
        *PlaytestEnumName(StaticEnum<EFFZSQueueJoinResult>(), static_cast<int64>(Result))),
        Result == EFFZSQueueJoinResult::Joined ? FColor::Green : FColor::Red);
}

// 대기열 에이전트는 퇴장 → 서비스 지점 → 대기 슬롯 순으로 아직 도착하지 않은 다음 단계만 진행한다.
void UFF_ZoneSystemPlaytestSubsystem::AdvanceQueueAgents()
{
    UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
    AFFZSFacilityActor* Facility = GetLastFacility();
    const UFFZSFacilityQueueComponent* Queue = Facility ? Facility->GetQueueComponent() : nullptr;
    if (!ZoneSystem || !Queue)
    {
        SetStatus(TEXT("Install and open a facility first."), FColor::Yellow);
        return;
    }

    const FFFZSQueueSnapshot Snapshot = Queue->CaptureSnapshot();
    int32 AdvancedCount = 0;
    for (const FFFZSQueueEntrySnapshot& Entry : Snapshot.Entries)
    {
        bool bAdvanced = false;
        if (Entry.bExiting && Entry.ExitIndex != INDEX_NONE)
        {
            bAdvanced = ZoneSystem->ReportExitPointArrival(
                Entry.AgentId, Queue->GetExitWorldLocation(Entry.ExitIndex));
        }
        else if (Entry.ServiceIndex != INDEX_NONE && !Entry.bArrivedAtServicePoint)
        {
            bAdvanced = ZoneSystem->ReportServicePointArrival(
                Entry.AgentId, Queue->GetServiceWorldLocation(Entry.ServiceIndex));
        }
        else if (Entry.AssignedQueueIndex != INDEX_NONE && !Entry.bArrivedAtAssignedSlot)
        {
            bAdvanced = ZoneSystem->ReportQueueSlotArrival(
                Entry.AgentId, Queue->GetQueueSlotWorldLocation(Entry.AssignedQueueIndex));
        }
        AdvancedCount += bAdvanced ? 1 : 0;
    }

    SetStatus(AdvancedCount > 0
        ? FString::Printf(TEXT("Advanced %d queue agent(s)."), AdvancedCount)
        : TEXT("No arrival step is pending; allow service time to elapse."),
        AdvancedCount > 0 ? FColor::Green : FColor::Yellow);
}

// SavedSnapshot은 사용자가 F5/F9로 저장·복원하는 작업 지점이며 InitialSnapshot과 별개다.
void UFF_ZoneSystemPlaytestSubsystem::CapturePlaytestSnapshot()
{
    if (UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem())
    {
        SavedSnapshot = ZoneSystem->CaptureWorldSnapshot();
        bHasSavedSnapshot = true;
        SetStatus(FString::Printf(TEXT("Captured snapshot v%d at revision %lld."),
            SavedSnapshot.SchemaVersion, SavedSnapshot.WorldRevision), FColor::Green);
    }
}

void UFF_ZoneSystemPlaytestSubsystem::RestorePlaytestSnapshot()
{
    UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
    if (!ZoneSystem || !bHasSavedSnapshot)
    {
        SetStatus(TEXT("Press F5 or use Save Snapshot first."), FColor::Yellow);
        return;
    }

	TArray<FText> Errors;
	UFF_ZoneSystemQueueBridgeSubsystem* Integration =
		GetWorld() ? GetWorld()->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>() : nullptr;
	if (Integration && Integration->RestoreZoneWorldSnapshotAndSynchronize(SavedSnapshot, Errors))
    {
        LastFacilityId = SavedSnapshot.Facilities.IsEmpty()
            ? FGuid()
            : SavedSnapshot.Facilities.Last().FacilityInstanceId;
        ApplyAllFacilityVisuals();
        RefreshPreview();
        SetStatus(FString::Printf(TEXT("Restored snapshot at revision %lld."),
            SavedSnapshot.WorldRevision), FColor::Green);
        return;
    }

    SetStatus(Errors.IsEmpty()
        ? TEXT("Snapshot restore failed.")
        : FString::Printf(TEXT("Snapshot restore failed: %s"), *Errors[0].ToString()), FColor::Red);
}

// InitialSnapshot은 월드 시작 직후의 빈 테스트 상태로, R 키 전체 초기화에만 사용한다.
void UFF_ZoneSystemPlaytestSubsystem::ResetPlaytest()
{
    UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
    if (!ZoneSystem)
    {
        return;
    }

	TArray<FText> Errors;
	UFF_ZoneSystemQueueBridgeSubsystem* Integration =
		GetWorld() ? GetWorld()->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>() : nullptr;
	if (!Integration || !Integration->RestoreZoneWorldSnapshotAndSynchronize(InitialSnapshot, Errors))
    {
        SetStatus(Errors.IsEmpty()
            ? TEXT("Playtest reset failed.")
            : FString::Printf(TEXT("Playtest reset failed: %s"), *Errors[0].ToString()), FColor::Red);
        return;
    }

    LastFacilityId.Invalidate();
    NextAgentNumber = 1;
    UpdatePlacementFromPlayer();
    RebuildPreviewCommand();
    RefreshPreview();
    SetStatus(TEXT("Playtest reset to its empty initial state."), FColor::Green);
}



