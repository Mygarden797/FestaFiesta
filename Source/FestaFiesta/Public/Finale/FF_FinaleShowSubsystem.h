// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Economy/FF_FestivalEconomyTypes.h"
#include "Time/FF_FestivalStage.h"
#include "Finale/FF_FinaleShowTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_FinaleShowSubsystem.generated.h"

struct IConsoleCommand;

class AFF_FinaleViewpoint;
class AFF_FinaleLaunchSite;
class AFF_FestivalPlayerController;

// 연출 진행 상태가 바뀔 때. UI 가 표시를 바꾸거나 다른 시스템이 멈출 때 쓴다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnFinaleShowStateChanged, bool, bInProgress);

// 한 발 터질 때마다. 사운드나 화면 흔들림을 붙이고 싶으면 여기에 건다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFF_OnFinaleBurst, FVector, WorldLocation, FLinearColor, Color);

// 피날레 불꽃 연출을 실행한다.
//
// ─── 경계 ───────────────────────────────────────────────────────────────────
// 등급은 경제·축제 쪽이 소유한다. 여기서는 읽기만 하고 구매나 판정에 관여하지 않는다.
// 카메라 전환은 컨트롤러에 요청만 하고 직접 시점을 만들지 않는다.
//
// ─── 아무것도 없어도 돌아야 한다 ────────────────────────────────────────────
// 레벨도 이펙트 에셋도 아직 없는 단계다. 발사 지점이 없으면 원점 위에서 터뜨리고,
// 이펙트가 없으면 로그만 남긴다. 어느 경우에도 크래시나 정지가 없어야
// 다른 파트 작업이 이것 때문에 막히지 않는다.
UCLASS()
class FESTAFIESTA_API UFF_FinaleShowSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// 지금 구매된 등급으로 연출을 시작한다.
	// 이미 진행 중이면 아무것도 하지 않는다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Finale")
	bool StartShow();

	// 등급을 직접 지정해서 시작한다. 연출 확인용이다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Finale")
	bool StartShowWithGrade(EFF_FinaleGrade Grade);

	// 진행 중인 연출을 중단하고 시점을 되돌린다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Finale")
	void StopShow();

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Finale")
	bool IsShowInProgress() const { return bShowInProgress; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Finale")
	EFF_FinaleGrade GetActiveGrade() const { return ActiveGrade; }

	// 지금까지 몇 발 터졌는지. 0 에서 안 올라가면 발사가 안 되고 있는 것이다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Finale")
	int32 GetFiredBurstCount() const { return FiredBursts; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Finale")
	float GetElapsedSeconds() const { return ElapsedSeconds; }

	// 축제가 피날레 단계에 들어가면 자동으로 시작할지.
	//
	// 끄면 StartShow 를 직접 불러야 한다. 연출을 다듬는 동안에는 꺼두고 콘솔로
	// 재생하는 편이 편하다. 단계를 넘기려면 축제를 끝까지 진행해야 하기 때문이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FestaFiesta|Finale")
	bool bAutoStartOnFinaleStage = true;

	// 연출 중 카메라를 연출 시점으로 옮길지. 끄면 현재 시점에서 그대로 본다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Finale")
	void SetCameraTakeoverEnabled(bool bEnabled) { bTakeOverCamera = bEnabled; }

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Finale")
	FFF_OnFinaleShowStateChanged OnShowStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Finale")
	FFF_OnFinaleBurst OnBurst;

	// 등급별 수치를 직접 지정한다. 보통은 비워두고 GameMode 나 Project Settings 를 쓴다.
	//
	// 월드 서브시스템은 에디터에 노출되지 않으므로 이 값은 코드에서만 채울 수 있다.
	// 연출을 임시로 바꿔보고 싶을 때 쓰는 자리다.
	UPROPERTY(BlueprintReadWrite, Category = "FestaFiesta|Finale")
	TSoftObjectPtr<UFF_FinaleShowData> ShowData;

	// 연출 시작·종료 시 카메라 블렌드 시간.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FestaFiesta|Finale", meta = (ClampMin = "0.0"))
	float CameraBlendSeconds = 1.5f;

	// 마지막 발이 터진 뒤 시점을 되돌리기까지 기다리는 시간.
	// 바로 돌아가면 마지막 불꽃이 화면에서 잘린다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FestaFiesta|Finale", meta = (ClampMin = "0.0"))
	float TailSeconds = 3.f;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_FinaleShowSubsystem, STATGROUP_Tickables);
	}

private:
	// ── 디버그 콘솔 ──────────────────────────────────────────────────────────
	//
	// 이펙트를 만들려면 계속 터뜨려 봐야 하는데, 그때마다 축제를 끝까지 진행할 수는 없다.
	//
	// UFUNCTION(Exec) 을 쓰지 않는 이유는 UGameInstance 에 ProcessConsoleExec 이 없어
	// 콘솔이 서브시스템의 Exec 함수를 찾아가지 않기 때문이다. IConsoleManager 에 직접 등록한다.
	// 해제를 반드시 하는 이유는 PIE 가 한 에디터 세션에서 여러 번 돌기 때문이다.
	// 남겨두면 두 번째 PIE 에서 같은 이름 등록이 실패하고, 파괴된 this 를 가리키는
	// 델리게이트가 콘솔에 남는다.
	// 축제 단계 신호를 구독한다. 단계는 축제 진행 쪽이 소유하고 여기서는 읽기만 한다.
	void BindFestivalStage();
	void UnbindFestivalStage();

	UFUNCTION()
	void HandleFestivalStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage);

	void RegisterDebugConsoleCommands();
	void UnregisterDebugConsoleCommands();

	// 문자열을 등급으로 바꾼다. "S" "a" "none" 전부 받는다.
	static bool TryParseGrade(const FString& Token, EFF_FinaleGrade& OutGrade);

	// 이펙트에 우리가 넣으려는 User 파라미터가 실제로 있는지 확인하고, 없으면 알린다.
	//
	// 이름이 틀리면 Niagara 는 조용히 무시한다. 크래시도 경고도 없어서
	// "왜 색이 안 바뀌지" 로만 나타나고, 이펙트 쪽을 한참 뒤진 뒤에야 이름 문제인 걸 안다.
	// 연출당 한 번만 검사한다. 매 발 찍으면 로그가 넘친다.
	void VerifyEffectParameters(const UNiagaraSystem& Effect) const;

	// 이번 연출에서 이미 검사한 이펙트. 같은 것을 반복해서 검사하지 않는다.
	mutable TWeakObjectPtr<const UNiagaraSystem> VerifiedEffect;

	IConsoleCommand* PlayConsoleCommand = nullptr;
	IConsoleCommand* StopConsoleCommand = nullptr;
	IConsoleCommand* StatConsoleCommand = nullptr;

	// 이번 등급의 수치를 결정한다. 데이터 애셋이 있으면 그것을, 없으면 기본값을 쓴다.
	FFF_FinaleGradeSpec ResolveSpec(EFF_FinaleGrade Grade) const;

	// 쓸 데이터 애셋을 고른다. 직접 지정 → GameMode → Project Settings 순으로 본다.
	//
	// 레벨마다 연출이 다를 수 있어서 GameMode 가 전역 설정보다 우선한다.
	// 전역 기본값을 둔 이유는 새 레벨을 만들 때마다 매번 지정하지 않아도 되게 하려는 것이다.
	const UFF_FinaleShowData* ResolveShowData() const;

	// 한 차례 발사. SimultaneousShells 만큼 동시에 터뜨린다.
	void FireVolley();

	// 발사 지점을 구한다. 레벨에 발사대가 없으면 원점 위를 쓴다.
	FVector ResolveLaunchOrigin() const;

	// 터질 지점 하나를 뽑는다. 반경 안에 흩고 높이도 범위 안에서 고른다.
	FVector PickBurstLocation(const FVector& Origin) const;

	FLinearColor PickColor(int32 Index) const;

	AFF_FestivalPlayerController* GetFestivalController() const;

	// 게임 인스턴스 서브시스템을 꺼낸다. 월드와 인스턴스 널체크가 매번 반복돼서 묶었다.
	template <typename SubsystemType>
	SubsystemType* GetGameInstanceSubsystem() const
	{
		const UWorld* World = GetWorld();
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<SubsystemType>() : nullptr;
	}

	bool bShowInProgress = false;
	bool bTakeOverCamera = true;

	EFF_FinaleGrade ActiveGrade = EFF_FinaleGrade::None;
	FFF_FinaleGradeSpec ActiveSpec;

	float ElapsedSeconds = 0.f;
	float NextVolleyAt = 0.f;
	int32 FiredBursts = 0;
	int32 FiredVolleys = 0;

	// 마지막 발 이후 여운 시간. 음수면 아직 발사가 안 끝난 것이다.
	float TailStartedAt = -1.f;

	// 색을 순서대로 돌린다. 같은 색이 연달아 나오면 등급이 낮아 보인다.
	int32 ColorCursor = 0;

	FRandomStream ShowRandom;

	// 단계 신호에 물려 있는지. 해제할 때 중복 호출을 막는다.
	bool bStageBound = false;
};
