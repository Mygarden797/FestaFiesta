// FF_FacilityRevenueRanking.h
// 역할: 시설별 수입 순위를 표시하고 선택 시설로 이동하는 요청을 전달한다.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Components/ListView.h"
#include "Styling/SlateTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "UI/Facility/FF_FacilityInfoTypes.h"
#include "FF_FacilityRevenueRanking.generated.h"

class UBorder;
class UButton;
class UTextBlock;
class UProgressBar;
class USizeBox;
class UFF_FacilityCodexImage;
class UFF_FestivalProgressionSubsystem;
class UFF_BuildCatalogSubsystem;
class UFF_BuildingFeatureDiscoverySubsystem;
enum class EFF_BuildModeState : uint8;
struct FFFZSWorldChangeEvent;

UCLASS()
class FESTAFIESTA_API UFF_FacilityRevenueRankItem : public UObject
{
	GENERATED_BODY()
  public:
	UPROPERTY(BlueprintReadOnly) FGuid FacilityId;
	UPROPERTY(BlueprintReadOnly) FText Name;
	UPROPERTY(BlueprintReadOnly) FText Location;
	UPROPERTY(BlueprintReadOnly) TSoftObjectPtr<UTexture2D> Thumbnail;
	UPROPERTY(BlueprintReadOnly) int32 TodayGold = 0;
	UPROPERTY(BlueprintReadOnly) int32 TotalGold = 0;
	UPROPERTY(BlueprintReadOnly) int32 Amount = 0;
	UPROPERTY(BlueprintReadOnly) int32 Rank = 0;
	UPROPERTY(BlueprintReadOnly) float RelativeRevenue = 0.f;
	UPROPERTY(BlueprintReadOnly) double Share = 0.;
	UPROPERTY(BlueprintReadOnly) bool bHasShare = false;
};
namespace FFFacilityRevenueRanking
{
// Sorts a presentation copy only; never changes an economy ledger or its ordering contract.
FESTAFIESTA_API int64 Rank(TArray<UFF_FacilityRevenueRankItem *> &Items, bool bCumulative);
} // namespace FFFacilityRevenueRanking
UCLASS(meta = (DisplayName = "FF Revenue List View"))
class FESTAFIESTA_API UFF_FacilityRevenueListView : public UListView
{
	GENERATED_BODY()
  public:
	void ConfigureEntryClass(TSubclassOf<UUserWidget> Class)
	{
		EntryWidgetClass = Class;
		SetSelectionMode(ESelectionMode::Single);
	}
};
UCLASS(Abstract, Blueprintable)
class FESTAFIESTA_API UFF_FacilityRevenueRankRow : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()
  public:
	void ApplyItem(const UFF_FacilityRevenueRankItem *Item);

  protected:
	virtual void NativeOnListItemObjectSet(UObject *Object) override;
	virtual void NativeOnItemSelectionChanged(bool bSelected) override;
	virtual void NativeOnEntryReleased() override;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Rank;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Name;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Location;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Amount;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Share;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SelectionMark;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_ThumbnailFallback;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UFF_FacilityCodexImage> Image_Thumbnail;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UProgressBar> Bar_Revenue;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UBorder> Border_Row;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance")
	FLinearColor SelectedTint = FLinearColor::FromSRGBColor(FColor(216, 244, 255));

  private:
	FLinearColor AuthoredTint = FLinearColor::White;
	bool bCapturedTint = false;
};
UCLASS(Abstract, Blueprintable)
class FESTAFIESTA_API UFF_FacilityRevenueRankingPanel : public UUserWidget
{
	GENERATED_BODY()
  public:
	void Present(const TArray<UFF_FacilityRevenueRankItem *> &Items, int64 Total, bool bAvailable,
				 FGuid PreferredSelection);
	FGuid GetSelectedFacilityId() const;
	bool IsCumulative() const
	{
		return bCumulative;
	}
	bool IsPointerOverPanel() const;
	bool ShowIntroduction();
	bool IsIntroductionVisible() const;
	void ShowInspectionFailure(const FText& Message);
	FSimpleMulticastDelegate OnCloseRequested;
	FSimpleMulticastDelegate OnPeriodChanged;
	FSimpleMulticastDelegate OnInspectRequested;

  protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnKeyDown(const FGeometry &Geometry, const FKeyEvent &Event) override;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UBorder> Border_Panel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USizeBox> Size_ClarityFrame;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UFF_FacilityRevenueListView> List_Ranking;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Total;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Count;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Empty;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Border_FirstRevenue;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_SelectedName;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_SelectedRank;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_SelectedShare;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_Today;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_Total;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_Close;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_Inspect;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> Border_Introduction;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_DismissIntroduction;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance|Tabs")
	FLinearColor SelectedPeriodTextColor = FLinearColor::White;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance|Tabs")
	FLinearColor InactivePeriodTextColor = FLinearColor::FromSRGBColor(FColor(0x3D, 0x3C, 0x4F));

  private:
	UFUNCTION() void Today();
	UFUNCTION() void Total();
	UFUNCTION() void CloseClicked();
	UFUNCTION() void InspectClicked();
	UFUNCTION() void DismissIntroduction();
	void SelectionChanged(UObject *Object);
	void ClearInspectionFailure();
	void UpdatePeriod();
	UPROPERTY(Transient) TMap<FName, FButtonStyle> AuthoredPeriodStyles;
	void UpdateFrameSize(const FVector2D& Viewport);
	FVector2D AuthoredFrameSize = FVector2D(1060.f, 690.f);
	bool bCapturedFrameSize = false;
	bool bCumulative = false;
	bool bForegroundSuppressed = false;
	FText AuthoredFootnote;
	FGuid FailedInspectionFacilityId;
	bool bCapturedFootnote = false;
	ESlateVisibility IntroductionVisibilityBeforeSuppression = ESlateVisibility::Collapsed;
};
UCLASS()
class FESTAFIESTA_API UFF_FacilityRevenueRankingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
  public:
	virtual void Initialize(FSubsystemCollectionBase &Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
	UFUNCTION(BlueprintCallable, Category = "Facility Revenue") void Open();
	UFUNCTION(BlueprintCallable, Category = "Facility Revenue") void Close();
	UFUNCTION(BlueprintPure, Category = "Facility Revenue") bool IsOpen() const;
	bool IsPointerOverPanel() const;
	FGuid GetSelectedFacilityId() const;
	/** The catalog owns the authored badge; only weak references are retained here. */
	void RegisterIntroductionHint(UBorder* Hint);

  private:
	void BindEconomy();
	void BindFeatureDiscovery();
	UFUNCTION() void FeatureSeen(FName FeatureId);
	void RefreshIntroductionHints();
	void Refresh();
	void Inspect();
	UFUNCTION() void Changed();
	UFUNCTION() void RevenueChanged(FGuid Id);
	UFUNCTION() void AmountChanged(int32 Value);
	UFUNCTION() void WorldChanged(const FFFZSWorldChangeEvent &Event);
	UFUNCTION() void SelectionChanged(FGuid Id, EFF_FacilityInfoContext Context);
	UFUNCTION() void BuildStateChanged(EFF_BuildModeState Previous, EFF_BuildModeState Current);
	UPROPERTY(Transient) TObjectPtr<UFF_FacilityRevenueRankingPanel> Panel;
	UPROPERTY(Transient) TObjectPtr<UFF_FestivalProgressionSubsystem> Progression;
	UPROPERTY(Transient) TObjectPtr<UFF_BuildCatalogSubsystem> Catalog;
	UPROPERTY(Transient) TObjectPtr<UFF_BuildingFeatureDiscoverySubsystem> FeatureDiscovery;
	TArray<TWeakObjectPtr<UBorder>> IntroductionHints;
	UPROPERTY(Transient) TArray<TObjectPtr<UFF_FacilityRevenueRankItem>> Items;
};
