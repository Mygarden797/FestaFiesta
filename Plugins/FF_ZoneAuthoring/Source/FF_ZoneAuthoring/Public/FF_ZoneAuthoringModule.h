// FF_ZoneAuthoringModule.h
// 역할: FF_ZoneAuthoring 런타임 모듈의 로그 카테고리를 공개한다.
// 이 플러그인은 FF_ZoneSystem 위에 "디자이너용 Zone 저작 계층"을 얹는다. FF_ZoneSystem 소스는 수정하지 않는다.
#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

FF_ZONEAUTHORING_API DECLARE_LOG_CATEGORY_EXTERN(LogFFZoneAuthoring, Log, All);

#if WITH_EDITOR
class UWorld;

// The runtime authoring layer requests editor UI without depending on its editor module.
DECLARE_DELEGATE_TwoParams(FFFZAOpenSelectedWeldRequest, UWorld*, float);
FF_ZONEAUTHORING_API FFFZAOpenSelectedWeldRequest& FFZAGetSelectedWeldRequest();
#endif
