#include "Building/Presentation/FF_BuildCatalogWebView.h"

#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Building/Presentation/FF_BuildFavoritesSubsystem.h"
#include "Building/Presentation/FF_BuildingFeatureDiscoverySubsystem.h"
#include "Building/UI/FF_BuildFacilityCard.h"
#include "CommonInputSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Fonts/CompositeFont.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "IWebBrowserWindow.h"
#include "Misc/Base64.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "SWebBrowser.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Tutorial/FF_TutorialSubsystem.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileData.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"
#include "Widgets/Layout/SBox.h"

#if WITH_EDITOR
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/App.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "TextureCompiler.h"
#include "TextureResource.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogFFBuildCatalogWeb, Log, All);

namespace FFBuildCatalogWeb
{
	constexpr const TCHAR* DocumentUrl = TEXT("https://ff-building.invalid/catalog/");
	constexpr const TCHAR* BindingName = TEXT("ffbuilding");
	constexpr double LoadTimeoutSeconds = 15.0;

	bool IsEmbeddedImage(const FString& Uri)
	{
		return Uri.Len() <= 2 * 1024 * 1024 &&
			(Uri.StartsWith(TEXT("data:image/png;base64,")) ||
			 Uri.StartsWith(TEXT("data:image/jpeg;base64,")) ||
			 Uri.StartsWith(TEXT("data:image/webp;base64,")));
	}
}

void UFF_BuildCatalogWebBridge::Attach(UFF_BuildCatalogWebView* InView) { View = InView; }
void UFF_BuildCatalogWebBridge::Detach() { View.Reset(); }

bool UFF_BuildCatalogWebBridge::Select(const FString& FacilityId)
{
	// Reject an unexpected browser thread before resolving or accessing a UObject.
	if (!IsInGameThread()) return false;
	UFF_BuildCatalogWebView* Owner = View.Get();
	return Owner && Owner->SelectFacility(FacilityId);
}

bool UFF_BuildCatalogWebBridge::Favorite(const FString& FacilityId, bool bFavorite)
{
	if (!IsInGameThread()) return false;
	UFF_BuildCatalogWebView* Owner = View.Get();
	return Owner && Owner->SetFacilityFavorite(FacilityId, bFavorite);
}

void UFF_BuildCatalogWebBridge::Ready()
{
	if (!IsInGameThread()) return;
	if (UFF_BuildCatalogWebView* Owner = View.Get()) Owner->AcknowledgeReady();
}

bool UFF_BuildCatalogWebBridge::DismissLightingIntroduction()
{
	if (!IsInGameThread()) return false;
	UFF_BuildCatalogWebView* Owner = View.Get();
	return Owner && Owner->DismissLightingIntroduction();
}

UFF_PrepPanelBuild* UFF_BuildCatalogWebView::FindPanel() const
{
	return GetTypedOuter<UFF_PrepPanelBuild>();
}

TSharedRef<SWidget> UFF_BuildCatalogWebView::RebuildWidget()
{
	bWebReady = false;
	bDocumentLoaded = false;
	bDocumentFailed = false;
	SnapshotPushCount = 0;
	LastWebItemCount = 0;
	LastSnapshotMetadata.Reset();
	SentThumbnailKeys.Reset();
	bDocumentSupportsThumbnailCache = DocumentHTML.Contains(TEXT("FF_BUILDING_THUMBNAIL_CACHE_V1"));
	LastWebError.Reset();
	LastWebDiagnostics.Reset();
	UWorld* World = GetWorld();
	// Designer and playerless automation continue to use the authored native list.
	if (GetVisibility() == ESlateVisibility::Collapsed)
	{
		// A collapsed authored host opts into the editable native UMG catalog.
		// Do not start Chromium or alter the native list in this configuration.
		return SNew(SBox).Visibility(EVisibility::Collapsed);
	}
	if (IsDesignTime() || !World || !World->IsGameWorld() || DocumentHTML.IsEmpty())
	{
		ApplyFallbackVisibility();
		return SNew(SBox).HeightOverride(210.f).Visibility(EVisibility::Hidden);
	}
	Bridge = NewObject<UFF_BuildCatalogWebBridge>(this);
	Bridge->Attach(this);
	LoadedDocument = DocumentHTML;
	DocumentStartTime = 0.0;
	NextSnapshotTime = 0.0;
	Browser = SNew(SWebBrowser)
		.InitialURL(FFBuildCatalogWeb::DocumentUrl)
		.ContentsToLoad(TOptional<FString>(SecuredDocument()))
		.ShowControls(false)
		.ShowAddressBar(false)
		.ShowErrorMessage(false)
		.ShowInitialThrobber(false)
		.SupportsTransparency(bSupportsTransparency)
		.SupportsThumbMouseButtonNavigation(false)
		.BackgroundColor(FColor(249, 249, 255, 255))
		.BrowserFrameRate(30)
		.OnLoadStarted(FSimpleDelegate::CreateUObject(this, &ThisClass::OnDocumentStarted))
		.OnLoadCompleted(FSimpleDelegate::CreateUObject(this, &ThisClass::OnDocumentLoaded))
		.OnLoadError(FSimpleDelegate::CreateUObject(this, &ThisClass::OnDocumentFailed))
		.OnBeforeNavigation_Lambda([](const FString& Url, const FWebNavigationRequest& Request)
		{
			return !Request.bIsMainFrame || Url != FFBuildCatalogWeb::DocumentUrl;
		})
		.OnBeforePopup_Lambda([](FString, FString) { return true; })
		.OnSuppressContextMenu_Lambda([]() { return true; })
		.OnConsoleMessage(FOnConsoleMessageDelegate::CreateUObject(this, &ThisClass::OnConsoleMessage));
	BindBridge();
	// Visibility is reasserted each Slate frame; JSON comparison is throttled below.
	// Native reconciliation can change TileView visibility after any UMG filter action.
	ActiveTimer = Browser->RegisterActiveTimer(0.f,
		FWidgetActiveTimerDelegate::CreateUObject(this, &ThisClass::TickPresentation));
	ApplyFallbackVisibility();
	return Browser.ToSharedRef();
}

FString UFF_BuildCatalogWebView::SecuredDocument() const
{
	const FString Policy = TEXT("<meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; img-src data:; font-src data:; connect-src 'none'; frame-src 'none'; object-src 'none'; base-uri 'none'; form-action 'none'\">");
	FString Html = DocumentHTML;
	const int32 Head = Html.Find(TEXT("<head>"), ESearchCase::IgnoreCase);
	if (Head != INDEX_NONE) Html.InsertAt(Head + 6, Policy);
	else Html = Policy + Html;
	return Html;
}

void UFF_BuildCatalogWebView::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	// Authoring can update embedded images without changing any gameplay metadata.
	LastSnapshotMetadata.Reset();
	SentThumbnailKeys.Reset();
	if (Browser && LoadedDocument != DocumentHTML)
	{
		LoadedDocument = DocumentHTML;
		OnDocumentStarted();
		Browser->LoadString(SecuredDocument(), FFBuildCatalogWeb::DocumentUrl);
	}
	ApplyFallbackVisibility();
}

void UFF_BuildCatalogWebView::BindBridge()
{
	if (Browser && Bridge)
	{
		// Temporary bindings disappear on navigation. No gameplay UObject is exposed.
		Browser->BindUObject(FFBuildCatalogWeb::BindingName, Bridge, false);
	}
}

void UFF_BuildCatalogWebView::OnDocumentStarted()
{
	if (!IsInGameThread()) return;
	bWebReady = false;
	bDocumentLoaded = false;
	bDocumentFailed = false;
	LastSnapshotMetadata.Reset();
	SentThumbnailKeys.Reset();
	bDocumentSupportsThumbnailCache = DocumentHTML.Contains(TEXT("FF_BUILDING_THUMBNAIL_CACHE_V1"));
	DocumentStartTime = 0.0;
	BindBridge();
	ApplyFallbackVisibility();
}

void UFF_BuildCatalogWebView::OnDocumentLoaded()
{
	if (!IsInGameThread() || !Browser || Browser->GetUrl() != FFBuildCatalogWeb::DocumentUrl) return;
	bDocumentLoaded = true;
	bDocumentFailed = false;
	BindBridge();
	LastSnapshotMetadata.Reset();
	NextSnapshotTime = 0.0;
}

void UFF_BuildCatalogWebView::OnDocumentFailed()
{
	if (!IsInGameThread()) return;
	bWebReady = false;
	bDocumentLoaded = false;
	bDocumentFailed = true;
	LastWebError = TEXT("The local card document did not finish loading/rendering; native cards remain available.");
	ApplyFallbackVisibility();
}

void UFF_BuildCatalogWebView::OnConsoleMessage(const FString& Message, const FString& Source, int32 Line, EWebBrowserConsoleLogSeverity Severity)
{
	(void)Source;
	(void)Line;
	constexpr const TCHAR* Prefix = TEXT("FF_BUILD_WEB_METRICS=");
	if (!IsInGameThread() || !Browser || Browser->GetUrl() != FFBuildCatalogWeb::DocumentUrl ||
		(Severity != EWebBrowserConsoleLogSeverity::Info && Severity != EWebBrowserConsoleLogSeverity::Default) ||
		!Message.StartsWith(Prefix)) return;
	LastWebDiagnostics = Message.Mid(FCString::Strlen(Prefix), 8192);
	UE_LOG(LogFFBuildCatalogWeb, Display, TEXT("FF_BUILD_WEB_METRICS=%s"), *LastWebDiagnostics);
}

void UFF_BuildCatalogWebView::AcknowledgeReady()
{
	if (!Browser || !bDocumentLoaded || bDocumentFailed || SnapshotPushCount <= 0 ||
		Browser->GetUrl() != FFBuildCatalogWeb::DocumentUrl) return;
	bWebReady = true;
	LastWebError.Reset();
	ApplyFallbackVisibility();
}

void UFF_BuildCatalogWebView::ApplyFallbackVisibility()
{
	if (!IsInGameThread()) return;
	if (GetVisibility() == ESlateVisibility::Collapsed) return;
	UFF_PrepPanelBuild* Panel = FindPanel();
	const UCommonInputSubsystem* Input = Panel ? UCommonInputSubsystem::Get(Panel->GetOwningLocalPlayer()) : nullptr;
	const bool bGamepad = Browser && Input && Input->GetCurrentInputType() == ECommonInputType::Gamepad;
	const bool bTransferToNative = bGamepad && !bUsingNativeGamepadFallback && Browser &&
		(Browser->HasKeyboardFocus() || Browser->HasFocusedDescendants());
	bUsingNativeGamepadFallback = bGamepad;
	const bool bShowWeb = bWebReady && !bGamepad;
	if (Browser)
	{
		// Keep its geometry and off-screen Chromium renderer alive during the handshake.
		// Hidden would prevent the first paint/tick from establishing the logical size.
		const EVisibility DesiredVisibility = bShowWeb ? EVisibility::Visible : EVisibility::HitTestInvisible;
		if (Browser->GetVisibility() != DesiredVisibility) Browser->SetVisibility(DesiredVisibility);
		const float DesiredOpacity = bShowWeb ? 1.f : 0.f;
		if (Browser->GetRenderOpacity() != DesiredOpacity) Browser->SetRenderOpacity(DesiredOpacity);
	}
	else if (const TSharedPtr<SWidget> Placeholder = GetCachedWidget())
	{
		// UWidget::SynchronizeProperties reapplies the saved Visible state after
		// RebuildWidget creates the playerless/empty-document placeholder. Keep
		// that transparent overlay out of the hit grid so native cards, including
		// their independent favorite buttons, remain usable in every fallback.
		Placeholder->SetVisibility(EVisibility::HitTestInvisible);
		Placeholder->SetRenderOpacity(0.f);
	}
	UWidget* Native = Panel ? Panel->GetWidgetFromName(NativeFallbackWidgetName) : nullptr;
	if (!Native || Native == this) return;
	// Native panel reconciliation can restore visibility later in the same frame.
	// Opacity prevents those hidden fallback cards from painting through web gaps.
	const float NativeOpacity = bShowWeb ? 0.f : 1.f;
	if (Native->GetRenderOpacity() != NativeOpacity) Native->SetRenderOpacity(NativeOpacity);
	// The web overlay fills its own host. Hidden native rows still participate in layout;
	// collapse them after the handshake, and restore them for errors/controller navigation.
	if (bShowWeb && Native->GetVisibility() != ESlateVisibility::Collapsed)
		Native->SetVisibility(ESlateVisibility::Collapsed);
	else if (!bShowWeb && (bUseNativeFallbackOnError || bGamepad) && Native->GetVisibility() != ESlateVisibility::Visible)
		Native->SetVisibility(ESlateVisibility::Visible);
	const UWidget* CatalogPanel = Panel->GetWidgetFromName(TEXT("Panel_Catalog"));
	if (bTransferToNative && Panel->IsVisible() && (!CatalogPanel || CatalogPanel->IsVisible()))
		Native->SetUserFocus(Panel->GetOwningPlayer());
}

EActiveTimerReturnType UFF_BuildCatalogWebView::TickPresentation(double CurrentTime, float DeltaTime)
{
	(void)DeltaTime;
	if (!Browser || !IsInGameThread()) return EActiveTimerReturnType::Stop;
	UFF_PrepPanelBuild* Panel = FindPanel();
	const UWidget* CatalogPanel = Panel ? Panel->GetWidgetFromName(TEXT("Panel_Catalog")) : nullptr;
	const bool bCatalogVisible = Panel && Panel->IsVisible() && (!CatalogPanel || CatalogPanel->IsVisible());
	// A HUD may construct the catalog while it is collapsed for minutes. Count only
	// a visible loading attempt, so opening building mode later still initializes CEF.
	if (!bCatalogVisible) DocumentStartTime = 0.0;
	else if (!bWebReady && !bDocumentFailed)
	{
		if (DocumentStartTime <= 0.0) DocumentStartTime = FPlatformTime::Seconds();
		else if (FPlatformTime::Seconds() - DocumentStartTime > FFBuildCatalogWeb::LoadTimeoutSeconds) OnDocumentFailed();
	}
	ApplyFallbackVisibility();
	if (bCatalogVisible && bDocumentLoaded && !bDocumentFailed && CurrentTime >= NextSnapshotTime)
	{
		NextSnapshotTime = CurrentTime + FMath::Clamp(RefreshInterval, .03f, 1.f);
		PushSnapshot();
	}
	return EActiveTimerReturnType::Continue;
}

void UFF_BuildCatalogWebView::PushSnapshot()
{
	UFF_PrepPanelBuild* Panel = FindPanel();
	if (!Panel || !Browser || !Panel->IsVisible()) return;
	UWorld* World = Panel->GetWorld();
	UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
	const auto* Catalog = Game ? Game->GetSubsystem<UFF_BuildCatalogSubsystem>() : nullptr;
	const auto* Favorites = Game ? Game->GetSubsystem<UFF_BuildFavoritesSubsystem>() : nullptr;
	const auto* BuildMode = World ? World->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
	const auto* Discovery = Game ? Game->GetSubsystem<UFF_BuildingFeatureDiscoverySubsystem>() : nullptr;
	const auto* Tutorial = World ? World->GetSubsystem<UFF_TutorialSubsystem>() : nullptr;
	const bool bTutorialLevel = Tutorial && Tutorial->IsTutorialLevel();
	const bool Active = BuildMode && BuildMode->GetBuildModeState() == EFF_BuildModeState::CatalogOpen;
	const TSharedRef<FJsonObject> Snapshot = MakeShared<FJsonObject>();
	Snapshot->SetNumberField(TEXT("version"), 1);
	Snapshot->SetStringField(TEXT("selectedId"), Panel->GetSelectedCatalogFacilityId().ToString());
	Snapshot->SetBoolField(TEXT("loading"), !Catalog || Catalog->GetLoadState() == EFF_BuildCatalogLoadState::Loading || Catalog->GetLoadState() == EFF_BuildCatalogLoadState::NotRequested);
	Snapshot->SetBoolField(TEXT("canInteract"), Active && !bUsingNativeGamepadFallback && Catalog && Catalog->IsCatalogReady());
	Snapshot->SetStringField(TEXT("error"), Catalog ? Catalog->GetLastError().ToString() : FString());
	const FVector2D LocalSize = Browser->GetCachedGeometry().GetLocalSize();
	Snapshot->SetNumberField(TEXT("logicalWidth"), LocalSize.X);
	Snapshot->SetNumberField(TEXT("logicalHeight"), LocalSize.Y);
	TArray<TSharedPtr<FJsonValue>> Items;
	TArray<const FString*> Images;
	bool bHasLightingFacility = false;
	for (UObject* Item : Panel->GetVisibleCatalogItemsForWeb())
	{
		const auto* Tile = Cast<UFF_PrepBuildTileData>(Item);
		if (!Tile) continue;
		const FString Id = Tile->GetFacilityDefinitionId().ToString();
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("id"), Id);
		Entry->SetStringField(TEXT("name"), Tile->GetDisplayName().ToString());
		Entry->SetNumberField(TEXT("cost"), Tile->GetInstallCost());
		Entry->SetStringField(TEXT("costText"), Tile->GetCostText().ToString());
		Entry->SetBoolField(TEXT("affordable"), Tile->IsAffordable());
		Entry->SetBoolField(TEXT("buildLimitReached"), Tile->GetTileState() == EFF_PrepBuildTileState::BuildLimitReached);
		Entry->SetBoolField(TEXT("favorite"), Favorites && Favorites->IsFavorite(Tile->GetFacilityDefinitionId()));
		Entry->SetBoolField(TEXT("required"), Tile->IsRequiredFacility());
		Entry->SetBoolField(TEXT("requiredSatisfied"), Tile->IsRequiredFacilitySatisfied());
		Entry->SetNumberField(TEXT("requiredCurrent"), Tile->GetRequiredFacilityCurrentCount());
		Entry->SetNumberField(TEXT("requiredMinimum"), Tile->GetRequiredFacilityMinimumCount());
		Entry->SetBoolField(TEXT("program"), Tile->HasCompatiblePrograms());
		Entry->SetBoolField(TEXT("lighting"), Tile->SupportsLighting());
		bHasLightingFacility |= Tile->SupportsLighting();
		Entry->SetBoolField(TEXT("synergy"), Tile->IsSynergyHighlighted());
		Entry->SetBoolField(TEXT("codexFocused"), Tile->IsCodexFocused());
		Entry->SetStringField(TEXT("stateText"), Tile->GetStateText().ToString());
		Entry->SetStringField(TEXT("stateLabel"), Tile->GetStateLabelText().ToString());
		FString ToolTip = Tile->GetDisplayName().ToString() + TEXT("\n") + Tile->GetCategoryPathText().ToString() +
			TEXT("\n") + Tile->GetCostText().ToString() + TEXT(" · ") + Tile->GetBuildLimitText().ToString() +
			TEXT("\n") + Tile->GetStateText().ToString();
		if (Tile->IsRequiredFacility())
		{
			ToolTip += TEXT("\n") + UFF_BuildFacilityCard::MakeRequiredFacilityToolTip(Tile->IsRequiredFacilitySatisfied(),
				Tile->GetRequiredFacilityCurrentCount(), Tile->GetRequiredFacilityMinimumCount()).ToString();
			const FFF_BuildCatalogEntry& Required = Tile->GetEntry();
			if (Required.RequiredStaffCount > 0)
				ToolTip += TEXT("\n") + UFF_BuildFacilityCard::MakeRequiredStaffToolTip(
					Required.RequiredAssignedStaffCount, Required.RequiredStaffCount).ToString();
		}
		if (Tile->HasCompatiblePrograms())
			ToolTip += TEXT("\n") + UFF_BuildFacilityCard::MakeProgramFacilityToolTip(Tile->GetEntry()).ToString();
		if (Tile->SupportsLighting())
			ToolTip += TEXT("\n") + UFF_BuildFacilityCard::MakeLightingToolTip().ToString();
		Entry->SetStringField(TEXT("tooltip"), ToolTip);
		const FString ThumbnailPath = Tile->GetThumbnail().ToSoftObjectPath().ToString();
		const FString* Image = ThumbnailDataUris.Find(Id);
		if (!Image) Image = ThumbnailDataUris.Find(ThumbnailPath);
		// Compare only small metadata first. Unchanged frames never copy/serialize image bytes.
		Entry->SetStringField(TEXT("thumbnailKey"), Id + TEXT("|") + ThumbnailPath);
		Entry->SetNumberField(TEXT("thumbnailCharacters"), Image ? Image->Len() : 0);
		Images.Add(Image);
		Items.Add(MakeShared<FJsonValueObject>(Entry));
	}
	LastWebItemCount = Items.Num();
	Snapshot->SetArrayField(TEXT("items"), Items);
	// Tutorial input is owned by its guide. Keep the catalog unobstructed for the entire level,
	// and leave first-use history untouched so the introduction can appear in a normal festival.
	Snapshot->SetBoolField(TEXT("showLightingIntroduction"), !bTutorialLevel && Active && bHasLightingFacility && Discovery &&
		!Discovery->HasSeenFeature(UFF_BuildingFeatureDiscoverySubsystem::LightingControlsFeatureId()));
	FString Metadata;
	const auto MetadataWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Metadata);
	if (!FJsonSerializer::Serialize(Snapshot, MetadataWriter) || Metadata == LastSnapshotMetadata) return;
	TArray<FString> NewlySentThumbnailKeys;
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		const TSharedPtr<FJsonObject>& Entry = Items[Index]->AsObject();
		Entry->RemoveField(TEXT("thumbnailCharacters"));
		const FString ThumbnailKey = Entry->GetStringField(TEXT("thumbnailKey"));
		const FString* Image = Images[Index];
		// Old saved documents remain compatible until their HTML is synchronized. Updated
		// documents need image bytes only once, even when gold/selection changes frequently.
		if (!bDocumentSupportsThumbnailCache || !SentThumbnailKeys.Contains(ThumbnailKey))
		{
			Entry->SetStringField(TEXT("thumbnail"), Image && FFBuildCatalogWeb::IsEmbeddedImage(*Image) ? *Image : FString());
			NewlySentThumbnailKeys.Add(ThumbnailKey);
		}
	}
	FString Json;
	const auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
	if (!FJsonSerializer::Serialize(Snapshot, Writer)) return;
	for (const FString& Key : NewlySentThumbnailKeys) SentThumbnailKeys.Add(Key);
	LastSnapshotMetadata = MoveTemp(Metadata);
	// JSON serialization escapes arbitrary facility text. No text is interpolated into JS strings.
	Json.ReplaceInline(TEXT("\u2028"), TEXT("\\u2028"));
	Json.ReplaceInline(TEXT("\u2029"), TEXT("\\u2029"));
	Browser->ExecuteJavascript(TEXT("if(window.ffBuilding&&typeof window.ffBuilding.render==='function'){window.ffBuilding.render(") + Json + TEXT(");}"));
	++SnapshotPushCount;
}

UFF_PrepBuildTileData* UFF_BuildCatalogWebView::FindActionTile(const FString& FacilityId) const
{
	if (!IsInGameThread() || !bWebReady || bUsingNativeGamepadFallback || !Browser || bDocumentFailed || FacilityId.IsEmpty() || FacilityId.Len() > 256 ||
		Browser->GetUrl() != FFBuildCatalogWeb::DocumentUrl) return nullptr;
	UFF_PrepPanelBuild* Panel = FindPanel();
	UWorld* World = Panel ? Panel->GetWorld() : nullptr;
	APlayerController* Controller = Panel ? Panel->GetOwningPlayer() : nullptr;
	if (!World || !World->IsGameWorld() || !Controller || !Controller->IsLocalController() ||
		Controller->GetWorld() != World || !Panel->IsVisible() || (!Panel->GetParent() && !Panel->IsInViewport())) return nullptr;
	const auto* Mode = World->GetSubsystem<UFF_BuildModeSubsystem>();
	if (!Mode || Mode->GetBuildModeState() != EFF_BuildModeState::CatalogOpen) return nullptr;
	// Resolve only an already-authorized visible ID, without interning untrusted strings into FName.
	for (UObject* Item : Panel->GetVisibleCatalogItemsForWeb())
	{
		if (auto* Tile = Cast<UFF_PrepBuildTileData>(Item))
		{
			if (Tile->GetFacilityDefinitionId().ToString() == FacilityId) return Tile;
		}
	}
	return nullptr;
}

bool UFF_BuildCatalogWebView::SelectFacility(const FString& FacilityId)
{
	LastWebAction = TEXT("select:") + FacilityId.Left(256);
	bLastWebActionSucceeded = false;
	UFF_PrepBuildTileData* Tile = FindActionTile(FacilityId);
	UFF_PrepPanelBuild* Panel = FindPanel();
	if (Tile && Panel) bLastWebActionSucceeded = Panel->SelectCatalogFacility(Tile->GetFacilityDefinitionId());
	NextSnapshotTime = 0.0;
	return bLastWebActionSucceeded;
}

bool UFF_BuildCatalogWebView::SetFacilityFavorite(const FString& FacilityId, bool bFavorite)
{
	LastWebAction = TEXT("favorite:") + FacilityId.Left(256);
	bLastWebActionSucceeded = false;
	UFF_PrepBuildTileData* Tile = FindActionTile(FacilityId);
	UFF_PrepPanelBuild* Panel = FindPanel();
	UGameInstance* Game = Panel ? Panel->GetGameInstance() : nullptr;
	auto* Favorites = Game ? Game->GetSubsystem<UFF_BuildFavoritesSubsystem>() : nullptr;
	auto* Catalog = Game ? Game->GetSubsystem<UFF_BuildCatalogSubsystem>() : nullptr;
	FFF_BuildCatalogEntry Actual;
	if (Tile && Favorites && Catalog && Catalog->FindEntry(Tile->GetFacilityDefinitionId(), Actual))
	{
		const FName Id = Tile->GetFacilityDefinitionId();
		Favorites->SetFavorite(Id, bFavorite);
		bLastWebActionSucceeded = Favorites->IsFavorite(Id) == bFavorite;
	}
	NextSnapshotTime = 0.0;
	return bLastWebActionSucceeded;
}

bool UFF_BuildCatalogWebView::DismissLightingIntroduction()
{
	UFF_PrepPanelBuild* Panel = FindPanel();
	UWorld* World = Panel ? Panel->GetWorld() : nullptr;
	const auto* Tutorial = World ? World->GetSubsystem<UFF_TutorialSubsystem>() : nullptr;
	if (Tutorial && Tutorial->IsTutorialLevel()) return false;
	UGameInstance* Game = Panel ? Panel->GetGameInstance() : nullptr;
	auto* Discovery = Game ? Game->GetSubsystem<UFF_BuildingFeatureDiscoverySubsystem>() : nullptr;
	if (!Panel || !Discovery) return false;
	// Reuse selection's world, visibility, mode and bridge-origin checks without placing a facility.
	for (UObject* Item : Panel->GetVisibleCatalogItemsForWeb())
	{
		const auto* Tile = Cast<UFF_PrepBuildTileData>(Item);
		if (Tile && Tile->SupportsLighting() && FindActionTile(Tile->GetFacilityDefinitionId().ToString()))
		{
			Discovery->MarkFeatureSeen(UFF_BuildingFeatureDiscoverySubsystem::LightingControlsFeatureId());
			NextSnapshotTime = 0.0;
			return true;
		}
	}
	return false;
}

bool UFF_BuildCatalogWebView::ExecutePreviewJavascript(const FString& Script)
{
#if WITH_EDITOR
	if (IsInGameThread() && bWebReady && Browser && GetWorld() && GetWorld()->WorldType == EWorldType::PIE &&
		Browser->GetUrl() == FFBuildCatalogWeb::DocumentUrl)
	{
		Browser->ExecuteJavascript(Script);
		return true;
	}
#endif
	return false;
}

FString UFF_BuildCatalogWebView::GetEmbeddedFontDataUri(UFont* Font, FName TypefaceName)
{
#if WITH_EDITOR
	if (!IsInGameThread() || !IsValid(Font)) return FString();
	const FCompositeFont* Composite = Font->GetCompositeFont();
	if (!Composite) return FString();
	for (const FTypefaceEntry& Entry : Composite->DefaultTypeface.Fonts)
	{
		if (!TypefaceName.IsNone() && Entry.Name != TypefaceName) continue;
		const FFontFaceDataConstPtr Face = Entry.Font.GetFontFaceData();
		if (Face.IsValid() && !Face->GetData().IsEmpty())
			return TEXT("data:font/otf;base64,") + FBase64::Encode(Face->GetData());
	}
#endif
	return FString();
}

FString UFF_BuildCatalogWebView::GetEmbeddedThumbnailDataUri(UTexture2D* Texture, int32 MaxDimension)
{
#if WITH_EDITOR
	if (!IsInGameThread())
	{
		UE_LOG(LogFFBuildCatalogWeb, Warning, TEXT("Thumbnail export refused outside the game thread."));
		return FString();
	}
	if (!IsValid(Texture))
	{
		UE_LOG(LogFFBuildCatalogWeb, Warning, TEXT("Thumbnail export received an invalid texture."));
		return FString();
	}
	if (!GEditor || !FApp::CanEverRender() || !GIsRHIInitialized || GUsingNullRHI)
	{
		UE_LOG(LogFFBuildCatalogWeb, Warning, TEXT("Thumbnail export has no usable renderer: %s Editor=%d CanRender=%d RHIInitialized=%d NullRHI=%d"),
			*Texture->GetPathName(), GEditor != nullptr, FApp::CanEverRender(), !!GIsRHIInitialized, !!GUsingNullRHI);
		return FString();
	}
	// LoadObject can return before the source texture's GPU resource is ready.
	TArray<UTexture*> TexturesToFinish { Texture };
	FTextureCompilingManager::Get().FinishCompilation(TexturesToFinish);
	Texture->SetForceMipLevelsToBeResident(30.f);
	Texture->WaitForStreaming();
	if (!Texture->GetResource() || Texture->GetSizeX() <= 0 || Texture->GetSizeY() <= 0)
	{
		UE_LOG(LogFFBuildCatalogWeb, Warning, TEXT("Thumbnail export found no compiled texture resource: %s"), *Texture->GetPathName());
		return FString();
	}
	const float Scale = FMath::Min(1.f, static_cast<float>(FMath::Clamp(MaxDimension, 32, 256)) /
		FMath::Max(Texture->GetSizeX(), Texture->GetSizeY()));
	const int32 Width = FMath::Max(1, FMath::RoundToInt(Texture->GetSizeX() * Scale));
	const int32 Height = FMath::Max(1, FMath::RoundToInt(Texture->GetSizeY() * Scale));
	UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
	Target->ClearColor = FLinearColor::Transparent;
	// The sRGB render target already encodes linear samples when the GPU writes them.
	// Disable Canvas's extra 1/2.2 gamma pass to avoid encoding the thumbnail twice.
	Target->TargetGamma = 1.f;
	Target->InitCustomFormat(Width, Height, PF_B8G8R8A8, false);
	Target->UpdateResourceImmediate(true);
	FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
	if (!Resource)
	{
		UE_LOG(LogFFBuildCatalogWeb, Warning, TEXT("Thumbnail export could not create a transient render target: %s"), *Texture->GetPathName());
		return FString();
	}
	FCanvas Canvas(Resource, nullptr, FGameTime::GetTimeSinceAppStart(), GMaxRHIFeatureLevel);
	Canvas.Clear(FLinearColor::Transparent);
	FCanvasTileItem Tile(FVector2D::ZeroVector, Texture->GetResource(), FVector2D(Width, Height), FLinearColor::White);
	// Opaque here means overwrite RGBA, not force alpha to one. Translucent blending
	// would bake a background/premultiply the RGB and lose the source's straight alpha.
	Tile.BlendMode = SE_BLEND_Opaque;
	Canvas.DrawItem(Tile);
	Canvas.Flush_GameThread();
	TArray<FColor> Pixels;
	FReadSurfaceDataFlags ReadFlags(RCM_UNorm);
	ReadFlags.SetLinearToGamma(false);
	if (!Resource->ReadPixels(Pixels, ReadFlags) || Pixels.Num() != Width * Height)
	{
		UE_LOG(LogFFBuildCatalogWeb, Warning, TEXT("Thumbnail export returned invalid pixels: %s Size=%dx%d Pixels=%d"),
			*Texture->GetPathName(), Width, Height, Pixels.Num());
		return FString();
	}
	const TArrayView64<const FColor> Colors(Pixels.GetData(), Pixels.Num());
	TArray64<uint8> Png;
	FImageUtils::PNGCompressImageArray(Width, Height, Colors, Png);
	if (Png.IsEmpty() || Png.Num() > 1024 * 1024)
	{
		UE_LOG(LogFFBuildCatalogWeb, Warning, TEXT("Thumbnail export PNG compression failed or exceeded limit: %s Bytes=%lld"),
			*Texture->GetPathName(), Png.Num());
		return FString();
	}
	FString Uri = TEXT("data:image/png;base64,") + FBase64::Encode(Png.GetData(), static_cast<uint32>(Png.Num()));
	if (!FFBuildCatalogWeb::IsEmbeddedImage(Uri))
	{
		UE_LOG(LogFFBuildCatalogWeb, Warning, TEXT("Thumbnail export URI exceeds limit: %s Characters=%d"), *Texture->GetPathName(), Uri.Len());
		return FString();
	}
	return Uri;
#else
	return FString();
#endif
}

void UFF_BuildCatalogWebView::ReleaseSlateResources(bool bReleaseChildren)
{
	if (Browser)
	{
		if (const TSharedPtr<FActiveTimerHandle> Timer = ActiveTimer.Pin()) Browser->UnRegisterActiveTimer(Timer.ToSharedRef());
		if (Bridge) Browser->UnbindUObject(FFBuildCatalogWeb::BindingName, Bridge, false);
	}
	ActiveTimer.Reset();
	if (Bridge) Bridge->Detach();
	Bridge = nullptr;
	bWebReady = false;
	bDocumentLoaded = false;
	SentThumbnailKeys.Reset();
	ApplyFallbackVisibility();
	Browser.Reset();
	Super::ReleaseSlateResources(bReleaseChildren);
}

#if WITH_EDITOR
const FText UFF_BuildCatalogWebView::GetPaletteCategory()
{
	return NSLOCTEXT("FFBuildCatalogWeb", "PaletteCategory", "FestaFiesta Building");
}
#endif
