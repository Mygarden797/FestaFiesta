#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Engine/World.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/UnrealType.h"
#include "UI/Atlas/FF_AtlasGlobeActor.h"
#include "UI/Atlas/FF_AtlasCameraRig.h"
#include "UI/Atlas/FF_AtlasSettings.h"
#include "UI/Atlas/FF_FestivalAtlasCatalog.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasInspectionLifecycle,
	"FestaFiesta.UI.AtlasInspection.SelectionAndCameraLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_AtlasInspectionLifecycle::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
		.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	// CreateWorld가 한 번 초기화하므로 InitializeNewWorld를 다시 호출하지 않는다.
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	AFF_AtlasGlobeActor* Globe = World->SpawnActor<AFF_AtlasGlobeActor>();
	Globe->RebuildPins();
	TArray<FFF_AtlasPinState> Pins;
	Globe->GetPinStates(Pins);
	if (!TestTrue(TEXT("검증용 카탈로그에 국가가 있다"), Pins.Num() > 0)) { World->DestroyWorld(false); return false; }
	const FName Country = Pins[0].CountryId;
	Globe->FocusOnCountry(Country, true);
	Globe->Tick(0.f);
	UStaticMeshComponent* FirstLandmark = nullptr;
	TInlineComponentArray<UStaticMeshComponent*> Components(Globe);
	for (UStaticMeshComponent* Component : Components)
	{
		if (Component->GetStaticMesh() && Component->GetAttachParent()
			&& Component->GetStaticMesh()->GetName().StartsWith(TEXT("SM_FF_Landmark_")))
		{
			FVector PinLocation;
			if (Globe->GetCountryPinWorldLocation(Country, PinLocation)
				&& Component->GetAttachParent()->GetComponentLocation().Equals(PinLocation, 0.01))
			{
				FirstLandmark = Component;
				break;
			}
		}
	}
	if (TestNotNull(TEXT("국가 대표 조형물이 실제 컴포넌트로 생성된다"), FirstLandmark))
	{
		const float FrontScale = FirstLandmark->GetRelativeScale3D().X;
		const FVector Center = FirstLandmark->Bounds.Origin;
		const FVector Outward = (Center - Globe->GetActorLocation()).GetSafeNormal();
		TestEqual(TEXT("정면 프랍 광선 선택"), Globe->PickLandmark(Center + Outward * 20000, -Outward), Country);
		TestTrue(TEXT("바다 뒤 프랍은 선택되지 않는다"), Globe->PickLandmark(Center - Outward * 20000, Outward) != Country);
		Globe->SetHoveredCountry(Country);
		TestTrue(TEXT("호버 프랍 확대"), FirstLandmark->GetRelativeScale3D().X > FrontScale);
		Globe->SetHoveredCountry(NAME_None);
		TestTrue(TEXT("호버 해제 크기 복원"), FMath::IsNearlyEqual(FirstLandmark->GetRelativeScale3D().X,FrontScale));
		Globe->SetGlobeActive(false);
		TestTrue(TEXT("숨긴 지구는 선택 불가"), Globe->PickLandmark(Center + Outward * 20000, -Outward).IsNone());
		Globe->SetGlobeActive(true);
		FFF_AtlasGlobeOrientation Edge = Globe->GetOrientation();
		Edge.Spin += 90.f;
		Globe->SetOrientation(Edge);
		TestTrue(TEXT("같은 조형물이 가장자리에서 정면보다 작다"), FirstLandmark->GetRelativeScale3D().X < FrontScale);
		Globe->FocusOnCountry(Country, true);
	}
	Globe->SetSelectedCountry(Country);
	const float SelectedSpin = Globe->GetOrientation().Spin;
	Globe->Tick(20.f);
	TestEqual(TEXT("선택 중에는 재개 대기를 넘어도 자전하지 않는다"), Globe->GetOrientation().Spin, SelectedSpin);
	Globe->SetSelectedCountry(NAME_None);
	Globe->Tick(UFF_AtlasSettings::Get()->AutoSpinResumeDelaySeconds + 1.f);
	TestTrue(TEXT("선택 해제 후 자전 재개"), !FMath::IsNearlyEqual(Globe->GetOrientation().Spin, SelectedSpin));
	Globe->SetSelectedCountry(Country);
	Globe->AddDragInput(FVector2D(15,0));
	TestTrue(TEXT("드래그로 선택 해제"), Globe->GetSelectedCountryId().IsNone());
	for (const FFF_AtlasPinState& Pin : Pins)
	{
		if (!Pin.bSelectable)
		{
			Globe->SetSelectedCountry(Pin.CountryId);
			TestEqual(TEXT("잠긴 국가도 살펴보기 선택 가능"), Globe->GetSelectedCountryId(), Pin.CountryId);
			TArray<FFF_AtlasPinState> After;
			Globe->GetPinStates(After);
			const auto* Same = After.FindByPredicate([&Pin](const FFF_AtlasPinState& P){ return P.CountryId == Pin.CountryId; });
			TestTrue(TEXT("살펴보기가 국가 잠금을 해제하지 않는다"), Same && !Same->bSelectable);
			break;
		}
	}

	ACameraActor* Camera = World->SpawnActor<ACameraActor>();
	Camera->SetActorLocation(FVector(15000,0,0));
	const FVector Base = Camera->GetActorLocation();
	AFF_AtlasCameraRig* Rig = World->SpawnActor<AFF_AtlasCameraRig>();
	for (const FName Slot : { FName("TitleCamera"), FName("GlobeCamera") })
	{
		FObjectProperty* Property = FindFProperty<FObjectProperty>(AFF_AtlasCameraRig::StaticClass(), Slot);
		Property->SetObjectPropertyValue_InContainer(Rig, Camera);
	}
	Rig->ViewForScreen(EFF_UIFlowScreen::AtlasGlobe, true);
	Rig->SetCountryInspection(true);
	Rig->Tick(2.f);
	TestTrue(TEXT("선택 카메라는 원래 축을 유지하며 접근"), Camera->GetActorLocation().X < Base.X && FMath::IsNearlyZero(Camera->GetActorLocation().Y));
	Rig->SetCountryInspection(false);
	Rig->Tick(2.f);
	TestTrue(TEXT("해제 시 원래 카메라 위치 복원"), Camera->GetActorLocation().Equals(Base,0.01));
	Rig->SetCountryInspection(true);
	Rig->Tick(2.f);
	Rig->ViewForScreen(EFF_UIFlowScreen::Title, true);
	TestTrue(TEXT("화면 전환 시 접근 상태가 타이틀에 남지 않는다"), Camera->GetActorLocation().Equals(Base,0.01));
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasAuthoredCountryView,
	"FestaFiesta.UI.AtlasInspection.AuthoredCountryViewAndInterruptedApproach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_AtlasAuthoredCountryView::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
		.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	AFF_AtlasGlobeActor* Globe = World->SpawnActor<AFF_AtlasGlobeActor>();
	UFF_FestivalAtlasCatalog* Catalog = NewObject<UFF_FestivalAtlasCatalog>();
	FFF_AtlasCountryEntry CountryA;
	CountryA.CountryId = TEXT("AuthoredA");
	CountryA.Location = FFF_AtlasLatLong(35.f, 129.f);
	CountryA.InspectionLocation = FFF_AtlasLatLong(12.f, 80.f);
	CountryA.InspectionApproachFraction = 0.25f;
	Catalog->Countries.Add(CountryA);
	FFF_AtlasCountryEntry CountryB;
	CountryB.CountryId = TEXT("AuthoredB");
	CountryB.Location = FFF_AtlasLatLong(-20.f, -40.f);
	CountryB.bOverrideInspectionView = true;
	CountryB.InspectionLocation = FFF_AtlasLatLong(-10.f, -30.f);
	CountryB.InspectionApproachFraction = 0.1f;
	Catalog->Countries.Add(CountryB);
	FindFProperty<FSoftObjectProperty>(AFF_AtlasGlobeActor::StaticClass(), TEXT("CatalogOverride"))
		->SetObjectPropertyValue_InContainer(Globe, Catalog);
	Globe->RebuildPins();
	Globe->SetAutoSpinPaused(true);
	Globe->SetOrientation(FFF_AtlasGlobeOrientation());
	FVector OriginalPropLocation;
	TestTrue(TEXT("원래 조형물 위치를 찾는다"), Globe->GetCountryPinWorldLocation(CountryA.CountryId, OriginalPropLocation));
	Globe->FocusOnCountry(CountryA.CountryId, true);
	TestEqual(TEXT("사용자 보기 비활성이면 원래 경도"), Globe->GetOrientation().Spin, CountryA.Location.Longitude);
	TestEqual(TEXT("사용자 보기 비활성이면 원래 위도"), Globe->GetOrientation().Tilt, CountryA.Location.Latitude);
	Catalog->Countries[0].bOverrideInspectionView = true;
	Globe->FocusOnCountry(CountryA.CountryId, true);
	TestEqual(TEXT("사용자 보기 경도로 회전"), Globe->GetOrientation().Spin, CountryA.InspectionLocation.Longitude);
	TestEqual(TEXT("사용자 보기 위도로 회전"), Globe->GetOrientation().Tilt, CountryA.InspectionLocation.Latitude);
	Globe->StepFocus(1);
	Globe->SkipTransition();
	TestEqual(TEXT("좌우 이동도 다음 나라의 저장된 보기 경도 사용"), Globe->GetOrientation().Spin, CountryB.InspectionLocation.Longitude);
	Globe->SetOrientation(FFF_AtlasGlobeOrientation());
	FVector UnchangedPropLocation;
	Globe->GetCountryPinWorldLocation(CountryA.CountryId, UnchangedPropLocation);
	TestTrue(TEXT("보기 기준점 변경은 조형물을 옮기지 않는다"), OriginalPropLocation.Equals(UnchangedPropLocation, 0.01));

	ACameraActor* Camera = World->SpawnActor<ACameraActor>();
	Camera->SetActorLocation(FVector(15000, 0, 0));
	const FVector Base = Camera->GetActorLocation();
	AFF_AtlasCameraRig* Rig = World->SpawnActor<AFF_AtlasCameraRig>();
	for (const FName Slot : { FName("TitleCamera"), FName("GlobeCamera") })
	{
		FindFProperty<FObjectProperty>(AFF_AtlasCameraRig::StaticClass(), Slot)->SetObjectPropertyValue_InContainer(Rig, Camera);
	}
	Rig->ViewForScreen(EFF_UIFlowScreen::AtlasGlobe, true);
	Globe->SetFocusViewportPosition(FVector2D(0.68, 0.5));
	Globe->FocusOnCountry(CountryA.CountryId);
	Rig->Tick(UFF_AtlasSettings::Get()->CountryInspectionSeconds * 0.4f);
	const FVector DuringA = Camera->GetActorLocation();
	TestTrue(TEXT("저장된 접근 거리까지 보간 중"), DuringA.X < Base.X && DuringA.X > Base.X * 0.75);
	Globe->FocusOnCountry(CountryB.CountryId);
	TestTrue(TEXT("다른 나라 선택 시 현재 카메라 위치에서 시작"), Camera->GetActorLocation().Equals(DuringA, 0.01));
	Rig->Tick(2.f);
	TestTrue(TEXT("다음 나라 접근 비율 10% 적용"), Camera->GetActorLocation().Equals(Base * 0.9, 0.01));
	Globe->FocusOnCountry(CountryA.CountryId);
	Rig->Tick(UFF_AtlasSettings::Get()->CountryInspectionSeconds * 0.3f);
	const FVector BeforeDrag = Camera->GetActorLocation();
	Globe->AddDragInput(FVector2D(15, 0));
	Rig->Tick(2.f);
	TestTrue(TEXT("드래그가 카메라 접근을 현 위치에 멈춘다"), Camera->GetActorLocation().Equals(BeforeDrag, 0.01));
	TestFalse(TEXT("드래그 후 자동 접근은 멈춤"), Rig->IsCountryInspectionMoving());
	TestFalse(TEXT("드래그 후 자동 회전은 멈춤"), Globe->IsTransitioning());
	Globe->FocusOnCountry(CountryA.CountryId);
	TestTrue(TEXT("중단 후 재선택도 보존한 카메라 위치에서 시작"), Camera->GetActorLocation().Equals(BeforeDrag, 0.01));
	Rig->Tick(2.f);
	TestTrue(TEXT("재선택은 저장된 접근 비율 25%에 도착"), Camera->GetActorLocation().Equals(Base * 0.75, 0.01));
	Rig->ViewForScreen(EFF_UIFlowScreen::Title, true);
	TestTrue(TEXT("국가 화면을 나가면 원래 카메라 위치로 복원"), Camera->GetActorLocation().Equals(Base, 0.01));
	Camera->SetActorLocation(FVector(9000, 0, 0));
	Rig->ViewForScreen(EFF_UIFlowScreen::AtlasGlobe, true);
	Rig->SetCountryInspection(true, 0.5f);
	Rig->Tick(2.f);
	TestTrue(TEXT("큰 접근 비율도 지구 표면의 안전거리를 넘지 않는다"),
		Camera->GetActorLocation().Size() >= Globe->GetGlobeRadius() * 1.35 - 0.01);
	World->DestroyWorld(false);
	return true;
}
#endif
