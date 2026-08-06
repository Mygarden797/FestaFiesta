// FF_ZoneSystem.h

// FF_ZoneSystem 런타임 모듈과 공용 로그 범주를 선언한다.
// 흐름: 엔진이 모듈을 적재하면 StartupModule을 호출 / 종료 시 ShutdownModule을 호출
// 불변식: 월드별 게임 상태는 이 모듈이 아니라 UFFZSWorldSubsystem 한 곳에서 소유
// 실패/성능: 시작·종료 훅은 의도적으로 무상태이며 게임플레이 작업이나 Tick 비용을 만들지 않는다.

#pragma once

#include "Modules/ModuleManager.h"

// 플러그인 내부 진단용 로그 범주. 사용자 메시지나 최종 현지화 문자열로 사용하지 않는다.

DECLARE_LOG_CATEGORY_EXTERN(LogFFZoneSystem, Log, All);

// FF_ZoneSystem 런타임 모듈 진입점.
// 모듈은 타입 등록만 담당한다. 월드별 상태와 명령 권한은 UFFZSWorldSubsystem이 소유

class FFFZoneSystemModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    
    virtual void ShutdownModule() override;
};
