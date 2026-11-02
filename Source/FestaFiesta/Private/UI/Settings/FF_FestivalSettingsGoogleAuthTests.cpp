// Exercise the real fullscreen settings account page without a game session or network request.
#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AssetCompilingManager.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "UI/Account/FF_GoogleAccountPanel.h"
#include "UI/Settings/FF_FestivalSettingsScreen.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SOverlay.h"

namespace FFFestivalGoogleAccountTest
{
	struct FScreenFixture
	{
		UWidgetBlueprintGeneratedClass* Class = nullptr;
		bool bPreviouslyAllowed = false;
		bool bInitializationFlagCaptured = false;
		bool bWasDirty = false;
		TStrongObjectPtr<UFF_FestivalSettingsScreen> Screen;

		~FScreenFixture()
		{
			if (Class && bInitializationFlagCaptured) { Class->bCanCallInitializedWithoutPlayerContext = bPreviouslyAllowed; }
		}

		bool Create(FAutomationTestBase& Test)
		{
			UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
			Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_FestivalSettingsScreen>(nullptr,
				TEXT("/Game/FestaFiesta/UI/Settings/WBP_FF_FullscreenOptions.WBP_FF_FullscreenOptions_C")));
			if (!Test.TestNotNull(TEXT("Editor world for account UI isolation"), World)
				|| !Test.TestNotNull(TEXT("Production fullscreen options WBP"), Class)) return false;
			if (!Test.TestNull(TEXT("Account presentation does not initialize a game session"), World->GetGameInstance())) return false;
			bPreviouslyAllowed = Class->bCanCallInitializedWithoutPlayerContext;
			bInitializationFlagCaptured = true;
			bWasDirty = Class->GetOutermost()->IsDirty();
			Class->bCanCallInitializedWithoutPlayerContext = true;
			Screen.Reset(CreateWidget<UFF_FestivalSettingsScreen>(World, Class));
			return Test.TestNotNull(TEXT("Actual fullscreen options instance"), Screen.Get());
		}
	};

	UFF_GoogleAccountPanel* FindAccountPanel(UVerticalBox& Rows, int32& Count)
	{
		Count = 0;
		UFF_GoogleAccountPanel* Result = nullptr;
		for (UWidget* Child : Rows.GetAllChildren())
		{
			if (auto* Panel = Cast<UFF_GoogleAccountPanel>(Child)) { Result = Panel; ++Count; }
		}
		return Result;
	}

	void CollectText(UWidget* Widget, FString& Result)
	{
		if (const auto* Text = Cast<UTextBlock>(Widget)) { Result += Text->GetText().ToString() + TEXT("\n"); }
		if (auto* UserWidget = Cast<UUserWidget>(Widget); UserWidget && UserWidget->WidgetTree)
		{
			CollectText(UserWidget->WidgetTree->RootWidget, Result);
		}
		else if (auto* Panel = Cast<UPanelWidget>(Widget))
		{
			for (UWidget* Child : Panel->GetAllChildren()) { CollectText(Child, Result); }
		}
	}

	bool Contains(const FSlateRect& Outer, const FSlateRect& Inner)
	{
		return Inner.Left >= Outer.Left - 1.f && Inner.Right <= Outer.Right + 1.f
			&& Inner.Top >= Outer.Top - 1.f && Inner.Bottom <= Outer.Bottom + 1.f;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FestivalSettingsGoogleAccountContractTest,
	"FestaFiesta.Authentication.Google.FestivalSettingsAccountContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FestivalSettingsGoogleAccountContractTest::RunTest(const FString& Parameters)
{
	using namespace FFFestivalGoogleAccountTest;
	(void)Parameters;
	FScreenFixture Fixture;
	if (!Fixture.Create(*this)) return false;
	const TSharedRef<SWidget> Surface = Fixture.Screen->TakeWidget();
	// Category navigation requests focus in production. This fixture intentionally has no LocalPlayer.
	AddExpectedError(TEXT("The PlayerController is not a valid local player so it can't focus on  Button_Account"),
		EAutomationExpectedErrorFlags::Contains, 2, false);
	AddExpectedError(TEXT("The PlayerController is not a valid local player so it can't focus on  Button_Language"),
		EAutomationExpectedErrorFlags::Contains, 1, false);
	Fixture.Screen->ShowCategory(EFF_FestivalSettingsCategory::Account);
	UVerticalBox* Rows = Cast<UVerticalBox>(Fixture.Screen->GetWidgetFromName(TEXT("VerticalBox_Rows")));
	UButton* Save = Cast<UButton>(Fixture.Screen->GetWidgetFromName(TEXT("Button_Save")));
	if (!TestNotNull(TEXT("Production settings row container"), Rows) || !TestNotNull(TEXT("Settings Save action"), Save)) return false;
	int32 Count = 0;
	UFF_GoogleAccountPanel* Account = FindAccountPanel(*Rows, Count);
	if (!TestNotNull(TEXT("Account category hosts shared Google account UI"), Account)) return false;
	TestEqual(TEXT("One shared account panel is mounted"), Count, 1);
	TestTrue(TEXT("Google connection appears before local environment information"), Rows->GetChildAt(0) == Account);
	const bool bSaveWasEnabled = Save->GetIsEnabled();
	Account->TakeWidget();
	Account->RefreshAccount();
	FString Copy;
	CollectText(Rows, Copy);
	TestTrue(TEXT("Account page identifies Google account information"), Copy.Contains(TEXT("Google")));
	TestFalse(TEXT("Stale claim that online account connection is absent is removed"),
		Copy.Contains(TEXT("온라인 계정 연결 기능이 없습니다")));
	TestTrue(TEXT("Local environment information remains available"), Copy.Contains(TEXT("설정 저장 위치")) && Copy.Contains(TEXT("게임 버전")));
	TestEqual(TEXT("Account refresh does not change settings draft Save availability"), Save->GetIsEnabled(), bSaveWasEnabled);
	Fixture.Screen->ShowCategory(EFF_FestivalSettingsCategory::Language);
	FindAccountPanel(*Rows, Count);
	TestEqual(TEXT("Other categories do not retain the account surface"), Count, 0);
	Fixture.Screen->ShowCategory(EFF_FestivalSettingsCategory::Account);
	TestNotNull(TEXT("Account surface is available after category revisit"), FindAccountPanel(*Rows, Count));
	TestEqual(TEXT("Category revisit creates no duplicate account panels"), Count, 1);
	TestEqual(TEXT("Viewing account information does not modify settings draft"), Save->GetIsEnabled(), bSaveWasEnabled);
	TestEqual(TEXT("Runtime account rows do not rewrite the saved WBP"), Fixture.Class->GetOutermost()->IsDirty(), Fixture.bWasDirty);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FestivalSettingsGoogleAccountLayoutTest,
	"FestaFiesta.Authentication.Google.FestivalSettingsAccountLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)

bool FFF_FestivalSettingsGoogleAccountLayoutTest::RunTest(const FString& Parameters)
{
	using namespace FFFestivalGoogleAccountTest;
	(void)Parameters;
	FScreenFixture Fixture;
	if (!Fixture.Create(*this)) return false;
	const TSharedRef<SOverlay> Surface = SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)[Fixture.Screen->TakeWidget()];
	AddExpectedError(TEXT("The PlayerController is not a valid local player so it can't focus on  Button_Account"),
		EAutomationExpectedErrorFlags::Contains, 1, false);
	Fixture.Screen->ShowCategory(EFF_FestivalSettingsCategory::Account);
	Fixture.Screen->SetVisibility(ESlateVisibility::Visible);
	UVerticalBox* Rows = Cast<UVerticalBox>(Fixture.Screen->GetWidgetFromName(TEXT("VerticalBox_Rows")));
	UScrollBox* Scroll = Cast<UScrollBox>(Fixture.Screen->GetWidgetFromName(TEXT("ScrollBox_Rows")));
	if (!TestNotNull(TEXT("Account row container"), Rows) || !TestNotNull(TEXT("Account scroll viewport"), Scroll)) return false;
	int32 Count = 0;
	UFF_GoogleAccountPanel* Account = FindAccountPanel(*Rows, Count);
	if (!TestNotNull(TEXT("Mounted Google account panel"), Account)) return false;
	FAssetCompilingManager::Get().FinishAllCompilation();
	FlushRenderingCommands();
	for (const FIntPoint Size : { FIntPoint(1280, 720), FIntPoint(1920, 1080) })
	{
		FWidgetRenderer Renderer(false, true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(
			FWidgetRenderer::CreateTargetFor(FVector2D(Size.X, Size.Y), TF_Bilinear, true));
		if (!TestNotNull(TEXT("Account layout render target"), Target.Get())) return false;
		Scroll->ScrollToStart();
		for (int32 Frame = 0; Frame < 3; ++Frame)
		{
			Fixture.Screen->ForceLayoutPrepass();
			Renderer.DrawWidget(Target.Get(), Surface, FVector2D(Size.X, Size.Y), 1.f / 60.f, false);
		}
		FlushRenderingCommands();
		const FSlateRect PanelRect = Account->GetCachedGeometry().GetLayoutBoundingRect();
		const FSlateRect ScrollRect = Scroll->GetCachedGeometry().GetLayoutBoundingRect();
		TestTrue(TEXT("Google account panel fits the initial visible account page"),
			PanelRect.GetSize().Y > 0.f && Contains(ScrollRect, PanelRect));
		TestTrue(TEXT("Google account information uses the available content width"),
			PanelRect.GetSize().X > ScrollRect.GetSize().X * 0.75f);
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags Flags;
		Flags.SetLinearToGamma(false);
		if (!TestTrue(TEXT("Read actual account page GPU pixels"),
			Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags) && Pixels.Num() == Size.X * Size.Y)) return false;
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GoogleAuth/SettingsLayout"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString Path = FPaths::Combine(Directory,
			FString::Printf(TEXT("FestivalSettings_GoogleAccount_%dx%d.png"), Size.X, Size.Y));
		TestTrue(TEXT("Save account page layout capture"), FFileHelper::SaveArrayToFile(Png, *Path));
		AddInfo(TEXT("Festival Google account layout capture: ") + FPaths::ConvertRelativePathToFull(Path));
	}
	TestEqual(TEXT("Account layout capture leaves the saved WBP unchanged"), Fixture.Class->GetOutermost()->IsDirty(), Fixture.bWasDirty);
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
