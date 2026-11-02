// FF_BuildSynergyPresentationTest.cpp
// 역할: 시설 조합 화면의 생성·해제·실제 배치를 검증한다.

// One integration fixture covers presentation lifetime, bounded data and the real Slate layout.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Building/Presentation/FF_BuildSynergyWidget.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Tests/AutomationCommon.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
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
    TestTrue(TEXT("Live combination card sits beside the right inspector below the toolbar"),
        SynergySlot && SynergySlot->GetAnchors().Minimum.Equals(FVector2D(1.f, 0.f)) &&
        SynergySlot->GetAlignment().Equals(FVector2D(1.f, 0.f)) &&
        SynergySlot->GetPosition().Equals(FVector2D(-512.f, 104.f)));
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
    TestTrue(TEXT("Native fallback shares the same right-side placement without changing its width"),
        FallbackSize && FallbackSize->GetWidthOverride() == 344.f && FallbackSlot &&
        FallbackSlot->GetAnchors().Minimum.Equals(FVector2D(1.f, 0.f)) &&
        FallbackSlot->GetAlignment().Equals(FVector2D(1.f, 0.f)) &&
        FallbackSlot->GetPosition().Equals(FVector2D(-512.f, 104.f)));
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
    // 제품 배치 화면은 시설 정보 안에 조합을 통합했다. 호환 위젯을 다시 겹쳐 띄우지 않는다.
    {
        FTestWorldWrapper GameFixture;
        if (!GameFixture.CreateTestWorld(EWorldType::Game)) { GameFixture.ForwardErrorMessages(this); return false; }
        auto* Build = GameFixture.GetTestWorld()->GetSubsystem<UFF_BuildModeSubsystem>();
        auto* PopupProperty = Build ? FindFProperty<FObjectProperty>(Build->GetClass(), TEXT("BuildSynergyWidget")) : nullptr;
        if (!TestNotNull(TEXT("Current build presentation owner"), Build) ||
            !TestNotNull(TEXT("Compatibility popup fixture seam"), PopupProperty)) return false;
        PopupProperty->SetObjectPropertyValue_InContainer(Build, Widget.Get());
        Build->RefreshBuildToolsOcclusion();
        TestNull(TEXT("Current placement never retains the retired standalone synergy popup"),
            PopupProperty->GetObjectPropertyValue_InContainer(Build));
        TestEqual(TEXT("Retiring the popup also clears its rendered content"), Widget->GetVisibility(), ESlateVisibility::Collapsed);
    }
    Widget->PresentPreview(Preview, FVector::ZeroVector);
    // 호환 Blueprint 자체의 수명·지역화·렌더링은 계속 검증하되 제거된 도크와 합성하지 않는다.
    for (const FIntPoint Size : {FIntPoint(1280, 720), FIntPoint(1920, 1080)})
    {
        FWidgetRenderer Renderer(false, true);
        TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(FVector2D(Size), TF_Bilinear, true));
        if (!TestNotNull(TEXT("Compatibility preview render target"), Target.Get())) return false;
        for (int32 Frame = 0; Frame < 5; ++Frame)
        { Widget->ForceLayoutPrepass(); Renderer.DrawWidget(Target.Get(), SynergySlate, FVector2D(Size), 0.f, false); }
        const FSlateRect Card = Widget->CardPanel->GetCachedGeometry().GetLayoutBoundingRect();
        TestTrue(TEXT("The editable compatibility card remains inside its viewport"),
            Card.Left >= 0.f && Card.Top >= 0.f && Card.Right <= Size.X && Card.Bottom <= Size.Y && Card.GetSize().GetMin() > 0.f);
        const FSlateRect Legend = Widget->Legend->GetCachedGeometry().GetLayoutBoundingRect();
        TestTrue(TEXT("Localized legend fits its authored card"), Legend.Left >= Card.Left && Legend.Right <= Card.Right &&
            Widget->Legend->GetDesiredSize().X <= Legend.GetSize().X + 1.f);
        float PreviousBottom = Legend.Bottom;
        for (int32 Index = 0; Index < 3; ++Index)
        {
            const FSlateRect Title = Widget->RowTitles[Index]->GetCachedGeometry().GetLayoutBoundingRect();
            const FSlateRect Detail = Widget->RowDetails[Index]->GetCachedGeometry().GetLayoutBoundingRect();
            TestTrue(TEXT("All three compatibility rows remain readable without overlap"),
                Title.Top >= PreviousBottom - 1.f && Detail.Top >= Title.Bottom - 1.f && Detail.Bottom < Card.Bottom);
            TestTrue(TEXT("The full localized explanation fits without ellipsis"),
                Widget->RowTitles[Index]->GetDesiredSize().X <= Title.GetSize().X + 1.f &&
                Widget->RowDetails[Index]->GetDesiredSize().X <= Detail.GetSize().X + 1.f &&
                Widget->RowDetails[Index]->GetDesiredSize().Y <= Detail.GetSize().Y + 1.f);
            PreviousBottom = Detail.Bottom;
        }
        const FSlateRect Footer = Widget->Footer->GetCachedGeometry().GetLayoutBoundingRect();
        TestTrue(TEXT("Compatibility footer remains below its rows"), Footer.Top >= PreviousBottom - 1.f && Footer.Bottom <= Card.Bottom);
        TArray<FColor> Pixels;
        FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
        if (!TestTrue(TEXT("Compatibility view supplies complete rendered pixels"),
            Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags) && Pixels.Num() == Size.X * Size.Y)) return false;
        TArray64<uint8> Png;
        FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
        const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/FacilitySynergy");
        IFileManager::Get().MakeDirectory(*Directory, true);
        TestTrue(TEXT("Compatibility presentation screenshot saved"), FFileHelper::SaveArrayToFile(Png,
            *(Directory / FString::Printf(TEXT("Synergy_%dx%d.png"), Size.X, Size.Y))));
    }
    Widget->ClearPreview();
    return true;
}
#endif
