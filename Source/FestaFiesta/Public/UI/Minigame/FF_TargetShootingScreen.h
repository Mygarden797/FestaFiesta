// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Minigame/FF_MinigameTypes.h"
#include "UI/Minigame/FF_MinigameScreenBase.h"
#include "FF_TargetShootingScreen.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UImage;
class UProgressBar;
class UTextBlock;
class UTexture2D;
class UViewport;
class UWidget;
class AFF_TargetShootingArena3D;
class UFF_TargetShootingRuntime;

/** Paint-only 2D target whose visible boundary is the authoritative hit ellipse. */
UCLASS(NotBlueprintable)
class FESTAFIESTA_API UFF_TargetShootingTargetVisual final : public UUserWidget
{
	GENERATED_BODY()

public:
	void Configure(
		const FLinearColor& InOuterColor,
		const FLinearColor& InPrecisionColor,
		const FLinearColor& InBullseyeColor,
		float InPrecisionRatio,
		float InBullseyeRatio,
		float InRemainingLifeRatio,
		const FLinearColor& InCountdownColor,
		float InLineThickness);

	const FLinearColor& GetOuterColor() const { return OuterColor; }
	float GetRemainingLifeRatio() const { return RemainingLifeRatio; }

protected:
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	FLinearColor OuterColor = FLinearColor::White;
	FLinearColor PrecisionColor = FLinearColor::White;
	FLinearColor BullseyeColor = FLinearColor::White;
	FLinearColor CountdownColor = FLinearColor::White;
	float PrecisionRatio = 0.58f;
	float BullseyeRatio = 0.26f;
	float RemainingLifeRatio = 1.0f;
	float LineThickness = 4.0f;
};

/** Paint-only tracer and glowing projectile core used by both WBP and native fallback. */
UCLASS(NotBlueprintable)
class FESTAFIESTA_API UFF_TargetShootingProjectileVisual final : public UUserWidget
{
	GENERATED_BODY()

public:
	void Configure(
		const FVector2D& InDirection,
		const FLinearColor& InCoreColor,
		const FLinearColor& InGlowColor,
		float InCoreThickness,
		float InTrailLength);

	const FLinearColor& GetCoreColor() const { return CoreColor; }

protected:
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	FVector2D Direction = FVector2D(0.0f, -1.0f);
	FLinearColor CoreColor = FLinearColor::White;
	FLinearColor GlowColor = FLinearColor(0.08f, 0.86f, 1.0f, 1.0f);
	float CoreThickness = 4.0f;
	float TrailLength = 34.0f;
};

/** Designer-editable colour and motion tokens; gameplay balance remains in the Definition Rules. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_TargetShootingVisualStyle
{
	GENERATED_BODY()

	/** Full-screen authored range plate. A missing asset cleanly falls back to the colour wash. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backdrop")
	TSoftObjectPtr<UTexture2D> BackdropTexture;

	/** First-person water blaster rendered at the bottom of the shooting canvas. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSoftObjectPtr<UTexture2D> WaterBlasterTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backdrop")
	FLinearColor BackdropTint = FLinearColor(0.92f, 0.98f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backdrop")
	FLinearColor ScreenWashColor = FLinearColor(0.004f, 0.014f, 0.030f, 0.18f);

	/** Clear colour behind the real-time 3D gallery viewport. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena")
	FLinearColor Arena3DBackgroundColor = FLinearColor(0.001f, 0.004f, 0.010f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float Arena3DKeyLightIntensity = 0.78f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float Arena3DSkyIntensity = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena")
	FVector Arena3DCameraLocation = FVector(-1100.0f, 0.0f, 190.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena")
	FRotator Arena3DCameraRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	FLinearColor ArenaSurfaceColor = FLinearColor(0.002f, 0.012f, 0.030f, 0.50f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	FLinearColor ArenaFrameColor = FLinearColor(0.08f, 0.82f, 0.88f, 0.90f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	FLinearColor HudPanelColor = FLinearColor(0.006f, 0.024f, 0.048f, 0.94f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	FLinearColor HudPanelMutedColor = FLinearColor(0.010f, 0.050f, 0.078f, 0.88f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	FLinearColor PrimaryTextColor = FLinearColor(0.88f, 0.98f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	FLinearColor SecondaryTextColor = FLinearColor(0.34f, 0.72f, 0.80f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	FLinearColor HudAccentColor = FLinearColor(0.10f, 0.92f, 0.90f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	FLinearColor HudAmberColor = FLinearColor(1.0f, 0.63f, 0.12f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target")
	FLinearColor NormalTargetColor = FLinearColor(1.0f, 0.20f, 0.08f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target")
	FLinearColor BonusTargetColor = FLinearColor(1.0f, 0.72f, 0.04f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target", meta = (ClampMin = "1.0", ClampMax = "12.0"))
	float TargetRingThickness = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target")
	FLinearColor TargetCueColor = FLinearColor(1.0f, 0.96f, 0.82f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target", meta = (ClampMin = "10.0", ClampMax = "32.0"))
	float TargetCueFontSize = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target", meta = (ClampMin = "0.0", Units = "s"))
	float TargetSpawnPulseSeconds = 0.28f;

	/** Keeps the visual edge identical to the authoritative hit ellipse. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TargetSpawnMinOpacity = 0.25f;

	/** Late-life pulse uses opacity, never render scale, so visible and clickable bounds stay equal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TargetUrgencyPulseMinOpacity = 0.60f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile")
	FLinearColor ProjectileCoreColor = FLinearColor(0.88f, 1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile")
	FLinearColor ProjectileGlowColor = FLinearColor(0.08f, 0.86f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "64.0", ClampMax = "220.0"))
	float ProjectileVisualSize = 132.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "1.0", ClampMax = "12.0"))
	float ProjectileCoreThickness = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "8.0", ClampMax = "96.0"))
	float ProjectileTrailLength = 62.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FLinearColor WaterBlasterTint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "180.0", ClampMax = "520.0"))
	float WaterBlasterVisualSize = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.0", ClampMax = "80.0"))
	float WaterBlasterBottomOffsetPixels = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float WeaponRecoilPixels = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.05", ClampMax = "0.5", Units = "s"))
	float MuzzleFlashSeconds = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FLinearColor MuzzleFlashColor = FLinearColor(0.30f, 1.0f, 0.96f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback")
	FLinearColor HitColor = FLinearColor(0.18f, 0.90f, 0.94f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback")
	FLinearColor PrecisionColor = FLinearColor(0.96f, 1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback")
	FLinearColor BullseyeColor = FLinearColor(1.0f, 0.94f, 0.12f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback")
	FLinearColor MissColor = FLinearColor(1.0f, 0.24f, 0.22f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback")
	FLinearColor EscapedColor = FLinearColor(1.0f, 0.50f, 0.18f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback", meta = (ClampMin = "0.05", Units = "s"))
	float FeedbackDurationSeconds = 0.56f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback", meta = (ClampMin = "0.0"))
	float FeedbackRisePixels = 54.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback", meta = (ClampMin = "8.0"))
	float FeedbackFontSize = 26.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float PlayAreaFlashStrength = 0.035f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reticle")
	FString ReticleGlyph = TEXT("+");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reticle")
	FLinearColor ReticleColor = FLinearColor(0.82f, 1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reticle", meta = (ClampMin = "8.0"))
	float ReticleFontSize = 38.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reticle", meta = (ClampMin = "0.0", Units = "s"))
	float ReticleShotPulseSeconds = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reticle", meta = (ClampMin = "1.0"))
	float ReticleShotPulseScale = 1.45f;
};

/**
 * DPI-independent target-shooting presentation.
 *
 * The screen converts a pointer position to play-area normalized coordinates and
 * fires a projectile toward that point together with the displayed TargetSerial.
 * Projectile travel, impact-time hit testing, scoring, target movement and time
 * expiration all remain authoritative in the runtime strategy.
 *
 * A designer WBP may provide the optional named widgets below. If no WBP is
 * available, this concrete class builds a complete native UMG tree so packaged
 * gameplay still works.
 */
UCLASS(BlueprintType, meta = (DisplayName = "FF Target Shooting Screen"))
class FESTAFIESTA_API UFF_TargetShootingScreen : public UFF_MinigameScreenBase
{
	GENERATED_BODY()

public:
	UFF_TargetShootingScreen(const FObjectInitializer& ObjectInitializer);

	/**
	 * Attaches the screen to one live runtime and immediately projects its snapshot.
	 * Returns false for null, invalid-session, or non-target-shooting runtimes.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Target Shooting")
	bool SetRuntime(UFF_TargetShootingRuntime* InRuntime);

	virtual bool BindRuntime(UFF_MinigameRuntime* InRuntime) override;
	virtual void ShowResult(const FFF_MinigameResult& Result) override;

	/** Presentation-only entry point used by runtime polling and automation. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Target Shooting")
	void ApplySnapshot(const FFF_TargetShootingSnapshot& Snapshot);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Target Shooting")
	const FFF_TargetShootingSnapshot& GetDisplayedSnapshot() const { return DisplayedSnapshot; }

	/** True when a complete designer WidgetTree is active instead of the native fallback. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Target Shooting")
	bool IsUsingDesignerWidgetTree() const { return bUsingDesignerWidgetTree; }

	/** True once the real-time perspective gallery has spawned in the UMG viewport. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Target Shooting")
	bool IsThreeDimensionalArenaReady() const { return bThreeDimensionalArenaReady; }

	/** WBP Class Defaults expose these tokens without changing runtime/scoring code. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting|Visual Style")
	FFF_TargetShootingVisualStyle VisualStyle;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

private:
	bool BuildNativeWidgetTree();
	void ResolveBindingsFromWidgetTree();
	bool HasUsableDesignerBindings() const;
	void InitializeBindings();
	void ResetTransientPresentationState();
	void EnsureTransientPresentationWidgets();
	bool TryInitializeThreeDimensionalArena();
	void RefreshPresentation();
	void RefreshTargetLayout();
	void RefreshProjectilePresentation();
	void RefreshWeaponPresentation();
	void RefreshFeedbackPresentation();
	void RefreshReticlePresentation();
	FLinearColor GetFeedbackColor(EFF_TargetShootingFeedbackType FeedbackType) const;
	FText MakeFeedbackText() const;
	bool TryMakeNormalizedPoint(
		const FVector2D& ScreenSpacePoint,
		FVector2D& OutNormalizedPoint) const;

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(Transient)
	TObjectPtr<UFF_TargetShootingRuntime> Runtime;

	UPROPERTY(Transient)
	FFF_TargetShootingSnapshot DisplayedSnapshot;

	UPROPERTY(Transient)
	FFF_MinigameResult DisplayedResult;

	UPROPERTY(Transient)
	TObjectPtr<UFF_TargetShootingTargetVisual> TargetVisual;

	UPROPERTY(Transient)
	TObjectPtr<UFF_TargetShootingProjectileVisual> ProjectileVisual;

	/** Real-time perspective shooting gallery; injected when an older WBP omits it. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UViewport> Viewport_Arena3D;

	UPROPERTY(Transient)
	TObjectPtr<AFF_TargetShootingArena3D> Arena3DActor;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UImage> Image_WaterBlaster;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UTextBlock> Text_MuzzleFlash;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UTextBlock> Text_WeaponStatus;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UTextBlock> Text_TargetCue;

	bool bUsingDesignerWidgetTree = false;
	bool bBuiltNativeWidgetTree = false;
	bool bBindingsInitialized = false;
	bool bResultFocusApplied = false;
	bool bHasSnapshot = false;
	bool bHasResult = false;
	bool bThreeDimensionalArenaReady = false;
	FVector2D LastPlayAreaSize = FVector2D::ZeroVector;
	int32 PresentedTargetSerial = 0;
	int32 PresentedProjectileSerial = 0;
	int32 PresentedFeedbackSerial = 0;
	float TargetSpawnPulseRemainingSeconds = 0.0f;
	float FeedbackRemainingSeconds = 0.0f;
	float ReticleShotPulseRemainingSeconds = 0.0f;
	float MuzzleFlashRemainingSeconds = 0.0f;
	FVector2D ReticlePointNormalized = FVector2D(0.5f, 0.5f);
	bool bReticleInsidePlayArea = false;
	FLinearColor BasePlayAreaBrushColor = FLinearColor::White;
	bool bHasBasePlayAreaBrushColor = false;

	/** Full interaction surface. It must cover only the intended shooting area. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UBorder> Border_PlayArea;

	/** Optional full-screen soft backdrop; runtime applies VisualStyle.BackdropTexture when present. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UImage> Image_Backdrop;

	/** Target positioning canvas inside Border_PlayArea. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UCanvasPanel> Canvas_PlayArea;

	/** Authoritative 2D target visual; must remain HitTestInvisible so the play area receives clicks. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UBorder> Border_Target;

	/** Optional in designer WBP; created on the play canvas when absent. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UTextBlock> Text_ShotFeedback;

	/** Optional custom reticle; runtime creates it when the WBP omits it. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UTextBlock> Text_Reticle;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UTextBlock> Text_RemainingTime;

	/** Optional match-clock gauge; drains without affecting the authoritative session clock. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UProgressBar> Progress_Time;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UTextBlock> Text_Score;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UTextBlock> Text_HitCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UTextBlock> Text_MissCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UTextBlock> Text_Streak;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UBorder> Panel_Result;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UTextBlock> Text_Result;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UButton> Button_Close;

	/** Optional close-button label shared by the generated WBP and native fallback. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NativeCloseLabel;
};
