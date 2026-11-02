#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "AssetCompilingManager.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
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
#include "UI/FF_CommonButtonBase.h"
#include "UI/FF_UIDisplayTextLibrary.h"
#include "UI/Frontend/FF_OptionsScreen.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SOverlay.h"

namespace FFOptionsGoogleAccountTest
{
	constexpr const TCHAR* OptionsPath =
		TEXT("/Game/FestaFiesta/UI/Frontend/WBP_FF_Options.WBP_FF_Options_C");

	struct FPlayerlessInitialization
	{
		explicit FPlayerlessInitialization(UWidgetBlueprintGeneratedClass& InClass)
			: Class(InClass), bOriginal(Class.bCanCallInitializedWithoutPlayerContext)
		{
			Class.bCanCallInitializedWithoutPlayerContext = true;
		}
		~FPlayerlessInitialization() { Class.bCanCallInitializedWithoutPlayerContext = bOriginal; }
		UWidgetBlueprintGeneratedClass& Class;
		bool bOriginal;
	};

	bool Contains(const FSlateRect& Outer, const FSlateRect& Inner)
	{
		return Inner.Left >= Outer.Left - 1.f && Inner.Top >= Outer.Top - 1.f
			&& Inner.Right <= Outer.Right + 1.f && Inner.Bottom <= Outer.Bottom + 1.f;
	}

	bool Overlaps(const FSlateRect& A, const FSlateRect& B)
	{
		return A.Left < B.Right - 1.f && A.Right > B.Left + 1.f
			&& A.Top < B.Bottom - 1.f && A.Bottom > B.Top + 1.f;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OptionsGoogleAccountCategoryTest,
	"FestaFiesta.Authentication.Google.FrontendOptionsAccount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_OptionsGoogleAccountCategoryTest::RunTest(const FString& Parameters)
{
	using namespace FFOptionsGoogleAccountTest;
	(void)Parameters;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	auto* OptionsClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_OptionsScreen>(nullptr, OptionsPath));
	if (!TestNotNull(TEXT("editor world"), World) || !TestNotNull(TEXT("saved frontend options WBP"), OptionsClass)) return false;
	if (!TestNull(TEXT("UI fixture starts no game or authentication session"), World->GetGameInstance())) return false;
	FPlayerlessInitialization Initialization(*OptionsClass);
	const bool bWasDirty = OptionsClass->GetOutermost()->IsDirty();
	TStrongObjectPtr<UFF_OptionsScreen> Screen(CreateWidget<UFF_OptionsScreen>(World, OptionsClass));
	if (!TestNotNull(TEXT("existing options screen instantiated"), Screen.Get())) return false;
	TSharedPtr<SWidget> Presented = Screen->TakeWidget();
	UFF_CommonButtonBase* Account = Cast<UFF_CommonButtonBase>(Screen->GetWidgetFromName(TEXT("Button_Account")));
	UVerticalBox* Categories = Cast<UVerticalBox>(Screen->GetWidgetFromName(TEXT("VerticalBox_Categories")));
	UVerticalBox* Rows = Cast<UVerticalBox>(Screen->GetWidgetFromName(TEXT("VerticalBox_Rows")));
	if (!TestNotNull(TEXT("account category exists in older authored WBP"), Account)
		|| !TestNotNull(TEXT("authored category column"), Categories)
		|| !TestNotNull(TEXT("authored option content column"), Rows)) return false;
	const UTextBlock* Label = Cast<UTextBlock>(Account->GetWidgetFromName(TEXT("Text_Label")));
	TestTrue(TEXT("account category has a localized label"), Label && Label->GetText().EqualTo(
		UFF_UIDisplayTextLibrary::GetOptionCategoryText(EFF_OptionCategory::Account)));
	TestEqual(TEXT("old credits enum value remains stable for saved WBP data"),
		static_cast<uint8>(EFF_OptionCategory::Credits), static_cast<uint8>(5));
	TestEqual(TEXT("account category extends rather than renumbers old categories"),
		static_cast<uint8>(EFF_OptionCategory::Account), static_cast<uint8>(6));
	const int32 CategoryCount = Categories->GetChildrenCount();
	Account->OnFFButtonClicked.Broadcast(Account);
	if (!TestEqual(TEXT("account category mounts one shared panel"), Rows->GetChildrenCount(), 1)) return false;
	auto* Panel = Cast<UFF_GoogleAccountPanel>(Rows->GetChildAt(0));
	if (!TestNotNull(TEXT("account page uses the real shared Google account surface"), Panel)) return false;
	TestTrue(TEXT("account becomes desired keyboard focus category"), Screen->GetDesiredFocusTarget() == Account);
	const UTextBlock* Heading = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_CategoryTitle")));
	TestTrue(TEXT("account content heading is localized"), Heading && Heading->GetText().ToString() == TEXT("계정정보"));
	if (const UTextBlock* Hint = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_ApplyHint"))))
	{
		TestTrue(TEXT("opening the account page does not dirty option drafts"), Hint->GetText().IsEmpty());
	}
	Account->OnFFButtonClicked.Broadcast(Account);
	TestTrue(TEXT("reselecting account reuses its shared panel"), Rows->GetChildAt(0) == Panel);
	Presented.Reset();
	static_cast<UUserWidget*>(Screen.Get())->ReleaseSlateResources(true);
	Presented = Screen->TakeWidget();
	TestEqual(TEXT("pooled options reconstruction adds no duplicate category"), Categories->GetChildrenCount(), CategoryCount);
	TestTrue(TEXT("account category rebinds after reconstruction"), Account->OnFFButtonClicked.IsBound());
	Account->OnFFButtonClicked.Broadcast(Account);
	TestTrue(TEXT("account page remains usable after reconstruction"), Rows->GetChildAt(0) == Panel);
	TestEqual(TEXT("runtime account additions leave the saved options package untouched"),
		OptionsClass->GetOutermost()->IsDirty(), bWasDirty);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OptionsGoogleAccountRenderedTest,
	"FestaFiesta.Authentication.Google.FrontendOptionsAccountRendered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)

bool FFF_OptionsGoogleAccountRenderedTest::RunTest(const FString& Parameters)
{
	using namespace FFOptionsGoogleAccountTest;
	(void)Parameters;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	auto* OptionsClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_OptionsScreen>(nullptr, OptionsPath));
	if (!TestNotNull(TEXT("editor world"), World) || !TestNotNull(TEXT("saved options WBP"), OptionsClass)) return false;
	if (!TestNull(TEXT("render fixture starts no account session"), World->GetGameInstance())) return false;
	FPlayerlessInitialization Initialization(*OptionsClass);
	TStrongObjectPtr<UFF_OptionsScreen> Screen(CreateWidget<UFF_OptionsScreen>(World, OptionsClass));
	if (!TestNotNull(TEXT("render options screen"), Screen.Get())) return false;
	const TSharedRef<SOverlay> Surface = SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)[Screen->TakeWidget()];
	Screen->SetVisibility(ESlateVisibility::Visible);
	auto* Account = Cast<UFF_CommonButtonBase>(Screen->GetWidgetFromName(TEXT("Button_Account")));
	auto* Rows = Cast<UVerticalBox>(Screen->GetWidgetFromName(TEXT("VerticalBox_Rows")));
	if (!TestNotNull(TEXT("account category"), Account) || !TestNotNull(TEXT("option content"), Rows)) return false;
	Account->OnFFButtonClicked.Broadcast(Account);
	auto* Panel = Cast<UFF_GoogleAccountPanel>(Rows->GetChildAt(0));
	if (!TestNotNull(TEXT("shared account panel"), Panel)) return false;
	FAssetCompilingManager::Get().FinishAllCompilation();
	FlushRenderingCommands();
	for (const FIntPoint Size : { FIntPoint(1280, 720), FIntPoint(1920, 1080) })
	{
		const FString Context = FString::Printf(TEXT("%dx%d / "), Size.X, Size.Y);
		FWidgetRenderer Renderer(false, true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(
			FWidgetRenderer::CreateTargetFor(FVector2D(Size.X, Size.Y), TF_Bilinear, true));
		if (!TestNotNull(Context + TEXT("render target"), Target.Get())) return false;
		for (int32 Frame = 0; Frame < 3; ++Frame)
		{
			Screen->ForceLayoutPrepass();
			Renderer.DrawWidget(Target.Get(), Surface, FVector2D(Size.X, Size.Y), 1.f / 60.f, false);
		}
		FlushRenderingCommands();
		const FSlateRect Viewport(0.f, 0.f, static_cast<float>(Size.X), static_cast<float>(Size.Y));
		const FSlateRect AccountRect = Account->GetCachedGeometry().GetLayoutBoundingRect();
		const FSlateRect PanelRect = Panel->GetCachedGeometry().GetLayoutBoundingRect();
		TestTrue(Context + TEXT("account category has visible geometry"), AccountRect.GetSize().X > 0.f && AccountRect.GetSize().Y > 0.f);
		TestTrue(Context + TEXT("account category fits viewport"), Contains(Viewport, AccountRect));
		TestTrue(Context + TEXT("account content fits viewport"), Contains(Viewport, PanelRect));
		TestTrue(Context + TEXT("account content has visible geometry"), PanelRect.GetSize().X > 0.f && PanelRect.GetSize().Y > 0.f);
		TestTrue(Context + TEXT("account content is beside its navigation column"), PanelRect.Left > AccountRect.Right);
		const UWidget* Rail = Screen->GetWidgetFromName(TEXT("SizeBox_Rail"));
		const UWidget* Footer = Screen->GetWidgetFromName(TEXT("SizeBox_Footer"));
		if (!TestNotNull(Context + TEXT("bounded navigation rail"), Rail)
			|| !TestNotNull(Context + TEXT("footer area"), Footer)) return false;
		const FSlateRect RailRect = Rail->GetCachedGeometry().GetLayoutBoundingRect();
		const FSlateRect FooterRect = Footer->GetCachedGeometry().GetLayoutBoundingRect();
		float PreviousBottom = RailRect.Top;
		for (const TCHAR* CategoryName : { TEXT("Button_Language"), TEXT("Button_Accessibility"),
			TEXT("Button_Sound"), TEXT("Button_Graphics"), TEXT("Button_Controls"), TEXT("Button_Credits"), TEXT("Button_Account") })
		{
			const UWidget* Category = Screen->GetWidgetFromName(CategoryName);
			if (!TestNotNull(Context + TEXT("accessible category / ") + CategoryName, Category)) return false;
			const FSlateRect CategoryRect = Category->GetCachedGeometry().GetLayoutBoundingRect();
			TestTrue(Context + TEXT("all seven categories fit the rail / ") + CategoryName, Contains(RailRect, CategoryRect));
			TestTrue(Context + TEXT("all seven categories are visible / ") + CategoryName,
				Category->IsVisible() && CategoryRect.GetSize().X >= 44.f && CategoryRect.GetSize().Y >= 44.f);
			TestTrue(Context + TEXT("categories do not overlap each other / ") + CategoryName, CategoryRect.Top >= PreviousBottom - 1.f);
			TestFalse(Context + TEXT("category does not overlap footer / ") + CategoryName, Overlaps(CategoryRect, FooterRect));
			PreviousBottom = CategoryRect.Bottom;
			for (const TCHAR* ActionName : { TEXT("Button_Apply"), TEXT("Button_Back") })
			{
				const UWidget* Action = Screen->GetWidgetFromName(ActionName);
				if (!TestNotNull(Context + TEXT("footer action / ") + ActionName, Action)) return false;
				const FSlateRect ActionRect = Action->GetCachedGeometry().GetLayoutBoundingRect();
				TestFalse(Context + TEXT("category cannot obscure footer action / ") + CategoryName + TEXT(" / ") + ActionName,
					Overlaps(CategoryRect, ActionRect));
			}
		}
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags Flags;
		Flags.SetLinearToGamma(false);
		if (!TestTrue(Context + TEXT("read rendered option pixels"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags))) return false;
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GoogleAuth/FrontendOptionsAccount"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString Path = FPaths::Combine(Directory, FString::Printf(TEXT("FrontendOptionsAccount_%dx%d.png"), Size.X, Size.Y));
		TestTrue(Context + TEXT("save account options capture"), FFileHelper::SaveArrayToFile(Png, *Path));
		AddInfo(TEXT("Frontend account options capture: ") + FPaths::ConvertRelativePathToFull(Path));
	}
	return true;
}

#endif
