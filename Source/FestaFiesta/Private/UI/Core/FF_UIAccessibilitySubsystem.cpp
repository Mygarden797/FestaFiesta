// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Core/FF_UIAccessibilitySubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"
#include "UI/FF_UILog.h"

namespace
{
	// 개인 설정 저장 위치. Saved/Config/.../GameUserSettings.ini 로 들어가며 커밋되지 않는다.
	// 프로젝트 ini(DefaultGame.ini)에 쓰면 팀 전체에 퍼지므로 쓰지 않는다.
	const TCHAR* AccessibilitySection = TEXT("/Script/FestaFiesta.UIAccessibility");
	const TCHAR* ReduceMotionKey = TEXT("bReduceMotion");
}

void UFF_UIAccessibilitySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadFromUserSettings();
}

UFF_UIAccessibilitySubsystem* UFF_UIAccessibilitySubsystem::Get(const UObject* WorldContextObject)
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
	return GameInstance ? GameInstance->GetSubsystem<UFF_UIAccessibilitySubsystem>() : nullptr;
}

void UFF_UIAccessibilitySubsystem::LoadFromUserSettings()
{
	if (!GConfig)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_UIAccessibilitySubsystem: GConfig 가 없어 접근성 설정을 읽지 못했다. 기본값으로 시작한다."));
		return;
	}

	// 값이 없으면 GetBool 이 false 를 그대로 두므로 기본값(꺼짐)이 유지된다.
	GConfig->GetBool(AccessibilitySection, ReduceMotionKey, bReduceMotion, GGameUserSettingsIni);
}

void UFF_UIAccessibilitySubsystem::SaveToUserSettings() const
{
	if (!GConfig)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_UIAccessibilitySubsystem: GConfig 가 없어 접근성 설정을 저장하지 못했다."));
		return;
	}

	GConfig->SetBool(AccessibilitySection, ReduceMotionKey, bReduceMotion, GGameUserSettingsIni);

	// Flush 하지 않으면 에디터를 정상 종료하지 않았을 때 값이 사라진다.
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UFF_UIAccessibilitySubsystem::SetReduceMotionEnabled(bool bEnabled)
{
	// 같은 값이면 아무 일도 하지 않는다. 안 막으면 토글을 누를 때마다 ini 를 쓰고
	// 구독자가 화면을 다시 그린다.
	if (bReduceMotion == bEnabled)
	{
		return;
	}

	bReduceMotion = bEnabled;
	SaveToUserSettings();

	UE_LOG(LogFestaFiestaUI, Log,
		TEXT("UFF_UIAccessibilitySubsystem: 움직임 감소 %s"), bEnabled ? TEXT("켜짐") : TEXT("꺼짐"));

	OnReduceMotionChanged.Broadcast(bReduceMotion);
}

float UFF_UIAccessibilitySubsystem::GetMotionDuration(EFF_UIMotionSpeed Speed) const
{
	if (bReduceMotion)
	{
		// 0 이면 전환이 즉시 끝난다. 애니메이션을 건너뛰는 분기를 위젯마다 두지 않게 하려는 것이다.
		return 0.f;
	}

	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	if (!Palette)
	{
		return 0.f;
	}

	switch (Speed)
	{
	case EFF_UIMotionSpeed::Fast:	return Palette->Motion.Fast;
	case EFF_UIMotionSpeed::Base:	return Palette->Motion.Base;
	case EFF_UIMotionSpeed::Screen:	return Palette->Motion.Screen;
	}

	// 새 단계가 추가되고 여기 빠지면 조용히 0 이 되어 애니메이션이 사라진다. 소리를 낸다.
	UE_LOG(LogFestaFiestaUI, Warning,
		TEXT("UFF_UIAccessibilitySubsystem: 처리하지 않은 전환 단계 %s. 0 을 돌려준다."),
		*UEnum::GetValueAsString(Speed));
	return 0.f;
}
