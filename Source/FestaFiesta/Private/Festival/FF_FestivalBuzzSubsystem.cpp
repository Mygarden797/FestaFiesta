// Copyright Epic Games, Inc. All Rights Reserved.

#include "Festival/FF_FestivalBuzzSubsystem.h"

#include "Economy/FF_EconomyLog.h"
#include "Economy/Rules/FF_VisitorForecastRules.h"
#include "Engine/GameInstance.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "NPC/Insights/FF_SnsTypes.h"
#include "Save/FF_SaveArchive.h"
#include "Save/FF_SaveSubsystem.h"

void UFF_FestivalBuzzSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UFF_SaveSubsystem* Save = Collection.InitializeDependency<UFF_SaveSubsystem>())
	{
		Save->RegisterSaveable(this);
	}
}

void UFF_FestivalBuzzSubsystem::Deinitialize()
{
	OnBuzzChanged.Clear();
	Super::Deinitialize();
}

void UFF_FestivalBuzzSubsystem::AddBuzz(const float Delta, const FName Reason)
{
	if (FMath::IsNearlyZero(Delta))
	{
		return;
	}
	SetBuzz(Buzz + Delta, Reason);
}

void UFF_FestivalBuzzSubsystem::ApplySnsPost(const FFF_SnsPostRow& Post)
{
	// 계수는 축제 정의에 있다. 같은 GameInstance 스코프라 매번 조회해도 수명 문제가 없다.
	const UGameInstance* GameInstance = GetGameInstance();
	const UFF_FestivalProgressionSubsystem* Progression = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
	const UFF_FestivalDefinition* Definition = Progression ? Progression->GetDefinition() : nullptr;
	if (Definition == nullptr)
	{
		return;
	}

	AddBuzz(FFF_BuzzRules::CalcSnsPostDelta(
		Post.Sentiment == EFF_SnsSentiment::Positive, Post.TopicSalience0To100, Definition->BuzzPerSnsPost),
		TEXT("SnsPost"));
}

void UFF_FestivalBuzzSubsystem::ResetForNewFestival()
{
	BuzzAtDayStart = FFF_BuzzRules::NeutralBuzz;
	SetBuzz(FFF_BuzzRules::NeutralBuzz, TEXT("NewFestival"), true);
}

void UFF_FestivalBuzzSubsystem::BeginNewDay(const float DecayRatio)
{
	// 아침 기준을 먼저 바꾼 뒤 알린다. 받는 쪽이 "오늘 +N" 을 새 기준으로 다시 계산한다.
	const float Decayed = FFF_BuzzRules::Clamp(FFF_BuzzRules::ApplyDailyDecay(Buzz, DecayRatio));
	BuzzAtDayStart = Decayed;
	SetBuzz(Decayed, TEXT("DayDecay"), true);
}

void UFF_FestivalBuzzSubsystem::SetBuzz(const float NewBuzz, const FName Reason, const bool bAlwaysBroadcast)
{
	const float Previous = Buzz;
	Buzz = FFF_BuzzRules::Clamp(NewBuzz);
	if (!bAlwaysBroadcast && FMath::IsNearlyEqual(Previous, Buzz))
	{
		return;
	}

	UE_LOG(LogFestaFiestaEconomy, Verbose, TEXT("[화제성] %.1f -> %.1f (%s)"), Previous, Buzz, *Reason.ToString());
	OnBuzzChanged.Broadcast(Buzz, Previous);
}

void UFF_FestivalBuzzSubsystem::WriteSaveData(TArray<uint8>& OutBytes) const
{
	FFF_BuzzSave State;
	State.Buzz = Buzz;
	State.BuzzAtDayStart = BuzzAtDayStart;
	UFF_SaveArchive::WriteStruct(OutBytes, State);
}

void UFF_FestivalBuzzSubsystem::ReadSaveData(const TArray<uint8>& InBytes)
{
	FFF_BuzzSave State;
	if (!UFF_SaveArchive::ReadStruct(InBytes, State))
	{
		// 화제성이 없던 옛 저장이다. 지금 세션 값을 남기지 않고 중립으로 되돌린다.
		ResetForNewFestival();
		return;
	}
	BuzzAtDayStart = FFF_BuzzRules::Clamp(State.BuzzAtDayStart);
	SetBuzz(State.Buzz, TEXT("Restore"), true);
}
