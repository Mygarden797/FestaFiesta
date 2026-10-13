// FF_SaveBeforeQuitPolicy.h
// 역할: "게임 저장 후 종료" 의 종료 허용 규칙. 엔진 상태 없이 입력만으로 결과가 정해진다.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Save/FF_SaveBeforeQuit.h"

/**
 * 종료 직전 저장 정책. 판단만 하는 순수 함수라 엔진 없이 자동화 테스트로 고정한다.
 * UHT 가 읽는 헤더와 나눠 둔다(FF_FestivalSettingsPolicy.h 와 같은 방식).
 *
 * 옵션 화면, 일시정지 허브 등 종료 버튼이 여러 곳에 생겨도 같은 규칙을 따르게 하려고
 * 화면 클래스 밖에 둔다.
 */
namespace FF::SaveBeforeQuit
{
	inline EFF_SaveBeforeQuitResult Classify(const bool bSaveWritten, const bool bFestivalInProgress)
	{
		if (bSaveWritten)
		{
			return EFF_SaveBeforeQuitResult::Saved;
		}
		return bFestivalInProgress ? EFF_SaveBeforeQuitResult::Failed : EFF_SaveBeforeQuitResult::NothingToSave;
	}

	inline bool CanQuit(const EFF_SaveBeforeQuitResult Result)
	{
		return Result != EFF_SaveBeforeQuitResult::Failed;
	}
}
