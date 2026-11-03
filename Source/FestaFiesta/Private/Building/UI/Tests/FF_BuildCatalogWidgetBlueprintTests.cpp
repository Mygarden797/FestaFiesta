// FF_BuildCatalogWidgetBlueprintTests.cpp

// 현재 PrepBuild WBP의 BindWidget·팝업과 네이티브 호환 버튼의 동작을 통합 검증한다.
// 흐름: 활성 에셋 경로/부모/WidgetTree 검사 -> 실제 패널 생성 -> 더보기 팝업 열기·닫기.
// 불변식: 저장 콘텐츠를 변경하지 않고 런타임과 같은 클래스·바인딩·클릭 경로를 사용한다.
// 실패/수명: 이름·타입 변경을 null 바인딩 전에 보고하고 Slate 래퍼를 상호작용 검사 끝까지 유지한다.

#include "CoreMinimal.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

// 이 테스트는 C++ 단위 로직만 검사하지 않는다. 에디터에서 생성/저장된 WBP와 텍스처가
// 활성 PrepBuild 클래스의 이름·타입 계약을 확인하고 실제 위젯 이벤트를 실행한다.
// 삭제된 Legacy 에셋을 되살리거나 현재 화면 디자인을 과거 전체화면으로 되돌리지 않는다.

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WrapBox.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/Texture2D.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Building/UI/FF_BuildCatalogScreen.h"
#include "Building/UI/FF_BuildFacilityCard.h"
#include "Building/UI/FF_BuildMinorSection.h"
#include "Building/UI/FF_BuildTopButton.h"
#include "Building/UI/FF_BuildTopButtonStyle.h"
#include "UI/FF_CommonButtonBase.h"
#include "UObject/UnrealType.h"
#include "UI/PrepHUD/Build/FF_PrepBuildCategoryChoice.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"
#include "Building/FacilityCollection/UI/FF_FacilityLauncher.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	// 생성기와 런타임 카드가 사용하는 정확한 패키지 경로이다. 경로 이동이나 에셋 누락도 회귀로 취급한다.
	const TCHAR* IconAtlasPackagePath =
		FFBuildUIAssetPaths::CategoryIconAtlasPackage;
	const TCHAR* FoodTruckThumbnailPackagePath =
		FFBuildUIAssetPaths::FoodTruckImagePackage;

	struct FRequiredWidgetContract
	{
		// WBP 안에 존재해야 하는 디자이너 위젯의 이름과 최소 호환 클래스이다.
		// IsA로 검사하므로 예상 클래스의 Blueprint 파생 타입은 허용한다.
		const TCHAR* Name;
		UClass* ExpectedClass;
	};

	struct FWidgetBlueprintContract
	{
		// 각 WBP의 저장 경로, 반드시 상속해야 할 네이티브 부모, 화면 구조상 필요한/금지된 위젯 목록을 한 묶음으로 표현한다.
		const TCHAR* PackagePath;
		UClass* ExpectedNativeParent;
		TArray<FRequiredWidgetContract> RequiredWidgets;
		TArray<const TCHAR*> ForbiddenWidgets;
	};

	FString MakeAssetObjectPath(const FString& PackagePath)
	{
		// Unreal 패키지 경로를 LoadObject가 요구하는 "Package.Asset" 형식으로 변환한다.
		return FString::Printf(
			TEXT("%s.%s"),
			*PackagePath,
			*FPackageName::GetLongPackageAssetName(PackagePath));
	}

	UTexture2D* CheckUiTextureContract(
		FAutomationTestBase& Test,
		const TCHAR* PackagePath)
	{
		// 단순 로드 성공뿐 아니라 UI 이미지 품질/메모리 정책에 필요한 import 설정까지 검증한다.
		// 반환된 포인터는 뒤의 런타임 브러시 리소스 동일성 검사에 재사용한다.
		const FString AssetObjectPath = MakeAssetObjectPath(PackagePath);
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *AssetObjectPath);
		Test.TestNotNull(
			FString::Printf(TEXT("Reference UI texture loads from exact path: %s"), *AssetObjectPath),
			Texture);
		if (!Texture)
		{
			// nullptr 상태에서 LODGroup/Source를 읽지 않도록 조기 반환하되, 위 TestNotNull 실패는 이미 기록된다.
			return nullptr;
		}

		Test.TestEqual(
			FString::Printf(TEXT("%s uses the UI texture group"), *AssetObjectPath),
			static_cast<TextureGroup>(Texture->LODGroup),
			TEXTUREGROUP_UI);
		Test.TestTrue(
			FString::Printf(TEXT("%s keeps sRGB enabled"), *AssetObjectPath),
			Texture->SRGB);
		Test.TestEqual(
			FString::Printf(TEXT("%s does not generate mipmaps"), *AssetObjectPath),
			static_cast<TextureMipGenSettings>(Texture->MipGenSettings),
			TMGS_NoMipmaps);
		Test.TestTrue(
			FString::Printf(TEXT("%s has non-zero imported dimensions"), *AssetObjectPath),
			Texture->Source.GetSizeX() > 0 && Texture->Source.GetSizeY() > 0);
		return Texture;
	}

	void CheckNativeBindWidgetContract(
		FAutomationTestBase& Test,
		const FWidgetBlueprintContract& Contract,
		const UWidgetTree& WidgetTree)
	{
		// 네이티브 부모와 상위 클래스의 UObject 프로퍼티를 리플렉션으로 순회한다. 이렇게 하면 향후
		// 필수 BindWidget 필드가 C++에 추가될 때 테스트 계약 배열을 갱신하지 않아도 자동으로 검증 대상이 된다.
		for (TFieldIterator<FObjectPropertyBase> PropertyIt(
			Contract.ExpectedNativeParent,
			EFieldIteratorFlags::IncludeSuper);
			PropertyIt;
			++PropertyIt)
		{
			const FObjectPropertyBase* Property = *PropertyIt;
			// BindWidgetOptional/OptionalWidget은 의도적으로 누락을 허용하므로 필수 계약에서 제외한다.
			// "BindWidget" 메타데이터가 함께 잡히는 경우까지 고려해 선택 메타데이터를 명시적으로 거른다.
			if (!Property->HasMetaData(TEXT("BindWidget")) ||
				Property->HasMetaData(TEXT("BindWidgetOptional")) ||
				Property->HasMetaData(TEXT("OptionalWidget")))
			{
				continue;
			}

			// 바인딩은 이름 기반이므로 정확한 프로퍼티 이름으로 컴파일된 템플릿 트리를 검색한다.
			UWidget* Widget = WidgetTree.FindWidget(Property->GetFName());
			Test.TestNotNull(
				FString::Printf(
					TEXT("%s contains required BindWidget '%s'"),
					Contract.PackagePath,
					*Property->GetName()),
				Widget);

			if (Widget)
			{
				Test.TestTrue(
					FString::Printf(
						TEXT("%s.%s has compatible type (expected %s, actual %s)"),
						Contract.PackagePath,
						*Property->GetName(),
						*GetNameSafe(Property->PropertyClass),
						*Widget->GetClass()->GetName()),
					Widget->IsA(Property->PropertyClass));
			}
		}
	}

	void CheckWidgetBlueprintContract(
		FAutomationTestBase& Test,
		const FWidgetBlueprintContract& Contract)
	{
		// 하나의 WBP에 대해 에셋 -> 컴파일 상태 -> GeneratedClass -> 컴파일된 WidgetTree -> 이름/타입 계약
		// 순으로 확인한다. 앞 단계가 없으면 후속 포인터 접근을 중단하고 원인을 직접 설명하는 오류를 남긴다.
		const FString PackagePath(Contract.PackagePath);
		const FString AssetObjectPath = MakeAssetObjectPath(PackagePath);
		const FString GeneratedClassPath = AssetObjectPath + TEXT("_C");

		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetObjectPath);
		if (!Blueprint)
		{
			Test.AddError(FString::Printf(
				TEXT("Required UMG asset could not be loaded: %s. Run the build-catalog WBP builder or create the asset at this exact path."),
				*AssetObjectPath));
			return;
		}

		Test.TestTrue(
			FString::Printf(TEXT("%s has no Blueprint compile error"), *AssetObjectPath),
			Blueprint->Status != BS_Error);

		// 저장된 Blueprint 객체와 실행 가능한 _C 클래스는 별개이므로 둘 다 로드 가능해야 한다.
		UClass* GeneratedClass = LoadClass<UUserWidget>(nullptr, *GeneratedClassPath);
		if (!GeneratedClass)
		{
			Test.AddError(FString::Printf(
				TEXT("UMG generated class could not be loaded: %s. Compile and save the Widget Blueprint."),
				*GeneratedClassPath));
			return;
		}

		Test.TestTrue(
			FString::Printf(
				TEXT("%s derives from %s"),
				*GeneratedClassPath,
				*GetNameSafe(Contract.ExpectedNativeParent)),
			GeneratedClass->IsChildOf(Contract.ExpectedNativeParent));

		// 일반 BlueprintGeneratedClass가 아니라 UMG 전용 클래스인지 확인해야 WidgetTree archetype을 안전하게 얻을 수 있다.
		UWidgetBlueprintGeneratedClass* WidgetGeneratedClass =
			Cast<UWidgetBlueprintGeneratedClass>(GeneratedClass);
		if (!WidgetGeneratedClass)
		{
			Test.AddError(FString::Printf(
				TEXT("Loaded class is not a WidgetBlueprintGeneratedClass: %s"),
				*GeneratedClassPath));
			return;
		}

		// 인스턴스 트리가 아니라 컴파일 결과의 archetype을 검사해 게임 월드/LocalPlayer 없이도 구조 계약을 검증한다.
		const UWidgetTree* WidgetTree = WidgetGeneratedClass->GetWidgetTreeArchetype();
		if (!WidgetTree)
		{
			Test.AddError(FString::Printf(
				TEXT("Widget Blueprint has no compiled WidgetTree: %s"),
				*AssetObjectPath));
			return;
		}

		// 배열에는 디자인 구조상 중요한 선택적 위젯까지 포함할 수 있고, 아래 리플렉션 검사는 네이티브 필수 BindWidget을 보완한다.
		for (const FRequiredWidgetContract& RequiredWidget : Contract.RequiredWidgets)
		{
			UWidget* Widget = WidgetTree->FindWidget(RequiredWidget.Name);
			Test.TestNotNull(
				FString::Printf(
					TEXT("%s contains required widget '%s'"),
					Contract.PackagePath,
					RequiredWidget.Name),
				Widget);
			if (Widget)
			{
				Test.TestTrue(
					FString::Printf(
						TEXT("%s.%s has expected type %s (actual %s)"),
						Contract.PackagePath,
						RequiredWidget.Name,
						*GetNameSafe(RequiredWidget.ExpectedClass),
						*Widget->GetClass()->GetName()),
					Widget->IsA(RequiredWidget.ExpectedClass));
			}
		}
		for (const TCHAR* ForbiddenWidgetName : Contract.ForbiddenWidgets)
		{
			Test.TestNull(
				FString::Printf(
					TEXT("%s does not contain removed widget '%s'"),
					Contract.PackagePath,
					ForbiddenWidgetName),
				WidgetTree->FindWidget(ForbiddenWidgetName));
		}

		CheckNativeBindWidgetContract(Test, Contract, *WidgetTree);
	}

	void CheckNoDrawBrush(
		FAutomationTestBase& Test,
		const TCHAR* BrushName,
		const FSlateBrush& Brush)
	{
		Test.TestEqual(
			FString::Printf(TEXT("Top-button style %s does not draw"), BrushName),
			Brush.DrawAs,
			ESlateBrushDrawType::NoDrawType);
		Test.TestNull(
			FString::Printf(TEXT("Top-button style %s has no resource"), BrushName),
			Brush.GetResourceObject());
	}

	void CheckTransparentTopButtonStyleContract(FAutomationTestBase& Test)
	{
		const UFF_BuildTopButtonStyle* Style = GetDefault<UFF_BuildTopButtonStyle>();
		Test.TestNotNull(TEXT("Native transparent top-button style CDO exists"), Style);
		if (!Style)
		{
			return;
		}

		Test.TestFalse(TEXT("Top-button style does not use a single material"), Style->bSingleMaterial);
		CheckNoDrawBrush(Test, TEXT("NormalBase"), Style->NormalBase);
		CheckNoDrawBrush(Test, TEXT("NormalHovered"), Style->NormalHovered);
		CheckNoDrawBrush(Test, TEXT("NormalPressed"), Style->NormalPressed);
		CheckNoDrawBrush(Test, TEXT("SelectedBase"), Style->SelectedBase);
		CheckNoDrawBrush(Test, TEXT("SelectedHovered"), Style->SelectedHovered);
		CheckNoDrawBrush(Test, TEXT("SelectedPressed"), Style->SelectedPressed);
		CheckNoDrawBrush(Test, TEXT("Disabled"), Style->Disabled);
		Test.TestTrue(TEXT("Top-button style has zero button padding"),
			Style->ButtonPadding == FMargin(0.0f));
		Test.TestTrue(TEXT("Top-button style has zero custom padding"),
			Style->CustomPadding == FMargin(0.0f));

		const UCommonButtonStyle* AppliedStyle = GetDefault<UFF_BuildTopButton>()->GetStyleCDO();
		Test.TestNotNull(TEXT("Top-button native class applies a CommonButton style"), AppliedStyle);
		if (AppliedStyle)
		{
			Test.TestTrue(TEXT("Top-button native class applies the transparent style"),
				AppliedStyle->IsA<UFF_BuildTopButtonStyle>());
		}
	}
}

// The full-screen WBP assets were deliberately retired. Validate their authored
// PrepBuild replacements and keep native compatibility behavior covered separately.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildCatalogWidgetBlueprintContractTest,
    "FestaFiesta.Building.UI.WidgetBlueprintContracts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildCatalogWidgetBlueprintContractTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const TArray<FWidgetBlueprintContract> Contracts = {
        {
            FFBuildUIAssetPaths::BuildPanelPackage, UFF_PrepPanelBuild::StaticClass(),
            {
                {TEXT("Panel_Catalog"), UVerticalBox::StaticClass()},
                {TEXT("Row_V9CatalogHeader"), UHorizontalBox::StaticClass()},
                {TEXT("HorizontalBox_Categories"), UHorizontalBox::StaticClass()},
                {TEXT("Grid_V9Categories"), UUniformGridPanel::StaticClass()},
                {TEXT("Search_Facilities"), UEditableTextBox::StaticClass()},
                {TEXT("TileView_Facilities"), UFF_PrepBuildTileView::StaticClass()},
                {TEXT("Button_V9More"), UButton::StaticClass()},
                {TEXT("Button_V9Close"), UButton::StaticClass()},
                {TEXT("Popup_V9More"), UBorder::StaticClass()},
                {TEXT("Column_V9More"), UVerticalBox::StaticClass()},
                {TEXT("Button_RevenueRanking"), UButton::StaticClass()},
                {TEXT("Widget_FacilityCodexCatalogLauncher"), UFF_FacilityCodexLauncher::StaticClass()},
            },
            {TEXT("Top_Bar"), TEXT("Gold_Size"), TEXT("TXT_Gold")}
        },
        {
            FFBuildUIAssetPaths::BuildTilePackage, UFF_PrepBuildTileWidget::StaticClass(),
            {
                {TEXT("Border_Card"), UBorder::StaticClass()},
                {TEXT("SizeBox_Card"), USizeBox::StaticClass()},
                {TEXT("Image_Thumbnail"), UImage::StaticClass()},
                {TEXT("Text_Name"), UTextBlock::StaticClass()},
                {TEXT("Text_Cost"), UTextBlock::StaticClass()},
                {TEXT("Text_State"), UTextBlock::StaticClass()},
                {TEXT("Button_Favorite"), UButton::StaticClass()},
            }
        },
        {
            FFBuildUIAssetPaths::CategoryChoicePackage, UFF_PrepBuildCategoryChoice::StaticClass(),
            {{TEXT("Text_Label"), UTextBlock::StaticClass()}}
        },
    };
    for (const FWidgetBlueprintContract& Contract : Contracts) CheckWidgetBlueprintContract(*this, Contract);
    CheckUiTextureContract(*this, IconAtlasPackagePath);
    CheckUiTextureContract(*this, FoodTruckThumbnailPackagePath);

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    auto* PanelClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepPanelBuild>(nullptr, FFBuildUIAssetPaths::BuildPanelClass));
    auto* TileClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepBuildTileWidget>(nullptr, FFBuildUIAssetPaths::PrepBuildTileClass));
    const FString CategoryChoiceClassPath = MakeAssetObjectPath(FFBuildUIAssetPaths::CategoryChoicePackage) + TEXT("_C");
    auto* ChoiceClass = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_PrepBuildCategoryChoice>(nullptr, *CategoryChoiceClassPath));
    if (!TestNotNull(TEXT("Editor world for the active catalog contract"), World) ||
        !TestNotNull(TEXT("Current catalog generated class"), PanelClass) ||
        !TestNotNull(TEXT("Current tile generated class"), TileClass) ||
        !TestNotNull(TEXT("Current category generated class"), ChoiceClass)) return false;

    struct FPlayerlessCatalogContractScope
    {
        explicit FPlayerlessCatalogContractScope(UWidgetBlueprintGeneratedClass& InClass)
            : Class(InClass), bPrevious(InClass.bCanCallInitializedWithoutPlayerContext)
        { Class.bCanCallInitializedWithoutPlayerContext = true; }
        ~FPlayerlessCatalogContractScope() { Class.bCanCallInitializedWithoutPlayerContext = bPrevious; }
        UWidgetBlueprintGeneratedClass& Class;
        bool bPrevious;
    };
    FPlayerlessCatalogContractScope PanelScope(*PanelClass), TileScope(*TileClass), ChoiceScope(*ChoiceClass);
    const bool bPanelWasDirty = PanelClass->GetOutermost()->IsDirty();
    const bool bTileWasDirty = TileClass->GetOutermost()->IsDirty();
    TStrongObjectPtr<UFF_PrepPanelBuild> Panel(CreateWidget<UFF_PrepPanelBuild>(World, PanelClass));
    TStrongObjectPtr<UFF_PrepBuildTileWidget> Tile(CreateWidget<UFF_PrepBuildTileWidget>(World, TileClass));
    if (!TestNotNull(TEXT("Current authored catalog instance"), Panel.Get()) ||
        !TestNotNull(TEXT("Current authored tile instance"), Tile.Get())) return false;
    const TSharedRef<SWidget> PanelSlate = Panel->TakeWidget();
    const TSharedRef<SWidget> TileSlate = Tile->TakeWidget();
    auto* List = Cast<UFF_PrepBuildTileView>(Panel->GetWidgetFromName(TEXT("TileView_Facilities")));
    TestTrue(TEXT("The live catalog uses the saved production tile class"), List && List->GetEntryWidgetClass() == TileClass);
    TestTrue(TEXT("The production tile preserves its authored thumbnail-only design"), Tile->IsThumbnailOnly());
    int32 CategoryChoiceCount = 0;
    Panel->WidgetTree->ForEachWidget([&](UWidget* Widget)
    {
        if (const auto* Choice = Cast<UFF_PrepBuildCategoryChoice>(Widget))
        {
            ++CategoryChoiceCount;
            TestTrue(TEXT("Saved category instances use the dedicated production choice WBP"), Choice->IsA(ChoiceClass));
        }
    });
    TestTrue(TEXT("The active catalog instantiates its authored category controls"), CategoryChoiceCount > 0);
    auto* Popup = Cast<UBorder>(Panel->GetWidgetFromName(TEXT("Popup_V9More")));
    auto* More = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_V9More")));
    if (!TestNotNull(TEXT("The authored More popup exists at runtime"), Popup) ||
        !TestNotNull(TEXT("The authored More button exists at runtime"), More)) return false;
    TestEqual(TEXT("NativeConstruct keeps additional catalog actions closed initially"), Popup->GetVisibility(), ESlateVisibility::Collapsed);
    More->OnClicked.Broadcast();
    TestEqual(TEXT("The real button binding opens the additional actions"), Popup->GetVisibility(), ESlateVisibility::Visible);
    More->OnClicked.Broadcast();
    TestEqual(TEXT("The same real button binding closes the additional actions"), Popup->GetVisibility(), ESlateVisibility::Collapsed);
    // Keyboard routing belongs to the surviving native compatibility class,
    // independently of the deliberately retired full-screen Widget Blueprint.
    TStrongObjectPtr<UFF_BuildCatalogScreen> NativeScreen(
        CreateWidget<UFF_BuildCatalogScreen>(World, UFF_BuildCatalogScreen::StaticClass()));
    if (!TestNotNull(TEXT("Native catalog for keyboard routing"), NativeScreen.Get())) return false;
    const TSharedRef<SWidget> NativeScreenSlate = NativeScreen->TakeWidget();
    int32 BuildModeRequests = 0;
    const FDelegateHandle BuildModeHandle = NativeScreen->OnBuildModeKeyRequested.AddLambda(
        [&BuildModeRequests]() { ++BuildModeRequests; });
    TestTrue(TEXT("Focused native catalog handles B"), NativeScreen->NativeOnPreviewKeyDown(
        FGeometry(), FKeyEvent(EKeys::B, FModifierKeysState(), 0, false, 0, 0)).IsEventHandled());
    TestEqual(TEXT("B requests BuildMode exactly once"), BuildModeRequests, 1);
    NativeScreen->NativeOnPreviewKeyDown(
        FGeometry(), FKeyEvent(EKeys::B, FModifierKeysState(), 0, true, 0, 0));
    TestEqual(TEXT("Holding B does not request BuildMode again"), BuildModeRequests, 1);
    NativeScreen->OnBuildModeKeyRequested.Remove(BuildModeHandle);

    int32 PauseMenuRequests = 0;
    const FDelegateHandle PauseMenuHandle = NativeScreen->OnPauseMenuKeyRequested.AddLambda(
        [&PauseMenuRequests]() { ++PauseMenuRequests; });
    for (const FKey& PauseKey : {EKeys::Escape, EKeys::F5})
    {
        const int32 RequestsBeforePress = PauseMenuRequests;
        TestTrue(FString::Printf(TEXT("Focused native catalog handles %s"), *PauseKey.ToString()),
            NativeScreen->NativeOnPreviewKeyDown(FGeometry(),
                FKeyEvent(PauseKey, FModifierKeysState(), 0, false, 0, 0)).IsEventHandled());
        TestEqual(FString::Printf(TEXT("%s requests Pause Hub exactly once"), *PauseKey.ToString()),
            PauseMenuRequests, RequestsBeforePress + 1);
        for (int32 Repeat = 0; Repeat < 2; ++Repeat)
            NativeScreen->NativeOnPreviewKeyDown(FGeometry(),
                FKeyEvent(PauseKey, FModifierKeysState(), 0, true, 0, 0));
        TestEqual(FString::Printf(TEXT("Holding %s does not request Pause Hub again"), *PauseKey.ToString()),
            PauseMenuRequests, RequestsBeforePress + 1);
    }
    const int32 RequestsBeforeRetiredKeys = PauseMenuRequests;
    for (const FKey& RetiredKey : {EKeys::F1, EKeys::F2, EKeys::F3, EKeys::F4, EKeys::F6, EKeys::F7})
    {
        TestTrue(FString::Printf(TEXT("Focused native catalog consumes retired %s"), *RetiredKey.ToString()),
            NativeScreen->NativeOnPreviewKeyDown(FGeometry(),
                FKeyEvent(RetiredKey, FModifierKeysState(), 0, false, 0, 0)).IsEventHandled());
        TestEqual(TEXT("Retired toolbar keys never request Pause Hub"), PauseMenuRequests, RequestsBeforeRetiredKeys);
    }
    NativeScreen->OnPauseMenuKeyRequested.Remove(PauseMenuHandle);

    TestEqual(TEXT("Using the catalog preserves its saved package"), PanelClass->GetOutermost()->IsDirty(), bPanelWasDirty);
    TestEqual(TEXT("Instantiating the virtualized tile preserves its saved package"), TileClass->GetOutermost()->IsDirty(), bTileWasDirty);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildCatalogTransparentTopButtonsTest,
    "FestaFiesta.Building.UI.TransparentTopButtons",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildCatalogTransparentTopButtonsTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    // Bind a transient indicator as a generated WidgetTree would do, without
    // restoring the retired Build/Skin screen or changing the production panel.
    CheckTransparentTopButtonStyleContract(*this);
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world for native transparent button contract"), World)) return false;
    TStrongObjectPtr<UFF_BuildTopButton> Button(CreateWidget<UFF_BuildTopButton>(World, UFF_BuildTopButton::StaticClass()));
    if (!TestNotNull(TEXT("Native transparent button can be instantiated"), Button.Get()) ||
        !TestNotNull(TEXT("Native button initializes a transient WidgetTree"), Button->WidgetTree.Get())) return false;
    auto* Content = Button->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TransientTopContent"));
    auto* Label = Button->WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("Text_Label"));
    auto* Indicator = Button->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_SelectedIndicator"));
    Button->WidgetTree->RootWidget = Content;
    Content->AddChild(Label);
    Content->AddChild(Indicator);
    auto* LabelProperty = FindFProperty<FObjectPropertyBase>(Button->GetClass(), TEXT("Text_Label"));
    auto* IndicatorProperty = FindFProperty<FObjectPropertyBase>(Button->GetClass(), TEXT("Image_SelectedIndicator"));
    if (!TestNotNull(TEXT("Native button retains its label binding"), LabelProperty) ||
        !TestNotNull(TEXT("Native button retains its selection-indicator binding"), IndicatorProperty)) return false;
    LabelProperty->SetObjectPropertyValue_InContainer(Button.Get(), Label);
    IndicatorProperty->SetObjectPropertyValue_InContainer(Button.Get(), Indicator);
    const TSharedRef<SWidget> ButtonSlate = Button->TakeWidget();
    TestTrue(TEXT("Native button retains the transparent CommonUI style"),
        Button->GetStyleCDO() && Button->GetStyleCDO()->IsA<UFF_BuildTopButtonStyle>());
    Button->SetIsSelectable(true);
    Button->SetIsSelected(true, false);
    TestTrue(TEXT("The native compatibility button becomes selected"), Button->GetSelected());
    TestEqual(TEXT("Selection shows only the indicator"), Indicator->GetVisibility(), ESlateVisibility::HitTestInvisible);
    Button->ClearSelection();
    TestFalse(TEXT("The native compatibility button clears its selection"), Button->GetSelected());
    TestEqual(TEXT("Deselection hides the indicator without changing layout"), Indicator->GetVisibility(), ESlateVisibility::Hidden);
    return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
