// 점검 WBP의 고정 헤더/푸터, 미확인 값과 늘어난 준비 목록의 스크롤을 저장 에셋으로 검사한다.
#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AssetCompilingManager.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Engine/UserInterfaceSettings.h"
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
#include "UI/PrepHUD/Panels/FF_PrepChoiceButton.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelCheck.h"
#include "UI/PrepHUD/FF_PrepHudBar.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SOverlay.h"

namespace FFPrepCheckSkyLayoutTest
{
	constexpr float Tolerance = .5f;
	enum class EFixture { Blocked, Ready, Morning, Unknown, LongRequirements };
	bool Contains(const FSlateRect& Outer, const FSlateRect& Inner)
	{
		return Inner.Left >= Outer.Left - Tolerance && Inner.Right <= Outer.Right + Tolerance &&
			Inner.Top >= Outer.Top - Tolerance && Inner.Bottom <= Outer.Bottom + Tolerance;
	}

	bool Intersects(const FSlateRect& A, const FSlateRect& B)
	{
		return A.Right > B.Left && B.Right > A.Left && A.Bottom > B.Top && B.Bottom > A.Top;
	}

	void CheckText(FAutomationTestBase& Test, UUserWidget& Owner, const TCHAR* Name,
		const FSlateRect& Container, const FString& Context)
	{
		auto* Text = Cast<UTextBlock>(Owner.GetWidgetFromName(Name));
		const FString Label = Context + TEXT(" / ") + Name;
		if (!Test.TestNotNull(Label, Text)) { return; }
		const FSlateRect Rect = Text->GetCachedGeometry().GetLayoutBoundingRect();
		const FVector2D Desired = Text->GetDesiredSize();
		Test.TestFalse(Label + TEXT(" 문구가 비어 있지 않다"), Text->GetText().IsEmpty());
		Test.TestTrue(Label + TEXT(" 배정된 내용 영역 안에 들어온다"), Contains(Container, Rect));
		Test.TestTrue(Label + TEXT(" 글자 높이가 잘리지 않는다"), Rect.GetSize().Y + Tolerance >= Desired.Y);
		Test.TestTrue(Label + TEXT(" 글자 폭이 잘리지 않는다"), Rect.GetSize().X + Tolerance >= Desired.X);
	}

	bool SetFixture(FAutomationTestBase& Test, UFF_PrepPanelCheck& Panel, EFixture Kind)
	{
		// 게임 인스턴스 없는 표시 자료다. 실제 시작/경제/시설 판정은 기존 규칙 테스트에서 검증한다.
		const bool bUnknown = Kind == EFixture::Unknown;
		const bool bReady = Kind == EFixture::Ready || Kind == EFixture::Morning;
		const bool bMorning = Kind == EFixture::Morning;
		const TPair<const TCHAR*, const TCHAR*> Values[] = {
			{ TEXT("Text_DayLabel"), bMorning ? TEXT("3일차") : TEXT("1일차") },
			{ TEXT("Text_Weather"), bUnknown ? TEXT("아직 알 수 없습니다") : TEXT("맑음") },
			{ TEXT("Text_DailyGoalValue"), bUnknown ? TEXT("아직 알 수 없습니다") : TEXT("500명") },
			{ TEXT("Text_DailyGoalBonus"), bUnknown ? TEXT("달성 보너스 미확인") : TEXT("달성 시 +300 Gold") },
			{ TEXT("Text_ExpectedVisitors"), bUnknown ? TEXT("아직 알 수 없습니다") : TEXT("425명") },
			{ TEXT("Text_Gold"), bUnknown ? TEXT("아직 알 수 없습니다") : TEXT("9,700") },
			{ TEXT("Text_CapacityRisk"), bUnknown ? TEXT("? 판정 불가 — 예상 방문객이나 정원을 아직 읽지 못했습니다") : TEXT("○ 여유") },
			{ TEXT("Text_CapacityDetail"), bUnknown ? TEXT("Zone 정원을 읽지 못했습니다. 시설을 놓으면 계산됩니다.") : TEXT("정원 62,887명 대비 최대 예상 425명") },
			{ TEXT("Text_StaffSummary"), bUnknown ? TEXT("아직 알 수 없습니다") : TEXT("6명 (기본 6 · 고용 0)") },
			{ TEXT("Text_ProgramSummary"), bUnknown ? TEXT("아직 알 수 없습니다") : TEXT("0개 편성") },
			{ TEXT("Text_RequiredFacilities"), bUnknown ? TEXT("필수 시설 정보를 아직 읽지 못했습니다.") : bReady ? TEXT("메인공연장 · 1/1 · ✓ 설치 완료") : TEXT("메인공연장 · 0/1 · ! 추가 설치 필요") },
			{ TEXT("Text_FinaleSummary"), TEXT("미구매 — 최소 연출만 실행됩니다") },
			{ TEXT("Text_StaffWageCost"), bUnknown ? TEXT("아직 알 수 없습니다") : TEXT("0 Gold") },
			{ TEXT("Text_ExpectedAdmission"), bUnknown ? TEXT("아직 알 수 없습니다") : TEXT("2,125 Gold") },
			{ TEXT("Text_ExpectedGoalBonus"), bUnknown ? TEXT("아직 알 수 없습니다") : TEXT("0 Gold") },
			{ TEXT("Text_EstimateNotice"), TEXT("부스 매출과 미니게임 보상은 운영 결과에 따라 달라지므로 이 표에 넣지 않았습니다.") },
			{ TEXT("Text_OpenGateReason"), bUnknown ? TEXT("아직 시작 가능 여부를 확인하지 못했습니다.") : bReady ? TEXT("✓ 준비가 완료되었습니다.") : TEXT("메인공연장: 1개 추가 설치가 필요합니다. (설치 0/1)") },
			{ TEXT("Text_OpenGateLabel"), bMorning ? TEXT("개장하기") : TEXT("확정하고 Day 1 시작") }
		};
		for (const auto& Value : Values)
		{
			auto* Text = Cast<UTextBlock>(Panel.GetWidgetFromName(Value.Key));
			if (!Test.TestNotNull(FString(TEXT("점검 fixture / ")) + Value.Key, Text)) { return false; }
			Text->SetText(FText::FromString(Value.Value));
			Text->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		auto* Lock = Cast<UTextBlock>(Panel.GetWidgetFromName(TEXT("Text_WeatherLockReason")));
		auto* Open = Cast<UButton>(Panel.GetWidgetFromName(TEXT("Button_OpenGate")));
		UWidget* Finale = Panel.GetWidgetFromName(TEXT("Box_FinaleSummaryRow"));
		if (!Test.TestNotNull(TEXT("날씨 잠금 안내"), Lock) || !Test.TestNotNull(TEXT("시작 버튼"), Open) ||
			!Test.TestNotNull(TEXT("마지막 날 피날레 줄"), Finale)) { return false; }
		Lock->SetText(FText::FromString(TEXT("지금 단계에서는 날씨를 바꿀 수 없습니다.")));
		Lock->SetVisibility(bUnknown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		Finale->SetVisibility(bMorning ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		Open->SetIsEnabled(bReady);
		Open->SetVisibility(ESlateVisibility::Visible);
		for (const TCHAR* Name : { TEXT("Choice_Clear"), TEXT("Choice_Rain"), TEXT("Choice_Windy") })
		{
			auto* Choice = Cast<UFF_PrepChoiceButton>(Panel.GetWidgetFromName(Name));
			if (!Test.TestNotNull(FString(TEXT("날씨 선택 / ")) + Name, Choice)) { return false; }
			Choice->SetSelected(FCString::Strcmp(Name, TEXT("Choice_Clear")) == 0);
			Choice->SetIsEnabled(!bUnknown);
		}
		if (Kind == EFixture::LongRequirements)
		{
			FString Lines;
			for (int32 Index = 1; Index <= 12; ++Index)
			{
				if (!Lines.IsEmpty()) { Lines += TEXT("\n"); }
				Lines += FString::Printf(TEXT("표시 검사 시설 %02d · 0/1 · ! 추가 설치 필요"), Index);
			}
			CastChecked<UTextBlock>(Panel.GetWidgetFromName(TEXT("Text_RequiredFacilities")))->SetText(FText::FromString(Lines));
			CastChecked<UTextBlock>(Panel.GetWidgetFromName(TEXT("Text_OpenGateReason")))->SetText(FText::FromString(
				TEXT("메인공연장: 1개 추가 설치가 필요합니다. (설치 0/1) 배치에서 필요한 시설을 설치한 뒤 준비 현황과 추가 설치 사유를 확인하고 다시 점검해주세요.")));
		}
		return true;
	}

	bool SaveTarget(FAutomationTestBase& Test, UTextureRenderTarget2D& Target, FIntPoint Size, const FString& Name)
	{
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags Flags;
		Flags.SetLinearToGamma(false);
		if (!Test.TestTrue(Name + TEXT(" 픽셀 읽기"), Target.GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)
			&& Pixels.Num() == Size.X * Size.Y)) { return false; }
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PrepCheckSky/Layout"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString Path = FPaths::Combine(Directory, Name + TEXT(".png"));
		const bool bSaved = FFileHelper::SaveArrayToFile(Png, *Path);
		Test.TestTrue(Name + TEXT(" PNG 저장"), bSaved);
		Test.AddInfo(FString::Printf(TEXT("점검 패널 캡처: %s"), *FPaths::ConvertRelativePathToFull(Path)));
		return bSaved;
	}

	bool CaptureAndCheck(FAutomationTestBase& Test, UFF_PrepPanelCheck& Panel,
		const TSharedRef<SWidget>& Surface, FIntPoint Size, EFixture Kind, const FString& Variant)
	{
		const FString Context = FString::Printf(TEXT("Check_%s_%dx%d"), *Variant, Size.X, Size.Y);
		auto* Scroll = Cast<UScrollBox>(Panel.GetWidgetFromName(TEXT("ScrollBox_Body")));
		UWidget* Body = Panel.GetWidgetFromName(TEXT("VerticalBox_Body"));
		UWidget* Header = Panel.GetWidgetFromName(TEXT("HorizontalBox_Header"));
		UWidget* Footer = Panel.GetWidgetFromName(TEXT("Border_Footer"));
		auto* Metrics = Cast<UWrapBox>(Panel.GetWidgetFromName(TEXT("WrapBox_Metrics")));
		const bool bHasAdmissionSection = Panel.GetWidgetFromName(TEXT("Border_AdmissionSection")) != nullptr;
		if (!Test.TestNotNull(Context + TEXT(" 스크롤 본문"), Scroll) || !Test.TestNotNull(Context + TEXT(" 전체 본문"), Body) ||
			!Test.TestNotNull(Context + TEXT(" 헤더"), Header) || !Test.TestNotNull(Context + TEXT(" 푸터"), Footer) ||
			!Test.TestNotNull(Context + TEXT(" 요약 카드 묶음"), Metrics)) { return false; }
		FWidgetRenderer Renderer(false, true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(
			FWidgetRenderer::CreateTargetFor(FVector2D(Size.X, Size.Y), TF_Bilinear, true));
		if (!Test.TestNotNull(Context + TEXT(" 렌더 타깃"), Target.Get())) { return false; }
		auto Draw = [&]()
		{
			for (int32 Frame = 0; Frame < 4; ++Frame)
			{
				Panel.ForceLayoutPrepass();
				Renderer.DrawWidget(Target.Get(), Surface, FVector2D(Size.X, Size.Y), 1.f / 60.f, false);
			}
			FlushRenderingCommands();
		};
		Scroll->SetScrollOffset(0.f);
		Draw();
		const FSlateRect PanelRect(0.f, 0.f, static_cast<float>(Size.X), static_cast<float>(Size.Y));
		const FSlateRect HeaderRect = Header->GetCachedGeometry().GetLayoutBoundingRect();
		const FSlateRect FooterRect = Footer->GetCachedGeometry().GetLayoutBoundingRect();
		const FSlateRect ScrollRect = Scroll->GetCachedGeometry().GetLayoutBoundingRect();
		Test.TestTrue(Context + TEXT(" 헤더와 실행 영역이 패널 안에 유지된다"), Contains(PanelRect, HeaderRect) && Contains(PanelRect, FooterRect));
		Test.TestTrue(Context + TEXT(" 본문이 고정 영역을 침범하지 않는다"), Contains(PanelRect, ScrollRect) &&
			ScrollRect.Top >= HeaderRect.Bottom - Tolerance && ScrollRect.Bottom <= FooterRect.Top + Tolerance && ScrollRect.GetSize().Y > 0.f);
		if (!bHasAdmissionSection && Size == FIntPoint(1140, 700) && (Kind == EFixture::Blocked || Kind == EFixture::Ready))
		{
			// 입장료 영역이 없는 기존 화면만 무스크롤을 요구한다.
			// 입장료 추가·축소·피날레·긴 목록은 아래의 스크롤 도달 검사로 확인한다.
			Test.TestTrue(Context + TEXT(" 기본 크기에서는 본문을 스크롤할 필요가 없다"),
				Scroll->GetScrollOffsetOfEnd() <= Tolerance);
			for (const TCHAR* Name : { TEXT("Text_RequiredFacilities"), TEXT("Text_ExpectedGoalBonus"), TEXT("Text_EstimateNotice") })
			{
				UWidget* Widget = Panel.GetWidgetFromName(Name);
				if (Test.TestNotNull(Context + TEXT(" 최초 표시 / ") + Name, Widget))
				{
					Test.TestTrue(Context + TEXT(" / ") + Name + TEXT(" 처음부터 전부 보인다"),
						Contains(ScrollRect, Widget->GetCachedGeometry().GetLayoutBoundingRect()));
				}
			}
		}
		CheckText(Test, Panel, TEXT("Text_PanelTitle"), HeaderRect, Context);
		CheckText(Test, Panel, TEXT("Text_DayLabel"), HeaderRect, Context);
		CheckText(Test, Panel, TEXT("Text_OpenGateReason"), FooterRect, Context);
		for (const auto& Pair : { TPair<const TCHAR*, const TCHAR*>(TEXT("Button_ContinueBuild"), TEXT("Text_ContinueBuildLabel")),
			TPair<const TCHAR*, const TCHAR*>(TEXT("Button_OpenGate"), TEXT("Text_OpenGateLabel")) })
		{
			UWidget* Button = Panel.GetWidgetFromName(Pair.Key);
			if (!Test.TestNotNull(Context + TEXT(" / ") + Pair.Key, Button)) { return false; }
			const FSlateRect Rect = Button->GetCachedGeometry().GetLayoutBoundingRect();
			Test.TestTrue(Context + TEXT(" 버튼은 고정 푸터 안에 남는다"), Contains(FooterRect, Rect));
			CheckText(Test, Panel, Pair.Value, Rect, Context);
		}
		if (UWidget* Close = Panel.GetWidgetFromName(TEXT("Button_Close")))
		{ Test.TestTrue(Context + TEXT(" 닫기는 고정 헤더 안에 남는다"), Contains(HeaderRect, Close->GetCachedGeometry().GetLayoutBoundingRect())); }
		else { Test.AddError(Context + TEXT(" 닫기 버튼이 없다")); }
		Test.TestEqual(Context + TEXT(" 입장료 영역 유무에 맞는 요약 카드 수"),
			Metrics->GetChildrenCount(), bHasAdmissionSection ? 3 : 4);
		if (bHasAdmissionSection)
		{
			UWidget* ForecastCard = Panel.GetWidgetFromName(TEXT("SizeBox_ExpectedVisitorsCard"));
			if (Test.TestNotNull(Context + TEXT(" 예상 방문객 카드"), ForecastCard))
			{
				Test.TestTrue(Context + TEXT(" 예상 방문객은 입장료 미리보기로 분리된다"), ForecastCard->GetParent() != Metrics);
			}
		}
		if (Size.X >= 1080)
		{
			FSlateRect Previous;
			for (int32 Index = 0; Index < Metrics->GetChildrenCount(); ++Index)
			{
				const FSlateRect Rect = Metrics->GetChildAt(Index)->GetCachedGeometry().GetLayoutBoundingRect();
				Test.TestTrue(Context + TEXT(" 요약 카드가 좌우 폭 안에 들어온다"), Rect.Left >= ScrollRect.Left - Tolerance && Rect.Right <= ScrollRect.Right + Tolerance);
				if (Index > 0) { Test.TestTrue(Context + TEXT(" 넓은 패널에서는 요약 카드가 한 줄이다"),
					FMath::IsNearlyEqual(Rect.Top, Previous.Top, Tolerance) && Rect.Left >= Previous.Right - Tolerance); }
				Previous = Rect;
			}
		}
		auto Reveal = [&](UUserWidget& Owner, const TCHAR* Name)
		{
			UWidget* Widget = Owner.GetWidgetFromName(Name);
			if (!Test.TestNotNull(Context + TEXT(" 스크롤 대상 / ") + Name, Widget)) { return; }
			for (int32 Attempt = 0; Attempt < 3; ++Attempt)
			{
				Scroll->ScrollWidgetIntoView(Widget, false, EDescendantScrollDestination::IntoView, 2.f);
				Draw();
				const FSlateRect Rect = Widget->GetCachedGeometry().GetLayoutBoundingRect();
				if (Contains(Scroll->GetCachedGeometry().GetLayoutBoundingRect(), Rect)) { break; }
			}
			const FSlateRect Rect = Widget->GetCachedGeometry().GetLayoutBoundingRect();
			const FSlateRect VisibleRect = Scroll->GetCachedGeometry().GetLayoutBoundingRect();
			// 한 화면보다 긴 준비 목록은 본문 전체에 보관하고 일부씩 스크롤한다.
			Test.TestTrue(Context + TEXT(" / ") + Name + TEXT(" 스크롤하면 읽을 수 있다"),
				Rect.GetSize().Y > VisibleRect.GetSize().Y ? Intersects(VisibleRect, Rect) : Contains(VisibleRect, Rect));
			CheckText(Test, Owner, Name, Body->GetCachedGeometry().GetLayoutBoundingRect(), Context);
		};
		for (const TCHAR* Name : { TEXT("Text_Weather"), TEXT("Text_DailyGoalValue"), TEXT("Text_DailyGoalBonus"),
			TEXT("Text_ExpectedVisitors"), TEXT("Text_Gold"), TEXT("Text_CapacityRisk"), TEXT("Text_CapacityDetail"),
			TEXT("Text_StaffSummary"), TEXT("Text_ProgramSummary"), TEXT("Text_RequiredFacilities"),
			TEXT("Text_StaffWageCost"), TEXT("Text_ExpectedAdmission"), TEXT("Text_ExpectedGoalBonus"), TEXT("Text_EstimateNotice") })
		{ Reveal(Panel, Name); }
		if (Kind == EFixture::Unknown) { Reveal(Panel, TEXT("Text_WeatherLockReason")); }
		if (Kind == EFixture::Morning) { Reveal(Panel, TEXT("Text_FinaleSummary")); }
		for (const TCHAR* Name : { TEXT("Choice_Clear"), TEXT("Choice_Rain"), TEXT("Choice_Windy") })
		{
			if (auto* Choice = Cast<UUserWidget>(Panel.GetWidgetFromName(Name))) { Reveal(*Choice, TEXT("Text_Label")); }
		}
		if (Size.X < 1080 || Kind == EFixture::LongRequirements)
		{ Test.TestTrue(Context + TEXT(" 줄바꿈·긴 목록을 위해 실제 스크롤 여유가 있다"), Scroll->GetScrollOffsetOfEnd() > 0.f); }
		Scroll->SetScrollOffset(0.f);
		Draw();
		if (!SaveTarget(Test, *Target, Size, Context)) { return false; }
		if (Kind == EFixture::LongRequirements)
		{
			Scroll->ScrollToEnd(); Draw();
			Test.TestTrue(Context + TEXT(" 늘어난 준비 목록의 끝까지 이동한다"),
				FMath::IsNearlyEqual(Scroll->GetScrollOffset(), Scroll->GetScrollOffsetOfEnd(), 2.f));
			if (!SaveTarget(Test, *Target, Size, Context + TEXT("_Bottom"))) { return false; }
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepCheckSkyLayoutTest,
	"FestaFiesta.UI.PrepCheck.SkyRenderedLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)

bool FFF_PrepCheckSkyLayoutTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFPrepCheckSkyLayoutTest;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	auto* PanelClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepPanelCheck>(nullptr,
		TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelCheck.WBP_FF_PrepPanelCheck_C")));
	auto* ChoiceClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepChoiceButton>(nullptr,
		TEXT("/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepCheckSkyWeatherChoice.WBP_FF_PrepCheckSkyWeatherChoice_C")));
	if (!TestNotNull(TEXT("검사용 에디터 월드"), World) || !TestNotNull(TEXT("저장 점검 WBP"), PanelClass) ||
		!TestNotNull(TEXT("점검 전용 날씨 선택 WBP"), ChoiceClass)) { return false; }
	if (!TestNull(TEXT("표시 검사에서 실제 게임 상태를 변경하지 않는다"), World->GetGameInstance())) { return false; }
	const bool bPanelInitialize = PanelClass->bCanCallInitializedWithoutPlayerContext;
	const bool bChoiceInitialize = ChoiceClass->bCanCallInitializedWithoutPlayerContext;
	const bool bPanelDirty = PanelClass->GetOutermost()->IsDirty();
	const bool bChoiceDirty = ChoiceClass->GetOutermost()->IsDirty();
	ON_SCOPE_EXIT
	{
		TestEqual(TEXT("검사가 저장 점검 에셋을 수정하지 않는다"), PanelClass->GetOutermost()->IsDirty(), bPanelDirty);
		TestEqual(TEXT("검사가 저장 날씨 에셋을 수정하지 않는다"), ChoiceClass->GetOutermost()->IsDirty(), bChoiceDirty);
		PanelClass->bCanCallInitializedWithoutPlayerContext = bPanelInitialize;
		ChoiceClass->bCanCallInitializedWithoutPlayerContext = bChoiceInitialize;
		PanelClass->GetOutermost()->SetDirtyFlag(bPanelDirty);
		ChoiceClass->GetOutermost()->SetDirtyFlag(bChoiceDirty);
	};
	PanelClass->bCanCallInitializedWithoutPlayerContext = true;
	ChoiceClass->bCanCallInitializedWithoutPlayerContext = true;
	TStrongObjectPtr<UFF_PrepPanelCheck> Panel(CreateWidget<UFF_PrepPanelCheck>(World, PanelClass));
	if (!TestNotNull(TEXT("점검 표시 검사 인스턴스"), Panel.Get())) { return false; }
	const TSharedRef<SOverlay> Surface = SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)[Panel->TakeWidget()];
	FAssetCompilingManager::Get().FinishAllCompilation();
	FlushRenderingCommands();
	struct FCase { FIntPoint Size; EFixture Kind; const TCHAR* Name; };
	const FCase Cases[] = {
		{ FIntPoint(1140, 700), EFixture::Blocked, TEXT("Saved") },
		{ FIntPoint(1080, 560), EFixture::Blocked, TEXT("Compact") },
		{ FIntPoint(860, 500), EFixture::Blocked, TEXT("Narrow") },
		{ FIntPoint(1140, 700), EFixture::Ready, TEXT("Ready") },
		{ FIntPoint(1140, 700), EFixture::Morning, TEXT("MorningCheck") },
		{ FIntPoint(860, 500), EFixture::Unknown, TEXT("Unknown") },
		{ FIntPoint(860, 500), EFixture::LongRequirements, TEXT("LongRequirements") }
	};
	for (const FCase& One : Cases)
	{
		if (!SetFixture(*this, *Panel, One.Kind) || !CaptureAndCheck(*this, *Panel, Surface, One.Size, One.Kind, One.Name)) { return false; }
	}
	// 인스턴스의 글꼴·패널 크기만 변경해 Designer 편집에 대응하는지 확인한다.
	if (!SetFixture(*this, *Panel, EFixture::Ready)) { return false; }
	for (const auto& Pair : { TPair<const TCHAR*, int32>(TEXT("Text_PanelTitle"), 29),
		TPair<const TCHAR*, int32>(TEXT("Text_EstimateNotice"), 18) })
	{
		auto* Text = Cast<UTextBlock>(Panel->GetWidgetFromName(Pair.Key));
		if (!TestNotNull(TEXT("글꼴 편집 대상"), Text)) { return false; }
		FSlateFontInfo Font = Text->GetFont(); Font.Size = Pair.Value; Text->SetFont(Font);
	}
	return CaptureAndCheck(*this, *Panel, Surface, FIntPoint(1140, 740), EFixture::Ready, TEXT("EditableFont"));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepCheckStaffShortcutLayoutTest,
	"FestaFiesta.UI.PrepCheck.StaffShortcutRenderedLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)

bool FFF_PrepCheckStaffShortcutLayoutTest::RunTest(const FString& Parameters)
{
	using namespace FFPrepCheckSkyLayoutTest;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	auto* PanelClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepPanelCheck>(nullptr,
		TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelCheck.WBP_FF_PrepPanelCheck_C")));
	UClass* BarClass = LoadClass<UFF_PrepHudBar>(nullptr,
		TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar.WBP_FF_PrepHudBar_C"));
	if (!TestNotNull(TEXT("검사용 에디터 월드"), World) || !TestNotNull(TEXT("저장 점검 WBP"), PanelClass) ||
		!TestNotNull(TEXT("저장 준비 HUD"), BarClass)) return false;
	const FMapProperty* LayoutProperty = FindFProperty<FMapProperty>(BarClass, TEXT("PanelLayouts"));
	if (!TestNotNull(TEXT("실제 HUD의 패널 크기 설정"), LayoutProperty)) return false;
	const auto* Layouts = LayoutProperty->ContainerPtrToValuePtr<TMap<EFF_PrepHudTab, FFF_PrepPanelLayout>>(BarClass->GetDefaultObject());
	const FFF_PrepPanelLayout* Layout = Layouts->Find(EFF_PrepHudTab::Check);
	if (!TestNotNull(TEXT("실제 점검 크기"), Layout)) return false;
	const bool bPreviousPlayerless = PanelClass->bCanCallInitializedWithoutPlayerContext;
	PanelClass->bCanCallInitializedWithoutPlayerContext = true;
	ON_SCOPE_EXIT { PanelClass->bCanCallInitializedWithoutPlayerContext = bPreviousPlayerless; };
	TStrongObjectPtr<UFF_PrepPanelCheck> Panel(CreateWidget<UFF_PrepPanelCheck>(World, PanelClass));
	if (!TestNotNull(TEXT("저장 점검 인스턴스"), Panel.Get())) return false;
	const TSharedRef<SWidget> PanelSlate = Panel->TakeWidget();
	if (!SetFixture(*this, *Panel, EFixture::Blocked)) return false;
	auto* Reason = Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("Text_OpenGateReason")));
	auto* Action = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_OpenGateReason")));
	auto* ActionLabel = Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("Text_OpenGateStaffLink")));
	UWidget* Row = Panel->GetWidgetFromName(TEXT("HorizontalBox_OpenGateReason"));
	UWidget* Footer = Panel->GetWidgetFromName(TEXT("Border_Footer"));
	if (!TestNotNull(TEXT("배정 사유 문구"), Reason) || !TestNotNull(TEXT("오른쪽 배정 버튼"), Action) ||
		!TestNotNull(TEXT("버튼 이름"), ActionLabel) || !TestNotNull(TEXT("사유와 버튼 행"), Row) ||
		!TestNotNull(TEXT("고정 푸터"), Footer)) return false;
	Action->SetVisibility(ESlateVisibility::Visible);
	Action->SetIsEnabled(true);
	FAssetCompilingManager::Get().FinishAllCompilation();
	FlushRenderingCommands();
	// Match both authoritative guidance branches, including the longer no-waiting-staff case.
	const TCHAR* Reasons[] = {
		TEXT("메인안내소: 스태프를 1명 더 배정해야 합니다. (배정 0/1) 설치한 시설의 `스태프` 탭에서 `+ 배정`을 누르세요."),
		TEXT("메인안내소: 스태프를 1명 더 배정해야 합니다. (배정 0/1) 대기 중인 스태프가 없습니다. `인력` 탭에서 고용하거나 다른 안내 시설의 스태프를 `- 해제`한 뒤 배정하세요.")
	};
	for (const FIntPoint Resolution : { FIntPoint(1280, 720), FIntPoint(1920, 1080) })
	{
		const float Dpi = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Resolution);
		const TSharedRef<SWidget> Surface = SNew(SDPIScaler).DPIScale(Dpi)
		[
			SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(Layout->Size.X).HeightOverride(Layout->Size.Y)[PanelSlate]
			]
		];
		FWidgetRenderer Renderer(false, true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(
			FVector2D(Resolution.X, Resolution.Y), TF_Bilinear, true));
		if (!TestNotNull(TEXT("배정 바로가기 렌더 타깃"), Target.Get())) return false;
		for (int32 Variant = 0; Variant < UE_ARRAY_COUNT(Reasons); ++Variant)
		{
			Reason->SetText(FText::FromString(Reasons[Variant]));
			for (int32 Frame = 0; Frame < 4; ++Frame)
			{
				Panel->ForceLayoutPrepass();
				Renderer.DrawWidget(Target.Get(), Surface, FVector2D(Resolution.X, Resolution.Y), 1.f / 60.f, false);
			}
			FlushRenderingCommands();
			const FString Context = FString::Printf(TEXT("Check_StaffShortcut_%s_%dx%d"),
				Variant == 0 ? TEXT("Available") : TEXT("NoWaitingStaff"), Resolution.X, Resolution.Y);
			const FSlateRect Screen(0.f, 0.f, Resolution.X, Resolution.Y);
			const FSlateRect ReasonRect = Reason->GetCachedGeometry().GetLayoutBoundingRect();
			const FSlateRect ActionRect = Action->GetCachedGeometry().GetLayoutBoundingRect();
			const FSlateRect RowRect = Row->GetCachedGeometry().GetLayoutBoundingRect();
			const FSlateRect FooterRect = Footer->GetCachedGeometry().GetLayoutBoundingRect();
			TestTrue(Context + TEXT(" 안내 문구 바로 오른쪽에 버튼이 보인다"),
				ActionRect.Left >= ReasonRect.Right + 8.f * Dpi &&
				FMath::IsNearlyEqual((ReasonRect.Top + ReasonRect.Bottom) * .5f,
					(ActionRect.Top + ActionRect.Bottom) * .5f, 1.f));
			TestFalse(Context + TEXT(" 문구와 버튼이 겹치지 않는다"), Intersects(ReasonRect, ActionRect));
			TestTrue(Context + TEXT(" 문구와 버튼이 같은 행과 화면 안에 전부 들어온다"),
				Contains(RowRect, ReasonRect) && Contains(RowRect, ActionRect) &&
				Contains(FooterRect, RowRect) && Contains(Screen, FooterRect));
			for (UTextBlock* Text : { Reason, ActionLabel })
			{
				const FVector2D Allocated = Text->GetCachedGeometry().GetLocalSize();
				const FVector2D Desired = Text->GetDesiredSize();
				TestTrue(Context + TEXT(" / ") + Text->GetName() + TEXT(" 글자 크기·줄바꿈이 잘리지 않는다"),
					Allocated.X + Tolerance >= Desired.X && Allocated.Y + Tolerance >= Desired.Y);
			}
			TestTrue(Context + TEXT(" 버튼 이름이 버튼 내부에 전부 들어온다"),
				Contains(ActionRect, ActionLabel->GetCachedGeometry().GetLayoutBoundingRect()));
			TestTrue(Context + TEXT(" 버튼 클릭 높이를 유지한다"), Action->GetCachedGeometry().GetLocalSize().Y >= 36.f);
			if (Variant == 1)
				TestTrue(Context + TEXT(" 긴 실제 배정 사유는 여러 줄로 읽힌다"),
					Reason->GetDesiredSize().Y > ActionLabel->GetDesiredSize().Y * 1.5f);
			if (!SaveTarget(*this, *Target, Resolution, Context)) return false;
		}
	}
	Panel->ReleaseSlateResources(true);
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
