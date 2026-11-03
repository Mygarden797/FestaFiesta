// Verifies the actual category WBP's painted icon, including narrow buttons and DPI scaling.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "UI/PrepHUD/Build/FF_PrepBuildCategoryChoice.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SDPIScaler.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepBuildCategoryIconAlignmentTest,
	"FestaFiesta.UI.PrepBuild.CategoryIconCentered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildCategoryIconAlignmentTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	const FString Package = FFBuildUIAssetPaths::CategoryChoicePackage;
	const FString ClassPath = Package + TEXT(".") + FPackageName::GetLongPackageAssetName(Package) + TEXT("_C");
	auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepBuildCategoryChoice>(nullptr, *ClassPath));
	auto* IconTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Asset/Icon_HKH/T_HKH_Category_All.T_HKH_Category_All"));
	if (!TestNotNull(TEXT("Saved production category class"), Class) ||
		!TestNotNull(TEXT("Editor test world"), World) || !TestNotNull(TEXT("Production category icon"), IconTexture)) return false;
	TStrongObjectPtr<UFF_PrepBuildCategoryChoice> Choice;
	{
		const bool bPreviousInitialization = Class->bCanCallInitializedWithoutPlayerContext;
		Class->bCanCallInitializedWithoutPlayerContext = true;
		TGuardValue<bool> StartWithCaption(Class->GetDefaultObject<UFF_PrepBuildCategoryChoice>()->bIconOnly, false);
		Choice.Reset(CreateWidget<UFF_PrepBuildCategoryChoice>(World, Class));
		Class->bCanCallInitializedWithoutPlayerContext = bPreviousInitialization;
	}
	if (!TestNotNull(TEXT("Transient production category"), Choice.Get())) return false;
	auto* Button = Cast<UButton>(Choice->GetWidgetFromName(TEXT("Button_Root")));
	auto* Icon = Cast<UImage>(Choice->GetWidgetFromName(TEXT("Image_Icon")));
	auto* Label = Cast<UTextBlock>(Choice->GetWidgetFromName(TEXT("Text_Label")));
	UButtonSlot* ContentSlot = Button && Button->GetContent() ? Cast<UButtonSlot>(Button->GetContent()->Slot) : nullptr;
	UHorizontalBoxSlot* IconSlot = nullptr;
	for (UWidget* Host = Icon; Host && Host != Button; Host = Host->GetParent())
	{
		if (auto* RowSlot = Cast<UHorizontalBoxSlot>(Host->Slot)) { IconSlot = RowSlot; break; }
	}
	if (!TestNotNull(TEXT("Real category button"), Button) || !TestNotNull(TEXT("Real icon"), Icon) ||
		!TestNotNull(TEXT("Caption binding"), Label) || !TestNotNull(TEXT("Saved content slot"), ContentSlot) ||
		!TestNotNull(TEXT("Saved outer icon host row slot"), IconSlot)) return false;

	// Reproduce the previous caption layout, including a fill slot and unequal
	// insets. Centered alignment alone cannot fix its surviving caption gap.
	FButtonStyle AuthoredStyle = Button->GetStyle();
	AuthoredStyle.NormalPadding = FMargin(10, 3);
	AuthoredStyle.PressedPadding = FMargin(12, 4, 8, 2);
	Button->SetStyle(AuthoredStyle);
	const FMargin AuthoredContentPadding(7, 1, 2, 3);
	const FMargin AuthoredIconPadding(0, 0, 6, 0);
	ContentSlot->SetPadding(AuthoredContentPadding);
	ContentSlot->SetHorizontalAlignment(HAlign_Fill);
	ContentSlot->SetVerticalAlignment(VAlign_Fill);
	IconSlot->SetPadding(AuthoredIconPadding);
	IconSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	IconSlot->SetHorizontalAlignment(HAlign_Left);
	Choice->AuthoredLabel = FText::FromString(TEXT("전체"));
	Choice->AuthoredIcon.SetResourceObject(IconTexture);
	Choice->bIconOnly = true;
	Choice->IconOnlySize = 22.f;
	Choice->ConfigureTab(INDEX_NONE, Choice->AuthoredLabel, Choice->AuthoredIcon);
	const TSharedRef<SWidget> Slate = Choice->TakeWidget();
	FWidgetRenderer Renderer(false, true);
	for (const float Scale : {2.f / 3.f, 1.f, 1.25f})
	{
		for (const float Width : {36.f, 40.f, 48.f})
		{
			const FVector2D DrawSize(FMath::CeilToFloat(Width * Scale), FMath::CeilToFloat(40.f * Scale));
			TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, true));
			if (!TestNotNull(TEXT("Icon alignment render target"), Target.Get())) return false;
			const TSharedRef<SWidget> Surface = SNew(SDPIScaler).DPIScale(Scale)[Slate];
			for (const bool bSelected : {false, true, false})
			{
				Choice->SetSelected(bSelected);
				for (int32 Frame = 0; Frame < 2; ++Frame) Renderer.DrawWidget(Target.Get(), Surface, DrawSize, 0.f, false);
				FlushRenderingCommands();
				const FGeometry& ButtonGeometry = Button->GetCachedGeometry();
				const FGeometry& IconGeometry = Icon->GetCachedGeometry();
				const FVector2D ButtonCenter = ButtonGeometry.LocalToAbsolute(ButtonGeometry.GetLocalSize() * .5);
				const FVector2D IconCenter = IconGeometry.LocalToAbsolute(IconGeometry.GetLocalSize() * .5);
				const FString Context = FString::Printf(TEXT("%.0fpx button / DPI %.3f / selected=%d"), Width, Scale, bSelected);
				TestTrue(Context + TEXT(": icon is horizontally and vertically centered in its actual button"),
					IconCenter.Equals(ButtonCenter, .5));
				TestTrue(Context + TEXT(": entire icon stays within its button"),
					IconGeometry.GetLocalSize().X > 0 && IconGeometry.GetLocalSize().Y > 0 &&
					IconGeometry.GetLocalSize().X <= ButtonGeometry.GetLocalSize().X + .5 &&
					IconGeometry.GetLocalSize().Y <= ButtonGeometry.GetLocalSize().Y + .5);
				TestTrue(Context + TEXT(": image requests the compact authored size"), Icon->GetDesiredSize().Equals(FVector2D(22.f), .5));
				TestEqual(Context + TEXT(": no hidden caption occupies rail width"), Label->GetVisibility(), ESlateVisibility::Collapsed);
			}
		}
	}
	TestEqual(TEXT("Icon-only button retains its localized tooltip"), Button->GetToolTipText().ToString(), Choice->AuthoredLabel.ToString());
	Choice->bIconOnly = false;
	Choice->ConfigureChoice(2, FText::FromString(TEXT("먹거리")));
	TestTrue(TEXT("Returning to text mode restores the authored caption layout"),
		Label->IsVisible() && ContentSlot->GetPadding() == AuthoredContentPadding &&
		ContentSlot->GetHorizontalAlignment() == HAlign_Fill && ContentSlot->GetVerticalAlignment() == VAlign_Fill &&
		IconSlot->GetPadding() == AuthoredIconPadding && IconSlot->GetSize().SizeRule == ESlateSizeRule::Fill &&
		IconSlot->GetHorizontalAlignment() == HAlign_Left);
	TestTrue(TEXT("Returning to text mode restores normal and pressed style insets"),
		Button->GetStyle().NormalPadding == AuthoredStyle.NormalPadding &&
		Button->GetStyle().PressedPadding == AuthoredStyle.PressedPadding);
	const TSharedRef<SImage> SlateImage = StaticCastSharedRef<SImage>(Icon->GetCachedWidget().ToSharedRef());
	SlateImage->SlatePrepass(1.f);
	TestTrue(TEXT("Returning to text mode restores the authored brush's natural icon size"),
		SlateImage->GetDesiredSize().Equals(FVector2D(Icon->GetBrush().ImageSize), .5));
	return true;
}

#endif
