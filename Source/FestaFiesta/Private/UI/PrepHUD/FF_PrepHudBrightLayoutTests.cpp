// 저장된 밝은 HUD를 실제 화면 크기로 그려 하단 밀착과 디자이너 높이 변경을 확인한다.
#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AssetCompilingManager.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
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
#include "UI/PrepHUD/FF_PrepHudBar.h"
#include "UI/PrepHUD/FF_PrepHudTabButton.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SOverlay.h"

namespace FFPrepHudBrightLayoutTest
{
	constexpr float Tolerance = .5f;
	constexpr const TCHAR* HudPath =
		TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar.WBP_FF_PrepHudBar_C");
	constexpr const TCHAR* TabPath =
		TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudTabButton.WBP_FF_PrepHudTabButton_C");

	class FPlayerlessInitialization
	{
	public:
		explicit FPlayerlessInitialization(UWidgetBlueprintGeneratedClass& InClass)
			: Class(InClass), bPrevious(Class.bCanCallInitializedWithoutPlayerContext)
		{
			Class.bCanCallInitializedWithoutPlayerContext = true;
		}
		~FPlayerlessInitialization() { Class.bCanCallInitializedWithoutPlayerContext = bPrevious; }
	private:
		UWidgetBlueprintGeneratedClass& Class;
		bool bPrevious;
	};

	bool Contains(const FSlateRect& Outer, const FSlateRect& Inner)
	{
		return Inner.Left >= Outer.Left - Tolerance && Inner.Right <= Outer.Right + Tolerance &&
			Inner.Top >= Outer.Top - Tolerance && Inner.Bottom <= Outer.Bottom + Tolerance;
	}

	void CheckText(FAutomationTestBase& Test, UTextBlock& Text, const FSlateRect& Container,
		const FString& Context, bool bAllowEllipsis = false)
	{
		const FSlateRect Rect = Text.GetCachedGeometry().GetLayoutBoundingRect();
		const FVector2D Desired = Text.GetDesiredSize();
		const FString Label = Context + TEXT(" / ") + Text.GetName();
		Test.TestTrue(Label + TEXT(" 문구가 비어 있지 않다"), !Text.GetText().IsEmpty());
		Test.TestTrue(Label + TEXT(" 글자가 행 또는 버튼 안에 들어온다"), Contains(Container, Rect));
		Test.TestTrue(Label + TEXT(" 글자 높이가 잘리지 않는다"), Rect.GetSize().Y + Tolerance >= Desired.Y);
		Test.TestTrue(Label + TEXT(" 읽을 수 있는 글자 폭을 확보한다"),
			Rect.GetSize().X + Tolerance >= (bAllowEllipsis ? FMath::Min(100.0, Desired.X) : Desired.X));
		Test.TestTrue(Label + TEXT(" HUD 글꼴은 읽을 수 있는 크기다"), Text.GetFont().Size >= 14);
	}

	bool CaptureAndCheck(FAutomationTestBase& Test, UFF_PrepHudBar& Hud,
		const TSharedRef<SWidget>& Surface, FIntPoint Size, const FString& Variant)
	{
		const FString Context = FString::Printf(TEXT("%s %dx%d"), *Variant, Size.X, Size.Y);
		FWidgetRenderer Renderer(false, true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(
			FWidgetRenderer::CreateTargetFor(FVector2D(Size.X, Size.Y), TF_Bilinear, true));
		if (!Test.TestNotNull(Context + TEXT(" 렌더 타깃"), Target.Get())) { return false; }
		for (int32 Frame = 0; Frame < 3; ++Frame)
		{
			Hud.ForceLayoutPrepass();
			Renderer.DrawWidget(Target.Get(), Surface, FVector2D(Size.X, Size.Y), 1.f / 60.f, false);
		}
		FlushRenderingCommands();

		UBorder* Menu = Cast<UBorder>(Hud.GetWidgetFromName(TEXT("Border_MenuRow")));
		UBorder* Status = Cast<UBorder>(Hud.GetWidgetFromName(TEXT("Border_StatusRow")));
		if (!Test.TestNotNull(Context + TEXT(" 메뉴 행"), Menu) ||
			!Test.TestNotNull(Context + TEXT(" 상태 행"), Status)) { return false; }
		const FSlateRect Screen(0.f, 0.f, static_cast<float>(Size.X), static_cast<float>(Size.Y));
		const FSlateRect MenuRect = Menu->GetCachedGeometry().GetLayoutBoundingRect();
		const FSlateRect StatusRect = Status->GetCachedGeometry().GetLayoutBoundingRect();
		Test.TestEqual(Context + TEXT(" 상태 행이 화면 바닥에 붙는다"), StatusRect.Bottom, Screen.Bottom, Tolerance);
		Test.TestEqual(Context + TEXT(" 상태 행이 화면 왼쪽 끝까지 온다"), StatusRect.Left, Screen.Left, Tolerance);
		Test.TestEqual(Context + TEXT(" 상태 행이 화면 오른쪽 끝까지 온다"), StatusRect.Right, Screen.Right, Tolerance);
		Test.TestEqual(Context + TEXT(" 메뉴와 상태 행 사이에 틈이나 겹침이 없다"), MenuRect.Bottom, StatusRect.Top, Tolerance);
		Test.TestTrue(Context + TEXT(" 메뉴와 상태 행이 화면 안에 들어온다"),
			Contains(Screen, MenuRect) && Contains(Screen, StatusRect));
		Test.TestTrue(Context + TEXT(" 두 행 모두 실제 높이를 가진다"),
			MenuRect.GetSize().Y >= 32.f && StatusRect.GetSize().Y >= 32.f);

		float PreviousRight = MenuRect.Left;
		for (const TCHAR* Name : { TEXT("Tab_Build"), TEXT("Tab_Program"), TEXT("Tab_Staff"),
			TEXT("Tab_Promotion"), TEXT("Tab_Check") })
		{
			UFF_PrepHudTabButton* Tab = Cast<UFF_PrepHudTabButton>(Hud.GetWidgetFromName(Name));
			if (!Test.TestNotNull(Context + TEXT(" / ") + Name, Tab)) { continue; }
			const FSlateRect Rect = Tab->GetCachedGeometry().GetLayoutBoundingRect();
			Test.TestTrue(Context + TEXT(" 표시되는 탭 / ") + Name, Tab->IsVisible());
			Test.TestTrue(Context + TEXT(" 탭이 메뉴 안에 들어온다 / ") + Name, Contains(MenuRect, Rect));
			Test.TestTrue(Context + TEXT(" 탭끼리 겹치지 않는다 / ") + Name, Rect.Left >= PreviousRight - Tolerance);
			Test.TestTrue(Context + TEXT(" 탭의 클릭 영역을 확보한다 / ") + Name,
				Rect.GetSize().X >= 44.f && Rect.GetSize().Y >= 32.f);
			PreviousRight = Rect.Right;
			UTextBlock* Label = Cast<UTextBlock>(Tab->GetWidgetFromName(TEXT("Text_Label")));
			if (Test.TestNotNull(Context + TEXT(" 탭 라벨 / ") + Name, Label))
			{
				CheckText(Test, *Label, Rect, Context + TEXT(" / ") + Name);
			}
		}

		UWidget* Left = Hud.GetWidgetFromName(TEXT("HorizontalBox_StatusLeft"));
		UWidget* Day = Hud.GetWidgetFromName(TEXT("Border_DayStage"));
		UWidget* Right = Hud.GetWidgetFromName(TEXT("HorizontalBox_StatusRight"));
		if (Test.TestNotNull(Context + TEXT(" 왼쪽 정보 묶음"), Left) &&
			Test.TestNotNull(Context + TEXT(" 중앙 일차 칩"), Day) &&
			Test.TestNotNull(Context + TEXT(" 오른쪽 사유 묶음"), Right))
		{
			const FSlateRect LeftRect = Left->GetCachedGeometry().GetLayoutBoundingRect();
			const FSlateRect DayRect = Day->GetCachedGeometry().GetLayoutBoundingRect();
			const FSlateRect RightRect = Right->GetCachedGeometry().GetLayoutBoundingRect();
			Test.TestTrue(Context + TEXT(" 상태 정보가 중앙 일차 칩과 겹치지 않는다"),
				LeftRect.Right <= DayRect.Left + Tolerance && DayRect.Right <= RightRect.Left + Tolerance);
			for (const TCHAR* Name : { TEXT("Text_FestivalName"), TEXT("Text_Gold"), TEXT("Text_Staff"),
				TEXT("Text_DayStage") })
			{
				UTextBlock* Text = Cast<UTextBlock>(Hud.GetWidgetFromName(Name));
				if (!Test.TestNotNull(Context + TEXT(" 상태 문구 / ") + Name, Text)) { continue; }
				const bool bDay = Text->GetFName() == TEXT("Text_DayStage");
				CheckText(Test, *Text, bDay ? DayRect : LeftRect, Context);
			}
			const UTextBlock* Reason = Cast<UTextBlock>(Hud.GetWidgetFromName(TEXT("Text_CtaReason")));
			if (Test.TestNotNull(Context + TEXT(" 상태 사유"), Reason))
			{
				// 진행 사유는 점검 패널이 소유하고 HUD의 중복 문구는 접힌다.
				Test.TestTrue(Context + TEXT(" 중복된 하단 진행 사유는 숨긴다"),
					Reason->GetText().IsEmpty() && Reason->GetVisibility() == ESlateVisibility::Collapsed);
			}
		}

		Test.AddInfo(FString::Printf(TEXT("%s: 메뉴=(%.1f, %.1f)-(%.1f, %.1f), 상태=(%.1f, %.1f)-(%.1f, %.1f)"),
			*Context, MenuRect.Left, MenuRect.Top, MenuRect.Right, MenuRect.Bottom,
			StatusRect.Left, StatusRect.Top, StatusRect.Right, StatusRect.Bottom));
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags Flags;
		Flags.SetLinearToGamma(false);
		if (!Test.TestTrue(Context + TEXT(" 전체 화면 픽셀을 읽는다"),
			Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags) && Pixels.Num() == Size.X * Size.Y))
		{
			return false;
		}
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BrightCountryHud/Layout"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString Path = FPaths::Combine(Directory,
			FString::Printf(TEXT("PrepHud_%s_%dx%d.png"), *Variant, Size.X, Size.Y));
		const bool bSaved = FFileHelper::SaveArrayToFile(Png, *Path);
		Test.TestTrue(Context + TEXT(" 렌더링 캡처를 저장한다"), bSaved);
		Test.AddInfo(FString::Printf(TEXT("HUD 레이아웃 캡처: %s"), *FPaths::ConvertRelativePathToFull(Path)));
		return bSaved;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepHudBrightLayoutTest,
	"FestaFiesta.UI.PrepHUD.BrightBottom.RenderedLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)

bool FFF_PrepHudBrightLayoutTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFPrepHudBrightLayoutTest;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	auto* HudClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepHudBar>(nullptr, HudPath));
	auto* TabClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepHudTabButton>(nullptr, TabPath));
	if (!TestNotNull(TEXT("레이아웃 검증용 에디터 월드"), World) ||
		!TestNotNull(TEXT("저장된 Prep HUD WBP"), HudClass) ||
		!TestNotNull(TEXT("저장된 Prep HUD 탭 WBP"), TabClass)) { return false; }
	if (!TestNull(TEXT("표시값 fixture는 게임 인스턴스가 없는 에디터 월드에서만 사용한다"), World->GetGameInstance()))
	{
		return false;
	}
	const bool bHudWasDirty = HudClass->GetOutermost()->IsDirty();
	const bool bTabWasDirty = TabClass->GetOutermost()->IsDirty();
	FPlayerlessInitialization HudInitialization(*HudClass);
	FPlayerlessInitialization TabInitialization(*TabClass);
	TStrongObjectPtr<UFF_PrepHudBar> Hud(CreateWidget<UFF_PrepHudBar>(World, HudClass));
	if (!TestNotNull(TEXT("저장된 HUD를 재생성 없이 인스턴스화한다"), Hud.Get())) { return false; }
	const TWeakObjectPtr<UFF_PrepHudBar> PreviousActive = UFF_PrepHudBar::ActiveBar;
	ON_SCOPE_EXIT
	{
		if (!UFF_PrepHudBar::ActiveBar.IsValid() || UFF_PrepHudBar::ActiveBar.Get() == Hud.Get())
		{
			UFF_PrepHudBar::ActiveBar = PreviousActive;
		}
	};
	// 이 인스턴스의 GI 부재만 예상한다. 실제 런타임의 다른 ViewModel 오류는 통과시키지 않는다.
	AddExpectedError(FString::Printf(TEXT("[%s] ViewModel 을 찾지 못했다. 금화·스태프·단계가 전부 비어 보인다."),
		*Hud->GetName()), EAutomationExpectedErrorFlags::Contains, 1, false);
	// UIFlow의 HUDLayer와 똑같이 전체 화면을 할당한다. 하단 위치는 저장된 WBP가 정한다.
	const TSharedRef<SOverlay> Surface = SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)[Hud->TakeWidget()];
	// RefreshStatusRow는 GI가 없으면 축제명만 비우고 나머지는 갱신하지 않는다.
	// 따라서 저장된 preview에 의존하지 않게 인스턴스에 표시값을 명시한다. 슬롯/스타일은 바꾸지 않는다.
	const auto SetDisplayFixture = [this, &Hud](const TCHAR* TextName, const TCHAR* GroupName, const TCHAR* Value)
	{
		UTextBlock* Text = Cast<UTextBlock>(Hud->GetWidgetFromName(TextName));
		UWidget* Group = Hud->GetWidgetFromName(GroupName);
		if (!TestNotNull(FString(TEXT("표시값 fixture 텍스트 / ")) + TextName, Text) ||
			!TestNotNull(FString(TEXT("표시값 fixture 묶음 / ")) + GroupName, Group)) { return false; }
		Text->SetText(FText::FromString(Value));
		Text->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Group->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return true;
	};
	if (!SetDisplayFixture(TEXT("Text_FestivalName"), TEXT("Box_FestivalName"), TEXT("다대포 바다축제")) ||
		!SetDisplayFixture(TEXT("Text_Gold"), TEXT("Box_Gold"), TEXT("10,000 Gold")) ||
		!SetDisplayFixture(TEXT("Text_Staff"), TEXT("Box_Staff"), TEXT("6명")) ||
		!SetDisplayFixture(TEXT("Text_DayStage"), TEXT("Border_DayStage"), TEXT("1일차 · 셋업"))) { return false; }
	// 진행 사유는 점검 패널에서 표시하며 하단 바의 중복 문구는 접힌다.
	// WBP를 처음 로드하면 아이콘 텍스처 컴파일이 비동기로 남을 수 있다.
	// 레이아웃 정착용 Draw 호출은 게임 루프를 기다리지 않으므로 첫 캡처 전에 리소스를 준비한다.
	FAssetCompilingManager::Get().FinishAllCompilation();
	FlushRenderingCommands();

	for (const FIntPoint Size : { FIntPoint(1280, 720), FIntPoint(1920, 1080), FIntPoint(2560, 1080) })
	{
		if (!CaptureAndCheck(*this, *Hud, Surface, Size, TEXT("Saved"))) { return false; }
	}
	USizeBox* StatusSize = Cast<USizeBox>(Hud->GetWidgetFromName(TEXT("SizeBox_StatusRow")));
	if (!TestNotNull(TEXT("Designer에서 높이를 조절할 상태 행 SizeBox"), StatusSize)) { return false; }
	const float PreviousHeight = StatusSize->GetCachedGeometry().GetLocalSize().Y;
	// 행 높이만 바꾸면 연결/하단 앵커가 유지되어야 한다. 위치나 부모 슬롯은 건드리지 않는다.
	StatusSize->SetHeightOverride(PreviousHeight + 24.f);
	if (!CaptureAndCheck(*this, *Hud, Surface, FIntPoint(1920, 1080), TEXT("TallerStatus"))) { return false; }
	TestEqual(TEXT("인스턴스의 Designer 높이 변경이 실제 행 높이에 반영된다"),
		static_cast<float>(StatusSize->GetCachedGeometry().GetLocalSize().Y), PreviousHeight + 24.f, Tolerance);
	TestEqual(TEXT("검증이 저장된 HUD 패키지를 변경하지 않는다"), HudClass->GetOutermost()->IsDirty(), bHudWasDirty);
	TestEqual(TEXT("검증이 저장된 탭 패키지를 변경하지 않는다"), TabClass->GetOutermost()->IsDirty(), bTabWasDirty);
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
