// FF_PauseHubTests.cpp
// 역할: 일시정지 메뉴의 입력·화면 연결·재개 동작을 검증한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SafeZone.h"
#include "Components/TextBlock.h"
#include "UI/Pause/FF_PauseHubScreen.h"
#include "UI/Settings/FF_FestivalSettingsSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_PauseHub_WidgetContract,
	"FestaFiesta.UI.Pause.WidgetContract",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_PauseHub_WidgetContract::RunTest(const FString& Parameters)
{
	(void)Parameters;
	constexpr const TCHAR* PauseHubClassPath =
		TEXT("/Game/FestaFiesta/UI/Pause/WBP_FF_PauseHub.WBP_FF_PauseHub_C");

	const UFF_FestivalSettingsDeveloperSettings* Settings =
		UFF_FestivalSettingsDeveloperSettings::Get();
	TestNotNull(TEXT("일시정지 개발 설정을 로드한다"), Settings);
	if (Settings)
	{
		TestEqual(TEXT("ESC/F5 기본 목적지는 1단계 Pause Hub다"),
			Settings->PauseHubScreenClass.ToSoftObjectPath().ToString(), FString(PauseHubClassPath));
	}

	UClass* HubClass = LoadClass<UFF_PauseHubScreen>(nullptr, PauseHubClassPath);
	TestNotNull(TEXT("Pause Hub WBP 클래스를 로드한다"), HubClass);
	TestTrue(TEXT("Pause Hub WBP는 네이티브 PauseHubScreen을 상속한다"),
		HubClass && HubClass->IsChildOf(UFF_PauseHubScreen::StaticClass()));

	const UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(HubClass);
	UWidgetTree* Tree = GeneratedClass ? GeneratedClass->GetWidgetTreeArchetype() : nullptr;
	TestNotNull(TEXT("Pause Hub WidgetTree가 저장됐다"), Tree);
	if (!Tree)
	{
		return false;
	}

	struct FRequiredWidget
	{
		const TCHAR* Name;
		UClass* Type;
	};
	const FRequiredWidget RequiredWidgets[] = {
		{ TEXT("Border_ViewportSurface"), UBorder::StaticClass() },
		{ TEXT("SafeZone_Viewport"), USafeZone::StaticClass() },
		{ TEXT("CanvasPanel_Responsive"), UCanvasPanel::StaticClass() },
		{ TEXT("Border_Header"), UBorder::StaticClass() },
		{ TEXT("Border_MenuPanel"), UBorder::StaticClass() },
		{ TEXT("Border_ContextPanel"), UBorder::StaticClass() },
		{ TEXT("Border_Footer"), UBorder::StaticClass() },
		{ TEXT("Button_Resume"), UButton::StaticClass() },
		{ TEXT("Button_Settings"), UButton::StaticClass() },
		{ TEXT("Button_Keyboard"), UButton::StaticClass() },
		{ TEXT("Button_Account"), UButton::StaticClass() },
		{ TEXT("Button_Exit"), UButton::StaticClass() },
		{ TEXT("Button_Close"), UButton::StaticClass() },
		{ TEXT("Image_FocusResume"), UImage::StaticClass() },
		{ TEXT("Image_FocusSettings"), UImage::StaticClass() },
		{ TEXT("Image_FocusKeyboard"), UImage::StaticClass() },
		{ TEXT("Image_FocusAccount"), UImage::StaticClass() },
		{ TEXT("Image_FocusExit"), UImage::StaticClass() },
		{ TEXT("Text_FestivalName"), UTextBlock::StaticClass() },
		{ TEXT("Text_DayStage"), UTextBlock::StaticClass() },
		{ TEXT("Text_Clock"), UTextBlock::StaticClass() },
		{ TEXT("Text_Gold"), UTextBlock::StaticClass() },
		{ TEXT("Text_Visitors"), UTextBlock::StaticClass() },
		{ TEXT("Text_Congestion"), UTextBlock::StaticClass() },
		{ TEXT("Text_SelectionTitle"), UTextBlock::StaticClass() },
		{ TEXT("Text_SelectionDescription"), UTextBlock::StaticClass() },
		{ TEXT("Text_KeyHint"), UTextBlock::StaticClass() },
	};
	for (const FRequiredWidget& Contract : RequiredWidgets)
	{
		UWidget* Widget = Tree->FindWidget(FName(Contract.Name));
		TestNotNull(*FString::Printf(TEXT("필수 Pause Hub 위젯이 존재한다: %s"), Contract.Name), Widget);
		TestTrue(*FString::Printf(TEXT("필수 Pause Hub 위젯 타입이 맞다: %s"), Contract.Name),
			Widget && Widget->IsA(Contract.Type));
	}
	TestNull(TEXT("장식용 생성 히어로는 Pause Hub 계약에서 제거됐다"),
		Tree->FindWidget(TEXT("Image_FestivalHero")));
	for (const TCHAR* ForbiddenWidget : {
		TEXT("BackgroundBlur_World"), TEXT("Border_Scrim"), TEXT("ScaleBox_Viewport"),
		TEXT("SizeBox_Design1920x1080"), TEXT("CanvasPanel_Design") })
	{
		TestNull(*FString::Printf(TEXT("고정 프레임/월드 노출 위젯을 사용하지 않는다: %s"),
			ForbiddenWidget), Tree->FindWidget(FName(ForbiddenWidget)));
	}

	const TPair<const TCHAR*, const TCHAR*> MenuLabels[] = {
		{ TEXT("Text_Resume"), TEXT("계속하기") },
		{ TEXT("Text_Settings"), TEXT("시스템 설정") },
		{ TEXT("Text_Keyboard"), TEXT("조작 가이드") },
		{ TEXT("Text_Account"), TEXT("계정 정보") },
		{ TEXT("Text_Exit"), TEXT("게임 종료") },
	};
	for (const TPair<const TCHAR*, const TCHAR*>& Contract : MenuLabels)
	{
		const UTextBlock* Label = Cast<UTextBlock>(Tree->FindWidget(FName(Contract.Key)));
		TestNotNull(*FString::Printf(TEXT("Pause Hub 메뉴 라벨이 존재한다: %s"), Contract.Key), Label);
		TestEqual(*FString::Printf(TEXT("Pause Hub 메뉴 문구가 목적과 일치한다: %s"), Contract.Key),
			Label ? Label->GetText().ToString() : FString(), FString(Contract.Value));
	}

	if (const UTextBlock* KeyHint = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_KeyHint"))))
	{
		TestTrue(TEXT("하단 힌트가 ESC를 안내한다"), KeyHint->GetText().ToString().Contains(TEXT("ESC")));
		TestTrue(TEXT("하단 힌트가 F5를 안내한다"), KeyHint->GetText().ToString().Contains(TEXT("F5")));
	}

	auto TestAnchoredRegion = [this, Tree](const TCHAR* WidgetName,
		const FVector2D ExpectedMin, const FVector2D ExpectedMax, const FMargin ExpectedOffsets)
	{
		const UWidget* Widget = Tree->FindWidget(FName(WidgetName));
		const UCanvasPanelSlot* Slot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr;
		TestNotNull(*FString::Printf(TEXT("%s는 반응형 캔버스에 배치됐다"), WidgetName), Slot);
		if (Slot)
		{
			TestEqual(*FString::Printf(TEXT("%s 최소 앵커"), WidgetName),
				Slot->GetAnchors().Minimum, ExpectedMin);
			TestEqual(*FString::Printf(TEXT("%s 최대 앵커"), WidgetName),
				Slot->GetAnchors().Maximum, ExpectedMax);
			TestEqual(*FString::Printf(TEXT("%s 반응형 오프셋"), WidgetName),
				Slot->GetOffsets(), ExpectedOffsets);
		}
	};
	TestAnchoredRegion(TEXT("Border_ViewportSurface"), FVector2D(0.f, 0.f), FVector2D(1.f, 1.f),
		FMargin(0.f));
	TestAnchoredRegion(TEXT("SafeZone_Viewport"), FVector2D(0.f, 0.f), FVector2D(1.f, 1.f),
		FMargin(0.f));
	TestAnchoredRegion(TEXT("Border_Header"), FVector2D(0.f, 0.f), FVector2D(1.f, 0.f),
		FMargin(0.f, 0.f, 0.f, 88.f));
	TestAnchoredRegion(TEXT("Border_MenuPanel"), FVector2D(0.f, 0.f), FVector2D(0.34f, 1.f),
		FMargin(0.f, 88.f, 0.f, 84.f));
	TestAnchoredRegion(TEXT("Border_ContextPanel"), FVector2D(0.34f, 0.f), FVector2D(1.f, 1.f),
		FMargin(0.f, 88.f, 0.f, 84.f));
	TestAnchoredRegion(TEXT("Border_Footer"), FVector2D(0.f, 1.f), FVector2D(1.f, 1.f),
		FMargin(0.f, -84.f, 0.f, 84.f));
	TestAnchoredRegion(TEXT("Border_CurrentFestival"), FVector2D(0.f, 0.f), FVector2D(1.f, 0.f),
		FMargin(24.f, 24.f, 24.f, 168.f));
	TestAnchoredRegion(TEXT("Border_SelectionCard"), FVector2D(0.f, 0.f), FVector2D(1.f, 1.f),
		FMargin(24.f, 296.f, 24.f, 158.f));
	TestAnchoredRegion(TEXT("Border_GuideCard"), FVector2D(0.f, 1.f), FVector2D(1.f, 1.f),
		FMargin(24.f, -142.f, 24.f, 126.f));

	const UBorder* ViewportSurface =
		Cast<UBorder>(Tree->FindWidget(TEXT("Border_ViewportSurface")));
	TestTrue(TEXT("Pause Hub 전체 화면 표면은 월드가 비치지 않는 완전 불투명 색이다"),
		ViewportSurface
		&& ViewportSurface->Background.TintColor.GetSpecifiedColor().A >= 0.999f);

	TArray<UWidget*> AllWidgets;
	Tree->GetAllWidgets(AllWidgets);
	FString AllCopy;
	for (const UWidget* Widget : AllWidgets)
	{
		if (const UTextBlock* Text = Cast<UTextBlock>(Widget))
		{
			AllCopy += Text->GetText().ToString();
			AllCopy += TEXT("\n");
		}
	}
	for (const TCHAR* UnsupportedCopy : {
		TEXT("상점"), TEXT("Discord"), TEXT("서버"), TEXT("고객센터"),
		TEXT("계정 연동"), TEXT("교환 코드"), TEXT("채팅 번역") })
	{
		TestFalse(*FString::Printf(TEXT("프로젝트에 없는 가짜 기능을 노출하지 않는다: %s"), UnsupportedCopy),
			AllCopy.Contains(UnsupportedCopy));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
