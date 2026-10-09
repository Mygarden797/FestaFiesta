// Copyright Epic Games, Inc. All Rights Reserved.

#include "Festival/FF_FestivalDefinition.h"

#include "DataTables/FF_FacilityCostRow.h"
#include "DataTables/FF_FacilityRevenueTableRow.h"
#include "Engine/DataTable.h"
#include "FF_FFZSFacilityDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "FestaFiestaEconomy"

UFF_FestivalDefinition::UFF_FestivalDefinition()
{
	// 경로만 지정한다. CDO 생성 중에는 에셋을 로드하지 않는다.
	FacilityRevenueTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(
		TEXT("/Game/FestaFiesta/Data/Economy/DT_FacilityRevenues.DT_FacilityRevenues")));

	FFF_RequiredFacility MainStage;
	MainStage.FacilityDefinition = TSoftObjectPtr<UFFZSFacilityDefinition>(FSoftObjectPath(
		TEXT("/Game/FestaFiesta/Building/FF_Data_Building/Performance/PDA_FFZS_Performance_MainStage_SunsetMainStage.PDA_FFZS_Performance_MainStage_SunsetMainStage")));
	RequiredSetupFacilities.Add(MainStage);

	// 스태프가 배정되는 안내 시설(Info) 1곳. 설치만으로는 아무도 배정되지 않으므로(InfoFacility_Staff_Guide_KO.md)
	// 1명 이상 배정해야 진행되게 해서, 플레이어가 `+ 배정` 을 반드시 한 번은 거치게 한다.
	FFF_RequiredFacility MainInfo;
	MainInfo.FacilityDefinition = TSoftObjectPtr<UFFZSFacilityDefinition>(FSoftObjectPath(
		TEXT("/Game/FestaFiesta/Building/FF_Data_Building/Info/PDA_FFZS_Info_GeneralInfo_General.PDA_FFZS_Info_GeneralInfo_General")));
	MainInfo.MinimumAssignedStaff = 1;
	RequiredSetupFacilities.Add(MainInfo);

	// 등급이 올라갈수록 비용과 보너스가 함께 커진다.
	FinaleTiers = {
		{ EFF_FinaleGrade::C, 1000,  3 },
		{ EFF_FinaleGrade::B, 1800,  6 },
		{ EFF_FinaleGrade::A, 2600, 10 },
		{ EFF_FinaleGrade::S, 3500, 15 }
	};
	
	RevenuePerUseByMinorToken = {
		{ TEXT("FoodStall"),       20 },
		{ TEXT("MarketGoods"),     25 },
		{ TEXT("CraftExperience"), 30 },
		{ TEXT("ActivityGame"),    30 },
		{ TEXT("Nightlife"),       40 },
	};

	// 유료 프로그램 카드 v0. 정원·체험비는 [기획서] §9.1 값 그대로이고,
	// 비용만 하루 기준으로 내렸다(원값 400·500 은 기간권 전제라 1일권으로는 손해다).
	{
		FFF_ProgramCard Workshop;
		Workshop.ProgramId = TEXT("Program_SandArtWorkshop");
		Workshop.DisplayName = LOCTEXT("Program_SandArtWorkshop", "바다향기 공방");
		Workshop.CostGold = 200;
		Workshop.Capacity = 12;
		Workshop.FeePerPerson = 30;   // 정원 다 차면 360, 확정 매출이다
		Workshop.StartMinutes = 30.f; 
		Workshop.EndMinutes = 60.f;
		// 공방 체험은 아이 동반 가족층이 잡는다는 가정. 프로그램이 데려오는 손님의
		// 아키타입이라 유입 인구 구성과 별개로 이 값이 그대로 스폰된다.
		Workshop.PreferredArchetypeId = TEXT("life_stage_30_40s");
		// 공예·제작 체험 부스에서만 열린다. 화장실에 공방을 여는 경로를 막는다.
		Workshop.AllowedFacilityTypes = { TEXT("Booth_CraftExperience") };

		FFF_ProgramCard Yoga;
		Yoga.ProgramId = TEXT("Program_SunsetYoga");
		Yoga.DisplayName = LOCTEXT("Program_SunsetYoga", "선셋 요가");
		Yoga.CostGold = 250;
		Yoga.Capacity = 16;
		Yoga.FeePerPerson = 25;       // 정원 다 차면 400, 확정 매출이다
		Yoga.StartMinutes = 90.f;
		Yoga.EndMinutes = 120.f;
		Yoga.PreferredArchetypeId = TEXT("life_stage_20s");
		// 참여형 무대. 관람용 무대(MainStage·SubStage)와 구분된다.
		Yoga.AllowedFacilityTypes = { TEXT("Performance_ExperienceStage") };

		ProgramCatalog = { Workshop, Yoga };
	}

	// "홍보 없음"은 데이터가 아니라 미구매 상태다 (0 Gold · NoPromotionVisitorMultiplier).
	// 지역 SNS 가 기준선(×1.00)이라 안 사면 손해다.
	PromotionBasic.CostGold = 600;
	PromotionBasic.VisitorMultiplier = 1.f;
	PromotionBasic.StartingBuzz = 6;

	PromotionFocused.CostGold = 1400;
	PromotionFocused.VisitorMultiplier = 1.12f;
	PromotionFocused.StartingBuzz = 14;

	PromotionInfluencer.CostGold = 2600;
	PromotionInfluencer.VisitorMultiplier = 1.25f;
	PromotionInfluencer.StartingBuzz = 26;
}

UDataTable* UFF_FestivalDefinition::GetFacilityRevenueTable() const
{
	return FacilityRevenueTable.LoadSynchronous();
}

bool UFF_FestivalDefinition::IsDefinitionValid(TArray<FText>& OutErrors) const
{
	const int32 ErrorCountBefore = OutErrors.Num();

	TSet<FName> RequiredIds;
	for (const FFF_RequiredFacility& Requirement : RequiredSetupFacilities)
	{
		const UFFZSFacilityDefinition* Facility = Requirement.FacilityDefinition.LoadSynchronous();
		if (!Facility || Facility->Spec.FacilityDefinitionId.IsNone() || Requirement.MinimumCount < 1)
		{
			OutErrors.Add(LOCTEXT("InvalidRequiredFacility", "필수 설치 시설의 정의와 최소 개수(1 이상)를 확인하세요."));
			continue;
		}
		if (RequiredIds.Contains(Facility->Spec.FacilityDefinitionId))
		{
			OutErrors.Add(LOCTEXT("DuplicateRequiredFacility", "필수 설치 시설 종류가 중복되었습니다. 한 행의 최소 개수로 설정하세요."));
		}
		RequiredIds.Add(Facility->Spec.FacilityDefinitionId);
		if (Facility->Spec.MaxInstancesPerWorld > 0 && Requirement.MinimumCount > Facility->Spec.MaxInstancesPerWorld)
		{
			OutErrors.Add(LOCTEXT("RequiredFacilityExceedsLimit", "필수 설치 개수가 시설의 월드 설치 상한을 초과합니다."));
		}
		// 판정 규칙은 UFF_RequiredFacilitiesSubsystem::EvaluateRequirements 와 같다. 여기는 저작 단계 안내다.
		if (Requirement.MinimumAssignedStaff > 0)
		{
			const int32 StaffCapacity = Facility->Spec.GetStaffCapacity();
			if (StaffCapacity <= 0)
			{
				OutErrors.Add(LOCTEXT("RequiredStaffWithoutCapacity",
					"스태프 배정을 요구하는 필수 시설은 Staff Capacity가 1 이상인 Info 시설이어야 합니다."));
			}
			else if (Facility->Spec.MaxInstancesPerWorld > 0
				&& static_cast<int64>(Requirement.MinimumAssignedStaff)
					> static_cast<int64>(StaffCapacity) * Facility->Spec.MaxInstancesPerWorld)
			{
				OutErrors.Add(LOCTEXT("RequiredStaffExceedsCapacity",
					"필수 배정 인원이 설치할 수 있는 시설의 Staff Capacity 합을 초과합니다."));
			}
		}
	}

	// --- 식별 ---
	if (FestivalId.IsNone())
	{
		OutErrors.Add(LOCTEXT("NoFestivalId", "FestivalId 가 비어 있다. 축제를 식별할 수 없다."));
	}

	if (FestivalDayCount < 1)
	{
		OutErrors.Add(FText::Format(
			LOCTEXT("BadDayCount", "FestivalDayCount 가 {0} 이다. 최소 1 이어야 한다."),
			FestivalDayCount));
	}

	// --- 초기 자원 ---
	// 0 이면 Setup 에서 아무것도 못 지어 게임이 시작되지 않는다.
	if (InitialGold <= 0)
	{
		OutErrors.Add(FText::Format(
			LOCTEXT("BadInitialGold", "InitialGold 가 {0} 이다. Setup 에서 시설을 지을 수 없다."),
			InitialGold));
	}

	if (InitialStaffCount < 0)
	{
		OutErrors.Add(LOCTEXT("BadStaffCount", "InitialStaffCount 가 음수다."));
	}

	// --- 스태프 고용 ---
	if (StaffHireCostGold < 0 || MaxHiredStaffCount < 0)
	{
		OutErrors.Add(LOCTEXT("BadHireCostOrCap", "고용 비용·총원 상한에 음수가 있다."));
	}
	
	// --- 뒷정리 ---
	// 등급이 올라가는데 값이 더 싸면 상위 등급을 고를 이유가 사라진다.
	if (!(CleanupCostMinimalGold <= CleanupCostStandardGold && CleanupCostStandardGold <= CleanupCostFullGold))
	{
		OutErrors.Add(FText::Format(
			LOCTEXT("CleanupNotAscending", "뒷정리 비용이 오름차순이 아니다: {0} / {1} / {2}."),
			CleanupCostMinimalGold, CleanupCostStandardGold, CleanupCostFullGold));
	}

	// --- 피날레 ---
	if (FinaleTiers.Num() == 0)
	{
		OutErrors.Add(LOCTEXT("NoFinaleTiers", "FinaleTiers 가 비어 있다. 피날레를 구매할 수 없다."));
	}
	else
	{
		// None 은 "구매하지 않음"이지 구매 가능한 등급이 아니다. 구매 UI 에 넣지 않는다.
		TSet<EFF_FinaleGrade> SeenGrades;
		for (const FFF_FinaleTier& Tier : FinaleTiers)
		{
			if (Tier.Grade == EFF_FinaleGrade::None)
			{
				OutErrors.Add(LOCTEXT("FinaleNoneTier", "FinaleTiers 에 None 등급이 있다. None 은 미구매를 뜻하므로 목록에 넣지 않는다."));
			}
			else if (SeenGrades.Contains(Tier.Grade))
			{
				OutErrors.Add(FText::Format(
					LOCTEXT("FinaleDuplicateGrade", "FinaleTiers 에 등급 {0} 이 중복이다."),
					FText::AsNumber(static_cast<int32>(Tier.Grade))));
			}
			SeenGrades.Add(Tier.Grade);

			if (Tier.CostGold < 0 || Tier.SatisfactionBonus < 0)
			{
				OutErrors.Add(LOCTEXT("FinaleNegative", "FinaleTiers 에 음수 비용 또는 음수 보너스가 있다."));
			}
		}

		// 등급이 높을수록 비싸고 보너스도 커야 한다.
		TArray<FFF_FinaleTier> Sorted = FinaleTiers;
		Sorted.Sort([](const FFF_FinaleTier& A, const FFF_FinaleTier& B)
		{
			return static_cast<uint8>(A.Grade) < static_cast<uint8>(B.Grade);
		});

		for (int32 Index = 1; Index < Sorted.Num(); ++Index)
		{
			if (Sorted[Index].CostGold <= Sorted[Index - 1].CostGold
				|| Sorted[Index].SatisfactionBonus <= Sorted[Index - 1].SatisfactionBonus)
			{
				OutErrors.Add(LOCTEXT("FinaleNotMonotonic",
					"FinaleTiers 가 단조 증가가 아니다. 등급이 높을수록 비용과 보너스가 모두 커야 한다."));
				break;
			}
		}
	}

	// --- 수익원 ---
	if (!FMath::IsFinite(BoothCommissionRate) || BoothCommissionRate < 0.f || BoothCommissionRate > 1.f)
	{
		OutErrors.Add(FText::Format(
			LOCTEXT("BadCommissionRate", "BoothCommissionRate 가 {0} 이다. 0.0~1.0 이어야 한다."),
			FText::AsNumber(BoothCommissionRate)));
	}

	if (AdmissionFeeGold < 0 || DailyGoalBonusGold < 0 || MinigameRewardMaxGold < 0)
	{
		OutErrors.Add(LOCTEXT("NegativeRevenue", "수익원 값에 음수가 있다."));
	}

	// --- 철거 ---
	if (FacilityDemolitionRefundRate < 0.f || FacilityDemolitionRefundRate > 1.f)
	{
		OutErrors.Add(FText::Format(
			LOCTEXT("BadRefundRate", "FacilityDemolitionRefundRate 가 {0} 이다. 0.0~1.0 이어야 한다."),
			FText::AsNumber(FacilityDemolitionRefundRate)));
	}

	// --- 홍보 ---
	// 배열이 아니라 멤버 2개라서 "비어 있다 / 중복 / 순서 섞임" 검사가 필요 없다. 값만 본다.

	// 배수가 1.0 미만이면 홍보가 방문객을 줄이게 된다.
	// 비용 0 이면 무료 홍보가 되어 "안 함"과 구분되지 않는다.
	if (PromotionBasic.CostGold <= 0 || PromotionBasic.VisitorMultiplier < 1.f
		|| PromotionFocused.CostGold <= 0 || PromotionFocused.VisitorMultiplier < 1.f
		|| PromotionInfluencer.CostGold <= 0 || PromotionInfluencer.VisitorMultiplier < 1.f)
	{
		OutErrors.Add(LOCTEXT("BadPromotionTier",
			"홍보 등급이 잘못됐다. 비용은 1 이상, 방문객 배수는 1.0 이상이어야 한다."));
	}

	// 윗 등급이 더 비싼데 효과가 같거나 낮으면 그 등급은 함정이 된다.
	if (PromotionFocused.CostGold <= PromotionBasic.CostGold
		|| PromotionFocused.VisitorMultiplier <= PromotionBasic.VisitorMultiplier
		|| PromotionInfluencer.CostGold <= PromotionFocused.CostGold
		|| PromotionInfluencer.VisitorMultiplier <= PromotionFocused.VisitorMultiplier)
	{
		OutErrors.Add(LOCTEXT("PromotionNotMonotonic",
			"윗 등급 홍보일수록 비싸면서 방문객 배수도 더 커야 한다(지역 SNS < 전국 < 인플루언서)."));
	}

	if (NoPromotionVisitorMultiplier > PromotionBasic.VisitorMultiplier)
	{
		OutErrors.Add(LOCTEXT("NoPromotionAboveBasic",
			"홍보 없음의 방문객 배수가 지역 SNS 보다 크면 홍보를 살 이유가 없다."));
	}

	// --- 입장료 ---
	if (AdmissionFeeMinGold > AdmissionFeeMaxGold
		|| AdmissionFeeGold < AdmissionFeeMinGold || AdmissionFeeGold > AdmissionFeeMaxGold)
	{
		OutErrors.Add(LOCTEXT("BadAdmissionFeeRange",
			"입장료 범위가 잘못됐다. Min <= 기준값(AdmissionFeeGold) <= Max 여야 한다."));
	}
	if (AdmissionFeeMinVisitorMultiplier > 1.f)
	{
		OutErrors.Add(LOCTEXT("BadAdmissionFeeMultiplier",
			"입장료 최소 방문객 배수가 1.0 보다 크면 비싸게 받아도 손님이 줄지 않는다."));
	}

	// --- 참조 표 ---
	// 비용표가 없으면 시설 설치비 차감이 전부 실패한다.
	if (FacilityCostTable == nullptr)
	{
		OutErrors.Add(LOCTEXT("NoFacilityCostTable", "FacilityCostTable 이 비어 있다. 시설 설치비를 조회할 수 없다."));
	}
	else if (FacilityCostTable->GetRowStruct() != FFF_FacilityCostRow::StaticStruct())
	{
		// 다른 CSV 를 잘못 임포트한 경우
		OutErrors.Add(FText::Format(
			LOCTEXT("BadFacilityCostRowStruct", "FacilityCostTable 의 행 구조체가 FFF_FacilityCostRow 가 아니다 (현재: {0})."),
			FText::FromString(GetNameSafe(FacilityCostTable->GetRowStruct()))));
	}
	else if (FacilityCostTable->GetRowMap().Num() == 0)
	{
		OutErrors.Add(LOCTEXT("EmptyFacilityCostTable", "FacilityCostTable 에 행이 없다."));
	}

	// 표 연결을 비운 축제는 기존 시설 Data Asset/소분류 기본값을 그대로 사용할 수 있다.
	if (!FacilityRevenueTable.IsNull())
	{
		const UDataTable* RevenueTable = GetFacilityRevenueTable();
		if (!RevenueTable)
		{
			OutErrors.Add(LOCTEXT("MissingFacilityRevenueTable", "Facility Revenue Table을 불러올 수 없습니다. DT_FacilityRevenues 연결을 확인하세요."));
		}
		else if (RevenueTable->GetRowStruct() != FFF_FacilityRevenueTableRow::StaticStruct())
		{
			OutErrors.Add(LOCTEXT("BadFacilityRevenueRowStruct", "Facility Revenue Table의 행 구조체는 FF_FacilityRevenueTableRow여야 합니다."));
		}
		else
		{
			for (const auto& Entry : RevenueTable->GetRowMap())
			{
				const FFF_FacilityRevenueTableRow* Row = reinterpret_cast<const FFF_FacilityRevenueTableRow*>(Entry.Value);
				if (Entry.Key.IsNone() || !Row)
				{
					OutErrors.Add(FText::Format(LOCTEXT("BadFacilityRevenueRow", "시설 수익표의 '{0}' 행을 확인하세요. 행 이름은 유효한 Facility Definition Id여야 합니다."),
						FText::FromName(Entry.Key)));
				}
				if (Row)
				{
					TArray<FText> RowErrors;
					Row->IsRowValid(RowErrors);
					for (const FText& Error : RowErrors)
					{
						OutErrors.Add(FText::Format(LOCTEXT("InvalidFacilityRevenueFields", "시설 수익표 '{0}': {1}"),
							FText::FromName(Entry.Key), Error));
					}
				}
			}
		}
	}

	return OutErrors.Num() == ErrorCountBefore;
}

FPrimaryAssetId UFF_FestivalDefinition::GetPrimaryAssetId() const
{
	if (!FestivalId.IsNone())
	{
		return FPrimaryAssetId(TEXT("FestivalDefinition"), FestivalId);
	}

	// ID 가 없으면 부모 규칙(에셋 경로 기반)을 쓴다. 검증이 별도로 오류를 낸다.
	return Super::GetPrimaryAssetId();
}

#if WITH_EDITOR
EDataValidationResult UFF_FestivalDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TArray<FText> Errors;
	if (!IsDefinitionValid(Errors))
	{
		for (const FText& Error : Errors)
		{
			Context.AddError(Error);
		}
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
