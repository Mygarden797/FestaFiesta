// FF_BuildFacilityCard.h

// 건설 카탈로그의 시설 하나를 표현하는 클릭 가능한 UMG 카드와 데이터 바인딩 계약을 선언한다.
// 흐름: 항목 스냅샷 설정 -> 텍스트·비용·이미지·툴팁 반영 -> 클릭 시 정의 ID 조회.
// 불변식: 시각 계층은 WBP가 소유하고 네이티브 카드는 전달받은 항목 복사본만 표시한다.
// 실패/성능: 썸네일은 동기 로드하지 않으며 미로드·누락 상태에서는 자리표시자를 유지한다.

#pragma once

#include "CoreMinimal.h"
#include "Building/FF_BuildCatalogTypes.h"
#include "UI/FF_CommonButtonBase.h"
#include "FF_BuildFacilityCard.generated.h"

class UCommonTextBlock;
class UBorder;
class UFF_BuildFacilityCostTooltip;
class UImage;
class UTextBlock;
class UTexture2D;




UCLASS(Blueprintable, meta = (DisplayName = "FF Build Facility Card"))
class FESTAFIESTA_API UFF_BuildFacilityCard : public UFF_CommonButtonBase
{
	GENERATED_BODY()

public:
	UFF_BuildFacilityCard();

	// 항목을 복사해 보관한 뒤 제목/정의 ID/썸네일을 현재 생성된 자식 위젯에 즉시 반영한다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|UI")
	void Configure(const FFF_BuildCatalogEntry& InEntry);

	// Asset Bundle 프리로드 완료 뒤 현재 카드의 이미지 브러시만 다시 해석합니다.
	// soft reference를 동기 로드하지 않고 이미 메모리에 있는 텍스처만 사용합니다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|UI")
	void RefreshThumbnail();

	/** 선택한 기존 시설과 조합할 수 있는 설치 후보입니다. 실제 수익 적용 여부와는 별개입니다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|UI|Synergy")
	void SetSynergyHighlighted(bool bHighlighted);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|UI|Synergy")
	bool IsSynergyHighlighted() const { return bSynergyHighlighted; }

	/** 필수 설치 조건의 읽기 전용 스냅샷입니다. 배지 스타일은 WBP가 소유합니다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|UI|Required")
	void SetRequiredFacilityStatus(bool bRequired, bool bSatisfied, int32 CurrentCount, int32 MinimumCount);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|UI|Required")
	bool IsRequiredFacility() const { return bRequiredFacility; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|UI|Required")
	bool IsRequiredFacilitySatisfied() const { return bRequiredFacilitySatisfied; }

	static FText MakeRequiredFacilityToolTip(bool bSatisfied, int32 CurrentCount, int32 MinimumCount);
	static FText MakeProgramFacilityToolTip(const FFF_BuildCatalogEntry& CatalogEntry);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|UI|Program")
	bool HasCompatiblePrograms() const { return Entry.CompatibleProgramCount > 0; }

	// 요청 이미지가 이미 로드돼 있으면 우선 사용하고, 아니면 이미 로드된 공용 대체 이미지를 반환합니다.
	static UTexture2D* ResolveLoadedThumbnail(
		const TSoftObjectPtr<UTexture2D>& RequestedThumbnail,
		const TSoftObjectPtr<UTexture2D>& FallbackThumbnail);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|UI")
	FName GetFacilityDefinitionId() const { return Entry.FacilityDefinitionId; }

	// UMG와 Slate 폴백이 공유하는 지역화된 hover 가격 문구입니다.
	static FText MakeFacilityCostToolTip(const FFF_BuildCatalogEntry& CatalogEntry);

	// Data Asset 상한과 화면 투영 시점의 현재 설치 수를 카드·Slate 폴백에서 같은 형식으로 표시합니다.
	static FText MakeFacilityBuildLimitText(const FFF_BuildCatalogEntry& CatalogEntry);

	const FFF_BuildCatalogEntry& GetCatalogEntry() const { return Entry; }

protected:
	// 디자이너 미리보기와 런타임 생성 순서 모두에서 저장된 데이터를 다시 투영한다.
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** 테두리·배지 외에 디자이너가 추가할 상태 표현입니다. 상태가 바뀔 때만 호출됩니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|Building|UI|Synergy")
	void BP_OnSynergyHighlightChanged(bool bHighlighted);

	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|Building|UI|Required")
	void BP_OnRequiredFacilityStatusChanged(bool bRequired, bool bSatisfied, int32 CurrentCount, int32 MinimumCount);

private:
	// Configure 호출 시점과 WidgetTree 생성 시점이 달라도 같은 결과가 되도록 바인딩을 한곳에 모은다.
	void ApplyEntryToWidgets();
	void ApplyThumbnailToWidget();
	void RefreshFacilityCostTooltip();
	void ApplyFacilityCostTextFallback();
	void ApplySynergyHighlight();
	void ApplyRequiredFacilityStatus();
	void ApplyProgramAvailability();
	bool bSynergyHighlighted = false;
	bool bRequiredFacility = false;
	bool bRequiredFacilitySatisfied = false;
	int32 RequiredFacilityCurrentCount = 0;
	int32 RequiredFacilityMinimumCount = 0;

	// 원본 카탈로그 배열 수명과 분리된 카드별 데이터 스냅샷이다.
	UPROPERTY(Transient)
	FFF_BuildCatalogEntry Entry;

	// WBP_FF_BuildFacilityCard가 반드시 같은 이름과 호환 타입으로 제공해야 하는 BindWidget 계약이다.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|UI", meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_DefinitionId;

	// 썸네일 메타데이터가 아직 없는 카탈로그도 허용하기 위한 선택적 WBP 계약이다.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|UI", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> Image_Thumbnail;

	/** 런타임은 표시 여부만 바꿉니다. 색상·두께·배치·문구는 WBP에서 편집합니다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|UI|Synergy", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> Border_SynergyHighlight;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|UI|Synergy", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Text_SynergyBadge;

	/** 부족·충족 각각의 문구/색상/폰트/배치는 WBP가 소유합니다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|UI|Required", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> Border_RequiredMissing;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|UI|Required", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> Border_RequiredSatisfied;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|UI|Required", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Text_RequiredMissing;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|UI|Required", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Text_RequiredSatisfied;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|UI|Program", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> Border_ProgramBadge;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|UI|Program", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Text_ProgramBadge;

	// 시설별 이미지가 아직 준비되지 않았을 때 사용하는 공용 대체 이미지입니다.
	// 카드에서는 로드하지 않으므로 UI 클래스 등 다른 소유자가 이미 메모리에 올린 경우에만 사용합니다.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Building|UI|Style")
	TSoftObjectPtr<UTexture2D> DefaultThumbnail;

	/**
	 * 기존 카드 WBP를 수정하지 않고 연결하는 디자이너 편집형 가격 툴팁입니다.
	 * 클래스가 없거나 생성에 실패하면 지역화된 기본 텍스트 툴팁으로 자동 폴백합니다.
	 */
	// 직렬화 이름은 기존 카드 WBP 호환성을 위해 유지하고 디자이너 표시명만 현재 역할에 맞춥니다.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Building|UI|Tooltip",
		meta = (DisplayName = "Facility Cost Tooltip Class"))
	TSoftClassPtr<UFF_BuildFacilityCostTooltip> InstallCostTooltipClass;

	UPROPERTY(Transient)
	TObjectPtr<UFF_BuildFacilityCostTooltip> FacilityCostTooltip;
};
