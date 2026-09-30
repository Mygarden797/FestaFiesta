#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "UI/Atlas/FF_AtlasCountryRegions.h"
#include "Engine/Texture2D.h"
#include <limits>

namespace FFAtlasRegionTests
{
	FFF_AtlasRegionRing Rectangle(double MinLon, double MinLat, double MaxLon, double MaxLat)
	{
		FFF_AtlasRegionRing Ring;
		Ring.Points = { FVector2D(MinLon, MinLat), FVector2D(MaxLon, MinLat), FVector2D(MaxLon, MaxLat), FVector2D(MinLon, MaxLat) };
		return Ring;
	}

	UFF_AtlasCountryRegions* MakeSyntheticData()
	{
		UFF_AtlasCountryRegions* Data = NewObject<UFF_AtlasCountryRegions>();
		FFF_AtlasCountryRegion Region;
		Region.CountryId = TEXT("KR");
		Region.Bounds = FLinearColor(125, 33, 131, 39);
		Region.Mask = NewObject<UTexture2D>(Data);
		FFF_AtlasRegionPolygon Mainland;
		Mainland.Outer = Rectangle(126, 34, 130, 38);
		Mainland.Holes.Add(Rectangle(128.5, 36.5, 129, 37));
		FFF_AtlasRegionPolygon Island;
		Island.Outer = Rectangle(126, 33, 127, 33.5);
		Region.Polygons = { Mainland, Island };
		Data->Regions.Add(Region);
		// 지리 위치와 일부러 다른 삼각형으로 방사 위경도에 의존하지 않는지 검증한다.
		Data->SurfaceVertices = { FVector(5100, -100, -100), FVector(5100, 100, -100), FVector(5100, 0, 100) };
		Data->SurfaceUVs = { FVector2D(306.0 / 360.0, 56.0 / 180.0), FVector2D(310.0 / 360.0, 56.0 / 180.0), FVector2D(308.0 / 360.0, 52.0 / 180.0) };
		Data->SurfaceWeights = { 0.f, 0.5f, 1.f };
		Data->SurfaceIndices = { 0, 1, 2 };
		return Data;
	}

	FBox MakeBounds(const TArray<FVector>& Vertices)
	{
		FBox Bounds(ForceInit);
		for (const FVector& Vertex : Vertices) { Bounds += Vertex; }
		return Bounds;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasCountryRegionPolygons,
	"FestaFiesta.UI.AtlasRegions.PolygonsHolesAndIslands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_AtlasCountryRegionPolygons::RunTest(const FString& Parameters)
{
	using namespace FFAtlasRegionTests;
	UFF_AtlasCountryRegions* Data = MakeSyntheticData();
	const FFF_AtlasCountryRegion& Region = Data->Regions[0];
	TestTrue(TEXT("본토 내부"), Region.Contains(FVector2D(128, 36)));
	TestTrue(TEXT("합성 제주형 분리 섬도 같은 국가"), Region.Contains(FVector2D(126.5, 33.25)));
	TestFalse(TEXT("본토와 섬 사이 바다는 제외"), Region.Contains(FVector2D(126.5, 33.75)));
	TestFalse(TEXT("내부 구멍은 제외"), Region.Contains(FVector2D(128.75, 36.75)));
	TestFalse(TEXT("구멍 경계도 제외"), Region.Contains(FVector2D(128.5, 36.75)));
	TestTrue(TEXT("외곽선 경계 포함"), Region.Contains(FVector2D(126, 36)));
	TestFalse(TEXT("범위 밖"), Region.Contains(FVector2D(140, 36)));
	TestFalse(TEXT("비유한 좌표 제외"), Region.Contains(FVector2D(std::numeric_limits<double>::quiet_NaN(), 36)));
	FFF_AtlasRegionPolygon Closed = Region.Polygons[0];
	const FVector2D FirstPoint = Closed.Outer.Points[0];
	Closed.Outer.Points.Add(FirstPoint);
	TestTrue(TEXT("첫 점을 반복한 닫힌 경계도 동일"), Closed.Contains(FVector2D(128, 36)));
	TestTrue(TEXT("표면 배열 계약"), Data->HasValidSurface());
	Data->SurfaceIndices.Add(2);
	TestFalse(TEXT("불완전한 삼각형은 모드 진입 전에 차단"), Data->HasValidSurface());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasCountryRegionSurface,
	"FestaFiesta.UI.AtlasRegions.SurfaceUVNearestAndWaterOcclusion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_AtlasCountryRegionSurface::RunTest(const FString& Parameters)
{
	using namespace FFAtlasRegionTests;
	UFF_AtlasCountryRegions* Data = MakeSyntheticData();
	FVector2D UV;
	double Distance = 0.0;
	const FVector Origin(15000, 0, 0), Direction(-1, 0, 0);
	const auto Trace = [&](const FVector& O, const FVector& D)
	{
		return FFF_AtlasRegionPicking::TraceSurface(Data->SurfaceVertices, Data->SurfaceUVs, Data->SurfaceIndices,
			MakeBounds(Data->SurfaceVertices), O, D, FFF_AtlasRegionPicking::WaterOcclusionDistance(O, D, 5000), UV, Distance);
	};
	TestTrue(TEXT("물보다 앞에 있는 Top 삼각형"), Trace(Origin, Direction));
	TestTrue(TEXT("삼각형의 무게중심 UV 보간"), UV.Equals(FVector2D(308.0 / 360.0, 54.0 / 180.0), 1.e-10));
	TestEqual(TEXT("방사 위치는 0도여도 보간 UV의 한국을 선택"), Data->FindCountryAtUV(UV), FName(TEXT("KR")));
	TestEqual(TEXT("월드 광선 거리"), Distance, 9900.0);
	TestFalse(TEXT("물구체 뒤 반대편 영역 차단"), Trace(-Origin, -Direction));
	TestFalse(TEXT("영역 바운드와 만나지 않는 광선"), Trace(Origin + FVector(0, 1000, 0), Direction));
	TestFalse(TEXT("영벡터 광선"), Trace(Origin, FVector::ZeroVector));
	TestFalse(TEXT("비유한 광선"), Trace(FVector(std::numeric_limits<double>::infinity(), 0, 0), Direction));

	// 더 가까운 육지가 한국 외부면 뒤쪽 한국 삼각형을 뚫고 고르면 안 된다.
	for (int32 I = 0; I < 3; ++I)
	{
		Data->SurfaceVertices.Add(Data->SurfaceVertices[I] + FVector(100, 0, 0));
		Data->SurfaceUVs.Add(FVector2D(0.5, 0.5));
	}
	Data->SurfaceIndices.Append({ 3, 4, 5 });
	TestTrue(TEXT("앞 육지가 먼저 검출됨"), Trace(Origin, Direction));
	TestEqual(TEXT("가까운 외부 육지에서 판정 종료"), Distance, 9800.0);
	TestTrue(TEXT("외부 육지 UV는 선택 없음"), Data->FindCountryAtUV(UV).IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasCountryRegionDateline,
	"FestaFiesta.UI.AtlasRegions.DatelineIslandsAndIndependentCountries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_AtlasCountryRegionDateline::RunTest(const FString& Parameters)
{
	using namespace FFAtlasRegionTests;
	UFF_AtlasCountryRegions* Data = MakeSyntheticData();
	FFF_AtlasCountryRegion Region;
	Region.CountryId = TEXT("US");
	Region.Bounds = FLinearColor(-190, 18, -65, 72);
	Region.Mask = NewObject<UTexture2D>(Data);
	FFF_AtlasRegionPolygon Mainland, WestIsland, CrossingIsland;
	Mainland.Outer = Rectangle(-125, 25, -67, 49);
	WestIsland.Outer = Rectangle(-188, 51, -186, 53);
	CrossingIsland.Outer = Rectangle(-181, 50, -179, 54);
	CrossingIsland.Holes.Add(Rectangle(-180.5, 51, -179.5, 52));
	Region.Polygons = { Mainland, WestIsland, CrossingIsland };
	Data->Regions.Add(Region);
	const auto CountryAt = [Data](double Lon, double Lat)
	{
		return Data->FindCountryAtUV(FVector2D((Lon + 180.0) / 360.0, (90.0 - Lat) / 180.0));
	};
	TestEqual(TEXT("기존 한국 경계 유지"), CountryAt(128, 36), FName(TEXT("KR")));
	TestEqual(TEXT("미국 본토"), CountryAt(-100, 40), FName(TEXT("US")));
	TestEqual(TEXT("양수 경도 섬을 음수 연속 바운드로 변환"), CountryAt(173, 52), FName(TEXT("US")));
	TestEqual(TEXT("날짜변경선 동쪽"), CountryAt(-179.25, 53), FName(TEXT("US")));
	TestEqual(TEXT("날짜변경선 서쪽"), CountryAt(179.25, 53), FName(TEXT("US")));
	TestEqual(TEXT("u=0 이음선"), CountryAt(-180, 53), FName(TEXT("US")));
	TestEqual(TEXT("u=1 이음선도 동일 국가"), CountryAt(180, 53), FName(TEXT("US")));
	TestTrue(TEXT("이음선 내부 구멍"), CountryAt(180, 51.5).IsNone());
	TestTrue(TEXT("섬 사이 바다"), CountryAt(176, 52).IsNone());
	TestTrue(TEXT("바운드 반대편 다른 대륙 제외"), CountryAt(20, 40).IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasCountryRegionAcceleration,
	"FestaFiesta.UI.AtlasRegions.AcceleratedNearestSurfaceAndWPORefit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_AtlasCountryRegionAcceleration::RunTest(const FString& Parameters)
{
	using namespace FFAtlasRegionTests;
	TArray<FVector> Vertices;
	TArray<FVector2D> UVs;
	TArray<int32> Indices;
	const auto AddTriangle = [&](double X, double Y, double Z, const FVector2D& UV)
	{
		const int32 First = Vertices.Num();
		Vertices.Append({ FVector(X, Y - 6, Z - 6), FVector(X, Y + 6, Z - 6), FVector(X, Y, Z + 6) });
		UVs.Append({ UV, UV, UV });
		Indices.Append({ First, First + 1, First + 2 });
	};
	for (int32 Y = -64; Y < 64; ++Y)
	{
		for (int32 Z = -64; Z < 64; ++Z) { AddTriangle(5100, Y * 20, Z * 20, FVector2D(0.25, 0.4)); }
	}
	// 배열 마지막에 있어도 가까운 삼각형이 이긴다. 실제 UV wrap도 같은 경로로 검사한다.
	AddTriangle(5200, 0, 0, FVector2D(1.001, 0.3));
	FFF_AtlasRegionSurfaceIndex Index;
	Index.Build(Vertices, Indices);
	FVector2D UV, LinearUV;
	double Distance = 0, LinearDistance = 0;
	int32 TrianglesTested = 0;
	const FVector Origin(15000, 0, 0), Direction(-1, 0, 0);
	const double Water = FFF_AtlasRegionPicking::WaterOcclusionDistance(Origin, Direction, 5000);
	TestTrue(TEXT("전체 표면에서 가까운 육지를 찾음"), Index.Trace(Vertices, UVs, Indices, Origin, Direction, Water, UV, Distance, &TrianglesTested));
	TestEqual(TEXT("배열 순서와 관계없는 최근접 거리"), Distance, 9800.0);
	TestTrue(TEXT("UV는 이음선에서 wrap"), UV.Equals(FVector2D(0.001, 0.3), 1.e-10));
	TestTrue(TEXT("16,385개 삼각형을 전수 검사하지 않음"), TrianglesTested > 0 && TrianglesTested < 256);
	TestFalse(TEXT("반대편 물구체를 통과하지 않음"), Index.Trace(Vertices, UVs, Indices, -Origin, -Direction,
		FFF_AtlasRegionPicking::WaterOcclusionDistance(-Origin, -Direction, 5000), UV, Distance));
	// WPO가 이전 바운드 밖으로 표면을 움직여도 topology 재구축 없이 정확하게 찾는다.
	for (FVector& Vertex : Vertices) { Vertex += FVector(350, 10000, 0); }
	Index.Refit(Vertices, Indices);
	const FVector MovedOrigin = Origin + FVector(0, 10000, 0);
	const double Max = TNumericLimits<double>::Max();
	TestTrue(TEXT("WPO refit 후 이동한 표면"), Index.Trace(Vertices, UVs, Indices, MovedOrigin, Direction, Max, UV, Distance));
	TestTrue(TEXT("전수 검사 기준과 비교"), FFF_AtlasRegionPicking::TraceSurface(Vertices, UVs, Indices,
		MakeBounds(Vertices), MovedOrigin, Direction, Max, LinearUV, LinearDistance));
	TestEqual(TEXT("refit 최근접 거리"), Distance, 9450.0);
	TestEqual(TEXT("가속/전수 검사 거리 일치"), Distance, LinearDistance);
	TestTrue(TEXT("가속/전수 검사 UV 일치"), UV.Equals(LinearUV, 1.e-10));
	TestFalse(TEXT("이전 바운드에 가짜 충돌을 남기지 않음"), Index.Trace(Vertices, UVs, Indices, Origin, Direction, Max, UV, Distance));
	Index.Reset();
	TestFalse(TEXT("화면 종료 시 인덱스 해제"), Index.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasCountryRegionTransforms,
	"FestaFiesta.UI.AtlasRegions.TransformedSurfaceAndWorldDisplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_AtlasCountryRegionTransforms::RunTest(const FString& Parameters)
{
	using namespace FFAtlasRegionTests;
	UFF_AtlasCountryRegions* Data = MakeSyntheticData();
	const FTransform Transform(FRotator(23, 71, -12), FVector(800, -400, 200), FVector(1.7, 0.8, 1.2));
	const FVector LocalCenter(120, 50, -30);
	const FVector ActorCenter = Transform.TransformPosition(LocalCenter);
	const double Delta = 125.0;
	for (const FVector& Vertex : Data->SurfaceVertices)
	{
		const FVector Deformed = FFF_AtlasRegionPicking::DisplaceVertex(Vertex, Transform.GetScale3D(), Delta, LocalCenter);
		const FVector World = Transform.TransformPosition(Vertex);
		const FVector Expected = World + (World - ActorCenter).GetSafeNormal() * Delta;
		TestTrue(TEXT("비균일 scale·offset actor 기준 월드 WPO 일치"), Transform.TransformPosition(Deformed).Equals(Expected, 1.e-8));
	}
	const FVector WorldOrigin = Transform.TransformPosition(FVector(15000, 0, 0));
	const FVector WorldDirection = Transform.TransformVector(FVector(-1, 0, 0)).GetSafeNormal();
	const FVector LocalOrigin = Transform.InverseTransformPosition(WorldOrigin);
	const FVector LocalDirection = Transform.InverseTransformVector(WorldDirection);
	FVector2D UV;
	double Distance = 0.0;
	TestTrue(TEXT("이동·회전·비균일 scale 광선"), FFF_AtlasRegionPicking::TraceSurface(Data->SurfaceVertices,
		Data->SurfaceUVs, Data->SurfaceIndices, MakeBounds(Data->SurfaceVertices), LocalOrigin, LocalDirection,
		FFF_AtlasRegionPicking::WaterOcclusionDistance(LocalOrigin, LocalDirection, 5000), UV, Distance));
	TestTrue(TEXT("역변환 뒤 정규화하지 않아 월드 거리 유지"), FMath::IsNearlyEqual(Distance, 9900.0 * 1.7, 1.e-6));
	TestEqual(TEXT("회전 후에도 같은 국가"), Data->FindCountryAtUV(UV), FName(TEXT("KR")));
	return true;
}

#if WITH_EDITOR
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/Atlas/FF_AtlasGlobeActor.h"
#include "UI/Atlas/FF_FestivalAtlasCatalog.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_AtlasCountryRegionLifecycle,
	"FestaFiesta.UI.AtlasRegions.ModeFallbackRebuildAndRestoration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_AtlasCountryRegionLifecycle::RunTest(const FString& Parameters)
{
	using namespace FFAtlasRegionTests;
	const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
		.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	AFF_AtlasGlobeActor* Globe = World->SpawnActor<AFF_AtlasGlobeActor>();
	UFF_FestivalAtlasCatalog* Catalog = NewObject<UFF_FestivalAtlasCatalog>(Globe);
	FFF_AtlasCountryEntry KR, JP;
	KR.CountryId = TEXT("KR");
	KR.Location = FFF_AtlasLatLong(0.f, 0.f);
	JP.CountryId = TEXT("JP");
	JP.Location = FFF_AtlasLatLong(0.f, 35.f);
	Catalog->Countries = { KR, JP };
	FindFProperty<FSoftObjectProperty>(Globe->GetClass(), TEXT("CatalogOverride"))->SetObjectPropertyValue_InContainer(Globe, Catalog);
	Globe->RebuildPins();
	Globe->SetOrientation(FFF_AtlasGlobeOrientation());
	Globe->SetAutoSpinPaused(true);
	UFF_AtlasCountryRegions* Data = MakeSyntheticData();
	Data->TargetMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	Data->SelectionMaterial = Data->TargetMaterial;
	FObjectProperty* DataProperty = FindFProperty<FObjectProperty>(Globe->GetClass(), TEXT("RegionData"));
	DataProperty->SetObjectPropertyValue_InContainer(Globe, Data);
	TInlineComponentArray<UStaticMeshComponent*> Components(Globe);
	UStaticMeshComponent* Top = nullptr;
	for (UStaticMeshComponent* Component : Components)
	{
		if (Component->GetFName() == TEXT("LandLayer00")) { Top = Component; break; }
	}
	if (!TestNotNull(TEXT("고정 육지 슬롯"), Top)) { World->DestroyWorld(false); return false; }
	Top->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	Top->SetMaterial(0, Data->TargetMaterial);
	Top->SetMaterial(1, nullptr);
	const TArray<TObjectPtr<UMaterialInterface>> OriginalOverrides = Top->OverrideMaterials;
	const auto FindLandmark = [Globe](FName Country)
	{
		FVector PinLocation;
		if (!Globe->GetCountryPinWorldLocation(Country, PinLocation)) { return static_cast<UStaticMeshComponent*>(nullptr); }
		TInlineComponentArray<UStaticMeshComponent*> Meshes(Globe);
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (Mesh->GetStaticMesh() && Mesh->GetAttachParent()
				&& Mesh->GetAttachParent()->GetComponentLocation().Equals(PinLocation, 0.01)) { return Mesh; }
		}
		return static_cast<UStaticMeshComponent*>(nullptr);
	};
	Globe->SetCountryRegionMode(true);
	TestTrue(TEXT("데이터와 대상 슬롯으로 영역 모드 진입"), Globe->IsCountryRegionMode());
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Top->GetMaterial(0));
	if (!TestNotNull(TEXT("영역 전용 MID"), MID)) { World->DestroyWorld(false); return false; }
	UStaticMeshComponent* LandmarkKR = FindLandmark(TEXT("KR"));
	TestTrue(TEXT("영역이 있는 한국 조형물만 숨김"), LandmarkKR && !LandmarkKR->IsVisible());
	TestEqual(TEXT("영역 클릭 라우팅"), Globe->PickCountry(FVector(15000, 0, 0), FVector(-1, 0, 0)), FName(TEXT("KR")));
	const auto CheckState = [&](const TCHAR* Label, float Hovered, float Selected)
	{
		TestEqual(FString::Printf(TEXT("%s: 기본 선택 가능 표시 유지"), Label), MID->K2_GetScalarParameterValue(TEXT("RegionEnabled")), 1.f);
		TestEqual(FString::Printf(TEXT("%s: 호버"), Label), MID->K2_GetScalarParameterValue(TEXT("RegionHovered")), Hovered);
		TestEqual(FString::Printf(TEXT("%s: 선택"), Label), MID->K2_GetScalarParameterValue(TEXT("RegionSelected")), Selected);
	};
	CheckState(TEXT("첫 진입 대기"), 0.f, 0.f);
	TestEqual(TEXT("기본 반사광 움직임 허용"), MID->K2_GetScalarParameterValue(TEXT("RegionMotionEnabled")), 1.f);
	TestEqual(TEXT("대기 상태에서도 한국 마스크를 사용"), MID->K2_GetTextureParameterValue(TEXT("RegionMask")), static_cast<UTexture*>(Data->Regions[0].Mask));
	Globe->SetHoveredCountry(TEXT("KR"));
	CheckState(TEXT("선택 전 한국 호버"), 1.f, 0.f);
	Globe->SetHoveredCountry(NAME_None);
	CheckState(TEXT("호버 이탈"), 0.f, 0.f);
	Globe->SetSelectedCountry(TEXT("KR"));
	CheckState(TEXT("한국 선택"), 0.f, 1.f);
	Globe->SetHoveredCountry(TEXT("KR"));
	CheckState(TEXT("이미 선택된 한국 호버도 별도 반응"), 1.f, 1.f);
	Globe->SetHoveredCountry(TEXT("JP"));
	CheckState(TEXT("선택은 다른 나라 호버보다 우선"), 0.f, 1.f);
	Globe->SetSelectedCountry(TEXT("JP"));
	CheckState(TEXT("조형물 국가 선택 시에도 한국 대기 표시"), 0.f, 0.f);
	Globe->SetHoveredCountry(TEXT("KR"));
	CheckState(TEXT("조형물 국가 선택 중 한국 호버"), 1.f, 0.f);
	Globe->AddDragInput(FVector2D(10, 0));
	TestTrue(TEXT("드래그 선택 해제"), Globe->GetSelectedCountryId().IsNone());
	CheckState(TEXT("드래그는 선택과 호버를 지우고 대기 표시 유지"), 0.f, 0.f);
	Globe->FocusOnCountry(TEXT("JP"), true);
	UStaticMeshComponent* LandmarkJP = FindLandmark(TEXT("JP"));
	if (TestNotNull(TEXT("미지원 국가 조형물 유지"), LandmarkJP))
	{
		const FVector Center = LandmarkJP->Bounds.Origin;
		const FVector Outward = (Center - Globe->GetActorLocation()).GetSafeNormal();
		TestTrue(TEXT("일본 조형물 표시"), LandmarkJP->IsVisible());
		TestEqual(TEXT("미지원 국가 기존 조형물 클릭"), Globe->PickCountry(Center + Outward * 20000, -Outward), FName(TEXT("JP")));
	}
	Globe->RebuildPins();
	TestTrue(TEXT("핀 재생성 뒤 영역 모드 유지"), Globe->IsCountryRegionMode());
	MID = Cast<UMaterialInstanceDynamic>(Top->GetMaterial(0));
	if (!TestNotNull(TEXT("핀 재생성 뒤에도 영역 MID"), MID)) { World->DestroyWorld(false); return false; }
	CheckState(TEXT("재생성 후 대기 표시"), 0.f, 0.f);
	Globe->SetCountryRegionMode(false);
	TestTrue(TEXT("원래 null 슬롯을 포함한 오버라이드 배열 복원"), Top->OverrideMaterials == OriginalOverrides);
	Globe->FocusOnCountry(TEXT("KR"), true);
	LandmarkKR = FindLandmark(TEXT("KR"));
	TestTrue(TEXT("타이틀 모드에서 한국 조형물 복원"), LandmarkKR && LandmarkKR->IsVisible());
	Globe->SetCountryRegionMode(true);
	MID = Cast<UMaterialInstanceDynamic>(Top->GetMaterial(0));
	if (!TestNotNull(TEXT("재진입 영역 MID"), MID)) { World->DestroyWorld(false); return false; }
	CheckState(TEXT("재진입 시 이전 호버가 남지 않음"), 0.f, 0.f);
	Globe->SetGlobeActive(false);
	TestFalse(TEXT("지구 숨김은 영역 모드도 종료"), Globe->IsCountryRegionMode());
	TestTrue(TEXT("숨김 시 재질 복원"), Top->OverrideMaterials == OriginalOverrides);
	TestTrue(TEXT("숨겨진 지구 클릭 차단"), Globe->PickCountry(FVector(15000, 0, 0), FVector(-1, 0, 0)).IsNone());
	Globe->SetGlobeActive(true);
	FFF_AtlasCountryRegion Japan = Data->Regions[0];
	Japan.CountryId = TEXT("JP");
	Japan.Mask = NewObject<UTexture2D>(Data);
	Japan.Bounds = FLinearColor(130, 30, 145, 46);
	Japan.Polygons[0].Outer = Rectangle(132, 32, 144, 45);
	Japan.Polygons[0].Holes.Reset();
	Japan.Polygons.SetNum(1);
	Data->Regions.Add(Japan);
	FFF_AtlasCountryRegion Unregistered = Japan;
	Unregistered.CountryId = TEXT("NOT_IN_CATALOG");
	Data->Regions.Add(Unregistered);
	Globe->SetCountryRegionMode(true);
	MID = Cast<UMaterialInstanceDynamic>(Top->GetMaterial(0));
	if (!TestNotNull(TEXT("다국가 재진입 MID"), MID)) { World->DestroyWorld(false); return false; }
	TestEqual(TEXT("한국 대기 반사광"), MID->K2_GetScalarParameterValue(TEXT("Region0Enabled")), 1.f);
	TestEqual(TEXT("일본 대기 반사광도 동시 유지"), MID->K2_GetScalarParameterValue(TEXT("Region1Enabled")), 1.f);
	TestEqual(TEXT("목록 없는 도형은 클릭 가능으로 표시하지 않음"), MID->K2_GetScalarParameterValue(TEXT("Region2Enabled")), 0.f);
	TestEqual(TEXT("일본 고유 마스크"), MID->K2_GetTextureParameterValue(TEXT("Region1Mask")), static_cast<UTexture*>(Japan.Mask));
	LandmarkJP = FindLandmark(TEXT("JP"));
	TestTrue(TEXT("영역을 추가한 일본 조형물도 숨김"), LandmarkJP && !LandmarkJP->IsVisible());
	Globe->SetSelectedCountry(TEXT("KR"));
	Globe->SetHoveredCountry(TEXT("JP"));
	TestEqual(TEXT("한국 선택 유지"), MID->K2_GetScalarParameterValue(TEXT("Region0Selected")), 1.f);
	TestEqual(TEXT("일본 호버 독립"), MID->K2_GetScalarParameterValue(TEXT("Region1Hovered")), 1.f);
	TestEqual(TEXT("한국은 일본 호버를 받지 않음"), MID->K2_GetScalarParameterValue(TEXT("Region0Hovered")), 0.f);
	TestEqual(TEXT("일본은 한국 선택을 받지 않음"), MID->K2_GetScalarParameterValue(TEXT("Region1Selected")), 0.f);
	Globe->SetSelectedCountry(TEXT("JP"));
	TestEqual(TEXT("선택을 일본으로 이전"), MID->K2_GetScalarParameterValue(TEXT("Region1Selected")), 1.f);
	TestEqual(TEXT("한국 이전 선택 해제"), MID->K2_GetScalarParameterValue(TEXT("Region0Selected")), 0.f);
	TestEqual(TEXT("한국 기본 표시 유지"), MID->K2_GetScalarParameterValue(TEXT("Region0Enabled")), 1.f);
	Globe->SetCountryRegionMode(false);
	Globe->FocusOnCountry(TEXT("JP"), true);
	LandmarkJP = FindLandmark(TEXT("JP"));
	TestTrue(TEXT("타이틀에 돌아오면 일본 조형물 복원"), LandmarkJP && LandmarkJP->IsVisible());
	TestTrue(TEXT("다국가 모드 종료 후 모든 슬롯 원상 복원"), Top->OverrideMaterials == OriginalOverrides);
	Globe->SetOrientation(FFF_AtlasGlobeOrientation());
	Globe->SetCountryRegionMode(true);
	TestEqual(TEXT("화면 왕복 뒤 같은 표면 클릭 유지"), Globe->PickCountryRegion(FVector(15000, 0, 0), FVector(-1, 0, 0)), FName(TEXT("KR")));
	Globe->SetCountryRegionMode(false);
	// 객체 identity와 배열 길이를 유지한 제작 데이터 수정도 캐시 재사용으로 놓치면 안 된다.
	for (FVector& Vertex : Data->SurfaceVertices) { Vertex.Y += 500.0; }
	Globe->SetCountryRegionMode(true);
	TestTrue(TEXT("같은 에셋의 이전 표면에는 충돌을 남기지 않음"), Globe->PickCountryRegion(FVector(15000, 0, 0), FVector(-1, 0, 0)).IsNone());
	TestEqual(TEXT("재진입 시 변경된 표면 내용 반영"), Globe->PickCountryRegion(FVector(15000, 500, 0), FVector(-1, 0, 0)), FName(TEXT("KR")));
	Globe->SetCountryRegionMode(false);
	Data->SourceRadius = 10000.f;
	Globe->SetCountryRegionMode(true);
	TestTrue(TEXT("원본 반지름 변경 후 물 아래로 이동한 육지는 선택 안 됨"), Globe->PickCountryRegion(FVector(15000, 250, 0), FVector(-1, 0, 0)).IsNone());
	Globe->SetCountryRegionMode(false);
	Data->SourceRadius = 5000.f;
	Globe->SetCountryRegionMode(true);
	TestEqual(TEXT("원본 반지름 복구 뒤 다시 선택"), Globe->PickCountryRegion(FVector(15000, 500, 0), FVector(-1, 0, 0)), FName(TEXT("KR")));
	Globe->SetCountryRegionMode(false);
	DataProperty->SetObjectPropertyValue_InContainer(Globe, nullptr);
	Globe->SetCountryRegionMode(true);
	TestFalse(TEXT("데이터 없는 화면은 기존 방식 유지"), Globe->IsCountryRegionMode());
	TestTrue(TEXT("데이터 없는 재진입에도 원본 재질 유지"), Top->OverrideMaterials == OriginalOverrides);
	World->DestroyWorld(false);
	return true;
}
#endif
#endif
