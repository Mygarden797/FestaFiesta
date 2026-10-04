// 창/DPI·하단 HUD 부모와 무관한 라이브 7패널 중앙 배치 및 실제 PIE 캡처.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Layout/Geometry.h"
#include "UI/LiveHUD/FF_LiveHudTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LivePanelCenterGeometryTest,
	"FestaFiesta.UI.LiveHUD.Center.ViewportGeometry",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_LivePanelCenterGeometryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FFF_LivePanelLayout Layout;
	const FGeometry Viewport = FGeometry::MakeRoot(FVector2f(1920.f, 1080.f),
		FSlateLayoutTransform(1.25f, FVector2f(400.f, 200.f)));
	const FGeometry BottomCanvas = FGeometry::MakeRoot(FVector2f(1920.f, 68.f),
		FSlateLayoutTransform(1.25f, FVector2f(500.f, 1465.f)));
	FVector2D Position, Size;
	TestTrue(TEXT("높이가 68인 HUD 부모에서도 화면 중앙을 구한다"),
		Layout.TryResolveCenteredLayout(Viewport, BottomCanvas, 68.f, 12.f, Position, Size));
	TestTrue(TEXT("창 원점을 포함한 실제 중앙에 놓인다"),
		FVector2D(BottomCanvas.LocalToAbsolute(FVector2D(960.0, 34.0) + Position))
			.Equals(FVector2D(1600.0, 875.0), 0.01));
	TestEqual(TEXT("보통 창에서 편집한 크기를 유지한다"), Size, FVector2D(520.0, 620.0));
	Layout.Offset = FVector2D(120.0, -80.0);
	TestTrue(TEXT("편집한 오프셋을 적용한다"),
		Layout.TryResolveCenteredLayout(Viewport, BottomCanvas, 68.f, 12.f, Position, Size));
	TestTrue(TEXT("DPI가 오프셋에 한 번만 적용된다"),
		FVector2D(BottomCanvas.LocalToAbsolute(FVector2D(960.0, 34.0) + Position))
			.Equals(FVector2D(1750.0, 775.0), 0.01));

	const FGeometry OtherViewport = FGeometry::MakeRoot(FVector2f(1600.f, 900.f),
		FSlateLayoutTransform(1.5f, FVector2f(320.f, 90.f)));
	const FGeometry OtherCanvas = FGeometry::MakeRoot(FVector2f(1920.f, 68.f),
		FSlateLayoutTransform(0.75f, FVector2f(450.f, 1290.f)));
	TestTrue(TEXT("서로 다른 viewport/Canvas 배율도 변환한다"),
		Layout.TryResolveCenteredLayout(OtherViewport, OtherCanvas, 68.f, 12.f, Position, Size));
	TestTrue(TEXT("다른 배율에서도 오프셋은 Canvas 단위다"),
		FVector2D(OtherCanvas.LocalToAbsolute(FVector2D(960.0, 34.0) + Position))
			.Equals(FVector2D(1610.0, 705.0), 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LivePanelCenterResizeTest,
	"FestaFiesta.UI.LiveHUD.Center.ResizeAndSavedLayout",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_LivePanelCenterResizeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FFF_LivePanelLayout Layout;
	Layout.Width = 1800.f;
	Layout.Height = 950.f;
	Layout.Offset = FVector2D(500.0, -400.0);
	const FGeometry Small = FGeometry::MakeRoot(FVector2f(1280.f, 720.f), FSlateLayoutTransform());
	const FGeometry Canvas = FGeometry::MakeRoot(FVector2f(1280.f, 64.f),
		FSlateLayoutTransform(FVector2f(0.f, 656.f)));
	FVector2D Position(11.0, 22.0), Size(33.0, 44.0);
	TestFalse(TEXT("최초 viewport geometry가 없으면 숨길 수 있도록 대기한다"),
		Layout.TryResolveCenteredLayout(FGeometry(), Canvas, 64.f, 12.f, Position, Size));
	TestEqual(TEXT("실패 시 좌표를 덮지 않는다"), Position, FVector2D(11.0, 22.0));
	TestEqual(TEXT("실패 시 크기를 덮지 않는다"), Size, FVector2D(33.0, 44.0));
	TestFalse(TEXT("최초 Canvas geometry도 기다린다"),
		Layout.TryResolveCenteredLayout(Small, FGeometry(), 64.f, 12.f, Position, Size));
	TestTrue(TEXT("작은 창에 표시 크기를 맞춘다"),
		Layout.TryResolveCenteredLayout(Small, Canvas, 64.f, 12.f, Position, Size));
	TestEqual(TEXT("닫기와 하단 HUD 영역을 남겨 둔다"), Size, FVector2D(1256.0, 632.0));
	const FVector2D Center = Canvas.LocalToAbsolute(FVector2D(640.0, 32.0) + Position);
	TestTrue(TEXT("위쪽 닫기 버튼 공간을 화면 안에 유지한다"), (Center - Size * 0.5).Equals(FVector2D(12.0, 12.0), 0.01));
	TestTrue(TEXT("패널 아래는 하단 메뉴보다 위에 있다"), Center.Y + Size.Y * 0.5 <= 644.01);
	TestEqual(TEXT("저장된 폭은 작은 창에 맞춰 덮어쓰지 않는다"), Layout.Width, 1800.f);
	TestEqual(TEXT("저장된 높이를 유지한다"), Layout.Height, 950.f);
	TestEqual(TEXT("드래그/Designer 오프셋도 유지한다"), Layout.Offset, FVector2D(500.0, -400.0));

	Layout.Offset = FVector2D::ZeroVector;
	const FGeometry Wide = FGeometry::MakeRoot(FVector2f(2560.f, 1440.f), FSlateLayoutTransform());
	TestTrue(TEXT("창을 넓히면 저장 크기로 돌아간다"),
		Layout.TryResolveCenteredLayout(Wide, Canvas, 64.f, 12.f, Position, Size));
	TestEqual(TEXT("임시 축소값이 다음 resize에 남지 않는다"), Size, FVector2D(1800.0, 950.0));
	TestTrue(TEXT("resize 후 새 화면 중앙을 쓴다"),
		FVector2D(Canvas.LocalToAbsolute(FVector2D(640.0, 32.0) + Position)).Equals(FVector2D(1280.0, 720.0), 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LivePanelCenterScopeTest,
	"FestaFiesta.UI.LiveHUD.Center.SevenTabsOnly",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_LivePanelCenterScopeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FFF_LivePanelLayout Layout;
	for (EFF_LiveTab Tab : { EFF_LiveTab::Operations, EFF_LiveTab::Facilities, EFF_LiveTab::Guests,
		EFF_LiveTab::Queues, EFF_LiveTab::SNS, EFF_LiveTab::Staff, EFF_LiveTab::Density })
	{
		TestTrue(TEXT("라이브 7패널은 viewport 중앙을 쓴다"), Layout.UsesViewportCenter(Tab));
	}
	TestFalse(TEXT("리포트의 기존 도킹을 보존한다"), Layout.UsesViewportCenter(EFF_LiveTab::Report));
	TestFalse(TEXT("피날레의 기존 도킹을 보존한다"), Layout.UsesViewportCenter(EFF_LiveTab::Finale));
	for (EFF_LivePanelDock Dock : { EFF_LivePanelDock::Designer, EFF_LivePanelDock::Left, EFF_LivePanelDock::Right })
	{
		Layout.Dock = Dock;
		TestFalse(TEXT("Designer/좌우 도킹 선택을 중앙으로 덮어쓰지 않는다"), Layout.UsesViewportCenter(EFF_LiveTab::Operations));
	}
	return true;
}

#if WITH_EDITOR
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/PanelWidget.h"
#include "Dom/JsonObject.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UI/LiveHUD/FF_LiveHudScreen.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/SViewport.h"

namespace FFLivePanelCenterProbe
{
	UWidget* FindChild(UWidget* Root, FName Name)
	{
		if (!Root || !Root->IsVisible()) { return nullptr; }
		if (Root->GetFName() == Name) { return Root; }
		if (UUserWidget* User = Cast<UUserWidget>(Root)) { return FindChild(User->GetRootWidget(), Name); }
		if (UPanelWidget* Panel = Cast<UPanelWidget>(Root))
		{
			for (UWidget* Child : Panel->GetAllChildren())
			{
				if (UWidget* Found = FindChild(Child, Name)) { return Found; }
			}
		}
		return nullptr;
	}

	void Vector(const TSharedRef<FJsonObject>& Json, const TCHAR* Name, const FVector2D& Value)
	{
		Json->SetArrayField(Name, { MakeShared<FJsonValueNumber>(Value.X), MakeShared<FJsonValueNumber>(Value.Y) });
	}

	FSlateRect Rect(UWidget* Widget)
	{
		if (!Widget) { return FSlateRect(); }
		const FGeometry Geometry = Widget->GetCachedGeometry();
		FVector2D Min, Max, Unused;
		USlateBlueprintLibrary::LocalToViewport(Widget, Geometry, FVector2D::ZeroVector, Min, Unused);
		USlateBlueprintLibrary::LocalToViewport(Widget, Geometry, Geometry.GetLocalSize(), Max, Unused);
		return FSlateRect(Min.X, Min.Y, Max.X, Max.Y);
	}

	void RectField(const TSharedRef<FJsonObject>& Json, const TCHAR* Name, const FSlateRect& Value)
	{
		Json->SetArrayField(Name, { MakeShared<FJsonValueNumber>(Value.Left), MakeShared<FJsonValueNumber>(Value.Top),
			MakeShared<FJsonValueNumber>(Value.Right), MakeShared<FJsonValueNumber>(Value.Bottom) });
	}

	bool Contains(const FSlateRect& Outer, const FSlateRect& Inner)
	{
		return Outer.IsValid() && Inner.IsValid() && !Outer.IsEmpty() && !Inner.IsEmpty()
			&& Inner.Left >= Outer.Left - .75f && Inner.Top >= Outer.Top - .75f
			&& Inner.Right <= Outer.Right + .75f && Inner.Bottom <= Outer.Bottom + .75f;
	}

	void Capture(const TArray<FString>& Args, UWorld* ConsoleWorld)
	{
		const FString Tag = Args.Num() == 1 || Args.Num() == 3 ? Args[0] : FString();
		float DeltaX = 0.f, DeltaY = 0.f;
		if (Args.Num() == 3 && (!LexTryParseString(DeltaX, *Args[1]) || !LexTryParseString(DeltaY, *Args[2])
			|| !FMath::IsFinite(DeltaX) || !FMath::IsFinite(DeltaY))) { return; }
		bool bSafeTag = !Tag.IsEmpty() && Tag.Len() <= 96;
		for (TCHAR Character : Tag)
		{
			bSafeTag &= (Character >= TEXT('a') && Character <= TEXT('z'))
				|| (Character >= TEXT('A') && Character <= TEXT('Z'))
				|| (Character >= TEXT('0') && Character <= TEXT('9')) || Character == TEXT('-') || Character == TEXT('_');
		}
		if (!bSafeTag)
		{
			UE_LOG(LogTemp, Warning, TEXT("FF.LivePanelCenterProbe: 태그는 영문·숫자·하이픈·밑줄 1~96자입니다."));
			return;
		}
		UFF_LiveHudScreen* Hud = nullptr;
		int32 Candidates = 0;
		for (TObjectIterator<UFF_LiveHudScreen> It; It; ++It)
		{
			UWorld* CandidateWorld = It->GetWorld();
			if (It->IsTemplate() || !CandidateWorld || CandidateWorld->WorldType != EWorldType::PIE
				|| !It->IsVisible() || !It->GetSelection().bHasSelection
				|| (ConsoleWorld && ConsoleWorld->WorldType == EWorldType::PIE && ConsoleWorld != CandidateWorld)) { continue; }
			Hud = *It;
			++Candidates;
		}
		const TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		Report->SetStringField(TEXT("tag"), Tag);
		Report->SetBoolField(TEXT("ok"), false);
		Report->SetBoolField(TEXT("capture_ok"), false);
		TArray<TSharedPtr<FJsonValue>> Failures;
		auto Fail = [&Failures](const FString& Text) { Failures.Add(MakeShared<FJsonValueString>(Text)); };
		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("LivePanelsSky"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		UWorld* World = Hud ? Hud->GetWorld() : nullptr;
		APlayerController* Player = Hud ? Hud->GetOwningPlayer() : nullptr;
		UWidget* Host = Hud ? Hud->GetWidgetFromName(TEXT("Panel_PanelHost")) : nullptr;
		UWidget* Canvas = Host ? Host->GetParent() : nullptr;
		if (Candidates != 1 || !World || !Player || !Host || !Canvas)
		{
			Fail(TEXT("열린 패널을 가진 활성 PIE Live HUD 하나와 Canvas가 필요합니다."));
		}
		else
		{
			// 에디터 검증 전용. 실제 드래그의 네이티브 이동 경로를 호출하고 다음 paint에서 측정한다.
			Report->SetBoolField(TEXT("nudge_allowed"), Hud->IsPanelDragAllowed());
			if (Args.Num() == 3) { Hud->NudgePanelPosition(FVector2D(DeltaX, DeltaY)); }
			int32 Width = 0, Height = 0;
			Player->GetViewportSize(Width, Height);
			const FSlateRect View(0.f, 0.f, Width, Height);
			const FSlateRect HostRect = Rect(Host);
			const FSlateRect MenuRect = Rect(Hud->GetWidgetFromName(TEXT("SizeBox_MenuRow")));
			UWidget* Close = FindChild(Host, TEXT("Button_Close"));
			const FSlateRect CloseRect = Rect(Close);
			const FFF_LivePanelLayout Layout = Hud->GetOpenPanelLayout();
			const FGeometry ViewGeometry = UWidgetLayoutLibrary::GetViewportWidgetGeometry(Hud);
			const FGeometry CanvasGeometry = Canvas->GetCachedGeometry();
			Vector(Report, TEXT("host_center_absolute"), Host->GetCachedGeometry().LocalToAbsolute(
				FVector2D(Host->GetCachedGeometry().GetLocalSize()) * 0.5));
			Vector(Report, TEXT("viewport_center_absolute"), ViewGeometry.LocalToAbsolute(
				FVector2D(ViewGeometry.GetLocalSize()) * 0.5));
			Vector(Report, TEXT("canvas_offset_absolute"), FVector2D(CanvasGeometry.LocalToAbsolute(Layout.Offset))
				- FVector2D(CanvasGeometry.LocalToAbsolute(FVector2D::ZeroVector)));
			Vector(Report, TEXT("controller_viewport_px"), FVector2D(Width, Height));
			Vector(Report, TEXT("layout_size"), FVector2D(Layout.Width, Layout.Height));
			Vector(Report, TEXT("layout_offset"), Layout.Offset);
			Vector(Report, TEXT("host_size"), Host->GetCachedGeometry().GetLocalSize());
			Report->SetNumberField(TEXT("dpi"), UWidgetLayoutLibrary::GetViewportScale(Hud));
			Report->SetNumberField(TEXT("bottom_height"), Hud->GetBottomBarHeight());
			Report->SetNumberField(TEXT("tab"), static_cast<int32>(Hud->GetSelection().Tab));
			RectField(Report, TEXT("host_pixel_bounds"), HostRect);
			RectField(Report, TEXT("menu_pixel_bounds"), MenuRect);
			RectField(Report, TEXT("close_pixel_bounds"), CloseRect);
			if (!Host->IsVisible() || !Contains(View, HostRect)) { Fail(TEXT("패널이 숨겨졌거나 viewport를 벗어납니다.")); }
			if (!Close || !Contains(HostRect, CloseRect)) { Fail(TEXT("닫기 버튼이 없거나 패널을 벗어납니다.")); }
			if (Layout.UsesViewportCenter(Hud->GetSelection().Tab) && HostRect.Bottom > MenuRect.Top + .75f)
			{
				Fail(TEXT("중앙 패널이 하단 메뉴와 겹칩니다."));
			}
			UGameViewportClient* Client = World->GetGameViewport();
			const TSharedPtr<SViewport> Viewport = Client ? Client->GetGameViewportWidget() : nullptr;
			TArray<FColor> Pixels;
			FIntVector Size = FIntVector::ZeroValue;
			if (Viewport.IsValid() && FSlateApplication::IsInitialized()
				&& FSlateApplication::Get().TakeScreenshot(Viewport.ToSharedRef(), Pixels, Size)
				&& Width > 0 && Height > 0 && Size.X == Width && Size.Y == Height
				&& static_cast<int64>(Pixels.Num()) == static_cast<int64>(Width) * Height)
			{
				for (FColor& Pixel : Pixels) { Pixel.A = 255; }
				TArray64<uint8> Png;
				FImageUtils::PNGCompressImageArray(Width, Height, Pixels, Png);
				const FString ImagePath = FPaths::Combine(Directory, TEXT("native-") + Tag + TEXT(".png"));
				Report->SetBoolField(TEXT("capture_ok"), !Png.IsEmpty() && FFileHelper::SaveArrayToFile(Png, *ImagePath));
				Report->SetStringField(TEXT("capture_path"), FPaths::ConvertRelativePathToFull(ImagePath));
			}
			if (!Report->GetBoolField(TEXT("capture_ok"))) { Fail(TEXT("실제 PIE GameViewportClient 이미지 캡처에 실패했습니다.")); }
		}
		Report->SetArrayField(TEXT("failures"), Failures);
		Report->SetBoolField(TEXT("ok"), Failures.IsEmpty());
		FString Json;
		if (FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json)))
		{
			FFileHelper::SaveStringToFile(Json, *FPaths::Combine(Directory, TEXT("native-") + Tag + TEXT(".json")),
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}
		UE_LOG(LogTemp, Display, TEXT("FF.LivePanelCenterProbe: tag=%s ok=%s failures=%d"),
			*Tag, Failures.IsEmpty() ? TEXT("true") : TEXT("false"), Failures.Num());
	}

	FAutoConsoleCommandWithWorldAndArgs Command(TEXT("FF.LivePanelCenterProbe"),
		TEXT("FF.LivePanelCenterProbe <tag> [dx dy] — 실제 PIE Live 경계/PNG 기록. 선택 인자는 드래그 이동량 검증용입니다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Capture));
}
#endif // WITH_EDITOR
#endif // WITH_DEV_AUTOMATION_TESTS
