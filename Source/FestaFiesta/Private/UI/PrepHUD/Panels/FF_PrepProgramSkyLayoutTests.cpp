// 저장된 프로그램 WBP와 카드로 두 열, 고정 푸터, 카탈로그 증가 시 스크롤을 확인한다.
#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AssetCompilingManager.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
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
#include "UI/PrepHUD/Panels/FF_PrepPanelProgram.h"
#include "UI/PrepHUD/Panels/FF_PrepProgramCard.h"
#include "UI/PrepHUD/Panels/FF_PrepProgramFacilityRow.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SOverlay.h"

namespace FFPrepProgramSkyLayoutTest
{
	constexpr float Tolerance = .5f;
	constexpr const TCHAR* PanelPath =
		TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelProgram.WBP_FF_PrepPanelProgram_C");
	constexpr const TCHAR* CardPath =
		TEXT("/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepProgramCard.WBP_FF_PrepProgramCard_C");
	constexpr const TCHAR* FacilityRowPath =
		TEXT("/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepProgramFacilityRow.WBP_FF_PrepProgramFacilityRow_C");

	bool Contains(const FSlateRect& Outer, const FSlateRect& Inner)
	{
		return Inner.Left >= Outer.Left - Tolerance && Inner.Right <= Outer.Right + Tolerance &&
			Inner.Top >= Outer.Top - Tolerance && Inner.Bottom <= Outer.Bottom + Tolerance;
	}

	void CheckText(FAutomationTestBase& Test, UUserWidget& Widget, const TCHAR* Name,
		const FSlateRect& Container, const FString& Context)
	{
		UTextBlock* Text = Cast<UTextBlock>(Widget.GetWidgetFromName(Name));
		const FString Label = Context + TEXT(" / ") + Name;
		if (!Test.TestNotNull(Label, Text)) { return; }
		const FSlateRect Rect = Text->GetCachedGeometry().GetLayoutBoundingRect();
		const FVector2D Desired = Text->GetDesiredSize();
		Test.TestTrue(Label + TEXT(" 내용이 존재한다"), !Text->GetText().IsEmpty());
		Test.TestTrue(Label + TEXT(" 할당 영역 안에 들어온다"), Contains(Container, Rect));
		Test.TestTrue(Label + TEXT(" 글자 높이를 확보한다"), Rect.GetSize().Y + Tolerance >= Desired.Y);
		Test.TestTrue(Label + TEXT(" 글자 폭을 확보한다"), Rect.GetSize().X + Tolerance >= Desired.X);
	}

	bool SetFixture(FAutomationTestBase& Test, UFF_PrepPanelProgram& Panel,
		UWidgetBlueprintGeneratedClass* CardClass, UWidgetBlueprintGeneratedClass* FacilityRowClass,
		int32 Count, bool bLongTitles)
	{
		// GI 없는 렌더 검사 인스턴스만 채운다. 카탈로그 에셋, 시설, 편성 또는 경제 상태는 수정하지 않는다.
		const TPair<const TCHAR*, const TCHAR*> Values[] = {
			{ TEXT("Text_Day"), TEXT("1일차") },
			{ TEXT("Text_SelectedProgram"), TEXT("바다향기 공방") },
			{ TEXT("Text_CostPreview"), TEXT("200 Gold") },
			{ TEXT("Text_GoldAfter"), TEXT("편성 후 잔액 9,500") },
			{ TEXT("Text_RevenuePreview"), TEXT("선택 시설 최대 매출 360 Gold") }
		};
		for (const auto& Pair : Values)
		{
			UTextBlock* Text = Cast<UTextBlock>(Panel.GetWidgetFromName(Pair.Key));
			if (!Test.TestNotNull(FString(TEXT("표시 fixture / ")) + Pair.Key, Text)) { return false; }
			Text->SetText(FText::FromString(Pair.Value));
			Text->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		for (const TCHAR* Name : { TEXT("Text_EmptyCatalog"), TEXT("Text_FacilityNotice"), TEXT("Text_FacilityEmpty"), TEXT("Box_CatalogChoices") })
		{
			if (UWidget* Widget = Panel.GetWidgetFromName(Name)) { Widget->SetVisibility(ESlateVisibility::Collapsed); }
		}
		if (UWidget* Block = Panel.GetWidgetFromName(TEXT("Text_BlockReason"))) { Block->SetVisibility(ESlateVisibility::Hidden); }
		UWidget* FacilityRow = Panel.GetWidgetFromName(TEXT("Box_FacilityRow"));
		UComboBoxString* Combo = Cast<UComboBoxString>(Panel.GetWidgetFromName(TEXT("Combo_Facilities")));
		UVerticalBox* Facilities = Cast<UVerticalBox>(Panel.GetWidgetFromName(TEXT("VerticalBox_FacilityInstances")));
		UWrapBox* Cards = Cast<UWrapBox>(Panel.GetWidgetFromName(TEXT("Box_ProgramCards")));
		UButton* Schedule = Cast<UButton>(Panel.GetWidgetFromName(TEXT("Button_Schedule")));
		if (!Test.TestNotNull(TEXT("시설 선택 줄"), FacilityRow) || !Test.TestNotNull(TEXT("시설 드롭다운"), Combo) ||
			!Test.TestNotNull(TEXT("현재 설치 시설 목록"), Facilities) ||
			!Test.TestNotNull(TEXT("프로그램 카드 목록"), Cards) || !Test.TestNotNull(TEXT("편성 버튼"), Schedule)) { return false; }
		FacilityRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		// 시설 목록 디자인은 드롭다운을 호환 바인딩용으로만 남기고 숨긴다.
		// 숨겨진 컨트롤을 강제로 보이게 하지 않고 실제 저장된 시설 행을 렌더한다.
		Test.TestEqual(TEXT("호환용 시설 드롭다운은 숨김 상태를 유지한다"), Combo->GetVisibility(), ESlateVisibility::Collapsed);
		Facilities->ClearChildren();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			UFF_PrepProgramFacilityRow* Row = CreateWidget<UFF_PrepProgramFacilityRow>(&Panel, FacilityRowClass);
			if (!Test.TestNotNull(TEXT("시설 행 검사 인스턴스"), Row)) { return false; }
			FFF_PrepFacilityOption Data;
			Data.InstanceLabel = FText::FromString(FString::Printf(TEXT("바다 공방 #%02d"), Index + 1));
			Data.LocationText = FText::FromString(Index % 2 == 0 ? TEXT("해변 구역") : TEXT("중앙 광장"));
			Row->ConfigureFacility(Index, Data);
			Row->SetFacilityState(Index == 0, false);
			UVerticalBoxSlot* RowSlot = Facilities->AddChildToVerticalBox(Row);
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
			RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}
		Schedule->SetIsEnabled(true);
		Cards->ClearChildren();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			UFF_PrepProgramCard* Card = CreateWidget<UFF_PrepProgramCard>(&Panel, CardClass);
			if (!Test.TestNotNull(TEXT("카드 검사 인스턴스"), Card)) { return false; }
			FFF_ProgramCard Data;
			Data.ProgramId = FName(*FString::Printf(TEXT("LayoutFixture_%d"), Index));
			Data.DisplayName = FText::FromString(bLongTitles
				? TEXT("해 질 녘 바다에서 함께하는 가족 공예 체험")
				: (Index == 0 ? TEXT("바다향기 공방") : TEXT("선셋 요가")));
			Data.CostGold = Index == 0 ? 200 : 250;
			Data.Capacity = Index == 0 ? 12 : 16;
			Data.FeePerPerson = 30;
			Card->ConfigureCard(Index, Data, FText::FromString(TEXT("18:30 ~ 19:00")));
			Card->SetRevenuePreview(Index == 0 ? &Data : nullptr);
			if (Index == 0)
			{
				UButton* Root = Cast<UButton>(Card->GetWidgetFromName(TEXT("Button_Root")));
				if (!Test.TestNotNull(TEXT("선택 카드 버튼"), Root)) { return false; }
				const FButtonStyle Style = Root->GetStyle();
				Test.TestTrue(TEXT("선택 테두리는 기본 테두리보다 두껍다"),
					Style.Pressed.OutlineSettings.Width > Style.Normal.OutlineSettings.Width);
				Card->ApplyCardState(EFF_PrepCardState::Selected, FText::FromString(TEXT("✓ 선택됨")));
				Test.TestEqual(TEXT("선택 브러시가 실제 버튼에 적용된다"),
					Root->GetStyle().Normal.OutlineSettings.Width, Style.Pressed.OutlineSettings.Width);
			}
			else
			{
				Card->ApplyCardState(EFF_PrepCardState::Selectable, FText::GetEmpty());
			}
			Cards->AddChildToWrapBox(Card);
		}
		return true;
	}

	bool CaptureAndCheck(FAutomationTestBase& Test, UFF_PrepPanelProgram& Panel,
		const TSharedRef<SWidget>& Surface, FIntPoint Size, const FString& Variant, bool bOverflow)
	{
		const FString Context = FString::Printf(TEXT("%s %dx%d"), *Variant, Size.X, Size.Y);
		FWidgetRenderer Renderer(false, true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(
			FWidgetRenderer::CreateTargetFor(FVector2D(Size.X, Size.Y), TF_Bilinear, true));
		if (!Test.TestNotNull(Context + TEXT(" 렌더 타깃"), Target.Get())) { return false; }
		auto DrawFrames = [&]()
		{
			for (int32 Frame = 0; Frame < 4; ++Frame)
			{
				Panel.ForceLayoutPrepass();
				Renderer.DrawWidget(Target.Get(), Surface, FVector2D(Size.X, Size.Y), 1.f / 60.f, false);
			}
			FlushRenderingCommands();
		};
		UScrollBox* Scroll = Cast<UScrollBox>(Panel.GetWidgetFromName(TEXT("ScrollBox_Body")));
		UScrollBox* FacilityScroll = Cast<UScrollBox>(Panel.GetWidgetFromName(TEXT("ScrollBox_FacilityInstances")));
		if (!Test.TestNotNull(Context + TEXT(" 본문 스크롤"), Scroll) ||
			!Test.TestNotNull(Context + TEXT(" 시설 목록 스크롤"), FacilityScroll)) { return false; }
		Scroll->SetScrollOffset(0.f);
		FacilityScroll->SetScrollOffset(0.f);
		DrawFrames();
		UWidget* Header = Panel.GetWidgetFromName(TEXT("HorizontalBox_Header"));
		UWidget* Close = Panel.GetWidgetFromName(TEXT("Button_Close"));
		UWidget* Footer = Panel.GetWidgetFromName(TEXT("Border_Footer"));
		UWidget* Schedule = Panel.GetWidgetFromName(TEXT("Button_Schedule"));
		UWidget* Catalog = Panel.GetWidgetFromName(TEXT("VerticalBox_Catalog"));
		UWidget* Summary = Panel.GetWidgetFromName(TEXT("Border_Summary"));
		UVerticalBox* Facilities = Cast<UVerticalBox>(Panel.GetWidgetFromName(TEXT("VerticalBox_FacilityInstances")));
		UWrapBox* Cards = Cast<UWrapBox>(Panel.GetWidgetFromName(TEXT("Box_ProgramCards")));
		if (!Test.TestNotNull(Context + TEXT(" 헤더"), Header) || !Test.TestNotNull(Context + TEXT(" 닫기"), Close) ||
			!Test.TestNotNull(Context + TEXT(" 푸터"), Footer) || !Test.TestNotNull(Context + TEXT(" 편성"), Schedule) ||
			!Test.TestNotNull(Context + TEXT(" 왼쪽 목록"), Catalog) || !Test.TestNotNull(Context + TEXT(" 오른쪽 요약"), Summary) ||
			!Test.TestNotNull(Context + TEXT(" 시설 선택"), Facilities) || !Test.TestNotNull(Context + TEXT(" 카드"), Cards)) { return false; }
		const FSlateRect PanelRect(0.f, 0.f, static_cast<float>(Size.X), static_cast<float>(Size.Y));
		const FSlateRect HeaderRect = Header->GetCachedGeometry().GetLayoutBoundingRect();
		const FSlateRect FooterRect = Footer->GetCachedGeometry().GetLayoutBoundingRect();
		const FSlateRect ScrollRect = Scroll->GetCachedGeometry().GetLayoutBoundingRect();
		const FSlateRect CatalogRect = Catalog->GetCachedGeometry().GetLayoutBoundingRect();
		const FSlateRect SummaryRect = Summary->GetCachedGeometry().GetLayoutBoundingRect();
		Test.TestTrue(Context + TEXT(" 헤더와 푸터가 패널 안에 유지된다"),
			Contains(PanelRect, HeaderRect) && Contains(PanelRect, FooterRect));
		Test.TestTrue(Context + TEXT(" 닫기와 편성 버튼이 고정 영역 안에 유지된다"),
			Contains(HeaderRect, Close->GetCachedGeometry().GetLayoutBoundingRect()) &&
			Contains(FooterRect, Schedule->GetCachedGeometry().GetLayoutBoundingRect()));
		Test.TestTrue(Context + TEXT(" 본문이 헤더와 푸터를 침범하지 않는다"),
			Contains(PanelRect, ScrollRect) && ScrollRect.Top >= HeaderRect.Bottom - Tolerance &&
			ScrollRect.Bottom <= FooterRect.Top + Tolerance && ScrollRect.GetSize().Y > 0.f);
		Test.TestTrue(Context + TEXT(" 목록과 요약 두 열이 겹치지 않는다"),
			CatalogRect.Right <= SummaryRect.Left + Tolerance && CatalogRect.Left >= ScrollRect.Left - Tolerance &&
			SummaryRect.Right <= ScrollRect.Right + Tolerance && CatalogRect.GetSize().X > 400.f && SummaryRect.GetSize().X > 250.f);
		CheckText(Test, Panel, TEXT("Text_PanelTitle"), HeaderRect, Context);
		CheckText(Test, Panel, TEXT("Text_Day"), PanelRect, Context);
		CheckText(Test, Panel, TEXT("Text_ScheduleAction"), Schedule->GetCachedGeometry().GetLayoutBoundingRect(), Context);
		CheckText(Test, Panel, TEXT("Text_DailyNotice"), FooterRect, Context);
		if (!bOverflow && Size.Y >= 600)
		{
			for (UWidget* Card : Cards->GetAllChildren())
			{
				Test.TestTrue(Context + TEXT(" 기본 두 카드가 본문 안에 모두 보인다"),
					Contains(ScrollRect, Card->GetCachedGeometry().GetLayoutBoundingRect()));
			}
		}
		// Slate는 화면 밖 자손을 그리지 않아 CachedGeometry가 이전 프레임이거나 비어 있을 수 있다.
		// 검사 기준을 완화하지 않고 실제 스크롤한 뒤 보이는 자손의 좌표를 측정한다.
		auto RevealAndCheckText = [&](UUserWidget& Owner, const TCHAR* Name, UWidget* Container, const FString& Label)
		{
			UWidget* Text = Owner.GetWidgetFromName(Name);
			if (!Test.TestNotNull(Label + TEXT(" / ") + Name, Text)) { return; }
			// 처음 보이는 WrapBox 행의 자동 줄바꿈은 페인트 뒤 DesiredSize를 갱신할 수 있다.
			// 다음 레이아웃의 좌표로 재요청하되 세 번 안에도 안 보이면 원래 기준대로 실패시킨다.
			for (int32 Attempt = 0; Attempt < 3; ++Attempt)
			{
				Scroll->ScrollWidgetIntoView(Text, false, EDescendantScrollDestination::IntoView, 2.f);
				DrawFrames();
				const FSlateRect ViewRect = Scroll->GetCachedGeometry().GetLayoutBoundingRect();
				const FSlateRect TextRect = Text->GetCachedGeometry().GetLayoutBoundingRect();
				const bool bVisible = Contains(ViewRect, TextRect);
				if (!bVisible || Attempt > 0)
				{
					Test.AddInfo(FString::Printf(
						TEXT("%s / %s 재배치 %d: 보임=%d 본문=(%.1f,%.1f)-(%.1f,%.1f), 글자=(%.1f,%.1f)-(%.1f,%.1f), 스크롤=%.1f"),
						*Label, Name, Attempt + 1, bVisible ? 1 : 0,
						ViewRect.Left, ViewRect.Top, ViewRect.Right, ViewRect.Bottom,
						TextRect.Left, TextRect.Top, TextRect.Right, TextRect.Bottom, Scroll->GetScrollOffset()));
				}
				if (bVisible) { break; }
			}
			Test.TestTrue(Label + TEXT(" / ") + Name + TEXT(" 스크롤하면 본문 안에 보인다"),
				Contains(Scroll->GetCachedGeometry().GetLayoutBoundingRect(), Text->GetCachedGeometry().GetLayoutBoundingRect()));
			CheckText(Test, Owner, Name, Container->GetCachedGeometry().GetLayoutBoundingRect(), Label);
		};
		for (const TCHAR* Name : { TEXT("Text_SelectedProgram"), TEXT("Text_CostPreview"), TEXT("Text_GoldAfter"), TEXT("Text_RevenuePreview") })
		{
			RevealAndCheckText(Panel, Name, Summary, Context);
		}
		// 현재 시설 선택은 최대 높이가 있는 중첩 목록이다. 본문에 목록을 드러낸 뒤
		// 각 행을 내부 스크롤로 찾아 두 클리핑 영역 모두에서 버튼과 글자가 보이는지 확인한다.
		Scroll->ScrollWidgetIntoView(FacilityScroll, false, EDescendantScrollDestination::IntoView, 2.f);
		DrawFrames();
		Test.TestTrue(Context + TEXT(" 실제 선택할 시설 행이 존재한다"), Facilities->GetChildrenCount() > 0);
		for (int32 Index = 0; Index < Facilities->GetChildrenCount(); ++Index)
		{
			UFF_PrepProgramFacilityRow* Row = Cast<UFF_PrepProgramFacilityRow>(Facilities->GetChildAt(Index));
			if (!Test.TestNotNull(Context + TEXT(" 렌더된 시설 행"), Row)) { return false; }
			FacilityScroll->ScrollWidgetIntoView(Row, false, EDescendantScrollDestination::IntoView, 2.f);
			DrawFrames();
			const FString Label = Context + FString::Printf(TEXT(" 시설 %d"), Index);
			const FSlateRect RowRect = Row->GetCachedGeometry().GetLayoutBoundingRect();
			Test.TestTrue(Label + TEXT(" 본문과 시설 목록 안에 전체가 보인다"),
				RowRect.GetSize().X > 0.f && RowRect.GetSize().Y > 0.f &&
				Contains(Scroll->GetCachedGeometry().GetLayoutBoundingRect(), RowRect) &&
				Contains(FacilityScroll->GetCachedGeometry().GetLayoutBoundingRect(), RowRect));
			for (const TCHAR* Name : { TEXT("Button_Select"), TEXT("Button_Locate") })
			{
				UWidget* Button = Row->GetWidgetFromName(Name);
				if (!Test.TestNotNull(Label + TEXT(" / ") + Name, Button)) { return false; }
				const FSlateRect ButtonRect = Button->GetCachedGeometry().GetLayoutBoundingRect();
				Test.TestTrue(Label + TEXT(" / ") + Name + TEXT(" 조작 영역이 행 안에 보인다"),
					Button->IsVisible() && Button->GetIsEnabled() && Contains(RowRect, ButtonRect) &&
					ButtonRect.GetSize().X > 0.f && ButtonRect.GetSize().Y > 0.f);
			}
			for (const TCHAR* Name : { TEXT("Text_FacilityName"), TEXT("Text_Location"), TEXT("Text_Locate") })
			{
				CheckText(Test, *Row, Name, RowRect, Label);
			}
		}
		for (int32 Index = 0; Index < Cards->GetChildrenCount(); ++Index)
		{
			UFF_PrepProgramCard* Card = Cast<UFF_PrepProgramCard>(Cards->GetChildAt(Index));
			if (!Test.TestNotNull(Context + TEXT(" 렌더된 카드"), Card)) { return false; }
			for (const TCHAR* Name : { TEXT("Text_Label"), TEXT("Text_TimeRange"), TEXT("Text_Cost"), TEXT("Text_Capacity") })
			{
				RevealAndCheckText(*Card, Name, Card, Context + FString::Printf(TEXT(" 카드 %d"), Index));
			}
			if (Index == 0) { RevealAndCheckText(*Card, TEXT("Text_Badge"), Card, Context); }
		}
		if (bOverflow)
		{
			Test.TestTrue(Context + TEXT(" 늘어난 카탈로그의 아래 카드까지 스크롤할 수 있다"), Scroll->GetScrollOffsetOfEnd() > 0.f);
			Test.TestTrue(Context + TEXT(" 늘어난 시설 목록도 내부 스크롤을 사용한다"), FacilityScroll->GetScrollOffsetOfEnd() > 0.f);
		}
		Scroll->SetScrollOffset(0.f);
		FacilityScroll->SetScrollOffset(0.f);
		DrawFrames();
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags Flags;
		Flags.SetLinearToGamma(false);
		if (!Test.TestTrue(Context + TEXT(" 픽셀 읽기"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)
			&& Pixels.Num() == Size.X * Size.Y)) { return false; }
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PrepProgramSky/Layout"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString Path = FPaths::Combine(Directory,
			FString::Printf(TEXT("Program_%s_%dx%d.png"), *Variant, Size.X, Size.Y));
		const bool bSaved = FFileHelper::SaveArrayToFile(Png, *Path);
		Test.TestTrue(Context + TEXT(" PNG 저장"), bSaved);
		Test.AddInfo(FString::Printf(TEXT("프로그램 패널 캡처: %s / 본문 %.1f, 스크롤 끝 %.1f"),
			*FPaths::ConvertRelativePathToFull(Path), ScrollRect.GetSize().Y, Scroll->GetScrollOffsetOfEnd()));
		return bSaved;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepProgramSkyLayoutTest,
	"FestaFiesta.UI.PrepProgram.SkyRenderedLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)

bool FFF_PrepProgramSkyLayoutTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFPrepProgramSkyLayoutTest;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	auto* PanelClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepPanelProgram>(nullptr, PanelPath));
	auto* CardClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepProgramCard>(nullptr, CardPath));
	auto* FacilityRowClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepProgramFacilityRow>(nullptr, FacilityRowPath));
	if (!TestNotNull(TEXT("검사용 에디터 월드"), World) || !TestNotNull(TEXT("저장된 프로그램 패널"), PanelClass) ||
		!TestNotNull(TEXT("저장된 프로그램 카드"), CardClass) || !TestNotNull(TEXT("저장된 시설 행"), FacilityRowClass)) { return false; }
	if (!TestNull(TEXT("표시 fixture는 게임 인스턴스 없는 월드에서만 쓴다"), World->GetGameInstance())) { return false; }
	const bool bPanelWasDirty = PanelClass->GetOutermost()->IsDirty();
	const bool bCardWasDirty = CardClass->GetOutermost()->IsDirty();
	const bool bFacilityRowWasDirty = FacilityRowClass->GetOutermost()->IsDirty();
	const bool bPanelPreviousInitialize = PanelClass->bCanCallInitializedWithoutPlayerContext;
	const bool bCardPreviousInitialize = CardClass->bCanCallInitializedWithoutPlayerContext;
	const bool bFacilityRowPreviousInitialize = FacilityRowClass->bCanCallInitializedWithoutPlayerContext;
	PanelClass->bCanCallInitializedWithoutPlayerContext = true;
	CardClass->bCanCallInitializedWithoutPlayerContext = true;
	FacilityRowClass->bCanCallInitializedWithoutPlayerContext = true;
	ON_SCOPE_EXIT
	{
		PanelClass->bCanCallInitializedWithoutPlayerContext = bPanelPreviousInitialize;
		CardClass->bCanCallInitializedWithoutPlayerContext = bCardPreviousInitialize;
		FacilityRowClass->bCanCallInitializedWithoutPlayerContext = bFacilityRowPreviousInitialize;
	};
	TStrongObjectPtr<UFF_PrepPanelProgram> Panel(CreateWidget<UFF_PrepPanelProgram>(World, PanelClass));
	if (!TestNotNull(TEXT("프로그램 패널 검사 인스턴스"), Panel.Get())) { return false; }
	const TSharedRef<SOverlay> Surface = SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)[Panel->TakeWidget()];
	if (!SetFixture(*this, *Panel, CardClass, FacilityRowClass, 2, false)) { return false; }
	FAssetCompilingManager::Get().FinishAllCompilation();
	FlushRenderingCommands();
	for (FIntPoint Size : { FIntPoint(1120, 600), FIntPoint(1160, 640), FIntPoint(1120, 430) })
	{
		if (!CaptureAndCheck(*this, *Panel, Surface, Size, TEXT("Saved"), false)) { return false; }
	}
	if (!SetFixture(*this, *Panel, CardClass, FacilityRowClass, 8, true) ||
		!CaptureAndCheck(*this, *Panel, Surface, FIntPoint(1120, 600), TEXT("LongCatalog"), true)) { return false; }
	TestEqual(TEXT("렌더 검사가 패널 WBP를 변경하지 않는다"), PanelClass->GetOutermost()->IsDirty(), bPanelWasDirty);
	TestEqual(TEXT("렌더 검사가 카드 WBP를 변경하지 않는다"), CardClass->GetOutermost()->IsDirty(), bCardWasDirty);
	TestEqual(TEXT("렌더 검사가 시설 행 WBP를 변경하지 않는다"), FacilityRowClass->GetOutermost()->IsDirty(), bFacilityRowWasDirty);
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
