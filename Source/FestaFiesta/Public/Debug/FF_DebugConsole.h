// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

/* 개발용 콘솔 명령 묶음. 한 객체가 등록한 명령을 모아 들고 있다가 한 번에 지운다.

   ─── 왜 따로 뺐나 ──────────────────────────────────────────────────────────
   카메라·피날레·사운드·튜토리얼·저장이 모두 같은 일을 각자 했다.
   명령마다 IConsoleCommand* 멤버를 하나씩 두고, 등록할 때 이름을 확인하고,
   해제할 때 포인터를 지웠다. 다섯 벌이 조금씩 달라서 아래 규칙을 다 지킨 건
   피날레 한 곳뿐이었다.

   ─── 지키는 규칙 두 가지 ───────────────────────────────────────────────────
   콘솔 이름은 프로세스 전역인데 명령을 거는 쪽은 월드나 액터마다 생긴다.
   PIE 를 여러 번 돌리거나 자동화 테스트가 월드를 수십 개 만들면 같은 이름을
   여러 객체가 노린다.

   등록: 그 이름이 비어 있을 때만 건다. 이미 있으면 들지 않는다.
   해제: 내가 건 그 객체가 아직 그 이름의 주인일 때만 지운다. 다른 월드가
         이미 지우고 새로 건 것을 내가 지우면, 해제된 객체를 다시 지우다 터진다.

   배포 빌드에서는 아무것도 걸지 않는다. 콘솔로 저장을 덮거나 튜토리얼을 끄면
   판이 꼬이기 때문이다. 호출부가 따로 #if 로 막지 않아도 된다.
*/
struct FFF_DebugConsoleCommands
{
	void Add(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandDelegate& Command)
	{
		AddImpl(Name, [Name, Help, &Command]()
		{
			return IConsoleManager::Get().RegisterConsoleCommand(Name, Help, Command);
		});
	}

	void Add(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithArgsDelegate& Command)
	{
		AddImpl(Name, [Name, Help, &Command]()
		{
			return IConsoleManager::Get().RegisterConsoleCommand(Name, Help, Command);
		});
	}

	// 내가 건 것 중 아직 내가 주인인 것만 지운다. 목록은 무조건 비운다.
	void UnregisterAll()
	{
		IConsoleManager& ConsoleManager = IConsoleManager::Get();

		for (const FEntry& Entry : Entries)
		{
			if (Entry.Command && ConsoleManager.FindConsoleObject(*Entry.Name, false) == Entry.Command)
			{
				ConsoleManager.UnregisterConsoleObject(Entry.Command);
			}
		}

		Entries.Reset();
	}

	// 지금 들고 있는 명령 수. 다른 월드가 먼저 걸었으면 0 일 수 있다.
	int32 Num() const { return Entries.Num(); }

private:
	struct FEntry
	{
		FString Name;
		IConsoleCommand* Command = nullptr;
	};

	template <typename RegisterFunc>
	void AddImpl(const TCHAR* Name, RegisterFunc&& Register)
	{
#if UE_BUILD_SHIPPING
		(void)Name;
		(void)Register;
#else
		if (IConsoleManager::Get().FindConsoleObject(Name, false))
		{
			return;
		}

		if (IConsoleCommand* Command = Register())
		{
			FEntry& Entry = Entries.AddDefaulted_GetRef();
			Entry.Name = Name;
			Entry.Command = Command;
		}
#endif
	}

	TArray<FEntry> Entries;
};
