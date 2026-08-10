// FF_BuildModeSubsystem.cpp

#include "Building/FF_BuildModeSubsystem.h"

#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/FF_BuildCatalogTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSPlacementPreviewActor.h"
#include "FF_FFZSProtectedArea.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Building/UI/FF_BuildCatalogScreen.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFBuildMode, Log, All);

namespace
{
	// 디자이너가 만든 기본 카탈로그 화면을 지연 참조한다. 패키지 로드 실패 시 아래 생성 함수가
	// UFF_BuildCatalogScreen의 C++ 구현으로 폴백하므로 에셋 누락이 건축 모드 전체를 중단시키지 않는다.
	const TSoftClassPtr<UFF_BuildCatalogScreen> BuildCatalogScreenClass(
		FSoftObjectPath(TEXT(
			"/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildCatalogScreen.WBP_FF_BuildCatalogScreen_C")));
}

bool UFF_BuildModeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// 상위 클래스가 거부한 Outer에는 서브시스템을 강제로 만들지 않는다.
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	const UWorld* World = Cast<UWorld>(Outer);
	if (!World || !DoesSupportWorldType(World->WorldType))
	{
		return false;
	}

	// ZoneTestMap에는 QA 전용 입력/프리뷰 하네스가 따로 있다. 프로덕션 시스템까지 생성하면 B 키 한 번이
	// 서로 독립적인 두 배치 시스템을 동시에 구동하므로, PIE 접두사를 제거한 실제 패키지명으로 예외 처리한다.
	const FString NormalizedPackageName = UWorld::RemovePIEPrefix(World->GetPackage()->GetName());
	return NormalizedPackageName != TEXT("/Game/FestaFiesta/Zone/Maps/ZoneTestMap");
}

bool UFF_BuildModeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// 에디터 미리보기나 썸네일 월드에는 로컬 PlayerController가 없으며 UI를 생성해서도 안 된다.
	return WorldType == EWorldType::Game ||
		WorldType == EWorldType::PIE ||
		WorldType == EWorldType::GamePreview;
}

void UFF_BuildModeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// 프리뷰/설치 명령은 Zone System의 read model과 revision을 즉시 사용하므로 초기화 순서를 보장한다.
	Collection.InitializeDependency<UFFZSWorldSubsystem>();
	Super::Initialize(Collection);
}

void UFF_BuildModeSubsystem::Deinitialize()
{
	// 월드 종료 중에는 일부 협력 객체가 이미 사라졌을 수 있다. 소유한 객체부터 정리하고,
	// 실제로 입력을 변경한 세션에서만 남아 있는 로컬 컨트롤러의 입력 모드를 복구한다.
	RemoveCatalogScreen();
	DestroyPlacementPreview();
	ReleaseCatalogThumbnailSession();
	if (bBuildSessionActive)
	{
		if (APlayerController* Controller = GetLocalPlayerController())
		{
			RestoreInputMode(*Controller);
		}
	}
	State = EFF_BuildModeState::Closed;
	bBuildSessionActive = false;
	Super::Deinitialize();
}

void UFF_BuildModeSubsystem::Tick(const float DeltaTime)
{
	// 이 기능은 첫 로컬 플레이어용이다. 서버 월드나 컨트롤러 생성 전 Tick에서는 아무 작업도 하지 않는다.
	APlayerController* Controller = GetLocalPlayerController();
	if (!Controller)
	{
		return;
	}

	// 입력 처리에서 상태가 바뀔 수 있으므로 처리 직후의 State를 기준으로 아래 작업을 선택한다.
	HandleInput();
	if (State == EFF_BuildModeState::CatalogOpen)
	{
		// 카탈로그 데이터 로드는 비동기일 수 있어 준비될 때까지 매 Tick 가볍게 상태만 확인한다.
		ApplyCatalogIfReady();
		return;
	}

	if (State != EFF_BuildModeState::Placing)
	{
		return;
	}

	// 라인트레이스와 전체 배치 정책 검증은 비용이 있으므로 렌더 프레임보다 느린 고정 간격으로 제한한다.
	PreviewRefreshAccumulator += DeltaTime;
	if (PreviewRefreshAccumulator >= PreviewRefreshIntervalSeconds)
	{
		PreviewRefreshAccumulator = 0.0f;
		UpdatePlacementFromCursor();
	}
}

TStatId UFF_BuildModeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_BuildModeSubsystem, STATGROUP_Tickables);
}

void UFF_BuildModeSubsystem::ToggleBuildMode()
{
	// B 키는 진입과 완전 종료만 토글한다. Placing에서 목록으로 돌아가는 동작은 Escape/우클릭이 담당한다.
	if (State == EFF_BuildModeState::Closed)
	{
		OpenCatalog();
		return;
	}
	ExitBuildMode();
}

void UFF_BuildModeSubsystem::OpenCatalog()
{
	APlayerController* Controller = GetLocalPlayerController();
	UFF_BuildCatalogSubsystem* Catalog = GetCatalogSubsystem();
	UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();

	// 카탈로그 진입은 UI 생성, 프리뷰 제거, 입력 변경을 포함하는 다단계 작업이다. 어느 선행 조건에서든
	// 실패하면 부분 상태를 남기지 않도록 동일한 정리 경로를 사용한다. Controller는 호출 시점의 로컬 포인터다.
	const auto AbortOpen = [this, Controller](const FString& Message)
	{
		RemoveCatalogScreen();
		DestroyPlacementPreview();
		ReleaseCatalogThumbnailSession();
		if (bBuildSessionActive && Controller)
		{
			RestoreInputMode(*Controller);
		}
		State = EFF_BuildModeState::Closed;
		bBuildSessionActive = false;
		ShowStatus(Message, true);
	};
	if (!Controller || !Catalog || !ZoneSystem)
	{
		AbortOpen(TEXT("건축 카탈로그를 초기화할 수 없습니다."));
		return;
	}
	if (ZoneSystem->BuildReadModel().Zones.IsEmpty())
	{
		// 배치 가능한 Zone이 전혀 없으면 목록을 보여도 어떤 항목도 설치할 수 없으므로 조기에 닫는다.
		AbortOpen(TEXT("이 월드에는 건축 가능한 Zone이 없습니다."));
		return;
	}

	if (!bBuildSessionActive)
	{
		// Placing -> CatalogOpen 왕복 중에는 같은 세션이다. 최초 진입에서만 원래 커서 상태를 캡처해야
		// 중간 상태의 true 값으로 덮어써져 종료 후 커서가 잘못 남는 일을 피할 수 있다.
		bBuildSessionActive = true;
		bPreviousShowMouseCursor = Controller->bShowMouseCursor;
	}

	// OpenCatalog는 Closed 진입뿐 아니라 배치 취소에서도 호출되므로 두 화면의 객체가 공존하지 않게 정리한다.
	DestroyPlacementPreview();
	RemoveCatalogScreen();
	State = EFF_BuildModeState::CatalogOpen;
	bCatalogWasApplied = false;
	bCatalogThumbnailsApplied = false;
	CreateCatalogScreen(*Controller);
	if (!CatalogScreen)
	{
		// C++ 폴백까지 실패한 경우에는 사용자 입력을 가둬 두지 않고 완전 종료한다.
		State = EFF_BuildModeState::Closed;
		RestoreInputMode(*Controller);
		ReleaseCatalogThumbnailSession();
		bBuildSessionActive = false;
		return;
	}
	Catalog->OnCatalogChanged.AddUniqueDynamic(this, &ThisClass::HandleCatalogChanged);
	Catalog->OnCatalogThumbnailsChanged.AddUniqueDynamic(this, &ThisClass::HandleCatalogThumbnailsChanged);
	SetCatalogInputMode(*Controller);
	AcquireCatalogThumbnailSession(*Catalog);

	const EFF_BuildCatalogLoadState LoadState = Catalog->GetLoadState();
	if (LoadState == EFF_BuildCatalogLoadState::NotRequested ||
		LoadState == EFF_BuildCatalogLoadState::Failed)
	{
		// 첫 진입은 로드를 시작하고, 이전 실패 뒤 다시 연 경우에는 명시적으로 재시도한다.
		Catalog->RefreshCatalog();
	}
	ApplyCatalogIfReady();
}

bool UFF_BuildModeSubsystem::StartPlacement(const FName FacilityDefinitionId)
{
	// NAME_None은 UI에서 아직 항목을 선택하지 않은 상태이므로 오류 UI 없이 단순 실패로 처리한다.
	UFF_BuildCatalogSubsystem* Catalog = GetCatalogSubsystem();
	if (!Catalog || FacilityDefinitionId.IsNone())
	{
		return false;
	}
	if (!CanReconfigure())
	{
		// 시간대/단계 정책은 프리뷰 생성 전에도 막아 불필요한 액터 및 입력 상태 변경을 피한다.
		if (CatalogScreen)
		{
			CatalogScreen->SetStatusText(
				FText::FromString(TEXT("현재 단계에서는 건축할 수 없습니다.")), true);
		}
		return false;
	}

	UFFZSFacilityDefinition* Definition = Catalog->FindDefinition(FacilityDefinitionId);
	if (!Definition)
	{
		// 카탈로그 항목과 실제 데이터 에셋 사이 연결이 끊긴 경우 현재 목록을 유지해 다른 시설을 고를 수 있게 한다.
		if (CatalogScreen)
		{
			CatalogScreen->SetStatusText(FText::FromString(TEXT("시설 정의를 불러오지 못했습니다.")), true);
		}
		return false;
	}

	TArray<FText> DefinitionErrors;
	if (!Definition->IsDefinitionValid(DefinitionErrors))
	{
		// 정의 자체의 스펙 오류는 Zone 배치 검증으로 넘기지 않는다. 여러 이유 중 첫 항목만 간결하게 노출한다.
		const FString Reason = DefinitionErrors.IsEmpty()
			? TEXT("시설 정의가 유효하지 않습니다.")
			: DefinitionErrors[0].ToString();
		if (CatalogScreen)
		{
			CatalogScreen->SetStatusText(FText::FromString(Reason), true);
		}
		return false;
	}

	// 새 선택은 이전 Zone/원점/검증 결과와 독립적이어야 한다. 선택 스펙만 복사하고 배치 표적은 초기화한다.
	SelectedFacilityDefinitionId = FacilityDefinitionId;
	SelectedSpec = Definition->Spec;
	CurrentPlacement = FFFZSGridPlacement{};
	CurrentZoneId = NAME_None;
	bHasPlacementTarget = false;

	RemoveCatalogScreen();
	State = EFF_BuildModeState::Placing;
	SpawnPlacementPreview();
	if (!PlacementPreview)
	{
		// 프리뷰 없이는 정책 검증과 CommitPreview를 수행할 수 없으므로 카탈로그로 안전하게 복귀한다.
		ShowStatus(TEXT("시설 배치 프리뷰를 생성하지 못했습니다."), true);
		ReturnToCatalog();
		return false;
	}
	if (APlayerController* Controller = GetLocalPlayerController())
	{
		SetPlacementInputMode(*Controller);
	}
	// 첫 프레임부터 현재 커서 위치와 회전을 명령에 반영하기 위해 위치 변화 여부와 무관하게 강제 갱신한다.
	UpdatePlacementFromCursor(true);
	ShowStatus(TEXT("Q/E 회전 · 좌클릭 설치 · 우클릭/Esc 목록 복귀 · B 종료"));
	return PlacementPreview != nullptr;
}

void UFF_BuildModeSubsystem::ReturnToCatalog()
{
	// Closed에서 호출된 늦은 UI 이벤트는 이미 끝난 세션을 다시 열지 않도록 무시한다.
	if (State == EFF_BuildModeState::Closed)
	{
		return;
	}
	// 선택을 해제하되 bBuildSessionActive는 유지한다. OpenCatalog가 최초 커서 상태를 덮어쓰지 않게 하기 위함이다.
	DestroyPlacementPreview();
	SelectedFacilityDefinitionId = NAME_None;
	SelectedSpec = FFFZSFacilitySpec{};
	OpenCatalog();
}

void UFF_BuildModeSubsystem::ExitBuildMode()
{
	// 어느 활성 상태에서 호출해도 같은 Closed 불변식(UI/프리뷰/선택/표적 없음)을 만든다.
	RemoveCatalogScreen();
	DestroyPlacementPreview();
	ReleaseCatalogThumbnailSession();
	SelectedFacilityDefinitionId = NAME_None;
	SelectedSpec = FFFZSFacilitySpec{};
	CurrentZoneId = NAME_None;
	bHasPlacementTarget = false;
	State = EFF_BuildModeState::Closed;

	// 컨트롤러가 월드 종료 등으로 사라졌다면 복구할 대상도 없으므로 정리만 완료한다.
	if (APlayerController* Controller = GetLocalPlayerController())
	{
		RestoreInputMode(*Controller);
	}
	bBuildSessionActive = false;
	ShowStatus(TEXT("건축 모드를 종료했습니다."), false, 1.2f);
}

void UFF_BuildModeSubsystem::RotatePlacement(const int32 QuarterTurnDelta)
{
	if (State != EFF_BuildModeState::Placing)
	{
		return;
	}
	// 임의의 증분도 허용하되 Zone System과 같은 정규화 함수를 사용해 4방향 표현을 일관되게 유지한다.
	CurrentPlacement.QuarterTurns = UFFZSGridLibrary::NormalizeQuarterTurns(
		CurrentPlacement.QuarterTurns + QuarterTurnDelta);
	// 회전은 원점/Zone이 같아도 footprint와 중심 보정이 달라지므로 명령을 반드시 다시 만든다.
	UpdatePlacementFromCursor(true);
}

bool UFF_BuildModeSubsystem::CommitPlacement()
{
	// 프리뷰가 없는 Placing 상태는 불완전한 상태이므로 실제 Zone 명령을 직접 만들지 않는다.
	if (State != EFF_BuildModeState::Placing || !PlacementPreview)
	{
		return false;
	}
	if (!CanReconfigure())
	{
		ShowStatus(TEXT("현재 단계에서는 건축할 수 없습니다."), true);
		return false;
	}

	// 클릭 직전 커서와 월드 revision을 강제로 반영해, 마지막 주기 갱신 이후의 이동/월드 변경으로 인한
	// 오래된 유효성 결과를 사용하지 않는다.
	UpdatePlacementFromCursor(true);
	if (!bHasPlacementTarget)
	{
		ShowStatus(TEXT("건축 가능한 Zone의 지면을 가리켜 주세요."), true);
		return false;
	}
	if (!LastValidation.bIsValid)
	{
		ShowStatus(FString::Printf(TEXT("배치 불가: %s"), *LastValidation.Summary.ToString()), true);
		return false;
	}

	const FFFZSCommandResult Result = PlacementPreview->CommitPreview();
	if (!Result.bSucceeded)
	{
		ShowStatus(FString::Printf(TEXT("설치 실패: %s"), *Result.Message.ToString()), true);
		return false;
	}

	ShowStatus(FString::Printf(TEXT("%s 설치 완료"), *SelectedSpec.DisplayName.ToString()), false);
	// 연속 배치를 지원하므로 성공 후 Placing 상태를 유지한다. 설치로 revision/점유 셀이 바뀌었으므로
	// 같은 위치의 다음 프리뷰도 즉시 다시 검증한다.
	UpdatePlacementFromCursor(true);
	return true;
}

void UFF_BuildModeSubsystem::HandleInput()
{
	APlayerController* Controller = GetLocalPlayerController();
	if (!Controller)
	{
		return;
	}

	if (Controller->WasInputKeyJustPressed(EKeys::B))
	{
		// B의 완전 종료를 최우선으로 처리해 같은 프레임의 클릭/회전 입력이 뒤이어 실행되지 않게 한다.
		ToggleBuildMode();
		return;
	}
	if (State != EFF_BuildModeState::Placing)
	{
		return;
	}

	if (Controller->WasInputKeyJustPressed(EKeys::Escape) ||
		Controller->WasInputKeyJustPressed(EKeys::RightMouseButton))
	{
		// 배치 취소는 건축 세션 종료가 아니라 목록 복귀다. 저장한 원래 입력 상태는 유지한다.
		ReturnToCatalog();
		return;
	}
	// 회전과 설치 입력은 독립적으로 검사하므로 같은 프레임에 들어오면 회전된 최종 배치를 설치한다.
	if (Controller->WasInputKeyJustPressed(EKeys::Q))
	{
		RotatePlacement(-1);
	}
	if (Controller->WasInputKeyJustPressed(EKeys::E))
	{
		RotatePlacement(1);
	}
	if (Controller->WasInputKeyJustPressed(EKeys::LeftMouseButton) ||
		Controller->WasInputKeyJustPressed(EKeys::Enter))
	{
		CommitPlacement();
	}
}

void UFF_BuildModeSubsystem::CreateCatalogScreen(APlayerController& Controller)
{
	// SoftClass를 여기서 동기 로드하는 이유는 건축 모드 진입 때만 UI 에셋 비용을 지불하기 위해서다.
	UClass* ResolvedScreenClass = BuildCatalogScreenClass.LoadSynchronous();
	if (!ResolvedScreenClass)
	{
		UE_LOG(LogFFBuildMode, Warning,
			TEXT("WBP_FF_BuildCatalogScreen을 불러오지 못해 네이티브 안전 화면을 사용합니다."));
		// 패키징 누락이나 경로 변경이 있어도 최소 기능을 제공하는 네이티브 화면으로 폴백한다.
		ResolvedScreenClass = UFF_BuildCatalogScreen::StaticClass();
	}

	CatalogScreen = CreateWidget<UFF_BuildCatalogScreen>(&Controller, ResolvedScreenClass);
	if (!CatalogScreen)
	{
		ShowStatus(TEXT("건축 메뉴 위젯 생성에 실패했습니다."), true);
		return;
	}

	// Viewport에 붙이기 전에 WidgetTree를 구성해 BindWidget 결과를 검사한다. 디자이너 위젯의 이름 변경이나
	// 오타로 필수 바인딩이 빠졌다면 빈 화면을 노출하지 않고 네이티브 안전 화면으로 다시 생성한다.
	CatalogScreen->TakeWidget();
	if (ResolvedScreenClass != UFF_BuildCatalogScreen::StaticClass() &&
		!CatalogScreen->IsUsingDesignerWidgetTree())
	{
		UE_LOG(LogFFBuildMode, Error,
			TEXT("WBP_FF_BuildCatalogScreen 바인딩 계약이 잘못되어 네이티브 안전 화면으로 전환합니다."));
		CatalogScreen = CreateWidget<UFF_BuildCatalogScreen>(
			&Controller, UFF_BuildCatalogScreen::StaticClass());
	}
	if (!CatalogScreen)
	{
		// 두 생성 경로가 모두 실패했음을 호출자에게 null로 알린다. OpenCatalog가 입력 모드까지 복구한다.
		ShowStatus(TEXT("건축 메뉴 안전 화면 생성에도 실패했습니다."), true);
		return;
	}

	// AddUObject는 UObject 수명과 연결되지만, 화면을 다시 열 때 중복 구독하지 않도록 제거 함수에서도 명시 해제한다.
	CatalogScreen->OnFacilitySelected.AddUObject(this, &UFF_BuildModeSubsystem::HandleFacilitySelected);
	CatalogScreen->OnCloseRequested.AddUObject(this, &UFF_BuildModeSubsystem::HandleCatalogCloseRequested);
	CatalogScreen->AddToPlayerScreen(80);
	CatalogScreen->ActivateWidget();
}

void UFF_BuildModeSubsystem::RemoveCatalogScreen()
{
	if (UFF_BuildCatalogSubsystem* Catalog = GetCatalogSubsystem())
	{
		Catalog->OnCatalogChanged.RemoveAll(this);
		Catalog->OnCatalogThumbnailsChanged.RemoveAll(this);
	}
	// 생성 도중 실패하거나 이미 정리된 경우에도 호출할 수 있는 멱등 정리 함수다.
	if (!CatalogScreen)
	{
		return;
	}
	// 위젯 제거 과정에서 뒤늦은 선택/닫기 이벤트가 상태를 다시 바꾸지 않도록 델리게이트를 먼저 끊는다.
	CatalogScreen->OnFacilitySelected.RemoveAll(this);
	CatalogScreen->OnCloseRequested.RemoveAll(this);
	CatalogScreen->DeactivateWidget();
	CatalogScreen->RemoveFromParent();
	CatalogScreen = nullptr;
}

void UFF_BuildModeSubsystem::ApplyCatalogIfReady()
{
	if (!CatalogScreen)
	{
		return;
	}
	UFF_BuildCatalogSubsystem* Catalog = GetCatalogSubsystem();
	if (!Catalog)
	{
		return;
	}
	if (Catalog->GetLoadState() == EFF_BuildCatalogLoadState::Failed)
	{
		// 로더가 구체적 오류를 제공하면 보존하고, 없을 때만 사용자 행동(B로 재진입)을 알려 주는 기본 문구를 쓴다.
		const FText Error = Catalog->GetLastError().IsEmpty()
			? FText::FromString(TEXT("시설 목록을 불러오지 못했습니다. B로 닫은 뒤 다시 시도해 주세요."))
			: Catalog->GetLastError();
		CatalogScreen->SetStatusText(Error, true);
		if (!bCatalogWasApplied)
		{
			CatalogScreen->SetCatalogEntries(TArray<FFF_BuildCatalogEntry>());
		}
		bCatalogWasApplied = true;
		bCatalogThumbnailsApplied = true;
		return;
	}
	if (!Catalog->IsCatalogReady())
	{
		// 비동기 로딩이 끝날 때까지 목록을 비어 있는 것으로 확정하지 않고 다음 Tick에서 다시 확인한다.
		CatalogScreen->SetStatusText(FText::FromString(TEXT("시설 목록을 불러오는 중입니다…")));
		return;
	}

	// 준비 완료 시 값 복사본을 UI에 한 번 넘긴다. 썸네일 완료 시에는 카드만 갱신해 선택 분류를 보존한다.
	const TArray<FFF_BuildCatalogEntry> Entries = Catalog->GetEntries();
	if (!bCatalogWasApplied)
	{
		CatalogScreen->SetCatalogEntries(Entries);
		bCatalogWasApplied = true;
		const EFF_BuildCatalogThumbnailLoadState ThumbnailState = Catalog->GetThumbnailLoadState();
		bCatalogThumbnailsApplied = Entries.IsEmpty() ||
			ThumbnailState == EFF_BuildCatalogThumbnailLoadState::Ready ||
			ThumbnailState == EFF_BuildCatalogThumbnailLoadState::Failed;
	}
	if (Entries.IsEmpty())
	{
		CatalogScreen->SetStatusText(FText::FromString(TEXT("등록된 시설 정의가 없습니다.")), true);
		return;
	}

	const EFF_BuildCatalogThumbnailLoadState ThumbnailState = Catalog->GetThumbnailLoadState();
	if (ThumbnailState == EFF_BuildCatalogThumbnailLoadState::WaitingForCatalog ||
		ThumbnailState == EFF_BuildCatalogThumbnailLoadState::Loading ||
		ThumbnailState == EFF_BuildCatalogThumbnailLoadState::NotRequested)
	{
		CatalogScreen->SetStatusText(FText::FromString(TEXT("시설 이미지를 불러오는 중입니다…")));
		return;
	}

	if (!bCatalogThumbnailsApplied)
	{
		CatalogScreen->RefreshLoadedThumbnails();
		bCatalogThumbnailsApplied = true;
	}
	if (ThumbnailState == EFF_BuildCatalogThumbnailLoadState::Failed)
	{
		CatalogScreen->SetStatusText(
			FText::FromString(TEXT("일부 시설 이미지를 불러오지 못해 자리표시자로 표시합니다.")),
			true);
		return;
	}
	if (!CanReconfigure())
	{
		// 목록 탐색은 허용하되 현재 게임 단계에서 설치할 수 없음을 선택 전에 명확히 알린다.
		CatalogScreen->SetStatusText(
			FText::FromString(TEXT("현재 단계에서는 시설을 설치할 수 없습니다. 셋업 또는 모닝체크에서 이용하세요.")),
			true);
	}
	else
	{
		CatalogScreen->SetStatusText(FText::FromString(TEXT("시설을 선택하면 배치 모드로 전환됩니다.")));
	}
}

void UFF_BuildModeSubsystem::AcquireCatalogThumbnailSession(UFF_BuildCatalogSubsystem& Catalog)
{
	if (bThumbnailPreloadAcquired)
	{
		return;
	}

	bThumbnailPreloadAcquired = true;
	// Acquire가 상태 델리게이트를 동기 방송할 수 있으므로, 구독자가 즉시 ExitBuildMode를 호출해도
	// Release 경로가 이번 참조를 볼 수 있게 플래그를 먼저 세웁니다.
	Catalog.AcquireThumbnailPreload();
}

void UFF_BuildModeSubsystem::ReleaseCatalogThumbnailSession()
{
	if (!bThumbnailPreloadAcquired)
	{
		return;
	}

	if (UFF_BuildCatalogSubsystem* Catalog = GetCatalogSubsystem())
	{
		Catalog->ReleaseThumbnailPreload();
	}
	bThumbnailPreloadAcquired = false;
}

void UFF_BuildModeSubsystem::SetCatalogInputMode(APlayerController& Controller) const
{
	// GameAndUI를 사용해 B 키 폴링은 유지하면서 위젯의 마우스/키보드 탐색도 가능하게 한다.
	Controller.bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	if (CatalogScreen)
	{
		// TakeWidget으로 얻은 Slate 위젯을 포커스 대상으로 지정해 첫 키보드 입력이 게임에만 빠지지 않게 한다.
		InputMode.SetWidgetToFocus(CatalogScreen->TakeWidget());
	}
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	Controller.SetInputMode(InputMode);
}

void UFF_BuildModeSubsystem::SetPlacementInputMode(APlayerController& Controller) const
{
	// 배치 중에도 커서를 표시하고 게임 입력을 함께 받는다. 특정 위젯에 포커스를 고정하지 않아
	// PlayerController가 Q/E/Escape/클릭을 계속 감지할 수 있다.
	Controller.bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	Controller.SetInputMode(InputMode);
}

void UFF_BuildModeSubsystem::RestoreInputMode(APlayerController& Controller)
{
	// 커서 가시성은 세션 진입 전 값으로 되돌리되 입력 정책은 이 게임의 기본인 GameOnly로 통일한다.
	Controller.bShowMouseCursor = bPreviousShowMouseCursor;
	Controller.SetInputMode(FInputModeGameOnly());
}

void UFF_BuildModeSubsystem::SpawnPlacementPreview()
{
	// 이전 프리뷰가 남아 있는 비정상 호출에도 액터가 중복되지 않게 먼저 정리한다.
	DestroyPlacementPreview();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 프리뷰는 저장/복제 대상이 아니고 실제 충돌 배치가 아니므로 Transient + AlwaysSpawn으로 생성한다.
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PlacementPreview = World->SpawnActor<AFFZSPlacementPreviewActor>(
		AFFZSPlacementPreviewActor::StaticClass(), FTransform::Identity, SpawnParameters);
	if (PlacementPreview)
	{
		// 갱신 주기와 디버그 표현은 이 서브시스템이 통제한다. 첫 유효 표적을 찾기 전까지 숨겨
		// 월드 원점에 프리뷰가 한 프레임 나타나는 현상도 방지한다.
		PlacementPreview->bAutoRefreshPreview = false;
		PlacementPreview->bDrawDebugCells = false;
		PlacementPreview->SetActorHiddenInGame(true);
	}
}

void UFF_BuildModeSubsystem::DestroyPlacementPreview()
{
	// TObjectPtr가 non-null이어도 PendingKill일 수 있으므로 IsValid일 때만 Destroy를 호출한다.
	if (IsValid(PlacementPreview))
	{
		PlacementPreview->Destroy();
	}
	PlacementPreview = nullptr;
}

bool UFF_BuildModeSubsystem::UpdatePlacementFromCursor(const bool bForceCommandRebuild)
{
	// 이 함수는 Closed/CatalogOpen/Placing 상태 전환을 일으키지 않는다. 필요한 객체가 없으면
	// 기존 배치 세션을 건드리지 않고 갱신 실패만 반환한다.
	UWorld* World = GetWorld();
	APlayerController* Controller = GetLocalPlayerController();
	UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
	if (!World || !Controller || !ZoneSystem || !PlacementPreview)
	{
		return false;
	}

	FVector RayOrigin = FVector::ZeroVector;
	FVector RayDirection = FVector::ForwardVector;
	if (!Controller->DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		// 화면 밖 커서 등으로 레이를 만들 수 없으면 이전 위치를 유효한 표적으로 재사용하지 않는다.
		bHasPlacementTarget = false;
		PlacementPreview->SetActorHiddenInGame(true);
		return false;
	}

	// 바닥을 찾는 Visibility trace가 프리뷰, 기존 시설, Zone 경계/보호 영역 자체에 먼저 맞지 않도록 제외한다.
	// 이 객체들은 배치 정책 검증에서는 고려되지만 커서가 가리키는 지면을 결정하는 장애물은 아니다.
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FFBuildPlacement), false);
	QueryParams.AddIgnoredActor(PlacementPreview);
	if (Controller->GetPawn())
	{
		QueryParams.AddIgnoredActor(Controller->GetPawn());
	}
	for (TActorIterator<AFFZSFacilityActor> It(World); It; ++It)
	{
		QueryParams.AddIgnoredActor(*It);
	}
	for (TActorIterator<AFFZSGameplayZone> It(World); It; ++It)
	{
		QueryParams.AddIgnoredActor(*It);
	}
	for (TActorIterator<AFFZSProtectedArea> It(World); It; ++It)
	{
		QueryParams.AddIgnoredActor(*It);
	}

	FHitResult GroundHit;
	const FVector RayEnd = RayOrigin + RayDirection * 500000.0f;
	if (!World->LineTraceSingleByChannel(GroundHit, RayOrigin, RayEnd, ECC_Visibility, QueryParams) ||
		GroundHit.ImpactNormal.Z < 0.45f)
	{
		// 지면을 못 찾거나 지나치게 수직인 표면은 배치 좌표로 쓰지 않는다. 프리뷰를 숨겨 설치 가능하다는 오해를 막는다.
		bHasPlacementTarget = false;
		PlacementPreview->SetActorHiddenInGame(true);
		return false;
	}

	// 경계에서 Zone이 매 프레임 흔들리지 않도록 현재 Zone을 힌트로 넘긴 뒤, 실제 등록된 Zone인지 다시 확인한다.
	const FName NewZoneId = ZoneSystem->ResolveZoneAtWorldLocation(GroundHit.ImpactPoint, CurrentZoneId);
	AFFZSGameplayZone* Zone = ZoneSystem->FindZone(NewZoneId);
	if (!Zone)
	{
		// 유효 Zone을 잃었음을 명시해 다음 유효 위치가 같은 원점이어도 bChanged가 true가 되게 한다.
		bHasPlacementTarget = false;
		CurrentZoneId = NAME_None;
		PlacementPreview->SetActorHiddenInGame(true);
		return false;
	}

	// Zone 그리드는 1cm 정수 좌표를 사용하지만 조작 안정성을 위해 커서 목표 중심을 10cm 단위로 스냅한다.
	FIntPoint TargetCell = Zone->WorldToGridCell1Cm(GroundHit.ImpactPoint);
	TargetCell.X = FMath::RoundToInt(static_cast<double>(TargetCell.X) / PlacementSnapCm) * PlacementSnapCm;
	TargetCell.Y = FMath::RoundToInt(static_cast<double>(TargetCell.Y) / PlacementSnapCm) * PlacementSnapCm;

	// 회전된 footprint의 실제 Min/Max를 구한다. 원점 기준 회전은 음수 Min을 만들 수 있으므로
	// 단순히 크기의 절반을 빼지 않고 회전 사각형의 중심을 이용해야 커서가 시설 중심에 유지된다.
	const FFFZSCellRect RotatedLocal = UFFZSGridLibrary::RotateRect(
		FFFZSCellRect(FIntPoint::ZeroValue, SelectedSpec.FootprintSizeCells),
		CurrentPlacement.QuarterTurns);
	if (!RotatedLocal.IsValid())
	{
		// 크기가 0 이하인 잘못된 정의는 여기서도 방어해 유효하지 않은 PreviewCommand 생성을 막는다.
		bHasPlacementTarget = false;
		PlacementPreview->SetActorHiddenInGame(true);
		return false;
	}

	const FVector2D CenterOffset(
		(static_cast<double>(RotatedLocal.Min.X) + RotatedLocal.Max.X) * 0.5,
		(static_cast<double>(RotatedLocal.Min.Y) + RotatedLocal.Max.Y) * 0.5);
	const FIntPoint NewOrigin(
		FMath::RoundToInt(static_cast<double>(TargetCell.X) - CenterOffset.X),
		FMath::RoundToInt(static_cast<double>(TargetCell.Y) - CenterOffset.Y));

	// 표적이 한 번 끊겼다가 같은 좌표로 돌아온 경우도 다시 표시/검증해야 하므로 이전 bHasPlacementTarget을 비교한다.
	const bool bChanged = NewOrigin != CurrentPlacement.OriginCell ||
		NewZoneId != CurrentZoneId || !bHasPlacementTarget;
	CurrentPlacement.OriginCell = NewOrigin;
	CurrentZoneId = NewZoneId;
	bHasPlacementTarget = true;
	PlacementPreview->SetActorHiddenInGame(false);
	if (!bChanged && !bForceCommandRebuild)
	{
		// 위치가 그대로면 비싼 command 생성과 정책 검증을 건너뛴다. 프리뷰의 현재 표시 상태는 이미 유효하다.
		return false;
	}

	// PreviewCommand가 현재 선택/Zone/배치/revision을 담도록 만든 후, Zone System의 동일 검증 경로로 시각 상태를 갱신한다.
	RebuildPreviewCommand();
	LastValidation = PlacementPreview->RefreshPreview();
	ShowStatus(LastValidation.bIsValid
		? TEXT("배치 가능 · 좌클릭으로 설치")
		: FString::Printf(TEXT("배치 불가: %s"), *LastValidation.Summary.ToString()),
		!LastValidation.bIsValid,
		PreviewRefreshIntervalSeconds * 2.5f);
	return true;
}

void UFF_BuildModeSubsystem::RebuildPreviewCommand()
{
	UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
	if (!PlacementPreview || !ZoneSystem || CurrentZoneId.IsNone())
	{
		return;
	}

	// 월드 revision을 명령에 넣어 프리뷰 이후 다른 설치/철거가 발생한 오래된 명령이 Commit에서 통과하지 않게 한다.
	PlacementPreview->PreviewCommand = UFFZSGridLibrary::MakeInstallCommand(
		SelectedSpec,
		CurrentZoneId,
		CurrentPlacement,
		ZoneSystem->GetWorldRevision());
}

bool UFF_BuildModeSubsystem::CanReconfigure() const
{
	const UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
	if (!ZoneSystem)
	{
		return false;
	}
	// 시설 변경 가능 단계는 Zone System이 소유한 정책을 그대로 따른다. UI에서 별도 규칙을 만들지 않는다.
	const EFFZSChangePhase Phase = ZoneSystem->GetChangePhase();
	return Phase == EFFZSChangePhase::InitialSetup || Phase == EFFZSChangePhase::MorningCheck;
}

void UFF_BuildModeSubsystem::HandleFacilitySelected(const FName FacilityDefinitionId)
{
	// 델리게이트 반환값은 없으므로 실패 안내는 StartPlacement가 현재 카탈로그 화면에 직접 표시한다.
	StartPlacement(FacilityDefinitionId);
}

void UFF_BuildModeSubsystem::HandleCatalogCloseRequested()
{
	// UI 닫기는 배치 취소와 달리 세션 자체를 끝내 입력 모드까지 원래대로 복구한다.
	ExitBuildMode();
}

void UFF_BuildModeSubsystem::HandleCatalogChanged()
{
	if (State != EFF_BuildModeState::CatalogOpen || !CatalogScreen)
	{
		return;
	}

	// 열린 화면에서 명시적 RefreshCatalog가 끝난 경우에도 새 스냅샷을 한 번 다시 적용합니다.
	bCatalogWasApplied = false;
	bCatalogThumbnailsApplied = false;
	ApplyCatalogIfReady();
}

void UFF_BuildModeSubsystem::HandleCatalogThumbnailsChanged()
{
	if (State != EFF_BuildModeState::CatalogOpen || !CatalogScreen)
	{
		return;
	}

	bCatalogThumbnailsApplied = false;
	ApplyCatalogIfReady();
}

void UFF_BuildModeSubsystem::ShowStatus(
	const FString& Message,
	const bool bIsError,
	const float DisplaySeconds) const
{
	if (Message != LastLoggedStatusMessage)
	{
		// 커서 검증은 초당 여러 번 같은 결과를 만들 수 있다. 화면 표시는 갱신하되 파일/콘솔 로그는 문구가 바뀔 때만 남긴다.
		LastLoggedStatusMessage = Message;
		UE_LOG(LogFFBuildMode, Log, TEXT("%s"), *Message);
	}
	if (GEngine)
	{
		// 고정 키를 사용해 이전 상태 문구를 같은 슬롯에서 교체하고 메시지가 화면에 누적되지 않게 한다.
		GEngine->AddOnScreenDebugMessage(
			620013,
			DisplaySeconds,
			bIsError ? FColor::Red : FColor::Cyan,
			Message);
	}
}

APlayerController* UFF_BuildModeSubsystem::GetLocalPlayerController() const
{
	// 현재 건축 UI는 단일 로컬 플레이어용이다. 첫 컨트롤러가 원격이면 서버에서 UI/입력을 만들지 않는다.
	UWorld* World = GetWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	return Controller && Controller->IsLocalController() ? Controller : nullptr;
}

UFF_BuildCatalogSubsystem* UFF_BuildModeSubsystem::GetCatalogSubsystem() const
{
	// 카탈로그는 월드보다 긴 GameInstance 수명의 캐시이므로 GameInstance를 통해 조회한다.
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UFF_BuildCatalogSubsystem>() : nullptr;
}

UFFZSWorldSubsystem* UFF_BuildModeSubsystem::GetZoneSystem() const
{
	// Zone 상태와 revision은 월드별이므로 현재 월드의 WorldSubsystem을 사용한다.
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
}
