// FF_BuildFacilityCostTooltipWbpBuilder.cpp

// 비용 툴팁 WBP가 없을 때 기준 WidgetTree를 생성하는 에디터 전용 콘솔 도구를 구현한다.
// 흐름: 기존 에셋 보존 확인 -> Blueprint 생성 -> 필수 BindWidget 계층 구성 -> 컴파일·저장.
// 불변식: 필수 위젯 이름과 타입은 네이티브 BindWidget 계약과 항상 일치해야 한다.
// 실패/수명: 명시적 콘솔 명령에서만 실행하며 기존 에셋이나 컴파일 실패 패키지는 덮어쓰지 않는다.

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildFacilityCostTooltip.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogFFBuildCostTooltipWbpBuilder, Log, All);

namespace FFBuildFacilityCostTooltipWbpBuilder
{
	const FString TooltipPackage(
		TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Tooltips/WBP_FF_BuildFacilityCostTooltip"));

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
		const ETextJustify::Type Justification = ETextJustify::Left)
	{
		FSlateFontInfo Font = Text.GetFont();
		Font.Size = FontSize;
		Text.SetFont(Font);
		Text.SetColorAndOpacity(FSlateColor(Color));
		Text.SetJustification(Justification);
	}

	bool BuildLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		USizeBox* Root = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Root_Size"));
		UBorder* Background = MakeWidget<UBorder>(
			Blueprint, Tree, TEXT("Tooltip_Background"));
		UVerticalBox* Content = MakeWidget<UVerticalBox>(
			Blueprint, Tree, TEXT("Tooltip_Content"));
		UCommonTextBlock* FacilityName = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("TXT_FacilityName"), true);
		UHorizontalBox* CostRow = MakeWidget<UHorizontalBox>(
			Blueprint, Tree, TEXT("Cost_Row"));
		UCommonTextBlock* CostLabel = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("TXT_CostLabel"));
		UCommonTextBlock* InstallCost = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("TXT_InstallCost"), true);
		UHorizontalBox* RecoveryRow = MakeWidget<UHorizontalBox>(
			Blueprint, Tree, TEXT("Recovery_Row"));
		UCommonTextBlock* RecoveryLabel = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("TXT_RecoveryLabel"));
		UCommonTextBlock* RecoveryCost = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("TXT_RecoveryCost"), true);
		if (!Root || !Background || !Content || !FacilityName || !CostRow ||
			!CostLabel || !InstallCost || !RecoveryRow || !RecoveryLabel || !RecoveryCost)
		{
			return false;
		}

		Root->SetWidthOverride(300.0f);
		Root->SetMinDesiredHeight(110.0f);
		Root->SetVisibility(ESlateVisibility::HitTestInvisible);
		Background->SetBrushColor(FLinearColor(0.018f, 0.047f, 0.070f, 0.98f));
		Background->SetPadding(FMargin(16.0f, 11.0f));
		Background->SetHorizontalAlignment(HAlign_Fill);
		Background->SetVerticalAlignment(VAlign_Fill);

		FacilityName->SetText(FText::FromString(TEXT("시설 이름")));
		FacilityName->SetAutoWrapText(true);
		StyleText(*FacilityName, 13, FLinearColor(0.72f, 0.80f, 0.88f, 1.0f));
		CostLabel->SetText(FText::FromString(TEXT("건설 비용")));
		StyleText(*CostLabel, 14, FLinearColor(0.91f, 0.96f, 1.0f, 1.0f));
		InstallCost->SetText(FText::GetEmpty());
		StyleText(
			*InstallCost, 20, FLinearColor(0.08f, 0.95f, 0.88f, 1.0f), ETextJustify::Right);
		RecoveryLabel->SetText(FText::FromString(TEXT("회수 비용")));
		StyleText(*RecoveryLabel, 14, FLinearColor(0.91f, 0.96f, 1.0f, 1.0f));
		RecoveryCost->SetText(FText::GetEmpty());
		StyleText(
			*RecoveryCost, 18, FLinearColor(0.98f, 0.80f, 0.28f, 1.0f), ETextJustify::Right);

		if (UHorizontalBoxSlot* LabelSlot = CostRow->AddChildToHorizontalBox(CostLabel))
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* CostSlot = CostRow->AddChildToHorizontalBox(InstallCost))
		{
			CostSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			CostSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* LabelSlot = RecoveryRow->AddChildToHorizontalBox(RecoveryLabel))
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* CostSlot = RecoveryRow->AddChildToHorizontalBox(RecoveryCost))
		{
			CostSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			CostSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UVerticalBoxSlot* NameSlot = Content->AddChildToVerticalBox(FacilityName))
		{
			NameSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		}
		Content->AddChildToVerticalBox(CostRow);
		if (UVerticalBoxSlot* RecoverySlot = Content->AddChildToVerticalBox(RecoveryRow))
		{
			RecoverySlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
		}
		Background->AddChild(Content);
		Root->AddChild(Background);
		Tree.RootWidget = Root;
		return true;
	}

	UWidgetBlueprint* LoadExistingBlueprint()
	{
		const FString AssetName = FPackageName::GetLongPackageAssetName(TooltipPackage);
		return LoadObject<UWidgetBlueprint>(
			nullptr, *FString::Printf(TEXT("%s.%s"), *TooltipPackage, *AssetName));
	}

	bool HasRequiredLayout(const UWidgetBlueprint& Blueprint)
	{
		const UWidgetTree* Tree = Blueprint.WidgetTree;
		const UVerticalBox* Content = Tree
			? Cast<UVerticalBox>(Tree->FindWidget(TEXT("Tooltip_Content")))
			: nullptr;
		const UHorizontalBox* CostRow = Tree
			? Cast<UHorizontalBox>(Tree->FindWidget(TEXT("Cost_Row")))
			: nullptr;
		const UHorizontalBox* RecoveryRow = Tree
			? Cast<UHorizontalBox>(Tree->FindWidget(TEXT("Recovery_Row")))
			: nullptr;
		const UCommonTextBlock* InstallCost = Tree
			? Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("TXT_InstallCost")))
			: nullptr;
		const UCommonTextBlock* RecoveryLabel = Tree
			? Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("TXT_RecoveryLabel")))
			: nullptr;
		const UCommonTextBlock* RecoveryCost = Tree
			? Cast<UCommonTextBlock>(Tree->FindWidget(TEXT("TXT_RecoveryCost")))
			: nullptr;

		return Content && CostRow && RecoveryRow && InstallCost && RecoveryLabel && RecoveryCost &&
			Content->GetChildIndex(CostRow) != INDEX_NONE &&
			Content->GetChildIndex(RecoveryRow) == Content->GetChildIndex(CostRow) + 1;
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

	void BuildFacilityCostTooltipUmg()
	{
		// 이 명령은 신규 기능 소유 패키지만 다룹니다. 기존 팀 UI 경로의 어떤 에셋도
		// 로드 후 수정하거나 교체하지 않으며, 재실행 시 디자이너 편집본을 그대로 보존합니다.
		if (FPackageName::DoesPackageExist(TooltipPackage))
		{
			UWidgetBlueprint* Existing = LoadExistingBlueprint();
			if (Existing && Existing->GeneratedClass &&
				Existing->GeneratedClass->IsChildOf(UFF_BuildFacilityCostTooltip::StaticClass()) &&
				HasRequiredLayout(*Existing))
			{
				UE_LOG(LogFFBuildCostTooltipWbpBuilder, Display,
					TEXT("FF_BUILD_COST_TOOLTIP_REUSE: %s (Designer changes preserved)"),
					*TooltipPackage);
			}
			else
			{
				UE_LOG(LogFFBuildCostTooltipWbpBuilder, Error,
					TEXT("FF_BUILD_COST_TOOLTIP_FAIL: existing asset has an invalid parent or widget contract: %s"),
					*TooltipPackage);
			}
			return;
		}

		UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
		Factory->ParentClass = UFF_BuildFacilityCostTooltip::StaticClass();
		FAssetToolsModule& AssetTools =
			FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		const FString AssetName = FPackageName::GetLongPackageAssetName(TooltipPackage);
		const FString PackagePath = FPackageName::GetLongPackagePath(TooltipPackage);
		UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(AssetTools.Get().CreateAsset(
			AssetName, PackagePath, UWidgetBlueprint::StaticClass(), Factory));
		if (!Blueprint || !Blueprint->WidgetTree ||
			!BuildLayout(*Blueprint, *Blueprint->WidgetTree) ||
			!SaveNewBlueprint(*Blueprint))
		{
			UE_LOG(LogFFBuildCostTooltipWbpBuilder, Error,
				TEXT("FF_BUILD_COST_TOOLTIP_FAIL: could not create %s"), *TooltipPackage);
			return;
		}

		UE_LOG(LogFFBuildCostTooltipWbpBuilder, Display,
			TEXT("FF_BUILD_COST_TOOLTIP_CREATED: %s"), *TooltipPackage);
	}

	FAutoConsoleCommand BuildFacilityCostTooltipUmgCommand(
		TEXT("FF.BuildFacilityCostTooltipUMG"),
		TEXT("Creates the editable build-facility cost tooltip WBP without modifying existing team UI assets."),
		FConsoleCommandDelegate::CreateStatic(&BuildFacilityCostTooltipUmg));
}

#endif // WITH_EDITOR
