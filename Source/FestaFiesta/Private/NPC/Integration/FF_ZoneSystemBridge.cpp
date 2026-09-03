// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC/Integration/FF_ZoneSystemBridge.h"

#include "Building/FF_FacilityNamingLibrary.h"
#include "Engine/AssetManager.h"
#include "FF_FFZSFacilityDefinition.h"

namespace
{
	// 후보 정렬용. FFF_DestinationCandidate 에는 ZoneId 가 없어서(선택기가 안 쓴다) 여기서만 들고 있는다.
	struct FRankedCandidate
	{
		FFF_DestinationCandidate Candidate;

		// 0 = 현재 Zone, 1 = 인접 Zone. 낮은 쪽이 먼저 남는다
		int32 ZoneRank = 0;
	};

	// 목적지 후보가 될 수 없는 카테고리.
	bool IsSelectableCategory(EFFZSFacilityCategory Category)
	{
		return Category != EFFZSFacilityCategory::Info
			&& Category != EFFZSFacilityCategory::Safety;
	}

}

// 메인 공연 시간표와 전역 안내는 SunsetMainStage에만 적용한다.
// 다른 공연 시설은 일반 지역 후보이며, 편성된 프로그램의 예약 정책은 Decision 계층에서 유지한다.
bool FFF_ZoneSystemBridge::IsMainStageFacility(EFFZSFacilityCategory Category, FName FacilityDefinitionId)
{
	static const FName SunsetMainStageId(TEXT("Performance_MainStage_SunsetMainStage"));
	return Category == EFFZSFacilityCategory::Performance && FacilityDefinitionId == SunsetMainStageId;
}

TArray<FFF_DestinationCandidate> FFF_ZoneSystemBridge::BuildCandidates(
	const FFFZSReadModel& ReadModel,
	FName CurrentZoneId,
	const FVector& PartyWorldLocation,
	int32 MaxCandidates,
	const FFF_WalkDistanceMatrix* WalkDistances,
	bool bMainStageOpen,
	bool bIncludeGlobalMainStages)
{
	// 파티가 서 있는 존의 앵커까지 걸어 나가는 구간. 행렬이 없으면 쓰이지 않는다.
	// 앵커를 못 찾으면(등록되지 않은 존) 0으로 두어 행렬 경유분만 남긴다 —
	// 여기서 폴백을 통째로 끄면 존 하나가 빠졌을 때 모든 거리가 직선으로 되돌아가
	// "왜 갑자기 강 건너편을 고르는가"의 원인을 찾기 어려워진다.
	float PartyToOwnAnchorCm = 0.f;
	if (WalkDistances)
	{
		FVector OwnAnchorCm;
		if (WalkDistances->TryGetZoneAnchorCm(CurrentZoneId, OwnAnchorCm))
		{
			PartyToOwnAnchorCm = static_cast<float>(FVector::Dist(PartyWorldLocation, OwnAnchorCm));
		}
	}

	// 행렬을 켰는데 경로가 없어 직선으로 되돌아간 횟수. 조용히 섞이면 안 되는 값이다
	int32 WalkDistanceFallbacks = 0;
	// ── 1. 현재 Zone 과 인접 Zone 확정 ────────────────────────────────────────
	const FFFZSZoneReadModel* CurrentZone = ReadModel.Zones.FindByPredicate(
		[CurrentZoneId](const FFFZSZoneReadModel& Zone) { return Zone.ZoneId == CurrentZoneId; });

	if (!CurrentZone)
	{
		// 조용히 빈 배열을 돌려주면 "후보가 없어서 no_choice"와 "Zone 을 못 찾아서 no_choice"가
		// 구분되지 않는다. 이건 호출부가 잘못된 ZoneId 를 넘긴 것이므로 Warning 이다.
		UE_LOG(LogFestaFiestaNpc, Warning,
			TEXT("BuildCandidates: CurrentZoneId='%s' 가 ReadModel 에 없다(Zone %d개). ")
			TEXT("빈 후보를 돌려준다 — 호출부가 넘긴 ZoneId 를 확인할 것."),
			*CurrentZoneId.ToString(), ReadModel.Zones.Num());
		return TArray<FFF_DestinationCandidate>();
	}

	// ZoneId → 정렬 우선순위. 현재 Zone 0, 인접 Zone 1.
	TMap<FName, int32> ZoneRankById;
	ZoneRankById.Reserve(CurrentZone->NeighborZoneIds.Num() + 1);
	ZoneRankById.Add(CurrentZoneId, 0);
	for (const FName NeighborId : CurrentZone->NeighborZoneIds)
	{
		// 자기 자신이 이웃 목록에 들어 있어도 rank 0 을 덮어쓰지 않는다.
		if (!ZoneRankById.Contains(NeighborId))
		{
			ZoneRankById.Add(NeighborId, 1);
		}
	}

	// ── 2. 밀집도 입력 — Zone 단위 값을 시설에 그대로 물려준다 ────────────────
	//
	// !!! assumption !!! FFFZSFacilityReadModel 에는 시설 단위 점유·수용 값이 없다
	// (WaitingCount·ActiveServiceCount 는 대기열이고 밀집도가 아니다). 그래서 v0 은
	// "시설 앞 밀집도 ≈ 그 시설이 속한 Zone 의 밀집도"로 근사한다.
	// 같은 Zone 안 시설들의 density_cost 항이 전부 같아진다는 뜻이다 — Zone 안에서 사람이 어디에
	// 몰려 있는지는 반영되지 않는다. 셀 격자 밀도(이정원)가 붙으면 그걸로 교체한다.
	struct FZoneDensity
	{
		float Occupancy = 0.f;
		float Capacity = 1.f;
	};
	TMap<FName, FZoneDensity> DensityByZoneId;
	DensityByZoneId.Reserve(ReadModel.Zones.Num());
	for (const FFFZSZoneReadModel& Zone : ReadModel.Zones)
	{
		// 전역 무대도 도착 구역의 혼잡을 비용으로 지불한다.
		FZoneDensity Density;
		Density.Occupancy = static_cast<float>(Zone.ReportedPopulation);
		// SoftCapacity 0 은 "아직 설정 안 됨"이다. 선택기가 Capacity 로 나누므로 최소 1 을 보장한다.
		Density.Capacity = static_cast<float>(FMath::Max(Zone.SoftCapacity, 1));
		DensityByZoneId.Add(Zone.ZoneId, Density);
	}

	// ── 3. 시설 순회 ─────────────────────────────────────────────────────────
	TArray<FRankedCandidate> Ranked;
	Ranked.Reserve(FMath::Min(ReadModel.Facilities.Num(), 64));
	TArray<FFF_DestinationCandidate> GlobalMainStages;

	// 왜 세는가: 후보가 0개로 나왔을 때 "시설이 없다"와 "전부 닫혀 있다"와 "카테고리가 다 제외됐다"를
	// 로그만 보고 구분할 수 있어야 한다.
	int32 SkippedByZone = 0;
	int32 SkippedByState = 0;
	int32 SkippedByCategory = 0;
	int32 SkippedByMainStageGate = 0;

	for (const FFFZSFacilityReadModel& Facility : ReadModel.Facilities)
	{
		const bool bGlobalMainStage = bIncludeGlobalMainStages && bMainStageOpen
			&& IsMainStageFacility(Facility.Category, Facility.FacilityDefinitionId);
		const int32* ZoneRank = ZoneRankById.Find(Facility.ZoneId);
		if (!ZoneRank && !(bGlobalMainStage && DensityByZoneId.Contains(Facility.ZoneId)))
		{
			++SkippedByZone;
			continue;
		}

		if (!IsSelectableCategory(Facility.Category))
		{
			++SkippedByCategory;
			continue;
		}

		// Active 가 아니면 아직 안 지어졌거나(Planned/Validated) 닫혔거나(Closed/MovePending/Removed) 다.
		if (Facility.State != EFFZSFacilityState::Active)
		{
			++SkippedByState;
			continue;
		}

		// !!! QueueState 로 후보를 자르지 않는다 !!!
		//
		// 여기서는 ZoneSystem 이 말하는 사실만 본다 — State(수명주기)와 bNeedsRelocation.
		// "지금 이용 가능한가"의 판정은 FFF_QueueWaitAdapter 에만 맡긴다.
		// "잠깐 만석"은 후보에서 빠지지 않고 대기시간이 커지는 것으로만 표현된다.

		if (Facility.bNeedsRelocation)
		{
			++SkippedByState;
			continue;
		}

		// 메인 공연 시각이 아니면 SunsetMainStage만 후보에서 뺀다.
		// bIsOpen = false 로 넣어두면 Choose 가 건너뛰기는 하지만 MaxCandidates 한 칸을 먹는다 —
		// "닫힌 시설은 후보에 넣지 않는다"는 이 함수의 기존 정책과 같은 처리다.
		if (!bMainStageOpen
			&& IsMainStageFacility(Facility.Category, Facility.FacilityDefinitionId))
		{
			++SkippedByMainStageGate;
			continue;
		}

		FRankedCandidate Entry;
		Entry.ZoneRank = ZoneRank ? *ZoneRank : 2;

		FFF_DestinationCandidate& Candidate = Entry.Candidate;
		Candidate.DestinationId = FacilityInstanceIdToDestinationId(Facility.FacilityInstanceId);
		Candidate.Category = MapToPreferenceCategory(Facility.Category, Facility.FacilityDefinitionId);
		Candidate.NeedRelief = GetNeedReliefFlags(Facility.Category, Facility.FacilityDefinitionId);

		// 수명주기상 운영 중이라는 뜻이다. "지금 줄에 들어갈 수 있는가"는 Queue 어댑터가 AND 로 닫는다.
		Candidate.bIsOpen = true;

		// !!! 출처 없음 !!! attraction_norm(0~1) 정규화 설계가 부스 쪽에서 아직 안 나왔다.
		// 0 으로 두면 모든 후보의 attraction 항이 같아져 항이 무력화될 뿐, 특정 시설이 유리해지지 않는다.
		// 임의의 상수(0.5 등)를 넣으면 "근거 있는 값"처럼 보이므로 넣지 않는다.
		Candidate.BaseAttraction = 0.f;

		// ZoneSystem 은 cm, 선택기는 m. 환산은 이 파일에서만 한다.
		//
		// 전제: WorldLocation 은 AFFZSFacilityActor::GetReadModel() 한 곳에서만 채워진다
		// [주의] 다른 경로로 FFFZSFacilityReadModel 을 만들면 기본값 FVector::ZeroVector 가 남아 
		// "Party 에서 월드 원점까지의 거리"가 조용히 들어온다 —
		// 0 이 아니라 그럴듯한 숫자라서 테스트로도, 로그로도 안 잡힌다.
		//
		// 거리 근사: |파티 → 출발존 앵커| + M[출발존][도착존] + |도착존 앵커 → 시설|
		// 같은 존이면 앵커를 경유하지 않는다 — 존 내부는 열린 공간이고, 경유시키면
		// 옆 부스가 앵커까지 돌아가는 거리로 잡혀 오히려 틀린다.
		float DistanceCm = static_cast<float>(FVector::Dist(PartyWorldLocation, Facility.WorldLocation));

		if (WalkDistances && Facility.ZoneId != CurrentZoneId)
		{
			float ZoneToZoneCm = 0.f;
			FVector TargetAnchorCm;

			if (WalkDistances->TryGetWalkDistanceCm(CurrentZoneId, Facility.ZoneId, ZoneToZoneCm)
				&& WalkDistances->TryGetZoneAnchorCm(Facility.ZoneId, TargetAnchorCm))
			{
				DistanceCm = PartyToOwnAnchorCm
					+ ZoneToZoneCm
					+ static_cast<float>(FVector::Dist(TargetAnchorCm, Facility.WorldLocation));
			}
			else
			{
				// 경로가 없다. 직선거리를 그대로 쓴다 — 큰 값을 넣어 후보에서 빼면
				// "왜 저 부스가 아무도 안 가는가"가 로그에 남지 않는다.
				++WalkDistanceFallbacks;
			}
		}

		Candidate.DistanceMeters = CentimetersToMeters(DistanceCm);

		// EstimatedWaitMinutes 는 채우지 않는다(0으로 남는다) — 소유자는 FFF_QueueWaitAdapter 다.
		// 이유는 FF_ZoneSystemBridge.h 의 "대기시간은 이 파일에서 계산하지 않는다" 주석 참조.
		// 0 은 Choose()에서 "대기 없음"으로 계산되므로, 어댑터 결선 전 wait_cost 는 신뢰할 수 없다.

		if (const FZoneDensity* Density = DensityByZoneId.Find(Facility.ZoneId))
		{
			Candidate.CurrentOccupancy = Density->Occupancy;
			Candidate.Capacity = Density->Capacity;
		}

		// 지역 후보 상한과 별도다. 같은 구역의 메인 무대도 이쪽에만 넣어 중복과 상한 소모를 막는다.
		if (bGlobalMainStage)
		{
			GlobalMainStages.Add(MoveTemp(Candidate));
		}
		else
		{
			Ranked.Add(MoveTemp(Entry));
		}
	}

	// ── 4. 상한 적용 ─────────────────────────────────────────────────────────
	//
	// 정렬 기준을 세 단계로 두는 이유: 같은 Zone 을 인접 Zone 보다 먼저 남기고(도달 비용이 낮다),
	// 그다음 가까운 순으로 남기고, 마지막에 DestionationId 로 동점을 깬다.
	// 마지막 단계가 없으면 같은 스냅샷에서 호출 순서에 따라 다른 후보가 잘려 재현성이 깨진다.
	const bool bNeedsTruncation = MaxCandidates > 0 && Ranked.Num() > MaxCandidates;
	if (bNeedsTruncation)
	{
		Ranked.Sort([](const FRankedCandidate& A, const FRankedCandidate& B)
		{
			if (A.ZoneRank != B.ZoneRank)
			{
				return A.ZoneRank < B.ZoneRank;
			}
			if (!FMath::IsNearlyEqual(A.Candidate.DistanceMeters, B.Candidate.DistanceMeters))
			{
				return A.Candidate.DistanceMeters < B.Candidate.DistanceMeters;
			}
			return A.Candidate.DestinationId.Compare(B.Candidate.DestinationId) < 0;
		});
		Ranked.SetNum(MaxCandidates);
	}

	TArray<FFF_DestinationCandidate> Candidates;
	Candidates.Reserve(Ranked.Num() + GlobalMainStages.Num());
	for (FRankedCandidate& Entry : Ranked)
	{
		Candidates.Add(MoveTemp(Entry.Candidate));
	}
	GlobalMainStages.Sort([](const FFF_DestinationCandidate& A, const FFF_DestinationCandidate& B)
	{
		return A.DestinationId.Compare(B.DestinationId) < 0;
	});
	Candidates.Append(GlobalMainStages);

	// ── 5. 진단 ──────────────────────────────────────────────────────────────

	// 행렬을 켰는데 직선으로 되돌아간 후보가 있으면 알린다.
	// 이게 조용히 섞이면 일부 후보만 다른 척도로 재진 상태가 되고, 그 상태의 결정 로그는
	// "가까운 곳을 골랐다"고 말하면서 실제로는 강 건너편을 고른다.
	if (WalkDistanceFallbacks > 0)
	{
		UE_LOG(LogFestaFiestaNpc, Warning,
			TEXT("BuildCandidates: Zone='%s' 후보 %d개가 보행거리 없이 직선으로 계산됐다 — ")
			TEXT("존 그래프와 NavMesh 연결성이 어긋났을 수 있다(행렬 빌드 로그의 불일치 항목 확인)"),
			*CurrentZoneId.ToString(), WalkDistanceFallbacks);
	}

	if (Candidates.IsEmpty())
	{
		// 후보 0개는 항상 no_choice 를 만든다. 원인이 데이터인지 필터인지 여기서 갈라준다.
		UE_LOG(LogFestaFiestaNpc, Warning,
			TEXT("BuildCandidates: Zone='%s' 후보가 0개다 — 이 Party 는 반드시 no_choice 가 된다. ")
			TEXT("제외 내역: Zone밖 %d / 카테고리 %d / 상태 %d / 메인공연시각아님 %d (전체 시설 %d개)"),
			*CurrentZoneId.ToString(), SkippedByZone, SkippedByCategory, SkippedByState,
			SkippedByMainStageGate, ReadModel.Facilities.Num());
	}
	else
	{
		UE_LOG(LogFestaFiestaNpc, Verbose,
			TEXT("BuildCandidates: Zone='%s'(인접 %d) → 후보 %d개%s. ")
			TEXT("제외: Zone밖 %d / 카테고리 %d / 상태 %d / 메인공연시각아님 %d"),
			*CurrentZoneId.ToString(), CurrentZone->NeighborZoneIds.Num(), Candidates.Num(),
			bNeedsTruncation ? TEXT("(상한으로 잘림)") : TEXT(""),
			SkippedByZone, SkippedByCategory, SkippedByState, SkippedByMainStageGate);
	}

	// 전역 예외는 공연 중 메인 무대뿐이다. 일반 인기 시설의 전역 상위 K 정책은 아직 없다.

	// TODO(예약 필터): reservation_program_spec.md §4.1. `예약 필요` 시설은 그 Party 가 예약을
	// 갖고 있지 않으면 제외해야 한다. 태그 출처(한광훈)가 정해지면 위 필터 옆에 붙인다.

	// TODO(예약 귀환 안전장치): 판정 함수는 이미 있다
	// `FFF_ProgramReservationReturnTrip::Evaluate` (별 파일로 파킹, 테스트 6개로 고정).
	// `EstimatedWaitMinutes` 가 아직 0 이기 때문에 아직 호출하지 않는다.
	// 0 이면 "줄이 없다"로 계산되어 판정이 낙관적으로 기운다 
	// Queue 어댑터 결선(= 그 값이 실제 값이 되는 시점) 후에 잇는다.
	//
	// [붙이는 방법]
	// 위 카테고리·상태 필터 옆에서 Party 의 예약 정보를 받아 `bCanReturnInTime == false` 인 후보를 제외한다. 
	// 보행 속도는 계수 `npc_walk_speed_mps`(1.5)를 쓴다 — 출처는 crowdAI 의 `FUnitData::Speed` 150 cm/s 다. 
	// 개체 변주 `WalkSpeedMultiplier` 가 결선되면(지금은 만들기만 하고 소비하는 곳이 0곳이다) 그 배수를 곱한다.
	// 그때까지는 2차 안전장치(하드 데드라인, L3 소관)만으로 동작한다.
	
	return Candidates;
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
			TEXT("이 카테고리의 아는 소분류: %s. 규칙 전문은 data/metadata/request/facility_naming_convention.md."),
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

	const FPrimaryAssetId AssetId(
		UFFZSFacilityDefinition::GetFacilityDefinitionAssetType(),
		FacilityDefinitionId);
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
