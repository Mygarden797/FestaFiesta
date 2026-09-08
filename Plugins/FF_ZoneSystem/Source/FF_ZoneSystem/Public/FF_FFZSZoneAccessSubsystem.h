// FF_FFZSZoneAccessSubsystem.h

// 존의 출입 기준점(EntryExitAnchors)이 저작되지 않아 NPC가 존에서 출구로 못 가는 문제를
// 런타임에 고친다. 존 경계 안에서 '실제로 걸어서 출구까지 갈 수 있는' 지점을 찾아 앵커로 넣는다.
//
// ─── 무엇이 문제였나 ──────────────────────────────────────────────────────────
// Test_Main의 지면은 서로 다른 높이의 평평한 슬래브를 이어 붙여 만들어져 있고, 슬래브 경계마다
// 45cm~582cm의 수직 단차가 있다. Agent Max Step Height를 넘는 단차는 NavMesh가 이어지지
// 않으므로 슬래브 하나하나가 독립된 NavMesh 조각이 된다. 측정 결과 존 대표점 31개가 7개
// 조각으로 갈라져 있었고, 출구까지 완전 경로가 있는 존은 9~11개뿐이었다.
//
// 그런데 갈라진 원인의 상당 부분은 지형이 아니라 '대표점을 어디로 잡는가'였다.
// EntryExitAnchors가 한 존도 저작되지 않아 소비 측(UFF_CrowdDecisionSubsystem)은 존 액터
// 위치를 대표점으로 쓴다. 그런데 존 경계 폴리곤은 로컬 (0,0)~(10000,10000)이라 액터 위치는
// 존의 한가운데가 아니라 '모서리'다 — 100m 격자에서 네 존이 만나는 꼭짓점이고, 하필 지형
// 슬래브의 이음새에 걸리기 쉽다. 같은 존 경계 안에 출구와 이어진 지면이 넓게 있는데도
// 모서리 한 점 때문에 그 존 전체가 '도달 불가'로 잡힌 것이다.
//
// ─── 이 서브시스템이 하는 일 ──────────────────────────────────────────────────
// 앵커가 비어 있고 존 중심에서 출구로 완전 경로가 없는 존만 대상으로, 존 경계 폴리곤 안을
// 훑어 NavMesh에 투영하고, 출구까지 완전 경로가 있는 지점 중 존 중심에 가장 가까운 것을
// 골라 EntryExitAnchors[0]에 넣는다. 훑는 격자는 존 크기의 1/8에서 시작해 단계마다 같은
// 원점에서 간격을 절반으로 줄이므로, 뒤 단계가 앞 단계를 포함해 결과가 격자 정렬에
// 좌우되지 않는다(초기 구현은 격자 간격에 따라 결과가 14~16개로 흔들렸다).
// 소비 측은 이미 "앵커가 있으면 앵커, 없으면 존 중심" 규칙이라 별도 연동이 필요 없다.
//
// 지형을 바꾸지 않고 데이터를 바꾸는 방식이므로 NPC가 절벽을 오르거나 허공을 건너뛰지 않는다.
// 앵커를 아무리 옮겨도 출구에 닿지 못하는 존은 경계 전체가 다른 지형 조각에 있다는 뜻이므로
// 좌표와 함께 오류로 보고한다 — 그건 레벨에서 램프를 놓거나 존을 옮겨야 하는 자리다.
//
// ─── 불변식·실패 ─────────────────────────────────────────────────────────────
// 게임 월드(Game/PIE)에서만 생성되고 런타임 값만 바꾼다. 에디터 월드는 건드리지 않으므로
// 레벨 에셋이 dirty해지지 않는다. 이미 저작된 앵커는 절대 덮어쓰지 않는다.
// NavMesh나 출구를 못 찾으면 아무것도 바꾸지 않고 경고만 남긴다.
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

    // 존과 NavMesh가 준비된 뒤 한 번 복구하고 결과를 로그로 남긴다.
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

    // 앵커가 비어 있고 존 중심에서 출구에 못 닿는 존의 접근점을 고쳐 넣는다.
    // 반환값은 고친 존 수다. 고칠 것이 없거나 NavMesh·출구가 없으면 0을 반환한다.
    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Zone Access")
    int32 RepairZoneAccessPoints();

    // 지금 상태에서 존별 출구 도달 가능 여부와 NavMesh 조각 수를 로그로 남긴다.
    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Zone Access")
    void LogReachabilityReport() const;

    const TArray<FFFZSZoneAccessRepair>& GetRepairs() const { return Repairs; }

private:
    // 퇴장 출구의 월드 좌표. AFF_SpawnActor의 위치 + Temp_ExitOffsetFromOrigin이며,
    // 게임 모듈에 의존하지 않도록 클래스 이름과 리플렉션으로 읽는다.
    bool ResolveExitLocation(FVector& OutExit) const;

    bool ProjectToNavMesh(const FVector& Point, FVector& OutProjected) const;

    // 완전 경로(부분 경로가 아닌)가 있으면 true. 같은 NavMesh 조각에 있다는 뜻이다.
    bool HasCompletePath(const FVector& A, const FVector& B) const;

    // 존이 실제로 쓰는 대표점. 앵커가 있으면 앵커, 없으면 존 중심이다(소비 측 규칙과 동일).
    FVector GetZoneRepresentativePoint(const AFFZSGameplayZone& Zone) const;

    // 존 경계 안에서 출구까지 완전 경로가 있는 지점을 존 한가운데에 가까운 순으로 찾는다.
    // OutMiddleWorld에는 경계 폴리곤의 한가운데(월드)가 담긴다 — 리포트의 기준점이다.
    bool FindReachableAccessPoint(const AFFZSGameplayZone& Zone, const FVector& ExitNav,
                                  FVector& OutWorld, FVector& OutMiddleWorld) const;

    static bool IsPointInPolygon(const TArray<FVector2D>& Polygon, const FVector2D& Point);

    TArray<FFFZSZoneAccessRepair> Repairs;
};
