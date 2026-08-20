// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Frontend/FF_SaveListProvider.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIFlowSettings.h"
#include "UI/FF_UILog.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

void UFF_SaveListProvider::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
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

	const UFF_UIFlowSettings* Settings = UFF_UIFlowSettings::Get();
	if (Settings && Settings->bUseDummySaveSlots)
	{
		BuildDummySlots();
	}

	// 저장 시각 내림차순. 자동/수동 분류는 화면이 한다 — 목록의 순서는 시간 하나로 정한다.
	Slots.Sort([](const FFF_SaveSlotEntry& A, const FFF_SaveSlotEntry& B)
	{
		return A.SavedAt > B.SavedAt;
	});

	UE_LOG(LogFestaFiestaUI, Log,
		TEXT("UFF_SaveListProvider: 저장 기록 %d건 (%s)."),
		Slots.Num(),
		IsBackedByRealSaveSystem() ? TEXT("실제 저장 시스템") : TEXT("더미 — 저장 API 미구현"));

	OnSaveListChanged.Broadcast();
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
