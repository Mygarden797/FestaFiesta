// Copyright Epic Games, Inc. All Rights Reserved.

// Free-ride Surfing WBP authoring and one-way ScreenClass migration.
//
// FF.RebuildSurfingUI is the explicit opt-in exception: it replaces only the
// clean, existing FreeRide WBP tree with the authored AAA layout. It never
// loads or mutates the Definition, course table or gameplay rules.
//
// The legacy and Edge WBP packages are rollback assets and are never renamed,
// deleted, dirtied or saved here. The command creates only the new FreeRide WBP
// and course-table packages, validates every
// package before mutation, then changes only the existing definition's soft
// ScreenClass reference. Re-running the command after migration performs no save.

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Factories/DataTableFactory.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Minigame/FF_MinigameDefinition.h"
#include "Minigame/FF_SurfingRuntime.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Minigame/FF_MinigameUIStyle.h"
#include "UI/Minigame/FF_SurfingScreen.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "FFSurfingFreeRideAssetBuilder"

namespace FFSurfingWbpBuilder
{
	using namespace FFUIWbpBuilder;

	const TCHAR* const LogPrefix = TEXT("FF_SURFING_FREERIDE_ASSETS");
	const FString ScreenFolder(TEXT("/Game/FestaFiesta/UI/Minigame"));
	const FString LegacyScreenPackage(ScreenFolder / TEXT("WBP_FF_Surfing"));
	const FString EdgeScreenPackage(ScreenFolder / TEXT("WBP_FF_Surfing_Edge"));
	const FString FreeRideScreenPackage(ScreenFolder / TEXT("WBP_FF_Surfing_FreeRide"));
	const FString DefinitionPackage(TEXT("/Game/FestaFiesta/Minigame/Data/DA_Minigame_Surfing"));
	const FString CourseTableFolder(TEXT("/Game/FestaFiesta/Minigame/Data"));
	const FString CourseTablePackage(CourseTableFolder / TEXT("DT_SurfingCourse"));
	const FString CourseTableObjectPath(
		TEXT("/Game/FestaFiesta/Minigame/Data/DT_SurfingCourse.DT_SurfingCourse"));
	const FString LegacyScreenClassPath(
		TEXT("/Game/FestaFiesta/UI/Minigame/WBP_FF_Surfing.WBP_FF_Surfing_C"));
	const FString EdgeScreenClassPath(
		TEXT("/Game/FestaFiesta/UI/Minigame/WBP_FF_Surfing_Edge.WBP_FF_Surfing_Edge_C"));
	const FString FreeRideScreenClassPath(
		TEXT("/Game/FestaFiesta/UI/Minigame/WBP_FF_Surfing_FreeRide.WBP_FF_Surfing_FreeRide_C"));
	const TCHAR* const BackdropTextureObjectPath =
		TEXT("/Game/FestaFiesta/UI/Minigame/Final/T_MG_Surfing_Backdrop_Final."
			 "T_MG_Surfing_Backdrop_Final");
	const TCHAR* const SurferTextureObjectPath =
		TEXT("/Game/FestaFiesta/UI/Minigame/Final/T_MG_Surfing_Surfer_Final."
			 "T_MG_Surfing_Surfer_Final");
	const FName SurfingId(TEXT("Surfing"));
	const FName SurfingFacilityId(TEXT("Booth_ActivityGame_Surfing"));

	enum class EScreenClassDisposition : uint8
	{
		LegacyWbp,
		EdgeWbp,
		FreeRideWbp,
		NativeFallback,
		Unexpected,
	};

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

	bool IsFullStretchCanvasSlot(const UCanvasPanelSlot* Slot)
	{
		if (!Slot)
		{
			return false;
		}

		const FAnchors Anchors = Slot->GetAnchors();
		const FMargin Offsets = Slot->GetOffsets();
		return Anchors.Minimum.Equals(FVector2D::ZeroVector)
			&& Anchors.Maximum.Equals(FVector2D(1.0f, 1.0f))
			&& FMath::IsNearlyZero(Offsets.Left)
			&& FMath::IsNearlyZero(Offsets.Top)
			&& FMath::IsNearlyZero(Offsets.Right)
			&& FMath::IsNearlyZero(Offsets.Bottom);
	}

	bool HasRequiredWidgetTree(const UWidgetBlueprint& Blueprint)
	{
		const UWidgetTree* Tree = Blueprint.WidgetTree;
		if (!Tree || !Tree->RootWidget)
		{
			return false;
		}

		const UCanvasPanel* SurfArea = Cast<UCanvasPanel>(Tree->FindWidget(TEXT("Canvas_SurfArea")));
		const UCanvasPanel* EntityLayer = Cast<UCanvasPanel>(Tree->FindWidget(TEXT("Canvas_EntityLayer")));
		const UBorder* SurferAvatar = Cast<UBorder>(Tree->FindWidget(TEXT("Border_SurferAvatar")));
		const UTextBlock* RemainingTime = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_RemainingTime")));
		const UTextBlock* Score = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_Score")));
		const UTextBlock* Lives = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_Lives")));
		const UTextBlock* Boost = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_Boost")));
		const UTextBlock* Controls = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_Controls")));
		const UBorder* ResultPanel = Cast<UBorder>(Tree->FindWidget(TEXT("Panel_Result")));
		const UTextBlock* ResultText = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_Result")));
		const UButton* CloseButton = Cast<UButton>(Tree->FindWidget(TEXT("Button_Close")));
		const UButton* ResultCloseButton = Cast<UButton>(
			Tree->FindWidget(TEXT("Button_ResultClose")));
		const UTextBlock* CloseLabel = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_CloseLabel")));
		const UTextBlock* ResultCloseLabel = Cast<UTextBlock>(
			Tree->FindWidget(TEXT("Text_ResultCloseLabel")));
		const UBorder* PillHud = Cast<UBorder>(Tree->FindWidget(TEXT("Border_PillHud")));
		const UBorder* ControlsHint = Cast<UBorder>(Tree->FindWidget(TEXT("Border_ControlsHint")));
		const UBorder* Backdrop = Cast<UBorder>(Tree->FindWidget(TEXT("Border_SurfBackdrop")));
		const UBorder* SurferCore = Cast<UBorder>(Tree->FindWidget(TEXT("Border_SurferCore")));
		const UBorder* IdentityCard = Cast<UBorder>(Tree->FindWidget(TEXT("Border_GameIdentity")));
		const UBorder* ScoreCard = Cast<UBorder>(Tree->FindWidget(TEXT("Border_ScoreCard")));
		const UBorder* LivesCard = Cast<UBorder>(Tree->FindWidget(TEXT("Border_LivesCombatCard")));
		const UBorder* BoostCard = Cast<UBorder>(Tree->FindWidget(TEXT("Border_BoostCombatCard")));
		const UBorder* ResultDimmer = Cast<UBorder>(Tree->FindWidget(TEXT("Border_ResultDimmer")));
		const UBorder* ResultCard = Cast<UBorder>(Tree->FindWidget(TEXT("Border_ResultCard")));
		const UOverlay* ResultStage = Cast<UOverlay>(Tree->FindWidget(TEXT("Overlay_ResultStage")));
		const UOverlay* Root = Cast<UOverlay>(Tree->RootWidget);

		const UWidget* RequiredVariables[] = {
			SurfArea,
			EntityLayer,
			SurferAvatar,
			RemainingTime,
			Score,
			Lives,
			Boost,
			Controls,
			ResultPanel,
			ResultText,
			CloseButton,
			ResultCloseButton,
		};
		for (const UWidget* Widget : RequiredVariables)
		{
			if (!IsVariableWidget(Blueprint, Widget))
			{
				return false;
			}
		}

		const FName RemovedEdgeWidgets[] = {
			TEXT("Text_Distance"),
			TEXT("Text_Status"),
			TEXT("Button_SteerLeft"),
			TEXT("Button_Boost"),
			TEXT("Button_SteerRight"),
			TEXT("Button_MoveUp"),
			TEXT("Button_MoveDown"),
		};
		for (const FName RemovedName : RemovedEdgeWidgets)
		{
			if (Tree->FindWidget(RemovedName))
			{
				return false;
			}
		}

		if (SurfArea->GetClipping() != EWidgetClipping::ClipToBoundsAlways
			|| EntityLayer->GetParent() != SurfArea
			|| EntityLayer->GetVisibility() != ESlateVisibility::HitTestInvisible
			|| !IsFullStretchCanvasSlot(Cast<UCanvasPanelSlot>(EntityLayer->Slot))
			|| SurferAvatar->GetVisibility() != ESlateVisibility::HitTestInvisible
			|| Controls->GetVisibility() != ESlateVisibility::HitTestInvisible
			|| !PillHud
			|| PillHud->GetParent() != SurfArea
			|| !ControlsHint
			|| ControlsHint->GetParent() != SurfArea
			|| Controls->GetParent() != ControlsHint
			|| !Backdrop
			|| Backdrop->GetParent() != SurfArea
			|| !SurferCore
			|| SurferCore->GetParent() != SurferAvatar
			|| !IdentityCard
			|| IdentityCard->GetParent() != SurfArea
			|| !ScoreCard
			|| ScoreCard->GetParent() != SurfArea
			|| !LivesCard
			|| LivesCard->GetParent() != SurfArea
			|| !BoostCard
			|| BoostCard->GetParent() != SurfArea
			|| !ResultDimmer
			|| ResultDimmer->GetParent() != SurfArea
			|| ResultDimmer->GetVisibility() != ESlateVisibility::Collapsed
			|| !ResultCard
			|| ResultCard->GetVisibility() != ESlateVisibility::SelfHitTestInvisible
			|| !ResultStage
			|| ResultStage->GetVisibility() != ESlateVisibility::SelfHitTestInvisible
			|| !Root
			|| ResultPanel->GetParent() != Root
			|| !CloseLabel
			|| CloseLabel->GetParent() != CloseButton
			|| !ResultCloseLabel
			|| ResultCloseLabel->GetParent() != ResultCloseButton)
		{
			return false;
		}

		const UCanvasPanelSlot* EntityLayerSlot = Cast<UCanvasPanelSlot>(EntityLayer->Slot);
		const UCanvasPanelSlot* AvatarSlot = Cast<UCanvasPanelSlot>(SurferAvatar->Slot);
		if (!EntityLayerSlot || !AvatarSlot)
		{
			return false;
		}

		const UCanvasPanelSlot* PillSlot = Cast<UCanvasPanelSlot>(PillHud->Slot);
		const UCanvasPanelSlot* ControlsHintSlot = Cast<UCanvasPanelSlot>(ControlsHint->Slot);
		const UCanvasPanelSlot* CloseSlot = Cast<UCanvasPanelSlot>(CloseButton->Slot);
		const UCanvasPanelSlot* ResultDimmerSlot = Cast<UCanvasPanelSlot>(ResultDimmer->Slot);
		if (!PillSlot || !ControlsHintSlot || !CloseSlot || !ResultDimmerSlot
			|| !IsFullStretchCanvasSlot(ResultDimmerSlot)
			|| PillSlot->GetZOrder() <= EntityLayerSlot->GetZOrder()
			|| ControlsHintSlot->GetZOrder() <= EntityLayerSlot->GetZOrder()
			|| CloseSlot->GetZOrder() <= EntityLayerSlot->GetZOrder()
			|| CloseSlot->GetZOrder() <= ResultDimmerSlot->GetZOrder())
		{
			return false;
		}

		if (SurferAvatar->GetParent() == SurfArea)
		{
			if (AvatarSlot->GetZOrder() <= EntityLayerSlot->GetZOrder())
			{
				return false;
			}
		}
		else if (SurferAvatar->GetParent() == EntityLayer)
		{
			if (AvatarSlot->GetZOrder() <= 0)
			{
				return false;
			}
		}
		else
		{
			return false;
		}

		return ResultPanel->GetVisibility() == ESlateVisibility::Collapsed;
	}

	UCanvasPanelSlot* AddCanvasChild(
		UCanvasPanel& Canvas,
		UWidget& Child,
		const FAnchors& Anchors,
		const FVector2D& Alignment,
		const FMargin& Offsets,
		const int32 ZOrder)
	{
		UCanvasPanelSlot* Slot = Canvas.AddChildToCanvas(&Child);
		if (!Slot)
		{
			return nullptr;
		}

		Slot->SetAnchors(Anchors);
		Slot->SetAlignment(Alignment);
		Slot->SetOffsets(Offsets);
		Slot->SetZOrder(ZOrder);
		return Slot;
	}

	bool BuildScreenLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const FFMinigameUIStyle::FTheme Theme = FFMinigameUIStyle::GetTheme();
		UTexture2D* BackdropTexture = LoadObject<UTexture2D>(
			nullptr, BackdropTextureObjectPath);
		UTexture2D* SurferTexture = LoadObject<UTexture2D>(
			nullptr, SurferTextureObjectPath);
		if (!BackdropTexture || !SurferTexture)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: production 2.5D artwork is missing: backdrop=%s surfer=%s"),
				LogPrefix,
				BackdropTexture ? TEXT("OK") : BackdropTextureObjectPath,
				SurferTexture ? TEXT("OK") : SurferTextureObjectPath);
			return false;
		}
		UOverlay* Root = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Root"));
		UCanvasPanel* SurfArea = MakeWidget<UCanvasPanel>(
			Blueprint, Tree, TEXT("Canvas_SurfArea"), true);
		UBorder* Backdrop = MakeWidget<UBorder>(
			Blueprint, Tree, TEXT("Border_SurfBackdrop"));
		if (!Root || !SurfArea || !Backdrop)
		{
			return false;
		}

		Tree.RootWidget = Root;
		SurfArea->SetClipping(EWidgetClipping::ClipToBoundsAlways);
		if (UOverlaySlot* SurfSlot = Root->AddChildToOverlay(SurfArea))
		{
			SurfSlot->SetHorizontalAlignment(HAlign_Fill);
			SurfSlot->SetVerticalAlignment(VAlign_Fill);
		}

		Backdrop->SetBrushColor(FLinearColor(0.015f, 0.12f, 0.20f, 1.0f));
		Backdrop->SetVisibility(ESlateVisibility::HitTestInvisible);
		FSlateBrush BackdropBrush;
		BackdropBrush.DrawAs = ESlateBrushDrawType::Image;
		BackdropBrush.SetResourceObject(BackdropTexture);
		BackdropBrush.TintColor = FSlateColor(FLinearColor(0.84f, 0.98f, 1.0f, 1.0f));
		Backdrop->SetBrush(BackdropBrush);
		if (!AddCanvasChild(
			*SurfArea,
			*Backdrop,
			FAnchors(0.0f, 0.0f, 1.0f, 1.0f),
			FVector2D::ZeroVector,
			FMargin(0.0f),
			0))
		{
			return false;
		}

		UCanvasPanel* EntityLayer = MakeWidget<UCanvasPanel>(
			Blueprint, Tree, TEXT("Canvas_EntityLayer"), true);
		if (!EntityLayer)
		{
			return false;
		}
		EntityLayer->SetVisibility(ESlateVisibility::HitTestInvisible);
		EntityLayer->SetClipping(EWidgetClipping::ClipToBoundsAlways);
		if (!AddCanvasChild(
			*SurfArea,
			*EntityLayer,
			FAnchors(0.0f, 0.0f, 1.0f, 1.0f),
			FVector2D::ZeroVector,
			FMargin(0.0f),
			20))
		{
			return false;
		}

		UBorder* SurferAvatar = MakeWidget<UBorder>(
			Blueprint, Tree, TEXT("Border_SurferAvatar"), true);
		UBorder* SurferCore = MakeWidget<UBorder>(
			Blueprint, Tree, TEXT("Border_SurferCore"));
		UVerticalBox* AvatarColumn = MakeWidget<UVerticalBox>(
			Blueprint, Tree, TEXT("VerticalBox_SurferAvatar"));
		UTextBlock* RiderMark = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_SurferRider"));
		UTextBlock* BoardMark = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_SurfboardMark"));
		if (!SurferAvatar || !SurferCore || !AvatarColumn || !RiderMark || !BoardMark)
		{
			return false;
		}

		SurferAvatar->SetBrushColor(FLinearColor::Transparent);
		SurferAvatar->SetPadding(FMargin(0.0f));
		SurferAvatar->SetVisibility(ESlateVisibility::HitTestInvisible);
		SurferAvatar->SetContent(SurferCore);
		FSlateBrush SurferBrush;
		SurferBrush.DrawAs = ESlateBrushDrawType::Image;
		SurferBrush.SetResourceObject(SurferTexture);
		SurferBrush.SetImageSize(FVector2D(150.0f, 225.0f));
		SurferBrush.TintColor = FSlateColor(FLinearColor::White);
		SurferCore->SetBrush(SurferBrush);
		SurferCore->SetBrushColor(FLinearColor::White);
		SurferCore->SetPadding(FMargin(75.0f, 112.5f));
		SurferCore->SetContent(AvatarColumn);
		AvatarColumn->SetVisibility(ESlateVisibility::Collapsed);
		RiderMark->SetText(LOCTEXT("SurferRider", "▲"));
		RiderMark->SetJustification(ETextJustify::Center);
		StyleText(*RiderMark, 22, FLinearColor(0.015f, 0.06f, 0.09f, 1.0f), TEXT("Bold"));
		AvatarColumn->AddChildToVerticalBox(RiderMark);
		BoardMark->SetText(LOCTEXT("SurfboardMark", "RIDER"));
		BoardMark->SetJustification(ETextJustify::Center);
		StyleText(*BoardMark, 12, FLinearColor(0.015f, 0.06f, 0.09f, 1.0f), TEXT("Bold"));
		AvatarColumn->AddChildToVerticalBox(BoardMark);
		if (!AddCanvasChild(
			*SurfArea,
			*SurferAvatar,
			FAnchors(0.5f, 0.51f),
			FVector2D(0.5f, 0.5f),
			FMargin(0.0f, 0.0f, 150.0f, 225.0f),
			100))
		{
			return false;
		}

		const FLinearColor HudPanel = FFMinigameUIStyle::WithOpacity(Theme.Surface, 0.96f);
		const FLinearColor Mint = Theme.HeaderStrong;
		const FLinearColor Amber = Theme.Warning;
		auto StyleAaaText = [](UTextBlock& Text, const int32 Size,
			const FLinearColor& Color, const FName Typeface)
		{
			StyleText(Text, Size, Color, Typeface);
			Text.SetShadowOffset(FVector2D(1.5f, 2.0f));
			Text.SetShadowColorAndOpacity(FLinearColor(0.0f, 0.02f, 0.04f, 0.88f));
		};

		UBorder* IdentityCard = MakeWidget<UBorder>(
			Blueprint, Tree, TEXT("Border_GameIdentity"));
		UVerticalBox* IdentityColumn = MakeWidget<UVerticalBox>(
			Blueprint, Tree, TEXT("VerticalBox_GameIdentity"));
		UTextBlock* IdentityEyebrow = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_IdentityEyebrow"));
		UTextBlock* IdentityTitle = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_IdentityTitle"));
		UTextBlock* IdentitySubtitle = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_IdentitySubtitle"));
		if (!IdentityCard || !IdentityColumn || !IdentityEyebrow
			|| !IdentityTitle || !IdentitySubtitle)
		{
			return false;
		}
		FFMinigameUIStyle::ApplyCard(
			*IdentityCard, Theme.Header, FMargin(18.0f, 10.0f), Theme.HeaderStrong);
		IdentityCard->SetVisibility(ESlateVisibility::HitTestInvisible);
		IdentityCard->SetContent(IdentityColumn);
		IdentityEyebrow->SetText(LOCTEXT("IdentityEyebrow", "FESTA SPORTS  //  LIVE"));
		IdentityTitle->SetText(LOCTEXT("IdentityTitle", "TIDAL RUN"));
		IdentitySubtitle->SetText(LOCTEXT("IdentitySubtitle", "DADAEPO  /  FREE RIDE"));
		StyleAaaText(*IdentityEyebrow, 11, Theme.AccentSoft, TEXT("SemiBold"));
		StyleAaaText(*IdentityTitle, 23, FLinearColor::White, TEXT("Bold"));
		StyleAaaText(*IdentitySubtitle, 10, Theme.AccentSoft, TEXT("SemiBold"));
		IdentityColumn->AddChildToVerticalBox(IdentityEyebrow);
		IdentityColumn->AddChildToVerticalBox(IdentityTitle);
		IdentityColumn->AddChildToVerticalBox(IdentitySubtitle);
		if (!AddCanvasChild(*SurfArea, *IdentityCard,
			FAnchors(0.025f, 0.026f), FVector2D(0.0f, 0.0f),
			FMargin(0.0f, 0.0f, 330.0f, 98.0f), 150))
		{
			return false;
		}

		UBorder* PillHud = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_PillHud"));
		UVerticalBox* TimerColumn = MakeWidget<UVerticalBox>(
			Blueprint, Tree, TEXT("VerticalBox_TimerCard"));
		UTextBlock* TimerCaption = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_TimerCaption"));
		UTextBlock* RemainingTime = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_RemainingTime"), true);
		if (!PillHud || !TimerColumn || !TimerCaption || !RemainingTime)
		{
			return false;
		}
		FFMinigameUIStyle::ApplyCard(
			*PillHud, HudPanel, FMargin(28.0f, 7.0f), Theme.Line);
		PillHud->SetVisibility(ESlateVisibility::HitTestInvisible);
		PillHud->SetContent(TimerColumn);
		TimerCaption->SetText(LOCTEXT("TimeCaption", "ROUND TIMER"));
		TimerCaption->SetJustification(ETextJustify::Center);
		StyleAaaText(*TimerCaption, 11, Mint, TEXT("SemiBold"));
		TimerColumn->AddChildToVerticalBox(TimerCaption);
		RemainingTime->SetText(LOCTEXT("TimePlaceholder", "30"));
		RemainingTime->SetJustification(ETextJustify::Center);
		StyleAaaText(*RemainingTime, 48, Theme.Text, TEXT("Bold"));
		TimerColumn->AddChildToVerticalBox(RemainingTime);
		if (!AddCanvasChild(
			*SurfArea,
			*PillHud,
			FAnchors(0.5f, 0.022f),
			FVector2D(0.5f, 0.0f),
			FMargin(0.0f, 0.0f, 244.0f, 98.0f),
			200))
		{
			return false;
		}

		auto BuildMetricCard = [&](const FName CardName, const FName ColumnName,
			const FName CaptionName, const FName ValueName, const FText& Caption,
			const FText& Placeholder, const FLinearColor& ValueColor,
			const FVector2D& Anchor, const FVector2D& Alignment,
			const FVector2D& Size, const int32 ValueFontSize) -> UTextBlock*
		{
			UBorder* Card = MakeWidget<UBorder>(Blueprint, Tree, CardName);
			UVerticalBox* Column = MakeWidget<UVerticalBox>(Blueprint, Tree, ColumnName);
			UTextBlock* CaptionText = MakeWidget<UTextBlock>(Blueprint, Tree, CaptionName);
			UTextBlock* ValueText = MakeWidget<UTextBlock>(Blueprint, Tree, ValueName, true);
			if (!Card || !Column || !CaptionText || !ValueText)
			{
				return nullptr;
			}
			FFMinigameUIStyle::ApplyCard(
				*Card, HudPanel, FMargin(18.0f, 9.0f), Theme.Line);
			Card->SetVisibility(ESlateVisibility::HitTestInvisible);
			Card->SetContent(Column);
			CaptionText->SetText(Caption);
			CaptionText->SetJustification(ETextJustify::Center);
			StyleAaaText(*CaptionText, 11, Mint, TEXT("SemiBold"));
			Column->AddChildToVerticalBox(CaptionText);
			ValueText->SetText(Placeholder);
			ValueText->SetJustification(ETextJustify::Center);
			StyleAaaText(*ValueText, ValueFontSize, ValueColor, TEXT("Bold"));
			Column->AddChildToVerticalBox(ValueText);
			return AddCanvasChild(*SurfArea, *Card,
				FAnchors(Anchor.X, Anchor.Y), Alignment,
				FMargin(0.0f, 0.0f, Size.X, Size.Y), 150)
				? ValueText
				: nullptr;
		};

		if (!BuildMetricCard(
				TEXT("Border_ScoreCard"), TEXT("VerticalBox_ScoreCard"),
				TEXT("Text_ScoreCaption"), TEXT("Text_Score"),
				LOCTEXT("ScoreCaption", "RUN SCORE"), LOCTEXT("ScorePlaceholder", "0"),
				Amber, FVector2D(0.952f, 0.026f), FVector2D(1.0f, 0.0f),
				FVector2D(244.0f, 94.0f), 34)
			|| !BuildMetricCard(
				TEXT("Border_LivesCombatCard"), TEXT("VerticalBox_LivesCombatCard"),
				TEXT("Text_LivesCaption"), TEXT("Text_Lives"),
				LOCTEXT("LivesCaption", "HULL INTEGRITY"), LOCTEXT("LivesPlaceholder", "3 / 3   ■■■"),
				FLinearColor(1.0f, 0.58f, 0.52f, 1.0f),
				FVector2D(0.025f, 0.315f), FVector2D(0.0f, 0.0f),
				FVector2D(236.0f, 82.0f), 25)
			|| !BuildMetricCard(
				TEXT("Border_BoostCombatCard"), TEXT("VerticalBox_BoostCombatCard"),
				TEXT("Text_BoostCaption"), TEXT("Text_Boost"),
				LOCTEXT("BoostCaption", "TURBO CHARGE"), LOCTEXT("BoostPlaceholder", "■■■   READY"),
				Mint, FVector2D(0.025f, 0.415f), FVector2D(0.0f, 0.0f),
				FVector2D(236.0f, 82.0f), 20))
		{
			return false;
		}

		UBorder* ControlsHint = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_ControlsHint"));
		UTextBlock* ControlsText = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_Controls"), true);
		if (!ControlsHint || !ControlsText)
		{
			return false;
		}
		FFMinigameUIStyle::ApplyCard(
			*ControlsHint,
			FFMinigameUIStyle::WithOpacity(Theme.SurfaceSunken, 0.96f),
			FMargin(22.0f, 10.0f),
			Theme.Line);
		ControlsHint->SetVisibility(ESlateVisibility::HitTestInvisible);
		ControlsText->SetText(LOCTEXT(
			"ControlsHint",
			"[ WASD / STICK ]  MOVE     [ MOUSE ]  TURN     [ SPACE / A ]  BOOST"));
		ControlsText->SetJustification(ETextJustify::Center);
		ControlsText->SetVisibility(ESlateVisibility::HitTestInvisible);
		StyleAaaText(*ControlsText, 13, Theme.Text, TEXT("SemiBold"));
		ControlsHint->SetContent(ControlsText);
		if (!AddCanvasChild(
			*SurfArea,
			*ControlsHint,
			FAnchors(0.28f, 0.925f, 0.72f, 0.975f),
			FVector2D::ZeroVector,
			FMargin(0.0f),
			200))
		{
			return false;
		}

		UButton* CloseButton = MakeWidget<UButton>(
			Blueprint, Tree, TEXT("Button_Close"), true);
		UTextBlock* CloseLabel = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_CloseLabel"));
		if (!CloseButton || !CloseLabel)
		{
			return false;
		}
		CloseLabel->SetText(LOCTEXT("Close", "나가기  [ESC]"));
		CloseLabel->SetJustification(ETextJustify::Center);
		StyleAaaText(*CloseLabel, 12, Theme.Text, TEXT("SemiBold"));
		FFMinigameUIStyle::ApplyButton(*CloseButton, Theme, false);
		CloseButton->SetContent(CloseLabel);
		if (!AddCanvasChild(
			*SurfArea,
			*CloseButton,
			FAnchors(0.982f, 0.028f),
			FVector2D(1.0f, 0.0f),
			FMargin(0.0f, 0.0f, 136.0f, 48.0f),
			400))
		{
			return false;
		}

		UBorder* ResultDimmer = MakeWidget<UBorder>(
			Blueprint, Tree, TEXT("Border_ResultDimmer"));
		if (!ResultDimmer)
		{
			return false;
		}
		ResultDimmer->SetBrushColor(FLinearColor::Transparent);
		ResultDimmer->SetVisibility(ESlateVisibility::Collapsed);
		if (!AddCanvasChild(
			*SurfArea, *ResultDimmer,
			FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector,
			FMargin(0.0f), 300))
		{
			return false;
		}

		UBorder* ResultPanel = MakeWidget<UBorder>(
			Blueprint, Tree, TEXT("Panel_Result"), true);
		UOverlay* ResultStage = MakeWidget<UOverlay>(
			Blueprint, Tree, TEXT("Overlay_ResultStage"));
		USizeBox* ResultCardSize = MakeWidget<USizeBox>(
			Blueprint, Tree, TEXT("SizeBox_ResultCard"));
		UBorder* ResultCard = MakeWidget<UBorder>(
			Blueprint, Tree, TEXT("Border_ResultCard"));
		UVerticalBox* ResultColumn = MakeWidget<UVerticalBox>(
			Blueprint, Tree, TEXT("VerticalBox_Result"));
		UTextBlock* ResultHeading = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_ResultHeading"));
		UTextBlock* ResultText = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_Result"), true);
		UTextBlock* ResultExitHint = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_ResultExitHint"));
		USizeBox* ResultCtaSize = MakeWidget<USizeBox>(
			Blueprint, Tree, TEXT("Size_ResultCta"));
		UButton* ResultCloseButton = MakeWidget<UButton>(
			Blueprint, Tree, TEXT("Button_ResultClose"), true);
		UTextBlock* ResultCloseLabel = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_ResultCloseLabel"));
		if (!ResultPanel || !ResultStage || !ResultCardSize || !ResultCard
			|| !ResultColumn || !ResultHeading || !ResultText || !ResultExitHint
			|| !ResultCtaSize || !ResultCloseButton || !ResultCloseLabel)
		{
			return false;
		}
		ResultPanel->SetBrushColor(Theme.Scrim);
		ResultPanel->SetPadding(FMargin(0.0f));
		ResultPanel->SetVisibility(ESlateVisibility::Collapsed);
		ResultPanel->SetContent(ResultStage);
		ResultStage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		ResultCardSize->SetWidthOverride(660.0f);
		ResultCardSize->SetHeightOverride(500.0f);
		ResultCardSize->SetContent(ResultCard);
		FFMinigameUIStyle::ApplyCard(
			*ResultCard, Theme.Surface, FMargin(56.0f, 40.0f), Theme.LineStrong, 10.0f, 2.0f);
		ResultCard->SetContent(ResultColumn);
		ResultCard->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (UOverlaySlot* CardSlot = ResultStage->AddChildToOverlay(ResultCardSize))
		{
			CardSlot->SetHorizontalAlignment(HAlign_Center);
			CardSlot->SetVerticalAlignment(VAlign_Center);
		}
		ResultHeading->SetText(LOCTEXT("ResultHeading", "TIDAL RUN  //  FINAL TELEMETRY"));
		ResultHeading->SetJustification(ETextJustify::Center);
		StyleAaaText(*ResultHeading, 12, Theme.HeaderStrong, TEXT("SemiBold"));
		if (UVerticalBoxSlot* HeadingSlot = ResultColumn->AddChildToVerticalBox(ResultHeading))
		{
			HeadingSlot->SetHorizontalAlignment(HAlign_Center);
			HeadingSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 22.0f));
		}
		ResultText->SetText(LOCTEXT("ResultPlaceholder", "라이드 기록을 계산 중입니다"));
		ResultText->SetJustification(ETextJustify::Center);
		ResultText->SetAutoWrapText(true);
		ResultText->SetMinDesiredWidth(540.0f);
		StyleAaaText(*ResultText, 30, Theme.Text, TEXT("Bold"));
		if (UVerticalBoxSlot* ResultTextSlot = ResultColumn->AddChildToVerticalBox(ResultText))
		{
			ResultTextSlot->SetHorizontalAlignment(HAlign_Center);
			ResultTextSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 28.0f));
		}
		ResultExitHint->SetText(LOCTEXT(
			"ResultExitHint", "아래 확인 버튼으로 라이드 종료"));
		ResultExitHint->SetJustification(ETextJustify::Center);
		StyleAaaText(*ResultExitHint, 12, Theme.TextMuted, TEXT("SemiBold"));
		if (UVerticalBoxSlot* ExitHintSlot = ResultColumn->AddChildToVerticalBox(ResultExitHint))
		{
			ExitHintSlot->SetHorizontalAlignment(HAlign_Center);
		}
		ResultCtaSize->SetWidthOverride(240.0f);
		ResultCtaSize->SetHeightOverride(54.0f);
		ResultCloseLabel->SetText(LOCTEXT("ResultClose", "확인  [ESC]"));
		ResultCloseLabel->SetJustification(ETextJustify::Center);
		StyleAaaText(*ResultCloseLabel, 15, Theme.White, TEXT("SemiBold"));
		FFMinigameUIStyle::ApplyButton(*ResultCloseButton, Theme, true);
		ResultCloseButton->SetContent(ResultCloseLabel);
		ResultCtaSize->SetContent(ResultCloseButton);
		if (UVerticalBoxSlot* ResultCtaSlot = ResultColumn->AddChildToVerticalBox(ResultCtaSize))
		{
			ResultCtaSlot->SetHorizontalAlignment(HAlign_Center);
			ResultCtaSlot->SetPadding(FMargin(0.0f, 22.0f, 0.0f, 0.0f));
		}
		else
		{
			return false;
		}
		if (UOverlaySlot* ResultSlot = Root->AddChildToOverlay(ResultPanel))
		{
			ResultSlot->SetHorizontalAlignment(HAlign_Fill);
			ResultSlot->SetVerticalAlignment(VAlign_Fill);
		}

		return HasRequiredWidgetTree(Blueprint);
	}

	int32 CourseTypeIndex(const EFF_SurfingObjectType Type)
	{
		switch (Type)
		{
		case EFF_SurfingObjectType::Rock: return 0;
		case EFF_SurfingObjectType::Log: return 1;
		case EFF_SurfingObjectType::Buoy: return 2;
		case EFF_SurfingObjectType::Coin: return 3;
		case EFF_SurfingObjectType::Heart: return 4;
		case EFF_SurfingObjectType::BoostPickup: return 5;
		default: return INDEX_NONE;
		}
	}

	bool ValidateCourseTable(const UDataTable& Table)
	{
		if (Table.GetRowStruct() != FFF_SurfingCourseRow::StaticStruct())
		{
			return false;
		}

		bool bHasEnabledObstacle = false;
		bool bHasEnabledPickup = false;
		for (const FName RowName : Table.GetRowNames())
		{
			const FFF_SurfingCourseRow* Row = Table.FindRow<FFF_SurfingCourseRow>(
				RowName, TEXT("Validate Surfing FreeRide course"), false);
			if (!Row)
			{
				return false;
			}
			if (!Row->bEnabled)
			{
				continue;
			}

			const int32 TypeIndex = CourseTypeIndex(Row->Type);
			if (TypeIndex == INDEX_NONE
				|| !FMath::IsFinite(Row->PositionNormalized.X)
				|| !FMath::IsFinite(Row->PositionNormalized.Y)
				|| Row->PositionNormalized.X < -1.0f
				|| Row->PositionNormalized.X > 1.0f
				|| Row->PositionNormalized.Y < -1.0f
				|| Row->PositionNormalized.Y > 1.0f
				|| !FMath::IsFinite(Row->CollisionRadiusNormalized)
				|| Row->CollisionRadiusNormalized <= 0.0f
				|| !FMath::IsFinite(Row->ScoreValueOverride)
				|| Row->ScoreValueOverride < 0.0f)
			{
				return false;
			}
			if (Row->Type == EFF_SurfingObjectType::Rock
				|| Row->Type == EFF_SurfingObjectType::Log
				|| Row->Type == EFF_SurfingObjectType::Buoy)
			{
				bHasEnabledObstacle = true;
			}
			else
			{
				bHasEnabledPickup = true;
			}
		}

		// Existing artist-authored tables may freely change counts and types.
		// Reuse only enforces the same minimum contract as the Definition.
		return bHasEnabledObstacle && bHasEnabledPickup;
	}

	void AddCourseRow(
		UDataTable& Table,
		const FName RowName,
		const EFF_SurfingObjectType Type,
		const FVector2D& Position,
		const float CollisionRadius)
	{
		FFF_SurfingCourseRow Row;
		Row.Type = Type;
		Row.PositionNormalized = Position;
		Row.CollisionRadiusNormalized = CollisionRadius;
		Row.ScoreValueOverride = 0.0f;
		Row.bEnabled = true;
		Table.AddRow(RowName, Row);
	}

	bool SaveNewCourseTable(UDataTable& Table)
	{
		UPackage* Package = Table.GetOutermost();
		if (!Package)
		{
			return false;
		}

		Table.MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);

		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, &Table, *Filename, Args);
	}

	UDataTable* EnsureCourseTable()
	{
		if (UObject* LoadedObject = FindLoadedAssetObject(CourseTablePackage))
		{
			UPackage* Package = LoadedObject->GetOutermost();
			if (Package && Package->IsDirty())
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_FAIL: loaded course table package is dirty; no asset was saved: %s"),
					LogPrefix, *CourseTablePackage);
				return nullptr;
			}

			UDataTable* Existing = Cast<UDataTable>(LoadedObject);
			if (!Existing || !ValidateCourseTable(*Existing))
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_FAIL: loaded course table violates the FreeRide row contract: %s"),
					LogPrefix, *CourseTablePackage);
				return nullptr;
			}

			UE_LOG(LogFFUIWbpBuilder, Display,
				TEXT("%s_REUSE: %s is already loaded and valid; no save was performed."),
				LogPrefix, *CourseTablePackage);
			return Existing;
		}

		if (FPackageName::DoesPackageExist(CourseTablePackage))
		{
			UDataTable* Existing = Cast<UDataTable>(LoadExistingAssetObject(CourseTablePackage));
			if (!Existing || Existing->GetOutermost()->IsDirty() || !ValidateCourseTable(*Existing))
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_FAIL: existing course table cannot be safely reused: %s"),
					LogPrefix, *CourseTablePackage);
				return nullptr;
			}

			UE_LOG(LogFFUIWbpBuilder, Display,
				TEXT("%s_REUSE: %s exists and is valid; designer rows were preserved."),
				LogPrefix, *CourseTablePackage);
			return Existing;
		}

		UDataTableFactory* Factory = NewObject<UDataTableFactory>();
		Factory->Struct = FFF_SurfingCourseRow::StaticStruct();
		FAssetToolsModule& AssetTools =
			FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		UDataTable* Table = Cast<UDataTable>(AssetTools.Get().CreateAsset(
			FPackageName::GetLongPackageAssetName(CourseTablePackage),
			CourseTableFolder,
			UDataTable::StaticClass(),
			Factory));
		if (!Table || Table->GetRowStruct() != FFF_SurfingCourseRow::StaticStruct())
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: could not create the FreeRide course table: %s"),
				LogPrefix, *CourseTablePackage);
			return nullptr;
		}

		AddCourseRow(*Table, TEXT("Rock_01"), EFF_SurfingObjectType::Rock,
			FVector2D(-0.72f, -0.54f), 0.090f);
		AddCourseRow(*Table, TEXT("Rock_02"), EFF_SurfingObjectType::Rock,
			FVector2D(0.62f, 0.32f), 0.085f);
		AddCourseRow(*Table, TEXT("Rock_03"), EFF_SurfingObjectType::Rock,
			FVector2D(-0.08f, 0.78f), 0.080f);
		AddCourseRow(*Table, TEXT("Log_01"), EFF_SurfingObjectType::Log,
			FVector2D(-0.18f, 0.05f), 0.105f);
		AddCourseRow(*Table, TEXT("Log_02"), EFF_SurfingObjectType::Log,
			FVector2D(0.72f, -0.48f), 0.100f);
		AddCourseRow(*Table, TEXT("Buoy_01"), EFF_SurfingObjectType::Buoy,
			FVector2D(0.30f, 0.65f), 0.075f);
		AddCourseRow(*Table, TEXT("Buoy_02"), EFF_SurfingObjectType::Buoy,
			FVector2D(-0.58f, -0.18f), 0.075f);
		AddCourseRow(*Table, TEXT("Coin_01"), EFF_SurfingObjectType::Coin,
			FVector2D(-0.46f, 0.43f), 0.045f);
		AddCourseRow(*Table, TEXT("Coin_02"), EFF_SurfingObjectType::Coin,
			FVector2D(0.05f, -0.31f), 0.045f);
		AddCourseRow(*Table, TEXT("Coin_03"), EFF_SurfingObjectType::Coin,
			FVector2D(0.58f, 0.05f), 0.045f);
		AddCourseRow(*Table, TEXT("Heart_01"), EFF_SurfingObjectType::Heart,
			FVector2D(-0.24f, 0.72f), 0.055f);
		AddCourseRow(*Table, TEXT("Heart_02"), EFF_SurfingObjectType::Heart,
			FVector2D(0.77f, 0.66f), 0.055f);
		AddCourseRow(*Table, TEXT("Boost_01"), EFF_SurfingObjectType::BoostPickup,
			FVector2D(-0.73f, -0.76f), 0.060f);
		AddCourseRow(*Table, TEXT("Boost_02"), EFF_SurfingObjectType::BoostPickup,
			FVector2D(0.38f, 0.80f), 0.060f);

		if (!ValidateCourseTable(*Table) || !SaveNewCourseTable(*Table))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: FreeRide course table validation/save failed: %s"),
				LogPrefix, *CourseTablePackage);
			return nullptr;
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_CREATE: editable FreeRide course table created: %s"),
			LogPrefix, *CourseTablePackage);
		return Table;
	}

	bool ValidateScreenBlueprint(const UWidgetBlueprint& Blueprint)
	{
		return Blueprint.Status != BS_Error
			&& Blueprint.GeneratedClass
			&& Blueprint.GeneratedClass->IsChildOf(UFF_SurfingScreen::StaticClass())
			&& Blueprint.GeneratedClass->GetPathName() == FreeRideScreenClassPath
			&& HasRequiredWidgetTree(Blueprint);
	}

	UWidgetBlueprint* EnsureFreeRideScreenBlueprint()
	{
		if (UObject* LoadedObject = FindLoadedAssetObject(FreeRideScreenPackage))
		{
			UPackage* Package = LoadedObject->GetOutermost();
			if (Package && Package->IsDirty())
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_FAIL: loaded FreeRide WBP package is dirty; no asset was saved: %s"),
					LogPrefix, *FreeRideScreenPackage);
				return nullptr;
			}

			UWidgetBlueprint* Existing = Cast<UWidgetBlueprint>(LoadedObject);
			if (!Existing || !ValidateScreenBlueprint(*Existing))
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_FAIL: loaded FreeRide WBP violates the simple arena contract: %s"),
					LogPrefix, *FreeRideScreenPackage);
				return nullptr;
			}

			UE_LOG(LogFFUIWbpBuilder, Display,
				TEXT("%s_REUSE: %s is already loaded and valid; no save was performed."),
				LogPrefix, *FreeRideScreenPackage);
			return Existing;
		}

		UWidgetBlueprint* Blueprint = EnsureWidgetBlueprint(
			LogPrefix,
			FreeRideScreenPackage,
			ScreenFolder,
			*UFF_SurfingScreen::StaticClass(),
			[](UWidgetBlueprint& NewBlueprint, UWidgetTree& Tree)
			{
				return BuildScreenLayout(NewBlueprint, Tree);
			});

		if (!Blueprint || !ValidateScreenBlueprint(*Blueprint))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: FreeRide WBP parent/widget contract validation failed: %s"),
				LogPrefix, *FreeRideScreenPackage);
			return nullptr;
		}
		return Blueprint;
	}

	void RebuildSurfingUI()
	{
		const TCHAR* const RebuildLogPrefix = TEXT("FF_SURFING_AAA_UI_REBUILD");
		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_BEGIN: rebuilding only the existing FreeRide WBP widget tree."),
			RebuildLogPrefix);

		if (!FPackageName::DoesPackageExist(FreeRideScreenPackage))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: the existing FreeRide WBP package is required: %s"),
				RebuildLogPrefix, *FreeRideScreenPackage);
			return;
		}

		UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(
			LoadExistingAssetObject(FreeRideScreenPackage));
		if (!Blueprint)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: existing FreeRide package is not a WidgetBlueprint: %s"),
				RebuildLogPrefix, *FreeRideScreenPackage);
			return;
		}

		UPackage* Package = Blueprint->GetOutermost();
		if (!Package || Package->IsDirty())
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REFUSE: FreeRide WBP package is dirty; save or revert it first: %s"),
				RebuildLogPrefix, *FreeRideScreenPackage);
			return;
		}
		if (GEditor && GEditor->PlayWorld)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REFUSE: stop PIE before rebuilding %s."),
				RebuildLogPrefix, *FreeRideScreenPackage);
			return;
		}
		if (!Blueprint->ParentClass
			|| !Blueprint->ParentClass->IsChildOf(UFF_SurfingScreen::StaticClass())
			|| Blueprint->Status == BS_Error
			|| !Blueprint->GeneratedClass
			|| !Blueprint->GeneratedClass->IsChildOf(UFF_SurfingScreen::StaticClass()))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: FreeRide WBP has an unexpected parent/generated class."),
				RebuildLogPrefix);
			return;
		}
		if (Blueprint->Animations.Num() > 0)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REFUSE: %s owns %d Widget Animation(s). The command backs up "
					 "only the tree, bindings and variable GUIDs, so the WBP was not modified."),
				RebuildLogPrefix, *FreeRideScreenPackage, Blueprint->Animations.Num());
			return;
		}
		const FString Filename = FPackageName::LongPackageNameToFilename(
			FreeRideScreenPackage, FPackageName::GetAssetPackageExtension());
		if (IFileManager::Get().IsReadOnly(*Filename))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REFUSE: WBP file is read-only; nothing was modified: %s"),
				RebuildLogPrefix, *Filename);
			return;
		}

		UWidgetTree* OriginalTree = Blueprint->WidgetTree;
		if (!OriginalTree || !OriginalTree->RootWidget)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: existing FreeRide WBP has no recoverable WidgetTree."),
				RebuildLogPrefix);
			return;
		}
		const FName OriginalTreeName = OriginalTree->GetFName();
		const bool bOriginalTreeWasTransient = OriginalTree->HasAnyFlags(RF_Transient);
		const auto OriginalBindings = Blueprint->Bindings;
		const auto OriginalVariableMap = Blueprint->WidgetVariableNameToGuidMap;
		const FName DiscardedName = MakeUniqueObjectName(
			Blueprint, UWidgetTree::StaticClass(), TEXT("DiscardedSurfingWidgetTree"));
		if (!OriginalTree->Rename(
			*DiscardedName.ToString(), Blueprint,
			REN_DontCreateRedirectors | REN_NonTransactional))
		{
			Package->SetDirtyFlag(false);
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: could not stage the existing WidgetTree; nothing saved."),
				RebuildLogPrefix);
			return;
		}
		OriginalTree->SetFlags(RF_Transient);

		Blueprint->Modify();
		Blueprint->Bindings.Reset();
		Blueprint->WidgetVariableNameToGuidMap.Reset();
		Blueprint->WidgetTree = NewObject<UWidgetTree>(
			Blueprint, UWidgetTree::StaticClass(), TEXT("WidgetTree"), RF_Transactional);

		auto Rollback = [&]() -> bool
		{
			UWidgetTree* FailedTree = Blueprint->WidgetTree;
			if (FailedTree && FailedTree != OriginalTree)
			{
				const FName FailedName = MakeUniqueObjectName(
					Blueprint, UWidgetTree::StaticClass(), TEXT("FailedSurfingWidgetTree"));
				if (!FailedTree->Rename(
					*FailedName.ToString(), Blueprint,
					REN_DontCreateRedirectors | REN_NonTransactional))
				{
					return false;
				}
				FailedTree->SetFlags(RF_Transient);
			}

			OriginalTree->ClearFlags(RF_Transient);
			if (!OriginalTree->Rename(
				*OriginalTreeName.ToString(), Blueprint,
				REN_DontCreateRedirectors | REN_NonTransactional))
			{
				return false;
			}
			if (bOriginalTreeWasTransient)
			{
				OriginalTree->SetFlags(RF_Transient);
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
				TEXT("%s_FAIL: layout construction failed; original tree %s, no save."),
				RebuildLogPrefix,
				bRolledBack ? TEXT("restored") : TEXT("could not be restored"));
			return;
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		if (Blueprint->Status == BS_Error || !ValidateScreenBlueprint(*Blueprint))
		{
			const bool bRolledBack = Rollback();
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: rebuilt WBP did not compile/validate; original tree %s, no save."),
				RebuildLogPrefix,
				bRolledBack ? TEXT("restored") : TEXT("could not be restored"));
			return;
		}

		const bool bSavedRebuild = SaveNewBlueprint(RebuildLogPrefix, *Blueprint);
		if (!bSavedRebuild || !ValidateScreenBlueprint(*Blueprint))
		{
			const bool bRolledBack = Rollback();
			const bool bRecoveredOriginal = bRolledBack
				&& SaveNewBlueprint(TEXT("FF_SURFING_AAA_UI_RECOVERY"), *Blueprint);
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: compile/save/validation failed; original tree rollback=%s, "
					 "recovery save=%s."),
				RebuildLogPrefix,
				bRolledBack ? TEXT("OK") : TEXT("FAILED"),
				bRecoveredOriginal ? TEXT("OK") : TEXT("FAILED"));
			return;
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_OK: rebuilt, compiled and saved only %s. Definition/course/rules untouched."),
			RebuildLogPrefix, *FreeRideScreenPackage);
	}

	EScreenClassDisposition GetScreenClassDisposition(
		const UFF_SurfingDefinition& Definition,
		UClass*& OutResolvedClass)
	{
		OutResolvedClass = nullptr;
		const FString SoftPath = Definition.ScreenClass.ToSoftObjectPath().ToString();
		if (SoftPath == LegacyScreenClassPath)
		{
			OutResolvedClass = Definition.ScreenClass.LoadSynchronous();
			return OutResolvedClass
				? EScreenClassDisposition::LegacyWbp
				: EScreenClassDisposition::Unexpected;
		}
		if (SoftPath == EdgeScreenClassPath)
		{
			OutResolvedClass = Definition.ScreenClass.LoadSynchronous();
			return OutResolvedClass
				? EScreenClassDisposition::EdgeWbp
				: EScreenClassDisposition::Unexpected;
		}
		if (SoftPath == FreeRideScreenClassPath)
		{
			OutResolvedClass = Definition.ScreenClass.LoadSynchronous();
			return EScreenClassDisposition::FreeRideWbp;
		}

		OutResolvedClass = Definition.ScreenClass.LoadSynchronous();
		return OutResolvedClass == UFF_SurfingScreen::StaticClass()
			? EScreenClassDisposition::NativeFallback
			: EScreenClassDisposition::Unexpected;
	}

	bool HasPreservedDefinitionIdentity(const UFF_SurfingDefinition& Definition)
	{
		FString RulesFailure;
		return Definition.MinigameId == SurfingId
			&& Definition.CompatibleFacilityDefinitionIds.Num() == 1
			&& Definition.CompatibleFacilityDefinitionIds.Contains(SurfingFacilityId)
			&& Definition.RuntimeClass.Get() == UFF_SurfingRuntime::StaticClass()
			&& Definition.Rules.IsValid(&RulesFailure)
			&& RulesFailure.IsEmpty();
	}

	UFF_SurfingDefinition* LoadDefinitionForMigration(
		EScreenClassDisposition& OutDisposition,
		UClass*& OutCurrentScreenClass)
	{
		UFF_SurfingDefinition* Definition = Cast<UFF_SurfingDefinition>(
			LoadExistingAssetObject(DefinitionPackage));
		if (!Definition)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: existing Surfing definition could not be loaded: %s"),
				LogPrefix, *DefinitionPackage);
			return nullptr;
		}

		UPackage* Package = Definition->GetOutermost();
		if (!Package || Package->IsDirty())
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: Surfing definition package is dirty; no asset was saved: %s"),
				LogPrefix, *DefinitionPackage);
			return nullptr;
		}

		if (!HasPreservedDefinitionIdentity(*Definition))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: Surfing definition identity/facility/runtime contract is invalid; "
					 "no asset was saved: %s"),
				LogPrefix, *DefinitionPackage);
			return nullptr;
		}

		const FSoftObjectPath CoursePath =
			Definition->CourseLayoutTable.ToSoftObjectPath();
		if (!CoursePath.IsNull() && CoursePath.ToString() != CourseTableObjectPath)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: CourseLayoutTable is not empty or the expected Surfing course: %s"),
				LogPrefix, *CoursePath.ToString());
			return nullptr;
		}

		OutDisposition = GetScreenClassDisposition(*Definition, OutCurrentScreenClass);
		if (OutDisposition == EScreenClassDisposition::Unexpected)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: ScreenClass is not legacy, Edge, FreeRide or native fallback: %s"),
				LogPrefix, *Definition->ScreenClass.ToSoftObjectPath().ToString());
			return nullptr;
		}

		if (OutCurrentScreenClass
			&& OutCurrentScreenClass != UFF_SurfingScreen::StaticClass()
			&& OutCurrentScreenClass->GetOutermost()->IsDirty())
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: current ScreenClass package is dirty; no asset was saved: %s"),
				LogPrefix, *OutCurrentScreenClass->GetOutermost()->GetName());
			return nullptr;
		}

		return Definition;
	}

	bool SaveMigratedDefinition(UFF_SurfingDefinition& Definition)
	{
		UPackage* Package = Definition.GetOutermost();
		if (!Package)
		{
			return false;
		}

		Definition.MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);

		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, &Definition, *Filename, Args);
	}

	bool MigrateDefinitionToFreeRide(
		UFF_SurfingDefinition& Definition,
		const EScreenClassDisposition Disposition,
		UClass& FreeRideScreenClass,
		UDataTable& CourseTable)
	{
		if (FreeRideScreenClass.GetPathName() != FreeRideScreenClassPath
			|| !FreeRideScreenClass.IsChildOf(UFF_SurfingScreen::StaticClass())
			|| CourseTable.GetPathName() != CourseTableObjectPath
			|| !ValidateCourseTable(CourseTable))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: generated FreeRide WBP/course table contract is invalid."),
				LogPrefix);
			return false;
		}

		UPackage* Package = Definition.GetOutermost();
		if (!Package || Package->IsDirty())
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: definition became dirty before migration; no save was performed."),
				LogPrefix);
			return false;
		}

		if (Disposition == EScreenClassDisposition::FreeRideWbp
			&& Definition.CourseLayoutTable.LoadSynchronous() == &CourseTable)
		{
			if (Definition.ScreenClass.LoadSynchronous() != &FreeRideScreenClass)
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_FAIL: FreeRide ScreenClass path did not resolve to the generated class."),
					LogPrefix);
				return false;
			}

			UE_LOG(LogFFUIWbpBuilder, Display,
				TEXT("%s_REUSE: definition already points to FreeRide WBP/course; no save was performed."),
				LogPrefix);
			return true;
		}

		const TSoftClassPtr<UFF_MinigameScreenBase> PreviousScreenClass = Definition.ScreenClass;
		const TSoftObjectPtr<UDataTable> PreviousCourseTable = Definition.CourseLayoutTable;
		Definition.ScreenClass = &FreeRideScreenClass;
		Definition.CourseLayoutTable = &CourseTable;
		TArray<FText> ValidationErrors;
		if (!HasPreservedDefinitionIdentity(Definition)
			|| !Definition.IsDefinitionValid(ValidationErrors)
			|| !ValidationErrors.IsEmpty())
		{
			Definition.ScreenClass = PreviousScreenClass;
			Definition.CourseLayoutTable = PreviousCourseTable;
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: definition validation failed after FreeRide migration; no save."),
				LogPrefix);
			return false;
		}

		if (!SaveMigratedDefinition(Definition))
		{
			Definition.ScreenClass = PreviousScreenClass;
			Definition.CourseLayoutTable = PreviousCourseTable;
			Package->SetDirtyFlag(false);
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: definition save failed; both in-memory references were rolled back."),
				LogPrefix);
			return false;
		}

		if (Definition.ScreenClass.LoadSynchronous() != &FreeRideScreenClass
			|| Definition.CourseLayoutTable.LoadSynchronous() != &CourseTable)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: saved definition does not resolve FreeRide WBP/course references."),
				LogPrefix);
			return false;
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_MIGRATE: %s now points to %s and %s. Previous WBPs preserved at %s and %s."),
			LogPrefix, *DefinitionPackage, *FreeRideScreenClassPath, *CourseTableObjectPath,
			*LegacyScreenPackage, *EdgeScreenPackage);
		return true;
	}

	void BuildSurfingAssets()
	{
		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_BEGIN: creates/reuses FreeRide WBP/course and safely migrates two DA references."),
			LogPrefix);

		EScreenClassDisposition Disposition = EScreenClassDisposition::Unexpected;
		UClass* CurrentScreenClass = nullptr;
		UFF_SurfingDefinition* Definition = LoadDefinitionForMigration(
			Disposition, CurrentScreenClass);
		if (!Definition)
		{
			return;
		}

		UDataTable* CourseTable = EnsureCourseTable();
		if (!CourseTable)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: course table is unavailable; definition was not modified."),
				LogPrefix);
			return;
		}

		UWidgetBlueprint* ScreenBlueprint = EnsureFreeRideScreenBlueprint();
		if (!ScreenBlueprint || !ScreenBlueprint->GeneratedClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: FreeRide WBP is unavailable; definition was not modified."),
				LogPrefix);
			return;
		}

		UClass* GeneratedScreenClass = ScreenBlueprint->GeneratedClass.Get();
		if (!GeneratedScreenClass
			|| !MigrateDefinitionToFreeRide(
				*Definition, Disposition, *GeneratedScreenClass, *CourseTable))
		{
			return;
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_OK: FreeRide WBP/course/definition ready; legacy and Edge WBPs untouched."),
			LogPrefix);
	}

	FAutoConsoleCommand BuildSurfingAssetsCommand(
		TEXT("FF.BuildSurfingAssets"),
		TEXT("Creates/reuses FreeRide WBP/course and safely migrates DA_Minigame_Surfing references."),
		FConsoleCommandDelegate::CreateStatic(&BuildSurfingAssets));

	FAutoConsoleCommand RebuildSurfingUICommand(
		TEXT("FF.RebuildSurfingUI"),
		TEXT("Rebuilds and saves only the existing clean, animation-free FreeRide Surfing WBP tree; refuses PIE."),
		FConsoleCommandDelegate::CreateStatic(&RebuildSurfingUI));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
