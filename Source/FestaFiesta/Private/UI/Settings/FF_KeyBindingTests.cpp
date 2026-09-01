// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "EnhancedActionKeyMapping.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "UI/Settings/FF_KeyBindingLayoutMetrics.h"
#include "UI/Settings/FF_KeyBindingPanel.h"

namespace FFKeyBindingTests
{
	constexpr const TCHAR* PanelGeneratedClassPath =
		TEXT("/Game/FestaFiesta/UI/Settings/WBP_FF_KeyBindingPanel.WBP_FF_KeyBindingPanel_C");
	constexpr const TCHAR* PanelNativeClassPath =
		TEXT("/Script/FestaFiesta.FF_KeyBindingPanel");
	constexpr const TCHAR* ArtworkTexturePackagePath =
		TEXT("/Game/FestaFiesta/UI/Settings/Textures/T_UI_Settings_Keyboard_ImageGen_v2");

	FString MakeAssetObjectPath(const TCHAR* PackagePath)
	{
		const FString PackagePathString(PackagePath);
		return FString::Printf(
			TEXT("%s.%s"),
			*PackagePathString,
			*FPackageName::GetLongPackageAssetName(PackagePathString));
	}

	const UWidgetTree* LoadPanelWidgetTree(FAutomationTestBase& Test)
	{
		UClass* PanelClass = LoadClass<UUserWidget>(nullptr, PanelGeneratedClassPath);
		Test.TestNotNull(TEXT("WBP_FF_KeyBindingPanel generated class loads"), PanelClass);
		if (!PanelClass)
		{
			return nullptr;
		}

		const UWidgetBlueprintGeneratedClass* WidgetClass =
			Cast<UWidgetBlueprintGeneratedClass>(PanelClass);
		Test.TestNotNull(
			TEXT("WBP_FF_KeyBindingPanel is a WidgetBlueprintGeneratedClass"),
			WidgetClass);
		if (!WidgetClass)
		{
			return nullptr;
		}

		const UWidgetTree* WidgetTree = WidgetClass->GetWidgetTreeArchetype();
		Test.TestNotNull(TEXT("WBP_FF_KeyBindingPanel has a compiled WidgetTree"), WidgetTree);
		return WidgetTree;
	}

	bool SourceFormatSupportsAlpha(const ETextureSourceFormat Format)
	{
		switch (Format)
		{
		case TSF_BGRA8:
		case TSF_RGBA16:
		case TSF_RGBA16F:
		case TSF_RGBA32F:
			return true;
		default:
			return false;
		}
	}

	bool SourceContainsTransparency(FTextureSource& Source)
	{
		if (!Source.IsValid() || Source.GetFormat() != TSF_BGRA8)
		{
			return false;
		}

		const uint8* Pixels = Source.LockMipReadOnly(0);
		if (!Pixels)
		{
			return false;
		}

		const int64 PixelCount = static_cast<int64>(Source.GetSizeX()) * Source.GetSizeY();
		bool bContainsTransparency = false;
		for (int64 PixelIndex = 0; PixelIndex < PixelCount; ++PixelIndex)
		{
			if (Pixels[PixelIndex * 4 + 3] < 255)
			{
				bContainsTransparency = true;
				break;
			}
		}
		Source.UnlockMip(0);
		return bContainsTransparency;
	}

	float ContrastRatio(const FLinearColor& A, const FLinearColor& B)
	{
		const auto Luminance = [](const FLinearColor& Color)
		{
			return 0.2126f * Color.R + 0.7152f * Color.G + 0.0722f * Color.B;
		};
		const float Lighter = FMath::Max(Luminance(A), Luminance(B));
		const float Darker = FMath::Min(Luminance(A), Luminance(B));
		return (Lighter + 0.05f) / (Darker + 0.05f);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_KeyBindingDefaultKeyFallbackContractTest,
	"FestaFiesta.UI.Settings.KeyBinding.DefaultKeyFallbackContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_KeyBindingDefaultKeyFallbackContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const UInputMappingContext* CameraContext = LoadObject<UInputMappingContext>(
		nullptr, TEXT("/Game/FestaFiesta/Input/IMC_Camera.IMC_Camera"));
	const UInputMappingContext* CharacterContext = LoadObject<UInputMappingContext>(
		nullptr, TEXT("/Game/FestaFiesta/Input/Character/IMC_Character.IMC_Character"));
	TestNotNull(TEXT("기본 카메라 IMC를 로드한다"), CameraContext);
	TestNotNull(TEXT("기본 캐릭터 IMC를 로드한다"), CharacterContext);

	struct FExpectedDefault
	{
		FName MappingName;
		FName ActionName;
		FKey Key;
		bool bCharacter = false;
	};
	const FExpectedDefault Defaults[] = {
		{ TEXT("FF.Camera.Move.Forward"), TEXT("IA_Pan"), EKeys::W },
		{ TEXT("FF.Camera.Move.Backward"), TEXT("IA_Pan"), EKeys::S },
		{ TEXT("FF.Camera.Move.Left"), TEXT("IA_Pan"), EKeys::A },
		{ TEXT("FF.Camera.Move.Right"), TEXT("IA_Pan"), EKeys::D },
		{ TEXT("FF.Camera.Rotate.Left"), TEXT("IA_SnapRotate"), EKeys::Q },
		{ TEXT("FF.Camera.Rotate.Right"), TEXT("IA_SnapRotate"), EKeys::E },
		{ TEXT("FF.Camera.ResetView"), TEXT("IA_ResetView"), EKeys::Home },
		{ TEXT("FF.Camera.ToggleView"), TEXT("IA_ToggleView"), EKeys::Tab },
		{ TEXT("FF.Character.Move.Forward"), TEXT("IA_Move"), EKeys::W, true },
		{ TEXT("FF.Character.Move.Backward"), TEXT("IA_Move"), EKeys::S, true },
		{ TEXT("FF.Character.Move.Left"), TEXT("IA_Move"), EKeys::A, true },
		{ TEXT("FF.Character.Move.Right"), TEXT("IA_Move"), EKeys::D, true },
		{ TEXT("FF.Character.Jump"), TEXT("IA_Jump"), EKeys::SpaceBar, true },
		{ TEXT("FF.Character.Sprint"), TEXT("IA_Sprint"), EKeys::LeftShift, true },
	};

	for (const FExpectedDefault& Expected : Defaults)
	{
		TestTrue(*FString::Printf(TEXT("%s는 기본키 %s를 표시한다"),
			*Expected.MappingName.ToString(), *Expected.Key.ToString()),
			UFF_KeyBindingPanel::GetDefaultKeyForMapping(Expected.MappingName) == Expected.Key);

		const UInputMappingContext* Context = Expected.bCharacter
			? CharacterContext : CameraContext;
		const bool bExistsInProjectInput = Context && Context->GetMappings().ContainsByPredicate(
			[&Expected](const FEnhancedActionKeyMapping& Mapping)
			{
				return Mapping.Action
					&& Mapping.Action->GetFName() == Expected.ActionName
					&& Mapping.Key == Expected.Key;
			});
		TestTrue(*FString::Printf(TEXT("%s 기본키는 팀 IMC의 실제 Action+Key와 일치한다"),
			*Expected.MappingName.ToString()), bExistsInProjectInput);
	}

	TestFalse(TEXT("기본 정의 자체가 없는 동작만 미지정 후보가 된다"),
		UFF_KeyBindingPanel::GetDefaultKeyForMapping(TEXT("FF.Unknown.Unassigned")).IsValid());

	const UFF_KeyBindingPanel* Panel = GetDefault<UFF_KeyBindingPanel>();
	TestNotNull(TEXT("기본키 요약을 위한 키 배치 패널 CDO를 로드한다"), Panel);
	if (Panel)
	{
		const FString WSummary = Panel->GetCurrentBindingSummary(EKeys::W).ToString();
		TestTrue(TEXT("W 기본키는 카메라 앞으로 이동을 표시한다"),
			WSummary.Contains(TEXT("카메라 앞으로 이동")));
		TestTrue(TEXT("W 기본키는 캐릭터 앞으로 이동을 표시한다"),
			WSummary.Contains(TEXT("캐릭터 앞으로 이동")));
		TestTrue(TEXT("Home 기본키는 카메라 초기화를 표시한다"),
			Panel->GetCurrentBindingSummary(EKeys::Home).ToString().Contains(TEXT("카메라 시점 초기화")));
		TestTrue(TEXT("사용되지 않는 F2는 미지정 액션을 꾸며내지 않는다"),
			Panel->GetCurrentBindingSummary(EKeys::F2).ToString().Contains(TEXT("연결된 동작이 없습니다")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_KeyBindingFixedBuildModeContractTest,
	"FestaFiesta.UI.Settings.KeyBinding.FixedBuildModeContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_KeyBindingFixedBuildModeContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestTrue(
		TEXT("the fixed build-mode key is B"),
		UFF_KeyBindingPanel::GetFixedBuildModeKey() == EKeys::B);
	TestTrue(
		TEXT("B is recognized as a fixed action key"),
		UFF_KeyBindingPanel::IsFixedActionKey(EKeys::B));
	TestFalse(
		TEXT("ordinary player-mappable keys are not fixed"),
		UFF_KeyBindingPanel::IsFixedActionKey(EKeys::W));

	const UFF_KeyBindingPanel* Panel = GetDefault<UFF_KeyBindingPanel>();
	TestNotNull(TEXT("native key-binding panel CDO loads"), Panel);
	if (Panel)
	{
		const FString Summary = Panel->GetCurrentBindingSummary(EKeys::B).ToString();
		TestTrue(TEXT("B inspection identifies build mode"), Summary.Contains(TEXT("건축 모드")));
		TestTrue(TEXT("B inspection identifies the binding as fixed"), Summary.Contains(TEXT("고정")));
		TestTrue(TEXT("B inspection explains it cannot be changed"), Summary.Contains(TEXT("변경 불가")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_KeyBindingEscapeLegibilityContractTest,
	"FestaFiesta.UI.Settings.KeyBinding.EscapeLegibilityContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_KeyBindingEscapeLegibilityContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFKeyBindingTests;

	UFF_KeyBindingKeyButton* EscapeButton =
		NewObject<UFF_KeyBindingKeyButton>(GetTransientPackage());
	TestNotNull(TEXT("Esc 키캡을 생성한다"), EscapeButton);
	if (!EscapeButton)
	{
		return false;
	}

	EscapeButton->Configure(EKeys::Escape, FText::FromString(TEXT("Esc")), true, false);
	const UScaleBox* EscapeLabelScale = Cast<UScaleBox>(EscapeButton->GetContent());
	TestNotNull(TEXT("Esc 라벨은 자동 맞춤 ScaleBox 안에 있다"), EscapeLabelScale);
	const UTextBlock* EscapeLabel = EscapeLabelScale
		? Cast<UTextBlock>(EscapeLabelScale->GetContent())
		: nullptr;
	TestNotNull(TEXT("Esc 키캡에 텍스트가 있다"), EscapeLabel);
	if (!EscapeLabel)
	{
		return false;
	}

	TestEqual(TEXT("Esc 라벨을 축약하지 않는다"), EscapeLabel->GetText().ToString(), FString(TEXT("Esc")));
	TestTrue(TEXT("Esc 라벨은 작은 DPI에서도 읽히도록 14px 이상이다"), EscapeLabel->GetFont().Size >= 14);
	TestTrue(TEXT("Esc 키캡은 일반 키보다 최소 25% 넓다"),
		FFF_KeyBindingLayoutMetrics::EscapeWidthUnits >= 1.25f);
	TestTrue(TEXT("Esc 라벨은 말줄임표 없이 원문을 유지한다"),
		EscapeLabel->GetTextOverflowPolicy() == ETextOverflowPolicy::Clip);
	TestTrue(TEXT("Esc 라벨은 넘칠 때만 축소한다"),
		EscapeLabelScale->GetStretch() == EStretch::ScaleToFit
		&& EscapeLabelScale->GetStretchDirection() == EStretchDirection::DownOnly);
	TestTrue(TEXT("Esc 키캡은 좌우 텍스트 여백을 4px 이하로 확보한다"),
		EscapeButton->GetStyle().NormalPadding.Left <= 4.0f
		&& EscapeButton->GetStyle().NormalPadding.Right <= 4.0f);

	const FLinearColor Fill = EscapeButton->GetStyle().Normal.TintColor.GetSpecifiedColor();
	const FLinearColor TextColor = EscapeLabel->GetColorAndOpacity().GetSpecifiedColor();
	TestTrue(TEXT("Esc 키캡과 글자는 WCAG AA 수준의 명도 대비를 갖는다"),
		ContrastRatio(Fill, TextColor) >= 4.5f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_KeyBindingKeyLabelLegibilityContractTest,
	"FestaFiesta.UI.Settings.KeyBinding.KeyLabelLegibilityContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_KeyBindingKeyLabelLegibilityContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UFF_KeyBindingKeyButton* LongLabelButton =
		NewObject<UFF_KeyBindingKeyButton>(GetTransientPackage());
	TestNotNull(TEXT("긴 라벨 키캡을 생성한다"), LongLabelButton);
	if (!LongLabelButton)
	{
		return false;
	}

	LongLabelButton->Configure(EKeys::Pause, FText::FromString(TEXT("Pause")), false, false);
	const UScaleBox* LabelScale = Cast<UScaleBox>(LongLabelButton->GetContent());
	const UTextBlock* Label = LabelScale ? Cast<UTextBlock>(LabelScale->GetContent()) : nullptr;
	TestNotNull(TEXT("모든 키 라벨에 자동 맞춤 ScaleBox가 있다"), LabelScale);
	TestNotNull(TEXT("자동 맞춤 ScaleBox가 전체 원문을 보존한다"), Label);
	if (LabelScale && Label)
	{
		TestEqual(TEXT("긴 키 이름을 축약하지 않는다"), Label->GetText().ToString(), FString(TEXT("Pause")));
		TestTrue(TEXT("라벨은 키캡보다 클 때만 아래 방향으로 축소된다"),
			LabelScale->GetStretch() == EStretch::ScaleToFit
			&& LabelScale->GetStretchDirection() == EStretchDirection::DownOnly);
	}
	TestTrue(TEXT("일반 키캡은 좌우 라벨 여백을 3px로 제한한다"),
		LongLabelButton->GetStyle().NormalPadding.Left <= 3.0f
		&& LongLabelButton->GetStyle().NormalPadding.Right <= 3.0f);

	const float MainClusterRight = FFF_KeyBindingLayoutMetrics::GetKeyWidth(15.0f);
	const float NavigationClusterRight = FFF_KeyBindingLayoutMetrics::NavigationClusterX
		+ FFF_KeyBindingLayoutMetrics::GetKeyWidth(3.0f);
	TestTrue(TEXT("메인 키 영역과 내비게이션 영역 사이에 자연스러운 간격이 있다"),
		FFF_KeyBindingLayoutMetrics::NavigationClusterX - MainClusterRight >= 20.0f);
	TestTrue(TEXT("내비게이션 영역과 숫자패드 사이에 자연스러운 간격이 있다"),
		FFF_KeyBindingLayoutMetrics::NumberPadClusterX - NavigationClusterRight >= 20.0f);
	TestTrue(TEXT("숫자패드 네 열이 공통 키보드 표면 안에 정확히 들어온다"),
		FMath::IsNearlyEqual(
			FFF_KeyBindingLayoutMetrics::NumberPadClusterX
				+ FFF_KeyBindingLayoutMetrics::GetKeyWidth(4.0f),
			FFF_KeyBindingLayoutMetrics::KeyboardSurfaceWidth));
	TestTrue(TEXT("여섯 키 행이 공통 키보드 표면 높이에 정확히 들어온다"),
		FMath::IsNearlyEqual(
			FFF_KeyBindingLayoutMetrics::GetRowY(5) + FFF_KeyBindingLayoutMetrics::KeyHeight,
			FFF_KeyBindingLayoutMetrics::KeyboardSurfaceHeight));
	TestTrue(TEXT("숫자패드 +와 Enter는 두 행 높이로 표현된다"),
		FFF_KeyBindingLayoutMetrics::GetKeyHeight(2.0f)
			> FFF_KeyBindingLayoutMetrics::KeyHeight * 2.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_KeyBindingWidgetContractTest,
	"FestaFiesta.UI.Settings.KeyBinding.WidgetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_KeyBindingWidgetContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFKeyBindingTests;

	UClass* NativePanelClass = LoadObject<UClass>(nullptr, PanelNativeClassPath);
	TestNotNull(TEXT("native UFF_KeyBindingPanel class loads"), NativePanelClass);

	UClass* PanelClass = LoadClass<UUserWidget>(nullptr, PanelGeneratedClassPath);
	TestNotNull(TEXT("WBP_FF_KeyBindingPanel generated class loads"), PanelClass);
	TestTrue(
		TEXT("WBP_FF_KeyBindingPanel derives from UFF_KeyBindingPanel"),
		PanelClass && NativePanelClass && PanelClass->IsChildOf(NativePanelClass));

	const UWidgetTree* WidgetTree = LoadPanelWidgetTree(*this);
	if (!WidgetTree)
	{
		return false;
	}

	struct FRequiredWidget
	{
		const TCHAR* Name;
		UClass* Type;
	};
	const FRequiredWidget RequiredWidgets[] = {
		{ TEXT("Image_KeyboardArtwork"), UImage::StaticClass() },
		{ TEXT("VerticalBox_KeyboardRows"), UVerticalBox::StaticClass() },
		{ TEXT("VerticalBox_BindingRows"), UVerticalBox::StaticClass() },
		{ TEXT("Text_CaptureStatus"), UTextBlock::StaticClass() },
		{ TEXT("Button_ResetAll"), UButton::StaticClass() },
		{ TEXT("Text_ResetAll"), UTextBlock::StaticClass() },
	};

	for (const FRequiredWidget& RequiredWidget : RequiredWidgets)
	{
		UWidget* Widget = WidgetTree->FindWidget(FName(RequiredWidget.Name));
		TestNotNull(
			*FString::Printf(TEXT("required widget exists: %s"), RequiredWidget.Name),
			Widget);
		TestTrue(
			*FString::Printf(TEXT("required widget has the correct type: %s"), RequiredWidget.Name),
			Widget && Widget->IsA(RequiredWidget.Type));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_KeyBindingArtworkTextureContractTest,
	"FestaFiesta.UI.Settings.KeyBinding.ArtworkTextureContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_KeyBindingArtworkTextureContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFKeyBindingTests;

	const FString TextureObjectPath = MakeAssetObjectPath(ArtworkTexturePackagePath);
	UTexture2D* ArtworkTexture = LoadObject<UTexture2D>(nullptr, *TextureObjectPath);
	TestNotNull(
		*FString::Printf(TEXT("keyboard artwork texture loads from %s"), *TextureObjectPath),
		ArtworkTexture);
	if (!ArtworkTexture)
	{
		return false;
	}

	TestEqual(TEXT("keyboard artwork uses the UI texture group"),
		static_cast<TextureGroup>(ArtworkTexture->LODGroup), TEXTUREGROUP_UI);
	TestEqual(TEXT("keyboard artwork uses uncompressed RGBA to preserve alpha"),
		static_cast<TextureCompressionSettings>(ArtworkTexture->CompressionSettings),
		TC_EditorIcon);
	TestEqual(TEXT("keyboard artwork does not generate mipmaps"),
		static_cast<TextureMipGenSettings>(ArtworkTexture->MipGenSettings), TMGS_NoMipmaps);
	TestTrue(TEXT("keyboard artwork keeps sRGB enabled"), ArtworkTexture->SRGB);
	TestFalse(TEXT("keyboard artwork import does not discard alpha"),
		ArtworkTexture->CompressionNoAlpha != 0);
	TestTrue(TEXT("keyboard artwork has valid imported source pixels"),
		ArtworkTexture->Source.IsValid()
		&& ArtworkTexture->Source.GetSizeX() > 0
		&& ArtworkTexture->Source.GetSizeY() > 0);
	TestTrue(TEXT("keyboard artwork source format preserves an alpha channel"),
		SourceFormatSupportsAlpha(ArtworkTexture->Source.GetFormat()));

	TestTrue(TEXT("keyboard artwork source contains transparent pixels"),
		SourceContainsTransparency(ArtworkTexture->Source));

	const UWidgetTree* WidgetTree = LoadPanelWidgetTree(*this);
	const UImage* ArtworkImage = WidgetTree
		? Cast<UImage>(WidgetTree->FindWidget(TEXT("Image_KeyboardArtwork")))
		: nullptr;
	TestNotNull(TEXT("Image_KeyboardArtwork exists for texture binding"), ArtworkImage);
	if (ArtworkImage)
	{
		TestEqual(
			TEXT("Image_KeyboardArtwork brush uses the generated keyboard texture"),
			ArtworkImage->GetBrush().GetResourceObject(),
			static_cast<UObject*>(ArtworkTexture));
	}

	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
