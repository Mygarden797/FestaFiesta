#include "Economy/Rules/FF_StaffEventRules.h"

float FFF_StaffEventRules::CalcAbilitySum(const TArray<FFF_StaffMember>& Assigned, int32 MinStaff)
{
	TArray<FFF_StaffMember> Sorted = Assigned;
	Sorted.Sort([](const FFF_StaffMember& A, const FFF_StaffMember& B)
	{
		return A.TrainingLevel > B.TrainingLevel;
	});

	float Total = 0.f;
	for (int32 Index = 0; Index < Sorted.Num(); ++Index)
	{
		const float LevelValue = static_cast<float>(Sorted[Index].TrainingLevel);
		Total += (Index < MinStaff) ? LevelValue : LevelValue * 0.5f;
	}
	return Total;
}

float FFF_StaffEventRules::CalcResolveSeconds(const FFF_FestivalEventCard& Card, float AbilitySum)
{
	if (AbilitySum <= 0.f)
	{
		return TNumericLimits<float>::Max();
	}
	// WorkAmount 는 "사람-분" 단위다(문서 예시: 작업량 12 ÷ 능력 3.0 = 4분).
	// TimeLimitSeconds 와 맞추려고 여기서만 초로 바꾼다 — WorkAmount 자체를 초 단위로
	// 저작하면 사람이 작업량을 가늠하기 어려워진다(12초짜리 화재는 감이 안 온다).
	return (Card.WorkAmount / AbilitySum) * 60.f;
}

bool FFF_StaffEventRules::CanPromote(const FFF_StaffMember& Member, int32 CurrentDay)
{
	if (Member.TrainingLevel >= 3) return false;
	if (CurrentDay <= Member.HiredOnDay) return false;
	if (Member.LastPromotedDay == CurrentDay) return false;
	return true;
}

int32 FFF_StaffEventRules::CalcOverduePenalty(const FFF_FestivalEventCard& Card, float Progress01)
{
	const float Clamped = FMath::Clamp(Progress01, 0.f, 1.f);
	return FMath::RoundToInt(static_cast<float>(Card.MaxOverduePenalty) * (1.f - Clamped));
}
