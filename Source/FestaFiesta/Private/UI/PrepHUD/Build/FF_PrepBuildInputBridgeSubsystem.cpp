// FF_PrepBuildInputBridgeSubsystem.cpp
// 역할: 건설 상태 변화와 준비 HUD의 건설 탭 표시를 연결한다.

// BuildMode의 의미 상태 이벤트만 구독해 B 입력의 소유권과 상태표는 기존 구현에 그대로 둔다.

#include "UI/PrepHUD/Build/FF_PrepBuildInputBridgeSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Layout/WidgetPath.h"
#include "Subsystems/SubsystemCollection.h"
#include "UI/PrepHUD/FF_PrepHudBar.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFPrepBuildInputBridge, Log, All);

void UFF_PrepBuildInputBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UFF_BuildModeSubsystem>();
	Super::Initialize(Collection);

	UWorld* World = GetWorld();
	BuildModeSubsystem = World ? World->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
	if (BuildModeSubsystem)
	{
		BuildModeSubsystem->OnBuildModeStateChanged.AddUniqueDynamic(
			this, &ThisClass::HandleBuildModeStateChanged);
	}
}

void UFF_PrepBuildInputBridgeSubsystem::Deinitialize()
{
	if (BuildModeSubsystem)
	{
		BuildModeSubsystem->OnBuildModeStateChanged.RemoveDynamic(
			this, &ThisClass::HandleBuildModeStateChanged);
	}

	bRouteInFlight = false;
	BuildModeSubsystem = nullptr;
	Super::Deinitialize();
}

bool UFF_PrepBuildInputBridgeSubsystem::DoesSupportWorldType(
	const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game ||
		WorldType == EWorldType::PIE ||
		WorldType == EWorldType::GamePreview;
}

void UFF_PrepBuildInputBridgeSubsystem::HandleBuildModeStateChanged(
	const EFF_BuildModeState PreviousState,
	const EFF_BuildModeState NewState)
{
	(void)PreviousState;

	if (!BuildModeSubsystem ||
		NewState != EFF_BuildModeState::CatalogOpen ||
		BuildModeSubsystem->GetBuildModeState() != EFF_BuildModeState::CatalogOpen ||
		bRouteInFlight ||
		BuildModeSubsystem->IsExternalCatalogPresentationActive())
	{
		return;
	}

	UFF_PrepHudBar* PrepHudBar = ResolveSingleActivePrepHudBar();
	const bool bBuildTabSelected = PrepHudBar &&
		PrepHudBar->GetSelection().IsSelected(EFF_PrepHudTab::Build);
	const FFPrepBuildInputBridgeModel::ERouteAction Action =
		FFPrepBuildInputBridgeModel::ResolveCatalogRoute(
			NewState,
			BuildModeSubsystem->GetBuildModeState(),
			bRouteInFlight,
			BuildModeSubsystem->IsExternalCatalogPresentationActive(),
			PrepHudBar != nullptr,
			bBuildTabSelected);
	if (Action == FFPrepBuildInputBridgeModel::ERouteAction::None)
	{
		return;
	}

	TGuardValue<bool> RouteGuard(bRouteInFlight, true);
	if (Action == FFPrepBuildInputBridgeModel::ERouteAction::ReopenBuildTab)
	{
		// SelectTab은 토글이다. 선택 표시만 남고 external opt-in이 사라진 비정상 상태는
		// 기존 Bar의 닫기 경로를 거친 뒤 다시 열어 정상 패널 수명주기를 복구한다.
		PrepHudBar->CloseCurrentPanel();
	}
	PrepHudBar->SelectTab(EFF_PrepHudTab::Build);

	const bool bSelectedAfterRoute =
		PrepHudBar->GetSelection().IsSelected(EFF_PrepHudTab::Build);
	const bool bPresentationReady =
		FFPrepBuildInputBridgeModel::IsPresentationReady(
			BuildModeSubsystem->GetBuildModeState(),
			bSelectedAfterRoute,
			BuildModeSubsystem->IsExternalCatalogPresentationActive());
	if (bPresentationReady)
	{
		return;
	}

	// 잘못 연결된 WBP/현재 단계 거부에서는 빈 탭을 남기지 않는다. external opt-in이 없으므로
	// 호출 중인 기존 OpenCatalog가 곧바로 레거시 카탈로그를 생성한다.
	if (bSelectedAfterRoute)
	{
		PrepHudBar->CloseCurrentPanel();
	}
	UE_LOG(LogFFPrepBuildInputBridge, Warning,
		TEXT("PrepHUD 배치 패널을 열지 못해 기존 건축 카탈로그를 사용합니다."));
}

UFF_PrepHudBar* UFF_PrepBuildInputBridgeSubsystem::ResolveSingleActivePrepHudBar() const
{
	TArray<UUserWidget*> FoundWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
		this, FoundWidgets, UFF_PrepHudBar::StaticClass(), false);

	UFF_PrepHudBar* Resolved = nullptr;
	int32 ActiveCount = 0;
	for (UUserWidget* Widget : FoundWidgets)
	{
		UFF_PrepHudBar* Candidate = Cast<UFF_PrepHudBar>(Widget);
		if (!IsUsablePrepHudBar(Candidate))
		{
			continue;
		}

		Resolved = Candidate;
		++ActiveCount;
	}

	if (ActiveCount == 1)
	{
		return Resolved;
	}
	if (ActiveCount > 1)
	{
		UE_LOG(LogFFPrepBuildInputBridge, Warning,
			TEXT("활성 PrepHUD가 %d개라 임의 선택하지 않고 기존 카탈로그로 폴백합니다."),
			ActiveCount);
	}
	return nullptr;
}

bool UFF_PrepBuildInputBridgeSubsystem::IsUsablePrepHudBar(
	const UFF_PrepHudBar* Candidate) const
{
	if (!IsValid(Candidate) ||
		Candidate->GetWorld() != GetWorld())
	{
		return false;
	}

	const ESlateVisibility Visibility = Candidate->GetVisibility();
	if (Visibility == ESlateVisibility::Collapsed ||
		Visibility == ESlateVisibility::Hidden)
	{
		return false;
	}

	// 다른 로컬 플레이어용 HUD를 현재 플레이어의 B 입력으로 열지 않는다. OwningPlayer가 없는
	// 기존 생성 경로는 Slate의 실제 표시 경로 검증으로 안전하게 판별한다.
	const APlayerController* FirstController =
		GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (const APlayerController* Owner = Candidate->GetOwningPlayer();
		Owner && FirstController && Owner != FirstController)
	{
		return false;
	}

	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}
	const TSharedPtr<SWidget> SlateWidget = Candidate->GetCachedWidget();
	if (!SlateWidget.IsValid())
	{
		return false;
	}

	// UObject 부모가 남은 CommonUI/위젯 풀 항목도 실제 창의 visible 경로에서 빠졌으면 제외한다.
	FWidgetPath VisiblePath;
	return FSlateApplication::Get().GeneratePathToWidgetUnchecked(
		SlateWidget.ToSharedRef(), VisiblePath, EVisibility::Visible);
}
