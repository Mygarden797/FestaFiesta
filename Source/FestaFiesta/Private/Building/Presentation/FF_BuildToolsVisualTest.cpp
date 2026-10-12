// FF_BuildToolsVisualTest.cpp
// 역할: 건설 도구 모음의 상태별 화면을 실제로 렌더링해 확인한다.

// Render each real authored HUD state without a player or building authority.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Building/Presentation/FF_BuildPlacementFailureFeedback.h"
#include "FF_FFZSFacilityDefinition.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Input/HittestGrid.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SVirtualWindow.h"

namespace FFBuildToolsVisual
{
struct FPlayerlessInitialization
{
    explicit FPlayerlessInitialization(UWidgetBlueprintGeneratedClass& InClass)
        : Class(InClass), bPrevious(InClass.bCanCallInitializedWithoutPlayerContext)
    { Class.bCanCallInitializedWithoutPlayerContext = true; }
    ~FPlayerlessInitialization() { Class.bCanCallInitializedWithoutPlayerContext = bPrevious; }
    UWidgetBlueprintGeneratedClass& Class;
    bool bPrevious;
};
bool IsRendered(const UWidget* Widget)
{
    for (const UWidget* Current = Widget; Current; Current = Current->GetParent())
        if (Current->GetVisibility() == ESlateVisibility::Collapsed || Current->GetVisibility() == ESlateVisibility::Hidden) return false;
    return Widget != nullptr;
}
bool IsWithin(const UWidget* Widget, const UWidget* Surface)
{
    for (const UWidget* Current = Widget; Current; Current = Current->GetParent())
        if (Current == Surface) return true;
    return false;
}
bool Contains(const FSlateRect& Outer, const FSlateRect& Inner)
{
    return Inner.Left >= Outer.Left - 1 && Inner.Right <= Outer.Right + 1 &&
        Inner.Top >= Outer.Top - 1 && Inner.Bottom <= Outer.Bottom + 1;
}
bool Overlaps(const FSlateRect& A, const FSlateRect& B)
{
    return A.Left < B.Right - .5f && A.Right > B.Left + .5f && A.Top < B.Bottom - .5f && A.Bottom > B.Top + .5f;
}
FSlateRect Bounds(const UWidget* Widget) { return Widget->GetPaintSpaceGeometry().GetRenderBoundingRect(); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildToolsRenderedPreviewTest,
    "FestaFiesta.Building.BuildTools.RenderedPreview",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFFBuildToolsRenderedPreviewTest::RunTest(const FString& Parameters)
{
    using namespace FFBuildToolsVisual;
    if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
    {
        AddInfo(TEXT("SKIPPED: HUD screenshots require a real RHI; use -RenderOffscreen."));
        return true;
    }
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_BuildToolsWidget>(nullptr,
        FFBuildUIAssetPaths::BuildToolsClass));
    if (!TestNotNull(TEXT("Playerless editor world"), World) || !TestNotNull(TEXT("Saved build HUD"), Class)) return false;
    FPlayerlessInitialization Context(*Class);
    TStrongObjectPtr<UFF_BuildToolsWidget> Widget(CreateWidget<UFF_BuildToolsWidget>(World, Class));
    if (!TestNotNull(TEXT("Production tools widget"), Widget.Get())) return false;
    const TSharedRef<SWidget> Slate = Widget->TakeWidget();
    Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Widget->SetIsEnabled(true);
    auto* Data = FindFProperty<FStructProperty>(Class, TEXT("ViewData"));
    if (!TestNotNull(TEXT("Presentation-only fixture seam"), Data)) return false;
    Data->ContainerPtrToValuePtr<FFF_BuildToolsViewData>(Widget.Get())->bPreviewActive = true;
    auto Text = [&](FName Name, const TCHAR* Value)
    {
        if (UTextBlock* Label = Cast<UTextBlock>(Widget->GetWidgetFromName(Name))) Label->SetText(FText::FromString(Value));
    };
    Text(TEXT("Text_Mode"), TEXT("시설 배치"));
    Text(TEXT("Text_FacilityName"), TEXT("구름 솜사탕 부스"));
    Text(TEXT("Text_Cost"), TEXT("설치 400 Gold"));
    Text(TEXT("Text_Balance"), TEXT("보유 7,200 Gold  ·  확정 후 6,800 Gold"));
    Text(TEXT("Text_BuildHelpPrimary"), TEXT("클릭해 시설 배치"));
    Text(TEXT("Text_BuildHelpRotate"), TEXT("시설 방향 회전"));
    Text(TEXT("Text_BuildHelpMove"), TEXT("카메라 이동"));
    Text(TEXT("Text_BuildHelpPrecision"), TEXT("정밀 위치 맞춤"));
    for (FName Name : {TEXT("Row_BuildHelpRotate"), TEXT("Row_BuildHelpPrecision"), TEXT("Row_BuildHelpNearby")})
        if (UWidget* Row = Widget->GetWidgetFromName(Name))
            Row->SetVisibility(ESlateVisibility::HitTestInvisible);
    Text(TEXT("Text_Legend"), TEXT("배치 가능 · 눌러서 설치"));
    Text(TEXT("Text_ActiveSnap"), TEXT("100 cm 맞춤"));
    Text(TEXT("Text_Confirm"), TEXT("배치 확정"));
    Text(TEXT("Text_Footprint"), TEXT("시설 크기  3.2 × 1.97 m"));
    Text(TEXT("Text_Queue"), TEXT("대기열 범위  3.2 × 1.2 m\nU자형 · 대기·이용 5팀\n7 기본 · 8 좌측 · 9 우측 · 0 U자"));
    if (auto* Queue = Widget->GetWidgetFromName(TEXT("Text_Queue")))
        Queue->SetVisibility(ESlateVisibility::HitTestInvisible);
    Text(TEXT("Text_ControlHelp"), TEXT(""));
    auto* Dock = Cast<UBorder>(Widget->GetWidgetFromName(TEXT("Border_Tools")));
    auto* Info = Cast<UBorder>(Widget->GetWidgetFromName(TEXT("Border_PlacementInfo")));
    auto* Help = Cast<UBorder>(Widget->GetWidgetFromName(TEXT("Border_PlacementHelp")));
    auto* HelpSurface = Widget->GetWidgetFromName(TEXT("Size_BuildWorldHelp"));
    auto* TopBar = Cast<UBorder>(Widget->GetWidgetFromName(TEXT("Border_BuildTopBar")));
    auto* Back = Cast<UBorder>(Widget->GetWidgetFromName(TEXT("Border_BuildBack")));
    auto* Gold = Cast<UBorder>(Widget->GetWidgetFromName(TEXT("Border_BuildToolbarGold")));
    auto* Settings = Cast<UBorder>(Widget->GetWidgetFromName(TEXT("Panel_BuildSettings")));
    if (!TestNotNull(TEXT("Placement action dock"), Dock) ||
        !TestNotNull(TEXT("Placement facility summary"), Info) ||
        !TestNotNull(TEXT("Placement instructions"), Help) ||
        !TestNotNull(TEXT("Complete instruction surface"), HelpSurface) ||
        !TestNotNull(TEXT("Build-session top toolbar"), TopBar) ||
        !TestNotNull(TEXT("Build-session back navigation"), Back) ||
        !TestNotNull(TEXT("Build settings drawer"), Settings)) return false;
    TestNull(TEXT("Duplicate top-right currency was removed from the saved Designer"), Gold);
    // NativeConstruct applies the editor world's Closed state. This render fixture
    // deliberately authors the visible surfaces without opening a build session.
    TopBar->SetVisibility(ESlateVisibility::Visible);
    Back->SetVisibility(ESlateVisibility::Visible);
    Dock->SetVisibility(ESlateVisibility::Visible);
    Info->SetVisibility(ESlateVisibility::Visible);
    Settings->SetVisibility(ESlateVisibility::Collapsed);
    TArray<UWidget*> Controls;
    TSet<UMaterialInterface*> Materials;
    auto Material = [&](const FSlateBrush& Brush)
    { if (auto* Value = Cast<UMaterialInterface>(Brush.GetResourceObject())) Materials.Add(Value); };
    Widget->WidgetTree->ForEachWidget([&](UWidget* Child)
    {
        if (UButton* Button = Cast<UButton>(Child))
        {
            Button->SetIsEnabled(true); Controls.Add(Button);
            Material(Button->GetStyle().Normal); Material(Button->GetStyle().Hovered);
            Material(Button->GetStyle().Pressed); Material(Button->GetStyle().Disabled);
        }
        if (const UBorder* Border = Cast<UBorder>(Child)) Material(Border->Background);
        if (const UImage* Image = Cast<UImage>(Child)) Material(Image->GetBrush());
    });
    auto* Confirm = Cast<UFF_BuildToolsActionButton>(Widget->GetWidgetFromName(TEXT("Button_Confirm")));
    if (Confirm)
    {
        Confirm->ApplySelection(true, Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Text_Confirm"))));
        Material(Confirm->SelectionStyle.Normal);
    }
    for (UMaterialInterface* Value : Materials) Value->EnsureIsComplete();
    if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
    FlushRenderingCommands();
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/BuildableGrid");
    IFileManager::Get().MakeDirectory(*Directory, true);
    auto Click = [&](FName Name)
    {
        UButton* Button = Cast<UButton>(Widget->GetWidgetFromName(Name));
        if (TestNotNull(*Name.ToString(), Button)) Button->OnClicked.Broadcast();
    };
    bool bAllSaved = true;
    for (const FIntPoint Resolution : {FIntPoint(1280, 720), FIntPoint(1920, 1080)})
    {
        const FVector2D Size(Resolution.X, Resolution.Y);
        const float Dpi = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Resolution);
        auto Window = SNew(SVirtualWindow).Size(Size);
        Window->SetContent(SNew(SDPIScaler).DPIScale(Dpi)[Slate]);
        FWidgetRenderer Renderer(false, true);
        TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, true));
        if (!TestNotNull(TEXT("HUD render target"), Target.Get())) return false;
        FHittestGrid HitGrid;
        TopBar->SetVisibility(ESlateVisibility::Visible);
        Settings->SetVisibility(ESlateVisibility::Collapsed);
        // Reset through the production action so the previous resolution's
        // expanded intent and the authored border cannot drift apart.
        if (Help->IsVisible()) Widget->ToggleHelp();
        for (FName Name : {TEXT("Panel_PlacementFacts")})
            if (UWidget* Panel = Widget->GetWidgetFromName(Name)) Panel->SetVisibility(ESlateVisibility::Collapsed);
        for (FName Name : {TEXT("Text_PlacementAdvice"), TEXT("Text_PlacementObstruction"), TEXT("Text_AssistanceMessage"),
            TEXT("Button_FindNearby"), TEXT("Button_NextPlacementProblem")})
            if (UWidget* Panel = Widget->GetWidgetFromName(Name)) Panel->SetVisibility(ESlateVisibility::Collapsed);
        auto Capture = [&](const TCHAR* State, bool bShowQueueDraft = false)
        {
            // Freeze authority-driven refresh only for this authored presentation capture.
            // Production state/queue behavior is covered by the BuildMode integration tests.
            const bool bWasTicking = Slate->GetCanTick();
            if (bShowQueueDraft)
            {
                Slate->SetCanTick(false);
                if (UWidget* Queue = Widget->GetWidgetFromName(TEXT("Row_BuildHelpQueue")))
                    Queue->SetVisibility(ESlateVisibility::HitTestInvisible);
                Text(TEXT("Text_BuildHelpQueueHint"), TEXT("7 기본 · 8 좌측 · 9 우측 · 0 U자\n현재: U자형 · 배치 확정 시 적용"));
            }
            for (int32 Frame = 0; Frame < 4; ++Frame)
            {
                Widget->ForceLayoutPrepass();
                if (bShowQueueDraft)
                {
                    auto* HelpSize = Cast<USizeBox>(Widget->GetWidgetFromName(TEXT("Size_BuildWorldHelp")));
                    UWidget* Column = Widget->GetWidgetFromName(TEXT("Column_BuildIconHelp"));
                    if (HelpSize && Column)
                    {
                        const FVector2D Placement = FFBuildToolsModel::ResolveHelpVerticalBounds(
                            Column->GetDesiredSize().Y + 24.f, Size.Y / Dpi - 100.f);
                        HelpSize->SetHeightOverride(Placement.Y);
                        if (auto* Slot = Cast<UCanvasPanelSlot>(HelpSize->Slot))
                            Slot->SetPosition(FVector2D(32.f, Placement.X));
                    }
                }
                Renderer.DrawWindow(Target.Get(), HitGrid, Window, 1.f, Size, .016f, false);
            }
            const FString ContextName = FString::Printf(TEXT("%s %dx%d"), State, Resolution.X, Resolution.Y);
            const FSlateRect Viewport(0, 0, Resolution.X, Resolution.Y);
            const FSlateRect TopRect = Bounds(TopBar);
            const FSlateRect BackRect = Bounds(Back);
            const FSlateRect DockRect = Bounds(Dock), InfoRect = Bounds(Info), SettingsRect = Bounds(Settings);
            const auto* SettingsScroll = Cast<UScrollBox>(Widget->GetWidgetFromName(TEXT("Scroll_BuildSettings")));
            const FSlateRect SettingsViewRect = SettingsScroll ? Bounds(SettingsScroll) : SettingsRect;
            const auto* HelpScroll = Cast<UScrollBox>(Widget->GetWidgetFromName(TEXT("Scroll_BuildHelp")));
            const FSlateRect HelpViewRect = HelpScroll ? Bounds(HelpScroll) : Bounds(Help);
            const bool bDock = IsRendered(Dock), bInfo = IsRendered(Info), bSettings = IsRendered(Settings);
            if (bInfo)
            {
                UWidget* Space = Widget->GetWidgetFromName(TEXT("Border_PlacementSpace"));
                TestTrue(ContextName + TEXT(": required space is visible without opening facility facts"),
                    Space && IsRendered(Space) && Contains(InfoRect, Bounds(Space)));
                for (const FName Name : {TEXT("Text_Footprint"), TEXT("Text_Queue"), TEXT("Text_RangeLegend")})
                {
                    UWidget* Label = Widget->GetWidgetFromName(Name);
                    TestTrue(ContextName + TEXT(": space information remains visible: ") + Name.ToString(),
                        Label && IsRendered(Label) && IsWithin(Label, Space));
                }
            }
            TestTrue(ContextName + TEXT(": top toolbar fits the viewport"),
                IsRendered(TopBar) && Contains(Viewport, TopRect));
            TestTrue(ContextName + TEXT(": pictogram toolbar has comfortable targets"),
                TopRect.Right - TopRect.Left >= 550.f * Dpi);
            TestFalse(ContextName + TEXT(": keyboard keycaps are no longer displayed"),
                IsRendered(Widget->GetWidgetFromName(TEXT("Row_BuildToolbarKeycaps"))));
            TestTrue(ContextName + TEXT(": back button fits the upper left"),
                IsRendered(Back) && Contains(Viewport, BackRect) &&
                BackRect.Right < Resolution.X * .25f && BackRect.Bottom < Resolution.Y * .15f);
            TestFalse(ContextName + TEXT(": back button and toolbar do not overlap"),
                Overlaps(BackRect, TopRect));
            TestFalse(ContextName + TEXT(": duplicate top currency is removed"), IsRendered(Gold));
            TestTrue(ContextName + TEXT(": top toolbar stays above the working center"),
                TopRect.Bottom <= Resolution.Y * .18f);
            if (bDock)
            {
                TestTrue(ContextName + TEXT(": placement actions fit the viewport"), Contains(Viewport, DockRect));
                TestTrue(ContextName + TEXT(": placement actions leave bottom navigation clear"),
                    DockRect.Bottom <= Resolution.Y - 80.f * Dpi);
                TestTrue(ContextName + TEXT(": placement actions leave the world center clear"),
                    DockRect.Left > Resolution.X * .65f);
                TestFalse(ContextName + TEXT(": top toolbar and placement actions do not overlap"),
                    Overlaps(TopRect, DockRect));
            }
            if (bInfo)
            {
                TestTrue(ContextName + TEXT(": facility summary fits the viewport"), Contains(Viewport, InfoRect));
                TArray<FSlateRect> HeaderItems;
                for (const FName Name : {FName(TEXT("Text_Mode")), FName(TEXT("Button_Facts")), FName(TEXT("Button_Help"))})
                {
                    const UWidget* Item = Widget->GetWidgetFromName(Name);
                    if (!IsRendered(Item)) continue;
                    const FSlateRect Rect = Bounds(Item);
                    TestTrue(ContextName + TEXT(": summary header item fits the card: ") + Name.ToString(), Contains(InfoRect, Rect));
                    for (const FSlateRect& Previous : HeaderItems)
                        TestFalse(ContextName + TEXT(": summary title and actions do not overlap"), Overlaps(Previous, Rect));
                    HeaderItems.Add(Rect);
                    if (const auto* HeaderLabel = Cast<UTextBlock>(Item))
                        TestTrue(ContextName + TEXT(": complete placement mode caption is readable"),
                            HeaderLabel->GetDesiredSize().X <= HeaderLabel->GetCachedGeometry().GetLocalSize().X + 1.f);
                }
                TestTrue(ContextName + TEXT(": facility summary docks to the right edge"),
                    InfoRect.Left > Resolution.X * .62f && InfoRect.Right <= Resolution.X - 8.f * Dpi);
                TestFalse(ContextName + TEXT(": top toolbar and summary do not overlap"),
                    Overlaps(TopRect, InfoRect));
                if (bDock) TestFalse(ContextName + TEXT(": summary and placement actions do not overlap"),
                    Overlaps(DockRect, InfoRect));
            }
            if (bSettings)
            {
                TestTrue(ContextName + TEXT(": settings drawer fits the viewport"), Contains(Viewport, SettingsRect));
                TestTrue(ContextName + TEXT(": settings drawer leaves the world center clear"),
                    SettingsRect.Right < Resolution.X * .35f);
                TestFalse(ContextName + TEXT(": settings drawer and top toolbar do not overlap"),
                    Overlaps(SettingsRect, TopRect));
                TestFalse(ContextName + TEXT(": settings drawer replaces the left summary"), bInfo);
                TestFalse(ContextName + TEXT(": settings hides instructions through their complete parent surface"), IsRendered(Help));
                TestEqual(ContextName + TEXT(": instruction surface is collapsed behind settings"),
                    HelpSurface->GetVisibility(), ESlateVisibility::Collapsed);
                TestTrue(ContextName + TEXT(": hiding instructions preserves their expanded state"), Help->IsVisible());
                for (const FName Name : {FName(TEXT("Button_BuildHelpUndo")), FName(TEXT("Button_BuildHelpRedo"))})
                    if (const UWidget* Action = Widget->GetWidgetFromName(Name))
                    {
                        const FGeometry& Geometry = Action->GetCachedGeometry();
                        const auto Path = HitGrid.GetBubblePath(Geometry.LocalToAbsolute(Geometry.GetLocalSize() * .5f), 0.f, true);
                        TestFalse(ContextName + TEXT(": hidden instruction action has no pointer path: ") + Name.ToString(),
                            Path.ContainsByPredicate([&](const FWidgetAndPointer& Entry)
                            { return &Entry.Widget.Get() == Action->GetCachedWidget().Get(); }));
                    }
            }
            if (IsRendered(Help))
            {
                const FSlateRect HelpRect = Bounds(Help);
                TestTrue(ContextName + TEXT(": help fits the viewport"), Contains(Viewport, HelpRect));
                TestTrue(ContextName + TEXT(": keyboard guide stays in the left scene margin"),
                    HelpRect.Right < Resolution.X * .35f);
                if (bInfo) TestFalse(ContextName + TEXT(": world shortcuts avoid the facility summary"),
                    Overlaps(HelpRect, InfoRect));
                TestTrue(ContextName + TEXT(": help leaves the bottom navigation clear"), HelpRect.Bottom <= Resolution.Y - 80.f * Dpi);
                if (bDock) TestFalse(ContextName + TEXT(": left help and right tools are separate"),
                    Overlaps(HelpRect, DockRect));
                for (FName Name : {TEXT("Size_BuildHelpMouseIcon"), TEXT("Size_BuildHelpArrowsIcon"),
                    TEXT("Size_BuildHelpGridIcon"), TEXT("Size_BuildHelpCatalogIcon"),
                    TEXT("Size_BuildHelpPauseIcon")})
                    TestTrue(ContextName + TEXT(": help shows the input device: ") + Name.ToString(),
                        IsRendered(Widget->GetWidgetFromName(Name)));
                for (const TCHAR* Key : {TEXT("Q"), TEXT("E"), TEXT("W"), TEXT("A"), TEXT("S"),
                    TEXT("D"), TEXT("Ctrl"), TEXT("G"), TEXT("B"), TEXT("Esc"), TEXT("Z"), TEXT("Y"), TEXT("N")})
                {
                    const FString Name = FString(TEXT("Text_BuildHelpKey_")) + Key;
                    TestTrue(ContextName + TEXT(": physical keyboard key is visible: ") + Key,
                        IsRendered(Widget->GetWidgetFromName(FName(*Name))));
                }
                for (FName Name : {TEXT("Button_BuildHelpUndo"), TEXT("Button_BuildHelpRedo")})
                {
                    const UWidget* Action = Widget->GetWidgetFromName(Name);
                    if (!TestNotNull(ContextName + TEXT(": existing guide owns the history action"), Action)) continue;
                    const FGeometry& ActionGeometry = Action->GetCachedGeometry();
                    const FVector2D Center = ActionGeometry.LocalToAbsolute(ActionGeometry.GetLocalSize() * .5f);
                    // This presentation fixture has no history. Ignore disabled status while
                    // verifying the actual painted input path through every guide ancestor.
                    const TArray<FWidgetAndPointer> Path = HitGrid.GetBubblePath(Center, 0.f, true);
                    TestTrue(ContextName + TEXT(": guide history action has a pointer path: ") + Name.ToString(),
                        Path.ContainsByPredicate([&](const FWidgetAndPointer& Entry)
                        { return &Entry.Widget.Get() == Action->GetCachedWidget().Get(); }));
                }
                for (FName Name : {TEXT("Text_BuildHelpUndo"), TEXT("Text_BuildHelpRedo")})
                    if (const UWidget* Caption = Widget->GetWidgetFromName(Name))
                        TestTrue(ContextName + TEXT(": history caption fits on one line: ") + Name.ToString(),
                            Caption->GetCachedGeometry().GetLocalSize().Y <= 26.f &&
                            Caption->GetCachedGeometry().GetLocalSize().X + 1.f >= Caption->GetDesiredSize().X);
                for (FName Name : {TEXT("Size_BuildHelpKey_UndoCtrl"), TEXT("Size_BuildHelpKey_RedoCtrl")})
                    if (const UWidget* Key = Widget->GetWidgetFromName(Name))
                        TestTrue(ContextName + TEXT(": history keys align with the existing guide"),
                            FMath::Abs(Bounds(Key).Left - Bounds(Widget->GetWidgetFromName(TEXT("Size_BuildHelpKey_W"))).Left) < 1.f);
                if (bShowQueueDraft)
                {
                    UWidget* Queue = Widget->GetWidgetFromName(TEXT("Row_BuildHelpQueue"));
                    TestTrue(ContextName + TEXT(": complete queue key guide fits the visible help"),
                        IsRendered(Queue) && Contains(HelpViewRect, Bounds(Queue)));
                    const TCHAR* Captions[] = {TEXT("7"), TEXT("8"), TEXT("9"), TEXT("0")};
                    for (int32 Index = 0; Index < 4; ++Index)
                    {
                        const FName KeyName(*FString::Printf(TEXT("Text_BuildHelpKey_%d"), Index + 1));
                        const UTextBlock* KeyText = Cast<UTextBlock>(Widget->GetWidgetFromName(KeyName));
                        TestTrue(ContextName + TEXT(": queue guide uses the updated shortcut: ") + Captions[Index],
                            KeyText && KeyText->GetText().ToString() == Captions[Index]);
                    }
                    for (FName Name : {TEXT("Text_BuildHelpKey_1"), TEXT("Text_BuildHelpKey_2"),
                        TEXT("Text_BuildHelpKey_3"), TEXT("Text_BuildHelpKey_4"), TEXT("Text_BuildHelpQueueHint")})
                        TestTrue(ContextName + TEXT(": queue mapping and current draft are visible: ") + Name.ToString(),
                            IsRendered(Widget->GetWidgetFromName(Name)) && Contains(HelpViewRect, Bounds(Widget->GetWidgetFromName(Name))));
                }
                TestFalse(ContextName + TEXT(": old letter-key legend stays hidden"),
                    IsRendered(Widget->GetWidgetFromName(TEXT("Text_ControlHelp"))));
            }
            TArray<UWidget*> VisibleControls;
            for (UWidget* Control : Controls)
            {
                if (!IsRendered(Control)) continue;
                const FSlateRect Rect = Bounds(Control);
                if (IsWithin(Control, Settings) && !Overlaps(Rect, SettingsViewRect)) continue;
                if (IsWithin(Control, Help) && !Overlaps(Rect, HelpViewRect)) continue;
                VisibleControls.Add(Control);
                const FString Label = ContextName + TEXT(": ") + Control->GetName();
                TestTrue(Label + TEXT(" has a usable target"), Control->GetCachedGeometry().GetLocalSize().X >= 30 && Control->GetCachedGeometry().GetLocalSize().Y >= 20);
                const UWidget* Surface = IsWithin(Control, Settings) ? static_cast<UWidget*>(Settings)
                    : IsWithin(Control, TopBar) ? static_cast<UWidget*>(TopBar)
                    : IsWithin(Control, Back) ? static_cast<UWidget*>(Back)
                    : IsWithin(Control, Info) ? static_cast<UWidget*>(Info)
                    : IsWithin(Control, Dock) ? static_cast<UWidget*>(Dock)
                    : IsWithin(Control, Help) ? static_cast<UWidget*>(Help) : nullptr;
                if (TestNotNull(Label + TEXT(" has a named interactive surface"), Surface))
                    TestTrue(Label + TEXT(" remains inside its own surface"),
                        IsWithin(Control, Settings) ? Overlaps(SettingsViewRect, Rect) :
                        IsWithin(Control, Help) ? Overlaps(HelpViewRect, Rect) : Contains(Bounds(Surface), Rect));
                if (const UButton* Button = Cast<UButton>(Control))
                    if (const UWidget* Content = Button->GetContent()) TestTrue(Label + TEXT(" contains its label"), Contains(Rect, Bounds(Content)));
            }
            for (int32 A = 0; A < VisibleControls.Num(); ++A)
                for (int32 B = A + 1; B < VisibleControls.Num(); ++B)
                {
                    // Slate clips scrolled children during paint, but their cached UMG
                    // geometries can still describe the pre-scroll positions here.
                    // The unscrolled capture checks their row spacing; the scrolled
                    // capture is saved for inspection of the actual clipped pixels.
                    if (FCString::Strcmp(State, TEXT("SettingsDrawerScrolled")) == 0 &&
                        IsWithin(VisibleControls[A], Settings) && IsWithin(VisibleControls[B], Settings))
                        continue;
                    auto PaintedBounds = [&](UWidget* Control)
                    {
                        FSlateRect Rect = Bounds(Control);
                        if (IsWithin(Control, Settings) || IsWithin(Control, Help))
                        {
                            const FSlateRect Clip = IsWithin(Control, Settings) ? SettingsViewRect : HelpViewRect;
                            Rect.Left = FMath::Max(Rect.Left, Clip.Left);
                            Rect.Top = FMath::Max(Rect.Top, Clip.Top);
                            Rect.Right = FMath::Min(Rect.Right, Clip.Right);
                            Rect.Bottom = FMath::Min(Rect.Bottom, Clip.Bottom);
                        }
                        return Rect;
                    };
                    TestFalse(ContextName + TEXT(": controls do not overlap: ") + VisibleControls[A]->GetName() + TEXT(" / ") + VisibleControls[B]->GetName(),
                        Overlaps(PaintedBounds(VisibleControls[A]), PaintedBounds(VisibleControls[B])));
                }
            for (FName Name : {TEXT("Text_ControlHelp"), TEXT("Text_Footprint"), TEXT("Text_Queue"), TEXT("Text_Audience"), TEXT("Text_RangeLegend"),
                TEXT("Text_RotationHint"),
                TEXT("Text_PlacementAdvice"), TEXT("Text_PlacementObstruction"), TEXT("Text_AssistanceMessage")})
            {
                UWidget* Label = Widget->GetWidgetFromName(Name);
                if (IsWithin(Label, Settings) && !Overlaps(Bounds(Label), SettingsViewRect)) continue;
                // The current inspector has a scrollable body. Offscreen warning
                // text must be reachable by scrolling, not squeezed into its hero.
                UScrollBox* BodyScroll = nullptr;
                for (UWidget* Parent = Label ? Label->GetParent() : nullptr; Parent && Parent != Info; Parent = Parent->GetParent())
                    if (auto* Scroll = Cast<UScrollBox>(Parent)) { BodyScroll = Scroll; break; }
                if (IsRendered(Label) && BodyScroll && IsWithin(Label, Info) && !Contains(Bounds(BodyScroll), Bounds(Label)))
                {
                    const float PreviousOffset = BodyScroll->GetScrollOffset();
                    BodyScroll->ScrollWidgetIntoView(Label, false, EDescendantScrollDestination::IntoView);
                    for (int32 Frame = 0; Frame < 4; ++Frame)
                        Renderer.DrawWindow(Target.Get(), HitGrid, Window, 1.f, Size, .016f, false);
                    TestTrue(ContextName + TEXT(": expanded information is fully reachable in its scroll viewport: ") + Name.ToString(),
                        Contains(Bounds(BodyScroll), Bounds(Label)));
                    BodyScroll->SetScrollOffset(PreviousOffset);
                    for (int32 Frame = 0; Frame < 4; ++Frame)
                        Renderer.DrawWindow(Target.Get(), HitGrid, Window, 1.f, Size, .016f, false);
                    continue;
                }
                if (IsRendered(Label)) TestTrue(ContextName + TEXT(": expanded information stays inside its surface: ") + Name.ToString(),
                    IsWithin(Label, Settings) ? Overlaps(SettingsViewRect, Bounds(Label)) :
                    Contains(Name == TEXT("Text_ControlHelp") ? Bounds(Help) : InfoRect, Bounds(Label)));
            }
            if (FCString::Strcmp(State, TEXT("AudienceBoundaryProblem")) == 0)
            {
                TArray<FSlateRect> RequiredRows;
                for (const FName Name : {TEXT("Text_Footprint"), TEXT("Text_Queue"), TEXT("Text_Audience"),
                    TEXT("Row_PlacementSpaceLegend"), TEXT("Row_PlacementBoundaryLegend"), TEXT("Text_RangeLegend")})
                {
                    UWidget* Row = Widget->GetWidgetFromName(Name);
                    if (!TestNotNull(ContextName + TEXT(": audience boundary row exists: ") + Name.ToString(), Row)) continue;
                    TestTrue(ContextName + TEXT(": audience requirements fit the visible summary: ") + Name.ToString(),
                        IsRendered(Row) && Contains(InfoRect, Bounds(Row)));
                    for (const FSlateRect& Previous : RequiredRows)
                        TestFalse(ContextName + TEXT(": audience requirements and legends do not overlap"), Overlaps(Previous, Bounds(Row)));
                    RequiredRows.Add(Bounds(Row));
                }
            }
            for (FName Name : {TEXT("Button_Radial"), TEXT("Panel_Radial"), TEXT("Panel_Details")})
                TestNull(ContextName + TEXT(": removed tool menu has no render or hit-test surface: ") + Name.ToString(),
                    Widget->GetWidgetFromName(Name));
            TestFalse(ContextName + TEXT(": legacy radial menu remains closed"), Widget->IsRadialMenuOpen());
            TestTrue(ContextName + TEXT(": only the settings drawer reserves auxiliary input"),
                Widget->IsAuxiliaryPanelOpen() == bSettings);
            TArray<FColor> Pixels;
            FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
            if (!TestTrue(ContextName + TEXT(": complete rendered image"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags) && Pixels.Num() == Resolution.X * Resolution.Y))
            { bAllSaved = false; return; }
            if (Confirm && IsRendered(Confirm))
            {
                const FVector2D Point = Confirm->GetCachedGeometry().LocalToAbsolute(FVector2D(10, Confirm->GetCachedGeometry().GetLocalSize().Y * .5));
                const int32 X = FMath::FloorToInt(Point.X), Y = FMath::FloorToInt(Point.Y);
                if (X >= 0 && Y >= 0 && X < Resolution.X && Y < Resolution.Y)
                    TestTrue(ContextName + TEXT(": primary action remains opaque enough to read"), Pixels[Y * Resolution.X + X].A >= 210);
            }
            TArray64<uint8> Png;
            FImageUtils::PNGCompressImageArray(Resolution.X, Resolution.Y, Pixels, Png);
            const FString Path = Directory / FString::Printf(TEXT("BuildTools_%s_%d.png"), State, Resolution.Y);
            const bool bSaved = FFileHelper::SaveArrayToFile(Png, *Path);
            bAllSaved &= bSaved;
            TestTrue(ContextName + TEXT(": screenshot saved"), bSaved);
            AddInfo(FString::Printf(TEXT("Saved %s"), *FPaths::ConvertRelativePathToFull(Path)));
            if (bShowQueueDraft) Slate->SetCanTick(bWasTicking);
        };
        Dock->SetVisibility(ESlateVisibility::Collapsed);
        Info->SetVisibility(ESlateVisibility::Collapsed);
        Capture(TEXT("CatalogTopBar"));
        Dock->SetVisibility(ESlateVisibility::Visible);
        Info->SetVisibility(ESlateVisibility::Visible);
        Click(TEXT("Button_Help")); Capture(TEXT("DefaultHelp"));
        Capture(TEXT("QueueDraftHelp"), true);
        Click(TEXT("Button_Help")); Capture(TEXT("Compact"));
        Click(TEXT("Button_Help"));
        Info->SetVisibility(ESlateVisibility::Collapsed);
        Settings->SetVisibility(ESlateVisibility::Visible);
        Capture(TEXT("SettingsDrawer"));
        if (auto* Scroll = Cast<UScrollBox>(Widget->GetWidgetFromName(TEXT("Scroll_BuildSettings"))))
        {
            Scroll->SetScrollOffset(200.0f);
            Capture(TEXT("SettingsDrawerScrolled"));
            Scroll->ScrollToStart();
        }
        Settings->SetVisibility(ESlateVisibility::Collapsed);
        Info->SetVisibility(ESlateVisibility::Visible);
        Widget->ToggleRadialMenu(); Capture(TEXT("RemovedMenuEntry"));
        TestTrue(TEXT("Closing settings restores the previously expanded instructions"), IsRendered(Help));
        Capture(TEXT("Help"));
        Click(TEXT("Button_Facts")); Capture(TEXT("FactsAndHelp"));
        Click(TEXT("Button_Help")); Capture(TEXT("Facts"));
        Click(TEXT("Button_Help")); Widget->ToggleRadialMenu(); Capture(TEXT("HelpAfterRemovedMenuEntry"));
        Text(TEXT("Text_Legend"), TEXT("대기 줄이 들어갈 공간이 없어요\n배치 문제 2개 · 1번째"));
        Text(TEXT("Text_PlacementAdvice"), TEXT("청록색 대기열의 붉은 사선을 확인하세요. 위치를 옮기거나 7·8·9·0으로 모양을 바꿔 보세요.\nN으로 가까운 빈자리로 이동할 수 있어요."));
        Text(TEXT("Text_PlacementObstruction"), TEXT("겹치는 대상: 구름 솜사탕 부스"));
        Text(TEXT("Text_AssistanceMessage"), TEXT("주변 6m의 후보 위치에서 자리를 찾지 못했어요. 회전하거나 T로 위치를 다시 선택해 주세요."));
        for (FName Name : {TEXT("Text_PlacementAdvice"), TEXT("Text_PlacementObstruction"), TEXT("Text_AssistanceMessage"),
            TEXT("Button_FindNearby"), TEXT("Button_NextPlacementProblem")})
            if (UWidget* Panel = Widget->GetWidgetFromName(Name)) Panel->SetVisibility(ESlateVisibility::Visible);
        Capture(TEXT("PlacementProblemAndHelp"));
        const auto* MainStage = LoadObject<UFFZSFacilityDefinition>(nullptr,
            TEXT("/Game/FestaFiesta/Building/FF_Data_Building/Performance/PDA_FFZS_Performance_MainStage_SunsetMainStage.PDA_FFZS_Performance_MainStage_SunsetMainStage"));
        if (TestNotNull(TEXT("Production main stage supplies real audience dimensions for visual QA"), MainStage))
        {
            FText FootprintText, QueueText;
            FFBuildToolsModel::FormatRequiredSpace(MainStage->Spec, FootprintText, QueueText);
            Text(TEXT("Text_FacilityName"), TEXT("메인공연장"));
            Text(TEXT("Text_Footprint"), *FootprintText.ToString());
            Text(TEXT("Text_Queue"), *QueueText.ToString());
            const FText AudienceText = FFBuildToolsModel::FormatAudienceSpace(MainStage->Spec);
            TestFalse(TEXT("Main stage audience requirements are available"), AudienceText.IsEmpty());
            Text(TEXT("Text_Audience"), *AudienceText.ToString());
            if (auto* Audience = Widget->GetWidgetFromName(TEXT("Text_Audience")))
                Audience->SetVisibility(ESlateVisibility::HitTestInvisible);
            FFFZSPlacementValidation Validation;
            FFFZSPlacementBoundaryIssue AudienceIssue;
            AudienceIssue.Region = EFFZSPlacementRegion::Audience;
            Validation.BoundaryIssues.Add(AudienceIssue);
            Text(TEXT("Text_Legend"), *FFBuildToolsModel::FormatProblemStatus(Validation.GetBoundaryFailureText(), 0, 4).ToString());
            Text(TEXT("Text_PlacementAdvice"), *UFF_BuildPlacementFailureFeedback::MakeBoundaryAdvice().ToString());
            for (FName Name : {TEXT("Panel_PlacementFacts"), TEXT("Text_PlacementObstruction"), TEXT("Text_AssistanceMessage")})
                if (auto* Panel = Widget->GetWidgetFromName(Name)) Panel->SetVisibility(ESlateVisibility::Collapsed);
            Capture(TEXT("AudienceBoundaryProblem"));
        }
        Text(TEXT("Text_FacilityName"), TEXT("구름 솜사탕 부스"));
        Text(TEXT("Text_Footprint"), TEXT("시설 크기  3.2 × 1.97 m"));
        Text(TEXT("Text_Queue"), TEXT("대기열 범위  3.2 × 1.2 m\nU자형 · 대기·이용 5팀\n7 기본 · 8 좌측 · 9 우측 · 0 U자"));
        if (auto* Audience = Widget->GetWidgetFromName(TEXT("Text_Audience")))
            Audience->SetVisibility(ESlateVisibility::Collapsed);
        Text(TEXT("Text_Legend"), TEXT("배치 가능 · 눌러서 설치"));
    }
    return bAllSaved;
}
#endif
