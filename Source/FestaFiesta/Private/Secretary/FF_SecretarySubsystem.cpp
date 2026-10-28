// Copyright Epic Games, Inc. All Rights Reserved.

#include "Secretary/FF_SecretarySubsystem.h"

#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSWorldSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Incident/FF_IncidentSubsystem.h"
#include "Incident/FF_IncidentTypes.h"
#include "Kismet/GameplayStatics.h"
#include "NPC/Metrics/FF_NpcNeedMetricsSubsystem.h"
#include "Save/FF_SaveSubsystem.h"
#include "Secretary/FF_SecretarySettings.h"
#include "Secretary/FF_SecretaryTipWidget.h"
#include "Staff/FF_StaffFacilitySubsystem.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "Tutorial/FF_TutorialSubsystem.h"
#include "UI/Core/FF_UIFlowSubsystem.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogFestaFiestaSecretary, Log, All);

namespace
{
	// 설치가 끝나 축제장에 있는 시설인가. 계획 · 철거된 것은 세지 않는다.
	bool IsSecretaryInstalled(const EFFZSFacilityState State)
	{
		return State == EFFZSFacilityState::Validated || State == EFFZSFacilityState::Active
			|| State == EFFZSFacilityState::Closed || State == EFFZSFacilityState::MovePending;
	}

	// 대사창이 다른 대사를 재생 중이면 기다릴 최대 횟수(2초씩). 그 뒤로는 그날 브리핑을 접는다.
	constexpr int32 MaxSecretaryBriefingRetries = 30;
	constexpr float SecretaryBriefingRetrySeconds = 2.f;

	EFF_SecretaryWeakness ToSecretaryWeakness(const EFF_DailyComplaintAxis Axis)
	{
		switch (Axis)
		{
		case EFF_DailyComplaintAxis::Need: return EFF_SecretaryWeakness::Need;
		case EFF_DailyComplaintAxis::Wait: return EFF_SecretaryWeakness::Wait;
		case EFF_DailyComplaintAxis::Density: return EFF_SecretaryWeakness::Density;
		case EFF_DailyComplaintAxis::Preference: return EFF_SecretaryWeakness::Preference;
		default: return EFF_SecretaryWeakness::None;
		}
	}
}

bool UFF_SecretarySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

UFF_SecretarySubsystem* UFF_SecretarySubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UFF_SecretarySubsystem>() : nullptr;
}

void UFF_SecretarySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	for (double& Time : KindLastShown)
	{
		Time = -1.0e9;
	}
	BindStage();

	// 레벨이 뜰 때 이미 그 단계일 수 있다(단계 바뀜 신호가 레벨보다 먼저 왔다). 그 경우도 한 번 챙긴다.
	if (const UFF_FestivalStageSubsystem* Stage = BoundStage.Get())
	{
		if (Stage->CurrentStage == EFF_FestivalStage::MorningCheck)
		{
			ScheduleBriefing(false);
		}
		else if (Stage->CurrentStage == EFF_FestivalStage::Setup && Stage->CurrentDay == 1)
		{
			ScheduleBriefing(true);
		}
	}
}

void UFF_SecretarySubsystem::Deinitialize()
{
	UnbindStage();
	if (TipWidget)
	{
		TipWidget->RemoveFromParent();
		TipWidget = nullptr;
	}
	Super::Deinitialize();
}

void UFF_SecretarySubsystem::BindStage()
{
	const UWorld* World = GetWorld();
	UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
	UFF_FestivalStageSubsystem* Stage = Game ? Game->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
	if (!Stage || BoundStage.Get() == Stage)
	{
		return;
	}
	UnbindStage();
	Stage->OnStageChanged.AddDynamic(this, &ThisClass::HandleStageChanged);
	BoundStage = Stage;
}

void UFF_SecretarySubsystem::UnbindStage()
{
	if (UFF_FestivalStageSubsystem* Stage = BoundStage.Get())
	{
		Stage->OnStageChanged.RemoveDynamic(this, &ThisClass::HandleStageChanged);
	}
	BoundStage = nullptr;
}

bool UFF_SecretarySubsystem::IsSecretaryWorld() const
{
	if (!BoundStage.IsValid())
	{
		return false;
	}
	// 타이틀 화면 월드에도 단계 시스템이 1일차 준비로 떠 있다. 거기서는 말하지 않는다.
	// 남의 시험 레벨(대기 상태)에서는 말한다.
	const UFF_UIFlowSubsystem* Flow = UFF_UIFlowSubsystem::Get(this);
	if (Flow && Flow->GetMode() == EFF_UIFlowMode::Frontend)
	{
		return false;
	}
	const UFF_TutorialSubsystem* Tutorial = UFF_TutorialSubsystem::Get(this);
	return !(Tutorial && Tutorial->IsTutorialLevel());
}

bool UFF_SecretarySubsystem::IsDialogueBusy() const
{
	const UFF_TutorialSubsystem* Tutorial = UFF_TutorialSubsystem::Get(this);
	return Tutorial && Tutorial->IsDialogueActive();
}

double UFF_SecretarySubsystem::NowSeconds() const
{
	// 실제 시간. 배속을 올려도 훈수 간격은 그대로다.
	const UWorld* World = GetWorld();
	return World ? World->GetRealTimeSeconds() : 0.0;
}

void UFF_SecretarySubsystem::HandleStageChanged(const EFF_FestivalStage PreviousStage, const EFF_FestivalStage NewStage)
{
	(void)PreviousStage;

	// 운영이 끝나면 떠 있던 훈수도 내린다. 결산 화면 위에 남지 않게.
	if (NewStage != EFF_FestivalStage::Live && TipWidget)
	{
		TipWidget->Dismiss();
	}

	switch (NewStage)
	{
	case EFF_FestivalStage::Live:
		ResetDaySamples();
		RememberBuiltCategories();
		CheckCountdown = 0.f;
		LiveStartedTime = NowSeconds();
		break;
	case EFF_FestivalStage::MorningCheck:
		ScheduleBriefing(false);
		break;
	case EFF_FestivalStage::Setup:
		if (BoundStage.IsValid() && BoundStage->CurrentDay == 1)
		{
			ScheduleBriefing(true);
		}
		break;
	case EFF_FestivalStage::Report:
		if (const UFF_SecretarySettings* Settings = UFF_SecretarySettings::Get(); Settings && Settings->bEnableReportReaction)
		{
			ReportReactionRetries = 0;
			ReportReactionCountdown = Settings->ReportReactionDelaySeconds;
		}
		break;
	default:
		break;
	}
}

void UFF_SecretarySubsystem::TickReportReaction(const float DeltaTime)
{
	if (ReportReactionCountdown < 0.f)
	{
		return;
	}
	ReportReactionCountdown -= DeltaTime;
	if (ReportReactionCountdown > 0.f)
	{
		return;
	}

	if (PlayReportReaction())
	{
		ReportReactionCountdown = -1.f;
		return;
	}

	// 결산 표시값은 원본 정산이 끝나야 선다. 잠깐씩 기다리고, 끝내 없으면 그날은 접는다.
	ReportReactionCountdown = (++ReportReactionRetries < 15) ? 1.f : -1.f;
}

bool UFF_SecretarySubsystem::PlayReportReaction(const bool bForce)
{
	const UFF_FestivalStageSubsystem* Stage = BoundStage.Get();
	const UWorld* World = GetWorld();
	UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
	const UFF_FestivalViewModelSubsystem* ViewModel = Game ? Game->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;
	const FFF_DailyReportView* Report = ViewModel ? ViewModel->GetDailyReport() : nullptr;
	if (!Stage || !Report)
	{
		return false;
	}
	if (ReactedReportDay == Report->Day && !bForce)
	{
		return true;
	}
	ReactedReportDay = Report->Day;

	// 결산 화면 · 다음 날 아침 브리핑과 같은 만족도 값을 본다.
	const int32 Satisfaction = Report->bHasSatisfaction ? Report->ScoreSatisfaction : INDEX_NONE;
	const EFF_SecretaryDayTier Tier = FFSecretaryAdvisor::ClassifyDay(Satisfaction);

	EFF_SecretaryTrustTier Trust = EFF_SecretaryTrustTier::Low;
	bool bTrustJustRoseHigh = false;
	if (UFF_SaveSubsystem* Save = UFF_SaveSubsystem::Get(this))
	{
		Trust = FFSecretaryAdvisor::TrustTierOf(
			Save->AddSecretaryTrust(FFSecretaryAdvisor::TrustDeltaForDay(Tier), TEXT("결산")));
		bTrustJustRoseHigh = Trust == EFF_SecretaryTrustTier::High && Save->MarkSecretaryTrustReachedHigh();
	}

	const FText Text = ResolvePlayerName(
		FFSecretaryAdvisor::BuildReportReaction(Tier, Trust, bTrustJustRoseHigh, Stage->IsFinalDay()));
	const UFF_SecretarySettings* Settings = UFF_SecretarySettings::Get();
	ShowTip(Text, Settings ? Settings->ReportReactionVisibleSeconds : 10.f);
	UE_LOG(LogFestaFiestaSecretary, Log, TEXT("[비서] %d일차 결산 반응(만족도 %d · 단계 %d · 신뢰 %d%s): %s"),
		Report->Day, Satisfaction, static_cast<int32>(Tier), static_cast<int32>(Trust),
		bTrustJustRoseHigh ? TEXT(" · 처음 높음") : TEXT(""), *Text.ToString());
	return true;
}

FText UFF_SecretarySubsystem::ResolvePlayerName(const FText& Text) const
{
	const FString Source = Text.ToString();
	if (!Source.Contains(TEXT("{이름}")))
	{
		return Text;
	}
	const UFF_TutorialSubsystem* Tutorial = UFF_TutorialSubsystem::Get(this);
	const FString Name = Tutorial ? Tutorial->GetPlayerDisplayName() : FString(TEXT("담당자"));
	return FText::FromString(Source.Replace(TEXT("{이름}"), *Name));
}

void UFF_SecretarySubsystem::Tick(const float DeltaTime)
{
	if (!IsSecretaryWorld())
	{
		return;
	}
	TickBriefing(DeltaTime);
	TickReportReaction(DeltaTime);
	if (BoundStage.IsValid() && BoundStage->CurrentStage == EFF_FestivalStage::Live)
	{
		TickLive(DeltaTime);
	}
}

// ── 아침 브리핑 ──────────────────────────────────────────────────────────

void UFF_SecretarySubsystem::ScheduleBriefing(const bool bFirstDay)
{
	const UFF_SecretarySettings* Settings = UFF_SecretarySettings::Get();
	if (!Settings || !Settings->bEnableMorningBriefing || (bFirstDay && bGreetedFirstDay))
	{
		return;
	}
	bBriefingFirstDay = bFirstDay;
	BriefingRetries = 0;
	BriefingCountdown = Settings->BriefingDelaySeconds;
}

void UFF_SecretarySubsystem::TickBriefing(const float DeltaTime)
{
	if (BriefingCountdown < 0.f)
	{
		return;
	}
	BriefingCountdown -= DeltaTime;
	if (BriefingCountdown > 0.f)
	{
		return;
	}

	if (PlayMorningBriefing(bBriefingFirstDay))
	{
		BriefingCountdown = -1.f;
		return;
	}

	// 다른 대사(튜토리얼 등)가 진행 중이면 조금 뒤 다시. 끝내 못 띄우면 그날은 접는다.
	if (++BriefingRetries >= MaxSecretaryBriefingRetries)
	{
		UE_LOG(LogFestaFiestaSecretary, Log, TEXT("[비서] 대사창이 계속 쓰이고 있어 오늘 브리핑을 접는다."));
		BriefingCountdown = -1.f;
		return;
	}
	BriefingCountdown = SecretaryBriefingRetrySeconds;
}

bool UFF_SecretarySubsystem::PlayMorningBriefing(const bool bFirstDay)
{
	UFF_TutorialSubsystem* Tutorial = UFF_TutorialSubsystem::Get(this);
	const UFF_SecretarySettings* Settings = UFF_SecretarySettings::Get();
	if (!Tutorial || !Settings || Tutorial->IsDialogueActive())
	{
		return false;
	}

	const TArray<FText> Texts = bFirstDay
		? FFSecretaryAdvisor::BuildFirstDayGreeting()
		: FFSecretaryAdvisor::BuildMorningBriefing(GatherBriefingInputs());

	TArray<FFF_DialogueLine> Lines;
	for (int32 Index = 0; Index < Texts.Num(); ++Index)
	{
		FFF_DialogueLine& Line = Lines.AddDefaulted_GetRef();
		Line.Label = FName(*FString::Printf(TEXT("SecretaryBriefing_%d"), Index));
		Line.Kind = EFF_DialogueLineKind::Speech;
		Line.Speaker = FText::FromString(Settings->SpeakerName);
		Line.Text = Texts[Index];
		// 대사 상자만 눌러 넘긴다. 아침에 축제장을 둘러보는 조작은 막지 않는다.
		Line.ClickArea = EFF_DialogueClickArea::BoxOnly;
		Line.bSkippable = true;
	}

	const bool bStarted = Tutorial->PlayLines(Lines,
		bFirstDay ? FName(TEXT("SecretaryFirstDay")) : FName(TEXT("SecretaryMorning")), EFF_TutorialScope::Briefing);
	if (bStarted && bFirstDay)
	{
		bGreetedFirstDay = true;
	}
	UE_LOG(LogFestaFiestaSecretary, Log, TEXT("[비서] %s 브리핑 %s (%d줄)"),
		bFirstDay ? TEXT("첫날") : TEXT("아침"), bStarted ? TEXT("시작") : TEXT("못 띄움"), Lines.Num());
	return bStarted;
}

FFF_SecretaryBriefingInputs UFF_SecretarySubsystem::GatherBriefingInputs() const
{
	FFF_SecretaryBriefingInputs Inputs;
	const UWorld* World = GetWorld();
	UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
	if (!World || !Game)
	{
		return Inputs;
	}
	if (const UFF_FestivalStageSubsystem* Stage = BoundStage.Get())
	{
		Inputs.Day = Stage->CurrentDay;
	}
	if (const UFF_SaveSubsystem* Save = UFF_SaveSubsystem::Get(this))
	{
		Inputs.Trust = FFSecretaryAdvisor::TrustTierOf(Save->GetSecretaryTrust());
	}
	// 어제 만족도 · 아쉬운 점 · 매출 1등은 어제 결산이 이미 정했다. 결산 화면과 다른 말을 하지 않도록 그대로 옮긴다.
	// 이어하기로 아침에 들어와 어제 결산을 못 봤으면 비워 둔다(브리핑이 그 줄들을 빼고 말한다).
	const UFF_FestivalViewModelSubsystem* ViewModel = Game->GetSubsystem<UFF_FestivalViewModelSubsystem>();
	if (const FFF_DailyReportView* Report = ViewModel ? ViewModel->GetPreviousDailyReport() : nullptr)
	{
		if (Report->bHasSatisfaction)
		{
			Inputs.YesterdaySatisfaction = Report->ScoreSatisfaction;
		}
		Inputs.bJudgedYesterday = Report->Complaint.bJudged;
		Inputs.Weakness = ToSecretaryWeakness(Report->Complaint.Axis);
		Inputs.bHasWorstNeed = Report->Complaint.bHasNeedShortage;
		Inputs.WorstNeed = Report->Complaint.NeedShortage;
		Inputs.TopRevenueFacility = Report->TopFacilityName;
		Inputs.TopRevenueGold = Report->TopFacilityGold;
	}
	Inputs.WorstWaitFacility = DayWorstWaitFacility;

	// 시설: 화장실 수 · 한 번도 안 놓아 본 분류.
	const UFFZSWorldSubsystem* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
	const FFFZSReadModel Model = Zones ? Zones->BuildReadModel() : FFFZSReadModel();
	TSet<EFFZSFacilityCategory> Built = BuiltCategories;
	int32 ToiletCount = 0;
	for (const FFFZSFacilityReadModel& Facility : Model.Facilities)
	{
		if (!IsSecretaryInstalled(Facility.State))
		{
			continue;
		}
		Built.Add(Facility.Category);
		ToiletCount += Facility.Category == EFFZSFacilityCategory::Toilet ? 1 : 0;
	}
	if (Zones)
	{
		Inputs.ToiletCount = ToiletCount;
		for (const EFFZSFacilityCategory Category : FFSecretaryAdvisor::GetIntroductionOrder())
		{
			if (!Built.Contains(Category))
			{
				Inputs.UnbuiltCategories.Add(Category);
			}
		}
	}
	return Inputs;
}

// ── 운영 중 훈수 ─────────────────────────────────────────────────────────

void UFF_SecretarySubsystem::ResetDaySamples()
{
	DayWorstWaitFacility = FText::GetEmpty();
	DayWorstWaitSeconds = 0.f;
}

void UFF_SecretarySubsystem::RememberBuiltCategories()
{
	const UWorld* World = GetWorld();
	const UFFZSWorldSubsystem* Zones = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	if (!Zones)
	{
		return;
	}
	for (const FFFZSFacilityReadModel& Facility : Zones->BuildReadModel().Facilities)
	{
		if (IsSecretaryInstalled(Facility.State))
		{
			BuiltCategories.Add(Facility.Category);
		}
	}
}

void UFF_SecretarySubsystem::SampleDay()
{
	const UWorld* World = GetWorld();
	if (const UFFZSWorldSubsystem* Zones = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr)
	{
		for (const FFFZSFacilityReadModel& Facility : Zones->BuildReadModel().Facilities)
		{
			if (!IsSecretaryInstalled(Facility.State))
			{
				continue;
			}
			BuiltCategories.Add(Facility.Category);
			if (Facility.AverageCompletedWaitSeconds > DayWorstWaitSeconds && !Facility.DisplayName.IsEmpty())
			{
				DayWorstWaitSeconds = Facility.AverageCompletedWaitSeconds;
				DayWorstWaitFacility = Facility.DisplayName;
			}
		}
	}
}

FFF_SecretaryLiveSignals UFF_SecretarySubsystem::GatherLiveSignals() const
{
	FFF_SecretaryLiveSignals Signals;
	const UWorld* World = GetWorld();
	UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
	const UFF_SecretarySettings* Settings = UFF_SecretarySettings::Get();
	if (!World || !Game || !Settings)
	{
		return Signals;
	}

	// 스태프가 안 간 채 오래된 사건. 시간은 사건 표시와 같은 축제 분으로 잰다.
	if (const UFF_IncidentSubsystem* Incidents = World->GetSubsystem<UFF_IncidentSubsystem>())
	{
		float NowMinutes = 0.f;
		const UFF_FestivalStageSubsystem* Stage = BoundStage.Get();
		if (Stage && Stage->TryGetLiveElapsedMinutes(NowMinutes))
		{
			TArray<FFF_IncidentHandle> Handles;
			Incidents->GetActiveHandle(Handles);
			for (const FFF_IncidentHandle& Handle : Handles)
			{
				FFF_IncidentView View;
				if (Incidents->GetIncidentView(Handle, View) && View.ResponseStage == EFF_IncidentResponseStage::None
					&& NowMinutes - View.OccurredAtMinutes >= Settings->IdleIncidentMinutes)
				{
					++Signals.IdleIncidentCount;
				}
			}
		}
	}

	if (const UFF_StaffFacilitySubsystem* Staff = World->GetSubsystem<UFF_StaffFacilitySubsystem>())
	{
		Signals.DispatchableStaffCount = Staff->GetDispatchableStaffCount();
	}

	if (const UFF_NpcNeedMetricsSubsystem* Needs = Game->GetSubsystem<UFF_NpcNeedMetricsSubsystem>())
	{
		Signals.bNeedSampled = Needs->GetSampleCount() > 0;
		for (int32 Index = 0; Index < static_cast<int32>(EFF_NeedType::Count); ++Index)
		{
			Signals.NeedUnmetPercent[Index] = Needs->GetNeedUnmetRatio(static_cast<EFF_NeedType>(Index));
		}
	}

	const UFF_CrowdSimulationSubsystem* Crowd = World->GetSubsystem<UFF_CrowdSimulationSubsystem>();
	for (int32 Index = 0; Index < static_cast<int32>(EFF_NeedType::Count); ++Index)
	{
		Signals.NeedCoverage[Index] = Crowd ? Crowd->GetNeedCoverage(static_cast<EFF_NeedType>(Index)) : 1.f;
	}

	if (const UFFZSWorldSubsystem* Zones = World->GetSubsystem<UFFZSWorldSubsystem>())
	{
		for (const FFFZSFacilityReadModel& Facility : Zones->BuildReadModel().Facilities)
		{
			if (!IsSecretaryInstalled(Facility.State))
			{
				continue;
			}
			Signals.ToiletCount += Facility.Category == EFFZSFacilityCategory::Toilet ? 1 : 0;
			if (Facility.WaitingCount > Signals.LongestQueue)
			{
				Signals.LongestQueue = Facility.WaitingCount;
				Signals.LongestQueueFacility = Facility.DisplayName;
			}
		}
	}

	if (Crowd)
	{
		Signals.CongestedCellCount = Crowd->GetCongestedCellCount();
	}
	return Signals;
}

void UFF_SecretarySubsystem::TickLive(const float DeltaTime)
{
	const UFF_SecretarySettings* Settings = UFF_SecretarySettings::Get();
	if (!Settings)
	{
		return;
	}

	CheckCountdown -= DeltaTime;
	if (CheckCountdown > 0.f)
	{
		return;
	}
	CheckCountdown = Settings->CheckIntervalSeconds;

	// 하루치 값은 훈수를 끈 날에도 모은다. 다음 아침 브리핑에 쓴다.
	SampleDay();

	const UWorld* World = GetWorld();
	if (!Settings->bEnableLiveTips || IsDialogueBusy() || (World && UGameplayStatics::IsGamePaused(World)))
	{
		return;
	}

	const double Now = NowSeconds();
	if (Now - LastTipTime < Settings->MinSecondsBetweenTips || (TipWidget && TipWidget->IsShowing()))
	{
		return;
	}

	FFF_SecretaryThresholds Thresholds;
	Thresholds.LongQueueCount = Settings->LongQueueCount;
	Thresholds.CrowdedCellCount = Settings->CrowdedCellCount;
	Thresholds.UnmetNeedPercent = Settings->UnmetNeedPercent;

	FFF_SecretaryTip Tip = FFSecretaryAdvisor::ChooseLiveTip(GatherLiveSignals(), Thresholds,
		[this, Now, Settings](const EFF_SecretaryTipKind Kind)
		{
			return Now - KindLastShown[static_cast<int32>(Kind)] < Settings->SameKindRestSeconds;
		});

	// 할 상황이 없고 한동안 조용했으면 조작 팁 하나. 운영이 막 시작됐을 때도 조용한 시간을 채운 뒤에.
	// 같은 얘기를 이 판에서 상황 훈수로 이미 했으면 그 팁은 건너뛴다.
	int32 GeneralIndex = INDEX_NONE;
	const bool bQuietLongEnough = Now - FMath::Max(LastTipTime, LiveStartedTime) >= Settings->GeneralTipQuietSeconds;
	if (!Tip.IsValid() && Settings->bEnableGeneralTips && bQuietLongEnough)
	{
		GeneralIndex = FFSecretaryAdvisor::FindNextGeneralTip(NextGeneralTip,
			[this](const EFF_SecretaryTipKind Kind) { return KindLastShown[static_cast<int32>(Kind)] > -1.0e8; });
		if (GeneralIndex != INDEX_NONE)
		{
			Tip.Kind = EFF_SecretaryTipKind::General;
			Tip.Text = FFSecretaryAdvisor::GetGeneralTips()[GeneralIndex].Text;
		}
	}

	if (!Tip.IsValid() || !ShowTip(Tip.Text))
	{
		return;
	}
	if (GeneralIndex != INDEX_NONE)
	{
		NextGeneralTip = GeneralIndex + 1;
	}
	LastTipTime = Now;
	KindLastShown[static_cast<int32>(Tip.Kind)] = Now;
	UE_LOG(LogFestaFiestaSecretary, Log, TEXT("[비서] 훈수(%d): %s"), static_cast<int32>(Tip.Kind), *Tip.Text.ToString());
}

UFF_SecretaryTipWidget* UFF_SecretarySubsystem::EnsureTipWidget()
{
	if (TipWidget)
	{
		return TipWidget;
	}
	const UWorld* World = GetWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	if (!Controller)
	{
		return nullptr;
	}
	TipWidget = CreateWidget<UFF_SecretaryTipWidget>(Controller, UFF_SecretaryTipWidget::StaticClass());
	if (TipWidget)
	{
		const UFF_SecretarySettings* Settings = UFF_SecretarySettings::Get();
		TipWidget->AddToViewport(Settings ? Settings->BubbleZOrder : 90);
	}
	return TipWidget;
}

bool UFF_SecretarySubsystem::ShowTip(const FText& Text, const float VisibleOverride)
{
	UFF_SecretaryTipWidget* Widget = Text.IsEmpty() ? nullptr : EnsureTipWidget();
	if (!Widget)
	{
		return false;
	}
	Widget->ShowTip(Text, VisibleOverride);
	return true;
}

// ── 개발 명령 ────────────────────────────────────────────────────────────

#if !UE_BUILD_SHIPPING
namespace
{
	FAutoConsoleCommandWithWorldAndArgs SecretaryTipCommand(
		TEXT("FF.Secretary.Tip"),
		TEXT("비서 말풍선을 바로 띄운다. FF.Secretary.Tip <문장> — 문장을 빼면 지금 상황으로 고른 훈수(쉬는 시간 무시)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UFF_SecretarySubsystem* Secretary = World ? World->GetSubsystem<UFF_SecretarySubsystem>() : nullptr;
			if (!Secretary)
			{
				return;
			}
			if (Args.Num() > 0)
			{
				Secretary->ShowTip(FText::FromString(FString::Join(Args, TEXT(" "))));
				return;
			}
			const UFF_SecretarySettings* Settings = UFF_SecretarySettings::Get();
			FFF_SecretaryThresholds Thresholds;
			if (Settings)
			{
				Thresholds.LongQueueCount = Settings->LongQueueCount;
				Thresholds.CrowdedCellCount = Settings->CrowdedCellCount;
				Thresholds.UnmetNeedPercent = Settings->UnmetNeedPercent;
			}
			const FFF_SecretaryLiveSignals Signals = Secretary->GatherLiveSignals();
			const FFF_SecretaryTip Tip = FFSecretaryAdvisor::ChooseLiveTip(Signals, Thresholds,
				[](EFF_SecretaryTipKind) { return false; });
			UE_LOG(LogFestaFiestaSecretary, Display,
				TEXT("[비서] 방치 사건 %d · 출동 가능 %d · 화장실 %d · 최장 줄 %d(%s) · 혼잡 %d · 필요 표본 %s → %s"),
				Signals.IdleIncidentCount, Signals.DispatchableStaffCount, Signals.ToiletCount, Signals.LongestQueue,
				*Signals.LongestQueueFacility.ToString(), Signals.CongestedCellCount,
				Signals.bNeedSampled ? TEXT("있음") : TEXT("없음"),
				Tip.IsValid() ? *Tip.Text.ToString() : TEXT("할 말 없음"));
			if (Tip.IsValid())
			{
				Secretary->ShowTip(Tip.Text);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs SecretaryReportCommand(
		TEXT("FF.Secretary.Report"),
		TEXT("결산 한마디를 지금 띄운다(그날 한 번 규칙 무시). 결산 단계에서 쓴다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UFF_SecretarySubsystem* Secretary = World ? World->GetSubsystem<UFF_SecretarySubsystem>() : nullptr)
			{
				UE_LOG(LogFestaFiestaSecretary, Display, TEXT("[비서] 결산 한마디 %s"),
					Secretary->PlayReportReaction(true) ? TEXT("띄움") : TEXT("결산 값 없음"));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs SecretaryTrustCommand(
		TEXT("FF.Secretary.Trust"),
		TEXT("비서 신뢰도를 보거나 맞춘다. FF.Secretary.Trust 75 — 숫자를 빼면 지금 값만 찍는다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UFF_SaveSubsystem* Save = World ? UFF_SaveSubsystem::Get(World) : nullptr;
			if (!Save)
			{
				return;
			}
			if (Args.Num() > 0)
			{
				Save->AddSecretaryTrust(FCString::Atoi(*Args[0]) - Save->GetSecretaryTrust(), TEXT("개발 명령"));
			}
			UE_LOG(LogFestaFiestaSecretary, Display, TEXT("[비서] 신뢰도 %d (단계 %d)"),
				Save->GetSecretaryTrust(), static_cast<int32>(FFSecretaryAdvisor::TrustTierOf(Save->GetSecretaryTrust())));
		}));

	FAutoConsoleCommandWithWorldAndArgs SecretaryBriefingCommand(
		TEXT("FF.Secretary.Briefing"),
		TEXT("비서 아침 브리핑을 지금 띄운다. FF.Secretary.Briefing first — 첫날 인사."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UFF_SecretarySubsystem* Secretary = World ? World->GetSubsystem<UFF_SecretarySubsystem>() : nullptr)
			{
				Secretary->PlayMorningBriefing(Args.Num() > 0 && Args[0].Equals(TEXT("first"), ESearchCase::IgnoreCase));
			}
		}));
}
#endif
