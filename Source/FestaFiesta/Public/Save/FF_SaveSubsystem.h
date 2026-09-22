// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Save/FF_SaveTypes.h"
#include "Save/FF_Saveable.h"
#include "Time/FF_FestivalStage.h"
#include "UObject/WeakInterfacePtr.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FF_SaveSubsystem.generated.h"

struct IConsoleCommand;

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

	/* 이어할 슬롯을 지정한다. 레벨을 열기 전에 프론트엔드가 부른다.

	   지정하지 않으면 가장 최근 자동 저장으로 이어한다. 목록에서 옛 기록이나 수동
	   저장을 고른 경우에는 그 슬롯이어야 하는데, 레벨을 열면 화면이 사라져서
	   선택을 넘겨줄 자리가 없다. 그래서 여기 맡겨둔다.

	   한 번 쓰면 비운다. 남겨두면 다음에 새 게임으로 들어와도 그 슬롯을 물고 들어간다.
	*/
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Save")
	void RequestContinueFromSlot(FName SlotId);

	// 지정된 슬롯의 축제. 프론트엔드가 어느 레벨을 열지 정할 때 쓴다. 없으면 None.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Save")
	FName PeekFestivalIdInSlot(FName SlotId) const;

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

	/* 로비 인트로를 이미 봤는가.

	   안내를 끈 사람도 다시 보지 않는다. 끈다는 것은 "대사도 안내도 그만" 이라는 뜻이라
	   오프닝만 예외로 두면 앞뒤가 안 맞는다.
	*/
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Save")
	bool WasLobbyIntroSeen() const
	{
		return Meta.bLobbyIntroSeen || Meta.bTutorialOptedOut;
	}

	/* 이 축제의 튜토리얼을 이미 봤는가.

	   축제별로 묻는 이유는 축제마다 배울 것이 다르기 때문이다. 전역 bool 하나면
	   첫 축제를 끝낸 사람이 다음 축제의 안내를 영영 못 본다.

	   [주의] 안내를 껐는지는 여기서 보지 않는다. 그건 IsTutorialOptedOut 이 답한다.
	          두 질문을 하나로 합치면 "안 본 축제인데 왜 안 나오지" 를 로그로 못 가른다.

	   FestivalId 가 비어 있으면 축제를 식별하지 못했다는 뜻이다. 그때는 안 본 것으로
	   본다 — 잘못 막아 안내를 못 보는 쪽이, 한 번 더 보는 쪽보다 나쁘다.
	*/
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Save")
	bool WasFestivalTutorialSeen(FName FestivalId) const
	{
		return Meta.HasSeenTutorialFor(FestivalId);
	}

	// 안내를 통째로 껐는가. 축제가 바뀌어도 유지된다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Save")
	bool IsTutorialOptedOut() const { return Meta.bTutorialOptedOut; }

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Save")
	void MarkLobbyIntroSeen();

	// 이 축제의 튜토리얼을 끝까지 봤다고 남긴다. FestivalId 가 비면 아무것도 하지 않는다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Save")
	void MarkFestivalTutorialSeen(FName FestivalId);

	// 안내를 통째로 끈다. 되돌리는 수단은 지금 없다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Save")
	void MarkTutorialOptedOut();

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
	/* 하루 경계와 축제 종료를 듣는다. 자동 저장이 걸리는 자리다.

	   ─── 왜 저장 쪽이 구독하는가 ────────────────────────────────────────────
	   각 파트가 "여기서 저장해줘" 를 부르게 하면, 파트마다 저장 시점을 알아야 하고
	   하나가 늘 때마다 그 판단이 한 벌씩 는다. RegisterSaveable 이 "언제 저장되는지
	   몰라도 된다" 고 약속한 것과도 어긋난다. 신호는 이미 공개돼 있으니 여기서 듣는다.
	*/
	void BindFestivalStage(FSubsystemCollectionBase& Collection);
	void UnbindFestivalStage();

	UFUNCTION()
	void HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage);

	UFUNCTION()
	void HandleFestivalEnded();

	/* 다음 틱에 자동 저장한다. 바로 담지 않는다.

	   하루 경계는 OnStageChanged 로 굴러가고 Zone·Crowd·경제가 모두 같은 신호를
	   듣는다. 델리게이트 호출 순서는 등록 순서라 보장이 없어서, 그 자리에서 담으면
	   아직 새 날 처리를 안 한 파트의 바이트가 섞인다 — 경제만 새 날이고 나머지는
	   어제인 저장이 나온다.

	   한 프레임 미루면 누가 먼저 등록했든 전부 끝난 뒤가 된다.
	*/
	void RequestDeferredAutoSave(const TCHAR* Reason);

	// 슬롯 이름을 실제 파일 이름으로. 공백이나 기호가 섞이면 파일이 안 만들어진다.
	static FString MakeSlotName(FName SlotId);

	/* 옛 판 저장을 지금 형식으로 옮긴다. 읽어온 직후에 한 번 부른다.

	   읽는 자리가 둘(Initialize 의 자동 저장 선택, LoadFromSlot)이라 함수로 뺐다.
	   한쪽에만 넣으면 그쪽으로 안 들어온 저장이 조용히 옛 형식으로 남는다.
	*/
	static void MigrateMeta(FFF_MetaSave& InOutMeta, int32 SavedVersion);

	/* 자동 저장 슬롯에 지금 상태를 쓰고 다음 차례를 넘긴다.

	   메타를 건드리는 곳마다 같은 두 줄이 반복돼서 묶었다. 한쪽에서 차례 넘기기를
	   빠뜨리면 한 슬롯만 계속 덮여 이중화가 무의미해진다.
	*/
	void WriteAutoSave();

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

	// 프론트엔드가 고른 이어하기 슬롯. 한 번 쓰면 비운다.
	FName PendingContinueSlotId;

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

	/* 자동 저장을 마지막으로 건 날. 같은 날에 두 번 걸지 않는다.

	   하루의 시작으로 보는 단계가 둘(1일차 셋업, 2·3일차 모닝체크)이라 하루에
	   두 번 걸릴 수 있다. 날짜로 막는 편이 단계 목록이 바뀌어도 견딘다.
	*/
	int32 LastAutoSavedDay = 0;

	// 단계 신호에 물려 있는지. 해제할 때 중복 호출을 막는다.
	bool bStageBound = false;

	IConsoleCommand* SaveConsoleCommand = nullptr;
	IConsoleCommand* LoadConsoleCommand = nullptr;
	IConsoleCommand* StatConsoleCommand = nullptr;
};
