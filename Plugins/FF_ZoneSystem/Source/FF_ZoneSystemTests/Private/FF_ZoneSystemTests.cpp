// FF_ZoneSystemTests.cpp

// FF_ZoneSystem의 Editor 전용 자동화 테스트 모듈 진입점이다.
// 이 모듈은 테스트 클래스 등록 시점을 PostEngineInit에 제공할 뿐 Runtime 게임 기능이나
// 전역 상태를 소유하지 않는다. Startup/Shutdown을 비워 두는 이유는 Automation macro와
// UObject 등록을 엔진 수명주기에 맡기기 위해서다. Shipping 대상에는 이 모듈을 연결하지 않는다.


#include "Modules/ModuleManager.h"

class FFF_ZoneSystemTestsModule final : public IModuleInterface
{
};

IMPLEMENT_MODULE(FFF_ZoneSystemTestsModule, FF_ZoneSystemTests)
