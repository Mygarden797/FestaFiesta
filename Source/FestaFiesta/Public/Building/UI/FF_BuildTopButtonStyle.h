// FF_BuildTopButtonStyle.h

// 건설 카탈로그 상단 액션 버튼의 배경을 모든 입력 상태에서 투명하게 유지하는 스타일을 선언한다.
// 흐름: 네이티브 생성자에서 상태 브러시·여백·크기 기본값을 한 번 구성한다.
// 불변식: Normal·Selected·Disabled의 모든 배경 브러시는 NoDraw 상태다.
// 실패/수명: 클릭 영역은 WBP Root_Size가 보장하며 스타일 객체는 런타임 자원을 소유하지 않는다.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "FF_BuildTopButtonStyle.generated.h"

/**
 * 건설 카탈로그 상단 액션 전용 투명 CommonButton 스타일이다.
 *
 * 상단 버튼은 배경 면 없이 월드/카탈로그 배경 위에 아이콘과 라벨만 보여야 한다.
 * 모든 상태 브러시를 NoDraw로 고정해 CommonButton 내부 SButton의 기본 회색 면이
 * 다시 노출되지 않도록 하고, 호버·선택·포커스 피드백은 버튼 WBP의 콘텐츠가 담당한다.
 */
UCLASS(meta = (DisplayName = "FF Build Top Button Style"))
class FESTAFIESTA_API UFF_BuildTopButtonStyle : public UCommonButtonStyle
{
	GENERATED_BODY()

public:
	UFF_BuildTopButtonStyle();
};
