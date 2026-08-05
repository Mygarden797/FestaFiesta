/**
 * @file FF_FFZSSaveGameTestObject.h
 * @brief World Snapshot을 Unreal의 실제 SaveGame archive에 통과시키기 위한 테스트 전용 root다.
 *
 * Snapshot 필드에 `UPROPERTY(SaveGame)`를 붙여야 SaveGameToMemory가 값을 기록한다는 사실을
 * 검증한다. 프로젝트의 슬롯명·디스크·클라우드 정책을 구현하는 Production SaveGame이 아니다.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "FF_FFZSTypes.h"
#include "FF_FFZSSaveGameTestObject.generated.h"

/** Unreal의 실제 SaveGame Archive를 검증하기 위한 테스트 전용 영속성 Root다. */
UCLASS()
class UFFZSSaveGameTestObject final : public USaveGame
{
    GENERATED_BODY()

public:
    /** SaveGame archive의 root property. 이 flag가 빠지면 내부 struct의 SaveGame 필드도 진입하지 않는다. */
    UPROPERTY(SaveGame)
    FFFZSWorldSnapshot Snapshot;
};
