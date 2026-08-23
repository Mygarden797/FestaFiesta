// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Atlas/FF_AtlasSettings.h"

// 기본값은 전부 헤더의 멤버 초기화로 들어간다. 생성자를 두지 않는 이유는 값마다 붙은
// "왜 이 숫자인가" 주석과 값이 떨어져 대조가 안 되기 때문이다 (`UFF_UIPalette` 와 같은 판단).

const UFF_AtlasSettings* UFF_AtlasSettings::Get()
{
	return GetDefault<UFF_AtlasSettings>();
}
