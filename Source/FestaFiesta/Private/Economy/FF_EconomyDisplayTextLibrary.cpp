// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/FF_EconomyDisplayTextLibrary.h"

#define LOCTEXT_NAMESPACE "FestaFiestaEconomy"

// [주의] 아래 switch 에 default: 를 넣지 않는다.
// enum 에 값을 추가하면 컴파일 경고가 뜨고, 이 프로젝트는 경고를 에러로 취급하므로 빌드가 막힌다.
// 문자열을 빠뜨린 채로 넘어갈 수가 없다. default 를 넣으면 그 방어가 사라진다.

FText UFF_EconomyDisplayTextLibrary::GetSpendReasonText(EFF_SpendReason Reason)
{
	switch (Reason)
	{
	case EFF_SpendReason::FacilityInstall: return LOCTEXT("Spend_FacilityInstall", "시설 설치비");
	case EFF_SpendReason::ProgramPurchase: return LOCTEXT("Spend_ProgramPurchase", "유료 프로그램 구매");
	case EFF_SpendReason::Promotion:       return LOCTEXT("Spend_Promotion",       "홍보비");
	case EFF_SpendReason::VenueRent:       return LOCTEXT("Spend_VenueRent",       "장소 임대료");
	case EFF_SpendReason::StaffWages:      return LOCTEXT("Spend_StaffWages",      "Staff 임금");
	case EFF_SpendReason::StaffHire:       return LOCTEXT("Spend_StaffHire",       "추가 고용");
	case EFF_SpendReason::EventSupplies:   return LOCTEXT("Spend_EventSupplies",   "Event 소모품");
	case EFF_SpendReason::Cleanup:         return LOCTEXT("Spend_Cleanup",         "뒷정리");
	case EFF_SpendReason::FinaleFireworks: return LOCTEXT("Spend_FinaleFireworks", "피날레 폭죽");
	case EFF_SpendReason::FacilityUpgrade: return LOCTEXT("Spend_FacilityUpgrade", "시설 강화비");
	}

	return FText::GetEmpty();
}

FText UFF_EconomyDisplayTextLibrary::GetEarnReasonText(EFF_EarnReason Reason)
{
	switch (Reason)
	{
	case EFF_EarnReason::AdmissionFee:       return LOCTEXT("Earn_AdmissionFee",       "NPC 입장료");
	case EFF_EarnReason::FacilityCommission: return LOCTEXT("Earn_FacilityCommission", "부스 수수료");
	case EFF_EarnReason::ProgramFee:         return LOCTEXT("Earn_ProgramFee",         "유료 프로그램 체험비");
	case EFF_EarnReason::DailyGoalBonus:     return LOCTEXT("Earn_DailyGoalBonus",     "일일 목표 보너스");
	case EFF_EarnReason::MinigameReward:     return LOCTEXT("Earn_MinigameReward",     "미니게임 보상");
	}

	return FText::GetEmpty();
}

FText UFF_EconomyDisplayTextLibrary::GetFinalePurchaseResultText(EFF_FinalePurchaseResult Result)
{
	switch (Result)
	{
	case EFF_FinalePurchaseResult::Success:            return LOCTEXT("Finale_Success",            "구매 완료");
	case EFF_FinalePurchaseResult::FestivalNotStarted: return LOCTEXT("Finale_FestivalNotStarted", "축제가 시작되지 않았다");
	case EFF_FinalePurchaseResult::InvalidGrade:       return LOCTEXT("Finale_InvalidGrade",       "구매할 수 없는 등급");
	case EFF_FinalePurchaseResult::UnknownGrade:       return LOCTEXT("Finale_UnknownGrade",       "저작되지 않은 등급");
	case EFF_FinalePurchaseResult::AlreadyPurchased:   return LOCTEXT("Finale_AlreadyPurchased",   "이미 구매했다");
	case EFF_FinalePurchaseResult::InsufficientGold:   return LOCTEXT("Finale_InsufficientGold",   "Gold 부족");
	}

	return FText::GetEmpty();
}

FText UFF_EconomyDisplayTextLibrary::GetFinaleGradeText(EFF_FinaleGrade Grade)
{
	switch (Grade)
	{
	case EFF_FinaleGrade::None: return LOCTEXT("Grade_None", "미구매");
	case EFF_FinaleGrade::C:    return LOCTEXT("Grade_C",    "C");
	case EFF_FinaleGrade::B:    return LOCTEXT("Grade_B",    "B");
	case EFF_FinaleGrade::A:    return LOCTEXT("Grade_A",    "A");
	case EFF_FinaleGrade::S:    return LOCTEXT("Grade_S",    "S");
	}

	return FText::GetEmpty();
}

FText UFF_EconomyDisplayTextLibrary::GetProgramPurchaseResultText(EFF_ProgramPurchaseResult Result)
{
	switch (Result)
	{
	case EFF_ProgramPurchaseResult::Success:            return LOCTEXT("Program_Success",            "편성 완료");
	case EFF_ProgramPurchaseResult::FestivalNotStarted: return LOCTEXT("Program_FestivalNotStarted", "축제가 시작되지 않았다");
	case EFF_ProgramPurchaseResult::UnknownProgram:     return LOCTEXT("Program_UnknownProgram",     "없는 프로그램");
	case EFF_ProgramPurchaseResult::AlreadyScheduled:   return LOCTEXT("Program_AlreadyScheduled",   "오늘 이미 편성했다");
	case EFF_ProgramPurchaseResult::NotEnoughStaff:     return LOCTEXT("Program_NotEnoughStaff",     "인원이 부족하다");
	case EFF_ProgramPurchaseResult::InsufficientGold:   return LOCTEXT("Program_InsufficientGold",   "Gold 부족");
	case EFF_ProgramPurchaseResult::FacilityTypeMismatch:
		return LOCTEXT("Program_FacilityTypeMismatch", "이 시설에서는 열 수 없다");
	}

	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
