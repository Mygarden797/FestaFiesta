// Exercise the configured title WBP without opening a browser or creating an account session.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "AssetCompilingManager.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "UI/FF_CommonButtonBase.h"
#include "UI/Account/FF_GoogleAccountPanel.h"
#include "UI/Title/FF_TitleScreen.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SOverlay.h"

namespace FFTitleGoogleAuthTest
{
	constexpr const TCHAR* TitleClassPath =
		TEXT("/Game/FestaFiesta/UI/Title/GlassOrbit/WBP_TitleMain_MyCasual.WBP_TitleMain_MyCasual_C");
	constexpr const TCHAR* ButtonClassPath =
		TEXT("/Game/FestaFiesta/UI/Title/GlassOrbit/WBP_TitleMenuButton_MyCasual.WBP_TitleMenuButton_MyCasual_C");

	struct FPlayerlessInitialization
	{
		explicit FPlayerlessInitialization(UWidgetBlueprintGeneratedClass& InClass)
			: Class(InClass), bPreviouslyAllowed(Class.bCanCallInitializedWithoutPlayerContext)
		{
			Class.bCanCallInitializedWithoutPlayerContext = true;
		}
		~FPlayerlessInitialization() { Class.bCanCallInitializedWithoutPlayerContext = bPreviouslyAllowed; }
		UWidgetBlueprintGeneratedClass& Class;
		bool bPreviouslyAllowed;
	};

	bool Contains(const FSlateRect& Outer, const FSlateRect& Inner)
	{
		return Inner.Left >= Outer.Left - 1.f && Inner.Right <= Outer.Right + 1.f
			&& Inner.Top >= Outer.Top - 1.f && Inner.Bottom <= Outer.Bottom + 1.f;
	}

	bool Overlaps(const FSlateRect& A, const FSlateRect& B)
	{
		return A.Left < B.Right - 1.f && A.Right > B.Left + 1.f
			&& A.Top < B.Bottom - 1.f && A.Bottom > B.Top + 1.f;
	}

	bool CaptureAndCheck(FAutomationTestBase& Test, UFF_TitleScreen& Title,
		const TSharedRef<SWidget>& Surface, const FIntPoint Size, const TCHAR* Variant)
	{
		const FString Context = FString::Printf(TEXT("%s %dx%d / "), Variant, Size.X, Size.Y);
		FWidgetRenderer Renderer(false, true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(
			FWidgetRenderer::CreateTargetFor(FVector2D(Size.X, Size.Y), TF_Bilinear, true));
		if (!Test.TestNotNull(Context + TEXT("render target"), Target.Get())) { return false; }
		for (int32 Frame = 0; Frame < 3; ++Frame)
		{
			Title.ForceLayoutPrepass();
			Renderer.DrawWidget(Target.Get(), Surface, FVector2D(Size.X, Size.Y), 1.f / 60.f, false);
		}
		FlushRenderingCommands();
		const FSlateRect Viewport(0.f, 0.f, static_cast<float>(Size.X), static_cast<float>(Size.Y));
		const bool bPrompt = Title.IsGoogleSignInPromptVisible();
		UUserWidget* AccountSurface = bPrompt
			? Cast<UUserWidget>(Title.GetWidgetFromName(TEXT("GoogleAccountPromptPanel"))) : &Title;
		if (!Test.TestNotNull(Context + TEXT("account presentation surface"), AccountSurface)) { return false; }
		auto RectFor = [&](const TCHAR* Name)
		{
			const UWidget* Widget = AccountSurface->GetWidgetFromName(Name);
			if (!Widget) { Widget = Title.GetWidgetFromName(Name); }
			if (!Test.TestNotNull(Context + Name, Widget)) { return FSlateRect(); }
			const FSlateRect Rect = Widget->GetCachedGeometry().GetLayoutBoundingRect();
			Test.TestTrue(Context + TEXT("visible positive-sized widget / ") + Name,
				Widget->IsVisible() && Rect.GetSize().X > 0.f && Rect.GetSize().Y > 0.f);
			Test.TestTrue(Context + TEXT("within viewport / ") + Name, Contains(Viewport, Rect));
			return Rect;
		};
		const FSlateRect Panel = RectFor(bPrompt ? TEXT("Border_GoogleSignInDialog") : TEXT("Border_GoogleAccount"));
		const FSlateRect Action = RectFor(TEXT("Button_GoogleAccount"));
		Test.TestTrue(Context + TEXT("account action within panel"), Contains(Panel, Action));
		Test.TestTrue(Context + TEXT("account action has a usable hit area"),
			Action.GetSize().X >= 44.f && Action.GetSize().Y >= 40.f);
		for (const TCHAR* Name : { TEXT("Btn_Play"), TEXT("Btn_Options"), TEXT("Btn_Quit") })
		{
			const FSlateRect MenuButton = RectFor(Name);
			if (bPrompt)
			{
				Test.TestFalse(Context + TEXT("background menu is disabled while sign-in is foreground / ") + Name,
					Title.GetWidgetFromName(Name)->GetIsEnabled());
			}
			else { Test.TestFalse(Context + TEXT("account does not overlap main menu / ") + Name, Overlaps(Panel, MenuButton)); }
		}
		if (bPrompt)
		{
			Test.TestTrue(Context + TEXT("initial account dialog is horizontally centered"),
				FMath::IsNearlyEqual((Panel.Left + Panel.Right) * 0.5f, Size.X * 0.5f, 2.f));
			Test.TestTrue(Context + TEXT("initial account dialog is vertically centered"),
				FMath::IsNearlyEqual((Panel.Top + Panel.Bottom) * 0.5f, Size.Y * 0.5f, 2.f));
			Test.TestTrue(Context + TEXT("offline continuation stays inside dialog"),
				Contains(Panel, RectFor(TEXT("Button_GoogleContinueOffline"))));
			for (const TCHAR* Field : { TEXT("Text_GoogleAccountName"), TEXT("Text_GoogleAccountEmail"), TEXT("Text_GoogleAccountState") })
			{
				Test.TestTrue(Context + TEXT("account field stays inside dialog / ") + Field, Contains(Panel, RectFor(Field)));
			}
		}
		const UImage* SignInArtwork = Cast<UImage>(AccountSurface->GetWidgetFromName(TEXT("Image_GoogleSignIn")));
		if (SignInArtwork && SignInArtwork->IsVisible())
		{
			const FSlateRect Artwork = RectFor(TEXT("Image_GoogleSignIn"));
			Test.TestTrue(Context + TEXT("official artwork lies within native action"), Contains(Action, Artwork));
			Test.TestTrue(Context + TEXT("official artwork keeps its 360:80 aspect ratio"),
				FMath::IsNearlyEqual(Artwork.GetSize().X / Artwork.GetSize().Y, 4.5f, 0.01f));
		}
		for (const TCHAR* Name : { TEXT("Text_GoogleAccountHeading"), TEXT("Text_GoogleAccountStatus"),
			TEXT("Text_GoogleAccountAction"), TEXT("Text_GoogleAccountOptional") })
		{
			const UTextBlock* Text = Cast<UTextBlock>(AccountSurface->GetWidgetFromName(Name));
			if (!Test.TestNotNull(Context + TEXT("text / ") + Name, Text)) { return false; }
			// The approved bitmap supplies visible sign-in copy; the Korean label remains available to accessibility.
			if (FName(Name) == TEXT("Text_GoogleAccountAction") && SignInArtwork && SignInArtwork->IsVisible())
			{
				Test.TestFalse(Context + TEXT("sign-in action keeps localized assistive label"), Text->GetText().IsEmpty());
				Test.TestFalse(Context + TEXT("native sign-in label does not cover approved artwork"), Text->IsVisible());
				continue;
			}
			const FSlateRect Rect = RectFor(Name);
			Test.TestFalse(Context + TEXT("nonempty text / ") + Name, Text->GetText().IsEmpty());
			Test.TestTrue(Context + TEXT("text within account panel / ") + Name, Contains(Panel, Rect));
			Test.TestTrue(Context + TEXT("text height is not clipped / ") + Name,
				Rect.GetSize().Y + 1.f >= Text->GetDesiredSize().Y);
		}
		if (const UWidget* Profile = Title.GetWidgetFromName(TEXT("Text_GoogleAccountProfile")); Profile && Profile->IsVisible())
		{
			Test.TestTrue(Context + TEXT("connected profile within panel"),
				Contains(Panel, RectFor(TEXT("Text_GoogleAccountProfile"))));
		}

		TArray<FColor> Pixels;
		FReadSurfaceDataFlags Flags;
		Flags.SetLinearToGamma(false);
		if (!Test.TestTrue(Context + TEXT("read GPU pixels"),
			Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)
			&& Pixels.Num() == Size.X * Size.Y)) { return false; }
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GoogleAuth/TitleLayout"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString Path = FPaths::Combine(Directory,
			FString::Printf(TEXT("TitleGoogleAccount_%s_%dx%d.png"), Variant, Size.X, Size.Y));
		const bool bSaved = FFileHelper::SaveArrayToFile(Png, *Path);
		Test.TestTrue(Context + TEXT("save rendered title capture"), bSaved);
		Test.AddInfo(TEXT("Title account layout capture: ") + FPaths::ConvertRelativePathToFull(Path));
		return bSaved;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_TitleGoogleAuth_RecreatedWidget,
	"FestaFiesta.Authentication.Google.TitleRecreatedWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_TitleGoogleAuth_RecreatedWidget::RunTest(const FString& Parameters)
{
	using namespace FFTitleGoogleAuthTest;
	(void)Parameters;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	auto* TitleClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_TitleScreen>(nullptr, TitleClassPath));
	auto* ButtonClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_CommonButtonBase>(nullptr, ButtonClassPath));
	if (!TestNotNull(TEXT("editor world"), World) || !TestNotNull(TEXT("configured title WBP"), TitleClass)
		|| !TestNotNull(TEXT("saved title button WBP"), ButtonClass)) { return false; }
	if (!TestNull(TEXT("UI isolation does not initialize a game or auth session"), World->GetGameInstance())) { return false; }
	FPlayerlessInitialization TitleInitialization(*TitleClass);
	FPlayerlessInitialization ButtonInitialization(*ButtonClass);
	const bool bWasDirty = TitleClass->GetOutermost()->IsDirty();
	// The saved WBP requests Play focus once during initialization. This fixture intentionally
	// has no local player so it cannot create an auth/game session; account/layout assertions still run.
	AddExpectedError(TEXT("The PlayerController is not a valid local player so it can't focus on  Btn_Play"),
		EAutomationExpectedErrorFlags::Contains, 1);
	TStrongObjectPtr<UFF_TitleScreen> Title(CreateWidget<UFF_TitleScreen>(World, TitleClass));
	if (!TestNotNull(TEXT("instantiate existing title WBP"), Title.Get())) { return false; }
	TSharedPtr<SWidget> PresentedTitle = Title->TakeWidget();
	UBorder* FirstPanel = Cast<UBorder>(Title->GetWidgetFromName(TEXT("Border_GoogleAccount")));
	UButton* AccountAction = Cast<UButton>(Title->GetWidgetFromName(TEXT("Button_GoogleAccount")));
	UFF_CommonButtonBase* Play = Cast<UFF_CommonButtonBase>(Title->GetWidgetFromName(TEXT("Btn_Play")));
	if (!TestNotNull(TEXT("native account panel"), FirstPanel) || !TestNotNull(TEXT("account action"), AccountAction)
		|| !TestNotNull(TEXT("existing Play action"), Play)) { return false; }
	TestTrue(TEXT("Google account action supports keyboard navigation"), AccountAction->GetIsFocusable());
	const UImage* Artwork = Cast<UImage>(Title->GetWidgetFromName(TEXT("Image_GoogleSignIn")));
	const UTexture2D* ArtworkTexture = Artwork ? Cast<UTexture2D>(Artwork->GetBrush().GetResourceObject()) : nullptr;
	if (!TestNotNull(TEXT("official Google sign-in artwork is loaded at runtime"), ArtworkTexture)) { return false; }
	TestEqual(TEXT("official artwork width is preserved"), ArtworkTexture->GetSizeX(), 360);
	TestEqual(TEXT("official artwork height is preserved"), ArtworkTexture->GetSizeY(), 80);
	TestTrue(TEXT("signed-out action shows Google's complete preapproved artwork"), Artwork->IsVisible());
	TestTrue(TEXT("account action binds while presented"), AccountAction->OnClicked.IsBound());
	TestFalse(TEXT("account action is unavailable without a game instance"), AccountAction->GetIsEnabled());
	TestTrue(TEXT("title first presents the centered account prompt"), Title->IsGoogleSignInPromptVisible());
	TestFalse(TEXT("background Play cannot bypass foreground account prompt"), Play->GetIsEnabled());
	UButton* ContinueOffline = Cast<UButton>(Title->GetWidgetFromName(TEXT("Button_GoogleContinueOffline")));
	if (!TestNotNull(TEXT("explicit optional-login continuation"), ContinueOffline)) { return false; }
	TestTrue(TEXT("unavailable login focuses its usable offline continuation"), Title->GetDesiredFocusTarget() == ContinueOffline);
	ContinueOffline->OnClicked.Broadcast();
	TestFalse(TEXT("offline continuation dismisses the account prompt"), Title->IsGoogleSignInPromptVisible());
	TestTrue(TEXT("offline continuation restores Play"), Play->GetIsEnabled());
	TestTrue(TEXT("dismissal restores initial Play focus"), Title->GetDesiredFocusTarget() == Play);
	UOverlay* Root = Cast<UOverlay>(Title->GetWidgetFromName(TEXT("Overlay_Root")));
	if (!TestNotNull(TEXT("existing title overlay"), Root)) { return false; }
	const int32 PresentedChildCount = Root->GetChildrenCount();
	bool bDestructed = false;
	const FDelegateHandle DestructHandle = Title->OnNativeDestruct.AddLambda([&](UUserWidget*) { bDestructed = true; });
	ON_SCOPE_EXIT { Title->OnNativeDestruct.Remove(DestructHandle); };
	PresentedTitle.Reset();
	// CommonUI narrows this override to protected; the UUserWidget lifecycle API is public.
	static_cast<UUserWidget*>(Title.Get())->ReleaseSlateResources(true);
	TestTrue(TEXT("releasing Slate executes NativeDestruct"), bDestructed);
	TestFalse(TEXT("account click delegate unbinds on destruction"), AccountAction->OnClicked.IsBound());
	PresentedTitle = Title->TakeWidget();
	TestTrue(TEXT("pooled reconstruction reuses the same account panel"),
		Title->GetWidgetFromName(TEXT("Border_GoogleAccount")) == FirstPanel);
	TestEqual(TEXT("pooled reconstruction adds no duplicate overlay children"), Root->GetChildrenCount(), PresentedChildCount);
	TestTrue(TEXT("account click delegate rebinds after reconstruction"), AccountAction->OnClicked.IsBound());
	TestTrue(TEXT("Play remains the desired focus after reconstruction"), Title->GetDesiredFocusTarget() == Play);
	TestFalse(TEXT("pooled reconstruction does not reopen the first-entry prompt"), Title->IsGoogleSignInPromptVisible());
	TArray<UWidget*> Widgets;
	Title->WidgetTree->GetAllWidgets(Widgets);
	TestEqual(TEXT("one account panel exists in the reconstructed hierarchy"),
		Widgets.FilterByPredicate([](const UWidget* Widget) { return Widget->GetFName() == TEXT("Border_GoogleAccount"); }).Num(), 1);
	TestEqual(TEXT("runtime panel creation leaves saved WBP untouched"), TitleClass->GetOutermost()->IsDirty(), bWasDirty);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_TitleGoogleAuth_RenderedLayout,
	"FestaFiesta.Authentication.Google.TitleRenderedLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)

bool FFF_TitleGoogleAuth_RenderedLayout::RunTest(const FString& Parameters)
{
	using namespace FFTitleGoogleAuthTest;
	(void)Parameters;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	auto* TitleClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_TitleScreen>(nullptr, TitleClassPath));
	auto* ButtonClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_CommonButtonBase>(nullptr, ButtonClassPath));
	if (!TestNotNull(TEXT("editor world"), World) || !TestNotNull(TEXT("configured title WBP"), TitleClass)
		|| !TestNotNull(TEXT("saved title button WBP"), ButtonClass)) { return false; }
	if (!TestNull(TEXT("layout capture has no game instance or real account"), World->GetGameInstance())) { return false; }
	FPlayerlessInitialization TitleInitialization(*TitleClass);
	FPlayerlessInitialization ButtonInitialization(*ButtonClass);
	const bool bWasDirty = TitleClass->GetOutermost()->IsDirty();
	// The playerless presentation fixture expects only this saved-WBP focus request, exactly once.
	AddExpectedError(TEXT("The PlayerController is not a valid local player so it can't focus on  Btn_Play"),
		EAutomationExpectedErrorFlags::Contains, 1);
	TStrongObjectPtr<UFF_TitleScreen> Title(CreateWidget<UFF_TitleScreen>(World, TitleClass));
	if (!TestNotNull(TEXT("instantiate saved title"), Title.Get())) { return false; }
	const TSharedRef<SOverlay> Surface = SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)[Title->TakeWidget()];
	Title->SetVisibility(ESlateVisibility::Visible);
	FAssetCompilingManager::Get().FinishAllCompilation();
	FlushRenderingCommands();
	for (const FIntPoint Size : { FIntPoint(1280, 720), FIntPoint(1920, 1080) })
	{
		if (!CaptureAndCheck(*this, *Title, Surface, Size, TEXT("InitialPrompt"))) { return false; }
	}
	Title->DismissGoogleSignInPrompt();
	for (const FIntPoint Size : { FIntPoint(1280, 720), FIntPoint(1920, 1080) })
	{
		if (!CaptureAndCheck(*this, *Title, Surface, Size, TEXT("DismissedSummary"))) { return false; }
	}
	// Presentation samples exercise long display names without changing auth state or making network requests.
	UTextBlock* Profile = Cast<UTextBlock>(Title->GetWidgetFromName(TEXT("Text_GoogleAccountProfile")));
	UTextBlock* Status = Cast<UTextBlock>(Title->GetWidgetFromName(TEXT("Text_GoogleAccountStatus")));
	UTextBlock* Action = Cast<UTextBlock>(Title->GetWidgetFromName(TEXT("Text_GoogleAccountAction")));
	if (!TestNotNull(TEXT("sample profile field"), Profile) || !TestNotNull(TEXT("sample status field"), Status)
		|| !TestNotNull(TEXT("sample action field"), Action)) { return false; }
	Profile->SetText(FText::FromString(TEXT("축제를 사랑하는 플레이어\nfestival.player@example.com")));
	Profile->SetVisibility(ESlateVisibility::HitTestInvisible);
	Status->SetText(FText::FromString(TEXT("Google 계정으로 로그인했습니다.")));
	Action->SetText(FText::FromString(TEXT("로그아웃")));
	Action->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UImage* Artwork = Cast<UImage>(Title->GetWidgetFromName(TEXT("Image_GoogleSignIn"))))
	{
		Artwork->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (UButton* AccountAction = Cast<UButton>(Title->GetWidgetFromName(TEXT("Button_GoogleAccount"))))
	{
		// Refresh only native presentation after switching the display sample; no sign-in action is invoked.
		AccountAction->OnUnhovered.Broadcast();
	}
	for (const FIntPoint Size : { FIntPoint(1280, 720), FIntPoint(1920, 1080) })
	{
		if (!CaptureAndCheck(*this, *Title, Surface, Size, TEXT("ProfileSample"))) { return false; }
	}
	TestEqual(TEXT("GPU layout verification does not alter title asset"), TitleClass->GetOutermost()->IsDirty(), bWasDirty);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
