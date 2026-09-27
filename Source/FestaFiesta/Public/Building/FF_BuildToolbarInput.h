// 역할: 더 이상 사용하지 않는 건축 단축키를 식별하여 예전 입력이 도구를 실행하지 않게 한다.
#pragma once

#include "InputCoreTypes.h"

namespace FFBuildToolbarInput
{
// F1~F7은 의도적으로 비활성화한 옛 단축키다. 현재 도구 버튼은 명령을 직접 호출하며,
// 이 키가 들어왔다고 버튼을 누른 것으로 바꾸어 처리하면 안 된다.
inline bool IsRetiredShortcut(const FKey& Key)
{
	return Key == EKeys::F1 || Key == EKeys::F2 || Key == EKeys::F3 ||
		Key == EKeys::F4 || Key == EKeys::F5 || Key == EKeys::F6 || Key == EKeys::F7;
}
}
