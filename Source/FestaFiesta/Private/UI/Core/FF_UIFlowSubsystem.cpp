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
#include "MoviePlayer.h"
#include "Save/FF_SaveSubsystem.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/Frontend/FF_SaveListProvider.h"
#include "UI/Atlas/FF_AtlasCameraRig.h"
#include "UI/Atlas/FF_AtlasCountryScreen.h"
#include "UI/Atlas/FF_AtlasGlobeScreen.h"
#include "UI/Atlas/FF_FestivalAtlasCatalog.h"
#include "UI/Core/FF_UIFlowSettings.h"
#include "UI/Core/FF_LoadingScreenWidget.h"
#include "UI/Core/FF_UIRootLayout.h"
#include "UI/FF_ActivatableScreenBase.h"
#include "UI/FF_UILog.h"
#include "UI/Frontend/FF_OptionsScreen.h"
#include "UI/Frontend/FF_PlayHubScreen.h"
#include "UI/Frontend/FF_SaveListScreen.h"
#include "UI/Title/FF_TitleScreen.h"
#include "UObject/UObjectGlobals.h"

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

	// GameInstance를 함께 주는 델리게이트라 다른 PIE/월드 컨텍스트의 맵 로드를 가로채지 않는다.
	// 엔진 MoviePlayer의 PreLoadMap 핸들러보다 먼저 불려 실제 재생 전에 속성을 확정할 수 있다.
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(
		this, &UFF_UIFlowSubsystem::HandlePreLoadMapWithContext);

	if (IGameMoviePlayer* MoviePlayer = GetMoviePlayer())
	{
		MoviePlaybackFinishedHandle = MoviePlayer->OnMoviePlaybackFinished().AddUObject(
			this, &UFF_UIFlowSubsystem::HandleLoadingScreenFinished);
	}

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
	if (UFF_UIRootLayout* Layout = RootLayout.Get())
	{
		Layout->CancelAtlasTransition();
	}

	// **해제는 조건부, 정리는 무조건.** 여기서 조기 반환하면 아래 정리를 못 한다 (안티패턴 3).
	if (PostLoginHandle.IsValid())
	{
		FGameModeEvents::OnGameModePostLoginEvent().Remove(PostLoginHandle);
	}
	PostLoginHandle.Reset();

	if (PreLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PreLoadMapWithContext.Remove(PreLoadMapHandle);
	}
	PreLoadMapHandle.Reset();

	if (MoviePlaybackFinishedHandle.IsValid())
	{
		if (IGameMoviePlayer* MoviePlayer = GetMoviePlayer())
		{
			MoviePlayer->OnMoviePlaybackFinished().Remove(MoviePlaybackFinishedHandle);
		}
	}
	MoviePlaybackFinishedHandle.Reset();

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
	FrontendHistory.Reset();
	Mode = EFF_UIFlowMode::Idle;
	CurrentFrontendScreen = EFF_UIFlowScreen::None;
	ActiveFestivalId = NAME_None;
	ActiveCountryId = NAME_None;
	bPendingFestivalStart = false;
	PendingLoadingScreenClass = nullptr;
	PendingLoadingFestivalName = FText::GetEmpty();
	PendingLoadingMapPackage.Reset();
	ActiveLoadingScreenWidget = nullptr;

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

FText UFF_UIFlowSubsystem::GetActiveFestivalDisplayName() const
{
	if (ActiveFestivalId.IsNone())
	{
		return FText::GetEmpty();
	}

	const UFF_FestivalAtlasCatalog* Catalog = LoadCatalog();
	if (!Catalog)
	{
		// LoadCatalog 가 이미 이유를 남긴다. 여기서 또 찍으면 같은 줄이 두 번 나온다.
		return FText::GetEmpty();
	}

	const FFF_FestivalAtlasEntry* Entry = Catalog->FindEntry(ActiveFestivalId);
	if (!Entry)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_UIFlowSubsystem: 카탈로그에 `%s` 축제 항목이 없어 이름을 표시할 수 없다."),
			*ActiveFestivalId.ToString());
		return FText::GetEmpty();
	}

	return Entry->FestivalName;
}

FString UFF_UIFlowSubsystem::DescribeState() const
{
	return FString::Printf(
		TEXT("Mode=%s Frontend=%s Country=%s Festival=%s RootLayout=%s PendingStart=%s"),
		*UEnum::GetValueAsString(Mode),
		*UEnum::GetValueAsString(CurrentFrontendScreen),
		ActiveCountryId.IsNone() ? TEXT("(없음)") : *ActiveCountryId.ToString(),
		ActiveFestivalId.IsNone() ? TEXT("(없음)") : *ActiveFestivalId.ToString(),
		RootLayout.IsValid() ? TEXT("있음") : TEXT("없음"),
		bPendingFestivalStart ? TEXT("true") : TEXT("false"));
}

void UFF_UIFlowSubsystem::SetActiveCountry(FName CountryId)
{
	if (ActiveCountryId == CountryId)
	{
		// 같은 값이면 아무것도 하지 않는다. 지구본 화면이 핀을 옮길 때마다 부를 수 있다.
		return;
	}

	ActiveCountryId = CountryId;

	if (IsVerbose())
	{
		UE_LOG(LogFestaFiestaUI, Log, TEXT("UFF_UIFlowSubsystem: 국가 -> %s"),
			CountryId.IsNone() ? TEXT("(없음)") : *CountryId.ToString());
	}
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

			// 🔴 **여기서 축제를 처음부터 되돌린다.**
			//
			// Stage 서브시스템은 `UGameInstanceSubsystem` 이라 레벨을 다시 열어도 살아남는다.
			// 그래서 3일차를 끝내고 나라 지도로 나갔다가 같은 축제를 다시 고르면
			// **3일차 셋업부터 시작했다** (2026-09-03 백종태 실기 확인).
			//
			// 초기화는 **들어오는 쪽**이 한다. 나가는 쪽(`ReturnToAtlas`)에 두면 게임을 끄거나
			// 다른 경로로 나갔을 때 빠지고, 다음 축제가 이전 상태를 물려받는다.
			//
			// ★ 이건 남의 모듈을 고치는 것이 아니라 **공개 API 를 쓰는 것**이다.
			//   `StartNewFestival()` 은 강민서 님이 이미 만들어 두셨다 (예전 주석은 "없다"고
			//   되어 있었는데 그 사이에 생겼다). 경제도 이 신호를 구독해
			//   `RestartFestival()` 로 함께 되돌아간다.
			Stage->StartNewFestival();

			// RequestTransition 이 OnStageChanged 를 울리고, 그 핸들러가 화면을 올린다.
			// `StartNewFestival()` 은 `OnFestivalStarted` 만 쏘고 단계 전환은 알리지 않으므로
			// 이 줄이 없으면 상태만 1일차가 되고 **화면이 안 뜬다.**
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

		// 🔴 **국가 지도는 혼자 서지 못한다.** 뒤로 가기가 CommonUI 스택을 쓰는데
		//    (`HandleFrontendBackRequested` → `GoBack()`), 국가 지도만 올려 두면 스택에
		//    그것 하나뿐이라 **뒤로 누르는 순간 스택이 비고 화면이 통째로 사라진다.**
		//
		//    실기에서는 "축제를 끝내고 나온 뒤 지구본을 눌렀더니 지구는 보이는데 힌트도
		//    핀도 없고 드래그도 안 된다" 로 나타났다 (2026-09-03). 그때 보이던 지구는
		//    **월드의 지구 액터**였고 그 위에 위젯이 하나도 없었다 — 그래서 입력도
		//    아무 데도 안 갔다.
		//
		//    평소 경로(지구본 → 나라 클릭 → 국가 지도)와 같은 스택을 만들어 준다.
		if (PendingFrontendScreen == EFF_UIFlowScreen::AtlasCountry)
		{
			// 스택 크로스페이드(0.4초)가 돌면 지구본 위젯이 잠깐 비친다. 두 번의 push 모두 끈다.
			TGuardValue<bool> NoStackFade(bPushScreensWithoutTransition, true);
			GoToScreen(EFF_UIFlowScreen::AtlasGlobe);

			// ★ 지구본은 **스택 바닥을 만들기 위해서만** 올린다. 보여 주려는 것이 아니다.
			//   이 줄이 없으면 카메라가 지구본 → 국가로 **블렌드**해서, 축제를 끝내고
			//   나올 때마다 지구가 잠깐 돌아가는 모습이 스쳤다 (2026-09-03 백종태 지적).
			//   레벨에 막 들어온 참이라 블렌드할 `이전 모습`도 없다 — 바로 국가로 간다.
			bInstantCameraForNextScreen = true;
			GoToScreen(EFF_UIFlowScreen::AtlasCountry);
		}
		else
		{
			GoToScreen(PendingFrontendScreen);
		}

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
	if (UFF_UIRootLayout* PreviousLayout = RootLayout.Get())
	{
		PreviousLayout->CancelAtlasTransition();
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

	// 새 레이아웃은 새 화면 세션이다. 이전 레벨의 히스토리를 물고 가면
	// 뒤로 가기가 이미 죽은 화면을 가리킨다.
	FrontendHistory.Reset();

	/* 현재 화면 표시도 같이 비운다. 새 레이아웃의 스택은 비어 있는데 이 값만 이전 레벨의
	   AtlasCountry 로 남아 있으면, 축제(튜토리얼 포함)를 끝내고 돌아왔을 때 HandlePostLogin 이
	   바닥으로 까는 지구본을 "국가 지도 -> 지구본" 뒤로 가기로 오해한다. 그러면 암전 전환이
	   걸리고, 그 전환 중이라 바로 뒤의 국가 지도 요청이 버려져 지구본에서 멈췄다
	   (2026-09-29 로그: 로비로 돌아간다 (국가 KR) 뒤에 AtlasGlobe 만 뜨고 AtlasCountry 가 없음).
	   None 이면 첫 화면으로 취급돼 카메라도 즉시 맞춰지고 히스토리에도 안 남는다.
	*/
	CurrentFrontendScreen = EFF_UIFlowScreen::None;

	if (IsVerbose())
	{
		UE_LOG(LogFestaFiestaUI, Log,
			TEXT("UFF_UIFlowSubsystem: RootLayout 을 뷰포트에 올렸다 (%s)."), *LayoutClass->GetName());
	}
	return true;
}

void UFF_UIFlowSubsystem::StartFrontend()
{
	if (UFF_UIRootLayout* Layout = RootLayout.Get())
	{
		Layout->CancelAtlasTransition();
	}
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
	if (UFF_UIRootLayout* Layout = RootLayout.Get(); Layout && Layout->IsAtlasTransitionActive())
	{
		// 연타/외부 호출이 암전 도중 화면을 한 번 더 쌓지 않게 한다.
		return;
	}
	if (!BeginAtlasScreenTransition(Screen, false))
	{
		GoToScreenImmediate(Screen);
	}
}

bool UFF_UIFlowSubsystem::BeginAtlasScreenTransition(EFF_UIFlowScreen Destination, bool bGoingBack)
{
	const bool bAtlasPair =
		(CurrentFrontendScreen == EFF_UIFlowScreen::AtlasGlobe && Destination == EFF_UIFlowScreen::AtlasCountry)
		|| (CurrentFrontendScreen == EFF_UIFlowScreen::AtlasCountry && Destination == EFF_UIFlowScreen::AtlasGlobe);
	UFF_UIRootLayout* Layout = RootLayout.Get();
	// 레벨 재진입 시 스택 바닥용 지구본은 실제 이전 화면이 아니다.
	if (Mode != EFF_UIFlowMode::Frontend || !bAtlasPair || !Layout || bInstantCameraForNextScreen)
	{
		return false;
	}

	const TWeakObjectPtr<UFF_UIRootLayout> ExpectedLayout(Layout);
	const TWeakObjectPtr<UFF_UIFlowSubsystem> WeakThis(this);
	return Layout->BeginAtlasTransition(
		FSimpleDelegate::CreateWeakLambda(this, [this, ExpectedLayout, Destination, bGoingBack]()
		{
			if (Mode != EFF_UIFlowMode::Frontend || RootLayout != ExpectedLayout)
			{
				if (UFF_UIRootLayout* PreviousLayout = ExpectedLayout.Get())
				{
					PreviousLayout->CancelAtlasTransition();
				}
				return;
			}
			// 비활성화 때 지구 중심 구도가 복원되는 프레임도 암전 안에 넣는다.
			// 카메라는 가려진 동안 바로 맞추고, 스택 전환/새 지도 배치를 기다린다.
			TGuardValue<bool> InstantCameraGuard(bInstantCameraForNextScreen, true);
			if (bGoingBack)
			{
				GoBackImmediate();
			}
			else
			{
				GoToScreenImmediate(Destination);
			}
		}),
		[WeakThis, ExpectedLayout]()
		{
			const UFF_UIFlowSubsystem* Flow = WeakThis.Get();
			const UFF_UIRootLayout* CurrentLayout = ExpectedLayout.Get();
			return !Flow || !CurrentLayout || Flow->RootLayout != ExpectedLayout
				|| !CurrentLayout->IsScreenStackTransitioning();
		});
}

void UFF_UIFlowSubsystem::GoToScreenImmediate(EFF_UIFlowScreen Screen)
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

	// !! 밀어 넣기 **전에** 갱신한다 !!
	// PushScreen 이 위젯을 만드는 도중 NativeConstruct 가 돌면서 이 값을 읽는다.
	// 뒤에서 갱신하면 새 화면이 **직전 화면 이름**을 표시한다 —
	// 2026-08-20 에 플레이 허브 스텁에 `타이틀` 이 찍혔다.
	const EFF_UIFlowScreen PreviousScreen = CurrentFrontendScreen;
	CurrentFrontendScreen = Screen;

	UFF_ActivatableScreenBase* Pushed = bPushScreensWithoutTransition
		? Layout->PushScreenWithoutTransition(ScreenClass)
		: Layout->PushScreen(ScreenClass);
	if (!Pushed)
	{
		// 올리지 못했으면 되돌린다. 안 그러면 뜨지도 않은 화면이 현재 화면으로 남는다.
		CurrentFrontendScreen = PreviousScreen;

		// PushScreen 이 실패 이유를 이미 남긴다. 여기서는 어느 화면이었는지만 덧붙인다.
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_UIFlowSubsystem::GoToScreen: %s 를 화면 스택에 올리지 못했다."),
			*UEnum::GetValueAsString(Screen));
		return;
	}

	// 성공한 뒤에 기억한다. 실패한 전환을 히스토리에 남기면 뒤로 가기가 엉뚱한 곳으로 간다.
	if (PreviousScreen != EFF_UIFlowScreen::None)
	{
		FrontendHistory.Push(PreviousScreen);
	}

	// 타이틀은 자기가 다음 화면을 모른다. 그게 설계다 — 축제가 늘 때마다 타이틀을 고치지 않게 한다.
	// 그래서 화면을 띄운 쪽인 여기가 델리게이트를 받는다.
	// 델리게이트는 **화면이** 소유한다. 화면이 죽으면 함께 사라지므로 여기서 추적하거나
	// 해제하지 않아도 된다. 구독자(this)가 화면보다 오래 사는 방향이라 안전하다.
	if (UFF_TitleScreen* Title = Cast<UFF_TitleScreen>(Pushed))
	{
		TitleScreen = Title;
		Title->OnPlayRequested.AddUniqueDynamic(this, &UFF_UIFlowSubsystem::HandleTitlePlayRequested);
		Title->OnOptionsRequested.AddUniqueDynamic(this, &UFF_UIFlowSubsystem::HandleTitleOptionsRequested);
	}
	else if (UFF_PlayHubScreen* PlayHub = Cast<UFF_PlayHubScreen>(Pushed))
	{
		PlayHub->OnContinueRequested.AddUniqueDynamic(this, &UFF_UIFlowSubsystem::HandlePlayHubContinueRequested);
		PlayHub->OnNewFestivalRequested.AddUniqueDynamic(this, &UFF_UIFlowSubsystem::HandlePlayHubNewFestivalRequested);
		PlayHub->OnSaveListRequested.AddUniqueDynamic(this, &UFF_UIFlowSubsystem::HandlePlayHubSaveListRequested);
		PlayHub->OnBackRequested.AddUniqueDynamic(this, &UFF_UIFlowSubsystem::HandleFrontendBackRequested);
	}
	else if (UFF_SaveListScreen* SaveList = Cast<UFF_SaveListScreen>(Pushed))
	{
		SaveList->OnBackRequested.AddUniqueDynamic(this, &UFF_UIFlowSubsystem::HandleFrontendBackRequested);
		SaveList->OnSlotChosen.AddUniqueDynamic(this, &UFF_UIFlowSubsystem::HandleSaveListSlotChosen);
	}
	else if (UFF_OptionsScreen* Options = Cast<UFF_OptionsScreen>(Pushed))
	{
		Options->OnBackRequested.AddUniqueDynamic(this, &UFF_UIFlowSubsystem::HandleFrontendBackRequested);
	}
	else if (UFF_AtlasGlobeScreen* Globe = Cast<UFF_AtlasGlobeScreen>(Pushed))
	{
		Globe->OnBrowseFestivalsRequested.AddUniqueDynamic(
			this, &UFF_UIFlowSubsystem::HandleAtlasBrowseFestivals);
		Globe->OnBackRequested.AddUniqueDynamic(this, &UFF_UIFlowSubsystem::HandleFrontendBackRequested);
	}
	else if (UFF_AtlasCountryScreen* Country = Cast<UFF_AtlasCountryScreen>(Pushed))
	{
		Country->OnStartFestivalRequested.AddUniqueDynamic(
			this, &UFF_UIFlowSubsystem::HandleAtlasStartFestival);
		Country->OnBackRequested.AddUniqueDynamic(this, &UFF_UIFlowSubsystem::HandleFrontendBackRequested);

		// 어느 나라의 지도인지 알려준다. 화면이 흐름을 되묻지 않게 하는 방향이다 —
		// 위젯이 서브시스템을 조회하기 시작하면 의존이 양방향이 된다.
		if (ActiveCountryId.IsNone())
		{
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("UFF_UIFlowSubsystem: 국가 지도를 띄우는데 ActiveCountryId 가 없다. "
					 "지구본에서 나라를 고르지 않고 바로 들어온 경로다 — 빈 지도가 뜬다."));
		}
		Country->SetCountry(ActiveCountryId);
	}

	// 카메라 연출. 리그는 **없어도 되는 것**이라 못 찾아도 조용히 지나간다 (경고를 끈다) —
	// 화면이 바뀔 때마다 부르므로 리그 없는 레벨에서 경고가 쌓이면 로그가 쓸모없어진다.
	//
	// 레벨이 막 열려 첫 화면을 올리는 경우(`PreviousScreen == None`)는 **즉시**다.
	// 섞을 이전 구도가 없는데 블렌드를 걸면 엉뚱한 곳에서 날아온다.
	if (AFF_AtlasCameraRig* CameraRig = AFF_AtlasCameraRig::Find(this, /*bLogIfMissing=*/false))
	{
		// 🔴 레벨에 막 들어온 첫 화면은 블렌드할 `이전 모습`이 없으므로 즉시 맞춘다.
		//    `bInstantCameraForNextScreen` 은 그 예외를 **한 번 더** 쓰는 자리다 —
		//    아래 `아틀라스 스택 복원` 주석 참고.
		const bool bInstant =
			(PreviousScreen == EFF_UIFlowScreen::None) || bInstantCameraForNextScreen;
		bInstantCameraForNextScreen = false;

		CameraRig->ViewForScreen(Screen, bInstant);
	}

	if (IsVerbose())
	{
		UE_LOG(LogFestaFiestaUI, Log, TEXT("UFF_UIFlowSubsystem: 프론트엔드 화면 -> %s"),
			*UEnum::GetValueAsString(Screen));
	}
}

bool UFF_UIFlowSubsystem::GoBack()
{
	if (UFF_UIRootLayout* Layout = RootLayout.Get(); Layout && Layout->IsAtlasTransitionActive())
	{
		// 전환 중 뒤로 입력은 소비한다. 동일 프레임의 중복 Pop을 막는다.
		return true;
	}
	const EFF_UIFlowScreen Destination = FrontendHistory.IsEmpty()
		? EFF_UIFlowScreen::None : FrontendHistory.Last();
	return BeginAtlasScreenTransition(Destination, true) || GoBackImmediate();
}

bool UFF_UIFlowSubsystem::GoBackImmediate()
{
	UFF_UIRootLayout* Layout = RootLayout.Get();
	if (!Layout || !Layout->PopScreen())
	{
		return false;
	}

	// 스택이 위젯을 되살렸으므로 우리 쪽 표시 상태도 함께 되돌린다.
	CurrentFrontendScreen = FrontendHistory.Num() > 0
		? FrontendHistory.Pop()
		: EFF_UIFlowScreen::None;

	// 뒤로 갈 때 카메라도 되돌아간다. **지구 각도와 국가 선택은 유지한다** —
	// 그건 지구 액터가 들고 있고 여기서 건드리지 않는다 (리셋하면 답답하다).
	if (AFF_AtlasCameraRig* CameraRig = AFF_AtlasCameraRig::Find(this, /*bLogIfMissing=*/false))
	{
		CameraRig->ViewForScreen(CurrentFrontendScreen, bInstantCameraForNextScreen);
	}

	return true;
}

void UFF_UIFlowSubsystem::HandleTitlePlayRequested()
{
	GoToScreen(EFF_UIFlowScreen::PlayHub);
}

void UFF_UIFlowSubsystem::HandleTitleOptionsRequested()
{
	GoToScreen(EFF_UIFlowScreen::Options);
}

void UFF_UIFlowSubsystem::HandlePlayHubContinueRequested()
{
	// 플레이 허브가 저장이 없으면 이 버튼을 잠그므로 보통 빈 목록으로 오지 않는다.
	const UFF_SaveListProvider* Provider = UFF_SaveListProvider::Get(this);

	FFF_SaveSlotEntry Recent;
	if (!Provider || !Provider->GetMostRecent(Recent))
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_UIFlowSubsystem: `계속하기` 를 눌렀지만 저장 기록이 없다."));
		return;
	}

	ContinueFromSlot(Recent.SlotId);
}

void UFF_UIFlowSubsystem::HandleSaveListSlotChosen(const FName SlotId)
{
	ContinueFromSlot(SlotId);
}

void UFF_UIFlowSubsystem::ContinueFromSlot(const FName SlotId)
{
	UFF_SaveSubsystem* Save = UFF_SaveSubsystem::Get(this);
	if (!Save)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_UIFlowSubsystem: 저장 시스템이 없어 이어할 수 없다."));
		return;
	}

	/* 슬롯에 담긴 축제의 레벨을 연다. 여기서 곧바로 불러오지 않는다.

	   복원은 레벨이 뜨고 모든 시스템이 준비된 뒤에야 의미가 있고, 그 시점은
	   GameMode 가 안다(BeginPlay 에서 TryContinueFestival 을 부른다).
	   여기서 하는 일은 어느 슬롯인지 넘겨두고 레벨을 여는 것뿐이다.
	*/
	/* 로비 세이브는 레벨을 열지 않는다. 그 판의 기록을 되돌리고 그 나라의 국가 지도로 간다.

	   축제를 끝내면 남는 자동 저장이 이것이다. 축제 정보 없이 나라만 들고 있어서, 아래
	   축제 경로로 보내면 "축제 정보가 없다" 로 조용히 끝났다. 목록에서 눌러도 반응이
	   없던 옛 기록의 정체다(피드백: 이어하기가 방금 것만 된다).

	   지구본을 먼저 깔고 국가 지도를 올리는 이유는 축제를 끝내고 돌아올 때(프론트엔드 레벨
	   진입)와 같다. 뒤로 가기가 지구본으로 가야 하고, 두 화면 사이 카메라 블렌드는 건너뛴다.
	*/
	if (Save->IsLobbySlot(SlotId))
	{
		FName CountryId = NAME_None;
		if (!Save->LoadLobbySave(SlotId, CountryId))
		{
			return;
		}

		if (CountryId.IsNone())
		{
			// 나라를 모르는 옛 로비 세이브다. 기록은 되돌렸으니 나라부터 고르게 한다.
			GoToScreen(EFF_UIFlowScreen::AtlasGlobe);
			return;
		}

		SetActiveCountry(CountryId);
		// 레벨 진입 복원과 같은 이유로 지구본이 비치지 않게 두 push 의 크로스페이드를 끈다.
		TGuardValue<bool> NoStackFade(bPushScreensWithoutTransition, true);
		GoToScreen(EFF_UIFlowScreen::AtlasGlobe);
		bInstantCameraForNextScreen = true;
		GoToScreen(EFF_UIFlowScreen::AtlasCountry);
		return;
	}

	const FName FestivalId = Save->PeekFestivalIdInSlot(SlotId);
	if (FestivalId.IsNone())
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_UIFlowSubsystem: 슬롯 `%s` 에 축제 정보가 없어 이어할 수 없다."),
			*SlotId.ToString());
		return;
	}

	Save->RequestContinueFromSlot(SlotId);

	if (!StartFestival(FestivalId))
	{
		// 레벨을 못 열었으면 지정도 물린다. 남겨두면 다음에 새로 시작해도 이 슬롯을 문다.
		Save->RequestContinueFromSlot(NAME_None);

		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_UIFlowSubsystem: `%s` 레벨을 열지 못해 이어하기를 취소한다."),
			*FestivalId.ToString());
	}
}

void UFF_UIFlowSubsystem::HandlePlayHubNewFestivalRequested()
{
	GoToScreen(EFF_UIFlowScreen::AtlasGlobe);
}

void UFF_UIFlowSubsystem::HandlePlayHubSaveListRequested()
{
	GoToScreen(EFF_UIFlowScreen::SaveList);
}

void UFF_UIFlowSubsystem::HandleFrontendBackRequested()
{
	// 스택이 이전 화면을 소유하므로 여기서 히스토리를 따로 들지 않는다.
	// 닫을 것이 없으면 GoBack 이 false 를 돌려주고 아무 일도 하지 않는다.
	if (!GoBack())
	{
		UE_LOG(LogFestaFiestaUI, Log,
			TEXT("UFF_UIFlowSubsystem: 뒤로 갈 화면이 없다. 스택이 비어 있다."));
	}
}

void UFF_UIFlowSubsystem::HandleAtlasBrowseFestivals(FName CountryId)
{
	if (UFF_UIRootLayout* Layout = RootLayout.Get(); Layout && Layout->IsAtlasTransitionActive())
	{
		return;
	}
	if (CountryId.IsNone())
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_UIFlowSubsystem: 국가 없이 `축제 둘러보기` 가 들어왔다. 넘어가지 않는다."));
		return;
	}

	// 국가를 먼저 기억한다. `GoToScreen` 이 국가 지도를 올리면서 이 값을 화면에 넣는다 —
	// 순서가 반대면 빈 지도가 한 프레임 보인다.
	SetActiveCountry(CountryId);
	GoToScreen(EFF_UIFlowScreen::AtlasCountry);
}

void UFF_UIFlowSubsystem::HandleAtlasStartFestival(FName FestivalId)
{
	// 여기서 프론트엔드가 인게임으로 넘어간다. 실패하면 `StartFestival` 이 이유를 남기고
	// 레벨을 열지 않으므로 화면은 그대로 국가 지도에 남는다.
	StartFestival(FestivalId);
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
	if (UFF_UIRootLayout* Layout = RootLayout.Get(); Layout && Layout->IsAtlasTransitionActive())
	{
		return false;
	}
	const FFF_FestivalAtlasEntry* Entry = FindStartableEntry(FestivalId);
	if (!Entry)
	{
		// 실패하면 레벨을 열지 않는다. 이유는 FindStartableEntry 가 이미 남겼다.
		return false;
	}

	ActiveFestivalId = Entry->FestivalId;
	Mode = EFF_UIFlowMode::Festival;

	// 축제가 속한 국가를 함께 기억한다. 최종 결산에서 `로비로` 를 누르면 그 나라의 지도로
	// 돌아가야 하는데, 그때 카탈로그를 다시 뒤져 역추적하면 CountryId 가 빈 항목에서 실패한다.
	// 콘솔로 시작한 경우에도 여기서 자동으로 채워진다.
	if (!Entry->CountryId.IsNone())
	{
		SetActiveCountry(Entry->CountryId);
	}

	// PreLoadMap 안에서 soft class를 처음 읽으면 로딩 화면을 준비하는 순간 자체가 무화면 hitch가 된다.
	// 목적지와 표시 문맥을 지금 복사해 두고, 맵 로드 훅에서는 위젯 조립만 한다.
	const UFF_UIFlowSettings* Settings = UFF_UIFlowSettings::Get();
	PendingLoadingMapPackage = Entry->Level.ToSoftObjectPath().GetLongPackageName();
	PendingLoadingFestivalName = Entry->FestivalName;
	PendingLoadingScreenClass = nullptr;

	if (Settings && !Settings->LoadingScreenClass.IsNull())
	{
		PendingLoadingScreenClass = Settings->LoadingScreenClass.LoadSynchronous();
		if (!PendingLoadingScreenClass)
		{
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("UFF_UIFlowSubsystem: LoadingScreenClass를 로드하지 못했다 (%s). 로딩 화면 없이 계속한다."),
				*Settings->LoadingScreenClass.ToString());
		}
	}
	else
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_UIFlowSubsystem: LoadingScreenClass가 비어 있어 로딩 화면 없이 축제 레벨을 연다."));
	}

	// Stage 를 여기서 세우지 않는다. OpenLevel 이 월드를 파괴하므로, 지금 RequestTransition 을
	// 부르면 죽어가는 월드의 위젯 위에 화면을 올리게 된다. 레벨이 열린 뒤에 세운다.
	bPendingFestivalStart = true;

	UE_LOG(LogFestaFiestaUI, Log,
		TEXT("UFF_UIFlowSubsystem: 축제 `%s` 시작 — 레벨 %s 를 연다."),
		*Entry->FestivalId.ToString(), *Entry->Level.ToSoftObjectPath().GetLongPackageName());

	UGameplayStatics::OpenLevelBySoftObjectPtr(this, Entry->Level);
	return true;
}

void UFF_UIFlowSubsystem::HandlePreLoadMapWithContext(
	const FWorldContext& WorldContext,
	const FString& MapName)
{
	if (!bPendingFestivalStart || PendingLoadingMapPackage.IsEmpty())
	{
		return;
	}

	// 다중 PIE나 다른 GameInstance의 여행은 건드리지 않는다.
	if (WorldContext.OwningGameInstance.Get() != GetGameInstance())
	{
		return;
	}

	const FString NormalizedMapName = UWorld::RemovePIEPrefix(MapName);
	if (NormalizedMapName != PendingLoadingMapPackage)
	{
		return;
	}

	// 목적지 하나에 한 번만 소비한다. bPendingFestivalStart는 새 월드의 PostLogin까지 유지해야 한다.
	PendingLoadingMapPackage.Reset();
	TSubclassOf<UFF_LoadingScreenWidget> LoadingClass = PendingLoadingScreenClass;
	PendingLoadingScreenClass = nullptr;

	if (!LoadingClass)
	{
		PendingLoadingFestivalName = FText::GetEmpty();
		return;
	}

	// UE 5.7의 MoviePlayer는 PIE/commandlet/dedicated/null RHI/-NoLoadingScreen에서 비활성이다.
	// 그 환경에서 안 보이는 것은 실패가 아니며 레벨 로드는 그대로 진행한다.
	if (!IsMoviePlayerEnabled())
	{
		PendingLoadingFestivalName = FText::GetEmpty();
		return;
	}

	IGameMoviePlayer* MoviePlayer = GetMoviePlayer();
	UGameInstance* GameInstance = GetGameInstance();
	if (!MoviePlayer || !MoviePlayer->IsInitialized() || !GameInstance)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_UIFlowSubsystem: MoviePlayer가 준비되지 않아 로딩 화면 없이 계속한다."));
		PendingLoadingFestivalName = FText::GetEmpty();
		return;
	}

	// Owner는 반드시 GameInstance다. Subsystem은 CreateWidget이 허용하는 owner 타입이 아니며,
	// GameInstance에 두어야 월드가 파괴되는 여행 중에도 UUserWidget이 살아남는다.
	ActiveLoadingScreenWidget = CreateWidget<UFF_LoadingScreenWidget>(GameInstance, LoadingClass);
	if (!ActiveLoadingScreenWidget)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_UIFlowSubsystem: 로딩 위젯 생성 실패 (%s)."), *LoadingClass->GetName());
		PendingLoadingFestivalName = FText::GetEmpty();
		return;
	}

	ActiveLoadingScreenWidget->SetLoadingContext(PendingLoadingFestivalName);

	const UFF_UIFlowSettings* Settings = UFF_UIFlowSettings::Get();
	FLoadingScreenAttributes Attributes;
	Attributes.WidgetLoadingScreen = ActiveLoadingScreenWidget->TakeWidget();
	Attributes.MinimumLoadingScreenDisplayTime = Settings
		? FMath::Max(0.f, Settings->MinimumLoadingScreenDisplayTime)
		: 0.75f;
	Attributes.bAutoCompleteWhenLoadingCompletes = true;
	Attributes.bMoviesAreSkippable = false;
	Attributes.bWaitForManualStop = false;
	Attributes.bAllowInEarlyStartup = false;
	Attributes.bAllowEngineTick = false;

	MoviePlayer->SetupLoadingScreen(Attributes);
}

void UFF_UIFlowSubsystem::HandleLoadingScreenFinished()
{
	ActiveLoadingScreenWidget = nullptr;
	PendingLoadingFestivalName = FText::GetEmpty();
}

void UFF_UIFlowSubsystem::ReturnToAtlas()
{
	if (UFF_UIRootLayout* Layout = RootLayout.Get())
	{
		Layout->CancelAtlasTransition();
	}
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

	// ★ `ActiveCountryId` 는 **비우지 않는다.** 국가 지도로 돌아가는데 어느 나라인지
	//   잃으면 빈 지도가 뜬다. 확정 요구사항이 "뒤로 갈 때 국가 선택을 유지한다" 다.
	if (ActiveCountryId.IsNone())
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_UIFlowSubsystem::ReturnToAtlas: 국가 지도로 돌아가는데 ActiveCountryId 가 없다. "
				 "축제 항목의 CountryId 가 비어 있었을 수 있다 — 국가 지도가 무엇을 그릴지 모른다."));
	}

	UE_LOG(LogFestaFiestaUI, Log, TEXT("UFF_UIFlowSubsystem: 로비로 돌아간다 (국가 %s)."),
		ActiveCountryId.IsNone() ? TEXT("(없음)") : *ActiveCountryId.ToString());
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
			/* 마지막 날에는 다음 날이 없다. 한 사이클을 닫아 로비로 보낸다.

			   예전에는 여기서 로그만 남기고 돌아갔다. 최종 결산 화면이 제대로 붙기
			   전까지는 그게 "버튼을 눌러도 아무 일도 없고 나갈 데도 없는" 상태였다.
			   결산 화면이 붙으면 그 화면의 `로비로` 가 이 역할을 가져간다.
			*/
			UE_LOG(LogFestaFiestaUI, Log,
				TEXT("UFF_UIFlowSubsystem: 마지막 날 Report 다. 로비로 돌아간다."));

			ReturnToAtlas();
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
		// 히스토리가 이전 화면을 알고 있으므로 여기서 짐작하지 않는다.
		GoBack();
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
		// 🔴 **배선이 없으면 이전 화면을 내린다.** 예전에는 그냥 return 했는데, 그러면
		// 앞 단계의 HUD 가 그대로 남아 "노을 전환인데 준비 단계 바가 떠 있는" 화면이 된다
		// (2026-08-27 확인). `배선 없음` 은 곧 `이 단계에는 HUD 가 없다` 로 다룬다.
		//
		// 노을 전환이 그 자리다 — 20초짜리 순수 연출이라 HUD 를 띄우지 않는다.
		ClearStageHudWidget();

		UE_LOG(LogFestaFiestaUI, Log,
			TEXT("UFF_UIFlowSubsystem: Stage `%s` 에 배선된 화면이 없어 HUD 를 내렸다. "
				 "의도한 것이 아니면 Project Settings > Game > FestaFiesta UI Flow > Stage Screens 를 채운다."),
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
