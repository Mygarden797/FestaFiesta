// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC/Integration/FF_ZoneSystemBridge.h"

#include "Engine/AssetManager.h"
#include "FF_FFZSFacilityDefinition.h"

// !!! 스텁 !!!
// 카테고리 매핑·ExpectedWait·Need 해소 판정·시설 인스턴스 ID 변환은 구현되었고, 남은 건 그걸 호출해 조립하는 것뿐이다.

TArray<FFF_DestinationCandidate> FFF_ZoneSystemBridge::BuildCandidates(
	const FFFZSReadModel& ReadModel,
	FName CurrentZoneId,
	int32 PartySize)
{
	// TODO(Zone 그래프): CurrentZoneId 의 인접 Zone 을 알아야 후보를 8~12개로 줄일 수 있다.
	// 지금 전체 시설을 그대로 넘기면 (동시 Party 수) × (전체 시설) 이 되어 프레임이 무너진다.
	(void)ReadModel;
	(void)CurrentZoneId;
	(void)PartySize;

	UE_LOG(LogFestaFiestaNpc, Verbose,
		TEXT("FFF_ZoneSystemBridge::BuildCandidates 는 아직 스텁이다 — 빈 후보를 돌려준다."));

	return TArray<FFF_DestinationCandidate>();
}

namespace
{
	// Booth 서브타입 네이밍 컨벤션 표.
	// FacilityDefinitionId 가 "Booth_<Token>_<이름>" 형태일 때 두 번째 토큰을 preference_vector 키로 바꾼다.
	// [주의] 새 부스를 만들 때 이 표에 없는 Token을 쓰면 조용히 NAME_None(취향 없음)으로 빠지니, 부스 만드는 담당자(한광훈/박건영)는 아래 표를 반드시 참고해야 한다.
	const TMap<FString, FName>& GetBoothTokenToPreferenceCategory()
	{
		static const TMap<FString, FName> Table = {
			{ TEXT("FoodStall"),       TEXT("food_stall") },
			{ TEXT("MarketGoods"),     TEXT("market_goods") },
			{ TEXT("CraftExperience"), TEXT("craft_experience") },
			{ TEXT("ActivityGame"),    TEXT("activity_game") },
			{ TEXT("Nightlife"),       TEXT("nightlife_entertainment") },
		};
		return Table;
	}

	FName ResolveBoothPreferenceCategory(FName FacilityDefinitionId)
	{
		TArray<FString> Tokens;
		FacilityDefinitionId.ToString().ParseIntoArray(Tokens, TEXT("_"));
		if (Tokens.Num() < 2 || Tokens[0] != TEXT("Booth"))
		{
			return NAME_None;
		}

		const FName* Found = GetBoothTokenToPreferenceCategory().Find(Tokens[1]);
		return Found ? *Found : NAME_None;
	}
}

FName FFF_ZoneSystemBridge::MapToPreferenceCategory(
	EFFZSFacilityCategory ZoneSystemCategory,
	FName FacilityDefinitionId)
{
	switch (ZoneSystemCategory)
	{
	case EFFZSFacilityCategory::Rest:
		return TEXT("rest_area");
	case EFFZSFacilityCategory::Performance:
		return TEXT("performance_stage");
	case EFFZSFacilityCategory::PhotoSpot:
		return TEXT("ambience_photo_zone");
	case EFFZSFacilityCategory::Booth:
		return ResolveBoothPreferenceCategory(FacilityDefinitionId);
	case EFFZSFacilityCategory::Toilet:
	case EFFZSFacilityCategory::Info:
	case EFFZSFacilityCategory::Safety:
	default:
		// Toilet 은 Need로만 해소(GetNeedReliefFlags), Info·Safety 는 목적지 후보 자체가 아니다.
		// NAME_None 이면 FFF_UtilityDestinationSelector 가 preference 항을 0으로 계산한다 —
		// 크래시 없이 "취향 없음"으로 동작한다.
		return NAME_None;
	}
}

FFF_NeedReliefFlags FFF_ZoneSystemBridge::GetNeedReliefFlags(
	EFFZSFacilityCategory ZoneSystemCategory,
	FName FacilityDefinitionId)
{
	FFF_NeedReliefFlags Flags;

	switch (ZoneSystemCategory)
	{
	case EFFZSFacilityCategory::Toilet:
		Flags.bRelievesRestroom = true;
		break;
	case EFFZSFacilityCategory::Rest:
		// 8/6 리플렛 확인: 지도 범례의 "오아시스 존"이 그늘+음수대+무릎나눔 복합 시설이다 —
		// 실제로는 Fatigue 뿐 아니라 Thirst도 여기서 풀린다.
		Flags.bRelievesFatigue = true;
		Flags.bRelievesThirst = true;
		break;
	case EFFZSFacilityCategory::Performance:
	case EFFZSFacilityCategory::PhotoSpot:
		Flags.bRelievesStimulation = true;
		break;
	case EFFZSFacilityCategory::Booth:
	{
		// MapToPreferenceCategory와 같은 Booth 네이밍 컨벤션을 재사용한다
		// 두 곳에서 각자 판정하면 나중에 어긋난다. FoodStall은 배고픔·갈증 둘 다,
		// Nightlife는 갈증(음주)과 자극(선셋클럽류 라운지 — 위 Booth 토큰 표 참고) 둘 다 해소.
		const FName PreferenceCategory = ResolveBoothPreferenceCategory(FacilityDefinitionId);
		Flags.bRelievesHunger = (PreferenceCategory == TEXT("food_stall"));
		Flags.bRelievesThirst = (PreferenceCategory == TEXT("food_stall")
			|| PreferenceCategory == TEXT("nightlife_entertainment"));
		Flags.bRelievesStimulation = (PreferenceCategory == TEXT("nightlife_entertainment"));
		break;
	}
	default:
		// Info·Safety 는 FF_NeedState.h 설계상 아예 Need 대상이 아니다.
		break;
	}

	return Flags;
}

float FFF_ZoneSystemBridge::GetServiceRatePerMinute(FName FacilityDefinitionId)
{
	if (FacilityDefinitionId.IsNone())
	{
		return 0.f;
	}

	// FF_FFZSFacilityDefinition.cpp::GetPrimaryAssetId 가 쓰는 것과 동일한 Primary Asset Type 문자열이다
	// 여기가 바뀌면 저기도 같이 바뀌어야 한다.
	const FPrimaryAssetId AssetId(TEXT("FFZSFacilityDefinition"), FacilityDefinitionId);
	const UFFZSFacilityDefinition* Definition =
		UAssetManager::Get().GetPrimaryAssetObject<UFFZSFacilityDefinition>(AssetId);
	if (!Definition)
	{
		// 동기 조회라 아직 로드되지 않은 자산은 nullptr 다 — 여기서 강제 로드하지 않는다.
		// 호출부가 0을 "모름"으로 처리해야 한다(나누기 금지, ExpectedWait 무한대 취급).
		return 0.f;
	}

	const float ServiceDurationSeconds = Definition->Spec.QueueRules.ServiceDurationSeconds;
	if (ServiceDurationSeconds <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	const float ServiceDurationMinutes = SecondsToMinutes(ServiceDurationSeconds);
	return static_cast<float>(Definition->Spec.QueueRules.ServiceCapacity) / ServiceDurationMinutes;
}
