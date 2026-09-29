// 하단 높이만 받는 HUD에서도 화면 중앙과 Designer 오프셋을 같은 좌표계로 유지한다.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Layout/Geometry.h"
#include "UI/PrepHUD/FF_PrepHudTypes.h"

#if WITH_EDITOR
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/PanelWidget.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UI/PrepHUD/FF_PrepHudBar.h"

namespace FFPrepPanelCenterProbe
{
	void SetVector(const TSharedRef<FJsonObject>& Report, const TCHAR* Name, const FVector2D& Value)
	{
		Report->SetArrayField(Name, { MakeShared<FJsonValueNumber>(Value.X), MakeShared<FJsonValueNumber>(Value.Y) });
	}

	bool HasSize(const FGeometry& Geometry)
	{
		const FVector2D Size = Geometry.GetLocalSize();
		return !Size.ContainsNaN() && Size.X > 0.0 && Size.Y > 0.0;
	}

	// Python FGeometry 복사 경로를 거치지 않고 실제 PIE 위젯의 Slate 좌표를 읽는다.
	// 상태·레이아웃을 수정하지 않으며, 결과만 프로젝트 Saved 아래에 기록한다.
	void Capture(const TArray<FString>& Args, UWorld* ConsoleWorld)
	{
		(void)ConsoleWorld;
		const TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		Report->SetStringField(TEXT("tag"), Args.Num() > 0 ? FString::Join(Args, TEXT(" ")) : TEXT("untagged"));
		Report->SetBoolField(TEXT("ok"), false);
		Report->SetStringField(TEXT("error"), TEXT(""));
		for (const TCHAR* Field : { TEXT("host_size"), TEXT("host_center_absolute"), TEXT("viewport_size"),
			TEXT("viewport_center_absolute"), TEXT("canvas_origin_absolute"), TEXT("canvas_offset_absolute"),
			TEXT("controller_viewport_px"), TEXT("layout_size"), TEXT("layout_offset") })
		{
			SetVector(Report, Field, FVector2D::ZeroVector);
		}
		Report->SetNumberField(TEXT("dpi"), 0.0);
		Report->SetArrayField(TEXT("host_pixel_bounds"), { MakeShared<FJsonValueNumber>(0.0),
			MakeShared<FJsonValueNumber>(0.0), MakeShared<FJsonValueNumber>(0.0), MakeShared<FJsonValueNumber>(0.0) });

		UFF_PrepHudBar* Bar = UFF_PrepHudBar::ActiveBar.Get();
		UWorld* World = Bar ? Bar->GetWorld() : nullptr;
		UWidget* Host = Bar ? Bar->GetWidgetFromName(TEXT("Panel_PanelHost")) : nullptr;
		UWidget* Canvas = Host ? Host->GetParent() : nullptr;
		APlayerController* Controller = Bar ? Bar->GetOwningPlayer() : nullptr;
		if (!Bar || !World || World->WorldType != EWorldType::PIE || !Controller)
		{
			Report->SetStringField(TEXT("error"), TEXT("활성 PIE Prep HUD와 소유 플레이어가 필요합니다."));
		}
		else if (!Bar->GetSelection().bHasSelection || !Host || !Canvas)
		{
			Report->SetStringField(TEXT("error"), TEXT("열린 패널과 Canvas 호스트가 필요합니다."));
		}
		else
		{
			const FGeometry ViewportGeometry = UWidgetLayoutLibrary::GetViewportWidgetGeometry(Bar);
			const FGeometry HostGeometry = Host->GetCachedGeometry();
			const FGeometry CanvasGeometry = Canvas->GetCachedGeometry();
			const FFF_PrepPanelLayout Layout = Bar->GetOpenPanelLayout();
			int32 PixelWidth = 0;
			int32 PixelHeight = 0;
			Controller->GetViewportSize(PixelWidth, PixelHeight);
			SetVector(Report, TEXT("controller_viewport_px"), FVector2D(PixelWidth, PixelHeight));
			Report->SetNumberField(TEXT("dpi"), UWidgetLayoutLibrary::GetViewportScale(Bar));
			SetVector(Report, TEXT("layout_size"), Layout.Size);
			SetVector(Report, TEXT("layout_offset"), Layout.Offset);
			SetVector(Report, TEXT("host_size"), HostGeometry.GetLocalSize());
			SetVector(Report, TEXT("viewport_size"), ViewportGeometry.GetLocalSize());
			SetVector(Report, TEXT("canvas_size"), CanvasGeometry.GetLocalSize());
			SetVector(Report, TEXT("hud_size"), Bar->GetCachedGeometry().GetLocalSize());
			Report->SetStringField(TEXT("hud_name"), Bar->GetPathName());
			if (!HasSize(ViewportGeometry) || !HasSize(HostGeometry) || !HasSize(CanvasGeometry)
				|| PixelWidth <= 0 || PixelHeight <= 0 || !Host->IsVisible())
			{
				Report->SetStringField(TEXT("error"), TEXT("PIE Slate geometry가 아직 유효하지 않거나 패널이 숨겨져 있습니다."));
			}
			else
			{
				const FVector2D HostSize = HostGeometry.GetLocalSize();
				const FVector2D ViewportSize = ViewportGeometry.GetLocalSize();
				const FVector2D HostCenter = HostGeometry.LocalToAbsolute(HostSize * 0.5);
				const FVector2D ViewportCenter = ViewportGeometry.LocalToAbsolute(ViewportSize * 0.5);
				const FVector2D CanvasOrigin = CanvasGeometry.LocalToAbsolute(FVector2D::ZeroVector);
				const FVector2D OffsetAbsolute = FVector2D(CanvasGeometry.LocalToAbsolute(Layout.Offset)) - CanvasOrigin;
				FVector2D TopLeftPixels, BottomRightPixels, UnusedViewportPosition;
				USlateBlueprintLibrary::LocalToViewport(Bar, HostGeometry, FVector2D::ZeroVector,
					TopLeftPixels, UnusedViewportPosition);
				USlateBlueprintLibrary::LocalToViewport(Bar, HostGeometry, HostSize,
					BottomRightPixels, UnusedViewportPosition);
				SetVector(Report, TEXT("host_center_absolute"), HostCenter);
				SetVector(Report, TEXT("viewport_center_absolute"), ViewportCenter);
				SetVector(Report, TEXT("canvas_origin_absolute"), CanvasOrigin);
				SetVector(Report, TEXT("canvas_offset_absolute"), OffsetAbsolute);
				Report->SetArrayField(TEXT("host_pixel_bounds"), { MakeShared<FJsonValueNumber>(TopLeftPixels.X),
					MakeShared<FJsonValueNumber>(TopLeftPixels.Y), MakeShared<FJsonValueNumber>(BottomRightPixels.X),
					MakeShared<FJsonValueNumber>(BottomRightPixels.Y) });
				const bool bFinite = !HostCenter.ContainsNaN() && !ViewportCenter.ContainsNaN()
					&& !CanvasOrigin.ContainsNaN() && !OffsetAbsolute.ContainsNaN()
					&& !TopLeftPixels.ContainsNaN() && !BottomRightPixels.ContainsNaN();
				Report->SetBoolField(TEXT("ok"), bFinite);
				if (!bFinite)
				{
					Report->SetStringField(TEXT("error"), TEXT("Slate 좌표에 유효하지 않은 수치가 있습니다."));
				}
			}
		}

		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PrepPanelCenter"));
		const FString Path = FPaths::Combine(Directory, TEXT("native-geometry.json"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		const bool bWritten = FJsonSerializer::Serialize(Report, Writer)
			&& FFileHelper::SaveStringToFile(Json, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		UE_LOG(LogTemp, Display, TEXT("FF.PrepPanelCenterProbe: tag=%s ok=%s saved=%s error=%s"),
			*Report->GetStringField(TEXT("tag")), Report->GetBoolField(TEXT("ok")) ? TEXT("true") : TEXT("false"),
			bWritten ? *Path : TEXT("기록 실패"), *Report->GetStringField(TEXT("error")));
	}

	FAutoConsoleCommandWithWorldAndArgs Command(
		TEXT("FF.PrepPanelCenterProbe"),
		TEXT("FF.PrepPanelCenterProbe <tag> — PIE 패널의 실제 Slate 좌표를 Saved/PrepPanelCenter/native-geometry.json에 기록한다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Capture));
}
#endif // WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepPanelCenterGeometryTest,
	"FestaFiesta.UI.PrepHUD.Center.ViewportGeometry",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_PrepPanelCenterGeometryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FFF_PrepPanelLayout Layout;
	const FGeometry Viewport = FGeometry::MakeRoot(FVector2f(1920.f, 1080.f),
		FSlateLayoutTransform(1.25f, FVector2f(400.f, 200.f)));
	// viewport와 다른 원점에 놓인 68px짜리 부모: 부모 중앙을 쓰면 패널이 화면 맨 아래에 열린다.
	const FGeometry BottomCanvas = FGeometry::MakeRoot(FVector2f(1920.f, 68.f),
		FSlateLayoutTransform(1.25f, FVector2f(500.f, 1465.f)));
	FVector2D Position;
	TestTrue(TEXT("68 높이의 부모에서도 중앙 좌표를 구한다"),
		Layout.TryResolveCenteredPosition(Viewport, BottomCanvas, Position));
	TestTrue(TEXT("Canvas 중앙 (960,34)를 뺀 0.5 앵커 위치는 (-80,-506)이다"),
		Position.Equals(FVector2D(-80.0, -506.0), 0.01));
	TestTrue(TEXT("중앙이 창 원점을 포함한 실제 화면 (1600, 875)에 놓인다"),
		FVector2D(BottomCanvas.LocalToAbsolute(FVector2D(960.0, 34.0) + Position))
			.Equals(FVector2D(1600.0, 875.0), 0.01));

	Layout.Offset = FVector2D(120.0, -80.0);
	Layout.Size = FVector2D(820.0, 470.0);
	TestTrue(TEXT("Designer 오프셋을 적용한다"),
		Layout.TryResolveCenteredPosition(Viewport, BottomCanvas, Position));
	TestTrue(TEXT("오프셋에 DPI를 한 번만 적용한다"),
		FVector2D(BottomCanvas.LocalToAbsolute(FVector2D(960.0, 34.0) + Position))
			.Equals(FVector2D(1750.0, 775.0), 0.01));
	TestEqual(TEXT("계산이 Designer 크기를 덮어쓰지 않는다"), Layout.Size, FVector2D(820.0, 470.0));
	TestEqual(TEXT("계산이 Designer 오프셋을 덮어쓰지 않는다"), Layout.Offset, FVector2D(120.0, -80.0));

	const FGeometry DifferentScaleViewport = FGeometry::MakeRoot(FVector2f(1600.f, 900.f),
		FSlateLayoutTransform(1.5f, FVector2f(320.f, 90.f)));
	const FGeometry DifferentScaleCanvas = FGeometry::MakeRoot(FVector2f(1920.f, 68.f),
		FSlateLayoutTransform(0.75f, FVector2f(450.f, 1290.f)));
	TestTrue(TEXT("viewport와 Canvas의 배율이 달라도 좌표를 구한다"),
		Layout.TryResolveCenteredPosition(DifferentScaleViewport, DifferentScaleCanvas, Position));
	TestTrue(TEXT("서로 다른 배율은 Geometry가 변환하고 오프셋은 Canvas 단위로 적용한다"),
		FVector2D(DifferentScaleCanvas.LocalToAbsolute(FVector2D(960.0, 34.0) + Position))
			.Equals(FVector2D(1610.0, 705.0), 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepPanelCenterResizeTest,
	"FestaFiesta.UI.PrepHUD.Center.ResizeAndFirstFrame",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_PrepPanelCenterResizeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FFF_PrepPanelLayout Layout;
	Layout.Offset = FVector2D(-90.0, 45.0);
	FVector2D Position(11.0, 22.0);
	const FGeometry Empty;
	const FGeometry Canvas = FGeometry::MakeRoot(FVector2f(1920.f, 68.f),
		FSlateLayoutTransform(FVector2f(0.f, 1012.f)));
	TestFalse(TEXT("처음 viewport 배치 전에는 화면 원점을 중앙으로 쓰지 않는다"),
		Layout.TryResolveCenteredPosition(Empty, Canvas, Position));
	TestEqual(TEXT("실패 시 기존 좌표를 잘못된 0으로 덮어쓰지 않는다"), Position, FVector2D(11.0, 22.0));
	const FGeometry WideViewport = FGeometry::MakeRoot(FVector2f(2560.f, 1080.f), FSlateLayoutTransform());
	TestFalse(TEXT("부모 Canvas 배치 전에도 대기한다"),
		Layout.TryResolveCenteredPosition(WideViewport, Empty, Position));
	TestTrue(TEXT("울트라와이드로 바뀐 화면 중앙을 다시 구한다"),
		Layout.TryResolveCenteredPosition(WideViewport, Canvas, Position));
	TestTrue(TEXT("부모 폭과 무관하게 새 화면 중앙 + 저장 오프셋을 쓴다"),
		Position.Equals(FVector2D(230.0, -461.0), 0.01));
	TestEqual(TEXT("창 변경 후에도 드래그/Designer 오프셋을 보존한다"), Layout.Offset, FVector2D(-90.0, 45.0));
	return true;
}

#endif
