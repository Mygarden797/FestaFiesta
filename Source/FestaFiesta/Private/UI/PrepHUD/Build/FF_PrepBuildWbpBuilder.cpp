// One-shot, fail-safe WBP scaffolding for the PrepHUD LightTeal build panel.

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Building/Presentation/FF_BuildUIAssetPaths.h"

#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/PrimaryAssetLabel.h"
#include "Factories/DataAssetFactory.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "UObject/SavePackage.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"
#include "UI/PrepHUD/Panels/FF_PrepChoiceButton.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPrepBuildWbpBuilder"

namespace FFPrepBuildWbpBuilder
{
	using namespace FFUIWbpBuilder;

	constexpr const TCHAR* LogPrefix = TEXT("FF_PREPBUILD_UMG");
	const FString BuildFolder = FFBuildUIAssetPaths::CatalogFolder;
	const FString PanelFolder = BuildFolder;
	const FString TilePackage = BuildFolder / TEXT("WBP_FF_PrepBuildTile");
	const FString PanelPackage = PanelFolder / TEXT("WBP_FF_PrepPanelBuild");
	const FString CookLabelPackage = BuildFolder / TEXT("PAL_FF_PrepBuildUI");
	const FString ChoiceButtonPackage =
		FString(FFBuildUIAssetPaths::SharedFolder) / TEXT("WBP_FF_PrepChoiceButton");
	const FString CategoryDependencyCookLabelPackage =
		BuildFolder / TEXT("PAL_FF_PrepBuildCategoryDependencies");

	FSlateBrush MakeSolidBrush(const FLinearColor& Color, const float Radius)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Color);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		return Brush;
	}

	FButtonStyle MakeButtonStyle(
		const UFF_UIPalette& Palette,
		const bool bPrimary = false,
		const float Radius = 4.0f)
	{
		FButtonStyle Style;
		Style.Normal = MakeSolidBrush(
			bPrimary ? Palette.InGame.Teal : Palette.InGame.PaperSunken, Radius);
		Style.Hovered = MakeSolidBrush(
			bPrimary ? Palette.InGame.TealDeep : Palette.InGame.Hover, Radius);
		Style.Pressed = MakeSolidBrush(Palette.InGame.TealDeep, Radius);
		Style.Disabled = MakeSolidBrush(Palette.InGame.PaperSunken, Radius);
		Style.NormalPadding = FMargin(10.0f, 5.0f);
		Style.PressedPadding = FMargin(10.0f, 5.0f);
		return Style;
	}

	UButton* MakeTextButton(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const UFF_UIPalette& Palette,
		const FName ButtonName,
		const FName LabelName,
		const FText& LabelText,
		const bool bPrimary = false)
	{
		UButton* Button = MakeWidget<UButton>(Blueprint, Tree, ButtonName, true);
		UTextBlock* Label = MakeWidget<UTextBlock>(Blueprint, Tree, LabelName);
		if (!Button || !Label)
		{
			return nullptr;
		}

		Button->SetStyle(MakeButtonStyle(Palette, bPrimary));
		StyleText(*Label, 12, bPrimary ? Palette.InGame.White : Palette.InGame.Ink, TEXT("Medium"));
		Label->SetText(LabelText);
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Button->AddChild(Label)))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		return Button;
	}

	UHorizontalBox* MakePanelHeader(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const UFF_UIPalette& Palette)
	{
		// ★ `Box_Header` 로 노출한다. 배치 중에 패널이 이 줄을 접는다
		//   (`UFF_PrepPanelBuild::ApplyBuildModeState`).
		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Box_Header"), true);
		UTextBlock* Title = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_PanelTitle"), true);
		UTextBlock* Gold = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Gold"), true);
		UButton* Close = MakeTextButton(
			Blueprint, Tree, Palette,
			TEXT("Button_Close"), TEXT("Text_CloseLabel"),
			LOCTEXT("Close", "\u00D7"));
		if (!Header || !Title || !Gold || !Close)
		{
			return nullptr;
		}

		StyleText(*Title, 22, Palette.InGame.Ink, TEXT("SemiBold"));
		Title->SetText(LOCTEXT("PanelTitle", "시설 배치"));
		if (UHorizontalBoxSlot* Slot = Header->AddChildToHorizontalBox(Title))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		StyleText(*Gold, 13, Palette.InGame.TealDeep, TEXT("SemiBold"));
		Gold->SetText(LOCTEXT("GoldPreview", "보유 7,500 Gold"));
		if (UHorizontalBoxSlot* Slot = Header->AddChildToHorizontalBox(Gold))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
		}

		if (UHorizontalBoxSlot* Slot = Header->AddChildToHorizontalBox(Close))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		return Header;
	}

	bool BuildTileLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			return false;
		}

		UBorder* Root = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Card"), true);
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Content"));
		USizeBox* ThumbnailSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Thumbnail"));
		UOverlay* ThumbnailStack = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Thumbnail"));
		UBorder* ThumbnailBackdrop = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_ThumbnailBackdrop"));
		UImage* Thumbnail = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Thumbnail"), true);
		UTextBlock* Name = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Name"), true);
		UHorizontalBox* Meta = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Meta"));
		UTextBlock* Cost = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Cost"), true);
		UTextBlock* State = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_State"), true);
		if (!Root || !Content || !ThumbnailSize || !ThumbnailStack || !ThumbnailBackdrop ||
			!Thumbnail || !Name || !Meta || !Cost || !State)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetBrush(MakeSolidBrush(Palette->InGame.Paper, 6.0f));
		Root->SetPadding(FMargin(10.0f));
		Root->SetClipping(EWidgetClipping::ClipToBoundsAlways);

		ThumbnailSize->SetHeightOverride(94.0f);
		ThumbnailBackdrop->SetBrush(MakeSolidBrush(Palette->InGame.StatusBar, 5.0f));
		if (UOverlaySlot* Slot = ThumbnailStack->AddChildToOverlay(ThumbnailBackdrop))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
		Thumbnail->SetColorAndOpacity(FLinearColor::White);
		if (UOverlaySlot* Slot = ThumbnailStack->AddChildToOverlay(Thumbnail))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
		if (USizeBoxSlot* Slot = Cast<USizeBoxSlot>(ThumbnailSize->AddChild(ThumbnailStack)))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
		if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(ThumbnailSize))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}

		StyleText(*Name, 14, Palette->InGame.Ink, TEXT("SemiBold"));
		Name->SetText(LOCTEXT("TileName", "푸드트럭"));
		Name->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(Name))
		{
			Slot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 2.0f));
		}

		StyleText(*Cost, 12, Palette->InGame.TealDeep, TEXT("SemiBold"));
		Cost->SetText(LOCTEXT("TileCost", "1,200 Gold"));
		if (UHorizontalBoxSlot* Slot = Meta->AddChildToHorizontalBox(Cost))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		StyleText(*State, 11, Palette->InGame.TealDeep, TEXT("Medium"));
		State->SetText(LOCTEXT("TileAvailable", "배치 가능"));
		if (UHorizontalBoxSlot* Slot = Meta->AddChildToHorizontalBox(State))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(Meta))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}

		if (UBorderSlot* Slot = Cast<UBorderSlot>(Root->AddChild(Content)))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
		return true;
	}

	bool BuildPanelLayout(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		TSubclassOf<UUserWidget> TileEntryClass)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette || !TileEntryClass)
		{
			return false;
		}

		// ★ 변수로 노출한다. 배치 중에는 상하 여백(14)을 0 으로 줄여야 탭 줄 높이에 맞는다.
		UBorder* Root = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_PanelRoot"), true);
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Content"));
		UHorizontalBox* Header = MakePanelHeader(Blueprint, Tree, *Palette);
		UVerticalBox* CatalogPanel = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Panel_Catalog"), true);
		UHorizontalBox* Tools = MakeWidget<UHorizontalBox>(
			Blueprint, Tree, TEXT("HorizontalBox_Tools"), true);
		UButton* All = MakeTextButton(
			Blueprint, Tree, *Palette, TEXT("Button_FilterAll"), TEXT("Text_FilterAll"), LOCTEXT("FilterAll", "전체"), true);
		UButton* Booth = MakeTextButton(
			Blueprint, Tree, *Palette, TEXT("Button_FilterBooth"), TEXT("Text_FilterBooth"), LOCTEXT("FilterBooth", "부스"));
		UButton* Performance = MakeTextButton(
			Blueprint, Tree, *Palette, TEXT("Button_FilterPerformance"), TEXT("Text_FilterPerformance"), LOCTEXT("FilterPerformance", "공연"));
		UEditableTextBox* Search = MakeWidget<UEditableTextBox>(Blueprint, Tree, TEXT("Search_Facilities"), true);
		UFF_PrepBuildTileView* TileView = MakeWidget<UFF_PrepBuildTileView>(
			Blueprint, Tree, TEXT("TileView_Facilities"), true);
		UHorizontalBox* Footer = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Footer"));
		UTextBlock* Status = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Status"), true);
		UTextBlock* Count = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_ItemCount"), true);
		UBorder* Compact = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Panel_PlacementCompact"), true);
		UHorizontalBox* CompactRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_PlacementCompact"));
		UTextBlock* Guide = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_PlacementGuide"), true);
		UButton* Return = MakeTextButton(
			Blueprint, Tree, *Palette, TEXT("Button_ReturnToCatalog"), TEXT("Text_ReturnToCatalog"), LOCTEXT("Return", "목록으로"), true);
		UButton* Exit = MakeTextButton(
			Blueprint, Tree, *Palette, TEXT("Button_ExitBuildMode"), TEXT("Text_ExitBuildMode"), LOCTEXT("Exit", "배치 종료"));

		if (!Root || !Content || !Header || !CatalogPanel || !Tools || !All || !Booth || !Performance ||
			!Search || !TileView || !Footer || !Status || !Count || !Compact || !CompactRow || !Guide ||
			!Return || !Exit)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetBrush(MakeSolidBrush(Palette->InGame.Paper, 8.0f));
		Root->SetPadding(FMargin(18.0f, 14.0f));
		if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(Header))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
		}

		for (UButton* FilterButton : { All, Booth, Performance })
		{
			if (UHorizontalBoxSlot* Slot = Tools->AddChildToHorizontalBox(FilterButton))
			{
				Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				Slot->SetVerticalAlignment(VAlign_Center);
				Slot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
			}
		}
		Search->SetHintText(LOCTEXT("SearchHint", "시설 이름 검색"));
		Search->SetMinDesiredWidth(220.0f);
		FEditableTextBoxStyle SearchStyle = Search->GetWidgetStyle();
		SearchStyle
			.SetBackgroundImageNormal(MakeSolidBrush(Palette->InGame.PaperSunken, 4.0f))
			.SetBackgroundImageHovered(MakeSolidBrush(Palette->InGame.Hover, 4.0f))
			.SetBackgroundImageFocused(MakeSolidBrush(Palette->InGame.Hover, 4.0f))
			.SetBackgroundImageReadOnly(MakeSolidBrush(Palette->InGame.PaperSunken, 4.0f))
			.SetForegroundColor(FSlateColor(Palette->InGame.Ink))
			.SetReadOnlyForegroundColor(FSlateColor(Palette->InGame.InkFaint))
			.SetFocusedForegroundColor(FSlateColor(Palette->InGame.Ink))
			.SetPadding(FMargin(10.0f, 6.0f));
		SearchStyle.TextStyle.Font.Size = 12;
		Search->SetWidgetStyle(SearchStyle);
		if (UHorizontalBoxSlot* Slot = Tools->AddChildToHorizontalBox(Search))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		if (UVerticalBoxSlot* Slot = CatalogPanel->AddChildToVerticalBox(Tools))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
		}

		TileView->ConfigureEntryClass(TileEntryClass);
		TileView->SetScrollbarVisibility(ESlateVisibility::Collapsed);
		if (UVerticalBoxSlot* Slot = CatalogPanel->AddChildToVerticalBox(TileView))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}

		StyleText(*Status, 12, Palette->InGame.InkMuted, TEXT("Medium"));
		Status->SetText(LOCTEXT("StatusPreview", "시설을 선택하면 월드 배치 모드로 전환됩니다."));
		Status->SetAutoWrapText(true);
		if (UHorizontalBoxSlot* Slot = Footer->AddChildToHorizontalBox(Status))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		StyleText(*Count, 12, Palette->InGame.InkMuted, TEXT("Medium"));
		Count->SetText(LOCTEXT("CountPreview", "시설 8 / 8"));
		if (UHorizontalBoxSlot* Slot = Footer->AddChildToHorizontalBox(Count))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		if (UVerticalBoxSlot* Slot = CatalogPanel->AddChildToVerticalBox(Footer))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
		}
		if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(CatalogPanel))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}

		Compact->SetBrush(MakeSolidBrush(Palette->InGame.StatusBar, 6.0f));
		Compact->SetPadding(FMargin(12.0f, 8.0f));
		Compact->SetVisibility(ESlateVisibility::Collapsed);
		StyleText(*Guide, 13, Palette->InGame.Ink, TEXT("Medium"));
		Guide->SetText(LOCTEXT("GuidePreview", "배치 중 · 좌클릭 설치 · Q/E 회전 · 우클릭 목록 복귀"));
		if (UHorizontalBoxSlot* Slot = CompactRow->AddChildToHorizontalBox(Guide))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		for (UButton* ActionButton : { Return, Exit })
		{
			if (UHorizontalBoxSlot* Slot = CompactRow->AddChildToHorizontalBox(ActionButton))
			{
				Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				Slot->SetVerticalAlignment(VAlign_Center);
				Slot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
			}
		}
		if (UBorderSlot* Slot = Cast<UBorderSlot>(Compact->AddChild(CompactRow)))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Center);

			// 🔴 **아래 여백 22 는 실기에서 맞춘 값이다** (2026-09-03 백종태).
			//   에디터 디자이너에서는 아래가 붕 떠 보이지만, 런타임에는 이 띠가 36px 로
			//   줄고 바 위에 얹히므로 이 여백이 있어야 글자가 탭 줄과 같은 높이에 온다.
			//   **에디터에서 예뻐 보이게 고치면 실기가 어긋난다.**
			Slot->SetPadding(FMargin(12.0f, 8.0f, 12.0f, 22.0f));
		}
		if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(Compact))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}

		if (UBorderSlot* Slot = Cast<UBorderSlot>(Root->AddChild(Content)))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
		return true;
	}

	UWidgetBlueprint* FindLoadedBlueprint(const FString& PackageName)
	{
		UPackage* Package = FindPackage(nullptr, *PackageName);
		return Package
			? FindObject<UWidgetBlueprint>(Package, *FPackageName::GetLongPackageAssetName(PackageName))
			: nullptr;
	}

	bool HasRequiredWidgets(
		UWidgetBlueprint& Blueprint,
		const TConstArrayView<FName> RequiredNames)
	{
		if (!Blueprint.WidgetTree)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: %s에 WidgetTree가 없다."), LogPrefix, *Blueprint.GetPathName());
			return false;
		}

		for (const FName RequiredName : RequiredNames)
		{
			if (!Blueprint.WidgetTree->FindWidget(RequiredName))
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_FAIL: %s에 필수 위젯 '%s'가 없다. 기존 에셋은 수정하지 않았다."),
					LogPrefix, *Blueprint.GetPathName(), *RequiredName.ToString());
				return false;
			}
		}
		return true;
	}

	bool ValidateTileBlueprint(UWidgetBlueprint& Blueprint)
	{
		if (!Blueprint.GeneratedClass ||
			!Blueprint.GeneratedClass->IsChildOf(UFF_PrepBuildTileWidget::StaticClass()))
		{
			return false;
		}

		const FName RequiredNames[] = {
			TEXT("Border_Card"),
			TEXT("Image_Thumbnail"),
			TEXT("Text_Name"),
			TEXT("Text_Cost"),
			TEXT("Text_State")
		};
		return HasRequiredWidgets(Blueprint, RequiredNames);
	}

	bool ValidatePanelBlueprint(UWidgetBlueprint& Blueprint, UClass* TileEntryClass)
	{
		if (!Blueprint.GeneratedClass ||
			!Blueprint.GeneratedClass->IsChildOf(UFF_PrepPanelBuild::StaticClass()))
		{
			return false;
		}

		const FName RequiredNames[] = {
			TEXT("Text_PanelTitle"),
			TEXT("Button_Close"),
			TEXT("Panel_Catalog"),
			TEXT("Panel_PlacementCompact"),
			TEXT("HorizontalBox_Tools"),
			TEXT("TileView_Facilities"),
			TEXT("Search_Facilities"),
			TEXT("Text_Gold"),
			TEXT("Text_Status"),
			TEXT("Text_ItemCount"),
			TEXT("Text_PlacementGuide"),
			TEXT("Button_ReturnToCatalog"),
			TEXT("Button_ExitBuildMode"),
			TEXT("Button_FilterAll"),
			TEXT("Button_FilterBooth"),
			TEXT("Button_FilterPerformance")
		};
		if (!HasRequiredWidgets(Blueprint, RequiredNames))
		{
			return false;
		}

		const UFF_PrepBuildTileView* TileView = Cast<UFF_PrepBuildTileView>(
			Blueprint.WidgetTree->FindWidget(TEXT("TileView_Facilities")));
		if (!TileView || TileView->GetEntryWidgetClass() != TileEntryClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: %s의 TileView entry class 계약이 다르다. 기존 에셋은 수정하지 않았다."),
				LogPrefix, *Blueprint.GetPathName());
			return false;
		}
		return true;
	}

	bool ValidateChoiceButtonBlueprint(UWidgetBlueprint& Blueprint)
	{
		if (!Blueprint.GeneratedClass ||
			!Blueprint.GeneratedClass->IsChildOf(UFF_PrepChoiceButton::StaticClass()))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: %s가 PrepChoiceButton을 상속하지 않는다. 기존 에셋은 수정하지 않았다."),
				LogPrefix, *Blueprint.GetPathName());
			return false;
		}

		const FName RequiredNames[] = {
			TEXT("Button_Root"),
			TEXT("Text_Label")
		};
		return HasRequiredWidgets(Blueprint, RequiredNames);
	}

	bool ValidateCookLabel(const UPrimaryAssetLabel& Label)
	{
		const FSoftObjectPath TilePath(MakeObjectPath(TilePackage));
		const FSoftObjectPath PanelPath(MakeObjectPath(PanelPackage));
		const auto ContainsPath = [&Label](const FSoftObjectPath& Expected)
		{
			return Label.ExplicitAssets.ContainsByPredicate(
				[&Expected](const TSoftObjectPtr<UObject>& Asset)
				{
					return Asset.ToSoftObjectPath() == Expected;
				});
		};

		return Label.bIsRuntimeLabel &&
			!Label.bLabelAssetsInMyDirectory &&
			Label.Rules.CookRule == EPrimaryAssetCookRule::AlwaysCook &&
			Label.ExplicitAssets.Num() == 2 &&
			ContainsPath(TilePath) && ContainsPath(PanelPath);
	}

	bool ValidateCategoryDependencyCookLabel(const UPrimaryAssetLabel& Label)
	{
		const FSoftObjectPath ChoiceButtonPath(MakeObjectPath(ChoiceButtonPackage));
		return Label.bIsRuntimeLabel &&
			!Label.bLabelAssetsInMyDirectory &&
			Label.Rules.CookRule == EPrimaryAssetCookRule::AlwaysCook &&
			Label.ExplicitAssets.Num() == 1 &&
			Label.ExplicitAssets.ContainsByPredicate(
				[&ChoiceButtonPath](const TSoftObjectPtr<UObject>& Asset)
				{
					return Asset.ToSoftObjectPath() == ChoiceButtonPath;
				});
	}

	UWidgetBlueprint* EnsureFailSafeBlueprint(
		const FString& PackageName,
		const FString& Folder,
		UClass& ParentClass,
		FBuildLayout&& BuildLayout)
	{
		// An unsaved in-memory package is also team work. Never replace it just because no file exists yet.
		if (FindPackage(nullptr, *PackageName))
		{
			UWidgetBlueprint* Existing = FindLoadedBlueprint(PackageName);
			if (Existing)
			{
				UE_LOG(LogFFUIWbpBuilder, Display,
					TEXT("%s_REUSE: %s 메모리 패키지가 이미 있어 수정하지 않았다."),
					LogPrefix, *PackageName);
			}
			else
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_FAIL: %s 메모리 패키지가 이미 있지만 WBP가 아니다. 수정하지 않았다."),
					LogPrefix, *PackageName);
			}
			return Existing;
		}

		return EnsureWidgetBlueprint(
			LogPrefix, PackageName, Folder, ParentClass, MoveTemp(BuildLayout));
	}

	bool CreateCookLabelIfMissing()
	{
		if (FPackageName::DoesPackageExist(CookLabelPackage) || FindPackage(nullptr, *CookLabelPackage))
		{
			const UPrimaryAssetLabel* Existing = LoadObject<UPrimaryAssetLabel>(
				nullptr, *MakeObjectPath(CookLabelPackage));
			if (!Existing || !ValidateCookLabel(*Existing))
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_FAIL: %s가 있지만 Runtime/AlwaysCook/ExplicitAssets 계약이 다르다. "
						"기존 에셋은 수정하지 않았다."),
					LogPrefix, *CookLabelPackage);
				return false;
			}
			UE_LOG(LogFFUIWbpBuilder, Display,
				TEXT("%s_REUSE: %s 계약이 유효해 수정하지 않았다."),
				LogPrefix, *CookLabelPackage);
			return true;
		}

		UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
		Factory->DataAssetClass = UPrimaryAssetLabel::StaticClass();
		FAssetToolsModule& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		UPrimaryAssetLabel* Label = Cast<UPrimaryAssetLabel>(AssetTools.Get().CreateAsset(
			FPackageName::GetLongPackageAssetName(CookLabelPackage),
			BuildFolder,
			UPrimaryAssetLabel::StaticClass(),
			Factory));
		if (!Label)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: %s 생성 실패."), LogPrefix, *CookLabelPackage);
			return false;
		}

		Label->bIsRuntimeLabel = true;
		Label->bLabelAssetsInMyDirectory = false;
		Label->Rules.CookRule = EPrimaryAssetCookRule::AlwaysCook;
		Label->Rules.bApplyRecursively = true;
		Label->ExplicitAssets = {
			TSoftObjectPtr<UObject>(FSoftObjectPath(MakeObjectPath(TilePackage))),
			TSoftObjectPtr<UObject>(FSoftObjectPath(MakeObjectPath(PanelPackage)))
		};
		Label->UpdateAssetBundleData();
		Label->MarkPackageDirty();

		UPackage* Package = Label->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			CookLabelPackage, FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		const bool bSaved = UPackage::SavePackage(Package, Label, *Filename, Args);
		if (bSaved)
		{
			UE_LOG(LogFFUIWbpBuilder, Display,
				TEXT("%s_CREATE: %s"), LogPrefix, *CookLabelPackage);
		}
		else
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: %s 저장 실패."), LogPrefix, *CookLabelPackage);
		}
		return bSaved;
	}

	bool CreateCategoryDependencyCookLabelIfMissing()
	{
		if (FPackageName::DoesPackageExist(CategoryDependencyCookLabelPackage) ||
			FindPackage(nullptr, *CategoryDependencyCookLabelPackage))
		{
			const UPrimaryAssetLabel* Existing = LoadObject<UPrimaryAssetLabel>(
				nullptr, *MakeObjectPath(CategoryDependencyCookLabelPackage));
			if (!Existing || !ValidateCategoryDependencyCookLabel(*Existing))
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_FAIL: %s가 있지만 ChoiceButton AlwaysCook 계약이 다르다. "
						"기존 에셋은 수정하지 않았다."),
					LogPrefix, *CategoryDependencyCookLabelPackage);
				return false;
			}
			UE_LOG(LogFFUIWbpBuilder, Display,
				TEXT("%s_REUSE: %s 계약이 유효해 수정하지 않았다."),
				LogPrefix, *CategoryDependencyCookLabelPackage);
			return true;
		}

		UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
		Factory->DataAssetClass = UPrimaryAssetLabel::StaticClass();
		FAssetToolsModule& AssetTools =
			FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		UPrimaryAssetLabel* Label = Cast<UPrimaryAssetLabel>(AssetTools.Get().CreateAsset(
			FPackageName::GetLongPackageAssetName(CategoryDependencyCookLabelPackage),
			BuildFolder,
			UPrimaryAssetLabel::StaticClass(),
			Factory));
		if (!Label)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: %s 생성 실패."),
				LogPrefix, *CategoryDependencyCookLabelPackage);
			return false;
		}

		Label->bIsRuntimeLabel = true;
		Label->bLabelAssetsInMyDirectory = false;
		Label->Rules.CookRule = EPrimaryAssetCookRule::AlwaysCook;
		Label->Rules.bApplyRecursively = false;
		Label->ExplicitAssets = {
			TSoftObjectPtr<UObject>(FSoftObjectPath(MakeObjectPath(ChoiceButtonPackage)))
		};
		Label->UpdateAssetBundleData();
		Label->MarkPackageDirty();

		UPackage* Package = Label->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			CategoryDependencyCookLabelPackage, FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		const bool bSaved = UPackage::SavePackage(Package, Label, *Filename, Args);
		if (bSaved)
		{
			UE_LOG(LogFFUIWbpBuilder, Display,
				TEXT("%s_CREATE: %s"), LogPrefix, *CategoryDependencyCookLabelPackage);
		}
		else
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: %s 저장 실패."), LogPrefix, *CategoryDependencyCookLabelPackage);
		}
		return bSaved;
	}

	void BuildPrepBuildUmgAssets()
	{
		UWidgetBlueprint* TileBlueprint = EnsureFailSafeBlueprint(
			TilePackage,
			BuildFolder,
			*UFF_PrepBuildTileWidget::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildTileLayout(Blueprint, Tree);
			});
		if (!TileBlueprint || !ValidateTileBlueprint(*TileBlueprint))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: Tile WBP 계약이 유효하지 않아 Panel WBP를 만들지 않았다."), LogPrefix);
			return;
		}

		const TSubclassOf<UUserWidget> TileEntryClass(TileBlueprint->GeneratedClass.Get());
		UWidgetBlueprint* PanelBlueprint = EnsureFailSafeBlueprint(
			PanelPackage,
			PanelFolder,
			*UFF_PrepPanelBuild::StaticClass(),
			[TileEntryClass](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildPanelLayout(Blueprint, Tree, TileEntryClass);
			});
		if (!PanelBlueprint || !ValidatePanelBlueprint(*PanelBlueprint, TileEntryClass.Get()))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: Panel WBP 계약이 유효하지 않아 Cook Label을 만들지 않았다."), LogPrefix);
			return;
		}

		UWidgetBlueprint* ChoiceButtonBlueprint = LoadObject<UWidgetBlueprint>(
			nullptr, *MakeObjectPath(ChoiceButtonPackage));
		if (!ChoiceButtonBlueprint || !ValidateChoiceButtonBlueprint(*ChoiceButtonBlueprint))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 공용 ChoiceButton 계약이 유효하지 않아 카테고리 의존성 label을 "
					"만들지 않았다. 기존 에셋은 수정하지 않았다."), LogPrefix);
			return;
		}

		if (CreateCookLabelIfMissing() && CreateCategoryDependencyCookLabelIfMissing())
		{
			UE_LOG(LogFFUIWbpBuilder, Display,
				TEXT("%s_DONE: 신규 WBP 2종과 런타임 의존성 AlwaysCook label이 준비됐다."),
				LogPrefix);
		}
	}

	// A targeted migration of the saved catalog, not a call to the historical full-tree scaffold.
	// The outer border, its slots, compact placement strip, TileView class and all bindings survive.
	void ApplyBuildCatalogV2Once()
	{
		constexpr const TCHAR* VersionKey = TEXT("FF.BuildCatalog.DesignVersion");
		constexpr const TCHAR* Version = TEXT("20260919.v2");
		constexpr const TCHAR* Prefix = TEXT("FF_BUILD_CATALOG_V2");
		UWidgetBlueprint* Blueprint = LoadExistingBlueprint(PanelPackage);
		if (!Blueprint || !Blueprint->WidgetTree || !Blueprint->GeneratedClass ||
			!Blueprint->GeneratedClass->IsChildOf(UFF_PrepPanelBuild::StaticClass()))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_ABORT: saved build panel or expected parent is missing."), Prefix);
			return;
		}
		FMetaData& Metadata = Blueprint->GetOutermost()->GetMetaData();
		if (Metadata.GetValue(Blueprint, VersionKey) == Version)
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: saved Designer edits preserved; already migrated."), Prefix);
			return;
		}
		UWidgetTree& Tree = *Blueprint->WidgetTree;
		UWidget* HeaderSurface = Tree.FindWidget(TEXT("Box_Header"));
		UHorizontalBox* Header = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("HorizontalBox_CatalogHeader")));
		if (!Header) Header = Cast<UHorizontalBox>(HeaderSurface);
		UWidget* Ancestor = Header;
		while (Ancestor && Ancestor != HeaderSurface) Ancestor = Ancestor->GetParent();
		UEditableTextBox* Search = Cast<UEditableTextBox>(Tree.FindWidget(TEXT("Search_Facilities")));
		UButton* Close = Cast<UButton>(Tree.FindWidget(TEXT("Button_Close")));
		UWidget* ExistingRanking = Tree.FindWidget(TEXT("Button_RevenueRanking"));
		if (!HeaderSurface || !Header || !Ancestor || !Search || !Close ||
			!Tree.FindWidget(TEXT("TileView_Facilities")) ||
			(ExistingRanking && !ExistingRanking->IsA<UButton>()))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_ABORT: catalog header contract differs; nothing was changed."), Prefix);
			return;
		}

		const auto Color = [](const TCHAR* Hex) { return FLinearColor::FromSRGBColor(FColor::FromHex(Hex)); };
		const FLinearColor Paper = Color(TEXT("F5F8F5"));
		const FLinearColor Card = Color(TEXT("EBF2ED"));
		const FLinearColor Green = Color(TEXT("315F50"));
		const FLinearColor GreenHover = Color(TEXT("3D7462"));
		const FLinearColor Ink = Color(TEXT("183D35"));
		const FLinearColor Muted = Color(TEXT("70877E"));
		const FLinearColor Line = Color(TEXT("D7E3DC"));
		const FLinearColor Hover = Color(TEXT("E1F0E8"));
		const auto Brush = [&Line](const FLinearColor& Fill)
		{
			FSlateBrush Result = MakeSolidBrush(Fill, 5.0f);
			Result.OutlineSettings.Color = FSlateColor(Line);
			Result.OutlineSettings.Width = 1.0f;
			return Result;
		};
		const auto RestyleButton = [&Brush, &Green, &GreenHover, &Paper, &Card, &Hover](UButton* Button, const bool bPrimary)
		{
			if (!Button) return;
			FButtonStyle Style = Button->GetStyle();
			Style.Normal = Brush(bPrimary ? Green : Paper);
			Style.Hovered = Brush(bPrimary ? GreenHover : Hover);
			Style.Pressed = Brush(bPrimary ? Green : Hover);
			Style.Disabled = Brush(Card);
			// Keep authored padding and input behavior; only the catalog button brushes change.
			Button->SetStyle(Style);
		};
		const auto Text = [&Tree](const TCHAR* Name, const int32 Size, const FLinearColor& Tint, const FName Weight)
		{
			if (UTextBlock* Widget = Cast<UTextBlock>(Tree.FindWidget(Name))) StyleText(*Widget, Size, Tint, Weight);
		};
		const auto Border = [&Tree, &Brush](const TCHAR* Name, const FLinearColor& Fill)
		{
			if (UBorder* Widget = Cast<UBorder>(Tree.FindWidget(Name))) Widget->SetBrush(Brush(Fill));
		};

		Blueprint->Modify();
		Tree.Modify();
		UButton* RankingButton = Cast<UButton>(ExistingRanking);
		if (!RankingButton)
		{
			RankingButton = MakeWidget<UButton>(*Blueprint, Tree, TEXT("Button_RevenueRanking"), true);
			UTextBlock* Label = MakeWidget<UTextBlock>(*Blueprint, Tree, TEXT("Text_RevenueRanking"));
			Label->SetText(LOCTEXT("RevenueRanking", "매출 순위"));
			StyleText(*Label, 13, Ink, TEXT("SemiBold"));
			RankingButton->SetToolTipText(LOCTEXT("RevenueRankingHint", "시설별 오늘·누적 매출을 비교합니다."));
			if (UButtonSlot* Slot = Cast<UButtonSlot>(RankingButton->AddChild(Label)))
			{
				Slot->SetPadding(FMargin(10.0f, 6.0f));
				Slot->SetHorizontalAlignment(HAlign_Center);
				Slot->SetVerticalAlignment(VAlign_Center);
			}
			UWidget* Before = Tree.FindWidget(TEXT("Widget_FacilityCodexCatalogLauncher"));
			int32 InsertIndex = Before ? Header->GetChildIndex(Before) : Header->GetChildIndex(Close);
			if (InsertIndex == INDEX_NONE) InsertIndex = Header->GetChildrenCount();
			if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(Header->InsertChildAt(InsertIndex, RankingButton)))
			{
				Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				Slot->SetVerticalAlignment(VAlign_Center);
				Slot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
			}
		}
		RestyleButton(RankingButton, false);
		for (const TCHAR* Name : { TEXT("Button_Close"), TEXT("Button_FavoritesOnly"), TEXT("Button_InspectFacilities"),
			TEXT("Button_PreviousPage"), TEXT("Button_NextPage"), TEXT("Button_ResetFilters"),
			TEXT("Button_FilterBooth"), TEXT("Button_FilterPerformance") })
		{
			RestyleButton(Cast<UButton>(Tree.FindWidget(Name)), false);
		}
		RestyleButton(Cast<UButton>(Tree.FindWidget(TEXT("Button_FilterAll"))), true);
		Text(TEXT("Text_PanelTitle"), 20, Green, TEXT("SemiBold"));
		Text(TEXT("Text_StudioHint"), 12, Muted, TEXT("Regular"));
		Text(TEXT("Text_Gold"), 14, Green, TEXT("SemiBold"));
		Text(TEXT("Text_CategoryBreadcrumb"), 13, Ink, TEXT("SemiBold"));
		for (const TCHAR* Name : { TEXT("Text_FavoritesOnly"), TEXT("Text_FavoriteCount"), TEXT("Text_ItemCount"),
			TEXT("Text_PageIndex"), TEXT("Text_InspectFacilities"), TEXT("Text_ResetFilters") })
		{
			Text(Name, 12, Ink, TEXT("Medium"));
		}
		Text(TEXT("Text_Status"), 12, Muted, TEXT("Regular"));
		Text(TEXT("Text_EmptyTitle"), 16, Ink, TEXT("SemiBold"));
		Text(TEXT("Text_EmptyHint"), 12, Muted, TEXT("Regular"));
		Border(TEXT("Border_BuildMark"), Card);
		Border(TEXT("Border_Gold"), Paper);
		Border(TEXT("Border_FavoriteCount"), Hover);
		Border(TEXT("Border_CatalogFooter"), Card);
		for (const TCHAR* Name : { TEXT("Border_HeaderDivider"), TEXT("Border_CategoryDivider") })
		{
			if (UBorder* Divider = Cast<UBorder>(Tree.FindWidget(Name))) Divider->SetBrushColor(Line);
		}
		FEditableTextBoxStyle SearchStyle = Search->GetWidgetStyle();
		SearchStyle.SetBackgroundImageNormal(Brush(Paper)).SetBackgroundImageHovered(Brush(Hover))
			.SetBackgroundImageFocused(Brush(Hover)).SetForegroundColor(FSlateColor(Ink))
			.SetFocusedForegroundColor(FSlateColor(Ink)).SetReadOnlyForegroundColor(FSlateColor(Muted));
		Search->SetWidgetStyle(SearchStyle);
		Search->SetHintText(LOCTEXT("CatalogV2SearchHint", "이름 · 종류 검색"));
		Metadata.SetValue(Blueprint, VersionKey, Version);
		if (!SaveNewBlueprint(Prefix, *Blueprint))
		{
			Metadata.RemoveValue(Blueprint, VersionKey);
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: migration was not saved."), Prefix);
			return;
		}
		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_SAVED: catalog interior and revenue launcher only; outer/compact/card/shared assets preserved."), Prefix);
	}

	FAutoConsoleCommand ApplyCatalogV2Command(
		TEXT("FF.ApplyBuildCatalogV2UMG"),
		TEXT("Migrate the saved build catalog interior once. Preserve outer layout, compact placement UI and later Designer edits."),
		FConsoleCommandDelegate::CreateStatic(&ApplyBuildCatalogV2Once));

	FAutoConsoleCommand PrepBuildUmgCommand(
		TEXT("FF.PrepBuildUMG"),
		TEXT("Creates the PrepHUD build tile, panel, and AlwaysCook label only when each target does not exist."),
		FConsoleCommandDelegate::CreateStatic(&BuildPrepBuildUmgAssets));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
