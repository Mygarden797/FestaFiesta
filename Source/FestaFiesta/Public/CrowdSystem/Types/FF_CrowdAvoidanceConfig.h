// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FF_CrowdAvoidanceConfig.generated.h"

/**
 * 회피(Helbing 사회력) 계수 모음.
 *
 * ── 단위 규약 ★ ──
 *
 * 이 구조체의 모든 값은 SI(m, m/s, kg, N)이다. 나머지 Crowd 코드가 UU(cm)인 것과 다르다.
 * 일부러 다르게 둔 것이다 — A=2000N, B=0.08m, tau=0.5s는 전부 실측 보행 데이터로 맞춰진
 * SI 문헌값이라서, UU로 옮기면 상수 6개를 다시 튜닝해야 하고 문헌과 대조할 근거가 사라진다.
 *
 * 그래서 변환은 딱 두 곳에만 있다.
 *   1. UFF_CrowdSimulationSubsystem::UpdateAvoidanceMirrors  (UU -> m)
 *   2. UFF_CrowdSimulationSubsystem::ComputeAvoidanceForces  (m/s^2 -> UU/s^2)
 *
 * 힘 계산 코드(FFF_CrowdForceSolver) 안에는 UU가 한 번도 등장하지 않는다.
 * 여기서 어긋나면 힘이 100배 틀리고, 그건 계수 조정으로는 절대 못 잡는다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_CrowdAvoidanceConfig
{
	GENERATED_BODY()

	//======================= 브로드페이즈 =======================

	// 이웃으로 간주할 최대 거리(m).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Broadphase", meta = (ClampMin = "0.5"))
	float NeighbourRadiusMeters = 2.f;

	// 균일 격자 셀 한 변(m). NeighbourRadiusMeters 이상이어야 이웃 탐색이 3x3 셀로 끝난다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Broadphase", meta = (ClampMin = "0.5"))
	float BroadphaseCellSizeMeters = 2.f;

	// 한 개체가 고려할 이웃 수 상한. 성능 상한이자 결정성의 전제다.
	// 먼 이웃의 기여는 exp 감쇠로 이미 무시할 만해서 잘라도 정확도 손실이 거의 없다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Broadphase", meta = (ClampMin = "4", ClampMax = "32"))
	int32 MaxNeighbours = 12;

	//==================== 사회력 (Helbing) ====================

	// 목표 속도로 수렴하는 완화 시간(s).
	// ★ 지금 오케스트레이션은 구동력을 쓰지 않는다(VelocitySmoothTime이 그 역할을 한다).
	//    ComputeDrivingForce를 단위 테스트하거나 나중에 구동력까지 사회력으로 옮길 때의 값이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forces", meta = (ClampMin = "0.1"))
	float RelaxationTau = 0.5f;

	// 대인 반발력 계수(N).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forces")
	float RepulsionA = 2000.f;

	// 대인 반발력 감쇠 거리(m). 0.08 = 8cm다. 8UU로 착각하면 힘이 100배 어긋난다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forces")
	float RepulsionB = 0.08f;

	// 이방성 계수. 0=전방향 균등, 1=전방만. 통상 0.2~0.35.
	// 사람은 뒤에서 다가오는 사람을 덜 피한다. 이걸 빼면 한 방향으로 흐르는 인파가
	// 실제보다 넓게 벌어져 밀도가 과소평가된다 — 이 프로젝트에서는 그게 곧 LOS 오류다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forces", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Anisotropy = 0.3f;

	// 실제로 겹쳤을 때만 작동하는 밀어내기 강성(N/m).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forces")
	float ContactStiffness = 120000.f;

	// 겹쳤을 때의 접선 마찰(N*s/m^2).
	// 이게 없으면 밀집 구간에서 사람들이 서로를 미끄러져 지나가 병목에서 아치가 생기지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forces")
	float ContactFriction = 240000.f;

	// 벽 반발 계수(N). 통상 대인 반발과 같은 크기로 시작한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forces")
	float WallRepulsionA = 2000.f;

	// 벽 반발 감쇠 거리(m).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forces")
	float WallRepulsionB = 0.08f;

	//===================== 안정성 클램프 =====================
	// 튜닝값이 아니라 안전장치다. 값을 만지기 전에 왜 있는지 먼저 읽을 것.

	/**
	 * exp() 인자의 상한. 초과 시 이 값으로 자른다.
	 *
	 * crush 밀도에서 겹침이 커지면 exp()가 오버플로되어 inf가 되고, 다음 스텝에
	 * inf - inf = NaN이 되어 배열 전체로 전파된다. NaN이 퍼진 뒤에는 원인 지점을
	 * 역추적할 수 없으므로 사후 검사로는 늦다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stability")
	float MaxExponentArg = 40.f;

	/**
	 * 한 스텝에서 허용할 가속도 크기 상한(m/s^2).
	 *
	 * 없으면 접촉력이 지배하는 순간 한 스텝에 수 미터를 튀어 벽을 통과한다.
	 * 접촉 강성을 낮춰서 막으려 하지 말 것 — 그러면 이번엔 겹침이 돌아온다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stability")
	float MaxAccelerationMps2 = 50.f;

	//======================= 막힘 해소 =======================

	// 이 스텝 수 이상 제자리에 머물면 막힌 것으로 보고 탈출 가속을 태운다. 0이면 끈다.
	// 처음부터 넣는다 — 나중에 붙이면 "가끔 안 움직이는 NPC"를 재현하느라 하루를 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unstuck", meta = (ClampMin = "0"))
	int32 StuckDetectSteps = 40;

	// 탈출 가속 크기(m/s^2). 너무 크면 막히지 않은 사람도 튀어 보인다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unstuck", meta = (ClampMin = "0.0"))
	float UnstuckAccelMps2 = 6.f;

	// 이 속도(UU/s) 미만으로 움직이면 정지로 본다. 막힘 카운터의 판정 기준이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unstuck", meta = (ClampMin = "0.0"))
	float StuckSpeedThresholdUU = 20.f;

	/**
	 * 설정값 간 모순을 검사한다. 진입점에서 막기 위한 것이다.
	 *
	 * 셀 크기 < 이웃 반경이면 3x3 셀 밖의 이웃을 놓쳐 반발력이 조용히 약해진다 —
	 * 크래시도 경고도 없이 "회피가 좀 약한" 상태가 되어서 원인을 찾기 어렵다.
	 */
	bool Validate(FString& OutError) const;
};
