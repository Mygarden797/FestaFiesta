// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// FF_BuildPlacementFailureFeedbackWbpBuilder.cpp

// 배치 실패 토스트 WBP가 없을 때 기준 WidgetTree를 생성하는 에디터 전용 콘솔 도구를 구현한다.
// 흐름: 에셋 존재 확인 -> Blueprint 생성 -> 제목/상세 BindWidget 구성 -> 컴파일·저장.
// 불변식: 제목과 상세 위젯 이름·타입은 네이티브 BindWidget 계약과 일치해야 한다.
// 실패/수명: 기존 에셋은 덮어쓰지 않고 생성 실패 단계마다 중단해 부분 저장을 피한다.

#include "CoreMinimal.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"

#if WITH_EDITOR

#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildPlacementFailureFeedback.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "WidgetBlueprint.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFBuildPlacementFailureWbpBuilder, Log, All);

namespace FFBuildPlacementFailureFeedbackWbpBuilder
{
	using FFUIWbpBuilder::MakeWidget;
	const FString FeedbackPackage(
		FFBuildUIAssetPaths::PlacementFailureFeedbackPackage);

	void StyleText(
		UCommonTextBlock& Text,
		const int32 FontSize,
		const FLinearColor& Color,
		const ETextJustify::Type Justification = ETextJustify::Center)
	{
		FSlateFontInfo Font = Text.GetFont();
		Font.Size = FontSize;
		Text.SetFont(Font);
		Text.SetColorAndOpacity(FSlateColor(Color));
		Text.SetJustification(Justification);
	}

	bool BuildLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("Root_Canvas"));
		USizeBox* ToastSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Failure_Size"));
		UBorder* Background = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Failure_Background"));
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Failure_Content"));
		UCommonTextBlock* Title = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("TXT_FailureTitle"), true);
		UCommonTextBlock* Reason = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("TXT_FailureReason"), true);
		if (!Root || !ToastSize || !Background || !Content || !Title || !Reason)
		{
			return false;
		}

		Root->SetVisibility(ESlateVisibility::HitTestInvisible);
		ToastSize->SetWidthOverride(620.0f);
		ToastSize->SetMinDesiredHeight(86.0f);
		ToastSize->SetVisibility(ESlateVisibility::HitTestInvisible);
		Background->SetBrushColor(FLinearColor(0.018f, 0.025f, 0.038f, 0.94f));
		Background->SetPadding(FMargin(28.0f, 14.0f));
		Background->SetHorizontalAlignment(HAlign_Fill);
		Background->SetVerticalAlignment(VAlign_Fill);

		Title->SetText(FText::FromString(TEXT("골드 부족")));
		Title->SetAutoWrapText(true);
		StyleText(*Title, 20, FLinearColor(1.0f, 0.38f, 0.20f, 1.0f));
		Reason->SetText(FText::FromString(
			TEXT("시설 설치비 400 Gold가 필요하지만 현재 잔액은 0 Gold입니다.")));
		Reason->SetAutoWrapText(true);
		StyleText(*Reason, 15, FLinearColor(0.94f, 0.95f, 0.98f, 1.0f));

		if (UVerticalBoxSlot* TitleSlot = Content->AddChildToVerticalBox(Title))
		{
			TitleSlot->SetHorizontalAlignment(HAlign_Fill);
			TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
		}
		if (UVerticalBoxSlot* ReasonSlot = Content->AddChildToVerticalBox(Reason))
		{
			ReasonSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		Background->AddChild(Content);
		ToastSize->AddChild(Background);
		if (UCanvasPanelSlot* ToastSlot = Root->AddChildToCanvas(ToastSize))
		{
			ToastSlot->SetAnchors(FAnchors(0.5f, 0.0f));
			ToastSlot->SetAlignment(FVector2D(0.5f, 0.0f));
			ToastSlot->SetPosition(FVector2D(0.0f, 54.0f));
			ToastSlot->SetAutoSize(true);
		}
		Tree.RootWidget = Root;
		return true;
	}

	void BuildPlacementFailureFeedbackUmg()
	{
		// 신규 기능 패키지만 만들며 기존 팀 UI나 기존 디자이너 편집본은 절대 저장하지 않습니다.
		if (FPackageName::DoesPackageExist(FeedbackPackage))
		{
			UWidgetBlueprint* Existing = FFUIWbpBuilder::LoadExistingBlueprint(FeedbackPackage);
			if (Existing && Existing->GeneratedClass &&
				Existing->GeneratedClass->IsChildOf(
					UFF_BuildPlacementFailureFeedback::StaticClass()))
			{
				UE_LOG(LogFFBuildPlacementFailureWbpBuilder, Display,
					TEXT("FF_BUILD_FAILURE_FEEDBACK_REUSE: %s (Designer changes preserved)"),
					*FeedbackPackage);
			}
			else
			{
				UE_LOG(LogFFBuildPlacementFailureWbpBuilder, Error,
					TEXT("FF_BUILD_FAILURE_FEEDBACK_FAIL: existing asset has an invalid parent: %s"),
					*FeedbackPackage);
			}
			return;
		}

		// 생성·위젯 이름 등록·고아 위젯 검사·저장은 팀 공통 도구가 담당한다.
		if (!FFUIWbpBuilder::EnsureWidgetBlueprint(
			TEXT("FF_BUILD_FAILURE_FEEDBACK"), FeedbackPackage, FPackageName::GetLongPackagePath(FeedbackPackage),
			*UFF_BuildPlacementFailureFeedback::StaticClass(), BuildLayout))
		{
			UE_LOG(LogFFBuildPlacementFailureWbpBuilder, Error,
				TEXT("FF_BUILD_FAILURE_FEEDBACK_FAIL: could not create %s"), *FeedbackPackage);
			return;
		}

		UE_LOG(LogFFBuildPlacementFailureWbpBuilder, Display,
			TEXT("FF_BUILD_FAILURE_FEEDBACK_CREATED: %s"), *FeedbackPackage);
	}

	FAutoConsoleCommand BuildPlacementFailureFeedbackUmgCommand(
		TEXT("FF.BuildPlacementFailureFeedbackUMG"),
		TEXT("Creates the editable build placement failure toast without modifying team UI assets."),
		FConsoleCommandDelegate::CreateStatic(&BuildPlacementFailureFeedbackUmg));
}

#endif // WITH_EDITOR

