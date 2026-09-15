// FF_TitleCleanCasualWbpBuilder.cpp

#include "CoreMinimal.h"

#if WITH_EDITOR

// 캐주얼(Clean Casual) 타이틀 화면 3종을 목업대로 다시 조립하는 에디터 전용 도구다.
// 콘솔 명령은 `FF.ApplyTitleCleanCasualDesign` 하나뿐이다.
//
// 🔴 **이 명령은 파괴적이다.** 아래 세 에셋의 위젯 트리와 스타일 값을 통째로 덮어쓴다.
//   /Game/FestaFiesta/UI/Styles/CBS_Title_MyCasual
//   /Game/FestaFiesta/UI/Title/GlassOrbit/WBP_TitleMenuButton_MyCasual
//   /Game/FestaFiesta/UI/Title/GlassOrbit/WBP_TitleMain_MyCasual
// 이름에 `Apply...Design` 을 박은 이유가 그것이다 (docs/memory/wbp-codegen-workflow.md 규칙 2).
// 디자이너에서 손으로 조정한 뒤에는 **다시 실행하지 않는다.** 그 시점부터 원본은 에셋 쪽이다.
//
// === 좌표의 출처 ===
// 목업 `docs/ui/mockups/title-main-menu-clean-casual-v2.png` (정확히 1920x1080) 를 픽셀로 재서
// 얻은 값이다. 버튼 내부 값은 아트 팩이 함께 내보낸 기계 판독 규격을 그대로 쓴다:
//   SourceArt/UI/Title/CleanCasualParts_v2/scalable-button-layout-reference.json
// 두 출처가 어긋나는 항목은 주석에 적어 두었다.
//
// === 왜 Surface 를 9-slice(Box) 로 쓰지 않는가 ===
// 아트 팩 README 는 `Draw As = Box` + Margin `0.064516/0.333333/0.064516/0.366667` 을 권한다.
// **Slate 에서는 그 조합이 동작하지 않는다.** `FSlateBrush::Margin` 은 브러시의 Image Size 가 아니라
// **텍스처 실해상도**에 곱해져 로컬 픽셀 크기가 된다
// (SlateCore/Private/Rendering/ElementBatcher.cpp — `LeftMarginX = TextureWidth * Margin.Left`).
// 텍스처가 1240x240 이므로 세로 고정 영역이 240*0.3333 + 240*0.3667 = 168 로 계산되어
// 버튼 높이 120 을 넘고, Slate 가 위·아래를 각각 60 으로 클램프해 **가운데 띠를 통째로 건너뛴다.**
// 2배로 그린 9-slice 를 1배 크기 위젯에 Box 로 붙이는 조합 자체가 성립하지 않는다.
//
// 그래서 `Draw As = Image` + Image Size 620x120 으로 균일 축소해 쓴다. 목업 비율과 정확히 같고
// 4K(배율 2)에서는 1:1 로 떨어진다. 대신 **폭만 늘리면 모서리가 함께 늘어난다** —
// 크기를 바꿀 때는 README 의 프리셋(496x96 / 620x120 / 744x144)처럼 비율을 유지한다.
// 진짜 9-slice 가 필요해지면 Surface 를 620x120 으로 다시 뽑아 임포트한 뒤 Box 로 바꾼다.
//
// === 이벤트 그래프 ===
// 복사본에 남아 있던 `OpenLevel(Test_Main)` 레거시 노드를 지운다. 지금 흐름은
// `Btn_Play → UFF_TitleScreen::OnPlayRequested → UFF_UIFlowSubsystem → PlayHub` 이고,
// 노드가 남아 있으면 두 경로가 동시에 돌아 레벨이 통째로 바뀐다.

#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SafeZone.h"
#include "Components/SafeZoneSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Blueprint.h"
#include "Engine/Texture2D.h"
#include "HAL/IConsoleManager.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Styling/SlateBrush.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/FF_CommonButtonBase.h"
#include "UI/Title/FF_TitleScreen.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

namespace FFTitleCleanCasualWbpBuilder
{
	using namespace FFUIWbpBuilder;

	const TCHAR* const LogPrefix = TEXT("FF_TITLE_CASUAL_UMG");

	// === 에셋 경로 ===
	// 폴더 이름이 `GlassOrbit` 인 것은 사용자가 기존 타이틀을 그 자리에서 복사했기 때문이다.
	// 옮기면 이미 잡혀 있는 참조가 끊기므로 그대로 둔다. 테마 이름은 에셋 이름(`_MyCasual`)이 갖는다.
	const FString TitleFolder(TEXT("/Game/FestaFiesta/UI/Title/GlassOrbit"));
	const FString StyleFolder(TEXT("/Game/FestaFiesta/UI/Styles"));

	const FString MainPackage(TitleFolder / TEXT("WBP_TitleMain_MyCasual"));
	const FString ButtonPackage(TitleFolder / TEXT("WBP_TitleMenuButton_MyCasual"));
	const FString StylePackage(StyleFolder / TEXT("CBS_Title_MyCasual"));

	const FString ArtFolder(TEXT("/Game/FestaFiesta/UI/Title/CleanCasual"));
	const FString BrandLogoTexture(ArtFolder / TEXT("Brand/T_FF_Title_CasualBright_Logo_512"));
	const FString BrandWordmarkTexture(ArtFolder / TEXT("Brand/T_FF_Title_CasualBright_Wordmark_1024x512"));
	const FString SurfaceNormalTexture(ArtFolder / TEXT("Buttons/T_FF_Title_CleanCasual_Surface_Normal_1240x240_9Slice"));
	const FString SurfaceFocusedTexture(ArtFolder / TEXT("Buttons/T_FF_Title_CleanCasual_Surface_Focused_1240x240_9Slice"));
	const FString SurfacePressedTexture(ArtFolder / TEXT("Buttons/T_FF_Title_CleanCasual_Surface_Pressed_1240x240_9Slice"));
	const FString SurfaceDisabledTexture(ArtFolder / TEXT("Buttons/T_FF_Title_CleanCasual_Surface_Disabled_1240x240_9Slice"));
	const FString IconPlateBaseTexture(ArtFolder / TEXT("Buttons/T_FF_Title_CleanCasual_IconPlate_Base_256"));
	const FString IconPlateFocusTexture(ArtFolder / TEXT("Buttons/T_FF_Title_CleanCasual_IconPlate_FocusOverlay_256"));
	const FString IconPlayTexture(ArtFolder / TEXT("Icons/T_FF_Title_CleanCasual_Icon_Play_White_256"));
	const FString IconSettingsTexture(ArtFolder / TEXT("Icons/T_FF_Title_CleanCasual_Icon_Settings_White_256"));
	const FString IconPowerTexture(ArtFolder / TEXT("Icons/T_FF_Title_CleanCasual_Icon_Power_White_256"));
	const FString IconChevronTexture(ArtFolder / TEXT("Icons/T_FF_Title_CleanCasual_Icon_Chevron_White_256"));

	// =========================================================================
	// 색 — 목업에서 스포이드로 뽑은 값이다
	// =========================================================================
	// 이 값들을 `UFF_UIPalette` 로 올리지 않았다. 그 구조체(`Frontend`)는 Glass Orbit 의 어두운
	// 정체성을 담고 있고, 두 테마가 한 구조체를 나눠 쓰면 한쪽을 고칠 때 다른 쪽이 깨진다.
	// 캐주얼 테마가 타이틀 밖으로 퍼지면 그때 팔레트에 `TitleCasual` 을 새로 만든다.

	/** `#123E34`. 워드마크·라벨·아이콘·Chevron 이 모두 같은 잉크색이다. */
	const FLinearColor InkColor = FLinearColor::FromSRGBColor(FColor(0x12, 0x3E, 0x34));

	/** `#839087`. 비활성. 색만으로 구분하지 않으므로 불투명도 0.35 와 함께 쓴다 (AT-011). */
	const FLinearColor DisabledInkColor = FLinearColor::FromSRGBColor(FColor(0x83, 0x90, 0x87));

	/**
	 * 왼쪽 패널 면. 목업의 패널 안쪽을 스포이드로 찍으면 `#FEFBF3` — 사실상 **따뜻한 흰색**이다.
	 *
	 * 알파 0.82 인 이유. 순수 흰색 60% 로 두면 지금처럼 **어두운 우주 배경 위에서 회색 판**이 된다
	 * (2026-09-04 실기 확인). 목업의 밝은 하늘 배경이 아직 없기 때문이다.
	 * 지구 씬을 캐주얼(밝은 배경)로 바꾸면 0.60~0.70 으로 내려도 목업처럼 보인다.
	 * 에디터에서 `Border_LeftPanel` 의 Brush Tint 알파 하나만 바꾸면 된다.
	 */
	const FLinearColor PanelFillColor = FLinearColor::FromSRGBColor(FColor(0xFE, 0xFB, 0xF3, 209));

	/** `#D7EFE4`. 패널 외곽선. */
	const FLinearColor PanelOutlineColor = FLinearColor::FromSRGBColor(FColor(0xD7, 0xEF, 0xE4));

	/** 패널 그림자. 잉크색 7%. */
	const FLinearColor PanelShadowColor = FLinearColor(InkColor.R, InkColor.G, InkColor.B, 0.07f);

	// =========================================================================
	// 치수 — 1920x1080 기준. 전부 에디터에서 다시 만질 수 있는 값이다
	// =========================================================================
	namespace Layout
	{
		// --- 버튼 한 개 (scalable-button-layout-reference.json) ---
		constexpr float ButtonWidth = 620.f;
		constexpr float ButtonHeight = 120.f;

		/**
		 * 내용 여백. 아래 두 값이 플레이트·아이콘·Chevron 의 중심을 버튼 표면 중심인 Y 56 에 맞춘다.
		 * 표면이 세로 가운데가 아닌 이유는 텍스처 아래쪽에 그림자가 있기 때문이다.
		 */
		constexpr float ContentPadLeft = 18.f;
		constexpr float ContentPadRight = 13.f;
		constexpr float ContentPadBottom = 8.f;

		constexpr float IconPlateSize = 88.f;   // 실제 보이는 원은 지름 80. 텍스처 여백까지 포함한 값이다
		constexpr float IconSize = 48.f;
		constexpr float ChevronSize = 48.f;

		/** 18 + 88 + 48 = 154 → 규격의 `labelStartX` 와 같다. */
		constexpr float IconToLabelGap = 48.f;
		constexpr int32 LabelFontSize = 34;

		// --- 화면 배치 (목업 실측) ---
		constexpr float PanelX = 88.f;
		constexpr float PanelY = 44.f;
		constexpr float PanelWidth = 692.f;
		constexpr float PanelHeight = 936.f;
		constexpr float PanelCornerRadius = 44.f;
		constexpr float PanelOutlineWidth = 2.f;

		/** 그림자는 패널을 오른쪽 아래로 조금 민 것이다. */
		constexpr float PanelShadowOffsetX = 4.f;
		constexpr float PanelShadowOffsetY = 8.f;

		/** 로고 PNG 는 90% 가 투명 여백이라 위젯 220 에서 실제 꽃 문양은 약 140 으로 보인다. */
		constexpr float BrandMarkX = 320.f;
		constexpr float BrandMarkY = 48.f;
		constexpr float BrandMarkSize = 220.f;

		/** 워드마크 원본이 정확히 2:1 이므로 752x376 에서 비율이 유지된다. */
		constexpr float WordmarkX = 66.f;
		constexpr float WordmarkY = 182.f;
		constexpr float WordmarkWidth = 752.f;
		constexpr float WordmarkHeight = 376.f;

		/**
		 * 메뉴. 목업의 버튼 간격은 8 이다 (표면 위쪽 간격 128 - 버튼 높이 120).
		 * 아트 팩 v1 의 `layout-reference.json` 은 14 라고 적었지만 그것은 이전 목업 기준이다.
		 */
		constexpr float MenuX = 130.f;
		constexpr float MenuY = 564.f;
		constexpr float MenuButtonGap = 8.f;
		constexpr float MenuHeight = ButtonHeight * 3.f + MenuButtonGap * 2.f;
	}

	// =========================================================================
	// 작은 헬퍼
	// =========================================================================

	/** 없으면 경고만 남기고 nullptr 을 돌려준다. 아이콘 하나 때문에 화면 전체를 못 만들면 안 된다. */
	UTexture2D* LoadTexture(const FString& PackageName)
	{
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *MakeObjectPath(PackageName));
		if (!Texture)
		{
			UE_LOG(LogFFUIWbpBuilder, Warning,
				TEXT("%s_WARN: 텍스처를 찾지 못했다: %s. 브러시를 비워 둔다."), LogPrefix, *PackageName);
		}
		return Texture;
	}

	/** 정사각 아이콘·플레이트용. 표시 크기를 박지 않으면 텍스처 원본 크기(256)로 부풀어 줄이 무너진다. */
	FSlateBrush MakeIconBrush(UTexture2D* Texture, const float Size)
	{
		FSlateBrush Brush;
		Brush.DrawAs = Texture ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
		Brush.SetResourceObject(Texture);
		Brush.SetImageSize(FVector2D(Size, Size));
		Brush.TintColor = FSlateColor(FLinearColor::White);
		return Brush;
	}

	/** 버튼 표면. `Box` 가 아니라 `Image` 인 이유는 파일 상단 주석에 있다. */
	FSlateBrush MakeSurfaceBrush(UTexture2D* Texture)
	{
		FSlateBrush Brush;
		Brush.DrawAs = Texture ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
		Brush.SetResourceObject(Texture);
		Brush.SetImageSize(FVector2D(Layout::ButtonWidth, Layout::ButtonHeight));
		Brush.TintColor = FSlateColor(FLinearColor::White);
		return Brush;
	}

	FSlateBrush MakeRoundedPanelBrush(const FLinearColor& Fill, const FLinearColor& Outline, const float OutlineWidth)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(
			Layout::PanelCornerRadius, FSlateColor(Outline), OutlineWidth);
		return Brush;
	}

	/** 장식 위젯. 입력을 먹으면 버튼이 안 눌린다. */
	void MakeDecorative(UWidget& Widget)
	{
		Widget.SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	// =========================================================================
	// 1. CBS_Title_MyCasual — 버튼 배경 상태 4종
	// =========================================================================
	//
	// 상태 매핑. `Focused` 표면을 Selected 쪽에 두는 이유 —
	// CommonButton 의 Hovered 는 **마우스에만** 반응한다. 게임패드 포커스로는 바뀌지 않는다.
	// 그래서 포커스 표현은 스타일이 아니라 WBP 의 `Image_FocusSurface` 가 맡고,
	// 스타일의 Selected 칸은 나중에 "지속 선택" 이 필요해질 때를 위해 같은 표면으로 채워 둔다.
	bool ApplyButtonStyle(TSubclassOf<UCommonButtonStyle>& OutStyleClass)
	{
		UBlueprint* StyleBlueprint = LoadObject<UBlueprint>(nullptr, *MakeObjectPath(StylePackage));
		if (!StyleBlueprint || !StyleBlueprint->GeneratedClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: %s 를 로드할 수 없다. 버튼 스타일을 적용하지 못했다."), LogPrefix, *StylePackage);
			return false;
		}

		UCommonButtonStyle* Style = Cast<UCommonButtonStyle>(StyleBlueprint->GeneratedClass->GetDefaultObject());
		if (!Style)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: %s 의 부모가 UCommonButtonStyle 이 아니다. 손대지 않았다."), LogPrefix, *StylePackage);
			return false;
		}

		const FSlateBrush NormalBrush = MakeSurfaceBrush(LoadTexture(SurfaceNormalTexture));
		const FSlateBrush FocusedBrush = MakeSurfaceBrush(LoadTexture(SurfaceFocusedTexture));
		const FSlateBrush PressedBrush = MakeSurfaceBrush(LoadTexture(SurfacePressedTexture));
		const FSlateBrush DisabledBrush = MakeSurfaceBrush(LoadTexture(SurfaceDisabledTexture));

		Style->Modify();
		Style->bSingleMaterial = false;

		Style->NormalBase = NormalBrush;
		// 마우스 Hover 에서 표면을 바꾸지 않는다. Hover 는 C++ 이 포커스로 바꿔 주고
		// 포커스 표현은 `Image_FocusSurface` 하나가 맡는다 — 마우스와 패드의 그림을 같게 하기 위함이다.
		Style->NormalHovered = NormalBrush;
		Style->NormalPressed = PressedBrush;
		Style->SelectedBase = FocusedBrush;
		Style->SelectedHovered = FocusedBrush;
		Style->SelectedPressed = PressedBrush;
		Style->Disabled = DisabledBrush;

		// 최소 크기는 스타일이 갖는다. 버튼 인스턴스의 SizeBox_Root 와 같은 값이어야
		// 표면과 내용이 어긋나지 않는다.
		Style->MinWidth = static_cast<int32>(Layout::ButtonWidth);
		Style->MinHeight = static_cast<int32>(Layout::ButtonHeight);
		Style->MaxWidth = 0;
		Style->MaxHeight = 0;

		// 내용 여백은 WBP 의 HorizontalBox 가 갖는다. 여기서 또 주면 두 번 밀린다.
		Style->ButtonPadding = FMargin(0.f);
		Style->CustomPadding = FMargin(0.f);

		if (!SaveAssetPackage(LogPrefix, *StyleBlueprint))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: %s 저장 실패."), LogPrefix, *StylePackage);
			return false;
		}

		OutStyleClass = StyleBlueprint->GeneratedClass;
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_STYLE: %s 적용 완료."), LogPrefix, *StylePackage);
		return true;
	}

	// =========================================================================
	// 2. WBP_TitleMenuButton_MyCasual — 메뉴 버튼 한 개
	// =========================================================================
	//
	// SizeBox_Root 620x120
	// └ Overlay_Root
	//   ├ Image_FocusSurface      ← 포커스 시 표면 전체 (뒤에 깔린다)
	//   └ HorizontalBox_Content   ← 18 / 0 / 13 / 8
	//     ├ SizeBox_IconPlate 88x88
	//     │ └ Overlay_Icon
	//     │   ├ Image_IconPlateBase
	//     │   ├ Image_FocusRing   ← 원 안쪽 음영만. 원의 위치·크기는 변하지 않는다
	//     │   └ SizeBox_Icon 48x48 → Image_Icon
	//     ├ Spacer_IconToLabel 48
	//     ├ Text_Label            ← UCommonTextBlock 이어야 BindWidget 이 붙는다
	//     └ SizeBox_Chevron 48x48 → Image_Chevron
	bool BuildMenuButtonLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		USizeBox* Root = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Root"), true);
		UOverlay* Overlay = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Root"));
		UImage* FocusSurface = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_FocusSurface"), true);
		UHorizontalBox* Row = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Content"));
		USizeBox* PlateBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_IconPlate"));
		UOverlay* PlateOverlay = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Icon"));
		UImage* PlateBase = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_IconPlateBase"));
		UImage* FocusRing = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_FocusRing"), true);
		USizeBox* IconBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Icon"));
		UImage* Icon = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Icon"), true);
		USpacer* Gap = MakeWidget<USpacer>(Blueprint, Tree, TEXT("Spacer_IconToLabel"));
		UCommonTextBlock* Label = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_Label"), true);
		USizeBox* ChevronBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Chevron"));
		UImage* Chevron = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Chevron"), true);

		if (!Root || !Overlay || !FocusSurface || !Row || !PlateBox || !PlateOverlay || !PlateBase ||
			!FocusRing || !IconBox || !Icon || !Gap || !Label || !ChevronBox || !Chevron)
		{
			return false;
		}

		Tree.RootWidget = Root;

		Root->SetWidthOverride(Layout::ButtonWidth);
		Root->SetHeightOverride(Layout::ButtonHeight);
		Root->AddChild(Overlay);

		// --- 포커스 표면. Overlay 의 첫 자식이라 내용 뒤에 깔린다 ---
		FocusSurface->SetBrush(MakeSurfaceBrush(LoadTexture(SurfaceFocusedTexture)));
		// Collapsed 가 아니라 Hidden 이다. 겹쳐 그리는 장식이라 자리를 차지하지 않는다.
		FocusSurface->SetVisibility(ESlateVisibility::Hidden);
		if (UOverlaySlot* Slot = Overlay->AddChildToOverlay(FocusSurface))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}

		if (UOverlaySlot* RowSlot = Overlay->AddChildToOverlay(Row))
		{
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
			RowSlot->SetVerticalAlignment(VAlign_Fill);
			RowSlot->SetPadding(FMargin(
				Layout::ContentPadLeft, 0.f, Layout::ContentPadRight, Layout::ContentPadBottom));
		}

		// --- 아이콘 원판 ---
		PlateBox->SetWidthOverride(Layout::IconPlateSize);
		PlateBox->SetHeightOverride(Layout::IconPlateSize);
		PlateBox->AddChild(PlateOverlay);
		if (UHorizontalBoxSlot* PlateSlot = Row->AddChildToHorizontalBox(PlateBox))
		{
			PlateSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			PlateSlot->SetVerticalAlignment(VAlign_Center);
		}

		PlateBase->SetBrush(MakeIconBrush(LoadTexture(IconPlateBaseTexture), Layout::IconPlateSize));
		MakeDecorative(*PlateBase);
		if (UOverlaySlot* Slot = PlateOverlay->AddChildToOverlay(PlateBase))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}

		// 포커스 음영. 원판 위, 아이콘 아래다. 순서를 바꾸면 아이콘이 음영에 덮인다.
		FocusRing->SetBrush(MakeIconBrush(LoadTexture(IconPlateFocusTexture), Layout::IconPlateSize));
		FocusRing->SetVisibility(ESlateVisibility::Hidden);
		if (UOverlaySlot* Slot = PlateOverlay->AddChildToOverlay(FocusRing))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}

		IconBox->SetWidthOverride(Layout::IconSize);
		IconBox->SetHeightOverride(Layout::IconSize);
		// 기본 브러시는 Play 다. 실제 아이콘은 화면 쪽 인스턴스의 `Icon Brush` 가 덮어쓴다.
		Icon->SetBrush(MakeIconBrush(LoadTexture(IconPlayTexture), Layout::IconSize));
		Icon->SetColorAndOpacity(InkColor);
		MakeDecorative(*Icon);
		IconBox->AddChild(Icon);
		if (UOverlaySlot* Slot = PlateOverlay->AddChildToOverlay(IconBox))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		// --- 아이콘과 글자 사이 ---
		Gap->SetSize(FVector2D(Layout::IconToLabelGap, 1.f));
		if (UHorizontalBoxSlot* GapSlot = Row->AddChildToHorizontalBox(Gap))
		{
			GapSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}

		// --- 라벨 ---
		// 문구는 화면 쪽 인스턴스의 `Button Label` 이 넣는다. 여기 값은 디자이너용 자리표시자다.
		Label->SetText(LOCTEXT("CasualMenuButtonPlaceholder", "메뉴 항목"));
		Label->SetJustification(ETextJustify::Left);
		Label->SetAutoWrapText(false);
		StyleText(*Label, Layout::LabelFontSize, InkColor, TEXT("SemiBold"));
		if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label))
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}

		// --- Chevron ---
		ChevronBox->SetWidthOverride(Layout::ChevronSize);
		ChevronBox->SetHeightOverride(Layout::ChevronSize);
		Chevron->SetBrush(MakeIconBrush(LoadTexture(IconChevronTexture), Layout::ChevronSize));
		Chevron->SetColorAndOpacity(InkColor);
		MakeDecorative(*Chevron);
		ChevronBox->AddChild(Chevron);
		if (UHorizontalBoxSlot* ChevronSlot = Row->AddChildToHorizontalBox(ChevronBox))
		{
			ChevronSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			ChevronSlot->SetVerticalAlignment(VAlign_Center);
		}

		return true;
	}

	// =========================================================================
	// 3. WBP_TitleMain_MyCasual — 타이틀 화면
	// =========================================================================
	//
	// Overlay_Root
	// └ SafeZone
	//   └ CanvasPanel_Content        ← 1920x1080 좌표계. 여기서 끌어 옮기면 된다
	//     ├ Border_LeftPanelShadow
	//     ├ Border_LeftPanel
	//     ├ Image_BrandMark
	//     ├ Image_Wordmark
	//     └ VerticalBox_Menu → Btn_Play / Btn_Options / Btn_Quit
	//
	// 전체 화면 배경 이미지를 넣지 않는다. 배경은 월드의 3D 지구본이고, 여기에 불투명한
	// 이미지를 깔면 그것이 통째로 가려진다 (docs/ui/visual-direction.md 4절).
	bool BuildTitleScreenLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, UClass* MenuButtonClass)
	{
		if (!MenuButtonClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 메뉴 버튼 클래스가 없어 타이틀 화면을 만들지 않았다."), LogPrefix);
			return false;
		}

		UOverlay* Root = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Root"));
		USafeZone* SafeZone = MakeWidget<USafeZone>(Blueprint, Tree, TEXT("SafeZone"));
		UCanvasPanel* Canvas = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Content"));
		UBorder* PanelShadow = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_LeftPanelShadow"));
		UBorder* Panel = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_LeftPanel"));
		UImage* BrandMark = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_BrandMark"));
		UImage* Wordmark = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Wordmark"));
		UVerticalBox* Menu = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Menu"));

		// 이름이 `UFF_TitleScreen` 의 BindWidget 과 정확히 같아야 한다.
		// 초기 포커스도 이름으로 `Btn_Play` 를 찾는다.
		UUserWidget* PlayButton = MakeWidgetOfClass(Blueprint, Tree, TEXT("Btn_Play"), MenuButtonClass);
		UUserWidget* OptionsButton = MakeWidgetOfClass(Blueprint, Tree, TEXT("Btn_Options"), MenuButtonClass);
		UUserWidget* QuitButton = MakeWidgetOfClass(Blueprint, Tree, TEXT("Btn_Quit"), MenuButtonClass);

		if (!Root || !SafeZone || !Canvas || !PanelShadow || !Panel || !BrandMark || !Wordmark || !Menu ||
			!PlayButton || !OptionsButton || !QuitButton)
		{
			return false;
		}

		Tree.RootWidget = Root;

		if (UOverlaySlot* SafeZoneSlot = Root->AddChildToOverlay(SafeZone))
		{
			SafeZoneSlot->SetHorizontalAlignment(HAlign_Fill);
			SafeZoneSlot->SetVerticalAlignment(VAlign_Fill);
		}

		if (USafeZoneSlot* CanvasSlot = Cast<USafeZoneSlot>(SafeZone->AddChild(Canvas)))
		{
			CanvasSlot->SetHorizontalAlignment(HAlign_Fill);
			CanvasSlot->SetVerticalAlignment(VAlign_Fill);
			CanvasSlot->SetPadding(FMargin(0.f));
		}

		// --- 왼쪽 패널 ---
		// 목업의 배경은 밝은 하늘 그라디언트지만 실기 배경은 **도는 지구본**이다.
		// 반투명 패널이 없으면 짙은 초록 글자가 지구 위에서 읽히지 않는다.
		// 필요 없어지면 두 Border 의 Visibility 를 Collapsed 로 바꾸면 된다 — 다른 배치는 안 바뀐다.
		PanelShadow->SetBrush(MakeRoundedPanelBrush(PanelShadowColor, FLinearColor::Transparent, 0.f));
		MakeDecorative(*PanelShadow);
		PlaceOnCanvas(*Canvas, *PanelShadow,
			Layout::PanelX + Layout::PanelShadowOffsetX,
			Layout::PanelY + Layout::PanelShadowOffsetY,
			Layout::PanelWidth, Layout::PanelHeight);

		Panel->SetBrush(MakeRoundedPanelBrush(PanelFillColor, PanelOutlineColor, Layout::PanelOutlineWidth));
		MakeDecorative(*Panel);
		PlaceOnCanvas(*Canvas, *Panel,
			Layout::PanelX, Layout::PanelY, Layout::PanelWidth, Layout::PanelHeight);

		// --- 브랜드 ---
		BrandMark->SetBrush(MakeIconBrush(LoadTexture(BrandLogoTexture), Layout::BrandMarkSize));
		MakeDecorative(*BrandMark);
		PlaceOnCanvas(*Canvas, *BrandMark,
			Layout::BrandMarkX, Layout::BrandMarkY, Layout::BrandMarkSize, Layout::BrandMarkSize);

		// `FESTA FIESTA` 는 번역 대상이 아닌 브랜드라 텍스트가 아니라 이미지다.
		// 반대로 버튼 문구는 절대 이미지로 굽지 않는다 — 현지화가 막힌다.
		{
			FSlateBrush WordmarkBrush;
			UTexture2D* WordmarkTexture = LoadTexture(BrandWordmarkTexture);
			WordmarkBrush.DrawAs = WordmarkTexture ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
			WordmarkBrush.SetResourceObject(WordmarkTexture);
			WordmarkBrush.SetImageSize(FVector2D(Layout::WordmarkWidth, Layout::WordmarkHeight));
			WordmarkBrush.TintColor = FSlateColor(FLinearColor::White);
			Wordmark->SetBrush(WordmarkBrush);
		}
		MakeDecorative(*Wordmark);
		PlaceOnCanvas(*Canvas, *Wordmark,
			Layout::WordmarkX, Layout::WordmarkY, Layout::WordmarkWidth, Layout::WordmarkHeight);

		// --- 메뉴 ---
		PlaceOnCanvas(*Canvas, *Menu,
			Layout::MenuX, Layout::MenuY, Layout::ButtonWidth, Layout::MenuHeight);

		const TArray<UUserWidget*> Buttons = { PlayButton, OptionsButton, QuitButton };
		for (int32 Index = 0; Index < Buttons.Num(); ++Index)
		{
			const bool bIsLast = (Index == Buttons.Num() - 1);
			if (UVerticalBoxSlot* Slot = Menu->AddChildToVerticalBox(Buttons[Index]))
			{
				Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				Slot->SetHorizontalAlignment(HAlign_Fill);
				Slot->SetVerticalAlignment(VAlign_Top);
				Slot->SetPadding(FMargin(0.f, 0.f, 0.f, bIsLast ? 0.f : Layout::MenuButtonGap));
			}
		}

		// --- 버튼별 내용 ---
		// 부모 화면에서 자식 WBP 내부를 편집할 수 없으므로, 문구·아이콘·크기는
		// `UFF_CommonButtonBase` 의 인스턴스 프로퍼티로 넣는다. 에디터 Details 에서 바꿀 수 있다.
		struct FButtonContent
		{
			UUserWidget* Widget;
			FText Label;
			FString IconPackage;
		};

		const TArray<FButtonContent> Contents = {
			{ PlayButton,    LOCTEXT("CasualTitlePlay", "플레이"),     IconPlayTexture },
			{ OptionsButton, LOCTEXT("CasualTitleOptions", "옵션"),    IconSettingsTexture },
			{ QuitButton,    LOCTEXT("CasualTitleQuit", "게임 종료"),  IconPowerTexture },
		};

		for (const FButtonContent& Content : Contents)
		{
			UFF_CommonButtonBase* Button = Cast<UFF_CommonButtonBase>(Content.Widget);
			if (!Button)
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_FAIL: %s 가 UFF_CommonButtonBase 가 아니다."),
					LogPrefix, *Content.Widget->GetName());
				return false;
			}

			Button->ApplyGeneratedButtonContent(
				Content.Label,
				MakeIconBrush(LoadTexture(Content.IconPackage), Layout::IconSize),
				FVector2D(Layout::ButtonWidth, Layout::ButtonHeight));
		}

		return true;
	}

	// =========================================================================
	// 레거시 그래프 노드 정리
	// =========================================================================

	/** 복사본에 남은 `OpenLevel(Test_Main)` 을 지운다. 지운 개수를 돌려준다. */
	int32 RemoveLegacyOpenLevelNodes(UWidgetBlueprint& Blueprint)
	{
		TArray<UEdGraphNode*> Doomed;
		for (UEdGraph* Graph : Blueprint.UbergraphPages)
		{
			if (!Graph)
			{
				continue;
			}

			for (UEdGraphNode* Node : Graph->Nodes)
			{
				const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
				if (Call && Call->FunctionReference.GetMemberName().ToString().StartsWith(TEXT("OpenLevel")))
				{
					Doomed.Add(Node);
				}
			}
		}

		for (UEdGraphNode* Node : Doomed)
		{
			// bDontRecompile = true. 어차피 바로 뒤에서 트리를 갈아 끼우고 한 번에 컴파일한다.
			FBlueprintEditorUtils::RemoveNode(&Blueprint, Node, true);
		}

		return Doomed.Num();
	}

	// =========================================================================
	// 콘솔 명령
	// =========================================================================
	void ApplyTitleCleanCasualDesign()
	{
		TSubclassOf<UCommonButtonStyle> StyleClass;
		if (!ApplyButtonStyle(StyleClass))
		{
			return;
		}

		UWidgetBlueprint* MenuButton = ReplaceWidgetBlueprint(
			LogPrefix,
			ButtonPackage,
			TitleFolder,
			*UFF_CommonButtonBase::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildMenuButtonLayout(Blueprint, Tree);
			});

		if (!MenuButton || !MenuButton->GeneratedClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 메뉴 버튼 WBP 를 만들지 못해 타이틀 화면을 건드리지 않았다."), LogPrefix);
			return;
		}

		// CDO 기본값은 **컴파일 뒤에** 박는다. 컴파일이 CDO 를 다시 만들기 때문이다.
		if (UFF_CommonButtonBase* ButtonCDO =
			Cast<UFF_CommonButtonBase>(MenuButton->GeneratedClass->GetDefaultObject()))
		{
			ButtonCDO->ApplyGeneratedButtonDefaults(
				StyleClass,
				InkColor,       // Normal — 흰 표면 위의 짙은 초록
				InkColor,       // Highlighted — 같은 색. 표면이 연녹색으로 바뀌어도 잘 읽힌다
				DisabledInkColor,
				/*bTintLabel=*/ true,
				/*bFocusOnHover=*/ true,
				/*bShowChevronOnlyWhenHighlighted=*/ false);

			if (!SaveAssetPackage(LogPrefix, *MenuButton))
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_FAIL: %s 의 기본값을 저장하지 못했다."), LogPrefix, *ButtonPackage);
				return;
			}
		}

		UClass* MenuButtonClass = MenuButton->GeneratedClass;

		// 트리를 갈아 끼우기 **전에** 그래프를 정리한다. 뒤에 오는 컴파일에 함께 반영된다.
		if (UWidgetBlueprint* ExistingMain = LoadExistingBlueprint(MainPackage))
		{
			const int32 RemovedCount = RemoveLegacyOpenLevelNodes(*ExistingMain);
			if (RemovedCount > 0)
			{
				UE_LOG(LogFFUIWbpBuilder, Display,
					TEXT("%s_GRAPH: %s 에서 레거시 OpenLevel 노드 %d개를 지웠다."),
					LogPrefix, *MainPackage, RemovedCount);
			}
		}

		UWidgetBlueprint* Main = ReplaceWidgetBlueprint(
			LogPrefix,
			MainPackage,
			TitleFolder,
			*UFF_TitleScreen::StaticClass(),
			[MenuButtonClass](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildTitleScreenLayout(Blueprint, Tree, MenuButtonClass);
			});

		if (!Main)
		{
			return;
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_DONE: 캐주얼 타이틀 3종 적용 완료. "
				 "이제 에디터에서 CanvasPanel_Content 의 위젯을 끌어 옮겨 마감한다."), LogPrefix);
	}

	// 이름에 `Apply...Design` 을 박아, 비파괴 생성 명령과 파괴적 교체 명령을 구분한다.
	FAutoConsoleCommand ApplyTitleCleanCasualDesignCommand(
		TEXT("FF.ApplyTitleCleanCasualDesign"),
		TEXT("Rebuilds CBS_Title_MyCasual, WBP_TitleMenuButton_MyCasual and WBP_TitleMain_MyCasual "
			 "to the clean-casual mockup. DESTRUCTIVE: existing layouts in those three assets are replaced."),
		FConsoleCommandDelegate::CreateStatic(&ApplyTitleCleanCasualDesign));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
