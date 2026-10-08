// FF_BuildSynergyPresentationTest.cpp
// 역할: 시설 조합 화면의 생성·해제·실제 배치를 검증한다.

// One integration fixture covers presentation lifetime, bounded data and the real Slate layout.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Building/Presentation/FF_BuildSynergyWidget.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Building/Synergy/FF_FacilitySynergyTypes.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
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
#include "UI/Core/FF_BuildPearlStyle.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
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
        FFBuildUIAssetPaths::BuildSynergyClass));
    if (!TestNotNull(TEXT("Editable synergy WBP (generate with FF.CreateBuildSynergyWidget)"), SynergyClass)) { return false; }
    FScopedSynergyPlayerlessInitialization SynergyInitialization(*SynergyClass);
    TStrongObjectPtr<UFF_BuildSynergyWidget> Widget(CreateWidget<UFF_BuildSynergyWidget>(World, SynergyClass));
    if (!TestNotNull(TEXT("Actual synergy WBP instance"), Widget.Get())) { return false; }
    const TSharedRef<SWidget> SynergySlate = Widget->TakeWidget();
    const auto* SynergySize=Cast<USizeBox>(Widget->GetWidgetFromName(TEXT("Size_Synergy")));
    const bool bClarity=SynergySize&&FMath::IsNearlyEqual(SynergySize->GetWidthOverride(),412.f);
    if (!TestTrue(TEXT("Designer tree satisfies named runtime binding contract"), Widget->HasRequiredBindings())) { return false; }
    const FSlateFontInfo AuthoredFont = Widget->RowTitles[0]->GetFont();
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
    const auto* SynergySlot = SynergySize ? Cast<UCanvasPanelSlot>(SynergySize->Slot) : nullptr;
    TestTrue(TEXT("Live combination card uses the left margin below the top toolbar"),
        SynergySlot && SynergySlot->GetAnchors().Minimum.Equals(FVector2D::ZeroVector) &&
        SynergySlot->GetAlignment().Equals(FVector2D::ZeroVector) &&
        SynergySlot->GetPosition().Equals(FVector2D(32.f, 104.f)));
    TestFalse(TEXT("Missing catalog never exposes internal facility IDs"),
        Widget->RowDetails[0]->GetText().ToString().Contains(TEXT("Booth_")) ||
        Widget->RowDetails[0]->GetText().ToString().Contains(TEXT("Rest_")));
    TestTrue(TEXT("Runtime data keeps authored font and applies the pearl ink color"),
        Widget->RowTitles[0]->GetFont() == AuthoredFont &&
        Widget->RowTitles[0]->GetColorAndOpacity() == FSlateColor(FFBuildPearl::Ink));
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

    TStrongObjectPtr<UFF_BuildSynergyWidget> Fallback(CreateWidget<UFF_BuildSynergyWidget>(World,
        UFF_BuildSynergyWidget::StaticClass()));
    if (!TestNotNull(TEXT("Missing-asset native fallback"), Fallback.Get())) return false;
    Fallback->TakeWidget();
    Fallback->PresentPreview(Preview, FVector::ZeroVector);
    const auto* FallbackSize = Cast<USizeBox>(Fallback->GetWidgetFromName(TEXT("Size_Synergy")));
    const auto* FallbackSlot = FallbackSize ? Cast<UCanvasPanelSlot>(FallbackSize->Slot) : nullptr;
    TestTrue(TEXT("Native fallback shares the same left placement without changing its width"),
        FallbackSize && FallbackSize->GetWidthOverride() == 344.f && FallbackSlot &&
        FallbackSlot->GetAnchors().Minimum.Equals(FVector2D::ZeroVector) &&
        FallbackSlot->GetAlignment().Equals(FVector2D::ZeroVector) &&
        FallbackSlot->GetPosition().Equals(FVector2D(32.f, 104.f)));
    Fallback->ClearPreview();

    if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
    {
        AddInfo(TEXT("Lifecycle checked; use -RenderOffscreen for the four composed layout PNGs."));
        return true;
    }
    if(bClarity)
    {
        // One realistically long facility pair exercises wrapped localization in a
        // full three-rule card without changing any gameplay preview calculation.
        Widget->RowTitles[1]->SetText(FText::FromString(TEXT("2. 노을 속 추억을 남기는 포토 산책길")));
        Widget->RowDetails[1]->SetText(FText::FromString(TEXT("함께: 추억을 담는 포토 키오스크 · 20m 이내\n거리 초과 · 현재 31m · 적용 +0%\n게임장 조합 보너스: +0% → +10%")));
    }
    UWidgetBlueprintGeneratedClass* ToolsClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_BuildToolsWidget>(nullptr,
        FFBuildUIAssetPaths::BuildToolsClass));
    if (!TestNotNull(TEXT("Existing production placement toolbar"), ToolsClass)) { return false; }
    FScopedSynergyPlayerlessInitialization ToolsInitialization(*ToolsClass);
    TStrongObjectPtr<UFF_BuildToolsWidget> ToolsWidget(CreateWidget<UFF_BuildToolsWidget>(World, ToolsClass));
    if (!TestNotNull(TEXT("Existing placement toolbar instance"), ToolsWidget.Get())) { return false; }
    const TSharedRef<SWidget> ToolsSlate = ToolsWidget->TakeWidget();
    ToolsWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    auto* ToolsData = FindFProperty<FStructProperty>(ToolsClass, TEXT("ViewData"));
    if (!TestNotNull(TEXT("Presentation-only placement fixture seam"), ToolsData)) return false;
    ToolsData->ContainerPtrToValuePtr<FFF_BuildToolsViewData>(ToolsWidget.Get())->bPreviewActive = true;
    UBorder* ToolsPanel = Cast<UBorder>(ToolsWidget->GetWidgetFromName(TEXT("Border_Tools")));
    UBorder* InfoPanel = Cast<UBorder>(ToolsWidget->GetWidgetFromName(TEXT("Border_PlacementInfo")));
    UBorder* HelpPanel = Cast<UBorder>(ToolsWidget->GetWidgetFromName(TEXT("Border_PlacementHelp")));
    USizeBox* HelpSize = Cast<USizeBox>(ToolsWidget->GetWidgetFromName(TEXT("Size_BuildWorldHelp")));
    UWidget* HelpColumn = ToolsWidget->GetWidgetFromName(TEXT("Column_BuildIconHelp"));
    UScrollBox* HelpScroll = Cast<UScrollBox>(ToolsWidget->GetWidgetFromName(TEXT("Scroll_BuildHelp")));
    if (!TestNotNull(TEXT("Toolbar geometry for overlap check"), ToolsPanel) ||
        !TestNotNull(TEXT("Required-space inspector for overlap check"), InfoPanel) ||
        !TestNotNull(TEXT("Independent placement help for overlap check"), HelpPanel) ||
        !TestNotNull(TEXT("Authored help bounds"), HelpSize) ||
        !TestNotNull(TEXT("Authored scrollable help content"), HelpColumn) ||
        !TestNotNull(TEXT("Help keeps all instructions reachable after fitting"), HelpScroll)) return false;
    // The playerless editor world is Closed. Render the real placement dock
    // explicitly, as the build-tools visual fixture does, to measure overlap.
    ToolsPanel->SetVisibility(ESlateVisibility::Visible);
    InfoPanel->SetVisibility(ESlateVisibility::Visible);
    auto SetText = [&](const TCHAR* Name, const TCHAR* Value)
    {
        if (auto* Text = Cast<UTextBlock>(ToolsWidget->GetWidgetFromName(Name)))
        {
            Text->SetText(FText::FromString(Value));
            Text->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
    };
    SetText(TEXT("Text_FacilityName"), TEXT("구름 솜사탕 부스"));
    SetText(TEXT("Text_Footprint"), TEXT("시설 크기  3.2 × 1.97 m"));
    SetText(TEXT("Text_Queue"), TEXT("대기열 범위  4.35 × 6.65 m\n기본형 · 대기·이용 5팀\n7 기본 · 8 좌측 · 9 우측 · 0 U자"));
    const TSharedRef<SWidget> Composition = SNew(SOverlay)
        + SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(UFF_UIPalette::Get()->InGame.PaperSunken)]
        + SOverlay::Slot()[SynergySlate]
        + SOverlay::Slot()[ToolsSlate];
    FWidgetRenderer Renderer(false, true);
    for (const FIntPoint Size : { FIntPoint(1280, 720), FIntPoint(1920, 1080) })
    for (const bool bShowHelp : {false, true})
    {
        HelpPanel->SetVisibility(bShowHelp ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
        TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(FVector2D(Size), TF_Bilinear, true));
        if (!TestNotNull(TEXT("sRGB synergy capture target"), Target.Get())) { return false; }
        for(int32 Frame=0;Frame<5;++Frame)
        {
            Widget->ForceLayoutPrepass();ToolsWidget->ForceLayoutPrepass();
            if (bShowHelp)
            {
                // Match NativeTick's shared fitting rule after the real combo
                // content has a desired height. No runtime gameplay state is seeded.
                const float ReservedTop = 104.f + Widget->CardPanel->GetDesiredSize().Y + 16.f;
                const FVector2D HelpBounds = FFBuildToolsModel::ResolveHelpVerticalBounds(
                    HelpColumn->GetDesiredSize().Y + 24.f, Size.Y - 100.f, 525.f, ReservedTop);
                HelpSize->SetHeightOverride(HelpBounds.Y);
                if (auto* HelpSlot = Cast<UCanvasPanelSlot>(HelpSize->Slot))
                    HelpSlot->SetPosition(FVector2D(32.f, HelpBounds.X));
            }
            Renderer.DrawWidget(Target.Get(), Composition, FVector2D(Size), 0.0f, false);
        }
        const FSlateRect Card = Widget->CardPanel->GetCachedGeometry().GetLayoutBoundingRect();
        const FSlateRect Tools = ToolsPanel->GetCachedGeometry().GetLayoutBoundingRect();
        const FSlateRect Inspector = InfoPanel->GetCachedGeometry().GetLayoutBoundingRect();
        TestTrue(TEXT("Placement dock has measurable rendered geometry"), Tools.GetSize().X > 0 && Tools.GetSize().Y > 0);
        TestTrue(TEXT("Combo panel stays in the left scene margin below the upper HUD"),
            FMath::IsNearlyEqual(Card.Left, 32.f, 1.f) && FMath::IsNearlyEqual(Card.Top, 104.f, 1.f) &&
            Card.Right < Size.X * .4f && Card.Bottom < Tools.Top);
        TestTrue(TEXT("Required placement space and all three combination rules remain separate"),
            Inspector.GetSize().X > 0.f && Inspector.GetSize().Y > 0.f && Card.Right + 16.f <= Inspector.Left);
        if(bClarity) TestTrue(TEXT("Three wrapped recipes fit above the preferred help position"), Card.Bottom + 16.f <= 525.f);
        if (bShowHelp)
        {
            const FSlateRect Help = HelpPanel->GetCachedGeometry().GetLayoutBoundingRect();
            TestTrue(TEXT("Open instructions remain measurable below every combination rule"),
                Help.GetSize().Y > 0.f && Help.Top >= Card.Bottom + 15.f && Help.Bottom <= Size.Y - 99.f);
            TestTrue(TEXT("Help stays in the same left margin and cannot cover the placement inspector"),
                FMath::IsNearlyEqual(Help.Left, 32.f, 1.f) && Help.Right + 16.f <= Inspector.Left);
            TestTrue(TEXT("A short viewport retains a scrollable guide without clipping the combination card"),
                Size.Y > 720 || HelpScroll->GetScrollOffsetOfEnd() > 0.f);
        }
        const FSlateRect LegendRect=Widget->Legend->GetCachedGeometry().GetLayoutBoundingRect();
        TestTrue(TEXT("Full legend wraps inside the card without truncation"),
            LegendRect.Left>=Card.Left && LegendRect.Right<=Card.Right &&
            LegendRect.GetSize().Y+1.f>=Widget->Legend->GetDesiredSize().Y &&
            Widget->Legend->GetDesiredSize().X<=LegendRect.GetSize().X+1.f);
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
                Detail->GetDesiredSize().X <= DetailRect.GetSize().X + 1.0f &&
                Title->GetDesiredSize().Y <= TitleRect.GetSize().Y + 1.0f &&
                Detail->GetDesiredSize().Y <= DetailRect.GetSize().Y + 1.0f);
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
        const FString Path = Directory / FString::Printf(TEXT("Synergy%s_%dx%d.png"),
            bShowHelp ? TEXT("_Help") : TEXT(""), Size.X, Size.Y);
        TArray64<uint8> Png;
        FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
        TestTrue(TEXT("Synergy and existing toolbar PNG saved"), FFileHelper::SaveArrayToFile(Png, *Path));
        AddInfo(FString::Printf(TEXT("Synergy layout %.0f,%.0f..%.0f,%.0f; inspector left %.0f; help %s; toolbar top %.0f; PNG %s"),
            Card.Left, Card.Top, Card.Right, Card.Bottom, Inspector.Left, bShowHelp ? TEXT("open") : TEXT("closed"),
            Tools.Top, *FPaths::ConvertRelativePathToFull(Path)));
    }
    Widget->ClearPreview();
    return true;
}

#endif
