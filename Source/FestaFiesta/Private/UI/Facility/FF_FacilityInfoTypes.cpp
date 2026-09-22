// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Facility/FF_FacilityInfoTypes.h"
#include "Building/FF_FacilityUpgradeTypes.h"
#include "Building/Synergy/FF_FacilitySynergyTypes.h"
#include "Economy/FF_FestivalEconomyTypes.h"

#define LOCTEXT_NAMESPACE "FFFacilityInfoModel"

namespace FFFacilityInfoModel
{
	namespace
	{
		FText FormatSignedChange(const double Value, const FNumberFormattingOptions* Options = nullptr)
		{
			const FText Number = FText::AsNumber(Value, Options);
			return Value > 0.0 ? FText::Format(LOCTEXT("PositiveChange", "+{0}"), Number) : Number;
		}
	}

	void ApplySynergyPreview(const FFF_FacilitySynergyPreview* Preview, const bool bSelectionEligible,
		const float SelectedBonusPercent, const TMap<FName, FText>& FacilityNames,
		FFF_FacilityInfoViewData& ViewData)
	{
		ViewData.SynergyEntries.Reset();
		if (!Preview)
		{
			ViewData.SynergySummaryText = LOCTEXT("SynergyUnavailable", "조합 정보를 준비하고 있습니다.");
			return;
		}
		if (!Preview->bRulesAvailable || Preview->Entries.IsEmpty())
		{
			ViewData.SynergySummaryText = LOCTEXT("NoSynergyRules", "이 시설에 설정된 조합이 없습니다.");
			return;
		}
		FNumberFormattingOptions Format;
		Format.SetMaximumFractionalDigits(1);
		auto Number = [&Format](float Value) { return FText::AsNumber(Value, &Format); };
		auto Name = [&FacilityNames](FName Id)
		{
			const FText* Found = FacilityNames.Find(Id);
			return Found && !Found->IsEmpty() ? *Found : LOCTEXT("SynergyUnnamedFacility", "이름이 설정되지 않은 시설");
		};
		auto Range = [&Number](float Minimum, float Maximum)
		{
			return FMath::IsNearlyEqual(Minimum, Maximum) ? Number(Maximum)
				: FText::Format(LOCTEXT("SynergyPercentRange", "{0}~{1}"), Number(Minimum), Number(Maximum));
		};
		struct FRuleGroup
		{
			const FFF_FacilitySynergyPreviewEntry* Representative = nullptr;
			bool bHasTargetRole = false;
			bool bHasPartnerRole = false;
			bool bHasCounterpart = false;
			int32 NearbyCount = 0;
			int32 ActiveCount = 0;
			float NearestMeters = TNumericLimits<float>::Max();
			float MinApplied = TNumericLimits<float>::Max();
			float MaxApplied = 0.f;
			float MinTotal = TNumericLimits<float>::Max();
			float MaxTotal = 0.f;
			TSet<FGuid> SeenTargets;
		};
		TArray<FRuleGroup> Groups;
		TMap<FName, int32> GroupByRule;
		for (const FFF_FacilitySynergyPreviewEntry& Entry : Preview->Entries)
		{
			int32* GroupIndex = GroupByRule.Find(Entry.RuleId);
			if (!GroupIndex)
			{
				const int32 NewIndex = Groups.AddDefaulted();
				GroupByRule.Add(Entry.RuleId, NewIndex);
				GroupIndex = GroupByRule.Find(Entry.RuleId);
			}
			FRuleGroup& Group = Groups[*GroupIndex];
			if (!Group.Representative || Entry.bCandidateIsTarget) Group.Representative = &Entry;
			Group.bHasTargetRole |= Entry.bCandidateIsTarget;
			Group.bHasPartnerRole |= !Entry.bCandidateIsTarget;
			Group.bHasCounterpart |= Entry.bHasPartner;
			if (Entry.bHasPartner) Group.NearestMeters = FMath::Min(Group.NearestMeters, Entry.DistanceMeters);
			if (!Entry.bHasPartner || !Entry.bInRange || Group.SeenTargets.Contains(Entry.TargetInstanceId)) continue;
			Group.SeenTargets.Add(Entry.TargetInstanceId);
			++Group.NearbyCount;
			// An ineligible selection is absent from the live index; its placement preview is a
			// hypothetical repair. Do not advertise that hypothetical bonus as currently applied.
			const float Applied = bSelectionEligible ? Entry.AppliedBonusPercent : 0.f;
			if (Applied > 0.f) ++Group.ActiveCount;
			Group.MinApplied = FMath::Min(Group.MinApplied, Applied);
			Group.MaxApplied = FMath::Max(Group.MaxApplied, Applied);
			Group.MinTotal = FMath::Min(Group.MinTotal, Entry.CurrentTotalBonusPercent);
			Group.MaxTotal = FMath::Max(Group.MaxTotal, Entry.CurrentTotalBonusPercent);
		}
		int32 ActiveRules = 0;
		for (const FRuleGroup& Group : Groups)
		{
			const FFF_FacilitySynergyPreviewEntry& Source = *Group.Representative;
			FFF_FacilityInfoSynergyEntry& Entry = ViewData.SynergyEntries.AddDefaulted_GetRef();
			Entry.RuleId = Source.RuleId;
			Entry.PartnerFacilityDefinitionId = Group.bHasTargetRole ? Source.PartnerFacilityId : Source.TargetFacilityId;
			// A recipe may have its own name. Renaming/localizing either ingredient must
			// not overwrite it or change navigation; missing titles use current ID-resolved names.
			Entry.TitleText = Source.DisplayName.IsEmpty()
				? FText::Format(LOCTEXT("SynergyUnnamedRecipe", "{0} + {1}"),
					Name(Source.TargetFacilityId), Name(Source.PartnerFacilityId))
				: Source.DisplayName;
			Entry.PartnerText = Group.bHasTargetRole
				? FText::Format(LOCTEXT("SynergyPartner", "함께 놓을 시설: {0}"), Name(Source.PartnerFacilityId))
				: FText::Format(LOCTEXT("SynergyBeneficiary", "수익을 받는 시설: {0}"), Name(Source.TargetFacilityId));
			const float MinApplied = Group.NearbyCount > 0 ? Group.MinApplied : 0.f;
			const float MaxApplied = Group.NearbyCount > 0 ? Group.MaxApplied : 0.f;
			Entry.EffectText = FText::Format(LOCTEXT("SynergyRuleEffect", "{0} 이용 수수료\n설정 +{1}% · 현재 적용 +{2}%"),
				Name(Source.TargetFacilityId), Number(Source.ConfiguredBonusPercent), Range(MinApplied, MaxApplied));
			if (Group.bHasPartnerRole && Group.NearbyCount > 0)
			{
				Entry.EffectText = FText::Format(LOCTEXT("SynergyBeneficiaryTotal", "{0}\n범위 내 수익 시설 {1}개 · 시설별 전체 조합 +{2}%"),
					Entry.EffectText, FText::AsNumber(Group.NearbyCount), Range(Group.MinTotal, Group.MaxTotal));
			}
			Entry.ConditionText = Group.bHasCounterpart
				? FText::Format(LOCTEXT("SynergyDistanceCondition", "중심 간 {0}m 이내 · 가장 가까운 상대 {1}m"), Number(Source.MaxDistanceMeters), Number(Group.NearestMeters))
				: FText::Format(LOCTEXT("SynergyMissingCondition", "중심 간 {0}m 이내 · 필요한 상대 시설 없음"), Number(Source.MaxDistanceMeters));
			Entry.bActive = Group.ActiveCount > 0;
			if (!bSelectionEligible) Entry.StatusText = LOCTEXT("SynergyIneligible", "현재 위치·시설 상태에서는 조합 적용 불가");
			else if (!Group.bHasCounterpart) Entry.StatusText = LOCTEXT("SynergyMissingPartner", "필요 시설을 설치해 주세요");
			else if (Group.NearbyCount == 0) Entry.StatusText = LOCTEXT("SynergyOutsideRange", "거리 초과 · 더 가까이 배치해 주세요");
			else if (!Entry.bActive) Entry.StatusText = LOCTEXT("SynergySuppressed", "중복 그룹 / 전체 상한으로 미적용");
			else if (Group.bHasPartnerRole) Entry.StatusText = FText::Format(
				LOCTEXT("SynergyAppliedToCount", "범위 내 수익 시설 {0}/{1}개에 적용 중"), FText::AsNumber(Group.ActiveCount), FText::AsNumber(Group.NearbyCount));
			else Entry.StatusText = LOCTEXT("SynergyApplied", "적용 중");
			if (Entry.bActive) ++ActiveRules;
		}
		ViewData.SynergySummaryText = bSelectionEligible
			? FText::Format(LOCTEXT("SynergySummary", "조합 {0}개 · 적용 중 {1}개\n이 시설의 이용 수수료 보너스 +{2}%"),
				FText::AsNumber(ViewData.SynergyEntries.Num()), FText::AsNumber(ActiveRules), Number(SelectedBonusPercent))
			: LOCTEXT("SynergySelectionIneligible", "현재 시설은 조합 효과를 받을 수 없습니다.\n유효한 위치에 배치한 뒤 다시 확인해 주세요.");
	}

	void ApplyUpgradePreview(
		const FFF_FacilityUpgradePreview* Preview, FFF_FacilityInfoViewData& ViewData)
	{
		ViewData.FacilityRevenueText = Preview && Preview->bRevenueAvailable
			? FText::Format(LOCTEXT("CurrentRevenueSigned", "1인 예상 수익  {0} Gold\n기본 대비 {1} Gold"),
				FText::AsNumber(Preview->CurrentRevenuePerUseGold),
				FormatSignedChange(Preview->CurrentRevenuePerUseGold - Preview->BaseRevenuePerUseGold))
			: LOCTEXT("RevenueUnavailable", "1인 예상 수익  정보 없음");
		ViewData.CurrentUpgradeLevel = Preview ? FMath::Max(1, Preview->CurrentLevel) : 1;
		ViewData.bHasFacilityLevels = Preview && Preview->bHasLevels;
		ViewData.FacilityLevelText = ViewData.bHasFacilityLevels ? FText::Format(
			LOCTEXT("FacilityLevel", "시설 강화 · Lv.{0}"),
			FText::AsNumber(ViewData.CurrentUpgradeLevel)) : FText::GetEmpty();
		ViewData.bShowUpgradeAction = ViewData.bHasFacilityLevels && ViewData.Context == EFF_FacilityInfoContext::Build;
		ViewData.bShowUpgradeDetails = ViewData.bShowUpgradeAction && Preview &&
			Preview->bSupported && Preview->bHasUpgradeQuote && Preview->TargetLevel > Preview->CurrentLevel;
		ViewData.bCanUpgrade = ViewData.bShowUpgradeDetails && Preview->bCanUpgrade;
		ViewData.UpgradeEffectText = FText::GetEmpty();
		ViewData.UpgradeCurrentText = FText::GetEmpty();
		ViewData.UpgradeNextText = FText::GetEmpty();
		ViewData.UpgradeCurrentValues = FFF_FacilityUpgradeColumnData();
		ViewData.UpgradeNextValues = FFF_FacilityUpgradeColumnData();
		ViewData.UpgradePriceText = FText::GetEmpty();
		ViewData.UpgradeBalanceText = FText::GetEmpty();
		ViewData.UpgradeBalanceLabel = FText::GetEmpty();
		ViewData.UpgradeSizeEffectText = FText::GetEmpty();
		ViewData.UpgradeRevenueEffectText = FText::GetEmpty();
		ViewData.UpgradeCostText = FText::GetEmpty();
		ViewData.UpgradeDisabledReasonText = ViewData.bShowUpgradeAction
			? (Preview ? Preview->FailureReason : LOCTEXT("UpgradeUnavailable", "강화 정보를 준비 중입니다."))
			: FText::GetEmpty();
		ViewData.UpgradeActionText = LOCTEXT("UpgradeFacility", "시설 강화");
		if (!ViewData.bShowUpgradeDetails)
		{
			if (Preview && Preview->bSupported && Preview->CurrentLevel >= Preview->MaxLevel)
			{
				ViewData.UpgradeActionText = LOCTEXT("UpgradeMaximum", "최대 레벨");
			}
			return;
		}

		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 1;
		Options.MaximumFractionalDigits = 1;
		FNumberFormattingOptions SecondsOptions = Options;
		SecondsOptions.MaximumFractionalDigits = FMath::Min(
			Preview->CurrentServiceDurationSeconds, Preview->NextServiceDurationSeconds) < .1f ? 3 : 1;
		auto FormatComparison = [&](const bool bNext)
		{
			const FText Revenue = Preview->bRevenueAvailable
				? FormatGold(true, bNext ? Preview->NextRevenuePerUseGold : Preview->CurrentRevenuePerUseGold)
				: LOCTEXT("ComparisonRevenueUnavailable", "정보 없음");
			return FText::Format(LOCTEXT("UpgradeComparisonValues", "1인 수익  {0}\n팀당 시간  {1}초\n분당  {2}팀\n크기  {3}"),
				Revenue,
				FText::AsNumber(bNext ? Preview->NextServiceDurationSeconds : Preview->CurrentServiceDurationSeconds, &SecondsOptions),
				FText::AsNumber(bNext ? Preview->NextThroughputPerMinute : Preview->CurrentThroughputPerMinute, &Options),
				FormatFootprint(bNext ? Preview->NextFootprintCentimeters : Preview->CurrentFootprintCentimeters));
		};
		ViewData.UpgradeCurrentText = FormatComparison(false);
		ViewData.UpgradeNextText = FormatComparison(true);
		auto MakeColumn = [&](const bool bNext)
		{
			FFF_FacilityUpgradeColumnData Values;
			Values.Revenue = Preview->bRevenueAvailable
				? FormatGold(true, bNext ? Preview->NextRevenuePerUseGold : Preview->CurrentRevenuePerUseGold)
				: LOCTEXT("ComparisonRevenueUnavailable", "정보 없음");
			Values.ServiceTime = FText::Format(LOCTEXT("UpgradeCellSeconds", "{0}초"),
				FText::AsNumber(bNext ? Preview->NextServiceDurationSeconds : Preview->CurrentServiceDurationSeconds, &SecondsOptions));
			Values.Throughput = FText::Format(LOCTEXT("UpgradeCellThroughput", "{0}팀/분"),
				FText::AsNumber(bNext ? Preview->NextThroughputPerMinute : Preview->CurrentThroughputPerMinute, &Options));
			Values.Footprint = FormatFootprint(bNext ? Preview->NextFootprintCentimeters : Preview->CurrentFootprintCentimeters);
			return Values;
		};
		ViewData.UpgradeCurrentValues = MakeColumn(false);
		ViewData.UpgradeNextValues = MakeColumn(true);
		ViewData.UpgradePriceText = FormatGold(true, Preview->CostGold);
		ViewData.UpgradeBalanceLabel = Preview->GoldAfterUpgrade < 0
			? LOCTEXT("UpgradeBalanceShortage", "부족한 금액") : LOCTEXT("UpgradeBalanceAfter", "구매 후 잔액");
		ViewData.UpgradeBalanceText = FText::Format(LOCTEXT("UpgradeBalanceGold", "{0} Gold"),
			FText::AsNumber(FMath::Abs(static_cast<int64>(Preview->GoldAfterUpgrade))));
		ViewData.UpgradeEffectText = FText::Format(
			LOCTEXT("UpgradeEffectsCompact", "팀당 시간 {0}초 → {1}초 · 분당 {2}팀 → {3}팀"),
			FText::AsNumber(Preview->CurrentServiceDurationSeconds, &SecondsOptions),
			FText::AsNumber(Preview->NextServiceDurationSeconds, &SecondsOptions),
			FText::AsNumber(Preview->CurrentThroughputPerMinute, &Options),
			FText::AsNumber(Preview->NextThroughputPerMinute, &Options));
		if (Preview->bRevenueAvailable)
		{
			const int32 AddedGold = Preview->NextRevenuePerUseGold - Preview->CurrentRevenuePerUseGold;
			FNumberFormattingOptions PercentOptions;
			PercentOptions.MaximumFractionalDigits = 1;
			const FText AddedText = Preview->CurrentRevenuePerUseGold > 0
				? FText::Format(LOCTEXT("RevenueChangePercent", "변화 {0} Gold ({1}%)"),
					FormatSignedChange(AddedGold), FormatSignedChange(
						100.0 * AddedGold / Preview->CurrentRevenuePerUseGold, &PercentOptions))
				: FText::Format(LOCTEXT("RevenueChangeFromZero", "변화 {0} Gold"), FormatSignedChange(AddedGold));
			ViewData.UpgradeRevenueEffectText = FText::Format(
				LOCTEXT("NextRevenueWithMultiplier", "1인 수익 {0} → {1} Gold (기본 ×{2})\n{3}"),
				FText::AsNumber(Preview->CurrentRevenuePerUseGold),
				FText::AsNumber(Preview->NextRevenuePerUseGold),
				FText::AsNumber(Preview->NextRevenueMultiplier), AddedText);
		}
		if (Preview->CurrentFootprintCentimeters != Preview->NextFootprintCentimeters)
		{
			ViewData.UpgradeSizeEffectText = FText::Format(
				LOCTEXT("UpgradeSizeEffect", "시설 크기  {0} → {1}"),
				FormatFootprint(Preview->CurrentFootprintCentimeters), FormatFootprint(Preview->NextFootprintCentimeters));
		}
		ViewData.UpgradeCostText = Preview->GoldAfterUpgrade < 0
			? FText::Format(LOCTEXT("UpgradeGoldShortageCompact", "비용 {0} Gold · 부족한 금액  {1} Gold\n강화비는 철거 시 환급되지 않습니다."),
				FText::AsNumber(Preview->CostGold), FText::AsNumber(-static_cast<int64>(Preview->GoldAfterUpgrade)))
			: FText::Format(LOCTEXT("UpgradeCostCompact", "비용 {0} Gold · 구매 후 잔액 {1} Gold\n강화비는 철거 시 환급되지 않습니다."),
				FText::AsNumber(Preview->CostGold), FText::AsNumber(Preview->GoldAfterUpgrade));
		ViewData.UpgradeActionText = Preview->CostGold == 0
			? FText::Format(LOCTEXT("UpgradeTargetLevelFree", "Lv.{0} 강화 · 무료"), FText::AsNumber(Preview->TargetLevel))
			: FText::Format(LOCTEXT("UpgradeTargetLevel", "Lv.{0} 강화 · {1} Gold"),
				FText::AsNumber(Preview->TargetLevel), FText::AsNumber(Preview->CostGold));
	}

	void ApplyFacilityRevenue(
		const FFF_FacilityInstanceRevenue* Revenue,
		const bool bSectionApplicable,
		const bool bGeneratesDirectRevenue,
		const int32 FestivalTodayRevenueGold,
		FFF_FacilityInfoViewData& ViewData)
	{
		ViewData.bShowRevenueSection = bSectionApplicable;
		ViewData.TodayRevenueGold = Revenue ? Revenue->TodayGold : 0;
		ViewData.TotalRevenueGold = Revenue ? Revenue->TotalGold : 0;
		ViewData.TodayPaymentCount = Revenue ? Revenue->TodayPaymentCount : 0;
		ViewData.TotalPaymentCount = Revenue ? Revenue->TotalPaymentCount : 0;
		// Money already earned proves the facility sells even when its per-use price is unknown.
		ViewData.bFacilityGeneratesRevenue = bGeneratesDirectRevenue || ViewData.TotalRevenueGold > 0;
		ViewData.TodayRevenueSharePercent =
			Revenue && FestivalTodayRevenueGold > 0 && ViewData.TodayRevenueGold > 0
				? FMath::Clamp(100.f * static_cast<float>(ViewData.TodayRevenueGold) /
					static_cast<float>(FestivalTodayRevenueGold), 0.f, 100.f)
				: 0.f;
		if (!Revenue)
		{
			const FText Unknown = LOCTEXT("FacilityRevenueUnknown", "정보 없음");
			ViewData.TodayRevenueText = Unknown;
			ViewData.TotalRevenueText = Unknown;
			ViewData.RevenuePaymentCountText = Unknown;
			ViewData.RevenueShareText = Unknown;
			ViewData.RevenueSummaryText = LOCTEXT("FacilityRevenueSummaryUnknown", "시설 수익  정보 없음");
			ViewData.RevenueStatusText = LOCTEXT("FacilityRevenueEconomyNotReady", "축제 경제 정보를 준비 중입니다.");
			return;
		}
		ViewData.TodayRevenueText = FormatGold(true, ViewData.TodayRevenueGold);
		ViewData.TotalRevenueText = FormatGold(true, ViewData.TotalRevenueGold);
		// Stat-card values stay on one line at the authored 15pt; the card labels carry the context.
		ViewData.RevenuePaymentCountText = FText::Format(
			LOCTEXT("FacilityRevenuePaymentCount", "{0}건 · 누적 {1}건"),
			FText::AsNumber(ViewData.TodayPaymentCount), FText::AsNumber(ViewData.TotalPaymentCount));
		FNumberFormattingOptions PercentOptions;
		PercentOptions.MaximumFractionalDigits = 1;
		ViewData.RevenueShareText = FText::Format(
			LOCTEXT("FacilityRevenueShare", "{0}%"),
			FText::AsNumber(ViewData.TodayRevenueSharePercent, &PercentOptions));
		ViewData.RevenueSummaryText = FText::Format(
			LOCTEXT("FacilityRevenueSummary", "오늘 {0} · 누적 {1}"),
			ViewData.TodayRevenueText, ViewData.TotalRevenueText);
		if (!ViewData.bFacilityGeneratesRevenue)
		{
			ViewData.RevenueStatusText = LOCTEXT("FacilityRevenueAttractionOnly",
				"직접 수익이 없는 집객 시설입니다. 주변 부스의 손님을 늘려 줍니다.");
		}
		else if (ViewData.TotalPaymentCount == 0)
		{
			ViewData.RevenueStatusText = LOCTEXT("FacilityRevenueNoPayments",
				"아직 결제가 없습니다. 손님이 이용을 마치면 수수료가 들어옵니다.");
		}
		else if (ViewData.TodayPaymentCount == 0)
		{
			ViewData.RevenueStatusText = LOCTEXT("FacilityRevenueNoPaymentsToday", "오늘은 아직 결제가 없습니다.");
		}
		else
		{
			ViewData.RevenueStatusText = FText::GetEmpty();
		}
	}

	FText GetCategoryText(const EFFZSFacilityCategory Category)
	{
		switch (Category)
		{
		case EFFZSFacilityCategory::Booth:
			return LOCTEXT("CategoryBooth", "부스");
		case EFFZSFacilityCategory::Toilet:
			return LOCTEXT("CategoryToilet", "화장실");
		case EFFZSFacilityCategory::Info:
			return LOCTEXT("CategoryInfo", "안내 시설");
		case EFFZSFacilityCategory::Rest:
			return LOCTEXT("CategoryRest", "휴게 시설");
		case EFFZSFacilityCategory::Performance:
			return LOCTEXT("CategoryPerformance", "공연 시설");
		case EFFZSFacilityCategory::Safety:
			return LOCTEXT("CategorySafety", "안전 시설");
		case EFFZSFacilityCategory::PhotoSpot:
			return LOCTEXT("CategoryPhotoSpot", "포토 스팟");
		case EFFZSFacilityCategory::Lighting:
			return LOCTEXT("CategoryLighting", "조명");
		case EFFZSFacilityCategory::Road:
			return LOCTEXT("CategoryRoad", "길");
		case EFFZSFacilityCategory::Tile:
			return LOCTEXT("CategoryTile", "타일");
		default:
			return LOCTEXT("CategoryUnknown", "기타 시설");
		}
	}

	FText GetFacilityStateText(const EFFZSFacilityState State)
	{
		switch (State)
		{
		case EFFZSFacilityState::Planned:
			return LOCTEXT("StatePlanned", "설치 예정");
		case EFFZSFacilityState::Validated:
			return LOCTEXT("StateValidated", "운영 준비 완료");
		case EFFZSFacilityState::Active:
			return LOCTEXT("StateActive", "운영 중");
		case EFFZSFacilityState::Closed:
			return LOCTEXT("StateClosed", "운영 종료");
		case EFFZSFacilityState::MovePending:
			return LOCTEXT("StateMovePending", "이동 대기");
		case EFFZSFacilityState::Removed:
			return LOCTEXT("StateRemoved", "철거됨");
		default:
			return LOCTEXT("StateUnknown", "상태 확인 중");
		}
	}

	FText FormatFootprint(const FIntPoint& SizeCentimeters)
	{
		return FText::Format(
			LOCTEXT("FootprintFormat", "{0} × {1} m"),
			FText::AsNumber(static_cast<double>(SizeCentimeters.X) / 100.0),
			FText::AsNumber(static_cast<double>(SizeCentimeters.Y) / 100.0));
	}

	FText FormatDistanceMeters(const int32 Centimeters)
	{
		return FText::Format(
			LOCTEXT("DistanceMetersFormat", "{0} m"),
			FText::AsNumber(static_cast<double>(Centimeters) / 100.0));
	}

	FText FormatGold(const bool bAvailable, const int32 Amount)
	{
		return bAvailable
			? FText::Format(LOCTEXT("GoldFormat", "{0} Gold"), FText::AsNumber(Amount))
			: LOCTEXT("UnavailableCost", "가격 정보 없음");
	}

	FText FormatProgramTimeRange(const float StartMinutes, const float EndMinutes)
	{
		if (!FMath::IsFinite(StartMinutes) || !FMath::IsFinite(EndMinutes) ||
			StartMinutes < 0.0f || EndMinutes <= StartMinutes)
		{
			return LOCTEXT("UnavailableProgramTime", "시간 정보 없음");
		}

		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 0;
		Options.MaximumFractionalDigits = 1;
		return FText::Format(
			LOCTEXT("ProgramTimeRangeFormat", "개장 +{0}분 ~ +{1}분"),
			FText::AsNumber(StartMinutes, &Options),
			FText::AsNumber(EndMinutes, &Options));
	}

	FText FormatScheduledProgramSummary(
		const FText& DisplayName,
		const float StartMinutes,
		const float EndMinutes,
		const bool bCostAvailable,
		const int32 CostGold)
	{
		return FText::Format(
			LOCTEXT("ScheduledProgramSummaryFormat", "{0} · {1} · 비용 {2}"),
			DisplayName.IsEmpty() ? LOCTEXT("UnknownProgramName", "이름 없는 프로그램") : DisplayName,
			FormatProgramTimeRange(StartMinutes, EndMinutes),
			FormatGold(bCostAvailable, CostGold));
	}

	FText FormatExpectedWait(const bool bAvailable, const float Seconds)
	{
		if (!bAvailable || !FMath::IsFinite(Seconds) || Seconds < 0.0f || Seconds >= 86400.0f)
		{
			return LOCTEXT("UnavailableWait", "계산 불가");
		}
		if (Seconds < 60.0f)
		{
			return Seconds <= KINDA_SMALL_NUMBER
				? LOCTEXT("NoWait", "바로 이용 가능")
				: LOCTEXT("UnderOneMinute", "1분 미만");
		}
		const int32 Minutes = FMath::Max(1, FMath::CeilToInt(Seconds / 60.0f));
		return FText::Format(LOCTEXT("WaitMinutesFormat", "약 {0}분"), FText::AsNumber(Minutes));
	}

	FText FormatThroughput(const bool bAvailable, const float TeamsPerMinute)
	{
		if (!bAvailable || !FMath::IsFinite(TeamsPerMinute) || TeamsPerMinute < 0.0f)
		{
			return LOCTEXT("UnavailableThroughput", "계산 불가");
		}
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 1;
		Options.MaximumFractionalDigits = 1;
		return FText::Format(
			LOCTEXT("ThroughputFormat", "분당 {0}팀"),
			FText::AsNumber(TeamsPerMinute, &Options));
	}

	FText GetOperationStateText(
		const bool bQueueSupported,
		const bool bQueueAvailable,
		const bool bIsOpen,
		const bool bAcceptingNew)
	{
		if (!bQueueSupported)
		{
			return LOCTEXT("QueueUnsupported", "운영 정보 사용 불가");
		}
		if (!bQueueAvailable)
		{
			return LOCTEXT("QueuePreparing", "운영 정보 준비 중");
		}
		if (!bIsOpen)
		{
			return LOCTEXT("QueueClosed", "운영 종료");
		}
		return bAcceptingNew
			? LOCTEXT("QueueAccepting", "운영 중 · 입장 가능")
			: LOCTEXT("QueueFull", "운영 중 · 현재 만석");
	}
}

#undef LOCTEXT_NAMESPACE
