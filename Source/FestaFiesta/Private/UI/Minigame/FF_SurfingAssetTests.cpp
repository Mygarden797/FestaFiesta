// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Minigame/FF_MinigameDefinition.h"
#include "Minigame/FF_SurfingRuntime.h"
#include "Minigame/Surfing/FF_SurfingTypes.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "TextureResource.h"
#include "UI/Minigame/FF_SurfingCharacterActor.h"
#include "UI/Minigame/FF_SurfingScreen.h"
#include "UI/Minigame/FF_SurfingSceneActor.h"
#include "WidgetBlueprint.h"

namespace FFSurfingAssetTestHelpers
{
	const FString LegacyScreenPackage(
		TEXT("/Game/FestaFiesta/UI/Minigame/WBP_FF_Surfing"));
	const FString EdgeScreenPackage(
		TEXT("/Game/FestaFiesta/UI/Minigame/WBP_FF_Surfing_Edge"));
	const FString FreeRideScreenPackage(
		TEXT("/Game/FestaFiesta/UI/Minigame/WBP_FF_Surfing_FreeRide"));
	const FString CourseTablePackage(
		TEXT("/Game/FestaFiesta/Minigame/Data/DT_SurfingCourse"));
	const FString DefinitionPackage(
		TEXT("/Game/FestaFiesta/Minigame/Data/DA_Minigame_Surfing"));

	const TCHAR* const DefinitionObjectPath =
		TEXT("/Game/FestaFiesta/Minigame/Data/DA_Minigame_Surfing."
			 "DA_Minigame_Surfing");
	const TCHAR* const FreeRideScreenObjectPath =
		TEXT("/Game/FestaFiesta/UI/Minigame/WBP_FF_Surfing_FreeRide."
			 "WBP_FF_Surfing_FreeRide");
	const TCHAR* const FreeRideScreenClassPath =
		TEXT("/Game/FestaFiesta/UI/Minigame/WBP_FF_Surfing_FreeRide."
			 "WBP_FF_Surfing_FreeRide_C");
	const TCHAR* const CourseTableObjectPath =
		TEXT("/Game/FestaFiesta/Minigame/Data/DT_SurfingCourse.DT_SurfingCourse");
	const TCHAR* const FinalBackdropObjectPath =
		TEXT("/Game/FestaFiesta/UI/Minigame/Final/T_MG_Surfing_Backdrop_Final."
			 "T_MG_Surfing_Backdrop_Final");
	const TCHAR* const FinalSurferObjectPath =
		TEXT("/Game/FestaFiesta/UI/Minigame/Final/T_MG_Surfing_Surfer_Final."
			 "T_MG_Surfing_Surfer_Final");

	struct FRequiredWidget
	{
		FName Name;
		UClass* WidgetClass;
	};

	const TArray<FRequiredWidget>& GetRequiredWidgets()
	{
		static const TArray<FRequiredWidget> Widgets = {
			{ TEXT("Canvas_SurfArea"), UCanvasPanel::StaticClass() },
			{ TEXT("Canvas_EntityLayer"), UCanvasPanel::StaticClass() },
			{ TEXT("Border_SurferAvatar"), UBorder::StaticClass() },
			{ TEXT("Text_RemainingTime"), UTextBlock::StaticClass() },
			{ TEXT("Text_Score"), UTextBlock::StaticClass() },
			{ TEXT("Text_Lives"), UTextBlock::StaticClass() },
			{ TEXT("Text_Boost"), UTextBlock::StaticClass() },
			{ TEXT("Text_Controls"), UTextBlock::StaticClass() },
			{ TEXT("Panel_Result"), UBorder::StaticClass() },
			{ TEXT("Text_Result"), UTextBlock::StaticClass() },
			{ TEXT("Button_ResultClose"), UButton::StaticClass() },
			{ TEXT("Button_Close"), UButton::StaticClass() },
		};
		return Widgets;
	}

	const TArray<FName>& GetRemovedEdgeWidgetNames()
	{
		static const TArray<FName> Names = {
			TEXT("Text_Distance"),
			TEXT("Text_Status"),
			TEXT("Button_SteerLeft"),
			TEXT("Button_Boost"),
			TEXT("Button_SteerRight"),
			TEXT("Button_MoveUp"),
			TEXT("Button_MoveDown"),
		};
		return Names;
	}

	bool IsFullStretchCanvasSlot(const UCanvasPanelSlot* Slot)
	{
		if (!Slot)
		{
			return false;
		}

		const FAnchors Anchors = Slot->GetAnchors();
		const FMargin Offsets = Slot->GetOffsets();
		return Anchors.Minimum.Equals(FVector2D::ZeroVector)
			&& Anchors.Maximum.Equals(FVector2D(1.0f, 1.0f))
			&& FMath::IsNearlyZero(Offsets.Left)
			&& FMath::IsNearlyZero(Offsets.Top)
			&& FMath::IsNearlyZero(Offsets.Right)
			&& FMath::IsNearlyZero(Offsets.Bottom);
	}

	bool TestArenaTree(
		FAutomationTestBase& Test,
		const UWidgetTree& Tree,
		const bool bCheckInitialResultVisibility)
	{
		const UCanvasPanel* SurfArea = Cast<UCanvasPanel>(
			Tree.FindWidget(TEXT("Canvas_SurfArea")));
		const UCanvasPanel* EntityLayer = Cast<UCanvasPanel>(
			Tree.FindWidget(TEXT("Canvas_EntityLayer")));
		const UBorder* SurferAvatar = Cast<UBorder>(
			Tree.FindWidget(TEXT("Border_SurferAvatar")));
		if (!SurfArea || !EntityLayer || !SurferAvatar)
		{
			Test.AddError(TEXT("FreeRide arena 계층을 검사하는 데 필요한 위젯이 없다."));
			return false;
		}

		Test.TestEqual(TEXT("SurfArea는 arena 경계에서 entity를 자른다"),
			SurfArea->GetClipping(), EWidgetClipping::ClipToBoundsAlways);
		Test.TestTrue(TEXT("EntityLayer는 SurfArea의 직접 자식이다"),
			EntityLayer->GetParent() == SurfArea);
		Test.TestEqual(TEXT("EntityLayer는 입력을 가로채지 않는다"),
			EntityLayer->GetVisibility(), ESlateVisibility::HitTestInvisible);

		const UCanvasPanelSlot* EntitySlot = Cast<UCanvasPanelSlot>(EntityLayer->Slot);
		Test.TestNotNull(TEXT("EntityLayer는 CanvasPanelSlot을 사용한다"), EntitySlot);
		Test.TestTrue(TEXT("EntityLayer는 SurfArea 전체를 채운다"),
			IsFullStretchCanvasSlot(EntitySlot));

		const UCanvasPanelSlot* AvatarSlot = Cast<UCanvasPanelSlot>(SurferAvatar->Slot);
		Test.TestNotNull(TEXT("서퍼 아바타는 CanvasPanelSlot을 사용한다"), AvatarSlot);
		Test.TestEqual(TEXT("서퍼 아바타는 입력을 가로채지 않는다"),
			SurferAvatar->GetVisibility(), ESlateVisibility::HitTestInvisible);
		const bool bAvatarInSurfArea = SurferAvatar->GetParent() == SurfArea;
		const bool bAvatarInEntityLayer = SurferAvatar->GetParent() == EntityLayer;
		Test.TestTrue(TEXT("서퍼 아바타는 SurfArea 또는 EntityLayer의 직접 자식이다"),
			bAvatarInSurfArea || bAvatarInEntityLayer);
		if (AvatarSlot && EntitySlot && bAvatarInSurfArea)
		{
			Test.TestTrue(TEXT("서퍼 아바타 ZOrder는 고정 entity 레이어보다 높다"),
				AvatarSlot->GetZOrder() > EntitySlot->GetZOrder());
		}
		else if (AvatarSlot && bAvatarInEntityLayer)
		{
			Test.TestTrue(TEXT("EntityLayer 내부 서퍼는 고정 entity보다 높은 ZOrder다"),
				AvatarSlot->GetZOrder() > 0);
		}

		if (bCheckInitialResultVisibility)
		{
			const UBorder* ResultPanel = Cast<UBorder>(Tree.FindWidget(TEXT("Panel_Result")));
			if (Test.TestNotNull(TEXT("결과 패널이 존재한다"), ResultPanel))
			{
				Test.TestEqual(TEXT("결과 패널은 처음에 숨겨져 있다"),
					ResultPanel->GetVisibility(), ESlateVisibility::Collapsed);
			}
		}
		return !Test.HasAnyErrors();
	}

	bool TestDesignerHierarchy(FAutomationTestBase& Test, const UWidgetTree& Tree)
	{
		const UCanvasPanel* SurfArea = Cast<UCanvasPanel>(
			Tree.FindWidget(TEXT("Canvas_SurfArea")));
		const UCanvasPanel* EntityLayer = Cast<UCanvasPanel>(
			Tree.FindWidget(TEXT("Canvas_EntityLayer")));
		const UBorder* PillHud = Cast<UBorder>(Tree.FindWidget(TEXT("Border_PillHud")));
		const UBorder* ControlsHint = Cast<UBorder>(
			Tree.FindWidget(TEXT("Border_ControlsHint")));
		const UTextBlock* ControlsText = Cast<UTextBlock>(
			Tree.FindWidget(TEXT("Text_Controls")));
		const UButton* CloseButton = Cast<UButton>(Tree.FindWidget(TEXT("Button_Close")));
		const UBorder* ResultPanel = Cast<UBorder>(Tree.FindWidget(TEXT("Panel_Result")));
		const UButton* ResultCloseButton = Cast<UButton>(
			Tree.FindWidget(TEXT("Button_ResultClose")));
		const UOverlay* Root = Cast<UOverlay>(Tree.RootWidget);
		if (!SurfArea || !EntityLayer || !PillHud || !ControlsHint || !ControlsText
			|| !CloseButton || !ResultPanel || !ResultCloseButton || !Root)
		{
			Test.AddError(TEXT("디자이너 편집용 FreeRide named hierarchy가 완전하지 않다."));
			return false;
		}

		const UCanvasPanelSlot* EntitySlot = Cast<UCanvasPanelSlot>(EntityLayer->Slot);
		const UCanvasPanelSlot* PillSlot = Cast<UCanvasPanelSlot>(PillHud->Slot);
		const UCanvasPanelSlot* HintSlot = Cast<UCanvasPanelSlot>(ControlsHint->Slot);
		const UCanvasPanelSlot* CloseSlot = Cast<UCanvasPanelSlot>(CloseButton->Slot);
		Test.TestTrue(TEXT("작은 pill HUD는 SurfArea 직접 자식이다"), PillHud->GetParent() == SurfArea);
		Test.TestTrue(TEXT("조작 힌트는 SurfArea 직접 자식이다"), ControlsHint->GetParent() == SurfArea);
		Test.TestTrue(TEXT("Text_Controls는 힌트 배경 안에서 편집 가능하다"),
			ControlsText->GetParent() == ControlsHint);
		Test.TestTrue(TEXT("작은 종료 버튼은 SurfArea 직접 자식이다"), CloseButton->GetParent() == SurfArea);
		Test.TestEqual(TEXT("조작 힌트는 입력을 가로채지 않는다"),
			ControlsText->GetVisibility(), ESlateVisibility::HitTestInvisible);
		if (EntitySlot && PillSlot && HintSlot && CloseSlot)
		{
			Test.TestTrue(TEXT("pill HUD는 entity보다 위에 그려진다"),
				PillSlot->GetZOrder() > EntitySlot->GetZOrder());
			Test.TestTrue(TEXT("조작 힌트는 entity보다 위에 그려진다"),
				HintSlot->GetZOrder() > EntitySlot->GetZOrder());
			Test.TestTrue(TEXT("종료 버튼은 entity보다 위에 그려진다"),
				CloseSlot->GetZOrder() > EntitySlot->GetZOrder());
			Test.TestTrue(TEXT("pill HUD는 화면 위쪽의 작은 영역만 사용한다"),
				PillSlot->GetAnchors().Minimum.Y < 0.10f
				&& PillSlot->GetOffsets().Bottom <= 100.0f);
			const FAnchors HintAnchors = HintSlot->GetAnchors();
			Test.TestTrue(TEXT("조작 힌트는 하단의 얇은 띠다"),
				HintAnchors.Minimum.Y >= 0.85f
				&& HintAnchors.Maximum.Y - HintAnchors.Minimum.Y <= 0.08f);
		}
		else
		{
			Test.AddError(TEXT("FreeRide HUD/hint/close가 CanvasPanelSlot을 사용하지 않는다."));
		}

		Test.TestTrue(TEXT("결과 모달은 arena 다음에 그려지는 최상위 overlay 자식이다"),
			ResultPanel->GetParent() == Root
			&& Root->GetChildIndex(ResultPanel) > Root->GetChildIndex(SurfArea));
		const UWidget* ResultAncestor = ResultCloseButton;
		while (ResultAncestor && ResultAncestor != ResultPanel)
		{
			ResultAncestor = ResultAncestor->GetParent();
		}
		Test.TestTrue(TEXT("결과 닫기 버튼은 결과 모달 안에 있어 스크림 뒤로 가려지지 않는다"),
			ResultAncestor == ResultPanel);
		return !Test.HasAnyErrors();
	}

	int32 CourseTypeIndex(const EFF_SurfingObjectType Type)
	{
		switch (Type)
		{
		case EFF_SurfingObjectType::Rock: return 0;
		case EFF_SurfingObjectType::Log: return 1;
		case EFF_SurfingObjectType::Buoy: return 2;
		case EFF_SurfingObjectType::Coin: return 3;
		case EFF_SurfingObjectType::Heart: return 4;
		case EFF_SurfingObjectType::BoostPickup: return 5;
		default: return INDEX_NONE;
		}
	}

	FString PackageFilename(const FString& LongPackageName)
	{
		return FPackageName::LongPackageNameToFilename(
			LongPackageName,
			FPackageName::GetAssetPackageExtension());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_SurfingDefinitionAssetContractTest,
	"FestaFiesta.Minigame.Surfing.Content.DefinitionAssetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_SurfingDefinitionAssetContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFSurfingAssetTestHelpers;

	UFF_SurfingDefinition* Definition =
		LoadObject<UFF_SurfingDefinition>(nullptr, DefinitionObjectPath);
	if (!TestNotNull(TEXT("서핑 Definition 에셋이 로드된다"), Definition))
	{
		return false;
	}

	TestEqual(TEXT("PrimaryAssetType은 FFMinigameDefinition이다"),
		Definition->GetPrimaryAssetId().PrimaryAssetType,
		FPrimaryAssetType(TEXT("FFMinigameDefinition")));
	TestEqual(TEXT("PrimaryAssetName은 Surfing이다"),
		Definition->GetPrimaryAssetId().PrimaryAssetName,
		FName(TEXT("Surfing")));
	TestEqual(TEXT("미니게임 ID가 보존된다"), Definition->MinigameId, FName(TEXT("Surfing")));
	TestTrue(TEXT("기본 서핑 에셋은 마지막 목숨 손실 시 즉시 종료한다"),
		Definition->Rules.bEndRunWhenLivesDepleted);
	TestEqual(TEXT("사용자 표시 이름이 보존된다"),
		Definition->DisplayName.ToString(), FString(TEXT("다대포 웨이브 서핑")));
	TestEqual(TEXT("연결 시설은 하나로 보존된다"),
		Definition->CompatibleFacilityDefinitionIds.Num(), 1);
	TestTrue(TEXT("Surfing 시설 연결이 보존된다"),
		Definition->CompatibleFacilityDefinitionIds.Contains(TEXT("Booth_ActivityGame_Surfing")));
	TestEqual(TEXT("플레이 시간은 30초다"), Definition->DurationSeconds, 30.0f);
	TestEqual(TEXT("최대 보상 점수는 3500이다"), Definition->ScoreForMaxReward, 3500);
	TestEqual(TEXT("서핑 runtime 클래스가 보존된다"),
		Definition->RuntimeClass.Get(), UFF_SurfingRuntime::StaticClass());

	UClass* ScreenClass = Definition->ScreenClass.LoadSynchronous();
	if (TestNotNull(TEXT("FreeRide 화면 soft class가 로드된다"), ScreenClass))
	{
		TestTrue(TEXT("FreeRide 화면은 SurfingScreen 파생 클래스다"),
			ScreenClass->IsChildOf(UFF_SurfingScreen::StaticClass()));
		TestEqual(TEXT("Definition은 새 FreeRide WBP를 가리킨다"),
			ScreenClass->GetPathName(), FString(FreeRideScreenClassPath));
		const UFF_SurfingScreen* ScreenDefaults =
			Cast<UFF_SurfingScreen>(ScreenClass->GetDefaultObject());
		if (TestNotNull(TEXT("FreeRide 화면 CDO가 로드된다"), ScreenDefaults))
		{
			TestFalse(TEXT("기본 플레이는 프리미티브 SceneCapture를 사용하지 않는다"),
				ScreenDefaults->VisualStyle.bEnableSceneCapture3D);
			TestEqual(TEXT("최종 백드롭이 기본 플레이에 연결된다"),
				ScreenDefaults->VisualStyle.BackdropTexture.ToSoftObjectPath().ToString(),
				FString(FinalBackdropObjectPath));
			TestEqual(TEXT("최종 서퍼가 기본 플레이에 연결된다"),
				ScreenDefaults->VisualStyle.SurferTexture.ToSoftObjectPath().ToString(),
				FString(FinalSurferObjectPath));
		}
	}

	UDataTable* CourseTable = Definition->CourseLayoutTable.LoadSynchronous();
	if (TestNotNull(TEXT("Definition의 course table이 로드된다"), CourseTable))
	{
		TestEqual(TEXT("Definition은 고정 Surfing course table을 가리킨다"),
			CourseTable->GetPathName(), FString(CourseTableObjectPath));
		TestTrue(TEXT("CourseLayoutTable row struct가 정확하다"),
			CourseTable->GetRowStruct() == FFF_SurfingCourseRow::StaticStruct());
	}

	FString RulesFailure;
	TestTrue(TEXT("FreeRide 서핑 규칙이 유효하다"), Definition->Rules.IsValid(&RulesFailure));
	if (!RulesFailure.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("Rules validation detail: %s"), *RulesFailure));
	}

	TArray<FText> DefinitionErrors;
	TestTrue(TEXT("전체 Definition 계약이 유효하다"),
		Definition->IsDefinitionValid(DefinitionErrors));
	TestEqual(TEXT("Definition 검증 오류가 없다"), DefinitionErrors.Num(), 0);
	TestFalse(TEXT("검증만으로 Definition package를 dirty 처리하지 않는다"),
		Definition->GetOutermost()->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_SurfingCourseTableAssetContractTest,
	"FestaFiesta.Minigame.Surfing.Content.CourseTableContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_SurfingCourseTableAssetContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFSurfingAssetTestHelpers;

	UDataTable* CourseTable = LoadObject<UDataTable>(nullptr, CourseTableObjectPath);
	if (!TestNotNull(TEXT("DT_SurfingCourse가 로드된다"), CourseTable))
	{
		return false;
	}
	if (!TestTrue(TEXT("고정 arena row struct가 정확하다"),
		CourseTable->GetRowStruct() == FFF_SurfingCourseRow::StaticStruct()))
	{
		return false;
	}

	int32 EnabledRowCount = 0;
	bool bHasEnabledObstacle = false;
	bool bHasEnabledPickup = false;
	const TArray<FName> RowNames = CourseTable->GetRowNames();
	TestTrue(TEXT("course table에는 최소 두 행이 있다"), RowNames.Num() >= 2);
	for (const FName RowName : RowNames)
	{
		const FFF_SurfingCourseRow* Row = CourseTable->FindRow<FFF_SurfingCourseRow>(
			RowName, TEXT("Surfing CourseTableContract"), false);
		if (!TestNotNull(
			*FString::Printf(TEXT("%s row가 역직렬화된다"), *RowName.ToString()), Row))
		{
			continue;
		}
		if (!Row->bEnabled)
		{
			continue;
		}

		const int32 TypeIndex = CourseTypeIndex(Row->Type);
		TestTrue(*FString::Printf(TEXT("%s type이 지원된다"), *RowName.ToString()),
			TypeIndex != INDEX_NONE);
		TestTrue(*FString::Printf(TEXT("%s X 좌표가 유한한 [-1,1] 값이다"), *RowName.ToString()),
			FMath::IsFinite(Row->PositionNormalized.X)
			&& Row->PositionNormalized.X >= -1.0f
			&& Row->PositionNormalized.X <= 1.0f);
		TestTrue(*FString::Printf(TEXT("%s Y 좌표가 유한한 [-1,1] 값이다"), *RowName.ToString()),
			FMath::IsFinite(Row->PositionNormalized.Y)
			&& Row->PositionNormalized.Y >= -1.0f
			&& Row->PositionNormalized.Y <= 1.0f);
		TestTrue(*FString::Printf(TEXT("%s 충돌 반경이 양의 유한값이다"), *RowName.ToString()),
			FMath::IsFinite(Row->CollisionRadiusNormalized)
			&& Row->CollisionRadiusNormalized > 0.0f);
		TestTrue(*FString::Printf(TEXT("%s 점수 override가 유효하다"), *RowName.ToString()),
			FMath::IsFinite(Row->ScoreValueOverride) && Row->ScoreValueOverride >= 0.0f);
		if (TypeIndex != INDEX_NONE)
		{
			++EnabledRowCount;
			if (Row->Type == EFF_SurfingObjectType::Rock
				|| Row->Type == EFF_SurfingObjectType::Log
				|| Row->Type == EFF_SurfingObjectType::Buoy)
			{
				bHasEnabledObstacle = true;
			}
			else
			{
				bHasEnabledPickup = true;
			}
		}
	}

	TestTrue(TEXT("활성 course 행이 최소 두 개다"), EnabledRowCount >= 2);
	TestTrue(TEXT("활성 장애물이 최소 하나 있다"), bHasEnabledObstacle);
	TestTrue(TEXT("활성 pickup이 최소 하나 있다"), bHasEnabledPickup);
	TestFalse(TEXT("DT 검증은 package를 dirty 처리하지 않는다"),
		CourseTable->GetOutermost()->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_SurfingWidgetAssetContractTest,
	"FestaFiesta.Minigame.Surfing.UI.WidgetAssetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_SurfingWidgetAssetContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFSurfingAssetTestHelpers;

	// Rollback WBPs are optional in a clean checkout. When present, the builder's
	// idempotence test below verifies that their exact files stay untouched.
	AddInfo(FString::Printf(
		TEXT("Optional rollback WBPs: legacy=%s edge=%s"),
		FPackageName::DoesPackageExist(LegacyScreenPackage) ? TEXT("present") : TEXT("absent"),
		FPackageName::DoesPackageExist(EdgeScreenPackage) ? TEXT("present") : TEXT("absent")));

	UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, FreeRideScreenObjectPath);
	if (!TestNotNull(TEXT("새 FreeRide Surfing WBP가 로드된다"), Blueprint))
	{
		return false;
	}

	UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(Blueprint->GeneratedClass);
	if (!TestNotNull(TEXT("FreeRide WBP 생성 클래스가 존재한다"), GeneratedClass))
	{
		return false;
	}
	TestTrue(TEXT("FreeRide WBP는 SurfingScreen을 상속한다"),
		GeneratedClass->IsChildOf(UFF_SurfingScreen::StaticClass()));
	TestEqual(TEXT("FreeRide WBP 생성 클래스 경로가 고정된다"),
		GeneratedClass->GetPathName(), FString(FreeRideScreenClassPath));

	const UWidgetTree* Tree = GeneratedClass->GetWidgetTreeArchetype();
	if (!TestNotNull(TEXT("컴파일된 FreeRide WidgetTree가 존재한다"), Tree))
	{
		return false;
	}

	for (const FRequiredWidget& Required : GetRequiredWidgets())
	{
		UWidget* Widget = Tree->FindWidget(Required.Name);
		TestNotNull(*FString::Printf(TEXT("필수 위젯 %s가 존재한다"), *Required.Name.ToString()), Widget);
		if (Widget)
		{
			TestTrue(*FString::Printf(TEXT("%s 타입이 심플 계약과 같다"), *Required.Name.ToString()),
				Widget->IsA(Required.WidgetClass));
			TestTrue(*FString::Printf(TEXT("%s가 Blueprint 변수다"), *Required.Name.ToString()),
				Widget->bIsVariable);
			TestTrue(*FString::Printf(TEXT("%s 변수 GUID가 저장됐다"), *Required.Name.ToString()),
				Blueprint->WidgetVariableNameToGuidMap.Contains(Required.Name));
		}
	}
	for (const FName RemovedName : GetRemovedEdgeWidgetNames())
	{
		TestNull(*FString::Printf(TEXT("단순화된 WBP에는 %s가 없다"), *RemovedName.ToString()),
			Tree->FindWidget(RemovedName));
	}

	TestArenaTree(*this, *Tree, true);
	TestDesignerHierarchy(*this, *Tree);
	TestFalse(TEXT("FreeRide WBP 검증은 package를 dirty 처리하지 않는다"),
		Blueprint->GetOutermost()->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_SurfingNativeFallbackContractTest,
	"FestaFiesta.Minigame.Surfing.UI.NativeFallbackContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_SurfingNativeFallbackContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFSurfingAssetTestHelpers;

	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("폴백 UI 계약 검증용 EditorWorld가 존재한다"), EditorWorld))
	{
		return false;
	}

	UFF_SurfingScreen* Screen = CreateWidget<UFF_SurfingScreen>(
		EditorWorld,
		UFF_SurfingScreen::StaticClass());
	if (!TestNotNull(TEXT("저장된 WBP 없이 네이티브 FreeRide 화면을 생성한다"), Screen))
	{
		return false;
	}

	const TSharedRef<SWidget> Slate = Screen->TakeWidget();
	(void)Slate;
	TestFalse(TEXT("네이티브 클래스는 디자이너 트리로 오인되지 않는다"),
		Screen->IsUsingDesignerWidgetTree());
	for (const FRequiredWidget& Required : GetRequiredWidgets())
	{
		UWidget* Widget = Screen->GetWidgetFromName(Required.Name);
		TestNotNull(
			*FString::Printf(TEXT("네이티브 폴백에 %s가 있다"), *Required.Name.ToString()),
			Widget);
		if (Widget)
		{
			TestTrue(
				*FString::Printf(TEXT("네이티브 %s 타입이 계약과 같다"), *Required.Name.ToString()),
				Widget->IsA(Required.WidgetClass));
		}
	}
	for (const FName RemovedName : GetRemovedEdgeWidgetNames())
	{
		TestNull(*FString::Printf(TEXT("네이티브 폴백에도 %s가 없다"), *RemovedName.ToString()),
			Screen->GetWidgetFromName(RemovedName));
	}

	const UWidgetTree* NativeTree = Screen->WidgetTree;
	if (TestNotNull(TEXT("네이티브 FreeRide WidgetTree가 존재한다"), NativeTree))
	{
		TestArenaTree(*this, *NativeTree, true);
	}

	FFF_SurfingSnapshot Snapshot;
	Snapshot.Session.Context.SessionId = FGuid::NewGuid();
	Snapshot.Session.Context.MinigameId = TEXT("Surfing");
	Snapshot.Session.State = EFF_MinigameSessionState::Playing;
	Snapshot.Session.RemainingSeconds = 11.4f;
	Snapshot.Session.Score = 1350;
	Snapshot.PlayerPositionNormalized = FVector2D(0.40f, 0.50f);
	Snapshot.MovementInput = FVector2D(0.75f, 0.50f);
	Snapshot.CurrentHeading = Snapshot.MovementInput.GetSafeNormal();
	Snapshot.Lives = 2;
	Snapshot.MaxLives = 3;
	Snapshot.BoostCharges = 2;
	Snapshot.MaxBoostCharges = 3;
	Snapshot.bBoosting = false;
	Snapshot.CollisionCount = 1;
	Snapshot.CoinCount = 4;
	Snapshot.LatestEventSerial = 5;
	Snapshot.LatestEventType = EFF_SurfingEventType::CoinCollected;

	FFF_SurfingObjectSnapshot Rock;
	Rock.ObjectId = 11;
	Rock.Type = EFF_SurfingObjectType::Rock;
	Rock.PositionNormalized = FVector2D(-0.55f, 0.65f);
	Rock.CollisionRadiusNormalized = 0.085f;
	Snapshot.VisibleObjects.Add(Rock);
	FFF_SurfingObjectSnapshot Coin;
	Coin.ObjectId = 12;
	Coin.Type = EFF_SurfingObjectType::Coin;
	Coin.PositionNormalized = FVector2D(0.35f, -0.28f);
	Coin.CollisionRadiusNormalized = 0.05f;
	Snapshot.VisibleObjects.Add(Coin);
	Screen->ApplySnapshot(Snapshot);

	const FFF_SurfingSnapshot& Displayed = Screen->GetDisplayedSnapshot();
	TestEqual(TEXT("네이티브 폴백이 점수를 표시 상태에 보존한다"),
		Displayed.Session.Score, 1350);
	TestEqual(TEXT("네이티브 폴백이 2D 플레이어 위치를 보존한다"),
		Displayed.PlayerPositionNormalized, FVector2D(0.40f, 0.50f));
	TestEqual(TEXT("네이티브 폴백이 생명을 보존한다"), Displayed.Lives, 2);
	TestEqual(TEXT("네이티브 폴백이 부스트 수를 보존한다"), Displayed.BoostCharges, 2);
	TestEqual(TEXT("네이티브 폴백이 고정 객체를 보존한다"),
		Displayed.VisibleObjects.Num(), 2);

	auto TextContains = [this, Screen](const FName WidgetName, const FString& Expected)
	{
		const UTextBlock* Text = Cast<UTextBlock>(Screen->GetWidgetFromName(WidgetName));
		if (!TestNotNull(*FString::Printf(TEXT("%s 텍스트가 있다"), *WidgetName.ToString()), Text))
		{
			return false;
		}
		return TestTrue(
			*FString::Printf(TEXT("%s에 %s 값이 투영된다"), *WidgetName.ToString(), *Expected),
			Text->GetText().ToString().Contains(Expected));
	};
	TextContains(TEXT("Text_RemainingTime"), TEXT("12"));
	const UTextBlock* ScoreText = Cast<UTextBlock>(
		Screen->GetWidgetFromName(TEXT("Text_Score")));
	if (TestNotNull(TEXT("Text_Score 텍스트가 있다"), ScoreText))
	{
		const FString ProjectedScore = ScoreText->GetText().ToString();
		TestTrue(TEXT("Text_Score에 1350점이 투영된다"),
			ProjectedScore.Contains(TEXT("1,350")) || ProjectedScore.Contains(TEXT("1350")));
	}
	TextContains(TEXT("Text_Lives"), TEXT("2"));
	TextContains(TEXT("Text_Boost"), TEXT("2"));

	const UBorder* Avatar = Cast<UBorder>(
		Screen->GetWidgetFromName(TEXT("Border_SurferAvatar")));
	if (TestNotNull(TEXT("네이티브 서퍼 아바타가 있다"), Avatar))
	{
		const UCanvasPanelSlot* AvatarSlot = Cast<UCanvasPanelSlot>(Avatar->Slot);
		if (TestNotNull(TEXT("서퍼 아바타가 CanvasPanelSlot을 사용한다"), AvatarSlot))
		{
			const FVector2D Anchor = AvatarSlot->GetAnchors().Minimum;
			TestTrue(TEXT("양의 X 위치가 서퍼를 화면 오른쪽으로 투영한다"), Anchor.X > 0.5f);
			TestTrue(TEXT("양의 Y 위치가 서퍼를 화면 중앙 위로 투영한다"), Anchor.Y < 0.5f);
		}
	}

	const UCanvasPanel* EntityLayer = Cast<UCanvasPanel>(
		Screen->GetWidgetFromName(TEXT("Canvas_EntityLayer")));
	if (TestNotNull(TEXT("네이티브 EntityLayer가 있다"), EntityLayer))
	{
		const UBorder* LayerAvatar = Cast<UBorder>(
			Screen->GetWidgetFromName(TEXT("Border_SurferAvatar")));
		const int32 StaticAvatarChildren =
			LayerAvatar && LayerAvatar->GetParent() == EntityLayer ? 1 : 0;
		TestEqual(TEXT("두 고정 object에 대응하는 두 위젯을 투영한다"),
			EntityLayer->GetChildrenCount() - StaticAvatarChildren, 2);
		bool bFoundUpperLeft = false;
		bool bFoundLowerRight = false;
		for (int32 Index = 0; Index < EntityLayer->GetChildrenCount(); ++Index)
		{
			UWidget* EntityWidget = EntityLayer->GetChildAt(Index);
			if (EntityWidget == LayerAvatar)
			{
				continue;
			}
			const UCanvasPanelSlot* EntitySlot = EntityWidget
				? Cast<UCanvasPanelSlot>(EntityWidget->Slot)
				: nullptr;
			if (TestNotNull(TEXT("고정 entity는 CanvasPanelSlot을 사용한다"), EntitySlot))
			{
				const FVector2D Anchor = EntitySlot->GetAnchors().Minimum;
				bFoundUpperLeft |= Anchor.X < 0.5f && Anchor.Y < 0.5f;
				bFoundLowerRight |= Anchor.X > 0.5f && Anchor.Y > 0.5f;
			}
			if (EntityWidget)
			{
				TestEqual(TEXT("고정 entity는 입력을 가로채지 않는다"),
					EntityWidget->GetVisibility(), ESlateVisibility::HitTestInvisible);
			}
		}
		TestTrue(TEXT("음의 X/양의 Y 객체가 arena 좌상단에 투영된다"), bFoundUpperLeft);
		TestTrue(TEXT("양의 X/음의 Y 객체가 arena 우하단에 투영된다"), bFoundLowerRight);
	}

	FFF_MinigameResult Result;
	Result.SessionId = Snapshot.Session.Context.SessionId;
	Result.MinigameId = TEXT("Surfing");
	Result.Score = 1840;
	Result.RewardGold = 275;
	Result.GradeText = FText::FromString(TEXT("파도 지배자"));
	Result.EndReason = EFF_MinigameEndReason::TimeExpired;
	Screen->ShowResult(Result);
	const UBorder* ResultPanel = Cast<UBorder>(
		Screen->GetWidgetFromName(TEXT("Panel_Result")));
	const UTextBlock* ResultText = Cast<UTextBlock>(
		Screen->GetWidgetFromName(TEXT("Text_Result")));
	if (TestNotNull(TEXT("네이티브 결과 패널이 있다"), ResultPanel))
	{
		TestEqual(TEXT("ShowResult 후 결과 패널을 보여 준다"),
			ResultPanel->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	}
	if (TestNotNull(TEXT("네이티브 결과 텍스트가 있다"), ResultText))
	{
		const FString ProjectedResult = ResultText->GetText().ToString();
		TestTrue(TEXT("결과 등급이 투영된다"), ProjectedResult.Contains(TEXT("파도 지배자")));
		TestTrue(TEXT("결과 점수가 투영된다"),
			ProjectedResult.Contains(TEXT("1,840")) || ProjectedResult.Contains(TEXT("1840")));
		TestTrue(TEXT("보상 골드가 투영된다"), ProjectedResult.Contains(TEXT("275")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_Surfing3DPresentationContractTest,
	"FestaFiesta.Minigame.Surfing.UI.ThreeDimensionalPresentationContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_Surfing3DPresentationContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("3D 서핑 계약 검증용 EditorWorld가 존재한다"), EditorWorld))
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.ObjectFlags |= RF_Transient;
	AFF_SurfingSceneActor* SceneActor = EditorWorld->SpawnActor<AFF_SurfingSceneActor>(
		AFF_SurfingSceneActor::StaticClass(),
		FVector(400000.0, 400000.0, 100000.0),
		FRotator::ZeroRotator,
		SpawnParameters);
	if (!TestNotNull(TEXT("transient 3D 서핑 scene actor가 생성된다"), SceneActor))
	{
		return false;
	}

	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(
		SceneActor, UTextureRenderTarget2D::StaticClass(), NAME_None, RF_Transient);
	if (!TestNotNull(TEXT("3D 서핑 전용 render target이 생성된다"), RenderTarget))
	{
		SceneActor->Destroy();
		return false;
	}
	RenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
	RenderTarget->InitAutoFormat(640, 360);

	FFF_SurfingVisualStyle Style;
	TestTrue(TEXT("기본 바다는 어두운 심해색이 아닌 낮 바다 색상이다"),
		Style.OceanColor.GetLuminance() > 0.25f
		&& Style.DaylightSkyColor.GetLuminance() > 0.45f);
	TestTrue(TEXT("카메라는 좌우 이동을 일부만 따라 캐릭터 움직임을 보존한다"),
		Style.CameraLateralFollowRatio > 0.0f
		&& Style.CameraLateralFollowRatio < 0.9f);
	SceneActor->InitializeScene(Style, RenderTarget);
	TestTrue(TEXT("수면, 카메라, 보드와 라이더가 3D scene을 구성한다"),
		SceneActor->IsSceneReady());
	TestEqual(TEXT("라이더는 몸통, 머리, 양팔, 양다리의 6개 3D 파트다"),
		SceneActor->GetRiderMeshPartCount(), 6);
	AFF_SurfingCharacterActor* InitialCharacter = SceneActor->GetSurfingCharacterActor();
	TestTrue(TEXT("기본 캐릭터는 교체 가능한 서핑 전용 actor다"),
		IsValid(InitialCharacter)
		&& InitialCharacter->IsA<AFF_SurfingPrimitiveCharacterActor>()
		&& InitialCharacter->IsCharacterReady());
	TestTrue(TEXT("기본 캐릭터는 보드, 라이더와 물보라를 실제 primitive로 렌더한다"),
		SceneActor->GetCharacterRenderablePrimitiveCount() >= 9);

	USceneCaptureComponent2D* Capture = SceneActor->GetSceneCaptureComponent();
	if (TestNotNull(TEXT("SceneCapture2D 카메라가 존재한다"), Capture))
	{
		TestTrue(TEXT("SceneCapture는 자기 arena만 show-only 렌더한다"),
			Capture->PrimitiveRenderMode
				== ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList
			&& Capture->ShowOnlyActors.Contains(SceneActor)
			&& Capture->ShowOnlyActors.Contains(InitialCharacter));
		TestTrue(TEXT("SceneCapture는 전용 render target을 사용한다"),
			Capture->TextureTarget == RenderTarget);
		TestTrue(TEXT("SceneCapture는 자동 노출이 아닌 고정 낮 노출을 사용한다"),
			Capture->PostProcessSettings.bOverride_AutoExposureMethod
			&& Capture->PostProcessSettings.AutoExposureMethod == AEM_Manual
			&& Capture->PostProcessSettings.bOverride_AutoExposureBias
			&& Capture->PostProcessSettings.AutoExposureBias > 0.0f);
	}

	FFF_SurfingSnapshot Snapshot;
	Snapshot.ArenaHorizontalMetricScale = 2.0f;
	Snapshot.PlayerPositionNormalized = FVector2D(0.40f, 0.25f);
	Snapshot.CurrentHeading = FVector2D(0.5f, 1.0f).GetSafeNormal();
	Snapshot.bBoosting = true;
	for (int32 Index = 0; Index < 6; ++Index)
	{
		FFF_SurfingObjectSnapshot& Object = Snapshot.VisibleObjects.AddDefaulted_GetRef();
		Object.ObjectId = 100 + Index;
		Object.Type = static_cast<EFF_SurfingObjectType>(Index);
		Object.PositionNormalized = FVector2D(
			-0.75f + Index * 0.30f,
			-0.60f + Index * 0.22f);
		Object.CollisionRadiusNormalized = 0.055f + Index * 0.005f;
	}
	SceneActor->ApplySnapshot(Snapshot);
	TestEqual(TEXT("여섯 종류의 코스 객체가 모두 3D component로 생성된다"),
		SceneActor->GetVisibleCourseObjectCount(), 6);

	const FVector ExpectedPlayerLocation(362.5, 1160.0, 76.0);
	TestTrue(TEXT("정규화 좌표는 충돌 metric을 보존한 3D 수면 좌표가 된다"),
		SceneActor->GetPlayerSceneLocalLocation().Equals(ExpectedPlayerLocation, 0.1));
	const FVector2D RoundTrip = SceneActor->SceneLocalToArena(
		SceneActor->ArenaToSceneLocal(FVector2D(-0.35f, 0.65f), 0.0f));
	TestTrue(TEXT("arena 정규화 좌표와 3D 좌표 변환은 왕복 가능하다"),
		RoundTrip.Equals(FVector2D(-0.35f, 0.65f), 0.001f));

	TArray<UStaticMeshComponent*> MeshComponents;
	SceneActor->GetComponents<UStaticMeshComponent>(MeshComponents);
	TestTrue(TEXT("수면, 파도와 코스가 실제 3D mesh component다"),
		MeshComponents.Num() >= 19);
	for (const UStaticMeshComponent* MeshComponent : MeshComponents)
	{
		if (!MeshComponent)
		{
			continue;
		}
		TestTrue(*FString::Printf(
			TEXT("3D mesh %s는 메인 월드가 아닌 SceneCapture에만 보인다"),
			*MeshComponent->GetName()),
			MeshComponent->bVisibleInSceneCaptureOnly);
		TestEqual(*FString::Printf(
			TEXT("3D 표현 mesh %s는 gameplay collision을 만들지 않는다"),
			*MeshComponent->GetName()),
			MeshComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	}
	if (InitialCharacter)
	{
		TArray<UPrimitiveComponent*> CharacterPrimitives;
		InitialCharacter->GetComponents<UPrimitiveComponent>(CharacterPrimitives);
		for (const UPrimitiveComponent* Primitive : CharacterPrimitives)
		{
			if (!Primitive)
			{
				continue;
			}
			TestTrue(TEXT("교체형 캐릭터 primitive는 SceneCapture에만 보인다"),
				Primitive->bVisibleInSceneCaptureOnly);
			TestEqual(TEXT("교체형 캐릭터 primitive는 gameplay collision이 없다"),
				Primitive->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		}
	}

	const FVector BeforeMove = SceneActor->GetPlayerSceneLocalLocation();
	Snapshot.PlayerPositionNormalized = FVector2D(-0.30f, -0.45f);
	Snapshot.CurrentHeading = FVector2D(-0.25f, -1.0f).GetSafeNormal();
	SceneActor->ApplySnapshot(Snapshot);
	for (int32 TickIndex = 0; TickIndex < 60; ++TickIndex)
	{
		SceneActor->Tick(0.05f);
	}
	const FVector AfterMove = SceneActor->GetPlayerSceneLocalLocation();
	TestTrue(TEXT("새 입력 snapshot을 받으면 3D 보드와 캐릭터가 함께 이동한다"),
		!AfterMove.Equals(BeforeMove, 1.0f)
		&& AfterMove.Equals(SceneActor->ArenaToSceneLocal(
			Snapshot.PlayerPositionNormalized, 76.0f), 8.0f));
	const FVector CameraAfterLateralMove = Capture
		? Capture->GetRelativeLocation()
		: FVector::ZeroVector;
	const float ExpectedCameraY = SceneActor->ArenaToSceneLocal(
		Snapshot.PlayerPositionNormalized, 76.0f).Y
		* Style.CameraLateralFollowRatio
		+ Style.Camera3DLocation.Y;
	TestTrue(TEXT("카메라가 수렴 후에도 설정 비율만큼만 좌우를 추적한다"),
		Capture && FMath::IsNearlyEqual(
			CameraAfterLateralMove.Y,
			ExpectedCameraY,
			2.0f));

	TestTrue(TEXT("동일한 전용 캐릭터 class도 런타임에 안전하게 교체할 수 있다"),
		SceneActor->SetSurfingCharacterClass(
			AFF_SurfingPrimitiveCharacterActor::StaticClass()));
	AFF_SurfingCharacterActor* ReplacedCharacter = SceneActor->GetSurfingCharacterActor();
	TestTrue(TEXT("교체 후 새 캐릭터 한 개가 준비되고 capture 목록도 갱신된다"),
		IsValid(ReplacedCharacter)
		&& ReplacedCharacter != InitialCharacter
		&& ReplacedCharacter->IsCharacterReady()
		&& Capture->ShowOnlyActors.Contains(ReplacedCharacter)
		&& !Capture->ShowOnlyActors.Contains(InitialCharacter));

	FVector2D CenterArenaPoint;
	TestTrue(TEXT("마우스 capture 좌표를 3D 수면 위치로 역투영할 수 있다"),
		SceneActor->DeprojectCapturePointToArena(
			FVector2D(0.5f, 0.5f), CenterArenaPoint)
		&& FMath::IsFinite(CenterArenaPoint.X)
		&& FMath::IsFinite(CenterArenaPoint.Y));

	SceneActor->ReleaseScene();
	TestTrue(TEXT("release 시 SceneCapture와 render target 연결을 해제한다"),
		Capture && Capture->TextureTarget == nullptr
		&& !SceneActor->IsSceneReady()
		&& SceneActor->GetSurfingCharacterActor() == nullptr);
	TestTrue(TEXT("검증 후 transient 3D scene actor를 정리한다"), SceneActor->Destroy());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_SurfingDaylightPixelContractTest,
	"FestaFiesta.Minigame.Surfing.Visual.DaylightOceanPixelContract",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter
		| EAutomationTestFlags::NonNullRHI)

bool FFF_SurfingDaylightPixelContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("낮 바다 픽셀 검증용 EditorWorld가 존재한다"), EditorWorld))
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.ObjectFlags |= RF_Transient;
	AFF_SurfingSceneActor* SceneActor = EditorWorld->SpawnActor<AFF_SurfingSceneActor>(
		AFF_SurfingSceneActor::StaticClass(),
		FVector(500000.0, 500000.0, 150000.0),
		FRotator::ZeroRotator,
		SpawnParameters);
	if (!TestNotNull(TEXT("낮 바다 scene actor가 생성된다"), SceneActor))
	{
		return false;
	}

	constexpr int32 CaptureWidth = 640;
	constexpr int32 CaptureHeight = 360;
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(
		SceneActor, UTextureRenderTarget2D::StaticClass(), NAME_None, RF_Transient);
	RenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
	FFF_SurfingVisualStyle Style;
	RenderTarget->ClearColor = Style.DaylightSkyColor;
	RenderTarget->InitAutoFormat(CaptureWidth, CaptureHeight);
	RenderTarget->UpdateResourceImmediate(true);
	SceneActor->InitializeScene(Style, RenderTarget);

	FFF_SurfingSnapshot Snapshot;
	Snapshot.PlayerPositionNormalized = FVector2D::ZeroVector;
	Snapshot.CurrentHeading = FVector2D(0.0, 1.0);
	Snapshot.ArenaHorizontalMetricScale = 2.18f;
	SceneActor->ApplySnapshot(Snapshot);
	USceneCaptureComponent2D* Capture = SceneActor->GetSceneCaptureComponent();
	Capture->bCaptureEveryFrame = false;
	Capture->CaptureScene();

	TArray<FColor> Pixels;
	FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
	const bool bReadPixels = Resource && Resource->ReadPixels(Pixels);
	TestTrue(TEXT("실제 RHI에서 낮 바다 render target 픽셀을 읽는다"),
		bReadPixels && Pixels.Num() == CaptureWidth * CaptureHeight);

	double LuminanceSum = 0.0;
	double LuminanceSquaredSum = 0.0;
	double SkyRedSum = 0.0;
	double WaterRedSum = 0.0;
	int32 BlackPixelCount = 0;
	int32 OceanBluePixelCount = 0;
	int32 OverexposedPixelCount = 0;
	int32 WarmCharacterPixelCount = 0;
	int32 SkyPixelCount = 0;
	int32 WaterPixelCount = 0;
	for (int32 PixelIndex = 0; PixelIndex < Pixels.Num(); ++PixelIndex)
	{
		const FColor& Pixel = Pixels[PixelIndex];
		const double Luminance = (
			0.2126 * Pixel.R + 0.7152 * Pixel.G + 0.0722 * Pixel.B) / 255.0;
		LuminanceSum += Luminance;
		LuminanceSquaredSum += Luminance * Luminance;
		BlackPixelCount += Luminance < 0.03 ? 1 : 0;
		OceanBluePixelCount += Pixel.B > Pixel.R * 1.15
			&& Pixel.G > Pixel.R * 1.05 ? 1 : 0;
		OverexposedPixelCount += Luminance > 0.97 ? 1 : 0;
		WarmCharacterPixelCount += Pixel.R > Pixel.G * 1.08
			&& Pixel.R > Pixel.B * 1.18
			&& Pixel.R > 110 ? 1 : 0;
		const int32 PixelY = PixelIndex / CaptureWidth;
		if (PixelY < CaptureHeight / 4)
		{
			SkyRedSum += Pixel.R / 255.0;
			++SkyPixelCount;
		}
		else if (PixelY > CaptureHeight / 3)
		{
			WaterRedSum += Pixel.R / 255.0;
			++WaterPixelCount;
		}
	}
	const double PixelCount = FMath::Max(1, Pixels.Num());
	const double MeanLuminance = LuminanceSum / PixelCount;
	const double LuminanceVariance = FMath::Max(
		0.0,
		LuminanceSquaredSum / PixelCount - MeanLuminance * MeanLuminance);
	const double BlackRatio = BlackPixelCount / PixelCount;
	const double OceanBlueRatio = OceanBluePixelCount / PixelCount;
	const double OverexposedRatio = OverexposedPixelCount / PixelCount;
	const double SkyMeanRed = SkyRedSum / FMath::Max(1, SkyPixelCount);
	const double WaterMeanRed = WaterRedSum / FMath::Max(1, WaterPixelCount);
	AddInfo(FString::Printf(
		TEXT("Daylight ocean capture: mean %.3f, variance %.4f, black %.3f, overexposed %.3f, blue/cyan %.3f, sky-water red delta %.3f, warm rider %d"),
		MeanLuminance,
		LuminanceVariance,
		BlackRatio,
		OverexposedRatio,
		OceanBlueRatio,
		SkyMeanRed - WaterMeanRed,
		WarmCharacterPixelCount));
	TestTrue(TEXT("전체 화면이 낮 바다 범위로 밝고 과노출되지 않는다"),
		MeanLuminance > 0.45 && MeanLuminance < 0.85);
	TestTrue(TEXT("검은 배경 영역이 화면의 5% 미만이다"), BlackRatio < 0.05);
	TestTrue(TEXT("화면 대부분이 파란색/청록색 바다 톤이다"), OceanBlueRatio > 0.55);
	TestTrue(TEXT("과노출 픽셀이 화면의 8% 미만이다"), OverexposedRatio < 0.08);
	TestTrue(TEXT("하늘과 바다가 색으로 구분되는 실제 수평선이 있다"),
		SkyMeanRed - WaterMeanRed > 0.08);
	TestTrue(TEXT("단색 clear 화면이 아니라 장면 명암 변화가 있다"),
		LuminanceVariance > 0.002);
	TestTrue(TEXT("주황색 서퍼 실루엣이 실제 픽셀에 렌더된다"),
		WarmCharacterPixelCount > 25);

	TArray64<uint8> PngData;
	FImageUtils::PNGCompressImageArray(CaptureWidth, CaptureHeight, Pixels, PngData);
	const FString CaptureDirectory = FPaths::Combine(
		FPaths::ProjectSavedDir(), TEXT("Automation"));
	IFileManager::Get().MakeDirectory(*CaptureDirectory, true);
	const FString CapturePath = FPaths::Combine(
		CaptureDirectory, TEXT("SurfingDaylightCapture.png"));
	TestTrue(TEXT("검증용 낮 바다 PNG를 저장한다"),
		FFileHelper::SaveArrayToFile(PngData, *CapturePath));
	AddInfo(FString::Printf(TEXT("Daylight capture saved: %s"), *CapturePath));

	SceneActor->ReleaseScene();
	SceneActor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_SurfingAssetIdempotentReuseContractTest,
	"FestaFiesta.Minigame.Surfing.Content.IdempotentReuseContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_SurfingAssetIdempotentReuseContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFSurfingAssetTestHelpers;

	UWidgetBlueprint* BeforeBlueprint =
		LoadObject<UWidgetBlueprint>(nullptr, FreeRideScreenObjectPath);
	UDataTable* BeforeCourseTable = LoadObject<UDataTable>(nullptr, CourseTableObjectPath);
	UFF_SurfingDefinition* BeforeDefinition =
		LoadObject<UFF_SurfingDefinition>(nullptr, DefinitionObjectPath);
	if (!TestNotNull(TEXT("idempotence 검증 전 FreeRide WBP가 로드된다"), BeforeBlueprint)
		|| !TestNotNull(TEXT("idempotence 검증 전 course table이 로드된다"), BeforeCourseTable)
		|| !TestNotNull(TEXT("idempotence 검증 전 Definition이 로드된다"), BeforeDefinition))
	{
		return false;
	}

	UClass* BeforeScreenClass = BeforeDefinition->ScreenClass.LoadSynchronous();
	if (!TestNotNull(TEXT("idempotence 검증 전 FreeRide ScreenClass가 로드된다"), BeforeScreenClass)
		|| BeforeScreenClass->GetPathName() != FreeRideScreenClassPath
		|| BeforeDefinition->CourseLayoutTable.LoadSynchronous() != BeforeCourseTable)
	{
		AddError(TEXT("idempotence 명령은 이미 완전히 마이그레이션된 Definition에서 실행한다."));
		return false;
	}
	if (BeforeBlueprint->GetOutermost()->IsDirty()
		|| BeforeCourseTable->GetOutermost()->IsDirty()
		|| BeforeDefinition->GetOutermost()->IsDirty())
	{
		AddError(TEXT("idempotence 검증 전 asset package가 이미 dirty다."));
		return false;
	}

	const bool bLegacyExistedBefore = FPackageName::DoesPackageExist(LegacyScreenPackage);
	const bool bEdgeExistedBefore = FPackageName::DoesPackageExist(EdgeScreenPackage);
	TArray<FString> PreservedPackages = {
		FreeRideScreenPackage,
		CourseTablePackage,
		DefinitionPackage,
	};
	if (bLegacyExistedBefore)
	{
		PreservedPackages.Insert(LegacyScreenPackage, 0);
	}
	if (bEdgeExistedBefore)
	{
		PreservedPackages.Insert(EdgeScreenPackage, bLegacyExistedBefore ? 1 : 0);
	}
	TArray<FDateTime> TimestampsBefore;
	TArray<FMD5Hash> HashesBefore;
	for (const FString& PackageName : PreservedPackages)
	{
		const FString Filename = PackageFilename(PackageName);
		TestTrue(*FString::Printf(TEXT("%s 파일이 존재한다"), *PackageName),
			IFileManager::Get().FileExists(*Filename));
		TimestampsBefore.Add(IFileManager::Get().GetTimeStamp(*Filename));
		const FMD5Hash Hash = FMD5Hash::HashFile(*Filename);
		TestTrue(*FString::Printf(TEXT("%s 실행 전 hash가 유효하다"), *PackageName),
			Hash.IsValid());
		HashesBefore.Add(Hash);
	}

	IConsoleObject* ConsoleObject =
		IConsoleManager::Get().FindConsoleObject(TEXT("FF.BuildSurfingAssets"));
	IConsoleCommand* Command = ConsoleObject ? ConsoleObject->AsCommand() : nullptr;
	if (!TestNotNull(TEXT("FF.BuildSurfingAssets 명령이 등록됐다"), Command))
	{
		return false;
	}
	TestTrue(TEXT("이미 준비된 FreeRide asset에 빌더 명령을 재실행한다"),
		Command->Execute(TArray<FString>(), nullptr, *GLog));

	UWidgetBlueprint* AfterBlueprint =
		LoadObject<UWidgetBlueprint>(nullptr, FreeRideScreenObjectPath);
	UDataTable* AfterCourseTable = LoadObject<UDataTable>(nullptr, CourseTableObjectPath);
	UFF_SurfingDefinition* AfterDefinition =
		LoadObject<UFF_SurfingDefinition>(nullptr, DefinitionObjectPath);
	TestTrue(TEXT("재실행은 같은 WBP 객체를 재사용한다"), AfterBlueprint == BeforeBlueprint);
	TestTrue(TEXT("재실행은 같은 course table 객체를 재사용한다"),
		AfterCourseTable == BeforeCourseTable);
	TestTrue(TEXT("재실행은 같은 Definition 객체를 재사용한다"),
		AfterDefinition == BeforeDefinition);
	TestTrue(TEXT("재실행은 같은 FreeRide ScreenClass를 유지한다"),
		AfterDefinition && AfterDefinition->ScreenClass.LoadSynchronous() == BeforeScreenClass);
	TestTrue(TEXT("재실행은 같은 CourseLayoutTable을 유지한다"),
		AfterDefinition
		&& AfterDefinition->CourseLayoutTable.LoadSynchronous() == BeforeCourseTable);
	TestFalse(TEXT("재실행 후 FreeRide WBP package는 clean이다"),
		AfterBlueprint && AfterBlueprint->GetOutermost()->IsDirty());
	TestFalse(TEXT("재실행 후 course table package는 clean이다"),
		AfterCourseTable && AfterCourseTable->GetOutermost()->IsDirty());
	TestFalse(TEXT("재실행 후 Definition package는 clean이다"),
		AfterDefinition && AfterDefinition->GetOutermost()->IsDirty());

	for (int32 Index = 0; Index < PreservedPackages.Num(); ++Index)
	{
		const FString Filename = PackageFilename(PreservedPackages[Index]);
		TestEqual(
			*FString::Printf(TEXT("재실행은 %s timestamp를 바꾸지 않는다"),
				*PreservedPackages[Index]),
			IFileManager::Get().GetTimeStamp(*Filename),
			TimestampsBefore[Index]);
		const FMD5Hash HashAfter = FMD5Hash::HashFile(*Filename);
		TestTrue(
			*FString::Printf(TEXT("재실행은 %s 파일 hash를 바꾸지 않는다"),
				*PreservedPackages[Index]),
			HashAfter.IsValid() && HashAfter == HashesBefore[Index]);
	}
	TestEqual(TEXT("재실행은 legacy WBP의 존재 상태를 바꾸지 않는다"),
		FPackageName::DoesPackageExist(LegacyScreenPackage), bLegacyExistedBefore);
	TestEqual(TEXT("재실행은 Edge WBP의 존재 상태를 바꾸지 않는다"),
		FPackageName::DoesPackageExist(EdgeScreenPackage), bEdgeExistedBefore);
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
