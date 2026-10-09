// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FF_SecretarySettings.generated.h"

/* 비서 훈수 · 아침 브리핑 설정(2026-10-09). Project Settings > FestaFiesta Secretary.

   수치는 코드가 아니라 여기서 맞춘다. 처음 결정은 "상황이 생길 때만, 많아도 1분에 한 번"이었는데,
   말풍선을 보고 "좀 더 자주 나와도 좋다"로 바뀌었다(2026-10-09). 운영 하루가 1배속으로 4~5분이라
   1분 간격이면 하루에 몇 번 안 나온다. 그래서 간격을 30초로 줄이고, 상황이 없으면 조작 팁을 한다.
*/
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "FestaFiesta Secretary"))
class FESTAFIESTA_API UFF_SecretarySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UFF_SecretarySettings* Get() { return GetDefault<UFF_SecretarySettings>(); }

	// 운영 중 말풍선 훈수.
	UPROPERTY(EditAnywhere, Config, Category = "Secretary")
	bool bEnableLiveTips = true;

	// 모닝체크(2 · 3일차 아침) 브리핑과 첫날 인사.
	UPROPERTY(EditAnywhere, Config, Category = "Secretary")
	bool bEnableMorningBriefing = true;

	// 대사창 화자 이름. 초상화도 튜토리얼 설정의 Speaker Portraits 에서 이 이름으로 찾는다.
	UPROPERTY(EditAnywhere, Config, Category = "Secretary")
	FString SpeakerName = TEXT("비서");

	// ── 운영 중 훈수 ───────────────────────────────────────────────────────

	// 상황을 살피는 주기(초).
	UPROPERTY(EditAnywhere, Config, Category = "Secretary|Live", meta = (ClampMin = "0.5"))
	float CheckIntervalSeconds = 2.f;

	// 훈수 사이 최소 간격(실제 초). 상황이 겹쳐도 이보다 자주 나오지 않는다.
	UPROPERTY(EditAnywhere, Config, Category = "Secretary|Live", meta = (ClampMin = "5.0"))
	float MinSecondsBetweenTips = 30.f;

	// 같은 종류의 훈수를 다시 하기까지(실제 초). 같은 말을 되풀이하지 않게 한다.
	UPROPERTY(EditAnywhere, Config, Category = "Secretary|Live", meta = (ClampMin = "5.0"))
	float SameKindRestSeconds = 150.f;

	// 할 상황이 없을 때 조작 · 규칙 팁. 팁마다 축제 동안 한 번씩만 한다.
	UPROPERTY(EditAnywhere, Config, Category = "Secretary|Live")
	bool bEnableGeneralTips = true;

	// 운영이 시작되거나 마지막 말풍선 뒤로 이만큼(실제 초) 조용하면 조작 팁을 하나 한다.
	UPROPERTY(EditAnywhere, Config, Category = "Secretary|Live", meta = (ClampMin = "5.0"))
	float GeneralTipQuietSeconds = 40.f;

	// 말풍선이 떠 있는 시간(초). 누르면 바로 닫힌다.
	UPROPERTY(EditAnywhere, Config, Category = "Secretary|Live", meta = (ClampMin = "1.0"))
	float TipVisibleSeconds = 7.f;

	// 이 인원 이상 줄 선 시설이 있으면 "줄이 길어요".
	UPROPERTY(EditAnywhere, Config, Category = "Secretary|Live", meta = (ClampMin = "1"))
	int32 LongQueueCount = 8;

	// 혼잡 칸(LOS D 보다 나쁜 칸)이 이만큼이면 "사람이 몰려요".
	UPROPERTY(EditAnywhere, Config, Category = "Secretary|Live", meta = (ClampMin = "1"))
	int32 CrowdedCellCount = 6;

	// 필요(배고픔 · 화장실 …)를 못 채운 손님 비율(%)이 이 이상이면 그 필요를 짚는다.
	UPROPERTY(EditAnywhere, Config, Category = "Secretary|Live", meta = (ClampMin = "1", ClampMax = "100"))
	int32 UnmetNeedPercent = 35;

	// 스태프가 안 간 사건이 이 축제 분 이상 지나면 짚는다.
	UPROPERTY(EditAnywhere, Config, Category = "Secretary|Live", meta = (ClampMin = "0.0"))
	float IdleIncidentMinutes = 10.f;

	// ── 아침 브리핑 ────────────────────────────────────────────────────────

	// 모닝체크가 시작되고 브리핑을 띄우기까지(초). 하루 넘김 처리(만족도 기록 · 장부)가 먼저 끝나게 한다.
	UPROPERTY(EditAnywhere, Config, Category = "Secretary|Briefing", meta = (ClampMin = "0.0"))
	float BriefingDelaySeconds = 1.5f;

	// 만족도 항목이 모두 이 점수 이상이면 "크게 아쉬운 점이 없었어요".
	UPROPERTY(EditAnywhere, Config, Category = "Secretary|Briefing", meta = (ClampMin = "0", ClampMax = "100"))
	int32 GoodScore = 70;

	// ── 말풍선 모양 ────────────────────────────────────────────────────────

	// 초상화에서 얼굴만 쓰는 영역(UV). 동그랗게 잘라 말풍선 옆에 띄운다.
	UPROPERTY(EditAnywhere, Config, Category = "Secretary|Bubble")
	FVector2D FaceUVMin = FVector2D(0.2945, 0.0576);

	UPROPERTY(EditAnywhere, Config, Category = "Secretary|Bubble")
	FVector2D FaceUVMax = FVector2D(0.8233, 0.4194);

	// 화면 왼쪽 아래 모서리에서 말풍선까지(화면 단위). 아래 탭 줄을 비켜 둔다.
	UPROPERTY(EditAnywhere, Config, Category = "Secretary|Bubble")
	FVector2D BubbleOffset = FVector2D(24.0, -120.0);

	// 말풍선 화면 순서. 준비 HUD(0) 위, 시설 정보창(95) · 튜토리얼 덮개(100) 아래.
	UPROPERTY(EditAnywhere, Config, Category = "Secretary|Bubble")
	int32 BubbleZOrder = 90;

	virtual FName GetCategoryName() const override { return TEXT("Project"); }
};
