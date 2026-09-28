// FF_FacilitySynergyEntryWidget.h
// 역할: 시설 정보의 조합 효과 한 행과 상대 시설로 이동하는 버튼을 표시한다.

#pragma once

#include "Blueprint/UserWidget.h"
#include "UI/Facility/FF_FacilityInfoTypes.h"
#include "FF_FacilitySynergyEntryWidget.generated.h"

class UTextBlock;
class UButton;

DECLARE_MULTICAST_DELEGATE_OneParam(FFF_OnSynergyPartnerCatalogRequested, FName);

/** Authored WBP row: the data model supplies names/effects, the Designer owns every visual. */
UCLASS(Blueprintable, meta = (DisplayName = "FF Facility Synergy Entry", DisableNativeTick))
class FESTAFIESTA_API UFF_FacilitySynergyEntryWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    FFF_OnSynergyPartnerCatalogRequested OnPartnerCatalogRequested;

    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Facility Info|Synergy")
    void ApplyEntry(const FFF_FacilityInfoSynergyEntry& Entry);

    UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Synergy")
    FFF_FacilityInfoSynergyEntry CurrentEntry;

protected:
    virtual void NativeOnInitialized() override;

    UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|Facility Info|Synergy")
    void OnPartnerCatalogClicked(FName FacilityDefinitionId);

    UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|Facility Info|Synergy")
    void OnEntryApplied(const FFF_FacilityInfoSynergyEntry& Entry);

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Title;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Partner;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Effect;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Condition;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Status;
    /** Optional for older authored rows; missing button keeps the same read-only text. */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_OpenPartnerCatalog;

private:
    UFUNCTION() void HandlePartnerCatalogClicked();
    bool bHasEntry = false;
};
