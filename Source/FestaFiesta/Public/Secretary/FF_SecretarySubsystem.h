// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Secretary/FF_SecretaryAdvisor.h"
#include "Subsystems/WorldSubsystem.h"
#include "Time/FF_FestivalStage.h"
#include "FF_SecretarySubsystem.generated.h"

class UFF_FestivalStageSubsystem;
class UFF_SecretaryTipWidget;

/* 본 게임의 비서(2026-10-09 사용자 요청: 튜토리얼 밖에서도 옆에서 훈수).

   - 운영 중: 상황이 생기면 왼쪽 아래 말풍선으로 짧게 훈수한다. 많아도 30초에 한 번, 같은 말은 한동안 안 한다.
     할 상황이 없이 한동안 조용하면 조작 · 규칙 팁을 하나씩 한다(팁마다 축제 동안 한 번).
   - 모닝체크(2 · 3일차 아침): 어제 운영을 대사창으로 짧게 브리핑한다. 첫날 셋업에는 인사만.

   무슨 말을 할지는 판단부(FF_SecretaryAdvisor)가 고르고, 여기서는 값을 모으고 띄울 때를 정한다.
   대사창은 튜토리얼 대사 장치를 브리핑 범위(EFF_TutorialScope::Briefing)로 다시 쓴다.

   ─── 왜 값을 따로 모아 두는가 ───────────────────────────────────────────────
   필요(배고픔 · 화장실 …) 수치는 운영 중의 순간값이라 모닝체크에는 0 일 수 있고, 시설별 "오늘 매출"은 모닝체크에
   0 으로 돌아간다. 그래서 운영 중에 하루치 평균 · 가장 오래 기다린 시설을 모아 두었다가 다음 아침에 쓴다.
   만족도 항목은 아침에도 어제 마지막 값이 남아 있어 그대로 읽는다(김시리 FF_SatisfactionMetricsSubsystem).

   ─── 안 하는 것 ─────────────────────────────────────────────────────────────
   - 팀원 시스템(만족도 · 필요 · 군중 · 사건 · 스태프 · 건축 · 장부)은 공개 값을 읽기만 한다.
   - 튜토리얼 레벨에서는 말하지 않는다. 그 레벨은 튜토리얼 대사가 안내한다.
   - 판 사이에 기억하지 않는다(저장 안 함). 이어하기로 아침에 들어오면 그날 운영 전 값이 없어 브리핑이 짧아진다.
*/
UCLASS()
class FESTAFIESTA_API UFF_SecretarySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_SecretarySubsystem, STATGROUP_Tickables);
	}

	static UFF_SecretarySubsystem* Get(const UObject* WorldContextObject);

	// 말풍선을 바로 띄운다. 개발 명령 · 시험에서 쓴다.
	bool ShowTip(const FText& Text);

	// 지금 값으로 브리핑을 지어 대사창에 띄운다. bFirstDay 면 첫날 인사. 다른 대사가 진행 중이면 false.
	bool PlayMorningBriefing(bool bFirstDay);

	// 지금 훈수 재료 · 브리핑 재료. 개발 명령에서 찍어 본다.
	FFF_SecretaryLiveSignals GatherLiveSignals() const;
	FFF_SecretaryBriefingInputs GatherBriefingInputs() const;

private:
	UFUNCTION()
	void HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage);

	void BindStage();
	void UnbindStage();

	// 튜토리얼 레벨이 아니고 축제 단계 시스템이 있는가.
	bool IsSecretaryWorld() const;
	bool IsDialogueBusy() const;

	void TickLive(float DeltaTime);
	void TickBriefing(float DeltaTime);
	void ScheduleBriefing(bool bFirstDay);

	// 운영 중 하루치 값을 모은다. 다음 아침 브리핑에 쓴다.
	void SampleDay();
	void ResetDaySamples();
	void RememberBuiltCategories();

	UFF_SecretaryTipWidget* EnsureTipWidget();
	double NowSeconds() const;

	TWeakObjectPtr<UFF_FestivalStageSubsystem> BoundStage;

	UPROPERTY(Transient)
	TObjectPtr<UFF_SecretaryTipWidget> TipWidget;

	float CheckCountdown = 0.f;

	// 0 이상이면 그만큼 뒤에 브리핑을 띄운다. 대사가 진행 중이면 조금씩 미룬다.
	float BriefingCountdown = -1.f;
	int32 BriefingRetries = 0;
	bool bBriefingFirstDay = false;
	bool bGreetedFirstDay = false;

	double LastTipTime = -1.0e9;
	double KindLastShown[static_cast<int32>(EFF_SecretaryTipKind::Count)];

	// 오늘 운영이 시작된 때. 조작 팁은 이때(또는 마지막 말풍선)부터 조용한 시간을 잰다.
	double LiveStartedTime = 0.0;

	// 다음에 할 조작 팁(GetGeneralTips 의 위치). 다 하면 더 하지 않는다.
	int32 NextGeneralTip = 0;

	// 하루 모음(운영 중). 운영이 다시 시작될 때 비운다.
	FText DayWorstWaitFacility;
	float DayWorstWaitSeconds = 0.f;

	// 이 판에서 한 번이라도 놓아 본 분류. "아직 안 놓아 본 분류" 소개에 쓴다.
	TSet<EFFZSFacilityCategory> BuiltCategories;
};
