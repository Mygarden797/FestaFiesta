// FF_KeyBindingTests.cpp
// 역할: 키 변경·충돌·저장·실제 입력 연결을 검증한다.

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
#include "UI/Settings/FF_KeyBindingDefinitions.h"
#include "UI/Settings/FF_KeyBindingLayoutMetrics.h"
#include "UI/Settings/FF_KeyBindingPanel.h"
#include "UI/Settings/FF_KeyBindingRuntimeSubsystem.h"
#include "UserSettings/EnhancedInputUserSettings.h"

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
		{ TEXT("FF.Character.ShiftLock"), TEXT("IA_ShiftLock"), EKeys::LeftShift, true },
	};

	for (const FExpectedDefault& Expected : Defaults)
	{
		const FFF_KeyBindingDefinition* Definition = FFKeyBindingDefinitions::Find(Expected.MappingName);
		TestNotNull(*FString::Printf(TEXT("%s의 공통 정의가 있다"), *Expected.MappingName.ToString()), Definition);
		if (Definition)
		{
			TestEqual(TEXT("런타임이 찾는 Action도 실제 IMC의 Action과 같다"),
				Definition->SourceActionName, Expected.ActionName);
			TestTrue(TEXT("런타임 입력 컨텍스트가 기본키 검사와 같다"),
				Definition->Context == (Expected.bCharacter
					? EFF_KeyBindingContext::Character : EFF_KeyBindingContext::Camera));
		}
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
	TestFalse(TEXT("잘못 노출되던 Sprint는 변경 가능한 기본 동작으로 남지 않는다"),
		UFF_KeyBindingPanel::GetDefaultKeyForMapping(TEXT("FF.Character.Sprint")).IsValid());

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
		const FString ShiftSummary = Panel->GetCurrentBindingSummary(EKeys::LeftShift).ToString();
		TestTrue(TEXT("LeftShift는 Shift Lock 켜기/끄기로 안내한다"),
			ShiftSummary.Contains(TEXT("시점 고정 켜기 / 끄기 (Shift Lock)")));
		TestFalse(TEXT("LeftShift를 달리기로 잘못 안내하지 않는다"),
			ShiftSummary.Contains(TEXT("달리기")) || ShiftSummary.Contains(TEXT("전력 질주")));
		TestTrue(TEXT("사용되지 않는 F2는 미지정 액션을 꾸며내지 않는다"),
			Panel->GetCurrentBindingSummary(EKeys::F2).ToString().Contains(TEXT("연결된 동작이 없습니다")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_KeyBindingRetiredMappingProfileCompatibilityTest,
	"FestaFiesta.UI.Settings.KeyBinding.RetiredMappingProfileCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_KeyBindingRetiredMappingProfileCompatibilityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// 디스크 저장 없이 실제 Enhanced Input 프로필에 이전 버전의 저장 행을 재현한다.
	UEnhancedInputUserSettings* Settings = NewObject<UEnhancedInputUserSettings>(GetTransientPackage());
	FPlayerMappableKeyProfileCreationArgs ProfileArgs;
	ProfileArgs.ProfileType = UEnhancedPlayerMappableKeyProfile::StaticClass();
	ProfileArgs.ProfileStringIdentifier = TEXT("FF.KeyBinding.CompatibilityTest");
	ProfileArgs.bSetAsCurrentProfile = true;
	if (!TestNotNull(TEXT("격리된 입력 프로필을 만든다"), Settings->CreateNewKeyProfile(ProfileArgs)))
	{
		return false;
	}

	const FName RetiredName(TEXT("FF.Character.Sprint"));
	const FName ShiftLockName(TEXT("FF.Character.ShiftLock"));
	const FName JumpName(TEXT("FF.Character.Jump"));
	const FFF_KeyBindingDefinition* ShiftLock = FFKeyBindingDefinitions::Find(ShiftLockName);
	if (!TestNotNull(TEXT("새 ShiftLock 정의가 있다"), ShiftLock))
	{
		return false;
	}
	const auto AddProfileRow = [](UInputMappingContext* Context, const FName Name, const FKey Key)
	{
		UFF_KeyBindingProfileAction* Action = NewObject<UFF_KeyBindingProfileAction>(Context);
		Action->Configure(Name, FText::FromName(Name), FText::GetEmpty(), false);
		Context->MapKey(Action, Key);
	};
	const auto SetKey = [this, Settings](const FName Name, const FKey Key)
	{
		FMapPlayerKeyArgs Args;
		Args.MappingName = Name;
		Args.Slot = EPlayerMappableKeySlot::First;
		Args.NewKey = Key;
		FGameplayTagContainer FailureReasons;
		Settings->MapPlayerKey(Args, FailureReasons);
		return TestTrue(TEXT("테스트 프로필의 키 변경이 성공한다"), FailureReasons.IsEmpty());
	};

	UInputMappingContext* LegacyContext = NewObject<UInputMappingContext>(Settings);
	AddProfileRow(LegacyContext, RetiredName, EKeys::LeftShift);
	AddProfileRow(LegacyContext, JumpName, EKeys::SpaceBar);
	if (!TestTrue(TEXT("이전 저장 행을 등록한다"), Settings->RegisterInputMappingContext(LegacyContext))
		|| !SetKey(RetiredName, EKeys::LeftAlt) || !SetKey(JumpName, EKeys::J))
	{
		return false;
	}
	TestTrue(TEXT("새 행이 없는 이전 프로필에서도 ShiftLock은 LeftShift로 시작한다"),
		FFKeyBindingDefinitions::ResolveCurrentKey(Settings, ShiftLockName) == EKeys::LeftShift);
	TestFalse(TEXT("이전 Sprint 저장 키는 UI와 런타임에 노출하지 않는다"),
		FFKeyBindingDefinitions::ResolveCurrentKey(Settings, RetiredName).IsValid());
	TestTrue(TEXT("관련 없는 점프 사용자 키는 보존한다"),
		FFKeyBindingDefinitions::ResolveCurrentKey(Settings, JumpName) == EKeys::J);

	UInputMappingContext* CurrentContext = NewObject<UInputMappingContext>(Settings);
	AddProfileRow(CurrentContext, ShiftLockName, ShiftLock->DefaultKey);
	if (!TestTrue(TEXT("현재 ShiftLock 행을 추가한다"), Settings->RegisterInputMappingContext(CurrentContext))
		|| !SetKey(ShiftLockName, EKeys::V))
	{
		return false;
	}
	TestTrue(TEXT("명시적으로 바꾼 ShiftLock 키를 사용한다"),
		FFKeyBindingDefinitions::ResolveCurrentKey(Settings, ShiftLockName) == EKeys::V);

	FMapPlayerKeyArgs ResetArgs;
	ResetArgs.MappingName = ShiftLockName;
	FGameplayTagContainer ResetFailures;
	Settings->ResetAllPlayerKeysInRow(ResetArgs, ResetFailures);
	TestTrue(TEXT("ShiftLock 기본값 복원이 성공한다"), ResetFailures.IsEmpty());
	TestTrue(TEXT("복원 후에는 이전 Sprint 키가 아닌 LeftShift를 쓴다"),
		FFKeyBindingDefinitions::ResolveCurrentKey(Settings, ShiftLockName) == EKeys::LeftShift);
	const FPlayerKeyMapping* RetiredMapping = Settings->FindCurrentMappingForSlot(
		RetiredName, EPlayerMappableKeySlot::First);
	TestTrue(TEXT("이전 저장 행을 삭제하거나 덮어쓰지 않는다"),
		RetiredMapping && RetiredMapping->GetCurrentKey() == EKeys::LeftAlt);
	TestTrue(TEXT("새 동작의 초기화가 다른 사용자 키를 초기화하지 않는다"),
		FFKeyBindingDefinitions::ResolveCurrentKey(Settings, JumpName) == EKeys::J);
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
	TestTrue(
		TEXT("the fixed nearby placement key is N"),
		UFF_KeyBindingPanel::GetFixedNearbyPlacementKey() == EKeys::N);
	TestTrue(
		TEXT("N is recognized as a fixed action key"),
		UFF_KeyBindingPanel::IsFixedActionKey(EKeys::N));
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
		const FString NearbySummary = Panel->GetCurrentBindingSummary(EKeys::N).ToString();
		TestTrue(TEXT("N inspection identifies nearby movement"), NearbySummary.Contains(TEXT("가까운 빈자리로 이동")));
		TestTrue(TEXT("N inspection explains the placement-only context"), NearbySummary.Contains(TEXT("시설 배치 중")));
		TestTrue(TEXT("N inspection explains it cannot be changed"), NearbySummary.Contains(TEXT("변경 불가")));
	}

	// Exercise the actual settings panel against an isolated Enhanced Input profile.
	// Existing saved mappings remain visible, while new conflicting assignments are rejected.
	UFF_KeyBindingPanel* RuntimePanel = NewObject<UFF_KeyBindingPanel>(GetTransientPackage());
	RuntimePanel->WidgetTree = NewObject<UWidgetTree>(RuntimePanel);
	if (!TestTrue(TEXT("native settings fallback can build its runtime tree"), RuntimePanel->BuildFallbackWidgetTree()))
	{
		return false;
	}
	UEnhancedInputUserSettings* Settings = NewObject<UEnhancedInputUserSettings>(RuntimePanel);
	FPlayerMappableKeyProfileCreationArgs ProfileArgs;
	ProfileArgs.ProfileType = UEnhancedPlayerMappableKeyProfile::StaticClass();
	ProfileArgs.ProfileStringIdentifier = TEXT("FF.KeyBinding.FixedBuildContractTest");
	ProfileArgs.bSetAsCurrentProfile = true;
	if (!TestNotNull(TEXT("isolated profile for fixed-key collision checks"), Settings->CreateNewKeyProfile(ProfileArgs)))
	{
		return false;
	}
	const FName JumpName(TEXT("FF.Character.Jump"));
	UInputMappingContext* Context = NewObject<UInputMappingContext>(Settings);
	// RefreshBindings reads every row when drawing the keyboard and checking option keys.
	// Match a complete runtime profile so those reads do not exercise missing-row diagnostics.
	for (const FFF_KeyBindingDefinition& Definition : FFKeyBindingDefinitions::GetAll())
	{
		UFF_KeyBindingProfileAction* Action = NewObject<UFF_KeyBindingProfileAction>(Context);
		Action->Configure(Definition.MappingName, Definition.ActionLabel,
			FFKeyBindingDefinitions::GetContextLabel(Definition.Context),
			Definition.Context == EFF_KeyBindingContext::System);
		Context->MapKey(Action, Definition.DefaultKey);
	}
	if (!TestTrue(TEXT("real profile registers all player-mappable actions"), Settings->RegisterInputMappingContext(Context)))
	{
		return false;
	}
	RuntimePanel->UserSettings = Settings;
	RuntimePanel->RefreshBindings();
	for (const TPair<const TCHAR*, FKey> Fixed : {
		TPair<const TCHAR*, FKey>(TEXT("KeyBindingFixedAction_BuildMode"), EKeys::B),
		TPair<const TCHAR*, FKey>(TEXT("KeyBindingFixedAction_NearbyPlacement"), EKeys::N)})
	{
		const auto* Row = Cast<UFF_KeyBindingActionButton>(RuntimePanel->WidgetTree->FindWidget(Fixed.Key));
		TestTrue(*FString::Printf(TEXT("%s is an inspectable read-only fixed row"), Fixed.Key),
			Row && Row->IsReadOnlyBinding() && Row->GetCurrentKey() == Fixed.Value);
	}
	TArray<UWidget*> Widgets;
	RuntimePanel->WidgetTree->GetAllWidgets(Widgets);
	const auto* NearbyKey = Widgets.FindByPredicate([](const UWidget* Widget)
	{
		const auto* KeyButton = Cast<UFF_KeyBindingKeyButton>(Widget);
		return KeyButton && KeyButton->GetConfiguredKey() == EKeys::N;
	});
	TestTrue(TEXT("the keyboard diagram marks N as a fixed action"),
		NearbyKey && Cast<UFF_KeyBindingKeyButton>(*NearbyKey)->IsFixedActionKey());
	for (const FKey FixedKey : {EKeys::B, EKeys::N})
	{
		RuntimePanel->SelectedMappingName = JumpName;
		RuntimePanel->TryAssignKey(FixedKey);
		TestTrue(TEXT("assigning a reserved fixed key never changes the actual profile"),
			FFKeyBindingDefinitions::ResolveCurrentKey(Settings, JumpName) == EKeys::SpaceBar);
		TestTrue(TEXT("a rejected fixed key keeps capture open for a different key"), RuntimePanel->IsCapturingKey());
		TestTrue(TEXT("the panel explains the fixed-key reassignment restriction"),
			RuntimePanel->Text_CaptureStatus->GetText().ToString().Contains(TEXT("다른 동작에 지정할 수 없습니다")));
	}
	FMapPlayerKeyArgs LegacyArgs;
	LegacyArgs.MappingName = JumpName;
	LegacyArgs.Slot = EPlayerMappableKeySlot::First;
	LegacyArgs.NewKey = EKeys::N;
	FGameplayTagContainer FailureReasons;
	Settings->MapPlayerKey(LegacyArgs, FailureReasons);
	if (!TestTrue(TEXT("the fixture recreates a saved custom N mapping"), FailureReasons.IsEmpty()))
	{
		return false;
	}
	RuntimePanel->RefreshBindings();
	const FString SavedSummary = RuntimePanel->GetCurrentBindingSummary(EKeys::N).ToString();
	TestTrue(TEXT("refresh preserves an existing custom N mapping"),
		FFKeyBindingDefinitions::ResolveCurrentKey(Settings, JumpName) == EKeys::N);
	TestTrue(TEXT("inspection includes the saved action alongside the fixed N action"),
		SavedSummary.Contains(TEXT("점프")) && SavedSummary.Contains(TEXT("가까운 빈자리로 이동")));
	TestTrue(TEXT("inspection warns about the saved N conflict"), SavedSummary.Contains(TEXT("주의: N")));
	RuntimePanel->HandleMappingChosen(TEXT("FF.Fixed.NearbyPlacement"));
	TestFalse(TEXT("clicking the fixed N action inspects it instead of starting capture"), RuntimePanel->IsCapturingKey());
	TestTrue(TEXT("the fixed N row shows the nearby placement action in the inspector"),
		RuntimePanel->Text_CaptureStatus->GetText().ToString().Contains(TEXT("가까운 빈자리로 이동")));

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
