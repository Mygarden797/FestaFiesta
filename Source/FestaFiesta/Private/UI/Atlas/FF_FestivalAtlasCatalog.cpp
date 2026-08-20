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

#if WITH_EDITOR
void UFF_FestivalAtlasCatalog::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 에디터에서 잡아 주지 않으면 런타임에 "축제가 안 열린다" 로만 드러나는 실수 세 가지다.
	// 조용히 넘어가는 대신 여기서 소리를 낸다.
	TSet<FName> SeenIds;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FFF_FestivalAtlasEntry& Entry = Entries[Index];

		if (Entry.FestivalId.IsNone())
		{
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("%s: %d번 항목에 FestivalId 가 없다. 김시리님 정의와 이어 붙일 수 없다."),
				*GetName(), Index);
		}
		else if (SeenIds.Contains(Entry.FestivalId))
		{
			// FindEntry 가 먼저 걸린 것만 돌려주므로 뒤쪽 항목은 영영 안 열린다.
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("%s: FestivalId `%s` 가 중복이다. 뒤에 있는 항목은 선택되지 않는다."),
				*GetName(), *Entry.FestivalId.ToString());
		}
		SeenIds.Add(Entry.FestivalId);

		if (Entry.Level.IsNull())
		{
			// 이 카탈로그의 존재 이유가 레벨 참조다. 비면 축제를 시작할 수 없다.
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("%s: `%s` 에 Festival Level 이 비어 있다. 이 축제는 시작할 수 없다."),
				*GetName(), *Entry.FestivalId.ToString());
		}

		if (!Entry.bUnlocked && Entry.LockReason.IsEmpty())
		{
			// 색이나 흐린 글자만으로 잠금을 알리면 AT-011 위반이다. 문구가 반드시 있어야 한다.
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("%s: `%s` 가 잠겨 있는데 LockReason 이 비었다. `준비 중` 같은 문구를 넣는다 (AT-011)."),
				*GetName(), *Entry.FestivalId.ToString());
		}
	}
}
#endif
