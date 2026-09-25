// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Tutorial/FF_DialogueBoxWidget.h"
#include "Tutorial/FF_SkipConfirmWidget.h"
#include "Tutorial/FF_TutorialOverlayWidget.h"
#include "FF_TutorialSettings.generated.h"

class UDataTable;
class UFF_DialogueScriptData;

// 튜토리얼의 프로젝트 기본값.
// Project Settings > Game > FestaFiesta Tutorial 에서 지정한다.
//
// 레벨마다 다른 대사로 시작할 수 있어야 해서 GameMode 쪽 지정이 우선한다.
// 여기 값은 그게 비어 있을 때 쓴다.
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "FestaFiesta Tutorial"))
class FESTAFIESTA_API UFF_TutorialSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Tutorial")
	static const UFF_TutorialSettings* Get();

	/* 새 게임을 누르면 재생할 대사.

	   축제 레벨이 아니라 로비에서 돈다. 사무실 장면처럼 축제장에 도착하기 전의
	   이야기가 여기 들어간다.

	   튜토리얼을 이미 봤으면 재생하지 않는다. 저장에서 읽는다.
	*/
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tutorial",
		meta = (DisplayName = "Lobby Intro Table"))
	TSoftObjectPtr<UDataTable> LobbyIntroTable;

	// 어느 레벨에서든 시작할 대사. 보통 비워둔다.
	//
	// 레벨마다 다른 대사가 필요하므로 GameMode 지정이 우선한다.
	// 여기에 넣으면 모든 레벨에서 재생되므로, 전체에 공통으로 나와야 하는
	// 안내가 아니라면 GameMode 쪽에 지정하는 편이 맞다.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tutorial",
		meta = (DisplayName = "Default Start Script"))
	TSoftObjectPtr<UFF_DialogueScriptData> DefaultStartScript;

	// 시작까지 기다릴 시간(초).
	//
	// 레벨이 뜨자마자 대사가 나오면 화면이 아직 정리되기 전이라 어수선하다.
	// 0 이면 즉시 시작한다.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tutorial",
		meta = (ClampMin = "0.0"))
	float StartDelaySeconds = 0.5f;

	// 대사창 위젯. 지정하면 대사가 시작될 때 자동으로 화면에 띄운다.
	//
	// 직접 띄우고 싶으면 비워두면 된다. 그때는 만들어둔 위젯이 알아서 구독한다.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tutorial",
		meta = (DisplayName = "Dialogue Box Widget Class"))
	TSoftClassPtr<UFF_DialogueBoxWidget> DialogueBoxWidgetClass;

	// 건너뛰기 확인 창. 비워두면 건너뛰기 버튼이 눌려도 아무 일도 없다.
	//
	// 확인 없이 바로 끝내지 않는 이유는 되돌릴 수 없는 선택이라서다.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tutorial",
		meta = (DisplayName = "Skip Confirm Widget Class"))
	TSoftClassPtr<UFF_SkipConfirmWidget> SkipConfirmWidgetClass;

	// 확인 창의 화면 순서. 대사창보다 위여야 가려지지 않는다.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tutorial")
	int32 SkipConfirmZOrder = 200;

	// 강조 오버레이 위젯. 비워두면 강조 표시가 나오지 않는다.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tutorial",
		meta = (DisplayName = "Overlay Widget Class"))
	TSoftClassPtr<UFF_TutorialOverlayWidget> OverlayWidgetClass;

	// 대사창과 오버레이의 화면 순서. 큰 값이 위에 온다.
	//
	// 대사창이 오버레이보다 위여야 어두운 덮개에 가려지지 않는다.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tutorial")
	int32 OverlayZOrder = 100;

	/* 축제 레벨에서 대사창의 화면 순서.

	   튜토리얼 레벨에서는 다른 UI(준비 HUD 등)를 대사창 위에서 눌러야 해서 음수로
	   내려 쓴다. 그 값을 로비에 그대로 쓰면 UI 루트(0) 밑에 깔려 대사가 안 보이므로
	   로비는 아래 LobbyDialogueBoxZOrder 를 따로 쓴다.
	*/
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tutorial")
	int32 DialogueBoxZOrder = 110;

	/* 로비 인트로에서 대사창의 화면 순서.

	   로비는 지구본 화면이 UI 루트(z 0) 위에 꽉 차게 뜬다. 대사가 그 위에 얹혀야
	   하므로 0 보다 커야 한다. 로비에서는 대사 말고 누를 UI 가 없어서 위로 올려도 된다.
	*/
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tutorial")
	int32 LobbyDialogueBoxZOrder = 110;

	// 건너뛰기를 눌렀을 때 물어볼 문구.
	//
	// 확인 창을 따로 만들지 않고 대사창의 선택지를 그대로 쓴다.
	// 창을 새로 만들면 위젯과 그걸 띄울 자리가 필요한데, 물어보는 것뿐이라
	// 이미 있는 것으로 충분하다.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tutorial|Skip")
	FText SkipConfirmText = FText::FromString(
		TEXT("튜토리얼을 건너뛰면 조작 안내가 더 이상 나오지 않습니다. 계속하시겠습니까?"));

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tutorial|Skip")
	FText SkipConfirmYesText = FText::FromString(TEXT("건너뛰기"));

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tutorial|Skip")
	FText SkipConfirmNoText = FText::FromString(TEXT("계속하기"));

	// 카메라를 이 거리 이상 움직이면 이동한 것으로 본다.
	//
	// 너무 작으면 손이 미끄러진 것도 조작으로 잡히고, 너무 크면 한참 움직여야
	// 다음으로 넘어간다.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tutorial",
		meta = (ClampMin = "0.0"))
	float CameraMoveThresholdCm = 1500.f;

	// 줌 배율이 이만큼 달라지면 줌한 것으로 본다. 0.3 이면 30% 변화.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tutorial",
		meta = (ClampMin = "0.01"))
	float CameraZoomThresholdRatio = 0.3f;
};
