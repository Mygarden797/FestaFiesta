// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
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
#include "CommonButtonBase.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Styling/SlateTypes.h"
#include "UObject/UnrealType.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/Atlas/FF_AtlasCountryScreen.h"
#include "UI/Atlas/FF_AtlasGlobeScreen.h"
#include "UI/Atlas/FF_AtlasPinButton.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/FF_CommonButtonBase.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/Frontend/FF_FrontendCasualBuilder.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

namespace
{
	using namespace FFUIWbpBuilder;

	const TCHAR* AtlasLogPrefix = TEXT("FF_ATLAS_UMG");

	const FString AtlasFolder = TEXT("/Game/FestaFiesta/UI/Atlas");
	const FString PinPackage = TEXT("/Game/FestaFiesta/UI/Atlas/WBP_FF_AtlasPin");
	const FString GlobePackage = TEXT("/Game/FestaFiesta/UI/Atlas/WBP_FF_AtlasGlobe");
	const FString CountryPackage = TEXT("/Game/FestaFiesta/UI/Atlas/WBP_FF_AtlasCountry");

	// 프론트엔드 셋업 스타일 버튼. `FF.ApplyFrontendSetupStyle` 이 소유한다 — 여기서는 읽기만 한다.
	const FString FrontendButtonPackage = TEXT("/Game/FestaFiesta/UI/Frontend/WBP_FF_FrontendButton");
	const FString FrontendPrimaryButtonPackage =
		TEXT("/Game/FestaFiesta/UI/Frontend/WBP_FF_FrontendPrimaryButton");

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

	/**
	 * 아트 없이 Slate 가 칠하는 라운드 면.
	 *
	 * 9-slice 패널 아트를 쓰지 않는 이유는 가장자리에 투명 여백이 있어 면이 안쪽으로 물러나기
	 * 때문이다 (`docs/ui/title-options-wiring.md` 6절). 왼쪽 패널은 밝은 지구 위에 얹히므로
	 * **불투명**해야 글자가 읽힌다.
	 */
	FSlateBrush MakeSetupPanelBrush(
		const FLinearColor& Fill, const FLinearColor& Outline, const float Radius)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Brush.OutlineSettings.Color = FSlateColor(Outline);
		Brush.OutlineSettings.Width = 1.f;
		return Brush;
	}

	// =========================================================================
	// WBP_FF_AtlasGlobe — 캐주얼 스타일 왼쪽 패널 하나 (2026-09-11 개편)
	// =========================================================================
	//
	// CanvasPanel_Root
	// ├ Border_DragSurface                    ← 첫 자식. 빈 곳을 끌어 지구를 돌린다
	// ├ CanvasPanel_Pins                      ← 화면 전체. 프랍 위 핀
	// └ Border_LeftPanel (세로로 길게)
	//   └ VerticalBox_LeftColumn
	//     ├ Text_CountryHeading
	//     ├ 구분선
	//     ├ ScrollBox_Countries (Fill)        ← **나라 목록이 화면 높이를 다 쓴다**
	//     ├ ScrollBox_CountryDetails (Auto)
	//     │   └ Border_Panel → VerticalBox_Panel
	//     │       ├ Text_CountryName
	//     │       ├ Text_FestivalCount        ← 상태 한 줄 (`축제 5개`)
	//     │       ├ Image_LockedIcon + Text_LockReason
	//     │       ├ Button_SelectCountry      `축제 지도 보기` (주 동작)
	//     │       └ Button_ClosePanel         `닫기`
	//     └ Text_ControlHints
	//
	// 🔴 **축제 종류 목록을 두지 않는다** (사용자 결정 2026-09-11). 어떤 축제가 있는지는
	// 국가 지도에서 본다. 같은 정보를 두 곳에 두면 지도로 갈 이유가 없고, 왼쪽 패널은
	// 나라 목록이 길게 보이는 것이 값어치다. `Image_Preview`·`Text_Description` 도 만들지 않는다 —
	// 화면 코드는 두 바인딩이 없어도 동작한다 (`BindWidgetOptional`).
	bool BuildCasualCountryRow(UWidgetBlueprint& Blueprint,UWidgetTree& Tree)
    {
        FFFrontendCasual::FBuilder B{Blueprint,Tree};
        auto* Root=B.Make<USizeBox>(TEXT("SizeBox_Root"),true);Root->SetHeightOverride(54);Tree.RootWidget=Root;
        auto* Overlay=B.Make<UOverlay>(TEXT("Overlay_Root"));Root->AddChild(Overlay);
        auto* Ring=B.Make<UImage>(TEXT("Image_FocusRing"),true);Ring->SetBrush(FFFrontendCasual::Rounded(FLinearColor::Transparent,B.T.Teal,16,3));Ring->SetVisibility(ESlateVisibility::Hidden);
        auto* RS=Overlay->AddChildToOverlay(Ring);RS->SetHorizontalAlignment(HAlign_Fill);RS->SetVerticalAlignment(VAlign_Fill);
        auto* Row=B.Make<UHorizontalBox>(TEXT("HorizontalBox_Content"));auto* OS=Overlay->AddChildToOverlay(Row);OS->SetHorizontalAlignment(HAlign_Fill);OS->SetVerticalAlignment(VAlign_Center);OS->SetPadding(FMargin(18,0));
        auto* Flag=B.Icon(TEXT("Image_CountryFlag"),TEXT("Flag_KR"),40,true);B.H(Row,B.Sized(TEXT("SizeBox_Flag"),Flag,42,30),18);
        auto* Label=B.Make<UCommonTextBlock>(TEXT("Text_Label"),true);
        Label->SetStyle(LoadClass<UCommonTextStyle>(nullptr,TEXT("/Game/FestaFiesta/UI/Styles/CTS_FF_FrontendInk.CTS_FF_FrontendInk_C")));
        Label->SetText(LOCTEXT("CountryRowPlaceholder","대한민국"));B.H(Row,Label,12,true);
        B.H(Row,B.Sized(TEXT("SizeBox_Status"),B.Text(TEXT("Text_PinBadge"),LOCTEXT("CountryStatusPlaceholder","준비 중"),16,true,true),104,0),14);
        auto* Arrow=B.Icon(TEXT("Image_Chevron"),TEXT("Chevron"),16,true);Arrow->SetColorAndOpacity(B.T.Ink);B.H(Row,Arrow);
        return true;
    }

    bool BuildAtlasGlobeSetupLayout(UWidgetBlueprint& Blueprint,UWidgetTree& Tree,
        UClass* RowClass,UClass* NavButtonClass,UClass* PrimaryButtonClass)
    {
        if(!RowClass || !NavButtonClass || !PrimaryButtonClass)return false;
        FFFrontendCasual::FBuilder B{Blueprint,Tree};auto* Root=B.Make<UCanvasPanel>(TEXT("CanvasPanel_Root"));Tree.RootWidget=Root;
        auto* Drag=B.Make<UBorder>(TEXT("Border_DragSurface"),true);Drag->SetBrushColor(FLinearColor::Transparent);
        StretchOnCanvas(*Root,*Drag);auto* Pins=B.Make<UCanvasPanel>(TEXT("CanvasPanel_Pins"),true);Pins->SetVisibility(ESlateVisibility::SelfHitTestInvisible);StretchOnCanvas(*Root,*Pins);
        auto* Column=B.Make<UVerticalBox>(TEXT("VerticalBox_LeftColumn"));B.SidePanel(Root,Column,620,56);
        B.Heading(Column,LOCTEXT("AtlasCountriesHeading","어느 나라로 떠날까요?"),LOCTEXT("AtlasCountriesSubheading","축제를 열고 싶은 나라를 선택하세요!"),29);
        auto* Countries=B.Make<UScrollBox>(TEXT("ScrollBox_Countries"),true);Countries->SetConsumeMouseWheel(EConsumeMouseWheel::Always);Countries->SetClipping(EWidgetClipping::ClipToBounds);
        B.V(Column,Countries,16,true);
        auto* Details=B.Make<UScrollBox>(TEXT("ScrollBox_CountryDetails"),true);Details->SetVisibility(ESlateVisibility::Collapsed);Details->SetClipping(EWidgetClipping::ClipToBounds);B.V(Column,Details,14);
        auto* Content=B.Make<UVerticalBox>(TEXT("VerticalBox_Panel"));auto* Panel=B.Panel(TEXT("Border_Panel"),Content,18,22,false,true);Details->AddChild(Panel);
        auto* Info=B.Make<UHorizontalBox>(TEXT("HorizontalBox_CountryInfo"));
        auto* Flag=B.Icon(TEXT("Image_CountryFlag"),TEXT("Flag_KR"),72,true);B.H(Info,B.Sized(TEXT("SizeBox_CountryFlag"),Flag,98,66),20);
        auto* Text=B.Make<UVerticalBox>(TEXT("VerticalBox_CountryInfo"));
        B.V(Text,B.Text(TEXT("Text_CountryName"),LOCTEXT("AtlasCountryPlaceholder","대한민국"),26,false,true),6);
        B.V(Text,B.Text(TEXT("Text_FestivalCount"),FText::GetEmpty(),17,true,true));B.H(Info,Text,0,true);B.V(Content,Info,14);
        auto* Lock=B.Make<UHorizontalBox>(TEXT("HorizontalBox_Lock"));
        auto* LockIcon=B.Icon(TEXT("Image_LockedIcon"),TEXT("Lock"),20,true);LockIcon->SetColorAndOpacity(B.T.InkMuted);B.H(Lock,LockIcon,10);
        B.H(Lock,B.Text(TEXT("Text_LockReason"),FText::GetEmpty(),16,true,true),0,true);B.V(Content,Lock,8);
        B.V(Content,B.Button(TEXT("Button_SelectCountry"),PrimaryButtonClass,LOCTEXT("AtlasGlobeSelectCountry","축제 지도 보기"),518,56,TEXT("Map")),8);
        B.V(Content,B.Button(TEXT("Button_ClosePanel"),NavButtonClass,LOCTEXT("AtlasGlobeClosePanel","닫기"),518,50,TEXT("Folder")));
        B.V(Column,B.Text(TEXT("Text_ControlHints"),LOCTEXT("AtlasGlobeHintPlaceholder","드래그: 회전   국가 목록 / 조형물: 살펴보기\nEsc: 뒤로"),14,true,true));
        return true;
    }

	/**
	 * 🔴 **파괴적이다.** `WBP_FF_AtlasGlobe` 의 위젯 트리를 통째로 교체한다.
	 * 손으로 마감한 뒤에는 다시 실행하지 않는다. 나라 행(`WBP_FF_AtlasCountryRow`)과
	 * 국가 지도 화면은 건드리지 않는다.
	 */
	void ApplyAtlasGlobeSetupStyle()
	{
		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_BEGIN: 지구 화면을 캐주얼 스타일 왼쪽 패널로 다시 만든다."), AtlasLogPrefix);

		UClass* NavButtonClass = LoadGeneratedClass(FrontendButtonPackage);
		UClass* PrimaryButtonClass = LoadGeneratedClass(FrontendPrimaryButtonPackage);
		if (!NavButtonClass || !PrimaryButtonClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_ABORT: 프론트엔드 버튼 WBP 가 없다. 먼저 `FF.ApplyFrontendSetupStyle` 을 실행한다."),
				AtlasLogPrefix);
			return;
		}

		const FString RowPackage=AtlasFolder/TEXT("WBP_FF_AtlasCountryRow");
        auto* RowBP=ReplaceWidgetBlueprint(AtlasLogPrefix,RowPackage,AtlasFolder,*UFF_AtlasPinButton::StaticClass(),
            [](UWidgetBlueprint& BP,UWidgetTree& Tree){return BuildCasualCountryRow(BP,Tree);});
        if(!RowBP || !RowBP->GeneratedClass)return;
        auto* RowDefaults=RowBP->GeneratedClass->GetDefaultObject<UFF_AtlasPinButton>();
        RowBP->Modify();RowDefaults->Modify();
        const auto Ink=FFFrontendCasual::Get().Ink;
        RowDefaults->ApplyGeneratedButtonDefaults(LoadClass<UCommonButtonStyle>(nullptr,
            TEXT("/Game/FestaFiesta/UI/Styles/CBS_FF_FrontendNav.CBS_FF_FrontendNav_C")),Ink,Ink,Ink,false,false,false);
        RowDefaults->SetIsSelectable(true);RowDefaults->SetIsToggleable(true);SaveAssetPackage(AtlasLogPrefix,*RowBP);
        UClass* RowClass=RowBP->GeneratedClass;

		UWidgetBlueprint* Globe = ReplaceWidgetBlueprint(
			AtlasLogPrefix, GlobePackage, AtlasFolder,
			*UFF_AtlasGlobeScreen::StaticClass(),
			[RowClass, NavButtonClass, PrimaryButtonClass](UWidgetBlueprint& BP, UWidgetTree& Tree)
			{
				return BuildAtlasGlobeSetupLayout(BP, Tree, RowClass, NavButtonClass, PrimaryButtonClass);
			});

		if (!Globe || !Globe->GeneratedClass)
		{
			return;
		}

		// UI 국기는 CDO가 강하게 참조해서 쿠킹에도 포함한다.
        auto* Defaults=Globe->GeneratedClass->GetDefaultObject<UFF_AtlasGlobeScreen>();
        Defaults->Modify();
        for(const TCHAR* Code : {TEXT("KR"),TEXT("BR"),TEXT("DE"),TEXT("JP"),TEXT("US"),TEXT("ES"),TEXT("TH"),TEXT("AU")})
            Defaults->CountryFlagTextures.Add(FName(Code),LoadObject<UTexture2D>(nullptr,*MakeObjectPath(FFFrontendCasual::ArtPackage(FString(TEXT("Flag_"))+Code))));
        // PinButtonClass는 나라 목록에만 쓰고 지구 액터는 변경하지 않는다.
		if (FClassProperty* Property =
			FindFProperty<FClassProperty>(Globe->GeneratedClass, TEXT("PinButtonClass")))
		{
			Property->SetObjectPropertyValue_InContainer(
				Globe->GeneratedClass->GetDefaultObject(), RowClass);
			SaveAssetPackage(AtlasLogPrefix, *Globe);
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_DONE: 지구 화면 적용 완료. 나라 목록은 세로로 길고 축제 종류 목록은 없다."),
			AtlasLogPrefix);
	}

	FAutoConsoleCommand ApplyAtlasGlobeSetupStyleCommand(
		TEXT("FF.ApplyAtlasGlobeSetupStyle"),
		TEXT("Rebuilds WBP_FF_AtlasGlobe as a Clean Casual left panel (country list + "
			 "status + map/close actions). DESTRUCTIVE: the existing layout is replaced."),
		FConsoleCommandDelegate::CreateStatic(&ApplyAtlasGlobeSetupStyle));

}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
