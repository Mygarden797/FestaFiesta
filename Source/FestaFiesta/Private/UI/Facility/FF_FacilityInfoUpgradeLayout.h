#pragma once

#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"

/** Shared by editor migration and the pre-Slate runtime compatibility path. */
namespace FFFacilityInfoUpgradeLayout
{
	/** Revenue remains outside the optional upgrade section, including in legacy designer layouts. */
	inline UVerticalBox* FindRevenueParent(UWidgetTree& Tree)
	{
		if (UVerticalBox* Meta = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Summary_Meta"))))
		{
			return Meta;
		}
		for (UWidget* Widget = Tree.FindWidget(TEXT("Text_FacilityState")); Widget; Widget = Widget->GetParent())
		{
			if (UVerticalBox* Parent = Cast<UVerticalBox>(Widget->GetParent()))
			{
				return Parent;
			}
		}
		return nullptr;
	}

	inline UWidget* FindContentChild(UWidget* Widget, const UVerticalBox& Content)
	{
		while (Widget && Widget->GetParent() != &Content)
		{
			Widget = Widget->GetParent();
		}
		return Widget;
	}

	/** Only the upgrade section moves; existing sibling nodes and their slots keep their properties. */
	inline bool PlaceAfterSummary(UWidgetTree& Tree, UWidget& UpgradeSection, bool& bOutModified)
	{
		UVerticalBox* Content = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Panel_Content")));
		if (!Content)
		{
			return false;
		}
		UWidget* Summary = FindContentChild(Tree.FindWidget(TEXT("Summary_Card")), *Content);
		if (!Summary)
		{
			Summary = FindContentChild(Tree.FindWidget(TEXT("Image_Thumbnail")), *Content);
		}
		if (!Summary || Summary == &UpgradeSection)
		{
			return false;
		}
		const int32 CurrentIndex = Content->GetChildIndex(&UpgradeSection);
		int32 TargetIndex = Content->GetChildIndex(Summary) + 1;
		if (CurrentIndex != INDEX_NONE && CurrentIndex < TargetIndex)
		{
			--TargetIndex;
		}
		if (CurrentIndex == TargetIndex)
		{
			return true;
		}
		if (CurrentIndex != INDEX_NONE)
		{
			Content->ShiftChild(TargetIndex, &UpgradeSection);
		}
		else if (!Content->InsertChildAt(TargetIndex, &UpgradeSection, UpgradeSection.Slot))
		{
			return false;
		}
		bOutModified = true;
		return true;
	}
}
