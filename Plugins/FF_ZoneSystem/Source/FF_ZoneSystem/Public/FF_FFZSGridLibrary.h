// FF_FFZSGridLibrary.h

// 고정 1 cm 격자의 압축 영역 회전·점유·내장 프리셋·명령 지문 계산을 제공하는 순수 함수 모음.
// 흐름: 사양과 배치를 정규화한 뒤 footprint·clearance·queue 압축 사각형과 정확한 예약 합집합을 만든다.
//불변식: 셀 좌표는 존 로컬 격자이며 명령 지문은 멱등성 판정을 위해 의미론적 전체 페이로드를 포함.
//실패/성능: 면적 안전 예산을 초과하면 fail-closed하고 정상 비용은 개별 셀 수가 아니라 사각형 수에 비례.

#pragma once


#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FF_FFZSTypes.h"
#include "FF_FFZSGridLibrary.generated.h"

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

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Validation")
    static int32 GetMaxEnumeratedCellsPerFacility();
	// Project Settings 값을 하드 상한 1,000,000에 제한한 실제 열거 예산을 반환.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Grid")
    static int32 NormalizeQuarterTurns(int32 QuarterTurns);
	// 임의의 정수 회전값을 시계 방향 0..3 사분면 회전으로 정규화.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Grid")
    static FIntPoint RotateCell(FIntPoint Cell, int32 QuarterTurns);
	// 원점 기준 셀 좌표를 정규화된 시계 방향 90도 단위로 회전.

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
	// 시설 본체를 보통 한 개의 압축 사각형으로 반환한다. 권위 런타임 API다. 
	
    static TArray<FFFZSCellRect> GetClearanceRects(const FFFZSFacilitySpec& Spec, const FFFZSGridPlacement& Placement);
	// footprint와 queue를 제외한 clearance 링을 반환한다. 어느 하위 좌표라도 오버플로면 부분 결과 없이 빈 배열. 
	
    static TArray<FFFZSCellRect> GetQueueRects(const FFFZSFacilitySpec& Spec, const FFFZSGridPlacement& Placement);
	// 회전·원점을 적용한 queue 압축 사각형들을 반환한다. 한 Rect라도 오버플로면 전체가 빈 배열.
	
    static TArray<FFFZSCellRect> GetReservedRects(const FFFZSFacilitySpec& Spec, const FFFZSGridPlacement& Placement);
	// footprint·clearance·queue의 정확한 압축 합집합. 하위 계산 하나라도 실패하면 부분 합집합 없이 빈 배열이다. 
	
    static bool TryMaterializeCellRects(const TArray<FFFZSCellRect>& Rects, int32 MaxCells, TArray<FIntPoint>& OutCells);
	// 작은 영역만 개별 셀로 펼치는 호환/디버그 도우미다.
	// @return 합집합이 MaxCells 이하면 true, 크거나 무효면 OutCells를 비우고 false.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Grid")
    static TArray<FIntPoint> GetFootprintCells(const FFFZSFacilitySpec& Spec, const FFFZSGridPlacement& Placement);
	// 호환/디버그용 개별 본체 셀. 10,000셀을 넘으면 fail-closed로 빈 배열이다. 권위 로직은 GetFootprintRects를 사용.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Grid")
    static TArray<FIntPoint> GetClearanceCells(const FFFZSFacilitySpec& Spec, const FFFZSGridPlacement& Placement);
	// 호환/디버그용 개별 clearance 셀. 10,000셀을 넘으면 빈 배열.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Grid")
    static TArray<FIntPoint> GetQueueCells(const FFFZSFacilitySpec& Spec, const FFFZSGridPlacement& Placement);
	// 호환/디버그용 개별 queue 셀. 10,000셀을 넘으면 빈 배열이다. 

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Grid")
    static TArray<FIntPoint> GetReservedCells(const FFFZSFacilitySpec& Spec, const FFFZSGridPlacement& Placement);
	// 호환/디버그용 개별 예약 셀. 10,000셀을 넘으면 빈 배열이며 권위 입력이 아니다. 

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Presets")
    static FFFZSFacilitySpec MakeBuiltInFacilitySpec(EFFZSBuiltInFacilityPreset Preset);
	// 선택한 내장 프리셋의 값 복사본을 만든다. Accepted 항목 외 수치는 측정 전 가설로 취급.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Presets")
    static TArray<FName> GetAcceptedGameplayZoneIds();
	// 이 프로젝트의 Accepted 기본 Gameplay Zone ID 목록을 값으로 반환한다. 이식 프로젝트는 설정에서 교체할 수 있다. 

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Commands")
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
