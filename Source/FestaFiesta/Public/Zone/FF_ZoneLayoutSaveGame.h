// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"
#include "GameFramework/SaveGame.h"
#include "FF_ZoneLayoutSaveGame.generated.h"

/**
 * 축제장 배치 한 벌을 슬롯에 담는다. 반복 측정용 도구이지 플레이어 세이브가 아니다.
 *
 * `Test_Main` 은 빈 축제장으로 시작하고 배치는 매 세션 손으로 만든다. 그래서 자동 실행이
 * 항상 시설 0개인 땅에서 시작했다 — 거리·NavMesh 에 의존하는 수치를 재려면 사람이 실제로
 * 쓰는 배치가 있어야 한다.
 *
 * ★ 루트 필드에 `SaveGame` 태그가 없으면 내부 struct 의 SaveGame 필드까지 통째로 안 써진다.
 */
UCLASS()
class FESTAFIESTA_API UFF_ZoneLayoutSaveGame final : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	FFFZSWorldSnapshot Snapshot;

	// 어느 맵의 배치인가. 다른 맵에 복원하면 좌표가 엉뚱한 곳을 가리키므로 경고용으로 남긴다.
	UPROPERTY(SaveGame)
	FString MapName;
};
