// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/FF_OverviewCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "Components/SplineComponent.h"
#include "EngineUtils.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "GameFramework/SpringArmComponent.h"

AFF_OverviewCameraPawn::AFF_OverviewCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	// 카메라가 시설이나 지형에 걸리면 배치 작업 자체가 불가능해진다.
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(SceneRoot);
	CameraBoom->TargetArmLength = 8000.f;
	CameraBoom->bDoCollisionTest = false;          // 지형에 카메라가 당겨지면 구도가 튄다
	CameraBoom->bUsePawnControlRotation = false;   // 회전은 리그가 소유한다
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritYaw = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bEnableCameraLag = false;          // 보간은 리그가 하므로 이중으로 걸지 않는다
	CameraBoom->bEnableCameraRotationLag = false;

	OverviewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("OverviewCamera"));
	OverviewCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	OverviewCamera->bUsePawnControlRotation = false;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
}

void AFF_OverviewCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	RegisterDebugConsoleCommands();

	// 레벨 크기를 아는 쪽이 InitializeOverview 를 불러주는 게 정상 경로다.
	// 안 불러도 현재 위치 기준으로 동작은 하게 둔다.
	if (!bInitialized)
	{
		InitializeOverview(GetActorLocation(), CameraBoom ? CameraBoom->TargetArmLength : 8000.f);
	}
}

void AFF_OverviewCameraPawn::InitializeOverview(const FVector& FocusLocation, float SuggestedArmLength)
{
	Rig.Initialize(CameraSettings, FocusLocation, SuggestedArmLength);
	StopCameraMotion();
	bHasValidFocus = false;
	LastZoneId = NAME_None;
	bNeedsInitialPlacement = true;
	bInitialized = true;
	ApplyRigToComponents();
}

void AFF_OverviewCameraPawn::AddPanInput(const FVector2D& ScreenInput)
{
	// 추적 중에 플레이어가 화면을 밀면 추적을 놓아준다.
	// 안 그러면 밀어도 카메라가 계속 끌려가서 입력이 씹히는 것처럼 느껴진다.
	if (!ScreenInput.IsNearlyZero() && FollowTarget.IsValid())
	{
		SetFollowTarget(nullptr);
	}
	PendingPanInput += ScreenInput;
}

void AFF_OverviewCameraPawn::AddEdgeScrollInput(const FVector2D& ScreenInput)
{
	if (!ScreenInput.IsNearlyZero() && FollowTarget.IsValid())
	{
		SetFollowTarget(nullptr);
	}
	PendingEdgeScrollInput += ScreenInput;
}

void AFF_OverviewCameraPawn::AddZoomInput(float Steps)
{
	if (FMath::IsNearlyZero(Steps))
	{
		return;
	}
	SetFollowTarget(nullptr);
	if (Rig.Zoom(Steps))
	{
		RequestTransition(EFF_CameraMotionChannel::Focus | EFF_CameraMotionChannel::Zoom);
	}
}

void AFF_OverviewCameraPawn::AddZoomInputAtAnchor(float Steps, const FVector& WorldAnchor)
{
	if (!CameraSettings.bZoomToCursor)
	{
		AddZoomInput(Steps);
		return;
	}
	if (FMath::IsNearlyZero(Steps))
	{
		return;
	}

	// 추적 중에 커서 쪽으로 줌하면 초점이 대상에서 떨어져 나간다.
	// 그 상태로 두면 다음 프레임에 추적이 도로 끌어당겨서 화면이 떨린다. 추적을 놓아준다.
	if (FollowTarget.IsValid())
	{
		SetFollowTarget(nullptr);
	}

	if (Rig.ZoomTowards(Steps, WorldAnchor))
	{
		RequestTransition(EFF_CameraMotionChannel::Focus | EFF_CameraMotionChannel::Zoom);
	}
}

float AFF_OverviewCameraPawn::GetFocusPlaneZ() const
{
	return Rig.GetDisplayState().FocusLocation.Z;
}

void AFF_OverviewCameraPawn::AddRotateInput(float Direction)
{
	if (!FMath::IsNearlyZero(Direction))
	{
		SetFollowTarget(nullptr);
	}
	PendingRotateInput += Direction;
}

void AFF_OverviewCameraPawn::SnapRotate(int32 Direction)
{
	if (Direction == 0)
	{
		return;
	}
	SetFollowTarget(nullptr);
	Rig.SnapRotate(Direction);
	RequestTransition(EFF_CameraMotionChannel::Rotation);
}

void AFF_OverviewCameraPawn::FocusOnLocation(const FVector& WorldLocation)
{
	SetFollowTarget(nullptr);
	Rig.FocusOn(WorldLocation);
	RequestTransition(EFF_CameraMotionChannel::Focus);
}

void AFF_OverviewCameraPawn::SetFollowTarget(AActor* NewTarget)
{
	if (FollowTarget.Get() == NewTarget)
	{
		return;
	}
	Rig.HoldChannels(EFF_CameraMotionChannel::Focus);
	TransitionChannels &= ~EFF_CameraMotionChannel::Focus;
	GroundHeightSample.Reset();
	bRefreshGroundRequested = IsValid(NewTarget);
	FollowTarget = NewTarget;
}

void AFF_OverviewCameraPawn::ResetView()
{
	SetFollowTarget(nullptr);
	Rig.ResetToInitial();
	RequestTransition(EFF_CameraMotionChannel::All);
}

void AFF_OverviewCameraPawn::StopCameraMotion()
{
	PendingPanInput = FVector2D::ZeroVector;
	PendingEdgeScrollInput = FVector2D::ZeroVector;
	PendingRotateInput = 0.f;
	FollowTarget.Reset();
	TransitionChannels = EFF_CameraMotionChannel::None;
	GroundHeightSample.Reset();
	bRefreshGroundRequested = false;
	Rig.HoldChannels(EFF_CameraMotionChannel::All);
	MotionState = EFF_OverviewCameraMotionState::Idle;
}

void AFF_OverviewCameraPawn::RequestTransition(const EFF_CameraMotionChannel Channels)
{
	TransitionChannels |= Channels;
	bRefreshGroundRequested |= EnumHasAnyFlags(Channels, EFF_CameraMotionChannel::Focus);
}

void AFF_OverviewCameraPawn::CompleteSettledTransitions()
{
	for (const EFF_CameraMotionChannel Channel : { EFF_CameraMotionChannel::Focus,
		EFF_CameraMotionChannel::Rotation, EFF_CameraMotionChannel::Zoom })
	{
		if (!EnumHasAnyFlags(TransitionChannels, Channel) || !Rig.HasReachedTarget(Channel))
		{
			continue;
		}
		if (Channel == EFF_CameraMotionChannel::Focus && GroundHeightSample.IsSet()
			&& !Rig.HasReachedGroundHeight(GroundHeightSample.GetValue()))
		{
			continue;
		}
		Rig.SnapChannelsToTarget(Channel);
		TransitionChannels &= ~Channel;
	}
}

bool AFF_OverviewCameraPawn::HasActiveCamera() const
{
	return OverviewCamera != nullptr && OverviewCamera->IsActive();
}

void AFF_OverviewCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bInitialized || DeltaSeconds <= 0.f)
	{
		return;
	}

	// 최초 배치는 입력을 기다리지 않는다. 로드 직후 한 번만 지면에 맞춘다.
	if (bNeedsInitialPlacement)
	{
		ApplyZoneBoundsIfNeeded(DeltaSeconds);
		ConstrainFocusToZones();
		UpdateGroundHeight(0.f, true);
		Rig.SnapToTarget();
		ApplyRigToComponents();
		bNeedsInitialPlacement = false;
	}

	const bool bPan = !PendingPanInput.IsNearlyZero() || !PendingEdgeScrollInput.IsNearlyZero();
	const bool bRotate = !FMath::IsNearlyZero(PendingRotateInput);
	const AActor* FollowActor = FollowTarget.Get();
	EFF_CameraMotionChannel DirectChannels = EFF_CameraMotionChannel::None;
	if (bPan || FollowActor)
	{
		DirectChannels |= EFF_CameraMotionChannel::Focus;
	}
	if (bRotate)
	{
		DirectChannels |= EFF_CameraMotionChannel::Rotation;
	}

	// 직접 입력은 같은 항목의 일회성 명령을 대체한다. 줌 중 이동 키를 놓아도
	// 이동 보간이 줌 전환에 딸려 남지 않으며, 거리/각도 전환은 계속 완료된다.
	Rig.HoldChannels(TransitionChannels & DirectChannels);
	TransitionChannels &= ~DirectChannels;
	const EFF_CameraMotionChannel ActiveChannels = DirectChannels | TransitionChannels;
	Rig.HoldChannels(EFF_CameraMotionChannel::All & ~ActiveChannels);
	if (ActiveChannels == EFF_CameraMotionChannel::None)
	{
		PendingPanInput = FVector2D::ZeroVector;
		PendingEdgeScrollInput = FVector2D::ZeroVector;
		PendingRotateInput = 0.f;
		MotionState = EFF_OverviewCameraMotionState::Idle;
		GroundHeightSample.Reset();
		bRefreshGroundRequested = false;
		return;
	}

	Rig.Rotate(PendingRotateInput, DeltaSeconds);
	Rig.Pan(PendingPanInput, DeltaSeconds);
	Rig.EdgeScroll(PendingEdgeScrollInput, DeltaSeconds);
	PendingRotateInput = 0.f;
	PendingPanInput = FVector2D::ZeroVector;
	PendingEdgeScrollInput = FVector2D::ZeroVector;
	if (FollowActor)
	{
		Rig.FocusOn(FollowActor->GetActorLocation());
	}

	ApplyZoneBoundsIfNeeded(DeltaSeconds);
	if (EnumHasAnyFlags(ActiveChannels, EFF_CameraMotionChannel::Focus))
	{
		ConstrainFocusToZones();
		UpdateGroundHeight(DeltaSeconds, bPan || FollowActor || bRefreshGroundRequested);
	}
	bRefreshGroundRequested = false;
	Rig.AdvanceInterpolation(DeltaSeconds);
	CompleteSettledTransitions();
	ApplyRigToComponents();
	MotionState = FollowActor ? EFF_OverviewCameraMotionState::Following
		: (bPan || bRotate) ? EFF_OverviewCameraMotionState::DirectInput
		: TransitionChannels != EFF_CameraMotionChannel::None ? EFF_OverviewCameraMotionState::Transition
		: EFF_OverviewCameraMotionState::Idle;
}

void AFF_OverviewCameraPawn::LogBoundsState() const
{
	const FBox& Bounds = CameraSettings.FocusBounds;

	UE_LOG(LogFestaFiestaCamera, Display,
		TEXT("[카메라] Zone 경계 사용=%s / 적용됨=%s / 범위 유효=%s"),
		CameraSettings.bUseZoneBounds ? TEXT("예") : TEXT("아니오"),
		bZoneBoundsApplied ? TEXT("예") : TEXT("아니오"),
		Bounds.IsValid ? TEXT("예") : TEXT("아니오"));

	if (Bounds.IsValid)
	{
		UE_LOG(LogFestaFiestaCamera, Display,
			TEXT("  X %.0f ~ %.0f / Y %.0f ~ %.0f"),
			Bounds.Min.X, Bounds.Max.X, Bounds.Min.Y, Bounds.Max.Y);
	}

	// Zone 이 몇 개고 각각 무엇을 들고 있는지 본다.
	// 0 개면 레벨 문제고, 있는데 점이 0 개면 스플라인 문제다.
	int32 ZoneCount = 0;
	int32 PolygonTotal = 0;
	int32 SplineTotal = 0;

	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<AFFZSGameplayZone> It(const_cast<UWorld*>(World)); It; ++It)
		{
			if (!IsValid(*It))
			{
				continue;
			}
			++ZoneCount;
			PolygonTotal += It->GetBoundaryPolygonLocalCm().Num();
			if (It->BoundarySpline)
			{
				SplineTotal += It->BoundarySpline->GetNumberOfSplinePoints();
			}
		}
	}

	UE_LOG(LogFestaFiestaCamera, Display,
		TEXT("  Zone %d개 / 폴리곤 점 %d개 / 스플라인 점 %d개"),
		ZoneCount, PolygonTotal, SplineTotal);

	const FVector Focus = Rig.GetTargetState().FocusLocation;
	UE_LOG(LogFestaFiestaCamera, Display,
		TEXT("  현재 초점 X %.0f / Y %.0f / 소속 Zone %s"),
		Focus.X, Focus.Y,
		LastZoneId.IsNone() ? TEXT("없음(Zone 밖)") : *LastZoneId.ToString());

	UE_LOG(LogFestaFiestaCamera, Display,
		TEXT("  Zone 안쪽 제한=%s / 차단 %d회 / 미끄러짐 %d회 / 기준점 있음=%s"),
		CameraSettings.bConstrainFocusToZones ? TEXT("켜짐") : TEXT("꺼짐"),
		ConstrainBlockCount, ConstrainSlideCount,
		bHasValidFocus ? TEXT("예") : TEXT("아니오"));

	if (bHasValidFocus)
	{
		UE_LOG(LogFestaFiestaCamera, Display,
			TEXT("  마지막 유효 초점 X %.0f / Y %.0f"), LastValidFocus.X, LastValidFocus.Y);
	}
}

void AFF_OverviewCameraPawn::ApplyZoneBoundsIfNeeded(const float DeltaSeconds)
{
	if (bZoneBoundsApplied || !CameraSettings.bUseZoneBounds)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FBox Combined(ForceInit);
	int32 ZoneCount = 0;
	int32 FromPolygon = 0;
	int32 FromBounds = 0;

	for (TActorIterator<AFFZSGameplayZone> It(const_cast<UWorld*>(World)); It; ++It)
	{
		const AFFZSGameplayZone* Zone = *It;
		if (!IsValid(Zone))
		{
			continue;
		}
		++ZoneCount;

		// 경계 폴리곤이 정답이다. Zone 이 배치 검증에 쓰는 것과 같은 값이라
		// 카메라 범위와 실제 축제장이 어긋나지 않는다.
		const TArray<FVector2D> Polygon = Zone->GetBoundaryPolygonLocalCm();
		if (Polygon.Num() > 0)
		{
			const FTransform& ZoneTransform = Zone->GetActorTransform();
			for (const FVector2D& LocalPoint : Polygon)
			{
				Combined += ZoneTransform.TransformPosition(FVector(LocalPoint.X, LocalPoint.Y, 0.f));
			}
			++FromPolygon;
			continue;
		}

		// 스플라인이 비어 있는 Zone 도 있다. 그때는 액터가 차지하는 범위로 대신한다.
		// 정확하지는 않지만 아예 경계가 없는 것보다는 낫다.
		const FBox ActorBox = Zone->GetComponentsBoundingBox(/*bNonColliding=*/true);
		if (ActorBox.IsValid)
		{
			Combined += ActorBox;
			++FromBounds;
		}
	}

	if (!Combined.IsValid)
	{
		// 아직 Zone 이 안 떴거나 레벨에 없다. 다음 틱에 다시 본다.
		//
		// 계속 못 잡으면 조용히 실패한 채로 카메라가 축제장 밖으로 나간다.
		// 한 번만 경고해서 원인을 찾을 수 있게 한다.
		ZoneBoundsWaitSeconds += DeltaSeconds;
		if (!bZoneBoundsWarned && ZoneBoundsWaitSeconds > 5.f)
		{
			bZoneBoundsWarned = true;
			UE_LOG(LogFestaFiestaCamera, Warning,
				TEXT("[카메라] Zone 경계를 5초 동안 잡지 못했다. 이동 범위 제한이 걸리지 않는다."));
			LogBoundsState();
		}
		return;
	}

	// 가장자리 시설이 화면 끝에 걸리지 않게 여유를 준다.
	const float Padding = FMath::Max(0.f, CameraSettings.ZoneBoundsPadding);
	Combined = Combined.ExpandBy(FVector(Padding, Padding, 0.f));

	// 높이는 제한하지 않는다. 지형 추종이 초점 Z 를 정하는데 여기서 또 막으면 서로 싸운다.
	Combined.Min.Z = -HALF_WORLD_MAX;
	Combined.Max.Z = HALF_WORLD_MAX;

	CameraSettings.FocusBounds = Combined;
	Rig.UpdateSettings(CameraSettings);
	bZoneBoundsApplied = true;

	UE_LOG(LogFestaFiestaCamera, Log,
		TEXT("[카메라] Zone %d개(폴리곤 %d, 바운드 %d)로 이동 범위를 잡았다. X %.0f~%.0f / Y %.0f~%.0f"),
		ZoneCount, FromPolygon, FromBounds,
		Combined.Min.X, Combined.Max.X, Combined.Min.Y, Combined.Max.Y);
}

bool AFF_OverviewCameraPawn::TraceGroundAt(const FVector2D& Where, float AroundZ, float& OutGroundZ) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float HalfHeight = FMath::Max(100.f, CameraSettings.GroundTraceHalfHeight);

	const FVector Start(Where.X, Where.Y, AroundZ + HalfHeight);
	const FVector End(Where.X, Where.Y, AroundZ - HalfHeight);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FF_CameraGround), false);
	QueryParams.AddIgnoredActor(this);

	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Start, End, CameraSettings.GroundTraceChannel, QueryParams))
	{
		return false;
	}

	OutGroundZ = static_cast<float>(Hit.ImpactPoint.Z);
	return true;
}

void AFF_OverviewCameraPawn::UpdateGroundHeight(const float DeltaSeconds, const bool bRefreshSample)
{
	if (!CameraSettings.bFollowGroundHeight)
	{
		GroundHeightSample.Reset();
		return;
	}
	if (bRefreshSample)
	{
		float GroundZ = 0.f;
		if (SampleGroundHeight(GroundZ))
		{
			GroundHeightSample = GroundZ;
		}
		else
		{
			GroundHeightSample.Reset();
		}
	}
	if (GroundHeightSample.IsSet())
	{
		Rig.ApplyGroundHeight(GroundHeightSample.GetValue(), DeltaSeconds);
	}
	else
	{
		Rig.OnGroundHeightUnavailable();
	}
}

bool AFF_OverviewCameraPawn::SampleGroundHeight(float& OutGroundZ) const
{

	// 초점의 수평 위치를 기준으로 위아래를 훑는다. 목표 위치를 쓰는 이유는
	// 표시 위치를 쓰면 보간 중인 값을 따라가면서 서로를 쫓는 진동이 생기기 때문이다.
	const FVector Focus = Rig.GetTargetState().FocusLocation;
	const FVector2D Center(Focus.X, Focus.Y);

	// 중심은 반드시 있어야 한다. 여기서 못 찾으면 주변을 재봐야 기준이 없다.
	float CenterZ = 0.f;
	if (!TraceGroundAt(Center, static_cast<float>(Focus.Z), CenterZ))
	{
		// 절벽, 물 위, 콜리전이 없는 구역. 마지막 높이를 유지한다.
		return false;
	}

	double Sum = CenterZ;
	int32 Count = 1;

	// 주변을 같이 재서 평균을 낸다.
	//
	// 한 점만 보면 벽 끝이나 물가를 지날 때 높이가 한 프레임에 뚝 끊긴다.
	// 그 순간 카메라가 방지턱을 넘는 것처럼 출렁인다.
	// 주변을 섞으면 경계를 지나는 동안 값이 서서히 이어진다.
	const int32 SampleCount = FMath::Clamp(CameraSettings.GroundSampleCount, 0, 8);
	if (SampleCount > 0)
	{
		const float Radius = FMath::Max(10.f, CameraSettings.GroundSampleRadius);
		const float MaxDeviation = CameraSettings.GroundSampleMaxDeviation;

		for (int32 Index = 0; Index < SampleCount; ++Index)
		{
			const float Angle = 2.f * PI * static_cast<float>(Index) / static_cast<float>(SampleCount);
			const FVector2D Offset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius);

			float SampleZ = 0.f;
			if (!TraceGroundAt(Center + Offset, static_cast<float>(Focus.Z), SampleZ))
			{
				// 이 방향은 허공이다. 평균에서 빼면 나머지가 그대로 반영된다.
				continue;
			}

			// 구멍이나 절벽 아래를 찍은 표본이 섞이면 평균이 끌려 내려간다.
			// 중심에서 너무 먼 값은 지형이 아니라 다른 층으로 본다.
			if (MaxDeviation > 0.f && FMath::Abs(SampleZ - CenterZ) > MaxDeviation)
			{
				continue;
			}

			Sum += SampleZ;
			++Count;
		}
	}

	OutGroundZ = static_cast<float>(Sum / Count);
	return true;
}

void AFF_OverviewCameraPawn::ApplyRigToComponents()
{
	const FFF_OverviewCameraState& State = Rig.GetDisplayState();

	SetActorLocation(State.FocusLocation);

	if (CameraBoom)
	{
		CameraBoom->TargetArmLength = State.ArmLength;
		// 스프링암은 자기 회전의 반대편에 카메라를 매단다.
		// Pitch 가 음수면 카메라가 위에서 아래를 내려다본다.
		CameraBoom->SetRelativeRotation(FRotator(State.Pitch, State.Yaw, 0.f));
	}
}

void AFF_OverviewCameraPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterDebugConsoleCommands();
	Super::EndPlay(EndPlayReason);
}

void AFF_OverviewCameraPawn::RegisterDebugConsoleCommands()
{
	UnregisterDebugConsoleCommands();

	DebugCommands.Add(
		TEXT("FF.Camera.Bounds"),
		TEXT("카메라 이동 범위 상태를 출력한다."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			LogBoundsState();
		}));

	DebugCommands.Add(
		TEXT("FF.Camera.RefreshBounds"),
		TEXT("Zone 을 다시 훑어 카메라 이동 범위를 재계산한다."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			// Zone 이 나중에 추가됐거나 처음에 못 잡은 경우를 위해 다시 시도한다.
			bZoneBoundsApplied = false;
			bZoneBoundsWarned = false;
			ZoneBoundsWaitSeconds = 0.f;
			// 경계 적용도 명시적인 명령으로 보낸다. Idle 중 Rig의 Display만 바꾸면
			// 실제 컴포넌트와 상태가 어긋나므로 다음 활성 Tick에서 함께 반영한다.
			RequestTransition(EFF_CameraMotionChannel::All);
			UE_LOG(LogFestaFiestaCamera, Log, TEXT("[카메라] 다음 카메라 갱신에서 Zone 경계를 다시 계산한다."));
		}));
}

void AFF_OverviewCameraPawn::UnregisterDebugConsoleCommands()
{
	// 내가 건 것만 지운다. 규칙은 FFF_DebugConsoleCommands 주석 참고.
	DebugCommands.UnregisterAll();
}

namespace
{
	// Zone 의 수평 범위 안인지만 본다.
	//
	// ContainsWorldPoint 는 높이도 같이 본다. Zone 원점에서 VerticalExtentCm 의 절반
	// (기본 10m) 넘게 벗어나면 수평으로는 안에 있어도 밖으로 판정한다.
	//
	// 카메라 초점 Z 는 지형을 따라다니므로 언덕이나 높이가 다른 구역에서 그 범위를
	// 쉽게 넘는다. 그러면 판정이 통째로 실패해서 이동 제한이 걸리지 않는다.
	// 그래서 Zone 자신의 높이로 물어본다.
	bool IsInsideZoneIgnoringHeight(const AFFZSGameplayZone& Zone, const FVector& WorldLocation)
	{
		const FVector TestPoint(WorldLocation.X, WorldLocation.Y, Zone.GetActorLocation().Z);
		return Zone.ContainsWorldPoint(TestPoint);
	}
}

FName AFF_OverviewCameraPawn::ResolveZoneAt(const FVector& WorldLocation) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return NAME_None;
	}

	// 직전 Zone 을 먼저 본다. 대개 그 안에 그대로 있으므로 검사 한 번으로 끝난다.
	if (const UFFZSWorldSubsystem* ZoneSystem = World->GetSubsystem<UFFZSWorldSubsystem>())
	{
		if (const AFFZSGameplayZone* Cached = ZoneSystem->FindZone(LastZoneId))
		{
			if (IsInsideZoneIgnoringHeight(*Cached, WorldLocation))
			{
				return LastZoneId;
			}
		}
	}

	// Zone 을 옮겼거나 처음이다. 전체를 훑는다.
	for (TActorIterator<AFFZSGameplayZone> It(const_cast<UWorld*>(World)); It; ++It)
	{
		const AFFZSGameplayZone* Zone = *It;
		if (!IsValid(Zone) || !IsInsideZoneIgnoringHeight(*Zone, WorldLocation))
		{
			continue;
		}

		// ZoneId 가 비어 있으면 캐시가 무의미해지므로 이름으로 대신한다.
		const FName ZoneId = Zone->ZoneId.IsNone() ? FName(*Zone->GetName()) : Zone->ZoneId;
		return ZoneId;
	}

	return NAME_None;
}

void AFF_OverviewCameraPawn::ConstrainFocusToZones()
{
	if (!CameraSettings.bConstrainFocusToZones)
	{
		return;
	}

	// 액터를 따라가는 중이면 건드리지 않는다. 그 대상이 어디 있든 보여주는 게 맞고,
	// 여기서 끌어당기면 추적과 매 프레임 싸운다.
	if (FollowTarget.IsValid())
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World || !World->GetSubsystem<UFFZSWorldSubsystem>())
	{
		// Zone 시스템이 없는 테스트 맵이다. 사각형 경계만으로 돈다.
		return;
	}

	const FVector Desired = Rig.GetTargetState().FocusLocation;

	if (const FName ZoneId = ResolveZoneAt(Desired); ZoneId != NAME_None)
	{
		LastZoneId = ZoneId;
		LastValidFocus = Desired;
		bHasValidFocus = true;
		return;
	}

	// 아직 한 번도 Zone 안에 있어본 적이 없다. 되돌릴 곳이 없으므로 그대로 둔다.
	// 시작 위치가 Zone 밖인 레벨에서 카메라가 얼어붙는 걸 막는다.
	if (!bHasValidFocus)
	{
		return;
	}

	// 축을 나눠 시도한다. 벽에 비스듬히 밀 때 완전히 멈추는 대신 벽을 따라 미끄러진다.
	// 한 축만 막힌 경우가 대부분이라 이것만으로 조작감이 크게 달라진다.
	const FVector SlideX(Desired.X, LastValidFocus.Y, Desired.Z);
	if (const FName ZoneId = ResolveZoneAt(SlideX); ZoneId != NAME_None)
	{
		LastZoneId = ZoneId;
		LastValidFocus = SlideX;
		Rig.FocusOn(SlideX);
		++ConstrainSlideCount;
		return;
	}

	const FVector SlideY(LastValidFocus.X, Desired.Y, Desired.Z);
	if (const FName ZoneId = ResolveZoneAt(SlideY); ZoneId != NAME_None)
	{
		LastZoneId = ZoneId;
		LastValidFocus = SlideY;
		Rig.FocusOn(SlideY);
		++ConstrainSlideCount;
		return;
	}

	// 양쪽 다 막혔다. 마지막으로 안에 있던 자리로 되돌린다.
	// 높이는 지형 추종이 정하므로 그대로 둔다.
	Rig.FocusOn(FVector(LastValidFocus.X, LastValidFocus.Y, Desired.Z));
	++ConstrainBlockCount;
}
