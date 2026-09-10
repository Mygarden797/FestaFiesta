// FF_FFZSGridLibrary.h

// 고정 1 cm 격자의 압축 영역 회전·점유·명령 지문 계산을 제공하는 순수 함수 모음.
// 흐름: 사양과 배치를 정규화한 뒤 footprint·clearance·queue 압축 사각형과 정확한 예약 합집합을 만든다.
//불변식: 셀 좌표는 존 로컬 격자이며 명령 지문은 멱등성 판정을 위해 의미론적 전체 페이로드를 포함.
//실패/성능: 면적 안전 예산을 초과하면 fail-closed하고 정상 비용은 개별 셀 수가 아니라 사각형 수에 비례.

#pragma once


#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FF_FFZSTypes.h"
#include "FF_FFZSGridLibrary.generated.h"

class UFFZSFacilityDefinition;

// 상태를 소유하지 않는 Blueprint 함수 라이브러리.
// 모든 계산은 입력값만 사용하며 1 cm 고정 격자와 결정적 사각형 정규화 규칙을 따른다.
 
UCLASS()
class FF_ZONESYSTEM_API UFFZSGridLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // 모든 격자 열거기에 적용되는 조기 실패 안전장치다. 담당자에게 노출되는 유효 설정값은
    // 잘못된 데이터가 오버플로를 일으키거나 Game Thread를 멈추지 않도록 최대 100만 Cell로 강제 제한.
     
    // footprint 확장 면적 + queue 압축 영역의 이론 셀 수가 안전 상한 이내인지 빠르게 확인.
    // @return 안전 예산 이내일 때만 true. 최종 고유 예약 셀 수를 계산하는 함수가 아니다.
     
    static bool CanEnumerateFacilityCells(const FFFZSFacilitySpec& Spec);

    // Cheap cardinality preflight used before validation, fingerprinting, or geometry work.
    static bool HasSafeFacilitySpecCardinality(const FFFZSFacilitySpec& Spec);

    // Converts centre-local audience bounds to footprint-corner cm coordinates, rounded outward.
    static bool TryGetAudienceReservedRect(const FFFZSFacilitySpec& Spec, FFFZSCellRect& OutRect);

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Validation")
    static int32 GetMaxEnumeratedCellsPerFacility();
	// Project Settings 값을 하드 상한 10,000,000에 제한한 실제 열거 예산을 반환.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Grid")
    static int32 NormalizeQuarterTurns(int32 QuarterTurns);
	// 임의의 정수 회전값을 Unreal 양의 Yaw 기준 0..3 사분면 회전으로 정규화.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Grid")
    static int32 NormalizeYawCentidegrees(int32 YawCentidegrees);

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Grid")
    static int32 DegreesToYawCentidegrees(float YawDegrees);

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Grid")
    static float YawCentidegreesToDegrees(int32 YawCentidegrees);

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Grid")
    static int32 GetEffectiveYawCentidegrees(const FFFZSGridPlacement& Placement);

    static void NormalizePlacement(FFFZSGridPlacement& InOutPlacement);

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Grid")
    static bool TryRotateCell(FIntPoint Cell, int32 QuarterTurns, FIntPoint& OutRotatedCell);

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Grid",
        meta = (DeprecatedFunction, DeprecationMessage = "Use TryRotateCell so integer-boundary failure is explicit."))
    static FIntPoint RotateCell(FIntPoint Cell, int32 QuarterTurns);
	// 원점 기준 셀 좌표를 정규화된 양의 Yaw 90도 단위로 회전.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Grid")
    static FFFZSCellRect RotateRect(const FFFZSCellRect& Rect, int32 QuarterTurns);
	// 반열림 사각형 전체를 정수 오차 없이 회전한다. 오버플로면 무효 사각형을 반환.
	
    static TArray<FFFZSCellRect> CanonicalizeCellRects(const TArray<FFFZSCellRect>& Rects);
	// 겹치거나 맞닿는 입력을 서로 겹치지 않는 결정적 사각형 합집합으로 정규화.
	
    static bool DoCellRectSetsOverlap(const TArray<FFFZSCellRect>& A, const TArray<FFFZSCellRect>& B);
	// A와 B의 정확한 면적 교집합 여부를 압축 상태에서 검사한다. 
	
    static bool IsCellInRects(FIntPoint Cell, const TArray<FFFZSCellRect>& Rects);
	// 한 셀이 압축 영역 합집합 안에 있는지 검사한다. 
	
    static int64 GetCellRectArea(const TArray<FFFZSCellRect>& Rects);
	// 중복을 제거한 압축 영역 합집합의 셀 수를 64비트로 반환한다. 
	
    static TArray<FFFZSCellRect> GetFootprintRects(const FFFZSFacilitySpec& Spec, const FFFZSGridPlacement& Placement);
	// 호환/디버그용 보수적 AABB다. 임의 Yaw 권위 판정에는 BuildPlacementOrientedRegions를 사용한다.
	
    static TArray<FFFZSCellRect> GetClearanceRects(const FFFZSFacilitySpec& Spec, const FFFZSGridPlacement& Placement);
	// 호환/디버그용 clearance 보수적 AABB 합집합. 어느 하위 좌표라도 오버플로면 빈 배열.
	
    static TArray<FFFZSCellRect> GetQueueRects(const FFFZSFacilitySpec& Spec, const FFFZSGridPlacement& Placement);
	// 호환/디버그용 queue 보수적 AABB 합집합. 한 Rect라도 오버플로면 전체가 빈 배열.
	
    static TArray<FFFZSCellRect> GetReservedRects(const FFFZSFacilitySpec& Spec, const FFFZSGridPlacement& Placement);
	// 호환/디버그용 전체 보수적 AABB 합집합. OBB 충돌의 좁은 단계 입력으로 사용하면 안 된다.

    static void BuildPlacementOrientedRegions(
        const FFFZSFacilitySpec& Spec,
        const FFFZSGridPlacement& Placement,
        TArray<FFFZSOrientedRect>& OutFootprintRects,
        TArray<FFFZSOrientedRect>& OutClearanceRects,
        TArray<FFFZSOrientedRect>& OutQueueRects,
        TArray<FFFZSOrientedRect>& OutReservedRects);
	// 자유 Yaw를 보존한 실제 권위 영역 네 그룹을 한 번에 만든다. 좌표 오버플로는 전체 fail-closed다.

    static bool DoOrientedRectSetsOverlap(
        const TArray<FFFZSOrientedRect>& A,
        const TArray<FFFZSOrientedRect>& B);
	// SAT 좁은 단계로 양의 면적 교집합만 검사한다. 경계 접촉은 겹침이 아니다.

    // [최적화] 네 영역을 한 번의 계산으로 함께 만든다.
    // 개별 Get*Rects를 네 번 부르면 예산 검사(CanEnumerateFacilityCells)와 queue 정규화가
    // 호출마다 중복 실행된다(Reserved 하나만 해도 내부에서 footprint·queue를 다시 만든다).
    // 배치 검증처럼 네 영역이 모두 필요한 경로는 이 API를 써서 같은 결과를 1회 계산으로 얻는다.
    // 출력은 임의 Yaw OBB를 감싸는 보수적 AABB이며 권위 충돌에는 사용하지 않는다.
    static void BuildPlacementRegions(
        const FFFZSFacilitySpec& Spec,
        const FFFZSGridPlacement& Placement,
        TArray<FFFZSCellRect>& OutFootprintRects,
        TArray<FFFZSCellRect>& OutClearanceRects,
        TArray<FFFZSCellRect>& OutQueueRects,
        TArray<FFFZSCellRect>& OutReservedRects);
	
    static bool TryMaterializeCellRects(const TArray<FFFZSCellRect>& Rects, int32 MaxCells, TArray<FIntPoint>& OutCells);
	// 작은 영역만 개별 셀로 펼치는 호환/디버그 도우미다.
	// @return 합집합이 MaxCells 이하면 true, 크거나 무효면 OutCells를 비우고 false.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Grid")
    static TArray<FIntPoint> GetFootprintCells(const FFFZSFacilitySpec& Spec, const FFFZSGridPlacement& Placement);
    // 호환/디버그용 보수적 AABB 개별 셀. 10,000셀을 넘으면 빈 배열이며 권위 입력이 아니다.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Grid")
    static TArray<FIntPoint> GetClearanceCells(const FFFZSFacilitySpec& Spec, const FFFZSGridPlacement& Placement);
	// 호환/디버그용 개별 clearance 셀. 10,000셀을 넘으면 빈 배열.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Grid")
    static TArray<FIntPoint> GetQueueCells(const FFFZSFacilitySpec& Spec, const FFFZSGridPlacement& Placement);
	// 호환/디버그용 개별 queue 셀. 10,000셀을 넘으면 빈 배열이다. 

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Grid")
    static TArray<FIntPoint> GetReservedCells(const FFFZSFacilitySpec& Spec, const FFFZSGridPlacement& Placement);
	// 호환/디버그용 개별 예약 셀. 10,000셀을 넘으면 빈 배열이며 권위 입력이 아니다. 

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Configuration")
    static TArray<FName> GetAcceptedGameplayZoneIds();
	// [하드코딩 제거] 현재 허용되는 Gameplay Zone ID를 Project Settings > FF Zone System >
	// Project Zone Ids에서 읽어 반환한다. 이전에는 이 함수가 Festa 전용 ID 12개를 코드에 직접
	// 담고 있어서, 설정에 추가한 ID(예: 테스트 Zone)를 Preset 적용이 거부하는 불일치가 있었다.
	// 설정이 비어 있을 때만 GetDefaultProjectZoneIds()의 시드 목록으로 되돌아간다.

    static const TArray<FName>& GetDefaultProjectZoneIds();
	// 새 프로젝트/이식 프로젝트가 Project Zone Ids를 채우기 전에 쓰는 시드 목록이다.
	// 설정 CDO 생성자만 이 값을 복사하며(설정 로드 중 재귀 조회를 피한다), 런타임 판정의
	// 권위 목록은 언제나 UFFZSDeveloperSettings::ProjectZoneIds다.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Configuration")
    static FFFZSCommand MakeInstallCommandFromDefinition(
        const UFFZSFacilityDefinition* Definition,
        FName ZoneId,
        FFFZSGridPlacement Placement,
        int64 ExpectedWorldRevision = -1);

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Development",
        meta = (DevelopmentOnly, DeprecatedFunction,
            DeprecationMessage = "Use MakeInstallCommandFromDefinition. Runtime accepts only the exact registered Definition Spec."))
    static FFFZSCommand MakeInstallCommand(const FFFZSFacilitySpec& Spec, FName ZoneId, FFFZSGridPlacement Placement, int64 ExpectedWorldRevision = -1);
	// 새 CommandId와 FacilityInstanceId를 만들고 회전을 정규화한 Install 명령을 반환한다. -1 리비전은 최신 상태를 허용.
	
    static FString BuildCommandFingerprint(const FFFZSCommand& Command);
	// CommandId 멱등성 판정에 쓰는 전체 의미론적 페이로드를 안정적인 문자열로 직렬화.
	
    static uint32 HashCommandPayload(const FFFZSCommand& Command);
	// 빠른 진단용 32비트 CRC를 반환한다. 내구성 있는 동일성 판정에는 BuildCommandFingerprint를 사용.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Validation")
    static FText PlacementFailureToText(EFFZSPlacementFailure Failure);
	// 실패 코드를 기본 사용자 설명으로 변환한다. 최종 제품 문구와 현지화는 UI 담당 계층에서 다듬는다. 
};
