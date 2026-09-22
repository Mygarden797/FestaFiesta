// 축제장 배치를 슬롯에 저장하고 되돌린다. 자동 측정이 사람이 쓰는 배치 위에서 돌게 하는 도구다.

#include "Zone/FF_ZoneLayoutSaveGame.h"

#if !UE_BUILD_SHIPPING

#include "Engine/World.h"
#include "FF_FFZSWorldSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

namespace FFZoneLayoutConsole
{
	FString ResolveSlot(const TArray<FString>& Args)
	{
		return Args.Num() > 0 ? Args[0] : TEXT("day1");
	}

	UFFZSWorldSubsystem* GetZones(UWorld* World, FOutputDevice& Ar)
	{
		UFFZSWorldSubsystem* Zones = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
		if (!Zones)
		{
			Ar.Logf(TEXT("ZoneSystem 이 없다. PIE 나 게임에서 실행할 것."));
		}
		return Zones;
	}
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GZoneSaveLayout(
	TEXT("FF.Zone.SaveLayout"),
	TEXT("지금 설치된 축제장 배치를 슬롯에 저장한다. FF.Zone.SaveLayout [슬롯명=day1]"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
		{
			UFFZSWorldSubsystem* Zones = FFZoneLayoutConsole::GetZones(World, Ar);
			if (!Zones) { return; }

			UFF_ZoneLayoutSaveGame* Save = Cast<UFF_ZoneLayoutSaveGame>(
				UGameplayStatics::CreateSaveGameObject(UFF_ZoneLayoutSaveGame::StaticClass()));
			if (!Save)
			{
				Ar.Logf(TEXT("세이브 객체를 만들지 못했다."));
				return;
			}
			Save->Snapshot = Zones->CaptureWorldSnapshot();
			Save->MapName = World->GetOutermost()->GetName();

			const FString Slot = FFZoneLayoutConsole::ResolveSlot(Args);
			if (!UGameplayStatics::SaveGameToSlot(Save, Slot, 0))
			{
				Ar.Logf(TEXT("슬롯 '%s' 저장 실패."), *Slot);
				return;
			}
			Ar.Logf(TEXT("배치를 슬롯 '%s' 에 저장했다 — 시설 %d개, 맵 %s. ")
				TEXT("자동 측정은 FF.Zone.LoadLayout %s 로 이 배치를 쓴다."),
				*Slot, Save->Snapshot.Facilities.Num(), *Save->MapName, *Slot);
		}));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GZoneLoadLayout(
	TEXT("FF.Zone.LoadLayout"),
	TEXT("저장해 둔 축제장 배치를 되돌린다. FF.Zone.LoadLayout [슬롯명=day1]"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
		{
			UFFZSWorldSubsystem* Zones = FFZoneLayoutConsole::GetZones(World, Ar);
			if (!Zones) { return; }

			const FString Slot = FFZoneLayoutConsole::ResolveSlot(Args);
			UFF_ZoneLayoutSaveGame* Save = Cast<UFF_ZoneLayoutSaveGame>(
				UGameplayStatics::LoadGameFromSlot(Slot, 0));
			if (!Save)
			{
				Ar.Logf(TEXT("슬롯 '%s' 을 못 읽었다. 먼저 FF.Zone.SaveLayout %s 로 만들 것."),
					*Slot, *Slot);
				return;
			}

			const FString CurrentMap = World->GetOutermost()->GetName();
			if (!Save->MapName.IsEmpty() && Save->MapName != CurrentMap)
			{
				// 좌표는 맵 기준이다. 다른 맵에 부으면 조용히 엉뚱한 곳에 선다.
				Ar.Logf(TEXT("경고: 슬롯은 %s 배치인데 지금 맵은 %s 다."), *Save->MapName, *CurrentMap);
			}

			TArray<FText> Errors;
			if (!Zones->RestoreWorldSnapshot(Save->Snapshot, Errors))
			{
				for (const FText& Error : Errors) { Ar.Logf(TEXT("  복원 오류: %s"), *Error.ToString()); }
				Ar.Logf(TEXT("슬롯 '%s' 복원 실패 (오류 %d건)."), *Slot, Errors.Num());
				return;
			}
			Ar.Logf(TEXT("슬롯 '%s' 배치를 복원했다 — 시설 %d개."),
				*Slot, Save->Snapshot.Facilities.Num());
		}));

#endif // !UE_BUILD_SHIPPING
