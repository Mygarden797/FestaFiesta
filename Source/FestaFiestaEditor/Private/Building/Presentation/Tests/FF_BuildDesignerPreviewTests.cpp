// 디자이너와 같은 생성 경로로 저장된 건축 화면을 렌더링한다. 게임/저장은 시작하지 않는다.
#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ListViewBase.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Input/HittestGrid.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "TextureCompiler.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Building/FacilityInfo/FF_FacilityInfoPanel.h"
#include "Building/FacilityCollection/UI/FF_FacilityCard.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditorUtils.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SVirtualWindow.h"

namespace FFBuildDesignerPreview
{
void ReadyMaterials(UUserWidget& Owner)
{
    if (!Owner.WidgetTree) return;
    Owner.WidgetTree->ForEachWidget([](UWidget* Widget)
    {
        const auto Ready = [](const FSlateBrush& Brush)
        {
            if (auto* Material = Cast<UMaterialInterface>(Brush.GetResourceObject())) Material->EnsureIsComplete();
        };
        if (auto* Border = Cast<UBorder>(Widget)) Ready(Border->Background);
        if (auto* Button = Cast<UButton>(Widget))
        { Ready(Button->GetStyle().Normal); Ready(Button->GetStyle().Hovered); Ready(Button->GetStyle().Pressed); }
        if (auto* Image = Cast<UImage>(Widget)) Ready(Image->GetBrush());
        if (auto* Child = Cast<UUserWidget>(Widget)) ReadyMaterials(*Child);
    });
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildDesignerPreviewTest,
    "FestaFiesta.Building.UI.DesignerPreviews",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFFBuildDesignerPreviewTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
    { AddError(TEXT("Designer preview validation requires -RenderOffscreen, not -nullrhi.")); return false; }
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Idle editor world"), World)) return false;
    const struct { const TCHAR* Package; const TCHAR* Name; } Screens[] = {
        {FFBuildUIAssetPaths::BuildPanelPackage, TEXT("Catalog")},
        {FFBuildUIAssetPaths::BuildToolsPackage, TEXT("BuildTools")},
        {FFBuildUIAssetPaths::FacilityInfoPanelPackage, TEXT("FacilityInfo")},
        {FFBuildUIAssetPaths::FacilityCodexScreenPackage, TEXT("Codex")},
        {FFBuildUIAssetPaths::BuildToolsPackage, TEXT("BuildSettings")},
        {FFBuildUIAssetPaths::FacilityInfoPanelPackage, TEXT("FacilityUpgrade")},
        {FFBuildUIAssetPaths::FacilityCodexScreenPackage, TEXT("CodexDetail")}
    };
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/BuildDesigner");
    IFileManager::Get().MakeDirectory(*Directory, true);
    for (const auto& Screen : Screens)
    {
        auto* BP = LoadObject<UWidgetBlueprint>(nullptr, Screen.Package);
        if (!TestNotNull(Screen.Name, BP)) return false;
        TStrongObjectPtr<UUserWidget> Preview(FWidgetBlueprintEditorUtils::CreateUserWidgetFromBlueprint(World, BP,
            {EWidgetDesignFlags::Designing | EWidgetDesignFlags::ExecutePreConstruct, nullptr}));
        if (!TestNotNull(TEXT("Designer-created preview"), Preview.Get())) return false;
        TestTrue(TEXT("Preview executes under the real Designer flag"), Preview->IsDesignTime());
        const FString PreviewName(Screen.Name);
        if (PreviewName == TEXT("BuildSettings"))
            CastChecked<UFF_BuildToolsWidget>(Preview.Get())->DesignerPage = EFF_BuildToolsDesignerPage::Settings;
        if (PreviewName == TEXT("FacilityUpgrade"))
            CastChecked<UFF_FacilityInfoPanel>(Preview.Get())->DesignerPage = EFF_FacilityInfoPage::Upgrade;
        if (PreviewName == TEXT("CodexDetail"))
            if (auto* DetailFlag = FindFProperty<FBoolProperty>(Preview->GetClass(), TEXT("bPreviewCollectionDetail")))
                DetailFlag->SetPropertyValue_InContainer(Preview.Get(), true);
        // Match choosing a screen resolution in Designer, including its project DPI.
        // This changes only the transient preview, never the authored asset's mode.
        Preview->DesignSizeMode = EDesignPreviewSizeMode::FillScreen;
        TSharedPtr<SWidget> Slate = Preview->TakeWidget();
        FFBuildDesignerPreview::ReadyMaterials(*Preview);
        if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
        FlushRenderingCommands();
        for (const FIntPoint Resolution : {FIntPoint(1280, 720), FIntPoint(1920, 1080)})
        {
            FWidgetRenderer Renderer(false, true);
            const FVector2D Size(Resolution.X, Resolution.Y);
            TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, true));
            if (!TestNotNull(TEXT("Designer render target"), Target.Get())) return false;
            auto Window = SNew(SVirtualWindow).Size(Size);
            const float Dpi = FWidgetBlueprintEditorUtils::GetWidgetPreviewDPIScale(Preview.Get(), Size);
            Window->SetContent(SNew(SDPIScaler).DPIScale(Dpi)[Slate.ToSharedRef()]);
            FHittestGrid Grid;
            for (int32 Frame = 0; Frame < 8; ++Frame)
            {
                Preview->ForceLayoutPrepass();
                Renderer.DrawWindow(Target.Get(), Grid, Window, 1.f, Size, .016f, false);
                if (Frame == 0)
                {
                    // Virtualized sample entries are created by the first layout pass.
                    FTextureCompilingManager::Get().FinishAllCompilation();
                    if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
                    FlushRenderingCommands();
                }
            }
            if (PreviewName == TEXT("Catalog"))
            {
                if (Preview->GetWidgetFromName(TEXT("Overlay_V9Catalog")))
                    for (const TCHAR* HiddenName : {TEXT("Panel_V9Retired"), TEXT("Popup_V9Filters"), TEXT("Popup_V9More")})
                    {
                        UWidget* Hidden = Preview->GetWidgetFromName(HiddenName);
                        if (TestNotNull(TEXT("Designer V9 conditional surface"), Hidden))
                            TestTrue(FString(HiddenName) + TEXT(" does not cover the Designer catalog"), Hidden->bHiddenInDesigner);
                    }
                auto* Tiles = Cast<UListViewBase>(Preview->GetWidgetFromName(TEXT("TileView_Facilities")));
                if (TestNotNull(TEXT("Designer facility list"), Tiles))
                {
                    TestTrue(TEXT("Designer facility list has a rendered area"), Tiles->GetCachedGeometry().GetLocalSize().GetMin() > 0.f);
                    TestTrue(TEXT("Designer facility list supplies preview entries"), Tiles->GetDisplayedEntryWidgets().Num() > 0);
                    bool bCheckedThumbnail = false;
                    for (UUserWidget* Entry : Tiles->GetDisplayedEntryWidgets())
                        if (auto* Thumbnail = Cast<UImage>(Entry->GetWidgetFromName(TEXT("Image_Thumbnail"))))
                        {
                            bCheckedThumbnail = true;
                            AddInfo(FString::Printf(TEXT("Catalog designer thumbnail: brush=%s desired=%s geometry=%s hidden=%d resource=%s"),
                                *FVector2D(Thumbnail->GetBrush().ImageSize).ToString(), *Thumbnail->GetDesiredSize().ToString(),
                                *Thumbnail->GetCachedGeometry().GetLocalSize().ToString(), Thumbnail->bHiddenInDesigner,
                                *GetNameSafe(Thumbnail->GetBrush().GetResourceObject())));
                            UWidget* Placeholder = Entry->GetWidgetFromName(TEXT("Text_ThumbnailPlaceholder"));
                            UWidget* Frame = Entry->GetWidgetFromName(TEXT("SizeBox_Thumbnail"));
                            const bool bHasTexture = Thumbnail->GetBrush().GetResourceObject() != nullptr;
                            // 제작 샘플은 실제 썸네일 또는 준비 중 표시를 사용한다. 빈 이미지의 렌더링을 요구하지 않는다.
                            TestTrue(TEXT("Catalog Designer reserves a visible thumbnail frame"),
                                Frame && Frame->GetCachedGeometry().GetLocalSize().GetMin() > 0.f);
                            TestTrue(TEXT("Catalog Designer shows one coherent thumbnail state"), bHasTexture
                                ? !Thumbnail->bHiddenInDesigner && Thumbnail->GetDesiredSize().GetMin() > 0.f &&
                                    Thumbnail->GetCachedGeometry().GetLocalSize().GetMin() > 0.f
                                : Thumbnail->bHiddenInDesigner && Placeholder && !Placeholder->bHiddenInDesigner &&
                                    Placeholder->GetCachedGeometry().GetLocalSize().GetMin() > 0.f);
                            break;
                        }
                    TestTrue(TEXT("Designer preview validates an actual thumbnail entry"), bCheckedThumbnail);
                }
            }
            if (PreviewName == TEXT("BuildSettings"))
                for (const TCHAR* LabelName : {TEXT("Text_SettingsGrid"), TEXT("Text_SettingsFacilityRanges"),
                    TEXT("Text_SettingsSynergyPreview"), TEXT("Text_SettingsDirectAdjustment"),
                    TEXT("Text_SettingsZoomToCursor"), TEXT("Text_SettingsEdgeScroll")})
                    if (UWidget* Label = Preview->GetWidgetFromName(LabelName))
                        TestTrue(TEXT("Designer setting state fits its button"),
                            Label->GetDesiredSize().X <= Label->GetCachedGeometry().GetLocalSize().X + 1.f);
            if (PreviewName.StartsWith(TEXT("Codex")))
            {
                UWidget* Dialog = Preview->GetWidgetFromName(TEXT("Size_Dialog"));
                if (TestNotNull(TEXT("Designer codex dialog"), Dialog))
                {
                    const FSlateRect Bounds = Dialog->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                    TestTrue(TEXT("Designer codex preserves the inset reference composition"),
                        Bounds.Left > 0.f && Bounds.Top > 0.f &&
                        Bounds.Right < Resolution.X && Bounds.Bottom < Resolution.Y &&
                        FMath::IsNearlyEqual(Bounds.GetSize().X / Bounds.GetSize().Y, 1584.f / 864.f, .002f));
                }
                for (const TCHAR* HiddenName : {TEXT("Text_Empty"), TEXT("Border_RewardFeedback")})
                    if (UWidget* Hidden = Preview->GetWidgetFromName(HiddenName))
                        TestTrue(TEXT("Designer sample does not overlay conditional feedback"), Hidden->bHiddenInDesigner);
                if (PreviewName == TEXT("Codex"))
                {
                    auto* Tiles = Cast<UFF_FacilityCodexTileView>(Preview->GetWidgetFromName(TEXT("TileView_Entries")));
                    if (TestNotNull(TEXT("Designer collection tiles"), Tiles))
                    {
                        const auto* ParallelProperty = FindFProperty<FBoolProperty>(Preview->GetClass(), TEXT("bUseParallelCollectionLayout"));
                        const int32 Columns = ParallelProperty && ParallelProperty->GetPropertyValue_InContainer(Preview.Get()) ? 2 : 6;
                        TestEqual(TEXT("Designer collection uses the active authored layout"), Tiles->ColumnCount, Columns);
                        TArray<FSlateRect> CardBounds;
                        for (UUserWidget* Entry : Tiles->GetDisplayedEntryWidgets())
                            if (Entry && Entry->GetCachedGeometry().GetLocalSize().X > 1.f)
                                CardBounds.Add(Entry->GetPaintSpaceGeometry().GetLayoutBoundingRect());
                        CardBounds.Sort([](const FSlateRect& A, const FSlateRect& B)
                        { return FMath::Abs(A.Top - B.Top) > 1.f ? A.Top < B.Top : A.Left < B.Left; });
                        if (TestTrue(TEXT("Designer provides two rows in the active layout"), CardBounds.Num() > Columns))
                            TestTrue(TEXT("Cards fill the configured columns before starting their next row"),
                                FMath::Abs(CardBounds[0].Top - CardBounds[Columns - 1].Top) <= 1.f &&
                                CardBounds[Columns].Top > CardBounds[0].Bottom - 1.f);
                    }
                    auto* Track = Preview->GetWidgetFromName(TEXT("Size_ReadableMilestoneTrack"));
                    auto* RewardViewport = Preview->GetWidgetFromName(TEXT("Scroll_ReadableMilestones"));
                    auto* Filters = Preview->GetWidgetFromName(TEXT("Horizontal_Toolbar"));
                    if (TestNotNull(TEXT("Designer progress track"), Track) &&
                        TestNotNull(TEXT("Designer reward viewport"), RewardViewport) && TestNotNull(TEXT("Designer filters"), Filters))
                    {
                        const FSlateRect TrackRect = Track->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                        const FSlateRect RewardRect = RewardViewport->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                        const FSlateRect DialogRect = Dialog->GetPaintSpaceGeometry().GetLayoutBoundingRect();
                        TestTrue(TEXT("Designer progress remains readable inside its current reward viewport"),
                            TrackRect.GetSize().X > 0.f && RewardRect.GetSize().X > 0.f &&
                            TrackRect.Left >= RewardRect.Left - 1.f && TrackRect.Right <= RewardRect.Right + 1.f &&
                            RewardRect.Left >= DialogRect.Left - 1.f && RewardRect.Right <= DialogRect.Right + 1.f &&
                            RewardRect.Bottom <= DialogRect.Bottom + 1.f);
                    }
                }
            }
            TArray<FColor> Pixels;
            FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
            if (!TestTrue(TEXT("Designer pixels can be read"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags))) return false;
            TArray64<uint8> Png;
            FImageUtils::PNGCompressImageArray(Resolution.X, Resolution.Y, Pixels, Png);
            const FString Path = Directory / FString::Printf(TEXT("%s_%dx%d.png"), Screen.Name, Resolution.X, Resolution.Y);
            TestTrue(TEXT("Designer screenshot saved"), FFileHelper::SaveArrayToFile(Png, *Path));
        }
        Slate.Reset();
        FWidgetBlueprintEditorUtils::DestroyUserWidget(Preview.Get());
    }
    return true;
}
#endif
