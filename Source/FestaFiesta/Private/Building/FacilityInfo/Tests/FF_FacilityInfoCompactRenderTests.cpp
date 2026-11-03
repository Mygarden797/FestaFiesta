// Exercises the shipped compact inspector's real Slate layout and color-wheel input.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Building/FacilityInfo/FF_FacilityColorPicker.h"
#include "Building/FacilityInfo/FF_FacilityInfoPanel.h"
#include "Building/FacilityInfo/FF_FacilityLightingControls.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Economy/FF_FestivalEconomyTypes.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Input/HittestGrid.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SVirtualWindow.h"

namespace FFFacilityCompactRenderTest
{
	class FIsolatedLightingPreference
	{
	public:
		FIsolatedLightingPreference()
			: Path(FPaths::ConvertRelativePathToFull(FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("CompactLightingTest"), TEXT(".ini"))))
			, PreviousPath(GGameUserSettingsIni)
		{
			FPaths::NormalizeFilename(Path);
			if (GConfig)
			{
				GConfig->Add(Path, FConfigFile());
				GConfig->SetBool(TEXT("FestaFiesta.FacilityLighting"), TEXT("bHasSeenColorIntroduction"), true, Path);
			}
			GGameUserSettingsIni = Path;
		}
		~FIsolatedLightingPreference()
		{
			GGameUserSettingsIni = PreviousPath;
			if (GConfig) GConfig->UnloadFile(Path);
			IFileManager::Get().Delete(*Path);
		}
	private:
		FString Path;
		FString PreviousPath;
	};

	bool IsVisible(const UWidget* Widget)
	{
		for (const UWidget* Current = Widget; Current; Current = Current->GetParent())
			if (!Current->IsVisible()) return false;
		return Widget != nullptr;
	}

	bool Contains(const FSlateRect& Outer, const FSlateRect& Inner)
	{
		constexpr float Tolerance = 1.5f;
		return Inner.GetSize().X > 0 && Inner.GetSize().Y > 0 &&
			Inner.Left >= Outer.Left - Tolerance && Inner.Right <= Outer.Right + Tolerance &&
			Inner.Top >= Outer.Top - Tolerance && Inner.Bottom <= Outer.Bottom + Tolerance;
	}

	void CollectWidgets(UUserWidget& Owner, TArray<UWidget*>& Out)
	{
		if (!Owner.WidgetTree) return;
		TArray<UWidget*> Children;
		Owner.WidgetTree->GetAllWidgets(Children);
		for (UWidget* Widget : Children)
		{
			Out.Add(Widget);
			if (auto* Nested = Cast<UUserWidget>(Widget); Nested && IsVisible(Nested)) CollectWidgets(*Nested, Out);
		}
	}

	struct FFixture
	{
		TStrongObjectPtr<UFF_FacilityInfoPanel> Panel;
		TSharedPtr<SWidget> PanelSlate;
		TSharedPtr<SVirtualWindow> Window;
		TStrongObjectPtr<UTextureRenderTarget2D> Target;
		FWidgetRenderer Renderer{false, true};
		FHittestGrid HitGrid;
		FVector2D Viewport;
		float Dpi = 1.f;

		~FFixture()
		{
			if (FSlateApplication::IsInitialized() && Window)
			{
				FSlateApplication::Get().ReleaseAllPointerCapture();
				FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
				FSlateApplication::Get().UnregisterVirtualWindow(Window.ToSharedRef());
			}
			if (Panel)
			{
				Panel->OnLightingRequested.Clear();
				Panel->OnLightingRollbackRequested.Clear();
				Panel->SetPanelOpen(false);
			}
			Window.Reset();
			PanelSlate.Reset();
			if (Panel) Panel->ReleaseSlateResources(true);
			FlushRenderingCommands();
		}

		bool Initialize(FAutomationTestBase& Test, const FIntPoint Resolution)
		{
			Viewport = FVector2D(Resolution);
			Dpi = static_cast<float>(Resolution.X) / 1920.f;
			UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
			auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_FacilityInfoPanel>(nullptr, FFBuildUIAssetPaths::FacilityInfoPanelClass));
			auto* LightingClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_FacilityLightingControls>(nullptr, FFBuildUIAssetPaths::FacilityLightingControlsClass));
			if (!Test.TestNotNull(TEXT("Inspector fixture world"), World) || !Test.TestNotNull(TEXT("Shipped inspector WBP"), Class) ||
				!Test.TestNotNull(TEXT("Shipped nested lighting WBP"), LightingClass)) return false;
			{
				const bool bPreviousPanelInitialization = Class->bCanCallInitializedWithoutPlayerContext;
				const bool bPreviousLightingInitialization = LightingClass->bCanCallInitializedWithoutPlayerContext;
				Class->bCanCallInitializedWithoutPlayerContext = true;
				LightingClass->bCanCallInitializedWithoutPlayerContext = true;
				Panel.Reset(CreateWidget<UFF_FacilityInfoPanel>(World, Class));
				Class->bCanCallInitializedWithoutPlayerContext = bPreviousPanelInitialization;
				LightingClass->bCanCallInitializedWithoutPlayerContext = bPreviousLightingInitialization;
			}
			if (!Test.TestNotNull(TEXT("Transient actual inspector"), Panel.Get())) return false;
			PanelSlate = Panel->TakeWidget();
			Window = SNew(SVirtualWindow).Size(Viewport);
			Window->SetContent(SNew(SDPIScaler).DPIScale(Dpi)[PanelSlate.ToSharedRef()]);
			FSlateApplication::Get().RegisterVirtualWindow(Window.ToSharedRef());
			Target.Reset(FWidgetRenderer::CreateTargetFor(Viewport, TF_Bilinear, true));
			return Test.TestNotNull(TEXT("Inspector render target"), Target.Get());
		}

		void Paint()
		{
			for (int32 Frame = 0; Frame < 3; ++Frame)
				Renderer.DrawWindow(Target.Get(), HitGrid, Window.ToSharedRef(), 1.f, Viewport, .016f, false);
			FlushRenderingCommands();
		}

		bool Show(FAutomationTestBase& Test, const FFF_FacilityInfoViewData& Data, EFF_FacilityInfoPage Page, const FString& Context, const bool bPrecision = false)
		{
			Panel->ApplyViewData(Data);
			Panel->SetPanelOpen(true);
			Panel->SetInfoPage(Page);
			if (Page == EFF_FacilityInfoPage::Lighting)
			{
				if (auto* Lighting = Cast<UFF_FacilityLightingControls>(Panel->GetWidgetFromName(TEXT("Widget_LightingControls"))))
					if (UWidget* Precision = Lighting->GetWidgetFromName(TEXT("Panel_LightingPrecision")); Precision && Precision->IsVisible() != bPrecision)
						if (auto* Toggle = Cast<UButton>(Lighting->GetWidgetFromName(TEXT("Button_LightingPrecision")))) Toggle->OnClicked.Broadcast();
			}
			Paint();
			if (!Test.TestEqual(Context + TEXT(": supported page is selected"), Panel->GetInfoPage(), Page)) return false;
			auto* Bounds = Cast<USizeBox>(Panel->GetWidgetFromName(TEXT("Panel_Size")));
			UWidget* Body = Panel->GetWidgetFromName(TEXT("Body_Surface"));
			if (!Test.TestNotNull(Context + TEXT(": fixed inspector frame"), Bounds) || !Test.TestNotNull(Context + TEXT(": page body host"), Body)) return false;
			Test.TestTrue(Context + TEXT(": inspector remains400x300 logical units"), Bounds->GetCachedGeometry().GetLocalSize().Equals(FVector2D(400, 300), 1.f));
			const FSlateRect FrameRect = Bounds->GetCachedGeometry().GetLayoutBoundingRect();
			Test.TestTrue(Context + TEXT(": inspector fits the actual viewport"), Contains(FSlateRect(0, 0, Viewport.X, Viewport.Y), FrameRect));
			TArray<UWidget*> Widgets;
			TArray<TPair<UWidget*, FSlateRect>> TextRects;
			CollectWidgets(*Panel.Get(), Widgets);
			for (UWidget* Widget : Widgets)
			{
				if (!IsVisible(Widget)) continue;
				const FSlateRect Rect = Widget->GetCachedGeometry().GetLayoutBoundingRect();
				const FString Label = Context + TEXT(" / ") + Widget->GetName();
				if (const auto* Scroll = Cast<UScrollBox>(Widget))
					Test.TestTrue(Label + TEXT(": no scroll is needed"), FMath::IsNearlyZero(Scroll->GetScrollOffset()) && Scroll->GetScrollOffsetOfEnd() <= 1.f);
				FText Text;
				int32 FontSize = 0;
				if (const auto* Common = Cast<UCommonTextBlock>(Widget)) { Text = Common->GetText(); FontSize = Common->GetFont().Size; }
				else if (const auto* Plain = Cast<UTextBlock>(Widget)) { Text = Plain->GetText(); FontSize = Plain->GetFont().Size; }
				if (Text.IsEmpty()) continue;
				TextRects.Emplace(Widget, Rect);
				Test.TestTrue(Label + TEXT(": readable text is fully inside the inspector"), Contains(FrameRect, Rect));
				Test.TestTrue(Label + TEXT(": text has at least14px logical type"), FontSize >= 14);
				const FVector2D Desired = Widget->GetDesiredSize();
				const FVector2D Allocated = Widget->GetCachedGeometry().GetLocalSize();
				Test.TestTrue(Label + TEXT(": text needs neither clipping nor horizontal dragging"),
					Allocated.X + 1.5 >= Desired.X && Allocated.Y + 1.5 >= Desired.Y);
			}
			for (int32 First = 0; First < TextRects.Num(); ++First)
				for (int32 Second = First + 1; Second < TextRects.Num(); ++Second)
				{
					const FSlateRect& A = TextRects[First].Value;
					const FSlateRect& B = TextRects[Second].Value;
					const float Width = FMath::Min(A.Right, B.Right) - FMath::Max(A.Left, B.Left);
					const float Height = FMath::Min(A.Bottom, B.Bottom) - FMath::Max(A.Top, B.Top);
					Test.TestFalse(Context + TEXT(": distinct visible text rows do not overlap: ") + TextRects[First].Key->GetName() + TEXT(" / ") + TextRects[Second].Key->GetName(),
						Width > 1.f && Height > 1.f);
				}
			TArray<const TCHAR*> ControlNames;
			switch (Page)
			{
			case EFF_FacilityInfoPage::Overview: ControlNames = {TEXT("Text_InstallCost"), TEXT("Text_Footprint"), TEXT("Button_Move"), TEXT("Button_Remove")}; break;
			case EFF_FacilityInfoPage::Staff: ControlNames = {TEXT("V10_Text_StaffCount"), TEXT("Button_AssignStaff"), TEXT("Button_UnassignStaff")}; break;
			case EFF_FacilityInfoPage::Queue: ControlNames = {TEXT("Editable_FacilityQueueTeams"), TEXT("Editable_FacilityQueueSpacing"), TEXT("Editable_FacilityQueueWidth"), TEXT("Button_ApplyFacilityQueueSettings")}; break;
			case EFF_FacilityInfoPage::Revenue: ControlNames = {TEXT("Text_FacilityTodayRevenue"), TEXT("Text_FacilityTotalRevenue")}; break;
			case EFF_FacilityInfoPage::Upgrade: ControlNames = {TEXT("Text_UpgradeCurrentRevenue"), TEXT("Text_UpgradeNextRevenue"), TEXT("Button_Upgrade")}; break;
			case EFF_FacilityInfoPage::Programs: ControlNames = {TEXT("Text_ProgramStatus"), TEXT("Button_ScheduleProgram")}; break;
			default: break;
			}
			const FSlateRect BodyRect = Body->GetCachedGeometry().GetLayoutBoundingRect();
			for (const TCHAR* Name : ControlNames)
			{
				UWidget* Control = Panel->GetWidgetFromName(Name);
				if (!Test.TestNotNull(Context + TEXT(": bound control ") + Name, Control)) return false;
				Test.TestTrue(Context + TEXT(": control is immediately visible ") + Name, IsVisible(Control) && Contains(FrameRect, Control->GetCachedGeometry().GetLayoutBoundingRect()));
				// Upgrade's action is an intentional fixed footer outside the page body.
				if (Page != EFF_FacilityInfoPage::Overview && FCString::Strcmp(Name, TEXT("Button_Upgrade")) != 0)
					Test.TestTrue(Context + TEXT(": control remains inside its page body ") + Name, Contains(BodyRect, Control->GetCachedGeometry().GetLayoutBoundingRect()));
			}
			if (Page == EFF_FacilityInfoPage::Lighting)
			{
				auto* Lighting = Cast<UFF_FacilityLightingControls>(Panel->GetWidgetFromName(TEXT("Widget_LightingControls")));
				if (!Test.TestNotNull(Context + TEXT(": real nested lighting controls"), Lighting)) return false;
				const TArray<const TCHAR*> LightingNames = bPrecision
					? TArray<const TCHAR*>{TEXT("Input_LightingH"), TEXT("Input_LightingS"), TEXT("Input_LightingV"), TEXT("Input_LightingR"), TEXT("Input_LightingG"), TEXT("Input_LightingB"), TEXT("Button_LightingApply"), TEXT("Button_LightingCancel")}
					: TArray<const TCHAR*>{TEXT("ColorPicker_Lighting"), TEXT("Slider_LightingBrightness"), TEXT("Button_LightingToggle"), TEXT("Button_LightingReset")};
				for (const TCHAR* Name : LightingNames)
				{
					UWidget* Control = Lighting->GetWidgetFromName(Name);
					if (!Test.TestNotNull(Context + TEXT(": real lighting control ") + Name, Control)) return false;
					Test.TestTrue(Context + TEXT(": lighting control fits without scrolling ") + Name,
						IsVisible(Control) && Contains(BodyRect, Control->GetCachedGeometry().GetLayoutBoundingRect()));
				}
			}
			return true;
		}

		bool Capture(FAutomationTestBase& Test, const FString& Name)
		{
			TArray<FColor> Pixels;
			FReadSurfaceDataFlags Flags;
			Flags.SetLinearToGamma(false);
			if (!Test.TestTrue(TEXT("Compact inspector rendered pixels"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags))) return false;
			TArray64<uint8> Png;
			FImageUtils::PNGCompressImageArray(FMath::RoundToInt(Viewport.X), FMath::RoundToInt(Viewport.Y), Pixels, Png);
			const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/FacilityInfoCompact");
			IFileManager::Get().MakeDirectory(*Directory, true);
			const FString File = Directory / FString::Printf(TEXT("%s_%dx%d.png"), *Name, FMath::RoundToInt(Viewport.X), FMath::RoundToInt(Viewport.Y));
			return Test.TestTrue(TEXT("Compact inspector review capture saved"), FFileHelper::SaveArrayToFile(Png, *File));
		}
	};

	FFF_FacilityInfoViewData CommonData()
	{
		FFF_FacilityInfoViewData Data;
		Data.FacilityInstanceId = FGuid::NewGuid();
		Data.FacilityDefinitionId = TEXT("Booth_CompactRenderFixture");
		Data.Context = EFF_FacilityInfoContext::Build;
		Data.DisplayName = FText::FromString(TEXT("서핑 밸런스 체험장"));
		Data.CategoryText = FText::FromString(TEXT("체험 시설"));
		Data.FacilityStateText = FText::FromString(TEXT("설치 완료"));
		Data.InstallCostText = FText::FromString(TEXT("300 Gold"));
		Data.FootprintText = FText::FromString(TEXT("5.5 × 4.89 m"));
		Data.bCanMove = Data.bCanRemove = true;
		return Data;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityInfoCompactRenderTest,
	"FestaFiesta.UI.FacilityInfo.CompactBuildReadability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityInfoCompactRenderTest::RunTest(const FString& Parameters)
{
	using namespace FFFacilityCompactRenderTest;
	(void)Parameters;
	FIsolatedLightingPreference Preference;
	TStrongObjectPtr<UTexture2D> Thumbnail(UTexture2D::CreateTransient(64, 64));
	for (const FIntPoint Resolution : {FIntPoint(1280, 720), FIntPoint(1920, 1080)})
	{
		FFixture Fixture;
		if (!Fixture.Initialize(*this, Resolution)) return false;
		FFF_FacilityInfoViewData Data = CommonData();
		Data.Thumbnail = Thumbnail.Get();
		if (!Fixture.Show(*this, Data, EFF_FacilityInfoPage::Overview, TEXT("Overview")) || !Fixture.Capture(*this, TEXT("Overview"))) return false;
		Data.DisplayName = FText::FromString(TEXT("메인안내소"));
		Data.CategoryText = FText::FromString(TEXT("안내 시설"));
		Data.bShowStaffSection = true;
		Data.StaffCapacity = 3;
		Data.AssignedStaffCount = 1;
		Data.StaffSummaryText = FText::FromString(TEXT("스태프 배정 1 / 3명 · 출동 중 0명"));
		Data.StaffAssignmentHint = FText::FromString(TEXT("시설 설치는 정원만 늘립니다. 고용 후 배정하세요."));
		Data.bCanAssignStaff = Data.bCanUnassignStaff = true;
		if (!Fixture.Show(*this, Data, EFF_FacilityInfoPage::Staff, TEXT("Staff")) || !Fixture.Capture(*this, TEXT("Staff"))) return false;
		Data = CommonData();
		Data.Thumbnail = Thumbnail.Get();
		Data.bHasQueueSettings = Data.bCanEditQueueSettings = true;
		Data.QueueSettings.bEnabled = true;
		Data.QueueSettings.MaxWaitingTeams = 6;
		Data.QueueSettings.SlotSpacingCm = 115.f;
		Data.QueueSettings.LaneWidthCm = 300.f;
		Data.QueueSettingsHint = FText::FromString(TEXT("준비 단계에서 대기열 간격과 폭을 조정합니다."));
		if (!Fixture.Show(*this, Data, EFF_FacilityInfoPage::Queue, TEXT("Queue")) || !Fixture.Capture(*this, TEXT("Queue"))) return false;
		Data = CommonData();
		Data.Thumbnail = Thumbnail.Get();
        // Use the authoritative formatter: the row labels already identify
        // payments and share, so values must not repeat invented long labels.
        FFF_FacilityInstanceRevenue Revenue;
        Revenue.TodayGold = 12000;
        Revenue.TotalGold = 250000;
        Revenue.TodayPaymentCount = 30;
        Revenue.TotalPaymentCount = 425;
        FFFacilityInfoModel::ApplyFacilityRevenue(&Revenue, true, true, 96000, Data);
		if (!Fixture.Show(*this, Data, EFF_FacilityInfoPage::Revenue, TEXT("Revenue")) || !Fixture.Capture(*this, TEXT("Revenue"))) return false;
		Data = CommonData();
		Data.Thumbnail = Thumbnail.Get();
		Data.bHasFacilityLevels = Data.bShowUpgradeDetails = Data.bShowUpgradeAction = Data.bCanUpgrade = true;
		Data.FacilityLevelText = FText::FromString(TEXT("레벨 1"));
		Data.UpgradeActionText = FText::FromString(TEXT("강화"));
		Data.UpgradeCurrentValues.Revenue = FText::FromString(TEXT("300 Gold"));
		Data.UpgradeNextValues.Revenue = FText::FromString(TEXT("450 Gold"));
		Data.UpgradeCurrentValues.ServiceTime = FText::FromString(TEXT("30초"));
		Data.UpgradeNextValues.ServiceTime = FText::FromString(TEXT("24초"));
		Data.UpgradeCurrentValues.Throughput = FText::FromString(TEXT("분당 2팀"));
		Data.UpgradeNextValues.Throughput = FText::FromString(TEXT("분당 2.5팀"));
		Data.UpgradeCurrentValues.Footprint = FText::FromString(TEXT("5.5 × 4.89 m"));
		Data.UpgradeNextValues.Footprint = FText::FromString(TEXT("6.0 × 5.0 m"));
		Data.UpgradePriceText = FText::FromString(TEXT("500 Gold"));
		Data.UpgradeBalanceText = FText::FromString(TEXT("9,500 Gold"));
		Data.UpgradeBalanceLabel = FText::FromString(TEXT("강화 후 잔액"));
		if (!Fixture.Show(*this, Data, EFF_FacilityInfoPage::Upgrade, TEXT("Upgrade")) || !Fixture.Capture(*this, TEXT("Upgrade"))) return false;
		Data = CommonData();
		Data.Thumbnail = Thumbnail.Get();
		FFF_FacilityInfoSynergyEntry DetailedSynergy;
		DetailedSynergy.RuleId = TEXT("SurfPartnerFixture");
		DetailedSynergy.PartnerFacilityDefinitionId = TEXT("Rest_Seating_Seating");
		DetailedSynergy.TitleText = FText::FromString(TEXT("파도 뒤 달콤한 휴식"));
		DetailedSynergy.PartnerText = FText::FromString(TEXT("해변 족욕 쉼터"));
		DetailedSynergy.EffectText = FText::FromString(TEXT("설정 +8% · 시설 수익 +10%"));
		DetailedSynergy.ConditionText = FText::FromString(TEXT("12m 이내에 함께 설치"));
		DetailedSynergy.StatusText = FText::FromString(TEXT("조합 적용 중"));
		DetailedSynergy.bActive = true;
		Data.SynergyEntries.Add(DetailedSynergy);
		Data.SynergySummaryText = FText::FromString(TEXT("적용 중인 조합 1개"));
		if (!Fixture.Show(*this, Data, EFF_FacilityInfoPage::Synergy, TEXT("Synergy")) || !Fixture.Capture(*this, TEXT("Synergy"))) return false;
		Data = CommonData();
		Data.Thumbnail = Thumbnail.Get();
		Data.bShowProgramSection = Data.bHasScheduledProgram = Data.bShowProgramScheduleAction = Data.bCanScheduleProgram = true;
		Data.ProgramStatusText = FText::FromString(TEXT("프로그램 편성 완료"));
		Data.ScheduledProgramSummaryText = FText::FromString(TEXT("바다향기 공방 · 14:00–14:30 · 300 Gold"));
		Data.ProgramScheduleActionText = FText::FromString(TEXT("프로그램 변경"));
		if (!Fixture.Show(*this, Data, EFF_FacilityInfoPage::Programs, TEXT("Programs")) || !Fixture.Capture(*this, TEXT("Programs"))) return false;
		Data = CommonData();
		Data.DisplayName = FText::FromString(TEXT("축제 가로등"));
		Data.CategoryText = FText::FromString(TEXT("조명 시설"));
		Data.Thumbnail = Thumbnail.Get();
		Data.bHasLightingRig = Data.bCanEditLighting = true;
		Data.LightingState.bEnabled = true;
		Data.LightingState.BrightnessMultiplier = 1.f;
		if (!Fixture.Show(*this, Data, EFF_FacilityInfoPage::Lighting, TEXT("Lighting")) || !Fixture.Capture(*this, TEXT("Lighting"))) return false;
		// A stage can support lighting, programs, income, upgrades and combinations
		// together. A second navigation row must not push its color controls offscreen.
		Data.DisplayName = FText::FromString(TEXT("선셋 메인무대"));
		Data.CategoryText = FText::FromString(TEXT("공연 시설"));
		Data.bHasFacilityLevels = true;
		Data.bShowProgramSection = true;
		Data.bShowRevenueSection = true;
		Data.OperationStateText = FText::FromString(TEXT("운영 준비"));
		FFF_FacilityInfoSynergyEntry Synergy;
		Synergy.RuleId = TEXT("CompactRenderFixture");
		Synergy.TitleText = FText::FromString(TEXT("함께 즐기는 공연"));
		Data.SynergyEntries.Add(Synergy);
		if (!Fixture.Show(*this, Data, EFF_FacilityInfoPage::Lighting, TEXT("Stage lighting with dense navigation")) || !Fixture.Capture(*this, TEXT("StageLighting"))) return false;
		if (!Fixture.Show(*this, Data, EFF_FacilityInfoPage::Lighting, TEXT("Precision lighting with dense navigation"), true) || !Fixture.Capture(*this, TEXT("LightingPrecision"))) return false;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityInfoCompactColorDragTest,
	"FestaFiesta.UI.FacilityInfo.CompactColorWheelPointerDrag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityInfoCompactColorDragTest::RunTest(const FString& Parameters)
{
	using namespace FFFacilityCompactRenderTest;
	(void)Parameters;
	FIsolatedLightingPreference Preference;
	FFixture Fixture;
	if (!Fixture.Initialize(*this, FIntPoint(1920, 1080))) return false;
	FFF_FacilityInfoViewData Data = CommonData();
	Data.bHasLightingRig = Data.bCanEditLighting = true;
	Data.LightingState.bEnabled = true;
	Data.LightingState.BrightnessMultiplier = 1.f;
	if (!Fixture.Show(*this, Data, EFF_FacilityInfoPage::Lighting, TEXT("Pointer drag"))) return false;
	auto* Lighting = Cast<UFF_FacilityLightingControls>(Fixture.Panel->GetWidgetFromName(TEXT("Widget_LightingControls")));
	auto* Picker = Lighting ? Cast<UFF_FacilityColorPicker>(Lighting->GetWidgetFromName(TEXT("ColorPicker_Lighting"))) : nullptr;
	if (!TestNotNull(TEXT("Actual nested lighting controls"), Lighting) || !TestNotNull(TEXT("Actual native color wheel"), Picker)) return false;
	int32 Requests = 0;
	FGuid RequestedId;
	FLinearColor RequestedColor = FLinearColor::White;
	Fixture.Panel->OnLightingRequested.AddLambda([&](FGuid Id, const FFFZSFacilityLightingState& State)
	{
		++Requests;
		RequestedId = Id;
		RequestedColor = State.Color;
	});
	const FGeometry Geometry = Picker->GetCachedGeometry();
	const FVector2D Size = Geometry.GetLocalSize();
	TestTrue(TEXT("The actual draggable color wheel uses the requested one-third32px size"), Size.Equals(FVector2D(32.f), 1.f));
	const FVector2D Start = Geometry.LocalToAbsolute(Size * FVector2D(.65, .5));
	const FVector2D End = Geometry.LocalToAbsolute(Size * FVector2D(.45, .75));
	TArray<FWidgetAndPointer> Bubble = Fixture.HitGrid.GetBubblePath(Start, 0.f, false);
	FWidgetPath Path(MakeArrayView(Bubble));
	if (!TestTrue(TEXT("Pointer down lands on the real color wheel"), Path.IsValid() && Path.ContainsWidget(Picker->GetCachedWidget().Get()))) return false;
	TSet<FKey> Pressed;
	Pressed.Add(EKeys::LeftMouseButton);
	const TSet<FKey> Released;
	const FPointerEvent Down(0, Start, Start, Pressed, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
	const FPointerEvent Move(0, End, Start, Pressed, EKeys::Invalid, 0.f, FModifierKeysState());
	const FPointerEvent Up(0, End, End, Released, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
	FSlateApplication& App = FSlateApplication::Get();
	TestTrue(TEXT("Color wheel consumes the actual pointer down"), App.RoutePointerDownEvent(Path, Down).IsEventHandled());
	const FLinearColor DownColor = RequestedColor;
	TestTrue(TEXT("Native color-wheel interaction captures the pointer"), Picker->GetCachedWidget()->HasMouseCapture());
	App.RoutePointerMoveEvent(Path, Move, false);
	App.RoutePointerUpEvent(Path, Up);
	TestTrue(TEXT("A pointer drag submits multiple identity-scoped lighting previews"), Requests >= 2 && RequestedId == Data.FacilityInstanceId);
	TestTrue(TEXT("Moving across the wheel changes the selected color"), !RequestedColor.Equals(DownColor, .001f));
	TestFalse(TEXT("Pointer release ends the native color-wheel capture"), Picker->GetCachedWidget()->HasMouseCapture());
	Fixture.Paint();
	return true;
}

#endif
