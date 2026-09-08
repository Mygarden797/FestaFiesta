// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Facility/FF_FacilityInfoTypes.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Building/FF_FacilityUpgradeTypes.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/BoxComponent.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "FF_FFZSCollision.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NPC/Party/FF_ProgramReservationSubsystem.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "TextureCompiler.h"
#include "RHITypes.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UI/Facility/FF_FacilityInfoSelection.h"
#include "UI/Facility/FF_FacilityInfoSubsystem.h"
#include "UI/Facility/FF_FacilityInfoUpgradeLayout.h"
#include "UI/Facility/FF_FacilityPaidProgramCard.h"
#include "UI/Facility/FF_FacilityPaidProgramPicker.h"
#include "WidgetBlueprint.h"

namespace
{
	UTextureRenderTarget2D* DrawCaptureWidget(
		FWidgetRenderer& Renderer, const TSharedRef<SWidget>& Widget, const FVector2D Size)
	{
		// An explicit sRGB target performs the one display conversion. Renderer must
		// disable shader gamma; its convenience DrawWidget would choose a linear target.
		UTextureRenderTarget2D* Target = FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, true);
		if (Target) Renderer.DrawWidget(Target, Widget, Size, 0.0f, false);
		return Target;
	}

	UTexture2D* LoadCaptureFoodTruckTexture(FAutomationTestBase& Test)
	{
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr,
			TEXT("/Game/FestaFiesta/Zone/Test_Asset/image/T_UI_BuildCatalog_FoodTruck.T_UI_BuildCatalog_FoodTruck"));
		if (!Test.TestNotNull(TEXT("실제 카탈로그 푸드트럭 썸네일"), Texture)) return nullptr;
		TArray<UTexture*> Textures = {Texture};
		FTextureCompilingManager::Get().FinishCompilation(Textures);
		Texture->SetForceMipLevelsToBeResident(30.0f);
		Texture->WaitForStreaming();
		return Texture;
	}

	const TCHAR* FacilityInfoObjectPath =
		TEXT("/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityInfoPanel."
			"WBP_FF_FacilityInfoPanel");
	const TCHAR* FacilityInfoClassPath =
		TEXT("/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityInfoPanel."
			"WBP_FF_FacilityInfoPanel_C");
	const TCHAR* PaidProgramPickerClassPath =
		TEXT("/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityPaidProgramPicker."
			"WBP_FF_FacilityPaidProgramPicker_C");
	const TCHAR* PaidProgramCardClassPath =
		TEXT("/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityPaidProgramCard."
			"WBP_FF_FacilityPaidProgramCard_C");

	class FScopedFacilityInfoPlayerlessWidgetInitialization
	{
	public:
		explicit FScopedFacilityInfoPlayerlessWidgetInitialization(
			UWidgetBlueprintGeneratedClass& InClass)
			: WidgetClass(InClass)
			, bPreviousValue(InClass.bCanCallInitializedWithoutPlayerContext)
		{
			WidgetClass.bCanCallInitializedWithoutPlayerContext = true;
		}

		~FScopedFacilityInfoPlayerlessWidgetInitialization()
		{
			WidgetClass.bCanCallInitializedWithoutPlayerContext = bPreviousValue;
		}

	private:
		UWidgetBlueprintGeneratedClass& WidgetClass;
		bool bPreviousValue;
	};

	class FScopedFacilitySelectionGeometrySettings
	{
	public:
		FScopedFacilitySelectionGeometrySettings()
			: Settings(GetMutableDefault<UFFZSDeveloperSettings>())
			, bGroundChecks(Settings->bEnableGroundChecks)
			, bWorldCollisionChecks(Settings->bEnableWorldCollisionChecks)
			, bNavigationChecks(Settings->bEnableNavigationProjectionChecks)
			, bProductionPolicies(Settings->bRequireProductionPolicyDomains)
			, bRestrictZoneIds(Settings->bRestrictZoneIdsToProjectPresets)
			, InitialPhase(Settings->InitialPhase)
			, MaxEnumeratedCells(Settings->MaxEnumeratedCellsPerFacility)
		{
			Settings->bEnableGroundChecks = false;
			Settings->bEnableWorldCollisionChecks = false;
			Settings->bEnableNavigationProjectionChecks = false;
			Settings->bRequireProductionPolicyDomains = false;
			Settings->bRestrictZoneIdsToProjectPresets = false;
			Settings->InitialPhase = EFFZSChangePhase::InitialSetup;
			Settings->MaxEnumeratedCellsPerFacility = 1'000'000;
		}

		~FScopedFacilitySelectionGeometrySettings()
		{
			Settings->bEnableGroundChecks = bGroundChecks;
			Settings->bEnableWorldCollisionChecks = bWorldCollisionChecks;
			Settings->bEnableNavigationProjectionChecks = bNavigationChecks;
			Settings->bRequireProductionPolicyDomains = bProductionPolicies;
			Settings->bRestrictZoneIdsToProjectPresets = bRestrictZoneIds;
			Settings->InitialPhase = InitialPhase;
			Settings->MaxEnumeratedCellsPerFacility = MaxEnumeratedCells;
		}

	private:
		UFFZSDeveloperSettings* Settings;
		bool bGroundChecks;
		bool bWorldCollisionChecks;
		bool bNavigationChecks;
		bool bProductionPolicies;
		bool bRestrictZoneIds;
		EFFZSChangePhase InitialPhase;
		int32 MaxEnumeratedCells;
	};

	UBoxComponent* AddSelectionTestBox(
		AActor& Owner,
		const FName ComponentName,
		const FVector& WorldLocation,
		const FVector& Extent,
		const ECollisionChannel BlockingChannel)
	{
		UBoxComponent* Box = NewObject<UBoxComponent>(&Owner, ComponentName);
		Owner.SetRootComponent(Box);
		Box->SetBoxExtent(Extent);
		Box->SetWorldLocation(WorldLocation);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionObjectType(ECC_WorldDynamic);
		Box->SetCollisionResponseToAllChannels(ECR_Ignore);
		Box->SetCollisionResponseToChannel(BlockingChannel, ECR_Block);
		Box->SetGenerateOverlapEvents(false);
		Box->RegisterComponent();
		return Box;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FacilityInfoFormattingTest,
	"FestaFiesta.UI.FacilityInfo.Formatting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityInfoFormattingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TestEqual(TEXT("부스 분류는 제품용 한글로 표시된다"),
		FFFacilityInfoModel::GetCategoryText(EFFZSFacilityCategory::Booth).ToString(),
		FString(TEXT("부스")));
	TestEqual(TEXT("운영 시설 상태는 제품용 한글로 표시된다"),
		FFFacilityInfoModel::GetFacilityStateText(EFFZSFacilityState::Active).ToString(),
		FString(TEXT("운영 중")));
	TestEqual(TEXT("가격 실패는 0 Gold로 오해되지 않는다"),
		FFFacilityInfoModel::FormatGold(false, 0).ToString(),
		FString(TEXT("가격 정보 없음")));
	TestEqual(TEXT("즉시 이용 가능한 Queue는 대기 0분 대신 의미를 표시한다"),
		FFFacilityInfoModel::FormatExpectedWait(true, 0.0f).ToString(),
		FString(TEXT("바로 이용 가능")));
	TestEqual(TEXT("Unreachable sentinel은 24시간으로 노출하지 않는다"),
		FFFacilityInfoModel::FormatExpectedWait(true, 86400.0f).ToString(),
		FString(TEXT("계산 불가")));
	TestEqual(TEXT("61초는 과소 예측하지 않고 2분으로 올림한다"),
		FFFacilityInfoModel::FormatExpectedWait(true, 61.0f).ToString(),
		FString(TEXT("약 2분")));
	TestEqual(TEXT("미등록 Queue 기본값을 열린 시설로 표시하지 않는다"),
		FFFacilityInfoModel::GetOperationStateText(true, false, true, true).ToString(),
		FString(TEXT("운영 정보 준비 중")));
	TestEqual(TEXT("Queue 시스템이 없는 월드는 영구 로딩 대신 사용 불가로 표시한다"),
		FFFacilityInfoModel::GetOperationStateText(false, false, true, true).ToString(),
		FString(TEXT("운영 정보 사용 불가")));
	TestEqual(TEXT("프로그램 시간은 하드코딩된 절대 시각 대신 개장 상대 시간으로 표시한다"),
		FFFacilityInfoModel::FormatProgramTimeRange(30.0f, 60.0f).ToString(),
		FString(TEXT("개장 +30분 ~ +60분")));
	TestEqual(TEXT("프로그램 요약은 판단에 필요한 이름·시간·비용을 함께 가진다"),
		FFFacilityInfoModel::FormatScheduledProgramSummary(
			FText::FromString(TEXT("바다향기 공방")), 30.0f, 60.0f, true, 200).ToString(),
		FString(TEXT("바다향기 공방 · 개장 +30분 ~ +60분 · 비용 200 Gold")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FacilityInfoUpgradeReadModelTest,
	"FestaFiesta.UI.FacilityInfo.UpgradeReadModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityInfoUpgradeReadModelTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FFF_FacilityInfoViewData ViewData;
	ViewData.Context = EFF_FacilityInfoContext::Build;
	FFF_FacilityUpgradePreview Preview;
	Preview.CurrentLevel = 1;
	Preview.TargetLevel = 2;
	Preview.MaxLevel = 3;
	Preview.bHasUpgradeQuote = true;
	Preview.bSupported = true;
	Preview.bHasLevels = true;
	Preview.bCanUpgrade = true;
	Preview.CostGold = 120;
	Preview.GoldAfterUpgrade = 880;
	Preview.CurrentServiceDurationSeconds = 4.0f;
	Preview.NextServiceDurationSeconds = 3.2f;
	Preview.CurrentThroughputPerMinute = 15.0f;
	Preview.NextThroughputPerMinute = 18.75f;
	Preview.CurrentFootprintCentimeters = FIntPoint(400, 400);
	Preview.NextFootprintCentimeters = FIntPoint(600, 600);
	Preview.bRevenueAvailable = true;
	Preview.BaseRevenuePerUseGold = 10;
	Preview.CurrentRevenuePerUseGold = 10;
	Preview.NextRevenuePerUseGold = 16;
	Preview.NextRevenueMultiplier = 1.5f;
	FFFacilityInfoModel::ApplyUpgradePreview(&Preview, ViewData);
	TestTrue(TEXT("준비 중 유효한 견적은 상세 정보와 구매 버튼을 활성화한다"),
		ViewData.bShowUpgradeAction && ViewData.bShowUpgradeDetails && ViewData.bCanUpgrade);
	TestEqual(TEXT("구매 검증에 사용할 확인 당시 레벨을 보관한다"), ViewData.CurrentUpgradeLevel, 1);
	TestTrue(TEXT("효과는 전후 값을 화살표로 비교한다"),
		ViewData.UpgradeEffectText.ToString().Contains(TEXT("4.0초 → 3.2초")));
	TestTrue(TEXT("현재와 다음 비교 카드가 같은 견적의 서로 다른 값을 표시한다"),
		ViewData.UpgradeCurrentText.ToString().Contains(TEXT("10 Gold")) &&
		ViewData.UpgradeCurrentText.ToString().Contains(TEXT("4 × 4 m")) &&
		ViewData.UpgradeNextText.ToString().Contains(TEXT("16 Gold")) &&
		ViewData.UpgradeNextText.ToString().Contains(TEXT("6 × 6 m")));
	TestTrue(TEXT("강화로 달라지는 시설 크기를 구매 전에 표시한다"),
		ViewData.UpgradeSizeEffectText.ToString().Contains(TEXT("4 × 4 m → 6 × 6 m")));
	TestTrue(TEXT("비용과 구매 후 잔액을 함께 표시한다"),
		ViewData.UpgradeCostText.ToString().Contains(TEXT("120 Gold")) &&
		ViewData.UpgradeCostText.ToString().Contains(TEXT("880 Gold")));
	TestTrue(TEXT("현재 수익과 기본 대비 증가액을 표시한다"),
		ViewData.FacilityRevenueText.ToString().Contains(TEXT("10 Gold")) &&
		ViewData.FacilityRevenueText.ToString().Contains(TEXT("기본 대비 0 Gold")));
	TestTrue(TEXT("수익 증가율은 반올림된 표시 금액에 일치한다"),
		ViewData.UpgradeRevenueEffectText.ToString().Contains(TEXT("10 → 16 Gold")) &&
		ViewData.UpgradeRevenueEffectText.ToString().Contains(TEXT("+6 Gold (+60%)")) &&
		ViewData.UpgradeRevenueEffectText.ToString().Contains(TEXT("×1.5")));

	Preview.bCanUpgrade = false;
	Preview.GoldAfterUpgrade = -40;
	Preview.FailureReason = FText::FromString(TEXT("골드가 부족합니다."));
	FFFacilityInfoModel::ApplyUpgradePreview(&Preview, ViewData);
	TestFalse(TEXT("실패 견적은 구매를 막는다"), ViewData.bCanUpgrade);
	TestTrue(TEXT("골드가 부족해도 효과와 가격은 비교할 수 있다"), ViewData.bShowUpgradeDetails);
	TestTrue(TEXT("부족한 골드는 가상의 구매 후 잔액 대신 부족액으로 표시한다"),
		ViewData.UpgradeCostText.ToString().Contains(TEXT("부족한 금액  40 Gold")) &&
		!ViewData.UpgradeCostText.ToString().Contains(TEXT("구매 후 잔액")));
	TestEqual(TEXT("권한 계층의 실패 사유를 표시한다"),
		ViewData.UpgradeDisabledReasonText.ToString(), Preview.FailureReason.ToString());

	ViewData.Context = EFF_FacilityInfoContext::Live;
	FFFacilityInfoModel::ApplyUpgradePreview(&Preview, ViewData);
	TestFalse(TEXT("운영 중에는 구매 동작을 숨긴다"), ViewData.bShowUpgradeAction);
	TestFalse(TEXT("운영 중에는 다음 단계 구매 상세를 숨긴다"), ViewData.bShowUpgradeDetails);
	TestTrue(TEXT("운영 전환 시 이전 비교 카드도 지운다"),
		ViewData.UpgradeCurrentText.IsEmpty() && ViewData.UpgradeNextText.IsEmpty());
	TestTrue(TEXT("운영 중에도 레벨이 보인다"), !ViewData.FacilityLevelText.IsEmpty());
	TestTrue(TEXT("운영 상태로 전환하면 이전 실패 사유를 지운다"), ViewData.UpgradeDisabledReasonText.IsEmpty());

	ViewData.Context = EFF_FacilityInfoContext::Build;
	Preview.CostGold = 0;
	Preview.bHasUpgradeQuote = false;
	FFFacilityInfoModel::ApplyUpgradePreview(&Preview, ViewData);
	TestFalse(TEXT("가격 조회가 실패하면 0 Gold 구매 견적을 표시하지 않는다"), ViewData.bShowUpgradeDetails);
	TestTrue(TEXT("가격 조회 실패 시 이전 견적을 지운다"), ViewData.UpgradeCostText.IsEmpty());
	Preview.bHasUpgradeQuote = true;
	FFFacilityInfoModel::ApplyUpgradePreview(&Preview, ViewData);
	TestTrue(TEXT("정상 무료 견적은 0 Gold여도 상세를 표시한다"), ViewData.bShowUpgradeDetails);
	TestTrue(TEXT("무료 구매 버튼을 명시한다"), ViewData.UpgradeActionText.ToString().Contains(TEXT("무료")));
	Preview.CostGold = 120;
	Preview.NextServiceDurationSeconds = Preview.CurrentServiceDurationSeconds;
	FFFacilityInfoModel::ApplyUpgradePreview(&Preview, ViewData);
	TestTrue(TEXT("속도가 같아도 수익이 늘면 다음 강화 견적을 표시한다"), ViewData.bShowUpgradeDetails);
	Preview.NextRevenueMultiplier = Preview.CurrentRevenueMultiplier;
	FFFacilityInfoModel::ApplyUpgradePreview(&Preview, ViewData);
	TestTrue(TEXT("외형만 바뀌거나 효과가 같은 단계도 견적을 표시한다"), ViewData.bShowUpgradeDetails);
	Preview.CurrentRevenuePerUseGold = 5;
	Preview.NextRevenuePerUseGold = 2;
	Preview.NextRevenueMultiplier = .2f;
	Preview.NextServiceDurationSeconds = 8.f;
	Preview.TargetLevel = 15;
	Preview.MaxLevel = 100;
	FFFacilityInfoModel::ApplyUpgradePreview(&Preview, ViewData);
	TestTrue(TEXT("수익 감소와 느려짐, 레벨 건너뛰기도 견적을 표시한다"), ViewData.bShowUpgradeDetails);
	TestTrue(TEXT("기본보다 줄어든 현재 수익은 음수 차액으로 표시한다"),
		ViewData.FacilityRevenueText.ToString().Contains(TEXT("기본 대비 -5 Gold")));
	TestTrue(TEXT("다음 수익 감소액과 감소율은 부호가 한 번만 표시된다"),
		ViewData.UpgradeRevenueEffectText.ToString().Contains(TEXT("-3 Gold (-60%)")) &&
		!ViewData.UpgradeRevenueEffectText.ToString().Contains(TEXT("+-")));
	TestTrue(TEXT("비교 카드도 수익 감소와 더 느린 시간을 실제 값으로 표시한다"),
		ViewData.UpgradeNextText.ToString().Contains(TEXT("2 Gold")) &&
		ViewData.UpgradeNextText.ToString().Contains(TEXT("8.0초")));
	Preview.CurrentLevel = Preview.MaxLevel;
	Preview.TargetLevel = Preview.MaxLevel;
	FFFacilityInfoModel::ApplyUpgradePreview(&Preview, ViewData);
	TestFalse(TEXT("최대 레벨에서 가상의 다음 단계 가격을 보여주지 않는다"), ViewData.bShowUpgradeDetails);
	TestEqual(TEXT("최대 레벨 버튼은 이유를 명시한다"),
		ViewData.UpgradeActionText.ToString(), FString(TEXT("최대 레벨")));
	FFFacilityInfoModel::ApplyUpgradePreview(nullptr, ViewData);
	TestFalse(TEXT("정보를 잃으면 이전 구매 가능 상태를 유지하지 않는다"), ViewData.bCanUpgrade);
	TestTrue(TEXT("정보를 잃으면 이전 견적을 지운다"), ViewData.UpgradeCostText.IsEmpty());
	TestTrue(TEXT("정보를 잃으면 이전 전후 수치를 지운다"),
		ViewData.UpgradeCurrentText.IsEmpty() && ViewData.UpgradeNextText.IsEmpty());
	Preview.bHasLevels = false;
	Preview.CurrentRevenuePerUseGold = 10;
	FFFacilityInfoModel::ApplyUpgradePreview(&Preview, ViewData);
	TestFalse(TEXT("레벨 없는 시설은 강화 섹션을 표시하지 않는다"), ViewData.bHasFacilityLevels);
	TestTrue(TEXT("레벨 없는 시설은 Lv.1 같은 가상의 등급도 표시하지 않는다"), ViewData.FacilityLevelText.IsEmpty());
	TestFalse(TEXT("레벨 없는 시설은 구매 동작도 제공하지 않는다"), ViewData.bShowUpgradeAction);
	TestTrue(TEXT("레벨 없는 시설에도 현재 수익을 표시한다"), ViewData.FacilityRevenueText.ToString().Contains(TEXT("10 Gold")));
	Preview.CurrentLevel = 1;
	Preview.TargetLevel = 2;
	Preview.bHasLevels = true;
	Preview.NextRevenueMultiplier = 1.5f;
	Preview.BaseRevenuePerUseGold = 0;
	Preview.CurrentRevenuePerUseGold = 0;
	Preview.NextRevenuePerUseGold = 0;
	FFFacilityInfoModel::ApplyUpgradePreview(&Preview, ViewData);
	TestTrue(TEXT("무료 시설은 현재 수익 0을 표시한다"), ViewData.FacilityRevenueText.ToString().Contains(TEXT("0 Gold")));
	TestFalse(TEXT("현재 수익 0일 때 증가율을 나눗셈으로 만들지 않는다"), ViewData.UpgradeRevenueEffectText.ToString().Contains(TEXT("%")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FacilityInfoProgramReadModelTest,
	"FestaFiesta.UI.FacilityInfo.ProgramReadModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityInfoProgramReadModelTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FScopedFacilitySelectionGeometrySettings SettingsGuard;
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}

	UWorld* World = TestWorld.GetTestWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UFFZSWorldSubsystem* ZoneSystem = World
		? World->GetSubsystem<UFFZSWorldSubsystem>()
		: nullptr;
	UFF_FacilityInfoSubsystem* Coordinator = World
		? World->GetSubsystem<UFF_FacilityInfoSubsystem>()
		: nullptr;
	UFF_FestivalProgressionSubsystem* Progression = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>()
		: nullptr;
	UFF_ProgramReservationSubsystem* Reservations = GameInstance
		? GameInstance->GetSubsystem<UFF_ProgramReservationSubsystem>()
		: nullptr;
	UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>()
		: nullptr;
	if (!TestNotNull(TEXT("프로그램 read model Game World"), World) ||
		!TestNotNull(TEXT("프로그램 read model GameInstance"), GameInstance) ||
		!TestNotNull(TEXT("프로그램 read model ZoneSystem"), ZoneSystem) ||
		!TestNotNull(TEXT("프로그램 read model FacilityInfoSubsystem"), Coordinator) ||
		!TestNotNull(TEXT("프로그램 read model Progression"), Progression) ||
		!TestNotNull(TEXT("프로그램 read model Reservations"), Reservations) ||
		!TestNotNull(TEXT("프로그램 read model Stage"), Stage))
	{
		return false;
	}

	const FName ZoneId(TEXT("GZ_DadaPocha"));
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>(
		AFFZSGameplayZone::StaticClass(), FTransform::Identity, SpawnParameters);
	if (!TestNotNull(TEXT("프로그램 read model Gameplay Zone"), Zone) ||
		!TestTrue(TEXT("프로그램 read model Zone preset"), Zone->ApplyAcceptedZonePreset(ZoneId)) ||
		!TestTrue(TEXT("프로그램 read model Zone 등록"), ZoneSystem->RegisterZone(Zone)))
	{
		return false;
	}

	UFF_FestivalDefinition* Definition = NewObject<UFF_FestivalDefinition>(GameInstance);
	UDataTable* CostTable = NewObject<UDataTable>(GameInstance);
	if (!TestNotNull(TEXT("프로그램 read model FestivalDefinition"), Definition) ||
		!TestNotNull(TEXT("프로그램 read model CostTable"), CostTable))
	{
		return false;
	}
	CostTable->RowStruct = FFF_FacilityCostRow::StaticStruct();
	FFF_FacilityCostRow CostRow;
	CostRow.InstallCost = 100;
	CostTable->AddRow(TEXT("Cost.Booth.CraftExperience.FacilityInfoProgramTest"), CostRow);
	Definition->FestivalId = TEXT("FacilityInfoProgramReadModelFestival");
	Definition->FacilityCostTable = CostTable;
	Definition->DailyVenueRentGold = 0;
	Progression->SetProgramReservations(Reservations);
	Progression->StartFestival(Definition);

	FFFZSFacilitySpec FacilitySpec =
		FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
	FacilitySpec.FacilityDefinitionId = TEXT("Booth_CraftExperience_FacilityInfoProgramTest");
	FacilitySpec.DisplayName = FText::FromString(TEXT("프로그램 테스트 공방"));
	FacilitySpec.Category = EFFZSFacilityCategory::Booth;
	FacilitySpec.InstallCostKey = NAME_None;
	FFFZSGridPlacement Placement;
	Placement.OriginCell = FIntPoint(1000, 1000);
	const FFFZSCommand InstallCommand = UFFZSGridLibrary::MakeInstallCommand(
		FacilitySpec,
		ZoneId,
		Placement,
		ZoneSystem->GetWorldRevision());
	const FFFZSCommandResult InstallResult = ZoneSystem->ExecuteCommand(InstallCommand);
	AFFZSFacilityActor* Facility = InstallResult.bSucceeded
		? ZoneSystem->FindFacility(InstallCommand.FacilityInstanceId)
		: nullptr;
	const FString InstallDiagnostic = FString::Printf(
		TEXT("프로그램 호환 시설 설치 (FailureReason=%d, Message=%s)"),
		static_cast<int32>(InstallResult.FailureReason),
		*InstallResult.Message.ToString());
	if (!TestTrue(*InstallDiagnostic, InstallResult.bSucceeded) ||
		!TestNotNull(TEXT("프로그램 호환 시설 조회"), Facility))
	{
		return false;
	}

	Coordinator->SelectionContext = EFF_FacilityInfoContext::Build;
	Stage->CurrentStage = EFF_FestivalStage::Setup;
	FFF_FacilityInfoViewData ViewData;
	if (!TestTrue(TEXT("Setup 시설 정보를 평탄화한다"),
		Coordinator->BuildViewData(*Facility, ViewData)))
	{
		return false;
	}
	TestTrue(TEXT("Setup Build 시설은 프로그램 섹션을 표시한다"), ViewData.bShowProgramSection);
	TestTrue(TEXT("Setup Build 시설은 편성 CTA를 표시한다"), ViewData.bShowProgramScheduleAction);
	TestTrue(TEXT("호환 프로그램과 자원이 있으면 CTA가 활성화된다"), ViewData.bCanScheduleProgram);
	TestTrue(TEXT("활성 CTA에는 실패 이유가 없다"), ViewData.ProgramScheduleDisabledReasonText.IsEmpty());

	Stage->CurrentStage = EFF_FestivalStage::PromotionPreview;
	TestTrue(TEXT("PromotionPreview 시설 정보를 평탄화한다"),
		Coordinator->BuildViewData(*Facility, ViewData));
	TestTrue(TEXT("PromotionPreview에서도 내역 섹션은 읽을 수 있다"), ViewData.bShowProgramSection);
	TestTrue(TEXT("PromotionPreview CTA는 위치를 유지한다"), ViewData.bShowProgramScheduleAction);
	TestFalse(TEXT("PromotionPreview CTA는 비활성화된다"), ViewData.bCanScheduleProgram);
	TestTrue(TEXT("PromotionPreview 비활성 이유를 명시한다"),
		ViewData.ProgramScheduleDisabledReasonText.ToString().Contains(TEXT("단계")));

	Stage->CurrentStage = EFF_FestivalStage::Setup;
	TestEqual(TEXT("시설 GUID와 함께 공방 프로그램을 편성한다"),
		Progression->TryScheduleProgram(
			TEXT("Program_SandArtWorkshop"), InstallCommand.FacilityInstanceId),
		EFF_ProgramPurchaseResult::Success);
	TestTrue(TEXT("편성 뒤 시설 정보를 다시 평탄화한다"),
		Coordinator->BuildViewData(*Facility, ViewData));
	TestTrue(TEXT("예약 핸들로 현재 시설의 편성을 찾는다"), ViewData.bHasScheduledProgram);
	TestTrue(TEXT("편성 요약에 프로그램 이름이 있다"),
		ViewData.ScheduledProgramSummaryText.ToString().Contains(TEXT("바다향기 공방")));
	TestTrue(TEXT("편성 요약에 상대 시간이 있다"),
		ViewData.ScheduledProgramSummaryText.ToString().Contains(TEXT("개장 +30분 ~ +60분")));
	TestTrue(TEXT("편성 요약에 구매 비용이 있다"),
		ViewData.ScheduledProgramSummaryText.ToString().Contains(TEXT("200 Gold")));
	TestTrue(TEXT("편성 완료 카드도 상세 내역을 다시 볼 수 있도록 CTA는 열린다"),
		ViewData.bCanScheduleProgram);
	TestTrue(TEXT("카드별 편성 완료 사유는 Picker에서 표시하므로 패널 사유는 비운다"),
		ViewData.ProgramScheduleDisabledReasonText.IsEmpty());

	Coordinator->SelectionContext = EFF_FacilityInfoContext::Live;
	Stage->CurrentStage = EFF_FestivalStage::Live;
	TestTrue(TEXT("Live 시설 정보를 평탄화한다"),
		Coordinator->BuildViewData(*Facility, ViewData));
	TestTrue(TEXT("Live에서도 오늘 편성 요약은 읽을 수 있다"), ViewData.bShowProgramSection);
	TestFalse(TEXT("Live에서는 편성 CTA를 숨긴다"), ViewData.bShowProgramScheduleAction);
	TestFalse(TEXT("숨긴 Live CTA는 활성 상태를 갖지 않는다"), ViewData.bCanScheduleProgram);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FacilityInfoSelectionTraceDecisionTest,
	"FestaFiesta.UI.FacilityInfo.SelectionTraceDecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityInfoSelectionTraceDecisionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using FFFacilityInfoSelection::ETraceDecision;
	using FFFacilityInfoSelection::ResolveTraceDecision;

	TestEqual(TEXT("선택 proxy가 없을 때만 footprint 호환 경로를 사용한다"),
		ResolveTraceDecision(false, false, false), ETraceDecision::UseFootprintFallback);
	TestEqual(TEXT("등록된 proxy가 보이면 proxy 시설을 즉시 사용한다"),
		ResolveTraceDecision(true, true, false), ETraceDecision::UseSelectionProxy);
	TestEqual(TEXT("외부 장애물에 가려진 proxy는 footprint로 관통 선택하지 않는다"),
		ResolveTraceDecision(true, true, true), ETraceDecision::Reject);
	TestEqual(TEXT("전용 채널의 비정상 후보도 footprint fallback을 열지 않는다"),
		ResolveTraceDecision(true, false, false), ETraceDecision::Reject);

	using FFFacilityInfoSelection::ShouldResolveLiveSelectionHover;
	TestTrue(TEXT("Live에서 피킹 가능하고 커서가 월드 위에 있으면 Hover를 판정한다"),
		ShouldResolveLiveSelectionHover(true, true, true, false));
	TestFalse(TEXT("PromotionPreview 등 Live가 아닌 Stage에서는 표시하지 않는다"),
		ShouldResolveLiveSelectionHover(false, true, true, false));
	TestFalse(TEXT("건축·모달·ActorView처럼 피킹이 막히면 표시하지 않는다"),
		ShouldResolveLiveSelectionHover(true, false, true, false));
	TestFalse(TEXT("월드 커서가 숨겨진 입력 모드에서는 표시하지 않는다"),
		ShouldResolveLiveSelectionHover(true, true, false, false));
	TestFalse(TEXT("정보 패널 위에서는 뒤쪽 시설 Hover를 표시하지 않는다"),
		ShouldResolveLiveSelectionHover(true, true, true, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FacilityInfoSelectionGeometryTest,
	"FestaFiesta.UI.FacilityInfo.SelectionGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityInfoSelectionGeometryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FScopedFacilitySelectionGeometrySettings SettingsGuard;
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}

	UWorld* World = TestWorld.GetTestWorld();
	UFFZSWorldSubsystem* ZoneSystem = World
		? World->GetSubsystem<UFFZSWorldSubsystem>()
		: nullptr;
	UFF_FacilityInfoSubsystem* Coordinator = World
		? World->GetSubsystem<UFF_FacilityInfoSubsystem>()
		: nullptr;
	if (!TestNotNull(TEXT("선택 기하 Game World"), World) ||
		!TestNotNull(TEXT("선택 기하 ZoneSystem"), ZoneSystem) ||
		!TestNotNull(TEXT("선택 기하 FacilityInfoSubsystem"), Coordinator))
	{
		return false;
	}

	const FName ZoneId(TEXT("GZ_DadaPocha"));
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>(
		AFFZSGameplayZone::StaticClass(), FTransform::Identity, SpawnParameters);
	if (!TestNotNull(TEXT("선택 기하 Gameplay Zone"), Zone) ||
		!TestTrue(TEXT("선택 기하 Zone preset"), Zone->ApplyAcceptedZonePreset(ZoneId)) ||
		!TestTrue(TEXT("선택 기하 Zone 등록"), ZoneSystem->RegisterZone(Zone)))
	{
		return false;
	}

	FFFZSGridPlacement Placement;
	Placement.OriginCell = FIntPoint(1000, 1000);
	Placement.QuarterTurns = 1;
	FFFZSFacilitySpec FacilitySpec =
		FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
	// Game World에서는 프로젝트 Economy 정책도 함께 동작하므로 플러그인 범용 fixture의
	// 레거시 가격 키 대신 실제 DT_FacilityCosts에 존재하는 프로젝트 행을 사용한다.
	FacilitySpec.InstallCostKey = TEXT("Cost.Booth.FoodStall.DadaPochaFoodTruck");
	const FFFZSCommand InstallCommand = UFFZSGridLibrary::MakeInstallCommand(
		FacilitySpec,
		ZoneId,
		Placement,
		ZoneSystem->GetWorldRevision());
	const FFFZSCommandResult InstallResult = ZoneSystem->ExecuteCommand(InstallCommand);
	const FString InstallDiagnostic = FString::Printf(
		TEXT("90도 회전 시설 설치 (FailureReason=%d, Message=%s)"),
		static_cast<int32>(InstallResult.FailureReason),
		*InstallResult.Message.ToString());
	if (!TestTrue(*InstallDiagnostic, InstallResult.bSucceeded))
	{
		return false;
	}

	AFFZSFacilityActor* Facility = ZoneSystem->FindFacility(InstallCommand.FacilityInstanceId);
	if (!TestNotNull(TEXT("선택 기하 등록 시설"), Facility) ||
		!TestNotNull(TEXT("선택 기하 실제 proxy"), Facility ? Facility->SelectionProxy.Get() : nullptr))
	{
		return false;
	}

	UBoxComponent* Proxy = Facility->SelectionProxy;
	const FVector ProxyExtent = Proxy->GetUnscaledBoxExtent();
	const FTransform ProxyTransform = Proxy->GetComponentTransform();
	auto MakeVerticalRayOrigin = [&ProxyTransform, &ProxyExtent](const float LocalX)
	{
		const FVector Target = ProxyTransform.TransformPosition(FVector(LocalX, 0.0f, 0.0f));
		return FVector(Target.X, Target.Y, ProxyTransform.GetLocation().Z + ProxyExtent.Z + 1000.0f);
	};
	const FVector RayDirection = FVector::DownVector;
	const FVector InsideRayOrigin = MakeVerticalRayOrigin(ProxyExtent.X - 1.0f);
	const FVector OutsideRayOrigin = MakeVerticalRayOrigin(ProxyExtent.X + 1.0f);

	TestNull(TEXT("선택 여백은 권위 footprint 바깥이라 구형 지면 좌표 조회로는 시설을 놓친다"),
		ZoneSystem->FindFacilityAtWorldPoint(InsideRayOrigin, ZoneId));
	TestEqual(TEXT("Build/Live 공용 resolver는 회전된 padded proxy 안쪽 1cm를 같은 시설로 판정한다"),
		Coordinator->ResolveFacilityAlongRay(
			InsideRayOrigin, RayDirection, nullptr),
		Facility);
	TestNull(TEXT("회전된 표시 proxy 바깥쪽 1cm는 선택하지 않는다"),
		Coordinator->ResolveFacilityAlongRay(
			OutsideRayOrigin, RayDirection, nullptr));
	TestEqual(TEXT("현재 Hover 표시 대상은 현재 ray와 교차하면 클릭 대상으로 유지한다"),
		Coordinator->ResolveFacilityAlongRay(
			InsideRayOrigin, RayDirection, Facility),
		Facility);

	FFFZSGridPlacement SecondPlacement = Placement;
	SecondPlacement.OriginCell = FIntPoint(5000, 1000);
	const FFFZSCommand SecondInstallCommand = UFFZSGridLibrary::MakeInstallCommand(
		FacilitySpec,
		ZoneId,
		SecondPlacement,
		ZoneSystem->GetWorldRevision());
	const FFFZSCommandResult SecondInstallResult = ZoneSystem->ExecuteCommand(SecondInstallCommand);
	AFFZSFacilityActor* SecondFacility = SecondInstallResult.bSucceeded
		? ZoneSystem->FindFacility(SecondInstallCommand.FacilityInstanceId)
		: nullptr;
	if (TestTrue(TEXT("겹친 Hover 회귀용 두 번째 시설 설치"), SecondInstallResult.bSucceeded) &&
		TestNotNull(TEXT("겹친 Hover 회귀용 두 번째 시설"), SecondFacility))
	{
		const FTransform SecondOriginalTransform = SecondFacility->GetActorTransform();
		const FTransform SharedTransform = Facility->GetActorTransform();
		SecondFacility->SetActorTransform(
			SharedTransform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);

		FHitResult RawOverlapHit;
		FCollisionQueryParams RawOverlapParams(SCENE_QUERY_STAT(FF_FacilityInfoOverlapPrecondition), false);
		const bool bRawOverlapHit = World->LineTraceSingleByChannel(
			RawOverlapHit,
			InsideRayOrigin,
			InsideRayOrigin + RayDirection * 500000.0f,
			FFZSCollision::FacilitySelectionTraceChannel,
			RawOverlapParams);
		AFFZSFacilityActor* FirstOverlapFacility = bRawOverlapHit
			? Cast<AFFZSFacilityActor>(RawOverlapHit.GetActor())
			: nullptr;
		AFFZSFacilityActor* PreferredOverlapFacility = FirstOverlapFacility == Facility
			? SecondFacility
			: Facility;
		AFFZSFacilityActor* OtherOverlapFacility = PreferredOverlapFacility == Facility
			? SecondFacility
			: Facility;
		if (TestTrue(TEXT("겹친 두 selection proxy 중 하나가 raw first hit다"),
			FirstOverlapFacility == Facility || FirstOverlapFacility == SecondFacility))
		{
			Coordinator->ApplyLiveSelectionIndicatorTargets(PreferredOverlapFacility, nullptr);
			TestEqual(TEXT("클릭 우선 대상은 실제 Hovered 표시 상태다"),
				PreferredOverlapFacility->GetSelectionIndicatorState(),
				EFFZSFacilitySelectionIndicatorState::Hovered);
			TestEqual(TEXT("겹친 proxy에서도 화면에 Hover된 시설을 클릭한다"),
				Coordinator->ResolveFacilityAlongRay(
					InsideRayOrigin,
					RayDirection,
					Coordinator->SelectionIndicatorHoverFacility.Get()),
				PreferredOverlapFacility);

			PreferredOverlapFacility->SetActorTransform(
				SecondOriginalTransform,
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
			AFFZSFacilityActor* ResolvedAfterHoverMoved =
				Coordinator->ResolveFacilityAlongRay(
					InsideRayOrigin,
					RayDirection,
					Coordinator->SelectionIndicatorHoverFacility.Get());
			TestEqual(TEXT("stale Hover가 현재 ray를 벗어나면 실제 교차 시설로 즉시 전환한다"),
				ResolvedAfterHoverMoved,
				OtherOverlapFacility);
			Coordinator->ApplyLiveSelectionIndicatorTargets(ResolvedAfterHoverMoved, nullptr);
			TestEqual(TEXT("클릭 동기 재판정은 이전 Hover 표시를 즉시 숨긴다"),
				PreferredOverlapFacility->GetSelectionIndicatorState(),
				EFFZSFacilitySelectionIndicatorState::Hidden);
			TestEqual(TEXT("클릭 동기 재판정은 실제 교차 시설을 Hovered로 갱신한다"),
				OtherOverlapFacility->GetSelectionIndicatorState(),
				EFFZSFacilitySelectionIndicatorState::Hovered);
			PreferredOverlapFacility->SetActorTransform(
				SharedTransform,
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
		}
		SecondFacility->SetActorTransform(
			SecondOriginalTransform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		Coordinator->ClearLiveSelectionIndicators();
	}

	AActor* InvalidFrontActor = World->SpawnActor<AActor>(
		AActor::StaticClass(), FTransform::Identity, SpawnParameters);
	if (TestNotNull(TEXT("앞쪽 stale proxy 테스트 Actor"), InvalidFrontActor))
	{
		AddSelectionTestBox(
			*InvalidFrontActor,
			TEXT("InvalidSelectionProxy"),
			InsideRayOrigin - FVector(0.0f, 0.0f, 300.0f),
			FVector(50.0f),
			FFZSCollision::FacilitySelectionTraceChannel);
		FHitResult InvalidFirstHit;
		FCollisionQueryParams InvalidFirstParams(
			SCENE_QUERY_STAT(FF_FacilityInfoInvalidProxyPrecondition), false);
		const bool bInvalidFirstHit = World->LineTraceSingleByChannel(
			InvalidFirstHit,
			InsideRayOrigin,
			InsideRayOrigin + RayDirection * 500000.0f,
			FFZSCollision::FacilitySelectionTraceChannel,
			InvalidFirstParams);
		if (TestTrue(TEXT("비등록 proxy가 실제 raw first hit다"),
			bInvalidFirstHit && InvalidFirstHit.GetActor() == InvalidFrontActor))
		{
			TestEqual(TEXT("앞쪽 비등록 proxy를 건너뛰고 뒤의 등록 시설을 선택한다"),
				Coordinator->ResolveFacilityAlongRay(
					InsideRayOrigin, RayDirection, nullptr),
				Facility);
		}
		InvalidFrontActor->Destroy();
	}

	FHitResult ExactProxyHit;
	FCollisionQueryParams ExactProxyParams(SCENE_QUERY_STAT(FF_FacilityInfoTolerancePrecondition), false);
	if (TestTrue(TEXT("가림 끝점 허용 테스트가 실제 proxy 표면을 찾는다"),
		Proxy->LineTraceComponent(
			ExactProxyHit,
			InsideRayOrigin,
			InsideRayOrigin + RayDirection * 500000.0f,
			ExactProxyParams)))
	{
		const float ProxyHitDistance = FVector::DotProduct(
			ExactProxyHit.ImpactPoint - InsideRayOrigin,
			RayDirection);
		AActor* ContactActor = World->SpawnActor<AActor>(
			AActor::StaticClass(), FTransform::Identity, SpawnParameters);
		if (TestNotNull(TEXT("proxy 접촉 허용 테스트 Actor"), ContactActor))
		{
			UBoxComponent* ContactBox = AddSelectionTestBox(
				*ContactActor,
				TEXT("ProxyContactTolerance"),
				InsideRayOrigin + RayDirection * (ProxyHitDistance - 1.0f),
				FVector(10.0f, 10.0f, 0.25f),
				ECC_Visibility);
			TestEqual(TEXT("proxy 표면 2cm 이내 접촉 오차는 클릭을 막지 않는다"),
				Coordinator->ResolveFacilityAlongRay(
					InsideRayOrigin, RayDirection, Facility),
				Facility);
			ContactBox->SetWorldLocation(
				InsideRayOrigin + RayDirection * (ProxyHitDistance - 4.0f));
			TestNull(TEXT("proxy 표면보다 2cm 이상 앞선 실제 blocker는 클릭을 막는다"),
				Coordinator->ResolveFacilityAlongRay(
					InsideRayOrigin, RayDirection, Facility));
			ContactActor->Destroy();
		}
	}

	AActor* VisibilityBlocker = World->SpawnActor<AActor>(
		AActor::StaticClass(), FTransform::Identity, SpawnParameters);
	if (TestNotNull(TEXT("외부 가림 테스트 Actor"), VisibilityBlocker))
	{
		AddSelectionTestBox(
			*VisibilityBlocker,
			TEXT("VisibilityBlocker"),
			InsideRayOrigin - FVector(0.0f, 0.0f, 300.0f),
			FVector(50.0f),
			ECC_Visibility);
		TestNull(TEXT("실제 외부 장애물 뒤 시설은 표시 proxy가 있어도 선택하지 않는다"),
			Coordinator->ResolveFacilityAlongRay(
				InsideRayOrigin, RayDirection, Facility));
		VisibilityBlocker->Destroy();
	}

	Proxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TestNull(TEXT("collision이 꺼진 stale Hover geometry는 직접 선택하지 않는다"),
		Coordinator->ResolveFacilityAlongRay(
			InsideRayOrigin, RayDirection, Facility));
	Proxy->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FacilityInfoIndicatorLifecycleTest,
	"FestaFiesta.UI.FacilityInfo.LiveSelectionIndicatorLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityInfoIndicatorLifecycleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("선택 표시 수명주기용 Editor World"), EditorWorld))
	{
		return false;
	}

	UFF_FacilityInfoSubsystem* Coordinator =
		NewObject<UFF_FacilityInfoSubsystem>(EditorWorld);
	AFFZSFacilityActor* FacilityA = EditorWorld->SpawnActor<AFFZSFacilityActor>();
	AFFZSFacilityActor* FacilityB = EditorWorld->SpawnActor<AFFZSFacilityActor>();
	if (!TestNotNull(TEXT("선택 표시 전환 Coordinator"), Coordinator) ||
		!TestNotNull(TEXT("Hover 대상 시설 A"), FacilityA) ||
		!TestNotNull(TEXT("Hover 대상 시설 B"), FacilityB))
	{
		if (FacilityA)
		{
			FacilityA->Destroy();
		}
		if (FacilityB)
		{
			FacilityB->Destroy();
		}
		return false;
	}

	Coordinator->ApplyLiveSelectionIndicatorTargets(FacilityA, nullptr);
	TestEqual(TEXT("첫 Hover 시설만 표시된다"),
		FacilityA->GetSelectionIndicatorState(),
		EFFZSFacilitySelectionIndicatorState::Hovered);
	TestEqual(TEXT("Hover되지 않은 시설은 숨김을 유지한다"),
		FacilityB->GetSelectionIndicatorState(),
		EFFZSFacilitySelectionIndicatorState::Hidden);

	Coordinator->ApplyLiveSelectionIndicatorTargets(FacilityB, FacilityA);
	TestEqual(TEXT("정보 패널 선택 상태가 Hover보다 우선한다"),
		FacilityA->GetSelectionIndicatorState(),
		EFFZSFacilitySelectionIndicatorState::Selected);
	TestEqual(TEXT("새 Hover 대상은 별도로 표시된다"),
		FacilityB->GetSelectionIndicatorState(),
		EFFZSFacilitySelectionIndicatorState::Hovered);

	Coordinator->ApplyLiveSelectionIndicatorTargets(nullptr, FacilityA);
	TestEqual(TEXT("커서가 패널·빈 공간으로 이동하면 이전 Hover가 정리된다"),
		FacilityB->GetSelectionIndicatorState(),
		EFFZSFacilitySelectionIndicatorState::Hidden);
	TestEqual(TEXT("패널이 열린 선택 시설 표시는 유지된다"),
		FacilityA->GetSelectionIndicatorState(),
		EFFZSFacilitySelectionIndicatorState::Selected);

	Coordinator->ClearLiveSelectionIndicators();
	TestEqual(TEXT("Live 이탈·복원·종료 정리는 선택 표시도 숨긴다"),
		FacilityA->GetSelectionIndicatorState(),
		EFFZSFacilitySelectionIndicatorState::Hidden);
	TestEqual(TEXT("정리 뒤 Hover 표시가 남지 않는다"),
		FacilityB->GetSelectionIndicatorState(),
		EFFZSFacilitySelectionIndicatorState::Hidden);

	FacilityA->Destroy();
	FacilityB->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FacilityInfoAssetContractTest,
	"FestaFiesta.UI.FacilityInfo.AssetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityInfoAssetContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, FacilityInfoObjectPath);
	TestNotNull(TEXT("시설 정보 WBP 에셋이 로드된다"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}
	TestTrue(TEXT("시설 정보 WBP에 컴파일 오류가 없다"), Blueprint->Status != BS_Error);

	UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(LoadObject<UClass>(nullptr, FacilityInfoClassPath));
	TestNotNull(TEXT("시설 정보 WBP GeneratedClass가 로드된다"), GeneratedClass);
	if (!GeneratedClass)
	{
		return false;
	}
	TestTrue(TEXT("WBP는 네이티브 시설 정보 패널 계약을 상속한다"),
		GeneratedClass->IsChildOf(UFF_FacilityInfoPanel::StaticClass()));

	const UWidgetTree* Tree = GeneratedClass->GetWidgetTreeArchetype();
	TestNotNull(TEXT("컴파일된 WidgetTree가 존재한다"), Tree);
	if (!Tree)
	{
		return false;
	}

	const UCanvasPanel* Root = Cast<UCanvasPanel>(Tree->RootWidget);
	TestNotNull(TEXT("전체 화면 루트는 CanvasPanel이다"), Root);
	if (Root)
	{
		TestEqual(TEXT("카드 밖 월드 입력을 막지 않는 루트 visibility"),
			Root->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	}
	const USizeBox* PanelSize = Cast<USizeBox>(Tree->FindWidget(TEXT("Panel_Size")));
	TestNotNull(TEXT("카드는 고정 크기를 소유하는 SizeBox다"), PanelSize);
	const UCanvasPanelSlot* PanelSlot = PanelSize
		? Cast<UCanvasPanelSlot>(PanelSize->Slot)
		: nullptr;
	TestNotNull(TEXT("카드는 전체 화면 Canvas의 직접 자식이다"), PanelSlot);
	constexpr float ExpectedPanelWidth = 376.0f;
	constexpr float ExpectedPanelHeight = 500.0f;
	constexpr float ExpectedPanelRightInset = -18.0f;
	constexpr float ExpectedPanelTop = 18.0f;
	if (PanelSize)
	{
		TestTrue(TEXT("카드 WidthOverride는 376px이다"),
			FMath::IsNearlyEqual(PanelSize->GetWidthOverride(), ExpectedPanelWidth));
		TestTrue(TEXT("카드 HeightOverride는 500px이다"),
			FMath::IsNearlyEqual(PanelSize->GetHeightOverride(), ExpectedPanelHeight));
	}
	if (PanelSlot)
	{
		const FAnchors Anchors = PanelSlot->GetAnchors();
		TestTrue(TEXT("카드 anchor는 우상단이다"),
			Anchors.Minimum.Equals(FVector2D(1.0f, 0.0f)) &&
			Anchors.Maximum.Equals(FVector2D(1.0f, 0.0f)));
		TestTrue(TEXT("카드 alignment는 우상단이다"),
			PanelSlot->GetAlignment().Equals(FVector2D(1.0f, 0.0f)));
		const FVector2D PanelPosition = PanelSlot->GetPosition();
		TestTrue(TEXT("카드 위치는 우측 -18px, 상단 18px이다"),
			PanelPosition.Equals(FVector2D(ExpectedPanelRightInset, ExpectedPanelTop)));
		TestFalse(TEXT("카드 Canvas 슬롯은 AutoSize를 사용하지 않는다"), PanelSlot->GetAutoSize());
		const FVector2D PanelSlotSize = PanelSlot->GetSize();
		TestTrue(TEXT("카드 Canvas 슬롯은 376x500 고정 크기다"),
			PanelSlotSize.Equals(FVector2D(ExpectedPanelWidth, ExpectedPanelHeight)));

		constexpr float ReferenceViewportHeight = 1080.0f;
		constexpr float PrepHudBarHeight = 58.0f;
		constexpr float PrepHudBottomMargin = 12.0f;
		constexpr float PrepBuildHostHeight = 320.0f;
		constexpr float PrepBuildHudTop = ReferenceViewportHeight
			- PrepHudBarHeight - PrepHudBottomMargin - PrepBuildHostHeight;
		static_assert(ExpectedPanelTop + ExpectedPanelHeight == 518.0f);
		static_assert(PrepBuildHudTop == 690.0f);
		const float PanelBottom = PanelPosition.Y + PanelSlotSize.Y;
		TestTrue(TEXT("720p에서도 카드 하단은 하단 도구 모음 시작 528px보다 위다"), PanelBottom < 528.0f);
		TestTrue(*FString::Printf(
			TEXT("1080p에서 시설 카드 하단 %.0fpx는 Build HUD 상단 %.0fpx보다 위다"),
			PanelBottom,
			PrepBuildHudTop),
			PanelBottom < PrepBuildHudTop);
	}
	TestNotNull(TEXT("카드가 명시적 Border 계약을 가진다"),
		Cast<UBorder>(Tree->FindWidget(TEXT("Border_Panel"))));
	TestNotNull(TEXT("닫기 버튼 계약"), Cast<UButton>(Tree->FindWidget(TEXT("Button_Close"))));
	TestNotNull(TEXT("이동 버튼 계약"), Cast<UButton>(Tree->FindWidget(TEXT("Button_Move"))));
	TestNotNull(TEXT("철거 버튼 계약"), Cast<UButton>(Tree->FindWidget(TEXT("Button_Remove"))));
	TestNotNull(TEXT("상세 정보는 내부 ScrollBox를 사용한다"),
		Cast<UScrollBox>(Tree->FindWidget(TEXT("ScrollBox_Details"))));
	TestNotNull(TEXT("썸네일 계약"), Cast<UImage>(Tree->FindWidget(TEXT("Image_Thumbnail"))));
	const USizeBox* ThumbnailSize = Cast<USizeBox>(Tree->FindWidget(TEXT("Thumbnail_Size")));
	TestTrue(TEXT("확대된 썸네일은 120x112px 공간을 가진다"), ThumbnailSize &&
		FMath::IsNearlyEqual(ThumbnailSize->GetWidthOverride(), 120.0f) &&
		FMath::IsNearlyEqual(ThumbnailSize->GetHeightOverride(), 112.0f));
	TestNotNull(TEXT("현재/강화 후 비교 카드 계약"), Tree->FindWidget(TEXT("Panel_UpgradeComparison")));
	for (const FName Name : {FName(TEXT("Text_UpgradeCurrent")), FName(TEXT("Text_UpgradeNext")),
		FName(TEXT("Text_Footprint_Card")), FName(TEXT("Text_InstallCost_Card")), FName(TEXT("Text_Occupancy_Card"))})
	{
		TestNotNull(*FString::Printf(TEXT("게임형 정보 카드 %s"), *Name.ToString()), Tree->FindWidget(Name));
	}
	TestNotNull(TEXT("건축 섹션 계약"), Tree->FindWidget(TEXT("Panel_BuildSection")));
	TestNotNull(TEXT("라이브 섹션 계약"), Tree->FindWidget(TEXT("Panel_LiveSection")));
	TestNotNull(TEXT("고정 행동 영역 계약"), Tree->FindWidget(TEXT("Panel_Actions")));
	TestNotNull(TEXT("건축 행동 영역 계약"), Tree->FindWidget(TEXT("Panel_BuildActions")));
	TestNotNull(TEXT("라이브 행동 영역 계약"), Tree->FindWidget(TEXT("Panel_LiveActions")));
	TestNotNull(TEXT("유료 프로그램 섹션 계약"), Tree->FindWidget(TEXT("Panel_ProgramSection")));
	TestNotNull(TEXT("유료 프로그램 편성 버튼 계약"),
		Cast<UButton>(Tree->FindWidget(TEXT("Button_ScheduleProgram"))));
	TestNotNull(TEXT("시설 강화 버튼 계약"), Cast<UButton>(Tree->FindWidget(TEXT("Button_Upgrade"))));
	const UWidget* UpgradeSection = Tree->FindWidget(TEXT("Panel_UpgradeSection"));
	bool bUpgradeInsideScroll = false;
	for (const UWidget* Ancestor = UpgradeSection; Ancestor; Ancestor = Ancestor->GetParent())
	{
		bUpgradeInsideScroll |= Ancestor == Tree->FindWidget(TEXT("ScrollBox_Details"));
	}
	TestTrue(TEXT("강화 컨트롤은 카드 외부와 겹치지 않고 기존 스크롤 안에 있다"), bUpgradeInsideScroll);
	UVerticalBox* DetailContent = Cast<UVerticalBox>(Tree->FindWidget(TEXT("Panel_Content")));
	if (TestNotNull(TEXT("시설 상세 스크롤 콘텐츠"), DetailContent) && UpgradeSection)
	{
		const UWidget* Summary = FFFacilityInfoUpgradeLayout::FindContentChild(
			Tree->FindWidget(TEXT("Summary_Card")), *DetailContent);
		const int32 UpgradeIndex = DetailContent->GetChildIndex(UpgradeSection);
		TestTrue(TEXT("강화 영역은 실제 WBP 요약 바로 다음에 위치한다"),
			Summary && UpgradeIndex == DetailContent->GetChildIndex(Summary) + 1);
		for (const FName Name : {FName(TEXT("Panel_BuildSection")), FName(TEXT("Panel_LiveSection")),
			FName(TEXT("Panel_ProgramSection"))})
		{
			UWidget* OtherSection = FFFacilityInfoUpgradeLayout::FindContentChild(Tree->FindWidget(Name), *DetailContent);
			TestTrue(*FString::Printf(TEXT("강화는 %s보다 먼저 표시한다"), *Name.ToString()),
				OtherSection && UpgradeIndex < DetailContent->GetChildIndex(OtherSection));
		}
	}

	const TArray<FName> RequiredTextNames = {
		TEXT("Text_ThumbnailPlaceholder"), TEXT("Text_Context"), TEXT("Text_FacilityName"),
		TEXT("Text_Category"), TEXT("Text_Zone"), TEXT("Text_FacilityState"),
		TEXT("Text_RelocationWarning"), TEXT("Text_Footprint"), TEXT("Text_Clearance"),
		TEXT("Text_InstallCost"), TEXT("Text_RecoveryCost"), TEXT("Text_QueueCapacity"),
		TEXT("Text_ServiceDuration"), TEXT("Text_OperationState"), TEXT("Text_WaitingParties"),
		TEXT("Text_Occupancy"), TEXT("Text_RemainingService"), TEXT("Text_ExpectedWait"),
		TEXT("Text_Throughput"), TEXT("Text_Feedback"), TEXT("Text_ProgramStatus"),
		TEXT("Text_ScheduledProgramSummary"), TEXT("Text_ProgramScheduleReason"),
		TEXT("Text_ScheduleProgram"), TEXT("Text_StartMinigame"), TEXT("Text_FacilityLevel"),
		TEXT("Text_UpgradeEffect"), TEXT("Text_UpgradeCost"), TEXT("Text_UpgradeReason"), TEXT("Text_Upgrade"),
		TEXT("Text_FacilityRevenue")
	};
	for (const FName Name : RequiredTextNames)
	{
		TestNotNull(*FString::Printf(TEXT("필수 텍스트 위젯: %s"), *Name.ToString()),
			Cast<UCommonTextBlock>(Tree->FindWidget(Name)));
	}
	TArray<FName> RequiredVariableNames = {
		TEXT("Border_Panel"), TEXT("Button_Close"), TEXT("Button_Move"),
		TEXT("Button_Remove"), TEXT("Button_StartMinigame"), TEXT("Image_Thumbnail"),
		TEXT("Panel_BuildSection"), TEXT("Panel_LiveSection"), TEXT("Panel_ProgramSection"),
		TEXT("Panel_Actions"), TEXT("Panel_BuildActions"), TEXT("Panel_LiveActions"),
		TEXT("Button_ScheduleProgram"), TEXT("Panel_UpgradeSection"), TEXT("Button_Upgrade")
	};
	RequiredVariableNames.Append(RequiredTextNames);
	for (const FName Name : RequiredVariableNames)
	{
		const UWidget* Widget = Tree->FindWidget(Name);
		TestTrue(*FString::Printf(TEXT("BindWidget 변수/GUID 계약: %s"), *Name.ToString()),
			Widget && Widget->bIsVariable && Blueprint->WidgetVariableNameToGuidMap.Contains(Name));
	}

	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	TestNotNull(TEXT("UMG 인스턴스 검증용 Editor World가 존재한다"), EditorWorld);
	if (!EditorWorld)
	{
		return false;
	}
	FScopedFacilityInfoPlayerlessWidgetInitialization InitializationGuard(*GeneratedClass);
	UFF_FacilityInfoPanel* Panel = CreateWidget<UFF_FacilityInfoPanel>(EditorWorld, GeneratedClass);
	TestNotNull(TEXT("시설 정보 BindWidget 계약으로 WBP 인스턴스를 만들 수 있다"), Panel);
	if (Panel)
	{
		FFF_FacilityInfoViewData ViewData;
		ViewData.FacilityInstanceId = FGuid::NewGuid();
		ViewData.Context = EFF_FacilityInfoContext::Build;
		ViewData.DisplayName = FText::FromString(TEXT("테스트 시설"));
		ViewData.bCanMove = true;
		ViewData.bCanRemove = true;
		ViewData.bShowProgramSection = true;
		ViewData.ProgramStatusText = FText::FromString(TEXT("오늘 편성 없음"));
		ViewData.bShowProgramScheduleAction = true;
		ViewData.bCanScheduleProgram = true;
		ViewData.ProgramScheduleActionText = FText::FromString(TEXT("유료 프로그램 편성"));
		ViewData.bShowUpgradeAction = true;
		ViewData.bHasFacilityLevels = true;
		ViewData.bShowUpgradeDetails = true;
		ViewData.bCanUpgrade = true;
		ViewData.FacilityLevelText = FText::FromString(TEXT("시설 강화 · Lv.1"));
		Panel->ApplyViewData(ViewData);
		const TSharedRef<SWidget> SlateWidget = Panel->TakeWidget();
		(void)SlateWidget;
		TestEqual(TEXT("적용된 ViewData의 GUID가 보존된다"),
			Panel->GetCurrentViewData().FacilityInstanceId, ViewData.FacilityInstanceId);
		UButton* MoveButton = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_Move")));
		UButton* RemoveButton = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_Remove")));
		TestNotNull(TEXT("인스턴스에 이동 버튼이 바인딩된다"), MoveButton);
		TestNotNull(TEXT("인스턴스에 철거 버튼이 바인딩된다"), RemoveButton);
		if (MoveButton && RemoveButton)
		{
			TestTrue(TEXT("이동 가능 read model은 이동 버튼을 활성화한다"),
				MoveButton->GetIsEnabled());
			TestTrue(TEXT("철거 가능 read model은 철거 버튼을 활성화한다"),
				RemoveButton->GetIsEnabled());

			int32 MoveRequestCount = 0;
			int32 RemoveRequestCount = 0;
			const FDelegateHandle MoveHandle = Panel->OnMoveRequested.AddLambda(
				[&MoveRequestCount]() { ++MoveRequestCount; });
			const FDelegateHandle RemoveHandle = Panel->OnRemoveRequested.AddLambda(
				[&RemoveRequestCount]() { ++RemoveRequestCount; });
			MoveButton->OnClicked.Broadcast();
			RemoveButton->OnClicked.Broadcast();
			TestEqual(TEXT("활성 이동 버튼은 요청을 한 번 보낸다"), MoveRequestCount, 1);
			TestEqual(TEXT("활성 철거 버튼은 요청을 한 번 보낸다"), RemoveRequestCount, 1);

			ViewData.bCanMove = false;
			ViewData.bCanRemove = false;
			Panel->ApplyViewData(ViewData);
			TestFalse(TEXT("이동 불가 read model은 이동 버튼을 비활성화한다"),
				MoveButton->GetIsEnabled());
			TestFalse(TEXT("철거 불가 read model은 철거 버튼을 비활성화한다"),
				RemoveButton->GetIsEnabled());
			MoveButton->OnClicked.Broadcast();
			RemoveButton->OnClicked.Broadcast();
			TestEqual(TEXT("비활성 이동 버튼은 요청을 우회 방송하지 않는다"), MoveRequestCount, 1);
			TestEqual(TEXT("비활성 철거 버튼은 요청을 우회 방송하지 않는다"), RemoveRequestCount, 1);
			Panel->OnMoveRequested.Remove(MoveHandle);
			Panel->OnRemoveRequested.Remove(RemoveHandle);
		}
		UWidget* ProgramSection = Panel->GetWidgetFromName(TEXT("Panel_ProgramSection"));
		UButton* ScheduleProgramButton = Cast<UButton>(
			Panel->GetWidgetFromName(TEXT("Button_ScheduleProgram")));
		TestNotNull(TEXT("인스턴스에 designer-editable 프로그램 섹션이 바인딩된다"), ProgramSection);
		TestNotNull(TEXT("인스턴스에 프로그램 편성 버튼이 바인딩된다"), ScheduleProgramButton);
		if (ProgramSection)
		{
			TestEqual(TEXT("프로그램 섹션 visibility는 read model을 따른다"),
				ProgramSection->GetVisibility(), ESlateVisibility::Visible);
		}
		if (ScheduleProgramButton)
		{
			TestTrue(TEXT("편성 가능 read model은 버튼을 활성화한다"),
				ScheduleProgramButton->GetIsEnabled());
			int32 PanelRequestCount = 0;
			const FDelegateHandle Handle = Panel->OnProgramScheduleRequested.AddLambda(
				[&PanelRequestCount]() { ++PanelRequestCount; });
			ScheduleProgramButton->OnClicked.Broadcast();
			TestEqual(TEXT("활성화된 편성 버튼은 패널 요청을 한 번 보낸다"), PanelRequestCount, 1);

			ViewData.bCanScheduleProgram = false;
			ViewData.ProgramScheduleDisabledReasonText =
				FText::FromString(TEXT("현재 단계에서는 편성할 수 없습니다."));
			Panel->ApplyViewData(ViewData);
			ScheduleProgramButton->OnClicked.Broadcast();
			TestEqual(TEXT("비활성화된 편성 버튼은 우회 방송하지 않는다"), PanelRequestCount, 1);
			Panel->OnProgramScheduleRequested.Remove(Handle);
		}
		UButton* UpgradeButton = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_Upgrade")));
		TestNotNull(TEXT("인스턴스에 시설 강화 버튼이 바인딩된다"), UpgradeButton);
		if (UpgradeButton)
		{
			TestTrue(TEXT("확인 가능한 강화 견적은 구매 버튼을 활성화한다"), UpgradeButton->GetIsEnabled());
			int32 UpgradeRequestCount = 0;
			const FDelegateHandle UpgradeHandle = Panel->OnUpgradeRequested.AddLambda(
				[&UpgradeRequestCount]() { ++UpgradeRequestCount; });
			UpgradeButton->OnClicked.Broadcast();
			TestEqual(TEXT("가격과 효과를 확인한 구매 클릭을 한 번 전달한다"), UpgradeRequestCount, 1);
			// A synchronous command can immediately display Lv.2 -> Lv.3 before a double-click ends.
			ViewData.CurrentUpgradeLevel = 2;
			Panel->ApplyViewData(ViewData);
			UpgradeButton->OnClicked.Broadcast();
			TestEqual(TEXT("더블클릭이 새로 표시된 다음 단계까지 구매하지 않는다"), UpgradeRequestCount, 1);
			ViewData.bCanUpgrade = false;
			ViewData.UpgradeDisabledReasonText = FText::FromString(TEXT("골드가 부족합니다."));
			Panel->ApplyViewData(ViewData);
			TestFalse(TEXT("구매 거절 견적은 버튼을 비활성화한다"), UpgradeButton->GetIsEnabled());
			UpgradeButton->OnClicked.Broadcast();
			TestEqual(TEXT("비활성 구매 버튼은 이벤트 우회도 거절한다"), UpgradeRequestCount, 1);
			ViewData.Context = EFF_FacilityInfoContext::Live;
			ViewData.bCanUpgrade = true;
			Panel->ApplyViewData(ViewData);
			TestEqual(TEXT("운영 중 구매 버튼은 숨긴다"), UpgradeButton->GetVisibility(), ESlateVisibility::Collapsed);
			TestEqual(TEXT("운영 중에도 현재 레벨을 유지한다"),
				Panel->GetWidgetFromName(TEXT("Panel_UpgradeSection"))->GetVisibility(), ESlateVisibility::Visible);
			UpgradeButton->OnClicked.Broadcast();
			TestEqual(TEXT("운영 중에는 구매 이벤트를 전달하지 않는다"), UpgradeRequestCount, 1);
			Panel->OnUpgradeRequested.Remove(UpgradeHandle);
			ViewData.bHasFacilityLevels = false;
			Panel->ApplyViewData(ViewData);
			TestEqual(TEXT("레벨 없는 시설의 강화 영역 전체를 숨긴다"),
				Panel->GetWidgetFromName(TEXT("Panel_UpgradeSection"))->GetVisibility(), ESlateVisibility::Collapsed);
			TestEqual(TEXT("레벨 없는 시설에도 수익 항목을 유지한다"),
				Panel->GetWidgetFromName(TEXT("Text_FacilityRevenue"))->GetVisibility(), ESlateVisibility::HitTestInvisible);
		}
		Panel->SetPanelOpen(true);
		TestEqual(TEXT("열린 full-screen wrapper는 카드 밖 클릭을 통과시킨다"),
			Panel->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
		Panel->SetPanelOpen(false);
		TestEqual(TEXT("닫힌 패널은 완전히 접힌다"),
			Panel->GetVisibility(), ESlateVisibility::Collapsed);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FacilityInfoUpgradeVisualPreviewTest,
	"FestaFiesta.UI.FacilityInfo.UpgradeVisualPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityInfoUpgradeVisualPreviewTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
	{
		AddInfo(TEXT("SKIPPED: 시설 강화 PNG 검증에는 실제 RHI가 필요합니다. -NullRHI 대신 -RenderOffscreen으로 실행하세요."));
		return true;
	}
	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	UWidgetBlueprintGeneratedClass* PanelClass = Cast<UWidgetBlueprintGeneratedClass>(
		LoadClass<UFF_FacilityInfoPanel>(nullptr, FacilityInfoClassPath));
	if (!TestNotNull(TEXT("렌더 검증용 에디터 월드"), EditorWorld) ||
		!TestNotNull(TEXT("렌더 검증은 실제 시설 WBP를 사용한다"), PanelClass))
	{
		return false;
	}
	FScopedFacilityInfoPlayerlessWidgetInitialization InitializationGuard(*PanelClass);
	UFF_FacilityInfoPanel* Panel = CreateWidget<UFF_FacilityInfoPanel>(EditorWorld, PanelClass);
	if (!TestNotNull(TEXT("실제 시설 패널 인스턴스"), Panel))
	{
		return false;
	}
	FFF_FacilityInfoViewData ViewData;
	ViewData.FacilityInstanceId = FGuid::NewGuid();
	ViewData.Context = EFF_FacilityInfoContext::Build;
	ViewData.ContextTitle = FText::FromString(TEXT("배치 시설"));
	ViewData.DisplayName = FText::FromString(TEXT("해변 간식 부스"));
	ViewData.Thumbnail = LoadCaptureFoodTruckTexture(*this);
	if (!ViewData.Thumbnail) return false;
	ViewData.CategoryText = FText::FromString(TEXT("음식 부스"));
	ViewData.ZoneText = FText::FromString(TEXT("중앙 구역"));
	ViewData.FacilityStateText = FText::FromString(TEXT("운영 준비 완료"));
	ViewData.ThumbnailPlaceholderText = FText::FromString(TEXT("음식 부스"));
	ViewData.bCanMove = true;
	ViewData.bCanRemove = true;
	ViewData.FootprintText = FText::FromString(TEXT("4 × 4 m"));
	ViewData.ServiceDurationText = FText::FromString(TEXT("4초"));
	FFF_FacilityUpgradePreview Preview;
	Preview.CurrentLevel = 1;
	Preview.TargetLevel = 2;
	Preview.MaxLevel = 3;
	Preview.CostGold = 120;
	Preview.bHasUpgradeQuote = true;
	Preview.GoldAfterUpgrade = 880;
	Preview.CurrentServiceDurationSeconds = 4.0f;
	Preview.NextServiceDurationSeconds = 3.2f;
	Preview.CurrentThroughputPerMinute = 15.0f;
	Preview.NextThroughputPerMinute = 18.75f;
	Preview.bSupported = true;
	Preview.CurrentFootprintCentimeters = FIntPoint(400, 400);
	Preview.bRevenueAvailable = true;
	Preview.BaseRevenuePerUseGold = 10;
	Preview.CurrentRevenuePerUseGold = 10;
	Preview.NextRevenuePerUseGold = 15;
	Preview.NextRevenueMultiplier = 1.5f;
	Preview.NextFootprintCentimeters = FIntPoint(600, 600);
	Preview.bHasLevels = true;
	Preview.bCanUpgrade = true;
	FFFacilityInfoModel::ApplyUpgradePreview(&Preview, ViewData);
	Panel->ApplyViewData(ViewData);
	Panel->SetPanelOpen(true);
	UBorder* Card = Cast<UBorder>(Panel->GetWidgetFromName(TEXT("Border_Panel")));
	if (!TestNotNull(TEXT("실제 WBP 카드 표면"), Card))
	{
		return false;
	}
	constexpr int32 CaptureWidth = 376;
	constexpr int32 CaptureHeight = 500;
	// Render the real card subtree at its shipped size, independent of the editor viewport.
	FWidgetRenderer Renderer(false, true);
	FReadSurfaceDataFlags ReadFlags;
	// The sRGB target already contains display-encoded color.
	ReadFlags.SetLinearToGamma(false);
	UTextureRenderTarget2D* RenderTarget = DrawCaptureWidget(
		Renderer, Card->TakeWidget(), FVector2D(CaptureWidth, CaptureHeight));
	TArray<FColor> Pixels;
	FTextureRenderTargetResource* Resource = RenderTarget ? RenderTarget->GameThread_GetRenderTargetResource() : nullptr;
	if (!TestTrue(TEXT("실제 RHI에서 시설 카드 픽셀을 읽는다"),
		Resource && Resource->ReadPixels(Pixels, ReadFlags) && Pixels.Num() == CaptureWidth * CaptureHeight))
	{
		return false;
	}
	const UWidget* UpgradeButton = Panel->GetWidgetFromName(TEXT("Button_Upgrade"));
	const UWidget* DetailScroll = Panel->GetWidgetFromName(TEXT("ScrollBox_Details"));
	if (TestNotNull(TEXT("강화 버튼 렌더 검증 대상"), UpgradeButton) &&
		TestNotNull(TEXT("스크롤 렌더 검증 대상"), DetailScroll))
	{
		const FGeometry& ButtonGeometry = UpgradeButton->GetCachedGeometry();
		const FGeometry& ScrollGeometry = DetailScroll->GetCachedGeometry();
		const FVector2D ButtonTop = ButtonGeometry.LocalToAbsolute(FVector2D::ZeroVector);
		const FVector2D ButtonBottom = ButtonGeometry.LocalToAbsolute(ButtonGeometry.GetLocalSize());
		const FVector2D ScrollTop = ScrollGeometry.LocalToAbsolute(FVector2D::ZeroVector);
		const FVector2D ScrollBottom = ScrollGeometry.LocalToAbsolute(ScrollGeometry.GetLocalSize());
		TestTrue(TEXT("첫 화면에서 강화 버튼의 전체 높이가 스크롤 안에 보여 footer에 가려지지 않는다"),
			ButtonGeometry.GetLocalSize().Y >= 24.0f && ButtonTop.Y >= ScrollTop.Y - 1.0 &&
			ButtonBottom.Y <= ScrollBottom.Y + 1.0);
		AddInfo(FString::Printf(TEXT("Upgrade button Y=%.1f..%.1f, scroll Y=%.1f..%.1f"),
			ButtonTop.Y, ButtonBottom.Y, ScrollTop.Y, ScrollBottom.Y));
	}
	TArray64<uint8> PngData;
	FImageUtils::PNGCompressImageArray(CaptureWidth, CaptureHeight, Pixels, PngData);
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString Path = FPaths::Combine(Directory, TEXT("FacilityUpgradePreview.png"));
	const bool bSaved = TestTrue(TEXT("시설 강화 화면 PNG 저장"), FFileHelper::SaveArrayToFile(PngData, *Path));
	AddInfo(FString::Printf(TEXT("시설 강화 시각 검증: %s"), *FPaths::ConvertRelativePathToFull(Path)));

	// Designer-authored tradeoffs and free, sparse levels must survive the real WBP binding/layout path.
	Preview.CurrentLevel = 5;
	Preview.TargetLevel = 20;
	Preview.MaxLevel = 20;
	Preview.CostGold = 0;
	Preview.GoldAfterUpgrade = 0;
	Preview.CurrentRevenueMultiplier = 1.5f;
	Preview.CurrentRevenuePerUseGold = 15;
	Preview.NextRevenueMultiplier = 1.f;
	Preview.NextRevenuePerUseGold = 10;
	Preview.NextServiceDurationSeconds = 8.f;
	Preview.NextThroughputPerMinute = 7.5f;
	Preview.NextFootprintCentimeters = FIntPoint(300, 300);
	FFFacilityInfoModel::ApplyUpgradePreview(&Preview, ViewData);
	Panel->ApplyViewData(ViewData);
	Panel->ForceLayoutPrepass();
	RenderTarget = DrawCaptureWidget(Renderer, Card->TakeWidget(), FVector2D(CaptureWidth, CaptureHeight));
	Resource = RenderTarget ? RenderTarget->GameThread_GetRenderTargetResource() : nullptr;
	Pixels.Reset();
	const bool bFlexiblePixels = TestTrue(TEXT("무료·감소·축소 견적도 실제 카드에서 렌더된다"),
		Resource && Resource->ReadPixels(Pixels, ReadFlags) && Pixels.Num() == CaptureWidth * CaptureHeight);
	bool bFlexibleSaved = false;
	if (bFlexiblePixels)
	{
		PngData.Reset();
		FImageUtils::PNGCompressImageArray(CaptureWidth, CaptureHeight, Pixels, PngData);
		const FString FlexiblePath = FPaths::Combine(Directory, TEXT("FacilityFlexibleUpgradePreview.png"));
		bFlexibleSaved = TestTrue(TEXT("자유 설정 강화 화면 PNG 저장"), FFileHelper::SaveArrayToFile(PngData, *FlexiblePath));
		const UButton* FreeButton = Cast<UButton>(UpgradeButton);
		TestTrue(TEXT("0 Gold에서도 무료 강화 버튼은 실제 WBP에서 활성화된다"), FreeButton && FreeButton->GetIsEnabled());
		if (FreeButton && DetailScroll)
		{
			const FGeometry& ButtonGeometry = FreeButton->GetCachedGeometry();
			const FGeometry& ScrollGeometry = DetailScroll->GetCachedGeometry();
			TestTrue(TEXT("무료·감소 견적의 버튼 전체도 첫 화면에 보인다"),
				ButtonGeometry.GetLocalSize().Y >= 24.f &&
				ButtonGeometry.LocalToAbsolute(FVector2D::ZeroVector).Y >= ScrollGeometry.LocalToAbsolute(FVector2D::ZeroVector).Y - 1. &&
				ButtonGeometry.LocalToAbsolute(ButtonGeometry.GetLocalSize()).Y <= ScrollGeometry.LocalToAbsolute(ScrollGeometry.GetLocalSize()).Y + 1.);
		}
	}
	Panel->ReleaseSlateResources(true);
	return bSaved && bFlexibleSaved;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FacilityInfoGamePanelVisualPreviewTest,
	"FestaFiesta.UI.FacilityInfo.GamePanelVisualPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityInfoGamePanelVisualPreviewTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
	{
		AddInfo(TEXT("SKIPPED: game panel screenshots require -RenderOffscreen with a real RHI."));
		return true;
	}
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	UWidgetBlueprintGeneratedClass* PanelClass = Cast<UWidgetBlueprintGeneratedClass>(
		LoadClass<UFF_FacilityInfoPanel>(nullptr, FacilityInfoClassPath));
	if (!TestNotNull(TEXT("게임 패널 시각 검증 월드"), World) ||
		!TestNotNull(TEXT("실제 게임 패널 WBP"), PanelClass)) return false;
	FScopedFacilityInfoPlayerlessWidgetInitialization InitializationGuard(*PanelClass);
	UFF_FacilityInfoPanel* Panel = CreateWidget<UFF_FacilityInfoPanel>(World, PanelClass);
	if (!TestNotNull(TEXT("시설 게임 패널 인스턴스"), Panel)) return false;
	FFF_FacilityInfoViewData Data;
	Data.FacilityInstanceId = FGuid::NewGuid();
	Data.Context = EFF_FacilityInfoContext::Build;
	Data.ContextTitle = FText::FromString(TEXT("배치 시설"));
	Data.DisplayName = FText::FromString(TEXT("다다포차 푸드트럭"));
	Data.Thumbnail = LoadCaptureFoodTruckTexture(*this);
	if (!Data.Thumbnail) return false;
	Data.CategoryText = FText::FromString(TEXT("부스 › 음식 판매"));
	Data.ZoneText = FText::FromString(TEXT("해변 구역"));
	Data.FacilityStateText = FText::FromString(TEXT("운영 준비 완료"));
	Data.ThumbnailPlaceholderText = FText::FromString(TEXT("시설 이미지 준비 중"));
	Data.FacilityRevenueText = FText::FromString(TEXT("1인 예상 수익  15 Gold"));
	Data.FootprintText = FText::FromString(TEXT("4 × 4 m"));
	Data.ClearanceText = FText::FromString(TEXT("1 m"));
	Data.InstallCostText = FText::FromString(TEXT("120 Gold"));
	Data.RecoveryCostText = FText::FromString(TEXT("60 Gold"));
	Data.InstanceLimitText = FText::FromString(TEXT("2 / 8개"));
	Data.QueueCapacityText = FText::FromString(TEXT("8명"));
	Data.ServiceDurationText = FText::FromString(TEXT("4초"));
	Data.bCanMove = true;
	Data.bCanRemove = true;
	Panel->ApplyViewData(Data);
	Panel->SetPanelOpen(true);
	// The renderer's virtual window is temporary. Retain SObjectWidget until after
	// geometry inspection so its destructor cannot release the child Slate resources.
	const TSharedRef<SWidget> PanelSlate = Panel->TakeWidget();
	FWidgetRenderer Renderer(false, true);
	FReadSurfaceDataFlags ReadFlags;
	ReadFlags.SetLinearToGamma(false);
	constexpr int32 Width = 1280;
	constexpr int32 Height = 720;
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	auto Capture = [&](const TCHAR* Filename)
	{
		Panel->ForceLayoutPrepass();
		UTextureRenderTarget2D* Target = DrawCaptureWidget(Renderer, PanelSlate, FVector2D(Width, Height));
		FTextureRenderTargetResource* Resource = Target ? Target->GameThread_GetRenderTargetResource() : nullptr;
		TArray<FColor> Pixels;
		if (!TestTrue(TEXT("720p 실제 WBP 렌더"), Resource && Resource->ReadPixels(Pixels, ReadFlags) && Pixels.Num() == Width * Height)) return false;
		// Solid header chrome above the context label, outside the rounded corners.
		const FColor HeaderPixel = Pixels[23 * Width + (Width - 18 - 376 + 20)];
		const FColor ExpectedHeader = UFF_UIPalette::Get()->InGame.TealDeep.ToFColor(true);
		TestTrue(TEXT("캡처 헤더 색은 실제 팔레트 sRGB와 일치한다"),
			FMath::Abs(int32(HeaderPixel.R) - ExpectedHeader.R) <= 3 &&
			FMath::Abs(int32(HeaderPixel.G) - ExpectedHeader.G) <= 3 &&
			FMath::Abs(int32(HeaderPixel.B) - ExpectedHeader.B) <= 3);
		AddInfo(FString::Printf(TEXT("Header RGB actual=%s, expected=%s"), *HeaderPixel.ToString(), *ExpectedHeader.ToString()));
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Width, Height, Pixels, Png);
		const FString Path = FPaths::Combine(Directory, Filename);
		AddInfo(FString::Printf(TEXT("Game panel screenshot: %s"), *FPaths::ConvertRelativePathToFull(Path)));
		return TestTrue(TEXT("720p 게임 패널 PNG 저장"), FFileHelper::SaveArrayToFile(Png, *Path));
	};
	bool bSaved = Capture(TEXT("FacilityGamePanelBuild720.png"));
	UWidget* Footer = Panel->GetWidgetFromName(TEXT("Panel_Actions"));
	UScrollBox* Scroll = Cast<UScrollBox>(Panel->GetWidgetFromName(TEXT("ScrollBox_Details")));
	if (TestNotNull(TEXT("고정 행동 영역"), Footer) && TestNotNull(TEXT("상세 스크롤"), Scroll))
	{
		const FGeometry& Geometry = Footer->GetPaintSpaceGeometry();
		const double FooterTop = Geometry.LocalToAbsolute(FVector2D::ZeroVector).Y;
		const double FooterBottom = Geometry.LocalToAbsolute(Geometry.GetLocalSize()).Y;
		TestTrue(TEXT("720p 고정 행동 영역은 도구 모음 위에 온전히 보인다"),
			Geometry.GetLocalSize().Y > 24.0 && FooterTop >= 18.0 && FooterBottom <= 518.5);
		AddInfo(FString::Printf(TEXT("Footer Y=%.1f..%.1f, height=%.1f, tools begin=528"),
			FooterTop, FooterBottom, Geometry.GetLocalSize().Y));
		Scroll->SetScrollOffset(180.0f);
		bSaved &= Capture(TEXT("FacilityGamePanelBuildDetails720.png"));
		TestTrue(TEXT("상세 스크롤은 고정 조작 버튼을 움직이지 않는다"),
			FMath::IsNearlyEqual(Footer->GetPaintSpaceGeometry().LocalToAbsolute(FVector2D::ZeroVector).Y, FooterTop, 0.5));
	}
	Data.Context = EFF_FacilityInfoContext::Live;
	Data.ContextTitle = FText::FromString(TEXT("운영 시설"));
	Data.FacilityStateText = FText::FromString(TEXT("운영 중"));
	Data.OperationStateText = FText::FromString(TEXT("손님을 맞이하고 있어요"));
	Data.WaitingPartiesText = FText::FromString(TEXT("3 / 1팀"));
	Data.OccupancyText = FText::FromString(TEXT("4 / 8명"));
	Data.RemainingServiceText = FText::FromString(TEXT("4명"));
	Data.ExpectedWaitText = FText::FromString(TEXT("12초"));
	Data.ThroughputText = FText::FromString(TEXT("분당 15명"));
	Data.bHasMinigame = true;
	Data.bCanStartMinigame = true;
	Panel->ApplyViewData(Data);
	bSaved &= Capture(TEXT("FacilityGamePanelLive720.png"));
	TestTrue(TEXT("컨텍스트 변경 시 새 정보의 처음으로 이동한다"), Scroll && Scroll->GetScrollOffset() < 1.0f);
	UButton* Minigame = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_StartMinigame")));
	TestTrue(TEXT("게임형 운영 화면에서도 미니게임 실행을 유지한다"), Minigame && Minigame->GetIsEnabled());
	Panel->ReleaseSlateResources(true);
	return bSaved;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FacilityPaidProgramWbpContractTest,
	"FestaFiesta.UI.FacilityInfo.PaidProgramWbpContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityPaidProgramWbpContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWidgetBlueprintGeneratedClass* PickerClass =
		Cast<UWidgetBlueprintGeneratedClass>(LoadObject<UClass>(nullptr, PaidProgramPickerClassPath));
	UWidgetBlueprintGeneratedClass* CardClass =
		Cast<UWidgetBlueprintGeneratedClass>(LoadObject<UClass>(nullptr, PaidProgramCardClassPath));
	if (!TestNotNull(TEXT("시설 유료 프로그램 Picker WBP class"), PickerClass) ||
		!TestNotNull(TEXT("시설 유료 프로그램 Card WBP class"), CardClass))
	{
		return false;
	}
	TestTrue(TEXT("Picker는 네이티브 순수 UI 계약을 상속한다"),
		PickerClass->IsChildOf(UFF_FacilityPaidProgramPicker::StaticClass()));
	TestTrue(TEXT("Card는 네이티브 순수 UI 계약을 상속한다"),
		CardClass->IsChildOf(UFF_FacilityPaidProgramCard::StaticClass()));

	const UWidgetTree* PickerTree = PickerClass->GetWidgetTreeArchetype();
	const UWidgetTree* CardTree = CardClass->GetWidgetTreeArchetype();
	if (!TestNotNull(TEXT("Picker WidgetTree"), PickerTree) ||
		!TestNotNull(TEXT("Card WidgetTree"), CardTree))
	{
		return false;
	}
	const UBorder* PickerBorder =
		Cast<UBorder>(PickerTree->FindWidget(TEXT("Border_Picker")));
	TestNotNull(TEXT("Picker 클릭 영역"), PickerBorder);
	const UCanvasPanelSlot* PickerSlot = PickerBorder
		? Cast<UCanvasPanelSlot>(PickerBorder->Slot)
		: nullptr;
	TestNotNull(TEXT("Picker는 전체 화면 Canvas에 우측 도킹된다"), PickerSlot);
	if (PickerSlot)
	{
		const FAnchors Anchors = PickerSlot->GetAnchors();
		TestTrue(TEXT("Picker anchor는 우측 세로 stretch다"),
			Anchors.Minimum.Equals(FVector2D(1.0f, 0.0f)) &&
			Anchors.Maximum.Equals(FVector2D(1.0f, 1.0f)));
		TestTrue(TEXT("Picker alignment는 우상단이다"),
			PickerSlot->GetAlignment().Equals(FVector2D(1.0f, 0.0f)));

		const FMargin Offsets = PickerSlot->GetOffsets();
		TestTrue(TEXT("Picker offsets는 우측 24px, 상단 72px, 폭 520px, 하단 400px 여백이다"),
			FMath::IsNearlyEqual(Offsets.Left, -24.0f) &&
			FMath::IsNearlyEqual(Offsets.Top, 72.0f) &&
			FMath::IsNearlyEqual(Offsets.Right, 520.0f) &&
			FMath::IsNearlyEqual(Offsets.Bottom, 400.0f));

		constexpr float ReferenceViewportHeight = 1080.0f;
		constexpr float PrepHudBarHeight = 58.0f;
		constexpr float PrepHudBottomMargin = 12.0f;
		constexpr float PrepBuildHostHeight = 320.0f;
		constexpr float PrepBuildHudTop = ReferenceViewportHeight
			- PrepHudBarHeight - PrepHudBottomMargin - PrepBuildHostHeight;
		const float PickerBottom = ReferenceViewportHeight - Offsets.Bottom;
		TestTrue(TEXT("1080p에서 Picker 하단은 680px이다"),
			FMath::IsNearlyEqual(PickerBottom, 680.0f));
		TestTrue(*FString::Printf(
			TEXT("1080p에서 Picker 하단 %.0fpx는 Build HUD 상단 %.0fpx보다 위다"),
			PickerBottom,
			PrepBuildHudTop),
			PickerBottom < PrepBuildHudTop);
	}
	TestNotNull(TEXT("프로그램 동적 목록"),
		Cast<UScrollBox>(PickerTree->FindWidget(TEXT("ScrollBox_Programs"))));
	TestNotNull(TEXT("별도 구매·편성 확인 버튼"),
		Cast<UButton>(PickerTree->FindWidget(TEXT("Button_Confirm"))));
	TestNotNull(TEXT("당일 취소·환불 불가 문구 계약"),
		Cast<UCommonTextBlock>(PickerTree->FindWidget(TEXT("Text_NoRefundNotice"))));
	TestNotNull(TEXT("카드 선택 버튼"),
		Cast<UButton>(CardTree->FindWidget(TEXT("Button_Select"))));
	TestNull(TEXT("순수 UI Picker에는 gameplay subsystem 객체가 저작되지 않는다"),
		PickerTree->FindWidget(TEXT("ProgramSchedulingSubsystem")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FacilityInfoPausePresentationLifecycleTest,
	"FestaFiesta.UI.FacilityInfo.PausePresentationLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityInfoPausePresentationLifecycleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Pause presentation Editor World"), EditorWorld))
	{
		return false;
	}

	UFF_FacilityInfoPanel* Panel = NewObject<UFF_FacilityInfoPanel>(EditorWorld);
	UFF_FacilityPaidProgramPicker* Picker =
		NewObject<UFF_FacilityPaidProgramPicker>(EditorWorld);
	UFF_FacilityInfoSubsystem* Coordinator =
		NewObject<UFF_FacilityInfoSubsystem>(EditorWorld);
	if (!TestNotNull(TEXT("Pause presentation panel"), Panel) ||
		!TestNotNull(TEXT("Pause presentation picker"), Picker) ||
		!TestNotNull(TEXT("Pause presentation coordinator"), Coordinator))
	{
		return false;
	}

	const FGuid SelectedFacilityId = FGuid::NewGuid();
	Coordinator->Panel = Panel;
	Coordinator->PaidProgramPicker = Picker;
	Coordinator->SelectedFacilityId = SelectedFacilityId;
	Coordinator->SelectionContext = EFF_FacilityInfoContext::Build;

	Panel->SetPanelOpen(true);
	Picker->SetPickerOpen(false);
	Coordinator->SetPauseMenuPresentationSuppressed(true);
	Coordinator->SetPauseMenuPresentationSuppressed(true);
	TestEqual(TEXT("Pause 중 panel은 높은 viewport layer에서 숨는다"),
		Panel->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Pause 중 picker도 높은 viewport layer에서 숨는다"),
		Picker->GetVisibility(), ESlateVisibility::Collapsed);
	Coordinator->SetPauseMenuPresentationSuppressed(false);
	TestEqual(TEXT("중복 suppress도 최초 panel 표면을 한 번에 복원한다"),
		Panel->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("panel 복원 때 picker는 겹쳐 열리지 않는다"),
		Picker->GetVisibility(), ESlateVisibility::Collapsed);

	Panel->SetPanelOpen(false);
	Picker->SetPickerOpen(true);
	Coordinator->SetPauseMenuPresentationSuppressed(true);
	Coordinator->SetPauseMenuPresentationSuppressed(false);
	TestEqual(TEXT("picker가 활성 표면이면 panel은 닫힌 채 유지된다"),
		Panel->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("picker 활성 표면만 복원된다"),
		Picker->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);

	Panel->SetPanelOpen(true);
	Picker->SetPickerOpen(false);
	Coordinator->SetPauseMenuPresentationSuppressed(true);
	Coordinator->SelectedFacilityId = FGuid::NewGuid();
	Coordinator->SetPauseMenuPresentationSuppressed(false);
	TestEqual(TEXT("Pause 중 선택 identity가 바뀌면 stale panel을 복원하지 않는다"),
		Panel->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Pause 중 선택 identity가 바뀌면 stale picker도 복원하지 않는다"),
		Picker->GetVisibility(), ESlateVisibility::Collapsed);

	Coordinator->SelectedFacilityId = SelectedFacilityId;
	Coordinator->SelectionContext = EFF_FacilityInfoContext::Build;
	FFF_FacilityInfoViewData ViewData;
	ViewData.FacilityInstanceId = SelectedFacilityId;
	ViewData.Context = EFF_FacilityInfoContext::Build;
	ViewData.bShowProgramSection = true;
	ViewData.bShowProgramScheduleAction = true;
	ViewData.bCanScheduleProgram = true;
	Panel->ApplyViewData(ViewData);
	Panel->SetPanelOpen(true);

	int32 MoveRequestCount = 0;
	int32 RemoveRequestCount = 0;
	int32 ProgramRequestCount = 0;
	const FDelegateHandle MoveHandle = Coordinator->OnMoveRequested.AddLambda(
		[&MoveRequestCount](const FGuid) { ++MoveRequestCount; });
	const FDelegateHandle RemoveHandle = Coordinator->OnRemoveRequested.AddLambda(
		[&RemoveRequestCount](const FGuid) { ++RemoveRequestCount; });
	const FDelegateHandle ProgramHandle = Coordinator->OnProgramScheduleRequested.AddLambda(
		[&ProgramRequestCount](const FGuid) { ++ProgramRequestCount; });

	Coordinator->SetPauseMenuPresentationSuppressed(true);
	Coordinator->HandlePanelMoveRequested();
	Coordinator->HandlePanelRemoveRequested();
	Coordinator->HandlePanelProgramScheduleRequested();
	Coordinator->HandlePanelCloseRequested();
	TestEqual(TEXT("Pause 중 이동 요청은 gameplay로 전달되지 않는다"), MoveRequestCount, 0);
	TestEqual(TEXT("Pause 중 철거 요청은 gameplay로 전달되지 않는다"), RemoveRequestCount, 0);
	TestEqual(TEXT("Pause 중 유료 프로그램 요청은 gameplay로 전달되지 않는다"), ProgramRequestCount, 0);
	TestEqual(TEXT("Pause 중 닫기 입력도 건축 선택을 지우지 않는다"),
		Coordinator->SelectedFacilityId, SelectedFacilityId);
	Coordinator->SetPauseMenuPresentationSuppressed(false);

	Coordinator->OnMoveRequested.Remove(MoveHandle);
	Coordinator->OnRemoveRequested.Remove(RemoveHandle);
	Coordinator->OnProgramScheduleRequested.Remove(ProgramHandle);
	Coordinator->Panel = nullptr;
	Coordinator->PaidProgramPicker = nullptr;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FacilityInfoProgramActionLifecycleTest,
	"FestaFiesta.UI.FacilityInfo.ProgramActionLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityInfoProgramActionLifecycleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(LoadObject<UClass>(nullptr, FacilityInfoClassPath));
	if (!TestNotNull(TEXT("편성 액션 수명주기 Editor World"), EditorWorld) ||
		!TestNotNull(TEXT("편성 액션 수명주기 WBP class"), GeneratedClass))
	{
		return false;
	}

	FScopedFacilityInfoPlayerlessWidgetInitialization InitializationGuard(*GeneratedClass);
	UFF_FacilityInfoPanel* Panel = CreateWidget<UFF_FacilityInfoPanel>(EditorWorld, GeneratedClass);
	UFF_FacilityInfoSubsystem* Coordinator =
		NewObject<UFF_FacilityInfoSubsystem>(EditorWorld);
	if (!TestNotNull(TEXT("편성 액션 패널"), Panel) ||
		!TestNotNull(TEXT("편성 액션 coordinator"), Coordinator))
	{
		return false;
	}
	const TSharedRef<SWidget> SlateWidget = Panel->TakeWidget();
	(void)SlateWidget;
	UButton* ScheduleProgramButton = Cast<UButton>(
		Panel->GetWidgetFromName(TEXT("Button_ScheduleProgram")));
	if (!TestNotNull(TEXT("편성 액션 버튼"), ScheduleProgramButton))
	{
		return false;
	}

	const FGuid SelectedFacilityId = FGuid::NewGuid();
	FFF_FacilityInfoViewData ViewData;
	ViewData.FacilityInstanceId = SelectedFacilityId;
	ViewData.Context = EFF_FacilityInfoContext::Build;
	ViewData.bShowProgramSection = true;
	ViewData.bShowProgramScheduleAction = true;
	ViewData.bCanScheduleProgram = true;
	Panel->ApplyViewData(ViewData);

	Coordinator->Panel = Panel;
	Coordinator->SelectedFacilityId = SelectedFacilityId;
	Coordinator->SelectionContext = EFF_FacilityInfoContext::Build;
	Panel->OnProgramScheduleRequested.AddUObject(
		Coordinator, &UFF_FacilityInfoSubsystem::HandlePanelProgramScheduleRequested);

	int32 BroadcastCount = 0;
	FGuid BroadcastFacilityId;
	const FDelegateHandle BroadcastHandle = Coordinator->OnProgramScheduleRequested.AddLambda(
		[&BroadcastCount, &BroadcastFacilityId](const FGuid FacilityInstanceId)
		{
			++BroadcastCount;
			BroadcastFacilityId = FacilityInstanceId;
		});
	ScheduleProgramButton->OnClicked.Broadcast();
	TestEqual(TEXT("클릭은 subsystem 액션을 한 번 방송한다"), BroadcastCount, 1);
	TestEqual(TEXT("subsystem 액션은 현재 선택 시설 GUID를 전달한다"),
		BroadcastFacilityId, SelectedFacilityId);

	Coordinator->SelectedFacilityId = FGuid::NewGuid();
	ScheduleProgramButton->OnClicked.Broadcast();
	TestEqual(TEXT("stale 패널 GUID는 새 선택으로 방송되지 않는다"), BroadcastCount, 1);

	ViewData.FacilityInstanceId = Coordinator->SelectedFacilityId;
	ViewData.bCanScheduleProgram = false;
	Panel->ApplyViewData(ViewData);
	ScheduleProgramButton->OnClicked.Broadcast();
	TestEqual(TEXT("비활성 read model은 subsystem 방송까지 도달하지 않는다"), BroadcastCount, 1);

	Coordinator->OnProgramScheduleRequested.Remove(BroadcastHandle);
	Panel->OnProgramScheduleRequested.RemoveAll(Coordinator);
	Coordinator->Panel = nullptr;
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
