// =================================================================================================
// ★ 일회성 프로브다. 값을 확인한 뒤 이 파일은 삭제한다.
//
// 목적: 실제 레벨(Test_Main)에 NavMesh가 구워져 있는지, 존 앵커가 NavMesh 위에 있는지,
//       그리고 라인 트레이스 지면과 NavMesh 지면이 얼마나 다른지를 한 번에 재서 로그로 남긴다.
//
// 검증(TestTrue)을 거의 하지 않는다 — 이 파일은 '측정'이고, 결과에 따라 다음 작업이 정해진다.
// =================================================================================================

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "CrowdSystem/Systems/FF_CrowdHeightField.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSTypes.h"
#include "FF_FFZSZoneActor.h"
#include "CrowdSystem/FF_SpawnActor.h"
#include "EngineUtils.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

DEFINE_LOG_CATEGORY_STATIC(LogNavProbe, Log, All);

DEFINE_LATENT_AUTOMATION_COMMAND(FFF_ProbeAnchorsInPIE);
bool FFF_ProbeAnchorsInPIE::Update()
{
	UWorld* PieWorld = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE && Context.World())
		{
			PieWorld = Context.World();
			break;
		}
	}

	if (!PieWorld)
	{
		UE_LOG(LogNavProbe, Error, TEXT("[5] PIE 월드를 얻지 못했다"));
		return true;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(PieWorld);
	const UFFZSWorldSubsystem* ZoneSystem = PieWorld->GetSubsystem<UFFZSWorldSubsystem>();

	if (!NavSys || !ZoneSystem)
	{
		UE_LOG(LogNavProbe, Error, TEXT("[5] PIE에 NavSys=%s ZoneSystem=%s"),
			NavSys ? TEXT("있음") : TEXT("없음"), ZoneSystem ? TEXT("있음") : TEXT("없음"));
		return true;
	}

	const FFFZSReadModel ReadModel = ZoneSystem->BuildReadModel();
	UE_LOG(LogNavProbe, Log, TEXT("[5] PIE: 존 %d개, 시설 %d개 (리비전 %lld)"),
		ReadModel.Zones.Num(), ReadModel.Facilities.Num(), ZoneSystem->GetWorldRevision());

	TArray<FVector> AnchorsCm;
	TArray<FName> ZoneIds;
	int32 OffMeshAnchors = 0;

	for (const FFFZSZoneReadModel& Zone : ReadModel.Zones)
	{
		const AFFZSGameplayZone* ZoneActor = ZoneSystem->FindZone(Zone.ZoneId);
		if (!ZoneActor)
		{
			UE_LOG(LogNavProbe, Warning, TEXT("[5] %s: 액터 없음"), *Zone.ZoneId.ToString());
			continue;
		}

		const FVector AnchorCm = Zone.EntryExitAnchors.IsEmpty()
			? ZoneActor->GetActorLocation()
			: ZoneActor->GetActorTransform().TransformPosition(Zone.EntryExitAnchors[0].GetLocation());

		FNavLocation Projected;
		const bool bOnMesh = NavSys->ProjectPointToNavigation(
			AnchorCm, Projected, FVector(50.0, 50.0, 500.0));

		if (!bOnMesh)
		{
			++OffMeshAnchors;
		}

		AnchorsCm.Add(bOnMesh ? FVector(Projected.Location) : AnchorCm);
		ZoneIds.Add(Zone.ZoneId);

		const FVector ZoneCenter = ZoneActor->GetActorLocation();
		const FVector Offset = AnchorCm - ZoneCenter;

		UE_LOG(LogNavProbe, Log,
			TEXT("[5] %-24s 앵커%d개  NavMesh %-4s  Z=%.0f→%.0f (Δ%.0f)  존중심에서 (%.0f, %.0f) %.0fcm"),
			*Zone.ZoneId.ToString(), Zone.EntryExitAnchors.Num(),
			bOnMesh ? TEXT("위") : TEXT("밖!"),
			AnchorCm.Z, bOnMesh ? Projected.Location.Z : 0.0,
			bOnMesh ? (Projected.Location.Z - AnchorCm.Z) : 0.0,
			Offset.X, Offset.Y, Offset.Size2D());
	}

	if (OffMeshAnchors > 0)
	{
		UE_LOG(LogNavProbe, Error,
			TEXT("[5] ★ 앵커 %d/%d개가 NavMesh 밖이다 — 그 존이 걸린 쌍은 보행거리가 안 나온다"),
			OffMeshAnchors, AnchorsCm.Num());
	}
	else if (AnchorsCm.Num() > 0)
	{
		UE_LOG(LogNavProbe, Log, TEXT("[5] ★ 앵커 %d개 전부 NavMesh 위에 있다"), AnchorsCm.Num());
	}

	int32 PairOk = 0;
	int32 PairFail = 0;
	double MaxPathCm = 0.0;
	double MaxRatio = 0.0;
	FString WorstPair;

	for (int32 i = 0; i < AnchorsCm.Num(); ++i)
	{
		for (int32 j = 0; j < AnchorsCm.Num(); ++j)
		{
			if (i == j)
			{
				continue;
			}

			double LengthCm = 0.0;
			if (NavSys->GetPathLength(AnchorsCm[i], AnchorsCm[j], LengthCm) != ENavigationQueryResult::Success)
			{
				++PairFail;
				if (PairFail <= 5)
				{
					UE_LOG(LogNavProbe, Warning, TEXT("[5] 경로 실패: %s -> %s"),
						*ZoneIds[i].ToString(), *ZoneIds[j].ToString());
				}
				continue;
			}

			++PairOk;
			MaxPathCm = FMath::Max(MaxPathCm, LengthCm);

			const double StraightCm = FVector::Dist2D(AnchorsCm[i], AnchorsCm[j]);
			if (StraightCm > 100.0)
			{
				const double Ratio = LengthCm / StraightCm;
				if (Ratio > MaxRatio)
				{
					MaxRatio = Ratio;
					WorstPair = FString::Printf(TEXT("%s -> %s (직선 %.0fm, 보행 %.0fm)"),
						*ZoneIds[i].ToString(), *ZoneIds[j].ToString(),
						StraightCm * 0.01, LengthCm * 0.01);
				}
			}
		}
	}

	UE_LOG(LogNavProbe, Log,
		TEXT("[5] ★ 앵커 쌍: 성공 %d / 실패 %d, 최대 보행거리 %.1fm  <-- distance_ref_meters 재보정의 입력"),
		PairOk, PairFail, MaxPathCm * 0.01);
	UE_LOG(LogNavProbe, Log, TEXT("[5] 최대 보행/직선(XY) 비율 %.2f : %s"), MaxRatio, *WorstPair);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NavMeshProbe, "FestaFiesta.Probe.NavMeshBakeStatus", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_NavMeshProbe::RunTest(const FString& Parameters)
{
	const FString MapName = TEXT("/Game/FestaFiesta/Maps/Test_Main");

	UEditorLoadingAndSavingUtils::LoadMap(MapName);

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr
		;
	if (!World)
	{
		UE_LOG(LogNavProbe, Error, TEXT("에디터 월드를 얻지 못했다"));
		return false;
	}

	UE_LOG(LogNavProbe, Log, TEXT("=== 맵: %s ==="), *World->GetName());

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		UE_LOG(LogNavProbe, Error, TEXT("[1] NavigationSystem이 없다"));
		return true;
	}

	ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
	UE_LOG(LogNavProbe, Log, TEXT("[1] NavData: %s"),
		NavData ? *NavData->GetName() : TEXT("없다"));

	if (const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(NavData))
	{
		UE_LOG(LogNavProbe, Log, TEXT("[1] Recast: 유효=%s, 타일 %d개, RuntimeGeneration=%d"),
			Recast->HasValidNavmesh() ? TEXT("예") : TEXT("아니오"),
			Recast->GetNavMeshTilesCount(),
			static_cast<int32>(Recast->GetRuntimeGenerationMode()));
	}
	else
	{
		UE_LOG(LogNavProbe, Warning, TEXT("[1] Recast NavMesh가 아니다"));
	}

	TArray<FVector> AnchorsCm;
	TArray<FName> AnchorZoneIds;

	if (const UFFZSWorldSubsystem* ZoneSystem = World->GetSubsystem<UFFZSWorldSubsystem>())
	{
		const FFFZSReadModel ReadModel = ZoneSystem->BuildReadModel();
		UE_LOG(LogNavProbe, Log, TEXT("[2] 존 %d개, 시설 %d개 (리비전 %lld)"),
			ReadModel.Zones.Num(), ReadModel.Facilities.Num(), ZoneSystem->GetWorldRevision());

		for (const FFFZSZoneReadModel& Zone : ReadModel.Zones)
		{
			const AFFZSGameplayZone* ZoneActor = ZoneSystem->FindZone(Zone.ZoneId);
			if (!ZoneActor)
			{
				UE_LOG(LogNavProbe, Warning, TEXT("[2] %s: 액터 없음"), *Zone.ZoneId.ToString());
				continue;
			}

			const FVector AnchorCm = Zone.EntryExitAnchors.IsEmpty()
				? ZoneActor->GetActorLocation()
				: ZoneActor->GetActorTransform().TransformPosition(Zone.EntryExitAnchors[0].GetLocation());

			AnchorsCm.Add(AnchorCm);
			AnchorZoneIds.Add(Zone.ZoneId);

			FNavLocation Projected;
			const bool bOnMesh = NavSys->ProjectPointToNavigation(
				AnchorCm, Projected, FVector(200.f, 200.f, 500.f));

			UE_LOG(LogNavProbe, Log,
				TEXT("[2] %-24s 앵커 Z=%.0f  NavMesh %s  투영 Z=%.0f  ΔZ=%.0f  (앵커 %d개)"),
				*Zone.ZoneId.ToString(), AnchorCm.Z,
				bOnMesh ? TEXT("위") : TEXT("밖!"),
				bOnMesh ? Projected.Location.Z : 0.f,
				bOnMesh ? (Projected.Location.Z - AnchorCm.Z) : 0.f,
				Zone.EntryExitAnchors.Num());
		}
	}
	else
	{
		UE_LOG(LogNavProbe, Warning, TEXT("[2] ZoneSystem이 없다"));
	}

	int32 PairOk = 0;
	int32 PairFail = 0;
	double MaxPathCm = 0.0;
	double MaxRatio = 0.0;

	for (int32 i = 0; i < AnchorsCm.Num(); ++i)
	{
		for (int32 j = 0; j < AnchorsCm.Num(); ++j)
		{
			if (i == j)
			{
				continue;
			}

			double LengthCm = 0.0;
			const ENavigationQueryResult::Type Result =
				NavSys->GetPathLength(AnchorsCm[i], AnchorsCm[j], LengthCm);

			if (Result != ENavigationQueryResult::Success)
			{
				++PairFail;
				UE_LOG(LogNavProbe, Warning, TEXT("[3] 실패: %s -> %s"),
					*AnchorZoneIds[i].ToString(), *AnchorZoneIds[j].ToString());
				continue;
			}

			++PairOk;
			MaxPathCm = FMath::Max(MaxPathCm, LengthCm);

			const double StraightCm = FVector::Dist(AnchorsCm[i], AnchorsCm[j]);
			if (StraightCm > 100.0)
			{
				MaxRatio = FMath::Max(MaxRatio, LengthCm / StraightCm);
			}
		}
	}

	UE_LOG(LogNavProbe, Log,
		TEXT("[3] 앵커 쌍 경로: 성공 %d / 실패 %d, 최대 보행거리 %.0fcm (%.1fm), 최대 보행/직선 비율 %.2f"),
		PairOk, PairFail, MaxPathCm, MaxPathCm * 0.01, MaxRatio);

	{
		FVector Center = FVector::ZeroVector;
		double HalfSizeCm = 10000.0;
		bool bFoundSpawnActor = false;

		for (TActorIterator<AFF_SpawnActor> It(World); It; ++It)
		{
			Center = It->GetActorLocation();
			bFoundSpawnActor = true;

			HalfSizeCm = 10000.0;

			UE_LOG(LogNavProbe, Log, TEXT("[4] AFF_SpawnActor: 위치 (%.0f, %.0f, %.0f)"),
				Center.X, Center.Y, Center.Z);
			break;
		}

		if (!bFoundSpawnActor)
		{
			const FBox NavBounds = NavData ? NavData->GetBounds() : FBox(ForceInit);
			if (NavBounds.IsValid)
			{
				Center = NavBounds.GetCenter();
				HalfSizeCm = FMath::Min(NavBounds.GetExtent().GetMax(), 20000.0);
			}
			UE_LOG(LogNavProbe, Warning,
				TEXT("[4] AFF_SpawnActor를 못 찾았다 — NavMesh 경계 중심 (%.0f, %.0f, %.0f)로 대체"),
				Center.X, Center.Y, Center.Z);
		}

		if (NavData)
		{
			const FBox NavBounds = NavData->GetBounds();
			UE_LOG(LogNavProbe, Log, TEXT("[4] NavMesh 경계: 중심 (%.0f, %.0f, %.0f), 크기 (%.0f, %.0f, %.0f)"),
				NavBounds.GetCenter().X, NavBounds.GetCenter().Y, NavBounds.GetCenter().Z,
				NavBounds.GetSize().X, NavBounds.GetSize().Y, NavBounds.GetSize().Z);
		}

		constexpr double SpacingCm = 500.0;

		FCollisionQueryParams Params(SCENE_QUERY_STAT(NavProbe), false);

		int32 TraceHit = 0;
		int32 NavHit = 0;
		int32 BothHit = 0;
		int32 TraceOnly = 0;
		int32 NavOnly = 0;
		double MaxAbsDeltaZ = 0.0;
		double SumAbsDeltaZ = 0.0;
		int32 BigDelta = 0;

		const int32 Divisions = static_cast<int32>(2.0 * HalfSizeCm / SpacingCm);
		for (int32 Iy = 0; Iy <= Divisions; ++Iy)
		{
			for (int32 Ix = 0; Ix <= Divisions; ++Ix)
			{
				const double X = Center.X - HalfSizeCm + Ix * SpacingCm;
				const double Y = Center.Y - HalfSizeCm + Iy * SpacingCm;

				FHitResult Hit;
				const bool bTrace = World->LineTraceSingleByChannel(
					Hit,
					FVector(X, Y, Center.Z + 500.0),
					FVector(X, Y, Center.Z - 50000.0),
					ECC_WorldStatic, Params);

				FNavLocation Nav;
				const bool bNav = NavSys->ProjectPointToNavigation(
					FVector(X, Y, bTrace ? Hit.ImpactPoint.Z : Center.Z), Nav,
					FVector(20.0, 20.0, 300.0));

				TraceHit += bTrace ? 1 : 0;
				NavHit += bNav ? 1 : 0;

				if (bTrace && bNav)
				{
					++BothHit;
					const double Delta = FMath::Abs(Nav.Location.Z - Hit.ImpactPoint.Z);
					MaxAbsDeltaZ = FMath::Max(MaxAbsDeltaZ, Delta);
					SumAbsDeltaZ += Delta;
					if (Delta > 50.0)
					{
						++BigDelta;
					}
				}
				else if (bTrace)
				{
					++TraceOnly;
				}
				else if (bNav)
				{
					++NavOnly;
				}
			}
		}

		const int32 Total = (Divisions + 1) * (Divisions + 1);
		UE_LOG(LogNavProbe, Log, TEXT("[4] 표본 %d점 (중심 %.0f,%.0f 반경 %.0fcm, 간격 %.0fcm)"),
			Total, Center.X, Center.Y, HalfSizeCm, SpacingCm);
		UE_LOG(LogNavProbe, Log, TEXT("[4] 트레이스 적중 %d, NavMesh 적중 %d, 둘 다 %d"),
			TraceHit, NavHit, BothHit);
		UE_LOG(LogNavProbe, Log,
			TEXT("[4] 트레이스만(=NavMesh 밖 지형) %d점  <-- 경로 추종에서 off-mesh 후보"),
			TraceOnly);
		UE_LOG(LogNavProbe, Log, TEXT("[4] NavMesh만 %d점"), NavOnly);

		if (BothHit > 0)
		{
			UE_LOG(LogNavProbe, Log,
				TEXT("[4] 둘 다 잡힌 점의 Z 차이: 평균 %.1fcm, 최대 %.1fcm, 50cm 초과 %d점"),
				SumAbsDeltaZ / BothHit, MaxAbsDeltaZ, BigDelta);
		}
	}

	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FFF_ProbeAnchorsInPIE());
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
