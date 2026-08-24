// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UI/Data/FF_UIPalette.h"
#include "FF_UIAccessibilitySubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnReduceMotionChanged, bool, bEnabled);

/**
 * 접근성 설정. 지금은 `움직임 감소` 하나다.
 *
 * === 왜 팔레트가 아니라 여기인가 ===
 * `UFF_UIPalette` 는 `UDeveloperSettings` 라 **프로젝트 설정**이다. `DefaultGame.ini` 에 저장되고
 * 팀 전체가 공유한다. 접근성은 **플레이어 개인 설정**이라 같은 곳에 둘 수 없다.
 * 그래서 값은 `GameUserSettings.ini`(개인·커밋 안 됨)에 저장하고, 길이의 기준치는 팔레트에서 읽는다.
 *
 * === 위젯은 팔레트에서 직접 길이를 읽지 않는다 ===
 * `GetMotionDuration()` 을 쓴다. `움직임 감소` 가 켜지면 0 을 돌려줘야 하는데, 그 판정을 위젯마다
 * 두면 어디 하나가 빠지고 그 화면만 계속 움직인다. 판정을 한 곳에 모은다.
 *
 * === 옵션 화면이 아니라 여기가 소유한다 ===
 * 옵션 화면이 값을 들고 있으면 화면이 닫힐 때 사라진다. 옵션은 이 서브시스템에 요청만 한다.
 */
UCLASS()
class FESTAFIESTA_API UFF_UIAccessibilitySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 월드 컨텍스트로 찾는다. 없으면 nullptr. */
	static UFF_UIAccessibilitySubsystem* Get(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Accessibility")
	bool IsReduceMotionEnabled() const { return bReduceMotion; }

	/** 값이 실제로 바뀔 때만 저장하고 알린다. 같은 값을 다시 넣으면 아무 일도 하지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Accessibility")
	void SetReduceMotionEnabled(bool bEnabled);

	/**
	 * 전환에 쓸 실제 초. **위젯은 이것만 쓴다.**
	 * `움직임 감소` 가 켜져 있으면 항상 0 이다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Accessibility")
	float GetMotionDuration(EFF_UIMotionSpeed Speed) const;

	/** 켜짐/꺼짐이 바뀌면 알린다. 이미 떠 있는 화면이 애니메이션을 멈추게 하려면 구독한다. */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI|Accessibility")
	FFF_OnReduceMotionChanged OnReduceMotionChanged;

private:
	/** `GameUserSettings.ini` 에서 읽는다. 없으면 꺼진 상태로 시작한다. */
	void LoadFromUserSettings();

	/** 개인 설정이라 `GGameUserSettingsIni` 에 쓴다. 프로젝트 ini 를 건드리지 않는다. */
	void SaveToUserSettings() const;

	bool bReduceMotion = false;
};
