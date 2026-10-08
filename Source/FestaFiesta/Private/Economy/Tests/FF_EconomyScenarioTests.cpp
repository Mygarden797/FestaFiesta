// 실제 릴리스 맵에서 3일 축제를 통째로 돌려 경제·평가 결과를 잰다. 밸런싱 비교표를 만드는 도구다.
//
// 플레이어의 결정은 시나리오별로 정해진 전략으로 대신 내린다:
//   - 시설: 배치 슬롯(Tools/Scenarios/*.layout.json 으로 만든 것)을 Day1 셋업에 한 번에 깔고 설치비를 낸다
//   - 스태프: 필수 안내소 배정 + 추가 고용 N명을 안내소에 배정
//   - 유료 프로그램: 하루 N개까지 편성 / 홍보·입장료: 축제 시작 시 / 피날레: Day3 아침
//   - 사건: 뜨는 대로 첫 번째 선택지로 출동
// 2·3일차에 시설을 더 짓거나 옮기지는 않는다.
//
// 판정이 아니라 계측이다. 결과는 AddInfo 와 Saved/EconomyScenarios/results.csv 한 줄로 남긴다.
// -FFScenario 없으면 건너뛴다.
//   UnrealEditor-Cmd.exe <uproject> -ExecCmds="Automation RunTests FestaFiesta.Scenario.Economy;Quit"
//       -unattended -nopause -nosplash -NullRHI -NoSound -FFScenario
//       -FFEconLayout=econ_std -FFEconPromotion=Influencer -FFEconFee=5 -FFEconFinale=S
//       -FFEconPrograms=1 -FFEconHire=2 -FFEconTimeScale=4 -FFEconTag=std_infl_S

#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CrowdSystem/Systems/FF_CrowdDecisionSubsystem.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "CrowdSystem/Types/FF_SimulationSettings.h"
#include "DataTables/FF_UtilityCoefficientRow.h"
#include "Economy/FF_FacilityCostResolver.h"
#include "Economy/FF_ProgramSchedulingSubsystem.h"
#include "Editor.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FileHelpers.h"
#include "Festival/FF_FestivalBuzzSubsystem.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Festival/FF_RequiredFacilitiesSubsystem.h"
#include "HAL/PlatformFileManager.h"
#include "Incident/FF_IncidentSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Staff/FF_StaffFacilitySubsystem.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "Zone/FF_ZoneLayoutSaveGame.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"
#include "Zone/FF_ZoneSystemStageBridgeSubsystem.h"
#include "Queue/FF_QueueTypes.h"

namespace FFEconScenario
{
	constexpr double WallClockBudgetSeconds = 60.0 * 40.0;
	const TCHAR* const ProgramIds[] = { TEXT("Program_SandArtWorkshop"), TEXT("Program_SunsetYoga") };

	struct FOptions
	{
		FString Layout = TEXT("econ_std");
		FString Tag;
		EFF_PromotionGrade Promotion = EFF_PromotionGrade::None;
		int32 Fee = INDEX_NONE;
		EFF_FinaleGrade Finale = EFF_FinaleGrade::None;
		int32 ProgramsPerDay = 0;
		int32 Hire = 0;
		float TimeScale = 4.f;
		// 실험용. 0 이하면 손대지 않는다
		float WalkScale = 0.f;
		float StayShare = 0.f;

		void Parse()
		{
			const TCHAR* Cmd = FCommandLine::Get();
			FParse::Value(Cmd, TEXT("FFEconLayout="), Layout);
			// 배치마다 기본 전략이 다르다. 명령줄로 덮어쓸 수 있다
			if (Layout.Contains(TEXT("min"))) { ProgramsPerDay = 0; Hire = 0; }
			else if (Layout.Contains(TEXT("full"))) { ProgramsPerDay = 2; Hire = 4; }
			else { ProgramsPerDay = 1; Hire = 2; }

			FString Text;
			if (FParse::Value(Cmd, TEXT("FFEconPromotion="), Text))
			{
				const int64 Value = StaticEnum<EFF_PromotionGrade>()->GetValueByNameString(Text);
				if (Value != INDEX_NONE) { Promotion = static_cast<EFF_PromotionGrade>(Value); }
			}
			if (FParse::Value(Cmd, TEXT("FFEconFinale="), Text))
			{
				const int64 Value = StaticEnum<EFF_FinaleGrade>()->GetValueByNameString(Text);
				if (Value != INDEX_NONE) { Finale = static_cast<EFF_FinaleGrade>(Value); }
			}
			FParse::Value(Cmd, TEXT("FFEconFee="), Fee);
			FParse::Value(Cmd, TEXT("FFEconPrograms="), ProgramsPerDay);
			FParse::Value(Cmd, TEXT("FFEconHire="), Hire);
			FParse::Value(Cmd, TEXT("FFEconTimeScale="), TimeScale);
			FParse::Value(Cmd, TEXT("FFEconWalkScale="), WalkScale);
			FParse::Value(Cmd, TEXT("FFEconStayShare="), StayShare);
			if (!FParse::Value(Cmd, TEXT("FFEconTag="), Tag))
			{
				Tag = FString::Printf(TEXT("%s_%s_%s"), *Layout,
					*StaticEnum<EFF_PromotionGrade>()->GetNameStringByValue(static_cast<int64>(Promotion)),
					*StaticEnum<EFF_FinaleGrade>()->GetNameStringByValue(static_cast<int64>(Finale)));
			}
		}
	};

	struct FDayResult
	{
		int32 Admitted = 0;
		int32 Admission = 0;
		int32 Commission = 0;
		int32 ProgramFee = 0;
		int32 OtherEarn = 0;
		int32 Spend = 0;
		int32 GoldAtEnd = 0;
		float BuzzAtEnd = 0.f;
		int32 PeakActive = 0;
	};

	bool HasSession()
	{
		if (GEditor && GEditor->PlayWorld) return true;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE) return true;
		}
		return false;
	}

	const UFFZSFacilityDefinition* LoadDefinition(FName FacilityDefinitionId)
	{
		const FPrimaryAssetId AssetId(UFFZSFacilityDefinition::GetFacilityDefinitionAssetType(), FacilityDefinitionId);
		UAssetManager& Manager = UAssetManager::Get();
		if (UObject* Loaded = Manager.GetPrimaryAssetObject(AssetId))
		{
			return Cast<UFFZSFacilityDefinition>(Loaded);
		}
		const FSoftObjectPath Path = Manager.GetPrimaryAssetPath(AssetId);
		return Path.IsValid() ? Cast<UFFZSFacilityDefinition>(Path.TryLoad()) : nullptr;
	}
}

class FFFEconScenarioRun final : public IAutomationLatentCommand
{
public:
	explicit FFFEconScenarioRun(FAutomationTestBase* InTest) : Test(InTest) { Options.Parse(); }

	virtual bool Update() override
	{
		if (StartedAt < 0.0) StartedAt = FPlatformTime::Seconds();
		if (FPlatformTime::Seconds() - StartedAt > FFEconScenario::WallClockBudgetSeconds)
		{
			Test->AddError(TEXT("시간 예산(40분)을 넘겼다. 단계가 어디서 멈췄는지 로그를 볼 것."));
			return true;
		}

		UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
		if (!World)
		{
			if (FPlatformTime::Seconds() - StartedAt < 30.0) return false;
			Test->AddError(TEXT("PIE 월드가 30초 안에 뜨지 않았다."));
			return true;
		}
		UGameInstance* Game = World->GetGameInstance();
		Stage = Game ? Game->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
		Progression = Game ? Game->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
		if (!Stage || !Progression)
		{
			Test->AddError(TEXT("Stage·Progression 서브시스템이 없다."));
			return true;
		}

		if (!bArranged)
		{
			bArranged = true;
			return !ArrangeFestival(World, Game);
		}

		SampleFrame(World);
		const int32 Day = Stage->CurrentDay;
		switch (Stage->CurrentStage)
		{
		case EFF_FestivalStage::Live:
			if (!FMath::IsNearlyEqual(Stage->TimeScale, Options.TimeScale)) { Stage->SetTimeScale(Options.TimeScale); }
			DispatchIncidents(World);
			break;
		case EFF_FestivalStage::Report:
			RecordDay(Day);
			if (Day < 3) { Stage->AdvanceToNextDay(); }
			break;
		case EFF_FestivalStage::MorningCheck:
			if (PreparedDay != Day)
			{
				PreparedDay = Day;
				PrepareMorning(World, Day);
				GoLive(World);
			}
			break;
		case EFF_FestivalStage::Closing:
			if (Day >= 3)
			{
				RecordDay(Day);
				Stage->FinishFestival();
				Report(World);
				return true;
			}
			SkipIfPossible();
			break;
		default:
			// 피날레·노을 전환 등은 넘길 수 있으면 넘긴다. 피날레 보너스는 구매 등급으로 정해져 연출을 기다릴 필요가 없다.
			SkipIfPossible();
			break;
		}
		return false;
	}

private:
	bool ArrangeFestival(UWorld* World, UGameInstance* Game)
	{
		UFFZSWorldSubsystem* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
		UFF_ZoneLayoutSaveGame* Layout = Cast<UFF_ZoneLayoutSaveGame>(UGameplayStatics::LoadGameFromSlot(Options.Layout, 0));
		if (!Zones || !Layout)
		{
			Test->AddError(FString::Printf(TEXT("배치 슬롯 '%s' 이 없다. Tools/Scenarios/auto_layout.py 와 prepare_zone_layout.py 로 만들 것."), *Options.Layout));
			return false;
		}
		TArray<FText> RestoreErrors;
		if (!Zones->RestoreWorldSnapshot(Layout->Snapshot, RestoreErrors))
		{
			for (const FText& Error : RestoreErrors) { Test->AddError(FString::Printf(TEXT("배치 복원 오류: %s"), *Error.ToString())); }
			return false;
		}

		// 슬롯 복원은 저장 불러오기 경로라 설치비를 받지 않는다. 플레이어가 지었을 때처럼 여기서 낸다.
		const UFF_FestivalDefinition* Festival = Progression->GetDefinition();
		const UDataTable* CostTable = Festival ? Festival->FacilityCostTable.Get() : nullptr;
		const FFFZSReadModel ReadModel = Zones->BuildReadModel();
		for (const FFFZSFacilityReadModel& Facility : ReadModel.Facilities)
		{
			++FacilityCount;
			const UFFZSFacilityDefinition* Definition = FFEconScenario::LoadDefinition(Facility.FacilityDefinitionId);
			int32 Cost = 0;
			FName Row;
			FString Reason;
			if (Definition && CostTable && FFF_FacilityCostResolver::FindInstallCost(Definition->Spec, CostTable, Cost, Row, Reason))
			{
				InstallCost += Cost;
			}
			else
			{
				Test->AddWarning(FString::Printf(TEXT("설치비를 못 찾았다: %s (%s)"), *Facility.FacilityDefinitionId.ToString(), *Reason));
			}
		}
		if (!Progression->TrySpend(InstallCost, EFF_SpendReason::FacilityInstall))
		{
			Test->AddWarning(FString::Printf(TEXT("설치비 %d 를 낼 돈이 없다(보유 %d). 설치비 없이 진행한다 — 결과를 그대로 믿지 말 것."),
				InstallCost, Progression->GetGold()));
			bInstallUnpaid = true;
		}

		if (Options.Promotion != EFF_PromotionGrade::None
			&& Progression->TryPurchasePromotion(Options.Promotion) != EFF_PromotionPurchaseResult::Success)
		{
			Test->AddWarning(TEXT("홍보를 사지 못했다."));
		}
		if (Options.Fee != INDEX_NONE && Progression->TrySetAdmissionFee(Options.Fee) != EFF_AdmissionFeeResult::Success)
		{
			Test->AddWarning(FString::Printf(TEXT("입장료 %d 를 적용하지 못했다."), Options.Fee));
		}
		if (Options.Hire > 0 && !Progression->TryHireStaff(Options.Hire))
		{
			Test->AddWarning(FString::Printf(TEXT("스태프 %d명을 고용하지 못했다."), Options.Hire));
		}

		ApplyCrowdExperiment(World);
		AssignStaff(World);
		PrepareMorning(World, 1);
		GoLive(World);
		PreparedDay = 1;

		Test->AddInfo(FString::Printf(TEXT("[설정] %s · 배치 %s 시설 %d개 설치비 %d · 홍보 %s · 입장료 %d · 피날레 %s · 프로그램 %d/일 · 고용 %d · 배속 %.0f"),
			*Options.Tag, *Options.Layout, FacilityCount, InstallCost,
			*UEnum::GetValueAsString(Options.Promotion), Progression->GetAdmissionFee(),
			*UEnum::GetValueAsString(Options.Finale), Options.ProgramsPerDay, Options.Hire, Options.TimeScale));
		return true;
	}

	// 걷는 속도·체류 비율을 바꿔 보는 실험. 계수 표는 메모리에서만 바꾸고 저장하지 않는다.
	void ApplyCrowdExperiment(UWorld* World)
	{
		if (Options.WalkScale > 0.f)
		{
			if (UFF_CrowdSimulationSubsystem* Crowd = World->GetSubsystem<UFF_CrowdSimulationSubsystem>())
			{
				Crowd->GuestWalkSpeedScale = Options.WalkScale;
			}
		}
		// -FFEconCrowd=AssignedSlotArrivalRadiusUU:10,EnRouteReevaluateIntervalScale:2 — 군중 서브시스템의 float 속성
		FString CrowdText;
		if (FParse::Value(FCommandLine::Get(), TEXT("FFEconCrowd="), CrowdText, /*bShouldStopOnSeparator=*/false))
		{
			UFF_CrowdSimulationSubsystem* Crowd = World->GetSubsystem<UFF_CrowdSimulationSubsystem>();
			TArray<FString> Pairs;
			CrowdText.ParseIntoArray(Pairs, TEXT(","));
			for (const FString& Pair : Pairs)
			{
				FString Name, Value;
				FFloatProperty* Property = Pair.Split(TEXT(":"), &Name, &Value) && Crowd
					? FindFProperty<FFloatProperty>(UFF_CrowdSimulationSubsystem::StaticClass(), FName(*Name.TrimStartAndEnd()))
					: nullptr;
				if (Property)
				{
					Property->SetPropertyValue_InContainer(Crowd, FCString::Atof(*Value));
					Test->AddInfo(FString::Printf(TEXT("[실험] 군중 %s = %s"), *Name, *Value));
				}
				else
				{
					Test->AddWarning(FString::Printf(TEXT("군중 속성 '%s' 을 찾지 못했다."), *Pair));
				}
			}
		}

		TArray<TPair<FName, float>> Overrides;
		if (Options.StayShare > 0.f)
		{
			Overrides.Emplace(TEXT("stay_duration_share_of_live_stage_mean"), Options.StayShare);
		}
		// -FFEconCoef=wait_threshold_minutes:30,distance_cost_cap:2
		FString CoefText;
		if (FParse::Value(FCommandLine::Get(), TEXT("FFEconCoef="), CoefText, /*bShouldStopOnSeparator=*/false))
		{
			TArray<FString> Pairs;
			CoefText.ParseIntoArray(Pairs, TEXT(","));
			for (const FString& Pair : Pairs)
			{
				FString Name, Value;
				if (Pair.Split(TEXT(":"), &Name, &Value))
				{
					Overrides.Emplace(FName(*Name.TrimStartAndEnd()), FCString::Atof(*Value));
				}
			}
		}
		const UFF_SimulationSettings* Settings = UFF_SimulationSettings::Get();
		UDataTable* Table = Settings ? Settings->UtilityCoefficientTable.LoadSynchronous() : nullptr;
		FString Applied;
		for (const TPair<FName, float>& Override : Overrides)
		{
			if (FFF_UtilityCoefficientRow* Row = Table ? Table->FindRow<FFF_UtilityCoefficientRow>(Override.Key, TEXT("EconScenario")) : nullptr)
			{
				Row->DefaultValue = Override.Value;
				Applied += FString::Printf(TEXT("%s=%g "), *Override.Key.ToString(), Override.Value);
			}
			else
			{
				Test->AddWarning(FString::Printf(TEXT("계수 '%s' 가 표에 없다."), *Override.Key.ToString()));
			}
		}
		Test->AddInfo(FString::Printf(TEXT("[실험] 보행 속도 배수 %.2f (0 = 기본값) · 계수 %s"), Options.WalkScale, *Applied));
	}

	// 필수 시설의 배정 부족분을 채우고, 남은 대기 스태프는 안내소에 붙인다(사건 출동은 안내소에서 나간다).
	void AssignStaff(UWorld* World)
	{
		const UFF_RequiredFacilitiesSubsystem* Required = World->GetSubsystem<UFF_RequiredFacilitiesSubsystem>();
		UFF_StaffFacilitySubsystem* Staffing = World->GetSubsystem<UFF_StaffFacilitySubsystem>();
		const UFFZSWorldSubsystem* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
		if (!Staffing || !Zones) return;
		const FFFZSReadModel Installed = Zones->BuildReadModel();
		if (Required)
		{
			for (const FFF_RequiredFacilityRow& Row : Required->GetRequiredFacilitiesStatus().Facilities)
			{
				int32 Missing = Row.MissingStaffCount;
				for (const FFFZSFacilityReadModel& Facility : Installed.Facilities)
				{
					if (Missing <= 0) break;
					if (Facility.FacilityDefinitionId != Row.FacilityDefinitionId) continue;
					while (Missing > 0 && Staffing->TryAssignAvailableStaff(Facility.FacilityInstanceId)) { --Missing; }
				}
			}
		}
		for (const FFFZSFacilityReadModel& Facility : Installed.Facilities)
		{
			if (Facility.Category != EFFZSFacilityCategory::Info) continue;
			while (Staffing->TryAssignAvailableStaff(Facility.FacilityInstanceId)) {}
		}
	}

	void PrepareMorning(UWorld* World, int32 Day)
	{
		if (Day == 3 && Options.Finale != EFF_FinaleGrade::None)
		{
			const EFF_FinalePurchaseResult Result = Progression->TryPurchaseFinale(Options.Finale);
			if (Result != EFF_FinalePurchaseResult::Success)
			{
				Test->AddWarning(FString::Printf(TEXT("피날레 %s 를 사지 못했다: %s (보유 %d)"),
					*UEnum::GetValueAsString(Options.Finale), *UEnum::GetValueAsString(Result), Progression->GetGold()));
			}
		}

		// 편성은 개장 전(운영 전 시설)에만 된다
		UFF_ProgramSchedulingSubsystem* Scheduler = World->GetSubsystem<UFF_ProgramSchedulingSubsystem>();
		const UFFZSWorldSubsystem* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
		int32 Scheduled = 0;
		if (Scheduler && Zones && Options.ProgramsPerDay > 0)
		{
			const FFFZSReadModel ReadModel = Zones->BuildReadModel();
			for (const TCHAR* ProgramId : FFEconScenario::ProgramIds)
			{
				for (const FFFZSFacilityReadModel& Facility : ReadModel.Facilities)
				{
					if (Scheduled >= Options.ProgramsPerDay) break;
					if (Scheduler->TryScheduleProgramAtFacility(ProgramId, Facility.FacilityInstanceId) == EFF_PaidProgramScheduleResult::Success)
					{
						++Scheduled;
						break;
					}
				}
			}
		}
		ProgramsScheduled += Scheduled;
		Test->AddInfo(FString::Printf(TEXT("[Day %d 아침] Gold %d · 예상 방문객 %d · 프로그램 편성 %d"),
			Day, Progression->GetGold(), Progression->GetExpectedVisitors(), Scheduled));
	}

	void GoLive(UWorld* World)
	{
		if (UFF_ZoneSystemStageBridgeSubsystem* StageBridge = World->GetSubsystem<UFF_ZoneSystemStageBridgeSubsystem>())
		{
			StageBridge->OpenEligibleFacilities(false);
		}
		FText Blocking;
		if (!Stage->CanTransitionTo(EFF_FestivalStage::Live, Blocking))
		{
			Test->AddError(FString::Printf(TEXT("Day %d Live 전환이 거부됐다: %s"), Stage->CurrentDay, *Blocking.ToString()));
			return;
		}
		Stage->RequestTransition(EFF_FestivalStage::Live);
	}

	void SkipIfPossible()
	{
		if (Stage->IsCurrentStageSkippable()) { Stage->SkipCurrentStage(); }
	}

	void DispatchIncidents(UWorld* World)
	{
		UFF_IncidentSubsystem* Incidents = World->GetSubsystem<UFF_IncidentSubsystem>();
		if (!Incidents) return;
		TArray<FFF_IncidentHandle> Handles;
		Incidents->GetActiveHandle(Handles);
		for (const FFF_IncidentHandle& Handle : Handles)
		{
			const uint64 Key = (static_cast<uint64>(Handle.Index) << 32) | static_cast<uint32>(Handle.Serial);
			if (Attempted.Contains(Key)) continue;
			Attempted.Add(Key);
			++IncidentsSeen;
			EFF_IncidentResolveResult Result = EFF_IncidentResolveResult::DispatchFailed;
			if (Incidents->ResolveIncident(Handle, 0, INDEX_NONE, &Result)) { ++IncidentsDispatched; }
		}
	}

	void SampleFrame(UWorld* World)
	{
		const double FrameMs = FApp::GetDeltaTime() * 1000.0;
		++FrameCount;
		FrameMsSum += FrameMs;
		PeakFrameMs = FMath::Max(PeakFrameMs, FrameMs);
		if (const UFF_CrowdSimulationSubsystem* Crowd = World->GetSubsystem<UFF_CrowdSimulationSubsystem>())
		{
			DayPeakActive = FMath::Max(DayPeakActive, Crowd->GetActiveAgentCount());
			PeakActive = FMath::Max(PeakActive, Crowd->GetActiveAgentCount());
			PoolSize = Crowd->GetAgentPoolSize();
			DroppedSimSeconds = Crowd->GetDroppedSimSeconds();
		}
	}

	void RecordDay(int32 Day)
	{
		if (Days.Contains(Day)) return;
		FFEconScenario::FDayResult& Result = Days.Add(Day);
		Result.Admitted = Progression->GetAdmittedVisitorsToday();
		for (const FFF_LedgerRow& Row : Progression->GetEarnRowsForDay(Day))
		{
			if (Row.ReasonId == TEXT("AdmissionFee")) Result.Admission += Row.Amount;
			else if (Row.ReasonId == TEXT("FacilityCommission")) Result.Commission += Row.Amount;
			else if (Row.ReasonId == TEXT("ProgramFee")) Result.ProgramFee += Row.Amount;
			else Result.OtherEarn += Row.Amount;
		}
		for (const FFF_LedgerRow& Row : Progression->GetSpendRowsForDay(Day)) { Result.Spend += Row.Amount; }
		Result.GoldAtEnd = Progression->GetGold();
		Result.PeakActive = DayPeakActive;
		DayPeakActive = 0;
		if (const UFF_FestivalBuzzSubsystem* Buzz = Progression->GetGameInstance()->GetSubsystem<UFF_FestivalBuzzSubsystem>())
		{
			Result.BuzzAtEnd = Buzz->GetBuzz();
		}
		Test->AddInfo(FString::Printf(
			TEXT("[Day %d] 입장 %d · 매출 입장료 %d / 부스 수수료 %d / 체험비 %d / 기타 %d · 지출 %d · 마감 Gold %d · 화제성 %.0f · 최고 동시 %d"),
			Day, Result.Admitted, Result.Admission, Result.Commission, Result.ProgramFee, Result.OtherEarn,
			Result.Spend, Result.GoldAtEnd, Result.BuzzAtEnd, Result.PeakActive));
		LogUsageFunnel(Day);
	}

	// 부스가 왜 안 쓰이는지 가르기 위한 깔때기. 전부 누적값이다.
	// 목적지 선택 → 줄 서기 시도 → 수락/거절 → 인내 초과 이탈 → 서비스 완료(시설 종류별)
	void LogUsageFunnel(int32 Day)
	{
		UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
		if (!World) return;
		if (const UFF_CrowdDecisionSubsystem* Decision = World->GetSubsystem<UFF_CrowdDecisionSubsystem>())
		{
			const FFF_CrowdDecisionStats Stats = Decision->GetDecisionStats();
			Test->AddInfo(FString::Printf(TEXT("[Day %d 결정] 요청 %d · 시설 선택 %d · no_choice %d · 후보없음 %d · Zone밖 %d · 실패감점 %d"),
				Day, Stats.Requests, Stats.Chosen, Stats.NoChoice, Stats.NoCandidate, Stats.OutsideZone, Stats.FailedDestinationPenalized));
			Test->AddInfo(FString::Printf(TEXT("[Day %d 결정] Zone 밖→근처 Zone 보정 %d · 퇴장 체류소진 %d · 퇴장 갈곳없음 %d"),
				Day, Stats.OutsideZoneUsedNearest, Decision->GetLeaveByStayExpiredCount(), Decision->GetLeaveByNoChoicePatienceCount()));
		}
		if (const UFF_CrowdSimulationSubsystem* Crowd = World->GetSubsystem<UFF_CrowdSimulationSubsystem>())
		{
			Test->AddInfo(FString::Printf(TEXT("[Day %d 줄] 시도 %d · 수락 %d · 거절 %d · 인내초과 이탈 %d"),
				Day, Crowd->GetQueueJoinAttemptCount(), Crowd->GetQueueJoinAcceptedCount(),
				Crowd->GetQueueJoinRejectedCount(), Crowd->GetQueuePatienceAbandons()));
			Test->AddInfo(FString::Printf(TEXT("[Day %d 줄] 칸 도착 %d · 묶임 해제 %d"),
				Day, Crowd->GetQueueArrivalNotifyCount(), Crowd->GetQueueHoldTimeoutCount()));
		}
		const UFFZSWorldSubsystem* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
		const UFF_ZoneSystemQueueBridgeSubsystem* QueueBridge = World->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>();
		if (!Zones || !QueueBridge) return;
		TMap<FString, int32> CompletedByCategory;
		TMap<FString, int32> WaitingByCategory;
		TArray<FString> PerBooth;
		for (const FFFZSFacilityReadModel& Facility : Zones->BuildReadModel().Facilities)
		{
			const FFF_QueueState Queue = QueueBridge->GetZoneFacilityQueueState(Facility.FacilityInstanceId);
			const FString Category = StaticEnum<EFFZSFacilityCategory>()->GetNameStringByValue(static_cast<int64>(Facility.Category));
			CompletedByCategory.FindOrAdd(Category) += Queue.CompletedPartyCount;
			WaitingByCategory.FindOrAdd(Category) += Queue.WaitingPartyCount;
			if (Facility.Category == EFFZSFacilityCategory::Booth)
			{
				PerBooth.Add(FString::Printf(TEXT("%s 완료%d 대기%d 정원%d 속도%.2f"), *Facility.DisplayName.ToString(),
					Queue.CompletedPartyCount, Queue.WaitingPartyCount, Queue.MaxCapacityParties, Queue.ServiceRate));
			}
		}
		FString Summary;
		for (const TPair<FString, int32>& Pair : CompletedByCategory)
		{
			Summary += FString::Printf(TEXT("%s 완료%d/대기%d  "), *Pair.Key, Pair.Value, WaitingByCategory.FindRef(Pair.Key));
		}
		Test->AddInfo(FString::Printf(TEXT("[Day %d 시설] %s"), Day, *Summary));
		Test->AddInfo(FString::Printf(TEXT("[Day %d 부스] %s"), Day, *FString::Join(PerBooth, TEXT(" | "))));
	}

	void Report(UWorld* World)
	{
		const FFF_FestivalEvaluation Eval = Progression->GetFestivalEvaluation();
		const TArray<int32> Satisfaction = Progression->GetDailySatisfactionHistory();
		auto Sat = [&Satisfaction](int32 Index) { return Satisfaction.IsValidIndex(Index) ? Satisfaction[Index] : -1; };
		const FString Grade = StaticEnum<EFF_FestivalEvaluationGrade>()->GetNameStringByValue(static_cast<int64>(Eval.Grade));

		Test->AddInfo(FString::Printf(TEXT("[만족도] Day1 %d · Day2 %d · Day3 %d"), Sat(0), Sat(1), Sat(2)));
		Test->AddInfo(FString::Printf(TEXT("[평가] %s %d점 — 만족도 %d · 방문객 %d · 재무 %d · 화제성 %d (최종 Gold %d)"),
			*Grade, Eval.TotalScore, Eval.SatisfactionScore, Eval.VisitorScore, Eval.FinanceScore, Eval.BuzzScore, Progression->GetGold()));
		Test->AddInfo(FString::Printf(TEXT("[사건] 발생 %d · 출동 %d / [성능] 최고 동시 %d / 풀 %d · 프레임 평균 %.1fms 최대 %.1fms · 버려진 시뮬 %.1f초"),
			IncidentsSeen, IncidentsDispatched, PeakActive, PoolSize,
			FrameCount > 0 ? FrameMsSum / FrameCount : 0.0, PeakFrameMs, DroppedSimSeconds));

		// 시나리오마다 한 줄. 여러 판을 돌린 뒤 표로 비교한다
		const FString Path = FPaths::ProjectSavedDir() / TEXT("EconomyScenarios/results.csv");
		IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
		const bool bNew = !Files.FileExists(*Path);
		FString Line;
		if (bNew)
		{
			Line += TEXT("tag,layout,facilities,install_cost,install_unpaid,promotion,fee,finale,programs_per_day,hire,timescale,")
				TEXT("d1_admitted,d2_admitted,d3_admitted,admission,commission,program_fee,other_earn,spend,final_gold,")
				TEXT("sat1,sat2,sat3,buzz,score_sat,score_visit,score_fin,score_buzz,total,grade,")
				TEXT("incidents,dispatched,peak_active,pool,frame_avg_ms,frame_max_ms,dropped_sim_s\n");
		}
		int32 Admission = 0, Commission = 0, ProgramFee = 0, Other = 0, Spend = 0;
		for (const TPair<int32, FFEconScenario::FDayResult>& Pair : Days)
		{
			Admission += Pair.Value.Admission; Commission += Pair.Value.Commission;
			ProgramFee += Pair.Value.ProgramFee; Other += Pair.Value.OtherEarn; Spend += Pair.Value.Spend;
		}
		auto Admitted = [this](int32 Day) { const FFEconScenario::FDayResult* R = Days.Find(Day); return R ? R->Admitted : -1; };
		const UFF_FestivalBuzzSubsystem* Buzz = Progression->GetGameInstance()->GetSubsystem<UFF_FestivalBuzzSubsystem>();
		Line += FString::Printf(TEXT("%s,%s,%d,%d,%d,%s,%d,%s,%d,%d,%.0f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.0f,%d,%d,%d,%d,%d,%s,%d,%d,%d,%d,%.1f,%.1f,%.1f\n"),
			*Options.Tag, *Options.Layout, FacilityCount, InstallCost, bInstallUnpaid ? 1 : 0,
			*StaticEnum<EFF_PromotionGrade>()->GetNameStringByValue(static_cast<int64>(Options.Promotion)),
			Progression->GetAdmissionFee(),
			*StaticEnum<EFF_FinaleGrade>()->GetNameStringByValue(static_cast<int64>(Options.Finale)),
			Options.ProgramsPerDay, Options.Hire, Options.TimeScale,
			Admitted(1), Admitted(2), Admitted(3), Admission, Commission, ProgramFee, Other, Spend, Progression->GetGold(),
			Sat(0), Sat(1), Sat(2), Buzz ? Buzz->GetBuzz() : 0.f,
			Eval.SatisfactionScore, Eval.VisitorScore, Eval.FinanceScore, Eval.BuzzScore, Eval.TotalScore, *Grade,
			IncidentsSeen, IncidentsDispatched, PeakActive, PoolSize,
			FrameCount > 0 ? FrameMsSum / FrameCount : 0.0, PeakFrameMs, DroppedSimSeconds);
		FFileHelper::SaveStringToFile(Line, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
			&IFileManager::Get(), FILEWRITE_Append);
		Test->AddInfo(FString::Printf(TEXT("결과 한 줄을 %s 에 덧붙였다."), *Path));
		(void)World;
	}

	FAutomationTestBase* Test;
	FFEconScenario::FOptions Options;
	UFF_FestivalStageSubsystem* Stage = nullptr;
	UFF_FestivalProgressionSubsystem* Progression = nullptr;
	double StartedAt = -1.0;
	bool bArranged = false;
	bool bInstallUnpaid = false;
	int32 PreparedDay = 0;
	int32 FacilityCount = 0;
	int32 InstallCost = 0;
	int32 ProgramsScheduled = 0;
	int32 IncidentsSeen = 0;
	int32 IncidentsDispatched = 0;
	TSet<uint64> Attempted;
	TMap<int32, FFEconScenario::FDayResult> Days;
	int32 DayPeakActive = 0;
	int32 PeakActive = 0;
	int32 PoolSize = 0;
	float DroppedSimSeconds = 0.f;
	int32 FrameCount = 0;
	double FrameMsSum = 0.0;
	double PeakFrameMs = 0.0;
};

class FFFEconScenarioWaitForTeardown final : public IAutomationLatentCommand
{
public:
	virtual bool Update() override
	{
		if (StartedAt < 0.0) StartedAt = FPlatformTime::Seconds();
		return !FFEconScenario::HasSession() || FPlatformTime::Seconds() - StartedAt > 60.0;
	}

private:
	double StartedAt = -1.0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFEconomyThreeDayScenario,
	"FestaFiesta.Scenario.Economy.ThreeDays",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFFEconomyThreeDayScenario::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (!FParse::Param(FCommandLine::Get(), TEXT("FFScenario")))
	{
		AddInfo(TEXT("건너뜀 — 3일을 실제로 돌리는 계측이다. -FFScenario 로 실행할 것."));
		return true;
	}
	if (FFEconScenario::HasSession())
	{
		AddError(TEXT("앞선 PIE 세션이 남아 있어 시나리오를 시작할 수 없다."));
		return false;
	}

	UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/FestaFiesta/Maps/Test_Main"));
	if (!TestNotNull(TEXT("릴리스 맵이 저장 없이 열린다"), World)) return false;

	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FFFEconScenarioRun(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FFFEconScenarioWaitForTeardown());
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
