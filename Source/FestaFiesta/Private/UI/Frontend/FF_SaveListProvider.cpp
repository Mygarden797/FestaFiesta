// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Frontend/FF_SaveListProvider.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Save/FF_SaveSubsystem.h"
#include "Save/FF_SaveTypes.h"
#include "Time/FF_FestivalStage.h"
#include "UI/Atlas/FF_FestivalAtlasCatalog.h"
#include "UI/Core/FF_UIFlowSettings.h"
#include "UI/FF_UILog.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

void UFF_SaveListProvider::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	/* 저장이 끝나거나 불러오면 목록을 다시 읽는다.

	   화면이 직접 갱신을 부르게 하면 저장이 일어나는 자리를 화면이 알아야 한다.
	   자동 저장은 화면과 무관하게 하루 경계에서 걸리므로 화면은 그 시점을 모른다.
	*/
	if (UFF_SaveSubsystem* Save = Collection.InitializeDependency<UFF_SaveSubsystem>())
	{
		Save->OnSaveCompleted.AddDynamic(this, &UFF_SaveListProvider::HandleSaveChanged);
		Save->OnSaveLoaded.AddDynamic(this, &UFF_SaveListProvider::HandleSaveChanged);
	}

	RefreshSlots();
}

void UFF_SaveListProvider::Deinitialize()
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFF_SaveSubsystem* Save = GameInstance->GetSubsystem<UFF_SaveSubsystem>())
		{
			Save->OnSaveCompleted.RemoveDynamic(this, &UFF_SaveListProvider::HandleSaveChanged);
			Save->OnSaveLoaded.RemoveDynamic(this, &UFF_SaveListProvider::HandleSaveChanged);
		}
	}

	Super::Deinitialize();
}

void UFF_SaveListProvider::HandleSaveChanged(FName SlotId)
{
	(void)SlotId;
	RefreshSlots();
}

UFF_SaveListProvider* UFF_SaveListProvider::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(
		WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UFF_SaveListProvider>() : nullptr;
}

bool UFF_SaveListProvider::GetMostRecent(FFF_SaveSlotEntry& OutEntry) const
{
	if (Slots.Num() == 0)
	{
		// Out 을 건드리지 않는다. 빈 구조체를 돌려주면 호출부가 그것을 유효한 기록으로 그린다.
		return false;
	}

	// RefreshSlots 가 저장 시각 내림차순으로 정렬해 둔다.
	OutEntry = Slots[0];
	return true;
}

void UFF_SaveListProvider::RefreshSlots()
{
	Slots.Reset();

	const UFF_UIFlowSettings* FlowSettings = Settings();
	bShowingDummySlots = FlowSettings && FlowSettings->bUseDummySaveSlots;

	if (bShowingDummySlots)
	{
		// 더미가 켜져 있으면 실제 기록을 읽지 않는다. 섞으면 어느 쪽이 진짜인지 모른다.
		BuildDummySlots();
	}
	else
	{
		BuildRealSlots();
	}

	// 저장 시각 내림차순. 자동/수동 분류는 화면이 한다 — 목록의 순서는 시간 하나로 정한다.
	Slots.Sort([](const FFF_SaveSlotEntry& A, const FFF_SaveSlotEntry& B)
	{
		return A.SavedAt > B.SavedAt;
	});

	UE_LOG(LogFestaFiestaUI, Log,
		TEXT("UFF_SaveListProvider: 저장 기록 %d건 (%s)."),
		Slots.Num(),
		IsBackedByRealSaveSystem() ? TEXT("실제 저장 시스템") : TEXT("더미 — 설정에서 켜짐"));

	OnSaveListChanged.Broadcast();
}

void UFF_SaveListProvider::BuildRealSlots()
{
	const UGameInstance* GameInstance = GetGameInstance();
	UFF_SaveSubsystem* Save =
		GameInstance ? GameInstance->GetSubsystem<UFF_SaveSubsystem>() : nullptr;

	if (!Save)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_SaveListProvider: 저장 시스템이 없어 목록이 빈다."));
		return;
	}

	/* 이름은 카탈로그에서 가져온다.

	   저장 파일은 축제 식별자만 들고 있다. 표시명을 저장에 같이 담으면 나중에
	   이름을 고쳤을 때 옛 저장이 옛 이름을 계속 보여준다. 식별자만 담고 그릴 때
	   찾는 편이 한 곳만 고치면 되게 한다.
	*/
	const UFF_FestivalAtlasCatalog* Catalog =
		(Settings() && !Settings()->AtlasCatalog.IsNull())
			? Settings()->AtlasCatalog.LoadSynchronous()
			: nullptr;

	const UEnum* StageEnum = StaticEnum<EFF_FestivalStage>();

	for (const FName& SlotId : Save->GetKnownSlotIds())
	{
		UFF_SaveGame* Loaded = nullptr;
		if (!Save->PeekSlot(SlotId, Loaded) || !Loaded)
		{
			// 목록과 실제 파일이 어긋난 경우다. 목록은 참고일 뿐이라 조용히 건너뛴다.
			continue;
		}

		FFF_SaveSlotEntry Entry;
		Entry.SlotId = SlotId;
		Entry.bAutoSave = Loaded->bAutoSave;
		Entry.Day = Loaded->Session.Day;
		Entry.SavedAt = Loaded->SavedAt;

		/* 로비 세이브는 축제가 없다. 이름 칸을 비우지 않고 무엇인지 알 수 있게 적는다.

		   예전에는 축제 이름도 단계도 비어 빈 카드로 보였고, 눌러도 반응이 없었다.
		   지역 칸에는 돌아갈 나라를 넣는다. 이어하면 그 나라의 국가 지도가 열린다.
		*/
		if (Loaded->Session.bAtLobby)
		{
			Entry.bAtLobby = true;
			Entry.FestivalName = LOCTEXT("SaveSlotLobbyName", "로비");
			Entry.StageName = LOCTEXT("SaveSlotLobbyStage", "다음 축제 선택");

			if (const FFF_AtlasCountryEntry* Country =
				Catalog ? Catalog->FindCountry(Loaded->Session.CountryId) : nullptr)
			{
				Entry.RegionName = Country->DisplayName;
			}

			Slots.Add(Entry);
			continue;
		}

		if (const FFF_FestivalAtlasEntry* Festival =
			Catalog ? Catalog->FindEntry(Loaded->Session.FestivalId) : nullptr)
		{
			Entry.FestivalName = Festival->FestivalName;

			if (const FFF_AtlasCountryEntry* Country = Catalog->FindCountry(Festival->CountryId))
			{
				Entry.RegionName = Country->DisplayName;
			}
		}
		else if (!Loaded->Session.FestivalId.IsNone())
		{
			// 카탈로그에 없는 축제다. 빈칸보다 식별자라도 보여야 어느 저장인지 가린다.
			Entry.FestivalName = FText::FromName(Loaded->Session.FestivalId);
		}

		/* 단계는 이름으로 저장돼 있다. 표시명은 열거형이 갖고 있다.

		   저장이 이름을 담는 이유(항목 순서가 바뀌어도 안 밀림)는 그대로 살리면서,
		   화면에는 내부 이름이 아니라 한글 표시명을 보여준다.
		*/
		if (StageEnum && !Loaded->Session.StageId.IsNone())
		{
			const int64 Value = StageEnum->GetValueByNameString(Loaded->Session.StageId.ToString());
			if (Value != INDEX_NONE)
			{
				Entry.StageName = StageEnum->GetDisplayNameTextByValue(Value);
			}
		}

		Slots.Add(Entry);
	}
}

bool UFF_SaveListProvider::DeleteSlot(const FName SlotId)
{
	// 더미를 보여주는 중이면 실제 파일을 지우지 않는다. 화면의 카드는 가짜다.
	if (bShowingDummySlots || SlotId.IsNone())
	{
		return false;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	UFF_SaveSubsystem* Save =
		GameInstance ? GameInstance->GetSubsystem<UFF_SaveSubsystem>() : nullptr;
	if (!Save)
	{
		return false;
	}

	/* 지우는 데 실패해도 목록은 다시 읽는다.

	   파일이 이미 없으면 저장 시스템은 false 를 돌려주고 알리지 않는다. 그때 목록을
	   그대로 두면 없는 기록이 카드로 남아 "눌러도 안 지워진다" 로 보인다.
	*/
	const bool bDeleted = Save->DeleteSlot(SlotId);
	if (!bDeleted)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_SaveListProvider: 슬롯 `%s` 를 지우지 못했다. 목록을 다시 읽는다."), *SlotId.ToString());
		RefreshSlots();
	}
	return bDeleted;
}

const UFF_UIFlowSettings* UFF_SaveListProvider::Settings()
{
	return UFF_UIFlowSettings::Get();
}

void UFF_SaveListProvider::BuildDummySlots()
{
	// 화면 배치를 확인하기 위한 값이다. 자동 2건 + 수동 1건으로 두 그룹이 다 보이게 한다.
	// 실제 저장 API 가 오면 이 함수와 bUseDummySaveSlots 를 함께 지운다.
	const FDateTime Now = FDateTime::Now();

	FFF_SaveSlotEntry AutoDay2;
	AutoDay2.SlotId = TEXT("Dummy_Auto_Day2");
	AutoDay2.bAutoSave = true;
	AutoDay2.FestivalName = LOCTEXT("DummySaveFestival", "다대포 바다축제");
	AutoDay2.RegionName = LOCTEXT("DummySaveRegion", "부산");
	AutoDay2.Day = 2;
	AutoDay2.StageName = LOCTEXT("DummySaveStageMorning", "모닝체크");
	AutoDay2.SavedAt = Now - FTimespan::FromMinutes(12);
	Slots.Add(AutoDay2);

	FFF_SaveSlotEntry AutoDay1;
	AutoDay1.SlotId = TEXT("Dummy_Auto_Day1");
	AutoDay1.bAutoSave = true;
	AutoDay1.FestivalName = AutoDay2.FestivalName;
	AutoDay1.RegionName = AutoDay2.RegionName;
	AutoDay1.Day = 1;
	AutoDay1.StageName = LOCTEXT("DummySaveStageClosing", "마감");
	AutoDay1.SavedAt = Now - FTimespan::FromHours(3);
	Slots.Add(AutoDay1);

	FFF_SaveSlotEntry Manual;
	Manual.SlotId = TEXT("Dummy_Manual_01");
	Manual.bAutoSave = false;
	Manual.FestivalName = AutoDay2.FestivalName;
	Manual.RegionName = AutoDay2.RegionName;
	Manual.Day = 1;
	Manual.StageName = LOCTEXT("DummySaveStageLive", "라이브 운영");
	Manual.SavedAt = Now - FTimespan::FromHours(26);
	Slots.Add(Manual);
}

#undef LOCTEXT_NAMESPACE

// =============================================================================
// 콘솔 명령 — 개발용
//
// `bUseDummySaveSlots` 는 Provider 가 Initialize 와 RefreshSlots 에서만 읽는다.
// Project Settings 에서 체크를 바꿔도 이미 살아 있는 GameInstance 는 모른다 —
// PIE 를 다시 시작하면 반영되지만, 그것 때문에 매번 재시작하는 것은 번거롭다.
// =============================================================================
#if !UE_BUILD_SHIPPING

namespace FFSaveListConsole
{
	void RefreshSaves(const TArray<FString>& Args, UWorld* World)
	{
		if (UFF_SaveListProvider* Provider = UFF_SaveListProvider::Get(World))
		{
			// OnSaveListChanged 가 나가므로 떠 있는 저장 기록 화면도 함께 다시 그려진다.
			Provider->RefreshSlots();
		}
		else
		{
			UE_LOG(LogFestaFiestaUI, Error,
				TEXT("FF.Flow.RefreshSaves: 저장 기록 Provider 를 찾을 수 없다. PIE 중인지 확인한다."));
		}
	}

	FAutoConsoleCommandWithWorldAndArgs RefreshSavesCommand(
		TEXT("FF.Flow.RefreshSaves"),
		TEXT("저장 기록 목록을 다시 읽는다. Project Settings 의 Use Dummy Save Slots 를 바꾼 뒤 쓴다. "
			 "PIE 재시작 없이 빈 상태와 있는 상태를 번갈아 볼 수 있다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RefreshSaves));
}

#endif // !UE_BUILD_SHIPPING
