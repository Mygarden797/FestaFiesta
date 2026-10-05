// Fill out your copyright notice in the Description page of Project Settings.

#include "Staff/FF_StaffActor.h"

#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CrowdSystem/Components/FF_CrowdAnimInstance.h"
#include "Engine/StaticMesh.h"
#include "GameplayEffect.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Staff/FF_StaffAttributeSet.h"
#include "UObject/ConstructorHelpers.h"

namespace FFStaffActor
{
	static constexpr float ProxyHeightUU = 180.f;
	static constexpr float ProxyDiameterUU = 68.f;
}

AFF_StaffActor::AFF_StaffActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
	SetRootComponent(RootSceneComponent);

	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMeshComponent"));
	SkeletalMeshComponent->SetupAttachment(RootSceneComponent);
	SkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkeletalMeshComponent->SetCanEverAffectNavigation(false);
	SkeletalMeshComponent->SetReceivesDecals(false);

	ProxyMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProxyMeshComponent"));
	ProxyMeshComponent->SetupAttachment(RootSceneComponent);
	ProxyMeshComponent->SetRelativeLocation(FVector(0.f, 0.f, FFStaffActor::ProxyHeightUU * 0.5f));
	ProxyMeshComponent->SetRelativeScale3D(FVector(
		FFStaffActor::ProxyDiameterUU / 100.f,
		FFStaffActor::ProxyDiameterUU / 100.f,
		FFStaffActor::ProxyHeightUU / 100.f));
	ProxyMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProxyMeshComponent->SetCanEverAffectNavigation(false);
	ProxyMeshComponent->SetReceivesDecals(false);
	ProxyMeshComponent->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ProxyMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (ProxyMeshFinder.Succeeded())
	{
		ProxyMeshComponent->SetStaticMesh(ProxyMeshFinder.Object);
	}
}

UAbilitySystemComponent* AFF_StaffActor::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AFF_StaffActor::SetStaffId(int32 InStaffId)
{
	StaffId = InStaffId;
}

void AFF_StaffActor::PushPresentation(const FVector& NewPosition, FRotator NewRotation, float NewSpeed)
{
	NewRotation.Yaw += MeshYawOffset;
	SetActorLocationAndRotation(NewPosition, NewRotation, false);
	if (AnimInstance)
	{
		AnimInstance->SetSpeed(NewSpeed);
	}
}

void AFF_StaffActor::InitFromLevel(int32 TrainingLevel)
{
}

float AFF_StaffActor::GetMoveSpeed() const
{
	return 0.f;
}

float AFF_StaffActor::GetWorkPower() const
{
	return 0.f;
}

bool AFF_StaffActor::ApplyEffect(TSubclassOf<UGameplayEffect> Effect)
{
	return false;
}

void AFF_StaffActor::BeginPlay()
{
	Super::BeginPlay();
	const bool bHasSkeletalMesh = SkeletalMeshComponent && SkeletalMeshComponent->GetSkeletalMeshAsset();
	if (bHasSkeletalMesh)
	{
		AnimInstance = Cast<UFF_CrowdAnimInstance>(SkeletalMeshComponent->GetAnimInstance());
	}
	if (!ProxyMeshComponent)
	{
		return;
	}
	ProxyMeshComponent->SetVisibility(!bHasSkeletalMesh);
	if (!bHasSkeletalMesh)
	{
		if (UMaterialInstanceDynamic* Material = ProxyMeshComponent->CreateDynamicMaterialInstance(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), ProxyColor);
		}
	}
}
