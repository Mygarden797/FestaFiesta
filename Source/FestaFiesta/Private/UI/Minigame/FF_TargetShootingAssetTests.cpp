// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Components/Viewport.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Minigame/FF_MinigameDefinition.h"
#include "Minigame/FF_TargetShootingRuntime.h"
#include "Misc/AutomationTest.h"
#include "UI/Minigame/FF_TargetShootingScreen.h"
#include "UI/Minigame/FF_TargetShootingArena3D.h"
#include "UObject/UnrealType.h"
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
	const TCHAR* const ArenaMaterialObjectPath =
		TEXT("/Game/FestaFiesta/UI/Minigame/UIUX_v2/M_MG_TargetShooting_Arena3D."
			 "M_MG_TargetShooting_Arena3D");

	bool ContainsHangul(const FString& Text)
	{
		for (const TCHAR Character : Text)
		{
			if (Character >= 0xAC00 && Character <= 0xD7A3)
			{
				return true;
			}
		}
		return false;
	}

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
			{ TEXT("Image_Target"), UImage::StaticClass() },
			{ TEXT("Image_WaterBlaster"), UImage::StaticClass() },
			{ TEXT("Text_MuzzleFlash"), UTextBlock::StaticClass() },
			{ TEXT("Text_WeaponStatus"), UTextBlock::StaticClass() },
			{ TEXT("Text_TargetCue"), UTextBlock::StaticClass() },
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
	TestTrue(TEXT("기획자가 설정한 플레이 시간이 유효하다"),
		FMath::IsFinite(Definition->DurationSeconds) && Definition->DurationSeconds > 0.0f);
	TestTrue(TEXT("기획자가 설정한 최대 보상 점수가 유효하다"),
		Definition->ScoreForMaxReward > 0);
	TestTrue(TEXT("슈팅 에셋은 개별 클리어 골드 값을 명시한다"),
		Definition->RewardGoldAtMaxScore >= 0);
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

	struct FEditorMetadataExpectation
	{
		const UStruct* Owner;
		FName PropertyName;
		FString KoreanDisplayName;
		FString KoreanTooltipKeyword;
	};
	const FEditorMetadataExpectation MetadataExpectations[] = {
		{ UFF_MinigameDefinition::StaticClass(),
			GET_MEMBER_NAME_CHECKED(UFF_MinigameDefinition, DurationSeconds),
			TEXT("한 판 플레이 시간"), TEXT("전체 시간") },
		{ UFF_MinigameDefinition::StaticClass(),
			GET_MEMBER_NAME_CHECKED(UFF_MinigameDefinition, ScoreForMaxReward),
			TEXT("최대 보상 달성 점수"), TEXT("골드") },
		{ UFF_MinigameDefinition::StaticClass(),
			GET_MEMBER_NAME_CHECKED(UFF_MinigameDefinition, RewardGoldAtMaxScore),
			TEXT("클리어 최대 획득 골드"), TEXT("정상 완료") },
		{ FFF_TargetShootingRules::StaticStruct(),
			GET_MEMBER_NAME_CHECKED(FFF_TargetShootingRules, TargetLifetimeStartSeconds),
			TEXT("초반 과녁 유지 시간"), TEXT("놓친 과녁") },
		{ FFF_TargetShootingRules::StaticStruct(),
			GET_MEMBER_NAME_CHECKED(FFF_TargetShootingRules, HitBaseScore),
			TEXT("명중 기본 점수"), TEXT("기본 점수") },
	};
	for (const FEditorMetadataExpectation& Expected : MetadataExpectations)
	{
		const FProperty* Property = Expected.Owner
			? Expected.Owner->FindPropertyByName(Expected.PropertyName)
			: nullptr;
		if (!TestNotNull(
			*FString::Printf(TEXT("%s 에디터 속성이 존재한다"), *Expected.PropertyName.ToString()),
			Property))
		{
			continue;
		}
		TestEqual(
			*FString::Printf(TEXT("%s에 한국어 표시 이름이 있다"), *Expected.PropertyName.ToString()),
			Property->GetMetaData(TEXT("DisplayName")),
			Expected.KoreanDisplayName);
		TestTrue(
			*FString::Printf(TEXT("%s에 상세 한국어 마우스 오버 설명이 있다"), *Expected.PropertyName.ToString()),
			Property->GetMetaData(TEXT("ToolTip")).Contains(Expected.KoreanTooltipKeyword));
	}
	const FProperty* RulesProperty = UFF_TargetShootingDefinition::StaticClass()
		->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UFF_TargetShootingDefinition, Rules));
	if (TestNotNull(TEXT("Rules 구조체 에디터 속성이 존재한다"), RulesProperty))
	{
		TestTrue(TEXT("세부 규칙이 Data Asset 상세 패널에 바로 펼쳐진다"),
			RulesProperty->HasMetaData(TEXT("ShowOnlyInnerProperties")));
	}
	for (TFieldIterator<FProperty> PropertyIt(
		FFF_TargetShootingRules::StaticStruct(),
		EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt)
	{
		const FProperty* Property = *PropertyIt;
		if (!Property || !Property->HasAnyPropertyFlags(CPF_Edit))
		{
			continue;
		}
		TestTrue(
			*FString::Printf(TEXT("%s 표시 이름은 기획자용 한국어다"), *Property->GetName()),
			ContainsHangul(Property->GetMetaData(TEXT("DisplayName"))));
		TestTrue(
			*FString::Printf(TEXT("%s 마우스 오버 설명은 상세 한국어다"), *Property->GetName()),
			ContainsHangul(Property->GetMetaData(TEXT("ToolTip")))
				&& Property->GetMetaData(TEXT("ToolTip")).Len() >= 20);
	}
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
	const UFF_TargetShootingScreen* ScreenDefaults = Cast<UFF_TargetShootingScreen>(
		GeneratedClass->GetDefaultObject());
	if (TestNotNull(TEXT("WBP 기본값을 로드한다"), ScreenDefaults))
	{
		TestTrue(TEXT("전경 구조물이 없는 단일 ImageGen v4 배경을 사용한다"),
			ScreenDefaults->VisualStyle.BackdropTexture.ToSoftObjectPath().ToString()
				.Contains(TEXT("T_MG_TargetShooting_Backdrop_Single_ImageGen_v4")));
		TestTrue(TEXT("기본 화면은 3D 환경 박스 없이 단일 배경 모드를 사용한다"),
			ScreenDefaults->VisualStyle.bUseSingleBackdrop);
		TestTrue(TEXT("새 물총 전경 자산을 사용한다"),
			ScreenDefaults->VisualStyle.WaterBlasterTexture.ToSoftObjectPath().ToString()
				.Contains(TEXT("T_MG_TargetShooting_WaterBlaster_UIUX_v2")));
		TestTrue(TEXT("미니멀 원형 ImageGen 과녁을 사용한다"),
			ScreenDefaults->VisualStyle.TargetTexture.ToSoftObjectPath().ToString()
				.Contains(TEXT("T_MG_TargetShooting_Target_Simple_ImageGen_v1")));
		TestTrue(TEXT("3D 사격장 클래스는 디자이너가 교체할 수 있다"),
			ScreenDefaults->VisualStyle.Arena3DActorClass
			&& ScreenDefaults->VisualStyle.Arena3DActorClass->IsChildOf(
				AFF_TargetShootingArena3D::StaticClass()));
		TestTrue(TEXT("단일 배경을 꺼도 3D 과녁과 탄환 연출을 사용할 수 있다"),
			ScreenDefaults->VisualStyle.bRenderTargetIn3D
			&& ScreenDefaults->VisualStyle.bRenderProjectileIn3D);
		TestTrue(TEXT("물 탄환은 명확하게 보이는 크기를 사용한다"),
			ScreenDefaults->VisualStyle.ProjectileVisualSize >= 120.0f);
		TestTrue(TEXT("과녁 외곽선은 배경에서 분리되는 굵기를 사용한다"),
			ScreenDefaults->VisualStyle.TargetRingThickness >= 6.0f);
		TestTrue(TEXT("일반 과녁은 청록 배경과 반대되는 코랄색이다"),
			ScreenDefaults->VisualStyle.NormalTargetColor.R >= 0.9f
			&& ScreenDefaults->VisualStyle.NormalTargetColor.G <= 0.4f);
		TestTrue(TEXT("3D 사격장은 입체감을 만들되 과노출되지 않는 키라이트를 사용한다"),
			ScreenDefaults->VisualStyle.Arena3DKeyLightIntensity >= 0.5f
			&& ScreenDefaults->VisualStyle.Arena3DKeyLightIntensity <= 1.25f);
	}
	TestNotNull(TEXT("3D 사격장 전용 lit 재질이 로드된다"),
		LoadObject<UMaterialInterface>(nullptr, ArenaMaterialObjectPath));

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
	const UImage* TargetImage = Cast<UImage>(Tree->FindWidget(TEXT("Image_Target")));
	if (PlayArea && PlayCanvas && Target)
	{
		TestTrue(TEXT("Canvas_PlayArea는 Border_PlayArea의 직접 콘텐츠다"),
			PlayArea->GetContent() == PlayCanvas);
		TestTrue(TEXT("표적은 PlayArea canvas의 직접 자식이다"), Target->GetParent() == PlayCanvas);
		TestNotNull(TEXT("표적은 CanvasPanelSlot을 사용한다"), Cast<UCanvasPanelSlot>(Target->Slot));
		TestEqual(TEXT("표적은 클릭을 가로채지 않는다"),
			Target->GetVisibility(), ESlateVisibility::HitTestInvisible);
		TestTrue(TEXT("교체 가능한 과녁 이미지는 Border_Target 내부 Overlay에 있다"),
			TargetImage
			&& TargetImage->GetParent() == Target->GetContent()
			&& Target->GetContent()->IsA(UOverlay::StaticClass()));
	}
	for (const FName PresentationWidgetName : {
		FName(TEXT("Image_WaterBlaster")),
		FName(TEXT("Text_MuzzleFlash")),
		FName(TEXT("Text_WeaponStatus")),
		FName(TEXT("Text_TargetCue")),
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
	TestNotNull(TEXT("네이티브 폴백에 실시간 3D 사격장 viewport가 있다"),
		Screen->GetWidgetFromName(TEXT("Viewport_Arena3D")));
	TestNotNull(TEXT("네이티브 폴백에 표적이 있다"),
		Screen->GetWidgetFromName(TEXT("Border_Target")));
	TestNotNull(TEXT("네이티브 폴백에 교체 가능한 과녁 이미지가 있다"),
		Screen->GetWidgetFromName(TEXT("Image_Target")));
	TestNotNull(TEXT("네이티브 폴백에 점수 팝업 레이어가 있다"),
		Screen->GetWidgetFromName(TEXT("Text_ShotFeedback")));
	TestNotNull(TEXT("네이티브 폴백에 커스텀 조준선이 있다"),
		Screen->GetWidgetFromName(TEXT("Text_Reticle")));
	TestNotNull(TEXT("네이티브 폴백에 총알 트레이서가 있다"),
		Screen->GetWidgetFromName(TEXT("ProjectileVisual_Tracer")));
	TestNotNull(TEXT("네이티브 폴백에 물총이 있다"),
		Screen->GetWidgetFromName(TEXT("Image_WaterBlaster")));
	TestNotNull(TEXT("네이티브 폴백에 총구 섬광이 있다"),
		Screen->GetWidgetFromName(TEXT("Text_MuzzleFlash")));
	TestNotNull(TEXT("네이티브 폴백에 발사 상태 안내가 있다"),
		Screen->GetWidgetFromName(TEXT("Text_WeaponStatus")));
	TestNotNull(TEXT("네이티브 폴백에 과녁 위치 안내가 있다"),
		Screen->GetWidgetFromName(TEXT("Text_TargetCue")));
	TestNotNull(TEXT("네이티브 폴백에 종료 버튼이 있다"),
		Screen->GetWidgetFromName(TEXT("Button_Close")));
	const UCanvasPanel* NativePlayCanvas = Cast<UCanvasPanel>(
		Screen->GetWidgetFromName(TEXT("Canvas_PlayArea")));
	for (const FName PresentationWidgetName : {
		FName(TEXT("Viewport_Arena3D")),
		FName(TEXT("Image_WaterBlaster")),
		FName(TEXT("Text_MuzzleFlash")),
		FName(TEXT("Text_WeaponStatus")),
		FName(TEXT("Text_TargetCue")),
		FName(TEXT("Text_ShotFeedback")),
		FName(TEXT("Text_Reticle")),
		FName(TEXT("ProjectileVisual_Tracer")) })
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
	Snapshot.ProjectileSerial = 1;
	Snapshot.bProjectileInFlight = true;
	Snapshot.ProjectileTargetSerial = 7;
	Snapshot.ProjectileStartNormalized = FVector2D(0.5f, 0.68f);
	Snapshot.ProjectileAimPointNormalized = FVector2D(0.3f, 0.7f);
	Snapshot.ProjectilePositionNormalized = FVector2D(0.4f, 0.83f);
	Snapshot.ProjectileTravelSeconds = 0.12f;
	Snapshot.ProjectileRemainingSeconds = 0.06f;
	Snapshot.HitCount = 8;
	Snapshot.MissCount = 2;
	Snapshot.EscapedTargetCount = 1;
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
	const bool bUsingThreeDimensionalArena = Screen->IsThreeDimensionalArenaReady();
	const UViewport* ArenaViewport = Cast<UViewport>(
		Screen->GetWidgetFromName(TEXT("Viewport_Arena3D")));
	TestNotNull(TEXT("3D 사격장 viewport를 찾을 수 있다"), ArenaViewport);
	if (ArenaViewport)
	{
		TestEqual(TEXT("3D 장면이 준비되면 사격장 viewport가 표시된다"),
			ArenaViewport->GetVisibility(),
			bUsingThreeDimensionalArena
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}
	TestTrue(TEXT("3D 좌표 변환은 화면 중앙을 사격장 중앙에 배치한다"),
		AFF_TargetShootingArena3D::NormalizedToTargetPlane(FVector2D(0.5f, 0.5f))
			.Equals(FVector(440.0f, 0.0f, 190.0f)));
	TestEqual(TEXT("FPS 카메라 조준은 세션 시작 시 과녁 평면 중앙이다"),
		Screen->GetCameraAimPointNormalized(), FVector2D(0.5f, 0.5f));
	const UTextBlock* FixedReticle = Cast<UTextBlock>(
		Screen->GetWidgetFromName(TEXT("Text_Reticle")));
	TestNotNull(TEXT("커스텀 조준점이 있다"), FixedReticle);
	if (const UCanvasPanelSlot* ReticleSlot = FixedReticle
		? Cast<UCanvasPanelSlot>(FixedReticle->Slot)
		: nullptr)
	{
		TestEqual(TEXT("조준점 앵커는 세션 시작 시 화면 중앙에서 시작한다"),
			ReticleSlot->GetAnchors().Minimum, FVector2D(0.5f, 0.5f));
	}

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
		TestEqual(TEXT("3D 배경 위에서도 권위 2D 과녁이 항상 표시된다"),
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
		TestTrue(TEXT("2D 과녁의 수명 드레인이 snapshot과 일치한다"),
			FMath::IsNearlyEqual(
				TargetVisual->GetRemainingLifeRatio(),
				Snapshot.TargetRemainingSeconds / Snapshot.TargetLifetimeSeconds));
	}
	const UTextBlock* TargetCue = Cast<UTextBlock>(
		Screen->GetWidgetFromName(TEXT("Text_TargetCue")));
	TestNotNull(TEXT("과녁 안내 문구를 찾을 수 있다"), TargetCue);
	if (TargetCue)
	{
		TestEqual(TEXT("플레이 중에는 과녁 안내가 표시된다"),
			TargetCue->GetVisibility(), ESlateVisibility::HitTestInvisible);
		TestTrue(TEXT("보너스 과녁을 문구로도 구분한다"),
			TargetCue->GetText().ToString().Contains(TEXT("보너스 과녁")));
	}
	const UFF_TargetShootingProjectileVisual* ProjectileVisual =
		Cast<UFF_TargetShootingProjectileVisual>(
			Screen->GetWidgetFromName(TEXT("ProjectileVisual_Tracer")));
	TestNotNull(TEXT("플레이 캔버스에 총알 트레이서가 생성된다"), ProjectileVisual);
	if (ProjectileVisual)
	{
		TestEqual(TEXT("물 탄환은 과녁과 같은 2D 판정 캔버스에 표시된다"),
			ProjectileVisual->GetVisibility(), ESlateVisibility::HitTestInvisible);
		TestTrue(TEXT("총알은 VisualStyle의 코어 색을 사용한다"),
			ProjectileVisual->GetCoreColor().Equals(
				Screen->VisualStyle.ProjectileCoreColor));
		TestTrue(TEXT("총알 트레이서는 플레이 캔버스의 직접 자식이다"),
			ProjectileVisual->GetParent() == NativePlayCanvas);
		TestNotNull(TEXT("총알 트레이서는 CanvasPanelSlot을 사용한다"),
			Cast<UCanvasPanelSlot>(ProjectileVisual->Slot));
	}
	const UImage* WaterBlaster = Cast<UImage>(
		Screen->GetWidgetFromName(TEXT("Image_WaterBlaster")));
	TestNotNull(TEXT("플레이 캔버스에 물총 스프라이트가 생성된다"), WaterBlaster);
	if (WaterBlaster)
	{
		TestEqual(TEXT("3D 사격장 위에 고품질 1인칭 물총 전경을 합성한다"),
			WaterBlaster->GetVisibility(), ESlateVisibility::HitTestInvisible);
		TestTrue(TEXT("물총은 플레이 캔버스의 직접 자식이다"),
			WaterBlaster->GetParent() == NativePlayCanvas);
	}
	const UTextBlock* MuzzleFlash = Cast<UTextBlock>(
		Screen->GetWidgetFromName(TEXT("Text_MuzzleFlash")));
	TestNotNull(TEXT("발사 시 총구 섬광을 표시한다"), MuzzleFlash);
	if (MuzzleFlash)
	{
		TestEqual(TEXT("발사 순간 물총 전경 위에 총구 섬광을 합성한다"),
			MuzzleFlash->GetVisibility(), ESlateVisibility::HitTestInvisible);
	}
	const UTextBlock* WeaponStatus = Cast<UTextBlock>(
		Screen->GetWidgetFromName(TEXT("Text_WeaponStatus")));
	TestNotNull(TEXT("발사 상태 안내를 표시한다"), WeaponStatus);
	if (WeaponStatus)
	{
		TestTrue(TEXT("비행 중인 탄환 상태가 안내된다"),
			WeaponStatus->GetText().ToString().Contains(TEXT("비행 중")));
	}

	const UTextBlock* FeedbackText = Cast<UTextBlock>(
		Screen->GetWidgetFromName(TEXT("Text_ShotFeedback")));
	TestNotNull(TEXT("점수 팝업 텍스트를 찾을 수 있다"), FeedbackText);
	if (FeedbackText)
	{
		TestEqual(TEXT("새 피드백 event는 즉시 표시된다"),
			FeedbackText->GetVisibility(), ESlateVisibility::HitTestInvisible);
		TestTrue(TEXT("황금 정중앙 점수 팝업이 명시적이다"),
			FeedbackText->GetText().ToString().Contains(TEXT("황금 정중앙"))
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
	if (ProjectileVisual)
	{
		TestEqual(TEXT("runtime 해제 시 비행 중이던 총알을 숨긴다"),
			ProjectileVisual->GetVisibility(), ESlateVisibility::Collapsed);
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
