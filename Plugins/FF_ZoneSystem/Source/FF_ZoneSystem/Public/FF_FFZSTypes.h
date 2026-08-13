// FF_FFZSTypes.h

// 시설·존·명령·대기열·저장·정책·읽기 모델이 공유하는 값 타입과 열거형을 정의한다.
// 흐름: 제작 데이터가 명령과 검증을 거쳐 런타임 상태가 되고, 스냅샷과 읽기 모델로 외부에 전달된다.
// 불변식: 길이는 cm, 시간은 초, 격자는 고정 1 cm 셀이며 저장 구조는 외부 UObject 수명을 소유하지 않는다.
//실패/성능: 유효성 오류는 누적 보고하고 배열 크기는 열거 예산과 상위 검증으로 제한한다.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"
#include "FF_FFZSTypes.generated.h"

class AActor;
class AFFZSFacilityActor;
class UStaticMesh;

// 존 허용 규칙과 UI 분류에 쓰는 시설의 기능 카테고리.
// 새 값 추가 시 저장 호환성, FacilityDefinition Data Asset, 정책 어댑터, UI 표시를 함께 갱신.
UENUM(BlueprintType)
enum class EFFZSFacilityCategory : uint8
{
    Booth UMETA(DisplayName = "Booth", ToolTip = "판매·체험·운영 부스 시설입니다."),
    Toilet UMETA(DisplayName = "Toilet", ToolTip = "화장실과 위생 시설입니다."),
    Info UMETA(DisplayName = "Info", ToolTip = "안내소와 정보 제공 시설입니다."),
    Rest UMETA(DisplayName = "Rest", ToolTip = "좌석·쉼터 등 휴식 시설입니다."),
    Performance UMETA(DisplayName = "Performance", ToolTip = "무대와 공연 운영 시설입니다."),
    Safety UMETA(DisplayName = "Safety", ToolTip = "의무실·안전 지원 시설입니다."),
    PhotoSpot UMETA(DisplayName = "Photo Spot", ToolTip = "랜드마크와 촬영 시설입니다.")
};

//권한 시설 수명주기.
//Planned/Validated/Closed/MovePending만 비점유일 때 이동·제거할 수 있고,
//Open 명령은 Validated·Closed·MovePending을 Active로 만든다. Removed는 종단 상태다.
UENUM(BlueprintType)
enum class EFFZSFacilityState : uint8
{
    Planned,
    Validated,
    Active,
    Closed,
    MovePending,
    Removed
};

//시설 큐의 참가·처리 상태.
// Open은 참가 가능, Full은 정원 도달, Draining은 신규 참가 금지 후 비우기,
// Closed는 완전히 비어 있음, Blocked는 이전 의미 상태를 보존한 일시 접근 장애다.
UENUM(BlueprintType)
enum class EFFZSQueueState : uint8
{
    Open,
    Full,
    Draining,
    Closed,
    Blocked
};

// 참가 요청의 완료 결과. 실패는 무효 ID/액터, 전역 중복, 정원, 비개방, 시설 누락으로 구분된다.
// Joined가 아니면 OutTicketId는 유효하지 않다.
UENUM(BlueprintType)
enum class EFFZSQueueJoinResult : uint8
{
    Joined,
    InvalidAgent,
    DuplicateAgent,
    Full,
    NotOpen,
    FacilityNotFound
};

// 최종 대기열 소유권 해제 사유.
// ServiceCompleted만 완료 통계, Abandoned만 포기 통계에 반영하며 Cancelled와 RemovedBySystem은 별도 운영 사유다.
UENUM(BlueprintType)
enum class EFFZSQueueReleaseReason : uint8
{
    ServiceCompleted,
    Abandoned,
    Cancelled,
    RemovedBySystem
};

// 운영 작업 단계. Install·Move·Remove는 InitialSetup과 MorningCheck에서만 허용한다.
// 나머지 단계는 Open·Close·EmergencyClose 같은 운영 수명주기에 사용한다.

UENUM(BlueprintType)
enum class EFFZSChangePhase : uint8
{
    InitialSetup = 0,
    // 값 1은 제거된 이전 단계의 저장값이 다른 단계로 오해석되지 않도록 비워 둔다.
    MorningCheck = 2,
    LiveOperation = 3,
    EveningSettlement = 4
};

// WorldSubsystem이 직렬화하는 시설 권한 명령.
// Install·Move는 배치 검증, Remove는 점유·상태 검증, Open·Close·EmergencyClose는 수명주기 검증을 거친다.
UENUM(BlueprintType)
enum class EFFZSCommandType : uint8
{
    Install,
    Move,
    Remove,
    Open,
    Close,
    EmergencyClose
};

// 검증·명령·정책의 안정 실패 코드.
// ID/데이터, 공간/지면/충돌, 보호 동선/내비, 단계/상태/점유, 동시성/멱등성,
// 외부 정책과 내부 오류를 구분한다. UI 문구는 코드와 별도로 현지화한다.
UENUM(BlueprintType)
enum class EFFZSPlacementFailure : uint8
{
    None,
    ZoneNotFound,
    FacilityNotFound,
    DuplicateZoneId,
    DuplicateFacilityId,
    CategoryNotAllowed,
    OutsideZone,
    InvalidGround,
    ExcessSlope,
    HeightVarianceExceeded,
    PlacementCollision,
    InsufficientClearance,
    QueueFootprintBlocked,
    ProtectedRouteBlocked,
    RequiredRouteDisconnected,
    AccessibilityRouteBlocked,
    WrongStage,
    FacilityOccupied,
    InvalidFacilityState,
    StaleWorldRevision,
    DuplicatePayloadMismatch,
    NavigationUnavailable,
    NavRebuildTimeout,
    DataInvalid,
    ExternalPolicyRejected,
    InternalError,
    // 경제 정책이 현재 잔액으로 명령 비용을 지불할 수 없음을 구조적으로 나타낸다.
    // 기존 SaveGame enum ordinal을 보존하기 위해 새 값은 항상 목록 끝에 추가한다.
    InsufficientFunds
};

// 시설 내 이동 의미 지점. Entry는 진입, Service는 처리 위치, Exit는 최종 소유권 해제 전 도착 위치
UENUM(BlueprintType)
enum class EFFZSAnchorType : uint8
{
    Entry UMETA(DisplayName = "Entry", ToolTip = "시설 또는 대기열로 접근할 때 사용하는 진입 지점입니다."),
    Service UMETA(DisplayName = "Service", ToolTip = "손님이 실제 서비스를 받는 지점입니다. ServiceCapacity 수만큼 0부터 연속 인덱스가 필요합니다."),
    Exit UMETA(DisplayName = "Exit", ToolTip = "서비스 완료 또는 이탈 후 이동하는 최종 퇴장 지점입니다.")
};

// VisualMesh의 원본 bounds를 시설의 권위 배치 크기에 맞추는 방법이다.
// Actor 자체의 Scale은 언제나 1로 유지하고 표시 전용 메시 컴포넌트에만 이 정책을 적용한다.
UENUM(BlueprintType)
enum class EFFZSVisualSizingMode : uint8
{
    SourceMeshSize UMETA(DisplayName = "Source Mesh Size", ToolTip = "임포트된 Static Mesh 크기를 그대로 씁니다. 건물 크기를 바꿔도 화면 메시 크기는 바뀌지 않으므로 레거시·아티스트 예외에만 사용하십시오."),

    UniformFit UMETA(DisplayName = "Uniform Fit", ToolTip = "메시 종횡비를 유지하면서 건물 크기와 장애물 높이 안에 들어오도록 균일하게 축소·확대합니다."),

    MatchPlacementBounds UMETA(DisplayName = "Match Placement Bounds", ToolTip = "표시 메시를 건물 가로·깊이와 배치 장애물 높이에 정확히 맞춥니다. UFFZSFacilityDefinition을 유일한 크기 원천으로 쓰는 프로덕션 권장 모드입니다.")
};

// 배치가 침범하면 안 되는 영역 의미.
// NoBuild와 ViewingArea는 명시 영역, 세 Route 유형은 Project Settings의 기본 확보 폭을 상속할 수 있다.
UENUM(BlueprintType)
enum class EFFZSProtectedAreaType : uint8
{
    NoBuild,
    GeneralRoute,
    EmergencyServiceRoute,
    AccessibilityRoute,
    ViewingArea
};


// 인구/SoftCapacity 비율의 운영 단계.
//  Caution은 즉시, Warning·Critical은 각 유지 시간을 충족한 뒤 상승하며 기준 아래에서는 즉시 낮아질 수 있다.
UENUM(BlueprintType)
enum class EFFZSDensityState : uint8
{
    Normal,
    Caution,
    Warning,
    Critical
};

// 외부 협업 정책의 책임 도메인.
// 운영 준비 모드에서는 Navigation·Economy·SimulationTime·UserInterface·Safety·Experience에
// 각각 정확히 한 제공자가 필요하고 Custom은 여러 독립 제공자를 허용한다.
UENUM(BlueprintType)
enum class EFFZSPolicyDomain : uint8
{
    Navigation,
    Economy,
    SimulationTime,
    UserInterface,
    Safety,
    Experience,
    Custom
};

// 존 로컬 고정 격자에서 시설의 원점과 시계 방향 90도 회전을 나타내는 값 타입.
// QuarterTurns는 명령 생성·시설 적용 때 0..3으로 정규화된다.
USTRUCT(BlueprintType)
struct FF_ZONESYSTEM_API FFFZSGridPlacement
{
    GENERATED_BODY()
    
    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "FF Zone System", meta = (DisplayName = "Origin Cell", ToolTip = "회전하기 전 건물 Footprint 왼쪽 아래가 놓일 Zone 로컬 1cm 셀 좌표입니다."))
    FIntPoint OriginCell = FIntPoint::ZeroValue;
    // 회전하지 않은 Facility Footprint 좌하단의 Grid 좌표다.//
    
    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "FF Zone System", meta = (DisplayName = "Quarter Turns", ToolTip = "시계 방향 90도 회전 횟수입니다. 0·1·2·3 중 하나로 정규화됩니다.", ClampMin = "0", ClampMax = "3"))
    int32 QuarterTurns = 0;
    // 시계 방향 90도 회전 횟수다. 값은 0, 1, 2 또는 3으로 정규화된다.//
    bool operator==(const FFFZSGridPlacement& Other) const
    {
        return OriginCell == Other.OriginCell && QuarterTurns == Other.QuarterTurns;
    }
};

FORCEINLINE uint32 GetTypeHash(const FFFZSGridPlacement& Value)
{
    return HashCombine(GetTypeHash(Value.OriginCell), GetTypeHash(Value.QuarterTurns));
}
// 원점과 회전값을 결합해 TMap/TSet용 해시를 만든다.//

USTRUCT(BlueprintType)
 // 시설 사양에 저장되는 논리 앵커 정의.
 // Type+Index 조합은 시설 안에서 유일해야 하며 LocalTransform은 스케일 1의 시설 로컬 cm 좌표다.

struct FF_ZONESYSTEM_API FFFZSAnchorDefinition
{
    GENERATED_BODY()

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "FF Zone System", meta = (DisplayName = "Anchor Type", ToolTip = "진입·서비스·퇴장 중 이 지점의 역할입니다."))
    EFFZSAnchorType Type = EFFZSAnchorType::Entry;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "FF Zone System", meta = (DisplayName = "Anchor Index", ToolTip = "같은 종류 안에서의 번호입니다. Entry[0]·Exit[0]이 필요하고 Service는 0부터 ServiceCapacity-1까지 연속이어야 합니다.", ClampMin = "0"))
    int32 Index = 0;

    // Unreal은 1uu = 1cm를 사용하므로 단위는 센티미터다.//
    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "FF Zone System", meta = (DisplayName = "Local Transform", ToolTip = "건물 중심을 원점으로 한 시설 로컬 cm 변환입니다. 위치·회전만 사용하며 Scale은 반드시 1이어야 합니다."))
    FTransform LocalTransform = FTransform::Identity;
};

USTRUCT(BlueprintType)
 // 시설 하나의 대기·서비스·도착 오차·차단 회복 수치.
 // 담당자는 Data Asset에서 조정하되 MaxWaitingAgents와 ServiceCapacity는 1 이상,
 // 시간·거리 값은 0 이상의 검증 범위를 지켜 실제 처리량을 측정한다.
struct FF_ZONESYSTEM_API FFFZSQueueRules
{
    GENERATED_BODY()

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Queue", meta = (DisplayName = "Max Waiting Agents", ToolTip = "줄에 동시에 기다릴 수 있는 최대 인원입니다. 서비스 중·퇴장 중 인원은 포함하지 않습니다. 경로 길이만 늘려도 이 값은 자동 증가하지 않습니다.", ClampMin = "1"))
    int32 MaxWaitingAgents = 12;
    // Waiting 배열에 허용할 최대 인원. ActiveServices와 Exiting은 이 수에 포함하지 않는다.
    
    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Queue", meta = (DisplayName = "Slot Spacing (cm)", ToolTip = "대기열 경로를 따라 배치되는 인접 슬롯 중심 사이 거리(cm)입니다. 필요한 경로 길이는 (최대 대기 인원 - 1) × 슬롯 간격입니다.", ClampMin = "1.0", Units = "cm"))
    float SlotSpacingCm = 115.0f;
    // queue spline 위 인접 대기 슬롯 중심 간격 cm. 115 cm는 측정이 필요한 프로토타입 기본값이다.

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Queue", meta = (DisplayName = "Service Capacity", ToolTip = "동시에 서비스를 받을 수 있는 손님 수입니다. 같은 수의 Service 앵커를 0부터 연속 번호로 만들어야 합니다.", ClampMin = "1"))
    // 동시에 ServiceIndex를 배정할 수 있는 최대 항목 수다.
    int32 ServiceCapacity = 1;
    // 동시에 ServiceIndex를 배정할 수 있는 최대 항목 수다.
    
    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Queue", meta = (DisplayName = "Service Duration (Seconds)", ToolTip = "손님이 Service 앵커에 도착한 뒤 서비스가 완료될 때까지 걸리는 시간(초)입니다.", ClampMin = "0.01", Units = "s"))
    float ServiceDurationSeconds = 4.0f;
    // Service 앵커 도착 보고 뒤 완료까지 필요한 시뮬레이션 시간이다.

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Queue", meta = (DisplayName = "Abandon After (Seconds)", ToolTip = "대기 시간이 이 값(초)에 도달하면 포기 흐름을 시작합니다. 0은 자동 포기를 끕니다.", ClampMin = "0.0", Units = "s"))
    float AbandonAfterSeconds = 30.0f;
    //양수면 Waiting 누적 시간이 이 값에 도달할 때 포기 흐름을 시작한다. 0은 자동 포기를 끈다.

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Queue", meta = (DisplayName = "Slot Arrival Tolerance (cm)", ToolTip = "대기 슬롯·서비스·퇴장 목표에 도착했다고 인정하는 월드 XY 거리 오차(cm)입니다.", ClampMin = "0.0", Units = "cm"))
    float SlotArrivalToleranceCm = 25.0f;
    // queue·service·exit 목표 도착 보고에 허용하는 월드 거리 오차다.//

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Queue", meta = (DisplayName = "Navigation Projection Extent (cm)", ToolTip = "대기열 이동 목표를 NavMesh에 투영할 때 찾는 범위(cm)입니다. 실제 이동 경로 생성은 NPC·Navigation 시스템이 담당합니다.", ClampMin = "0.0", Units = "cm"))
    float NavigationProjectionExtentCm = 30.0f;
    // queue 이동 목표의 NavMesh 투영 검색 범위다. 실제 경로 생성은 외부 Navigation 책임이다.

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Queue", meta = (DisplayName = "Blocked Timeout (Seconds)", ToolTip = "Blocked 상태가 이 시간(초)만큼 지속될 때 재경로를 요청합니다. 0은 시간 기반 재시도를 끕니다.", ClampMin = "0.0", Units = "s"))
    float BlockedTimeoutSeconds = 5.0f;
    // 양수면 Blocked 상태에서 주기적 재경로 요청을 만드는 누적 시간 간격이다. 0은 시간 기반 요청을 끈다.

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Queue", meta = (DisplayName = "Max Repath Attempts", ToolTip = "막힘 또는 이동 실패 때 허용할 재경로 요청 최대 횟수입니다. 0은 재시도하지 않습니다.", ClampMin = "0"))
    int32 MaxRepathAttempts = 2;
    // 차단 구간의 시간 기반 요청과 에이전트별 이동 실패 재시도에 적용되는 최대 횟수다. 0은 재시도하지 않는다.//
};

USTRUCT(BlueprintType)
 // 1 cm 정수 격자에서 [Min, Max) 반열림 구간으로 표현하는 축 정렬 사각형이다.
 // 셀을 수십만 개의 FIntPoint로 펼치지 않고 네 정수만 저장하므로 600 cm x 300 cm 시설도
 // 한 개의 값으로 표현된다. Max는 포함하지 않기 때문에 폭은 Max.X-Min.X이며, 서로 맞닿기만
 // 하는 두 사각형은 겹치지 않는다. 모든 권위 점유·저장·충돌 좁은 단계가 이 타입을 사용한다.
struct FF_ZONESYSTEM_API FFFZSCellRect
{
    GENERATED_BODY()
    
    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "FF Zone System|Grid", meta = (DisplayName = "Min (Inclusive)", ToolTip = "사각형에 포함되는 왼쪽 아래 1cm 셀 좌표입니다."))
    FIntPoint Min = FIntPoint::ZeroValue;
    // 포함되는 최소 셀 좌표다.
    
    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "FF Zone System|Grid", meta = (DisplayName = "Max (Exclusive)", ToolTip = "사각형에 포함되지 않는 오른쪽 위 경계 좌표입니다. 실제 폭·높이는 Max - Min입니다."))
    FIntPoint Max = FIntPoint::ZeroValue;
    // 포함되지 않는 최대 셀 좌표다.

    FFFZSCellRect() = default;
    FFFZSCellRect(const FIntPoint InMin, const FIntPoint InMax) : Min(InMin), Max(InMax) {}
    
    bool IsValid() const { return Min.X < Max.X && Min.Y < Max.Y; }
    // 양 축 길이가 모두 양수일 때만 실제 영역이다.
    
    int64 GetWidth() const { return IsValid() ? static_cast<int64>(Max.X) - Min.X : 0; }
    // X축 셀 수. IsValid가 false면 0을 반환한다.
    
    int64 GetHeight() const { return IsValid() ? static_cast<int64>(Max.Y) - Min.Y : 0; }
    // Y축 셀 수. IsValid가 false면 0을 반환한다.

    
    int64 GetArea() const
    {
        const int64 Width = GetWidth();
        const int64 Height = GetHeight();
        if (Width <= 0 || Height <= 0) return 0;
        return Width <= MAX_int64 / Height ? Width * Height : -1;
    }

    // 반열림 경계 규칙으로 한 셀의 포함 여부를 검사한다.//
    bool Contains(const FIntPoint Cell) const
    {
        return Cell.X >= Min.X && Cell.X < Max.X && Cell.Y >= Min.Y && Cell.Y < Max.Y;
    }

    // 면적이 양수인 교집합이 있을 때만 true다. 경계 접촉만으로는 겹치지 않는다.//
    bool Intersects(const FFFZSCellRect& Other) const
    {
        return Min.X < Other.Max.X && Max.X > Other.Min.X &&
            Min.Y < Other.Max.Y && Max.Y > Other.Min.Y;
    }

    bool operator==(const FFFZSCellRect& Other) const
    {
        return Min == Other.Min && Max == Other.Max;
    }
};

// 사각형 네 모서리를 결합한 값 해시다. 디버그·보조 집합에서만 사용한다.//
FORCEINLINE uint32 GetTypeHash(const FFFZSCellRect& Value)
{
    return HashCombine(GetTypeHash(Value.Min), GetTypeHash(Value.Max));
}
USTRUCT(BlueprintType)

 // 시설 종류 하나를 완전히 설명하는 저장 가능한 값 사양.
 // 격자 공간, queue 경로·규칙, 앵커, 존 제한, 외부 시스템 키, 소프트 비주얼을 포함하며
 // 런타임 액터에 복사되어 원본 Data Asset 로드 상태와 분리된다.

struct FF_ZONESYSTEM_API FFFZSFacilitySpec
{
    GENERATED_BODY()

    // 이 런타임이 해석하는 유일한 사양 저장 형식(1 Cell = 1 cm)이다.
    // [하드코딩 제거] 기본값·검증·구 스냅샷 마이그레이션이 각자 리터럴 2를 적고 있어서
    // 스키마를 올릴 때 한 군데를 빠뜨리기 쉬웠다. 세 곳이 이 상수를 공유한다.
    static constexpr int32 CurrentSchemaVersion = 2;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Identity", meta = (DisplayName = "Schema Version", ToolTip = "시설 정의 저장 형식 버전입니다. 마이그레이션 코드 없이 제작자가 임의로 바꾸면 안 됩니다.", ClampMin = "1"))
    int32 SchemaVersion = CurrentSchemaVersion;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Identity", meta = (DisplayName = "Facility Definition ID", ToolTip = "카탈로그·저장·연동이 사용하는 영구 고유 ID입니다. 다른 시설과 중복할 수 없고 출시 후 변경하면 기존 저장 참조가 끊길 수 있습니다."))
    FName FacilityDefinitionId = NAME_None;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Identity", meta = (DisplayName = "Display Name", ToolTip = "건설 카탈로그와 UI에 표시할 현지화 가능한 시설 이름입니다."))
    FText DisplayName;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Identity", meta = (DisplayName = "Category", ToolTip = "시설의 기능 분류입니다. Zone 허용 규칙과 카탈로그 분류에 사용됩니다."))
    EFFZSFacilityCategory Category = EFFZSFacilityCategory::Booth;
    
    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (DisplayName = "Footprint Size (cm)", ToolTip = "회전 전 건물이 차지하는 바닥 크기입니다. 1 Cell은 1cm이므로 X=가로 폭, Y=세로 깊이입니다. 예: 600×300은 6m×3m입니다. (가로+여유)×(깊이+여유)와 대기열 예약 면적의 안전 예산이 10,000,000셀을 넘으면 검증에 실패합니다.", ClampMin = "1"))
    FIntPoint FootprintSizeCells = FIntPoint(100, 100);
    // 시설의 실제 바닥 크기를 1 cm 셀 수로 저장한다. 예: 6 m x 3 m는 (600, 300)이다.
    
    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (DisplayName = "Clearance (cm)", ToolTip = "건물 크기 네 방향에 추가로 확보하는 안전·통행 여백입니다. 값이 커질수록 배치 필요 면적과 안전 예산 사용량이 함께 커지며, 다른 시설과 장애물에서 더 멀리 떨어져야 합니다.", ClampMin = "0"))
    int32 ClearanceCells = 100;
    // 시설 둘레 확보 폭을 1 cm 셀 수로 저장한다. 예: 1 m는 100이다.//
    
    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (DisplayName = "Queue Reserved Rects", ToolTip = "다른 시설과 장애물이 침범하지 못하도록 확보할 대기열 영역입니다. 좌표 원점은 회전 전 건물의 왼쪽 아래이고 단위는 1cm이며 Max 경계는 포함하지 않습니다. 건물 바닥과 면적으로 겹치면 안 되고, 예약 면적은 10,000,000셀 안전 예산에 포함됩니다. 영역만 늘려도 실제 대기 인원은 늘지 않습니다."))
    TArray<FFFZSCellRect> QueueReservedRects;
    // 회전 전 footprint 왼쪽 아래를 원점으로 하는 최대 대기열 예약 사각형들이다.
    // 서로 겹치거나 맞닿아도 런타임에서 결정적으로 합쳐지며 실제 좌표 단위는 1 cm 셀이다.
    
    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (DisplayName = "Legacy Queue Reserved Cells", ToolTip = "구버전 호환용 개별 1cm 셀 목록입니다. 새 시설은 사용하지 말고 대기열 예약 사각형을 사용하십시오.", DeprecatedProperty, DeprecationMessage = "QueueReservedRects를 사용하십시오. 개별 1cm Cell 목록은 확장성이 없습니다."))
    TArray<FIntPoint> QueueReservedCells;
    // 구 스키마/C++ 호출부를 잠시 수용하는 1 cm 단일 셀 목록이다.
    // 새 콘텐츠는 QueueReservedRects를 사용한다. 큰 영역을 이 배열로 만들면 검증에서 거부된다.
    
    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Queue", meta = (DisplayName = "Queue Spline Points (cm)", ToolTip = "건물 중심 기준 시설 로컬 cm 좌표의 선형 경로입니다. Index 0이 줄 머리·서비스 쪽이고 마지막 점이 줄 꼬리입니다. 모든 대기 슬롯 중심이 예약 사각형 안에 있어야 합니다."))
    TArray<FVector> QueueSplinePointsCm;
    // Service 지점에서 Queue 끝 방향으로 정렬된 Local 센티미터 단위 Queue Spline Point다.

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Queue", meta = (DisplayName = "Queue Rules", ToolTip = "대기 정원·슬롯 간격·동시 서비스 수·처리 시간·이탈·내비 재시도 규칙입니다."))
    FFFZSQueueRules QueueRules;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Anchors", meta = (DisplayName = "Entry / Service / Exit Anchors", ToolTip = "건물 중심 기준 시설 로컬 cm 좌표의 이동 지점 목록입니다. Entry[0]·Exit[0]과 Service[0..ServiceCapacity-1]가 필요합니다."))
    TArray<FFFZSAnchorDefinition> Anchors;
    
    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (DisplayName = "Allowed Zone IDs", ToolTip = "시설을 배치할 수 있는 Zone ID 제한 목록입니다. 비어 있으면 시설 분류와 Zone 정책만 사용합니다."))
    TArray<FName> AllowedZoneIds;
    // 비어 있으면 이 정의는 Zone 카테고리 규칙에만 의존한다.

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "External Keys", meta = (DisplayName = "Install Cost Key", ToolTip = "외부 경제 시스템에서 설치 비용을 조회할 키입니다. None이면 프로젝트 정책에 따라 비용이 없거나 별도 기본값을 사용합니다."))
    FName InstallCostKey = NAME_None;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "External Keys", meta = (DisplayName = "Revenue Rule Key", ToolTip = "외부 경제 시스템에서 시설 수익 규칙을 조회할 키입니다."))
    FName RevenueRuleKey = NAME_None;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "External Keys", meta = (DisplayName = "Experience Effect Key", ToolTip = "외부 경험·만족도 시스템에서 시설 효과를 조회할 키입니다."))
    FName ExperienceEffectKey = NAME_None;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Visual", meta = (DisplayName = "Visual Mesh", ToolTip = "월드와 배치 미리보기에 표시할 Static Mesh입니다. 배치 권위 크기는 이 메시 Collision이 아니라 건물 크기·여유 폭·대기열 예약 영역입니다.", AssetBundles = "FacilityVisual"))
    TSoftObjectPtr<UStaticMesh> VisualMesh;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Visual", meta = (DisplayName = "Visual Sizing Mode", ToolTip = "Static Mesh 원본 Bounds를 권위 배치 크기에 맞추는 방식입니다. 프로덕션 시설은 '배치 경계에 정확히 맞춤'을 사용하십시오."))
    EFFZSVisualSizingMode VisualSizingMode = EFFZSVisualSizingMode::SourceMeshSize;
    // SourceMeshSize는 기존 임포트 크기를 보존한다. 나머지 모드는 Footprint/ObstacleHeight를 유일한 목표 크기로 사용한다.

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Visual", meta = (DisplayName = "Visual Offset (cm)", ToolTip = "자동 중심·지면 정렬 뒤 더하는 시설 로컬 위치 보정입니다. '배치 경계에 정확히 맞춤'에서는 권위 경계 일치를 위해 0이어야 합니다.", Units = "cm"))
    FVector VisualOffsetCm = FVector::ZeroVector;
    // 자동 중심/지면 정렬이 끝난 뒤 더하는 시설 로컬 위치 보정값이다.

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Visual", meta = (DisplayName = "Visual Rotation", ToolTip = "메시 원본의 앞 방향을 시설 로컬 +Y 손님 방향에 맞추는 회전입니다. 정확 맞춤 모드에서는 0·90·180·270도 Yaw만 허용합니다."))
    FRotator VisualRotation = FRotator::ZeroRotator;
    // 메시 원본의 전면 방향을 시설 로컬 +Y 계약에 맞추는 회전이다.

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (DisplayName = "Placement Obstacle Height (cm)", ToolTip = "배치 충돌·보호 영역 검사에 사용할 높이입니다. '배치 경계에 정확히 맞춤'에서는 표시 메시의 목표 높이로도 사용됩니다.", ClampMin = "10.0", Units = "cm"))
    float PlacementObstacleHeightCm = 250.0f;
    
    bool IsValid(TArray<FText>& OutErrors) const;
    // 사양을 변경하지 않고 ID·크기·예산·큐·앵커·수치의 모든 오류를 누적한다. 오류가 없을 때 true다.
};

USTRUCT(BlueprintType)
 // 배치 읽기 검증 결과와 계산 산출물.
 // 유효할 때 Transform과 네 압축 사각형 배열을 실행 단계가 재사용하며, 실패 시 Reasons와 Summary가 원인을 보존한다.
struct FF_ZONESYSTEM_API FFFZSPlacementValidation
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation")
    bool bIsValid = false;
    // Reasons가 비어 있고 모든 로컬·정책 검증을 통과했을 때만 true다.

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation")
    TArray<EFFZSPlacementFailure> Reasons;
    // 중복 제거된 안정 실패 코드 목록. 첫 항목은 명령의 대표 실패 코드로 사용될 수 있다.

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation")
    FText Summary;
    // 누적 실패 설명. 로직 분기에는 Reasons를 사용하고 이 값은 UI 표시용으로 취급한다.

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation")
    FTransform PlacementTransform = FTransform::Identity;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation")
    TArray<FFFZSCellRect> FootprintRects;
    // 권위 본체 영역. 일반 시설은 회전 후에도 한 사각형이다.
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation")
    TArray<FFFZSCellRect> ClearanceRects;
    // 본체와 Queue를 제외한 권위 여유 영역.
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation")
    TArray<FFFZSCellRect> QueueRects;
    // 최대 대기열의 권위 압축 영역.
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation")
    TArray<FFFZSCellRect> ReservedRects;
    // FootprintRects·ClearanceRects·QueueRects의 정확한 합집합이며 점유 인덱스 입력이다.
    
    // 소규모 디버그/기존 Blueprint 호환용 개별 셀 보기다. 영역이 10,000셀을 넘으면 빈 배열이다.
    // 권위 판정에는 절대 사용하지 않는다.
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation|Legacy")
    TArray<FIntPoint> FootprintCells;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation|Legacy")
    TArray<FIntPoint> ClearanceCells;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation|Legacy")
    TArray<FIntPoint> QueueCells;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation|Legacy")
    TArray<FIntPoint> ReservedCells;
    
    void AddFailure(EFFZSPlacementFailure Reason, const FText& Message);
    // 중복되지 않은 실패 코드를 추가하고 Summary를 누적한 뒤 bIsValid를 false로 만든다.
};

USTRUCT(BlueprintType)
 // 모든 시설 권한 변경의 불변 입력 메시지.
 // CommandId는 멱등성 키, TransactionId는 상위 작업 상관관계, ExpectedWorldRevision은 선택적 낙관적 잠금이다.
 // 같은 CommandId를 재사용하려면 전체 의미론적 페이로드가 완전히 같아야 한다.
struct FF_ZONESYSTEM_API FFFZSCommand
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
    FGuid CommandId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
    FGuid TransactionId;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
    int64 ExpectedWorldRevision = -1;
    // -1은 실행 시 최신 리비전을 허용한다. 0 이상은 현재 WorldRevision과 정확히 일치해야 하는 낙관적 동시성 조건이다.

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
    EFFZSCommandType Type = EFFZSCommandType::Install;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
    FGuid FacilityInstanceId;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
    FFFZSFacilitySpec FacilitySpec;
    // Install에는 유효한 전체 사양이 필요하다. Move는 기존 인스턴스의 복사 사양을 사용하므로 비워 둘 수 있다.

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
    FName ZoneId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
    FFFZSGridPlacement Placement;
};

USTRUCT(BlueprintType)

 // 명령의 안정 결과 값.
 // 성공 시 WorldRevision과 관련 FacilityActor를 제공하며, 동일 페이로드 재실행이면 bWasDuplicateReplay가 true다.
 // 실패 시 FailureReason과 Message를 사용하고 포인터 존재를 가정하지 않는다.

struct FF_ZONESYSTEM_API FFFZSCommandResult
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Result")
    bool bSucceeded = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Result")
    bool bWasDuplicateReplay = false;
    // 같은 CommandId와 같은 전체 지문의 완료 영수증을 재사용한 결과면 true다. 새 변경이나 이벤트는 만들지 않는다.

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Result")
    EFFZSPlacementFailure FailureReason = EFFZSPlacementFailure::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Result")
    FText Message;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Result")
    int64 WorldRevision = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Result")
    TObjectPtr<AFFZSFacilityActor> FacilityActor = nullptr;
};

USTRUCT(BlueprintType)
 // 대기열 항목 하나의 UObject 없는 SaveGame 상태.
 // AgentActor는 저장하지 않으며 로드 후 안정 AgentId로 재바인딩한다. Waiting·Service·Exiting 단계 정보를 모두 보존한다.

struct FF_ZONESYSTEM_API FFFZSQueueEntrySnapshot
{
    GENERATED_BODY()

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Queue")
    FGuid TicketId;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Queue")
    FName AgentId = NAME_None;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Queue")
    float WaitSeconds = 0.0f;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Queue")
    float ServiceElapsedSeconds = 0.0f;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Queue")
    int64 JoinSequence = 0;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Queue")
    bool bInService = false;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Queue")
    bool bArrivedAtAssignedSlot = false;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Queue")
    bool bArrivedAtServicePoint = false;
    
    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Queue")
    bool bExiting = false;
    // 물리 에이전트가 Exit로 이동 중이며 전역 대기열 소유권이 아직 남아 있을 때 true다.

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Queue")
    int32 MovementRepathCount = 0;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Queue")
    int32 AssignedQueueIndex = INDEX_NONE;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Queue")
    int32 ServiceIndex = INDEX_NONE;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Queue")
    int32 ExitIndex = INDEX_NONE;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Queue")
    EFFZSQueueReleaseReason PendingReleaseReason = EFFZSQueueReleaseReason::RemovedBySystem;
};

USTRUCT(BlueprintType)
 // 시설 큐 전체의 저장 상태.
 // 의미 상태, 모든 항목, FIFO 다음 순서, 통계, Blocked 회복 값, Drain 의도를 포함해 시간 진행을 이어 간다.
struct FF_ZONESYSTEM_API FFFZSQueueSnapshot
{
    GENERATED_BODY()

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Queue")
    EFFZSQueueState State = EFFZSQueueState::Closed;
    
    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Queue")
    EFFZSQueueState StateBeforeBlocked = EFFZSQueueState::Closed;
    // 일시 Blocked가 해제될 때 복구할 Open·Full·Draining·Closed 의미 상태다.

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Queue")
    TArray<FFFZSQueueEntrySnapshot> Entries;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Queue")
    int64 NextJoinSequence = 1;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Queue")
    int32 CompletedCount = 0;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Queue")
    int32 AbandonedCount = 0;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Queue")
    float TotalCompletedWaitSeconds = 0.0f;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Queue")
    float BlockedElapsedSeconds = 0.0f;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Queue")
    int32 RepathAttemptCount = 0;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Queue")
    bool bDrainRequested = false;
};

USTRUCT(BlueprintType)

 // 권한 시설 액터 하나와 그 큐의 값 스냅샷.
 // 사양을 내장하므로 원본 Data Asset이 없어도 복원할 수 있으며 존과 스키마 유효성은 월드 복원 전에 검사한다.
struct FF_ZONESYSTEM_API FFFZSFacilityInstanceSnapshot
{
    GENERATED_BODY()

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    FGuid FacilityInstanceId;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    FFFZSFacilitySpec FacilitySpec;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    FName ZoneId = NAME_None;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    FFFZSGridPlacement Placement;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    EFFZSFacilityState State = EFFZSFacilityState::Planned;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    FTransform WorldTransform = FTransform::Identity;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    bool bNeedsRelocation = false;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    FFFZSQueueSnapshot Queue;
};
USTRUCT(BlueprintType)

 // 재시작 뒤에도 CommandId 멱등성을 유지하는 저장 영수증.
 // 전체 PayloadFingerprint와 결과 요약, 선택적 시설 참조를 저장하며 실행 중 영수증은 저장 대상이 아니다.

struct FF_ZONESYSTEM_API FFFZSCommandReceiptSnapshot
{
    GENERATED_BODY()

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    FGuid CommandId;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    FString PayloadFingerprint;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    bool bSucceeded = false;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    EFFZSPlacementFailure FailureReason = EFFZSPlacementFailure::None;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    FText Message;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    int64 ResultWorldRevision = 0;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    FGuid FacilityInstanceId;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    bool bResultReferencesFacility = false;
};

USTRUCT(BlueprintType)

 // FF_ZoneSystem이 소유하는 월드 저장 경계, 현재 스키마는 4다.
 // WorldRevision·CurrentPhase·시설/큐·완료 영수증만 포함하며 제작 존, 보호 영역, 정책 제공자, 외부 시스템 상태는 제외한다.

struct FF_ZONESYSTEM_API FFFZSWorldSnapshot
{
    GENERATED_BODY()

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    int32 SchemaVersion = 4;
    // 현재 지원 저장 스키마. RestoreWorldSnapshot은 구 Schema 3만 명시적으로 1cm Schema 4로 변환한다.

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    int64 WorldRevision = 0;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    EFFZSChangePhase CurrentPhase = EFFZSChangePhase::InitialSetup;
    // 저장 시점 운영 단계. 복원 후 명령 허용 규칙에 즉시 사용한다.

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    TArray<FFFZSFacilityInstanceSnapshot> Facilities;

    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Save")
    TArray<FFFZSCommandReceiptSnapshot> CommandReceipts;
};

USTRUCT(BlueprintType)
// UI·분석에 필요한 시설 ID·수명주기·재배치 플래그·큐 규모·평균 대기 시간·대표 위치의 비권한 값 복사본.
struct FF_ZONESYSTEM_API FFFZSFacilityReadModel
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Read Model")
    FGuid FacilityInstanceId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Read Model")
    FName FacilityDefinitionId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Read Model")
    FName ZoneId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Read Model")
    EFFZSFacilityCategory Category = EFFZSFacilityCategory::Booth;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Read Model")
    EFFZSFacilityState State = EFFZSFacilityState::Planned;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Read Model")
    bool bNeedsRelocation = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Read Model")
    EFFZSQueueState QueueState = EFFZSQueueState::Closed;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Read Model")
    int32 WaitingCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Read Model")
    int32 ActiveServiceCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Read Model")
    float AverageCompletedWaitSeconds = 0.0f;

    // Facility Actor의 대표 월드 지점. Unreal 월드 좌표 단위(cm)를 그대로 노출한다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Read Model", meta = (Units = "cm"))
    FVector WorldLocation = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
// UI·분석에 필요한 존 그래프·출입 기준점·경계 면적·밀도 상태의 비권한 값 복사본.
struct FF_ZONESYSTEM_API FFFZSZoneReadModel
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Read Model")
    FName ZoneId = NAME_None;

    // Actor에 저작된 방향 없는 그래프 이웃 ID의 값 복사본. 존재·양방향성은 ValidateZoneGraph가 검증한다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Read Model")
    TArray<FName> NeighborZoneIds;

    // 월드 좌표로 변환하지 않은 Zone 로컬 cm 기준 출입 Transform의 값 복사본.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Read Model")
    TArray<FTransform> EntryExitAnchors;

    // 현재 BoundarySpline에 GetBoundaryAreaSquareMeters()를 적용한 실제 경계 면적(m²).
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Read Model")
    float AreaSquareMeters = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Read Model")
    int32 SoftCapacity = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Read Model")
    int32 ReportedPopulation = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Read Model")
    float DensityRatio = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Read Model")
    EFFZSDensityState DensityState = EFFZSDensityState::Normal;
};

USTRUCT(BlueprintType)
// 한 WorldRevision에서 수집해 ID 순으로 정렬한 존·시설 읽기 모델 묶음. 수정해도 런타임 상태는 바뀌지 않는다.
struct FF_ZONESYSTEM_API FFFZSReadModel
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Read Model")
    int64 WorldRevision = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Read Model")
    TArray<FFFZSZoneReadModel> Zones;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Read Model")
    TArray<FFFZSFacilityReadModel> Facilities;
};

USTRUCT(BlueprintType)
 // 외부 정책 콜백의 허용·거부 응답.
 // 거부 시 bAllowed=false와 구체 FailureReason·사용자 설명 Message를 함께 제공한다.

struct FF_ZONESYSTEM_API FFFZSPolicyDecision
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Policy")
    bool bAllowed = true;
    // true면 다음 정책 또는 적용 단계로 진행한다. false면 트랜잭션을 시작하거나 계속하지 않는다.

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Policy")
    EFFZSPlacementFailure FailureReason = EFFZSPlacementFailure::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Policy")
    FText Message;
};

USTRUCT(BlueprintType)
 // 정책이 읽는 명령 시점 컨텍스트.
 // Command·로컬 Validation·대상 FacilityActor·현재 리비전을 제공하지만, 콜백 안에서 새 권한 변경을 시작하면 거부된다.
struct FF_ZONESYSTEM_API FFFZSPolicyContext
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Policy")
    FFFZSCommand Command;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Policy")
    FFFZSPlacementValidation Validation;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Policy")
    TObjectPtr<AFFZSFacilityActor> FacilityActor = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Policy")
    int64 CurrentWorldRevision = 0;
    // 정책 검토 시작 시점의 권한 리비전. 콜백 중에는 변경할 수 없다.
};

USTRUCT(BlueprintType)
 // 성공 확정 뒤 방송하는 불변 변경 이벤트.
 // 고유 EventId와 원본 CommandId/TransactionId, 시설·이전/새 존, 확정 WorldRevision을 제공한다.
struct FF_ZONESYSTEM_API FFFZSWorldChangeEvent
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Event")
    FGuid EventId;
    // 재방송·로그 중복 판별을 위한 이벤트 고유 GUID다. CommandId와 별개다.

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Event")
    FGuid CommandId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Event")
    FGuid TransactionId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Event")
    EFFZSCommandType Type = EFFZSCommandType::Install;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Event")
    FGuid FacilityInstanceId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Event")
    FName PreviousZoneId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Event")
    FName NewZoneId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Event")
    int64 WorldRevision = 0;
};

USTRUCT()

 // 런타임 CommandId 멱등성 레코드.
 // 전체 지문과 원래 결과를 보관하며 bInFlight는 같은 명령의 동기 재진입을 fail-closed하는 내부 상태다.
struct FF_ZONESYSTEM_API FFFZSCommandReceipt
{
    GENERATED_BODY()

    UPROPERTY()
    FString PayloadFingerprint;

    UPROPERTY()
    FFFZSCommandResult Result;

    UPROPERTY()
    FGuid FacilityInstanceId;

    UPROPERTY()
    bool bInFlight = false;
    // 아직 결과가 확정되지 않은 내부 영수증 표시. 스냅샷에는 저장하지 않는다.
};

USTRUCT()
// 서로 다른 존의 같은 좌표가 충돌하지 않도록 ZoneId와 FIntPoint를 묶는 점유 맵 키.
struct FF_ZONESYSTEM_API FFFZSCellKey
{
    GENERATED_BODY()

    UPROPERTY()
    FName ZoneId = NAME_None;

    UPROPERTY()
    FIntPoint Cell = FIntPoint::ZeroValue;

// 존과 셀 좌표가 모두 같을 때만 같은 점유 키다.//
    bool operator==(const FFFZSCellKey& Other) const
    {
        return ZoneId == Other.ZoneId && Cell == Other.Cell;
    }
};

// ZoneId와 셀 좌표 해시를 결합한다. operator==와 같은 필드를 사용한다.
FORCEINLINE uint32 GetTypeHash(const FFFZSCellKey& Value)
{
    return HashCombine(GetTypeHash(Value.ZoneId), GetTypeHash(Value.Cell));
}
