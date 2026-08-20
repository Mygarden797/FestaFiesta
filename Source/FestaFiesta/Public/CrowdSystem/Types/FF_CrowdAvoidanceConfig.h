// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FF_CrowdAvoidanceConfig.generated.h"

// 회피(Helbing 사회력) 계수 모음.
//
// 단위 규약: 이 구조체의 값은 전부 SI(m, kg, N)다. 나머지 Crowd 코드는 UU(cm)다.
// 일부러 다르게 뒀다 — A=2000N, B=0.08m, tau=0.5s는 실측 보행 데이터의 SI 문헌값이라
// UU로 옮기면 문헌과 대조할 근거가 사라진다.
//
// 변환 지점은 두 곳뿐이다.
//   UpdateAvoidanceMirrors  (UU -> m)
//   ComputeAvoidanceForces  (m/s^2 -> UU/s^2)
// 여기서 어긋나면 힘이 100배 틀리고 계수 조정으로는 못 잡는다.
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_CrowdAvoidanceConfig
{
	GENERATED_BODY()

	// 이웃으로 간주할 최대 거리(m).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Broadphase", meta = (ClampMin = "0.5"))
	float NeighbourRadiusMeters = 2.f;

	// 균일 격자 셀 한 변(m). NeighbourRadiusMeters 이상이어야 이웃 탐색이 3x3 셀로 끝난다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Broadphase", meta = (ClampMin = "0.5"))
	float BroadphaseCellSizeMeters = 2.f;

	// 한 개체가 고려할 이웃 수 상한. 성능 상한이자 결정성의 전제다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Broadphase", meta = (ClampMin = "4", ClampMax = "32"))
	int32 MaxNeighbours = 12;

	// 목표 속도로 수렴하는 완화 시간(s). 0.5s가 보행자 문헌값이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forces", meta = (ClampMin = "0.1"))
	float RelaxationTau = 0.5f;

	// 대인 반발력 계수(N).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forces")
	float RepulsionA = 2000.f;

	// 대인 반발력 감쇠 거리(m). 0.08 = 8cm다. 8UU로 착각하면 힘이 100배 어긋난다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forces")
	float RepulsionB = 0.08f;

	// 이방성 계수. 0=전방향 균등, 1=전방만. 통상 0.2~0.35. 사람은 뒤에서 다가오는 사람을 덜 피한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forces", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Anisotropy = 0.3f;

	// 실제로 겹쳤을 때만 작동하는 밀어내기 강성(N/m).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forces")
	float ContactStiffness = 120000.f;

	// 겹쳤을 때의 접선 마찰(N*s/m^2). 이게 없으면 밀집 구간에서 사람들이 서로를 미끄러져 지나가 병목에서 아치가 생기지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forces")
	float ContactFriction = 240000.f;

	// 벽 반발 계수(N). 통상 대인 반발과 같은 크기로 시작한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forces")
	float WallRepulsionA = 2000.f;

	// 벽 반발 감쇠 거리(m).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forces")
	float WallRepulsionB = 0.08f;

	// 마주 오는 사람을 만났을 때 자기 오른쪽으로 비키는 세기. 반발력 크기에 대한 배율이다. 0이면 끈다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forces", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float LaneBiasGain = 0.f;

	// 희망 속도가 이 값(m/s) 미만이면 방향이 없는 것으로 보고 접선 편향을 걸지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forces", meta = (ClampMin = "0.0"))
	float LaneBiasMinDesiredSpeed = 0.1f;

	// exp() 인자의 상한. 초과 시 이 값으로 자른다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stability")
	float MaxExponentArg = 40.f;

	// 한 스텝에서 허용할 **전체** 가속도 크기 상한(m/s^2).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stability")
	float MaxAccelerationMps2 = 50.f;

	// 비접촉(심리적) 반발에만 적용하는 가속도 상한(m/s^2). 보행 스케일이어야 한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stability", meta = (ClampMin = "0.1"))
	float MaxSocialAccelMps2 = 4.f;

	// 비접촉 반발 중 '목적지로 가는 것을 거스르는' 성분의 상한(m/s^2). 음수면 끈다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stability")
	float MaxOpposingAccelMps2 = 1.f;

	// 벽 반발에 적용할 역방향 가속도 상한(m/s^2). **음수면 면제(기본값).**
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stability")
	float MaxWallOpposingAccelMps2 = -1.f;

	// 이 거리(m)보다 먼 벽 선분은 계산하지 않는다. 개체 반경에 더해서 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stability")
	float WallCullRadiusMeters = 1.f;

	// 이 스텝 수 이상 제자리에 머물면 막힌 것으로 보고 탈출 가속을 태운다. 0이면 끈다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unstuck", meta = (ClampMin = "0"))
	int32 StuckDetectSteps = 40;

	// 탈출 가속 크기(m/s^2). 너무 크면 막히지 않은 사람도 튀어 보인다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unstuck", meta = (ClampMin = "0.0"))
	float UnstuckAccelMps2 = 6.f;

	// 목적지 방향 진척이 이 속도(UU/s) 미만이면 막힌 것으로 본다. 막힘 카운터의 판정 기준이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unstuck", meta = (ClampMin = "0.0"))
	float StuckSpeedThresholdUU = 20.f;

	bool Validate(FString& OutError) const;
};
