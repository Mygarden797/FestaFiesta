#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "FF_BuildCatalogWebView.generated.h"

class FActiveTimerHandle;
class SWebBrowser;
class UFont;
class UTexture2D;
class UFF_BuildCatalogWebView;
class UFF_PrepPanelBuild;
class UFF_PrepBuildTileData;
enum class EActiveTimerReturnType : uint8;
enum class EWebBrowserConsoleLogSeverity;

/** Only the explicit presentation actions are bound to JavaScript, never the gameplay owner. */
UCLASS(Transient)
class FESTAFIESTA_API UFF_BuildCatalogWebBridge final : public UObject
{

	GENERATED_BODY()

public:
	void Attach(UFF_BuildCatalogWebView* InView);
	void Detach();

	UFUNCTION()
	bool Select(const FString& FacilityId);

	UFUNCTION()
	bool Favorite(const FString& FacilityId, bool bFavorite);

	/** The local document acknowledges its first successfully rendered snapshot. */
	UFUNCTION()
	void Ready();

	/** Acknowledges the lighting explanation after the player presses its confirmation button. */
	UFUNCTION()
	bool DismissLightingIntroduction();

private:
	TWeakObjectPtr<UFF_BuildCatalogWebView> View;
};

/**
 * Building-only HTML card ribbon. The surrounding UMG filters and the original
 * TileView remain the authoritative presentation model and native fallback.
 * HTML and thumbnail bytes are saved in this widget, requiring no loose files.
 */
UCLASS(meta = (DisplayName = "FF Build Catalog Web Ribbon"))
class FESTAFIESTA_API UFF_BuildCatalogWebView final : public UWidget
{

	GENERATED_BODY()

public:
	/** Local HTML must provide window.ffBuilding.render(snapshot). No external resources. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building Web|Document", meta = (MultiLine = true))
	FString DocumentHTML;

	/** FacilityDefinitionId (or thumbnail soft-object path) -> embedded PNG/JPEG/WebP data URI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building Web|Document")
	TMap<FString, FString> ThumbnailDataUris;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building Web|Appearance")
	bool bSupportsTransparency = true;

	/** Seconds between snapshot comparisons. Only changed snapshots cross the browser boundary. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building Web|Behavior", meta = (ClampMin = "0.03", ClampMax = "1.0"))
	float RefreshInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building Web|Behavior")
	FName NativeFallbackWidgetName = TEXT("TileView_Facilities");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building Web|Behavior")
	bool bUseNativeFallbackOnError = true;

	/** True only after the local page acknowledges an actual native snapshot render. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Building Web|Status")
	bool bWebReady = false;

	/** Controller navigation uses the original TileView and its existing accept handler. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Building Web|Status")
	bool bUsingNativeGamepadFallback = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Building Web|Status")
	int32 SnapshotPushCount = 0;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Building Web|Status")
	int32 LastWebItemCount = 0;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Building Web|Status")
	FString LastWebAction;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Building Web|Status")
	bool bLastWebActionSucceeded = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Building Web|Status")
	FString LastWebError;

	/** Bounded diagnostic JSON from the local document's FF_BUILD_WEB_METRICS console prefix. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Building Web|Status")
	FString LastWebDiagnostics;

	/** Local PIE validation hook. Has no effect outside editor builds; never exposed to JS. */
	UFUNCTION(BlueprintCallable, Category = "Building Web|Development", meta = (DevelopmentOnly))
	bool ExecutePreviewJavascript(const FString& Script);

	/** Editor authoring helper: embed an existing project font in the saved local HTML. */
	UFUNCTION(BlueprintCallable, Category = "Building Web|Development", meta = (DevelopmentOnly))
	static FString GetEmbeddedFontDataUri(UFont* Font, FName TypefaceName);

	/** Editor authoring helper: render a bounded thumbnail without caching or saving the source asset. */
	UFUNCTION(BlueprintCallable, Category = "Building Web|Development", meta = (DevelopmentOnly))
	static FString GetEmbeddedThumbnailDataUri(UTexture2D* Texture, int32 MaxDimension = 256);

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	friend class UFF_BuildCatalogWebBridge;
	UFF_PrepPanelBuild* FindPanel() const;
	UFF_PrepBuildTileData* FindActionTile(const FString& FacilityId) const;
	bool SelectFacility(const FString& FacilityId);
	bool SetFacilityFavorite(const FString& FacilityId, bool bFavorite);
	void AcknowledgeReady();
	bool DismissLightingIntroduction();
	void BindBridge();
	void OnDocumentStarted();
	void OnDocumentLoaded();
	void OnDocumentFailed();
	void OnConsoleMessage(const FString& Message, const FString& Source, int32 Line, EWebBrowserConsoleLogSeverity Severity);
	void ApplyFallbackVisibility();
	void PushSnapshot();
	FString SecuredDocument() const;
	EActiveTimerReturnType TickPresentation(double CurrentTime, float DeltaTime);

	UPROPERTY(Transient)
	TObjectPtr<UFF_BuildCatalogWebBridge> Bridge;

	TSharedPtr<SWebBrowser> Browser;
	TWeakPtr<FActiveTimerHandle> ActiveTimer;
	FString LoadedDocument;
	FString LastSnapshotMetadata;
	// A document keeps decoded thumbnails across state/filter updates. Reset on navigation.
	TSet<FString> SentThumbnailKeys;
	bool bDocumentSupportsThumbnailCache = false;
	double NextSnapshotTime = 0.0;
	double DocumentStartTime = 0.0;
	bool bDocumentLoaded = false;
	bool bDocumentFailed = false;
};
