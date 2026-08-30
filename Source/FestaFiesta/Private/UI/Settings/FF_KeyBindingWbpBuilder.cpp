// Copyright FestaFiesta. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_EDITOR

// Editor-only, non-destructive scaffolder for the keyboard remapping panel.
// Run `FF.KeyBindingUMG` in the editor console. Existing texture/WBP assets are reused;
// only the explicitly listed keyboard mappings receive per-mapping mappable settings.

#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "EnhancedActionKeyMapping.h"
#include "Factories/TextureFactory.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PlayerMappableKeySettings.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "TextureCompiler.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Core/FF_PauseOptionsLightTealStyle.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/Settings/FF_KeyBindingLayoutMetrics.h"
#include "UI/Settings/FF_KeyBindingPanel.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "FestaFiestaKeyBindingWbpBuilder"

namespace FFKeyBindingWbpBuilder
{
	using namespace FFUIWbpBuilder;

	const TCHAR* const LogPrefix = TEXT("FF_KEY_BINDING_UMG");
	const FString SettingsFolder(TEXT("/Game/FestaFiesta/UI/Settings"));
	const FString TextureFolder(SettingsFolder / TEXT("Textures"));
	const FString TexturePackage(TextureFolder / TEXT("T_UI_Settings_Keyboard_ImageGen_v2"));
	const FString PanelPackage(SettingsFolder / TEXT("WBP_FF_KeyBindingPanel"));
	const FString TextureSource = FPaths::ProjectDir()
		/ TEXT("SourceAssets/UI/Settings/T_UI_Settings_Keyboard_ImageGen_v2.png");

	const TCHAR* const CameraContextPath =
		TEXT("/Game/FestaFiesta/Input/IMC_Camera.IMC_Camera");
	const TCHAR* const CharacterContextPath =
		TEXT("/Game/FestaFiesta/Input/Character/IMC_Character.IMC_Character");

	FSlateBrush MakeSolidBrush(const FLinearColor& Color, const float CornerRadius = 0.f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Color);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(
			CornerRadius, CornerRadius, CornerRadius, CornerRadius);
		return Brush;
	}

	FSlateBrush MakePanelBrush(const FLinearColor& Fill, const FLinearColor& Outline,
		const float CornerRadius = 4.f, const float OutlineWidth = 1.f)
	{
		FSlateBrush Brush = MakeSolidBrush(Fill, CornerRadius);
		Brush.OutlineSettings.Color = FSlateColor(Outline);
		Brush.OutlineSettings.Width = OutlineWidth;
		return Brush;
	}

	FButtonStyle MakeResetButtonStyle(const UFF_UIPalette& Palette)
	{
		const FFPauseOptionsLightTeal::FTokens Tokens = FFPauseOptionsLightTeal::Get(Palette);
		FButtonStyle Style;
		Style.Normal = MakePanelBrush(Tokens.Teal, Tokens.TealDeep, 6.f);
		Style.Hovered = MakePanelBrush(Tokens.TealDeep, Tokens.TealDeep, 6.f, 1.5f);
		Style.Pressed = MakePanelBrush(Tokens.TealDeep, Tokens.TealDeep, 6.f, 2.f);
		Style.Disabled = MakePanelBrush(Tokens.Disabled, Tokens.Line, 6.f);
		Style.NormalPadding = FMargin(18.f, 11.f);
		Style.PressedPadding = FMargin(18.f, 12.f, 18.f, 10.f);
		return Style;
	}

	bool SaveAsset(UObject& Asset)
	{
		UPackage* Package = Asset.GetOutermost();
		if (!Package)
		{
			return false;
		}

		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, &Asset, *Filename, SaveArgs);
	}

	bool SaveImportedTexture(UTexture2D& Texture)
	{
		Texture.Modify();
		Texture.PreEditChange(nullptr);
		Texture.LODGroup = TEXTUREGROUP_UI;
		Texture.MipGenSettings = TMGS_NoMipmaps;
		Texture.CompressionSettings = TC_EditorIcon;
		Texture.SRGB = true;
		Texture.NeverStream = true;
		Texture.AddressX = TA_Clamp;
		Texture.AddressY = TA_Clamp;
		Texture.PostEditChange();
		Texture.MarkPackageDirty();

		TArray<UTexture*> TexturesToFinish { &Texture };
		FTextureCompilingManager::Get().FinishCompilation(TexturesToFinish);
		return SaveAsset(Texture);
	}

	UTexture2D* ImportKeyboardTextureIfMissing()
	{
		const FString ObjectPath = MakeObjectPath(TexturePackage);
		if (UTexture2D* Existing = LoadObject<UTexture2D>(nullptr, *ObjectPath))
		{
			UE_LOG(LogFFUIWbpBuilder, Display,
				TEXT("%s_TEXTURE_REUSE: %s"), LogPrefix, *Existing->GetPathName());
			return Existing;
		}

		if (FPackageName::DoesPackageExist(TexturePackage))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_TEXTURE_FAIL: %s 패키지가 있지만 UTexture2D가 아닙니다. 기존 자산을 수정하지 않았습니다."),
				LogPrefix, *TexturePackage);
			return nullptr;
		}

		const FString AbsoluteSource = FPaths::ConvertRelativePathToFull(TextureSource);
		if (!IFileManager::Get().FileExists(*AbsoluteSource))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_TEXTURE_FAIL: 원본 PNG가 없습니다: %s"), LogPrefix, *AbsoluteSource);
			return nullptr;
		}

		UTextureFactory* Factory = NewObject<UTextureFactory>();
		Factory->LODGroup = TEXTUREGROUP_UI;
		Factory->MipGenSettings = TMGS_NoMipmaps;
		Factory->CompressionSettings = TC_EditorIcon;
		Factory->ColorSpaceMode = ETextureSourceColorSpace::SRGB;

		UAssetImportTask* ImportTask = NewObject<UAssetImportTask>();
		ImportTask->Filename = AbsoluteSource;
		ImportTask->DestinationPath = TextureFolder;
		ImportTask->DestinationName = FPackageName::GetLongPackageAssetName(TexturePackage);
		ImportTask->bAutomated = true;
		ImportTask->bAsync = false;
		ImportTask->bSave = false;
		ImportTask->bReplaceExisting = false;
		ImportTask->bReplaceExistingSettings = false;
		ImportTask->Factory = Factory;

		TArray<UAssetImportTask*> Tasks { ImportTask };
		FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"))
			.Get().ImportAssetTasks(Tasks);

		const TArray<UObject*>& ImportedObjects = ImportTask->GetObjects();
		UTexture2D* Texture = ImportedObjects.IsEmpty()
			? nullptr
			: Cast<UTexture2D>(ImportedObjects[0]);
		if (!Texture || !SaveImportedTexture(*Texture))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_TEXTURE_FAIL: %s -> %s 임포트/저장에 실패했습니다."),
				LogPrefix, *AbsoluteSource, *TexturePackage);
			return nullptr;
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_TEXTURE_CREATE: %s"), LogPrefix, *Texture->GetPathName());
		return Texture;
	}

	bool BuildPanelLayout(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		UTexture2D& KeyboardTexture)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_WBP_FAIL: LightTeal 팔레트를 불러오지 못했습니다."), LogPrefix);
			return false;
		}

		USizeBox* Root = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Root"));
		UVerticalBox* Content = MakeWidget<UVerticalBox>(
			Blueprint, Tree, TEXT("VerticalBox_Root"));
		UTextBlock* Title = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Title"));
		UTextBlock* Description = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_Description"));

		USizeBox* KeyboardBox = MakeWidget<USizeBox>(
			Blueprint, Tree, TEXT("SizeBox_KeyboardArtwork"));
		UBorder* KeyboardBorder = MakeWidget<UBorder>(
			Blueprint, Tree, TEXT("Border_KeyboardArtwork"));
		UOverlay* KeyboardOverlay = MakeWidget<UOverlay>(
			Blueprint, Tree, TEXT("Overlay_KeyboardArtwork"));
		UScaleBox* KeyboardArtworkScale = MakeWidget<UScaleBox>(
			Blueprint, Tree, TEXT("ScaleBox_KeyboardArtwork"));
		UImage* KeyboardArtwork = MakeWidget<UImage>(
			Blueprint, Tree, TEXT("Image_KeyboardArtwork"), true);
		UScaleBox* KeyboardRowsScale = MakeWidget<UScaleBox>(
			Blueprint, Tree, TEXT("ScaleBox_KeyboardRows"));
		UVerticalBox* KeyboardRows = MakeWidget<UVerticalBox>(
			Blueprint, Tree, TEXT("VerticalBox_KeyboardRows"), true);

		UBorder* CaptureStatusBorder = MakeWidget<UBorder>(
			Blueprint, Tree, TEXT("Border_CaptureStatus"));
		UTextBlock* CaptureStatus = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_CaptureStatus"), true);
		USizeBox* ResetButtonBox = MakeWidget<USizeBox>(
			Blueprint, Tree, TEXT("SizeBox_ResetAll"));
		UButton* ResetAll = MakeWidget<UButton>(
			Blueprint, Tree, TEXT("Button_ResetAll"), true);
		UTextBlock* ResetAllText = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_ResetAll"), true);

		UBorder* SectionHeaderBorder = MakeWidget<UBorder>(
			Blueprint, Tree, TEXT("Border_BindingSectionHeader"));
		UTextBlock* SectionHeader = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_BindingSectionHeader"));
		UVerticalBox* BindingRows = MakeWidget<UVerticalBox>(
			Blueprint, Tree, TEXT("VerticalBox_BindingRows"), true);

		if (!Root || !Content || !Title || !Description || !KeyboardBox || !KeyboardBorder
			|| !KeyboardOverlay || !KeyboardArtworkScale || !KeyboardArtwork || !KeyboardRowsScale
			|| !KeyboardRows || !CaptureStatusBorder
			|| !CaptureStatus || !ResetButtonBox || !ResetAll || !ResetAllText
			|| !SectionHeaderBorder || !SectionHeader || !BindingRows)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetMinDesiredHeight(650.f);
		Root->SetClipping(EWidgetClipping::ClipToBounds);
		Content->SetClipping(EWidgetClipping::ClipToBounds);
		Root->AddChild(Content);

		StyleText(*Title, 22, Palette->InGame.Ink, TEXT("SemiBold"));
		Title->SetText(LOCTEXT("PanelTitle", "키보드 한눈에 보기"));
		Title->SetAutoWrapText(false);
		Title->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		Title->SetClipping(EWidgetClipping::ClipToBounds);
		Title->SetToolTipText(Title->GetText());
		if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(Title))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetPadding(FMargin(0.f, 0.f, 0.f, 5.f));
		}

		StyleText(*Description, 14, Palette->InGame.InkMuted, TEXT("Medium"));
		Description->SetText(LOCTEXT(
			"PanelDescription",
			"키를 누르거나 키캡을 클릭하면 현재 동작을 확인합니다. 변경하려면 아래 조작 항목을 먼저 선택하세요."));
		Description->SetAutoWrapText(true);
		Description->SetClipping(EWidgetClipping::ClipToBounds);
		if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(Description))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
		}

		KeyboardBox->SetHeightOverride(FFF_KeyBindingLayoutMetrics::StageHeight);
		KeyboardBox->SetClipping(EWidgetClipping::ClipToBounds);
		KeyboardBorder->SetBrush(MakePanelBrush(Palette->InGame.PaperSunken, Palette->InGame.Line, 6.f));
		KeyboardBorder->SetClipping(EWidgetClipping::ClipToBounds);
		KeyboardBorder->SetPadding(FMargin(18.f));
		KeyboardOverlay->SetClipping(EWidgetClipping::ClipToBounds);
		KeyboardBorder->AddChild(KeyboardOverlay);
		KeyboardBox->AddChild(KeyboardBorder);

		KeyboardArtwork->SetBrushFromTexture(&KeyboardTexture, true);
		KeyboardArtwork->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.18f));
		KeyboardArtwork->SetVisibility(ESlateVisibility::HitTestInvisible);
		// The raster artwork and generated keycap grid do not share an authored coordinate system.
		// Rendering them through independent ScaleBoxes produces a doubled, misregistered keyboard
		// as aspect ratio changes, so retain the named artwork for the BindWidget contract but hide
		// it until both layers can use one common transform.
		KeyboardArtworkScale->SetVisibility(ESlateVisibility::Collapsed);
		KeyboardArtworkScale->AddChild(KeyboardArtwork);
		if (UOverlaySlot* Slot = KeyboardOverlay->AddChildToOverlay(KeyboardArtworkScale))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}

		// The full ANSI layout is wider than compact/windowed viewports. Scale only when needed so
		// keycaps remain legible at desktop sizes while every key stays inside the artwork stage.
		KeyboardRowsScale->SetStretch(EStretch::ScaleToFit);
		KeyboardRowsScale->SetStretchDirection(EStretchDirection::DownOnly);
		KeyboardRowsScale->SetClipping(EWidgetClipping::ClipToBounds);
		KeyboardRows->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		KeyboardRows->SetClipping(EWidgetClipping::ClipToBounds);
		KeyboardRowsScale->AddChild(KeyboardRows);
		if (UOverlaySlot* Slot = KeyboardOverlay->AddChildToOverlay(KeyboardRowsScale))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
			Slot->SetPadding(FMargin(12.f));
		}

		if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(KeyboardBox))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		}

		CaptureStatusBorder->SetBrush(MakePanelBrush(Palette->InGame.StatusBar, Palette->InGame.Line, 6.f));
		CaptureStatusBorder->SetClipping(EWidgetClipping::ClipToBounds);
		CaptureStatusBorder->SetPadding(FMargin(14.f, 10.f));
		StyleText(*CaptureStatus, 14, Palette->InGame.InkMuted, TEXT("SemiBold"));
		CaptureStatus->SetText(LOCTEXT(
			"CaptureIdle", "키를 누르거나 키캡을 클릭하면 현재 설정을 확인할 수 있습니다."));
		CaptureStatus->SetAutoWrapText(true);
		CaptureStatus->SetClipping(EWidgetClipping::ClipToBounds);
		CaptureStatusBorder->AddChild(CaptureStatus);
		if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(CaptureStatusBorder))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		}

		ResetButtonBox->SetMaxDesiredWidth(260.f);
		ResetButtonBox->SetHeightOverride(48.f);
		ResetButtonBox->SetClipping(EWidgetClipping::ClipToBounds);
		ResetAll->SetStyle(MakeResetButtonStyle(*Palette));
		ResetAll->SetClipping(EWidgetClipping::ClipToBounds);
		StyleText(*ResetAllText, 16, Palette->InGame.White, TEXT("SemiBold"));
		ResetAllText->SetText(LOCTEXT("ResetAllLabel", "전체 키 기본값 복원"));
		ResetAllText->SetAutoWrapText(true);
		ResetAllText->SetJustification(ETextJustify::Center);
		ResetAllText->SetClipping(EWidgetClipping::ClipToBounds);
		ResetAllText->SetToolTipText(ResetAllText->GetText());
		if (UButtonSlot* Slot = Cast<UButtonSlot>(ResetAll->AddChild(ResetAllText)))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		ResetButtonBox->AddChild(ResetAll);
		if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(ResetButtonBox))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
		}

		SectionHeaderBorder->SetBrush(MakePanelBrush(Palette->InGame.TealSoft, Palette->InGame.LineStrong, 4.f));
		SectionHeaderBorder->SetClipping(EWidgetClipping::ClipToBounds);
		SectionHeaderBorder->SetPadding(FMargin(16.f, 10.f));
		StyleText(*SectionHeader, 18, Palette->InGame.TealDeep, TEXT("SemiBold"));
		SectionHeader->SetText(LOCTEXT("BindingSectionHeader", "조작별 키 설정"));
		SectionHeader->SetAutoWrapText(false);
		SectionHeader->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		SectionHeader->SetClipping(EWidgetClipping::ClipToBounds);
		SectionHeader->SetToolTipText(SectionHeader->GetText());
		SectionHeaderBorder->AddChild(SectionHeader);
		if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(SectionHeaderBorder))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		}

		BindingRows->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		BindingRows->SetClipping(EWidgetClipping::ClipToBounds);
		if (UVerticalBoxSlot* Slot = Content->AddChildToVerticalBox(BindingRows))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}

		return true;
	}

	bool EnsureKeyBindingPanel(UTexture2D& KeyboardTexture)
	{
		return EnsureWidgetBlueprint(
			LogPrefix,
			PanelPackage,
			SettingsFolder,
			*UFF_KeyBindingPanel::StaticClass(),
			[&KeyboardTexture](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildPanelLayout(Blueprint, Tree, KeyboardTexture);
			}) != nullptr;
	}

	bool HasKeyBindingPanelContract(const UWidgetBlueprint& Blueprint)
	{
		if (!Blueprint.WidgetTree)
		{
			return false;
		}
		auto HasWidget = [&Blueprint](const FName Name, const UClass& ExpectedClass)
		{
			const UWidget* Widget = Blueprint.WidgetTree->FindWidget(Name);
			return Widget && Widget->IsA(&ExpectedClass);
		};
		return HasWidget(TEXT("SizeBox_Root"), *USizeBox::StaticClass())
			&& HasWidget(TEXT("SizeBox_KeyboardArtwork"), *USizeBox::StaticClass())
			&& HasWidget(TEXT("Image_KeyboardArtwork"), *UImage::StaticClass())
			&& HasWidget(TEXT("ScaleBox_KeyboardRows"), *UScaleBox::StaticClass())
			&& HasWidget(TEXT("VerticalBox_KeyboardRows"), *UVerticalBox::StaticClass())
			&& HasWidget(TEXT("VerticalBox_BindingRows"), *UVerticalBox::StaticClass())
			&& HasWidget(TEXT("Text_CaptureStatus"), *UTextBlock::StaticClass())
			&& HasWidget(TEXT("Button_ResetAll"), *UButton::StaticClass())
			&& HasWidget(TEXT("Text_ResetAll"), *UTextBlock::StaticClass());
	}

	bool RefreshKeyBindingPanelBlueprint(UTexture2D& KeyboardTexture)
	{
		if (!FPackageName::DoesPackageExist(PanelPackage))
		{
			return EnsureKeyBindingPanel(KeyboardTexture);
		}

		UWidgetBlueprint* Blueprint = LoadExistingBlueprint(PanelPackage);
		if (!Blueprint || !Blueprint->GeneratedClass || !Blueprint->WidgetTree
			|| !Blueprint->GeneratedClass->IsChildOf(UFF_KeyBindingPanel::StaticClass())
			|| !Blueprint->WidgetTree->RootWidget
			|| Blueprint->WidgetTree->RootWidget->GetFName() != FName(TEXT("SizeBox_Root")))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REFRESH_SKIP: 소유권 또는 부모 계약이 다른 키 패널은 덮어쓰지 않습니다."),
				LogPrefix);
			return false;
		}

		UWidgetTree* OriginalTree = Blueprint->WidgetTree;
		const FName OriginalTreeName = OriginalTree->GetFName();
		const bool bOriginalTreeWasTransient = OriginalTree->HasAnyFlags(RF_Transient);
		const auto OriginalBindings = Blueprint->Bindings;
		const auto OriginalVariableMap = Blueprint->WidgetVariableNameToGuidMap;
		const FName DiscardedName = MakeUniqueObjectName(
			Blueprint, UWidgetTree::StaticClass(), TEXT("DiscardedKeyBindingWidgetTree"));
		if (!OriginalTree->Rename(*DiscardedName.ToString(), Blueprint,
			REN_DontCreateRedirectors | REN_NonTransactional))
		{
			return false;
		}

		Blueprint->Modify();
		OriginalTree->SetFlags(RF_Transient);
		Blueprint->Bindings.Reset();
		Blueprint->WidgetVariableNameToGuidMap.Reset();
		Blueprint->WidgetTree = NewObject<UWidgetTree>(Blueprint, UWidgetTree::StaticClass(),
			TEXT("WidgetTree"), RF_Transactional);

		auto Rollback = [&]()
		{
			if (UWidgetTree* FailedTree = Blueprint->WidgetTree)
			{
				const FName FailedName = MakeUniqueObjectName(
					Blueprint, UWidgetTree::StaticClass(), TEXT("FailedKeyBindingWidgetTree"));
				FailedTree->Rename(*FailedName.ToString(), Blueprint,
					REN_DontCreateRedirectors | REN_NonTransactional);
				FailedTree->SetFlags(RF_Transient);
			}
			if (!bOriginalTreeWasTransient)
			{
				OriginalTree->ClearFlags(RF_Transient);
			}
			OriginalTree->Rename(*OriginalTreeName.ToString(), Blueprint,
				REN_DontCreateRedirectors | REN_NonTransactional);
			Blueprint->WidgetTree = OriginalTree;
			Blueprint->Bindings = OriginalBindings;
			Blueprint->WidgetVariableNameToGuidMap = OriginalVariableMap;
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			FKismetEditorUtilities::CompileBlueprint(Blueprint);
		};

		if (!Blueprint->WidgetTree || !BuildPanelLayout(*Blueprint, *Blueprint->WidgetTree, KeyboardTexture)
			|| !HasKeyBindingPanelContract(*Blueprint) || !SaveNewBlueprint(LogPrefix, *Blueprint))
		{
			Rollback();
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REFRESH_FAIL: 기존 키 패널 WidgetTree를 복구했습니다."), LogPrefix);
			return false;
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_REFRESHED: LightTeal 키 배치 패널 저장 완료."), LogPrefix);
		return true;
	}

	void RefreshKeyBindingUmg()
	{
		if (UTexture2D* KeyboardTexture = ImportKeyboardTextureIfMissing())
		{
			RefreshKeyBindingPanelBlueprint(*KeyboardTexture);
		}
	}

	struct FMappingSpec
	{
		FName ActionName;
		FKey Key;
		FName MappingName;
		FText DisplayName;
		FText DisplayCategory;
	};

	struct FResolvedMapping
	{
		int32 Index = INDEX_NONE;
		FMappingSpec Spec;
	};

	struct FContextPlan
	{
		UInputMappingContext* Context = nullptr;
		TArray<FResolvedMapping> Mappings;
	};

	TArray<FMappingSpec> MakeCameraMappingSpecs()
	{
		const FText Category = LOCTEXT("CameraCategory", "관찰 카메라");
		return {
			{ TEXT("IA_Pan"), EKeys::W, TEXT("FF.Camera.Move.Forward"),
				LOCTEXT("CameraForward", "카메라 앞으로 이동"), Category },
			{ TEXT("IA_Pan"), EKeys::S, TEXT("FF.Camera.Move.Backward"),
				LOCTEXT("CameraBackward", "카메라 뒤로 이동"), Category },
			{ TEXT("IA_Pan"), EKeys::A, TEXT("FF.Camera.Move.Left"),
				LOCTEXT("CameraLeft", "카메라 왼쪽 이동"), Category },
			{ TEXT("IA_Pan"), EKeys::D, TEXT("FF.Camera.Move.Right"),
				LOCTEXT("CameraRight", "카메라 오른쪽 이동"), Category },
			{ TEXT("IA_SnapRotate"), EKeys::Q, TEXT("FF.Camera.Rotate.Left"),
				LOCTEXT("CameraRotateLeft", "카메라 왼쪽 회전"), Category },
			{ TEXT("IA_SnapRotate"), EKeys::E, TEXT("FF.Camera.Rotate.Right"),
				LOCTEXT("CameraRotateRight", "카메라 오른쪽 회전"), Category },
			{ TEXT("IA_ResetView"), EKeys::Home, TEXT("FF.Camera.ResetView"),
				LOCTEXT("CameraResetView", "관찰 카메라 초기화"), Category },
			{ TEXT("IA_ToggleView"), EKeys::Tab, TEXT("FF.Camera.ToggleView"),
				LOCTEXT("CameraToggleView", "관찰 / 캐릭터 시점 전환"), Category },
		};
	}

	TArray<FMappingSpec> MakeCharacterMappingSpecs()
	{
		const FText Category = LOCTEXT("CharacterCategory", "캐릭터");
		return {
			{ TEXT("IA_Move"), EKeys::W, TEXT("FF.Character.Move.Forward"),
				LOCTEXT("CharacterForward", "캐릭터 앞으로 이동"), Category },
			{ TEXT("IA_Move"), EKeys::S, TEXT("FF.Character.Move.Backward"),
				LOCTEXT("CharacterBackward", "캐릭터 뒤로 이동"), Category },
			{ TEXT("IA_Move"), EKeys::A, TEXT("FF.Character.Move.Left"),
				LOCTEXT("CharacterLeft", "캐릭터 왼쪽 이동"), Category },
			{ TEXT("IA_Move"), EKeys::D, TEXT("FF.Character.Move.Right"),
				LOCTEXT("CharacterRight", "캐릭터 오른쪽 이동"), Category },
			{ TEXT("IA_Jump"), EKeys::SpaceBar, TEXT("FF.Character.Jump"),
				LOCTEXT("CharacterJump", "점프"), Category },
			{ TEXT("IA_Sprint"), EKeys::LeftShift, TEXT("FF.Character.Sprint"),
				LOCTEXT("CharacterSprint", "달리기"), Category },
		};
	}

	bool ResolveContextPlan(
		const TCHAR* ContextPath,
		const TArray<FMappingSpec>& Specs,
		FContextPlan& OutPlan)
	{
		UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, ContextPath);
		if (!Context)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_MAPPING_FAIL: IMC를 불러오지 못했습니다: %s"), LogPrefix, ContextPath);
			return false;
		}

		OutPlan.Context = Context;
		OutPlan.Mappings.Reset();
		const TArray<FEnhancedActionKeyMapping>& Mappings = Context->GetMappings();

		for (const FMappingSpec& Spec : Specs)
		{
			int32 MatchIndex = INDEX_NONE;
			int32 MatchCount = 0;
			for (int32 Index = 0; Index < Mappings.Num(); ++Index)
			{
				const FEnhancedActionKeyMapping& Mapping = Mappings[Index];
				if (Mapping.Action
					&& Mapping.Action->GetFName() == Spec.ActionName
					&& Mapping.Key == Spec.Key)
				{
					MatchIndex = Index;
					++MatchCount;
				}
			}

			if (MatchCount != 1)
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_MAPPING_FAIL: %s에서 %s + %s 매핑이 정확히 하나여야 하지만 %d개입니다. 어떤 매핑도 수정하지 않았습니다."),
					LogPrefix, *Context->GetPathName(), *Spec.ActionName.ToString(),
					*Spec.Key.GetDisplayName().ToString(), MatchCount);
				return false;
			}

			OutPlan.Mappings.Add({ MatchIndex, Spec });
		}

		return true;
	}

	bool ResolveMappingProperties(
		FEnumProperty*& OutBehaviorProperty,
		FObjectPropertyBase*& OutSettingsProperty)
	{
		UScriptStruct* MappingStruct = FEnhancedActionKeyMapping::StaticStruct();
		OutBehaviorProperty = FindFProperty<FEnumProperty>(
			MappingStruct, TEXT("SettingBehavior"));
		OutSettingsProperty = FindFProperty<FObjectPropertyBase>(
			MappingStruct, TEXT("PlayerMappableKeySettings"));

		if (!OutBehaviorProperty || !OutSettingsProperty)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_MAPPING_FAIL: UE 5.7 Enhanced Input의 매핑 설정 프로퍼티를 찾지 못했습니다."),
				LogPrefix);
			return false;
		}

		return true;
	}

	EPlayerMappableKeySettingBehaviors GetSettingBehavior(
		const FEnhancedActionKeyMapping& Mapping,
		const FEnumProperty& Property)
	{
		const void* Value = Property.ContainerPtrToValuePtr<const void>(&Mapping);
		const uint64 RawValue = Property.GetUnderlyingProperty()->GetUnsignedIntPropertyValue(Value);
		return static_cast<EPlayerMappableKeySettingBehaviors>(RawValue);
	}

	void SetSettingBehavior(
		FEnhancedActionKeyMapping& Mapping,
		const FEnumProperty& Property,
		const EPlayerMappableKeySettingBehaviors Behavior)
	{
		void* Value = Property.ContainerPtrToValuePtr<void>(&Mapping);
		Property.GetUnderlyingProperty()->SetIntPropertyValue(
			Value, static_cast<uint64>(Behavior));
	}

	UPlayerMappableKeySettings* GetRawSettings(
		const FEnhancedActionKeyMapping& Mapping,
		const FObjectPropertyBase& Property)
	{
		return Cast<UPlayerMappableKeySettings>(
			Property.GetObjectPropertyValue_InContainer(&Mapping));
	}

	bool TextMatches(const FText& A, const FText& B)
	{
		return A.ToString().Equals(B.ToString(), ESearchCase::CaseSensitive);
	}

	bool IsSettingsObjectUniqueToMapping(
		const UInputMappingContext& Context,
		const UPlayerMappableKeySettings& Settings,
		const FObjectPropertyBase& SettingsProperty)
	{
		int32 ReferenceCount = 0;
		for (const FEnhancedActionKeyMapping& Mapping : Context.GetMappings())
		{
			if (GetRawSettings(Mapping, SettingsProperty) == &Settings)
			{
				++ReferenceCount;
			}
		}
		return ReferenceCount == 1;
	}

	bool MappingNeedsUpdate(
		const UInputMappingContext& Context,
		const FEnhancedActionKeyMapping& Mapping,
		const FMappingSpec& Spec,
		const FEnumProperty& BehaviorProperty,
		const FObjectPropertyBase& SettingsProperty)
	{
		if (GetSettingBehavior(Mapping, BehaviorProperty)
			!= EPlayerMappableKeySettingBehaviors::OverrideSettings)
		{
			return true;
		}

		const UPlayerMappableKeySettings* Settings = GetRawSettings(Mapping, SettingsProperty);
		return !Settings
			|| Settings->GetOuter() != &Context
			|| !IsSettingsObjectUniqueToMapping(Context, *Settings, SettingsProperty)
			|| Settings->Name != Spec.MappingName
			|| !TextMatches(Settings->DisplayName, Spec.DisplayName)
			|| !TextMatches(Settings->DisplayCategory, Spec.DisplayCategory);
	}

	bool ApplyContextPlan(
		const FContextPlan& Plan,
		const FEnumProperty& BehaviorProperty,
		const FObjectPropertyBase& SettingsProperty)
	{
		UInputMappingContext* Context = Plan.Context;
		if (!Context)
		{
			return false;
		}

		bool bNeedsAnyUpdate = false;
		for (const FResolvedMapping& Resolved : Plan.Mappings)
		{
			const FEnhancedActionKeyMapping& Mapping = Context->GetMappings()[Resolved.Index];
			bNeedsAnyUpdate |= MappingNeedsUpdate(
				*Context, Mapping, Resolved.Spec, BehaviorProperty, SettingsProperty);
		}

		if (!bNeedsAnyUpdate)
		{
			UE_LOG(LogFFUIWbpBuilder, Display,
				TEXT("%s_MAPPING_REUSE: %s의 설정이 이미 동일합니다."),
				LogPrefix, *Context->GetPathName());
			return true;
		}

		Context->Modify();
		Context->PreEditChange(nullptr);

		for (const FResolvedMapping& Resolved : Plan.Mappings)
		{
			FEnhancedActionKeyMapping& Mapping = Context->GetMapping(Resolved.Index);
			if (!MappingNeedsUpdate(
				*Context, Mapping, Resolved.Spec, BehaviorProperty, SettingsProperty))
			{
				continue;
			}

			UPlayerMappableKeySettings* Settings = nullptr;
			if (GetSettingBehavior(Mapping, BehaviorProperty)
				== EPlayerMappableKeySettingBehaviors::OverrideSettings)
			{
				UPlayerMappableKeySettings* Existing = GetRawSettings(Mapping, SettingsProperty);
				if (Existing
					&& Existing->GetOuter() == Context
					&& IsSettingsObjectUniqueToMapping(*Context, *Existing, SettingsProperty))
				{
					Settings = Existing;
				}
			}

			if (!Settings)
			{
				Settings = NewObject<UPlayerMappableKeySettings>(
					Context, UPlayerMappableKeySettings::StaticClass(), NAME_None, RF_Transactional);
				if (!Settings)
				{
					Context->PostEditChange();
					UE_LOG(LogFFUIWbpBuilder, Error,
						TEXT("%s_MAPPING_FAIL: %s용 UPlayerMappableKeySettings 생성에 실패했습니다."),
						LogPrefix, *Resolved.Spec.MappingName.ToString());
					return false;
				}
				SettingsProperty.SetObjectPropertyValue_InContainer(&Mapping, Settings);
			}

			Settings->Modify();
			Settings->PreEditChange(nullptr);
			Settings->Name = Resolved.Spec.MappingName;
			Settings->DisplayName = Resolved.Spec.DisplayName;
			Settings->DisplayCategory = Resolved.Spec.DisplayCategory;
			Settings->PostEditChange();
			SetSettingBehavior(
				Mapping, BehaviorProperty, EPlayerMappableKeySettingBehaviors::OverrideSettings);
		}

		Context->PostEditChange();
		Context->MarkPackageDirty();
		if (!SaveAsset(*Context))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_MAPPING_FAIL: %s 패키지 저장에 실패했습니다."),
				LogPrefix, *Context->GetPathName());
			return false;
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_MAPPING_SAVE: %s (%d개 키)"),
			LogPrefix, *Context->GetPathName(), Plan.Mappings.Num());
		return true;
	}

	void BuildKeyBindingAssets()
	{
		FEnumProperty* BehaviorProperty = nullptr;
		FObjectPropertyBase* SettingsProperty = nullptr;
		if (!ResolveMappingProperties(BehaviorProperty, SettingsProperty))
		{
			return;
		}

		FContextPlan CameraPlan;
		FContextPlan CharacterPlan;
		if (!ResolveContextPlan(CameraContextPath, MakeCameraMappingSpecs(), CameraPlan)
			|| !ResolveContextPlan(CharacterContextPath, MakeCharacterMappingSpecs(), CharacterPlan))
		{
			return;
		}

		UTexture2D* KeyboardTexture = ImportKeyboardTextureIfMissing();
		if (!KeyboardTexture || !EnsureKeyBindingPanel(*KeyboardTexture))
		{
			return;
		}

		if (!ApplyContextPlan(CameraPlan, *BehaviorProperty, *SettingsProperty)
			|| !ApplyContextPlan(CharacterPlan, *BehaviorProperty, *SettingsProperty))
		{
			return;
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_DONE: 키보드 텍스처, WBP, 플레이어 매핑 설정을 준비했습니다."), LogPrefix);
	}

	FAutoConsoleCommand BuildKeyBindingAssetsCommand(
		TEXT("FF.KeyBindingUMG"),
		TEXT("Import the generated keyboard artwork, scaffold WBP_FF_KeyBindingPanel, and configure keyboard mappings."),
		FConsoleCommandDelegate::CreateStatic(&BuildKeyBindingAssets));

	FAutoConsoleCommand RefreshKeyBindingUmgCommand(
		TEXT("FF.KeyBindingUMG.Refresh"),
		TEXT("Refresh only the builder-owned key binding panel with the LightTeal layout."),
		FConsoleCommandDelegate::CreateStatic(&RefreshKeyBindingUmg));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
