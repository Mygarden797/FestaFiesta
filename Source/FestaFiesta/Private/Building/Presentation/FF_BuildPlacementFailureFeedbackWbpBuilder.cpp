// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildPlacementFailureFeedback.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFBuildPlacementFailureWbpBuilder, Log, All);

namespace FFBuildPlacementFailureFeedbackWbpBuilder
{
	const FString FeedbackPackage(
		TEXT("/Game/FestaFiesta/Building/Feedback/WBP_FF_BuildPlacementFailureFeedback"));

	template <typename TWidget>
	TWidget* MakeWidget(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const FName Name,
		const bool bVariable = false)
	{
		TWidget* Widget = Tree.ConstructWidget<TWidget>(TWidget::StaticClass(), Name);
		if (Widget)
		{
			Widget->bIsVariable = bVariable;
			Blueprint.OnVariableAdded(Name);
		}
		return Widget;
	}

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

	UWidgetBlueprint* LoadExistingBlueprint()
	{
		const FString AssetName = FPackageName::GetLongPackageAssetName(FeedbackPackage);
		return LoadObject<UWidgetBlueprint>(
			nullptr, *FString::Printf(TEXT("%s.%s"), *FeedbackPackage, *AssetName));
	}

	bool SaveNewBlueprint(UWidgetBlueprint& Blueprint)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&Blueprint);
		FKismetEditorUtilities::CompileBlueprint(&Blueprint);
		if (Blueprint.Status == BS_Error)
		{
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

	void BuildPlacementFailureFeedbackUmg()
	{
		// 신규 기능 패키지만 만들며 기존 팀 UI나 기존 디자이너 편집본은 절대 저장하지 않습니다.
		if (FPackageName::DoesPackageExist(FeedbackPackage))
		{
			UWidgetBlueprint* Existing = LoadExistingBlueprint();
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

		UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
		Factory->ParentClass = UFF_BuildPlacementFailureFeedback::StaticClass();
		FAssetToolsModule& AssetTools =
			FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		const FString AssetName = FPackageName::GetLongPackageAssetName(FeedbackPackage);
		const FString PackagePath = FPackageName::GetLongPackagePath(FeedbackPackage);
		UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(AssetTools.Get().CreateAsset(
			AssetName, PackagePath, UWidgetBlueprint::StaticClass(), Factory));
		if (!Blueprint || !Blueprint->WidgetTree ||
			!BuildLayout(*Blueprint, *Blueprint->WidgetTree) ||
			!SaveNewBlueprint(*Blueprint))
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

