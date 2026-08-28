// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Minigame/Surfing/FF_SurfingTypes.h"
#include "UI/Minigame/FF_MinigameScreenBase.h"
#include "FF_SurfingScreen.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UTextBlock;
class UTexture2D;
class UTextureRenderTarget2D;
class UWidget;
class UFF_SurfingRuntime;
class AFF_SurfingSceneActor;
class AFF_SurfingCharacterActor;

/** Designer-editable projection, colours and optional arena sprites. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_SurfingVisualStyle
{
	GENERATED_BODY()

	/** Forward half-length of the isolated capture arena in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Scene", meta = (ClampMin = "500.0"))
	float ArenaWorldHalfForward = 1450.0f;

	/** Third-person camera offset in the camera-relative endless course window. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Scene")
	FVector Camera3DLocation = FVector(-1350.0, 0.0, 720.0);

	/** Third-person look-at offset relative to the moving surfer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Scene")
	FVector Camera3DLookAt = FVector(950.0, 0.0, 80.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Scene", meta = (ClampMin = "30.0", ClampMax = "120.0"))
	float Camera3DFov = 68.0f;

	/** 0 keeps the camera centred; 1 follows lateral movement completely. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Scene", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float CameraLateralFollowRatio = 0.62f;

	/** Fixed capture exposure for a stable daylight presentation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Scene", meta = (ClampMin = "-2.0", ClampMax = "4.0"))
	float DaylightExposureBias = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Scene", meta = (ClampMin = "320", ClampMax = "3840"))
	int32 RenderTargetWidth = 1280;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Scene", meta = (ClampMin = "180", ClampMax = "2160"))
	int32 RenderTargetHeight = 720;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Scene", meta = (ClampMin = "0.0"))
	float SceneKeyLightIntensity = 100000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Scene", meta = (ClampMin = "0.0"))
	float SceneFillLightIntensity = 12000.0f;

	/** Blueprint/native visual actor used for the surfer; null uses the native fallback. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Character", meta = (AllowAbstract = "false"))
	TSubclassOf<AFF_SurfingCharacterActor> SurfingCharacterClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	FVector2D ArenaCenter = FVector2D(0.5f, 0.51f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena", meta = (ClampMin = "0.1", ClampMax = "0.5"))
	float ArenaHalfWidth = 0.43f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena", meta = (ClampMin = "0.1", ClampMax = "0.5"))
	float ArenaHalfHeight = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena", meta = (ClampMin = "0.1"))
	float EntityScale = 0.92f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena", meta = (ClampMin = "8.0"))
	float EntityLabelFontSize = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena", meta = (ClampMin = "1.0"))
	float FocusedControlScale = 1.06f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena", meta = (ClampMin = "0.0"))
	float EventPulseSeconds = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena", meta = (ClampMin = "1.0"))
	float EventPulseScale = 1.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artwork", meta = (ClampMin = "8.0"))
	FVector2D EntityTextureSize = FVector2D(52.0f, 52.0f);

	/** Bright cinematic arena plate. The HUD and entity halos are authored for this image. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artwork")
	TSoftObjectPtr<UTexture2D> BackdropTexture;

	/** Empty texture slots use the readable colour-card fallback. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artwork")
	TSoftObjectPtr<UTexture2D> RockTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artwork")
	TSoftObjectPtr<UTexture2D> LogTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artwork")
	TSoftObjectPtr<UTexture2D> BuoyTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artwork")
	TSoftObjectPtr<UTexture2D> CoinTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artwork")
	TSoftObjectPtr<UTexture2D> HeartTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artwork")
	TSoftObjectPtr<UTexture2D> BoostPickupTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour")
	FLinearColor OceanColor = FLinearColor(0.03f, 0.62f, 0.92f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour")
	FLinearColor OceanDeepColor = FLinearColor(0.02f, 0.35f, 0.65f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour")
	FLinearColor DaylightSkyColor = FLinearColor(0.28f, 0.78f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour")
	FLinearColor BackdropTint = FLinearColor(0.84f, 0.98f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour")
	FLinearColor ArenaScrimColor = FLinearColor(0.006f, 0.055f, 0.105f, 0.14f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour")
	FLinearColor HudPanelColor = FLinearColor(0.008f, 0.045f, 0.090f, 0.93f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour")
	FLinearColor MintAccentColor = FLinearColor(0.38f, 0.98f, 0.86f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour")
	FLinearColor AmberAccentColor = FLinearColor(1.0f, 0.67f, 0.16f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour")
	FLinearColor EntityHaloColor = FLinearColor(0.002f, 0.018f, 0.036f, 0.96f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour")
	FLinearColor FoamColor = FLinearColor(0.86f, 1.0f, 0.98f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour")
	FLinearColor SurferColor = FLinearColor(1.0f, 0.52f, 0.16f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour")
	FLinearColor BoostingSurferColor = FLinearColor(1.0f, 0.92f, 0.24f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour")
	FLinearColor RiderSuitColor = FLinearColor(0.95f, 0.12f, 0.035f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour")
	FLinearColor RiderSkinColor = FLinearColor(0.95f, 0.55f, 0.28f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour")
	FLinearColor RockColor = FLinearColor(0.38f, 0.48f, 0.52f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour")
	FLinearColor LogColor = FLinearColor(0.46f, 0.23f, 0.08f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour")
	FLinearColor BuoyColor = FLinearColor(0.96f, 0.20f, 0.15f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour")
	FLinearColor CoinColor = FLinearColor(1.0f, 0.78f, 0.08f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour")
	FLinearColor HeartColor = FLinearColor(0.98f, 0.18f, 0.34f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colour")
	FLinearColor BoostPickupColor = FLinearColor(0.42f, 0.96f, 0.90f, 1.0f);
};

/**
 * Cinematic fixed-arena presentation for the Surfing challenge.
 *
 * The runtime owns continuous movement, DataTable object placement, collision
 * and score. This screen only forwards direction/boost intent and projects the
 * immutable surface snapshot into an isolated SceneCapture-driven 3D arena.
 * The AAA HUD hierarchy stays editable in WBP_FF_Surfing_FreeRide; the legacy
 * Canvas projection remains available only as a fail-safe when 3D cannot start.
 */
UCLASS(BlueprintType, meta = (DisplayName = "FF Surfing Screen"))
class FESTAFIESTA_API UFF_SurfingScreen : public UFF_MinigameScreenBase
{
	GENERATED_BODY()

public:
	UFF_SurfingScreen(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Surfing")
	bool SetRuntime(UFF_SurfingRuntime* InRuntime);

	virtual bool BindRuntime(UFF_MinigameRuntime* InRuntime) override;
	virtual void ShowResult(const FFF_MinigameResult& Result) override;

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Surfing")
	void ApplySnapshot(const FFF_SurfingSnapshot& Snapshot);

	/** Changes the dedicated surfing character without rebuilding gameplay state. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Surfing")
	bool SetSurfingCharacterClass(TSubclassOf<AFF_SurfingCharacterActor> InCharacterClass);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Surfing")
	const FFF_SurfingSnapshot& GetDisplayedSnapshot() const { return DisplayedSnapshot; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Surfing")
	bool IsUsingDesignerWidgetTree() const { return bUsingDesignerWidgetTree; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Surfing")
	bool IsUsing3DScene() const { return bUsing3DScene; }

	AFF_SurfingSceneActor* Get3DSceneActor() const { return SurfingSceneActor; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing|Visual Style")
	FFF_SurfingVisualStyle VisualStyle;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(
		const FGeometry& InGeometry,
		const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyUp(
		const FGeometry& InGeometry,
		const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnAnalogValueChanged(
		const FGeometry& InGeometry,
		const FAnalogInputEvent& InAnalogInputEvent) override;
	virtual FReply NativeOnMouseButtonDown(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

private:
	bool BuildNativeWidgetTree();
	void ResolveBindingsFromWidgetTree();
	bool HasUsableDesignerBindings() const;
	void InitializeBindings();
	bool Initialize3DScene();
	void Release3DScene();
	void Set3DPresentationActive(bool bActive);
	void RefreshBackdropBrush();
	void ResetTransientPresentationState();
	void RefreshPresentation();
	void RefreshFocusPresentation();
	void RefreshArenaLayout();
	void RefreshEntityWidgets();
	UBorder* AcquireEntityWidget(int32 ObjectId);
	void RecycleMissingEntityWidgets(const TSet<int32>& VisibleObjectIds);
	void ConfigureEntityWidget(
		UBorder& EntityWidget,
		const FFF_SurfingObjectSnapshot& ObjectSnapshot) const;
	FVector2D GetProjectedArenaHalfExtents(const FVector2D& LocalSize) const;
	FVector2D ConvertDisplayDirectionToArenaInput(const FVector2D& DisplayDirection) const;
	bool TrySetMovementInput(const FVector2D& InputAxis);
	bool TryBoost();
	void UpdateHeldMovementInput();
	void ReleaseAllMovementInput();

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(Transient)
	TObjectPtr<UFF_SurfingRuntime> Runtime;

	UPROPERTY(Transient)
	FFF_SurfingSnapshot DisplayedSnapshot;

	UPROPERTY(Transient)
	FFF_MinigameResult DisplayedResult;

	bool bUsingDesignerWidgetTree = false;
	bool bBuiltNativeWidgetTree = false;
	bool bBindingsInitialized = false;
	bool bHasSnapshot = false;
	bool bHasResult = false;
	bool bLeftKeyHeld = false;
	bool bRightKeyHeld = false;
	bool bUpKeyHeld = false;
	bool bDownKeyHeld = false;
	FVector2D AnalogMovementInput = FVector2D::ZeroVector;
	int32 PresentedEventSerial = 0;
	EFF_SurfingEventType PresentedEventType = EFF_SurfingEventType::None;
	float EventPulseRemainingSeconds = 0.0f;
	uint32 PresentedEntityHash = 0;
	bool bHasPresentedEntityHash = false;
	bool bUsing3DScene = false;

	UPROPERTY(Transient)
	TObjectPtr<AFF_SurfingSceneActor> SurfingSceneActor;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> SurfingRenderTarget;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UCanvasPanel> Canvas_SurfArea;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Border_SurfBackdrop;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UCanvasPanel> Canvas_EntityLayer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UBorder> Border_SurferAvatar;

	/** Optional inner colour plate. Older designer WBPs continue to use Border_SurferAvatar directly. */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> Border_SurferCore;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UTextBlock> Text_RemainingTime;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UTextBlock> Text_Score;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UTextBlock> Text_Lives;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UTextBlock> Text_Boost;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UTextBlock> Text_Controls;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UBorder> Panel_Result;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Border_ResultDimmer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UTextBlock> Text_Result;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UButton> Button_Close;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UBorder>> EntityWidgetsById;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> RecycledEntityWidgets;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CloseLabel;
};
