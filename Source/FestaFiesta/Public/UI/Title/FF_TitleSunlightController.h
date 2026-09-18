#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FF_TitleSunlightController.generated.h"

class UFF_UIAccessibilitySubsystem;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;

/** 햇살 재질의 움직임을 플레이어의 움직임 감소 설정에 연결한다. 에디터 재질은 수정하지 않는다. */
UCLASS(meta = (DisplayName = "FF Title Sunlight Controller"))
class FESTAFIESTA_API AFF_TitleSunlightController : public AActor
{
	GENERATED_BODY()
public:
	AFF_TitleSunlightController();

	UPROPERTY(EditInstanceOnly, Category = "Sunlight", meta = (DisplayName = "햇살 배경 액터"))
	TObjectPtr<AActor> BackdropActor;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UFUNCTION()
	void OnReduceMotionChanged(bool bEnabled);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SunMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> OriginalMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMeshComponent> BackdropMesh;
	TWeakObjectPtr<UFF_UIAccessibilitySubsystem> Accessibility;
};
