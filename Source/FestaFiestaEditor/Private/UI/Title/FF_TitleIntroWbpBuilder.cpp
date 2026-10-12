// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// FF_TitleIntroWbpBuilder.cpp

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Title/FF_TitleIntroScreen.h"

// 런타임 화면과 같은 네임스페이스를 쓴다. 문구는 WBP 에 저장되고 현지화 수집이 이 키를 본다.
#define LOCTEXT_NAMESPACE "FestaFiestaUI"

/*
 * 인트로 화면 WBP 골격 (2026-10-02 v2: 검은 시작 화면 · 글자 없는 지구 공개).
 *
 * Overlay_Root
 *  ├ Image_Backdrop            검은 바탕 (시작 화면 ↔ 우주 영상 이음)
 *  ├ ScaleBox_Video            ScaleToFill — 화면비가 달라도 빈틈 없이 덮는다
 *  │   └ Image_Video           브러시는 실행 중 MediaTexture 로 바뀐다
 *  ├ Panel_Splash              큰 꽃 로고 · 팀 이름 · 과정명 / 맨 아래 안내 문구
 *  └ Image_FlashCover          흰 덮개 (영상의 마지막 흰 빛을 이어받는다)
 *
 * FF.TitleIntroUMG          없을 때만 만든다(비파괴).
 * FF.ApplyTitleIntroDesign  🔴 있는 WBP 의 트리를 이 골격으로 통째로 바꾼다(파괴). 디자이너 수정이 사라진다.
 */
namespace FFTitleIntroWbpBuilder
{
	using namespace FFUIWbpBuilder;

	constexpr const TCHAR* LogPrefix = TEXT("FF_TITLE_INTRO_UMG");
	const FString Folder = TEXT("/Game/FestaFiesta/UI/Title/Intro");
	const FString Package = Folder / TEXT("WBP_FF_TitleIntro");
	/** 검은 바탕용 밝은 꽃 로고. 원본 `Tools/ui/title_intro/make_logo_on_dark.py`. */
	const FString MarkTexture = Folder / TEXT("T_FF_Title_Logo_OnDark");

	const FLinearColor TextMain = FLinearColor::FromSRGBColor(FColor(0xF6, 0xF1, 0xE4));
	const FLinearColor TextSub = FLinearColor::FromSRGBColor(FColor(0xB4, 0xB8, 0xB2));
	const FLinearColor TextNote = FLinearColor::FromSRGBColor(FColor(0x8A, 0x8F, 0x8A));

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

	FSlateBrush ImageBrush(UTexture2D* Texture, const FVector2D Size)
	{
		FSlateBrush Brush;
		Brush.DrawAs = Texture ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
		Brush.SetResourceObject(Texture);
		Brush.SetImageSize(Size);
		Brush.TintColor = FSlateColor(FLinearColor::White);
		return Brush;
	}

	/** 단색 판. 리소스 없이 Image 로 그리면 틴트 색 사각형이 된다. */
	void MakeFlat(UImage& Image, const FLinearColor& Color)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.SetImageSize(FVector2D(32.f, 32.f));
		Brush.TintColor = FSlateColor(Color);
		Image.SetBrush(Brush);
	}

	void Fill(UOverlay& Parent, UWidget& Child)
	{
		if (UOverlaySlot* Slot = Parent.AddChildToOverlay(&Child))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	void Place(UOverlay& Parent, UWidget& Child, const EHorizontalAlignment H, const EVerticalAlignment V, const FMargin& Padding)
	{
		if (UOverlaySlot* Slot = Parent.AddChildToOverlay(&Child))
		{
			Slot->SetHorizontalAlignment(H);
			Slot->SetVerticalAlignment(V);
			Slot->SetPadding(Padding);
		}
	}

	void Stack(UVerticalBox& Parent, UWidget& Child, const FMargin& Padding)
	{
		if (UVerticalBoxSlot* Slot = Parent.AddChildToVerticalBox(&Child))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetPadding(Padding);
		}
	}

	bool Build(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		UOverlay* Root = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Root"));
		UImage* Backdrop = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Backdrop"), true);
		UScaleBox* VideoBox = MakeWidget<UScaleBox>(Blueprint, Tree, TEXT("ScaleBox_Video"));
		UImage* Video = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Video"), true);
		UOverlay* Splash = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Panel_Splash"), true);
		UVerticalBox* SplashCenter = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_SplashCenter"));
		UImage* SplashMark = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_SplashMark"));
		UTextBlock* TeamName = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_TeamName"), true);
		UTextBlock* TeamCourse = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_TeamCourse"), true);
		UTextBlock* Disclaimer = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Disclaimer"), true);
		UImage* FlashCover = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_FlashCover"), true);

		if (!Root || !Backdrop || !VideoBox || !Video || !Splash || !SplashCenter || !SplashMark
			|| !TeamName || !TeamCourse || !Disclaimer || !FlashCover)
		{
			return false;
		}

		Tree.RootWidget = Root;
		// 클릭은 화면(UserWidget) 자신이 받는다. 아이들은 전부 입력을 통과시킨다.
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		// --- 검은 바탕 ---
		MakeFlat(*Backdrop, FLinearColor::Black);
		Backdrop->SetVisibility(ESlateVisibility::HitTestInvisible);
		Fill(*Root, *Backdrop);

		// --- 영상 ---
		VideoBox->SetStretch(EStretch::ScaleToFill);
		VideoBox->SetVisibility(ESlateVisibility::HitTestInvisible);
		Video->SetBrush(ImageBrush(nullptr, FVector2D(1920.f, 1080.f)));
		Video->SetVisibility(ESlateVisibility::Collapsed);
		if (UScaleBoxSlot* Slot = Cast<UScaleBoxSlot>(VideoBox->AddChild(Video)))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		Fill(*Root, *VideoBox);

		// --- 시작 화면: 게임 이름 글자 없이 큰 로고 ---
		Splash->SetVisibility(ESlateVisibility::HitTestInvisible);
		SplashMark->SetBrush(ImageBrush(LoadTexture(MarkTexture), FVector2D(360.f, 360.f)));
		Stack(*SplashCenter, *SplashMark, FMargin(0.f));

		StyleText(*TeamName, 32, TextMain, TEXT("SemiBold"));
		TeamName->SetText(LOCTEXT("TitleIntroTeamName", "스튜디오 들썩"));
		TeamName->SetJustification(ETextJustify::Center);
		Stack(*SplashCenter, *TeamName, FMargin(0.f, 36.f, 0.f, 0.f));

		StyleText(*TeamCourse, 19, TextSub, TEXT("Medium"));
		TeamCourse->SetText(LOCTEXT("TitleIntroTeamCourse", "내일배움캠프 Unreal 트랙 8기 13조"));
		TeamCourse->SetJustification(ETextJustify::Center);
		Stack(*SplashCenter, *TeamCourse, FMargin(0.f, 8.f, 0.f, 0.f));
		Place(*Splash, *SplashCenter, HAlign_Center, VAlign_Center, FMargin(0.f, 0.f, 0.f, 40.f));

		StyleText(*Disclaimer, 17, TextNote, TEXT("Medium"));
		Disclaimer->SetText(LOCTEXT("TitleIntroDisclaimer",
			"이 게임에 등장하는 축제는 실제 축제를 바탕으로 새롭게 구성한 것이며, 실제 축제 주최 측과 관련이 없습니다."));
		Disclaimer->SetJustification(ETextJustify::Center);
		Disclaimer->SetAutoWrapText(true);
		Disclaimer->SetWrapTextAt(1200.f);
		Place(*Splash, *Disclaimer, HAlign_Center, VAlign_Bottom, FMargin(160.f, 0.f, 160.f, 56.f));
		Fill(*Root, *Splash);

		// --- 흰 덮개 ---
		MakeFlat(*FlashCover, FLinearColor::White);
		FlashCover->SetVisibility(ESlateVisibility::Collapsed);
		Fill(*Root, *FlashCover);
		return true;
	}

	void Run()
	{
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_BEGIN: 없는 WBP_FF_TitleIntro 만 만든다."), LogPrefix);
		const UWidgetBlueprint* Blueprint = EnsureWidgetBlueprint(
			LogPrefix, Package, Folder, *UFF_TitleIntroScreen::StaticClass(),
			[](UWidgetBlueprint& InBlueprint, UWidgetTree& Tree) { return Build(InBlueprint, Tree); });
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_%s"), LogPrefix, Blueprint ? TEXT("END") : TEXT("FAIL"));
	}

	void RunApply()
	{
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLY_BEGIN: WBP_FF_TitleIntro 트리를 v2 골격으로 바꾼다."), LogPrefix);
		const UWidgetBlueprint* Blueprint = ReplaceWidgetBlueprint(
			LogPrefix, Package, Folder, *UFF_TitleIntroScreen::StaticClass(),
			[](UWidgetBlueprint& InBlueprint, UWidgetTree& Tree) { return Build(InBlueprint, Tree); });
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLY_%s"), LogPrefix, Blueprint ? TEXT("END") : TEXT("FAIL"));
	}

	FAutoConsoleCommand Command(
		TEXT("FF.TitleIntroUMG"),
		TEXT("시작 화면·인트로 영상·지구 공개 화면 WBP_FF_TitleIntro 를 만든다. 이미 있으면 건드리지 않는다."),
		FConsoleCommandDelegate::CreateStatic(&Run));

	FAutoConsoleCommand ApplyCommand(
		TEXT("FF.ApplyTitleIntroDesign"),
		TEXT("🔴 WBP_FF_TitleIntro 의 트리를 코드 골격으로 통째로 바꾼다. 디자이너에서 고친 배치가 사라진다."),
		FConsoleCommandDelegate::CreateStatic(&RunApply));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
