// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Facility/FF_FacilityInfoTypes.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/BoxComponent.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "FF_FFZSCollision.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UI/Facility/FF_FacilityInfoSelection.h"
#include "UI/Facility/FF_FacilityInfoSubsystem.h"
#include "WidgetBlueprint.h"

namespace
{
	const TCHAR* FacilityInfoObjectPath =
		TEXT("/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityInfoPanel."
			"WBP_FF_FacilityInfoPanel");
	const TCHAR* FacilityInfoClassPath =
		TEXT("/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityInfoPanel."
			"WBP_FF_FacilityInfoPanel_C");

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
	const FFFZSFacilitySpec FacilitySpec =
		FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
	const FFFZSCommand InstallCommand = UFFZSGridLibrary::MakeInstallCommand(
		FacilitySpec,
		ZoneId,
		Placement,
		ZoneSystem->GetWorldRevision());
	const FFFZSCommandResult InstallResult = ZoneSystem->ExecuteCommand(InstallCommand);
	if (!TestTrue(TEXT("90도 회전 시설 설치"), InstallResult.bSucceeded))
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

	TestEqual(TEXT("회전된 표시 proxy 안쪽 1cm는 같은 시설로 판정한다"),
		Coordinator->ResolveLiveFacilityAlongRay(
			InsideRayOrigin, RayDirection, nullptr),
		Facility);
	TestNull(TEXT("회전된 표시 proxy 바깥쪽 1cm는 선택하지 않는다"),
		Coordinator->ResolveLiveFacilityAlongRay(
			OutsideRayOrigin, RayDirection, nullptr));
	TestEqual(TEXT("현재 Hover 표시 대상은 현재 ray와 교차하면 클릭 대상으로 유지한다"),
		Coordinator->ResolveLiveFacilityAlongRay(
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
				Coordinator->ResolveLiveFacilityAlongRay(
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
				Coordinator->ResolveLiveFacilityAlongRay(
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
				Coordinator->ResolveLiveFacilityAlongRay(
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
				Coordinator->ResolveLiveFacilityAlongRay(
					InsideRayOrigin, RayDirection, Facility),
				Facility);
			ContactBox->SetWorldLocation(
				InsideRayOrigin + RayDirection * (ProxyHitDistance - 4.0f));
			TestNull(TEXT("proxy 표면보다 2cm 이상 앞선 실제 blocker는 클릭을 막는다"),
				Coordinator->ResolveLiveFacilityAlongRay(
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
			Coordinator->ResolveLiveFacilityAlongRay(
				InsideRayOrigin, RayDirection, Facility));
		VisibilityBlocker->Destroy();
	}

	Proxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TestNull(TEXT("collision이 꺼진 stale Hover geometry는 직접 선택하지 않는다"),
		Coordinator->ResolveLiveFacilityAlongRay(
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
	const UCanvasPanelSlot* PanelSlot = PanelSize
		? Cast<UCanvasPanelSlot>(PanelSize->Slot)
		: nullptr;
	TestNotNull(TEXT("카드는 전체 화면 Canvas의 직접 자식이다"), PanelSlot);
	if (PanelSlot)
	{
		const FAnchors Anchors = PanelSlot->GetAnchors();
		TestTrue(TEXT("카드 anchor는 우측 중앙이다"),
			Anchors.Minimum.Equals(FVector2D(1.0f, 0.5f)) &&
			Anchors.Maximum.Equals(FVector2D(1.0f, 0.5f)));
		TestTrue(TEXT("카드 alignment는 우측 중앙이다"),
			PanelSlot->GetAlignment().Equals(FVector2D(1.0f, 0.5f)));
		const FVector2D PanelPosition = PanelSlot->GetPosition();
		TestFalse(*FString::Printf(TEXT("카드 오프셋은 디자이너가 조정할 수 있고 유한한 값이다 (Actual: %s)"),
			*PanelPosition.ToString()),
			PanelPosition.ContainsNaN());
		TestTrue(TEXT("카드 크기는 내용에 맞춰 계산된다"), PanelSlot->GetAutoSize());
	}
	TestNotNull(TEXT("카드가 명시적 Border 계약을 가진다"),
		Cast<UBorder>(Tree->FindWidget(TEXT("Border_Panel"))));
	TestNotNull(TEXT("닫기 버튼 계약"), Cast<UButton>(Tree->FindWidget(TEXT("Button_Close"))));
	TestNotNull(TEXT("이동 버튼 계약"), Cast<UButton>(Tree->FindWidget(TEXT("Button_Move"))));
	TestNotNull(TEXT("철거 버튼 계약"), Cast<UButton>(Tree->FindWidget(TEXT("Button_Remove"))));
	TestNotNull(TEXT("썸네일 계약"), Cast<UImage>(Tree->FindWidget(TEXT("Image_Thumbnail"))));
	TestNotNull(TEXT("건축 섹션 계약"), Tree->FindWidget(TEXT("Panel_BuildSection")));
	TestNotNull(TEXT("라이브 섹션 계약"), Tree->FindWidget(TEXT("Panel_LiveSection")));

	const TArray<FName> RequiredTextNames = {
		TEXT("Text_ThumbnailPlaceholder"), TEXT("Text_Context"), TEXT("Text_FacilityName"),
		TEXT("Text_Category"), TEXT("Text_Zone"), TEXT("Text_FacilityState"),
		TEXT("Text_RelocationWarning"), TEXT("Text_Footprint"), TEXT("Text_Clearance"),
		TEXT("Text_InstallCost"), TEXT("Text_RecoveryCost"), TEXT("Text_QueueCapacity"),
		TEXT("Text_ServiceDuration"), TEXT("Text_OperationState"), TEXT("Text_WaitingParties"),
		TEXT("Text_Occupancy"), TEXT("Text_RemainingService"), TEXT("Text_ExpectedWait"),
		TEXT("Text_Throughput"), TEXT("Text_Feedback")
	};
	for (const FName Name : RequiredTextNames)
	{
		TestNotNull(*FString::Printf(TEXT("필수 텍스트 위젯: %s"), *Name.ToString()),
			Cast<UCommonTextBlock>(Tree->FindWidget(Name)));
	}
	TArray<FName> RequiredVariableNames = {
		TEXT("Border_Panel"), TEXT("Button_Close"), TEXT("Button_Move"),
		TEXT("Button_Remove"), TEXT("Image_Thumbnail"), TEXT("Panel_BuildSection"),
		TEXT("Panel_LiveSection")
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
	TestNotNull(TEXT("27개 BindWidget 계약으로 WBP 인스턴스를 만들 수 있다"), Panel);
	if (Panel)
	{
		FFF_FacilityInfoViewData ViewData;
		ViewData.FacilityInstanceId = FGuid::NewGuid();
		ViewData.Context = EFF_FacilityInfoContext::Build;
		ViewData.DisplayName = FText::FromString(TEXT("테스트 시설"));
		ViewData.bCanMove = true;
		ViewData.bCanRemove = true;
		Panel->ApplyViewData(ViewData);
		const TSharedRef<SWidget> SlateWidget = Panel->TakeWidget();
		(void)SlateWidget;
		TestEqual(TEXT("적용된 ViewData의 GUID가 보존된다"),
			Panel->GetCurrentViewData().FacilityInstanceId, ViewData.FacilityInstanceId);
		Panel->SetPanelOpen(true);
		TestEqual(TEXT("열린 full-screen wrapper는 카드 밖 클릭을 통과시킨다"),
			Panel->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
		Panel->SetPanelOpen(false);
		TestEqual(TEXT("닫힌 패널은 완전히 접힌다"),
			Panel->GetVisibility(), ESlateVisibility::Collapsed);
	}
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
