// FF_SaveBeforeQuitPolicy.h
// 역할: "게임 저장 후 종료" 의 종료 허용 규칙. 엔진 상태 없이 입력만으로 결과가 정해진다.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Save/FF_SaveBeforeQuit.h"
#include "Time/FF_FestivalStage.h"

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

	/* 축제 세이브를 쓸 수 있는 단계. 하루 정산이 끝난 그날의 시작(셋업·모닝체크)뿐이다.

	   운영 시간은 저장하지 않아서, 운영 중 세이브를 이어하면 그날이 처음부터 다시 돌고
	   이미 번 돈 위에 하루를 또 번다. 그 밖의 단계에서는 그날 아침 자동 저장이 이어할 자리다.
	*/
	inline bool CanSaveFestivalAt(const EFF_FestivalStage Stage)
	{
		return Stage == EFF_FestivalStage::Setup || Stage == EFF_FestivalStage::MorningCheck;
	}
}
