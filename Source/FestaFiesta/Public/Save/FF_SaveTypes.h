// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Economy/Rules/FF_FestivalEvaluationRules.h"
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

	// 축제 평가 총점(0~100)과 그 등급. 같은 판의 최고 총점과 그때의 등급을 함께 남긴다.
	// 등급 D 가 기본값이라 "기록 없음"과 구분하려면 PlayCount 를 본다.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	int32 BestScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	EFF_FestivalEvaluationGrade BestGrade = EFF_FestivalEvaluationGrade::D;

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
		if (Other.BestScore > BestScore)
		{
			BestScore = Other.BestScore; BestGrade = Other.BestGrade; bChanged = true;
		}

		PlayCount += FMath::Max(1, Other.PlayCount);
		return bChanged;
	}
};

/* 축제 사이에 남는 것들. 어느 파트에도 속하지 않아서 저장 시스템이 직접 들고 있는다.

   ─── 전부 판 단위다 ────────────────────────────────────────────────────────
   튜토리얼 기록·깬 축제·최고 기록 모두 세이브 파일 안에 담긴다. 새 게임을 누르면
   전부 비우고, 이어하기를 하면 그 세이브에 담긴 값으로 돌아간다.

   계정 단위로 남기는 값은 없다. 기획이 "새 게임은 말 그대로 새 게임" 이라서다 —
   튜토리얼부터 축제 진행까지 처음부터 다시 한다. 예전에는 기록·깬 축제를
   FF_MetaStore 에 따로 두고 판과 상관없이 쌓았는데, 그러면 새 게임에 이전 판의
   진행이 따라온다. 그 파일은 이제 읽지도 쓰지도 않는다.
*/
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_MetaSave
{
	GENERATED_BODY()

	/* 로비 인트로를 끝까지 봤는가.

	   축제를 고르기 전에 로비에서 도는 오프닝이라 축제별로 나눌 수 없다.
	   새 게임마다 한 번 보여준다.
	*/
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	bool bLobbyIntroSeen = false;

	/* 안내를 통째로 껐는가.

	   건너뛰기는 "이번 것만 넘긴다" 가 아니라 "안내 없이 운영만 하겠다" 는 선언이다.
	   그래서 같은 판 안에서는 축제가 바뀌어도 되살아나지 않는다.
	   새 게임을 누르면 풀린다.

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

   ─── 목록 표시용 요약이다 ──────────────────────────────────────────────────
   각 파트의 공개 API 로 값을 긁어 담는다. 목록 화면에 "3일차 셋업" 을 보여주고,
   이어하기가 어느 레벨을 열지 정하는 데 쓴다.

   실제로 되돌리는 값(돈·시설·장부)은 여기가 아니라 각 파트가 IFF_Saveable 로 담은
   PartData 에 있다. 여기 Gold 를 고쳐도 게임 속 돈은 바뀌지 않는다.
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

	/* 로비에서 찍은 세이브인가. 축제와 축제 사이, 레벨 선택 화면에 있던 때다.

	   축제가 끝나면 판은 로비로 돌아가 다음 축제를 고른다. 축제가 늘어날수록 이
	   지점이 판의 진행(어디까지 깼는지)을 담는 자리가 된다. 이때는 축제 도중의
	   상태(돈·시설·날짜)가 없으므로 위 값들은 비어 있고, 파트 데이터도 담지 않는다.
	*/
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	bool bAtLobby = false;

	// 로비 세이브일 때 어느 나라의 레벨 선택 화면이었는가. 이어하면 그 화면으로 돌아간다.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Save")
	FName CountryId;

	// 축제 도중의 세이브인가. 이어하면 그 축제 레벨을 연다.
	bool IsFestivalInProgress() const { return !bAtLobby && !FestivalId.IsNone() && Day > 0; }

	// 이어할 수 있는 세이브인가. 축제 도중이거나 로비다.
	bool IsValid() const { return bAtLobby || IsFestivalInProgress(); }
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
	/* 지금 코드가 쓰는 판번호.

	   구조가 바뀌면 올린다. 올리지 않으면 옛 저장을 새 코드가 읽을 때
	   빠진 값이 기본값으로 들어가고, 그게 정상인지 오류인지 구분할 수 없다.

	   판 2: 튜토리얼 판정을 전역 bool 두 개에서 축제별 목록으로 바꿨다.
	         FFF_MetaSave 의 `판 1 에서만 쓰던 값` 항목을 볼 것.
	*/
	static constexpr int32 CurrentVersion = 2;

	/* 이 파일이 쓰인 판번호. 기본값은 일부러 0 이다.

	   SaveGame 은 CDO 기본값과 같은 값을 파일에 쓰지 않는다. 기본값을 지금 판으로
	   두면 판번호가 한 번도 파일에 안 들어가고, 읽을 때는 그때 코드의 기본값이
	   채워진다. 판 1 저장을 판 2 코드가 읽으면 2 로 보여서 옮기기가 건너뛰어졌다.

	   기본값을 0 으로 두고 저장할 때마다 CurrentVersion 을 직접 넣는다. 그러면
	   0 은 "판번호를 모르는 옛 파일" 이라는 뜻만 남는다. 이 수정 전에 쓴 파일은
	   판 1 이든 판 2 든 전부 0 으로 읽히므로, 옮기기는 판번호가 아니라 옛 값이
	   남아 있는지로 판단한다(UFF_SaveSubsystem::MigrateMeta).
	*/
	UPROPERTY()
	int32 Version = 0;

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
