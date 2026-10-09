// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// FF_BuildPlacementFailureFeedbackWbpBuilder.cpp

// 배치 실패 토스트의 Pearl 카드와 경고 아이콘을 디자이너 편집 가능한 WBP로 만든다.
// 흐름: 최초 생성 또는 명시적 스타일 적용 -> 기존 위젯 재사용 -> 컴파일·저장.
// 불변식: 제목과 상세 위젯 이름·타입은 네이티브 BindWidget 계약과 일치해야 한다.
// 실패/수명: 생성 명령은 기존 에셋을 보존하고 스타일 적용은 이 팝업 한 개만 저장한다.

#include "CoreMinimal.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"

#if WITH_EDITOR

#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildPlacementFailureFeedback.h"
#include "Building/Presentation/FF_BuildTypography.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "WidgetBlueprint.h"
#include "UI/Core/FF_BuildPearlStyle.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFBuildPlacementFailureWbpBuilder, Log, All);

namespace FFBuildPlacementFailureFeedbackWbpBuilder
{
	using FFUIWbpBuilder::MakeWidget;
	const FString FeedbackPackage(
		FFBuildUIAssetPaths::PlacementFailureFeedbackPackage);

	template <typename TWidget>
	TWidget* GetWidget(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, FName Name, bool bVariable = false)
	{
		if (UWidget* Existing = Tree.FindWidget(Name))
		{
			TWidget* Typed = Cast<TWidget>(Existing);
			if (Typed) Typed->Modify();
			return Typed;
		}
		return MakeWidget<TWidget>(Blueprint, Tree, Name, bVariable);
	}

	void StyleText(
		UCommonTextBlock& Text,
		const FFBuildTypography::ERole Role,
		const int32 FontSize,
		const FLinearColor& Color)
	{
		Text.SetStyle(nullptr);
		Text.SetFont(FFBuildTypography::Make(Role, FontSize));
		Text.SetColorAndOpacity(FSlateColor(Color));
		Text.SetJustification(ETextJustify::Left);
		Text.SetAutoWrapText(true);
		Text.SetLineHeightPercentage(1.15f);
		Text.SetShadowOffset(FVector2D::ZeroVector);
	}

	bool BuildLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		UTexture2D* WarningTexture = LoadObject<UTexture2D>(nullptr,
			TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Status/T_FF_LH_Icon_StatusWarning_White_256.T_FF_LH_Icon_StatusWarning_White_256"));
		if (!WarningTexture || !FFBuildTypography::Font(FFBuildTypography::ERole::Heading) ||
			!FFBuildTypography::Font(FFBuildTypography::ERole::Body)) return false;

		UCanvasPanel* Root = GetWidget<UCanvasPanel>(Blueprint, Tree, TEXT("Root_Canvas"));
		USizeBox* ToastSize = GetWidget<USizeBox>(Blueprint, Tree, TEXT("Failure_Size"));
		UBorder* Background = GetWidget<UBorder>(Blueprint, Tree, TEXT("Failure_Background"));
		UHorizontalBox* Row = GetWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Failure_Row"));
		USizeBox* IconSize = GetWidget<USizeBox>(Blueprint, Tree, TEXT("Failure_IconSize"));
		UBorder* IconBackground = GetWidget<UBorder>(Blueprint, Tree, TEXT("Failure_IconBackground"));
		UImage* Icon = GetWidget<UImage>(Blueprint, Tree, TEXT("Failure_Icon"));
		UVerticalBox* Content = GetWidget<UVerticalBox>(Blueprint, Tree, TEXT("Failure_Content"));
		UCommonTextBlock* Title = GetWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("TXT_FailureTitle"), true);
		UCommonTextBlock* Reason = GetWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("TXT_FailureReason"), true);
		if (!Root || !ToastSize || !Background || !Row || !IconSize || !IconBackground || !Icon ||
			!Content || !Title || !Reason)
		{
			return false;
		}

		Root->SetVisibility(ESlateVisibility::HitTestInvisible);
		ToastSize->SetWidthOverride(620.0f);
		ToastSize->SetMinDesiredHeight(112.0f);
		ToastSize->SetVisibility(ESlateVisibility::HitTestInvisible);
		Background->SetBrush(FFBuildPearl::Rounded(
			FFBuildPearl::Color(250, 250, 255, 0.98f), 18.0f, FFBuildPearl::Line, 1.0f));
		Background->SetBrushColor(FLinearColor::White);
		Background->SetPadding(FMargin(22.0f, 18.0f));
		Background->SetHorizontalAlignment(HAlign_Fill);
		Background->SetVerticalAlignment(VAlign_Center);
		IconSize->SetWidthOverride(42.0f);
		IconSize->SetHeightOverride(42.0f);
		IconBackground->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Color(255, 237, 232), 13.0f));
		IconBackground->SetBrushColor(FLinearColor::White);
		IconBackground->SetPadding(FMargin(10.0f));
		Icon->SetBrushFromTexture(WarningTexture);
		Icon->SetColorAndOpacity(FFBuildPearl::Color(168, 75, 64));

		Title->SetText(UFF_BuildPlacementFailureFeedback::MakeFailureTitle(
			EFFZSPlacementFailure::PlacementCollision, false));
		StyleText(*Title, FFBuildTypography::ERole::Heading, 21, FFBuildPearl::Ink);
		Reason->SetText(FText::FromString(
			TEXT("붉은 사선으로 표시된 겹친 부분이 사라지도록 시설을 옮겨 주세요.")));
		StyleText(*Reason, FFBuildTypography::ERole::Body, 16, FFBuildPearl::Color(94, 96, 115));

		Title->RemoveFromParent();
		if (UVerticalBoxSlot* TitleSlot = Content->AddChildToVerticalBox(Title))
		{
			TitleSlot->SetHorizontalAlignment(HAlign_Fill);
			TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
		}
		Reason->RemoveFromParent();
		if (UVerticalBoxSlot* ReasonSlot = Content->AddChildToVerticalBox(Reason))
		{
			ReasonSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		Content->RemoveFromParent();
		IconSize->RemoveFromParent();
		IconBackground->SetContent(Icon);
		IconSize->SetContent(IconBackground);
		UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(IconSize);
		IconSlot->SetVerticalAlignment(VAlign_Top);
		IconSlot->SetPadding(FMargin(0.0f, 2.0f, 16.0f, 0.0f));
		UHorizontalBoxSlot* ContentSlot = Row->AddChildToHorizontalBox(Content);
		ContentSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ContentSlot->SetVerticalAlignment(VAlign_Center);
		Background->SetContent(Row);
		ToastSize->SetContent(Background);
		// Keep the existing Designer placement when applying the style to a saved toast.
		if (ToastSize->GetParent() != Root)
		{
			ToastSize->RemoveFromParent();
			UCanvasPanelSlot* ToastSlot = Root->AddChildToCanvas(ToastSize);
			ToastSlot->SetAnchors(FAnchors(0.5f, 0.0f));
			ToastSlot->SetAlignment(FVector2D(0.5f, 0.0f));
			ToastSlot->SetPosition(FVector2D(0.0f, 120.0f));
			ToastSlot->SetAutoSize(true);
		}
		Tree.RootWidget = Root;
		return true;
	}

	void ApplyPlacementFailureFeedbackStyle()
	{
		if (!GEditor || GEditor->PlayWorld)
		{
			UE_LOG(LogFFBuildPlacementFailureWbpBuilder, Error,
				TEXT("FF_BUILD_FAILURE_STYLE_FAIL: requires an idle editor."));
			return;
		}
		UWidgetBlueprint* Blueprint = FFUIWbpBuilder::LoadExistingBlueprint(FeedbackPackage);
		if (!Blueprint || !Blueprint->GeneratedClass || !Blueprint->WidgetTree ||
			!Blueprint->GeneratedClass->IsChildOf(UFF_BuildPlacementFailureFeedback::StaticClass()))
		{
			UE_LOG(LogFFBuildPlacementFailureWbpBuilder, Error,
				TEXT("FF_BUILD_FAILURE_STYLE_FAIL: invalid feedback WBP."));
			return;
		}
		Blueprint->Modify();
		Blueprint->WidgetTree->Modify();
		const bool bSaved = BuildLayout(*Blueprint, *Blueprint->WidgetTree) &&
			FFUIWbpBuilder::SaveNewBlueprint(TEXT("FF_BUILD_FAILURE_STYLE"), *Blueprint);
		UE_LOG(LogFFBuildPlacementFailureWbpBuilder, Display,
			TEXT("FF_BUILD_FAILURE_STYLE_%s: %s"), bSaved ? TEXT("PASS") : TEXT("FAIL"), *FeedbackPackage);
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

	FAutoConsoleCommand ApplyPlacementFailureFeedbackStyleCommand(
		TEXT("FF.ApplyBuildPlacementFailureFeedbackStyle"),
		TEXT("Restyles only the saved placement failure toast using the shared Pearl palette and construction fonts."),
		FConsoleCommandDelegate::CreateStatic(&ApplyPlacementFailureFeedbackStyle));
}

#endif // WITH_EDITOR

