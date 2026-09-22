// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FF_FinaleShapeData.generated.h"

/* 불꽃으로 그릴 모양 하나. 스플라인으로 그려서 구운 결과다.

   ─── 왜 점 목록이 아니라 폴리라인인가 ──────────────────────────────────────
   발수를 애셋에 박으면 하트 하나를 B 등급 12발과 S 등급 40발에 같이 못 쓴다.
   선으로 담아두고 쓸 때 발수만큼 다시 찍으면 같은 애셋이 등급마다 늘어난다.

   재샘플을 호 길이 기준으로 하는 이유는, 스플라인 제어점을 그대로 쓰면 곡률이
   큰 곳에 점이 몰려서 하트의 패인 부분만 빽빽해지기 때문이다.

   ─── 왜 정규화해서 담는가 ──────────────────────────────────────────────────
   크기는 FFF_FinaleVolley::ExtentCm 이 이미 갖고 있다. 애셋에 절대 좌표를 담으면
   두 곳이 크기를 다퉈서, 기획이 ExtentCm 을 올려도 안 커지는 일이 생긴다.

   가로세로를 각각 늘려 맞추지 않고 긴 쪽 기준으로 한 번에 나눈다. 축마다 따로
   맞추면 세로로 긴 하트가 납작해진다 — 모양을 담는 애셋이 모양을 바꾸면 안 된다.
*/
UCLASS(BlueprintType)
class FESTAFIESTA_API UFF_FinaleShapeData : public UDataAsset
{
	GENERATED_BODY()

public:
	// 에디터 목록과 로그에 쓸 이름. 애셋 이름과 달라도 된다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finale|Shape")
	FText DisplayName;

	/* 구워진 윤곽선. 긴 쪽이 1 이 되도록 맞추고 가운데를 원점에 둔 좌표다.

	   X 가 가로, Y 가 세로다. 수직면에 세울 때 Y 가 그대로 높이가 된다.

	   손으로 고칠 일은 없지만 열어볼 수는 있어야 해서 EditAnywhere 로 둔다.
	   베이크 도구가 통째로 덮어쓴다.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finale|Shape")
	TArray<FVector2D> Points;

	/* 닫힌 도형인가. 하트나 원은 닫혀 있고, 글자 획은 열려 있다.

	   재샘플할 때 마지막 점과 첫 점 사이를 이을지가 여기서 갈린다. 닫힌 도형을
	   열린 것으로 다루면 그 한 구간만 점이 비어서 모양이 끊어져 보인다.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finale|Shape")
	bool bClosed = true;

	// 그릴 수 있는 모양인가. 점이 둘은 있어야 선이 된다.
	bool IsValidShape() const { return Points.Num() >= 2; }

	/* 윤곽선을 따라 Count 개의 점을 고르게 찍는다.

	   출력은 입력과 같은 정규화 좌표다. 크기와 방향은 호출부가 입힌다.

	   닫힌 도형은 끝점을 다시 찍지 않는다. 첫 점과 겹쳐서 한 자리에 두 발이 터진다.

	   그릴 수 없는 모양이면 아무것도 채우지 않는다. 호출부가 빈 결과를 보고
	   물러설 수 있게 하려는 것이다 — 원점에 몰아서 찍으면 한 점에서 전부 터진다.
	*/
	void SamplePoints(int32 Count, TArray<FVector2D>& OutPoints) const;

	/* 원본 점들을 정규화해 Points 에 담는다. 베이크 도구가 부른다.

	   정규화를 애셋 쪽에 두는 이유는, 저작 도구가 여럿이 되어도(스플라인, CSV,
	   직접 입력) 담기는 형식이 한 곳에서만 정해지게 하려는 것이다.
	*/
	void SetFromRawPoints(const TArray<FVector2D>& RawPoints, bool bInClosed);
};
