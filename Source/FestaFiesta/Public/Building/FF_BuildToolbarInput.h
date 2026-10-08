// 역할: 더 이상 사용하지 않는 건축 단축키를 식별하여 예전 입력이 도구를 실행하지 않게 한다.
#pragma once

#include "InputCoreTypes.h"

namespace FFBuildToolbarInput
{
// F1~F4, F6~F7은 비활성화한 옛 건축 단축키다. F5는 전역 일시정지 입력으로 전달한다.
// 현재 도구 버튼은 명령을 직접 호출하며, 함수 키로 옛 도구 동작을 실행하지 않는다.
inline bool IsRetiredShortcut(const FKey& Key)
{
	return Key == EKeys::F1 || Key == EKeys::F2 || Key == EKeys::F3 ||
		Key == EKeys::F4 || Key == EKeys::F6 || Key == EKeys::F7;
}
}
