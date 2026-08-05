/**
 * @file FF_FFZSDeveloperSettings.cpp
 * @brief 설정 패널 위치와 초기 프로젝트 존 ID 기본값을 구성한다.
 * 흐름: 설정 CDO 생성 시 내장 허용 존 목록을 기본값으로 복사하고 이후 Game Config가 오버라이드한다.
 * 불변식: 격자 크기 코드 계약은 설정 생성자가 변경하지 않는다.
 * 실패/성능: 생성 시 작은 프리셋 배열을 한 번 복사할 뿐 Tick 비용이 없다.
 */

#include "FF_FFZSDeveloperSettings.h"

#include "FF_FFZSGridLibrary.h"

// Project Settings > Game > FF_ZoneSystem 경로를 고정하고 Accepted Zone ID를 새 프로젝트 기본값으로 채운다.
// Config=Game 역직렬화가 이후 담당자가 저장한 값을 덮어쓰므로 이 코드는 사용자 설정을 런타임마다 강제하지 않는다.
UFFZSDeveloperSettings::UFFZSDeveloperSettings()
{
    CategoryName = TEXT("Game");
    SectionName = TEXT("FF_ZoneSystem");
    // 값 복사이므로 담당자가 설정 배열을 수정해도 GridLibrary의 내장 기준 목록 자체는 바뀌지 않는다.
    ProjectZoneIds = UFFZSGridLibrary::GetAcceptedGameplayZoneIds();
}
