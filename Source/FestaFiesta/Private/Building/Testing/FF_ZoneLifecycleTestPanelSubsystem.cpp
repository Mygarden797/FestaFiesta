// FF_ZoneLifecycleTestPanelSubsystem.cpp

// Test_Main에서 Lifecycle 개발 패널을 숨김 상태로 부착하고 Ctrl+G 표시 토글을 처리한다.
// 흐름: 월드 유형/맵 확인 -> 생성 재시도 -> Collapsed 부착 -> raw chord 감지 -> Visible/Collapsed.
// 불변식: 일반 콘텐츠·Shipping에는 만들지 않고 기존 게임 입력 모드와 커서 소유권을 건드리지 않는다.
// 실패/성능: 생성 전만 로드를 재시도하고 생성 뒤 Tick은 저비용 key edge 검사만 수행한다.

#include "Building/Testing/FF_ZoneLifecycleTestPanelSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Building/Testing/FF_ZoneLifecycleTestPanel.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
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
	HandleToggleInput();
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

	// WBP 자체는 Designer에서 보이는 상태로 유지하고 런타임 인스턴스만 숨긴다.
	// 그래야 에디터 편집성과 "기본 비표시" 런타임 계약을 동시에 지킬 수 있다.
	PanelWidget->SetVisibility(ESlateVisibility::Collapsed);
	PanelWidget->AddToPlayerScreen(120);
	bPanelCreationPending = false;
	UE_LOG(LogFFZoneLifecycleTestPanel, Display,
		TEXT("시설 Lifecycle 패널 준비 완료 (기본 숨김, Ctrl+G로 표시)."));
	return true;
}

void UFF_ZoneLifecycleTestPanelSubsystem::HandleToggleInput()
{
	if (!PanelWidget)
	{
		return;
	}

	const UWorld* World = GetWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	if (!Controller)
	{
		return;
	}

	const bool bControlDown =
		Controller->IsInputKeyDown(EKeys::LeftControl)
		|| Controller->IsInputKeyDown(EKeys::RightControl);
	if (bControlDown && Controller->WasInputKeyJustPressed(EKeys::G))
	{
		TogglePanelVisibility();
	}
}

void UFF_ZoneLifecycleTestPanelSubsystem::TogglePanelVisibility()
{
	if (!PanelWidget)
	{
		return;
	}

	const bool bOpen = PanelWidget->GetVisibility() != ESlateVisibility::Visible;
	PanelWidget->SetVisibility(bOpen
		? ESlateVisibility::Visible
		: ESlateVisibility::Collapsed);
	if (bOpen)
	{
		PanelWidget->RefreshStatus();
	}
	UE_LOG(LogFFZoneLifecycleTestPanel, Verbose,
		TEXT("시설 Lifecycle 패널 %s."), bOpen ? TEXT("표시") : TEXT("숨김"));
}

bool UFF_ZoneLifecycleTestPanelSubsystem::IsPointerOverPanel() const
{
	return PanelWidget && PanelWidget->IsPointerOverInteractivePanel();
}

bool UFF_ZoneLifecycleTestPanelSubsystem::IsTestMainWorld() const
{
	const UWorld* World = GetWorld();
	return World
		&& UGameplayStatics::GetCurrentLevelName(World, true).Equals(
			TEXT("Test_Main"), ESearchCase::CaseSensitive);
}

