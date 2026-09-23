// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Save/FF_Saveable.h"
#include "FF_FestivalBuzzSubsystem.generated.h"

struct FFF_SnsPostRow;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFF_OnBuzzChanged, float, NewBuzz, float, PreviousBuzz);

USTRUCT()
struct FFF_BuzzSave
{
	GENERATED_BODY()

	UPROPERTY() float Buzz = 50.f;
	UPROPERTY() float BuzzAtDayStart = 50.f;
};

/**
 * 축제 화제성(Buzz). 축제 전체에 값 하나, 0~100, 50 이 중립이다.
 *
 * 오르내리는 곳: 홍보 구매(시작값), SNS 글(긍정 +·부정 -), 유료 프로그램 개최, 하루 경계(중립으로 식음).
 * 쓰는 곳: 하루 예상 방문객 배수(UFF_FestivalProgressionSubsystem::GetVisitorForecast)와 축제 평가.
 *
 * GameInstance 스코프다 — 하루를 넘어 축제 끝까지 쌓이는 값이라 World 보다 오래 산다.
 * 계수(얼마나 움직이나)는 축제 정의(UFF_FestivalDefinition)의 Buzz 항목이 갖는다.
 */
UCLASS()
class FESTAFIESTA_API UFF_FestivalBuzzSubsystem : public UGameInstanceSubsystem, public IFF_Saveable
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- 읽기 (UI) ---

	// 0~100. 50 이 중립.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Buzz")
	float GetBuzz() const { return Buzz; }

	// 오늘 아침(하루 경계 직후) 값. 지금 값과 비교하면 "오늘 얼마나 올랐나" 가 나온다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Buzz")
	float GetBuzzAtDayStart() const { return BuzzAtDayStart; }

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Buzz")
	FFF_OnBuzzChanged OnBuzzChanged;

	// --- 쓰기 ---

	// 0~100 으로 자른다. Reason 은 로그용이다.
	void AddBuzz(float Delta, FName Reason);

	// SNS 글 한 건을 반영한다. 크기는 축제 정의의 BuzzPerSnsPost × 화제 강도.
	void ApplySnsPost(const FFF_SnsPostRow& Post);

	// 새 축제. 중립으로 되돌린다.
	void ResetForNewFestival();

	// 하루 경계. 중립 쪽으로 DecayRatio 만큼 식히고 오늘 아침 값을 기록한다.
	void BeginNewDay(float DecayRatio);

	// --- 저장 ---
	virtual void WriteSaveData(TArray<uint8>& OutBytes) const override;
	virtual void ReadSaveData(const TArray<uint8>& InBytes) override;
	virtual FName GetSaveKey() const override { return TEXT("FestivalBuzz"); }

private:
	void SetBuzz(float NewBuzz, FName Reason);

	float Buzz = 50.f;
	float BuzzAtDayStart = 50.f;
};
