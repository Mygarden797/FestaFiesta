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
	//
	// RepulsionB와 함께 움직여야 한다. 반발이 exp(-(d-RSum)/B)로 죽으므로 이 반경은
	// "힘이 무시할 만해지는 거리"여야 한다. B=0.25 기준 3m에서 0.003m/s^2라 여기서 자른다.
	// 반경만 줄이면 아직 유효한 힘을 잘라내 이웃 경계에서 가속도가 튄다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Broadphase", meta = (ClampMin = "0.5"))
	float NeighbourRadiusMeters = 3.f;

	// 균일 격자 셀 한 변(m). NeighbourRadiusMeters 이상이어야 이웃 탐색이 3x3 셀로 끝난다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Broadphase", meta = (ClampMin = "0.5"))
	float BroadphaseCellSizeMeters = 3.f;

	// 한 개체가 고려할 이웃 수 상한. 성능 상한이자 결정성의 전제다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Broadphase", meta = (ClampMin = "4", ClampMax = "32"))
	int32 MaxNeighbours = 12;

	// 목표 속도로 수렴하는 완화 시간(s). 0.5s가 보행자 문헌값이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forces", meta = (ClampMin = "0.1"))
	float RelaxationTau = 0.5f;

	// 대인 반발력 계수(N).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forces")
	float RepulsionA = 2000.f;

	// 대인 반발력 감쇠 거리(m). 0.25 = 25cm다. 25UU로 착각하면 힘이 100배 어긋난다.
	//
	// ★ 문헌값 0.08은 Helbing 2000의 **패닉/접촉 스케일**이다. 그 값을 축제 보행 군중에
	// 쓰면 개인 공간이 사라진다 — 반경 34cm 두 명 기준 1.1m만 떨어져도 반발이
	// 0.04m/s^2로 죽는데 구동력은 3.0m/s^2라, 서로 닿을 때까지 걸어 들어간 뒤
	// ContactStiffness가 받을 때까지 멈추지 않는다(평형 겹침 1.2mm = 몸이 붙는다).
	//
	// 0.25는 일상 보행 사회력 문헌 범위(0.2~0.4m)다. 이 값이면 1.5m에서 1.1m/s^2가
	// 나와 다가가면서 자연스럽게 간격이 벌어진다.
	// 병목 아치·압사 재현이 목적이면 0.08로 되돌리고 MaxOpposingAccelMps2도 같이 낮출 것.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forces")
	float RepulsionB = 0.25f;

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

	// 벽 반발 감쇠 거리(m). Helbing 원본 스케일이다.
	//
	// 한때 0.25로 올려 뒀었다. 그때는 경로추종이 부스를 관통하는 2점 직선만 받아서 우회를
	// 힘으로 흉내내야 했고, 균형점을 0.56m까지 벌려야 사람이 벽을 스치지 않고 돌아갔다.
	//   2000·exp(-x/0.08)/70 = 3  ->  x = 0.18m   (구동력 3.0m/s^2 기준)
	//
	// 지금은 시설이 NavMesh를 도려내므로 우회는 경로가 한다. 벽 반발에 남은 일은
	// **부스 안으로 들어가지 않게 하는 것**뿐이고, 여기에 18cm면 충분하다.
	// ★ 그렇다고 없애면 안 된다 — 적분은 Unit.Position += Velocity*dt 뿐이고 NavMesh는
	//   위치를 구속하지 않는다. 코너 컷(WaypointAcceptanceRadiusUU)과 대인 반발에 밀려
	//   부스로 들어가는 것을 막는 장치는 이 힘 하나다.
	// 0.08은 병목 아치·압사 재현이 살아나는 값이기도 하다. 올리면 그 현상이 뭉개진다.
	// ★ WallCullRadiusMeters와 함께 움직인다. Validate()가 그 관계를 검사한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forces")
	float WallRepulsionB = 0.08f;

	// 마주 오는 사람을 만났을 때 자기 오른쪽으로 비키는 세기. 반발력 크기에 대한 배율이다. 0이면 끈다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forces", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float LaneBiasGain = 0.4f;

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
	//
	// 이 클램프는 **이웃 전원의 합**에 걸린다. 그래서 이 값이 구동력(희망속도/RelaxationTau,
	// 기본 1.5/0.5 = 3.0)보다 크지 않으면 밀집할수록 불리해진다 — 이웃이 12명이든 1명이든
	// 빠져나오는 힘은 같은데 목적지로 미는 힘은 그대로라 뭉친 덩어리가 풀리지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stability", meta = (ClampMin = "0.1"))
	float MaxSocialAccelMps2 = 6.f;

	// 비접촉 반발 중 '목적지로 가는 것을 거스르는' 성분의 상한(m/s^2). 음수면 끈다.
	//
	// ★ 구동력(기본 3.0m/s^2)과의 차이가 곧 "앞사람에게 밀어붙이는 힘"이다.
	// 같은 목적지로 가는 두 명은 서로의 반발이 100% 진행 방향을 거스르므로 전부 이 캡을 탄다.
	// 1.0이면 순 +2.0m/s^2로 앞사람을 밀어 결국 몸이 닿을 때까지 압축된다 — 뭉침의 직접 원인.
	// 2.5면 순 +0.5m/s^2라 간격을 유지하면서도 전진은 계속한다(뒤로 밀려 정체되지 않는다).
	// 구동력을 넘기면 사람이 목적지에서 뒷걸음질 치므로 RelaxationTau를 만졌다면 같이 볼 것.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stability")
	float MaxOpposingAccelMps2 = 2.5f;

	// 벽 반발에 적용할 역방향 가속도 상한(m/s^2). **음수면 면제(기본값).**
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stability")
	float MaxWallOpposingAccelMps2 = -1.f;

	// 이 거리(m)보다 먼 벽 선분은 계산하지 않는다. 개체 반경에 더해서 쓴다.
	//
	// ★ WallRepulsionB와 함께 움직여야 한다 — NeighbourRadiusMeters ↔ RepulsionB와 같은 관계다.
	// 컷오프에서 버려지는 가속도는 WallRepulsionA·exp(-이값/WallRepulsionB)/질량이다.
	// 컷오프가 B에 비해 짧으면 그 경계에서 반발이 뚝 끊겨 사람이 벽 쪽으로 튄다.
	// B=0.08에서 1m면 버리는 양이 0.0001 m/s^2라 경계가 보이지 않는다.
	// B를 0.25로 올린다면 같은 1m에서 0.55 m/s^2(구동력의 18%)가 되므로 2.5m로 같이 올릴 것.
	// 벽 순회는 개체×선분이라 이 반경이 곧 비용이다. B를 낮췄으면 여기도 같이 낮춘다.
	// Validate()가 이 관계를 검사한다. 한쪽만 만지면 거기서 걸린다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stability")
	float WallCullRadiusMeters = 1.0f;

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
