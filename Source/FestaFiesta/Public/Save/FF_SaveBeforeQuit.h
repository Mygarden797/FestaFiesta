// FF_SaveBeforeQuit.h
// 역할: "게임 저장 후 종료" 가 쓰는 저장 시도와 종료 가능 여부 판정을 한곳에 모은다.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FF_SaveBeforeQuit.generated.h"

/* 종료 직전 저장의 결과.

   "저장이 안 됐다" 를 하나로 묶으면 화면이 두 경우를 가리지 못한다.
   타이틀에서 종료하는 것은 담을 판이 없을 뿐이라 그냥 꺼도 되고,
   축제 도중에 쓰기를 실패한 것은 그대로 끄면 진행이 사라진다.
*/
UENUM(BlueprintType)
enum class EFF_SaveBeforeQuitResult : uint8
{
	/** 진행 상황을 슬롯에 썼다. 바로 종료해도 된다. */
	Saved			UMETA(DisplayName = "저장함"),

	/** 담을 판이 없다(타이틀·플레이 허브·불러오는 중). 종료해도 잃는 것이 없다. */
	NothingToSave	UMETA(DisplayName = "저장할 것 없음"),

	/** 축제 도중인데 쓰지 못했다. 종료하면 진행이 사라지므로 화면이 막아야 한다. */
	Failed			UMETA(DisplayName = "저장 실패"),
};

/**
 * 종료 버튼이 부르는 저장 진입점.
 *
 * 저장 자체는 UFF_SaveSubsystem::SaveManual 에 맡긴다. 축제 도중이면 판 전체를,
 * 판을 시작한 로비면 로비 세이브를 남기고, 담을 판이 없으면 막는 규칙이 이미 거기 있다.
 * 여기서 규칙을 다시 만들면 저장 버튼과 종료 버튼이 서로 다른 판단을 하게 된다.
 *
 * SaveManual 은 SaveGameToSlot 을 그대로 부르는 동기 저장이라, 결과가 Saved 면
 * 같은 프레임에 QuitGame 을 불러도 파일이 다 쓰인 뒤다.
 */
UCLASS()
class FESTAFIESTA_API UFF_SaveBeforeQuitLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 지금 판을 저장하고 종료해도 되는지 돌려준다.
	 * @param OutSlotId 저장한 슬롯. Saved 가 아니면 None.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Save", meta = (WorldContext = "WorldContextObject"))
	static EFF_SaveBeforeQuitResult SaveBeforeQuit(const UObject* WorldContextObject, FName& OutSlotId);

	/**
	 * 축제가 진행 중인가. 이때 저장에 실패하면 종료를 막아야 한다.
	 * SaveManual 이 축제 세이브를 고르는 조건(FFF_SessionSave::IsFestivalInProgress)과 맞춘다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Save", meta = (WorldContext = "WorldContextObject"))
	static bool IsFestivalInProgress(const UObject* WorldContextObject);
};
