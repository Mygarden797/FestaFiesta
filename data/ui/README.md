# UI 담당자 전용 편집 폴더

이 폴더는 `data/derived/`·`data/unreal/`과 분리된 **UI 담당자 편집 창구**다. 목적은 표시 명칭(아키타입·동반유형)을 바꾸고 싶을 때 데이터 파이프라인 파일을 직접 만지지 않아도 되게 하는 것이다.

## 규칙

1. 이 폴더의 CSV만 UI 담당자가 자유롭게 편집한다. `data/derived/`, `data/unreal/`은 건드리지 않는다.
2. `ui_override_label` 열만 채운다. 비워두면 엔지니어링 쪽 가칭(`current_default_label_draft`)이 그대로 쓰인다.
3. `current_default_label_draft` 열은 참고용이며 편집하지 않는다 — 빌드 스크립트가 아니라 사람이 수동으로 최신화한다(엔지니어링 쪽에서 가칭이 바뀌면 갱신).
4. 여기서 이름을 바꿔도 **여전히 가칭 상태**다. `DataStatus`는 `draft_assumption`으로 유지된다. "확정"은 팀이 명시적으로 합의했을 때만 `data/metadata/CLAUDE_HANDOFF...` 12장 체크리스트를 따라 별도로 처리한다.
5. `note` 열에 왜 바꿨는지 한 줄 남기면 나중에 되돌릴 때 참고할 수 있다.

## 반영 방법

`node Tools/data/build_unreal_datatables.mjs`를 실행하면 이 폴더의 override가 `data/unreal/*.csv`에 자동 반영된다. UE에서 해당 DataTable을 Reimport하면 끝난다.

> ⚠️ 10/14 기준: 이 스크립트는 `data/unreal/`의 유틸 계수·SNS 문장 CSV도 `data/derived/`로 덮어쓴다. 그 두 파일은 `data/unreal/`에서 직접 고쳐 와서 지금 돌리면 행이 사라진다(`data/unreal/README.md` 참조). 명칭만 바꿀 때는 `data/unreal/npc_archetypes_v0.csv`·`companion_type_modifier_v0.csv`만 반영되게 하고, 나머지 CSV는 되돌린다.

## 파일

| 파일 | 대응 |
|---|---|
| `archetype_label_overrides.csv` | 아키타입 5종 (`ArchetypeLabelDraft`) |
| `companion_label_overrides.csv` | 동반유형 3종 (`CompanionLabelDraft`) |
