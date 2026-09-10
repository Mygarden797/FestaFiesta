// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Facility/FF_FacilityInfoTypes.h"
#include "Building/FF_FacilityUpgradeTypes.h"

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
			return FText::Format(LOCTEXT("UpgradeComparisonValues", "1인 수익  {0}\n1인 시간  {1}초\n분당  {2}명\n크기  {3}"),
				Revenue,
				FText::AsNumber(bNext ? Preview->NextServiceDurationSeconds : Preview->CurrentServiceDurationSeconds, &SecondsOptions),
				FText::AsNumber(bNext ? Preview->NextThroughputPerMinute : Preview->CurrentThroughputPerMinute, &Options),
				FormatFootprint(bNext ? Preview->NextFootprintCentimeters : Preview->CurrentFootprintCentimeters));
		};
		ViewData.UpgradeCurrentText = FormatComparison(false);
		ViewData.UpgradeNextText = FormatComparison(true);
		ViewData.UpgradeEffectText = FText::Format(
			LOCTEXT("UpgradeEffectsCompact", "시간 {0}초 → {1}초 · 분당 {2}명 → {3}명"),
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

	FText FormatThroughput(const bool bAvailable, const float PeoplePerMinute)
	{
		if (!bAvailable || !FMath::IsFinite(PeoplePerMinute) || PeoplePerMinute < 0.0f)
		{
			return LOCTEXT("UnavailableThroughput", "계산 불가");
		}
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 1;
		Options.MaximumFractionalDigits = 1;
		return FText::Format(
			LOCTEXT("ThroughputFormat", "분당 {0}명"),
			FText::AsNumber(PeoplePerMinute, &Options));
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
