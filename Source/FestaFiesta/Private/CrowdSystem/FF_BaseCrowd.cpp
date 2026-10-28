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
	SkeletalMeshComponent->SetReceivesDecals(false);


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
	ProxyMeshComponent->SetReceivesDecals(false);
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

	// 메시 유무를 따지기 전에 받아 둬야 한다. 메시가 없으면 아래에서 곧바로 return 하는데,
	// 외형은 런타임에 DataTable로 들어오므로 BP에 기본 메시가 없는 구성도 정상이다.
	if (IsValid(SkeletalMeshComponent))
	{
		DefaultAnimClass = SkeletalMeshComponent->GetAnimClass();
	}

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
	SetState(EFF_CrowdRunState::Running);
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
		const bool bInPlace = CurrentState == EFF_CrowdRunState::Holding
			|| CurrentState == EFF_CrowdRunState::Dancing;
		CrowdAnimInstance->SetSpeed(bInPlace ? 0.f : NewSpeed);
	}
}

void AFF_BaseCrowd::SetAnimPlayRate(float PlayRate)
{
	RunningAnimPlayRate = PlayRate;
	ApplyStateToAnimation();
}

void AFF_BaseCrowd::SetState(EFF_CrowdRunState NewState)
{
	if (CurrentState == NewState)
	{
		return;
	}

	CurrentState = NewState;
	ApplyStateToAnimation();

	// 제자리 상태에서 마지막 걷기 속도가 남으면 제자리 걷기가 되므로 Speed 를 0 으로 내린다
	if (CrowdAnimInstance && (CurrentState == EFF_CrowdRunState::Holding || CurrentState == EFF_CrowdRunState::Dancing))
	{
		CrowdAnimInstance->SetSpeed(0.f);
	}
}

void AFF_BaseCrowd::ApplyStateToAnimation()
{
	if (IsValid(SkeletalMeshComponent))
	{
		float PlayRate = 1.f;
		if (CurrentState == EFF_CrowdRunState::Running)
		{
			PlayRate = RunningAnimPlayRate;
		}
		else if (CurrentState == EFF_CrowdRunState::Stopped)
		{
			PlayRate = 0.f;
		}
		SkeletalMeshComponent->GlobalAnimRateScale = PlayRate;
	}

	if (CrowdAnimInstance)
	{
		CrowdAnimInstance->SetState(CurrentState);
	}
}

// AnimClass는 '클래스'를 받는다.
void AFF_BaseCrowd::ApplyAppearance(USkeletalMesh* Mesh, UClass* AnimClass, float YawOffset, float UniformScale)
{
	if (!IsValid(SkeletalMeshComponent))
	{
		return;
	}

	MeshYawOffset = YawOffset;
	SkeletalMeshComponent->SetSkeletalMeshAsset(Mesh);
	SkeletalMeshComponent->SetRelativeScale3D(FVector(UniformScale));

	UClass* const DesiredAnimClass = AnimClass ? AnimClass : DefaultAnimClass.Get();
	if (DesiredAnimClass != nullptr)
	{
		SkeletalMeshComponent->SetAnimInstanceClass(DesiredAnimClass);
	}

	CrowdAnimInstance = Cast<UFF_CrowdAnimInstance>(SkeletalMeshComponent->GetAnimInstance());
	ApplyStateToAnimation();

	// 메시가 있는데 인스턴스가 없다면 AnimClass의 대상 Skeleton과 메시의 Skeleton이 다른 것이다.
	// InitAnim은 이 경우 조용히 아무것도 만들지 않으므로 여기서 짚어 주지 않으면 원인을 찾을 수 없다.
	if (Mesh != nullptr && !IsValid(CrowdAnimInstance))
	{
		static bool bLoggedAppearanceAnimFailure = false;
		if (!bLoggedAppearanceAnimFailure)
		{
			bLoggedAppearanceAnimFailure = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[AFF_BaseCrowd] ApplyAppearance: AnimInstance를 얻지 못했다 — AnimClass(%s)의 대상 Skeleton과 ")
				TEXT("Mesh(%s)의 Skeleton이 다른지 확인할 것 (이하 동일 경고 생략)"),
				*GetNameSafe(DesiredAnimClass), *GetNameSafe(Mesh));
		}
	}

	if (IsValid(ProxyMeshComponent))
	{
		ProxyMeshComponent->SetVisibility(Mesh == nullptr);
	}

}


