// FF_FacilityCodexPresentation.h
// 역할: 도감 카드와 상세 영역이 공유하는 글자·이미지 표시 도구를 제공한다.

#pragma once

#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/FacilityCodex/FF_FacilityCodexImage.h"

namespace FFFacilityCodexPresentation
{
	inline FText Number(float Value)
	{
		FNumberFormattingOptions Format;
		Format.SetMaximumFractionalDigits(1);
		return FText::AsNumber(Value, &Format);
	}

	/** UImage owns the cancellable async request. Never synchronously load per-card thumbnails. */
	inline void Thumbnail(UFF_FacilityCodexImage* Image, UTextBlock* Fallback,
		const TSoftObjectPtr<UTexture2D>& Texture, TSoftObjectPtr<UTexture2D>& Shown)
	{
		if (Image && (Shown != Texture || Texture.IsNull()))
		{
			Image->ShowThumbnail(Texture, Fallback);
		}
		Shown = Texture;
	}
}
