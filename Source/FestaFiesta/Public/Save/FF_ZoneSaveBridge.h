// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"
#include "Save/FF_Saveable.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_ZoneSaveBridge.generated.h"

class UFFZSWorldSubsystem;

/* 시설 하나가 저장에 담기는 모양.

   액터 참조는 담지 않는다. 불러오면 그 액터는 이미 없다.
   인스턴스 아이디는 담는다. 스태프 배치·프로그램 편성이 이 아이디로 시설을 가리켜서,
   새 아이디로 다시 놓으면 그 연결이 전부 끊긴다.
*/
USTRUCT()
struct FFF_ZoneFacilitySave
{
	GENERATED_BODY()

	// 어떤 시설인가. 애셋 매니저로 정의를 찾을 때 쓴다.
	UPROPERTY()
	FName DefinitionId;

	UPROPERTY()
	FName ZoneId;

	// 격자 위치와 회전. 이 구조체는 플러그인이 SaveGame 플래그를 붙여 뒀다.
	UPROPERTY()
	FFFZSGridPlacement Placement;

	// 옛 저장이면 비어 있다. 그때는 새 아이디로 놓는다.
	UPROPERTY()
	FGuid InstanceId;

	// 돈을 내고 올린 강화 단계. 되돌릴 때 같은 단계까지 다시 밟는다(비용 없이).
	UPROPERTY()
	int32 UpgradeLevel = 1;
};

USTRUCT()
struct FFF_ZoneSaveState
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FFF_ZoneFacilitySave> Facilities;
};

/* Zone 의 시설 배치를 저장에 담고 되돌린다.

   ─── 왜 게임 쪽에 두는가 ───────────────────────────────────────────────────
   Zone 은 플러그인이라 다른 프로젝트에서도 쓸 수 있다. 이 게임의 저장 규약을
   플러그인에 넣으면 그 프로젝트들이 쓰지 않을 코드를 떠안는다.

   플러그인이 공개한 것만 써서 게임 쪽에서 붙이면 플러그인은 그대로 둘 수 있다.

   ─── 값을 직접 쓰지 않는다 ─────────────────────────────────────────────────
   시설을 되돌릴 때 액터를 직접 스폰하지 않고 설치 명령을 만들어 보낸다.
   플레이어가 시설을 놓을 때 타는 길과 같아서, 겹침 검사와 격자 정렬과
   Zone 안쪽 판정이 전부 그대로 돈다.

   값을 우겨넣으면 그 검사들을 건너뛰게 되고, 어긋난 배치가 조용히 들어간다.

   ─── 실패가 보인다 ─────────────────────────────────────────────────────────
   명령이 검증에 걸리면 설치가 실패한다. 값을 쓰는 방식이었으면 무조건 들어가서
   나중에야 이상한 걸 발견하는데, 명령은 그 자리에서 알려준다.
*/
UCLASS()
class FESTAFIESTA_API UFF_ZoneSaveBridge : public UWorldSubsystem, public IFF_Saveable
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

	// ── 저장 참여 ───────────────────────────────────────────────────────────

	virtual FName GetSaveKey() const override { return TEXT("ZoneFacilities"); }

	/* 시설은 월드에 놓이는 것이라 World 단계다.

	   돈이 복원된 뒤에 와야 한다. 설치 명령이 비용을 치르는데, 그 전이면
	   잔액이 모자라 실패한다.
	*/
	virtual EFF_SaveLoadPhase GetSaveLoadPhase() const override
	{
		return EFF_SaveLoadPhase::World;
	}

	virtual void WriteSaveData(TArray<uint8>& OutBytes) const override;
	virtual void ReadSaveData(const TArray<uint8>& InBytes) override;

private:
	// 지금 레벨의 시설을 훑어 담는다.
	void CollectFacilities(FFF_ZoneSaveState& Out) const;

	// 담긴 시설을 설치 명령으로 되돌린다. 성공한 수를 돌려준다.
	int32 RestoreFacilities(const FFF_ZoneSaveState& In);

	// 이미 놓인 시설을 치운다. 복원 전에 부른다.
	void ClearExistingFacilities();

	// 갓 설치한 시설을 저장된 강화 단계까지 한 단계씩 올린다.
	static void ReapplyUpgrades(UFFZSWorldSubsystem& ZoneWorld, const FGuid& InstanceId, int32 TargetLevel);

	bool bRegistered = false;
};
