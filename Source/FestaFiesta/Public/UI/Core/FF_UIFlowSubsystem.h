// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Time/FF_FestivalStage.h"
#include "UI/Core/FF_UIFlowTypes.h"
#include "FF_UIFlowSubsystem.generated.h"

class AGameModeBase;
class APlayerController;
class UFF_FestivalAtlasCatalog;
class UFF_ActivatableScreenBase;
class UFF_TitleScreen;
class UFF_UIRootLayout;
class UUserWidget;
struct FFF_FestivalAtlasEntry;

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

	UFUNCTION()
	void HandleTitleOptionsRequested();

private:
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

	// --- 상태 ---

	/** 위젯은 월드와 함께 죽는다. 강한 참조로 들면 죽은 위젯을 붙들게 된다. */
	TWeakObjectPtr<UFF_UIRootLayout> RootLayout;

	/** 타이틀 델리게이트를 해제하기 위해 들고 있는다. */
	TWeakObjectPtr<UFF_TitleScreen> TitleScreen;

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

	/** 프론트엔드 레벨이 열린 뒤 띄울 화면. 첫 진입은 타이틀, `로비로` 는 국가 지도다. */
	EFF_UIFlowScreen PendingFrontendScreen = EFF_UIFlowScreen::Title;

	FName ActiveFestivalId;

	/**
	 * 축제 레벨 로드를 기다리는 중인지.
	 * `StartFestival` 에서 곧바로 `RequestTransition` 을 부르면 죽어가는 월드에 화면을 올리게 된다.
	 * 그래서 레벨이 열린 뒤(`HandlePostLogin`)에 Stage 를 세운다.
	 */
	bool bPendingFestivalStart = false;

	FDelegateHandle PostLoginHandle;
};
