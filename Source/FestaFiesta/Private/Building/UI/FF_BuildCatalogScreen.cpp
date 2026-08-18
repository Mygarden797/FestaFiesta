// FF_BuildCatalogScreen.cpp

#include "Building/UI/FF_BuildCatalogScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Building/FF_FacilityNamingLibrary.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Building/FF_BuildCatalogEconomyReadModel.h"
#include "Building/UI/FF_BuildFacilityCard.h"
#include "Building/UI/FF_BuildMinorSection.h"
#include "Building/UI/FF_BuildTopButtonStyle.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "UI/FF_CommonButtonBase.h"
#include "UI/FF_UILog.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FFBuildCatalogScreen"

namespace
{
	// UI에 노출할 대분류의 순서 자체가 화면 순서이자 "첫 데이터 분류" 탐색 우선순위이다.
	// enum 전체를 순회하지 않고 명시 목록을 두어 숨겨진/추가 enum 값이 자동 노출되는 것을 막는다.
	constexpr EFFZSFacilityCategory AllCategories[] = {
		EFFZSFacilityCategory::Booth,
		EFFZSFacilityCategory::Toilet,
		EFFZSFacilityCategory::Info,
		EFFZSFacilityCategory::Rest,
		EFFZSFacilityCategory::Performance,
		EFFZSFacilityCategory::Safety,
		EFFZSFacilityCategory::PhotoSpot,
	};

	// 아래 색상은 WBP가 없을 때 만들어지는 Slate 폴백 전용이다. 디자이너 경로의 색은 UPROPERTY를 따른다.
	const FLinearColor NativeSelectedColor(0.08f, 0.72f, 0.66f, 1.0f);
	const FLinearColor NativeIdleColor(0.55f, 0.62f, 0.68f, 1.0f);

	// 런타임에 동적 생성하는 WBP 템플릿과 아이콘 에셋의 기본 경로이다.
	// 생성자에서 soft reference로만 등록하므로 화면 클래스 로드만으로 에셋을 강제 로드하지 않는다.
	const TCHAR* CategoryButtonPath =
		TEXT("/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildCategoryButton.WBP_FF_BuildCategoryButton_C");
	const TCHAR* MinorSectionPath =
		TEXT("/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildMinorSection.WBP_FF_BuildMinorSection_C");
	const TCHAR* FacilityCardPath =
		TEXT("/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildFacilityCard.WBP_FF_BuildFacilityCard_C");
	const TCHAR* CategoryIconAtlasPath =
		TEXT("/Game/FestaFiesta/Zone/Test_Asset/Image/T_UI_BuildCatalog_IconAtlas.T_UI_BuildCatalog_IconAtlas");

	void ApplyButtonIconBrush(UFF_CommonButtonBase* Button, const FSlateBrush& Brush)
	{
		// Preserve a WBP-authored icon when the atlas is unavailable.
		if (!IsValid(Button) || Brush.GetResourceObject() == nullptr)
		{
			return;
		}

		if (UImage* Icon = Cast<UImage>(Button->GetWidgetFromName(TEXT("Image_Icon"))))
		{
			Icon->SetBrush(Brush);
			return;
		}

		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] Image_Icon widget is missing."), *GetNameSafe(Button));
	}

	void MakeButtonSurfaceTransparent(UFF_CommonButtonBase* Button, const FName SurfaceName)
	{
		if (!IsValid(Button))
		{
			return;
		}

		// The generated category/card WBP has its own Border in addition to the
		// CommonButton Slate brush. Both layers must be transparent.
		Button->SetStyle(UFF_BuildTopButtonStyle::StaticClass());
		if (UBorder* Surface = Cast<UBorder>(Button->GetWidgetFromName(SurfaceName)))
		{
			Surface->SetBrushColor(FLinearColor::Transparent);
		}
	}

	void ApplyExclusiveTabSelection(UFF_CommonButtonBase* Button, const bool bSelected)
	{
		if (!IsValid(Button))
		{
			return;
		}

		if (bSelected)
		{
			Button->SetIsSelected(true, false);
		}
		else
		{
			// SetIsSelected(false) only clears toggleable CommonButtons. These tabs
			// are intentionally non-toggleable, so use the explicit reset API.
			Button->ClearSelection();
		}
		if (UImage* Indicator = Cast<UImage>(
			Button->GetWidgetFromName(TEXT("Image_SelectedIndicator"))))
		{
			Indicator->SetVisibility(
				bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		}
		Button->RefreshVisualState();
	}

	void StyleRuntimeGoldText(
		UCommonTextBlock& Text,
		const int32 FontSize,
		const FLinearColor& Color,
		const ETextJustify::Type Justification = ETextJustify::Left)
	{
		FSlateFontInfo Font = Text.GetFont();
		Font.Size = FontSize;
		Text.SetFont(Font);
		Text.SetColorAndOpacity(FSlateColor(Color));
		Text.SetJustification(Justification);
	}
}

UFF_BuildCatalogScreen::UFF_BuildCatalogScreen()
{
	// 카탈로그는 게임 입력보다 UI 탐색을 우선하는 메뉴 화면이다. 활성화 시 마우스를 노출하고,
	// CommonUI가 건설 탭으로 포커스를 보낼 수 있도록 포커스 가능 화면으로 설정한다.
	ScreenInputMode = EFF_ScreenInputMode::Menu;
	bShowMouseCursor = true;
	bIgnoreInputConfig = true;
	bForceFocusOnActivate = true;
	SetIsFocusable(true);

	// WBP는 Class Defaults에서 교체 가능하지만, 별도 설정이 없어도 기준 에셋으로 동작하도록 기본값을 제공한다.
	CategoryButtonClass = TSoftClassPtr<UFF_CommonButtonBase>(FSoftObjectPath(CategoryButtonPath));
	MinorSectionClass = TSoftClassPtr<UFF_BuildMinorSection>(FSoftObjectPath(MinorSectionPath));
	FacilityCardClass = TSoftClassPtr<UFF_BuildFacilityCard>(FSoftObjectPath(FacilityCardPath));
	CategoryIconAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(CategoryIconAtlasPath));
	CurrentStatusText = LOCTEXT("DefaultStatus", "시설을 선택하면 배치 모드로 전환됩니다.");
}

void UFF_BuildCatalogScreen::SetCatalogEntries(const TArray<FFF_BuildCatalogEntry>& InEntries)
{
	// 데이터 공급자가 NativeConstruct보다 먼저 호출되는 경우에도 이미 BindWidget 계약이 성립했다면
	// 먼저 이벤트와 초기 UI를 연결한다. InitializeDesignerBindings는 멱등이라 중복 호출해도 안전하다.
	if (!bDesignerBindingsInitialized && HasUsableDesignerBindings())
	{
		InitializeDesignerBindings();
	}
	// 호출자 배열의 수명과 분리된 스냅샷을 보관한 뒤 전체 동적 UI를 재생성한다.
	CatalogEntries = InEntries;
	SelectFirstPopulatedCategory();
	RefreshCategoryButtons();
	RefreshContent();
}

void UFF_BuildCatalogScreen::RefreshLoadedThumbnails()
{
	for (const TWeakObjectPtr<UFF_BuildFacilityCard>& Card : FacilityCards)
	{
		if (Card.IsValid())
		{
			Card->RefreshThumbnail();
		}
	}
}

void UFF_BuildCatalogScreen::SetStatusText(const FText& InStatusText, const bool bIsError)
{
	// UI 트리가 아직 없더라도 상태를 먼저 저장해 두면 RebuildWidget/Initialize 단계에서 동일하게 복원된다.
	CurrentStatusText = InStatusText;
	bCurrentStatusIsError = bIsError;
	ApplyStatusText();
}

bool UFF_BuildCatalogScreen::IsUsingDesignerWidgetTree() const
{
	// 초기화 플래그가 아직 서기 전(예: 자동화 테스트의 TakeWidget 직후)에도 현재 바인딩 상태를 진단할 수 있다.
	return bUsingDesignerWidgetTree || HasUsableDesignerBindings();
}

FText UFF_BuildCatalogScreen::GetDisplayedGoldText() const
{
	if (TXT_Gold)
	{
		return TXT_Gold->GetText();
	}
	return GoldTextBlock.IsValid() ? GoldTextBlock->GetText() : FText::GetEmpty();
}

void UFF_BuildCatalogScreen::EnsureGoldDisplay()
{
	if (!WidgetTree)
	{
		return;
	}

	// 명시적으로 제작된 최신 WBP라면 디자이너 위젯을 그대로 사용한다.
	if (UCommonTextBlock* ExistingGold =
		Cast<UCommonTextBlock>(WidgetTree->FindWidget(TEXT("TXT_Gold"))))
	{
		TXT_Gold = ExistingGold;
		ApplyGoldText();
		return;
	}

	UHorizontalBox* TopBar = Cast<UHorizontalBox>(WidgetTree->FindWidget(TEXT("Top_Bar")));
	UWidget* CloseButton = WidgetTree->FindWidget(TEXT("BTN_Close"));
	const int32 CloseIndex = TopBar && CloseButton
		? TopBar->GetChildIndex(CloseButton)
		: INDEX_NONE;
	if (!TopBar || CloseIndex == INDEX_NONE)
	{
		// 네이티브 Slate 폴백은 RebuildWidget에서 자체 Gold 패널을 제공한다.
		return;
	}

	USizeBox* GoldSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("Gold_Size"));
	UBorder* GoldPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Gold_Panel"));
	UHorizontalBox* GoldRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("Gold_Row"));
	UCommonTextBlock* GoldLabel = WidgetTree->ConstructWidget<UCommonTextBlock>(
		UCommonTextBlock::StaticClass(), TEXT("TXT_GoldLabel"));
	USpacer* GoldSpacer = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(), TEXT("Gold_ValueSpacer"));
	UCommonTextBlock* GoldValue = WidgetTree->ConstructWidget<UCommonTextBlock>(
		UCommonTextBlock::StaticClass(), TEXT("TXT_Gold"));
	if (!GoldSize || !GoldPanel || !GoldRow || !GoldLabel || !GoldSpacer || !GoldValue)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] 건축 카탈로그 Gold 표시 위젯 생성에 실패했습니다."), *GetName());
		return;
	}

	GoldSize->SetWidthOverride(320.0f);
	GoldSize->SetHeightOverride(58.0f);
	GoldSize->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	GoldPanel->SetBrushColor(FLinearColor(0.035f, 0.110f, 0.145f, 0.74f));
	GoldPanel->SetPadding(FMargin(18.0f, 10.0f));
	GoldPanel->SetHorizontalAlignment(HAlign_Fill);
	GoldPanel->SetVerticalAlignment(VAlign_Fill);

	GoldLabel->SetText(LOCTEXT("CurrentGoldLabelUMG", "현재 잔액"));
	StyleRuntimeGoldText(
		*GoldLabel, 13, FLinearColor(0.58f, 0.69f, 0.78f, 1.0f));
	StyleRuntimeGoldText(
		*GoldValue, 22, FLinearColor(0.08f, 0.95f, 0.88f, 1.0f), ETextJustify::Right);

	if (UHorizontalBoxSlot* LabelSlot = GoldRow->AddChildToHorizontalBox(GoldLabel))
	{
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UHorizontalBoxSlot* SpacerSlot = GoldRow->AddChildToHorizontalBox(GoldSpacer))
	{
		SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	if (UHorizontalBoxSlot* ValueSlot = GoldRow->AddChildToHorizontalBox(GoldValue))
	{
		ValueSlot->SetVerticalAlignment(VAlign_Center);
	}
	GoldPanel->AddChild(GoldRow);
	GoldSize->AddChild(GoldPanel);

	if (UHorizontalBoxSlot* GoldSlot =
		Cast<UHorizontalBoxSlot>(TopBar->InsertChildAt(CloseIndex, GoldSize)))
	{
		GoldSlot->SetPadding(FMargin(0.0f, 0.0f, 18.0f, 0.0f));
		GoldSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		GoldSlot->SetVerticalAlignment(VAlign_Center);
	}
	else
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] 건축 카탈로그 Top_Bar에 Gold 표시를 추가하지 못했습니다."), *GetName());
		return;
	}

	TXT_Gold = GoldValue;
	ApplyGoldText();
}

void UFF_BuildCatalogScreen::BindGoldReadModel()
{
	UnbindGoldReadModel();
	bHasCurrentGold = false;
	ApplyGoldText();

	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (!GameInstance)
	{
		// 에디터의 playerless Widget 계약 테스트에는 GameInstance가 없다.
		return;
	}

	CachedEconomyReadModel =
		GameInstance->GetSubsystem<UFF_BuildCatalogEconomyReadModel>();
	if (!CachedEconomyReadModel)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] BuildCatalogEconomyReadModel을 찾지 못해 Gold를 표시할 수 없습니다."),
			*GetName());
		return;
	}

	CachedEconomyReadModel->OnGoldChanged.AddUniqueDynamic(this, &ThisClass::HandleGoldChanged);
	// Read Model은 화면보다 오래 살고 초기 이벤트는 이미 지나갔으므로 Bind + Sync가 한 묶음입니다.
	if (CachedEconomyReadModel->HasAuthoritativeGold())
	{
		HandleGoldChanged(CachedEconomyReadModel->GetGold());
	}
}

void UFF_BuildCatalogScreen::UnbindGoldReadModel()
{
	if (CachedEconomyReadModel)
	{
		CachedEconomyReadModel->OnGoldChanged.RemoveDynamic(this, &ThisClass::HandleGoldChanged);
	}
	CachedEconomyReadModel = nullptr;
}

void UFF_BuildCatalogScreen::HandleGoldChanged(const int32 NewGold)
{
	CurrentGold = FMath::Max(0, NewGold);
	bHasCurrentGold = true;
	ApplyGoldText();
}

FText UFF_BuildCatalogScreen::MakeCurrentGoldText() const
{
	if (!bHasCurrentGold)
	{
		return LOCTEXT("CurrentGoldUnavailable", "—");
	}

	return FText::Format(
		LOCTEXT("CurrentGoldFormat", "{0} Gold"),
		FText::AsNumber(CurrentGold));
}

void UFF_BuildCatalogScreen::ApplyGoldText()
{
	const FText GoldText = MakeCurrentGoldText();
	if (TXT_Gold)
	{
		TXT_Gold->SetText(GoldText);
	}
	if (GoldTextBlock.IsValid())
	{
		GoldTextBlock->SetText(GoldText);
	}
}

TSharedRef<SWidget> UFF_BuildCatalogScreen::RebuildWidget()
{
	// 파생 WBP가 유효한 RootWidget을 제공하면 반드시 상위 구현에 생성을 맡긴다.
	// 그래야 디자이너가 만든 계층이 유지되고 이름 기반 BindWidget 필드도 정상 연결된다.
	if (WidgetTree && WidgetTree->RootWidget)
	{
		return Super::RebuildWidget();
	}

	// WBP가 없거나 손상된 경우 화면 자체가 사라지지 않도록 최소 기능의 Slate 트리를 만든다.
	// 이 경로는 운영 디자인을 대체하려는 것이 아니라 선택/닫기 기능을 보존하는 안전망이다.
	if (!IsDesignTime())
	{
		// Super::RebuildWidget을 거치지 않으므로, 런타임 리스너가 재구축 시점을 알 수 있게 같은 브로드캐스트를 보완한다.
		UCommonActivatableWidget::OnRebuilding.Broadcast(*this);
	}

	// 계층: 반투명 루트 Border -> 세로 레이아웃 -> 상단 탭/닫기, 대분류 행, 구분선,
	// 스크롤 가능한 소분류/카드 영역, 상태 문구 순서이다. 일부 슬롯 포인터를 보관해 이후 갱신한다.
	TSharedRef<SWidget> Root =
		SNew(SBorder)
		.Padding(FMargin(42.0f, 28.0f))
		.BorderBackgroundColor(FLinearColor(0.015f, 0.045f, 0.075f, 0.94f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 16.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 18.0f, 0.0f)
				[
					SAssignNew(InitialFocusButton, SButton)
					.ContentPadding(FMargin(28.0f, 10.0f))
					.ButtonColorAndOpacity(TAttribute<FSlateColor>::Create(
						TAttribute<FSlateColor>::FGetter::CreateUObject(
							this, &UFF_BuildCatalogScreen::GetBuildTabColor)))
					.OnClicked_UObject(this, &UFF_BuildCatalogScreen::HandleBuildTabClicked)
					[
						SNew(STextBlock).Text(LOCTEXT("BuildTab", "건축"))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.ContentPadding(FMargin(28.0f, 10.0f))
					.ButtonColorAndOpacity(TAttribute<FSlateColor>::Create(
						TAttribute<FSlateColor>::FGetter::CreateUObject(
							this, &UFF_BuildCatalogScreen::GetSkinTabColor)))
					.OnClicked_UObject(this, &UFF_BuildCatalogScreen::HandleSkinTabClicked)
					[
						SNew(STextBlock).Text(LOCTEXT("SkinTab", "스킨"))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SSpacer)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 18.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(320.0f)
					.HeightOverride(58.0f)
					[
						SNew(SBorder)
						.Padding(FMargin(18.0f, 10.0f))
						.BorderBackgroundColor(FLinearColor(0.035f, 0.110f, 0.145f, 0.74f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("CurrentGoldLabelSlate", "현재 잔액"))
								.ColorAndOpacity(FLinearColor(0.58f, 0.69f, 0.78f, 1.0f))
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							[
								SNew(SSpacer)
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SAssignNew(GoldTextBlock, STextBlock)
								.Text(MakeCurrentGoldText())
								.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 22))
								.ColorAndOpacity(FLinearColor(0.08f, 0.95f, 0.88f, 1.0f))
							]
						]
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.ContentPadding(FMargin(16.0f, 8.0f))
					.OnClicked_UObject(this, &UFF_BuildCatalogScreen::HandleCloseClicked)
					[
						SNew(STextBlock).Text(LOCTEXT("Close", "닫기  B"))
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 12.0f)
			[
				SAssignNew(CategoryBox, SHorizontalBox)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0.0f, 14.0f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SAssignNew(ContentBox, SVerticalBox)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SAssignNew(StatusTextBlock, STextBlock)
				.Text(CurrentStatusText)
				.ColorAndOpacity(bCurrentStatusIsError ? ErrorStatusColor : NormalStatusColor)
			]
		];

	// Rebuild 이전에 데이터나 상태가 공급됐을 수 있으므로 새 트리에 저장 상태를 즉시 투영한다.
	RefreshCategoryButtons();
	RefreshContent();
	return Root;
}

void UFF_BuildCatalogScreen::NativeConstruct()
{
	Super::NativeConstruct();
	// PIE/런타임의 일반 생성 경로. Initialized와 호출 순서가 달라도 멱등 가드가 중복 바인딩을 막는다.
	InitializeDesignerBindings();
	BindGoldReadModel();
}

void UFF_BuildCatalogScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// 자동화의 playerless 생성처럼 Construct와 다른 수명주기에서도 WBP 데이터 경로를 실행하기 위한 보완 훅이다.
	InitializeDesignerBindings();
}

void UFF_BuildCatalogScreen::InitializeDesignerBindings()
{
	// 현재 저장된 WBP는 Gold 패널 이전 버전이다. 첫 Slate 생성 전에 Top_Bar만 보강하고,
	// 향후 Builder 생성본에 TXT_Gold가 있으면 그대로 재사용한다.
	EnsureGoldDisplay();

	// 두 수명주기 훅과 조기 SetCatalogEntries 호출이 모두 이 함수에 들어올 수 있으므로 반드시 한 번만 연결한다.
	if (bDesignerBindingsInitialized)
	{
		return;
	}

	// 단순히 RootWidget 존재만 보지 않고, 이 클래스가 실제 갱신에 사용하는 모든 필수 필드와
	// Build/Skin 두 Switcher 페이지까지 확인해야 부분 생성된 WBP를 안전하게 거부할 수 있다.
	bUsingDesignerWidgetTree = HasUsableDesignerBindings();
	if (!bUsingDesignerWidgetTree)
	{
		if (WidgetTree && WidgetTree->RootWidget)
		{
			// 루트는 있는데 계약이 불완전한 경우는 에셋 제작 오류이므로 조용한 폴백과 구분해 로그를 남긴다.
			UE_LOG(LogFestaFiestaUI, Error,
				TEXT("[%s] WBP_FF_BuildCatalogScreen의 필수 BindWidget 이름 또는 타입이 올바르지 않습니다."),
				*GetName());
		}
		return;
	}
	bDesignerBindingsInitialized = true;

	// 정적 WBP 버튼은 한 번만 바인딩하고, 런타임 텍스트/아이콘을 네이티브 기준 데이터로 덮어쓴다.
	// AddUniqueDynamic은 에디터 재구성 등 예외적인 중복 진입에서도 같은 대상/함수 조합의 중복 실행을 막는다.
	BTN_BuildTab->OnFFButtonClicked.AddUniqueDynamic(this, &ThisClass::HandleBuildTabPressed);
	BTN_SkinTab->OnFFButtonClicked.AddUniqueDynamic(this, &ThisClass::HandleSkinTabPressed);
	BTN_Close->OnFFButtonClicked.AddUniqueDynamic(this, &ThisClass::HandleClosePressed);
	BTN_BuildTab->SetButtonLabel(LOCTEXT("BuildTabUMG", "건축"));
	BTN_SkinTab->SetButtonLabel(LOCTEXT("SkinTabUMG", "스킨"));
	BTN_Close->SetButtonLabel(LOCTEXT("CloseUMG", "닫기  B"));
	ApplyButtonIconBrush(BTN_BuildTab, MakeAtlasIconBrush(0));
	ApplyButtonIconBrush(BTN_SkinTab, MakeAtlasIconBrush(1));
	ApplyButtonIconBrush(BTN_Close, MakeAtlasIconBrush(2));
	InitialFocusTarget = BTN_BuildTab;

	// 초기화 전에 들어온 상태와 카탈로그 스냅샷을 이제 완성된 디자이너 트리에 한 번에 반영한다.
	RefreshTabStyles();
	RefreshCategoryButtons();
	RefreshContent();
	ApplyStatusText();
}

void UFF_BuildCatalogScreen::NativeDestruct()
{
	UnbindGoldReadModel();

	// UMG 동적 멀티캐스트 델리게이트는 UObject 수명과 별도로 연결을 보유하므로 명시적으로 제거한다.
	// 화면이 다시 Construct될 때 이전 연결이 남아 클릭 한 번에 핸들러가 여러 번 실행되는 것도 방지한다.
	if (BTN_BuildTab)
	{
		BTN_BuildTab->OnFFButtonClicked.RemoveAll(this);
	}
	if (BTN_SkinTab)
	{
		BTN_SkinTab->OnFFButtonClicked.RemoveAll(this);
	}
	if (BTN_Close)
	{
		BTN_Close->OnFFButtonClicked.RemoveAll(this);
	}
	ClearDynamicUMGBindings();
	// 다음 생성 주기에는 현재 WBP 포인터를 기준으로 계약을 다시 검사하고 새 위젯에 다시 바인딩해야 한다.
	InitialFocusTarget = nullptr;
	bUsingDesignerWidgetTree = false;
	bDesignerBindingsInitialized = false;
	Super::NativeDestruct();
}

void UFF_BuildCatalogScreen::ReleaseSlateResources(const bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	// Slate 폴백에서 SAssignNew로 보관한 포인터만 해제한다. UMG UObject 자식은 WidgetTree가 관리한다.
	CategoryBox.Reset();
	InitialFocusButton.Reset();
	ContentBox.Reset();
	StatusTextBlock.Reset();
	GoldTextBlock.Reset();
}

FReply UFF_BuildCatalogScreen::NativeOnPreviewKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::B && !InKeyEvent.IsRepeat())
	{
		// UI가 먼저 처리한 B가 같은 프레임의 PlayerController 폴링과 중복되지 않도록 별도 의도로 전달한다.
		OnBuildModeKeyRequested.Broadcast();
		return FReply::Handled();
	}
	if (InKeyEvent.GetKey() == EKeys::Escape && !InKeyEvent.IsRepeat())
	{
		OnExitBuildModeKeyRequested.Broadcast();
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UFF_BuildCatalogScreen::NativeOnFocusReceived(
	const FGeometry& InGeometry,
	const FFocusEvent& InFocusEvent)
{
	if (InitialFocusButton.IsValid())
	{
		// 폴백에서는 CommonUI의 UWidget 포커스 대상이 없으므로 Slate 버튼에 사용자 포커스를 직접 지정한다.
		return FReply::Handled().SetUserFocus(
			InitialFocusButton.ToSharedRef(), InFocusEvent.GetCause());
	}
	return Super::NativeOnFocusReceived(InGeometry, InFocusEvent);
}

UWidget* UFF_BuildCatalogScreen::NativeGetDesiredFocusTarget() const
{
	if (BTN_BuildTab)
	{
		// 디자이너 경로에서는 CommonActivatableWidget이 이 반환값을 활성화 직후 초기 포커스로 사용한다.
		return BTN_BuildTab;
	}
	return Super::NativeGetDesiredFocusTarget();
}

bool UFF_BuildCatalogScreen::HasUsableDesignerBindings() const
{
	// Switcher 인덱스 0/1을 Build/Skin으로 고정 사용하므로 위젯 포인터뿐 아니라 최소 페이지 수도 계약에 포함한다.
	return BTN_BuildTab && BTN_SkinTab && BTN_Close && BOX_CategoryButtons &&
		SWITCHER_Content && BOX_MinorSections && TXT_Status &&
		SWITCHER_Content->GetNumWidgets() >= 2;
}

void UFF_BuildCatalogScreen::ClearDynamicUMGBindings()
{
	// 분류 버튼은 새로고침 때마다 폐기되므로, 역매핑을 지우기 전에 화면 핸들러부터 제거한다.
	for (const TPair<UFF_CommonButtonBase*, EFFZSFacilityCategory>& Pair : CategoryByButton)
	{
		if (Pair.Key)
		{
			Pair.Key->OnFFButtonClicked.RemoveAll(this);
		}
	}
	CategoryByButton.Reset();

	// 카드는 패널이 강하게 소유하고 이 배열은 약참조만 가진다. 이미 패널 제거로 파괴된 카드는 건너뛴다.
	for (const TWeakObjectPtr<UFF_BuildFacilityCard>& Card : FacilityCards)
	{
		if (Card.IsValid())
		{
			Card->OnFFButtonClicked.RemoveAll(this);
		}
	}
	FacilityCards.Reset();
}

void UFF_BuildCatalogScreen::RefreshCategoryButtons()
{
	// 운영 경로: WBP의 HorizontalBox를 데이터로 다시 채운다.
	if (bUsingDesignerWidgetTree && BOX_CategoryButtons)
	{
		// 기존 버튼을 제거하기 전에 델리게이트를 끊는다. ClearChildren만 호출하면 UObject가 GC되기 전까지
		// 화면과 연결된 동적 델리게이트가 남을 수 있고, 역매핑도 이전 포인터를 가리키게 된다.
		for (const TPair<UFF_CommonButtonBase*, EFFZSFacilityCategory>& Pair : CategoryByButton)
		{
			if (Pair.Key)
			{
				Pair.Key->OnFFButtonClicked.RemoveAll(this);
			}
		}
		CategoryByButton.Reset();
		BOX_CategoryButtons->ClearChildren();
		DisplayedCategoryCount = 0;

		// Skin 페이지에는 대분류가 의미 없으므로 행을 빈 상태로 둔다.
		if (SelectedTopTab != ETopTab::Build)
		{
			return;
		}

		// 동적 버튼을 실제 생성하는 시점에만 soft class를 로드한다. 템플릿이 없으면 부분 UI를 만들지 않고
		// 사용자에게 보일 상태 문구를 오류 색상으로 전환한다.
		const TSubclassOf<UFF_CommonButtonBase> ButtonClass = CategoryButtonClass.LoadSynchronous();
		if (!ButtonClass)
		{
			SetStatusText(LOCTEXT("MissingCategoryTemplate", "대분류 버튼 UMG 템플릿을 불러오지 못했습니다."), true);
			return;
		}

		// 데이터가 비어 있는 분류도 일관된 탐색 구조를 위해 모두 만든다. 비어 있음을 선택 후 콘텐츠에서 안내한다.
		for (const EFFZSFacilityCategory Category : AllCategories)
		{
			UFF_CommonButtonBase* Button = CreateWidget<UFF_CommonButtonBase>(this, ButtonClass);
			if (!Button)
			{
				// 개별 생성 실패는 나머지 분류까지 막지 않는다. 진단 개수는 실제 성공한 위젯만 센다.
				continue;
			}

			// 선택 가능 CommonButton으로 구성하고, 자동 선택 이벤트는 발생시키지 않은 채 현재 상태만 투영한다.
			Button->SetButtonLabel(GetCategoryDisplayName(Category));
			ApplyButtonIconBrush(Button, MakeAtlasIconBrush(GetCategoryAtlasIndex(Category)));
			MakeButtonSurfaceTransparent(Button, TEXT("Surface"));
			Button->SetIsSelectable(true);
			Button->SetIsSelected(SelectedCategory == Category, false);
			Button->SetColorAndOpacity(FLinearColor::White);
			Button->RefreshVisualState();
			Button->OnFFButtonClicked.AddUniqueDynamic(this, &ThisClass::HandleCategoryPressed);
			// 공통 클릭 시그니처에는 분류 값이 없으므로 포인터-분류 맵이 핸들러의 컨텍스트 역할을 한다.
			CategoryByButton.Add(Button, Category);
			if (UHorizontalBoxSlot* CategorySlot = BOX_CategoryButtons->AddChildToHorizontalBox(Button))
			{
				CategorySlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
			}
			++DisplayedCategoryCount;
		}
		return;
	}

	// 안전 폴백 경로: RebuildWidget이 Slate 컨테이너를 아직 만들지 않았다면 저장 상태만 유지한다.
	if (!CategoryBox.IsValid())
	{
		return;
	}

	CategoryBox->ClearChildren();
	DisplayedCategoryCount = 0;
	if (SelectedTopTab != ETopTab::Build)
	{
		return;
	}

	// Slate는 각 OnClicked 델리게이트에 Category 값을 직접 바인딩하므로 별도 역매핑이 필요 없다.
	for (const EFFZSFacilityCategory Category : AllCategories)
	{
		CategoryBox->AddSlot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			SNew(SButton)
			.ContentPadding(FMargin(14.0f, 8.0f))
			.ButtonColorAndOpacity(FLinearColor::Transparent)
			.ForegroundColor(TAttribute<FSlateColor>::Create(
				TAttribute<FSlateColor>::FGetter::CreateUObject(
					this, &UFF_BuildCatalogScreen::GetCategoryColor, Category)))
			.OnClicked_UObject(this, &UFF_BuildCatalogScreen::HandleCategoryClicked, Category)
			[
				SNew(STextBlock).Text(GetCategoryDisplayName(Category))
			]
		];
		++DisplayedCategoryCount;
	}
}

void UFF_BuildCatalogScreen::RefreshContent()
{
	// 운영 경로: WBP의 Build 컨테이너를 선택 분류의 소분류 섹션/카드 계층으로 다시 채운다.
	if (bUsingDesignerWidgetTree && BOX_MinorSections && SWITCHER_Content)
	{
		// 카드 클릭 바인딩을 먼저 해제한 뒤 패널 자식을 비운다. 약참조 목록은 화면이 소유하지 않으므로
		// 유효성 검사 후에만 접근하며, 새로 생성한 카드만 다시 추적한다.
		for (const TWeakObjectPtr<UFF_BuildFacilityCard>& Card : FacilityCards)
		{
			if (Card.IsValid())
			{
				Card->OnFFButtonClicked.RemoveAll(this);
			}
		}
		FacilityCards.Reset();
		BOX_MinorSections->ClearChildren();
		DisplayedFacilityCardCount = 0;

		// Switcher의 0번은 Build 목록, 1번은 WBP에 준비된 Skin 안내 페이지라는 고정 계약이다.
		if (SelectedTopTab == ETopTab::Skin)
		{
			SWITCHER_Content->SetActiveWidgetIndex(1);
			return;
		}
		SWITCHER_Content->SetActiveWidgetIndex(0);

		// 두 템플릿 중 하나라도 없으면 온전한 계층을 만들 수 없으므로 조기 종료한다.
		// 기존 자식은 이미 비워 두어 오래된 카탈로그가 새 상태처럼 남지 않게 한다.
		const TSubclassOf<UFF_BuildMinorSection> SectionClass = MinorSectionClass.LoadSynchronous();
		const TSubclassOf<UFF_BuildFacilityCard> CardClass = FacilityCardClass.LoadSynchronous();
		if (!SectionClass || !CardClass)
		{
			SetStatusText(LOCTEXT("MissingCatalogTemplates", "소분류 또는 시설 카드 UMG 템플릿을 불러오지 못했습니다."), true);
			return;
		}

		bool bAddedAnySection = false;
		// NamingLibrary가 반환하는 정규 소분류 순서를 기준으로 그룹을 만든다. 입력 배열 순서에 따라
		// 섹션 순서가 흔들리지 않으며, 알려지지 않은 토큰은 의도적으로 이 화면에서 노출하지 않는다.
		const TArray<FName> KnownMinorTokens =
			UFF_FacilityNamingLibrary::GetKnownMinorTokens(SelectedCategory);
		for (const FName MinorToken : KnownMinorTokens)
		{
			// CatalogEntries는 이 함수가 끝날 때까지 변하지 않으므로 복사 대신 포인터 목록으로 묶는다.
			// Category와 MinorToken을 모두 비교해 다른 대분류의 같은 이름 토큰이 섞이지 않게 한다.
			TArray<const FFF_BuildCatalogEntry*> MatchingEntries;
			for (const FFF_BuildCatalogEntry& Entry : CatalogEntries)
			{
				if (Entry.Category == SelectedCategory && Entry.MinorToken == MinorToken)
				{
					MatchingEntries.Add(&Entry);
				}
			}
			if (MatchingEntries.IsEmpty())
			{
				continue;
			}

			// 실제 항목이 한 개 이상인 소분류만 섹션으로 생성하여 빈 제목 행을 만들지 않는다.
			UFF_BuildMinorSection* Section = CreateWidget<UFF_BuildMinorSection>(this, SectionClass);
			if (!Section)
			{
				continue;
			}
			Section->SetSectionTitle(GetMinorDisplayName(MinorToken));
			if (UVerticalBoxSlot* SectionSlot = BOX_MinorSections->AddChildToVerticalBox(Section))
			{
				SectionSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
			}

			// 계층은 Screen VerticalBox -> MinorSection WrapBox -> FacilityCard 순이다.
			// 카드가 데이터를 복사해 보관하므로 아래 Entry 포인터는 함수 밖으로 유출되지 않는다.
			for (const FFF_BuildCatalogEntry* Entry : MatchingEntries)
			{
				UFF_BuildFacilityCard* Card = CreateWidget<UFF_BuildFacilityCard>(this, CardClass);
				if (!Card)
				{
					continue;
				}
				Card->Configure(*Entry);
				MakeButtonSurfaceTransparent(Card, TEXT("Card_Surface"));
				// 공통 버튼 클릭을 화면의 시설 선택 델리게이트로 변환하기 위해 카드 자신을 인수로 받는다.
				Card->OnFFButtonClicked.AddUniqueDynamic(this, &ThisClass::HandleFacilityCardPressed);
				Section->AddCard(Card);
				FacilityCards.Add(Card);
				++DisplayedFacilityCardCount;
			}
			bAddedAnySection = true;
		}

		// 선택 분류가 비어 있으면 동적 텍스트를 같은 WidgetTree의 자식으로 생성한다.
		// WidgetTree가 없는 비정상 중간 상태에서는 ConstructWidget을 호출하지 않는 방어 조건이다.
		if (!bAddedAnySection && WidgetTree)
		{
			UCommonTextBlock* EmptyText = WidgetTree->ConstructWidget<UCommonTextBlock>();
			EmptyText->SetText(LOCTEXT("EmptyCategoryUMG", "이 대분류에 등록된 완성형 시설이 없습니다."));
			EmptyText->SetColorAndOpacity(FSlateColor(NativeIdleColor));
			if (UVerticalBoxSlot* EmptySlot = BOX_MinorSections->AddChildToVerticalBox(EmptyText))
			{
				EmptySlot->SetPadding(FMargin(12.0f, 32.0f));
			}
		}
		return;
	}

	// 이하 Slate 폴백은 위와 같은 데이터 규칙을 네이티브 위젯으로 재현한다.
	if (!ContentBox.IsValid())
	{
		return;
	}

	ContentBox->ClearChildren();
	DisplayedFacilityCardCount = 0;
	if (SelectedTopTab == ETopTab::Skin)
	{
		ContentBox->AddSlot()
		.AutoHeight()
		.Padding(12.0f, 32.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SkinComingSoon", "스킨 데이터 모델은 2차 구현 예정입니다."))
			.ColorAndOpacity(NativeIdleColor)
		];
		return;
	}

	bool bAddedAnySection = false;
	// 폴백도 NamingLibrary 순서를 사용해 운영 WBP 경로와 표시 순서를 동일하게 유지한다.
	const TArray<FName> KnownMinorTokens = UFF_FacilityNamingLibrary::GetKnownMinorTokens(SelectedCategory);
	for (const FName MinorToken : KnownMinorTokens)
	{
		TArray<const FFF_BuildCatalogEntry*> MatchingEntries;
		for (const FFF_BuildCatalogEntry& Entry : CatalogEntries)
		{
			if (Entry.Category == SelectedCategory && Entry.MinorToken == MinorToken)
			{
				MatchingEntries.Add(&Entry);
			}
		}
		if (MatchingEntries.IsEmpty())
		{
			continue;
		}

		bAddedAnySection = true;
		ContentBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 10.0f, 0.0f, 6.0f)
		[
			SNew(STextBlock)
			.Text(GetMinorDisplayName(MinorToken))
			.ColorAndOpacity(FLinearColor(0.30f, 0.90f, 0.84f, 1.0f))
		];

		// 한 소분류의 카드는 WrapBox로 묶어 가용 폭에 따라 자동 줄바꿈한다.
		TSharedRef<SWrapBox> Cards = SNew(SWrapBox).UseAllottedSize(true);
		for (const FFF_BuildCatalogEntry* Entry : MatchingEntries)
		{
			Cards->AddSlot()
			.Padding(FMargin(0.0f, 0.0f, 10.0f, 10.0f))
			[
				SNew(SBox)
				.WidthOverride(210.0f)
				.MinDesiredHeight(96.0f)
				[
					SNew(SButton)
					.ContentPadding(FMargin(14.0f, 12.0f))
					.ButtonColorAndOpacity(FLinearColor::Transparent)
					.ToolTipText(UFF_BuildFacilityCard::MakeFacilityCostToolTip(*Entry))
					// Slate 델리게이트에는 값으로 시설 ID를 캡처해 CatalogEntries 포인터 수명에 의존하지 않는다.
					.OnClicked_UObject(this, &UFF_BuildCatalogScreen::HandleFacilityClicked, Entry->FacilityDefinitionId)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(Entry->DisplayName.IsEmpty()
								? FText::FromString(Entry->UniqueNameToken)
								: Entry->DisplayName)
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 8.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(FText::FromName(Entry->FacilityDefinitionId))
							.ColorAndOpacity(NativeIdleColor)
							.AutoWrapText(true)
						]
					]
				]
			];
			++DisplayedFacilityCardCount;
		}

		ContentBox->AddSlot().AutoHeight()[Cards];
	}

	if (!bAddedAnySection)
	{
		ContentBox->AddSlot()
		.AutoHeight()
		.Padding(12.0f, 32.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("EmptyCategory", "이 대분류에 등록된 완성형 시설이 없습니다."))
			.ColorAndOpacity(NativeIdleColor)
		];
	}
}

void UFF_BuildCatalogScreen::RefreshTabStyles()
{
	// Slate 폴백은 TAttribute getter가 SelectedTopTab을 매번 읽으므로 별도 스타일 밀어넣기가 필요 없다.
	if (!bUsingDesignerWidgetTree)
	{
		return;
	}
	// Keep the two non-toggleable CommonButtons mutually exclusive. The helper
	// explicitly clears the inactive tab because SetIsSelected(false) cannot.
	BTN_BuildTab->SetIsSelectable(true);
	BTN_SkinTab->SetIsSelectable(true);
	ApplyExclusiveTabSelection(BTN_BuildTab, SelectedTopTab == ETopTab::Build);
	ApplyExclusiveTabSelection(BTN_SkinTab, SelectedTopTab == ETopTab::Skin);
	BTN_BuildTab->SetColorAndOpacity(FLinearColor::White);
	BTN_SkinTab->SetColorAndOpacity(FLinearColor::White);
}

FSlateBrush UFF_BuildCatalogScreen::MakeAtlasIconBrush(const int32 AtlasIndex) const
{
	FSlateBrush Brush;
	// 에셋 로드 실패나 범위 밖 인덱스에는 빈 브러시를 반환한다. 버튼 생성은 계속되어 텍스트 탐색은 보존된다.
	UTexture2D* Atlas = CategoryIconAtlas.LoadSynchronous();
	if (!Atlas || AtlasIndex < 0 || AtlasIndex >= 10)
	{
		return Brush;
	}

	// 5열 x 2행 정규화 UV에서 셀의 좌상단과 우하단을 계산한다.
	// 이미지 크기 40x40은 버튼이 요구하는 표시 크기이며 원본 아틀라스 픽셀 크기와 독립적이다.
	constexpr int32 ColumnCount = 5;
	constexpr int32 RowCount = 2;
	const int32 Column = AtlasIndex % ColumnCount;
	const int32 Row = AtlasIndex / ColumnCount;
	const FVector2f CellSize(1.0f / ColumnCount, 1.0f / RowCount);
	const FVector2f MinUV(Column * CellSize.X, Row * CellSize.Y);

	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.SetResourceObject(Atlas);
	Brush.SetImageSize(FVector2D(40.0f, 40.0f));
	Brush.SetUVRegion(FBox2f(MinUV, MinUV + CellSize));
	return Brush;
}

int32 UFF_BuildCatalogScreen::GetCategoryAtlasIndex(const EFFZSFacilityCategory Category)
{
	// 0~2번 셀은 상단 버튼이 사용하므로 시설 대분류는 3번부터 명시적으로 대응한다.
	// 알 수 없는 enum 값은 INDEX_NONE을 반환해 MakeAtlasIconBrush가 빈 브러시로 안전하게 처리한다.
	switch (Category)
	{
	case EFFZSFacilityCategory::Booth: return 3;
	case EFFZSFacilityCategory::Toilet: return 4;
	case EFFZSFacilityCategory::Info: return 5;
	case EFFZSFacilityCategory::Rest: return 6;
	case EFFZSFacilityCategory::Performance: return 7;
	case EFFZSFacilityCategory::Safety: return 8;
	case EFFZSFacilityCategory::PhotoSpot: return 9;
	default: return INDEX_NONE;
	}
}

void UFF_BuildCatalogScreen::ApplyStatusText()
{
	// 초기화 시점에 따라 UMG와 Slate 포인터 중 하나 또는 둘 다 없을 수 있다.
	// 저장된 단일 상태를 존재하는 출력 경로 각각에 적용해 경로별 동작 차이를 없앤다.
	if (TXT_Status)
	{
		TXT_Status->SetText(CurrentStatusText);
		TXT_Status->SetColorAndOpacity(FSlateColor(
			bCurrentStatusIsError ? ErrorStatusColor : NormalStatusColor));
	}
	if (StatusTextBlock.IsValid())
	{
		StatusTextBlock->SetText(CurrentStatusText);
		StatusTextBlock->SetColorAndOpacity(FSlateColor(
			bCurrentStatusIsError ? ErrorStatusColor : NormalStatusColor));
	}
}

void UFF_BuildCatalogScreen::SelectFirstPopulatedCategory()
{
	// 명시된 UI 순서대로 첫 항목 보유 분류를 찾는다. 데이터가 전혀 없으면 기존 선택을 유지해
	// 기본 Booth 탭과 빈 상태 문구가 안정적으로 보이도록 한다.
	for (const EFFZSFacilityCategory Category : AllCategories)
	{
		if (CatalogEntries.ContainsByPredicate([Category](const FFF_BuildCatalogEntry& Entry)
		{
			return Entry.Category == Category;
		}))
		{
			SelectedCategory = Category;
			return;
		}
	}
}

void UFF_BuildCatalogScreen::HandleBuildTabPressed(UFF_CommonButtonBase* Button)
{
	// 버튼 인수는 공통 동적 델리게이트 시그니처를 맞추기 위한 값이며 탭은 핸들러 자체로 식별한다.
	// 상태 변경 후 스타일, 대분류 행, 콘텐츠 순으로 갱신해 모든 파생 UI를 같은 프레임에 동기화한다.
	SelectedTopTab = ETopTab::Build;
	RefreshTabStyles();
	RefreshCategoryButtons();
	RefreshContent();
}

void UFF_BuildCatalogScreen::HandleSkinTabPressed(UFF_CommonButtonBase* Button)
{
	// Skin 선택 시 RefreshCategoryButtons는 대분류 행을 비우고 RefreshContent는 Switcher 1번을 활성화한다.
	SelectedTopTab = ETopTab::Skin;
	RefreshTabStyles();
	RefreshCategoryButtons();
	RefreshContent();
}

void UFF_BuildCatalogScreen::HandleClosePressed(UFF_CommonButtonBase* Button)
{
	// 화면은 닫기 의도만 방송한다. CommonUI 스택 pop과 입력 모드 복원은 화면 소유자가 처리한다.
	OnCloseRequested.Broadcast();
}

void UFF_BuildCatalogScreen::HandleCategoryPressed(UFF_CommonButtonBase* Button)
{
	// 런타임 생성 버튼만 맵에 등록된다. 알 수 없는/이미 폐기된 버튼 이벤트는 무시해 잘못된 enum 접근을 막는다.
	if (const EFFZSFacilityCategory* Category = CategoryByButton.Find(Button))
	{
		SelectedCategory = *Category;
		RefreshCategoryButtons();
		RefreshContent();
	}
}

void UFF_BuildCatalogScreen::HandleFacilityCardPressed(UFF_CommonButtonBase* Button)
{
	// 공통 버튼 델리게이트를 실제 카드 타입으로 좁힌 뒤 카드가 보관한 시설 정의 ID만 외부에 노출한다.
	// 캐스팅 실패 시 잘못 연결된 템플릿으로 간주하고 선택 이벤트를 발생시키지 않는다.
	if (const UFF_BuildFacilityCard* Card = Cast<UFF_BuildFacilityCard>(Button))
	{
		OnFacilitySelected.Broadcast(Card->GetFacilityDefinitionId());
	}
}

FReply UFF_BuildCatalogScreen::HandleCloseClicked()
{
	// Slate 폴백도 UMG와 같은 외부 델리게이트를 사용하므로 화면 소유자 관점의 이벤트 흐름은 동일하다.
	OnCloseRequested.Broadcast();
	return FReply::Handled();
}

FReply UFF_BuildCatalogScreen::HandleBuildTabClicked()
{
	// Slate 버튼 색상은 getter attribute가 현재 상태를 읽기 때문에 명시적 RefreshTabStyles 호출이 필요 없다.
	SelectedTopTab = ETopTab::Build;
	RefreshCategoryButtons();
	RefreshContent();
	return FReply::Handled();
}

FReply UFF_BuildCatalogScreen::HandleSkinTabClicked()
{
	SelectedTopTab = ETopTab::Skin;
	RefreshCategoryButtons();
	RefreshContent();
	return FReply::Handled();
}

FReply UFF_BuildCatalogScreen::HandleCategoryClicked(const EFFZSFacilityCategory Category)
{
	// 분류 값은 Slate 델리게이트에 값으로 바인딩되어 별도 버튼-분류 맵 없이 직접 상태를 갱신한다.
	SelectedCategory = Category;
	RefreshCategoryButtons();
	RefreshContent();
	return FReply::Handled();
}

FReply UFF_BuildCatalogScreen::HandleFacilityClicked(const FName FacilityDefinitionId)
{
	// 시설 ID도 버튼 구성 시 값으로 바인딩되므로 카탈로그 배열이 나중에 교체되어도 클릭 처리 중 포인터가 남지 않는다.
	OnFacilitySelected.Broadcast(FacilityDefinitionId);
	return FReply::Handled();
}

FSlateColor UFF_BuildCatalogScreen::GetBuildTabColor() const
{
	// Slate의 TAttribute가 페인트 시점마다 호출하여 별도 스타일 위젯 재생성 없이 선택 색을 반영한다.
	return SelectedTopTab == ETopTab::Build ? NativeSelectedColor : NativeIdleColor;
}

FSlateColor UFF_BuildCatalogScreen::GetSkinTabColor() const
{
	return SelectedTopTab == ETopTab::Skin ? NativeSelectedColor : NativeIdleColor;
}

FSlateColor UFF_BuildCatalogScreen::GetCategoryColor(const EFFZSFacilityCategory Category) const
{
	return SelectedCategory == Category ? NativeSelectedColor : NativeIdleColor;
}

FText UFF_BuildCatalogScreen::GetCategoryDisplayName(const EFFZSFacilityCategory Category)
{
	// enum을 사용자 표시용 현지화 텍스트로 변환한다. 미지원 값도 빈 레이블 대신 "기타"로 식별 가능하게 한다.
	switch (Category)
	{
	case EFFZSFacilityCategory::Booth: return LOCTEXT("CategoryBooth", "부스");
	case EFFZSFacilityCategory::Toilet: return LOCTEXT("CategoryToilet", "화장실");
	case EFFZSFacilityCategory::Info: return LOCTEXT("CategoryInfo", "안내");
	case EFFZSFacilityCategory::Rest: return LOCTEXT("CategoryRest", "휴게");
	case EFFZSFacilityCategory::Performance: return LOCTEXT("CategoryPerformance", "공연");
	case EFFZSFacilityCategory::Safety: return LOCTEXT("CategorySafety", "안전");
	case EFFZSFacilityCategory::PhotoSpot: return LOCTEXT("CategoryPhotoSpot", "포토스팟");
	default: return LOCTEXT("CategoryUnknown", "기타");
	}
}

FText UFF_BuildCatalogScreen::GetMinorDisplayName(const FName MinorToken)
{
	// 시설 이름 파싱에서 사용하는 안정적인 토큰을 현지화 가능한 표시 텍스트로 매핑한다.
	// 새 토큰이 NamingLibrary에 먼저 추가되더라도 원본 이름을 대체 표시해 UI가 공백이 되지 않게 한다.
	static const TMap<FName, FText> Labels = {
		{ TEXT("FoodStall"), LOCTEXT("MinorFoodStall", "먹거리·음료") },
		{ TEXT("MarketGoods"), LOCTEXT("MinorMarketGoods", "상품 판매") },
		{ TEXT("CraftExperience"), LOCTEXT("MinorCraftExperience", "공예·제작 체험") },
		{ TEXT("ActivityGame"), LOCTEXT("MinorActivityGame", "게임·활동 체험") },
		{ TEXT("Nightlife"), LOCTEXT("MinorNightlife", "야간·주점") },
		{ TEXT("Standard"), LOCTEXT("MinorStandard", "일반") },
		{ TEXT("Accessible"), LOCTEXT("MinorAccessible", "장애인 화장실") },
		{ TEXT("Family"), LOCTEXT("MinorFamily", "가족") },
		{ TEXT("Temporary"), LOCTEXT("MinorTemporary", "임시") },
		{ TEXT("GeneralInfo"), LOCTEXT("MinorGeneralInfo", "종합 안내") },
		{ TEXT("Ticketing"), LOCTEXT("MinorTicketing", "티켓") },
		{ TEXT("LostFound"), LOCTEXT("MinorLostFound", "분실물") },
		{ TEXT("Translation"), LOCTEXT("MinorTranslation", "통역") },
		{ TEXT("Seating"), LOCTEXT("MinorSeating", "좌석") },
		{ TEXT("Shade"), LOCTEXT("MinorShade", "그늘") },
		{ TEXT("Picnic"), LOCTEXT("MinorPicnic", "피크닉") },
		{ TEXT("Cooling"), LOCTEXT("MinorCooling", "무더위 쉼터") },
		{ TEXT("MainStage"), LOCTEXT("MinorMainStage", "메인 무대") },
		{ TEXT("SubStage"), LOCTEXT("MinorSubStage", "보조 무대") },
		{ TEXT("Busking"), LOCTEXT("MinorBusking", "버스킹") },
		{ TEXT("ExperienceStage"), LOCTEXT("MinorExperienceStage", "참여형 무대") },
		{ TEXT("FirstAid"), LOCTEXT("MinorFirstAid", "응급 처치") },
		{ TEXT("Security"), LOCTEXT("MinorSecurity", "보안") },
		{ TEXT("FireSafety"), LOCTEXT("MinorFireSafety", "소방") },
		{ TEXT("Evacuation"), LOCTEXT("MinorEvacuation", "대피") },
		{ TEXT("Landmark"), LOCTEXT("MinorLandmark", "랜드마크") },
		{ TEXT("Theme"), LOCTEXT("MinorTheme", "테마") },
		{ TEXT("Scenic"), LOCTEXT("MinorScenic", "경관") },
		{ TEXT("Interactive"), LOCTEXT("MinorInteractive", "상호작용") },
	};

	if (const FText* Label = Labels.Find(MinorToken))
	{
		return *Label;
	}
	return FText::FromName(MinorToken);
}

#undef LOCTEXT_NAMESPACE
