// FF_ProgramAuthoringValidationLibrary.cpp
// 역할: 프로그램 편성 데이터를 검사하고 수정할 항목을 작성자에게 설명한다.
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/FF_ProgramAuthoringValidationLibrary.h"

#define LOCTEXT_NAMESPACE "FFProgramAuthoringValidation"

namespace
{
	bool IsSimpleToken(const FString& Token)
	{
		if (Token.IsEmpty())
		{
			return false;
		}
		for (const TCHAR Character : Token)
		{
			if (!FChar::IsAlnum(Character))
			{
				return false;
			}
		}
		return true;
	}

	FText CardLabel(const FFF_ProgramCard& Card, const int32 Index)
	{
		return Card.ProgramId.IsNone()
			? FText::Format(LOCTEXT("CardIndex", "ProgramCatalog[{0}]"), FText::AsNumber(Index))
			: FText::FromName(Card.ProgramId);
	}
}

bool UFF_ProgramAuthoringValidationLibrary::IsAllowedFacilityTypeTokenValid(
	const FName FacilityType)
{
	if (FacilityType.IsNone())
	{
		return false;
	}

	TArray<FString> Tokens;
	FacilityType.ToString().ParseIntoArray(Tokens, TEXT("_"), false);
	return Tokens.Num() == 2 && IsSimpleToken(Tokens[0]) && IsSimpleToken(Tokens[1]);
}

bool UFF_ProgramAuthoringValidationLibrary::ValidateProgramCatalog(
	const TArray<FFF_ProgramCard>& ProgramCatalog,
	TArray<FText>& OutErrors)
{
	const int32 InitialErrorCount = OutErrors.Num();
	if (ProgramCatalog.IsEmpty())
	{
		OutErrors.Add(LOCTEXT("EmptyCatalog", "ProgramCatalog가 비어 있다. 프로그램을 한 개 이상 저작해야 한다."));
		return false;
	}

	TSet<FName> SeenProgramIds;
	for (int32 Index = 0; Index < ProgramCatalog.Num(); ++Index)
	{
		const FFF_ProgramCard& Card = ProgramCatalog[Index];
		const FText Label = CardLabel(Card, Index);

		if (Card.ProgramId.IsNone())
		{
			OutErrors.Add(FText::Format(
				LOCTEXT("MissingId", "{0}: ProgramId가 비어 있다."), Label));
		}
		else if (SeenProgramIds.Contains(Card.ProgramId))
		{
			OutErrors.Add(FText::Format(
				LOCTEXT("DuplicateId", "{0}: ProgramId가 카탈로그 안에서 중복된다."), Label));
		}
		else
		{
			SeenProgramIds.Add(Card.ProgramId);
		}

		if (Card.DisplayName.ToString().TrimStartAndEnd().IsEmpty())
		{
			OutErrors.Add(FText::Format(
				LOCTEXT("EmptyDisplayName", "{0}: 화면 표시 이름이 비어 있다."), Label));
		}

		// 무료 입장도 유효한 프로그램이다. 개최 비용과 정원은 양수여야 한다.
		// FFF_ProgramCard::IsValid와 같은 경계를 사용해 작성 검사와 실행 규칙을 맞춘다.
		if (Card.CostGold <= 0 || Card.Capacity <= 0 || Card.FeePerPerson < 0)
		{
			OutErrors.Add(FText::Format(
				LOCTEXT("InvalidEconomyBounds", "{0}: CostGold와 Capacity는 1 이상, FeePerPerson은 0 이상이어야 한다."),
				Label));
		}

		if (!FMath::IsFinite(Card.StartMinutes) || !FMath::IsFinite(Card.EndMinutes)
			|| Card.StartMinutes < 0.f || Card.EndMinutes <= Card.StartMinutes)
		{
			OutErrors.Add(FText::Format(
				LOCTEXT("InvalidTime", "{0}: 시간 범위는 유한한 값이고 0 <= StartMinutes < EndMinutes여야 한다."),
				Label));
		}

		TSet<FName> SeenFacilityTypes;
		for (const FName FacilityType : Card.AllowedFacilityTypes)
		{
			if (!IsAllowedFacilityTypeTokenValid(FacilityType))
			{
				OutErrors.Add(FText::Format(
					LOCTEXT("BadFacilityType", "{0}: 허용 시설 타입 '{1}'은 정확한 대분류_소분류 형식이어야 한다."),
					Label, FText::FromName(FacilityType)));
			}
			if (SeenFacilityTypes.Contains(FacilityType))
			{
				OutErrors.Add(FText::Format(
					LOCTEXT("DuplicateFacilityType", "{0}: 허용 시설 타입 '{1}'이 중복된다."),
					Label, FText::FromName(FacilityType)));
			}
			else
			{
				SeenFacilityTypes.Add(FacilityType);
			}
		}
	}

	return OutErrors.Num() == InitialErrorCount;
}

#undef LOCTEXT_NAMESPACE
