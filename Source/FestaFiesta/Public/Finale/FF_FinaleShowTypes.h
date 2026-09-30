// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Economy/FF_FestivalEconomyTypes.h"
#include "Engine/DataAsset.h"
#include "FF_FinaleShowTypes.generated.h"

class UNiagaraSystem;

// 피날레 불꽃 연출 관련 타입.
//
// 등급은 경제 쪽이 소유한다. 여기서는 그 등급을 받아 연출 수치로 바꾸는 일만 한다.
// 등급을 정하거나 구매를 처리하지 않는다.

DECLARE_LOG_CATEGORY_EXTERN(LogFestaFiestaFinale, Log, All);

// 한 등급이 만들어내는 연출의 규모.
//
// 수치를 코드가 아니라 데이터 애셋에 두는 이유는, 이런 값은 굴려보면서 정해지기 때문이다.
// 발수 30 이 화려한지 초라한지는 화면을 봐야 알 수 있고, 그때마다 빌드를 다시 하고 싶지 않다.
/* 한 차례에 어떻게 쏠지.

   단발만으로는 아무리 많이 쏴도 흩어진 점으로만 보인다. 사람이 "연출됐다" 고
   느끼는 건 여러 발 사이의 관계 — 같은 순간에 터지거나, 줄지어 날아가거나,
   한 점에서 부챗살로 퍼지거나 — 이기 때문이다.
*/
UENUM(BlueprintType)
enum class EFF_FinaleVolleyPattern : uint8
{
	// 한 발. 지금까지의 기본 동작.
	Single      UMETA(DisplayName = "단발"),

	// 같은 순간에 여러 발. 한 점 주변에 흩어서 터진다.
	Burst       UMETA(DisplayName = "동시"),

	// 일렬. 짧은 간격을 두고 한쪽 끝에서 반대쪽으로 이어진다.
	Line        UMETA(DisplayName = "일렬"),

	// 부챗살. 한 점에서 각도를 벌려 퍼진다.
	Fan         UMETA(DisplayName = "부챗살"),

	// 원. 중심을 두고 둘러싼다.
	Ring        UMETA(DisplayName = "원"),

	/* 구워둔 모양. 하트처럼 수식으로 안 나오는 것들이다.

	   위 넷과 달리 자리를 코드가 아니라 애셋이 갖는다. 수식으로 만들 수 있는 것을
	   굳이 애셋으로 빼지 않는 이유는, 애셋이 늘면 어느 등급이 무엇을 쓰는지
	   에디터를 열어봐야 알게 되기 때문이다.
	*/
	Shape       UMETA(DisplayName = "모양")
};

/* 모양을 어느 면에 그리는가.

   바닥에 그린 모양은 위에서 내려다볼 때만 읽힌다. 불꽃은 하늘에 터지고 관객은
   땅에서 올려다보므로, 읽혀야 하는 모양은 관객을 마주보는 수직면에 그려야 한다.

   일렬·부챗살·원은 바닥에 눕혀도 실루엣이 남아서 지금까지 문제가 없었다.
*/
UENUM(BlueprintType)
enum class EFF_FinaleShapePlane : uint8
{
	/* 패턴에 맞춰 고른다. 기하 패턴은 바닥면, 구워둔 모양은 수직면.

	   기본값을 Ground 로 두면 모양 패턴을 새로 넣는 사람이 이 칸을 못 보고 지나쳐서
	   하트가 바닥에 눕는다. 화면에는 선 하나로 나오는데 원인이 이 칸이라는 걸
	   알아내기 어렵다. 그래서 의도를 기본값에 담고, 필요하면 아래 둘로 덮는다.
	*/
	Auto     UMETA(DisplayName = "자동"),

	// 바닥과 나란한 면. 하늘을 넓게 쓰는 산개형에 맞는다.
	Ground   UMETA(DisplayName = "바닥면"),

	// 관객을 마주보는 수직면. 읽혀야 하는 모양은 이쪽이다.
	Vertical UMETA(DisplayName = "수직면")
};

/* 최고 등급 연출의 막. 시점마다 어느 막에 쓰일지 정한다.

   불꽃만 90초 보여주면 중간부터 지루하다. 그래서 불꽃을 보다가 사흘 동안 지은
   축제장을 한 바퀴 돌고, 다시 하늘로 돌아와 마무리를 본다.

   영상으로 미리 만들어 끼우지 않는 이유는 플레이어가 놓은 시설과 관객이 화면에
   나와야 하기 때문이다. 미리 만든 영상에는 그 판의 축제장이 없다.
*/
UENUM(BlueprintType)
enum class EFF_FinaleTourAct : uint8
{
	// 하늘을 보는 자리. 기본값이라 예전에 놓은 시점은 전부 여기에 든다.
	Fireworks    UMETA(DisplayName = "불꽃 관람"),

	// 축제장을 보는 자리. 이 막 동안은 발사를 멈추고 엔딩곡을 튼다.
	SiteTour     UMETA(DisplayName = "축제장 순회"),

	// 마무리를 보는 자리. 하나도 없으면 첫 불꽃 관람 자리로 돌아온다.
	GrandFinale  UMETA(DisplayName = "마무리")
};

/* 한 차례의 자리를 정할 때 필요한 바깥 사정.

   패턴이 월드를 직접 뒤지지 않게 값으로 받는다. 자리 계산을 순수하게 두면
   월드 없이 테스트할 수 있고, 무엇보다 중심을 바깥에서 한 번만 정하게 강제된다.
   발마다 중심을 다시 뽑으면 흩뿌림 반경만큼 모양이 뭉개진다.
*/
struct FFF_FinaleLayoutContext
{
	// 이 차례가 그려질 중심. 차례당 한 번만 뽑는다.
	FVector Center = FVector::ZeroVector;

	/* 모양이 마주볼 방향(중심에서 관객 쪽으로). 수직면일 때만 쓴다.

	   무작위로 두면 모양이 옆을 보고 선으로 보인다. 바닥면은 이 값을 무시한다.
	*/
	FVector FacingDirection = FVector::ForwardVector;

	/* 바닥면 모양을 통째로 돌리는 각도. 수직면은 무시한다.

	   매번 같은 방향으로 일렬이 지나가면 두 번째부터는 눈에 익어 연출로 안 보인다.
	*/
	float GroundYawDegrees = 0.f;

	// 실제로 쏠 발수. 남은 발수 때문에 Volley 의 ShellCount 보다 적을 수 있다.
	int32 ShellCount = 1;
};

// 발사 패턴 하나.
USTRUCT(BlueprintType)
struct FFF_FinaleVolley
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale")
	EFF_FinaleVolleyPattern Pattern = EFF_FinaleVolleyPattern::Single;

	// 이 패턴이 쓰는 발수. 상한을 두지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale", meta = (ClampMin = "1"))
	int32 ShellCount = 1;

	/* 발 사이의 시간 간격(초). 0 이면 전부 같은 순간에 터진다.

	   일렬 패턴에서 이 값이 0 이면 그냥 한 줄로 늘어선 동시 폭발이 된다.
	   흐르는 느낌을 내려면 0.05 ~ 0.15 정도가 필요하다.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale", meta = (ClampMin = "0.0"))
	float ShellDelaySeconds = 0.f;

	/* 패턴이 차지하는 크기(cm).

	   일렬이면 길이, 부챗살이면 폭, 원이면 지름이다.
	   등급 스펙의 SpreadRadius 와 별개다. 저쪽은 흩뿌리는 범위고 이건 모양의 크기다.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale", meta = (ClampMin = "0.0"))
	float ExtentCm = 6000.f;

	// 이 패턴 안에서 색을 하나로 맞출지.
	//
	// 모양이 있는 패턴은 색이 섞이면 모양이 안 보인다. 단발은 섞이는 편이 화려하다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale")
	bool bUniformColor = false;

	// 어느 면에 그릴지. 자동이면 패턴에 맞춰 고른다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale")
	EFF_FinaleShapePlane Plane = EFF_FinaleShapePlane::Auto;

	/* Shape 패턴이 쓸 모양. 다른 패턴은 무시한다.

	   비어 있으면 이 차례는 아무것도 안 쏜다. 한 점에 몰아서 터뜨리지 않는 이유는,
	   그러면 "모양을 안 넣었다" 가 "이상한 연출" 로 보여서 원인을 찾기 어려워지기
	   때문이다. 로그로 알린다.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale",
		meta = (EditCondition = "Pattern == EFF_FinaleVolleyPattern::Shape"))
	TSoftObjectPtr<class UFF_FinaleShapeData> Shape;

	// 이 패턴이 실제로 쓸 면. Auto 를 풀어준다.
	EFF_FinaleShapePlane ResolvePlane() const
	{
		if (Plane != EFF_FinaleShapePlane::Auto)
		{
			return Plane;
		}

		// 읽혀야 하는 것만 세운다. 기하 패턴은 눕혀도 실루엣이 남는다.
		return (Pattern == EFF_FinaleVolleyPattern::Shape)
			? EFF_FinaleShapePlane::Vertical
			: EFF_FinaleShapePlane::Ground;
	}

	// 뽑기 가중치. 클수록 자주 나온다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale", meta = (ClampMin = "0.01"))
	float Weight = 1.f;

	// 이 패턴이 끝나기까지 걸리는 시간.
	float GetSpanSeconds() const
	{
		return FMath::Max(0.f, ShellDelaySeconds) * FMath::Max(0, ShellCount - 1);
	}

	/* 이 차례에 쏠 발들의 월드 좌표를 한 번에 정한다.

	   발 하나씩 묻지 않고 차례 단위로 받는 이유는, 모양이 발 하나의 성질이 아니라
	   발들 사이의 관계이기 때문이다. 한 발씩 물으면 호출부가 중심과 방향을 매번
	   넘겨야 하고, 그러다 발마다 다른 중심을 넘기면 모양이 조용히 뭉개진다.

	   높이는 Context.Center 가 이미 갖고 있다. 여기서 또 정하면 등급 스펙의
	   높이 범위와 두 곳이 같은 값을 다투게 된다.

	   자리를 정하지 않는 패턴(단발·동시)은 아무것도 채우지 않는다. 호출부가
	   흩뿌리는 규칙을 그대로 쓰면 된다 — HasFixedLayout() 으로 미리 가른다.
	*/
	FESTAFIESTA_API void BuildShellLayout(
		const FFF_FinaleLayoutContext& Context, TArray<FVector>& OutLocations) const;

	// 자리를 패턴이 정하는가. 아니면 호출부가 흩뿌린다.
	bool HasFixedLayout() const
	{
		return Pattern != EFF_FinaleVolleyPattern::Single
			&& Pattern != EFF_FinaleVolleyPattern::Burst;
	}
};

USTRUCT(BlueprintType)
struct FFF_FinaleGradeSpec
{
	GENERATED_BODY()

	// 연출이 끝날 때까지 쏘는 총 발수.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale", meta = (ClampMin = "0"))
	int32 BurstCount = 12;

	// 연출 전체 길이(초). 발수를 이 시간에 나눠 쏜다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale", meta = (ClampMin = "0.5"))
	float DurationSeconds = 20.f;

	// 한 번에 동시에 터지는 수. 늘리면 하늘이 꽉 찬다.
	/* 상한을 두지 않는다.

	   화면을 보면서 계속 올리게 될 값이라, 코드에 최대치를 박아두면 늘리고 싶을 때
	   그 숫자부터 찾아야 한다. 대신 지나치게 많으면 로그로 알린다.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale", meta = (ClampMin = "1"))
	int32 SimultaneousShells = 1;

	/* 카메라가 돌 시점의 최대 개수.

	   1 이면 한 자리에 고정한다. 2 면 한 번 옮긴다. 0 이면 레벨에 놓인 전부를 돈다.

	   등급 차이를 숫자가 아니라 연출로 벌리려는 값이다. 발수가 두 배인 것보다
	   카메라가 움직이는 쪽이 훨씬 크게 느껴지고, 그래야 예산을 아껴서라도
	   올려보려는 이유가 생긴다.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale", meta = (ClampMin = "0"))
	int32 TourViewpointLimit = 1;

	/* 불꽃 관람 → 축제장 순회 → 마무리 3막으로 진행할지.

	   레벨에 불꽃 관람 시점과 축제장 순회 시점이 하나 이상씩 있어야 막이 선다.
	   하나라도 없으면 예전처럼 하늘 시점만 돈다. 3막일 때는 TourViewpointLimit 를 보지 않는다.

	   최고 등급에만 켠다. 순회 동안 발사를 멈추므로 발수가 적은 등급에서 켜면
	   하늘이 비어 보인다. 기본값을 false 로 둔 이유도 같다. 데이터 애셋은 기본값과
	   다른 값만 저장하므로, 기본값이 true 면 이미 만든 애셋의 모든 등급이 켜진다.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale|3막")
	bool bSiteTour = false;

	// 마무리 막 몫으로 남겨둘 발수 비율(0~1).
	// 1막에서 다 써버리면 돌아와서 볼 게 없다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale|3막",
		meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bSiteTour"))
	float GrandFinaleShare = 0.5f;

	// 마무리 막의 발사 간격 배수. 0.5 면 두 배 빠르게 쏜다.
	// 끝으로 갈수록 몰아쳐야 끝난다는 느낌이 난다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale|3막",
		meta = (ClampMin = "0.1", ClampMax = "1.0", EditCondition = "bSiteTour"))
	float GrandFinaleIntervalScale = 0.5f;

	/* 마지막에 모든 발사대에서 한꺼번에 올릴 발수. 0 이면 일제 발사 없이 끝난다.

	   남은 발수가 이 값 이하로 내려오면 나머지를 한 번에 쏜다. 마지막이 한 발씩
	   잦아들면 플레이어는 끝났는지 몰라 계속 기다린다.
	   마무리 몫보다 크게 잡으면 마무리 막에 들어서자마자 전부 터진다.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale|3막",
		meta = (ClampMin = "0", EditCondition = "bSiteTour"))
	int32 FinalSalvoShells = 0;

	/* 이 등급에서 쓸 발사 패턴.

	   비어 있으면 단발만 쏜다. 여러 개 넣으면 차례마다 무작위로 고른다.
	   등급을 가르는 축이 발수가 아니라 패턴이다. 같은 발수여도 일렬로 날아가는
	   것과 흩어지는 것은 전혀 다르게 보인다.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale")
	TArray<FFF_FinaleVolley> Volleys;

	// 발사 지점을 중심으로 이 반경 안에 흩어서 쏜다. 0 이면 한 점에서만 터진다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale", meta = (ClampMin = "0.0"))
	float SpreadRadius = 3000.f;

	// 터지는 높이 범위. 등급이 높을수록 위아래로 넓게 퍼뜨린다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale", meta = (ClampMin = "0.0"))
	float BurstHeightMin = 15000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale", meta = (ClampMin = "0.0"))
	float BurstHeightMax = 25000.f;

	// 폭발 크기 배수. 등급이 높을수록 크게 터진다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale", meta = (ClampMin = "0.1"))
	float BurstScale = 1.f;

	/* 높이 터지는 발일수록 크게 터뜨리는 폭. 0 이면 모든 발이 같은 크기다.

	   실제 불꽃은 알(구경)이 클수록 높이 올라가서 크게 터진다. 3인치 약 120m, 6인치 약 210m,
	   12인치 약 350m (Skylighter 표). 높이만 무작위로 두고 크기를 고정하면 높은 발이
	   오히려 작아 보여 원근이 뒤집힌다. 0.25 면 가장 낮은 발 0.75배, 가장 높은 발 1.25배다.
	   모양 패턴은 크기가 섞이면 모양이 안 읽혀서 이 값을 쓰지 않는다.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float CaliberScaleSpread = 0.25f;

	// 올라가는 궤적의 길이 배수. 크기와 따로 두면 "작지만 높이 올라가는" 연출도 만들 수 있다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale", meta = (ClampMin = "0.1"))
	float TrailLength = 1.f;

	// 이 등급에서 쓸 색. 비어 있으면 이펙트 자체 색을 그대로 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale")
	TArray<FLinearColor> Colors;

	/* 터질 때 재생할 이펙트들. 발마다 무작위로 고른다.

	   하나만 넣으면 예전과 같이 돈다. 여러 개 넣으면 섞여서 터진다.
	   등급이 올라갈수록 종류를 늘리는 게 B 와 C 를 가르는 축이다.
	   발수만 다르면 "많이 터지네" 로 끝나는데, 모양이 섞이면 눈이 계속 간다.

	   비어 있어도 나머지 시스템은 돈다. 에셋이 아직 없는 단계에서 다른 파트
	   작업이 막히면 안 되기 때문이다.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale")
	TArray<TSoftObjectPtr<UNiagaraSystem>> BurstEffects;

	/* 이번 발에 쓸 이펙트를 고른다. 비어 있으면 nullptr.

	   Seed 를 받는 이유는 한 패턴 안에서 같은 이펙트로 맞추고 싶을 때가 있어서다.
	   모양이 있는 패턴은 종류가 섞이면 모양이 안 보인다.

	   빈 칸은 건너뛴다. 배열에 None 이 섞여 있으면 그 칸이 뽑힐 때마다 이펙트 없이
	   신호만 나가서, "지정했는데 가끔 안 터진다" 로 보인다.
	   Seed 를 부호 없는 수로 받는 이유는 Abs(INT_MIN) 이 음수로 남기 때문이다.

	   구현을 헤더에 두지 않는 이유는 로드에 UNiagaraSystem 의 정의가 필요한데,
	   그것 때문에 이 헤더를 포함하는 모든 곳이 Niagara 를 같이 끌고 오게 되기 때문이다.
	*/
	FESTAFIESTA_API UNiagaraSystem* PickEffect(uint32 Seed) const;

	// 비어 있지 않은 이펙트 칸 수. 진단용이다.
	int32 CountAssignedEffects() const
	{
		int32 Count = 0;
		for (const TSoftObjectPtr<UNiagaraSystem>& Effect : BurstEffects)
		{
			if (!Effect.IsNull())
			{
				++Count;
			}
		}
		return Count;
	}

	bool HasAnyEffect() const { return CountAssignedEffects() > 0; }

	// 발사 간격. 총 발수와 지속 시간에서 계산한다.
	// 발수가 0 이거나 하나뿐이면 간격이 의미 없으므로 전체 길이를 돌려준다.
	float GetIntervalSeconds() const
	{
		// Volleys 는 패턴 배열 이름이라 지역 변수에 쓰면 가려진다.
		const int32 VolleyCount = FMath::Max(1, GetVolleyCount());
		if (VolleyCount <= 1)
		{
			return FMath::Max(0.5f, DurationSeconds);
		}
		return FMath::Max(0.05f, DurationSeconds / static_cast<float>(VolleyCount));
	}

	/* 한 차례에 평균 몇 발이 나가는가.

	   실제 발수는 max(SimultaneousShells, 패턴의 ShellCount) 라서 동시 발사 수만으로
	   나누면 틀린다. 예전에 그렇게 나눴더니 S 등급은 차례당 6발로 잡혀 100차례가
	   됐는데, 실제로는 평균 8발 남짓이라 70차례쯤에 발이 동났다. 남은 30차례는
	   빈 하늘로 흘렀다 — 연출의 마지막 3분의 1이 정적이었다.

	   가중치로 기댓값을 낸다. PickVolley 가 뽑는 확률과 같은 식이다.
	*/
	float GetExpectedShellsPerVolley() const
	{
		const int32 Base = FMath::Max(1, SimultaneousShells);
		if (Volleys.Num() == 0)
		{
			return static_cast<float>(Base);
		}

		float TotalWeight = 0.f;
		float WeightedShells = 0.f;
		for (const FFF_FinaleVolley& Volley : Volleys)
		{
			const float Weight = FMath::Max(0.01f, Volley.Weight);
			TotalWeight += Weight;
			WeightedShells += Weight * static_cast<float>(FMath::Max(Base, Volley.ShellCount));
		}

		return TotalWeight > 0.f ? WeightedShells / TotalWeight : static_cast<float>(Base);
	}

	/* 몇 번에 나눠 쏘는가. 간격을 정하는 데만 쓴다.

	   기댓값이라 실제 차례 수와 조금 어긋난다. 그래서 연출이 언제 끝나는지는 이
	   값이 아니라 남은 발수로 정한다(UFF_FinaleShowSubsystem::Tick).
	*/
	int32 GetVolleyCount() const
	{
		const float PerVolley = FMath::Max(1.f, GetExpectedShellsPerVolley());
		return FMath::CeilToInt(static_cast<float>(FMath::Max(0, BurstCount)) / PerVolley);
	}

	bool IsPlayable() const
	{
		return BurstCount > 0 && DurationSeconds > 0.f;
	}

	// 발사 지점 위 높이로 크기 배수를 정한다. 높이 범위 아래 끝 1-폭, 위 끝 1+폭.
	float ResolveCaliberScale(const float HeightAboveLaunchCm) const
	{
		const float Spread = FMath::Clamp(CaliberScaleSpread, 0.f, 0.9f);
		const float Low = FMath::Min(BurstHeightMin, BurstHeightMax);
		const float High = FMath::Max(BurstHeightMin, BurstHeightMax);
		if (Spread <= 0.f || High - Low <= KINDA_SMALL_NUMBER)
		{
			return 1.f;
		}

		const float Alpha = FMath::Clamp((HeightAboveLaunchCm - Low) / (High - Low), 0.f, 1.f);
		return FMath::Lerp(1.f - Spread, 1.f + Spread, Alpha);
	}

	// 3막일 때 마무리 막 몫으로 남겨둘 발수. 3막이 아니면 0 이다.
	int32 GetGrandFinaleReserve() const
	{
		if (!bSiteTour || BurstCount <= 0)
		{
			return 0;
		}

		const float Share = FMath::Clamp(GrandFinaleShare, 0.f, 1.f);
		return FMath::Clamp(FMath::CeilToInt(static_cast<float>(BurstCount) * Share), 0, BurstCount);
	}
};

// Niagara 이펙트에 넘기는 User 파라미터 이름.
//
// 이펙트 쪽에서 같은 이름으로 User 파라미터를 만들어두면 값이 자동으로 들어간다.
// 이름이 다르면 조용히 무시된다 — 크래시도 경고도 없으므로 오타를 조심해야 한다.
// 그래서 문자열을 여기 한 곳에만 두고 코드가 직접 쓰지 않게 한다.
namespace FFF_FinaleParams
{
	inline const FName BurstColor(TEXT("BurstColor"));
	inline const FName BurstScale(TEXT("BurstScale"));
	inline const FName TrailLength(TEXT("TrailLength"));
}

// 낮은 등급부터 높은 등급 순. 등급을 추가하면 여기만 고치면 된다.
//
// 예전에는 이 배열을 쓰는 곳마다 따로 나열했다. 등급이 하나 늘면 세 군데를 고쳐야 하고,
// 한 곳을 빠뜨려도 컴파일은 통과하므로 조용히 어긋난다.
namespace FFF_FinaleGrades
{
	inline const TArray<EFF_FinaleGrade>& GetAscendingOrder()
	{
		static const TArray<EFF_FinaleGrade> Order = {
			EFF_FinaleGrade::None,
			EFF_FinaleGrade::C,
			EFF_FinaleGrade::B,
			EFF_FinaleGrade::A,
			EFF_FinaleGrade::S
		};
		return Order;
	}
}

// 등급별 연출 수치를 담는 데이터 애셋.
//
// 지정하지 않아도 동작한다. 없으면 코드에 박힌 기본값을 쓴다.
// 레벨과 에셋이 아직 없는 단계라, 아무것도 안 붙어 있어도 굴러가는 게 중요하다.
UCLASS(BlueprintType)
class FESTAFIESTA_API UFF_FinaleShowData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finale")
	FFF_FinaleGradeSpec NoneGrade;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finale")
	FFF_FinaleGradeSpec GradeC;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finale")
	FFF_FinaleGradeSpec GradeB;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finale")
	FFF_FinaleGradeSpec GradeA;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finale")
	FFF_FinaleGradeSpec GradeS;

	const FFF_FinaleGradeSpec& GetSpec(EFF_FinaleGrade Grade) const;
};

// 데이터 애셋이 없을 때 쓰는 기본 수치.
//
// 미구매 등급은 불꽃을 쏘지 않는다(BurstCount 0). 예산을 못 모은 결과를 빈 하늘로
// 보여준다. 연출 쪽은 이 등급을 재생 불가로 보고 시작하지 않는다.
namespace FFF_FinaleDefaults
{
	FESTAFIESTA_API FFF_FinaleGradeSpec MakeSpec(EFF_FinaleGrade Grade);

	// 등급별 기본 패턴. 데이터 애셋이 있으면 그쪽이 이긴다.
	FESTAFIESTA_API TArray<FFF_FinaleVolley> MakeVolleys(EFF_FinaleGrade Grade);
}
