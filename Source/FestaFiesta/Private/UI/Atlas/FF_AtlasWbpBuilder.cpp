// FF_AtlasWbpBuilder.cpp

#include "CoreMinimal.h"

#if WITH_EDITOR

// Atlas 화면(U3-b)의 WBP 골격을 만드는 에디터 전용 도구다. 콘솔 명령은 `FF.AtlasUMG`.
//
// **없는 WBP만 만든다.** 규칙과 함정은 FF_UIWbpBuilderCommon.h 상단에 있다.
//
// === 생성기가 하지 않는 것 ===
// 두 화면의 `PinButtonClass` 는 **에디터에서 지정한다.** CDO 프로퍼티는 컴파일 뒤에야
// 존재해서 생성 흐름 안에서 넣기 어렵다. 빠뜨리면 핀이 하나도 뜨지 않는데,
// 각 화면이 그때 경고를 남기도록 해 두었다.
//
// === 좌표는 대략값이다 ===
// 1920x1080 기준으로 왼쪽 1/3 을 패널, 오른쪽을 지구·지도에 비워 둔다
// (확정 방향 — "지구가 주인공, UI 는 왼쪽에만"). 크기·여백·폰트 마감은 디자이너에서 한다.
// **여기 상수를 레이아웃의 원본으로 삼지 않는다.**

#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/Atlas/FF_AtlasCountryScreen.h"
#include "UI/Atlas/FF_AtlasGlobeScreen.h"
#include "UI/Atlas/FF_AtlasPinButton.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

namespace
{
	using namespace FFUIWbpBuilder;

	const TCHAR* AtlasLogPrefix = TEXT("FF_ATLAS_UMG");

	const FString AtlasFolder = TEXT("/Game/FestaFiesta/UI/Atlas");
	const FString PinPackage = TEXT("/Game/FestaFiesta/UI/Atlas/WBP_FF_AtlasPin");
	const FString GlobePackage = TEXT("/Game/FestaFiesta/UI/Atlas/WBP_FF_AtlasGlobe");
	const FString CountryPackage = TEXT("/Game/FestaFiesta/UI/Atlas/WBP_FF_AtlasCountry");

	// 1920x1080 기준 대략값. 왼쪽 1/3 이 패널, 오른쪽은 비운다.
	constexpr float PanelX = 120.f;
	constexpr float PanelWidth = 520.f;
	constexpr float PanelTop = 240.f;

	/** 포커스 표시. `RenderFocusRule=Never` 가 전역이라 이 링이 유일한 포커스 신호다. */
	UImage* MakeFocusRing(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, const FLinearColor& Color)
	{
		UImage* Ring = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_FocusRing"), true);
		if (Ring)
		{
			Ring->SetColorAndOpacity(Color);
			// 처음엔 숨긴다. 버튼 베이스가 포커스에 따라 켠다.
			Ring->SetVisibility(ESlateVisibility::Collapsed);
		}
		return Ring;
	}

	/** 세로 목록에 텍스트 한 줄을 넣는다. */
	UTextBlock* AddPanelText(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		UVerticalBox& Column,
		const FName Name,
		const int32 FontSize,
		const FLinearColor& Color,
		const FText& Placeholder,
		const float BottomPadding)
	{
		UTextBlock* Text = MakeWidget<UTextBlock>(Blueprint, Tree, Name, true);
		if (!Text)
		{
			return nullptr;
		}

		StyleText(*Text, FontSize, Color, TEXT("Regular"));
		Text->SetText(Placeholder);
		Text->SetAutoWrapText(true);

		if (UVerticalBoxSlot* TextSlot = Column.AddChildToVerticalBox(Text))
		{
			TextSlot->SetPadding(FMargin(0.f, 0.f, 0.f, BottomPadding));
		}
		return Text;
	}

	// =========================================================================
	// WBP_FF_AtlasPin — 지구본의 국가 핀과 국가 지도의 축제 핀을 겸한다
	// =========================================================================
	bool BuildAtlasPin(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 팔레트를 읽을 수 없어 WBP_FF_AtlasPin 을 만들지 않았다."), AtlasLogPrefix);
			return false;
		}

		USizeBox* Root = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Root"));
		UOverlay* Overlay = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Root"));
		UBorder* Background = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Background"));
		UHorizontalBox* Row = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Content"));

		// `Text_Label` 은 베이스의 **필수** 바인딩이고 UCommonTextBlock 이어야 한다.
		UCommonTextBlock* Label = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_Label"), true);
		UTextBlock* Badge = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_PinBadge"), true);
		UImage* Lock = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_PinLock"), true);
		UImage* FocusRing = MakeFocusRing(Blueprint, Tree, Palette->Frontend.EdgeStrong);

		if (!Root || !Overlay || !Background || !Row || !Label || !Badge || !Lock || !FocusRing)
		{
			return false;
		}

		Tree.RootWidget = Root;

		// 🔴 판정 영역이다. 확정안이 "넓은 UMG 핀 버튼" 이므로 작게 만들지 않는다 —
		//    핀은 작아 보여도 누를 곳은 넉넉해야 한다.
		Root->SetMinDesiredWidth(180.f);
		Root->SetHeightOverride(64.f);
		Root->AddChild(Overlay);

		// CBS_ 스타일 에셋이 없어 배경을 여기서 그린다.
		Background->SetBrushColor(Palette->Frontend.GlassRaised);
		Background->SetPadding(FMargin(18.f, 8.f, 18.f, 8.f));
		Background->AddChild(Row);
		if (UOverlaySlot* BackgroundSlot = Overlay->AddChildToOverlay(Background))
		{
			BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
			BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
		}

		// 자물쇠 — 색만으로 잠금을 알리지 않기 위한 두 번째 신호다 (AT-011).
		Lock->SetColorAndOpacity(Palette->Frontend.Sub);
		Lock->SetDesiredSizeOverride(FVector2D(24.0, 24.0));
		Lock->SetVisibility(ESlateVisibility::Collapsed);
		if (UHorizontalBoxSlot* LockSlot = Row->AddChildToHorizontalBox(Lock))
		{
			LockSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			LockSlot->SetVerticalAlignment(VAlign_Center);
			LockSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
		}

		// 문구는 화면 코드가 SetPinData 로 넣는다. 여기 값은 디자이너용 자리표시자다.
		Label->SetText(LOCTEXT("AtlasPinPlaceholder", "핀 이름"));
		if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label))
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}

		StyleText(*Badge, 16, Palette->Frontend.Sub, TEXT("Regular"));
		Badge->SetText(LOCTEXT("AtlasPinBadgePlaceholder", "축제 1"));
		if (UHorizontalBoxSlot* BadgeSlot = Row->AddChildToHorizontalBox(Badge))
		{
			BadgeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			BadgeSlot->SetVerticalAlignment(VAlign_Center);
			BadgeSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
		}

		if (UOverlaySlot* RingSlot = Overlay->AddChildToOverlay(FocusRing))
		{
			RingSlot->SetHorizontalAlignment(HAlign_Fill);
			RingSlot->SetVerticalAlignment(VAlign_Fill);
		}

		return true;
	}

	// =========================================================================
	// WBP_FF_AtlasGlobe — 나라 선택. 왼쪽 패널 + 전체를 덮는 핀 캔버스
	// =========================================================================
	bool BuildAtlasGlobe(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, UClass* ButtonClass)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette || !ButtonClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 팔레트나 버튼 클래스가 없어 WBP_FF_AtlasGlobe 을 만들지 않았다."), AtlasLogPrefix);
			return false;
		}

		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Root"));

		// 🔴 투명 드래그 표면이 루트의 첫 자식이다. 빈 공간도 히트테스트되어야 화면의
		//    NativeOnMouseButtonDown까지 이벤트가 버블링된다. 핀·패널은 그 위에 놓는다.
		UBorder* DragSurface = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_DragSurface"), true);
		UCanvasPanel* Pins = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Pins"), true);
		UBorder* Panel = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Panel"), true);
		UVerticalBox* Column = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Panel"));
		UVerticalBox* FestivalList = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_FestivalList"), true);
		UImage* Preview = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Preview"), true);
		UImage* LockIcon = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_LockedIcon"), true);
		UTextBlock* Hints = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_ControlHints"), true);

		UUserWidget* SelectCountry = MakeWidgetOfClass(Blueprint, Tree, TEXT("Button_SelectCountry"), ButtonClass);
		UUserWidget* ClosePanel = MakeWidgetOfClass(Blueprint, Tree, TEXT("Button_ClosePanel"), ButtonClass);

		if (!Root || !DragSurface || !Pins || !Panel || !Column || !FestivalList
			|| !Preview || !LockIcon || !Hints || !SelectCountry || !ClosePanel)
		{
			return false;
		}

		Tree.RootWidget = Root;

		DragSurface->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
		DragSurface->SetVisibility(ESlateVisibility::Visible);
		StretchOnCanvas(*Root, *DragSurface);

		// 핀 캔버스는 화면 전체를 덮는다. 지구 어디에나 핀을 놓을 수 있어야 한다.
		// 🔴 `Self Hit Test Invisible` — `Visible` 이면 캔버스가 드래그를 먹어 지구가 안 돌아간다.
		Pins->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		StretchOnCanvas(*Root, *Pins);

		Panel->SetBrushColor(Palette->Frontend.Glass);
		Panel->SetPadding(FMargin(36.f, 32.f, 36.f, 32.f));
		Panel->AddChild(Column);
		Panel->SetVisibility(ESlateVisibility::Collapsed);
		PlaceOnCanvas(*Root, *Panel, PanelX, PanelTop, PanelWidth, 720.f);

		AddPanelText(Blueprint, Tree, *Column, TEXT("Text_CountryName"), 40,
			Palette->Frontend.Title, LOCTEXT("AtlasGlobeCountryPlaceholder", "대한민국"), 8.f);

		AddPanelText(Blueprint, Tree, *Column, TEXT("Text_FestivalCount"), 18,
			Palette->Frontend.Sub, LOCTEXT("AtlasGlobeCountPlaceholder", "축제 1개"), 20.f);

		// 대표 이미지. 없으면 화면 코드가 칸을 숨긴다.
		Preview->SetDesiredSizeOverride(FVector2D(PanelWidth - 72.0, 240.0));
		if (UVerticalBoxSlot* PreviewSlot = Column->AddChildToVerticalBox(Preview))
		{
			PreviewSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
		}

		AddPanelText(Blueprint, Tree, *Column, TEXT("Text_Description"), 18,
			Palette->Frontend.Sub, LOCTEXT("AtlasGlobeDescPlaceholder", "국가 소개가 들어갑니다."), 16.f);

		FestivalList->SetVisibility(ESlateVisibility::Collapsed);
		if (UVerticalBoxSlot* ListSlot = Column->AddChildToVerticalBox(FestivalList))
		{
			ListSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
		}

		// 잠금 표시 — 자물쇠 + 사유를 함께 둔다 (AT-011).
		LockIcon->SetColorAndOpacity(Palette->Frontend.Sub);
		LockIcon->SetDesiredSizeOverride(FVector2D(28.0, 28.0));
		LockIcon->SetVisibility(ESlateVisibility::Collapsed);
		if (UVerticalBoxSlot* LockSlot = Column->AddChildToVerticalBox(LockIcon))
		{
			LockSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}

		AddPanelText(Blueprint, Tree, *Column, TEXT("Text_LockReason"), 18,
			Palette->Frontend.Dim, LOCTEXT("AtlasGlobeLockPlaceholder", "준비 중"), 20.f);

		if (UVerticalBoxSlot* SelectSlot = Column->AddChildToVerticalBox(SelectCountry))
		{
			SelectSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		}
		Column->AddChildToVerticalBox(ClosePanel);

		// 하단 조작 안내. 문구는 화면 코드가 넣는다.
		StyleText(*Hints, 16, Palette->Frontend.Dim, TEXT("Regular"));
		Hints->SetText(LOCTEXT("AtlasGlobeHintPlaceholder", "드래그: 회전    ← →: 핀 이동"));
		PlaceOnCanvas(*Root, *Hints, PanelX, 960.f, 900.f, 40.f);

		return true;
	}

	// =========================================================================
	// WBP_FF_AtlasCountry — 국가 지도. 지도 캔버스 + 왼쪽 축제 카드
	// =========================================================================
	bool BuildAtlasCountry(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, UClass* ButtonClass)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette || !ButtonClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 팔레트나 버튼 클래스가 없어 WBP_FF_AtlasCountry 를 만들지 않았다."), AtlasLogPrefix);
			return false;
		}

		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Root"));

		// 지도 캔버스가 계산 기준이다. 화면 코드가 이 캔버스 크기로 지도 사각형을 정한다.
		UCanvasPanel* MapCanvas = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Map"), true);
		UImage* Map = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Map"), true);

		UBorder* Card = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Card"));
		UVerticalBox* Column = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Card"));
		UImage* Preview = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Preview"), true);
		UImage* LockIcon = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_LockedIcon"), true);

		UUserWidget* Start = MakeWidgetOfClass(Blueprint, Tree, TEXT("Button_StartFestival"), ButtonClass);
		UUserWidget* Back = MakeWidgetOfClass(Blueprint, Tree, TEXT("Button_Back"), ButtonClass);

		if (!Root || !MapCanvas || !Map || !Card || !Column || !Preview || !LockIcon || !Start || !Back)
		{
			return false;
		}

		Tree.RootWidget = Root;

		// 지도는 오른쪽 2/3 을 쓴다. 왼쪽은 카드가 덮으므로 비워 둔다.
		// 🔴 `Self Hit Test Invisible` — 핀만 입력을 받아야 한다.
		MapCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		PlaceOnCanvas(*Root, *MapCanvas, 700.f, 80.f, 1140.f, 920.f);

		// 지도 이미지. **크기·위치는 코드가 매 프레임 정한다** (`ComputeImageRect`).
		// 여기 값은 디자이너에서 눈으로 보기 위한 초기값이다.
		Map->SetColorAndOpacity(Palette->Frontend.GlassRaised);
		PlaceOnCanvas(*MapCanvas, *Map, 0.f, 0.f, 1140.f, 920.f);

		Card->SetBrushColor(Palette->Frontend.Glass);
		Card->SetPadding(FMargin(36.f, 32.f, 36.f, 32.f));
		Card->AddChild(Column);
		PlaceOnCanvas(*Root, *Card, PanelX, PanelTop, PanelWidth, 640.f);

		AddPanelText(Blueprint, Tree, *Column, TEXT("Text_CountryName"), 24,
			Palette->Frontend.Sub, LOCTEXT("AtlasCountryCountryPlaceholder", "대한민국"), 6.f);

		AddPanelText(Blueprint, Tree, *Column, TEXT("Text_FestivalName"), 36,
			Palette->Frontend.Title, LOCTEXT("AtlasCountryNamePlaceholder", "다대포 바다축제"), 8.f);

		AddPanelText(Blueprint, Tree, *Column, TEXT("Text_RegionName"), 18,
			Palette->Frontend.Sub, LOCTEXT("AtlasCountryRegionPlaceholder", "부산"), 16.f);

		Preview->SetDesiredSizeOverride(FVector2D(PanelWidth - 72.0, 220.0));
		if (UVerticalBoxSlot* PreviewSlot = Column->AddChildToVerticalBox(Preview))
		{
			PreviewSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
		}

		// 난이도는 `잠정` 을 함께 띄운다 — 산정 기준이 아직 없다.
		AddPanelText(Blueprint, Tree, *Column, TEXT("Text_Difficulty"), 18,
			Palette->Frontend.Text, LOCTEXT("AtlasCountryDiffPlaceholder", "난이도 1 (잠정)"), 6.f);

		AddPanelText(Blueprint, Tree, *Column, TEXT("Text_OperatingDays"), 18,
			Palette->Frontend.Text, LOCTEXT("AtlasCountryDaysPlaceholder", "운영 3일"), 16.f);

		AddPanelText(Blueprint, Tree, *Column, TEXT("Text_Description"), 18,
			Palette->Frontend.Sub, LOCTEXT("AtlasCountryDescPlaceholder", "축제 설명이 들어갑니다."), 18.f);

		// 축제를 고르기 전에 카드가 비어 있는 이유를 적는다.
		AddPanelText(Blueprint, Tree, *Column, TEXT("Text_EmptyHint"), 18,
			Palette->Frontend.Dim, LOCTEXT("AtlasCountryEmptyPlaceholder", "지도에서 축제를 고르세요"), 18.f);

		LockIcon->SetColorAndOpacity(Palette->Frontend.Sub);
		LockIcon->SetDesiredSizeOverride(FVector2D(28.0, 28.0));
		LockIcon->SetVisibility(ESlateVisibility::Collapsed);
		if (UVerticalBoxSlot* LockSlot = Column->AddChildToVerticalBox(LockIcon))
		{
			LockSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}

		AddPanelText(Blueprint, Tree, *Column, TEXT("Text_LockReason"), 18,
			Palette->Frontend.Dim, LOCTEXT("AtlasCountryLockPlaceholder", "준비 중"), 20.f);

		if (UVerticalBoxSlot* StartSlot = Column->AddChildToVerticalBox(Start))
		{
			StartSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		}
		Column->AddChildToVerticalBox(Back);

		return true;
	}

	void RunAtlasUmg()
	{
		// 프론트엔드 버튼을 재사용한다. 타이틀 버튼(`WBP_TitleMenuButton_GlassOrbit`)은
		// **임시 테스트 메뉴**의 것이라 쓰지 않는다 — 타이틀을 다시 만들 때 함께 깨진다.
		UClass* ButtonClass = LoadGeneratedClass(TEXT("/Game/FestaFiesta/UI/Frontend/WBP_FF_MenuButton"));
		if (!ButtonClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: WBP_FF_MenuButton 이 없다. 먼저 `FF.FrontendUMG` 를 실행한다."), AtlasLogPrefix);
			return;
		}

		// 핀을 먼저 만든다 — 두 화면이 이 클래스를 슬롯에 넣게 되므로 순서가 있다.
		EnsureWidgetBlueprint(
			AtlasLogPrefix, PinPackage, AtlasFolder,
			*UFF_AtlasPinButton::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{ return BuildAtlasPin(Blueprint, Tree); });

		EnsureWidgetBlueprint(
			AtlasLogPrefix, GlobePackage, AtlasFolder,
			*UFF_AtlasGlobeScreen::StaticClass(),
			[ButtonClass](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{ return BuildAtlasGlobe(Blueprint, Tree, ButtonClass); });

		EnsureWidgetBlueprint(
			AtlasLogPrefix, CountryPackage, AtlasFolder,
			*UFF_AtlasCountryScreen::StaticClass(),
			[ButtonClass](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{ return BuildAtlasCountry(Blueprint, Tree, ButtonClass); });

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_END: 끝났다. 에디터에서 해야 하는 것 — "
				 "① WBP_FF_AtlasGlobe 와 WBP_FF_AtlasCountry 의 `PinButtonClass` 에 "
				 "WBP_FF_AtlasPin 을 지정한다. "
				 "② Project Settings > Game > FestaFiesta UI Flow > Frontend Screens 의 "
				 "AtlasGlobe·AtlasCountry 를 스텁에서 이 두 WBP 로 바꾼다."), AtlasLogPrefix);
	}

	FAutoConsoleCommand AtlasUmgCommand(
		TEXT("FF.AtlasUMG"),
		TEXT("Atlas WBP 3종(AtlasPin / AtlasGlobe / AtlasCountry)을 만든다. "
			 "이미 있는 에셋은 건드리지 않는다. 먼저 FF.FrontendUMG 가 필요하다."),
		FConsoleCommandDelegate::CreateStatic(&RunAtlasUmg));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
