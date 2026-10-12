// FFZAZoneLayoutManager.cpp
// 역할: Zone 작업 도구 상자 버튼 구현(생성·정리·검사·갱신). 에디터 빌드에서만 동작한다.
#include "FFZAZoneLayoutManager.h"

#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"

#if WITH_EDITORONLY_DATA
#include "Components/BillboardComponent.h"
#endif

#if WITH_EDITOR
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FF_FFZSBuildArea.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSProtectedArea.h"
#include "FF_FFZSZoneActor.h"
#include "FF_ZoneAuthoringModule.h"
#include "FFZAAuthoredZone.h"
#include "FFZADistrict.h"
#include "FFZALayoutOps.h"
#include "FFZAPlot.h"
#include "FFZAPolygonMath.h"
#include "FFZASplineAuthoring.h"
#include "FFZAZoneAuthoringSubsystem.h"
#include "FFZAZoneCellMap.h"
#include "FFZAZoneDefinition.h"
#include "FFZAZoneOverlayActor.h"
#include "Logging/MessageLog.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(FFZAZoneLayoutManager)

#define LOCTEXT_NAMESPACE "FFZAZoneLayoutManager"

AFFZAZoneLayoutManager::AFFZAZoneLayoutManager()
{
    PrimaryActorTick.bCanEverTick = false;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
#if WITH_EDITORONLY_DATA
    SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
    if (SpriteComponent)
    {
        SpriteComponent->SetupAttachment(SceneRoot);
        SpriteComponent->bIsScreenSizeScaled = true;
    }
#endif
}

#if WITH_EDITOR
namespace
{
    FFZASplineAuthoring::FNormalizeOptions MakeZoneOptions()
    {
        return FFZASplineAuthoring::FNormalizeOptions();
    }

    // 보고서: Map Check 창 + 디테일 패널 목록에 같은 내용을 남긴다.
    struct FReport
    {
        FMessageLog MapCheck{ TEXT("MapCheck") };
        TArray<FString> Lines;
        int32 Errors = 0;
        int32 Warnings = 0;

        void Error(const FText& Text)
        {
            ++Errors;
            Lines.Add(TEXT("[오류] ") + Text.ToString());
            MapCheck.Error(Text);
        }
        void Warning(const FText& Text)
        {
            ++Warnings;
            Lines.Add(TEXT("[경고] ") + Text.ToString());
            MapCheck.Warning(Text);
        }
        void Info(const FText& Text)
        {
            Lines.Add(TEXT("[정보] ") + Text.ToString());
            MapCheck.Info(Text);
        }
    };

    FText ZoneLabel(const AActor& Actor)
    {
        return FText::FromString(Actor.GetActorNameOrLabel());
    }
}

FVector AFFZAZoneLayoutManager::GetSpawnLocation() const
{
    const FVector Location = GetActorLocation();
    return FVector(FMath::RoundToDouble(Location.X), FMath::RoundToDouble(Location.Y), Location.Z);
}

FName AFFZAZoneLayoutManager::MakeUniqueAreaId(const TCHAR* Prefix) const
{
    TSet<FName> Used;
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<AFFZAPolygonAreaActor> It(World); It; ++It)
        {
            Used.Add(It->AreaId);
        }
    }
    for (int32 Number = 1; Number < 10000; ++Number)
    {
        const FName Candidate(*FString::Printf(TEXT("%s_%02d"), Prefix, Number));
        if (!Used.Contains(Candidate))
        {
            return Candidate;
        }
    }
    return FName(*FString::Printf(TEXT("%s_%s"), Prefix, *FGuid::NewGuid().ToString()));
}

void AFFZAZoneLayoutManager::SelectInEditor(AActor* Actor) const
{
    if (GEditor && Actor)
    {
        GEditor->SelectNone(false, true);
        GEditor->SelectActor(Actor, true, true);
    }
}
#endif

// ---------------------------------------------------------------------------------------------
// 1. 만들기
// ---------------------------------------------------------------------------------------------

void AFFZAZoneLayoutManager::CreateZone()
{
#if WITH_EDITOR
    UWorld* World = GetWorld();
    if (!World || World->IsGameWorld())
    {
        return;
    }
    if (!NewZoneDefinition)
    {
        UE_LOG(LogFFZoneAuthoring, Warning, TEXT("Create Zone: 먼저 'New Zone Definition'에 Zone 정의 에셋을 지정하세요."));
    }
    const FScopedTransaction Transaction(LOCTEXT("CreateZone", "Zone 만들기"));
    FActorSpawnParameters Params;
    Params.ObjectFlags |= RF_Transactional;
    AFFZAAuthoredZone* Zone = World->SpawnActor<AFFZAAuthoredZone>(GetSpawnLocation(), FRotator::ZeroRotator, Params);
    if (!Zone)
    {
        return;
    }
    Zone->ZoneDefinition = NewZoneDefinition;
    if (Zone->BoundarySpline)
    {
        Zone->BoundarySpline->Modify();
        FFZASplineAuthoring::SetLocalRectangle(*Zone->BoundarySpline, NewAreaSizeMeters * 100.0);
    }
    Zone->ApplyDefinition();
    Zone->SetFolderPath(TEXT("Zones/Zones"));
    Zone->SetActorLabel(NewZoneDefinition ? FString::Printf(TEXT("Zone_%s"), *NewZoneDefinition->ZoneId.ToString()) : TEXT("Zone_New"));
    Zone->RefreshAuthoringStatus();
    SelectInEditor(Zone);
#endif
}

void AFFZAZoneLayoutManager::CreatePlot()
{
#if WITH_EDITOR
    UWorld* World = GetWorld();
    if (!World || World->IsGameWorld())
    {
        return;
    }
    const FScopedTransaction Transaction(LOCTEXT("CreatePlot", "구획 만들기"));
    FActorSpawnParameters Params;
    Params.ObjectFlags |= RF_Transactional;
    AFFZAPlot* Plot = World->SpawnActor<AFFZAPlot>(GetSpawnLocation(), FRotator::ZeroRotator, Params);
    if (!Plot)
    {
        return;
    }
    Plot->AreaId = MakeUniqueAreaId(TEXT("PLOT"));
    if (Plot->BoundarySpline)
    {
        Plot->BoundarySpline->Modify();
        FFZASplineAuthoring::SetLocalRectangle(*Plot->BoundarySpline, NewAreaSizeMeters * 100.0);
    }
    Plot->SetFolderPath(TEXT("Zones/Plots"));
    Plot->SetActorLabel(Plot->AreaId.ToString());
    Plot->RefreshAuthoringStatus();
    SelectInEditor(Plot);
#endif
}

void AFFZAZoneLayoutManager::CreateDistrict()
{
#if WITH_EDITOR
    UWorld* World = GetWorld();
    if (!World || World->IsGameWorld())
    {
        return;
    }
    const FScopedTransaction Transaction(LOCTEXT("CreateDistrict", "구역 만들기"));
    FActorSpawnParameters Params;
    Params.ObjectFlags |= RF_Transactional;
    AFFZADistrict* District = World->SpawnActor<AFFZADistrict>(GetSpawnLocation(), FRotator::ZeroRotator, Params);
    if (!District)
    {
        return;
    }
    District->AreaId = MakeUniqueAreaId(TEXT("DISTRICT"));
    if (District->BoundarySpline)
    {
        District->BoundarySpline->Modify();
        FFZASplineAuthoring::SetLocalRectangle(*District->BoundarySpline, NewAreaSizeMeters * 100.0);
    }
    District->SetFolderPath(TEXT("Zones/Districts"));
    District->SetActorLabel(District->AreaId.ToString());
    District->RefreshAuthoringStatus();
    SelectInEditor(District);
#endif
}

void AFFZAZoneLayoutManager::CreateOverlay()
{
#if WITH_EDITOR
    UWorld* World = GetWorld();
    if (!World || World->IsGameWorld())
    {
        return;
    }
    for (TActorIterator<AFFZAZoneOverlayActor> It(World); It; ++It)
    {
        UE_LOG(LogFFZoneAuthoring, Log, TEXT("Create Overlay: 이미 오버레이가 있습니다(%s)."), *It->GetActorNameOrLabel());
        SelectInEditor(*It);
        return;
    }
    const FScopedTransaction Transaction(LOCTEXT("CreateOverlay", "오버레이 만들기"));
    FActorSpawnParameters Params;
    Params.ObjectFlags |= RF_Transactional;
    AFFZAZoneOverlayActor* Overlay = World->SpawnActor<AFFZAZoneOverlayActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (Overlay)
    {
        Overlay->SetFolderPath(TEXT("Zones"));
        Overlay->SetActorLabel(TEXT("ZoneOverlay"));
        Overlay->RebuildOverlay();
        SelectInEditor(Overlay);
    }
#endif
}

// ---------------------------------------------------------------------------------------------
// 2. 정리
// ---------------------------------------------------------------------------------------------

#if WITH_EDITOR
void AFFZAZoneLayoutManager::ApplyAllDefinitionsInternal(int32& OutCount)
{
    OutCount = 0;
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    for (TActorIterator<AFFZAAuthoredZone> It(World); It; ++It)
    {
        if (It->ZoneDefinition && It->IsOutOfSyncWithDefinition())
        {
            It->ApplyDefinition();
            ++OutCount;
        }
        else
        {
            It->RefreshAuthoringStatus();
        }
    }
}

void AFFZAZoneLayoutManager::NormalizeAllTransformsInternal(int32& OutCount)
{
    OutCount = 0;
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    // 기존(비저작형) Zone도 같은 규칙으로 정리한다. 경계 모양은 바뀌지 않는다.
    for (TActorIterator<AFFZSGameplayZone> It(World); It; ++It)
    {
        if (It->BoundarySpline &&
            FFZASplineAuthoring::NormalizeActorTransformPreservingSpline(**It, *It->BoundarySpline, MakeZoneOptions()))
        {
            ++OutCount;
        }
        if (AFFZAAuthoredZone* Authored = Cast<AFFZAAuthoredZone>(*It))
        {
            Authored->RefreshAuthoringStatus();
        }
    }
}

int32 AFFZAZoneLayoutManager::WeldSharedBoundariesInternal()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return 0;
    }
    TArray<AFFZSGameplayZone*> Zones;
    TArray<TArray<FVector2D>> Polygons;
    UFFZAZoneAuthoringSubsystem::CollectWorldZones(*World, Zones, Polygons);
    TArray<bool> Changed;
    const int32 Moved = FFZALayoutOps::WeldPolygons(Polygons, WeldToleranceCm, Changed);
    for (int32 Index = 0; Index < Zones.Num(); ++Index)
    {
        if (!Changed[Index] || !Zones[Index]->BoundarySpline)
        {
            continue;
        }
        Zones[Index]->Modify();
        FFZASplineAuthoring::SetWorldPolygon(*Zones[Index]->BoundarySpline, Polygons[Index], Zones[Index]->GetActorLocation().Z);
        if (AFFZAAuthoredZone* Authored = Cast<AFFZAAuthoredZone>(Zones[Index]))
        {
            Authored->RefreshAuthoringStatus();
        }
    }
    return Moved;
}

int32 AFFZAZoneLayoutManager::AutoFillNeighborsInternal()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return 0;
    }
    TArray<AFFZSGameplayZone*> Zones;
    TArray<TArray<FVector2D>> Polygons;
    UFFZAZoneAuthoringSubsystem::CollectWorldZones(*World, Zones, Polygons);
    if (Zones.Num() < 2)
    {
        return 0;
    }
    FFFZAZoneCellMap Map;
    if (!Map.Build(Polygons, AnalysisCellSizeCm))
    {
        return 0;
    }

    // 셀 지도에서 맞닿은 쌍 → 양방향 이웃 목록
    TArray<TSet<FName>> Neighbors;
    Neighbors.SetNum(Zones.Num());
    if (bKeepManualNeighbors)
    {
        for (int32 Index = 0; Index < Zones.Num(); ++Index)
        {
            Neighbors[Index].Append(Zones[Index]->NeighborZoneIds);
        }
    }
    for (const FIntPoint& Pair : Map.GetAdjacentZonePairs())
    {
        const FName A = Zones[Pair.X]->ZoneId;
        const FName B = Zones[Pair.Y]->ZoneId;
        if (A.IsNone() || B.IsNone() || A == B)
        {
            continue;
        }
        Neighbors[Pair.X].Add(B);
        Neighbors[Pair.Y].Add(A);
    }
    // 손으로 넣은 이웃도 반대쪽에 채워 대칭을 맞춘다.
    for (int32 Index = 0; Index < Zones.Num(); ++Index)
    {
        for (const FName Neighbor : Neighbors[Index])
        {
            for (int32 Other = 0; Other < Zones.Num(); ++Other)
            {
                if (Other != Index && Zones[Other]->ZoneId == Neighbor && !Zones[Index]->ZoneId.IsNone())
                {
                    Neighbors[Other].Add(Zones[Index]->ZoneId);
                }
            }
        }
    }

    int32 ChangedZones = 0;
    for (int32 Index = 0; Index < Zones.Num(); ++Index)
    {
        TArray<FName> Sorted = Neighbors[Index].Array();
        Sorted.Remove(Zones[Index]->ZoneId);
        Sorted.Remove(NAME_None);
        Sorted.Sort(FNameLexicalLess());
        if (Sorted != Zones[Index]->NeighborZoneIds)
        {
            Zones[Index]->Modify();
            Zones[Index]->NeighborZoneIds = MoveTemp(Sorted);
            ++ChangedZones;
        }
        if (AFFZAAuthoredZone* Authored = Cast<AFFZAAuthoredZone>(Zones[Index]))
        {
            Authored->RefreshAuthoringStatus();
        }
    }
    return ChangedZones;
}

void AFFZAZoneLayoutManager::RefreshInternal()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    for (TActorIterator<AFFZSBuildArea> It(World); It; ++It)
    {
        It->RebuildFromWorldRegions();
    }
    if (UFFZAZoneAuthoringSubsystem* Subsystem = World->GetSubsystem<UFFZAZoneAuthoringSubsystem>())
    {
        Subsystem->RebuildZoneCellMap(AnalysisCellSizeCm);
    }
    for (TActorIterator<AFFZAZoneOverlayActor> It(World); It; ++It)
    {
        It->RebuildOverlay();
    }
}
#endif

void AFFZAZoneLayoutManager::ApplyAllDefinitions()
{
#if WITH_EDITOR
    const FScopedTransaction Transaction(LOCTEXT("ApplyAll", "모든 Zone 정의 적용"));
    int32 Count = 0;
    ApplyAllDefinitionsInternal(Count);
    UE_LOG(LogFFZoneAuthoring, Log, TEXT("Apply All Definitions: %d개 Zone을 정의에 맞췄습니다."), Count);
#endif
}

void AFFZAZoneLayoutManager::NormalizeAllTransforms()
{
#if WITH_EDITOR
    const FScopedTransaction Transaction(LOCTEXT("NormalizeAll", "모든 Zone Transform 정리"));
    int32 Count = 0;
    NormalizeAllTransformsInternal(Count);
    UE_LOG(LogFFZoneAuthoring, Log, TEXT("Normalize All Transforms: %d개 Zone을 정리했습니다."), Count);
#endif
}

void AFFZAZoneLayoutManager::WeldSharedBoundaries()
{
#if WITH_EDITOR
    const FScopedTransaction Transaction(LOCTEXT("Weld", "Zone 경계 용접"));
    const int32 Moved = WeldSharedBoundariesInternal();
    UE_LOG(LogFFZoneAuthoring, Log, TEXT("Weld Shared Boundaries: 꼭짓점 %d개를 이웃 경계에 붙였습니다."), Moved);
#endif
}

void AFFZAZoneLayoutManager::AutoFillNeighbors()
{
#if WITH_EDITOR
    const FScopedTransaction Transaction(LOCTEXT("Neighbors", "이웃 Zone 자동 채우기"));
    const int32 Changed = AutoFillNeighborsInternal();
    UE_LOG(LogFFZoneAuthoring, Log, TEXT("Auto Fill Neighbors: %d개 Zone의 이웃 목록을 갱신했습니다."), Changed);
#endif
}

void AFFZAZoneLayoutManager::SyncProjectZoneIds()
{
#if WITH_EDITOR
    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    // 에디터 시작 직후에는 에셋 목록 스캔이 끝나지 않았을 수 있다. 일부만 보고 목록을 지우지 않도록 기다린다.
    if (AssetRegistry.IsLoadingAssets())
    {
        AssetRegistry.WaitForCompletion();
    }
    FARFilter Filter;
    Filter.ClassPaths.Add(UFFZAZoneDefinition::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;
    TArray<FAssetData> Assets;
    AssetRegistry.GetAssets(Filter, Assets);

    TMap<FName, FString> IdOwners;
    TArray<FName> DefinitionIds;
    for (const FAssetData& Asset : Assets)
    {
        // 에셋을 메모리에 올리지 않고 검색 태그(ZoneId)만 읽는다.
        FString IdString;
        if (!Asset.GetTagValue(GET_MEMBER_NAME_CHECKED(UFFZAZoneDefinition, ZoneId), IdString) ||
            IdString.IsEmpty() || IdString == TEXT("None"))
        {
            continue;
        }
        const FName ZoneId(*IdString);
        if (const FString* ExistingPath = IdOwners.Find(ZoneId))
        {
            UE_LOG(LogFFZoneAuthoring, Error, TEXT("Sync Project Zone Ids: '%s' ID가 %s 와 %s 에 중복됩니다. 하나를 고치세요."),
                *ZoneId.ToString(), **ExistingPath, *Asset.GetObjectPathString());
            continue;
        }
        IdOwners.Add(ZoneId, Asset.GetObjectPathString());
        DefinitionIds.Add(ZoneId);
    }

    UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
    TArray<FName> NewIds = bRemoveIdsWithoutDefinition ? TArray<FName>() : Settings->ProjectZoneIds;
    for (const FName Id : DefinitionIds)
    {
        NewIds.AddUnique(Id);
    }
    if (NewIds.IsEmpty())
    {
        // 빈 목록을 저장하면 모든 Zone이 검증에 실패한다. 실수로 지우는 일을 막는다.
        UE_LOG(LogFFZoneAuthoring, Warning, TEXT("Sync Project Zone Ids: 정의 에셋을 찾지 못해 저장하지 않았습니다."));
    }
    else if (NewIds == Settings->ProjectZoneIds)
    {
        UE_LOG(LogFFZoneAuthoring, Log, TEXT("Sync Project Zone Ids: 이미 최신입니다(정의 %d개)."), DefinitionIds.Num());
    }
    else
    {
        Settings->Modify();
        Settings->ProjectZoneIds = MoveTemp(NewIds);
        if (const FProperty* Property = UFFZSDeveloperSettings::StaticClass()->FindPropertyByName(
            GET_MEMBER_NAME_CHECKED(UFFZSDeveloperSettings, ProjectZoneIds)))
        {
            Settings->UpdateSinglePropertyInConfigFile(Property, Settings->GetDefaultConfigFilename());
        }
        UE_LOG(LogFFZoneAuthoring, Log, TEXT("Sync Project Zone Ids: %d개 ID로 저장했습니다(%s)."),
            Settings->ProjectZoneIds.Num(), *Settings->GetDefaultConfigFilename());
    }

    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<AFFZAAuthoredZone> It(World); It; ++It)
        {
            It->RefreshAuthoringStatus();
        }
    }
#endif
}

// ---------------------------------------------------------------------------------------------
// 3. 검사
// ---------------------------------------------------------------------------------------------

#if WITH_EDITOR
void AFFZAZoneLayoutManager::ValidateInternal()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    FReport Report;
    Report.MapCheck.NewPage(LOCTEXT("ReportPage", "FF Zone Layout"));

    TArray<AFFZSGameplayZone*> Zones;
    TArray<TArray<FVector2D>> Polygons;
    UFFZAZoneAuthoringSubsystem::CollectWorldZones(*World, Zones, Polygons);

    // 1) Zone별 규칙
    TMap<FName, const AFFZSGameplayZone*> ZonesById;
    for (const AFFZSGameplayZone* Zone : Zones)
    {
        TArray<FText> Issues;
        if (const AFFZAAuthoredZone* Authored = Cast<AFFZAAuthoredZone>(Zone))
        {
            Authored->CollectAuthoringIssues(Issues);
        }
        else
        {
            Zone->ValidateZoneData(Issues);
        }
        for (const FText& Issue : Issues)
        {
            Report.Error(FText::Format(LOCTEXT("ZoneIssue", "{0}: {1}"), ZoneLabel(*Zone), Issue));
        }
        if (!Zone->ZoneId.IsNone())
        {
            if (const AFFZSGameplayZone* const* Existing = ZonesById.Find(Zone->ZoneId))
            {
                Report.Error(FText::Format(LOCTEXT("DupZone", "Zone ID '{0}'가 {1}, {2}에 중복됩니다."),
                    FText::FromName(Zone->ZoneId), ZoneLabel(**Existing), ZoneLabel(*Zone)));
            }
            else
            {
                ZonesById.Add(Zone->ZoneId, Zone);
            }
        }
    }

    // 2) 이웃 그래프: 존재·대칭
    for (const AFFZSGameplayZone* Zone : Zones)
    {
        for (const FName Neighbor : Zone->NeighborZoneIds)
        {
            const AFFZSGameplayZone* const* Other = ZonesById.Find(Neighbor);
            if (!Other)
            {
                Report.Error(FText::Format(LOCTEXT("MissingNeighbor", "{0}: 이웃 '{1}' Zone이 레벨에 없습니다."),
                    ZoneLabel(*Zone), FText::FromName(Neighbor)));
            }
            else if (!(*Other)->NeighborZoneIds.Contains(Zone->ZoneId))
            {
                Report.Error(FText::Format(LOCTEXT("AsymNeighbor", "{0} → {1} 이웃 연결이 한쪽에만 있습니다. 'Auto Fill Neighbors'로 맞추세요."),
                    ZoneLabel(*Zone), ZoneLabel(**Other)));
            }
        }
    }

    // 3) 공간 분석(셀 지도): 겹침, 이웃 누락/과잉, 너무 작은 Zone
    FFFZAZoneCellMap Map;
    if (Zones.Num() > 0 && Map.Build(Polygons, AnalysisCellSizeCm))
    {
        const double CellAreaM2 = FMath::Square(Map.GetCellSizeCm() / 100.0);
        for (const FFFZAZoneCellOverlap& Overlap : Map.GetOverlaps())
        {
            Report.Error(FText::Format(LOCTEXT("Overlap", "{0}와 {1}가 약 {2} m² 겹칩니다. 겹친 곳에 걸친 시설은 설치가 거부됩니다."),
                ZoneLabel(*Zones[Overlap.ZoneIndexA]), ZoneLabel(*Zones[Overlap.ZoneIndexB]),
                FText::AsNumber(FMath::RoundToInt(Overlap.CellCount * CellAreaM2))));
        }
        for (const FIntPoint& Pair : Map.GetAdjacentZonePairs())
        {
            const AFFZSGameplayZone* A = Zones[Pair.X];
            const AFFZSGameplayZone* B = Zones[Pair.Y];
            if (!A->NeighborZoneIds.Contains(B->ZoneId) || !B->NeighborZoneIds.Contains(A->ZoneId))
            {
                Report.Warning(FText::Format(LOCTEXT("MissingAdjacency", "{0}와 {1}는 맞닿아 있지만 이웃으로 등록되지 않았습니다."),
                    ZoneLabel(*A), ZoneLabel(*B)));
            }
        }
        for (int32 Index = 0; Index < Zones.Num(); ++Index)
        {
            if (Map.GetOwnedCellCount(Index) == 0)
            {
                Report.Warning(FText::Format(LOCTEXT("TinyZone", "{0}: 분석 칸({1} cm)보다 작거나 전부 겹쳐 있습니다."),
                    ZoneLabel(*Zones[Index]), FText::AsNumber(FMath::RoundToInt(Map.GetCellSizeCm()))));
            }
            for (const FName Neighbor : Zones[Index]->NeighborZoneIds)
            {
                const int32 OtherIndex = Zones.IndexOfByPredicate([Neighbor](const AFFZSGameplayZone* Z) { return Z->ZoneId == Neighbor; });
                if (OtherIndex != INDEX_NONE &&
                    !Map.GetAdjacentZonePairs().Contains(FIntPoint(FMath::Min(Index, OtherIndex), FMath::Max(Index, OtherIndex))))
                {
                    Report.Info(FText::Format(LOCTEXT("FarNeighbor", "{0}의 이웃 {1}은 실제로 맞닿아 있지 않습니다(의도한 연결이면 무시)."),
                        ZoneLabel(*Zones[Index]), FText::FromName(Neighbor)));
                }
            }
        }
    }

    // 4) Project Settings 목록과 레벨 비교
    if (const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>(); Settings && Settings->bRestrictZoneIdsToProjectPresets)
    {
        for (const FName Id : Settings->ProjectZoneIds)
        {
            if (!ZonesById.Contains(Id))
            {
                Report.Warning(FText::Format(LOCTEXT("SettingsZoneMissing",
                    "Project Zone Ids의 '{0}' Zone이 이 레벨에 없습니다. 게임 시작 시 Zone 그래프 검증이 실패할 수 있습니다."),
                    FText::FromName(Id)));
            }
        }
    }

    // 5) 구획/구역
    TSet<FName> AreaIds;
    for (TActorIterator<AFFZAPolygonAreaActor> It(World); It; ++It)
    {
        TArray<FText> Issues;
        It->ValidateAreaData(Issues);
        for (const FText& Issue : Issues)
        {
            Report.Error(FText::Format(LOCTEXT("AreaIssue", "{0}: {1}"), ZoneLabel(**It), Issue));
        }
        if (!It->AreaId.IsNone())
        {
            bool bAlreadyUsed = false;
            AreaIds.Add(It->AreaId, &bAlreadyUsed);
            if (bAlreadyUsed)
            {
                Report.Error(FText::Format(LOCTEXT("DupArea", "{0}: Area Id '{1}'가 중복됩니다."), ZoneLabel(**It), FText::FromName(It->AreaId)));
            }
        }
        // 어떤 Zone과도 겹치지 않는 구획/구역은 효과가 없다.
        const TArray<FVector2D> AreaPolygon = It->GetWorldPolygon();
        bool bTouchesZone = false;
        for (const TArray<FVector2D>& ZonePolygon : Polygons)
        {
            for (const FVector2D& Point : AreaPolygon)
            {
                if (FFZAPolygonMath::IsPointInside(ZonePolygon, Point))
                {
                    bTouchesZone = true;
                    break;
                }
            }
            for (int32 P = 0; !bTouchesZone && P < ZonePolygon.Num(); ++P)
            {
                bTouchesZone = FFZAPolygonMath::IsPointInside(AreaPolygon, ZonePolygon[P]);
            }
            if (bTouchesZone)
            {
                break;
            }
        }
        if (!bTouchesZone && AreaPolygon.Num() >= 3)
        {
            Report.Warning(FText::Format(LOCTEXT("AreaOutside", "{0}: 어떤 Zone과도 겹치지 않아 효과가 없습니다."), ZoneLabel(**It)));
        }
    }

    // 6) 보호 영역(기존 FF_ZoneSystem 검증 재사용)
    for (TActorIterator<AFFZSProtectedArea> It(World); It; ++It)
    {
        TArray<FText> Issues;
        It->ValidateProtectedAreaData(Issues);
        for (const FText& Issue : Issues)
        {
            Report.Error(FText::Format(LOCTEXT("ProtectedIssue", "{0}: {1}"), ZoneLabel(**It), Issue));
        }
    }

    // 7) 보조 액터 존재 여부
    bool bHasOverlay = false;
    for (TActorIterator<AFFZAZoneOverlayActor> It(World); It; ++It)
    {
        bHasOverlay = true;
        if (!It->OverlayMaterial.IsNull() && !It->OverlayMaterial.LoadSynchronous())
        {
            Report.Warning(LOCTEXT("NoOverlayMaterial",
                "오버레이 머티리얼이 없습니다. 패키지 게임에서 회색으로 보입니다. Content/Python/ffza_create_overlay_material.py를 실행하세요."));
        }
    }
    if (!bHasOverlay)
    {
        Report.Info(LOCTEXT("NoOverlay", "레벨에 FF Zone Overlay가 없습니다. 건축 모드 바닥 표시를 원하면 'Create Overlay'를 누르세요."));
    }
    bool bHasBuildArea = false;
    for (TActorIterator<AFFZSBuildArea> It(World); It; ++It)
    {
        bHasBuildArea = true;
    }
    Report.Info(bHasBuildArea
        ? LOCTEXT("BuildAreaOn", "통합 건설 영역(Build Area) 사용 중: Zone 경계를 넘어 설치할 수 있습니다.")
        : LOCTEXT("BuildAreaOff", "통합 건설 영역(Build Area)이 없습니다: 시설은 한 Zone 안에만 설치됩니다."));

    Report.Info(FText::Format(LOCTEXT("Summary", "검사 완료: Zone {0}개, 오류 {1}개, 경고 {2}개"),
        FText::AsNumber(Zones.Num()), FText::AsNumber(Report.Errors), FText::AsNumber(Report.Warnings)));

    LastErrorCount = Report.Errors;
    LastWarningCount = Report.Warnings;
    LastReport = MoveTemp(Report.Lines);
    Report.MapCheck.Open(EMessageSeverity::Info, true);
}
#endif

void AFFZAZoneLayoutManager::ValidateAll()
{
#if WITH_EDITOR
    ValidateInternal();
#endif
}

// ---------------------------------------------------------------------------------------------
// 4. 갱신 / 0. 한 번에
// ---------------------------------------------------------------------------------------------

void AFFZAZoneLayoutManager::RefreshBuildAreaAndOverlays()
{
#if WITH_EDITOR
    RefreshInternal();
#endif
}

void AFFZAZoneLayoutManager::FixAndValidateAll()
{
#if WITH_EDITOR
    {
        const FScopedTransaction Transaction(LOCTEXT("FixAll", "Zone 레이아웃 일괄 정리"));
        int32 Applied = 0;
        int32 Normalized = 0;
        ApplyAllDefinitionsInternal(Applied);
        NormalizeAllTransformsInternal(Normalized);
        const int32 Welded = WeldSharedBoundariesInternal();
        const int32 NeighborChanges = AutoFillNeighborsInternal();
        UE_LOG(LogFFZoneAuthoring, Log, TEXT("Fix And Validate All: 정의 %d, 정리 %d, 용접 꼭짓점 %d, 이웃 갱신 %d"),
            Applied, Normalized, Welded, NeighborChanges);
    }
    RefreshInternal();
    ValidateInternal();
#endif
}

#undef LOCTEXT_NAMESPACE
