// 역할: 에디터에서만 제작 명령과 검증 코드를 불러오는 모듈 진입점이다.
#include "Modules/ModuleManager.h"

// 제작 명령은 각 파일의 FAutoConsoleCommand가 등록하고 모듈 해제 시 정리한다.
IMPLEMENT_MODULE(FDefaultModuleImpl, FestaFiestaEditor);
