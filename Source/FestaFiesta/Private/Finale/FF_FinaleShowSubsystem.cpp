// Copyright Epic Games, Inc. All Rights Reserved.

#include "Finale/FF_FinaleShowSubsystem.h"

#include "Camera/FF_FestivalPlayerController.h"
#include "Camera/FF_FinaleViewpoint.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Finale/FF_FinaleLaunchSite.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

bool UFF_FinaleShowSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UFF_FinaleShowSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// 시드를 고정하지 않는다. 불꽃은 매번 달라 보이는 편이 낫다.
	// 재현이 필요해지면 여기에 시드를 주입하는 경로를 열면 된다.
	ShowRandom.GenerateNewSeed();
}

void UFF_FinaleShowSubsystem::Deinitialize()
{
	if (bShowInProgress)
	{
		StopShow();
	}
	Super::Deinitialize();
}

AFF_FestivalPlayerController* UFF_FinaleShowSubsystem::GetFestivalController() const
{
	const UWorld* World = GetWorld();
	return World ? Cast<AFF_FestivalPlayerController>(World->GetFirstPlayerController()) : nullptr;
}

FFF_FinaleGradeSpec UFF_FinaleShowSubsystem::ResolveSpec(const EFF_FinaleGrade Grade) const
{
	// 데이터 애셋이 지정돼 있으면 그쪽 값이 이긴다. 없으면 코드 기본값이다.
	if (const UFF_FinaleShowData* Data = ShowData.LoadSynchronous())
	{
		return Data->GetSpec(Grade);
	}
	return FFF_FinaleDefaults::MakeSpec(Grade);
}

bool UFF_FinaleShowSubsystem::StartShow()
{
	EFF_FinaleGrade Grade = EFF_FinaleGrade::None;

	// 등급은 축제 진행 쪽이 소유한다. 여기서는 읽기만 한다.
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (const UFF_FestivalProgressionSubsystem* Progression =
		GameInstance ? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr)
	{
		Grade = Progression->GetPurchasedFinaleGrade();
	}
	else
	{
		// 진행 시스템이 없는 테스트 맵이라면 미구매 연출로 돈다.
		UE_LOG(LogFestaFiestaFinale, Log,
			TEXT("[피날레] 축제 진행 시스템이 없어 미구매 등급으로 진행한다."));
	}

	return StartShowWithGrade(Grade);
}

bool UFF_FinaleShowSubsystem::StartShowWithGrade(const EFF_FinaleGrade Grade)
{
	if (bShowInProgress)
	{
		UE_LOG(LogFestaFiestaFinale, Warning, TEXT("[피날레] 이미 연출이 진행 중이다."));
		return false;
	}

	ActiveSpec = ResolveSpec(Grade);
	if (!ActiveSpec.IsPlayable())
	{
		UE_LOG(LogFestaFiestaFinale, Warning,
			TEXT("[피날레] 등급 %d 의 연출 수치가 비어 있어 시작하지 않는다."), static_cast<int32>(Grade));
		return false;
	}

	ActiveGrade = Grade;
	bShowInProgress = true;
	ElapsedSeconds = 0.f;
	FiredBursts = 0;
	FiredVolleys = 0;
	ColorCursor = 0;
	TailStartedAt = -1.f;

	// 첫 발은 바로 쏜다. 간격만큼 기다리면 시점 전환 뒤 정적이 생긴다.
	NextVolleyAt = 0.f;

	if (bTakeOverCamera)
	{
		if (AFF_FestivalPlayerController* Controller = GetFestivalController())
		{
			// 시점 액터가 없으면 false 를 돌려주는데, 그래도 연출은 진행한다.
			// 카메라가 안 옮겨질 뿐 불꽃은 터져야 한다.
			Controller->EnterScriptedView(nullptr, CameraBlendSeconds);
		}
	}

	UE_LOG(LogFestaFiestaFinale, Log,
		TEXT("[피날레] 연출 시작. 등급=%d 발수=%d 지속=%.1fs 동시=%d"),
		static_cast<int32>(Grade), ActiveSpec.BurstCount, ActiveSpec.DurationSeconds,
		ActiveSpec.SimultaneousShells);

	OnShowStateChanged.Broadcast(true);
	return true;
}

void UFF_FinaleShowSubsystem::StopShow()
{
	if (!bShowInProgress)
	{
		return;
	}

	bShowInProgress = false;

	if (AFF_FestivalPlayerController* Controller = GetFestivalController())
	{
		if (Controller->IsInScriptedView())
		{
			Controller->ExitScriptedView(CameraBlendSeconds);
		}
	}

	UE_LOG(LogFestaFiestaFinale, Log,
		TEXT("[피날레] 연출 종료. 총 %d 발, %.1f 초"), FiredBursts, ElapsedSeconds);

	OnShowStateChanged.Broadcast(false);
}

void UFF_FinaleShowSubsystem::Tick(float DeltaTime)
{
	if (!bShowInProgress || DeltaTime <= 0.f)
	{
		return;
	}

	ElapsedSeconds += DeltaTime;

	// 아직 쏠 게 남았으면 간격을 채울 때마다 한 차례씩 쏜다.
	const int32 TotalVolleys = ActiveSpec.GetVolleyCount();
	if (FiredVolleys < TotalVolleys)
	{
		const float Interval = ActiveSpec.GetIntervalSeconds();

		// 프레임이 크게 밀렸을 때 밀린 만큼 한꺼번에 쏘면 하늘이 갑자기 꽉 찬다.
		// 한 프레임에 최대 두 차례까지만 따라잡는다.
		int32 CatchUp = 0;
		while (ElapsedSeconds >= NextVolleyAt && FiredVolleys < TotalVolleys && CatchUp < 2)
		{
			FireVolley();
			NextVolleyAt += Interval;
			++CatchUp;
		}

		// 따라잡기를 포기했으면 다음 기준을 현재 시각으로 당긴다.
		if (CatchUp >= 2 && ElapsedSeconds > NextVolleyAt)
		{
			NextVolleyAt = ElapsedSeconds + Interval;
		}
		return;
	}

	// 다 쐈으면 여운을 두고 시점을 되돌린다.
	// 마지막 발이 터지자마자 화면이 바뀌면 그 불꽃이 잘린다.
	if (TailStartedAt < 0.f)
	{
		TailStartedAt = ElapsedSeconds;
		return;
	}

	if (ElapsedSeconds - TailStartedAt >= TailSeconds)
	{
		StopShow();
	}
}

FVector UFF_FinaleShowSubsystem::ResolveLaunchOrigin() const
{
	TArray<AFF_FinaleLaunchSite*> Sites;
	AFF_FinaleLaunchSite::CollectAll(GetWorld(), Sites);

	if (Sites.Num() == 0)
	{
		// 레벨이 아직 없는 단계다. 원점 위에서 터뜨리고 연출 자체는 굴린다.
		return FVector::ZeroVector;
	}

	// 지점이 여럿이면 차례로 돌아간다. 좌우에 하나씩 두면 번갈아 터진다.
	const int32 Index = FiredVolleys % Sites.Num();
	return Sites[Index]->GetActorLocation();
}

FVector UFF_FinaleShowSubsystem::PickBurstLocation(const FVector& Origin) const
{
	const float Radius = FMath::Max(0.f, ActiveSpec.SpreadRadius);

	// 원 안에 고르게 흩는다. 반지름을 그냥 균등 추출하면 가운데가 빽빽해진다.
	const float Angle = ShowRandom.FRandRange(0.f, 2.f * PI);
	const float Distance = Radius * FMath::Sqrt(ShowRandom.FRand());

	const float Height = ShowRandom.FRandRange(
		FMath::Min(ActiveSpec.BurstHeightMin, ActiveSpec.BurstHeightMax),
		FMath::Max(ActiveSpec.BurstHeightMin, ActiveSpec.BurstHeightMax));

	return Origin + FVector(
		FMath::Cos(Angle) * Distance,
		FMath::Sin(Angle) * Distance,
		Height);
}

FLinearColor UFF_FinaleShowSubsystem::PickColor(const int32 Index) const
{
	if (ActiveSpec.Colors.Num() == 0)
	{
		return FLinearColor::White;
	}
	return ActiveSpec.Colors[Index % ActiveSpec.Colors.Num()];
}

void UFF_FinaleShowSubsystem::FireVolley()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Origin = ResolveLaunchOrigin();

	// 마지막 차례는 남은 발수만큼만 쏜다. 총 발수를 넘기면 등급 차이가 흐려진다.
	const int32 Remaining = FMath::Max(0, ActiveSpec.BurstCount - FiredBursts);
	const int32 ShellCount = FMath::Min(FMath::Max(1, ActiveSpec.SimultaneousShells), Remaining);

	UNiagaraSystem* Effect = ActiveSpec.BurstEffect.LoadSynchronous();

	for (int32 Shell = 0; Shell < ShellCount; ++Shell)
	{
		const FVector BurstLocation = PickBurstLocation(Origin);
		const FLinearColor Color = PickColor(ColorCursor++);

		if (Effect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Effect, BurstLocation);
		}
		else if (FiredBursts == 0)
		{
			// 에셋이 아직 없는 단계다. 매 발 찍으면 로그가 넘치니 첫 발만 알린다.
			UE_LOG(LogFestaFiestaFinale, Log,
				TEXT("[피날레] 이펙트가 지정되지 않아 위치 신호만 보낸다. 등급 데이터의 BurstEffect 를 확인할 것."));
		}

		// 이펙트가 없어도 신호는 보낸다. 사운드나 화면 흔들림은 여기에 붙는다.
		OnBurst.Broadcast(BurstLocation, Color);
		++FiredBursts;
	}

	++FiredVolleys;
}
