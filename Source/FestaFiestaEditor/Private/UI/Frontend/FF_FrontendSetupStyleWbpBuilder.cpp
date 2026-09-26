// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// FF_FrontendSetupStyleWbpBuilder.cpp

#include "CoreMinimal.h"

#if WITH_EDITOR

// 타이틀의 크림·민트·숲색을 플레이 허브·옵션·저장 목록으로 확장한다.
// FF.ApplyFrontendSetupStyle은 기존 프런트엔드 에셋을 재생성한다.
// 인게임 ESC·준비·라이브 HUD와 지구 씬에는 쓰지 않는다.

#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SafeZone.h"
#include "Components/SafeZoneSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Blueprint.h"
#include "Engine/Font.h"
#include "Factories/BlueprintFactory.h"
#include "HAL/IConsoleManager.h"
#include "IAssetTools.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Styling/SlateBrush.h"
#include "UI/Frontend/FF_FrontendCasualBuilder.h"
#include "UI/Frontend/FF_SaveListScreen.h"
#include "UI/Frontend/FF_SaveSlotCard.h"
#include "UObject/UnrealType.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/FF_CommonButtonBase.h"
#include "UI/FF_UIDisplayTextLibrary.h"
#include "UI/Frontend/FF_OptionsScreen.h"
#include "UI/Frontend/FF_PlayHubScreen.h"
#include "UI/Settings/FF_KeyBindingPanel.h"
#include "UI/Settings/FF_SettingSelectRow.h"
#include "UI/Settings/FF_SettingSliderRow.h"
#include "UI/Settings/FF_SettingToggleRow.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

namespace FFFrontendSetupStyleWbpBuilder
{
	using namespace FFUIWbpBuilder;

	const TCHAR* const LogPrefix = TEXT("FF_FRONTEND_SETUP_UMG");

	const FString FrontendFolder(TEXT("/Game/FestaFiesta/UI/Frontend"));
	const FString StyleFolder(TEXT("/Game/FestaFiesta/UI/Styles"));
	const FString SettingsFolder(TEXT("/Game/FestaFiesta/UI/Settings"));

	const FString ScreenPackage(FrontendFolder / TEXT("WBP_FF_Options"));
	const FString PlayHubPackage(FrontendFolder / TEXT("WBP_FF_PlayHub"));
	const FString ButtonPackage(FrontendFolder / TEXT("WBP_FF_FrontendButton"));
	const FString PrimaryButtonPackage(FrontendFolder / TEXT("WBP_FF_FrontendPrimaryButton"));
	const FString StylePackage(StyleFolder / TEXT("CBS_FF_FrontendNav"));
	const FString PrimaryStylePackage(StyleFolder / TEXT("CBS_FF_FrontendPrimary"));
	const FString TextStylePackage(StyleFolder / TEXT("CTS_FF_FrontendInk"));
	const FString PrimaryTextStylePackage(StyleFolder / TEXT("CTS_FF_FrontendOnTeal"));

	// 행 위젯은 한광훈님 ESC 옵션의 것을 그대로 쓴다. 새로 만들지 않는다.
	const FString ToggleRowPackage(SettingsFolder / TEXT("WBP_FF_SettingToggleRow"));
	const FString SliderRowPackage(SettingsFolder / TEXT("WBP_FF_SettingSliderRow"));
	const FString SelectRowPackage(SettingsFolder / TEXT("WBP_FF_SettingSelectRow"));
	const FString KeyBindingPanelPackage(SettingsFolder / TEXT("WBP_FF_KeyBindingPanel"));

	namespace Layout
	{
		/** 세로 리듬과 레일 폭만 고정한다. 나머지 크기는 UMG 가 뷰포트에서 정한다. */
		constexpr float HeaderHeight = 126.f;
		constexpr float FooterHeight = 112.f;
		constexpr float Margin = 48.f;
		constexpr float MainTopPad = 28.f;
		constexpr float MainBottomPad = 20.f;

		constexpr float RailWidth = 320.f;
		constexpr float RailToBodyGap = 32.f;
		constexpr float CategoryHeight = 64.f;
		constexpr float CategoryGap = 12.f;

		constexpr float BodyPad = 28.f;

		// 왼쪽 패널(플레이 허브·지구 화면)에서 쓰는 값.
		constexpr float PanelPad = 28.f;
		constexpr float SidePanelX = 64.f;
		constexpr float SidePanelWidth = 620.f;
		constexpr float SidePanelTopAnchor = 0.08f;
		constexpr float SidePanelBottomAnchor = 0.92f;

		constexpr float ActionWidth = 220.f;
		constexpr float ActionHeight = 60.f;
		constexpr float ActionGap = 16.f;
	}

	/**
	 * 아트 없이 Slate 가 직접 칠하는 단색 면.
	 *
	 * 🔴 **화면 전체를 덮는 면에 9-slice 패널 아트를 쓰지 않는다.** `Panel_Body` 류는 모서리가
	 * 둥글고 가장자리에 투명 여백이 있어 화면 끝까지 덮이지 않는다 — 2026-09-10 실기에서
	 * 오른쪽에 지구본이 띠처럼 보인 원인이다. 셋업 HUD 의 바(`Border_StatusRow`)도 단색이다.
	 */
	FSlateBrush MakeFlatBrush(const FLinearColor& Color)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.TintColor = FSlateColor(Color);
		return Brush;
	}

	/** 모서리를 둥글게 칠하는 면. 본문 패널처럼 테두리가 필요한 곳에 쓴다. */
	FSlateBrush MakeRoundedBrush(
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

	/**
	 * 이미 있으면 그것을, 없으면 새로 만든다. 위젯이 아닌 일반 Blueprint(CBS_)용이다.
	 *
	 * 에디터가 콘텐츠 브라우저에서 쓰는 것과 같은 경로(`IAssetTools::CreateAsset` + `UBlueprintFactory`)를
	 * 쓴다. 패키지를 직접 만들고 `FKismetEditorUtilities::CreateBlueprint` 를 부르는 짧은 길도 있지만,
	 * 그쪽은 에셋 레지스트리 등록·GC 처리를 호출부가 떠안는다.
	 */
	UBlueprint* EnsurePlainBlueprint(const FString& PackageName, UClass& ParentClass)
	{
		if (UBlueprint* Existing = LoadObject<UBlueprint>(nullptr, *MakeObjectPath(PackageName)))
		{
			// 이름이 같아도 부모가 다르면 손대지 않는다 — 남의 에셋을 지우지 않기 위함이다.
			if (!Existing->ParentClass || !Existing->ParentClass->IsChildOf(&ParentClass))
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_FAIL: %s 의 부모가 %s 가 아니다. 손대지 않았다."),
					LogPrefix, *PackageName, *ParentClass.GetName());
				return nullptr;
			}
			return Existing;
		}
		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_NEW: %s 가 없어 새로 만든다."), LogPrefix, *PackageName);

		UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
		if (!Factory)
		{
			return nullptr;
		}
		Factory->ParentClass = &ParentClass;

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(
			TEXT("AssetTools")).Get();
		UBlueprint* Created = Cast<UBlueprint>(AssetTools.CreateAsset(
			FPackageName::GetShortName(PackageName),
			FPackageName::GetLongPackagePath(PackageName),
			UBlueprint::StaticClass(),
			Factory));

		if (!Created)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: %s 생성 실패."), LogPrefix, *PackageName);
			return nullptr;
		}

		return Created;
	}

	// =========================================================================
	// 1. CTS_FF_FrontendInk — 버튼 라벨의 글자 스타일
	// =========================================================================
	//
	// 🔴 **이것이 없으면 호버한 뒤 라벨이 사라진다.** (2026-09-10 실기에서 확인)
	//
	// `UCommonTextBlock` 은 `Style` 이 비어 있으면 CommonUI 프로젝트 설정의 기본 텍스트
	// 스타일로 되돌아간다. 그 색이 밝은 글자라 종이색 버튼 위에서 글자가 사라진다.
	// 게다가 `UpdateFromStyle()` 이 `SynchronizeProperties` 마다 다시 돌아
	// (버튼이 호버·언호버에서 `Invalidate` 를 부른다) C++ 이 칠한 색을 덮는다.
	// → 라벨 색의 주인을 **스타일 하나**로 정한다. 그래서 버튼의 `bTintLabel` 은 false 다.
	bool ApplyTextStyle(
		const FString& PackageName,
		const FLinearColor& Color,
		TSubclassOf<UCommonTextStyle>& OutTextStyleClass)
	{
		UBlueprint* StyleBlueprint = EnsurePlainBlueprint(
			PackageName, *UCommonTextStyle::StaticClass());
		if (!StyleBlueprint)
		{
			return false;
		}

		UCommonTextStyle* Style = StyleBlueprint->GeneratedClass
			? Cast<UCommonTextStyle>(StyleBlueprint->GeneratedClass->GetDefaultObject()) : nullptr;
		if (!Style)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: %s 의 CDO 를 얻지 못했다."), LogPrefix, *PackageName);
			return false;
		}

		Style->Modify();

		FSlateFontInfo Font = Style->Font;
		if (UFont* UiFont = LoadObject<UFont>(nullptr, *MakeObjectPath(GetUiFontPackage())))
		{
			Font.FontObject = UiFont;
			Font.TypefaceFontName = TEXT("SemiBold");
		}
		Font.Size = 20;
		Style->Font = Font;

		Style->Color = Color;
		Style->bUsesDropShadow = false;
		Style->Margin = FMargin(0.f);
		Style->LineHeightPercentage = 1.f;

		if (!SaveAssetPackage(LogPrefix, *StyleBlueprint))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: %s 저장 실패."), LogPrefix, *PackageName);
			return false;
		}

		OutTextStyleClass = StyleBlueprint->GeneratedClass;
		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_TEXTSTYLE: %s 적용 완료."), LogPrefix, *PackageName);
		return true;
	}

	// =========================================================================
	// 2. CBS_FF_FrontendNav — 버튼 배경 상태
	// =========================================================================
	//
	// 최소 크기를 스타일에 박지 않는다 (`MinWidth = 0`). 분류 버튼(320x56)과 하단 동작 버튼(220x56)이
	// **같은 스타일을 공유**하므로, 크기는 인스턴스의 `ButtonSizeOverride` 가 정해야 한다.
	bool ApplyButtonStyle(
		const FString& PackageName,
		const TSubclassOf<UCommonTextStyle>& TextStyleClass,
		const bool bPrimary,
		TSubclassOf<UCommonButtonStyle>& OutStyleClass)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 팔레트가 없다."), LogPrefix);
			return false;
		}

		UBlueprint* StyleBlueprint = EnsurePlainBlueprint(PackageName, *UCommonButtonStyle::StaticClass());
		if (!StyleBlueprint)
		{
			return false;
		}

		if (!StyleBlueprint->GeneratedClass)
		{
			FKismetEditorUtilities::CompileBlueprint(StyleBlueprint);
		}

		UCommonButtonStyle* Style = StyleBlueprint->GeneratedClass
			? Cast<UCommonButtonStyle>(StyleBlueprint->GeneratedClass->GetDefaultObject()) : nullptr;
		if (!Style)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: %s 의 CDO 를 얻지 못했다."), LogPrefix, *StylePackage);
			return false;
		}

		const FFFrontendCasual::FTokens Tokens = FFFrontendCasual::Get();

		Style->Modify();
		Style->bSingleMaterial = false;

		const auto Surface = [&](const FLinearColor& Fill, const FLinearColor& Line, float Width=1.5f)
        { return FFFrontendCasual::Rounded(Fill,Line,18.f,Width); };
        Style->NormalBase = Surface(bPrimary?Tokens.Teal:Tokens.Paper,Tokens.LineStrong);
        Style->NormalHovered = Surface(bPrimary?Tokens.TealDeep:Tokens.Hover,Tokens.Teal,2.f);
        Style->NormalPressed = Surface(bPrimary?Tokens.TealDeep:Tokens.TealSoft,Tokens.Teal,3.f);
        Style->SelectedBase = Surface(bPrimary?Tokens.TealDeep:Tokens.TealSoft,Tokens.Teal,2.5f);
        Style->SelectedHovered = Style->SelectedBase;
        Style->SelectedPressed = Style->NormalPressed;
        Style->Disabled = Surface(Tokens.Disabled,Tokens.Line);

		// 상태별 글자 스타일도 같은 CTS 하나로 채운다. CommonButton 이 이 값을 자식 라벨에
		// 자동으로 적용하지는 않지만(5.7 확인), 비워 두면 나중에 BP 가 참조할 때 빈 값이 나온다.
		Style->NormalTextStyle = TextStyleClass;
		Style->NormalHoveredTextStyle = TextStyleClass;
		Style->SelectedTextStyle = TextStyleClass;
		Style->SelectedHoveredTextStyle = TextStyleClass;
		Style->DisabledTextStyle = TextStyleClass;

		Style->MinWidth = 0;
		Style->MinHeight = static_cast<int32>(Layout::CategoryHeight);
		Style->MaxWidth = 0;
		Style->MaxHeight = 0;

		// 내용 여백은 버튼 WBP 의 Overlay 슬롯이 갖는다. 여기서 또 주면 두 번 밀린다.
		Style->ButtonPadding = FMargin(0.f);
		Style->CustomPadding = FMargin(0.f);

		if (!SaveAssetPackage(LogPrefix, *StyleBlueprint))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: %s 저장 실패."), LogPrefix, *PackageName);
			return false;
		}

		OutStyleClass = StyleBlueprint->GeneratedClass;
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_STYLE: %s 적용 완료."), LogPrefix, *PackageName);
		return true;
	}

	// =========================================================================
	// 3. 버튼 WBP 2종 — 라벨만 있는 버튼 (분류·뒤로 / 적용)
	// =========================================================================
	//
	// SizeBox_Root 320x56
	// └ Overlay_Root
	//   ├ Image_FocusSurface   ← 마우스 호버·패드 포커스에서 같은 그림을 내기 위한 표면
	//   └ Text_Label           ← UCommonTextBlock 이어야 BindWidget 이 붙는다
	//
	// 아이콘·Chevron 을 두지 않는다. 분류가 6개뿐이고 글자만으로 구분되며,
	// 아이콘을 넣으면 9종 아이콘 인벤토리에 6개가 더 늘어난다 (icon-inventory.md).
	bool BuildButtonLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree,
        const TSubclassOf<UCommonTextStyle>& TextStyleClass, const bool bPrimary)
    {
        FFFrontendCasual::FBuilder B{Blueprint,Tree};
        auto* Root=B.Make<USizeBox>(TEXT("SizeBox_Root"),true);
        auto* Overlay=B.Make<UOverlay>(TEXT("Overlay_Root"));
        Root->SetWidthOverride(Layout::RailWidth);Root->SetHeightOverride(Layout::CategoryHeight);
        Root->AddChild(Overlay);Tree.RootWidget=Root;
        // 내부를 덮지 않고 포커스는 테두리로만 나타낸다.
        auto* Ring=B.Make<UImage>(TEXT("Image_FocusRing"),true);
        Ring->SetBrush(FFFrontendCasual::Rounded(FLinearColor::Transparent,
            bPrimary?B.T.LineStrong:B.T.Teal,18,3));
        Ring->SetVisibility(ESlateVisibility::Hidden);
        auto* RS=Overlay->AddChildToOverlay(Ring);RS->SetHorizontalAlignment(HAlign_Fill);RS->SetVerticalAlignment(VAlign_Fill);
        auto* Row=B.Make<UHorizontalBox>(TEXT("HorizontalBox_Content"));
        auto* OS=Overlay->AddChildToOverlay(Row);OS->SetHorizontalAlignment(HAlign_Fill);OS->SetVerticalAlignment(VAlign_Center);OS->SetPadding(FMargin(22,0));
        auto* Icon=B.Icon(TEXT("Image_Icon"),TEXT("Spark"),28,true);
        B.H(Row,B.Sized(TEXT("SizeBox_Icon"),Icon,32,32),16);
        auto* Label=B.Make<UCommonTextBlock>(TEXT("Text_Label"),true);
        Label->SetStyle(TextStyleClass);Label->SetText(LOCTEXT("FrontendButtonPlaceholder","선택"));
        Label->SetVisibility(ESlateVisibility::HitTestInvisible);B.H(Row,Label,12,true);
        auto* Chevron=B.Icon(TEXT("Image_Chevron"),TEXT("Chevron"),18,true);
        B.H(Row,B.Sized(TEXT("SizeBox_Chevron"),Chevron,18,18));
        return true;
    }

	// =========================================================================
	// 4. WBP_FF_Options — 좌측 분류 6개 + 우측 설정 행
	// =========================================================================
	//
	// Overlay_Root
	// ├ Border_Backdrop                      ← 화면 전체를 덮는 종이 면
	// └ SafeZone → VerticalBox_Shell
	//   ├ SizeBox_Header (88) → Border_Header → HorizontalBox_Header → Text_ScreenTitle
	//   ├ HorizontalBox_Main (Fill)
	//   │   ├ SizeBox_Rail (320) → VerticalBox_Categories → Button_Language … Button_Credits
	//   │   └ Border_Body (Fill) → VerticalBox_BodyContent
	//   │       ├ Text_CategoryTitle
	//   │       ├ SizeBox_ContentRule → Border_ContentRule
	//   │       ├ Text_NotImplemented       ← 아직 붙지 않은 분류의 사유
	//   │       └ ScrollBox_Rows → VerticalBox_Rows
	//   └ SizeBox_Footer (104) → Border_Footer → HorizontalBox_Footer
	//       ├ Text_ApplyHint (Fill)
	//       ├ Button_Apply
	//       └ Button_Back
	//
	// 🔴 **좌표를 박지 않는다.** 세로 리듬(머리 88 · 바닥 104)과 레일 폭(320)만 고정이고
	// 나머지 크기는 UMG 가 뷰포트에서 정한다. 처음에는 1920x1080 캔버스에 절대 좌표로 놓았는데
	// **DPI 배율(`UIScaleRule=ShortestSide`)이 걸린 창에서는 화면 오른쪽이 덮이지 않아**
	// 뒤의 지구본이 띠처럼 보였다 (2026-09-10 실기 확인).
	//
	// 🔴 이름이 `UFF_OptionsScreen` 의 BindWidget 과 정확히 같아야 한다.
	//    분류 버튼 6개와 `Button_Back` 은 필수이고 나머지는 Optional 이다.
	bool BuildOptionsScreenLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree,
		UClass* ButtonClass, UClass* PrimaryButtonClass)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette || !ButtonClass || !PrimaryButtonClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 팔레트 또는 버튼 클래스가 없어 WBP_FF_Options 를 만들지 않았다."), LogPrefix);
			return false;
		}

		const FFFrontendCasual::FTokens Tokens = FFFrontendCasual::Get();

		UOverlay* Root = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Root"));
		UBorder* Backdrop = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Backdrop"));
		USafeZone* SafeZone = MakeWidget<USafeZone>(Blueprint, Tree, TEXT("SafeZone"));
		UVerticalBox* Shell = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Shell"));

		USizeBox* HeaderBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Header"));
		UBorder* Header = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Header"));
		UHorizontalBox* HeaderRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Header"));
		UTextBlock* ScreenTitle = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_ScreenTitle"));

		UHorizontalBox* Main = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Main"));
		USizeBox* RailBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Rail"));
		UVerticalBox* Categories = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Categories"));

		UBorder* Body = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Body"));
		UVerticalBox* BodyContent = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_BodyContent"));
		UTextBlock* CategoryTitle = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CategoryTitle"), true);
		USizeBox* RuleBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_ContentRule"));
		UBorder* Rule = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_ContentRule"));
		UTextBlock* NotImplemented = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_NotImplemented"), true);
		UScrollBox* RowsScroll = MakeWidget<UScrollBox>(Blueprint, Tree, TEXT("ScrollBox_Rows"));
		UVerticalBox* Rows = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Rows"), true);

		USizeBox* FooterBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Footer"));
		UBorder* Footer = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Footer"));
		UHorizontalBox* FooterRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Footer"));
		UTextBlock* ApplyHint = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_ApplyHint"), true);
		// `적용` 만 주 동작 스타일이다. `뒤로` 와 분류는 같은 종이색 버튼을 쓴다.
		UUserWidget* Apply = MakeWidgetOfClass(Blueprint, Tree, TEXT("Button_Apply"), PrimaryButtonClass);
		UUserWidget* Back = MakeWidgetOfClass(Blueprint, Tree, TEXT("Button_Back"), ButtonClass);

		if (!Root || !Backdrop || !SafeZone || !Shell || !HeaderBox || !Header || !HeaderRow ||
			!ScreenTitle || !Main || !RailBox || !Categories || !Body || !BodyContent ||
			!CategoryTitle || !RuleBox || !Rule || !NotImplemented || !RowsScroll || !Rows ||
			!FooterBox || !Footer || !FooterRow || !ApplyHint || !Apply || !Back)
		{
			return false;
		}

		Tree.RootWidget = Root;

		// --- 화면 전체를 덮는 면 ---
		// 알파를 섞지 않는다. 반투명이면 지구본이 종이 밑에서 비친다.
		Backdrop->SetBrush(MakeFlatBrush(Tokens.PaperSunken));
		// 🔴 여기만 `Visible` 이다. 옵션은 화면을 덮는 화면이라 뒤로 마우스가 새면
		// 배경의 지구본이 드래그로 돌아간다. HUD 루트를 SelfHitTestInvisible 로 두는 규칙
		// (prep-hud-reference 6절) 은 월드를 계속 조작해야 하는 비모달 HUD 의 규칙이다.
		Backdrop->SetVisibility(ESlateVisibility::Visible);
		if (UOverlaySlot* Slot = Root->AddChildToOverlay(Backdrop))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}

		if (UOverlaySlot* Slot = Root->AddChildToOverlay(SafeZone))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}

		if (USafeZoneSlot* Slot = Cast<USafeZoneSlot>(SafeZone->AddChild(Shell)))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
			Slot->SetPadding(FMargin(0.f));
		}

		// --- 머리 띠 ---
		HeaderBox->SetHeightOverride(Layout::HeaderHeight);
		HeaderBox->AddChild(Header);
		Header->SetBrush(MakeFlatBrush(Tokens.Paper));
		Header->SetHorizontalAlignment(HAlign_Fill);
		Header->SetVerticalAlignment(VAlign_Fill);
		Header->SetPadding(FMargin(Layout::Margin, 0.f));
		Header->AddChild(HeaderRow);

		StyleText(*ScreenTitle, 34, Tokens.Ink, TEXT("SemiBold"));
		ScreenTitle->SetText(LOCTEXT("OptionsScreenTitle", "옵션"));
		if (UHorizontalBoxSlot* Slot = HeaderRow->AddChildToHorizontalBox(ScreenTitle))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		FFFrontendCasual::FBuilder HeaderBuilder{Blueprint,Tree};
        HeaderBuilder.H(HeaderRow,HeaderBuilder.Text(TEXT("Text_OptionsSubtitle"),
            LOCTEXT("OptionsSubtitle","나에게 맞는 축제 여행을 준비하세요"),18,true),24);
        auto* Brand=HeaderBuilder.Make<UImage>(TEXT("Image_BrandMark"));
        Brand->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,TEXT("/Game/FestaFiesta/UI/Title/CleanCasual/Brand/T_FF_Title_CasualBright_Logo_512.T_FF_Title_CasualBright_Logo_512")));
        Brand->SetOpacity(0.55f);HeaderBuilder.H(HeaderRow,HeaderBuilder.Sized(TEXT("SizeBox_BrandMark"),Brand,70,70));
		if (UVerticalBoxSlot* Slot = Shell->AddChildToVerticalBox(HeaderBox))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}

		// --- 가운데 (레일 + 본문) ---
		if (UVerticalBoxSlot* Slot = Shell->AddChildToVerticalBox(Main))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetPadding(FMargin(
				Layout::Margin, Layout::MainTopPad, Layout::Margin, Layout::MainBottomPad));
		}

		// 버튼을 런타임에 채우지 않는다. 디자이너에서 하나하나 보이고 만질 수 있어야 한다.
		const TArray<TPair<FName, EFF_OptionCategory>> CategoryButtons = {
			{ TEXT("Button_Language"),		EFF_OptionCategory::Language },
			{ TEXT("Button_Accessibility"),	EFF_OptionCategory::Accessibility },
			{ TEXT("Button_Sound"),			EFF_OptionCategory::Sound },
			{ TEXT("Button_Graphics"),		EFF_OptionCategory::Graphics },
			{ TEXT("Button_Controls"),		EFF_OptionCategory::Controls },
			{ TEXT("Button_Credits"),		EFF_OptionCategory::Credits }
		};

		for (int32 Index = 0; Index < CategoryButtons.Num(); ++Index)
		{
			UUserWidget* Widget = MakeWidgetOfClass(
				Blueprint, Tree, CategoryButtons[Index].Key, ButtonClass);
			UFF_CommonButtonBase* Button = Cast<UFF_CommonButtonBase>(Widget);
			if (!Button)
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_FAIL: 분류 버튼 %s 생성 실패."),
					LogPrefix, *CategoryButtons[Index].Key.ToString());
				return false;
			}

			// 문구는 화면 C++ 이 런타임에 다시 넣는다. 여기 값은 디자이너에서 보이기 위한 것이다.
			Button->ApplyGeneratedButtonContent(
				UFF_UIDisplayTextLibrary::GetOptionCategoryText(CategoryButtons[Index].Value),
				FFFrontendCasual::Art(TArray<FString>{TEXT("Language"),TEXT("Accessibility"),TEXT("Sound"),TEXT("Graphics"),TEXT("Controls"),TEXT("Info")}[Index]),
				FVector2D(Layout::RailWidth, Layout::CategoryHeight));

			const bool bIsLast = (Index == CategoryButtons.Num() - 1);
			if (UVerticalBoxSlot* Slot = Categories->AddChildToVerticalBox(Button))
			{
				Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				Slot->SetHorizontalAlignment(HAlign_Fill);
				Slot->SetVerticalAlignment(VAlign_Top);
				Slot->SetPadding(FMargin(0.f, 0.f, 0.f, bIsLast ? 0.f : Layout::CategoryGap));
			}
		}

		RailBox->SetWidthOverride(Layout::RailWidth);
		RailBox->AddChild(Categories);
		if (UHorizontalBoxSlot* Slot = Main->AddChildToHorizontalBox(RailBox))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Top);
			Slot->SetPadding(FMargin(0.f, 0.f, Layout::RailToBodyGap, 0.f));
		}

		// --- 우측 본문 ---
		Body->SetBrush(MakeRoundedBrush(Tokens.Paper, Tokens.Line, 28.f));
		Body->SetPadding(FMargin(Layout::BodyPad));
		Body->SetHorizontalAlignment(HAlign_Fill);
		Body->SetVerticalAlignment(VAlign_Fill);
		Body->AddChild(BodyContent);
		if (UHorizontalBoxSlot* Slot = Main->AddChildToHorizontalBox(Body))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		StyleText(*CategoryTitle, 28, Tokens.Ink, TEXT("SemiBold"));
		CategoryTitle->SetText(UFF_UIDisplayTextLibrary::GetOptionCategoryText(
			EFF_OptionCategory::Accessibility));
		if (UVerticalBoxSlot* Slot = BodyContent->AddChildToVerticalBox(CategoryTitle))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		}

		Rule->SetBrushColor(Tokens.LineStrong);
		Rule->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		RuleBox->SetHeightOverride(1.f);
		RuleBox->AddChild(Rule);
		if (UVerticalBoxSlot* Slot = BodyContent->AddChildToVerticalBox(RuleBox))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
		}

		// 만져도 아무 일이 없는 조작 UI 는 고장으로 보인다. 사유를 적는 자리다.
		StyleText(*NotImplemented, 16, Tokens.InkMuted, TEXT("Medium"));
		NotImplemented->SetText(LOCTEXT("OptionsPendingPlaceholder", "준비 중인 분류의 사유가 여기 나온다"));
		NotImplemented->SetAutoWrapText(true);
		NotImplemented->SetVisibility(ESlateVisibility::Collapsed);
		if (UVerticalBoxSlot* Slot = BodyContent->AddChildToVerticalBox(NotImplemented))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		}

		// 행이 화면보다 길어질 수 있다 — 크레딧 7줄과 키 배치 패널이 그렇다.
		if (UVerticalBoxSlot* Slot = BodyContent->AddChildToVerticalBox(RowsScroll))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		if (UScrollBoxSlot* Slot = Cast<UScrollBoxSlot>(RowsScroll->AddChild(Rows)))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}

		// --- 바닥 띠 ---
		FooterBox->SetHeightOverride(Layout::FooterHeight);
		FooterBox->AddChild(Footer);
		Footer->SetBrush(MakeFlatBrush(Tokens.PaperSunken));
		Footer->SetHorizontalAlignment(HAlign_Fill);
		Footer->SetVerticalAlignment(VAlign_Fill);
		Footer->SetPadding(FMargin(Layout::Margin, 0.f));
		Footer->AddChild(FooterRow);

		StyleText(*ApplyHint, 16, Tokens.InkMuted, TEXT("Medium"));
		ApplyHint->SetText(LOCTEXT("OptionsApplyHintPlaceholder", "설정을 바꾼 뒤 적용해 주세요"));
		ApplyHint->SetAutoWrapText(true);
		ApplyHint->SetVisibility(ESlateVisibility::Collapsed);
		if (UHorizontalBoxSlot* Slot = FooterRow->AddChildToHorizontalBox(ApplyHint))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		if (UFF_CommonButtonBase* ApplyButton = Cast<UFF_CommonButtonBase>(Apply))
		{
			ApplyButton->ApplyGeneratedButtonContent(
				LOCTEXT("OptionsApply", "적용"), FFFrontendCasual::Art(TEXT("Check")),
				FVector2D(Layout::ActionWidth, Layout::ActionHeight));
		}
		if (UHorizontalBoxSlot* Slot = FooterRow->AddChildToHorizontalBox(Apply))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(0.f, 0.f, Layout::ActionGap, 0.f));
		}

		if (UFF_CommonButtonBase* BackButton = Cast<UFF_CommonButtonBase>(Back))
		{
			BackButton->ApplyGeneratedButtonContent(
				LOCTEXT("OptionsBack", "뒤로"), FFFrontendCasual::Art(TEXT("Back")),
				FVector2D(Layout::ActionWidth, Layout::ActionHeight));
		}
		if (UHorizontalBoxSlot* Slot = FooterRow->AddChildToHorizontalBox(Back))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		if (UVerticalBoxSlot* Slot = Shell->AddChildToVerticalBox(FooterBox))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}

		return true;
	}

	// =========================================================================
	// 5. WBP_FF_PlayHub — 왼쪽 패널 하나
	// =========================================================================
	//
	// Overlay_Root → SafeZone → CanvasPanel_Content → Border_LeftPanel → VerticalBox_Column
	//   Text_ScreenTitle · 구분선 · Button_Continue · (자물쇠 + 기록 없음) ·
	//   Button_NewFestival(주 동작) · Button_SaveList · Panel_Recent · Text_RecentEmpty ·
	//   여백 · Button_Back
	//
	// 🔴 **화면을 덮지 않는다.** 옵션과 다르다 — 여기서는 지구본이 주인공이고 UI 는 왼쪽에만 둔다
	// (`docs/ui/visual-direction.md`). 대신 패널은 **불투명**하다. 반투명으로 두면 밝은 지구 위에서
	// 글자가 읽히지 않는다 — 2026-09-10 실기에서 제목과 버튼 문구가 배경에 묻혀 보였다.
	bool BuildPlayHubLayout(UWidgetBlueprint& Blueprint,UWidgetTree& Tree,UClass* ButtonClass,UClass* PrimaryButtonClass)
    {
        FFFrontendCasual::FBuilder B{Blueprint,Tree};
        auto* Root=B.Make<UCanvasPanel>(TEXT("CanvasPanel_Content"));Tree.RootWidget=Root;
        auto* Column=B.Make<UVerticalBox>(TEXT("VerticalBox_Column"));B.SidePanel(Root,Column);
        B.Heading(Column,LOCTEXT("PlayHubTitle","플레이"),LOCTEXT("PlayHubSubtitle","세상의 축제를 만들어보세요!"),44);
        auto* Scroll=B.Make<UScrollBox>(TEXT("ScrollBox_Content"));
        auto* Content=B.Make<UVerticalBox>(TEXT("VerticalBox_Content"));Scroll->AddChild(Content);B.V(Column,Scroll,20,true);
        auto* Continue=B.Make<UVerticalBox>(TEXT("VerticalBox_Continue"));
        B.V(Continue,B.Button(TEXT("Button_Continue"),ButtonClass,LOCTEXT("PlayHubContinue","최근 축제 계속하기"),504,62,TEXT("Continue")),8);
        B.V(Continue,B.Text(TEXT("Text_ContinueHelp"),LOCTEXT("ContinueHelp","최근에 플레이한 기록을 이어갑니다."),16,true),12);
        auto* Lock=B.Make<UHorizontalBox>(TEXT("HorizontalBox_Locked"));
        auto* LockIcon=B.Icon(TEXT("Image_LockedIcon"),TEXT("Clock"),28,true);LockIcon->SetColorAndOpacity(B.T.InkMuted);
        B.H(Lock,LockIcon,14);B.H(Lock,B.Text(TEXT("Text_LockedNotice"),LOCTEXT("PlayHubNoRecord","기록 없음"),18,true,true),0,true);
        B.V(Continue,B.Panel(TEXT("Border_ContinueStatus"),Lock,16,16,true));
        B.V(Content,B.Panel(TEXT("Border_ContinueCard"),Continue,18,24),18);
        B.V(Content,B.Button(TEXT("Button_NewFestival"),PrimaryButtonClass,LOCTEXT("PlayHubNewFestival","새 축제 시작"),552,94,TEXT("Plus")),8);
        B.V(Content,B.Text(TEXT("Text_NewFestivalHelp"),LOCTEXT("NewFestivalHelp","새로운 축제를 만들러 떠나볼까요?"),16,true),20);
        auto* Saved=B.Make<UVerticalBox>(TEXT("VerticalBox_Saved"));
        B.V(Saved,B.Button(TEXT("Button_SaveList"),ButtonClass,LOCTEXT("PlayHubSaveList","저장 기록 선택"),504,62,TEXT("Save")),10);
        B.V(Saved,B.Text(TEXT("Text_SaveHelp"),LOCTEXT("SaveHelp","저장한 축제 기록을 찾아볼 수 있습니다."),16,true),12);
        auto* Empty=B.Make<UVerticalBox>(TEXT("VerticalBox_Empty"));
        auto* Folder=B.Icon(TEXT("Image_EmptyFolder"),TEXT("Folder"),44);Folder->SetColorAndOpacity(B.T.InkFaint);
        auto* FS=Empty->AddChildToVerticalBox(Folder);FS->SetHorizontalAlignment(HAlign_Center);FS->SetPadding(FMargin(0,4,0,10));
        auto* EmptyText=B.Text(TEXT("Text_RecentEmpty"),LOCTEXT("PlayHubRecentEmpty","저장 기록이 없습니다"),18,true,true);EmptyText->SetJustification(ETextJustify::Center);
        B.V(Empty,EmptyText);B.V(Saved,B.Panel(TEXT("Border_SaveEmpty"),Empty,18,18,true));
        auto* Recent=B.Make<UVerticalBox>(TEXT("Panel_Recent"),true);B.V(Saved,Recent);
        B.V(Content,B.Panel(TEXT("Border_SaveCard"),Saved,18,24));
        B.V(Column,B.Button(TEXT("Button_Back"),ButtonClass,LOCTEXT("PlayHubBack","뒤로"),552,62,TEXT("Back")));
        return true;
    }

    bool BuildSaveListCasual(UWidgetBlueprint& Blueprint,UWidgetTree& Tree,UClass* ButtonClass)
    {
        FFFrontendCasual::FBuilder B{Blueprint,Tree};auto* Root=B.Make<UCanvasPanel>(TEXT("CanvasPanel_Root"));Tree.RootWidget=Root;
        auto* Column=B.Make<UVerticalBox>(TEXT("VerticalBox_Column"));B.SidePanel(Root,Column,820);
        B.Heading(Column,LOCTEXT("SaveListScreenTitle","저장 기록"),LOCTEXT("SaveListSubtitle","다시 만나고 싶은 축제를 찾아보세요."));
        B.V(Column,B.Text(TEXT("Text_SaveSystemNotice"),FText::GetEmpty(),16,true,true),18);
        auto* Scroll=B.Make<UScrollBox>(TEXT("ScrollBox_Saves"));auto* Content=B.Make<UVerticalBox>(TEXT("VerticalBox_Content"));Scroll->AddChild(Content);B.V(Column,Scroll,20,true);
        for(bool Auto : {true,false})
        {
            auto* Group=B.Make<UVerticalBox>(Auto?TEXT("Panel_AutoGroup"):TEXT("Panel_ManualGroup"),true);
            B.V(Group,B.Text(Auto?TEXT("Text_AutoHeader"):TEXT("Text_ManualHeader"),Auto?LOCTEXT("SaveAuto","자동 저장"):LOCTEXT("SaveManual","수동 저장"),24),14);
            B.V(Group,B.Make<UVerticalBox>(Auto?TEXT("VerticalBox_Auto"):TEXT("VerticalBox_Manual"),true));B.V(Content,Group,22);
        }
        B.V(Content,B.Text(TEXT("Text_Empty"),LOCTEXT("SaveListEmpty","저장 기록이 없습니다"),22,true,true));
        B.V(Column,B.Button(TEXT("Button_Back"),ButtonClass,LOCTEXT("SaveListBack","뒤로"),760,62,TEXT("Back")));return true;
    }

    bool BuildSaveCardCasual(UWidgetBlueprint& Blueprint,UWidgetTree& Tree)
    {
        FFFrontendCasual::FBuilder B{Blueprint,Tree};auto* Root=B.Make<UOverlay>(TEXT("Overlay_Root"));Tree.RootWidget=Root;
        auto* Row=B.Make<UHorizontalBox>(TEXT("HorizontalBox_Content"));auto* Info=B.Make<UVerticalBox>(TEXT("VerticalBox_Info"));
        B.V(Info,B.Text(TEXT("Text_FestivalName"),LOCTEXT("SaveCardNamePlaceholder","축제 이름"),24,false,true),5);
        B.V(Info,B.Text(TEXT("Text_Region"),FText::GetEmpty(),16,true,true),4);
        B.V(Info,B.Text(TEXT("Text_DayStage"),FText::GetEmpty(),16,true,true));
        B.H(Row,B.Icon(TEXT("Image_KindIcon"),TEXT("Save"),32,true),20);B.H(Row,Info,20,true);
        auto* Meta=B.Make<UVerticalBox>(TEXT("VerticalBox_Meta"));B.V(Meta,B.Text(TEXT("Text_Kind"),FText::GetEmpty(),16,false,true),6);
        B.V(Meta,B.Text(TEXT("Text_SavedAt"),FText::GetEmpty(),14,true,true));B.H(Row,Meta);
        auto* S=Root->AddChildToOverlay(B.Panel(TEXT("Border_Card"),Row,22,22));S->SetHorizontalAlignment(HAlign_Fill);S->SetPadding(FMargin(0,0,0,12));
        auto* Ring=B.Make<UImage>(TEXT("Image_FocusRing"),true);Ring->SetBrush(FFFrontendCasual::Rounded(FLinearColor::Transparent,B.T.Teal,22,3));Ring->SetVisibility(ESlateVisibility::Hidden);
        auto* R=Root->AddChildToOverlay(Ring);R->SetHorizontalAlignment(HAlign_Fill);R->SetVerticalAlignment(VAlign_Fill);R->SetPadding(FMargin(0,0,0,12));return true;
    }

	// =========================================================================
	// 콘솔 명령
	// =========================================================================
	void ApplyFrontendSetupStyle()
	{
		// 단계마다 로그를 남긴다. 헤드리스로 돌릴 때 어디서 멈췄는지 알 수 있어야 한다.
		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_BEGIN: 프론트엔드 캐주얼 스타일 8종을 다시 만든다."), LogPrefix);

		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_ABORT: 팔레트가 없다."), LogPrefix);
			return;
		}

		// 글자 스타일이 먼저다. 버튼 스타일과 버튼 WBP 가 둘 다 이것을 참조한다.
		TSubclassOf<UCommonTextStyle> InkTextStyle;
		TSubclassOf<UCommonTextStyle> OnTealTextStyle;
		if (!ApplyTextStyle(TextStylePackage, FFFrontendCasual::Get().Ink, InkTextStyle) ||
			!ApplyTextStyle(PrimaryTextStylePackage, FFFrontendCasual::Get().White, OnTealTextStyle))
		{
			return;
		}

		TSubclassOf<UCommonButtonStyle> NavStyle;
		TSubclassOf<UCommonButtonStyle> PrimaryStyle;
		if (!ApplyButtonStyle(StylePackage, InkTextStyle, /*bPrimary=*/ false, NavStyle) ||
			!ApplyButtonStyle(PrimaryStylePackage, OnTealTextStyle, /*bPrimary=*/ true, PrimaryStyle))
		{
			return;
		}

		// 버튼 WBP 2종. 레이아웃은 같고 CDO 의 스타일·라벨 스타일만 다르다.
		struct FButtonVariant
		{
			const FString& Package;
			TSubclassOf<UCommonTextStyle> TextStyle;
			TSubclassOf<UCommonButtonStyle> ButtonStyle;
			FLinearColor ContentColor;
			bool bPrimary = false;
			UClass* GeneratedClass = nullptr;
		};

		FButtonVariant Variants[] = {
			{ ButtonPackage, InkTextStyle, NavStyle, FFFrontendCasual::Get().Ink, false },
			{ PrimaryButtonPackage, OnTealTextStyle, PrimaryStyle, FFFrontendCasual::Get().White, true }
		};

		for (FButtonVariant& Variant : Variants)
		{
			const TSubclassOf<UCommonTextStyle> TextStyleClass = Variant.TextStyle;
			const bool bPrimaryVariant = Variant.bPrimary;
			UWidgetBlueprint* ButtonBlueprint = ReplaceWidgetBlueprint(
				LogPrefix, Variant.Package, FrontendFolder,
				*UFF_CommonButtonBase::StaticClass(),
				[TextStyleClass, bPrimaryVariant](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
				{
					return BuildButtonLayout(Blueprint, Tree, TextStyleClass, bPrimaryVariant);
				});

			if (!ButtonBlueprint || !ButtonBlueprint->GeneratedClass)
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_ABORT: %s 를 만들지 못해 옵션 화면을 건드리지 않았다."),
					LogPrefix, *Variant.Package);
				return;
			}

			// CDO 기본값은 **컴파일 뒤에** 박는다. 컴파일이 CDO 를 다시 만들기 때문이다.
			if (UFF_CommonButtonBase* ButtonCDO =
				Cast<UFF_CommonButtonBase>(ButtonBlueprint->GeneratedClass->GetDefaultObject()))
			{
				// 아이콘·Chevron 틴트용 색이다. 상태별로 바꾸지 않는다.
				//
				// 🔴 `bTintLabel` 은 **false** 다. 라벨 색의 주인은 CTS 하나다.
				// true 로 두면 C++ 틴트와 `UCommonTextBlock::UpdateFromStyle()` 이 서로 덮어써서
				// 호버한 뒤 라벨이 사라지는 상태로 돌아간다 (2026-09-10).
				//
				// 🔴 `bFocusOnHover` 도 **false** 다. 켜 두면 마우스가 지나간 자리에 포커스가 남아
				// **고른 분류가 아닌 버튼이 선택된 것처럼** 보인다. 호버는 CBS 의 Hovered 브러시가,
				// 선택은 Selected 브러시가, 패드 포커스는 `Image_FocusSurface` 가 따로 낸다.
				ButtonBlueprint->Modify();
                ButtonCDO->Modify();
                ButtonCDO->ApplyGeneratedButtonDefaults(
					Variant.ButtonStyle,
					Variant.ContentColor,
					Variant.ContentColor,
					FFFrontendCasual::Get().InkMuted,
					/*bTintLabel=*/ false,
					/*bFocusOnHover=*/ false,
					/*bShowChevronOnlyWhenHighlighted=*/ false);

				if (FFloatProperty* Opacity=FindFProperty<FFloatProperty>(ButtonCDO->GetClass(),TEXT("DisabledOpacity")))
                    Opacity->SetPropertyValue_InContainer(ButtonCDO,1.f);
                if (!SaveAssetPackage(LogPrefix, *ButtonBlueprint))
				{
					UE_LOG(LogFFUIWbpBuilder, Error,
						TEXT("%s_FAIL: %s 의 기본값을 저장하지 못했다."), LogPrefix, *Variant.Package);
					return;
				}
			}

			Variant.GeneratedClass = ButtonBlueprint->GeneratedClass;
		}

		UClass* NavButtonClass = Variants[0].GeneratedClass;
		UClass* PrimaryButtonClass = Variants[1].GeneratedClass;

		UWidgetBlueprint* Screen = ReplaceWidgetBlueprint(
			LogPrefix, ScreenPackage, FrontendFolder,
			*UFF_OptionsScreen::StaticClass(),
			[NavButtonClass, PrimaryButtonClass](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildOptionsScreenLayout(Blueprint, Tree, NavButtonClass, PrimaryButtonClass);
			});

		if (!Screen || !Screen->GeneratedClass)
		{
			return;
		}

		// 행 위젯 클래스를 CDO 에 박는다. 문자열 경로 대비책이 있지만 그것은 쿠커 참조 그래프에
		// 잡히지 않으므로, 실제 참조는 이 에셋이 들고 있어야 한다.
		if (UFF_OptionsScreen* ScreenCDO =
			Cast<UFF_OptionsScreen>(Screen->GeneratedClass->GetDefaultObject()))
		{
			ScreenCDO->ApplyGeneratedRowClasses(
				LoadGeneratedClass(ToggleRowPackage),
				LoadGeneratedClass(SliderRowPackage),
				LoadGeneratedClass(SelectRowPackage),
				LoadGeneratedClass(KeyBindingPanelPackage));

			if (!SaveAssetPackage(LogPrefix, *Screen))
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_FAIL: %s 의 행 클래스 기본값을 저장하지 못했다."), LogPrefix, *ScreenPackage);
				return;
			}
		}

		// 플레이 허브도 같은 버튼·색을 쓴다. 지구본 위에 왼쪽 패널만 얹는다.
		if (!ReplaceWidgetBlueprint(
			LogPrefix, PlayHubPackage, FrontendFolder,
			*UFF_PlayHubScreen::StaticClass(),
			[NavButtonClass, PrimaryButtonClass](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildPlayHubLayout(Blueprint, Tree, NavButtonClass, PrimaryButtonClass);
			}))
		{
			return;
		}

		const FString CardPackage=FrontendFolder/TEXT("WBP_FF_SaveSlotCard");
        UWidgetBlueprint* Card=ReplaceWidgetBlueprint(LogPrefix,CardPackage,FrontendFolder,*UFF_SaveSlotCard::StaticClass(),
            [](UWidgetBlueprint& BP,UWidgetTree& Tree){return BuildSaveCardCasual(BP,Tree);});
        if(!Card || !Card->GeneratedClass)return;
        UWidgetBlueprint* Saves=ReplaceWidgetBlueprint(LogPrefix,FrontendFolder/TEXT("WBP_FF_SaveList"),FrontendFolder,*UFF_SaveListScreen::StaticClass(),
            [NavButtonClass](UWidgetBlueprint& BP,UWidgetTree& Tree){return BuildSaveListCasual(BP,Tree,NavButtonClass);});
        if(!Saves || !Saves->GeneratedClass)return;
        UObject* Defaults=Saves->GeneratedClass->GetDefaultObject();Defaults->Modify();
        if(auto* Property=FindFProperty<FClassProperty>(Defaults->GetClass(),TEXT("CardWidgetClass")))
            Property->SetObjectPropertyValue_InContainer(Defaults,Card->GeneratedClass);
        SaveAssetPackage(LogPrefix,*Saves);
		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_DONE: 프론트엔드 캐주얼 스타일 적용 완료 "
				 "(CTS 2 / CBS 2 / 버튼 2 / 옵션 화면 / 플레이 허브). "
				 "치수는 각 화면의 SizeBox·슬롯 패딩에서 조절한다."), LogPrefix);
	}

	// 이름에 `Apply...Style` 을 박아, 비파괴 생성 명령과 파괴적 교체 명령을 구분한다.
	FAutoConsoleCommand ApplyFrontendSetupStyleCommand(
		TEXT("FF.ApplyFrontendSetupStyle"),
		TEXT("Rebuilds the Clean Casual frontend option assets (2 text styles, 2 button styles, "
			 "2 button WBPs, WBP_FF_Options). DESTRUCTIVE: existing layouts are replaced."),
		FConsoleCommandDelegate::CreateStatic(&ApplyFrontendSetupStyle));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
