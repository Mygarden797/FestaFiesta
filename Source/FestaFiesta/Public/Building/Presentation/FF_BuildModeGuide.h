// FF_BuildModeGuide.h

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
