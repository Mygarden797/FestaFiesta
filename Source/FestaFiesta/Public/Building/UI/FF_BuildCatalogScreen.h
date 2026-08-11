// FF_BuildCatalogScreen.h

#pragma once

#include "CoreMinimal.h"
#include "Building/FF_BuildCatalogTypes.h"
#include "UI/FF_ActivatableScreenBase.h"
#include "FF_BuildCatalogScreen.generated.h"

class SHorizontalBox;
class SButton;
class STextBlock;
class SVerticalBox;
class UCommonTextBlock;
class UFF_BuildFacilityCard;
class UFF_BuildMinorSection;
class UFF_CommonButtonBase;
class UHorizontalBox;
class UTexture2D;
class UVerticalBox;
class UWidgetSwitcher;
#if WITH_DEV_AUTOMATION_TESTS
class FFF_BuildCatalogWidgetBlueprintContractTest;
#endif

// 건축 모드에서 B 키로 펼치며, 배치 가능한 시설을 대분류/소분류별로 보여 주는 화면이다.
 
// 실제 게임 화면은 WBP_FF_BuildCatalogScreen의 WidgetTree를 사용한다. 이 네이티브 클래스는
// 카탈로그 데이터를 WBP의 BindWidget 필드에 연결하고, 실행 중 생성한 분류 버튼과 시설 카드의
// 클릭 이벤트를 외부 시스템이 소비할 수 있는 델리게이트로 변환한다.
 
// WBP의 RootWidget이 누락되어 디자이너 트리를 만들 수 없는 경우에도 화면 생성 자체가 실패하지 않도록
// 동일한 핵심 동작을 제공하는 Slate 트리를 폴백으로 보유한다. RootWidget은 있지만 필수 이름/타입 계약이
// 깨진 경우에는 잘못된 트리에 접근하지 않고 오류를 기록한다. 따라서 아래 갱신 함수 대부분은
// "디자이너 UMG 경로"와 "네이티브 Slate 경로"를 모두 처리한다.
 
UCLASS(Blueprintable, meta = (DisplayName = "FF Build Catalog Screen"))
class FESTAFIESTA_API UFF_BuildCatalogScreen : public UFF_ActivatableScreenBase
{
	GENERATED_BODY()

public:
	UFF_BuildCatalogScreen();

	// 시설 카드 선택 결과를 시설 정의 ID로 전달한다. 실제 배치 모드 전환은 화면 소유자가 결정한다. 
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnFacilitySelected, FName  FacilityDefinitionId );
	// 닫기 버튼에 대한 의도만 전달하며, 화면 제거 자체는 화면 스택 소유자가 수행한다.
	DECLARE_MULTICAST_DELEGATE(FOnCloseRequested);
	// 포커스된 카탈로그가 B 키를 먼저 받은 경우 건축 모드 복귀 의도를 별도로 전달한다.
	DECLARE_MULTICAST_DELEGATE(FOnBuildModeKeyRequested);
	// 포커스된 카탈로그가 Esc를 먼저 받은 경우 건축 세션 전체 종료 의도를 전달한다.
	DECLARE_MULTICAST_DELEGATE(FOnExitBuildModeKeyRequested);

	FOnFacilitySelected OnFacilitySelected;
	FOnCloseRequested OnCloseRequested;
	FOnBuildModeKeyRequested OnBuildModeKeyRequested;
	FOnExitBuildModeKeyRequested OnExitBuildModeKeyRequested;

	//화면에 표시할 스냅샷을 교체하고, 최초로 항목이 존재하는 대분류를 선택한 뒤 UI를 재구성한다.
	// 호출 시점이 NativeConstruct보다 빠른 경우도 허용하므로 사용 가능한 BindWidget이 있으면 먼저 초기화한다.
	 
	void SetCatalogEntries(const TArray<FFF_BuildCatalogEntry>& InEntries);

	// 선택 탭·대분류와 카드 객체를 유지한 채, 현재 보이는 카드들의 이미지만 다시 적용합니다.
	void RefreshLoadedThumbnails();

	// UMG/Slate 중 현재 활성화된 표현 경로 양쪽에 같은 상태 문구와 오류 색상을 반영한다. 
	void SetStatusText(const FText& InStatusText, bool bIsError = false);

	// Runtime diagnostics used by automation and UI smoke tests. 
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|UI")
	int32 GetDisplayedCategoryCount() const { return DisplayedCategoryCount; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|UI")
	int32 GetDisplayedFacilityCardCount() const { return DisplayedFacilityCardCount; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|UI")
	bool IsUsingDesignerWidgetTree() const;

protected:
	// 디자이너 WidgetTree가 있으면 Super에 맡기고, 없을 때만 안전용 Slate 트리를 만든다. 
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// 생성 단계가 호출되는 환경 차이를 흡수하기 위해 Initialized와 Construct 모두에서 멱등 초기화를 시도한다. 
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	// 동적 버튼 델리게이트를 해제하여 재생성 시 중복 호출과 소멸 객체 참조를 막는다. 
	virtual void NativeDestruct() override;
	// Slate 폴백의 공유 포인터만 해제하며 UObject 기반 WBP 위젯 수명주기와 섞지 않는다. 
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	// 카탈로그가 키보드 포커스를 가져도 B 키를 건축 모드 복귀 요청으로 변환한다.
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	// Slate 폴백에서는 첫 탭으로 키보드/게임패드 포커스를 직접 전달한다. 
	virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;
	// UMG 경로의 CommonUI 포커스 대상으로 건설 탭을 제공한다. 
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FFF_BuildCatalogWidgetBlueprintContractTest;
#endif
	// 상단 탭 상태. Skin은 현재 안내 패널을 표시하고 Build만 카탈로그를 채운다. 
	enum class ETopTab : uint8
	{
		Build,
		Skin
	};

	// 외부에서 받은 표시 데이터의 화면 소유 복사본. 동적 카드가 이 배열의 항목을 복사해 사용한다. 
	TArray<FFF_BuildCatalogEntry> CatalogEntries;
	EFFZSFacilityCategory SelectedCategory = EFFZSFacilityCategory::Booth;
	ETopTab SelectedTopTab = ETopTab::Build;
	FText CurrentStatusText;
	bool bCurrentStatusIsError = false;

	// 현재 표현 경로와 델리게이트 연결 여부를 분리해, 초기화 훅이 여러 번 와도 한 번만 바인딩한다. 
	bool bUsingDesignerWidgetTree = false;
	bool bDesignerBindingsInitialized = false;

	// 실제 생성 결과를 기록하는 진단 값으로, 에디터 자동화 테스트가 동적 UI 생성을 검증한다. 
	int32 DisplayedCategoryCount = 0;
	int32 DisplayedFacilityCardCount = 0;

	// WBP 계약이 성립하지 않을 때만 사용하는 네이티브 Slate 폴백의 수명 추적 포인터이다.
	TSharedPtr<SHorizontalBox> CategoryBox;
	TSharedPtr<SButton> InitialFocusButton;
	TSharedPtr<SVerticalBox> ContentBox;
	TSharedPtr<STextBlock> StatusTextBlock;

	// WBP_FF_BuildCatalogScreen과의 이름 기반 계약이다. 이름/타입 변경 시 BindWidget 연결과 자동화 테스트가 실패한다.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|UI", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UFF_CommonButtonBase> BTN_BuildTab;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|UI", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UFF_CommonButtonBase> BTN_SkinTab;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|UI", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UFF_CommonButtonBase> BTN_Close;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|UI", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UHorizontalBox> BOX_CategoryButtons;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|UI", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetSwitcher> SWITCHER_Content;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|UI", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UVerticalBox> BOX_MinorSections;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|UI", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> TXT_Status;

	// 실행 중 CreateWidget으로 복제할 세 가지 WBP 템플릿이다.
	// SoftClass로 두어 화면 클래스 로드 시 강제 로드를 피하고, Class Defaults에서 아트 템플릿을 교체할 수 있게 한다.
	 
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Building|UI|Templates")
	TSoftClassPtr<UFF_CommonButtonBase> CategoryButtonClass;

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Building|UI|Templates")
	TSoftClassPtr<UFF_BuildMinorSection> MinorSectionClass;

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Building|UI|Templates")
	TSoftClassPtr<UFF_BuildFacilityCard> FacilityCardClass;

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Building|UI|Style")
	FLinearColor SelectedWidgetTint = FLinearColor(0.08f, 0.72f, 0.66f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Building|UI|Style")
	FLinearColor IdleWidgetTint = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Building|UI|Style")
	FLinearColor NormalStatusColor = FLinearColor(0.45f, 0.92f, 0.84f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Building|UI|Style")
	FLinearColor ErrorStatusColor = FLinearColor(1.0f, 0.32f, 0.28f, 1.0f);

	// 5x2 아이콘 아틀라스. 인덱스 0~2는 건설/스킨/닫기, 3~9는 일곱 시설 대분류에 고정 대응한다. 
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Building|UI|Style")
	TSoftObjectPtr<UTexture2D> CategoryIconAtlas;

	// 동적으로 만든 버튼에는 분류 인수가 없으므로, 클릭한 버튼 포인터를 분류 값으로 역매핑한다. 
	TMap<UFF_CommonButtonBase*, EFFZSFacilityCategory> CategoryByButton;
	// 컨테이너가 소유하는 동적 카드를 약참조해 수명 연장을 피하면서 델리게이트 해제에 사용한다. 
	TArray<TWeakObjectPtr<UFF_BuildFacilityCard>> FacilityCards;

	// 선택 탭/분류 상태를 기준으로 동적 분류 버튼을 완전히 다시 만든다. 
	void RefreshCategoryButtons();
	// 선택 분류의 항목을 소분류별로 묶어 섹션과 카드를 다시 만들고 표시 개수를 갱신한다. 
	void RefreshContent();
	// CommonButton의 선택 상태를 상단 탭 상태와 동기화한다. 
	void RefreshTabStyles();
	// 저장된 상태 문구를 현재 존재하는 UMG와 Slate 출력 위젯에 각각 반영한다. 
	void ApplyStatusText();
	// 빈 기본 분류 대신 실제 데이터가 존재하는 첫 분류가 처음 보이도록 선택한다. 
	void SelectFirstPopulatedCategory();
	// 필수 BindWidget과 Switcher의 두 페이지 계약이 모두 유효한지 검사한다. 
	bool HasUsableDesignerBindings() const;
	// 멱등적으로 정적 버튼 이벤트를 연결하고 초기 데이터를 디자이너 트리에 투영한다. 
	void InitializeDesignerBindings();
	// 새로고침/소멸 전에 런타임 생성 위젯의 동적 델리게이트를 정리한다. 
	void ClearDynamicUMGBindings();
	// 아틀라스 셀 인덱스를 UV 영역으로 변환한 아이콘 브러시를 만든다. 
	FSlateBrush MakeAtlasIconBrush(int32 AtlasIndex) const;
	static int32 GetCategoryAtlasIndex(EFFZSFacilityCategory Category);

	// UMG 이벤트 경로: CommonButton의 동적 델리게이트 시그니처를 화면 상태 변경/외부 이벤트로 변환한다.
	UFUNCTION()
	void HandleBuildTabPressed(UFF_CommonButtonBase* Button);

	UFUNCTION()
	void HandleSkinTabPressed(UFF_CommonButtonBase* Button);

	UFUNCTION()
	void HandleClosePressed(UFF_CommonButtonBase* Button);

	UFUNCTION()
	void HandleCategoryPressed(UFF_CommonButtonBase* Button);

	UFUNCTION()
	void HandleFacilityCardPressed(UFF_CommonButtonBase* Button);

	// Slate 폴백 이벤트 경로: 동일한 상태 변경 후 FReply::Handled를 반환해 입력 소비를 명시한다.
	FReply HandleCloseClicked();
	FReply HandleBuildTabClicked();
	FReply HandleSkinTabClicked();
	FReply HandleCategoryClicked(EFFZSFacilityCategory Category);
	FReply HandleFacilityClicked(FName FacilityDefinitionId);

	FSlateColor GetBuildTabColor() const;
	FSlateColor GetSkinTabColor() const;
	FSlateColor GetCategoryColor(EFFZSFacilityCategory Category) const;

	static FText GetCategoryDisplayName(EFFZSFacilityCategory Category);
	static FText GetMinorDisplayName(FName MinorToken);
};
