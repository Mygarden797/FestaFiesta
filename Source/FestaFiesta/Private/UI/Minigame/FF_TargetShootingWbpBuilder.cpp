// Copyright Epic Games, Inc. All Rights Reserved.

// Target-shooting authoring commands.
//
// FF.BuildTargetShootingAssets creates only missing assets and remains non-destructive.
// FF.RebuildTargetShootingUI is the explicit opt-in path that replaces one clean,
// existing screen WBP with the authored AAA layout. It validates but never rewrites
// designer-authored balance values in DA_Minigame_TargetShooting.

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Factories/DataAssetFactory.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Minigame/FF_MinigameDefinition.h"
#include "Minigame/FF_TargetShootingRuntime.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Minigame/FF_TargetShootingScreen.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "FFTargetShootingAssetBuilder"

namespace FFTargetShootingWbpBuilder
{
	using namespace FFUIWbpBuilder;

	const TCHAR* const LogPrefix = TEXT("FF_TARGET_SHOOTING_ASSETS");
	const FString ScreenFolder(TEXT("/Game/FestaFiesta/UI/Minigame"));
	const FString ScreenPackage(ScreenFolder / TEXT("WBP_FF_TargetShooting"));
	const FString DefinitionFolder(TEXT("/Game/FestaFiesta/Minigame/Data"));
	const FString DefinitionPackage(DefinitionFolder / TEXT("DA_Minigame_TargetShooting"));
	const FName TargetShootingId(TEXT("TargetShooting"));
	const FName ShootingFacilityId(TEXT("Booth_ActivityGame_Shoot"));

	bool SaveNewDataAsset(UFF_TargetShootingDefinition& Definition);

	bool ValidateWaterBlasterDefinition()
	{
		UFF_TargetShootingDefinition* Definition = Cast<UFF_TargetShootingDefinition>(
			LoadObject<UObject>(nullptr, *MakeObjectPath(DefinitionPackage)));
		if (!Definition || !Definition->GetOutermost()
			|| Definition->GetOutermost()->IsDirty())
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REBUILD_FAIL: target-shooting definition is unavailable or has unsaved edits."),
				LogPrefix);
			return false;
		}

		FString RulesFailure;
		if (!Definition->Rules.IsValid(&RulesFailure))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REBUILD_FAIL: designer-authored target-shooting rules are invalid: %s"),
				LogPrefix,
				*RulesFailure);
			return false;
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_REBUILD_PRESERVE: target-shooting DataAsset balance was not modified."),
			LogPrefix);
		return true;
	}

	UObject* FindLoadedAssetObject(const FString& PackageName)
	{
		UPackage* Package = FindPackage(nullptr, *PackageName);
		if (!Package)
		{
			return nullptr;
		}

		return FindObject<UObject>(Package, *FPackageName::GetLongPackageAssetName(PackageName));
	}

	UObject* LoadExistingAssetObject(const FString& PackageName)
	{
		if (UObject* Loaded = FindLoadedAssetObject(PackageName))
		{
			return Loaded;
		}

		return LoadObject<UObject>(nullptr, *MakeObjectPath(PackageName));
	}

	bool IsVariableWidget(const UWidgetBlueprint& Blueprint, const UWidget* Widget)
	{
		return Widget
			&& Widget->bIsVariable
			&& Blueprint.WidgetVariableNameToGuidMap.Contains(Widget->GetFName());
	}

	bool HasRequiredWidgetTree(const UWidgetBlueprint& Blueprint)
	{
		const UWidgetTree* Tree = Blueprint.WidgetTree;
		if (!Tree || !Tree->RootWidget)
		{
			return false;
		}

		const UBorder* PlayArea = Cast<UBorder>(Tree->FindWidget(TEXT("Border_PlayArea")));
		const UCanvasPanel* PlayCanvas = Cast<UCanvasPanel>(Tree->FindWidget(TEXT("Canvas_PlayArea")));
		const UBorder* Target = Cast<UBorder>(Tree->FindWidget(TEXT("Border_Target")));
		const UImage* TargetImage = Cast<UImage>(Tree->FindWidget(TEXT("Image_Target")));
		const UImage* WaterBlaster = Cast<UImage>(Tree->FindWidget(TEXT("Image_WaterBlaster")));
		const UTextBlock* MuzzleFlash = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_MuzzleFlash")));
		const UTextBlock* WeaponStatus = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_WeaponStatus")));
		const UTextBlock* TargetCue = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_TargetCue")));
		const UTextBlock* RemainingTime = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_RemainingTime")));
		const UTextBlock* Score = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_Score")));
		const UTextBlock* HitCount = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_HitCount")));
		const UTextBlock* MissCount = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_MissCount")));
		const UTextBlock* Streak = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_Streak")));
		const UBorder* ResultPanel = Cast<UBorder>(Tree->FindWidget(TEXT("Panel_Result")));
		const UTextBlock* ResultText = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_Result")));
		const UButton* CloseButton = Cast<UButton>(Tree->FindWidget(TEXT("Button_Close")));

		if (!IsVariableWidget(Blueprint, PlayArea)
			|| !IsVariableWidget(Blueprint, PlayCanvas)
			|| !IsVariableWidget(Blueprint, Target)
			|| !IsVariableWidget(Blueprint, TargetImage)
			|| !IsVariableWidget(Blueprint, WaterBlaster)
			|| !IsVariableWidget(Blueprint, MuzzleFlash)
			|| !IsVariableWidget(Blueprint, WeaponStatus)
			|| !IsVariableWidget(Blueprint, TargetCue)
			|| !IsVariableWidget(Blueprint, RemainingTime)
			|| !IsVariableWidget(Blueprint, Score)
			|| !IsVariableWidget(Blueprint, HitCount)
			|| !IsVariableWidget(Blueprint, MissCount)
			|| !IsVariableWidget(Blueprint, Streak)
			|| !IsVariableWidget(Blueprint, ResultPanel)
			|| !IsVariableWidget(Blueprint, ResultText)
			|| !IsVariableWidget(Blueprint, CloseButton))
		{
			return false;
		}

		// Runtime pointer normalization assumes this exact ownership boundary.
		return PlayArea->GetContent() == PlayCanvas
			&& Target->GetParent() == PlayCanvas
			&& TargetImage->GetParent() == Target->GetContent()
			&& WaterBlaster->GetParent() == PlayCanvas
			&& MuzzleFlash->GetParent() == PlayCanvas
			&& WeaponStatus->GetParent() == PlayCanvas
			&& TargetCue->GetParent() == PlayCanvas
			&& Cast<UCanvasPanelSlot>(Target->Slot) != nullptr
			&& Cast<UCanvasPanelSlot>(WaterBlaster->Slot) != nullptr
			&& Target->GetVisibility() == ESlateVisibility::HitTestInvisible;
	}

	void StyleAaaText(
		UTextBlock& Text,
		const int32 FontSize,
		const FLinearColor& Color,
		const FName Typeface = TEXT("Bold"))
	{
		StyleText(Text, FontSize, Color, Typeface);
		Text.SetShadowOffset(FVector2D(1.0f, 2.0f));
		Text.SetShadowColorAndOpacity(FLinearColor(0.0f, 0.01f, 0.02f, 0.82f));
	}

	bool BuildScreenLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const FFF_TargetShootingVisualStyle& Style =
			GetDefault<UFF_TargetShootingScreen>()->VisualStyle;
		UTexture2D* BackdropTexture = Style.BackdropTexture.LoadSynchronous();
		UTexture2D* WaterBlasterTexture = Style.WaterBlasterTexture.LoadSynchronous();
		UTexture2D* TargetTexture = Style.TargetTexture.LoadSynchronous();
		if (!BackdropTexture)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: target-shooting backdrop is missing: %s"),
				LogPrefix,
				*Style.BackdropTexture.ToSoftObjectPath().ToString());
			return false;
		}
		if (!WaterBlasterTexture)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: water-blaster foreground is missing: %s"),
				LogPrefix,
				*Style.WaterBlasterTexture.ToSoftObjectPath().ToString());
			return false;
		}
		if (!TargetTexture)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: target image is missing: %s"),
				LogPrefix,
				*Style.TargetTexture.ToSoftObjectPath().ToString());
			return false;
		}
		UOverlay* Root = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Root"));
		UImage* Backdrop = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Backdrop"), true);
		UBorder* ScreenWash = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_ScreenWash"));
		UCanvasPanel* LayoutCanvas = MakeWidget<UCanvasPanel>(
			Blueprint, Tree, TEXT("Canvas_WaterBlasterHud"));
		UBorder* ArenaFrame = MakeWidget<UBorder>(
			Blueprint, Tree, TEXT("Border_ArenaFrame"));
		UBorder* PlayArea = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_PlayArea"), true);
		UCanvasPanel* PlayCanvas = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("Canvas_PlayArea"), true);
		UBorder* Target = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Target"), true);
		UOverlay* TargetOverlay = MakeWidget<UOverlay>(
			Blueprint, Tree, TEXT("Overlay_TargetVisual"));
		UImage* TargetImage = MakeWidget<UImage>(
			Blueprint, Tree, TEXT("Image_Target"), true);
		if (!Root || !Backdrop || !ScreenWash || !LayoutCanvas || !ArenaFrame
			|| !PlayArea || !PlayCanvas || !Target || !TargetOverlay || !TargetImage)
		{
			return false;
		}

		Tree.RootWidget = Root;
		auto StretchOnRoot = [Root](UWidget* Widget) -> bool
		{
			if (UOverlaySlot* Slot = Root->AddChildToOverlay(Widget))
			{
				Slot->SetHorizontalAlignment(HAlign_Fill);
				Slot->SetVerticalAlignment(VAlign_Fill);
				return true;
			}
			return false;
		};
		auto PlaceAnchored = [LayoutCanvas](
			UWidget* Widget,
			const FAnchors& Anchors,
			const int32 ZOrder) -> bool
		{
			if (UCanvasPanelSlot* Slot = LayoutCanvas->AddChildToCanvas(Widget))
			{
				Slot->SetAnchors(Anchors);
				Slot->SetOffsets(FMargin(0.0f));
				Slot->SetAlignment(FVector2D::ZeroVector);
				Slot->SetZOrder(ZOrder);
				return true;
			}
			return false;
		};
		auto ConfigureCard = [](UBorder* Card, const FLinearColor& Color, const FMargin& InPadding)
		{
			Card->SetBrushColor(Color);
			Card->SetPadding(InPadding);
			Card->SetHorizontalAlignment(HAlign_Fill);
			Card->SetVerticalAlignment(VAlign_Center);
			Card->SetVisibility(ESlateVisibility::HitTestInvisible);
		};

		Backdrop->SetColorAndOpacity(Style.BackdropTint);
		Backdrop->SetVisibility(ESlateVisibility::HitTestInvisible);
		Backdrop->SetBrushFromTexture(BackdropTexture, true);
		ScreenWash->SetBrushColor(Style.ScreenWashColor);
		ScreenWash->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (!StretchOnRoot(Backdrop)
			|| !StretchOnRoot(ScreenWash)
			|| !StretchOnRoot(LayoutCanvas))
		{
			return false;
		}

		ArenaFrame->SetBrushColor(Style.bUseSingleBackdrop
			? FLinearColor::Transparent
			: Style.ArenaFrameColor);
		ArenaFrame->SetPadding(Style.bUseSingleBackdrop
			? FMargin(0.0f)
			: FMargin(2.0f));
		ArenaFrame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		PlayArea->SetBrushColor(Style.bUseSingleBackdrop
			? FLinearColor::Transparent
			: Style.ArenaSurfaceColor);
		PlayArea->SetPadding(FMargin(0.0f));
		PlayArea->SetContent(PlayCanvas);
		ArenaFrame->SetContent(PlayArea);
		if (!PlaceAnchored(ArenaFrame, FAnchors(0.08f, 0.16f, 0.92f, 0.93f), -100))
		{
			return false;
		}

		Target->SetPadding(FMargin(0.0f));
		Target->SetVisibility(ESlateVisibility::HitTestInvisible);
		FSlateBrush NoTargetBackground;
		NoTargetBackground.DrawAs = ESlateBrushDrawType::NoDrawType;
		Target->SetBrush(NoTargetBackground);
		TargetImage->SetBrushFromTexture(TargetTexture, true);
		TargetImage->SetColorAndOpacity(Style.TargetImageTint);
		TargetImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* ImageSlot = TargetOverlay->AddChildToOverlay(TargetImage))
		{
			ImageSlot->SetHorizontalAlignment(HAlign_Fill);
			ImageSlot->SetVerticalAlignment(VAlign_Fill);
		}
		else
		{
			return false;
		}
		Target->SetContent(TargetOverlay);
		if (UCanvasPanelSlot* TargetSlot = PlayCanvas->AddChildToCanvas(Target))
		{
			TargetSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			TargetSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			TargetSlot->SetPosition(FVector2D::ZeroVector);
			TargetSlot->SetSize(FVector2D(112.0f, 112.0f));
			TargetSlot->SetZOrder(1);
		}
		else
		{
			return false;
		}

		UTextBlock* TargetCue = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_TargetCue"), true);
		if (!TargetCue)
		{
			return false;
		}
		TargetCue->SetText(LOCTEXT("TargetCue", "여기를 쏘세요"));
		TargetCue->SetJustification(ETextJustify::Center);
		TargetCue->SetVisibility(ESlateVisibility::HitTestInvisible);
		StyleAaaText(
			*TargetCue,
			FMath::RoundToInt(Style.TargetCueFontSize),
			Style.TargetCueColor);
		if (UCanvasPanelSlot* CueSlot = PlayCanvas->AddChildToCanvas(TargetCue))
		{
			CueSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			CueSlot->SetAlignment(FVector2D(0.5f, 1.0f));
			CueSlot->SetPosition(FVector2D(0.0f, -72.0f));
			CueSlot->SetSize(FVector2D(240.0f, 38.0f));
			CueSlot->SetZOrder(7);
		}
		else
		{
			return false;
		}

		UImage* WaterBlaster = MakeWidget<UImage>(
			Blueprint, Tree, TEXT("Image_WaterBlaster"), true);
		UTextBlock* MuzzleFlash = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_MuzzleFlash"), true);
		UTextBlock* WeaponStatus = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_WeaponStatus"), true);
		if (!WaterBlaster || !MuzzleFlash || !WeaponStatus)
		{
			return false;
		}
		WaterBlaster->SetBrushFromTexture(WaterBlasterTexture, true);
		WaterBlaster->SetColorAndOpacity(Style.WaterBlasterTint);
		WaterBlaster->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UCanvasPanelSlot* BlasterSlot = PlayCanvas->AddChildToCanvas(WaterBlaster))
		{
			const float BlasterSize = FMath::Clamp(
				Style.WaterBlasterVisualSize, 180.0f, 520.0f);
			BlasterSlot->SetAnchors(FAnchors(0.5f, 1.0f));
			BlasterSlot->SetAlignment(FVector2D(0.5f, 1.0f));
			BlasterSlot->SetPosition(FVector2D(
				0.0f,
				FMath::Clamp(Style.WaterBlasterBottomOffsetPixels, 0.0f, 80.0f)));
			BlasterSlot->SetSize(FVector2D(BlasterSize, BlasterSize));
			BlasterSlot->SetZOrder(15);
		}
		else
		{
			return false;
		}

		MuzzleFlash->SetText(FText::FromString(TEXT("+")));
		MuzzleFlash->SetJustification(ETextJustify::Center);
		MuzzleFlash->SetVisibility(ESlateVisibility::Collapsed);
		StyleAaaText(*MuzzleFlash, 54, Style.MuzzleFlashColor);
		if (UCanvasPanelSlot* MuzzleSlot = PlayCanvas->AddChildToCanvas(MuzzleFlash))
		{
			MuzzleSlot->SetAnchors(FAnchors(0.5f, 0.68f));
			MuzzleSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			MuzzleSlot->SetPosition(FVector2D::ZeroVector);
			MuzzleSlot->SetSize(FVector2D(96.0f, 96.0f));
			MuzzleSlot->SetZOrder(17);
		}
		else
		{
			return false;
		}

		WeaponStatus->SetText(LOCTEXT(
			"WeaponStatusPlaceholder", "총기를 회전해 과녁을 조준하고 발사!"));
		WeaponStatus->SetJustification(ETextJustify::Center);
		WeaponStatus->SetVisibility(ESlateVisibility::HitTestInvisible);
		StyleAaaText(*WeaponStatus, 16, Style.HudAmberColor, TEXT("SemiBold"));
		if (UCanvasPanelSlot* StatusSlot = PlayCanvas->AddChildToCanvas(WeaponStatus))
		{
			StatusSlot->SetAnchors(FAnchors(0.5f, 0.075f));
			StatusSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			StatusSlot->SetPosition(FVector2D::ZeroVector);
			StatusSlot->SetSize(FVector2D(440.0f, 42.0f));
			StatusSlot->SetZOrder(19);
		}
		else
		{
			return false;
		}

		UTextBlock* ShotFeedback = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_ShotFeedback"), true);
		UTextBlock* Reticle = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_Reticle"), true);
		if (!ShotFeedback || !Reticle)
		{
			return false;
		}
		ShotFeedback->SetText(LOCTEXT("ShotFeedbackPlaceholder", "정중앙!  +200"));
		ShotFeedback->SetJustification(ETextJustify::Center);
		ShotFeedback->SetVisibility(ESlateVisibility::Collapsed);
		StyleAaaText(*ShotFeedback, 28, Style.HudAmberColor);
		if (UCanvasPanelSlot* FeedbackSlot = PlayCanvas->AddChildToCanvas(ShotFeedback))
		{
			FeedbackSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			FeedbackSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			FeedbackSlot->SetPosition(FVector2D::ZeroVector);
			FeedbackSlot->SetSize(FVector2D(360.0f, 96.0f));
			FeedbackSlot->SetZOrder(10);
		}
		else
		{
			return false;
		}

		Reticle->SetText(LOCTEXT("ReticlePlaceholder", "+"));
		Reticle->SetJustification(ETextJustify::Center);
		Reticle->SetVisibility(ESlateVisibility::Collapsed);
		StyleAaaText(*Reticle, FMath::RoundToInt(Style.ReticleFontSize), Style.ReticleColor);
		if (UCanvasPanelSlot* ReticleSlot = PlayCanvas->AddChildToCanvas(Reticle))
		{
			ReticleSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			ReticleSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			ReticleSlot->SetPosition(FVector2D::ZeroVector);
			ReticleSlot->SetAutoSize(true);
			ReticleSlot->SetZOrder(20);
		}
		else
		{
			return false;
		}

		UTextBlock* RangeLabel = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_RangeLabel"));
		if (!RangeLabel)
		{
			return false;
		}
		RangeLabel->SetText(LOCTEXT("RangeLabel", "WATER SHOT RANGE  //  과녁을 조준하세요"));
		StyleAaaText(*RangeLabel, 11, Style.SecondaryTextColor, TEXT("SemiBold"));
		RangeLabel->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* RangeSlot = PlayCanvas->AddChildToCanvas(RangeLabel))
		{
			RangeSlot->SetAnchors(FAnchors(0.0f, 0.0f));
			RangeSlot->SetAlignment(FVector2D::ZeroVector);
			RangeSlot->SetPosition(FVector2D(18.0f, 14.0f));
			RangeSlot->SetAutoSize(true);
			RangeSlot->SetZOrder(30);
		}
		else
		{
			return false;
		}

		UBorder* TitleCard = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_TitleCard"));
		UVerticalBox* TitleColumn = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Column_Title"));
		UTextBlock* TitleEyebrow = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_TitleEyebrow"));
		UTextBlock* TitleText = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Title"));
		if (!TitleCard || !TitleColumn || !TitleEyebrow || !TitleText)
		{
			return false;
		}
		ConfigureCard(TitleCard, Style.HudPanelColor, FMargin(20.0f, 12.0f));
		TitleEyebrow->SetText(LOCTEXT("TitleEyebrow", "BUSAN WATERFRONT FESTIVAL"));
		StyleAaaText(*TitleEyebrow, 10, Style.HudAccentColor, TEXT("SemiBold"));
		TitleText->SetText(LOCTEXT("Title", "WATER BLAST"));
		StyleAaaText(*TitleText, 27, Style.PrimaryTextColor);
		TitleColumn->AddChildToVerticalBox(TitleEyebrow);
		TitleColumn->AddChildToVerticalBox(TitleText);
		TitleCard->SetContent(TitleColumn);
		if (!PlaceAnchored(TitleCard, FAnchors(0.025f, 0.035f, 0.285f, 0.145f), 30))
		{
			return false;
		}

		UBorder* TimerCard = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_TimerCard"));
		UVerticalBox* TimerColumn = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Column_Timer"));
		USizeBox* TimerGaugeSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Size_TimeGauge"));
		UProgressBar* TimeGauge = MakeWidget<UProgressBar>(
			Blueprint, Tree, TEXT("Progress_Time"), true);
		UBorder* ScoreCard = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_ScoreCard"));
		UBorder* HitCard = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_HitCard"));
		UBorder* PerformanceCard = MakeWidget<UBorder>(
			Blueprint, Tree, TEXT("Border_PerformanceCard"));
		UBorder* StreakCard = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_StreakCard"));
		UTextBlock* RemainingTime = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_RemainingTime"), true);
		UTextBlock* Score = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Score"), true);
		UTextBlock* HitCount = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_HitCount"), true);
		UTextBlock* MissCount = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_MissCount"), true);
		UTextBlock* Streak = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Streak"), true);
		if (!TimerCard || !TimerColumn || !TimerGaugeSize || !TimeGauge
			|| !ScoreCard || !HitCard || !PerformanceCard || !StreakCard
			|| !RemainingTime || !Score || !HitCount || !MissCount || !Streak)
		{
			return false;
		}
		ConfigureCard(TimerCard, Style.HudPanelColor, FMargin(18.0f, 6.0f));
		ConfigureCard(ScoreCard, Style.HudPanelColor, FMargin(18.0f, 10.0f));
		ConfigureCard(HitCard, Style.HudPanelMutedColor, FMargin(16.0f, 12.0f));
		ConfigureCard(PerformanceCard, Style.HudPanelMutedColor, FMargin(16.0f, 12.0f));
		ConfigureCard(StreakCard, Style.HudPanelColor, FMargin(18.0f, 8.0f));
		RemainingTime->SetText(LOCTEXT("Time", "00:30"));
		RemainingTime->SetJustification(ETextJustify::Center);
		StyleAaaText(*RemainingTime, 48, Style.PrimaryTextColor);
		TimeGauge->SetPercent(1.0f);
		TimeGauge->SetFillColorAndOpacity(Style.HudAccentColor);
		TimeGauge->SetVisibility(ESlateVisibility::HitTestInvisible);
		TimerGaugeSize->SetHeightOverride(5.0f);
		TimerGaugeSize->SetContent(TimeGauge);
		if (UVerticalBoxSlot* TimeSlot = TimerColumn->AddChildToVerticalBox(RemainingTime))
		{
			TimeSlot->SetHorizontalAlignment(HAlign_Fill);
			TimeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		if (UVerticalBoxSlot* GaugeSlot = TimerColumn->AddChildToVerticalBox(TimerGaugeSize))
		{
			GaugeSlot->SetHorizontalAlignment(HAlign_Fill);
			GaugeSlot->SetPadding(FMargin(12.0f, 0.0f, 12.0f, 4.0f));
		}
		TimerCard->SetContent(TimerColumn);
		Score->SetText(LOCTEXT("Score", "점수  000000"));
		Score->SetJustification(ETextJustify::Right);
		StyleAaaText(*Score, 27, Style.HudAmberColor);
		HitCount->SetText(LOCTEXT("Hits", "명중  0\n정확도  0%"));
		HitCount->SetAutoWrapText(true);
		StyleAaaText(*HitCount, 16, Style.PrimaryTextColor, TEXT("SemiBold"));
		MissCount->SetText(LOCTEXT("Performance", "초당  0.0발\n반응  0ms\n빗나감  0"));
		MissCount->SetAutoWrapText(true);
		StyleAaaText(*MissCount, 15, Style.SecondaryTextColor, TEXT("SemiBold"));
		Streak->SetText(LOCTEXT("Streak", "연속 x0     최고 0"));
		Streak->SetJustification(ETextJustify::Center);
		StyleAaaText(*Streak, 22, Style.HudAccentColor);
		ScoreCard->SetContent(Score);
		HitCard->SetContent(HitCount);
		PerformanceCard->SetContent(MissCount);
		StreakCard->SetContent(Streak);
		if (!PlaceAnchored(TimerCard, FAnchors(0.405f, 0.025f, 0.605f, 0.150f), 31)
			|| !PlaceAnchored(ScoreCard, FAnchors(0.715f, 0.035f, 0.910f, 0.145f), 30)
			|| !PlaceAnchored(HitCard, FAnchors(0.080f, 0.835f, 0.255f, 0.925f), 30)
			|| !PlaceAnchored(PerformanceCard, FAnchors(0.745f, 0.835f, 0.920f, 0.925f), 30)
			|| !PlaceAnchored(StreakCard, FAnchors(0.665f, 0.175f, 0.900f, 0.235f), 30))
		{
			return false;
		}

		UBorder* HintCard = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_InputHint"));
		UTextBlock* HintText = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_InputHint"));
		if (!HintCard || !HintText)
		{
			return false;
		}
		ConfigureCard(HintCard, Style.HudPanelMutedColor, FMargin(16.0f, 8.0f));
		HintText->SetText(LOCTEXT("InputHint", "마우스로 총기 회전  ·  좌클릭 발사  ·  ESC 나가기"));
		HintText->SetJustification(ETextJustify::Center);
		StyleAaaText(*HintText, 13, Style.SecondaryTextColor, TEXT("SemiBold"));
		HintCard->SetContent(HintText);
		if (!PlaceAnchored(HintCard, FAnchors(0.350f, 0.940f, 0.650f, 0.985f), 30))
		{
			return false;
		}
		HintCard->SetVisibility(ESlateVisibility::Collapsed);

		UBorder* ResultPanel = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Panel_Result"), true);
		UOverlay* ResultStage = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_ResultStage"));
		USizeBox* ResultSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Size_ResultCard"));
		UBorder* ResultFrame = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_ResultFrame"));
		UBorder* ResultSurface = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_ResultSurface"));
		UVerticalBox* ResultColumn = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Column_Result"));
		UTextBlock* ResultEyebrow = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_ResultEyebrow"));
		UTextBlock* ResultText = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Result"), true);
		USizeBox* ResultCtaReserve = MakeWidget<USizeBox>(
			Blueprint, Tree, TEXT("Size_ResultCtaReserve"));
		if (!ResultPanel || !ResultStage || !ResultSize || !ResultFrame || !ResultSurface
			|| !ResultColumn || !ResultEyebrow || !ResultText || !ResultCtaReserve)
		{
			return false;
		}
		ResultPanel->SetBrushColor(FLinearColor(0.002f, 0.010f, 0.024f, 0.88f));
		ResultPanel->SetPadding(FMargin(0.0f));
		ResultPanel->SetVisibility(ESlateVisibility::Collapsed);
		ResultPanel->SetContent(ResultStage);
		ResultSize->SetWidthOverride(720.0f);
		ResultSize->SetHeightOverride(500.0f);
		ResultSize->SetContent(ResultFrame);
		ResultFrame->SetBrushColor(Style.HudAmberColor);
		ResultFrame->SetPadding(FMargin(2.0f));
		ResultFrame->SetContent(ResultSurface);
		ResultSurface->SetBrushColor(Style.HudPanelColor);
		ResultSurface->SetPadding(FMargin(48.0f, 40.0f));
		ResultSurface->SetContent(ResultColumn);
		ResultEyebrow->SetText(LOCTEXT("ResultEyebrow", "WATER BLAST  //  플레이 결과"));
		ResultEyebrow->SetJustification(ETextJustify::Center);
		StyleAaaText(*ResultEyebrow, 13, Style.HudAccentColor, TEXT("SemiBold"));
		ResultText->SetText(LOCTEXT("ResultPlaceholder", "결과를 계산 중입니다"));
		ResultText->SetJustification(ETextJustify::Center);
		ResultText->SetAutoWrapText(true);
		StyleAaaText(*ResultText, 26, Style.PrimaryTextColor);
		if (UVerticalBoxSlot* EyebrowSlot = ResultColumn->AddChildToVerticalBox(ResultEyebrow))
		{
			EyebrowSlot->SetHorizontalAlignment(HAlign_Center);
			EyebrowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));
		}
		if (UVerticalBoxSlot* ResultTextSlot = ResultColumn->AddChildToVerticalBox(ResultText))
		{
			ResultTextSlot->SetHorizontalAlignment(HAlign_Center);
			ResultTextSlot->SetVerticalAlignment(VAlign_Center);
			ResultTextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		ResultCtaReserve->SetHeightOverride(62.0f);
		if (!ResultColumn->AddChildToVerticalBox(ResultCtaReserve))
		{
			return false;
		}
		if (UOverlaySlot* ResultCardSlot = ResultStage->AddChildToOverlay(ResultSize))
		{
			ResultCardSlot->SetHorizontalAlignment(HAlign_Center);
			ResultCardSlot->SetVerticalAlignment(VAlign_Center);
		}
		else
		{
			return false;
		}
		if (!PlaceAnchored(ResultPanel, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), 100))
		{
			return false;
		}

		UButton* CloseButton = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Close"), true);
		UTextBlock* CloseLabel = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CloseLabel"));
		if (!CloseButton || !CloseLabel)
		{
			return false;
		}
		CloseLabel->SetText(LOCTEXT("Close", "나가기  [ESC]"));
		CloseLabel->SetJustification(ETextJustify::Center);
		StyleAaaText(*CloseLabel, 13, Style.PrimaryTextColor, TEXT("SemiBold"));
		CloseButton->SetContent(CloseLabel);
		CloseButton->SetBackgroundColor(FLinearColor(
			Style.HudPanelMutedColor.R,
			Style.HudPanelMutedColor.G,
			Style.HudPanelMutedColor.B,
			1.0f));
		if (!PlaceAnchored(CloseButton, FAnchors(0.925f, 0.050f, 0.982f, 0.110f), 110))
		{
			return false;
		}

		return HasRequiredWidgetTree(Blueprint);
	}

	bool ValidateScreenBlueprint(const UWidgetBlueprint& Blueprint)
	{
		return Blueprint.Status != BS_Error
			&& Blueprint.GeneratedClass
			&& Blueprint.GeneratedClass->IsChildOf(UFF_TargetShootingScreen::StaticClass())
			&& HasRequiredWidgetTree(Blueprint);
	}

	UWidgetBlueprint* EnsureScreenBlueprint()
	{
		// Protect an unsaved, already loaded designer asset too. DoesPackageExist only
		// covers disk packages, while an editor may already own an in-memory package.
		if (UObject* LoadedObject = FindLoadedAssetObject(ScreenPackage))
		{
			UWidgetBlueprint* Existing = Cast<UWidgetBlueprint>(LoadedObject);
			if (!Existing || !ValidateScreenBlueprint(*Existing))
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_FAIL: loaded screen asset violates the parent/widget contract: %s"),
					LogPrefix, *ScreenPackage);
				return nullptr;
			}

			UE_LOG(LogFFUIWbpBuilder, Display,
				TEXT("%s_REUSE: %s is already loaded; designer changes were preserved."),
				LogPrefix, *ScreenPackage);
			return Existing;
		}

		UWidgetBlueprint* Blueprint = EnsureWidgetBlueprint(
			LogPrefix,
			ScreenPackage,
			ScreenFolder,
			*UFF_TargetShootingScreen::StaticClass(),
			[](UWidgetBlueprint& NewBlueprint, UWidgetTree& Tree)
			{
				return BuildScreenLayout(NewBlueprint, Tree);
			});

		if (!Blueprint || !ValidateScreenBlueprint(*Blueprint))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: screen parent/widget contract validation failed: %s"),
				LogPrefix, *ScreenPackage);
			return nullptr;
		}
		return Blueprint;
	}

	void RebuildTargetShootingUI()
	{
		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_REBUILD_BEGIN: explicitly replaces only the existing target-shooting WBP."),
			LogPrefix);

		// This command is intentionally not an ensure path. A missing screen must be
		// created through FF.BuildTargetShootingAssets so the two operations cannot be
		// confused in automation or by a designer reading the log.
		if (!FPackageName::DoesPackageExist(ScreenPackage))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REBUILD_FAIL: %s does not exist on disk; nothing was modified."),
				LogPrefix, *ScreenPackage);
			return;
		}

		UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(
			LoadExistingAssetObject(ScreenPackage));
		if (!Blueprint)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REBUILD_FAIL: existing package is not a WidgetBlueprint: %s."),
				LogPrefix, *ScreenPackage);
			return;
		}

		UPackage* Package = Blueprint->GetOutermost();
		if (!Package || Package->IsDirty())
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REBUILD_FAIL: %s has unsaved edits; save or revert them first. "
					 "The WBP was not modified."),
				LogPrefix, *ScreenPackage);
			return;
		}
		if (GEditor && GEditor->PlayWorld)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REBUILD_FAIL: stop PIE before rebuilding %s."),
				LogPrefix, *ScreenPackage);
			return;
		}
		if (!Blueprint->ParentClass
			|| !Blueprint->ParentClass->IsChildOf(UFF_TargetShootingScreen::StaticClass()))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REBUILD_FAIL: %s has an unexpected parent; nothing was modified."),
				LogPrefix, *ScreenPackage);
			return;
		}
		if (Blueprint->Animations.Num() > 0)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REBUILD_FAIL: %s owns %d Widget Animation(s). The command backs up "
					 "only the tree, bindings and variable GUIDs, so the WBP was not modified."),
				LogPrefix, *ScreenPackage, Blueprint->Animations.Num());
			return;
		}

		const FString Filename = FPackageName::LongPackageNameToFilename(
			ScreenPackage, FPackageName::GetAssetPackageExtension());
		if (IFileManager::Get().IsReadOnly(*Filename))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REBUILD_FAIL: %s is read-only; nothing was modified."),
				LogPrefix, *Filename);
			return;
		}

		UWidgetTree* OriginalTree = Blueprint->WidgetTree;
		const FName OriginalTreeName = OriginalTree
			? OriginalTree->GetFName()
			: FName(TEXT("WidgetTree"));
		const bool bOriginalTreeWasTransient = OriginalTree
			&& OriginalTree->HasAnyFlags(RF_Transient);
		const auto OriginalBindings = Blueprint->Bindings;
		const auto OriginalVariableMap = Blueprint->WidgetVariableNameToGuidMap;

		if (OriginalTree)
		{
			const FName DiscardedName = MakeUniqueObjectName(
				Blueprint,
				UWidgetTree::StaticClass(),
				TEXT("DiscardedWidgetTree"));
			if (!OriginalTree->Rename(
				*DiscardedName.ToString(),
				Blueprint,
				REN_DontCreateRedirectors | REN_NonTransactional))
			{
				Package->SetDirtyFlag(false);
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_REBUILD_FAIL: could not stage the existing WidgetTree; "
						 "nothing was modified."),
					LogPrefix);
				return;
			}
			OriginalTree->SetFlags(RF_Transient);
		}

		Blueprint->Modify();
		Blueprint->Bindings.Reset();
		Blueprint->WidgetVariableNameToGuidMap.Reset();
		Blueprint->WidgetTree = NewObject<UWidgetTree>(
			Blueprint,
			UWidgetTree::StaticClass(),
			TEXT("WidgetTree"),
			RF_Transactional);

		auto Rollback = [&]() -> bool
		{
			UWidgetTree* FailedTree = Blueprint->WidgetTree;
			if (FailedTree && FailedTree != OriginalTree)
			{
				const FName FailedName = MakeUniqueObjectName(
					Blueprint,
					UWidgetTree::StaticClass(),
					TEXT("FailedWidgetTree"));
				if (!FailedTree->Rename(
					*FailedName.ToString(),
					Blueprint,
					REN_DontCreateRedirectors | REN_NonTransactional))
				{
					return false;
				}
				FailedTree->SetFlags(RF_Transient);
			}

			if (OriginalTree)
			{
				OriginalTree->ClearFlags(RF_Transient);
				if (!OriginalTree->Rename(
					*OriginalTreeName.ToString(),
					Blueprint,
					REN_DontCreateRedirectors | REN_NonTransactional))
				{
					return false;
				}
				if (bOriginalTreeWasTransient)
				{
					OriginalTree->SetFlags(RF_Transient);
				}
			}

			Blueprint->WidgetTree = OriginalTree;
			Blueprint->Bindings = OriginalBindings;
			Blueprint->WidgetVariableNameToGuidMap = OriginalVariableMap;
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			FKismetEditorUtilities::CompileBlueprint(Blueprint);
			Package->SetDirtyFlag(false);
			return true;
		};

		const bool bLayoutBuilt = Blueprint->WidgetTree
			&& BuildScreenLayout(*Blueprint, *Blueprint->WidgetTree)
			&& HasRequiredWidgetTree(*Blueprint);
		if (!bLayoutBuilt)
		{
			const bool bRolledBack = Rollback();
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REBUILD_FAIL: layout construction failed; previous in-memory tree %s."),
				LogPrefix, bRolledBack ? TEXT("was restored") : TEXT("could not be restored"));
			return;
		}

		// Compile once before saving so a bad generated tree can be rolled back while
		// the disk package is still untouched. SaveNewBlueprint performs its own final
		// orphan check and compile immediately before SavePackage.
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		if (!ValidateScreenBlueprint(*Blueprint))
		{
			const bool bRolledBack = Rollback();
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REBUILD_FAIL: rebuilt WBP did not compile or validate; previous tree %s."),
				LogPrefix, bRolledBack ? TEXT("was restored") : TEXT("could not be restored"));
			return;
		}
		if (UFF_TargetShootingScreen* RebuiltDefaults = Cast<UFF_TargetShootingScreen>(
			Blueprint->GeneratedClass->GetDefaultObject()))
		{
			RebuiltDefaults->Modify();
			RebuiltDefaults->VisualStyle =
				GetDefault<UFF_TargetShootingScreen>()->VisualStyle;
		}

		const bool bSavedRebuild = SaveNewBlueprint(LogPrefix, *Blueprint);
		if (!bSavedRebuild || !ValidateScreenBlueprint(*Blueprint))
		{
			const bool bRolledBack = Rollback();
			const bool bRecoveredOriginal = bRolledBack
				&& SaveNewBlueprint(TEXT("FF_TARGET_SHOOTING_UI_RECOVERY"), *Blueprint);
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REBUILD_FAIL: compile/save/validation failed; previous tree rollback=%s, "
					 "recovery save=%s."),
				LogPrefix,
				bRolledBack ? TEXT("OK") : TEXT("FAILED"),
				bRecoveredOriginal ? TEXT("OK") : TEXT("FAILED"));
			return;
		}

		if (!ValidateWaterBlasterDefinition())
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REBUILD_FAIL: WBP was saved, but water-blaster gameplay tuning failed."),
				LogPrefix);
			return;
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_REBUILD_OK: %s now uses the water-blaster target-shooting layout and tuning."),
			LogPrefix, *ScreenPackage);
	}

	bool SaveNewDataAsset(UFF_TargetShootingDefinition& Definition)
	{
		Definition.MarkPackageDirty();
		UPackage* Package = Definition.GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);

		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, &Definition, *Filename, Args);
	}

	bool ValidateDefinition(const UFF_TargetShootingDefinition& Definition)
	{
		TArray<FText> ValidationErrors;
		const UClass* ScreenClass = Definition.ScreenClass.LoadSynchronous();
		return Definition.IsDefinitionValid(ValidationErrors)
			&& ValidationErrors.IsEmpty()
			&& Definition.MinigameId == TargetShootingId
			&& Definition.CompatibleFacilityDefinitionIds.Contains(ShootingFacilityId)
			&& Definition.RuntimeClass.Get() == UFF_TargetShootingRuntime::StaticClass()
			&& ScreenClass
			&& ScreenClass->IsChildOf(UFF_TargetShootingScreen::StaticClass());
	}

	UFF_TargetShootingDefinition* EnsureDefinition(UClass& ResolvedScreenClass)
	{
		const bool bExistsOnDisk = FPackageName::DoesPackageExist(DefinitionPackage);
		UObject* ExistingObject = LoadExistingAssetObject(DefinitionPackage);
		if (bExistsOnDisk || ExistingObject)
		{
			UFF_TargetShootingDefinition* Existing = Cast<UFF_TargetShootingDefinition>(ExistingObject);
			if (!Existing || !ValidateDefinition(*Existing))
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_FAIL: existing definition violates the target-shooting contract: %s. "
						 "The asset was not modified."),
					LogPrefix, *DefinitionPackage);
				return nullptr;
			}

			UE_LOG(LogFFUIWbpBuilder, Display,
				TEXT("%s_REUSE: %s already exists; designer changes were preserved."),
				LogPrefix, *DefinitionPackage);
			return Existing;
		}

		UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
		Factory->DataAssetClass = UFF_TargetShootingDefinition::StaticClass();
		FAssetToolsModule& AssetTools =
			FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		UFF_TargetShootingDefinition* Definition = Cast<UFF_TargetShootingDefinition>(
			AssetTools.Get().CreateAsset(
				FPackageName::GetLongPackageAssetName(DefinitionPackage),
				DefinitionFolder,
				UFF_TargetShootingDefinition::StaticClass(),
				Factory));
		if (!Definition)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: could not create %s."), LogPrefix, *DefinitionPackage);
			return nullptr;
		}

		Definition->MinigameId = TargetShootingId;
		Definition->DisplayName = LOCTEXT("TargetShootingDisplayName", "부산 바다 물총 사격");
		Definition->CompatibleFacilityDefinitionIds = { ShootingFacilityId };
		Definition->DurationSeconds = 30.0f;
		Definition->ScoreForMaxReward = 2000;
		Definition->RewardGoldAtMaxScore = 150;
		Definition->RuntimeClass = UFF_TargetShootingRuntime::StaticClass();
		Definition->ScreenClass = &ResolvedScreenClass;

		if (!ValidateDefinition(*Definition) || !SaveNewDataAsset(*Definition))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: definition validation/save failed: %s."),
				LogPrefix, *DefinitionPackage);
			return nullptr;
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_CREATE: %s created."), LogPrefix, *DefinitionPackage);
		return Definition;
	}

	void BuildTargetShootingAssets()
	{
		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_BEGIN: creates only missing WBP/DataAsset packages."), LogPrefix);

		UWidgetBlueprint* ScreenBlueprint = EnsureScreenBlueprint();
		if (!ScreenBlueprint || !ScreenBlueprint->GeneratedClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: screen asset is unavailable; definition was not created."), LogPrefix);
			return;
		}

		UClass* GeneratedScreenClass = ScreenBlueprint->GeneratedClass.Get();
		if (!GeneratedScreenClass || !EnsureDefinition(*GeneratedScreenClass))
		{
			return;
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_OK: target-shooting WBP and definition are ready."), LogPrefix);
	}

	FAutoConsoleCommand BuildTargetShootingAssetsCommand(
		TEXT("FF.BuildTargetShootingAssets"),
		TEXT("Creates the target-shooting WBP and DataAsset only when their packages are missing."),
		FConsoleCommandDelegate::CreateStatic(&BuildTargetShootingAssets));

	FAutoConsoleCommand RebuildTargetShootingUICommand(
		TEXT("FF.RebuildTargetShootingUI"),
		TEXT("Replaces only the existing clean target-shooting WBP with the AAA layout; "
			 "refuses missing/dirty/animated assets and never modifies the DataAsset or gameplay rules."),
		FConsoleCommandDelegate::CreateStatic(&RebuildTargetShootingUI));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
