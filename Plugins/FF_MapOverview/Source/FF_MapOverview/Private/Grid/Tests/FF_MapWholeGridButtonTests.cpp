#if WITH_DEV_AUTOMATION_TESTS

#include "FF_MapWholeGridButtonWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace
{
    UFFMapWholeGridButtonWidget* CreateAuthoredWholeGridButton(FAutomationTestBase& Test)
    {
        UClass* Class = LoadClass<UFFMapWholeGridButtonWidget>(nullptr,
            TEXT("/FF_MapOverview/UI/WBP_FF_WholeGridButton.WBP_FF_WholeGridButton_C"));
        if (!Test.TestNotNull(TEXT("Actual editable whole-grid toolbar WBP loads"), Class) ||
            !Test.TestTrue(TEXT("Toolbar class comes from a Blueprint asset"), Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint)))
            return nullptr;
        UFFMapWholeGridButtonWidget* Widget = NewObject<UFFMapWholeGridButtonWidget>(GetTransientPackage(), Class);
        if (!Test.TestNotNull(TEXT("Actual toolbar WBP instance"), Widget) ||
            !Test.TestTrue(TEXT("Toolbar initializes its Designer widget tree"), Widget->Initialize())) return nullptr;
        return Widget;
    }

    template <typename DelegateType>
    int32 CountOwnerBindings(const DelegateType& Delegate, const UObject& Owner)
    {
        int32 Count = 0;
        for (const UObject* Object : Delegate.GetAllObjects()) if (Object == &Owner) ++Count;
        return Count;
    }

    FString EditablePresentation(const UObject& Object)
    {
        FString Result;
        for (TFieldIterator<FProperty> It(Object.GetClass()); It; ++It)
        {
            const FProperty* Property = *It;
            if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Transient)) continue;
            FString Value;
            Property->ExportText_InContainer(0, Value, &Object, nullptr, const_cast<UObject*>(&Object), PPF_None);
            Result += Property->GetName() + TEXT("=") + Value + TEXT("\n");
        }
        return Result;
    }

    struct FAuthoredGlyphNode
    {
        UWidget* Widget = nullptr;
        UPanelSlot* Slot = nullptr;
        FName Name;
        FString WidgetPresentation;
        FString SlotPresentation;
    };

    TArray<FAuthoredGlyphNode> CaptureGlyph(USizeBox& IconBounds)
    {
        TArray<UWidget*> Widgets;
        UWidgetTree::GetChildWidgets(&IconBounds, Widgets);
        Widgets.Insert(&IconBounds, 0);
        TArray<FAuthoredGlyphNode> Result;
        for (UWidget* Widget : Widgets)
            Result.Add({Widget, Widget->Slot, Widget->GetFName(), EditablePresentation(*Widget),
                Widget->Slot ? EditablePresentation(*Widget->Slot) : FString()});
        return Result;
    }

    void VerifyGlyph(FAutomationTestBase& Test, const UFFMapWholeGridButtonWidget& Owner,
        const TArray<FAuthoredGlyphNode>& Glyph)
    {
        for (const FAuthoredGlyphNode& Node : Glyph)
        {
            Test.TestTrue(TEXT("Runtime retains every Designer glyph object and slot"),
                Owner.WidgetTree->FindWidget(Node.Name) == Node.Widget && Node.Widget->Slot == Node.Slot);
            Test.TestEqual(FString::Printf(TEXT("Glyph %s preserves authored appearance and font metadata"), *Node.Name.ToString()),
                EditablePresentation(*Node.Widget), Node.WidgetPresentation);
            if (Node.Slot)
                Test.TestEqual(FString::Printf(TEXT("Glyph %s preserves authored slot layout"), *Node.Name.ToString()),
                    EditablePresentation(*Node.Slot), Node.SlotPresentation);
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFMapWholeGridAuthoredLayoutTest,
    "FF_MapOverview.Toolbar.WholeGridAuthoredLayoutAndOutsideBuildGuard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFMapWholeGridAuthoredLayoutTest::RunTest(const FString&)
{
    TStrongObjectPtr<UFFMapWholeGridButtonWidget> Instance(CreateAuthoredWholeGridButton(*this));
    UFFMapWholeGridButtonWidget* Widget = Instance.Get();
    if (!Widget) return false;
    USizeBox* Bounds = Cast<USizeBox>(Widget->GetWidgetFromName(TEXT("Bounds")));
    UButton* Button = Cast<UButton>(Widget->GetWidgetFromName(TEXT("WholeGridButton")));
    USizeBox* IconBounds = Cast<USizeBox>(Widget->GetWidgetFromName(TEXT("IconBounds")));
    UTextBlock* Label = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Label")));
    if (!TestNotNull(TEXT("Designer toolbar bounds"), Bounds) || !TestNotNull(TEXT("Designer toolbar button"), Button) ||
        !TestNotNull(TEXT("Designer toolbar glyph bounds"), IconBounds) || !TestNotNull(TEXT("Designer toolbar label"), Label)) return false;
    if (!TestNotNull(TEXT("Designer glyph bounds own an editable glyph subtree"), IconBounds->GetContent())) return false;
    const TArray<FAuthoredGlyphNode> AuthoredGlyph = CaptureGlyph(*IconBounds);
    TestTrue(TEXT("Designer bounds are the root and contain the button"), Widget->GetRootWidget() == Bounds && Bounds->GetContent() == Button);
    TestTrue(TEXT("Designer owns a nonzero toolbar width and height"), Bounds->IsWidthOverride() && Bounds->IsHeightOverride() &&
        Bounds->GetWidthOverride() > 0.0f && Bounds->GetHeightOverride() > 0.0f);
    TestEqual(TEXT("Caption is authored static text in the WBP"), Label->GetText().ToString(), FString(TEXT("전체 격자")));
    TestNotNull(TEXT("Toolbar caption uses an actual UI font resource"), Label->GetFont().FontObject.Get());
    for (const FName Name : {FName(TEXT("bUseSelectionStyle")), FName(TEXT("SelectionStyle")),
        FName(TEXT("SelectedIconColor")), FName(TEXT("SelectedTextColor"))})
    {
        const FProperty* Property = FindFProperty<FProperty>(Widget->GetClass(), Name);
        if (TestNotNull(FString::Printf(TEXT("Designer selection property %s exists"), *Name.ToString()), Property))
            TestTrue(FString::Printf(TEXT("Selection property %s remains editable"), *Name.ToString()), Property->HasAnyPropertyFlags(CPF_Edit));
    }
    const FText AuthoredCaption = Label->GetText();
    const FSlateFontInfo AuthoredFont = Label->GetFont();
    const FButtonStyle AuthoredStyle = Button->GetStyle();
    // Retain the Slate root just as the HUD does; otherwise NativeDestruct correctly unbinds.
    const TSharedRef<SWidget> Slate = Widget->TakeWidget();
    Widget->InitializeOverview(nullptr);
    Widget->RefreshState();
    Widget->RefreshState();
    TestEqual(TEXT("Pressed handler is bound to this WBP exactly once"), CountOwnerBindings(Button->OnPressed, *Widget), 1);
    TestEqual(TEXT("Clicked handler is bound to this WBP exactly once"), CountOwnerBindings(Button->OnClicked, *Widget), 1);
    TestFalse(TEXT("Toolbar wrapper is disabled without an active construction subsystem"), Widget->GetIsEnabled());
    TestFalse(TEXT("Toolbar button cannot accept outside-build clicks"), Button->GetIsEnabled());
    TestFalse(TEXT("Whole grid is unselected outside construction"), Widget->IsWholeGridSelected());
    Button->OnPressed.Broadcast();
    Button->OnClicked.Broadcast();
    Widget->ActivateWholeGrid();
    TestFalse(TEXT("Direct activation without a subsystem is a no-op"), Widget->IsWholeGridSelected());
    TestFalse(TEXT("Invalid activation cannot enable the outside-build button"), Button->GetIsEnabled());
    TestTrue(TEXT("Runtime preserves static Designer caption"), Label->GetText().EqualTo(AuthoredCaption));
    TestTrue(TEXT("Runtime preserves authored typography"), Label->GetFont() == AuthoredFont);
    TestTrue(TEXT("Outside-build refresh retains authored normal brush"), Button->GetStyle().Normal == AuthoredStyle.Normal);
    TestTrue(TEXT("Outside-build refresh retains authored button padding"), Button->GetStyle().NormalPadding == AuthoredStyle.NormalPadding);
    VerifyGlyph(*this, *Widget, AuthoredGlyph);
    Widget->ReleaseSlateResources(true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFMapWholeGridDesignerPreservedTest,
    "FF_MapOverview.Toolbar.WholeGridDesignerContentAndBindingsSurviveRebuild",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFMapWholeGridDesignerPreservedTest::RunTest(const FString&)
{
    TStrongObjectPtr<UFFMapWholeGridButtonWidget> Instance(CreateAuthoredWholeGridButton(*this));
    UFFMapWholeGridButtonWidget* Widget = Instance.Get();
    if (!Widget) return false;
    UWidgetTree* Tree = Widget->WidgetTree;
    USizeBox* Bounds = Cast<USizeBox>(Widget->GetWidgetFromName(TEXT("Bounds")));
    UButton* Button = Cast<UButton>(Widget->GetWidgetFromName(TEXT("WholeGridButton")));
    UTextBlock* Label = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Label")));
    USizeBox* IconBounds = Cast<USizeBox>(Widget->GetWidgetFromName(TEXT("IconBounds")));
    USizeBoxSlot* Slot = Button ? Cast<USizeBoxSlot>(Button->Slot) : nullptr;
    if (!TestNotNull(TEXT("Designer tree"), Tree) || !TestNotNull(TEXT("Designer bounds"), Bounds) ||
        !TestNotNull(TEXT("Designer button"), Button) || !TestNotNull(TEXT("Designer caption"), Label) ||
        !TestNotNull(TEXT("Designer glyph bounds"), IconBounds) || !TestNotNull(TEXT("Designer button slot"), Slot)) return false;
    if (!TestNotNull(TEXT("Designer glyph subtree"), IconBounds->GetContent())) return false;
    // Edit only this transient instance before construction. No Blueprint or teammate asset is saved.
    const float DesignerWidth = Bounds->GetWidthOverride() + 13.0f;
    const float DesignerHeight = Bounds->GetHeightOverride() + 7.0f;
    const FMargin DesignerPadding(9.0f, 5.0f, 11.0f, 6.0f);
    const FName SlotName = Slot->GetFName();
    Bounds->SetWidthOverride(DesignerWidth);
    Bounds->SetHeightOverride(DesignerHeight);
    Slot->SetPadding(DesignerPadding);
    const FText DesignerCaption = FText::FromString(TEXT("Designer-authored grid caption"));
    Label->SetText(DesignerCaption);
    FSlateFontInfo DesignerFont = Label->GetFont();
    DesignerFont.Size += 2.0f;
    Label->SetFont(DesignerFont);
    FButtonStyle DesignerStyle = Button->GetStyle();
    DesignerStyle.Normal.TintColor = FSlateColor(FLinearColor(0.74f, 0.12f, 0.51f, 1.0f));
    DesignerStyle.NormalPadding = FMargin(17.0f, 9.0f);
    Button->SetStyle(DesignerStyle);
    if (IconBounds->IsWidthOverride()) IconBounds->SetWidthOverride(IconBounds->GetWidthOverride() + 3.0f);
    if (IconBounds->IsHeightOverride()) IconBounds->SetHeightOverride(IconBounds->GetHeightOverride() + 2.0f);
    const TArray<FAuthoredGlyphNode> AuthoredGlyph = CaptureGlyph(*IconBounds);
    Widget->bUseSelectionStyle = true;
    Widget->SelectionStyle = DesignerStyle;
    Widget->SelectedIconColor = FLinearColor(0.14f, 0.84f, 0.52f, 1.0f);
    Widget->SelectedTextColor = FSlateColor(FLinearColor(0.3f, 0.8f, 0.7f, 1.0f));
    const FLinearColor SelectedIconColor = Widget->SelectedIconColor;
    const FSlateColor SelectedTextColor = Widget->SelectedTextColor;

    TSharedPtr<SWidget> Slate = Widget->TakeWidget();
    Widget->RefreshState();
    Slate.Reset();
    Widget->ReleaseSlateResources(true);
    Slate = Widget->TakeWidget();
    Widget->RefreshState();
    TestTrue(TEXT("Runtime retains the exact Designer tree and root"), Widget->WidgetTree == Tree && Widget->GetRootWidget() == Bounds);
    TestTrue(TEXT("Runtime retains the authored button object and slot"), Widget->GetWidgetFromName(TEXT("WholeGridButton")) == Button && Button->Slot == Slot);
    TestEqual(TEXT("Runtime retains the Designer slot name"), Slot->GetFName(), SlotName);
    TestEqual(TEXT("Runtime retains Designer width"), Bounds->GetWidthOverride(), DesignerWidth);
    TestEqual(TEXT("Runtime retains Designer height"), Bounds->GetHeightOverride(), DesignerHeight);
    TestTrue(TEXT("Runtime retains Designer slot padding"), Slot->GetPadding() == DesignerPadding);
    TestTrue(TEXT("Slate rebuild does not overwrite static caption"), Label->GetText().EqualTo(DesignerCaption));
    TestTrue(TEXT("Slate rebuild preserves actual font metadata"), Label->GetFont() == DesignerFont);
    TestTrue(TEXT("Slate rebuild preserves authored normal brush"), Button->GetStyle().Normal == DesignerStyle.Normal);
    TestTrue(TEXT("Slate rebuild preserves authored style padding"), Button->GetStyle().NormalPadding == DesignerStyle.NormalPadding);
    VerifyGlyph(*this, *Widget, AuthoredGlyph);
    TestTrue(TEXT("Runtime preserves Designer selection-style configuration"), Widget->bUseSelectionStyle &&
        Widget->SelectionStyle.Normal == DesignerStyle.Normal && Widget->SelectionStyle.NormalPadding == DesignerStyle.NormalPadding);
    TestEqual(TEXT("Runtime preserves Designer selected icon color"), Widget->SelectedIconColor, SelectedIconColor);
    TestTrue(TEXT("Runtime preserves Designer selected label color"), Widget->SelectedTextColor == SelectedTextColor);
    TestEqual(TEXT("Slate rebuild binds one pressed callback"), CountOwnerBindings(Button->OnPressed, *Widget), 1);
    TestEqual(TEXT("Slate rebuild binds one clicked callback"), CountOwnerBindings(Button->OnClicked, *Widget), 1);
    Widget->ReleaseSlateResources(true);
    return true;
}

#endif
