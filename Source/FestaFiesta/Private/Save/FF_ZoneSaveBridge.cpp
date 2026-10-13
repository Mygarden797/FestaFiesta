// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/FF_ZoneSaveBridge.h"

#include "Engine/AssetManager.h"
#include "EngineUtils.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "Save/FF_SaveArchive.h"
#include "Save/FF_SaveSubsystem.h"

bool UFF_ZoneSaveBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UFF_ZoneSaveBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (UFF_SaveSubsystem* Save = UFF_SaveSubsystem::Get(this))
	{
		Save->RegisterSaveable(this);
		bRegistered = true;
	}
}

void UFF_ZoneSaveBridge::Deinitialize()
{
	if (bRegistered)
	{
		if (UFF_SaveSubsystem* Save = UFF_SaveSubsystem::Get(this))
		{
			Save->UnregisterSaveable(this);
		}
		bRegistered = false;
	}

	Super::Deinitialize();
}

void UFF_ZoneSaveBridge::CollectFacilities(FFF_ZoneSaveState& Out) const
{
	Out.Facilities.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AFFZSFacilityActor> It(World); It; ++It)
	{
		const AFFZSFacilityActor* Facility = *It;
		if (!IsValid(Facility))
		{
			continue;
		}

		const FFFZSFacilitySpec& Spec = Facility->GetFacilitySpec();
		if (Spec.FacilityDefinitionId.IsNone())
		{
			// 정의를 모르면 되돌릴 수 없다. 담아도 쓸모가 없으므로 건너뛴다.
			continue;
		}

		FFF_ZoneFacilitySave Saved;
		Saved.DefinitionId = Spec.FacilityDefinitionId;
		Saved.ZoneId = Facility->GetZoneId();
		Saved.Placement = Facility->GetGridPlacement();
		Saved.InstanceId = Facility->GetFacilityInstanceId();
		Saved.UpgradeLevel = Facility->GetUpgradeLevel();

		Out.Facilities.Add(Saved);
	}
}

void UFF_ZoneSaveBridge::ClearExistingFacilities()
{
	UWorld* World = GetWorld();
	UFFZSWorldSubsystem* ZoneWorld = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	if (!ZoneWorld)
	{
		return;
	}

	/* 먼저 모아두고 그다음에 지운다.

	   훑는 도중에 지우면 반복자가 무효가 된다. 액터를 지우는 것은 목록을
	   바꾸는 일이라, 목록을 도는 중에 하면 안 된다.
	*/
	TArray<FGuid> ToRemove;
	for (TActorIterator<AFFZSFacilityActor> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			ToRemove.Add(It->GetFacilityInstanceId());
		}
	}

	for (const FGuid& InstanceId : ToRemove)
	{
		FFFZSCommand Command;
		Command.Type = EFFZSCommandType::Remove;
		Command.CommandId = FGuid::NewGuid();
		Command.FacilityInstanceId = InstanceId;

		ZoneWorld->ExecuteCommand(Command);
	}
}

int32 UFF_ZoneSaveBridge::RestoreFacilities(const FFF_ZoneSaveState& In)
{
	UWorld* World = GetWorld();
	UFFZSWorldSubsystem* ZoneWorld = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	if (!ZoneWorld)
	{
		return 0;
	}

	const UAssetManager* Manager = UAssetManager::GetIfInitialized();
	if (!Manager)
	{
		UE_LOG(LogFestaFiestaSave, Warning,
			TEXT("[저장] 애셋 매니저가 없어 시설을 되돌리지 못한다."));
		return 0;
	}

	int32 Restored = 0;

	for (const FFF_ZoneFacilitySave& Saved : In.Facilities)
	{
		const FPrimaryAssetId AssetId(
			UFFZSFacilityDefinition::GetFacilityDefinitionAssetType(), Saved.DefinitionId);

		const FSoftObjectPath Path = Manager->GetPrimaryAssetPath(AssetId);
		if (!Path.IsValid())
		{
			/* 정의가 사라졌다. 시설을 지우거나 이름을 바꾼 경우다.

			   전체를 실패로 돌리지 않는다. 나머지 시설은 되돌릴 수 있고,
			   하나 때문에 저장 전체를 버리면 되살릴 수 있는 것까지 잃는다.
			*/
			UE_LOG(LogFestaFiestaSave, Warning,
				TEXT("[저장] 시설 정의 %s 를 찾지 못했다. 건너뛴다."),
				*Saved.DefinitionId.ToString());
			continue;
		}

		const UFFZSFacilityDefinition* Definition =
			Cast<UFFZSFacilityDefinition>(Path.TryLoad());
		if (!Definition)
		{
			UE_LOG(LogFestaFiestaSave, Warning,
				TEXT("[저장] 시설 정의 %s 를 읽지 못했다."), *Saved.DefinitionId.ToString());
			continue;
		}

		/* 설치 명령을 만들어 보낸다.

		   액터를 직접 스폰하지 않는 이유는, 플레이어가 놓을 때 타는 검증을
		   그대로 타기 위해서다. 겹침·격자·Zone 안쪽 판정이 여기서 다 걸린다.
		*/
		FFFZSCommand Command = UFFZSGridLibrary::MakeInstallCommandFromDefinition(
			Definition, Saved.ZoneId, Saved.Placement);
		if (Saved.InstanceId.IsValid() && !ZoneWorld->FindFacility(Saved.InstanceId))
		{
			Command.FacilityInstanceId = Saved.InstanceId;
		}

		const FFFZSCommandResult Result = ZoneWorld->ExecuteCommand(Command);
		if (Result.bSucceeded)
		{
			ReapplyUpgrades(*ZoneWorld, Command.FacilityInstanceId, Saved.UpgradeLevel);
			++Restored;
			continue;
		}

		/* 검증에 걸렸다.

		   값을 직접 쓰는 방식이었으면 무조건 들어갔을 것이고, 어긋난 배치를
		   나중에야 발견했을 것이다. 여기서 알려주는 편이 낫다.
		*/
		UE_LOG(LogFestaFiestaSave, Warning,
			TEXT("[저장] 시설 %s 설치 실패. Zone=%s"),
			*Saved.DefinitionId.ToString(), *Saved.ZoneId.ToString());
	}

	return Restored;
}

void UFF_ZoneSaveBridge::ReapplyUpgrades(UFFZSWorldSubsystem& ZoneWorld, const FGuid& InstanceId, const int32 TargetLevel)
{
	/* 강화도 명령으로 한 단계씩 밟는다. 플러그인은 "다음 단계"만 받고, 커진 크기의 겹침 검사도 거기서 돈다.
	   비용은 강화 정책이 저장 복원 중이면 받지 않는다(UFF_FacilityUpgradeSubsystem::ValidatePurchase).
	*/
	while (const AFFZSFacilityActor* Facility = ZoneWorld.FindFacility(InstanceId))
	{
		if (Facility->GetUpgradeLevel() >= TargetLevel)
		{
			return;
		}

		const FFFZSFacilityUpgradeTier* Tier = FFZSFacilityUpgradeRules::FindNextTier(
			Facility->GetBaseFacilitySpec().UpgradeTiers, Facility->GetUpgradeLevel());
		if (!Tier)
		{
			UE_LOG(LogFestaFiestaSave, Warning,
				TEXT("[저장] 시설 %s 의 강화 %d단계가 지금 정의에 없다. %d단계로 둔다."),
				*InstanceId.ToString(), TargetLevel, Facility->GetUpgradeLevel());
			return;
		}

		FFFZSCommand Command;
		Command.CommandId = FGuid::NewGuid();
		Command.TransactionId = Command.CommandId;
		Command.ExpectedWorldRevision = ZoneWorld.GetWorldRevision();
		Command.Type = EFFZSCommandType::Upgrade;
		Command.FacilityInstanceId = InstanceId;
		Command.TargetUpgradeLevel = Tier->TargetLevel;
		Command.UpgradeServiceDurationMultiplier = Tier->ServiceDurationMultiplier;
		Command.UpgradeSizeMultiplier = Tier->SizeMultiplier;
		Command.UpgradeRevenueMultiplier = Tier->RevenueMultiplier;

		const FFFZSCommandResult Result = ZoneWorld.ExecuteCommand(Command);
		if (!Result.bSucceeded)
		{
			UE_LOG(LogFestaFiestaSave, Warning,
				TEXT("[저장] 시설 %s 강화 %d단계 복원 실패: %s"),
				*InstanceId.ToString(), Tier->TargetLevel, *Result.Message.ToString());
			return;
		}
	}
}

void UFF_ZoneSaveBridge::WriteSaveData(TArray<uint8>& OutBytes) const
{
	FFF_ZoneSaveState State;
	CollectFacilities(State);

	UFF_SaveArchive::WriteStruct(OutBytes, State);

	UE_LOG(LogFestaFiestaSave, Log,
		TEXT("[저장] 시설 %d개를 담았다."), State.Facilities.Num());
}

void UFF_ZoneSaveBridge::ReadSaveData(const TArray<uint8>& InBytes)
{
	FFF_ZoneSaveState State;
	if (!UFF_SaveArchive::ReadStruct(InBytes, State))
	{
		// 저장에 시설이 없다. 첫 축제이거나 옛 저장이다.
		return;
	}

	// 지금 놓인 것을 치우고 저장된 것으로 갈아끼운다.
	// 치우지 않으면 저장된 것 위에 지금 것이 겹쳐 남는다.
	ClearExistingFacilities();

	const int32 Restored = RestoreFacilities(State);

	UE_LOG(LogFestaFiestaSave, Log,
		TEXT("[저장] 시설 %d/%d개를 되돌렸다."), Restored, State.Facilities.Num());
}
