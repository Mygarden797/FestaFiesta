#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Building/FF_BuildModeSubsystem.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Components/Border.h"
#include "Engine/World.h"
#include "FF_FFZSWorldSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildFeatureGuideInputTest,
    "FestaFiesta.Building.Introduction.GuideBlocksRawWorldShortcuts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFFBuildFeatureGuideInputTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game))
    {
        Fixture.ForwardErrorMessages(this);
        return false;
    }
    UWorld* World = Fixture.GetTestWorld();
    auto* Build = World->GetSubsystem<UFF_BuildModeSubsystem>();
    auto* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
    auto* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("Build input owner"), Build) || !TestNotNull(TEXT("World authority"), Zones) ||
        !TestNotNull(TEXT("Input controller"), Controller)) return false;
    Controller->PlayerInput = NewObject<UPlayerInput>(Controller);
    TStrongObjectPtr<UFF_BuildToolsWidget> Guide(NewObject<UFF_BuildToolsWidget>(World));
    Guide->Panel_FeatureGuide = NewObject<UBorder>(Guide.Get());
    Guide->Panel_FeatureGuide->SetVisibility(ESlateVisibility::Visible);
    // A UObject visibility property alone has no live Slate visibility to query.
    const TSharedRef<SWidget> GuideSurface = Guide->Panel_FeatureGuide->TakeWidget();
    if (!TestTrue(TEXT("The realized guide surface owns input"), Guide->IsFeatureGuideOpen())) return false;
    Build->BuildToolsWidget = Guide.Get();
    Build->State = EFF_BuildModeState::Placing;
    Build->bBuildSessionActive = true;
    const bool bGrid = Build->IsPlacementGridVisible();
    const auto Placement = Build->CurrentPlacement;
    const int64 Revision = Zones->GetWorldRevision();
    constexpr float Delta = 1.f / 60.f;
    TArray<UInputComponent*> InputStack;
    auto Press = [&](const FKey Key, const bool bCtrl)
    {
        if (bCtrl) Controller->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(
            EKeys::LeftControl, IE_Pressed, 1.f));
        Controller->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.f));
        Controller->PlayerInput->ProcessInputStack(InputStack, Delta, false);
        TestTrue(TEXT("The fixture produces an actual controller press edge"), Controller->WasInputKeyJustPressed(Key));
        Build->HandleInput(*Controller, Delta);
        TestEqual(TEXT("Guide protects the active construction state"), Build->GetBuildModeState(), EFF_BuildModeState::Placing);
        TestEqual(TEXT("Guide blocks the grid shortcut"), Build->IsPlacementGridVisible(), bGrid);
        TestEqual(TEXT("Guide blocks held rotation input"), Build->CurrentPlacement.YawCentidegrees, Placement.YawCentidegrees);
        TestEqual(TEXT("Guide blocks construction/history changes"), Zones->GetWorldRevision(), Revision);
        Controller->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 0.f));
        if (bCtrl) Controller->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(
            EKeys::LeftControl, IE_Released, 0.f));
        Controller->PlayerInput->ProcessInputStack(InputStack, Delta, false);
    };
    for (const FKey Key : {EKeys::G, EKeys::B, EKeys::Q, EKeys::E, EKeys::Enter,
        EKeys::LeftMouseButton, EKeys::RightMouseButton}) Press(Key, false);
    Press(EKeys::Z, true);
    Press(EKeys::Y, true);
    TestTrue(TEXT("Closing-frame pointer press remains suppressed"), Build->bSuppressWorldClickUntilReleased);
    TestTrue(TEXT("Closing-frame confirmation remains suppressed"), Build->bSuppressPlacementConfirmUntilReleased);
    TestTrue(TEXT("Closing-frame build toggle remains suppressed"), Build->bSuppressBuildToggleUntilReleased);
    Build->BuildToolsWidget = nullptr;
    Build->State = EFF_BuildModeState::Closed;
    Build->bBuildSessionActive = false;
    return true;
}

#endif
