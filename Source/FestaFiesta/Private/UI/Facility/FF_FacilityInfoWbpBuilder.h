// FF_FacilityInfoWbpBuilder.h
// 역할: 시설 정보·조명·유료 프로그램 화면의 초기 생성과 명시적인 에셋 수정을 담당한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

class UWidgetBlueprint;
class UWidgetTree;

/**
 * 시설 정보 WBP 생성기 중 자동화 테스트가 직접 부르는 비파괴 마이그레이션만 노출한다.
 * 테스트는 실제 에셋을 저장하지 않고 메모리 사본에서 이 함수를 돌려 계약을 검증한다.
 */
namespace FFFacilityInfoWbpBuilder
{
	/** 시설 수익 섹션이 요구하는 BindWidgetOptional 이름 전부. */
	const TArray<FName>& GetRevenueContractVariableNames();

	bool HasRevenueWidgetContract(const UWidgetBlueprint& Blueprint);

	/** 없는 노드만 Panel_Content 의 강화 섹션 바로 뒤에 추가한다. 기존 디자이너 노드는 건드리지 않는다. */
	bool EnsureRevenueWidgetContract(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, bool& bOutModified);
	/** Explicit approved V2 migration. Only this inspector tree changes; repeated calls preserve Designer edits. */
	bool ApplyInspectorV2Design(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, bool& bOutModified);
	/** Keep purchase controls outside the scroll body; repair the first approved V2 asset once. */
	bool RefineInspectorV2Layout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, bool& bOutModified);
}

#endif // WITH_EDITOR
