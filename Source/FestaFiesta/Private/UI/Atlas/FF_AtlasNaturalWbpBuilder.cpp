// FF_AtlasNaturalWbpBuilder.cpp

#include "CoreMinimal.h"

#if WITH_EDITOR

// 대한민국 국가 지도 화면을 UMG로 조립하는 에디터 전용 도구다.
//
// === 2026-08-27 2차: 파츠 팩 UI를 걷어냈다 ===
// 팩(`KoreaNaturalParts_v1`)의 금장 패널·알약·키캡·마커는 **더 쓰지 않는다.** 남긴 것은
// **지도 세 장뿐**이고(지형·경계·선택 오버레이), 나머지 UI는 프로젝트 공통 방식으로 다시 그린다.
//
//   텍스처 없이 단색 둥근 패널  = `ESlateBrushDrawType::RoundedBox`
//   색                        = `UFF_UIPalette`
//   글자                      = `FFUIWbpBuilder::StyleText`
//
// 같은 방식이 `FF_PrepHudWbpBuilder.cpp` 의 `MakeSolidBrush` 다. 거기 주석에 있는 함정
// (`SetBrushColor` 만으로는 아무것도 안 그려진다 / 기본 `RoundingType` 이 알약이라 바를 그리면
// 알약이 된다 / 색을 브러시와 위젯 양쪽에 넣으면 두 번 곱해진다)이 여기에도 그대로 적용된다.
//
// 팩 UI 를 걷어낸 이유는 두 가지다.
//   1. 금장 판타지 톤이 나머지 화면(준비 단계 HUD 등)의 평면 톤과 맞지 않는다.
//   2. 스타디움 모양 9-Slice 가 작은 크기에서 끝이 뾰족해진다 — 크기를 키워도 안 고쳐진다.
//      경위는 `ui-antipatterns-to-avoid.md` 18·19번.
//
// === 원칙 ===
// - 기존 `WBP_FF_AtlasCountry`/`WBP_FF_AtlasPin`과 Atlas 카탈로그를 수정하지 않는다.
// - 인자 없이 실행하면 **비파괴**다. 이미 있는 에셋을 덮지 않는다.
// - `FF.AtlasNaturalUMG force`만 Natural WBP의 위젯 트리를 다시 굽는다.
// - 카탈로그(`DA_FestivalAtlas`)는 팀원이 에디터에서 편집하는 데이터라 별도 명령으로만 건드린다.

#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Factories/BlueprintFactory.h"
#include "Factories/TextureFactory.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "TextureCompiler.h"
#include "UI/Atlas/FF_AtlasCountryScreen.h"
#include "UI/Atlas/FF_FestivalAtlasCatalog.h"
#include "UI/Atlas/FF_AtlasPinButton.h"
#include "UI/Core/FF_UIFlowSettings.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/FF_CommonButtonBase.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

namespace FFAtlasNaturalWbpBuilder
{
	using namespace FFUIWbpBuilder;

	const TCHAR* LogPrefix = TEXT("FF_ATLAS_NATURAL_UMG");

	const FString AtlasFolder = TEXT("/Game/FestaFiesta/UI/Atlas");
	const FString StyleFolder = TEXT("/Game/FestaFiesta/UI/Styles");
	const FString TextureRoot = TEXT("/Game/FestaFiesta/UI/Atlas/Korea");

	const FString NaturalPinPackage = AtlasFolder / TEXT("WBP_FF_AtlasPin_Natural");
	const FString NaturalCountryPackage = AtlasFolder / TEXT("WBP_FF_AtlasCountry_Natural");
	const FString PrimaryButtonPackage = AtlasFolder / TEXT("WBP_FF_AtlasPrimaryButton");
	const FString GhostButtonPackage = AtlasFolder / TEXT("WBP_FF_AtlasGhostButton");

	const FString TransparentStylePackage = StyleFolder / TEXT("CBS_FF_AtlasTransparent");
	const FString PrimaryStylePackage = StyleFolder / TEXT("CBS_FF_AtlasPrimary");

	const FString SourceRoot = FPaths::ProjectDir() /
		TEXT("SourceArt/UI/Atlas/KoreaNaturalParts_v1");

	// 팩의 좌표 정합 규격이다. 지형·경계·선택 오버레이·마스크가 모두 이 크기를 공유한다.
	const FVector2D MapSourceSize(2400.f, 2160.f);

	// =========================================================================
	// 테마 — 색과 치수를 **여기 한 곳**에서만 정한다
	// =========================================================================
	//
	// ★ 라이트/다크를 바꾸려면 `MakeTheme` 한 함수만 고치면 된다. 아래 조립 코드에는
	//   팔레트 필드 이름이 등장하지 않는다.
	//
	// 지금은 **프론트엔드 다크 글래스**다. 이 화면은 지구본 → 국가 지도 → 축제 시작으로 이어지는
	// 프론트엔드 흐름이고 배경이 어두운 바다이기 때문이다. 준비 단계 HUD 는 인게임 라이트 팔레트를
	// 쓰는데, 그건 밝은 월드 위에 얹히기 때문이지 화면마다 다른 방식을 쓰는 것이 아니다.

	struct FAtlasTheme
	{
		FLinearColor Backdrop;
		FLinearColor Panel;
		FLinearColor PanelRaised;
		FLinearColor Edge;
		FLinearColor EdgeStrong;

		FLinearColor Title;
		FLinearColor Text;
		FLinearColor Sub;
		FLinearColor Dim;

		FLinearColor Accent;
		FLinearColor AccentHover;
		FLinearColor AccentPressed;
		FLinearColor AccentText;
	};

	FAtlasTheme MakeTheme(const UFF_UIPalette& Palette)
	{
		FAtlasTheme Theme;
		Theme.Backdrop = Palette.Frontend.Deep;
		Theme.Panel = Palette.Frontend.Glass;
		Theme.PanelRaised = Palette.Frontend.GlassRaised;
		Theme.Edge = Palette.Frontend.Edge;
		Theme.EdgeStrong = Palette.Frontend.EdgeStrong;

		Theme.Title = Palette.Frontend.Title;
		Theme.Text = Palette.Frontend.Text;
		Theme.Sub = Palette.Frontend.Sub;
		Theme.Dim = Palette.Frontend.Dim;

		// 강조색은 프로젝트에 하나뿐이다. 준비 단계 HUD 의 `고용` 버튼도 같은 계열을 쓴다.
		Theme.Accent = Palette.InGame.Teal;
		Theme.AccentHover = Palette.InGame.BandGood;
		Theme.AccentPressed = Palette.InGame.TealDeep;
		Theme.AccentText = Palette.InGame.White;
		return Theme;
	}

	// 모서리 반경. 준비 단계 HUD 와 같은 단계(4 / 6 / 8)를 쓴다.
	constexpr float ChipRadius = 4.f;
	constexpr float ButtonRadius = 6.f;
	constexpr float CardRadius = 10.f;

	// =========================================================================
	// 텍스처
	// =========================================================================

	struct FTextureSpec
	{
		const TCHAR* Key;
		const TCHAR* RelativeSource;
		const TCHAR* RelativePackage;
	};

	// 🔴 **지도 세 장만 남았다.**
	//
	// `T_FF_FA_Map_KoreaNaturalADM1`(RGB 합성본)은 알파가 없어 UMG에서 불투명 사각형이 된다.
	// 그래서 알파가 있는 TerrainEdgeSafe 위에 ADM1Borders 를 얹는다. EdgeSafe 판은 축소 필터링
	// 때 해안에 1px 검은 테두리가 생기지 않도록 만든 런타임 권장본이다.
	//
	// 축제 썸네일은 UI 장식이 아니라 **콘텐츠**라 남긴다. 실제 값은 카탈로그가 넣고,
	// 여기 있는 것은 디자이너에서 카드를 볼 때 쓰는 미리보기다.
	//
	// 팩의 나머지(금장 패널·알약·키캡·버튼·마커·바다·비네트)는 더 이상 임포트하지 않는다.
	// 이미 `/Game/FestaFiesta/UI/Atlas/Korea/` 에 들어와 있는 것은 지워도 된다.
	const TArray<FTextureSpec> RequiredTextures =
	{
		{ TEXT("Terrain"), TEXT("Map/T_FF_FA_Map_KoreaTerrainEdgeSafe_2400x2160.png"),
			TEXT("Map/T_FF_FA_Map_KoreaTerrainEdgeSafe_2400x2160") },
		{ TEXT("Borders"), TEXT("Map/T_FF_FA_Map_KoreaADM1Borders_2400x2160.png"),
			TEXT("Map/T_FF_FA_Map_KoreaADM1Borders_2400x2160") },
		{ TEXT("BusanOverlay"), TEXT("Overlays/T_FF_FA_Overlay_ADM1_08_Busan_Selected_2400x2160.png"),
			TEXT("Overlays/T_FF_FA_Overlay_ADM1_08_Busan_Selected_2400x2160") },
		{ TEXT("DadaepoPreview"), TEXT("Cards/T_FF_FA_Preview_Dadaepo_512x256.png"),
			TEXT("Cards/T_FF_FA_Preview_Dadaepo_512x256") },
	};

	/**
	 * 임포트한 텍스처 모음.
	 *
	 * 이름 붙은 멤버 스물몇 개 대신 맵으로 든다. 파츠가 늘 때마다 구조체·대입·`IsComplete`
	 * 세 곳을 같이 고쳐야 했고, 한 곳을 빠뜨리면 `nullptr` 브러시가 조용히 흰 네모로 뜬다.
	 */
	struct FNaturalAssets
	{
		TMap<FName, UTexture2D*> ByKey;

		UTexture2D* Get(const TCHAR* Key) const
		{
			UTexture2D* const* Found = ByKey.Find(FName(Key));
			return Found ? *Found : nullptr;
		}

		bool IsComplete() const
		{
			for (const FTextureSpec& Spec : RequiredTextures)
			{
				if (Get(Spec.Key) == nullptr)
				{
					return false;
				}
			}
			return true;
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

	/**
	 * 🔴 텍스처 없이 단색을 칠하는 브러시.
	 *
	 * `FSlateBrush` 기본값은 `DrawAs = Image` + `ImageType = NoImage` 다. 그 상태로 색만 넣으면
	 * **`UBorder` 는 아무것도 그리지 않고 `UImage` 는 흰 사각형을 그린다.** 단색을 텍스처 없이
	 * 칠하는 것은 `RoundedBox` 뿐이다. 같은 함정이 `FF_PrepHudWbpBuilder.cpp` 주석에도 있다.
	 *
	 * 기본 `RoundingType` 이 `HalfHeightRadius` 라 **그대로 두면 바가 알약이 된다.**
	 * 그래서 여기서는 반경을 고정으로 못 박는다. 알약·원이 필요하면 `MakeRoundBrush` 를 쓴다.
	 */
	FSlateBrush MakeSolidBrush(
		const FLinearColor& Fill,
		const float CornerRadius,
		const FLinearColor& OutlineColor = FLinearColor::Transparent,
		const float OutlineWidth = 0.f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(
			CornerRadius, FSlateColor(OutlineColor), OutlineWidth);
		return Brush;
	}

	/**
	 * 알약·원. 반경을 세로 절반으로 두면 정사각형은 원이 되고 가로로 긴 것은 알약이 된다.
	 * 문구 길이에 따라 폭이 변해도 끝이 항상 반원이라, 9-Slice 처럼 크기에 묶이지 않는다.
	 */
	FSlateBrush MakeRoundBrush(
		const FLinearColor& Fill,
		const FLinearColor& OutlineColor = FLinearColor::Transparent,
		const float OutlineWidth = 0.f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FSlateColor(OutlineColor), OutlineWidth);
		return Brush;
	}

	/** 같은 색을 브러시와 위젯 양쪽에 넣으면 두 번 곱해져 어두워진다. 위젯 쪽은 항상 흰색이다. */
	void PaintBorder(UBorder& Border, const FSlateBrush& Brush)
	{
		Border.SetBrush(Brush);
		Border.SetBrushColor(FLinearColor::White);
	}

	void PaintImage(UImage& Image, const FSlateBrush& Brush)
	{
		Image.SetBrush(Brush);
		Image.SetColorAndOpacity(FLinearColor::White);
		Image.SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	/** 반투명 강조색. 선택 후광처럼 색 자체는 같고 진하기만 다를 때 쓴다. */
	FLinearColor WithAlpha(const FLinearColor& Color, const float Alpha)
	{
		return FLinearColor(Color.R, Color.G, Color.B, Alpha);
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

	bool ImportRequiredTextures(FNaturalAssets& OutAssets)
	{
		// 하나라도 원본이 없으면 임포트를 시작조차 하지 않는다.
		// 절반만 들어간 상태로 WBP를 만들면 어느 파츠가 빠졌는지 화면에서만 보인다.
		bool bAllSourcesFound = true;
		for (const FTextureSpec& Spec : RequiredTextures)
		{
			const FString AbsoluteSource = FPaths::ConvertRelativePathToFull(SourceRoot / Spec.RelativeSource);
			if (!IFileManager::Get().FileExists(*AbsoluteSource))
			{
				bAllSourcesFound = false;
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_PREFLIGHT_FAIL: 원본 PNG가 없다: %s"), LogPrefix, *AbsoluteSource);
			}
		}
		if (!bAllSourcesFound)
		{
			return false;
		}

		for (const FTextureSpec& Spec : RequiredTextures)
		{
			UTexture2D* Texture = ImportTextureIfMissing(Spec);
			if (!Texture)
			{
				return false;
			}
			OutAssets.ByKey.Add(FName(Spec.Key), Texture);
		}

		return OutAssets.IsComplete();
	}

	// =========================================================================
	// CommonButtonStyle (CBS_) 에셋
	// =========================================================================

	/**
	 * 🔴 **`UCommonButtonBase` 는 `Style` 이 비면 Slate 기본 회색 버튼을 그린다.**
	 *
	 * 첫 판에서 지도 핀 뒤에 회색 네모가 떴던 원인이 이것이다. 핀은 이미지 자체가 전부라
	 * 배경이 있으면 안 되고, 시작 버튼은 팩의 빨간 9-Slice를 써야 한다.
	 * `Style`은 `TSubclassOf`라서 값을 채우려면 **에셋이 반드시 필요하다** — 그래서 만든다.
	 */
	UClass* EnsureButtonStyle(
		const FString& PackageName,
		const bool bForce,
		TFunctionRef<void(UCommonButtonStyle&)> Configure)
	{
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *MakeObjectPath(PackageName));

		if (Blueprint && !bForce)
		{
			UE_LOG(LogFFUIWbpBuilder, Display,
				TEXT("%s_STYLE_REUSE: %s 이미 존재. 디자이너 수정을 보존했다."), LogPrefix, *PackageName);
			return Blueprint->GeneratedClass.Get();
		}

		if (!Blueprint)
		{
			if (FPackageName::DoesPackageExist(PackageName))
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_STYLE_FAIL: %s 패키지가 있지만 UBlueprint로 로드되지 않는다."),
					LogPrefix, *PackageName);
				return nullptr;
			}

			UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
			Factory->ParentClass = UCommonButtonStyle::StaticClass();
			// 클래스 선택 창을 띄우면 콘솔 명령이 그 자리에서 멈춘다.
			Factory->bSkipClassPicker = true;

			FAssetToolsModule& AssetTools =
				FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
			Blueprint = Cast<UBlueprint>(AssetTools.Get().CreateAsset(
				FPackageName::GetLongPackageAssetName(PackageName),
				FPaths::GetPath(PackageName),
				UBlueprint::StaticClass(),
				Factory));
		}

		UClass* GeneratedClass = Blueprint ? Blueprint->GeneratedClass.Get() : nullptr;
		UCommonButtonStyle* StyleDefaults = GeneratedClass
			? Cast<UCommonButtonStyle>(GeneratedClass->GetDefaultObject())
			: nullptr;
		if (!Blueprint || !StyleDefaults)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_STYLE_FAIL: %s 생성 실패."), LogPrefix, *PackageName);
			return nullptr;
		}

		Blueprint->Modify();
		StyleDefaults->Modify();
		StyleDefaults->PreEditChange(nullptr);
		Configure(*StyleDefaults);
		StyleDefaults->PostEditChange();

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		Blueprint->MarkPackageDirty();

		UPackage* Package = Blueprint->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		if (!UPackage::SavePackage(Package, Blueprint, *Filename, SaveArgs))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_STYLE_FAIL: %s 저장 실패."), LogPrefix, *PackageName);
			return nullptr;
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_STYLE_CREATE: %s"), LogPrefix, *PackageName);
		return Blueprint->GeneratedClass.Get();
	}

	void ConfigureTransparentStyle(UCommonButtonStyle& Style)
	{
		// 핀과 브레드크럼처럼 **배경이 없어야 하는** 버튼용이다.
		// 상태 표시는 각 WBP의 `Image_FocusRing`과 핀 이미지 교대가 맡는다.
		const FSlateBrush None = FSlateNoResource();
		Style.bSingleMaterial = false;
		Style.NormalBase = None;
		Style.NormalHovered = None;
		Style.NormalPressed = None;
		Style.SelectedBase = None;
		Style.SelectedHovered = None;
		Style.SelectedPressed = None;
		Style.Disabled = None;
		Style.ButtonPadding = FMargin(0.f);
		Style.CustomPadding = FMargin(0.f);
		Style.MinWidth = 0;
		Style.MinHeight = 0;
	}

	/** `축제 시작` 용 빨간 9-Slice. 상태별로 브러시가 바뀐다 — 색만으로 알리지 않는다 (AT-011). */
	/** `축제 시작` 용. 텍스처 없이 단색 둥근 사각형 네 상태다. */
	void ConfigurePrimaryStyle(UCommonButtonStyle& Style, const FAtlasTheme& Theme)
	{
		const FSlateBrush Normal = MakeSolidBrush(Theme.Accent, ButtonRadius);
		const FSlateBrush Hovered = MakeSolidBrush(Theme.AccentHover, ButtonRadius);
		const FSlateBrush Pressed = MakeSolidBrush(Theme.AccentPressed, ButtonRadius);

		Style.bSingleMaterial = false;
		Style.NormalBase = Normal;
		Style.NormalHovered = Hovered;
		Style.NormalPressed = Pressed;
		// 이 버튼은 토글이 아니라서 Selected 상태로 들어갈 일이 없다.
		// 그래도 비워 두면 CommonUI 가 빈 브러시를 그려 버튼이 사라지므로 Normal 과 같게 채운다.
		Style.SelectedBase = Normal;
		Style.SelectedHovered = Hovered;
		Style.SelectedPressed = Pressed;
		Style.Disabled = MakeSolidBrush(Theme.Panel, ButtonRadius, Theme.Edge, 1.f);
		Style.ButtonPadding = FMargin(20.f, 8.f);
		Style.CustomPadding = FMargin(0.f);
		Style.MinWidth = 0;
		Style.MinHeight = 0;
	}

	// =========================================================================
	// WBP CDO 값 쓰기
	// =========================================================================

	/**
	 * WBP의 CDO 기본값을 리플렉션으로 쓴다.
	 *
	 * 위젯 트리를 만든 **뒤에야** `GeneratedClass`/CDO가 생기므로 레이아웃 콜백 안에서는 못 한다.
	 * `Style`·`bTintLabel` 처럼 부모 C++ 클래스의 `EditDefaultsOnly` 값이 대상이다.
	 */
	template <typename TPropertyType, typename TValueType>
	bool SetCdoValue(UWidgetBlueprint& Blueprint, const TCHAR* PropertyName, const TValueType& Value)
	{
		UClass* GeneratedClass = Blueprint.GeneratedClass.Get();
		UObject* Defaults = GeneratedClass ? GeneratedClass->GetDefaultObject() : nullptr;
		TPropertyType* Property = GeneratedClass
			? FindFProperty<TPropertyType>(GeneratedClass, PropertyName)
			: nullptr;
		if (!Defaults || !Property)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: %s 에서 `%s` 기본값을 찾지 못했다."),
				LogPrefix, *Blueprint.GetName(), PropertyName);
			return false;
		}

		Blueprint.Modify();
		Defaults->Modify();
		Property->SetPropertyValue_InContainer(Defaults, Value);
		return true;
	}

	/** `TSubclassOf` 계열은 `FClassProperty` 라 위 템플릿과 대입 방식이 다르다. */
	bool SetCdoClass(
		UWidgetBlueprint& Blueprint,
		const TCHAR* PropertyName,
		UClass* Value,
		const bool bOverwriteExisting)
	{
		UClass* GeneratedClass = Blueprint.GeneratedClass.Get();
		UObject* Defaults = GeneratedClass ? GeneratedClass->GetDefaultObject() : nullptr;
		FClassProperty* Property = GeneratedClass
			? FindFProperty<FClassProperty>(GeneratedClass, PropertyName)
			: nullptr;
		if (!Defaults || !Property)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: %s 에서 `%s` 기본값을 찾지 못했다."),
				LogPrefix, *Blueprint.GetName(), PropertyName);
			return false;
		}

		if (UClass* Existing = Cast<UClass>(Property->GetObjectPropertyValue_InContainer(Defaults)))
		{
			if (!bOverwriteExisting)
			{
				if (Existing != Value)
				{
					UE_LOG(LogFFUIWbpBuilder, Warning,
						TEXT("%s_PRESERVE: %s 의 `%s` 기존 값 `%s` 를 덮어쓰지 않았다."),
						LogPrefix, *Blueprint.GetName(), PropertyName, *Existing->GetPathName());
				}
				return true;
			}
		}

		Blueprint.Modify();
		Defaults->Modify();
		Property->SetObjectPropertyValue_InContainer(Defaults, Value);
		return true;
	}

	/**
	 * `FLinearColor` 같은 구조체 기본값. `FStructProperty` 라 위 둘과 대입 방식이 또 다르다.
	 *
	 * 🔴 이게 필요한 이유는 `UFF_CommonButtonBase::RefreshVisualState` 가 **`bTintLabel` 과 무관하게**
	 *    `Image_Icon` 을 항상 `ContentColor` 로 칠하기 때문이다. 기본
	 *    `HighlightedContentColor` 는 프론트엔드용 남색(#102B4E)이고, 그건 CBS_ 스타일이 호버에서
	 *    배경을 밝게 바꾼다는 전제 위에 있다. 배경이 투명한 Atlas 고스트 버튼은 그 전제가 없어서,
	 *    호버하면 지구본 아이콘과 라벨이 어두운 브레드크럼 패널에 묻혀 사라진다.
	 */
	bool SetCdoLinearColor(
		UWidgetBlueprint& Blueprint,
		const TCHAR* PropertyName,
		const FLinearColor& Value)
	{
		UClass* GeneratedClass = Blueprint.GeneratedClass.Get();
		UObject* Defaults = GeneratedClass ? GeneratedClass->GetDefaultObject() : nullptr;
		FStructProperty* Property = GeneratedClass
			? FindFProperty<FStructProperty>(GeneratedClass, PropertyName)
			: nullptr;
		if (!Defaults || !Property || Property->Struct != TBaseStructure<FLinearColor>::Get())
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: %s 에서 FLinearColor `%s` 기본값을 찾지 못했다."),
				LogPrefix, *Blueprint.GetName(), PropertyName);
			return false;
		}

		Blueprint.Modify();
		Defaults->Modify();
		*Property->ContainerPtrToValuePtr<FLinearColor>(Defaults) = Value;
		return true;
	}

	/** `FVector2D` 기본값. `MapAnchor` 처럼 화면이 읽어 가는 위치 값에 쓴다. */
	bool SetCdoVector2D(
		UWidgetBlueprint& Blueprint,
		const TCHAR* PropertyName,
		const FVector2D& Value)
	{
		UClass* GeneratedClass = Blueprint.GeneratedClass.Get();
		UObject* Defaults = GeneratedClass ? GeneratedClass->GetDefaultObject() : nullptr;
		FStructProperty* Property = GeneratedClass
			? FindFProperty<FStructProperty>(GeneratedClass, PropertyName)
			: nullptr;
		if (!Defaults || !Property || Property->Struct != TBaseStructure<FVector2D>::Get())
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: %s 에서 FVector2D `%s` 기본값을 찾지 못했다."),
				LogPrefix, *Blueprint.GetName(), PropertyName);
			return false;
		}

		Blueprint.Modify();
		Defaults->Modify();
		*Property->ContainerPtrToValuePtr<FVector2D>(Defaults) = Value;
		return true;
	}

	bool SaveBlueprintDefaults(UWidgetBlueprint& Blueprint)
	{
		UClass* GeneratedClass = Blueprint.GeneratedClass.Get();
		if (UObject* Defaults = GeneratedClass ? GeneratedClass->GetDefaultObject() : nullptr)
		{
			Defaults->PostEditChange();
		}
		Blueprint.MarkPackageDirty();

		UPackage* Package = Blueprint.GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		if (!UPackage::SavePackage(Package, &Blueprint, *Filename, SaveArgs))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: %s 기본값 저장 실패."), LogPrefix, *Blueprint.GetPathName());
			return false;
		}
		return true;
	}

	// =========================================================================
	// 캔버스 배치 헬퍼
	// =========================================================================
	//
	// 🔴 절대 좌표만 쓰면 16:9가 아닌 창에서 카드가 화면 한가운데로 밀려온다.
	//    `UIScaleRule=ShortestSide` 는 배율만 맞출 뿐 남는 가로 폭을 나눠 주지 않는다.
	//    그래서 카드는 오른쪽 모서리, 안내 바는 아래 가운데에 앵커를 건다.

	UCanvasPanelSlot* PlaceStretched(UCanvasPanel& Canvas, UWidget& Child, const FMargin& Insets)
	{
		UCanvasPanelSlot* PlacedSlot = Canvas.AddChildToCanvas(&Child);
		if (!PlacedSlot)
		{
			return nullptr;
		}
		PlacedSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		PlacedSlot->SetOffsets(Insets);
		return PlacedSlot;
	}

	UCanvasPanelSlot* PlaceTopRight(
		UCanvasPanel& Canvas, UWidget& Child,
		const float RightInset, const float Y, const float W, const float H)
	{
		UCanvasPanelSlot* PlacedSlot = Canvas.AddChildToCanvas(&Child);
		if (!PlacedSlot)
		{
			return nullptr;
		}
		PlacedSlot->SetAnchors(FAnchors(1.f, 0.f));
		PlacedSlot->SetAlignment(FVector2D(1.f, 0.f));
		PlacedSlot->SetAutoSize(false);
		PlacedSlot->SetPosition(FVector2D(-RightInset, Y));
		PlacedSlot->SetSize(FVector2D(W, H));
		return PlacedSlot;
	}

	/** 내용 크기에 맞춰 늘어나는 배치. 브레드크럼·알약처럼 문구 길이가 데이터인 것에 쓴다. */
	UCanvasPanelSlot* PlaceAuto(
		UCanvasPanel& Canvas, UWidget& Child,
		const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position)
	{
		UCanvasPanelSlot* PlacedSlot = Canvas.AddChildToCanvas(&Child);
		if (!PlacedSlot)
		{
			return nullptr;
		}
		PlacedSlot->SetAnchors(Anchors);
		PlacedSlot->SetAlignment(Alignment);
		PlacedSlot->SetAutoSize(true);
		PlacedSlot->SetPosition(Position);
		return PlacedSlot;
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

	/** 가로 상자에 자동 크기로 넣는다. 세로 가운데 정렬 + 오른쪽 여백. */
	UHorizontalBoxSlot* AddToRow(UHorizontalBox& Row, UWidget& Child, const float RightPadding)
	{
		UHorizontalBoxSlot* RowSlot = Row.AddChildToHorizontalBox(&Child);
		if (RowSlot)
		{
			RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			RowSlot->SetVerticalAlignment(VAlign_Center);
			RowSlot->SetPadding(FMargin(0.f, 0.f, RightPadding, 0.f));
		}
		return RowSlot;
	}

	/** 세로 상자에 자동 높이로 넣는다. 가로는 채우고 위쪽 여백만 준다. */
	UVerticalBoxSlot* AddToColumn(UVerticalBox& Column, UWidget& Child, const float TopPadding)
	{
		UVerticalBoxSlot* ColumnSlot = Column.AddChildToVerticalBox(&Child);
		if (ColumnSlot)
		{
			ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			ColumnSlot->SetHorizontalAlignment(HAlign_Fill);
			ColumnSlot->SetPadding(FMargin(0.f, TopPadding, 0.f, 0.f));
		}
		return ColumnSlot;
	}

	/** 겹쳐 그리는 원. 마커 각 겹이 같은 중심에 오도록 위쪽 여백으로 맞춘다. */
	void AddMarkerLayer(UOverlay& Overlay, UImage& Layer, const float Size, const float BandHeight)
	{
		Layer.SetDesiredSizeOverride(FVector2D(Size));
		if (UOverlaySlot* Slot = Overlay.AddChildToOverlay(&Layer))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Top);
			Slot->SetPadding(FMargin(0.f, (BandHeight - Size) * 0.5f, 0.f, 0.f));
		}
	}

	// =========================================================================
	// WBP_FF_AtlasPin_Natural — 점 마커 + 이름
	// =========================================================================
	//
	// 🔴 **마커 밴드 높이(`PinBandHeight`)와 `MapAnchor` 는 같이 움직인다.**
	//    지도 좌표가 닿아야 하는 곳은 점의 한가운데이고, 그 지점은 위젯 높이 대비
	//    `(밴드/2) / 전체높이` 다. 한쪽만 고치면 핀이 지도에서 통째로 어긋난다.

	constexpr float PinBandHeight = 40.f;
	constexpr float PinWidth = 160.f;
	constexpr float PinHeight = 80.f;

	bool BuildNaturalPin(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const FAtlasTheme& Theme)
	{
		USizeBox* Root = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Root"), true);
		UOverlay* Overlay = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Root"));
		UImage* Focus = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_FocusRing"), true);
		UImage* Halo = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_PinChevrons"), true);
		UImage* LockPin = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_PinLock"), true);
		UImage* DefaultPin = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_PinDefault"), true);
		UImage* SelectedPin = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_PinSelected"), true);
		UBorder* LabelChip = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_PinLabel"));
		UVerticalBox* LabelColumn = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_PinLabel"));
		UCommonTextBlock* Label = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_Label"), true);
		UTextBlock* Badge = MakeText(
			Blueprint, Tree, TEXT("Text_PinBadge"), 10, Theme.Dim,
			LOCTEXT("AtlasPinBadgePlaceholder", "부산"), true);

		if (!Root || !Overlay || !Focus || !Halo || !LockPin || !DefaultPin || !SelectedPin
			|| !LabelChip || !LabelColumn || !Label || !Badge)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetWidthOverride(PinWidth);
		Root->SetHeightOverride(PinHeight);
		Root->AddChild(Overlay);

		// 포커스 링 — 마우스·패드 어느 쪽이든 지금 어디를 보고 있는지 알린다.
		PaintImage(*Focus, MakeRoundBrush(FLinearColor::Transparent, Theme.EdgeStrong, 2.f));
		Focus->SetVisibility(ESlateVisibility::Hidden);
		AddMarkerLayer(*Overlay, *Focus, 40.f, PinBandHeight);

		// 선택 후광. 색만 다른 것이 아니라 **점 바깥에 한 겹이 더 생긴다** (AT-011).
		PaintImage(*Halo, MakeRoundBrush(WithAlpha(Theme.Accent, 0.30f)));
		Halo->SetVisibility(ESlateVisibility::Collapsed);
		AddMarkerLayer(*Overlay, *Halo, 34.f, PinBandHeight);

		// 잠긴 핀 — 속이 빈 것처럼 보이도록 배경색으로 채우고 흐린 테두리만 남긴다.
		// 🔴 이것만으로 잠금을 알리지 않는다. 아래 `Text_PinBadge` 가 `준비 중` 을 함께 띄운다.
		PaintImage(*LockPin, MakeRoundBrush(Theme.Backdrop, Theme.Dim, 2.f));
		LockPin->SetVisibility(ESlateVisibility::Collapsed);
		AddMarkerLayer(*Overlay, *LockPin, 16.f, PinBandHeight);

		// 고를 수 있는 핀. 지도의 초록 위에서도 보이도록 어두운 테두리를 두른다.
		PaintImage(*DefaultPin, MakeRoundBrush(Theme.Sub, Theme.Backdrop, 2.f));
		AddMarkerLayer(*Overlay, *DefaultPin, 14.f, PinBandHeight);

		PaintImage(*SelectedPin, MakeRoundBrush(Theme.Accent, Theme.AccentText, 3.f));
		SelectedPin->SetVisibility(ESlateVisibility::Collapsed);
		AddMarkerLayer(*Overlay, *SelectedPin, 20.f, PinBandHeight);

		// 이름과 상태를 마커 아래에 둔다. 점만 있으면 하나하나 짚어 봐야 뭐가 뭔지 알 수 있다.
		//
		// 🔴 **글자를 지도 위에 그냥 얹지 않는다.** 지형이 밝은 초록이라 흰 글자가 묻힌다.
		//    반투명한 어두운 칩을 깔면 육지 위에서도 바다 위에서도 같은 대비가 나온다.
		StyleText(*Label, 12, Theme.Text, TEXT("Medium"));
		Label->SetText(LOCTEXT("AtlasNaturalPinPlaceholder", "축제 이름"));
		Label->SetJustification(ETextJustify::Center);
		Label->SetAutoWrapText(false);
		if (UVerticalBoxSlot* Slot = LabelColumn->AddChildToVerticalBox(Label))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetHorizontalAlignment(HAlign_Center);
		}

		Badge->SetAutoWrapText(false);
		Badge->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* Slot = LabelColumn->AddChildToVerticalBox(Badge))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetPadding(FMargin(0.f, 1.f, 0.f, 0.f));
		}

		PaintBorder(*LabelChip, MakeSolidBrush(WithAlpha(Theme.Backdrop, 0.62f), ChipRadius));
		LabelChip->SetPadding(FMargin(7.f, 2.f, 7.f, 4.f));
		LabelChip->SetHorizontalAlignment(HAlign_Center);
		LabelChip->SetVerticalAlignment(VAlign_Center);
		LabelChip->SetVisibility(ESlateVisibility::HitTestInvisible);
		LabelChip->AddChild(LabelColumn);
		if (UOverlaySlot* Slot = Overlay->AddChildToOverlay(LabelChip))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Top);
			Slot->SetPadding(FMargin(0.f, PinBandHeight + 2.f, 0.f, 0.f));
		}

		return true;
	}

	// =========================================================================
	// WBP_FF_AtlasPrimaryButton — 카드의 `축제 시작`
	// =========================================================================

	bool BuildPrimaryButton(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const FAtlasTheme& Theme)
	{
		USizeBox* Root = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Root"), true);
		UOverlay* Overlay = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Root"));
		UImage* FocusRing = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_FocusRing"), true);
		UCommonTextBlock* Label = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_Label"), true);

		if (!Root || !Overlay || !FocusRing || !Label)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetHeightOverride(48.f);
		Root->AddChild(Overlay);

		// 배경은 `CBS_FF_AtlasPrimary` 가 그린다. 여기 링은 **포커스 전용**이다 —
		// CommonButtonStyle 의 브러시는 호버에서만 바뀌고 포커스에서는 바뀌지 않는다.
		PaintImage(*FocusRing, MakeSolidBrush(
			FLinearColor::Transparent, ButtonRadius, Theme.AccentText, 2.f));
		FocusRing->SetVisibility(ESlateVisibility::Hidden);
		if (UOverlaySlot* Slot = Overlay->AddChildToOverlay(FocusRing))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}

		StyleText(*Label, 16, Theme.AccentText, TEXT("SemiBold"));
		Label->SetText(LOCTEXT("AtlasNaturalStartPlaceholder", "축제 시작"));
		Label->SetJustification(ETextJustify::Center);
		if (UOverlaySlot* Slot = Overlay->AddChildToOverlay(Label))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		return true;
	}

	// =========================================================================
	// WBP_FF_AtlasGhostButton — 브레드크럼의 `지구본`
	// =========================================================================

	bool BuildGhostButton(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const FAtlasTheme& Theme)
	{
		USizeBox* Root = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Root"), true);
		UOverlay* Overlay = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Root"));
		UImage* FocusRing = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_FocusRing"), true);
		UCommonTextBlock* Label = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_Label"), true);

		if (!Root || !Overlay || !FocusRing || !Label)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetHeightOverride(30.f);
		Root->AddChild(Overlay);

		// 평소에는 글자만 있고, 포커스가 오면 테두리 칩이 생긴다.
		PaintImage(*FocusRing, MakeSolidBrush(
			FLinearColor::Transparent, ChipRadius, Theme.EdgeStrong, 1.f));
		FocusRing->SetVisibility(ESlateVisibility::Hidden);
		if (UOverlaySlot* Slot = Overlay->AddChildToOverlay(FocusRing))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}

		StyleText(*Label, 18, Theme.Sub, TEXT("Medium"));
		Label->SetText(LOCTEXT("AtlasNaturalGlobe", "지구본"));
		if (UOverlaySlot* Slot = Overlay->AddChildToOverlay(Label))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(8.f, 0.f, 8.f, 0.f));
		}

		return true;
	}

	// =========================================================================
	// WBP_FF_AtlasCountry_Natural
	// =========================================================================

	/** 하단 안내의 키 칩. 문구 길이에 맞춰 늘어난다. */
	UBorder* MakeKeyChip(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const FAtlasTheme& Theme,
		const FName Name,
		const FText& KeyText)
	{
		UBorder* Chip = MakeWidget<UBorder>(Blueprint, Tree, Name);
		UTextBlock* Text = MakeText(
			Blueprint, Tree, FName(*(Name.ToString() + TEXT("_Text"))), 12, Theme.Title, KeyText);
		if (!Chip || !Text)
		{
			return nullptr;
		}

		PaintBorder(*Chip, MakeSolidBrush(Theme.Panel, ChipRadius, Theme.Edge, 1.f));
		Chip->SetPadding(FMargin(10.f, 3.f, 10.f, 5.f));
		Chip->SetHorizontalAlignment(HAlign_Center);
		Chip->SetVerticalAlignment(VAlign_Center);
		// 칩 안 글자는 줄바꿈하지 않는다. 자동 줄바꿈이 켜져 있으면 좁은 칩에서 세로로 쪼개진다.
		Text->SetAutoWrapText(false);
		Chip->AddChild(Text);
		return Chip;
	}

	bool BuildNaturalCountry(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		UClass* PrimaryButtonClass,
		UClass* GhostButtonClass,
		const FAtlasTheme& Theme,
		const FNaturalAssets& Assets)
	{
		if (!PrimaryButtonClass || !GhostButtonClass || !Assets.IsComplete())
		{
			return false;
		}

		// --- 배경과 지도 ---
		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Root"));
		UImage* Backdrop = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Backdrop"));
		UCanvasPanel* MapCanvas = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Map"), true);
		UImage* Map = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Map"), true);
		UImage* SelectedRegion = MakeWidget<UImage>(
			Blueprint, Tree, TEXT("Image_SelectedRegionOverlay"), true);
		UImage* MapBorders = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_MapBorders"), true);

		// --- 브레드크럼 ---
		UHorizontalBox* BreadcrumbRow = MakeWidget<UHorizontalBox>(
			Blueprint, Tree, TEXT("HorizontalBox_Breadcrumb"));
		UUserWidget* BackButton = MakeWidgetOfClass(
			Blueprint, Tree, TEXT("Button_Back"), GhostButtonClass);
		UTextBlock* Separator = MakeText(
			Blueprint, Tree, TEXT("Text_BreadcrumbSeparator"), 18, Theme.Dim,
			LOCTEXT("AtlasNaturalSeparator", "›"));
		UTextBlock* CountryName = MakeText(
			Blueprint, Tree, TEXT("Text_CountryName"), 20, Theme.Title,
			LOCTEXT("AtlasNaturalCountry", "대한민국"), true);

		// --- 오른쪽 카드 ---
		USizeBox* CardBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Card"));
		UBorder* Card = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Panel_FestivalCard"), true);
		UVerticalBox* CardColumn = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Card"));
		USizeBox* PreviewBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Preview"));
		UImage* Preview = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_Preview"), true);
		UTextBlock* RegionName = MakeText(
			Blueprint, Tree, TEXT("Text_RegionName"), 12, Theme.Sub,
			LOCTEXT("AtlasNaturalRegion", "부산"), true);
		UTextBlock* FestivalName = MakeText(
			Blueprint, Tree, TEXT("Text_FestivalName"), 24, Theme.Title,
			LOCTEXT("AtlasNaturalFestival", "다대포 바다축제"), true);

		UHorizontalBox* StatusRow = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Status"));
		UBorder* Pill = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Panel_AvailabilityPill"), true);
		UTextBlock* Availability = MakeText(
			Blueprint, Tree, TEXT("Text_Availability"), 12, Theme.Title,
			LOCTEXT("AtlasNaturalAvailable", "플레이 가능"), true);
		UTextBlock* OperatingDays = MakeText(
			Blueprint, Tree, TEXT("Text_OperatingDays"), 12, Theme.Sub,
			LOCTEXT("AtlasNaturalDays", "운영 3일"), true);

		UTextBlock* LockReason = MakeText(
			Blueprint, Tree, TEXT("Text_LockReason"), 13, Theme.Dim,
			LOCTEXT("AtlasNaturalLocked", "다음 업데이트에서 열립니다"), true);
		USizeBox* StartSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_StartFestival"));
		UUserWidget* StartButton = MakeWidgetOfClass(
			Blueprint, Tree, TEXT("Button_StartFestival"), PrimaryButtonClass);

		UTextBlock* EmptyHint = MakeText(
			Blueprint, Tree, TEXT("Text_EmptyHint"), 15, Theme.Dim,
			LOCTEXT("AtlasNaturalEmpty", "지도에서 축제를 고르세요"), true);

		// --- 하단 입력 안내 ---
		UHorizontalBox* GuideRow = MakeWidget<UHorizontalBox>(
			Blueprint, Tree, TEXT("HorizontalBox_ActionGuide"));
		UBorder* KeyMove = MakeKeyChip(Blueprint, Tree, Theme, TEXT("Chip_Move"),
			LOCTEXT("AtlasNaturalKeyMove", "W A S D"));
		UBorder* KeyEnter = MakeKeyChip(Blueprint, Tree, Theme, TEXT("Chip_Enter"),
			LOCTEXT("AtlasNaturalKeyEnter", "ENTER"));
		UBorder* KeyEsc = MakeKeyChip(Blueprint, Tree, Theme, TEXT("Chip_Esc"),
			LOCTEXT("AtlasNaturalKeyEsc", "ESC"));
		UTextBlock* HintMove = MakeText(Blueprint, Tree, TEXT("Text_HintMove"), 13,
			Theme.Sub, LOCTEXT("AtlasNaturalHintMove", "핀 이동"));
		UTextBlock* HintSelect = MakeText(Blueprint, Tree, TEXT("Text_HintSelect"), 13,
			Theme.Sub, LOCTEXT("AtlasNaturalHintSelect", "선택"));
		UTextBlock* HintBack = MakeText(Blueprint, Tree, TEXT("Text_HintBack"), 13,
			Theme.Sub, LOCTEXT("AtlasNaturalHintBack", "뒤로"));

		if (!Root || !Backdrop || !MapCanvas || !Map || !SelectedRegion || !MapBorders
			|| !BreadcrumbRow || !BackButton || !Separator || !CountryName
			|| !CardBox || !Card || !CardColumn || !PreviewBox || !Preview
			|| !RegionName || !FestivalName
			|| !StatusRow || !Pill || !Availability || !OperatingDays
			|| !LockReason || !StartSize || !StartButton || !EmptyHint
			|| !GuideRow || !KeyMove || !KeyEnter || !KeyEsc
			|| !HintMove || !HintSelect || !HintBack)
		{
			return false;
		}

		Tree.RootWidget = Root;

		// --- 배경 ---
		// 팩의 그려진 바다 이미지를 쓰지 않는다. 팔레트 단색이 나머지 UI 와 톤이 맞고,
		// 지형에 이미 해안 그림자가 구워져 있어 배경 그림이 없어도 떠 보이지 않는다.
		PaintImage(*Backdrop, MakeSolidBrush(Theme.Backdrop, 0.f));
		PlaceStretched(*Root, *Backdrop, FMargin(0.f));

		// --- 지도 ---
		// 지도 영역을 절대 크기로 박지 않는다. 여백만 지정하면 창 비율이 바뀌어도 화면을 채우고,
		// 그 안에서 `EFF_AtlasImageFit::Contain` 이 지도 비율을 지켜 핀 좌표가 어긋나지 않는다.
		MapCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		PlaceStretched(*Root, *MapCanvas, FMargin(200.f, 76.f, 520.f, 84.f));

		// 🔴 세 이미지 모두 `CanvasPanel_Map` 의 **직접 자식**이어야 한다.
		//    런타임(`UFF_AtlasCountryScreen::UpdateMapLayout`)이 슬롯을 `UCanvasPanelSlot` 으로
		//    캐스팅해 셋에 같은 사각형을 넣는다. 한 겹이라도 끼면 경계가 해안선에서 어긋난다.
		//    아래 좌표는 첫 프레임용 자리값이고, 실제 값은 런타임이 매 리사이즈마다 다시 넣는다.
		SetImageTexture(*Map, Assets.Get(TEXT("Terrain")), MapSourceSize);
		PlaceOnCanvas(*MapCanvas, *Map, 0.f, 0.f, 1120.f, 1008.f);

		SetImageTexture(*SelectedRegion, Assets.Get(TEXT("BusanOverlay")), MapSourceSize);
		SelectedRegion->SetVisibility(ESlateVisibility::Collapsed);
		PlaceOnCanvas(*MapCanvas, *SelectedRegion, 0.f, 0.f, 1120.f, 1008.f);

		SetImageTexture(*MapBorders, Assets.Get(TEXT("Borders")), MapSourceSize);
		PlaceOnCanvas(*MapCanvas, *MapBorders, 0.f, 0.f, 1120.f, 1008.f);

		// --- 브레드크럼 ---
		// 판을 깔지 않는다. `지구본` 칩이 곧 뒤로 버튼이고, 포커스가 오면 그 칩에만 테두리가 생긴다.
		PlaceAuto(*Root, *BreadcrumbRow, FAnchors(0.f, 0.f), FVector2D::ZeroVector, FVector2D(36.f, 32.f));
		AddToRow(*BreadcrumbRow, *BackButton, 10.f);
		AddToRow(*BreadcrumbRow, *Separator, 10.f);
		AddToRow(*BreadcrumbRow, *CountryName, 0.f);

		// --- 오른쪽 카드 ---
		// 🔴 절대 좌표로 높이를 잡지 않는다. 세로 상자로 쌓으면 문구가 길어지거나 줄이 늘어도
		//    카드가 알아서 자란다. 예전 판은 y 좌표를 손으로 계산해서 잠금 사유가 들어올 때마다
		//    카드 높이를 다시 맞춰야 했다.
		//    오른쪽 세로 가운데 앵커라 창 비율이 바뀌어도 자리가 유지된다.
		PaintBorder(*Card, MakeSolidBrush(Theme.Panel, CardRadius, Theme.Edge, 1.f));
		Card->SetPadding(FMargin(20.f));
		Card->SetHorizontalAlignment(HAlign_Fill);
		Card->SetVerticalAlignment(VAlign_Fill);
		Card->AddChild(CardColumn);
		CardBox->SetWidthOverride(340.f);
		CardBox->AddChild(Card);
		PlaceAuto(*Root, *CardBox, FAnchors(1.f, 0.5f), FVector2D(1.f, 0.5f), FVector2D(-72.f, 0.f));

		// 런타임은 카탈로그 `PreviewImage` 를 이 브러시에 넣는다. 여기 텍스처는 디자이너 미리보기다.
		SetImageTexture(*Preview, Assets.Get(TEXT("DadaepoPreview")), FVector2D(300.f, 140.f));
		PreviewBox->SetHeightOverride(140.f);
		PreviewBox->AddChild(Preview);
		AddToColumn(*CardColumn, *PreviewBox, 0.f);

		// 지역을 작게 위에, 축제 이름을 크게 아래에 둔다.
		// 목업의 `부산 다대포 바다축제` 를 코드에서 이어 붙이지 않는다 — 두 값은 다른 필드고,
		// 언어에 따라 어순이 달라진다.
		AddToColumn(*CardColumn, *RegionName, 16.f);
		AddToColumn(*CardColumn, *FestivalName, 2.f);

		// 상태 줄: [알약] 운영 N일
		PaintBorder(*Pill, MakeRoundBrush(Theme.PanelRaised, Theme.Edge, 1.f));
		Pill->SetPadding(FMargin(12.f, 3.f, 12.f, 5.f));
		Pill->SetHorizontalAlignment(HAlign_Center);
		Pill->SetVerticalAlignment(VAlign_Center);
		Pill->SetVisibility(ESlateVisibility::HitTestInvisible);
		Availability->SetAutoWrapText(false);
		Pill->AddChild(Availability);
		AddToRow(*StatusRow, *Pill, 10.f);
		OperatingDays->SetAutoWrapText(false);
		AddToRow(*StatusRow, *OperatingDays, 0.f);
		if (UVerticalBoxSlot* StatusSlot = AddToColumn(*CardColumn, *StatusRow, 14.f))
		{
			// 알약이 카드 폭만큼 늘어나면 안 된다. 내용만큼만 차지하고 왼쪽에 붙는다.
			StatusSlot->SetHorizontalAlignment(HAlign_Left);
		}

		// 🔴 시작 버튼과 잠금 사유는 **같은 자리를 나눠 쓴다.** `RefreshFestivalCard` 가 둘 중
		//    하나만 켠다. 세로 상자라 꺼진 쪽이 자리를 차지하지 않고 카드가 알아서 줄어든다.
		LockReason->SetJustification(ETextJustify::Center);
		LockReason->SetVisibility(ESlateVisibility::Collapsed);
		AddToColumn(*CardColumn, *LockReason, 18.f);

		StartSize->SetHeightOverride(48.f);
		StartSize->AddChild(StartButton);
		AddToColumn(*CardColumn, *StartSize, 18.f);

		// 카드가 숨어 있을 때 그 자리에 남는 한 줄이다. 카드 바깥이어야 함께 숨지 않는다.
		EmptyHint->SetJustification(ETextJustify::Center);
		PlaceTopRight(*Root, *EmptyHint, 72.f, 0.f, 340.f, 48.f);
		if (UCanvasPanelSlot* HintSlot = Cast<UCanvasPanelSlot>(EmptyHint->Slot))
		{
			HintSlot->SetAnchors(FAnchors(1.f, 0.5f));
			HintSlot->SetAlignment(FVector2D(1.f, 0.5f));
			HintSlot->SetPosition(FVector2D(-72.f, 0.f));
		}

		// --- 하단 입력 안내 ---
		// 판을 깔지 않는다. 어두운 바다 위라 칩과 글자만으로 충분히 읽힌다.
		PlaceAuto(*Root, *GuideRow, FAnchors(0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -36.f));
		AddToRow(*GuideRow, *KeyMove, 10.f);
		AddToRow(*GuideRow, *HintMove, 28.f);
		AddToRow(*GuideRow, *KeyEnter, 10.f);
		AddToRow(*GuideRow, *HintSelect, 28.f);
		AddToRow(*GuideRow, *KeyEsc, 10.f);
		AddToRow(*GuideRow, *HintBack, 0.f);

		return true;
	}

	// =========================================================================
	// 생성 · 재생성
	// =========================================================================

	/**
	 * 없으면 만들고, `bForce` 면 **위젯 트리만** 다시 굽는다.
	 *
	 * ★ 에셋을 지웠다 만들지 않는다. 경로가 같아야 `DefaultGame.ini` 의 화면 등록과
	 *   국가 WBP가 들고 있는 핀 클래스 참조가 그대로 산다.
	 *
	 * 🔴 낡은 트리를 비우는 대신 **새 `UWidgetTree` 를 만든다.** 같은 트리에 같은 이름으로
	 *    다시 만들면 UObject 이름 충돌이 나서 `Image_Map` 이 `Image_Map_1` 이 되고,
	 *    `BindWidget` 이 조용히 전부 끊긴다.
	 */
	UWidgetBlueprint* EnsureOrRebuild(
		const FString& PackageName,
		UClass& ParentClass,
		const bool bForce,
		FBuildLayout&& BuildLayout)
	{
		UWidgetBlueprint* Existing = FPackageName::DoesPackageExist(PackageName)
			? LoadExistingBlueprint(PackageName)
			: nullptr;

		if (!Existing || !bForce)
		{
			return EnsureWidgetBlueprint(
				LogPrefix, PackageName, FPaths::GetPath(PackageName), ParentClass, MoveTemp(BuildLayout));
		}

		Existing->Modify();
		Existing->WidgetVariableNameToGuidMap.Empty();
		Existing->WidgetTree = NewObject<UWidgetTree>(
			Existing, UWidgetTree::StaticClass(), NAME_None, RF_Transactional);

		if (!BuildLayout(*Existing, *Existing->WidgetTree))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: %s 레이아웃 재구성 실패. 저장하지 않았다."), LogPrefix, *PackageName);
			return nullptr;
		}

		if (!SaveNewBlueprint(LogPrefix, *Existing))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: %s 재생성 저장 실패."), LogPrefix, *PackageName);
			return nullptr;
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_REBUILD: %s 위젯 트리를 다시 만들었다."), LogPrefix, *PackageName);
		return Existing;
	}

	void RunAtlasNaturalUmg(const TArray<FString>& Args, UWorld* World)
	{
		const bool bForce = Args.ContainsByPredicate([](const FString& Arg)
		{
			return Arg.Equals(TEXT("force"), ESearchCase::IgnoreCase);
		});

		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 팔레트를 읽을 수 없다."), LogPrefix);
			return;
		}

		const FAtlasTheme Theme = MakeTheme(*Palette);

		FNaturalAssets Assets;
		if (!ImportRequiredTextures(Assets))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 필수 텍스처 집합을 준비하지 못해 WBP를 만들지 않았다."), LogPrefix);
			return;
		}

		UClass* TransparentStyle = EnsureButtonStyle(
			TransparentStylePackage, bForce, &ConfigureTransparentStyle);
		UClass* PrimaryStyle = EnsureButtonStyle(
			PrimaryStylePackage, bForce,
			[&Theme](UCommonButtonStyle& Style) { ConfigurePrimaryStyle(Style, Theme); });
		if (!TransparentStyle || !PrimaryStyle)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: 버튼 스타일 에셋을 준비하지 못했다. "
					 "스타일이 없으면 CommonUI가 회색 기본 버튼을 그린다."), LogPrefix);
			return;
		}

		// --- 핀 ---
		UWidgetBlueprint* PinBlueprint = EnsureOrRebuild(
			NaturalPinPackage, *UFF_AtlasPinButton::StaticClass(), bForce,
			[&Theme](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildNaturalPin(Blueprint, Tree, Theme);
			});
		if (!PinBlueprint
			|| !SetCdoClass(*PinBlueprint, TEXT("Style"), TransparentStyle, bForce)
			// 🔴 점 마커는 **점의 한가운데**가 지도 좌표에 닿는다. 옛 조형물 핀의 기본값
			//    `(0.5, 1)`(바닥 중앙) 을 그대로 두면 모든 핀이 위로 밀린다.
			//    분자는 마커 밴드의 절반이다 — `PinBandHeight` 를 바꾸면 이 값도 같이 바뀐다.
			|| !SetCdoVector2D(*PinBlueprint, TEXT("MapAnchor"),
				FVector2D(0.5, (PinBandHeight * 0.5f) / PinHeight))
			// 핀 이름도 라벨이라 베이스가 색을 칠한다. 기본 호버 색(남색)을 그대로 두면
			// 마우스를 올리는 순간 어두운 지도 위에서 이름이 사라진다.
			|| !SetCdoLinearColor(*PinBlueprint, TEXT("NormalContentColor"), Theme.Text)
			|| !SetCdoLinearColor(*PinBlueprint, TEXT("HighlightedContentColor"), Theme.Title)
			|| !SetCdoLinearColor(*PinBlueprint, TEXT("DisabledContentColor"), Theme.Dim)
			|| !SaveBlueprintDefaults(*PinBlueprint))
		{
			return;
		}

		UClass* NaturalPinClass = PinBlueprint->GeneratedClass.Get();
		if (!NaturalPinClass || !NaturalPinClass->IsChildOf(UFF_AtlasPinButton::StaticClass()))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: WBP_FF_AtlasPin_Natural의 GeneratedClass가 유효하지 않다."), LogPrefix);
			return;
		}

		// --- 버튼 두 개 ---
		UWidgetBlueprint* PrimaryBlueprint = EnsureOrRebuild(
			PrimaryButtonPackage, *UFF_CommonButtonBase::StaticClass(), bForce,
			[&Theme](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildPrimaryButton(Blueprint, Tree, Theme);
			});
		if (!PrimaryBlueprint
			|| !SetCdoClass(*PrimaryBlueprint, TEXT("Style"), PrimaryStyle, bForce)
			// 🔴 라벨 색을 베이스가 칠하지 못하게 막는다. 켜 두면 호버할 때 라벨이
			//    프론트엔드용 남색(#102B4E)으로 바뀌어 강조색 버튼 위에서 읽히지 않는다.
			|| !SetCdoValue<FBoolProperty>(*PrimaryBlueprint, TEXT("bTintLabel"), false)
			|| !SaveBlueprintDefaults(*PrimaryBlueprint))
		{
			return;
		}

		UWidgetBlueprint* GhostBlueprint = EnsureOrRebuild(
			GhostButtonPackage, *UFF_CommonButtonBase::StaticClass(), bForce,
			[&Theme](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildGhostButton(Blueprint, Tree, Theme);
			});
		if (!GhostBlueprint
			|| !SetCdoClass(*GhostBlueprint, TEXT("Style"), TransparentStyle, bForce)
			// 배경이 투명하므로 호버 표현을 **글자·아이콘 색**으로 한다. 기본값을 그대로 두면
			// 호버할 때 남색으로 바뀌어 어두운 브레드크럼 패널에서 보이지 않는다.
			|| !SetCdoLinearColor(*GhostBlueprint, TEXT("NormalContentColor"), Theme.Sub)
			|| !SetCdoLinearColor(*GhostBlueprint, TEXT("HighlightedContentColor"), Theme.Title)
			|| !SetCdoLinearColor(*GhostBlueprint, TEXT("DisabledContentColor"), Theme.Dim)
			|| !SaveBlueprintDefaults(*GhostBlueprint))
		{
			return;
		}

		UClass* PrimaryButtonClass = PrimaryBlueprint->GeneratedClass.Get();
		UClass* GhostButtonClass = GhostBlueprint->GeneratedClass.Get();
		if (!PrimaryButtonClass || !GhostButtonClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: Atlas 버튼 WBP의 GeneratedClass가 유효하지 않다."), LogPrefix);
			return;
		}

		// --- 국가 지도 화면 ---
		UWidgetBlueprint* CountryBlueprint = EnsureOrRebuild(
			NaturalCountryPackage, *UFF_AtlasCountryScreen::StaticClass(), bForce,
			[PrimaryButtonClass, GhostButtonClass, &Theme, &Assets](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildNaturalCountry(
					Blueprint, Tree, PrimaryButtonClass, GhostButtonClass, Theme, Assets);
			});
		if (!CountryBlueprint
			|| !SetCdoClass(*CountryBlueprint, TEXT("PinButtonClass"), NaturalPinClass, bForce)
			|| !SaveBlueprintDefaults(*CountryBlueprint))
		{
			return;
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_END: 지도 텍스처 %d장과 스타일 2종 · 버튼 WBP 2종 · 핀/국가 WBP를 준비했다. "
				 "UI 는 팔레트 단색으로 그리므로 팩의 금장 파츠는 더 쓰지 않는다. "
				 "이어서 `FF.AtlasNaturalData` 를 실행해 카탈로그 이미지 참조를 갱신한다. "
				 "%s"),
			LogPrefix,
			RequiredTextures.Num(),
			bForce
				? TEXT("(force: 위젯 트리를 다시 구웠다)")
				: TEXT("(비파괴 모드: 이미 있는 에셋은 건드리지 않았다. 다시 구우려면 `FF.AtlasNaturalUMG force`)"));
	}

	// =========================================================================
	// 카탈로그 데이터
	// =========================================================================

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

	UFF_FestivalAtlasCatalog* LoadCatalogForEdit()
	{
		const UFF_UIFlowSettings* Settings = UFF_UIFlowSettings::Get();
		if (!Settings || Settings->AtlasCatalog.IsNull())
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_DATA_FAIL: Project Settings > Game > FestaFiesta UI Flow에 AtlasCatalog가 없다."),
				LogPrefix);
			return nullptr;
		}

		UFF_FestivalAtlasCatalog* Catalog = Settings->AtlasCatalog.LoadSynchronous();
		if (!Catalog)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_DATA_FAIL: AtlasCatalog를 로드하지 못했다: %s"),
				LogPrefix, *Settings->AtlasCatalog.ToSoftObjectPath().ToString());
		}
		return Catalog;
	}

	/**
	 * 명시적으로 호출했을 때에만 기존 Atlas 카탈로그의 이미지 참조를 연결한다.
	 *
	 * `FF.AtlasNaturalUMG`와 분리한 이유는 카탈로그가 팀원이 에디터에서 편집하는 데이터이기
	 * 때문이다. 텍스처/WBP 생성만 다시 실행해도 사용자 데이터가 바뀌어서는 안 된다.
	 */
	// 인자는 `FConsoleCommandWithWorldAndArgsDelegate` 규격이라 `UWorld*` 가 따라온다.
	// 이 도구는 에셋만 만지므로 월드를 쓰지 않는다 — 인자를 지우면 델리게이트가 안 맞는다.
	void RunAtlasNaturalData(const TArray<FString>& Args, UWorld* World)
	{
		UFF_FestivalAtlasCatalog* Catalog = LoadCatalogForEdit();
		if (!Catalog)
		{
			return;
		}

		FName CountryId = NAME_None;
		FName FestivalId = NAME_None;
		bool bOverrideMapPosition = false;
		FVector2D OverrideMapPosition = FVector2D::ZeroVector;
		if (Args.IsEmpty())
		{
			if (Catalog->Countries.Num() != 1)
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_DATA_FAIL: 인자를 생략하려면 국가가 정확히 하나여야 한다. "
						 "현재 Countries=%d. 사용법: FF.AtlasNaturalData <CountryId> <FestivalId> [MapX MapY]"),
					LogPrefix, Catalog->Countries.Num());
				return;
			}

			CountryId = Catalog->Countries[0].CountryId;

			// 잠긴 시드 축제가 늘어난 뒤로 "축제도 하나"라는 가정은 깨졌다.
			// 대표 이미지를 붙일 대상은 **플레이 가능한 축제**다.
			const FFF_FestivalAtlasEntry* Playable = Catalog->Entries.FindByPredicate(
				[CountryId](const FFF_FestivalAtlasEntry& Entry)
				{
					return Entry.CountryId == CountryId && Entry.bUnlocked;
				});
			if (!Playable)
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_DATA_FAIL: `%s` 에 플레이 가능한 축제가 없어 대상을 정할 수 없다. "
						 "FestivalId를 직접 넘긴다."),
					LogPrefix, *CountryId.ToString());
				return;
			}
			FestivalId = Playable->FestivalId;
		}
		else if (Args.Num() == 2 || Args.Num() == 4)
		{
			CountryId = FName(*Args[0]);
			FestivalId = FName(*Args[1]);
			if (Args.Num() == 4)
			{
				if (!Args[2].IsNumeric() || !Args[3].IsNumeric())
				{
					UE_LOG(LogFFUIWbpBuilder, Error,
						TEXT("%s_DATA_FAIL: MapX와 MapY는 0~1 숫자여야 한다."), LogPrefix);
					return;
				}

				OverrideMapPosition = FVector2D(
					FCString::Atod(*Args[2]),
					FCString::Atod(*Args[3]));
				if (OverrideMapPosition.X < 0.0 || OverrideMapPosition.X > 1.0
					|| OverrideMapPosition.Y < 0.0 || OverrideMapPosition.Y > 1.0)
				{
					UE_LOG(LogFFUIWbpBuilder, Error,
						TEXT("%s_DATA_FAIL: MapPosition은 0~1 범위여야 한다. 입력=(%.4f, %.4f)"),
						LogPrefix, OverrideMapPosition.X, OverrideMapPosition.Y);
					return;
				}
				bOverrideMapPosition = true;
			}
		}
		else
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_DATA_FAIL: 사용법: FF.AtlasNaturalData [CountryId FestivalId [MapX MapY]]"),
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

		// 🔴 **알파가 있는 지형을 넣는다.** 예전 값이던 `KoreaNaturalADM1` 은 RGB 합성본이라
		//    자기 바다까지 불투명하게 그려서, 배경 Ocean 위에 지도 사각형 경계선이 그대로 보였다.
		//    경계선은 별도 레이어(`MapBordersImage`)로 얹는다.
		UTexture2D* TerrainTexture = LoadImportedNaturalTexture(
			TEXT("Map/T_FF_FA_Map_KoreaTerrainEdgeSafe_2400x2160"));
		UTexture2D* BordersTexture = LoadImportedNaturalTexture(
			TEXT("Map/T_FF_FA_Map_KoreaADM1Borders_2400x2160"));
		UTexture2D* OverlayTexture = LoadImportedNaturalTexture(
			TEXT("Overlays/T_FF_FA_Overlay_ADM1_08_Busan_Selected_2400x2160"));
		UTexture2D* PreviewTexture = LoadImportedNaturalTexture(
			TEXT("Cards/T_FF_FA_Preview_Dadaepo_512x256"));
		if (!TerrainTexture || !BordersTexture || !OverlayTexture || !PreviewTexture)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_DATA_FAIL: 필요한 Natural 텍스처가 없다. 먼저 FF.AtlasNaturalUMG를 실행한다."),
				LogPrefix);
			return;
		}

		// 네 에셋을 모두 확인한 뒤에만 Modify한다. 중간 실패로 카탈로그가 반쯤 바뀌지 않는다.
		Catalog->Modify();
		Catalog->PreEditChange(nullptr);
		FFF_AtlasCountryEntry& Country = Catalog->Countries[CountryIndex];
		FFF_FestivalAtlasEntry& Festival = Catalog->Entries[FestivalIndex];
		Country.MapImage = TerrainTexture;
		Country.MapBordersImage = BordersTexture;
		Festival.SelectionOverlayImage = OverlayTexture;
		Festival.PreviewImage = PreviewTexture;
		if (bOverrideMapPosition)
		{
			Festival.MapPosition = OverrideMapPosition;
		}
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
			TEXT("%s_DATA_END: %s / %s 에 지형·경계·선택 오버레이·프리뷰%s 연결하고 저장했다: %s"),
			LogPrefix,
			*CountryId.ToString(),
			*FestivalId.ToString(),
			bOverrideMapPosition ? TEXT("와 명시적 MapPosition을") : TEXT("를"),
			*Catalog->GetPathName());
	}

	// =========================================================================
	// 잠긴 축제 시드
	// =========================================================================

	/**
	 * 지도에 잠긴 축제 핀을 채운다.
	 *
	 * === 왜 필요한가 ===
	 * 카탈로그에 다대포 하나뿐이면 대한민국 지도에 핀이 한 개만 떠서, 지도 절반이 빈 채로
	 * "고를 것이 없는 화면"처럼 보인다. 목업(`07-cinematic-dem-adm1-province-boundaries`)도
	 * 잠긴 핀 네 개를 함께 그렸다.
	 *
	 * === `assumption` 이다 ===
	 * 아래 네 축제는 **확정된 콘텐츠 계획이 아니다.** 전부 `bUnlocked=false` · 레벨 없음이라
	 * 시작할 수 없고, 카드에는 잠금 사유만 뜬다. 실제 축제가 정해지면 이 항목을 지우거나 채운다.
	 *
	 * === 좌표 ===
	 * `MapPosition` 은 팩의 등록 규칙을 그대로 쓴다 (`map-registration.json`).
	 *   X = (경도 - 124) / 8 · Y = (39 - 위도) / 6
	 */
	struct FLockedSeed
	{
		const TCHAR* FestivalId;
		const TCHAR* RegionName;
		const TCHAR* FestivalName;
		double Longitude;
		double Latitude;
	};

	const TArray<FLockedSeed> LockedSeeds =
	{
		{ TEXT("Seoul_HanRiverFestival"), TEXT("서울"), TEXT("한강 여름축제"), 126.99, 37.53 },
		{ TEXT("Gangneung_DanoFestival"), TEXT("강릉"), TEXT("강릉 단오제"), 128.90, 37.75 },
		{ TEXT("Jeonju_BibimbapFestival"), TEXT("전주"), TEXT("전주 비빔밥축제"), 127.15, 35.82 },
		{ TEXT("Jeju_FireFestival"), TEXT("제주"), TEXT("제주 들불축제"), 126.55, 33.45 },
	};

	void RunAtlasNaturalSeedLocked(const TArray<FString>& Args, UWorld* World)
	{
		UFF_FestivalAtlasCatalog* Catalog = LoadCatalogForEdit();
		if (!Catalog)
		{
			return;
		}

		FName CountryId = NAME_None;
		if (Args.Num() == 1)
		{
			CountryId = FName(*Args[0]);
		}
		else if (Args.IsEmpty() && Catalog->Countries.Num() == 1)
		{
			CountryId = Catalog->Countries[0].CountryId;
		}
		else
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_SEED_FAIL: 사용법: FF.AtlasNaturalSeedLocked [CountryId]. "
					 "국가가 여럿이면 생략할 수 없다. 현재 Countries=%d"),
				LogPrefix, Catalog->Countries.Num());
			return;
		}

		if (!Catalog->Countries.ContainsByPredicate(
			[CountryId](const FFF_AtlasCountryEntry& Country) { return Country.CountryId == CountryId; }))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_SEED_FAIL: CountryId `%s` 가 카탈로그에 없다."), LogPrefix, *CountryId.ToString());
			return;
		}

		int32 AddedCount = 0;
		TArray<FFF_FestivalAtlasEntry> NewEntries;
		for (const FLockedSeed& Seed : LockedSeeds)
		{
			const FName SeedId(Seed.FestivalId);
			// 🔴 이미 있는 항목은 절대 덮지 않는다. 같은 이름으로 진짜 축제를 만들었을 수 있고,
			//    그 경우 이 명령이 잠금 상태로 되돌리면 플레이 가능한 축제가 조용히 사라진다.
			if (Catalog->Entries.ContainsByPredicate(
				[SeedId](const FFF_FestivalAtlasEntry& Entry) { return Entry.FestivalId == SeedId; }))
			{
				UE_LOG(LogFFUIWbpBuilder, Display,
					TEXT("%s_SEED_SKIP: `%s` 는 이미 있다."), LogPrefix, Seed.FestivalId);
				continue;
			}

			FFF_FestivalAtlasEntry Entry;
			Entry.FestivalId = SeedId;
			Entry.CountryId = CountryId;
			Entry.RegionName = FText::FromString(Seed.RegionName);
			Entry.FestivalName = FText::FromString(Seed.FestivalName);
			Entry.bUnlocked = false;
			Entry.LockReason = LOCTEXT("AtlasNaturalSeedLockReason", "다음 업데이트에서 열립니다");
			Entry.MapPosition = FVector2D(
				(Seed.Longitude - 124.0) / 8.0,
				(39.0 - Seed.Latitude) / 6.0);

			NewEntries.Add(Entry);
			++AddedCount;
		}

		if (AddedCount == 0)
		{
			UE_LOG(LogFFUIWbpBuilder, Display,
				TEXT("%s_SEED_END: 새로 넣을 항목이 없다. 카탈로그를 저장하지 않았다."), LogPrefix);
			return;
		}

		Catalog->Modify();
		Catalog->PreEditChange(nullptr);
		Catalog->Entries.Append(NewEntries);
		Catalog->PostEditChange();
		Catalog->MarkPackageDirty();

		if (!SaveAtlasCatalog(*Catalog))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_SEED_FAIL: 항목 %d개는 메모리에 들어갔지만 카탈로그 저장에 실패했다: %s"),
				LogPrefix, AddedCount, *Catalog->GetPathName());
			return;
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_SEED_END: `%s` 에 잠긴 축제 %d개를 넣었다. 전부 `assumption` 이며 "
				 "레벨이 없어 시작할 수 없다. 실제 콘텐츠가 정해지면 지우거나 채운다."),
			LogPrefix, *CountryId.ToString(), AddedCount);
	}

	// =========================================================================
	// 콘솔 명령
	// =========================================================================

	// !! `FAutoConsoleCommandWithArgs` 라는 클래스는 없다 !!
	// 인자를 받는 자동 등록 클래스는 `WithWorldAndArgs` · `WithArgsAndOutputDevice` ·
	// `WithWorldArgsAndOutputDevice` 뿐이다 (`HAL/IConsoleManager.h`).
	// 델리게이트 타입(`FConsoleCommandWithArgsDelegate`)은 있어서 이름만 보면 짝이 있어 보인다.
	FAutoConsoleCommandWithWorldAndArgs AtlasNaturalUmgCommand(
		TEXT("FF.AtlasNaturalUMG"),
		TEXT("FF.AtlasNaturalUMG [force] - 대한민국 자연 지형 Atlas 텍스처·스타일·WBP를 만든다. "
			 "인자 없이는 비파괴, `force` 면 Natural WBP의 위젯 트리를 다시 굽는다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunAtlasNaturalUmg));

	FAutoConsoleCommandWithWorldAndArgs AtlasNaturalDataCommand(
		TEXT("FF.AtlasNaturalData"),
		TEXT("FF.AtlasNaturalData [CountryId FestivalId [MapX MapY]] - Natural 이미지 참조와 선택적 핀 좌표를 연결한다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunAtlasNaturalData));

	FAutoConsoleCommandWithWorldAndArgs AtlasNaturalSeedLockedCommand(
		TEXT("FF.AtlasNaturalSeedLocked"),
		TEXT("FF.AtlasNaturalSeedLocked [CountryId] - 지도에 잠긴 축제 핀을 채운다. 기존 항목은 덮지 않는다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunAtlasNaturalSeedLocked));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
