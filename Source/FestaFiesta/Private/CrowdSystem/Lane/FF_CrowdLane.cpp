// Fill out your copyright notice in the Description page of Project Settings.


#include "CrowdSystem/Lane/FF_CrowdLane.h"

#include "Components/SplineComponent.h"
#include "CrowdSystem/Lane/FF_CrowdLaneBake.h"

#if WITH_EDITOR
#include "Components/BoxComponent.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSProtectedArea.h"
#include "FF_FFZSZoneActor.h"
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "FFCrowdLane"

DEFINE_LOG_CATEGORY_STATIC(LogFFCrowdLane, Log, All);

namespace
{
	CrowdLaneBake::FBakeParams MakeBakeParams(const AFF_CrowdLane& Lane)
	{
		CrowdLaneBake::FBakeParams Params;
		Params.LaneHalfWidthCm = Lane.LaneHalfWidthCm;
		Params.SimplifyToleranceCm = Lane.SimplifyToleranceCm;
		Params.MinSegmentLengthRatio = Lane.MinSegmentLengthRatio;
		Params.CornerOverlapCm = Lane.CornerOverlapCm;
		return Params;
	}

#if WITH_EDITOR
	struct FLaneBox
	{
		FVector2D Center = FVector2D::ZeroVector;
		FVector2D AxisX = FVector2D(1.f, 0.f);
		FVector2D AxisY = FVector2D(0.f, 1.f);
		float HalfX = 0.f;
		float HalfY = 0.f;
		float CenterZ = 0.f;
		float HalfZ = 0.f;
	};

	FLaneBox MakeBox(const FVector& CenterCm, float YawDegrees, float HalfXCm, float HalfYCm, float HalfZCm)
	{
		const float Radians = FMath::DegreesToRadians(YawDegrees);

		FLaneBox Box;
		Box.Center = FVector2D(CenterCm.X, CenterCm.Y);
		Box.AxisX = FVector2D(FMath::Cos(Radians), FMath::Sin(Radians));
		Box.AxisY = FVector2D(-FMath::Sin(Radians), FMath::Cos(Radians));
		Box.HalfX = HalfXCm;
		Box.HalfY = HalfYCm;
		Box.CenterZ = static_cast<float>(CenterCm.Z);
		Box.HalfZ = HalfZCm;
		return Box;
	}

	FLaneBox MakeBoxFromComponent(const UBoxComponent& Component)
	{
		const FVector Extent = Component.GetScaledBoxExtent();
		return MakeBox(Component.GetComponentLocation(),
			static_cast<float>(Component.GetComponentRotation().Yaw),
			static_cast<float>(Extent.X), static_cast<float>(Extent.Y), static_cast<float>(Extent.Z));
	}

	// 플러그인의 단축 확장 규칙(FF_FFZSProtectedArea.cpp:115-125)을 그대로 재현한다.
	FLaneBox MakeBoxFromProtectedArea(const AFFZSProtectedArea& Area)
	{
		FLaneBox Box = MakeBoxFromComponent(*Area.AreaBounds);

		const float RequiredHalf = FMath::Max(0.f, Area.GetEffectiveRequiredClearWidthCm() * 0.5f);
		if (Box.HalfX <= Box.HalfY)
		{
			Box.HalfX = FMath::Max(Box.HalfX, RequiredHalf);
		}
		else
		{
			Box.HalfY = FMath::Max(Box.HalfY, RequiredHalf);
		}

		return Box;
	}

	bool SeparatedOnAxis(const FLaneBox& A, const FLaneBox& B, const FVector2D& Axis)
	{
		const FVector2D Delta = B.Center - A.Center;

		const float ProjectedDelta = FMath::Abs(static_cast<float>(FVector2D::DotProduct(Delta, Axis)));
		const float ReachA = A.HalfX * FMath::Abs(static_cast<float>(FVector2D::DotProduct(A.AxisX, Axis)))
			+ A.HalfY * FMath::Abs(static_cast<float>(FVector2D::DotProduct(A.AxisY, Axis)));
		const float ReachB = B.HalfX * FMath::Abs(static_cast<float>(FVector2D::DotProduct(B.AxisX, Axis)))
			+ B.HalfY * FMath::Abs(static_cast<float>(FVector2D::DotProduct(B.AxisY, Axis)));

		return ProjectedDelta > ReachA + ReachB;
	}

	bool BoxesOverlap(const FLaneBox& A, const FLaneBox& B)
	{
		if (FMath::Abs(A.CenterZ - B.CenterZ) > A.HalfZ + B.HalfZ)
		{
			return false;
		}

		return !SeparatedOnAxis(A, B, A.AxisX) && !SeparatedOnAxis(A, B, A.AxisY)
			&& !SeparatedOnAxis(A, B, B.AxisX) && !SeparatedOnAxis(A, B, B.AxisY);
	}
#endif
}

AFF_CrowdLane::AFF_CrowdLane()
{
	PrimaryActorTick.bCanEverTick = false;

	LanePath = CreateDefaultSubobject<USplineComponent>(TEXT("LanePath"));
	SetRootComponent(LanePath);
}

void AFF_CrowdLane::BuildPolylineCm(TArray<FVector>& OutPointsCm) const
{
	OutPointsCm.Reset();

	if (!LanePath || LanePath->GetNumberOfSplinePoints() < 2)
	{
		return;
	}

	const float LengthCm = LanePath->GetSplineLength();
	if (LengthCm <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float Spacing = FMath::Max(SampleSpacingCm, 50.f);
	const int32 Steps = FMath::Max(1, FMath::CeilToInt(LengthCm / Spacing));

	OutPointsCm.Reserve(Steps + 1);
	for (int32 Step = 0; Step <= Steps; ++Step)
	{
		const float DistanceCm = LengthCm * static_cast<float>(Step) / static_cast<float>(Steps);
		OutPointsCm.Add(LanePath->GetLocationAtDistanceAlongSpline(DistanceCm, ESplineCoordinateSpace::World));
	}

	// 닫힌 레인은 끝점을 시작점과 정확히 같게 맞춰 FFF_CrowdSplineLane이 루프로 인식하게 한다.
	if (LanePath->IsClosedLoop() && OutPointsCm.Num() >= 2)
	{
		OutPointsCm.Last() = OutPointsCm[0];
	}
}

float AFF_CrowdLane::GetRequiredClearWidthCm() const
{
	return 2.f * LaneHalfWidthCm + 2.f * (AgentRadiusCm + CrowdLaneBake::WallClearanceCm);
}

FString AFF_CrowdLane::GetSegmentIdPrefix() const
{
	return FString::Printf(TEXT("%s_Seg"), *LaneId.ToString());
}

#if WITH_EDITOR

void AFF_CrowdLane::Bake()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<FText> Errors;
	if (!ValidateLaneData(Errors))
	{
		for (const FText& Error : Errors)
		{
			UE_LOG(LogFFCrowdLane, Error, TEXT("[%s] %s"), *GetName(), *Error.ToString());
		}
		return;
	}

	ClearBakedAreas();

	TArray<FVector> Polyline;
	BuildPolylineCm(Polyline);

	TArray<FVector> Simplified;
	CrowdLaneBake::SimplifyPolyline(Polyline, SimplifyToleranceCm, Simplified);

	TArray<CrowdLaneBake::FLaneSegment> Segments;
	CrowdLaneBake::EmitSegments(Simplified, MakeBakeParams(*this), Segments);

	const float RequiredWidthCm = GetRequiredClearWidthCm();
	const FString Prefix = GetSegmentIdPrefix();

	Modify();

	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		const CrowdLaneBake::FLaneSegment& Segment = Segments[Index];

		FActorSpawnParameters SpawnParams;
		SpawnParams.OverrideLevel = GetLevel();
		SpawnParams.ObjectFlags = RF_Transactional;

		const FTransform SegmentTransform(FRotator(0.f, Segment.YawDegrees, 0.f), Segment.CenterCm);

		AFFZSProtectedArea* Area = World->SpawnActor<AFFZSProtectedArea>(
			AFFZSProtectedArea::StaticClass(), SegmentTransform, SpawnParams);
		if (!Area)
		{
			continue;
		}

		const FString SegmentName = FString::Printf(TEXT("%s%02d"), *Prefix, Index);

		Area->ProtectedAreaId = FName(*SegmentName);
		Area->LinkedZoneId = NAME_None;
		Area->AreaType = EFFZSProtectedAreaType::GeneralRoute;
		Area->RequiredClearWidthCm = RequiredWidthCm;

		if (Area->AreaBounds)
		{
			Area->AreaBounds->SetBoxExtent(FVector(
				Segment.HalfLengthCm, Segment.HalfWidthCm, ProtectedHeightCm * 0.5f));
		}

		Area->SetActorLabel(SegmentName);

		BakedAreas.Add(Area);
	}

	UE_LOG(LogFFCrowdLane, Log, TEXT("[%s] 레인 베이크 완료: 세그먼트 %d개, 보호 폭 %.1fcm"),
		*GetName(), Segments.Num(), RequiredWidthCm);
}

void AFF_CrowdLane::ClearBakedAreas()
{
	Modify();

	for (const TObjectPtr<AFFZSProtectedArea>& Area : BakedAreas)
	{
		if (AFFZSProtectedArea* Baked = Area.Get())
		{
			Baked->Destroy();
		}
	}
	BakedAreas.Reset();

	UWorld* World = GetWorld();
	if (!World || LaneId.IsNone())
	{
		return;
	}

	// 배열이 유실된 뒤 재베이크하면 같은 ID의 고아 박스가 남아 중복 ID 검사에 걸린다.
	const FString Prefix = GetSegmentIdPrefix();

	TArray<AFFZSProtectedArea*> Orphans;
	for (TActorIterator<AFFZSProtectedArea> It(World); It; ++It)
	{
		if (It->ProtectedAreaId.ToString().StartsWith(Prefix))
		{
			Orphans.Add(*It);
		}
	}

	for (AFFZSProtectedArea* Orphan : Orphans)
	{
		Orphan->Destroy();
	}
}

bool AFF_CrowdLane::ValidateLaneData(TArray<FText>& OutErrors) const
{
	OutErrors.Reset();

	if (LaneId.IsNone())
	{
		OutErrors.Add(LOCTEXT("MissingLaneId", "LaneId가 필요합니다. 세그먼트 ID의 접두사로 쓰입니다."));
	}

	if (!LanePath || LanePath->GetNumberOfSplinePoints() < 2)
	{
		OutErrors.Add(LOCTEXT("ShortSpline", "레인 스플라인은 점이 둘 이상이어야 합니다."));
		return OutErrors.IsEmpty();
	}

	FString WidthError;
	if (!CrowdLaneBake::ValidateWidthInvariant(LaneHalfWidthCm, GetRequiredClearWidthCm(),
		AgentRadiusCm, WidthError))
	{
		OutErrors.Add(FText::FromString(WidthError));
	}

	TArray<FVector> Polyline;
	BuildPolylineCm(Polyline);
	if (Polyline.Num() < 2)
	{
		OutErrors.Add(LOCTEXT("EmptyPolyline", "레인 스플라인의 길이가 0입니다."));
		return OutErrors.IsEmpty();
	}

	TArray<FVector> Simplified;
	CrowdLaneBake::SimplifyPolyline(Polyline, SimplifyToleranceCm, Simplified);

	TArray<CrowdLaneBake::FLaneSegment> Segments;
	CrowdLaneBake::EmitSegments(Simplified, MakeBakeParams(*this), Segments);

	if (Segments.Num() == 0)
	{
		OutErrors.Add(LOCTEXT("NoSegments", "보호 구역 세그먼트가 하나도 나오지 않았습니다."));
		return OutErrors.IsEmpty();
	}

	const float EffectiveHalfWidthCm = FMath::Max(LaneHalfWidthCm, GetRequiredClearWidthCm() * 0.5f);
	const float HalfHeightCm = ProtectedHeightCm * 0.5f;

	TArray<FLaneBox> LaneBoxes;
	LaneBoxes.Reserve(Segments.Num());
	for (const CrowdLaneBake::FLaneSegment& Segment : Segments)
	{
		// 짧고 뚱뚱한 박스는 RequiredClearWidthCm이 길이 축을 부풀려 엉뚱한 시설을 막는다.
		if (Segment.HalfLengthCm <= Segment.HalfWidthCm)
		{
			OutErrors.Add(LOCTEXT("StubbySegment",
				"세그먼트가 폭보다 짧습니다. CornerOverlapCm이나 MinSegmentLengthRatio를 키우십시오."));
			break;
		}

		LaneBoxes.Add(MakeBox(Segment.CenterCm, Segment.YawDegrees,
			Segment.HalfLengthCm, EffectiveHalfWidthCm, HalfHeightCm));
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return OutErrors.IsEmpty();
	}

	for (TActorIterator<AFFZSFacilityActor> It(World); It; ++It)
	{
		const AFFZSFacilityActor* Facility = *It;
		if (!Facility || !Facility->FootprintBounds)
		{
			continue;
		}

		const FLaneBox FacilityBox = MakeBoxFromComponent(*Facility->FootprintBounds);
		for (const FLaneBox& LaneBox : LaneBoxes)
		{
			if (BoxesOverlap(LaneBox, FacilityBox))
			{
				OutErrors.Add(FText::Format(
					LOCTEXT("LaneOverFacility", "레인이 이미 배치된 시설 {0}과 겹칩니다."),
					FText::FromString(Facility->GetActorNameOrLabel())));
				break;
			}
		}
	}

	const FString Prefix = GetSegmentIdPrefix();
	for (TActorIterator<AFFZSProtectedArea> It(World); It; ++It)
	{
		const AFFZSProtectedArea* Area = *It;
		if (!Area || !Area->AreaBounds || Area->AreaType != EFFZSProtectedAreaType::NoBuild)
		{
			continue;
		}
		if (Area->ProtectedAreaId.ToString().StartsWith(Prefix))
		{
			continue;
		}

		const FLaneBox AreaBox = MakeBoxFromProtectedArea(*Area);
		for (const FLaneBox& LaneBox : LaneBoxes)
		{
			if (BoxesOverlap(LaneBox, AreaBox))
			{
				OutErrors.Add(FText::Format(
					LOCTEXT("LaneOverNoBuild", "레인이 건설 금지 구역 {0}과 겹칩니다."),
					FText::FromName(Area->ProtectedAreaId)));
				break;
			}
		}
	}

	TArray<const AFFZSGameplayZone*> Zones;
	for (TActorIterator<AFFZSGameplayZone> It(World); It; ++It)
	{
		Zones.Add(*It);
	}

	if (Zones.Num() > 0)
	{
		for (const FVector& Point : Polyline)
		{
			bool bInsideAnyZone = false;
			for (const AFFZSGameplayZone* Zone : Zones)
			{
				if (Zone && Zone->ContainsWorldPoint(Point))
				{
					bInsideAnyZone = true;
					break;
				}
			}

			if (!bInsideAnyZone)
			{
				OutErrors.Add(LOCTEXT("LaneOutsideZone", "레인이 Gameplay Zone 밖으로 나갑니다."));
				break;
			}
		}
	}

	// 높이 격자는 BeginPlay에 구워지므로 에디터(비 PIE)에서는 검사하지 않는다.
	if (const UFF_CrowdSimulationSubsystem* Sim = World->GetSubsystem<UFF_CrowdSimulationSubsystem>())
	{
		const FFF_CrowdHeightField& HeightField = Sim->GetHeightField();
		if (HeightField.IsValid())
		{
			for (const FVector& Point : Polyline)
			{
				if (!HeightField.IsWalkable(FVector2D(Point.X, Point.Y)))
				{
					OutErrors.Add(LOCTEXT("LaneNotWalkable", "레인이 통행 불가 지형을 지납니다."));
					break;
				}
			}
		}
	}

	return OutErrors.IsEmpty();
}

void AFF_CrowdLane::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	if (!bFinished)
	{
		return;
	}

	TArray<FText> Errors;
	if (!ValidateLaneData(Errors))
	{
		for (const FText& Error : Errors)
		{
			UE_LOG(LogFFCrowdLane, Warning, TEXT("[%s] %s"), *GetName(), *Error.ToString());
		}
	}
}

EDataValidationResult AFF_CrowdLane::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TArray<FText> Errors;
	if (!ValidateLaneData(Errors))
	{
		for (const FText& Error : Errors)
		{
			Context.AddError(Error);
		}
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
