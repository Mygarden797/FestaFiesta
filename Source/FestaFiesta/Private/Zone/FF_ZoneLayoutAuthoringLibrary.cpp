// FF_ZoneLayoutAuthoringLibrary.cpp
// 역할: 에디터 제작 요청을 ZoneSystem에 위임하고, 유효한 미개장 시설 배치를 안전한 슬롯명으로 저장·재검증한다.
#include "Zone/FF_ZoneLayoutAuthoringLibrary.h"

#if WITH_EDITOR

#include "Engine/World.h"
#include "FF_FFZSWorldSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "UObject/Class.h"
#include "Zone/FF_ZoneLayoutSaveGame.h"

#define LOCTEXT_NAMESPACE "FFZoneLayoutAuthoringLibrary"

namespace FFZoneLayoutAuthoring
{
	// 기존 FF.Zone.SaveLayout과 시나리오가 사용하는 동일한 로컬 사용자 슬롯이다.
	constexpr int32 LayoutUserIndex = 0;

	/** 유효한 월드와 ZoneSystem이 모두 있을 때만 제작 요청을 위임한다. */
	UFFZSWorldSubsystem* ResolveZones(UWorld* World)
	{
		return IsValid(World) ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	}

	/** 경로·확장자·공백을 슬롯으로 받지 않으며, 운영체제가 예약한 파일 이름도 거부한다. */
	bool ValidateSlotName(const FString& SlotName, TArray<FText>& OutErrors)
	{
		if (SlotName.IsEmpty())
		{
			OutErrors.Add(LOCTEXT("EmptySlot", "저장 슬롯 이름이 비어 있습니다."));
			return false;
		}

		for (const TCHAR Character : SlotName)
		{
			if (!FChar::IsAlnum(Character) && Character != TEXT('_') && Character != TEXT('-'))
			{
				OutErrors.Add(LOCTEXT("InvalidSlotCharacters", "슬롯 이름에는 문자, 숫자, 밑줄(_)과 하이픈(-)만 사용할 수 있습니다."));
				return false;
			}
		}

		FText PathError;
		if (!FPaths::ValidatePath(SlotName, &PathError))
		{
			OutErrors.Add(PathError);
			return false;
		}
		return true;
	}
}

FFFZSPlacementValidation UFF_ZoneLayoutAuthoringLibrary::ValidatePlacement(UWorld* World, const FFFZSCommand& Command)
{
	if (const UFFZSWorldSubsystem* Zones = FFZoneLayoutAuthoring::ResolveZones(World))
	{
		return Zones->ValidatePlacement(Command);
	}

	FFFZSPlacementValidation Result;
	Result.AddFailure(EFFZSPlacementFailure::InternalError,
		LOCTEXT("ValidationMissingWorld", "배치를 검증할 유효한 월드 또는 ZoneSystem이 없습니다."));
	return Result;
}

FFFZSCommandResult UFF_ZoneLayoutAuthoringLibrary::ExecutePlacement(UWorld* World, const FFFZSCommand& Command)
{
	if (UFFZSWorldSubsystem* Zones = FFZoneLayoutAuthoring::ResolveZones(World))
	{
		return Zones->ExecuteCommand(Command);
	}

	FFFZSCommandResult Result;
	Result.FailureReason = EFFZSPlacementFailure::InternalError;
	Result.Message = LOCTEXT("ExecutionMissingWorld", "배치 명령을 실행할 유효한 월드 또는 ZoneSystem이 없습니다.");
	return Result;
}

bool UFF_ZoneLayoutAuthoringLibrary::SaveLayout(UWorld* World, const FString& SlotName,
	const bool bOverwrite, TArray<FText>& OutErrors)
{
	OutErrors.Reset();
	UFFZSWorldSubsystem* Zones = FFZoneLayoutAuthoring::ResolveZones(World);
	if (!Zones)
	{
		OutErrors.Add(LOCTEXT("SaveMissingWorld", "배치를 저장할 유효한 월드 또는 ZoneSystem이 없습니다."));
		return false;
	}
	if (!FFZoneLayoutAuthoring::ValidateSlotName(SlotName, OutErrors))
	{
		return false;
	}
	if (!bOverwrite && UGameplayStatics::DoesSaveGameExist(SlotName, FFZoneLayoutAuthoring::LayoutUserIndex))
	{
		OutErrors.Add(FText::Format(LOCTEXT("SlotExists", "슬롯 '{0}'이 이미 있습니다. 덮어쓰기를 명시해야 저장할 수 있습니다."),
			FText::FromString(SlotName)));
		return false;
	}

	// 한 번 캡처한 값으로 저장 전 검사와 저장을 모두 수행하여 서로 다른 시점의 배치가 섞이지 않게 한다.
	const FFFZSWorldSnapshot Snapshot = Zones->CaptureWorldSnapshot();
	if (Snapshot.Facilities.IsEmpty())
	{
		OutErrors.Add(LOCTEXT("EmptyLayout", "설치된 시설이 하나 이상 있어야 배치를 저장할 수 있습니다."));
		return false;
	}
	for (const FFFZSFacilityInstanceSnapshot& Facility : Snapshot.Facilities)
	{
		if (Facility.State != EFFZSFacilityState::Validated || Facility.bNeedsRelocation)
		{
			OutErrors.Add(FText::Format(
				LOCTEXT("FacilityNotReady", "시설 '{0}'({1})이 검증 완료 상태가 아니거나 재배치가 필요합니다. 개장 전 정상 배치만 저장할 수 있습니다."),
				FText::FromName(Facility.FacilitySpec.FacilityDefinitionId),
				FText::FromString(Facility.FacilityInstanceId.ToString())));
		}
	}
	if (!OutErrors.IsEmpty())
	{
		return false;
	}

	UFF_ZoneLayoutSaveGame* Save = Cast<UFF_ZoneLayoutSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UFF_ZoneLayoutSaveGame::StaticClass()));
	if (!Save)
	{
		OutErrors.Add(LOCTEXT("SaveObjectFailed", "배치 저장 객체를 만들지 못했습니다."));
		return false;
	}
	Save->Snapshot = Snapshot;
	Save->MapName = World->GetOutermost()->GetName();
	if (!UGameplayStatics::SaveGameToSlot(Save, SlotName, FFZoneLayoutAuthoring::LayoutUserIndex))
	{
		OutErrors.Add(LOCTEXT("WriteFailed", "배치 슬롯 파일 저장에 실패했습니다."));
		return false;
	}

	// 시설 수만 비교하지 않고 사양·위치·상태·대기열·명령 영수증을 포함한 전체 저장 값을 확인한다.
	const UFF_ZoneLayoutSaveGame* Reloaded = Cast<UFF_ZoneLayoutSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SlotName, FFZoneLayoutAuthoring::LayoutUserIndex));
	if (!Reloaded || Reloaded->MapName != Save->MapName
		|| !FFFZSWorldSnapshot::StaticStruct()->CompareScriptStruct(&Save->Snapshot, &Reloaded->Snapshot, 0))
	{
		OutErrors.Add(LOCTEXT("ReadbackFailed", "저장 후 재읽기 검증에 실패했습니다. 슬롯 파일은 확인할 수 있도록 남겨 두었습니다."));
		return false;
	}
	return true;
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
