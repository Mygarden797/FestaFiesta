#pragma once

#include "InputCoreTypes.h"

namespace FFBuildToolbarInput
{
// These former construction shortcuts are intentionally inert. Toolbar buttons
// call their commands directly; an input event must never simulate a button.
inline bool IsRetiredShortcut(const FKey& Key)
{
	return Key == EKeys::F1 || Key == EKeys::F2 || Key == EKeys::F3 ||
		Key == EKeys::F4 || Key == EKeys::F5 || Key == EKeys::F6 || Key == EKeys::F7;
}
}
