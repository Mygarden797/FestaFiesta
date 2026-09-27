// FF_FFZSDeveloperSettings.cpp

// 설정 패널 위치와 초기 프로젝트 존 ID 기본값을 구성한다.
// 흐름: 설정 CDO 생성 시 내장 허용 존 목록을 기본값으로 복사하고 이후 Game Config가 오버라이드한다.
// 불변식: 격자 크기 코드 계약은 설정 생성자가 변경하지 않는다.
// 실패/성능: 생성 시 작은 프리셋 배열을 한 번 복사할 뿐 Tick 비용이 없다.
 

#include "FF_FFZSDeveloperSettings.h"

#include "FF_FFZSGridLibrary.h"

namespace
{
    // 유효하지 않은 외부 설정이 물리 추적 좌표나 표본 개수로 전달되지 않게 한다.
    double FiniteOrDefault(const double Value, const double Default)
    {
        return FMath::IsFinite(Value) ? Value : Default;
    }
}

FFFZSGroundValidationSettings UFFZSDeveloperSettings::ResolveGroundValidationSettings() const
{
    return {
        FMath::Clamp(FiniteOrDefault(GroundSampleSpacingCm, DefaultGroundSampleSpacingCm), 1.0, 1000.0),
        FMath::Max(1.0, FiniteOrDefault(GroundTraceAboveCm, DefaultGroundTraceDistanceCm)),
        FMath::Max(1.0, FiniteOrDefault(GroundTraceBelowCm, DefaultGroundTraceDistanceCm)),
        FMath::Clamp(FiniteOrDefault(MaxGroundSlopeDegrees, DefaultMaxGroundSlopeDegrees), 0.0, 89.0),
        FMath::Max(0.0, FiniteOrDefault(MaxGroundHeightVarianceCm, DefaultMaxGroundHeightVarianceCm))
    };
}

// Project Settings > Game > FF_ZoneSystem 경로를 고정하고 Accepted Zone ID를 새 프로젝트 기본값으로 채운다.
// Config=Game 역직렬화가 이후 담당자가 저장한 값을 덮어쓰므로 이 코드는 사용자 설정을 런타임마다 강제하지 않는다.

UFFZSDeveloperSettings::UFFZSDeveloperSettings()
{
    CategoryName = TEXT("Game");
    SectionName = TEXT("FF_ZoneSystem");
    // 값 복사이므로 담당자가 설정 배열을 수정해도 GridLibrary의 내장 시드 목록 자체는 바뀌지 않는다.
    // [하드코딩 제거] 시드 전용 API를 호출한다. GetAcceptedGameplayZoneIds()는 이제 이 설정을
    // 되읽으므로, CDO 생성 중에 그 함수를 부르면 초기화되지 않은 자기 자신을 조회하게 된다.
    ProjectZoneIds = UFFZSGridLibrary::GetDefaultProjectZoneIds();
}
