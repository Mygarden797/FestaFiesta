#include "UI/FacilityCodex/FF_FacilityCodexImage.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void UFF_FacilityCodexImage::ShowThumbnail(TSoftObjectPtr<UTexture2D> Texture, UTextBlock* Fallback)
{
	SetBrushFromTexture(nullptr); // Also cancels the request from the previous recycled row.
	SetVisibility(ESlateVisibility::Hidden);
	if (Fallback) Fallback->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (Texture.IsNull()) return;
	TWeakObjectPtr<UFF_FacilityCodexImage> WeakImage(this);
	TWeakObjectPtr<UTextBlock> WeakFallback(Fallback);
	RequestAsyncLoad(Texture, [WeakImage, WeakFallback, Texture]()
	{
		UFF_FacilityCodexImage* Image = WeakImage.Get();
		UTexture2D* Loaded = Texture.Get();
		// Missing art is a supported content state. The authored placeholder remains visible.
		if (!Image || !Loaded) return;
		Image->SetBrushFromTexture(Loaded, true);
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UTextBlock* Label = WeakFallback.Get()) Label->SetVisibility(ESlateVisibility::Collapsed);
	});
}
