// 파일: FF_MinigameDefinition.h
// 역할: 비소유 Config의 기존 클래스 등록을 해석하기 위한 빈 추상 데이터 타입이다.
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FF_MinigameDefinition.generated.h"

/**
 * Config/DefaultGame.ini:93의 PrimaryAssetTypesToScan이
 * /Script/FestaFiesta.FF_MinigameDefinition을 참조한다. 해당 설정은 다른 작성자의
 * 파일이므로 이번 삭제 범위에서 유지하며, 엔진의 클래스 로딩 ensure 오류를 막기
 * 위해 등록 이름만 남긴다. 미니게임 실행·점수·화면·보상 기능과 데이터 필드는 없다.
 *
 * 담당자가 해당 Primary Asset 등록을 제거하면 이 헤더도 함께 삭제할 수 있다.
 */
UCLASS(Abstract)
class FESTAFIESTA_API UFF_MinigameDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
};
