// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Audio/FF_AudioSettings.h"

// 불꽃 소리 지연 계산. 거리와 속도로 초를 내고, 상한을 넘지 않으며, 잘못된 값이면 지연하지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AudioSoundTravelDelay, "FestaFiesta.Audio.Settings.SoundTravelDelay", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_AudioSoundTravelDelay::RunTest(const FString& Parameters)
{
	// 343m 는 1초.
	TestEqual(TEXT("343m 는 1초"), UFF_AudioSettings::ComputeSoundTravelDelay(34300.f, 34300.f, 3.f), 1.f);

	// 300m 는 약 0.87초. 실제 해변 관람 거리에서 느껴지는 지연이다.
	TestTrue(TEXT("300m 는 1초보다 짧다"),
		UFF_AudioSettings::ComputeSoundTravelDelay(30000.f, 34300.f, 3.f) < 1.f);

	TestEqual(TEXT("상한에서 멈춘다"), UFF_AudioSettings::ComputeSoundTravelDelay(500000.f, 34300.f, 3.f), 3.f);
	TestEqual(TEXT("속도 0 이면 지연 없음"), UFF_AudioSettings::ComputeSoundTravelDelay(30000.f, 0.f, 3.f), 0.f);
	TestEqual(TEXT("거리 0 이면 지연 없음"), UFF_AudioSettings::ComputeSoundTravelDelay(0.f, 34300.f, 3.f), 0.f);
	TestEqual(TEXT("상한 음수는 0 으로 본다"), UFF_AudioSettings::ComputeSoundTravelDelay(30000.f, 34300.f, -1.f), 0.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
