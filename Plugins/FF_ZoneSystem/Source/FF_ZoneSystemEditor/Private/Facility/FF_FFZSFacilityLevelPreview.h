// FF_FFZSFacilityLevelPreview.h
// 역할: 시설 정의에서 선택한 레벨과 등급에 맞는 메시를 에디터 미리보기에 반영한다.
#pragma once

class UFFZSFacilityDefinition;

/** An editor-only preview world. Never spawns actors in or modifies the user's level. */
void OpenFFZSFacilityLevelPreview(UFFZSFacilityDefinition* Definition);
