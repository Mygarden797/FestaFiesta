// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Atlas/FF_AtlasCameraRig.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UI/Atlas/FF_AtlasSettings.h"
#include "UI/Atlas/FF_AtlasGlobeActor.h"
#include "UI/Core/FF_UIAccessibilitySubsystem.h"
#include "UI/FF_UILog.h"

AFF_AtlasCameraRig::AFF_AtlasCameraRig()
{
	// 화면 사이 뷰타깃 보간은 엔진이, 같은 카메라의 국가별 접근은 Tick이 처리한다.
	PrimaryActorTick.bCanEverTick = true;

	// 연출용 액터다. 월드 클릭을 먹으면 안 된다.
	SetActorEnableCollision(false);
}

AFF_AtlasCameraRig* AFF_AtlasCameraRig::Find(const UObject* WorldContextObject, bool bLogIfMissing)
{
	UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	if (!World)
	{
		if (bLogIfMissing)
		{
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("AFF_AtlasCameraRig::Find: 월드를 찾을 수 없다 (컨텍스트 %s)."),
				WorldContextObject ? *WorldContextObject->GetName() : TEXT("(없음)"));
		}
		return nullptr;
	}

	for (TActorIterator<AFF_AtlasCameraRig> It(World); It; ++It)
	{
		return *It;
	}

	if (bLogIfMissing)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("AFF_AtlasCameraRig::Find: 이 레벨에 카메라 리그가 없다. "
				 "BP_FF_AtlasCameraRig 을 프론트엔드 레벨에 배치하고 카메라를 지정한다."));
	}
	return nullptr;
}

void AFF_AtlasCameraRig::BeginPlay()
{
	Super::BeginPlay();

	if (!TitleCamera)
	{
		// 이것만은 없으면 리그가 할 일이 없다. 조용히 넘기지 않는다.
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("AFF_AtlasCameraRig: Title Camera 가 지정되지 않았다. 카메라 연출이 동작하지 않는다 — "
				 "레벨의 ACameraActor 를 슬롯에 넣는다."));
		return;
	}

	// 시작 구도를 즉시 잡는다. 레벨이 막 열렸으므로 섞을 이전 구도가 없다.
	// 카메라의 `Auto Activate for Player` 에 의존하지 않는다 — 그건 리그가 없을 때의 폴백이다.
	CurrentScreen = EFF_UIFlowScreen::Title;
	ApplyViewTarget(TitleCamera, 0.f);

	// 인트로 구도 카메라는 미리 만들어 둔다. 렌즈를 타이틀 카메라에서 물려받아야 블렌드가 매끄럽다.
	EnsureDerivedIntroCamera();
}

void AFF_AtlasCameraRig::ViewForScreen(EFF_UIFlowScreen Screen, bool bInstant)
{
	// 당기는 도중에 떠나면 끝 위치에 붙인 뒤 블렌드한다. 출발점이 움직이면 블렌드가 출렁인다.
	if (Screen != EFF_UIFlowScreen::TitleIntro)
	{
		FinishIntroReveal();
	}
	else
	{
		EnsureDerivedIntroCamera();
		FrameDerivedIntroCamera();
	}
	if (Screen != EFF_UIFlowScreen::AtlasGlobe)
	{
		ResetCountryInspection();
	}
	ACameraActor* Camera = ResolveCameraForScreen(Screen);
	if (!Camera)
	{
		// 슬롯이 전부 비어 있는 상태다. BeginPlay 가 이미 이유를 남겼으므로 여기서는 조용히 나간다.
		return;
	}

	const float BlendSeconds = bInstant ? 0.f : GetBlendSeconds(CurrentScreen, Screen);

	// 같은 카메라로 다시 가는 것은 이동이 아니다 (타이틀 → 플레이 허브가 그렇다).
	// 그때 블렌드를 다시 걸면 카메라가 제자리에서 미묘하게 흔들린다.
	if (PendingCamera.Get() == Camera && !IsMoving())
	{
		CurrentScreen = Screen;
		return;
	}

	CurrentScreen = Screen;
	ApplyViewTarget(Camera, BlendSeconds);
}

void AFF_AtlasCameraRig::SkipCameraMove(bool bIncludeInspection)
{
	if (bIncludeInspection && bInspectionTransitioning)
	{
		InspectionAlpha = 1.f;
		Tick(0.f);
	}
	if (!IsMoving())
	{
		return;
	}

	if (ACameraActor* Camera = PendingCamera.Get())
	{
		// 목표에 0초로 다시 세팅하면 그 자리에 붙는다.
		ApplyViewTarget(Camera, 0.f);
	}
}

bool AFF_AtlasCameraRig::IsMoving() const
{
	const APlayerController* Controller = GetOwningController();
	if (!Controller || !Controller->PlayerCameraManager)
	{
		return false;
	}

	// 블렌드 중에만 `PendingViewTarget` 이 채워져 있다. 끝나면 엔진이 비운다.
	return Controller->PlayerCameraManager->PendingViewTarget.Target != nullptr;
}

FString AFF_AtlasCameraRig::DescribeState() const
{
	const AActor* ViewTarget = nullptr;
	if (const APlayerController* Controller = GetOwningController())
	{
		if (Controller->PlayerCameraManager)
		{
			ViewTarget = Controller->PlayerCameraManager->ViewTarget.Target;
		}
	}

	return FString::Printf(
		TEXT("Screen=%s ViewTarget=%s Moving=%s | Title=%s Globe=%s CountryMap=%s Intro=%s%s"),
		*UEnum::GetValueAsString(CurrentScreen),
		ViewTarget ? *ViewTarget->GetName() : TEXT("(없음)"),
		IsMoving() ? TEXT("이동 중") : TEXT("정지"),
		TitleCamera ? *TitleCamera->GetName() : TEXT("(미지정)"),
		GlobeCamera ? *GlobeCamera->GetName() : TEXT("(미지정)"),
		CountryMapCamera ? *CountryMapCamera->GetName() : TEXT("(미지정)"),
		IntroCamera ? *IntroCamera->GetName() : (DerivedIntroCamera ? TEXT("(계산 구도)") : TEXT("(미지정)")),
		bIntroRevealing ? TEXT(" 인트로 당기는 중") : TEXT(""));
}

// --- 내부 ---

ACameraActor* AFF_AtlasCameraRig::GetGlobeCamera() const
{
	return ResolveCameraForScreen(EFF_UIFlowScreen::AtlasGlobe);
}

ACameraActor* AFF_AtlasCameraRig::ResolveCameraForScreen(EFF_UIFlowScreen Screen) const
{
	// 깊은 구도가 비어 있으면 한 단계 얕은 쪽으로 내려간다. 그래야 카메라 하나만 놓아도 동작하고,
	// U3-b 를 만들기 전에는 국가 지도 카메라를 비워 둘 수 있다.
	switch (Screen)
	{
	case EFF_UIFlowScreen::TitleIntro:
		if (IntroCamera) { return IntroCamera; }
		if (DerivedIntroCamera) { return DerivedIntroCamera; }
		return TitleCamera;
	case EFF_UIFlowScreen::AtlasCountry:
		if (CountryMapCamera) { return CountryMapCamera; }
		// fall through
	case EFF_UIFlowScreen::AtlasGlobe:
		if (GlobeCamera) { return GlobeCamera; }
		// fall through
	default:
		return TitleCamera;
	}
}

int32 AFF_AtlasCameraRig::GetScreenDepth(EFF_UIFlowScreen Screen)
{
	switch (Screen)
	{
	case EFF_UIFlowScreen::AtlasGlobe:		return 1;
	case EFF_UIFlowScreen::AtlasCountry:	return 2;
	default:								return 0;
	}
}

float AFF_AtlasCameraRig::GetBlendSeconds(EFF_UIFlowScreen FromScreen, EFF_UIFlowScreen ToScreen) const
{
	// 판정은 접근성 서브시스템이 갖는다. 여기서 `bReduceMotion` 을 다시 해석하지 않는다.
	if (const UFF_UIAccessibilitySubsystem* Access = UFF_UIAccessibilitySubsystem::Get(this))
	{
		if (Access->IsReduceMotionEnabled())
		{
			return 0.f;
		}
	}

	const UFF_AtlasSettings& Settings = GetAtlasSettings();

	// 인트로 「아무 키」 구도 ↔ 메뉴 구도. 메뉴끼리는 0초지만 이것은 지구가 비켜 가는 연출이다.
	if ((FromScreen == EFF_UIFlowScreen::TitleIntro) != (ToScreen == EFF_UIFlowScreen::TitleIntro))
	{
		return Settings.IntroToTitleSeconds;
	}

	// **더 깊은 쪽**이 시간을 정한다. 그래서 들어갈 때와 나올 때가 같은 길이가 된다 —
	// 한쪽만 빠르면 뒤로 가기가 잘려 나가는 것처럼 보인다.
	const int32 Depth = FMath::Max(GetScreenDepth(FromScreen), GetScreenDepth(ToScreen));

	switch (Depth)
	{
	case 2:		return Settings.CountryMapTransitionSeconds;
	case 1:		return Settings.GlobeApproachSeconds;
	default:	return 0.f;	// 메뉴끼리는 카메라가 움직이지 않는다.
	}
}

void AFF_AtlasCameraRig::ApplyViewTarget(ACameraActor* Camera, float BlendSeconds)
{
	if (!Camera)
	{
		return;
	}

	APlayerController* Controller = GetOwningController();
	if (!Controller)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("AFF_AtlasCameraRig: 플레이어 컨트롤러가 없어 카메라를 옮기지 못했다."));
		return;
	}

	PendingCamera = Camera;
	Controller->SetViewTargetWithBlend(Camera, BlendSeconds, BlendFunction, BlendExponent);
}

APlayerController* AFF_AtlasCameraRig::GetOwningController() const
{
	return UGameplayStatics::GetPlayerController(this, 0);
}

const UFF_AtlasSettings& AFF_AtlasCameraRig::GetAtlasSettings() const
{
	const UFF_AtlasSettings* Settings = UFF_AtlasSettings::Get();
	check(Settings);
	return *Settings;
}

void AFF_AtlasCameraRig::SetCountryInspection(bool bInspect, float ApproachFractionOverride)
{
	if (CurrentScreen != EFF_UIFlowScreen::AtlasGlobe) { return; }
	if (!bInspect && !InspectionCamera.IsValid()) { return; }
	if (!InspectionCamera.IsValid())
	{
		ACameraActor* Camera = ResolveCameraForScreen(CurrentScreen);
		if (!Camera) { return; }
		InspectionCamera = Camera;
		InspectionBaseLocation = Camera->GetActorLocation();
	}
	ACameraActor* Camera = InspectionCamera.Get();
	FVector TargetLocation = InspectionBaseLocation;
	if (bInspect)
	{
		if (const AFF_AtlasGlobeActor* Globe = AFF_AtlasGlobeActor::Find(this))
		{
			const FVector Delta = Globe->GetActorLocation() - InspectionBaseLocation;
			const double Distance = Delta.Size();
			const double Radius = Globe->GetGlobeRadius() * Globe->GetActorScale3D().GetAbsMax();
			// 기존 설정은 최대 20%, 나라별 명시 설정은 최대 50%. 표면에 너무 가까워지는 값은 모두 제한한다.
			const float Fraction = FMath::IsFinite(ApproachFractionOverride) && ApproachFractionOverride >= 0.f
				? FMath::Clamp(ApproachFractionOverride, 0.f, 0.5f)
				: FMath::Clamp(GetAtlasSettings().CountryInspectionApproachFraction, 0.f, 0.2f);
			const double Approach = FMath::Clamp(Distance * Fraction, 0.0, FMath::Max(0.0, Distance - Radius * 1.35));
			TargetLocation += Delta.GetSafeNormal() * Approach;
		}
	}
	// 회전 도착 알림이 같은 국가를 다시 선택해도 진행 중인 접근 시간을 재시작하지 않는다.
	if (!bInspectionPaused && bCountryInspection == bInspect && InspectionTargetLocation.Equals(TargetLocation, 0.01)) { return; }
	InspectionFromLocation = Camera->GetActorLocation();
	InspectionTargetLocation = TargetLocation;
	InspectionAlpha = 0.f;
	bCountryInspection = bInspect;
	bInspectionPaused = false;
	bInspectionTransitioning = !InspectionFromLocation.Equals(InspectionTargetLocation, 0.01);
	if (!bInspectionTransitioning && !bInspect) { ResetCountryInspection(); }
	// 움직임 감소나 0초 설정은 다음 프레임을 기다리지 않는다.
	Tick(0.f);
}

void AFF_AtlasCameraRig::PauseCountryInspection()
{
	bInspectionTransitioning = false;
	bInspectionPaused = true;
}

void AFF_AtlasCameraRig::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickIntroReveal(DeltaSeconds);

	ACameraActor* Camera = InspectionCamera.Get();
	if (!Camera || CurrentScreen != EFF_UIFlowScreen::AtlasGlobe || !bInspectionTransitioning) { return; }
	const UFF_AtlasSettings& Settings = GetAtlasSettings();
	const UFF_UIAccessibilitySubsystem* Access = UFF_UIAccessibilitySubsystem::Get(this);
	const bool bReduceMotion = Access && Access->IsReduceMotionEnabled();
	InspectionAlpha = bReduceMotion || Settings.CountryInspectionSeconds <= 0.f ? 1.f
		: FMath::FInterpConstantTo(InspectionAlpha, 1.f, DeltaSeconds, 1.f / Settings.CountryInspectionSeconds);
	const float Eased = InspectionAlpha * InspectionAlpha * (3.f - 2.f * InspectionAlpha);
	Camera->SetActorLocation(FMath::Lerp(InspectionFromLocation, InspectionTargetLocation, Eased));
	if (InspectionAlpha >= 1.f)
	{
		bInspectionTransitioning = false;
		if (!bCountryInspection) { ResetCountryInspection(); }
	}
}

void AFF_AtlasCameraRig::ResetCountryInspection()
{
	if (ACameraActor* Camera = InspectionCamera.Get()) { Camera->SetActorLocation(InspectionBaseLocation); }
	InspectionCamera.Reset();
	InspectionAlpha = 0.f;
	bCountryInspection = false;
	bInspectionTransitioning = false;
	bInspectionPaused = false;
}

// --- 타이틀 인트로 ---

void AFF_AtlasCameraRig::PlayIntroReveal()
{
	EnsureDerivedIntroCamera();
	FinishIntroReveal();
	FrameDerivedIntroCamera();

	ACameraActor* Camera = ResolveCameraForScreen(EFF_UIFlowScreen::TitleIntro);
	if (!Camera)
	{
		return;
	}

	// 뷰 타깃을 여기서 다시 잡는다. 첫 화면을 올릴 때 정한 구도는 이 리그의 BeginPlay 가 타이틀로 덮었다.
	ResetCountryInspection();
	CurrentScreen = EFF_UIFlowScreen::TitleIntro;
	ApplyViewTarget(Camera, 0.f);

	// 타이틀 카메라를 대신 쓰는 경우에는 당기지 않는다 — 레벨에 놓은 타이틀 구도를 옮기면 안 된다.
	const AFF_AtlasGlobeActor* Globe = AFF_AtlasGlobeActor::Find(this);
	const UFF_UIAccessibilitySubsystem* Access = UFF_UIAccessibilitySubsystem::Get(this);
	const UFF_AtlasSettings& Settings = GetAtlasSettings();
	if (!Globe || Camera == TitleCamera || (Access && Access->IsReduceMotionEnabled())
		|| Settings.IntroRevealSeconds <= 0.f)
	{
		return;
	}

	IntroRevealCamera = Camera;
	IntroRevealCenter = Globe->GetActorLocation();
	IntroRevealFinalLocation = Camera->GetActorLocation();
	IntroRevealAlpha = 0.f;
	bIntroRevealing = true;
	TickIntroReveal(0.f);
}

void AFF_AtlasCameraRig::TickIntroReveal(float DeltaSeconds)
{
	if (!bIntroRevealing)
	{
		return;
	}
	ACameraActor* Camera = IntroRevealCamera.Get();
	if (!Camera)
	{
		bIntroRevealing = false;
		return;
	}

	const UFF_AtlasSettings& Settings = GetAtlasSettings();
	IntroRevealAlpha = FMath::Min(1.f, IntroRevealAlpha + DeltaSeconds / FMath::Max(Settings.IntroRevealSeconds, KINDA_SMALL_NUMBER));

	// ease-out-back: 빠르게 다가와 살짝 지나쳤다가 제자리에 선다. 그 지나침이 「펑」 이다.
	const float X = IntroRevealAlpha - 1.f;
	const float C1 = Settings.IntroRevealOvershoot;
	const float Eased = 1.f + (C1 + 1.f) * X * X * X + C1 * X * X;
	const float DistanceScale = FMath::Lerp(Settings.IntroRevealStartDistanceScale, 1.f, Eased);
	Camera->SetActorLocation(IntroRevealCenter + (IntroRevealFinalLocation - IntroRevealCenter) * DistanceScale);

	if (IntroRevealAlpha >= 1.f)
	{
		FinishIntroReveal();
	}
}

void AFF_AtlasCameraRig::FinishIntroReveal()
{
	if (!bIntroRevealing)
	{
		return;
	}
	if (ACameraActor* Camera = IntroRevealCamera.Get())
	{
		Camera->SetActorLocation(IntroRevealFinalLocation);
	}
	bIntroRevealing = false;
	IntroRevealCamera.Reset();
}

void AFF_AtlasCameraRig::EnsureDerivedIntroCamera()
{
	if (IntroCamera || DerivedIntroCamera || !TitleCamera)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACameraActor* Camera = World->SpawnActor<ACameraActor>(
		TitleCamera->GetActorLocation(), TitleCamera->GetActorRotation(), Params);
	if (!Camera)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("AFF_AtlasCameraRig: 인트로 구도 카메라를 만들지 못했다. 인트로가 타이틀 구도로 보인다."));
		return;
	}

	// 렌즈를 같게 둔다. 화각·후처리가 다르면 타이틀로 블렌드할 때 화면이 한 번 튄다.
	const UCameraComponent* From = TitleCamera->GetCameraComponent();
	UCameraComponent* To = Camera->GetCameraComponent();
	if (From && To)
	{
		To->SetFieldOfView(From->FieldOfView);
		To->SetConstraintAspectRatio(From->bConstrainAspectRatio);
		To->SetAspectRatio(From->AspectRatio);
		To->bOverrideAspectRatioAxisConstraint = From->bOverrideAspectRatioAxisConstraint;
		To->SetAspectRatioAxisConstraint(From->AspectRatioAxisConstraint);
		To->PostProcessSettings = From->PostProcessSettings;
		To->SetPostProcessBlendWeight(From->PostProcessBlendWeight);
	}
	DerivedIntroCamera = Camera;
}

void AFF_AtlasCameraRig::FrameDerivedIntroCamera()
{
	ACameraActor* Camera = DerivedIntroCamera.Get();
	const AFF_AtlasGlobeActor* Globe = AFF_AtlasGlobeActor::Find(this);
	if (IntroCamera || !Camera || !Globe || !TitleCamera || bIntroRevealing)
	{
		return;
	}

	const UFF_AtlasSettings& Settings = GetAtlasSettings();
	const FVector Center = Globe->GetActorLocation();
	const double Radius = Globe->GetGlobeRadius() * Globe->GetActorScale3D().GetAbsMax();

	// 타이틀 카메라가 있는 쪽에서 본다. 지구의 정면 축 규약을 타이틀과 같게 유지하고,
	// 타이틀로 블렌드할 때 지구가 옆으로 비켜 가기만 하게 한다.
	FVector Direction = TitleCamera->GetActorLocation() - Center;
	if (!Direction.Normalize())
	{
		Direction = Globe->GetActorForwardVector();
	}

	// 화각은 가로 기준이다. 세로 화각은 화면비 규칙에 따라 정해진다.
	const UCameraComponent* Lens = Camera->GetCameraComponent();
	double Aspect = 16.0 / 9.0;
	EAspectRatioAxisConstraint Constraint = AspectRatio_MaintainXFOV;
	if (const APlayerController* Controller = GetOwningController())
	{
		if (const ULocalPlayer* Player = Controller->GetLocalPlayer())
		{
			Constraint = Player->AspectRatioAxisConstraint;
			if (Player->ViewportClient)
			{
				FVector2D Size;
				Player->ViewportClient->GetViewportSize(Size);
				if (Size.X > 1.0 && Size.Y > 1.0)
				{
					Aspect = Size.X / Size.Y;
				}
			}
		}
	}
	if (Lens && Lens->bOverrideAspectRatioAxisConstraint)
	{
		Constraint = Lens->AspectRatioAxisConstraint;
	}
	if (Lens && (Lens->bConstrainAspectRatio || Constraint == AspectRatio_MaintainYFOV))
	{
		Aspect = Lens->AspectRatio;
	}

	const double HalfHorizontal = FMath::DegreesToRadians(Lens ? Lens->FieldOfView : 90.f) * 0.5;
	const double HalfVertical = FMath::Atan(FMath::Tan(HalfHorizontal) / FMath::Max(Aspect, 0.1));
	const double Fraction = FMath::Clamp(static_cast<double>(Settings.IntroGlobeScreenHeightFraction), 0.2, 1.0);
	const double Angular = FMath::Atan(Fraction * FMath::Tan(HalfVertical));
	const double Distance = Radius / FMath::Max(FMath::Sin(Angular), 1e-3);

	const FVector Location = Center + Direction * Distance;
	const FVector Aim = Center + FVector::UpVector * (Settings.IntroGlobeAimOffset * Radius);
	Camera->SetActorLocationAndRotation(Location, (Aim - Location).Rotation());

	// 타이틀 카메라의 초점 거리를 그대로 두면 거리가 달라 지구가 흐려진다.
	if (UCameraComponent* Mutable = Camera->GetCameraComponent();
		Mutable && Mutable->PostProcessSettings.bOverride_DepthOfFieldFocalDistance)
	{
		Mutable->PostProcessSettings.DepthOfFieldFocalDistance = static_cast<float>(Distance - Radius);
	}
}

void AFF_AtlasCameraRig::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetCountryInspection();
	FinishIntroReveal();
	if (DerivedIntroCamera)
	{
		DerivedIntroCamera->Destroy();
		DerivedIntroCamera = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

// =============================================================================
// 콘솔 명령 — 개발용
// =============================================================================
#if !UE_BUILD_SHIPPING

namespace FFAtlasCameraConsole
{
	AFF_AtlasCameraRig* Resolve(UWorld* World)
	{
		return AFF_AtlasCameraRig::Find(World);
	}

	void PrintStatus(const TArray<FString>& Args, UWorld* World)
	{
		if (const AFF_AtlasCameraRig* Rig = Resolve(World))
		{
			UE_LOG(LogFestaFiestaUI, Display, TEXT("FF.Atlas.Camera: %s"), *Rig->DescribeState());
		}
	}

	void Skip(const TArray<FString>& Args, UWorld* World)
	{
		if (AFF_AtlasCameraRig* Rig = Resolve(World))
		{
			Rig->SkipCameraMove();
			UE_LOG(LogFestaFiestaUI, Display, TEXT("FF.Atlas.CameraSkip: %s"), *Rig->DescribeState());
		}
	}

	FAutoConsoleCommandWithWorldAndArgs StatusCommand(
		TEXT("FF.Atlas.Camera"),
		TEXT("카메라 리그 상태를 찍는다 — 지금 화면·뷰타깃·이동 중인지·슬롯 지정 여부."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PrintStatus));

	FAutoConsoleCommandWithWorldAndArgs SkipCommand(
		TEXT("FF.Atlas.CameraSkip"),
		TEXT("진행 중인 카메라 이동을 즉시 끝낸다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Skip));
}

#endif // !UE_BUILD_SHIPPING
