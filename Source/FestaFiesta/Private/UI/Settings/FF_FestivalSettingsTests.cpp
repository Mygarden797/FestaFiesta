// FF_FestivalSettingsTests.cpp
// 역할: 설정 화면의 수명과 게임 입력·건설 상태의 복구를 검증한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/Settings/FF_FestivalSettingsPolicy.h"

#if WITH_EDITOR
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/SafeZone.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "InputMappingContext.h"
#include "Misc/ConfigCacheIni.h"
#include "UI/Settings/FF_FestivalSettingsScreen.h"
#include "UI/Settings/FF_FestivalSettingsSubsystem.h"
#endif

namespace FFFestivalSettingsPolicyTest
{
	constexpr int32 MinimumWidth = 1024;
	constexpr int32 MinimumHeight = 576;

	void TestResolutionList(
		FAutomationTestBase& Test,
		const TCHAR* What,
		const TArray<FIntPoint>& Actual,
		const TArray<FIntPoint>& Expected)
	{
		Test.TestEqual(*FString::Printf(TEXT("%s 후보 수"), What), Actual.Num(), Expected.Num());
		const int32 ComparableCount = FMath::Min(Actual.Num(), Expected.Num());
		for (int32 Index = 0; Index < ComparableCount; ++Index)
		{
			Test.TestEqual(
				*FString::Printf(TEXT("%s [%d]"), What, Index),
				Actual[Index], Expected[Index]);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FestivalSettingsPolicy_WindowedFullscreenUsesDesktopOnly,
	"FestaFiesta.UI.Settings.Resolution.WindowedFullscreenUsesDesktopOnly",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_FestivalSettingsPolicy_WindowedFullscreenUsesDesktopOnly::RunTest(const FString& Parameters)
{
	using namespace FFFestivalSettingsPolicyTest;

	const FIntPoint Desktop(2560, 1440);
	const TArray<FIntPoint> Actual = FF::SettingsPolicy::BuildResolutionOptions(
		EWindowMode::WindowedFullscreen,
		{ FIntPoint(1280, 720), FIntPoint(1920, 1080) },
		{ FIntPoint(1600, 900) },
		{ FIntPoint(1280, 720), FIntPoint(3840, 2160) },
		Desktop,
		FIntPoint(1366, 768),
		MinimumWidth,
		MinimumHeight);

	TestResolutionList(*this, TEXT("테두리 없는 전체화면"), Actual, { Desktop });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FestivalSettingsPolicy_FullscreenUsesDetectedModesExclusively,
	"FestaFiesta.UI.Settings.Resolution.FullscreenUsesDetectedModesExclusively",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_FestivalSettingsPolicy_FullscreenUsesDetectedModesExclusively::RunTest(const FString& Parameters)
{
	using namespace FFFestivalSettingsPolicyTest;

	const TArray<FIntPoint> Actual = FF::SettingsPolicy::BuildResolutionOptions(
		EWindowMode::Fullscreen,
		{
			FIntPoint(1920, 1080), FIntPoint(1280, 720), FIntPoint(1920, 1080),
			FIntPoint(800, 600), FIntPoint(-1, 1080)
		},
		{},
		{ FIntPoint(1600, 900), FIntPoint(2560, 1440) },
		FIntPoint(2560, 1440),
		FIntPoint(1366, 768),
		MinimumWidth,
		MinimumHeight);

	TestResolutionList(*this, TEXT("검출 성공 전체화면"), Actual,
		{ FIntPoint(1280, 720), FIntPoint(1920, 1080) });
	TestFalse(TEXT("fallback은 검출 성공 목록에 섞이지 않는다"), Actual.Contains(FIntPoint(1600, 900)));
	TestFalse(TEXT("비표준 현재값은 검출 성공 목록에 섞이지 않는다"), Actual.Contains(FIntPoint(1366, 768)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FestivalSettingsPolicy_FullscreenFallsBackSafely,
	"FestaFiesta.UI.Settings.Resolution.FullscreenFallsBackSafely",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_FestivalSettingsPolicy_FullscreenFallsBackSafely::RunTest(const FString& Parameters)
{
	using namespace FFFestivalSettingsPolicyTest;

	const TArray<FIntPoint> Actual = FF::SettingsPolicy::BuildResolutionOptions(
		EWindowMode::Fullscreen,
		{},
		{},
		{
			FIntPoint(3840, 2160), FIntPoint(1600, 900), FIntPoint(1280, 720),
			FIntPoint(1280, 720), FIntPoint(800, 600)
		},
		FIntPoint(1920, 1080),
		FIntPoint(1366, 768),
		MinimumWidth,
		MinimumHeight);

	TestResolutionList(*this, TEXT("검출 실패 전체화면"), Actual,
		{ FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080) });
	TestFalse(TEXT("데스크톱보다 큰 fallback은 제외한다"), Actual.Contains(FIntPoint(3840, 2160)));
	TestFalse(TEXT("검출 실패에서도 임의 현재값은 안전 후보가 아니다"), Actual.Contains(FIntPoint(1366, 768)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FestivalSettingsPolicy_WindowedPrefersConvenientModes,
	"FestaFiesta.UI.Settings.Resolution.WindowedPrefersConvenientModes",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_FestivalSettingsPolicy_WindowedPrefersConvenientModes::RunTest(const FString& Parameters)
{
	using namespace FFFestivalSettingsPolicyTest;

	const TArray<FIntPoint> Actual = FF::SettingsPolicy::BuildResolutionOptions(
		EWindowMode::Windowed,
		{ FIntPoint(3840, 2160) },
		{
			FIntPoint(2560, 1440), FIntPoint(1600, 900), FIntPoint(1280, 720),
			FIntPoint(1600, 900), FIntPoint(800, 600)
		},
		{ FIntPoint(1366, 768), FIntPoint(1920, 1080) },
		FIntPoint(1920, 1080),
		FIntPoint(1536, 864),
		MinimumWidth,
		MinimumHeight);

	TestResolutionList(*this, TEXT("검출 성공 창 모드"), Actual,
		{ FIntPoint(1280, 720), FIntPoint(1600, 900) });
	TestFalse(TEXT("창 모드는 데스크톱보다 큰 후보를 제외한다"), Actual.Contains(FIntPoint(2560, 1440)));
	TestFalse(TEXT("편리한 창 크기가 있으면 fallback을 섞지 않는다"), Actual.Contains(FIntPoint(1366, 768)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FestivalSettingsPolicy_WindowedFallsBackWithinDesktop,
	"FestaFiesta.UI.Settings.Resolution.WindowedFallsBackWithinDesktop",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_FestivalSettingsPolicy_WindowedFallsBackWithinDesktop::RunTest(const FString& Parameters)
{
	using namespace FFFestivalSettingsPolicyTest;

	const TArray<FIntPoint> Actual = FF::SettingsPolicy::BuildResolutionOptions(
		EWindowMode::Windowed,
		{},
		{},
		{
			FIntPoint(2560, 1440), FIntPoint(1920, 1080), FIntPoint(1280, 720),
			FIntPoint(1600, 900), FIntPoint(1280, 720), FIntPoint(0, 900)
		},
		FIntPoint(1920, 1080),
		FIntPoint(1366, 768),
		MinimumWidth,
		MinimumHeight);

	TestResolutionList(*this, TEXT("검출 실패 창 모드"), Actual,
		{ FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080) });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FestivalSettingsPolicy_SelectsClosestSafeResolution,
	"FestaFiesta.UI.Settings.Resolution.SelectsClosestSafeResolution",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_FestivalSettingsPolicy_SelectsClosestSafeResolution::RunTest(const FString& Parameters)
{
	const TArray<FIntPoint> Options = {
		FIntPoint(0, 0),
		FIntPoint(1280, 720),
		FIntPoint(1600, 900),
		FIntPoint(1920, 1080)
	};

	TestEqual(TEXT("목록에 있는 값은 정확히 선택한다"),
		FF::SettingsPolicy::FindClosestResolutionIndex(Options, FIntPoint(1600, 900)), 2);
	TestEqual(TEXT("비표준 1366x768은 가장 가까운 안전 후보를 선택한다"),
		FF::SettingsPolicy::FindClosestResolutionIndex(Options, FIntPoint(1366, 768)), 1);
	TestEqual(TEXT("유효하지 않은 후보는 선택하지 않는다"),
		FF::SettingsPolicy::FindClosestResolutionIndex(Options, FIntPoint(1, 1)), 1);

	const TArray<FIntPoint> TiedOptions = { FIntPoint(1920, 900), FIntPoint(1600, 900) };
	TestEqual(TEXT("거리 동률이면 더 작은 안전 후보를 선택한다"),
		FF::SettingsPolicy::FindClosestResolutionIndex(TiedOptions, FIntPoint(1760, 900)), 1);
	TestEqual(TEXT("후보가 없으면 INDEX_NONE"),
		FF::SettingsPolicy::FindClosestResolutionIndex({}, FIntPoint(1920, 1080)), INDEX_NONE);
	return true;
}

#if WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FestivalSettings_KeyboardGuideContract,
	"FestaFiesta.UI.Settings.KeyboardGuideContract",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_FestivalSettings_KeyboardGuideContract::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const UFF_FestivalSettingsScreen* NativeDefaults =
		UFF_FestivalSettingsScreen::StaticClass()->GetDefaultObject<UFF_FestivalSettingsScreen>();
	TestNotNull(TEXT("네이티브 키 안내 기본값을 로드한다"), NativeDefaults);
	if (!NativeDefaults)
	{
		return false;
	}

	const TArray<FFF_SettingsKeyGuideEntry>& Guide = NativeDefaults->GetKeyboardGuideEntries();
	auto HasGuide = [&Guide](const TCHAR* Context, const TCHAR* Keys, const TCHAR* ActionFragment)
	{
		return Guide.ContainsByPredicate([=](const FFF_SettingsKeyGuideEntry& Entry)
		{
			return Entry.Context.ToString() == Context && Entry.Keys.ToString() == Keys
				&& Entry.Action.ToString().Contains(ActionFragment);
		});
	};
	TestTrue(TEXT("관찰 카메라 이동은 실제 IMC대로 WASD만 안내한다"),
		HasGuide(TEXT("관찰 카메라"), TEXT("WASD"), TEXT("카메라 이동")));
	TestFalse(TEXT("관찰 카메라에 없는 방향키 매핑을 안내하지 않는다"),
		Guide.ContainsByPredicate([](const FFF_SettingsKeyGuideEntry& Entry)
		{
			return Entry.Context.ToString() == TEXT("관찰 카메라")
				&& Entry.Keys.ToString().Contains(TEXT("방향키"));
		}));
	TestTrue(TEXT("Home 카메라 초기화를 안내한다"),
		HasGuide(TEXT("관찰 카메라"), TEXT("Home"), TEXT("초기화")));
	TestTrue(TEXT("Tab 시점 전환을 안내한다"),
		HasGuide(TEXT("관찰 카메라"), TEXT("Tab"), TEXT("시점 전환")));
	TestTrue(TEXT("M 시설 이동을 안내한다"),
		HasGuide(TEXT("건축"), TEXT("M"), TEXT("시설 이동")));
	TestTrue(TEXT("캐릭터 마우스 시점 조작을 안내한다"),
		HasGuide(TEXT("캐릭터"), TEXT("마우스 이동"), TEXT("시점 조작")));
	TestTrue(TEXT("공통 옵션 키로 ESC와 F5를 함께 안내한다"),
		HasGuide(TEXT("공통"), TEXT("ESC / F5"), TEXT("일시정지 메뉴")));
	TestTrue(TEXT("건축 중에도 ESC와 F5가 상태 보존 일시정지를 안내한다"),
		HasGuide(TEXT("건축"), TEXT("ESC / F5"), TEXT("상태를 유지")));

	const UInputMappingContext* CameraContext = LoadObject<UInputMappingContext>(
		nullptr, TEXT("/Game/FestaFiesta/Input/IMC_Camera.IMC_Camera"));
	const UInputMappingContext* CharacterContext = LoadObject<UInputMappingContext>(
		nullptr, TEXT("/Game/FestaFiesta/Input/Character/IMC_Character.IMC_Character"));
	TestNotNull(TEXT("카메라 입력 매핑 자산을 로드한다"), CameraContext);
	TestNotNull(TEXT("캐릭터 입력 매핑 자산을 로드한다"), CharacterContext);
	auto HasMapping = [](const UInputMappingContext* Context, const FName ActionName, const FKey Key)
	{
		return Context && Context->GetMappings().ContainsByPredicate(
			[&](const FEnhancedActionKeyMapping& Mapping)
			{
				return Mapping.Action && Mapping.Action->GetFName() == ActionName && Mapping.Key == Key;
			});
	};
	TestTrue(TEXT("IMC_Camera의 Home 초기화 계약이 유지된다"),
		HasMapping(CameraContext, TEXT("IA_ResetView"), EKeys::Home));
	TestTrue(TEXT("IMC_Camera의 Tab 시점 전환 계약이 유지된다"),
		HasMapping(CameraContext, TEXT("IA_ToggleView"), EKeys::Tab));
	TestTrue(TEXT("IMC_Character의 Mouse2D 시점 계약이 유지된다"),
		HasMapping(CharacterContext, TEXT("IA_Look"), EKeys::Mouse2D));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FestivalSettings_WidgetContract,
	"FestaFiesta.UI.Settings.WidgetContract",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_FestivalSettings_WidgetContract::RunTest(const FString& Parameters)
{
	(void)Parameters;
	constexpr const TCHAR* ScreenClassPath =
		TEXT("/Game/FestaFiesta/UI/Settings/WBP_FF_FullscreenOptions.WBP_FF_FullscreenOptions_C");

	const UFF_FestivalSettingsDeveloperSettings* Settings =
		UFF_FestivalSettingsDeveloperSettings::Get();
	TestNotNull(TEXT("옵션 개발 설정을 로드한다"), Settings);
	if (Settings)
	{
		TestEqual(TEXT("새 수동 저장 옵션 WBP를 기본 화면으로 사용한다"),
			Settings->SettingsScreenClass.ToSoftObjectPath().ToString(), FString(ScreenClassPath));
		TestEqual(TEXT("옵션 진입 키는 ESC와 F5 두 개다"), Settings->OpenCloseKeys.Num(), 2);
		int32 EscapeCount = 0;
		int32 F5Count = 0;
		for (const FKey& Key : Settings->OpenCloseKeys)
		{
			EscapeCount += Key == EKeys::Escape ? 1 : 0;
			F5Count += Key == EKeys::F5 ? 1 : 0;
		}
		TestEqual(TEXT("ESC 옵션 바인딩은 정확히 하나다"), EscapeCount, 1);
		TestEqual(TEXT("F5 옵션 바인딩은 정확히 하나다"), F5Count, 1);
		TestTrue(TEXT("1920x1080 디자인의 최소 가독성 해상도는 1280x720 이상이다"),
			Settings->MinimumResolutionWidth >= 1280 && Settings->MinimumResolutionHeight >= 720);
	}

	TArray<FString> DebugExecBindings;
	TestNotNull(TEXT("병합 입력 설정 캐시를 사용할 수 있다"), GConfig);
	if (GConfig)
	{
		GConfig->GetArray(TEXT("/Script/Engine.PlayerInput"), TEXT("DebugExecBindings"),
			DebugExecBindings, GInputIni);
	}
	TestTrue(TEXT("팀 에디터 입력 설정은 보존하고 F5 충돌은 런타임 옵션 서브시스템이 격리한다"),
		DebugExecBindings.ContainsByPredicate([](const FString& Binding)
		{
			return Binding.Contains(TEXT("Key=F5"), ESearchCase::IgnoreCase)
				&& Binding.Contains(TEXT("viewmode shadercomplexity"), ESearchCase::IgnoreCase);
		}));
	TestTrue(TEXT("다른 엔진 디버그 단축키는 보존한다"),
		DebugExecBindings.ContainsByPredicate([](const FString& Binding)
		{
			return Binding.Contains(TEXT("Key=F1"), ESearchCase::IgnoreCase)
				&& Binding.Contains(TEXT("viewmode wireframe"), ESearchCase::IgnoreCase);
		}));

	UClass* ScreenClass = LoadClass<UFF_FestivalSettingsScreen>(nullptr, ScreenClassPath);
	TestNotNull(TEXT("새 ESC 옵션 WBP 클래스를 로드한다"), ScreenClass);
	TestTrue(TEXT("새 WBP는 FestivalSettingsScreen을 상속한다"),
		ScreenClass && ScreenClass->IsChildOf(UFF_FestivalSettingsScreen::StaticClass()));

	const UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(ScreenClass);
	UWidgetTree* Tree = GeneratedClass ? GeneratedClass->GetWidgetTreeArchetype() : nullptr;
	TestNotNull(TEXT("새 옵션 WBP의 WidgetTree가 저장됐다"), Tree);
	if (!Tree)
	{
		return false;
	}

	if (const UTextBlock* KeyHint = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_KeyHint"))))
	{
		const FString Hint = KeyHint->GetText().ToString();
		TestTrue(TEXT("WBP 키 안내가 ESC를 표시한다"), Hint.Contains(TEXT("ESC")));
		TestTrue(TEXT("WBP 키 안내가 F5를 표시한다"), Hint.Contains(TEXT("F5")));
	}

	struct FRequiredWidget
	{
		const TCHAR* Name;
		UClass* Type;
	};
	const FRequiredWidget RequiredWidgets[] = {
		{ TEXT("SafeZone_Viewport"), USafeZone::StaticClass() },
		{ TEXT("Border_ViewportSurface"), UBorder::StaticClass() },
		{ TEXT("VerticalBox_ResponsiveShell"), UVerticalBox::StaticClass() },
		{ TEXT("HorizontalBox_Categories"), UHorizontalBox::StaticClass() },
		{ TEXT("ScrollBox_Rows"), UScrollBox::StaticClass() },
		{ TEXT("VerticalBox_Rows"), UVerticalBox::StaticClass() },
		{ TEXT("Button_Close"), UButton::StaticClass() },
		{ TEXT("Button_Back"), UButton::StaticClass() },
		{ TEXT("Button_Save"), UButton::StaticClass() },
		{ TEXT("Button_Basic"), UButton::StaticClass() },
		{ TEXT("Button_Account"), UButton::StaticClass() },
		{ TEXT("Button_Keyboard"), UButton::StaticClass() },
		{ TEXT("Button_Graphics"), UButton::StaticClass() },
		{ TEXT("Button_ExitGame"), UButton::StaticClass() },
		{ TEXT("Text_Intro"), UTextBlock::StaticClass() },
		{ TEXT("Text_KeyHint"), UTextBlock::StaticClass() },
		{ TEXT("Text_CategoryTitle"), UTextBlock::StaticClass() },
		{ TEXT("Text_CurrentSummary"), UTextBlock::StaticClass() },
		{ TEXT("Text_Back"), UTextBlock::StaticClass() },
		{ TEXT("Text_Save"), UTextBlock::StaticClass() },
	};
	for (const FRequiredWidget& Contract : RequiredWidgets)
	{
		UWidget* Widget = Tree->FindWidget(FName(Contract.Name));
		TestNotNull(*FString::Printf(TEXT("필수 옵션 위젯이 존재한다: %s"), Contract.Name), Widget);
		TestTrue(*FString::Printf(TEXT("필수 옵션 위젯 타입이 맞다: %s"), Contract.Name),
			Widget && Widget->IsA(Contract.Type));
	}

	const TPair<const TCHAR*, const TCHAR*> CategoryLabels[] = {
		{ TEXT("Text_BasicCategory"), TEXT("기본설정") },
		{ TEXT("Text_AccountCategory"), TEXT("계정정보") },
		{ TEXT("Text_KeyboardCategory"), TEXT("키보드 키 배치") },
		{ TEXT("Text_GraphicsCategory"), TEXT("그래픽") },
		{ TEXT("Text_ExitGameCategory"), TEXT("게임종료") },
	};
	for (const TPair<const TCHAR*, const TCHAR*>& Contract : CategoryLabels)
	{
		const UTextBlock* Label = Cast<UTextBlock>(Tree->FindWidget(FName(Contract.Key)));
		TestNotNull(*FString::Printf(TEXT("카테고리 라벨이 존재한다: %s"), Contract.Key), Label);
		TestEqual(*FString::Printf(TEXT("카테고리 문구가 기획 계약과 같다: %s"), Contract.Key),
			Label ? Label->GetText().ToString() : FString(), FString(Contract.Value));
	}

	const UBorder* ViewportSurface = Cast<UBorder>(Tree->FindWidget(TEXT("Border_ViewportSurface")));
	const USafeZone* ViewportSafeZone = Cast<USafeZone>(Tree->FindWidget(TEXT("SafeZone_Viewport")));
	const UVerticalBox* ResponsiveShell = Cast<UVerticalBox>(
		Tree->FindWidget(TEXT("VerticalBox_ResponsiveShell")));
	TestNotNull(TEXT("불투명 옵션 표면이 뷰포트 전체를 소유한다"), ViewportSurface);
	TestNotNull(TEXT("해상도별 안전 영역을 사용한다"), ViewportSafeZone);
	TestNotNull(TEXT("고정 해상도 대신 반응형 세로 셸을 사용한다"), ResponsiveShell);
	if (const UCanvasPanelSlot* SurfaceSlot = ViewportSurface
		? Cast<UCanvasPanelSlot>(ViewportSurface->Slot) : nullptr)
	{
		const FAnchors Anchors = SurfaceSlot->GetAnchors();
		const FMargin Offsets = SurfaceSlot->GetOffsets();
		TestTrue(TEXT("옵션 표면 앵커는 전체 뷰포트다"),
			Anchors.Minimum.IsNearlyZero() && Anchors.Maximum.Equals(FVector2D(1.f, 1.f)));
		TestTrue(TEXT("옵션 표면은 해상도 독립 zero-offset stretch다"),
			FMath::IsNearlyZero(Offsets.Left) && FMath::IsNearlyZero(Offsets.Top)
			&& FMath::IsNearlyZero(Offsets.Right) && FMath::IsNearlyZero(Offsets.Bottom));
	}
	else
	{
		AddError(TEXT("Border_ViewportSurface에 full-stretch CanvasPanelSlot이 없다."));
	}
	if (ResponsiveShell)
	{
		TestEqual(TEXT("반응형 셸은 헤더·탭·본문·푸터 네 영역만 가진다"),
			ResponsiveShell->GetChildrenCount(), 4);
		const TArray<FName> ExpectedRegions = {
			TEXT("SizeBox_Header"), TEXT("SizeBox_CategoryBar"),
			TEXT("Border_SettingsPanel"), TEXT("SizeBox_Footer")
		};
		for (int32 Index = 0; Index < ExpectedRegions.Num()
			&& Index < ResponsiveShell->GetChildrenCount(); ++Index)
		{
			TestEqual(*FString::Printf(TEXT("반응형 영역 순서 %d"), Index),
				ResponsiveShell->GetChildAt(Index)->GetFName(), ExpectedRegions[Index]);
		}
	}

	for (const FName RemovedLayoutWidget : {
		FName(TEXT("BackgroundBlur_World")), FName(TEXT("Border_Scrim")),
		FName(TEXT("ScaleBox_Viewport")), FName(TEXT("SizeBox_Design1920x1080")),
		FName(TEXT("Border_InspectorPanel")), FName(TEXT("Border_RightPanel")),
		FName(TEXT("Border_SaveGuide")), FName(TEXT("Border_CategoryOverview")) })
	{
		TestNull(*FString::Printf(TEXT("고정 프레임·월드 노출·비상호작용 우측 패널을 제거했다: %s"),
			*RemovedLayoutWidget.ToString()), Tree->FindWidget(RemovedLayoutWidget));
	}
	if (const UTextBlock* CurrentSummary = Cast<UTextBlock>(
		Tree->FindWidget(TEXT("Text_CurrentSummary"))))
	{
		TestEqual(TEXT("필수 상태 문구는 별도 우측 패널이 아니라 하단 액션 바에 있다"),
			CurrentSummary->GetParent() ? CurrentSummary->GetParent()->GetFName() : NAME_None,
			FName(TEXT("HorizontalBox_Footer")));
	}

	for (const FName RemovedWidget : {
		FName(TEXT("Button_Reset")), FName(TEXT("Button_Done")),
		FName(TEXT("Button_Display")), FName(TEXT("Button_Audio")),
		FName(TEXT("Button_Gameplay")), FName(TEXT("Button_Accessibility")) })
	{
		TestNull(*FString::Printf(TEXT("불필요한 구형 옵션 위젯을 새 화면에서 제거했다: %s"),
			*RemovedWidget.ToString()), Tree->FindWidget(RemovedWidget));
	}

	return true;
}

#endif // WITH_EDITOR

#endif // WITH_DEV_AUTOMATION_TESTS
