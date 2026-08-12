// FF_LiveHudWbpBuilder.cpp

#include "CoreMinimal.h"

#if WITH_EDITOR

// Live HUD 하단(메뉴 레일 + 상태 바) UMG 에셋을 코드로 부트스트랩하는 에디터 전용 도구다.
//
// === 계약 ===
// `FF.LiveHudUMG`는 **없는 WBP만 만든다.** 이미 있으면 레이아웃 콜백을 실행하지 않는다.
// 덮어쓰는 명령은 만들지 않는다. 규칙 원본: docs/memory/wbp-codegen-workflow.md
//
// === 좌표의 출처 ===
// `Tools/ui/normalize_live_hud_menu_mockups.js`의 상수와
// `SourceArt/UI/LiveHUD/LightTealParts_v1/asset-manifest.json`의 `margin_px`를 그대로 옮겼다.
// 목업을 다시 뽑으면 이 상수를 갱신한다. 목업과 WBP가 따로 놀지 않게 하는 장치다.
//
// === 9-slice의 두 값을 함께 넣는다 ===
// `Draw As = Box`는 `Brush.Margin`(정규화 0~1)과 `Brush.ImageSize`를 **둘 다** 봐야 정상 동작한다.
// ImageSize를 텍스처 기본값으로 두면 모서리가 표시 크기를 넘어 렌즈처럼 부푼다 (2026-08-10 함정).

#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UI/FF_LiveHudMenuBar.h"
#include "UI/FF_LiveHudStatusBar.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFLiveHudWbpBuilder, Log, All);

namespace FFLiveHudWbpBuilder
{
	const FString UiRoot(TEXT("/Game/FestaFiesta/UI"));
	const FString LiveHudFolder(UiRoot / TEXT("LiveHUD"));
	const FString ThemeFolder(LiveHudFolder / TEXT("Themes/LightTeal"));
	const FString FontPackage(UiRoot / TEXT("Fonts/F_UI_Pretendard"));

	const FString TabButtonPackage(LiveHudFolder / TEXT("WBP_FF_LiveHudTabButton"));
	const FString MenuBarPackage(LiveHudFolder / TEXT("WBP_FF_LiveHudMenuBar"));
	const FString StatusBarPackage(LiveHudFolder / TEXT("WBP_FF_LiveHudStatusBar"));

	// === 목업 레이아웃 (normalize_live_hud_menu_mockups.js) ===
	constexpr float RailWidth = 1078.f;
	constexpr float RowHeight = 29.f;
	constexpr float RailPaddingX = 65.f;
	constexpr float TabWidth = 100.f;
	constexpr float TabHeight = 25.f;
	constexpr float TabIconSize = 13.f;
	constexpr float StatusBarWidth = 1920.f;

	// === 9-slice margin_px (asset-manifest.json) ===
	constexpr float RailMarginPx = 40.f;
	constexpr float NavMarginPx = 24.f;
	constexpr float StatusMarginPx = 24.f;
	constexpr float TrackMarginPx = 10.f;

	const FLinearColor ColorInk = FLinearColor::FromSRGBColor(FColor(0x10, 0x2B, 0x4E));
	const FLinearColor ColorOnDark = FLinearColor::FromSRGBColor(FColor(0xEC, 0xF4, 0xFF));
	const FLinearColor ColorMuted = FLinearColor::FromSRGBColor(FColor(0xBF, 0xD0, 0xE5));

	using TBuildLayout = TFunction<bool(UWidgetBlueprint&, UWidgetTree&)>;

	FString MakeObjectPath(const FString& PackageName)
	{
		return FString::Printf(TEXT("%s.%s"), *PackageName, *FPackageName::GetLongPackageAssetName(PackageName));
	}

	UTexture2D* LoadThemeTexture(const FString& RelativeName)
	{
		const FString PackageName = ThemeFolder / RelativeName;
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *MakeObjectPath(PackageName));
		if (!Texture)
		{
			// 조용히 넘기지 않는다. 브러시가 비면 화면이 빈 사각형으로 나오고 원인을 찾기 어렵다.
			UE_LOG(LogFFLiveHudWbpBuilder, Warning,
				TEXT("FF_LIVEHUD_UMG_WARN: 텍스처를 찾지 못했다 — %s"), *PackageName);
		}
		return Texture;
	}

	/**
	 * 9-slice 브러시를 만든다.
	 * Margin은 정규화 값이라 `margin_px / 텍스처 변 길이`로 넣어야 한다. px를 그대로 넣으면 안 된다.
	 */
	FSlateBrush MakeBoxBrush(UTexture2D* Texture, float MarginPx, const FVector2D& DisplaySize)
	{
		FSlateBrush Brush;
		if (!Texture)
		{
			return Brush;
		}

		Brush.SetResourceObject(Texture);
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.SetImageSize(DisplaySize);

		const float TexW = FMath::Max(1.f, static_cast<float>(Texture->GetSizeX()));
		const float TexH = FMath::Max(1.f, static_cast<float>(Texture->GetSizeY()));
		Brush.Margin = FMargin(MarginPx / TexW, MarginPx / TexH, MarginPx / TexW, MarginPx / TexH);

		return Brush;
	}

	FSlateBrush MakeImageBrush(UTexture2D* Texture, const FVector2D& DisplaySize)
	{
		FSlateBrush Brush;
		if (!Texture)
		{
			return Brush;
		}

		Brush.SetResourceObject(Texture);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.SetImageSize(DisplaySize);
		return Brush;
	}

	template <typename TWidget>
	TWidget* MakeWidget(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, const FName Name, const bool bVariable = false)
	{
		TWidget* Widget = Tree.ConstructWidget<TWidget>(TWidget::StaticClass(), Name);
		if (Widget)
		{
			Widget->bIsVariable = bVariable;
			// 이 호출을 빼면 컴파일·재저장 후 위젯 이름을 잃는다. 반드시 유지한다.
			Blueprint.OnVariableAdded(Name);
		}
		return Widget;
	}

	void StyleText(UTextBlock& Text, const int32 FontSize, const FLinearColor& Color, const FName TypefaceName)
	{
		FSlateFontInfo Font = Text.GetFont();
		if (UFont* Pretendard = LoadObject<UFont>(nullptr, *MakeObjectPath(FontPackage)))
		{
			Font.FontObject = Pretendard;
			Font.TypefaceFontName = TypefaceName;
		}
		Font.Size = FontSize;
		Text.SetFont(Font);
		Text.SetColorAndOpacity(FSlateColor(Color));
	}

	UCanvasPanelSlot* PlaceOnCanvas(UCanvasPanel& Canvas, UWidget& Child, float X, float Y, float W, float H)
	{
		UCanvasPanelSlot* CanvasSlot = Canvas.AddChildToCanvas(&Child);
		if (!CanvasSlot)
		{
			return nullptr;
		}
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
		CanvasSlot->SetAlignment(FVector2D(0.f, 0.f));
		CanvasSlot->SetAutoSize(false);
		CanvasSlot->SetOffsets(FMargin(X, Y, W, H));
		return CanvasSlot;
	}

	bool SaveNewBlueprint(UWidgetBlueprint& Blueprint)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&Blueprint);
		FKismetEditorUtilities::CompileBlueprint(&Blueprint);
		if (Blueprint.Status == BS_Error)
		{
			UE_LOG(LogFFLiveHudWbpBuilder, Error,
				TEXT("FF_LIVEHUD_UMG_FAIL: %s 컴파일 실패."), *Blueprint.GetPathName());
			return false;
		}

		Blueprint.MarkPackageDirty();
		UPackage* Package = Blueprint.GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);

		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, &Blueprint, *Filename, Args);
	}

	UWidgetBlueprint* LoadExistingBlueprint(const FString& PackageName)
	{
		return LoadObject<UWidgetBlueprint>(nullptr, *MakeObjectPath(PackageName));
	}

	UWidgetBlueprint* EnsureWidgetBlueprint(
		const FString& PackageName,
		const FString& Folder,
		UClass& ParentClass,
		TBuildLayout&& BuildLayout)
	{
		if (FPackageName::DoesPackageExist(PackageName))
		{
			UWidgetBlueprint* Existing = LoadExistingBlueprint(PackageName);
			UE_LOG(LogFFLiveHudWbpBuilder, Display,
				TEXT("FF_LIVEHUD_UMG_REUSE: %s 이미 존재. 디자이너 수정을 보존했다."), *PackageName);
			return Existing;
		}

		UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
		Factory->ParentClass = &ParentClass;

		FAssetToolsModule& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(AssetTools.Get().CreateAsset(
			AssetName, Folder, UWidgetBlueprint::StaticClass(), Factory));
		if (!Blueprint || !Blueprint->WidgetTree)
		{
			UE_LOG(LogFFLiveHudWbpBuilder, Error, TEXT("FF_LIVEHUD_UMG_FAIL: %s 생성 실패."), *PackageName);
			return nullptr;
		}

		if (!BuildLayout(*Blueprint, *Blueprint->WidgetTree))
		{
			UE_LOG(LogFFLiveHudWbpBuilder, Error,
				TEXT("FF_LIVEHUD_UMG_FAIL: %s 레이아웃 구성 실패. 저장하지 않았다."), *PackageName);
			return nullptr;
		}

		if (!SaveNewBlueprint(*Blueprint))
		{
			return nullptr;
		}

		UE_LOG(LogFFLiveHudWbpBuilder, Display, TEXT("FF_LIVEHUD_UMG_CREATE: %s 생성 완료."), *PackageName);
		return Blueprint;
	}

	// =========================================================================
	// WBP_FF_LiveHudTabButton — 100 x 25
	// =========================================================================
	bool BuildTabButton(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		UOverlay* Root = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Root"));
		UButton* Button = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Root"), true);
		UHorizontalBox* Content = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Content"));
		UImage* Icon = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Icon"), true);
		UTextBlock* Label = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Label"), true);

		if (!Root || !Button || !Content || !Icon || !Label)
		{
			return false;
		}

		Tree.RootWidget = Root;

		// 버튼 스타일 4종. Selected는 `RefreshVisualState`가 Pressed 브러시를 Normal에 끼워 표현한다.
		const FVector2D TabSize(TabWidth, TabHeight);
		FButtonStyle Style = Button->GetStyle();
		Style.SetNormal(MakeBoxBrush(LoadThemeTexture(TEXT("Buttons/T_FF_LH_Button_Nav_Default_512x128_9Slice")), NavMarginPx, TabSize));
		Style.SetHovered(MakeBoxBrush(LoadThemeTexture(TEXT("Buttons/T_FF_LH_Button_Nav_Hover_512x128_9Slice")), NavMarginPx, TabSize));
		Style.SetPressed(MakeBoxBrush(LoadThemeTexture(TEXT("Buttons/T_FF_LH_Button_Nav_Selected_512x128_9Slice")), NavMarginPx, TabSize));
		Style.SetDisabled(MakeBoxBrush(LoadThemeTexture(TEXT("Buttons/T_FF_LH_Button_Nav_Disabled_512x128_9Slice")), NavMarginPx, TabSize));
		Button->SetStyle(Style);

		if (UOverlaySlot* RootSlot = Root->AddChildToOverlay(Button))
		{
			RootSlot->SetHorizontalAlignment(HAlign_Fill);
			RootSlot->SetVerticalAlignment(VAlign_Fill);
		}

		Button->SetContent(Content);

		// 아이콘은 흰색 단색이고 색은 C++가 상태에 맞게 틴트한다. 여기서 색을 정하지 않는다.
		Icon->SetBrush(MakeImageBrush(
			LoadThemeTexture(TEXT("Icons/Navigation/T_FF_LH_Icon_Operations_White_256")),
			FVector2D(TabIconSize, TabIconSize)));

		if (UHorizontalBoxSlot* IconSlot = Content->AddChildToHorizontalBox(Icon))
		{
			IconSlot->SetVerticalAlignment(VAlign_Center);
			IconSlot->SetPadding(FMargin(0.f, 0.f, 5.f, 0.f));
		}

		StyleText(*Label, 11, ColorInk, TEXT("SemiBold"));
		if (UHorizontalBoxSlot* LabelSlot = Content->AddChildToHorizontalBox(Label))
		{
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}

		return true;
	}

	// =========================================================================
	// WBP_FF_LiveHudMenuBar — 1078 x 29
	// =========================================================================
	bool BuildMenuBar(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		UOverlay* Root = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Root"));
		UImage* Rail = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Rail"));
		UHorizontalBox* Tabs = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Box_Tabs"), true);

		if (!Root || !Rail || !Tabs)
		{
			return false;
		}

		Tree.RootWidget = Root;

		Rail->SetBrush(MakeBoxBrush(
			LoadThemeTexture(TEXT("Panels/T_FF_LH_HUD_MenuRail_2048x128_9Slice")),
			RailMarginPx,
			FVector2D(RailWidth, RowHeight)));

		if (UOverlaySlot* RailSlot = Root->AddChildToOverlay(Rail))
		{
			RailSlot->SetHorizontalAlignment(HAlign_Fill);
			RailSlot->SetVerticalAlignment(VAlign_Fill);
		}

		if (UOverlaySlot* TabsSlot = Root->AddChildToOverlay(Tabs))
		{
			TabsSlot->SetHorizontalAlignment(HAlign_Fill);
			TabsSlot->SetVerticalAlignment(VAlign_Center);
			// 목업의 좌우 내부 여백 65. 이 값이 레일 폭 1078과 탭 9개(100+gap 6)를 맞춘다.
			TabsSlot->SetPadding(FMargin(RailPaddingX, 0.f, RailPaddingX, 0.f));
		}

		return true;
	}

	// =========================================================================
	// WBP_FF_LiveHudStatusBar — 1920 x 29
	// =========================================================================
	bool BuildStatusBar(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Root"));
		UImage* Bar = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Bar"));

		if (!Root || !Bar)
		{
			return false;
		}

		Tree.RootWidget = Root;

		Bar->SetBrush(MakeBoxBrush(
			LoadThemeTexture(TEXT("Panels/T_FF_LH_HUD_StatusBar_2048x128_9Slice")),
			StatusMarginPx,
			FVector2D(StatusBarWidth, RowHeight)));

		if (UCanvasPanelSlot* BarSlot = Root->AddChildToCanvas(Bar))
		{
			BarSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			BarSlot->SetOffsets(FMargin(0.f));
		}

		// --- 좌측: 배지와 지표 ---
		// 좌표는 목업 01-operations.png의 하단 행을 1920 기준으로 읽은 값이다.
		// 정확한 위치 맞추기는 에디터 마감 단계에서 한다.

		struct FIconEntry
		{
			const TCHAR* WidgetName;
			const TCHAR* TexturePath;
			float X;
			float Size;
		};

		const FIconEntry Icons[] = {
			{ TEXT("Image_BadgeFF"),      TEXT("Badges/T_FF_LH_Badge_FF_256"),                    30.f, 20.f },
			{ TEXT("Image_BadgeGold"),    TEXT("Badges/T_FF_LH_Badge_Gold_256"),                  96.f, 18.f },
			{ TEXT("Image_IconVisitors"), TEXT("Icons/Metrics/T_FF_LH_Icon_Visitors_White_256"),  228.f, 16.f },
			{ TEXT("Image_IconStaff"),    TEXT("Icons/Metrics/T_FF_LH_Icon_StaffCount_White_256"),322.f, 16.f },
			{ TEXT("Image_IconClock"),    TEXT("Icons/Metrics/T_FF_LH_Icon_Clock_White_256"),    1378.f, 16.f },
		};

		for (const FIconEntry& Entry : Icons)
		{
			UImage* Icon = MakeWidget<UImage>(Blueprint, Tree, FName(Entry.WidgetName));
			if (!Icon)
			{
				return false;
			}
			Icon->SetBrush(MakeImageBrush(LoadThemeTexture(Entry.TexturePath), FVector2D(Entry.Size, Entry.Size)));
			PlaceOnCanvas(*Root, *Icon, Entry.X, (RowHeight - Entry.Size) * 0.5f, Entry.Size, Entry.Size);
		}

		struct FTextEntry
		{
			const TCHAR* WidgetName;
			float X;
			float W;
			int32 FontSize;
			FLinearColor Color;
		};

		const FTextEntry Texts[] = {
			{ TEXT("Text_Gold"),           120.f, 100.f, 12, ColorOnDark },
			{ TEXT("Text_VisitorCount"),   250.f,  70.f, 12, ColorOnDark },
			{ TEXT("Text_Staff"),          344.f,  70.f, 12, ColorOnDark },
			{ TEXT("Text_DayStage"),       880.f, 160.f, 11, ColorOnDark },
			{ TEXT("Text_Clock"),         1400.f,  70.f, 12, ColorOnDark },
			{ TEXT("Text_NextProgram"),   1486.f, 180.f, 11, ColorMuted },
			{ TEXT("Text_RemainingTime"),  430.f,  70.f, 12, ColorMuted },
			{ TEXT("Text_TimeScaleHint"),  520.f, 300.f, 10, ColorMuted },
		};

		for (const FTextEntry& Entry : Texts)
		{
			UTextBlock* Text = MakeWidget<UTextBlock>(Blueprint, Tree, FName(Entry.WidgetName), true);
			if (!Text)
			{
				return false;
			}
			StyleText(*Text, Entry.FontSize, Entry.Color, TEXT("Medium"));
			PlaceOnCanvas(*Root, *Text, Entry.X, 6.f, Entry.W, 18.f);
		}

		// --- 진행 트랙 ---
		UProgressBar* Progress = MakeWidget<UProgressBar>(Blueprint, Tree, TEXT("Progress_Program"), true);
		if (!Progress)
		{
			return false;
		}

		FProgressBarStyle ProgressStyle = Progress->GetWidgetStyle();
		ProgressStyle.SetBackgroundImage(MakeBoxBrush(
			LoadThemeTexture(TEXT("Progress/T_FF_LH_Progress_ProgramTrack_1024x24_9Slice")), TrackMarginPx, FVector2D(240.f, 6.f)));
		ProgressStyle.SetFillImage(MakeBoxBrush(
			LoadThemeTexture(TEXT("Progress/T_FF_LH_Progress_Fill_WhiteMask_1024x32_9Slice")), TrackMarginPx, FVector2D(240.f, 6.f)));
		Progress->SetWidgetStyle(ProgressStyle);
		PlaceOnCanvas(*Root, *Progress, 1060.f, 12.f, 240.f, 6.f);

		// --- 우측: 재생 컨트롤 ---
		struct FControlEntry
		{
			const TCHAR* WidgetName;
			const TCHAR* TexturePath;
			float X;
		};

		const FControlEntry Controls[] = {
			{ TEXT("Button_Pause"),       TEXT("Icons/Actions/T_FF_LH_Icon_Pause_White_256"),        1716.f },
			{ TEXT("Button_Play"),        TEXT("Icons/Actions/T_FF_LH_Icon_Play_White_256"),         1762.f },
			{ TEXT("Button_FastForward"), TEXT("Icons/Actions/T_FF_LH_Icon_FastForward_White_256"),  1808.f },
			{ TEXT("Button_Skip"),        TEXT("Icons/Actions/T_FF_LH_Icon_Next_White_256"),         1854.f },
		};

		for (const FControlEntry& Entry : Controls)
		{
			UButton* Button = MakeWidget<UButton>(Blueprint, Tree, FName(Entry.WidgetName), true);
			if (!Button)
			{
				return false;
			}

			// 아이콘 버튼 배경. 원형 브러시 2종이 테마에 있다.
			FButtonStyle Style = Button->GetStyle();
			Style.SetNormal(MakeImageBrush(LoadThemeTexture(TEXT("Buttons/T_FF_LH_Button_IconCircle_Default_256")), FVector2D(24.f, 24.f)));
			Style.SetHovered(MakeImageBrush(LoadThemeTexture(TEXT("Buttons/T_FF_LH_Button_IconCircle_Selected_256")), FVector2D(24.f, 24.f)));
			Style.SetPressed(MakeImageBrush(LoadThemeTexture(TEXT("Buttons/T_FF_LH_Button_IconCircle_Selected_256")), FVector2D(24.f, 24.f)));
			Button->SetStyle(Style);

			UImage* Icon = MakeWidget<UImage>(Blueprint, Tree, FName(*FString::Printf(TEXT("Image_%s"), Entry.WidgetName)));
			if (!Icon)
			{
				return false;
			}
			Icon->SetBrush(MakeImageBrush(LoadThemeTexture(Entry.TexturePath), FVector2D(12.f, 12.f)));
			Button->SetContent(Icon);

			PlaceOnCanvas(*Root, *Button, Entry.X, 2.5f, 24.f, 24.f);
		}

		return true;
	}

	void RunLiveHudUmg()
	{
		UE_LOG(LogFFLiveHudWbpBuilder, Display, TEXT("FF_LIVEHUD_UMG_BEGIN: 없는 WBP만 생성한다."));

		// 탭 버튼이 먼저다. 메뉴 바가 그 클래스를 기본값으로 물어야 한다.
		UWidgetBlueprint* TabButton = EnsureWidgetBlueprint(
			TabButtonPackage, LiveHudFolder, *UFF_LiveHudTabButton::StaticClass(),
			[](UWidgetBlueprint& BP, UWidgetTree& Tree) { return BuildTabButton(BP, Tree); });

		UWidgetBlueprint* MenuBar = EnsureWidgetBlueprint(
			MenuBarPackage, LiveHudFolder, *UFF_LiveHudMenuBar::StaticClass(),
			[](UWidgetBlueprint& BP, UWidgetTree& Tree) { return BuildMenuBar(BP, Tree); });

		// 메뉴 바의 TabButtonClass 기본값을 채운다. 비어 있으면 런타임에 탭이 하나도 안 생긴다.
		if (MenuBar && MenuBar->GeneratedClass && TabButton && TabButton->GeneratedClass)
		{
			if (UFF_LiveHudMenuBar* Defaults = MenuBar->GeneratedClass->GetDefaultObject<UFF_LiveHudMenuBar>())
			{
				if (!Defaults->TabButtonClass)
				{
					Defaults->TabButtonClass = TabButton->GeneratedClass;
					MenuBar->Modify();
					FKismetEditorUtilities::CompileBlueprint(MenuBar);
					SaveNewBlueprint(*MenuBar);
					UE_LOG(LogFFLiveHudWbpBuilder, Display,
						TEXT("FF_LIVEHUD_UMG_LINK: MenuBar의 TabButtonClass를 %s로 채웠다."),
						*TabButton->GetName());
				}
			}
		}

		EnsureWidgetBlueprint(
			StatusBarPackage, LiveHudFolder, *UFF_LiveHudStatusBar::StaticClass(),
			[](UWidgetBlueprint& BP, UWidgetTree& Tree) { return BuildStatusBar(BP, Tree); });

		UE_LOG(LogFFLiveHudWbpBuilder, Display,
			TEXT("FF_LIVEHUD_UMG_END: 끝났다. 좌표·색 마감은 디자이너에서 한다."));
	}

	FAutoConsoleCommand LiveHudUmgCommand(
		TEXT("FF.LiveHudUMG"),
		TEXT("Live HUD 하단 WBP 3종(TabButton / MenuBar / StatusBar)을 만든다. 이미 있는 에셋은 건드리지 않는다."),
		FConsoleCommandDelegate::CreateStatic(&RunLiveHudUmg));
}

#endif // WITH_EDITOR
