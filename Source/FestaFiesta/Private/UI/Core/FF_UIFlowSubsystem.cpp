// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Core/FF_UIFlowSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/Atlas/FF_FestivalAtlasCatalog.h"
#include "UI/Core/FF_UIFlowSettings.h"
#include "UI/Core/FF_UIRootLayout.h"
#include "UI/FF_ActivatableScreenBase.h"
#include "UI/FF_UILog.h"
#include "UI/Title/FF_TitleScreen.h"

void UFF_UIFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// Stage 서브시스템이 먼저 살아 있어야 델리게이트를 붙일 수 있다.
	// 초기화 순서를 운에 맡기지 않는다.
	Collection.InitializeDependency<UFF_FestivalStageSubsystem>();

	Super::Initialize(Collection);

	// 레벨이 열릴 때마다 불린다. travel 후 위젯 재생성 타이밍을 이것으로 잡는다.
	// AFF_FestivalPlayerController::BeginPlay 에 한 줄 넣는 방법도 되지만
	// 그 파일은 카메라 담당 경계라 건드리지 않는다.
	PostLoginHandle = FGameModeEvents::OnGameModePostLoginEvent().AddUObject(
		this, &UFF_UIFlowSubsystem::HandlePostLogin);

	if (UFF_FestivalStageSubsystem* Stage = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFF_FestivalStageSubsystem>()
		: nullptr)
	{
		// AddDynamic 이 아니라 AddUniqueDynamic 을 쓴다 (안티패턴 9).
		Stage->OnStageChanged.AddUniqueDynamic(this, &UFF_UIFlowSubsystem::HandleStageChanged);
		Stage->OnFestivalEnded.AddUniqueDynamic(this, &UFF_UIFlowSubsystem::HandleFestivalEnded);
	}
	else
	{
		// 조용히 넘어가면 "Stage 가 바뀌는데 화면이 안 바뀐다" 로만 드러난다.
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_UIFlowSubsystem: Stage 서브시스템을 찾지 못해 Stage 전환을 구독하지 못했다."));
	}
}

void UFF_UIFlowSubsystem::Deinitialize()
{
	// **해제는 조건부, 정리는 무조건.** 여기서 조기 반환하면 아래 정리를 못 한다 (안티패턴 3).
	if (PostLoginHandle.IsValid())
	{
		FGameModeEvents::OnGameModePostLoginEvent().Remove(PostLoginHandle);
	}
	PostLoginHandle.Reset();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFF_FestivalStageSubsystem* Stage = GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>())
		{
			// Clear() 를 쓰지 않는다 — 남이 붙인 구독까지 지워진다 (안티패턴 8).
			Stage->OnStageChanged.RemoveDynamic(this, &UFF_UIFlowSubsystem::HandleStageChanged);
			Stage->OnFestivalEnded.RemoveDynamic(this, &UFF_UIFlowSubsystem::HandleFestivalEnded);
		}
	}

	if (UFF_TitleScreen* Title = TitleScreen.Get())
	{
		Title->OnPlayRequested.RemoveDynamic(this, &UFF_UIFlowSubsystem::HandleTitlePlayRequested);
		Title->OnOptionsRequested.RemoveDynamic(this, &UFF_UIFlowSubsystem::HandleTitleOptionsRequested);
	}

	TitleScreen.Reset();
	StageHudWidget.Reset();
	RootLayout.Reset();
	Mode = EFF_UIFlowMode::Idle;
	CurrentFrontendScreen = EFF_UIFlowScreen::None;
	ActiveFestivalId = NAME_None;
	bPendingFestivalStart = false;

	Super::Deinitialize();
}

UFF_UIFlowSubsystem* UFF_UIFlowSubsystem::Get(const UObject* WorldContextObject)
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
	return GameInstance ? GameInstance->GetSubsystem<UFF_UIFlowSubsystem>() : nullptr;
}

bool UFF_UIFlowSubsystem::IsVerbose() const
{
	const UFF_UIFlowSettings* Settings = UFF_UIFlowSettings::Get();
	return Settings && Settings->bVerboseFlowLogging;
}

UFF_UIRootLayout* UFF_UIFlowSubsystem::GetRootLayout() const
{
	return RootLayout.Get();
}

bool UFF_UIFlowSubsystem::IsFrontendWorld(const UWorld* World) const
{
	const UFF_UIFlowSettings* Settings = UFF_UIFlowSettings::Get();
	if (!World || !Settings || Settings->FrontendLevel.IsNull())
	{
		// 프론트엔드 레벨을 지정하지 않았으면 어떤 레벨도 프론트엔드가 아니다.
		// 자동 시작을 끄는 방법이기도 하다.
		return false;
	}

	// PIE 에서는 패키지 이름에 `UEDPIE_0_` 접두어가 붙는다. 떼지 않으면 절대 일치하지 않는다.
	const FString WorldPackage = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
	const FString FrontendPackage = Settings->FrontendLevel.ToSoftObjectPath().GetLongPackageName();

	return !FrontendPackage.IsEmpty() && WorldPackage == FrontendPackage;
}

const UFF_FestivalAtlasCatalog* UFF_UIFlowSubsystem::LoadCatalog() const
{
	const UFF_UIFlowSettings* Settings = UFF_UIFlowSettings::Get();
	if (!Settings || Settings->AtlasCatalog.IsNull())
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_UIFlowSubsystem: Atlas 카탈로그가 지정되지 않았다. "
				 "Project Settings > Game > FestaFiesta UI Flow 에서 지정한다."));
		return nullptr;
	}

	// 카탈로그는 작고 화면 진입 직전에만 필요하다. 비동기로 만들 이유가 없다.
	const UFF_FestivalAtlasCatalog* Catalog = Settings->AtlasCatalog.LoadSynchronous();
	if (!Catalog)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_UIFlowSubsystem: Atlas 카탈로그를 로드할 수 없다 (%s)."),
			*Settings->AtlasCatalog.ToString());
	}
	return Catalog;
}

FString UFF_UIFlowSubsystem::DescribeState() const
{
	return FString::Printf(
		TEXT("Mode=%s Frontend=%s Festival=%s RootLayout=%s PendingStart=%s"),
		*UEnum::GetValueAsString(Mode),
		*UEnum::GetValueAsString(CurrentFrontendScreen),
		ActiveFestivalId.IsNone() ? TEXT("(없음)") : *ActiveFestivalId.ToString(),
		RootLayout.IsValid() ? TEXT("있음") : TEXT("없음"),
		bPendingFestivalStart ? TEXT("true") : TEXT("false"));
}

void UFF_UIFlowSubsystem::HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	// 로컬 컨트롤러만 본다. 멀티가 붙어도 남의 화면을 만들지 않는다.
	if (!NewPlayer || !NewPlayer->IsLocalController())
	{
		return;
	}

	const UWorld* World = NewPlayer->GetWorld();
	if (!World)
	{
		return;
	}

	// --- 1) 우리가 직접 열어 온 축제 레벨 ---
	if (Mode == EFF_UIFlowMode::Festival)
	{
		if (!EnsureRootLayout(NewPlayer))
		{
			return;
		}

		UFF_FestivalStageSubsystem* Stage = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UFF_FestivalStageSubsystem>()
			: nullptr;
		if (!Stage)
		{
			UE_LOG(LogFestaFiestaUI, Error,
				TEXT("UFF_UIFlowSubsystem: 축제 레벨에 들어왔지만 Stage 서브시스템이 없다."));
			return;
		}

		if (bPendingFestivalStart)
		{
			bPendingFestivalStart = false;

			// 🔴 여기가 아직 막혀 있는 곳이다.
			// Stage 서브시스템에 **축제를 처음부터 시작하는 API 가 없다.** CurrentDay 는
			// BlueprintReadOnly 이고 리셋 함수가 없어, 프론트엔드에 오래 머물러 시계가
			// 흘러 버렸으면 Day 2·3 에서 축제가 시작된다.
			// UI 가 남의 상태를 직접 쓰지 않으므로 여기서는 알리기만 한다.
			// 강민서님께 StartNewFestival() 요청 — UI_API.md 참고.
			if (Stage->CurrentDay != 1)
			{
				UE_LOG(LogFestaFiestaUI, Warning,
					TEXT("UFF_UIFlowSubsystem: 축제를 시작하는데 CurrentDay=%d 다. "
						 "Stage 시계가 프론트엔드에서 이미 흘렀다. Day 1 부터 시작하는 API 가 필요하다."),
					Stage->CurrentDay);
			}

			// RequestTransition 이 OnStageChanged 를 울리고, 그 핸들러가 화면을 올린다.
			// 여기서 직접 ApplyStageScreen 을 부르지 않는다 — 경로가 둘이 되면
			// 어느 쪽이 화면을 올렸는지 알 수 없다 (안티패턴 10).
			Stage->RequestTransition(EFF_FestivalStage::Setup);
		}
		else
		{
			// 축제 중에 레벨이 다시 열린 경우다. 지금 Stage 화면을 다시 올린다.
			// 이벤트는 미래만 알려주므로 현재값을 한 번 읽는다 (Bind + Sync).
			ApplyStageScreen(Stage->CurrentStage);
		}
		return;
	}

	// --- 2) 프론트엔드 레벨 ---
	if (IsFrontendWorld(World))
	{
		Mode = EFF_UIFlowMode::Frontend;
		if (!EnsureRootLayout(NewPlayer))
		{
			return;
		}

		GoToScreen(PendingFrontendScreen);

		// 다음 진입의 기본값은 항상 타이틀이다. `로비로` 가 한 번만 국가 지도로 보낸다.
		PendingFrontendScreen = EFF_UIFlowScreen::Title;
		return;
	}

	// --- 3) 남의 테스트 레벨 ---
	// **아무것도 하지 않는다.** Test_Main 이나 팀원 맵에서 UI 가 끼어들면 전원의 작업이 막힌다.
	// 여기서 흐름을 태워 보려면 콘솔에서 FF.Flow.Start <FestivalId> 를 쓴다.
	if (IsVerbose())
	{
		UE_LOG(LogFestaFiestaUI, Log,
			TEXT("UFF_UIFlowSubsystem: %s 는 FrontendLevel 이 아니라 UI 흐름을 시작하지 않았다. "
				 "필요하면 FF.Flow.Start 를 쓴다."),
			*World->GetOutermost()->GetName());
	}
}

bool UFF_UIFlowSubsystem::EnsureRootLayout(APlayerController* Controller)
{
	if (!Controller)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_UIFlowSubsystem: PlayerController 가 없어 RootLayout 을 만들 수 없다."));
		return false;
	}

	// 위젯은 월드와 함께 죽는다. travel 후에는 약한 참조가 이미 무효라 새로 만든다.
	if (RootLayout.IsValid() && RootLayout->GetOwningPlayer() == Controller)
	{
		return true;
	}

	const UFF_UIFlowSettings* Settings = UFF_UIFlowSettings::Get();
	if (!Settings || Settings->RootLayoutClass.IsNull())
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_UIFlowSubsystem: RootLayoutClass 가 지정되지 않았다. "
				 "Project Settings > Game > FestaFiesta UI Flow 에 WBP_FF_UIRootLayout 을 넣는다."));
		return false;
	}

	UClass* LayoutClass = Settings->RootLayoutClass.LoadSynchronous();
	if (!LayoutClass)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_UIFlowSubsystem: RootLayoutClass 를 로드할 수 없다 (%s)."),
			*Settings->RootLayoutClass.ToString());
		return false;
	}

	UFF_UIRootLayout* Layout = CreateWidget<UFF_UIRootLayout>(Controller, LayoutClass);
	if (!Layout)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_UIFlowSubsystem: RootLayout 생성에 실패했다 (%s)."), *LayoutClass->GetName());
		return false;
	}

	// 뷰포트에는 이것 하나만 올린다. 나머지 UI 는 전부 이 아래 레이어로 들어간다.
	Layout->AddToViewport(0);
	RootLayout = Layout;
	StageHudWidget.Reset();
	TitleScreen.Reset();

	if (IsVerbose())
	{
		UE_LOG(LogFestaFiestaUI, Log,
			TEXT("UFF_UIFlowSubsystem: RootLayout 을 뷰포트에 올렸다 (%s)."), *LayoutClass->GetName());
	}
	return true;
}

void UFF_UIFlowSubsystem::StartFrontend()
{
	Mode = EFF_UIFlowMode::Frontend;

	// 여기서 RootLayout 을 함께 보장한다. 콘솔에서 아무 레벨에서 불러도 동작해야 하고,
	// 그 레벨은 FrontendLevel 이 아니라 HandlePostLogin 이 이미 아무것도 안 하고 지나갔다.
	APlayerController* Controller = GetGameInstance()
		? GetGameInstance()->GetFirstLocalPlayerController()
		: nullptr;

	if (!EnsureRootLayout(Controller))
	{
		return;
	}

	GoToScreen(EFF_UIFlowScreen::Title);
}

void UFF_UIFlowSubsystem::GoToScreen(EFF_UIFlowScreen Screen)
{
	if (Screen == EFF_UIFlowScreen::None)
	{
		return;
	}

	UFF_UIRootLayout* Layout = RootLayout.Get();
	if (!Layout)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_UIFlowSubsystem::GoToScreen: RootLayout 이 없어 %s 를 띄우지 못했다."),
			*UEnum::GetValueAsString(Screen));
		return;
	}

	const UFF_UIFlowSettings* Settings = UFF_UIFlowSettings::Get();
	const TSoftClassPtr<UFF_ActivatableScreenBase>* Found =
		Settings ? Settings->FrontendScreens.Find(Screen) : nullptr;

	if (!Found || Found->IsNull())
	{
		// 스텁 단계에서 어느 화면이 아직 배선되지 않았는지 여기서 드러난다.
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_UIFlowSubsystem::GoToScreen: %s 에 배선된 화면이 없다. "
				 "Project Settings > Game > FestaFiesta UI Flow > Frontend Screens 를 채운다."),
			*UEnum::GetValueAsString(Screen));
		return;
	}

	UClass* ScreenClass = Found->LoadSynchronous();
	if (!ScreenClass)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_UIFlowSubsystem::GoToScreen: %s 의 클래스를 로드할 수 없다 (%s)."),
			*UEnum::GetValueAsString(Screen), *Found->ToString());
		return;
	}

	UFF_ActivatableScreenBase* Pushed = Layout->PushScreen(ScreenClass);
	if (!Pushed)
	{
		// PushScreen 이 실패 이유를 이미 남긴다. 여기서는 어느 화면이었는지만 덧붙인다.
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_UIFlowSubsystem::GoToScreen: %s 를 화면 스택에 올리지 못했다."),
			*UEnum::GetValueAsString(Screen));
		return;
	}

	CurrentFrontendScreen = Screen;

	// 타이틀은 자기가 다음 화면을 모른다. 그게 설계다 — 축제가 늘 때마다 타이틀을 고치지 않게 한다.
	// 그래서 화면을 띄운 쪽인 여기가 델리게이트를 받는다.
	if (UFF_TitleScreen* Title = Cast<UFF_TitleScreen>(Pushed))
	{
		TitleScreen = Title;
		Title->OnPlayRequested.AddUniqueDynamic(this, &UFF_UIFlowSubsystem::HandleTitlePlayRequested);
		Title->OnOptionsRequested.AddUniqueDynamic(this, &UFF_UIFlowSubsystem::HandleTitleOptionsRequested);
	}

	if (IsVerbose())
	{
		UE_LOG(LogFestaFiestaUI, Log, TEXT("UFF_UIFlowSubsystem: 프론트엔드 화면 -> %s"),
			*UEnum::GetValueAsString(Screen));
	}
}

bool UFF_UIFlowSubsystem::GoBack()
{
	UFF_UIRootLayout* Layout = RootLayout.Get();
	return Layout ? Layout->PopScreen() : false;
}

void UFF_UIFlowSubsystem::HandleTitlePlayRequested()
{
	GoToScreen(EFF_UIFlowScreen::PlayHub);
}

void UFF_UIFlowSubsystem::HandleTitleOptionsRequested()
{
	GoToScreen(EFF_UIFlowScreen::Options);
}

const FFF_FestivalAtlasEntry* UFF_UIFlowSubsystem::FindStartableEntry(FName FestivalId) const
{
	const UFF_FestivalAtlasCatalog* Catalog = LoadCatalog();
	if (!Catalog)
	{
		return nullptr;
	}

	// 이름을 안 주면 잠기지 않은 첫 축제를 쓴다. 콘솔에서 인자 없이 태워 볼 때의 경로다.
	const FFF_FestivalAtlasEntry* Entry = FestivalId.IsNone()
		? Catalog->FindFirstUnlocked()
		: Catalog->FindEntry(FestivalId);

	if (!Entry)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_UIFlowSubsystem: 카탈로그에 `%s` 가 없다 (항목 %d개). "
				 "DA_FestivalAtlas 의 FestivalId 를 확인한다."),
			FestivalId.IsNone() ? TEXT("(잠금 해제된 첫 항목)") : *FestivalId.ToString(),
			Catalog->Entries.Num());
		return nullptr;
	}

	// UI 가 잠긴 축제를 제시하지 않는 것이 원칙이지만, 콘솔 명령으로도 들어올 수 있어 여기서 막는다.
	if (!Entry->bUnlocked)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_UIFlowSubsystem: `%s` 는 잠겨 있다 — %s"),
			*Entry->FestivalId.ToString(),
			Entry->LockReason.IsEmpty() ? TEXT("(이유 없음)") : *Entry->LockReason.ToString());
		return nullptr;
	}

	if (Entry->Level.IsNull())
	{
		// 이 카탈로그의 존재 이유가 레벨 참조다. 비면 시작할 수 없다.
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_UIFlowSubsystem: `%s` 에 Festival Level 이 지정되지 않아 축제를 시작할 수 없다."),
			*Entry->FestivalId.ToString());
		return nullptr;
	}

	return Entry;
}

bool UFF_UIFlowSubsystem::StartFestival(FName FestivalId)
{
	const FFF_FestivalAtlasEntry* Entry = FindStartableEntry(FestivalId);
	if (!Entry)
	{
		// 실패하면 레벨을 열지 않는다. 이유는 FindStartableEntry 가 이미 남겼다.
		return false;
	}

	ActiveFestivalId = Entry->FestivalId;
	Mode = EFF_UIFlowMode::Festival;

	// Stage 를 여기서 세우지 않는다. OpenLevel 이 월드를 파괴하므로, 지금 RequestTransition 을
	// 부르면 죽어가는 월드의 위젯 위에 화면을 올리게 된다. 레벨이 열린 뒤에 세운다.
	bPendingFestivalStart = true;

	UE_LOG(LogFestaFiestaUI, Log,
		TEXT("UFF_UIFlowSubsystem: 축제 `%s` 시작 — 레벨 %s 를 연다."),
		*Entry->FestivalId.ToString(), *Entry->Level.ToSoftObjectPath().GetLongPackageName());

	UGameplayStatics::OpenLevelBySoftObjectPtr(this, Entry->Level);
	return true;
}

void UFF_UIFlowSubsystem::ReturnToAtlas()
{
	const UFF_UIFlowSettings* Settings = UFF_UIFlowSettings::Get();
	if (!Settings || Settings->FrontendLevel.IsNull())
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_UIFlowSubsystem::ReturnToAtlas: FrontendLevel 이 지정되지 않아 돌아갈 곳이 없다."));
		return;
	}

	// 한 사이클이 국가 지도에서 닫힌다. 타이틀로 되돌리지 않는다.
	Mode = EFF_UIFlowMode::Frontend;
	PendingFrontendScreen = EFF_UIFlowScreen::AtlasCountry;
	ActiveFestivalId = NAME_None;
	bPendingFestivalStart = false;
	ClearStageHudWidget();

	UE_LOG(LogFestaFiestaUI, Log, TEXT("UFF_UIFlowSubsystem: 로비로 돌아간다."));
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, Settings->FrontendLevel);
}

void UFF_UIFlowSubsystem::RequestAdvanceStage()
{
	UFF_FestivalStageSubsystem* Stage = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFF_FestivalStageSubsystem>()
		: nullptr;
	if (!Stage)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_UIFlowSubsystem::RequestAdvanceStage: Stage 서브시스템이 없다."));
		return;
	}

	// Report 는 타이머가 끝나도 자동 전환하지 않는다. 플레이어가 `다음 날` 을 눌러야 넘어간다.
	if (Stage->IsReportStage())
	{
		if (Stage->IsFinalDay())
		{
			// 마지막 날에는 다음 날이 없다. 축제 종료는 OnFestivalEnded 가 알린다.
			UE_LOG(LogFestaFiestaUI, Log,
				TEXT("UFF_UIFlowSubsystem: 마지막 날 Report 다. 다음 날로 갈 수 없다."));
			return;
		}

		Stage->AdvanceToNextDay();
		return;
	}

	if (Stage->IsCurrentStageSkippable())
	{
		Stage->SkipCurrentStage();
		return;
	}

	// 실패는 이유를 함께 남긴다. 버튼이 안 먹는 것처럼 보이는 상황을 설명해 준다.
	UE_LOG(LogFestaFiestaUI, Log,
		TEXT("UFF_UIFlowSubsystem: `%s` 는 스킵할 수 없는 단계다. 남은 시간 %.1f초를 기다린다."),
		*UEnum::GetValueAsString(Stage->CurrentStage), Stage->RemainingTime);
}

void UFF_UIFlowSubsystem::RequestAdvanceFlow()
{
	if (Mode == EFF_UIFlowMode::Festival)
	{
		RequestAdvanceStage();
		return;
	}

	switch (CurrentFrontendScreen)
	{
	case EFF_UIFlowScreen::Title:
		GoToScreen(EFF_UIFlowScreen::PlayHub);
		return;

	case EFF_UIFlowScreen::PlayHub:
		GoToScreen(EFF_UIFlowScreen::AtlasGlobe);
		return;

	case EFF_UIFlowScreen::AtlasGlobe:
		GoToScreen(EFF_UIFlowScreen::AtlasCountry);
		return;

	case EFF_UIFlowScreen::AtlasCountry:
		// 여기서 프론트엔드가 인게임으로 넘어간다. 이름을 주지 않으면 잠기지 않은 첫 축제를 쓴다.
		StartFestival(NAME_None);
		return;

	case EFF_UIFlowScreen::SaveList:
	case EFF_UIFlowScreen::Options:
		// 갈림길이라 `다음` 이 없다. 뒤로 간다.
		// 스택이 이전 화면을 소유하므로 여기서 히스토리를 따로 들지 않는다.
		// 스텁 단계에서는 표시 상태만 플레이 허브로 맞춘다 — 둘 다 거기서 들어오는 화면이다.
		// U2 에서 실제 화면이 각자 뒤로 가기를 갖게 되면 이 분기는 사라진다.
		GoBack();
		CurrentFrontendScreen = EFF_UIFlowScreen::PlayHub;
		return;

	default:
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_UIFlowSubsystem::RequestAdvanceFlow: 떠 있는 프론트엔드 화면이 없어 "
				 "다음으로 갈 수 없다. FF.Flow.Frontend 로 타이틀부터 시작한다."));
		return;
	}
}

void UFF_UIFlowSubsystem::HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage)
{
	// 🔴 프론트엔드에서는 Stage 전환을 버린다.
	// Stage 서브시스템은 CurrentStage 기본값이 Setup 이고 Tick 이 GameInstance 시작부터 돌아간다.
	// 이 가드가 없으면 타이틀 화면 위로 인게임 화면이 올라온다.
	if (Mode != EFF_UIFlowMode::Festival)
	{
		if (IsVerbose())
		{
			UE_LOG(LogFestaFiestaUI, Log,
				TEXT("UFF_UIFlowSubsystem: 축제 진행 중이 아니라 Stage 전환(%s -> %s)을 무시했다."),
				*UEnum::GetValueAsString(PreviousStage), *UEnum::GetValueAsString(NewStage));
		}
		return;
	}

	ApplyStageScreen(NewStage);
}

void UFF_UIFlowSubsystem::ApplyStageScreen(EFF_FestivalStage Stage)
{
	UFF_UIRootLayout* Layout = RootLayout.Get();
	if (!Layout)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_UIFlowSubsystem::ApplyStageScreen: RootLayout 이 없어 %s 화면을 올리지 못했다."),
			*UEnum::GetValueAsString(Stage));
		return;
	}

	const UFF_UIFlowSettings* Settings = UFF_UIFlowSettings::Get();
	const FFF_UIFlowStageScreen* Mapping = Settings ? Settings->StageScreens.Find(Stage) : nullptr;

	if (!Mapping || Mapping->WidgetClass.IsNull())
	{
		// SunsetTransition(노을 전환)처럼 문서에 없는 Stage 가 여기서 드러난다.
		// EFF_FestivalStage 는 코드에 8종인데 module-ownership.md 는 7종으로 적고 있다.
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_UIFlowSubsystem: Stage `%s` 에 배선된 화면이 없다. 화면이 바뀌지 않는다. "
				 "Project Settings > Game > FestaFiesta UI Flow > Stage Screens 를 채운다."),
			*UEnum::GetValueAsString(Stage));
		return;
	}

	UClass* WidgetClass = Mapping->WidgetClass.LoadSynchronous();
	if (!WidgetClass)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_UIFlowSubsystem: Stage `%s` 의 위젯 클래스를 로드할 수 없다 (%s)."),
			*UEnum::GetValueAsString(Stage), *Mapping->WidgetClass.ToString());
		return;
	}

	// **Stage 하나에 위젯 하나다.** 이전 것을 먼저 내린다 — 안 내리면 결산 시트가 쌓이고
	// 라이브 HUD 가 시트 아래에 살아남는다.
	ClearStageHudWidget();

	if (Mapping->Layer == EFF_UIFlowLayer::Screen)
	{
		if (!WidgetClass->IsChildOf(UFF_ActivatableScreenBase::StaticClass()))
		{
			// 화면 스택은 CommonUI 컨테이너라 일반 UUserWidget 을 받지 못한다.
			// 조용히 실패하면 "시트가 안 뜬다" 로만 드러난다.
			UE_LOG(LogFestaFiestaUI, Error,
				TEXT("UFF_UIFlowSubsystem: Stage `%s` 의 Layer 가 화면 스택인데 %s 는 "
					 "UFF_ActivatableScreenBase 파생이 아니다. Layer 를 HUD 로 바꾸거나 부모를 고친다."),
				*UEnum::GetValueAsString(Stage), *WidgetClass->GetName());
			return;
		}

		StageScreenWidget = Layout->PushScreen(WidgetClass);
	}
	else
	{
		UPanelWidget* HudLayer = Layout->GetHUDLayer();
		if (!HudLayer)
		{
			UE_LOG(LogFestaFiestaUI, Error,
				TEXT("UFF_UIFlowSubsystem: HUDLayer 바인딩이 없어 Stage `%s` 화면을 올리지 못했다."),
				*UEnum::GetValueAsString(Stage));
			return;
		}

		UUserWidget* Widget = CreateWidget<UUserWidget>(Layout->GetOwningPlayer(), WidgetClass);
		if (!Widget)
		{
			UE_LOG(LogFestaFiestaUI, Error,
				TEXT("UFF_UIFlowSubsystem: Stage `%s` 화면 생성에 실패했다 (%s)."),
				*UEnum::GetValueAsString(Stage), *WidgetClass->GetName());
			return;
		}

		// **Visible 로 두면 안 된다.** 화면 전체가 월드 클릭을 먹어 건축 배치가 죽는다.
		// 2026-08-12 에 WBP_TimeHud 에서 실제로 발생했다.
		Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		// 슬롯을 Fill 로 맞춘다. HUDLayer 는 Overlay 이고 기본 정렬이 좌상단이라,
		// 그대로 두면 하단에 붙어야 하는 준비·라이브 HUD 바가 화면 왼쪽 위에 뜬다.
		// 위젯이 자기 위치를 스스로 정할 수 있게 화면 전체를 준다.
		if (UOverlaySlot* HudSlot = Cast<UOverlaySlot>(HudLayer->AddChild(Widget)))
		{
			HudSlot->SetHorizontalAlignment(HAlign_Fill);
			HudSlot->SetVerticalAlignment(VAlign_Fill);
		}

		StageHudWidget = Widget;
	}

	if (IsVerbose())
	{
		UE_LOG(LogFestaFiestaUI, Log, TEXT("UFF_UIFlowSubsystem: Stage `%s` -> %s (%s)"),
			*UEnum::GetValueAsString(Stage),
			*WidgetClass->GetName(),
			Mapping->Layer == EFF_UIFlowLayer::Screen ? TEXT("화면 스택") : TEXT("HUD 레이어"));
	}
}

void UFF_UIFlowSubsystem::ClearStageHudWidget()
{
	// 해제는 조건부, 정리는 무조건이다.
	if (UUserWidget* Widget = StageHudWidget.Get())
	{
		Widget->RemoveFromParent();
	}
	StageHudWidget.Reset();

	if (UFF_ActivatableScreenBase* Screen = StageScreenWidget.Get())
	{
		// PopScreen 이 아니라 이것을 지정해 내린다. 최상단이 일시정지일 수 있다.
		if (UFF_UIRootLayout* Layout = RootLayout.Get())
		{
			Layout->RemoveScreen(Screen);
		}
	}
	StageScreenWidget.Reset();
}

void UFF_UIFlowSubsystem::HandleFestivalEnded()
{
	if (Mode != EFF_UIFlowMode::Festival)
	{
		return;
	}

	const UFF_UIFlowSettings* Settings = UFF_UIFlowSettings::Get();
	UFF_UIRootLayout* Layout = RootLayout.Get();

	if (!Settings || Settings->FinalReportScreenClass.IsNull() || !Layout)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_UIFlowSubsystem: 축제가 끝났지만 최종 결산 화면이 배선되지 않았다. "
				 "Project Settings > Game > FestaFiesta UI Flow > Final Report Screen Class 를 채운다."));
		return;
	}

	UClass* ScreenClass = Settings->FinalReportScreenClass.LoadSynchronous();
	if (!ScreenClass)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_UIFlowSubsystem: 최종 결산 화면 클래스를 로드할 수 없다 (%s)."),
			*Settings->FinalReportScreenClass.ToString());
		return;
	}

	// 일자별 결산 시트를 내리고 최종 결산을 올린다.
	ClearStageHudWidget();
	StageScreenWidget = Layout->PushScreen(ScreenClass);

	UE_LOG(LogFestaFiestaUI, Log, TEXT("UFF_UIFlowSubsystem: 축제 종료 — 최종 결산을 띄운다."));
}

// =============================================================================
// 콘솔 명령 — 개발용
//
// `GameDefaultMap` 을 바꾸지 않고 흐름을 태워 보기 위한 것이다. Config/DefaultEngine.ini 는
// 팀 공용이라 바꾸면 전원의 PIE 시작 지점이 달라진다.
// =============================================================================
#if !UE_BUILD_SHIPPING

namespace FFUIFlowConsole
{
	UFF_UIFlowSubsystem* Resolve(UWorld* World)
	{
		UFF_UIFlowSubsystem* Flow = UFF_UIFlowSubsystem::Get(World);
		if (!Flow)
		{
			UE_LOG(LogFestaFiestaUI, Error,
				TEXT("FF.Flow: 흐름 서브시스템을 찾을 수 없다. PIE 중인지 확인한다."));
		}
		return Flow;
	}

	void StartFestival(const TArray<FString>& Args, UWorld* World)
	{
		if (UFF_UIFlowSubsystem* Flow = Resolve(World))
		{
			// 인자가 없으면 잠기지 않은 첫 축제를 쓴다.
			const FName FestivalId = Args.Num() > 0 ? FName(*Args[0]) : NAME_None;
			Flow->StartFestival(FestivalId);
		}
	}

	void StartFrontend(const TArray<FString>& Args, UWorld* World)
	{
		if (UFF_UIFlowSubsystem* Flow = Resolve(World))
		{
			Flow->StartFrontend();
		}
	}

	void PrintStatus(const TArray<FString>& Args, UWorld* World)
	{
		if (const UFF_UIFlowSubsystem* Flow = Resolve(World))
		{
			UE_LOG(LogFestaFiestaUI, Display, TEXT("FF.Flow: %s"), *Flow->DescribeState());
		}
	}

	FAutoConsoleCommandWithWorldAndArgs StartFestivalCommand(
		TEXT("FF.Flow.Start"),
		TEXT("축제를 시작한다. FF.Flow.Start <FestivalId> — 인자를 빼면 잠기지 않은 첫 축제를 쓴다. "
			 "DA_FestivalAtlas 의 레벨을 연다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StartFestival));

	FAutoConsoleCommandWithWorldAndArgs StartFrontendCommand(
		TEXT("FF.Flow.Frontend"),
		TEXT("지금 레벨에서 타이틀 화면을 띄운다. FrontendLevel 이 아니어도 동작한다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StartFrontend));

	FAutoConsoleCommandWithWorldAndArgs StatusCommand(
		TEXT("FF.Flow.Status"),
		TEXT("지금 흐름이 어디에 있는지 한 줄로 찍는다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PrintStatus));
}

#endif // !UE_BUILD_SHIPPING
