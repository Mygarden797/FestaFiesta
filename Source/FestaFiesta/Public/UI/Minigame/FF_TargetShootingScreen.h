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

	/** Prefer one unobstructed image instead of the native 3D environment box. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backdrop",
		meta = (DisplayName = "단일 배경 모드", ToolTip = "켜면 배경 이미지만 사용하고 검은 뷰포트와 전경 기둥·벽·바닥이 있는 3D 환경 박스를 생성하지 않습니다. 과녁, 탄환과 물총 UMG 연출은 그대로 유지됩니다."))
	bool bUseSingleBackdrop = true;

	/** First-person water blaster rendered at the bottom of the shooting canvas. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSoftObjectPtr<UTexture2D> WaterBlasterTexture;

	/** Minimal circular target plate shown by the WBP Image_Target layer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target",
		meta = (DisplayName = "과녁 이미지", ToolTip = "WBP의 Image_Target에 표시할 원형 과녁 텍스처입니다. 이 값만 교체하면 판정과 게임 로직을 건드리지 않고 과녁 디자인을 바꿀 수 있습니다."))
	TSoftObjectPtr<UTexture2D> TargetTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backdrop")
	FLinearColor BackdropTint = FLinearColor(0.92f, 0.98f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backdrop")
	FLinearColor ScreenWashColor = FLinearColor(0.004f, 0.014f, 0.030f, 0.18f);

	/** Clear colour behind the real-time 3D gallery viewport. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena")
	FLinearColor Arena3DBackgroundColor = FLinearColor(0.001f, 0.004f, 0.010f, 1.0f);

	/** Replace this class with a Blueprint child to swap the entire 3D art kit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena",
		meta = (DisplayName = "3D 사격장 클래스", ToolTip = "전체 3D 배경·과녁·탄환·물총 에셋을 교체하려면 FF Target Shooting 3D Arena의 Blueprint 자식 클래스를 지정합니다."))
	TSubclassOf<AFF_TargetShootingArena3D> Arena3DActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena",
		meta = (DisplayName = "3D 과녁 사용", ToolTip = "켜면 권위 판정 좌표와 같은 정규화 좌표에 실제 3D 과녁을 표시합니다."))
	bool bRenderTargetIn3D = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena",
		meta = (DisplayName = "3D 물 탄환 사용", ToolTip = "켜면 발사 궤적과 함께 원근감이 있는 3D 물 탄환을 표시합니다."))
	bool bRenderProjectileIn3D = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena",
		meta = (DisplayName = "3D 물총 사용", ToolTip = "켜면 2D 전경 물총과 별도로 3D 사격장 물총 모델을 표시합니다. 완전 3D 아트 구성이 준비된 경우 사용합니다."))
	bool bRenderWeaponIn3D = false;

	/** A faint exact 2D ring preserves pixel-perfect hit readability over the 3D plate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena",
		meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "3D 과녁 판정 링 투명도", ToolTip = "3D 과녁 위에 겹치는 정확한 판정 타원 링의 투명도입니다. 0이면 3D 과녁만 보이고 1이면 판정 링이 완전히 보입니다."))
	float AuthoritativeRingOpacityIn3D = 0.34f;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target",
		meta = (DisplayName = "과녁 이미지 색상", ToolTip = "Image_Target 텍스처에 곱하는 색입니다. 원본 색을 그대로 사용하려면 흰색을 유지합니다."))
	FLinearColor TargetImageTint = FLinearColor::White;

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

	/** Rotation origin inside Image_WaterBlaster, normalized from its top-left corner. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon",
		meta = (DisplayName = "총기 회전축", ToolTip = "교체한 Image_WaterBlaster 에셋 내부에서 총기가 회전할 기준점입니다. (0,0)은 좌상단, (1,1)은 우하단입니다."))
	FVector2D WaterBlasterAimPivotNormalized = FVector2D(0.5f, 0.85f);

	/** Projectile origin inside Image_WaterBlaster, normalized from its top-left corner. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon",
		meta = (DisplayName = "총구 위치", ToolTip = "교체한 Image_WaterBlaster 에셋 내부의 총구 위치입니다. 발사 섬광과 물 탄환이 이 지점에서 시작합니다."))
	FVector2D WaterBlasterMuzzleNormalized = FVector2D(0.5f, 0.14f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float WeaponRecoilPixels = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon",
		meta = (ClampMin = "0.0", ClampMax = "0.15", DisplayName = "총기 조준 이동 보정", ToolTip = "총기 회전과 함께 적용할 1인칭 조준 이동 보정량입니다."))
	float WeaponViewFollowStrength = 0.07f;

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
 * The custom reticle is an exact visual replacement for the pointer: absolute
 * screen-space mouse coordinates are transformed into the play canvas on move and
 * resampled on trigger. The projectile then starts at the authored weapon muzzle.
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

	/** Normalized screen coordinate currently shared by the custom reticle and shot destination. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Target Shooting")
	FVector2D GetCameraAimPointNormalized() const { return CameraAimPointNormalized; }

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
	void RefreshCameraAimPresentation();
	bool UpdateAimFromScreenPosition(const FVector2D& ScreenSpacePosition);
	FVector2D ResolveWeaponPlayAreaSize() const;
	FVector2D CalculateWeaponPivotPointPixels() const;
	FVector2D CalculateWeaponBaseForwardDirection() const;
	float CalculateWeaponAimAngleDegrees() const;
	FVector2D CalculateWeaponViewOffsetPixels() const;
	FVector2D CalculateWeaponMuzzlePointNormalized() const;
	bool ProjectArenaPointToCanvas(
		const FVector2D& ArenaPointNormalized,
		FVector2D& OutCanvasPointNormalized,
		float DepthX = 440.0f) const;
	FLinearColor GetFeedbackColor(EFF_TargetShootingFeedbackType FeedbackType) const;
	FText MakeFeedbackText() const;

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
	/** Hidden-mouse aim coordinate shared by the custom reticle, weapon and shot destination. */
	FVector2D CameraAimPointNormalized = FVector2D(0.5f, 0.5f);
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

	/** Designer-swappable target art. Runtime keeps the exact hit/countdown rings above it. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UImage> Image_Target;

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

	/** Dedicated result CTA. Keeping it inside the modal prevents z-order and aspect-ratio overlap. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UButton> Button_ResultClose;

	/** Optional close-button label shared by the generated WBP and native fallback. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NativeCloseLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NativeResultCloseLabel;
};
