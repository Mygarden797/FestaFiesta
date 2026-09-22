// FF_FacilityCodexImage.cpp
// 역할: 도감 시설 이미지를 표시하고 이미지가 없거나 준비 중인 상태를 처리한다.

#include "UI/FacilityCodex/FF_FacilityCodexImage.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

void UFF_FacilityCodexImage::ShowThumbnail(TSoftObjectPtr<UTexture2D> Texture, UTextBlock* Fallback)
{
	const uint64 RequestSerial = ++ThumbnailRequestSerial;
	SetBrushFromTexture(nullptr); // Also cancels the request from the previous recycled row.
	LoadedThumbnail = nullptr;
	DiscoveryMID = nullptr;
	SetVisibility(ESlateVisibility::Hidden);
	if (Fallback) Fallback->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (Texture.IsNull()) return;
	TWeakObjectPtr<UFF_FacilityCodexImage> WeakImage(this);
	TWeakObjectPtr<UTextBlock> WeakFallback(Fallback);
	RequestAsyncLoad(Texture, [WeakImage, WeakFallback, Texture, RequestSerial]()
	{
		UFF_FacilityCodexImage* Image = WeakImage.Get();
		UTexture2D* Loaded = Texture.Get();
		// Missing art is a supported content state. The authored placeholder remains visible.
		if (!Image || Image->ThumbnailRequestSerial != RequestSerial || !Loaded) return;
		Image->LoadedThumbnail = Loaded;
		Image->ApplyDiscoveryVisualState();
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UTextBlock* Label = WeakFallback.Get()) Label->SetVisibility(ESlateVisibility::Collapsed);
	});
}

void UFF_FacilityCodexImage::SetDiscoveryVisualState(EFF_FacilityCodexDiscoveryVisualState State)
{
	DiscoveryVisualState = State;
	// A pending load reads the latest state when it completes. Do not replace its brush here,
	// because UImage brush setters cancel the stream owned by this pooled image.
	if (LoadedThumbnail) ApplyDiscoveryVisualState();
}

void UFF_FacilityCodexImage::ApplyDiscoveryVisualState()
{
	if (!LoadedThumbnail) return;
	SetBrushFromTexture(LoadedThumbnail, true);
	if (DiscoveryVisualState == EFF_FacilityCodexDiscoveryVisualState::Color) return;
	if (!DiscoveryMaterial)
	{
		// Older authored widgets can still show the state. Saved WBP references are the
		// primary loading/cooking path; this shared fallback never streams art per card.
		static TWeakObjectPtr<UMaterialInterface> SharedMaterial;
		if (!SharedMaterial.IsValid()) SharedMaterial = LoadObject<UMaterialInterface>(nullptr, FFBuildUIAssetPaths::M_FF_Codex_DiscoveryThumbnailObject);
		DiscoveryMaterial = SharedMaterial.Get();
	}
	if (!DiscoveryMaterial) return; // Missing shader keeps useful source art instead of a blank card.
	if (!DiscoveryMID || DiscoveryMID->Parent != DiscoveryMaterial)
		DiscoveryMID = UMaterialInstanceDynamic::Create(DiscoveryMaterial, this);
	if (!DiscoveryMID) return;
	DiscoveryMID->SetTextureParameterValue(TEXT("ThumbnailTexture"), LoadedThumbnail);
	DiscoveryMID->SetScalarParameterValue(TEXT("VisualState"), static_cast<float>(DiscoveryVisualState));
	DiscoveryMID->SetVectorParameterValue(TEXT("TexelSize"), FLinearColor(
		1.f / FMath::Max(LoadedThumbnail->GetSizeX(), 1), 1.f / FMath::Max(LoadedThumbnail->GetSizeY(), 1), 0.f, 0.f));
	// SetBrushFromMaterial retains the source image dimensions for authored ScaleBoxes.
	SetBrushFromMaterial(DiscoveryMID);
}
