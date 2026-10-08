// 저장된 인력 WBP를 그려 두 열 배치, 고정 푸터와 작은 높이의 본문 스크롤을 확인한다.
#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AssetCompilingManager.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
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
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SOverlay.h"

namespace FFPrepStaffSkyLayoutTest
{
	constexpr float Tolerance = .5f;
	constexpr const TCHAR* PanelPath =
		TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelStaff.WBP_FF_PrepPanelStaff_C");

	bool Contains(const FSlateRect& Outer, const FSlateRect& Inner)
	{
		return Inner.Left >= Outer.Left - Tolerance && Inner.Right <= Outer.Right + Tolerance &&
			Inner.Top >= Outer.Top - Tolerance && Inner.Bottom <= Outer.Bottom + Tolerance;
	}

	bool SetFixture(FAutomationTestBase& Test, UFF_PrepPanelStaff& Panel)
	{
		// GI 없는 검사 인스턴스의 표시만 채운다. 실제 고용 요청이나 경제 상태는 건드리지 않는다.
		const TPair<const TCHAR*, const TCHAR*> Values[] = {
			{ TEXT("Text_StaffCapacity"), TEXT("6 / 12명") },
			{ TEXT("Text_StaffBreakdown"), TEXT("기본 6명 · 추가 고용 0명") },
			{ TEXT("Text_SelectedCount"), TEXT("1명") },
			{ TEXT("Text_HireUnitCost"), TEXT("1명 × 450 Gold") },
			{ TEXT("Text_HireCostTotal"), TEXT("450 Gold") },
			{ TEXT("Text_GoldAfterValue"), TEXT("9,250 Gold") },
			{ TEXT("Text_HireSpentValue"), TEXT("0 Gold") },
			{ TEXT("Text_HireAction"), TEXT("1명 고용하기") }
		};
		for (const auto& Pair : Values)
		{
			UTextBlock* Text = Cast<UTextBlock>(Panel.GetWidgetFromName(Pair.Key));
			if (!Test.TestNotNull(FString(TEXT("표시 fixture / ")) + Pair.Key, Text)) { return false; }
			Text->SetText(FText::FromString(Pair.Value));
			Text->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		UProgressBar* Capacity = Cast<UProgressBar>(Panel.GetWidgetFromName(TEXT("Progress_StaffCapacity")));
		if (!Test.TestNotNull(TEXT("인원 비율 막대"), Capacity)) { return false; }
		Capacity->SetPercent(.5f);
		return true;
	}

	void CheckText(FAutomationTestBase& Test, UFF_PrepPanelStaff& Panel, const TCHAR* Name,
		const FSlateRect& Container, const FString& Context)
	{
		UTextBlock* Text = Cast<UTextBlock>(Panel.GetWidgetFromName(Name));
		if (!Test.TestNotNull(Context + TEXT(" / ") + Name, Text)) { return; }
		const FSlateRect Rect = Text->GetCachedGeometry().GetLayoutBoundingRect();
		const FVector2D Desired = Text->GetDesiredSize();
		const FString Label = Context + TEXT(" / ") + Name;
		Test.TestTrue(Label + TEXT(" 문구가 존재한다"), !Text->GetText().IsEmpty());
		Test.TestTrue(Label + TEXT(" 부모 열/행의 할당 영역에 들어온다"), Contains(Container, Rect));
		Test.TestTrue(Label + TEXT(" 글자 높이를 확보한다"), Rect.GetSize().Y + Tolerance >= Desired.Y);
		Test.TestTrue(Label + TEXT(" 글자 폭을 확보한다"), Rect.GetSize().X + Tolerance >= Desired.X);
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
		UWidget* Footer = Panel.GetWidgetFromName(TEXT("Border_Footer"));
		UWidget* HireButton = Panel.GetWidgetFromName(TEXT("Button_Hire"));
		UWidget* Current = Panel.GetWidgetFromName(TEXT("VerticalBox_CurrentStaff"));
		UWidget* Quote = Panel.GetWidgetFromName(TEXT("VerticalBox_HireQuote"));
		UScrollBox* Scroll = Cast<UScrollBox>(Panel.GetWidgetFromName(TEXT("ScrollBox_Body")));
		if (!Test.TestNotNull(Context + TEXT(" 헤더"), Header) ||
			!Test.TestNotNull(Context + TEXT(" 닫기"), Close) ||
			!Test.TestNotNull(Context + TEXT(" 푸터"), Footer) ||
			!Test.TestNotNull(Context + TEXT(" 고용 버튼"), HireButton) ||
			!Test.TestNotNull(Context + TEXT(" 현재 인원 열"), Current) ||
			!Test.TestNotNull(Context + TEXT(" 고용 견적 열"), Quote) ||
			!Test.TestNotNull(Context + TEXT(" 본문 스크롤"), Scroll)) { return false; }

		const FSlateRect PanelRect(0.f, 0.f, static_cast<float>(Size.X), static_cast<float>(Size.Y));
		const FSlateRect HeaderRect = Header->GetCachedGeometry().GetLayoutBoundingRect();
		const FSlateRect FooterRect = Footer->GetCachedGeometry().GetLayoutBoundingRect();
		const FSlateRect ScrollRect = Scroll->GetCachedGeometry().GetLayoutBoundingRect();
		const FSlateRect CurrentRect = Current->GetCachedGeometry().GetLayoutBoundingRect();
		const FSlateRect QuoteRect = Quote->GetCachedGeometry().GetLayoutBoundingRect();
		Test.TestTrue(Context + TEXT(" 헤더/푸터는 패널 안에 유지된다"),
			Contains(PanelRect, HeaderRect) && Contains(PanelRect, FooterRect));
		Test.TestTrue(Context + TEXT(" 닫기/고용 버튼은 패널 안에 유지된다"),
			Contains(HeaderRect, Close->GetCachedGeometry().GetLayoutBoundingRect()) &&
			Contains(FooterRect, HireButton->GetCachedGeometry().GetLayoutBoundingRect()));
		Test.TestTrue(Context + TEXT(" 본문 뷰포트가 헤더/푸터를 침범하지 않는다"),
			Contains(PanelRect, ScrollRect) && ScrollRect.Top >= HeaderRect.Bottom - Tolerance &&
			ScrollRect.Bottom <= FooterRect.Top + Tolerance && ScrollRect.GetSize().Y > 0.f);
		Test.TestTrue(Context + TEXT(" 현재 인원/견적 두 열은 가로로 겹치지 않는다"),
			CurrentRect.Right <= QuoteRect.Left + Tolerance &&
			CurrentRect.Left >= ScrollRect.Left - Tolerance && QuoteRect.Right <= ScrollRect.Right + Tolerance);
		Test.TestTrue(Context + TEXT(" 두 열 모두 읽을 수 있는 폭이 있다"),
			CurrentRect.GetSize().X > 250.f && QuoteRect.GetSize().X > 250.f);
		CheckText(Test, Panel, TEXT("Text_PanelTitle"), HeaderRect, Context);
		CheckText(Test, Panel, TEXT("Text_HireAction"), HireButton->GetCachedGeometry().GetLayoutBoundingRect(), Context);
		CheckText(Test, Panel, TEXT("Text_DailyNotice"), FooterRect, Context);
		for (const TCHAR* Name : { TEXT("Text_StaffCapacity"), TEXT("Text_StaffBreakdown"), TEXT("Text_HireSpentValue") })
		{
			CheckText(Test, Panel, Name, CurrentRect, Context);
		}
		for (const TCHAR* Name : { TEXT("Text_SelectedCount"), TEXT("Text_HireUnitCost"),
			TEXT("Text_HireCostTotal"), TEXT("Text_GoldAfterValue") })
		{
			CheckText(Test, Panel, Name, QuoteRect, Context);
		}
		// The staff/training tabs share the authored frame. Available body height,
		// rather than the outer panel's old fixed height, determines scrolling.
		if (Scroll->GetScrollOffsetOfEnd() > Tolerance)
		{
			Scroll->ScrollToEnd();
			for (int32 Frame = 0; Frame < 4; ++Frame)
				Renderer.DrawWidget(Target.Get(), Surface, FVector2D(Size.X, Size.Y), 1.f / 60.f, false);
			for (const TCHAR* Name : {TEXT("Text_HireSpentValue"), TEXT("Text_GoldAfterValue")})
			{
				const UWidget* Value = Panel.GetWidgetFromName(Name);
				Test.TestTrue(Context + TEXT(" 스크롤하면 마지막 비용 수치를 모두 읽을 수 있다 / ") + Name,
					Value && Contains(Scroll->GetCachedGeometry().GetLayoutBoundingRect(),
						Value->GetCachedGeometry().GetLayoutBoundingRect()));
			}
			Scroll->ScrollToStart();
			for (int32 Frame = 0; Frame < 4; ++Frame)
				Renderer.DrawWidget(Target.Get(), Surface, FVector2D(Size.X, Size.Y), 1.f / 60.f, false);
		}
		else
		{
			Test.TestTrue(Context + TEXT(" 기본/기존 크기에서 두 열이 본문 안에 모두 보인다"),
				Contains(ScrollRect, CurrentRect) && Contains(ScrollRect, QuoteRect));
		}

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
	for (const FIntPoint Size : { FIntPoint(820, 470), FIntPoint(760, 520), FIntPoint(820, 350) })
	{
		if (!CaptureAndCheck(*this, *Panel, Surface, Size, TEXT("Saved"))) { return false; }
	}

	// Designer와 같은 속성을 새 인스턴스에 넣고 NativePreConstruct/Construct를 거쳐도 유지되는지 본다.
	TStrongObjectPtr<UFF_PrepPanelStaff> Edited(CreateWidget<UFF_PrepPanelStaff>(World, PanelClass));
	if (!TestNotNull(TEXT("편집 가능성 검사 인스턴스"), Edited.Get())) { return false; }
	UTextBlock* Capacity = Cast<UTextBlock>(Edited->GetWidgetFromName(TEXT("Text_StaffCapacity")));
	UButton* Hire = Cast<UButton>(Edited->GetWidgetFromName(TEXT("Button_Hire")));
	if (!TestNotNull(TEXT("편집할 인원 글꼴"), Capacity) || !TestNotNull(TEXT("편집할 버튼 테두리"), Hire)) { return false; }
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
		!CaptureAndCheck(*this, *Edited, EditedSurface, FIntPoint(820, 470), TEXT("EditedStyle"))) { return false; }
	TestEqual(TEXT("사용자가 바꾼 글꼴 크기를 생명주기 함수가 덮어쓰지 않는다"), Capacity->GetFont().Size, Font.Size);
	TestEqual(TEXT("사용자가 바꾼 테두리 두께를 생명주기 함수가 덮어쓰지 않는다"),
		Hire->GetStyle().Normal.OutlineSettings.Width, Style.Normal.OutlineSettings.Width);
	TestTrue(TEXT("사용자가 바꾼 테두리 색을 생명주기 함수가 덮어쓰지 않는다"),
		Hire->GetStyle().Normal.OutlineSettings.Color.GetSpecifiedColor().Equals(Style.Normal.OutlineSettings.Color.GetSpecifiedColor()));
	TestEqual(TEXT("렌더 검사가 저장된 WBP 패키지를 변경하지 않는다"), PanelClass->GetOutermost()->IsDirty(), bWasDirty);
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
