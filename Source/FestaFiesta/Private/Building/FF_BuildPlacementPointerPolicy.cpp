// FF_BuildPlacementPointerPolicy.cpp
// 역할: 실제 조작 UI를 향한 포인터 입력이 바닥 설치로 새지 않게 판정한다.

#include "FF_BuildPlacementPointerPolicy.h"

#include "Building/UI/FF_BuildCatalogScreen.h"
#include "Layout/WidgetPath.h"
#include "Slate/SObjectWidget.h"
#include "UI/PrepHUD/FF_PrepPanelBase.h"

bool FFBuildPlacementPointer::IsBlockedByUiPath(const FWidgetPath& Path)
{
	for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
	{
		const TSharedRef<SWidget>& Widget = Path.Widgets[Index].Widget;
		if (Widget->IsInteractable())
		{
			return true;
		}
		if (Widget->GetType() == TEXT("SObjectWidget"))
		{
			// Prep panels own their draggable empty surface, and standalone catalogs own their
			// empty card area. Neither ownership is advertised by SObjectWidget::IsInteractable().
			const TSharedRef<SObjectWidget> ObjectWidget = StaticCastSharedRef<SObjectWidget>(Widget);
			const UUserWidget* WidgetObject = ObjectWidget->GetWidgetObject();
			if (IsValid(WidgetObject) &&
				(WidgetObject->IsA<UFF_PrepPanelBase>() || WidgetObject->IsA<UFF_BuildCatalogScreen>()))
			{
				return true;
			}
		}
	}
	// Empty paths and passive full-screen HUD wrappers do not veto controller-owned input.
	return false;
}
