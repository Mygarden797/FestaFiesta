// Fill out your copyright notice in the Description page of Project Settings.

#include "CrowdSystem/Systems/FF_CrowdDensityHeatmapSubsystem.h"

#include "Components/DecalComponent.h"
#include "CrowdSystem/Systems/FF_CrowdDensityGrid.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "CrowdSystem/Types/FF_SimulationSettings.h"
#include "Engine/DecalActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

const FName UFF_CrowdDensityHeatmapSubsystem::TextureParamName(TEXT("DensityTex"));
const FName UFF_CrowdDensityHeatmapSubsystem::GridOriginParamName(TEXT("GridOrigin"));
const FName UFF_CrowdDensityHeatmapSubsystem::GridSizeParamName(TEXT("GridSize"));
const FName UFF_CrowdDensityHeatmapSubsystem::GroundHeightParamName(TEXT("GroundHeightTex"));
const FName UFF_CrowdDensityHeatmapSubsystem::GroundToleranceParamName(TEXT("GroundTolerance"));

bool UFF_CrowdDensityHeatmapSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UFF_CrowdDensityHeatmapSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UFF_SimulationSettings* Settings = UFF_SimulationSettings::Get();
	if (Settings && !Settings->DensityHeatmapMaterial.IsNull())
	{
		BaseMaterial = Settings->DensityHeatmapMaterial.LoadSynchronous();
		if (!BaseMaterial)
		{
			UE_LOG(LogTemp, Error, TEXT("[CrowdHeatmap] DensityHeatmapMaterial '%s'을 불러오지 못했다 — 히트맵을 켤 수 없다"),
				*Settings->DensityHeatmapMaterial.ToString());
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrowdHeatmap] Project Settings > FestaFiesta Simulation > DensityHeatmapMaterial 미설정 — 히트맵을 켤 수 없다"));
	}
}

void UFF_CrowdDensityHeatmapSubsystem::Deinitialize()
{
	ReleaseResources();
	BaseMaterial = nullptr;

	Super::Deinitialize();
}

TStatId UFF_CrowdDensityHeatmapSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_CrowdDensityHeatmapSubsystem, STATGROUP_Tickables);
}

const UFF_CrowdSimulationSubsystem* UFF_CrowdDensityHeatmapSubsystem::FindCrowd() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;
}

bool UFF_CrowdDensityHeatmapSubsystem::IsDensityGridReady() const
{
	const UFF_CrowdSimulationSubsystem* Crowd = FindCrowd();
	return Crowd && Crowd->GetDensityGrid().IsBuilt();
}

bool UFF_CrowdDensityHeatmapSubsystem::SetHeatmapVisible(const bool bVisible)
{
	if (!bVisible)
	{
		bWantVisible = false;
		ApplyDecalVisibility(false);
		return false;
	}

	const UFF_CrowdSimulationSubsystem* Crowd = FindCrowd();
	if (!Crowd || !Crowd->GetDensityGrid().IsBuilt())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrowdHeatmap] 밀도 격자가 준비되지 않아 히트맵을 켤 수 없다"));
		bWantVisible = false;
		return false;
	}

	if (!EnsureResources(*Crowd))
	{
		bWantVisible = false;
		return false;
	}

	bGroundHeightsDirty = true;
	UploadGroundHeights(*Crowd);
	UploadPixels(Crowd->GetDensityGrid());
	TimeSinceRefresh = 0.f;
	bWantVisible = true;
	ApplyDecalVisibility(true);
	return true;
}

void UFF_CrowdDensityHeatmapSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bWantVisible)
	{
		return;
	}

	const UFF_CrowdSimulationSubsystem* Crowd = FindCrowd();
	if (!Crowd || !Crowd->GetDensityGrid().IsBuilt() || !EnsureResources(*Crowd))
	{
		ApplyDecalVisibility(false);
		return;
	}

	ApplyDecalVisibility(true);
	UploadGroundHeights(*Crowd);

	TimeSinceRefresh += DeltaTime;
	if (TimeSinceRefresh < RefreshIntervalSeconds)
	{
		return;
	}

	TimeSinceRefresh = 0.f;
	UploadPixels(Crowd->GetDensityGrid());
}

void UFF_CrowdDensityHeatmapSubsystem::BuildPixels(const FCrowdDensityGrid& Grid, const float MinDensity,
	const uint8 Alpha, TArray<FColor>& OutPixels)
{
	const TArray<float>& Density = Grid.GetDensityField();
	const int32 NumCells = Grid.GetNumX() * Grid.GetNumY();

	OutPixels.SetNumUninitialized(NumCells);
	for (int32 Index = 0; Index < NumCells; ++Index)
	{
		const float CellDensity = Density.IsValidIndex(Index) ? Density[Index] : 0.f;
		if (CellDensity <= MinDensity)
		{
			OutPixels[Index] = FColor(0, 0, 0, 0);
			continue;
		}

		FColor Color = FCrowdDensityGrid::LOSToColor(FCrowdDensityGrid::DensityToLOS(CellDensity));
		Color.A = Alpha;
		OutPixels[Index] = Color;
	}
}

void UFF_CrowdDensityHeatmapSubsystem::BuildGroundHeights(const FCrowdDensityGrid& Grid,
	TFunctionRef<bool(const FVector& WorldLocation, double& OutZ)> ResolveGroundZ, TArray<float>& OutHeights)
{
	const int32 NumX = Grid.GetNumX();
	const int32 NumY = Grid.GetNumY();
	OutHeights.SetNumUninitialized(NumX * NumY);
	for (int32 Y = 0; Y < NumY; ++Y)
	{
		for (int32 X = 0; X < NumX; ++X)
		{
			double GroundZ = 0.0;
			OutHeights[Y * NumX + X] = ResolveGroundZ(Grid.GetCellCenterWorld(X, Y), GroundZ)
				? static_cast<float>(GroundZ) : NoGroundHeightUU;
		}
	}
}

bool UFF_CrowdDensityHeatmapSubsystem::EnsureResources(const UFF_CrowdSimulationSubsystem& Crowd)
{
	const FCrowdDensityGrid& Grid = Crowd.GetDensityGrid();
	const FBox& Bounds = Crowd.GetSimAreaBounds();
	const int32 NumX = Grid.GetNumX();
	const int32 NumY = Grid.GetNumY();
	const float CellSize = Grid.GetCellSizeUU();

	const bool bLayoutUnchanged = HeatmapTexture && GroundHeightTexture && HeatmapMaterial && IsValid(HeatmapDecal)
		&& BuiltNumX == NumX && BuiltNumY == NumY
		&& BuiltCellSizeUU == CellSize
		&& BuiltOriginUU.Equals(Grid.GetOriginUU())
		&& BuiltBoundsCm == Bounds;
	if (bLayoutUnchanged)
	{
		return true;
	}

	if (!BaseMaterial)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World || NumX <= 0 || NumY <= 0)
	{
		return false;
	}

	if (!HeatmapTexture || BuiltNumX != NumX || BuiltNumY != NumY)
	{
		HeatmapTexture = UTexture2D::CreateTransient(NumX, NumY, PF_B8G8R8A8, TEXT("CrowdDensityHeatmap"));
		if (!HeatmapTexture)
		{
			UE_LOG(LogTemp, Error, TEXT("[CrowdHeatmap] %dx%d 텍스처를 만들지 못했다"), NumX, NumY);
			return false;
		}

		HeatmapTexture->Filter = TF_Nearest;
		HeatmapTexture->SRGB = true;
		HeatmapTexture->AddressX = TA_Clamp;
		HeatmapTexture->AddressY = TA_Clamp;
		HeatmapTexture->CompressionSettings = TC_VectorDisplacementmap;
		HeatmapTexture->NeverStream = true;
#if WITH_EDITORONLY_DATA
		HeatmapTexture->MipGenSettings = TMGS_NoMipmaps;
#endif
		HeatmapTexture->UpdateResource();
	}

	if (!GroundHeightTexture || BuiltNumX != NumX || BuiltNumY != NumY)
	{
		GroundHeightTexture = UTexture2D::CreateTransient(NumX, NumY, PF_R32_FLOAT, TEXT("CrowdHeatmapGroundHeight"));
		if (!GroundHeightTexture)
		{
			UE_LOG(LogTemp, Error, TEXT("[CrowdHeatmap] %dx%d 지면 높이 텍스처를 만들지 못했다"), NumX, NumY);
			return false;
		}

		GroundHeightTexture->Filter = TF_Bilinear;
		GroundHeightTexture->SRGB = false;
		GroundHeightTexture->AddressX = TA_Clamp;
		GroundHeightTexture->AddressY = TA_Clamp;
		GroundHeightTexture->CompressionSettings = TC_HDR;
		GroundHeightTexture->NeverStream = true;
#if WITH_EDITORONLY_DATA
		GroundHeightTexture->MipGenSettings = TMGS_NoMipmaps;
#endif
		GroundHeightTexture->UpdateResource();
	}
	bGroundHeightsDirty = true;

	if (!HeatmapMaterial)
	{
		HeatmapMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	}

	const FVector& Origin = Grid.GetOriginUU();
	const double SizeX = NumX * static_cast<double>(CellSize);
	const double SizeY = NumY * static_cast<double>(CellSize);

	HeatmapMaterial->SetTextureParameterValue(TextureParamName, HeatmapTexture);
	HeatmapMaterial->SetTextureParameterValue(GroundHeightParamName, GroundHeightTexture);
	HeatmapMaterial->SetScalarParameterValue(GroundToleranceParamName, GroundToleranceUU);
	HeatmapMaterial->SetVectorParameterValue(GridOriginParamName, FLinearColor(Origin.X, Origin.Y, 0.f, 0.f));
	HeatmapMaterial->SetVectorParameterValue(GridSizeParamName, FLinearColor(SizeX, SizeY, 0.f, 0.f));

	if (!IsValid(HeatmapDecal))
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		HeatmapDecal = World->SpawnActor<ADecalActor>(ADecalActor::StaticClass(), FTransform::Identity, Params);
		if (!HeatmapDecal)
		{
			UE_LOG(LogTemp, Error, TEXT("[CrowdHeatmap] 데칼 액터를 스폰하지 못했다"));
			return false;
		}
		HeatmapDecal->SetActorHiddenInGame(true);
	}

	const double CenterZ = Bounds.IsValid ? Bounds.GetCenter().Z : Origin.Z;
	const double HalfDepth = (Bounds.IsValid ? Bounds.GetExtent().Z : 0.0) + ProjectionMarginUU;
	const FVector Center(Origin.X + SizeX * 0.5, Origin.Y + SizeY * 0.5, CenterZ);

	UDecalComponent* Decal = HeatmapDecal->GetDecal();
	Decal->SetMobility(EComponentMobility::Movable);
	Decal->SetDecalMaterial(HeatmapMaterial);
	Decal->SetFadeScreenSize(0.f);
	Decal->DecalSize = FVector(HalfDepth, SizeY * 0.5, SizeX * 0.5);
	HeatmapDecal->SetActorLocationAndRotation(Center, FRotator(-90.f, 0.f, 0.f));
	Decal->MarkRenderStateDirty();

	BuiltNumX = NumX;
	BuiltNumY = NumY;
	BuiltCellSizeUU = CellSize;
	BuiltOriginUU = Origin;
	BuiltBoundsCm = Bounds;
	return true;
}

void UFF_CrowdDensityHeatmapSubsystem::UploadPixels(const FCrowdDensityGrid& Grid)
{
	if (!HeatmapTexture || Grid.GetNumX() != BuiltNumX || Grid.GetNumY() != BuiltNumY)
	{
		return;
	}

	TArray<FColor> Pixels;
	BuildPixels(Grid, MinDensityToDraw, CellAlpha, Pixels);

	const int32 NumBytes = Pixels.Num() * sizeof(FColor);
	uint8* Data = new uint8[NumBytes];
	FMemory::Memcpy(Data, Pixels.GetData(), NumBytes);

	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, BuiltNumX, BuiltNumY);
	HeatmapTexture->UpdateTextureRegions(0, 1, Region, BuiltNumX * sizeof(FColor), sizeof(FColor), Data,
		[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
		{
			delete[] SrcData;
			delete Regions;
		});
}

void UFF_CrowdDensityHeatmapSubsystem::UploadGroundHeights(const UFF_CrowdSimulationSubsystem& Crowd)
{
	const FCrowdDensityGrid& Grid = Crowd.GetDensityGrid();
	if (!bGroundHeightsDirty || !GroundHeightTexture || Grid.GetNumX() != BuiltNumX || Grid.GetNumY() != BuiltNumY)
	{
		return;
	}
	bGroundHeightsDirty = false;

	TArray<float> Heights;
	BuildGroundHeights(Grid, [&Crowd](const FVector& WorldLocation, double& OutZ)
	{
		return Crowd.TryGetGroundZ(WorldLocation, OutZ);
	}, Heights);

	const int32 NumBytes = Heights.Num() * sizeof(float);
	uint8* Data = new uint8[NumBytes];
	FMemory::Memcpy(Data, Heights.GetData(), NumBytes);

	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, BuiltNumX, BuiltNumY);
	GroundHeightTexture->UpdateTextureRegions(0, 1, Region, BuiltNumX * sizeof(float), sizeof(float), Data,
		[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
		{
			delete[] SrcData;
			delete Regions;
		});
}

void UFF_CrowdDensityHeatmapSubsystem::ApplyDecalVisibility(const bool bVisible)
{
	if (IsValid(HeatmapDecal) && HeatmapDecal->IsHidden() == bVisible)
	{
		HeatmapDecal->SetActorHiddenInGame(!bVisible);
	}
}

void UFF_CrowdDensityHeatmapSubsystem::ReleaseResources()
{
	const UWorld* World = GetWorld();
	if (IsValid(HeatmapDecal) && World && !World->bIsTearingDown)
	{
		HeatmapDecal->Destroy();
	}
	HeatmapDecal = nullptr;
	HeatmapMaterial = nullptr;
	HeatmapTexture = nullptr;
	GroundHeightTexture = nullptr;
	bGroundHeightsDirty = true;
	BuiltNumX = 0;
	BuiltNumY = 0;
	BuiltCellSizeUU = 0.f;
	BuiltOriginUU = FVector::ZeroVector;
	BuiltBoundsCm = FBox(ForceInit);
}
