// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Core/FF_UIFlowSettings.h"

const UFF_UIFlowSettings* UFF_UIFlowSettings::Get()
{
	// 기본값을 생성자에 넣지 않는다. 클래스·레벨·카탈로그 참조는 전부 에디터에서 지정하는 값이라
	// 코드에 경로를 박으면 에셋 이름을 바꿀 때 조용히 끊긴다.
	return GetDefault<UFF_UIFlowSettings>();
}
