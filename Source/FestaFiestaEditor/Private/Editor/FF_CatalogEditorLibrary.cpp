#include "Editor/FF_CatalogEditorLibrary.h"

#include "Components/Widget.h"
#include "WidgetBlueprint.h"

int32 UFF_CatalogEditorLibrary::RegisterWidgetGuids(UWidgetBlueprint* Blueprint)
{
	if (Blueprint == nullptr)
	{
		return 0;
	}

	int32 RegisteredCount = 0;
	Blueprint->ForEachSourceWidget([Blueprint, &RegisteredCount](UWidget* Widget)
	{
		if (Widget != nullptr && !Blueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
		{
			if (RegisteredCount == 0)
			{
				Blueprint->Modify();
			}
			Blueprint->OnVariableAdded(Widget->GetFName());
			++RegisteredCount;
		}
	});
	return RegisteredCount;
}
