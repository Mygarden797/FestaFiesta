// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FF_Saveable.generated.h"

/* 저장에 참여할 단계.

   복원 순서가 어긋나면 크래시가 아니라 값이 조금 틀린 채로 돌아간다.
   시설이 복원되기 전에 대기열을 복원하면 붙을 자리가 없고, 돈이 복원되기 전에
   시설을 놓으면 잔액이 모자라 실패한다.

   주석으로만 적어두면 반드시 어긋나므로 단계를 값으로 받는다.
*/
UENUM(BlueprintType)
enum class EFF_SaveLoadPhase : uint8
{
	// 돈, 날짜, 진행도처럼 다른 것이 기대는 값.
	Global      UMETA(DisplayName = "전역"),

	// 시설, 지형처럼 월드에 놓이는 것.
	World       UMETA(DisplayName = "월드"),

	// 대기열, NPC 처럼 월드 위에 붙는 것.
	Attached    UMETA(DisplayName = "부착"),

	// UI, 카메라처럼 보여주기만 하는 것.
	Presentation UMETA(DisplayName = "표시")
};

UINTERFACE(MinimalAPI, BlueprintType)
class UFF_Saveable : public UInterface
{
	GENERATED_BODY()
};

/* 저장에 참여하는 시스템이 구현한다.

   ─── 붙이는 법 ─────────────────────────────────────────────────────────────
   1. 자기 서브시스템에 이 인터페이스를 단다.
   2. WriteSaveData / ReadSaveData 를 구현한다.
   3. Initialize 에서 UFF_SaveSubsystem::RegisterSaveable(this, 단계) 를 부른다.

   그게 전부다. 언제 저장되는지, 파일이 어디 쓰이는지는 몰라도 된다.

   ─── 왜 바이트 배열인가 ────────────────────────────────────────────────────
   저장 시스템이 각 파트의 구조체를 알아야 하면, 파트가 늘 때마다 저장 쪽을
   고쳐야 한다. 그러면 결국 한 사람이 전부 들고 있게 된다.

   바이트로 주고받으면 저장은 담기만 하고, 무엇이 들었는지는 그 파트만 안다.
   구조가 바뀌어도 저장 시스템은 그대로다.

   ─── 무엇을 담나 ───────────────────────────────────────────────────────────
   되돌릴 수 없는 것만 담는다. 다시 계산할 수 있는 것은 담지 않는 편이 낫다.
   담은 것이 늘수록 복원 순서가 얽히고, 어긋나면 찾기 어렵다.
*/
class FESTAFIESTA_API IFF_Saveable
{
	GENERATED_BODY()

public:
	/* 자기 상태를 바이트로 쓴다.

	   FMemoryWriter 와 FObjectAndNameAsStringProxyArchive 를 쓰면
	   USTRUCT 하나를 통째로 넣을 수 있다. 예시는 헤더 아래 주석에 있다.
	*/
	virtual void WriteSaveData(TArray<uint8>& OutBytes) const = 0;

	/* 바이트에서 자기 상태를 되돌린다.

	   빈 배열이 올 수 있다. 저장 당시에 이 시스템이 없었거나, 옛 저장이라서다.
	   그때는 아무것도 하지 않고 기본 상태로 둔다.
	*/
	virtual void ReadSaveData(const TArray<uint8>& InBytes) = 0;

	/* 저장 파일에서 이 시스템을 부를 이름.

	   클래스 이름을 그대로 쓰면 클래스명을 바꿀 때 옛 저장을 못 읽는다.
	   직접 정한 이름을 돌려주는 편이 낫다.
	*/
	virtual FName GetSaveKey() const = 0;

	// 어느 단계에서 복원할지.
	virtual EFF_SaveLoadPhase GetSaveLoadPhase() const
	{
		return EFF_SaveLoadPhase::Global;
	}
};

/* 구현 예시

	// 담을 것을 구조체로 정의한다. UPROPERTY 를 붙여야 저장된다.
	USTRUCT()
	struct FMyState
	{
		GENERATED_BODY()

		UPROPERTY() int32 Gold = 0;
		UPROPERTY() TArray<FName> Owned;
	};

	// 서브시스템에 인터페이스를 단다.
	UCLASS()
	class UMySubsystem : public UGameInstanceSubsystem, public IFF_Saveable
	{
		GENERATED_BODY()

	public:
		virtual void Initialize(FSubsystemCollectionBase& Collection) override
		{
			Super::Initialize(Collection);

			if (UFF_SaveSubsystem* Save = GetGameInstance()->GetSubsystem<UFF_SaveSubsystem>())
			{
				Save->RegisterSaveable(this);
			}
		}

		virtual FName GetSaveKey() const override { return TEXT("MySystem"); }

		virtual EFF_SaveLoadPhase GetSaveLoadPhase() const override
		{
			return EFF_SaveLoadPhase::Global;
		}

		virtual void WriteSaveData(TArray<uint8>& OutBytes) const override
		{
			FMyState State;
			State.Gold = Gold;
			State.Owned = Owned;

			UFF_SaveArchive::WriteStruct(OutBytes, State);
		}

		virtual void ReadSaveData(const TArray<uint8>& InBytes) override
		{
			FMyState State;
			if (!UFF_SaveArchive::ReadStruct(InBytes, State))
			{
				return;   // 저장이 없거나 읽지 못했다. 기본 상태로 둔다.
			}

			Gold = State.Gold;
			Owned = State.Owned;
		}
	};
*/
