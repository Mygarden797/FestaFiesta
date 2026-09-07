// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Atlas/FF_AtlasGlobeActor.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "UI/Atlas/FF_AtlasSettings.h"
#include "UI/Atlas/FF_AtlasCameraRig.h"
#include "UI/Atlas/FF_FestivalAtlasCatalog.h"
#include "UI/Core/FF_UIAccessibilitySubsystem.h"
#include "UI/Core/FF_UIFlowSettings.h"
#include "UI/FF_UILog.h"

#if WITH_EDITORONLY_DATA
#include "Components/ArrowComponent.h"
#endif

namespace
{
	/**
	 * 엔진 기본 도형. **생성자에서 잡는다.**
	 * 런타임에 경로 문자열로 `LoadObject` 하면 쿠커가 참조를 못 보고 패키징에서 조용히 빠진다.
	 * 생성자에서 찾으면 CDO 가 하드 참조를 들어 쿠킹 목록에 잡힌다
	 * (`AFF_BaseCrowd` 가 스켈레탈 메시 대체 프리미티브에 쓴 것과 같은 이유다).
	 */
	const TCHAR* EngineSpherePath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	const TCHAR* EngineConePath = TEXT("/Engine/BasicShapes/Cone.Cone");

	/** 이 각도 이상 어긋나면 정면 규약이 깨진 것으로 보고 진단에 적는다. */
	constexpr float FrontAxisWarnDegrees = 45.f;

	/** 메시 하나가 목표 높이가 되도록 등비 스케일을 구한다. 바운드가 0 이면 1 을 돌려준다. */
	float ComputeUniformScaleForHeight(const UStaticMesh& Mesh, float TargetHeight)
	{
		const double SourceHeight = Mesh.GetBounds().BoxExtent.Z * 2.0;
		if (SourceHeight <= UE_KINDA_SMALL_NUMBER)
		{
			return 1.f;
		}
		return static_cast<float>(TargetHeight / SourceHeight);
	}

	/** 조형물을 지표에 세우는 데 필요한 스케일과 들어올림. */
	struct FAtlasLandmarkFit
	{
		float Scale = 1.f;
		double LiftZ = 0.0;
	};

	/**
	 * 스케일과 들어올림을 **함께** 구한다.
	 *
	 * 🔴 둘을 따로 계산하면 반드시 어긋난다. 들어올림은 스케일에 비례하므로
	 * (밑면이 표면에 닿으려면 `(BoxExtent.Z - Origin.Z) * Scale` 만큼 올려야 한다),
	 * 선택으로 스케일만 키우고 위치를 그대로 두면 **조형물이 지표에 파묻힌다.**
	 */
	FAtlasLandmarkFit ComputeLandmarkFit(const UStaticMesh& Mesh, float TargetHeight, float ExtraScale)
	{
		FAtlasLandmarkFit Fit;
		Fit.Scale = ComputeUniformScaleForHeight(Mesh, TargetHeight) * ExtraScale;

		// 메시 원점이 가운데인 프리미티브를 그대로 붙이면 절반이 묻힌다. 바운드로 들어 올린다.
		const FBoxSphereBounds Bounds = Mesh.GetBounds();
		Fit.LiftZ = (Bounds.BoxExtent.Z - Bounds.Origin.Z) * static_cast<double>(Fit.Scale);
		return Fit;
	}
}

AFF_AtlasGlobeActor::AFF_AtlasGlobeActor()
{
	// 자전과 회전 보간이 매 프레임 필요하다. 게임 모듈에서 Tick 하는 액터는 카메라 Pawn 하나뿐이라
	// 여기서도 그 관례를 따라 TickGroup 을 명시하고, 숨길 때는 `SetGlobeActive(false)` 로 끈다.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	// 🔴 콜리전을 켜면 지구가 화면 전체의 월드 클릭을 먹는다. 국가 클릭 판정은 UMG 핀 버튼이 한다.
	SetActorEnableCollision(false);

	// 레벨에 놓은 방향(= 정면 규약)을 지키는 고정 루트. 회전은 이 아래에서만 일어난다.
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	GlobeRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GlobeRoot"));
	GlobeRoot->SetupAttachment(SceneRoot);

	GlobeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GlobeMesh"));
	GlobeMesh->SetupAttachment(GlobeRoot);
	GlobeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GlobeMesh->SetCastShadow(false);

	CloudMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CloudMesh"));
	CloudMesh->SetupAttachment(GlobeRoot);
	CloudMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CloudMesh->SetCastShadow(false);

#if WITH_EDITORONLY_DATA
	// 정면(+X)이 어디인지 에디터에서 보이게 한다. 이 화살표를 카메라 쪽으로 돌려 놓으면 끝이다.
	FrontArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("FrontArrow"));
	FrontArrow->SetupAttachment(SceneRoot);
	FrontArrow->ArrowColor = FColor(255, 96, 64);
	FrontArrow->ArrowSize = 4.f;
	FrontArrow->bIsScreenSizeScaled = true;
#endif

	// 에셋 슬롯의 기본값. 에디터에서 비워도 여기 것이 다시 들어오지는 않지만,
	// 처음 배치했을 때 아무것도 안 보이는 상태를 피한다.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(EngineSpherePath);
	if (SphereFinder.Succeeded())
	{
		GlobeMeshAsset = SphereFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(EngineConePath);
	if (ConeFinder.Succeeded())
	{
		FallbackLandmarkMesh = ConeFinder.Object;
	}
}

void AFF_AtlasGlobeActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 에디터에서 반지름·구름 배율·텍스처 보정을 돌리면 바로 보인다.
	ApplyGlobeVisuals();

	// 🔴 시작 각도를 에디터에서도 적용한다. 이게 없으면 뷰포트는 회전 0(위경도 0,0 = 기니 만)을
	//    보여주고 PIE 는 시작 지점을 보여줘서 **둘이 완전히 다른 면**이 된다.
	//    텍스처 정렬을 뷰포트에서 맞출 수 있게 하려면 반드시 같아야 한다.
	Orientation = ComputeStartOrientation();
	ApplyOrientationToScene();
}

void AFF_AtlasGlobeActor::BeginPlay()
{
	Super::BeginPlay();

	Accessibility = UFF_UIAccessibilitySubsystem::Get(this);
	if (!Accessibility.IsValid())
	{
		// 치명적이지 않다 — 접근성 판정 없이 기본 연출로 돈다. 다만 조용히 넘기지는 않는다.
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("AFF_AtlasGlobeActor: 접근성 서브시스템을 찾지 못했다. `움직임 감소` 가 지구본에 "
				 "걸리지 않는다."));
	}

	if (!GlobeMeshAsset)
	{
		// 이것만은 없으면 아무것도 안 보인다. 조용히 넘기지 않는다.
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("AFF_AtlasGlobeActor: 지구 메시가 비어 있다. 액터의 Globe Mesh Asset 에 구체 메시를 "
				 "넣는다 (기본값은 /Engine/BasicShapes/Sphere 다)."));
	}

	ApplyGlobeVisuals();
	RebuildPins();

	// 시작 각도. `OnConstruction` 과 **같은 함수**를 쓰므로 에디터에서 본 각도가 그대로 나온다.
	Orientation = ComputeStartOrientation();

	// 🔴 **시작 정렬은 조작이 아니다.** 예전에는 여기서 `FocusOnCountry` 를 불렀는데,
	// 그게 재개 타이머를 되감아서 지구본에 들어온 직후 재개 지연(기본 4초)만큼 지구가
	// 멈춰 서 있었다 (2026-08-23 실측). 각도만 직접 세우고 타이머는 건드리지 않는다.
	bHasReceivedInput = false;
	SecondsSinceInput = 0.f;

	// 시작 지점이 국가면 그 국가를 포커스로 둔다 — 화면 왼쪽 패널이 처음부터 그 나라를 띄운다.
	if (bStartFacingFirstCountry && Pins.Num() > 0)
	{
		SetFocusedCountryInternal(Pins[0].CountryId);
	}

	ApplyOrientationToScene();
	UpdatePinPresentation();
}

void AFF_AtlasGlobeActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 해제는 조건부, 정리는 무조건이다 (안티패턴 3).
	DestroyPins();
	Accessibility.Reset();

	Super::EndPlay(EndPlayReason);
}

AFF_AtlasGlobeActor* AFF_AtlasGlobeActor::Find(const UObject* WorldContextObject)
{
	UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	if (!World)
	{
		// 조용히 나가면 "핀이 하나도 안 뜬다" 는 증상만 남고, 레벨 배치 문제인지 호출 컨텍스트
		// 문제인지 구분할 단서가 없다.
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("AFF_AtlasGlobeActor::Find: 월드를 찾을 수 없다 (컨텍스트 %s). "
				 "이미 뷰포트에서 내려간 위젯이나 CDO 에서 부른 것이 아닌지 확인한다."),
			WorldContextObject ? *WorldContextObject->GetName() : TEXT("(없음)"));
		return nullptr;
	}

	for (TActorIterator<AFF_AtlasGlobeActor> It(World); It; ++It)
	{
		return *It;
	}

	UE_LOG(LogFestaFiestaUI, Warning,
		TEXT("AFF_AtlasGlobeActor::Find: 이 레벨에 지구본 액터가 없다. "
			 "BP_FF_AtlasGlobe 을 프론트엔드 레벨에 배치한다."));
	return nullptr;
}

void AFF_AtlasGlobeActor::SetGlobeActive(bool bActive)
{
	SetActorHiddenInGame(!bActive);
	SetActorTickEnabled(bActive);

	if (!bActive)
	{
		// 진행하던 연출은 **버린다.** `SkipTransition` 으로 끝내면 화면이 이미 넘어간 뒤에
		// "도착했다" 는 알림이 울려서, 국가 지도로 넘어간 순간 지구본 패널이 되살아난다.
		CancelTransition();
	}
}

void AFF_AtlasGlobeActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UFF_AtlasSettings& Settings = GetAtlasSettings();

	if (bTransitioning)
	{
		TransitionElapsed += DeltaSeconds;

		if (TransitionDuration <= UE_KINDA_SMALL_NUMBER || TransitionElapsed >= TransitionDuration)
		{
			SkipTransition();
		}
		else
		{
			Orientation = FFF_AtlasGeo::InterpolateOrientation(
				TransitionFrom, TransitionTo, TransitionElapsed / TransitionDuration);
		}
	}
	else
	{
		SecondsSinceInput += DeltaSeconds;

		// 손을 뗀 직후에 지구가 스스로 움직이면 조작을 빼앗긴 느낌이 된다. 그래서 지연이 있다.
		// ★ 단 **조작이 한 번이라도 있었을 때만** 기다린다. 첫 진입부터 지연을 적용하면
		//   지구본에 들어온 직후 몇 초간 지구가 멈춰 있어 배경이 죽은 것처럼 보인다.
		const bool bWaitingForResume =
			bHasReceivedInput && (SecondsSinceInput < Settings.AutoSpinResumeDelaySeconds);

		if (IsAutoSpinAllowed() && SelectedCountryId.IsNone() && !bWaitingForResume)
		{
			// 🔴 부호가 음수인 것이 **실제 지구 자전 방향**이다.
			// 지구는 동쪽으로 돌고, 우리 규약에서 동쪽은 화면 오른쪽이므로 지표가 화면
			// 오른쪽으로 흘러야 한다. 그것은 정면 경도가 서쪽으로 가는 것과 같다(= Spin 감소).
			// 부호를 뒤집으면 해가 서쪽에서 뜨는 지구가 된다.
			Orientation.Spin -= Settings.AutoSpinDegreesPerSecond * DeltaSeconds;
			Orientation = FFF_AtlasGeo::NormalizeOrientation(Orientation, Settings.MaxTiltDegrees);
		}
	}

	// 구름은 지표에 대해 천천히 반대로 흐른다. `움직임 감소` 면 멈춘다.
	if (IsAutoSpinAllowed() && CloudMesh)
	{
		CloudSpin = FMath::UnwindDegrees(
			CloudSpin + Settings.CloudRelativeSpinDegreesPerSecond * DeltaSeconds);
		CloudMesh->SetRelativeRotation(FRotator(0.f, CloudSpin, 0.f));
	}

	ApplyOrientationToScene();
	UpdatePinPresentation();
}

// --- 조작 ---

void AFF_AtlasGlobeActor::AddDragInput(const FVector2D& DragPixels)
{
	SetSelectedCountry(NAME_None);
	const UFF_AtlasSettings& Settings = GetAtlasSettings();

	// 손으로 돌리는 중에 연출이 겹치면 지구가 두 방향으로 끌린다. 진행 중인 회전은 버린다.
	// 포커스도 함께 잃는다 — 드래그로 돌린 뒤에 이전 국가가 포커스로 남아 있으면 화면이
	// 정면에 없는 나라 이름을 계속 띄운다.
	CancelTransition();

	Orientation = FFF_AtlasGeo::ApplyDrag(
		Orientation, DragPixels, Settings.DragDegreesPerPixel, Settings.MaxTiltDegrees);

	MarkInputReceived();
	ApplyOrientationToScene();
}

void AFF_AtlasGlobeActor::FocusOnCountry(FName CountryId, bool bInstant)
{
	const int32 PinIndex = FindPinIndex(CountryId);
	if (!Pins.IsValidIndex(PinIndex))
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("AFF_AtlasGlobeActor::FocusOnCountry: `%s` 핀이 없다. DA_FestivalAtlas 의 "
				 "Countries 에 그 CountryId 가 있는지 확인한다."),
			CountryId.IsNone() ? TEXT("(없음)") : *CountryId.ToString());
		return;
	}

	TransitionFrom = Orientation;

	// ★ 목표를 만드는 지점에서 기울기를 클램프한다. 목표를 안 누르고 도착할 때만 누르면
	//   보간 중에 한계를 넘어갔다가 마지막 프레임에 툭 되돌아온다.
	TransitionTo = FFF_AtlasGeo::MakeFocusOrientation(
		Pins[PinIndex].Location, GetAtlasSettings().MaxTiltDegrees);

	TransitionTargetCountryId = CountryId;
	TransitionElapsed = 0.f;

	// ★ 거리와 무관하게 같은 시간이다. 비례시키면 가까운 나라는 튀고 먼 나라는 늘어진다.
	TransitionDuration = bInstant ? 0.f : GetEffectiveTransitionSeconds();
	bTransitioning = true;

	MarkInputReceived();

	// 0초면 이 프레임에 끝낸다 — Tick 을 기다리면 즉시 이동이 한 프레임 늦는다.
	if (TransitionDuration <= UE_KINDA_SMALL_NUMBER)
	{
		SkipTransition();
	}
	else
	{
		// 아직 도착하지 않았으므로 포커스는 없다. "가고 있는 중" 을 도착과 같은 값으로 두면
		// 화면이 아직 오지 않은 나라를 미리 띄운다.
		SetFocusedCountryInternal(NAME_None);
	}
}

void AFF_AtlasGlobeActor::StepFocus(int32 ScreenDirection)
{
	if (Pins.Num() == 0)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("AFF_AtlasGlobeActor::StepFocus: 핀이 없다. DA_FestivalAtlas 의 Countries 가 비어 있다."));
		return;
	}

	TArray<FFF_AtlasLatLong> Locations;
	GetPinLocations(Locations);

	// ★ 전환 중이면 **목표**가 기준이다. 보간 중인 각도를 기준으로 하면 이미 향해 가고 있는
	//   핀이 여전히 그 방향의 최근접이라 같은 핀을 다시 고르고 연출만 되돌린다.
	//   판정은 순수 함수가 갖는다 (`StepReferenceLongitude` 테스트 참고).
	const float ReferenceLongitude = FFF_AtlasGeo::StepReferenceLongitude(
		FFF_AtlasGeo::OrientationToFrontLatLong(Orientation).Longitude,
		TransitionTo.Spin,
		bTransitioning);

	// 동률(경도가 같은 핀)을 순서로 깨기 위해 지금 기준이 되는 핀을 알려 준다.
	const int32 CurrentIndex = bTransitioning
		? FindPinIndex(TransitionTargetCountryId)
		: FindPinIndex(FocusedCountryId);

	const int32 NextIndex = FFF_AtlasGeo::FindAdjacentPinIndex(
		Locations, ReferenceLongitude, ScreenDirection, CurrentIndex);

	if (Pins.IsValidIndex(NextIndex))
	{
		FocusOnCountry(Pins[NextIndex].CountryId);
	}
}

void AFF_AtlasGlobeActor::ReSnapToNearestCountry()
{
	if (Pins.Num() == 0)
	{
		// StepFocus 와 같은 조건인데 여기만 조용히 나가면, 좌우 입력은 이유를 말해 주고
		// 스틱 재스냅만 먹지 않는 것으로 보인다 (안티패턴 1).
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("AFF_AtlasGlobeActor::ReSnapToNearestCountry: 핀이 없다. "
				 "DA_FestivalAtlas 의 Countries 가 비어 있다."));
		return;
	}

	TArray<FFF_AtlasLatLong> Locations;
	GetPinLocations(Locations);

	const int32 NearestIndex = FFF_AtlasGeo::FindNearestPinIndex(
		Locations, FFF_AtlasGeo::OrientationToFrontLatLong(Orientation));

	if (Pins.IsValidIndex(NearestIndex))
	{
		FocusOnCountry(Pins[NearestIndex].CountryId);
	}
}

void AFF_AtlasGlobeActor::SetSelectedCountry(FName CountryId)
{
	if (SelectedCountryId == CountryId)
	{
		// 같은 값을 다시 넣으면 아무 일도 하지 않는다. 방송이 두 번 울리면 화면이 두 번 갱신된다.
		return;
	}

	if (!CountryId.IsNone())
	{
		const bool bHasPin = Pins.ContainsByPredicate(
			[CountryId](const FFF_AtlasPinInstance& Pin) { return Pin.CountryId == CountryId; });
		if (!bHasPin)
		{
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("AFF_AtlasGlobeActor::SetSelectedCountry: `%s` 핀이 없다. 선택하지 않는다."),
				*CountryId.ToString());
			return;
		}
	}

	SelectedCountryId = CountryId;
	if (CountryId.IsNone())
	{
		MarkInputReceived();
	}
	if (AFF_AtlasCameraRig* Rig = AFF_AtlasCameraRig::Find(this, false))
	{
		Rig->SetCountryInspection(!CountryId.IsNone());
	}
	ApplySelectionVisuals();
	OnSelectedCountryChanged.Broadcast(SelectedCountryId);
}

void AFF_AtlasGlobeActor::SkipTransition()
{
	if (!bTransitioning)
	{
		return;
	}

	bTransitioning = false;

	// `TransitionTo` 는 이미 클램프된 목표다 (`MakeFocusOrientation`). 여기서 다시 누르지 않는다.
	Orientation = TransitionTo;
	ApplyOrientationToScene();

	const FName ArrivedCountryId = TransitionTargetCountryId;
	TransitionTargetCountryId = NAME_None;

	SetFocusedCountryInternal(ArrivedCountryId);
}

void AFF_AtlasGlobeActor::CancelTransition()
{
	bTransitioning = false;
	TransitionTargetCountryId = NAME_None;

	// 지금 각도에 그대로 멈춘다. 정면에 국가가 없으므로 포커스도 없다.
	SetFocusedCountryInternal(NAME_None);
}

void AFF_AtlasGlobeActor::SetOrientation(const FFF_AtlasGlobeOrientation& NewOrientation)
{
	// 각도를 직접 되살리는 경로다. 진행 중인 연출은 버린다 — 그러지 않으면 복원한 각도 위로
	// 이전 목표가 다시 끌어당긴다.
	CancelTransition();

	Orientation = FFF_AtlasGeo::NormalizeOrientation(NewOrientation, GetAtlasSettings().MaxTiltDegrees);
	MarkInputReceived();

	ApplyOrientationToScene();
	UpdatePinPresentation();
}

void AFF_AtlasGlobeActor::SetFocusedCountryInternal(FName CountryId)
{
	if (FocusedCountryId == CountryId)
	{
		// 같은 값으로는 방송하지 않는다. 드래그는 매 프레임 불리므로 이 가드가 없으면
		// 구독자가 초당 수십 번 갱신된다.
		return;
	}

	FocusedCountryId = CountryId;
	OnCountryFocused.Broadcast(FocusedCountryId);
}
void AFF_AtlasGlobeActor::MarkInputReceived()
{
	SecondsSinceInput = 0.f;
	bHasReceivedInput = true;
}
// --- 조회 ---

void AFF_AtlasGlobeActor::GetPinStates(TArray<FFF_AtlasPinState>& OutPins) const
{
	OutPins.Reset(Pins.Num());

	const float MinDot = ComputePinVisibleMinDot();

	for (const FFF_AtlasPinInstance& Pin : Pins)
	{
		FFF_AtlasPinState State;
		State.CountryId = Pin.CountryId;
		State.bSelectable = Pin.bSelectable;
		State.bFacingViewer = FFF_AtlasGeo::IsPinFacingFront(Pin.Location, Orientation, MinDot);
		State.WorldLocation = Pin.Anchor ? Pin.Anchor->GetComponentLocation() : GetActorLocation();

		OutPins.Add(State);
	}
}

bool AFF_AtlasGlobeActor::GetCountryPinWorldLocation(FName CountryId, FVector& OutLocation) const
{
	const int32 PinIndex = FindPinIndex(CountryId);
	if (!Pins.IsValidIndex(PinIndex) || !Pins[PinIndex].Anchor)
	{
		return false;
	}

	OutLocation = Pins[PinIndex].Anchor->GetComponentLocation();
	return true;
}

int32 AFF_AtlasGlobeActor::FindPinIndex(FName CountryId) const
{
	if (CountryId.IsNone())
	{
		return INDEX_NONE;
	}

	return Pins.IndexOfByPredicate(
		[CountryId](const FFF_AtlasPinInstance& Pin) { return Pin.CountryId == CountryId; });
}

void AFF_AtlasGlobeActor::GetPinLocations(TArray<FFF_AtlasLatLong>& OutLocations) const
{
	OutLocations.Reset(Pins.Num());
	for (const FFF_AtlasPinInstance& Pin : Pins)
	{
		OutLocations.Add(Pin.Location);
	}
}

FString AFF_AtlasGlobeActor::DescribeState() const
{
	const FFF_AtlasLatLong Front = FFF_AtlasGeo::OrientationToFrontLatLong(Orientation);

	FString FrontAxisNote = TEXT("카메라 없음");
	FVector ViewerLocation = FVector::ZeroVector;
	if (GetViewerLocation(ViewerLocation))
	{
		const FVector ToViewer = (ViewerLocation - GetActorLocation()).GetSafeNormal();
		const double Alignment = FVector::DotProduct(GetActorForwardVector(), ToViewer);
		const float AngleDegrees = static_cast<float>(
			FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Alignment, -1.0, 1.0))));

		// 🔴 정면 규약이 깨진 상태를 여기서 알려 준다. 이게 어긋나면 핀·조형물·앞뒤 판정이
		//    전부 그럴싸하게 틀리고, 원인을 찾기가 아주 어렵다.
		FrontAxisNote = (AngleDegrees > FrontAxisWarnDegrees)
			? FString::Printf(
				TEXT("🔴 정면축 %.0f도 어긋남 — 액터의 +X 화살표가 카메라를 향하게 돌린다"), AngleDegrees)
			: FString::Printf(TEXT("정면축 %.0f도 (정상)"), AngleDegrees);
	}

	return FString::Printf(
		TEXT("Pins=%d Front=(위도 %.2f, 경도 %.2f) Spin=%.2f Tilt=%.2f "
			 "Focused=%s Selected=%s Transition=%s PinMinDot=%.3f LandmarkMinDot=%.3f 자동자전=%s | %s"),
		Pins.Num(), Front.Latitude, Front.Longitude, Orientation.Spin, Orientation.Tilt,
		FocusedCountryId.IsNone() ? TEXT("(없음)") : *FocusedCountryId.ToString(),
		SelectedCountryId.IsNone() ? TEXT("(없음)") : *SelectedCountryId.ToString(),
		bTransitioning ? TEXT("진행 중") : TEXT("없음"),
		ComputePinVisibleMinDot(),
		ComputeLandmarkVisibleMinDot(),
		bAutoSpinPaused ? TEXT("일시정지") : (IsAutoSpinAllowed() ? TEXT("켬") : TEXT("끔 (움직임 감소)")),
		*FrontAxisNote);
}

// --- 내부 ---

const UFF_AtlasSettings& AFF_AtlasGlobeActor::GetAtlasSettings() const
{
	// `GetDefault` 는 CDO 를 돌려주므로 항상 유효하다. nullptr 검사가 필요 없다.
	const UFF_AtlasSettings* Settings = UFF_AtlasSettings::Get();
	check(Settings);
	return *Settings;
}

const UFF_FestivalAtlasCatalog* AFF_AtlasGlobeActor::ResolveCatalog() const
{
	// 액터 인스턴스의 지정이 먼저다. 이 액터만 다른 카탈로그로 시험해 볼 수 있게 한다.
	if (!CatalogOverride.IsNull())
	{
		if (const UFF_FestivalAtlasCatalog* Override = CatalogOverride.LoadSynchronous())
		{
			return Override;
		}
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("AFF_AtlasGlobeActor: CatalogOverride 를 로드할 수 없다 (%s). 흐름 설정 쪽을 쓴다."),
			*CatalogOverride.ToString());
	}

	const UFF_UIFlowSettings* FlowSettings = UFF_UIFlowSettings::Get();
	if (!FlowSettings || FlowSettings->AtlasCatalog.IsNull())
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("AFF_AtlasGlobeActor: Atlas 카탈로그가 지정되지 않았다. "
				 "Project Settings > Game > FestaFiesta UI Flow 에서 지정한다."));
		return nullptr;
	}

	const UFF_FestivalAtlasCatalog* Catalog = FlowSettings->AtlasCatalog.LoadSynchronous();
	if (!Catalog)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("AFF_AtlasGlobeActor: Atlas 카탈로그를 로드할 수 없다 (%s)."),
			*FlowSettings->AtlasCatalog.ToString());
	}
	return Catalog;
}

void AFF_AtlasGlobeActor::ApplyGlobeVisuals()
{
	if (!GlobeMesh || !CloudMesh)
	{
		return;
	}

	GlobeMesh->SetStaticMesh(GlobeMeshAsset);

	// 🔴 슬롯을 **비웠을 때도** 이전 값을 지운다. 조건부로만 적용하면 에디터에서 슬롯을 비운 뒤에도
	//    앞서 넣은 머티리얼·메시가 남아, 비운 것이 화면에 반영되지 않는다.
	//    null 오버라이드는 메시의 기본 머티리얼로 돌아가는 것이 UE 의 동작이다.
	GlobeMesh->SetMaterial(0, GlobeMaterial);

	// 구름은 지구 메시와 머티리얼이 **둘 다** 있을 때만 띄운다.
	// 불투명 기본 머티리얼로 켜면 지구를 덮어 버리고, 지구 메시를 비운 채 구름만 남기면
	// 속이 빈 껍데기가 떠 있게 된다.
	const bool bShowClouds = (CloudMaterial != nullptr) && (GlobeMeshAsset != nullptr);
	CloudMesh->SetStaticMesh(bShowClouds ? GlobeMeshAsset : nullptr);
	CloudMesh->SetMaterial(0, bShowClouds ? CloudMaterial : nullptr);
	CloudMesh->SetVisibility(bShowClouds);

	if (!GlobeMeshAsset)
	{
		// 비면 아무것도 안 보인다. 경고는 `BeginPlay` 가 한 번만 남긴다 —
		// 이 함수는 에디터에서 속성을 만질 때마다 불리므로 여기서 찍으면 로그가 넘친다.
		return;
	}

	// 메시가 무엇이든 요청한 반지름이 되게 맞춘다. 구체 바운드는 반지름이므로 지름으로 환산한다.
	const float GlobeScale = ComputeUniformScaleForHeight(*GlobeMeshAsset, GlobeRadius * 2.f);
	GlobeMesh->SetRelativeScale3D(FVector(GlobeScale));
	GlobeMesh->SetRelativeRotation(FRotator(0.f, GlobeTextureYawOffset, 0.f));

	if (bShowClouds)
	{
		CloudMesh->SetRelativeScale3D(FVector(GlobeScale * CloudRadiusScale));
	}
}

void AFF_AtlasGlobeActor::DestroyPins()
{
	for (FFF_AtlasPinInstance& Pin : Pins)
	{
		// 자식부터 지운다. 앵커를 먼저 지우면 붙어 있던 것들이 루트로 올라붙는다.
		if (Pin.Light)
		{
			Pin.Light->DestroyComponent();
		}
		if (Pin.Landmark)
		{
			Pin.Landmark->DestroyComponent();
		}
		if (Pin.Anchor)
		{
			Pin.Anchor->DestroyComponent();
		}
	}
	Pins.Reset();
}

void AFF_AtlasGlobeActor::RebuildPins()
{
	DestroyPins();

	const UFF_FestivalAtlasCatalog* Catalog = ResolveCatalog();
	if (!Catalog || !GlobeRoot)
	{
		// 핀이 전부 사라졌으므로 여기서도 상태를 맞춘다 — 빠져나가는 경로마다 맞춰야 한다.
		ReconcileStateWithPins();
		return;
	}

	const TArray<FFF_AtlasCountryEntry>& Countries = Catalog->GetCountries();
	if (Countries.Num() == 0)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("AFF_AtlasGlobeActor: 카탈로그에 국가가 없다. 지구만 돌고 핀은 하나도 없다 — "
				 "DA_FestivalAtlas 의 Countries 를 채운다."));
		ReconcileStateWithPins();
		return;
	}

	Pins.Reserve(Countries.Num());

	TSet<FName> SeenCountryIds;

	for (const FFF_AtlasCountryEntry& Country : Countries)
	{
		if (Country.CountryId.IsNone())
		{
			// 조인 키가 없으면 어느 축제와도 이어지지 않는다. 핀을 만들면 눌러도 갈 곳이 없다.
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("AFF_AtlasGlobeActor: CountryId 가 빈 국가 항목을 건너뛴다."));
			continue;
		}

		if (SeenCountryIds.Contains(Country.CountryId))
		{
			// 핀을 둘 만들면 조회(`FindPinIndex`)가 앞의 것만 돌려주므로 뒤의 핀은 눌러도
			// 아무 반응이 없는 유령이 된다. 만들지 않는 편이 낫다.
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("AFF_AtlasGlobeActor: CountryId `%s` 가 중복이라 뒤 항목을 건너뛴다."),
				*Country.CountryId.ToString());
			continue;
		}
		SeenCountryIds.Add(Country.CountryId);

		FFF_AtlasPinInstance Pin;
		Pin.CountryId = Country.CountryId;
		Pin.Location = FFF_AtlasGeo::NormalizeLatLong(Country.Location);
		Pin.bSelectable = Country.bUnlocked;

		const FVector SurfaceLocation = FFF_AtlasGeo::LatLongToSurfacePosition(Pin.Location, GlobeRadius);
		const FVector SurfaceNormal = FFF_AtlasGeo::LatLongToUnitVector(Pin.Location);

		// 런타임에 만드는 컴포넌트는 등록 후에 붙인다. 그리고 `AddInstanceComponent` 는 부르지
		// 않는다 — 에디터에서 레벨에 직렬화되어 `.uasset` 충돌로 남을 수 있고, 팀 공용 레벨에서
		// 그 위험을 감수할 이득이 없다.
		Pin.Anchor = NewObject<USceneComponent>(this);
		Pin.Anchor->RegisterComponent();
		Pin.Anchor->AttachToComponent(GlobeRoot, FAttachmentTransformRules::KeepRelativeTransform);
		Pin.Anchor->SetRelativeLocation(SurfaceLocation);
		Pin.Anchor->SetRelativeRotation(FFF_AtlasGeo::MakeSurfaceUpQuat(SurfaceNormal));

		// 조형물 메시. 실제 메시가 없으면 프리미티브로 대신 세운다 (2026-08-21 결정).
		// `bUseFallbackLandmark` 를 끄면 계획서의 "비어 있으면 붙이지 않는다" 대로 동작한다.
		UStaticMesh* LandmarkMesh = Country.LandmarkMesh.LoadSynchronous();
		if (!LandmarkMesh && bUseFallbackLandmark)
		{
			LandmarkMesh = FallbackLandmarkMesh;
		}

		if (LandmarkMesh)
		{
			const float TargetHeight = Country.LandmarkHeightOverride > 0.f ? Country.LandmarkHeightOverride : LandmarkHeight;
			float LandmarkScale = ComputeUniformScaleForHeight(*LandmarkMesh, TargetHeight);
			const FVector MeshSize = LandmarkMesh->GetBounds().BoxExtent * 2.0;
			const double Width = FMath::Max(MeshSize.X, MeshSize.Y);
			if (Country.LandmarkMaxWidth > 0.f && Width > UE_SMALL_NUMBER)
			{
				LandmarkScale = FMath::Min(LandmarkScale, static_cast<float>(Country.LandmarkMaxWidth / Width));
			}
			Pin.BaseLandmarkScale = LandmarkScale;
			Pin.BaseLandmarkHeight = static_cast<float>(MeshSize.Z) * LandmarkScale;
			Pin.LandmarkSurfaceOffset = FMath::Max(0.f, Country.LandmarkSurfaceOffset);
			Pin.LandmarkHeading = Country.LandmarkHeading;

			// 메시 원점이 가운데인 프리미티브를 그대로 붙이면 절반이 지표에 묻힌다.
			// 바운드로 들어 올려 밑면이 표면에 닿게 한다.
			const FBoxSphereBounds Bounds = LandmarkMesh->GetBounds();
			const double LiftLocal = Bounds.BoxExtent.Z - Bounds.Origin.Z;

			Pin.Landmark = NewObject<UStaticMeshComponent>(this);
			Pin.Landmark->RegisterComponent();
			Pin.Landmark->AttachToComponent(Pin.Anchor, FAttachmentTransformRules::KeepRelativeTransform);
			Pin.Landmark->SetStaticMesh(LandmarkMesh);
			Pin.Landmark->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Pin.Landmark->SetRelativeLocation(FVector(0.0, 0.0, LiftLocal * LandmarkScale + Pin.LandmarkSurfaceOffset));
			Pin.Landmark->SetRelativeRotation(FRotator(0.f, Pin.LandmarkHeading, 0.f));
			Pin.Landmark->SetRelativeScale3D(FVector(LandmarkScale));

			// 선택 표시용 조명. 기본은 꺼져 있다.
			//
			// 🔴 **앵커에 붙인다. 조형물에 붙이면 안 된다.** 조형물은 메시 원본 크기에 맞춰
			//    스케일이 들어가 있고(원뿔이면 5배), 자식의 상대 위치와 감쇠 반경에는 부모
			//    스케일이 곱해진다. 조형물에 붙이면 조명이 목표 높이의 5배 위에 뜨고,
			//    선택으로 스케일이 커질 때마다 조명 높이까지 함께 움직인다.
			Pin.Light = NewObject<UPointLightComponent>(this);
			Pin.Light->RegisterComponent();
			Pin.Light->AttachToComponent(Pin.Anchor, FAttachmentTransformRules::KeepRelativeTransform);
			Pin.Light->SetRelativeLocation(FVector(0.0, 0.0, LandmarkHeight));
			Pin.Light->SetIntensity(SelectionLightIntensity);
			Pin.Light->SetLightColor(SelectionLightColor);
			Pin.Light->SetAttenuationRadius(LandmarkHeight * 4.f);
			Pin.Light->SetCastShadows(false);
			Pin.Light->SetVisibility(false);
		}

		Pins.Add(Pin);
	}

	UE_LOG(LogFestaFiestaUI, Log,
		TEXT("AFF_AtlasGlobeActor: 핀 %d개를 만들었다 (국가 항목 %d개)."), Pins.Num(), Countries.Num());

	ReconcileStateWithPins();

	ApplySelectionVisuals();
	UpdatePinPresentation();
}

void AFF_AtlasGlobeActor::ReconcileStateWithPins()
{
	// 카탈로그를 고치고 `FF.Atlas.Rebuild` 를 부르는 것이 문서가 권하는 작업 흐름인데,
	// 그때 사라진 국가가 선택·포커스·전환 목표로 남으면 **조형물도 조명도 없는데 화면은 계속
	// 그 나라를 그린다.** 전환이 살아 있으면 더 나쁘다 — 존재하지 않는 국가로 도착 알림이 울리고
	// 화면이 이어서 묻는 핀 위치는 실패한다.
	if (bTransitioning && FindPinIndex(TransitionTargetCountryId) == INDEX_NONE)
	{
		CancelTransition();
	}

	// 같은 값이면 방송하지 않는 가드가 있으므로 원래 비어 있었으면 조용하다.
	if (FindPinIndex(FocusedCountryId) == INDEX_NONE)
	{
		SetFocusedCountryInternal(NAME_None);
	}

	if (!SelectedCountryId.IsNone() && FindPinIndex(SelectedCountryId) == INDEX_NONE)
	{
		// 구독자에게 선택이 사라졌음을 알려야 한다. 조용히 지우면 화면이 stale 상태를 못 안다.
		SetSelectedCountry(NAME_None);
	}
}

void AFF_AtlasGlobeActor::ApplyOrientationToScene()
{
	if (GlobeRoot)
	{
		GlobeRoot->SetRelativeRotation(FFF_AtlasGeo::OrientationToQuat(Orientation));
	}
}

FFF_AtlasGlobeOrientation AFF_AtlasGlobeActor::ComputeStartOrientation() const
{
	const float MaxTilt = GetAtlasSettings().MaxTiltDegrees;

	if (bStartFacingFirstCountry)
	{
		// 핀이 이미 만들어져 있으면 그것을 쓴다 (런타임). 에디터에는 핀이 없으므로 카탈로그를 읽는다.
		if (Pins.Num() > 0)
		{
			return FFF_AtlasGeo::MakeFocusOrientation(Pins[0].Location, MaxTilt);
		}

		if (const UFF_FestivalAtlasCatalog* Catalog = ResolveCatalog())
		{
			for (const FFF_AtlasCountryEntry& Country : Catalog->GetCountries())
			{
				if (!Country.CountryId.IsNone())
				{
					return FFF_AtlasGeo::MakeFocusOrientation(Country.Location, MaxTilt);
				}
			}
		}
		// 국가가 없으면 아래로 떨어져 `StartLatLong` 을 쓴다. 빈 카탈로그로 (0,0) 을 보는 것보다
		// 사용자가 지정한 각도를 존중하는 편이 낫다.
	}

	return FFF_AtlasGeo::MakeFocusOrientation(StartLatLong, MaxTilt);
}

void AFF_AtlasGlobeActor::SetAutoSpinPaused(bool bPaused)
{
	bAutoSpinPaused = bPaused;

	UE_LOG(LogFestaFiestaUI, Log,
		TEXT("AFF_AtlasGlobeActor: 자동 자전 %s."), bPaused ? TEXT("일시정지") : TEXT("재개"));
}

bool AFF_AtlasGlobeActor::GetViewerLocation(FVector& OutLocation) const
{
	APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);
	if (!Controller || !Controller->PlayerCameraManager)
	{
		return false;
	}

	OutLocation = Controller->PlayerCameraManager->GetCameraLocation();
	return true;
}

float AFF_AtlasGlobeActor::ComputePinVisibleMinDot() const
{
	const float Margin = GetAtlasSettings().PinVisibleDotMargin;

	FVector ViewerLocation = FVector::ZeroVector;
	if (!GetViewerLocation(ViewerLocation))
	{
		// 카메라를 모르면 원근 보정을 할 근거가 없다. 여유값만 쓴다.
		return Margin;
	}

	// 🔴 거리는 월드 단위인데 `GlobeRadius` 는 액터 로컬이다. 액터에 스케일이 걸려 있으면
	//    실제 지구는 그만큼 크므로 반지름도 환산해야 한다 — 안 하면 스케일 2배인 액터에서
	//    실루엣 기준이 절반으로 계산되어 뒤편 핀이 테두리 밖으로 튀어나온다.
	//    구체를 비등방으로 늘리는 것은 지원하지 않으므로 최대 성분을 쓴다.
	const float WorldRadius = GlobeRadius * static_cast<float>(GetActorScale3D().GetAbsMax());
	const float Distance = static_cast<float>(FVector::Dist(ViewerLocation, GetActorLocation()));

	return FMath::Clamp(FFF_AtlasGeo::HorizonMinDot(WorldRadius, Distance) + Margin, -1.f, 1.f);
}

float AFF_AtlasGlobeActor::ComputeLandmarkVisibleMinDot(float HeightScale) const
{
	const float Margin = GetAtlasSettings().LandmarkVisibleDotMargin;

	FVector ViewerLocation = FVector::ZeroVector;
	if (!GetViewerLocation(ViewerLocation))
	{
		return Margin;
	}

	// 반지름·높이·카메라 거리를 모두 월드 단위로 맞춘다. 비등방 스케일 구체는 지원하지 않는다.
	const float WorldScale = static_cast<float>(GetActorScale3D().GetAbsMax());
	const float WorldRadius = GlobeRadius * WorldScale;
	const float SafeHeightScale = FMath::Max(HeightScale, 0.f);
	const float WorldPointRadius = WorldRadius + LandmarkHeight * SafeHeightScale * WorldScale;
	const float Distance = static_cast<float>(FVector::Dist(ViewerLocation, GetActorLocation()));

	return FMath::Clamp(
		FFF_AtlasGeo::ElevatedHorizonMinDot(WorldRadius, WorldPointRadius, Distance) + Margin,
		-1.f,
		1.f);
}

void AFF_AtlasGlobeActor::UpdatePinPresentation()
{
	const float PinMinDot = ComputePinVisibleMinDot();
	const float SelectedScale = GetAtlasSettings().SelectedLandmarkScale;
	FVector ViewerLocation;
	const bool bHasViewer = GetViewerLocation(ViewerLocation);
	const FVector ViewDirection = bHasViewer ? (ViewerLocation - GetActorLocation()).GetSafeNormal() : GetActorForwardVector();
	const bool bReduceMotion = Accessibility.IsValid() && Accessibility->IsReduceMotionEnabled();

	for (const FFF_AtlasPinInstance& Pin : Pins)
	{
		const bool bSelected = (Pin.CountryId == SelectedCountryId);
		const FVector SurfaceDirection = Pin.Anchor
			? (Pin.Anchor->GetComponentLocation() - GetActorLocation()).GetSafeNormal() : ViewDirection;
		const float Facing = FMath::Clamp(static_cast<float>(FVector::DotProduct(SurfaceDirection, ViewDirection)), 0.f, 1.f);
		// 원근 지평선을 기준으로 보간해 카메라가 가까워져도 가장자리에서 충분히 작아진다.
		const float T = FMath::Clamp((Facing - PinMinDot) / FMath::Max(1.f - PinMinDot, 0.01f), 0.f, 1.f);
		const float EdgeScale = bReduceMotion ? 1.f : FMath::Lerp(FMath::Clamp(GetAtlasSettings().LandmarkEdgeScale, 0.3f, 1.f), 1.f, T * T * (3.f - 2.f * T));
		const float VisualScale = EdgeScale * (bSelected ? SelectedScale : 1.f);
		const float EffectiveHeight = Pin.LandmarkSurfaceOffset + Pin.BaseLandmarkHeight * VisualScale;
		const float LandmarkMinDot = ComputeLandmarkVisibleMinDot(EffectiveHeight / FMath::Max(LandmarkHeight, 1.f));
		const bool bLandmarkVisible =
			FFF_AtlasGeo::IsPinFacingFront(
				Pin.Location, Orientation, LandmarkMinDot);
		const bool bSurfaceVisible =
			FFF_AtlasGeo::IsPinFacingFront(Pin.Location, Orientation, PinMinDot);

		if (Pin.Anchor)
		{
			// 앵커를 숨기면 자식(조형물·조명)까지 함께 숨는다.
			Pin.Anchor->SetVisibility(bLandmarkVisible, /*bPropagateToChildren=*/true);
		}
		if (Pin.Landmark && Pin.Landmark->GetStaticMesh())
		{
			const FAtlasLandmarkFit Fit = ComputeLandmarkFit(*Pin.Landmark->GetStaticMesh(), Pin.BaseLandmarkHeight, VisualScale);
			Pin.Landmark->SetRelativeScale3D(FVector(Fit.Scale));
			Pin.Landmark->SetRelativeLocation(FVector(0, 0, Pin.LandmarkSurfaceOffset + Fit.LiftZ));
		}

		// 조형물은 높이만큼 일찍 보여도 되지만 조명은 지표 기준을 넘기 전까지 켜지 않는다.
		// CastShadows=false인 빛이 뒤편에서 지구를 관통하는 것을 막는다.
		if (Pin.Light)
		{
			// 작은 국가 모델도 원래 큰 원뿔과 같은 광량을 받으면 흰색으로 포화된다.
			const float SizeRatio = Pin.BaseLandmarkHeight / FMath::Max(LandmarkHeight, 1.f);
			Pin.Light->SetIntensity(SelectionLightIntensity * FMath::Square(SizeRatio) * GetAtlasSettings().LandmarkSelectionLightScale);
			Pin.Light->SetRelativeLocation(FVector(0, 0, EffectiveHeight));
			Pin.Light->SetAttenuationRadius(FMath::Max(EffectiveHeight * 4.f, 1.f));
			Pin.Light->SetVisibility(bSurfaceVisible && bSelected);
		}
	}
}

void AFF_AtlasGlobeActor::ApplySelectionVisuals()
{
	// 회전과 선택이 같은 크기·밑면 보정 경로를 사용한다.
	UpdatePinPresentation();
}

float AFF_AtlasGlobeActor::GetEffectiveTransitionSeconds() const
{
	const float Configured = GetAtlasSettings().FocusRotationSeconds;

	// 판정은 접근성 서브시스템이 갖는다. 여기서 `bReduceMotion` 을 다시 해석하지 않는다.
	if (const UFF_UIAccessibilitySubsystem* Access = Accessibility.Get())
	{
		return Access->IsReduceMotionEnabled() ? 0.f : Configured;
	}
	return Configured;
}

bool AFF_AtlasGlobeActor::IsAutoSpinAllowed() const
{
	// 손으로 멈춰 둔 상태가 가장 강하다 (정렬 확인 중에는 무조건 멈춰 있어야 한다).
	if (bAutoSpinPaused)
	{
		return false;
	}

	// 상시로 도는 장식성 움직임이라 `움직임 감소` 에서 가장 먼저 멈춰야 하는 종류다.
	if (const UFF_UIAccessibilitySubsystem* Access = Accessibility.Get())
	{
		return !Access->IsReduceMotionEnabled();
	}
	return true;
}

// =============================================================================
// 콘솔 명령 — 개발용
//
// U3-b 화면이 아직 없어서 **이 명령들이 U3-a 의 유일한 조작 수단**이다.
// 서브시스템·액터에서 `UFUNCTION(Exec)` 은 라우팅되지 않으므로 `FAutoConsoleCommand` 를 쓴다.
// =============================================================================
#if !UE_BUILD_SHIPPING

namespace FFAtlasConsole
{
	AFF_AtlasGlobeActor* Resolve(UWorld* World)
	{
		// 실패 문구는 Find() 가 이미 남긴다. 여기서 또 찍으면 같은 원인이 두 줄로 나온다.
		return AFF_AtlasGlobeActor::Find(World);
	}

	void PrintStatus(const TArray<FString>& Args, UWorld* World)
	{
		const AFF_AtlasGlobeActor* Globe = Resolve(World);
		if (!Globe)
		{
			return;
		}

		UE_LOG(LogFestaFiestaUI, Display, TEXT("FF.Atlas: %s"), *Globe->DescribeState());

		TArray<FFF_AtlasPinState> PinStates;
		Globe->GetPinStates(PinStates);
		for (const FFF_AtlasPinState& Pin : PinStates)
		{
			UE_LOG(LogFestaFiestaUI, Display,
				TEXT("  핀 %s — %s · %s · 위치 %s"),
				*Pin.CountryId.ToString(),
				Pin.bFacingViewer ? TEXT("앞면") : TEXT("뒤편"),
				Pin.bSelectable ? TEXT("선택 가능") : TEXT("잠김"),
				*Pin.WorldLocation.ToCompactString());
		}
	}

	void Focus(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("FF.Atlas.Focus: CountryId 가 필요하다. 목록은 FF.Atlas.Status 로 본다."));
			return;
		}

		if (AFF_AtlasGlobeActor* Globe = Resolve(World))
		{
			const bool bInstant = Args.Num() > 1 && Args[1].Equals(TEXT("instant"), ESearchCase::IgnoreCase);
			Globe->FocusOnCountry(FName(*Args[0]), bInstant);
		}
	}

	void Select(const TArray<FString>& Args, UWorld* World)
	{
		if (AFF_AtlasGlobeActor* Globe = Resolve(World))
		{
			// 인자를 비우면 선택 해제다.
			Globe->SetSelectedCountry(Args.Num() > 0 ? FName(*Args[0]) : NAME_None);
		}
	}

	void Step(const TArray<FString>& Args, UWorld* World)
	{
		if (AFF_AtlasGlobeActor* Globe = Resolve(World))
		{
			const int32 Direction = (Args.Num() > 0) ? FCString::Atoi(*Args[0]) : 1;
			Globe->StepFocus(Direction);
		}
	}

	void Drag(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogFestaFiestaUI, Warning, TEXT("FF.Atlas.Drag: X 와 Y 픽셀이 필요하다."));
			return;
		}

		if (AFF_AtlasGlobeActor* Globe = Resolve(World))
		{
			Globe->AddDragInput(FVector2D(FCString::Atod(*Args[0]), FCString::Atod(*Args[1])));
		}
	}

	void PauseSpin(const TArray<FString>& Args, UWorld* World)
	{
		if (AFF_AtlasGlobeActor* Globe = Resolve(World))
		{
			// 인자를 빼면 토글이다. 정렬을 맞추는 동안 반복해서 누르는 명령이라 그게 편하다.
			const bool bPause = (Args.Num() > 0)
				? (FCString::Atoi(*Args[0]) != 0)
				: !Globe->IsAutoSpinPaused();

			Globe->SetAutoSpinPaused(bPause);
			UE_LOG(LogFestaFiestaUI, Display, TEXT("FF.Atlas.PauseSpin: %s"), *Globe->DescribeState());
		}
	}

	void Rebuild(const TArray<FString>& Args, UWorld* World)
	{
		if (AFF_AtlasGlobeActor* Globe = Resolve(World))
		{
			Globe->RebuildPins();
			UE_LOG(LogFestaFiestaUI, Display, TEXT("FF.Atlas.Rebuild: %s"), *Globe->DescribeState());
		}
	}

	FAutoConsoleCommandWithWorldAndArgs StatusCommand(
		TEXT("FF.Atlas.Status"),
		TEXT("지구본의 현재 상태와 핀 목록을 찍는다. 정면축 규약이 어긋났는지도 알려준다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PrintStatus));

	FAutoConsoleCommandWithWorldAndArgs FocusCommand(
		TEXT("FF.Atlas.Focus"),
		TEXT("그 국가를 정면으로 돌린다. FF.Atlas.Focus <CountryId> [instant] — "
			 "instant 를 붙이면 보간 없이 즉시 이동한다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Focus));

	FAutoConsoleCommandWithWorldAndArgs SelectCommand(
		TEXT("FF.Atlas.Select"),
		TEXT("조형물을 키우고 조명을 켠다. FF.Atlas.Select <CountryId> — 인자를 빼면 선택을 해제한다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Select));

	FAutoConsoleCommandWithWorldAndArgs StepCommand(
		TEXT("FF.Atlas.Step"),
		TEXT("화면 기준 좌우로 다음 핀. FF.Atlas.Step <1|-1> — +1 이 오른쪽(동쪽)이다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Step));

	FAutoConsoleCommandWithWorldAndArgs DragCommand(
		TEXT("FF.Atlas.Drag"),
		TEXT("드래그 픽셀을 흉내낸다. FF.Atlas.Drag <X> <Y> — U3-b 화면 없이 회전을 확인하는 경로다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Drag));

	FAutoConsoleCommandWithWorldAndArgs PauseSpinCommand(
		TEXT("FF.Atlas.PauseSpin"),
		TEXT("자동 자전을 멈춘다/재개한다. FF.Atlas.PauseSpin [0|1] — 인자를 빼면 토글. "
			 "텍스처 정렬을 맞출 때 쓴다 (설정값은 건드리지 않는다)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PauseSpin));

	FAutoConsoleCommandWithWorldAndArgs RebuildCommand(
		TEXT("FF.Atlas.Rebuild"),
		TEXT("카탈로그를 다시 읽어 핀·조형물을 새로 만든다. DA_FestivalAtlas 를 고친 뒤에 쓴다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Rebuild));
}

#endif // !UE_BUILD_SHIPPING
