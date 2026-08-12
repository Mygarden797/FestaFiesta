// Fill out your copyright notice in the Description page of Project Settings.


#include "CrowdSystem/FF_BaseCrowd.h"

#include "CrowdSystem/Components/FF_CrowdAnimInstance.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace FFBaseCrowd
{
	// 대체 표시용 원기둥의 크기. 엔진 기본 Cylinder는 지름 100UU, 높이 100UU에 원점이 중앙이다.
	// FUnitData::Radius(34UU)에 맞춰 지름 68UU, 키 180UU로 세우고, 발이 Agent 위치에 닿도록 절반만큼 올린다.
	static constexpr float ProxyHeightUU = 180.f;
	static constexpr float ProxyDiameterUU = 68.f;
}


AFF_BaseCrowd::AFF_BaseCrowd()
{

	PrimaryActorTick.bCanEverTick = false;


	RootSceneComponent = CreateDefaultSubobject<USceneComponent>("RootSceneComponent");
	SetRootComponent(RootSceneComponent);

	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("SkeletalMeshComponent");
	SkeletalMeshComponent->SetupAttachment(RootSceneComponent);
	

	// 대체 표시용 원기둥. 기본은 꺼 둔다 — SkeletalMesh가 없을 때만 BeginPlay에서 켠다.
	ProxyMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("ProxyMeshComponent");
	ProxyMeshComponent->SetupAttachment(RootSceneComponent);
	ProxyMeshComponent->SetRelativeLocation(FVector(0.f, 0.f, FFBaseCrowd::ProxyHeightUU * 0.5f));
	ProxyMeshComponent->SetRelativeScale3D(FVector(
		FFBaseCrowd::ProxyDiameterUU / 100.f,
		FFBaseCrowd::ProxyDiameterUU / 100.f,
		FFBaseCrowd::ProxyHeightUU / 100.f));
	ProxyMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProxyMeshComponent->SetCanEverAffectNavigation(false);
	// 500개가 그림자를 던지면 표시 비용이 시뮬레이션 비용을 덮어 버린다. 확인용이므로 끈다.
	ProxyMeshComponent->SetCastShadow(false);
	ProxyMeshComponent->SetVisibility(false);

	// 생성자에서 찾아야 쿠킹 시 참조로 잡혀 패키징 빌드에서도 빠지지 않는다.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ProxyMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (ProxyMeshFinder.Succeeded())
	{
		ProxyMeshComponent->SetStaticMesh(ProxyMeshFinder.Object);
	}
}


void AFF_BaseCrowd::BeginPlay()
{
	Super::BeginPlay();

	const bool bHasSkeletalMesh = IsValid(SkeletalMeshComponent) && SkeletalMeshComponent->GetSkeletalMeshAsset() != nullptr;

	// 메시가 없으면 원기둥으로 대신 보여 준다. 있으면 원기둥은 계속 꺼진 채로 둔다.
	if (IsValid(ProxyMeshComponent))
	{
		ProxyMeshComponent->SetVisibility(!bHasSkeletalMesh);
	}

	if (!bHasSkeletalMesh)
	{
		// AnimBP를 따질 상황이 아니다. 아래 경고는 건너뛴다.
		return;
	}

	CrowdAnimInstance = Cast<UFF_CrowdAnimInstance>(SkeletalMeshComponent->GetAnimInstance());
	if (!IsValid(CrowdAnimInstance))
	{
		// 풀 크기만큼(수백 개) 같은 줄이 쏟아지면 정작 읽어야 할 로그가 묻힌다. 한 번만 남긴다.
		static bool bLoggedMissingAnimInstance = false;
		if (!bLoggedMissingAnimInstance)
		{
			bLoggedMissingAnimInstance = true;
			UE_LOG(LogTemp,Warning,TEXT("[AFF_BaseCrowd] SkeletalMeshComponent's AnimInstance is not CrowdAnimInstance (이하 동일 경고 생략)"));
		}
	}
}




void AFF_BaseCrowd::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AFF_BaseCrowd::BindToAgent(int32 NewAgentIndex, FVector SpawnLocation, FRotator SpawnRotation)
{
	AgentIndex = NewAgentIndex;
	SetActorLocationAndRotation(SpawnLocation,SpawnRotation);
}

void AFF_BaseCrowd::ReleaseFromAgent()
{
	AgentIndex = INDEX_NONE;
}

void AFF_BaseCrowd::PushPresentation(FVector NewPosition, FRotator NewRotator, float NewSpeed)
{
	// GetVelocity()용. 반드시 MeshYawOffset을 더하기 전에 계산해야 한다 —
	// 그 오프셋은 메시 표시 보정이고 진행 방향이 아니다. 보정 후 값으로 재면 속도가 옆을 향한다.
	CachedVelocity = NewRotator.Vector() * NewSpeed;

	// 메시 정면이 +X가 아닌 에셋을 위한 보정. 시뮬레이션 회전값은 건드리지 않는다 —
	// 여기는 표시 단계이고, 회전의 '의미'는 진행 방향 그대로여야 한다.
	NewRotator.Yaw += MeshYawOffset;

	SetActorLocationAndRotation(NewPosition,NewRotator,false);

	// AnimInstance가 없어도 위치는 갱신한다 — 애니메이션만 못 할 뿐 이동은 되어야 한다.
	// 없다는 경고는 BeginPlay에서 한 번만 찍는다. 여기서 찍으면 Actor 수 x 매 프레임만큼 로그가 쏟아진다.
	if (CrowdAnimInstance)
	{
		CrowdAnimInstance->SetSpeed(NewSpeed);
	}
}


