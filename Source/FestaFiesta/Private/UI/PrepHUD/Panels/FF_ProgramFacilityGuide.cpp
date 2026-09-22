// FF_ProgramFacilityGuide.cpp
// 역할: 프로그램에 필요한 시설을 찾아 건설 목록이나 설치된 시설로 안내한다.

#include "UI/PrepHUD/Panels/FF_ProgramFacilityGuide.h"

#include "Building/FF_FacilityNamingLibrary.h"
#include "Building/UI/FF_BuildCatalogScreen.h"
#include "Economy/Rules/FF_ProgramScheduleRules.h"

#define LOCTEXT_NAMESPACE "FestaFiestaProgramFacilityGuide"

namespace
{
	FText GetFacilityDisplayName(const FFF_BuildCatalogEntry& Entry)
	{
		// 표시 이름이 없는 정의에서 ID를 이름 대신 노출하지 않는다.
		return Entry.DisplayName.IsEmpty()
			|| Entry.DisplayName.ToString() == Entry.FacilityDefinitionId.ToString()
			? LOCTEXT("UnnamedFacility", "프로그램 가능 시설")
			: Entry.DisplayName;
	}
}

TArray<FFF_BuildCatalogEntry> FFF_ProgramFacilityGuide::FindCompatibleEntries(
	const FFF_ProgramCard& Card, const TArray<FFF_BuildCatalogEntry>& Entries)
{
	TArray<FFF_BuildCatalogEntry> Compatible;
	for (const FFF_BuildCatalogEntry& Entry : Entries)
	{
		FFF_FacilityNameParts Parts;
		FString FailureReason;
		if (UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
			Entry.Category, Entry.FacilityDefinitionId, Parts, FailureReason)
			&& FFF_ProgramScheduleRules::IsFacilityTypeAllowed(Card, Parts.MajorToken, Parts.MinorToken))
		{
			Compatible.Add(Entry);
		}
	}
	return Compatible;
}

FText FFF_ProgramFacilityGuide::GetCatalogPath(const FFF_BuildCatalogEntry& Entry)
{
	const FText CategoryName = UFF_BuildCatalogScreen::GetCategoryDisplayName(Entry.Category);
	FFF_FacilityNameParts Parts;
	FString FailureReason;
	if (UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
		Entry.Category, Entry.FacilityDefinitionId, Parts, FailureReason))
	{
		const FText MinorName = UFF_BuildCatalogScreen::GetMinorDisplayName(Parts.MinorToken);
		// 새로운 소분류의 번역이 아직 없으면 내부 영문 토큰 대신 대분류까지만 안내한다.
		if (!MinorName.IsEmpty() && MinorName.ToString() != Parts.MinorToken.ToString())
		{
			return FText::Format(LOCTEXT("CatalogPath", "건축목록 > {0} > {1}"), CategoryName, MinorName);
		}
	}
	return FText::Format(LOCTEXT("CatalogCategoryPath", "건축목록 > {0}"), CategoryName);
}

FText FFF_ProgramFacilityGuide::BuildFacilityNotice(
	const FFF_ProgramCard& Card,
	const TArray<FFF_BuildCatalogEntry>& Entries,
	const int32 EligibleFacilityCount)
{
	const TArray<FFF_BuildCatalogEntry> Compatible = FindCompatibleEntries(Card, Entries);
	TArray<FText> Lines;
	if (EligibleFacilityCount > 0)
	{
		Lines.Add(FText::Format(
			LOCTEXT("InstalledFacilities", "설치된 시설 {0}곳에서 프로그램을 편성할 수 있습니다."),
			FText::AsNumber(EligibleFacilityCount)));
	}
	else if (!Compatible.IsEmpty())
	{
		Lines.Add(EligibleFacilityCount == 0
			? LOCTEXT("InstallFacility", "아래 시설 중 한 곳을 설치한 뒤 프로그램을 편성하세요.")
			: LOCTEXT("CompatibleFacilities", "이 프로그램을 열 수 있는 시설입니다."));
	}

	if (Compatible.IsEmpty())
	{
		Lines.Add(Card.AllowedFacilityTypes.IsEmpty()
			? LOCTEXT("UnrestrictedCatalogUnavailable", "시설 종류 제한은 없지만 건축목록에서 설치할 시설을 찾지 못했습니다.")
			: LOCTEXT("CompatibleCatalogUnavailable", "프로그램에 맞는 시설을 건축목록에서 찾지 못했습니다."));
	}
	else
	{
		constexpr int32 MaxShownFacilities = 3;
		const int32 ShownCount = FMath::Min(Compatible.Num(), MaxShownFacilities);
		for (int32 Index = 0; Index < ShownCount; ++Index)
		{
			const FFF_BuildCatalogEntry& Entry = Compatible[Index];
			Lines.Add(FText::Format(LOCTEXT("FacilityLocation", "{0} · {1}"),
				GetFacilityDisplayName(Entry), GetCatalogPath(Entry)));
		}
		if (Compatible.Num() > ShownCount)
		{
			Lines.Add(FText::Format(LOCTEXT("MoreFacilities", "외 {0}종 · 아래 시설 목록에서 확인하세요."),
				FText::AsNumber(Compatible.Num() - ShownCount)));
		}
	}
	return FText::Join(FText::FromString(TEXT("\n")), Lines);
}

#undef LOCTEXT_NAMESPACE
