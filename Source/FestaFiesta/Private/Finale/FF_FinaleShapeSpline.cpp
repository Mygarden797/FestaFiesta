// Copyright Epic Games, Inc. All Rights Reserved.

#include "Finale/FF_FinaleShapeSpline.h"

#include "Components/SplineComponent.h"
#include "Finale/FF_FinaleShapeData.h"
#include "Finale/FF_FinaleShowTypes.h"

#if WITH_EDITOR
#include "DrawDebugHelpers.h"
#endif

AFF_FinaleShapeSpline::AFF_FinaleShapeSpline()
{
	PrimaryActorTick.bCanEverTick = false;

	// 저작 도구다. 게임에 들어갈 이유가 없어서 쿠킹에서 빠진다.
	bIsEditorOnlyActor = true;

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	RootComponent = Spline;

	/* 처음부터 모양을 깔아둔다.

	   USplineComponent 는 점 두 개짜리 직선으로 시작한다. 그걸 비우면 깔끔하긴 한데
	   점이 0개인 스플라인은 뷰포트에 잡을 것이 없어서 점을 추가할 방법도 없다.
	   맨 점에서 하트를 그리는 것도 스플라인으로 할 일이 아니라, 수식으로 깔아두고
	   손으로 다듬게 한다.
	*/
	ApplyTemplateToSpline();
}

void AFF_FinaleShapeSpline::BuildTemplatePoints(
	const EFF_FinaleShapeTemplate InTemplate, const float SizeCm, TArray<FVector2D>& OutPoints)
{
	OutPoints.Reset();

	const float Size = FMath::Max(10.f, SizeCm);

	switch (InTemplate)
	{
	case EFF_FinaleShapeTemplate::Heart:
	{
		/* 고전적인 하트 곡선.
		     x = 16 sin³t
		     y = 13 cos t - 5 cos 2t - 2 cos 3t - cos 4t

		   제어점을 16개만 뽑는다. 촘촘히 뽑으면 손으로 고칠 수 없는 덩어리가 되고,
		   성기면 스플라인 보간이 알아서 메워준다. 다듬는 게 목적이라 적은 편이 낫다.

		   위쪽 가운데가 뾰족한 부분은 보간이 둥글게 만든다. 날을 세우고 싶으면
		   그 점의 타입을 Linear 로 바꾸면 된다.
		*/
		const int32 Count = 16;
		OutPoints.Reserve(Count);

		// 곡선 자체의 최대 반경. 이것으로 나눠 SizeCm 에 맞춘다.
		const float RawExtent = 17.f;
		const float Scale = (Size * 0.5f) / RawExtent;

		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float T = 2.f * PI * static_cast<float>(Index) / static_cast<float>(Count);

			const float X = 16.f * FMath::Pow(FMath::Sin(T), 3.f);
			const float Y = 13.f * FMath::Cos(T)
				- 5.f * FMath::Cos(2.f * T)
				- 2.f * FMath::Cos(3.f * T)
				- FMath::Cos(4.f * T);

			OutPoints.Add(FVector2D(X, Y) * Scale);
		}
		break;
	}

	case EFF_FinaleShapeTemplate::Star:
	{
		// 오각별. 바깥 꼭짓점과 안쪽 꼭짓점을 번갈아 찍는다.
		const int32 Tips = 5;
		const float Outer = Size * 0.5f;
		const float Inner = Outer * 0.4f;

		OutPoints.Reserve(Tips * 2);

		for (int32 Index = 0; Index < Tips * 2; ++Index)
		{
			// 위쪽이 꼭짓점이 되도록 90도 돌려서 시작한다.
			const float Angle = PI * 0.5f + PI * static_cast<float>(Index) / static_cast<float>(Tips);
			const float Radius = (Index % 2 == 0) ? Outer : Inner;

			OutPoints.Add(FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		break;
	}

	case EFF_FinaleShapeTemplate::Circle:
	default:
	{
		const int32 Count = 8;
		const float Radius = Size * 0.5f;

		OutPoints.Reserve(Count);

		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Angle = 2.f * PI * static_cast<float>(Index) / static_cast<float>(Count);
			OutPoints.Add(FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		break;
	}
	}
}

FVector AFF_FinaleShapeSpline::PlanarToLocal(const FVector2D& Point) const
{
	return (AuthorPlane == EFF_FinaleShapeAuthorPlane::LocalXZ)
		? FVector(Point.X, 0.f, Point.Y)
		: FVector(Point.X, Point.Y, 0.f);
}

void AFF_FinaleShapeSpline::ApplyTemplateToSpline()
{
	if (!Spline)
	{
		return;
	}

	TArray<FVector2D> Planar;
	BuildTemplatePoints(Template, TemplateSizeCm, Planar);

	TArray<FVector> LocalPoints;
	LocalPoints.Reserve(Planar.Num());

	for (const FVector2D& Point : Planar)
	{
		LocalPoints.Add(PlanarToLocal(Point));
	}

	Spline->SetSplinePoints(LocalPoints, ESplineCoordinateSpace::Local, false);

	// 세 모양 모두 닫힌 도형이다. 열린 획을 그릴 때만 직접 끈다.
	Spline->SetClosedLoop(true, true);
}

void AFF_FinaleShapeSpline::SampleSpline(TArray<FVector2D>& OutPoints) const
{
	OutPoints.Reset();

	if (!Spline || Spline->GetNumberOfSplinePoints() < 2)
	{
		return;
	}

	const float Length = Spline->GetSplineLength();
	if (Length <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const int32 Resolution = FMath::Clamp(BakeResolution, 8, 1024);

	/* 닫힌 고리는 끝점을 다시 찍지 않는다. 시작점과 겹쳐서 한 자리에 점이 둘 생기고,
	   런타임 재샘플이 그 구간 길이를 0 으로 보게 된다.
	*/
	const bool bClosed = Spline->IsClosedLoop();

	// 간격은 같고 개수만 다르다. 열린 곡선은 끝점을 하나 더 찍어 끝까지 덮는다.
	const int32 SampleCount = bClosed ? Resolution : Resolution + 1;
	const float Step = Length / static_cast<float>(Resolution);

	OutPoints.Reserve(SampleCount);

	for (int32 Index = 0; Index < SampleCount; ++Index)
	{
		const float Distance = FMath::Min(Step * static_cast<float>(Index), Length);

		// 지역 좌표로 읽는다. 액터를 돌려놓고 작업해도 구워진 모양이 달라지면 안 된다.
		const FVector Local = Spline->GetLocationAtDistanceAlongSpline(
			Distance, ESplineCoordinateSpace::Local);

		OutPoints.Add(AuthorPlane == EFF_FinaleShapeAuthorPlane::LocalXZ
			? FVector2D(Local.X, Local.Z)
			: FVector2D(Local.X, Local.Y));
	}
}

#if WITH_EDITOR

void AFF_FinaleShapeSpline::ResetSplineToTemplate()
{
	// 되돌리기에 걸리게 한다. 공들여 다듬은 것을 실수로 눌러 날리면 안 된다.
	Modify();
	Spline->Modify();

	ApplyTemplateToSpline();

	UE_LOG(LogFestaFiestaFinale, Log,
		TEXT("[피날레] 시작 모양을 깔았다. 점 %d개. 뷰포트에서 점을 잡아 다듬을 것."),
		Spline ? Spline->GetNumberOfSplinePoints() : 0);
}

void AFF_FinaleShapeSpline::BakeToAsset()
{
	UFF_FinaleShapeData* Target = TargetShape.LoadSynchronous();

	if (!Target)
	{
		UE_LOG(LogFestaFiestaFinale, Warning,
			TEXT("[피날레] 구울 대상이 없다. TargetShape 에 모양 애셋을 물릴 것. "
				 "애셋은 Content/FestaFiesta/Fireworks/Shapes 아래에 만든다."));
		return;
	}

	TArray<FVector2D> Raw;
	SampleSpline(Raw);

	if (Raw.Num() < 2)
	{
		UE_LOG(LogFestaFiestaFinale, Warning,
			TEXT("[피날레] 스플라인에 점이 모자라 굽지 않는다. 최소 두 점이 필요하다."));
		return;
	}

	const bool bClosed = Spline && Spline->IsClosedLoop();

	/* 되돌리기에 걸리도록 Modify 를 먼저 부른다.

	   실수로 구웠을 때 Ctrl+Z 로 되돌아가지 않으면, 공들여 맞춘 모양을 다시 그려야 한다.
	*/
	Target->Modify();
	Target->SetFromRawPoints(Raw, bClosed);

	/* 저장은 하지 않고 더티 표시만 한다.

	   여기서 SavePackage 를 부르면 이 파일이 에디터 모듈에 의존하게 되고,
	   무엇보다 잘못 구운 것이 그대로 디스크에 박힌다. 저장 시점은 사람이 정한다.
	*/
	Target->MarkPackageDirty();

	UE_LOG(LogFestaFiestaFinale, Log,
		TEXT("[피날레] %s 에 구웠다. 원본 %d점 → 저장 %d점. 저장(Ctrl+S)은 직접 할 것."),
		*Target->GetName(), Raw.Num(), Target->Points.Num());

	TArray<FVector2D> Preview;
	Target->SamplePoints(FMath::Max(1, PreviewShellCount), Preview);
	DrawPreview(Preview, bClosed, MeasureLongestSide(Raw));
}

void AFF_FinaleShapeSpline::PreviewOnly()
{
	TArray<FVector2D> Raw;
	SampleSpline(Raw);

	if (Raw.Num() < 2)
	{
		UE_LOG(LogFestaFiestaFinale, Warning,
			TEXT("[피날레] 스플라인에 점이 모자라 미리보기를 그리지 않는다."));
		return;
	}

	/* 애셋을 거치지 않고 같은 계산을 태운다.

	   임시 객체를 하나 만들어 굽는 이유는, 미리보기가 실제로 구워질 결과와
	   다르면 미리보기를 볼 이유가 없기 때문이다. 정규화와 재샘플 규칙이
	   애셋 쪽에만 있으므로 그 코드를 그대로 쓴다.
	*/
	UFF_FinaleShapeData* Scratch = NewObject<UFF_FinaleShapeData>(GetTransientPackage());
	Scratch->SetFromRawPoints(Raw, Spline && Spline->IsClosedLoop());

	TArray<FVector2D> Preview;
	Scratch->SamplePoints(FMath::Max(1, PreviewShellCount), Preview);

	DrawPreview(Preview, Scratch->bClosed, MeasureLongestSide(Raw));
}

float AFF_FinaleShapeSpline::MeasureLongestSide(const TArray<FVector2D>& Points)
{
	if (Points.Num() == 0)
	{
		return 0.f;
	}

	FVector2D Min = Points[0];
	FVector2D Max = Points[0];

	for (const FVector2D& Point : Points)
	{
		Min.X = FMath::Min(Min.X, Point.X);
		Min.Y = FMath::Min(Min.Y, Point.Y);
		Max.X = FMath::Max(Max.X, Point.X);
		Max.Y = FMath::Max(Max.Y, Point.Y);
	}

	return FMath::Max(Max.X - Min.X, Max.Y - Min.Y);
}

void AFF_FinaleShapeSpline::DrawPreview(
	const TArray<FVector2D>& NormalizedPoints, const bool bClosed, const float LocalLongestSide) const
{
	UWorld* World = GetWorld();
	if (!World || NormalizedPoints.Num() == 0)
	{
		return;
	}

	/* 그린 크기로 되돌려서 스플라인 위에 겹쳐 찍는다.

	   정규화 좌표를 그대로 그리면 1cm 짜리 점 무더기가 된다. 원래 자리에 겹쳐야
	   "이 간격이면 하트로 보이나" 를 눈으로 판단할 수 있다.

	   스플라인의 Bounds 를 쓰지 않는 이유는 그게 월드 기준이라서다. 액터를 돌려놓으면
	   회전이 두 번 먹는다. 지역 좌표로 잰 값을 받아 쓴다.
	*/
	const float Scale = FMath::Max(1.f, LocalLongestSide);

	const FTransform& ToWorld = GetActorTransform();

	TArray<FVector> WorldPoints;
	WorldPoints.Reserve(NormalizedPoints.Num());

	for (const FVector2D& Point : NormalizedPoints)
	{
		const FVector Local = (AuthorPlane == EFF_FinaleShapeAuthorPlane::LocalXZ)
			? FVector(Point.X * Scale, 0.f, Point.Y * Scale)
			: FVector(Point.X * Scale, Point.Y * Scale, 0.f);

		WorldPoints.Add(ToWorld.TransformPosition(Local));
	}

	/* 영구 드로잉으로 찍는다.

	   에디터 뷰포트는 게임처럼 매 틱 돌지 않아서, 수명을 준 일반 드로잉은 한 프레임
	   보이고 말거나 아예 안 보인다. 영구로 찍으면 플러시할 때까지 남는다.
	   치우는 것은 Clear Preview 가 한다.
	*/
	const bool bPersistent = true;
	const float PointSize = FMath::Max(8.f, Scale * 0.02f);

	// 먼저 지운다. 안 그러면 다듬을 때마다 옛 점 위에 새 점이 겹쳐 쌓인다.
	FlushPersistentDebugLines(World);

	for (int32 Index = 0; Index < WorldPoints.Num(); ++Index)
	{
		// 첫 발만 색을 달리한다. 어디서 시작해 어느 쪽으로 도는지 보여야
		// ShellDelaySeconds 로 윤곽선이 그려질 때의 방향을 예상할 수 있다.
		const FColor Color = (Index == 0) ? FColor::Green : FColor::Yellow;

		DrawDebugPoint(World, WorldPoints[Index], PointSize, Color, bPersistent);

		const bool bHasNext = (Index + 1 < WorldPoints.Num()) || bClosed;
		if (bHasNext)
		{
			const FVector& Next = WorldPoints[(Index + 1) % WorldPoints.Num()];
			DrawDebugLine(World, WorldPoints[Index], Next, FColor(80, 80, 80), bPersistent);
		}
	}

	UE_LOG(LogFestaFiestaFinale, Log,
		TEXT("[피날레] 미리보기 %d발. 초록이 첫 발이다. 찍은 자리에 그대로 남으므로 "
			 "액터를 옮기면 따라오지 않는다. 치우려면 Clear Preview."),
		WorldPoints.Num());
}

void AFF_FinaleShapeSpline::ClearPreview()
{
	if (UWorld* World = GetWorld())
	{
		FlushPersistentDebugLines(World);
	}
}

#endif
