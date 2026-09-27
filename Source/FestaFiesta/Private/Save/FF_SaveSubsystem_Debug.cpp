// Copyright Epic Games, Inc. All Rights Reserved.

/* 저장 서브시스템의 개발용 콘솔 명령(FF.Save.*).

   저장·불러오기 본체와 섞여 있으면 "게임이 실제로 타는 길" 과 "테스트용 길" 이
   구분되지 않아서 따로 뒀다. 배포 빌드에서는 등록 자체가 안 된다(FFF_DebugConsoleCommands).
*/

#include "Save/FF_SaveSubsystem.h"

void UFF_SaveSubsystem::RegisterDebugConsoleCommands()
{
	// 개발용이다. 배포 빌드에서 콘솔로 저장을 덮거나 불러오면 판이 꼬인다.
#if !UE_BUILD_SHIPPING
	UnregisterDebugConsoleCommands();

	DebugCommands.Add(
		TEXT("FF.Save.Write"),
		TEXT("지금 상태를 저장한다. 사용법: FF.Save.Write [슬롯]  생략 시 자동 저장 차례대로"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			// 생략하면 실제 자동 저장과 같은 길로 간다. 따로 `AutoSave` 칸을 만들면
			// 게임은 안 읽는 파일이 목록에만 하나 더 생긴다.
			if (Args.Num() == 0)
			{
				WriteAutoSave();
				return;
			}
			SaveToSlot(FName(*Args[0]), false);
		}));

	DebugCommands.Add(
		TEXT("FF.Save.Read"),
		TEXT("저장을 읽는다. 사용법: FF.Save.Read [슬롯]  생략 시 가장 최근 자동 저장"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			FName SlotId = Args.Num() > 0 ? FName(*Args[0]) : FName();
			if (SlotId.IsNone())
			{
				UFF_SaveGame* Newest = nullptr;
				if (!FindNewestAutoSave(Newest, SlotId))
				{
					UE_LOG(LogFestaFiestaSave, Display, TEXT("[저장] 읽을 자동 저장이 없다."));
					return;
				}
			}
			LoadFromSlot(SlotId);
		}));

	/* 이 판의 메타를 비운다. 인자 없으면 튜토리얼 기록만, all 이면 깬 축제·기록까지.

	   건너뛰기는 같은 판 안에서는 되돌릴 수 없다. 테스트하다 누르면 새 게임을 해야만
	   튜토리얼을 다시 볼 수 있어서, 개발용 출구를 둔다. 판(돈·시설)은 건드리지 않는다.
	*/
	DebugCommands.Add(
		TEXT("FF.Save.ResetMeta"),
		TEXT("이 판의 튜토리얼 기록(로비 인트로·안내 끔·본 축제)을 지운다. 사용법: FF.Save.ResetMeta [all]  all 이면 기록·깬 축제까지"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			const bool bAll = Args.Num() > 0 && Args[0].Equals(TEXT("all"), ESearchCase::IgnoreCase);

			if (bAll)
			{
				Meta = FFF_MetaSave();
			}
			else
			{
				Meta.bLobbyIntroSeen = false;
				Meta.bTutorialOptedOut = false;
				Meta.TutorialSeenFestivals.Reset();
			}

			// 이 판의 세이브에도 반영한다. 안 하면 이어할 때 옛 기록이 돌아온다.
			WriteRunMeta();

			UE_LOG(LogFestaFiestaSave, Display,
				TEXT("[저장] 메타를 비웠다(%s). 로비 튜토리얼은 바로, 축제 튜토리얼은 레벨을 다시 열면 나온다."),
				bAll ? TEXT("전부") : TEXT("튜토리얼만"));
		}));

	DebugCommands.Add(
		TEXT("FF.Save.Stat"),
		TEXT("저장 상태를 출력한다."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			UE_LOG(LogFestaFiestaSave, Display,
				TEXT("[저장] 로비 인트로=%s 안내 끔=%s / 깬 축제 %d개"),
				Meta.bLobbyIntroSeen ? TEXT("예") : TEXT("아니오"),
				Meta.bTutorialOptedOut ? TEXT("예") : TEXT("아니오"),
				Meta.ClearedFestivals.Num());

			// 어느 축제의 튜토리얼이 막혀 있는지 이름으로 봐야 원인을 가릴 수 있다.
			UE_LOG(LogFestaFiestaSave, Display,
				TEXT("  튜토리얼 본 축제 %d개: %s"),
				Meta.TutorialSeenFestivals.Num(),
				Meta.TutorialSeenFestivals.IsEmpty()
					? TEXT("(없음)")
					: *FString::JoinBy(Meta.TutorialSeenFestivals, TEXT(", "),
						[](const FName& Id) { return Id.ToString(); }));

			FFF_SessionSave Session;
			CaptureSession(Session);
			UE_LOG(LogFestaFiestaSave, Display,
				TEXT("  지금 진행: %d일차 %s / 금화 %d / 만족 %d"),
				Session.Day,
				Session.StageId.IsNone() ? TEXT("-") : *Session.StageId.ToString(),
				Session.Gold, Session.Satisfaction);

			TArray<FString> PartNames;
			for (const TWeakInterfacePtr<IFF_Saveable>& Weak : Saveables)
			{
				if (const IFF_Saveable* Part = Weak.Get())
				{
					PartNames.Add(Part->GetSaveKey().ToString());
				}
			}
			UE_LOG(LogFestaFiestaSave, Display,
				TEXT("  참여 파트 %d개: %s"),
				PartNames.Num(),
				PartNames.Num() == 0 ? TEXT("없음") : *FString::Join(PartNames, TEXT(", ")));

			const TArray<FName> Slots = GetKnownSlotIds();
			UE_LOG(LogFestaFiestaSave, Display,
				TEXT("  슬롯 %d개: %s"),
				Slots.Num(),
				Slots.Num() == 0 ? TEXT("없음")
					: *FString::JoinBy(Slots, TEXT(", "),
						[](const FName& Id) { return Id.ToString(); }));

			for (const TPair<FName, FFF_FestivalRecord>& Pair : Meta.Records)
			{
				UE_LOG(LogFestaFiestaSave, Display,
					TEXT("  %s — 방문 %d / 만족 %.1f / 수익 %d / %d회"),
					*Pair.Key.ToString(), Pair.Value.BestVisitors,
					Pair.Value.BestSatisfaction, Pair.Value.BestProfit, Pair.Value.PlayCount);
			}
		}));
#endif
}

void UFF_SaveSubsystem::UnregisterDebugConsoleCommands()
{
#if !UE_BUILD_SHIPPING
	// 내가 건 것만 지운다. 규칙은 FFF_DebugConsoleCommands 주석 참고.
	DebugCommands.UnregisterAll();
#endif
}
