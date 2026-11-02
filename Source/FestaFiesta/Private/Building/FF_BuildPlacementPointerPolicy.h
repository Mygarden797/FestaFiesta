// FF_BuildPlacementPointerPolicy.h
// 역할: 실제 조작 UI를 향한 포인터 입력이 바닥 설치로 새지 않게 판정한다.

#pragma once

class FWidgetPath;

namespace FFBuildPlacementPointer
{
	// 현재 Slate 히트 경로에 입력을 소유한 UI가 있는지만 판정한다.
	// 뷰포트 밖 포인터와 키보드 포커스 판정은 BuildModeSubsystem에서 먼저 처리한다.
	bool IsBlockedByUiPath(const FWidgetPath& Path);
}
