#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Building/Presentation/FF_BuildMultiSelectionWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Layout/SDPIScaler.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildMultiSelectionVisualTest,
	"FestaFiesta.Building.BuildMode.MultiSelectionEditingLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildMultiSelectionVisualTest::RunTest(const FString& Parameters)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Editor fixture world"), World)) return false;
	UWidgetBlueprintGeneratedClass* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_BuildMultiSelectionWidget>(nullptr,
		TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Controls/WBP_FF_BuildMultiSelection.WBP_FF_BuildMultiSelection_C")));
	if (!TestNotNull(TEXT("Existing saved selection widget"), Class)) return false;
	const UWidgetTree* Authored = Class->GetWidgetTreeArchetype();
	if (!TestNotNull(TEXT("Saved Designer widget tree"), Authored)) return false;
	for (const TCHAR* Required : {TEXT("Button_MoveGroup"), TEXT("Button_DuplicateGroup"), TEXT("Button_RemoveGroup"),
		TEXT("Button_RotateLeft"), TEXT("Button_RotateRight"), TEXT("Button_ApplyGroup"), TEXT("Button_CancelGroup"),
		TEXT("Button_SavePreset"), TEXT("Button_LoadPreset"), TEXT("Button_DeletePreset"), TEXT("Edit_PresetName"),
		TEXT("Combo_Presets"), TEXT("Text_GroupEditingStatus")})
		if (!TestNotNull(*FString::Printf(TEXT("%s exists in saved WBP before runtime construction"), Required), Authored->FindWidget(Required))) return false;
	const bool bPrevious = Class->bCanCallInitializedWithoutPlayerContext;
	Class->bCanCallInitializedWithoutPlayerContext = true;
	TStrongObjectPtr<UFF_BuildMultiSelectionWidget> Widget(CreateWidget<UFF_BuildMultiSelectionWidget>(World, Class));
	Class->bCanCallInitializedWithoutPlayerContext = bPrevious;
	if (!TestNotNull(TEXT("Selection widget instance"), Widget.Get())) return false;
	const TSharedRef<SWidget> Slate = Widget->TakeWidget();
	UBorder* Panel = Cast<UBorder>(Widget->GetWidgetFromName(TEXT("Border_MultiSelection")));
	if (!TestNotNull(TEXT("Designer selection panel"), Panel)) return false;
	Panel->SetVisibility(ESlateVisibility::Visible);
	Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UTextBlock* Count = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Text_MultiSelectionCount")));
	if (Count) Count->SetText(FText::FromString(TEXT("선택 128개")));
	UEditableTextBox* Name = Cast<UEditableTextBox>(Widget->GetWidgetFromName(TEXT("Edit_PresetName")));
	UComboBoxString* Presets = Cast<UComboBoxString>(Widget->GetWidgetFromName(TEXT("Combo_Presets")));
	if (!TestNotNull(TEXT("Authored preset name field bound"), Name) ||
		!TestNotNull(TEXT("Authored preset picker bound"), Presets)) return false;
	Name->SetText(FText::FromString(TEXT("먹거리 거리")));
	Presets->AddOption(TEXT("휴게 공간"));
	Presets->SetSelectedOption(TEXT("휴게 공간"));
	UClass* CardClass = Widget->SelectedFacilityCardClass.LoadSynchronous();
	if (!TestNotNull(TEXT("Selected facilities use an editable card WBP"), CardClass)) return false;
	UHorizontalBox* Cards = Cast<UHorizontalBox>(Widget->GetWidgetFromName(TEXT("Row_SelectedFacilities")));
	if (!TestNotNull(TEXT("Facility card row"), Cards)) return false;
	auto* List = Cast<UVerticalBox>(Widget->GetWidgetFromName(TEXT("Column_A5_SelectedFacilities")));
	auto* PanelSize = Cast<USizeBox>(Widget->GetWidgetFromName(TEXT("Size_MultiSelection")));
	auto* BoundsSlot = PanelSize ? Cast<UCanvasPanelSlot>(PanelSize->Slot) : nullptr;
	if (!TestNotNull(TEXT("Approved vertical facility list"), List) ||
		!TestNotNull(TEXT("Selection panel bounds"), PanelSize) ||
		!TestNotNull(TEXT("Selection panel canvas slot"), BoundsSlot)) return false;
	TestTrue(TEXT("Selection panel is bottom-right aligned"), PanelSize->GetWidthOverride() == 390.f &&
		BoundsSlot->GetAnchors().Minimum.Equals(FVector2D(1.f, 1.f)) &&
		BoundsSlot->GetAlignment().Equals(FVector2D(1.f, 1.f)));
	TestTrue(TEXT("Original selection row hosts the vertical list"), List->GetParent() == Cards);
	if (UWidget* Empty = Widget->GetWidgetFromName(TEXT("Text_MultiSelectionDesignerHint"))) Empty->SetVisibility(ESlateVisibility::Collapsed);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UUserWidget* Card = CreateWidget<UUserWidget>(World, CardClass);
		if (!TestNotNull(TEXT("Editable card template instantiates"), Card)) return false;
		Card->TakeWidget();
		if (UTextBlock* Text = Cast<UTextBlock>(Card->GetWidgetFromName(TEXT("Text_FacilityName"))))
			Text->SetText(FText::FromString(Index == 0 ? TEXT("먹거리 부스") : Index == 1 ? TEXT("그늘 쉼터") : TEXT("공연 앞 휴게 공간")));
		List->AddChildToVerticalBox(Card)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	}
	TArray<UButton*> Buttons;
	Widget->WidgetTree->ForEachWidget([&](UWidget* Child)
	{
		if (UButton* Button = Cast<UButton>(Child)) { Button->SetIsEnabled(true); Buttons.Add(Button); }
	});
	TestTrue(TEXT("Saved widget exposes all ten group/preset actions"), Buttons.Num() >= 13);
	if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi"))) return true;
	if (UMaterialInterface* Material = Cast<UMaterialInterface>(Panel->Background.GetResourceObject())) Material->EnsureIsComplete();
	if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
	FlushRenderingCommands();
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/ConstructionEditing");
	IFileManager::Get().MakeDirectory(*Directory, true);
	for (const FIntPoint Size : { FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080), FIntPoint(2560, 1440) })
	{
		const float Dpi = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Size);
		const TSharedRef<SWidget> Scaled = SNew(SDPIScaler).DPIScale(Dpi)[Slate];
		FWidgetRenderer Renderer(false, true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(FVector2D(Size), TF_Bilinear, true));
		if (!Target) return false;
		for (int32 Frame = 0; Frame < 4; ++Frame) Renderer.DrawWidget(Target.Get(), Scaled, FVector2D(Size), 0.f, false);
		const FSlateRect Bounds = Panel->GetCachedGeometry().GetLayoutBoundingRect();
		TestTrue(TEXT("Expanded selection panel fits the viewport"), Bounds.Left >= 0 && Bounds.Top >= 0 && Bounds.Right <= Size.X && Bounds.Bottom <= Size.Y);
		for (UButton* Button : Buttons)
		{
			if (!Button->IsVisible()) continue;
			const FSlateRect Rect = Button->GetCachedGeometry().GetLayoutBoundingRect();
			TestTrue(*FString::Printf(TEXT("%s is inside panel and usable"), *Button->GetName()),
				Rect.Left >= Bounds.Left && Rect.Right <= Bounds.Right + 1.f && Rect.Top >= Bounds.Top && Rect.Bottom <= Bounds.Bottom + 1.f &&
				Rect.GetSize().X >= 20 * Dpi && Rect.GetSize().Y >= 18 * Dpi);
			if (UTextBlock* Label = Cast<UTextBlock>(Button->GetContent()))
				TestTrue(TEXT("Action label fits without ellipsis"), Label->GetDesiredSize().X <= Label->GetCachedGeometry().GetLocalSize().X + 1.f);
		}
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags Flags;
		Flags.SetLinearToGamma(false);
		if (!Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)) return false;
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
		const FString Path = Directory / FString::Printf(TEXT("GroupEditing_%dx%d.png"), Size.X, Size.Y);
		TestTrue(TEXT("Group editor PNG saved"), FFileHelper::SaveArrayToFile(Png, *Path));
		AddInfo(FPaths::ConvertRelativePathToFull(Path));
	}
	return true;
}
#endif
