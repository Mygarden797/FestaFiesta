// Exercise the saved facility inspector, including the ancestors that determine
// whether its existing revenue text can actually be seen by a player.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AssetCompilingManager.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Building/FF_FacilityUpgradeTypes.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Economy/FF_FestivalEconomyTypes.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "RHITypes.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"

namespace
{
bool HasVisibleAncestors(const UWidget* Widget)
{
    for (const UWidget* Ancestor = Widget; Ancestor; Ancestor = Ancestor->GetParent())
        if (Ancestor->GetVisibility() == ESlateVisibility::Hidden ||
            Ancestor->GetVisibility() == ESlateVisibility::Collapsed) return false;
    return true;
}

class FScopedTeamRevenuePlayerlessInitialization
{
public:
    explicit FScopedTeamRevenuePlayerlessInitialization(UWidgetBlueprintGeneratedClass& InClass)
        : Class(InClass), bPrevious(InClass.bCanCallInitializedWithoutPlayerContext)
    {
        Class.bCanCallInitializedWithoutPlayerContext = true;
    }
    ~FScopedTeamRevenuePlayerlessInitialization()
    {
        Class.bCanCallInitializedWithoutPlayerContext = bPrevious;
    }
private:
    UWidgetBlueprintGeneratedClass& Class;
    bool bPrevious;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityTeamRevenueLayoutTest,
    "FestaFiesta.UI.FacilityInfo.TeamRevenueLayout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityTeamRevenueLayoutTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_FacilityInfoPanel>(
        nullptr, FFBuildUIAssetPaths::FacilityInfoPanelClass));
    if (!TestNotNull(TEXT("Editor world"), World) ||
        !TestNotNull(TEXT("Saved facility inspector WBP"), Class)) return false;
    FScopedTeamRevenuePlayerlessInitialization Initialization(*Class);
    UFF_FacilityInfoPanel* Panel = CreateWidget<UFF_FacilityInfoPanel>(World, Class);
    if (!TestNotNull(TEXT("Actual facility inspector instance"), Panel)) return false;

    const TSharedRef<SWidget> Slate = Panel->TakeWidget();
    auto* Value = Cast<UCommonTextBlock>(Panel->GetWidgetFromName(TEXT("Text_FacilityRevenue")));
    auto* Card = Cast<UBorder>(Panel->GetWidgetFromName(TEXT("TeamRevenue_OverviewCard")));
    auto* Scroll = Cast<UScrollBox>(Panel->GetWidgetFromName(TEXT("ScrollBox_Details")));
    UWidget* Actions = Panel->GetWidgetFromName(TEXT("Panel_Actions"));
    UWidget* Basics = Panel->GetWidgetFromName(TEXT("Clarity_BasicFacts"));
    UWidget* FirstFact = Panel->GetWidgetFromName(TEXT("Clarity_Row_Text_QueueCapacityValue"));
    auto* LastFact = Cast<UCommonTextBlock>(Panel->GetWidgetFromName(TEXT("Text_InstanceLimit")));
    auto* Move = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_Move")));
    auto* Remove = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_Remove")));
    UWidget* UpgradeRevenueRow = Panel->GetWidgetFromName(TEXT("V2_UpgradeRowRevenue"));
    UWidget* UpgradeTimeRow = Panel->GetWidgetFromName(TEXT("V2_UpgradeRowTime"));
    auto* UpgradeCurrentRevenue = Cast<UCommonTextBlock>(Panel->GetWidgetFromName(TEXT("Text_UpgradeCurrentRevenue")));
    auto* UpgradeNextRevenue = Cast<UCommonTextBlock>(Panel->GetWidgetFromName(TEXT("Text_UpgradeNextRevenue")));
    UWidget* RevenueTab = Panel->GetWidgetFromName(TEXT("Button_TabRevenue"));
    UWidget* RevenueSection = Panel->GetWidgetFromName(TEXT("Panel_RevenueSection"));
    auto* OperationValue = Cast<UCommonTextBlock>(Panel->GetWidgetFromName(TEXT("Text_Occupancy")));
    if (!TestNotNull(TEXT("Existing revenue binding"), Value) ||
        !TestNotNull(TEXT("Authored overview revenue card"), Card) ||
        !TestNotNull(TEXT("Scrollable detail body"), Scroll) ||
        !TestNotNull(TEXT("Fixed move/remove actions"), Actions) ||
        !TestNotNull(TEXT("Overview facts column"), Basics) ||
        !TestNotNull(TEXT("First overview fact row"), FirstFact) ||
        !TestNotNull(TEXT("Last overview fact"), LastFact) ||
        !TestNotNull(TEXT("Existing move action"), Move) ||
        !TestNotNull(TEXT("Existing remove action"), Remove) ||
        !TestNotNull(TEXT("Authored upgrade revenue row"), UpgradeRevenueRow) ||
        !TestNotNull(TEXT("Authored upgrade service-time row"), UpgradeTimeRow) ||
        !TestNotNull(TEXT("Current upgrade revenue cell"), UpgradeCurrentRevenue) ||
        !TestNotNull(TEXT("Next upgrade revenue cell"), UpgradeNextRevenue) ||
        !TestNotNull(TEXT("Revenue tab"), RevenueTab) ||
        !TestNotNull(TEXT("Revenue page"), RevenueSection) ||
        !TestNotNull(TEXT("Existing operation facts"), OperationValue)) return false;
    TestTrue(TEXT("Saved WBP places the real binding in the visible revenue card"), Card->GetContent() == Value);

    // The presentation fixture uses the real model for quote and ledger eligibility.
    // Economy tests separately verify settlement and its final rounding.
    FFF_FacilityInfoViewData Data;
    Data.FacilityInstanceId = FGuid::NewGuid();
    Data.FacilityDefinitionId = TEXT("TeamRevenue_LayoutFixture");
    Data.Context = EFF_FacilityInfoContext::Build;
    Data.DisplayName = FText::FromString(TEXT("팀당 수익 검증 시설"));
    Data.CategoryText = FText::FromString(TEXT("부스"));
    Data.bCanMove = Data.bCanRemove = true;
    Data.InstallCostText = FText::FromString(TEXT("300 Gold"));
    Data.RecoveryCostText = FText::FromString(TEXT("150 Gold"));
    Data.QueueCapacityValueText = FText::FromString(TEXT("5팀"));
    Data.ServiceCapacityValueText = FText::FromString(TEXT("1팀"));
    Data.ServiceDurationText = FText::FromString(TEXT("6초"));
    Data.FootprintText = FText::FromString(TEXT("3.2 × 1.97 m"));
    Data.ClearanceText = FText::FromString(TEXT("1 m"));
    Data.InstanceLimitText = FText::FromString(TEXT("현재 2 / 최대 12 · 10개 남음"));
    Data.OperationStateText = FText::FromString(TEXT("운영 중 · 새 손님 이용 가능"));
    Data.OccupancyText = FText::FromString(TEXT("0 / 6팀"));
    Data.WaitingPartiesText = FText::FromString(TEXT("0팀"));
    Data.RemainingServiceText = FText::FromString(TEXT("0팀"));
    Data.ExpectedWaitText = FText::FromString(TEXT("0초"));
    Data.ThroughputText = FText::FromString(TEXT("6초"));
    FFF_FacilityUpgradePreview PaidPreview;
    PaidPreview.bRevenueAvailable = true;
    PaidPreview.BaseRevenuePerUseGold = PaidPreview.CurrentRevenuePerUseGold = 4;
    PaidPreview.NextRevenuePerUseGold = 6;
    PaidPreview.CurrentRevenueByTeamSizeGold = {4, 7, 11, 14, 18, 22};
    FFF_FacilityInstanceRevenue NoSales;
    FFFacilityInfoModel::ApplyUpgradePreview(&PaidPreview, Data);
    FFFacilityInfoModel::ApplyFacilityRevenue(&NoSales, true, true, 0, Data);
    Panel->ApplyViewData(Data);
    Panel->SetInfoPage(EFF_FacilityInfoPage::Overview);
    Panel->SetPanelOpen(true);
    TestEqual(TEXT("Native data reaches the authored revenue text"), Value->GetText().ToString(), Data.FacilityRevenueText.ToString());
    TestTrue(TEXT("No hidden or collapsed ancestor conceals the team quote"), HasVisibleAncestors(Value));
    TestEqual(TEXT("Actual paid facility has no settled sales yet"), Data.TodayRevenueGold, 0);
    TestTrue(TEXT("No-sales paid facility keeps its expected team quote and revenue tab"),
        !Value->GetText().IsEmpty() && HasVisibleAncestors(Card) && HasVisibleAncestors(RevenueTab));

    const bool bCanRender = !FParse::Param(FCommandLine::Get(), TEXT("nullrhi"));
    auto RenderOverview = [&](const TCHAR* FileStem, const bool bShowQuote)
    {
        if (!bCanRender) return;
        const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/FacilityTeamRevenue");
        IFileManager::Get().MakeDirectory(*Directory, true);
        for (const FIntPoint Resolution : {FIntPoint(1280, 720), FIntPoint(1920, 1080)})
        {
            FWidgetRenderer Renderer(false, true);
            UTextureRenderTarget2D* Target = FWidgetRenderer::CreateTargetFor(
                FVector2D(Resolution.X, Resolution.Y), TF_Bilinear, true);
            if (!TestNotNull(TEXT("Offscreen render target"), Target)) continue;
            for (int32 Frame = 0; Frame < 3; ++Frame)
            {
                Panel->ForceLayoutPrepass();
                Renderer.DrawWidget(Target, Slate, FVector2D(Resolution.X, Resolution.Y), 0.f, false);
                FlushRenderingCommands();
                FAssetCompilingManager::Get().FinishAllCompilation();
            }
            const FSlateRect ValueRect = Value->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            const FSlateRect CardRect = Card->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            const FSlateRect ScrollRect = Scroll->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            const FSlateRect ActionsRect = Actions->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            const FSlateRect LastFactRect = LastFact->GetPaintSpaceGeometry().GetLayoutBoundingRect();
            if (bShowQuote)
            {
                TestTrue(TEXT("All three quote lines have visible geometry"), ValueRect.GetSize().Y >= 40.f &&
                    ValueRect.GetSize().X > 100.f && HasVisibleAncestors(Value));
                TestTrue(TEXT("Revenue fits horizontally inside its card"),
                    ValueRect.Left >= CardRect.Left && ValueRect.Right <= CardRect.Right + 1.f);
                TestTrue(TEXT("The first overview screen contains the complete team quote"),
                    CardRect.Top >= ScrollRect.Top - 1.f && CardRect.Bottom <= ScrollRect.Bottom + 1.f);
            }
            else
            {
                const FSlateRect BasicsRect = Basics->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                const FSlateRect FirstFactRect = FirstFact->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                TestTrue(*FString::Printf(TEXT("Non-revenue overview leaves no empty card gap at %dp (%.1fpx)"),
                    Resolution.Y, FirstFactRect.Top - BasicsRect.Top),
                    FirstFactRect.Top >= BasicsRect.Top - 1.f && FirstFactRect.Top <= BasicsRect.Top + 16.f);
                TestEqual(TEXT("Rendered non-revenue card stays collapsed"), Card->GetVisibility(), ESlateVisibility::Collapsed);
                TestTrue(TEXT("First ordinary facility fact remains visible"),
                    HasVisibleAncestors(FirstFact) && FirstFactRect.GetSize().Y > 0.f);
            }
            TestTrue(*FString::Printf(TEXT("Ordinary overview facts remain readable without scrolling at %dp (%.1fpx)"),
                Resolution.Y, Scroll->GetScrollOffsetOfEnd()), Scroll->GetScrollOffsetOfEnd() <= 1.f);
            TestTrue(TEXT("All ordinary overview facts remain inside the scroll viewport"),
                HasVisibleAncestors(LastFact) && LastFactRect.Bottom <= ScrollRect.Bottom + 1.f);
            if (Data.Context == EFF_FacilityInfoContext::Build)
                TestTrue(TEXT("Overview facts and any quote cannot cover the fixed facility actions"),
                    HasVisibleAncestors(Actions) && ActionsRect.Top >= ScrollRect.Bottom - 1.f &&
                    LastFactRect.Bottom <= ActionsRect.Top && (!bShowQuote || CardRect.Bottom <= ActionsRect.Top));

            TArray<FColor> Pixels;
            FReadSurfaceDataFlags Flags;
            Flags.SetLinearToGamma(false);
            if (TestTrue(TEXT("Read actual WBP rendered pixels"),
                Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)))
            {
                TArray64<uint8> Png;
                FImageUtils::PNGCompressImageArray(Resolution.X, Resolution.Y, Pixels, Png);
                const FString Path = Directory / FString::Printf(TEXT("%s_%d.png"), FileStem, Resolution.Y);
                TestTrue(TEXT("Save actual facility overview screenshot"), FFileHelper::SaveArrayToFile(Png, *Path));
                AddInfo(Path);
            }
        }
    };
    RenderOverview(TEXT("TeamRevenueOverview"), true);
    if (!bCanRender)
    {
        AddInfo(TEXT("Geometry screenshots require -RenderOffscreen with a real RHI; saved-asset visibility still verified."));
    }

    PaidPreview.bHasLevels = PaidPreview.bSupported = PaidPreview.bHasUpgradeQuote = PaidPreview.bCanUpgrade = true;
    PaidPreview.TargetLevel = 2;
    PaidPreview.CostGold = 120;
    PaidPreview.GoldAfterUpgrade = 880;
    PaidPreview.CurrentServiceDurationSeconds = 6.f;
    PaidPreview.NextServiceDurationSeconds = 4.8f;
    PaidPreview.CurrentThroughputPerMinute = 10.f;
    PaidPreview.NextThroughputPerMinute = 12.5f;
    PaidPreview.CurrentFootprintCentimeters = PaidPreview.NextFootprintCentimeters = FIntPoint(320, 197);
    FFF_FacilityUpgradePreview FreePreview = PaidPreview;
    FreePreview.BaseRevenuePerUseGold = FreePreview.CurrentRevenuePerUseGold = FreePreview.NextRevenuePerUseGold = 0;
    FreePreview.CurrentRevenueByTeamSizeGold = {0, 0, 0, 0, 0, 0};

    for (const EFF_FacilityInfoContext Context : {EFF_FacilityInfoContext::Build, EFF_FacilityInfoContext::Live})
    {
        Data.Context = Context;
        Data.FacilityInstanceId = FGuid::NewGuid();
        FFFacilityInfoModel::ApplyUpgradePreview(&PaidPreview, Data);
        FFFacilityInfoModel::ApplyFacilityRevenue(&NoSales, true, true, 0, Data);
        Panel->ApplyViewData(Data);
        Panel->SetInfoPage(EFF_FacilityInfoPage::Overview);
        TestTrue(TEXT("Paid selection exposes the expected quote in both contexts"),
            !Value->GetText().IsEmpty() && HasVisibleAncestors(Value) && HasVisibleAncestors(Card));
        if (Context == EFF_FacilityInfoContext::Build)
        {
            Panel->SetInfoPage(EFF_FacilityInfoPage::Upgrade);
            TestTrue(TEXT("Paid upgrade retains its existing revenue comparison row"), HasVisibleAncestors(UpgradeRevenueRow));
        }

        Data.FacilityInstanceId = FGuid::NewGuid();
        Data.DisplayName = FText::FromString(TEXT("이용 수익 없는 시설"));
        FFFacilityInfoModel::ApplyUpgradePreview(&FreePreview, Data);
        FFFacilityInfoModel::ApplyFacilityRevenue(&NoSales, true, false, 0, Data);
        Panel->ApplyViewData(Data);
        Panel->SetInfoPage(EFF_FacilityInfoPage::Overview);
        TestTrue(TEXT("Non-revenue selection clears the previous facility's estimate"), Value->GetText().IsEmpty());
        TestEqual(TEXT("Non-revenue quote value collapses in Build and Live"), Value->GetVisibility(), ESlateVisibility::Collapsed);
        TestEqual(TEXT("Whole non-revenue overview card collapses in Build and Live"), Card->GetVisibility(), ESlateVisibility::Collapsed);
        TestEqual(TEXT("Non-revenue facility hides the revenue tab"), RevenueTab->GetVisibility(), ESlateVisibility::Collapsed);
        TestTrue(TEXT("Free service clears both upgrade revenue values"),
            UpgradeCurrentRevenue->GetText().IsEmpty() && UpgradeNextRevenue->GetText().IsEmpty());
        TestEqual(TEXT("Free service removes the whole upgrade revenue row"), UpgradeRevenueRow->GetVisibility(), ESlateVisibility::Collapsed);
        if (Context == EFF_FacilityInfoContext::Build)
        {
            Panel->SetInfoPage(EFF_FacilityInfoPage::Upgrade);
            TestTrue(TEXT("Free facility still exposes its service-time upgrade comparison"), HasVisibleAncestors(UpgradeTimeRow));
        }
        Panel->SetInfoPage(EFF_FacilityInfoPage::Revenue);
        TestEqual(TEXT("Non-revenue selection cannot reveal a stale revenue ledger"), RevenueSection->GetVisibility(), ESlateVisibility::Collapsed);
        Panel->SetInfoPage(EFF_FacilityInfoPage::Operation);
        TestTrue(TEXT("Non-revenue facility retains operation facts"), HasVisibleAncestors(OperationValue));
        TestEqual(TEXT("Non-revenue facility retains its actual occupancy text"),
            OperationValue->GetText().ToString(), Data.OccupancyText.ToString());
        Panel->SetInfoPage(EFF_FacilityInfoPage::Overview);
        TestTrue(TEXT("Non-revenue facility keeps ordinary build facts"), HasVisibleAncestors(FirstFact) && HasVisibleAncestors(LastFact));
        TestEqual(TEXT("Non-revenue facility keeps the authored construction-limit text"),
            LastFact->GetText().ToString(), Data.InstanceLimitText.ToString());
        TestEqual(TEXT("Move/remove actions retain their context visibility"), Actions->GetVisibility(),
            Context == EFF_FacilityInfoContext::Build ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        if (Context == EFF_FacilityInfoContext::Build)
            TestTrue(TEXT("Non-revenue facility's move and remove actions remain usable"),
                HasVisibleAncestors(Move) && HasVisibleAncestors(Remove) && Move->GetIsEnabled() && Remove->GetIsEnabled());
        RenderOverview(Context == EFF_FacilityInfoContext::Build
            ? TEXT("NonRevenueOverview_Build") : TEXT("NonRevenueOverview_Live"), false);

        Data.FacilityInstanceId = FGuid::NewGuid();
        FFFacilityInfoModel::ApplyUpgradePreview(&PaidPreview, Data);
        FFFacilityInfoModel::ApplyFacilityRevenue(&NoSales, true, true, 0, Data);
        Panel->ApplyViewData(Data);
        Panel->SetInfoPage(EFF_FacilityInfoPage::Overview);
        TestTrue(TEXT("Paid-free-paid selection restores the quote and its card"),
            HasVisibleAncestors(Value) && HasVisibleAncestors(Card) && !Value->GetText().IsEmpty());
        TestTrue(TEXT("Paid-free-paid selection restores the revenue tab without settled sales"),
            HasVisibleAncestors(RevenueTab) && Data.TodayRevenueGold == 0);
        if (Context == EFF_FacilityInfoContext::Build)
        {
            Panel->SetInfoPage(EFF_FacilityInfoPage::Upgrade);
            TestTrue(TEXT("Paid-free-paid selection restores the existing upgrade revenue row"),
                HasVisibleAncestors(UpgradeRevenueRow) && !UpgradeCurrentRevenue->GetText().IsEmpty());
            for (const bool bKeepNext : {false, true})
            {
                FFF_FacilityInfoViewData OneSidedRevenue = Data;
                if (bKeepNext) OneSidedRevenue.UpgradeCurrentValues.Revenue = FText::GetEmpty();
                else OneSidedRevenue.UpgradeNextValues.Revenue = FText::GetEmpty();
                Panel->ApplyViewData(OneSidedRevenue);
                Panel->SetInfoPage(EFF_FacilityInfoPage::Upgrade);
                TestTrue(TEXT("Either available upgrade revenue cell keeps the comparison row visible"),
                    HasVisibleAncestors(UpgradeRevenueRow));
            }
        }

        Data.FacilityInstanceId.Invalidate();
        FFFacilityInfoModel::ApplyUpgradePreview(nullptr, Data);
        FFFacilityInfoModel::ApplyFacilityRevenue(nullptr, true, false, 0, Data);
        Panel->ApplyViewData(Data);
        Panel->SetInfoPage(EFF_FacilityInfoPage::Overview);
        TestEqual(TEXT("Missing selection hides the estimate value"), Value->GetVisibility(), ESlateVisibility::Collapsed);
        TestEqual(TEXT("Missing selection hides the whole estimate card"), Card->GetVisibility(), ESlateVisibility::Collapsed);
        TestEqual(TEXT("Missing selection hides the revenue tab"), RevenueTab->GetVisibility(), ESlateVisibility::Collapsed);
    }
    Panel->ReleaseSlateResources(true);
    return true;
}
#endif
