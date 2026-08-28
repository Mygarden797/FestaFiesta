// Fill out your copyright notice in the Description page of Project Settings.


#include "Interaction/FF_TriggerInteractionActor.h"

#include "AbilitySystemComponent.h"

#include "Interaction/FF_GameplayAbility_TriggerInteraction.h"

#include "Components/SphereComponent.h"

AFF_TriggerInteractionActor::AFF_TriggerInteractionActor()
{
	InteractionRange = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionRange"));
	// ⭐ 순서 변경 - InteractionRange를 먼저 만들고
	InteractionRange->SetSphereRadius(150.f);

	RootComponent = InteractionRange;
	// ⭐ NEW - InteractionRange를 이 액터의 "뿌리 컴포넌트"로 명확히 지정
	// 이렇게 해야 다른 컴포넌트(Cube 등)들이 여기에 붙을 기준점이 생김

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	// 능력 가방은 위치가 필요 없어서 순서 상관없음

	InteractionRange->OnComponentBeginOverlap.AddDynamic(this, &AFF_TriggerInteractionActor::OnRangeBeginOverlap);
	InteractionRange->OnComponentEndOverlap.AddDynamic(this, &AFF_TriggerInteractionActor::OnRangeEndOverlap);
}

UAbilitySystemComponent* AFF_TriggerInteractionActor::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
	// 외부/시스템 요청 시 소유한 GAS 컴포넌트 반환
}

void AFF_TriggerInteractionActor::TryStartInteraction()
{
	if (!AbilitySystemComponent || !InteractionAbilityClass)
		
	{
		UE_LOG(LogTemp, Warning, TEXT("[TriggerInteraction] AbilitySystemComponent 또는 InteractionAbilityClass가 비어있습니다."));
		return;
		// 실행할 상호작용 기능이 비어있으면 경고 및 종료
	}

	if (UGameInstance* GameInstance = GetGameInstance())
		
	{
		if (UFF_FestivalStageSubsystem* StageSubsystem = GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>())
			
		{
			if (!StageSubsystem->IsLiveStage())
				
			{
				UE_LOG(LogTemp, Log, TEXT("[TriggerInteraction] Live 단계가 아니라 상호작용 불가"));
				return;
				
			}
		}
	}
	//live단계인지 확인

	if (!AbilitySystemComponent->FindAbilitySpecFromClass(InteractionAbilityClass))
		
	{
		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(InteractionAbilityClass, 1));
		
	}

	AbilitySystemComponent->TryActivateAbilityByClass(InteractionAbilityClass);
	// 준비된 상호작용 기능 실행
}

void AFF_TriggerInteractionActor::OnRangeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor->IsA(APawn::StaticClass()))
		// 범위 안에 들어온 대상이 플레이어인지 확인
	{
		bPlayerInRange = true;
		// 플레이어 진입 상태로 전환

		UE_LOG(LogTemp, Log, TEXT("[TriggerInteraction] 플레이어가 범위에 들어옴"));
	}
}

void AFF_TriggerInteractionActor::OnRangeEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor && OtherActor->IsA(APawn::StaticClass()))
		// 범위 밖으로 나간 대상이 플레이어인지 확인
	{
		bPlayerInRange = false;
		// 플레이어 이탈 상태로 전환

		UE_LOG(LogTemp, Log, TEXT("[TriggerInteraction] 플레이어가 범위에서 나감"));
	}
}