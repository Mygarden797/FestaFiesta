// FF_FFZSDeveloperSettings.h

// 기획 담당자가 Project Settings에서 조정할 전역 프로토타입 수치와 안전 한계를 정의.
// 흐름: CDO 기본값 위에 Config=Game 값을 읽고, 런타임 검증기와 시뮬레이터가 이를 조회.
// 불변식: 거리 단위는 cm이며 배치 셀은 코드 계약상 1 cm(= 1 Unreal Unit)로 고정.
// 실패/성능: 검사 비활성화는 책임 경계를 바꾸며, 열거 예산은 잘못된 데이터가 게임 스레드를 장시간 점유하지 못하게 한다.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FF_FFZSTypes.h"
#include "FF_FFZSDeveloperSettings.generated.h"

// 화면 격자와 실제 설치 검증이 함께 쓰는 지면 규칙의 읽기 전용 복사본이다.
// Config를 직접 읽지 않고 ResolveGroundValidationSettings()로 만들면 비정상 숫자도 방어한다.
struct FF_ZONESYSTEM_API FFFZSGroundValidationSettings
{
    double SampleSpacingCm;
    double TraceAboveCm;
    double TraceBelowCm;
    double MaxSlopeDegrees;
    double MaxHeightVarianceCm;
    // 타일은 지면 최고점 위에 수평으로 놓는다. 길과 일반 시설의 기존 높이 규칙은 유지한다.
    bool bUseHighestSupport = false;
};

// Project Settings > Game > FF_ZoneSystem에서 담당자가 조정하는 전역 규칙.
// 값 변경은 새 검증·시뮬레이션부터 적용되므로 운영 중 변경 시 저장 데이터와 플레이 테스트 기준도 함께 갱신.
 
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "FF Zone System"))
class FF_ZONESYSTEM_API UFFZSDeveloperSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UFFZSDeveloperSettings();
	
	// 설정 섹션 이름과 프로젝트 기본 ZoneId 목록을 구성. 

    //고정 격자 계약. 1 배치 셀은 1 cm이고 1 Unreal Unit도 1 cm다.
	// @warning 단순 설정값이 아니므로 변경하려면 좌표·저장·콘텐츠 마이그레이션이 필요.
     
    static constexpr double DefaultPlacementCellSizeCm = 1.0;
	
    static constexpr int32 DefaultMaxEnumeratedCellsPerFacility = 20000000;
	// 사용자 설정이 없을 때 적용하는 시설당 보수적 셀 열거 작업 예산.
	
    static constexpr int32 MaxMaterializedDebugCells = 10000;
	// 대형 1 cm 영역을 디버그용 개별 셀 배열로 펼칠 때 허용하는 고정 상한.

    // 건축 모드 범위 표시와 배치 Preview가 한 그룹에서 개별로 그리는 Rect 수 상한이다.
    // 이 수를 넘는 조각난 그룹은 합집합 외접 박스 하나로 축약해 표시 비용을 제한한다.
    // [하드코딩 제거] 예전에는 FacilityActor와 PlacementPreviewActor에 같은 1024가 따로 있어
    // 한쪽만 조정하면 두 화면의 표시 규칙이 서로 달라졌다.
    static constexpr int32 MaxDisplayedRangeRectsPerGroup = 1024;

    // Per-Definition collection guards. These are safety contracts, not per-facility tuning data.
    static constexpr int32 MaxQueueReservedRects = 64;
    static constexpr int32 MaxLegacyQueueReservedCells = 256;
    static constexpr int32 MaxQueueSplinePoints = 64;
    static constexpr int32 MaxFacilityAnchors = 64;
    static constexpr int32 MaxAllowedZoneIds = 64;
    static constexpr int32 MaxQueueWaitingAgents = 512;
    static constexpr int32 MaxFacilityServiceCapacity = 32;
    static constexpr int32 MaxQueueRuntimeEntries = 1024;
    static constexpr int32 MaxQueueRepathAttempts = 32;
	
    static constexpr float DefaultGroundSampleSpacingCm = 100.0f;

    static constexpr float DefaultGroundTraceDistanceCm = 1000.0f;
    // Game Config가 없거나 값이 비정상일 때 사용하는 지면 허용치. 두 조건을 모두 만족해야 한다.
    static constexpr float DefaultMaxGroundSlopeDegrees = 15.0f;
    static constexpr float DefaultMaxGroundHeightVarianceCm = 80.0f;
    static constexpr float DefaultMaxTileGroundSlopeDegrees = 89.0f;
    static constexpr float DefaultMaxTileGroundHeightVarianceCm = DefaultGroundTraceDistanceCm * 2.0f;

    // 설정창의 제한을 우회한 ini/코드 입력도 같은 기준으로 보정한다.
    // 정상 설정은 그대로 전달하며 NaN/무한대는 선언된 기본값으로 되돌린다.
    FFFZSGroundValidationSettings ResolveGroundValidationSettings() const;

    // 분류별 지면 정책의 단일 진입점. Tile만 기반 바닥 정책을 적용하고 Road는 일반 정책을 유지한다.
    FFFZSGroundValidationSettings ResolveGroundValidationSettings(EFFZSFacilityCategory Category) const;
	// 지면 검증 밀도는 1 cm 점유 해상도와 분리한다. 기본 100 cm마다 대표 표본 하나를 검사.

    // 시설별 저장 필드를 추가하지 않고 Footprint에서 선택 Query Box를 파생하는 전역 정책이다.
    // UBoxComponent는 반치수를 사용하므로 이 값은 전체 폭 증가량이 아니라 XY 각 변의 여백이다.
    static constexpr float DefaultFacilitySelectionPaddingCm = 75.0f;
    static constexpr float MaxFacilitySelectionPaddingCm = 500.0f;

    // 시설 하나를 검증할 때 허용하는 보수적 열거 작업 예산.
    // 회전 전 footprint 면적과 정규화한 Queue 압축 영역 면적을 합산해
    // 비정상적으로 큰 저작 데이터를 조기에 거부한다. 권위 런타임은 영역을 개별 셀로 펼치지 않는다.
    // 원본 크기의 대형 무대도 허용하되 구 호환·디버그 배열은 별도 10,000셀 상한을 유지한다.
    // 이 값은 1..20,000,000으로 제한한다.

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Performance", meta = (ClampMin = "1", ClampMax = "20000000"))
    int32 MaxEnumeratedCellsPerFacility = DefaultMaxEnumeratedCellsPerFacility;

    // 시설 footprint 지면을 검사하는 물리 표본 간격(cm)이다.
    // 1 cm 셀을 모두 LineTrace하지 않도록 점유 해상도와 분리한다. 각 축의 표본 간격이 이 값보다
    // 커지지 않게 균등 배치하므로 작은 시설도 최소 한 번 검사.
     
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Performance", meta = (ClampMin = "1.0", ClampMax = "1000.0", Units = "cm"))
    float GroundSampleSpacingCm = DefaultGroundSampleSpacingCm;
	
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Performance", meta = (ClampMin = "0.0", ClampMax = "1.0", Units = "s"))
    float QueueSimulationTickIntervalSeconds = 0.1f;
	// Queue Component 한 개의 시뮬레이션 간격입니다. 0이면 매 프레임 실행합니다. 

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Selection", meta = (DisplayName = "Facility Selection Padding (cm)", ToolTip = "시설 Footprint의 XY 각 변에 더할 선택 전용 Query 여백입니다. FacilityDefinition과 SaveGame에는 저장하지 않으며 모든 시설에 같은 런타임 정책을 적용합니다.", ClampMin = "0.0", ClampMax = "500.0", Units = "cm", ConfigRestartRequired = true))
    float FacilitySelectionPaddingCm = DefaultFacilitySelectionPaddingCm;
	// 선택 편의만 넓히며 물리, Navigation, Overlap 또는 권위 배치 점유에는 영향을 주지 않는다.
	
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.0", ClampMax = "89.0", Units = "deg"))
    float MaxGroundSlopeDegrees = DefaultMaxGroundSlopeDegrees;
	// 모든 지면 표본의 법선으로 허용할 최대 경사각이다. 

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placement|Tile", meta = (DisplayName = "Tile Maximum Ground Slope (Degrees)", ToolTip = "타일 아래 지면에서 허용하는 최대 경사각입니다. 타일은 지면을 따라 기울지 않고 확인된 최고점 위에 수평으로 놓입니다. 길과 일반 건축물에는 일반 지면 설정을 적용합니다.", ClampMin = "0.0", ClampMax = "89.0", Units = "deg"))
    float MaxTileGroundSlopeDegrees = DefaultMaxTileGroundSlopeDegrees;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placement|Tile", meta = (DisplayName = "Tile Maximum Ground Height Difference (cm)", ToolTip = "타일 아래에서 확인된 최고점과 최저점의 허용 높이 차입니다. 기본값 2000cm는 기본 지면 탐색 범위 전체에 해당합니다. 이 값이 커도 건축 가능 구역, 지면 존재, 다른 시설과의 겹침 검사는 유지됩니다.", ClampMin = "0.0", Units = "cm"))
    float MaxTileGroundHeightVarianceCm = DefaultMaxTileGroundHeightVarianceCm;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placement|Controls", meta = (DisplayName = "Rotation Snap (Degrees)", ToolTip = "건축 모드에서 Q/E를 한 번 눌렀을 때 회전하는 각도입니다.", ClampMin = "0.01", ClampMax = "180.0", Units = "deg"))
    float PlacementRotationSnapDegrees = 45.0f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placement|Controls", meta = (DisplayName = "Free Rotation Speed (Degrees/Second)", ToolTip = "건축 모드에서 Shift+Q/E를 누르고 있을 때의 자유 회전 속도입니다.", ClampMin = "1.0", ClampMax = "720.0", Units = "deg/s"))
    float FreePlacementRotationSpeedDegreesPerSecond = 90.0f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placement|Controls", meta = (DisplayName = "Default Position Snap (cm)", ToolTip = "평소 배치할 때 사용하는 이해하기 쉬운 기본 간격입니다. 권위 저장 격자는 계속 1cm이며 이 값은 마우스 조작만 양자화합니다.", ClampMin = "1", ClampMax = "100", Units = "cm"))
    int32 PlacementPositionSnapCm = 100;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placement|Controls", meta = (DisplayName = "Precision Position Snap (cm)", ToolTip = "Ctrl을 누른 동안 사용하는 미세 조정 간격입니다.", ClampMin = "1", ClampMax = "100", Units = "cm"))
    int32 PrecisionPlacementPositionSnapCm = 25;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placement|Controls", meta = (DisplayName = "Free Position Snap (cm)", ToolTip = "Alt를 누른 동안 사용하는 자유 배치 간격입니다. 1cm는 현재 권위 좌표 계약에서 가능한 가장 자유로운 값입니다.", ClampMin = "1", ClampMax = "100", Units = "cm"))
    int32 FreePlacementPositionSnapCm = 1;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placement|Grid", meta = (DisplayName = "Local Grid Padding (cm)", ToolTip = "배치 대상의 footprint 바깥으로 더 보여 줄 로컬 격자 여백입니다.", ClampMin = "0.0", ClampMax = "2000.0", Units = "cm"))
    float PlacementGridPaddingCm = 600.0f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placement|Grid", meta = (DisplayName = "Local Grid Maximum Radius (cm)", ToolTip = "로컬 격자 여백을 제한하는 소프트 최대 반경입니다. 대형 시설의 footprint 자체는 이 값보다 커도 잘리지 않습니다.", ClampMin = "100.0", ClampMax = "5000.0", Units = "cm"))
    float PlacementGridMaxRadiusCm = 3600.0f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placement|Grid", meta = (DisplayName = "Major Grid Interval (cm)", ToolTip = "기본선보다 굵게 표시할 기준선 간격입니다.", ClampMin = "100", ClampMax = "2000", Units = "cm"))
    int32 PlacementGridMajorIntervalCm = 500;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placement|Grid", meta = (DisplayName = "Grid Surface Lift (cm)", ToolTip = "지면과 격자가 겹쳐 깜빡이는 현상을 막기 위해 선을 위로 띄우는 거리입니다.", ClampMin = "0.5", ClampMax = "20.0", Units = "cm"))
    float PlacementGridSurfaceLiftCm = 3.0f;
	
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.0", Units = "cm"))
    float MaxGroundHeightVarianceCm = DefaultMaxGroundHeightVarianceCm;
	// footprint 지면 표본 중 최고점과 최저점의 허용 높이 차이다. 

  
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "1.0", Units = "cm"))
    float GroundTraceAboveCm = DefaultGroundTraceDistanceCm;
	// 예상 배치 높이 위에서 지면 추적을 시작할 거리다. 
	
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "1.0", Units = "cm"))
    float GroundTraceBelowCm = DefaultGroundTraceDistanceCm;
	// 예상 배치 높이 아래까지 지면을 찾을 거리다. 
	
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placement")
    bool bEnableGroundChecks = true;
	// 끄면 경사·높이 편차·지면 누락 검사를 생략한다. 프로토타입 외에는 담당자 승인이 필요.
	
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placement")
    bool bRequireEveryGroundSample = true;
	// 켜면 footprint의 모든 표본이 지면에 닿아야 한다. 끄면 일부 표본 누락을 허용.
	
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placement")
    bool bEnableWorldCollisionChecks = true;
	// 켜면 예약 공간과 기존 월드 충돌체의 중첩을 검사한다. 격자 점유 검사와 별도다. 
	
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Navigation")
    bool bEnableNavigationProjectionChecks = true;
	// 켜면 필요한 지점을 내비게이션 데이터에 투영할 수 있는지 검사.
	
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Navigation")
    bool bFailWhenNavigationSystemIsUnavailable = false;
	// 내비게이션 시스템 자체가 없을 때 거부할지 정한다. 운영 빌드는 true를 권장.
	
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Navigation", meta = (ClampMin = "0.0", Units = "cm"))
    float NavigationProjectionExtentCm = 30.0f;
	// 배치 지점을 NavMesh에 투영할 때 사용하는 수평·수직 검색 범위다.

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Navigation")
    bool bEnableQueueLaneNavCost = true;
	// 켜면 예약된 Queue 레인을 고비용 NavArea로 표시해, 지나가는 NPC가 줄을 관통하지 않고
	// 우회하도록 만든다. 통행 불가가 아니라 비용이므로 배치 검증(투영·경로 연결성)에는 영향이 없다.
	// 끄면 레인이 일반 보행로로 돌아간다 — 효과를 A/B로 비교할 때 이 스위치만 내리면 된다.

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Navigation", meta = (ClampMin = "0.0", Units = "cm"))
    float QueueLaneNavCostPaddingCm = 100.0f;
	// 고비용 영역을 예약 레인보다 이만큼 넓게 잡는다. 경로추종은 웨이포인트를 일정 반경 앞에서
	// 넘기며 모서리를 자르므로, 레인에 딱 맞추면 그 컷이 레인 안으로 들어온다. 0이면 딱 맞춘다.

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Zone Authoring")
    bool bRestrictZoneIdsToProjectPresets = true;
	// 켜면 ZoneId를 아래 프로젝트 목록에 한정합니다. 이식 프로젝트에서는 목록을 교체하거나 끌 수 있습니다. 
	
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Zone Authoring", meta = (EditCondition = "bRestrictZoneIdsToProjectPresets"))
    TArray<FName> ProjectZoneIds;
	// 안정적인 프로젝트 Gameplay Zone ID 목록입니다. 표시 이름과 별개이며 저장 후 임의 변경하면 안 됩니다. 
	
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Integration")
    bool bRequireProductionPolicyDomains = false;
	// 켜면 Custom을 제외한 6개 기본 정책 도메인에 정확히 하나의 어댑터가 없을 때 준비 검증을 실패시킨다. 
	
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Zone Membership", meta = (ClampMin = "0.0", Units = "cm"))
    float ZoneMembershipHysteresisCm = 50.0f;
	// 경계 부근에서 이전 존 소속을 유지하는 구성 가능한 여유 거리다. 0이면 히스테리시스를 끈다. 
	
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Protected Routes", meta = (ClampMin = "0.0", Units = "cm"))
    float GeneralRouteWidthCm = 300.0f;
	// 일반 동선 ProtectedArea가 폭 0일 때 상속하는 전체 확보 폭이다. 

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Protected Routes", meta = (ClampMin = "0.0", Units = "cm"))
    float EmergencyServiceRouteWidthCm = 400.0f;
	// 긴급·서비스 동선 ProtectedArea가 폭 0일 때 상속하는 전체 확보 폭이다. 
	
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Protected Routes", meta = (ClampMin = "0.0", Units = "cm"))
    float AccessibilityRouteWidthCm = 150.0f;
	// 접근성 동선 ProtectedArea가 폭 0일 때 상속하는 전체 확보 폭이다. 
	
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Density", meta = (ClampMin = "0.0"))
    float SquareMetersPerPersonForSoftCapacity = 4.0f;
	// 명시적 SoftCapacity가 없을 때 존 사용 가능 면적을 나눌 1인당 면적이다. 
	
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Density", meta = (ClampMin = "0.0"))
    float DensityCautionRatio = 0.8f;
	// Caution 진입 비율. Caution ≤ Warning ≤ Critical 순서를 유지한다. 
	
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Density", meta = (ClampMin = "0.0"))
    float DensityWarningRatio = 1.0f;
	// Warning 후보가 되는 인구/SoftCapacity 비율이다. 

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Density", meta = (ClampMin = "0.0"))
    float DensityCriticalRatio = 1.25f;
	// Critical 후보가 되는 인구/SoftCapacity 비율이다. 
	
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Density", meta = (ClampMin = "0.0", Units = "s"))
    float DensityWarningHoldSeconds = 5.0f;
	// Warning 비율을 연속 유지해야 실제 Warning으로 승격되는 시간이다. 
	
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Density", meta = (ClampMin = "0.0", Units = "s"))
    float DensityCriticalHoldSeconds = 3.0f;
	// Critical 비율을 연속 유지해야 실제 Critical로 승격되는 시간이다. 
	
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Workflow")
    EFFZSChangePhase InitialPhase = EFFZSChangePhase::InitialSetup;
	// 새 월드 서브시스템이 시작할 운영 단계. 단계마다 허용 명령이 다르다. 

private:
    UPROPERTY(VisibleAnywhere, Category = "Units", meta = (Units = "cm", DisplayName = "Placement Cell Size Cm (Fixed)", AllowPrivateAccess = "true"))
    float PlacementCellSizeCm = DefaultPlacementCellSizeCm;
	// Project Settings에 보여 주는 읽기 전용 값. 런타임 계산은 항상 DefaultPlacementCellSizeCm를 사용.
};
