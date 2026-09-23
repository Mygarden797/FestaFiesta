// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/FF_SaveSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "NPC/Metrics/FF_SatisfactionMetricsSubsystem.h"
#include "Time/FF_FestivalStage.h"
#include "Save/FF_SaveArchive.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/Core/FF_UIFlowSubsystem.h"

DEFINE_LOG_CATEGORY(LogFestaFiestaSave);

namespace
{
	// 슬롯은 한 명이 여러 개 쓸 수 있지만 사용자 인덱스는 지금 하나뿐이다.
	constexpr int32 SaveUserIndex = 0;

	// 목록을 만들 때 훑어볼 슬롯 수.
	//
	// 언리얼은 슬롯을 나열하는 API 를 주지 않는다. 이름을 알아야 열 수 있어서,
	// 정해둔 규칙으로 만든 이름을 차례로 두드려 본다.
	constexpr int32 MaxManualSlots = 8;

	// 자동 저장 두 칸. 번갈아 쓴다(PickAutoSaveSlot).
	const FName AutoSaveSlotA(TEXT("AutoSave_A"));
	const FName AutoSaveSlotB(TEXT("AutoSave_B"));

	/* 메타만 담는 파일. 슬롯 목록에 올리지 않는다.

	   MakeSlotName 을 거치지 않고 이름을 따로 둔다. 거치면 콘솔로 `Meta` 라는 수동
	   슬롯을 만들었을 때 같은 파일을 가리킨다.
	*/
	const TCHAR* const MetaStoreSlotName = TEXT("FF_MetaStore");
}

void UFF_SaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	/* 시작할 때 메타를 채운다. 읽지 않으면 튜토리얼을 봤는지 알 수 없어
	   게임을 켤 때마다 다시 나온다.

	   메타 파일을 먼저 읽고, 가장 최근 자동 저장의 메타를 합친다. 메타 파일이
	   생기기 전에 만든 저장은 메타를 자동 저장 안에만 들고 있어서, 합치지 않으면
	   그동안의 이력이 사라진다. 합치기는 OR·합집합이라 두 번 해도 같다.

	   아무것도 없으면 기본값 그대로 둔다. 첫 실행이 정상이다.
	*/
	FFF_MetaSave StoredMeta;
	int32 StoredVersion = 0;
	if (ReadMetaStore(StoredMeta, StoredVersion))
	{
		// 메모리가 비어 있으니 합칠 것 없이 그대로 받는다. 합치면 같은 내용을 다시 쓴다.
		MigrateMeta(StoredMeta, StoredVersion);
		Meta = StoredMeta;
	}

	UFF_SaveGame* Newest = nullptr;
	FName NewestSlot;
	if (FindNewestAutoSave(Newest, NewestSlot))
	{
		AdoptMeta(Newest->Meta, Newest->Version, NewestSlot.ToString());
		LastUsedSlotId = NewestSlot;

		// 다음 자동 저장은 방금 읽은 쪽 반대편에 쓴다. 최근 것을 덮지 않는다.
		bAutoSaveUsesSecondSlot = (NewestSlot == AutoSaveSlotA);
	}

	UE_LOG(LogFestaFiestaSave, Log,
		TEXT("[저장] 메타 준비. 로비 인트로=%s 안내 끔=%s 튜토리얼 본 축제 %d개 / 깬 축제 %d개"),
		Meta.bLobbyIntroSeen ? TEXT("봤음") : TEXT("안 봤음"),
		Meta.bTutorialOptedOut ? TEXT("예") : TEXT("아니오"),
		Meta.TutorialSeenFestivals.Num(),
		Meta.ClearedFestivals.Num());

	BindFestivalStage(Collection);
	RegisterDebugConsoleCommands();
}

void UFF_SaveSubsystem::Deinitialize()
{
	UnbindFestivalStage();
	UnregisterDebugConsoleCommands();
	Super::Deinitialize();
}

void UFF_SaveSubsystem::BindFestivalStage(FSubsystemCollectionBase& Collection)
{
	if (bStageBound)
	{
		return;
	}

	/* 의존을 선언해 초기화를 보장받는다.

	   그냥 GetSubsystem 으로 꺼내면 아직 안 만들어졌을 수 있다. 그때는 조용히
	   구독에 실패하고, 증상은 "하루가 지나도 저장이 안 된다" 로만 나타난다.
	*/
	UFF_FestivalStageSubsystem* Stage =
		Collection.InitializeDependency<UFF_FestivalStageSubsystem>();

	if (!Stage)
	{
		UE_LOG(LogFestaFiestaSave, Warning,
			TEXT("[저장] 단계 시스템이 없어 자동 저장을 걸지 못한다. "
				 "콘솔(FF.Save.Write)로만 저장된다."));
		return;
	}

	Stage->OnStageChanged.AddDynamic(this, &UFF_SaveSubsystem::HandleStageChanged);
	Stage->OnFestivalEnded.AddDynamic(this, &UFF_SaveSubsystem::HandleFestivalEnded);
	Stage->OnFestivalStarted.AddDynamic(this, &UFF_SaveSubsystem::HandleFestivalStarted);
	bStageBound = true;
}

void UFF_SaveSubsystem::UnbindFestivalStage()
{
	if (!bStageBound)
	{
		return;
	}

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFF_FestivalStageSubsystem* Stage =
			GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>())
		{
			Stage->OnStageChanged.RemoveDynamic(this, &UFF_SaveSubsystem::HandleStageChanged);
			Stage->OnFestivalEnded.RemoveDynamic(this, &UFF_SaveSubsystem::HandleFestivalEnded);
			Stage->OnFestivalStarted.RemoveDynamic(this, &UFF_SaveSubsystem::HandleFestivalStarted);
		}
	}

	bStageBound = false;
}

void UFF_SaveSubsystem::HandleFestivalStarted()
{
	/* 날짜 기준을 비운다.

	   안 비우면 1일차에 로비로 나갔다가 다른 축제를 새로 시작했을 때, 그 판의
	   1일차가 "이미 저장한 날" 로 보여 첫 자동 저장이 빠진다.
	*/
	LastAutoSavedDay = 0;
}

void UFF_SaveSubsystem::HandleStageChanged(
	const EFF_FestivalStage PreviousStage, const EFF_FestivalStage NewStage)
{
	(void)PreviousStage;

	/* 하루가 시작되는 순간에만 담는다.

	   1일차는 셋업으로 시작하고 2·3일차는 모닝체크로 들어온다. 하루가 끝나는
	   쪽(마감·리포트)에 걸지 않는 이유는, 그 시점에는 경제의 하루 정산이 아직
	   안 돌아서 장부가 어중간하기 때문이다. 정산까지 끝난 다음 날 아침이
	   "여기서 이어하면 되는" 지점이다.

	   잃어봐야 하루치다. 매 단계마다 담으면 잦고, 수동만 두면 사람이 잊는다.
	*/
	const bool bDayStart =
		NewStage == EFF_FestivalStage::Setup || NewStage == EFF_FestivalStage::MorningCheck;

	if (!bDayStart)
	{
		return;
	}

	/* 이어하기를 기다리는 중이면 담지 않는다.

	   레벨이 열리면 UIFlow 가 새 판(1일차 셋업)을 먼저 세우고, 그다음에 GameMode 가
	   저장을 되돌린다. 그 사이에 1일차를 담으면 자동 저장 칸 하나를 초기 상태로
	   덮는데, 그게 지금 불러오려는 칸일 수 있다.
	*/
	if (HasPendingContinue())
	{
		UE_LOG(LogFestaFiestaSave, Verbose,
			TEXT("[저장] 이어하기 대기 중이라 하루 시작 저장을 건너뛴다."));
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UFF_FestivalStageSubsystem* Stage =
		GameInstance ? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;

	const int32 Day = Stage ? Stage->CurrentDay : 0;

	if (Day <= 0 || Day == LastAutoSavedDay)
	{
		return;
	}

	LastAutoSavedDay = Day;
	RequestDeferredAutoSave(TEXT("하루 시작"));
}

void UFF_SaveSubsystem::HandleFestivalEnded()
{
	const UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	const UFF_FestivalProgressionSubsystem* Progression =
		GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>();

	if (!Progression || !Progression->IsFestivalStarted())
	{
		return;
	}

	// 기록은 프론트엔드가 고른 축제 이름으로 남긴다. ResolveCurrentFestivalId 주석 참고.
	const FName FestivalId = ResolveCurrentFestivalId(this);
	if (FestivalId.IsNone())
	{
		UE_LOG(LogFestaFiestaSave, Warning,
			TEXT("[저장] 축제를 식별하지 못해 기록을 남기지 않는다."));
		return;
	}

	FFF_FestivalRecord Record;

	/* 남긴 돈으로 성적을 잰다. 누적 매출이 아니라 초기 자금과의 차이다.

	   매출만 보면 돈을 아무리 써도 숫자가 올라가서, 많이 벌고 많이 쓴 판과
	   적게 벌고 아낀 판이 구분되지 않는다.

	   초기 자금은 Progression 의 Definition 에서 읽는다. 이름과 달리 이건 실제
	   경제가 쓰는 값이라, 돈이 그 기준으로 시작했으니 차이도 그 기준으로 잰다.
	*/
	const UFF_FestivalDefinition* Definition = Progression->GetDefinition();
	Record.BestProfit = Progression->GetGold() - (Definition ? Definition->InitialGold : 0);

	if (const UFF_SatisfactionMetricsSubsystem* Metrics =
		GameInstance->GetSubsystem<UFF_SatisfactionMetricsSubsystem>())
	{
		Record.BestSatisfaction = static_cast<float>(Metrics->GetSimSatisfaction());
	}

	/* 방문객 수 — 경제 쪽에 누적 방문객 getter 가 생겨서 채운다(2026-09-22).

	   Crowd 가 아니라 경제 쪽 API 다 — 입장료 일일 목표 보너스를 만들면서 이미
	   `OnVisitorsAdmitted` 를 하루 단위로 누적하고 있었고, 그 옆에 하루 경계로
	   안 지워지는 축제 전체 누적 칸을 하나 더 두는 것으로 해결됐다.
	*/
	Record.BestVisitors = Progression->GetTotalAdmittedVisitors();

	Record.PlayCount = 1;

	// 기록을 메타에 먼저 쓴 다음 판을 버린다. 순서가 반대면 그 사이에 꺼졌을 때 둘 다 잃는다.
	MarkFestivalCleared(FestivalId, Record);
	DiscardFinishedRunAutoSaves(FestivalId);
}

void UFF_SaveSubsystem::DiscardFinishedRunAutoSaves(const FName FestivalId)
{
	for (const FName& SlotId : { AutoSaveSlotA, AutoSaveSlotB })
	{
		UFF_SaveGame* Found = nullptr;
		if (!PeekSlot(SlotId, Found) || !Found)
		{
			continue;
		}

		// 다른 축제의 판이면 둔다. 그 축제는 아직 안 끝났다.
		if (Found->Session.FestivalId != FestivalId)
		{
			continue;
		}

		DeleteSlot(SlotId);
	}

	LastAutoSavedDay = 0;
}

void UFF_SaveSubsystem::RequestDeferredAutoSave(const TCHAR* Reason)
{
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	const FString ReasonText = Reason;

	GameInstance->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this, ReasonText]()
		{
			// 건 뒤 한 틱 사이에 이어하기가 지정됐을 수 있다. HandleStageChanged 와 같은 이유로 막는다.
			if (HasPendingContinue())
			{
				return;
			}

			UE_LOG(LogFestaFiestaSave, Log, TEXT("[저장] 자동 저장 — %s"), *ReasonText);
			WriteAutoSave();
		}));
}

UFF_SaveSubsystem* UFF_SaveSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UFF_SaveSubsystem>() : nullptr;
}

FName UFF_SaveSubsystem::ResolveCurrentFestivalId(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	if (!World)
	{
		return NAME_None;
	}

	if (const UGameInstance* GameInstance = World->GetGameInstance())
	{
		if (const UFF_UIFlowSubsystem* Flow = GameInstance->GetSubsystem<UFF_UIFlowSubsystem>())
		{
			const FName ActiveId = Flow->GetActiveFestivalId();
			if (!ActiveId.IsNone())
			{
				return ActiveId;
			}
		}
	}

	/* 흐름을 안 타고 레벨을 직접 열었다. 레벨 이름으로 대신한다.

	   PIE 는 맵 이름 앞에 UEDPIE_0_ 같은 접두사를 붙인다. 떼지 않으면 에디터에서
	   본 것과 패키지에서 본 것이 다른 축제로 잡힌다.
	*/
	FString MapName = World->GetMapName();
	MapName.RemoveFromStart(World->StreamingLevelsPrefix);

	return MapName.IsEmpty() ? NAME_None : FName(*MapName);
}

FString UFF_SaveSubsystem::MakeSlotName(const FName SlotId)
{
	if (SlotId.IsNone())
	{
		return TEXT("FF_AutoSave");
	}

	// 파일 이름이 되므로 안전한 문자만 남긴다.
	FString Clean = SlotId.ToString();
	Clean = Clean.Replace(TEXT(" "), TEXT("_"));

	return FString::Printf(TEXT("FF_%s"), *Clean);
}

bool UFF_SaveSubsystem::SaveToSlot(const FName SlotId, const bool bAutoSave)
{
	UFF_SaveGame* Save = Cast<UFF_SaveGame>(
		UGameplayStatics::CreateSaveGameObject(UFF_SaveGame::StaticClass()));

	if (!Save)
	{
		UE_LOG(LogFestaFiestaSave, Error, TEXT("[저장] 저장 객체를 만들지 못했다."));
		return false;
	}

	// 판번호는 직접 넣는다. 기본값에 맡기면 파일에 안 들어간다(UFF_SaveGame::Version 주석).
	Save->Version = UFF_SaveGame::CurrentVersion;

	// 메타의 원본은 메타 파일이다. 여기 담는 것은 메타 파일이 깨졌을 때 쓸 사본이다.
	Save->Meta = Meta;
	Save->SavedAt = FDateTime::UtcNow();
	Save->bAutoSave = bAutoSave;

	CaptureSession(Save->Session);
	CollectPartData(Save->PartData);

	const FString SlotName = MakeSlotName(SlotId);
	if (!UGameplayStatics::SaveGameToSlot(Save, SlotName, SaveUserIndex))
	{
		UE_LOG(LogFestaFiestaSave, Error, TEXT("[저장] %s 쓰기 실패."), *SlotName);
		return false;
	}

	LastUsedSlotId = SlotId;
	UpdateSlotIndex(*Save, SlotId);

	UE_LOG(LogFestaFiestaSave, Log,
		TEXT("[저장] %s 에 저장했다. (자동=%s, 파트 %d개)"),
		*SlotName, bAutoSave ? TEXT("예") : TEXT("아니오"), Save->PartData.Num());

	OnSaveCompleted.Broadcast(SlotId);
	return true;
}

namespace
{
	const TCHAR* const SlotIndexName = TEXT("FF_SlotIndex");
}

FName UFF_SaveSubsystem::PickAutoSaveSlot() const
{
	/* 자동 저장 슬롯 둘을 번갈아 쓴다.

	   하나만 계속 덮으면 저장 도중에 꺼졌을 때 전부 잃는다. 번갈아 쓰면
	   하나가 깨져도 직전 것이 남는다.
	*/
	return bAutoSaveUsesSecondSlot ? FName(TEXT("AutoSave_B")) : FName(TEXT("AutoSave_A"));
}

UFF_SaveSlotIndex* UFF_SaveSubsystem::LoadSlotIndex() const
{
	if (UGameplayStatics::DoesSaveGameExist(SlotIndexName, SaveUserIndex))
	{
		if (UFF_SaveSlotIndex* Loaded = Cast<UFF_SaveSlotIndex>(
			UGameplayStatics::LoadGameFromSlot(SlotIndexName, SaveUserIndex)))
		{
			return Loaded;
		}
	}

	// 없거나 못 읽으면 새로 만든다. 목록은 다시 만들 수 있는 것이라 버려도 된다.
	return Cast<UFF_SaveSlotIndex>(
		UGameplayStatics::CreateSaveGameObject(UFF_SaveSlotIndex::StaticClass()));
}

void UFF_SaveSubsystem::UpdateSlotIndex(const UFF_SaveGame& Save, const FName SlotId)
{
	UFF_SaveSlotIndex* Index = LoadSlotIndex();
	if (!Index)
	{
		return;
	}

	FFF_SaveSlotSummary Summary;
	Summary.SlotId = SlotId;
	Summary.bAutoSave = Save.bAutoSave;
	Summary.SavedAt = Save.SavedAt;
	Summary.FestivalDisplayName = Save.FestivalDisplayName;
	Summary.RegionDisplayName = Save.RegionDisplayName;
	Summary.Day = Save.Session.Day;
	Summary.StageId = Save.Session.StageId;

	Index->Upsert(Summary);
	UGameplayStatics::SaveGameToSlot(Index, SlotIndexName, SaveUserIndex);
}

void UFF_SaveSubsystem::RemoveFromSlotIndex(const FName SlotId)
{
	UFF_SaveSlotIndex* Index = LoadSlotIndex();
	if (!Index)
	{
		return;
	}

	Index->Remove(SlotId);
	UGameplayStatics::SaveGameToSlot(Index, SlotIndexName, SaveUserIndex);
}

void UFF_SaveSubsystem::RegisterSaveable(TScriptInterface<IFF_Saveable> Saveable)
{
	if (!Saveable.GetObject())
	{
		return;
	}

	TWeakInterfacePtr<IFF_Saveable> Weak(Saveable.GetObject());
	if (Saveables.Contains(Weak))
	{
		return;
	}

	Saveables.Add(Weak);

	UE_LOG(LogFestaFiestaSave, Log,
		TEXT("[저장] %s 가 참여한다. (단계 %d)"),
		*Saveable->GetSaveKey().ToString(),
		static_cast<int32>(Saveable->GetSaveLoadPhase()));
}

void UFF_SaveSubsystem::UnregisterSaveable(TScriptInterface<IFF_Saveable> Saveable)
{
	if (!Saveable.GetObject())
	{
		return;
	}
	Saveables.Remove(TWeakInterfacePtr<IFF_Saveable>(Saveable.GetObject()));
}

void UFF_SaveSubsystem::CollectPartData(TMap<FName, FFF_SaveBlob>& Out) const
{
	Out.Reset();

	for (const TWeakInterfacePtr<IFF_Saveable>& Weak : Saveables)
	{
		const IFF_Saveable* Part = Weak.Get();
		if (!Part)
		{
			continue;
		}

		const FName Key = Part->GetSaveKey();
		if (Key.IsNone())
		{
			UE_LOG(LogFestaFiestaSave, Warning,
				TEXT("[저장] 이름 없는 파트가 있다. 건너뛴다."));
			continue;
		}

		FFF_SaveBlob Blob;
		Part->WriteSaveData(Blob.Bytes);

		// 빈 것도 담는다. 나중에 "그때 이 파트가 있었나" 를 구분할 수 있어야 한다.
		Out.Add(Key, MoveTemp(Blob));
	}
}

void UFF_SaveSubsystem::RestorePartData(const TMap<FName, FFF_SaveBlob>& In)
{
	/* 단계 순서대로 되돌린다.

	   시설이 복원되기 전에 대기열을 복원하면 붙을 자리가 없고, 돈이 복원되기
	   전에 시설을 놓으면 잔액이 모자라 실패한다. 주석으로만 적어두면 반드시
	   어긋나므로 코드가 순서를 강제한다.
	*/
	constexpr EFF_SaveLoadPhase Phases[] = {
		EFF_SaveLoadPhase::Global,
		EFF_SaveLoadPhase::World,
		EFF_SaveLoadPhase::Attached,
		EFF_SaveLoadPhase::Presentation
	};

	TGuardValue<bool> RestoringGuard(bIsRestoring, true);

	for (const EFF_SaveLoadPhase Phase : Phases)
	{
		for (const TWeakInterfacePtr<IFF_Saveable>& Weak : Saveables)
		{
			IFF_Saveable* Part = Weak.Get();
			if (!Part || Part->GetSaveLoadPhase() != Phase)
			{
				continue;
			}

			const FFF_SaveBlob* Blob = In.Find(Part->GetSaveKey());

			/* 저장에 없으면 빈 배열을 넘긴다.

			   저장 당시에 이 파트가 없었거나 옛 저장이라서 그렇다. 받는 쪽이
			   기본 상태로 두면 된다. 아예 안 부르면 그쪽은 복원이 일어났는지
			   조차 모른다.
			*/
			static const TArray<uint8> Empty;
			Part->ReadSaveData(Blob ? Blob->Bytes : Empty);
		}
	}
}

void UFF_SaveSubsystem::CaptureSession(FFF_SessionSave& Out) const
{
	Out = FFF_SessionSave();

	const UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	// 단계와 날짜.
	if (const UFF_FestivalStageSubsystem* Stage =
		GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>())
	{
		Out.Day = Stage->CurrentDay;

		/* 열거형 값이 아니라 이름을 담는다.

		   열거형은 순서가 바뀌면 같은 숫자가 다른 단계를 가리킨다.
		   옛 저장을 새 코드가 읽었을 때 조용히 엉뚱한 값이 되는 것을 막는다.
		*/
		if (const UEnum* EnumClass = StaticEnum<EFF_FestivalStage>())
		{
			Out.StageId = FName(*EnumClass->GetNameStringByValue(
				static_cast<int64>(Stage->CurrentStage)));
		}
	}

	// 돈과 수입.
	if (const UFF_FestivalProgressionSubsystem* Progression =
		GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>())
	{
		if (Progression->IsFestivalStarted())
		{
			Out.Gold = Progression->GetGold();
			Out.TotalRevenue = Progression->GetTotalRevenue();
		}
	}

	/* 어느 축제인지 담는다. 이걸로 저장과 지금 축제가 같은지 가리고, 이어하기가
	   어느 레벨을 열지 정한다. Progression 의 Definition 은 늘 기본 축제라 쓰지 않는다.
	*/
	Out.FestivalId = ResolveCurrentFestivalId(this);

	// 만족도.
	if (const UFF_SatisfactionMetricsSubsystem* Metrics =
		GameInstance->GetSubsystem<UFF_SatisfactionMetricsSubsystem>())
	{
		Out.Satisfaction = Metrics->GetSimSatisfaction();
	}

}

bool UFF_SaveSubsystem::LoadFromSlot(const FName SlotId)
{
	UFF_SaveGame* Save = nullptr;
	if (!PeekSlot(SlotId, Save) || !Save)
	{
		UE_LOG(LogFestaFiestaSave, Warning,
			TEXT("[저장] %s 를 읽지 못했다."), *MakeSlotName(SlotId));
		return false;
	}

	/* 판번호가 다르면 그대로 읽는다. 다만 무엇이 다른지 남긴다.

	   막아버리면 구조를 조금 바꿀 때마다 그동안의 저장이 전부 무효가 된다.
	   빠진 값은 기본값으로 들어가고, 이상하면 로그를 보고 판단할 수 있다.
	*/
	if (Save->Version > UFF_SaveGame::CurrentVersion)
	{
		// 앞선 판이다. 지금 코드가 모르는 값이 들어 있다는 뜻이라 되돌려 옮길 수 없다.
		UE_LOG(LogFestaFiestaSave, Warning,
			TEXT("[저장] 저장이 코드보다 새 판이다. 저장=%d 코드=%d. "
				 "모르는 값은 무시되고, 다시 저장하면 그 값이 사라진다."),
			Save->Version, UFF_SaveGame::CurrentVersion);
	}

	// 갈아끼우지 않고 합친다. 옛 슬롯을 읽었다고 그 뒤의 이력이 사라지면 안 된다.
	AdoptMeta(Save->Meta, Save->Version, SlotId.ToString());
	LastUsedSlotId = SlotId;

	RestorePartData(Save->PartData);

	UE_LOG(LogFestaFiestaSave, Log,
		TEXT("[저장] %s 를 읽었다. 깬 축제 %d개"),
		*MakeSlotName(SlotId), Meta.ClearedFestivals.Num());

	OnSaveLoaded.Broadcast(SlotId);
	return true;
}

bool UFF_SaveSubsystem::TryContinueFestival(const FName FestivalId)
{
	/* 프론트엔드가 고른 슬롯만 쓴다. 한 번 쓰고 비운다.

	   어떤 이유로 여기서 돌아가든 먼저 비운다. 남겨두면 다음에 새 게임으로
	   들어와도 이 슬롯을 물고 들어가고, 그동안 자동 저장도 계속 막힌다.
	*/
	const FName Requested = PendingContinueSlotId;
	PendingContinueSlotId = NAME_None;

	if (Requested.IsNone())
	{
		// 새 게임이다. 헤더 주석의 [주의] 참고 — 자동 저장으로 떨어지지 않는다.
		return false;
	}

	if (FestivalId.IsNone())
	{
		UE_LOG(LogFestaFiestaSave, Warning,
			TEXT("[저장] 지금 축제를 식별하지 못해 %s 를 이어하지 않는다."), *Requested.ToString());
		return false;
	}

	UFF_SaveGame* Loaded = nullptr;
	if (!PeekSlot(Requested, Loaded) || !Loaded)
	{
		// 고른 슬롯이 사라졌다. 다른 슬롯으로 떨어지지 않는다 —
		// 사용자가 고른 것과 다른 판이 열리면 그게 더 나쁘다.
		UE_LOG(LogFestaFiestaSave, Warning,
			TEXT("[저장] 고른 슬롯 %s 를 읽지 못해 이어하지 않는다."), *Requested.ToString());
		return false;
	}

	// 다른 축제의 저장이면 건드리지 않는다.
	if (Loaded->Session.FestivalId != FestivalId)
	{
		UE_LOG(LogFestaFiestaSave, Warning,
			TEXT("[저장] 저장된 축제(%s)와 지금 축제(%s)가 달라 이어하지 않는다."),
			*Loaded->Session.FestivalId.ToString(), *FestivalId.ToString());
		return false;
	}

	if (!Loaded->Session.IsValid())
	{
		UE_LOG(LogFestaFiestaSave, Warning,
			TEXT("[저장] %s 에 진행 중인 판이 없어 이어하지 않는다."), *Requested.ToString());
		return false;
	}

	AdoptMeta(Loaded->Meta, Loaded->Version, Requested.ToString());
	LastUsedSlotId = Requested;

	/* 되돌리기 전에 날짜 기준을 맞춘다.

	   Stage 가 되돌려지면서 OnStageChanged 를 울리는데, 기준이 옛 값이면 방금 읽은
	   판을 곧바로 다시 찍는다. 해롭지는 않지만 자동 저장 두 칸이 같은 내용이 되어
	   이중화가 사라진다.
	*/
	LastAutoSavedDay = Loaded->Session.Day;

	// 자동 저장에서 이어했으면 다음 저장은 반대 칸에 쓴다. 읽은 칸을 바로 덮지 않는다.
	if (Requested == AutoSaveSlotA)
	{
		bAutoSaveUsesSecondSlot = true;
	}
	else if (Requested == AutoSaveSlotB)
	{
		bAutoSaveUsesSecondSlot = false;
	}

	RestorePartData(Loaded->PartData);

	UE_LOG(LogFestaFiestaSave, Log,
		TEXT("[저장] %s 를 %s 에서 이어서 시작한다. %d일차"),
		*FestivalId.ToString(), *Requested.ToString(), Loaded->Session.Day);

	OnSaveLoaded.Broadcast(Requested);
	return true;
}

bool UFF_SaveSubsystem::DeleteSlot(const FName SlotId)
{
	const FString SlotName = MakeSlotName(SlotId);
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, SaveUserIndex))
	{
		return false;
	}

	const bool bDeleted = UGameplayStatics::DeleteGameInSlot(SlotName, SaveUserIndex);
	if (bDeleted)
	{
		RemoveFromSlotIndex(SlotId);
		UE_LOG(LogFestaFiestaSave, Log, TEXT("[저장] %s 를 지웠다."), *SlotName);

		// 목록이 바뀌었다. FFF_OnSaveCompleted 선언 주석 참고.
		OnSaveCompleted.Broadcast(SlotId);
	}
	return bDeleted;
}

bool UFF_SaveSubsystem::DoesSlotExist(const FName SlotId) const
{
	return UGameplayStatics::DoesSaveGameExist(MakeSlotName(SlotId), SaveUserIndex);
}

bool UFF_SaveSubsystem::PeekSlot(const FName SlotId, UFF_SaveGame*& OutSave) const
{
	OutSave = nullptr;

	const FString SlotName = MakeSlotName(SlotId);
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, SaveUserIndex))
	{
		return false;
	}

	OutSave = Cast<UFF_SaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, SaveUserIndex));
	return OutSave != nullptr;
}

TArray<FName> UFF_SaveSubsystem::GetKnownSlotIds() const
{
	TArray<FName> Result;

	/* 목록 파일 하나만 읽는다.

	   슬롯마다 파일을 두드리면 여덟 개일 때 디스크를 여덟 번 친다.
	   그 시간이 목록 화면이 뜨는 순간에 몰린다.
	*/
	const UFF_SaveSlotIndex* Index = LoadSlotIndex();
	if (!Index)
	{
		return Result;
	}

	for (const FFF_SaveSlotSummary& Summary : Index->Slots)
	{
		/* 목록과 실제 파일이 어긋날 수 있다.

		   사용자가 저장 폴더를 직접 건드리거나 저장 도중에 꺼지면 그렇다.
		   목록은 참고로만 쓰고 실제로 있는 것만 돌려준다.
		*/
		if (DoesSlotExist(Summary.SlotId))
		{
			Result.Add(Summary.SlotId);
		}
	}

	return Result;
}

void UFF_SaveSubsystem::RequestContinueFromSlot(const FName SlotId)
{
	PendingContinueSlotId = SlotId;

	UE_LOG(LogFestaFiestaSave, Log,
		TEXT("[저장] 이어할 슬롯을 %s."),
		SlotId.IsNone() ? TEXT("물렸다") : *FString::Printf(TEXT("%s 로 지정했다"), *SlotId.ToString()));
}

FName UFF_SaveSubsystem::PeekFestivalIdInSlot(const FName SlotId) const
{
	UFF_SaveGame* Loaded = nullptr;
	if (!PeekSlot(SlotId, Loaded) || !Loaded)
	{
		return NAME_None;
	}
	return Loaded->Session.FestivalId;
}

void UFF_SaveSubsystem::MigrateMeta(FFF_MetaSave& InOutMeta, const int32 SavedVersion)
{
	/* 판번호로 가르지 않는다. 옛 값이 남아 있으면 옮긴다.

	   이 수정 전에 쓴 파일은 판 1 이든 판 2 든 판번호가 0 으로 읽힌다
	   (UFF_SaveGame::Version 주석). 판 2 코드는 옛 값에 쓰지 않으므로, 옛 값이
	   true 면 판 1 에서 온 것이 확실하다. 옮긴 뒤 비우므로 여러 번 불려도 같다.
	*/
	if (!InOutMeta.bTutorialCompleted_DEPRECATED && !InOutMeta.bTutorialSkipped_DEPRECATED)
	{
		return;
	}

	/* 판 1 에는 전역 bool 두 개뿐이라 "어느 축제의 튜토리얼을 봤는가" 를 알 수 없다.

	   그래서 완료 플래그는 로비 인트로를 본 것으로만 옮긴다. 축제 튜토리얼은 다시
	   나오는데, 그게 원래 의도한 동작이다 — 두 종류가 전역 플래그 하나를 나눠 쓴 것이
	   버그였고, 그렇게 세워진 플래그를 "축제도 다 봤다" 로 읽으면 버그를 저장에
	   굳히는 셈이 된다.

	   건너뛰기는 그대로 옮긴다. 안내를 끄겠다는 선언이라 판이 바뀌어도 뜻이 같다.
	*/
	InOutMeta.bLobbyIntroSeen |= InOutMeta.bTutorialCompleted_DEPRECATED;
	InOutMeta.bTutorialOptedOut |= InOutMeta.bTutorialSkipped_DEPRECATED;

	InOutMeta.bTutorialCompleted_DEPRECATED = false;
	InOutMeta.bTutorialSkipped_DEPRECATED = false;

	UE_LOG(LogFestaFiestaSave, Log,
		TEXT("[저장] 판 1 값을 옮겼다(파일 판번호 %d). 로비 인트로=%s 안내 끔=%s "
			 "(축제별 이력은 판 1 에 없어서 비운 채로 시작한다)"),
		SavedVersion,
		InOutMeta.bLobbyIntroSeen ? TEXT("봤음") : TEXT("안 봤음"),
		InOutMeta.bTutorialOptedOut ? TEXT("예") : TEXT("아니오"));
}

void UFF_SaveSubsystem::AdoptMeta(
	const FFF_MetaSave& Loaded, const int32 SavedVersion, const FString& Source)
{
	FFF_MetaSave Incoming = Loaded;
	MigrateMeta(Incoming, SavedVersion);

	if (!Meta.MergeFrom(Incoming))
	{
		return;
	}

	UE_LOG(LogFestaFiestaSave, Log,
		TEXT("[저장] %s 의 메타를 합쳤다. 튜토리얼 본 축제 %d개 / 깬 축제 %d개"),
		*Source, Meta.TutorialSeenFestivals.Num(), Meta.ClearedFestivals.Num());

	// 합쳐서 늘어난 것은 바로 남긴다. 안 쓰면 다음 실행에서 이 슬롯을 다시 읽어야만 나온다.
	WriteMetaStore();
}

bool UFF_SaveSubsystem::ReadMetaStore(FFF_MetaSave& OutMeta, int32& OutVersion) const
{
	if (!UGameplayStatics::DoesSaveGameExist(MetaStoreSlotName, SaveUserIndex))
	{
		return false;
	}

	const UFF_SaveGame* Loaded = Cast<UFF_SaveGame>(
		UGameplayStatics::LoadGameFromSlot(MetaStoreSlotName, SaveUserIndex));
	if (!Loaded)
	{
		// 깨졌다. 자동 저장 안의 사본으로 이어지므로 여기서 멈추지 않는다.
		UE_LOG(LogFestaFiestaSave, Warning,
			TEXT("[저장] 메타 파일을 읽지 못했다. 자동 저장 안의 사본으로 채운다."));
		return false;
	}

	OutMeta = Loaded->Meta;
	OutVersion = Loaded->Version;
	return true;
}

void UFF_SaveSubsystem::WriteMetaStore()
{
	UFF_SaveGame* Store = Cast<UFF_SaveGame>(
		UGameplayStatics::CreateSaveGameObject(UFF_SaveGame::StaticClass()));
	if (!Store)
	{
		return;
	}

	// 같은 그릇을 쓰되 판(Session·PartData)은 비워 둔다. 목록에도 올리지 않는다.
	Store->Version = UFF_SaveGame::CurrentVersion;
	Store->Meta = Meta;
	Store->SavedAt = FDateTime::UtcNow();

	if (!UGameplayStatics::SaveGameToSlot(Store, MetaStoreSlotName, SaveUserIndex))
	{
		UE_LOG(LogFestaFiestaSave, Error, TEXT("[저장] 메타 파일 쓰기 실패."));
	}
}

bool UFF_SaveSubsystem::FindNewestAutoSave(UFF_SaveGame*& OutSave, FName& OutSlotId) const
{
	/* 자동 저장이 둘이므로 더 최근 것을 고른다.

	   하나가 저장 도중에 깨졌으면 그쪽은 못 읽히고, 그때는 나머지가 답이다.
	*/
	OutSave = nullptr;
	OutSlotId = NAME_None;

	for (const FName& Candidate : { AutoSaveSlotA, AutoSaveSlotB })
	{
		UFF_SaveGame* Found = nullptr;
		if (!PeekSlot(Candidate, Found) || !Found)
		{
			continue;
		}

		if (!OutSave || Found->SavedAt > OutSave->SavedAt)
		{
			OutSave = Found;
			OutSlotId = Candidate;
		}
	}

	return OutSave != nullptr;
}

void UFF_SaveSubsystem::WriteAutoSave()
{
	// 두 슬롯을 번갈아 쓴다. 하나가 깨져도 직전 것이 남는다.
	SaveToSlot(PickAutoSaveSlot(), true);
	bAutoSaveUsesSecondSlot = !bAutoSaveUsesSecondSlot;
}

void UFF_SaveSubsystem::MarkLobbyIntroSeen()
{
	// 이미 같은 상태면 저장할 것이 없다. 화면이 바뀔 때마다 불려도 괜찮게 둔다.
	if (Meta.bLobbyIntroSeen)
	{
		return;
	}

	Meta.bLobbyIntroSeen = true;

	UE_LOG(LogFestaFiestaSave, Log, TEXT("[저장] 로비 인트로를 끝까지 봤다."));

	/* 바로 남긴다. 여기서 안 쓰면 게임을 끌 때 봤다는 사실이 사라진다.

	   메타 파일에만 쓴다. 예전처럼 자동 저장을 쓰면 로비에서 빈 판이 목록에 올라오고,
	   축제 도중이면 한낮 상태가 아침 저장을 덮는다.
	*/
	WriteMetaStore();
}

void UFF_SaveSubsystem::MarkFestivalTutorialSeen(const FName FestivalId)
{
	/* 축제를 식별하지 못했으면 기록하지 않는다.

	   빈 이름으로 넣어두면 어느 축제를 물어도 맞지 않는 항목이 하나 남는다.
	   식별에 실패한 경우는 다음에 한 번 더 보여주는 편이 안전하다.
	*/
	if (FestivalId.IsNone())
	{
		UE_LOG(LogFestaFiestaSave, Warning,
			TEXT("[저장] 축제를 식별하지 못해 튜토리얼 이력을 남기지 않는다. "
				 "같은 레벨에 다시 들어가면 또 나온다."));
		return;
	}

	if (Meta.TutorialSeenFestivals.Contains(FestivalId))
	{
		return;
	}

	Meta.TutorialSeenFestivals.Add(FestivalId);

	UE_LOG(LogFestaFiestaSave, Log,
		TEXT("[저장] %s 튜토리얼을 끝까지 봤다. (누적 %d개)"),
		*FestivalId.ToString(), Meta.TutorialSeenFestivals.Num());

	WriteMetaStore();
}

void UFF_SaveSubsystem::MarkTutorialOptedOut()
{
	if (Meta.bTutorialOptedOut)
	{
		return;
	}

	Meta.bTutorialOptedOut = true;

	UE_LOG(LogFestaFiestaSave, Log,
		TEXT("[저장] 튜토리얼 안내를 껐다. 축제가 바뀌어도 다시 나오지 않는다."));

	WriteMetaStore();
}

void UFF_SaveSubsystem::MarkFestivalCleared(
	const FName FestivalId, const FFF_FestivalRecord& Record)
{
	if (FestivalId.IsNone())
	{
		return;
	}

	Meta.ClearedFestivals.AddUnique(FestivalId);

	// 더 나은 값만 남긴다. 작년 기록을 넘는 것이 반복 동기라, 내려가면 그게 사라진다.
	FFF_FestivalRecord& Stored = Meta.Records.FindOrAdd(FestivalId);
	const bool bImproved = Stored.MergeBest(Record);

	UE_LOG(LogFestaFiestaSave, Log,
		TEXT("[저장] %s 완료. 기록 갱신=%s (%d회째)"),
		*FestivalId.ToString(), bImproved ? TEXT("예") : TEXT("아니오"), Stored.PlayCount);

	// 판은 여기서 담지 않는다. 끝난 판은 HandleFestivalEnded 가 버린다.
	WriteMetaStore();
}

FFF_FestivalRecord UFF_SaveSubsystem::GetRecord(const FName FestivalId) const
{
	if (const FFF_FestivalRecord* Found = Meta.Records.Find(FestivalId))
	{
		return *Found;
	}
	return FFF_FestivalRecord();
}

void UFF_SaveSubsystem::RegisterDebugConsoleCommands()
{
	// 개발용이다. 배포 빌드에서 콘솔로 저장을 덮거나 불러오면 판이 꼬인다.
#if !UE_BUILD_SHIPPING
	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	UnregisterDebugConsoleCommands();

	if (!ConsoleManager.FindConsoleObject(TEXT("FF.Save.Write")))
	{
		SaveConsoleCommand = ConsoleManager.RegisterConsoleCommand(
			TEXT("FF.Save.Write"),
			TEXT("지금 상태를 저장한다. 사용법: FF.Save.Write [슬롯]  생략 시 자동 저장 차례대로"),
			FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
			{
				// 생략하면 실제 자동 저장과 같은 길로 간다. 따로 `AutoSave` 칸을 만들면
				// 게임은 안 읽는 파일이 목록에만 하나 더 생긴다.
				if (Args.Num() == 0)
				{
					WriteAutoSave();
					return;
				}
				SaveToSlot(FName(*Args[0]), false);
			}));
	}

	if (!ConsoleManager.FindConsoleObject(TEXT("FF.Save.Read")))
	{
		LoadConsoleCommand = ConsoleManager.RegisterConsoleCommand(
			TEXT("FF.Save.Read"),
			TEXT("저장을 읽는다. 사용법: FF.Save.Read [슬롯]  생략 시 가장 최근 자동 저장"),
			FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
			{
				FName SlotId = Args.Num() > 0 ? FName(*Args[0]) : FName();
				if (SlotId.IsNone())
				{
					UFF_SaveGame* Newest = nullptr;
					if (!FindNewestAutoSave(Newest, SlotId))
					{
						UE_LOG(LogFestaFiestaSave, Display, TEXT("[저장] 읽을 자동 저장이 없다."));
						return;
					}
				}
				LoadFromSlot(SlotId);
			}));
	}

	if (!ConsoleManager.FindConsoleObject(TEXT("FF.Save.Stat")))
	{
		StatConsoleCommand = ConsoleManager.RegisterConsoleCommand(
			TEXT("FF.Save.Stat"),
			TEXT("저장 상태를 출력한다."),
			FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
			{
				UE_LOG(LogFestaFiestaSave, Display,
					TEXT("[저장] 로비 인트로=%s 안내 끔=%s / 깬 축제 %d개"),
					Meta.bLobbyIntroSeen ? TEXT("예") : TEXT("아니오"),
					Meta.bTutorialOptedOut ? TEXT("예") : TEXT("아니오"),
					Meta.ClearedFestivals.Num());

				// 어느 축제의 튜토리얼이 막혀 있는지 이름으로 봐야 원인을 가릴 수 있다.
				UE_LOG(LogFestaFiestaSave, Display,
					TEXT("  튜토리얼 본 축제 %d개: %s"),
					Meta.TutorialSeenFestivals.Num(),
					Meta.TutorialSeenFestivals.IsEmpty()
						? TEXT("(없음)")
						: *FString::JoinBy(Meta.TutorialSeenFestivals, TEXT(", "),
							[](const FName& Id) { return Id.ToString(); }));

				FFF_SessionSave Session;
				CaptureSession(Session);
				UE_LOG(LogFestaFiestaSave, Display,
					TEXT("  지금 진행: %d일차 %s / 금화 %d / 만족 %d"),
					Session.Day,
					Session.StageId.IsNone() ? TEXT("-") : *Session.StageId.ToString(),
					Session.Gold, Session.Satisfaction);

				TArray<FString> PartNames;
				for (const TWeakInterfacePtr<IFF_Saveable>& Weak : Saveables)
				{
					if (const IFF_Saveable* Part = Weak.Get())
					{
						PartNames.Add(Part->GetSaveKey().ToString());
					}
				}
				UE_LOG(LogFestaFiestaSave, Display,
					TEXT("  참여 파트 %d개: %s"),
					PartNames.Num(),
					PartNames.Num() == 0 ? TEXT("없음") : *FString::Join(PartNames, TEXT(", ")));

				const TArray<FName> Slots = GetKnownSlotIds();
				UE_LOG(LogFestaFiestaSave, Display,
					TEXT("  슬롯 %d개: %s"),
					Slots.Num(),
					Slots.Num() == 0 ? TEXT("없음")
						: *FString::JoinBy(Slots, TEXT(", "),
							[](const FName& Id) { return Id.ToString(); }));

				for (const TPair<FName, FFF_FestivalRecord>& Pair : Meta.Records)
				{
					UE_LOG(LogFestaFiestaSave, Display,
						TEXT("  %s — 방문 %d / 만족 %.1f / 수익 %d / %d회"),
						*Pair.Key.ToString(), Pair.Value.BestVisitors,
						Pair.Value.BestSatisfaction, Pair.Value.BestProfit, Pair.Value.PlayCount);
				}
			}));
	}
#endif
}

void UFF_SaveSubsystem::UnregisterDebugConsoleCommands()
{
#if !UE_BUILD_SHIPPING
	IConsoleManager& ConsoleManager = IConsoleManager::Get();

	if (SaveConsoleCommand)
	{
		ConsoleManager.UnregisterConsoleObject(SaveConsoleCommand);
	}
	SaveConsoleCommand = nullptr;

	if (LoadConsoleCommand)
	{
		ConsoleManager.UnregisterConsoleObject(LoadConsoleCommand);
	}
	LoadConsoleCommand = nullptr;

	if (StatConsoleCommand)
	{
		ConsoleManager.UnregisterConsoleObject(StatConsoleCommand);
	}
	StatConsoleCommand = nullptr;
#endif
}
