/**
 * @file FF_ZoneSystem.cpp
 * @brief 런타임 모듈과 LogFFZoneSystem 저장소를 정의한다.
 * 흐름: 엔진 모듈 수명주기 훅을 전달한다.
 * 불변식: 월드 상태나 싱글턴을 만들지 않는다.
 * 실패/성능: 훅이 비어 있어 적재 비용과 지속 작업이 없다.
 */

#include "FF_ZoneSystem.h"

#include "Modules/ModuleManager.h"

// 선언 헤더의 공용 로그 범주 저장소를 이 모듈 구현 단위에서 정확히 한 번 정의한다.
DEFINE_LOG_CATEGORY(LogFFZoneSystem);

// 전역 싱글턴이나 월드 상태를 만들지 않는다.
// 월드마다 필요한 상태는 UFFZSWorldSubsystem::Initialize가 해당 월드 수명에 맞춰 구성한다.
void FFFZoneSystemModule::StartupModule()
{
}

// 모듈 소유 자원이 없으므로 명시적 해제가 없다.
// 액터·서브시스템 정리는 각 월드의 Deinitialize/EndPlay 경계에서 수행한다.
void FFFZoneSystemModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FFFZoneSystemModule, FF_ZoneSystem)
