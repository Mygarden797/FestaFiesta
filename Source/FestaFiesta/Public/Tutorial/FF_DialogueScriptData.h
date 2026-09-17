// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Tutorial/FF_TutorialTypes.h"
#include "FF_DialogueScriptData.generated.h"

// 한 씬의 대사 묶음.
//
// 씬 단위로 나누는 이유는 중간부터 다시 시작할 수 있어야 하기 때문이다.
// 전체를 한 파일에 넣으면 "예산 설명부터 다시" 같은 게 안 된다.
UCLASS(BlueprintType)
class FESTAFIESTA_API UFF_DialogueScriptData : public UDataAsset
{
	GENERATED_BODY()

public:
	// 이 씬을 부를 이름. 다른 씬에서 이어붙일 때 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName ScriptId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TArray<FFF_DialogueLine> Lines;

	// 이 씬이 끝나면 이어서 재생할 씬. 비어 있으면 거기서 끝난다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TSoftObjectPtr<UFF_DialogueScriptData> NextScript;

	// 이 씬이 도는 동안 시뮬레이션을 멈출지.
	//
	// 대사를 읽는 사이에 시간이 흐르면 안 되는 구간이 있다. 반대로 운영 중에
	// 안내만 띄우는 경우에는 멈추면 곤란하다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	bool bPauseSimulation = true;

	// 라벨로 줄을 찾는다. 없으면 INDEX_NONE.
	int32 FindLineIndexByLabel(FName Label) const
	{
		if (Label.IsNone())
		{
			return INDEX_NONE;
		}
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			if (Lines[Index].Label == Label)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}
};
