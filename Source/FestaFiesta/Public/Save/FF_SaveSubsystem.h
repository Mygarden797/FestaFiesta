// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Save/FF_SaveTypes.h"
#include "Save/FF_Saveable.h"
#include "UObject/WeakInterfacePtr.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FF_SaveSubsystem.generated.h"

class IConsoleCommand;

// 저장이 끝났다. 목록 화면이 이걸 받아 다시 그린다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnSaveCompleted, FName, SlotId);

// 불러오기가 끝났다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnSaveLoaded, FName, SlotId);

/* 저장과 불러오기.

   ─── 왜 GameInstance 인가 ───────────────────────────────────────────────────
   레벨을 옮겨도 살아 있어야 한다. 로비에서 저장을 읽고 축제 레벨로 넘어가는데,
   월드 서브시스템이면 그 사이에 통째로 사라진다.

   ─── 무엇을 담는가 ─────────────────────────────────────────────────────────
   메타는 여기서 직접 들고 있는다. 어느 파트에도 속하지 않는 값이라서다.
   축제 도중 상태는 각 파트가 채워야 해서 자리만 비워 뒀다.

   ─── 자동 저장 ─────────────────────────────────────────────────────────────
   하루가 끝날 때와 튜토리얼이 끝날 때 자동으로 부른다. 잃어도 하루치다.
   매 단계마다 저장하면 잦고, 수동만 두면 사람이 잊는다.
*/
UCLASS()
class FESTAFIESTA_API UFF_SaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static UFF_SaveSubsystem* Get(const UObject* WorldContextObject);

	// ── 저장·불러오기 ───────────────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Save")
	bool SaveToSlot(FName SlotId, bool bAutoSave = false);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Save")
	bool LoadFromSlot(FName SlotId);

	/* 이 축제의 저장이 있으면 이어서 시작한다.

	   축제 레벨이 열리고 모든 시스템이 준비된 뒤에 부른다. 서브시스템 Initialize
	   에서 부르면 안 되는데, 그 시점에는 다른 시스템이 아직 자기 초기화를 하는
	   중이라 복원한 값을 곧바로 덮어쓴다.

	   저장된 축제와 지금 축제가 다르면 아무것도 하지 않는다. 다대포 저장으로
	   실습장을 시작하면 엉뚱한 상태가 된다.
	*/
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Save")
	bool TryContinueFestival(FName FestivalId);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Save")
	bool DeleteSlot(FName SlotId);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Save")
	bool DoesSlotExist(FName SlotId) const;

	// 저장된 슬롯 이름들. 목록 화면이 쓴다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Save")
	TArray<FName> GetKnownSlotIds() const;

	// 슬롯을 열지 않고 표시 정보만 읽는다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Save")
	bool PeekSlot(FName SlotId, UFF_SaveGame*& OutSave) const;

	// ── 파트 참여 ───────────────────────────────────────────────────────────

	/* 저장에 참여할 시스템을 등록한다.

	   자기 Initialize 에서 한 번 부르면 된다. 언제 저장되는지, 파일이 어디
	   쓰이는지는 몰라도 된다.

	   약참조로 들기 때문에 시스템이 사라지면 목록에서 알아서 빠진다.
	*/
	void RegisterSaveable(TScriptInterface<IFF_Saveable> Saveable);

	void UnregisterSaveable(TScriptInterface<IFF_Saveable> Saveable);

	// 지금 등록된 파트 수. 진단용.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Save")
	int32 GetRegisteredPartCount() const { return Saveables.Num(); }

	// ── 메타 조회·기록 ──────────────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Save")
	bool WasTutorialSeen() const
	{
		return Meta.bTutorialCompleted || Meta.bTutorialSkipped;
	}

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Save")
	void MarkTutorialCompleted(bool bSkipped);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Save")
	void MarkFestivalCleared(FName FestivalId, const FFF_FestivalRecord& Record);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Save")
	bool HasClearedFestival(FName FestivalId) const { return Meta.HasCleared(FestivalId); }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Save")
	FFF_FestivalRecord GetRecord(FName FestivalId) const;

	// 지금 메모리에 있는 메타. 저장 전 상태도 포함한다.
	const FFF_MetaSave& GetMeta() const { return Meta; }

	// ── 신호 ────────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Save")
	FFF_OnSaveCompleted OnSaveCompleted;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Save")
	FFF_OnSaveLoaded OnSaveLoaded;

	// 자동 저장이 쓸 슬롯 이름.
	static FName GetAutoSaveSlotId() { return TEXT("AutoSave"); }

	/* 지금 진행 중인 축제 상태를 읽어 담는다.

	   각 파트의 공개 조회 API 만 쓴다. 남의 파일을 고치지 않는 대신,
	   그쪽이 공개하지 않은 값은 가져올 수 없다.
	*/
	void CaptureSession(FFF_SessionSave& Out) const;

private:
	// 슬롯 이름을 실제 파일 이름으로. 공백이나 기호가 섞이면 파일이 안 만들어진다.
	static FString MakeSlotName(FName SlotId);

	void RegisterDebugConsoleCommands();
	void UnregisterDebugConsoleCommands();

	/* 지금 진행 상태. 저장하면 이 값이 파일로 나간다.

	   파일을 매번 읽지 않고 메모리에 들고 있는 이유는, 튜토리얼을 봤는지 같은
	   질문이 화면이 바뀔 때마다 들어오기 때문이다. 그때마다 디스크를 읽으면
	   느리고, 읽는 도중에 값이 갈릴 수도 있다.
	*/
	FFF_MetaSave Meta;

	// 마지막으로 쓴 슬롯. 이어하기 기본값으로 쓴다.
	FName LastUsedSlotId;

	/* 저장에 참여하는 시스템들.

	   약참조로 든다. 월드가 바뀌며 사라지는 서브시스템이 섞일 수 있는데,
	   강참조로 들면 그것들이 안 지워진다.
	*/
	TArray<TWeakInterfacePtr<IFF_Saveable>> Saveables;

	// 등록된 파트에서 데이터를 걷는다.
	void CollectPartData(TMap<FName, FFF_SaveBlob>& Out) const;

	// 단계 순서대로 되돌린다.
	void RestorePartData(const TMap<FName, FFF_SaveBlob>& In);

	// 목록 파일을 읽고 쓴다.
	UFF_SaveSlotIndex* LoadSlotIndex() const;
	void UpdateSlotIndex(const UFF_SaveGame& Save, FName SlotId);
	void RemoveFromSlotIndex(FName SlotId);

	// 자동 저장이 번갈아 쓸 슬롯을 고른다.
	FName PickAutoSaveSlot() const;

	// 다음 자동 저장에 쓸 차례. 둘을 번갈아 쓴다.
	bool bAutoSaveUsesSecondSlot = false;

	IConsoleCommand* SaveConsoleCommand = nullptr;
	IConsoleCommand* LoadConsoleCommand = nullptr;
	IConsoleCommand* StatConsoleCommand = nullptr;
};
