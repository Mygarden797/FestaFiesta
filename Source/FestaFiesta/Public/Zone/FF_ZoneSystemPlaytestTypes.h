#pragma once

#include "CoreMinimal.h"
#include "FF_ZoneSystemPlaytestTypes.generated.h"

/**
 * 편집 가능한 Zone 플레이테스트 WBP가 표시할 값만 모은 경량 상태 구조체다.
 * 시뮬레이션 객체를 UI에 직접 노출하지 않고 텍스트, 색상과 버튼 활성 조건만 전달한다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_ZoneSystemPlaytestHudState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Zone Playtest")
    FText ZoneText;

    UPROPERTY(BlueprintReadOnly, Category = "Zone Playtest")
    FText PresetText;

    UPROPERTY(BlueprintReadOnly, Category = "Zone Playtest")
    FText PlacementText;

    UPROPERTY(BlueprintReadOnly, Category = "Zone Playtest")
    FText ValidationText;

    UPROPERTY(BlueprintReadOnly, Category = "Zone Playtest")
    FText StatusText;

    UPROPERTY(BlueprintReadOnly, Category = "Zone Playtest")
    FText HelpText;

    UPROPERTY(BlueprintReadOnly, Category = "Zone Playtest")
    FLinearColor ValidationColor = FLinearColor::White;

    UPROPERTY(BlueprintReadOnly, Category = "Zone Playtest")
    FLinearColor StatusColor = FLinearColor::White;

    UPROPERTY(BlueprintReadOnly, Category = "Zone Playtest")
    bool bHasSelectedFacility = false;

    UPROPERTY(BlueprintReadOnly, Category = "Zone Playtest")
    bool bHasSavedSnapshot = false;

    /** 중앙 레이가 Zone 내부의 수평 지면을 가리키는지 나타낸다. */
    UPROPERTY(BlueprintReadOnly, Category = "Zone Playtest")
    bool bHasPlacementTarget = false;

    /** 지면 타깃과 플러그인 배치 검증을 모두 통과해 실제 설치가 가능한지 나타낸다. */
    UPROPERTY(BlueprintReadOnly, Category = "Zone Playtest")
    bool bCanCommitPlacement = false;

    UPROPERTY(BlueprintReadOnly, Category = "Zone Playtest")
    bool bShowHelp = true;

    UPROPERTY(BlueprintReadOnly, Category = "Zone Playtest")
    bool bUiInteractionMode = false;

    UPROPERTY(BlueprintReadOnly, Category = "Zone Playtest")
    bool bShowWorldDebug = false;
};

