// Exercise the saved facility inspector, including the ancestors that determine
// whether its existing revenue text can actually be seen by a player.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AssetCompilingManager.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
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
    if (!TestNotNull(TEXT("Existing revenue binding"), Value) ||
        !TestNotNull(TEXT("Authored overview revenue card"), Card) ||
        !TestNotNull(TEXT("Scrollable detail body"), Scroll) ||
        !TestNotNull(TEXT("Fixed move/remove actions"), Actions)) return false;
    TestTrue(TEXT("Saved WBP places the real binding in the visible revenue card"), Card->GetContent() == Value);

    // Presentation fixture only: economy tests separately verify the settled quote.
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
    Data.FacilityRevenueText = FText::FromString(TEXT(
        "팀당 예상 수익 · 일반 이용 1회\n1명 4 · 2명 7 · 3명 11 Gold\n4명 14 · 5명 18 · 6명 22 Gold"));
    Panel->ApplyViewData(Data);
    Panel->SetInfoPage(EFF_FacilityInfoPage::Overview);
    Panel->SetPanelOpen(true);
    TestEqual(TEXT("Native data reaches the authored revenue text"), Value->GetText().ToString(), Data.FacilityRevenueText.ToString());
    TestTrue(TEXT("No hidden or collapsed ancestor conceals the team quote"), HasVisibleAncestors(Value));

    const bool bCanRender = !FParse::Param(FCommandLine::Get(), TEXT("nullrhi"));
    if (bCanRender)
    {
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
            TestTrue(TEXT("All three quote lines have visible geometry"), ValueRect.GetSize().Y >= 40.f &&
                ValueRect.GetSize().X > 100.f && HasVisibleAncestors(Value));
            TestTrue(TEXT("Revenue fits horizontally inside its card"),
                ValueRect.Left >= CardRect.Left && ValueRect.Right <= CardRect.Right + 1.f);
            TestTrue(TEXT("The first overview screen contains the complete team quote"),
                CardRect.Top >= ScrollRect.Top - 1.f && CardRect.Bottom <= ScrollRect.Bottom + 1.f);
            TestTrue(*FString::Printf(TEXT("Ordinary overview facts remain readable without scrolling at %dp (%.1fpx)"),
                Resolution.Y, Scroll->GetScrollOffsetOfEnd()), Scroll->GetScrollOffsetOfEnd() <= 1.f);
            TestTrue(TEXT("The quote cannot cover the fixed facility actions"),
                ActionsRect.Top >= ScrollRect.Bottom - 1.f && CardRect.Bottom <= ActionsRect.Top);

            TArray<FColor> Pixels;
            FReadSurfaceDataFlags Flags;
            Flags.SetLinearToGamma(false);
            if (TestTrue(TEXT("Read actual WBP rendered pixels"),
                Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)))
            {
                TArray64<uint8> Png;
                FImageUtils::PNGCompressImageArray(Resolution.X, Resolution.Y, Pixels, Png);
                const FString Path = Directory / FString::Printf(TEXT("TeamRevenueOverview_%d.png"), Resolution.Y);
                TestTrue(TEXT("Save team-revenue overview screenshot"), FFileHelper::SaveArrayToFile(Png, *Path));
                AddInfo(Path);
            }
        }
    }
    else
    {
        AddInfo(TEXT("Geometry screenshots require -RenderOffscreen with a real RHI; saved-asset visibility still verified."));
    }

    Data.Context = EFF_FacilityInfoContext::Live;
    Data.FacilityRevenueText = FText::FromString(TEXT("이 시설은 이용 수익이 없습니다."));
    Panel->ApplyViewData(Data);
    Panel->SetInfoPage(EFF_FacilityInfoPage::Overview);
    TestEqual(TEXT("Selecting a non-revenue facility replaces the previous team's quote"),
        Value->GetText().ToString(), Data.FacilityRevenueText.ToString());
    TestTrue(TEXT("Live overview also exposes the team-revenue explanation"), HasVisibleAncestors(Value));
    Panel->ReleaseSlateResources(true);
    return true;
}
#endif
