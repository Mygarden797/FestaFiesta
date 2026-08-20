// FF_BuildModeGuideWbpBuilder.cpp

// 건축 모드 안내 WBP가 없을 때 기준 레이아웃을 생성하는 에디터 전용 콘솔 도구를 구현한다.
// 흐름: 대상 패키지 확인 -> Blueprint/WidgetTree 구성 -> 컴파일 -> 콘텐츠 패키지 저장.
// 불변식: 생성 클래스는 UFF_BuildModeGuide를 상속하고 안내 패널은 입력을 가로채지 않는다.
// 실패/수명: 기존 에셋은 보존하며 생성·컴파일·저장 실패 단계마다 로그를 남기고 중단한다.

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildModeGuide.h"
#include "CommonTextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogFFBuildModeGuideWbpBuilder, Log, All);

namespace FFBuildModeGuideWbpBuilder
{
	const FString GuidePackage(TEXT("/Game/FestaFiesta/Building/Guide/WBP_FF_BuildModeGuide"));

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

	void StyleRowText(UCommonTextBlock& Text, const int32 FontSize, const FLinearColor& Color)
	{
		FSlateFontInfo Font = Text.GetFont();
		Font.Size = FontSize;
		Text.SetFont(Font);
		Text.SetColorAndOpacity(FSlateColor(Color));
		Text.SetJustification(ETextJustify::Left);
	}

	bool BuildLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("Root_Canvas"));
		UVerticalBox* Rows = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Guide_Rows"), true);
		if (!Root || !Rows)
		{
			return false;
		}

		Root->SetVisibility(ESlateVisibility::HitTestInvisible);

		// 프로덕션 건축 모드의 기본 조작 계약을 좌하단 요약으로 표시한다.
		static const TArray<TPair<FString, FString>> RowsData = {
			{TEXT("[Esc]"), TEXT("건축모드에서 나가기")},
			{TEXT("[WASD]"), TEXT("카메라 이동")},
			{TEXT("[Q] [E]"), TEXT("시설 90도 회전")},
			{TEXT("[좌클릭]"), TEXT("철거/이동 대상 선택")},
			{TEXT("[우클릭]"), TEXT("선택 취소")},
		};

		int32 RowIndex = 0;
		for (const TPair<FString, FString>& Row : RowsData)
		{
			UCommonTextBlock* RowText = MakeWidget<UCommonTextBlock>(
				Blueprint, Tree, *FString::Printf(TEXT("TXT_GuideRow_%d"), RowIndex++), true);
			if (!RowText)
			{
				return false;
			}
			RowText->SetText(FText::FromString(FString::Printf(TEXT("%s  %s"), *Row.Key, *Row.Value)));
			RowText->SetAutoWrapText(false);
			StyleRowText(*RowText, 15, FLinearColor(0.94f, 0.95f, 0.98f, 1.0f));
			if (UVerticalBoxSlot* RowSlot = Rows->AddChildToVerticalBox(RowText))
			{
				RowSlot->SetHorizontalAlignment(HAlign_Left);
				RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
			}
		}

		if (UCanvasPanelSlot* RowsSlot = Root->AddChildToCanvas(Rows))
		{
			RowsSlot->SetAnchors(FAnchors(0.0f, 1.0f));
			RowsSlot->SetAlignment(FVector2D(0.0f, 1.0f));
			RowsSlot->SetPosition(FVector2D(32.0f, -32.0f));
			RowsSlot->SetAutoSize(true);
		}
		Tree.RootWidget = Root;
		return true;
	}

	UWidgetBlueprint* LoadExistingBlueprint()
	{
		const FString AssetName = FPackageName::GetLongPackageAssetName(GuidePackage);
		return LoadObject<UWidgetBlueprint>(
			nullptr, *FString::Printf(TEXT("%s.%s"), *GuidePackage, *AssetName));
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

	void BuildModeGuideUmg()
	{
		// 신규 기능 패키지만 만들며 기존 팀 UI나 기존 디자이너 편집본은 절대 저장하지 않습니다.
		if (FPackageName::DoesPackageExist(GuidePackage))
		{
			UWidgetBlueprint* Existing = LoadExistingBlueprint();
			if (Existing && Existing->GeneratedClass &&
				Existing->GeneratedClass->IsChildOf(UFF_BuildModeGuide::StaticClass()))
			{
				UE_LOG(LogFFBuildModeGuideWbpBuilder, Display,
					TEXT("FF_BUILD_MODE_GUIDE_REUSE: %s (Designer changes preserved)"), *GuidePackage);
			}
			else
			{
				UE_LOG(LogFFBuildModeGuideWbpBuilder, Error,
					TEXT("FF_BUILD_MODE_GUIDE_FAIL: existing asset has an invalid parent: %s"), *GuidePackage);
			}
			return;
		}

		UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
		Factory->ParentClass = UFF_BuildModeGuide::StaticClass();
		FAssetToolsModule& AssetTools =
			FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		const FString AssetName = FPackageName::GetLongPackageAssetName(GuidePackage);
		const FString PackagePath = FPackageName::GetLongPackagePath(GuidePackage);
		UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(AssetTools.Get().CreateAsset(
			AssetName, PackagePath, UWidgetBlueprint::StaticClass(), Factory));
		if (!Blueprint || !Blueprint->WidgetTree ||
			!BuildLayout(*Blueprint, *Blueprint->WidgetTree) ||
			!SaveNewBlueprint(*Blueprint))
		{
			UE_LOG(LogFFBuildModeGuideWbpBuilder, Error,
				TEXT("FF_BUILD_MODE_GUIDE_FAIL: could not create %s"), *GuidePackage);
			return;
		}

		UE_LOG(LogFFBuildModeGuideWbpBuilder, Display,
			TEXT("FF_BUILD_MODE_GUIDE_CREATED: %s"), *GuidePackage);
	}

	FAutoConsoleCommand BuildModeGuideUmgCommand(
		TEXT("FF.BuildModeGuideUMG"),
		TEXT("Creates the editable build mode guide panel without modifying team UI assets."),
		FConsoleCommandDelegate::CreateStatic(&BuildModeGuideUmg));
}

#endif // WITH_EDITOR
