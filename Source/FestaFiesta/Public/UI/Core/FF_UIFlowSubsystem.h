// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Time/FF_FestivalStage.h"
#include "UI/Core/FF_UIFlowTypes.h"
#include "FF_UIFlowSubsystem.generated.h"

class AGameModeBase;
class APlayerController;
class UFF_FestivalAtlasCatalog;
class UFF_ActivatableScreenBase;
class UFF_LoadingScreenWidget;
class UFF_TitleIntroScreen;
class UFF_TitleScreen;
class UFF_UIRootLayout;
class UUserWidget;
struct FFF_FestivalAtlasEntry;
struct FWorldContext;

/** 흐름이 지금 어디에 있는지. 프론트엔드에서 Stage 전환을 무시하기 위한 구분이다. */
UENUM(BlueprintType)
enum class EFF_UIFlowMode : uint8
{
	/** 아직 아무것도 시작하지 않았다. 남의 테스트 레벨에서 PIE 를 눌렀을 때가 여기다. */
	Idle		UMETA(DisplayName = "대기"),

	/** 타이틀·플레이 허브·지구본·국가 지도. 축제가 시작되기 전이다. */
	Frontend	UMETA(DisplayName = "프론트엔드"),

	/** 축제 레벨에 들어왔다. 이때만 Stage 전환에 반응한다. */
	Festival	UMETA(DisplayName = "축제 진행")
};

/**
 * UI 흐름의 주인. 타이틀부터 최종 결산까지 화면을 올리고 내린다.
 *
 * === 이 클래스가 왜 필요했나 ===
 * `UFF_UIRootLayout` 은 C++ 과 `WBP_FF_UIRootLayout` 이 이미 있었는데 **그것을 만들어 뷰포트에
 * 올리는 코드가 어디에도 없었다.** 타이틀이 뜨던 것은 `Test_Main` 레벨 블루프린트 배선 덕분이라
 * 다른 레벨에서는 아무것도 뜨지 않았다. 그 빈자리를 채운다.
 *
 * === 왜 GameInstanceSubsystem 인가 ===
 * Stage · Progression · ViewModel 세 개가 전부 `UGameInstanceSubsystem` 이라 레벨을 넘어 산다.
 * 흐름 소유자가 World 수명이면 `OpenLevel` 직후 자기 상태를 잃는다.
 * **위젯은 월드와 함께 죽으므로** 캐시는 `TWeakObjectPtr` 로 들고 레벨이 열릴 때마다 다시 만든다.
 *
 * === 남의 파일을 고치지 않고 플레이어에 붙는 방법 ===
 * `FGameModeEvents::OnGameModePostLoginEvent()` 에 바인딩한다. 레벨이 열릴 때마다 자동으로 다시
 * 불리므로 travel 후 재생성이 공짜로 해결된다. `AFF_FestivalPlayerController::BeginPlay` 에
 * 한 줄 넣는 방법도 되지만 그 파일은 카메라 담당 경계다 (장원재·박건영).
 *
 * === 🔴 프론트엔드에서 Stage 전환을 무시한다 ===
 * `UFF_FestivalStageSubsystem` 은 `CurrentStage` 기본값이 `Setup` 이고 `Tick` 이 GameInstance
 * 시작부터 돌아간다. **타이틀 화면에 앉아 있는 동안 축제 시계가 이미 진행된다.**
 * 그래서 `Mode == Festival` 이 아니면 `OnStageChanged` 를 버린다. 안 그러면 타이틀 위로
 * 인게임 화면이 올라온다.
 *
 * === 남의 레벨을 가로채지 않는다 ===
 * `UFF_UIFlowSettings::FrontendLevel` 에 지정한 레벨에서만 자동으로 타이틀을 띄운다.
 * `Test_Main` 이나 팀원 테스트 맵에서 UI 가 끼어들면 전원의 작업이 막힌다.
 * 아무 레벨에서 태워 보려면 `FF.Flow.Start <FestivalId>` 를 쓴다.
 *
 * === 시간 정지 권한 ===
 * `SetTimeScale` 은 강민서님의 Stage 서브시스템이 소유한다. **위젯이 부르지 않고 이 서브시스템이
 * Command 로 요청한다.** 멈추는 곳은 `MorningCheck` 과 ESC 둘뿐이다.
 */
UCLASS()
class FESTAFIESTA_API UFF_UIFlowSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 편의 접근자. 월드가 없거나 GameInstance 가 없으면 nullptr. */
	static UFF_UIFlowSubsystem* Get(const UObject* WorldContextObject);

	// --- 흐름 진입 ---

	/** 타이틀부터 시작한다. 프론트엔드 레벨에서 자동으로 불린다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Flow")
	void StartFrontend();

	/**
	 * 축제를 시작한다. 카탈로그에서 레벨을 찾아 `OpenLevel` 한다.
	 * 실패하면 false 를 돌려주고 이유를 로그에 남긴다 — 레벨을 열지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Flow")
	bool StartFestival(FName FestivalId);

	/** 최종 결산의 `로비로`. 프론트엔드 레벨로 돌아가 국가 지도를 띄운다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Flow")
	void ReturnToAtlas();

	/**
	 * 다음 단계로 보내 달라고 **요청한다.** UI 가 Stage 를 직접 바꾸지 않는다.
	 *
	 * Report 면 `AdvanceToNextDay()`, 스킵 가능한 단계면 `SkipCurrentStage()` 를 부른다.
	 * 둘 다 아니면 아무것도 하지 않고 이유를 로그에 남긴다 — 타이머가 끝나야 넘어가는 단계다.
	 * 어느 쪽을 부를지 UI 가 계산하지 않게 하려고 판정을 여기 한 곳에 모은다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Flow")
	void RequestAdvanceStage();

	/**
	 * 마지막 날의 "축제 마치기". 축제 종료를 알려(`UFF_FestivalStageSubsystem::FinishFestival`)
	 * 최종 결산 화면을 띄우고 기록을 남긴다. 이미 최종 결산이 떠 있거나 띄울 화면이 배선되지
	 * 않았으면 나라 지도로 돌아가 한 사이클을 닫는다 — 어느 쪽이든 갇히지 않는다.
	 * 최종 결산 화면의 `로비로` 도 이 함수를 부르면 된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Flow")
	void RequestFinishFestival();

	/**
	 * "지금 화면에서 다음으로" 를 한 곳에서 판정한다. 스텁 화면의 버튼이 이걸 부른다.
	 *
	 * 축제 중이면 `RequestAdvanceStage()`, 프론트엔드면 다음 화면으로 간다.
	 * 국가 지도에서 부르면 축제를 시작한다 — 여기서 프론트엔드가 인게임으로 넘어간다.
	 *
	 * **스텁 단계용 편의 함수다.** U2~U3 에서 실제 화면이 각자 버튼과 목적지를 갖게 되면
	 * 이 함수는 쓰이지 않게 된다. 그때 지워도 흐름 코드는 그대로다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Flow")
	void RequestAdvanceFlow();

	// --- 프론트엔드 화면 이동 ---

	/** 프론트엔드 화면을 스택에 올린다. 배선이 없으면 올리지 않고 이유를 로그에 남긴다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Flow")
	void GoToScreen(EFF_UIFlowScreen Screen);

	/** 최상단 화면을 닫는다. 닫을 것이 있었으면 true. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Flow")
	bool GoBack();

	// --- 조회 ---

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Flow")
	UFF_UIRootLayout* GetRootLayout() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Flow")
	EFF_UIFlowMode GetMode() const { return Mode; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Flow")
	FName GetActiveFestivalId() const { return ActiveFestivalId; }

	/**
	 * 지금 진행 중인 축제의 화면 표시 이름. 축제 중이 아니거나 카탈로그에 없으면 빈 값이다.
	 *
	 * === 왜 여기인가 ===
	 * 이름은 `UFF_FestivalAtlasCatalog` 가 갖고 `UFF_FestivalDefinition` 에는 없다(김시리님 소유라
	 * 필드를 늘리지 않는다). 카탈로그를 소프트 참조로 들고 있는 것은 흐름 서브시스템뿐이라,
	 * 위젯마다 카탈로그를 로드하지 않게 여기서 한 번만 해석한다.
	 *
	 * **빈 값이면 호출부는 그 칸을 숨긴다.** 축제 이름 자리에 `None` 이 뜨면 고장으로 보인다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Flow")
	FText GetActiveFestivalDisplayName() const;

	/**
	 * 지금 고른 국가. 국가 지도 화면이 어느 나라를 그려야 하는지가 이 값이다.
	 *
	 * ★ **축제를 시작해도, 최종 결산에서 로비로 돌아와도 유지된다.**
	 *   확정 요구사항이 "뒤로 갈 때 각도와 국가 선택을 유지한다" 인데,
	 *   지구 각도는 `AFF_AtlasGlobeActor` 가 들고 있고 국가는 여기가 들고 있다.
	 *   위젯에 두면 화면이 내려갈 때 사라지고, 액터에 두면 레벨이 바뀔 때 사라진다 —
	 *   흐름 서브시스템만 둘 다 넘어 산다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Flow")
	FName GetActiveCountryId() const { return ActiveCountryId; }

	/**
	 * 국가를 고른다. 지구본 화면의 `축제 둘러보기` 가 국가 지도로 넘어가기 전에 부른다.
	 * 값이 실제로 바뀔 때만 알린다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Flow")
	void SetActiveCountry(FName CountryId);

	/** 지금 떠 있는 프론트엔드 화면. 축제 중이면 `None`. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Flow")
	EFF_UIFlowScreen GetFrontendScreen() const { return CurrentFrontendScreen; }

	/** 콘솔 `FF.Flow.Status` 가 찍는 한 줄. 지금 어디에 있는지 진단용. */
	FString DescribeState() const;

protected:
	void HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);

	UFUNCTION()
	void HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage);

	UFUNCTION()
	void HandleFestivalEnded();

	UFUNCTION()
	void HandleTitlePlayRequested();

	/**
	 * 인트로의 「아무 키」. 타이틀을 올리고 인트로는 히스토리와 스택에서 뺀다.
	 * 그래야 타이틀에서 `뒤로` 를 눌러도 인트로가 다시 나오지 않는다.
	 */
	UFUNCTION()
	void HandleTitleIntroFinished();

	UFUNCTION()
	void HandleTitleOptionsRequested();

	UFUNCTION()
	void HandlePlayHubContinueRequested();

	/**
	 * 슬롯 하나를 지정해 이어한다. 플레이 허브의 `계속하기` 와 저장 기록 목록이 함께 쓴다.
	 *
	 * 여기서 불러오지 않는다. 슬롯만 저장 시스템에 넘겨두고 레벨을 열면, 레벨이 준비된 뒤
	 * `GameMode::BeginPlay` 가 복원을 부른다. 화면은 그 순서를 몰라도 된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Flow")
	void ContinueFromSlot(FName SlotId);

	UFUNCTION()
	void HandleSaveListSlotChosen(FName SlotId);

	UFUNCTION()
	void HandlePlayHubNewFestivalRequested();

	UFUNCTION()
	void HandlePlayHubSaveListRequested();

	/** 플레이 허브·저장 기록·옵션·Atlas 화면의 `뒤로` 가 모두 여기로 온다. */
	UFUNCTION()
	void HandleFrontendBackRequested();

	/** 지구본의 `축제 둘러보기`. 고른 나라를 기억하고 국가 지도로 넘어간다. */
	UFUNCTION()
	void HandleAtlasBrowseFestivals(FName CountryId);

	/** 국가 지도의 `축제 시작`. 여기서 프론트엔드가 인게임으로 넘어간다. */
	UFUNCTION()
	void HandleAtlasStartFestival(FName FestivalId);

private:
	/** 지구본↔국가 지도만 암전으로 감싼다. 화면/히스토리는 완전히 가려진 뒤 바꾼다. */
	bool BeginAtlasScreenTransition(EFF_UIFlowScreen Destination, bool bGoingBack);
	void GoToScreenImmediate(EFF_UIFlowScreen Screen);
	bool GoBackImmediate();

	/** 이 GameInstance가 시작한 축제 맵 로드에만 MoviePlayer 화면을 준비한다. */
	void HandlePreLoadMapWithContext(const FWorldContext& WorldContext, const FString& MapName);

	/** MoviePlayer가 끝난 뒤 UUserWidget 강참조를 놓는다. */
	void HandleLoadingScreenFinished();

	/** 없으면 만들어 뷰포트에 올린다. 레벨이 바뀌면 이전 것은 이미 죽어 있다. */
	bool EnsureRootLayout(APlayerController* Controller);

	/** Stage 표를 보고 화면을 올린다. 매핑이 없으면 경고를 남긴다. */
	void ApplyStageScreen(EFF_FestivalStage Stage);

	/** HUD 레이어에 올려 둔 위젯을 내린다. Stage 가 바뀔 때마다 한 번만 남게 한다. */
	void ClearStageHudWidget();

	/** 이 월드가 설정의 `FrontendLevel` 인가. */
	bool IsFrontendWorld(const UWorld* World) const;

	/** 동기 로드한다. 카탈로그는 작고 화면 진입 직전에만 필요하다. */
	const UFF_FestivalAtlasCatalog* LoadCatalog() const;

	/** 축제 하나를 찾는다. 잠김·레벨 누락까지 여기서 걸러 이유를 로그에 남긴다. */
	const FFF_FestivalAtlasEntry* FindStartableEntry(FName FestivalId) const;

	/** 로그를 남길지. 설정의 `bVerboseFlowLogging` 을 읽는다. */
	bool IsVerbose() const;

	/**
	 * 이번 프론트엔드 진입에서 인트로를 틀지. 게임을 켜고 **처음 한 번**만이다.
	 * 콘솔 `FF.Intro.Enable 0` 이거나 `FrontendScreens` 에 TitleIntro 가 없으면 바로 타이틀이다.
	 */
	bool ShouldPlayTitleIntro() const;

	/** 스택 전환(크로스페이드)이 끝난 뒤 인트로 위젯을 스택에서 뺀다. 전환 중에 빼면 화면이 끊긴다. */
	void ScheduleTitleIntroRemoval(UFF_TitleIntroScreen* Intro);

	// --- 상태 ---

	/** 위젯은 월드와 함께 죽는다. 강한 참조로 들면 죽은 위젯을 붙들게 된다. */
	TWeakObjectPtr<UFF_UIRootLayout> RootLayout;

	/** 타이틀 델리게이트를 해제하기 위해 들고 있는다. */
	TWeakObjectPtr<UFF_TitleScreen> TitleScreen;

	/** 떠 있는 인트로. 끝나면 비운다. */
	TWeakObjectPtr<UFF_TitleIntroScreen> TitleIntroScreen;

	/**
	 * 이 GameInstance 에서 인트로를 이미 틀었는지. 축제를 끝내고 돌아오거나 레벨을 다시 열어도
	 * 다시 틀지 않는다 — 인트로는 게임을 켤 때 한 번이다.
	 */
	bool bTitleIntroPlayed = false;

	FTSTicker::FDelegateHandle TitleIntroRemovalTicker;

	/** HUD 레이어에 올린 Stage 화면. 다음 Stage 로 갈 때 내린다. */
	TWeakObjectPtr<UUserWidget> StageHudWidget;

	/**
	 * 화면 스택에 올린 Stage 화면(결산 계열 시트).
	 * `PopScreen` 이 아니라 이것을 지정해 내린다 — ESC 일시정지가 시트 위에 올라간 상태에서
	 * Stage 가 바뀌면 최상단을 내리는 방식은 일시정지를 지운다.
	 */
	TWeakObjectPtr<UFF_ActivatableScreenBase> StageScreenWidget;

	EFF_UIFlowMode Mode = EFF_UIFlowMode::Idle;

	EFF_UIFlowScreen CurrentFrontendScreen = EFF_UIFlowScreen::None;

	/**
	 * 뒤로 가기용 화면 히스토리. `GoToScreen` 이 직전 화면을 쌓고 `GoBack` 이 꺼낸다.
	 *
	 * 이게 없으면 "뒤로 갔는데 화면 이름이 그대로" 가 되거나, 어디서 왔는지 짐작으로
	 * 정하게 된다 — 저장 기록은 플레이 허브에서 오는데 타이틀로 되돌리는 식이다.
	 * CommonUI 스택이 **위젯**은 되살려 주지만 우리 쪽 표시 상태는 알려주지 않는다.
	 */
	TArray<EFF_UIFlowScreen> FrontendHistory;

	/** 프론트엔드 레벨이 열린 뒤 띄울 화면. 첫 진입은 타이틀, `로비로` 는 국가 지도다. */
	EFF_UIFlowScreen PendingFrontendScreen = EFF_UIFlowScreen::Title;

	/**
	 * 다음 `GoToScreen` 한 번만 카메라를 블렌드 없이 맞춘다.
	 *
	 * 축제를 끝내고 나올 때 지구본을 스택 바닥으로 깔고 그 위에 국가 지도를 올리는데,
	 * 그대로 두면 카메라가 지구본 → 국가로 블렌드해서 **지구가 잠깐 스친다.**
	 * 쓰고 나면 스스로 꺼진다 — 다음 전환까지 남으면 엉뚱한 화면이 끊겨 보인다.
	 */
	bool bInstantCameraForNextScreen = false;

	/* 참인 동안 화면을 스택 크로스페이드 없이 올린다. 카메라 플래그와 달리 스스로 꺼지지 않고
	   TGuardValue 범위로만 켠다. 지구본과 국가 지도 두 번의 push 에 모두 걸려야 해서다.
	*/
	bool bPushScreensWithoutTransition = false;

	FName ActiveFestivalId;

	/**
	 * 지금 고른 국가. **`ActiveFestivalId` 와 달리 축제가 끝나도 비우지 않는다** —
	 * 로비로 돌아오면 그 나라의 지도를 다시 보여줘야 하기 때문이다.
	 */
	FName ActiveCountryId;

	/**
	 * 축제 레벨 로드를 기다리는 중인지.
	 * `StartFestival` 에서 곧바로 `RequestTransition` 을 부르면 죽어가는 월드에 화면을 올리게 된다.
	 * 그래서 레벨이 열린 뒤(`HandlePostLogin`)에 Stage 를 세운다.
	 */
	bool bPendingFestivalStart = false;

	/** 최종 결산 화면을 실제로 올렸는지. 안 올렸으면 "축제 마치기" 가 곧장 나라 지도로 보낸다. */
	bool bFinalReportShown = false;

	/** StartFestival이 미리 로드한 로딩 화면 클래스와 표시 문맥. PreLoadMap에서 한 번 소비한다. */
	UPROPERTY(Transient)
	TSubclassOf<UFF_LoadingScreenWidget> PendingLoadingScreenClass;

	FText PendingLoadingFestivalName;
	FString PendingLoadingMapPackage;

	/** 맵 여행 동안 로딩 UUserWidget이 GC되지 않게 GameInstance 수명으로 붙든다. */
	UPROPERTY(Transient)
	TObjectPtr<UFF_LoadingScreenWidget> ActiveLoadingScreenWidget;

	FDelegateHandle PostLoginHandle;
	FDelegateHandle PreLoadMapHandle;
	FDelegateHandle MoviePlaybackFinishedHandle;
};
