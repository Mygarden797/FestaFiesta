// Regression for the saved, designer-editable quick-key guide. This does not start PIE.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Input/HittestGrid.h"
#include "Layout/WidgetPath.h"
#include "Misc/AutomationTest.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SVirtualWindow.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildApprovedQuickKeysTest,
    "FestaFiesta.Building.BuildTools.ApprovedQuickKeys",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFFBuildApprovedQuickKeysTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_BuildToolsWidget>(nullptr,
        FFBuildUIAssetPaths::BuildToolsClass));
    if (!TestNotNull(TEXT("Editor world"), World) || !TestNotNull(TEXT("Saved tools WBP"), Class)) return false;
    const bool PreviousPlayerless = Class->bCanCallInitializedWithoutPlayerContext;
    Class->bCanCallInitializedWithoutPlayerContext = true;
    TStrongObjectPtr<UFF_BuildToolsWidget> Tools(CreateWidget<UFF_BuildToolsWidget>(World, Class));
    Class->bCanCallInitializedWithoutPlayerContext = PreviousPlayerless;
    if (!TestNotNull(TEXT("Transient tools instance"), Tools.Get())) return false;
    const TSharedRef<SWidget> ToolsSlate = Tools->TakeWidget();
    (void)ToolsSlate;
    auto* Guide = Cast<USizeBox>(Tools->GetWidgetFromName(TEXT("Size_ApprovedQuickKeys")));
    auto* Column = Cast<UVerticalBox>(Tools->GetWidgetFromName(TEXT("Column_ApprovedQuickKeys")));
    if (!TestNotNull(TEXT("Designer guide"), Guide) || !TestNotNull(TEXT("Designer column"), Column)) return false;
    const auto* Slot = Cast<UCanvasPanelSlot>(Guide->Slot);
    if (!TestNotNull(TEXT("Guide has a canvas slot"), Slot)) return false;
    TestTrue(TEXT("Guide is left anchored"), Slot->GetAnchors().Minimum.Equals(FVector2D::ZeroVector) &&
        Slot->GetAnchors().Maximum.Equals(FVector2D::ZeroVector) && Slot->GetPosition().X >= 0.f);
    const TCHAR* Rows[] = {TEXT("Move"), TEXT("Rotate"), TEXT("Grid"), TEXT("Catalog"),
        TEXT("Adjust"), TEXT("Undo"), TEXT("Redo")};
    TestEqual(TEXT("Exactly seven independent shortcut strips"), Column->GetChildrenCount(), static_cast<int32>(UE_ARRAY_COUNT(Rows)));
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Rows); ++Index)
    {
        const FName RowName(*FString::Printf(TEXT("Row_ApprovedKey_%s"), Rows[Index]));
        auto* Row = Cast<UBorder>(Tools->GetWidgetFromName(RowName));
        auto* Label = Cast<UTextBlock>(Tools->GetWidgetFromName(FName(*FString::Printf(TEXT("Text_ApprovedLabel_%s"), Rows[Index]))));
        if (!TestNotNull(*RowName.ToString(), Row) || !TestNotNull(TEXT("Shortcut action label"), Label)) return false;
        TestTrue(TEXT("Each row keeps its authored vertical order"), Column->GetChildIndex(Row) == Index);
        TestEqual(TEXT("Shortcut strip does not intercept input"), Row->GetVisibility(), ESlateVisibility::HitTestInvisible);
        TestFalse(TEXT("Every action has a readable caption"), Label->GetText().IsEmpty());
    }
    for (const TCHAR* Key : {TEXT("W"), TEXT("A"), TEXT("S"), TEXT("D")})
    {
        const auto* Text = Cast<UTextBlock>(Tools->GetWidgetFromName(FName(*FString::Printf(TEXT("Text_ApprovedKey_%s"), Key))));
        if (!TestNotNull(TEXT("Movement key label"), Text)) return false;
        TestEqual(TEXT("Playerless fallback retains the authored movement key"), Text->GetText().ToString(), FString(Key));
    }
    for (const TCHAR* Name : {TEXT("Rotate"), TEXT("Adjust")})
    {
        auto* Row = Cast<UBorder>(Tools->GetWidgetFromName(FName(*FString::Printf(TEXT("Row_ApprovedKey_%s"), Name))));
        const auto* Label = Cast<UTextBlock>(Tools->GetWidgetFromName(FName(*FString::Printf(TEXT("Text_ApprovedLabel_%s"), Name))));
        if (!TestNotNull(TEXT("Preview-only shortcut"), Row) || !TestNotNull(TEXT("Preview context caption"), Label)) return false;
        TestTrue(TEXT("Preview-only action identifies its context"), Label->GetText().ToString().Contains(TEXT("배치 중")));
        Row->SetRenderOpacity(1.f);
        Tools->RefreshTools();
        TestTrue(TEXT("Without a placement preview the action is de-emphasized"), FMath::IsNearlyEqual(Row->GetRenderOpacity(), .8f));
    }

    // Put the real guide over a separate input target and use Slate's hit-test grid.
    // This tests input passthrough only, not in-game visibility or keyboard remapping.
    Guide->SetVisibility(ESlateVisibility::HitTestInvisible);
    TSharedPtr<SButton> Behind;
    const FVector2D FrameSize(230.f, 274.f);
    auto Window = SNew(SVirtualWindow).Size(FrameSize);
    Window->SetContent(SNew(SOverlay)
        + SOverlay::Slot()[SAssignNew(Behind, SButton)]
        + SOverlay::Slot()[Guide->TakeWidget()]);
    FHittestGrid Grid;
    for (int32 Pass = 0; Pass < 3; ++Pass)
    {
        Window->SlatePrepass(1.f);
        Grid.SetHittestArea(FVector2D::ZeroVector, FrameSize);
        Grid.Clear();
        FSlateWindowElementList Elements(Window);
        const FGeometry Geometry = FGeometry::MakeRoot(FrameSize, FSlateLayoutTransform(1.f));
        const FPaintArgs Args(nullptr, Grid, FVector2D::ZeroVector, FPlatformTime::Seconds(), 1.f / 60.f);
        Window->Paint(Args, Geometry, Geometry.GetLayoutBoundingRect(), Elements, 0, FWidgetStyle(), true);
    }
    float PreviousBottom = -1.f;
    for (const TCHAR* Name : Rows)
    {
        UWidget* Row = Tools->GetWidgetFromName(FName(*FString::Printf(TEXT("Row_ApprovedKey_%s"), Name)));
        const FSlateRect Rect = Row->GetCachedGeometry().GetLayoutBoundingRect();
        TestTrue(TEXT("Each strip has a nonzero layout"), Rect.GetSize().X > 1.f && Rect.GetSize().Y > 1.f);
        TestTrue(TEXT("Strips are separated vertically"), Rect.Top >= PreviousBottom - .5f);
        PreviousBottom = Rect.Bottom;
        const FVector2D Point((Rect.Left + Rect.Right) * .5f, (Rect.Top + Rect.Bottom) * .5f);
        TArray<FWidgetAndPointer> Bubble = Grid.GetBubblePath(Point, 0.f, false);
        const FWidgetPath Path(MakeArrayView(Bubble));
        TestTrue(TEXT("Pointer reaches the target behind each shortcut"), Path.IsValid() && Path.ContainsWidget(Behind.Get()));
    }
    return true;
}
#endif
