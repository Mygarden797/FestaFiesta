// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Data/FF_UIPalette.h"

// 값의 기본치는 전부 헤더의 멤버 초기화로 들어간다. 여기 생성자를 두지 않는 이유는
// 색이 35개라 생성자에 나열하면 헤더의 CSS 변수 주석과 값이 떨어져 대조가 안 되기 때문이다.
// `UFF_UIMetricBandSettings`는 구간 배열을 조립해야 해서 생성자를 갖지만 이쪽은 그럴 게 없다.

const UFF_UIPalette* UFF_UIPalette::Get()
{
	// GetDefault는 CDO를 돌려주므로 항상 유효하다. Config 프로퍼티라
	// DefaultGame.ini에 값이 있으면 그쪽이 이미 반영된 상태다.
	return GetDefault<UFF_UIPalette>();
}
