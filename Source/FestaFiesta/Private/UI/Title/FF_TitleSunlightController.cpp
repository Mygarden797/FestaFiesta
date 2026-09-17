#include "UI/Title/FF_TitleSunlightController.h"

#include "Components/MeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/Core/FF_UIAccessibilitySubsystem.h"
#include "UI/FF_UILog.h"

AFF_TitleSunlightController::AFF_TitleSunlightController()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AFF_TitleSunlightController::BeginPlay()
{
	Super::BeginPlay();
	BackdropMesh = IsValid(BackdropActor) ? BackdropActor->FindComponentByClass<UMeshComponent>() : nullptr;
	if (!IsValid(BackdropMesh))
	{
		UE_LOG(LogFestaFiestaUI, Warning, TEXT("햇살 컨트롤러: 배경 액터의 메시를 찾지 못했습니다."));
		return;
	}
	OriginalMaterial = BackdropMesh->GetMaterial(0);
	SunMaterial = BackdropMesh->CreateDynamicMaterialInstance(0);
	Accessibility = UFF_UIAccessibilitySubsystem::Get(this);
	if (Accessibility.IsValid())
	{
		Accessibility->OnReduceMotionChanged.AddUniqueDynamic(this, &ThisClass::OnReduceMotionChanged);
	}
	OnReduceMotionChanged(Accessibility.IsValid() && Accessibility->IsReduceMotionEnabled());
}

void AFF_TitleSunlightController::OnReduceMotionChanged(bool bEnabled)
{
	if (SunMaterial) SunMaterial->SetScalarParameterValue(TEXT("SunMotionEnabled"), bEnabled ? 0.f : 1.f);
}

void AFF_TitleSunlightController::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Accessibility.IsValid()) Accessibility->OnReduceMotionChanged.RemoveDynamic(this, &ThisClass::OnReduceMotionChanged);
	if (IsValid(BackdropMesh) && BackdropMesh->GetMaterial(0) == SunMaterial) BackdropMesh->SetMaterial(0, OriginalMaterial);
	SunMaterial = nullptr;
	OriginalMaterial = nullptr;
	BackdropMesh = nullptr;
	Accessibility.Reset();
	Super::EndPlay(Reason);
}
