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

	// 세 명령은 한 인스턴스가 함께 갖는다. 다른 월드가 이미 갖고 있으면 통째로 넘긴다.
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
					Spec.BurstEffect.IsNull() ? TEXT("없음") : TEXT("지정됨"));
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

	// 연출을 새로 시작하면 검사도 다시 한다. 데이터 애셋을 고치고 재생했을 때
	// 바뀐 이펙트를 그대로 통과시키면 안 된다.
	VerifiedEffect = nullptr;

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

	// 이펙트가 바뀌었을 때만 검사한다. 첫 발에서 한 번 돌고 끝난다.
	if (Effect && VerifiedEffect.Get() != Effect)
	{
		VerifyEffectParameters(*Effect);
		VerifiedEffect = Effect;
	}

	for (int32 Shell = 0; Shell < ShellCount; ++Shell)
	{
		const FVector BurstLocation = PickBurstLocation(Origin);
		const FLinearColor Color = PickColor(ColorCursor++);

		if (Effect)
		{
			// 컴포넌트를 받아 파라미터를 넣는다. SpawnSystemAtLocation 자체는 값을 못 넘긴다.
			UNiagaraComponent* Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World, Effect, BurstLocation);

			if (Spawned)
			{
				// 이펙트에 같은 이름의 User 파라미터가 없으면 조용히 무시된다.
				// 크래시가 안 나는 대신 "왜 색이 안 바뀌지" 로 나타나므로, 이름은 상수로만 쓴다.
				Spawned->SetColorParameter(FFF_FinaleParams::BurstColor, Color);
				Spawned->SetFloatParameter(FFF_FinaleParams::BurstScale, ActiveSpec.BurstScale);
				Spawned->SetFloatParameter(FFF_FinaleParams::TrailLength, ActiveSpec.TrailLength);
			}
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
