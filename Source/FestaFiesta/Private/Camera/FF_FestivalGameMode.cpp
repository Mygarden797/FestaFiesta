// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/FF_FestivalGameMode.h"

#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Save/FF_SaveSubsystem.h"

#include "Camera/FF_CameraTypes.h"
#include "Camera/FF_FestivalPlayerController.h"
#include "Camera/FF_OverviewCameraPawn.h"

#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

AFF_FestivalGameMode::AFF_FestivalGameMode()
{
	DefaultPawnClass = AFF_OverviewCameraPawn::StaticClass();
	PlayerControllerClass = AFF_FestivalPlayerController::StaticClass();

	// 관찰 시점이라 HUD 나 스테이트 클래스를 따로 지정하지 않는다.
	// 필요해지면 그때 담당자가 정하면 된다.
}

void AFF_FestivalGameMode::BeginPlay()
{
	Super::BeginPlay();

	/* 저장이 있으면 이어서 시작한다.

	   여기서 부르는 이유는 서브시스템이 전부 자기 초기화를 마친 뒤이기 때문이다.
	   Initialize 에서 부르면 다른 시스템이 아직 초기화 중이라 복원한 값을
	   곧바로 덮어쓴다.
	*/
	if (UFF_SaveSubsystem* Save = UFF_SaveSubsystem::Get(this))
	{
		const UFF_FestivalProgressionSubsystem* Progression = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UFF_FestivalProgressionSubsystem>()
			: nullptr;

		const UFF_FestivalDefinition* Definition =
			Progression ? Progression->GetDefinition() : nullptr;

		if (Definition)
		{
			Save->TryContinueFestival(Definition->FestivalId);
		}
	}

	// 첫 플레이어만 다룬다. 이 프로젝트는 싱글 관찰 시점이다.
	AFF_FestivalPlayerController* Controller =
		Cast<AFF_FestivalPlayerController>(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr);
	if (!Controller)
	{
		return;
	}

	// 초기 구도와 캐릭터 준비는 서로 독립이다.
	// 한쪽을 껐다고 다른 쪽까지 건너뛰면, 레벨에 배치한 카메라 위치를 쓰려고
	// bApplyInitialFraming 을 끈 순간 전환할 캐릭터가 사라진다.
	if (bPrepareVisitorCharacter)
	{
		PrepareVisitorCharacter(Controller);
	}

	if (!bApplyInitialFraming)
	{
		return;
	}

	if (AFF_OverviewCameraPawn* CameraPawn = Controller->GetOverviewCameraPawn())
	{
		CameraPawn->InitializeOverview(InitialFocusLocation, InitialArmLength);
	}
	else
	{
		UE_LOG(LogFestaFiestaCamera, Warning,
			TEXT("[카메라] 관찰 카메라 Pawn 을 못 찾아 초기 구도를 적용하지 못했다. ")
			TEXT("GameMode 의 DefaultPawnClass 를 확인할 것."));
	}
}

void AFF_FestivalGameMode::PrepareVisitorCharacter(AFF_FestivalPlayerController* Controller)
{
	UWorld* World = GetWorld();
	if (!World || !Controller || !VisitorCharacterClass)
	{
		if (!VisitorCharacterClass)
		{
			UE_LOG(LogFestaFiestaCamera, Warning,
				TEXT("[캐릭터] VisitorCharacterClass 가 비어 있어 전환할 캐릭터가 없다. ")
				TEXT("GameMode 에서 지정하거나 bPrepareVisitorCharacter 를 끌 것."));
		}
		return;
	}

	// 레벨에 이미 놓여 있으면 그것을 쓴다. 작업 중에 위치를 눈으로 잡아둔 것을 존중한다.
	for (TActorIterator<APawn> It(World, VisitorCharacterClass); It; ++It)
	{
		if (IsValid(*It))
		{
			Controller->SetVisitorCharacter(*It);
			UE_LOG(LogFestaFiestaCamera, Log,
				TEXT("[캐릭터] 레벨에 배치된 방문객 캐릭터를 사용한다: %s"), *It->GetName());
			return;
		}
	}

	// 없으면 PlayerStart 에 스폰한다.
	const AActor* StartSpot = FindPlayerStart(Controller);
	const FTransform SpawnTransform = StartSpot
		? StartSpot->GetActorTransform()
		: FTransform(FRotator::ZeroRotator, InitialFocusLocation);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnParams.Owner = Controller;

	APawn* Spawned = World->SpawnActor<APawn>(VisitorCharacterClass, SpawnTransform, SpawnParams);
	if (!Spawned)
	{
		UE_LOG(LogFestaFiestaCamera, Warning, TEXT("[캐릭터] 방문객 캐릭터 스폰에 실패했다."));
		return;
	}

	// 스폰만 하고 빙의는 하지 않는다. 시작 시점은 관찰 시점이고,
	// 전환 키를 눌렀을 때 비로소 이 캐릭터로 들어간다.
	Controller->SetVisitorCharacter(Spawned);
	UE_LOG(LogFestaFiestaCamera, Log, TEXT("[캐릭터] 방문객 캐릭터를 %s 에 스폰했다."),
		StartSpot ? TEXT("PlayerStart") : TEXT("초기 초점 위치"));
}
