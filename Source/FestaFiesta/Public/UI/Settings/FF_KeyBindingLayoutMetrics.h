// FF_KeyBindingLayoutMetrics.h
// 역할: 키 설정 화면에서 공유하는 행 크기와 여백을 정의한다.
// 실행 화면과 에디터 생성기가 같은 치수를 쓰도록 공개 헤더에서 공유한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Shared authored dimensions for the LightTeal keyboard stage and UMG keycaps. */
struct FFF_KeyBindingLayoutMetrics
{
	static constexpr float StageHeight = 320.f;
	static constexpr float KeyUnitWidth = 38.f;
	static constexpr float KeyHeight = 34.f;
	static constexpr float KeyGap = 4.f;
	static constexpr float KeyPitch = KeyUnitWidth + KeyGap;
	static constexpr float RowGap = 4.f;
	static constexpr float FunctionRowGap = 14.f;

	// One shared authored surface keeps the main block, navigation block and number pad aligned.
	static constexpr float KeyboardSurfaceWidth = 964.f;
	static constexpr float KeyboardSurfaceHeight = 234.f;
	static constexpr float NavigationClusterX = 650.f;
	static constexpr float NumberPadClusterX = 800.f;
	static constexpr float FunctionF1X = 70.f;
	static constexpr float FunctionF5X = 250.f;
	static constexpr float FunctionF9X = 430.f;

	/** Keeps the three-glyph Esc label readable after the stage is DPI-scaled down. */
	static constexpr float EscapeWidthUnits = 1.35f;

	/** ANSI width units include the gap that would otherwise sit between adjacent one-unit keys. */
	static constexpr float GetKeyWidth(const float WidthUnits)
	{
		return WidthUnits * KeyPitch - KeyGap;
	}

	static constexpr float GetKeyHeight(const float HeightRows)
	{
		return HeightRows * KeyHeight + (HeightRows - 1.f) * RowGap;
	}

	static constexpr float GetRowY(const int32 RowIndex)
	{
		return RowIndex <= 0
			? 0.f
			: KeyHeight + FunctionRowGap
				+ static_cast<float>(RowIndex - 1) * (KeyHeight + RowGap);
	}
};
