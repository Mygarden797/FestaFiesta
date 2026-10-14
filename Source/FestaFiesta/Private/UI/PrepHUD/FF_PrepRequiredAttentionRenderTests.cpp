// 실제 저장 HUD 안에서 배지를 그려 우상단 넘침이 부모에 잘리지 않고 해제되는지 확인한다.
#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AssetCompilingManager.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
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
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SOverlay.h"

namespace FFPrepRequiredAttentionRender
{
	constexpr int32 Width = 1920;
	constexpr int32 Height = 1080;
	constexpr float Tolerance = .5f;

	bool Different(const FColor& A, const FColor& B)
	{
		return FMath::Abs(int32(A.R) - int32(B.R)) > 8 || FMath::Abs(int32(A.G) - int32(B.G)) > 8 ||
			FMath::Abs(int32(A.B) - int32(B.B)) > 8 || FMath::Abs(int32(A.A) - int32(B.A)) > 8;
	}

	bool SameRect(const FSlateRect& A, const FSlateRect& B)
	{
		return FMath::IsNearlyEqual(A.Left, B.Left, Tolerance) && FMath::IsNearlyEqual(A.Top, B.Top, Tolerance) &&
			FMath::IsNearlyEqual(A.Right, B.Right, Tolerance) && FMath::IsNearlyEqual(A.Bottom, B.Bottom, Tolerance);
	}

	struct FBadgeRegion
	{
		FString Name;
		FSlateRect Tab;
		FSlateRect Badge;
		FColor ExpectedFill;
	};

	FBadgeRegion Region(const UFF_PrepHudTabButton& Tab)
	{
		const FGeometry Geometry = Tab.GetCachedGeometry();
		const FVector2D Size = Geometry.GetLocalSize();
		const float Diameter = FMath::Clamp(Tab.RequiredBadgeDiameter, 16.f, 40.f);
		// 위치 기대값만 계산한다. 합성 원을 그리지 않고 실제 NativePaint의 픽셀을 아래에서 읽는다.
		const FVector2D Origin(Size.X - Diameter + 3.f, -Diameter * .35f);
		const FVector2D Min = Geometry.LocalToAbsolute(Origin);
		const FVector2D Max = Geometry.LocalToAbsolute(Origin + FVector2D(Diameter));
		return { Tab.GetName(), Geometry.GetLayoutBoundingRect(),
			FSlateRect(float(Min.X), float(Min.Y), float(Max.X), float(Max.Y)), Tab.RequiredBadgeColor.ToFColor(true) };
	}

	int32 ColorDistance(const FColor& A, const FColor& B)
	{
		return FMath::Max3(FMath::Abs(int32(A.R) - int32(B.R)), FMath::Abs(int32(A.G) - int32(B.G)),
			FMath::Abs(int32(A.B) - int32(B.B)));
	}

	void CheckFillAndGlyph(FAutomationTestBase& Test, const TArray<FColor>& Pixels, const FBadgeRegion& Area)
	{
		const FVector2D Center((Area.Badge.Left + Area.Badge.Right) * .5f, (Area.Badge.Top + Area.Badge.Bottom) * .5f);
		const FVector2D Radius = Area.Badge.GetSize() * .34f;
		int32 Interior = 0, Fill = 0, ContrastGlyph = 0;
		const int32 Left = FMath::Clamp(FMath::FloorToInt(Center.X - Radius.X), 0, Width);
		const int32 Right = FMath::Clamp(FMath::CeilToInt(Center.X + Radius.X), 0, Width);
		const int32 Top = FMath::Clamp(FMath::FloorToInt(Center.Y - Radius.Y), 0, Height);
		const int32 Bottom = FMath::Clamp(FMath::CeilToInt(Center.Y + Radius.Y), 0, Height);
		for (int32 Y = Top; Y < Bottom; ++Y)
		{
			for (int32 X = Left; X < Right; ++X)
			{
				// 흰 외곽선을 제외한 원 내부만 판정한다. 월드/HUD 배경의 색이나 면적은 조건에 넣지 않는다.
				if (FMath::Square((X + .5 - Center.X) / Radius.X) + FMath::Square((Y + .5 - Center.Y) / Radius.Y) > 1.0) continue;
				++Interior;
				const FColor& Pixel = Pixels[Y * Width + X];
				Fill += ColorDistance(Pixel, Area.ExpectedFill) <= 16 ? 1 : 0;
				if (ColorDistance(Pixel, FColor::White) > 20 || ColorDistance(Pixel, Area.ExpectedFill) < 64) continue;
				// 중앙의 흰 글자 옆에 실제 채움색이 있어야 한다. 흰 원 전체를 느낌표로 오인하지 않는다.
				bool bHasColoredNeighbor = false;
				for (int32 OffsetY = -3; OffsetY <= 3 && !bHasColoredNeighbor; ++OffsetY)
				{
					for (int32 OffsetX = -3; OffsetX <= 3; ++OffsetX)
					{
						const int32 NX = X + OffsetX, NY = Y + OffsetY;
						if (NX >= 0 && NX < Width && NY >= 0 && NY < Height &&
							ColorDistance(Pixels[NY * Width + NX], Area.ExpectedFill) <= 16)
						{
							bHasColoredNeighbor = true;
							break;
						}
					}
				}
				ContrastGlyph += bHasColoredNeighbor ? 1 : 0;
			}
		}
		Test.TestTrue(Area.Name + TEXT(" 원 내부가 설정한 배지 채움색으로 그려진다"),
			Interior > 0 && Fill >= FMath::Max(16, FMath::RoundToInt(Interior * .55f)));
		Test.TestTrue(Area.Name + TEXT(" 중앙 흰 느낌표가 채움색과 대비되어 보인다"), ContrastGlyph >= 3);
		Test.AddInfo(FString::Printf(TEXT("%s 배지 예상 채움 RGB=(%d,%d,%d), 내부=%d 채움 일치=%d 흰 글자 대비=%d"),
			*Area.Name, Area.ExpectedFill.R, Area.ExpectedFill.G, Area.ExpectedFill.B, Interior, Fill, ContrastGlyph));
	}

	int32 CountDifference(const TArray<FColor>& A, const TArray<FColor>& B, const FSlateRect& Rect)
	{
		const int32 Left = FMath::Clamp(FMath::FloorToInt(Rect.Left), 0, Width);
		const int32 Right = FMath::Clamp(FMath::CeilToInt(Rect.Right), 0, Width);
		const int32 Top = FMath::Clamp(FMath::FloorToInt(Rect.Top), 0, Height);
		const int32 Bottom = FMath::Clamp(FMath::CeilToInt(Rect.Bottom), 0, Height);
		int32 Count = 0;
		for (int32 Y = Top; Y < Bottom; ++Y)
			for (int32 X = Left; X < Right; ++X)
				Count += Different(A[Y * Width + X], B[Y * Width + X]) ? 1 : 0;
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepRequiredAttentionRenderedBadges,
	"FestaFiesta.UI.PrepHud.RequiredAttentionRenderedBadges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)

bool FFF_PrepRequiredAttentionRenderedBadges::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFPrepRequiredAttentionRender;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	auto* HudClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepHudBar>(nullptr,
		TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar.WBP_FF_PrepHudBar_C")));
	auto* TabClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepHudTabButton>(nullptr,
		TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudTabButton.WBP_FF_PrepHudTabButton_C")));
	if (!TestNotNull(TEXT("표시 검사용 에디터 월드"), World) || !TestNotNull(TEXT("실제 준비 HUD WBP"), HudClass) ||
		!TestNotNull(TEXT("실제 준비 탭 WBP"), TabClass)) return false;
	if (!TestNull(TEXT("게임 상태와 분리된 표시 검사"), World->GetGameInstance()) ||
		!TestFalse(TEXT("PIE 실행 중에는 표시 fixture를 만들지 않는다"), GEditor->PlayWorld != nullptr)) return false;
	const bool bHudInitialize = HudClass->bCanCallInitializedWithoutPlayerContext;
	const bool bTabInitialize = TabClass->bCanCallInitializedWithoutPlayerContext;
	const bool bHudDirty = HudClass->GetOutermost()->IsDirty();
	const bool bTabDirty = TabClass->GetOutermost()->IsDirty();
	const TWeakObjectPtr<UFF_PrepHudBar> PreviousActive = UFF_PrepHudBar::ActiveBar;
	HudClass->bCanCallInitializedWithoutPlayerContext = true;
	TabClass->bCanCallInitializedWithoutPlayerContext = true;
	ON_SCOPE_EXIT
	{
		HudClass->bCanCallInitializedWithoutPlayerContext = bHudInitialize;
		TabClass->bCanCallInitializedWithoutPlayerContext = bTabInitialize;
		UFF_PrepHudBar::ActiveBar = PreviousActive;
		TestEqual(TEXT("저장 HUD 에셋을 변경하지 않았다"), HudClass->GetOutermost()->IsDirty(), bHudDirty);
		TestEqual(TEXT("저장 탭 에셋을 변경하지 않았다"), TabClass->GetOutermost()->IsDirty(), bTabDirty);
	};
	TStrongObjectPtr<UFF_PrepHudBar> Hud(CreateWidget<UFF_PrepHudBar>(World, HudClass));
	if (!TestNotNull(TEXT("저장된 계층 그대로 생성한 HUD"), Hud.Get())) return false;
	AddExpectedError(FString::Printf(TEXT("[%s] ViewModel 을 찾지 못했다. 금화·스태프·단계가 전부 비어 보인다."),
		*Hud->GetName()), EAutomationExpectedErrorFlags::Contains, 1, false);
	// 기존 Sky 렌더 검사의 전체 화면 Surface와 프로젝트 DPI를 사용한다.
	// 부모의 clipping, 슬롯, 패딩, 높이, 선택 상태는 변경하지 않는다.
	const float Dpi = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(Width, Height));
	const TSharedRef<SWidget> Surface = SNew(SDPIScaler).DPIScale(Dpi)
	[
		SNew(SOverlay) + SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)[Hud->TakeWidget()]
	];
	auto* Check = Cast<UFF_PrepHudTabButton>(Hud->GetWidgetFromName(TEXT("Tab_Check")));
	auto* Staff = Cast<UFF_PrepHudTabButton>(Hud->GetWidgetFromName(TEXT("Tab_Staff")));
	UWidget* Menu = Hud->GetWidgetFromName(TEXT("Border_MenuRow"));
	UWidget* Status = Hud->GetWidgetFromName(TEXT("Border_StatusRow"));
	if (!TestNotNull(TEXT("실제 점검 탭"), Check) || !TestNotNull(TEXT("실제 스태프 탭"), Staff) ||
		!TestNotNull(TEXT("실제 메뉴 부모"), Menu) || !TestNotNull(TEXT("실제 상태 행"), Status)) return false;
	FAssetCompilingManager::Get().FinishAllCompilation();
	FlushRenderingCommands();
	FWidgetRenderer Renderer(false, true);
	TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(
		FVector2D(Width, Height), TF_Bilinear, true));
	if (!TestNotNull(TEXT("실제 RHI 렌더 타깃"), Target.Get())) return false;
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("PrepRequiredAttention") /
		(TEXT("Render-") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
	if (!TestTrue(TEXT("이번 실행의 캡처 폴더 생성"), IFileManager::Get().MakeDirectory(*Directory, true))) return false;
	const auto Capture = [&](bool bEnabled, const TCHAR* Name, TArray<FColor>& Pixels)
	{
		Check->SetRequiredAttention(bEnabled);
		Staff->SetRequiredAttention(bEnabled);
		for (int32 Frame = 0; Frame < 4; ++Frame)
		{
			Hud->ForceLayoutPrepass();
			Renderer.DrawWidget(Target.Get(), Surface, FVector2D(Width, Height), 1.f / 60.f, false);
		}
		FlushRenderingCommands();
		FReadSurfaceDataFlags Flags;
		Flags.SetLinearToGamma(false);
		if (!TestTrue(FString(Name) + TEXT(" 픽셀 읽기"),
			Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags) && Pixels.Num() == Width * Height)) return false;
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Width, Height, Pixels, Png);
		const FString Path = Directory / (FString(Name) + TEXT("_1920x1080.png"));
		return TestTrue(FString(Name) + TEXT(" 캡처 저장"), FFileHelper::SaveArrayToFile(Png, *Path));
	};
	TArray<FColor> OffBefore, On, OffAfter;
	if (!Capture(false, TEXT("BadgesOffBefore"), OffBefore)) return false;
	const FSlateRect MenuRect = Menu->GetCachedGeometry().GetLayoutBoundingRect();
	const FSlateRect StatusRect = Status->GetCachedGeometry().GetLayoutBoundingRect();
	const TArray<FBadgeRegion> Regions { Region(*Check), Region(*Staff) };
	if (!Capture(true, TEXT("BadgesOn"), On)) return false;
	for (const FBadgeRegion& Area : Regions)
	{
		const bool bInViewport = Area.Badge.Left >= 0.f && Area.Badge.Top >= 0.f &&
			Area.Badge.Right <= Width && Area.Badge.Bottom <= Height;
		TestTrue(Area.Name + TEXT(" 실제 클릭 탭 크기가 있다"), Area.Tab.GetSize().X >= 44.f && Area.Tab.GetSize().Y >= 32.f);
		TestTrue(Area.Name + TEXT(" 배지가 화면 안에 들어온다"), bInViewport);
		const FSlateRect TopOverflow(Area.Badge.Left, Area.Badge.Top, Area.Badge.Right, Area.Tab.Top - 1.f);
		const FSlateRect RightOverflow(FMath::FloorToFloat(Area.Tab.Right) + 1.f, Area.Badge.Top,
			Area.Badge.Right, Area.Badge.Bottom);
		const int32 Changed = CountDifference(OffBefore, On, Area.Badge);
		const int32 OverflowChanged = CountDifference(OffBefore, On, TopOverflow);
		const int32 RightChanged = CountDifference(OffBefore, On, RightOverflow);
		const float BadgeArea = Area.Badge.GetSize().X * Area.Badge.GetSize().Y;
		// 배지 전체의 충분한 면적과 탭 바깥 위쪽 원호가 모두 실제로 보여야 한다.
		// NativePaint가 불리지 않거나 조상 clipping이 탭 경계에서 자르면 이 조건이 실패한다.
		TestTrue(Area.Name + TEXT(" 우상단에 배지 픽셀이 나타난다"), Changed >= FMath::Max(20, FMath::RoundToInt(BadgeArea * .35f)));
		TestTrue(Area.Name + TEXT(" 탭 윗변 밖 원호가 부모에 잘리지 않는다"),
			OverflowChanged >= FMath::Max(10, FMath::RoundToInt(BadgeArea * .09f)));
		TestTrue(Area.Name + TEXT(" 탭 오른쪽 밖 원호가 부모에 잘리지 않는다"), RightChanged >= 2);
		CheckFillAndGlyph(*this, On, Area);
		AddInfo(FString::Printf(TEXT("%s 배지 픽셀=%d 위쪽 넘침 픽셀=%d 오른쪽 넘침 픽셀=%d 탭=(%.1f,%.1f)-(%.1f,%.1f) 배지=(%.1f,%.1f)-(%.1f,%.1f)"),
			*Area.Name, Changed, OverflowChanged, RightChanged, Area.Tab.Left, Area.Tab.Top, Area.Tab.Right, Area.Tab.Bottom,
			Area.Badge.Left, Area.Badge.Top, Area.Badge.Right, Area.Badge.Bottom));
	}
	TestTrue(TEXT("점검 배지 표시가 탭 크기·위치를 바꾸지 않는다"), SameRect(Regions[0].Tab, Region(*Check).Tab));
	TestTrue(TEXT("스태프 배지 표시가 탭 크기·위치를 바꾸지 않는다"), SameRect(Regions[1].Tab, Region(*Staff).Tab));
	TestTrue(TEXT("배지 표시가 메뉴·상태 행 크기·위치를 바꾸지 않는다"),
		SameRect(MenuRect, Menu->GetCachedGeometry().GetLayoutBoundingRect()) &&
		SameRect(StatusRect, Status->GetCachedGeometry().GetLayoutBoundingRect()));
	if (!Capture(false, TEXT("BadgesOffAfter"), OffAfter)) return false;
	TestFalse(TEXT("점검 배지 해제 상태"), Check->HasRequiredAttention());
	TestFalse(TEXT("스태프 배지 해제 상태"), Staff->HasRequiredAttention());
	for (const FBadgeRegion& Area : Regions)
	{
		TestEqual(Area.Name + TEXT(" 해제 후 배지 픽셀이 남지 않는다"), CountDifference(OffBefore, OffAfter, Area.Badge), 0);
	}
	AddInfo(TEXT("실제 HUD 배지 캡처: ") + FPaths::ConvertRelativePathToFull(Directory));
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
