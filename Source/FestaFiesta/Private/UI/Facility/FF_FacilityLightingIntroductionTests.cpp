// FF_FacilityLightingIntroductionTests.cpp
// 역할: 조명 색상 변경의 첫 안내, 한 번만 표시되는 강조와 프로필 저장을 실제 WBP로 검증한다.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Layout/SDPIScaler.h"

namespace
{
	// Never read or consume the developer's actual first-use preference.
	class FLightingIntroductionProfile
	{
	public:
		FLightingIntroductionProfile()
			: Path(FPaths::ConvertRelativePathToFull(FPaths::CreateTempFilename(
				*FPaths::ProjectSavedDir(), TEXT("LightingIntroductionTest"), TEXT(".ini"))))
			, PreviousPath(GGameUserSettingsIni)
		{
			FPaths::NormalizeFilename(Path);
			if (GConfig) GConfig->Add(Path, FConfigFile());
			GGameUserSettingsIni = Path;
		}
		~FLightingIntroductionProfile()
		{
			if (GConfig) GConfig->UnloadFile(Path);
			GGameUserSettingsIni = PreviousPath;
			IFileManager::Get().Delete(*Path);
		}
		bool IsSaved() const
		{
			FConfigFile Saved;
			Saved.Read(Path);
			bool bSeen = false;
			return Saved.GetBool(TEXT("FestaFiesta.FacilityLighting"),
				TEXT("bHasSeenColorIntroduction"), bSeen) && bSeen;
		}
	private:
		FString Path;
		FString PreviousPath;
	};

	struct FLightingIntroductionPanel
	{
		TStrongObjectPtr<UFF_FacilityInfoPanel> Panel;
		TSharedPtr<SWidget> Slate;
		UButton* Help = nullptr;
		UButton* Tab = nullptr;
		UButton* Dismiss = nullptr;
		UBorder* Hint = nullptr;
		UWidget* HelpRow = nullptr;
		UWidget* Introduction = nullptr;
		UFF_FacilityLightingControls* Controls = nullptr;
		int32 Writes = 0;
		int32 Rollbacks = 0;
		int32 Resets = 0;

		bool Create(FAutomationTestBase& Test, UWorld* World, UWidgetBlueprintGeneratedClass* Class)
		{
			const bool bPrevious = Class->bCanCallInitializedWithoutPlayerContext;
			Class->bCanCallInitializedWithoutPlayerContext = true;
			Panel.Reset(CreateWidget<UFF_FacilityInfoPanel>(World, Class));
			Class->bCanCallInitializedWithoutPlayerContext = bPrevious;
			if (!Test.TestNotNull(TEXT("Production inspector instance"), Panel.Get())) return false;
			// UWidget caches Slate weakly; production keeps this alive in the viewport.
			Slate = Panel->TakeWidget();
			Help = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_LightingColorHelp")));
			Tab = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_TabLighting")));
			Dismiss = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_DismissLightingIntroduction")));
			Hint = Cast<UBorder>(Panel->GetWidgetFromName(TEXT("Border_LightingIntroductionHint")));
			HelpRow = Panel->GetWidgetFromName(TEXT("Panel_LightingColorHelp"));
			Introduction = Panel->GetWidgetFromName(TEXT("Panel_LightingIntroduction"));
			Controls = Cast<UFF_FacilityLightingControls>(Panel->GetWidgetFromName(TEXT("Widget_LightingControls")));
			if (!Test.TestNotNull(TEXT("Overview color action"), Help) ||
				!Test.TestNotNull(TEXT("Lighting tab action"), Tab) ||
				!Test.TestNotNull(TEXT("Explanation continue action"), Dismiss) ||
				!Test.TestNotNull(TEXT("First-use red reminder"), Hint) ||
				!Test.TestNotNull(TEXT("Overview color action wrapper"), HelpRow) ||
				!Test.TestNotNull(TEXT("Authored lighting explanation"), Introduction) ||
				!Test.TestNotNull(TEXT("Production lighting editor"), Controls)) return false;
			Panel->OnLightingRequested.AddLambda([this](FGuid, const FFFZSFacilityLightingState&) { ++Writes; });
			Panel->OnLightingRollbackRequested.AddLambda([this](FGuid, const FFFZSFacilityLightingState&) { ++Rollbacks; });
			Panel->OnLightingResetRequested.AddLambda([this](FGuid) { ++Resets; });
			return true;
		}
		~FLightingIntroductionPanel()
		{
			if (!Panel.IsValid()) return;
			Panel->OnLightingRequested.Clear();
			Panel->OnLightingRollbackRequested.Clear();
			Panel->OnLightingResetRequested.Clear();
			Panel->SetPanelOpen(false);
			Slate.Reset();
			Panel->ReleaseSlateResources(true);
		}
		void CheckNoCommands(FAutomationTestBase& Test) const
		{
			Test.TestEqual(TEXT("Reading the guide sends no lighting preview"), Writes, 0);
			Test.TestEqual(TEXT("Reading the guide sends no lighting rollback"), Rollbacks, 0);
			Test.TestEqual(TEXT("Reading the guide sends no lighting reset"), Resets, 0);
		}
	};

	FFF_FacilityInfoViewData MakeLightingView(const bool bStage)
	{
		FFF_FacilityInfoViewData View;
		View.FacilityInstanceId = FGuid::NewGuid();
		View.FacilityDefinitionId = bStage ? TEXT("Performance_MainStage_Test") : TEXT("Decoration_Lighting_Test");
		View.Context = EFF_FacilityInfoContext::Live;
		View.ContextTitle = FText::FromString(TEXT("시설 정보"));
		View.DisplayName = FText::FromString(bStage ? TEXT("메인 공연장") : TEXT("조명"));
		View.CategoryText = FText::FromString(bStage ? TEXT("공연 시설") : TEXT("장식 시설"));
		View.ZoneText = FText::FromString(TEXT("페스티벌 구역"));
		View.bHasLightingRig = true;
		View.bCanEditLighting = true;
		View.LightingState.bEnabled = true;
		View.LightingState.BrightnessMultiplier = 1.f;
		return View;
	}

	bool CaptureLightingIntroduction(FAutomationTestBase& Test, FLightingIntroductionPanel& Fixture,
		const FIntPoint Resolution, const bool bExplanation)
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi"))) return true;
		const FVector2D Size(Resolution.X, Resolution.Y);
		const float Dpi = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Resolution);
		FWidgetRenderer Renderer(false, true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, true));
		if (!Test.TestNotNull(TEXT("Lighting introduction capture target"), Target.Get())) return false;
		const TSharedRef<SWidget> CaptureWidget = SNew(SDPIScaler).DPIScale(Dpi)[Fixture.Panel->TakeWidget()];
		for (int32 Frame = 0; Frame < 8; ++Frame)
		{
			Fixture.Panel->ForceLayoutPrepass();
			Renderer.DrawWidget(Target.Get(), CaptureWidget, Size, .016f, false);
		}
		FlushRenderingCommands();
		UButton* Action = bExplanation ? Fixture.Dismiss : Fixture.Help;
		const FSlateRect ActionRect = Action->GetCachedGeometry().GetLayoutBoundingRect();
		Test.TestEqual(TEXT("Rendering preserves the expected explanation state"),
			Fixture.Panel->IsLightingIntroductionVisible(), bExplanation);
		Test.TestTrue(*FString::Printf(TEXT("Lighting %s action fits the %dx%d viewport"),
			bExplanation ? TEXT("explanation") : TEXT("overview"), Resolution.X, Resolution.Y),
			Action->IsVisible() && ActionRect.Left >= 0.f && ActionRect.Top >= 0.f &&
			ActionRect.Right <= Size.X + 1.f && ActionRect.Bottom <= Size.Y + 1.f &&
			ActionRect.Right > ActionRect.Left && ActionRect.Bottom > ActionRect.Top);
		if (!bExplanation)
		{
			const FSlateRect HintRect = Fixture.Hint->GetCachedGeometry().GetRenderBoundingRect();
			Test.TestTrue(TEXT("The red reminder fits inside the color action"), Fixture.Hint->IsVisible() &&
				HintRect.Right > HintRect.Left && HintRect.Bottom > HintRect.Top &&
				HintRect.Left >= ActionRect.Left - .5f && HintRect.Top >= ActionRect.Top - .5f &&
				HintRect.Right <= ActionRect.Right + .5f && HintRect.Bottom <= ActionRect.Bottom + .5f);
		}
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags ReadFlags;
		ReadFlags.SetLinearToGamma(false);
		if (!Test.TestTrue(TEXT("Lighting introduction pixels are readable"),
			Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, ReadFlags))) return false;
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Resolution.X, Resolution.Y, Pixels, Png);
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("FacilityLightingIntroduction");
		IFileManager::Get().MakeDirectory(*Directory, true);
		return Test.TestTrue(TEXT("Lighting introduction screenshot saved"), FFileHelper::SaveArrayToFile(Png,
			*(Directory / FString::Printf(TEXT("Lighting%s_%dx%d.png"),
				bExplanation ? TEXT("Explanation") : TEXT("Overview"), Resolution.X, Resolution.Y))));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityLightingIntroductionTest,
	"FestaFiesta.UI.FacilityInfo.LightingFirstUseIntroduction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityLightingIntroductionTest::RunTest(const FString&)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_FacilityInfoPanel>(nullptr,
		FFBuildUIAssetPaths::FacilityInfoPanelClass));
	if (!TestNotNull(TEXT("Production inspector WBP"), Class) ||
		!TestNotNull(TEXT("Editor world"), World) || !TestNotNull(TEXT("Config cache"), GConfig)) return false;

	// Both entry routes must explain the feature when they are the first clicked action.
	for (int32 Entry = 0; Entry < 2; ++Entry)
	{
		FLightingIntroductionProfile Profile;
		FLightingIntroductionPanel Fixture;
		if (!Fixture.Create(*this, World, Class)) return false;
		UFF_FacilityInfoPanel& Panel = *Fixture.Panel.Get();
		FFF_FacilityInfoViewData View = MakeLightingView(Entry == 0);
		Panel.ApplyViewData(View);
		Panel.SetPanelOpen(true);
		TestTrue(TEXT("Overview advertises that lighting colors can change"), Fixture.HelpRow->IsVisible());
		TestTrue(TEXT("Fresh profile shows the red reminder"), Fixture.Hint->IsVisible());
		TestFalse(TEXT("Selecting a facility does not open the explanation"), Panel.IsLightingIntroductionVisible());
		TestFalse(TEXT("Merely displaying the reminder does not consume it"), Profile.IsSaved());
		TestEqual(TEXT("The editor is initially hidden"), Fixture.Controls->GetVisibility(), ESlateVisibility::Collapsed);
		Panel.SetInfoPage(EFF_FacilityInfoPage::Operation);
		TestEqual(TEXT("The color action belongs only to overview"), Fixture.HelpRow->GetVisibility(), ESlateVisibility::Collapsed);
		Panel.SetInfoPage(EFF_FacilityInfoPage::Overview);
		if (Entry == 0)
		{
			if (!CaptureLightingIntroduction(*this, Fixture, FIntPoint(1280, 720), false) ||
				!CaptureLightingIntroduction(*this, Fixture, FIntPoint(1920, 1080), false)) return false;
		}
		(Entry == 0 ? Fixture.Help : Fixture.Tab)->OnClicked.Broadcast();
		TestTrue(TEXT("Either first-use entry displays the explanation"), Panel.IsLightingIntroductionVisible());
		TestTrue(TEXT("The authored explanation is visible"), Fixture.Introduction->IsVisible());
		TestEqual(TEXT("Showing the explanation removes the red reminder"), Fixture.Hint->GetVisibility(), ESlateVisibility::Collapsed);
		TestEqual(TEXT("Color controls stay hidden while reading"), Fixture.Controls->GetVisibility(), ESlateVisibility::Collapsed);
		TestFalse(TEXT("Reading the guide does not begin an edit transaction"), Fixture.Controls->IsEditing());
		TestTrue(TEXT("The first explanation persists its seen flag to disk"), Profile.IsSaved());
		Panel.ApplyViewData(View);
		TestTrue(TEXT("Normal facility refresh preserves the explanation"), Panel.IsLightingIntroductionVisible());
		if (Entry == 0)
		{
			if (!CaptureLightingIntroduction(*this, Fixture, FIntPoint(1280, 720), true) ||
				!CaptureLightingIntroduction(*this, Fixture, FIntPoint(1920, 1080), true)) return false;
		}
		Fixture.CheckNoCommands(*this);
		Fixture.Dismiss->OnClicked.Broadcast();
		TestFalse(TEXT("Continue dismisses the explanation"), Panel.IsLightingIntroductionVisible());
		TestEqual(TEXT("Continue hides its explanation surface"), Fixture.Introduction->GetVisibility(), ESlateVisibility::Collapsed);
		TestTrue(TEXT("Continue reveals the lighting editor"), Fixture.Controls->IsVisible());
		TestTrue(TEXT("Editing starts after the explanation"), Fixture.Controls->IsEditing());
		Fixture.CheckNoCommands(*this);
		Panel.SetPanelOpen(false);
		Panel.SetPanelOpen(true);
		Fixture.Tab->OnClicked.Broadcast();
		TestFalse(TEXT("Reopening lighting skips the consumed explanation"), Panel.IsLightingIntroductionVisible());
		TestEqual(TEXT("Reopening never restores the red reminder"), Fixture.Hint->GetVisibility(), ESlateVisibility::Collapsed);
		TestTrue(TEXT("Reopening resumes a cancellable lighting edit"), Fixture.Controls->IsEditing());

		// The preference is profile-wide, across both facility definitions and new widgets.
		View = MakeLightingView(Entry != 0);
		Panel.ApplyViewData(View);
		TestTrue(TEXT("Other lighting facilities retain a discoverable color action"), Fixture.HelpRow->IsVisible());
		TestEqual(TEXT("Switching stage and lighting preserves the consumed reminder"), Fixture.Hint->GetVisibility(), ESlateVisibility::Collapsed);
		Fixture.Help->OnClicked.Broadcast();
		TestFalse(TEXT("The other facility does not repeat the explanation"), Panel.IsLightingIntroductionVisible());
		TestTrue(TEXT("The other facility opens its editor directly"), Fixture.Controls->IsVisible());
		View.FacilityInstanceId = FGuid::NewGuid();
		View.bHasLightingRig = false;
		View.bCanEditLighting = false;
		Panel.ApplyViewData(View);
		TestEqual(TEXT("Facilities without a rig hide the color action"), Fixture.HelpRow->GetVisibility(), ESlateVisibility::Collapsed);
		TestEqual(TEXT("Facilities without a rig hide the lighting tab"), Fixture.Tab->GetVisibility(), ESlateVisibility::Collapsed);
		TestFalse(TEXT("Facilities without a rig have no explanation"), Panel.IsLightingIntroductionVisible());
		FLightingIntroductionPanel Recreated;
		if (!Recreated.Create(*this, World, Class)) return false;
		Recreated.Panel->ApplyViewData(MakeLightingView(true));
		Recreated.Panel->SetPanelOpen(true);
		TestTrue(TEXT("Recreated inspectors keep the color action"), Recreated.HelpRow->IsVisible());
		TestEqual(TEXT("Recreated inspectors read the saved reminder preference"), Recreated.Hint->GetVisibility(), ESlateVisibility::Collapsed);
		Recreated.Tab->OnClicked.Broadcast();
		TestFalse(TEXT("Recreated inspectors skip the consumed explanation"), Recreated.Panel->IsLightingIntroductionVisible());
		TestTrue(TEXT("Recreated inspectors open lighting controls directly"), Recreated.Controls->IsVisible());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityLightingIntroductionGuardsTest,
	"FestaFiesta.UI.FacilityInfo.LightingIntroductionGuards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityLightingIntroductionGuardsTest::RunTest(const FString&)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_FacilityInfoPanel>(nullptr,
		FFBuildUIAssetPaths::FacilityInfoPanelClass));
	if (!TestNotNull(TEXT("Production inspector WBP"), Class) ||
		!TestNotNull(TEXT("Editor world"), World) || !TestNotNull(TEXT("Config cache"), GConfig)) return false;
	FLightingIntroductionProfile Profile;
	FLightingIntroductionPanel Fixture;
	if (!Fixture.Create(*this, World, Class)) return false;
	for (int32 InvalidCase = 0; InvalidCase < 5; ++InvalidCase)
	{
		FFF_FacilityInfoViewData View = MakeLightingView(true);
		if (InvalidCase == 0) View.Context = EFF_FacilityInfoContext::None;
		if (InvalidCase == 1) View.bHasLightingRig = false;
		if (InvalidCase == 2) View.bCanEditLighting = false;
		if (InvalidCase == 3) View.FacilityInstanceId.Invalidate();
		Fixture.Panel->ApplyViewData(View);
		Fixture.Panel->SetPanelOpen(InvalidCase != 4);
		// Broadcast deliberately bypasses button disabling to exercise native event guards.
		Fixture.Help->OnClicked.Broadcast();
		Fixture.Tab->OnClicked.Broadcast();
		Fixture.Dismiss->OnClicked.Broadcast();
		TestFalse(*FString::Printf(TEXT("Ineligible entry %d does not consume first use"), InvalidCase), Profile.IsSaved());
		TestFalse(TEXT("Ineligible entry cannot open the explanation"), Fixture.Panel->IsLightingIntroductionVisible());
		TestFalse(TEXT("Ineligible entry cannot begin a lighting edit"), Fixture.Controls->IsEditing());
		Fixture.CheckNoCommands(*this);
	}
	Fixture.Panel->ApplyViewData(MakeLightingView(true));
	Fixture.Panel->SetPanelOpen(true);
	TestTrue(TEXT("An eligible facility still receives its first reminder"), Fixture.Hint->IsVisible());
	Fixture.Help->OnClicked.Broadcast();
	TestTrue(TEXT("The first eligible click still opens the explanation"), Fixture.Panel->IsLightingIntroductionVisible());
	TestTrue(TEXT("The first eligible click finally saves the preference"), Profile.IsSaved());
	Fixture.CheckNoCommands(*this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityLightingIntroductionInterruptionTest,
	"FestaFiesta.UI.FacilityInfo.LightingIntroductionInterruption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityLightingIntroductionInterruptionTest::RunTest(const FString&)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_FacilityInfoPanel>(nullptr,
		FFBuildUIAssetPaths::FacilityInfoPanelClass));
	if (!TestNotNull(TEXT("Production inspector WBP"), Class) ||
		!TestNotNull(TEXT("Editor world"), World) || !TestNotNull(TEXT("Config cache"), GConfig)) return false;
	for (int32 Interruption = 0; Interruption < 3; ++Interruption)
	{
		FLightingIntroductionProfile Profile;
		FLightingIntroductionPanel Fixture;
		if (!Fixture.Create(*this, World, Class)) return false;
		FFF_FacilityInfoViewData View = MakeLightingView(true);
		Fixture.Panel->ApplyViewData(View);
		Fixture.Panel->SetPanelOpen(true);
		Fixture.Help->OnClicked.Broadcast();
		TestTrue(TEXT("The interrupted guide was initially open"), Fixture.Panel->IsLightingIntroductionVisible());
		if (Interruption == 0) Fixture.Panel->SetPanelOpen(false);
		else
		{
			if (Interruption == 1) View = MakeLightingView(false);
			else View.Context = EFF_FacilityInfoContext::Build;
			Fixture.Panel->ApplyViewData(View);
		}
		TestFalse(TEXT("Closing or changing the selection dismisses its explanation"), Fixture.Panel->IsLightingIntroductionVisible());
		TestEqual(TEXT("An interrupted explanation surface is hidden"), Fixture.Introduction->GetVisibility(), ESlateVisibility::Collapsed);
		TestEqual(TEXT("An interrupted guide never restores the red reminder"), Fixture.Hint->GetVisibility(), ESlateVisibility::Collapsed);
		TestTrue(TEXT("An interrupted guide remains saved as seen"), Profile.IsSaved());
		Fixture.CheckNoCommands(*this);
		Fixture.Panel->SetPanelOpen(true);
		Fixture.Tab->OnClicked.Broadcast();
		TestFalse(TEXT("Returning after interruption skips the explanation"), Fixture.Panel->IsLightingIntroductionVisible());
		TestTrue(TEXT("Returning after interruption opens the editor"), Fixture.Controls->IsVisible());
		TestTrue(TEXT("Returning after interruption begins a cancellable edit"), Fixture.Controls->IsEditing());
	}
	return true;
}

#endif
