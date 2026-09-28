// 실제 릴리스 맵에서 Day1 하루를 통째로 돌려 메인 공연 집결과 유료 프로그램 복귀를 실측한다.
//
// 왜 유닛 테스트로 안 되는가: 여기서 재는 값(좌석까지 남은 거리, 도착 인원)은 전부 맵 의존이다.
// 시설 배치·NavMesh 우회·방문객 입장 곡선이 결과를 정한다. 합성 월드에서는 그 숫자가 안 나온다.
//
// 이 테스트는 판정이 아니라 **계측**이다. 단언은 "공연이 열렸는가" 수준만 두고 나머지는
// AddInfo 로 남긴다 — 숫자를 보고 계수를 정하는 것이 목적이라, 임계를 박으면 튜닝할 때마다 깨진다.
//
// 기본 스위트에서는 즉시 건너뛴다(-FFScenario 없으면 AddInfo 만 남기고 반환).
// Day1 한 판이 실시간 2분이라 매 실행에 붙이면 반복 주기가 망가진다.
// ★ StressFilter 를 쓰면 `Automation RunTests` 목록에서 아예 빠져 실행할 방법이 없다.
//   UnrealEditor-Cmd.exe <uproject> -ExecCmds="Automation RunTests FestaFiesta.Scenario;Quit"
//       -unattended -nopause -nosplash -NullRHI -NoSound -FFScenario

#include "Misc/App.h"
#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "Economy/FF_ProgramSchedulingSubsystem.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FileHelpers.h"
#include "Festival/FF_FestivalBuzzSubsystem.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "NPC/Party/FF_ProgramReservationSubsystem.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Zone/FF_ZoneLayoutSaveGame.h"
#include "Zone/FF_ZoneSystemStageBridgeSubsystem.h"

namespace FFMainStageScenario
{
	// 유료 프로그램은 플레이어가 UI 로 사는 것이라 자동 실행에서는 대신 편성해 준다.
	// 어느 시설이 되는지는 맵 배치에 달렸으므로 스케줄러에게 물어 첫 번째 가능한 곳을 쓴다.
	const TCHAR* ProgramId = TEXT("Program_SandArtWorkshop");

	// 배치는 `FF.Zone.SaveLayout` 으로 사람이 한 번 만들어 둔 것을 쓴다. Test_Main 은 빈 땅으로
	// 시작하고, 여기서 재는 값은 전부 시설 위치에 달렸다 — 코드로 심은 배치를 재면 의미가 없다.
	const TCHAR* LayoutSlot = TEXT("day1");

	// Live 는 그날 마지막 이벤트 시각까지다. Day1 은 240초이고 공연은 180~210 이다.
	constexpr float ObserveUntilLiveSeconds = 215.f;
	constexpr double WallClockBudgetSeconds = 600.0;

	bool HasSession()
	{
		if (GEditor && GEditor->PlayWorld) return true;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE) return true;
		}
		return false;
	}

	UWorld* PlayWorld()
	{
		return GEditor ? GEditor->PlayWorld : nullptr;
	}
}

// 축제를 Live 로 올리고 유료 프로그램 하나를 편성한다. 플레이어의 클릭을 대신하는 단계다.
class FFFScenarioArrangeDay final : public IAutomationLatentCommand
{
public:
	explicit FFFScenarioArrangeDay(FAutomationTestBase* InTest) : Test(InTest) {}

	virtual bool Update() override
	{
		UWorld* World = FFMainStageScenario::PlayWorld();
		if (!World)
		{
			if (StartedAt < 0.0) StartedAt = FPlatformTime::Seconds();
			if (FPlatformTime::Seconds() - StartedAt < 30.0) return false;
			Test->AddError(TEXT("PIE 월드가 30초 안에 뜨지 않았다."));
			return true;
		}

		UGameInstance* Game = World->GetGameInstance();
		UFF_FestivalStageSubsystem* Stage = Game ? Game->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
		// 편성 스케줄러는 World 스코프다(GameInstance 아님) — 시설이 월드에 사니까.
		UFF_ProgramSchedulingSubsystem* Scheduler = World->GetSubsystem<UFF_ProgramSchedulingSubsystem>();
		const UFFZSWorldSubsystem* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
		UFF_ZoneSystemStageBridgeSubsystem* StageBridge = World->GetSubsystem<UFF_ZoneSystemStageBridgeSubsystem>();
		if (!Test->TestNotNull(TEXT("Stage 서브시스템"), Stage)
			|| !Test->TestNotNull(TEXT("프로그램 스케줄러"), Scheduler)
			|| !Test->TestNotNull(TEXT("ZoneSystem"), Zones))
		{
			return true;
		}

		// 저장해 둔 배치를 먼저 붓는다.
		if (UFF_ZoneLayoutSaveGame* Layout = Cast<UFF_ZoneLayoutSaveGame>(
			UGameplayStatics::LoadGameFromSlot(FFMainStageScenario::LayoutSlot, 0)))
		{
			UFFZSWorldSubsystem* MutableZones = World->GetSubsystem<UFFZSWorldSubsystem>();
			TArray<FText> RestoreErrors;
			if (MutableZones && MutableZones->RestoreWorldSnapshot(Layout->Snapshot, RestoreErrors))
			{
				Test->AddInfo(FString::Printf(TEXT("배치 슬롯 '%s' 복원 — 시설 %d개"),
					FFMainStageScenario::LayoutSlot, Layout->Snapshot.Facilities.Num()));
			}
			else
			{
				for (const FText& Error : RestoreErrors)
				{
					Test->AddError(FString::Printf(TEXT("배치 복원 오류: %s"), *Error.ToString()));
				}
			}
		}
		else
		{
			Test->AddError(FString::Printf(
				TEXT("배치 슬롯 '%s' 이 없다. PIE 에서 축제장을 짓고 `FF.Zone.SaveLayout %s` 를 한 번 실행할 것."),
				FFMainStageScenario::LayoutSlot, FFMainStageScenario::LayoutSlot));
			return true;
		}

		ApplyVisitorConditions(Game);

		// 편성 판정에 쓰는 목록이라 개장 전에 읽는다.
		const FFFZSReadModel ReadModel = Zones->BuildReadModel();
		Test->AddInfo(FString::Printf(TEXT("설치된 시설 %d개"), ReadModel.Facilities.Num()));

		// 어느 시설에 왜 거부됐는지를 남긴다. 사유 없이 "편성 실패"만 찍으면 배치 문제인지
		// 카탈로그 문제인지 단계 문제인지 못 가른다 — 그걸 손으로 찾느라 한 판을 버린다.
		int32 Scheduled = 0;
		TMap<FString, int32> RejectionsByReason;
		for (const FFFZSFacilityReadModel& Facility : ReadModel.Facilities)
		{
			const EFF_PaidProgramScheduleResult Result = Scheduler->TryScheduleProgramAtFacility(
				FFMainStageScenario::ProgramId, Facility.FacilityInstanceId);
			if (Result == EFF_PaidProgramScheduleResult::Success)
			{
				Test->AddInfo(FString::Printf(TEXT("유료 프로그램 편성: %s"),
					*Facility.FacilityDefinitionId.ToString()));
				++Scheduled;
				break;
			}
			++RejectionsByReason.FindOrAdd(
				UFF_ProgramSchedulingSubsystem::GetResultText(Result).ToString());
		}
		if (Scheduled == 0)
		{
			for (const TPair<FString, int32>& Pair : RejectionsByReason)
			{
				Test->AddWarning(FString::Printf(TEXT("편성 거부 %d건: %s"), Pair.Value, *Pair.Key));
			}
			Test->AddWarning(FString::Printf(
				TEXT("'%s' 을 받을 시설이 없어 유료 프로그램 없이 관측한다."),
				FFMainStageScenario::ProgramId));
		}


		// 후보 탐색은 현재 존 + 직접 이웃만 본다. 이웃이 비어 있으면 그 존의 파티는 시설을
		// 하나도 못 본다 — `Zone밖 N` 경고의 원인이 배치인지 이웃 그래프인지 여기서 갈린다.
		{
			TSet<FName> ZonesWithFacility;
			for (const FFFZSFacilityReadModel& Facility : ReadModel.Facilities)
			{
				ZonesWithFacility.Add(Facility.ZoneId);
			}
			int32 ZonesWithoutNeighbours = 0;
			int32 ZonesThatSeeNothing = 0;
			TArray<FString> BlindZones;
			for (const FFFZSZoneReadModel& Zone : ReadModel.Zones)
			{
				if (Zone.NeighborZoneIds.IsEmpty())
				{
					++ZonesWithoutNeighbours;
				}
				bool bSeesAny = ZonesWithFacility.Contains(Zone.ZoneId);
				for (const FName Neighbour : Zone.NeighborZoneIds)
				{
					bSeesAny |= ZonesWithFacility.Contains(Neighbour);
				}
				if (!bSeesAny)
				{
					++ZonesThatSeeNothing;
					if (BlindZones.Num() < 8) { BlindZones.Add(Zone.ZoneId.ToString()); }
				}
			}
			Test->AddInfo(FString::Printf(
				TEXT("[존] 전체 %d · 이웃 미저작 %d · 시설 보유 %d · 시설을 하나도 못 보는 존 %d [%s]"),
				ReadModel.Zones.Num(), ZonesWithoutNeighbours, ZonesWithFacility.Num(),
				ZonesThatSeeNothing, *FString::Join(BlindZones, TEXT(","))));
		}

		// ★ 개장은 편성 **뒤**다. 유료 프로그램은 Validated(운영 전) 시설에만 걸리므로
		// 먼저 열어버리면 전 시설이 "설치가 확정되고 운영 전인 시설에만" 사유로 거부된다.
		if (StageBridge)
		{
			StageBridge->OpenEligibleFacilities(false);
		}

		// 거부 사유를 그대로 남긴다. 이게 없으면 "왜 Live 가 안 되는가"를 또 손으로 찾는다.
		FText BlockingReason;
		if (!Stage->CanTransitionTo(EFF_FestivalStage::Live, BlockingReason))
		{
			Test->AddError(FString::Printf(TEXT("Live 전환이 거부됐다: %s"), *BlockingReason.ToString()));
			return true;
		}
		Stage->RequestTransition(EFF_FestivalStage::Live);
		if (!Test->TestEqual(TEXT("Live 로 전환됐다"), Stage->CurrentStage, EFF_FestivalStage::Live))
		{
			return true;
		}
		Test->AddInfo(FString::Printf(TEXT("Day %d Live 시작, 총 길이 %.0f초"),
			Stage->CurrentDay, Stage->GetLiveDurationForCurrentDay()));
		return true;
	}

private:
	// 방문객 조건을 명령줄로 바꾼다. 풀 한도를 재려면 인원이 가장 많이 나오는 조합을 건다.
	//   -FFScenarioPromotion=Influencer -FFScenarioFee=0 -FFScenarioBuzz=100
	void ApplyVisitorConditions(UGameInstance* Game)
	{
		UFF_FestivalProgressionSubsystem* Progression =
			Game ? Game->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
		if (!Progression)
		{
			return;
		}

		FString PromotionName;
		if (FParse::Value(FCommandLine::Get(), TEXT("FFScenarioPromotion="), PromotionName))
		{
			const int64 Value = StaticEnum<EFF_PromotionGrade>()->GetValueByNameString(PromotionName);
			if (Value == INDEX_NONE
				|| Progression->TryPurchasePromotion(static_cast<EFF_PromotionGrade>(Value)) != EFF_PromotionPurchaseResult::Success)
			{
				Test->AddError(FString::Printf(TEXT("홍보 '%s' 을 적용하지 못했다."), *PromotionName));
			}
		}

		int32 Fee = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("FFScenarioFee="), Fee)
			&& Progression->TrySetAdmissionFee(Fee) != EFF_AdmissionFeeResult::Success)
		{
			Test->AddError(FString::Printf(TEXT("입장료 %d 를 적용하지 못했다."), Fee));
		}

		float TargetBuzz = 0.f;
		UFF_FestivalBuzzSubsystem* Buzz = Game->GetSubsystem<UFF_FestivalBuzzSubsystem>();
		if (Buzz && FParse::Value(FCommandLine::Get(), TEXT("FFScenarioBuzz="), TargetBuzz))
		{
			Buzz->AddBuzz(TargetBuzz - Buzz->GetBuzz(), TEXT("Scenario"));
		}

		const FFF_VisitorForecast Forecast = Progression->GetVisitorForecast();
		Test->AddInfo(FString::Printf(
			TEXT("[조건] 홍보 %s · 입장료 %d · 화제성 %.0f → 예상 %d (배수 홍보 %.2f · 입장료 %.2f · 화제성 %.2f)"),
			*UEnum::GetValueAsString(Progression->GetPurchasedPromotionGrade()), Forecast.AdmissionFeeGold,
			Buzz ? Buzz->GetBuzz() : 0.f, Forecast.ExpectedVisitors,
			Forecast.PromotionMultiplier, Forecast.AdmissionFeeMultiplier, Forecast.BuzzMultiplier));
	}

	FAutomationTestBase* Test;
	double StartedAt = -1.0;
};

// 공연이 끝날 때까지 월드를 돌리고, 그 사이의 최고치와 종료 시점 값을 남긴다.
class FFFScenarioObserveShow final : public IAutomationLatentCommand
{
public:
	explicit FFFScenarioObserveShow(FAutomationTestBase* InTest) : Test(InTest) {}

	virtual bool Update() override
	{
		if (StartedAt < 0.0) StartedAt = FPlatformTime::Seconds();
		UWorld* World = FFMainStageScenario::PlayWorld();
		UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
		UFF_FestivalStageSubsystem* Stage = Game ? Game->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
		UFF_CrowdSimulationSubsystem* Crowd = World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;
		if (!Stage || !Crowd)
		{
			Test->AddError(TEXT("관측 중 월드나 서브시스템이 사라졌다."));
			return true;
		}
		if (Stage->CurrentStage != EFF_FestivalStage::Live)
		{
			// 준비 단계가 실패했으면 여기서 실시간 예산을 태울 이유가 없다.
			Test->AddError(TEXT("Live 가 아니라 관측을 건너뛴다 — 앞 단계 오류를 볼 것."));
			return true;
		}

		int32 Seated = 0, Walking = 0, AtVenue = 0, ProgramWalking = 0;
		Crowd->GetScheduledPhaseCounts(Seated, Walking, AtVenue, ProgramWalking);
		PeakSeated = FMath::Max(PeakSeated, Seated);
		PeakProgramAtVenue = FMath::Max(PeakProgramAtVenue, AtVenue);

		// -NullRHI 로 돌리면 렌더 비용이 빠져 게임 스레드(군중 시뮬) 비용만 남는다.
		PeakActive = FMath::Max(PeakActive, Crowd->GetActiveAgentCount());
		// 파티 최대 인원(6)만큼도 안 남으면 입장이 밀리는 중이다.
		if (Crowd->GetAgentPoolSize() - Crowd->GetActiveAgentCount() < 6) { ++PoolFullFrames; }
		const double FrameMs = FApp::GetDeltaTime() * 1000.0;
		FrameMsSum += FrameMs;
		++FrameCount;
		PeakFrameMs = FMath::Max(PeakFrameMs, FrameMs);
		if (FrameMs > 33.3) { ++SlowFrames; }

		const bool bTimedOut =
			FPlatformTime::Seconds() - StartedAt > FFMainStageScenario::WallClockBudgetSeconds;
		if (Stage->ElapsedLiveSeconds < FFMainStageScenario::ObserveUntilLiveSeconds && !bTimedOut)
		{
			return false;
		}
		if (bTimedOut)
		{
			Test->AddWarning(FString::Printf(
				TEXT("실시간 예산 초과. Live %.0f초에서 관측을 끊었다."), Stage->ElapsedLiveSeconds));
		}

		int32 Attempts = 0, Parties = 0, Members = 0, CapacityRejects = 0, SnapshotFailures = 0, ProgramConflicts = 0;
		Crowd->GetAudienceStats(Attempts, Parties, Members, CapacityRejects, SnapshotFailures, ProgramConflicts);

		Test->AddInfo(FString::Printf(
			TEXT("[관람] 최고 착석 %d파티 · 종료 시 착석 %d · 이동 중 %d"),
			PeakSeated, Seated, Walking));
		Test->AddInfo(FString::Printf(
			TEXT("[관람] 하루 누적 배정 %d파티(%d명) — 동시 인원이 아니라 연인원이다"),
			Parties, Members));
		Test->AddInfo(FString::Printf(
			TEXT("[관람] 정원거절 %d · 스냅샷실패 %d · 예약우선제외 %d · 좌석회수 %d"),
			CapacityRejects, SnapshotFailures, ProgramConflicts, Crowd->GetAudienceSeatsReclaimed()));
		Test->AddInfo(FString::Printf(
			TEXT("[예약] 최고 장내 %d파티 / 종료 시 장내 %d · 이동 중 %d"),
			PeakProgramAtVenue, AtVenue, ProgramWalking));
		Test->AddInfo(FString::Printf(
			TEXT("[군중] 활성 %d명 · 버려진 시뮬 시간 %.2f초 (0 이 아니면 위 수치를 믿지 말 것)"),
			Crowd->GetActiveAgentCount(), Crowd->GetDroppedSimSeconds()));

		const UFF_FestivalProgressionSubsystem* Progression =
			Game ? Game->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
		Test->AddInfo(FString::Printf(
			TEXT("[인원] 예상 %d · 입장 %d · 최고 동시 %d / 풀 %d · 풀이 차서 입장이 밀린 프레임 %d/%d"),
			Progression ? Progression->GetExpectedVisitors() : INDEX_NONE,
			Progression ? Progression->GetAdmittedVisitorsToday() : INDEX_NONE,
			PeakActive, Crowd->GetAgentPoolSize(), PoolFullFrames, FrameCount));
		Test->AddInfo(FString::Printf(
			TEXT("[프레임] 평균 %.1fms · 최대 %.1fms · 33ms 초과 %d/%d 프레임 (NullRHI 면 게임 스레드만)"),
			FrameCount > 0 ? FrameMsSum / FrameCount : 0.0, PeakFrameMs, SlowFrames, FrameCount));

		// 계측이 목적이라 임계를 박지 않는다. 배선이 끊긴 경우만 잡는다.
		Test->TestTrue(TEXT("공연 구간에 한 파티라도 무대에 앉았다"), PeakSeated > 0);
		return true;
	}

private:
	FAutomationTestBase* Test;
	double StartedAt = -1.0;
	int32 PeakSeated = 0;
	int32 PeakProgramAtVenue = 0;
	int32 PeakActive = 0;
	double FrameMsSum = 0.0;
	double PeakFrameMs = 0.0;
	int32 FrameCount = 0;
	int32 SlowFrames = 0;
	int32 PoolFullFrames = 0;
};

class FFFScenarioWaitForTeardown final : public IAutomationLatentCommand
{
public:
	virtual bool Update() override
	{
		if (StartedAt < 0.0) StartedAt = FPlatformTime::Seconds();
		if (!FFMainStageScenario::HasSession()) return true;
		return FPlatformTime::Seconds() - StartedAt > 60.0;
	}

private:
	double StartedAt = -1.0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFMainStageDayOneScenario,
	"FestaFiesta.Scenario.MainStageDayOne",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFFMainStageDayOneScenario::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (!FParse::Param(FCommandLine::Get(), TEXT("FFScenario")))
	{
		AddInfo(TEXT("건너뜀 — 실시간 2분짜리 시나리오다. -FFScenario 로 실행할 것."));
		return true;
	}
	if (FFMainStageScenario::HasSession())
	{
		AddError(TEXT("앞선 PIE 세션이 남아 있어 시나리오를 시작할 수 없다."));
		return false;
	}

	UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/FestaFiesta/Maps/Test_Main"));
	if (!TestNotNull(TEXT("릴리스 맵이 저장 없이 열린다"), World)) return false;

	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FFFScenarioArrangeDay(this));
	ADD_LATENT_AUTOMATION_COMMAND(FFFScenarioObserveShow(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FFFScenarioWaitForTeardown());
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
