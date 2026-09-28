// FF_FacilityCodexImage.h
// 역할: 도감 시설 이미지를 표시하고 이미지가 없거나 준비 중인 상태를 처리한다.

#pragma once

#include "CoreMinimal.h"
#include "Components/Image.h"
#include "FF_FacilityCodexImage.generated.h"

class UTextBlock;
class UTexture2D;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class EFF_FacilityCodexDiscoveryVisualState : uint8
{
	Color,
	Grayscale,
	Silhouette
};

/** Async thumbnail with a designer-owned placeholder. Pooled rows cancel stale requests. */
UCLASS(meta=(DisplayName="FF Facility Codex Image"))
class FESTAFIESTA_API UFF_FacilityCodexImage final : public UImage
{
	GENERATED_BODY()
public:
	void ShowThumbnail(TSoftObjectPtr<UTexture2D> Texture, UTextBlock* Fallback);
	/** Change state without reloading the source texture or interrupting a pending request. */
	UFUNCTION(BlueprintCallable, Category="Facility Codex")
	void SetDiscoveryVisualState(EFF_FacilityCodexDiscoveryVisualState State);
	EFF_FacilityCodexDiscoveryVisualState GetDiscoveryVisualState() const { return DiscoveryVisualState; }
	UTexture2D* GetLoadedThumbnail() const { return LoadedThumbnail; }

	/** Authored on the WBP image, so the UI shader is included in cooked builds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Facility Codex")
	TObjectPtr<UMaterialInterface> DiscoveryMaterial;
private:
	void ApplyDiscoveryVisualState();
	UPROPERTY(Transient) TObjectPtr<UTexture2D> LoadedThumbnail;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> DiscoveryMID;
	EFF_FacilityCodexDiscoveryVisualState DiscoveryVisualState = EFF_FacilityCodexDiscoveryVisualState::Color;
	uint64 ThumbnailRequestSerial = 0;
};
