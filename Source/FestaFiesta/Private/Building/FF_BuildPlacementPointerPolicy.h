// FF_BuildPlacementPointerPolicy.h
// 역할: 실제 조작 UI를 향한 포인터 입력이 바닥 설치로 새지 않게 판정한다.

#pragma once

class FWidgetPath;

namespace FFBuildPlacementPointer
{
	// Classifies only the current hit path. The caller owns viewport bounds and focus checks.
	bool IsBlockedByUiPath(const FWidgetPath& Path);
}
