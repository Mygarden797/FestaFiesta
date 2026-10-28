// Exercise the saved toolbar against a real world's CatalogOpen state. A preview-only
// widget cannot expose a catalog accidentally treated as an occluding global window.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/EditableTextBox.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildToolsCatalogHelpTest,
    "FestaFiesta.Building.BuildTools.CatalogHelpLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFBuildToolsCatalogHelpTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game))
    {
        Fixture.ForwardErrorMessages(this);
        return false;
    }
    UWorld* World = Fixture.GetTestWorld();
    auto* Build = World ? World->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
    auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_BuildToolsWidget>(nullptr,
        FFBuildUIAssetPaths::BuildToolsClass));
    if (!TestNotNull(TEXT("Real game-world build subsystem"), Build) ||
        !TestNotNull(TEXT("Saved production toolbar class"), Class)) return false;
    const bool bPreviousPlayerlessInitialization = Class->bCanCallInitializedWithoutPlayerContext;
    Class->bCanCallInitializedWithoutPlayerContext = true;
    ON_SCOPE_EXIT { Class->bCanCallInitializedWithoutPlayerContext = bPreviousPlayerlessInitialization; };
    TStrongObjectPtr<UFF_BuildToolsWidget> Widget(CreateWidget<UFF_BuildToolsWidget>(World, Class));
    if (!TestNotNull(TEXT("Production toolbar instance"), Widget.Get())) return false;
    const TSharedRef<SWidget> Slate = Widget->TakeWidget();
    // First-use explanations have separate coverage. This fixture follows the
    // same real buttons after those explanations have already been acknowledged.
    Widget->FeatureDiscovery.Reset();
    Build->BuildToolsWidget = Widget.Get();
    Build->State = EFF_BuildModeState::CatalogOpen;
    Build->bBuildSessionActive = true;
    ON_SCOPE_EXIT
    {
        Build->BuildToolsWidget = nullptr;
        Build->State = EFF_BuildModeState::Closed;
        Build->bBuildSessionActive = false;
    };
    Widget->RefreshTools();
    UWidget* Help = Widget->GetWidgetFromName(TEXT("Border_PlacementHelp"));
    UWidget* Surface = Widget->GetWidgetFromName(TEXT("Size_BuildWorldHelp"));
    UWidget* Settings = Widget->GetWidgetFromName(TEXT("Panel_BuildSettings"));
    if (!TestNotNull(TEXT("Authored instructions"), Help) ||
        !TestNotNull(TEXT("Complete instruction surface"), Surface) ||
        !TestNotNull(TEXT("Authored settings drawer"), Settings)) return false;
    const auto HelpIsRendered = [&]
    {
        for (const UWidget* Current = Help; Current; Current = Current->GetParent())
            if (Current->GetVisibility() == ESlateVisibility::Collapsed ||
                Current->GetVisibility() == ESlateVisibility::Hidden) return false;
        return true;
    };
    const auto Click = [&](const FName Name)
    {
        UButton* Button = Cast<UButton>(Widget->GetWidgetFromName(Name));
        if (!TestNotNull(*Name.ToString(), Button)) return false;
        TestTrue(Name.ToString() + TEXT(" retains its native action"), Button->OnClicked.IsBound());
        Button->OnClicked.Broadcast();
        // Run the real occlusion refresh used each frame, without a render viewport.
        Widget->NativeTick(FGeometry(), 0.f);
        TestEqual(TEXT("Toolbar actions preserve catalog browsing"), Build->GetBuildModeState(), EFF_BuildModeState::CatalogOpen);
        return true;
    };
    TestFalse(TEXT("Instructions start dismissed while catalog browsing"), HelpIsRendered());
    if (!Click(TEXT("Button_ToolbarHelp"))) return false;
    TestFalse(TEXT("The toolbar action reaches instructions directly"), Widget->IsFeatureGuideOpen());
    TestTrue(TEXT("Catalog browsing allows the complete instructions to appear"), HelpIsRendered() && Surface->IsVisible());
    if (!Click(TEXT("Button_ToolbarHelp"))) return false;
    TestFalse(TEXT("The same toolbar action dismisses instructions in the catalog"), HelpIsRendered());
    if (!Click(TEXT("Button_ToolbarHelp")) || !Click(TEXT("Button_BuildSettings"))) return false;
    TestTrue(TEXT("The real settings action opens its drawer"), Settings->IsVisible());
    auto* GapInput = Cast<UEditableTextBox>(Widget->GetWidgetFromName(TEXT("Editable_DragPlacementGap")));
    if (!TestNotNull(TEXT("Saved toolbar gains the spacing control without a binary migration"), GapInput)) return false;
    TestTrue(TEXT("The gap input has its real native commit binding"), GapInput->OnTextCommitted.IsBound());
    const int32 PreviousSnap = Build->GetPreferredPositionSnapCm();
    GapInput->SetText(FText::FromString(TEXT("250")));
    GapInput->OnTextCommitted.Broadcast(GapInput->GetText(), ETextCommit::OnEnter);
    TestEqual(TEXT("Editing the visible control updates actual drag spacing"), Build->GetDragPlacementGapCm(), 250);
    TestEqual(TEXT("Facility spacing does not change the position snap preference"), Build->GetPreferredPositionSnapCm(), PreviousSnap);
    GapInput->SetText(FText::FromString(TEXT("1001")));
    GapInput->OnTextCommitted.Broadcast(GapInput->GetText(), ETextCommit::OnEnter);
    TestEqual(TEXT("An invalid visible edit preserves the previous valid setting"), Build->GetDragPlacementGapCm(), 250);
    GapInput->OnTextCommitted.Broadcast(GapInput->GetText(), ETextCommit::OnCleared);
    TestEqual(TEXT("Cancelling an invalid edit restores the stored centimeters"), GapInput->GetText().ToString(), FString(TEXT("250")));
    TestFalse(TEXT("Settings cover previously open instructions"), HelpIsRendered());
    TestEqual(TEXT("Settings collapse the complete instruction surface"), Surface->GetVisibility(), ESlateVisibility::Collapsed);
    if (!Click(TEXT("Button_BuildSettingsClose"))) return false;
    TestFalse(TEXT("The real close action dismisses settings"), Settings->IsVisible());
    TestTrue(TEXT("Closing settings restores instructions over the still-open catalog"), HelpIsRendered());
    if (!Click(TEXT("Button_ToolbarHelp")) || !Click(TEXT("Button_BuildSettings")) ||
        !Click(TEXT("Button_BuildSettingsClose"))) return false;
    TestFalse(TEXT("Closing settings keeps previously dismissed instructions closed"), HelpIsRendered());
    return true;
}

#endif
