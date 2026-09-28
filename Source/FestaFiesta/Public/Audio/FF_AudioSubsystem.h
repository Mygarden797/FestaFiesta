// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Audio/FF_AudioTypes.h"
#include "CoreMinimal.h"
#include "Debug/FF_DebugConsole.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_AudioSubsystem.generated.h"

class UAudioComponent;
class UFF_AudioBankData;

// 축제 전반의 소리를 담당한다.
//
// ─── 두 갈래 ────────────────────────────────────────────────────────────────
// 원샷    불꽃 터짐, 클릭음. 한 번 울리고 사라진다.
// 배경음  계속 흐르고 상황이 바뀌면 크로스페이드로 갈아탄다.
//
// 둘을 한 함수로 묶지 않는 이유는 수명이 다르기 때문이다. 원샷은 재생 후 잊어도 되지만
// 배경음은 컴포넌트를 들고 있다가 다음 곡으로 넘겨줘야 한다.
//
// ─── 애셋이 없어도 돌아간다 ─────────────────────────────────────────────────
// 사운드 뱅크가 비어 있으면 조용히 넘어간다. 크래시도 경고 폭탄도 없다.
// 소리가 안 붙은 상태에서 다른 파트 작업이 막히면 안 된다.
//
// ─── 볼륨 ───────────────────────────────────────────────────────────────────
// 볼륨 설정은 여기서 소유하지 않는다. 플레이어 개인 설정은 다른 곳이 단일 소유자다.
// 여기서는 재생할 때 갈래(EFF_AudioChannel)만 지정해두고, 실제 볼륨 분리는
// 그쪽에 음악·효과음 값이 생겼을 때 이어 붙이면 된다.
UCLASS()
class FESTAFIESTA_API UFF_AudioSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

	static UFF_AudioSubsystem* Get(const UObject* WorldContextObject);

	// ── 원샷 ────────────────────────────────────────────────────────────────

	// 화면 전체에 들리는 소리. UI 나 전역 알림에 쓴다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Audio")
	bool PlayEvent2D(EFF_AudioEvent Event);

	// 특정 위치에서 나는 소리. 거리에 따라 작아진다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Audio")
	bool PlayEventAtLocation(EFF_AudioEvent Event, const FVector& WorldLocation);

	// ── 배경음 ──────────────────────────────────────────────────────────────

	// 배경음을 갈아탄다. 같은 것을 다시 요청하면 아무것도 하지 않는다.
	//
	// 재요청을 무시하는 게 중요하다. 매 틱 상태를 확인해서 호출하는 코드가 붙으면
	// 그때마다 처음부터 다시 재생되어 소리가 계속 끊긴다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Audio")
	void SetAmbience(EFF_AudioAmbience Ambience);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Audio")
	void StopAmbience();

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Audio")
	EFF_AudioAmbience GetCurrentAmbience() const { return CurrentAmbience; }

	// 뱅크에 이 상황의 곡이 들어 있는지.
	// 없는 곡으로 갈아타면 지금 곡만 꺼지고 조용해진다. 그걸 피하고 싶은 호출부가 먼저 본다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Audio")
	bool HasAmbienceTrack(EFF_AudioAmbience Ambience) const;

	// ── 진단 ────────────────────────────────────────────────────────────────

	// 소리를 전부 끈다. 다른 파트를 확인할 때 방해되지 않게.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Audio")
	void SetAudioEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Audio")
	bool IsAudioEnabled() const { return bAudioEnabled; }

	// 재생한 횟수. 0 에서 안 올라가면 이벤트가 안 들어오고 있는 것이다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Audio")
	int32 GetPlayedCueCount() const { return PlayedCues; }

	// 뱅크에 항목이 없어서 넘어간 횟수. 이게 크면 뱅크를 채워야 한다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Audio")
	int32 GetMissingCueCount() const { return MissingCues; }

	// 연타 제한에 걸려 무시된 횟수.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Audio")
	int32 GetSuppressedCueCount() const { return SuppressedCues; }

	// 직접 지정한 뱅크. 비워두면 GameMode → Project Settings 순으로 찾는다.
	UPROPERTY(BlueprintReadWrite, Category = "FestaFiesta|Audio")
	TSoftObjectPtr<UFF_AudioBankData> BankOverride;

private:
	// 쓸 뱅크를 고른다. 직접 지정 → GameMode → Project Settings 순.
	const UFF_AudioBankData* ResolveBank() const;

	// 재생 직전 검사. 꺼져 있거나, 항목이 없거나, 연타면 false.
	bool CanPlay(EFF_AudioEvent Event, const FFF_AudioCue*& OutCue);

	// 여러 개 중 하나를 고른다. 같은 소리가 반복되면 금방 질린다.
	USoundBase* PickSound(const FFF_AudioCue& Cue) const;

	void RegisterDebugConsoleCommands();
	void UnregisterDebugConsoleCommands();

	bool bAudioEnabled = true;

	EFF_AudioAmbience CurrentAmbience = EFF_AudioAmbience::None;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> AmbienceComponent;

	// 이전 배경음. 크로스페이드가 끝날 때까지 들고 있다가 정리한다.
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> FadingAmbienceComponent;

	FTimerHandle FadingCleanupTimer;

	// 이벤트별 마지막 재생 시각. 연타를 걸러내는 데 쓴다.
	TMap<EFF_AudioEvent, double> LastPlayTimeSeconds;

	int32 PlayedCues = 0;
	int32 MissingCues = 0;
	int32 SuppressedCues = 0;

	// 개발용 콘솔 명령. 이름 충돌·해제 규칙은 FFF_DebugConsoleCommands 가 맡는다.
	FFF_DebugConsoleCommands DebugCommands;
};
