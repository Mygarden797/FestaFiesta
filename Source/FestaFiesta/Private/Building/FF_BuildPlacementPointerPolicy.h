#pragma once

class FWidgetPath;

namespace FFBuildPlacementPointer
{
	// Classifies only the current hit path. The caller owns viewport bounds and focus checks.
	bool IsBlockedByUiPath(const FWidgetPath& Path);
}
