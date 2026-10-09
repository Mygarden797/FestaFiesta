// FFZAZoneOverlayActor.cpp
// 역할: Zone/구획/구역/보호 영역을 바닥 오버레이 메시로 만들고, 건축 모드에 맞춰 켜고 끈다.
#include "FFZAZoneOverlayActor.h"

#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSProtectedArea.h"
#include "FF_FFZSZoneActor.h"
#include "FF_ZoneAuthoringModule.h"
#include "FFZAAuthoredZone.h"
#include "FFZADistrict.h"
#include "FFZAOverlayMeshBuilder.h"
#include "FFZAPlot.h"
#include "FFZAZoneAuthoringSubsystem.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/UnrealType.h"
#include "TimerManager.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(FFZAZoneOverlayActor)

namespace
{
    const TCHAR* const DefaultOverlayMaterialPath = TEXT("/FF_ZoneAuthoring/Materials/M_FFZA_ZoneOverlay.M_FFZA_ZoneOverlay");
    const FLinearColor OverlayFallbackZoneColor(0.10f, 0.60f, 1.00f, 1.0f);
    constexpr int32 FillSection = 0;
    constexpr int32 OutlineSection = 1;

    FLinearColor WithAlpha(const FLinearColor& Color, const float Alpha)
    {
        return FLinearColor(Color.R, Color.G, Color.B, FMath::Clamp(Alpha, 0.0f, 1.0f));
    }
}

AFFZAZoneOverlayActor::AFFZAZoneOverlayActor()
{
    PrimaryActorTick.bCanEverTick = false;
#if WITH_EDITORONLY_DATA
    // World Partition 맵에서도 항상 로드되게 한다.
    bIsSpatiallyLoaded = false;
#endif

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    // 두 번째 인자 true = Transient: 만든 메시(수만 개 삼각형)를 .umap에 저장하지 않는다.
    OverlayMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("OverlayMesh"), true);
    OverlayMesh->SetupAttachment(SceneRoot);
    OverlayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    OverlayMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    OverlayMesh->SetCastShadow(false);
    OverlayMesh->SetCanEverAffectNavigation(false);
    OverlayMesh->bUseAsyncCooking = true;

    OverlayMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(DefaultOverlayMaterialPath));
}

// ---------------------------------------------------------------------------------------------
// 보이기/숨기기
// ---------------------------------------------------------------------------------------------

void AFFZAZoneOverlayActor::SetOverlayVisible(const bool bVisible)
{
    if (!OverlayMesh)
    {
        return;
    }
    // 처음 켤 때(또는 레이어가 바뀐 뒤) 메시를 만든다. 숨겨진 동안에는 비용을 쓰지 않는다.
    if (bVisible && OverlayMesh->GetNumSections() == 0)
    {
        RebuildInternal(false);
    }
    OverlayMesh->SetVisibility(bVisible, true);
}

bool AFFZAZoneOverlayActor::IsOverlayVisible() const
{
    return OverlayMesh && OverlayMesh->IsVisible();
}

void AFFZAZoneOverlayActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    const UWorld* World = GetWorld();
    if (!World || World->IsGameWorld() || IsTemplate() || !OverlayMesh)
    {
        return;
    }
    // 에디터: 액터를 놓거나 값을 바꾸면 즉시 다시 그린다.
    if (bShowInEditor)
    {
        RebuildInternal(false);
    }
    else
    {
        OverlayMesh->ClearAllMeshSections();
    }
    OverlayMesh->SetVisibility(bShowInEditor, true);
}

void AFFZAZoneOverlayActor::PostRegisterAllComponents()
{
    Super::PostRegisterAllComponents();
#if WITH_EDITOR
    // 에디터에서 레벨을 열면 저장되지 않은 메시가 비어 있다. 다른 액터 등록이 끝난 다음 프레임에 다시 그린다.
    const UWorld* World = GetWorld();
    if (GEditor && World && !World->IsGameWorld() && !IsTemplate() && bShowInEditor && OverlayMesh &&
        OverlayMesh->GetNumSections() == 0)
    {
        GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
        {
            const UWorld* CurrentWorld = GetWorld();
            if (CurrentWorld && !CurrentWorld->IsGameWorld() && OverlayMesh && bShowInEditor && OverlayMesh->GetNumSections() == 0)
            {
                RebuildInternal(false);
            }
        }));
    }
#endif
}

void AFFZAZoneOverlayActor::BeginPlay()
{
    Super::BeginPlay();
    if (OverlayMesh)
    {
        // 에디터에서 저장된 메시는 버리고, 실제로 보여 줄 때 현재 상태로 다시 만든다.
        OverlayMesh->ClearAllMeshSections();
    }
    if (UWorld* World = GetWorld())
    {
        if (UFFZAZoneAuthoringSubsystem* Subsystem = World->GetSubsystem<UFFZAZoneAuthoringSubsystem>())
        {
            Subsystem->OnAuthoringLayersChanged.AddDynamic(this, &AFFZAZoneOverlayActor::HandleAuthoringLayersChanged);
            bLayersDelegateBound = true;
        }
    }
    BindBuildMode();
    SetOverlayVisible(bVisibleOnBeginPlay);
}

void AFFZAZoneOverlayActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnbindBuildMode();
    if (bLayersDelegateBound)
    {
        if (UWorld* World = GetWorld())
        {
            if (UFFZAZoneAuthoringSubsystem* Subsystem = World->GetSubsystem<UFFZAZoneAuthoringSubsystem>())
            {
                Subsystem->OnAuthoringLayersChanged.RemoveDynamic(this, &AFFZAZoneOverlayActor::HandleAuthoringLayersChanged);
            }
        }
        bLayersDelegateBound = false;
    }
    Super::EndPlay(EndPlayReason);
}

void AFFZAZoneOverlayActor::HandleAuthoringLayersChanged()
{
    if (!OverlayMesh)
    {
        return;
    }
    // 구획 해제 등으로 그림이 바뀌었다. 보이는 중이면 즉시, 아니면 다음에 켤 때 다시 만든다.
    if (OverlayMesh->IsVisible())
    {
        RebuildInternal(false);
    }
    else
    {
        OverlayMesh->ClearAllMeshSections();
    }
}

void AFFZAZoneOverlayActor::HandleBuildModeStateChanged(const uint8 PreviousState, const uint8 NewState)
{
    // EFF_BuildModeState: 0 = Closed. 그 외(BuildMode/CatalogOpen/Placing/Moving)는 모두 건축 중이다.
    SetOverlayVisible(NewState != 0);
}

// ---------------------------------------------------------------------------------------------
// 건축 모드 느슨한 연결(리플렉션). 게임 모듈을 include하지 않는다.
// ---------------------------------------------------------------------------------------------

void AFFZAZoneOverlayActor::BindBuildMode()
{
    UWorld* World = GetWorld();
    if (!bAutoShowInBuildMode || !World || !World->IsGameWorld() || BuildModeSubsystemClass.IsNull())
    {
        return;
    }
    UClass* SubsystemClass = BuildModeSubsystemClass.ResolveClass();
    if (!SubsystemClass || !SubsystemClass->IsChildOf(UWorldSubsystem::StaticClass()))
    {
        UE_LOG(LogFFZoneAuthoring, Log, TEXT("오버레이: 건축 모드 클래스 '%s'를 찾지 못해 자동 연동을 건너뜁니다."),
            *BuildModeSubsystemClass.ToString());
        return;
    }
    UWorldSubsystem* Subsystem = World->GetSubsystemBase(SubsystemClass);
    FMulticastDelegateProperty* EventProperty = FindFProperty<FMulticastDelegateProperty>(SubsystemClass, BuildModeStateEventName);
    UFunction* Handler = FindFunction(GET_FUNCTION_NAME_CHECKED(AFFZAZoneOverlayActor, HandleBuildModeStateChanged));
    if (!Subsystem || !EventProperty || !Handler)
    {
        UE_LOG(LogFFZoneAuthoring, Log, TEXT("오버레이: 건축 모드 이벤트 '%s'를 찾지 못해 자동 연동을 건너뜁니다."),
            *BuildModeStateEventName.ToString());
        return;
    }
    // 인자 모양(1바이트 2개)이 같을 때만 연결한다. 다르면 잘못된 메모리를 읽지 않도록 포기한다.
    const UFunction* Signature = EventProperty->SignatureFunction;
    if (!Signature || Signature->NumParms != Handler->NumParms || Signature->ParmsSize != Handler->ParmsSize)
    {
        UE_LOG(LogFFZoneAuthoring, Warning, TEXT("오버레이: 건축 모드 이벤트 인자 모양이 달라 자동 연동을 건너뜁니다. SetOverlayVisible을 직접 호출하세요."));
        return;
    }
    FScriptDelegate Delegate;
    Delegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(AFFZAZoneOverlayActor, HandleBuildModeStateChanged));
    EventProperty->AddDelegate(MoveTemp(Delegate), Subsystem);
    BoundBuildModeSubsystem = Subsystem;
}

void AFFZAZoneOverlayActor::UnbindBuildMode()
{
    UObject* Subsystem = BoundBuildModeSubsystem.Get();
    BoundBuildModeSubsystem.Reset();
    if (!Subsystem)
    {
        return;
    }
    if (FMulticastDelegateProperty* EventProperty =
        FindFProperty<FMulticastDelegateProperty>(Subsystem->GetClass(), BuildModeStateEventName))
    {
        FScriptDelegate Delegate;
        Delegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(AFFZAZoneOverlayActor, HandleBuildModeStateChanged));
        EventProperty->RemoveDelegate(Delegate, Subsystem);
    }
}

// ---------------------------------------------------------------------------------------------
// 메시 생성
// ---------------------------------------------------------------------------------------------

UMaterialInterface* AFFZAZoneOverlayActor::ResolveMaterial() const
{
    if (UMaterialInterface* Material = OverlayMaterial.LoadSynchronous())
    {
        return Material;
    }
#if WITH_EDITOR
    // 에디터 전용 대체 머티리얼(반투명 + 정점 색). 패키지 게임에는 없으므로 실제 머티리얼을 꼭 만들어 둔다.
    if (UMaterialInterface* EditorFallback = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/EditorMaterials/WidgetVertexColorMaterial.WidgetVertexColorMaterial")))
    {
        return EditorFallback;
    }
#endif
    UE_LOG(LogFFZoneAuthoring, Warning,
        TEXT("오버레이 머티리얼 '%s'이 없습니다. 플러그인 Content/Python/ffza_create_overlay_material.py를 실행하세요."),
        *OverlayMaterial.ToString());
    return UMaterial::GetDefaultMaterial(MD_Surface);
}

double AFFZAZoneOverlayActor::SampleGroundHeight(const FVector2D& Point, const double FallbackZ)
{
    const FIntPoint Key(static_cast<int32>(FMath::RoundToDouble(Point.X)), static_cast<int32>(FMath::RoundToDouble(Point.Y)));
    if (const double* Cached = GroundCache.Find(Key))
    {
        return *Cached;
    }
    UWorld* World = GetWorld();
    if (!World || GroundTracesUsed >= MaxGroundTraces)
    {
        return FallbackZ;
    }
    ++GroundTracesUsed;

    FCollisionQueryParams Params(SCENE_QUERY_STAT(FFZAOverlayGround), false, this);
    for (const TWeakObjectPtr<AActor>& Ignored : GroundIgnoredActors)
    {
        if (const AActor* Actor = Ignored.Get())
        {
            Params.AddIgnoredActor(Actor);
        }
    }
    FHitResult Hit;
    const FVector Start(Point.X, Point.Y, FallbackZ + GroundTraceHalfHeightCm);
    const FVector End(Point.X, Point.Y, FallbackZ - GroundTraceHalfHeightCm);
    if (World->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
    {
        // 맞은 값만 저장한다. 빗나감(지형 충돌이 아직 없음 등)은 다음 빌드 때 다시 잰다.
        GroundCache.Add(Key, Hit.ImpactPoint.Z);
        return Hit.ImpactPoint.Z;
    }
    return FallbackZ;
}

void AFFZAZoneOverlayActor::RebuildOverlay()
{
    RebuildInternal(true);
}

void AFFZAZoneOverlayActor::RebuildInternal(const bool bResetGroundCache)
{
    UWorld* World = GetWorld();
    if (!World || !OverlayMesh)
    {
        return;
    }
    OverlayMesh->ClearAllMeshSections();
    if (bResetGroundCache)
    {
        GroundCache.Reset();
    }
    GroundTracesUsed = 0;

    // 설치된 시설 지붕 위로 오버레이가 올라가지 않도록 시설은 지면 추적에서 제외한다.
    GroundIgnoredActors.Reset();
    for (TActorIterator<AFFZSFacilityActor> It(World); It; ++It)
    {
        GroundIgnoredActors.Add(*It);
    }

    // 액터마다 기준 높이가 다르므로 높이 함수가 현재 기준 높이를 참조하게 한다.
    double CurrentFallbackZ = 0.0;
    FFFZAOverlayMeshBuilder Builder(
        [this, &CurrentFallbackZ](const FVector2D& Point)
        {
            return bFollowGround ? SampleGroundHeight(Point, CurrentFallbackZ) : CurrentFallbackZ;
        },
        MaxTriangleEdgeCm, SurfaceLiftCm, MaxTriangles);

    FFFZAOverlayMeshBuffers Fill;
    FFFZAOverlayMeshBuffers Outline;
    const auto AddLayer = [&](const TArray<FVector2D>& Polygon, const double BaseZ, const FLinearColor& Color,
        const float FillAlpha)
    {
        if (Polygon.Num() < 3)
        {
            return;
        }
        CurrentFallbackZ = BaseZ;
        if (FillAlpha > 0.0f)
        {
            Builder.AddFilledPolygon(Polygon, WithAlpha(Color, FillAlpha), Fill);
        }
        Builder.AddOutline(Polygon, OutlineWidthCm, WithAlpha(Color, OutlineOpacity), Outline);
    };

    // 1) Zone
    if (bShowZones)
    {
        TArray<AFFZSGameplayZone*> Zones;
        TArray<TArray<FVector2D>> Polygons;
        UFFZAZoneAuthoringSubsystem::CollectWorldZones(*World, Zones, Polygons);
        for (int32 Index = 0; Index < Zones.Num(); ++Index)
        {
            const AFFZAAuthoredZone* Authored = Cast<AFFZAAuthoredZone>(Zones[Index]);
            const FLinearColor Color = Authored ? Authored->GetZoneColor() : OverlayFallbackZoneColor;
            AddLayer(Polygons[Index], Zones[Index]->GetActorLocation().Z, Color, FillOpacity);
        }
    }

    const UFFZAZoneAuthoringSubsystem* Authoring = World->GetSubsystem<UFFZAZoneAuthoringSubsystem>();

    // 2) 구역(정책) — Zone 위에 옅게
    if (bShowDistricts && Authoring)
    {
        for (const AFFZADistrict* District : Authoring->GetDistricts())
        {
            AddLayer(District->GetWorldPolygon(), District->GetActorLocation().Z, District->GetOverlayColor(), DistrictFillOpacity);
        }
    }

    // 3) 잠긴 구획 — 진하게
    if (bShowLockedPlots && Authoring)
    {
        for (const AFFZAPlot* Plot : Authoring->GetPlots())
        {
            if (Plot->ShouldDrawOverlay())
            {
                AddLayer(Plot->GetWorldPolygon(), Plot->GetActorLocation().Z, Plot->GetOverlayColor(),
                    FMath::Clamp(FillOpacity + 0.25f, 0.0f, 1.0f));
            }
        }
    }

    // 4) 보호 영역(통로·건설 금지) — 빨강
    if (bShowProtectedAreas)
    {
        for (TActorIterator<AFFZSProtectedArea> It(World); It; ++It)
        {
            FVector Center;
            FVector HalfExtent;
            FQuat Rotation;
            if (!It->GetEffectiveWorldBox(Center, HalfExtent, Rotation))
            {
                continue;
            }
            TArray<FVector2D> Corners;
            const FVector LocalCorners[4] = {
                FVector(-HalfExtent.X, -HalfExtent.Y, 0.0), FVector(HalfExtent.X, -HalfExtent.Y, 0.0),
                FVector(HalfExtent.X, HalfExtent.Y, 0.0), FVector(-HalfExtent.X, HalfExtent.Y, 0.0)
            };
            for (const FVector& Local : LocalCorners)
            {
                const FVector World3 = Center + Rotation.RotateVector(Local);
                Corners.Add(FVector2D(World3.X, World3.Y));
            }
            AddLayer(Corners, Center.Z - HalfExtent.Z, ProtectedAreaColor, FillOpacity);
        }
    }

    // 빌더는 월드 좌표로 만든다. 메시 정점은 컴포넌트 로컬 좌표여야 하므로 액터를 옮겨도 맞도록 변환한다.
    const FTransform ComponentTransform = OverlayMesh->GetComponentTransform();
    for (FFFZAOverlayMeshBuffers* Buffers : { &Fill, &Outline })
    {
        for (FVector& Vertex : Buffers->Vertices)
        {
            Vertex = ComponentTransform.InverseTransformPosition(Vertex);
        }
        for (FVector& Normal : Buffers->Normals)
        {
            Normal = ComponentTransform.InverseTransformVectorNoScale(Normal);
        }
    }

    UMaterialInterface* Material = ResolveMaterial();
    const TArray<FProcMeshTangent> NoTangents;
    if (Fill.Triangles.Num() > 0)
    {
        OverlayMesh->CreateMeshSection_LinearColor(FillSection, Fill.Vertices, Fill.Triangles, Fill.Normals, Fill.UV0,
            Fill.Colors, NoTangents, false);
        OverlayMesh->SetMaterial(FillSection, Material);
    }
    if (Outline.Triangles.Num() > 0)
    {
        OverlayMesh->CreateMeshSection_LinearColor(OutlineSection, Outline.Vertices, Outline.Triangles, Outline.Normals,
            Outline.UV0, Outline.Colors, NoTangents, false);
        OverlayMesh->SetMaterial(OutlineSection, Material);
    }
    if (Builder.WasBudgetLimited())
    {
        UE_LOG(LogFFZoneAuthoring, Warning,
            TEXT("오버레이: 삼각형 예산(%d)에 걸려 일부 품질을 낮췄습니다. MaxTriangleEdgeCm를 키우거나 MaxTriangles를 늘리세요."),
            MaxTriangles);
    }
    if (bFollowGround && GroundTracesUsed >= MaxGroundTraces)
    {
        UE_LOG(LogFFZoneAuthoring, Warning,
            TEXT("오버레이: 지면 추적 예산(%d)을 다 써서 일부는 평평하게 그렸습니다. MaxGroundTraces를 늘리세요."),
            MaxGroundTraces);
    }
}
