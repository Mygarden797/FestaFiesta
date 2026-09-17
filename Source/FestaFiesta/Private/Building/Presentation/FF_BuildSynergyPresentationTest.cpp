// One integration fixture covers presentation lifetime, bounded data and the real Slate layout.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Building/Presentation/FF_BuildSynergyWidget.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Building/Synergy/FF_FacilitySynergyTypes.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Slate/WidgetRenderer.h"
#include "Styling/CoreStyle.h"
#include "TextureResource.h"
#include "UI/Data/FF_UIPalette.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"

namespace
{
    struct FScopedSynergyPlayerlessInitialization
    {
        explicit FScopedSynergyPlayerlessInitialization(UWidgetBlueprintGeneratedClass& InClass)
            : Class(InClass), bPrevious(InClass.bCanCallInitializedWithoutPlayerContext)
        { Class.bCanCallInitializedWithoutPlayerContext = true; }
        ~FScopedSynergyPlayerlessInitialization()
        { Class.bCanCallInitializedWithoutPlayerContext = bPrevious; }
        UWidgetBlueprintGeneratedClass& Class;
        bool bPrevious;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildSynergyPresentationTest,
    "FestaFiesta.Building.Synergy.PresentationLifecycleAndLayout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFFBuildSynergyPresentationTest::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world for isolated presentation fixture"), World)) { return false; }
    UWidgetBlueprintGeneratedClass* SynergyClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_BuildSynergyWidget>(nullptr,
        TEXT("/Game/FestaFiesta/UI/Building/WBP_FF_BuildSynergy.WBP_FF_BuildSynergy_C")));
    if (!TestNotNull(TEXT("Editable synergy WBP (generate with FF.CreateBuildSynergyWidget)"), SynergyClass)) { return false; }
    FScopedSynergyPlayerlessInitialization SynergyInitialization(*SynergyClass);
    TStrongObjectPtr<UFF_BuildSynergyWidget> Widget(CreateWidget<UFF_BuildSynergyWidget>(World, SynergyClass));
    if (!TestNotNull(TEXT("Actual synergy WBP instance"), Widget.Get())) { return false; }
    const TSharedRef<SWidget> SynergySlate = Widget->TakeWidget();
    if (!TestTrue(TEXT("Designer tree satisfies named runtime binding contract"), Widget->HasRequiredBindings())) { return false; }
    const FSlateFontInfo AuthoredFont = Widget->RowTitles[0]->GetFont();
    const FSlateColor AuthoredColor = Widget->RowTitles[0]->GetColorAndOpacity();
    FFF_FacilitySynergyPreview Preview;
    Preview.bRulesAvailable = true;
    for (int32 Index = 0; Index < 3; ++Index)
    {
        FFF_FacilitySynergyPreviewEntry& Entry = Preview.Entries.AddDefaulted_GetRef();
        Entry.DisplayName = FText::FromString(Index == 0 ? TEXT("먹거리와 휴식")
            : Index == 1 ? TEXT("공연 뒤 한 끼") : TEXT("즐거운 체험 거리"));
        // A playerless fixture shows a loading label, never a raw definition ID.
        Entry.TargetFacilityId = TEXT("Booth_FoodStall_Burger");
        Entry.PartnerFacilityId = Index == 0 ? TEXT("Rest_Shade_Shade")
            : Index == 1 ? TEXT("Performance_MainStage_Test") : TEXT("Booth_CraftExperience_Test");
        Entry.bHasPartner = Index < 2;
        Entry.bInRange = Index == 0;
        Entry.DistanceMeters = Index == 0 ? 12.5f : 31.0f;
        Entry.MaxDistanceMeters = 20.0f;
        Entry.ConfiguredBonusPercent = 10.0f;
        Entry.AppliedBonusPercent = Index == 0 ? 10.0f : 0.0f;
        Entry.ProposedTotalBonusPercent = Entry.AppliedBonusPercent;
        Entry.PartnerWorldLocation = FVector(1250.0 + Index * 500.0, 0.0, 0.0);
    }
    Widget->PresentPreview(Preview, FVector::ZeroVector);
    TestFalse(TEXT("Missing catalog never exposes internal facility IDs"),
        Widget->RowDetails[0]->GetText().ToString().Contains(TEXT("Booth_")) ||
        Widget->RowDetails[0]->GetText().ToString().Contains(TEXT("Rest_")));
    TestTrue(TEXT("Runtime data keeps authored font and text color"),
        Widget->RowTitles[0]->GetFont() == AuthoredFont &&
        (Widget->bApplyStateTextColors || Widget->RowTitles[0]->GetColorAndOpacity() == AuthoredColor));
    TestEqual(TEXT("Read-only overlay never consumes placement clicks"),
        Widget->GetVisibility(), ESlateVisibility::HitTestInvisible);
    TestEqual(TEXT("Missing counterpart has no misleading world marker"), Widget->Markers.Num(), 2);
    TestTrue(TEXT("Active and unmet counterparts retain different visual states"),
        Widget->Markers[0].bActive && !Widget->Markers[1].bActive);
    Widget->ClearPreview();
    TestEqual(TEXT("Cancel/pause clear the complete panel"), Widget->GetVisibility(), ESlateVisibility::Collapsed);
    TestTrue(TEXT("Clear removes world geometry, not just text"), Widget->Markers.IsEmpty() && Widget->RangePoints.IsEmpty());
    Widget->PresentPreview(Preview, FVector(500.0, 0.0, 0.0));
    TestTrue(TEXT("A resumed or new preview replaces the prior origin"), Widget->CandidateWorldLocation.X == 500.0);
    Widget->PresentPreview(FFF_FacilitySynergyPreview(), FVector::ZeroVector);
    TestEqual(TEXT("A facility without relevant rules cannot leave old rows visible"),
        Widget->GetVisibility(), ESlateVisibility::Collapsed);
    Widget->PresentPreview(Preview, FVector::ZeroVector);

    if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
    {
        AddInfo(TEXT("Lifecycle checked; use -RenderOffscreen for the two real layout PNGs."));
        return true;
    }
    UWidgetBlueprintGeneratedClass* ToolsClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_BuildToolsWidget>(nullptr,
        TEXT("/Game/FestaFiesta/UI/Building/WBP_FF_BuildTools.WBP_FF_BuildTools_C")));
    if (!TestNotNull(TEXT("Existing production placement toolbar"), ToolsClass)) { return false; }
    FScopedSynergyPlayerlessInitialization ToolsInitialization(*ToolsClass);
    TStrongObjectPtr<UFF_BuildToolsWidget> ToolsWidget(CreateWidget<UFF_BuildToolsWidget>(World, ToolsClass));
    if (!TestNotNull(TEXT("Existing placement toolbar instance"), ToolsWidget.Get())) { return false; }
    const TSharedRef<SWidget> ToolsSlate = ToolsWidget->TakeWidget();
    ToolsWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    UBorder* ToolsPanel = Cast<UBorder>(ToolsWidget->GetWidgetFromName(TEXT("Border_Tools")));
    if (!TestNotNull(TEXT("Toolbar geometry for overlap check"), ToolsPanel)) { return false; }
    const TSharedRef<SWidget> Composition = SNew(SOverlay)
        + SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(UFF_UIPalette::Get()->InGame.PaperSunken)]
        + SOverlay::Slot()[SynergySlate]
        + SOverlay::Slot()[ToolsSlate];
    FWidgetRenderer Renderer(false, true);
    for (const FIntPoint Size : { FIntPoint(1280, 720), FIntPoint(1920, 1080) })
    {
        TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(FVector2D(Size), TF_Bilinear, true));
        if (!TestNotNull(TEXT("sRGB synergy capture target"), Target.Get())) { return false; }
        Renderer.DrawWidget(Target.Get(), Composition, FVector2D(Size), 0.0f, false);
        const FSlateRect Card = Widget->CardPanel->GetCachedGeometry().GetLayoutBoundingRect();
        const FSlateRect Tools = ToolsPanel->GetCachedGeometry().GetLayoutBoundingRect();
        TestTrue(TEXT("Combo panel fits between upper HUD and existing bottom toolbar"),
            Card.Left >= 0.0f && Card.Right <= Size.X && Card.Top >= 90.0f && Card.Bottom < Tools.Top);
        float PreviousBottom = Card.Top;
        for (int32 Index = 0; Index < 3; ++Index)
        {
            const UTextBlock* Title = Widget->RowTitles[Index];
            const UTextBlock* Detail = Widget->RowDetails[Index];
            const FSlateRect TitleRect = Title->GetCachedGeometry().GetLayoutBoundingRect();
            const FSlateRect DetailRect = Detail->GetCachedGeometry().GetLayoutBoundingRect();
            TestTrue(TEXT("Readable combo rows never overlap each other"),
                TitleRect.Top >= PreviousBottom && DetailRect.Top >= TitleRect.Bottom && DetailRect.Bottom < Card.Bottom);
            TestTrue(TEXT("Localized fixture text fits without ellipsis"),
                Title->GetDesiredSize().X <= TitleRect.GetSize().X + 1.0f &&
                Detail->GetDesiredSize().X <= DetailRect.GetSize().X + 1.0f);
            PreviousBottom = DetailRect.Bottom;
        }
        const FSlateRect FooterRect = Widget->Footer->GetCachedGeometry().GetLayoutBoundingRect();
        TestTrue(TEXT("Compact footer stays readable below every combo row"),
            FooterRect.Top >= PreviousBottom && FooterRect.Bottom <= Card.Bottom &&
            Widget->Footer->GetDesiredSize().X <= FooterRect.GetSize().X + 1.0f);
        TArray<FColor> Pixels;
        FReadSurfaceDataFlags ReadFlags;
        ReadFlags.SetLinearToGamma(false);
        if (!TestTrue(TEXT("Actual native widget renders complete image pixels"),
            Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, ReadFlags) && Pixels.Num() == Size.X * Size.Y))
        { return false; }
        const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/FacilitySynergy"));
        IFileManager::Get().MakeDirectory(*Directory, true);
        const FString Path = Directory / FString::Printf(TEXT("Synergy_%dx%d.png"), Size.X, Size.Y);
        TArray64<uint8> Png;
        FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
        TestTrue(TEXT("Synergy and existing toolbar PNG saved"), FFileHelper::SaveArrayToFile(Png, *Path));
        AddInfo(FString::Printf(TEXT("Synergy layout %.0f,%.0f..%.0f,%.0f; toolbar top %.0f; PNG %s"),
            Card.Left, Card.Top, Card.Right, Card.Bottom, Tools.Top, *FPaths::ConvertRelativePathToFull(Path)));
    }
    Widget->ClearPreview();
    return true;
}

#endif
