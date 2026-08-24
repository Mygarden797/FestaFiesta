// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Minigame/FF_MinigameDefinition.h"
#include "Minigame/FF_TargetShootingRuntime.h"
#include "Misc/AutomationTest.h"
#include "UI/Minigame/FF_TargetShootingScreen.h"
#include "WidgetBlueprint.h"

namespace FFTargetShootingAssetTestHelpers
{
	const TCHAR* const DefinitionObjectPath =
		TEXT("/Game/FestaFiesta/Minigame/Data/DA_Minigame_TargetShooting."
			 "DA_Minigame_TargetShooting");
	const TCHAR* const ScreenObjectPath =
		TEXT("/Game/FestaFiesta/UI/Minigame/WBP_FF_TargetShooting."
			 "WBP_FF_TargetShooting");
	const TCHAR* const ScreenClassPath =
		TEXT("/Game/FestaFiesta/UI/Minigame/WBP_FF_TargetShooting."
			 "WBP_FF_TargetShooting_C");

	struct FRequiredWidget
	{
		FName Name;
		UClass* WidgetClass;
	};

	const TArray<FRequiredWidget>& GetRequiredWidgets()
	{
		static const TArray<FRequiredWidget> Widgets = {
			{ TEXT("Border_PlayArea"), UBorder::StaticClass() },
			{ TEXT("Canvas_PlayArea"), UCanvasPanel::StaticClass() },
			{ TEXT("Border_Target"), UBorder::StaticClass() },
			{ TEXT("Text_RemainingTime"), UTextBlock::StaticClass() },
			{ TEXT("Text_Score"), UTextBlock::StaticClass() },
			{ TEXT("Text_HitCount"), UTextBlock::StaticClass() },
			{ TEXT("Text_MissCount"), UTextBlock::StaticClass() },
			{ TEXT("Text_Streak"), UTextBlock::StaticClass() },
			{ TEXT("Panel_Result"), UBorder::StaticClass() },
			{ TEXT("Text_Result"), UTextBlock::StaticClass() },
			{ TEXT("Button_Close"), UButton::StaticClass() },
		};
		return Widgets;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_TargetShootingDefinitionAssetContractTest,
	"FestaFiesta.Minigame.TargetShooting.Content.DefinitionAssetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_TargetShootingDefinitionAssetContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFTargetShootingAssetTestHelpers;

	UFF_TargetShootingDefinition* Definition =
		LoadObject<UFF_TargetShootingDefinition>(nullptr, DefinitionObjectPath);
	if (!TestNotNull(TEXT("타깃 슈팅 Definition 에셋이 로드된다"), Definition))
	{
		return false;
	}

	TestEqual(TEXT("PrimaryAssetType은 FFMinigameDefinition이다"),
		Definition->GetPrimaryAssetId().PrimaryAssetType,
		FPrimaryAssetType(TEXT("FFMinigameDefinition")));
	TestEqual(TEXT("PrimaryAssetName은 TargetShooting이다"),
		Definition->GetPrimaryAssetId().PrimaryAssetName,
		FName(TEXT("TargetShooting")));
	TestEqual(TEXT("미니게임 ID가 고정된다"), Definition->MinigameId, FName(TEXT("TargetShooting")));
	TestFalse(TEXT("디자이너가 수정할 수 있는 사용자 표시 이름이 비어 있지 않다"),
		Definition->DisplayName.IsEmpty());
	TestTrue(TEXT("사격 시설과 연결된다"),
		Definition->CompatibleFacilityDefinitionIds.Contains(TEXT("Booth_ActivityGame_Shoot")));
	TestEqual(TEXT("플레이 시간은 30초다"), Definition->DurationSeconds, 30.0f);
	TestEqual(TEXT("최대 보상 점수는 2000이다"), Definition->ScoreForMaxReward, 2000);
	TestEqual(TEXT("타깃 슈팅 runtime 클래스를 사용한다"),
		Definition->RuntimeClass.Get(), UFF_TargetShootingRuntime::StaticClass());

	UClass* ScreenClass = Definition->ScreenClass.LoadSynchronous();
	TestNotNull(TEXT("화면 soft class가 로드된다"), ScreenClass);
	if (ScreenClass)
	{
		TestTrue(TEXT("화면은 TargetShootingScreen 파생 클래스다"),
			ScreenClass->IsChildOf(UFF_TargetShootingScreen::StaticClass()));
		TestEqual(TEXT("Definition은 생성된 디자이너 WBP를 가리킨다"),
			ScreenClass->GetPathName(), FString(ScreenClassPath));
	}

	FString RulesFailure;
	TestTrue(TEXT("타깃 슈팅 규칙이 유효하다"), Definition->Rules.IsValid(&RulesFailure));
	if (!RulesFailure.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("Rules validation detail: %s"), *RulesFailure));
	}

	TArray<FText> DefinitionErrors;
	TestTrue(TEXT("전체 Definition 계약이 유효하다"),
		Definition->IsDefinitionValid(DefinitionErrors));
	TestEqual(TEXT("Definition 검증 오류가 없다"), DefinitionErrors.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_TargetShootingWidgetAssetContractTest,
	"FestaFiesta.Minigame.TargetShooting.UI.WidgetAssetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_TargetShootingWidgetAssetContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFTargetShootingAssetTestHelpers;

	UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, ScreenObjectPath);
	if (!TestNotNull(TEXT("타깃 슈팅 WBP가 로드된다"), Blueprint))
	{
		return false;
	}

	UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(Blueprint->GeneratedClass);
	if (!TestNotNull(TEXT("WBP 생성 클래스가 존재한다"), GeneratedClass))
	{
		return false;
	}
	TestTrue(TEXT("WBP는 TargetShootingScreen을 상속한다"),
		GeneratedClass->IsChildOf(UFF_TargetShootingScreen::StaticClass()));

	const UWidgetTree* Tree = GeneratedClass->GetWidgetTreeArchetype();
	if (!TestNotNull(TEXT("컴파일된 WidgetTree가 존재한다"), Tree))
	{
		return false;
	}

	for (const FRequiredWidget& Required : GetRequiredWidgets())
	{
		UWidget* Widget = Tree->FindWidget(Required.Name);
		TestNotNull(*FString::Printf(TEXT("필수 위젯 %s가 존재한다"), *Required.Name.ToString()), Widget);
		if (Widget)
		{
			TestTrue(*FString::Printf(TEXT("%s 타입이 계약과 같다"), *Required.Name.ToString()),
				Widget->IsA(Required.WidgetClass));
			TestTrue(*FString::Printf(TEXT("%s가 Blueprint 변수다"), *Required.Name.ToString()),
				Widget->bIsVariable);
			TestTrue(*FString::Printf(TEXT("%s 변수 GUID가 저장됐다"), *Required.Name.ToString()),
				Blueprint->WidgetVariableNameToGuidMap.Contains(Required.Name));
		}
	}

	const UBorder* PlayArea = Cast<UBorder>(Tree->FindWidget(TEXT("Border_PlayArea")));
	const UCanvasPanel* PlayCanvas = Cast<UCanvasPanel>(Tree->FindWidget(TEXT("Canvas_PlayArea")));
	const UBorder* Target = Cast<UBorder>(Tree->FindWidget(TEXT("Border_Target")));
	if (PlayArea && PlayCanvas && Target)
	{
		TestTrue(TEXT("Canvas_PlayArea는 Border_PlayArea의 직접 콘텐츠다"),
			PlayArea->GetContent() == PlayCanvas);
		TestTrue(TEXT("표적은 PlayArea canvas의 직접 자식이다"), Target->GetParent() == PlayCanvas);
		TestNotNull(TEXT("표적은 CanvasPanelSlot을 사용한다"), Cast<UCanvasPanelSlot>(Target->Slot));
		TestEqual(TEXT("표적은 클릭을 가로채지 않는다"),
			Target->GetVisibility(), ESlateVisibility::HitTestInvisible);
	}
	for (const FName PresentationWidgetName : {
		FName(TEXT("Text_ShotFeedback")),
		FName(TEXT("Text_Reticle")) })
	{
		if (const UWidget* PresentationWidget = Tree->FindWidget(PresentationWidgetName))
		{
			TestTrue(
				*FString::Printf(TEXT("%s는 PlayArea canvas의 직접 자식이다"), *PresentationWidgetName.ToString()),
				PresentationWidget->GetParent() == PlayCanvas);
			TestNotNull(
				*FString::Printf(TEXT("%s는 CanvasPanelSlot을 사용한다"), *PresentationWidgetName.ToString()),
				Cast<UCanvasPanelSlot>(PresentationWidget->Slot));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_TargetShootingNativeFallbackContractTest,
	"FestaFiesta.Minigame.TargetShooting.UI.NativeFallbackContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_TargetShootingNativeFallbackContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("폴백 UI 계약 검증용 EditorWorld가 존재한다"), EditorWorld))
	{
		return false;
	}

	UFF_TargetShootingScreen* Screen = CreateWidget<UFF_TargetShootingScreen>(
		EditorWorld,
		UFF_TargetShootingScreen::StaticClass());
	if (!TestNotNull(TEXT("저장된 WBP 없이 네이티브 화면을 생성한다"), Screen))
	{
		return false;
	}

	const TSharedRef<SWidget> Slate = Screen->TakeWidget();
	(void)Slate;
	TestFalse(TEXT("디자이너 트리가 아니면 네이티브 폴백을 사용한다"),
		Screen->IsUsingDesignerWidgetTree());
	TestNotNull(TEXT("네이티브 폴백에 플레이 영역이 있다"),
		Screen->GetWidgetFromName(TEXT("Border_PlayArea")));
	TestNotNull(TEXT("네이티브 폴백에 표적이 있다"),
		Screen->GetWidgetFromName(TEXT("Border_Target")));
	TestNotNull(TEXT("네이티브 폴백에 점수 팝업 레이어가 있다"),
		Screen->GetWidgetFromName(TEXT("Text_ShotFeedback")));
	TestNotNull(TEXT("네이티브 폴백에 커스텀 조준선이 있다"),
		Screen->GetWidgetFromName(TEXT("Text_Reticle")));
	TestNotNull(TEXT("네이티브 폴백에 종료 버튼이 있다"),
		Screen->GetWidgetFromName(TEXT("Button_Close")));
	const UCanvasPanel* NativePlayCanvas = Cast<UCanvasPanel>(
		Screen->GetWidgetFromName(TEXT("Canvas_PlayArea")));
	for (const FName PresentationWidgetName : {
		FName(TEXT("Text_ShotFeedback")),
		FName(TEXT("Text_Reticle")) })
	{
		const UWidget* PresentationWidget = Screen->GetWidgetFromName(PresentationWidgetName);
		TestTrue(
			*FString::Printf(TEXT("네이티브 %s는 PlayArea canvas의 직접 자식이다"), *PresentationWidgetName.ToString()),
			PresentationWidget && PresentationWidget->GetParent() == NativePlayCanvas);
		TestNotNull(
			*FString::Printf(TEXT("네이티브 %s는 CanvasPanelSlot을 사용한다"), *PresentationWidgetName.ToString()),
			PresentationWidget ? Cast<UCanvasPanelSlot>(PresentationWidget->Slot) : nullptr);
	}

	FFF_TargetShootingSnapshot Snapshot;
	Snapshot.Session.Context.SessionId = FGuid::NewGuid();
	Snapshot.Session.Context.MinigameId = TEXT("TargetShooting");
	Snapshot.Session.State = EFF_MinigameSessionState::Playing;
	Snapshot.Session.RemainingSeconds = 12.25f;
	Snapshot.Session.Score = 750;
	Snapshot.TargetSerial = 7;
	Snapshot.bTargetVisible = true;
	Snapshot.TargetCenterNormalized = FVector2D(0.3f, 0.7f);
	Snapshot.TargetRadiusNormalized = FVector2D(0.04f, 0.07f);
	Snapshot.TargetLifetimeSeconds = 0.9f;
	Snapshot.TargetRemainingSeconds = 0.5f;
	Snapshot.TargetAgeSeconds = 0.4f;
	Snapshot.bBonusTarget = true;
	Snapshot.HitCount = 8;
	Snapshot.MissCount = 2;
	Snapshot.CurrentCombo = 3;
	Snapshot.BestCombo = 5;
	Snapshot.LatestFeedbackSerial = 1;
	Snapshot.LatestFeedbackType = EFF_TargetShootingFeedbackType::Bullseye;
	Snapshot.LatestFeedbackPointNormalized = FVector2D(0.3f, 0.7f);
	Snapshot.LatestScoreDelta = 250;
	Snapshot.LatestReactionSeconds = 0.18f;
	Snapshot.bLatestFeedbackWasBonusTarget = true;
	Snapshot.AccuracyPercent = 80.0f;
	Snapshot.HitsPerSecond = 2.5f;
	Snapshot.AverageHitReactionSeconds = 0.24f;
	Screen->ApplySnapshot(Snapshot);

	const FFF_TargetShootingSnapshot& Displayed = Screen->GetDisplayedSnapshot();
	TestEqual(TEXT("네이티브 폴백이 표적 serial을 표시 상태에 보존한다"),
		Displayed.TargetSerial, 7);
	TestEqual(TEXT("네이티브 폴백이 점수를 표시 상태에 보존한다"),
		Displayed.Session.Score, 750);
	TestEqual(TEXT("네이티브 폴백이 정규화된 표적 위치를 보존한다"),
		Displayed.TargetCenterNormalized, FVector2D(0.3f, 0.7f));

	const UBorder* Target = Cast<UBorder>(Screen->GetWidgetFromName(TEXT("Border_Target")));
	TestNotNull(TEXT("플레이 중에는 표적 위젯을 찾을 수 있다"), Target);
	if (Target)
	{
		TestEqual(TEXT("플레이 중인 표적은 클릭을 가로채지 않는다"),
			Target->GetVisibility(), ESlateVisibility::HitTestInvisible);
		TestTrue(TEXT("황금 표적은 VisualStyle 보너스 색을 사용한다"),
			Target->GetBrushColor().Equals(Screen->VisualStyle.BonusTargetColor));
		TestTrue(TEXT("표적 본체는 시각 크기와 판정 타원을 어긋나게 확대하지 않는다"),
			Target->GetRenderTransform().Scale.Equals(FVector2D(1.0f, 1.0f)));
	}
	const UFF_TargetShootingTargetVisual* TargetVisual =
		Cast<UFF_TargetShootingTargetVisual>(
			Screen->GetWidgetFromName(TEXT("TargetVisual_AuthoritativeRings")));
	TestNotNull(TEXT("사각형 모서리 오판정을 없애는 권위 타원 링이 있다"), TargetVisual);
	if (TargetVisual)
	{
		TestTrue(TEXT("권위 타원 링도 황금 표적 색을 사용한다"),
			TargetVisual->GetOuterColor().Equals(Screen->VisualStyle.BonusTargetColor));
	}

	const UTextBlock* FeedbackText = Cast<UTextBlock>(
		Screen->GetWidgetFromName(TEXT("Text_ShotFeedback")));
	TestNotNull(TEXT("점수 팝업 텍스트를 찾을 수 있다"), FeedbackText);
	if (FeedbackText)
	{
		TestEqual(TEXT("새 피드백 event는 즉시 표시된다"),
			FeedbackText->GetVisibility(), ESlateVisibility::HitTestInvisible);
		TestTrue(TEXT("황금 불스아이 점수 팝업이 명시적이다"),
			FeedbackText->GetText().ToString().Contains(TEXT("GOLD BULLSEYE"))
			&& FeedbackText->GetText().ToString().Contains(TEXT("250")));
	}

	Screen->SetSessionId(Snapshot.Session.Context.SessionId);
	TestFalse(TEXT("runtime 해제 요청은 false를 반환한다"), Screen->SetRuntime(nullptr));
	TestFalse(TEXT("runtime 해제 시 재사용 화면의 session id를 비운다"),
		Screen->GetSessionId().IsValid());
	TestEqual(TEXT("runtime 해제 시 표시 snapshot을 초기화한다"),
		Screen->GetDisplayedSnapshot().TargetSerial, 0);
	if (FeedbackText)
	{
		TestEqual(TEXT("runtime 해제 시 이전 점수 팝업을 숨긴다"),
			FeedbackText->GetVisibility(), ESlateVisibility::Collapsed);
	}
	Screen->ApplySnapshot(Snapshot);
	if (FeedbackText)
	{
		TestEqual(TEXT("같은 serial도 새 표시 세션에서 다시 피드백한다"),
			FeedbackText->GetVisibility(), ESlateVisibility::HitTestInvisible);
	}

	const UTextBlock* ScoreText = Cast<UTextBlock>(
		Screen->GetWidgetFromName(TEXT("Text_Score")));
	TestNotNull(TEXT("네이티브 폴백 점수 텍스트가 있다"), ScoreText);
	if (ScoreText)
	{
		TestTrue(TEXT("네이티브 폴백에 실제 점수가 표시된다"),
			ScoreText->GetText().ToString().Contains(TEXT("750")));
	}
	const UTextBlock* AccuracyText = Cast<UTextBlock>(
		Screen->GetWidgetFromName(TEXT("Text_HitCount")));
	const UTextBlock* PerformanceText = Cast<UTextBlock>(
		Screen->GetWidgetFromName(TEXT("Text_MissCount")));
	TestTrue(TEXT("HUD가 명중률을 표시한다"),
		AccuracyText && AccuracyText->GetText().ToString().Contains(TEXT("80")));
	TestTrue(TEXT("HUD가 TPS와 평균 반응시간을 표시한다"),
		PerformanceText
		&& PerformanceText->GetText().ToString().Contains(TEXT("2.5"))
		&& PerformanceText->GetText().ToString().Contains(TEXT("240")));

	Snapshot.Session.State = EFF_MinigameSessionState::Completed;
	Snapshot.Session.RemainingSeconds = 0.0f;
	Snapshot.bTargetVisible = false;
	Screen->ApplySnapshot(Snapshot);
	const UBorder* ResultPanel = Cast<UBorder>(
		Screen->GetWidgetFromName(TEXT("Panel_Result")));
	TestNotNull(TEXT("네이티브 폴백 결과 패널이 있다"), ResultPanel);
	if (ResultPanel)
	{
		TestEqual(TEXT("세션 완료 후 네이티브 결과 패널을 보여 준다"),
			ResultPanel->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	}

	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
