// FF_ZoneAuthoringModule.cpp
// 역할: 런타임 모듈 등록. 시작/종료 시 별도 작업이 없으므로 기본 모듈 구현을 사용한다.
#include "FF_ZoneAuthoringModule.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogFFZoneAuthoring);

IMPLEMENT_MODULE(FDefaultModuleImpl, FF_ZoneAuthoring)
