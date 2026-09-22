// FF_FFZSZoneAccessSubsystem.h

// 존의 출입 기준점(EntryExitAnchors)이 저작되지 않아 NPC가 존에서 출구로 못 가는 문제를
// 런타임에 고친다. 존 경계 안에서 '실제로 걸어서 출구까지 갈 수 있는' 지점을 찾아 앵커로 넣는다.
//
// ─── 제작 데이터와 재현 근거 ─────────────────────────────────────────────────
// 앵커가 비어 있으면 소비 측은 존 액터 위치를 대표점으로 쓴다. Test_Main의 경계는 로컬
// (0,0)에서 시작하므로 그 위치는 중심이 아닌 모서리이며 투영 실패에 민감하다.
// 다만 과거 경로 단절 측정만으로 실제 지형 단차가 원인이라고 단정하면 안 된다.
// 메시 컴파일·물리 등록이 끝나기 전에 검사하면 빠진 충돌 때문에 연결을 잘못 판단할 수 있다.
// 에디터 도구는 PrepareWorldForNavigation 후 굽고, 별도 PIE에서 실제 출구·방향 경로를 검사한다.
// Test_Main의 현재 경계와 저작 앵커는 Tools/Build/navigation_layouts의 계획 및 맵에서 관리한다.
// 지형/충돌 변경 후 먼저 준비 완료·Build Paths·맵 저장을 수행한다. 이 서브시스템은 오래된
// NavMesh를 자동으로 다시 굽는 기능이 아니며 유효한 베이크 위에서만 접근점을 고른다.
//
// ─── 이 서브시스템이 하는 일 ──────────────────────────────────────────────────
// 앵커가 비어 있고 존 액터 위치에서 출구로 완전 경로가 없는 존만 대상으로, 존 경계 폴리곤 안을
// 훑어 NavMesh에 투영하고, 출구까지 완전 경로가 있는 지점 중 존 중심에 가장 가까운 것을
// 골라 EntryExitAnchors[0]에 넣는다. 훑는 격자는 존 크기의 1/8에서 시작해 단계마다 같은
// 원점에서 간격을 절반으로 줄이므로, 뒤 단계가 앞 단계를 포함해 결과가 격자 정렬에
// 좌우되지 않는다(초기 구현은 격자 간격에 따라 결과가 14~16개로 흔들렸다).
// 소비 측은 이미 "앵커가 있으면 앵커, 없으면 존 액터 위치" 규칙이라 별도 연동이 필요 없다.
//
// 지형을 바꾸지 않고 데이터를 바꾸는 방식이므로 NPC가 절벽을 오르거나 허공을 건너뛰지 않는다.
// 제한된 표본에서 접근점을 찾지 못하면 좌표와 함께 보고한다. 이 결과만으로 존 전체가
// 끊겼다고 단정하지 않는다. NavMesh 준비 상태·투영 범위·표본 간격과 실제 지형을 확인해야 한다.
//
// ─── 불변식·실패 ─────────────────────────────────────────────────────────────
// 게임 월드(Game/PIE)에서만 생성되고 런타임 값만 바꾼다. 에디터 월드는 건드리지 않으므로
// 레벨 에셋이 dirty해지지 않는다. 이미 저작된 앵커는 절대 덮어쓰지 않는다.
// 게임플레이 존이 없는 Frontend 월드는 검사하지 않는다. 존이 있는 월드에서 NavMesh나
// 출구를 못 찾으면 아무것도 바꾸지 않고 경고만 남긴다.
//
// 타이밍이 이 클래스의 유일한 전제다. 앵커를 쓰는 쪽(UFF_CrowdDecisionSubsystem의 보행거리
// 행렬)은 UFFZSWorldSubsystem의 WorldRevision이 바뀔 때만 다시 만드는데, 앵커를 직접 넣는
// 것으로는 그 리비전이 올라가지 않는다. 여기서는 OnWorldBeginPlay에서 고치므로 — 액터
// BeginPlay와 첫 결정 라운드보다 앞이라 — 항상 첫 빌드가 고쳐진 앵커를 보게 된다.
// 나중에 이 복구를 더 늦은 시점으로 옮긴다면 그때는 리비전을 함께 올려야 한다.
// ──────────────────────────────────────────────────────────────────────────────

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "FF_FFZSZoneAccessSubsystem.generated.h"

class AFFZSGameplayZone;

FF_ZONESYSTEM_API DECLARE_LOG_CATEGORY_EXTERN(LogFFZSZoneAccess, Log, All);

// 존 하나의 접근점 복구 결과. 진단·리포트용이며 게임 규칙에는 쓰이지 않는다.
USTRUCT()
struct FFFZSZoneAccessRepair
{
    GENERATED_BODY()

    UPROPERTY()
    FName ZoneId = NAME_None;

    // 존 경계 폴리곤의 한가운데와 새로 고른 접근점. 둘 다 월드 좌표다.
    UPROPERTY()
    FVector CentreWorld = FVector::ZeroVector;

    UPROPERTY()
    FVector AccessWorld = FVector::ZeroVector;

    // 존 한가운데에서 새 접근점까지의 수평 거리. 클수록 존이 지형과 안 맞았다는 뜻이다.
    UPROPERTY()
    float OffsetCm = 0.0f;
};

// 존 접근점을 걸어갈 수 있는 지점으로 맞춘다. 월드 서브시스템이라 레벨에 액터를 배치하지
// 않아도 게임 월드마다 자동 생성된다.
UCLASS()
class FF_ZONESYSTEM_API UFFZSZoneAccessSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // 게임 월드(Game/PIE)에만 만든다. 에디터 월드에서 존 속성을 바꾸면 레벨이 dirty해진다.
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

    // 액터 BeginPlay 전에 한 번 복구한다. 구워서 저장한 NavMesh가 준비되어 있어야 한다.
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

    // 앵커가 비어 있고 존 액터 위치에서 출구에 못 닿는 존의 접근점을 고쳐 넣는다.
    // 반환값은 고친 존 수다. 고칠 것이 없거나 NavMesh·출구가 없으면 0을 반환한다.
    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Zone Access")
    int32 RepairZoneAccessPoints();

    // 전체 존 수에 투영 실패 존도 포함해 보고한다. ffzs.ZoneAccess.Report 콘솔에서도 호출한다.
    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Zone Access")
    void LogReachabilityReport() const;

    const TArray<FFFZSZoneAccessRepair>& GetRepairs() const { return Repairs; }

    // 군중의 기본 퇴장 위치를 읽는다: 유효한 DespawnAchor 우선, 없으면
    // 스폰 액터 위치 + Temp_ExitOffsetFromOrigin. 입장용 SpawnAchor는 사용하지 않는다.
    // 액터 BeginPlay 전에도 조회할 수 있으며 월드·에셋을 변경하지 않는다.
    // 스폰 액터나 필요한 속성을 찾지 못하면 false이고 OutExit는 보존한다.
    bool ResolveExitLocation(FVector& OutExit) const;

private:
    bool HasGameplayZones() const;

    bool ProjectToNavMesh(const FVector& Point, FVector& OutProjected) const;

    // 완전 경로(부분 경로가 아닌)가 있으면 true. 같은 NavMesh 조각에 있다는 뜻이다.
    bool HasCompletePath(const FVector& A, const FVector& B) const;

    // 존이 실제로 쓰는 대표점. 앵커가 있으면 앵커, 없으면 존 액터 위치이다(소비 측 규칙과 동일).
    FVector GetZoneRepresentativePoint(const AFFZSGameplayZone& Zone) const;

    // 존 경계 안에서 출구까지 완전 경로가 있는 지점을 존 한가운데에 가까운 순으로 찾는다.
    // OutMiddleWorld에는 경계 폴리곤의 한가운데(월드)가 담긴다 — 리포트의 기준점이다.
    bool FindReachableAccessPoint(const AFFZSGameplayZone& Zone, const FVector& ExitNav,
                                  FVector& OutWorld, FVector& OutMiddleWorld) const;

    static bool IsPointInPolygon(const TArray<FVector2D>& Polygon, const FVector2D& Point);

    TArray<FFFZSZoneAccessRepair> Repairs;
};
