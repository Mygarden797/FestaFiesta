// Copyright Epic Games, Inc. All Rights Reserved.

#include "Festival/FF_FestivalDefinition.h"

#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "FestaFiestaEconomy"

UFF_FestivalDefinition::UFF_FestivalDefinition()
{
	// 등급이 올라갈수록 비용과 보너스가 함께 커진다.
	FinaleTiers = {
		{ EFF_FinaleGrade::C, 2000,  5 },
		{ EFF_FinaleGrade::B, 4000, 12 },
		{ EFF_FinaleGrade::A, 6000, 20 },
		{ EFF_FinaleGrade::S, 8000, 30 }
	};
	
	RevenuePerUseByMinorToken = {
		{ TEXT("FoodStall"),       20 },
		{ TEXT("MarketGoods"),     25 },
		{ TEXT("CraftExperience"), 30 },
		{ TEXT("ActivityGame"),    30 },
		{ TEXT("Nightlife"),       40 },
	};

	// "안 함"은 데이터가 아니라 미구매 상태다 (0 Gold · 배수 1.0)
	PromotionBasic.CostGold = 500;
	PromotionBasic.VisitorMultiplier = 1.08f;

	PromotionFocused.CostGold = 1200;
	PromotionFocused.VisitorMultiplier = 1.18f;
}

bool UFF_FestivalDefinition::IsDefinitionValid(TArray<FText>& OutErrors) const
{
	const int32 ErrorCountBefore = OutErrors.Num();

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

	// --- 고정비 ---
	if (DailyVenueRentGold < 0)
	{
		OutErrors.Add(LOCTEXT("BadRecurringCost", "고정비에 음수가 있다."));
	}

	// --- 스태프 임금/고용 ---
	if (DailyStaffWageGold < 0 || MaxHiredStaffPerDay < 0)
	{
		OutErrors.Add(LOCTEXT("BadWageOrHireCap", "임금·고용 상한에 음수가 있다."));
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
	if (BoothCommissionRate < 0.f || BoothCommissionRate > 1.f)
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
		|| PromotionFocused.CostGold <= 0 || PromotionFocused.VisitorMultiplier < 1.f)
	{
		OutErrors.Add(LOCTEXT("BadPromotionTier",
			"홍보 등급이 잘못됐다. 비용은 1 이상, 방문객 배수는 1.0 이상이어야 한다."));
	}

	// 집중이 기본보다 비싼데 효과가 같거나 낮으면 그 등급은 함정이 된다.
	if (PromotionFocused.CostGold <= PromotionBasic.CostGold
		|| PromotionFocused.VisitorMultiplier <= PromotionBasic.VisitorMultiplier)
	{
		OutErrors.Add(LOCTEXT("PromotionNotMonotonic",
			"집중 홍보가 기본보다 비싸면서 방문객 배수도 더 커야 한다."));
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
