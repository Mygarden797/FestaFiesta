#include "Zone/FF_ZoneSystemPlaytestSubsystem.h"

#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSPlacementPreviewActor.h"
#include "FF_FFZSProtectedArea.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"

// 월드 시각화, WBP 실패 시 폴백 HUD와 명령행 회귀 스모크를 분리해 둔 구현 파일이다.
DEFINE_LOG_CATEGORY_STATIC(LogFFZonePlaytestRuntime, Log, All);

namespace
{
    // 스모크 중 Navigation 검증 설정을 임시 변경하고 종료 시 원래 값으로 복원한다.
    class FScopedSmokeNavigationValidation
    {
    public:
        explicit FScopedSmokeNavigationValidation(const bool bEnable)
            : Settings(GetMutableDefault<UFFZSDeveloperSettings>())
            , bPreviousValue(Settings ? Settings->bEnableNavigationProjectionChecks : false)
        {
            if (Settings)
            {
                Settings->bEnableNavigationProjectionChecks = bEnable;
            }
        }

        ~FScopedSmokeNavigationValidation()
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

    FString RuntimeEnumName(const UEnum* Enum, const int64 Value)
    {
        return Enum ? Enum->GetNameStringByValue(Value) : TEXT("Unknown");
    }

    FColor RuntimeFacilityStateColor(const EFFZSFacilityState State)
    {
        switch (State)
        {
        case EFFZSFacilityState::Active:
            return FColor::Green;
        case EFFZSFacilityState::Closed:
        case EFFZSFacilityState::MovePending:
            return FColor::Orange;
        case EFFZSFacilityState::Removed:
            return FColor::Silver;
        default:
            return FColor::Cyan;
        }
    }

    FFFZSCommand MakeSmokeLifecycleCommand(const EFFZSCommandType Type, const FGuid FacilityId)
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

// 디버그 색상 규칙: 파랑=Zone 경계, 빨강=보호 구역, 시설=상태색, 노랑=대기열 영역이다.
void UFF_ZoneSystemPlaytestSubsystem::DrawWorldDebug() const
{
    UWorld* World = GetWorld();
    const UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
    if (!World || !ZoneSystem || !TestZone)
    {
        return;
    }

    if (const USplineComponent* Boundary = TestZone->BoundarySpline)
    {
        const int32 PointCount = Boundary->GetNumberOfSplinePoints();
        for (int32 Index = 0; Index < PointCount; ++Index)
        {
            DrawDebugLine(World,
                Boundary->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::World) + FVector(0.0f, 0.0f, 10.0f),
                Boundary->GetLocationAtSplinePoint((Index + 1) % PointCount, ESplineCoordinateSpace::World) + FVector(0.0f, 0.0f, 10.0f),
                FColor::Blue, false, 0.0f, 0, 8.0f);
        }
    }

    if (TestProtectedArea && TestProtectedArea->AreaBounds)
    {
        DrawDebugBox(World,
            TestProtectedArea->AreaBounds->Bounds.Origin,
            TestProtectedArea->AreaBounds->GetScaledBoxExtent(),
            TestProtectedArea->GetActorQuat(),
            FColor::Red, false, 0.0f, 0, 8.0f);
        DrawDebugString(World,
            TestProtectedArea->GetActorLocation() + FVector(0.0f, 0.0f, 250.0f),
            TEXT("NO BUILD - press V"), nullptr, FColor::Red, 0.0f, false, 1.2f);
    }

    const FFFZSWorldSnapshot Snapshot = ZoneSystem->CaptureWorldSnapshot();
    for (const FFFZSFacilityInstanceSnapshot& Instance : Snapshot.Facilities)
    {
        if (Instance.State == EFFZSFacilityState::Removed)
        {
            continue;
        }

        const FColor StateColor = RuntimeFacilityStateColor(Instance.State);
        const float HalfHeight = FMath::Max(10.0f, Instance.FacilitySpec.PlacementObstacleHeightCm * 0.5f);
        const TArray<FFFZSCellRect> FootprintRects = UFFZSGridLibrary::GetFootprintRects(
            Instance.FacilitySpec, Instance.Placement);
        for (const FFFZSCellRect& Rect : FootprintRects)
        {
            FVector Center = TestZone->GetActorTransform().TransformPosition(FVector(
                (static_cast<double>(Rect.Min.X) + Rect.Max.X) * 0.5,
                (static_cast<double>(Rect.Min.Y) + Rect.Max.Y) * 0.5,
                0.0));
            Center.Z = Instance.WorldTransform.GetLocation().Z + HalfHeight;
            DrawDebugBox(World, Center,
                FVector(Rect.GetWidth() * 0.5, Rect.GetHeight() * 0.5, HalfHeight),
                TestZone->GetActorQuat(), StateColor, false, 0.0f, 0, 5.0f);
        }

        const TArray<FFFZSCellRect> QueueRects = UFFZSGridLibrary::GetQueueRects(
            Instance.FacilitySpec, Instance.Placement);
        for (const FFFZSCellRect& Rect : QueueRects)
        {
            FVector Center = TestZone->GetActorTransform().TransformPosition(FVector(
                (static_cast<double>(Rect.Min.X) + Rect.Max.X) * 0.5,
                (static_cast<double>(Rect.Min.Y) + Rect.Max.Y) * 0.5,
                10.0));
            Center.Z = Instance.WorldTransform.GetLocation().Z + 10.0f;
            DrawDebugBox(World, Center,
                FVector(Rect.GetWidth() * 0.5, Rect.GetHeight() * 0.5, 10.0f),
                TestZone->GetActorQuat(), FColor::Yellow, false, 0.0f, 0, 3.0f);
        }

        DrawDebugString(World,
            Instance.WorldTransform.GetLocation() + FVector(0.0f, 0.0f, Instance.FacilitySpec.PlacementObstacleHeightCm + 50.0f),
            FString::Printf(TEXT("%s | %s"), *Instance.FacilitySpec.FacilityDefinitionId.ToString(),
                *RuntimeEnumName(StaticEnum<EFFZSFacilityState>(), static_cast<int64>(Instance.State))),
            nullptr, StateColor, 0.0f, false, 1.0f);
    }
}

// 편집 가능한 WBP를 로드하지 못했을 때만 최소 상태와 단축키를 화면 문자열로 표시한다.
void UFF_ZoneSystemPlaytestSubsystem::DrawHud() const
{
    if (!GEngine)
    {
        return;
    }

    const UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
    const FFFZSReadModel ReadModel = ZoneSystem ? ZoneSystem->BuildReadModel() : FFFZSReadModel{};
    const FString PresetName = RuntimeEnumName(
        StaticEnum<EFFZSBuiltInFacilityPreset>(), static_cast<int64>(SelectedPreset));
    const FString NavigationMode = bEnableFullNavigationValidation
        ? TEXT("FULL NAV")
        : TEXT("CORE (Nav projection scoped off)");
    const FString ValidationText = LastValidation.bIsValid
        ? TEXT("VALID")
        : FString::Printf(TEXT("INVALID: %s"), *LastValidation.Summary.ToString());

    GEngine->AddOnScreenDebugMessage(static_cast<uint64>(0xFF250001), 0.2f, FColor::White,
        FString::Printf(TEXT("FF Zone System | %s | Origin=(%d,%d) Rot=%d | %s"),
            *PresetName, CurrentPlacement.OriginCell.X, CurrentPlacement.OriginCell.Y,
            CurrentPlacement.QuarterTurns, *NavigationMode), false);
    GEngine->AddOnScreenDebugMessage(static_cast<uint64>(0xFF250002), 0.2f,
        LastValidation.bIsValid ? FColor::Green : FColor::Red,
        FString::Printf(TEXT("Preview: %s"), *ValidationText), false);
    GEngine->AddOnScreenDebugMessage(static_cast<uint64>(0xFF250003), 0.2f, FColor::Cyan,
        FString::Printf(TEXT("World revision=%lld | Facilities=%d | Zone=%s"),
            ReadModel.WorldRevision, ReadModel.Facilities.Num(), *ActiveZoneId.ToString()), false);
    GEngine->AddOnScreenDebugMessage(static_cast<uint64>(0xFF250004), 0.2f, StatusColor,
        FString::Printf(TEXT("Status: %s"), *StatusMessage), false);

    if (bShowHelp)
    {
        GEngine->AddOnScreenDebugMessage(static_cast<uint64>(0xFF250005), 0.2f, FColor::Silver,
            TEXT("[WASD/Arrows] Pan  [Shift] Fast pan  [Wheel] Zoom  [RMB drag] Rotate  [Home] Recenter\n")
            TEXT("[Center marker] Target  [LMB/I/Enter] Install  [Q/E] Rotate facility  [1-7] Preset\n")
            TEXT("[M] Move last  [Del] Remove  [O/C/X] Open/Close/Emergency  [K/J/B] Queue\n")
            TEXT("[F5/F9] Snapshot/Restore  [R] Reset  [N] Full Nav  [Tab] HUD  [Esc] Stop PIE"), false);
    }
}

// 최근 명령 결과를 WBP와 폴백 HUD가 같은 방식으로 읽을 수 있도록 한곳에 저장한다.
void UFF_ZoneSystemPlaytestSubsystem::SetStatus(const FString& Message, const FColor& Color)
{
    StatusMessage = Message;
    StatusColor = Color;
    UE_LOG(LogFFZonePlaytestRuntime, Display, TEXT("%s"), *Message);
}

// Zone 등록부터 설치, 운영 상태, 대기열, 스냅샷 복원과 보호 구역 거부까지 한 번에 검증한다.
// 성공·실패 마커를 로그에 남긴 뒤 프로세스 종료 코드를 설정해 자동화에서 판정할 수 있게 한다.
void UFF_ZoneSystemPlaytestSubsystem::RunCommandLineSmokeTest()
{
    TArray<FString> Failures;
    UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
    if (!ZoneSystem || !TestZone || ZoneSystem->FindZone(ActiveZoneId) != TestZone)
    {
        Failures.Add(TEXT("Gameplay Zone was not registered."));
    }

    bEnableFullNavigationValidation = false;
    SetPlacement(FIntPoint(1000, 1000), 0);
    RefreshPreview();
    if (!LastValidation.bIsValid)
    {
        Failures.Add(FString::Printf(TEXT("Initial preview invalid: %s"), *LastValidation.Summary.ToString()));
    }

    FFFZSCommandResult InstallResult;
    if (ZoneSystem && PlacementPreview)
    {
        const FScopedSmokeNavigationValidation NavigationScope(false);
        InstallResult = PlacementPreview->CommitPreview();
    }
    if (!InstallResult.bSucceeded || !InstallResult.FacilityActor)
    {
        Failures.Add(FString::Printf(TEXT("Install failed: %s"), *InstallResult.Message.ToString()));
    }
    else
    {
        LastFacilityId = InstallResult.FacilityActor->GetFacilityInstanceId();
        ApplyFacilityVisualScale(InstallResult.FacilityActor);

        const FFFZSCommandResult OpenResult = ZoneSystem->ExecuteCommand(
            MakeSmokeLifecycleCommand(EFFZSCommandType::Open, LastFacilityId));
        if (!OpenResult.bSucceeded)
        {
            Failures.Add(FString::Printf(TEXT("Open failed: %s"), *OpenResult.Message.ToString()));
        }

        FGuid TicketId;
        const EFFZSQueueJoinResult JoinResult = ZoneSystem->JoinFacilityQueue(
            LastFacilityId, TEXT("SmokeAgent"), nullptr, TicketId);
        if (JoinResult != EFFZSQueueJoinResult::Joined || !TicketId.IsValid())
        {
            Failures.Add(TEXT("Queue join failed."));
        }

        const FFFZSWorldSnapshot SmokeSnapshot = ZoneSystem->CaptureWorldSnapshot();
        if (SmokeSnapshot.Facilities.Num() != 1 || SmokeSnapshot.SchemaVersion != 4)
        {
            Failures.Add(TEXT("Snapshot did not capture the installed facility with schema 4."));
        }
        TArray<FText> RestoreErrors;
        if (!ZoneSystem->RestoreWorldSnapshot(SmokeSnapshot, RestoreErrors))
        {
            Failures.Add(RestoreErrors.IsEmpty()
                ? TEXT("Snapshot restore failed.")
                : FString::Printf(TEXT("Snapshot restore failed: %s"), *RestoreErrors[0].ToString()));
        }
    }

    SetPlacement(FIntPoint(7700, 4800), 0);
    RefreshPreview();
    if (LastValidation.bIsValid || !LastValidation.Reasons.Contains(EFFZSPlacementFailure::ProtectedRouteBlocked))
    {
        Failures.Add(TEXT("No-build protected area did not reject the preview."));
    }

    if (Failures.IsEmpty())
    {
        UE_LOG(LogFFZonePlaytestRuntime, Display,
            TEXT("FFZS_ZONETESTMAP_SMOKE_PASS: zone, preview, install, lifecycle, queue, snapshot and protected-area rejection passed."));
        SmokeExitCode = 0;
    }
    else
    {
        for (const FString& Failure : Failures)
        {
            UE_LOG(LogFFZonePlaytestRuntime, Error, TEXT("FFZS_ZONETESTMAP_SMOKE_FAILURE: %s"), *Failure);
        }
        UE_LOG(LogFFZonePlaytestRuntime, Error, TEXT("FFZS_ZONETESTMAP_SMOKE_FAIL: %d failure(s)."), Failures.Num());
        SmokeExitCode = 1;
    }

    bSmokeExitPending = true;
}

// 여러 구현 파일에서 반복 사용하는 런타임 객체 조회를 안전한 헬퍼로 모은다.
UFFZSWorldSubsystem* UFF_ZoneSystemPlaytestSubsystem::GetZoneSystem() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
}

AFFZSFacilityActor* UFF_ZoneSystemPlaytestSubsystem::GetLastFacility() const
{
    UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
    return ZoneSystem && LastFacilityId.IsValid()
        ? ZoneSystem->FindFacility(LastFacilityId)
        : nullptr;
}


