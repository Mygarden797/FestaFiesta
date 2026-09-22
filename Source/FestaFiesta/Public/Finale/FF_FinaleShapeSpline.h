// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FF_FinaleShapeSpline.generated.h"

class UFF_FinaleShapeData;
class USplineComponent;

/* 스플라인을 어느 면에 그렸는가. 굽는 쪽이 2차원으로 눌러 담으려면 알아야 한다.

   액터의 지역 좌표 기준이다. 월드 기준이 아닌 이유는, 액터를 돌려놓고 작업해도
   구워진 모양이 달라지면 안 되기 때문이다. 세우고 눕히는 건 런타임이 정한다.
*/
UENUM()
enum class EFF_FinaleShapeAuthorPlane : uint8
{
	/* 서 있는 면(지역 X-Z). 기본값이다.

	   하트처럼 읽혀야 하는 모양은 세워서 그려야 눈으로 확인하며 다듬을 수 있다.
	   바닥에 눕혀 그리면 뷰포트를 위에서 내려다봐야 한다.
	*/
	LocalXZ UMETA(DisplayName = "서 있는 면 (X-Z)"),

	// 누운 면(지역 X-Y). 바닥에 깔 모양을 그릴 때.
	LocalXY UMETA(DisplayName = "누운 면 (X-Y)")
};

/* 시작 모양. 여기서부터 당겨가며 다듬는다.

   맨 점에서 하트를 그리는 것은 스플라인으로 할 일이 아니다. 수식으로 대충 만들어
   놓고 손으로 고치는 편이 훨씬 빠르고, 좌우 대칭도 처음부터 맞아 있다.
*/
UENUM()
enum class EFF_FinaleShapeTemplate : uint8
{
	Heart  UMETA(DisplayName = "하트"),
	Circle UMETA(DisplayName = "원"),
	Star   UMETA(DisplayName = "별")
};

/* 불꽃 모양을 스플라인으로 그려 데이터 애셋으로 굽는 도구.

   ─── 왜 액터인가 ───────────────────────────────────────────────────────────
   하트가 하트로 보이는지는 숫자로 알 수 없고 눈으로 봐야 한다. 레벨에 놓으면
   뷰포트에서 바로 당기고 밀면서 다듬을 수 있고, 다음에 열어 고칠 수도 있다.

   ─── 왜 굽는가 ─────────────────────────────────────────────────────────────
   런타임이 스플라인을 직접 읽게 하면 연출이 레벨에 놓인 액터에 매달린다.
   레벨마다 하트를 하나씩 놓아야 하고, 누가 지우면 조용히 안 나온다.
   구워두면 애셋 하나를 모든 레벨이 참조한다.

   ─── 패키징에 안 딸려간다 ──────────────────────────────────────────────────
   bIsEditorOnlyActor 로 표시한다. 저작 도구가 게임에 들어갈 이유가 없다.
*/
UCLASS()
class FESTAFIESTA_API AFF_FinaleShapeSpline : public AActor
{
	GENERATED_BODY()

public:
	AFF_FinaleShapeSpline();

	// 여기에 모양을 그린다. 닫힌 도형은 스플라인의 Closed Loop 를 켜면 된다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FestaFiesta|Finale")
	TObjectPtr<USplineComponent> Spline;

	// 구운 결과를 받을 애셋. 미리 만들어 두고 여기에 물린다.
	UPROPERTY(EditAnywhere, Category = "FestaFiesta|Finale")
	TSoftObjectPtr<UFF_FinaleShapeData> TargetShape;

	// 스플라인을 그린 면. 액터 지역 좌표 기준이다.
	UPROPERTY(EditAnywhere, Category = "FestaFiesta|Finale")
	EFF_FinaleShapeAuthorPlane AuthorPlane = EFF_FinaleShapeAuthorPlane::LocalXZ;

	// 되돌리기 버튼이 깔아줄 시작 모양.
	UPROPERTY(EditAnywhere, Category = "FestaFiesta|Finale|Template")
	EFF_FinaleShapeTemplate Template = EFF_FinaleShapeTemplate::Heart;

	/* 시작 모양의 크기(cm). 뷰포트에서 보기 좋을 정도면 된다.

	   구울 때 정규화되므로 실제 연출 크기와 무관하다. 그건 볼리의 ExtentCm 이 정한다.
	*/
	UPROPERTY(EditAnywhere, Category = "FestaFiesta|Finale|Template", meta = (ClampMin = "10.0"))
	float TemplateSizeCm = 1000.f;

	/* 곡선을 몇 개의 점으로 눌러 담을지.

	   런타임이 이 점들을 다시 호 길이로 재샘플하므로, 여기서는 곡선 모양이
	   뭉개지지 않을 만큼만 촘촘하면 된다. 발수와는 무관한 값이다.

	   너무 낮으면 하트의 둥근 부분이 각져서 구워지고, 그건 발수를 올려도 안 펴진다.
	*/
	UPROPERTY(EditAnywhere, Category = "FestaFiesta|Finale", meta = (ClampMin = "8", ClampMax = "1024"))
	int32 BakeResolution = 128;

	/* 미리보기로 찍어볼 발수. 굽기를 누르면 이 수만큼 뷰포트에 점을 찍는다.

	   실제로 쏠 발수를 넣어보면 그 등급에서 모양이 읽히는지 바로 보인다.
	   점이 성기면 하트가 아니라 점 무더기로 보이는데, 그건 구운 뒤에야 알 수 있다.
	*/
	UPROPERTY(EditAnywhere, Category = "FestaFiesta|Finale", meta = (ClampMin = "1"))
	int32 PreviewShellCount = 24;

#if WITH_EDITOR
	/* 스플라인을 Template 모양으로 되돌린다. 지금 그린 것은 지워진다.

	   맨 앞에 두는 이유는 이게 첫 단추라서다. 점이 없는 스플라인은 뷰포트에서
	   잡을 것이 없어 점을 추가할 수도 없다.
	*/
	UFUNCTION(CallInEditor, Category = "FestaFiesta|Finale")
	void ResetSplineToTemplate();

	/* 스플라인을 읽어 TargetShape 에 담는다.

	   애셋을 저장까지 하지는 않는다. 더티 표시만 하고 저장은 에디터에 맡긴다 —
	   여기서 SavePackage 를 부르면 이 파일이 에디터 모듈에 의존하게 되고,
	   실수로 구웠을 때 되돌릴 방법도 없어진다.
	*/
	UFUNCTION(CallInEditor, Category = "FestaFiesta|Finale")
	void BakeToAsset();

	// 지금 스플라인을 PreviewShellCount 만큼 찍어본다. 굽지 않는다.
	UFUNCTION(CallInEditor, Category = "FestaFiesta|Finale")
	void PreviewOnly();

	/* 찍어둔 미리보기를 지운다.

	   미리보기는 그 순간의 월드 좌표에 찍는 스냅샷이라 액터를 옮겨도 따라오지 않는다.
	   따라오게 만들지 않는 이유는, 옮기기 전후를 견줘보는 게 쓸모 있을 때가 있어서다.
	   대신 치우는 수단을 둔다.

	   [주의] 월드의 영구 디버그 드로잉을 전부 지운다. 다른 시스템이 찍어둔 것도 같이 사라진다.
	*/
	UFUNCTION(CallInEditor, Category = "FestaFiesta|Finale")
	void ClearPreview();
#endif

private:
	/* 시작 모양의 2차원 좌표를 만든다. 크기는 TemplateSizeCm 를 따른다.

	   생성자와 되돌리기 버튼이 같은 것을 써야 처음 놓았을 때와 되돌렸을 때가
	   같은 모양이 된다.
	*/
	static void BuildTemplatePoints(
		EFF_FinaleShapeTemplate InTemplate, float SizeCm, TArray<FVector2D>& OutPoints);

	// 2차원 좌표를 저작 면에 맞춰 지역 3차원으로 편다.
	FVector PlanarToLocal(const FVector2D& Point) const;

	// 점들을 스플라인에 깐다. 생성자에서도 불리므로 에디터 전용이 아니다.
	void ApplyTemplateToSpline();

	/* 스플라인을 지역 좌표의 2차원 점들로 눌러 담는다.

	   등간격으로 뽑는 이유는, 제어점을 그대로 쓰면 곡률이 큰 곳에만 점이 몰려
	   런타임 재샘플이 그 왜곡을 물려받기 때문이다.
	*/
	void SampleSpline(TArray<FVector2D>& OutPoints) const;

#if WITH_EDITOR
	// 점들이 차지하는 가장 긴 변. 정규화된 미리보기를 원래 크기로 되돌릴 때 쓴다.
	static float MeasureLongestSide(const TArray<FVector2D>& Points);

	/* 구워질 결과를 뷰포트에 찍는다. 그릴 월드가 없으면 아무것도 하지 않는다.

	   크기를 인자로 받는 이유는 스플라인의 Bounds 가 월드 기준이기 때문이다.
	   그것으로 되돌리면 액터를 돌려놓았을 때 회전이 두 번 먹는다.
	*/
	void DrawPreview(
		const TArray<FVector2D>& NormalizedPoints, bool bClosed, float LocalLongestSide) const;
#endif
};
