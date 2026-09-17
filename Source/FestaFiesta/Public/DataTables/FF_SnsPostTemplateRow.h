// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FF_SnsPostTemplateRow.generated.h"

/**
 * SNS 피드 문장 한 줄. data/unreal/sns_post_templates_v0.csv 에서 Import as DataTable.
 *
 * `Text`에 `{시설}`이 그대로 들어있을 수 있다(`bUsesFacilityPlaceholder`가 true인 행) —
 * 아직 시설 표기(실명 vs 카테고리) 자체가 미정이라 치환 규칙은 여기서 정하지 않는다.
 * 치환하는 쪽이 최종 표기 문자열을 넣는다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_SnsPostTemplateRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNS")
	FName TemplateId;

	// utility_ai_todo.md §10 확정 화제 목록의 번호(1~13) + 재방문(14)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNS")
	int32 TopicId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNS")
	FString TopicName;

	// "positive" / "negative". 실제 극성 판정은 GetTopicPolarity(TopicId)가 하고,
	// 이 값은 로드 시점에 그 판정과 일치하는지 검증하는 용도다(FFF_SnsFeedGenerator::DoesSentimentMatchTopic)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNS")
	FName Sentiment;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNS")
	bool bUsesFacilityPlaceholder = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNS")
	FText Text;
};
