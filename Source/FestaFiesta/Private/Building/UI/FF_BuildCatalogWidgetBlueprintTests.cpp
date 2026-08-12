// FF_BuildCatalogWidgetBlueprintTests.cpp

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

// 이 테스트는 C++ 단위 로직만 검사하지 않는다. 에디터에서 생성/저장된 WBP와 텍스처가
// 다섯 네이티브 클래스의 이름·타입 계약을 실제로 만족하는지 확인하고, 다섯 생성 클래스를 인스턴스화해
// 런타임 동적 섹션/카드 생성까지 이어지는 통합 스모크 경로를 검증한다.

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

namespace
{
	// 생성기와 런타임 카드가 사용하는 정확한 패키지 경로이다. 경로 이동이나 에셋 누락도 회귀로 취급한다.
	const TCHAR* IconAtlasPackagePath =
		TEXT("/Game/FestaFiesta/Zone/Test_Asset/Image/T_UI_BuildCatalog_IconAtlas");
	const TCHAR* FoodTruckThumbnailPackagePath =
		TEXT("/Game/FestaFiesta/Zone/Test_Asset/Image/T_UI_BuildCatalog_FoodTruck");

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_BuildCatalogWidgetBlueprintContractTest,
	"FestaFiesta.Building.UI.WidgetBlueprintContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildCatalogWidgetBlueprintContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// 생성기가 책임지는 다섯 WBP의 정적 계약이다. 각 항목은 정확한 에셋 경로와 네이티브 부모를 고정하고,
	// BindWidget 여부와 별개로 기준 디자인 계층에서 반드시 살아 있어야 하는 핵심 위젯도 열거한다.
	const TArray<FWidgetBlueprintContract> Contracts = {
		{
			TEXT("/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildCatalogScreen"),
			UFF_BuildCatalogScreen::StaticClass(),
			{
				{ TEXT("Background_Dim"), UBorder::StaticClass() },
				{ TEXT("Catalog_Panel"), UBorder::StaticClass() },
				{ TEXT("Top_Bar"), UHorizontalBox::StaticClass() },
				{ TEXT("BTN_BuildTab"), UFF_BuildTopButton::StaticClass() },
				{ TEXT("BTN_SkinTab"), UFF_BuildTopButton::StaticClass() },
				{ TEXT("BTN_Close"), UFF_BuildTopButton::StaticClass() },
				{ TEXT("BOX_CategoryButtons"), UHorizontalBox::StaticClass() },
				{ TEXT("Divider"), UBorder::StaticClass() },
				{ TEXT("SWITCHER_Content"), UWidgetSwitcher::StaticClass() },
				{ TEXT("Build_Scroll"), UScrollBox::StaticClass() },
				{ TEXT("BOX_MinorSections"), UVerticalBox::StaticClass() },
				{ TEXT("Status_Surface"), UBorder::StaticClass() },
				{ TEXT("TXT_Status"), UCommonTextBlock::StaticClass() },
			},
			{ TEXT("Background_Blur") }
		},
		{
			TEXT("/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildTopButton"),
			UFF_BuildTopButton::StaticClass(),
			{
				{ TEXT("Text_Label"), UCommonTextBlock::StaticClass() },
				{ TEXT("Image_Icon"), UImage::StaticClass() },
				{ TEXT("Image_SelectedIndicator"), UImage::StaticClass() },
				{ TEXT("Image_FocusRing"), UImage::StaticClass() },
			}
		},
		{
			TEXT("/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildCategoryButton"),
			UFF_CommonButtonBase::StaticClass(),
			{
				{ TEXT("Surface"), UBorder::StaticClass() },
				{ TEXT("Text_Label"), UCommonTextBlock::StaticClass() },
				{ TEXT("Image_Icon"), UImage::StaticClass() },
			}
		},
		{
			TEXT("/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildMinorSection"),
			UFF_BuildMinorSection::StaticClass(),
			{
				{ TEXT("Text_MinorTitle"), UCommonTextBlock::StaticClass() },
				{ TEXT("Box_Cards"), UWrapBox::StaticClass() },
			}
		},
		{
			TEXT("/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildFacilityCard"),
			UFF_BuildFacilityCard::StaticClass(),
			{
				{ TEXT("Card_Surface"), UBorder::StaticClass() },
				{ TEXT("Text_Label"), UCommonTextBlock::StaticClass() },
				{ TEXT("Text_DefinitionId"), UCommonTextBlock::StaticClass() },
				{ TEXT("Image_Thumbnail"), UImage::StaticClass() },
			}
		},
	};

	// 한 에셋 실패가 나머지 진단을 막지 않도록 모든 계약을 순회해 한 번의 테스트에서 전체 오류를 수집한다.
	for (const FWidgetBlueprintContract& Contract : Contracts)
	{
		CheckWidgetBlueprintContract(*this, Contract);
	}
	CheckTransparentTopButtonStyleContract(*this);

	// 텍스처 import 계약을 먼저 확인하고, 성공한 포인터는 이후 실제 Image 브러시가 같은 리소스를 쓰는지 비교한다.
	UTexture2D* IconAtlas = CheckUiTextureContract(*this, IconAtlasPackagePath);
	UTexture2D* FoodTruckThumbnail =
		CheckUiTextureContract(*this, FoodTruckThumbnailPackagePath);

	// 여기부터는 정적 archetype 검사를 넘어 실제 WidgetTree 초기화와 데이터 바인딩을 실행하는 스모크 테스트다.
	const TCHAR* ScreenClassPath =
		TEXT("/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildCatalogScreen.WBP_FF_BuildCatalogScreen_C");
	UClass* ScreenClass = LoadClass<UFF_BuildCatalogScreen>(nullptr, ScreenClassPath);
	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (ScreenClass && EditorWorld)
	{
		// 에디터/Commandlet 자동화에는 LocalPlayer가 없지만 UserWidget은 기본적으로 플레이어 컨텍스트를 요구한다.
		// 다섯 생성 클래스에 한해 playerless 초기화를 임시 허용하여 PIE 세션 없이도 NativeOnInitialized와
		// 동일한 WidgetTree/데이터 채우기 경로를 실행한다. 테스트가 끝나면 반드시 원래 false로 복원한다.
		TArray<UWidgetBlueprintGeneratedClass*> PlayerlessClasses;
		const TCHAR* GeneratedClassPaths[] = {
			ScreenClassPath,
			TEXT("/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildTopButton.WBP_FF_BuildTopButton_C"),
			TEXT("/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildCategoryButton.WBP_FF_BuildCategoryButton_C"),
			TEXT("/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildMinorSection.WBP_FF_BuildMinorSection_C"),
			TEXT("/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildFacilityCard.WBP_FF_BuildFacilityCard_C"),
		};
		for (const TCHAR* GeneratedClassPath : GeneratedClassPaths)
		{
			// 일부 보조 클래스 로드가 실패해도 앞선 계약 검사가 별도 오류를 제공하므로, 로드된 클래스만 임시 변경한다.
			if (UWidgetBlueprintGeneratedClass* GeneratedClass =
				Cast<UWidgetBlueprintGeneratedClass>(LoadObject<UClass>(nullptr, GeneratedClassPath)))
			{
				GeneratedClass->bCanCallInitializedWithoutPlayerContext = true;
				PlayerlessClasses.Add(GeneratedClass);
			}
		}

		// 화면 클래스의 CreateWidget부터 시작해 실제 인스턴스 수명주기를 확인한다.
		UFF_BuildCatalogScreen* Screen = CreateWidget<UFF_BuildCatalogScreen>(EditorWorld, ScreenClass);
		TestNotNull(TEXT("Generated build catalog screen can be instantiated"), Screen);
		if (Screen)
		{
			// TakeWidget이 RebuildWidget을 실행한다. 생성된 WBP RootWidget이 있다면 네이티브 Slate 폴백이 아니라
			// 디자이너 WidgetTree 경로를 선택해야 하므로 진단 API로 이를 명시적으로 검증한다.
			// Keep the Slate wrapper alive while exercising runtime bindings. Dropping
			// this reference immediately invokes NativeDestruct and would turn the
			// interaction checks below into tests of a detached widget tree.
			const TSharedRef<SWidget> ScreenSlateWidget = Screen->TakeWidget();
			TestTrue(TEXT("Generated screen uses its Designer WidgetTree"), Screen->IsUsingDesignerWidgetTree());

			USizeBox* GoldSize = Cast<USizeBox>(Screen->GetWidgetFromName(TEXT("Gold_Size")));
			UCommonTextBlock* GoldText = Cast<UCommonTextBlock>(
				Screen->GetWidgetFromName(TEXT("TXT_Gold")));
			TestNotNull(TEXT("Runtime-compatible screen provides the top-bar Gold panel"), GoldSize);
			TestNotNull(TEXT("Runtime-compatible screen provides the current Gold text"), GoldText);
			if (GoldSize)
			{
				TestTrue(TEXT("Gold panel has an explicit width"), GoldSize->IsWidthOverride());
				TestEqual(TEXT("Gold panel occupies the requested top-right width"),
					GoldSize->GetWidthOverride(), 320.0f);
			}
			if (GoldText)
			{
				TestEqual(TEXT("A playerless screen does not invent a Gold balance"),
					GoldText->GetText().ToString(), FString(TEXT("—")));
			}
			if (UHorizontalBox* TopBar = Cast<UHorizontalBox>(
				Screen->GetWidgetFromName(TEXT("Top_Bar")));
				TopBar && GoldSize)
			{
				UWidget* CloseButton = Screen->GetWidgetFromName(TEXT("BTN_Close"));
				if (CloseButton)
				{
					TestEqual(TEXT("Gold panel is immediately before the top-right Close button"),
						TopBar->GetChildIndex(GoldSize), TopBar->GetChildIndex(CloseButton) - 1);
				}
			}

			bool bBuildModeKeyRequested = false;
			const FDelegateHandle BuildModeKeyHandle = Screen->OnBuildModeKeyRequested.AddLambda(
				[&bBuildModeKeyRequested]() { bBuildModeKeyRequested = true; });
			const FKeyEvent BuildModeKeyEvent(
				EKeys::B, FModifierKeysState(), 0, false, 0, 0);
			const FReply BuildModeKeyReply = Screen->NativeOnPreviewKeyDown(
				FGeometry(), BuildModeKeyEvent);
			TestTrue(TEXT("Focused catalog handles B"), BuildModeKeyReply.IsEventHandled());
			TestTrue(TEXT("Focused catalog requests BuildMode on B"), bBuildModeKeyRequested);
			Screen->OnBuildModeKeyRequested.Remove(BuildModeKeyHandle);

			bool bExitBuildModeKeyRequested = false;
			const FDelegateHandle ExitBuildModeKeyHandle = Screen->OnExitBuildModeKeyRequested.AddLambda(
				[&bExitBuildModeKeyRequested]() { bExitBuildModeKeyRequested = true; });
			const FKeyEvent ExitBuildModeKeyEvent(
				EKeys::Escape, FModifierKeysState(), 0, false, 0, 0);
			const FReply ExitBuildModeKeyReply = Screen->NativeOnPreviewKeyDown(
				FGeometry(), ExitBuildModeKeyEvent);
			TestTrue(TEXT("Focused catalog handles Escape"), ExitBuildModeKeyReply.IsEventHandled());
			TestTrue(TEXT("Focused catalog requests session exit on Escape"), bExitBuildModeKeyRequested);
			Screen->OnExitBuildModeKeyRequested.Remove(ExitBuildModeKeyHandle);

			const TCHAR* TopButtonNames[] = {
				TEXT("BTN_BuildTab"),
				TEXT("BTN_SkinTab"),
				TEXT("BTN_Close"),
			};
			for (const TCHAR* TopButtonName : TopButtonNames)
			{
				UFF_BuildTopButton* TopButton = Cast<UFF_BuildTopButton>(
					Screen->GetWidgetFromName(TopButtonName));
				TestNotNull(
					FString::Printf(TEXT("%s uses the dedicated transparent top-button class"), TopButtonName),
					TopButton);
				if (!TopButton)
				{
					continue;
				}

				TestNull(
					FString::Printf(TEXT("%s has no colored Surface layer"), TopButtonName),
					TopButton->GetWidgetFromName(TEXT("Surface")));
				const UCommonButtonStyle* AppliedStyle = TopButton->GetStyleCDO();
				TestTrue(
					FString::Printf(TEXT("%s uses the native transparent style"), TopButtonName),
					AppliedStyle && AppliedStyle->IsA<UFF_BuildTopButtonStyle>());

				UImage* TopIcon = Cast<UImage>(
					TopButton->GetWidgetFromName(TEXT("Image_Icon")));
				TestNotNull(
					FString::Printf(TEXT("%s binds Image_Icon"), TopButtonName),
					TopIcon);
				if (TopIcon && IconAtlas)
				{
					TestEqual(
						FString::Printf(TEXT("%s loads the transparent icon atlas"), TopButtonName),
						TopIcon->GetBrush().GetResourceObject(),
						static_cast<UObject*>(IconAtlas));
				}
			}

			UFF_BuildTopButton* BuildTab = Cast<UFF_BuildTopButton>(
				Screen->GetWidgetFromName(TEXT("BTN_BuildTab")));
			UFF_BuildTopButton* SkinTab = Cast<UFF_BuildTopButton>(
				Screen->GetWidgetFromName(TEXT("BTN_SkinTab")));
			if (BuildTab && SkinTab)
			{
				TestTrue(TEXT("Build tab starts selected"), BuildTab->GetSelected());
				TestFalse(TEXT("Skin tab starts deselected"), SkinTab->GetSelected());
				UImage* BuildSelectedIndicator = Cast<UImage>(
					BuildTab->GetWidgetFromName(TEXT("Image_SelectedIndicator")));
				UImage* SkinSelectedIndicator = Cast<UImage>(
					SkinTab->GetWidgetFromName(TEXT("Image_SelectedIndicator")));
				TestNotNull(TEXT("Build tab binds selected indicator"), BuildSelectedIndicator);
				TestNotNull(TEXT("Skin tab binds selected indicator"), SkinSelectedIndicator);
				if (BuildSelectedIndicator && SkinSelectedIndicator)
				{
					TestEqual(TEXT("Selected Build tab shows its underline"),
						BuildSelectedIndicator->GetVisibility(), ESlateVisibility::HitTestInvisible);
					TestEqual(TEXT("Deselected Skin tab hides its underline"),
						SkinSelectedIndicator->GetVisibility(), ESlateVisibility::Hidden);
				}

				SkinTab->OnFFButtonClicked.Broadcast(SkinTab);
				TestFalse(TEXT("Selecting Skin clears Build selection"), BuildTab->GetSelected());
				TestTrue(TEXT("Selecting Skin activates only Skin"), SkinTab->GetSelected());
				if (BuildSelectedIndicator && SkinSelectedIndicator)
				{
					TestEqual(TEXT("Deselected Build tab hides its underline"),
						BuildSelectedIndicator->GetVisibility(), ESlateVisibility::Hidden);
					TestEqual(TEXT("Selected Skin tab shows its underline"),
						SkinSelectedIndicator->GetVisibility(), ESlateVisibility::HitTestInvisible);
				}
				if (UWidgetSwitcher* ContentSwitcher = Cast<UWidgetSwitcher>(
					Screen->GetWidgetFromName(TEXT("SWITCHER_Content"))))
				{
					TestEqual(TEXT("Selecting Skin opens only the Skin page"),
						ContentSwitcher->GetActiveWidgetIndex(), 1);
				}

				BuildTab->OnFFButtonClicked.Broadcast(BuildTab);
				TestTrue(TEXT("Selecting Build activates Build"), BuildTab->GetSelected());
				TestFalse(TEXT("Selecting Build clears Skin selection"), SkinTab->GetSelected());
				if (BuildSelectedIndicator && SkinSelectedIndicator)
				{
					TestEqual(TEXT("Returning to Build shows only the Build underline"),
						BuildSelectedIndicator->GetVisibility(), ESlateVisibility::HitTestInvisible);
					TestEqual(TEXT("Returning to Build hides the Skin underline"),
						SkinSelectedIndicator->GetVisibility(), ESlateVisibility::Hidden);
				}
				if (UWidgetSwitcher* ContentSwitcher = Cast<UWidgetSwitcher>(
					Screen->GetWidgetFromName(TEXT("SWITCHER_Content"))))
				{
					TestEqual(TEXT("Selecting Build opens only the Build page"),
						ContentSwitcher->GetActiveWidgetIndex(), 0);
				}
			}

			// 단 하나의 Booth/FoodStall 항목을 주입해 대분류 전체 생성, 소분류 그룹화,
			// 시설 카드 데이터/기본 썸네일 바인딩을 결정론적으로 검증한다.
			FFF_BuildCatalogEntry Entry;
			Entry.FacilityDefinitionId = TEXT("Booth_FoodStall_AutomationCard");
			Entry.DisplayName = FText::FromString(TEXT("Automation Card"));
			Entry.Category = EFFZSFacilityCategory::Booth;
			Entry.MinorToken = TEXT("FoodStall");
			Entry.UniqueNameToken = TEXT("AutomationCard");
			Screen->SetCatalogEntries({ Entry });

			// 진단 카운터는 실제 성공적으로 생성된 위젯만 세므로 템플릿 로드나 CreateWidget 실패도 감지한다.
			TestEqual(TEXT("Designer screen creates seven major-category buttons"),
				Screen->GetDisplayedCategoryCount(), 7);
			TestEqual(TEXT("Designer screen creates one facility card from one entry"),
				Screen->GetDisplayedFacilityCardCount(), 1);

			// 숫자만 맞는 잘못된 자식도 통과하지 않도록 실제 트리를 내려가 각 대분류 버튼의 타입과 아이콘 바인딩을 확인한다.
			UHorizontalBox* CategoryButtons = Cast<UHorizontalBox>(
				Screen->GetWidgetFromName(TEXT("BOX_CategoryButtons")));
			TestNotNull(TEXT("Designer screen exposes the category-button row"), CategoryButtons);
			if (CategoryButtons)
			{
				TestEqual(TEXT("Category row contains seven button widgets"),
					CategoryButtons->GetChildrenCount(), 7);
				for (int32 Index = 0; Index < CategoryButtons->GetChildrenCount(); ++Index)
				{
					UFF_CommonButtonBase* CategoryButton =
						Cast<UFF_CommonButtonBase>(CategoryButtons->GetChildAt(Index));
					TestNotNull(
						FString::Printf(TEXT("Category child %d uses FF common button"), Index),
						CategoryButton);
					if (!CategoryButton)
					{
						continue;
					}

					UBorder* CategorySurface = Cast<UBorder>(
						CategoryButton->GetWidgetFromName(TEXT("Surface")));
					TestNotNull(
						FString::Printf(TEXT("Category child %d binds Surface"), Index),
						CategorySurface);
					if (CategorySurface)
					{
						TestEqual(
							FString::Printf(TEXT("Category child %d has a transparent background"), Index),
							CategorySurface->GetBrushColor().A, 0.0f);
					}
					const UCommonButtonStyle* CategoryStyle = CategoryButton->GetStyleCDO();
					TestTrue(
						FString::Printf(TEXT("Category child %d uses the no-draw button style"), Index),
						CategoryStyle && CategoryStyle->IsA<UFF_BuildTopButtonStyle>());

					UImage* CategoryIcon = Cast<UImage>(
						CategoryButton->GetWidgetFromName(TEXT("Image_Icon")));
					TestNotNull(
						FString::Printf(TEXT("Category child %d binds Image_Icon"), Index),
						CategoryIcon);
					// 앞선 텍스처 로드가 성공했을 때만 리소스 동일성을 비교해 하나의 누락이 연쇄 nullptr 실패를 만들지 않게 한다.
					if (CategoryIcon && IconAtlas)
					{
						TestEqual(
							FString::Printf(TEXT("Category child %d loads the icon atlas"), Index),
							CategoryIcon->GetBrush().GetResourceObject(),
							static_cast<UObject*>(IconAtlas));
					}
				}
			}

			// 데이터 바인딩 결과의 실제 계층(Screen -> MinorSection -> WrapBox -> FacilityCard -> Image)을 순서대로 검사한다.
			// 각 단계는 직전 포인터/자식 수를 가드해 실패해도 크래시 없이 구체적인 TestNotNull 진단을 남긴다.
			UVerticalBox* MinorSections = Cast<UVerticalBox>(
				Screen->GetWidgetFromName(TEXT("BOX_MinorSections")));
			TestNotNull(TEXT("Designer screen exposes the minor-section container"), MinorSections);
			if (MinorSections && MinorSections->GetChildrenCount() > 0)
			{
				UFF_BuildMinorSection* MinorSection =
					Cast<UFF_BuildMinorSection>(MinorSections->GetChildAt(0));
				TestNotNull(TEXT("Catalog entry creates a minor-section widget"), MinorSection);
				UWrapBox* CardBox = MinorSection
					? Cast<UWrapBox>(MinorSection->GetWidgetFromName(TEXT("Box_Cards")))
					: nullptr;
				TestNotNull(TEXT("Minor section exposes its card container"), CardBox);
				UFF_BuildFacilityCard* FacilityCard =
					CardBox && CardBox->GetChildrenCount() > 0
					? Cast<UFF_BuildFacilityCard>(CardBox->GetChildAt(0))
					: nullptr;
				TestNotNull(TEXT("Minor section contains the generated facility card"), FacilityCard);
				UBorder* CardSurface = FacilityCard
					? Cast<UBorder>(FacilityCard->GetWidgetFromName(TEXT("Card_Surface")))
					: nullptr;
				TestNotNull(TEXT("Facility card binds Card_Surface"), CardSurface);
				if (CardSurface)
				{
					TestEqual(TEXT("Facility card has a transparent background"),
						CardSurface->GetBrushColor().A, 0.0f);
				}
				const UCommonButtonStyle* CardStyle = FacilityCard ? FacilityCard->GetStyleCDO() : nullptr;
				TestTrue(TEXT("Facility card uses the no-draw button style"),
					CardStyle && CardStyle->IsA<UFF_BuildTopButtonStyle>());
				UImage* ThumbnailImage = FacilityCard
					? Cast<UImage>(FacilityCard->GetWidgetFromName(TEXT("Image_Thumbnail")))
					: nullptr;
				TestNotNull(TEXT("Facility card binds Image_Thumbnail"), ThumbnailImage);
				// Entry.Thumbnail을 비워 두었으므로 카드의 방어적 기본 이미지 경로가 실제 브러시까지 적용되어야 한다.
				if (ThumbnailImage && FoodTruckThumbnail)
				{
					TestEqual(
						TEXT("Facility card uses the generated default thumbnail when metadata is empty"),
						ThumbnailImage->GetBrush().GetResourceObject(),
						static_cast<UObject*>(FoodTruckThumbnail));
				}
			}
		}
		// GeneratedClass는 전역 에셋 객체이므로 임시 플래그를 남기면 이후 테스트/에디터 동작에 영향을 준다.
		// 화면 생성 성공 여부와 관계없이 로드해 변경한 모든 클래스의 기본 정책을 복원한다.
		for (UWidgetBlueprintGeneratedClass* GeneratedClass : PlayerlessClasses)
		{
			GeneratedClass->bCanCallInitializedWithoutPlayerContext = false;
		}
	}
	else
	{
		// 스모크 검증의 전제(Screen _C와 EditorWorld)가 없으면 조용히 건너뛰지 않고 환경/에셋 문제를 실패로 노출한다.
		AddError(TEXT("Could not load the generated screen class or acquire the editor world for runtime UMG smoke validation."));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_BuildCatalogTransparentTopButtonsTest,
	"FestaFiesta.Building.UI.TransparentTopButtons",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildCatalogTransparentTopButtonsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// Keep this regression test focused on the top-bar migration. The broader reference-layout
	// contract may legitimately report unrelated designer drift elsewhere in the catalog screen.
	CheckWidgetBlueprintContract(*this, {
		TEXT("/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildCatalogScreen"),
		UFF_BuildCatalogScreen::StaticClass(),
		{
			{ TEXT("BTN_BuildTab"), UFF_BuildTopButton::StaticClass() },
			{ TEXT("BTN_SkinTab"), UFF_BuildTopButton::StaticClass() },
			{ TEXT("BTN_Close"), UFF_BuildTopButton::StaticClass() },
		}
	});
	CheckWidgetBlueprintContract(*this, {
		TEXT("/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildTopButton"),
		UFF_BuildTopButton::StaticClass(),
		{
			{ TEXT("Text_Label"), UCommonTextBlock::StaticClass() },
			{ TEXT("Image_Icon"), UImage::StaticClass() },
			{ TEXT("Image_SelectedIndicator"), UImage::StaticClass() },
			{ TEXT("Image_FocusRing"), UImage::StaticClass() },
		}
	});
	CheckTransparentTopButtonStyleContract(*this);

	const FString TopButtonObjectPath = MakeAssetObjectPath(
		TEXT("/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildTopButton"));
	UClass* TopButtonClass = LoadClass<UFF_BuildTopButton>(
		nullptr, *(TopButtonObjectPath + TEXT("_C")));
	if (const UWidgetBlueprintGeneratedClass* TopGeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(TopButtonClass))
	{
		const UWidgetTree* TopTree = TopGeneratedClass->GetWidgetTreeArchetype();
		TestNotNull(TEXT("Transparent top-button WBP has a compiled WidgetTree"), TopTree);
		if (TopTree)
		{
			TestNull(TEXT("Transparent top-button WBP has no colored Surface layer"),
				TopTree->FindWidget(TEXT("Surface")));
		}
	}
	else
	{
		AddError(TEXT("Could not load the transparent top-button generated class."));
	}

	UTexture2D* IconAtlas = CheckUiTextureContract(*this, IconAtlasPackagePath);
	const TCHAR* ScreenClassPath =
		TEXT("/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildCatalogScreen.WBP_FF_BuildCatalogScreen_C");
	UClass* ScreenClass = LoadClass<UFF_BuildCatalogScreen>(nullptr, ScreenClassPath);
	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!ScreenClass || !EditorWorld)
	{
		AddError(TEXT("Could not load the generated catalog screen or acquire the editor world."));
		return true;
	}

	struct FPlayerlessFlagRestore
	{
		UWidgetBlueprintGeneratedClass* GeneratedClass = nullptr;
		bool bPreviousValue = false;
	};
	TArray<FPlayerlessFlagRestore> FlagRestores;
	const TCHAR* GeneratedClassPaths[] = {
		ScreenClassPath,
		TEXT("/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildTopButton.WBP_FF_BuildTopButton_C"),
		TEXT("/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildCategoryButton.WBP_FF_BuildCategoryButton_C"),
		TEXT("/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildMinorSection.WBP_FF_BuildMinorSection_C"),
		TEXT("/Game/FestaFiesta/Zone/UI/Building/WBP_FF_BuildFacilityCard.WBP_FF_BuildFacilityCard_C"),
	};
	for (const TCHAR* GeneratedClassPath : GeneratedClassPaths)
	{
		if (UWidgetBlueprintGeneratedClass* GeneratedClass =
			Cast<UWidgetBlueprintGeneratedClass>(LoadObject<UClass>(nullptr, GeneratedClassPath)))
		{
			FPlayerlessFlagRestore& Restore = FlagRestores.AddDefaulted_GetRef();
			Restore.GeneratedClass = GeneratedClass;
			Restore.bPreviousValue = GeneratedClass->bCanCallInitializedWithoutPlayerContext;
			GeneratedClass->bCanCallInitializedWithoutPlayerContext = true;
		}
	}

	UFF_BuildCatalogScreen* Screen = CreateWidget<UFF_BuildCatalogScreen>(EditorWorld, ScreenClass);
	TestNotNull(TEXT("Catalog screen with transparent top buttons can be instantiated"), Screen);
	if (Screen)
	{
		Screen->TakeWidget();
		TestTrue(TEXT("Catalog screen uses its saved Designer WidgetTree"),
			Screen->IsUsingDesignerWidgetTree());

		const TCHAR* TopButtonNames[] = {
			TEXT("BTN_BuildTab"),
			TEXT("BTN_SkinTab"),
			TEXT("BTN_Close"),
		};
		for (const TCHAR* TopButtonName : TopButtonNames)
		{
			UFF_BuildTopButton* Button = Cast<UFF_BuildTopButton>(
				Screen->GetWidgetFromName(TopButtonName));
			TestNotNull(
				FString::Printf(TEXT("%s is the dedicated transparent top button"), TopButtonName),
				Button);
			if (!Button)
			{
				continue;
			}

			TestNull(
				FString::Printf(TEXT("%s has no colored Surface layer"), TopButtonName),
				Button->GetWidgetFromName(TEXT("Surface")));
			const UCommonButtonStyle* AppliedStyle = Button->GetStyleCDO();
			TestTrue(
				FString::Printf(TEXT("%s uses the native transparent style"), TopButtonName),
				AppliedStyle && AppliedStyle->IsA<UFF_BuildTopButtonStyle>());

			UImage* Icon = Cast<UImage>(Button->GetWidgetFromName(TEXT("Image_Icon")));
			TestNotNull(FString::Printf(TEXT("%s binds Image_Icon"), TopButtonName), Icon);
			if (Icon && IconAtlas)
			{
				TestEqual(
					FString::Printf(TEXT("%s loads the transparent icon atlas"), TopButtonName),
					Icon->GetBrush().GetResourceObject(), static_cast<UObject*>(IconAtlas));
			}
		}

		UFF_BuildTopButton* BuildTab = Cast<UFF_BuildTopButton>(
			Screen->GetWidgetFromName(TEXT("BTN_BuildTab")));
		UFF_BuildTopButton* SkinTab = Cast<UFF_BuildTopButton>(
			Screen->GetWidgetFromName(TEXT("BTN_SkinTab")));
		if (BuildTab && SkinTab)
		{
			TestTrue(TEXT("Build tab starts selected"), BuildTab->GetSelected());
			TestFalse(TEXT("Skin tab starts deselected"), SkinTab->GetSelected());
			UImage* BuildIndicator = Cast<UImage>(
				BuildTab->GetWidgetFromName(TEXT("Image_SelectedIndicator")));
			UImage* SkinIndicator = Cast<UImage>(
				SkinTab->GetWidgetFromName(TEXT("Image_SelectedIndicator")));
			TestNotNull(TEXT("Build tab binds selected indicator"), BuildIndicator);
			TestNotNull(TEXT("Skin tab binds selected indicator"), SkinIndicator);
			if (BuildIndicator && SkinIndicator)
			{
				TestEqual(TEXT("Selected Build tab shows its underline"),
					BuildIndicator->GetVisibility(), ESlateVisibility::HitTestInvisible);
				TestEqual(TEXT("Deselected Skin tab hides its underline"),
					SkinIndicator->GetVisibility(), ESlateVisibility::Hidden);
			}
		}
	}

	for (const FPlayerlessFlagRestore& Restore : FlagRestores)
	{
		Restore.GeneratedClass->bCanCallInitializedWithoutPlayerContext = Restore.bPreviousValue;
	}
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
