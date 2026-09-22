// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "FF_SaveTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogFestaFiestaSave, Log, All);

/* 축제 한 건의 최고 기록.

   같은 축제를 다시 맡을 수 있으므로 매번 덮어쓰지 않고 더 나은 쪽만 남긴다.
   작년 기록을 넘는 것이 이 게임의 반복 동기라서, 기록이 내려가면 그 동기가 사라진다.
*/
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_FestivalRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	int32 BestVisitors = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	float BestSatisfaction = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	int32 BestProfit = 0;

	// 몇 번 맡았는지. 두 번째부터는 안내를 줄이는 데 쓸 수 있다.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	int32 PlayCount = 0;

	// 더 나은 값만 남긴다. 무엇이 갱신됐는지 돌려준다.
	bool MergeBest(const FFF_FestivalRecord& Other)
	{
		bool bChanged = false;

		if (Other.BestVisitors > BestVisitors)
		{
			BestVisitors = Other.BestVisitors; bChanged = true;
		}
		if (Other.BestSatisfaction > BestSatisfaction)
		{
			BestSatisfaction = Other.BestSatisfaction; bChanged = true;
		}
		if (Other.BestProfit > BestProfit)
		{
			BestProfit = Other.BestProfit; bChanged = true;
		}

		PlayCount += FMath::Max(1, Other.PlayCount);
		return bChanged;
	}
};

/* 축제 사이에 남는 것들.

   게임을 껐다 켜도 유지돼야 하는 진행 상태다. 축제 도중의 상태와 달리
   어느 파트에도 속하지 않아서 저장 시스템이 직접 들고 있는다.
*/
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_MetaSave
{
	GENERATED_BODY()

	/* 로비 인트로를 끝까지 봤는가.

	   축제를 고르기 전에 로비에서 도는 오프닝이라 축제별로 나눌 수 없다.
	   계정 단위로 한 번만 보여준다.
	*/
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	bool bLobbyIntroSeen = false;

	/* 안내를 통째로 껐는가.

	   건너뛰기는 "이번 것만 넘긴다" 가 아니라 "안내 없이 운영만 하겠다" 는 선언이다.
	   그래서 축제가 바뀌어도 되살아나지 않는다. 끝까지 본 것과 달리 전역이다.

	   끝까지 본 것과 나눠 두는 이유는, 둘 다 "다시 안 보여준다" 는 같지만
	   건너뛴 사람에게는 나중에 다시 볼 기회를 주고 싶어질 수 있어서다.
	*/
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	bool bTutorialOptedOut = false;

	/* 튜토리얼을 끝까지 본 축제들.

	   축제마다 배울 것이 다르므로 판정도 축제 단위다. 전역 bool 하나로 두면
	   첫 축제를 끝낸 사람이 다음 축제의 안내를 영영 못 본다.

	   Set 이 아니라 배열인 이유는 ClearedFestivals 와 같다. 수가 적고,
	   저장 형식이 단순한 편이 나중에 눈으로 들여다볼 때 낫다.
	*/
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	TArray<FName> TutorialSeenFestivals;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	TArray<FName> ClearedFestivals;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	TMap<FName, FFF_FestivalRecord> Records;

	bool HasCleared(const FName FestivalId) const
	{
		return ClearedFestivals.Contains(FestivalId);
	}

	bool HasSeenTutorialFor(const FName FestivalId) const
	{
		return !FestivalId.IsNone() && TutorialSeenFestivals.Contains(FestivalId);
	}

	// ── 판 1 에서만 쓰던 값 ─────────────────────────────────────────────────

	/* 판 1 에는 전역 bool 두 개뿐이었다.

	   새 코드는 여기에 쓰지 않는다. 판 1 저장을 읽을 때 위의 값들로 한 번 옮기고
	   그 뒤로는 비어 있다(UFF_SaveSubsystem::MigrateMeta).

	   지우지 않고 남겨 두는 이유는, 지우면 옛 저장에서 이 값을 읽을 자리가 없어져
	   튜토리얼 이력이 통째로 사라지기 때문이다. `_DEPRECATED` 접미사는 UHT 가
	   떼고 맞춰주므로 파일에 적힌 이름은 예전 그대로다.
	*/
	UPROPERTY()
	bool bTutorialCompleted_DEPRECATED = false;

	UPROPERTY()
	bool bTutorialSkipped_DEPRECATED = false;
};

/* 축제 도중의 상태.

   ─── 지금은 읽기만 한다 ────────────────────────────────────────────────────
   각 파트의 공개 API 로 값을 긁어 담는다. 목록 화면에 "3일차 셋업" 을 보여주는
   데는 이걸로 충분하다.

   되돌리는 것은 아직 안 된다. 돈이나 장부를 직접 설정하는 API 가 없어서,
   지금 있는 수단으로 우겨넣으면 장부에 가짜 기록이 남는다. 이어하기는 각 파트가
   복원 수단을 열어준 다음의 일이다.

   그때가 오면 이 구조체에 파트별 항목이 붙는다. 지금 자리를 만들어 두는 이유는,
   저장 형식이 나중에 통째로 바뀌면 그 전에 만든 저장이 전부 무효가 되기 때문이다.
*/
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_SessionSave
{
	GENERATED_BODY()

	// 어느 축제를 하던 중인가. 비어 있으면 진행 중인 축제가 없다.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	FName FestivalId;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	int32 Day = 0;

	/* 어느 단계였는가.

	   열거형이 아니라 이름으로 두는 이유는 두 가지다. 단계 목록이 바뀌어도 옛
	   저장이 읽히기는 해야 하고, 열거형 값은 순서가 바뀌면 다른 단계를 가리킨다.
	*/
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	FName StageId;

	// 목록에 보여줄 값들.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	int32 Gold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	int32 TotalRevenue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	int32 Satisfaction = 0;

	bool IsValid() const { return !FestivalId.IsNone() && Day > 0; }
};

/* 목록에 보여줄 값만 모은 것.

   슬롯을 열지 않고도 "3일차 셋업, 어제 저장" 을 보여줄 수 있어야 한다.
   슬롯이 여덟 개면 목록을 그릴 때마다 파일을 여덟 번 열게 되는데,
   그 시간이 화면이 뜨는 순간에 몰린다.
*/
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_SaveSlotSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	FName SlotId;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	bool bAutoSave = false;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	FDateTime SavedAt = FDateTime::MinValue();

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	FString FestivalDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	FString RegionDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	int32 Day = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	FName StageId;

	bool IsValid() const { return !SlotId.IsNone(); }
};

/* 슬롯 목록만 담는 작은 파일.

   슬롯이 늘어도 이것 하나만 읽으면 목록이 나온다. 저장할 때 같이 갱신한다.

   목록과 실제 슬롯이 어긋날 수 있다. 사용자가 저장 폴더를 직접 건드리거나
   저장 도중에 꺼지면 그렇다. 그래서 목록은 참고로만 쓰고, 실제로 열 때는
   파일이 있는지 다시 확인한다.
*/
UCLASS()
class FESTAFIESTA_API UFF_SaveSlotIndex : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Version = 1;

	UPROPERTY()
	TArray<FFF_SaveSlotSummary> Slots;

	// 같은 이름이 있으면 갈아끼운다.
	void Upsert(const FFF_SaveSlotSummary& Summary)
	{
		for (FFF_SaveSlotSummary& Existing : Slots)
		{
			if (Existing.SlotId == Summary.SlotId)
			{
				Existing = Summary;
				return;
			}
		}
		Slots.Add(Summary);
	}

	void Remove(const FName SlotId)
	{
		Slots.RemoveAll([SlotId](const FFF_SaveSlotSummary& S)
		{
			return S.SlotId == SlotId;
		});
	}
};

/* 파트 하나가 담은 바이트.

   TMap 의 값으로 TArray<uint8> 를 바로 쓸 수 없어서 구조체로 감싼다.
*/
USTRUCT()
struct FESTAFIESTA_API FFF_SaveBlob
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<uint8> Bytes;
};

// 슬롯 하나에 담기는 전체.
UCLASS()
class FESTAFIESTA_API UFF_SaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/* 저장 형식의 판번호.

	   구조가 바뀌면 올린다. 올리지 않으면 옛 저장을 새 코드가 읽을 때
	   빠진 값이 기본값으로 들어가고, 그게 정상인지 오류인지 구분할 수 없다.

	   판 2: 튜토리얼 판정을 전역 bool 두 개에서 축제별 목록으로 바꿨다.
	         FFF_MetaSave 의 `판 1 에서만 쓰던 값` 항목을 볼 것.
	*/
	UPROPERTY()
	int32 Version = 2;

	UPROPERTY()
	FFF_MetaSave Meta;

	UPROPERTY()
	FFF_SessionSave Session;

	/* 각 파트가 담은 것. 키는 그쪽이 정한 이름이다.

	   저장 시스템은 무엇이 들었는지 모른다. 파트가 늘어도 여기는 안 바뀐다.
	   모르는 키가 들어 있으면 그냥 둔다. 그 파트가 아직 안 붙었거나
	   빠졌을 수 있는데, 지우면 되돌릴 수 없다.
	*/
	UPROPERTY()
	TMap<FName, FFF_SaveBlob> PartData;

	// 목록에 보여줄 값들. 슬롯을 열지 않고도 표시할 수 있어야 한다.
	UPROPERTY()
	FDateTime SavedAt = FDateTime::MinValue();

	UPROPERTY()
	FString FestivalDisplayName;

	UPROPERTY()
	FString RegionDisplayName;

	UPROPERTY()
	bool bAutoSave = false;
};
