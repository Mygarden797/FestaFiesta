// FF_ZoneLifecycleTestPanelWbpBuilder.cpp

// Test_Main용 Lifecycle 패널 WBP가 없을 때 기준 컨트롤과 상태 영역을 생성한다.
// 흐름: Blueprint 생성 -> 필수 BindWidget 이름으로 레이아웃 조립 -> 컴파일·저장.
// 불변식: 버튼·텍스트 이름과 타입은 테스트 패널 헤더의 BindWidget 계약과 일치해야 한다.
// 실패/수명: 에디터 콘솔 명령에서만 동작하고 기존 에셋이나 컴파일 실패 결과는 덮어쓰지 않는다.

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Building/Testing/FF_ZoneLifecycleTestPanel.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogFFZoneLifecycleTestWbpBuilder, Log, All);

namespace FFZoneLifecycleTestWbpBuilder
{
	const FString PanelPackage(
		TEXT("/Game/FestaFiesta/Building/Debug/WBP_FF_ZoneLifecycleTestPanel"));

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
		Text.SetAutoWrapText(true);
	}

	UButton* MakeButton(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const FName ButtonName,
		const FString& Label)
	{
		UButton* Button = MakeWidget<UButton>(Blueprint, Tree, ButtonName, true);
		UCommonTextBlock* Text = MakeWidget<UCommonTextBlock>(
			Blueprint,
			Tree,
			*FString::Printf(TEXT("TXT_%s_Label"), *ButtonName.ToString()));
		if (!Button || !Text)
		{
			return nullptr;
		}

		Button->SetBackgroundColor(FLinearColor(0.10f, 0.19f, 0.29f, 1.0f));
		Text->SetText(FText::FromString(Label));
		Text->SetMargin(FMargin(8.0f, 5.0f));
		StyleText(*Text, 13, FLinearColor(0.94f, 0.96f, 1.0f, 1.0f), ETextJustify::Center);
		Button->AddChild(Text);
		return Button;
	}

	bool AddButtonRow(UHorizontalBox& Row, const TArray<UButton*>& Buttons)
	{
		for (UButton* Button : Buttons)
		{
			if (!Button)
			{
				return false;
			}
			if (UHorizontalBoxSlot* Slot = Row.AddChildToHorizontalBox(Button))
			{
				Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				Slot->SetPadding(FMargin(3.0f));
				Slot->SetHorizontalAlignment(HAlign_Fill);
			}
		}
		return true;
	}

	bool BuildLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("Root_Canvas"));
		USizeBox* PanelSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Panel_Size"));
		UBorder* Background = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Panel_Background"));
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_Content"));
		if (!Root || !PanelSize || !Background || !Content)
		{
			return false;
		}

		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		PanelSize->SetWidthOverride(510.0f);
		PanelSize->SetMinDesiredHeight(390.0f);
		Background->SetBrushColor(FLinearColor(0.012f, 0.025f, 0.045f, 0.96f));
		Background->SetPadding(FMargin(18.0f));

		UCommonTextBlock* Title = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("TXT_Title"));
		UCommonTextBlock* Guide = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("TXT_Guide"));
		UCommonTextBlock* Stage = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("TXT_Stage"), true);
		UCommonTextBlock* PhaseRevision = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("TXT_PhaseRevision"), true);
		UCommonTextBlock* FacilityCounts = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("TXT_FacilityCounts"), true);
		UCommonTextBlock* QueueCounts = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("TXT_QueueCounts"), true);
		UCommonTextBlock* LastResult = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("TXT_LastResult"), true);
		if (!Title || !Guide || !Stage || !PhaseRevision || !FacilityCounts || !QueueCounts
			|| !LastResult)
		{
			return false;
		}

		Title->SetText(FText::FromString(TEXT("시설 자동 개장 · 폐쇄 테스트")));
		StyleText(*Title, 22, FLinearColor(0.25f, 0.78f, 1.0f, 1.0f));
		Guide->SetText(FText::FromString(
			TEXT("Setup/MorningCheck에서 시설 설치 → Live 자동 개장 → Closing 자동 폐쇄")));
		StyleText(*Guide, 13, FLinearColor(0.72f, 0.78f, 0.86f, 1.0f));
		Stage->SetText(FText::FromString(TEXT("Festival Stage: -")));
		PhaseRevision->SetText(FText::FromString(TEXT("FFZS Phase: - | Revision: -")));
		FacilityCounts->SetText(FText::FromString(TEXT("시설 상태를 불러오는 중...")));
		QueueCounts->SetText(FText::FromString(TEXT("Queue 상태를 불러오는 중...")));
		LastResult->SetText(FText::FromString(TEXT("마지막 결과 없음")));
		StyleText(*Stage, 15, FLinearColor(1.0f, 0.87f, 0.38f, 1.0f));
		StyleText(*PhaseRevision, 14, FLinearColor(0.88f, 0.91f, 0.96f, 1.0f));
		StyleText(*FacilityCounts, 13, FLinearColor(0.88f, 0.91f, 0.96f, 1.0f));
		StyleText(*QueueCounts, 13, FLinearColor(0.88f, 0.91f, 0.96f, 1.0f));
		StyleText(*LastResult, 13, FLinearColor(0.45f, 1.0f, 0.62f, 1.0f));

		auto AddText = [Content](UCommonTextBlock* Text, const FMargin Padding)
		{
			if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(Text))
			{
				Slot->SetPadding(Padding);
				Slot->SetHorizontalAlignment(HAlign_Fill);
			}
		};
		AddText(Title, FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		AddText(Guide, FMargin(0.0f, 0.0f, 0.0f, 12.0f));
		AddText(Stage, FMargin(0.0f, 0.0f, 0.0f, 3.0f));
		AddText(PhaseRevision, FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		AddText(FacilityCounts, FMargin(0.0f, 0.0f, 0.0f, 3.0f));
		AddText(QueueCounts, FMargin(0.0f, 0.0f, 0.0f, 12.0f));

		UHorizontalBox* StageRow = MakeWidget<UHorizontalBox>(
			Blueprint, Tree, TEXT("Stage_Button_Row"));
		UHorizontalBox* LifecycleRow = MakeWidget<UHorizontalBox>(
			Blueprint, Tree, TEXT("Lifecycle_Button_Row"));
		UHorizontalBox* UtilityRow = MakeWidget<UHorizontalBox>(
			Blueprint, Tree, TEXT("Utility_Button_Row"));
		if (!StageRow || !LifecycleRow || !UtilityRow
			|| !AddButtonRow(*StageRow, {
				MakeButton(Blueprint, Tree, TEXT("BTN_Setup"), TEXT("Setup")),
				MakeButton(Blueprint, Tree, TEXT("BTN_MorningCheck"), TEXT("Morning")),
				MakeButton(Blueprint, Tree, TEXT("BTN_Live"), TEXT("Live")),
				MakeButton(Blueprint, Tree, TEXT("BTN_Closing"), TEXT("Closing"))})
			|| !AddButtonRow(*LifecycleRow, {
				MakeButton(Blueprint, Tree, TEXT("BTN_OpenAll"), TEXT("전체 개장")),
				MakeButton(Blueprint, Tree, TEXT("BTN_CloseAll"), TEXT("정상 폐쇄"))})
			|| !AddButtonRow(*UtilityRow, {
				MakeButton(Blueprint, Tree, TEXT("BTN_EmergencyCloseAll"), TEXT("긴급 폐쇄")),
				MakeButton(Blueprint, Tree, TEXT("BTN_Refresh"), TEXT("새로고침"))}))
		{
			return false;
		}

		auto AddRow = [Content](UHorizontalBox* Row)
		{
			if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(Row))
			{
				Slot->SetPadding(FMargin(0.0f, 2.0f));
				Slot->SetHorizontalAlignment(HAlign_Fill);
			}
		};
		AddRow(StageRow);
		AddRow(LifecycleRow);
		AddRow(UtilityRow);
		AddText(LastResult, FMargin(0.0f, 10.0f, 0.0f, 0.0f));

		Background->AddChild(Content);
		PanelSize->AddChild(Background);
		if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(PanelSize))
		{
			PanelSlot->SetAnchors(FAnchors(1.0f, 0.0f));
			PanelSlot->SetAlignment(FVector2D(1.0f, 0.0f));
			PanelSlot->SetPosition(FVector2D(-24.0f, 92.0f));
			PanelSlot->SetAutoSize(true);
		}
		Tree.RootWidget = Root;
		return true;
	}

	UWidgetBlueprint* LoadExistingBlueprint()
	{
		const FString AssetName = FPackageName::GetLongPackageAssetName(PanelPackage);
		return LoadObject<UWidgetBlueprint>(
			nullptr, *FString::Printf(TEXT("%s.%s"), *PanelPackage, *AssetName));
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

	void BuildZoneLifecycleTestPanel()
	{
		if (FPackageName::DoesPackageExist(PanelPackage))
		{
			UWidgetBlueprint* Existing = LoadExistingBlueprint();
			if (Existing && Existing->GeneratedClass
				&& Existing->GeneratedClass->IsChildOf(
					UFF_ZoneLifecycleTestPanel::StaticClass()))
			{
				UE_LOG(LogFFZoneLifecycleTestWbpBuilder, Display,
					TEXT("FF_ZONE_LIFECYCLE_TEST_UMG_REUSE: %s"), *PanelPackage);
			}
			else
			{
				UE_LOG(LogFFZoneLifecycleTestWbpBuilder, Error,
					TEXT("FF_ZONE_LIFECYCLE_TEST_UMG_FAIL: invalid existing parent: %s"),
					*PanelPackage);
			}
			return;
		}

		UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
		Factory->ParentClass = UFF_ZoneLifecycleTestPanel::StaticClass();
		FAssetToolsModule& AssetTools =
			FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		const FString AssetName = FPackageName::GetLongPackageAssetName(PanelPackage);
		const FString PackagePath = FPackageName::GetLongPackagePath(PanelPackage);
		UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(AssetTools.Get().CreateAsset(
			AssetName,
			PackagePath,
			UWidgetBlueprint::StaticClass(),
			Factory));
		if (!Blueprint || !Blueprint->WidgetTree
			|| !BuildLayout(*Blueprint, *Blueprint->WidgetTree)
			|| !SaveNewBlueprint(*Blueprint))
		{
			UE_LOG(LogFFZoneLifecycleTestWbpBuilder, Error,
				TEXT("FF_ZONE_LIFECYCLE_TEST_UMG_FAIL: could not create %s"),
				*PanelPackage);
			return;
		}

		UE_LOG(LogFFZoneLifecycleTestWbpBuilder, Display,
			TEXT("FF_ZONE_LIFECYCLE_TEST_UMG_CREATED: %s"), *PanelPackage);
	}

	FAutoConsoleCommand BuildZoneLifecycleTestPanelCommand(
		TEXT("FF.ZoneLifecycleTestUMG"),
		TEXT("Creates the editable Test_Main facility lifecycle test panel."),
		FConsoleCommandDelegate::CreateStatic(&BuildZoneLifecycleTestPanel));
}

#endif // WITH_EDITOR

