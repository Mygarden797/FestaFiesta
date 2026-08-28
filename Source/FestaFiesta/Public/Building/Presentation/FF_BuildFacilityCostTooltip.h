// FF_BuildFacilityCostTooltip.h

// 건설 카드의 시설명과 설치·복구 비용을 표시하는 디자이너 편집형 UMG 툴팁을 선언한다.
// 흐름: 카탈로그 항목 스냅샷 설정 -> 표시 문자열 계산 -> 선택적 BindWidget 갱신.
// 불변식: 비용이 해석되지 않은 항목은 숫자 대신 명시적인 미확정 문구를 보여 준다.
// 실패/수명: BindWidget이 아직 없으면 상태만 저장하고 PreConstruct에서 안전하게 다시 적용한다.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Building/FF_BuildCatalogTypes.h"
#include "FF_BuildFacilityCostTooltip.generated.h"

class UCommonTextBlock;

/**
 * 건축 카드의 가격을 표시하는 디자이너 편집형 UMG 툴팁의 네이티브 계약입니다.
 * 배경, 여백, 폰트, 애니메이션은 파생 WBP가 소유하고 C++은 카탈로그 스냅샷만 전달합니다.
 */
UCLASS(Blueprintable, meta = (DisplayName = "FF Build Facility Cost Tooltip"))
class FESTAFIESTA_API UFF_BuildFacilityCostTooltip : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|UI")
	void Configure(const FFF_BuildCatalogEntry& InEntry);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|UI")
	FText GetFacilityNameText() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|UI")
	FText GetInstallCostText() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|UI")
	FText GetDisplayedInstallCostText() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|UI")
	FText GetRecoveryCostText() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|UI")
	FText GetDisplayedRecoveryCostText() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|UI")
	bool HasResolvedInstallCost() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|UI")
	bool HasResolvedRecoveryCost() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|UI")
	int32 GetInstallCost() const { return Entry.InstallCost; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|UI")
	int32 GetRecoveryCost() const { return Entry.RecoveryCost; }

protected:
	virtual void NativePreConstruct() override;

	/** 디자이너가 가격 유무에 따른 색상·장식·애니메이션을 Blueprint에서 확장할 수 있습니다.
	 *  회수비용은 HasResolvedRecoveryCost/GetRecoveryCost로 조회해 기존 Blueprint 이벤트 계약을 보존합니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|Building|UI",
		meta = (DisplayName = "On Tooltip Data Changed"))
	void BP_OnTooltipDataChanged(bool bHasResolvedCost, int32 InstallCost);

private:
	void ApplyEntryToWidgets();

	UPROPERTY(Transient)
	FFF_BuildCatalogEntry Entry;

	bool bConfigured = false;

	/** 파생 WBP가 반드시 같은 이름의 CommonTextBlock을 제공해야 하는 최소 계약입니다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|UI",
		meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> TXT_InstallCost;

	/** 파생 WBP가 건설 비용 아래에 제공해야 하는 회수 비용 표시 계약입니다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|UI",
		meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> TXT_RecoveryCost;

	/** 시설명을 표시하지 않는 간결한 디자인도 허용합니다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|UI",
		meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> TXT_FacilityName;

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Building|UI|Designer")
	FText DesignerPreviewFacilityName = NSLOCTEXT(
		"FFBuildFacilityCostTooltip", "DesignerPreviewFacility", "시설 이름");

};
