// FF_LoadingScreenWbpBuilder.cpp

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Components/Border.h"
#include "Components/CircularThrobber.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_LoadingScreenWidget.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLoadingWbpBuilder"

namespace FFLoadingScreenWbpBuilder
{
	using namespace FFUIWbpBuilder;

	constexpr const TCHAR* LoadingLogPrefix = TEXT("FF_LOADING_UMG");
	const FString LoadingFolder = TEXT("/Game/FestaFiesta/UI/Loading");
	const FString LoadingScreenPackage = LoadingFolder / TEXT("WBP_FF_LoadingScreen");

	bool BuildLoadingScreen(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: UI 팔레트를 읽을 수 없어 로딩 화면을 만들지 않았다."),
				LoadingLogPrefix);
			return false;
		}

		UOverlay* Root = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Root"));
		UBorder* Background = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Background"), true);
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Content"));
		UTextBlock* FestivalName = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_FestivalName"), true);
		USizeBox* DividerBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Divider"));
		UBorder* Divider = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Divider"));
		UHorizontalBox* LoadingRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Loading"));
		USizeBox* ThrobberBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_LoadingThrobber"));
		UCircularThrobber* Throbber = MakeWidget<UCircularThrobber>(
			Blueprint, Tree, TEXT("Throbber_Loading"), true);
		UTextBlock* LoadingText = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Loading"), true);
		UTextBlock* Tip = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Tip"), true);

		if (!Root || !Background || !Content || !FestivalName || !DividerBox || !Divider
			|| !LoadingRow || !ThrobberBox || !Throbber || !LoadingText || !Tip)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetVisibility(ESlateVisibility::HitTestInvisible);

		Background->SetBrushColor(Palette->Frontend.Void);
		Background->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* Slot = Root->AddChildToOverlay(Background))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}

		StyleText(*FestivalName, 52, Palette->Frontend.Title, TEXT("SemiBold"));
		FestivalName->SetText(LOCTEXT("LoadingFestivalPreview", "축제 이름"));
		FestivalName->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(FestivalName))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}

		Divider->SetBrushColor(Palette->Frontend.EdgeStrong);
		DividerBox->SetWidthOverride(180.f);
		DividerBox->SetHeightOverride(2.f);
		DividerBox->AddChild(Divider);
		if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(DividerBox))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetPadding(FMargin(0.f, 24.f, 0.f, 30.f));
		}

		Throbber->SetNumberOfPieces(8);
		Throbber->SetPeriod(1.2f);
		Throbber->SetRadius(14.f);
		ThrobberBox->SetWidthOverride(36.f);
		ThrobberBox->SetHeightOverride(36.f);
		ThrobberBox->AddChild(Throbber);
		if (UHorizontalBoxSlot* Slot = LoadingRow->AddChildToHorizontalBox(ThrobberBox))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
		}

		StyleText(*LoadingText, 22, Palette->Frontend.Text, TEXT("Medium"));
		LoadingText->SetText(LOCTEXT("LoadingMessagePreview", "축제를 준비하고 있습니다"));
		if (UHorizontalBoxSlot* Slot = LoadingRow->AddChildToHorizontalBox(LoadingText))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(LoadingRow))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetHorizontalAlignment(HAlign_Center);
		}

		StyleText(*Tip, 16, Palette->Frontend.Sub, TEXT("Medium"));
		Tip->SetText(LOCTEXT(
			"LoadingTipPreview",
			"운영 중에도 시간은 계속 흐릅니다. 상황을 확인하며 계획을 조정하세요."));
		Tip->SetJustification(ETextJustify::Center);
		Tip->SetAutoWrapText(true);
		Tip->SetWrapTextAt(900.f);
		if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(Tip))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetPadding(FMargin(0.f, 44.f, 0.f, 0.f));
		}

		Content->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* Slot = Root->AddChildToOverlay(Content))
		{
			// MoviePlayer는 일반 뷰포트의 DPI 규칙을 그대로 보장하지 않는다.
			// 절대 좌표 대신 Slate 중앙 정렬을 써서 해상도와 화면비가 달라도 중앙을 유지한다.
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(64.f));
		}
		return true;
	}

	void RunLoadingUmg()
	{
		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_BEGIN: 없는 WBP_FF_LoadingScreen만 생성한다."), LoadingLogPrefix);

		EnsureWidgetBlueprint(
			LoadingLogPrefix,
			LoadingScreenPackage,
			LoadingFolder,
			*UFF_LoadingScreenWidget::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildLoadingScreen(Blueprint, Tree);
			});

		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_END"), LoadingLogPrefix);
	}

	FAutoConsoleCommand LoadingUmgCommand(
		TEXT("FF.LoadingUMG"),
		TEXT("축제 레벨용 WBP_FF_LoadingScreen을 만든다. 이미 있으면 건드리지 않는다."),
		FConsoleCommandDelegate::CreateStatic(&RunLoadingUmg));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
