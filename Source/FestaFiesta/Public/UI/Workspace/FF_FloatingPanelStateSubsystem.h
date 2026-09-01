// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UI/LiveHUD/FF_LiveHudTypes.h"
#include "UI/PrepHUD/FF_PrepHudTypes.h"
#include "FF_FloatingPanelStateSubsystem.generated.h"

/** 준비 화면의 플로팅 패널 작업 공간 상태. 값 타입만 보관한다. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_PrepFloatingPanelSnapshot
{
	GENERATED_BODY()

	/** 열린 순서대로 유지되는 탭 목록. */
	UPROPERTY(BlueprintReadWrite, Category = "FestaFiesta|UI|Workspace")
	TArray<EFF_PrepHudTab> OpenTabs;

	/** 닫기와 별개로 자리를 고정한 탭. */
	UPROPERTY(BlueprintReadWrite, Category = "FestaFiesta|UI|Workspace")
	TSet<EFF_PrepHudTab> PinnedTabs;

	/** 플레이 중 사용자가 옮기거나 크기를 바꾼 탭별 레이아웃. */
	UPROPERTY(BlueprintReadWrite, Category = "FestaFiesta|UI|Workspace")
	TMap<EFF_PrepHudTab, FFF_PrepPanelLayout> RuntimeLayouts;

	/** 여러 패널 중 키보드·게임패드 포커스를 마지막으로 받은 탭. */
	UPROPERTY(BlueprintReadWrite, Category = "FestaFiesta|UI|Workspace")
	FFF_PrepTabSelection FocusedSelection;
};

/** 라이브 화면의 플로팅 패널 작업 공간 상태. 값 타입만 보관한다. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_LiveFloatingPanelSnapshot
{
	GENERATED_BODY()

	/** 열린 순서대로 유지되는 탭 목록. */
	UPROPERTY(BlueprintReadWrite, Category = "FestaFiesta|UI|Workspace")
	TArray<EFF_LiveTab> OpenTabs;

	/** 닫기와 별개로 자리를 고정한 탭. */
	UPROPERTY(BlueprintReadWrite, Category = "FestaFiesta|UI|Workspace")
	TSet<EFF_LiveTab> PinnedTabs;

	/** 플레이 중 사용자가 옮기거나 크기를 바꾼 탭별 레이아웃. */
	UPROPERTY(BlueprintReadWrite, Category = "FestaFiesta|UI|Workspace")
	TMap<EFF_LiveTab, FFF_LivePanelLayout> RuntimeLayouts;

	/** 여러 패널 중 키보드·게임패드 포커스를 마지막으로 받은 탭. */
	UPROPERTY(BlueprintReadWrite, Category = "FestaFiesta|UI|Workspace")
	FFF_LiveTabSelection FocusedSelection;
};

/**
 * 화면 재생성이나 맵 이동 뒤에도 플로팅 패널 작업 공간을 복원하는 런타임 저장소.
 *
 * GameInstance 수명 동안만 유지하며 디스크 저장은 하지 않는다. 스냅샷에는 UObject 포인터를
 * 넣지 않아 위젯 수명과 상태 수명을 분리한다.
 */
UCLASS()
class FESTAFIESTA_API UFF_FloatingPanelStateSubsystem final : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Prep과 Live가 함께 따르는 동시 오픈 상한. */
	static constexpr int32 MaxOpenPanelCount = 5;

	/** 월드 컨텍스트의 GameInstance 저장소. 유효한 월드가 없으면 nullptr. */
	static UFF_FloatingPanelStateSubsystem* Get(const UObject* WorldContextObject);

	const FFF_PrepFloatingPanelSnapshot& GetPrepSnapshot() const { return PrepSnapshot; }
	const FFF_LiveFloatingPanelSnapshot& GetLiveSnapshot() const { return LiveSnapshot; }

	void SavePrep(const FFF_PrepFloatingPanelSnapshot& Snapshot);
	void SaveLive(const FFF_LiveFloatingPanelSnapshot& Snapshot);

private:
	UPROPERTY(Transient)
	FFF_PrepFloatingPanelSnapshot PrepSnapshot;

	UPROPERTY(Transient)
	FFF_LiveFloatingPanelSnapshot LiveSnapshot;
};
