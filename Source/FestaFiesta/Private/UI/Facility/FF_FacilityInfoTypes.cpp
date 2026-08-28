// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Facility/FF_FacilityInfoTypes.h"

#define LOCTEXT_NAMESPACE "FFFacilityInfoModel"

namespace FFFacilityInfoModel
{
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
