// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tutorial/FF_TutorialMarker.h"

#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"

AFF_TutorialMarker::AFF_TutorialMarker()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

#if WITH_EDITORONLY_DATA
	// 레벨에서 눈에 보이게 아이콘을 붙인다.
	// 없으면 빈 액터라 어디 놓았는지 찾기가 어렵다.
	Icon = CreateDefaultSubobject<UBillboardComponent>(TEXT("Icon"));
	if (Icon)
	{
		Icon->SetupAttachment(SceneRoot);
		Icon->bIsScreenSizeScaled = true;
	}
#endif
}
