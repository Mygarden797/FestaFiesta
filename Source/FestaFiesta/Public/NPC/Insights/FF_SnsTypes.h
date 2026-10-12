// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FF_SnsTypes.generated.h"

UENUM(BlueprintType)
enum class EFF_SnsSentiment : uint8
{
	Positive,
	Negative
};

UENUM(BlueprintType)
enum class EFF_SnsReactionResult : uint8
{
	Success,
	AlreadyReacted,
	PostUnavailable,
	Unavailable
};

USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_SnsReactionState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) bool bExists = false;
	UPROPERTY(BlueprintReadOnly) bool bReacted = false;
	UPROPERTY(BlueprintReadOnly) bool bCanReact = false;
	// 설정상 보상. 실제 수용량은 화제성 상한에 따라 줄어들 수 있다.
	UPROPERTY(BlueprintReadOnly) float BuzzReward = 0.f;
};

USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_SnsReactionOutcome
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) EFF_SnsReactionResult Result = EFF_SnsReactionResult::Unavailable;
	UPROPERTY(BlueprintReadOnly) float AppliedBuzz = 0.f;
};

/**
 * SNS 피드에 올라간 글 한 건. `UFF_CrowdInsightsSubsystem`의 링버퍼가 들고 있는 단위.
 *
 * `DisplayHandle`은 실제 `StableAgentId`가 아니라 이 값만 보고 만든 표시용 번호다 —
 * 실제 개체를 특정할 수 있게 노출하지 않는다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_SnsPostRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|SNS")
	FGuid PostId;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|SNS")
	FString DisplayHandle;		// "방문객 #1234" — 실제 StableAgentId 아님

	// 시설 표기 방식이 아직 미정이라(utility_ai_todo.md §10) 지금은 항상 NAME_None이다.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|SNS")
	FName FacilityCategory;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|SNS")
	EFF_SnsSentiment Sentiment = EFF_SnsSentiment::Positive;

	/**
	 * 이 화제가 그 순간 얼마나 두드러졌는지(0~100) — `FFF_SnsFeedGenerator::ComputeTopicScore`
	 * 값을 그대로 담는다. "이 글의 감정이 얼마나 센지"가 아니라 "이 화제를 고른 신호가
	 * 얼마나 강했는지"다 — 화제 선택에 실제로 쓰인 값을 재사용하는 것이라, 임의로 정한
	 * 상수보다 근거가 있다. 강제 화제(피날레 등)는 게이트를 통과했다는 사실 자체가
	 * 신호이므로 100 고정.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|SNS")
	float TopicSalience0To100 = 0.f;

	// 화제 번호(1~13, 재방문=14). utility_ai_todo.md §10 확정 목록과 대응
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|SNS")
	int32 TopicId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|SNS")
	FName MessageTemplateId;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|SNS")
	FText Body;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|SNS")
	float GameTimestampSeconds = 0.f;
};
