// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Minigame/FF_MinigameTypes.h"
#include "FF_ClawMachineTypes.generated.h"

class UMaterialInterface;
class UStaticMesh;

/** Authoritative mechanical phase. Presentation reads this state but never advances it. */
UENUM(BlueprintType)
enum class EFF_ClawMachinePhase : uint8
{
	Aiming,
	Dropping,
	Closing,
	Lifting,
	Returning,
	Releasing,
	RoundComplete
};

UENUM(BlueprintType)
enum class EFF_ClawMachineInputResult : uint8
{
	Applied,
	DropStarted,
	RejectedNotPlaying,
	RejectedWrongSession,
	RejectedNotAiming,
	RejectedNoAttempts,
	RejectedInvalidInput
};

UENUM(BlueprintType)
enum class EFF_ClawMachineFeedbackType : uint8
{
	None,
	Grabbed,
	PerfectGrab,
	Slipped,
	EmptyDrop
};

/**
 * One planner-authored prize archetype.
 *
 * Balance fields and replaceable art references intentionally live in one row so a content
 * owner can duplicate an archetype without asking code to add a parallel visual mapping.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_ClawMachinePrizeTuning : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "인형 데이터|기본",
		meta = (DisplayName = "인형 ID", ToolTip = "저장·로그·아트 연결에 쓰는 고유 ID입니다. 중복할 수 없습니다."))
	FName PrizeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "인형 데이터|기본",
		meta = (DisplayName = "표시 이름", ToolTip = "획득 결과 UI에 표시되는 현지화 가능한 이름입니다."))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "인형 데이터|밸런스",
		meta = (ClampMin = "1", UIMin = "1", DisplayName = "기본 점수"))
	int32 ScoreValue = 300;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "인형 데이터|밸런스",
		meta = (ClampMin = "0.01", UIMin = "0.01", DisplayName = "등장 가중치", ToolTip = "전체 가중치 대비 이 인형이 채워질 상대 확률입니다."))
	float SpawnWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "인형 데이터|밸런스",
		meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", DisplayName = "집기 난이도", ToolTip = "0은 쉽게 잡히고 1은 정중앙 정렬과 높은 집게 힘이 필요합니다."))
	float GripDifficulty = 0.42f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "인형 데이터|밸런스",
		meta = (ClampMin = "0.01", ClampMax = "0.3", UIMin = "0.02", UIMax = "0.2", DisplayName = "잡힘 반경", ToolTip = "인형 중심에서 집게까지 허용하는 정규화 거리입니다."))
	float CaptureRadiusNormalized = 0.105f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "인형 데이터|물성",
		meta = (ClampMin = "0.05", ClampMax = "3.0", Units = "kg", DisplayName = "질량", ToolTip = "무거운 인형일수록 동일한 집게 힘에서 미끄러질 가능성이 커집니다."))
	float MassKg = 0.32f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "인형 데이터|물성",
		meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "표면 마찰", ToolTip = "집게 발과 인형 표면 사이의 유효 마찰 계수입니다."))
	float SurfaceFriction = 0.58f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "인형 데이터|물성",
		meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "압축 순응도", ToolTip = "봉제 인형이 집게 압력에 맞춰 눌리며 고정되는 정도입니다."))
	float GripCompliance = 0.64f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "인형 데이터|물성",
		meta = (DisplayName = "무게중심 오프셋", ToolTip = "인형 중심 대비 무게중심의 정규화 오프셋입니다. 클수록 편심 토크가 발생합니다."))
	FVector2D CenterOfMassOffsetNormalized = FVector2D::ZeroVector;

	/** Optional designer mesh. The native arena uses a lightweight plush fallback when empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "인형 데이터|아트 교체",
		meta = (DisplayName = "3D 인형 메시", ToolTip = "비워 두면 최적화된 기본 3D 봉제인형 형태를 사용합니다."))
	TSoftObjectPtr<UStaticMesh> PrizeMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "인형 데이터|아트 교체",
		meta = (DisplayName = "인형 머티리얼", ToolTip = "비워 두면 기본 색상으로 동적 머티리얼을 만듭니다."))
	TSoftObjectPtr<UMaterialInterface> PrizeMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "인형 데이터|아트 교체",
		meta = (DisplayName = "기본 인형 색상"))
	FLinearColor FallbackColor = FLinearColor(0.18f, 0.78f, 0.86f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "인형 데이터|아트 교체",
		meta = (ClampMin = "0.1", ClampMax = "3.0", DisplayName = "3D 표시 배율"))
	FVector VisualScale = FVector(1.0f);

	bool IsValid(FString* OutFailureReason = nullptr) const;
};

/** All values a planner needs for feel, difficulty, pacing and prize composition. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_ClawMachineRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "집게 조작|이동",
		meta = (ClampMin = "0.05", ClampMax = "2.0", DisplayName = "집게 이동 속도", ToolTip = "초당 정규화 플레이 영역에서 이동하는 거리입니다."))
	float MovementSpeedNormalizedPerSecond = 0.54f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "집게 조작|이동",
		meta = (DisplayName = "이동 최소 경계", ToolTip = "플레이 영역 왼쪽 위가 (0,0)입니다."))
	FVector2D MovementMinNormalized = FVector2D(0.08f, 0.08f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "집게 조작|이동",
		meta = (DisplayName = "이동 최대 경계", ToolTip = "플레이 영역 오른쪽 아래가 (1,1)입니다."))
	FVector2D MovementMaxNormalized = FVector2D(0.92f, 0.86f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "집게 조작|이동",
		meta = (DisplayName = "집게 시작·배출 위치"))
	FVector2D HomePositionNormalized = FVector2D(0.14f, 0.12f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "집게 조작|판정",
		meta = (ClampMin = "0.01", ClampMax = "0.3", DisplayName = "집게 기본 탐색 반경"))
	float GripRadiusNormalized = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "집게 조작|판정",
		meta = (ClampMin = "0.0", ClampMax = "1.5", DisplayName = "집게 힘"))
	float GripPower = 0.62f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "집게 조작|판정",
		meta = (ClampMin = "0.0", ClampMax = "0.5", DisplayName = "기계 편차", ToolTip = "세션 시드로 고정되는 작은 집게 편차입니다. 같은 입력과 시드에서는 결과가 같습니다."))
	float DeterministicGripVariance = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "집게 조작|판정",
		meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "성공 판정선"))
	float RequiredGripQuality = 0.62f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "점수·횟수",
		meta = (ClampMin = "1", ClampMax = "10", DisplayName = "한 판 시도 횟수"))
	int32 MaxAttempts = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "점수·횟수",
		meta = (ClampMin = "0.5", ClampMax = "1.0", DisplayName = "퍼펙트 정렬 기준"))
	float PerfectAlignmentThreshold = 0.82f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "점수·횟수",
		meta = (ClampMin = "0", DisplayName = "퍼펙트 보너스 점수"))
	int32 PerfectGrabBonusScore = 120;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "기계 애니메이션",
		meta = (ClampMin = "0.05", Units = "s", DisplayName = "하강 시간"))
	float DropDurationSeconds = 0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "기계 애니메이션",
		meta = (ClampMin = "0.05", Units = "s", DisplayName = "집게 닫힘 시간"))
	float CloseDurationSeconds = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "기계 애니메이션",
		meta = (ClampMin = "0.05", Units = "s", DisplayName = "상승 시간"))
	float LiftDurationSeconds = 0.64f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "기계 애니메이션",
		meta = (ClampMin = "0.05", Units = "s", DisplayName = "출구 복귀 시간"))
	float ReturnDurationSeconds = 0.78f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "기계 애니메이션",
		meta = (ClampMin = "0.05", Units = "s", DisplayName = "인형 배출 시간"))
	float ReleaseDurationSeconds = 0.34f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "물리 디지털트윈|갠트리",
		meta = (ClampMin = "1.0", ClampMax = "12.0", Units = "Hz", DisplayName = "갠트리 응답 주파수", ToolTip = "높을수록 집게 상부가 입력 목표를 빠르게 따라갑니다."))
	float GantryResponseFrequencyHz = 5.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "물리 디지털트윈|갠트리",
		meta = (ClampMin = "0.2", ClampMax = "2.0", DisplayName = "갠트리 감쇠비", ToolTip = "1은 임계 감쇠이며, 낮을수록 관성이 오래 남습니다."))
	float GantryDampingRatio = 0.86f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "물리 디지털트윈|케이블",
		meta = (ClampMin = "80.0", ClampMax = "400.0", Units = "cm", DisplayName = "진자 기준 케이블 길이"))
	float PendulumLengthCm = 175.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "물리 디지털트윈|케이블",
		meta = (ClampMin = "0.1", ClampMax = "8.0", DisplayName = "진자 감쇠", ToolTip = "케이블 흔들림이 초당 줄어드는 정도입니다."))
	float PendulumDampingPerSecond = 2.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "물리 디지털트윈|케이블",
		meta = (ClampMin = "1.0", ClampMax = "25.0", Units = "deg", DisplayName = "최대 케이블 흔들림"))
	float MaxPendulumSwingDegrees = 11.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "물리 디지털트윈|인형",
		meta = (ClampMin = "1.0", ClampMax = "12.0", Units = "Hz", DisplayName = "획득 인형 추종 주파수"))
	float HeldPrizeResponseFrequencyHz = 4.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "인형 배치",
		meta = (ClampMin = "1", ClampMax = "64", DisplayName = "기계 안 인형 수"))
	int32 PrizeCount = 18;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "인형 배치",
		meta = (DisplayName = "인형 배치 최소 경계"))
	FVector2D PrizeSpawnMinNormalized = FVector2D(0.12f, 0.22f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "인형 배치",
		meta = (DisplayName = "인형 배치 최대 경계"))
	FVector2D PrizeSpawnMaxNormalized = FVector2D(0.90f, 0.90f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "인형 배치",
		meta = (ClampMin = "0.0", ClampMax = "0.3", DisplayName = "인형 최소 간격"))
	float PrizeMinSeparationNormalized = 0.075f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "인형 데이터",
		meta = (TitleProperty = "PrizeId", DisplayName = "인형 종류 목록", ToolTip = "기획자는 점수·확률·난이도를, 디자이너는 메시·머티리얼·색상을 이 목록에서 교체합니다."))
	TArray<FFF_ClawMachinePrizeTuning> PrizePool;

	bool IsValid(FString* OutFailureReason = nullptr) const;
	const FFF_ClawMachinePrizeTuning* FindPrize(FName PrizeId) const;
};

USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_ClawMachinePrizeSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Claw Machine")
	int32 PrizeSerial = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Claw Machine")
	FName PrizeId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Claw Machine")
	FVector2D PositionNormalized = FVector2D(0.5f, 0.5f);

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Claw Machine")
	float RotationDegrees = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Claw Machine")
	bool bAvailable = true;
};

/** Complete read model used by native UMG, WBP implementations and automation. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_ClawMachineSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Claw Machine")
	FFF_MinigameSessionSnapshot Session;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Claw Machine")
	EFF_ClawMachinePhase Phase = EFF_ClawMachinePhase::Aiming;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Claw Machine")
	float PhaseAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Claw Machine")
	FVector2D ClawPositionNormalized = FVector2D(0.14f, 0.12f);

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Claw Machine")
	FVector2D DropPositionNormalized = FVector2D(0.14f, 0.12f);

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Claw Machine")
	int32 AttemptsUsed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Claw Machine")
	int32 MaxAttempts = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Claw Machine")
	int32 CapturedCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Claw Machine")
	int32 MissedAttempts = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Claw Machine")
	bool bCarryingPrize = false;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Claw Machine")
	int32 HeldPrizeSerial = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Claw Machine")
	float LastAlignmentQuality = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Claw Machine")
	float LastGripQuality = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Claw Machine")
	FName LatestPrizeId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Claw Machine")
	int32 LatestScoreDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Claw Machine")
	int32 LatestFeedbackSerial = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Claw Machine")
	EFF_ClawMachineFeedbackType LatestFeedbackType = EFF_ClawMachineFeedbackType::None;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Claw Machine")
	TArray<FFF_ClawMachinePrizeSnapshot> Prizes;
};
