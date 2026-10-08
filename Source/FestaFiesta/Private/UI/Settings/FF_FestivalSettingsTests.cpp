// FF_FestivalSettingsTests.cpp
// 역할: 설정 화면의 수명과 게임 입력·건설 상태의 복구를 검증한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/Settings/FF_FestivalSettingsPolicy.h"

#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/SafeZone.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "InputMappingContext.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "UI/Settings/FF_FestivalSettingsScreen.h"
#include "UI/Settings/FF_FestivalSettingsSubsystem.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SOverlay.h"
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
		{ TEXT("HorizontalBox_MainContent"), UHorizontalBox::StaticClass() },
		{ TEXT("SizeBox_CategoryRail"), USizeBox::StaticClass() },
		{ TEXT("VerticalBox_Categories"), UVerticalBox::StaticClass() },
		{ TEXT("ScrollBox_Rows"), UScrollBox::StaticClass() },
		{ TEXT("VerticalBox_Rows"), UVerticalBox::StaticClass() },
		{ TEXT("Button_Close"), UButton::StaticClass() },
		{ TEXT("Button_Back"), UButton::StaticClass() },
		{ TEXT("Button_Save"), UButton::StaticClass() },
		{ TEXT("Button_Language"), UButton::StaticClass() },
		{ TEXT("Button_Accessibility"), UButton::StaticClass() },
		{ TEXT("Button_Sound"), UButton::StaticClass() },
		{ TEXT("Button_Credits"), UButton::StaticClass() },
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
		{ TEXT("Text_LanguageCategory"), TEXT("언어") },
		{ TEXT("Text_AccessibilityCategory"), TEXT("접근성") },
		{ TEXT("Text_SoundCategory"), TEXT("사운드") },
		{ TEXT("Text_CreditsCategory"), TEXT("크레딧") },
		{ TEXT("Text_BasicCategory"), TEXT("기본설정") },
		{ TEXT("Text_AccountCategory"), TEXT("계정정보") },
		{ TEXT("Text_KeyboardCategory"), TEXT("조작") },
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
		TestEqual(TEXT("반응형 셸은 헤더·본문·푸터 세 영역만 가진다"),
			ResponsiveShell->GetChildrenCount(), 3);
		const TArray<FName> ExpectedRegions = {
			TEXT("SizeBox_Header"), TEXT("HorizontalBox_MainContent"), TEXT("SizeBox_Footer")
		};
		for (int32 Index = 0; Index < ExpectedRegions.Num()
			&& Index < ResponsiveShell->GetChildrenCount(); ++Index)
		{
			TestEqual(*FString::Printf(TEXT("반응형 영역 순서 %d"), Index),
				ResponsiveShell->GetChildAt(Index)->GetFName(), ExpectedRegions[Index]);
		}
	}
	if (const UHorizontalBox* Main = Cast<UHorizontalBox>(Tree->FindWidget(TEXT("HorizontalBox_MainContent"))))
	{
		TestEqual(TEXT("본문은 왼쪽 탐색과 설정 패널만 가진다"), Main->GetChildrenCount(), 2);
		if (Main->GetChildrenCount() == 2)
		{
			TestEqual(TEXT("탐색 목록이 내용보다 먼저 온다"), Main->GetChildAt(0)->GetFName(), FName(TEXT("SizeBox_CategoryRail")));
			TestEqual(TEXT("선택한 설정이 오른쪽 패널에 나타난다"), Main->GetChildAt(1)->GetFName(), FName(TEXT("Border_SettingsPanel")));
			const UHorizontalBoxSlot* PanelSlot = Cast<UHorizontalBoxSlot>(Main->GetChildAt(1)->Slot);
			TestTrue(TEXT("설정 패널은 남은 너비를 채운다"), PanelSlot && PanelSlot->GetSize().SizeRule == ESlateSizeRule::Fill);
		}
	}
	if (const UVerticalBox* Categories = Cast<UVerticalBox>(Tree->FindWidget(TEXT("VerticalBox_Categories"))))
	{
		TestEqual(TEXT("참고 화면의 여섯 분류와 기존 기본·계정·종료를 모두 제공한다"), Categories->GetChildrenCount(), 9);
	}
	if (const UHorizontalBox* Footer = Cast<UHorizontalBox>(Tree->FindWidget(TEXT("HorizontalBox_Footer"))))
	{
		TestEqual(TEXT("푸터는 적용·뒤로·상태 세 영역만 가진다"), Footer->GetChildrenCount(), 3);
		if (Footer->GetChildrenCount() == 3)
		{
			TestEqual(TEXT("주요 저장 동작이 왼쪽 첫 자리다"), Footer->GetChildAt(0)->GetFName(), FName(TEXT("SizeBox_SaveAction")));
			TestEqual(TEXT("돌아가기 동작이 저장 옆에 있다"), Footer->GetChildAt(1)->GetFName(), FName(TEXT("SizeBox_BackAction")));
			TestEqual(TEXT("상태는 버튼 뒤 남은 너비를 사용한다"), Footer->GetChildAt(2)->GetFName(), FName(TEXT("VerticalBox_FooterStatus")));
		}
	}

	for (const FName RemovedLayoutWidget : {
		FName(TEXT("BackgroundBlur_World")), FName(TEXT("Border_Scrim")),
		FName(TEXT("ScaleBox_Viewport")), FName(TEXT("SizeBox_Design1920x1080")),
		FName(TEXT("Border_InspectorPanel")), FName(TEXT("Border_RightPanel")),
		FName(TEXT("Border_SaveGuide")), FName(TEXT("Border_CategoryOverview")),
		FName(TEXT("ScaleBox_CategoryActions")), FName(TEXT("SizeBox_CategoryActionsDesign")),
		FName(TEXT("ScaleBox_FooterActions")), FName(TEXT("SizeBox_FooterActionsDesign")),
		FName(TEXT("HorizontalBox_Categories")) })
	{
		TestNull(*FString::Printf(TEXT("고정 프레임·월드 노출·비상호작용 우측 패널을 제거했다: %s"),
			*RemovedLayoutWidget.ToString()), Tree->FindWidget(RemovedLayoutWidget));
	}
	if (const UTextBlock* CurrentSummary = Cast<UTextBlock>(
		Tree->FindWidget(TEXT("Text_CurrentSummary"))))
	{
		TestEqual(TEXT("필수 상태 문구는 별도 우측 패널이 아니라 하단 액션 바에 있다"),
			CurrentSummary->GetParent() ? CurrentSummary->GetParent()->GetFName() : NAME_None,
			FName(TEXT("VerticalBox_FooterStatus")));
	}

	for (const FName RemovedWidget : {
		FName(TEXT("Button_Reset")), FName(TEXT("Button_Done")),
		FName(TEXT("Button_Display")), FName(TEXT("Button_Audio")),
		FName(TEXT("Button_Gameplay")) })
	{
		TestNull(*FString::Printf(TEXT("불필요한 구형 옵션 위젯을 새 화면에서 제거했다: %s"),
			*RemovedWidget.ToString()), Tree->FindWidget(RemovedWidget));
	}

	return true;
}

namespace FFFestivalSettingsLayoutTest
{
	constexpr float Tolerance = 1.f;

	void VisitText(UWidget* Widget, const TFunctionRef<void(UTextBlock&)>& Visitor)
	{
		if (UTextBlock* Text = Cast<UTextBlock>(Widget)) { Visitor(*Text); }
		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (UWidget* Child : Panel->GetAllChildren()) { VisitText(Child, Visitor); }
		}
	}

	UUserWidget* FindSettingRow(UFF_FestivalSettingsScreen& Screen, const TCHAR* Label)
	{
		if (const UVerticalBox* Rows = Cast<UVerticalBox>(Screen.GetWidgetFromName(TEXT("VerticalBox_Rows"))))
		{
			for (UWidget* Child : Rows->GetAllChildren())
			{
				if (UUserWidget* Row = Cast<UUserWidget>(Child))
				{
					const UTextBlock* Text = Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Text_Label")));
					if (Text && Text->GetText().ToString() == Label) { return Row; }
				}
			}
		}
		return nullptr;
	}

	bool Contains(const FSlateRect& Outer, const FSlateRect& Inner)
	{
		return Inner.Left >= Outer.Left - Tolerance && Inner.Right <= Outer.Right + Tolerance
			&& Inner.Top >= Outer.Top - Tolerance && Inner.Bottom <= Outer.Bottom + Tolerance;
	}

	bool CaptureAndCheck(FAutomationTestBase& Test, UFF_FestivalSettingsScreen& Screen,
		const TSharedRef<SWidget>& Surface, const FIntPoint Size, const FString& Variant)
	{
		const FString Context = FString::Printf(TEXT("%s %dx%d"), *Variant, Size.X, Size.Y);
		FWidgetRenderer Renderer(false, true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(
			FWidgetRenderer::CreateTargetFor(FVector2D(Size.X, Size.Y), TF_Bilinear, true));
		if (!Test.TestNotNull(Context + TEXT(" 렌더 타깃"), Target.Get())) { return false; }
		auto DrawSettledFrames = [&]()
		{
			for (int32 Frame = 0; Frame < 3; ++Frame)
			{
				Screen.ForceLayoutPrepass();
				Renderer.DrawWidget(Target.Get(), Surface, FVector2D(Size.X, Size.Y), 1.f / 60.f, false);
			}
		};
		DrawSettledFrames();
		if (Variant == TEXT("CreditsBottom"))
		{
			// Newly exposed credit text can wrap after the first scroll, changing the final extent.
			UScrollBox* RowsScroll = Cast<UScrollBox>(Screen.GetWidgetFromName(TEXT("ScrollBox_Rows")));
			if (!Test.TestNotNull(Context + TEXT(" 크레딧 끝 스크롤"), RowsScroll)) { return false; }
			RowsScroll->ScrollToEnd();
			DrawSettledFrames();
		}
		FlushRenderingCommands();

		const FSlateRect Viewport(0.f, 0.f, static_cast<float>(Size.X), static_cast<float>(Size.Y));
		auto RectFor = [&](const TCHAR* Name)
		{
			const UWidget* Widget = Screen.GetWidgetFromName(Name);
			if (!Test.TestNotNull(Context + TEXT(" / ") + Name, Widget)) { return FSlateRect(); }
			const FSlateRect Rect = Widget->GetCachedGeometry().GetLayoutBoundingRect();
			Test.TestTrue(Context + TEXT(" 표시되는 영역 / ") + Name, Widget->IsVisible());
			Test.TestTrue(Context + TEXT(" 화면 안의 영역 / ") + Name, Contains(Viewport, Rect));
			Test.TestTrue(Context + TEXT(" 실제 크기를 가진 영역 / ") + Name,
				Rect.GetSize().X > 0.f && Rect.GetSize().Y > 0.f);
			return Rect;
		};
		const FSlateRect Header = RectFor(TEXT("SizeBox_Header"));
		const FSlateRect Rail = RectFor(TEXT("SizeBox_CategoryRail"));
		const FSlateRect Body = RectFor(TEXT("Border_SettingsPanel"));
		const FSlateRect Footer = RectFor(TEXT("SizeBox_Footer"));
		const FSlateRect Scroll = RectFor(TEXT("ScrollBox_Rows"));
		Test.TestTrue(Context + TEXT(" 탐색 목록과 설정 내용이 겹치지 않는다"), Rail.Right <= Body.Left + Tolerance);
		Test.TestTrue(Context + TEXT(" 헤더·본문·고정 푸터가 겹치지 않는다"),
			Header.Bottom <= FMath::Min(Rail.Top, Body.Top) + Tolerance
			&& FMath::Max(Rail.Bottom, Body.Bottom) <= Footer.Top + Tolerance);
		Test.TestTrue(Context + TEXT(" 스크롤 내용이 설정 패널 안에 있다"), Contains(Body, Scroll));
		Test.TestTrue(Context + TEXT(" 최소 화면에서도 내용을 읽을 높이가 남는다"), Scroll.GetSize().Y >= 300.f);
		Test.TestEqual(Context + TEXT(" 푸터가 화면 아래까지 닿는다"), Footer.Bottom, Viewport.Bottom, Tolerance);

		auto CheckText = [&](const TCHAR* Name, const FSlateRect& Container, const bool bEllipsis = false)
		{
			const UTextBlock* Text = Cast<UTextBlock>(Screen.GetWidgetFromName(Name));
			if (!Test.TestNotNull(Context + TEXT(" 문구 / ") + Name, Text)) { return; }
			const FSlateRect Rect = Text->GetCachedGeometry().GetLayoutBoundingRect();
			const FVector2D Desired = Text->GetDesiredSize();
			Test.TestTrue(Context + TEXT(" 읽을 수 있는 문구 / ") + Name,
				Text->IsVisible() && !Text->GetText().IsEmpty() && Text->GetFont().Size >= 14);
			Test.TestTrue(Context + TEXT(" 문구가 컨테이너 안에 들어온다 / ") + Name, Contains(Container, Rect));
			Test.TestTrue(Context + TEXT(" 글자 높이가 잘리지 않는다 / ") + Name, Rect.GetSize().Y + Tolerance >= Desired.Y);
			Test.TestTrue(Context + TEXT(" 글자 폭이 확보된다 / ") + Name,
				Rect.GetSize().X + Tolerance >= (bEllipsis ? FMath::Min(200.0, Desired.X) : Desired.X));
		};
		const TPair<const TCHAR*, const TCHAR*> Navigation[] = {
			{ TEXT("Button_Language"), TEXT("Text_LanguageCategory") },
			{ TEXT("Button_Accessibility"), TEXT("Text_AccessibilityCategory") },
			{ TEXT("Button_Sound"), TEXT("Text_SoundCategory") },
			{ TEXT("Button_Graphics"), TEXT("Text_GraphicsCategory") },
			{ TEXT("Button_Keyboard"), TEXT("Text_KeyboardCategory") },
			{ TEXT("Button_Credits"), TEXT("Text_CreditsCategory") },
			{ TEXT("Button_Basic"), TEXT("Text_BasicCategory") },
			{ TEXT("Button_Account"), TEXT("Text_AccountCategory") },
			{ TEXT("Button_ExitGame"), TEXT("Text_ExitGameCategory") }
		};
		float PreviousBottom = Rail.Top;
		for (const auto& Item : Navigation)
		{
			const FSlateRect Button = RectFor(Item.Key);
			Test.TestTrue(Context + TEXT(" 탐색 버튼이 목록 안에서 겹치지 않는다 / ") + Item.Key,
				Contains(Rail, Button) && Button.Top >= PreviousBottom - Tolerance);
			Test.TestTrue(Context + TEXT(" 탐색 버튼의 클릭 영역이 충분하다 / ") + Item.Key,
				Button.GetSize().X >= 44.f && Button.GetSize().Y >= 44.f);
			PreviousBottom = Button.Bottom;
			CheckText(Item.Value, Button);
		}
		const FSlateRect Save = RectFor(TEXT("Button_Save"));
		const FSlateRect Back = RectFor(TEXT("Button_Back"));
		Test.TestTrue(Context + TEXT(" 저장과 뒤로 버튼이 푸터 안에서 겹치지 않는다"),
			Contains(Footer, Save) && Contains(Footer, Back) && Save.Right <= Back.Left + Tolerance);
		Test.TestTrue(Context + TEXT(" 고정 액션의 클릭 높이를 유지한다"),
			Save.GetSize().Y >= 44.f && Back.GetSize().Y >= 44.f);
		CheckText(TEXT("Text_Save"), Save);
		CheckText(TEXT("Text_Back"), Back);
		CheckText(TEXT("Text_CurrentSummary"), Footer, true);
		CheckText(TEXT("Text_CategoryTitle"), Body);
		CheckText(TEXT("Text_Intro"), Body);
		// Only fully visible rows are checked; partially scrolled rows are deliberately clipped by ScrollBox.
		if (const UVerticalBox* Rows = Cast<UVerticalBox>(Screen.GetWidgetFromName(TEXT("VerticalBox_Rows"))))
		{
			if (Variant == TEXT("CreditsBottom"))
			{
				const UWidget* LastRow = Rows->GetChildrenCount() > 0 ? Rows->GetChildAt(Rows->GetChildrenCount() - 1) : nullptr;
				if (!Test.TestNotNull(Context + TEXT(" 마지막 출처 카드"), LastRow)) { return false; }
				const FSlateRect LastRect = LastRow->GetCachedGeometry().GetLayoutBoundingRect();
				Test.TestTrue(Context + TEXT(" 끝까지 스크롤하면 마지막 출처 카드 전체를 읽을 수 있다"),
					LastRect.GetSize().Y > 0.f && Contains(Scroll, LastRect));
			}
			int32 CheckedRows = 0;
			for (UWidget* Child : Rows->GetAllChildren())
			{
				UUserWidget* Row = Cast<UUserWidget>(Child);
				const FSlateRect RowRect = Child->GetCachedGeometry().GetLayoutBoundingRect();
				if (!Contains(Scroll, RowRect) || RowRect.GetSize().Y <= 0.f) { continue; }
				if (!Row)
				{
					VisitText(Child, [&](UTextBlock& Text)
					{
						if (!Text.IsVisible()) { return; }
						const FSlateRect Rect = Text.GetCachedGeometry().GetLayoutBoundingRect();
						Test.TestTrue(Context + TEXT(" 정보 카드 문구가 잘리지 않는다 / ") + Text.GetText().ToString(),
							Contains(RowRect, Rect) && Rect.GetSize().Y + Tolerance >= Text.GetDesiredSize().Y);
					});
					continue;
				}
				if (!Row->GetWidgetFromName(TEXT("Text_Label"))) { continue; }
				++CheckedRows;
				for (const TCHAR* Name : { TEXT("Text_Label"), TEXT("Text_Description"), TEXT("Text_Value"), TEXT("Text_FestivalToggleState") })
				{
					const UTextBlock* Text = Cast<UTextBlock>(Row->GetWidgetFromName(Name));
					if (!Text || !Text->IsVisible()) { continue; }
					const FSlateRect Rect = Text->GetCachedGeometry().GetLayoutBoundingRect();
					Test.TestTrue(Context + TEXT(" 설정 행 문구가 카드 안에 있다 / ") + Name, Contains(RowRect, Rect));
					Test.TestTrue(Context + TEXT(" 설정 행 글자가 잘리지 않는다 / ") + Name,
						Rect.GetSize().Y + Tolerance >= Text->GetDesiredSize().Y && Rect.GetSize().X > 0.f);
				}
				for (const TCHAR* Name : { TEXT("Slider_Value"), TEXT("CheckBox_Value"), TEXT("Button_Previous"), TEXT("Button_Next") })
				{
					const UWidget* Control = Row->GetWidgetFromName(Name);
					if (!Control || !Control->IsVisible()) { continue; }
					const FSlateRect Rect = Control->GetCachedGeometry().GetLayoutBoundingRect();
					Test.TestTrue(Context + TEXT(" 설정 조작부가 카드 안에 있다 / ") + Name,
						Contains(RowRect, Rect) && Rect.GetSize().X > 0.f && Rect.GetSize().Y > 0.f);
				}
			}
			if (Variant.StartsWith(TEXT("Basic")) || Variant == TEXT("Graphics")
				|| Variant == TEXT("Sound") || Variant == TEXT("Accessibility"))
			{
				Test.TestTrue(Context + TEXT(" 실제 설정 행을 하나 이상 검증했다"), CheckedRows > 0);
			}
		}

		TArray<FColor> Pixels;
		FReadSurfaceDataFlags Flags;
		Flags.SetLinearToGamma(false);
		if (!Test.TestTrue(Context + TEXT(" 실제 렌더 픽셀을 읽는다"),
			Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)
			&& Pixels.Num() == Size.X * Size.Y)) { return false; }
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("FestivalOptions/Layout"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString Path = FPaths::Combine(Directory,
			FString::Printf(TEXT("Options_%s_%dx%d.png"), *Variant, Size.X, Size.Y));
		const bool bSaved = FFileHelper::SaveArrayToFile(Png, *Path);
		Test.TestTrue(Context + TEXT(" 레이아웃 캡처를 저장한다"), bSaved);
		Test.AddInfo(FString::Printf(TEXT("옵션 레이아웃 캡처: %s"), *FPaths::ConvertRelativePathToFull(Path)));
		return bSaved;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FestivalSettings_RenderedLayout,
	"FestaFiesta.UI.Settings.RenderedLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)

bool FFF_FestivalSettings_RenderedLayout::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	UWidgetBlueprintGeneratedClass* ScreenClass = Cast<UWidgetBlueprintGeneratedClass>(
		LoadClass<UFF_FestivalSettingsScreen>(nullptr,
			TEXT("/Game/FestaFiesta/UI/Settings/WBP_FF_FullscreenOptions.WBP_FF_FullscreenOptions_C")));
	if (!TestNotNull(TEXT("레이아웃 검증용 에디터 월드"), World)
		|| !TestNotNull(TEXT("저장된 옵션 화면 클래스"), ScreenClass)) { return false; }
	if (!TestNull(TEXT("입력·게임 설정을 변경하지 않는 에디터 월드다"), World->GetGameInstance())) { return false; }
	const bool bWasDirty = ScreenClass->GetOutermost()->IsDirty();
	TArray<TPair<UWidgetBlueprintGeneratedClass*, bool>> InitializationFlags;
	auto AllowPlayerlessInitialization = [&InitializationFlags](UWidgetBlueprintGeneratedClass* Class)
	{
		InitializationFlags.Emplace(Class, static_cast<bool>(Class->bCanCallInitializedWithoutPlayerContext));
		Class->bCanCallInitializedWithoutPlayerContext = true;
	};
	ON_SCOPE_EXIT
	{
		for (const auto& Flag : InitializationFlags) { Flag.Key->bCanCallInitializedWithoutPlayerContext = Flag.Value; }
	};
	AllowPlayerlessInitialization(ScreenClass);
	for (const TCHAR* Name : { TEXT("WBP_FF_SettingToggleRow"), TEXT("WBP_FF_SettingSliderRow"),
		TEXT("WBP_FF_SettingSelectRow"), TEXT("WBP_FF_KeyBindingPanel") })
	{
		const FString Path = FString::Printf(TEXT("/Game/FestaFiesta/UI/Settings/%s.%s_C"), Name, Name);
		UWidgetBlueprintGeneratedClass* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UUserWidget>(nullptr, *Path));
		if (!TestNotNull(FString(TEXT("저장된 하위 설정 위젯 / ")) + Name, Class)) { return false; }
		AllowPlayerlessInitialization(Class);
	}
	TStrongObjectPtr<UFF_FestivalSettingsScreen> Screen(CreateWidget<UFF_FestivalSettingsScreen>(World, ScreenClass));
	if (!TestNotNull(TEXT("저장된 옵션 WBP를 재생성 없이 인스턴스화한다"), Screen.Get())) { return false; }
	const TSharedRef<SOverlay> Surface = SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)[Screen->TakeWidget()];
	// ShowCategory도 실제 진입 경로처럼 버튼 포커스를 요청한다. 이 표시 전용 검사는 가짜
	// LocalPlayer를 만들지 않으므로 아래 ShowCategory 호출 수만큼의 포커스 오류만 예상한다.
	// 다른 버튼/횟수의 포커스 오류나 레이아웃 실패는 정상적으로 검사 실패로 남긴다.
	for (const TPair<const TCHAR*, int32>& Expected : {
		TPair<const TCHAR*, int32>(TEXT("Button_Account"), 1),
		TPair<const TCHAR*, int32>(TEXT("Button_Basic"), 2),
		TPair<const TCHAR*, int32>(TEXT("Button_Graphics"), 2),
		TPair<const TCHAR*, int32>(TEXT("Button_Keyboard"), 2),
		TPair<const TCHAR*, int32>(TEXT("Button_Language"), 2),
		TPair<const TCHAR*, int32>(TEXT("Button_Accessibility"), 2),
		TPair<const TCHAR*, int32>(TEXT("Button_Sound"), 2),
		TPair<const TCHAR*, int32>(TEXT("Button_Credits"), 2) })
	{
		AddExpectedError(FString(TEXT("The PlayerController is not a valid local player so it can't focus on  "))
			+ Expected.Key, EAutomationExpectedErrorFlags::Contains, Expected.Value, false);
	}
	// ScreenStack의 전체 화면 배치를 재현한다. 계정 화면은 로컬 플레이어가 없어도 동일한 정보 행을 만든다.
	Screen->SetVisibility(ESlateVisibility::Visible);
	Screen->ShowCategory(EFF_FestivalSettingsCategory::Account);
	FAssetCompilingManager::Get().FinishAllCompilation();
	FlushRenderingCommands();
	for (const FIntPoint Size : { FIntPoint(1280, 720), FIntPoint(1920, 1080), FIntPoint(2560, 1080) })
	{
		if (!FFFestivalSettingsLayoutTest::CaptureAndCheck(*this, *Screen, Surface, Size, TEXT("Account"))) { return false; }
	}
	UScrollBox* RowsScroll = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("ScrollBox_Rows")));
	if (!TestNotNull(TEXT("설정 내용 스크롤"), RowsScroll)) { return false; }
	UVerticalBox* Rows = Cast<UVerticalBox>(Screen->GetWidgetFromName(TEXT("VerticalBox_Rows")));
	if (!TestNotNull(TEXT("현재 분류의 실제 설정 행"), Rows)) { return false; }
	const TCHAR* SoundLabels[] = { TEXT("마스터 음량"), TEXT("배경음 음량"), TEXT("효과음 음량"),
		TEXT("불꽃놀이 음량"), TEXT("UI 소리 음량") };
	auto FindSlider = [&](const TCHAR* Label)
	{
		UUserWidget* Row = FFFestivalSettingsLayoutTest::FindSettingRow(*Screen, Label);
		USlider* Slider = Row ? Cast<USlider>(Row->GetWidgetFromName(TEXT("Slider_Value"))) : nullptr;
		TestNotNull(FString(TEXT("조작 가능한 음량 / ")) + Label, Slider);
		return Slider;
	};
	auto FindToggle = [&](const TCHAR* Label)
	{
		UUserWidget* Row = FFFestivalSettingsLayoutTest::FindSettingRow(*Screen, Label);
		UCheckBox* Toggle = Row ? Cast<UCheckBox>(Row->GetWidgetFromName(TEXT("CheckBox_Value"))) : nullptr;
		TestNotNull(FString(TEXT("조작 가능한 토글 / ")) + Label, Toggle);
		return Toggle;
	};
	auto ActiveInfoCopy = [&]()
	{
		FString Copy;
		FFFestivalSettingsLayoutTest::VisitText(Rows, [&](UTextBlock& Text)
		{
			Copy += Text.GetText().ToString() + TEXT("\n");
		});
		return Copy;
	};
	for (const FIntPoint Size : { FIntPoint(1280, 720), FIntPoint(1920, 1080) })
	{
		Screen->ShowCategory(EFF_FestivalSettingsCategory::Language);
		TestEqual(TEXT("언어 분류는 표시 언어 정보 하나를 제공한다"), Rows->GetChildrenCount(), 1);
		TestTrue(TEXT("지원하지 않는 언어를 선택지로 노출하지 않는다"), ActiveInfoCopy().Contains(TEXT("현재는 한국어만 지원합니다")));
		if (!FFFestivalSettingsLayoutTest::CaptureAndCheck(*this, *Screen, Surface, Size, TEXT("Language"))) { return false; }
		Screen->ShowCategory(EFF_FestivalSettingsCategory::Accessibility);
		TestEqual(TEXT("접근성 분류는 움직임 감소 조작을 제공한다"), Rows->GetChildrenCount(), 1);
		if (!FindToggle(TEXT("움직임 감소"))) { return false; }
		if (!FFFestivalSettingsLayoutTest::CaptureAndCheck(*this, *Screen, Surface, Size, TEXT("Accessibility"))) { return false; }
		Screen->ShowCategory(EFF_FestivalSettingsCategory::Sound);
		TestEqual(TEXT("사운드 분류는 기존 다섯 음량을 모두 제공한다"), Rows->GetChildrenCount(), 5);
		for (const TCHAR* Label : SoundLabels) { if (!FindSlider(Label)) { return false; } }
		if (!FFFestivalSettingsLayoutTest::CaptureAndCheck(*this, *Screen, Surface, Size, TEXT("Sound"))) { return false; }
		Screen->ShowCategory(EFF_FestivalSettingsCategory::Credits);
		TestEqual(TEXT("크레딧에 출처 표기와 등록된 일곱 에셋이 있다"), Rows->GetChildrenCount(), 8);
		const FString CreditsCopy = ActiveInfoCopy();
		for (const TCHAR* Required : { TEXT("Solar System Scope"), TEXT("Pretendard"), TEXT("Michroma"),
			TEXT("Dongle"), TEXT("Copernicus"), TEXT("geoBoundaries"), TEXT("Natural Earth"),
			TEXT("CC BY 4.0"), TEXT("SIL OFL 1.1"), TEXT("Public Domain"), TEXT("THIRD_PARTY_NOTICES") })
		{
			TestTrue(FString(TEXT("크레딧 출처와 라이선스가 빠지지 않는다 / ")) + Required, CreditsCopy.Contains(Required));
		}
		if (!FFFestivalSettingsLayoutTest::CaptureAndCheck(*this, *Screen, Surface, Size, TEXT("Credits"))) { return false; }
		RowsScroll->ScrollToEnd();
		if (!FFFestivalSettingsLayoutTest::CaptureAndCheck(*this, *Screen, Surface, Size, TEXT("CreditsBottom"))) { return false; }
		Screen->ShowCategory(EFF_FestivalSettingsCategory::Basic);
		RowsScroll->ScrollToStart();
		FAssetCompilingManager::Get().FinishAllCompilation();
		if (!FFFestivalSettingsLayoutTest::CaptureAndCheck(*this, *Screen, Surface, Size, TEXT("Basic"))) { return false; }
		RowsScroll->ScrollToEnd();
		if (!FFFestivalSettingsLayoutTest::CaptureAndCheck(*this, *Screen, Surface, Size, TEXT("BasicToggles"))) { return false; }
		Screen->ShowCategory(EFF_FestivalSettingsCategory::Graphics);
		RowsScroll->ScrollToStart();
		if (!FFFestivalSettingsLayoutTest::CaptureAndCheck(*this, *Screen, Surface, Size, TEXT("Graphics"))) { return false; }
		Screen->ShowCategory(EFF_FestivalSettingsCategory::Keyboard);
		RowsScroll->ScrollToStart();
		FAssetCompilingManager::Get().FinishAllCompilation();
		if (!FFFestivalSettingsLayoutTest::CaptureAndCheck(*this, *Screen, Surface, Size, TEXT("Keyboard"))) { return false; }
	}
	// Exercise the actual category/row delegates on this temporary screen. No Save action or
	// player subsystem is called, so shared drafts can be tested without changing user settings.
	auto ClickCategory = [&](const TCHAR* ButtonName, const TCHAR* ExpectedTitle)
	{
		UButton* Button = Cast<UButton>(Screen->GetWidgetFromName(ButtonName));
		if (!TestNotNull(FString(TEXT("연결된 분류 버튼 / ")) + ButtonName, Button)) { return false; }
		if (!TestTrue(FString(TEXT("분류 클릭 이벤트 / ")) + ButtonName, Button->OnClicked.IsBound())) { return false; }
		Button->OnClicked.Broadcast();
		const UTextBlock* Title = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_CategoryTitle")));
		return TestTrue(FString(TEXT("클릭이 해당 분류로 이동한다 / ")) + ButtonName,
			Title && Title->GetText().ToString() == ExpectedTitle);
	};
	if (!ClickCategory(TEXT("Button_Language"), TEXT("언어"))
		|| !ClickCategory(TEXT("Button_Credits"), TEXT("크레딧"))
		|| !ClickCategory(TEXT("Button_Basic"), TEXT("기본설정"))) { return false; }
	const float EditedVolumes[] = { 0.15f, 0.30f, 0.45f, 0.60f, 0.75f };
	TArray<float> OriginalVolumes;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(SoundLabels); ++Index)
	{
		USlider* Slider = FindSlider(SoundLabels[Index]);
		if (!Slider) { return false; }
		OriginalVolumes.Add(Slider->GetValue());
		Slider->SetValue(EditedVolumes[Index]);
		Slider->OnValueChanged.Broadcast(EditedVolumes[Index]);
	}
	const TCHAR* ToggleLabels[] = { TEXT("움직임 감소"), TEXT("커서 기준 줌"), TEXT("가장자리 스크롤") };
	TArray<bool> OriginalToggles;
	for (const TCHAR* Label : ToggleLabels)
	{
		UCheckBox* Toggle = FindToggle(Label);
		if (!Toggle) { return false; }
		const bool bOriginal = Toggle->IsChecked();
		OriginalToggles.Add(bOriginal);
		Toggle->SetIsChecked(!bOriginal);
		Toggle->OnCheckStateChanged.Broadcast(!bOriginal);
	}
	if (!ClickCategory(TEXT("Button_Sound"), TEXT("사운드"))) { return false; }
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(SoundLabels); ++Index)
	{
		USlider* Slider = FindSlider(SoundLabels[Index]);
		if (!Slider) { return false; }
		TestEqual(FString(TEXT("기본설정의 음량 초안이 사운드에서도 유지된다 / ")) + SoundLabels[Index],
			Slider->GetValue(), EditedVolumes[Index]);
		Slider->SetValue(OriginalVolumes[Index]);
		Slider->OnValueChanged.Broadcast(OriginalVolumes[Index]);
	}
	if (!ClickCategory(TEXT("Button_Accessibility"), TEXT("접근성"))) { return false; }
	UCheckBox* Motion = FindToggle(ToggleLabels[0]);
	if (!Motion) { return false; }
	TestEqual(TEXT("기본설정의 움직임 감소 초안이 접근성에서도 유지된다"), Motion->IsChecked(), !OriginalToggles[0]);
	Motion->SetIsChecked(OriginalToggles[0]);
	Motion->OnCheckStateChanged.Broadcast(OriginalToggles[0]);
	if (!ClickCategory(TEXT("Button_Keyboard"), TEXT("조작"))) { return false; }
	for (int32 Index = 1; Index < UE_ARRAY_COUNT(ToggleLabels); ++Index)
	{
		UCheckBox* Toggle = FindToggle(ToggleLabels[Index]);
		if (!Toggle) { return false; }
		TestEqual(FString(TEXT("기본설정의 카메라 초안이 조작에서도 유지된다 / ")) + ToggleLabels[Index],
			Toggle->IsChecked(), !OriginalToggles[Index]);
		Toggle->SetIsChecked(OriginalToggles[Index]);
		Toggle->OnCheckStateChanged.Broadcast(OriginalToggles[Index]);
	}
	if (!ClickCategory(TEXT("Button_Basic"), TEXT("기본설정"))) { return false; }
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(SoundLabels); ++Index)
	{
		USlider* Slider = FindSlider(SoundLabels[Index]);
		if (!Slider) { return false; }
		TestEqual(FString(TEXT("사운드 변경이 기본설정에도 반영된다 / ")) + SoundLabels[Index],
			Slider->GetValue(), OriginalVolumes[Index]);
	}
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ToggleLabels); ++Index)
	{
		UCheckBox* Toggle = FindToggle(ToggleLabels[Index]);
		if (!Toggle) { return false; }
		TestEqual(FString(TEXT("전용 분류의 토글 변경이 기본설정에도 반영된다 / ")) + ToggleLabels[Index],
			Toggle->IsChecked(), OriginalToggles[Index]);
	}
	AddInfo(TEXT("키보드 캡처는 로컬 플레이어 없는 표시 상태다. 실제 키 변경·저장 동작은 호출하지 않는다."));
	// 가장 긴 확인 문구도 버튼 너비/높이를 넘지 않아야 한다. 인스턴스의 문구만 바꾸며 저장 동작은 호출하지 않는다.
	UTextBlock* BackText = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_Back")));
	UTextBlock* SaveText = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_Save")));
	if (!TestNotNull(TEXT("뒤로 확인 문구"), BackText) || !TestNotNull(TEXT("해상도 유지 문구"), SaveText)) { return false; }
	BackText->SetText(FText::FromString(TEXT("변경 버리고 이전으로")));
	SaveText->SetText(FText::FromString(TEXT("해상도 유지 (15)")));
	if (!FFFestivalSettingsLayoutTest::CaptureAndCheck(*this, *Screen, Surface, FIntPoint(1280, 720), TEXT("Confirmation"))) { return false; }
	TestEqual(TEXT("레이아웃 검증이 저장된 옵션 패키지를 변경하지 않는다"), ScreenClass->GetOutermost()->IsDirty(), bWasDirty);
	return true;
}

#endif // WITH_EDITOR

#endif // WITH_DEV_AUTOMATION_TESTS
