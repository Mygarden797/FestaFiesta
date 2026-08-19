// FF_BuildModeGuide.h

// 건축 세션 동안 좌하단에 표시되는 조작 안내 WBP의 네이티브 클래스 계약을 선언한다.
// 흐름: BuildModeSubsystem이 파생 클래스를 로드 -> 위젯 생성·Viewport 부착 -> 세션 종료 시 제거.
// 불변식: 레이아웃과 문구는 파생 WBP가 소유하고 네이티브 타입은 상태를 갖지 않는다.
// 실패/수명: WBP가 없으면 안내만 생략하며 건축 세션의 권위 상태와 입력은 계속 유지한다.

#pragma once

#include "Blueprint/UserWidget.h"
#include "FF_BuildModeGuide.generated.h"

/**
 * 건축 세션(BuildMode/CatalogOpen/Placing/Moving) 동안 좌하단에 고정 표시되는 조작 안내 패널입니다.
 * 텍스트, 배경, 배치는 파생 WBP가 소유하며 네이티브 클래스는 서브시스템이 생성·부착할 클래스 계약만 제공합니다.
 */
UCLASS(Blueprintable, meta = (DisplayName = "FF Build Mode Guide"))
class FESTAFIESTA_API UFF_BuildModeGuide : public UUserWidget
{
	GENERATED_BODY()
};
