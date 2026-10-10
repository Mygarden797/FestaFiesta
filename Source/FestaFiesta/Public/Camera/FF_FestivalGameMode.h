// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "FF_FestivalGameMode.generated.h"

// 관찰 시점 게임 모드.
//
// 하는 일은 카메라 Pawn 과 PlayerController 를 이어주는 것뿐이다.
// 규칙, 승패, 스폰 흐름은 여기 넣지 않는다. 그건 정해지면 각자 주인이 생길 것이고,
// 지금 미리 자리를 잡아두면 나중에 그 사람이 이 파일을 고쳐야 한다.
//
// 레벨의 축제장 범위를 알고 있으면 InitialFocusLocation 과 InitialArmLength 를
// 지정해 첫 구도를 잡아줄 수 있다. 안 지정하면 Pawn 이 자기 위치를 기준으로 시작한다.
UCLASS()
class FESTAFIESTA_API AFF_FestivalGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFF_FestivalGameMode();

	virtual void BeginPlay() override;

	// 축제장 중심. 보통 메인 게이트 앞이나 중앙 광장이다.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Camera")
	FVector InitialFocusLocation = FVector::ZeroVector;

	// 축제장 전체가 한 화면에 들어오는 거리.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Camera", meta = (ClampMin = "100.0"))
	float InitialArmLength = 12000.f;

	// true 면 BeginPlay 에서 위 두 값으로 카메라 구도를 잡는다.
	// 레벨에 배치한 Pawn 의 위치를 그대로 쓰고 싶으면 끈다.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Camera")
	bool bApplyInitialFraming = true;

	// 저장된 초기 초점 높이에서 시작하고, 최초 지면 대비 높이를 유지하며 이동한다.
	// 끄면 기존처럼 지면 + CameraSettings.GroundFocusOffset으로 초점을 맞춘다.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Camera", meta = (EditCondition = "bApplyInitialFraming"))
	bool bPreserveInitialCameraHeight = true;

	// 이 레벨에서 쓸 피날레 연출 수치.
	//
	// 비워두면 Project Settings 의 기본값을 쓴다. 연등축제처럼 연출이 다른 레벨에서만
	// 여기에 따로 지정하면 되고, 보통 레벨은 건드릴 필요가 없다.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Finale")
	TSoftObjectPtr<class UFF_FinaleShowData> FinaleShowDataOverride;

	// 이 레벨이 시작될 때 재생할 튜토리얼 대사.
	//
	// 튜토리얼 레벨에만 지정한다. 비워두면 아무것도 시작하지 않으므로
	// 본편 레벨의 GameMode 는 손댈 필요가 없다.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Tutorial")
	TSoftObjectPtr<class UFF_DialogueScriptData> TutorialStartScript;

	// CSV 로 만든 대사 표. 위 애셋보다 먼저 본다.
	//
	// 대사가 길어지면 표가 편하다. 엑셀에서 쓰고 찾아바꾸기가 되고,
	// 깃에서도 줄 단위로 합쳐진다.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Tutorial")
	TSoftObjectPtr<class UDataTable> TutorialStartTable;

	// 이 레벨에서 쓸 사운드 표.
	//
	// 비워두면 Project Settings 의 기본값을 쓴다. 분위기가 다른 레벨에서만 지정하면 된다.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Audio")
	TSoftObjectPtr<class UFF_AudioBankData> AudioBankOverride;

	// 플레이어가 직접 조종할 방문객 캐릭터. 보통 BP_FestivalCharacter 를 넣는다.
	//
	// 레벨에 이미 배치돼 있으면 그것을 쓰고, 없으면 PlayerStart 에 스폰한다.
	// 두 방식을 다 열어둔 이유는, 배치 쪽이 위치를 눈으로 정할 수 있어 작업 중에 편하고
	// 스폰 쪽이 레벨마다 캐릭터를 놓지 않아도 되기 때문이다.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Character")
	TSubclassOf<APawn> VisitorCharacterClass;

	// 끄면 캐릭터를 찾지도 스폰하지도 않는다. 관찰 전용 레벨에서 쓴다.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Character")
	bool bPrepareVisitorCharacter = true;

private:
	// 레벨에서 찾거나 PlayerStart 에 스폰해서 컨트롤러에 넘긴다.
	void PrepareVisitorCharacter(class AFF_FestivalPlayerController* Controller);
};
