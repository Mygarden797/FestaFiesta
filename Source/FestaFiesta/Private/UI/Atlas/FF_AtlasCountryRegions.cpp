#include "UI/Atlas/FF_AtlasCountryRegions.h"

namespace
{
	bool RingContains(const FFF_AtlasRegionRing& Ring, const FVector2D& P)
	{
		if (Ring.Points.Num() < 3 || !FMath::IsFinite(P.X) || !FMath::IsFinite(P.Y)) { return false; }
		bool bInside = false;
		for (int32 I = 0, J = Ring.Points.Num() - 1; I < Ring.Points.Num(); J = I++)
		{
			const FVector2D A = Ring.Points[J], B = Ring.Points[I];
			if (!FMath::IsFinite(A.X) || !FMath::IsFinite(A.Y) || !FMath::IsFinite(B.X) || !FMath::IsFinite(B.Y)) { return false; }
			const FVector2D Edge = B - A, Relative = P - A;
			const double LengthSquared = Edge.SizeSquared();
			if (LengthSquared > UE_DOUBLE_SMALL_NUMBER)
			{
				const double T = FVector2D::DotProduct(Relative, Edge) / LengthSquared;
				if (T >= 0.0 && T <= 1.0 && (Relative - T * Edge).SizeSquared() <= 1.e-16) { return true; }
			}
			if ((A.Y > P.Y) != (B.Y > P.Y) && P.X < A.X + (P.Y - A.Y) * (B.X - A.X) / (B.Y - A.Y))
			{
				bInside = !bInside;
			}
		}
		return bInside;
	}

	bool RayHitsBox(const FBox& Box, const FVector& Origin, const FVector& Direction, double MaxDistance)
	{
		if (!Box.IsValid) { return false; }
		double Enter = 0.0, Exit = MaxDistance;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (FMath::Abs(Direction[Axis]) < 1.e-15)
			{
				if (Origin[Axis] < Box.Min[Axis] || Origin[Axis] > Box.Max[Axis]) { return false; }
				continue;
			}
			double A = (Box.Min[Axis] - Origin[Axis]) / Direction[Axis];
			double B = (Box.Max[Axis] - Origin[Axis]) / Direction[Axis];
			if (A > B) { Swap(A, B); }
			Enter = FMath::Max(Enter, A);
			Exit = FMath::Min(Exit, B);
			if (Enter > Exit) { return false; }
		}
		return true;
	}

	FBox TriangleBoundsAt(const TArray<FVector>& Vertices, const TArray<int32>& Indices, int32 Offset)
	{
		FBox Result(ForceInit);
		if (Offset < 0 || Offset + 2 >= Indices.Num()) { return Result; }
		for (int32 I = 0; I < 3; ++I)
		{
			if (!Vertices.IsValidIndex(Indices[Offset + I]) || Vertices[Indices[Offset + I]].ContainsNaN()) { return FBox(ForceInit); }
			Result += Vertices[Indices[Offset + I]];
		}
		return Result;
	}

	bool TraceTriangle(const TArray<FVector>& Vertices, const TArray<FVector2D>& UVs,
		const TArray<int32>& Indices, int32 Offset, const FVector& Origin, const FVector& Direction,
		double& Nearest, FVector2D& OutUV)
	{
		if (Offset < 0 || Offset + 2 >= Indices.Num()) { return false; }
		const int32 IA = Indices[Offset], IB = Indices[Offset + 1], IC = Indices[Offset + 2];
		if (!Vertices.IsValidIndex(IA) || !Vertices.IsValidIndex(IB) || !Vertices.IsValidIndex(IC)) { return false; }
		const FVector A = Vertices[IA], Edge1 = Vertices[IB] - A, Edge2 = Vertices[IC] - A;
		const FVector P = FVector::CrossProduct(Direction, Edge2);
		const double Determinant = FVector::DotProduct(Edge1, P);
		if (FMath::Abs(Determinant) <= 1.e-12) { return false; }
		const double Inverse = 1.0 / Determinant;
		const FVector T = Origin - A;
		const double U = FVector::DotProduct(T, P) * Inverse;
		if (U < -1.e-9 || U > 1.0 + 1.e-9) { return false; }
		const FVector Q = FVector::CrossProduct(T, Edge1);
		const double V = FVector::DotProduct(Direction, Q) * Inverse;
		if (V < -1.e-9 || U + V > 1.0 + 1.e-9) { return false; }
		const double Distance = FVector::DotProduct(Edge2, Q) * Inverse;
		if (!FMath::IsFinite(Distance) || Distance < 0.0 || Distance >= Nearest) { return false; }
		Nearest = Distance;
		// 셰이더와 같은 삼각형 UV 보간 후 텍스처 주소만 wrap한다. 경계 삼각형을 임의로 재투영하지 않는다.
		OutUV = UVs[IA] * (1.0 - U - V) + UVs[IB] * U + UVs[IC] * V;
		OutUV.X -= FMath::FloorToDouble(OutUV.X);
		return true;
	}
}

bool FFF_AtlasRegionPolygon::Contains(const FVector2D& LongitudeLatitude) const
{
	if (!RingContains(Outer, LongitudeLatitude)) { return false; }
	for (const FFF_AtlasRegionRing& Hole : Holes)
	{
		if (RingContains(Hole, LongitudeLatitude)) { return false; }
	}
	return true;
}

bool FFF_AtlasCountryRegion::Contains(const FVector2D& LongitudeLatitude) const
{
	if (!FMath::IsFinite(LongitudeLatitude.X) || !FMath::IsFinite(LongitudeLatitude.Y)) { return false; }
	FVector2D Point = LongitudeLatitude;
	const double CenterLongitude = (static_cast<double>(Bounds.R) + Bounds.B) * 0.5;
	Point.X += 360.0 * FMath::FloorToDouble((CenterLongitude - Point.X) / 360.0 + 0.5);
	if (Point.X < Bounds.R || Point.X > Bounds.B || Point.Y < Bounds.G || Point.Y > Bounds.A) { return false; }
	for (const FFF_AtlasRegionPolygon& Polygon : Polygons)
	{
		if (Polygon.Contains(Point)) { return true; }
	}
	return false;
}

const FFF_AtlasCountryRegion* UFF_AtlasCountryRegions::FindRegion(FName CountryId) const
{
	if (CountryId.IsNone()) { return nullptr; }
	return Regions.FindByPredicate([CountryId](const FFF_AtlasCountryRegion& Region)
	{
		return Region.CountryId == CountryId && !Region.Polygons.IsEmpty() && Region.Mask;
	});
}

FName UFF_AtlasCountryRegions::FindCountryAtUV(const FVector2D& UV) const
{
	const FVector2D LongitudeLatitude(UV.X * 360.0 - 180.0, 90.0 - UV.Y * 180.0);
	for (const FFF_AtlasCountryRegion& Region : Regions)
	{
		if (Region.Mask && Region.Contains(LongitudeLatitude)) { return Region.CountryId; }
	}
	return NAME_None;
}

bool UFF_AtlasCountryRegions::HasValidSurface() const
{
	if (SurfaceVertices.IsEmpty() || SurfaceUVs.Num() != SurfaceVertices.Num() || SurfaceWeights.Num() != SurfaceVertices.Num()
		|| SurfaceIndices.Num() < 3 || SurfaceIndices.Num() % 3 != 0 || !FMath::IsFinite(SourceRadius) || SourceRadius <= 0.f) { return false; }
	for (int32 I = 0; I < SurfaceVertices.Num(); ++I)
	{
		if (SurfaceVertices[I].ContainsNaN() || !FMath::IsFinite(SurfaceUVs[I].X) || !FMath::IsFinite(SurfaceUVs[I].Y)
			|| !FMath::IsFinite(SurfaceWeights[I])) { return false; }
	}
	for (int32 Index : SurfaceIndices)
	{
		if (!SurfaceVertices.IsValidIndex(Index)) { return false; }
	}
	return FMath::IsFinite(BaseLandThickness) && FMath::IsFinite(BaseMountainHeight);
}

bool FFF_AtlasRegionPicking::TraceSurface(const TArray<FVector>& Vertices, const TArray<FVector2D>& UVs,
	const TArray<int32>& Indices, const FBox& Bounds, const FVector& Origin, const FVector& Direction,
	double MaxDistance, FVector2D& OutUV, double& OutDistance)
{
	if (Origin.ContainsNaN() || Direction.ContainsNaN() || Direction.IsNearlyZero() || MaxDistance <= 0.0
		|| UVs.Num() != Vertices.Num() || !RayHitsBox(Bounds, Origin, Direction, MaxDistance)) { return false; }
	bool bHit = false;
	double Nearest = MaxDistance;
	for (int32 I = 0; I + 2 < Indices.Num(); I += 3)
	{
		bHit = TraceTriangle(Vertices, UVs, Indices, I, Origin, Direction, Nearest, OutUV) || bHit;
	}
	if (bHit) { OutDistance = Nearest; }
	return bHit;
}

void FFF_AtlasRegionSurfaceIndex::Reset()
{
	Nodes.Reset();
	TriangleOffsets.Reset();
}

void FFF_AtlasRegionSurfaceIndex::Build(const TArray<FVector>& Vertices, const TArray<int32>& Indices)
{
	Reset();
	TArray<FBox> TriangleBounds;
	TriangleBounds.Reserve(Indices.Num() / 3);
	TriangleOffsets.Reserve(Indices.Num() / 3);
	for (int32 I = 0; I + 2 < Indices.Num(); I += 3)
	{
		const FBox Bounds = TriangleBoundsAt(Vertices, Indices, I);
		TriangleBounds.Add(Bounds);
		if (Bounds.IsValid) { TriangleOffsets.Add(I); }
	}
	if (!TriangleOffsets.IsEmpty()) { BuildNode(TriangleBounds, 0, TriangleOffsets.Num(), 0); }
}

int32 FFF_AtlasRegionSurfaceIndex::BuildNode(const TArray<FBox>& TriangleBounds, int32 First, int32 Count, int32 Depth)
{
	const int32 NodeIndex = Nodes.AddDefaulted();
	FBox Bounds(ForceInit), Centers(ForceInit);
	for (int32 I = First; I < First + Count; ++I)
	{
		const FBox& Triangle = TriangleBounds[TriangleOffsets[I] / 3];
		Bounds += Triangle;
		Centers += Triangle.GetCenter();
	}
	Nodes[NodeIndex].Bounds = Bounds;
	Nodes[NodeIndex].First = First;
	Nodes[NodeIndex].Count = Count;
	if (Count <= 16 || Depth >= 32) { return NodeIndex; }
	const FVector Extent = Centers.GetSize();
	const int32 Axis = Extent.X >= Extent.Y && Extent.X >= Extent.Z ? 0 : (Extent.Y >= Extent.Z ? 1 : 2);
	const double Middle = Centers.GetCenter()[Axis];
	int32 Split = First;
	for (int32 I = First; I < First + Count; ++I)
	{
		if (TriangleBounds[TriangleOffsets[I] / 3].GetCenter()[Axis] < Middle) { TriangleOffsets.Swap(I, Split++); }
	}
	if (Split == First || Split == First + Count) { Split = First + Count / 2; }
	// 재귀 중 배열 재할당이 가능하므로 FNode 참조를 보관하지 않는다.
	const int32 Left = BuildNode(TriangleBounds, First, Split - First, Depth + 1);
	const int32 Right = BuildNode(TriangleBounds, Split, First + Count - Split, Depth + 1);
	Nodes[NodeIndex].Left = Left;
	Nodes[NodeIndex].Right = Right;
	return NodeIndex;
}

void FFF_AtlasRegionSurfaceIndex::Refit(const TArray<FVector>& Vertices, const TArray<int32>& Indices)
{
	for (int32 I = Nodes.Num() - 1; I >= 0; --I)
	{
		FNode& Node = Nodes[I];
		Node.Bounds = FBox(ForceInit);
		if (Node.Left != INDEX_NONE)
		{
			Node.Bounds += Nodes[Node.Left].Bounds;
			Node.Bounds += Nodes[Node.Right].Bounds;
		}
		else
		{
			for (int32 J = Node.First; J < Node.First + Node.Count; ++J)
			{
				Node.Bounds += TriangleBoundsAt(Vertices, Indices, TriangleOffsets[J]);
			}
		}
	}
}

bool FFF_AtlasRegionSurfaceIndex::Trace(const TArray<FVector>& Vertices, const TArray<FVector2D>& UVs,
	const TArray<int32>& Indices, const FVector& Origin, const FVector& Direction,
	double MaxDistance, FVector2D& OutUV, double& OutDistance, int32* OutTrianglesTested) const
{
	if (OutTrianglesTested) { *OutTrianglesTested = 0; }
	if (!IsValid() || Origin.ContainsNaN() || Direction.ContainsNaN() || Direction.IsNearlyZero()
		|| MaxDistance <= 0.0 || UVs.Num() != Vertices.Num()) { return false; }
	TArray<int32, TInlineAllocator<64>> Pending;
	Pending.Add(0);
	bool bHit = false;
	double Nearest = MaxDistance;
	while (!Pending.IsEmpty())
	{
		const FNode& Node = Nodes[Pending.Pop(EAllowShrinking::No)];
		if (!RayHitsBox(Node.Bounds, Origin, Direction, Nearest)) { continue; }
		if (Node.Left != INDEX_NONE)
		{
			Pending.Add(Node.Left);
			Pending.Add(Node.Right);
			continue;
		}
		for (int32 I = Node.First; I < Node.First + Node.Count; ++I)
		{
			if (OutTrianglesTested) { ++*OutTrianglesTested; }
			bHit = TraceTriangle(Vertices, UVs, Indices, TriangleOffsets[I], Origin, Direction, Nearest, OutUV) || bHit;
		}
	}
	if (bHit) { OutDistance = Nearest; }
	return bHit;
}

double FFF_AtlasRegionPicking::WaterOcclusionDistance(const FVector& Origin, const FVector& Direction, double Radius)
{
	if (Origin.ContainsNaN() || Direction.ContainsNaN() || !FMath::IsFinite(Radius) || Radius <= 0.0) { return 0.0; }
	const double A = Direction.SizeSquared(), B = FVector::DotProduct(Origin, Direction);
	const double C = Origin.SizeSquared() - Radius * Radius;
	if (A <= UE_DOUBLE_SMALL_NUMBER || C <= 0.0) { return 0.0; }
	const double D = B * B - A * C;
	if (D < 0.0 || B >= 0.0) { return TNumericLimits<double>::Max(); }
	return FMath::Max(0.0, (-B - FMath::Sqrt(D)) / A);
}

FVector FFF_AtlasRegionPicking::DisplaceVertex(const FVector& ScaledVertex, const FVector& RootWorldScale,
	double WorldDisplacement, const FVector& LocalActorCenter)
{
	const FVector Radial = ScaledVertex - LocalActorCenter;
	const double WorldRadius = (Radial * RootWorldScale).Size();
	return WorldRadius > UE_DOUBLE_SMALL_NUMBER ? ScaledVertex + Radial * (WorldDisplacement / WorldRadius) : ScaledVertex;
}
