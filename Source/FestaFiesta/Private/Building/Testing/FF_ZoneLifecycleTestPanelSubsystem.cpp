// FF_ZoneLifecycleTestPanelSubsystem.cpp

// Test_Main 게임 월드에서 로컬 플레이어가 준비되면 Lifecycle 개발 패널을 자동 부착한다.
// 흐름: 월드 유형/맵 확인 -> Tick에서 생성 재시도 -> WBP 로드·Viewport 추가 -> 해제 시 제거.
// 불변식: 패널 생성에 성공한 뒤 Tick 재시도를 멈추고 일반 콘텐츠 월드에는 객체를 만들지 않는다.
// 실패/성능: 플레이어 준비 전만 저비용 재시도하고 클래스 로드 실패는 한 번 기록한 뒤 중단한다.

#include "Building/Testing/FF_ZoneLifecycleTestPanelSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Building/Testing/FF_ZoneLifecycleTestPanel.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFZoneLifecycleTestPanel, Log, All);

namespace
{
	const FSoftClassPath LifecyclePanelClassPath(
		TEXT("/Game/FestaFiesta/Building/Debug/WBP_FF_ZoneLifecycleTestPanel."
			"WBP_FF_ZoneLifecycleTestPanel_C"));
}

void UFF_ZoneLifecycleTestPanelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
#if !UE_BUILD_SHIPPING
	bPanelCreationPending = true;
#endif
}

void UFF_ZoneLifecycleTestPanelSubsystem::Deinitialize()
{
	if (PanelWidget)
	{
		PanelWidget->RemoveFromParent();
	}
	PanelWidget = nullptr;
	bPanelCreationPending = false;
	bPanelLoadFailureLogged = false;
	Super::Deinitialize();
}

bool UFF_ZoneLifecycleTestPanelSubsystem::DoesSupportWorldType(
	const EWorldType::Type WorldType) const
{
#if UE_BUILD_SHIPPING
	(void)WorldType;
	return false;
#else
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
#endif
}

void UFF_ZoneLifecycleTestPanelSubsystem::Tick(const float DeltaTime)
{
	(void)DeltaTime;
	if (bPanelCreationPending)
	{
		TryCreatePanel();
	}
}

TStatId UFF_ZoneLifecycleTestPanelSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(
		UFF_ZoneLifecycleTestPanelSubsystem, STATGROUP_Tickables);
}

bool UFF_ZoneLifecycleTestPanelSubsystem::TryCreatePanel()
{
	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay())
	{
		return false;
	}
	if (!IsTestMainWorld())
	{
		bPanelCreationPending = false;
		return false;
	}

	APlayerController* Controller = World->GetFirstPlayerController();
	if (!Controller)
	{
		return false;
	}

	UClass* PanelClass = LifecyclePanelClassPath.TryLoadClass<UFF_ZoneLifecycleTestPanel>();
	if (!PanelClass || !PanelClass->IsChildOf(UFF_ZoneLifecycleTestPanel::StaticClass()))
	{
		if (!bPanelLoadFailureLogged)
		{
			bPanelLoadFailureLogged = true;
			UE_LOG(LogFFZoneLifecycleTestPanel, Error,
				TEXT("시설 Lifecycle 테스트 WBP를 불러오지 못했습니다: %s"),
				*LifecyclePanelClassPath.ToString());
		}
		bPanelCreationPending = false;
		return false;
	}

	PanelWidget = CreateWidget<UFF_ZoneLifecycleTestPanel>(Controller, PanelClass);
	if (!PanelWidget)
	{
		return false;
	}

	PanelWidget->AddToPlayerScreen(120);
	bPanelCreationPending = false;
	UE_LOG(LogFFZoneLifecycleTestPanel, Display,
		TEXT("Test_Main에 시설 Lifecycle 테스트 패널을 표시했습니다."));
	return true;
}

bool UFF_ZoneLifecycleTestPanelSubsystem::IsTestMainWorld() const
{
	const UWorld* World = GetWorld();
	return World
		&& UGameplayStatics::GetCurrentLevelName(World, true).Equals(
			TEXT("Test_Main"), ESearchCase::CaseSensitive);
}

