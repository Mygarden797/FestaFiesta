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

// 플레이어가 [보내기]/[확인]을 눌렀을 때의 결과. 화면 문구는 UFF_UIDisplayTextLibrary 가 낸다.
UENUM(BlueprintType)
enum class EFF_IncidentResolveResult : uint8
{
	Dispatched        = 0	UMETA(DisplayName = "출동"),
	Resolved          = 1	UMETA(DisplayName = "즉시 해결"),
	InvalidIncident   = 2	UMETA(DisplayName = "사건 없음"),
	InvalidOption     = 3	UMETA(DisplayName = "선택지 없음"),
	AlreadyResponding = 4	UMETA(DisplayName = "이미 출동 중"),
	NotEnoughStaff    = 5	UMETA(DisplayName = "스태프 부족"),
	NoSite            = 6	UMETA(DisplayName = "현장 위치 없음"),
	DispatchFailed    = 7	UMETA(DisplayName = "출동 실패")
};

// 사건에 보낸 스태프가 지금 어디까지 왔는지. 화면 문구는 UFF_UIDisplayTextLibrary 가 낸다.
UENUM(BlueprintType)
enum class EFF_IncidentResponseStage : uint8
{
	None       = 0	UMETA(DisplayName = "출동 없음"),
	Travelling = 1	UMETA(DisplayName = "이동 중"),
	Working    = 2	UMETA(DisplayName = "작업 중")
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

// 사건이 닫힌 순간의 값 복사본. 닫힌 뒤에는 Handle 로 조회할 수 없고 슬롯도 재사용되므로 방송 전에 채운다.
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_IncidentClosedInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Incident")
	FFF_IncidentHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Incident")
	FGameplayTag IncidentTag;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Incident")
	FText Title;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Incident")
	EFF_IncidentState FinalState = EFF_IncidentState::Resolved;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Incident")
	EFF_IncidentTone Tone = EFF_IncidentTone::Problem;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Incident")
	int32 ChosenOptionIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Incident")
	bool bWasResponding = false;
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

// 선택지 하나의 비용·보상을 값으로 복사한 것. UI 가 UFF_IncidentOption 을 직접 잡지 않게 한다.
// 정의에서 선택지가 null 이어도 bValid=false 로 자리를 채워 OptionIndex 와 배열 위치를 맞춘다.
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_IncidentOptionView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Incident|Option")
	int32 OptionIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Incident|Option")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Incident|Option")
	FText Label;

	UPROPERTY(BlueprintReadOnly, Category = "Incident|Option")
	int32 CostGold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Incident|Option")
	int32 CostStaff = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Incident|Option")
	int32 MinStaff = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Incident|Option")
	float WorkAmount = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Incident|Option")
	FText RewardText;
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
	float MinuteUntileLapse = 60.f;

	UPROPERTY(BlueprintReadOnly, Category = "Incident|Identity")
	EFF_IncidentTone Tone = EFF_IncidentTone::Problem;

	UPROPERTY(BlueprintReadOnly, Category = "Incident|Identity")
	EFF_IncidentUrgency Urgency = EFF_IncidentUrgency::Medium;

	UPROPERTY(BlueprintReadOnly, Category = "Incident|Display")
	float OccurredAtMinutes = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Incident|Options")
	TArray<FFF_IncidentOptionView> Options;

	// 출동 단계. None 이면 아직 아무도 보내지 않았거나 출동이 취소된 것이다.
	UPROPERTY(BlueprintReadOnly, Category = "Incident|Response")
	EFF_IncidentResponseStage ResponseStage = EFF_IncidentResponseStage::None;

	// Working 일 때만 의미가 있다. 축제 분 단위.
	UPROPERTY(BlueprintReadOnly, Category = "Incident|Response")
	float RemainingWorkMinutes = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Incident|Response")
	float TotalWorkMinutes = 0.f;
};

