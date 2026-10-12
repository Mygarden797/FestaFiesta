// 저장된 통합 스태프 WBP의 활성 관리 화면, 고정 고용 푸터와 목록 스크롤을 확인한다.
#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AssetCompilingManager.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
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
#include "UI/PrepHUD/Panels/FF_PrepPanelStaff.h"
#include "UI/PrepHUD/FF_PrepHudBar.h"
#include "UI/Staff/FF_StaffFacilityRowWidget.h"
#include "UI/Staff/FF_StaffManagementWidget.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Widgets/SOverlay.h"

namespace FFPrepStaffSkyLayoutTest
{
	constexpr float Tolerance = .5f;
	constexpr const TCHAR* PanelPath =
		TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelStaff.WBP_FF_PrepPanelStaff_C");
	constexpr const TCHAR* HudPath =
		TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar.WBP_FF_PrepHudBar_C");

	bool Contains(const FSlateRect& Outer, const FSlateRect& Inner)
	{
		return Inner.Left >= Outer.Left - Tolerance && Inner.Right <= Outer.Right + Tolerance &&
			Inner.Top >= Outer.Top - Tolerance && Inner.Bottom <= Outer.Bottom + Tolerance;
	}

	bool SetFixture(FAutomationTestBase& Test, UFF_PrepPanelStaff& Panel)
	{
		// GI 없는 검사 인스턴스의 표시만 채운다. 실제 고용 요청이나 경제 상태는 건드리지 않는다.
		const TPair<const TCHAR*, const TCHAR*> Values[] = {
			{ TEXT("Text_SelectedCount"), TEXT("1명") },
			{ TEXT("Text_HireUnitCost"), TEXT("1명 × 450") },
			{ TEXT("Text_HireCostTotal"), TEXT("450") },
			{ TEXT("Text_CurrentGold"), TEXT("9,700") },
			{ TEXT("Text_GoldAfterValue"), TEXT("9,250") },
			{ TEXT("Text_HireAction"), TEXT("1명 고용하기") }
		};
		for (const auto& Pair : Values)
		{
			UTextBlock* Text = Cast<UTextBlock>(Panel.GetWidgetFromName(Pair.Key));
			if (!Test.TestNotNull(FString(TEXT("표시 fixture / ")) + Pair.Key, Text)) { return false; }
			Text->SetText(FText::FromString(Pair.Value));
			Text->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		auto* Management = Cast<UFF_StaffManagementWidget>(Panel.GetWidgetFromName(TEXT("Widget_StaffManagement")));
		UWidget* Legacy = Panel.GetWidgetFromName(TEXT("Management_Legacy"));
		if (!Test.TestNotNull(TEXT("활성 통합 관리 위젯"), Management) ||
			!Test.TestNotNull(TEXT("보존된 이전 디자인"), Legacy)) { return false; }
		Test.TestEqual(TEXT("이전 디자인은 숨김 보존된다"), Legacy->GetVisibility(), ESlateVisibility::Collapsed);
		// 실제 준비 단계 표시/가시성 수명주기를 거친 뒤 표시 fixture만 채운다.
		// GI가 없는 월드이므로 게임 데이터 구독이나 변경 요청은 발생하지 않는다.
		Management->ActivateManagement(false);
		Management->DeactivateManagement();
		const TPair<const TCHAR*, const TCHAR*> ManagementValues[] = {
			{ TEXT("Text_Phase"), TEXT("셋업") },
			{ TEXT("Text_TotalStaff"), TEXT("6명") },
			{ TEXT("Text_StaffBreakdown"), TEXT("기본 6 · 고용 0") },
			{ TEXT("Text_SecondValue"), TEXT("6명") },
			{ TEXT("Text_ThirdValue"), TEXT("0명") },
			{ TEXT("Text_FourthValue"), TEXT("6명") },
			{ TEXT("Text_AssignmentGuide"), TEXT("미배정 인원 없음 · 빈자리 6명") }
		};
		for (const auto& Pair : ManagementValues)
		{
			auto* Text = Cast<UTextBlock>(Management->GetWidgetFromName(Pair.Key));
			if (!Test.TestNotNull(FString(TEXT("관리 fixture / ")) + Pair.Key, Text)) { return false; }
			Text->SetText(FText::FromString(Pair.Value));
		}
		UWidget* Incident = Management->GetWidgetFromName(TEXT("Group_Incident"));
		UWidget* Empty = Management->GetWidgetFromName(TEXT("Text_FacilitiesEmpty"));
		auto* Facilities = Cast<UVerticalBox>(Management->GetWidgetFromName(TEXT("Box_Facilities")));
		if (!Test.TestNotNull(TEXT("준비 단계 사건 영역"), Incident) ||
			!Test.TestNotNull(TEXT("빈 시설 안내"), Empty) || !Test.TestNotNull(TEXT("시설 목록"), Facilities) ||
			!Test.TestNotNull(TEXT("저장된 시설 행 클래스"), Management->FacilityRowClass.Get())) { return false; }
		Test.TestEqual(TEXT("준비 단계는 라이브 사건 영역을 숨긴다"), Incident->GetVisibility(), ESlateVisibility::Collapsed);
		Empty->SetVisibility(ESlateVisibility::Collapsed);
		Facilities->ClearChildren();
		// 실제 저장된 행 6개로 overflow를 만든다. 임의 크기의 빈 spacer로 통과시키지 않는다.
		for (int32 Index = 0; Index < 6; ++Index)
		{
			auto* Row = CreateWidget<UFF_StaffFacilityRowWidget>(Management, Management->FacilityRowClass);
			if (!Test.TestNotNull(TEXT("시설 표시 행"), Row)) { return false; }
			FFF_StaffFacilityManagementView View;
			View.FacilityId = FGuid(0, 0, 0, Index + 1);
			View.Label = FText::FromString(FString::Printf(TEXT("안내시설 %d"), Index + 1));
			View.Capacity = 2;
			View.Assigned = 1;
			View.bCanUnassign = true;
			Row->SetFacility(View, false);
			UVerticalBoxSlot* Slot = Facilities->AddChildToVerticalBox(Row);
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetPadding(FMargin(0.f, 0.f, 0.f, Management->FacilityRowSpacing));
		}
		return true;
	}

	bool SetFilterCaptions(FAutomationTestBase& Test, UFF_PrepPanelStaff& Panel, bool bUnassignedSelected)
	{
		auto* Management = Cast<UUserWidget>(Panel.GetWidgetFromName(TEXT("Widget_StaffManagement")));
		auto* Training = Management ? Cast<UUserWidget>(Management->GetWidgetFromName(TEXT("Widget_StaffTraining"))) : nullptr;
		if (!Test.TestNotNull(TEXT("필터 문구 fixture의 숙련도 목록"), Training)) { return false; }
		const TPair<const TCHAR*, const TCHAR*> Captions[] = {
			{ TEXT("Text_AllStaffFilter"), bUnassignedSelected ? TEXT("전체 12") : TEXT("✓ 전체 12") },
			{ TEXT("Text_UnassignedStaffFilter"), bUnassignedSelected ? TEXT("✓ 미배정 12") : TEXT("미배정 12") }
		};
		// 표시 전용 인스턴스에서 두 자리 인원과 양쪽 선택 상태의 실제 문구 형식을 확인한다.
		for (const auto& Caption : Captions)
		{
			auto* Text = Cast<UTextBlock>(Training->GetWidgetFromName(Caption.Key));
			if (!Test.TestNotNull(FString(TEXT("필터 문구 / ")) + Caption.Key, Text)) { return false; }
			Text->SetText(FText::FromString(Caption.Value));
		}
		return true;
	}

	void CheckText(FAutomationTestBase& Test, UUserWidget& Panel, const TCHAR* Name,
		const FSlateRect& Container, const FString& Context)
	{
		UTextBlock* Text = Cast<UTextBlock>(Panel.GetWidgetFromName(Name));
		if (!Test.TestNotNull(Context + TEXT(" / ") + Name, Text)) { return; }
		const FSlateRect Rect = Text->GetCachedGeometry().GetLayoutBoundingRect();
		const FVector2D Desired = Text->GetDesiredSize();
		const FString Label = Context + TEXT(" / ") + Name;
		Test.TestTrue(Label + TEXT(" 문구가 존재한다"), !Text->GetText().IsEmpty());
		Test.TestTrue(Label + TEXT(" 활성 글자가 실제 배치된다"), Rect.GetSize().X > 0.f && Rect.GetSize().Y > 0.f);
		Test.TestTrue(Label + TEXT(" 읽을 수 있는 글꼴 크기다"), Text->GetFont().Size >= 12.f);
		Test.TestTrue(Label + TEXT(" 부모 열/행의 할당 영역에 들어온다"), Contains(Container, Rect));
		Test.TestTrue(Label + TEXT(" 글자 높이를 확보한다"), Rect.GetSize().Y + Tolerance >= Desired.Y);
		Test.TestTrue(FString::Printf(TEXT("%s 글자 폭을 확보한다 (배치 %.1f / 필요 %.1f)"),
			*Label, Rect.GetSize().X, Desired.X), Rect.GetSize().X + Tolerance >= Desired.X);
	}

	bool CaptureAndCheck(FAutomationTestBase& Test, UFF_PrepPanelStaff& Panel,
		const TSharedRef<SWidget>& Surface, const FIntPoint Size, const FString& Variant)
	{
		const FString Context = FString::Printf(TEXT("%s %dx%d"), *Variant, Size.X, Size.Y);
		FWidgetRenderer Renderer(false, true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(
			FWidgetRenderer::CreateTargetFor(FVector2D(Size.X, Size.Y), TF_Bilinear, true));
		if (!Test.TestNotNull(Context + TEXT(" 렌더 타깃"), Target.Get())) { return false; }
		for (int32 Frame = 0; Frame < 3; ++Frame)
		{
			Panel.ForceLayoutPrepass();
			Renderer.DrawWidget(Target.Get(), Surface, FVector2D(Size.X, Size.Y), 1.f / 60.f, false);
		}
		FlushRenderingCommands();

		UWidget* Header = Panel.GetWidgetFromName(TEXT("HorizontalBox_Header"));
		UWidget* Close = Panel.GetWidgetFromName(TEXT("Button_Close"));
		UWidget* Footer = Panel.GetWidgetFromName(TEXT("Management_HireCard"));
		UWidget* HireButton = Panel.GetWidgetFromName(TEXT("Button_Hire"));
		auto* Management = Cast<UFF_StaffManagementWidget>(Panel.GetWidgetFromName(TEXT("Widget_StaffManagement")));
		if (!Test.TestNotNull(Context + TEXT(" 활성 관리 화면"), Management)) { return false; }
		UWidget* Current = Management->GetWidgetFromName(TEXT("VerticalBox_Facilities"));
		auto* Training = Cast<UUserWidget>(Management->GetWidgetFromName(TEXT("Widget_StaffTraining")));
		auto* Scroll = Cast<UScrollBox>(Management->GetWidgetFromName(TEXT("ScrollBox_Facilities")));
		auto* Facilities = Cast<UVerticalBox>(Management->GetWidgetFromName(TEXT("Box_Facilities")));
		if (!Test.TestNotNull(Context + TEXT(" 헤더"), Header) ||
			!Test.TestNotNull(Context + TEXT(" 닫기"), Close) ||
			!Test.TestNotNull(Context + TEXT(" 푸터"), Footer) ||
			!Test.TestNotNull(Context + TEXT(" 고용 버튼"), HireButton) ||
			!Test.TestNotNull(Context + TEXT(" 시설 배정 열"), Current) ||
			!Test.TestNotNull(Context + TEXT(" 직원 숙련도 열"), Training) ||
			!Test.TestNotNull(Context + TEXT(" 본문 시설 스크롤"), Scroll) ||
			!Test.TestNotNull(Context + TEXT(" 시설 표시 행 목록"), Facilities)) { return false; }

		const FSlateRect PanelRect(0.f, 0.f, static_cast<float>(Size.X), static_cast<float>(Size.Y));
		const FSlateRect HeaderRect = Header->GetCachedGeometry().GetLayoutBoundingRect();
		const FSlateRect FooterRect = Footer->GetCachedGeometry().GetLayoutBoundingRect();
		const FSlateRect BodyRect = Management->GetCachedGeometry().GetLayoutBoundingRect();
		const FSlateRect ScrollRect = Scroll->GetCachedGeometry().GetLayoutBoundingRect();
		const FSlateRect CurrentRect = Current->GetCachedGeometry().GetLayoutBoundingRect();
		const FSlateRect TrainingRect = Training->GetCachedGeometry().GetLayoutBoundingRect();
		Test.TestTrue(Context + TEXT(" 활성 헤더/본문/푸터가 실제 배치된다"),
			HeaderRect.GetSize().Y > 30.f && BodyRect.GetSize().Y > 250.f && FooterRect.GetSize().Y > 70.f);
		Test.TestTrue(Context + TEXT(" 헤더/푸터는 패널 안에 유지된다"),
			Contains(PanelRect, HeaderRect) && Contains(PanelRect, FooterRect));
		Test.TestTrue(Context + TEXT(" 닫기/고용 버튼은 패널 안에 유지된다"),
			Contains(HeaderRect, Close->GetCachedGeometry().GetLayoutBoundingRect()) &&
			Contains(FooterRect, HireButton->GetCachedGeometry().GetLayoutBoundingRect()));
		Test.TestTrue(Context + TEXT(" 활성 본문이 헤더/고용 푸터를 침범하지 않는다"),
			Contains(PanelRect, BodyRect) && BodyRect.Top >= HeaderRect.Bottom - Tolerance &&
			BodyRect.Bottom <= FooterRect.Top + Tolerance);
		Test.TestTrue(Context + TEXT(" 시설 배정/숙련도 두 열은 가로로 겹치지 않는다"),
			CurrentRect.Right <= TrainingRect.Left + Tolerance &&
			Contains(BodyRect, CurrentRect) && Contains(BodyRect, TrainingRect));
		Test.TestTrue(Context + TEXT(" 두 열 모두 읽을 수 있는 폭이 있다"),
			CurrentRect.GetSize().X > 400.f && TrainingRect.GetSize().X > 500.f);
		Test.TestTrue(Context + TEXT(" 시설 스크롤이 해당 열 안에 배치된다"),
			Contains(CurrentRect, ScrollRect) && ScrollRect.GetSize().Y > 100.f);
		CheckText(Test, Panel, TEXT("Text_PanelTitle"), HeaderRect, Context);
		CheckText(Test, Panel, TEXT("Text_HireAction"), HireButton->GetCachedGeometry().GetLayoutBoundingRect(), Context);
		CheckText(Test, Panel, TEXT("Text_DailyNotice"), FooterRect, Context);
		for (const TCHAR* Name : { TEXT("Text_TotalStaff"), TEXT("Text_StaffBreakdown"),
			TEXT("Text_SecondValue"), TEXT("Text_ThirdValue"), TEXT("Text_FourthValue"), TEXT("Text_ManagementNote") })
		{
			CheckText(Test, *Management, Name, BodyRect, Context);
		}
		for (const TCHAR* Name : { TEXT("Text_SelectedCount"), TEXT("Text_HireUnitCost"),
			TEXT("Text_HireCostTotal"), TEXT("Text_CurrentGold"), TEXT("Text_GoldAfterValue") })
		{
			CheckText(Test, Panel, Name, FooterRect, Context);
		}
		for (const TCHAR* Name : { TEXT("Text_TrainingTitle"), TEXT("Text_AllStaffFilter"), TEXT("Text_UnassignedStaffFilter") })
		{
			CheckText(Test, *Training, Name, TrainingRect, Context);
		}
		for (const TCHAR* Name : { TEXT("Button_Hire"), TEXT("Button_Decrease"), TEXT("Button_Increase") })
		{
			const UWidget* Button = Panel.GetWidgetFromName(Name);
			if (!Test.TestNotNull(Context + TEXT(" / ") + Name, Button)) { return false; }
			const FSlateRect Rect = Button->GetCachedGeometry().GetLayoutBoundingRect();
			Test.TestTrue(Context + TEXT(" 고용 조작 영역은 푸터 안에서 최소 34px를 확보한다 / ") + Name,
				Contains(FooterRect, Rect) && Rect.GetSize().X >= 34.f && Rect.GetSize().Y >= 34.f);
		}
		// 숨겨진 구형 ScrollBox_Body가 아니라 실제 시설 목록의 끝까지 접근할 수 있어야 한다.
		Test.TestTrue(Context + TEXT(" 시설 행 fixture가 실제 스크롤을 만든다"), Scroll->GetScrollOffsetOfEnd() > Tolerance);
		Scroll->SetScrollOffset(Scroll->GetScrollOffsetOfEnd());
		for (int32 Frame = 0; Frame < 4; ++Frame)
			Renderer.DrawWidget(Target.Get(), Surface, FVector2D(Size.X, Size.Y), 1.f / 60.f, false);
		auto* LastRow = Cast<UUserWidget>(Facilities->GetChildAt(Facilities->GetChildrenCount() - 1));
		if (!Test.TestNotNull(Context + TEXT(" 마지막 시설 행"), LastRow)) { return false; }
		const FSlateRect EndRect = Scroll->GetCachedGeometry().GetLayoutBoundingRect();
		CheckText(Test, *LastRow, TEXT("Text_FacilityName"), EndRect, Context);
		CheckText(Test, *LastRow, TEXT("Text_AssignedCapacity"), EndRect, Context);
		UWidget* LastAction = LastRow->GetWidgetFromName(TEXT("Button_Locate"));
		Test.TestTrue(Context + TEXT(" 스크롤 끝에서 마지막 시설 동작에 접근한다"),
			LastAction && Contains(EndRect, LastAction->GetCachedGeometry().GetLayoutBoundingRect()));
		Scroll->SetScrollOffset(0.f);
		for (int32 Frame = 0; Frame < 4; ++Frame)
			Renderer.DrawWidget(Target.Get(), Surface, FVector2D(Size.X, Size.Y), 1.f / 60.f, false);
		FlushRenderingCommands();

		TArray<FColor> Pixels;
		FReadSurfaceDataFlags Flags;
		Flags.SetLinearToGamma(false);
		if (!Test.TestTrue(Context + TEXT(" 렌더 픽셀을 읽는다"),
			Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags) && Pixels.Num() == Size.X * Size.Y))
		{
			return false;
		}
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PrepStaffSky/Layout"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString Path = FPaths::Combine(Directory,
			FString::Printf(TEXT("Staff_%s_%dx%d.png"), *Variant, Size.X, Size.Y));
		const bool bSaved = FFileHelper::SaveArrayToFile(Png, *Path);
		Test.TestTrue(Context + TEXT(" PNG를 저장한다"), bSaved);
		Test.AddInfo(FString::Printf(TEXT("인력 패널 캡처: %s / 본문 %.1f, 스크롤 끝 %.1f"),
			*FPaths::ConvertRelativePathToFull(Path), ScrollRect.GetSize().Y, Scroll->GetScrollOffsetOfEnd()));
		return bSaved;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepStaffSkyLayoutTest,
	"FestaFiesta.UI.PrepStaff.SkyRenderedLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)

bool FFF_PrepStaffSkyLayoutTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFPrepStaffSkyLayoutTest;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	auto* PanelClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepPanelStaff>(nullptr, PanelPath));
	if (!TestNotNull(TEXT("검사용 에디터 월드"), World) ||
		!TestNotNull(TEXT("저장된 인력 패널 클래스"), PanelClass)) { return false; }
	if (!TestNull(TEXT("표시 fixture는 게임 인스턴스 없는 월드에서만 쓴다"), World->GetGameInstance())) { return false; }
	UClass* HudClass = LoadClass<UFF_PrepHudBar>(nullptr, HudPath);
	if (!TestNotNull(TEXT("저장된 준비 HUD 클래스"), HudClass)) { return false; }
	const UObject* HudDefaults = HudClass->GetDefaultObject();
	const FMapProperty* LayoutProperty = FindFProperty<FMapProperty>(HudClass, TEXT("PanelLayouts"));
	if (!TestNotNull(TEXT("HUD 패널 크기 설정"), LayoutProperty)) { return false; }
	const auto* Layouts = LayoutProperty->ContainerPtrToValuePtr<TMap<EFF_PrepHudTab, FFF_PrepPanelLayout>>(HudDefaults);
	const FFF_PrepPanelLayout* StaffLayout = Layouts->Find(EFF_PrepHudTab::Staff);
	if (!TestNotNull(TEXT("스태프 HUD 크기"), StaffLayout)) { return false; }
	// 통합 관리 디자인의 저장된 기본 크기와 렌더 fixture가 따로 낡지 않게 확인한다.
	if (!TestEqual(TEXT("통합 스태프 HUD 기본 크기"), StaffLayout->Size, FVector2D(1300, 800))) { return false; }
	const FIntPoint SavedSize(FMath::RoundToInt(StaffLayout->Size.X), FMath::RoundToInt(StaffLayout->Size.Y));
	const bool bWasDirty = PanelClass->GetOutermost()->IsDirty();
	const bool bPreviousInitialize = PanelClass->bCanCallInitializedWithoutPlayerContext;
	PanelClass->bCanCallInitializedWithoutPlayerContext = true;
	ON_SCOPE_EXIT { PanelClass->bCanCallInitializedWithoutPlayerContext = bPreviousInitialize; };
	TStrongObjectPtr<UFF_PrepPanelStaff> Panel(CreateWidget<UFF_PrepPanelStaff>(World, PanelClass));
	if (!TestNotNull(TEXT("인력 패널 인스턴스"), Panel.Get())) { return false; }
	const TSharedRef<SOverlay> Surface = SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)[Panel->TakeWidget()];
	if (!SetFixture(*this, *Panel)) { return false; }
	FAssetCompilingManager::Get().FinishAllCompilation();
	FlushRenderingCommands();
	for (const FIntPoint Size : { SavedSize, FIntPoint(SavedSize.X, 700) })
	{
		if (!CaptureAndCheck(*this, *Panel, Surface, Size, TEXT("Saved"))) { return false; }
	}
	for (const bool bUnassignedSelected : { false, true })
	{
		if (!SetFilterCaptions(*this, *Panel, bUnassignedSelected) ||
			!CaptureAndCheck(*this, *Panel, Surface, FIntPoint(SavedSize.X, 700),
				bUnassignedSelected ? TEXT("Unassigned12") : TEXT("AllStaff12"))) { return false; }
	}

	// Designer와 같은 속성을 새 인스턴스에 넣고 NativePreConstruct/Construct를 거쳐도 유지되는지 본다.
	TStrongObjectPtr<UFF_PrepPanelStaff> Edited(CreateWidget<UFF_PrepPanelStaff>(World, PanelClass));
	if (!TestNotNull(TEXT("편집 가능성 검사 인스턴스"), Edited.Get())) { return false; }
	UTextBlock* Capacity = Cast<UTextBlock>(Edited->GetWidgetFromName(TEXT("Text_HireCostTotal")));
	UButton* Hire = Cast<UButton>(Edited->GetWidgetFromName(TEXT("Button_Hire")));
	if (!TestNotNull(TEXT("편집할 고용 비용 글꼴"), Capacity) || !TestNotNull(TEXT("편집할 버튼 테두리"), Hire)) { return false; }
	FSlateFontInfo Font = Capacity->GetFont();
	Font.Size += 2;
	Capacity->SetFont(Font);
	FButtonStyle Style = Hire->GetStyle();
	Style.Normal.OutlineSettings.Width = 3.f;
	Style.Normal.OutlineSettings.Color = FLinearColor(.15f, .4f, .8f);
	Hire->SetStyle(Style);
	const TSharedRef<SOverlay> EditedSurface = SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)[Edited->TakeWidget()];
	if (!SetFixture(*this, *Edited) ||
		!CaptureAndCheck(*this, *Edited, EditedSurface, SavedSize, TEXT("EditedStyle"))) { return false; }
	TestEqual(TEXT("사용자가 바꾼 글꼴 크기를 생명주기 함수가 덮어쓰지 않는다"), Capacity->GetFont().Size, Font.Size);
	TestEqual(TEXT("사용자가 바꾼 테두리 두께를 생명주기 함수가 덮어쓰지 않는다"),
		Hire->GetStyle().Normal.OutlineSettings.Width, Style.Normal.OutlineSettings.Width);
	TestTrue(TEXT("사용자가 바꾼 테두리 색을 생명주기 함수가 덮어쓰지 않는다"),
		Hire->GetStyle().Normal.OutlineSettings.Color.GetSpecifiedColor().Equals(Style.Normal.OutlineSettings.Color.GetSpecifiedColor()));
	TestEqual(TEXT("렌더 검사가 저장된 WBP 패키지를 변경하지 않는다"), PanelClass->GetOutermost()->IsDirty(), bWasDirty);
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
