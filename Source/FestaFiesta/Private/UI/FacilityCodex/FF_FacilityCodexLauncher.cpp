#include "UI/FacilityCodex/FF_FacilityCodexLauncher.h"

#include "Components/Button.h"
#include "Engine/World.h"
#include "UI/FacilityCodex/FF_FacilityCodexPresenterSubsystem.h"

void UFF_FacilityCodexLauncher::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button_OpenCodex) Button_OpenCodex->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleOpen);
}

void UFF_FacilityCodexLauncher::HandleOpen()
{
	UWorld* World = GetWorld();
	if (!World) return;
	if (auto* Presenter = World->GetSubsystem<UFF_FacilityCodexPresenterSubsystem>())
	{
		// An inspector selection is an entry point, not an implicit collection filter.
		Presenter->OpenCodex(GetOwningPlayer(), ExplicitFacilityId);
	}
}
