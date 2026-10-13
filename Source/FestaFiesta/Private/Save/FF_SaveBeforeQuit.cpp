// FF_SaveBeforeQuit.cpp
// 역할: 종료 직전 저장을 SaveManual 에 맡기고, 그 결과로 종료 가능 여부를 판정한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/FF_SaveBeforeQuit.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Save/FF_SaveBeforeQuitPolicy.h"
#include "Save/FF_SaveSubsystem.h"
#include "Save/FF_SaveTypes.h"
#include "Time/FF_FestivalStageSubsystem.h"

EFF_SaveBeforeQuitResult UFF_SaveBeforeQuitLibrary::SaveBeforeQuit(
	const UObject* WorldContextObject, FName& OutSlotId)
{
	OutSlotId = NAME_None;

	// 판정을 저장 "전" 에 해 둔다. 저장이 실패한 뒤의 상태로 판정하면 원인과 판정이 섞인다.
	const bool bFestivalInProgress = IsFestivalInProgress(WorldContextObject);

	UFF_SaveSubsystem* SaveSubsystem = UFF_SaveSubsystem::Get(WorldContextObject);
	if (!SaveSubsystem)
	{
		// 서브시스템이 없으면 저장할 길이 없다. 축제 도중이면 Failed 로 남겨 종료를 막는다.
		UE_LOG(LogFestaFiestaSave, Error, TEXT("[저장 후 종료] 저장 서브시스템을 찾지 못했다."));
		return FF::SaveBeforeQuit::Classify(false, bFestivalInProgress);
	}

	OutSlotId = SaveSubsystem->SaveManual();
	const EFF_SaveBeforeQuitResult Result =
		FF::SaveBeforeQuit::Classify(!OutSlotId.IsNone(), bFestivalInProgress);

	switch (Result)
	{
	case EFF_SaveBeforeQuitResult::Saved:
		UE_LOG(LogFestaFiestaSave, Log, TEXT("[저장 후 종료] %s 에 저장했다."), *OutSlotId.ToString());
		break;
	case EFF_SaveBeforeQuitResult::NothingToSave:
		UE_LOG(LogFestaFiestaSave, Log, TEXT("[저장 후 종료] 담을 판이 없어 저장 없이 종료한다."));
		break;
	case EFF_SaveBeforeQuitResult::Failed:
		UE_LOG(LogFestaFiestaSave, Warning, TEXT("[저장 후 종료] 축제 도중 저장에 실패해 종료를 보류한다."));
		break;
	}

	return Result;
}

bool UFF_SaveBeforeQuitLibrary::IsFestivalInProgress(const UObject* WorldContextObject)
{
	/* SaveManual 이 "축제 세이브" 를 고르는 조건과 같다.

	   CaptureSession 이 축제 이름은 ResolveCurrentFestivalId 로, 날짜는 Stage 의 CurrentDay 로
	   채우고, FFF_SessionSave::IsFestivalInProgress 가 "이름이 있고 날짜가 1 이상" 을 본다.
	   로비(프론트엔드)에서는 ResolveCurrentFestivalId 가 None 을 주므로 자연히 false 다.
	*/
	if (UFF_SaveSubsystem::ResolveCurrentFestivalId(WorldContextObject).IsNone())
	{
		return false;
	}

	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	const UFF_FestivalStageSubsystem* Stage = World
		? UGameInstance::GetSubsystem<UFF_FestivalStageSubsystem>(World->GetGameInstance())
		: nullptr;

	return Stage && Stage->CurrentDay > 0;
}
