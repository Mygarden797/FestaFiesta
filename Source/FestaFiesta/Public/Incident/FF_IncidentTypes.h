// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "FF_IncidentTypes.generated.h"

class AActor;
class UWorld;

// 값이 저장·집계 키가 된다. 중간에 끼워 넣지 않고 뒤에만 추가한다.
UENUM(BlueprintType)
enum class EFF_IncidentTone : uint8
{
	Problem     = 0	UMETA(DisplayName = "문제"),
	Opportunity = 1	UMETA(DisplayName = "기회"),
	Notice      = 2	UMETA(DisplayName = "알림")
};

// Tone 과 직교한다. 기회도 급할 수 있다.
UENUM(BlueprintType)
enum class EFF_IncidentUrgency : uint8
{
	Low       = 0	UMETA(DisplayName = "낮음"),
	Medium    = 1	UMETA(DisplayName = "보통"),
	High      = 2	UMETA(DisplayName = "높음"),
	Immediate = 3	UMETA(DisplayName = "즉시")
};

// Ignored(문제 방치)와 Missed(기회 놓침)를 합치지 않는다. 리포트에서 다른 이야기다.
UENUM(BlueprintType)
enum class EFF_IncidentState : uint8
{
	Active       = 0	UMETA(DisplayName = "발생"),
	Acknowledged = 1	UMETA(DisplayName = "확인"),
	Resolved     = 2	UMETA(DisplayName = "해소"),
	Taken        = 3	UMETA(DisplayName = "포착"),
	Expired      = 4	UMETA(DisplayName = "자연 소멸"),
	Ignored      = 5	UMETA(DisplayName = "방치"),
	Missed       = 6	UMETA(DisplayName = "놓침")
};

// Crowd 발 사건은 개별 NPC 가 아니라 DensityCell 로만 올린다.
UENUM(BlueprintType)
enum class EFF_IncidentAnchorKind : uint8
{
	WorldPoint  = 0	UMETA(DisplayName = "월드 좌표"),
	Actor       = 1	UMETA(DisplayName = "액터"),
	Zone        = 2	UMETA(DisplayName = "구역"),
	DensityCell = 3	UMETA(DisplayName = "밀집 셀"),
	Screenspace = 4	UMETA(DisplayName = "화면 전용")
};

// Serial 은 슬롯 재사용 시 옛 핸들이 남의 사건을 가리키는 것을 막는다.
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_IncidentHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Incident")
	int32 Index = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Incident")
	int32 Serial = 0;

	bool IsSet() const { return Index != INDEX_NONE; }

	bool operator==(const FFF_IncidentHandle& Other) const
	{
		return Index == Other.Index && Serial == Other.Serial;
	}

	bool operator!=(const FFF_IncidentHandle& Other) const { return !(*this == Other); }

	//해쉬 값을 만들기 위한 함수
	friend uint32 GetTypeHash(const FFF_IncidentHandle& Handle)
	{
		return HashCombine(::GetTypeHash(Handle.Index), ::GetTypeHash(Handle.Serial));
	}
};

// Kind 에 따라 아래 필드 중 하나만 의미를 갖는다.
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_IncidentAnchor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Incident")
	EFF_IncidentAnchorKind Kind = EFF_IncidentAnchorKind::Screenspace;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Incident")
	FVector WorldPoint = FVector::ZeroVector;

	// 약 포인터는 BP 노출이 안 된다.
	UPROPERTY()
	TWeakObjectPtr<AActor> Actor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Incident")
	FName ZoneId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Incident")
	FIntPoint DensityCell = FIntPoint::ZeroValue;
};

// 발생원이 아는 것은 이게 전부다. 문구·톤·긴급도·수명은 UFF_IncidentDefinition 에 있다.
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_IncidentReport
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Incident")
	FGameplayTag IncidentTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Incident")
	FFF_IncidentAnchor Anchor;

	// 비워 두면 허브가 FFF_IncidentRules::MakeDedupeKey 로 채운다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Incident")
	FName DedupeKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Incident", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Intensity = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Incident")
	FGameplayTagContainer ContextTags;
};


// 효과가 Apply 에서 받는 것. 허브 슬롯이 닫힌 뒤에도 쓸 수 있게 값으로 복사해 둔다.
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_IncidentContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Incident")
	TObjectPtr<UWorld> World = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Incident")
	FFF_IncidentHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Incident")
	FGameplayTag IncidentTag;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Incident")
	FFF_IncidentAnchor Anchor;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Incident")
	FGameplayTagContainer ContextTags;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Incident")
	int32 OptionIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Incident")
	float PeakIntensity = 0.f;
};

//UI 전달용으로 만들어진 View 전용 함수
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_IncidentView
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Incident|Tag")
	FGameplayTag IncidentTag;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Incident")
	FFF_IncidentAnchor Anchor;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "Incident|Display")
	FText Title;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "Incident|Display")
	FText Body;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "Incident|Display")
	TSoftObjectPtr<UTexture2D> MarkerIcon;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "Incident|Display")
	float MinuteUntileLapse = 60.f;
	
};

