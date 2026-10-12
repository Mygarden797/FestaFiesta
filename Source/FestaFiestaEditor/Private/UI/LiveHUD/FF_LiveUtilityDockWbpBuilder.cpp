// 왼쪽 위 SNS·음악 도크. 최초 생성 후 스타일과 문구는 Designer 편집을 보존한다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"
#include "UI/LiveHUD/FF_LiveUtilityDock.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Editor.h"
#include "HAL/IConsoleManager.h"

#define LOCTEXT_NAMESPACE "FFLiveUtilityDockBuilder"

// FF_LiveUtilityDockIconWbpBuilder.cpp — 흰 원 제거·음표 이미지. 처음 만든 직후에도 같은 모양이 되게 이어서 부른다.
namespace FFLiveDockIconDesign { void Apply(); }

namespace FFLiveUtilityDockBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	const FString Folder = TEXT("/Game/FestaFiesta/UI/LiveHUD");
	const FString Package = Folder / TEXT("WBP_FF_LiveUtilityDock");
	const TCHAR* Prefix = TEXT("FF_LIVE_UTILITY_DOCK");

	FSlateBrush RoundBrush(FLinearColor Fill, FLinearColor Stroke, float Width)
	{
		FSlateBrush Result = Brush(Fill, 30.f, Width, Stroke);
		Result.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		return Result;
	}

	USizeBox* MakeRoundButton(UWidgetBlueprint& BP, UWidgetTree& Tree, bool bSNS)
	{
		const TCHAR* Suffix = bSNS ? TEXT("SNS") : TEXT("Music");
		USizeBox* Size = MakeWidget<USizeBox>(BP, Tree, FName(FString(TEXT("SizeBox_")) + Suffix), true);
		Size->SetWidthOverride(60.f); Size->SetHeightOverride(60.f);
		UOverlay* Layers = MakeWidget<UOverlay>(BP, Tree, FName(FString(TEXT("Overlay_")) + Suffix)); Size->AddChild(Layers);
		UVerticalBox* Content = MakeWidget<UVerticalBox>(BP, Tree, FName(FString(TEXT("Content_")) + Suffix));
		if (bSNS)
		{
			UImage* Icon = MakeWidget<UImage>(BP, Tree, TEXT("Image_SNS"));
			Icon->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr, TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Navigation/T_FF_LH_Icon_SNS_White_256.T_FF_LH_Icon_SNS_White_256")));
			Icon->SetColorAndOpacity(Ink); Icon->SetDesiredSizeOverride(FVector2D(26.f));
			Row(*Content, *Icon); CastChecked<UVerticalBoxSlot>(Icon->Slot)->SetHorizontalAlignment(HAlign_Center);
		}
		else
		{
			UTextBlock* Icon = Text(BP, Tree, TEXT("Text_MusicIcon"), FText::FromString(TEXT("♫")), 25, Ink);
			Icon->SetJustification(ETextJustify::Center); Row(*Content, *Icon);
		}
		UTextBlock* Label = Text(BP, Tree, bSNS ? TEXT("Text_SNSLabel") : TEXT("Text_MusicLabel"), bSNS ? LOCTEXT("SNS", "SNS") : LOCTEXT("Music", "음악"), 11, Ink);
		Label->SetJustification(ETextJustify::Center); Row(*Content, *Label);
		UButton* Control = Button(BP, Tree, bSNS ? TEXT("Button_SNS") : TEXT("Button_Music"), *Content);
		FButtonStyle Style = Control->GetStyle();
		Style.Normal = RoundBrush(Paper, SkyLine, 1.5f);
		Style.Hovered = RoundBrush(SkyWash, Color(92, 168, 203), 2.f);
		Style.Pressed = RoundBrush(Color(220, 240, 245), Green, 2.f);
		Style.NormalPadding = Style.PressedPadding = FMargin(3.f);
		Control->SetStyle(Style);
		Control->SetToolTipText(bSNS ? LOCTEXT("SnsHint", "SNS 열기 / 닫기") : LOCTEXT("MusicHint", "음악 열기 / 닫기"));
		UOverlaySlot* ButtonSlot = Layers->AddChildToOverlay(Control);
		ButtonSlot->SetHorizontalAlignment(HAlign_Fill); ButtonSlot->SetVerticalAlignment(VAlign_Fill);
		UTextBlock* Selected = Text(BP, Tree, bSNS ? TEXT("Text_SNSSelected") : TEXT("Text_MusicSelected"), FText::FromString(TEXT("✓")), 18, Green, true);
		UOverlaySlot* CheckSlot = Layers->AddChildToOverlay(Selected);
		CheckSlot->SetHorizontalAlignment(HAlign_Right); CheckSlot->SetVerticalAlignment(VAlign_Bottom);
		Selected->SetVisibility(ESlateVisibility::Collapsed);
		if (bSNS)
		{
			UBorder* Badge = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Unread"));
			Badge->SetBrush(RoundBrush(Color(199, 88, 66), Paper, 1.f)); Badge->SetPadding(FMargin(5.f, 1.f));
			Badge->AddChild(Text(BP, Tree, TEXT("Text_Unread"), FText::AsNumber(0), 12, Paper, true));
			UOverlaySlot* BadgeSlot = Layers->AddChildToOverlay(Badge);
			BadgeSlot->SetHorizontalAlignment(HAlign_Right); BadgeSlot->SetVerticalAlignment(VAlign_Top);
			Badge->SetVisibility(ESlateVisibility::Collapsed);
		}
		return Size;
	}

	bool Build(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(BP, Tree, TEXT("Canvas_Root")); Tree.RootWidget = Root;
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UCanvasPanel* Dock = MakeWidget<UCanvasPanel>(BP, Tree, TEXT("Panel_Dock"), true);
		Dock->SetVisibility(ESlateVisibility::SelfHitTestInvisible); StretchOnCanvas(*Root, *Dock);
		PlaceOnCanvas(*Dock, *MakeRoundButton(BP, Tree, true), 24.f, 24.f, 60.f, 60.f);
		PlaceOnCanvas(*Dock, *MakeRoundButton(BP, Tree, false), 24.f, 96.f, 60.f, 60.f);
		UOverlay* Host = MakeWidget<UOverlay>(BP, Tree, TEXT("Panel_Flyout"), true);
		Host->SetVisibility(ESlateVisibility::Collapsed);
		PlaceOnCanvas(*Dock, *Host, 104.f, 24.f, 480.f, 640.f);
		return true;
	}

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld) return;
		if (EnsureWidgetBlueprint(Prefix, Package, Folder, *UFF_LiveUtilityDock::StaticClass(), Build))
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: SNS·음악 도크 저장, 기존 Designer 편집 보존"), Prefix);
			// 이미 적용된 WBP는 건너뛴다. 음표 텍스처가 없으면 그 명령이 실패를 남긴다.
			FFLiveDockIconDesign::Apply();
		}
	}
	FAutoConsoleCommand Command(TEXT("FF.LiveUtilityDockSky"), TEXT("SNS·음악 원형 버튼과 공통 펼침 위치를 최초 한 번 생성한다."), FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
