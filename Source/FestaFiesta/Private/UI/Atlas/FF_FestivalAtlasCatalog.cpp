// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Atlas/FF_FestivalAtlasCatalog.h"

#include "UI/FF_UILog.h"

const FFF_FestivalAtlasEntry* UFF_FestivalAtlasCatalog::FindEntry(FName InFestivalId) const
{
	if (InFestivalId.IsNone())
	{
		return nullptr;
	}

	return Entries.FindByPredicate(
		[InFestivalId](const FFF_FestivalAtlasEntry& Entry) { return Entry.FestivalId == InFestivalId; });
}

const FFF_FestivalAtlasEntry* UFF_FestivalAtlasCatalog::FindFirstUnlocked() const
{
	return Entries.FindByPredicate(
		[](const FFF_FestivalAtlasEntry& Entry) { return Entry.bUnlocked; });
}

const FFF_AtlasCountryEntry* UFF_FestivalAtlasCatalog::FindCountry(FName InCountryId) const
{
	// 빈 이름은 국가가 아니다. 이 가드가 없으면 `CountryId` 를 채우지 않은 국가 항목이
	// "아직 국가를 지정하지 않은 축제" 와 서로 맞아 버린다.
	if (InCountryId.IsNone())
	{
		return nullptr;
	}

	return Countries.FindByPredicate(
		[InCountryId](const FFF_AtlasCountryEntry& Country) { return Country.CountryId == InCountryId; });
}

void UFF_FestivalAtlasCatalog::GetFestivalsInCountry(
	FName InCountryId,
	TArray<const FFF_FestivalAtlasEntry*>& OutFestivals) const
{
	// 호출부가 배열을 재사용할 수 있게 비우고 시작한다. 누적되면 목록이 화면마다 늘어난다.
	OutFestivals.Reset();

	if (InCountryId.IsNone())
	{
		return;
	}

	for (const FFF_FestivalAtlasEntry& Entry : Entries)
	{
		if (Entry.CountryId == InCountryId)
		{
			OutFestivals.Add(&Entry);
		}
	}
}

int32 UFF_FestivalAtlasCatalog::CountFestivalsInCountry(FName InCountryId) const
{
	if (InCountryId.IsNone())
	{
		return 0;
	}

	int32 Count = 0;
	for (const FFF_FestivalAtlasEntry& Entry : Entries)
	{
		if (Entry.CountryId == InCountryId)
		{
			++Count;
		}
	}
	return Count;
}

#if WITH_EDITOR
void UFF_FestivalAtlasCatalog::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ValidateCatalog();
}

void UFF_FestivalAtlasCatalog::ValidateCatalog() const
{
	// --- 국가 ---

	TSet<FName> SeenCountryIds;
	for (int32 Index = 0; Index < Countries.Num(); ++Index)
	{
		const FFF_AtlasCountryEntry& Country = Countries[Index];

		if (Country.CountryId.IsNone())
		{
			// 조인 키가 없으면 축제와 이어지지 않아 지구본에 조형물만 서고 아무것도 안 열린다.
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("%s: 국가 %d번 항목에 CountryId 가 없다. 축제와 이어 붙일 수 없다."),
				*GetName(), Index);
		}
		else if (SeenCountryIds.Contains(Country.CountryId))
		{
			// FindCountry 가 먼저 걸린 것만 돌려주므로 뒤쪽 항목은 영영 안 쓰인다.
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("%s: CountryId `%s` 가 중복이다. 뒤에 있는 국가는 선택되지 않는다."),
				*GetName(), *Country.CountryId.ToString());
		}
		SeenCountryIds.Add(Country.CountryId);

		if (Country.DisplayName.IsEmpty())
		{
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("%s: 국가 `%s` 에 DisplayName 이 없다. 화면에 빈 칸이 나간다."),
				*GetName(), *Country.CountryId.ToString());
		}

		// (0, 0) 은 기니 만 앞바다다. 비워 둔 것과 구분이 안 되므로 소리를 낸다.
		if (FMath::IsNearlyZero(Country.Location.Latitude) && FMath::IsNearlyZero(Country.Location.Longitude))
		{
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("%s: 국가 `%s` 의 Location 이 (0, 0) 이다. 핀이 기니 만 앞바다에 찍힌다 — "
					 "위경도를 채운다."),
				*GetName(), *Country.CountryId.ToString());
		}

		if (!Country.bUnlocked && Country.LockReason.IsEmpty())
		{
			// 색이나 흐린 글자만으로 잠금을 알리면 AT-011 위반이다. 문구가 반드시 있어야 한다.
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("%s: 국가 `%s` 가 잠겨 있는데 LockReason 이 비었다. `준비 중` 같은 문구를 넣는다 (AT-011)."),
				*GetName(), *Country.CountryId.ToString());
		}

		if (!Country.CountryId.IsNone() && CountFestivalsInCountry(Country.CountryId) == 0)
		{
			// 조형물은 서는데 눌러도 갈 곳이 없는 국가다. 배지에 `0` 을 찍지 않고 국가를 지운다.
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("%s: 국가 `%s` 에 속한 축제가 없다. 지구본에 눌러도 갈 곳이 없는 핀이 생긴다."),
				*GetName(), *Country.CountryId.ToString());
		}
	}

	// --- 축제 ---

	TSet<FName> SeenFestivalIds;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FFF_FestivalAtlasEntry& Entry = Entries[Index];

		if (Entry.FestivalId.IsNone())
		{
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("%s: %d번 항목에 FestivalId 가 없다. 김시리님 정의와 이어 붙일 수 없다."),
				*GetName(), Index);
		}
		else if (SeenFestivalIds.Contains(Entry.FestivalId))
		{
			// FindEntry 가 먼저 걸린 것만 돌려주므로 뒤쪽 항목은 영영 안 열린다.
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("%s: FestivalId `%s` 가 중복이다. 뒤에 있는 항목은 선택되지 않는다."),
				*GetName(), *Entry.FestivalId.ToString());
		}
		SeenFestivalIds.Add(Entry.FestivalId);

		// 이 카탈로그의 존재 이유가 레벨 참조다. 비면 축제를 시작할 수 없다.
		//
		// ★ **잠긴 축제는 경고하지 않는다.** 잠금 상태에서 레벨이 없는 것은 결함이 아니라
		//   `아직 만들지 않았다`는 정상적인 표현이다 (지도를 채우는 잠금 핀이 그렇다).
		//   여기서 경고하면 의도한 상태가 매번 경고로 올라와, 진짜 결함인
		//   `플레이 가능한데 레벨이 없는` 항목이 그 안에 묻힌다.
		if (Entry.bUnlocked && Entry.Level.IsNull())
		{
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("%s: `%s` 는 플레이 가능인데 Festival Level 이 비어 있다. 이 축제는 시작할 수 없다."),
				*GetName(), *Entry.FestivalId.ToString());
		}

		if (Entry.CountryId.IsNone())
		{
			// 로드는 되고 콘솔로 시작도 된다. 다만 지구본·국가 지도에는 나타나지 않는다.
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("%s: `%s` 에 CountryId 가 없다. 콘솔로는 시작되지만 지구본·국가 지도에는 "
					 "나타나지 않는다."),
				*GetName(), *Entry.FestivalId.ToString());
		}
		else if (!FindCountry(Entry.CountryId))
		{
			// 오타가 가장 흔하다. 국가 목록에 없는 이름은 어디에도 안 붙는다.
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("%s: `%s` 의 CountryId `%s` 가 국가 목록에 없다. 오타를 확인한다."),
				*GetName(), *Entry.FestivalId.ToString(), *Entry.CountryId.ToString());
		}

		// 0~1 밖이면 지도 바깥에 핀이 찍힌다. ClampMin/Max 는 직접 입력만 막고
		// 복사·붙여넣기나 이전 데이터는 통과하므로 여기서 한 번 더 본다.
		if (Entry.MapPosition.X < 0.0 || Entry.MapPosition.X > 1.0
			|| Entry.MapPosition.Y < 0.0 || Entry.MapPosition.Y > 1.0)
		{
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("%s: `%s` 의 MapPosition (%.3f, %.3f) 이 0~1 밖이다. 국가 지도 바깥에 핀이 찍힌다."),
				*GetName(), *Entry.FestivalId.ToString(), Entry.MapPosition.X, Entry.MapPosition.Y);
		}

		if (!Entry.bUnlocked && Entry.LockReason.IsEmpty())
		{
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("%s: `%s` 가 잠겨 있는데 LockReason 이 비었다. `준비 중` 같은 문구를 넣는다 (AT-011)."),
				*GetName(), *Entry.FestivalId.ToString());
		}

		// 잠긴 국가 안의 열린 축제는 지구본에서 도달할 수 없다. 데이터가 서로 모순인 상태다.
		if (Entry.bUnlocked)
		{
			if (const FFF_AtlasCountryEntry* Country = FindCountry(Entry.CountryId))
			{
				if (!Country->bUnlocked)
				{
					UE_LOG(LogFestaFiestaUI, Warning,
						TEXT("%s: `%s` 는 열려 있는데 국가 `%s` 가 잠겨 있다. 지구본에서 도달할 수 없다."),
						*GetName(), *Entry.FestivalId.ToString(), *Country->CountryId.ToString());
				}
			}
		}
	}
}
#endif
