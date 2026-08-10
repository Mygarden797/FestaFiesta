// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC/Integration/FF_ZoneSystemBridge.h"

#include "Building/FF_FacilityNamingLibrary.h"
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
	// Booth 소분류 → preference_vector 키. NamingLibrary의 Booth 토큰 목록과 1:1로 유지한다.
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

	/**
	 * 컨벤션을 안 지킨 시설을 하나당 딱 한 번만 Warning 으로 알린다.
	 *
	 * 왜 필요한가: 컨벤션 위반은 크래시가 아니라 조용한 오작동을 낸다. Booth 라면 "그 부스는 영구히
	 * 취향 없음"이 되어 아무도 취향으로 가지 않고 Need 도 해소하지 못하는데, 부스를 만든 사람은
	 * 그 사실을 알 방법이 없었다.
	 *
	 * 매 평가마다 찍으면 방문객 8만 명 규모에서 로그가 감당이 안 되므로 시설별 1회로 제한한다.
	 */
	void WarnOnceAboutNamingViolation(FName FacilityDefinitionId, const FString& Reason, const FString& KnownTokens)
	{
		static TSet<FName> AlreadyWarned;
		if (AlreadyWarned.Contains(FacilityDefinitionId))
		{
			return;
		}
		AlreadyWarned.Add(FacilityDefinitionId);

		UE_LOG(LogFestaFiestaNpc, Warning,
			TEXT("[시설 네이밍 컨벤션 위반] FacilityDefinitionId='%s' — %s ")
			TEXT("형태는 '대분류_소분류_고유이름'이어야 한다(대분류 = EFFZSFacilityCategory 이름). ")
			TEXT("이 카테고리의 아는 소분류: %s. 규칙 전문은 data/metadata/handoff/facility_naming_convention.md."),
			*FacilityDefinitionId.ToString(), *Reason, *KnownTokens);
	}

	/**
	 * 공용 NamingLibrary의 순수 파싱 결과에 기존 시설별 Warning-once 정책을 덧씌운다.
	 * UI와 카탈로그는 라이브러리를 직접 사용하고, 고빈도 NPC 평가만 이 래퍼를 거친다.
	 */
	bool TryParseFacilityId(EFFZSFacilityCategory Category, FName FacilityDefinitionId, FString& OutMinorToken)
	{
		OutMinorToken.Reset();
		FFF_FacilityNameParts Parts;
		FString FailureReason;
		if (!UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
			Category, FacilityDefinitionId, Parts, FailureReason))
		{
			// NAME_None은 정의가 아직 안 붙은 정상 상태일 수 있어 라이브러리가 이유를 비워 반환한다.
			if (!FailureReason.IsEmpty())
			{
				const FString KnownTokens = UFF_FacilityNamingLibrary::GetMajorToken(Category).IsNone()
					? TEXT("(없음)")
					: UFF_FacilityNamingLibrary::JoinKnownMinorTokens(Category);
				WarnOnceAboutNamingViolation(FacilityDefinitionId, FailureReason, KnownTokens);
			}
			return false;
		}

		OutMinorToken = Parts.MinorToken.ToString();
		return true;
	}

	// Booth 소분류 → preference_vector 키. 컨벤션 위반이면 NAME_None(취향 없음)으로 동작한다.
	FName ResolveBoothPreferenceCategory(FName FacilityDefinitionId)
	{
		FString MinorToken;
		if (!TryParseFacilityId(EFFZSFacilityCategory::Booth, FacilityDefinitionId, MinorToken))
		{
			return NAME_None;
		}

		const FName* Found = GetBoothTokenToPreferenceCategory().Find(MinorToken);
		return Found ? *Found : NAME_None;
	}
}

FName FFF_ZoneSystemBridge::MapToPreferenceCategory(
	EFFZSFacilityCategory ZoneSystemCategory,
	FName FacilityDefinitionId)
{
	// Booth 가 아닌 카테고리도 컨벤션을 검증한다 — 지금은 소분류가 취향 카테고리를 바꾸지 않지만,
	// 오타를 방치하면 나중에 소분류별 분기를 추가할 때 조용히 빠진다. 반환값은 안 쓰고 경고만 노린다.
	FString UnusedMinorToken;
	TryParseFacilityId(ZoneSystemCategory, FacilityDefinitionId, UnusedMinorToken);

	switch (ZoneSystemCategory)
	{
	case EFFZSFacilityCategory::Rest:
		// 소분류 4종(Seating/Shade/Picnic/Cooling) 전부 rest_area 로 본다 — 취향 단위에서는 구분하지 않는다.
		return TEXT("rest_area");
	case EFFZSFacilityCategory::Performance:
		// TODO(합의): ExperienceStage(참여형 공연)는 activity_game 에 더 가까울 수 있다. 현재는 전부 performance_stage.
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
	{
		// Rest 4종(Seating/Shade/Picnic/Cooling) 전부 피로를 풀어준다.
		Flags.bRelievesFatigue = true;

		FString MinorToken;
		if (TryParseFacilityId(EFFZSFacilityCategory::Rest, FacilityDefinitionId, MinorToken))
		{
			Flags.bRelievesThirst = (MinorToken == TEXT("Cooling"));
		}
		break;
	}
	case EFFZSFacilityCategory::Performance:
	case EFFZSFacilityCategory::PhotoSpot:
		Flags.bRelievesStimulation = true;
		break;
	case EFFZSFacilityCategory::Booth:
	{
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
