// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Minigame/ClawMachine/FF_ClawMachineTypes.h"
#include "Minigame/FF_MinigameTypes.h"
#include "Minigame/Surfing/FF_SurfingTypes.h"
#include "FF_MinigameDefinition.generated.h"

class UFF_MinigameRuntime;
class UFF_MinigameScreenBase;
class UDataTable;

/**
 * Data-authored contract shared by every facility minigame.
 *
 * A definition contains identity, facility routing, duration and reward scaling,
 * while RuntimeClass contains only the session rules. This keeps balance changes
 * out of the coordinator and lets later minigames plug in without editing it.
 */
UCLASS(Abstract, BlueprintType, meta = (DisplayName = "미니게임 설정"))
class FESTAFIESTA_API UFF_MinigameDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UFF_MinigameDefinition();

	static const FPrimaryAssetType& GetMinigameDefinitionAssetType();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AdvancedDisplay, Category = "기본 정보",
		meta = (DisplayName = "미니게임 ID", ToolTip = "코드와 저장 데이터에서 사용하는 고유 ID입니다. 이미 배포된 뒤에는 변경하지 마세요."))
	FName MinigameId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "기본 정보",
		meta = (DisplayName = "게임 표시 이름", ToolTip = "UI에서 플레이어에게 보여 주는 미니게임 이름입니다."))
	FText DisplayName;

	/** Stable FFZS FacilityDefinitionIds that may launch this minigame. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AdvancedDisplay, Category = "기본 정보",
		meta = (DisplayName = "연결 시설 ID", ToolTip = "이 미니게임을 실행할 수 있는 시설 Definition ID 목록입니다. 기획 변경이 아니라면 수정하지 마세요."))
	TArray<FName> CompatibleFacilityDefinitionIds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "진행 시간",
		meta = (ClampMin = "0.1", UIMin = "1.0", Units = "s", DisplayName = "한 판 플레이 시간", ToolTip = "미니게임 한 판이 자동으로 종료될 때까지의 전체 시간입니다. 단위는 초입니다."))
	float DurationSeconds = 30.0f;

	/** A score at or above this value receives the coordinator's full reward cap. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "클리어 보상",
		meta = (ClampMin = "1", UIMin = "1", DisplayName = "최대 보상 달성 점수", ToolTip = "이 점수 이상이면 아래의 '클리어 최대 획득 골드'를 전부 받습니다. 더 낮은 점수는 달성 비율만큼 골드를 받습니다."))
	int32 ScoreForMaxReward = 1000;

	/** Per-minigame reward at ScoreForMaxReward. -1 keeps the festival-wide fallback. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "클리어 보상",
		meta = (ClampMin = "-1", UIMin = "-1", DisplayName = "클리어 최대 획득 골드", ToolTip = "게임을 정상 완료하고 '최대 보상 달성 점수'를 채웠을 때 받는 골드입니다. 점수가 낮으면 비율에 따라 줄어듭니다. -1은 축제 공통 설정을 사용하고, 0은 골드 보상을 주지 않습니다."))
	int32 RewardGoldAtMaxScore = -1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AdvancedDisplay, Category = "기술 설정",
		meta = (DisplayName = "런타임 클래스", ToolTip = "미니게임 규칙을 실행하는 코드 클래스입니다. 프로그래머 요청 없이 변경하지 마세요."))
	TSubclassOf<UFF_MinigameRuntime> RuntimeClass;

	/** Soft UI dependency: catalog/runtime tests never need to load a widget class. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AdvancedDisplay, Category = "기술 설정",
		meta = (DisplayName = "화면 위젯 클래스", ToolTip = "이 미니게임에 표시할 UI 위젯 클래스입니다. 프로그래머 또는 UI 담당자 요청 없이 변경하지 마세요."))
	TSoftClassPtr<UFF_MinigameScreenBase> ScreenClass;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame")
	bool SupportsFacility(FName FacilityDefinitionId) const;

	/** Resolves the per-minigame reward value, preserving the legacy festival fallback. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame")
	int32 ResolveRewardGoldAtMaxScore(int32 FestivalFallbackGold) const;

	/** Appends errors and returns true when this definition can create a session. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame")
	bool IsDefinitionValid(TArray<FText>& OutErrors) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	virtual void AppendValidationErrors(TArray<FText>& OutErrors) const;
};

/** First MVP: a deterministic thirty-second target-shooting session. */
UCLASS(BlueprintType, meta = (DisplayName = "과녁 슈팅 미니게임 설정"))
class FESTAFIESTA_API UFF_TargetShootingDefinition final : public UFF_MinigameDefinition
{
	GENERATED_BODY()

public:
	UFF_TargetShootingDefinition();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "슈팅 세부 설정",
		meta = (ShowOnlyInnerProperties, DisplayName = "과녁·점수 세부 규칙", ToolTip = "과녁 유지 시간과 크기, 명중 점수, 콤보, 정밀 판정, 보너스 과녁을 조정합니다."))
	FFF_TargetShootingRules Rules;

protected:
	virtual void AppendValidationErrors(TArray<FText>& OutErrors) const override;
};

/** Deterministic thirty-second free-ride session in a fixed two-dimensional arena. */
UCLASS(BlueprintType)
class FESTAFIESTA_API UFF_SurfingDefinition final : public UFF_MinigameDefinition
{
	GENERATED_BODY()

public:
	UFF_SurfingDefinition();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minigame|Surfing")
	FFF_SurfingRules Rules;

	/** Fixed Rock/Log/Buoy and pickup placement authored as FFF_SurfingCourseRow rows. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minigame|Surfing")
	TSoftObjectPtr<UDataTable> CourseLayoutTable;

protected:
	virtual void AppendValidationErrors(TArray<FText>& OutErrors) const override;
};

/** Three-dimensional, finite-attempt claw machine inspired by an arcade cabinet loop. */
UCLASS(BlueprintType, meta = (DisplayName = "인형뽑기 미니게임 설정"))
class FESTAFIESTA_API UFF_ClawMachineDefinition final : public UFF_MinigameDefinition
{
	GENERATED_BODY()

public:
	UFF_ClawMachineDefinition();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "인형뽑기 세부 설정",
		meta = (ShowOnlyInnerProperties, DisplayName = "기계·점수·인형 데이터", ToolTip = "조작감, 집게 판정, 점수, 시도 횟수와 교체 가능한 인형 에셋을 한 곳에서 관리합니다."))
	FFF_ClawMachineRules Rules;

	/** Optional row-based catalog. When assigned, its rows replace Rules.PrizePool. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "인형뽑기 세부 설정",
		meta = (DisplayName = "인형 카탈로그 데이터테이블", RowType = "/Script/FestaFiesta.FF_ClawMachinePrizeTuning", ToolTip = "기획자는 행 추가·삭제로 인형 목록과 밸런스를 관리하고, 디자이너는 같은 행의 메시·머티리얼을 교체합니다."))
	TSoftObjectPtr<UDataTable> PrizeCatalogTable;

	/** Resolves the DataTable override into a stable, row-name-sorted session catalog. */
	bool BuildResolvedPrizePool(
		TArray<FFF_ClawMachinePrizeTuning>& OutPrizePool,
		FString* OutFailureReason = nullptr) const;

protected:
	virtual void AppendValidationErrors(TArray<FText>& OutErrors) const override;
};
