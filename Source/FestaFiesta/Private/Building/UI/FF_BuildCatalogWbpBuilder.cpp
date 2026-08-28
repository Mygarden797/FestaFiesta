// FF_BuildCatalogWbpBuilder.cpp

// 건설 카탈로그의 다섯 UMG 에셋을 생성하거나 명시적으로 기준 디자인에 맞추는 에디터 도구다.
// 흐름: 대상 전체 검증 -> 필요한 Blueprint/WidgetTree 구성 -> 컴파일 -> 제한된 패키지 저장.
// 불변식: FF.BuildCatalogUMG는 없는 WBP만 만들고 기준 디자인 적용은 지정 에셋만 교체한다.
// 실패/수명: 변경 전 원본 Tree/Slot을 보존하고 컴파일·저장 실패 시 메모리 상태까지 롤백한다.

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WrapBox.h"
#include "Engine/Texture2D.h"
#include "Factories/TextureFactory.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Building/UI/FF_BuildCatalogScreen.h"
#include "Building/UI/FF_BuildFacilityCard.h"
#include "Building/UI/FF_BuildMinorSection.h"
#include "Building/UI/FF_BuildTopButton.h"
#include "UI/FF_CommonButtonBase.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "TextureCompiler.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFBuildCatalogWbpBuilder, Log, All);

namespace FFBuildCatalogWbpBuilder
{
	// 생성 대상의 패키지 경로와 원본 PNG 경로를 한곳에 고정한다. 런타임 화면의 soft reference 및
	// 자동화 테스트가 이 정확한 경로를 계약으로 사용하므로 이름을 바꾸면 세 위치를 함께 갱신해야 한다.
	const FString UiFolder(TEXT("/Game/FestaFiesta/Zone/UI/Building"));
	const FString ImageFolder(TEXT("/Game/FestaFiesta/Zone/Test_Asset/Image"));
	const FString TopButtonPackage(UiFolder / TEXT("WBP_FF_BuildTopButton"));
	const FString CategoryButtonPackage(UiFolder / TEXT("WBP_FF_BuildCategoryButton"));
	const FString FacilityCardPackage(UiFolder / TEXT("WBP_FF_BuildFacilityCard"));
	const FString MinorSectionPackage(UiFolder / TEXT("WBP_FF_BuildMinorSection"));
	const FString CatalogScreenPackage(UiFolder / TEXT("WBP_FF_BuildCatalogScreen"));
	const FString IconAtlasPackage(ImageFolder / TEXT("T_UI_BuildCatalog_IconAtlas"));
	const FString FoodTruckPackage(ImageFolder / TEXT("T_UI_BuildCatalog_FoodTruck"));
	const FString IconAtlasSource = FPaths::ProjectDir() /
		TEXT("SourceAssets/UI/BuildCatalog/T_UI_BuildCatalog_IconAtlas.png");
	const FString FoodTruckSource = FPaths::ProjectDir() /
		TEXT("SourceAssets/UI/BuildCatalog/T_UI_BuildCatalog_FoodTruck.png");

	// 모든 생성 레이아웃이 공유하는 기준 팔레트이다. 재생성해도 위젯 간 색상 관계가 일관되게 유지된다.
	const FLinearColor ColorBackdrop(0.008f, 0.024f, 0.045f, 0.66f);
	const FLinearColor ColorPanel(0.020f, 0.075f, 0.115f, 0.70f);
	const FLinearColor ColorRaised(0.035f, 0.110f, 0.145f, 0.74f);
	const FLinearColor ColorCard(0.018f, 0.047f, 0.070f, 0.94f);
	const FLinearColor ColorAccent(0.08f, 0.95f, 0.88f, 1.0f);
	const FLinearColor ColorText(0.91f, 0.96f, 1.0f, 1.0f);
	const FLinearColor ColorMuted(0.58f, 0.69f, 0.78f, 1.0f);

	template <typename TWidget>
	TWidget* MakeWidget(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const FName Name,
		const bool bVariable = false)
	{
		// WidgetTree를 Outer로 하는 템플릿 위젯을 만들고, 네이티브 BindWidget 대상만 bIsVariable로 노출한다.
		// 반환값은 모든 레이아웃 함수에서 일괄 검증하므로 생성 실패 시 nullptr을 그대로 전달한다.
		TWidget* Widget = Tree.ConstructWidget<TWidget>(TWidget::StaticClass(), Name);
		if (Widget)
		{
			Widget->bIsVariable = bVariable;
			// Widget Blueprint 컴파일러는 필드로 노출하지 않는 이름 있는 템플릿에도 안정적인 원본 GUID를 기대한다.
			// OnVariableAdded가 WidgetVariableNameToGuidMap을 채워 동적 생성 트리가 컴파일/재저장 후에도
			// 동일한 위젯 이름을 추적할 수 있게 한다.
			Blueprint.OnVariableAdded(Name);
		}
		return Widget;
	}

	template <typename TWidget>
	TWidget* MakeWidgetOfClass(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const FName Name,
		const TSubclassOf<TWidget> WidgetClass,
		const bool bVariable = false)
	{
		// 화면 내부 탭처럼 기본 UMG 타입이 아니라 생성된 WBP 클래스를 자식으로 넣어야 할 때 사용하는 변형이다.
		// 선행 WBP 생성/컴파일이 실패해 클래스가 없으면 잘못된 기본 타입을 대신 만들지 않고 전체 레이아웃 실패로 전파한다.
		if (!WidgetClass)
		{
			return nullptr;
		}

		TWidget* Widget = Tree.ConstructWidget<TWidget>(WidgetClass, Name);
		if (Widget)
		{
			Widget->bIsVariable = bVariable;
			Blueprint.OnVariableAdded(Name);
		}
		return Widget;
	}

	void StyleText(
		UCommonTextBlock& Text,
		const int32 FontSize,
		const FLinearColor& Color,
		const ETextJustify::Type Justification = ETextJustify::Left)
	{
		// 디자이너 에셋 생성 코드에서 반복되는 폰트 크기/색/정렬 설정을 한 단위로 묶는다.
		// 기존 FontInfo를 복사해 폰트 패밀리 등 다른 스타일 속성은 보존한다.
		FSlateFontInfo Font = Text.GetFont();
		Font.Size = FontSize;
		Text.SetFont(Font);
		Text.SetColorAndOpacity(FSlateColor(Color));
		Text.SetJustification(Justification);
	}

	UVerticalBoxSlot* AddVertical(
		UVerticalBox& Parent,
		UWidget& Child,
		const FMargin Padding,
		const ESlateSizeRule::Type SizeRule = ESlateSizeRule::Automatic)
	{
		// 자식 추가와 슬롯 레이아웃 설정을 원자적인 구성 단계처럼 사용한다.
		// 슬롯 생성 실패를 호출자가 필요하면 진단할 수 있도록 포인터를 반환한다.
		UVerticalBoxSlot* Slot = Parent.AddChildToVerticalBox(&Child);
		if (Slot)
		{
			Slot->SetPadding(Padding);
			Slot->SetSize(FSlateChildSize(SizeRule));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
		return Slot;
	}

	UHorizontalBoxSlot* AddHorizontal(
		UHorizontalBox& Parent,
		UWidget& Child,
		const FMargin Padding,
		const ESlateSizeRule::Type SizeRule = ESlateSizeRule::Automatic)
	{
		// 상단 바/섹션 헤더의 공통 슬롯 규칙이다. Fill을 전달한 자식만 남는 공간을 점유한다.
		UHorizontalBoxSlot* Slot = Parent.AddChildToHorizontalBox(&Child);
		if (Slot)
		{
			Slot->SetPadding(Padding);
			Slot->SetSize(FSlateChildSize(SizeRule));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		return Slot;
	}

	UOverlaySlot* AddOverlay(UOverlay& Parent, UWidget& Child, const FMargin Padding = FMargin(0.0f))
	{
		// Overlay는 추가 순서가 곧 뒤에서 앞으로의 페인팅 순서이다. 기본은 전체 채움으로 두고,
		// 선택 표시선처럼 별도 정렬이 필요한 호출자는 반환 슬롯을 다시 조정한다.
		if (UOverlaySlot* Slot = Parent.AddChildToOverlay(&Child))
		{
			Slot->SetPadding(Padding);
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
			return Slot;
		}
		return nullptr;
	}

	bool SaveNewBlueprint(UWidgetBlueprint& Blueprint)
	{
		// WidgetTree와 변수 GUID 변경을 구조 변경으로 알린 뒤 컴파일해야 GeneratedClass에 최신 트리가 반영된다.
		// 컴파일 오류가 있으면 손상된 기준 에셋을 저장하지 않고 즉시 실패를 반환한다.
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&Blueprint);
		FKismetEditorUtilities::CompileBlueprint(&Blueprint);
		if (Blueprint.Status == BS_Error)
		{
			UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
				TEXT("FF_BUILD_CATALOG_UMG_FAIL: %s did not compile."), *Blueprint.GetPathName());
			return false;
		}

		Blueprint.MarkPackageDirty();
		// LongPackageName을 플랫폼 파일 경로로 변환하고, 신규 폴더까지 만든 후 공개 독립 에셋으로 저장한다.
		UPackage* Package = Blueprint.GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);

		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, &Blueprint, *Filename, Args);
	}

	UWidgetBlueprint* LoadExistingBlueprint(const FString& PackageName)
	{
		// 패키지명(/Game/.../Asset)을 UObject 경로(/Game/.../Asset.Asset)로 변환해 정확한 WBP만 로드한다.
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackageName, *AssetName);
		return LoadObject<UWidgetBlueprint>(nullptr, *ObjectPath);
	}

	FString MakeAssetObjectPath(const FString& PackageName)
	{
		// WBP와 텍스처 양쪽에서 재사용하는 패키지명 -> 오브젝트 경로 변환이다.
		return FString::Printf(
			TEXT("%s.%s"),
			*PackageName,
			*FPackageName::GetLongPackageAssetName(PackageName));
	}

	bool SaveTexture(UTexture2D& Texture)
	{
		// 인게임 HUD용 원본 색과 선명도를 유지하도록 UI 전용 import 설정을 강제한다.
		// Pre/PostEditChange로 에디터 리소스 재빌드를 알리고, 비동기 텍스처 컴파일 완료 후 패키지를 저장한다.
		Texture.Modify();
		Texture.PreEditChange(nullptr);
		Texture.LODGroup = TEXTUREGROUP_UI;
		Texture.MipGenSettings = TMGS_NoMipmaps;
		Texture.CompressionSettings = TC_EditorIcon;
		Texture.SRGB = true;
		Texture.NeverStream = true;
		Texture.AddressX = TA_Clamp;
		Texture.AddressY = TA_Clamp;
		Texture.PostEditChange();
		Texture.MarkPackageDirty();

		TArray<UTexture*> TexturesToFinish { &Texture };
		FTextureCompilingManager::Get().FinishCompilation(TexturesToFinish);

		UPackage* Package = Texture.GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);

		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, &Texture, *Filename, Args);
	}

	UTexture2D* ImportUiTextureIfMissing(
		const FString& SourceFilename,
		const FString& PackageName)
	{
		// 기존 텍스처는 다시 import하지 않아 소스/디자이너 변경을 보존하되, 현재 UI 설정으로 정규화해 저장한다.
		if (UTexture2D* Existing = LoadObject<UTexture2D>(nullptr, *MakeAssetObjectPath(PackageName)))
		{
			return SaveTexture(*Existing) ? Existing : nullptr;
		}

		// ImportAssetTasks에 넘기기 전에 절대 경로와 파일 존재를 검증해 모호한 팩토리 오류를 피한다.
		const FString AbsoluteSource = FPaths::ConvertRelativePathToFull(SourceFilename);
		if (!IFileManager::Get().FileExists(*AbsoluteSource))
		{
			UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
				TEXT("FF_BUILD_CATALOG_TEXTURE_FAIL: source PNG is missing: %s"),
				*AbsoluteSource);
			return nullptr;
		}

		// 자동화 import 팩토리와 작업을 구성한다. 비동기로 두면 직후 SaveTexture가 미완성 Source를 볼 수 있어 동기로 고정한다.
		UTextureFactory* Factory = NewObject<UTextureFactory>();
		Factory->LODGroup = TEXTUREGROUP_UI;
		Factory->MipGenSettings = TMGS_NoMipmaps;
		Factory->CompressionSettings = TC_EditorIcon;
		Factory->ColorSpaceMode = ETextureSourceColorSpace::SRGB;

		UAssetImportTask* Task = NewObject<UAssetImportTask>();
		Task->Filename = AbsoluteSource;
		Task->DestinationPath = ImageFolder;
		Task->DestinationName = FPackageName::GetLongPackageAssetName(PackageName);
		Task->bAutomated = true;
		Task->bAsync = false;
		Task->bSave = false;
		Task->bReplaceExisting = false;
		Task->bReplaceExistingSettings = false;
		Task->Factory = Factory;

		TArray<UAssetImportTask*> Tasks { Task };
		FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"))
			.Get().ImportAssetTasks(Tasks);

		// import 결과가 비었거나 첫 객체가 예상 타입이 아니면 저장을 시도하지 않는다.
		const TArray<UObject*>& ImportedObjects = Task->GetObjects();
		UTexture2D* Texture = ImportedObjects.IsEmpty()
			? nullptr
			: Cast<UTexture2D>(ImportedObjects[0]);
		if (!Texture || !SaveTexture(*Texture))
		{
			UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
				TEXT("FF_BUILD_CATALOG_TEXTURE_FAIL: could not import %s as %s."),
				*AbsoluteSource,
				*PackageName);
			return nullptr;
		}

		UE_LOG(LogFFBuildCatalogWbpBuilder, Display,
			TEXT("FF_BUILD_CATALOG_TEXTURE_CREATED: %s"), *Texture->GetPathName());
		return Texture;
	}

	bool ResetWidgetTree(UWidgetBlueprint& Blueprint)
	{
		// 참조 디자인 적용 명령만 호출하는 파괴적 단계이다. 이전 트리를 교체하기 전에 그래프 바인딩과
		// 이름-GUID 맵을 비워 새 트리의 위젯 계약이 과거 템플릿 정보와 충돌하지 않게 한다.
		Blueprint.Modify();
		Blueprint.Bindings.Reset();
		Blueprint.WidgetVariableNameToGuidMap.Reset();

		if (UWidgetTree* OldTree = Blueprint.WidgetTree)
		{
			// 같은 Blueprint Outer 아래에 "WidgetTree"라는 새 객체를 만들 수 있도록 기존 객체를 고유 이름으로 옮긴다.
			// Redirector/트랜잭션 기록은 생성 에셋에 불필요하며, Rename 실패 시 기존 트리를 건드리지 않고 중단한다.
			const FName DiscardedName = MakeUniqueObjectName(
				&Blueprint,
				UWidgetTree::StaticClass(),
				TEXT("DiscardedWidgetTree"));
			if (!OldTree->Rename(
				*DiscardedName.ToString(),
				&Blueprint,
				REN_DontCreateRedirectors | REN_NonTransactional))
			{
				UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
					TEXT("FF_BUILD_CATALOG_UMG_FAIL: could not replace WidgetTree for %s."),
					*Blueprint.GetPathName());
				return false;
			}
			// 옛 트리가 새 패키지에 직렬화되지 않도록 transient로 표시한다. 실제 회수는 UObject/GC 수명주기에 맡긴다.
			OldTree->SetFlags(RF_Transient);
		}

		// 컴파일러가 기대하는 고정 이름과 트랜잭션 플래그로 빈 트리를 만든 뒤 호출자 레이아웃 함수가 채운다.
		Blueprint.WidgetTree = NewObject<UWidgetTree>(
			&Blueprint,
			UWidgetTree::StaticClass(),
			TEXT("WidgetTree"),
			RF_Transactional);
		return Blueprint.WidgetTree != nullptr;
	}

	template <typename TBuildLayout>
	UWidgetBlueprint* EnsureWidgetBlueprint(
		const FString& PackageName,
		UClass& ParentClass,
		TBuildLayout&& BuildLayout)
	{
		// 비파괴 생성 경로: 패키지가 있으면 레이아웃 콜백을 절대 실행하지 않는다.
		// 즉 명령을 여러 번 실행해도 디자이너가 기존 WBP에서 수정한 계층과 스타일이 보존된다.
		if (FPackageName::DoesPackageExist(PackageName))
		{
			UWidgetBlueprint* Existing = LoadExistingBlueprint(PackageName);
			if (!Existing)
			{
				UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
					TEXT("FF_BUILD_CATALOG_UMG_FAIL: %s exists but could not be loaded; it was not modified."),
					*PackageName);
				return nullptr;
			}

			if (!Existing->GeneratedClass || !Existing->GeneratedClass->IsChildOf(&ParentClass))
			{
				// 같은 경로에 다른 종류의 WBP가 있어도 삭제/교체하지 않는다. 경고만 남기고 기존 객체를 반환한다.
				UE_LOG(LogFFBuildCatalogWbpBuilder, Warning,
					TEXT("FF_BUILD_CATALOG_UMG_REUSE: %s exists with an unexpected parent; it was left untouched."),
					*PackageName);
			}
			else
			{
				UE_LOG(LogFFBuildCatalogWbpBuilder, Display,
					TEXT("FF_BUILD_CATALOG_UMG_REUSE: %s already exists; Designer changes were preserved."),
					*PackageName);
			}
			return Existing;
		}

		// 패키지가 없을 때만 지정 네이티브 클래스를 부모로 하는 Widget Blueprint를 생성한다.
		UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
		Factory->ParentClass = &ParentClass;

		FAssetToolsModule& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(AssetTools.Get().CreateAsset(
			AssetName, UiFolder, UWidgetBlueprint::StaticClass(), Factory));
		if (!Blueprint || !Blueprint->WidgetTree)
		{
			UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
				TEXT("FF_BUILD_CATALOG_UMG_FAIL: could not create %s."), *PackageName);
			return nullptr;
		}

		// 레이아웃 콜백은 모든 필수 위젯 생성 여부를 bool로 반환한다. 실패한 부분 트리는 저장하지 않는다.
		if (!BuildLayout(*Blueprint, *Blueprint->WidgetTree))
		{
			UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
				TEXT("FF_BUILD_CATALOG_UMG_FAIL: layout creation failed for %s."), *PackageName);
			return nullptr;
		}

		if (!SaveNewBlueprint(*Blueprint))
		{
			UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
				TEXT("FF_BUILD_CATALOG_UMG_FAIL: could not save %s."), *PackageName);
			return nullptr;
		}

		UE_LOG(LogFFBuildCatalogWbpBuilder, Display,
			TEXT("FF_BUILD_CATALOG_UMG_CREATED: %s"), *PackageName);
		return Blueprint;
	}

	template <typename TBuildLayout>
	UWidgetBlueprint* ReplaceGeneratedWidgetBlueprint(
		const FString& PackageName,
		UClass& ParentClass,
		TBuildLayout&& BuildLayout)
	{
		// 명시적 기준 디자인 적용 경로이다. 에셋이 아직 없으면 비파괴 생성 함수로 위임해 동일 결과를 만든다.
		UWidgetBlueprint* Blueprint = LoadExistingBlueprint(PackageName);
		if (!Blueprint)
		{
			return EnsureWidgetBlueprint(
				PackageName,
				ParentClass,
				Forward<TBuildLayout>(BuildLayout));
		}

		// 이름이 같아도 부모가 예상과 다르면 사용자 에셋일 가능성이 있으므로 절대 교체하지 않는다.
		if (!Blueprint->GeneratedClass || !Blueprint->GeneratedClass->IsChildOf(&ParentClass))
		{
			UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
				TEXT("FF_BUILD_CATALOG_UMG_FAIL: %s has an unexpected parent and was not modified."),
				*PackageName);
			return nullptr;
		}

		// 순서가 중요하다: 이전 트리/바인딩 초기화 -> 새 계층 구축 -> 컴파일 및 저장.
		// 단축 평가로 어느 단계든 실패하면 후속 단계와 성공 로그를 실행하지 않는다.
		if (!ResetWidgetTree(*Blueprint) ||
			!BuildLayout(*Blueprint, *Blueprint->WidgetTree) ||
			!SaveNewBlueprint(*Blueprint))
		{
			UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
				TEXT("FF_BUILD_CATALOG_UMG_FAIL: reference layout could not be applied to %s."),
				*PackageName);
			return nullptr;
		}

		UE_LOG(LogFFBuildCatalogWbpBuilder, Display,
			TEXT("FF_BUILD_CATALOG_UMG_UPDATED: %s"), *PackageName);
		return Blueprint;
	}

	bool BuildTopButtonLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		// 상단 액션은 배경 Surface 없이 아이콘/라벨과 상태 표시선만 그린다. 고정 Root 크기로
		// 클릭 영역은 유지하고, CommonButton 내부 배경은 UFF_BuildTopButtonStyle이 NoDraw로 만든다.
		USizeBox* Root = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Root_Size"));
		UOverlay* Overlay = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Root_Overlay"));
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Button_Content"));
		USizeBox* IconSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Icon_Size"));
		UImage* Icon = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Icon"), true);
		UCommonTextBlock* Label = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_Label"), true);
		USizeBox* SelectedIndicatorSize = MakeWidget<USizeBox>(
			Blueprint, Tree, TEXT("SelectedIndicator_Size"));
		UImage* SelectedIndicator = MakeWidget<UImage>(
			Blueprint, Tree, TEXT("Image_SelectedIndicator"), true);
		USizeBox* FocusIndicatorSize = MakeWidget<USizeBox>(
			Blueprint, Tree, TEXT("FocusIndicator_Size"));
		UImage* FocusIndicator = MakeWidget<UImage>(
			Blueprint, Tree, TEXT("Image_FocusRing"), true);
		if (!Root || !Overlay || !Content || !IconSize || !Icon || !Label ||
			!SelectedIndicatorSize || !SelectedIndicator || !FocusIndicatorSize || !FocusIndicator)
		{
			return false;
		}

		Root->SetWidthOverride(112.0f);
		Root->SetHeightOverride(68.0f);

		IconSize->SetWidthOverride(38.0f);
		IconSize->SetHeightOverride(38.0f);
		Icon->SetDesiredSizeOverride(FVector2D(38.0f, 38.0f));
		Icon->SetColorAndOpacity(ColorText);
		// 런타임이 아틀라스 브러시를 주입하기 전 Designer에서 흰 자리표시자 사각형이 보이지 않게 한다.
		FSlateBrush EmptyIconBrush;
		EmptyIconBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		Icon->SetBrush(EmptyIconBrush);
		IconSize->AddChild(Icon);

		Label->SetText(FText::FromString(TEXT("탭")));
		StyleText(*Label, 13, ColorText, ETextJustify::Center);
		AddVertical(*Content, *IconSize, FMargin(0.0f, 0.0f, 0.0f, 2.0f));
		AddVertical(*Content, *Label, FMargin(0.0f));

		SelectedIndicatorSize->SetHeightOverride(3.0f);
		FocusIndicatorSize->SetHeightOverride(1.0f);
		if (UTexture2D* WhiteTexture = LoadObject<UTexture2D>(
			nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture")))
		{
			SelectedIndicator->SetBrushFromTexture(WhiteTexture, true);
			FocusIndicator->SetBrushFromTexture(WhiteTexture, true);
		}
		SelectedIndicator->SetColorAndOpacity(ColorAccent);
		SelectedIndicator->SetVisibility(ESlateVisibility::Hidden);
		FocusIndicator->SetColorAndOpacity(ColorText);
		FocusIndicator->SetVisibility(ESlateVisibility::Hidden);
		SelectedIndicatorSize->AddChild(SelectedIndicator);
		FocusIndicatorSize->AddChild(FocusIndicator);

		if (UOverlaySlot* ContentSlot = AddOverlay(*Overlay, *Content))
		{
			ContentSlot->SetHorizontalAlignment(HAlign_Center);
			ContentSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UOverlaySlot* SelectedSlot = AddOverlay(*Overlay, *SelectedIndicatorSize))
		{
			SelectedSlot->SetHorizontalAlignment(HAlign_Fill);
			SelectedSlot->SetVerticalAlignment(VAlign_Bottom);
		}
		if (UOverlaySlot* FocusSlot = AddOverlay(*Overlay, *FocusIndicatorSize))
		{
			FocusSlot->SetHorizontalAlignment(HAlign_Fill);
			FocusSlot->SetVerticalAlignment(VAlign_Bottom);
		}

		Root->AddChild(Overlay);
		Tree.RootWidget = Root;
		return true;
	}

	bool BuildCategoryButtonLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		// 계층: SizeBox(고정 크기) -> Overlay -> 배경 Surface와 하단 선택 표시선을 겹친다.
		// Surface 안에는 아이콘과 레이블을 세로 배치하며, 네이티브 CommonButtonBase가 접근하는
		// Image_Icon/Text_Label/Image_FocusRing만 변수로 노출한다.
		USizeBox* Root = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Root_Size"));
		UOverlay* Overlay = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Root_Overlay"));
		UBorder* Surface = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Surface"));
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Button_Content"));
		USizeBox* IconSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Icon_Size"));
		UImage* Icon = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Icon"), true);
		UCommonTextBlock* Label = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_Label"), true);
		USizeBox* IndicatorSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Indicator_Size"));
		UImage* SelectionIndicator = MakeWidget<UImage>(
			Blueprint, Tree, TEXT("Image_FocusRing"), true);
		// 하나라도 없으면 불완전 WidgetTree를 컴파일하지 않도록 전체 레이아웃 실패로 처리한다.
		if (!Root || !Overlay || !Surface || !Content || !IconSize || !Icon || !Label ||
			!IndicatorSize || !SelectionIndicator)
		{
			return false;
		}

		Root->SetWidthOverride(112.0f);
		Root->SetHeightOverride(68.0f);
		Surface->SetBrushColor(FLinearColor::Transparent);
		Surface->SetPadding(FMargin(8.0f, 5.0f, 8.0f, 7.0f));
		Surface->SetHorizontalAlignment(HAlign_Center);
		Surface->SetVerticalAlignment(VAlign_Center);

		IconSize->SetWidthOverride(38.0f);
		IconSize->SetHeightOverride(38.0f);
		Icon->SetDesiredSizeOverride(FVector2D(38.0f, 38.0f));
		Icon->SetColorAndOpacity(ColorText);
		IconSize->AddChild(Icon);

		Label->SetText(FText::FromString(TEXT("분류")));
		StyleText(*Label, 13, ColorText, ETextJustify::Center);
		AddVertical(*Content, *IconSize, FMargin(0.0f, 0.0f, 0.0f, 2.0f));
		AddVertical(*Content, *Label, FMargin(0.0f));

		// CommonButtonBase가 선택/포커스 상태에 따라 표시하는 3px 강조선이다. 백색 텍스처에 색을 곱해
		// 별도 색상 에셋 없이 팔레트 변경이 가능하며, 초기 상태는 Hidden으로 둔다.
		IndicatorSize->SetHeightOverride(3.0f);
		if (UTexture2D* WhiteTexture = LoadObject<UTexture2D>(
			nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture")))
		{
			SelectionIndicator->SetBrushFromTexture(WhiteTexture, true);
		}
		SelectionIndicator->SetColorAndOpacity(ColorAccent);
		SelectionIndicator->SetVisibility(ESlateVisibility::Hidden);
		IndicatorSize->AddChild(SelectionIndicator);

		// Overlay 추가 순서로 Surface 위에 선택 표시선을 올리고, 표시선 슬롯만 하단 정렬로 재지정한다.
		Surface->AddChild(Content);
		AddOverlay(*Overlay, *Surface);
		if (UOverlaySlot* IndicatorSlot = AddOverlay(*Overlay, *IndicatorSize))
		{
			IndicatorSlot->SetVerticalAlignment(VAlign_Bottom);
			IndicatorSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		Root->AddChild(Overlay);
		Tree.RootWidget = Root;
		return true;
	}

	bool BuildFacilityCardLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		// 계층: 고정 카드 SizeBox -> Overlay(카드 표면 + 하단 선택선) -> VerticalBox 안에
		// 썸네일 레이어, 표시 이름, 숨김 정의 ID를 둔다. 이미지/텍스트 변수명은 네이티브 BindWidget 계약이다.
		USizeBox* Root = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Root_Size"));
		UOverlay* CardLayer = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Card_Layer"));
		UBorder* Surface = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Card_Surface"));
		UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Card_Content"));
		USizeBox* ThumbnailSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Thumbnail_Size"));
		UOverlay* ThumbnailLayer = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Thumbnail_Layer"));
		UBorder* ThumbnailBackdrop = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Thumbnail_Backdrop"));
		UImage* Thumbnail = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Thumbnail"), true);
		UCommonTextBlock* Label = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_Label"), true);
		UCommonTextBlock* DefinitionId = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_DefinitionId"), true);
		USizeBox* IndicatorSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Indicator_Size"));
		UImage* SelectionIndicator = MakeWidget<UImage>(
			Blueprint, Tree, TEXT("Image_FocusRing"), true);
		if (!Root || !CardLayer || !Surface || !Content || !ThumbnailSize || !ThumbnailLayer ||
			!ThumbnailBackdrop || !Thumbnail || !Label || !DefinitionId ||
			!IndicatorSize || !SelectionIndicator)
		{
			return false;
		}

		Root->SetWidthOverride(178.0f);
		Root->SetHeightOverride(188.0f);
		Surface->SetBrushColor(FLinearColor::Transparent);
		Surface->SetPadding(FMargin(8.0f, 8.0f, 8.0f, 10.0f));
		Surface->SetHorizontalAlignment(HAlign_Fill);
		Surface->SetVerticalAlignment(VAlign_Fill);

		// 썸네일 뒤에 어두운 Border를 먼저 깔아 투명 이미지나 로드 전 상태에서도 카드 면이 안정적으로 보인다.
		ThumbnailSize->SetHeightOverride(130.0f);
		ThumbnailBackdrop->SetBrushColor(FLinearColor(0.010f, 0.026f, 0.042f, 0.90f));
		Thumbnail->SetColorAndOpacity(FLinearColor::White);
		Thumbnail->SetDesiredSizeOverride(FVector2D(162.0f, 130.0f));
		AddOverlay(*ThumbnailLayer, *ThumbnailBackdrop);
		AddOverlay(*ThumbnailLayer, *Thumbnail);
		ThumbnailSize->AddChild(ThumbnailLayer);

		Label->SetText(FText::FromString(TEXT("시설 이름")));
		Label->SetAutoWrapText(true);
		StyleText(*Label, 14, ColorText);
		DefinitionId->SetText(FText::FromString(TEXT("Major_Minor_UniqueName")));
		DefinitionId->SetAutoWrapText(true);
		// 정의 ID는 진단/향후 디자인 확장을 위해 바인딩은 유지하되 기준 디자인에서는 레이아웃 공간까지 제거한다.
		DefinitionId->SetVisibility(ESlateVisibility::Collapsed);
		StyleText(*DefinitionId, 10, ColorMuted);

		AddVertical(*Content, *ThumbnailSize, FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		AddVertical(*Content, *Label, FMargin(2.0f, 0.0f, 2.0f, 0.0f));
		AddVertical(*Content, *DefinitionId, FMargin(1.0f, 0.0f));
		Surface->AddChild(Content);

		// 분류 버튼과 동일한 선택 표시 계약을 사용해 CommonButtonBase의 상태 처리 코드를 재사용한다.
		IndicatorSize->SetHeightOverride(3.0f);
		if (UTexture2D* WhiteTexture = LoadObject<UTexture2D>(
			nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture")))
		{
			SelectionIndicator->SetBrushFromTexture(WhiteTexture, true);
		}
		SelectionIndicator->SetColorAndOpacity(ColorAccent);
		SelectionIndicator->SetVisibility(ESlateVisibility::Hidden);
		IndicatorSize->AddChild(SelectionIndicator);

		AddOverlay(*CardLayer, *Surface);
		if (UOverlaySlot* IndicatorSlot = AddOverlay(*CardLayer, *IndicatorSize))
		{
			IndicatorSlot->SetVerticalAlignment(VAlign_Bottom);
			IndicatorSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		Root->AddChild(CardLayer);
		Tree.RootWidget = Root;
		return true;
	}

	bool BuildMinorSectionLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		// 계층: VerticalBox -> Header(강조 막대 + 제목 + 남는 폭을 채우는 구분선) -> 카드 WrapBox.
		// 화면 코드가 런타임에 Text_MinorTitle과 Box_Cards를 찾아 데이터와 카드를 주입하므로 두 위젯만 변수화한다.
		UVerticalBox* Root = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Root_Section"));
		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Section_Header"));
		USizeBox* AccentSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Accent_Size"));
		UBorder* Accent = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Accent"));
		UCommonTextBlock* Title = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("Text_MinorTitle"), true);
		USizeBox* DividerSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Header_DividerSize"));
		UBorder* Divider = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Header_Divider"));
		UWrapBox* Cards = MakeWidget<UWrapBox>(Blueprint, Tree, TEXT("Box_Cards"), true);
		if (!Root || !Header || !AccentSize || !Accent || !Title || !DividerSize ||
			!Divider || !Cards)
		{
			return false;
		}

		AccentSize->SetWidthOverride(4.0f);
		AccentSize->SetHeightOverride(22.0f);
		Accent->SetBrushColor(ColorAccent);
		AccentSize->AddChild(Accent);
		Title->SetText(FText::FromString(TEXT("소분류")));
		StyleText(*Title, 19, ColorText);
		DividerSize->SetHeightOverride(1.0f);
		Divider->SetBrushColor(FLinearColor(0.35f, 0.72f, 0.74f, 0.22f));
		DividerSize->AddChild(Divider);
		// WrapBox가 화면 폭에 따라 카드를 자동 줄바꿈하며, 섹션은 카드 개수나 열 수를 직접 계산하지 않는다.
		Cards->SetInnerSlotPadding(FVector2D(12.0f, 12.0f));
		Cards->SetHorizontalAlignment(HAlign_Left);

		AddHorizontal(*Header, *AccentSize, FMargin(0.0f, 0.0f, 10.0f, 0.0f));
		AddHorizontal(*Header, *Title, FMargin(0.0f, 0.0f, 14.0f, 0.0f));
		AddHorizontal(*Header, *DividerSize, FMargin(0.0f), ESlateSizeRule::Fill);
		AddVertical(*Root, *Header, FMargin(0.0f, 0.0f, 0.0f, 12.0f));
		AddVertical(*Root, *Cards, FMargin(0.0f, 0.0f, 0.0f, 18.0f));
		Tree.RootWidget = Root;
		return true;
	}

	bool BuildCatalogScreenLayout(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const TSubclassOf<UFF_BuildTopButton> TopButtonClass)
	{
		// 전체 계층은 Overlay의 반투명 딤 -> 실제 패널 순으로 쌓인다. 패널 내부는
		// TopBar, 대분류 행, 구분선, Build/Skin Switcher, 상태 표시줄을 세로로 배치한다.
		// BTN_/BOX_/SWITCHER_/TXT_Status 이름은 UFF_BuildCatalogScreen의 BindWidget 계약과 정확히 일치해야 한다.
		UOverlay* Root = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Root_Overlay"));
		UBorder* Backdrop = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Background_Dim"));
		UBorder* Panel = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Catalog_Panel"));
		UVerticalBox* Layout = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Catalog_Layout"));
		UHorizontalBox* TopBar = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Top_Bar"));
		UCommonTextBlock* Title = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("TXT_Title"));
		// 상단 세 버튼은 배경 없는 전용 WBP를 사용한다. 동적 대분류는 기존 CategoryButtonClass를 유지해
		// 상단 투명화가 부스·화장실 등 대분류 Surface의 시각 계약을 바꾸지 않도록 분리한다.
		UFF_BuildTopButton* BuildTab = MakeWidgetOfClass<UFF_BuildTopButton>(
			Blueprint, Tree, TEXT("BTN_BuildTab"), TopButtonClass, true);
		UFF_BuildTopButton* SkinTab = MakeWidgetOfClass<UFF_BuildTopButton>(
			Blueprint, Tree, TEXT("BTN_SkinTab"), TopButtonClass, true);
		UFF_BuildTopButton* CloseButton = MakeWidgetOfClass<UFF_BuildTopButton>(
			Blueprint, Tree, TEXT("BTN_Close"), TopButtonClass, true);
		USpacer* TopSpacer = MakeWidget<USpacer>(Blueprint, Tree, TEXT("Top_Spacer"));
		UHorizontalBox* Categories = MakeWidget<UHorizontalBox>(
			Blueprint, Tree, TEXT("BOX_CategoryButtons"), true);
		UBorder* Divider = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Divider"));
		UWidgetSwitcher* Switcher = MakeWidget<UWidgetSwitcher>(
			Blueprint, Tree, TEXT("SWITCHER_Content"), true);
		UScrollBox* BuildScroll = MakeWidget<UScrollBox>(Blueprint, Tree, TEXT("Build_Scroll"));
		UVerticalBox* MinorSections = MakeWidget<UVerticalBox>(
			Blueprint, Tree, TEXT("BOX_MinorSections"), true);
		UBorder* SkinPanel = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Skin_Panel"));
		UCommonTextBlock* SkinMessage = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("TXT_SkinMessage"));
		UBorder* StatusSurface = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Status_Surface"));
		UCommonTextBlock* Status = MakeWidget<UCommonTextBlock>(
			Blueprint, Tree, TEXT("TXT_Status"), true);
		// 동적 화면 코드가 일부 필드를 필수로 가정하므로 누락된 채 저장하는 대신 레이아웃 전체를 실패시킨다.
		if (!Root || !Backdrop || !Panel || !Layout || !TopBar || !Title ||
			!BuildTab || !SkinTab || !CloseButton || !TopSpacer || !Categories || !Divider ||
			!Switcher || !BuildScroll || !MinorSections || !SkinPanel || !SkinMessage ||
			!StatusSurface || !Status)
		{
			return false;
		}

		Backdrop->SetBrushColor(ColorBackdrop);
		Panel->SetBrushColor(ColorPanel);
		Panel->SetPadding(FMargin(34.0f, 18.0f, 34.0f, 14.0f));
		Panel->SetHorizontalAlignment(HAlign_Fill);
		Panel->SetVerticalAlignment(VAlign_Fill);

		Title->SetText(FText::FromString(TEXT("FESTA BUILD")));
		StyleText(*Title, 13, ColorMuted);
		BuildTab->SetButtonLabel(FText::FromString(TEXT("건축")));
		SkinTab->SetButtonLabel(FText::FromString(TEXT("스킨")));
		CloseButton->SetButtonLabel(FText::FromString(TEXT("닫기  B")));
		TopSpacer->SetSize(FVector2D(16.0f, 1.0f));

		// TopSpacer만 Fill 규칙을 사용해 닫기 버튼을 오른쪽 끝으로 밀고, 나머지는 콘텐츠 크기를 유지한다.
		AddHorizontal(*TopBar, *Title, FMargin(0.0f, 0.0f, 24.0f, 0.0f));
		AddHorizontal(*TopBar, *BuildTab, FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		AddHorizontal(*TopBar, *SkinTab, FMargin(0.0f));
		AddHorizontal(*TopBar, *TopSpacer, FMargin(0.0f), ESlateSizeRule::Fill);
		AddHorizontal(*TopBar, *CloseButton, FMargin(0.0f));

		Divider->SetBrushColor(FLinearColor(0.18f, 0.62f, 0.64f, 0.36f));
		Divider->SetPadding(FMargin(0.0f, 0.5f));

		// Switcher 인덱스는 런타임 코드와의 계약이다: 0 = 스크롤 가능한 Build 목록, 1 = Skin 준비 안내.
		// BOX_MinorSections는 BuildScroll 안에서 화면이 동적으로 소분류 섹션을 추가하는 유일한 컨테이너다.
		BuildScroll->AddChild(MinorSections);
		SkinPanel->SetBrushColor(FLinearColor(0.018f, 0.038f, 0.055f, 0.48f));
		SkinPanel->SetPadding(FMargin(28.0f));
		SkinPanel->SetHorizontalAlignment(HAlign_Center);
		SkinPanel->SetVerticalAlignment(VAlign_Center);
		SkinMessage->SetText(FText::FromString(TEXT("스킨 기능은 준비 중입니다.")));
		StyleText(*SkinMessage, 20, ColorMuted, ETextJustify::Center);
		SkinPanel->AddChild(SkinMessage);
		Switcher->AddChild(BuildScroll);
		Switcher->AddChild(SkinPanel);
		Switcher->SetActiveWidgetIndex(0);

		StatusSurface->SetBrushColor(FLinearColor(0.015f, 0.045f, 0.065f, 0.56f));
		StatusSurface->SetPadding(FMargin(12.0f, 7.0f));
		Status->SetText(FText::FromString(TEXT("시설을 선택하면 배치 모드로 전환됩니다.")));
		StyleText(*Status, 13, ColorAccent);
		StatusSurface->AddChild(Status);

		// Switcher만 Fill 규칙으로 남는 세로 공간을 점유하고, 헤더/분류/상태줄은 콘텐츠 높이를 유지한다.
		AddVertical(*Layout, *TopBar, FMargin(0.0f, 0.0f, 0.0f, 10.0f));
		AddVertical(*Layout, *Categories, FMargin(0.0f, 0.0f, 0.0f, 10.0f));
		AddVertical(*Layout, *Divider, FMargin(0.0f, 0.0f, 0.0f, 12.0f));
		AddVertical(*Layout, *Switcher, FMargin(0.0f), ESlateSizeRule::Fill);
		AddVertical(*Layout, *StatusSurface, FMargin(0.0f, 12.0f, 0.0f, 0.0f));
		Panel->AddChild(Layout);

		// Overlay 페인트 순서를 이용해 딤을 화면 전체에 먼저 깔고 여백이 있는 패널을 가장 위에 둔다.
		AddOverlay(*Root, *Backdrop);
		AddOverlay(*Root, *Panel, FMargin(14.0f));
		Tree.RootWidget = Root;
		return true;
	}

	void BuildCatalogUmgAssets()
	{
		// 비파괴 부트스트랩 순서이다. 화면 WBP가 상단 버튼의 GeneratedClass를 자식 템플릿으로 필요로 하므로
		// 두 버튼을 먼저 생성/컴파일하고, 카드와 섹션을 만든 뒤 마지막에 화면을 만든다.
		UWidgetBlueprint* TopButton = EnsureWidgetBlueprint(
			TopButtonPackage,
			*UFF_BuildTopButton::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildTopButtonLayout(Blueprint, Tree);
			});

		UWidgetBlueprint* CategoryButton = EnsureWidgetBlueprint(
			CategoryButtonPackage,
			*UFF_CommonButtonBase::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildCategoryButtonLayout(Blueprint, Tree);
			});

		EnsureWidgetBlueprint(
			FacilityCardPackage,
			*UFF_BuildFacilityCard::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildFacilityCardLayout(Blueprint, Tree);
			});

		EnsureWidgetBlueprint(
			MinorSectionPackage,
			*UFF_BuildMinorSection::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildMinorSectionLayout(Blueprint, Tree);
			});

		// 생성 또는 재사용한 두 버튼 WBP가 예상 네이티브 부모를 상속할 때만 완성된 화면을 만든다.
		// 잘못된 동명 에셋을 자식으로 넣거나 런타임 대분류 템플릿을 누락하는 것을 방지한다.
		TSubclassOf<UFF_BuildTopButton> TopButtonClass;
		if (TopButton && TopButton->GeneratedClass &&
			TopButton->GeneratedClass->IsChildOf(UFF_BuildTopButton::StaticClass()))
		{
			TopButtonClass = TopButton->GeneratedClass;
		}

		TSubclassOf<UFF_CommonButtonBase> CategoryButtonClass;
		if (CategoryButton && CategoryButton->GeneratedClass &&
			CategoryButton->GeneratedClass->IsChildOf(UFF_CommonButtonBase::StaticClass()))
		{
			CategoryButtonClass = CategoryButton->GeneratedClass;
		}

		// 화면 패키지가 이미 있으면 Ensure 함수가 내용을 건드리지 않으므로 클래스가 없어도 안전하게 재사용한다.
		// 신규 화면을 만들 때는 상단/대분류 버튼 클래스가 모두 필요하다.
		if (FPackageName::DoesPackageExist(CatalogScreenPackage) ||
			(TopButtonClass && CategoryButtonClass))
		{
			EnsureWidgetBlueprint(
				CatalogScreenPackage,
				*UFF_BuildCatalogScreen::StaticClass(),
				[TopButtonClass](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
				{
					return BuildCatalogScreenLayout(Blueprint, Tree, TopButtonClass);
				});
		}
		else
		{
			UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
				TEXT("FF_BUILD_CATALOG_UMG_FAIL: a required top/category button class is unavailable; screen creation was skipped."));
		}

		UE_LOG(LogFFBuildCatalogWbpBuilder, Display,
			TEXT("FF_BUILD_CATALOG_UMG_DONE: requested assets were created or reused without overwriting existing packages."));
	}

	void MigrateBuildCatalogTopButtons()
	{
		// 이 마이그레이션은 ResetWidgetTree를 의도적으로 피한다. 필요하면 상단 버튼 WBP를 만든 뒤
		// 기존 화면의 정적 상단 바 자식 세 개만 교체한다. UPanelWidget Slot 객체는 유지되므로
		// 순서, 여백, 정렬, 크기가 보존된다.
		UWidgetBlueprint* TopButton = EnsureWidgetBlueprint(
			TopButtonPackage,
			*UFF_BuildTopButton::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildTopButtonLayout(Blueprint, Tree);
			});

		TSubclassOf<UFF_BuildTopButton> TopButtonClass;
		if (TopButton && TopButton->GeneratedClass &&
			TopButton->GeneratedClass->IsChildOf(UFF_BuildTopButton::StaticClass()))
		{
			TopButtonClass = TopButton->GeneratedClass;
		}
		if (!TopButtonClass)
		{
			UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
				TEXT("FF_BUILD_CATALOG_TOP_MIGRATION_FAIL: the dedicated top-button class is unavailable."));
			return;
		}

		UWidgetBlueprint* Screen = LoadExistingBlueprint(CatalogScreenPackage);
		if (!Screen || !Screen->WidgetTree || !Screen->GeneratedClass ||
			!Screen->GeneratedClass->IsChildOf(UFF_BuildCatalogScreen::StaticClass()))
		{
			UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
				TEXT("FF_BUILD_CATALOG_TOP_MIGRATION_FAIL: the existing catalog screen is unavailable or has an unexpected parent."));
			return;
		}
		if (Screen->GetOutermost()->IsDirty())
		{
			UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
				TEXT("FF_BUILD_CATALOG_TOP_MIGRATION_FAIL: the catalog screen has unsaved edits; save or revert them before migration."));
			return;
		}
		const FString ScreenFilename = FPackageName::LongPackageNameToFilename(
			CatalogScreenPackage, FPackageName::GetAssetPackageExtension());
		if (IFileManager::Get().IsReadOnly(*ScreenFilename))
		{
			UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
				TEXT("FF_BUILD_CATALOG_TOP_MIGRATION_FAIL: the catalog screen package is read-only."));
			return;
		}

		struct FTopButtonMigration
		{
			FName VariableName = NAME_None;
			UWidget* OldWidget = nullptr;
			UPanelWidget* Parent = nullptr;
			UPanelSlot* OriginalSlot = nullptr;
			int32 ChildIndex = INDEX_NONE;
			bool bOldIsVariable = false;
			FName DiscardedName = NAME_None;
			UFF_BuildTopButton* NewWidget = nullptr;
		};

		const FName TargetNames[] = {
			TEXT("BTN_BuildTab"),
			TEXT("BTN_SkinTab"),
			TEXT("BTN_Close")
		};
		TArray<FTopButtonMigration> Migrations;
		Migrations.Reserve(UE_ARRAY_COUNT(TargetNames));
		UHorizontalBox* TopBar = Cast<UHorizontalBox>(
			Screen->WidgetTree->FindWidget(TEXT("Top_Bar")));
		if (!TopBar)
		{
			UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
				TEXT("FF_BUILD_CATALOG_TOP_MIGRATION_FAIL: Top_Bar is missing or has an unexpected type; the screen was not modified."));
			return;
		}
		TSet<int32> TargetIndices;
		TSet<UPanelSlot*> TargetSlots;

		// 객체 이름이나 Slot을 바꾸기 전에 전체 대상 집합을 먼저 검증한다.
		for (const FName TargetName : TargetNames)
		{
			const FObjectPropertyBase* NativeBinding = FindFProperty<FObjectPropertyBase>(
				UFF_BuildCatalogScreen::StaticClass(), TargetName);
			if (!NativeBinding || !NativeBinding->HasMetaData(TEXT("BindWidgetOptional")) ||
				!TopButtonClass->IsChildOf(NativeBinding->PropertyClass))
			{
				UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
					TEXT("FF_BUILD_CATALOG_TOP_MIGRATION_FAIL: %s is not compatible with the native screen binding; the screen was not modified."),
					*TargetName.ToString());
				return;
			}

			UWidget* Existing = Screen->WidgetTree->FindWidget(TargetName);
			if (!Existing)
			{
				UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
					TEXT("FF_BUILD_CATALOG_TOP_MIGRATION_FAIL: %s is missing; the screen was not modified."),
					*TargetName.ToString());
				return;
			}
			if (!Existing->IsA(UFF_CommonButtonBase::StaticClass()))
			{
				UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
					TEXT("FF_BUILD_CATALOG_TOP_MIGRATION_FAIL: %s is not a FestaFiesta common button; the screen was not modified."),
					*TargetName.ToString());
				return;
			}

			UHorizontalBox* Parent = Cast<UHorizontalBox>(Existing->GetParent());
			const int32 ChildIndex = Parent ? Parent->GetChildIndex(Existing) : INDEX_NONE;
			if (!Parent || Parent != TopBar || !Cast<UHorizontalBoxSlot>(Existing->Slot) ||
				ChildIndex == INDEX_NONE || Parent->GetChildAt(ChildIndex) != Existing)
			{
				UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
					TEXT("FF_BUILD_CATALOG_TOP_MIGRATION_FAIL: %s has no stable HorizontalBox slot; the screen was not modified."),
					*TargetName.ToString());
				return;
			}
			if (TargetIndices.Contains(ChildIndex) || TargetSlots.Contains(Existing->Slot))
			{
				UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
					TEXT("FF_BUILD_CATALOG_TOP_MIGRATION_FAIL: %s shares a slot/index with another target; the screen was not modified."),
					*TargetName.ToString());
				return;
			}
			TargetIndices.Add(ChildIndex);
			TargetSlots.Add(Existing->Slot);
			if (!Screen->WidgetVariableNameToGuidMap.Contains(TargetName))
			{
				UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
					TEXT("FF_BUILD_CATALOG_TOP_MIGRATION_FAIL: %s has no variable GUID; the screen was not modified."),
					*TargetName.ToString());
				return;
			}
			if (Existing->IsA(TopButtonClass.Get()))
			{
				continue;
			}

			FTopButtonMigration& Migration = Migrations.AddDefaulted_GetRef();
			Migration.VariableName = TargetName;
			Migration.OldWidget = Existing;
			Migration.Parent = Parent;
			Migration.OriginalSlot = Existing->Slot;
			Migration.ChildIndex = ChildIndex;
			Migration.bOldIsVariable = Existing->bIsVariable;
		}

		if (Migrations.IsEmpty())
		{
			UE_LOG(LogFFBuildCatalogWbpBuilder, Display,
				TEXT("FF_BUILD_CATALOG_TOP_MIGRATION_REUSE: all three top buttons already use the dedicated transparent WBP."));
			return;
		}

		Screen->Modify();
		Screen->WidgetTree->Modify();
		for (FTopButtonMigration& Migration : Migrations)
		{
			Migration.Parent->Modify();
			Migration.OldWidget->Modify();

			Migration.DiscardedName = MakeUniqueObjectName(
				Screen->WidgetTree,
				Migration.OldWidget->GetClass(),
				*FString::Printf(TEXT("%s_PreTransparent"), *Migration.VariableName.ToString()));
			if (!Migration.OldWidget->Rename(
				*Migration.DiscardedName.ToString(),
				Screen->WidgetTree,
				REN_DontCreateRedirectors | REN_NonTransactional))
			{
				UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
					TEXT("FF_BUILD_CATALOG_TOP_MIGRATION_FAIL: %s could not be prepared; the screen was not saved."),
					*Migration.VariableName.ToString());
				break;
			}

			Migration.NewWidget = Screen->WidgetTree->ConstructWidget<UFF_BuildTopButton>(
				TopButtonClass,
				Migration.VariableName);
			if (!Migration.NewWidget)
			{
				UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
					TEXT("FF_BUILD_CATALOG_TOP_MIGRATION_FAIL: %s could not be constructed; the screen was not saved."),
					*Migration.VariableName.ToString());
				break;
			}
			Migration.NewWidget->bIsVariable = Migration.bOldIsVariable;
		}

		bool bPreparedAll = true;
		for (const FTopButtonMigration& Migration : Migrations)
		{
			bPreparedAll &= Migration.NewWidget != nullptr;
		}
		int32 ReplacedCount = 0;
		if (bPreparedAll)
		{
			for (FTopButtonMigration& Migration : Migrations)
			{
				if (!Migration.Parent->ReplaceChildAt(Migration.ChildIndex, Migration.NewWidget) ||
					Migration.Parent->GetChildAt(Migration.ChildIndex) != Migration.NewWidget ||
					Migration.NewWidget->Slot != Migration.OriginalSlot)
				{
					UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
						TEXT("FF_BUILD_CATALOG_TOP_MIGRATION_FAIL: the slot for %s could not be replaced; the screen was not saved."),
						*Migration.VariableName.ToString());
					break;
				}
				++ReplacedCount;
			}
		}

		if (!bPreparedAll || ReplacedCount != Migrations.Num())
		{
			// 아직 패키지를 저장하지 않았으므로 메모리의 이름/Slot 변경을 모두 되돌린다.
			for (int32 Index = 0; Index < ReplacedCount; ++Index)
			{
				FTopButtonMigration& Migration = Migrations[Index];
				Migration.Parent->ReplaceChildAt(Migration.ChildIndex, Migration.OldWidget);
				Migration.NewWidget->Slot = nullptr;
			}
			for (FTopButtonMigration& Migration : Migrations)
			{
				if (Migration.NewWidget)
				{
					const FName FailedName = MakeUniqueObjectName(
						Screen->WidgetTree, Migration.NewWidget->GetClass(), TEXT("DiscardedTopButton"));
					Migration.NewWidget->Rename(
						*FailedName.ToString(), Screen->WidgetTree,
						REN_DontCreateRedirectors | REN_NonTransactional);
					Migration.NewWidget->SetFlags(RF_Transient);
				}
			}
			for (FTopButtonMigration& Migration : Migrations)
			{
				if (!Migration.DiscardedName.IsNone())
				{
					Migration.OldWidget->Rename(
						*Migration.VariableName.ToString(), Screen->WidgetTree,
						REN_DontCreateRedirectors | REN_NonTransactional);
				}
			}
			return;
		}

		for (FTopButtonMigration& Migration : Migrations)
		{
			Migration.OldWidget->Slot = nullptr;
			Migration.OldWidget->bIsVariable = false;
			Migration.OldWidget->SetFlags(RF_Transient);
		}

		if (!SaveNewBlueprint(*Screen))
		{
			// 대화형 에디터에서 명령을 실행한 뒤 컴파일이나 저장이 실패할 수 있으므로
			// 원래 메모리 WidgetTree도 함께 복구한다.
			for (FTopButtonMigration& Migration : Migrations)
			{
				Migration.Parent->ReplaceChildAt(Migration.ChildIndex, Migration.OldWidget);
				Migration.NewWidget->Slot = nullptr;
			}
			for (FTopButtonMigration& Migration : Migrations)
			{
				const FName FailedName = MakeUniqueObjectName(
					Screen->WidgetTree, Migration.NewWidget->GetClass(), TEXT("DiscardedTopButton"));
				Migration.NewWidget->Rename(
					*FailedName.ToString(), Screen->WidgetTree,
					REN_DontCreateRedirectors | REN_NonTransactional);
				Migration.NewWidget->SetFlags(RF_Transient);
			}
			for (FTopButtonMigration& Migration : Migrations)
			{
				Migration.OldWidget->ClearFlags(RF_Transient);
				Migration.OldWidget->bIsVariable = Migration.bOldIsVariable;
				Migration.OldWidget->Rename(
					*Migration.VariableName.ToString(), Screen->WidgetTree,
					REN_DontCreateRedirectors | REN_NonTransactional);
			}
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Screen);
			FKismetEditorUtilities::CompileBlueprint(Screen);
			if (Screen->Status != BS_Error)
			{
				Screen->GetOutermost()->SetDirtyFlag(false);
			}
			UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
				TEXT("FF_BUILD_CATALOG_TOP_MIGRATION_FAIL: the migrated catalog screen could not be compiled or saved."));
			return;
		}

		UE_LOG(LogFFBuildCatalogWbpBuilder, Display,
			TEXT("FF_BUILD_CATALOG_TOP_MIGRATION_DONE: replaced %d top-bar button(s); all other screen widgets were preserved."),
			Migrations.Num());
	}

	void ApplyBuildCatalogReferenceDesign()
	{
		// 기준 디자인은 다섯 WBP가 참조할 두 텍스처가 모두 준비된 경우에만 적용한다.
		// 하나라도 실패하면 WidgetTree 교체 전에 종료해 에셋 집합이 반쪽 상태가 되는 것을 막는다.
		UTexture2D* IconAtlas = ImportUiTextureIfMissing(IconAtlasSource, IconAtlasPackage);
		UTexture2D* FoodTruck = ImportUiTextureIfMissing(FoodTruckSource, FoodTruckPackage);
		if (!IconAtlas || !FoodTruck)
		{
			UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
				TEXT("FF_BUILD_CATALOG_REFERENCE_FAIL: required UI textures were not available; WBP assets were not replaced."));
			return;
		}

		// 참조 디자인 명령은 아래 다섯 WBP의 기존 WidgetTree를 의도적으로 교체한다.
		// 의존 순서는 부트스트랩과 마찬가지로 두 버튼을 먼저 컴파일한 후 화면에서 GeneratedClass를 사용한다.
		UWidgetBlueprint* TopButton = ReplaceGeneratedWidgetBlueprint(
			TopButtonPackage,
			*UFF_BuildTopButton::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildTopButtonLayout(Blueprint, Tree);
			});

		UWidgetBlueprint* CategoryButton = ReplaceGeneratedWidgetBlueprint(
			CategoryButtonPackage,
			*UFF_CommonButtonBase::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildCategoryButtonLayout(Blueprint, Tree);
			});

		ReplaceGeneratedWidgetBlueprint(
			FacilityCardPackage,
			*UFF_BuildFacilityCard::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildFacilityCardLayout(Blueprint, Tree);
			});

		ReplaceGeneratedWidgetBlueprint(
			MinorSectionPackage,
			*UFF_BuildMinorSection::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildMinorSectionLayout(Blueprint, Tree);
			});

		TSubclassOf<UFF_BuildTopButton> TopButtonClass;
		if (TopButton && TopButton->GeneratedClass &&
			TopButton->GeneratedClass->IsChildOf(UFF_BuildTopButton::StaticClass()))
		{
			TopButtonClass = TopButton->GeneratedClass;
		}

		TSubclassOf<UFF_CommonButtonBase> CategoryButtonClass;
		if (CategoryButton && CategoryButton->GeneratedClass &&
			CategoryButton->GeneratedClass->IsChildOf(UFF_CommonButtonBase::StaticClass()))
		{
			CategoryButtonClass = CategoryButton->GeneratedClass;
		}

		// 화면 트리 교체 직전 다시 두 클래스 계약을 확인한다. 앞선 에셋 중 일부 실패와 무관하게
		// 잘못된 버튼 클래스로 기존 화면까지 교체하는 것을 막는 마지막 방어선이다.
		if (!TopButtonClass || !CategoryButtonClass)
		{
			UE_LOG(LogFFBuildCatalogWbpBuilder, Error,
				TEXT("FF_BUILD_CATALOG_REFERENCE_FAIL: a required top/category button class is unavailable."));
			return;
		}

		UWidgetBlueprint* Screen = ReplaceGeneratedWidgetBlueprint(
			CatalogScreenPackage,
			*UFF_BuildCatalogScreen::StaticClass(),
			[TopButtonClass](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildCatalogScreenLayout(Blueprint, Tree, TopButtonClass);
			});

		if (Screen)
		{
			UE_LOG(LogFFBuildCatalogWbpBuilder, Display,
				TEXT("FF_BUILD_CATALOG_REFERENCE_DONE: textures and five UMG assets are ready."));
		}
	}

	// 정적 등록으로 에디터 콘솔에서 호출 가능하게 한다. 파일 전체가 WITH_EDITOR 안에 있어 패키지 런타임에는 포함되지 않는다.
	FAutoConsoleCommand BuildCatalogUmgCommand(
		TEXT("FF.BuildCatalogUMG"),
		TEXT("Creates the five editable build-catalog Widget Blueprints when they do not already exist."),
		FConsoleCommandDelegate::CreateStatic(&BuildCatalogUmgAssets));

	FAutoConsoleCommand MigrateBuildCatalogTopButtonsCommand(
		TEXT("FF.MigrateBuildCatalogTopButtons"),
		TEXT("Creates the transparent top-button WBP and replaces only BTN_BuildTab, BTN_SkinTab, and BTN_Close in the existing catalog screen."),
		FConsoleCommandDelegate::CreateStatic(&MigrateBuildCatalogTopButtons));

	// 이름에 Apply/ReferenceDesign을 명시해, 기존 디자인을 보존하는 기본 생성 명령과 의도적으로 구분한다.
	FAutoConsoleCommand ApplyBuildCatalogReferenceDesignCommand(
		TEXT("FF.ApplyBuildCatalogReferenceDesign"),
		TEXT("Imports the catalog textures and explicitly replaces the five generated build-catalog UMG layouts."),
		FConsoleCommandDelegate::CreateStatic(&ApplyBuildCatalogReferenceDesign));
}

#endif // WITH_EDITOR
