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

#include "CrowdSystem/Systems/FF_CrowdDecisionSubsystem.h"
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
			AnchorCm, Projected, FFF_CrowdNav::GetAnchorProjectionExtentCm());

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

	TArray<int32> Parent;
	Parent.SetNum(AnchorsCm.Num());
	for (int32 i = 0; i < Parent.Num(); ++i)
	{
		Parent[i] = i;
	}

	TFunction<int32(int32)> FindRoot = [&Parent, &FindRoot](int32 X) -> int32
	{
		while (Parent[X] != X)
		{
			Parent[X] = Parent[Parent[X]];
			X = Parent[X];
		}
		return X;
	};

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

			const int32 RootI = FindRoot(i);
			const int32 RootJ = FindRoot(j);
			if (RootI != RootJ)
			{
				Parent[RootJ] = RootI;
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

	// --- NavMesh 섬 목록 -------------------------------------------------------------------
	TMap<int32, TArray<int32>> Islands;
	for (int32 i = 0; i < AnchorsCm.Num(); ++i)
	{
		Islands.FindOrAdd(FindRoot(i)).Add(i);
	}

	TArray<TArray<int32>> Sorted;
	Islands.GenerateValueArray(Sorted);
	Sorted.Sort([](const TArray<int32>& A, const TArray<int32>& B) { return A.Num() > B.Num(); });

	UE_LOG(LogNavProbe, Log, TEXT("[6] === NavMesh 섬 %d개 (존 %d개) ==="),
		Sorted.Num(), AnchorsCm.Num());

	if (Sorted.Num() > 1)
	{
		UE_LOG(LogNavProbe, Warning,
			TEXT("[6] ★ NavMesh가 %d조각으로 끊겨 있다 — 다른 섬에 있는 존끼리는 보행거리가 안 나오고 ")
			TEXT("직선거리로 폴백한다. 경로추종도 그 사이는 우회하지 못한다. ")
			TEXT("섬을 잇거나 NavLinkProxy 를 놓을 것"),
			Sorted.Num());
	}

	for (int32 IslandIndex = 0; IslandIndex < Sorted.Num(); ++IslandIndex)
	{
		const TArray<int32>& Members = Sorted[IslandIndex];

		FVector Centre = FVector::ZeroVector;
		FString Names;
		for (const int32 MemberIndex : Members)
		{
			Centre += AnchorsCm[MemberIndex];
			if (!Names.IsEmpty())
			{
				Names += TEXT(", ");
			}
			Names += ZoneIds[MemberIndex].ToString();
		}
		Centre /= FMath::Max(Members.Num(), 1);

		UE_LOG(LogNavProbe, Log,
			TEXT("[6] 섬 %d: 존 %2d개  중심 (%.0f, %.0f, %.0f)  %s"),
			IslandIndex, Members.Num(), Centre.X, Centre.Y, Centre.Z, *Names);
	}

	// ── [8] 각 존 -> 퇴장 출구 ─────────────────────────────────────────────────────
	// Closing에서 AllAgentLeavingTrigger가 전원의 목적지를 출구로 박는다. 시설에 있던 파티가
	// 거기서 실제로 출구까지 갈 수 있는지를 존별로 잰다. 두 가지를 따로 본다 —
	// GetPathLength는 '섬이 이어져 있나'(연결성), FindPathSync는 BuildPathForAgent와 똑같은
	// 설정이라 '개체가 실제로 어디서 멈추나'다. 후자의 끝점이 도착 반경 400cm 밖이면 그 존의
	// 파티는 180초 타임아웃으로만 사라진다.
	{
		const AActor* SpawnActor = nullptr;
		for (TActorIterator<AFF_SpawnActor> It(PieWorld); It; ++It)
		{
			SpawnActor = *It;
			break;
		}

		if (!SpawnActor)
		{
			UE_LOG(LogNavProbe, Warning, TEXT("[8] PIE에 AFF_SpawnActor가 없다 — 출구를 못 잰다"));
			return true;
		}

		FVector Offset(-4000.0, 0.0, 0.0);
		if (const FStructProperty* OffsetProp = CastField<FStructProperty>(
			AFF_SpawnActor::StaticClass()->FindPropertyByName(TEXT("Temp_ExitOffsetFromOrigin"))))
		{
			Offset = *OffsetProp->ContainerPtrToValuePtr<FVector>(SpawnActor);
		}

		const FVector Exit = SpawnActor->GetActorLocation() + Offset;

		FNavLocation ExitProjected;
		const bool bExitOnMesh = NavSys->ProjectPointToNavigation(
			Exit, ExitProjected, FVector(200.0, 200.0, 500.0));
		const FVector ExitNav = bExitOnMesh ? FVector(ExitProjected.Location) : Exit;

		UE_LOG(LogNavProbe, Log, TEXT("[8] 출구 (%.0f, %.0f, %.0f)  NavMesh %s"),
			Exit.X, Exit.Y, Exit.Z, bExitOnMesh ? TEXT("위") : TEXT("밖!"));

		const ANavigationData* PieNavData = NavSys->GetDefaultNavDataInstance();
		constexpr double ArrivalRadiusCm = 400.0;

		int32 Reachable = 0;
		int32 Unreachable = 0;
		int32 Stranded = 0;
		TArray<FName> StrandedZones;

		for (int32 i = 0; i < AnchorsCm.Num(); ++i)
		{
			double LengthCm = 0.0;
			const bool bConnected =
				NavSys->GetPathLength(AnchorsCm[i], ExitNav, LengthCm) == ENavigationQueryResult::Success;

			bool bPartial = true;
			double GapCm = -1.0;

			if (PieNavData)
			{
				FPathFindingQuery Query(nullptr, *PieNavData, AnchorsCm[i], Exit);
				Query.SetAllowPartialPaths(true);
				Query.SetRequireNavigableEndLocation(false);

				const FPathFindingResult Result = NavSys->FindPathSync(Query);
				if (Result.IsSuccessful() && Result.Path.IsValid())
				{
					const TArray<FNavPathPoint>& Points = Result.Path->GetPathPoints();
					bPartial = Result.IsPartial();
					GapCm = Points.Num() > 0 ? FVector::Dist2D(Points.Last().Location, Exit) : -1.0;
				}
			}

			const bool bArrives = (GapCm >= 0.0) && (GapCm <= ArrivalRadiusCm);

			if (bConnected && bArrives)
			{
				++Reachable;
			}
			else
			{
				++Unreachable;
				++Stranded;
				StrandedZones.Add(ZoneIds[i]);
			}

			UE_LOG(LogNavProbe, Log,
				TEXT("[8] %-24s 연결=%-4s 부분경로=%-4s 끝점거리=%7.0fcm  보행 %7.1fm  %s"),
				*ZoneIds[i].ToString(),
				bConnected ? TEXT("예") : TEXT("아니오"),
				bPartial ? TEXT("예") : TEXT("아니오"),
				GapCm,
				bConnected ? LengthCm * 0.01 : 0.0,
				bArrives ? TEXT("퇴장 가능") : TEXT("<-- 타임아웃 전용"));
		}

		UE_LOG(LogNavProbe, Log,
			TEXT("[8] ★ 퇴장 가능 %d / %d 존   출구에 못 닿는 존 %d개"),
			Reachable, AnchorsCm.Num(), Unreachable);

		if (Stranded > 0)
		{
			FString Names;
			for (const FName& ZoneId : StrandedZones)
			{
				if (!Names.IsEmpty())
				{
					Names += TEXT(", ");
				}
				Names += ZoneId.ToString();
			}
			UE_LOG(LogNavProbe, Error,
				TEXT("[8] ★ 출구 도달 불가 존 %d개 — 여기 있던 파티는 전부 180초 강제 Despawn이다: %s"),
				Stranded, *Names);
		}
	}

	// ── [9] 앵커 투영이 층을 잘못 잡는지 ───────────────────────────────────────────
	// GetAnchorProjectionExtentCm의 Z가 ±2000cm라 앵커가 20m 안의 아무 층에나 붙는다.
	// 같은 XY에서 라인 트레이스로 실제 지면을 찾아 그 Z와 비교한다. 크게 벌어지면 앵커가
	// 옥상이나 지하층에 붙은 것이고, 그 앵커는 프로덕션 보행거리 행렬도 같이 쓰므로
	// 개체가 그 엉뚱한 층을 목적지로 받고 있다는 뜻이다.
	{
		int32 WrongLayer = 0;
		FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(NavProbeLayer), false);

		for (const FFFZSZoneReadModel& Zone : ReadModel.Zones)
		{
			const AFFZSGameplayZone* ZoneActor = ZoneSystem->FindZone(Zone.ZoneId);
			if (!ZoneActor)
			{
				continue;
			}

			const FVector RawAnchor = Zone.EntryExitAnchors.IsEmpty()
				? ZoneActor->GetActorLocation()
				: ZoneActor->GetActorTransform().TransformPosition(Zone.EntryExitAnchors[0].GetLocation());

			FHitResult Hit;
			const bool bTrace = PieWorld->LineTraceSingleByChannel(
				Hit,
				RawAnchor + FVector(0.0, 0.0, 5000.0),
				RawAnchor - FVector(0.0, 0.0, 50000.0),
				ECC_WorldStatic, TraceParams);

			FNavLocation Wide;
			const bool bWide = NavSys->ProjectPointToNavigation(
				RawAnchor, Wide, FFF_CrowdNav::GetAnchorProjectionExtentCm());

			FNavLocation Tight;
			const bool bTight = bTrace
				? NavSys->ProjectPointToNavigation(
					FVector(RawAnchor.X, RawAnchor.Y, Hit.ImpactPoint.Z), Tight,
					FVector(300.0, 300.0, 100.0))
				: false;

			const double GroundZ = bTrace ? Hit.ImpactPoint.Z : 0.0;
			const double WideDelta = bWide ? (Wide.Location.Z - GroundZ) : 0.0;
			const bool bSuspect = bTrace && bWide && FMath::Abs(WideDelta) > 200.0;

			if (bSuspect)
			{
				++WrongLayer;
			}

			UE_LOG(LogNavProbe, Log,
				TEXT("[9] %-24s 지면Z=%7.0f  넓은투영Z=%7.0f (Δ%7.0f)  좁은투영=%-6s  %s"),
				*Zone.ZoneId.ToString(),
				GroundZ,
				bWide ? Wide.Location.Z : 0.0,
				WideDelta,
				bTight ? TEXT("성공") : TEXT("실패"),
				bSuspect ? TEXT("<-- 지면과 다른 층") : TEXT(""));
		}

		if (WrongLayer > 0)
		{
			UE_LOG(LogNavProbe, Error,
				TEXT("[9] ★ 앵커 %d/%d개가 지면에서 2m 이상 벗어난 층에 투영된다 — ")
				TEXT("보행거리 행렬과 목적지가 그 층을 가리킨다"),
				WrongLayer, ReadModel.Zones.Num());
		}
		else
		{
			UE_LOG(LogNavProbe, Log, TEXT("[9] ★ 앵커 전부 지면 층에 정상 투영된다"));
		}
	}

	// ── [10] 지면 격자 연결성 ──────────────────────────────────────────────────────
	// 앵커를 배제하고 바닥만 잰다. NavMesh 경계에 격자를 깔아 각 점을 라인 트레이스로
	// 지면에 내리고 Z ±100cm로만 투영한 뒤, 인접 격자점끼리 경로가 있으면 union 한다.
	// 큰 덩어리 하나로 뭉치면 바닥은 이어져 있고 [6]의 섬은 앵커 투영 아티팩트다.
	{
		const ANavigationData* PieNavData = NavSys->GetDefaultNavDataInstance();
		const FBox Bounds = PieNavData ? PieNavData->GetBounds() : FBox(ForceInit);

		if (!Bounds.IsValid)
		{
			UE_LOG(LogNavProbe, Warning, TEXT("[10] NavMesh 경계가 없다"));
			return true;
		}

		constexpr double SpacingCm = 1000.0;
		const int32 NumX = FMath::Min(static_cast<int32>(Bounds.GetSize().X / SpacingCm) + 1, 128);
		const int32 NumY = FMath::Min(static_cast<int32>(Bounds.GetSize().Y / SpacingCm) + 1, 128);

		FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(NavProbeGrid), false);

		TArray<FVector> Samples;
		TArray<int32> CellToSample;
		Samples.Reserve(NumX * NumY);
		CellToSample.Init(INDEX_NONE, NumX * NumY);

		for (int32 Iy = 0; Iy < NumY; ++Iy)
		{
			for (int32 Ix = 0; Ix < NumX; ++Ix)
			{
				const double X = Bounds.Min.X + Ix * SpacingCm;
				const double Y = Bounds.Min.Y + Iy * SpacingCm;

				FHitResult Hit;
				if (!PieWorld->LineTraceSingleByChannel(
					Hit,
					FVector(X, Y, Bounds.Max.Z + 5000.0),
					FVector(X, Y, Bounds.Min.Z - 5000.0),
					ECC_WorldStatic, TraceParams))
				{
					continue;
				}

				FNavLocation OnMesh;
				if (!NavSys->ProjectPointToNavigation(
					Hit.ImpactPoint, OnMesh, FVector(300.0, 300.0, 100.0)))
				{
					continue;
				}

				CellToSample[Iy * NumX + Ix] = Samples.Num();
				Samples.Add(OnMesh.Location);
			}
		}

		UE_LOG(LogNavProbe, Log,
			TEXT("[10] 격자 %dx%d (간격 %.0fcm), 지면+NavMesh 표본 %d점"),
			NumX, NumY, SpacingCm, Samples.Num());

		TArray<int32> GridParent;
		GridParent.SetNum(Samples.Num());
		for (int32 i = 0; i < GridParent.Num(); ++i)
		{
			GridParent[i] = i;
		}

		TFunction<int32(int32)> GridRoot = [&GridParent, &GridRoot](int32 X) -> int32
		{
			while (GridParent[X] != X)
			{
				GridParent[X] = GridParent[GridParent[X]];
				X = GridParent[X];
			}
			return X;
		};

		int32 LinkOk = 0;
		int32 LinkFail = 0;

		for (int32 Iy = 0; Iy < NumY; ++Iy)
		{
			for (int32 Ix = 0; Ix < NumX; ++Ix)
			{
				const int32 Here = CellToSample[Iy * NumX + Ix];
				if (Here == INDEX_NONE)
				{
					continue;
				}

				const int32 Neighbours[2] = {
					(Ix + 1 < NumX) ? CellToSample[Iy * NumX + (Ix + 1)] : INDEX_NONE,
					(Iy + 1 < NumY) ? CellToSample[(Iy + 1) * NumX + Ix] : INDEX_NONE,
				};

				for (const int32 There : Neighbours)
				{
					if (There == INDEX_NONE || GridRoot(Here) == GridRoot(There))
					{
						continue;
					}

					double LengthCm = 0.0;
					if (NavSys->GetPathLength(Samples[Here], Samples[There], LengthCm)
						== ENavigationQueryResult::Success)
					{
						GridParent[GridRoot(There)] = GridRoot(Here);
						++LinkOk;
					}
					else
					{
						++LinkFail;
					}
				}
			}
		}

		TMap<int32, int32> ClusterSizes;
		for (int32 i = 0; i < Samples.Num(); ++i)
		{
			ClusterSizes.FindOrAdd(GridRoot(i)) += 1;
		}

		TArray<int32> Sizes;
		ClusterSizes.GenerateValueArray(Sizes);
		Sizes.Sort([](int32 A, int32 B) { return A > B; });

		UE_LOG(LogNavProbe, Log, TEXT("[10] 인접 연결 성공 %d / 실패 %d"), LinkOk, LinkFail);
		UE_LOG(LogNavProbe, Log, TEXT("[10] ★ 지면 덩어리 %d개"), Sizes.Num());

		const int32 Shown = FMath::Min(Sizes.Num(), 10);
		for (int32 i = 0; i < Shown; ++i)
		{
			UE_LOG(LogNavProbe, Log, TEXT("[10] 덩어리 %d: %d점 (%.1f%%)"),
				i, Sizes[i], 100.0 * Sizes[i] / FMath::Max(Samples.Num(), 1));
		}

		if (Sizes.Num() > 0 && Sizes[0] >= Samples.Num() * 0.8)
		{
			UE_LOG(LogNavProbe, Log,
				TEXT("[10] ★ 지면의 %.0f%%가 한 덩어리다 — 바닥은 이어져 있고 [6]의 섬은 앵커 투영 아티팩트다"),
				100.0 * Sizes[0] / Samples.Num());
		}
		else
		{
			UE_LOG(LogNavProbe, Error,
				TEXT("[10] ★ 지면 자체가 조각나 있다 — 최대 덩어리가 %.0f%%뿐이다. 레벨에서 섬을 이어야 한다"),
				Sizes.Num() > 0 ? 100.0 * Sizes[0] / Samples.Num() : 0.0);
		}
	}

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

	// ── [7] 퇴장 출구 좌표 ─────────────────────────────────────────────────────────
	// AllAgentLeavingTrigger가 전원의 TargetLocation으로 박는 좌표가 NavMesh 위인지 재고,
	// BuildPathForAgent와 같은 쿼리 설정(부분 경로 허용, 끝점 도달성 불요)으로 실제 경로를
	// 뽑아 부분 경로가 나오는지까지 확인한다. 부분 경로면 개체는 출구가 아니라 그 경로의
	// 마지막 점에서 멈추고, HasArrived는 TargetLocation 기준이라 도착 판정이 영원히 안 난다.
	{
		const AFF_SpawnActor* SpawnActor = nullptr;
		for (TActorIterator<AFF_SpawnActor> It(World); It; ++It)
		{
			SpawnActor = *It;
			break;
		}

		if (!SpawnActor)
		{
			UE_LOG(LogNavProbe, Warning, TEXT("[7] AFF_SpawnActor가 없다 — 출구 좌표를 못 잰다"));
		}
		else
		{
			// 두 값은 protected다. 프로덕션 헤더의 접근 지정자를 프로브 때문에 넓히지 않고
			// 리플렉션으로 읽는다 — 레벨에 배치된 인스턴스의 실제 오버라이드 값이 잡힌다.
			FVector Offset(-4000.0, 0.0, 0.0);
			float SimRadius = 10000.f;

			const UClass* SpawnClass = AFF_SpawnActor::StaticClass();
			if (const FStructProperty* OffsetProp =
				CastField<FStructProperty>(SpawnClass->FindPropertyByName(TEXT("Temp_ExitOffsetFromOrigin"))))
			{
				Offset = *OffsetProp->ContainerPtrToValuePtr<FVector>(SpawnActor);
			}
			else
			{
				UE_LOG(LogNavProbe, Warning, TEXT("[7] Temp_ExitOffsetFromOrigin 프로퍼티를 못 찾았다 — 기본값으로 잰다"));
			}

			if (const FFloatProperty* RadiusProp =
				CastField<FFloatProperty>(SpawnClass->FindPropertyByName(TEXT("SimRadiusUU"))))
			{
				SimRadius = RadiusProp->GetPropertyValue_InContainer(SpawnActor);
			}

			const FVector Origin = SpawnActor->GetActorLocation();
			const FVector Exit = Origin + Offset;

			UE_LOG(LogNavProbe, Log, TEXT("[7] 원점 (%.0f, %.0f, %.0f)  SimRadius %.0f"),
				Origin.X, Origin.Y, Origin.Z, SimRadius);
			UE_LOG(LogNavProbe, Log, TEXT("[7] 오프셋 (%.0f, %.0f, %.0f)  ->  출구 (%.0f, %.0f, %.0f)"),
				Offset.X, Offset.Y, Offset.Z, Exit.X, Exit.Y, Exit.Z);

			FHitResult ExitHit;
			FCollisionQueryParams ExitParams(SCENE_QUERY_STAT(NavProbeExit), false);
			const bool bExitTrace = World->LineTraceSingleByChannel(
				ExitHit,
				Exit + FVector(0.0, 0.0, 5000.0),
				Exit - FVector(0.0, 0.0, 50000.0),
				ECC_WorldStatic, ExitParams);

			UE_LOG(LogNavProbe, Log, TEXT("[7] 지면 트레이스: %s  Z=%.0f"),
				bExitTrace ? TEXT("적중") : TEXT("실패(허공)"),
				bExitTrace ? ExitHit.ImpactPoint.Z : 0.0);

			const FVector ExitOnGround = bExitTrace
				? FVector(Exit.X, Exit.Y, ExitHit.ImpactPoint.Z)
				: Exit;

			const FVector Extents[] = {
				FVector(20.0, 20.0, 300.0),
				FVector(200.0, 200.0, 500.0),
				FVector(1000.0, 1000.0, 2000.0),
			};

			for (const FVector& Extent : Extents)
			{
				FNavLocation Projected;
				const bool bOnMesh = NavSys->ProjectPointToNavigation(ExitOnGround, Projected, Extent);
				UE_LOG(LogNavProbe, Log,
					TEXT("[7] 투영 허용범위 %.0f: %s  %s  거리 %.0fcm"),
					Extent.X,
					bOnMesh ? TEXT("NavMesh 위") : TEXT("NavMesh 밖!"),
					bOnMesh ? *Projected.Location.ToCompactString() : TEXT(""),
					bOnMesh ? FVector::Dist(ExitOnGround, Projected.Location) : 0.0);
			}

			if (NavData)
			{
				FPathFindingQuery Query(nullptr, *NavData, Origin, Exit);
				Query.SetAllowPartialPaths(true);
				Query.SetRequireNavigableEndLocation(false);

				const FPathFindingResult Result = NavSys->FindPathSync(Query);

				if (!Result.IsSuccessful() || !Result.Path.IsValid())
				{
					UE_LOG(LogNavProbe, Error,
						TEXT("[7] ★ 원점->출구 경로 쿼리 실패 — 개체는 직선 조향으로 벽을 따라 미끄러진다"));
				}
				else
				{
					const TArray<FNavPathPoint>& Points = Result.Path->GetPathPoints();
					const FVector LastPoint = Points.Num() > 0 ? Points.Last().Location : Origin;
					const double GapCm = FVector::Dist2D(LastPoint, Exit);

					UE_LOG(LogNavProbe, Log,
						TEXT("[7] 원점->출구 경로: 점 %d개, 부분경로=%s"),
						Points.Num(), Result.IsPartial() ? TEXT("예") : TEXT("아니오"));
					UE_LOG(LogNavProbe, Log,
						TEXT("[7] 경로 마지막 점 (%.0f, %.0f, %.0f) — 출구와 %.0fcm 차이"),
						LastPoint.X, LastPoint.Y, LastPoint.Z, GapCm);

					if (Result.IsPartial() || GapCm > 400.0)
					{
						UE_LOG(LogNavProbe, Error,
							TEXT("[7] ★ 도착 반경 400cm 밖에서 경로가 끊긴다 — 퇴장이 타임아웃(180초)으로만 끝난다"));
					}
					else
					{
						UE_LOG(LogNavProbe, Log, TEXT("[7] 출구는 정상 도달 가능하다"));
					}
				}
			}
		}
	}

	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FFF_ProbeAnchorsInPIE());
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
