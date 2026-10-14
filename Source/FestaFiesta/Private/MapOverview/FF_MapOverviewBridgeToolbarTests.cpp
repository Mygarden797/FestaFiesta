#if WITH_DEV_AUTOMATION_TESTS

#include "MapOverview/FF_MapOverviewGameBridgeSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Layout/Children.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SWidget.h"

namespace
{
    bool CheckLiveOrder(FAutomationTestBase& Test, UHorizontalBox& Row)
    {
        const TSharedPtr<SWidget> Slate = Row.GetCachedWidget();
        if (!Test.TestTrue(TEXT("Toolbar has a live Slate panel"), Slate.IsValid())) return false;
        FChildren* Children = Slate->GetChildren();
        if (!Test.TestEqual(TEXT("UMG and actual Slate child count match"), Children->Num(), Row.GetChildrenCount())) return false;
        bool bMatches = true;
        for (int32 Index = 0; Index < Row.GetChildrenCount(); ++Index)
        {
            const TSharedPtr<SWidget> Expected = Row.GetChildAt(Index)->GetCachedWidget();
            bMatches &= Test.TestTrue(FString::Printf(TEXT("Live Slate child %d matches exact UMG child object"), Index),
                Expected.IsValid() && &Children->GetChildAt(Index).Get() == Expected.Get());
        }
        return bMatches;
    }

    struct FToolbarFixture
    {
        TStrongObjectPtr<UFF_BuildToolsWidget> Tools{NewObject<UFF_BuildToolsWidget>()};
        TStrongObjectPtr<UFF_BuildToolsWidget> Addon{NewObject<UFF_BuildToolsWidget>()};
        UHorizontalBox* Row = nullptr;
        USizeBox* Bounds = nullptr;
        USizeBox* GridCell = nullptr;
        USizeBox* TailCell = nullptr;
        UButton* TailButton = nullptr;
        TSharedPtr<SWidget> ToolsSlate;
        TSharedPtr<SWidget> AddonSlate;

        ~FToolbarFixture()
        {
            FFMapOverviewToolbar::DetachFromRow(*Tools.Get(), *Addon.Get());
        }

        bool Create(FAutomationTestBase& Test)
        {
            if (!Tools->Initialize() || !Addon->Initialize()) return false;
            Bounds = Tools->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Size_BuildTopBar"));
            Bounds->SetWidthOverride(216.f);
            Row = Tools->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row_BuildReferenceTopBar"));
            Bounds->AddChild(Row);
            Tools->WidgetTree->RootWidget = Bounds;
            const TCHAR* Names[] = { TEXT("Button_MultiSelect"), TEXT("Button_Grid"), TEXT("Button_SnapCycle") };
            for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
            {
                USizeBox* Cell = Tools->WidgetTree->ConstructWidget<USizeBox>();
                Cell->SetWidthOverride(64.f); Cell->SetHeightOverride(72.f);
                UButton* Button = Tools->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Names[Index]);
                Cell->AddChild(Button);
                auto* Slot = Row->AddChildToHorizontalBox(Cell);
                Slot->SetPadding(FMargin(2.f, 3.f, 6.f, 5.f));
                Slot->SetHorizontalAlignment(HAlign_Center); Slot->SetVerticalAlignment(VAlign_Bottom);
                if (Index == 1) GridCell = Cell;
                if (Index == 2)
                {
                    TailCell = Cell; TailButton = Button;
                    Button->OnClicked.AddDynamic(Tools.Get(), &UFF_BuildToolsWidget::ToggleHelp);
                    FButtonStyle Style = Button->GetStyle();
                    Style.Normal.TintColor = FSlateColor(FLinearColor(.17f, .71f, .53f, 1.f));
                    Style.NormalPadding = FMargin(11.f, 7.f);
                    Button->SetStyle(Style);
                }
            }
            USizeBox* AddonBounds = Addon->WidgetTree->ConstructWidget<USizeBox>();
            AddonBounds->SetWidthOverride(80.f); AddonBounds->SetHeightOverride(72.f);
            AddonBounds->AddChild(Addon->WidgetTree->ConstructWidget<UButton>());
            Addon->WidgetTree->RootWidget = AddonBounds;
            ToolsSlate = Tools->TakeWidget(); AddonSlate = Addon->TakeWidget();
            Tools->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            Addon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            Addon->ForceLayoutPrepass();
            return Test.TestTrue(TEXT("Synthetic toolbar begins with real native Slate order"), CheckLiveOrder(Test, *Row));
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFMapToolbarLiveInsertionTest, "FF_MapOverview.Bridge.ToolbarLiveInsertion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFMapToolbarLiveInsertionTest::RunTest(const FString&)
{
    FToolbarFixture Fixture;
    if (!Fixture.Create(*this)) return false;
    const FButtonStyle OriginalStyle = Fixture.TailButton->GetStyle();
    const int32 OriginalBindings = Fixture.TailButton->OnClicked.GetAllObjects().Num();
    TestTrue(TEXT("Mount succeeds on an already constructed toolbar"),
        FFMapOverviewToolbar::AttachAfterGridToRow(*Fixture.Tools.Get(), *Fixture.Addon.Get()));
    TestTrue(TEXT("Addon is directly after the actual Grid cell"), Fixture.Row->GetChildAt(2) == Fixture.Addon.Get());
    TestTrue(TEXT("Same original tail cell follows addon"), Fixture.Row->GetChildAt(3) == Fixture.TailCell);
    CheckLiveOrder(*this, *Fixture.Row);
    TestEqual(TEXT("Authored addon width and copied margins expand toolbar bounds"), Fixture.Bounds->GetWidthOverride(), 304.f);
    const auto* TailSlot = CastChecked<UHorizontalBoxSlot>(Fixture.TailCell->Slot);
    TestTrue(TEXT("Native tail slot margins survive"), TailSlot->GetPadding() == FMargin(2.f, 3.f, 6.f, 5.f));
    TestTrue(TEXT("Native tail alignment survives"), TailSlot->GetHorizontalAlignment() == HAlign_Center && TailSlot->GetVerticalAlignment() == VAlign_Bottom);
    TestTrue(TEXT("Original button object survives"), Fixture.TailCell->GetContent() == Fixture.TailButton);
    TestTrue(TEXT("Original designer button tint survives"), Fixture.TailButton->GetStyle().Normal.TintColor == OriginalStyle.Normal.TintColor);
    TestTrue(TEXT("Original designer button padding survives"), Fixture.TailButton->GetStyle().NormalPadding == OriginalStyle.NormalPadding);
    TestEqual(TEXT("Original dynamic callbacks survive Slate recreation"), Fixture.TailButton->OnClicked.GetAllObjects().Num(), OriginalBindings);
    TestTrue(TEXT("Repeat mount is idempotent"), FFMapOverviewToolbar::AttachAfterGridToRow(*Fixture.Tools.Get(), *Fixture.Addon.Get()));
    TestEqual(TEXT("Repeat mount adds no duplicate widgets"), Fixture.Row->GetChildrenCount(), 4);
    TestEqual(TEXT("Repeat mount adds no duplicate width"), Fixture.Bounds->GetWidthOverride(), 304.f);
    CheckLiveOrder(*this, *Fixture.Row);
    FFMapOverviewToolbar::DetachFromRow(*Fixture.Tools.Get(), *Fixture.Addon.Get());
    TestNull(TEXT("Teardown detaches only the added WBP"), Fixture.Addon->GetParent());
    TestEqual(TEXT("Teardown restores original toolbar width"), Fixture.Bounds->GetWidthOverride(), 216.f);
    TestEqual(TEXT("Teardown restores original child count"), Fixture.Row->GetChildrenCount(), 3);
    TestTrue(TEXT("Teardown restores original tail position"), Fixture.Row->GetChildAt(2) == Fixture.TailCell);
    CheckLiveOrder(*this, *Fixture.Row);
    FFMapOverviewToolbar::DetachFromRow(*Fixture.Tools.Get(), *Fixture.Addon.Get());
    TestEqual(TEXT("Repeated teardown preserves width"), Fixture.Bounds->GetWidthOverride(), 216.f);

    auto* NestedCell = Fixture.Tools->WidgetTree->ConstructWidget<USizeBox>();
    TStrongObjectPtr<UFF_BuildToolsWidget> Nested(NewObject<UFF_BuildToolsWidget>());
    Nested->Initialize();
    NestedCell->AddChild(Nested.Get());
    Fixture.Row->AddChildToHorizontalBox(NestedCell);
    TestFalse(TEXT("Unknown nested WBP tail is rejected before changes"),
        FFMapOverviewToolbar::AttachAfterGridToRow(*Fixture.Tools.Get(), *Fixture.Addon.Get()));
    TestEqual(TEXT("Rejected mount preserves original width"), Fixture.Bounds->GetWidthOverride(), 216.f);
    TestEqual(TEXT("Rejected mount preserves original row"), Fixture.Row->GetChildrenCount(), 4);
    CheckLiveOrder(*this, *Fixture.Row);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFMapToolbarAuthoredAssetTest, "FF_MapOverview.Bridge.ToolbarAuthoredAsset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFMapToolbarAuthoredAssetTest::RunTest(const FString&)
{
    UClass* Class = LoadClass<UFF_BuildToolsWidget>(nullptr, FFBuildUIAssetPaths::BuildToolsClass);
    if (!TestNotNull(TEXT("Real teammate toolbar WBP class loads read-only"), Class)) return false;
    UPackage* Package = Class->GetOutermost();
    const bool bOriginalDirty = Package->IsDirty();
    auto* CDO = Class->GetDefaultObject<UFF_BuildToolsWidget>();
    UWidgetTree* OriginalDefaultTree = CDO->WidgetTree;
    TStrongObjectPtr<UFF_BuildToolsWidget> Tools(NewObject<UFF_BuildToolsWidget>(GetTransientPackage(), Class));
    if (!Tools->Initialize()) return false;
    const TSharedRef<SWidget> Slate = Tools->TakeWidget();
    auto* Row = Cast<UHorizontalBox>(Tools->GetWidgetFromName(TEXT("Row_BuildReferenceTopBar")));
    auto* Bounds = Cast<USizeBox>(Tools->GetWidgetFromName(TEXT("Size_BuildTopBar")));
    if (!TestNotNull(TEXT("Authored real toolbar row"), Row) || !TestNotNull(TEXT("Authored real toolbar bounds"), Bounds)) return false;
    const TArray<UWidget*> OriginalChildren = Row->GetAllChildren();
    const float Width = Bounds->GetWidthOverride();
    TArray<int32> BindingCounts;
    TArray<UButton*> OriginalButtons;
    TArray<UWidget*> Descendants;
    UWidgetTree::GetChildWidgets(Row, Descendants);
    for (UWidget* Widget : Descendants)
        if (auto* Button = Cast<UButton>(Widget)) { OriginalButtons.Add(Button); BindingCounts.Add(Button->OnClicked.GetAllObjects().Num()); }
    FToolbarFixture AddonFixture;
    if (!AddonFixture.Create(*this)) return false;
    TestTrue(TEXT("New addon mounts in actual teammate toolbar instance"),
        FFMapOverviewToolbar::AttachAfterGridToRow(*Tools.Get(), *AddonFixture.Addon.Get()));
    CheckLiveOrder(*this, *Row);
    for (int32 Index = 0; Index < OriginalButtons.Num(); ++Index)
        TestEqual(TEXT("Real toolbar native callbacks preserved"), OriginalButtons[Index]->OnClicked.GetAllObjects().Num(), BindingCounts[Index]);
    FFMapOverviewToolbar::DetachFromRow(*Tools.Get(), *AddonFixture.Addon.Get());
    TestTrue(TEXT("Exact authored child objects and order restored"), Row->GetAllChildren() == OriginalChildren);
    TestEqual(TEXT("Actual designer toolbar width restored"), Bounds->GetWidthOverride(), Width);
    CheckLiveOrder(*this, *Row);
    TestTrue(TEXT("Authored CDO tree was never replaced"), CDO->WidgetTree == OriginalDefaultTree);
    TestEqual(TEXT("Source teammate asset dirtiness unchanged"), Package->IsDirty(), bOriginalDirty);
    return true;
}

#endif
