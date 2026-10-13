// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/FF_SaveSubsystem.h"

#include "Camera/FF_FestivalGameMode.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "NPC/Metrics/FF_SatisfactionMetricsSubsystem.h"
#include "Time/FF_FestivalStage.h"
#include "Save/FF_SaveArchive.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "Tutorial/FF_TutorialSubsystem.h"
#include "UI/Core/FF_UIFlowSubsystem.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY(LogFestaFiestaSave);

namespace
{
	// 슬롯은 한 명이 여러 개 쓸 수 있지만 사용자 인덱스는 지금 하나뿐이다.
	constexpr int32 SaveUserIndex = 0;

	// 수동 저장 칸 수. Manual_1 ~ Manual_8.
	//
	// 언리얼은 슬롯을 나열하는 API 를 주지 않는다. 이름을 알아야 열 수 있어서,
	// 정해둔 규칙으로 만든 이름을 차례로 두드려 본다.
	constexpr int32 MaxManualSlots = 8;

	// 자동 저장 두 칸. 번갈아 쓴다(PickAutoSaveSlot).
	const FName AutoSaveSlotA(TEXT("AutoSave_A"));
	const FName AutoSaveSlotB(TEXT("AutoSave_B"));

	// 슬롯 목록 파일. 시작할 때 정리(PruneSlotIndex)도 해야 해서 위로 올렸다.
	const TCHAR* const SlotIndexName = TEXT("FF_SlotIndex");
}

void UFF_SaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	/* 메타는 채우지 않는다. 게임을 켠 직후에는 아직 어느 판도 아니다.

	   메타는 전부 판 단위다(FFF_MetaSave 주석). 새 게임이면 빈 채로 시작하고,
	   이어하기면 그 세이브에서 가져온다. 예전에 쓰던 FF_MetaStore 파일은 읽지 않는다.

	   자동 저장 두 칸 중 어느 쪽이 최근인지만 본다. 다음 자동 저장이 최근 것을 덮지 않게.
	*/
	UFF_SaveGame* Newest = nullptr;
	FName NewestSlot;
	if (FindNewestAutoSave(Newest, NewestSlot))
	{
		bAutoSaveUsesSecondSlot = (NewestSlot == AutoSaveSlotA);
	}

	PruneSlotIndex();

	BindFestivalStage(Collection);
	RegisterDebugConsoleCommands();

	// 축제 레벨을 벗어나기 직전에 담는다(HandlePreLoadMap).
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UFF_SaveSubsystem::HandlePreLoadMap);
}

void UFF_SaveSubsystem::PruneSlotIndex()
{
	/* 이어할 수 없는 슬롯을 목록에서 뺀다. 파일은 지우지 않는다.

	   축제 이름을 담기 전에 만든 옛 슬롯(예: 단일 `AutoSave`)은 목록에는 멀쩡히
	   보이는데 눌러도 어느 레벨을 열지 몰라 아무 일도 안 일어난다. 누르면 고장난
	   것처럼 보이니 목록에서 빼는 게 낫다. 파일을 남기는 이유는 사용자 데이터라서다.

	   시작할 때 한 번만 한다. 목록 화면이 뜰 때마다 슬롯을 전부 열면 그 순간이 느려진다.
	*/
	UFF_SaveSlotIndex* Index = LoadSlotIndex();
	if (!Index)
	{
		return;
	}

	TArray<FName> Removed;
	for (const FFF_SaveSlotSummary& Summary : Index->Slots)
	{
		UFF_SaveGame* Loaded = nullptr;
		if (!PeekSlot(Summary.SlotId, Loaded) || !Loaded || !Loaded->Session.IsValid())
		{
			Removed.Add(Summary.SlotId);
		}
	}

	if (Removed.Num() == 0)
	{
		return;
	}

	for (const FName& SlotId : Removed)
	{
		Index->Remove(SlotId);
	}
	UGameplayStatics::SaveGameToSlot(Index, SlotIndexName, SaveUserIndex);

	UE_LOG(LogFestaFiestaSave, Log,
		TEXT("[저장] 이어할 수 없는 슬롯 %d개를 목록에서 뺐다(파일은 남김): %s"),
		Removed.Num(),
		*FString::JoinBy(Removed, TEXT(", "), [](const FName& Id) { return Id.ToString(); }));
}

void UFF_SaveSubsystem::Deinitialize()
{
	UnbindFestivalStage();
	UnregisterDebugConsoleCommands();
	FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
	PreLoadMapHandle.Reset();
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
	RunningFestivalId = ResolveCurrentFestivalId(this);
	ResumeRunSaveIfSameFestival();
}

void UFF_SaveSubsystem::ResumeRunSaveIfSameFestival()
{
	// 프론트엔드가 이미 슬롯을 골랐으면(계속하기 · 저장 목록) 그것이 먼저다.
	if (HasPendingContinue() || CurrentRunSlotId.IsNone())
	{
		return;
	}

	const FName FestivalId = ResolveCurrentFestivalId(this);
	UFF_SaveGame* RunSave = nullptr;
	if (FestivalId.IsNone() || !PeekSlot(CurrentRunSlotId, RunSave) || !RunSave)
	{
		return;
	}

	// 이 판의 세이브가 다른 축제이거나 끝난 축제(로비 세이브)면 새로 시작한다.
	if (RunSave->Session.FestivalId != FestivalId || !RunSave->Session.IsFestivalInProgress())
	{
		return;
	}

	PendingContinueSlotId = CurrentRunSlotId;
	UE_LOG(LogFestaFiestaSave, Log,
		TEXT("[저장] 같은 판에서 %s 를 다시 골랐다. 이 판의 세이브 %s 에서 이어한다."),
		*FestivalId.ToString(), *CurrentRunSlotId.ToString());
}

void UFF_SaveSubsystem::HandlePreLoadMap(const FString& MapName)
{
	(void)MapName;

	const UGameInstance* GameInstance = GetGameInstance();
	const UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!World || !World->IsGameWorld() || HasPendingContinue() || bIsRestoring)
	{
		return;
	}

	// 축제 레벨에서 나갈 때만. 프론트엔드 · 로딩 레벨은 축제 GameMode 가 아니다.
	if (!Cast<AFF_FestivalGameMode>(World->GetAuthGameMode()))
	{
		return;
	}

	// 튜토리얼 실습장은 연습이다. 끝나면 잠기는 레벨이라 이어할 자리를 만들지 않는다.
	if (const UFF_TutorialSubsystem* Tutorial = UFF_TutorialSubsystem::Get(World); Tutorial && Tutorial->IsTutorialLevel())
	{
		return;
	}

	const UFF_FestivalProgressionSubsystem* Progression = GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>();
	const UFF_FestivalStageSubsystem* Stage = GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>();
	if (!Progression || !Progression->IsFestivalStarted() || !Stage)
	{
		return;
	}

	// 짓는 시간에만. 운영 도중은 하루 정산이 안 끝나 장부와 어긋난다(HandleStageChanged 주석).
	if (Stage->CurrentStage != EFF_FestivalStage::Setup && Stage->CurrentStage != EFF_FestivalStage::MorningCheck)
	{
		return;
	}

	// 이 판에서 시작한 축제가 아니면(이름을 모르면) 담지 않는다. 이름 없는 세이브는 이어할 수 없다.
	if (RunningFestivalId.IsNone())
	{
		return;
	}

	UE_LOG(LogFestaFiestaSave, Log, TEXT("[저장] 자동 저장 — 축제를 벗어남(%s %d일차)"),
		*RunningFestivalId.ToString(), Stage->CurrentDay);
	bSavingOnLeave = true;
	WriteAutoSave();
	bSavingOnLeave = false;
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

	// 튜토리얼 실습장은 담지 않는다(IsInTutorialLevel 주석). 다른 축제 세이브의 이어하기를 끊지 않게.
	if (IsInTutorialLevel())
	{
		UE_LOG(LogFestaFiestaSave, Log, TEXT("[저장] 튜토리얼 실습장이라 하루 시작 저장을 건너뛴다."));
		return;
	}

	const UFF_FestivalStageSubsystem* Stage =
		UGameInstance::GetSubsystem<UFF_FestivalStageSubsystem>(GetGameInstance());

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

	/* 만족도와 등급은 축제 평가에서 읽는다.

	   종료 순간의 만족도 한 점은 마지막 날 마감의 순간값이라, 평가가 쓰는 날짜별 평균과
	   다르다. 같은 판에서 화면에 보이는 값과 기록이 어긋나지 않도록 평가 값을 그대로 쓴다.
	*/
	const FFF_FestivalEvaluation Evaluation = Progression->GetFestivalEvaluation();
	Record.BestSatisfaction = static_cast<float>(Evaluation.SatisfactionScore);
	Record.BestScore = Evaluation.TotalScore;
	Record.BestGrade = Evaluation.Grade;

	/* 방문객 수 — 경제 쪽에 누적 방문객 getter 가 생겨서 채운다(2026-09-22).

	   Crowd 가 아니라 경제 쪽 API 다 — 입장료 일일 목표 보너스를 만들면서 이미
	   `OnVisitorsAdmitted` 를 하루 단위로 누적하고 있었고, 그 옆에 하루 경계로
	   안 지워지는 축제 전체 누적 칸을 하나 더 두는 것으로 해결됐다.
	*/
	Record.BestVisitors = Progression->GetTotalAdmittedVisitors();

	Record.PlayCount = 1;

	MarkFestivalCleared(FestivalId, Record);

	/* 로비 세이브를 자동으로 남긴다.

	   축제가 끝나면 판은 로비로 돌아가 다음 축제를 고른다. 여기서 찍어두면 방금 깬
	   축제까지 담긴 채로 레벨 선택 화면에서 이어할 수 있다. 축제가 늘어날수록 이게
	   판의 진행을 잇는 지점이 된다.

	   끝난 축제의 도중 세이브(마지막 날 아침)는 지우지 않는다. 그 역시 그 시점의
	   정당한 저장 지점이다. 자동 저장 두 칸을 번갈아 쓰므로 한 칸은 이 로비 세이브가 된다.
	*/
	WriteLobbyAutoSave();
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

	return UGameInstance::GetSubsystem<UFF_SaveSubsystem>(World ? World->GetGameInstance() : nullptr);
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
			// 로비에서는 진행 중인 축제가 없다. 아래 맵 이름 대체로 가면 로비 맵 이름이 축제로 잡힌다.
			if (Flow->GetMode() == EFF_UIFlowMode::Frontend)
			{
				return NAME_None;
			}

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


	// 이 판의 세이브가 됐다. 이후 튜토리얼 기록이 바뀌면 여기에 같이 남긴다(WriteRunMeta).
	CurrentRunSlotId = SlotId;

	UpdateSlotIndex(*Save, SlotId);

	UE_LOG(LogFestaFiestaSave, Log,
		TEXT("[저장] %s 에 저장했다. (자동=%s, 파트 %d개)"),
		*SlotName, bAutoSave ? TEXT("예") : TEXT("아니오"), Save->PartData.Num());

	OnSaveCompleted.Broadcast(SlotId);
	return true;
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
	// 나갈 때 저장은 UI 흐름이 이미 프론트엔드라 위 규칙이 None 을 준다. 시작할 때 담아 둔 이름을 쓴다.
	Out.FestivalId = bSavingOnLeave ? RunningFestivalId : ResolveCurrentFestivalId(this);

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

	// 메타(튜토리얼·깬 축제·기록)는 이 세이브의 것으로 돌린다. 전부 판 단위다.
	AdoptRunMeta(Save->Meta, Save->Version);
	CurrentRunSlotId = SlotId;
	bRunActive = true;

	// 로비 세이브에는 축제 도중의 상태가 없다. 되돌릴 파트도 없다.
	if (!Save->Session.bAtLobby)
	{
		RestorePartData(Save->PartData);
	}

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

	if (!Loaded->Session.IsFestivalInProgress())
	{
		// 로비 세이브는 여기로 오지 않는다. 프론트엔드가 LoadLobbySave 로 따로 연다.
		UE_LOG(LogFestaFiestaSave, Warning,
			TEXT("[저장] %s 에 진행 중인 축제가 없어 이어하지 않는다."), *Requested.ToString());
		return false;
	}

	/* 메타는 이 세이브의 것으로 돌린다. 전부 판 단위다.

	   세이브에 "봤음" 이나 "건너뜀" 이 있으면 튜토리얼이 안 나오고, 없으면 나온다.
	   깬 축제·기록도 그 판의 것으로 돌아간다.
	*/
	AdoptRunMeta(Loaded->Meta, Loaded->Version);
	CurrentRunSlotId = Requested;
	bRunActive = true;

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

	// 보통은 아직 이 판의 세이브가 없다. 그러면 첫 자동 저장에 같이 담긴다.
	WriteRunMeta();
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

	WriteRunMeta();
}

void UFF_SaveSubsystem::MarkTutorialOptedOut()
{
	if (Meta.bTutorialOptedOut)
	{
		return;
	}

	Meta.bTutorialOptedOut = true;

	UE_LOG(LogFestaFiestaSave, Log,
		TEXT("[저장] 튜토리얼 안내를 껐다. 이 판에서는 축제가 바뀌어도 다시 나오지 않는다."));

	WriteRunMeta();
}

void UFF_SaveSubsystem::WriteLobbyAutoSaveAfterTutorial()
{
	if (!bRunActive)
	{
		UE_LOG(LogFestaFiestaSave, Log, TEXT("[저장] 판을 시작하지 않아 튜토리얼 뒤 로비 세이브를 남기지 않는다."));
		return;
	}

	// 이 판에 진행 중인 축제가 있으면 그 세이브를 지킨다. 튜토리얼 기록만 고쳐 쓴다(헤더 주석).
	if (HasRunFestivalInProgress())
	{
		UE_LOG(LogFestaFiestaSave, Log,
			TEXT("[저장] 튜토리얼 레벨을 마쳤다. 진행 중인 축제 세이브 %s 를 지키고 튜토리얼 기록만 고쳐 쓴다."),
			*CurrentRunSlotId.ToString());
		WriteRunMeta();
		return;
	}

	UE_LOG(LogFestaFiestaSave, Log, TEXT("[저장] 튜토리얼 레벨을 마치고 레벨 선택으로 돌아간다. 로비 세이브를 남긴다."));
	WriteLobbyAutoSave();
}

bool UFF_SaveSubsystem::IsInTutorialLevel() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UFF_TutorialSubsystem* Tutorial = GameInstance ? UFF_TutorialSubsystem::Get(GameInstance->GetWorld()) : nullptr;
	return Tutorial && Tutorial->IsTutorialLevel();
}

bool UFF_SaveSubsystem::HasRunFestivalInProgress() const
{
	UFF_SaveGame* RunSave = nullptr;
	return !CurrentRunSlotId.IsNone() && PeekSlot(CurrentRunSlotId, RunSave) && RunSave
		&& RunSave->Session.IsFestivalInProgress();
}

void UFF_SaveSubsystem::BeginNewRun()
{
	/* 새 게임. 메타를 전부 비우고, 이 판의 세이브도 아직 없는 것으로 한다.

	   튜토리얼만이 아니라 깬 축제·기록까지 비운다. 새 게임은 말 그대로 새 게임이다.

	   세이브를 비워두는 것이 중요하다. 안 비우면 직전 판의 세이브를 "이 판의 것" 으로
	   알고, 새 판의 기록을 남의 세이브에 덮어쓴다.
	*/
	Meta = FFF_MetaSave();
	CurrentRunSlotId = NAME_None;
	LastAutoSavedDay = 0;
	bRunActive = true;

	UE_LOG(LogFestaFiestaSave, Log, TEXT("[저장] 새 게임. 메타를 전부 비웠다."));
}

void UFF_SaveSubsystem::AdoptRunMeta(const FFF_MetaSave& FromSave, const int32 SavedVersion)
{
	// 판 1 세이브면 옛 값을 먼저 옮긴다. 옮긴 뒤의 값을 가져와야 한다.
	FFF_MetaSave Incoming = FromSave;
	MigrateMeta(Incoming, SavedVersion);

	Meta = Incoming;

	UE_LOG(LogFestaFiestaSave, Log,
		TEXT("[저장] 세이브의 메타를 따른다. 로비 인트로=%s 안내 끔=%s 본 축제 %d개 / 깬 축제 %d개"),
		Meta.bLobbyIntroSeen ? TEXT("봤음") : TEXT("안 봤음"),
		Meta.bTutorialOptedOut ? TEXT("예") : TEXT("아니오"),
		Meta.TutorialSeenFestivals.Num(), Meta.ClearedFestivals.Num());
}

void UFF_SaveSubsystem::WriteRunMeta()
{
	/* 이 판의 세이브에 메타만 고쳐 쓴다. 판(돈·시설·날짜)은 그대로 둔다.

	   왜 고쳐 쓰는가: 1일차 자동 저장은 셋업이 시작될 때 찍히고, 축제 튜토리얼은
	   그 뒤에 본다. 고쳐 쓰지 않으면 그 세이브로 이어할 때 방금 본 튜토리얼이
	   또 나온다. 튜토리얼 기록은 "그때의 판" 이 아니라 "이 판에서 겪은 것" 이라
	   시점을 따지지 않는다.

	   저장 시각은 바꾸지 않는다. 바꾸면 목록 순서가 튜토리얼 때문에 뒤바뀐다.
	*/
	if (CurrentRunSlotId.IsNone())
	{
		return;
	}

	UFF_SaveGame* Slot = nullptr;
	if (!PeekSlot(CurrentRunSlotId, Slot) || !Slot)
	{
		// 누가 지웠다. 다음 저장에 담기면 된다.
		CurrentRunSlotId = NAME_None;
		return;
	}

	Slot->Meta = Meta;

	if (!UGameplayStatics::SaveGameToSlot(Slot, MakeSlotName(CurrentRunSlotId), SaveUserIndex))
	{
		UE_LOG(LogFestaFiestaSave, Warning,
			TEXT("[저장] %s 에 메타를 고쳐 쓰지 못했다."), *CurrentRunSlotId.ToString());
	}
}

FName UFF_SaveSubsystem::SaveManual()
{
	/* 수동 저장. 빈 칸(Manual_1 ~ Manual_N)에 쓰고, 다 찼으면 가장 오래된 칸을 덮는다.

	   칸 이름을 호출부가 정하게 하지 않는 이유: 버튼 하나로 저장하는 게 기본이라
	   UI 가 이름 규칙까지 알 필요가 없다. 특정 칸에 덮어쓰려면 SaveToSlot 을 쓰면 된다.

	   축제 도중이면 판 전체를, 로비(판을 시작한 뒤)면 로비 세이브를 남긴다.
	   타이틀·플레이 허브처럼 아직 판을 고르기 전에는 담을 판이 없어 막는다.
	*/
	if (HasPendingContinue() || bIsRestoring)
	{
		UE_LOG(LogFestaFiestaSave, Warning, TEXT("[저장] 불러오는 중이라 수동 저장을 할 수 없다."));
		return NAME_None;
	}

	FFF_SessionSave Probe;
	CaptureSession(Probe);

	const bool bFestival = Probe.IsFestivalInProgress();
	const bool bLobby = !bFestival && bRunActive && IsAtLobby();

	if (!bFestival && !bLobby)
	{
		UE_LOG(LogFestaFiestaSave, Warning,
			TEXT("[저장] 지금은 수동 저장을 할 수 없다. 축제 진행 중이거나, 판을 시작한 뒤의 로비에서만 된다."));
		return NAME_None;
	}

	FName Target;
	FDateTime OldestAt = FDateTime::MaxValue();
	FName Oldest;

	for (int32 Index = 1; Index <= MaxManualSlots; ++Index)
	{
		const FName Candidate(*FString::Printf(TEXT("Manual_%d"), Index));

		UFF_SaveGame* Existing = nullptr;
		if (!PeekSlot(Candidate, Existing) || !Existing)
		{
			Target = Candidate;
			break;
		}

		if (Existing->SavedAt < OldestAt)
		{
			OldestAt = Existing->SavedAt;
			Oldest = Candidate;
		}
	}

	if (Target.IsNone())
	{
		Target = Oldest;
	}

	const bool bSaved = bFestival ? SaveToSlot(Target, false) : SaveLobbyToSlot(Target, false);
	return bSaved ? Target : NAME_None;
}

bool UFF_SaveSubsystem::IsAtLobby() const
{
	const UFF_UIFlowSubsystem* Flow = UGameInstance::GetSubsystem<UFF_UIFlowSubsystem>(GetGameInstance());

	return Flow && Flow->GetMode() == EFF_UIFlowMode::Frontend;
}

bool UFF_SaveSubsystem::SaveLobbyToSlot(const FName SlotId, const bool bAutoSave)
{
	/* 로비 세이브. 메타와 "어느 나라의 레벨 선택 화면이었는가" 만 담는다.

	   축제 도중의 상태(돈·시설·날짜)는 담지 않는다. 축제는 매번 새로 시작하므로
	   로비에서 들고 갈 것이 없다. 파트 데이터를 걷으면 끝난 축제의 찌꺼기가 섞인다.
	*/
	UFF_SaveGame* Save = Cast<UFF_SaveGame>(
		UGameplayStatics::CreateSaveGameObject(UFF_SaveGame::StaticClass()));
	if (!Save)
	{
		return false;
	}

	Save->Version = UFF_SaveGame::CurrentVersion;
	Save->Meta = Meta;
	Save->SavedAt = FDateTime::UtcNow();
	Save->bAutoSave = bAutoSave;
	Save->Session.bAtLobby = true;

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UFF_UIFlowSubsystem* Flow = GameInstance->GetSubsystem<UFF_UIFlowSubsystem>())
		{
			Save->Session.CountryId = Flow->GetActiveCountryId();
		}
	}

	const FString SlotName = MakeSlotName(SlotId);
	if (!UGameplayStatics::SaveGameToSlot(Save, SlotName, SaveUserIndex))
	{
		UE_LOG(LogFestaFiestaSave, Error, TEXT("[저장] %s 쓰기 실패."), *SlotName);
		return false;
	}

	CurrentRunSlotId = SlotId;
	UpdateSlotIndex(*Save, SlotId);

	UE_LOG(LogFestaFiestaSave, Log,
		TEXT("[저장] %s 에 로비 세이브를 남겼다. (자동=%s, 나라 %s, 깬 축제 %d개)"),
		*SlotName, bAutoSave ? TEXT("예") : TEXT("아니오"),
		Save->Session.CountryId.IsNone() ? TEXT("-") : *Save->Session.CountryId.ToString(),
		Meta.ClearedFestivals.Num());

	OnSaveCompleted.Broadcast(SlotId);
	return true;
}

void UFF_SaveSubsystem::WriteLobbyAutoSave()
{
	// 축제 도중 자동 저장과 같은 두 칸을 번갈아 쓴다. 차례도 같이 넘긴다.
	SaveLobbyToSlot(PickAutoSaveSlot(), true);
	bAutoSaveUsesSecondSlot = !bAutoSaveUsesSecondSlot;
}

bool UFF_SaveSubsystem::IsLobbySlot(const FName SlotId) const
{
	UFF_SaveGame* Loaded = nullptr;
	return PeekSlot(SlotId, Loaded) && Loaded && Loaded->Session.bAtLobby;
}

bool UFF_SaveSubsystem::LoadLobbySave(const FName SlotId, FName& OutCountryId)
{
	OutCountryId = NAME_None;

	UFF_SaveGame* Loaded = nullptr;
	if (!PeekSlot(SlotId, Loaded) || !Loaded || !Loaded->Session.bAtLobby)
	{
		UE_LOG(LogFestaFiestaSave, Warning,
			TEXT("[저장] %s 는 로비 세이브가 아니라 로비로 이어할 수 없다."), *SlotId.ToString());
		return false;
	}

	AdoptRunMeta(Loaded->Meta, Loaded->Version);
	CurrentRunSlotId = SlotId;
	bRunActive = true;
	OutCountryId = Loaded->Session.CountryId;

	// 자동 저장에서 이어했으면 다음 저장은 반대 칸에 쓴다. 읽은 칸을 바로 덮지 않는다.
	if (SlotId == AutoSaveSlotA)
	{
		bAutoSaveUsesSecondSlot = true;
	}
	else if (SlotId == AutoSaveSlotB)
	{
		bAutoSaveUsesSecondSlot = false;
	}

	UE_LOG(LogFestaFiestaSave, Log,
		TEXT("[저장] %s 에서 로비로 이어한다. 깬 축제 %d개"),
		*SlotId.ToString(), Meta.ClearedFestivals.Num());

	OnSaveLoaded.Broadcast(SlotId);
	return true;
}

void UFF_SaveSubsystem::MarkFestivalCleared(
	const FName FestivalId, const FFF_FestivalRecord& Record)
{
	if (FestivalId.IsNone())
	{
		return;
	}

	Meta.ClearedFestivals.AddUnique(FestivalId);

	// 더 나은 값만 남긴다. 같은 판에서 축제를 다시 맡았을 때 기록이 내려가지 않게.
	FFF_FestivalRecord& Stored = Meta.Records.FindOrAdd(FestivalId);
	const bool bImproved = Stored.MergeBest(Record);

	UE_LOG(LogFestaFiestaSave, Log,
		TEXT("[저장] %s 완료. 기록 갱신=%s (%d회째)"),
		*FestivalId.ToString(), bImproved ? TEXT("예") : TEXT("아니오"), Stored.PlayCount);

	/* 여기서는 파일에 쓰지 않는다.

	   축제가 끝나는 경로(HandleFestivalEnded)는 바로 뒤에 로비 세이브를 남긴다.
	   여기서 이 판의 세이브(마지막 날 아침)에 고쳐 쓰면, 그 세이브로 다시 끝냈을 때
	   같은 축제가 두 번 깬 것으로 셈해진다.
	*/
}

FFF_FestivalRecord UFF_SaveSubsystem::GetRecord(const FName FestivalId) const
{
	if (const FFF_FestivalRecord* Found = Meta.Records.Find(FestivalId))
	{
		return *Found;
	}
	return FFF_FestivalRecord();
}
