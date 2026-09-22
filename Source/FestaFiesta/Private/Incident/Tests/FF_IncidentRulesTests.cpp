// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Incident/Rules/FF_IncidentRules.h"

/**
 * 사건 판정 순수 함수. [참고] 작업사항 저장/2026-09-20_사건(Incident)시스템_최소착수_사다리와_알고리즘지도.md §4
 *
 * 순수 구조체라 World 도 GameInstance 도 세우지 않는다.
 */
namespace FFF_IncidentRulesTestHelpers
{
	inline FFF_IncidentAnchor MakeWorldPoint(float X, float Y)
	{
		FFF_IncidentAnchor Anchor;
		Anchor.Kind = EFF_IncidentAnchorKind::WorldPoint;
		Anchor.WorldPoint = FVector(X, Y, 0.f);
		return Anchor;
	}

	inline FFF_IncidentAnchor MakeDensityCell(int32 X, int32 Y)
	{
		FFF_IncidentAnchor Anchor;
		Anchor.Kind = EFF_IncidentAnchorKind::DensityCell;
		Anchor.DensityCell = FIntPoint(X, Y);
		return Anchor;
	}

	inline FGameplayTag CongestionTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Incident.Crowd.Congestion"), /*ErrorIfNotFound=*/false);
	}

	inline FGameplayTag BreakdownTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Incident.Facility.Breakdown"), /*ErrorIfNotFound=*/false);
	}
}

// 1UU 흔들린 좌표가 같은 키로 나온다. 양자화가 없으면 매 틱 다른 키가 나와 중복 억제가 무력화된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_IncidentRules_DedupeKeyQuantizesWorldPoint, "FestaFiesta.Incident.Rules.DedupeKeyQuantizesWorldPoint", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_IncidentRules_DedupeKeyQuantizesWorldPoint::RunTest(const FString& Parameters)
{
	// TODO
	return true;
}

// 원점 왼쪽 -1UU 와 오른쪽 +1UU 는 다른 칸이다. FloorToInt 가 아니면 여기서 깨진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_IncidentRules_DedupeKeyHandlesNegativeCoordinates, "FestaFiesta.Incident.Rules.DedupeKeyHandlesNegativeCoordinates", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_IncidentRules_DedupeKeyHandlesNegativeCoordinates::RunTest(const FString& Parameters)
{
	// TODO
	return true;
}

// 같은 셀 · 같은 태그면 같은 키다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_IncidentRules_DedupeKeySameCellMatches, "FestaFiesta.Incident.Rules.DedupeKeySameCellMatches", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_IncidentRules_DedupeKeySameCellMatches::RunTest(const FString& Parameters)
{
	// TODO
	return true;
}

// 셀이 같아도 태그가 다르면 다른 사건이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_IncidentRules_DedupeKeyDifferentTagDiffers, "FestaFiesta.Incident.Rules.DedupeKeyDifferentTagDiffers", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_IncidentRules_DedupeKeyDifferentTagDiffers::RunTest(const FString& Parameters)
{
	// TODO
	return true;
}

// MinutesUntilLapse = 0 이면 시간 제한이 없다. 아무리 지나도 만료되지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_IncidentRules_LapseZeroNeverExpires, "FestaFiesta.Incident.Rules.LapseZeroNeverExpires", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_IncidentRules_LapseZeroNeverExpires::RunTest(const FString& Parameters)
{
	// TODO
	return true;
}

// Problem → Ignored, Opportunity → Missed. 합치면 리포트가 못 쓰게 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_IncidentRules_LapseStateSeparatesIgnoredAndMissed, "FestaFiesta.Incident.Rules.LapseStateSeparatesIgnoredAndMissed", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_IncidentRules_LapseStateSeparatesIgnoredAndMissed::RunTest(const FString& Parameters)
{
	// TODO
	return true;
}

// 0.9 뒤에 0.3 이 와도 0.9 가 남는다. 리포트가 보는 것은 가장 나빴던 순간이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_IncidentRules_MergeIntensityKeepsPeak, "FestaFiesta.Incident.Rules.MergeIntensityKeepsPeak", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_IncidentRules_MergeIntensityKeepsPeak::RunTest(const FString& Parameters)
{
	// TODO
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
