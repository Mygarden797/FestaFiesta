// Copyright Epic Games, Inc. All Rights Reserved.

#include "Finale/FF_FinaleShowSubsystem.h"

#include "Camera/FF_FestivalPlayerController.h"
#include "Camera/FF_FinaleViewpoint.h"
#include "Engine/GameInstance.h"
#if !UE_BUILD_SHIPPING
#include "HAL/IConsoleManager.h"
#endif
#include "Engine/World.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "Camera/FF_FestivalGameMode.h"
#include "Finale/FF_FinaleLaunchSite.h"
#include "Finale/FF_FinaleSettings.h"
#include "GameFramework/GameModeBase.h"
// GetPlayerViewPoint 를 직접 부른다. 컨트롤러 헤더가 끌고 오지만 명시해 둔다.
#include "GameFramework/PlayerController.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraParameterStore.h"
#include "NiagaraSystem.h"

#if !UE_BUILD_SHIPPING

namespace
{
	// 콘솔 명령 이름은 IConsoleManager 가 **프로세스 전역**으로 들고 있는데,
	// 이 서브시스템은 **월드마다** 만들어진다. 수명이 어긋난다.
	//
	// 자동화 테스트는 월드를 수십 개 만들고 부순다. 인스턴스가 무조건 등록·해제하면
	// 같은 이름을 두 인스턴스가 나눠 갖게 되고, 앞 월드가 지운 객체를 뒤 월드가
	// 다시 지우면서 해제된 가상 함수 테이블로 점프한다(EXCEPTION_ACCESS_VIOLATION).
	//
	// 그래서 규칙을 둘로 고정한다.
	//   등록: 그 이름이 비어 있을 때만 등록하고, 아니면 포인터를 들지 않는다.
	//   해제: 내가 등록한 그 객체가 아직 그 이름의 주인일 때만 지운다.
	const TCHAR* const FinalePlayCommandName = TEXT("FF.Finale.Play");
	const TCHAR* const FinaleStopCommandName = TEXT("FF.Finale.Stop");
	const TCHAR* const FinaleStatCommandName = TEXT("FF.Finale.Stat");
	const TCHAR* const FinaleSkipCameraCommandName = TEXT("FF.Finale.SkipCamera");

	bool IsConsoleNameFree(const TCHAR* Name)
	{
		return IConsoleManager::Get().FindConsoleObject(Name, false) == nullptr;
	}

	void UnregisterOwnedConsoleCommand(IConsoleCommand*& Command, const TCHAR* Name)
	{
		if (Command == nullptr)
		{
			return;
		}

		IConsoleManager& ConsoleManager = IConsoleManager::Get();
		if (ConsoleManager.FindConsoleObject(Name, false) == Command)
		{
			ConsoleManager.UnregisterConsoleObject(Command);
		}

		// 주인이 아니어도 포인터는 반드시 버린다. 남겨두면 다음 해제에서 같은 사고가 난다.
		Command = nullptr;
	}
}

#endif // !UE_BUILD_SHIPPING

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

#if !UE_BUILD_SHIPPING
	RegisterDebugConsoleCommands();
#endif
	BindFestivalStage();
}

void UFF_FinaleShowSubsystem::BindFestivalStage()
{
	if (bStageBound)
	{
		return;
	}

	UFF_FestivalStageSubsystem* Stage = GetGameInstanceSubsystem<UFF_FestivalStageSubsystem>();
	if (!Stage)
	{
		// 단계 시스템이 없는 테스트 맵이면 자동 시작만 못 할 뿐 콘솔 재생은 된다.
		UE_LOG(LogFestaFiestaFinale, Log,
			TEXT("[피날레] 축제 단계 시스템이 없어 자동 시작을 연결하지 않는다."));
		return;
	}

	Stage->OnStageChanged.AddDynamic(this, &UFF_FinaleShowSubsystem::HandleFestivalStageChanged);
	bStageBound = true;

	// 이미 피날레 단계에서 시작한 경우. 신호는 지나갔으므로 현재 값을 직접 본다.
	if (bAutoStartOnFinaleStage && Stage->CurrentStage == EFF_FestivalStage::Finale)
	{
		StartShow();
	}
}

void UFF_FinaleShowSubsystem::UnbindFestivalStage()
{
	if (!bStageBound)
	{
		return;
	}

	if (UFF_FestivalStageSubsystem* Stage = GetGameInstanceSubsystem<UFF_FestivalStageSubsystem>())
	{
		Stage->OnStageChanged.RemoveDynamic(this, &UFF_FinaleShowSubsystem::HandleFestivalStageChanged);
	}
	bStageBound = false;
}

void UFF_FinaleShowSubsystem::HandleFestivalStageChanged(
	const EFF_FestivalStage PreviousStage,
	const EFF_FestivalStage NewStage)
{
	(void)PreviousStage;

	// 피날레를 벗어나면 진행 중이던 연출을 접는다. 단계가 넘어갔는데 불꽃이
	// 계속 터지면 다음 화면 위에 얹힌다.
	if (NewStage != EFF_FestivalStage::Finale)
	{
		if (bShowInProgress)
		{
			StopShow();
		}
		return;
	}

	if (!bAutoStartOnFinaleStage)
	{
		return;
	}
	StartShow();
}

void UFF_FinaleShowSubsystem::Deinitialize()
{
	UnbindFestivalStage();
#if !UE_BUILD_SHIPPING
	UnregisterDebugConsoleCommands();
#endif

	if (bShowInProgress)
	{
		StopShow();
	}
	Super::Deinitialize();
}

#if !UE_BUILD_SHIPPING

bool UFF_FinaleShowSubsystem::TryParseGrade(const FString& Token, EFF_FinaleGrade& OutGrade)
{
	const FString Upper = Token.TrimStartAndEnd().ToUpper();

	if (Upper == TEXT("S")) { OutGrade = EFF_FinaleGrade::S; return true; }
	if (Upper == TEXT("A")) { OutGrade = EFF_FinaleGrade::A; return true; }
	if (Upper == TEXT("B")) { OutGrade = EFF_FinaleGrade::B; return true; }
	if (Upper == TEXT("C")) { OutGrade = EFF_FinaleGrade::C; return true; }
	if (Upper == TEXT("NONE") || Upper == TEXT("N")) { OutGrade = EFF_FinaleGrade::None; return true; }

	return false;
}

void UFF_FinaleShowSubsystem::RegisterDebugConsoleCommands()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();

	// 중복 등록 방어. 어떤 이유로 해제가 안 됐어도 두 번째 PIE 가 조용히 망가지지 않게 한다.
	UnregisterDebugConsoleCommands();

	// 이 명령들은 한 인스턴스가 함께 갖는다. 다른 월드가 이미 갖고 있으면 통째로 넘긴다.
	// 명령은 프로세스에 하나면 충분하고, 두 인스턴스가 같은 객체를 나눠 갖는 것이 사고의 원인이다.
	if (!IsConsoleNameFree(FinalePlayCommandName))
	{
		return;
	}

	PlayConsoleCommand = ConsoleManager.RegisterConsoleCommand(
		FinalePlayCommandName,
		TEXT("피날레 연출을 즉시 재생한다. 사용법: FF.Finale.Play [None|C|B|A|S]  등급 생략 시 구매한 등급"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			// 이미 돌고 있으면 갈아엎는다. 등급을 바꿔가며 비교할 때 매번 멈추기 번거롭다.
			if (bShowInProgress)
			{
				StopShow();
			}

			if (Args.Num() == 0)
			{
				StartShow();
				return;
			}

			EFF_FinaleGrade Grade = EFF_FinaleGrade::None;
			if (!TryParseGrade(Args[0], Grade))
			{
				UE_LOG(LogFestaFiestaFinale, Warning,
					TEXT("[피날레] 알 수 없는 등급입니다: %s (None, C, B, A, S 중 하나)"), *Args[0]);
				return;
			}
			StartShowWithGrade(Grade);
		}));

	StopConsoleCommand = ConsoleManager.RegisterConsoleCommand(
		FinaleStopCommandName,
		TEXT("진행 중인 피날레 연출을 중단하고 시점을 되돌린다."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			StopShow();
		}));

	SkipCameraConsoleCommand = ConsoleManager.RegisterConsoleCommand(
		FinaleSkipCameraCommandName,
		TEXT("카메라 순회만 끊는다. 불꽃은 계속 터진다."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			SkipCinematicCamera();
		}));

	StatConsoleCommand = ConsoleManager.RegisterConsoleCommand(
		FinaleStatCommandName,
		TEXT("현재 연출 상태와 등급별 수치를 출력한다."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			UE_LOG(LogFestaFiestaFinale, Display,
				TEXT("[피날레] 진행=%s 등급=%d 발사=%d/%d 경과=%.1fs"),
				bShowInProgress ? TEXT("예") : TEXT("아니오"),
				static_cast<int32>(ActiveGrade), FiredBursts, ActiveSpec.BurstCount, ElapsedSeconds);

			// 등급별 수치를 같이 찍는다. 데이터 애셋을 고치고 나서 반영됐는지 바로 확인할 수 있다.
			for (const EFF_FinaleGrade Grade : FFF_FinaleGrades::GetAscendingOrder())
			{
				const FFF_FinaleGradeSpec Spec = ResolveSpec(Grade);
				UE_LOG(LogFestaFiestaFinale, Display,
					TEXT("  등급 %d: %d발 / %.0f초 / 동시 %d / %d차례 / 간격 %.2fs / 색 %d개 / 이펙트 %s"),
					static_cast<int32>(Grade), Spec.BurstCount, Spec.DurationSeconds,
					Spec.SimultaneousShells, Spec.GetVolleyCount(), Spec.GetIntervalSeconds(),
					Spec.Colors.Num(),
					*DescribeEffects(Spec));
			}

			// 어느 애셋이 먹었는지 같이 찍는다. 세 군데서 올 수 있어서
			// "값을 고쳤는데 왜 안 바뀌지" 가 나오기 쉽다.
			const UFF_FinaleShowData* Data = ResolveShowData();
			UE_LOG(LogFestaFiestaFinale, Display,
				TEXT("  사용 중인 데이터: %s"),
				Data ? *Data->GetName() : TEXT("없음 (코드 기본값)"));

			TArray<AFF_FinaleLaunchSite*> Sites;
			AFF_FinaleLaunchSite::CollectAll(GetWorld(), Sites);
			UE_LOG(LogFestaFiestaFinale, Display,
				TEXT("  발사 지점 %d개 / 연출 시점 %s"),
				Sites.Num(),
				AFF_FinaleViewpoint::FindBest(GetWorld()) ? TEXT("있음") : TEXT("없음"));

			TArray<AFF_FinaleViewpoint*> TourCandidates;
			AFF_FinaleViewpoint::CollectTour(GetWorld(), TourCandidates);
			UE_LOG(LogFestaFiestaFinale, Display,
				TEXT("  순회 가능 시점 %d개 / 지금 쓰는 %d개 / 끊김=%s"),
				TourCandidates.Num(), TourViewpoints.Num(),
				bTourSkipped ? TEXT("예") : TEXT("아니오"));
		}));
}

void UFF_FinaleShowSubsystem::UnregisterDebugConsoleCommands()
{
	// 해제는 조건부, 정리는 무조건.
	// 조건이 "포인터가 있는가" 만이면 부족하다 — 다른 월드가 이미 지운 객체를 가리키고
	// 있을 수 있어서, 그 이름의 현재 주인이 나인지까지 확인한다.
	UnregisterOwnedConsoleCommand(PlayConsoleCommand, FinalePlayCommandName);
	UnregisterOwnedConsoleCommand(StopConsoleCommand, FinaleStopCommandName);
	UnregisterOwnedConsoleCommand(StatConsoleCommand, FinaleStatCommandName);
	UnregisterOwnedConsoleCommand(SkipCameraConsoleCommand, FinaleSkipCameraCommandName);
}

#endif // !UE_BUILD_SHIPPING

AFF_FestivalPlayerController* UFF_FinaleShowSubsystem::GetFestivalController() const
{
	const UWorld* World = GetWorld();
	return World ? Cast<AFF_FestivalPlayerController>(World->GetFirstPlayerController()) : nullptr;
}

const UFF_FinaleShowData* UFF_FinaleShowSubsystem::ResolveShowData() const
{
	// 1. 코드에서 직접 꽂은 것. 연출을 임시로 바꿔볼 때 쓴다.
	if (const UFF_FinaleShowData* Direct = ShowData.LoadSynchronous())
	{
		return Direct;
	}

	// 2. 이 레벨의 GameMode 지정. 연등축제처럼 연출이 다른 레벨에서 쓴다.
	const UWorld* World = GetWorld();
	if (const AFF_FestivalGameMode* GameMode =
		World ? Cast<AFF_FestivalGameMode>(World->GetAuthGameMode()) : nullptr)
	{
		if (const UFF_FinaleShowData* FromLevel = GameMode->FinaleShowDataOverride.LoadSynchronous())
		{
			return FromLevel;
		}
	}

	// 3. 프로젝트 기본값. 대부분의 레벨은 여기까지 온다.
	if (const UFF_FinaleSettings* Settings = UFF_FinaleSettings::Get())
	{
		if (const UFF_FinaleShowData* FromSettings = Settings->DefaultShowData.LoadSynchronous())
		{
			return FromSettings;
		}
	}

	return nullptr;
}

FFF_FinaleGradeSpec UFF_FinaleShowSubsystem::ResolveSpec(const EFF_FinaleGrade Grade) const
{
	// 데이터 애셋이 있으면 그쪽 값이 이긴다. 없으면 코드 기본값이다.
	if (const UFF_FinaleShowData* Data = ResolveShowData())
	{
		return Data->GetSpec(Grade);
	}
	return FFF_FinaleDefaults::MakeSpec(Grade);
}

bool UFF_FinaleShowSubsystem::StartShow()
{
	EFF_FinaleGrade Grade = EFF_FinaleGrade::None;

	// 등급은 축제 진행 쪽이 소유한다. 여기서는 읽기만 한다.
	if (const UFF_FestivalProgressionSubsystem* Progression =
		GetGameInstanceSubsystem<UFF_FestivalProgressionSubsystem>())
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
		// 미구매는 원래 안 쏜다(FFF_FinaleDefaults 주석). 그건 경고할 일이 아니다.
		if (Grade == EFF_FinaleGrade::None)
		{
			UE_LOG(LogFestaFiestaFinale, Log, TEXT("[피날레] 미구매라 불꽃을 쏘지 않는다."));
		}
		else
		{
			UE_LOG(LogFestaFiestaFinale, Warning,
				TEXT("[피날레] 등급 %d 의 연출 수치가 비어 있어 시작하지 않는다."), static_cast<int32>(Grade));
		}
		return false;
	}

	ActiveGrade = Grade;
	bShowInProgress = true;
	ElapsedSeconds = 0.f;
	FiredBursts = 0;
	PendingShells.Reset();
	bWarnedHighShellCount = false;
	FiredVolleys = 0;
	ColorCursor = 0;
	TailStartedAt = -1.f;
	bWarnedMissingEffect = false;
	bWarnedDeadline = false;

	// 연출을 새로 시작하면 검사도 다시 한다. 데이터 애셋을 고치고 재생했을 때
	// 바뀐 이펙트를 그대로 통과시키면 안 된다.
	VerifiedEffect = nullptr;

	// 첫 발은 바로 쏜다. 간격만큼 기다리면 시점 전환 뒤 정적이 생긴다.
	NextVolleyAt = 0.f;

	BuildTour();

	if (bTakeOverCamera)
	{
		// 시점이 없어도 연출은 진행한다. 카메라가 안 옮겨질 뿐 불꽃은 터져야 한다.
		ApplyTourViewpoint(CameraBlendSeconds);
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

	// 예약된 발사는 버린다. 연출이 끝난 뒤에 뒤늦게 터지면 이상하다.
	PendingShells.Reset();

	/* 순회 목록도 비운다.

	   남겨두면 IsCinematicTourActive 가 연출이 끝난 뒤에도 true 를 돌려줘서
	   `카메라 건너뛰기` 안내가 계속 떠 있다. 시점 액터를 강참조로 붙잡고 있기도 하다.
	*/
	TourViewpoints.Reset();
	TourIndex = 0;
	TourHeldSeconds = 0.f;

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

	/* 지연 발사를 먼저 처리한다.

	   뒤에서 처리하면 이번 프레임에 예약된 것이 한 프레임 늦게 나간다.
	   일렬 패턴의 간격이 0.05초 단위라 한 프레임 차이가 눈에 보인다.

	   때가 된 것을 먼저 빼낸 뒤에 쏜다. 쏘면 OnBurst 가 울리는데, 받는 쪽이 연출을
	   멈추면 StopShow 가 대기열을 비운다. 대기열을 돌면서 쏘면 그 순간 인덱스가
	   배열 밖을 가리킨다.
	*/
	TArray<FPendingShell> DueShells;
	for (int32 Index = PendingShells.Num() - 1; Index >= 0; --Index)
	{
		FPendingShell& Pending = PendingShells[Index];
		Pending.DelayRemaining -= DeltaTime;

		if (Pending.DelayRemaining <= 0.f)
		{
			DueShells.Add(Pending);
			PendingShells.RemoveAtSwap(Index);
		}
	}

	for (const FPendingShell& Due : DueShells)
	{
		if (!bShowInProgress)
		{
			return;
		}
		SpawnShell(Due.Location, Due.Color, Due.Effect.Get());
	}

	if (!bShowInProgress)
	{
		return;
	}

	/* 카메라 순회. 자리마다 정한 시간만큼 머물고 다음으로 넘어간다.

	   스킵했으면 더 움직이지 않는다. 플레이어가 끊은 뒤에 다시 끌고 가면
	   끊은 의미가 없다. 카메라를 넘겨받지 않는 연출이면 처음부터 돌지 않는다 —
	   안 그러면 플레이어가 보던 시점을 순회가 빼앗아 간다.
	*/
	if (bTakeOverCamera && !bTourSkipped && TourViewpoints.Num() > 1)
	{
		TourHeldSeconds += DeltaTime;

		const AFF_FinaleViewpoint* Current = TourViewpoints.IsValidIndex(TourIndex)
			? TourViewpoints[TourIndex].Get()
			: nullptr;
		const float Hold = Current ? FMath::Max(0.5f, Current->TourHoldSeconds) : 6.f;

		if (TourHeldSeconds >= Hold)
		{
			AdvanceTour();
		}
	}

	/* 아직 쏠 게 남았으면 간격을 채울 때마다 한 차례씩 쏜다.

	   끝은 차례 수가 아니라 남은 발수로 정한다. 차례 수는 기댓값이라(GetVolleyCount)
	   그걸로 끊으면 발이 남았는데 끝나거나, 발이 동났는데 빈 차례가 계속 돈다.

	   다만 무한히 돌지는 않게 시간으로 한 번 더 막는다. 모든 차례가 0발을 내는
	   설정(예: 모양 애셋이 전부 비었는데 흩뿌림도 막힌 경우)이 생겨도 연출은 끝나야 한다.
	*/
	const float FiringDeadline = FMath::Max(ActiveSpec.DurationSeconds, 1.f) * 2.f;
	const bool bPastDeadline = ElapsedSeconds >= FiringDeadline;

	if (bPastDeadline && GetRemainingShells() > 0 && !bWarnedDeadline)
	{
		bWarnedDeadline = true;
		UE_LOG(LogFestaFiestaFinale, Warning,
			TEXT("[피날레] 지속 시간의 두 배가 지나도록 %d발이 남아 발사를 멈춘다. 볼리 설정을 볼 것."),
			GetRemainingShells());
	}

	if (!bPastDeadline && GetRemainingShells() > 0)
	{
		const float Interval = ActiveSpec.GetIntervalSeconds();

		// 프레임이 크게 밀렸을 때 밀린 만큼 한꺼번에 쏘면 하늘이 갑자기 꽉 찬다.
		// 한 프레임에 최대 두 차례까지만 따라잡는다.
		int32 CatchUp = 0;
		while (bShowInProgress && ElapsedSeconds >= NextVolleyAt
			&& GetRemainingShells() > 0 && CatchUp < 2)
		{
			FireVolley();
			NextVolleyAt += Interval;
			++CatchUp;
		}

		if (!bShowInProgress)
		{
			return;
		}

		// 따라잡기를 포기했으면 다음 기준을 현재 시각으로 당긴다.
		if (CatchUp >= 2 && ElapsedSeconds > NextVolleyAt)
		{
			NextVolleyAt = ElapsedSeconds + Interval;
		}
		return;
	}

	// 지연 발사가 남아 있으면 기다린다. 여운을 먼저 세면 줄지어 나가던 마지막 발들이 잘린다.
	if (PendingShells.Num() > 0 && !bPastDeadline)
	{
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

FVector UFF_FinaleShowSubsystem::ResolveAudienceDirection(const FVector& Center) const
{
	/* 모양이 마주봐야 할 방향. 중심에서 보는 쪽으로 향한다.

	   순회 중인 시점을 먼저 본다. 카메라가 거기 있으니 그게 곧 관객의 눈이다.
	   순회를 끊었거나 시점이 없으면 플레이어 카메라를 본다.

	   둘 다 없으면(테스트 맵, 콘솔 재생) 기본축을 돌려준다. 방향을 모를 때
	   모양을 안 그리는 것보다, 한 방향으로라도 세우는 편이 낫다.
	*/
	// 순회 시점은 카메라가 실제로 거기 있을 때만 믿는다. 끊었거나 넘겨받지 않았으면
	// 플레이어는 자기 카메라로 보고 있다.
	const bool bViewingFromTour = bTakeOverCamera && !bTourSkipped;

	if (bViewingFromTour && TourViewpoints.IsValidIndex(TourIndex))
	{
		if (const AFF_FinaleViewpoint* Viewpoint = TourViewpoints[TourIndex])
		{
			const FVector Direction = (Viewpoint->GetActorLocation() - Center).GetSafeNormal2D();
			if (!Direction.IsNearlyZero())
			{
				return Direction;
			}
		}
	}

	if (const UWorld* World = GetWorld())
	{
		if (const APlayerController* Controller = World->GetFirstPlayerController())
		{
			FVector ViewLocation = FVector::ZeroVector;
			FRotator ViewRotation = FRotator::ZeroRotator;
			Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);

			const FVector Direction = (ViewLocation - Center).GetSafeNormal2D();
			if (!Direction.IsNearlyZero())
			{
				return Direction;
			}
		}
	}

	return FVector::ForwardVector;
}

FLinearColor UFF_FinaleShowSubsystem::PickColor(const int32 Index) const
{
	if (ActiveSpec.Colors.Num() == 0)
	{
		return FLinearColor::White;
	}
	return ActiveSpec.Colors[Index % ActiveSpec.Colors.Num()];
}

void UFF_FinaleShowSubsystem::VerifyEffectParameters(const UNiagaraSystem& Effect) const
{
	// 주의: 이 함수만 Niagara 내부 파라미터 조회에 의존한다.
	// 엔진 버전에 따라 조회 API 이름이 다를 수 있는데, 그때는 이 함수 본문만
	// 비워도 나머지 연출은 그대로 돈다. 값 전달과 재생 경로는 여기에 얽혀 있지 않다.
	const FNiagaraUserRedirectionParameterStore& Store = Effect.GetExposedParameters();

	TArray<FNiagaraVariable> Exposed;
	Store.GetParameters(Exposed);

	// 이펙트가 실제로 노출한 이름을 모은다. User. 접두사가 붙어 오는 경우가 있어 양쪽을 다 담는다.
	TSet<FName> Available;
	for (const FNiagaraVariable& Variable : Exposed)
	{
		const FString Full = Variable.GetName().ToString();
		Available.Add(Variable.GetName());

		FString Trimmed;
		if (Full.Split(TEXT("."), nullptr, &Trimmed, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
		{
			Available.Add(FName(*Trimmed));
		}
	}

	const TArray<FName> Required = {
		FFF_FinaleParams::BurstColor,
		FFF_FinaleParams::BurstScale,
		FFF_FinaleParams::TrailLength
	};

	TArray<FString> Missing;
	for (const FName& Name : Required)
	{
		if (!Available.Contains(Name))
		{
			Missing.Add(Name.ToString());
		}
	}

	if (Missing.Num() == 0)
	{
		UE_LOG(LogFestaFiestaFinale, Verbose,
			TEXT("[피날레] 이펙트 %s 의 파라미터가 모두 연결되었다."), *Effect.GetName());
		return;
	}

	// 없어도 연출은 계속한다. 색이 안 먹을 뿐 불꽃은 터져야 한다.
	UE_LOG(LogFestaFiestaFinale, Warning,
		TEXT("[피날레] 이펙트 %s 에 User 파라미터가 없어 값이 무시된다: %s"),
		*Effect.GetName(), *FString::Join(Missing, TEXT(", ")));
	UE_LOG(LogFestaFiestaFinale, Warning,
		TEXT("[피날레] Niagara 에서 같은 이름으로 User 파라미터를 만들면 등급별 값이 반영된다."));
}

void UFF_FinaleShowSubsystem::BuildTour()
{
	TourViewpoints.Reset();
	TourIndex = 0;
	TourHeldSeconds = 0.f;
	bTourSkipped = false;

	TArray<AFF_FinaleViewpoint*> Candidates;
	AFF_FinaleViewpoint::CollectTour(GetWorld(), Candidates);

	if (Candidates.Num() == 0)
	{
		// 순회용으로 표시된 시점이 없다. 고정 연출로 쓸 자리라도 찾아본다.
		if (AFF_FinaleViewpoint* Best = AFF_FinaleViewpoint::FindBest(GetWorld()))
		{
			TourViewpoints.Add(Best);
		}
		return;
	}

	// 0 이면 제한 없음. 등급이 낮으면 앞에서부터 몇 개만 쓴다.
	const int32 Limit = ActiveSpec.TourViewpointLimit;
	const int32 Take = (Limit <= 0) ? Candidates.Num() : FMath::Min(Limit, Candidates.Num());

	for (int32 Index = 0; Index < Take; ++Index)
	{
		TourViewpoints.Add(Candidates[Index]);
	}

	UE_LOG(LogFestaFiestaFinale, Log,
		TEXT("[피날레] 시점 %d개 중 %d개를 쓴다. (제한 %d)"),
		Candidates.Num(), TourViewpoints.Num(), Limit);
}

void UFF_FinaleShowSubsystem::ApplyTourViewpoint(const float BlendSeconds)
{
	AFF_FestivalPlayerController* Controller = GetFestivalController();
	if (!Controller)
	{
		return;
	}

	AFF_FinaleViewpoint* Viewpoint = TourViewpoints.IsValidIndex(TourIndex)
		? TourViewpoints[TourIndex].Get()
		: nullptr;

	// nullptr 을 넘기면 컨트롤러가 알아서 가장 나은 자리를 고른다.
	Controller->EnterScriptedView(Viewpoint, BlendSeconds);
}

void UFF_FinaleShowSubsystem::AdvanceTour()
{
	if (TourViewpoints.Num() <= 1)
	{
		return;
	}

	TourIndex = (TourIndex + 1) % TourViewpoints.Num();
	TourHeldSeconds = 0.f;

	// 다음 자리가 정한 전환 시간을 쓴다. 자리마다 컷으로 자를지 흐르게 할지 다르다.
	const AFF_FinaleViewpoint* Next = TourViewpoints[TourIndex].Get();
	const float Blend = Next ? Next->TourBlendSeconds : CameraBlendSeconds;

	ApplyTourViewpoint(Blend);
}

void UFF_FinaleShowSubsystem::SkipCinematicCamera()
{
	if (bTourSkipped || TourViewpoints.Num() <= 1)
	{
		return;
	}

	bTourSkipped = true;

	// 카메라만 놓아준다. 연출은 그대로 둔다.
	if (AFF_FestivalPlayerController* Controller = GetFestivalController())
	{
		if (Controller->IsInScriptedView())
		{
			Controller->ExitScriptedView(CameraBlendSeconds);
		}
	}

	UE_LOG(LogFestaFiestaFinale, Log, TEXT("[피날레] 카메라 순회를 끊었다. 불꽃은 계속된다."));
}

const FFF_FinaleVolley& UFF_FinaleShowSubsystem::PickVolley(const int32 RemainingShells) const
{
	// 패턴이 없으면 단발이다. 정적 객체를 돌려줘 호출부가 널 검사를 안 하게 한다.
	static const FFF_FinaleVolley SingleShot;

	if (ActiveSpec.Volleys.Num() == 0)
	{
		return SingleShot;
	}

	/* 남은 발수로 온전히 그릴 수 있는 것만 후보로 둔다.

	   모양이 있는 패턴은 발수가 곧 모양이다. 12발짜리 원을 3발로 줄이면 원이 아니라
	   점 세 개가 되고, 연출의 마지막이 그 꼴로 끝난다. 발수를 깎는 것보다
	   그 차례에 단발을 쏘는 편이 낫다.

	   자리를 안 정하는 패턴(단발·동시)은 발수가 줄어도 그냥 덜 터질 뿐이라 거른다.
	*/
	auto Fits = [RemainingShells](const FFF_FinaleVolley& Volley)
	{
		return !Volley.HasFixedLayout() || Volley.ShellCount <= RemainingShells;
	};

	float Total = 0.f;
	for (const FFF_FinaleVolley& Volley : ActiveSpec.Volleys)
	{
		if (Fits(Volley))
		{
			Total += FMath::Max(0.01f, Volley.Weight);
		}
	}

	if (Total <= 0.f)
	{
		// 남은 발수로 그릴 수 있는 모양이 없다. 남은 것은 단발로 채운다.
		return SingleShot;
	}

	// 가중치로 뽑는다. 같은 패턴이 계속 나오면 연출이 단조로워진다.
	float Roll = ShowRandom.FRandRange(0.f, Total);
	for (const FFF_FinaleVolley& Volley : ActiveSpec.Volleys)
	{
		if (!Fits(Volley))
		{
			continue;
		}

		Roll -= FMath::Max(0.01f, Volley.Weight);
		if (Roll <= 0.f)
		{
			return Volley;
		}
	}

	// 부동소수 오차로 여기까지 오면 마지막으로 통과한 것을 쓴다.
	for (int32 Index = ActiveSpec.Volleys.Num() - 1; Index >= 0; --Index)
	{
		if (Fits(ActiveSpec.Volleys[Index]))
		{
			return ActiveSpec.Volleys[Index];
		}
	}

	return SingleShot;
}

void UFF_FinaleShowSubsystem::SpawnShell(
	const FVector& Location, const FLinearColor& Color, UNiagaraSystem* Effect)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 이펙트가 바뀌었을 때만 검사한다.
	//
	// 여러 종류를 섞어 쓰면 종류마다 한 번씩 돈다. 파라미터 이름이 빠진 이펙트가
	// 섞여 있으면 그것만 색이 안 바뀌는데, 검사를 첫 발에서만 하면 그걸 놓친다.
	if (Effect && VerifiedEffect.Get() != Effect)
	{
		VerifyEffectParameters(*Effect);
		VerifiedEffect = Effect;
	}

	if (Effect)
	{
		// 컴포넌트를 받아 파라미터를 넣는다. SpawnSystemAtLocation 자체는 값을 못 넘긴다.
		UNiagaraComponent* Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World, Effect, Location);

		if (Spawned)
		{
			// 이펙트에 같은 이름의 User 파라미터가 없으면 조용히 무시된다.
			// 크래시가 안 나는 대신 "왜 색이 안 바뀌지" 로 나타나므로, 이름은 상수로만 쓴다.
			Spawned->SetColorParameter(FFF_FinaleParams::BurstColor, Color);
			Spawned->SetFloatParameter(FFF_FinaleParams::BurstScale, ActiveSpec.BurstScale);
			Spawned->SetFloatParameter(FFF_FinaleParams::TrailLength, ActiveSpec.TrailLength);
		}
	}
	else if (!bWarnedMissingEffect)
	{
		// 에셋이 아직 없는 단계다. 매 발 찍으면 로그가 넘치니 한 번만 알린다.
		// 어느 애셋의 몇 칸이 찼는지 같이 찍는다. 칸은 채웠는데 안 나올 때 원인이 여기서 갈린다.
		bWarnedMissingEffect = true;
		const UFF_FinaleShowData* Data = ResolveShowData();
		UE_LOG(LogFestaFiestaFinale, Warning,
			TEXT("[피날레] 이펙트 없이 위치 신호만 보낸다. 데이터=%s 등급=%d 이펙트 %s. "
				 "BurstEffects 를 확인할 것."),
			Data ? *Data->GetName() : TEXT("없음 (코드 기본값)"),
			static_cast<int32>(ActiveGrade), *DescribeEffects(ActiveSpec));
	}

	// 이펙트가 없어도 신호는 보낸다. 사운드나 화면 흔들림은 여기에 붙는다.
	OnBurst.Broadcast(Location, Color);
	++FiredBursts;
}

void UFF_FinaleShowSubsystem::FireVolley()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Origin = ResolveLaunchOrigin();

	/* 마지막 차례는 남은 발수만큼만 쏜다. 총 발수를 넘기면 등급 차이가 흐려진다.

	   예약만 되고 아직 안 터진 발도 뺀다. 빼지 않으면 줄지어 나가는 패턴 뒤에서
	   같은 발수를 또 쏴서 총 발수를 넘긴다.
	*/
	const int32 Remaining = GetRemainingShells();

	/* 남은 발수를 넘겨 고른다. 모양이 있는 패턴은 발수가 모자라면 모양이 깨진다.

	   참조가 아니라 복사로 든다. 아래에서 쏘면 OnBurst 가 울리는데, 받는 쪽이 연출을
	   멈추거나 다시 틀면 ActiveSpec 이 바뀌어 참조가 사라진 배열을 가리킨다.
	*/
	const FFF_FinaleVolley Volley = PickVolley(Remaining);

	const int32 Requested = FMath::Max(
		FMath::Max(1, ActiveSpec.SimultaneousShells), Volley.ShellCount);
	const int32 ShellCount = FMath::Min(Requested, Remaining);

	if (ShellCount <= 0)
	{
		++FiredVolleys;
		return;
	}

	/* 지나치게 많으면 알린다. 막지는 않는다.

	   화면을 보면서 계속 올리게 될 값이라 상한을 박으면 늘리고 싶을 때 걸린다.
	   대신 느려졌을 때 왜 느린지는 보여야 한다.
	*/
	if (!bWarnedHighShellCount && ShellCount > 64)
	{
		bWarnedHighShellCount = true;
		UE_LOG(LogFestaFiestaFinale, Warning,
			TEXT("[피날레] 한 차례에 %d 발이다. 프레임이 떨어지면 이 값부터 볼 것."), ShellCount);
	}

	// 모양이 있는 패턴은 색도 이펙트도 섞이면 모양이 안 보인다.
	const FLinearColor UniformColor = PickColor(ColorCursor++);
	UNiagaraSystem* UniformEffect = ActiveSpec.PickEffect(ShowRandom.GetUnsignedInt());

	/* 이번 차례에 쏠 자리를 먼저 다 구한다.

	   예전에는 루프 안에서 발마다 PickBurstLocation 을 불렀다. 그 함수는 부를 때마다
	   흩뿌림 반경 안에서 중심과 높이를 새로 뽑으므로, 모양이 있는 패턴은 발마다
	   다른 중심에서 오프셋돼 뭉개졌다. S 등급은 흩뿌림 반경 9000 에 모양 크기 10000
	   이라 원이 원으로 보이지 않았다.

	   그래서 모양이 있는 패턴은 중심을 여기서 한 번만 뽑아 넘긴다.
	*/
	TArray<FVector> ShellLocations;

	if (Volley.HasFixedLayout())
	{
		FFF_FinaleLayoutContext Layout;
		Layout.Center = PickBurstLocation(Origin);
		Layout.FacingDirection = ResolveAudienceDirection(Layout.Center);
		Layout.ShellCount = ShellCount;

		// 바닥면 모양만 돌린다. 수직면은 돌리면 관객에게서 비껴서 읽히지 않는다.
		Layout.GroundYawDegrees = ShowRandom.FRandRange(0.f, 360.f);

		Volley.BuildShellLayout(Layout, ShellLocations);

		/* 모양을 못 그렸으면(애셋이 비었거나 깨짐) 이번 차례는 흩뿌린다.

		   예전에는 이 차례를 건너뛰었다. 끝을 남은 발수로 정하게 바꾼 뒤로는 건너뛰면
		   발수가 안 줄어 같은 볼리가 계속 뽑히고 하늘이 빈다.
		*/
		if (ShellLocations.Num() == 0)
		{
			ShellLocations.Reserve(ShellCount);
			for (int32 Shell = 0; Shell < ShellCount; ++Shell)
			{
				ShellLocations.Add(PickBurstLocation(Origin));
			}
		}
	}
	else
	{
		// 단발과 동시는 흩뿌린다. 발마다 따로 뽑는 것이 여기서는 의도다.
		ShellLocations.Reserve(ShellCount);
		for (int32 Shell = 0; Shell < ShellCount; ++Shell)
		{
			ShellLocations.Add(PickBurstLocation(Origin));
		}
	}

	for (int32 Shell = 0; Shell < ShellLocations.Num(); ++Shell)
	{
		const FVector& Location = ShellLocations[Shell];

		const FLinearColor Color = Volley.bUniformColor ? UniformColor : PickColor(ColorCursor++);

		// 모양을 그리는 패턴은 이펙트도 하나로 맞춘다. 종류가 섞이면 모양이 안 보인다.
		UNiagaraSystem* Effect = Volley.HasFixedLayout()
			? UniformEffect
			: ActiveSpec.PickEffect(ShowRandom.GetUnsignedInt());

		if (Volley.ShellDelaySeconds > 0.f && Shell > 0)
		{
			// 줄지어 흐르는 느낌은 이 지연에서 나온다. 한꺼번에 쏘면 그냥 모양일 뿐이다.
			FPendingShell& Pending = PendingShells.AddDefaulted_GetRef();
			Pending.Location = Location;
			Pending.Color = Color;
			Pending.DelayRemaining = Volley.ShellDelaySeconds * static_cast<float>(Shell);
			Pending.Effect = Effect;
			continue;
		}

		SpawnShell(Location, Color, Effect);

		// OnBurst 를 받은 쪽이 연출을 멈췄다. 남은 발을 쏘지 않는다.
		if (!bShowInProgress)
		{
			return;
		}
	}

	++FiredVolleys;
}

int32 UFF_FinaleShowSubsystem::GetRemainingShells() const
{
	return FMath::Max(0, ActiveSpec.BurstCount - FiredBursts - PendingShells.Num());
}

FString UFF_FinaleShowSubsystem::DescribeEffects(const FFF_FinaleGradeSpec& Spec)
{
	/* 칸 수와 채워진 수를 따로 보여준다.

	   `3종` 이라고만 찍으면 그중 None 이 섞여 있어도 모른다. 칸은 채웠다고 믿는데
	   가끔 안 터지는 원인이 대개 그 빈 칸이다.
	*/
	const int32 Slots = Spec.BurstEffects.Num();
	const int32 Assigned = Spec.CountAssignedEffects();

	if (Slots == 0)
	{
		return TEXT("없음");
	}
	if (Assigned == Slots)
	{
		return FString::Printf(TEXT("%d종"), Assigned);
	}
	return FString::Printf(TEXT("%d칸 중 %d종 (빈 칸 %d)"), Slots, Assigned, Slots - Assigned);
}
