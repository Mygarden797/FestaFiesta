// 저장된 일일 결산 WBP에서 비서 카드가 스크롤 밖에 고정되는지 실제 렌더로 검사한다.
#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AssetCompilingManager.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "UI/LiveHUD/FF_LiveHudScreen.h"
#include "UI/LiveHUD/Panels/FF_LivePanelReport.h"
#include "UI/ViewModel/FF_DailyReportComment.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SOverlay.h"

namespace FFReportAssistantFocusLayoutTest
{
constexpr float Tolerance = .5f;

bool Contains(const FSlateRect& Outer, const FSlateRect& Inner)
{
    return Inner.Left >= Outer.Left - Tolerance && Inner.Right <= Outer.Right + Tolerance &&
        Inner.Top >= Outer.Top - Tolerance && Inner.Bottom <= Outer.Bottom + Tolerance;
}

bool Intersects(const FSlateRect& A, const FSlateRect& B)
{
    return A.Right > B.Left && B.Right > A.Left && A.Bottom > B.Top && B.Bottom > A.Top;
}

bool SameRect(const FSlateRect& A, const FSlateRect& B)
{
    return FMath::IsNearlyEqual(A.Left, B.Left, Tolerance) && FMath::IsNearlyEqual(A.Right, B.Right, Tolerance) &&
        FMath::IsNearlyEqual(A.Top, B.Top, Tolerance) && FMath::IsNearlyEqual(A.Bottom, B.Bottom, Tolerance);
}

bool SetFixture(FAutomationTestBase& Test, UFF_LivePanelReport& Panel, bool bFinalDay)
{
    // 표시 전용 합성 입력이다. 조언 문자열은 실제 결산 생성 규칙으로 만들며 게임 상태는 생성하지 않는다.
    FFF_DailyReportCommentInputs Inputs;
    Inputs.TopFacilityName = FText::FromString(TEXT("푸드트럭"));
    Inputs.TopFacilityGold = 731;
    Inputs.bHasSatisfaction = true;
    Inputs.WaitScore = 42;
    Inputs.DensityScore = 80;
    Inputs.PreferenceScore = 78;
    Inputs.NeedScore = 76;
    Inputs.AdmittedVisitors = 812;
    Inputs.DailyVisitorGoal = 900;
    Inputs.bFinalDay = bFinalDay;
    Inputs.DaysPlayed = 3;
    Inputs.FestivalTopFacilityName = Inputs.TopFacilityName;
    Inputs.FestivalTopFacilityGold = 2431;
    Inputs.FirstDaySatisfaction = 52;
    Inputs.FinalDaySatisfaction = 68;
    Inputs.GoalDaysMet = 2;
    const FFF_DailyReportCommentPresentation Comment = FFF_DailyReportCommentRules::BuildPresentation(Inputs);
    const TPair<const TCHAR*, FText> Values[] = {
        { TEXT("Text_DayLabel"), FText::FromString(bFinalDay ? TEXT("3일차 결산") : TEXT("1일차 결산")) },
        { TEXT("Text_AssistantAdvice"), Comment.Advice },
        { TEXT("Text_AssistantComment"), Comment.Details },
        { TEXT("Text_TodayRevenue"), FText::AsNumber(731) },
        { TEXT("Text_TodayExpense"), FText::AsNumber(219) },
        { TEXT("Text_TodayNetProfit"), FText::AsNumber(512) },
        { TEXT("Text_SimSatisfaction"), FText::AsNumber(68) },
        { TEXT("Text_Gold"), FText::AsNumber(3245) },
        { TEXT("Text_AdvanceLabel"), FText::FromString(bFinalDay ? TEXT("축제 마치기") : TEXT("다음 날 준비하기")) }
    };
    for (const auto& Value : Values)
    {
        auto* Text = Cast<UTextBlock>(Panel.GetWidgetFromName(Value.Key));
        if (!Test.TestNotNull(FString(TEXT("결산 표시 자료 / ")) + Value.Key, Text)) return false;
        Text->SetText(Value.Value);
        Text->SetVisibility(Value.Value.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
    }
    UWidget* Card = Panel.GetWidgetFromName(TEXT("Group_AssistantComment"));
    auto* Advance = Cast<UButton>(Panel.GetWidgetFromName(TEXT("Button_Advance")));
    if (!Test.TestNotNull(TEXT("비서 카드"), Card) || !Test.TestNotNull(TEXT("기존 다음 진행 버튼"), Advance)) return false;
    Card->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Advance->SetVisibility(ESlateVisibility::Visible);
    Advance->SetIsEnabled(true);
    Advance->SetRenderOpacity(1.f);
    for (const TCHAR* Name : { TEXT("Text_MissingNote"), TEXT("Text_AdvanceReason") })
    {
        if (UWidget* Widget = Panel.GetWidgetFromName(Name)) Widget->SetVisibility(ESlateVisibility::Collapsed);
    }
    Test.TestFalse(TEXT("실제 생성 규칙의 핵심 조언"), Comment.Advice.IsEmpty());
    Test.TestFalse(TEXT("실제 생성 규칙의 보조 정보"), Comment.Details.IsEmpty());
    return true;
}

bool SaveTarget(FAutomationTestBase& Test, UTextureRenderTarget2D& Target, FIntPoint Size, const FString& Name)
{
    TArray<FColor> Pixels;
    FReadSurfaceDataFlags Flags;
    Flags.SetLinearToGamma(false);
    if (!Test.TestTrue(Name + TEXT(" 픽셀 읽기"), Target.GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)
        && Pixels.Num() == Size.X * Size.Y)) return false;
    TArray64<uint8> Png;
    FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ReportAssistantFocus/Layout"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Path = FPaths::Combine(Directory, Name + TEXT(".png"));
    const bool bSaved = FFileHelper::SaveArrayToFile(Png, *Path);
    Test.TestTrue(Name + TEXT(" PNG 저장"), bSaved);
    Test.AddInfo(FString::Printf(TEXT("결산 비서 카드 캡처: %s"), *FPaths::ConvertRelativePathToFull(Path)));
    return bSaved;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ReportAssistantFocusLayoutTest,
    "FestaFiesta.UI.Report.AssistantFocusRenderedLayout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)

bool FFF_ReportAssistantFocusLayoutTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace FFReportAssistantFocusLayoutTest;
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    auto* PanelClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_LivePanelReport>(nullptr,
        TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelReport.WBP_FF_LivePanelReport_C")));
    UClass* HudClass = LoadClass<UFF_LiveHudScreen>(nullptr,
        TEXT("/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen.WBP_FF_LiveHudScreen_C"));
    if (!TestNotNull(TEXT("검사용 에디터 월드"), World) || !TestNotNull(TEXT("저장 결산 WBP"), PanelClass) ||
        !TestNotNull(TEXT("저장 라이브 HUD"), HudClass)) return false;
    if (!TestNull(TEXT("실제 게임 상태 없는 표시 검사"), World->GetGameInstance())) return false;
    const FMapProperty* LayoutProperty = FindFProperty<FMapProperty>(HudClass, TEXT("PanelLayouts"));
    if (!TestNotNull(TEXT("실제 HUD의 패널 크기 설정"), LayoutProperty)) return false;
    const auto* Layouts = LayoutProperty->ContainerPtrToValuePtr<TMap<EFF_LiveTab, FFF_LivePanelLayout>>(HudClass->GetDefaultObject());
    const FFF_LivePanelLayout* Layout = Layouts->Find(EFF_LiveTab::Report);
    if (!TestNotNull(TEXT("저장된 결산 패널 크기"), Layout)) return false;
    const bool bPlayerless = PanelClass->bCanCallInitializedWithoutPlayerContext;
    const bool bPanelDirty = PanelClass->GetOutermost()->IsDirty();
    const bool bHudDirty = HudClass->GetOutermost()->IsDirty();
    PanelClass->bCanCallInitializedWithoutPlayerContext = true;
    ON_SCOPE_EXIT
    {
        PanelClass->bCanCallInitializedWithoutPlayerContext = bPlayerless;
        TestEqual(TEXT("결산 에셋을 수정하지 않는다"), PanelClass->GetOutermost()->IsDirty(), bPanelDirty);
        TestEqual(TEXT("사용자 HUD 에셋을 수정하지 않는다"), HudClass->GetOutermost()->IsDirty(), bHudDirty);
    };
    TStrongObjectPtr<UFF_LivePanelReport> Panel(CreateWidget<UFF_LivePanelReport>(World, PanelClass));
    if (!TestNotNull(TEXT("실제 결산 표시 인스턴스"), Panel.Get())) return false;
    const TSharedRef<SWidget> PanelSlate = Panel->TakeWidget();
    Panel->HandlePanelOpened();
    ON_SCOPE_EXIT { Panel->HandlePanelClosed(); Panel->ReleaseSlateResources(true); };
    auto* Scroll = Cast<UScrollBox>(Panel->GetWidgetFromName(TEXT("Report_ScrollBody")));
    UWidget* Card = Panel->GetWidgetFromName(TEXT("Group_AssistantComment"));
    UWidget* Header = Panel->GetWidgetFromName(TEXT("Report_HeaderSize"));
    UWidget* Footer = Panel->GetWidgetFromName(TEXT("Report_Footer"));
    UWidget* FooterRule = Panel->GetWidgetFromName(TEXT("Report_FooterRule"));
    auto* Sections = Cast<UPanelWidget>(Panel->GetWidgetFromName(TEXT("Report_Sections")));
    auto* Advice = Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("Text_AssistantAdvice")));
    auto* Details = Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("Text_AssistantComment")));
    auto* Title = Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("AssistantFocus_Title")));
    if (!TestNotNull(TEXT("결산 스크롤"), Scroll) || !TestNotNull(TEXT("고정 비서 카드"), Card) ||
        !TestNotNull(TEXT("결산 헤더"), Header) || !TestNotNull(TEXT("기존 Footer"), Footer) ||
        !TestNotNull(TEXT("기존 Footer 구분선"), FooterRule) || !TestNotNull(TEXT("핵심 조언"), Advice) ||
        !TestNotNull(TEXT("보조 정보"), Details) || !TestNotNull(TEXT("비서 제목"), Title) ||
        !TestNotNull(TEXT("결산 섹션"), Sections)) return false;
    UWidget* FooterSection = FooterRule;
    while (FooterSection && FooterSection->GetParent() != Sections) FooterSection = FooterSection->GetParent();
    if (!TestNotNull(TEXT("구분선을 포함하는 기존 Footer 섹션"), FooterSection)) return false;
    TestTrue(TEXT("비서 카드는 스크롤 밖 같은 부모에 있다"),
        Card->GetParent() == Sections && Scroll->GetParent() == Sections && Footer->GetParent() == Sections);
    TestEqual(TEXT("비서 카드는 Footer 구분선 바로 앞에 있다"),
        Sections->GetChildIndex(Card) + 1, Sections->GetChildIndex(FooterSection));
    TestTrue(TEXT("핵심 조언 글자가 보조 정보보다 크다"), Advice->GetFont().Size > Details->GetFont().Size);
    FAssetCompilingManager::Get().FinishAllCompilation();
    FlushRenderingCommands();
    for (const FIntPoint Resolution : { FIntPoint(1280, 720), FIntPoint(1920, 1080) })
    {
        const float Dpi = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Resolution);
        // 이 검사는 패널 내부 배치가 대상이다. 외부 HUD의 도킹 위치와 크기는 수정하지 않는다.
        const TSharedRef<SWidget> Surface = SNew(SDPIScaler).DPIScale(Dpi)
        [
            SNew(SOverlay)
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
            [ SNew(SBox).WidthOverride(Layout->Width).HeightOverride(Layout->Height)[PanelSlate] ]
        ];
        FWidgetRenderer Renderer(false, true);
        TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(
            FVector2D(Resolution.X, Resolution.Y), TF_Bilinear, true));
        if (!TestNotNull(TEXT("결산 렌더 타깃"), Target.Get())) return false;
        const auto Draw = [&]()
        {
            for (int32 Frame = 0; Frame < 4; ++Frame)
            {
                Panel->ForceLayoutPrepass();
                Renderer.DrawWidget(Target.Get(), Surface, FVector2D(Resolution.X, Resolution.Y), 1.f / 60.f, false);
            }
            FlushRenderingCommands();
        };
        for (const bool bFinalDay : { false, true })
        {
            if (!SetFixture(*this, *Panel, bFinalDay)) return false;
            Scroll->SetScrollOffset(0.f); Draw();
            const FString Context = FString::Printf(TEXT("Report_Assistant_%s_%dx%d"),
                bFinalDay ? TEXT("FinalDay") : TEXT("Daily"), Resolution.X, Resolution.Y);
            const FSlateRect Screen(0.f, 0.f, Resolution.X, Resolution.Y);
            const FSlateRect CardRect = Card->GetCachedGeometry().GetLayoutBoundingRect();
            const FSlateRect FooterRect = Footer->GetCachedGeometry().GetLayoutBoundingRect();
            const FSlateRect FooterSectionRect = FooterSection->GetCachedGeometry().GetLayoutBoundingRect();
            const FSlateRect ScrollRect = Scroll->GetCachedGeometry().GetLayoutBoundingRect();
            const FSlateRect HeaderRect = Header->GetCachedGeometry().GetLayoutBoundingRect();
            const FSlateRect AdviceRect = Advice->GetCachedGeometry().GetLayoutBoundingRect();
            const FSlateRect DetailsRect = Details->GetCachedGeometry().GetLayoutBoundingRect();
            TestTrue(Context + TEXT(" 고정 영역이 화면 안에 들어온다"),
                Contains(Screen, CardRect) && Contains(Screen, FooterRect) && Contains(Screen, HeaderRect));
            TestTrue(Context + TEXT(" 본문 아래·Footer 위에 충분한 높이로 배치된다"),
                ScrollRect.GetSize().Y > 0.f && CardRect.GetSize().Y > 0.f &&
                ScrollRect.Top >= HeaderRect.Bottom - Tolerance && ScrollRect.Bottom <= CardRect.Top + Tolerance &&
                CardRect.Bottom <= FooterSectionRect.Top + Tolerance && FooterSectionRect.Bottom <= FooterRect.Top + Tolerance);
            TestFalse(Context + TEXT(" 스크롤과 겹치지 않는다"), Intersects(CardRect, ScrollRect));
            TestFalse(Context + TEXT(" Footer와 겹치지 않는다"), Intersects(CardRect, FooterRect));
            TestTrue(Context + TEXT(" 핵심 조언과 보조 정보가 위아래로 분리된다"), AdviceRect.Bottom <= DetailsRect.Top + Tolerance);
            for (UTextBlock* Text : { Title, Advice, Details })
            {
                const FVector2D Allocated = Text->GetCachedGeometry().GetLocalSize();
                const FVector2D Desired = Text->GetDesiredSize();
                TestTrue(Context + TEXT(" / ") + Text->GetName() + TEXT(" 문구가 카드 안에서 잘리지 않는다"),
                    !Text->GetText().IsEmpty() && Contains(CardRect, Text->GetCachedGeometry().GetLayoutBoundingRect()) &&
                    Allocated.X + Tolerance >= Desired.X && Allocated.Y + Tolerance >= Desired.Y);
            }
            if (!SaveTarget(*this, *Target, Resolution, Context)) return false;
            TestTrue(Context + TEXT(" 실제 보고서 본문을 스크롤할 수 있다"), Scroll->GetScrollOffsetOfEnd() > 0.f);
            Scroll->ScrollToEnd(); Draw();
            TestTrue(Context + TEXT(" 스크롤 끝으로 실제 이동했다"), Scroll->GetScrollOffset() > 0.f &&
                FMath::IsNearlyEqual(Scroll->GetScrollOffset(), Scroll->GetScrollOffsetOfEnd(), 2.f));
            TestTrue(Context + TEXT(" 스크롤 뒤에도 비서 카드와 Footer가 고정된다"),
                SameRect(CardRect, Card->GetCachedGeometry().GetLayoutBoundingRect()) &&
                SameRect(FooterRect, Footer->GetCachedGeometry().GetLayoutBoundingRect()));
            if (!SaveTarget(*this, *Target, Resolution, Context + TEXT("_Bottom"))) return false;
        }
    }
    return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
