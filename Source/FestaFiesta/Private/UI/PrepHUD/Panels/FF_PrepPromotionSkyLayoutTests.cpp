// 저장된 홍보 WBP와 카드의 선택·구매 표시, 네 카드 정렬과 축소 시 내부 스크롤을 확인한다.
#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AssetCompilingManager.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
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
#include "UI/PrepHUD/Panels/FF_PrepPanelPromotion.h"
#include "UI/PrepHUD/Panels/FF_PrepPromotionCard.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SOverlay.h"

namespace FFPrepPromotionSkyLayoutTest
{
	constexpr float Tolerance = .5f;
	constexpr const TCHAR* PanelPath =
		TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelPromotion.WBP_FF_PrepPanelPromotion_C");
	constexpr const TCHAR* CardPath =
		TEXT("/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepPromotionCard.WBP_FF_PrepPromotionCard_C");

	bool Contains(const FSlateRect& Outer, const FSlateRect& Inner)
	{
		return Inner.Left >= Outer.Left - Tolerance && Inner.Right <= Outer.Right + Tolerance &&
			Inner.Top >= Outer.Top - Tolerance && Inner.Bottom <= Outer.Bottom + Tolerance;
	}

	void CheckText(FAutomationTestBase& Test, UUserWidget& Owner, const TCHAR* Name,
		const FSlateRect& Container, const FString& Context)
	{
		UTextBlock* Text = Cast<UTextBlock>(Owner.GetWidgetFromName(Name));
		const FString Label = Context + TEXT(" / ") + Name;
		if (!Test.TestNotNull(Label, Text)) { return; }
		const FSlateRect Rect = Text->GetCachedGeometry().GetLayoutBoundingRect();
		const FVector2D Desired = Text->GetDesiredSize();
		Test.TestTrue(Label + TEXT(" 내용이 존재한다"), !Text->GetText().IsEmpty());
		Test.TestTrue(Label + TEXT(" 할당 영역 안에 들어온다"), Contains(Container, Rect));
		Test.TestTrue(Label + TEXT(" 글자 높이를 확보한다"), Rect.GetSize().Y + Tolerance >= Desired.Y);
		Test.TestTrue(Label + TEXT(" 글자 폭을 확보한다"), Rect.GetSize().X + Tolerance >= Desired.X);
	}

	bool SetFixture(FAutomationTestBase& Test, UFF_PrepPanelPromotion& Panel,
		UWidgetBlueprintGeneratedClass* CardClass, bool bPurchased, bool bLongLabels)
	{
		// GI 없는 표시 인스턴스다. 실제 구매 명령, 경제 상태, 카탈로그 에셋은 변경하지 않는다.
		const TPair<const TCHAR*, const TCHAR*> Values[] = {
			{ TEXT("Text_SelectedPromotion"), TEXT("전국 홍보") },
			{ TEXT("Text_PurchaseCost"), TEXT("1,400 Gold") },
			{ TEXT("Text_GoldAfterLabel"), bPurchased ? TEXT("현재 잔액") : TEXT("구매 후 잔액") },
			{ TEXT("Text_GoldAfterValue"), TEXT("8,300 Gold") },
			{ TEXT("Text_OnceNotice"), TEXT("홍보는 축제당 한 번 선택하며 3일 전체에 적용됩니다.") },
			{ TEXT("Text_NotAppliedNotice"), TEXT("홍보 효과는 예상 방문객에 반영됩니다. 실제 입장 인원은 운영 상황에 따라 달라집니다.") },
			{ TEXT("Text_BlockReason"), bPurchased ? TEXT("이미 홍보를 구매했습니다. 축제당 한 번만 선택할 수 있습니다.") : TEXT("선택한 홍보의 비용을 확인한 뒤 구매하세요.") },
			{ TEXT("Text_PurchaseAction"), bPurchased ? TEXT("구매 완료") : TEXT("구매하기") }
		};
		for (const auto& Pair : Values)
		{
			UTextBlock* Text = Cast<UTextBlock>(Panel.GetWidgetFromName(Pair.Key));
			if (!Test.TestNotNull(FString(TEXT("표시 fixture / ")) + Pair.Key, Text)) { return false; }
			Text->SetText(FText::FromString(Pair.Value));
			Text->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		UWrapBox* Cards = Cast<UWrapBox>(Panel.GetWidgetFromName(TEXT("Box_PromotionCards")));
		UButton* Purchase = Cast<UButton>(Panel.GetWidgetFromName(TEXT("Button_Purchase")));
		if (!Test.TestNotNull(TEXT("홍보 카드 목록"), Cards) || !Test.TestNotNull(TEXT("구매 버튼"), Purchase)) { return false; }
		Purchase->SetIsEnabled(!bPurchased);
		Cards->ClearChildren();
		const EFF_PromotionChoice Choices[] = {
			EFF_PromotionChoice::None, EFF_PromotionChoice::Basic, EFF_PromotionChoice::Focused, EFF_PromotionChoice::Influencer
		};
		const int32 Costs[] = { 0, 600, 1400, 2600 };
		const float Multipliers[] = { .85f, 1.f, 1.12f, 1.25f };
		const TCHAR* Labels[] = { TEXT("홍보 없음"), TEXT("지역 SNS"), TEXT("전국 홍보"), TEXT("인플루언서") };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Choices); ++Index)
		{
			UFF_PrepPromotionCard* Card = CreateWidget<UFF_PrepPromotionCard>(&Panel, CardClass);
			if (!Test.TestNotNull(TEXT("홍보 카드 검사 인스턴스"), Card)) { return false; }
			FFF_PromotionOptionView Data;
			Data.Choice = Choices[Index];
			Data.CostGold = Costs[Index];
			Data.VisitorMultiplier = Multipliers[Index];
			Card->ConfigureOption(Data, FText::FromString(bLongLabels
				? TEXT("함께 즐기는 축제 전국 온라인 홍보") : Labels[Index]));
			if (Index == 0)
			{
				UTextBlock* Effect = Cast<UTextBlock>(Card->GetWidgetFromName(TEXT("Text_Effect")));
				if (!Test.TestNotNull(TEXT("홍보 없음 효과 표시"), Effect)) { return false; }
				Test.TestTrue(TEXT("홍보 없음도 고정된 그대로 문구 대신 데이터 배수 0.85를 표시한다"),
					Effect->GetText().ToString().Contains(TEXT("0.85")));
			}
			UButton* Root = Cast<UButton>(Card->GetWidgetFromName(TEXT("Button_Root")));
			if (!Test.TestNotNull(TEXT("홍보 카드 버튼"), Root)) { return false; }
			if (bPurchased)
			{
				Card->ApplyCardState(Index == 2 ? EFF_PrepCardState::Committed : EFF_PrepCardState::Locked,
					FText::FromString(Index == 2 ? TEXT("✓ 구매 완료") : TEXT("변경 불가")));
				Test.TestFalse(TEXT("구매 상태 fixture의 카드는 다시 누를 수 없다"), Root->GetIsEnabled());
			}
			else if (Index == 2)
			{
				const FButtonStyle Style = Root->GetStyle();
				Test.TestTrue(TEXT("선택 카드 테두리가 기본보다 두껍다"),
					Style.Pressed.OutlineSettings.Width > Style.Normal.OutlineSettings.Width);
				Card->ApplyCardState(EFF_PrepCardState::Selected, FText::FromString(TEXT("✓ 선택됨")));
				Test.TestEqual(TEXT("선택 브러시가 실제 카드 버튼에 적용된다"),
					Root->GetStyle().Normal.OutlineSettings.Width, Style.Pressed.OutlineSettings.Width);
			}
			else { Card->ApplyCardState(EFF_PrepCardState::Selectable, FText::GetEmpty()); }
			Cards->AddChildToWrapBox(Card);
		}
		return true;
	}

	bool CaptureAndCheck(FAutomationTestBase& Test, UFF_PrepPanelPromotion& Panel,
		const TSharedRef<SWidget>& Surface, FIntPoint Size, const FString& Variant, bool bOverflow, bool bPurchased)
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
		if (!Test.TestNotNull(Context + TEXT(" 본문 스크롤"), Scroll)) { return false; }
		Scroll->SetScrollOffset(0.f);
		DrawFrames();
		UWidget* Header = Panel.GetWidgetFromName(TEXT("HorizontalBox_Header"));
		UWidget* Close = Panel.GetWidgetFromName(TEXT("Button_Close"));
		UWidget* Footer = Panel.GetWidgetFromName(TEXT("Border_Footer"));
		UWidget* Purchase = Panel.GetWidgetFromName(TEXT("Button_Purchase"));
		UWidget* Summary = Panel.GetWidgetFromName(TEXT("Border_PurchaseSummary"));
		UWrapBox* Cards = Cast<UWrapBox>(Panel.GetWidgetFromName(TEXT("Box_PromotionCards")));
		if (!Test.TestNotNull(Context + TEXT(" 헤더"), Header) || !Test.TestNotNull(Context + TEXT(" 닫기"), Close) ||
			!Test.TestNotNull(Context + TEXT(" 푸터"), Footer) || !Test.TestNotNull(Context + TEXT(" 구매"), Purchase) ||
			!Test.TestNotNull(Context + TEXT(" 구매 요약"), Summary) || !Test.TestNotNull(Context + TEXT(" 카드"), Cards)) { return false; }
		const FSlateRect PanelRect(0.f, 0.f, static_cast<float>(Size.X), static_cast<float>(Size.Y));
		const FSlateRect HeaderRect = Header->GetCachedGeometry().GetLayoutBoundingRect();
		const FSlateRect FooterRect = Footer->GetCachedGeometry().GetLayoutBoundingRect();
		const FSlateRect ScrollRect = Scroll->GetCachedGeometry().GetLayoutBoundingRect();
		Test.TestTrue(Context + TEXT(" 헤더와 푸터가 패널 안에 유지된다"),
			Contains(PanelRect, HeaderRect) && Contains(PanelRect, FooterRect));
		Test.TestTrue(Context + TEXT(" 닫기와 구매 버튼이 고정 영역 안에 유지된다"),
			Contains(HeaderRect, Close->GetCachedGeometry().GetLayoutBoundingRect()) &&
			Contains(FooterRect, Purchase->GetCachedGeometry().GetLayoutBoundingRect()));
		Test.TestTrue(Context + TEXT(" 본문이 헤더와 푸터를 침범하지 않는다"),
			Contains(PanelRect, ScrollRect) && ScrollRect.Top >= HeaderRect.Bottom - Tolerance &&
			ScrollRect.Bottom <= FooterRect.Top + Tolerance && ScrollRect.GetSize().Y > 0.f);
		Test.TestTrue(Context + TEXT(" 구매 요약이 고정 푸터 안에 유지된다"),
			Contains(FooterRect, Summary->GetCachedGeometry().GetLayoutBoundingRect()));
		CheckText(Test, Panel, TEXT("Text_PanelTitle"), HeaderRect, Context);
		CheckText(Test, Panel, TEXT("Text_PurchaseAction"), Purchase->GetCachedGeometry().GetLayoutBoundingRect(), Context);
		for (const TCHAR* Name : { TEXT("Text_SelectedPromotion"), TEXT("Text_PurchaseCost"), TEXT("Text_GoldAfterLabel"), TEXT("Text_GoldAfterValue"), TEXT("Text_BlockReason") })
		{
			CheckText(Test, Panel, Name, FooterRect, Context);
		}
		Test.TestEqual(Context + TEXT(" 카드 네 장을 유지한다"), Cards->GetChildrenCount(), 4);
		if (!bOverflow)
		{
			FSlateRect Previous;
			for (int32 Index = 0; Index < Cards->GetChildrenCount(); ++Index)
			{
				const FSlateRect Rect = Cards->GetChildAt(Index)->GetCachedGeometry().GetLayoutBoundingRect();
				Test.TestTrue(Context + TEXT(" 기본 크기에서 네 카드가 모두 보인다"), Contains(ScrollRect, Rect));
				if (Index > 0)
				{
					Test.TestTrue(Context + TEXT(" 네 카드는 같은 크기로 한 줄에 배치된다"),
						FMath::IsNearlyEqual(Rect.GetSize().X, Previous.GetSize().X, Tolerance) &&
						FMath::IsNearlyEqual(Rect.GetSize().Y, Previous.GetSize().Y, Tolerance) &&
						FMath::IsNearlyEqual(Rect.Top, Previous.Top, Tolerance) && Rect.Left >= Previous.Right - Tolerance);
				}
				Previous = Rect;
			}
		}
		// 화면 밖 자손은 CachedGeometry가 갱신되지 않는다. 먼저 스크롤한 뒤 실제 보이는 좌표를 검사한다.
		auto RevealAndCheckText = [&](UUserWidget& Owner, const TCHAR* Name, UWidget* Container, const FString& Label)
		{
			UWidget* Text = Owner.GetWidgetFromName(Name);
			if (!Test.TestNotNull(Label + TEXT(" / ") + Name, Text)) { return; }
			for (int32 Attempt = 0; Attempt < 3; ++Attempt)
			{
				Scroll->ScrollWidgetIntoView(Text, false, EDescendantScrollDestination::IntoView, 2.f);
				DrawFrames();
				if (Contains(Scroll->GetCachedGeometry().GetLayoutBoundingRect(), Text->GetCachedGeometry().GetLayoutBoundingRect())) { break; }
			}
			Test.TestTrue(Label + TEXT(" / ") + Name + TEXT(" 스크롤하면 본문 안에 보인다"),
				Contains(Scroll->GetCachedGeometry().GetLayoutBoundingRect(), Text->GetCachedGeometry().GetLayoutBoundingRect()));
			CheckText(Test, Owner, Name, Container->GetCachedGeometry().GetLayoutBoundingRect(), Label);
		};
		for (const TCHAR* Name : { TEXT("Text_OnceNotice"), TEXT("Text_NotAppliedNotice") })
		{
			RevealAndCheckText(Panel, Name, Scroll, Context);
		}
		for (int32 Index = 0; Index < Cards->GetChildrenCount(); ++Index)
		{
			UFF_PrepPromotionCard* Card = Cast<UFF_PrepPromotionCard>(Cards->GetChildAt(Index));
			if (!Test.TestNotNull(Context + TEXT(" 렌더된 카드"), Card)) { return false; }
			for (const TCHAR* Name : { TEXT("Text_Label"), TEXT("Text_Cost"), TEXT("Text_Effect") })
			{
				RevealAndCheckText(*Card, Name, Card, Context + FString::Printf(TEXT(" 카드 %d"), Index));
			}
			if (Index == 2 || bPurchased) { RevealAndCheckText(*Card, TEXT("Text_Badge"), Card, Context); }
		}
		if (bOverflow)
		{
			Test.TestTrue(Context + TEXT(" 줄바꿈된 아래 카드까지 스크롤할 수 있다"), Scroll->GetScrollOffsetOfEnd() > 0.f);
		}
		Scroll->SetScrollOffset(0.f);
		DrawFrames();
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags Flags;
		Flags.SetLinearToGamma(false);
		if (!Test.TestTrue(Context + TEXT(" 픽셀 읽기"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)
			&& Pixels.Num() == Size.X * Size.Y)) { return false; }
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PrepPromotionSky/Layout"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString Path = FPaths::Combine(Directory,
			FString::Printf(TEXT("Promotion_%s_%dx%d.png"), *Variant, Size.X, Size.Y));
		const bool bSaved = FFileHelper::SaveArrayToFile(Png, *Path);
		Test.TestTrue(Context + TEXT(" PNG 저장"), bSaved);
		Test.AddInfo(FString::Printf(TEXT("홍보 패널 캡처: %s / 본문 %.1f, 스크롤 끝 %.1f"),
			*FPaths::ConvertRelativePathToFull(Path), ScrollRect.GetSize().Y, Scroll->GetScrollOffsetOfEnd()));
		return bSaved;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepPromotionSkyLayoutTest,
	"FestaFiesta.UI.PrepPromotion.SkyRenderedLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)

bool FFF_PrepPromotionSkyLayoutTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFPrepPromotionSkyLayoutTest;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	auto* PanelClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepPanelPromotion>(nullptr, PanelPath));
	auto* CardClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepPromotionCard>(nullptr, CardPath));
	if (!TestNotNull(TEXT("검사용 에디터 월드"), World) || !TestNotNull(TEXT("저장된 홍보 패널"), PanelClass) ||
		!TestNotNull(TEXT("저장된 홍보 카드"), CardClass)) { return false; }
	if (!TestNull(TEXT("표시 fixture는 게임 인스턴스 없는 월드에서만 쓴다"), World->GetGameInstance())) { return false; }
	const bool bPanelWasDirty = PanelClass->GetOutermost()->IsDirty();
	const bool bCardWasDirty = CardClass->GetOutermost()->IsDirty();
	const bool bPanelPreviousInitialize = PanelClass->bCanCallInitializedWithoutPlayerContext;
	const bool bCardPreviousInitialize = CardClass->bCanCallInitializedWithoutPlayerContext;
	PanelClass->bCanCallInitializedWithoutPlayerContext = true;
	CardClass->bCanCallInitializedWithoutPlayerContext = true;
	ON_SCOPE_EXIT
	{
		PanelClass->bCanCallInitializedWithoutPlayerContext = bPanelPreviousInitialize;
		CardClass->bCanCallInitializedWithoutPlayerContext = bCardPreviousInitialize;
	};
	TStrongObjectPtr<UFF_PrepPanelPromotion> Panel(CreateWidget<UFF_PrepPanelPromotion>(World, PanelClass));
	if (!TestNotNull(TEXT("홍보 패널 검사 인스턴스"), Panel.Get())) { return false; }
	const TSharedRef<SOverlay> Surface = SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)[Panel->TakeWidget()];
	if (!SetFixture(*this, *Panel, CardClass, false, false)) { return false; }
	FAssetCompilingManager::Get().FinishAllCompilation();
	FlushRenderingCommands();
	for (FIntPoint Size : { FIntPoint(1140, 620), FIntPoint(1080, 600), FIntPoint(860, 500) })
	{
		if (!CaptureAndCheck(*this, *Panel, Surface, Size, TEXT("Saved"), Size.X < 1000, false)) { return false; }
	}
	if (!SetFixture(*this, *Panel, CardClass, true, false) ||
		!CaptureAndCheck(*this, *Panel, Surface, FIntPoint(1140, 620), TEXT("Purchased"), false, true)) { return false; }
	if (!SetFixture(*this, *Panel, CardClass, false, true) ||
		!CaptureAndCheck(*this, *Panel, Surface, FIntPoint(860, 500), TEXT("LongLabels"), true, false)) { return false; }
	TestEqual(TEXT("렌더 검사가 패널 WBP를 변경하지 않는다"), PanelClass->GetOutermost()->IsDirty(), bPanelWasDirty);
	TestEqual(TEXT("렌더 검사가 카드 WBP를 변경하지 않는다"), CardClass->GetOutermost()->IsDirty(), bCardWasDirty);
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
