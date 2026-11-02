#include "Building/Presentation/FF_BuildFeatureGuideButton.h"

#include "Building/FF_BuildModeSubsystem.h"
#include "Building/Presentation/FF_BuildFeatureGuide.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "UI/Core/FF_BuildAtlasImage.h"
#include "Widgets/SWidget.h"

void UFF_BuildFeatureGuideButton::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshGuidePresentation();
}

void UFF_BuildFeatureGuideButton::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button_Guide) Button_Guide->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGuideClicked);
}

void UFF_BuildFeatureGuideButton::NativeConstruct()
{
	Super::NativeConstruct();
	// The inner Slate button exists after construction; apply the accessibility
	// label there as well as the UMG tooltip used by mouse and keyboard users.
	RefreshGuidePresentation();
}

FSlateBrush UFF_BuildFeatureGuideButton::ResolveIconBrush() const
{
	FSlateBrush Resolved = IconBrush;
	if (!bUseIconAtlas) return Resolved;
	UTexture2D* Texture = Cast<UTexture2D>(Resolved.GetResourceObject());
	if (!FFBuildAtlas::TryApply(Resolved, Texture, IconAtlasColumns, IconAtlasRows, IconAtlasCell))
	{
		// Never display the full atlas when an instance has an invalid cell.
		Resolved.SetResourceObject(nullptr);
	}
	return Resolved;
}

void UFF_BuildFeatureGuideButton::RefreshGuidePresentation()
{
	if (Image_GuideIcon) Image_GuideIcon->SetBrush(ResolveIconBrush());
	if (!Button_Guide) return;
	const auto* Topic = FFBuildFeatureGuide::Find(FeatureTopicId);
	const FText Title = Topic ? Topic->Title : FText::GetEmpty();
	Button_Guide->SetToolTipText(Title);
#if WITH_ACCESSIBILITY
	if (const TSharedPtr<SWidget> Slate = Button_Guide->GetCachedWidget())
		Slate->SetAccessibleBehavior(Topic ? EAccessibleBehavior::Custom : EAccessibleBehavior::NotAccessible,
			TAttribute<FText>(Title), EAccessibleType::Main);
#endif
}

bool UFF_BuildFeatureGuideButton::RestoreGuideFocus()
{
	APlayerController* Controller = GetOwningPlayer();
	if (!FSlateApplication::IsInitialized() || !Controller || !GetOwningLocalPlayer() || !IsVisible() ||
		!Button_Guide || !Button_Guide->IsVisible() || !Button_Guide->GetIsEnabled() ||
		!Button_Guide->GetCachedWidget().IsValid()) return false;
	Button_Guide->SetFocus();
	return Button_Guide->HasUserFocus(Controller);
}

void UFF_BuildFeatureGuideButton::HandleGuideClicked()
{
	TryOpenGuide();
}

bool UFF_BuildFeatureGuideButton::TryOpenGuide()
{
	if (IsDesignTime() || FeatureTopicId.IsNone() || !FFBuildFeatureGuide::Find(FeatureTopicId)) return false;

	// A nested UUserWidget is instanced inside the parent's WidgetTree. Do not
	// search globally: another player or another build screen could own a guide.
	UFF_BuildToolsWidget* Tools = GetTypedOuter<UFF_BuildToolsWidget>();
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	UWorld* World = GetWorld();
	if (!IsValid(Tools) || !LocalPlayer || Tools->GetOwningLocalPlayer() != LocalPlayer ||
		!World || Tools->GetWorld() != World || !Tools->IsVisible()) return false;

	const UFF_BuildModeSubsystem* Build = World->GetSubsystem<UFF_BuildModeSubsystem>();
	if (!Build || Build->IsInputSuspendedForPauseMenu() ||
		!FFBuildModeModel::IsBuildSessionActive(Build->GetBuildModeState())) return false;
	// The old settings toolbar button also closed its own drawer. Preserve that
	// route while keeping all other auxiliary panels protected from icon clicks.
	const UWidget* Details = Tools->GetWidgetFromName(TEXT("Panel_Details"));
	if (FeatureTopicId == TEXT("BuildSettings") && Tools->IsBuildSettingsOpen() && !Tools->IsFeatureGuideOpen() &&
		(!Details || !Details->IsVisible()))
	{
		Tools->ToggleBuildSettings();
		return true;
	}
	if (Tools->IsAuxiliaryPanelOpen()) return false;

	return Tools->OpenToolbarFeatureTopic(FeatureTopicId, this);
}
