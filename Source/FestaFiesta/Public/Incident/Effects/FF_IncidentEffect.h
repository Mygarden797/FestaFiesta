// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Incident/FF_IncidentTypes.h"
#include "FF_IncidentEffect.generated.h"

// 효과를 재실행하거나 UI가 잔액 차이를 추측하지 않고 전달하는 이번 실행의 영수증.
struct FFF_IncidentEffectResult
{
	int32 GrantedGold = 0;
};

// 사건을 처리했을 때 월드에 일어나는 일. 미정 영역은 이 클래스의 자식 목록뿐이다.
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, BlueprintType, CollapseCategories)
class FESTAFIESTA_API UFF_IncidentEffect : public UObject
{
	GENERATED_BODY()

public:
	virtual bool CanApply(const FFF_IncidentContext& Context) const { return true; }
	virtual void Apply(const FFF_IncidentContext& Context) {}
	virtual FFF_IncidentEffectResult ApplyWithResult(const FFF_IncidentContext& Context)
	{
		Apply(Context);
		return {};
	}

	virtual FText GetDescription() const { return FText::GetEmpty(); }
};
