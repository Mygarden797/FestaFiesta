// FF_AtlasNaturalWbpBuilder.cpp

#include "CoreMinimal.h"

#if WITH_EDITOR

// 대한민국 자연 지형 Atlas 시안을 기존 WBP와 분리해 만드는 에디터 전용 도구다.
//
// `FF.AtlasNaturalUMG`는 다음 원칙을 지킨다.
// - 기존 WBP_FF_AtlasCountry/WBP_FF_AtlasPin과 Atlas 카탈로그를 수정하지 않는다.
// - 새 Natural WBP와 누락된 텍스처만 만든다. 같은 명령을 다시 실행해도 기존 에셋을 덮지 않는다.
// - MapImage/PreviewImage는 데이터 에셋 소유 값이므로 자동으로 기록하지 않는다. 명령 종료 로그에
//   에디터에서 연결할 정확한 경로를 남긴다.

#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Factories/TextureFactory.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "TextureCompiler.h"
#include "UI/Atlas/FF_AtlasCountryScreen.h"
#include "UI/Atlas/FF_FestivalAtlasCatalog.h"
#include "UI/Atlas/FF_AtlasPinButton.h"
#include "UI/Core/FF_UIFlowSettings.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/FF_CommonButtonBase.h"
#include "UObject/SavePackage.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

namespace FFAtlasNaturalWbpBuilder
{
	using namespace FFUIWbpBuilder;

	const TCHAR* LogPrefix = TEXT("FF_ATLAS_NATURAL_UMG");

	const FString AtlasFolder = TEXT("/Game/FestaFiesta/UI/Atlas");
	const FString TextureRoot = TEXT("/Game/FestaFiesta/UI/Atlas/Korea");
	const FString NaturalPinPackage = AtlasFolder / TEXT("WBP_FF_AtlasPin_Natural");
	const FString NaturalCountryPackage = AtlasFolder / TEXT("WBP_FF_AtlasCountry_Natural");
	const FString MenuButtonPackage = TEXT("/Game/FestaFiesta/UI/Frontend/WBP_FF_MenuButton");

	const FString SourceRoot = FPaths::ProjectDir() /
		TEXT("SourceArt/UI/Atlas/KoreaNaturalParts_v1");

	struct FTextureSpec
	{
		FName Key;
		const TCHAR* RelativeSource;
		const TCHAR* RelativePackage;
	};

	// P0 화면을 실제로 조립하는 색상 텍스처만 가져온다. 대용량 ADM1/높이 마스크와
	// Preview/GeneratedSource는 후속 머티리얼 작업의 입력이므로 이 명령의 대상이 아니다.
	const TArray<FTextureSpec> RequiredTextures =
	{
		{ TEXT("Ocean"), TEXT("Map/T_FF_FA_Background_Ocean_1920x1080.png"),
			TEXT("Map/T_FF_FA_Background_Ocean_1920x1080") },
		{ TEXT("Map"), TEXT("Map/T_FF_FA_Map_KoreaNaturalADM1_2400x2160.png"),
			TEXT("Map/T_FF_FA_Map_KoreaNaturalADM1_2400x2160") },
		{ TEXT("BusanOverlay"), TEXT("Overlays/T_FF_FA_Overlay_ADM1_08_Busan_Selected_2400x2160.png"),
			TEXT("Overlays/T_FF_FA_Overlay_ADM1_08_Busan_Selected_2400x2160") },
		{ TEXT("Card"), TEXT("Cards/T_FF_FA_Card_Festival_512_9Slice.png"),
			TEXT("Cards/T_FF_FA_Card_Festival_512_9Slice") },
		{ TEXT("PreviewFrame"), TEXT("Cards/T_FF_FA_Frame_Preview_512x256_9Slice.png"),
			TEXT("Cards/T_FF_FA_Frame_Preview_512x256_9Slice") },
		{ TEXT("DadaepoPreview"), TEXT("Cards/T_FF_FA_Preview_Dadaepo_512x256.png"),
			TEXT("Cards/T_FF_FA_Preview_Dadaepo_512x256") },
		{ TEXT("Breadcrumb"), TEXT("Decor/T_FF_FA_Panel_Breadcrumb_1024x128_9Slice.png"),
			TEXT("Decor/T_FF_FA_Panel_Breadcrumb_1024x128_9Slice") },
		{ TEXT("ActionGuide"), TEXT("Decor/T_FF_FA_Panel_ActionGuide_1024x128_9Slice.png"),
			TEXT("Decor/T_FF_FA_Panel_ActionGuide_1024x128_9Slice") },
		{ TEXT("Vignette"), TEXT("Decor/T_FF_FA_Overlay_Vignette_1920x1080.png"),
			TEXT("Decor/T_FF_FA_Overlay_Vignette_1920x1080") },
		{ TEXT("Caret"), TEXT("Decor/T_FF_FA_Caret_Left_64.png"),
			TEXT("Decor/T_FF_FA_Caret_Left_64") },
		{ TEXT("Globe"), TEXT("Icons/Actions/T_FF_FA_Icon_Globe_White_64.png"),
			TEXT("Icons/Actions/T_FF_FA_Icon_Globe_White_64") },
		{ TEXT("Lock"), TEXT("Icons/Actions/T_FF_FA_Icon_Lock_White_64.png"),
			TEXT("Icons/Actions/T_FF_FA_Icon_Lock_White_64") },
		{ TEXT("PinDefault"), TEXT("Markers/T_FF_FA_Marker_FestivalDefault_256.png"),
			TEXT("Markers/T_FF_FA_Marker_FestivalDefault_256") },
		{ TEXT("PinLocked"), TEXT("Markers/T_FF_FA_Marker_FestivalLocked_256.png"),
			TEXT("Markers/T_FF_FA_Marker_FestivalLocked_256") },
		{ TEXT("PinFocus"), TEXT("Markers/T_FF_FA_Marker_FocusRing_256.png"),
			TEXT("Markers/T_FF_FA_Marker_FocusRing_256") },
	};

	struct FNaturalAssets
	{
		UTexture2D* Ocean = nullptr;
		UTexture2D* Map = nullptr;
		UTexture2D* BusanOverlay = nullptr;
		UTexture2D* Card = nullptr;
		UTexture2D* PreviewFrame = nullptr;
		UTexture2D* DadaepoPreview = nullptr;
		UTexture2D* Breadcrumb = nullptr;
		UTexture2D* ActionGuide = nullptr;
		UTexture2D* Vignette = nullptr;
		UTexture2D* Caret = nullptr;
		UTexture2D* Globe = nullptr;
		UTexture2D* Lock = nullptr;
		UTexture2D* PinDefault = nullptr;
		UTexture2D* PinLocked = nullptr;
		UTexture2D* PinFocus = nullptr;

		bool IsComplete() const
		{
			return Ocean && Map && BusanOverlay && Card && PreviewFrame && DadaepoPreview
				&& Breadcrumb && ActionGuide && Vignette && Caret && Globe && Lock
				&& PinDefault && PinLocked && PinFocus;
		}
	};

	FSlateBrush MakeTextureBrush(
		UTexture2D* Texture,
		const FVector2D ImageSize,
		const ESlateBrushDrawType::Type DrawAs = ESlateBrushDrawType::Image,
		const FMargin Margin = FMargin(0.f))
	{
		FSlateBrush Brush;
		Brush.DrawAs = DrawAs;
		Brush.SetResourceObject(Texture);
		Brush.SetImageSize(ImageSize);
		Brush.Margin = Margin;
		Brush.TintColor = FSlateColor(FLinearColor::White);
		return Brush;
	}

	void SetImageTexture(UImage& Image, UTexture2D* Texture, const FVector2D ImageSize)
	{
		Image.SetBrush(MakeTextureBrush(Texture, ImageSize));
		Image.SetColorAndOpacity(FLinearColor::White);
		Image.SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	bool SaveImportedTexture(UTexture2D& Texture)
	{
		Texture.Modify();
		Texture.PreEditChange(nullptr);
		Texture.LODGroup = TEXTUREGROUP_UI;
		Texture.MipGenSettings = TMGS_NoMipmaps;
		// UE 5.7에서 UI용 무손실 RGBA8 항목의 enum 이름은 여전히 TC_EditorIcon이다.
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

	UTexture2D* ImportTextureIfMissing(const FTextureSpec& Spec)
	{
		const FString PackageName = TextureRoot / Spec.RelativePackage;
		const FString ObjectPath = MakeObjectPath(PackageName);

		// 재실행 시 기존 텍스처의 설정이나 원본 경로까지 바꾸지 않는다.
		if (UTexture2D* Existing = LoadObject<UTexture2D>(nullptr, *ObjectPath))
		{
			UE_LOG(LogFFUIWbpBuilder, Display,
				TEXT("%s_TEXTURE_REUSE: %s"), LogPrefix, *ObjectPath);
			return Existing;
		}

		if (FPackageName::DoesPackageExist(PackageName))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_TEXTURE_FAIL: %s 패키지가 있지만 UTexture2D로 로드되지 않는다. 수정하지 않았다."),
				LogPrefix, *PackageName);
			return nullptr;
		}

		const FString AbsoluteSource = FPaths::ConvertRelativePathToFull(SourceRoot / Spec.RelativeSource);
		if (!IFileManager::Get().FileExists(*AbsoluteSource))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_TEXTURE_FAIL: 원본 PNG가 없다: %s"), LogPrefix, *AbsoluteSource);
			return nullptr;
		}

		UTextureFactory* Factory = NewObject<UTextureFactory>();
		Factory->LODGroup = TEXTUREGROUP_UI;
		Factory->MipGenSettings = TMGS_NoMipmaps;
		Factory->CompressionSettings = TC_EditorIcon;
		Factory->ColorSpaceMode = ETextureSourceColorSpace::SRGB;

		UAssetImportTask* Task = NewObject<UAssetImportTask>();
		Task->Filename = AbsoluteSource;
		Task->DestinationPath = FPaths::GetPath(PackageName);
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

		const TArray<UObject*>& ImportedObjects = Task->GetObjects();
		UTexture2D* Texture = ImportedObjects.IsEmpty()
			? nullptr
			: Cast<UTexture2D>(ImportedObjects[0]);
		if (!Texture || !SaveImportedTexture(*Texture))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_TEXTURE_FAIL: %s -> %s 임포트/저장 실패."),
				LogPrefix, *AbsoluteSource, *PackageName);
			return nullptr;
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_TEXTURE_CREATE: %s"), LogPrefix, *Texture->GetPathName());
		return Texture;
	}

	bool ValidateTextureSources()
	{
		bool bAllFound = true;
		for (const FTextureSpec& Spec : RequiredTextures)
		{
			const FString AbsoluteSource = FPaths::ConvertRelativePathToFull(SourceRoot / Spec.RelativeSource);
			if (!IFileManager::Get().FileExists(*AbsoluteSource))
			{
				bAllFound = false;
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_PREFLIGHT_FAIL: 원본 PNG가 없다: %s"), LogPrefix, *AbsoluteSource);
			}
		}
		return bAllFound;
	}

	bool ImportRequiredTextures(FNaturalAssets& OutAssets)
	{
		if (!ValidateTextureSources())
		{
			return false;
		}

		TMap<FName, UTexture2D*> Imported;
		for (const FTextureSpec& Spec : RequiredTextures)
		{
			UTexture2D* Texture = ImportTextureIfMissing(Spec);
			if (!Texture)
			{
				return false;
			}
			Imported.Add(Spec.Key, Texture);
		}

		OutAssets.Ocean = Imported.FindRef(TEXT("Ocean"));
		OutAssets.Map = Imported.FindRef(TEXT("Map"));
		OutAssets.BusanOverlay = Imported.FindRef(TEXT("BusanOverlay"));
		OutAssets.Card = Imported.FindRef(TEXT("Card"));
		OutAssets.PreviewFrame = Imported.FindRef(TEXT("PreviewFrame"));
		OutAssets.DadaepoPreview = Imported.FindRef(TEXT("DadaepoPreview"));
		OutAssets.Breadcrumb = Imported.FindRef(TEXT("Breadcrumb"));
		OutAssets.ActionGuide = Imported.FindRef(TEXT("ActionGuide"));
		OutAssets.Vignette = Imported.FindRef(TEXT("Vignette"));
		OutAssets.Caret = Imported.FindRef(TEXT("Caret"));
		OutAssets.Globe = Imported.FindRef(TEXT("Globe"));
		OutAssets.Lock = Imported.FindRef(TEXT("Lock"));
		OutAssets.PinDefault = Imported.FindRef(TEXT("PinDefault"));
		OutAssets.PinLocked = Imported.FindRef(TEXT("PinLocked"));
		OutAssets.PinFocus = Imported.FindRef(TEXT("PinFocus"));
		return OutAssets.IsComplete();
	}

	UTextBlock* MakeText(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const FName Name,
		const int32 FontSize,
		const FLinearColor& Color,
		const FText& Placeholder,
		const bool bVariable = false)
	{
		UTextBlock* Text = MakeWidget<UTextBlock>(Blueprint, Tree, Name, bVariable);
		if (!Text)
		{
			return nullptr;
		}

		StyleText(*Text, FontSize, Color, TEXT("Regular"));
		Text->SetText(Placeholder);
		Text->SetAutoWrapText(true);
		return Text;
	}

	bool BuildNaturalPin(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const FNaturalAssets& Assets)
	{
		USizeBox* Root = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Root"), true);
		UOverlay* Overlay = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Root"));
		UImage* Focus = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_FocusRing"), true);
		UImage* DefaultPin = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_PinDefault"));
		UImage* LockPin = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_PinLock"), true);
		// 공통 버튼 베이스의 필수 바인딩이다. 지도 핀은 이미지 자체가 라벨 역할을 하므로 숨겨 둔다.
		UCommonTextBlock* Label = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_Label"), true);

		if (!Root || !Overlay || !Focus || !DefaultPin || !LockPin || !Label)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetWidthOverride(132.f);
		Root->SetHeightOverride(132.f);
		Root->AddChild(Overlay);

		SetImageTexture(*Focus, Assets.PinFocus, FVector2D(132.f));
		Focus->SetDesiredSizeOverride(FVector2D(132.f));
		Focus->SetVisibility(ESlateVisibility::Hidden);
		if (UOverlaySlot* Slot = Overlay->AddChildToOverlay(Focus))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		SetImageTexture(*DefaultPin, Assets.PinDefault, FVector2D(104.f));
		DefaultPin->SetDesiredSizeOverride(FVector2D(104.f));
		if (UOverlaySlot* Slot = Overlay->AddChildToOverlay(DefaultPin))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		// 잠금 이미지를 조금 크게 그려 아래 기본 핀의 실루엣을 완전히 덮는다.
		SetImageTexture(*LockPin, Assets.PinLocked, FVector2D(120.f));
		LockPin->SetDesiredSizeOverride(FVector2D(120.f));
		LockPin->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* Slot = Overlay->AddChildToOverlay(LockPin))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		Label->SetText(LOCTEXT("AtlasNaturalPinPlaceholder", "축제 핀"));
		Label->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* Slot = Overlay->AddChildToOverlay(Label))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		return true;
	}

	bool BuildNaturalCountry(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		UClass* MenuButtonClass,
		const FNaturalAssets& Assets)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette || !MenuButtonClass || !Assets.IsComplete())
		{
			return false;
		}

		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Root"));
		UImage* Ocean = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_BackgroundOcean"));
		UCanvasPanel* MapCanvas = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Map"), true);
		UImage* Map = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Map"), true);
		// 현재 런타임에는 선택 오버레이 바인딩이 없으므로 기본은 숨긴다. 디자이너에서 확인하고
		// 후속 데이터 연결을 붙일 수 있도록 Blueprint 변수로만 노출한다.
		UImage* SelectedRegion = MakeWidget<UImage>(
			Blueprint, Tree, TEXT("Image_SelectedRegionOverlay"), true);
		UImage* Vignette = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Vignette"));

		UBorder* Breadcrumb = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Breadcrumb"));
		UHorizontalBox* BreadcrumbRow = MakeWidget<UHorizontalBox>(
			Blueprint, Tree, TEXT("HorizontalBox_Breadcrumb"));
		UImage* Globe = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_BreadcrumbGlobe"));
		UTextBlock* GlobeLabel = MakeText(
			Blueprint, Tree, TEXT("Text_BreadcrumbGlobe"), 22, Palette->Frontend.Sub,
			LOCTEXT("AtlasNaturalGlobe", "지구본"));
		UTextBlock* Separator = MakeText(
			Blueprint, Tree, TEXT("Text_BreadcrumbSeparator"), 22, Palette->Frontend.Dim,
			LOCTEXT("AtlasNaturalSeparator", ">"));
		UTextBlock* CountryName = MakeText(
			Blueprint, Tree, TEXT("Text_CountryName"), 22, Palette->Frontend.Title,
			LOCTEXT("AtlasNaturalCountry", "대한민국"), true);

		UBorder* Card = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Card"));
		UCanvasPanel* CardCanvas = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Card"));
		UImage* CardCaret = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_CardCaret"));
		UImage* Preview = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Preview"), true);
		UImage* PreviewFrame = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_PreviewFrame"));
		UTextBlock* FestivalName = MakeText(
			Blueprint, Tree, TEXT("Text_FestivalName"), 27, Palette->Frontend.Title,
			LOCTEXT("AtlasNaturalFestival", "다대포 바다축제"), true);
		UTextBlock* RegionName = MakeText(
			Blueprint, Tree, TEXT("Text_RegionName"), 17, Palette->Frontend.Sub,
			LOCTEXT("AtlasNaturalRegion", "부산"), true);
		UTextBlock* EmptyHint = MakeText(
			Blueprint, Tree, TEXT("Text_EmptyHint"), 18, Palette->Frontend.Dim,
			LOCTEXT("AtlasNaturalEmpty", "지도에서 축제를 고르세요"), true);
		UImage* LockedIcon = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_LockedIcon"), true);
		UTextBlock* LockReason = MakeText(
			Blueprint, Tree, TEXT("Text_LockReason"), 16, Palette->Frontend.Dim,
			LOCTEXT("AtlasNaturalLocked", "준비 중"), true);
		USizeBox* StartSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_StartFestival"));
		UUserWidget* StartButton = MakeWidgetOfClass(
			Blueprint, Tree, TEXT("Button_StartFestival"), MenuButtonClass);

		UBorder* Guide = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_ActionGuide"));
		UHorizontalBox* GuideRow = MakeWidget<UHorizontalBox>(
			Blueprint, Tree, TEXT("HorizontalBox_ActionGuide"));
		UTextBlock* Hints = MakeText(
			Blueprint, Tree, TEXT("Text_ControlHints"), 18, Palette->Frontend.Sub,
			LOCTEXT("AtlasNaturalHints", "W A S D   핀 이동      ENTER   선택      ESC"));
		USizeBox* BackSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Back"));
		UUserWidget* BackButton = MakeWidgetOfClass(
			Blueprint, Tree, TEXT("Button_Back"), MenuButtonClass);

		if (!Root || !Ocean || !MapCanvas || !Map || !SelectedRegion || !Vignette
			|| !Breadcrumb || !BreadcrumbRow || !Globe || !GlobeLabel || !Separator || !CountryName
			|| !Card || !CardCanvas || !CardCaret || !Preview || !PreviewFrame || !FestivalName
			|| !RegionName || !EmptyHint || !LockedIcon || !LockReason || !StartSize || !StartButton
			|| !Guide || !GuideRow || !Hints || !BackSize || !BackButton)
		{
			return false;
		}

		Tree.RootWidget = Root;

		SetImageTexture(*Ocean, Assets.Ocean, FVector2D(1920.f, 1080.f));
		StretchOnCanvas(*Root, *Ocean);

		MapCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		PlaceOnCanvas(*Root, *MapCanvas, 250.f, 28.f, 1120.f, 1008.f);

		// Image_Map은 반드시 CanvasPanel_Map의 직접 자식이어야 한다. 런타임이 슬롯을
		// UCanvasPanelSlot으로 캐스팅해 지도 사각형과 핀을 같은 좌표계로 맞춘다.
		SetImageTexture(*Map, Assets.Map, FVector2D(2400.f, 2160.f));
		PlaceOnCanvas(*MapCanvas, *Map, 0.f, 0.f, 1120.f, 1008.f);

		SetImageTexture(*SelectedRegion, Assets.BusanOverlay, FVector2D(2400.f, 2160.f));
		SelectedRegion->SetVisibility(ESlateVisibility::Collapsed);
		StretchOnCanvas(*MapCanvas, *SelectedRegion);

		SetImageTexture(*Vignette, Assets.Vignette, FVector2D(1920.f, 1080.f));
		StretchOnCanvas(*Root, *Vignette);

		Breadcrumb->SetBrush(MakeTextureBrush(
			Assets.Breadcrumb,
			FVector2D(1024.f, 128.f),
			ESlateBrushDrawType::Box,
			FMargin(0.03125f, 0.25f, 0.03125f, 0.25f)));
		Breadcrumb->SetPadding(FMargin(16.f, 8.f));
		Breadcrumb->AddChild(BreadcrumbRow);
		PlaceOnCanvas(*Root, *Breadcrumb, 28.f, 26.f, 366.f, 58.f);

		SetImageTexture(*Globe, Assets.Globe, FVector2D(31.f));
		Globe->SetDesiredSizeOverride(FVector2D(31.f));
		if (UHorizontalBoxSlot* Slot = BreadcrumbRow->AddChildToHorizontalBox(Globe))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
		}
		for (UTextBlock* Item : { GlobeLabel, Separator, CountryName })
		{
			if (UHorizontalBoxSlot* Slot = BreadcrumbRow->AddChildToHorizontalBox(Item))
			{
				Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				Slot->SetVerticalAlignment(VAlign_Center);
				Slot->SetPadding(Item == CountryName
					? FMargin(12.f, 0.f, 0.f, 0.f)
					: FMargin(0.f, 0.f, 12.f, 0.f));
			}
		}

		Card->SetBrush(MakeTextureBrush(
			Assets.Card,
			FVector2D(512.f),
			ESlateBrushDrawType::Box,
			FMargin(0.09375f)));
		Card->SetPadding(FMargin(0.f));
		Card->AddChild(CardCanvas);
		PlaceOnCanvas(*Root, *Card, 1508.f, 475.f, 340.f, 370.f);

		SetImageTexture(*CardCaret, Assets.Caret, FVector2D(42.f));
		PlaceOnCanvas(*Root, *CardCaret, 1465.f, 646.f, 42.f, 42.f);

		// 런타임은 카탈로그 PreviewImage를 이 브러시에 넣는다. 여기 텍스처는 디자이너 미리보기다.
		SetImageTexture(*Preview, Assets.DadaepoPreview, FVector2D(302.f, 151.f));
		PlaceOnCanvas(*CardCanvas, *Preview, 19.f, 20.f, 302.f, 151.f);

		PreviewFrame->SetBrush(MakeTextureBrush(
			Assets.PreviewFrame,
			FVector2D(512.f, 256.f),
			ESlateBrushDrawType::Box,
			FMargin(0.046875f, 0.09375f, 0.046875f, 0.09375f)));
		PreviewFrame->SetColorAndOpacity(FLinearColor::White);
		PreviewFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlaceOnCanvas(*CardCanvas, *PreviewFrame, 19.f, 20.f, 302.f, 151.f);

		PlaceOnCanvas(*CardCanvas, *FestivalName, 22.f, 187.f, 296.f, 40.f);
		PlaceOnCanvas(*CardCanvas, *EmptyHint, 22.f, 187.f, 296.f, 56.f);
		PlaceOnCanvas(*CardCanvas, *RegionName, 22.f, 228.f, 296.f, 28.f);

		SetImageTexture(*LockedIcon, Assets.Lock, FVector2D(24.f));
		LockedIcon->SetDesiredSizeOverride(FVector2D(24.f));
		LockedIcon->SetVisibility(ESlateVisibility::Collapsed);
		PlaceOnCanvas(*CardCanvas, *LockedIcon, 22.f, 252.f, 24.f, 24.f);
		LockReason->SetVisibility(ESlateVisibility::Collapsed);
		PlaceOnCanvas(*CardCanvas, *LockReason, 52.f, 250.f, 266.f, 30.f);

		StartSize->SetWidthOverride(298.f);
		StartSize->SetHeightOverride(64.f);
		StartSize->AddChild(StartButton);
		PlaceOnCanvas(*CardCanvas, *StartSize, 21.f, 284.f, 298.f, 64.f);

		Guide->SetBrush(MakeTextureBrush(
			Assets.ActionGuide,
			FVector2D(1024.f, 128.f),
			ESlateBrushDrawType::Box,
			FMargin(0.03125f, 0.25f, 0.03125f, 0.25f)));
		Guide->SetPadding(FMargin(28.f, 5.f, 10.f, 5.f));
		Guide->AddChild(GuideRow);
		PlaceOnCanvas(*Root, *Guide, 640.f, 994.f, 640.f, 60.f);

		if (UHorizontalBoxSlot* Slot = GuideRow->AddChildToHorizontalBox(Hints))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		BackSize->SetWidthOverride(150.f);
		BackSize->SetHeightOverride(50.f);
		BackSize->AddChild(BackButton);
		if (UHorizontalBoxSlot* Slot = GuideRow->AddChildToHorizontalBox(BackSize))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		return true;
	}

	void RunAtlasNaturalUmg()
	{
		UClass* MenuButtonClass = LoadGeneratedClass(MenuButtonPackage);
		if (!MenuButtonClass || !MenuButtonClass->IsChildOf(UFF_CommonButtonBase::StaticClass()))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: WBP_FF_MenuButton이 없다. 먼저 FF.FrontendUMG를 실행한다."), LogPrefix);
			return;
		}

		FNaturalAssets Assets;
		if (!ImportRequiredTextures(Assets))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 필수 텍스처 집합을 준비하지 못해 WBP를 만들지 않았다."), LogPrefix);
			return;
		}

		UWidgetBlueprint* PinBlueprint = EnsureWidgetBlueprint(
			LogPrefix,
			NaturalPinPackage,
			AtlasFolder,
			*UFF_AtlasPinButton::StaticClass(),
			[&Assets](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildNaturalPin(Blueprint, Tree, Assets);
			});

		UClass* NaturalPinClass = PinBlueprint ? PinBlueprint->GeneratedClass.Get() : nullptr;
		if (!NaturalPinClass || !NaturalPinClass->IsChildOf(UFF_AtlasPinButton::StaticClass()))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: WBP_FF_AtlasPin_Natural의 GeneratedClass가 유효하지 않다."), LogPrefix);
			return;
		}

		UWidgetBlueprint* CountryBlueprint = EnsureWidgetBlueprint(
			LogPrefix,
			NaturalCountryPackage,
			AtlasFolder,
			*UFF_AtlasCountryScreen::StaticClass(),
			[MenuButtonClass, &Assets](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildNaturalCountry(Blueprint, Tree, MenuButtonClass, Assets);
			});

		if (!CountryBlueprint)
		{
			return;
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_END: 새 Natural 에셋만 준비했다. 기존 Atlas WBP와 DA_FestivalAtlas는 수정하지 않았다. ")
			TEXT("에디터에서 ① WBP_FF_AtlasCountry_Natural Class Defaults의 Pin Button Class를 ")
			TEXT("WBP_FF_AtlasPin_Natural로 지정, ② DA_FestivalAtlas Countries[KR].MapImage를 ")
			TEXT("/Game/FestaFiesta/UI/Atlas/Korea/Map/T_FF_FA_Map_KoreaNaturalADM1_2400x2160로 지정, ")
			TEXT("③ Entries[Busan_SeaFestival].PreviewImage를 ")
			TEXT("/Game/FestaFiesta/UI/Atlas/Korea/Cards/T_FF_FA_Preview_Dadaepo_512x256로 지정, ")
			TEXT("④ UI Flow의 AtlasCountry 화면을 WBP_FF_AtlasCountry_Natural로 바꾼다. ")
			TEXT("Image_SelectedRegionOverlay는 선택 데이터 연결 전까지 의도적으로 숨김 상태다."),
			LogPrefix);
	}

	bool SaveAtlasCatalog(UFF_FestivalAtlasCatalog& Catalog)
	{
		UPackage* Package = Catalog.GetOutermost();
		if (!Package)
		{
			return false;
		}

		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, &Catalog, *Filename, SaveArgs);
	}

	UTexture2D* LoadImportedNaturalTexture(const TCHAR* RelativePackage)
	{
		return LoadObject<UTexture2D>(
			nullptr,
			*MakeObjectPath(TextureRoot / RelativePackage));
	}

	/**
	 * 명시적으로 호출했을 때에만 기존 Atlas 카탈로그의 이미지 참조 세 개를 연결한다.
	 *
	 * FF.AtlasNaturalUMG와 분리한 이유는 카탈로그가 팀원이 에디터에서 편집하는 데이터이기
	 * 때문이다. 텍스처/WBP 생성만 다시 실행해도 사용자 데이터가 바뀌어서는 안 된다.
	 */
	// 인자는 `FConsoleCommandWithWorldAndArgsDelegate` 규격이라 `UWorld*` 가 따라온다.
	// 이 도구는 에셋만 만지므로 월드를 쓰지 않는다 — 인자를 지우면 델리게이트가 안 맞는다.
	void RunAtlasNaturalData(const TArray<FString>& Args, UWorld* World)
	{
		const UFF_UIFlowSettings* Settings = UFF_UIFlowSettings::Get();
		if (!Settings || Settings->AtlasCatalog.IsNull())
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_DATA_FAIL: Project Settings > Game > FestaFiesta UI Flow에 AtlasCatalog가 없다."),
				LogPrefix);
			return;
		}

		UFF_FestivalAtlasCatalog* Catalog = Settings->AtlasCatalog.LoadSynchronous();
		if (!Catalog)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_DATA_FAIL: AtlasCatalog를 로드하지 못했다: %s"),
				LogPrefix, *Settings->AtlasCatalog.ToSoftObjectPath().ToString());
			return;
		}

		FName CountryId = NAME_None;
		FName FestivalId = NAME_None;
		if (Args.IsEmpty())
		{
			if (Catalog->Countries.Num() != 1 || Catalog->Entries.Num() != 1)
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_DATA_FAIL: 인자를 생략하려면 국가와 축제가 각각 정확히 하나여야 한다. ")
					TEXT("현재 Countries=%d, Entries=%d. 사용법: FF.AtlasNaturalData <CountryId> <FestivalId>"),
					LogPrefix, Catalog->Countries.Num(), Catalog->Entries.Num());
				return;
			}

			CountryId = Catalog->Countries[0].CountryId;
			FestivalId = Catalog->Entries[0].FestivalId;
		}
		else if (Args.Num() == 2)
		{
			CountryId = FName(*Args[0]);
			FestivalId = FName(*Args[1]);
		}
		else
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_DATA_FAIL: 사용법: FF.AtlasNaturalData [CountryId FestivalId]"),
				LogPrefix);
			return;
		}

		const int32 CountryIndex = Catalog->Countries.IndexOfByPredicate(
			[CountryId](const FFF_AtlasCountryEntry& Country)
			{
				return Country.CountryId == CountryId;
			});
		const int32 FestivalIndex = Catalog->Entries.IndexOfByPredicate(
			[FestivalId](const FFF_FestivalAtlasEntry& Festival)
			{
				return Festival.FestivalId == FestivalId;
			});

		if (CountryIndex == INDEX_NONE || FestivalIndex == INDEX_NONE)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_DATA_FAIL: CountryId `%s` 또는 FestivalId `%s`를 카탈로그에서 찾지 못했다."),
				LogPrefix, *CountryId.ToString(), *FestivalId.ToString());
			return;
		}

		const FFF_FestivalAtlasEntry& SelectedFestival = Catalog->Entries[FestivalIndex];
		if (SelectedFestival.CountryId != CountryId)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_DATA_FAIL: 축제 `%s`의 CountryId는 `%s`이며 요청한 `%s`와 다르다."),
				LogPrefix,
				*FestivalId.ToString(),
				*SelectedFestival.CountryId.ToString(),
				*CountryId.ToString());
			return;
		}

		// 세 에셋을 모두 확인한 뒤에만 Modify한다. 중간 실패로 카탈로그가 반쯤 바뀌지 않는다.
		UTexture2D* MapTexture = LoadImportedNaturalTexture(
			TEXT("Map/T_FF_FA_Map_KoreaNaturalADM1_2400x2160"));
		UTexture2D* OverlayTexture = LoadImportedNaturalTexture(
			TEXT("Overlays/T_FF_FA_Overlay_ADM1_08_Busan_Selected_2400x2160"));
		UTexture2D* PreviewTexture = LoadImportedNaturalTexture(
			TEXT("Cards/T_FF_FA_Preview_Dadaepo_512x256"));
		if (!MapTexture || !OverlayTexture || !PreviewTexture)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_DATA_FAIL: 필요한 Natural 텍스처가 없다. 먼저 FF.AtlasNaturalUMG를 실행한다."),
				LogPrefix);
			return;
		}

		Catalog->Modify();
		Catalog->PreEditChange(nullptr);
		FFF_AtlasCountryEntry& Country = Catalog->Countries[CountryIndex];
		FFF_FestivalAtlasEntry& Festival = Catalog->Entries[FestivalIndex];
		Country.MapImage = MapTexture;
		Festival.SelectionOverlayImage = OverlayTexture;
		Festival.PreviewImage = PreviewTexture;
		Catalog->PostEditChange();
		Catalog->MarkPackageDirty();

		if (!SaveAtlasCatalog(*Catalog))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_DATA_FAIL: 이미지 참조는 메모리에 반영됐지만 카탈로그 패키지를 저장하지 못했다: %s"),
				LogPrefix, *Catalog->GetPathName());
			return;
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_DATA_END: %s / %s에 MapImage, SelectionOverlayImage, PreviewImage만 연결하고 저장했다: %s"),
			LogPrefix,
			*CountryId.ToString(),
			*FestivalId.ToString(),
			*Catalog->GetPathName());
	}

	FAutoConsoleCommand AtlasNaturalUmgCommand(
		TEXT("FF.AtlasNaturalUMG"),
		TEXT("대한민국 자연 지형 Atlas 텍스처와 새 Natural Pin/Country WBP를 비파괴 생성한다."),
		FConsoleCommandDelegate::CreateStatic(&RunAtlasNaturalUmg));

	// !! `FAutoConsoleCommandWithArgs` 라는 클래스는 없다 !!
	// 인자를 받는 자동 등록 클래스는 `WithWorldAndArgs` · `WithArgsAndOutputDevice` ·
	// `WithWorldArgsAndOutputDevice` 뿐이다 (`HAL/IConsoleManager.h`).
	// 델리게이트 타입(`FConsoleCommandWithArgsDelegate`)은 있어서 이름만 보면 짝이 있어 보인다.
	// 같은 폴더의 `FF_AtlasGlobeActor` · `FF_AtlasCameraRig` 도 `WithWorldAndArgs` 를 쓴다.
	FAutoConsoleCommandWithWorldAndArgs AtlasNaturalDataCommand(
		TEXT("FF.AtlasNaturalData"),
		TEXT("FF.AtlasNaturalData [CountryId FestivalId] - 기존 Atlas 카탈로그의 Natural 이미지 참조 세 개만 연결한다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunAtlasNaturalData));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
