// 하단 높이만 받는 HUD에서도 화면 중앙과 Designer 오프셋을 같은 좌표계로 유지한다.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Layout/Geometry.h"
#include "UI/PrepHUD/FF_PrepHudTypes.h"

#if WITH_EDITOR
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ImageUtils.h"
#include "Serialization/JsonSerializer.h"
#include "UI/PrepHUD/FF_PrepHudBar.h"
#include "UI/PrepHUD/FF_PrepPanelBase.h"
#include "Widgets/SViewport.h"

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
	TSharedRef<FJsonObject> BuildReport(const TArray<FString>& Args, UWorld* ConsoleWorld)
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

		return Report;
	}

	void Capture(const TArray<FString>& Args, UWorld* ConsoleWorld)
	{
		const TSharedRef<FJsonObject> Report = BuildReport(Args, ConsoleWorld);
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

namespace FFPrepPanelsIntegrationProbe
{
	constexpr float Tolerance = 0.75f;

	// 중첩 UserWidget 경계를 넘어서 찾는다. GetWidgetFromName만 쓰면 카드/공통 버튼 내부를 놓친다.
	UWidget* FindChild(UWidget* Root, FName Name)
	{
		if (!Root) { return nullptr; }
		if (Root->GetFName() == Name) { return Root; }
		if (UUserWidget* User = Cast<UUserWidget>(Root))
		{
			if (UWidget* Found = User->GetWidgetFromName(Name)) { return Found; }
			return FindChild(User->GetRootWidget(), Name);
		}
		if (UPanelWidget* Panel = Cast<UPanelWidget>(Root))
		{
			for (UWidget* Child : Panel->GetAllChildren())
			{
				if (UWidget* Found = FindChild(Child, Name)) { return Found; }
			}
		}
		return nullptr;
	}

	FSlateRect PixelRect(UWidget* Widget)
	{
		if (!Widget) { return FSlateRect(); }
		const FGeometry Geometry = Widget->GetCachedGeometry();
		FVector2D Min, Max, Unused;
		USlateBlueprintLibrary::LocalToViewport(Widget, Geometry, FVector2D::ZeroVector, Min, Unused);
		USlateBlueprintLibrary::LocalToViewport(Widget, Geometry, Geometry.GetLocalSize(), Max, Unused);
		return FSlateRect(Min.X, Min.Y, Max.X, Max.Y);
	}

	void SetRect(const TSharedRef<FJsonObject>& Row, const TCHAR* Name, const FSlateRect& Rect)
	{
		Row->SetArrayField(Name, { MakeShared<FJsonValueNumber>(Rect.Left), MakeShared<FJsonValueNumber>(Rect.Top),
			MakeShared<FJsonValueNumber>(Rect.Right), MakeShared<FJsonValueNumber>(Rect.Bottom) });
	}

	bool Contains(const FSlateRect& Outer, const FSlateRect& Inner)
	{
		return Outer.IsValid() && Inner.IsValid() && !Outer.IsEmpty() && !Inner.IsEmpty()
			&& Inner.Left >= Outer.Left - Tolerance && Inner.Top >= Outer.Top - Tolerance
			&& Inner.Right <= Outer.Right + Tolerance && Inner.Bottom <= Outer.Bottom + Tolerance;
	}

	bool Overlaps(const FSlateRect& A, const FSlateRect& B)
	{
		return A.IsValid() && B.IsValid() && !A.IsEmpty() && !B.IsEmpty()
			&& FMath::Min(A.Right, B.Right) - FMath::Max(A.Left, B.Left) > Tolerance
			&& FMath::Min(A.Bottom, B.Bottom) - FMath::Max(A.Top, B.Top) > Tolerance;
	}

	void Capture(const TArray<FString>& Args, UWorld* ConsoleWorld)
	{
		const TSharedRef<FJsonObject> Report = FFPrepPanelCenterProbe::BuildReport(Args, ConsoleWorld);
		TArray<TSharedPtr<FJsonValue>> Failures;
		auto Fail = [&Failures](const FString& Message) { Failures.Add(MakeShared<FJsonValueString>(Message)); };
		const FString Tag = Args.Num() == 1 ? Args[0] : FString();
		bool bSafeTag = !Tag.IsEmpty() && Tag.Len() <= 96;
		for (TCHAR Character : Tag)
		{
			bSafeTag &= (Character >= TEXT('a') && Character <= TEXT('z'))
				|| (Character >= TEXT('A') && Character <= TEXT('Z'))
				|| (Character >= TEXT('0') && Character <= TEXT('9')) || Character == TEXT('_') || Character == TEXT('-');
		}
		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PrepPanelsIntegration"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		Report->SetBoolField(TEXT("capture_ok"), false);
		FFPrepPanelCenterProbe::SetVector(Report, TEXT("capture_size"), FVector2D::ZeroVector);
		Report->SetStringField(TEXT("capture_path"), TEXT(""));
		Report->SetStringField(TEXT("capture_source"), TEXT("활성 Prep HUD 월드의 GameViewportClient->GetGameViewportWidget"));
		Report->SetStringField(TEXT("scope"), TEXT("실제 PIE Slate 경계/픽셀 캡처. 물리 입력·클릭 성공·가독성 수용을 대신하지 않는다."));
		if (!bSafeTag) { Fail(TEXT("태그는 1~96자의 영문·숫자·하이픈·밑줄 하나만 허용합니다.")); }
		if (!Report->GetBoolField(TEXT("ok"))) { Fail(Report->GetStringField(TEXT("error"))); }

		UFF_PrepHudBar* Bar = UFF_PrepHudBar::ActiveBar.Get();
		UWorld* World = Bar ? Bar->GetWorld() : nullptr;
		UGameViewportClient* Client = World ? World->GetGameViewport() : nullptr;
		const bool bWorldMatches = !ConsoleWorld || ConsoleWorld->WorldType != EWorldType::PIE || World == ConsoleWorld;
		if (!bWorldMatches)
		{
			Fail(TEXT("콘솔 PIE 월드와 활성 Prep HUD 월드가 다릅니다."));
		}
		if (bSafeTag && bWorldMatches && Report->GetBoolField(TEXT("ok")) && Client && FSlateApplication::IsInitialized())
		{
			const TSharedPtr<SViewport> Viewport = Client->GetGameViewportWidget();
			TArray<FColor> Pixels;
			FIntVector ImageSize = FIntVector::ZeroValue;
			const bool bCaptured = Viewport.IsValid()
				&& FSlateApplication::Get().TakeScreenshot(Viewport.ToSharedRef(), Pixels, ImageSize);
			int32 Width = 0, Height = 0;
			Bar->GetOwningPlayer()->GetViewportSize(Width, Height);
			FFPrepPanelCenterProbe::SetVector(Report, TEXT("capture_size"), FVector2D(ImageSize.X, ImageSize.Y));
			Report->SetStringField(TEXT("capture_world"), World->GetPathName());
			Report->SetStringField(TEXT("capture_viewport_client"), Client->GetPathName());
			if (bCaptured && ImageSize.X == Width && ImageSize.Y == Height
				&& static_cast<int64>(Pixels.Num()) == static_cast<int64>(Width) * Height)
			{
				// Slate의 창 버퍼 알파는 0일 수 있다. 일반 UE 스크린샷과 같이 불투명하게 저장한다.
				for (FColor& Pixel : Pixels) { Pixel.A = 255; }
				TArray64<uint8> Png;
				FImageUtils::PNGCompressImageArray(Width, Height, Pixels, Png);
				const FString ImagePath = FPaths::Combine(Directory, Tag + TEXT(".png"));
				Report->SetBoolField(TEXT("capture_ok"), !Png.IsEmpty() && FFileHelper::SaveArrayToFile(Png, *ImagePath));
				Report->SetStringField(TEXT("capture_path"), FPaths::ConvertRelativePathToFull(ImagePath));
			}
			if (!Report->GetBoolField(TEXT("capture_ok")))
			{
				Fail(FString::Printf(TEXT("실제 PIE viewport 캡처 실패: 요청 %dx%d, 캡처 %dx%d. 창 크기나 렌더 준비를 확인하세요."),
					Width, Height, ImageSize.X, ImageSize.Y));
			}

			UWidget* Host = Bar->GetWidgetFromName(TEXT("Panel_PanelHost"));
			// 호스트는 이전에 연 패널도 Collapsed로 보관한다. 이름이 같은 이전 패널을 측정하지 않는다.
			UFF_PrepPanelBase* ActivePanel = nullptr;
			int32 VisiblePanelCount = 0;
			if (UPanelWidget* PanelHost = Cast<UPanelWidget>(Host))
			{
				for (UWidget* Child : PanelHost->GetAllChildren())
				{
					if (UFF_PrepPanelBase* Panel = Cast<UFF_PrepPanelBase>(Child); Panel && Panel->IsVisible())
					{
						++VisiblePanelCount;
						if (Panel->GetTab() == Bar->GetSelection().Tab) { ActivePanel = Panel; }
					}
				}
			}
			Report->SetNumberField(TEXT("visible_panel_count"), VisiblePanelCount);
			Report->SetStringField(TEXT("active_panel"), ActivePanel ? ActivePanel->GetPathName() : TEXT(""));
			Report->SetStringField(TEXT("selected_tab"), UEnum::GetValueAsString(Bar->GetSelection().Tab));
			if (!ActivePanel || VisiblePanelCount != 1) { Fail(TEXT("선택한 탭의 패널 하나만 표시되어야 합니다.")); }
			UWidget* Menu = Bar->GetWidgetFromName(TEXT("SizeBox_MenuRow"));
			UWidget* Status = Bar->GetWidgetFromName(TEXT("SizeBox_StatusRow"));
			const FSlateRect HostRect = PixelRect(Host);
			const FSlateRect ViewportRect(0.f, 0.f, Width, Height);
			const FSlateRect MenuRect = PixelRect(Menu), StatusRect = PixelRect(Status);
			const FSlateRect HudRect(FMath::Min(MenuRect.Left, StatusRect.Left), FMath::Min(MenuRect.Top, StatusRect.Top),
				FMath::Max(MenuRect.Right, StatusRect.Right), FMath::Max(MenuRect.Bottom, StatusRect.Bottom));
			SetRect(Report, TEXT("bottom_hud_pixel_bounds"), HudRect);
			Report->SetBoolField(TEXT("host_inside_viewport"), Contains(ViewportRect, HostRect));
			Report->SetBoolField(TEXT("host_overlaps_bottom_hud"), Overlaps(HostRect, HudRect));
			if (!Contains(ViewportRect, HostRect)) { Fail(TEXT("패널 호스트가 실제 viewport 밖으로 나갑니다. 저장 Size/Offset을 확인하세요.")); }
			if (Overlaps(HostRect, HudRect)) { Fail(TEXT("패널 호스트와 하단 HUD가 겹칩니다. 저장 Size/Offset을 확인하세요.")); }
			const TSharedRef<FJsonObject> Children = MakeShared<FJsonObject>();
			auto Inspect = [&](UWidget* Root, const TCHAR* Name, const FSlateRect& ParentRect,
				bool bIsPanelChild, bool bRequireVisible = true)
			{
				UWidget* Widget = FindChild(Root, FName(Name));
				const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
				Children->SetObjectField(Name, Row);
				Row->SetBoolField(TEXT("found"), Widget != nullptr);
				Row->SetBoolField(TEXT("required_visible"), bRequireVisible);
				if (!Widget) { Fail(FString(Name) + TEXT(": 런타임 위젯 연결이 없습니다.")); return; }
				const FGeometry Geometry = Widget->GetCachedGeometry();
				const FSlateRect Rect = PixelRect(Widget);
				const bool bValid = Widget->IsVisible() && FFPrepPanelCenterProbe::HasSize(Geometry);
				Row->SetStringField(TEXT("path"), Widget->GetPathName());
				Row->SetBoolField(TEXT("visible"), Widget->IsVisible());
				Row->SetBoolField(TEXT("enabled"), Widget->GetIsEnabled());
				Row->SetBoolField(TEXT("geometry_valid"), bValid);
				Row->SetBoolField(TEXT("inside_parent"), Contains(ParentRect, Rect));
				Row->SetBoolField(TEXT("inside_host"), Contains(HostRect, Rect));
				Row->SetBoolField(TEXT("inside_viewport"), Contains(ViewportRect, Rect));
				Row->SetBoolField(TEXT("overlaps_bottom_hud"), Overlaps(Rect, HudRect));
				SetRect(Row, TEXT("pixel_bounds"), Rect);
				FFPrepPanelCenterProbe::SetVector(Row, TEXT("local_size"), Geometry.GetLocalSize());
				FFPrepPanelCenterProbe::SetVector(Row, TEXT("desired_size"), Widget->GetDesiredSize());
				// 준비 완료 시 하단 차단 사유가 없는 것은 정상이다. 오래된 CachedGeometry를 실패 근거로 쓰지 않는다.
				if (!bRequireVisible && !Widget->IsVisible())
				{
					Row->SetBoolField(TEXT("skipped_hidden"), true);
					return;
				}
				Row->SetBoolField(TEXT("skipped_hidden"), false);
				if (!bValid) { Fail(FString(Name) + TEXT(": 표시 중인 유효한 geometry가 없습니다.")); }
				if (!Contains(ParentRect, Rect)) { Fail(FString(Name) + TEXT(": 고정 부모 영역 밖으로 나갑니다.")); }
				if (!Contains(ViewportRect, Rect)) { Fail(FString(Name) + TEXT(": 실제 viewport 밖으로 나갑니다.")); }
				if (bIsPanelChild && Overlaps(Rect, HudRect)) { Fail(FString(Name) + TEXT(": 하단 HUD와 겹칩니다.")); }
				if (UTextBlock* Text = Cast<UTextBlock>(Widget))
				{
					Row->SetStringField(TEXT("text"), Text->GetText().ToString());
					if (!Text->GetText().IsEmpty() && Geometry.GetLocalSize().Y + Tolerance < Text->GetDesiredSize().Y)
					{
						Fail(FString(Name) + TEXT(": 글자 높이가 할당 높이보다 큽니다. 부모의 세로 여백을 확인하세요."));
					}
				}
			};
			const FSlateRect HeaderRect = PixelRect(FindChild(ActivePanel, TEXT("HorizontalBox_Header")));
			const FSlateRect FooterRect = PixelRect(FindChild(ActivePanel, TEXT("Border_Footer")));
			for (const TCHAR* Name : { TEXT("HorizontalBox_Header"), TEXT("Border_Footer"), TEXT("ScrollBox_Body") })
			{
				Inspect(ActivePanel, Name, HostRect, true);
			}
			Inspect(ActivePanel, TEXT("Button_Close"), HeaderRect, true);
			Inspect(ActivePanel, TEXT("Text_PanelTitle"), HeaderRect, true);
			const FSlateRect ScrollRect = PixelRect(FindChild(ActivePanel, TEXT("ScrollBox_Body")));
			if (ScrollRect.Top < HeaderRect.Bottom - Tolerance || ScrollRect.Bottom > FooterRect.Top + Tolerance)
			{
				Fail(TEXT("ScrollBox_Body: 본문이 고정 헤더 또는 푸터와 겹칩니다."));
			}
			const TCHAR* Action = nullptr;
			const TCHAR* Label = nullptr;
			switch (Bar->GetSelection().Tab)
			{
			case EFF_PrepHudTab::Program: Action = TEXT("Button_Schedule"); Label = TEXT("Text_ScheduleAction"); break;
			case EFF_PrepHudTab::Staff: Action = TEXT("Button_Hire"); Label = TEXT("Text_HireAction"); break;
			case EFF_PrepHudTab::Promotion: Action = TEXT("Button_Purchase"); Label = TEXT("Text_PurchaseAction"); break;
			case EFF_PrepHudTab::Check: Action = TEXT("Button_OpenGate"); Label = TEXT("Text_OpenGateLabel");
				Inspect(ActivePanel, TEXT("Button_ContinueBuild"), FooterRect, true);
				Inspect(ActivePanel, TEXT("Text_OpenGateReason"), FooterRect, true); break;
			default: Fail(TEXT("통합 probe는 Program/Staff/Promotion/Check 패널만 검사합니다.")); break;
			}
			if (Action)
			{
				Inspect(ActivePanel, Action, FooterRect, true);
				Inspect(ActivePanel, Label, PixelRect(FindChild(ActivePanel, FName(Action))), true);
			}
			Inspect(Bar, TEXT("SizeBox_MenuRow"), ViewportRect, false);
			Inspect(Bar, TEXT("SizeBox_StatusRow"), ViewportRect, false);
			const bool bHasCtaReason = !Bar->GetCtaReason().IsEmpty();
			Report->SetStringField(TEXT("cta_reason"), Bar->GetCtaReason().ToString());
			Inspect(Bar, TEXT("Box_Cta"), StatusRect, false, bHasCtaReason);
			Inspect(Bar, TEXT("Text_CtaReason"), PixelRect(FindChild(Bar, TEXT("Box_Cta"))), false, bHasCtaReason);
			Report->SetObjectField(TEXT("children"), Children);
		}
		else if (bSafeTag && bWorldMatches && Report->GetBoolField(TEXT("ok")))
		{
			Fail(TEXT("PIE 게임 viewport 또는 Slate 렌더러를 찾지 못했습니다."));
		}
		Report->SetArrayField(TEXT("failures"), Failures);
		Report->SetBoolField(TEXT("integration_ok"), Failures.IsEmpty());
		FString Json;
		const bool bSerialized = FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
		const bool bWritten = bSerialized && FFileHelper::SaveStringToFile(Json,
			*FPaths::Combine(Directory, TEXT("native-geometry.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		if (bSafeTag && bSerialized)
		{
			FFileHelper::SaveStringToFile(Json, *FPaths::Combine(Directory, Tag + TEXT(".json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}
		UE_LOG(LogTemp, Display, TEXT("FF.PrepPanelsIntegrationProbe: tag=%s integration_ok=%s saved=%s failures=%d"),
			*Tag, Failures.IsEmpty() ? TEXT("true") : TEXT("false"), bWritten ? TEXT("true") : TEXT("false"), Failures.Num());
	}

	FAutoConsoleCommandWithWorldAndArgs Command(TEXT("FF.PrepPanelsIntegrationProbe"),
		TEXT("FF.PrepPanelsIntegrationProbe <영문숫자·하이픈·밑줄태그> — 실제 PIE viewport PNG와 패널/하단 HUD 경계를 Saved/PrepPanelsIntegration에 기록한다."),
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
