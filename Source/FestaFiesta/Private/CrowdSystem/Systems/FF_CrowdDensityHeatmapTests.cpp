// Fill out your copyright notice in the Description page of Project Settings.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdSystem/Systems/FF_CrowdDensityGrid.h"
#include "CrowdSystem/Systems/FF_CrowdDensityHeatmapSubsystem.h"
#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"

namespace FFF_HeatmapTestHelpers
{
	void AddUnitsAt(TArray<FUnitData>& Units, const FVector& Position, const int32 Count)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FUnitData Unit;
			Unit.Position = Position;
			Unit.PrevPosition = Position;
			Unit.bIsActive = true;
			Units.Add(Unit);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdHeatmap_UnbuiltGridGivesNoPixels, "FestaFiesta.Crowd.Heatmap.UnbuiltGridGivesNoPixels", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdHeatmap_UnbuiltGridGivesNoPixels::RunTest(const FString& Parameters)
{
	const FCrowdDensityGrid Grid;
	TArray<FColor> Pixels;
	Pixels.Add(FColor::White);

	UFF_CrowdDensityHeatmapSubsystem::BuildPixels(Grid, 0.05f, 160, Pixels);

	TestEqual(TEXT("격자가 없으면 픽셀도 없다"), Pixels.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdHeatmap_PixelsFollowCellIndexAndLOSColor, "FestaFiesta.Crowd.Heatmap.PixelsFollowCellIndexAndLOSColor", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdHeatmap_PixelsFollowCellIndexAndLOSColor::RunTest(const FString& Parameters)
{
	FCrowdDensityGrid Grid;
	Grid.Build(FVector::ZeroVector, FVector2D(1000.f, 1000.f), 200.f);

	TArray<FUnitData> Units;
	const FVector CellCenter(3 * 200.f + 100.f, 1 * 200.f + 100.f, 0.f);
	FFF_HeatmapTestHelpers::AddUnitsAt(Units, CellCenter, 8);
	Grid.Update(Units);

	TArray<FColor> Pixels;
	UFF_CrowdDensityHeatmapSubsystem::BuildPixels(Grid, 0.05f, 160, Pixels);

	TestEqual(TEXT("픽셀 수 = 칸 수"), Pixels.Num(), Grid.GetNumX() * Grid.GetNumY());

	const int32 FilledIndex = 1 * Grid.GetNumX() + 3;
	if (!Pixels.IsValidIndex(FilledIndex))
	{
		return false;
	}

	FColor Expected = FCrowdDensityGrid::LOSToColor(FCrowdDensityGrid::DensityToLOS(Grid.GetDensityAtLocation(CellCenter)));
	Expected.A = 160;
	TestEqual(TEXT("채운 칸은 Y*NumX+X 자리에 LOS 색으로 찍힌다"), Pixels[FilledIndex], Expected);

	for (int32 Index = 0; Index < Pixels.Num(); ++Index)
	{
		if (Index != FilledIndex)
		{
			TestEqual(*FString::Printf(TEXT("빈 칸 %d 은 투명"), Index), Pixels[Index].A, static_cast<uint8>(0));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdHeatmap_GroundHeightsFollowCellCenters, "FestaFiesta.Crowd.Heatmap.GroundHeightsFollowCellCenters", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdHeatmap_GroundHeightsFollowCellCenters::RunTest(const FString& Parameters)
{
	FCrowdDensityGrid Grid;
	Grid.Build(FVector::ZeroVector, FVector2D(1000.f, 1000.f), 200.f);

	TArray<float> Heights;
	UFF_CrowdDensityHeatmapSubsystem::BuildGroundHeights(Grid, [](const FVector& WorldLocation, double& OutZ)
	{
		if (WorldLocation.X > 800.0)
		{
			return false;
		}
		OutZ = WorldLocation.X + WorldLocation.Y * 0.01;
		return true;
	}, Heights);

	TestEqual(TEXT("높이 수 = 칸 수"), Heights.Num(), Grid.GetNumX() * Grid.GetNumY());

	const int32 Index = 1 * Grid.GetNumX() + 3;
	if (!Heights.IsValidIndex(Index))
	{
		return false;
	}
	const FVector Center = Grid.GetCellCenterWorld(3, 1);
	TestEqual(TEXT("칸 높이는 칸 중심에서 읽은 지면 높이다"), Heights[Index], static_cast<float>(Center.X + Center.Y * 0.01));

	const int32 LastX = Grid.GetNumX() - 1;
	if (Grid.GetCellCenterWorld(LastX, 0).X > 800.0)
	{
		TestEqual(TEXT("지면을 못 찾은 칸은 가리지 않도록 아주 높은 값을 쓴다"),
			Heights[LastX], UFF_CrowdDensityHeatmapSubsystem::NoGroundHeightUU);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
