// Exercise the saved release map and its real GameMode-spawned camera. Synthetic
// adjacent zones cannot expose the small gaps between this map's authored zones.
#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Camera/CameraComponent.h"
#include "Camera/FF_FestivalGameMode.h"
#include "Camera/FF_FestivalPlayerController.h"
#include "Camera/FF_OverviewCameraPawn.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"

namespace
{
bool HasCameraProductionPIESession()
{
    if (GEditor && GEditor->PlayWorld) return true;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (Context.WorldType == EWorldType::PIE) return true;
    }
    return false;
}

bool ContainsCameraXY(const AFFZSGameplayZone& Zone, const FVector& Point)
{
    return Zone.ContainsWorldPoint(FVector(Point.X, Point.Y, Zone.GetActorLocation().Z));
}

class FFFCheckProductionOverviewCamera final : public IAutomationLatentCommand
{
public:
    explicit FFFCheckProductionOverviewCamera(FAutomationTestBase* InTest) : Test(InTest) {}

    virtual bool Update() override
    {
        if (StartedAt < 0.0) StartedAt = FPlatformTime::Seconds();
        if (!ScreenshotPath.IsEmpty() && !bScreenshotComplete)
        {
            if (!IFileManager::Get().FileExists(*ScreenshotPath))
            {
                if (FPlatformTime::Seconds() - ScreenshotStartedAt < 15.0) return false;
                Test->AddWarning(TEXT("Initial camera PNG was not produced; inspect the logged lens transform and capture separately."));
                if (FScreenshotRequest::GetFilename() == ScreenshotPath) FScreenshotRequest::Reset();
            }
            else Test->AddInfo(TEXT("Production camera initial screenshot: ") + ScreenshotPath);
            bScreenshotComplete = true;
        }
        UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
        AFF_FestivalPlayerController* Controller = World
            ? Cast<AFF_FestivalPlayerController>(World->GetFirstPlayerController()) : nullptr;
        AFF_OverviewCameraPawn* Camera = Controller ? Controller->GetOverviewCameraPawn() : nullptr;
        if (!World || !World->HasBegunPlay() || World->GetTimeSeconds() < 0.1f || !Camera)
        {
            if (FPlatformTime::Seconds() - StartedAt < 60.0) return false;
            Test->AddError(TEXT("Test_Main did not start with an overview camera within 60 seconds."));
            return true;
        }

        // Yield before any synchronous movement so the renderer captures the
        // actual initial, idle game viewport on a later engine frame.
        if (ScreenshotPath.IsEmpty() && World->GetGameViewport() && World->GetGameViewport()->Viewport)
        {
            ScreenshotPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),
                TEXT("Automation/CameraProduction"), TEXT("initial-") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".png")));
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(ScreenshotPath), true);
            ScreenshotStartedAt = FPlatformTime::Seconds();
            FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
            return false;
        }

        const AFF_FestivalGameMode* Mode = World->GetAuthGameMode<AFF_FestivalGameMode>();
        const UFFZSWorldSubsystem* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
        if (!Test->TestNotNull(TEXT("Production festival GameMode is active"), Mode)
            || !Test->TestNotNull(TEXT("Production zone registry exists"), Zones)) return true;

        // Read before arranging any camera movement: InitializeOverview in this
        // test must never conceal a broken GameMode BeginPlay/possession path.
        const FFF_OverviewCameraState Initial = Camera->GetCameraState();
        Test->TestTrue(TEXT("Production GameMode applies its authored initial framing"), Mode->bApplyInitialFraming);
        Test->TestTrue(TEXT("The first possessed pawn is the overview camera"), Controller->GetPawn() == Camera);
        Test->TestTrue(TEXT("The first view target is the overview camera"), Controller->GetViewTarget() == Camera);
        Test->TestEqual(TEXT("Initial view mode is overview"), Controller->GetViewMode(), EFF_CameraViewMode::Overview);
        Test->TestTrue(TEXT("GameMode spawned its configured camera Blueprint"), Camera->GetClass() == Mode->DefaultPawnClass.Get());
        Test->TestTrue(TEXT("The spawned overview camera component is active"), Camera->HasActiveCamera());
        Test->TestTrue(TEXT("Initial focus X matches GameMode framing"),
            FMath::IsNearlyEqual(Initial.FocusLocation.X, Mode->InitialFocusLocation.X, 1.0));
        Test->TestTrue(TEXT("Initial focus Y matches GameMode framing"),
            FMath::IsNearlyEqual(Initial.FocusLocation.Y, Mode->InitialFocusLocation.Y, 1.0));
        Test->TestTrue(TEXT("Initial focus preserves the authored GameMode height"),
            FMath::IsNearlyEqual(Initial.FocusLocation.Z, Mode->InitialFocusLocation.Z, 1.0));
        Test->TestTrue(TEXT("Initial arm length matches GameMode framing"),
            FMath::IsNearlyEqual(Initial.ArmLength, Mode->InitialArmLength, 0.1f));
        Test->AddInfo(FString::Printf(TEXT("Production camera initial focus=(%s), arm=%.1f, pawn=%s, controller=%s"),
            *Initial.FocusLocation.ToString(), Initial.ArmLength,
            *Camera->GetClass()->GetPathName(), *Controller->GetClass()->GetPathName()));
        const UCameraComponent* Lens = Camera->FindComponentByClass<UCameraComponent>();
        const UGameViewportClient* ViewportClient = World->GetGameViewport();
        const FIntPoint ViewportSize = ViewportClient && ViewportClient->Viewport
            ? ViewportClient->Viewport->GetSizeXY() : FIntPoint::ZeroValue;
        if (Test->TestNotNull(TEXT("Production camera lens exists"), Lens))
        {
            Test->AddInfo(FString::Printf(TEXT("Production camera screenshot reference: pawn=(%s), lens=(%s), rotation=(%s), FOV=%.2f, viewport=%dx%d"),
                *Camera->GetActorLocation().ToString(), *Lens->GetComponentLocation().ToString(),
                *Lens->GetComponentRotation().ToString(), Lens->FieldOfView, ViewportSize.X, ViewportSize.Y));
        }
        for (int32 Frame = 0; Frame < 120; ++Frame) Camera->Tick(1.f / 120.f);
        Test->TestTrue(TEXT("Initial authored focus remains fixed while idle"),
            Camera->GetCameraState().FocusLocation.Equals(Initial.FocusLocation, 0.01));
        Test->TestTrue(TEXT("Initial authored arm length remains fixed while idle"),
            FMath::IsNearlyEqual(Camera->GetCameraState().ArmLength, Initial.ArmLength, 0.01f));

        AFFZSGameplayZone* Tutorial = Zones->FindZone(TEXT("GZ_Tutorial"));
        AFFZSGameplayZone* Parking = Zones->FindZone(TEXT("GZ_TemporaryParkingEast"));
        if (!Test->TestNotNull(TEXT("Authored tutorial zone exists"), Tutorial)
            || !Test->TestNotNull(TEXT("Authored parking zone exists"), Parking)) return true;

        // October 10 layout: at Y=35800 the two polygon boundaries are X=75909.875
        // and X=75884.515. Small 120 Hz inputs cannot jump the 25.36 cm gap.
        // Keep these independent witnesses so closing/moving the gap cannot
        // silently turn this regression into an adjacent-zone test.
        const FVector TutorialSide(75950.0, 35800.0, Initial.FocusLocation.Z);
        const FVector ParkingSide(75850.0, 35800.0, Initial.FocusLocation.Z);
        const FVector Gap(75897.0, 35800.0, Initial.FocusLocation.Z);
        bool bGapOutsideEveryZone = true;
        FBox ExpectedBounds(ForceInit);
        for (TActorIterator<AFFZSGameplayZone> It(World); It; ++It)
        {
            bGapOutsideEveryZone &= !ContainsCameraXY(**It, Gap);
            for (const FVector2D& Point : It->GetBoundaryPolygonLocalCm())
            {
                ExpectedBounds += It->GetActorTransform().TransformPosition(FVector(Point, 0.0));
            }
        }
        const bool bWitnessesValid = Test->TestTrue(TEXT("Gap starts inside tutorial"), ContainsCameraXY(*Tutorial, TutorialSide))
            & Test->TestTrue(TEXT("Gap ends inside parking"), ContainsCameraXY(*Parking, ParkingSide))
            & Test->TestTrue(TEXT("The real map retains a gap outside every zone"), bGapOutsideEveryZone);
        if (!bWitnessesValid || !Test->TestTrue(TEXT("Authored camera extent is valid"), !!ExpectedBounds.IsValid)) return true;

        constexpr float DeltaSeconds = 1.f / 120.f;
        // The already spawned/possessed pawn consumes its normal input and
        // ground-following code. No camera setting or zone geometry is replaced.
        // Tick synchronously to remove OS viewport focus/frame-rate dependence.
        Camera->InitializeOverview(TutorialSide, Initial.ArmLength, Mode->bPreserveInitialCameraHeight);
        Camera->Tick(DeltaSeconds);
        for (int32 Frame = 0; Frame < 120; ++Frame)
        {
            Camera->AddPanInput(FVector2D(0.0, -0.25));
            Camera->Tick(DeltaSeconds);
        }
        const FVector AfterPan = Camera->GetCameraState().FocusLocation;
        Test->TestTrue(TEXT("Small pan inputs cross the actual tutorial-to-parking gap"),
            AfterPan.X < ParkingSide.X && ContainsCameraXY(*Parking, AfterPan));
        Camera->StopCameraMotion();
        for (int32 Frame = 0; Frame < 120; ++Frame)
        {
            Camera->AddEdgeScrollInput(FVector2D(0.0, 0.1));
            Camera->Tick(DeltaSeconds);
        }
        const FVector AfterEdge = Camera->GetCameraState().FocusLocation;
        Test->TestTrue(TEXT("Small edge-scroll inputs cross back into tutorial"),
            AfterEdge.X > TutorialSide.X && ContainsCameraXY(*Tutorial, AfterEdge));
        Test->AddInfo(FString::Printf(TEXT("Production gap traversal: pan=(%s), edge=(%s)"),
            *AfterPan.ToString(), *AfterEdge.ToString()));

        ExpectedBounds = ExpectedBounds.ExpandBy(FVector(
            FMath::Max(0.f, Camera->CameraSettings.ZoneBoundsPadding),
            FMath::Max(0.f, Camera->CameraSettings.ZoneBoundsPadding), 0.0));
        Test->TestTrue(TEXT("Production camera uses the complete authored map extent"), Camera->CameraSettings.bUseZoneBounds);
        const auto CheckOuterEdge = [&](const TCHAR* Label, const bool bXAxis, const bool bMaximum)
        {
            FVector Start = ExpectedBounds.GetCenter();
            Start.Z = Initial.FocusLocation.Z;
            const double Limit = bXAxis
                ? (bMaximum ? ExpectedBounds.Max.X : ExpectedBounds.Min.X)
                : (bMaximum ? ExpectedBounds.Max.Y : ExpectedBounds.Min.Y);
            (bXAxis ? Start.X : Start.Y) = Limit + (bMaximum ? -100.0 : 100.0);
            Camera->InitializeOverview(Start, Initial.ArmLength, Mode->bPreserveInitialCameraHeight);
            Camera->Tick(DeltaSeconds);
            const double Direction = bMaximum ? 0.25 : -0.25;
            for (int32 Frame = 0; Frame < 120; ++Frame)
            {
                Camera->AddPanInput(bXAxis ? FVector2D(0.0, Direction) : FVector2D(Direction, 0.0));
                Camera->Tick(DeltaSeconds);
            }
            const FVector Actual = Camera->GetCameraState().FocusLocation;
            const double Coordinate = bXAxis ? Actual.X : Actual.Y;
            Test->TestTrue(FString::Printf(TEXT("%s: reaches the map perimeter including padding"), Label),
                FMath::IsNearlyEqual(Coordinate, Limit, 1.0));
            Test->TestTrue(FString::Printf(TEXT("%s: cannot escape the outer map bounds"), Label),
                Actual.X >= ExpectedBounds.Min.X - 0.1 && Actual.X <= ExpectedBounds.Max.X + 0.1
                && Actual.Y >= ExpectedBounds.Min.Y - 0.1 && Actual.Y <= ExpectedBounds.Max.Y + 0.1);
        };
        CheckOuterEdge(TEXT("Minimum X"), true, false);
        CheckOuterEdge(TEXT("Maximum X"), true, true);
        CheckOuterEdge(TEXT("Minimum Y"), false, false);
        CheckOuterEdge(TEXT("Maximum Y"), false, true);
        Camera->StopCameraMotion();
        Test->TestTrue(TEXT("Movement never changes possession"), Controller->GetPawn() == Camera);
        Test->TestTrue(TEXT("Movement never changes the view target"), Controller->GetViewTarget() == Camera);
        return true;
    }

private:
    FAutomationTestBase* Test;
    double StartedAt = -1.0;
    double ScreenshotStartedAt = -1.0;
    FString ScreenshotPath;
    bool bScreenshotComplete = false;
};

class FFFWaitForCameraProductionPIEEnd final : public IAutomationLatentCommand
{
public:
    explicit FFFWaitForCameraProductionPIEEnd(FAutomationTestBase* InTest) : Test(InTest) {}
    virtual bool Update() override
    {
        if (StartedAt < 0.0) StartedAt = FPlatformTime::Seconds();
        if (!HasCameraProductionPIESession()) return true;
        if (FPlatformTime::Seconds() - StartedAt < 60.0) return false;
        Test->AddError(TEXT("Production camera PIE teardown left a session/world context after 60 seconds."));
        return true;
    }
private:
    FAutomationTestBase* Test;
    double StartedAt = -1.0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFOverviewCameraProductionTest,
    "FestaFiesta.Camera.Production.InitialFramingAndZoneTraversal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFFOverviewCameraProductionTest::RunTest(const FString& Parameters)
{
    if (HasCameraProductionPIESession())
    {
        AddError(TEXT("A previous PIE session remains; production camera checks require a fresh session."));
        return false;
    }
    UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/FestaFiesta/Maps/Test_Main"));
    if (!TestNotNull(TEXT("Production camera map loads without saving"), World)) return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FFFCheckProductionOverviewCamera(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    ADD_LATENT_AUTOMATION_COMMAND(FFFWaitForCameraProductionPIEEnd(this));
    return true;
}
#endif
