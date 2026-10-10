#pragma once

#include "CoreMinimal.h"
#include "UI/PrepHUD/Panels/FF_PrepChoiceButton.h"
#include "FF_PrepProgramCatalogChoice.generated.h"

class UBorder;
class UTextBlock;

/** 시설 이름과 바로가기 동작을 분리한 버튼. 강조는 짧게 끝나며 입력·게임 시간을 건드리지 않는다. */
UCLASS(Abstract, Blueprintable)
class FESTAFIESTA_API UFF_PrepProgramCatalogChoice : public UFF_PrepChoiceButton
{
	GENERATED_BODY()
public:
	void ConfigureCatalogChoice(int32 InIndex, const FText& FacilityName);
	void SetInstalledCount(int32 Count);
	void SetAttention(bool bAttention);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;
	virtual void NativePreConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> Border_Attention;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Subtitle;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Chevron;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> Border_InstallationStatus;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_InstallationStatus;

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Program Shortcut")
	FText NotInstalledText = NSLOCTEXT("FestaFiestaPrepProgram", "CatalogNotInstalled", "미설치");
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Program Shortcut")
	FText InstalledTextFormat = NSLOCTEXT("FestaFiestaPrepProgram", "CatalogInstalledCount", "✓ 설치 {0}곳");
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Program Shortcut")
	FLinearColor InstalledBackgroundColor = FLinearColor::FromSRGBColor(FColor(231, 246, 237));
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Program Shortcut")
	FLinearColor InstalledTextColor = FLinearColor::FromSRGBColor(FColor(24, 137, 91));

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Program Shortcut")
	FText ShortcutText = NSLOCTEXT("FestaFiestaPrepProgram", "CatalogShortcut", "건축목록에서 보기");

	/** 0이면 동작 없이 기본 테두리와 안내 문구로만 알린다. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Program Shortcut", meta = (ClampMin = "0", ClampMax = "5"))
	float AttentionSeconds = 2.f;

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Program Shortcut", meta = (ClampMin = "1", ClampMax = "3"))
	int32 AttentionCycles = 2;

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Program Shortcut", meta = (ClampMin = "0", ClampMax = "1"))
	float AttentionOpacity = 0.75f;

private:
	bool bAttentionRequested = false;
	double AttentionStart = -1.0;
};
