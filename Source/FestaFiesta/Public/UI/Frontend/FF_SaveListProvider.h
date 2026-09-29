// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FF_SaveListProvider.generated.h"

/**
 * 저장 기록 한 건. **화면에 그릴 것만 담는다.**
 * 실제 세이브 데이터(월드·경제·Crowd 스냅샷)는 `UFF_FestivalProgressionSubsystem` 이 소유하며
 * UI 는 그것을 직접 직렬화하지 않는다 (`module-ownership.md` 저장 UI 경계).
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_SaveSlotEntry
{
	GENERATED_BODY()

	/** 불러오기를 요청할 때 넘길 키. 표시용이 아니다. */
	UPROPERTY(BlueprintReadOnly, Category = "Save Slot")
	FName SlotId;

	/**
	 * 자동 저장인가.
	 * **색으로 구분하지 않는다** — 화면에서는 아이콘 자리와 라벨을 함께 쓴다 (AT-011).
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Save Slot")
	bool bAutoSave = false;

	/**
	 * 축제를 마치고 로비에서 남긴 기록인가.
	 * 일차와 단계가 없으므로 카드는 `Day N` 대신 상태 문구만 보여준다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Save Slot")
	bool bAtLobby = false;

	UPROPERTY(BlueprintReadOnly, Category = "Save Slot")
	FText FestivalName;

	UPROPERTY(BlueprintReadOnly, Category = "Save Slot")
	FText RegionName;

	/** 몇 일차인가. 1부터. */
	UPROPERTY(BlueprintReadOnly, Category = "Save Slot")
	int32 Day = 1;

	/** 단계 표시명. 내부 enum 이름을 쓰지 않는다 — `셋업`, `라이브 운영`. */
	UPROPERTY(BlueprintReadOnly, Category = "Save Slot")
	FText StageName;

	/** 저장 시각. 화면 서식은 보여주는 쪽이 정한다. */
	UPROPERTY(BlueprintReadOnly, Category = "Save Slot")
	FDateTime SavedAt;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnSaveListChanged);

/**
 * 저장 기록 목록을 화면에 공급하는 **얇은 층.**
 *
 * === 왜 층을 하나 두는가 ===
 * 저장·불러오기 API 가 **아직 없다** (2026-08-20 Dev 확인 — `UFF_FestivalProgressionSubsystem` 에
 * `Save`/`Load`/`Checkpoint` 없음, `USaveGame` 파생 없음). 화면에 더미를 박으면 실물이 왔을 때
 * 플레이 허브·저장 기록 두 화면을 다 고쳐야 한다. 여기 한 곳만 갈아끼우면 되게 한다.
 *
 * === GameInstanceSubsystem 인 이유 ===
 * 플레이 허브와 저장 기록 화면이 **같은 목록을 봐야 한다.** 화면마다 들고 있으면
 * "허브에서는 기록이 없는데 목록에는 있는" 상태가 만들어진다. 레벨을 넘어 살아야 하는 것도 같다.
 *
 * === 더미임을 화면에 숨기지 않는다 ===
 * `IsBackedByRealSaveSystem()` 이 false 인 동안 화면은 그 사실을 함께 표시한다.
 * 진짜처럼 보이는 가짜 목록은 시연에서 오해를 만든다.
 */
UCLASS()
class FESTAFIESTA_API UFF_SaveListProvider : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static UFF_SaveListProvider* Get(const UObject* WorldContextObject);

	/**
	 * 실제 저장 시스템에 연결돼 있는가.
	 * **false 인 동안 화면은 `저장 기능 준비 중` 을 함께 띄운다.**
	 *
	 * 저장 API 는 붙었다. 이제 false 가 되는 경우는 더미를 일부러 켜둔 때뿐이다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Save")
	bool IsBackedByRealSaveSystem() const { return !bShowingDummySlots; }

	/** 기록이 하나라도 있는가. 플레이 허브의 `최근 축제 계속하기` 활성 판정에 쓴다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Save")
	bool HasAnySave() const { return Slots.Num() > 0; }

	/** 저장 시각 내림차순. 자동/수동 분류는 보여주는 쪽이 한다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Save")
	const TArray<FFF_SaveSlotEntry>& GetSlots() const { return Slots; }

	/** 가장 최근 기록. 없으면 false 를 돌려주고 Out 을 건드리지 않는다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Save")
	bool GetMostRecent(FFF_SaveSlotEntry& OutEntry) const;

	/** 목록이 바뀌면 알린다. 실물이 오면 Progression 의 이벤트를 여기로 중계한다. */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI|Save")
	FFF_OnSaveListChanged OnSaveListChanged;

	/**
	 * 기록 하나를 지운다. 지워지면 저장 시스템이 알려와 목록이 다시 만들어진다.
	 *
	 * 화면이 저장 시스템을 직접 부르지 않고 여기를 거친다. 목록을 공급하는 쪽이 삭제도
	 * 맡아야 더미 목록일 때 실제 파일을 건드리는 사고가 없다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Save")
	bool DeleteSlot(FName SlotId);

	/** 더미 목록을 다시 만든다. 콘솔에서 빈 상태와 있는 상태를 번갈아 보려고 둔다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Save")
	void RefreshSlots();

private:
	/**
	 * 더미를 채운다. `UFF_UIFlowSettings::bUseDummySaveSlots` 가 켜져 있을 때만이다.
	 * **기본은 꺼짐** — 첫 실행이 기본 상태이고, 플레이 허브는 그 상태를 보여줘야 한다.
	 */
	void BuildDummySlots();

	/**
	 * 저장 시스템에서 실제 슬롯을 읽어 표시용으로 바꾼다.
	 *
	 * 저장 파일은 축제 식별자만 들고 있고 표시명은 카탈로그가 갖는다. 표시명을 저장에
	 * 같이 담으면 이름을 고쳤을 때 옛 저장이 옛 이름을 계속 보여준다.
	 */
	void BuildRealSlots();

	/** 흐름 설정. 카탈로그와 더미 스위치를 여기서 읽는다. */
	static const class UFF_UIFlowSettings* Settings();

	/** 저장이 끝나거나 불러오면 목록을 다시 읽는다. */
	UFUNCTION()
	void HandleSaveChanged(FName SlotId);

	/** 지금 보여주는 것이 더미인가. 화면이 그 사실을 함께 표시한다. */
	bool bShowingDummySlots = false;

	UPROPERTY()
	TArray<FFF_SaveSlotEntry> Slots;
};
