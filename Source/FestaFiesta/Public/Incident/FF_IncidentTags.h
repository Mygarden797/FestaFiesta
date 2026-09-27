// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "NativeGameplayTags.h"

namespace FFIncidentTags
{
	namespace Crowd
	{
		FESTAFIESTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Congestion);
		FESTAFIESTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Crush);
		FESTAFIESTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(MemberMissing);
	}

	namespace Facility
	{
		FESTAFIESTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Collapse);
		FESTAFIESTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(MinorFault);
	}

	namespace Queue
	{
		FESTAFIESTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Overload);
	}

	namespace Economy
	{
		FESTAFIESTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Bankrupt);
	}

	namespace Weather
	{
		FESTAFIESTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Shower);
	}

	namespace Visitor
	{
		FESTAFIESTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(LostItem);
		FESTAFIESTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Dispute);
	}
}
